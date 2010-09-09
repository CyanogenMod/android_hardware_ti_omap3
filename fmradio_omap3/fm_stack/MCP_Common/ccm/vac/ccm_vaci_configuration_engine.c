/*
 * TI's FM Stack
 *
 * Copyright 2001-2008 Texas Instruments, Inc. - http://www.ti.com/
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and  
 * limitations under the License.
 */

/*******************************************************************************\
*
*   FILE NAME:      ccm_vaci_configuration_engine.c
*
*   BRIEF:          This file defines the implementation of the Connectivity Chip
*                   Manager (CCM) Voice and Audio Control (VAC) configuration 
*                   engine component.
*                  
*
*   DESCRIPTION:    The configuration engine is a CCM-VAC internal module Performing
*                   VAC operations (starting and stopping an operation, changing
*                   routing and configuration)
*
*   AUTHOR:         Ronen Kalish
*
\*******************************************************************************/

#include "ccm_vaci_configuration_engine.h"
#include "ccm_vaci_allocation_engine.h"
#include "ccm_vaci_mapping_engine.h"
#include "mcp_defs.h"
#include "ccm_vaci_debug.h"
#include "ccm_config.h"
#include "mcp_hal_string.h"
#include "mcp_hal_os.h"
#include "mcp_hal_memory.h"
#include "mcp_config_parser.h"
#include "ccm_vaci_chip_abstration.h"
#include "mcp_hal_log.h"

MCP_HAL_LOG_SET_MODULE(MCP_HAL_LOG_MODULE_TYPE_CCM_VAC);

#define CCM_VAC_MUTEX_NAME              ("CCM_VAC_Mutex")

/* data structures */
typedef enum _ECCM_VAC_CEOperationState
{
    CCM_VAC_OPERATION_STATE_IDLE = 0, /* operation is not running */
    CCM_VAC_OPERATION_STATE_STARTING, /* operation start requested, resources configuration in progress */
    CCM_VAC_OPERATION_STATE_RUNNING,  /* operation is in progress */
    CCM_VAC_OPERATION_STATE_STOPPING  /* operation stop requested, resources configuration in progress */
} ECCM_VAC_CEOperationState;

typedef struct _TCCM_VAC_CECALUserData
{
    ECAL_Operation                  eOperation;
    TCCM_VAC_ConfigurationEngine    *ptConfigEngine;
} TCCM_VAC_CECALUserData;

typedef struct _TCCM_VAC_CEOperationDB
{
    McpBool                     bSupported;
            /* whether the operation is supported */
    TCCM_VAC_Callback           fCB[ CCM_VAC_MAX_NUMBER_OF_CLIENTS_PER_OPERATION ];
    McpU32                      uNumberOfCBs;
            /* operations callback functions, and current registered callbacks number */
    ECCM_VAC_CEOperationState   eOperationState;
            /* current operation state */
    TCAL_ResourceList           tCurrentRequiredResources; 
            /* resources required for current operation */
    McpU32                      uResourceBeingConfigured;
            /* index of the resource currently being configured */
    ECCM_VAC_Event              eEventToClient; 
            /* the event to send to the client when the operation will finish starting */
    TCAL_DigitalConfig          tDigitalConfig;
            /* struct to hold data required for CAL callbacks */
    TCCM_VAC_CECALUserData      tCALUserData;
} TCCM_VAC_CEOperationDB;

struct _TCCM_VAC_ConfigurationEngine
{
    McpHalOsSemaphoreHandle     tSemaphore;             /* the VAC semaphore */
    TCCM_VAC_CEOperationDB      tOperations[ CAL_OPERATION_MAX_NUM ]; /* all operations data */
    TCCM_VAC_MappingEngine      *ptMappingEngine;       /* mapping engine object */
    TCCM_VAC_AllocationEngine   *ptAllocationEngine;    /* allocation engine object */
    McpConfigParser             *pConfigParser;          /* configuration file storage and parser */
    Cal_Config_ID               *pCAL;                  /* chip abstraction layer object */
};

/* the actual configuration engine objects */
TCCM_VAC_ConfigurationEngine    tCEObjects[ MCP_HAL_MAX_NUM_OF_CHIPS ];

/* forward declarations */
ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_ConfigResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                              ECAL_Operation eOperation);

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_StopResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                            ECAL_Operation eOperation);

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_AllocateOperationResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine, 
                                                                         ECAL_Operation eOperation,
                                                                         TCCM_VAC_UnavailResourceList *ptUnavailResources);

void _CCM_VAC_ConfigurationEngine_DeallocateOperationResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine, 
                                                                ECAL_Operation eOperation);

void _CCM_VAC_ConfiguratioEngine_MaskToList (ECAL_ResourceMask eResourceMask, 
                                             TCAL_ResourceList *ptResourceList);

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_StaticInit(void)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_StaticInit");

    /* no need to do anything - everything is per object */

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_Create(McpHalChipId chipId, 
                                                    Cal_Config_ID *pCAL,
                                                    TCCM_VAC_ConfigurationEngine **thisObj,
                                                    McpConfigParser 			*pConfigParser)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_Create");


    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_Create: creating VAC configuration engine object "
                   "for chip %d", chipId));
    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_Configure (TCCM_VAC_ConfigurationEngine *thisObj)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_Configure");

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

void _CCM_VAC_ConfigurationEngine_Destroy (TCCM_VAC_ConfigurationEngine **thisObj)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_Destroy");

    MCP_FUNC_END ();
}

void _CCM_VAC_ConfigurationEngine_RegisterCallback (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                    ECAL_Operation eOperation, 
                                                    TCCM_VAC_Callback fCB)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_RegisterCallback");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_RegisterCallback: registering callback for opeartion %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    MCP_FUNC_END();
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_StartOperation (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                             ECAL_Operation eOperation,
                                                             TCAL_DigitalConfig *ptConfig,
                                                             TCCM_VAC_UnavailResourceList *ptUnavailResources)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_StartOperation");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StartOperation: attempting to start operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_StopOperation (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                            ECAL_Operation eOperation)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_StopOperation");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StopOperation: stopping operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_ChangeResource (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                             ECAL_Operation eOperation, 
                                                             ECAL_ResourceMask eResourceMask, 
                                                             TCAL_DigitalConfig *ptConfig, 
                                                             TCCM_VAC_UnavailResourceList *ptUnavailResources)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_ChangeResource");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_ChangeResource: changing resources for operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    MCP_FUNC_END ();

    /* Shouldn't reach this point - in order to avoid warning */
    return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_ChangeConfiguration (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                                  ECAL_Operation eOperation, 
                                                                  TCAL_DigitalConfig *ptConfig)
{
    MCP_FUNC_START ("CCM_VAC_ConfigurationEngine_ChangeConfiguration");

    MCP_LOG_INFO (("CCM_VAC_ConfigurationEngine_ChangeConfiguration: changing configuration for operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

void _CCM_VAC_ConfigurationEngine_CalCb (void *pUserData, 
                                         ECAL_RetValue eRetValue)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_CalCb");

    MCP_FUNC_END ();
}

void _CCM_VAC_ConfigurationEngine_CalStopCb (void *pUserData, 
                                             ECAL_RetValue eRetValue)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_CalStopCb");

    MCP_FUNC_END ();
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_ConfigResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                              ECAL_Operation eOperation)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_ConfigResources");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_ConfigResources: continuing configuration for operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    MCP_FUNC_END ();

	return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_StopResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                            ECAL_Operation eOperation)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_StopResources");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StopResources: continuing de-configuration for operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    MCP_FUNC_END ();

	return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_AllocateOperationResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine, 
                                                                         ECAL_Operation eOperation,
                                                                         TCCM_VAC_UnavailResourceList *ptUnavailResources)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_AllocateOperationResources");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_AllocateOperationResources: allocating resources"
                   " for operation %s", _CCM_VAC_DebugOperationStr(eOperation)));

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

void _CCM_VAC_ConfigurationEngine_DeallocateOperationResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine, 
                                                                ECAL_Operation eOperation)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_DeallocateOperationResources");

    MCP_FUNC_END ();
}

void _CCM_VAC_ConfiguratioEngine_MaskToList (ECAL_ResourceMask eResourceMask, 
                                             TCAL_ResourceList *ptResourceList)
{
}

void _CCM_VAC_ConfigurationEngine_SetResourceProperties (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                    ECAL_Resource eResource,
                                    TCAL_ResourceProperties *pProperties)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_SetResourceProperties");

    MCP_FUNC_END ();
}


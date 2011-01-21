/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
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
    McpU32                  uIndex, uIndex2;
    ECCM_VAC_Status         status;
    McpHalOsStatus          eOsStatus;
    char                    mutexName[ MCP_HAL_OS_MAX_ENTITY_NAME_LEN + 1 ];

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_Create");

    /* verify chip ID */
    MCP_VERIFY_FATAL ((MCP_HAL_MAX_NUM_OF_CHIPS > chipId), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_Create: invalid chip ID %d", chipId));

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_Create: creating VAC configuration engine object "
                   "for chip %d", chipId));

    /* initialize the object pointer */
    *thisObj = &(tCEObjects[ chipId ]);

    /* initialize operations data */
    for (uIndex = 0; uIndex < CAL_OPERATION_MAX_NUM; uIndex++)
    {
        (*thisObj)->tOperations[ uIndex ].bSupported = MCP_FALSE;
        for (uIndex2 = 0; uIndex2 < CCM_VAC_MAX_NUMBER_OF_CLIENTS_PER_OPERATION; uIndex2++)
        {
            (*thisObj)->tOperations[ uIndex ].fCB[ uIndex2 ] = NULL;
        }
        (*thisObj)->tOperations[ uIndex ].uNumberOfCBs = 0;
        (*thisObj)->tOperations[ uIndex ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;
        (*thisObj)->tOperations[ uIndex ].tCurrentRequiredResources.uNumOfResources = 0;
        (*thisObj)->tOperations[ uIndex ].uResourceBeingConfigured = 0;
        (*thisObj)->tOperations[ uIndex ].tCALUserData.ptConfigEngine = *thisObj;
        (*thisObj)->tOperations[ uIndex ].tCALUserData.eOperation = uIndex;
    }

    /* store the CAL object */
    (*thisObj)->pCAL = pCAL;

    (*thisObj)->pConfigParser = pConfigParser;

     /* Initialize the semaphore name */
    MCP_HAL_STRING_Sprintf (mutexName, "%s%d", CCM_VAC_MUTEX_NAME, chipId);

    /* create the semaphore */
    eOsStatus = MCP_HAL_OS_CreateSemaphore (mutexName, &((*thisObj)->tSemaphore));
    MCP_VERIFY_FATAL ((eOsStatus == MCP_HAL_OS_STATUS_SUCCESS), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                        ("_CCM_VAC_ConfigurationEngine_Create: Semaphore creation failed (%d)",
                         eOsStatus));

    /* initialize the mapping engine */
    status = _CCM_VAC_MappingEngine_Create (chipId, (*thisObj)->pConfigParser, &((*thisObj)->ptMappingEngine));
    MCP_VERIFY_ERR ((CCM_VAC_STATUS_SUCCESS == status), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                    ("_CCM_VAC_ConfigurationEngine_Create: mapping engine creation failed (%d)",
                     status));

    /* initialize the allocation engine */
    status = _CCM_VAC_AllocationEngine_Create (chipId, (*thisObj)->pConfigParser, &((*thisObj)->ptAllocationEngine));
    MCP_VERIFY_ERR ((CCM_VAC_STATUS_SUCCESS == status), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                    ("_CCM_VAC_ConfigurationEngine_Create: allocation engine creation failed (%d)",
                     status));
 
    MCP_FUNC_END ();

    return status;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_Configure (TCCM_VAC_ConfigurationEngine *thisObj)
{
    TCAL_ResourceSupport   tAvailResources;
    TCAL_OperationSupport  tAvailOperations;
    ECCM_VAC_Status        status;
    McpU32                 uOperationNum, uIndex;
    ECAL_Operation         eOperation;
    McpConfigParserStatus  eCPStatus;
    McpU8                  *pTempValue;

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_Configure");

    /* retrieve chip capabilities from CAL */
    CAL_GetAudioCapabilities (thisObj->pCAL,
                              _CCM_VAC_MappingEngine_GetConfigData (thisObj->ptMappingEngine),
                              _CCM_VAC_AllocationEngine_GetConfigData (thisObj->ptAllocationEngine),
                              &tAvailResources, &tAvailOperations);

    /* configure allowed operations: */
    /* first, get number or operations from ini file */
    eCPStatus = MCP_CONFIG_PARSER_GetIntegerKey (thisObj->pConfigParser, 
                                                 (McpU8 *)CCM_VAC_CONFIG_INI_OPERATIONS_SECTION_NAME,
                                                 (McpU8 *)CCM_VAC_CONFIG_INI_OPERATIONS_NUMBER_KEY_NAME,
                                                 (McpS32*)&uOperationNum);
    MCP_VERIFY_FATAL (((eCPStatus == MCP_CONFIG_PARSER_STATUS_SUCCESS) && (0 < uOperationNum)),
                        CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                        ("_CCM_VAC_ConfigurationEngine_Configure: error reading number of operations "
                         "from file, number: %d, status: %d", uOperationNum, eCPStatus));

    /* for every operation */
    for (uIndex = 0; uIndex < uOperationNum; uIndex++)
    {
        /* read operation name from ini file */
        eCPStatus = MCP_CONFIG_PARSER_GetAsciiKey (thisObj->pConfigParser, 
                                                   (McpU8 *)CCM_VAC_CONFIG_INI_OPERATIONS_SECTION_NAME,
                                                   (McpU8 *)CCM_VAC_CONFIG_INI_OPERATION_NAME_KEY_NAME,
                                                   &pTempValue);
        MCP_VERIFY_FATAL (((eCPStatus == MCP_CONFIG_PARSER_STATUS_SUCCESS) && (NULL != pTempValue)),
                            CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                            ("_CCM_VAC_ConfigurationEngine_Configure: Can't get operation name for operation number %d",
                             uIndex));
            
        /* get operation enum value */
        eOperation = StringtoOperation (pTempValue);
        MCP_VERIFY_FATAL ((CAL_OPERATION_INVALID != eOperation),
                          CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                          ("_CCM_VAC_ConfigurationEngine_Configure: Can't convert operation number %d, "
                           "string %s to enum value", uIndex, pTempValue));

        /* verify the chip supports it */
        MCP_VERIFY_FATAL ((MCP_TRUE == tAvailOperations.bOperationSupport[ eOperation ]),
                          CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                          ("_CCM_VAC_ConfigurationEngine_Configure: operation %s required by ini file"
                           "is not supported by chip!", _CCM_VAC_DebugOperationStr(eOperation)));

        /* and set it to supported */
        thisObj->tOperations[ eOperation ].bSupported = MCP_TRUE;
    }

    /* configure the mapping engine with chip capabilities */
    status = _CCM_VAC_MappingEngine_Configure (thisObj->ptMappingEngine, 
                                               &tAvailResources, &tAvailOperations);
    MCP_VERIFY_ERR ((CCM_VAC_STATUS_SUCCESS == status), status,
                    ("_CCM_VAC_ConfigurationEngine_Configure: mapping engine configuration faield with status %d",
                     status));

    /* configure the allocation engine with chip capabilities */
    status = _CCM_VAC_AllocationEngine_Configure (thisObj->ptAllocationEngine, 
                                                  &tAvailResources, &tAvailOperations);
    MCP_VERIFY_ERR ((CCM_VAC_STATUS_SUCCESS == status), status,
                    ("_CCM_VAC_ConfigurationEngine_Configure: allocation engine configuration faield with status %d",
                     status));

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

void _CCM_VAC_ConfigurationEngine_Destroy (TCCM_VAC_ConfigurationEngine **thisObj)
{
    McpHalOsStatus      eOsStatus;

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_Destroy");

    /* destroy the sempahore */
    eOsStatus = MCP_HAL_OS_DestroySemaphore(&((*thisObj)->tSemaphore));
    MCP_VERIFY_FATAL_NO_RETVAR ((eOsStatus == MCP_HAL_OS_STATUS_SUCCESS),
                                ("_CCM_VAC_ConfigurationEngine_Destroy: semaphore destruction failed (%d)", 
                                 eOsStatus));

    /* destroy the mapping engine */
    _CCM_VAC_MappingEngine_Destroy (&((*thisObj)->ptMappingEngine));

    /* destroy the allocation engine */
    _CCM_VAC_AllocationEngine_Destroy (&((*thisObj)->ptAllocationEngine));

    *thisObj = NULL;

    MCP_FUNC_END ();
}

void _CCM_VAC_ConfigurationEngine_RegisterCallback (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                    ECAL_Operation eOperation, 
                                                    TCCM_VAC_Callback fCB)
{
    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_RegisterCallback");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_RegisterCallback: registering callback for opeartion %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify object pointer and callback function */
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptConfigEngine),
                                ("_CCM_VAC_ConfigurationEngine_RegisterCallback: NULL object!"));
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != fCB),
                                ("_CCM_VAC_ConfigurationEngine_RegisterCallback: NULL callback!"));
    /* verify the number of maximum CBs has not been reached */
    MCP_VERIFY_ERR_NO_RETVAR ((CCM_VAC_MAX_NUMBER_OF_CLIENTS_PER_OPERATION > ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs),
                              ("_CCM_VAC_ConfigurationEngine_RegisterCallback: %d callbacks are "
                               "already registered for operation %s!", 
                               ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs,
                               _CCM_VAC_DebugOperationStr(eOperation)));

    /* register the callback */
    ptConfigEngine->tOperations[ eOperation ].fCB[ ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs ]
         = fCB;
    ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs++;

    MCP_FUNC_END();
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_StartOperation (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                             ECAL_Operation eOperation,
                                                             TCAL_DigitalConfig *ptConfig,
                                                             TCCM_VAC_UnavailResourceList *ptUnavailResources)
{
    McpHalOsStatus          eOsStatus;
    TCAL_ResourceList       *pResourceList;
    ECCM_VAC_Status         eConfigStatus;
    ECCM_VAC_Status         eAllocationStatus, status = CCM_VAC_STATUS_FAILURE_UNSPECIFIED; 

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_StartOperation");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StartOperation: attempting to start operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify object pointer */
    MCP_VERIFY_FATAL ((NULL != ptConfigEngine), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_StartOperation: NULL object!"));

/* verify operation is valid */
    MCP_VERIFY_ERR ((eOperation < CAL_OPERATION_MAX_NUM),
                    CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                    ("_CCM_VAC_ConfigurationEngine_StartOperation: invalid operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));


    /* verify operation is supported */
    MCP_VERIFY_ERR ((MCP_TRUE == ptConfigEngine->tOperations[ eOperation ].bSupported),
                    CCM_VAC_STATUS_FAILURE_OPERATION_NOT_SUPPORTED,
                    ("_CCM_VAC_ConfigurationEngine_StartOperation: unsupported operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));

    pResourceList = &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);
	

	/* lock the semaphore (while accessing the mapping and allocation engines) */
    eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_StartOperation: semaphore returned error %d",
                       eOsStatus));

    /* if the operation is already running */
    if (CCM_VAC_OPERATION_STATE_RUNNING == ptConfigEngine->tOperations[ eOperation ].eOperationState)
    {
        /* unlock the semaphore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_StartOperation: unlock semaphore status: %d",
                           eOsStatus));

        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StartOperation: request to start opearation %s"
                       ", which is alredy running", _CCM_VAC_DebugOperationStr(eOperation)));

        /* return success, to indicate operation is running */
        return CCM_VAC_STATUS_SUCCESS;
    }
    /* if operation is still undergoing configuration */
    else if (CCM_VAC_OPERATION_STATE_STARTING == ptConfigEngine->tOperations[ eOperation ].eOperationState)
    {
        /* unlock the semaphore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_StartOperation: unlock semaphore status: %d",
                           eOsStatus));

        /* 
         * verify that the operation is actually starting (rather than stopping or changing
         * resoure or configuration)
         */
        if (CCM_VAC_EVENT_OPERATION_STARTED == ptConfigEngine->tOperations[ eOperation ].eEventToClient)
        {
            /* return pending, when configuration will be completed the CB will be called */
            return CCM_VAC_STATUS_PENDING;
        }
        else
        {
            /* state is atrting sue to some other request - return busy */
            return CCM_VAC_STATUS_FAILURE_BUSY;
        }
    }
    /* if operation is being stopped */
    else if (CCM_VAC_OPERATION_STATE_STOPPING == ptConfigEngine->tOperations[ eOperation ].eOperationState)
    {
        /* unlock the semaphore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_StartOperation: unlock semaphore status: %d",
                           eOsStatus));

        return CCM_VAC_STATUS_FAILURE_BUSY;
    }

    /*Try to allocate all required resources */
    eAllocationStatus = _CCM_VAC_ConfigurationEngine_AllocateOperationResources (ptConfigEngine,
                                                                                 eOperation,
                                                                                 ptUnavailResources);

    /* if not all resources were allocated successfuly */
    if (CCM_VAC_STATUS_SUCCESS != eAllocationStatus)
    {
        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StartOperation: operation %s could not be started due to unavailable resources",
                       _CCM_VAC_DebugOperationStr(eOperation)));

        /* unlock the sempahore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_StartOperation: unlock semaphore status: %d",
                           eOsStatus));

        /* return failure status */
        return eAllocationStatus;
    }

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StartOperation: operation %s allocation stage finished successfully!",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    /* mark operation has entered configuration stage */
    ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_STARTING;

    /* unlock the sempahore */
    eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_StartOperation: unlock semaphore status: %d",
                       eOsStatus));

    /* initialize configuration stage */
    ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured = 0;
    ptConfigEngine->tOperations[ eOperation ].eEventToClient = CCM_VAC_EVENT_OPERATION_STARTED;
    MCP_HAL_MEMORY_MemCopy (&(ptConfigEngine->tOperations[ eOperation ].tDigitalConfig), ptConfig,
                            sizeof(TCAL_DigitalConfig));

    /* start configuration process */
    eConfigStatus = _CCM_VAC_ConfigurationEngine_ConfigResources (ptConfigEngine, eOperation);

    /* lock the semaphore again (while updating the operation state) */
    eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_StartOperation: semaphore returned error %d",
                       eOsStatus));    

    /* if all resources were synchronously configured */
    if (CCM_VAC_STATUS_SUCCESS == eConfigStatus)
    {
        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StartOperation: operation %s configuration stage finished!",
                       _CCM_VAC_DebugOperationStr(eOperation)));
        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_RUNNING;
        status = CCM_VAC_STATUS_SUCCESS;
    }
    /* if asynchronous configuration is required, the CB will complete the configuration */
    else if (CCM_VAC_STATUS_PENDING == eConfigStatus)
    {
        status = CCM_VAC_STATUS_PENDING;
    }
    else /* an error occurred in the configuration process */
    {
        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StartOperation: operation %s configuration stage failed, "
                       "resource %s returned error %d!", 
                       _CCM_VAC_DebugResourceStr (pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                       _CCM_VAC_DebugOperationStr(eOperation), eConfigStatus));
        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;
        status = CCM_VAC_STATUS_FAILURE_UNSPECIFIED;
    }

    /* unlock the sempahore */
    eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_StartOperation: unlock semaphore status: %d",
                       eOsStatus));

    MCP_FUNC_END ();

    return status;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_StopOperation (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                            ECAL_Operation eOperation)
{
    ECCM_VAC_Status         status;
    McpHalOsStatus          eOsStatus;
    TCAL_ResourceList       *pResourceList;
 

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_StopOperation");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StopOperation: stopping operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify object pointer */
    MCP_VERIFY_FATAL ((NULL != ptConfigEngine), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_StopOperation: NULL object!"));

   /* verify operation is valid */
    MCP_VERIFY_ERR ((eOperation < CAL_OPERATION_MAX_NUM),
                    CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                    ("_CCM_VAC_ConfigurationEngine_StartOperation: invalid operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));

   /* verify operation is supported */
    MCP_VERIFY_ERR ((MCP_TRUE == ptConfigEngine->tOperations[ eOperation ].bSupported),
                    CCM_VAC_STATUS_FAILURE_OPERATION_NOT_SUPPORTED,
                    ("_CCM_VAC_ConfigurationEngine_StopOperation: unsupported operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));
    
    pResourceList = &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);

    /* lock the semaphore (while accessing the mapping engine and operation state) */
    eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_StopOperation: "
                       "lock semaphore returned error %d",
                       eOsStatus));

    /* if operation is starting */
    if (CCM_VAC_OPERATION_STATE_STARTING == ptConfigEngine->tOperations[ eOperation ].eOperationState)
    {
        /* unlock the sempahore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_StopOperation: unlock semaphore status: %d",
                           eOsStatus));

        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StopOperation: operation %s is starting, can't stop",
                       _CCM_VAC_DebugOperationStr(eOperation)));

        return CCM_VAC_STATUS_FAILURE_BUSY;
    }
    else if (CCM_VAC_OPERATION_STATE_IDLE == ptConfigEngine->tOperations[ eOperation ].eOperationState)
    {
        /* unlock the sempahore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_StopOperation: unlock semaphore status: %d",
                           eOsStatus));

        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StopOperation: operation %s is already idle",
                       _CCM_VAC_DebugOperationStr(eOperation)));

        return CCM_VAC_STATUS_SUCCESS;
    }
    /* if operation is already stopping */
    else if (CCM_VAC_OPERATION_STATE_STOPPING == ptConfigEngine->tOperations[ eOperation ].eOperationState)
    {
        /* unlock the sempahore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_StopOperation: unlock semaphore status: %d",
                           eOsStatus));

        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StopOperation: operation %s is already stopping",
                       _CCM_VAC_DebugOperationStr(eOperation)));

        return CCM_VAC_STATUS_PENDING;
    }

    /* change operation state to stopping */
    ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_STOPPING;

    /* get the required resources from the mapping engine */
    _CCM_VAC_MappingEngine_OperationToResourceList (ptConfigEngine->ptMappingEngine, eOperation,
                                                    pResourceList);

    /* unlock the sempahore */
    eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_StopOperation: unlock semaphore status: %d",
                       eOsStatus));

    /* initialize de-configuration stage */
    ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured = 0;
    ptConfigEngine->tOperations[ eOperation ].eEventToClient = CCM_VAC_EVENT_OPERATION_STOPPED;

    /* 
     * request to de-configure (or stop all resources
     * if a resource is being configured, its configuration will be stopped. Only for FM RX over
     * SCO, the configuration must be reverted (due to FW implementation)
     */
    status = _CCM_VAC_ConfigurationEngine_StopResources (ptConfigEngine, eOperation);

    /* if de-configuration failed */
    if (CCM_VAC_STATUS_FAILURE_UNSPECIFIED == status)
    {
        MCP_LOG_ERROR (("_CCM_VAC_ConfigurationEngine_StopOperation: operation %s de-configuration stage failed, "
                        "resource %s returned error %d!", 
                        _CCM_VAC_DebugResourceStr (pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                        _CCM_VAC_DebugOperationStr(eOperation), status));
        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;

        return CCM_VAC_STATUS_FAILURE_UNSPECIFIED;
    }
    /* if de-configuration is pending */
    else if (CCM_VAC_STATUS_PENDING == status)
    {
        /* asynchronous de-configuration - the CB will complete the stop operation */
        return CCM_VAC_STATUS_PENDING;
    }

    /* lock the semaphore (while accessing the allocation engine and operation state) */
    eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
    MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                ("_CCM_VAC_ConfigurationEngine_StopOperation: "
                                 "lock semaphore returned error %d",
                                 eOsStatus));

    /* de-configuration completed - continue to de-allocating resources */
    _CCM_VAC_ConfigurationEngine_DeallocateOperationResources (ptConfigEngine, eOperation);

    /* mark that the operation is idle */
    ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;

    /* unlock the sempahore */
    eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_StopOperation: unlock semaphore status: %d",
                       eOsStatus));

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_ChangeResource (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                             ECAL_Operation eOperation, 
                                                             ECAL_ResourceMask eResourceMask, 
                                                             TCAL_DigitalConfig *ptConfig, 
                                                             TCCM_VAC_UnavailResourceList *ptUnavailResources)
{
    McpHalOsStatus          eOsStatus;
    TCAL_ResourceList       tCurrentOptionalresources, tNewOptionalResources;
    TCAL_ResourceList       *pResourceList;	
    ECCM_VAC_Status         eAllocationStatus, status = CCM_VAC_STATUS_FAILURE_UNSPECIFIED;
    ECCM_VAC_Status         eConfigStatus;

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_ChangeResource");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_ChangeResource: changing resources for operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify object pointer */
    MCP_VERIFY_FATAL ((NULL != ptConfigEngine), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_ChangeResource: NULL object!"));

    /* verify operation is valid */
    MCP_VERIFY_ERR ((eOperation < CAL_OPERATION_MAX_NUM),
                    CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                    ("_CCM_VAC_ConfigurationEngine_StartOperation: invalid operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify operation is supported */
    MCP_VERIFY_ERR ((MCP_TRUE == ptConfigEngine->tOperations[ eOperation ].bSupported),
                    CCM_VAC_STATUS_FAILURE_OPERATION_NOT_SUPPORTED,
                    ("_CCM_VAC_ConfigurationEngine_ChangeResource: unsupported operation %s",
                    _CCM_VAC_DebugOperationStr(eOperation)));

    pResourceList = &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);

    /* lock the semaphore (while accessing the mapping and allocation engines) */
    eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_ChangeResource: semaphore returned error %d",
                       eOsStatus));

    /* if the operation is starting or stopping */
    if ((CCM_VAC_OPERATION_STATE_STARTING == ptConfigEngine->tOperations[ eOperation ].eOperationState) ||
        (CCM_VAC_OPERATION_STATE_STOPPING == ptConfigEngine->tOperations[ eOperation ].eOperationState))
    {
        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_ChangeResource: operation %s is still undergoing configuration!",
                     _CCM_VAC_DebugOperationStr(eOperation)));

        /* unlock the sempahore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_ChangeResource: unlock semaphore status: %d",
                           eOsStatus));

        return CCM_VAC_STATUS_FAILURE_BUSY;
    }
    /* if the operation is currently running */
    else if (CCM_VAC_OPERATION_STATE_RUNNING == ptConfigEngine->tOperations[ eOperation ].eOperationState)
    {
        /* first convert the resource mask to resource list */
        _CCM_VAC_ConfiguratioEngine_MaskToList (eResourceMask, &tNewOptionalResources);

        /* initialize the unavailable resource list to an empty list */
        ptUnavailResources->uNumOfUnavailResources = 0;

        /* release current resources */
        _CCM_VAC_ConfigurationEngine_DeallocateOperationResources (ptConfigEngine, eOperation);

        /* retrieve current optional resources in use from the mapping engine */
        _CCM_VAC_MappingEngine_GetOptionalResourcesList(ptConfigEngine->ptMappingEngine, 
                                                        eOperation, &tCurrentOptionalresources);

        /* change the mapping engine to new optional resources */
        _CCM_VAC_MappingEngine_SetOptionalResourcesList (ptConfigEngine->ptMappingEngine, 
                                                         eOperation, &tNewOptionalResources);

        /* try allocating all required resources */
        eAllocationStatus = 
            _CCM_VAC_ConfigurationEngine_AllocateOperationResources (ptConfigEngine, eOperation,
                                                                     ptUnavailResources);

        /* if allocation succeeded */
        if (CCM_VAC_STATUS_SUCCESS == eAllocationStatus)
        {
            /* change state to starting, before the actual configuration */
            ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_STARTING;

            /* unlock the sempahore */
            eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
            MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                              ("_CCM_VAC_ConfigurationEngine_ChangeResource: unlock semaphore status: %d",
                               eOsStatus));

            /* initialize configuration stage */
            ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured = 0;
            ptConfigEngine->tOperations[ eOperation ].eEventToClient = CCM_VAC_EVENT_RESOURCE_CHANGED;
            MCP_HAL_MEMORY_MemCopy (&(ptConfigEngine->tOperations[ eOperation ].tDigitalConfig), ptConfig,
                                    sizeof(TCAL_DigitalConfig));

            /* start configuration process */
            eConfigStatus = _CCM_VAC_ConfigurationEngine_ConfigResources (ptConfigEngine, eOperation);

            /* lock the semaphore again (while accessing the operation state) */
            eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
            MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                              ("_CCM_VAC_ConfigurationEngine_ChangeResource: semaphore returned error %d",
                               eOsStatus));

            /* if all resources were synchronously configured */
            if (CCM_VAC_STATUS_SUCCESS == eConfigStatus)
            {
                MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_ChangeResource: operation %s configuration "
                               "stage finished!", _CCM_VAC_DebugOperationStr(eOperation)));
                ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_RUNNING;
                status = CCM_VAC_STATUS_SUCCESS;
            }
            /* if asynchronous configuration is required, the CB will complete the configuration */
            else if (CCM_VAC_STATUS_PENDING == eConfigStatus)
            {
                status = CCM_VAC_STATUS_PENDING;
            }
            else /* an error occurred in the configuration process */
            {
                MCP_LOG_ERROR (("_CCM_VAC_ConfigurationEngine_ChangeResource: operation %s configuration "
                                "stage failed, resource %s returned error %d!", 
                                _CCM_VAC_DebugResourceStr (pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                                _CCM_VAC_DebugOperationStr(eOperation), eConfigStatus));
                ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;
                status = CCM_VAC_STATUS_FAILURE_UNSPECIFIED;
            }

            /* unlock the sempahore */
            eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
            MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                              ("_CCM_VAC_ConfigurationEngine_ChangeResource: unlock semaphore status: %d",
                               eOsStatus));

            return status;
        }
        else
        {
            /* 
             * the allocation failure should have already released all resources, and filled the 
             * unavailable resource list
             */
            /* change the mapping engine to the previous optional resources */
            _CCM_VAC_MappingEngine_SetOptionalResourcesList (ptConfigEngine->ptMappingEngine, 
                                                             eOperation, &tCurrentOptionalresources);

            /* allocate all resource */
            eAllocationStatus = 
                _CCM_VAC_ConfigurationEngine_AllocateOperationResources (ptConfigEngine, eOperation,
                                                                         ptUnavailResources);

            /* unlock the sempahore */
            eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
            MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                              ("_CCM_VAC_ConfigurationEngine_ChangeResource: unlock semaphore status: %d",
                               eOsStatus));

            /* verify all resources had been allocated successfully */
            MCP_VERIFY_ERR ((CCM_VAC_STATUS_SUCCESS == eAllocationStatus), 
                            CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                            ("_CCM_VAC_ConfigurationEngine_ChangeResource: Can't allocate old resources"
                             "for operation %s!", _CCM_VAC_DebugOperationStr(eOperation)));

            return CCM_VAC_STATUS_FAILURE_UNAVAILABLE_RESOURCES;
        }
    }
    /* operation is idle */
    else
    {
        /* first convert the resource mask to resource list */
        _CCM_VAC_ConfiguratioEngine_MaskToList (eResourceMask, &tNewOptionalResources);

        /* change the mapping engine to new optional resources */
        _CCM_VAC_MappingEngine_SetOptionalResourcesList (ptConfigEngine->ptMappingEngine, 
                                                         eOperation, &tNewOptionalResources);

        /* unlock the sempahore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_ChangeResource: unlock semaphore status: %d",
                           eOsStatus));

        return CCM_VAC_STATUS_SUCCESS;
    }

    MCP_FUNC_END ();

    /* Shouldn't reach this point - in order to avoid warning */
    return CCM_VAC_STATUS_FAILURE_UNSPECIFIED;
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_ChangeConfiguration (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                                  ECAL_Operation eOperation, 
                                                                  TCAL_DigitalConfig *ptConfig)
{
    McpHalOsStatus          eOsStatus;
    TCAL_ResourceList       *pResourceList;
    ECCM_VAC_Status         eConfigStatus, status;

    MCP_FUNC_START ("CCM_VAC_ConfigurationEngine_ChangeConfiguration");

    MCP_LOG_INFO (("CCM_VAC_ConfigurationEngine_ChangeConfiguration: changing configuration for operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify object pointer */
    MCP_VERIFY_FATAL ((NULL != ptConfigEngine), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("CCM_VAC_ConfigurationEngine_ChangeConfiguration: NULL object!"));

    /* verify operation is valid */
    MCP_VERIFY_ERR ((eOperation < CAL_OPERATION_MAX_NUM),
                    CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                    ("_CCM_VAC_ConfigurationEngine_StartOperation: invalid operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));	

    /* verify operation is supported */
    MCP_VERIFY_ERR ((MCP_TRUE == ptConfigEngine->tOperations[ eOperation ].bSupported),
                    CCM_VAC_STATUS_FAILURE_OPERATION_NOT_SUPPORTED,
                    ("_CCM_VAC_ConfigurationEngine_ChangeConfiguration: unsupported operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));

    pResourceList = &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);

    /* lock the semaphore (while accessing the mapping and allocation engines) */
    eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("CCM_VAC_ConfigurationEngine_ChangeConfiguration: semaphore returned error %d",
                       eOsStatus));

    /* verify operation is running */
    if (CCM_VAC_OPERATION_STATE_RUNNING != ptConfigEngine->tOperations[ eOperation ].eOperationState)
    {
        /* unlock the semaphore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                          ("_CCM_VAC_ConfigurationEngine_ChangeConfiguration: unlock semaphore status: %d",
                           eOsStatus));

        MCP_LOG_INFO (("CCM_VAC_ConfigurationEngine_ChangeConfiguration: operation %s is idle or undergoing configuration!",
                     _CCM_VAC_DebugOperationStr(eOperation)));

        return CCM_VAC_STATUS_FAILURE_BUSY;
    }

    /* get the required resources from the mapping engine */
    _CCM_VAC_MappingEngine_OperationToResourceList (ptConfigEngine->ptMappingEngine, eOperation,
                                                    pResourceList);

    /* change the state back to starting, to indicate operation is under configuration */
    ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_STARTING;

    /* unlock the semaphore */
    eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_ChangeConfiguration: unlock semaphore status: %d",
                       eOsStatus));

    /* initialize configuration stage */
    ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured = 0;
    ptConfigEngine->tOperations[ eOperation ].eEventToClient = CCM_VAC_EVENT_CONFIGURATION_CHANGED;
    MCP_HAL_MEMORY_MemCopy (&(ptConfigEngine->tOperations[ eOperation ].tDigitalConfig), ptConfig,
                            sizeof(TCAL_DigitalConfig));

    /* start configuration process */
    eConfigStatus = _CCM_VAC_ConfigurationEngine_ConfigResources (ptConfigEngine, eOperation);

    /* lock the semaphore again (while accessing the operation state) */
    eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("CCM_VAC_ConfigurationEngine_ChangeConfiguration: semaphore returned error %d",
                       eOsStatus));

    /* if all resources were synchronously configured */
    if (CCM_VAC_STATUS_SUCCESS == eConfigStatus)
    {
        MCP_LOG_INFO (("CCM_VAC_ConfigurationEngine_ChangeConfiguration: operation %s configuration stage finished!",
                       _CCM_VAC_DebugOperationStr(eOperation)));
        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_RUNNING;
        status = CCM_VAC_STATUS_SUCCESS;
    }
    /* if asynchronous configuration is required, the CB will complete the configuration */
    else if (CCM_VAC_STATUS_PENDING == eConfigStatus)
    {
        status = CCM_VAC_STATUS_PENDING;
    }
    else /* an error occured in the configuration process */
    {
        MCP_LOG_INFO (("CCM_VAC_ConfigurationEngine_ChangeConfiguration: operation %s configuration stage failed, "
                       "resource %s returned error %d!", 
                       _CCM_VAC_DebugResourceStr (pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                       _CCM_VAC_DebugOperationStr(eOperation), eConfigStatus));
        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;
        status = CCM_VAC_STATUS_FAILURE_UNSPECIFIED;
    }

    /* unlock the semaphore */
    eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
    MCP_VERIFY_FATAL ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus), CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
                      ("_CCM_VAC_ConfigurationEngine_ChangeConfiguration: unlock semaphore status: %d",
                       eOsStatus));

    MCP_FUNC_END ();

    return status;
}

void _CCM_VAC_ConfigurationEngine_CalCb (void *pUserData, 
                                         ECAL_RetValue eRetValue)
{
    TCCM_VAC_ConfigurationEngine    *ptConfigEngine = 
        ((TCCM_VAC_CECALUserData *)pUserData)->ptConfigEngine;
    ECAL_Operation                  eOperation = 
        ((TCCM_VAC_CECALUserData *)pUserData)->eOperation;
    TCAL_ResourceList               *pResourceList;
    ECCM_VAC_Status                 eConfigStatus;
    McpHalOsStatus                  eOsStatus;
    McpU32                          uIndex;

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_CalCb");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_CalCb: called for operation %s, with status %d",
                   _CCM_VAC_DebugOperationStr(eOperation), eRetValue));

    /* verify object pointer */
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptConfigEngine),
                                ("_CCM_VAC_ConfigurationEngine_CalCb: NULL object!"));
	
    /* verify operation is valid */	
    MCP_VERIFY_FATAL_NO_RETVAR ((eOperation < CAL_OPERATION_MAX_NUM),
                                ("_CCM_VAC_ConfigurationEngine_StartOperation: invalid operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));
		
    pResourceList = &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);

    /* if configuration failed for current resource */
    if (CAL_STATUS_SUCCESS != eRetValue)
    {
        MCP_LOG_ERROR (("_CCM_VAC_ConfigurationEngine_CalCb: configuration of resource %s for operation %s"
                        " failed with status %d, stopping configuration process",
                        _CCM_VAC_DebugResourceStr(pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                        _CCM_VAC_DebugOperationStr(eOperation), eRetValue));

        /* lock the semaphore (while changing the state) */
        eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalCb: semaphore returned error %d",
                                     eOsStatus));

        /* cancel the configuration operation */
        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;

        /* unlock the semaphore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalCb: unlock semaphore status: %d",
                                     eOsStatus));

        /* and call the operations callbacks indicating the error */
        for (uIndex = 0; uIndex < ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs; uIndex++)
        {
            ptConfigEngine->tOperations[ eOperation ].fCB[ uIndex ] 
                (eOperation, ptConfigEngine->tOperations[ eOperation ].eEventToClient,
                 CCM_VAC_STATUS_FAILURE_UNSPECIFIED);
        }

        return;
    }

    /* if configuration process was stopped, simply do nothing */
    if ((CCM_VAC_OPERATION_STATE_STARTING != ptConfigEngine->tOperations[ eOperation ].eOperationState))
    {
        MCP_LOG_ERROR (("_CCM_VAC_ConfigurationEngine_CalCb: operation %s is not undergoing configuration",
                        _CCM_VAC_DebugOperationStr(eOperation)));
        return;
    }

    /* advance to the next resource */
    ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured++;

    /* continue configuration process */
    eConfigStatus = _CCM_VAC_ConfigurationEngine_ConfigResources (ptConfigEngine, eOperation);

    /* if all remaining resources were synchronously configured */
    if (CCM_VAC_STATUS_SUCCESS == eConfigStatus)
    {
        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_CalCb: operation %s configuration stage finished!",
                       _CCM_VAC_DebugOperationStr(eOperation)));

        /* lock the semaphore (while changing the state) */
        eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalCb: semaphore returned error %d",
                                     eOsStatus));

        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_RUNNING;

        /* unlock the semaphore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalCb: unlock semaphore status: %d",
                                     eOsStatus));

        /* send completion event */
        for (uIndex = 0; uIndex < ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs; uIndex++)
        {
            ptConfigEngine->tOperations[ eOperation ].fCB[ uIndex ] 
                (eOperation, ptConfigEngine->tOperations[ eOperation ].eEventToClient,
                 CCM_VAC_STATUS_SUCCESS);
        }
    }
    /* if the status is not success or pending, a failure was encountered */
    else if (CCM_VAC_STATUS_PENDING != eConfigStatus)
    {
        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_CalCb: operation %s configuration stage failed, "
                       "resource %s returned error %d!", 
                       _CCM_VAC_DebugResourceStr (pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                       _CCM_VAC_DebugOperationStr(eOperation), eConfigStatus));

        /* lock the semaphore (while changing the state) */
        eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalCb: semaphore returned error %d",
                                     eOsStatus));

        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;

        /* unlock the semaphore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalCb: unlock semaphore status: %d",
                                     eOsStatus));

        /* send completion event */
        for (uIndex = 0; uIndex < ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs; uIndex++)
        {
            ptConfigEngine->tOperations[ eOperation ].fCB[ uIndex ] 
                (eOperation, ptConfigEngine->tOperations[ eOperation ].eEventToClient,
                 CCM_VAC_STATUS_FAILURE_UNSPECIFIED);
        }
    }
    /* for pending the function exists and will be called again once resource configuration is complete */

    MCP_FUNC_END ();
}

void _CCM_VAC_ConfigurationEngine_CalStopCb (void *pUserData, 
                                             ECAL_RetValue eRetValue)
{
    TCCM_VAC_ConfigurationEngine    *ptConfigEngine = 
        ((TCCM_VAC_CECALUserData *)pUserData)->ptConfigEngine;
    ECAL_Operation                  eOperation = 
        ((TCCM_VAC_CECALUserData *)pUserData)->eOperation;
    TCAL_ResourceList               *pResourceList;
    ECCM_VAC_Status                 eConfigStatus;
    McpU32                          uIndex;
    McpHalOsStatus                  eOsStatus;

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_CalStopCb");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_CalStopCb: called for operation %s, with status %d",
                   _CCM_VAC_DebugOperationStr(eOperation), eRetValue));

    /* verify object pointer */
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptConfigEngine),
                                ("_CCM_VAC_ConfigurationEngine_CalStopCb: NULL object!"));

    /* verify operation is valid */	
    MCP_VERIFY_FATAL_NO_RETVAR ((eOperation < CAL_OPERATION_MAX_NUM),
                                ("_CCM_VAC_ConfigurationEngine_StartOperation: invalid operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));
		
    pResourceList = &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);

    /* if configuration failed for current resource */
    if (CAL_STATUS_SUCCESS != eRetValue)
    {
        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_CalStopCb: de-configuration of resource %s for operation %s"
                       " failed with status %d, stopping de-configuration process",
                       _CCM_VAC_DebugResourceStr(pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                       _CCM_VAC_DebugOperationStr(eOperation), eRetValue));

        /* lock the semaphore (while changing the state) */
        eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalStopCb: semaphore returned error %d",
                                     eOsStatus));

        /* cancel the configuration operation */
        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;

        /* unlock the semaphore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalStopCb: unlock semaphore status: %d",
                                     eOsStatus));

        /* and call the operations callbacks indicating the error */
        for (uIndex = 0; uIndex < ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs; uIndex++)
        {
            ptConfigEngine->tOperations[ eOperation ].fCB[ uIndex ] 
                (eOperation, ptConfigEngine->tOperations[ eOperation ].eEventToClient,
                 CCM_VAC_STATUS_FAILURE_UNSPECIFIED);
        }

        return;
    }

    /* advance to the next resource */
    ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured++;

    /* continue configuration process */
    eConfigStatus = _CCM_VAC_ConfigurationEngine_StopResources (ptConfigEngine, eOperation);

    /* if all remaining resources were synchronously configured */
    if (CCM_VAC_STATUS_SUCCESS == eConfigStatus)
    {
        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_CalStopCb: operation %s de-configuration stage finished!",
                       _CCM_VAC_DebugOperationStr(eOperation)));

        /* lock the semaphore (while accessing the allocation engine) */
        eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalStopCb: "
                                     "lock semaphore returned error %d",
                                     eOsStatus));

        /* de-configuration completed - continue to de-allocating resources */
        _CCM_VAC_ConfigurationEngine_DeallocateOperationResources (ptConfigEngine, eOperation);

        /* mark that the operation is idle */
        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;

        /* unlock the sempahore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalStopCb: unlock semaphore status: %d",
                                     eOsStatus));

        /* send completion event */
        for (uIndex = 0; uIndex < ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs; uIndex++)
        {
            ptConfigEngine->tOperations[ eOperation ].fCB[ uIndex ] 
                (eOperation, ptConfigEngine->tOperations[ eOperation ].eEventToClient,
                 CCM_VAC_STATUS_SUCCESS);
        }
    }
    /* if the status is not success or pending, a failure was encountered */
    else if (CCM_VAC_STATUS_PENDING != eConfigStatus)
    {
        MCP_LOG_ERROR (("_CCM_VAC_ConfigurationEngine_CalStopCb: operation %s de-configuration stage failed, "
                        "resource %s returned error %d!", 
                        _CCM_VAC_DebugResourceStr (pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                        _CCM_VAC_DebugOperationStr(eOperation), eConfigStatus));

        /* lock the semaphore (while changing the state) */
        eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalStopCb: semaphore returned error %d",
                                    eOsStatus));

        /* mark the state as idle */
        ptConfigEngine->tOperations[ eOperation ].eOperationState = CCM_VAC_OPERATION_STATE_IDLE;

        /* unlock the semaphore */
        eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
        MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                    ("_CCM_VAC_ConfigurationEngine_CalStopCb: unlock semaphore status: %d",
                                    eOsStatus));

        /* send completion event */
        for (uIndex = 0; uIndex < ptConfigEngine->tOperations[ eOperation ].uNumberOfCBs; uIndex++)
        {
            ptConfigEngine->tOperations[ eOperation ].fCB[ uIndex ] 
                (eOperation, ptConfigEngine->tOperations[ eOperation ].eEventToClient,
                 CCM_VAC_STATUS_FAILURE_UNSPECIFIED);
        }
    }
    /* for pending the function exists and will be called again once resource configuration is complete */

    MCP_FUNC_END ();
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_ConfigResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                              ECAL_Operation eOperation)
{
    TCAL_ResourceList       *pResourceList;
    ECAL_RetValue           eConfigStatus = CAL_STATUS_FAILURE;

    /*
     * it is assumed the VAC semaphore is NOT locked when calling this function. This must hold to
     * avoid attempting to lock the stack semaphore (from the transport layer) when already holding
     * the VAC semaphore, which may lead to a deadlock
     */

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_ConfigResources");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_ConfigResources: continuing configuration for operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify operation is valid */	
    MCP_VERIFY_FATAL_NO_RETVAR ((eOperation < CAL_OPERATION_MAX_NUM),
                                ("_CCM_VAC_ConfigurationEngine_StartOperation: invalid operation %s",
                     _CCM_VAC_DebugOperationStr(eOperation)));
	
    pResourceList = &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);

    eConfigStatus = CAL_STATUS_SUCCESS;
    /* 
     * start configuration - configure resources as long as more resources need to be configured
     * and configuration is synchronous
     */
    while ((CAL_STATUS_SUCCESS == eConfigStatus) && 
           (ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured < 
            ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources.uNumOfResources))
    {
        /* configure current resource */
        eConfigStatus = CAL_ConfigResource (ptConfigEngine->pCAL, eOperation,
                                            pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ], 
                                            &(ptConfigEngine->tOperations[ eOperation ].tDigitalConfig),
                                            _CCM_VAC_AllocationEngine_GetResourceProperties (ptConfigEngine->ptAllocationEngine, 
                                                                                             pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                                            _CCM_VAC_ConfigurationEngine_CalCb, 
                                            (void *)&(ptConfigEngine->tOperations[ eOperation ].tCALUserData));

        /* if configuration was synchronous, advance to the next resource */
        if (CAL_STATUS_SUCCESS == eConfigStatus)
        {
            ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured++;
        }
        /* if configuration is pending, moving to the next resource would be preformed by the CAL callback */
    } 

    MCP_FUNC_END ();

    if (CAL_STATUS_SUCCESS == eConfigStatus)
    {
        return CCM_VAC_STATUS_SUCCESS;
    }
    else if (CAL_STATUS_PENDING == eConfigStatus)
    {
        return CCM_VAC_STATUS_PENDING;
    }
    else
    {
        return CCM_VAC_STATUS_FAILURE_UNSPECIFIED;
    }
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_StopResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                                            ECAL_Operation eOperation)
{
    TCAL_ResourceList       *pResourceList = 
        &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);
    ECAL_RetValue           eConfigStatus;

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_StopResources");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_StopResources: continuing de-configuration for operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    eConfigStatus = CAL_STATUS_SUCCESS;
    /* 
     * start configuration - configure resources as long as more resources need to be configured
     * and configuration is synchronous
     */
    while ((CAL_STATUS_SUCCESS == eConfigStatus) && 
           (ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured < 
            pResourceList->uNumOfResources))
    {
        /* de-configure current resource */
        eConfigStatus = CAL_StopResourceConfiguration (ptConfigEngine->pCAL, eOperation,
                                                       pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ], 
                                                       _CCM_VAC_AllocationEngine_GetResourceProperties (ptConfigEngine->ptAllocationEngine, 
                                                                                                        pResourceList->eResources[ ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured ]),
                                                       _CCM_VAC_ConfigurationEngine_CalStopCb, 
                                                       (void *)&(ptConfigEngine->tOperations[ eOperation ].tCALUserData));

        /* if configuration was synchronous, advance to the next resource */
        if (CAL_STATUS_SUCCESS == eConfigStatus)
        {
            ptConfigEngine->tOperations[ eOperation ].uResourceBeingConfigured++;
        }
        /* if configuration is pending, moving to the next resource would be preformed by the CAL stop callback */
    } 

    MCP_FUNC_END ();

    if (CAL_STATUS_SUCCESS == eConfigStatus)
    {
        return CCM_VAC_STATUS_SUCCESS;
    }
    else if (CAL_STATUS_PENDING == eConfigStatus)
    {
        return CCM_VAC_STATUS_PENDING;
    }
    else
    {
        return CCM_VAC_STATUS_FAILURE_UNSPECIFIED;
    }
}

ECCM_VAC_Status _CCM_VAC_ConfigurationEngine_AllocateOperationResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine, 
                                                                         ECAL_Operation eOperation,
                                                                         TCCM_VAC_UnavailResourceList *ptUnavailResources)
{
    McpU32                  uIndex;
    ECAL_Operation          eTempOperation;
    McpBool                 bAllocStatus[ CCM_VAC_MAX_NUM_OF_RESOURCES_PER_OP ];
    TCAL_ResourceList       *pResourceList = 
        &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);

    /* it is assumed that the VAC semaphore is locked when this function is called!!! */

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_AllocateOperationResources");

    MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_AllocateOperationResources: allocating resources"
                   " for operation %s", _CCM_VAC_DebugOperationStr(eOperation)));

    /* initialize the unavailable resource list to an empty list */
    ptUnavailResources->uNumOfUnavailResources = 0;

    /* get the required resources from the mapping engine */
    _CCM_VAC_MappingEngine_OperationToResourceList (ptConfigEngine->ptMappingEngine, eOperation,
                                                    pResourceList);

    /* for each required resource */
    for (uIndex = 0; 
         uIndex < pResourceList->uNumOfResources;
         uIndex++)
    {
        /* set the operation pointer to the required operation */
        eTempOperation = eOperation;

        /* try allocate the resource */
        bAllocStatus[ uIndex ] = _CCM_VAC_AllocationEngine_TryAllocate (ptConfigEngine->ptAllocationEngine,
                                                                        pResourceList->eResources[ uIndex ],
                                                                        &eTempOperation);

        /* if allocation failed */
        if (MCP_FALSE == bAllocStatus[ uIndex ])
        {
            /* mark this resource in the unavailable resource list */
            ptUnavailResources->tUnavailResources[ ptUnavailResources->uNumOfUnavailResources ].eOperation = eTempOperation;
            ptUnavailResources->tUnavailResources[ ptUnavailResources->uNumOfUnavailResources ].eResource = pResourceList->eResources[ uIndex ];
            ptUnavailResources->uNumOfUnavailResources++;
        }
    }

    /* if not all resources were allocated successfuly */
    if (0 < ptUnavailResources->uNumOfUnavailResources)
    {
        MCP_LOG_INFO (("_CCM_VAC_ConfigurationEngine_AllocateOperationResources: resources for "
                       "operation %s could not be allocated",
                       _CCM_VAC_DebugOperationStr(eOperation)));

        /* release all resources that were allocated */
        for (uIndex = 0; 
             uIndex < pResourceList->uNumOfResources;
             uIndex++)
        {
            /* if the resource was successfully allocated */
            if (MCP_TRUE == bAllocStatus[ uIndex ])
            {
                /* release the resource */
                _CCM_VAC_AllocationEngine_Release (ptConfigEngine->ptAllocationEngine,
                                                   pResourceList->eResources[ uIndex ],
                                                   eOperation);
            }
        }

        /* return failure status */
        return CCM_VAC_STATUS_FAILURE_UNAVAILABLE_RESOURCES;
    }
    /* all resources were allocated successfully */
    else
    {
        /* return success! */
        return CCM_VAC_STATUS_SUCCESS;
    }

    MCP_FUNC_END ();
}

void _CCM_VAC_ConfigurationEngine_DeallocateOperationResources (TCCM_VAC_ConfigurationEngine *ptConfigEngine, 
                                                                ECAL_Operation eOperation)
{
    McpU32                  uIndex;
    TCAL_ResourceList       *pResourceList = 
        &(ptConfigEngine->tOperations[ eOperation ].tCurrentRequiredResources);

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_DeallocateOperationResources");

    /* for each required resource */
    for (uIndex = 0; 
         uIndex < pResourceList->uNumOfResources;
         uIndex++)
    {
        /* release the resource */
        _CCM_VAC_AllocationEngine_Release (ptConfigEngine->ptAllocationEngine, 
                                           pResourceList->eResources[ uIndex ], eOperation);
    }

    MCP_FUNC_END ();
}

void _CCM_VAC_ConfiguratioEngine_MaskToList (ECAL_ResourceMask eResourceMask, 
                                             TCAL_ResourceList *ptResourceList)
{
    McpU32      uIndex;

    /* nullify the resource list */
    ptResourceList->uNumOfResources = 0;

    /* for all resources */
    for (uIndex = 0; uIndex < CAL_RESOURCE_MAX_NUM; uIndex++)
    {
        /* if the resource is on the resource mask */
        if (0 != (eResourceMask & (1 << uIndex)))
        {
            ptResourceList->eResources[ ptResourceList->uNumOfResources ] = uIndex;
            ptResourceList->uNumOfResources++;
        }
    }
}

void _CCM_VAC_ConfigurationEngine_SetResourceProperties (TCCM_VAC_ConfigurationEngine *ptConfigEngine,
                                    ECAL_Resource eResource,
                                    TCAL_ResourceProperties *pProperties)
{
    McpHalOsStatus  eOsStatus;

    MCP_FUNC_START ("_CCM_VAC_ConfigurationEngine_SetResourceProperties");

    /* lock the semaphore (while accessing the allocation engine) */
    eOsStatus = MCP_HAL_OS_LockSemaphore (ptConfigEngine->tSemaphore, MCP_HAL_OS_TIME_INFINITE);

    MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                ("_CCM_VAC_ConfigurationEngine_SetResourceProperties: semaphore returned error %d",
                                 eOsStatus));

    /* set the properties */
    _CCM_VAC_AllocationEngine_SetResourceProperties (ptConfigEngine->ptAllocationEngine,
                                                     eResource,
                                                     pProperties);

    /* unlock the semaphore */
    eOsStatus = MCP_HAL_OS_UnlockSemaphore (ptConfigEngine->tSemaphore);
    MCP_VERIFY_FATAL_NO_RETVAR ((MCP_HAL_OS_STATUS_SUCCESS == eOsStatus),
                                ("_CCM_VAC_ConfigurationEngine_SetResourceProperties: unlock semaphore status: %d",
                                 eOsStatus));

    MCP_FUNC_END ();
}


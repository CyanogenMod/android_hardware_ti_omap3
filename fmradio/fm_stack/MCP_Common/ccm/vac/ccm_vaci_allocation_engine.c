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
*   FILE NAME:      ccm_vaci_allocation_engine.c
*
*   BRIEF:          This file defines the implementation of the Connectivity Chip
*                   Manager (CCM) Voice and Audio Control (VAC) allocation 
*                   engine component.
*                  
*
*   DESCRIPTION:    The Allocation engine is a CCM-VAC internal module storing
*                   audio resources allocations to operations.
*
*   AUTHOR:         Ronen Kalish
*
\*******************************************************************************/

#include "ccm_vaci_allocation_engine.h"
#include "mcp_defs.h"
#include "ccm_vaci_debug.h"
#include "ccm_config.h"
#include "ccm_vaci_chip_abstration.h"
#include "mcp_hal_memory.h"
#include "mcp_hal_log.h"

MCP_HAL_LOG_SET_MODULE(MCP_HAL_LOG_MODULE_TYPE_CCM_VAC);

typedef struct _TCCM_VAC_AEResource
{
    McpBool                         bAvailable;             /* whether the resource is available in the system */
    McpU32                          uNumberOfOwners;        /* number of current resource owners */
    ECAL_Operation                  eCurrentOwners[ 2 ];    /* current resource owner(s) (if any) */
    TCAL_ResourceProperties         tProperties;            /* resource properties */
} TCCM_VAC_AEResource;

struct _TCCM_VAC_AllocationEngine
{
    TCCM_VAC_AEResource             tResources[ CAL_RESOURCE_MAX_NUM ]; /* all system resources */
    TCAL_OpPairConfig               tConfig;                            /* allowed pairs config */
    McpConfigParser                 *pConfigParser;                     /* configuration parser object (holding VAC ini file) */
};

/* internal prototypes */
McpBool _CCM_VAC_AEIsPairAllowed (TCCM_VAC_AllocationEngine *ptAllocEngine, 
                                  ECAL_Resource eResource,
                                  TCAL_OperationPair *ptPair);

/* the actual allocation engine objects */
TCCM_VAC_AllocationEngine    tAEObjects[ MCP_HAL_MAX_NUM_OF_CHIPS ];

/* function implementation */
ECCM_VAC_Status _CCM_VAC_AllocationEngine_Create (McpHalChipId chipId,
                                                  McpConfigParser *pConfigParser,
                                                  TCCM_VAC_AllocationEngine **ptAllocEngine)
{
    McpU32      uIndex;

    MCP_FUNC_START ("_CCM_VAC_AllocationEngine_Create");

    *ptAllocEngine = &(tAEObjects[ chipId ]);
    
    /* store ini file configuration object */
    tAEObjects[ chipId ].pConfigParser = pConfigParser;

    /* initialize resources */
    for (uIndex = 0; uIndex < CAL_RESOURCE_MAX_NUM; uIndex++)
    {
        (*ptAllocEngine)->tResources[ uIndex ].uNumberOfOwners = 0;
        (*ptAllocEngine)->tResources[ uIndex ].bAvailable = MCP_FALSE;
        (*ptAllocEngine)->tResources[ uIndex ].tProperties.uPropertiesNumber = 0;
    }

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_AllocationEngine_Configure (TCCM_VAC_AllocationEngine *ptAllocEngine, 
                                                     TCAL_ResourceSupport *ptAvailResources,
                                                     TCAL_OperationSupport *ptAvailOperations)
{
    McpConfigParserStatus   eCPStatus;
    McpU32                  uResourcesNum, uIndex;
    McpU8                   *pTempValue;
    ECAL_Resource           eResource;
    ECCM_VAC_Status         status;

    MCP_FUNC_START ("_CCM_VAC_AllocationEngine_Configure");

    MCP_UNUSED_PARAMETER (ptAvailOperations);

    /* configure available resources: */
    /* 1. update all available external resources (read from ini file and compare with CAL) */

    /* get number of resources from ini file */
    eCPStatus = MCP_CONFIG_PARSER_GetIntegerKey (ptAllocEngine->pConfigParser, 
                                                 (McpU8 *)CCM_VAC_CONFIG_INI_RESOURCES_SECTION_NAME,
                                                 (McpU8 *)CCM_VAC_CONFIG_INI_RESOURCES_NUMBER_KEY_NAME,
                                                 (McpS32*)&uResourcesNum);
    MCP_VERIFY_FATAL (((eCPStatus == MCP_CONFIG_PARSER_STATUS_SUCCESS) && (0 < uResourcesNum)),
                      CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                      ("_CCM_VAC_AllocationEngine_Configure: error reading number of resources "
                       "from file, number: %d, status: %d", uResourcesNum, eCPStatus));

    /* for each resource */
    for (uIndex = 0; uIndex < uResourcesNum; uIndex++)
    {
        /* read resource name */
        eCPStatus = MCP_CONFIG_PARSER_GetAsciiKey (ptAllocEngine->pConfigParser, 
                                                   (McpU8 *)CCM_VAC_CONFIG_INI_RESOURCES_SECTION_NAME,
                                                   (McpU8 *)CCM_VAC_CONFIG_INI_RESOURCE_NAME_KEY_NAME,
                                                   &pTempValue);
        MCP_VERIFY_FATAL (((eCPStatus == MCP_CONFIG_PARSER_STATUS_SUCCESS) && (NULL != pTempValue)),
                          CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                          ("_CCM_VAC_AllocationEngine_Configure: Can't get resource name for resource number %d",
                           uIndex));

        /* convert to enum value */
        eResource = StringtoResource (pTempValue);
        MCP_VERIFY_FATAL ((CAL_RESOURCE_INVALID != eResource),
                          CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                          ("_CCM_VAC_AllocationEngine_Configure: Can't convert resource number %d, "
                           "string %s to enum value", uIndex, pTempValue));

        /* verify it is available by the chip */
        MCP_VERIFY_FATAL ((MCP_TRUE == ptAvailResources->bResourceSupport[ eResource ]),
                          CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                          ("_CCM_VAC_AllocationEngine_Configure: resource %s required by ini file"
                           "is not supported by chip!", _CCM_VAC_DebugResourceStr(eResource)));

        /* and finally, mark it as enabled */
        ptAllocEngine->tResources[ eResource ].bAvailable = MCP_TRUE;
    }

    /* 2. update all available internal resources (read from CAL) */
    for (uIndex = CAL_RESOURCE_LAST_EX_RESOURCE + 1; uIndex < CAL_RESOURCE_MAX_NUM; uIndex++)
    {
        /* if the internal resource is supported by the chip, mark it as supported */
        ptAllocEngine->tResources[ uIndex ].bAvailable = ptAvailResources->bResourceSupport[ uIndex ];
    }

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

void _CCM_VAC_AllocationEngine_Destroy (TCCM_VAC_AllocationEngine **ptAllocEngine)
{
    MCP_FUNC_START ("_CCM_VAC_AllocationEngine_Destroy");

    /* nullify the object pointer to avoid future use */
    *ptAllocEngine = NULL;

    MCP_FUNC_END ();
}

McpBool _CCM_VAC_AllocationEngine_TryAllocate (TCCM_VAC_AllocationEngine *ptAllocEngine,
                                               ECAL_Resource eResource, 
                                               ECAL_Operation *peOperation)
{
    McpBool     status = MCP_FALSE;

    MCP_FUNC_START ("_CCM_VAC_AllocationEngine_TryAllocate");

    MCP_LOG_INFO (("_CCM_VAC_AllocationEngine_TryAllocate: operation %s requesting resource %s",
                   _CCM_VAC_DebugOperationStr(*peOperation),
                   _CCM_VAC_DebugResourceStr (eResource)));

    /* verify object pointer and resource availability */
    MCP_VERIFY_FATAL ((NULL != ptAllocEngine), MCP_FALSE, 
                      ("_CCM_VAC_AllocationEngine_TryAllocate: NULL object!"));
    MCP_VERIFY_ERR ((MCP_TRUE == ptAllocEngine->tResources[ eResource ].bAvailable),
                    MCP_FALSE,
                    ("_CCM_VAC_AllocationEngine_TryAllocate: attempting to allocate resource %s "
                     "which is unavailable, for operation %s", _CCM_VAC_DebugResourceStr (eResource),
                     _CCM_VAC_DebugOperationStr (*peOperation)));

    /* if the resource is currently not allocated at all */
    if (0 == ptAllocEngine->tResources[ eResource ].uNumberOfOwners)
    {
        MCP_LOG_INFO (("_CCM_VAC_AllocationEngine_TryAllocate: no current owner, resource is allocated"));

        /* allocate the resource to the requesting operation */
        ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ] = *peOperation;
        ptAllocEngine->tResources[ eResource ].uNumberOfOwners++;
        status = MCP_TRUE;
    }
    /* one operation is already holding the resource */
    else if (1 == ptAllocEngine->tResources[ eResource ].uNumberOfOwners)
    {
        TCAL_OperationPair      tNewPair;

        /* first check if this is a re-allocation (e.g. for resource change) */
        if (*peOperation == ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ])
        {
            MCP_LOG_INFO (("_CCM_VAC_AllocationEngine_TryAllocate: operation %s already owning the "
                           "resource, allocation succeeds", 
                           _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ])));
            status = MCP_TRUE;
        }
        /* now check if both operations may share the resource */
        else
        {
            /* fill in the new operation pair */
            tNewPair.eOperations[ 0 ] = ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ];
            tNewPair.eOperations[ 1 ] = *peOperation;
    
            /* if the new pair is allowed */
            if (MCP_TRUE == _CCM_VAC_AEIsPairAllowed (ptAllocEngine, eResource, &tNewPair))
            {
                MCP_LOG_INFO (("_CCM_VAC_AllocationEngine_TryAllocate: operation %s already owning the "
                                   "resource, operation pair is allowed, allocation succeeded", 
                                   _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ])));
    
                /* fill in the new operation mutually owning the resource */
                ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 1 ] = *peOperation;
                ptAllocEngine->tResources[ eResource ].uNumberOfOwners++;
                status = MCP_TRUE;
            }
            else
            {
                MCP_LOG_INFO (("_CCM_VAC_AllocationEngine_TryAllocate: operation %s already owns the "
                               "resource, allocation failed", 
                               _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ])));

                /* indicate current owner */
                *peOperation = ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ];
                status = MCP_FALSE;
            }
        }
    }
    /* two operations are currently mutually owning the resource */
    else
    {
        /* first check if this is a re-allocation */
        if ((*peOperation == ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ]) || 
            (*peOperation == ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 1 ]))
        {
            MCP_LOG_INFO (("_CCM_VAC_AllocationEngine_TryAllocate: operations %s and %s already owning the "
                           "resource, allocation succeeded", 
                           _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ]),
                           _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 1 ])));
            status = MCP_TRUE;
        }
        else
        {
            MCP_LOG_INFO (("_CCM_VAC_AllocationEngine_TryAllocate: operations %s and %s already owning the "
                           "resource, allocation failed", 
                           _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ]),
                           _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 1 ])));
    
            /* the current owner is indicated as the first operation */
            *peOperation = ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ];
            status = MCP_FALSE;
        }
    }

    MCP_FUNC_END ();

    return status;
}

void _CCM_VAC_AllocationEngine_Release (TCCM_VAC_AllocationEngine *ptAllocEngine,
                                        ECAL_Resource eResource,
                                        ECAL_Operation eOperation)
{
    MCP_FUNC_START ("_CCM_VAC_AllocationEngine_Release");

    MCP_LOG_INFO (("_CCM_VAC_AllocationEngine_Release: operation %s releasing resource %s",
                   _CCM_VAC_DebugOperationStr(eOperation),
                   _CCM_VAC_DebugResourceStr (eResource)));

    /* verify object pointer, resource availability, and resource allocation */
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptAllocEngine),
                                ("_CCM_VAC_AllocationEngine_Release: NULL object!"));
    MCP_VERIFY_ERR_NO_RETVAR ((MCP_TRUE == ptAllocEngine->tResources[ eResource ].bAvailable),
                              ("_CCM_VAC_AllocationEngine_Release: attempting to release resource %s "
                               "which is unavailable, by operation %s",
                               _CCM_VAC_DebugResourceStr (eResource),
                               _CCM_VAC_DebugOperationStr (eOperation)));
    MCP_VERIFY_ERR_NO_RETVAR ((0 < ptAllocEngine->tResources[ eResource ].uNumberOfOwners),
                              ("_CCM_VAC_AllocationEngine_Release: resource %s is not allocated!",
                               _CCM_VAC_DebugResourceStr (eResource)));
    
    /* if the resource is allocated by only one operation */
    if (1 == ptAllocEngine->tResources[ eResource ].uNumberOfOwners)
    {
        /* verify resource owner is the requesting operation */
        MCP_VERIFY_ERR_NO_RETVAR ((ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ] == eOperation),
                                  ("_CCM_VAC_AllocationEngine_Release: operation %s requesting to release "
                                   "resource %s, which is allocated to operation %s!",
                                   _CCM_VAC_DebugOperationStr(eOperation),
                                   _CCM_VAC_DebugResourceStr (eResource),
                                   _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ])));

        /* release the resource */
        ptAllocEngine->tResources[ eResource ].uNumberOfOwners--;
    }
    /* the resource is mutually allocated by two operations */
    else
    {
        /* the requesting operation is in first place */
        if (ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ] == eOperation)
        {
            /* move the operation in second place to first place */
            ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ] = 
                ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 1 ];
            /* reduce number of owners */
            ptAllocEngine->tResources[ eResource ].uNumberOfOwners--;
        }
        /* the requesting operation is in second place */
        else if (ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 1 ] == eOperation)
        {
            /* simply reduce number of owners */
            ptAllocEngine->tResources[ eResource ].uNumberOfOwners--;
        }
        else
        {
            MCP_ERR_NO_RETVAR (("_CCM_VAC_AllocationEngine_Release: resource %s ia allocated to "
                                "operations %s and %s!",
                                _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 0 ]),
                                _CCM_VAC_DebugOperationStr(ptAllocEngine->tResources[ eResource ].eCurrentOwners[ 1 ])));
        }
    }

    MCP_FUNC_END ();
}

void _CCM_VAC_AllocationEngine_SetResourceProperties (TCCM_VAC_AllocationEngine *ptAllocEngine,
                                                      ECAL_Resource eResource,
                                                      TCAL_ResourceProperties *pProperties)
{
    MCP_FUNC_START ("_CCM_VAC_AESetResourceProperties");

    /* copy resource properties */
    MCP_HAL_MEMORY_MemCopy ((void *)&(ptAllocEngine->tResources[ eResource ].tProperties),
                            (void *)pProperties, sizeof (TCAL_ResourceProperties));

    MCP_FUNC_END ();
}

TCAL_ResourceProperties *_CCM_VAC_AllocationEngine_GetResourceProperties (TCCM_VAC_AllocationEngine *ptAllocEngine,
                                                                          ECAL_Resource eResource)
{
    MCP_FUNC_START ("_CCM_VAC_AESetResourceProperties");

    return &(ptAllocEngine->tResources[ eResource ].tProperties);

    MCP_FUNC_END ();
}

TCAL_OpPairConfig *_CCM_VAC_AllocationEngine_GetConfigData (TCCM_VAC_AllocationEngine *ptAllocEngine)
{
    return &(ptAllocEngine->tConfig);
}

McpBool _CCM_VAC_AEIsPairAllowed (TCCM_VAC_AllocationEngine *ptAllocEngine, 
                                  ECAL_Resource eResource,
                                  TCAL_OperationPair *ptPair)
{
    McpU32                      uIndex;
    TCAL_OperationPair          *ptCurrentPair;

    MCP_FUNC_START ("_TCCM_VAC_AEIsPairAllowed");

    /* check all allowed pairs, until a match is found or list is exhusted */
    for (uIndex = 0; 
         uIndex < ptAllocEngine->tConfig.tAllowedPairs[ eResource ].uNumOfAllowedPairs; 
         uIndex++)
    {
        /* get current pair pointer */
        ptCurrentPair = &(ptAllocEngine->tConfig.tAllowedPairs[ eResource ].tOpPairs[ uIndex ]);

        /* compare given pair to current pair (resource order is of no significance) */
        if (((ptCurrentPair->eOperations[ 0 ] == ptPair->eOperations[ 0 ]) && 
             (ptCurrentPair->eOperations[ 1 ] == ptPair->eOperations[ 1 ])) ||
            ((ptCurrentPair->eOperations[ 0 ] == ptPair->eOperations[ 1 ]) && 
             (ptCurrentPair->eOperations[ 1 ] == ptPair->eOperations[ 0 ])))
        {
            /* pairs match - pair is allowed for this resource */
            return MCP_TRUE;
        }
    }

    /* no match was found - pair is not allowed for this resource */
    return MCP_FALSE;

    MCP_FUNC_END ();
}


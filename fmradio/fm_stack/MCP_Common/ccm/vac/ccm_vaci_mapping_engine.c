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
*   FILE NAME:      ccm_vaci_mapping_engine.c
*
*   BRIEF:          This file defines the implementation of the Connectivity Chip
*                   Manager (CCM) Voice and Audio Control (VAC) mapping
*                   engine component.
*                  
*
*   DESCRIPTION:    The mapping engine is a CCM-VAC internal module storing
*                   possible and current mapping between operations and 
*                   audio resources.
*
*   AUTHOR:         Ronen Kalish
*
\*******************************************************************************/

#include "ccm_vaci_mapping_engine.h"
#include "mcp_defs.h"
#include "ccm_vaci_debug.h"
#include "ccm_config.h"
#include "ccm_vaci_chip_abstration.h"
#include "mcp_hal_memory.h"
#include "mcp_hal_log.h"

MCP_HAL_LOG_SET_MODULE(MCP_HAL_LOG_MODULE_TYPE_CCM_VAC);

struct _TCCM_VAC_MappingEngine
{
    TCAL_MappingConfiguration           tConfig;
    McpConfigParser                     *pConfigParser;
};

/* the actual mapping engine objects */
TCCM_VAC_MappingEngine    tMEObjects[ MCP_HAL_MAX_NUM_OF_CHIPS ];

/* forward declarations */
McpBool _CCM_VAC_MEIsResourceOnList (ECAL_Resource eResource, TCAL_ResourceList *ptList);

ECCM_VAC_Status _CCM_VAC_MappingEngine_Create (McpHalChipId chipId,
                                               McpConfigParser *pConfigParser,
                                               TCCM_VAC_MappingEngine **ptMappEngine)
{
    MCP_FUNC_START ("_CCM_VAC_MappingEngine_Create");

    *ptMappEngine = &(tMEObjects[ chipId ]);

    /* store ini file configuration object */
    tMEObjects[ chipId ].pConfigParser = pConfigParser;

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

ECCM_VAC_Status _CCM_VAC_MappingEngine_Configure (TCCM_VAC_MappingEngine *ptMappEngine, 
                                                  TCAL_ResourceSupport *ptAvailResources,
                                                  TCAL_OperationSupport *ptAvailOperations)
{
    McpU32                      uOperationNum, uOperationIndex, uOptResourceNum, uOptResourceIndex, uIndex;
    McpConfigParserStatus       eCPStatus;
    McpU8                       *pTempValue;
    ECAL_Operation              eOperation;
    ECAL_Resource               eResource;
    TCAL_OptionalResource       tTempOptResource;
    ECCM_VAC_Status             status;

    /*
     * The configuration process involves reading chip capabilities and ini file settings 
     * (host capabilities). Chip capabilities are read by the configuration engine and are 
     * set in advance to the mapping engine configuration. This function than read the 
     * mapping section of the ini file. For every operation it read the possible optional 
     * resources and arrange them accordingly. It also read the default resource to be used
     * and set it.
     */

    MCP_FUNC_START ("_CCM_VAC_MappingEngine_Configure");

    MCP_UNUSED_PARAMETER (ptAvailOperations);
    MCP_UNUSED_PARAMETER (ptAvailResources);

    /* read number of operations from ini file */
    eCPStatus = MCP_CONFIG_PARSER_GetIntegerKey (ptMappEngine->pConfigParser, 
                                                 (McpU8 *)CCM_VAC_CONFIG_INI_OPERATIONS_SECTION_NAME,
                                                 (McpU8 *)CCM_VAC_CONFIG_INI_OPERATIONS_NUMBER_KEY_NAME,
                                                 (McpS32 *)&uOperationNum);
    MCP_VERIFY_FATAL (((eCPStatus == MCP_CONFIG_PARSER_STATUS_SUCCESS) && (0 < uOperationNum)),
                        CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                        ("_CCM_VAC_MappingEngine_Configure: error reading number of operations "
                         "from file, number: %d, status: %d", uOperationNum, eCPStatus));

    /* for every operation */
    for (uOperationIndex = 0; uOperationIndex < uOperationNum; uOperationIndex++)
    {
        /* read operation */
        eCPStatus = MCP_CONFIG_PARSER_GetAsciiKey (ptMappEngine->pConfigParser,
                                                   (McpU8 *)CCM_VAC_CONFIG_INI_MAPPING_SECTION_NAME,
                                                   (McpU8 *)CCM_VAC_CONFIG_INI_MAPPING_OP_KEY_NAME,
                                                   &pTempValue);
        MCP_VERIFY_FATAL ((eCPStatus == MCP_CONFIG_PARSER_STATUS_SUCCESS),
                          CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                          ("_CCM_VAC_MappingEngine_Configure: error reading operation name, status: %d",
                           eCPStatus));

        /* convert operation name to enum value */
        eOperation = StringtoOperation ((const McpU8*)pTempValue);
        MCP_VERIFY_FATAL ((CAL_OPERATION_INVALID != eOperation),
                          CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                          ("_CCM_VAC_MappingEngine_Configure: erro converting string %s to operation",
                           pTempValue));

        /* read number of optional resources */
        eCPStatus = MCP_CONFIG_PARSER_GetIntegerKey (ptMappEngine->pConfigParser, 
                                                     (McpU8 *)CCM_VAC_CONFIG_INI_MAPPING_SECTION_NAME,
                                                     (McpU8 *)CCM_VAC_CONFIG_INI_MAPPING_CONFIG_NUM_KEY_NAME,
                                                     (McpS32*)&uOptResourceNum);
        MCP_VERIFY_FATAL ((eCPStatus == MCP_CONFIG_PARSER_STATUS_SUCCESS),
                          CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                          ("_CCM_VAC_MappingEngine_Configure: error reading number of optional resources for "
                           "operation %s, status %d",  _CCM_VAC_DebugOperationStr(eOperation), eCPStatus));

        /* for each optional resource */
        for (uOptResourceIndex = 0; uOptResourceIndex < uOptResourceNum; uOptResourceIndex++)
        {
            /* read resource name */
            eCPStatus = MCP_CONFIG_PARSER_GetAsciiKey (ptMappEngine->pConfigParser,
                                                       (McpU8 *)CCM_VAC_CONFIG_INI_MAPPING_SECTION_NAME,
                                                       (McpU8 *)CCM_VAC_CONFIG_INI_MAPPING_RESOURCE_KEY_NAME,
                                                       &pTempValue);
            MCP_VERIFY_FATAL ((eCPStatus == MCP_CONFIG_PARSER_STATUS_SUCCESS),
                              CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                              ("_CCM_VAC_MappingEngine_Configure: error reading resource name (index %d) "
                               "for operation %s, status: %d", uOptResourceIndex, 
                               _CCM_VAC_DebugOperationStr(eOperation), eCPStatus));

            /* convert resource name to enum value */
            eResource = StringtoResource ((const McpU8 *)pTempValue);
            MCP_VERIFY_FATAL ((CAL_RESOURCE_INVALID != eResource),
                              CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                              ("_CCM_VAC_MappingEngine_Configure: error converting string %s to resource",
                               pTempValue));

            /* find the resource entry in the optional resources list */
            for (uIndex = 0; 
                 uIndex < ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.uNumOfResourceLists;
                 uIndex++)
            {
                if (eResource == ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].eOptionalResource)
                {
                    break;
                }
            }

            /* verify resource was found */
            MCP_VERIFY_FATAL ((uIndex < ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.uNumOfResourceLists),
                              CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                              ("_CCM_VAC_MappingEngine_Configure: optional resource %s for operation %s in "
                               "ini file is not within chip capabilities", 
                               _CCM_VAC_DebugResourceStr(eResource), _CCM_VAC_DebugOperationStr(eOperation)));

            /* if needed, move it to the beginning of the list */
            if (uIndex > uOptResourceIndex)
            {
                /* copy resource at the list beginning to temp storage */
                MCP_HAL_MEMORY_MemCopy (&tTempOptResource, 
                                        &(ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uOptResourceIndex ]), 
                                        sizeof (TCAL_OptionalResource));

                /* copy found resource to list beginning */
                MCP_HAL_MEMORY_MemCopy (&(ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uOptResourceIndex ]), 
                                        &(ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ]), 
                                        sizeof (TCAL_OptionalResource));

                /* finally, copy back temp storage to found resource */
                MCP_HAL_MEMORY_MemCopy (&(ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ]),
                                        &tTempOptResource,
                                        sizeof (TCAL_OptionalResource));
            }

            /* mark the resource as unused */
            ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uOptResourceIndex ].bIsUsed = MCP_FALSE;
        }

        /* set number of possible optional resources */
        ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.uNumOfResourceLists = uOptResourceNum;

        /* if there are optional resources, read and indicate the default optional resource */
        if (0 < uOptResourceNum)
        {
            /* read the default resource */
            eCPStatus = MCP_CONFIG_PARSER_GetAsciiKey (ptMappEngine->pConfigParser,
                                                       (McpU8 *)CCM_VAC_CONFIG_INI_MAPPING_SECTION_NAME,
                                                       (McpU8 *)CCM_VAC_CONFIG_INI_MAPPING_DEF_RESOURCE_KEY_NAME,
                                                       &pTempValue);
            MCP_VERIFY_FATAL ((eCPStatus == MCP_CONFIG_PARSER_STATUS_SUCCESS),
                              CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                              ("_CCM_VAC_MappingEngine_Configure: error reading default resource name "
                               "for operation %s, status: %d", uOptResourceIndex, 
                               _CCM_VAC_DebugOperationStr(eOperation), eCPStatus));

            /* convert default resource name to enum value */
            eResource = StringtoResource ((const McpU8 *)pTempValue);
            MCP_VERIFY_FATAL ((CAL_RESOURCE_INVALID != eResource),
                              CCM_VAC_STATUS_FAILURE_INVALID_CONFIGURATION,
                              ("_CCM_VAC_MappingEngine_Configure: error converting string %s to resource",
                               pTempValue));

            /* search default resource within optional resources */
            for (uIndex = 0; 
                 uIndex < ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.uNumOfResourceLists;
                 uIndex++)
            {
                /* if this is the default resource */
                if (eResource == ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].eOptionalResource)
                {
                    /* mark it as current mapping for this operation */
                    ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].bIsUsed = MCP_TRUE;
                }
            } 
        }
    }

    MCP_FUNC_END ();

    return CCM_VAC_STATUS_SUCCESS;
}

void _CCM_VAC_MappingEngine_Destroy (TCCM_VAC_MappingEngine **ptMappEngine)
{
    MCP_FUNC_START ("_CCM_VAC_MappingEngine_Destroy");

    /* nullify the object pointer to avoid future use */
    *ptMappEngine = NULL;

    MCP_FUNC_END ();
}

void _CCM_VAC_MappingEngine_OperationToResourceList (TCCM_VAC_MappingEngine *ptMappEngine, 
                                                     ECAL_Operation eOperation,
                                                     TCAL_ResourceList *ptResourceList)
{
    McpU32                      uIndex, uOptListsIndex;
    TCAL_ResourceList           *ptTempList;

    MCP_FUNC_START ("_CCM_VAC_MappingEngine_OperationToResourceList");

    MCP_LOG_INFO (("_CCM_VAC_MappingEngine_OperationToResourceList: requesting resource list for operation %s",
                   _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify object pointer and list pointer */
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptMappEngine), 
                                ("_CCM_VAC_MappingEngine_OperationToResourceList: NULL object!"));
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptResourceList), 
                                ("_CCM_VAC_MappingEngine_OperationToResourceList: NULL resource list!"));

    /* nullify the output resource list */
    ptResourceList->uNumOfResources = 0;

    /* First copy all mandatory resources */
    ptTempList = &(ptMappEngine->tConfig.tOpToResMap[ eOperation ].tMandatoryResources);
    for (uIndex = 0; uIndex < ptTempList->uNumOfResources; uIndex++)
    {
        ptResourceList->eResources[ ptResourceList->uNumOfResources ] = ptTempList->eResources[ uIndex ];
        ptResourceList->uNumOfResources++;
    }

    /* than copy all optional resources lists currently in use */
    for (uOptListsIndex = 0; 
         uOptListsIndex < ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.uNumOfResourceLists;
         uOptListsIndex++)
    {
        /* if this optional resource is in use at the moment */
        if (MCP_TRUE == 
            ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uOptListsIndex ].bIsUsed)
        {
            /* copy the optional resource */
            ptResourceList->eResources[ ptResourceList->uNumOfResources ] =
                ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uOptListsIndex ].eOptionalResource;
            ptResourceList->uNumOfResources++;

            /* set the pointer to the derived resources list */
            ptTempList = 
                &(ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uOptListsIndex ].tDerivedResourcesList);

            /* now copy all derived resources in the list */
            for (uIndex = 0; uIndex < ptTempList->uNumOfResources; uIndex++)
            {
                ptResourceList->eResources[ ptResourceList->uNumOfResources ] = ptTempList->eResources[ uIndex ];
                ptResourceList->uNumOfResources++;
            }
        }
    }

    MCP_FUNC_END ();
}

void _CCM_VAC_MappingEngine_GetOptionalResourcesList (TCCM_VAC_MappingEngine *ptMappEngine,
                                                      ECAL_Operation eOperation,
                                                      TCAL_ResourceList *ptResourceList)
{
    McpU32                      uIndex;

    MCP_FUNC_START ("_CCM_VAC_MappingEngine_GetOptionalResourcesList");

    MCP_LOG_INFO (("_CCM_VAC_MappingEngine_GetOptionalResourcesList: requesting optional resources list for "
                   "operation %s", _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify object pointer and list pointer */
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptMappEngine), 
                                ("_CCM_VAC_MappingEngine_GetOptionalResourcesList: NULL object!"));
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptResourceList), 
                                ("_CCM_VAC_MappingEngine_GetOptionalResourcesList: NULL resource list!"));

    /* nullify the output resource list */
    ptResourceList->uNumOfResources = 0;

    /* loop over all optional resources */
    for (uIndex = 0;
         uIndex < ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.uNumOfResourceLists;
         uIndex++)
    {
        /* if the optional resource is currently in use */
        if (MCP_TRUE == ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].bIsUsed)
        {
            /* add it to the list */
            ptResourceList->eResources[ ptResourceList->uNumOfResources ] = 
                ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].eOptionalResource;
            ptResourceList->uNumOfResources++;
        }
    }

    MCP_FUNC_END ();
}

void _CCM_VAC_MappingEngine_SetOptionalResourcesList (TCCM_VAC_MappingEngine *ptMappEngine,
                                                      ECAL_Operation eOperation,
                                                      TCAL_ResourceList *ptResourceList)
{
    McpU32                      uIndex;

    MCP_FUNC_START ("_CCM_VAC_MappingEngine_SetOptionalResourcesList");

    MCP_LOG_INFO (("_CCM_VAC_MappingEngine_SetOptionalResourcesList: requesting to change optional resources list for "
                   "operation %s", _CCM_VAC_DebugOperationStr(eOperation)));

    /* verify object pointer and list pointer */
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptMappEngine), 
                                ("_CCM_VAC_MappingEngine_SetOptionalResourcesList: NULL object!"));
    MCP_VERIFY_FATAL_NO_RETVAR ((NULL != ptResourceList), 
                                ("_CCM_VAC_MappingEngine_SetOptionalResourcesList: NULL resource list!"));

    /* loop over all optional resources */
    for (uIndex = 0;
         uIndex < ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.uNumOfResourceLists;
         uIndex++)
    {
        /* if this optional resource is include in the new list */
        if (MCP_TRUE ==
            _CCM_VAC_MEIsResourceOnList (ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].eOptionalResource,
            ptResourceList))
        {
            /* mark the optional resource as used */
            ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].bIsUsed =
                MCP_TRUE;
            MCP_LOG_INFO (("_CCM_VAC_MappingEngine_SetOptionalResourcesList: optional resource %s is "
                           "set to used for operation %s",
                           _CCM_VAC_DebugResourceStr(ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].eOptionalResource),
                           _CCM_VAC_DebugOperationStr(eOperation)));
        }
        else
        {
            /* mark the optional resource as unused */
            ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].bIsUsed = 
                MCP_FALSE;
            MCP_LOG_INFO (("_CCM_VAC_MappingEngine_SetOptionalResourcesList: optional resource %s is "
                           "set to unused for operation %s",
                           _CCM_VAC_DebugResourceStr(ptMappEngine->tConfig.tOpToResMap[ eOperation ].tOptionalResources.tOptionalResourceLists[ uIndex ].eOptionalResource),
                           _CCM_VAC_DebugOperationStr(eOperation)));
        }
    }

    MCP_FUNC_END ();
}

TCAL_MappingConfiguration *_CCM_VAC_MappingEngine_GetConfigData (TCCM_VAC_MappingEngine *ptMappEngine)
{
    return &(ptMappEngine->tConfig);
}

McpBool _CCM_VAC_MEIsResourceOnList (ECAL_Resource eResource, TCAL_ResourceList *ptList)
{
    McpU32      uIndex;

    MCP_FUNC_START ("_CCM_VAC_MEIsResourceOnList");

    /* loop on all resources in the list */
    for (uIndex = 0; uIndex < ptList->uNumOfResources; uIndex++)
    {
        /* if the current resource is the requested one */
        if (eResource == ptList->eResources[ uIndex ])
        {
            /* resource is on the list */
            return MCP_TRUE;
        }
    }

    MCP_FUNC_END ();

    /* list was exhausted and resource was not found - resource is not on the list */
    return MCP_FALSE;
}


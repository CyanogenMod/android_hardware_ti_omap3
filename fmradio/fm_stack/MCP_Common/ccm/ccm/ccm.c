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


#include "mcp_hal_fs.h"
#include "mcp_hal_pm.h"
#include "mcp_hal_os.h"
#include "mcp_defs.h"
#include "ccm_hal_pwr_up_dwn.h"
#include "ccm.h"
#include "ccm_imi.h"
#include "ccm_vaci.h"
#include "ccm_vaci_chip_abstration.h"
#include "mcp_config_parser.h"
#include "mcp_unicode.h"
#include "mcp_hal_log.h"

MCP_HAL_LOG_SET_MODULE(MCP_HAL_LOG_MODULE_TYPE_CCM);

struct tagCcmObj {
    McpUint         refCount;
    McpHalChipId        chipId;
    
    CcmImObj        *imObj;
    TCCM_VAC_Object *vacObj;
    Cal_Config_ID   *calObj;

McpConfigParser 			tConfigParser;			/* configuration file storage and parser */
};

typedef struct tagCcmStaticData {
    CcmObj  _ccm_Objs[MCP_HAL_MAX_NUM_OF_CHIPS];
} CcmStaticData;


/*
    A single instance that holds the static ("class") CCM data 
*/
MCP_STATIC  CcmStaticData _CCM_StaticData;

MCP_STATIC  CcmStatus _CCM_StaticInit(void);

void _CCM_NotifyChipOn(void *handle,
                       McpU16 projectType,
                       McpU16 versionMajor,
                       McpU16 versionMinor);

CcmStatus CCM_StaticInit(void)
{
    MCP_FUNC_START("CCM_StaticInit");

    MCP_FUNC_END();
    
    return CCM_STATUS_SUCCESS;
}

/*
    The function needs to do the following:
    - Perform CCM "class" static initialization (if necessary - first time)
    - "Create" the instance (again, if it's the first creation of this instance)
*/
CcmStatus CCM_Create(McpHalChipId chipId, CcmObj **thisObj)
{
    MCP_FUNC_START("CCM_Create");
    
    MCP_FUNC_END();

    return CCM_STATUS_SUCCESS;
}

/*
    
*/
CcmStatus CCM_Destroy(CcmObj **thisObj)
{   
    MCP_FUNC_START("CCM_Destroy");
   
    MCP_FUNC_END();

    return CCM_STATUS_SUCCESS;
}

CcmImObj *CCM_GetIm(CcmObj *thisObj)
{
    return 0xdeadbeef;
}

TCCM_VAC_Object *CCM_GetVac(CcmObj *thisObj)
{
    return 0xdeadbeef;
}

Cal_Config_ID *CCM_GetCAL(CcmObj *thisObj)
{
    return 0xdeadbeef;
}

CcmStatus _CCM_StaticInit(void)
{
    MCP_FUNC_START("_CCM_StaticInit");
 
    MCP_FUNC_END();

    return CCM_STATUS_SUCCESS;
}

void _CCM_NotifyChipOn(void *handle,
                       McpU16 projectType,
                       McpU16 versionMajor,
                       McpU16 versionMinor)
{
}

BtHciIfObj *CCM_GetBtHciIfObj(CcmObj *thisObj)
{
    return 0xdeadbeef;
}


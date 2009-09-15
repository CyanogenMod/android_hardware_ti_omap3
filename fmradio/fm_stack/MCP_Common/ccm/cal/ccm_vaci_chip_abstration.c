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
*   AUTHOR:        Malovany Ram
*
\*******************************************************************************/



/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "ccm_vaci_chip_abstration.h"
#include "mcp_defs.h"

 /********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/
void CCM_CAL_StaticInit(void)
{
}

void CAL_Create(McpHalChipId chipId, BtHciIfObj *hciIfObj, Cal_Config_ID **ppConfigid)
{    
    MCP_UNUSED_PARAMETER (chipId);
    MCP_UNUSED_PARAMETER (hciIfObj);

    *ppConfigid = (Cal_Config_ID *)1;
}

void CAL_Destroy(Cal_Config_ID **ppConfigid)
{       
    *ppConfigid = (Cal_Config_ID *)0;
}


void CAL_VAC_Set_Chip_Version(Cal_Config_ID *pConfigid,
                              McpU16 projectType,
                              McpU16 versionMajor,
                              McpU16 versionMinor)
{
    MCP_UNUSED_PARAMETER (pConfigid);
    MCP_UNUSED_PARAMETER (projectType);
    MCP_UNUSED_PARAMETER (versionMajor);
    MCP_UNUSED_PARAMETER (versionMinor);
}

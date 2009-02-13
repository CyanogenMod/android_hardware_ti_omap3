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
*   FILE NAME:      ccm_hal_pwr_up_dwn.c
*
*   DESCRIPTION:    Implementation of reset and shutdown sequences of CCM chips
*
*   AUTHOR:         Udi Ron
*
\*******************************************************************************/
#include "ccm_hal_pwr_up_dwn.h"

/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include <stdio.h>
#include "mcp_defs.h"
#include "ccm_hal_pwr_up_dwn.h"
#include "fmc_os.h"
#include "fmc_defs.h"
#include "fm_trace.h"

/********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/
McpBool _ccmHalPwrUpDwn_NshutdownControlAvailable = MCP_FALSE;

static McpUint _ccmHalPwrUpDwn_PinRefCount = 0;

static void _CcmHalPwrUpDwn_SetPin();
static void _CcmHalPwrUpDwn_ClearPin();

CcmHalPwrUpDwnStatus CCM_HAL_PWR_UP_DWN_Init()
{
	return CCM_HAL_PWR_UP_DWN_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * CCM_HAL_PWR_UP_DWN_Reset()
 *
 *		Resets BT Host Controller chip.
 */
CcmHalPwrUpDwnStatus CCM_HAL_PWR_UP_DWN_Reset(McpHalChipId	chipId, McpHalCoreId coreId)
{
	MCP_UNUSED_PARAMETER(chipId);
	MCP_UNUSED_PARAMETER(coreId);
	
	if (_ccmHalPwrUpDwn_NshutdownControlAvailable == MCP_TRUE)
	{
		/* Actually reset only when the first core requests reset */
		if (_ccmHalPwrUpDwn_PinRefCount == 0)
		{
			_CcmHalPwrUpDwn_ClearPin();
			
			/* Delaying to let reset value stay long enough on the line */
			FMC_OS_Sleep(10);

			_CcmHalPwrUpDwn_SetPin();
		}
	}

	++_ccmHalPwrUpDwn_PinRefCount;

	return CCM_HAL_PWR_UP_DWN_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * CCM_HAL_PWR_UP_DWN_Shutdown()
 *
 *		Shutdowns BT Host Controller chip.
 */
CcmHalPwrUpDwnStatus CCM_HAL_PWR_UP_DWN_Shutdown(McpHalChipId	chipId, McpHalCoreId coreId)
{	
	MCP_UNUSED_PARAMETER(chipId);
	MCP_UNUSED_PARAMETER(coreId);

	--_ccmHalPwrUpDwn_PinRefCount;

	if (_ccmHalPwrUpDwn_NshutdownControlAvailable == MCP_TRUE)
	{
		/* Actually Shut Down only when the last core requests shutdown */
		if (_ccmHalPwrUpDwn_PinRefCount == 0)
		{
			_CcmHalPwrUpDwn_ClearPin();
		}
	}
	
	return CCM_HAL_PWR_UP_DWN_STATUS_SUCCESS;
}

short  _CcmHalPwrUpDwn_Inp32 (short portaddr)
{
	FMC_UNUSED_PARAMETER(portaddr);
	/* No NShutdown support in FM Linux Implementation (not relevant - see design) */
	FM_ASSERT(0);

	return 0;
}

void  _CcmHalPwrUpDwn_Out32 (short portaddr, short datum)
{
	FMC_UNUSED_PARAMETER(portaddr);
	FMC_UNUSED_PARAMETER(datum);

	/* No NShutdown support in FM Linux Implementation (not relevant - see design) */
	FM_ASSERT(0);
} 

void _CcmHalPwrUpDwn_SetPin()
{
	/* No NShutdown support in FM Linux Implementation (not relevant - see design) */
	FM_ASSERT(0);
}

void _CcmHalPwrUpDwn_ClearPin()
{
	/* No NShutdown support in FM Linux Implementation (not relevant - see design) */
	FM_ASSERT(0);
}


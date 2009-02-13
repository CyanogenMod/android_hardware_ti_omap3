/******************************************************************************\
##                                                                            *
## Unpublished Proprietary and Confidential Information of Texas Instruments  *
## Israel Ltd. Do Not Disclose.                                               *
## Copyright 2008 Texas Instruments Israel Ltd.                               *
## All rights reserved. All unpublished rights reserved.                      *
##                                                                            *
## No part of this work may be used or reproduced in any form or by any       *
## means, or stored in a database or retrieval system, without prior written  *
## permission of Texas Instruments Israel Ltd. or its parent company Texas    *
## Instruments Incorporated.                                                  *
## Use of this work is subject to a license from Texas Instruments Israel     *
## Ltd. or its parent company Texas Instruments Incorporated.                 *
##                                                                            *
## This work contains Texas Instruments Israel Ltd. confidential and          *
## proprietary information which is protected by copyright, trade secret,     *
## trademark and other intellectual property rights.                          *
##                                                                            *
## The United States, Israel  and other countries maintain controls on the    *
## export and/or import of cryptographic items and technology. Unless prior   *
## authorization is obtained from the U.S. Department of Commerce and the     *
## Israeli Government, you shall not export, reexport, or release, directly   *
## or indirectly, any technology, software, or software source code received  *
## from Texas Instruments Incorporated (TI) or Texas Instruments Israel,      *
## or export, directly or indirectly, any direct product of such technology,  *
## software, or software source code to any destination or country to which   *
## the export, reexport or release of the technology, software, software      *
## source code, or direct product is prohibited by the EAR. The subject items *
## are classified as encryption items under Part 740.17 of the Commerce       *
## Control List (“CCL”). The assurances provided for herein are furnished in  *
## compliance with the specific encryption controls set forth in Part 740.17  *
## of the EAR -Encryption Commodities and Software (ENC).                     *
##                                                                            *
## NOTE: THE TRANSFER OF THE TECHNICAL INFORMATION IS BEING MADE UNDER AN     *
## EXPORT LICENSE ISSUED BY THE ISRAELI GOVERNMENT AND THE APPLICABLE EXPORT  *
## LICENSE DOES NOT ALLOW THE TECHNICAL INFORMATION TO BE USED FOR THE        *
## MODIFICATION OF THE BT ENCRYPTION OR THE DEVELOPMENT OF ANY NEW ENCRYPTION.*
## UNDER THE ISRAELI GOVERNMENT'S EXPORT LICENSE, THE INFORMATION CAN BE USED *
## FOR THE INTERNAL DESIGN AND MANUFACTURE OF TI PRODUCTS THAT WILL CONTAIN   *
## THE BT IC.                                                                 *
##                                                                            *
\******************************************************************************/
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
#include <conio.h>
#include <windows.h>
#include "mcp_defs.h"
#include "ccm_hal_pwr_up_dwn.h"

#define _CCM_HAL_PWR_UP_DWN_LPT1_ADDRESS 					(0x378)
#define _CCM_HAL_PWR_UP_DWN_LPT_DE_ASSERTION_VALUE		(0x0)
#define _CCM_HAL_PWR_UP_DWN_LPT_DEFAULT_ASSERTION_VALUE 	(0xFF)

#define _CCM_HAL_PWR_UP_DWN_LPT_ACCESS_DLL_NAME			("LptAcc.DLL")

#define _NSHUTDOWN_LPT_PIN_DONT_CARE	0

/********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/
typedef short (_stdcall *_CcmHalPwrUpDwn_InpFuncPtr)(short portaddr);
typedef void (_stdcall *_CcmHalPwrUpDwn_OupFuncPtr)(short portaddr, short datum);

_CcmHalPwrUpDwn_InpFuncPtr _ccmHalPwrUpDwn_Inp32Fp;
_CcmHalPwrUpDwn_OupFuncPtr _ccmHalPwrUpDwn_Oup32Fp;

McpBool _ccmHalPwrUpDwn_NshutdownControlAvailable = MCP_FALSE;

static McpUint _ccmHalPwrUpDwn_PinRefCount = 0;

static void _CcmHalPwrUpDwn_SetPin();
static void _CcmHalPwrUpDwn_ClearPin();

CcmHalPwrUpDwnStatus CCM_HAL_PWR_UP_DWN_Init()
{
     HINSTANCE hLib;

	_ccmHalPwrUpDwn_NshutdownControlAvailable = MCP_FALSE;
	
     /* Load the library */
     hLib = LoadLibrary(_CCM_HAL_PWR_UP_DWN_LPT_ACCESS_DLL_NAME);

	if (hLib != NULL)
	{
	 	/* get the address of the Input function */
		_ccmHalPwrUpDwn_Inp32Fp = (_CcmHalPwrUpDwn_InpFuncPtr) GetProcAddress(hLib, "Inp32");
		
		if (_ccmHalPwrUpDwn_Inp32Fp != NULL)
		{
		 	/* get the address of the Output function */
			_ccmHalPwrUpDwn_Oup32Fp = (_CcmHalPwrUpDwn_OupFuncPtr) GetProcAddress(hLib, "Out32");

			if (_ccmHalPwrUpDwn_Oup32Fp != NULL)
			{
				_ccmHalPwrUpDwn_NshutdownControlAvailable = TRUE;
			}
		}
	}

	_ccmHalPwrUpDwn_PinRefCount = 0;
	
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
			Sleep(10);

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
          return (_ccmHalPwrUpDwn_Inp32Fp)(portaddr);
}

void  _CcmHalPwrUpDwn_Out32 (short portaddr, short datum)
{
	(_ccmHalPwrUpDwn_Oup32Fp)(portaddr,datum);
} 

void _CcmHalPwrUpDwn_SetPin()
{
	long	bitMaskValue = 0;
	McpUint	lpt1PinNum = _NSHUTDOWN_LPT_PIN_DONT_CARE;
	short 	lpt1CurrentValue = 0;
	long 	lpt1OutputValue = 0;

	if (lpt1PinNum != _NSHUTDOWN_LPT_PIN_DONT_CARE)
	{
		lpt1CurrentValue = _CcmHalPwrUpDwn_Inp32(_CCM_HAL_PWR_UP_DWN_LPT1_ADDRESS);

		bitMaskValue = (1 << (lpt1PinNum - 1));
		
		/* Clear pin's bit and write */
		lpt1OutputValue = lpt1CurrentValue | bitMaskValue;
	}
	else
	{
		lpt1OutputValue = 0xFFFFUL;
	}

	_CcmHalPwrUpDwn_Out32(_CCM_HAL_PWR_UP_DWN_LPT1_ADDRESS, (short)lpt1OutputValue);	
}

void _CcmHalPwrUpDwn_ClearPin()
{
	short	bitMaskValue = 0;
	McpUint	lpt1PinNum = _NSHUTDOWN_LPT_PIN_DONT_CARE;
	short 	lpt1CurrentValue = 0;
	short 	lpt1OutputValue = _CCM_HAL_PWR_UP_DWN_LPT_DE_ASSERTION_VALUE;

	if (lpt1PinNum != _NSHUTDOWN_LPT_PIN_DONT_CARE)
	{
		lpt1CurrentValue = _CcmHalPwrUpDwn_Inp32(_CCM_HAL_PWR_UP_DWN_LPT1_ADDRESS);

		bitMaskValue = (short)(~(1 << (lpt1PinNum - 1)));
		
		/* Clear pin's bit and write */
		lpt1OutputValue = (short)(lpt1CurrentValue & bitMaskValue);
	}
	else
	{
		lpt1OutputValue = 0x0;
	}

	_CcmHalPwrUpDwn_Out32(_CCM_HAL_PWR_UP_DWN_LPT1_ADDRESS, lpt1OutputValue);	
}


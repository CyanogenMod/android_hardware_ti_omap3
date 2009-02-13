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
*   FILE NAME:      chip_pm.c
*
*   DESCRIPTION:    This file contains the implementation of the chip power 
*					management.
*
*   AUTHOR:         Malovany Ram
*
\*******************************************************************************/


#include "mcp_hal_pm.h"
#include "mcp_hal_log.h"


/********************************************************************************
 *
 * Internal function prototypes
 *
 *******************************************************************************/

/* Utilities */
static const char *Tran_Id_AsStr(McpHalTranId transport);
static const char *Chip_Id_AsStr(McpHalChipId chip);
/********************************************************************************
 *
 * Types
 *
 *******************************************************************************/


#define MCP_HAL_PM_UNUSED_PARAMETER(_PARM)     ((_PARM) = (_PARM))

#define MCP_HAL_PM_LOG_DEBUG(msg)		MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, "MCP_HAL_PM ", msg)


/********************************************************************************
 *
 * Functions
 *
 *******************************************************************************/
/*---------------------------------------------------------------------------
 *            CCM_HAL_PM_Init()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Init chip pm module.
 *
 *  Returns:
 *		      MCP_HAL_FS_STATUS_SUCCESS - Operation is successful.
 *
 */
McpHalPmStatus MCP_HAL_PM_Init(void)
{
	
	MCP_HAL_PM_LOG_DEBUG(("MCP_HAL_PM_Init is called."));
	return MCP_HAL_PM_STATUS_SUCCESS;
}
/*---------------------------------------------------------------------------
 *            HAL_PM_Deinit()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Deinit chip pm module.
 * 
 * Returns:
 *		      MCP_HAL_FS_STATUS_SUCCESS - Operation is successful.
 *
 */
McpHalPmStatus MCP_HAL_PM_Deinit(void)
{
	return MCP_HAL_PM_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
 *            CCM_HAL_PM_RegisterTransport()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Register client on specific transport on specific chip.
 *
 */
McpHalPmStatus MCP_HAL_PM_RegisterTransport(McpHalChipId chip_id,McpHalTranId tran_id,McpHalPmHandler* handler)
{
		
	MCP_HAL_PM_UNUSED_PARAMETER(handler);
	MCP_HAL_PM_UNUSED_PARAMETER(chip_id);
	MCP_HAL_PM_UNUSED_PARAMETER(tran_id);
	MCP_HAL_PM_LOG_DEBUG(("%s on %s is register succesfully.",Tran_Id_AsStr(tran_id),Chip_Id_AsStr(chip_id)));
	return MCP_HAL_PM_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
 *            CCM_HAL_PM_Deregister_Transport()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Deregister client on specific transport on specific chip.
 *
 */
McpHalPmStatus MCP_HAL_PM_DeregisterTransport(McpHalChipId chip_id,McpHalTranId tran_id)
{
	MCP_HAL_PM_UNUSED_PARAMETER(chip_id);
	MCP_HAL_PM_UNUSED_PARAMETER(tran_id);
	MCP_HAL_PM_LOG_DEBUG(("%s on %s deregister succesfully",Tran_Id_AsStr(tran_id),Chip_Id_AsStr(chip_id)));
	return MCP_HAL_PM_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
 *            CCM_HAL_PM_Transport_Wakeup()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Synopsis:  Chip power manager wakes up.
 *
 */

McpHalPmStatus MCP_HAL_PM_TransportWakeup(McpHalChipId chip_id,McpHalTranId tran_id)
{			
	MCP_HAL_PM_UNUSED_PARAMETER(chip_id);
	MCP_HAL_PM_UNUSED_PARAMETER(tran_id);
	MCP_HAL_PM_LOG_DEBUG(("%s on %s sleep state is changed to awake.",Tran_Id_AsStr(tran_id),Chip_Id_AsStr(chip_id)));
	return MCP_HAL_PM_STATUS_SUCCESS;
}



/*---------------------------------------------------------------------------
 *            CCM_HAL_PM_Transport_Sleep()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Chip power manager go to sleep.
 *
 */
McpHalPmStatus MCP_HAL_PM_TransportSleep(McpHalChipId chip_id,McpHalTranId tran_id)
{
	MCP_HAL_PM_UNUSED_PARAMETER(chip_id);
	MCP_HAL_PM_UNUSED_PARAMETER(tran_id);
	MCP_HAL_PM_LOG_DEBUG(("%s on %s sleep state is changed to sleep.",Tran_Id_AsStr(tran_id),Chip_Id_AsStr(chip_id)));
	return MCP_HAL_PM_STATUS_SUCCESS;	
}



 const char *Tran_Id_AsStr(McpHalTranId transport)
{
	switch (transport)
	{
		case MCP_HAL_TRAN_ID_0:         	return "MCP_HAL_TRAN_ID_0";
		case MCP_HAL_TRAN_ID_1:			return "MCP_HAL_TRAN_ID_1";
		case MCP_HAL_TRAN_ID_2:			return "MCP_HAL_TRAN_ID_2";		
		default:							return "Unknown";
	};
};

const char *Chip_Id_AsStr(McpHalChipId chip)
{
	switch (chip)
	{
		case MCP_HAL_CHIP_ID_0:                    return "MCP_HAL_CHIP_ID_0";
		case MCP_HAL_CHIP_ID_1:			     return "MCP_HAL_CHIP_ID_1";
		default:					   		     return "Unknown";
	};
};



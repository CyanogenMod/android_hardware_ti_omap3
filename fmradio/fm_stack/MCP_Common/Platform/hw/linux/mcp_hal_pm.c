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



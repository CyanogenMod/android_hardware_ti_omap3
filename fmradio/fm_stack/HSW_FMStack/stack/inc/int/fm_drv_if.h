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

/******************************************************************************\
*
*   FILE NAME:      fm_drv_if.h
*
*   BRIEF:          Definitions and prototypes related to the implementation of
*                   the platform independent part of the Shared Transport
*                   (in Kernel for Linux).
*
*   DESCRIPTION:    
*
*                   
*   AUTHOR:         Malovany Ram
*
\******************************************************************************/

#ifndef __FM_DRV_IF_H
#define __FM_DRV_IF_H



/*******************************************************************************
 *
 * Include files
 *
 ******************************************************************************/
#include "mcpf_defs.h"


/*******************************************************************************
 *
 * Types
 *
 ******************************************************************************/

/*CHAN8 Header len */
#define CHAN8_COMMAND_HDR_LEN			5
#define CHAN8_EVENT_HDR_LEN			6


typedef struct
{
    EMcpfRes 	eResult;	
	McpU8		uEvtParamLen;
	McpU8		*pEvtParams;
} TCmdComplEvent;


typedef void (*FM_DRV_InterputCb)(void);
typedef void (*FM_DRV_CmdComplCb)(handle_t hTrans, TCmdComplEvent  *pCmdCom );


typedef struct
{    
	FM_DRV_InterputCb		fInteruptCalllback;
	FM_DRV_CmdComplCb		fCmdCompCallback;
} FMDrvIfCallBacks;



/*******************************************************************************
 *
 * Function declarations
 *
 ******************************************************************************/
 
/*------------------------------------------------------------------------------
 * FMDrvIf_TransportOn()
 *
 * Brief:  
 *     Set the FM STK driver transoprt to On.
 *
 * Description:
 *
 * Set the FM STK driver transoprt On.
 *
 * Type:
 *		 Synchronous
 *
 * Parameters:
 *     hMcpf [in] - Handle to OS Framework
 *     InterruptCb [in] - Callback functions for Interupts
 *     CmdCompleteCb [in] - Callback functions for command complete
 *
 * Returns:
 *     RES_OK - Operation ended successfully.
 *     RES_ERROR - Operation ended with failure.
 */
EMcpfRes FMDrvIf_TransportOn(handle_t  hMcpf ,const FM_DRV_InterputCb InterruptCb, FM_DRV_CmdComplCb CmdCompleteCb);


/*------------------------------------------------------------------------------
 * FMDrvIf_TransportOff()
 *
 * Brief:
 *     Set the FM STK driver transoprt to Off.
 *
 * Description:
 *     Set the FM STK driver transoprt to Off.
 *
 * Type:
 *		 Synchronous
 *
 * Returns:
 *     RES_OK - Operation ended successfully.
 *     RES_ERROR - Operation ended with failure.
 */
EMcpfRes FMDrvIf_TransportOff(void);


/*------------------------------------------------------------------------------
 * FmDrvIf_Write()
 *
 * Brief:
 *     Write FM command via channel 8 via FM STK driver.
 *
 * Description:
 *     Set the FM STK transoprt to Off.
 *
 * Type:
 *		 Synchronous
 *
 * Parameters:
 *     uHciOpcode [in] - VS_HCI Fm opcode command.
 *     pHciCmdParms [in] - Pointer to the command parms data.
 *     uHciCmdParmsLen [in] - Len of the command data.
 *     hCbParam [in] - User data for the callback.
 *
 * Returns:
 *     RES_OK - Operation ended successfully.
 *     RES_ERROR - Operation ended with failure.
 */


EMcpfRes FmDrvIf_Write(const McpU16 			uHciOpcode,
				const McpU8 *			pHciCmdParms,
				const McpU16			uHciCmdParmsLen,		
				const handle_t 		hCbParam);


#endif /* __FM_DRV_IF_H */

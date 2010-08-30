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



/** \file   mcp_hci_adapt.h 
 *  \brief  HCI Adapter API definition                                  
 * 
 *  HCI Adapter module provides:
 * 
 * 	- Interface between CCM, FM & BTIPS adapters and shared transport layer.
 *  - Provides clients registration for HCI message (ACL/SCO/Events) reception.
 *	- builds HCI command in Txn structure and sends to transport
 *	- Serializes outgoing HCI commands and provides command flow control based
 *      on command complete event
 *	- Invokes command completion call back function of the client
 *  - Forwards the event message to registered client
 *
 *  \see    mcp_hci_adapt.c
 */

#ifndef __MCP_HCI_ADAPT_API_H__
#define __MCP_HCI_ADAPT_API_H__


/******************************************************************************
 * Defines
 ******************************************************************************/

/* HCIA registration */
#define HCIA_ANY_OPCODE           0


/******************************************************************************
 * Macros
 ******************************************************************************/


/******************************************************************************
 * Types
 ******************************************************************************/

/* Client register method to HCIA for Rx path */
typedef enum
{
    HCIA_REG_METHOD_NORMAL = 1,  /* HCIA intercepts Rx from transport layer and
                                  * uses ClientRegister for forwarding */
	HCIA_REG_METHOD_BYPASS       /* Bypass HCIA, client call back is passed to
                                  * transport layer */

} EHcaRegisterMethod;

/* Event structure type used as parameter in command complete callback */
typedef struct
{
    EMcpfRes 	eResult;
	McpU8		uEvtOpcode;
	McpU8		uEvtParamLen;
	McpU8		*pEvtParams;

} THciaCmdComplEvent;


/* Command complete call back function prototype */
typedef void (*THCIA_CmdComplCb) (handle_t hHandle, THciaCmdComplEvent *event);


/******************************************************************************
 * Functions
 ******************************************************************************/

/** 
 * \fn     HCIA_Create 
 * \brief  Create HCIA object
 * 
 * Allocate and clear the module's object
 * 
 * \note
 * \param	uCmdMaxNum    - max number of oustandig commands in command queue
 * \param	uOutCmdMaxNum - max number of sent commands but not acknowledged yet
 * \param  	uHciTypeMin   - minimum value of range of processed HCI packet types
 * \param  	uHciTypeMax   - maximum value of range of processed HCI packet types
 * \return 	Handle of the allocated object, NULL if allocation failed 
 * \sa     	HCIA_Destroy
 */ 
handle_t HCIA_Create (const handle_t hMcpf,
					  const McpU32	 uCmdMaxNum,
					  const McpU32	 uOutCmdMaxNum,
					  const McpU8	 uHciTypeMin,
					  const McpU8	 uHciTypeMax);

/** 
 * \fn     HCIA_Destroy 
 * \brief  Destroy HCIA object
 * 
 * Destroy HCIA object and free the object memory
 * 
 * \note
 * \param	hHcia - handle to HCIA object
 * \return 	void
 * \sa     	HCIA_Create
 */ 
void HCIA_Destroy (handle_t	hHcia);


/** 
 * \fn     HCIA_RegisterClient 
 * \brief  Register the client to HCIA
 * 
 * Register the client for reception of specified HCI message by type (ACL/SCO/Events) and opcode.
 * 
 * \note
 * \param	hHcia - handle to HCIA object
 * \param	uHciMsgType  - HCI Message Type to register for HCI message receive (ACL/SCO/Event)
 * \param	uHciOpcode   - HCI command/event opcode to register to receive or HCIA_ANY_OPCODE
 * \param	fCallBack    - user call back function to forward the received HCI packet
 * \param	hCbParam     - user parameter to path to call back function
 * \param	eRegisterMethod  - register method (normal or bypass)
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HCIA_UnregisterClient
 */ 
EMcpfRes HCIA_RegisterClient (const handle_t		hHcia, 
							  const McpU8 			uHciMsgType,
							  const McpU16 			uHciOpcode,
							  const TI_TransRxIndCb fCallBack,
							  const handle_t 		hCbParam,
							  const EHcaRegisterMethod eRegisterMethod);


/** 
 * \fn     HCIA_UnregisterClient 
 * \brief  Unregister the client from HCIA
 * 
 * Remove the client from reception of specified HCI message by type (ACL/SCO/Events) and opcode.
 * 
 * \note
 * \param	hHcia - handle to HCIA object
 * \param	uHciMsgType  - HCI Message Type to register for HCI message receive (ACL/SCO/Event)
 * \param	uHciOpcode   - HCI command/event opcode to register to receive or HCIA_ANY_OPCODE
 * \param	eRegisterMethod  - register method (normal or bypass)
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HCIA_RegisterClient
 */ 
EMcpfRes HCIA_UnregisterClient (const handle_t			hHcia, 
							    const McpU8 			uHciMsgType,
							    const McpU16 			uHciOpcode,
							    const EHcaRegisterMethod eRegisterMethod);
   

/** 
 * \fn     HCIA_SendCommand 
 * \brief  Send HCI command
 * 
 * Build command including HCI command header and parameters and add to HCI command output queue
 * 
 * \note
 * \param	hHcia - handle to HCIA object
 * \param	uHciOpcode   - HCI command/event opcode to register to receive or HCIA_ANY_OPCODE
 * \param	pHciCmdParms - pointer to command parameters
 * \param	pHciCmdParmsLen - parameters size in bytes
 * \param	fCallBack    - user call back function to call on command completion
 * \param	hCbParam     - user parameter to pass to call back function
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HCIA_SendData
 */ 
EMcpfRes HCIA_SendCommand (const handle_t			hHcia, 
						   const McpU16 			uHciOpcode,
						   const McpU8 *			pHciCmdParms,
						   const McpU16				uHciCmdParmsLen,
						   const THCIA_CmdComplCb 	fCallBack,
						   const handle_t 			hCbParam);

/** 
 * \fn     HCIA_SendData 
 * \brief  Send HCI packet
 * 
 * The function sends HCI ACL and SCO packet to the transport layer while HCI command
 * adds to the HCIA command queue to send according to command flow control
 * 
 * \note
 * \param	hHcia - handle to HCIA object
 * \param	pTxn - pointer to transaction sturcture containing HCI packet
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HCIA_SendCommand
 */ 
EMcpfRes HCIA_SendData (const handle_t hHcia, TTxnStruct *pTxn);


/** 
 * \fn     HCIA_CmdQueueFlush 
 * \brief  Clear HCIA command queue
 * 
 * The function clears HCI command queue and resets the flow control to initial state.
 * 
 * \note
 * \param	hHcia - handle to HCIA object
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HCIA_SendCommand
 */ 
EMcpfRes HCIA_CmdQueueFlush (const handle_t	hHcia); 

/** 
 * \fn     HCIA_CmdQueueRemove 
 * \brief  remove item from HCIA command queue
 * 
 * The function removes item from HCI command queue. May be used
 * in specific cases when no response is expected for the
 * command
 * 
 * \note
 * \param	hHcia - handle to HCIA object
 * \param   uEvtOpcode  command opcode
 * \param   uEvtParamLen parameters length
 * \param   pEvtParams pointer to command parameters
 * \return 	Returns the status of operation: OK or Error
 * \sa HCIA_SendCommand
 */ 
EMcpfRes HCIA_CmdQueueRemove (const handle_t pHcia,
							  const McpU8 uEvtOpcode,
							  const McpU8 uEvtParamLen,
							  McpU8 *pEvtParams);
  
#endif /*__MCP_HCI_ADAPT_API_H__*/

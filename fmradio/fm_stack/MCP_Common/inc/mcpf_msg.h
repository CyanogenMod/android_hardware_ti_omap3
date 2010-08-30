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



/** \file   mcp_msg.h 
 *  \brief  MCPF message structure and API specification
 *
 *  \see    mcpf_msg.c
 */

#ifndef __MCPF_MSG_H
#define __MCPF_MSG_H

#ifdef __cplusplus
extern "C" {
#endif 

#include "mcpf_defs.h"

/** 
 * \fn     mcpf_SendMsg
 * \brief  Send MCPF message
 * 
 * Allocate MCPF message, populate it's fields and send to specified target queue of 
 * the destination task
 * 
 * \note
 * \param	hMcpf     - handle to OS Framework
 * \param	eDestTaskId - destination task ID
 * \param   uDestQId  - destination queue ID 
 * \param   eSrcTaskId- source task ID
 * \param   eSrcQId   - source queue ID to return response if any
 * \param   uOpcode   - message opcode
 * \param   uLen      - number of bytes in data buffer pointed by pData
 * \param   uUserDefined - user defined parameter
 * \param	pData		 - message data buffer
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_MsgqEnable, mcpf_MsgqDisable
 */ 
EMcpfRes	mcpf_SendMsg (handle_t		hMcpf,
						  EmcpTaskId	eDestTaskId,
						  McpU8			uDestQId,
						  EmcpTaskId	eSrcTaskId,
						  McpU8			uSrcQId,
						  McpU16		uOpcode,
						  McpU32		uLen,
						  McpU32		uUserDefined,
						  void 			*pData);


/** 
 * \fn     mcpf_MsgqEnable
 * \brief  Enable message queue
 * 
 * Enable the destination queue, meaning that messages in this queue 
 * will be processed by destination task after this function call
 * 
 * \note
 * \param	hMcpf     - handle to OS Framework
 * \param	eDestTaskId - destination task ID
 * \param   uDestQId  - destination queue ID 
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_MsgqDisable
 */ 
EMcpfRes	mcpf_MsgqEnable (handle_t 	 hMcpf, 
							 EmcpTaskId eDestTaskId,
							 McpU8		 uDestQId);

/** 
 * \fn     mcpf_MsgqDisable
 * \brief  Enable message queue
 * 
 * Disable the destination queue, meaning that messages in this queue 
 * will not be processed by destination task after this function call
 * 
 * \note
 * \param	hMcpf     - handle to OS Framework
 * \param	eDestTaskId - destination task ID
 * \param   uDestQId  - destination queue ID 
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_MsgqEnable
 */ 
EMcpfRes	mcpf_MsgqDisable (handle_t 	 	hMcpf, 
							  EmcpTaskId 	eDestTaskId,
							  McpU8		 	uDestQId);

/** 
 * \fn     mcpf_SetEvent
 * \brief  Set event
 * 
 * Set the bit in the Events bitmap of the destination task
 * 
 * \note
 * \param	hMcpf	- handle to OS Framework
 * \param	eDestTaskId - destination task ID
 * \param	uEvent	- event index
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_MsgqEnable
 */ 
EMcpfRes	mcpf_SetEvent (handle_t		hMcpf, 
						   EmcpTaskId	eDestTaskId,
						   McpU32		uEvent);

/** 
 * \fn     mcpf_RegisterClientCb
 * \brief  Register external send message handler function
 * 
 * Register external send message handler function and allocates external Task ID
 * 
 * \note
 * \param	hMcpf	- handle to OS Framework
 * \param	pDestTaskId - pointer to destination task ID (return value)
 * \param	fCb			- function handler
 * \param	hCb			- function callback handler
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_UnRegisterClientCb
 */ 
EMcpfRes	mcpf_RegisterClientCb (handle_t			hMcpf,
								   McpU32			*pDestTaskId,
								   tClientSendCb	fCb,
								   handle_t		 	hCb);

/** 
 * \fn     mcpf_RegisterClientCb
 * \brief  Unregister MCPF external send message handler 
 * 
 * Unregister MCPF external send message handler for specified external destination task id 
 * and free the task id
 * 
 * \note
 * \param	hMcpf	- handle to OS Framework
 * \param	uDestTaskId - destination task ID
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_RegisterClientCb
 */ 

EMcpfRes	mcpf_UnRegisterClientCb (handle_t	hMcpf,
									 McpU32		uDestTaskId);


/** 
 * \fn     mcpf_EnqueueMsg 
 * \brief  Enqueue MCPF message
 * 
 * Enqueue MCPF message to destination queue of specified target task and 
 * set task's signal object
 * 
 * \note
 * \param	hMcpf     - handle to OS Framework
 * \param	eDestTaskId - destination task ID
 * \param   uDestQId  - destination queue ID 
 * \param	pMsg 	  - pointer to MCPF message
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_SendMsg
 */
EMcpfRes mcpf_EnqueueMsg (handle_t		hMcpf,
						  EmcpTaskId	eDestTaskId,
						  McpU8			uDestQId,
								  TmcpfMsg  	*pMsg);

#ifdef __cplusplus
}
#endif 

#endif /* __MCPF_MSG_H */


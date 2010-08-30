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


#ifndef __MCPF_MAIN_H
#define __MCPF_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif 

#include <stdio.h>

#include "mcpf_defs.h"
#include "mcp_endian.h"
#include "mcpf_queue.h"


/** MCPF APIs - Definitions **/
#define		MCPF_GET_CCM_OBJ(hMcpf)				((Tmcpf *)hMcpf)->hCcmObj
#define		MCPF_GET_REPORT_HANDLE(hMcpf)		((Tmcpf *)hMcpf)->hReport

/** MCPF APIs - Initialization **/

/** 
 * \fn     mcpf_create
 * \brief  Create MCPF
 * 
 * This function is used to allocate and initiate the context of the MCP framework.
 * 
 * \note
 * \param	hPla     		- handle to Platform Adaptation
 * \param	uPortNum     	- Port Number
 * \return 	MCPF Handler or NULL on failure
 * \sa     	mcpf_create
*/
handle_t 		mcpf_create(handle_t hPla, McpU16 uPortNum);

/** 
 * \fn     mcpf_destroy
 * \brief  Destroy MCPF
 * 
 * This function is used to free all the resources allocated in the MCPF creation.
 * 
 * \note
 * \param	hMcpf     - handle to MCPF
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_destroy
 */ 
EMcpfRes 	mcpf_destroy(handle_t	hMcpf);

/** 
 * \fn     mcpf_CreateTask
 * \brief  Create an MCPF Task
 * 
 * This function opens a session with the MCPF, 
 * with the calling driver's specific requirements.
 * 
 * \note
 * \param	hMcpf - MCPF handler.
 * \param	eTaskId - a static (constant) identification of the driver.
 * \param	pTaskName - pointer to null terminated task name string
 * \param	uNum_of_queues - number of priority queues required by the driver.
 * \param	fEvntCb - Event's callback function.
 * \param	hEvtCbHandle - Event's callback handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_CreateTask
*/
EMcpfRes		mcpf_CreateTask (handle_t 		hMcpf, 
								 EmcpTaskId 	eTaskId, 
								 char 			*pTaskName, 
								 McpU8 			uNum_of_queues, 
								 mcpf_event_cb fEvntCb, 
								 handle_t 		hEvtCbHandle);

/** 
 * \fn     mcpf_DestroyTask
 * \brief  Destroy MCPF
 * 
 * This function will be used to free all the driver resources 
 * and to un-register from MCPF.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	eTask_id - a static (constant) identification of the driver.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_DestroyTask
 */ 
EMcpfRes		mcpf_DestroyTask (handle_t hMcpf, EmcpTaskId eTask_id);

/** 
 * \fn     mcpf_RegisterTaskQ
 * \brief  Destroy MCPF
 * 
 * In order for the driver to be able to receive messages, 
 * it should register its message handler callbacks to the MCPF.
 * 
 * \note
 * \param	hMcpf - MCPF handler. 
 * \param	eTaskId - Task ID.
 * \param	uQueueId - Queue Index that the callback is related to.
 * \param	fCb - the message handler callback function.
 * \param	hCb - The callback function's handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_RegisterTaskQ
*/
EMcpfRes		mcpf_RegisterTaskQ (handle_t hMcpf, EmcpTaskId eTaskId, McpU8 uQueueId, 
									mcpf_msg_handler_cb  	fCb, handle_t hCb);

/** 
 * \fn     mcpf_UnregisterTaskQ
 * \brief  Destroy MCPF
 * 
 * This function un-registers the message handler Cb for the specified queue.
 * 
 * \note
 * \param	hMcpf - MCPF handler. 
 * \param	eTaskId - Task ID.
 * \param	uQueueId - Queue Index that the callback is related to.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_UnregisterTaskQ
 */ 
EMcpfRes		mcpf_UnregisterTaskQ (handle_t hMcpf, EmcpTaskId	eTaskId, McpU8 uQueueId);

/** 
 * \fn     mcpf_SetTaskPriority
 * \brief  Set the task's priority
 * 
 * This function will set the priority of the given task.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	eTask_id - Task ID.
 * \param	sPriority - Priority to set.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_SetTaskPriority
 */ 
EMcpfRes		mcpf_SetTaskPriority (handle_t hMcpf, EmcpTaskId eTask_id, McpInt sPriority);

#ifdef __cplusplus
}
#endif 

#endif

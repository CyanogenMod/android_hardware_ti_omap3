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


#ifndef __MCPF_TIME_H
#define __MCPF_TIME_H

#ifdef __cplusplus
extern "C" {
#endif 

#include "mcpf_defs.h"
#include "mcpf_sll.h"

/** Definitions **/
typedef enum
{	
	TMR_STATE_ACTIVE,
	TMR_STATE_EXPIRED,
	TMR_STATE_DELETED
} EMcpfTimerState;


typedef void (*mcpf_timer_cb)(handle_t hCaller, McpUint uTimerId);


/** Structures **/
typedef struct
{	
	mcpf_timer_cb 	fCb;
	handle_t		hCaller;
} TMcpfTimerCb;

typedef struct
{
	TSllNodeHdr		tTimerListNode; /* Sorted linked List */
	McpU32      		uEexpiryTime;
	EmcpTaskId		eSource;
	EMcpfTimerState	eState;	
	TMcpfTimerCb		tCbData;
} TMcpfTimer;


/** APIs **/

/** 
 * \fn     mcpf_timer_start
 * \brief  Timer Start
 * 
 * This function will put the 'timer start' request in a sorted list, 
 * and will start an OS timer when necessary.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	uExpiryTime - Requested expiry time in milliseconds (duration time).
 * \param	eTaskId - Source task id.
 * \param	fCb - Timer expiration Cb.
 * \param	hCaller - Cb's handler.
 * \return 	Pointer to the timer handler.
 * \sa     	mcpf_timer_start
 */ 
handle_t  	mcpf_timer_start (handle_t hMcpf, McpU32 uExpiryTime, EmcpTaskId eTaskId, 
								mcpf_timer_cb fCb, handle_t hCaller);

/** 
 * \fn     mcpf_timer_stop
 * \brief  Timer Stop
 * 
 * This function will remove the tiemr fro mteh list
 * and will stop the OS timer when necessary.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	hTimer - Timer's handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_timer_stop
 */  
EMcpfRes	 	mcpf_timer_stop (handle_t hMcpf, handle_t hTimer);

/** 
 * \fn     mcpf_getCurrentTime_inSec
 * \brief  Return time in seconds
 * 
 * This function will return the time in seconds from 1.1.1970.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \return 	The time in seconds from 1.1.1970.
 * \sa     	mcpf_getCurrentTime_inSec
 */  
McpU32		mcpf_getCurrentTime_inSec(handle_t hMcpf);

/** 
 * \fn     mcpf_getCurrentTime_InMilliSec
 * \brief  Return time in milliseconds
 * 
 * This function will return the system time in milliseconds.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \return 	The time in milliseconds from system's start-up (system time).
 * \sa     	mcpf_getCurrentTime_InMilliSec
 */  
McpU32		mcpf_getCurrentTime_InMilliSec(handle_t hMcpf);

/** 
 * \fn     mcpf_handleTimer
 * \brief  Timer message handler
 * 
 * This function will be called upon de-queuing a message from the timer queue.
 * 
 * \note
 * \param	hMcpf - MCPF handler.
 * \param	*tMsg - The message containing the timer. 
 * \return 	None
 * \sa     	mcpf_handleTimer
 */  
void 	mcpf_handleTimer(handle_t hCaller, TmcpfMsg *tMsg);

#ifdef __cplusplus
}
#endif 

#endif


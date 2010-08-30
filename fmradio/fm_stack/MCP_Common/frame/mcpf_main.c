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


/** Include Files **/
#include "mcpf_defs.h"
#include "mcpf_main.h"
#include "mcpf_mem.h"
#include "mcpf_services.h"
#include "mcpf_report.h"
#include "pla_os.h"
#include "mcp_hal_misc.h"
#include "mcp_hal_pm.h"
#include "mcp_hal_st.h"
#include "mcp_hal_string.h"
#include "mcpf_msg.h"
#include "bmtrace.h"

/************************************************************************/
/*							Internal Function							*/
/************************************************************************/
EMcpfRes		mcpf_TaskProc (handle_t	hMcpf, EmcpTaskId	eTaskId);


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
handle_t 	mcpf_create(handle_t hPla, McpU16 uPortNum)
{
	Tmcpf			*pMcpf;

	MCPF_UNUSED_PARAMETER(uPortNum);

	/* allocate MCPF module */
	pMcpf = (Tmcpf *)os_malloc(hPla, sizeof(Tmcpf));
	
	if (!pMcpf)
	{
		MCPF_OS_REPORT (("Error allocating the MCPF Module\n"));
		return NULL;
	}
    os_memory_zero (hPla, pMcpf, sizeof(Tmcpf));

	/* Register to PLA */
	pMcpf->hPla = hPla;
	pla_registerClient(hPla, (handle_t)pMcpf);
	
	
	/* Initiate report module. */
	pMcpf->hReport = report_Create((handle_t)pMcpf);

	/* Create Critical Section Object. */
	mcpf_critSec_CreateObj((handle_t)pMcpf, "McpfCritSecObj", &pMcpf->hMcpfCritSecObj);


	/* Allocate memory pools (for Msg & Timer). */
	pMcpf->hMsgPool				= mcpf_memory_pool_create((handle_t)pMcpf, sizeof(TmcpfMsg), 
															MCPF_MSG_POOL_SIZE);
	MCPF_Assert((pMcpf->hMsgPool));
	
	pMcpf->hTimerPool			= mcpf_memory_pool_create((handle_t)pMcpf, sizeof(TMcpfTimer), 
															MCPF_TIMER_POOL_SIZE);
	MCPF_Assert(pMcpf->hTimerPool);
	
	/* Create Timer's linked list. */
	pMcpf->hTimerList = mcpf_SLL_Create((handle_t)pMcpf, 10, MCPF_FIELD_OFFSET(TMcpfTimer, tTimerListNode), 
										MCPF_FIELD_OFFSET(TMcpfTimer, uEexpiryTime), SLL_UP);


	CL_TRACE_INIT(pMcpf);
	CL_TRACE_ENABLE();

	return (handle_t)pMcpf;
}

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
EMcpfRes 	mcpf_destroy(handle_t	hMcpf)
{
	Tmcpf *pMcpf = (Tmcpf *)hMcpf;

	CL_TRACE_DISABLE();
	CL_TRACE_DEINIT();

	/* Destroy Timer's linked list. */
	mcpf_SLL_Destroy(pMcpf->hTimerList);


	/* Free memory pools (for Msg & Timer). */
	mcpf_memory_pool_destroy(hMcpf, pMcpf->hMsgPool);
	mcpf_memory_pool_destroy(hMcpf, pMcpf->hTimerPool);

	/* Destroy Critical Section Object. */
	mcpf_critSec_DestroyObj(hMcpf, &pMcpf->hMcpfCritSecObj);


	/* Unload report module. */
	report_Unload(pMcpf->hReport);


	/* Deallocate MCPF module */
	os_free(pMcpf->hPla, hMcpf);


	return RES_COMPLETE;
}

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
								 mcpf_event_cb 	fEvntCb, 
								 handle_t 		hEvtCbHandle)
{
	Tmcpf *pMcpf = (Tmcpf *)hMcpf;
	char	critSecObjName[20];
	McpU32	i;
	
	/*	If a context to the specified 'eTaskId' is already created --> Return Fail */
	if (pMcpf->tTask[eTaskId])
		return RES_ERROR;
	

	/*  Allocate task handler */
	pMcpf->tTask[eTaskId] = (TMcpfTask *) mcpf_mem_alloc(hMcpf, sizeof(TMcpfTask));
	MCPF_Assert(pMcpf->tTask[eTaskId]);


	/**	Allocate Queues according to 'uNum_of_queues' + 1 queue for timer handling **/
	pMcpf->tTask[eTaskId]->uNumOfQueues  = uNum_of_queues + 1;
	pMcpf->tTask[eTaskId]->hQueue		 = mcpf_mem_alloc(hMcpf, (McpU16)(sizeof(handle_t) * (uNum_of_queues + 1)));
	pMcpf->tTask[eTaskId]->fQueueCb		 = mcpf_mem_alloc(hMcpf, (McpU16)(sizeof(mcpf_msg_handler_cb) * (uNum_of_queues + 1)));
    pMcpf->tTask[eTaskId]->hQueCbHandler = mcpf_mem_alloc(hMcpf, (McpU16)(sizeof(handle_t) * (uNum_of_queues + 1)));
	pMcpf->tTask[eTaskId]->bQueueFlag	 = mcpf_mem_alloc(hMcpf, (McpU16)(sizeof(McpBool) * (uNum_of_queues + 1)));
	/*  Set all 'bQueueFlag' fields to be disabled.	*/
	for(i = 0; i <= uNum_of_queues; i++)
		pMcpf->tTask[eTaskId]->bQueueFlag[i] = MCP_FALSE;

	/* Create Timer's Queue */
	pMcpf->tTask[eTaskId]->hQueue[0] = mcpf_que_Create(hMcpf, QUE_MAX_LIMIT, 
														MCPF_FIELD_OFFSET(TmcpfMsg, tMsgQNode));
	pMcpf->tTask[eTaskId]->fQueueCb[0] = mcpf_handleTimer;
	pMcpf->tTask[eTaskId]->hQueCbHandler[0] = hMcpf;
	pMcpf->tTask[eTaskId]->bQueueFlag[0] = MCP_TRUE;
	

	/*  Allocate Critical section & signaling object */
	sprintf(critSecObjName, "Task_%d_critSecObj", eTaskId);
	mcpf_critSec_CreateObj(hMcpf, critSecObjName, &(pMcpf->tTask[eTaskId]->hCritSecObj));
    pMcpf->tTask[eTaskId]->hSignalObj = os_sigobj_create(pMcpf->hPla);

    /*  Set Event Handler */
	pMcpf->tTask[eTaskId]->uEvntBitmap = 0;
	pMcpf->tTask[eTaskId]->fEvntCb = fEvntCb;
    pMcpf->tTask[eTaskId]->hEvtCbHandler = hEvtCbHandle;

	/* Set Destroy Flag to FALSE */
	pMcpf->tTask[eTaskId]->bDestroyTask = MCP_FALSE;

	/* Set task's Send Cb function */
	pMcpf->tClientsTable[eTaskId].fCb = (tClientSendCb)mcpf_EnqueueMsg;
	pMcpf->tClientsTable[eTaskId].hCb = hMcpf;

	if (MCP_HAL_STRING_StrLen(pTaskName) < MCPF_TASK_NAME_MAX_LEN)
	{
		MCP_HAL_STRING_StrCpy (pMcpf->tTask[eTaskId]->cName, pTaskName);
	}
	else
	{
		/* task name string is too long, truncate it */
		mcpf_mem_copy (hMcpf, pMcpf->tTask[eTaskId]->cName, (McpU8 *) pTaskName, MCPF_TASK_NAME_MAX_LEN - 1);
		pMcpf->tTask[eTaskId]->cName[MCPF_TASK_NAME_MAX_LEN - 1] = 0; /* line termination */
	}

	/* Create Thread */
	pMcpf->tTask[eTaskId]->hOsTaskHandle = os_createThread(NULL, 0, mcpf_TaskProc, &eTaskId, 0, NULL);

	return RES_COMPLETE;
}

/** 
 * \fn     mcpf_DestroyTask
 * \brief  Destroy MCPF
 * 
 * This function will be used to free all the driver resources 
 * and to Unregister from MCPF.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	eTask_id - a static (constant) identification of the driver.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_DestroyTask
 */ 
EMcpfRes		mcpf_DestroyTask (handle_t hMcpf, EmcpTaskId eTask_id)
{
	Tmcpf *pMcpf = (Tmcpf *)hMcpf;

	/* Set Destroy Flag to TRUE and set signal */
	pMcpf->tTask[eTask_id]->bDestroyTask = MCP_TRUE;
	os_sigobj_set(pMcpf->hPla, pMcpf->tTask[eTask_id]->hSignalObj);

	return RES_COMPLETE;
}

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
									mcpf_msg_handler_cb  	fCb, handle_t hCb)
{
	Tmcpf	*pMcpf = (Tmcpf *)hMcpf;
	
	/* Check if 'uQueueId' is valid */
	if( (uQueueId == 0) || (uQueueId >= pMcpf->tTask[eTaskId]->uNumOfQueues) )
	{
		MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, ("mcpf_RegisterTaskQ: Queue ID is invalid!"));
		return RES_ERROR;
	}

	/* Register Queue */
	pMcpf->tTask[eTaskId]->hQueue[uQueueId] = mcpf_que_Create(hMcpf, QUE_MAX_LIMIT, 
																MCPF_FIELD_OFFSET(TmcpfMsg, tMsgQNode));
	pMcpf->tTask[eTaskId]->fQueueCb[uQueueId] = fCb;
	pMcpf->tTask[eTaskId]->hQueCbHandler[uQueueId] = hCb;

	return RES_COMPLETE;
}

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
EMcpfRes		mcpf_UnregisterTaskQ (handle_t hMcpf, EmcpTaskId	eTaskId, McpU8 uQueueId)
{
	Tmcpf	*pMcpf = (Tmcpf *)hMcpf;

	/* Check if 'uQueueId' is valid */
	if( (uQueueId == 0) || (uQueueId >= pMcpf->tTask[eTaskId]->uNumOfQueues) )
	{
		MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, ("mcpf_UnregisterTaskQ: Queue ID is invalid!"));
		return RES_ERROR;
	}

	/* Unregister Queue */
	mcpf_que_Destroy(pMcpf->tTask[eTaskId]->hQueue[uQueueId]);

	return RES_COMPLETE;
}

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
EMcpfRes		mcpf_SetTaskPriority (handle_t hMcpf, EmcpTaskId eTask_id, McpInt sPriority)
{
	Tmcpf		*pMcpf = (Tmcpf *)hMcpf;
	TMcpfTask   	*pTask = pMcpf->tTask[eTask_id];
	
	return os_SetTaskPriority(pTask->hOsTaskHandle, sPriority);
}

/** Internal Functions **/

/** 
 * \fn     mcpf_TaskProc 
 * \brief  Task main loop
 * 
 * Wait on task signaling object and having received it process event bitmask and 
 * task queues
 * 
 * \note
 * \param	hMcpf     - handle to OS Framework
 * \param	eTaskId   - task ID
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_SendMsg, mcpf_SetEvent
 */
EMcpfRes	mcpf_TaskProc (handle_t	hMcpf, EmcpTaskId	eTaskId)
{
	Tmcpf		*pMcpf = (Tmcpf	*) hMcpf;
	TMcpfTask   *pTask = pMcpf->tTask[ eTaskId ];
	McpU32		uEvent = 0;
	EMcpfRes  	res;
	McpU32		uQindx;
	TmcpfMsg	*pMsg;

	CL_TRACE_TASK_DEF();

	MCP_HAL_LOG_SetThreadName(pMcpf->tTask[eTaskId]->cName);

	while (pTask->bDestroyTask != MCP_TRUE)
	{
        res = os_sigobj_wait (pMcpf->hPla, pTask->hSignalObj);

		CL_TRACE_TASK_START();

		if (res == RES_OK)
		{
#ifdef DEBUG
			MCPF_REPORT_INFORMATION(hMcpf, NAVC_MODULE_LOG, ("mcpf_TaskProc: Task #%d was invoked", eTaskId));
#endif
			
			
			if(pTask->bDestroyTask == MCP_TRUE)
			{
				MCPF_REPORT_INFORMATION(hMcpf, NAVC_MODULE_LOG, ("mcpf_TaskProc: Task #%d about to destroy...", eTaskId));
				break;
			}

			/* Check event bitmap and invoke task event handler if event bitmap is set */
			mcpf_critSec_Enter (hMcpf, pTask->hCritSecObj, MCPF_INFINIT);
			if (pTask->uEvntBitmap && pTask->fEvntCb)
			{
				uEvent = pTask->uEvntBitmap;
				pTask->uEvntBitmap = 0;
			}
			mcpf_critSec_Exit (hMcpf, pTask->hCritSecObj);

			if (uEvent)
			{
				pTask->fEvntCb (pTask->hEvtCbHandler, uEvent);
			}

			/* Check event queues and invoke task queue event handler if there is a message */
			for (uQindx = 0; uQindx < pTask->uNumOfQueues; uQindx++)
			{
				do
				{
					mcpf_critSec_Enter (hMcpf, pTask->hCritSecObj, MCPF_INFINIT);
					if (pTask->bQueueFlag[ uQindx ])
					{
						pMsg = (TmcpfMsg *) que_Dequeue (pTask->hQueue[ uQindx ]);
					}
					else
					{
						mcpf_critSec_Exit (hMcpf, pTask->hCritSecObj);
						break;
					}
					mcpf_critSec_Exit (hMcpf, pTask->hCritSecObj);

					if (pMsg)
					{
#ifdef DEBUG
						MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, 
									("DEQUEUED MSG FOR TASK #%d, FROM QUEUE #%d", eTaskId, uQindx));
#endif
						
						pTask->fQueueCb[ uQindx ] (pTask->hQueCbHandler[ uQindx ], pMsg);
					}
					else
					{
						break;
					}
				} while (pMsg);
			}
		}
		else
		{
			MCPF_REPORT_ERROR(hMcpf, NAVC_MODULE_LOG, ("mcpf_TaskProc: fail on return from 'os_sigobj_wait'"));
			return RES_ERROR;
		}

		CL_TRACE_TASK_END("mcpf_TaskProc", CL_TRACE_CONTEXT_TASK, pTask->cName, "MCPF");
	}

	/* Unregister task's Send Cb function */
	pMcpf->tClientsTable[eTaskId].fCb = NULL;
	pMcpf->tClientsTable[eTaskId].hCb = NULL;
	
	/*  Free Critical section & signaling object */
	mcpf_critSec_DestroyObj(hMcpf, &pTask->hCritSecObj);
	os_sigobj_destroy(pMcpf->hPla, pTask->hSignalObj);
						
	/* Destroy Timer's Queue */
	mcpf_que_Destroy(pTask->hQueue[0]);
					
	/* Free Queues */
	mcpf_mem_free(hMcpf, pTask->bQueueFlag);
	mcpf_mem_free(hMcpf, pTask->fQueueCb);
	mcpf_mem_free(hMcpf, pTask->hQueue);
	mcpf_mem_free(hMcpf, pTask->hQueCbHandler);			
				
	/*  Free task handler */
	mcpf_mem_free(hMcpf, pTask);
				
	/* Exit Thread */
	os_exitThread(0);
				
	return RES_OK;
}


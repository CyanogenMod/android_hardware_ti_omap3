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
#include "mcpf_time.h"
#include "pla_os.h"
#include "mcpf_report.h"
#include "mcpf_services.h"
#include "mcpf_mem.h"
#include "mcpf_msg.h"


/************************************************************************/
/*                  Internal Functions Definitions                      */
/************************************************************************/
static void 	mcpf_timer_callback(handle_t hMcpf, McpUint uTimerId);

static McpU32 mcpf_getAbsoluteTime(handle_t hMcpf, McpU32 uEexpiryTime);

static McpS32 mcpf_getExpiryInMilliSecs(handle_t hMcpf, McpU32 uEexpiryAbsoluteTime);

#ifdef DEBUG
/* Added for testing timer expiry */
static void handleTimerExpiry(Tmcpf*, TMcpfTimer *hMcpfTimer);
#endif


/************************************************************************/
/*								APIs                                    */
/************************************************************************/

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
 * \param	*fCb - Timer expiration Cb.
 * \param	hCaller - Cb's handler.
 * \return 	Pointer to the timer handler.
 * \sa     	mcpf_timer_start
 */ 
handle_t  	mcpf_timer_start (handle_t hMcpf, McpU32 uExpiryTime, EmcpTaskId eTaskId, 
							  mcpf_timer_cb fCb, handle_t hCaller)
{
	Tmcpf				*pMcpf;
    TMcpfTimer			*pMcpfTimer, *pFirstTimer;
	McpU32				uCurrentExpiryTime = 0;
	McpU32				uListCount;
	
    if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return NULL;	
	}

	pMcpf = (Tmcpf*)hMcpf;

	/* Allocate Timer */
	pMcpfTimer = (TMcpfTimer*)mcpf_mem_alloc_from_pool(pMcpf, pMcpf->hTimerPool);

	if(NULL == pMcpfTimer)
	{
		MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, ("mcpf_timer_start, No memory available in TimerPool\n"));
        return NULL;
	}	 

	MCPF_ENTER_CRIT_SEC(hMcpf);
	/* Init Timer Fields */
	pMcpfTimer->uEexpiryTime = mcpf_getAbsoluteTime(hMcpf, uExpiryTime); 
	pMcpfTimer->eSource = eTaskId;
	pMcpfTimer->tCbData.fCb =  fCb ;                                                       
	pMcpfTimer->tCbData.hCaller = hCaller;    
	pMcpfTimer->eState = TMR_STATE_ACTIVE;		

	/* if list is not empty get current expiry time(Absolute) */
	uListCount = mcpf_SLL_Size(pMcpf->hTimerList);
	if(uListCount != 0)
	{
		/* Get the first timer on the list*/
		pFirstTimer = (TMcpfTimer*) mcpf_SLL_Get(pMcpf->hTimerList);	
		uCurrentExpiryTime = pFirstTimer->uEexpiryTime;
	}

	/* Insert new Timer to the list */
	if (RES_ERROR == mcpf_SLL_Insert(pMcpf->hTimerList, pMcpfTimer))
	{
		MCPF_EXIT_CRIT_SEC(hMcpf);
		mcpf_mem_free_from_pool(hMcpf, pMcpfTimer);
		MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, ("mcpf_SLL_Insert Failed\n"));
        return NULL;
	}	        

	/* If list was empty before insertion (first timer on the list) --> Start the OS Timer */
    if (uListCount == 0)
	{
		pMcpf->uTimerId  = os_timer_start(pMcpf->hPla, mcpf_timer_callback, 
											hMcpf, uExpiryTime);
	}	
	else if ( uCurrentExpiryTime > (pMcpfTimer->uEexpiryTime))
	{
		/* The previous first timer on the list is having greater exp time 
		   than the new one. Stop this timer and start the new one.*/
      
		/* Stop the current timer */
        if (MCP_FALSE == os_timer_stop(pMcpf->hPla, pMcpf->uTimerId))
		{
			MCPF_EXIT_CRIT_SEC(hMcpf);
			mcpf_mem_free_from_pool(hMcpf, pMcpfTimer);
			MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, ("os_timer_stop Failed\n"));
		    return NULL; 		
		}

		/* Start the new timer */
		pMcpf->uTimerId  = os_timer_start(pMcpf->hPla, mcpf_timer_callback, hMcpf, uExpiryTime);
		
	} 

	MCPF_EXIT_CRIT_SEC(hMcpf);
	return ((handle_t)pMcpfTimer);
}

/** 
 * \fn     mcpf_timer_stop
 * \brief  Timer Stop
 * 
 * This function will remove the timer from the list
 * and will stop the OS timer when necessary.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	hTimer - Timer's handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_timer_stop
 */  
EMcpfRes	 	mcpf_timer_stop (handle_t hMcpf, handle_t hTimer)
{
	Tmcpf		*pMcpf;
    TMcpfTimer	*pMcpfTimer;
	McpS32		uExpiryTime;
	TMcpfTimer  	*pExpiredTimersQ[10]; 
	McpU16		i, counter = 0;
	McpBool		bIsOnlyTimer = MCP_FALSE;
	McpBool		bIsFirstTimer = MCP_FALSE;
	EMcpfRes		eRetVal = RES_OK;

    if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return RES_ERROR;
	}
        	
	pMcpf = (Tmcpf*)hMcpf;

    pMcpfTimer = (TMcpfTimer *)hTimer;

	MCPF_ENTER_CRIT_SEC(hMcpf);
	/* hTimer has already expired */
	if (pMcpfTimer->eState == TMR_STATE_EXPIRED ) 
    {
	       pMcpfTimer->eState = TMR_STATE_DELETED;
		MCPF_EXIT_CRIT_SEC(hMcpf);
		MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, ("mcpf_timer_stop: hTimer has already expired"));
		return RES_OK;
    }

	
	/* If it is the first/only timer on the list --> need to stop the OS timer */
	if(mcpf_SLL_Size(pMcpf->hTimerList) == 1)
	{
		bIsOnlyTimer = MCP_TRUE;
	}
	else if(mcpf_SLL_Get(pMcpf->hTimerList) == pMcpfTimer)
		bIsFirstTimer = MCP_TRUE;
	
	/* Remove Timer From the List */
	if ( (eRetVal = mcpf_SLL_Remove(pMcpf->hTimerList,hTimer)) == RES_OK)
	{
		/* If it is the only timer on the list --> need to stop the OS timer */
		if(bIsOnlyTimer)    
		{ 
			if (MCP_TRUE == os_timer_stop(pMcpf->hPla, pMcpf->uTimerId))
				eRetVal = RES_COMPLETE;
			else
				eRetVal = RES_ERROR;
			
			pMcpf->uTimerId = 0;
		}
		else if(bIsFirstTimer) /* It is the First Timer on the list */
		{
			if (MCP_TRUE == os_timer_stop(pMcpf->hPla, pMcpf->uTimerId))
			{
				pMcpf->uTimerId = 0;
				pMcpfTimer = mcpf_SLL_Get(pMcpf->hTimerList);
				
				do
				{		
					/* Get actual exp time in msecs from absolute exp time in the list */
					uExpiryTime = mcpf_getExpiryInMilliSecs(hMcpf,pMcpfTimer->uEexpiryTime);
					if(uExpiryTime <= 0)
					{
						pExpiredTimersQ[counter] = mcpf_SLL_Retrieve(pMcpf->hTimerList);
						counter++;
					}
					else
					{
						pMcpf->uTimerId = os_timer_start(pMcpf->hPla, mcpf_timer_callback, hMcpf, (McpU32)uExpiryTime);
						break;
					}
					pMcpfTimer = mcpf_SLL_Get(pMcpf->hTimerList);	
					
				} while (pMcpfTimer);
			}
			else 
			{
				pMcpf->uTimerId = 0;
				eRetVal = RES_ERROR; 
			}
		} 
	}
	MCPF_EXIT_CRIT_SEC(hMcpf);

	for(i = 0; i < counter; i++)
	{
		pMcpfTimer = pExpiredTimersQ[i];
		mcpf_SendMsg(hMcpf, 
					pMcpfTimer->eSource,	/* Destination task is the task that started the timer */
					0,						/* Timer's Queue Id is alway 0 */
					pMcpfTimer->eSource,	/* Source task is the task that started the timer */
					0,						/* Since in timer queue there will be only
												message about the timer expiration */
					0,						/* opcode */
					sizeof(TMcpfTimer),		/* Length of the data */
					0,						/* User Defined is NOT USED*/
					pMcpfTimer);			/* The data is the timer structure */
	}
	
	/* Free Timer */
	if (RES_ERROR == mcpf_mem_free_from_pool(pMcpf, (McpU8*) hTimer))
		return RES_ERROR;

	return eRetVal;
}

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
McpU32		mcpf_getCurrentTime_inSec(handle_t hMcpf)
{
	if (!hMcpf)
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return 0;
	}	
	return (os_get_current_time_inSec(((Tmcpf*)hMcpf)->hPla));  
}

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
McpU32		mcpf_getCurrentTime_InMilliSec(handle_t hMcpf)
{
	if (!hMcpf)
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return 0;
	}
	
	return (os_get_current_time_inMilliSec(((Tmcpf*)hMcpf)->hPla));  
}

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
void 	mcpf_handleTimer(handle_t hCaller, TmcpfMsg *tMsg)
{
	TMcpfTimer *pMcpfTimer = (TMcpfTimer*)tMsg->pData;
	mcpf_timer_cb pCallBack = *(pMcpfTimer->tCbData.fCb);
	
#ifdef DEBUG
	/* Added for testing timer expiry */
	handleTimerExpiry(hCaller, pMcpfTimer);
#endif

	if( (pMcpfTimer->eState) == TMR_STATE_EXPIRED)
    {
		pCallBack(pMcpfTimer->tCbData.hCaller, 0);
    }
	
	/* Irrespective of the state the timer structure needs to be freed */
	mcpf_mem_free_from_pool(hCaller, (McpU8*)tMsg->pData);
	mcpf_mem_free_from_pool(hCaller, (McpU8*)tMsg);
}


/************************************************************************/
/*						Internal Functions				                */
/************************************************************************/

/** 
 * \fn     mcpf_timer_callback
 * \brief  General Timer Callback
 * 
 * This function will be called upon expiry time interrupt.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	uTimerId     - The timer Id of the expired timer
 * \return 	None
 * \sa     	mcpf_timer_callback
 */  
static void 	mcpf_timer_callback(handle_t hMcpf, McpUint uTimerId)
{
	Tmcpf		*pMcpf;
    TMcpfTimer  	*pMcpfTimer;    
	McpU32		uExpiryTime;
	TMcpfTimer  	*pExpiredTimersQ[10]; 
	McpU16		i, counter = 0;

    if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return;	
	}
	pMcpf = (Tmcpf*)hMcpf;

	MCPF_ENTER_CRIT_SEC(hMcpf);

	/* Check if expired timer is already handled, and now another OS timer was alreay started */
	if(uTimerId != pMcpf->uTimerId)
	{
		MCPF_EXIT_CRIT_SEC(hMcpf);
		return;
	}
	
	do
    	{
		pMcpfTimer = (TMcpfTimer*)mcpf_SLL_Retrieve(pMcpf->hTimerList);

		/* This is added to handle race condition between 
		   this function and the 'mcpf_timer_stop()' function. 
		   This condition can only be valid in the first iteration of the loop.
		   The timer was stopped before the expiration Cb is handled, 
		   and the timer is already been removed from the timer's list. */
		if(pMcpfTimer == NULL)
		{
			MCPF_EXIT_CRIT_SEC(hMcpf);
			return;
		}
		
		/* Store a copy of the exp time of retrieved timer */
		uExpiryTime = pMcpfTimer->uEexpiryTime;

		pMcpfTimer->eState	= TMR_STATE_EXPIRED;	

		/* Save all expired timers, in order to send an 
		    experation message outside the critical section. */
		pExpiredTimersQ[counter] = pMcpfTimer;
		counter++;

		/* Get the Next Timer from the List */
		pMcpfTimer = mcpf_SLL_Get(pMcpf->hTimerList);		

    	} while( (pMcpfTimer) && (uExpiryTime == pMcpfTimer->uEexpiryTime) );
    
	if(pMcpfTimer)
	{
		/* Start the next timer request */
		uExpiryTime = mcpf_getExpiryInMilliSecs(hMcpf, pMcpfTimer->uEexpiryTime);    
		pMcpf->uTimerId = os_timer_start(pMcpf->hPla, mcpf_timer_callback, hMcpf, uExpiryTime);  
	}
	MCPF_EXIT_CRIT_SEC(hMcpf);

	for(i = 0; i < counter; i++)
	{
		pMcpfTimer = pExpiredTimersQ[i];
		mcpf_SendMsg(hMcpf, 
					pMcpfTimer->eSource,	/* Destination task is the task that started the timer */
					0,						/* Timer's Queue Id is alway 0 */
					pMcpfTimer->eSource,	/* Source task is the task that started the timer */
					0,						/* Since in timer queue there will be only
												message about the timer expiration */
					0,						/* opcode */
					sizeof(TMcpfTimer),		/* Length of the data */
					0,						/* User Defined is NOT USED*/
					pMcpfTimer);			/* The data is the timer structure */
	}
}

/**
 * \fn		mcpf_getAbsoluteTime():  
 * \brief	Converts duration(TIMER EXPIRY) in Milliseconds into Absolute time
 * 
 * This function gets the timer expiry(duration) in Milliseconds and converts it 
 * into Absolute time in Milli-Secs using os_get_current_time_inMilliSec() API
 *
 * \note   
 * \param   hMcpf			- MCPF handler. 
 * \param   uEexpiryTime	- Expiry Time in MilliSeconds (Duration of Timer)
 * \return  McpU32			- The absolute time in milliseconds.
 */ 

static McpU32 mcpf_getAbsoluteTime(handle_t hMcpf, McpU32 uEexpiryTime)
{
	if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return 0;	
	}
	
	return(mcpf_getCurrentTime_InMilliSec(hMcpf) + uEexpiryTime);         
}


/**
 * \fn		mcpf_getExpiryInMilliSecs():  
 * \brief	Converts Absolute time back into actual timer expiry duration in Milli-Secs
 * 
 * This function takes absolute time(MilliSecs) and converts it into actual duration
 * in Milli-Secs
 *
 * \note   
 * \param  hMcpf				  - MCPF handler. 
 * \param  uEexpiryAbsoluteTime	  - Absolute Expiry Time
 * \return McpU32				  – The actual duration in Milli-Secs.
 */ 

static McpS32 mcpf_getExpiryInMilliSecs(handle_t hMcpf, McpU32 uEexpiryAbsoluteTime)
{	
	if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return 0;	
	}
	
	return (uEexpiryAbsoluteTime - mcpf_getCurrentTime_InMilliSec(hMcpf));
}

#ifdef DEBUG
/** 
 * \fn     handleTimerExpiry 
 * \brief  handles store keeping activity after timer expiry
 * 
 * This function can be removed, is added only for Testing purpose
 * 
 * 
 */
static void handleTimerExpiry(Tmcpf *hMcpf, TMcpfTimer *hMcpfTimer)
{
	Tmcpf		*pMcpf;
	TMcpfTimer	*pMcpfTimer;

	pMcpfTimer = (TMcpfTimer*)hMcpfTimer;

    if (!hMcpf) 
	{
		MCPF_OS_REPORT("Mcpf handler is NULL\n"));
		return NULL;	
	}
	pMcpf = (Tmcpf*)hMcpf;


	printf("\n\n-------------- TIMER EXPIRED ---------------  \n");
	printf("    Timer ID		= %d\n", hMcpf->uTimerId);
	printf("    Absolute Time	= %u\n", pMcpfTimer->uEexpiryTime);
		
	switch(pMcpfTimer->eState)
	{
		case 0:
			printf("    Timer State		= TMR_STATE_ACTIVE\n");			
		break;

		case 1:
			printf("    Timer State		= TMR_STATE_EXPIRED\n");			
		break;

		case 2:
			printf("    Timer State	0	= TMR_STATE_DELETED\n");						
		break;
	}
    printf("--------------------------------------------  \n\n");

	printf("\n\n\nMenu:\n");
	printf("0  - Start New Timer\n");
	printf("1  - Stop Current Timer\n");
	printf("2  - Running Timer ID\n");
	printf("3  - Display Timer List\n");
	printf("99 - Exit\n");
	printf("\nEnter: ");
}
#endif

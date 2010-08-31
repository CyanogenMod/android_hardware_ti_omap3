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

#include <stdarg.h>
#include <stdio.h>

#include <time.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h> 

#include "pla_os.h"


/************************************************************************/
/*                           Defines                                */
/************************************************************************/
#define SIGTIMER     (SIGRTMAX)
#define ONESHOTTIMER (SIGRTMAX-1)


/************************************************************************/
/*                           Definitions                                */
/************************************************************************/
/* Client's structure */
typedef struct
{
    handle_t            hRegClient;
    mcpf_TaskProcedure  pThreadProcArry[TASK_MAX_ID];
} TPlaClient;

/* Timer */
typedef struct
{
    timer_t timer_id;
    mcpf_timer_cb cb;
    handle_t hCaller;
} MCPTimer_t;


/************************************************************************/
/*                          Global Variables                            */
/************************************************************************/
TPlaClient      tPlaClient;
MCPTimer_t      timerData;

/************************************************************************/
/*                  Internal Function Declaration                      */
/************************************************************************/
/* Timer Callback */
static void *os_threadProc(void *param);
void    os_timer_cb(handle_t hwnd, McpUint u_Msg, McpUint idEvent, McpU32 u_Time);
static McpBool SetTimer(McpU32 signo, McpU32 sec, McpU32 mode, timer_t *timer);
static McpBool KillTimer(timer_t timerid);
static McpBool SetTimer(McpU32 signo, McpU32 sec, McpU32 mode, timer_t *timer);

/************************************************************************/
/*                              APIs                                    */
/************************************************************************/
/* Init */

/**
 * \fn     pla_create
 * \brief  Create Platform Abstraction Context
 *
 * This function creates the Platform Abstraction Context.
 *
 * \note
 * \return  Handler to PLA
 * \sa      pla_create
 */
handle_t    pla_create ()
{
    return NULL;
}

/**
 * \fn     pla_registerClient
 * \brief  Register a client (handler) to the PLA
 *
 * This function registers a client (handler) to the PLA.
 * Only one client is allowed.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   hClient - Client's handler.
 * \return  None
 * \sa      pla_registerClient
 */
void        pla_registerClient(handle_t hPla, handle_t hClient)
{
    McpU8 i;

    MCPF_UNUSED_PARAMETER(hPla);

    tPlaClient.hRegClient = hClient;
    for(i = 0; i < TASK_MAX_ID; i++)
        tPlaClient.pThreadProcArry [i] = NULL;

}


/* Memory */

/**
 * \fn     os_malloc
 * \brief  Memory Allocation
 *
 * This function allocates a memory according to the requested size.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   mem_size - size of buffer to allocate.
 * \return  Pointer to the allocated memory.
 * \sa      os_malloc
 */
McpU8       *os_malloc (handle_t hPla, McpU32 mem_size)
{
    MCPF_UNUSED_PARAMETER(hPla);
    return malloc(mem_size);
}

/**
 * \fn     os_free
 * \brief  Memory Free
 *
 * This function frees the received memory.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   *mem - pointer to the memory to free.
 * \return  Result of operation: OK or ERROR
 * \sa      os_free
 */
EMcpfRes    os_free (handle_t hPla, McpU8 *mem)
{
    MCPF_UNUSED_PARAMETER(hPla);
    free(mem);
    return RES_COMPLETE;
}

/**
 * \fn     os_memory_set
 * \brief  Memory Set
 *
 * This function sets the given memory with a given value.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   *pMemPtr - pointer to the memory to set.
 * \param   Value - value to set.
 * \param   Length - number of bytes to set.
 * \return  None
 * \sa      os_memory_set
 */
void        os_memory_set (handle_t hPla, void *pMemPtr, McpS32 Value, McpU32 Length)
{
    MCPF_UNUSED_PARAMETER(hPla);
    memset(pMemPtr,Value, Length);
}

/**
 * \fn     os_memory_zero
 * \brief  Set memory to zero
 *
 * This function sets the given memory to zero.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   *pMemPtr - pointer to the memory to set.
 * \param   Length - number of bytes to set.
 * \return  None
 * \sa      os_memory_zero
 */
void        os_memory_zero (handle_t hPla, void *pMemPtr, McpU32 Length)
{
    MCPF_UNUSED_PARAMETER(hPla);
    memset(pMemPtr, 0, Length);
}

/**
 * \fn     os_memory_copy
 * \brief  Copy Memory
 *
 * This function copies given memory size from src to dest.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   *pDestination - pointer to the memory to copy to.
 * \param   *pSource - pointer to the memory to copy from.
 * \param   uSize - number of bytes to cpoy.
 * \return  None
 * \sa      os_memory_copy
 */
void        os_memory_copy (handle_t hPla, void *pDestination, void *pSource, McpU32 uSize)
{
    MCPF_UNUSED_PARAMETER(hPla);
    memcpy(pDestination, pSource, uSize);
}


/* Time */

/**
 * \fn     os_timer_start
 * \brief  Starts an OS timer
 *
 * This function starts an OS timer. It should supply it's own cb function,
 * and should store the received cb & handler with the returned timer ID.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   cb - timer's callback function
 * \param   hCaller - callback's handler
 * \param   expiry_time - timer's expiry time
 * \return  Timer ID
 * \sa      os_timer_start
 */
McpUint  os_timer_start (handle_t hPla, mcpf_timer_cb cb, handle_t hCaller, McpU32 expiry_time)
{
    struct sigaction sigact;
    timer_t timer;
    McpU8 res;

    MCPF_UNUSED_PARAMETER(hPla);

    #ifdef DEBUG
    printf("Timer Started :os_timer_start exp time = %u \n", (unsigned int)  expiry_time    );
    #endif

    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO;
    sigact.sa_sigaction = (void(*) (int, siginfo_t *, void *)) os_timer_cb;

    //Set up sigaction to catch signal
    if (sigaction(SIGTIMER, &sigact, NULL) == -1)
    {
        perror("sigaction failed");
        return -1;
    }
    #ifdef DEBUG
    printf("\n From os_timer_start going to call SetTimer \n");
    #endif
    res = SetTimer(SIGTIMER, expiry_time, 0, &timer);
    if( res == MCP_TRUE)
    {
    timerData.timer_id = timer;
    timerData.cb = cb;
    timerData.hCaller = hCaller;
    }

    return timerData.timer_id;
}


/**
 * \fn     os_timer_stop
 * \brief  Stops the received timer
 *
 * This function stops the received timer
 * and clears its related stored parameters.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   timer_id - The ID of the timer to stop.
 * \return  Result of operation: OK or ERROR
 * \sa      os_timer_stop
 */
McpBool os_timer_stop (handle_t hPla, McpUint timer_id)
{
    MCPF_UNUSED_PARAMETER(hPla);

    if(KillTimer(timer_id) == MCP_FALSE)
    {
        /* Clear timer data */
        timerData.timer_id = 0;
        timerData.cb = NULL;
        timerData.hCaller = NULL;
    }
    return MCP_TRUE;
}

static McpBool KillTimer(timer_t timerid)
{
    int st;
    #ifdef DEBUG
        printf("\n Timerid : %d is going to delete \n", timerid);
    #endif
    st = timer_delete(timerid);
    if(st == 0)
        return MCP_TRUE;
    else
        return MCP_FALSE;
}

/**
 * \fn     SetTimer
 * \brief
 *
 * This function...
 *
 * \note
 * \param
 * \return
 * \sa      SetTimer
 */
static McpBool SetTimer(McpU32 signo, McpU32 sec, McpU32 mode, timer_t *timer)
{
    struct sigevent sigev;
    timer_t *timerid = timer;
    struct itimerspec itval;
    struct itimerspec oitval;

    // Create the POSIX timer to generate signo
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = signo;
    sigev.sigev_value.sival_ptr = timerid;


    if (timer_create(CLOCK_REALTIME, &sigev, timerid) == 0)
    {
        #ifdef DEBUG
            printf("\n timer id is %d",  *timerid);
        #endif

        itval.it_value.tv_sec = sec / 1000;
        itval.it_value.tv_nsec = (long)(sec % 1000) * (1000000L);

        if (mode == 1)
        {
            itval.it_interval.tv_sec = itval.it_value.tv_sec;
            itval.it_interval.tv_nsec = itval.it_value.tv_nsec;
        }
        else
        {
            itval.it_interval.tv_sec = 0;
            itval.it_interval.tv_nsec = 0;
        }

        if (timer_settime(*timerid, 0, &itval, &oitval) != 0)
            perror("time_settime error!");

    }
    else
    {
        perror("timer_create error!");
        return MCP_FALSE;
    }

        return MCP_TRUE;
}



/**
 * \fn     os_get_current_time_inSec
 * \brief  Get time in seconds
 *
 * This function returns time in seconds since 1.1.1970.
 *
 * \note
 * \param   hPla - PLA handler.
 * \return  time in seconds since 1.1.1970
 * \sa      os_get_current_time_inSec
 */
McpU32      os_get_current_time_inSec (handle_t hPla)
{
    McpU32 seconds;

    MCPF_UNUSED_PARAMETER(hPla);

    /* Time in sec since Epoch (00:00:00 UTC, 1.1.1970 */
    seconds = (McpU32)time (NULL);
    return (McpU32) seconds;
}

/**
 * \fn     os_get_current_time_inMilliSec
 * \brief  Get time in milliseconds
 *
 * This function returns system time in milliseconds.
 *
 * \note
 * \param   hPla - PLA handler.
 * \return  system time in milliseconds
 * \sa      os_get_current_time_inMilliSec
 */
McpU32      os_get_current_time_inMilliSec (handle_t hPla)
{
    time_t seconds;
    MCPF_UNUSED_PARAMETER(hPla);

    seconds = time (NULL);
    return seconds*1000;
}


/* Signaling Object */

/**
 * \fn     os_sigobj_create
 * \brief  Create signaling object
 *
 * This function creates a signaling object.
 *
 * \note
 * \param   hPla - PLA handler.
 * \return  Handler to object
 * \sa      os_sigobj_create
 */
void * os_sigobj_create (handle_t hPla)
{
    int result;
    sem_t *sem;

    MCPF_UNUSED_PARAMETER(hPla);
    sem = (sem_t *)malloc(sizeof(sem_t));

    if(sem != NULL)
    {
        result  = sem_init(sem, 0, 0);
        if(result != 0)
        {
            perror(" !!!! signal creation FAILED...");
            return NULL;
        }
        return (void *)sem;
    }

    return NULL;

}

/**
 * \fn     os_sigobj_wait
 * \brief  Wait on the signaling object
 *
 * This function waits on the signaling object.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   *evt - signaling object handler.
 * \return  Result of operation: OK or ERROR
 * \sa      os_sigobj_wait
 */
EMcpfRes    os_sigobj_wait (handle_t hPla, void *evt)
{
    sem_t* sem;
    int result;

    MCPF_UNUSED_PARAMETER(hPla);
    sem = (sem_t*)(evt);

    result = sem_wait(sem);
    if (result != 0)
    {
        return RES_ERROR;
    }
    return RES_COMPLETE;
}


/**
 * \fn     os_sigobj_set
 * \brief  Set the signaling object
 *
 * This function sets the signaling object.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   *evt - signaling object handler.
 * \return  Result of operation: OK or ERROR
 * \sa      os_sigobj_set
 */
EMcpfRes    os_sigobj_set (handle_t hPla, void *evt)
{
    sem_t* sem;
    int result;

    MCPF_UNUSED_PARAMETER(hPla);

    sem = (sem_t*)(evt);

    /* Try to release the semphore */
    result = sem_post(sem);
    if (result != 0)
    {
        return RES_ERROR;
    }

    return RES_COMPLETE;
}

/**
 * \fn     os_sigobj_destroy
 * \brief  Destroy the signaling object
 *
 * This function destroys the signaling object.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   *evt - signaling object handler.
 * \return  Result of operation: OK or ERROR
 * \sa      os_sigobj_destroy
 */
EMcpfRes    os_sigobj_destroy (handle_t hPla, void *evt)
{
    sem_t* sem;
    int result;

    MCPF_UNUSED_PARAMETER(hPla);

    sem = (sem_t*)(evt);
    result = sem_destroy(sem);
    if (result != 0)
        return RES_ERROR;

    return RES_COMPLETE;
}



/* Messaging */

/**
 * \fn     os_send_message
 * \brief  Send a message to another process
 *
 * This function sends a given message to the destination process.
 *
 * \note
 * \param   hPla - PLA handler.
 * \param   dest - Index of destination process/application.
 * \param   *msg - The message to send.
 * \return  Result of operation: OK or ERROR
 * \sa      os_send_message
 */
EMcpfRes    os_send_message (handle_t hPla, McpU32 dest, TmcpfMsg *msg)
{
    MCPF_UNUSED_PARAMETER(hPla);
    MCPF_UNUSED_PARAMETER(dest);
    MCPF_UNUSED_PARAMETER(msg);

    return RES_COMPLETE;
}

/**
 * \fn     os_createThread
 * \brief  Create a Thread
 *
 * This function should save the thread procedure that
 * it received (lpStartAddress) in the array, according
 * to the task id that is received (lpParameter).
 * Than it should create the thread, with a local function as the thread's procedure
 * and the received parameter (lpParameter) as the param.
 *
 * \note
 * \param   *lpThreadAttributes - Thread's attributes.
 * \param   uStackSize - Stack Size.
 * \param   *lpStartAddress - Pointer tot he thread's procedure.
 * \param   *lpParameter - Parameter of the thred's procedure.
 * \param   uCreationFlags - Thread's creation flags.
 * \param   *puThreadId - pointer to the created thread id.
 * \return  Thread's Handler
 * \sa      os_createThread
 */

// EMcpfRes os_createThread (pthread_t *lpThreadAttributes, McpU32 uStackSize, void *lpStartAddress,
//                           void *lpParameter, McpU32 uCreationFlags, McpU32 *puThreadId)


handle_t	os_createThread (void *lpThreadAttributes, McpU32 uStackSize, mcpf_TaskProcedure lpStartAddress, 
							 void *lpParameter, McpU32 uCreationFlags, McpU32 *puThreadId)
{
    EmcpTaskId *pTaskId = (EmcpTaskId *)(lpParameter);
    EmcpTaskId eTaskId = *pTaskId;
    McpU32 ret;
    pthread_t threadId;

    MCPF_UNUSED_PARAMETER(lpThreadAttributes);
    MCPF_UNUSED_PARAMETER(uStackSize);
    MCPF_UNUSED_PARAMETER(uCreationFlags);
    MCPF_UNUSED_PARAMETER(puThreadId);

    tPlaClient.pThreadProcArry[(McpU32)eTaskId] = (mcpf_TaskProcedure)lpStartAddress;
    ret = pthread_create(&threadId, NULL, os_threadProc, (void*)(eTaskId));

    if(ret != 0)
        return  NULL;
    else
        return (handle_t) threadId;
}



/**
 * \fn     os_exitThread
 * \brief  Exit a running thread
 *
 * This function exits a running thread.
 *
 * \note
 * \param   uExitCode - Exit Code.
 * \return  None
 * \sa      os_exitThread
 */

// EMcpfRes os_exitThread (pthread_t thread_id)

void os_exitThread (McpU32 thread_id)
{
    MCPF_UNUSED_PARAMETER(thread_id);
  //  pthread_exit(NULL);  // commented for L25
}

/** 
 * \fn     os_SetTaskPriority
 * \brief  Set the task's priority
 * 
 * This function will set the priority of the given task.
 * 
 * \note
 * \param	hTaskHandle     - Task's handler.
 * \param	sPriority - Priority to set.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_SetTaskPriority
 */ 
EMcpfRes		os_SetTaskPriority (handle_t hTaskHandle, McpInt sPriority)
{
	EMcpfRes eRes;

	/* TODO convert hTaskHandle to task ID */
    if (setpriority(PRIO_PROCESS, (int) hTaskHandle, sPriority) == 0)
	{
		eRes = RES_OK;
	}
	else
	{
		eRes = RES_ERROR;

	}
    return eRes;
}

/**
 * \fn     os_print
 * \brief  Print String
 *
 * This function prints the string to the default output.
 *
 * \note
 * \param   format - String's format.
 * \param   ...     - String's arguments.
 * \return  None
 * \sa      os_print
 */
void        os_print (const char *format ,...)
{
    va_list args;

    va_start (args, format);
    vprintf (format, args);
    va_end (args);
}


/************************************************************************/
/*                      Internal Functions                              */
/************************************************************************/
void    os_timer_cb(handle_t hwnd, McpUint u_Msg, McpUint idEvent, McpU32 u_Time)
{
    MCPF_UNUSED_PARAMETER(hwnd);
    MCPF_UNUSED_PARAMETER(u_Msg);
    MCPF_UNUSED_PARAMETER(idEvent);
    MCPF_UNUSED_PARAMETER(u_Time);

    if (timerData.cb)
    {
        #ifdef DEBUG
            printf("\n from os_timer_cb going to call mcpf_timer_callback");
        #endif
            timerData.cb (timerData.hCaller, timerData.timer_id);
    }
}


static void *os_threadProc(void *param)
{
    EmcpTaskId  eTaskId = (McpU32)(param);

    if (tPlaClient.pThreadProcArry[eTaskId] != NULL)
        tPlaClient.pThreadProcArry[eTaskId](tPlaClient.hRegClient, eTaskId);

    return NULL;
}



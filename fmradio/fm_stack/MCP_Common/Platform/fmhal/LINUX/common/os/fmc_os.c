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


#define _GNU_SOURCE /* needed for PTHREAD_MUTEX_RECURSIVE */

#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "fmc_os.h"
#include "mcp_hal_string.h"
#include "mcp_hal_memory.h"
#include "mcp_hal_misc.h"
#include "mcp_hal_os.h"
#include "fmc_defs.h"
#include "fmc_log.h"

FMC_LOG_SET_MODULE(FMC_LOG_MODULE_FMOS);

#if FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED


/* Stack task */
#define Fmc_OS_TASK_HANDLE_STACK								(0x00)

/* data type definitions */
typedef struct _FMC_OS_TASK_PARAMS
{
	/* Handle to the thread */
	pthread_t threadHandle;

    /* Event Mask used as condition for synchronization */
    FmcOsEvent evtMask;

    /* Mutex used for the protecting the condition 'evtMask' */
    pthread_mutex_t  evtMaskMutex;

    /* Condition variable used for the signaling when the condition 'evtMask' changed */
    pthread_cond_t evtMaskCond;

	/* handle to the task */
	FmcOsTaskHandle taskHandle;       
    
	/* Tracks whether the task is running */
	FMC_BOOL taskRunning;                

	/* Callbacks provided by the task */
	FmcOsEventCallback taskCallback;             

} FMC_OS_TASK_PARAMS;

typedef struct _FMC_OS_TIMER_PARAMS
{
	/* Timer event sent to the task */
	FmcOsEvent timerEvent;

	/* Identifier for the timer events */ 
	timer_t timerId;
	int timerUsed;

	/* associate timerHandle to taskHandle (used in FmcTimerFired()) */
	FmcOsTaskHandle taskHandle;

} FMC_OS_TIMER_PARAMS;

/* Global variables */
FMC_OS_TASK_PARAMS          fmParams;

/* FM task parameters */
FMC_OS_TIMER_PARAMS         timerParams[ FMHAL_OS_MAX_NUM_OF_TIMERS ]; /* timers storage */

/* Handles to the semaphores */
pthread_mutex_t* semaphores_ptr[FMHAL_OS_MAX_NUM_OF_SEMAPHORES];
pthread_mutex_t  semaphores[FMHAL_OS_MAX_NUM_OF_SEMAPHORES];

#define FMC_OS_LARGEST_TASK_HANDLE FMC_OS_TASK_HANDLE_FM

static void* FmStackThread(void* param);
static void TimerHandlerFunc(union sigval val);


static void initTaskParms(FMC_OS_TASK_PARAMS* params)
{
    int rc;

    params->evtMask = 0;
    params->taskHandle = 0;
    params->taskRunning = FMC_FALSE;
    params->threadHandle = 0;
    params->taskCallback = 0;

    rc = pthread_mutex_init(&params->evtMaskMutex, NULL);
    FMC_ASSERT(rc == 0);

    rc = pthread_cond_init(&params->evtMaskCond, NULL);
    FMC_ASSERT(rc == 0);
}

static void destroyTaskParms(FMC_OS_TASK_PARAMS* params)
{
    int rc;

    rc = pthread_mutex_destroy(&params->evtMaskMutex);
    FMC_ASSERT(rc == 0);

    rc = pthread_cond_destroy(&params->evtMaskCond);
    FMC_ASSERT(rc == 0);
}

/*-------------------------------------------------------------------------------
 * FMC_OS_Init()
 *
 */
FmcStatus FMC_OS_Init(void)
{
	FMC_U32     idx;
	FmcStatus ret = FMC_STATUS_SUCCESS;

	FMC_FUNC_START(("FMC_OS_Init"));

	/* init FM task parameters */
    initTaskParms(&fmParams);

	/* init timer handles array */
	for (idx = 0; idx < FMHAL_OS_MAX_NUM_OF_TIMERS; idx++)
	{
		timerParams[idx].timerUsed = 0;
		timerParams[ idx ].timerId = 0;
		timerParams[ idx ].taskHandle = 0;
		timerParams[ idx ].timerEvent = 0;
	}

	/* init semaphore handles array */
	for (idx=0; idx < FMHAL_OS_MAX_NUM_OF_SEMAPHORES; idx++)
	{
		semaphores_ptr[idx] = NULL;
	}

	goto out;

out:
	FMC_FUNC_END();
	return ret;    
}


/*-------------------------------------------------------------------------------
 * FMC_OS_Deinit()
 *
 */
FmcStatus FMC_OS_Deinit(void)
{
	FMC_FUNC_START(("FMC_OS_Deinit"));

	FMC_LOG_INFO(("FMHAL_OS_Deinit executing\n"));
	
    destroyTaskParms(&fmParams);
    
	FMC_FUNC_END();
	return FMC_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * FMC_OS_CreateTask()
 *
 */
FmcStatus FMC_OS_CreateTask(FmcOsTaskHandle taskHandle, 
                            FmcOsEventCallback callback, 
                            const char *taskName)
{
	pthread_t threadHandle = 0;	/* thread Id */
	int rc=-1;
	FmcStatus ret;
	FMC_FUNC_START(("FMC_OS_CreateTask"));

	FMC_UNUSED_PARAMETER(taskName);

	FMC_ASSERT (taskHandle == FMC_OS_TASK_HANDLE_FM);

	/* verify the desired task is not already running */
	if (fmParams.taskRunning == FMC_TRUE) 
	{
		ret = FMC_STATUS_FAILED;
		goto out;
	}    
    
	/* 
	* set task callback and handle - must be done before task is started to have this 
	* info ready for it
	*/
	fmParams.taskHandle = taskHandle;
	fmParams.taskCallback = callback;

	/* mark that the desired task is now running */
	fmParams.taskRunning = FMC_TRUE;

	/* and finally create the task */
	rc = pthread_create(&threadHandle, /* thread structure */
				NULL, /* no attributes for now */
				FmStackThread, /* main function */
				NULL);
	if(0 != rc)
	{
		FMC_LOG_INFO(("FMHAL_OS_CreateTask | pthread_create() failed to create 1st FM thread: %s", strerror(errno)));
		goto err;
	}

	FMC_LOG_INFO(("Created 1st FM thread, id = %u", threadHandle));

	fmParams.threadHandle = threadHandle;

	ret = FMC_STATUS_SUCCESS;
	goto out;

err:
	/* failed to create the task */
	fmParams.taskRunning = FMC_FALSE;
	ret = FMC_STATUS_FAILED;
out:
	FMC_FUNC_END();
	return ret;
}


/*-------------------------------------------------------------------------------
 * FMC_OS_DestroyTask()
 *
 */
FmcStatus FMC_OS_DestroyTask(FmcOsTaskHandle taskHandle)
{
	FMC_FUNC_START(("FMC_OS_DestroyTask"));

	FMC_LOG_INFO(("FMHAL_OS_DestroyTask: killing task %d", taskHandle));
    
	FMC_ASSERT (taskHandle == FMC_OS_TASK_HANDLE_FM);

	fmParams.taskRunning = FMC_FALSE;

	FMC_FUNC_END();
	return FMC_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * FMC_OS_CreateSemaphore()
 *
 */
FmcStatus FMC_OS_CreateSemaphore(const char *semaphoreName, 
                                 FmcOsSemaphoreHandle *semaphoreHandle)
{
	int rc;
	FMC_U32 idx;

	pthread_mutexattr_t mutex_attr;
	FMC_FUNC_START(("FMC_OS_CreateSemaphore"));
	
	FMC_UNUSED_PARAMETER(semaphoreName);
		
	/* we use recursive mutex - a mutex owner can relock the mutex. to release it,
	     the mutex shall be unlocked as many times as it was locked
	  */
	for (idx=0; idx < FMHAL_OS_MAX_NUM_OF_SEMAPHORES; idx++)
	{
		if (semaphores_ptr[idx] == NULL)
		{
			break;
		}
	}
	
        if (idx == FMHAL_OS_MAX_NUM_OF_SEMAPHORES)
	{
		/* exceeds maximum of available handles */
		return FMC_STATUS_FAILED;
	}

	semaphores_ptr[idx] = &semaphores[idx];	

	rc = pthread_mutexattr_init(&mutex_attr);
	if(rc != 0)
	{
		FMC_LOG_INFO(("FMC_OS_CreateSemaphore | pthread_mutexattr_init() failed: %s", strerror(rc)));
		return FMC_STATUS_FAILED;
	}
	
	rc = pthread_mutexattr_settype( &mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	if(rc != 0)
	{
		FMC_LOG_INFO(("FMC_OS_CreateSemaphore | pthread_mutexattr_settype() failed: %s", strerror(rc)));
		pthread_mutexattr_destroy(&mutex_attr);
		return FMC_STATUS_FAILED;
	}
		
	rc = pthread_mutex_init(&semaphores[idx], &mutex_attr);
	if(rc != 0)
	{
		FMC_LOG_INFO(("FMC_OS_CreateSemaphore | pthread_mutex_init() failed: %s", strerror(rc)));
		pthread_mutexattr_destroy(&mutex_attr);
		return FMC_STATUS_FAILED;
	}
	
	rc = pthread_mutexattr_destroy(&mutex_attr);
	if(rc != 0)
	{
		FMC_LOG_INFO(("FMC_OS_CreateSemaphore | pthread_mutexattr_destroy() failed: %s", strerror(rc)));
		pthread_mutex_destroy(&semaphores[idx]);
		return FMC_STATUS_FAILED;
	}
	*semaphoreHandle = (FmcOsSemaphoreHandle)idx;
			
	FMC_FUNC_END();	
	return FMC_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * FMC_OS_DestroySemaphore()
 *
 */
FmcStatus FMC_OS_DestroySemaphore(FmcOsSemaphoreHandle semaphoreHandle)
{
	int rc;
	FMC_FUNC_START(("FMC_OS_DestroySemaphore"));

	FMC_LOG_INFO(("FMC_OS_DestroySemaphore: killing semaphore"));
	
	FMC_ASSERT(semaphoreHandle < FMHAL_OS_MAX_NUM_OF_SEMAPHORES);

	if (semaphores_ptr[semaphoreHandle] == NULL)
	{
		/* mutex does not exists */
		return FMC_STATUS_FAILED;
	}

	rc = pthread_mutex_destroy(semaphores_ptr[semaphoreHandle]);
	semaphores_ptr[semaphoreHandle] = NULL;

	if (rc != 0)
	{
		FMC_LOG_INFO(("FMC_OS_DestroySemaphore | pthread_mutex_destroy() failed: %s", strerror(rc)));
		return FMC_STATUS_FAILED;
	}

	FMC_FUNC_END();
	return FMC_STATUS_SUCCESS;
}


FmcStatus FMC_OS_LockSemaphore(FmcOsSemaphoreHandle semaphore)
{
	int rc;
	pthread_mutex_t * mutex;
	
	FMC_ASSERT(semaphore < FMHAL_OS_MAX_NUM_OF_SEMAPHORES);
	mutex = semaphores_ptr[semaphore];
	
	if (mutex == NULL)
	{
		/* mutex does not exists */
		FMC_LOG_ERROR(("FMHAL_OS_LockSemaphore: failed: mutex is NULL!"));
		return FMC_STATUS_FAILED;
	}


	rc = pthread_mutex_lock(mutex);
	if (rc != 0)
	{
		FMC_LOG_INFO(("FMC_OS_LockSemaphore: pthread_mutex_lock() failed: %s", strerror(rc)));
		return FMC_STATUS_FAILED;
	}

	return FMC_STATUS_SUCCESS;
}

FmcStatus FMC_OS_UnlockSemaphore(FmcOsSemaphoreHandle semaphore)
{
	int rc;
	pthread_mutex_t* mutex; 

	FMC_ASSERT(semaphore < FMHAL_OS_MAX_NUM_OF_SEMAPHORES);
	mutex = semaphores_ptr[semaphore];

	if (mutex == NULL)
	{
		FMC_LOG_ERROR(("FMHAL_OS_UnLockSemaphore: failed: mutex is NULL!"));
		/* mutex does not exist */
		return FMC_STATUS_FAILED;
	}
	

	rc = pthread_mutex_unlock(mutex);
	if(rc != 0)
	{
		FMC_LOG_INFO(("FMC_OS_UnlockSemaphore: pthread_mutex_unlock() failed: %s", strerror(rc)));
		return FMC_STATUS_FAILED;
	}

	return FMC_STATUS_SUCCESS;

}


/*-------------------------------------------------------------------------------
 * FMC_OS_CreateTimer()
 * note: timer event is created in FMC_OS_Init(). 
 */
FmcStatus FMC_OS_CreateTimer(FmcOsTaskHandle taskHandle, 
                             const char *timerName, 
                             FmcOsTimerHandle *timerHandle)
{
    FMC_U32 idx;
	struct sigevent evp;
	int rc;
	
    FMC_UNUSED_PARAMETER(timerName);

	bzero((void*)&evp, sizeof(evp));

    for (idx = 0; idx < FMHAL_OS_MAX_NUM_OF_TIMERS; idx++)
    {
        if (timerParams[ idx ].timerUsed == 0)
        {
            break;
        }
    }
    
    if (idx == FMHAL_OS_MAX_NUM_OF_TIMERS)
    {
        /* exceeds maximum of available timers */
        return FMC_STATUS_FAILED;
    }

	*timerHandle = (FmcOsTimerHandle)idx;

	/* mark as used (when timer started, will hold timerId) */
	timerParams[idx].timerUsed = 1;
	timerParams[idx].timerId = 0; /* nonexistent timer id */
	
	/* associate timerHandle to taskHandle (used in TimerHandler()) */
	timerParams[idx].taskHandle = taskHandle;
 	timerParams[idx].timerEvent = 7; /* nonexistent event */

    /* Create the timer */
    evp.sigev_value.sival_int = *timerHandle; /* pass the handle as argument */
    evp.sigev_notify_function = TimerHandlerFunc;
    evp.sigev_notify_attributes = NULL;
    evp.sigev_notify = SIGEV_THREAD;

    rc = timer_create(CLOCK_REALTIME, &evp, &timerParams[idx].timerId);
    
    /* FMC_LOG_DEBUG(("Create Timer: %s, handle=%d, timerId=%d\n", timerName, idx, timerParams[idx].timerId)); */   
	if (rc != 0)
	{
		timerParams[idx].timerUsed = 0;
		FMC_LOG_INFO(("FMC_OS_CreateTimer: timer_create() failed: %s", strerror(errno)));
		return FMC_STATUS_FAILED;
	}

    return FMC_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * FMC_OS_DestroyTimer()
 *
 */
FmcStatus FMC_OS_DestroyTimer (FmcOsTimerHandle timerHandle)
{
	int rc;

	/* FMC_LOG_DEBUG(("FMCOS | FMC_OS_DestroyTimer | timerHandle = %d\n", timerHandle)); */
	FMC_ASSERT(timerHandle < FMHAL_OS_MAX_NUM_OF_TIMERS);
	FMC_ASSERT(timerParams[timerHandle].timerUsed == 1);
		
	rc = timer_delete(timerParams[timerHandle].timerId);
	/* FMC_LOG_DEBUG(("FMCOS | FMC_OS_DestroyTimer | rc = %d\n", rc)); */
	if (rc == 0)
	{
		timerParams[timerHandle].timerId = 0;
		timerParams[timerHandle].timerUsed = 0;
		return FMC_STATUS_SUCCESS;
	}

	FMC_LOG_INFO(("FMC_OS_DestroyTimer | timer_delete() failed: %s", strerror(errno)));
    return FMC_STATUS_FAILED;
}


/*-------------------------------------------------------------------------------
 * FMC_OS_ResetTimer()
 *
 */
FmcStatus FMC_OS_ResetTimer(FmcOsTimerHandle timerHandle, 
                            FmcOsTime time, 
                            FmcOsEvent evt)
{
	timer_t timerId;
	struct itimerspec ts;
	long rc;
	
	/* FMC_LOG_DEBUG(("Reset Timer: %d, t=%d, eidx=%d\n", timerHandle, time, eventIdx)); */
	
	FMC_ASSERT(timerHandle < FMHAL_OS_MAX_NUM_OF_TIMERS);
	FMC_ASSERT(timerParams[timerHandle].timerUsed == 1);
	if (timerParams[timerHandle].timerId == 0){
		return FMC_STATUS_FAILED;
	}

	timerId = timerParams[timerHandle].timerId;
	/* FMC_LOG_DEBUG(("FMC_OS_ResetTimer | timerHandle = %d , timerId = %d\n", timerHandle, timerId)); */
	
	timerParams[timerHandle].timerEvent = evt;

	bzero(&ts, sizeof(struct itimerspec));
	ts.it_value.tv_sec = time ? time / 1000 : 0;
	ts.it_value.tv_nsec = time ? (time % 1000) * 1000 * 1000 : 0;  /* from mili to nano */
	ts.it_interval.tv_sec = 0; /* non-periodic */
	ts.it_interval.tv_nsec = 0;	
	rc = timer_settime (timerId, 0, &ts, NULL);

	if (rc == 0)
	{
		return FMC_STATUS_SUCCESS;
	}
	else
	{
		FMC_LOG_INFO(("FMC_OS_ResetTimer: timer_settime() failed: %s", strerror(errno)));
		return FMC_STATUS_FAILED;
	}
}


/*-------------------------------------------------------------------------------
 * FMC_OS_CancelTimer()
 *
 */
FmcStatus FMC_OS_CancelTimer(FmcOsTimerHandle timerHandle)
{
	/* disarm timer using 0 as expiration value */
	return FMC_OS_ResetTimer(timerHandle, 0, 0xFF);
}

/*---------------------------------------------------------------------------
 * Called by the POSIX timer to indicate that the timer fired.
 * value is the index of the timer in the timers array
 */

static void TimerHandlerFunc(union sigval val)
{
	int idx = val.sival_int;

	FMC_ASSERT(idx < FMHAL_OS_MAX_NUM_OF_TIMERS);

    FMC_OS_SendEvent(timerParams[idx].taskHandle, timerParams[idx].timerEvent);

}

/*-------------------------------------------------------------------------------
 * FMC_OS_SendEvent()
 *
 */
FmcStatus FMC_OS_SendEvent(FmcOsTaskHandle taskHandle, FmcOsEvent evt)
{
    int rc;
    FMC_BOOL evtMaskChanged = FMC_FALSE;

	/* sanity check */
	FMC_ASSERT(taskHandle <= FMC_OS_LARGEST_TASK_HANDLE);

    rc = pthread_mutex_lock(&fmParams.evtMaskMutex);
    FMC_ASSERT(rc == 0);

    if ((fmParams.evtMask & evt) == 0)
    {
        fmParams.evtMask |= evt;
        evtMaskChanged = FMC_TRUE;
    }

    rc = pthread_mutex_unlock(&fmParams.evtMaskMutex);
    FMC_ASSERT(rc == 0);

    if (evtMaskChanged == FMC_TRUE)
    {
        rc = pthread_cond_signal(&fmParams.evtMaskCond);
        FMC_ASSERT(rc == 0);
    }
    
    return FMC_STATUS_SUCCESS;
}

void FMC_OS_Assert(const char*expression, const char*file, FMC_U16 line)
{
    MCP_HAL_MISC_Assert(expression, file, line);
}

void FMC_OS_MemCopy(void *dest, const void *source, FMC_U32 numBytes)
{
    MCP_HAL_MEMORY_MemCopy(dest, source, numBytes);
}

FMC_BOOL FMC_OS_MemCmp(const void *buffer1, FMC_U16 len1, const void *buffer2, FMC_U16 len2)
{
    return MCP_HAL_MEMORY_MemCmp(buffer1, len1,buffer2, len2);
}

void FMC_OS_MemSet(void *dest, FMC_U8 byte, FMC_U32 len)
{
    MCP_HAL_MEMORY_MemSet(dest, byte, len);
}

FMC_U8 FMC_OS_StrCmp(const FMC_U8*Str1, const FMC_U8*Str2)
{
    return MCP_HAL_STRING_StrCmp((const char *)Str1, (const char *)Str2);
}

FMC_U8 FMC_OS_StriCmp(const FMC_U8*Str1, const FMC_U8*Str2)
{
    return MCP_HAL_STRING_StriCmp((const char *)Str1, (const char *)Str2);
}

FMC_U16 FMC_OS_StrLen(const FMC_U8 *Str)
{
    return (FMC_U16)MCP_HAL_STRING_StrLen((const char *)Str);
}

FMC_U8* FMC_OS_StrCpy(FMC_U8* StrDest, const FMC_U8*StrSource)
{
    return (FMC_U8*)MCP_HAL_STRING_StrCpy((char*)StrDest, (const char *)StrSource);
 }

FMC_U8* FMC_OS_StrnCpy(FMC_U8*  StrDest, const FMC_U8*StrSource, FMC_U32 Count)
{
    return (FMC_U8*)MCP_HAL_STRING_StrnCpy((char *)StrDest, (const char *)StrSource, Count);
}

FMC_U8 FMC_OS_Sleep(FMC_U32 time)
{
    return (FMC_U8)MCP_HAL_OS_Sleep(time);
}

/*
FM Main thread.
 listens for events, and calls the callback
*/
static void * FmStackThread(void* param)
{
	int rc;
	FmcOsEventCallback callback;
    FmcOsEvent evtMask = 0;
	FMC_UNUSED_PARAMETER(param);
    
	FMC_FUNC_START(("FmStackThread"));
   
	/* Identify ! */
	MCP_HAL_LOG_SetThreadName("FMSTK");

	/* set self priority to 0.
	   The UART threads are set to -1, stack threads are set to 0, and the
	   application thread will be set to 1 */
	if (-1 == setpriority(PRIO_PROCESS, 0, 0))
	{
		FMC_LOG_INFO(("FMHAL GenericThread: failed to set priority: %s", strerror(errno)));
		FMC_ASSERT(0);
	}

	callback = fmParams.taskCallback;

	FMC_ASSERT(callback != NULL);

	/* Stay in this thread while still connected */
	while (fmParams.taskRunning) {

        rc = pthread_mutex_lock(&fmParams.evtMaskMutex);
        FMC_ASSERT(rc == 0);

		/* First check if we already received any signal */
        if (fmParams.evtMask == 0) {
        	/* No signal yet => block until we get a signal */
        	rc = pthread_cond_wait(&fmParams.evtMaskCond, &fmParams.evtMaskMutex);
			FMC_ASSERT(rc == 0);
        }

        /* Got a signal, copy evtMask to local variable, and clear evtMask */
        evtMask = fmParams.evtMask;
        fmParams.evtMask = 0;

        rc = pthread_mutex_unlock(&fmParams.evtMaskMutex);
        FMC_ASSERT(rc == 0);

        FMC_ASSERT(callback != NULL);
        callback(evtMask);
	}

	FMC_LOG_INFO(("FMHALOS | GenericThread | thread id %u is now terminating", pthread_self()));
	FMC_FUNC_END();
	return 0;
}

#endif  /* FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED */


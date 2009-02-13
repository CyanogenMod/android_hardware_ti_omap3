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

#define _GNU_SOURCE /* needed for PTHREAD_MUTEX_RECURSIVE */

#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/un.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "fm_trace.h"
#include "fmc_os.h"
#include "mcp_hal_string.h"
#include "mcp_hal_memory.h"
#include "mcp_hal_misc.h"
#include "mcp_hal_os.h"
#include "fmc_types.h"
#include "fmc_defs.h"


#if FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED

/* macros and constants */
#define FMC_OS_ASSERT(condition)                       \
        (((condition) != 0) ? (void)0 : FMC_OS_Assert(#condition, __FILE__, (FMC_U16)__LINE__))

/* data type definitions */
typedef struct _FMC_OS_TASK_PARAMS
{
	/* Handle to the thread */
	pthread_t threadHandle;

	/* Unix domain socket for the listener */
	int sock;

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
	FMC_U32 timerEvent;

	/* Identifier for the timer events */ 
	timer_t timerId;
	int timerUsed;

	/* associate timerHandle to taskHandle (used in FmcTimerFired()) */
	FmcOsTaskHandle taskHandle;

} FMC_OS_TIMER_PARAMS;

/* prototypes */
static FmcOsEvent FmcOs_ConvertToFmcEvent (FMC_U32 evtIdx);
static FMC_U32 FmcOs_ConvertToEventIdx (FmcOsEvent evt);

/* Global variables */
FMC_OS_TASK_PARAMS          fmParams;
FMC_OS_TASK_PARAMS          fmParams2;
/* FM task parameters */
FMC_OS_TIMER_PARAMS         timerParams[ FMHAL_OS_MAX_NUM_OF_TIMERS ]; /* timers storage */
/* Handles to the semaphores */
pthread_mutex_t* semaphores_ptr[FMHAL_OS_MAX_NUM_OF_SEMAPHORES];
pthread_mutex_t  semaphores[FMHAL_OS_MAX_NUM_OF_SEMAPHORES];

/* this key will hold a thread-specific unix domain sockets vector that will be used
 * for sending messages to other threads. the goal is to refrain from creating and connecting
 * ad-hoc sockets for every message sending to reduce runtime overhead */
static pthread_key_t thread_socket_vector;

#define FMC_OS_LARGEST_TASK_HANDLE FMC_OS_TASK_HANDLE_FM
/* will be initialized to hold all unix domain paths to server sockets */
static char *server_paths[FMC_OS_LARGEST_TASK_HANDLE+1];

static void* FmStackThread(void* param);
static void *fm_wait_for_interrupt_thread(void *dev);
inline static int CreateConnectedSocket(FmcOsTaskHandle taskHandle);
static void TimerHandlerFunc(union sigval val);//int value);

// returns a listener socket on unix domain (e.g. /tmp/stack)
// upon failure returns 0
static int Create_Unix_Domain_Socket(char *path)
{
	FM_BEGIN();
	
	#define offsetof(TYPE, MEMBER) ((int)&((TYPE *)0)->MEMBER)

	struct sockaddr_un servaddr; /* address of server */
	int sock, size;

	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
	{
		FM_TRACE("Error in socket() %s", strerror(errno));
		return -1;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strncpy(servaddr.sun_path, path, sizeof(servaddr.sun_path)-1);

	/* if such a file already exists, remove it */
	    if (unlink(servaddr.sun_path) < 0)
	{
		/* This error msg is not really interesting */
		FM_TRACE("Create_Unix_Domain_Socket | unlink(): %s", strerror(errno));
	}

	size = offsetof(struct sockaddr_un, sun_path) + strlen(servaddr.sun_path);

	if (bind(sock, (struct sockaddr *)&servaddr, size) < 0)
	{
		FM_TRACE("Create_Unix_Domain_Socket | bind() failed: %s", strerror(errno));
		close(sock);
		return -1;
	}        

	FM_END();
	return sock;
}

// Send Event over Unix Domain Socket
static FmcStatus Send_Unix_Domain(FmcOsEvent evt, int sock)
{
	int count;
	FM_BEGIN();
	
	//count = sendto(sock, (U8*)&evt, sizeof(evt), 0, (struct sockaddr *)&servaddr, sizeof(servaddr));
	count = send(sock, (FMC_U8*)&evt, sizeof(evt), 0);
	if (count != sizeof(evt))
	{
		if (count == -1)
		{
			FM_TRACE("Send_Unix_Domain | send() failed: %s", strerror(errno));
		} else {
			FM_TRACE("Send_Unix_Domain | send() sent only %d out of %d bytes", count, sizeof(evt));
		}
		
		return FMC_STATUS_FAILED;
	}

	FM_END();
	return FMC_STATUS_SUCCESS;
}

static void ThreadSocketDestructor(void *socket_vector)
{
	int i;
	FM_BEGIN();

	pthread_t id = pthread_self();

	FM_ASSERT(NULL != socket_vector);
	
	for(i = 0; i <= FMC_OS_LARGEST_TASK_HANDLE; i++)
	{
		if(((int *)socket_vector)[i] >= 0)
		{
			FM_TRACE("ThreadSocketDestructor: thread %u closing socket %d", id, i);
			
			if (-1 == close(((int *)socket_vector)[i]))
			{
				FM_TRACE("ThreadSocketDestructor: close() failed: %s\n", strerror(errno));
			}

			((int *)socket_vector)[i] = -1;
		}
	}

	free(socket_vector);
	FM_END();
}

/*-------------------------------------------------------------------------------
 * FMC_OS_Init()
 *
 */
FmcStatus FMC_OS_Init(void)
{
	FMC_U32     idx;
	int rc;
	FmcStatus ret = FMC_STATUS_SUCCESS;
#ifdef ANDROID
	server_paths[FMC_OS_TASK_HANDLE_FM] = "/data/misc/hcid/ti_fm.socket";
#else
	server_paths[FMC_OS_TASK_HANDLE_FM] = "/tmp/ti_fm.socket";
#endif
	FM_BEGIN();

	/* init FM task parameters */
	fmParams.taskHandle = 0;   
	fmParams.taskRunning = FMC_FALSE;  
	fmParams.threadHandle = 0;

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

	/* initialize thread specific unix domain socket */
	rc = pthread_key_create(&thread_socket_vector, ThreadSocketDestructor);
	if(0 != rc)
	{
		FM_TRACE("FMHAL_OS_Init | pthread_key_create() failed: %s", strerror(rc));
		ret = FMC_STATUS_FAILED;
		goto out;
	}
	
	fmParams.sock = Create_Unix_Domain_Socket(server_paths[FMC_OS_TASK_HANDLE_FM]);
	if (fmParams.sock < 0)
	{
		FM_TRACE("FMHAL_OS_Init | failed to create unix domain socket");
		ret = FMC_STATUS_FAILED;
		goto del_key;
	}

	goto out;

del_key:
	pthread_key_delete(thread_socket_vector);
out:
	FM_END();
	return ret;    
}


/*-------------------------------------------------------------------------------
 * FMC_OS_Deinit()
 *
 */
FmcStatus FMC_OS_Deinit(void)
{
	int rc;
	FM_BEGIN();

	FM_TRACE("FMHAL_OS_Deinit executing\n");
	
	if (fmParams.sock >= 0)
	{
		close(fmParams.sock);
		fmParams.sock = -1;
	}
	
	rc = pthread_key_delete(thread_socket_vector);
	if(0 != rc)
	{
		FM_TRACE("FMHAL_OS_Deinit | pthread_key_delete() failed: %s", strerror(rc));
	}
    
	FM_END();
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
	FM_BEGIN();

	FMC_UNUSED_PARAMETER(taskName);

	FMC_OS_ASSERT (taskHandle == FMC_OS_TASK_HANDLE_FM);

	/* verify the desired task is not already running */
	if (fmParams.taskRunning == FMC_TRUE ||
			fmParams2.taskRunning == FMC_TRUE)
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
	fmParams2.taskRunning = FMC_TRUE;

	/* and finally create the task */
	rc = pthread_create(&threadHandle, // thread structure
				NULL, // no attributes for now
				FmStackThread, // main function
				NULL);
	if(0 != rc)
	{
		FM_TRACE("FMHAL_OS_CreateTask | pthread_create() failed to create 1st FM thread: %s", strerror(errno));
		goto err;
	}

	FM_TRACE("Created 1st FM thread, id = %u", threadHandle);

	fmParams.threadHandle = threadHandle;

	fmParams2.taskHandle = taskHandle;
	fmParams2.taskCallback = NULL;

	rc = pthread_create(&threadHandle, // thread structure
				NULL, // no attributes for now
				fm_wait_for_interrupt_thread,
				NULL);
	if(0 != rc)
	{
		FM_TRACE("FMHAL_OS_CreateTask | pthread_create() failed to create 2nd FM thread: %s", strerror(errno));
		goto err;
	}

	FM_TRACE("Created 2nd FM thread, id = %u", threadHandle);

	fmParams2.threadHandle = threadHandle;

	ret = FMC_STATUS_SUCCESS;
	goto out;

err:
	/* failed to create the task */
	fmParams.taskRunning = FMC_FALSE;
	fmParams2.taskRunning = FMC_FALSE;
	ret = FMC_STATUS_FAILED;
out:
	FM_END();
	return ret;
}


/*-------------------------------------------------------------------------------
 * FMC_OS_DestroyTask()
 *
 */
FmcStatus FMC_OS_DestroyTask(FmcOsTaskHandle taskHandle)
{
	FM_BEGIN();

	FM_TRACE("FMHAL_OS_DestroyTask: killing task %d", taskHandle);
    
	FMC_OS_ASSERT (taskHandle == FMC_OS_TASK_HANDLE_FM);

	fmParams.taskRunning = FMC_FALSE;
	fmParams2.taskRunning = FMC_FALSE;

	FM_END();
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
	pthread_mutexattr_t mutex_attr;
	FM_BEGIN();
	
	FMC_UNUSED_PARAMETER(semaphoreName);
		
	/* we use recursive mutex - a mutex owner can relock the mutex. to release it,
	     the mutex shall be unlocked as many times as it was locked
	  */
	rc = pthread_mutexattr_init(&mutex_attr);
	if(rc != 0)
	{
		FM_TRACE("FMC_OS_CreateSemaphore | pthread_mutexattr_init() failed: %s", strerror(rc));
		return FMC_STATUS_FAILED;
	}
	
	rc = pthread_mutexattr_settype( &mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	if(rc != 0)
	{
		FM_TRACE("FMC_OS_CreateSemaphore | pthread_mutexattr_settype() failed: %s", strerror(rc));
		pthread_mutexattr_destroy(&mutex_attr);
		return FMC_STATUS_FAILED;
	}
		
	rc = pthread_mutex_init(*semaphoreHandle, &mutex_attr);
	if(rc != 0)
	{
		FM_TRACE("FMC_OS_CreateSemaphore | pthread_mutex_init() failed: %s", strerror(rc));
		pthread_mutexattr_destroy(&mutex_attr);
		return FMC_STATUS_FAILED;
	}
	
	rc = pthread_mutexattr_destroy(&mutex_attr);
	if(rc != 0)
	{
		FM_TRACE("FMC_OS_CreateSemaphore | pthread_mutexattr_destroy() failed: %s", strerror(rc));
		pthread_mutex_destroy(*semaphoreHandle);
		return FMC_STATUS_FAILED;
	}
			
	FM_END();	
	return FMC_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * FMC_OS_DestroySemaphore()
 *
 */
FmcStatus FMC_OS_DestroySemaphore(FmcOsSemaphoreHandle semaphoreHandle)
{
	int rc;
	FM_BEGIN();

	FM_TRACE("FMC_OS_DestroySemaphore: killing semaphore");
	
	rc = pthread_mutex_destroy(semaphoreHandle);
	if (rc != 0)
	{
		FM_TRACE("FMC_OS_DestroySemaphore | pthread_mutex_destroy() failed: %s", strerror(rc));
		return FMC_STATUS_FAILED;
	}

	FM_END();
	return FMC_STATUS_SUCCESS;
}


FmcStatus FMC_OS_LockSemaphore(FmcOsSemaphoreHandle semaphore)
{
	int rc;
	
	rc = pthread_mutex_lock(semaphore);
	if (rc != 0)
	{
		FM_TRACE("FMC_OS_LockSemaphore: pthread_mutex_lock() failed: %s", strerror(rc));
		return FMC_STATUS_FAILED;

	}

	return FMC_STATUS_SUCCESS;
}

FmcStatus FMC_OS_UnlockSemaphore(FmcOsSemaphoreHandle semaphore)
{
	int rc;

	rc = pthread_mutex_unlock(semaphore);
	if(rc != 0)
	{
		FM_TRACE("FMC_OS_UnlockSemaphore: pthread_mutex_unlock() failed: %s", strerror(rc));
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

	bzero(&evp, sizeof(evp));

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

    // Create the timer
    evp.sigev_value.sival_int = *timerHandle; // pass the handle as argument
    evp.sigev_notify_function = TimerHandlerFunc;
    evp.sigev_notify_attributes = NULL;
    evp.sigev_notify = SIGEV_THREAD;

    rc = timer_create(CLOCK_REALTIME, &evp, &timerParams[idx].timerId);
	//printf("Create Timer: %s, handle=%d, timerId=%d\n", timerName, idx, timerParams[idx].timerId);
	if (rc != 0)
	{
		timerParams[idx].timerUsed = 0;
		FM_TRACE("FMC_OS_CreateTimer: timer_create() failed: %s", strerror(errno));
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

	//printf("FMCOS | FMC_OS_DestroyTimer | timerHandle = %d\n", timerHandle);
	FM_ASSERT(timerHandle < FMHAL_OS_MAX_NUM_OF_TIMERS);
	FM_ASSERT(timerParams[timerHandle].timerUsed == 1);
		
	rc = timer_delete(timerParams[timerHandle].timerId);
	//printf("FMCOS | FMC_OS_DestroyTimer | rc = %d\n", rc);
	if (rc == 0)
	{
		timerParams[timerHandle].timerId = 0;
		timerParams[timerHandle].timerUsed = 0;
		return FMC_STATUS_SUCCESS;
	}

	FM_TRACE("FMC_OS_DestroyTimer | timer_delete() failed: %s", strerror(errno));
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
	
	//printf("Reset Timer: %d, t=%d, eidx=%d\n", timerHandle, time, eventIdx);
	
	FM_ASSERT(timerHandle < FMHAL_OS_MAX_NUM_OF_TIMERS);
	FM_ASSERT(timerParams[timerHandle].timerUsed == 1);
	if (timerParams[timerHandle].timerId == 0){
		return FMC_STATUS_FAILED;
	}

	timerId = timerParams[timerHandle].timerId;
	//printf("FMC_OS_ResetTimer | timerHandle = %d , timerId = %d\n", timerHandle, timerId);
	
	timerParams[timerHandle].timerEvent = evt;

	bzero(&ts, sizeof(struct itimerspec));
	ts.it_value.tv_sec = time ? time / 1000 : 0;
	ts.it_value.tv_nsec = time ? (time % 1000) * 1000 * 1000 : 0;  //from mili to nano
	ts.it_interval.tv_sec = 0; // non-periodic
	ts.it_interval.tv_nsec = 0;	
	rc = timer_settime (timerId, 0, &ts, NULL);

	if (rc == 0)
	{
		return FMC_STATUS_SUCCESS;
	}
	else
	{
		FM_TRACE("FMC_OS_ResetTimer: timer_settime() failed: %s", strerror(errno));
		return FMC_STATUS_FAILED;
	}
}


/*-------------------------------------------------------------------------------
 * FMC_OS_CancelTimer()
 *
 */
FmcStatus FMC_OS_CancelTimer(FmcOsTimerHandle timerHandle)
{
	//disarm timer using 0 as expiration value
	return FMC_OS_ResetTimer(timerHandle, 0, 0xFF);
}

/*---------------------------------------------------------------------------
 * Called by the POSIX timer to indicate that the timer fired.
 * value is the index of the timer in the timers array
 */

static void TimerHandlerFunc(union sigval val)
{
	FmcOsTaskHandle taskHandle;
	FmcOsEvent evt;
	int idx = val.sival_int;
	int sock;
	FM_ASSERT(idx < FMHAL_OS_MAX_NUM_OF_TIMERS);

	//printf("\nTimerHandler! val=%d\n", value);
	FM_TRACE("TimerHandlerFunc: timer thread id %u\n", pthread_self());
	
	/* Timer event idx sent to the task */
	evt = timerParams[idx].timerEvent;
	/* The Timer event was triggered to this task */
	taskHandle = timerParams[idx].taskHandle;

	/* Timer threads will always create an adhoc socket since they
	 * are recreated every time a timer is fired, and thus don't have
	 * persistent thread specific storage. Even if they did have,
	 * we may end up having two timer handles running at the same time, leading
	 * to socket races. So creating a new, ad-hoc, socket here is safe */
	sock = CreateConnectedSocket(taskHandle);

	/* no point to continue running without being able to deliver event */
	if (-1 == sock)
	{
		FM_TRACE("TimerHandlerFunc: failed to create adhoc socket to task %d, can't deliver event!\n", taskHandle);
		FM_ASSERT(0);
	}

	if (FMC_STATUS_FAILED == Send_Unix_Domain(evt, sock))
	{
		FM_TRACE("TimerHandlerFunc: failed to send message to task %d\n", taskHandle);
		FM_ASSERT(0);
	}

	/* failing to close a socket here is severe, because the rate of creating these
	 * adhoc sockets is pretty high, e.g. in A2DP scenarios */
	if(-1 == close(sock))
	{
		FM_TRACE("TimerHandlerFunc: failed to close adhoc socket: %s\n", strerror(errno));
		FM_ASSERT(0);
	}
}

inline static int *GetSocketVector(void)
{
	int *socket_vector;

	/* get our copy of the socket vector */
	socket_vector = pthread_getspecific(thread_socket_vector);

	/* if we still don't have one, create it */
	if (NULL == socket_vector)
	{
		int i, rc;
		FM_TRACE("GetSocketVector: thread id %u is creating a new socket vector", pthread_self());
		
		socket_vector = (int *)malloc(sizeof(int)*(FMC_OS_LARGEST_TASK_HANDLE+1));
		if (NULL == socket_vector)
		{
			FM_TRACE("GetSocketVector | malloc() failed: %s", strerror(errno));
			return NULL;
		}

		for(i = 0; i < FMC_OS_LARGEST_TASK_HANDLE + 1; i++)
		{
			socket_vector[i] = -1;
		}

		rc = pthread_setspecific(thread_socket_vector, socket_vector);
		if (rc != 0)
		{
			FM_TRACE("GetSocketVector | pthread_setspecific() failed: %s", strerror(rc));
			return NULL;
		}
	}

	return socket_vector;
}

inline static int CreateConnectedSocket(FmcOsTaskHandle taskHandle)
{
	int sock;
	char *path = server_paths[taskHandle];
	FM_ASSERT(path != NULL);

	struct sockaddr_un servaddr; /* address of server */

	FM_TRACE("CreateConnectedSocket thread id %u is creating a connected socket for task %u", pthread_self(), taskHandle);
	
	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
	{
		FM_TRACE("CreateConnectedSocket | socket() failed: %s", strerror(errno));
		return -1;
	}
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sun_family = AF_UNIX;
	strcpy(servaddr.sun_path, path);
	
	if(-1 == connect(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)))
	{
		FM_TRACE("CreateConnectedSocket | connect() failed: %s", strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

inline static int GetConnectedSocket(int *socket_vector, FmcOsTaskHandle taskHandle)
{
	if (-1 == socket_vector[taskHandle])
	{
		socket_vector[taskHandle] = CreateConnectedSocket(taskHandle);
	}

	return socket_vector[taskHandle];
}

/*-------------------------------------------------------------------------------
 * FMC_OS_SendEvent()
 *
 */
FmcStatus FMC_OS_SendEvent(FmcOsTaskHandle taskHandle, FmcOsEvent evt)
{
	int *socket_vector;
	int sock;

	/* sanity check */
	FM_ASSERT(taskHandle <= FMC_OS_LARGEST_TASK_HANDLE);

	socket_vector = GetSocketVector();
	if (NULL == socket_vector)
	{
		return FMC_STATUS_FAILED;
	}

	sock = GetConnectedSocket(socket_vector, taskHandle);
	if (-1 == sock)
	{
		return FMC_STATUS_FAILED;
	}

	return Send_Unix_Domain(evt, sock);
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
	FMC_UNUSED_PARAMETER(param);
	FMC_U32 idx;
	fd_set rfds;
	struct timeval tv;
	int rc;
	int sock;
	FmcOsEventCallback callback;

	FM_BEGIN_L(HIGHEST_TRACE_LVL);
	
	/* set self priority to 0.
	   The UART threads are set to -1, stack threads are set to 0, and the
	   application thread will be set to 1 */
	if (-1 == setpriority(PRIO_PROCESS, 0, 0))
	{
		FM_TRACE("FMHAL GenericThread: failed to set priority: %s", strerror(errno));
		FM_ASSERT(0);
	}

	sock = fmParams.sock;
	callback = fmParams.taskCallback;
	if (callback == NULL)
	{
		FM_TRACE("FMHALOS | GenericThread | got NULL callback");
	}

	/*sock was already created */
	FM_ASSERT(sock > 0);

	/* Stay in this thread while still connected */
	while (fmParams.taskRunning) {
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);

		tv.tv_sec = 0;
		tv.tv_usec = 500 * 1000; //wake up every 500 milli

		rc = select(sock+1, &rfds, NULL, NULL, &tv);
		/* timeout ? */
		if (rc == 0) {
			continue;
		}

		/* error ? */
		if (rc < 0) {
			/* nothing much to do really, but complain */
			FM_TRACE("GenericThread | select() failed: %s", strerror(errno));
			continue;
		}

		/* we have data ! */
		if (FD_ISSET(sock, &rfds)) {
			// retrieve the event id
			rc = recv(sock, &idx, sizeof(idx), 0);
			FM_ASSERT(rc == sizeof(idx));
			FM_TRACE("FM main thread callback sock=%d event=%d", sock, idx);
			if (callback != NULL)
				callback(idx);
		}
	}

	FM_TRACE("FMHALOS | GenericThread | thread id %u is now terminating", pthread_self());
	FM_END();
	return 0;
}

/*
 * FM Stack's second thread.
 * This thread waits for fm interrupts.
 * upon Invokation, it should receive the hci device
 * number on which to attach its socket.
 */
static void *fm_wait_for_interrupt_thread(void *dev)
{
	#define HCI_FM_EVENT 0xF0

	int dd, ret = FMC_STATUS_SUCCESS, len;
	struct hci_filter nf;
	hci_event_hdr *hdr;
	unsigned char buf[HCI_MAX_EVENT_SIZE];
	FM_BEGIN_L(HIGHEST_TRACE_LVL);

	dd = hci_open_dev((int)dev);
	if (dd < 0) {
		FM_ERROR_SYS("can't open device hci%d", dev);
		ret = FMC_STATUS_FAILED;
		goto out;
	}

	hci_filter_clear(&nf);
	hci_filter_set_ptype(HCI_EVENT_PKT,  &nf);

	/*
	 * Our interrupt event is 0xF0.
	 * BlueZ filters out two most-significant bits of the
	 * event type, which makes us wait for event 0x48.
	 * It is still ok since 0x48 does not exists as an event, too.
	 * to be 100% we verify the event type upon arrival
	 */
	hci_filter_set_event(HCI_FM_EVENT, &nf);
	if (setsockopt(dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf)) < 0) {
		FM_ERROR_SYS("failed to set socket options");
		ret = FMC_STATUS_FAILED;
		goto close;
	}

	/* continue running as long as not terminated */
	while (fmParams2.taskRunning) {
		struct pollfd p;
		int n;

		p.fd = dd; p.events = POLLIN;

		/* poll socket, wait for fm interrupts */
		while ((n = poll(&p, 1, 500)) == -1) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			FM_ERROR_SYS("failed to poll socket");
			ret = FMC_STATUS_FAILED;
			goto close;
		}

		/* we timeout once in a while */
		/* this let's us the chance to check if we were terminated */
		if (0 == n)
			continue;

		/* read newly arrived data */
		/* TBD: rethink assumption about single arrival */
		while ((len = read(dd, buf, sizeof(buf))) < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			FM_ERROR_SYS("failed to read socket");
			ret = FMC_STATUS_FAILED;
			goto close;
		}

		/* skip packet type byte */
		hdr = (void *) (buf + 1);

		/* continue if this is not the event we are waiting for */
		if (HCI_FM_EVENT != hdr->evt)
			continue;

		FM_TRACE_L(LOWEST_TRACE_LVL, "interrupt received!");
		/* notify FM stack that an interrupt has arrived */
		if (fmParams2.taskCallback)
			fmParams2.taskCallback(0);/* the param is ignored */
	}

close:
	hci_close_dev(dd);
out:
	FM_END();
	pthread_exit((void *)ret);
}

void FMC_OS_RegisterFMInterruptCallback(FmcOsEventCallback func)
{
	fmParams2.taskCallback = func;
}

/*---------------------------------------------------------------------------
 * Convert bthal event to event index. 
 */
static FMC_U32 FmcOs_ConvertToEventIdx(FmcOsEvent evt)
{
    FMC_U32 eventIdx = 0;

    while (evt > 0)
    {
        evt >>= 1;
        eventIdx++;
    }

    eventIdx--;

    return eventIdx;
}

/*---------------------------------------------------------------------------
 * Convert event index to bthal event. 
 */
static FmcOsEvent FmcOs_ConvertToFmcEvent(FMC_U32 evtIdx)
{
    FmcOsEvent FmcEvent = 1;

    while (evtIdx > 0)
    {
        FmcEvent <<= 1;
        evtIdx--;
    }

    return FmcEvent;
}

#endif  /* FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED */


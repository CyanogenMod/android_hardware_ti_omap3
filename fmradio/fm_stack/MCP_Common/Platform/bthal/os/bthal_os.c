/******************************************************************************\
##                                                                            *
## Unpublished Proprietary and Confidential Information of Texas Instruments  *
## Israel Ltd. Do Not Disclose.                                               *
## Copyright 2008 Texas Instruments Israel Ltd.                               *
## All rights reserved. All unpublished rights reserved.                      *
##                                                                            *
## No part of this work may be used or reproduced in any form or by any       *
## means, or stored in a database or retrieval system, without prior written  *
## permission of Texas Instruments Israel Ltd. or its parent company Texas    *
## Instruments Incorporated.                                                  *
## Use of this work is subject to a license from Texas Instruments Israel     *
## Ltd. or its parent company Texas Instruments Incorporated.                 *
##                                                                            *
## This work contains Texas Instruments Israel Ltd. confidential and          *
## proprietary information which is protected by copyright, trade secret,     *
## trademark and other intellectual property rights.                          *
##                                                                            *
## The United States, Israel  and other countries maintain controls on the    *
## export and/or import of cryptographic items and technology. Unless prior   *
## authorization is obtained from the U.S. Department of Commerce and the     *
## Israeli Government, you shall not export, reexport, or release, directly   *
## or indirectly, any technology, software, or software source code received  *
## from Texas Instruments Incorporated (TI) or Texas Instruments Israel,      *
## or export, directly or indirectly, any direct product of such technology,  *
## software, or software source code to any destination or country to which   *
## the export, reexport or release of the technology, software, software      *
## source code, or direct product is prohibited by the EAR. The subject items *
## are classified as encryption items under Part 740.17 of the Commerce       *
## Control List (“CCL”). The assurances provided for herein are furnished in  *
## compliance with the specific encryption controls set forth in Part 740.17  *
## of the EAR -Encryption Commodities and Software (ENC).                     *
##                                                                            *
## NOTE: THE TRANSFER OF THE TECHNICAL INFORMATION IS BEING MADE UNDER AN     *
## EXPORT LICENSE ISSUED BY THE ISRAELI GOVERNMENT AND THE APPLICABLE EXPORT  *
## LICENSE DOES NOT ALLOW THE TECHNICAL INFORMATION TO BE USED FOR THE        *
## MODIFICATION OF THE BT ENCRYPTION OR THE DEVELOPMENT OF ANY NEW ENCRYPTION.*
## UNDER THE ISRAELI GOVERNMENT'S EXPORT LICENSE, THE INFORMATION CAN BE USED *
## FOR THE INTERNAL DESIGN AND MANUFACTURE OF TI PRODUCTS THAT WILL CONTAIN   *
## THE BT IC.                                                                 *
##                                                                            *
\******************************************************************************/
/*******************************************************************************\
*
*   FILE NAME:      bthal_os.c
*
*   DESCRIPTION:    This file contain implementation of uart in WIN
*
*   AUTHOR:         Ronen Levy
*
\*******************************************************************************/

/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#define BTHAL_OS_PRAGMAS
#include "bthal_config.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <mmsystem.h>  /* You must also include winmm.lib when compiling
                        * to access the Windows multimedia subsystem. */
#include "osapi.h"
#include "bthal_os.h"

/********************************************************************************
 *
 * Constants 
 *
 *******************************************************************************/
#define BTHAL_OS_MAX_NUM_OF_EVENTS_PER_TASK             (7)

#if BTHAL_OS_MAX_NUM_OF_EVENTS_PER_TASK < BTHAL_OS_MAX_NUM_OF_EVENTS_STACK
#error BTHAL_OS_MAX_NUM_OF_EVENTS_PER_TASK must be greater than BTHAL_OS_MAX_NUM_OF_EVENTS_STACK
#endif

#if BTHAL_OS_MAX_NUM_OF_EVENTS_PER_TASK < BTHAL_OS_MAX_NUM_OF_EVENTS_TRANSPORT
#error BTHAL_OS_MAX_NUM_OF_EVENTS_PER_TASK must be greater than BTHAL_OS_MAX_NUM_OF_EVENTS_TRANSPORT
#endif

#if BTHAL_OS_MAX_NUM_OF_EVENTS_PER_TASK < BTHAL_OS_MAX_NUM_OF_EVENTS_A2DP
#error BTHAL_OS_MAX_NUM_OF_EVENTS_PER_TASK must be greater than BTHAL_OS_MAX_NUM_OF_EVENTS_A2DP
#endif


/*******************************************************************************
 *
 * Macro definitions
 *
 ******************************************************************************/
#define UNUSED_PARAMETER(_PARM)     ((_PARM) = (_PARM))


/********************************************************************************
 *
 * Data Structures
 *
 *******************************************************************************/

typedef struct T_BTHAL_OS_TASK_PARAMS
{
    /* thread Id */
    DWORD threadId;  

    /* Handle to the thread */
    HANDLE threadHandle;

    /* Events signaled to the task */
    HANDLE notifyEvent[BTHAL_OS_MAX_NUM_OF_EVENTS_PER_TASK];                     

    /* handle to the task */
    BthalOsTaskHandle taskHandle;       

    /* Tracks whether the task is running */
    BOOL taskRunning;                

    /* Callbacks provided by the task */
    EventCallback taskCallback;             

} BTHAL_OS_TASK_PARAMS;


typedef struct T_BTHAL_OS_TIMER_PARAMS
{
    /* Timer event sent to the task */
    U32 timerEventIdx;

    /* Identifier for the timer events */ 
    MMRESULT timerId;

    /* associate timerHandle to taskHandle (used in TimerFired()) */
    BthalOsTaskHandle taskHandle;

} BTHAL_OS_TIMER_PARAMS;


typedef struct T_BTHAL_OS_PARAMS
{
    /* stack task parameters */
    BTHAL_OS_TASK_PARAMS stackParams;

    /* A2DP task parameters */
    BTHAL_OS_TASK_PARAMS a2dpParams;

    /* timer parameters */
    BTHAL_OS_TIMER_PARAMS timerParams[BTHAL_OS_MAX_NUM_OF_TIMERS];

    /* Handles to the semaphores */
    HANDLE semaphores[BTHAL_OS_MAX_NUM_OF_SEMAPHORES];

    /* Maximum timer timeout */
    U32 maxTimer;                                

    /* Mutex for OS_StopHardware() & OS_ResumeHardware() */
    HANDLE hwMutex;     

} BTHAL_OS_PARAMS;   


/********************************************************************************
 *
 * Globals
 *
 *******************************************************************************/


/***************************** BTHAL_OS_PARAMS ***************************************/

/* BTHAL control block */
BTHAL_OS_PARAMS bthal_os_cb;                 

/*

a PATCH to solve a problem created with the A2DP timer in Windows

The problem: 
While we are still inside BTHAL_OS_ResetTimer, the TimeFired callback is being called for the timer
we're just setting now. the timerId in bthal_os_cb.timerParams was still not updated, so in TimerFired
we did not know which timer fired and the Assert was asserted.

To workaround this, we try to find out whether indeed TimerFired is entered while we are still
executing BTHAL_OS_ResetTimer (using the variable inResetTimer). If it is true, we take the timerId
from the dwUser argument (which we previosly set in the call to timeSetEvent).
*
/* help track reentrancy */
BOOL inResetTimer = FALSE;

/********************************************************************************
 *
 * Internal function prototypes
 *
 *******************************************************************************/

/* Local functions */
static BOOL Os_CancelTimer(BthalOsTimerHandle timerHandle);
static BOOL Os_SendEvent(BthalOsTaskHandle taskHandle, U32 eventIdx);
static BOOL Os_SemTake(HANDLE mutex, BthalOsTime timeout);
static BOOL Os_SemGive(HANDLE mutex);
static BOOL Os_SetEvent(HANDLE hNotifyEvent);
static U32  Os_ConvertToEventIdx(BthalOsEvent evt);
static BthalOsEvent Os_ConvertToBthalEvent(U32 eventIdx);

static DWORD WINAPI  StackThread(LPVOID param);
static DWORD WINAPI  A2dpThread(LPVOID param);
static void CALLBACK TimerFired(UINT uTimerID, UINT uMsg,
                                DWORD dwUser, DWORD dw1, DWORD dw2);

/* External functions */
void PumpMessages(void);                        /* Defined in winmain.c */

/********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BTHAL_OS_Init()
 *
 */
BthalStatus BTHAL_OS_Init(BthalCallBack callback)
{
    TIMECAPS tc;
    U32 idx;
    HANDLE hEvent = NULL;

    UNUSED_PARAMETER(callback);

    /* init stack task parameters */
    bthal_os_cb.stackParams.taskHandle = 0;   
    bthal_os_cb.stackParams.taskRunning = FALSE;   
    bthal_os_cb.stackParams.threadId = 0;  
    bthal_os_cb.stackParams.threadHandle = NULL;  

    /* init A2DP task parameters */
    bthal_os_cb.a2dpParams.taskHandle = 0;   
    bthal_os_cb.a2dpParams.taskRunning = FALSE;   
    bthal_os_cb.a2dpParams.threadId = 0;  
    bthal_os_cb.a2dpParams.threadHandle = NULL; 

    /* init timer handles array */
    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_TIMERS; idx++)
    {
        bthal_os_cb.timerParams[idx].timerId = 0;
        bthal_os_cb.timerParams[idx].taskHandle = 0;
        bthal_os_cb.timerParams[idx].timerEventIdx = 0;
    }

    /* init semaphore handles array */
    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_SEMAPHORES; idx++)
    {
        bthal_os_cb.semaphores[idx] = NULL;
    }

    /* queries the timer device to determine its resolution */
    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR)
    {
        return BTHAL_STATUS_FAILED;
    }
    bthal_os_cb.maxTimer = tc.wPeriodMax;

    /* Windows event objects allow one thread to wait for an event
     * that is generated by other threads. We use this to determine
     * whether it's time for the stack thread to execute.
     * We also use this to notify that timer is expired.
     */
    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_EVENTS_STACK; idx++)
    {
        Assert ((hEvent = CreateEvent(NULL, FALSE, FALSE, NULL)) != NULL);
        bthal_os_cb.stackParams.notifyEvent[idx] = hEvent;  
    }

    /* Init A2DP events */
    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_EVENTS_A2DP; idx++)
    {
        Assert ((hEvent = CreateEvent(NULL, FALSE, FALSE, NULL)) != NULL);
        bthal_os_cb.a2dpParams.notifyEvent[idx] = hEvent;   
    }

    /* The hardware will rely on this mutex before it can execute.
     * This is necessary because the Windows hardware driver will be
     * based on a high-priority thread, not an interrupt.
     */
    Assert ((bthal_os_cb.hwMutex = CreateMutex(NULL, FALSE, NULL)) != 0);

    return BTHAL_STATUS_SUCCESS;    
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_Deinit()
 *
 */
BthalStatus BTHAL_OS_Deinit(void)
{
    U32 idx;

    if (CloseHandle(bthal_os_cb.hwMutex) == FALSE)
    {
        return BTHAL_STATUS_FAILED;
    }

    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_EVENTS_STACK; idx++)
    {
        if (CloseHandle(bthal_os_cb.stackParams.notifyEvent[idx]) == FALSE)
        {
            return BTHAL_STATUS_FAILED;
        }
    }

    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_EVENTS_A2DP; idx++)
    {
        if (CloseHandle(bthal_os_cb.a2dpParams.notifyEvent[idx]) == FALSE)
        {
            return BTHAL_STATUS_FAILED;
        }
    }

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_CreateTask()
 *
 */
BthalStatus BTHAL_OS_CreateTask(BthalOsTaskHandle taskHandle, 
                                EventCallback callback, 
                                const char *taskName)
{
    BTHAL_OS_TASK_PARAMS*   bthal_task_params = NULL;
    DWORD                   threadId = 0;           /* thread Id */
    HANDLE                  thread = NULL;          /* a handle to the new thread */
    DWORD                   dwCreationFlags = 0;    /* the thread is created in a running state */
    LPTHREAD_START_ROUTINE  pThreadFunc = NULL;

    UNUSED_PARAMETER(taskName);

    switch (taskHandle)
    {
    case BTHAL_OS_TASK_HANDLE_STACK:
        bthal_task_params = &bthal_os_cb.stackParams;
        pThreadFunc = (LPTHREAD_START_ROUTINE)StackThread;
        break;

    case BTHAL_OS_TASK_HANDLE_A2DP:
        bthal_task_params = &bthal_os_cb.a2dpParams;
        pThreadFunc = (LPTHREAD_START_ROUTINE)A2dpThread;
        /* Stack thread priority is THREAD_PRIORITY_NORMAL, so A2DP task is 1 point above normal priority */
        //AssertEval(SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL) != 0);
        break;

    default:
        Assert(FALSE);   
        break;
    }

    /* verify the desired task is not already running */
    if (bthal_task_params->taskRunning == TRUE)
    {
        return BTHAL_STATUS_FAILED;
    }

    /* 
     * set task callback and handle - must be done before task is started to have this 
     * info ready for it
     */
    bthal_task_params->taskHandle = taskHandle;
    bthal_task_params->taskCallback = callback;

    /* mark that the desired task is now running */
    bthal_task_params->taskRunning = TRUE;
    /* and finally create the task */
    thread = CreateThread(NULL, 4096, pThreadFunc, NULL, dwCreationFlags, &threadId);
    if (thread == NULL)
    {
        /* failed to create the task */
        bthal_task_params->taskRunning = FALSE;
        return BTHAL_STATUS_FAILED;
    }

    /* set thread information */
    bthal_task_params->threadId = threadId;
    bthal_task_params->threadHandle = thread;

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_DestroyTask()
 *
 */
BthalStatus BTHAL_OS_DestroyTask(BthalOsTaskHandle taskHandle)
{
    DWORD lpExitCode;
    BOOL retVal = 1;

    switch (taskHandle)
    {
    case BTHAL_OS_TASK_HANDLE_STACK:
        bthal_os_cb.stackParams.taskRunning = FALSE;
        Os_SendEvent(BTHAL_OS_TASK_HANDLE_STACK, 0); /* dummy event to terminate stack task */

        do
        {
            retVal = GetExitCodeThread(bthal_os_cb.stackParams.threadHandle, &lpExitCode);
            PumpMessages();
            Sleep(50);
        }
        while ((retVal) && (lpExitCode == STILL_ACTIVE));
        break;

    case BTHAL_OS_TASK_HANDLE_A2DP:
        bthal_os_cb.a2dpParams.taskRunning = FALSE;
        Os_SendEvent(BTHAL_OS_TASK_HANDLE_A2DP, 0); /* dummy event to terminate a2dp task */

        do
        {
            retVal = GetExitCodeThread(bthal_os_cb.a2dpParams.threadHandle, &lpExitCode);
            PumpMessages();
            Sleep(50);
        }
        while ((retVal) && (lpExitCode == STILL_ACTIVE));
        break;

    default:
        Assert(FALSE);    
        break;
    }

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_SendEvent()
 *
 */
BthalStatus BTHAL_OS_SendEvent(BthalOsTaskHandle taskHandle, BthalOsEvent evt)
{
    U32 eventIdx = 0;

    eventIdx = Os_ConvertToEventIdx(evt);

    Os_SendEvent(taskHandle, eventIdx);

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_CreateSemaphore()
 *
 */
BthalStatus BTHAL_OS_CreateSemaphore(const char *semaphoreName, 
                                     BthalOsSemaphoreHandle *semaphoreHandle)
{
    U32 idx;
    HANDLE mutex = 0;

    UNUSED_PARAMETER(semaphoreName);

    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_SEMAPHORES; idx++)
    {
        if (bthal_os_cb.semaphores[idx] == NULL)
        {
            break;
        }
    }

    if (idx == BTHAL_OS_MAX_NUM_OF_SEMAPHORES)
    {
        /* exceeds maximum of available handles */
        return BTHAL_STATUS_FAILED;
    }

    Assert ((mutex = CreateMutex(NULL, FALSE, NULL)) != 0);

    bthal_os_cb.semaphores[idx] = mutex;
    *semaphoreHandle = (BthalOsSemaphoreHandle)idx;

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_DestroySemaphore()
 *
 */
BthalStatus BTHAL_OS_DestroySemaphore(BthalOsSemaphoreHandle semaphoreHandle)
{
    Assert(semaphoreHandle < BTHAL_OS_MAX_NUM_OF_SEMAPHORES);

    if (bthal_os_cb.semaphores[semaphoreHandle] == NULL)
    {
        /* mutex does not exists */
        return BTHAL_STATUS_FAILED;
    }

    if (CloseHandle(bthal_os_cb.semaphores[semaphoreHandle]) == FALSE)
    {
        return BTHAL_STATUS_FAILED;
    }

    bthal_os_cb.semaphores[semaphoreHandle] = NULL;

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_LockSemaphore()
 *
 */
BthalStatus BTHAL_OS_LockSemaphore(BthalOsSemaphoreHandle semaphoreHandle,
                                   BthalOsTime timeout)
{
    HANDLE mutex; 

    Assert(semaphoreHandle < BTHAL_OS_MAX_NUM_OF_SEMAPHORES);

    mutex = bthal_os_cb.semaphores[semaphoreHandle];

    if (mutex == NULL)
    {
        /* mutex does not exists */
        return BTHAL_STATUS_FAILED;
    }

    if (timeout == 0)
    {
        while (Os_SemTake(mutex, timeout) == FALSE)
            PumpMessages();
    }
    else
    {
        if (FALSE == Os_SemTake(mutex, timeout))
        {
            /* fail to lock the semaphore, or semaphore does not exists */
            return BTHAL_STATUS_FAILED;
        }
    }

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_UnlockSemaphore()
 *
 */
BthalStatus BTHAL_OS_UnlockSemaphore(BthalOsSemaphoreHandle semaphoreHandle)
{
    HANDLE mutex; 

    Assert(semaphoreHandle < BTHAL_OS_MAX_NUM_OF_SEMAPHORES);

    mutex = bthal_os_cb.semaphores[semaphoreHandle];

    if (mutex == NULL)
    {
        /* mutex does not exists */
        return BTHAL_STATUS_FAILED;
    }

    if (FALSE == Os_SemGive(mutex))
    {
        return BTHAL_STATUS_FAILED;
    }

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_StopHardware()
 *
 */
BthalStatus BTHAL_OS_StopHardware(void)
{
    /* Because the hardware thread always takes this mutex before it
     * runs, we can take it to prevent the hardware thread from running.
     * In a typical MINIOS system, this code should be replaced with
     * a system call to disable I/O interrupts.
     */

    while (Os_SemTake(bthal_os_cb.hwMutex, 0) == FALSE)
        PumpMessages();

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_ResumeHardware()
 *
 */
BthalStatus BTHAL_OS_ResumeHardware(void)
{
    /* Because the hardware thread always takes this mutex before it
     * runs, we can take it to prevent the hardware thread from running.
     */

    if (FALSE == Os_SemGive(bthal_os_cb.hwMutex))
    {
        return BTHAL_STATUS_FAILED;
    }

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_CreateTimer()
 * note: timer event is created in bthal_os_init(). 
 */
BthalStatus BTHAL_OS_CreateTimer(BthalOsTaskHandle taskHandle, 
                                 const char *timerName, 
                                 BthalOsTimerHandle *timerHandle)
{
    U32 idx;

    UNUSED_PARAMETER(timerName);

    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_TIMERS; idx++)
    {
        if (bthal_os_cb.timerParams[idx].timerId == 0)
        {
            break;
        }
    }

    if (idx == BTHAL_OS_MAX_NUM_OF_TIMERS)
    {
        /* exceeds maximum of available timers */
        return BTHAL_STATUS_FAILED;
    }

    /* mark as used (when timer started, will hold timerId) */
    bthal_os_cb.timerParams[idx].timerId = 0xff;  

    /* associate timerHandle to taskHandle (used in TimerFired()) */
    bthal_os_cb.timerParams[idx].taskHandle = taskHandle;

    *timerHandle = (BthalOsTimerHandle)idx;

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_DestroyTimer()
 *
 */
BthalStatus BTHAL_OS_DestroyTimer(BthalOsTimerHandle timerHandle)
{
    U32 timerId;

    Assert(timerHandle < BTHAL_OS_MAX_NUM_OF_TIMERS);

    timerId = bthal_os_cb.timerParams[timerHandle].timerId;

    if (timerId != 0)
    {

        /*  This is a fix for the FM but should be applyed to all. 
        *   The current implementation of the cancel timer and destroy timer is wrong.
     *  The Create timer indicates that a timer was created by setting it's id to 0xff.
     *  Therefore the cancel should change back the timer id to 0xff and not back to 0 that indicates 
     *  the timer is free. This will couse that if one creates timer and then cancels it some one else can now 
     *  use this timer even though the timer wasen't freed by the first creator of the timer.
     */
        /*if (bthal_os_cb.timerParams[timerHandle].taskHandle == BTHAL_OS_TASK_HANDLE_FM)
        {
            if (bthal_os_cb.timerParams[timerHandle].timerId==0xff)
            {
                bthal_os_cb.timerParams[timerHandle].timerId = 0;
                return BTHAL_STATUS_SUCCESS;
            }
            else
            {
                if (timeKillEvent(timerId) == TIMERR_NOERROR)
                {
                    bthal_os_cb.timerParams[timerHandle].timerId = 0;
                    return BTHAL_STATUS_SUCCESS;
                }
            }
        }
        else
        {*/
            if (timeKillEvent(timerId) == TIMERR_NOERROR)
            {
                bthal_os_cb.timerParams[timerHandle].timerId = 0;
                return BTHAL_STATUS_SUCCESS;
            }
       /* }*/
    }
    else
    {
        return BTHAL_STATUS_SUCCESS;
    }

    return BTHAL_STATUS_FAILED;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_ResetTimer()
 *
 */
BthalStatus BTHAL_OS_ResetTimer(BthalOsTimerHandle timerHandle, 
                                BthalOsTime time, 
                                BthalOsEvent evt)
{
    MMRESULT timerId;

    U32 eventIdx = Os_ConvertToEventIdx(evt);

    inResetTimer = TRUE;

    Assert(timerHandle < BTHAL_OS_MAX_NUM_OF_TIMERS);

    timerId = bthal_os_cb.timerParams[timerHandle].timerId;

    if (timerId != 0)
    {

        if (FALSE == Os_CancelTimer(timerHandle))
        {
            inResetTimer = FALSE;
            return BTHAL_STATUS_FAILED;
        }
    }

    /* If the requested time is greater than allowed, pick a smaller timeout.
     * This is OK because we know that EVM verifies with OS_GetSystemTime 
     * before actually firing any timers.
     */
    if (time > bthal_os_cb.maxTimer)
    {
        time = bthal_os_cb.maxTimer - 1;
    }

    timerId = timeSetEvent(time, 5, TimerFired, timerHandle, TIME_ONESHOT | TIME_CALLBACK_FUNCTION);

    if (timerId != 0)
    {
        bthal_os_cb.timerParams[timerHandle].timerEventIdx = eventIdx;
        bthal_os_cb.timerParams[timerHandle].timerId = timerId; 
        inResetTimer = FALSE;
        return BTHAL_STATUS_SUCCESS;
    }
    else
    {
        inResetTimer = FALSE;
        return BTHAL_STATUS_FAILED;
    }
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_CancelTimer()
 *
 */
BthalStatus BTHAL_OS_CancelTimer(BthalOsTimerHandle timerHandle)
{
    Assert(timerHandle < BTHAL_OS_MAX_NUM_OF_TIMERS);

    if (FALSE == Os_CancelTimer(timerHandle))
    {
        return BTHAL_STATUS_FAILED;
    }

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_Sleep()
 *
 */
BthalStatus BTHAL_OS_Sleep(BthalOsTime time)
{
    Sleep(time);

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_OS_GetSystemTime()
 *
 */
BthalStatus BTHAL_OS_GetSystemTime(BthalOsTime *time)
{
    /* Uses the multimedia timer for the best accuracy */
    *time = timeGetTime();

    return BTHAL_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
 * StackThread() 
 *      This is the main thread for the stack. Calls the stack callback handler
 *      to handle the received events (i.e. schedule task and timer expired).
 *
 * Requires:
 *     
 * Parameters:
 *
 * Returns:
 *      TRUE    success
 *      FALSE   failed
 */
static DWORD WINAPI StackThread(LPVOID param)
{
    EventCallback callback;
    DWORD result;
    HANDLE eventArray[BTHAL_OS_MAX_NUM_OF_EVENTS_STACK];
    U32 idx;

    UNUSED_PARAMETER(param);

    /* Fill the event array with possible stack task events */
    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_EVENTS_STACK; idx++)
    {
        eventArray[idx] = bthal_os_cb.stackParams.notifyEvent[idx];
    }

    /* get the event callback for the stack thread */
    callback = bthal_os_cb.stackParams.taskCallback;

    /* Pause briefly to allow the application thread to start.  Some
     * Initialization is performed during the call to APP_Init().  If
     * the stack thread starts too quickly, initialization may not happen
     * in time.  TODO: Instead of pausing, block on an event that is
     * triggered when the application has completed initialization.
     */
    Sleep(500);

    /* Stay in this thread while still connected */
    while (bthal_os_cb.stackParams.taskRunning)
    {

        /* Wake up every now and then just in case a DeInit is requested */
        result = WaitForMultipleObjects(BTHAL_OS_MAX_NUM_OF_EVENTS_STACK, eventArray, FALSE, INFINITE);

        callback(Os_ConvertToBthalEvent(result - WAIT_OBJECT_0));
    }

    return 0;
}


/*---------------------------------------------------------------------------
 * A2dpThread() 
 *      This is the thread for the A2DP. Calls the A2DP callback handler
 *      to handle the received events.
 *
 * Requires:
 *     
 * Parameters:
 *
 * Returns:
 *      TRUE    success
 *      FALSE   failed
 */
static DWORD WINAPI A2dpThread(LPVOID param)
{
    EventCallback callback;
    DWORD result;
    HANDLE eventArray[BTHAL_OS_MAX_NUM_OF_EVENTS_A2DP];
    U32 idx;

    UNUSED_PARAMETER(param);

    /* Fill the event array with possible A2DP task events */
    for (idx=0; idx < BTHAL_OS_MAX_NUM_OF_EVENTS_A2DP; idx++)
    {
        eventArray[idx] = bthal_os_cb.a2dpParams.notifyEvent[idx];
    }

    /* get the event callback for the A2DP thread */
    callback = bthal_os_cb.a2dpParams.taskCallback;

    /* Pause briefly to allow the application thread to start.  Some
     * Initialization is performed during the call to APP_Init().  If
     * the stack thread starts too quickly, initialization may not happen
     * in time.  TODO: Instead of pausing, block on an event that is
     * triggered when the application has completed initialization.
     */
    Sleep(500);

    /* Stay in this thread while still connected */
    while (bthal_os_cb.a2dpParams.taskRunning)
    {

        /* Wake up every now and then just in case a DeInit is requested */
        result = WaitForMultipleObjects(BTHAL_OS_MAX_NUM_OF_EVENTS_A2DP, eventArray, FALSE, INFINITE);

        callback(Os_ConvertToBthalEvent(result - WAIT_OBJECT_0));
    }

    return 0;
}



/*---------------------------------------------------------------------------
 * Called by Windows to indicate that the timer fired.
 */
static void CALLBACK TimerFired(UINT uTimerID, UINT uMsg,
                                DWORD dwUser, DWORD dw1, DWORD dw2)
{
    BthalOsTimerHandle timerHandle;
    BthalOsTaskHandle taskHandle;
    U32 eventIdx;
    U32 idx;

    BOOL myInResetTimer = inResetTimer;

    UNUSED_PARAMETER(uMsg);
    UNUSED_PARAMETER(dw1);
    UNUSED_PARAMETER(dw2);

    for (idx=0; idx< BTHAL_OS_MAX_NUM_OF_TIMERS; idx++)
    {
        if (bthal_os_cb.timerParams[idx].timerId == uTimerID)
        {
            break;
        }
    }

    if ((idx >= BTHAL_OS_MAX_NUM_OF_TIMERS) && myInResetTimer)
    {
        idx = dwUser; /* dwUser holds the timerHandle */
    }

    Assert(idx < BTHAL_OS_MAX_NUM_OF_TIMERS);

    timerHandle = (BthalOsTaskHandle)idx;

    eventIdx = bthal_os_cb.timerParams[timerHandle].timerEventIdx;  /* Timer event idx sent to the task */
    taskHandle = bthal_os_cb.timerParams[timerHandle].taskHandle;   /* The Timer event was triggered to this task */

    bthal_os_cb.timerParams[timerHandle].timerId = 0;

    Os_SendEvent(taskHandle, eventIdx);

}


/*---------------------------------------------------------------------------
 * Cancel a timer
 */
static BOOL Os_CancelTimer(BthalOsTimerHandle timerHandle)
{
    U32 timerId;

    timerId = bthal_os_cb.timerParams[timerHandle].timerId;

    if (timerId != 0)
    {
        if (timeKillEvent(timerId) == TIMERR_NOERROR)
        {
            /*  This is a fix for the FM but should be applyed to all. 
            *   The current implementation of the cancel timer and destroy timer is wrong.
         *  The Create timer indicates that a timer was created by setting it's id to 0xff.
         *  Therefore the cancel should change back the timer id to 0xff and not back to 0 that indicates 
         *  the timer is free. This will couse that if one creates timer and then cancels some one else can now 
         *  use this timer even though the timer wasen't freed by the first creator of the timer.
         */
            /*if (bthal_os_cb.timerParams[timerHandle].taskHandle == BTHAL_OS_TASK_HANDLE_FM)
            {
                bthal_os_cb.timerParams[timerHandle].timerId = 0xff;
                return TRUE;
            }
            else
            {*/
                bthal_os_cb.timerParams[timerHandle].timerId = 0;
                return TRUE;
            /*}*/
        }
    }

    return TRUE;
}


/*---------------------------------------------------------------------------
 * Send Event
 */
static BOOL Os_SendEvent(BthalOsTaskHandle taskHandle, U32 eventIdx)
{
    switch (taskHandle)
    {
    case BTHAL_OS_TASK_HANDLE_STACK:
        Assert(eventIdx < BTHAL_OS_MAX_NUM_OF_EVENTS_STACK);

        Os_SetEvent(bthal_os_cb.stackParams.notifyEvent[eventIdx]);
        break;

    case BTHAL_OS_TASK_HANDLE_A2DP:
        Assert(eventIdx < BTHAL_OS_MAX_NUM_OF_EVENTS_A2DP);

        Os_SetEvent(bthal_os_cb.a2dpParams.notifyEvent[eventIdx]);
        break;

    default:
        Assert(FALSE);
        break;
    }

    return TRUE;
}


/*---------------------------------------------------------------------------
 * Acquire a mutually exclusive semaphore.
 */
static BOOL Os_SemTake(HANDLE mutex, BthalOsTime timeout)
{
    DWORD result;

    /* Try to acquire the stack mutex object. If we fail, we're most likely
     * running on the window message thread. To prevent deadlocks we must
     * keep processing messages while we re-attempt to acquire the lock.
     */

    result = WaitForSingleObject(mutex, timeout);
    if ((result == WAIT_TIMEOUT) || (result == WAIT_ABANDONED) || (result == WAIT_FAILED))
    {
        return FALSE;
    }

    Assert(result == WAIT_OBJECT_0);

    return TRUE;
}


/*---------------------------------------------------------------------------
 * Release a mutually exclusive semaphore.
 */
static BOOL Os_SemGive(HANDLE mutex)
{
    /* In a typical MINIOS system, this code should re-enable whatever
     * interrupts were disabled by the previous OS_StopHardware call.
     */
    Assert (ReleaseMutex(mutex) == TRUE);

    return TRUE;
}


/*---------------------------------------------------------------------------
 * Send Event 
 */
static BOOL Os_SetEvent(HANDLE hNotifyEvent)
{
    /* We do NOT call EVM_Process directly. We simply event "StackThread"
     * by setting event object, which will cause the StackThread to unblock
     * and run again. If the StackThread was already running, OS_NotifyEvm
     * will keep it from blocking during the next attempt.
     */
    Assert(SetEvent(hNotifyEvent) == TRUE);

    return TRUE;
}


/*---------------------------------------------------------------------------
 * Convert bthal event to event index. 
 */
static U32 Os_ConvertToEventIdx(BthalOsEvent evt)
{
    U32 eventIdx = 0;

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
static BthalOsEvent Os_ConvertToBthalEvent(U32 evtIdx)
{
    BthalOsEvent bthalEvent = 1;

    while (evtIdx > 0)
    {
        bthalEvent <<= 1;
        evtIdx--;
    }

    return bthalEvent;
}




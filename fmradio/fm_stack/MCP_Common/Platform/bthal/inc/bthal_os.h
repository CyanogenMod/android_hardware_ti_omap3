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
*   FILE NAME:      bthal_os.h
*
*   BRIEF:          This file defines the API of the BTHAL OS.
*
*   DESCRIPTION:    General
*
*                   BTHAL OS API layer 
*
*		            Tasks & Events
*		            --------------
*		            Upon task creation, an event callback is registered to the porting layer.
*		            The event callback must be called by the porting layer with an event 
*		            bitmask	in the task context (to which it's associated).
*
*     	            The stack requires several possible tasks:
*
*		            1) The stack task.
*
*		            2) The hardware context can be implemented as an high-priority task.
*
*		            3) A2DP task for processing and encoding.
*
*		            Max number of events is BTHAL_OS_MAX_NUM_OF_EVENTS_XXX per task 
*		            (where XXX is the task name).
*		
*		            For example, max number of events for the stack task is 
*		            BTHAL_OS_MAX_NUM_OF_EVENTS_STACK. If BTHAL_OS_MAX_NUM_OF_EVENTS_STACK = 2, 
*		            the possible events are: 0x00000001 and 0x00000002.
*
*		            Semaphores
*		            ----------
*		            The stack requires several possible semaphores:
*
*		            1) The stack semaphore is required for protecting data structures shared
*		            by stack task & application tasks.
*
*		            2) If the hardware context is implemented as an high-priority task, then
*		            an hardware semaphore is required for protecting data structures shared
*		            by the stack task & the transport task.
*
*		            BTHAL_OS_StopHardware & BTHAL_OS_ResumeHardware are implemented as:
*			        - Semaphore, if the hardware context is an high-priority task.
*			        - Enable/Disable IRQs, if the hardware context is an ISR.
*
*		            3) SPP may need several semaphores.
*
*		            4) A2DP may need several semaphores.
*
*		            Max number of semaphores is BTHAL_OS_MAX_NUM_OF_SEMAPHORES.
*
*
*		            Timers
*		            ------
*		            Timers expired notification is performed via events sent to the event 
*	            	callback (registered during task creation).
*
*		            The stack requires several possible timers:
*
*		            1) The stack task requires a timer.
*
*		            2) A2DP may need a timer.
*
*	    	        Max number of timers is BTHAL_OS_MAX_NUM_OF_TIMERS.
*                   
*   AUTHOR:         Ilan Elias
*
\*******************************************************************************/

#ifndef __BTHAL_OS_H
#define __BTHAL_OS_H


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include <bthal_common.h>
                        

/********************************************************************************
 *
 * Types
 *
 *******************************************************************************/


/*-------------------------------------------------------------------------------
 * BthalOsTaskHandle type
 *
 *     Defines task handle.
 */
typedef BTHAL_U8 BthalOsTaskHandle;

/* Stack task */
#define BTHAL_OS_TASK_HANDLE_STACK								(0x00)

/* The transport task handle is used only if hardware context is implemented 
as an high-priority task */
#define BTHAL_OS_TASK_HANDLE_TRANSPORT							(0x01)

/* A2DP task */
#define BTHAL_OS_TASK_HANDLE_A2DP								(0x02)


/*-------------------------------------------------------------------------------
 * BthalOsEvent type
 *
 *     	Defines event type. Events can be OR'ed together.
 */
typedef BTHAL_U32 BthalOsEvent;


/*-------------------------------------------------------------------------------
 * BthalOsSemaphoreHandle type
 *
 *     Defines semaphore handle.
 */
typedef BTHAL_U8 BthalOsSemaphoreHandle;


/*-------------------------------------------------------------------------------
 * BthalOsTimerHandle type
 *
 *     Defines timer handle.
 */
typedef BTHAL_U8 BthalOsTimerHandle;


/*-------------------------------------------------------------------------------
 * BthalOsTime type
 *
 *     Defines time type.
 */
typedef BTHAL_U32 BthalOsTime;


/*---------------------------------------------------------------------------
 * eventCallback type
 *
 *     	A eventCallback function is provided when creating a task.
 *		This callback will be called when events are received for the task.
 *		The passed argument is a bitmask of the received events.
 */
typedef void (*EventCallback)(BthalOsEvent evtMask);


/********************************************************************************
 *
 * Data Structures
 *
 *******************************************************************************/


/********************************************************************************
 *
 * Function declarations
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BTHAL_OS_Init()
 *
 * Brief:  
 *	    Initializes the operating system layer.
 *
 * Description:
 *	    Initializes the operating system layer.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		callback [in] - BTHAL OS events will be sent to this callback
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *		BT_STATUS_PENDING - Operation started successfully. Completion will 
 *              be signaled via an event to the callback
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_Init(BthalCallBack	callback);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_Deinit()
 *
 * Brief:  
 *	    Deinitializes the operating system layer.
 *
 * Description:
 *	    Deinitializes the operating system layer.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		void.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *		BT_STATUS_PENDING - Operation started successfully. Completion will 
 *          be signaled via an event to the callback
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_Deinit(void);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_CreateTask()
 *
 * Brief:  
 *	    After this function is called, the given task is created.
 *
 * Description:
 *	    After this function is called, the given task is created.
 *		In fact, the task can be created statically at system-startup or 
 *		dynamically when this function is called.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		taskHandle [in] - the task to be created.
 *
 *		callback [in] - this function is called with events bitmask sent to this task.
 *
 *		taskName [in] - null-terminated string representing the task name, 
 *			it can be null.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_CreateTask(BthalOsTaskHandle taskHandle, 
								EventCallback callback, 
								const char *taskName);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_DestroyTask()
 *
 * Brief:  
 *      After this function is called, the given task will not be used anymore.
 *
 * Description:
 *      After this function is called, the given task will not be used anymore.
 *		In fact, the task can be destroyed statically at system-shutdown or 
 *		dynamically when this function is called.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		taskHandle [in] - the task to be destroyed.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_DestroyTask(BthalOsTaskHandle taskHandle);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_SendEvent()
 *
 * Brief:  
 *	    Send the given event to the given task.
 *
 * Description:
 *	    Send the given event to the given task.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		taskHandle [in] - the task which should receive the event.
 *
 *		evt [in] - the event to be sent to the event callback (given in the 
 *			task creation).
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_SendEvent(BthalOsTaskHandle taskHandle, BthalOsEvent evt);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_CreateSemaphore()
 *
 * Brief:  
 *	    Create a semaphore, and returns the handle to that semaphore.
 *
 * Description:  
 *	    Create a semaphore, and returns the handle to that semaphore.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		semaphoreName [in] - null-treminated string representing the semaphore name, 
 *			it can be null.
 *
 *		semaphoreHandle [out] - the handle to this semaphore, which is returned.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_CreateSemaphore(const char *semaphoreName, 
									BthalOsSemaphoreHandle *semaphoreHandle);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_DestroySemaphore()
 *
 * Brief:  
 *	    Destroy a semaphore.
 *
 * Description:  
 *	    Destroy a semaphore.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		semaphoreHandle [in] - the semaphore to be destroyed.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_DestroySemaphore(BthalOsSemaphoreHandle semaphoreHandle);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_LockSemaphore()
 *
 * Brief:  
 *	    Lock a semaphore.
 *
 * Description:  
 *	    Lock a semaphore.
 *		A timeout interval can be provided to indicate the amount of time to try
 *		locking this semaphore.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		semaphoreHandle [in] - the semaphore to be locked.
 *
 *		timeout [in] - timeout interval in milliseconds, in which try to lock 
 *			this semaphore. If timeout is 0, then try infinitely to lock it.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *		BTHAL_STATUS_TIMEOUT - Indicates that the operation failed, because of 
 *			timeout interval elapsed.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.	
 */
BthalStatus BTHAL_OS_LockSemaphore(BthalOsSemaphoreHandle semaphoreHandle,
									BthalOsTime timeout);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_UnlockSemaphore()
 *
 * Brief:  
 *	    Unlock a semaphore.
 *
 * Description:  
 *	    Unlock a semaphore.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		semaphoreHandle [in] - the semaphore to be unlocked.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.	
 */
BthalStatus BTHAL_OS_UnlockSemaphore(BthalOsSemaphoreHandle semaphoreHandle);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_StopHardware()
 *
 * Brief:  
 *      Called by the stack to enter a critical section, during which the 
 *		hardware driver must not execute.
 *
 * Description:    
 *      Called by the stack to enter a critical section, during which the 
 *		hardware driver must not execute.
 *
 *		The stack calls BTHAL_OS_StopHardware just before it is going to read
 *     	or modify data that might be used by the hardware driver.
 *     	Immediately following the data access, the stack calls
 *     	BTHAL_OS_ResumeHardware. In most systems, the time spent in the critical
 *     	section will be less than 50 microseconds.
 *
 *     	In systems where the hardware driver is implemented with a system
 *     	task, the hardware driver task must be at a higher priority and
 *     	run to completion before the stack task can execute. Between
 *     	BTHAL_OS_StopHardware and BTHAL_OS_ResumeHardware, the hardware driver
 *     	task must not be scheduled to execute. For example, this could be
 *     	accomplished by taking a semaphore required by the hardware driver.
 *
 *     	In systems where the hardware driver is implemented entirely in an
 *     	interrupt service routine, BTHAL_OS_StopHardware must prevent stack-
 *     	related hardware interrupts from firing. Other interrupts can
 *     	be left as they are.
 *
 *     	The stack never "nests" calls to BTHAL_OS_StopHardware.
 *     	BTHAL_OS_ResumeHardware is always called before BTHAL_OS_StopHardware
 *     	is called again.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		void.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.	
 */
BthalStatus BTHAL_OS_StopHardware(void);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_ResumeHardware()
 *
 * Brief:  
 *	    Called by the stack to leave the critical section entered by 
 *		BTHAL_OS_StopHardware.
 *
 * Description:    
 *	    Called by the stack to leave the critical section entered by 
 *		BTHAL_OS_StopHardware.
 *
 *     	In systems where the hardware driver is implemented with a system
 *     	task, BTHAL_OS_ResumeHardware allows the hardware driver task to be
 *     	scheduled again. For example, a semaphore could be released
 *     	that allows the hardware driver to run.
 *
 *     	In systems where the hardware driver is implemented entirely by an
 *     	interrupt service routine, BTHAL_OS_ResumeHardware must restore the
 *     	interrupts disabled by BTHAL_OS_StopHardware.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		void.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_ResumeHardware(void);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_CreateTimer()
 *
 * Brief:  
 *	    Create a timer, and returns the handle to that timer.
 *
 * Description:    
 *	    Create a timer, and returns the handle to that timer.
 *		Upon creation, the timer is associated with a task, meaning the timer 
 *		expired event is sent to that task event callback.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		taskHandle [in] - the task which will receive the timer expired event 
 *			(via the event callback given in the task creation).
 *		
 *		timerName [in] - null-terminated string representing the timer name, 
 *			it can be null.
 *
 *		timerHandle [out] - the handle to this timer, which is returned.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_CreateTimer(BthalOsTaskHandle taskHandle, 
								const char *timerName, 
								BthalOsTimerHandle *timerHandle);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_DestroyTimer()
 *
 * Brief:  
 *	    Destroy a timer.
 *
 * Description:    
 *	    Destroy a timer.
 * 
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		timerHandle [in] - the timer to be destroyed.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_DestroyTimer(BthalOsTimerHandle timerHandle);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_ResetTimer()
 *
 * Brief:  
 *	    Reset a timer.
 * 
 * Description:    
 *	    Reset a timer.
 *		If the timer is already active, BTHAL_OS_ResetTimer automatically cancels 
 *		the previous timer as if BTHAL_OS_CancelTimer was called.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		timerHandle [in] - the timer to be reset.
 *
 *		time [in] - number of ticks until the timer fires, after which the given event 
 *			will be sent to the associated task (via the event callback given during 
 *			the task creation).
 *
 *		evt [in] - the event to be sent to the task event callback when the timer expires.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_ResetTimer(BthalOsTimerHandle timerHandle, 
								BthalOsTime time, 
								BthalOsEvent evt);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_CancelTimer()
 *
 * Brief:  
 *	    Cancel a timer.
 *
 * Description:    
 *	    Cancel a timer.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		timerHandle [in] - the timer to be canceled.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_CancelTimer(BthalOsTimerHandle timerHandle);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_Sleep()
 *
 * Brief:  
 *	    Suspends the execution of the current task for the specified interval.
 *
 * Description:  
 *	    Suspends the execution of the current task for the specified interval.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		time [in] - sleep time in milliseconds.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_Sleep(BthalOsTime time);


/*-------------------------------------------------------------------------------
 * BTHAL_OS_GetSystemTime()
 *
 * Brief:  
 *	    Called by the stack to get the current system time in ticks.
 *
 * Description:  
 *	    Called by the stack to get the current system time in ticks.
 *
 *     	The system time provided by this function can start at any value,
 *     	it does not to start at 0. However, the time must "roll over" only
 *     	when reaching the maximum value allowed by BthalOsTime. 
 *		For instance, a 16-bit BthalOsTime must roll over at 0xFFFF. 
 *		A 32-bit BthalOsTime must roll over at 0xFFFFFFFF.
 *
 *     	System ticks may or may not be equivalent to milliseconds. See the
 *     	MS_TO_TICKS macro in config.h (General Configuration Constants)
 *     	for more information.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		time [out] - current system time in ticks.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_OS_GetSystemTime(BthalOsTime *time);


#endif /* __BTHAL_OS_H */



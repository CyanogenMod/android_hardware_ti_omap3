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


#ifndef __PLA_OS_H
#define __PLA_OS_H


#include "mcpf_defs.h"
#include "mcpf_time.h"


/* Init */

/** 
 * \fn     pla_create
 * \brief  Create Platform Abstraction Context
 * 
 * This function creates the Platform Abstraction Context.
 * 
 * \note
 * \return 	Handler to PLA
 * \sa     	pla_create
 */
handle_t 	pla_create ();

/** 
 * \fn     pla_registerClient
 * \brief  Register a client (handler) to the PLA
 * 
 * This function registers a client (handler) to the PLA. 
 * Only one client is allowed.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	hClient - Client's handler.
 * \return 	None
 * \sa     	pla_registerClient
 */
void 		pla_registerClient(handle_t hPla, handle_t hClient);


/* Memory */

/** 
 * \fn     os_malloc
 * \brief  Memory Allocation
 * 
 * This function allocates a memory according to the requested size.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	mem_size - size of buffer to allocate.
 * \return 	Pointer to the allocated memory.
 * \sa     	os_malloc
 */
McpU8 		*os_malloc (handle_t hPla, McpU32 mem_size);

/** 
 * \fn     os_free
 * \brief  Memory Free
 * 
 * This function frees the received memory.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*mem - pointer to the memory to free.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_free
 */
EMcpfRes  	os_free (handle_t hPla, McpU8 *mem);

/** 
 * \fn     os_memory_set
 * \brief  Memory Set
 * 
 * This function sets the given memory with a given value.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*pMemPtr - pointer to the memory to set.
 * \param	Value - value to set.
 * \param	Length - number of bytes to set.
 * \return 	None
 * \sa     	os_memory_set
 */
void 		os_memory_set (handle_t hPla, void *pMemPtr, McpS32 Value, McpU32 Length);

/** 
 * \fn     os_memory_zero
 * \brief  Set memory to zero
 * 
 * This function sets the given memory to zero.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*pMemPtr - pointer to the memory to set.
 * \param	Length - number of bytes to set.
 * \return 	None
 * \sa     	os_memory_zero
 */
void 		os_memory_zero (handle_t hPla, void *pMemPtr, McpU32 Length);

/** 
 * \fn     os_memory_copy
 * \brief  Copy Memory
 * 
 * This function copies given memory size from src to dest.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*pDestination - pointer to the memory to copy to.
 * \param	*pSource - pointer to the memory to copy from.
 * \param	uSize - number of bytes to copy.
 * \return 	None
 * \sa     	os_memory_copy
 */
void 		os_memory_copy (handle_t hPla, void *pDestination, void *pSource, McpU32 uSize);


/* Time */

/** 
 * \fn     os_timer_start
 * \brief  Starts an OS timer
 * 
 * This function starts an OS timer. It should supply it's own cb function, 
 * and should store the received cb & handler with the returned timer ID.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	cb - timer's callback function
 * \param	hCaller - callback's handler
 * \param	expiry_time - timer's expiry time
 * \return 	Timer ID
 * \sa     	os_timer_start
 */
McpUint  	os_timer_start (handle_t hPla, mcpf_timer_cb cb, handle_t hCaller, McpU32 expiry_time); 

/** 
 * \fn     os_timer_stop
 * \brief  Stops the received timer
 * 
 * This function stops the received timer 
 * and clears its related stored parameters.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	timer_id - The ID of the timer to stop.
 * \param	*pCritSecHandle - Handler to the created critical section object.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_timer_stop
 */
McpBool  	os_timer_stop (handle_t hPla, McpUint timer_id);

/** 
 * \fn     os_get_current_time_inSec
 * \brief  Get time in seconds
 * 
 * This function returns time in seconds since 1.1.1970.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \return 	time in seconds since 1.1.1970
 * \sa     	os_get_current_time_inSec
 */
McpU32  	os_get_current_time_inSec (handle_t hPla);

/** 
 * \fn     os_get_current_time_inMilliSec
 * \brief  Get time in milliseconds
 * 
 * This function returns system time in milliseconds.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \return 	system time in milliseconds
 * \sa     	os_get_current_time_inMilliSec
 */
McpU32  	os_get_current_time_inMilliSec (handle_t hPla);


/** 
 * \fn     os_get_performance_counter
 * \brief  Read performance counter
 * 
 * This function returns the contents of performance couter (ticks)
 * 
 * \note
 * \return 	current performance counter
 * \sa     	os_get_performance_counter_tick_nsec
 */
McpU32 os_get_performance_counter (void);

/** 
 * \fn     os_get_performance_tick_nsec
 * \brief  Get performance counter resolution
 * 
 * This function returns the tick duration of performance counter in nano seconds
 * 
 * \note
 * \return 	tick duration of performance counter in nano seconds units
 * \sa     	os_get_performance_counter
 */
McpU32 os_get_performance_tick_nsec (void);


/* Critical Section */

/** 
 * \fn     os_cs_create
 * \brief  Create critical section object
 * 
 * This function creates a critical section object.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \return 	Handler to object
 * \sa     	os_cs_create
 */
/* void  		*os_cs_create (handle_t hPla); */ /* Taken from mcp_hal_os */

/** 
 * \fn     os_cs_enter
 * \brief  Enter critical section
 * 
 * This function enters the critical section.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*cs - Critical section object handler.
 * \param	timeout - timeout to wait on CS.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_cs_enter
 */
/* EMcpfRes    os_cs_enter (handle_t hPla, void *cs, McpUint timeout); */ /* Taken from mcp_hal_os */

/** 
 * \fn     os_cs_exit
 * \brief  Exit critical section
 * 
 * This function exits the critical section.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*cs - Critical section object handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_cs_exit
 */
/* EMcpfRes    os_cs_exit (handle_t hPla, void *cs); */ /* Taken from mcp_hal_os */

/** 
 * \fn     os_cs_destroy
 * \brief  Destroy critical section object
 * 
 * This function destroys the critical section object.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*cs - Critical section object handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_cs_destroy
 */
/* EMcpfRes    os_cs_destroy (handle_t hPla, void *cs); */ /* Taken from mcp_hal_os */


/* Signaling Object */

/** 
 * \fn     os_sigobj_create
 * \brief  Create signaling object
 * 
 * This function creates a signaling object.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \return 	Handler to object
 * \sa     	os_sigobj_create
 */
void  		*os_sigobj_create (handle_t hPla);

/** 
 * \fn     os_sigobj_wait
 * \brief  Wait on the signaling object
 * 
 * This function waits on the signaling object.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*evt - signaling object handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_sigobj_wait
 */
EMcpfRes    os_sigobj_wait (handle_t hPla, void *evt);	

/** 
 * \fn     os_sigobj_set
 * \brief  Set the signaling object
 * 
 * This function sets the signaling object.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*evt - signaling object handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_sigobj_set
 */
EMcpfRes    os_sigobj_set (handle_t hPla, void *evt);

/** 
 * \fn     os_sigobj_destroy
 * \brief  Destroy the signaling object
 * 
 * This function destroys the signaling object.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*evt - signaling object handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_sigobj_destroy
 */
EMcpfRes    os_sigobj_destroy (handle_t hPla, void *evt);


/* Files */

/** 
 * \fn     os_file_open
 * \brief  Open a file
 * 
 * This function opens a file.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*filename - name of file to open.
 * \param	*flags - flags indicating how to open the file.
 * \return 	File's handler
 * \sa     	os_file_open
 */
/* void  		*os_file_open (handle_t hPla, McpS8 *filename, McpU8 *flags); */ /* Taken from mcp_hal_fs */

/** 
 * \fn     os_file_close
 * \brief  Close a file
 * 
 * This function closes the given file.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*fp - File's handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_file_close
 */
/* EMcpfRes  	os_file_close (handle_t hPla, void *fp); */ /* Taken from mcp_hal_fs */

/** 
 * \fn     os_file_read
 * \brief  Read from the file
 * 
 * This function reads from the file the given 
 * number of bytes, into the given buffer.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*fp - File's handler.
 * \param	*buff - Destination buffer.
 * \param	num_bytes - Number of bytes to read.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_file_read
 */
/* EMcpfRes    os_file_read (handle_t hPla, void *fp, McpU8 *buff, McpU16 num_bytes); */ /* Taken from mcp_hal_fs */

/** 
 * \fn     os_file_write
 * \brief  Write to the file
 * 
 * This function writes to the file the given 
 * number of bytes, from the given buffer.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*fp - File's handler.
 * \param	*buff - Source buffer.
 * \param	num_bytes - Number of bytes to write.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_file_write
 */
/* EMcpfRes    os_file_write (handle_t hPla, void *fp, McpU8 *buff, McpU16 num_bytes); */ /* Taken from mcp_hal_fs */

/** 
 * \fn     os_file_seek
 * \brief  Seek the file for a specific position
 * 
 * This function seeks the file for a specific position, 
 * and sets the pointer to this position.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*fp - File's handler.
 * \param	iOffset - The offset to seek.
 * \param	uOrigin - The location to start seeking from.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_file_seek
 */
/* EMcpfRes    os_file_seek (handle_t hPla, void *fp, McpS32 iOffset, McpU16 uOrigin); */ /* Taken from mcp_hal_fs */

/** 
 * \fn     os_file_tell
 * \brief  Get the current position of the file's pointer
 * 
 * This function gets the current position of the file's pointer.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	*fp - File's handler.
 * \return 	Position of file's pointer
 * \sa     	os_file_tell
 */
/* McpS32		os_file_tell (handle_t hPla, void *fp); */ /* Taken from mcp_hal_fs */


/* Messaging */

/** 
 * \fn     os_send_message
 * \brief  Send a message to another process
 * 
 * This function sends a given message to the destination process. 
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	dest - Index of destination process/application.
 * \param	*msg - The message to send.
 * \return 	Result of operation: OK or ERROR
 * \sa     	os_send_message
 */
EMcpfRes    os_send_message (handle_t hPla, McpU32 dest, TmcpfMsg *msg);	


/* Threads */

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
 * \param	*lpThreadAttributes - Thread's attributes. 
 * \param	uStackSize - Stack Size.
 * \param	lpStartAddress - Pointer to the thread's procedure.
 * \param	*lpParameter - Parameter of the thred's procedure. 
 * \param	uCreationFlags - Thread's creation flags.
 * \param	*puThreadId - pointer to the created thread id.
 * \return 	Thread's Handler
 * \sa     	os_createThread
 */
handle_t	os_createThread (void *lpThreadAttributes, McpU32 uStackSize, mcpf_TaskProcedure lpStartAddress, 
							 void *lpParameter, McpU32 uCreationFlags, McpU32 *puThreadId);

/** 
 * \fn     os_exitThread
 * \brief  Exit a running thread
 * 
 * This function exits a running thread.
 * 
 * \note
 * \param	uExitCode - Exit Code. 
 * \return 	None
 * \sa     	os_exitThread
 */
void		os_exitThread (McpU32 uExitCode);

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
EMcpfRes		os_SetTaskPriority (handle_t hTaskHandle, McpInt sPriority);


/* Print */

/** 
 * \fn     os_print
 * \brief  Print String
 * 
 * This function prints the string to the default output.
 * 
 * \note
 * \param	format - String's format.
 * \param	...		- String's arguments.
 * \return 	None
 * \sa     	os_print
 */
void		os_print (const char *format ,...);

#endif

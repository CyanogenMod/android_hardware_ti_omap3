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
*   FILE NAME:      bthal_uart.h
*
*   BRIEF:          This file defines the API of the BTHAL UART hardware driver.
*
*   DESCRIPTION:    General
*
*                   BTHAL UART Driver API layer
*
*     	            The UART hardware driver is a set of functions used by the UART
*     	            transport to exchange data with Bluetooth radio hardware. If you
*     	            are porting the stack to a new hardware platform, you must provide
*     	            the UART functions described here.
*
*     	            If your system will require an interrupt service routine (ISR) to
*     	            handle UART receive and transmit events, we recommend that you create
*     	            a high-priority driver thread. When an event occurs, the ISR should
*     	            simply signal the thread, then exit. This minimizes the time spent
*     	            in interrupt mode. The driver thread then reads or writes data and
*     	            indicates events as described below.
*
*     	            If it is not possible to create a separate, high-priority driver
*     	            thread, the interrupt may issue the required callback events itself.
*     	            BTHAL_UART_Read and BTHAL_UART_Write must then be designed to operate 
*	   	            in interrupt mode (BTHAL_UART_Write is also called from the stack task).
*
*     	            Some operating systems take care of the ISR internally, and provide
*     	            an API to read and write UART data. In this case, the high-priority
*     	            driver thread receives its signals from the operating system instead of
*     	            a custom ISR.
*
*		            Because data structures in the transport driver are modified both by 
*		            the stack task and the hardware context (which may be an interrupt or 
*		            a task, depending on its design), there is a possibility that data may 
*		            become corrupted because both contexts may attempt to access the data 
*		            concurrently.
*		            As a result, OS_StopHardware and OS_ResumeHardware must surround any 
*		            access to data structures that may be used by both the hardware context 
*		            and the stack task. Because these functions prevent the hardware 
*		            thread/interrupt from executing, the time spent between OS_StopHardware 
*	            	and OS_ResumeHardware should be kept to a minimum. 
*		            Furthermore, the transport layer must never call an HCI API layer 
*		            function while in OS_StopHardware mode.
*
*     	            If a high-priority thread is used, OS_StopHardware and OS_ResumeHardware
*     	            can be implemented using a mutual-exclusion semaphore. 
*	   	            If only an ISR is used, then OS_StopHardware and OS_ResumeHardware 
*	   	            should disable/re-enable UART-related interrupts.
*
*		            At any time a write is not pending, the hardware driver should revert to 
*		            “read mode” in which it is sensitive to incoming bytes from the UART. 
*		            UART reads are handled entirely within the context of the hardware driver’s 
*		            task or interrupt.
*
*		            It is very important that callback events are provided in the correct order. 
*		            For instance, if data has been written to the transport and data is 
*		            subsequently received through the transport, this should be reflected in
*		            the order of callbacks to the UART Transport Driver: first send a 
*		            UE_WRITE_COMPLETE event, then a UE_DATA_TO_READ event.
*                   
*   AUTHOR:         Ilan Elias
*
\*******************************************************************************/

#ifndef __BTHAL_UART_H
#define __BTHAL_UART_H


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

/*------------------------------------------------------------------------------
 * UartEvent type
 *
 *     UART events are provided to the stack through the UartCallback.
 */
typedef BTHAL_U8 UartEvent;

/* Indicates that data is available and can be read with BTHAL_UART_Read. */
#define UE_DATA_TO_READ          1

/* Indicates that the data transmission requested by BTHAL_UART_Write is complete. */
#define UE_WRITE_COMPLETE        2

/* 
 * Indicates that the UART has completed initialization. Note that the
 * hardware resource (see OS_StopHardware) should not be allocated
 * by the UART hardware driver when delivering this callback. 
 */
#define UE_INIT_COMPLETE         3

/* 
 * Indicates that the UART has completed shutdown. Note that the
 * hardware resource (see OS_StopHardware) should not be allocated
 * by the UART hardware driver when delivering this callback. 
 */
#define UE_SHUTDOWN_COMPLETE     4

/* End of UartEvent */


/*---------------------------------------------------------------------------
 * UartCallback type
 *
 *     A UartCallback function is provided to the UART hardware driver.
 *     When data is available to be read, the UART driver must call the
 *     callback function with an event of
 *     UE_DATA_TO_READ. The stack will immediately call UART_Read
 *     repeatedly until all data has been read.
 *
 *     The UART must also call the callback function with UE_WRITE_COMPLETE
 *     when all data written with UART_Write has been transmitted.
 */
typedef void (*UartCallback)(UartEvent event);

/* End of UartCallback */


/*-------------------------------------------------------------------------------
 * BthalUartSpeed type
 *
 *     Defines speed (in KBits/sec) at which the UART should operate.
 */
typedef BTHAL_U16 BthalUartSpeed;

#define BTHAL_UART_SPEED_9600									(9)
#define BTHAL_UART_SPEED_38400									(38)
#define BTHAL_UART_SPEED_57600									(57)
#define BTHAL_UART_SPEED_115200									(115)
#define BTHAL_UART_SPEED_128000									(128)
#define BTHAL_UART_SPEED_230400									(230)
#define BTHAL_UART_SPEED_256000									(256)
#define BTHAL_UART_SPEED_460800									(460)
#define BTHAL_UART_SPEED_921600									(921)
#define BTHAL_UART_SPEED_1843200								(1834)
#define BTHAL_UART_SPEED_3686400									(3686)


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
 * BTHAL_UART_Init()
 *
 * Brief:  
 *	    Called by the stack (during ME_RadioInit) to initialize the UART hardware
 *      driver. 
 *
 * Description:
 *	    Called by the stack (during ME_RadioInit) to initialize the UART hardware
 *      driver. This function should perform the following tasks:
 *
 *      1) Initialize all local memory.
 *
 *      2) Open and initialize the communications port using OS-specific
 *      calls.
 *
 *      3) Initialize a thread or interrupt service routine to process
 *      communications port events.
 *
 *      4) Register a callback function. When a read or write event occurs, 
 *      the UART hardware driver calls the callback function with the 
 *      appropriate event.
 *
 *		Initialization can occur synchronously or asynchronously. 
 *		Returning BTHAL_STATUS_SUCCESS from BTHAL_UART_Init informs the 
 *		transport layer that initialization is complete. 
 *		BTHAL_STATUS_FAILED indicates that initialization failed. 
 *		BTHAL_STATUS_PENDING indicates that initialization will happen 
 *		asynchronously and that an event (UE_INIT_COMPLETE) will be passed 
 *		to the transport layer when initialization is complete.
 *
 * Type:
 *		Asynchronous/Synchronous
 *
 * Parameters:
 *		func [in] - Callback function to call with events.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that initialization was successful.
 *
 *     	BTHAL_STATUS_PENDING - Indicates that initialization was started and
 *          a notification will be sent when initialization is complete with
 *			the event UE_INIT_COMPLETE.
 *
 *     	BTHAL_STATUS_FAILED - Indicate that the port could not be opened
 *          or that some other initialization error occurred.
 */
BthalStatus BTHAL_UART_Init(UartCallback func);


/*-------------------------------------------------------------------------------
 * BTHAL_UART_Shutdown()
 *
 * Brief:  
 *      Called by the stack (during shutdown) to release any resources
 *     	allocated by the BTHAL_UART_Init function. 
 *
 * Description:
 *	    Called by the stack (during shutdown) to release any resources
 *     	allocated by the BTHAL_UART_Init function. This may include closing
 *     	the communications port, killing the I/O thread, deregistering
 *     	the interrupt service routine, etc.
 *
 *		Shutdown of the transport will occur when ME_RadioShutdown is called. 
 *		Shutdown can occur synchronously or asynchronously like initialization. 
 *		The return codes are the same and, if it occurs asynchronously, an event
 *		(UE_SHUTDOWN_COMPLETE) must be passed to the transport layer upon completion. 
 *		During shutdown, the following tasks should be performed:
 *
 *		1) Free (closing) the hardware port.
 *
 *		2) Deregister the interrupt service routine or terminate the driver task.
 *
 * Type:
 *		Asynchronous/Synchronous
 *
 * Parameters:
 *		void.
 *
 * Generated Events:
 *      UE_SHUTDOWN_COMPLETE
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that shutdown was successful.
 *
 *     	BTHAL_STATUS_PENDING - Indicates that shutdown was started and
 *         	a notification will be sent when shutdown is complete with
 *			the event UE_SHUTDOWN_COMPLETE.
 *
 *     	BTHAL_STATUS_FAILED - Indicate that the port could not be closed
 *         	or that some other error occurred.
 */
BthalStatus BTHAL_UART_Shutdown(void);


/*-------------------------------------------------------------------------------
 * BTHAL_UART_Read()
 *
 * Brief:  
 *	    Called by the stack to read bytes from the UART.
 *
 * Description:
 *	    Called by the stack to read bytes from the UART.
 *
 *     	When the UART driver first detects that data has been received, it
 *     	notifies the stack by calling the callback with the
 *     	UE_DATA_TO_READ event. During processing of this callback, the
 *     	stack calls BTHAL_UART_Read repeatedly until all available data has
 *     	been read.
 *
 *     	This function must not block. If no bytes are currently available,
 *     	BTHAL_UART_Read returns 0.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		buffer [in] - Buffer to fill with received bytes.
 *
 *		length [in] - Maximum number of bytes to read.
 *
 *		readBytes [out] - The number of bytes actually copied into "buffer". 
 *			If no more bytes are available, BTHAL_UART_Read returns 0.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the reading was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicate that the reading could not be performed
 *         	or that some other error occurred.
 */
BthalStatus BTHAL_UART_Read(BTHAL_U8 *buffer, 
							BTHAL_U16 length, 
							BTHAL_U16 *readBytes);


/*-------------------------------------------------------------------------------
 * BTHAL_UART_Write()
 *
 * Brief:  
 *	    Called by the stack to begin writing bytes to the UART.
 *
 * Description:
 *	    Called by the stack to begin writing bytes to the UART.
 *     	This function must not block, but return immediately after
 *     	initializing the write.
 *
 *     	This function is first called by the stack thread. When the UART
 *     	driver determines that the caller's buffer is free, it calls the
 *     	callback function with the UE_WRITE_COMPLETE event. 
 *		During processing of the event, BTHAL_UART_Write may be called once to 
 *		provide more transmit data to the hardware driver.
 *
 * Type:
 *		Asynchronous
 *
 * Parameters:
 *		buffer [in] - Bytes to write.
 *
 *		length [in] - Number of bytes in "buffer".
 *
 *		writtenBytes [out] - The actual number of bytes sent to the UART. 
 *			Returns 0 if a transport error occurred.
 *
 * Generated Events:
 *      UE_WRITE_COMPLETE
 *
 * Returns:
 *		BTHAL_STATUS_PENDING - Indicates that the writing was started and
 *         	a notification will be sent when shutdown is complete with
 *			the event UE_WRITE_COMPLETE.
 *
 *     	BTHAL_STATUS_FAILED - Indicate that the writing could not be performed
 *         	or that some other error occurred.
 */
BthalStatus BTHAL_UART_Write(const BTHAL_U8 *buffer, 
								BTHAL_U16 length, 
								BTHAL_U16 *writtenBytes);


/*-------------------------------------------------------------------------------
 * BTHAL_UART_SetSpeed()
 *
 * Brief:  
 *	    Called to change the speed of the UART.
 *
 * Description:
 *	    Called to change the speed of the UART.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		speed [in] - Speed (in KBits/sec) at which the UART should operate.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that setting the speed was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicate that setting the speed could not be 
 *			performed or that some other error occurred.
 */
BthalStatus BTHAL_UART_SetSpeed(BthalUartSpeed speed);


/*-------------------------------------------------------------------------------
 * BTHAL_UART_ResetRxFifo()
 *
 * Brief:  
 *	    Called to clear the UART RX FIFO.
 *		This function is only used if power management is activated.
 *
 * Description:
 *	    Called to clear the UART RX FIFO.
 *		This function is only used if power management is activated.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		none.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that resetting RX FIFO was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicate that resetting RX FIFO could not be 
 *			performed or that some other error occurred.
 */
BthalStatus BTHAL_UART_ResetRxFifo(void);


#endif /* __BTHAL_UART_H */



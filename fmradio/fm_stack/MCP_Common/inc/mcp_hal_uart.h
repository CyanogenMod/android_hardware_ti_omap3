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



/** \file   mcp_hal_uart.h 
 *  \brief  Hardware/OS Adaptation Layer for UART Interface API-s
 * 
 * 
 * The HAL UART is OS and Bus dependent module.  
 *
 *  \see    mcp_hal_uart.c
 */

#ifndef __MCP_HALUART_API_H__
#define __MCP_HALUART_API_H__



/************************************************************************
 * Defines
 ************************************************************************/

/* UART Baudratge */

typedef McpU16 ThalUartSpeed;

#define HAL_UART_SPEED_1200	  		1
#define HAL_UART_SPEED_2400	  		2
#define HAL_UART_SPEED_4800	  		4
#define HAL_UART_SPEED_9600	  		9
#define HAL_UART_SPEED_14400	  		14
#define HAL_UART_SPEED_19200	  		19
#define HAL_UART_SPEED_38400  		38
#define HAL_UART_SPEED_57600  		57
#define HAL_UART_SPEED_115200 		115
#define HAL_UART_SPEED_128000 		128
#define HAL_UART_SPEED_230400 		230
#define HAL_UART_SPEED_256000 		256
#define HAL_UART_SPEED_460800 		460
#define HAL_UART_SPEED_921600 		921
#define HAL_UART_SPEED_1000000		1000
#define HAL_UART_SPEED_1843200		1834
#define HAL_UART_SPEED_2000000		2000
#define HAL_UART_SPEED_3000000		3000
#define HAL_UART_SPEED_3686400		3686


/************************************************************************
 * Macros
 ************************************************************************/


/************************************************************************
 * Types
 ************************************************************************/

/************************************************************************
 * Functions
 ************************************************************************/

/** 
 * \fn     HAL_UART_Create 
 * \brief  Create the HAL UART object
 * 
 * Allocate and clear the module's object
 * 
 * \note
 * \param  	hMcpf - Handle to OS Framework
 * \return 	Handle of the allocated object, NULL if allocation failed 
 * \sa     	HAL_UART_Destroy
 */ 
handle_t HAL_UART_Create (const handle_t hMcpf, const char *pDevName);

/** 
 * \fn     HAL_UART_Destroy 
 * \brief  Destroy the HAL UART object
 * 
 * Destroy and free the Interface Sleep Manager object.
 * 
 * \note
 * \param	hHalUart - handle to HAL UART object
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_UART_Create
 */ 
EMcpfRes HAL_UART_Destroy (handle_t	hHalUart);


/** 
 * \fn     HAL_UART_Init
 * \brief  HAL UART object initialization
 * 
 * Initiate state of HAL UART object
 * 
 * \note
 * \param	hHalUart  - handle to HAL UART object
 * \param	fEventHandlerCb - upper layer event handler callback
 * \param	hHandleCb - callback parameter
 * \param	pBuf  	  - pointer to Rx buffer to start data reception
 * \param	pConf  	  - UART port configuration
 * \param	pBuf	  - receive buffer
 * \param	len		  - buffer length
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_UART_Create
 */ 
EMcpfRes HAL_UART_Init (handle_t 					hHalUart, 
						 const TI_TxnEventHandlerCb fEventHandlerCb, 
						 const handle_t 			hHandleCb,  
						 const TBusDrvCfg 	   		*pConf,
						 McpU8		   				*pBuf,
						 const McpU16				len);

/** 
 * \fn     HAL_UART_Deinit
 * \brief  HAL UART object de-initialization
 * 
 * De-initiate HAL UART object
 * 
 * \note
 * \param	hHalUart  - handle to HAL UART object
 * \sa     	HAL_UART_Init
 */ 
EMcpfRes HAL_UART_Deinit (handle_t hHalUart);

/** 
 * \fn     HAL_UART_Write
 * \brief  Wrtite data to OS UART device
 * 
 *  Send data of specified length over OS UART port
 * 
 * \note
 * \param	hHalUart - handle to HAL UART object
 * \param	pBuf 	 - data buffer to write
 * \param	len 	 - lenght of data buffer
 * \param	sentLen	 - lenght of data actually written
 * \return 	Returns the status of operation: COMPLETE, PENDING or Error
 * \sa     	HAL_UART_Read
 */ 
ETxnStatus HAL_UART_Write (const handle_t 	hHalUart, 
						   const McpU8		*pBuf, 
						   const McpU16		len,  
						   McpU16			*sentLen);


/** 
 * \fn     HAL_UART_Read
 * \brief  Read data from OS UART device
 * 
 *  Receive data of specified length from OS UART port
 * 
 * \note
 * \param	hHalUart - handle to HAL UART object
 * \param	pBuf 	 - data buffer to read
 * \param	len 	 - lenght of data buffer
 * \param	sentLen	 - lenght of data actually read
 * \return 	Returns the status of operation: COMPLETE, PENDING or Error
 * \sa     	HAL_UART_Write, HAL_UART_ReadResult
 */ 
ETxnStatus HAL_UART_Read (const handle_t	hHalUart, 
						  McpU8				*pBuf, 
						  const McpU16		len,  
						  McpU16			*readLen);


/** 
 * \fn     HAL_UART_ReadResult
 * \brief  Return the number of received bytes
 * 
 *  Return the read status and the number of received bytes
 * 
 * \note
 * \param	hHalUart - handle to HAL UART object
 * \param	pLen	 - pointer to return the value of received length
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_UART_Read
 */ 
EMcpfRes HAL_UART_ReadResult (const handle_t  hHalUart, McpU16 *pLen);


/** 
 * \fn     HAL_UART_RestartRead
 * \brief  Signal to start read operation
 * 
 *  Send signal to start read operation
 * 
 * \note
 * \param	hHalUart - handle to HAL UART object
 * \param	pLen	 - pointer to return the value of received length
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_UART_Read
 */ 
EMcpfRes HAL_UART_RestartRead (const handle_t  hHalUart);


/** 
 * \fn     HAL_UART_ResetRead
 * \brief  Reset and start data receiption
 * 
 *  Cleans Rx buffer and starts a new read operation
 * 
 * \note
 * \param	hHalUart - handle to HAL UART object
 * \param	pBuf	 - pointer to buffer to start a new receiption
 * \param	len	 	 - number of bytes to read
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_UART_Read
 */ 
EMcpfRes HAL_UART_ResetRead (const handle_t  hHalUart, McpU8 *pBuf, const McpU16 len);

/** 
 * \fn     HAL_UART_Init
 * \brief  Set HAL UART object new configuration
 * 
 * Set or change the configuration of HAL UART object
 * 
 * \note
 * \param	hHalUart  - handle to HAL UART object
 * \param	pConf  	  - UART port configuration
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_UART_Init
 */ 
EMcpfRes HAL_UART_Set (handle_t 			hHalUart, 
					    const TBusDrvCfg 	*pConf);


/**
 * \fn     HAL_UART_Port_Reset
 * \brief  reset and optionally start HAL UART  hardware to
 * 
 * 
 */ 
EMcpfRes HAL_UART_Port_Reset (handle_t  *hHalUart, McpU8 *pBuf, McpU16 len, const McpU16 baudrate);
void resetDataPtr(handle_t hHalUart, McpU8 *pBuf);
#endif /*__MCP_HALUART_API_H__*/

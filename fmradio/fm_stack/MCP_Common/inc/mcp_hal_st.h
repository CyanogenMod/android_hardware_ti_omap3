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



/** \file   mcp_hal_st.h 
 *  \brief  Hardware/OS Adaptation Layer for Shared Transport Interface API-s
 * 
 * 
 * The HAL ST is OS and Bus dependent module.  
 *
 *  \see    mcp_hal_st.c
 */

#ifndef __MCP_HALST_API_H__
#define __MCP_HALST_API_H__



/************************************************************************
 * Defines
 ************************************************************************/

/* IF Baudrate */

typedef McpU16 ThalStSpeed;

#define HAL_ST_SPEED_1200	  		            1
#define HAL_ST_SPEED_2400	  		            2
#define HAL_ST_SPEED_4800	  		            4
#define HAL_ST_SPEED_9600	  		            9
#define HAL_ST_SPEED_14400	  	                14
#define HAL_ST_SPEED_19200	  	                19
#define HAL_ST_SPEED_38400  		            38
#define HAL_ST_SPEED_57600  		            57
#define HAL_ST_SPEED_115200 		            115
#define HAL_ST_SPEED_128000 		            128
#define HAL_ST_SPEED_230400 		            230
#define HAL_ST_SPEED_256000 		            256
#define HAL_ST_SPEED_460800 		            460
#define HAL_ST_SPEED_921600 		            921
#define HAL_ST_SPEED_1000000		            1000
#define HAL_ST_SPEED_1843200		            1834
#define HAL_ST_SPEED_2000000		            2000
#define HAL_ST_SPEED_3000000		            3000
#define HAL_ST_SPEED_3686400		            3686

#define HAL_ST_CHIP_VERSION_FILE_NAME           "/sys/uim/version"


/************************************************************************
 * Enums
 ************************************************************************/

/* HAL ST Events */
typedef enum
{
    HalStEvent_WriteComplInd,
    HalStEvent_ReadReadylInd,
    HalStEvent_Terminate,
    HalStEvent_AwakeRxThread,
    HalStEvent_Error,
    HalStEvent_MaxNum

} EHalStEvent;


/************************************************************************
 * Types
 ************************************************************************/

typedef void (*HalST_EventHandlerCb)(handle_t hHandleCb, EHalStEvent eEvent);

typedef struct
{
    McpU32    uBaudRate;
    McpU32    uFlowCtrl;
	McpU16    XonLimit;
	McpU16    XoffLimit;
	McpU16    portNum;

} THalSt_PortCfg;
 
typedef struct
{
    McpU16    full;         /* Full 16-bits version */
    McpU16    projectType;  /* Project type (7 for 1271, 10 for 1283) */
    McpU16    major;        /* Version major */
    McpU16    minor;        /* Version minor */
    
} THalSt_ChipVersion; 

/************************************************************************
 * Functions
 ************************************************************************/

/** 
 * \fn     HAL_ST_Create 
 * \brief  Create the HAL ST object
 * 
 * Allocate and clear the module's object
 * 
 * \note
 * \param  	hMcpf - Handle to OS Framework
 * \return 	Handle of the allocated object, NULL if allocation failed 
 * \sa     	HAL_ST_Destroy
 */ 
handle_t HAL_ST_Create (const handle_t hMcpf, const char *pModuleName);

/** 
 * \fn     HAL_ST_Destroy 
 * \brief  Destroy the HAL ST object
 * 
 * Destroy and free the Interface object.
 * 
 * \note
 * \param	hHalSt - handle to HAL ST object
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_ST_Create
 */ 
EMcpfRes HAL_ST_Destroy (handle_t	hHalSt);


/** 
 * \fn     HAL_ST_Init
 * \brief  HAL ST object initialization
 * 
 * Initiate state of HAL ST object and opens the IF
 * 
 * \note
 * \param	hHalSt  - handle to HAL ST object
 * \param	fEventHandlerCb 	- upper layer event handler callback
 * \param	hHandleCb 		- callback parameter
 * \param	pConf  	  		- port configuration
 * \param	pDevName		- The name of the device to open
 * \param	pBuf	  			- pointer to Rx buffer to start data reception
 * \param	len		  		- buffer length
 * \param	bIsBlockOnWrite	- MCP_TRUE/MCP_FALSE
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_ST_Create
 */ 
EMcpfRes HAL_ST_Init (handle_t 					hHalSt, 
						 const HalST_EventHandlerCb fEventHandlerCb, 
						 const handle_t 			hHandleCb,  
						 const THalSt_PortCfg   		*pConf,
						 const char				*pDevName,
						 McpU8		   			*pBuf,
						 const McpU16				len,
						 McpBool					bIsBlockOnWrite);

/** 
 * \fn     HAL_ST_Deinit
 * \brief  HAL ST object de-initialization
 * 
 * De-initiate HAL ST object and closes the IF
 * 
 * \note
 * \param	hHalSt  - handle to HAL ST object
 * \sa     	HAL_ST_Init
 */ 
EMcpfRes HAL_ST_Deinit (handle_t hHalSt);

/** 
 * \fn     HAL_ST_Write
 * \brief  Wrtite data to OS ST device
 * 
 *  Send data of specified length over OS ST port
 * 
 * \note
 * \param	hHalSt 	- handle to HAL ST object
 * \param	pBuf 	- data buffer to write
 * \param	len 	 	- lenght of data buffer
 * \param	sentLen	- lenght of data actually written
 * \return 	Returns the status of operation: COMPLETE, PENDING or Error
 * \sa     	HAL_ST_Read
 */ 
EMcpfRes HAL_ST_Write (const handle_t 	hHalSt, 
						   const McpU8		*pBuf, 
						   const McpU16		len,  
						   McpU16			*sentLen);


/** 
 * \fn     HAL_ST_Read
 * \brief  Read data from OS ST device
 * 
 *  Receive data of specified length from OS ST port
 * 
 * \note
 * \param	hHalSt 	- handle to HAL ST object
 * \param	pBuf 	- data buffer to read
 * \param	len 	 	- lenght of data buffer
 * \param	sentLen	- lenght of data actually read
 * \return 	Returns the status of operation: COMPLETE, PENDING or Error
 * \sa     	HAL_ST_Write, HAL_ST_ReadResult
 */ 
EMcpfRes HAL_ST_Read (const handle_t	hHalSt, 
						  McpU8				*pBuf, 
						  const McpU16		len,  
						  McpU16			*readLen);


/** 
 * \fn     HAL_ST_ReadResult
 * \brief  Return the number of received bytes
 * 
 *  Return the read status and the number of received bytes
 * 
 * \note
 * \param	hHalSt 	- handle to HAL ST object
 * \param	pLen	 	- pointer to return the value of received length
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_ST_Read
 */ 
EMcpfRes HAL_ST_ReadResult (const handle_t  hHalSt, McpU16 *pLen);


/** 
 * \fn     HAL_ST_RestartRead
 * \brief  Signal to start read operation
 * 
 *  Send signal to start read operation
 * 
 * \note
 * \param	hHalSt 	- handle to HAL ST object
 * \param	pLen	 	- pointer to return the value of received length
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_ST_Read
 */ 
EMcpfRes HAL_ST_RestartRead (const handle_t  hHalSt);


/** 
 * \fn     HAL_ST_ResetRead
 * \brief  Reset and start data receiption
 * 
 *  Cleans Rx buffer and starts a new read operation
 * 
 * \note
 * \param	hHalSt 	- handle to HAL ST object
 * \param	pBuf	 	- pointer to buffer to start a new receiption
 * \param	len	 	- number of bytes to read
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_ST_Read
 */ 
EMcpfRes HAL_ST_ResetRead (const handle_t  hHalSt, McpU8 *pBuf, const McpU16 len);

/** 
 * \fn     HAL_ST_Set
 * \brief  Set HAL ST object new configuration
 * 
 * Set or change the configuration of HAL ST object
 * 
 * \note
 * \param	hHalSt  	- handle to HAL ST object
 * \param	pConf  	- ST port configuration
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_ST_Set
 */ 
EMcpfRes HAL_ST_Set (handle_t 			hHalSt, 
					    const THalSt_PortCfg 	*pConf);

/** 
 * \fn     HAL_ST_RegisterVSEvent
 * \brief  Register to receive a VS event (or DEFAULT for all events)
 * 
 * Register to receive a VS event (or DEFAULT for all events)
 * NOTE: THIS IOCTL IS ONLY SUPPORTED BY THE TI_BT_DRV
 * 
 * \note
 * \param	hHalSt  		- handle to HAL ST object
 * \param	uSVEvent		- VS Event to register (0 - Default)
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_ST_RegisterVSEvent
 */ 
EMcpfRes HAL_ST_RegisterVSEvent (handle_t 			hHalSt, 
					    				    McpU32			uVSEvent);

/** 
 * \fn     HAL_ST_SetWriteBlockMode
 * \brief  Enable/Disable Write Blocking Mode
 * 
 * Enable/Disable Write Blocking Mode. 
 * If Enabled, sending a command will block 
 * the process until a command complete is received.
 * If disabed, command complete will be received in the Rx context.
 * NOTE: THIS IOCTL IS ONLY SUPPORTED BY THE TI_BT_DRV
 * 
 * \note
 * \param	hHalSt  		- handle to HAL ST object
 * \param	bIsWriteBlock 	- MCP_TRUE/MCP_FALSE
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_ST_SetWriteBlockMode
 */ 
EMcpfRes HAL_ST_SetWriteBlockMode (handle_t 			hHalSt, 
					    					  McpBool				bIsWriteBlock);

/**
 * \fn     HAL_ST_Port_Reset
 * \brief  reset and optionally start HAL ST  hardware to
 * 
 * 
 */ 
EMcpfRes HAL_ST_Port_Reset (handle_t  *hHalSt, McpU8 *pBuf, McpU16 len, const McpU16 baudrate);


/** 
 * \fn     HAL_ST_GetChipVersion
 * \brief  Get chip's version
 * 
 * Get chip's version from the file which was written by the ST driver in the
 * kernel: project type, version major, and version minor.
 * 
 * \note
 * \param	hHalSt  		- handle to HAL ST object
 * \param	fileName  		- filename to read version info from
 * \param	chipVersion 	- pointer to struct for returning chip version
 * \return 	Returns the status of operation: OK or Error
 * \sa     	HAL_ST_Init
 */ 
EMcpfRes HAL_ST_GetChipVersion (handle_t hHalSt,
                                THalSt_ChipVersion *chipVersion);


void resetDataPtr(handle_t hHalSt, McpU8 *pBuf);
#endif /*__MCP_HALST_API_H__*/

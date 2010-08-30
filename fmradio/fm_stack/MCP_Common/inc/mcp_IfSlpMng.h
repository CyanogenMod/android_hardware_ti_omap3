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



/** \file   mcp_IfSlpMng.h 
 *  \brief  Interface Sleep Manager module
 * 
 *  Interface Sleep Manager module provides:
 * - sleep (power management) protocol state machine handling
 * - creation/deletion of sleep manager object 
 * 
 * The Inteerface Sleep Manager module is OS and Bus independent module.  
 *
 *  \see    mcp_IfSlpMng.c
 */

#ifndef __IFSLPMNG_API_H__
#define __IFSLPMNG_API_H__


/************************************************************************
 * Defines
 ************************************************************************/


/************************************************************************
 * Macros
 ************************************************************************/


/************************************************************************
 * Types
 ************************************************************************/

/*  return value type */
typedef enum
{
    IfSlpMng_OK,
	IfSlpMng_SLP_PKT_TYPE,
	IfSlpMng_ERR

} EIfSlpMngStatus;

/* Interface Sleep Manager States */
typedef enum
{
	IFSLPMNG_STATE_ASLEEP = 1,
	IFSLPMNG_STATE_WAIT_FOR_AWAKE_ACK,
	IFSLPMNG_STATE_AWAKE,
	IFSLPMNG_STATE_WAIT_FOR_SLEEPACK_TX_COMPL,
	IFSLPMNG_STATE_MAX

} EIfSlpMngState;

/* Statistics counters */
typedef struct
{
	McpU32       AwakeInd_Tx;          
	McpU32       AwakeAck_Tx;         
	McpU32       SleepAck_Tx;         
	McpU32       SleepAckTxCompl;         
	McpU32       AwakeInd_Rx;          
	McpU32       AwakeAck_Rx;         
	McpU32       SleepInd_Rx;         
	McpU32       SleepAck_Rx;         
	McpU32       Error;         

} TIfSlpMngStat;

/************************************************************************
 * Functions
 ************************************************************************/

/** 
 * \fn     ifSlpMng_Create 
 * \brief  Create the Interface Sleep Manager object
 * 
 * Allocate and clear the module's object
 * 
 * \note
 * \param  	hMcpf - Handle to Os framework
 * \return 	Handle of the allocated object, NULL if allocation failed 
 * \sa     	ifSlpMng_Destroy
 */ 
handle_t ifSlpMng_Create (const handle_t hMcpf);

/** 
 * \fn     ifSlpMng_Destroy 
 * \brief  Destroy the Interface Sleep Manager object
 * 
 * Destroy and free the Interface Sleep Manager object.
 * 
 * \note
 * \param	hIfSlpMng - handle to Interface Sleep Manager object
 * \return 	Returns the status of operation: OK or Error
 * \sa     	ifSlpMng_Create
 */ 
EMcpfRes ifSlpMng_Destroy (handle_t	hIfSlpMng);


/** 
 * \fn     ifSlpMng_Init
 * \brief  Interface Sleep Manager object initialization
 * 
 * Initiate state of Interface Sleep Manager object
 * 
 * \note
 * \param	hIfSlpMng - Interface Sleep Manager object handle
 * \param	hTxnQ	  - transaction queue handle 
 * \param	hBusDrv	  - bus driver handle 
 * \param	uFuncId	  - function ID
 * \param	tChipId   - PM chip ID
 * \param	tTransId  - PM transport ID
 * \return 	Returns the status of operation: OK or Error
 * \sa     	ifSlpMng_Create
 */ 
EMcpfRes ifSlpMng_Init (handle_t 			 hIfSlpMng, 
						  const handle_t 	 hTxnQ,
						  const handle_t 	 hBusDrv,
						  const McpU32 		 uFuncId,
						  const McpHalChipId tChipId, 
						  const McpHalTranId tTransId);


/** 
 * \fn     ifSlpMng_HandleTxEvent
 * \brief  Interface Sleep Manager state machine for Tx event
 * 
 * Process Tx event of Interface Sleep Manager state machine
 * 
 * \note
 * \param	hIfSlpMng - handle to Interface Sleep Manager object
 * \return 	Returns the status of operation: OK or Error
 * \sa     	ifSlpMng_Init
 */ 
EMcpfRes ifSlpMng_HandleTxEvent (handle_t hIfSlpMng);


/** 
 * \fn     ifSlpMng_HandleRxEvent
 * \brief  Interface Sleep Manager state machine for Rx event
 * 
 * Process Rx event of Interface Sleep Manager state machine
 * 
 * \note
 * \param	hIfSlpMng - handle to Interface Sleep Manager object
 * \param	pTxn 	  - pointer to transport transaction
 * \return 	Returns the status of operation of type EIfSlpMngStatus:
 * 			IfSlpMng_OK - successful processing of normal packet
 *          IfSlpMng_SLP_PKT_TYPE - successful processing of IfSlpMng packet type
 *          IfSlpMng_ERR - operation error
 * \sa     	IfSlpMng_Init
 */ 
EIfSlpMngStatus ifSlpMng_HandleRxEvent (handle_t hIfSlpMng, TTxnStruct *pTxn);


/** 
 * \fn     ifSlpMng_handleRxReady
 * \brief  Interface Sleep Manager state machine for Rx ready event
 * 
 * Process receive ready event of Interface Sleep Manager state machine.
 * The Rx message (any bytes) received in sleep state are to be treated as 
 * awake indication. The message can be "gargbage" as local UART clock 
 * is not appliedin a sleep state.
 * 
 * \note
 * \param	hIfSlpMng - handle to Interface Sleep Manager object
 * \return 	Returns the status of operation of type EMcpfRes:
 * 			E_COMPLETED  - power management state is not SLEEP and Rx reset is not required
 * 			RES_PENDING - power management state is SLEEP and Rx reset is required
 * \sa     	IfSlpMng_Init
 */ 
EMcpfRes ifSlpMng_handleRxReady(handle_t hIfSlpMng); 


/** 
 * \fn     ifSlpMng_HandleTxComplEvent
 * \brief  Interface Sleep Manager state machine for Tx complete event
 * 
 * Process transaction sending complete event of Interface Sleep Manager state machine
 * 
 * \note
 * \param	hIfSlpMng - handle to Interface Sleep Manager object
 * \return 	Returns the status of operation: OK or Error
 * \sa     	ifSlpMng_Init
 */ 
EMcpfRes ifSlpMng_HandleTxComplEvent (handle_t hIfSlpMng);

/** 
 * \fn     ifSlpMng_GetStat
 * \brief  Gets Interface Sleep Management statistics
 * 
 * Get IfSlpMng statistics counters and state
 * 
 * \note
 * \param	hIfSlpMng - handle to Interface Sleep Manager object
 * \param	pStat 	- pointer to statistics structure to return counters
 * \param	pState  - pointer to state variable to return the current IfSlpMng state
 * \return 	void
 * \sa     	ifSlpMng_Init
 */ 
void ifSlpMng_GetStat (const handle_t hIfSlpMng, TIfSlpMngStat  * pStat, EIfSlpMngState * pState);

/** 
 * \fn     ifSlpMng_Reinit
 * \brief  reset slpmanager state
 * 
 * Called  on reset - must reset the sleep manager SM and
 * transaction queue.
 * 
 * \note   
 * \param  hIfSlpMng      - The module's object
 * \return RES_OK / RES_ERROR
 * \sa
 */ 
EMcpfRes ifSlpMng_Reinit (handle_t 		 	 hIfSlpMng);

#endif /*__IFSLPMNG_API_H__*/

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



/** \file   mcp_transport.h 
 *  \brief  Shared Transport module API definition                                  
 * 
 *  Shared Transport Layer provides:
 * - common functions for message transmitting and reception, 
 * - transaction queue and bus driver creation, initialization and deletion, 
 * - register/unregister upper layer to receive packets by channel number,
 * - invoking of Interface Sleep Management protocol state machine for
 *   bus awake/sleep power management in Tx and Rx paths,
 * - forwarding of upper layer transaction to transaction queue,
 * - receiving of Rx indication from bus driver and forwarding received packet to registered 
 *   upper layer application according to the channel number the packet was received from.
 * 
 * The transport layer reports initialization/destroy completion or error events by invoking of 
 * event callback function. The callback parameters are valid in the called context only. 
 * If the processing of the event is performed asynchronously in the different context, 
 * the event structure (fields) is to be saved in the buffer provided by upper layer.
 * 
 * The Transport is OS and Bus independent module.  
 * 
 *
 *  \see    mcp_transport.c
 */

#ifndef __TRANSPORT_API_H__
#define __TRANSPORT_API_H__

#include "mcp_IfSlpMng.h"

/************************************************************************
 * Defines
 ************************************************************************/

/* Transaction priorities */
#define TRANS_PRIO_HIGH          0
#define TRANS_PRIO_MEDIUM        1
#define TRANS_PRIO_LOW           2


/************************************************************************
 * Macros
 ************************************************************************/


/************************************************************************
 * Types
 ************************************************************************/

/* Application call back for Rx indication from Transport Layer */

typedef EMcpfRes (*TI_TransRxIndCb)(handle_t hTrans, handle_t hCbParam, TTxnStruct *pTxn);

typedef McpU8 TI_TransChan;
typedef McpU8 TI_TransPrio;


/* Transport Layer configuration structure */
typedef struct  _TTransConfig {

	TI_TransChan 		 maxChanNum;  	/* Maximum number of channels			*/
	TI_TransPrio  		 maxPrioNum;  	/* Maximum number of priorities			*/
	McpHalChipId 		 tChipId;      	/* Chip CCM id for power management 	*/
	McpHalTranId 		 tTransId;     	/* Transport layer CCM id for power management 	*/
	TI_TxnEventHandlerCb fEventCb; 		/* pointer to upper layer function to 
										indicate initialization, destroy and error  	*/
	handle_t			 hHandleCb;	 	/* Callback parameter					*/

} TTransConfig;

/* Statistics counters */
typedef struct
{

	McpU32       Tx;          
	McpU32       TxCompl;         
	McpU32       RxInd;               
	McpU32       BufAvail;               
	McpU32       Error;               

} TTransStat;

/* Statistics counters */
typedef struct
{
	TTransStat      transStat;          
	TIfSlpMngStat 	ifSlpMngStat;
	EIfSlpMngState  ifSlpMngState;
	TBusDrvStat		busDrvStat;
	EBusDrvRxState	busDrvState;

} TTransAllStat;


/************************************************************************
 * Functions
 ************************************************************************/

/** 
 * \fn     trans_Create 
 * \brief  Create the Transport Layer module
 * 
 * Allocate and clear the module's object, initialize Transaction queue and
 * Interface Sleep Management object
 * 
 * \note
 * \param	pTransConf - pointer to transport configuration structure
 * \param  	hMcpf - handle to Os framework
 * \return 	Handle of the allocated object, NULL if allocation failed 
 * \sa     	trans_Destroy
 */ 
handle_t trans_Create (const TTransConfig *	pTransConf, 
					   const handle_t 		hMcpf);

/** 
 * \fn     trans_Destroy 
 * \brief  Destroy bus driver and free the module's object
 * 
 * Destroy transaction module and free the module's object.
 * 
 * \note
 * \param	hTrans - handle to transport layer object
 * \return 	Returns the status of operation: OK or Error
 * \sa     	trans_Create
 */ 
EMcpfRes trans_Destroy (handle_t	hTrans);


/** 
 * \fn     trans_Init 
 * \brief  Transport Layer initialization
 * 
 * Intializes transport layer module, configures transaction queue and bus driver
 * 
 * \note
 * \param	hTrans - handle to transport layer object
 * \param	pBusDrvConf  - transport bus driver configuration
 * \return 	Returns the status of operation: OK or Error
 * \sa     	trans_Create
 */ 
EMcpfRes trans_Init (handle_t	hTrans, const TBusDrvCfg * pBusDrvConf);


/** 
 * \fn     trans_ChOpen
 * \brief  Transport Layer channel open
 * 
 * Open transport channel and subscribe for reception of packets
 * 
 * \note
 * \param	hTrans   - handle to transport layer object
 * \param	chanNum  - channel number handle to transport layer object
 * \param	fRxIndCb - RxInd function callback to path received data
 * \param	hCbParam - parameter to be passed while invoking RxInd callback 
 * \return 	Returns the status of operation: OK or Error
 * \sa     	trans_ChClose
 */ 
EMcpfRes trans_ChOpen (handle_t 				hTrans, 
						const TI_TransChan 		chanNum, 
						const TI_TransRxIndCb 	fRxIndCb, 
						const handle_t 			hCbParam);


/** 
 * \fn     trans_ChClose
 * \brief  Transport Layer channel close
 * 
 *  Close transport channel and un-subscribe from reception of packets
 * 
 * \note
 * \param	hTrans - handle to transport layer object
 * \param	chanNum - channel number handle to transport layer object
 * \return 	Returns the status of operation: OK or Error
 * \sa     	trans_ChOpen
 */ 
EMcpfRes trans_ChClose (handle_t hTrans, const TI_TransChan chanNum);


/** 
 * \fn     trans_TxData
 * \brief  Transport Layer transmit data function
 * 
 *  Transmit data specified by transaction structure over Transport
 * 
 * \note
 * \param	hTrans 	- handle to transport layer object
 * \param   pTxn    - pointer to transaction structure with data to send
 * \return 	Returns the status of operation: OK or Error
 * \sa      trans_ChOpen
 */ 
EMcpfRes trans_TxData (const handle_t hTrans, TTxnStruct *pTxn);


/** 
 * \fn     trans_BufAvailable
 * \brief  Rx buffer is available 
 * 
 *  Indication that Rx buffer of upper layer is available to receive data from Transport Layer,
 * used to exit from receiver congestion state
 * 
 * \note
 * \param	hTrans 	- handle to transport layer object
 * \return 	Returns the status of operation: OK or Error
 * \sa     	trans_ChOpen
 */ 
EMcpfRes trans_BufAvailable (const handle_t hTrans);

/** 
 * \fn     trans_SetSpeed
 * \brief  sets bus driver speed
 * 
 *  Sets bus driver speed
 * 
 * \note
 * \param	hTrans 	- handle to transport layer object
 * \param	speed 	- speed (baudrate) value
 * \return 	operation result
 * \sa
 */ 
EMcpfRes trans_SetSpeed(const handle_t hTrans, const McpU16 speed);

/** 
 * \fn     trans_GetStat
 * \brief  Gets transport layers statistics
 * 
 *  Return transport, bus driver  and hal layers statistic counters
 * 
 * \note
 * \param	hTrans 	- handle to transport layer object
 * \param	pStat 	- pointer to statistics structure to return counters
 * \return 	void
 * \sa
 */ 
void trans_GetStat (const handle_t hTrans, TTransAllStat  * pStat);

#ifdef TRAN_DBG
void trans_testTx (const handle_t hTrans, TTxnStruct *pTxn);
#endif

EMcpfRes trans_Reinit (handle_t   hTrans, const McpU16 baudrate);

#endif /*__TRANSPORT_API_H__*/

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


/** \file   TxnQueue.h 
 *  \brief  TxnQueue module API definition                                  
 *
 *  \see    TxnQueue.c
 */

#ifndef __TXN_QUEUE_API_H__
#define __TXN_QUEUE_API_H__


#include "TxnDefs.h"
#include "BusDrv.h"



/************************************************************************
 * Defines
 ************************************************************************/


/************************************************************************
 * Macros
 ************************************************************************/


/************************************************************************
 * Types
 ************************************************************************/


/************************************************************************
 * Functions
 ************************************************************************/
handle_t   	txnQ_Create (const handle_t hMcpf);

EMcpfRes  txnQ_Destroy (handle_t hTxnQ);

void        txnQ_Init (handle_t hTxnQ, const handle_t hMcpf);

EMcpfRes  txnQ_ConnectBus (handle_t 				hTxnQ, 
							 const TBusDrvCfg 		*pBusDrvCfg,
							 const TTxnDoneCb 		fConnectCb,
							 const handle_t 		hConnectCb, 
							 const TI_BusDvrRxIndCb fRxIndCb,
							 const handle_t 		hRxIndHandle, 
							 const TI_TxnEventHandlerCb fEventHandlerCb,
							 const handle_t 		hEventHandle,
							 const handle_t 		hIfSlpMng);

EMcpfRes  txnQ_DisconnectBus (handle_t hTxnQ);

EMcpfRes  txnQ_Open (handle_t       		hTxnQ, 
                       const McpU32       	uFuncId, 
                       const McpU32       	uNumPrios, 
                       const TTxnQueueDoneCb fTxnQueueDoneCb,
                       const handle_t       hCbHandle);

void        txnQ_Close (handle_t  hTxnQ, const McpU32 uFuncId);

ETxnStatus  txnQ_Restart (handle_t hTxnQ, const McpU32 uFuncId);

void        txnQ_Run (handle_t hTxnQ, const McpU32 uFuncId);

void        txnQ_Stop (handle_t hTxnQ, const McpU32 uFuncId);

ETxnStatus  txnQ_Transact (handle_t hTxnQ, TTxnStruct *pTxn);

handle_t	txnQ_GetBusDrvHandle (const handle_t hTxnQ);

McpBool 	txnQ_IsQueueEmpty (const handle_t hTxnQ, const McpU32 uFuncId);

EMcpfRes    txnQ_ResetBus (handle_t hTxnQ, const McpU16 baudrate);

#ifdef TRAN_DBG
    void txnQ_PrintQueues (handle_t hTxnQ);
	void txnQ_testTx (handle_t hTxnQ, TTxnStruct *pTxn);
#endif



#endif /*__TXN_QUEUE_API_H__*/

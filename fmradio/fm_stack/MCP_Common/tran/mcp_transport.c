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


/** \file   mcp_transport.c 
 *  \brief  Shared Transport Layer implementation. 
 * 
 *  \see    mcp_transport.h, TxnQueue.h
 */

#include "mcpf_mem.h"
#include "mcpf_services.h"
#include "mcpf_report.h"
#include "mcp_hal_os.h"
#include "TxnDefs.h"
#include "TxnQueue.h"
#include "BusDrv.h"
#include "mcp_txnpool.h"
#include "mcp_IfSlpMng.h"
#include "mcp_transport.h"


/************************************************************************
 * Defines
 ************************************************************************/

#define TRANS_TXNQ_FUN_ID	0

/************************************************************************
 * Types
 ************************************************************************/

/* Transport Channel */
typedef struct
{
	TI_TransRxIndCb   	fRxIndCb;	/* upper layer Rx indication callback function  */
	handle_t       		hCbHandle;	/* upper layer Rx indication callback parameter */

} TTransChanSt;


/* Transport Layer Object */
typedef struct
{
	/* module handles */
	handle_t       hMcpf;                        

	handle_t       hTxnQ;			 /* Transaction queue handle 		*/
	handle_t       hIfSlpMng;	 	 /* Interface Sleep Manager handle 	*/
	handle_t       hBusDrv;			 /* Bus driver handle 				*/

	TI_TxnEventHandlerCb fEventCb;
	handle_t       hHandleCb;	 	 /* Callback parameter */
	TI_TransChan   maxChanNum;  	 /* Maximum channel number 			    */
	TI_TransPrio   maxPrioNum;  	 /* Maximum priorities 				    */
	TTransChanSt   *pTransChanTable;/* pointer to Transport Channel Table   */
	McpHalChipId   tChipId;      	 /* PM chip id for power management 	*/
	McpHalTranId   tTransId;      	 /* PM transport layer id for power management */

	TTransStat 	   stat;            /* Statistics */

} TTransObj;


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/

/* Transport layer Rx indication called by lower layer (Bus Driver) */
static ETxnStatus transRxInd (handle_t hTrans, TTxnStruct *pTxn);

/* Transport layer Tx complete indication called by Txn Queue */
static void transTxComplInd (const handle_t hTrans, TTxnStruct *pTxn, McpBool bInExtContxt);

static void transDestroy(TTransObj  *pTrans);


/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/** 
 * \fn     trans_Create 
 * \brief  Create the Transport Layer module
 * 
 */
handle_t trans_Create (const TTransConfig 	*pTransConf, 
					   const handle_t     	hMcpf)
{
	TTransObj  * pTrans;

	pTrans = mcpf_mem_alloc (hMcpf, sizeof(TTransObj));
	if (pTrans == NULL)
		return NULL;
	mcpf_mem_zero (hMcpf, pTrans, sizeof(TTransObj));

	pTrans->hMcpf 	   = hMcpf;
	pTrans->maxChanNum = pTransConf->maxChanNum;
	pTrans->maxPrioNum = pTransConf->maxPrioNum;
	pTrans->fEventCb   = pTransConf->fEventCb;
	pTrans->hHandleCb  = pTransConf->hHandleCb;
	pTrans->tChipId    = pTransConf->tChipId;
	pTrans->tTransId   = pTransConf->tTransId;

	pTrans->pTransChanTable = mcpf_mem_alloc (hMcpf,(McpU16) (sizeof(TTransChanSt) * pTrans->maxChanNum));
	if (pTrans->pTransChanTable == NULL)
	{
		transDestroy (pTrans);
		return NULL;
	}
	mcpf_mem_zero (hMcpf, pTrans->pTransChanTable, sizeof(TTransChanSt) * pTrans->maxChanNum);

	pTrans->hTxnQ = txnQ_Create (hMcpf);
	if (pTrans->hTxnQ == NULL)
	{
		transDestroy (pTrans);
		return NULL;
	}

	pTrans->hBusDrv = txnQ_GetBusDrvHandle (pTrans->hTxnQ);

	pTrans->hIfSlpMng = ifSlpMng_Create (hMcpf);
  
	if (pTrans->hIfSlpMng == NULL)
	{
		transDestroy (pTrans);
		return NULL;
	}
	return(handle_t) pTrans;
}


/** 
 * \fn     trans_Destroy 
 * \brief  Destroy bus driver and free the module's object
 * 
 */
EMcpfRes trans_Destroy (handle_t hTrans)
{
	TTransObj *pTrans = (TTransObj *) hTrans;
	TTxnEvent  tEvent;


	if (pTrans)
	{
		if (pTrans->hIfSlpMng)
			ifSlpMng_Destroy(pTrans->hIfSlpMng);
		
		if (pTrans->hTxnQ)
		{
			txnQ_DisconnectBus (pTrans->hTxnQ);
			txnQ_Close (pTrans->hTxnQ, TRANS_TXNQ_FUN_ID);
			txnQ_Destroy (pTrans->hTxnQ);
		}
		if (pTrans->pTransChanTable )
		{
			mcpf_mem_free (pTrans->hMcpf, pTrans->pTransChanTable);
		}

		/* report destroy completion to the upper layer */
		tEvent.eEventType = Txn_DestroyComplInd;
		pTrans->fEventCb (pTrans->hHandleCb, &tEvent);

		mcpf_mem_free (pTrans->hMcpf, pTrans);
	}
	return RES_OK;
}

/** 
 * \fn     trans_Init 
 * \brief  Transport Layer initialization
 * 
 */
EMcpfRes trans_Init (handle_t hTrans, const TBusDrvCfg * pBusDrvConf)
{
	TTransObj *pTrans = (TTransObj *) hTrans;
	EMcpfRes  res;
	TTxnEvent  tEvent;


	txnQ_Init (pTrans->hTxnQ, pTrans->hMcpf);

	res = txnQ_Open (pTrans->hTxnQ, 
					 TXN_FUNC_ID_CTRL, 
					 pTrans->maxPrioNum, 
					 (TTxnQueueDoneCb) transTxComplInd, 
					 hTrans);

	ifSlpMng_Init (pTrans->hIfSlpMng, 
				   pTrans->hTxnQ, 
				   pTrans->hBusDrv, 
				   TRANS_TXNQ_FUN_ID, 
				   pTrans->tChipId, 
				   pTrans->tTransId);

	if (res != RES_OK)
	{
		return RES_ERROR;
	}

	res = txnQ_ConnectBus (pTrans->hTxnQ, 
						   pBusDrvConf, 
						   NULL,		/* connect bus callback is not used			*/
						   NULL,		/* connect bus callback param, is not used	*/
						   (TI_BusDvrRxIndCb) transRxInd, hTrans,
						   pTrans->fEventCb, pTrans->hHandleCb, 
						   pTrans->hIfSlpMng );
	if (res != RES_OK)
	{
		return RES_ERROR;
	}

	/* report initialization completion to upper layer */
	tEvent.eEventType = Txn_InitComplInd;
	pTrans->fEventCb (pTrans->hHandleCb, &tEvent);

	return RES_OK;
}

/** 
 * \fn     trans_Reinit
 * \brief  reset transport
 * 
 * Called on reset - must reset the baudrate.
 * 
 * \note   
 * \param  hTrans      - The module's object
 * \param  baudrate - the baudrate to set configuration.
 * \return RES_OK / RES_ERROR
 * \sa
 */ 
EMcpfRes trans_Reinit (handle_t   hTrans, const McpU16 baudrate)
{
    TTransObj *pTrans = (TTransObj *) hTrans;
    EMcpfRes status;
    
    if ((status = ifSlpMng_Reinit (pTrans->hIfSlpMng)) == RES_OK)
    {
        status =  txnQ_ResetBus (pTrans->hTxnQ, baudrate);
    }

    return status;
}
/** 
 * \fn     trans_ChOpen
 * \brief  Transport Layer channel open
 * 
 */
EMcpfRes trans_ChOpen (handle_t     			hTrans, 
						const TI_TransChan    	chanNum, 
						const TI_TransRxIndCb 	fRxIndCb, 
						const handle_t     		hCbParam)
{
	TTransObj *pTrans = (TTransObj *) hTrans;

	if (chanNum >= pTrans->maxChanNum)
	{
		return RES_ERROR;
	}

	if (pTrans->pTransChanTable[chanNum].fRxIndCb)
	{
		MCPF_REPORT_WARNING (pTrans->hMcpf, TRANS_MODULE_LOG, 
							 ("%s: channel is overwritten, chan=%u\n", __FUNCTION__, chanNum));	
	}

	MCPF_ENTER_CRIT_SEC (pTrans->hMcpf);

	pTrans->pTransChanTable[chanNum].fRxIndCb  = fRxIndCb;
	pTrans->pTransChanTable[chanNum].hCbHandle = hCbParam;

	MCPF_EXIT_CRIT_SEC (pTrans->hMcpf);

	return RES_OK;
}



/** 
 * \fn     trans_ChClose
 * \brief  Transport Layer channel close
 * 
 */
EMcpfRes trans_ChClose (handle_t hTrans, const TI_TransChan chanNum)
{
	TTransObj *pTrans = (TTransObj *) hTrans;

	if (chanNum >= pTrans->maxChanNum)
	{
		return RES_ERROR;
	}

	MCPF_ENTER_CRIT_SEC (pTrans->hMcpf);

	pTrans->pTransChanTable[chanNum].fRxIndCb  = NULL;
	pTrans->pTransChanTable[chanNum].hCbHandle = NULL;

	MCPF_EXIT_CRIT_SEC (pTrans->hMcpf);

	return RES_OK;
}


/** 
 * \fn     trans_TxData
 * \brief  Transport Layer transmit data function
 * 
 */
EMcpfRes trans_TxData (const handle_t hTrans, TTxnStruct *pTxn)
{
	TTransObj *pTrans = (TTransObj *) hTrans;
	ETxnStatus eStatus;

	ifSlpMng_HandleTxEvent (pTrans->hIfSlpMng);

	/* Send transaction to TxnQ */
	eStatus = txnQ_Transact (pTrans->hTxnQ, pTxn);
	if (eStatus == TXN_STATUS_ERROR)
	{
		pTrans->stat.Error++;
		return RES_ERROR;
	}
	pTrans->stat.Tx++;
	return RES_OK;
}

/** 
 * \fn     trans_BufAvailable
 * \brief  Rx buffer is available 
 * 
 */
EMcpfRes trans_BufAvailable (const handle_t hTrans)
{
	TTransObj *pTrans = (TTransObj *) hTrans;

	busDrv_BufAvailable (pTrans->hBusDrv);
	pTrans->stat.BufAvail++;
	return RES_OK;
}

/** 
 * \fn     trans_SetSpeed
 * \brief  sets bus driver speed
 * 
 */

EMcpfRes trans_SetSpeed(const handle_t hTrans, const McpU16 speed)
{
	TTransObj *pTrans = (TTransObj *) hTrans;
	
	busDrv_SetSpeed(pTrans->hBusDrv, speed);
	return RES_OK;
}

/** 
 * \fn     trans_GetStat
 * \brief  Gets transport layers statistics
 * 
 */ 
void trans_GetStat (const handle_t hTrans, TTransAllStat  * pStat)
{
	TTransObj *pTrans = (TTransObj *) hTrans;

	MCPF_ENTER_CRIT_SEC (pTrans->hMcpf);

	busDrv_GetStat (pTrans->hBusDrv, &pStat->busDrvStat, &pStat->busDrvState);
	ifSlpMng_GetStat ( pTrans->hIfSlpMng, &pStat->ifSlpMngStat, &pStat->ifSlpMngState);
	mcpf_mem_copy (pTrans->hMcpf, &pStat->transStat, &pTrans->stat, sizeof(pStat->transStat));

	MCPF_EXIT_CRIT_SEC (pTrans->hMcpf);
}



/************************************************************************
 * Module Private Functions
 ************************************************************************/

/** 
 * \fn     transTxComplInd 
 * \brief  transmit complete indication
 * 
 * Process transmit complete from Transaction Queue module
 * 
 * \note
 * \param	hTrans - pointer to transport object
 * \param  	pTxn - pointer to transaction structure
 * \param	bInExtContxt - external context (true)
 * \return 	void
 * \sa     	trans_Init
 */ 
static void transTxComplInd (const handle_t hTrans, TTxnStruct *pTxn, McpBool bInExtContxt)
{
	TTransObj *pTrans = (TTransObj *) hTrans;

	MCPF_UNUSED_PARAMETER(bInExtContxt);

	/* Check whether transaciont is single step and relates to Interface Sleep Manager */
	if (!(TXN_PARAM_GET_SINGLE_STEP (pTxn)))
	{
		/* Completion of normal send transaction, invoke transaction callback */
		if (pTxn->fTxnDoneCb != NULL)
		{
			pTxn->fTxnDoneCb (pTxn->hCbHandle, pTxn);
		}
	} 
	else
	{
		/* Transaction is single step, internal transaction of Interface Sleep Manager */
		ifSlpMng_HandleTxComplEvent (pTrans->hIfSlpMng); 
	}
	pTrans->stat.TxCompl++;
}


/** 
 * \fn     transRxInd 
 * \brief  Receive indication
 * 
 * Process Rx indication called by lower layer (Bus Driver)
 * 
 * \note
 * \param	hCbHandle - pointer to transport object
 * \param  	pTxn - pointer to transaction structure
 * \return 	void
 * \sa     	trans_Init
 */ 
static ETxnStatus transRxInd (const handle_t hTrans, TTxnStruct *pTxn)
{
	TTransObj  	   *pTrans = (TTransObj *) hTrans;
	TI_TransChan   	tChanNum;
	handle_t    	hCbHandle;
	TI_TransRxIndCb   fRxIndCb;
	EIfSlpMngStatus eRes;
	EMcpfRes		status=RES_ERROR;

	eRes = ifSlpMng_HandleRxEvent (pTrans->hIfSlpMng, pTxn);
	if (eRes != IfSlpMng_SLP_PKT_TYPE)
	{
		/*  The message is normal, does not relate to Interface Sleep protocol,
		 *  fetch Rx callback from transport channel table and forward Rx to upper layer
		 */
		tChanNum = (TI_TransChan) TXN_PARAM_GET_CHAN_NUM (pTxn);
		if (tChanNum < pTrans->maxChanNum)
		{
			MCPF_ENTER_CRIT_SEC (pTrans->hMcpf);
			fRxIndCb  = pTrans->pTransChanTable[tChanNum].fRxIndCb;
			hCbHandle = pTrans->pTransChanTable[tChanNum].hCbHandle;
			MCPF_EXIT_CRIT_SEC (pTrans->hMcpf);

			if (fRxIndCb)
			{
			   status = fRxIndCb( hTrans, hCbHandle, pTxn);
			}
			else
			{
				MCPF_REPORT_ERROR (pTrans->hMcpf, TRANS_MODULE_LOG, 
								  ("%s: Rx for closed channel=%u\n", __FUNCTION__, tChanNum));
				pTrans->stat.Error++;
			}
		}
		else
		{
			/* Invalid channel number, release txn and buf */
			mcpf_txnPool_FreeBuf (pTrans->hMcpf, pTxn);
			MCPF_REPORT_ERROR (pTrans->hMcpf, TRANS_MODULE_LOG, 
							  ("%s: Invalid channel=%u\n", __FUNCTION__, tChanNum));
			pTrans->stat.Error++;
		}
	}
	else
	{
		/* Receivied message is Interface Sleep Manager packet, release it */
		mcpf_txnPool_FreeBuf (pTrans->hMcpf, pTxn);
		status=RES_OK;
	}
	pTrans->stat.RxInd++;
	return status;
}

/** 
 * \fn     transDestroy 
 * \brief  Destroy transport object
 * 
 * Frees memory occupied by transport object
 * 
 * \note
 * \param	pTrans - pointer to transport object
 * \return 	void
 * \sa     	trans_Init
 */ 
static void transDestroy(TTransObj  *pTrans)
{
	if (pTrans->hTxnQ)
	{
		txnQ_Destroy (pTrans->hTxnQ);
	}
	if (pTrans->pTransChanTable)
	{
		mcpf_mem_free (pTrans->hMcpf, pTrans->pTransChanTable);
	}
	mcpf_mem_free (pTrans->hMcpf, pTrans);
}


#ifdef TRAN_DBG

void trans_testTx (const handle_t hTrans, TTxnStruct *pTxn)
{
	TTransObj  *pTrans = (TTransObj *) hTrans;

	txnQ_testTx ( pTrans->hTxnQ, pTxn);
}

#endif



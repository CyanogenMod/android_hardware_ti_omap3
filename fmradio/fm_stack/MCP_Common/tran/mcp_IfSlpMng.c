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


/** \file   mcp_IfSlpMng.c 
 *  \brief  Interface Sleep Manager implementation. 
 * 
 *  \see    mcp_IfSlpMng.h, mcp_transport.c
 */

#include "mcpf_mem.h"
#include "mcpf_services.h"
#include "mcpf_report.h"
#include "mcp_hal_os.h"
#include "mcp_hal_pm.h"
#include "BusDrv.h"
#include "TxnDefs.h"
#include "mcp_txnpool.h"
#include "TxnQueue.h"
#include "mcp_IfSlpMng.h"


/************************************************************************
 * Defines
 ************************************************************************/

/************************************************************************
 * Types
 ************************************************************************/


/* Transport Layer Object */
typedef struct
{
	/* module handles */
	handle_t       	hMcpf;             

	EIfSlpMngState  eState;
	TTxnStruct	  *	pMngTxn; 		/* Preallocated transaction for sleep/awake request */
	handle_t       	hTxnQ;			/* Transaction queue handle */
	handle_t       	hBusDrv;		/* Bus driver handle */
	McpU32 			uFuncId;		/* Transaction queue function id */
	McpHalChipId 	tChipId;      	/* PM chip id for power management 	*/
	McpHalTranId 	tTransId;      	/* PM transport layer id for power management */

	TIfSlpMngStat 	stat;           /* Statistics */

} TifSlpMngObj;


/* Debug stuff */
#ifdef TRAN_DBG
TifSlpMngObj * g_pIfSlpMngObj;
#endif

/************************************************************************
 * Internal functions prototypes
 ************************************************************************/

/* Temporal functions for debug!!! change for real CCM functions */

static void pmEventHandler (const McpHalPmEventParms * pPmEvent);

/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/** 
 * \fn     ifSlpMng_Create 
 * \brief  Create the Interface Sleep Manager object
 * 
 */
handle_t ifSlpMng_Create (const handle_t hMcpf)
{
	TifSlpMngObj  * pIfSlpMng;

	pIfSlpMng = mcpf_mem_alloc (hMcpf, sizeof(TifSlpMngObj));
	if (pIfSlpMng == NULL)
	{
		return NULL;
	}
	mcpf_mem_zero (hMcpf, pIfSlpMng, sizeof(TifSlpMngObj));

	pIfSlpMng->hMcpf = hMcpf;

	pIfSlpMng->pMngTxn = mcpf_txnPool_Alloc (hMcpf);
  
	if (pIfSlpMng->pMngTxn == NULL)
	{
		mcpf_mem_free (hMcpf, pIfSlpMng);
		return NULL;
	}
#ifdef TRAN_DBG
	g_pIfSlpMngObj = pIfSlpMng;
#endif

	return ((handle_t) pIfSlpMng);
}


/** 
 * \fn     ifSlpMng_Init
 * \brief  Interface Sleep Manager object initialization
 * 
 */ 
EMcpfRes ifSlpMng_Init (handle_t 		 	 hIfSlpMng, 
						  const handle_t 	 hTxnQ,
						  const handle_t 	 hBusDrv,
						  const McpU32 	 	 uFuncId,
						  const McpHalChipId tChipId, 
						  const McpHalTranId tTransId)
{
	TifSlpMngObj  * pIfSlpMng = (TifSlpMngObj *) hIfSlpMng;
	McpHalPmHandler pmHandler = {pmEventHandler};

	pIfSlpMng->hTxnQ 	= hTxnQ;			/* Transaction queue handle 	 	*/
	pIfSlpMng->hBusDrv 	= hBusDrv;			/* Bus driver handle 	 			*/
	pIfSlpMng->uFuncId	= uFuncId;     		/* Transaction queue function id 	*/
	pIfSlpMng->tChipId 	= tChipId;      	/* PM chip id for power management 	*/
	pIfSlpMng->tTransId = tTransId;     	/* PM transport layer id for power management */

    /* Build Sleep Management transaction */
    TXN_PARAM_SET (pIfSlpMng->pMngTxn, TXN_HIGH_PRIORITY, TXN_FUNC_ID_CTRL, TXN_DIRECTION_WRITE, 0);
    BUILD_TTxnStruct (pIfSlpMng->pMngTxn, 0, NULL, 0, NULL, NULL);
    TXN_PARAM_SET_SINGLE_STEP(pIfSlpMng->pMngTxn, 1);  /* Sleep Management transaction is always single step */

	pIfSlpMng->eState = IFSLPMNG_STATE_AWAKE;

	MCP_HAL_PM_RegisterTransport (pIfSlpMng->tChipId, pIfSlpMng->tTransId, &pmHandler);

	return RES_OK;
}

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

EMcpfRes ifSlpMng_Reinit (handle_t 		 	 hIfSlpMng)
{
    TifSlpMngObj  * pIfSlpMng = (TifSlpMngObj *) hIfSlpMng;
    McpHalPmHandler pmHandler = {pmEventHandler};

    MCPF_ENTER_CRIT_SEC (pIfSlpMng->hMcpf);
    pIfSlpMng->eState = IFSLPMNG_STATE_AWAKE;
    MCPF_EXIT_CRIT_SEC (pIfSlpMng->hMcpf);
    txnQ_Stop (pIfSlpMng->hTxnQ, pIfSlpMng->uFuncId);  
    txnQ_Run (pIfSlpMng->hTxnQ, pIfSlpMng->uFuncId);

	MCP_HAL_PM_RegisterTransport (pIfSlpMng->tChipId, pIfSlpMng->tTransId, &pmHandler);

	return RES_OK;
}

/** 
 * \fn     ifSlpMng_Destroy 
 * \brief  Destroy the Interface Sleep Manager object
 * 
 */ 
EMcpfRes ifSlpMng_Destroy (handle_t	hIfSlpMng)
{
	TifSlpMngObj  * pIfSlpMng = (TifSlpMngObj *) hIfSlpMng;

	if (pIfSlpMng)
	{
		MCP_HAL_PM_DeregisterTransport (pIfSlpMng->tChipId, pIfSlpMng->tTransId);

		if (pIfSlpMng->pMngTxn)
		{
			mcpf_txnPool_FreeBuf (pIfSlpMng->hMcpf, pIfSlpMng->pMngTxn);
		}
		mcpf_mem_free (pIfSlpMng->hMcpf, pIfSlpMng);
	}
	return RES_OK;
}


/** 
 * \fn     ifSlpMng_HandleTxEvent
 * \brief  Interface Sleep Manager state machine for Tx event
 * 
 * Process Tx event of Interface Sleep Manager state machine
 * 
 */ 
EMcpfRes ifSlpMng_HandleTxEvent (handle_t hIfSlpMng)
{
	TifSlpMngObj  * pIfSlpMng = (TifSlpMngObj *) hIfSlpMng;
	McpBool			stateChanged = MCP_FALSE;

    MCPF_ENTER_CRIT_SEC (pIfSlpMng->hMcpf);
	if (pIfSlpMng->eState == IFSLPMNG_STATE_ASLEEP)
	{
		pIfSlpMng->eState = IFSLPMNG_STATE_WAIT_FOR_AWAKE_ACK;
		stateChanged = MCP_TRUE;
		pIfSlpMng->stat.AwakeInd_Tx++;
	}
    MCPF_EXIT_CRIT_SEC (pIfSlpMng->hMcpf);

	if (stateChanged)
	{
		MCP_HAL_PM_TransportWakeup (pIfSlpMng->tChipId, pIfSlpMng->tTransId);

		/* Prepare and send AwakeInd request */
		TXN_PARAM_SET_IFSLPMNG_OP (pIfSlpMng->pMngTxn, TXN_PARAM_IFSLPMNG_OP_AWAKE);
		MCPF_REPORT_DEBUG_TX(pIfSlpMng->hMcpf, IFSLPMNG_MODULE_LOG, ("ifSlpMng_HandleTxEvent: Send Awake, pIfSlpMng=%p, state=%d\n", 
												pIfSlpMng, pIfSlpMng->eState));
		txnQ_Transact (pIfSlpMng->hTxnQ, pIfSlpMng->pMngTxn);
	}

	return RES_OK;
}


/** 
 * \fn     ifSlpMng_HandleRxEvent
 * \brief  Interface Sleep Manager state machine for Rx event
 * 
 * Process Rx event of Interface Sleep Manager state machine
 * 
 */ 
EIfSlpMngStatus ifSlpMng_HandleRxEvent (handle_t hIfSlpMng, TTxnStruct *pTxn) 
{
	TifSlpMngObj  * pIfSlpMng = (TifSlpMngObj *) hIfSlpMng;
	EIfSlpMngStatus eRes = IfSlpMng_ERR;
	McpBool			stateChanged = MCP_FALSE;

	if (TXN_PARAM_GET_SINGLE_STEP (pTxn))
	{
		/* IfSlpMng transaction is always single step, 
		 * get IfSlpMng opcode from transaction structure
		 */
		eRes = IfSlpMng_SLP_PKT_TYPE;

		switch (TXN_PARAM_GET_IFSLPMNG_OP (pTxn))
		{

		case TXN_PARAM_IFSLPMNG_OP_SLEEP:

			MCPF_REPORT_DEBUG_RX(pIfSlpMng->hMcpf, IFSLPMNG_MODULE_LOG, ("ifSlpMng_HandleRxEvent: SLEEP_IND, state=%d\n",pIfSlpMng->eState));

			MCPF_ENTER_CRIT_SEC (pIfSlpMng->hMcpf);
			if (pIfSlpMng->eState == IFSLPMNG_STATE_AWAKE)
			{
				pIfSlpMng->eState = IFSLPMNG_STATE_WAIT_FOR_SLEEPACK_TX_COMPL;
				stateChanged = MCP_TRUE;
			}
			pIfSlpMng->stat.SleepInd_Rx++;
			MCPF_EXIT_CRIT_SEC (pIfSlpMng->hMcpf);

			if (stateChanged)
			{
				txnQ_Stop (pIfSlpMng->hTxnQ, pIfSlpMng->uFuncId); 

				/* Prepare and send SleepAck request */
				TXN_PARAM_SET_IFSLPMNG_OP (pIfSlpMng->pMngTxn, TXN_PARAM_IFSLPMNG_OP_SLEEP_ACK);
				txnQ_Transact (pIfSlpMng->hTxnQ, pIfSlpMng->pMngTxn);
				pIfSlpMng->stat.SleepAck_Tx++;
			}
			break;

		case TXN_PARAM_IFSLPMNG_OP_SLEEP_ACK:
			MCPF_REPORT_ERROR (pIfSlpMng->hMcpf, IFSLPMNG_MODULE_LOG, 
							  ("%s: Not expected SleepAck received\n", __FUNCTION__));
			pIfSlpMng->stat.SleepAck_Rx++;
			break;

		case TXN_PARAM_IFSLPMNG_OP_AWAKE:
			{
				EIfSlpMngState  oldState;

				MCPF_REPORT_DEBUG_RX(pIfSlpMng->hMcpf, IFSLPMNG_MODULE_LOG, ("ifSlpMng_HandleRxEvent: AWAKE_IND, state=%d\n",pIfSlpMng->eState));

				MCPF_ENTER_CRIT_SEC (pIfSlpMng->hMcpf);
				oldState = pIfSlpMng->eState;
				if ((pIfSlpMng->eState == IFSLPMNG_STATE_ASLEEP) ||
					(pIfSlpMng->eState == IFSLPMNG_STATE_WAIT_FOR_AWAKE_ACK))
				{
					pIfSlpMng->eState = IFSLPMNG_STATE_AWAKE;
					stateChanged = MCP_TRUE;
				}
				pIfSlpMng->stat.AwakeInd_Rx++;
				MCPF_EXIT_CRIT_SEC (pIfSlpMng->hMcpf);
	
				if (stateChanged)
				{
					switch (oldState)
					{
					case IFSLPMNG_STATE_ASLEEP:
	
						/* Prepare and send SleepAck request */
						TXN_PARAM_SET_IFSLPMNG_OP (pIfSlpMng->pMngTxn, TXN_PARAM_IFSLPMNG_OP_AWAKE_ACK);
						txnQ_Transact (pIfSlpMng->hTxnQ, pIfSlpMng->pMngTxn);
						txnQ_Run (pIfSlpMng->hTxnQ, pIfSlpMng->uFuncId); 
						pIfSlpMng->stat.AwakeAck_Tx++;
						break;
	
					case IFSLPMNG_STATE_WAIT_FOR_AWAKE_ACK:
	
						txnQ_Run (pIfSlpMng->hTxnQ, pIfSlpMng->uFuncId); 
						break;

                    default:
                       break;
					}
				}
			}
			break;

		case TXN_PARAM_IFSLPMNG_OP_AWAKE_ACK:

			MCPF_REPORT_DEBUG_RX(pIfSlpMng->hMcpf, IFSLPMNG_MODULE_LOG, ("ifSlpMng_HandleRxEvent: AWAKE_ACK, state=%d\n",pIfSlpMng->eState));

			MCPF_ENTER_CRIT_SEC (pIfSlpMng->hMcpf);
			if (pIfSlpMng->eState == IFSLPMNG_STATE_WAIT_FOR_AWAKE_ACK)
			{
				pIfSlpMng->eState =  IFSLPMNG_STATE_AWAKE;
				stateChanged = MCP_TRUE;
			}
			pIfSlpMng->stat.AwakeAck_Rx++;
			MCPF_EXIT_CRIT_SEC (pIfSlpMng->hMcpf);

			if (stateChanged)
			{
				txnQ_Run (pIfSlpMng->hTxnQ, pIfSlpMng->uFuncId);
			}
			break;

		default:
			MCPF_REPORT_ERROR (pIfSlpMng->hMcpf, IFSLPMNG_MODULE_LOG, 
							  ("%s: Invalid IfSlpMng opcode=%u\n", __FUNCTION__, TXN_PARAM_GET_IFSLPMNG_OP (pTxn)));
			eRes = IfSlpMng_ERR;
			break;
		} /* Rx IFSLPMNG_OP */
	}
	else
	{
		/* Normal packet received */
		MCPF_ENTER_CRIT_SEC (pIfSlpMng->hMcpf);
		if (pIfSlpMng->eState == IFSLPMNG_STATE_ASLEEP)
		{
			pIfSlpMng->eState = IFSLPMNG_STATE_AWAKE;
			stateChanged = MCP_TRUE;
			pIfSlpMng->stat.AwakeAck_Tx++;
		}
		MCPF_EXIT_CRIT_SEC (pIfSlpMng->hMcpf);

		if (stateChanged)
		{
			/* Prepare and send SleepAck request */
			TXN_PARAM_SET_IFSLPMNG_OP (pIfSlpMng->pMngTxn, TXN_PARAM_IFSLPMNG_OP_AWAKE_ACK);
			txnQ_Transact (pIfSlpMng->hTxnQ, pIfSlpMng->pMngTxn);

			txnQ_Run (pIfSlpMng->hTxnQ, pIfSlpMng->uFuncId);

			MCPF_REPORT_DEBUG_RX(pIfSlpMng->hMcpf, IFSLPMNG_MODULE_LOG, ("ifSlpMng_HandleRxEvent: Normal RX, state=%d\n",pIfSlpMng->eState));
		}

		eRes = IfSlpMng_OK;
	}
	return eRes;
}


/** 
 * \fn     ifSlpMng_handleRxReady
 * \brief  Interface Sleep Manager state machine for Rx ready event
 * 
 * Processes receive ready event of Interface Sleep Manager state machine.
 * Any bytes received in sleep state are treated as awake indication and Rx buffer is cleared.
 * 
 */ 
EMcpfRes ifSlpMng_handleRxReady(handle_t hIfSlpMng) 
{
	TifSlpMngObj  * pIfSlpMng = (TifSlpMngObj *) hIfSlpMng;
	McpBool			stateChanged = MCP_FALSE;
    
	MCPF_ENTER_CRIT_SEC (pIfSlpMng->hMcpf);
	if (pIfSlpMng->eState == IFSLPMNG_STATE_ASLEEP)
	{
		pIfSlpMng->eState = IFSLPMNG_STATE_AWAKE;
		stateChanged = MCP_TRUE;
		pIfSlpMng->stat.AwakeInd_Rx++;
	}
	MCPF_EXIT_CRIT_SEC (pIfSlpMng->hMcpf);

	if (stateChanged)
	{
		MCPF_REPORT_DEBUG_TX(pIfSlpMng->hMcpf,IFSLPMNG_MODULE_LOG, ("ifSlpMng_handleRxReady: Awaking, state=%d\n", pIfSlpMng->eState));

		MCP_HAL_PM_TransportWakeup (pIfSlpMng->tChipId, pIfSlpMng->tTransId);

		busDrv_RxReset (pIfSlpMng->hBusDrv);

		/* Prepare and send SleepAck request */
		TXN_PARAM_SET_IFSLPMNG_OP (pIfSlpMng->pMngTxn, TXN_PARAM_IFSLPMNG_OP_AWAKE_ACK);
		txnQ_Transact (pIfSlpMng->hTxnQ, pIfSlpMng->pMngTxn);

		txnQ_Run (pIfSlpMng->hTxnQ, pIfSlpMng->uFuncId); 
		pIfSlpMng->stat.AwakeAck_Tx++;
		return RES_PENDING;         /* indicate the state was sleeping */
	}
	return RES_COMPLETE;
}


/** 
 * \fn     ifSlpMng_HandleTxComplEvent
 * \brief  Interface Sleep Manager state machine for Tx complete event
 * 
 * Process transaction sending complete event of Interface Sleep Manager state machine
 * 
 */ 
EMcpfRes ifSlpMng_HandleTxComplEvent (handle_t hIfSlpMng) 
{
	TifSlpMngObj  * pIfSlpMng = (TifSlpMngObj *) hIfSlpMng;
	McpBool			stateChanged  = MCP_FALSE;
	McpBool			awakeRequired = MCP_FALSE;
    
	MCPF_ENTER_CRIT_SEC (pIfSlpMng->hMcpf);
	if (pIfSlpMng->eState == IFSLPMNG_STATE_WAIT_FOR_SLEEPACK_TX_COMPL)
	{
		stateChanged = MCP_TRUE;
		pIfSlpMng->stat.SleepAckTxCompl++;

		if (txnQ_IsQueueEmpty (pIfSlpMng->hTxnQ, pIfSlpMng->uFuncId))
		{
			pIfSlpMng->eState = IFSLPMNG_STATE_ASLEEP;
		}
		else
		{
			pIfSlpMng->eState = IFSLPMNG_STATE_WAIT_FOR_AWAKE_ACK;
			awakeRequired = MCP_TRUE;
			pIfSlpMng->stat.AwakeInd_Tx++;
		}
	}
	MCPF_EXIT_CRIT_SEC (pIfSlpMng->hMcpf);

	if (stateChanged)
	{
		if (!awakeRequired)
		{
			MCP_HAL_PM_TransportSleep (pIfSlpMng->tChipId, pIfSlpMng->tTransId);

			MCPF_REPORT_DEBUG_TX(pIfSlpMng->hMcpf, IFSLPMNG_MODULE_LOG, ("ifSlpMng_HandleTxComplEvent: going to sleep, pIfSlpMng=%p, state=%d\n", 
													pIfSlpMng, pIfSlpMng->eState));
		}
		else
		{
			/* Prepare and send AwakeInd request */
			TXN_PARAM_SET_IFSLPMNG_OP (pIfSlpMng->pMngTxn, TXN_PARAM_IFSLPMNG_OP_AWAKE);
			txnQ_Transact (pIfSlpMng->hTxnQ, pIfSlpMng->pMngTxn);

			MCPF_REPORT_DEBUG_TX(pIfSlpMng->hMcpf, IFSLPMNG_MODULE_LOG, ("ifSlpMng_HandleTxComplEvent: Send Awake, pIfSlpMng=%p, state=%d\n", 
													pIfSlpMng, pIfSlpMng->eState));
		}
	}

	return RES_OK;
}


/** 
 * \fn     ifSlpMng_GetStat
 * \brief  Gets Interface Sleep Management statistics
 * 
 */ 
void ifSlpMng_GetStat (const handle_t hIfSlpMng, TIfSlpMngStat  * pStat, EIfSlpMngState * pState)
{
	TifSlpMngObj  * pIfSlpMng = (TifSlpMngObj *) hIfSlpMng;

	mcpf_mem_copy (pIfSlpMng->hMcpf, pStat, &pIfSlpMng->stat, sizeof(TIfSlpMngStat));
	*pState = pIfSlpMng->eState;
}


/************************************************************************
 * Module Private Functions
 ************************************************************************/

/** 
 * \fn     pmEventHandler 
 * \brief  Power management callback function
 * 
 * The function is called by PM upon completion of requested operation if 
 * PM works in asynch mode.
 * 
 * \note
 * \param	pPmEvent - pointer to PM event
 * \return 	void
 * \sa     	pmEventHandler
 */ 

static void pmEventHandler (const McpHalPmEventParms * pPmEvent )
{
	const McpHalPmEventParms *unused = pPmEvent;

	MCPF_UNUSED_PARAMETER(unused);
	
	/* TBD for PM async mode */
}



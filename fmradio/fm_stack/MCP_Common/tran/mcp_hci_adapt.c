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


/** \file   mcp_hci_adapter.c 
 *  \brief  Host Controller Interface Adapter (HCIA) implementation. 
 * 
 *  \see    mcp_hci_adapter, mcp_transport.h
 */

#include "mcpf_mem.h"
#include "mcpf_services.h"
#include "mcpf_report.h"
#include "mcp_txnpool.h"
#include "mcp_transport.h"
#include "mcp_hciDefs.h"
#include "mcp_hci_adapt.h"
#include "mcpf_time.h"
#include "ccm_adapt.h"


/************************************************************************
 * Defines
 ************************************************************************/

/* MS to wait for HCI events */
#define COMMAND_COMPL_WAIT_TIME  (500)

#if 0
#define WATCHDOG_COMMANDS
#endif
/************************************************************************
 * Types
 ************************************************************************/

/* Command Node */
typedef struct
{
    MCP_DL_LIST_Node	tHead;          /* Header for queueing 						*/
	THCIA_CmdComplCb   	fCmdComplCb;	/* User command complete call back function */
	handle_t       		hCbParam;		/* call back parameter to pass 				*/
	TTxnStruct 			*pTxn;			/* Transaction structure containing HCI command packet */			
    McpBool             needFreeMem;    /* indication if need to free transaction buffer when command complete */		

} TCmdNode;

/* Client Node */
typedef struct
{
    MCP_DL_LIST_Node	tHead;          /* Header for link list 					  */
	McpU32				uOpcode;        /* Hci message opcode to subscribe 			  */
	TI_TransRxIndCb   	fRxIndCb;		/* User receive indicaiton call back function */
	handle_t       		hCbParam;		/* call back parameter to pass 				  */

} TClientNode;

/* Command Queue */
typedef struct
{
	handle_t       	hMcpf;                        
	handle_t		hOutQue;
	handle_t		hSentQue;
	handle_t		hPool;

	McpU32			uCmdMaxNum;			/* maximum number of command nodes in pool */
	McpU32			uOutCmdMaxNum;		/* maximum number of commands to send while waiting for compl. event 	 */
	McpU32			uOutCmdNum;			/* current number of commands already already sent but not completed yet */
    handle_t        watchdogTimer;      /* Watchdog timer for HCI events */
    mcpf_timer_cb   watchDogTimerCB;    /* watchdog callback routine */
    McpBool         unackedCommand;     /* TRUE when there is an unacknowledged command */

} TCmdQueue;

/* Client Register */
typedef struct
{
	handle_t       		hMcpf;                        
	handle_t 			hHcia;
	McpU32		   		uListNum;			/* Number of client lists in array 			  				*/
	McpU32       		uIndexOfs;			/* Offset to convert HCI packet type to client list index 	*/
    MCP_DL_LIST_Node  	*pListArray;     	/* Array of list headers, list per HCI packet type  		*/
    McpBool   		  	*pChanStateArray;  	/* Array of bool flags whether the transport channel is open*/

} TClientRegister;


/* HCI Adapter Object */
typedef struct
{
	/* module handles */
	handle_t       	hMcpf;
	TCmdQueue		*pCmdQue;
	TClientRegister *pClientRegister;

	McpU32			uHciTypeMin;
	McpU32			uHciTypeMax;

	/* THciaStat 	    stat;  */          		/* HCIA Statistics */

} THciaObj;


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/

/* HCIA Rx indication call back function invoked by transport layer */
static EMcpfRes hciaRxInd (handle_t hTrans, handle_t hCbParam, TTxnStruct *pTxn);

static TCmdQueue * cmdQueue_Create (const THciaObj 	*pHcia,
									const McpU32  	uCmdMaxNum,
									const McpU32	uOutCmdMaxNum);

static void cmdQueue_Destroy (TCmdQueue * pCmdQue);

static EMcpfRes cmdQueue_add (TCmdQueue 			*pCmdQue,
							  TTxnStruct 			*pTxn,
							  const THCIA_CmdComplCb fCallBack,
							  const handle_t 		 hCbParam,
                              McpBool needFreeMem);

static  EMcpfRes cmdQueue_complete (TCmdQueue   	*pCmdQue,
								   const McpU8	    uEvtOpcode,
								   const McpU8	    uEvtParamLen,
								   McpU8		    *pEvtParams,
                                   McpBool          *needFreeMem);

static void cmdQueue_freeCmdTxn (const TCmdQueue *pCmdQue, TCmdNode *pCmd);

static void cmdQueue_flush (TCmdQueue *pCmdQue);

static TClientRegister * clientRegister_Create (const THciaObj 	*pHcia,
												const McpU8		uHciTypeMin,
												const McpU8		uHciTypeMax);
	
static void clientRegister_Destroy (TClientRegister * pClientReg);

static EMcpfRes clientRegister_add (TClientRegister 	  	*pClientReg, 
									const McpU8 			uHciMsgType,
									const McpU16 			uHciOpcode,
									const TI_TransRxIndCb 	fCallBack,
									const handle_t 			hCbParam);

static EMcpfRes clientRegister_delete (TClientRegister *pClientReg, 
									   const McpU8 		uHciMsgType,
									   const McpU16 	uHciOpcode);

static EMcpfRes clientRegister_searchAndInvoke (TClientRegister 	*pClientReg, 
												const McpU8 		uHciMsgType,
												const McpU16 		uHciOpcode,
												handle_t			hTrans,
												TTxnStruct 			*pTxn);

static EMcpfRes clientRegister_openTransChan (TClientRegister 	  		*pClientReg, 
											  const McpU8 				uHciMsgType,
											  const TI_TransRxIndCb 	fCallBack,
											  const handle_t 			hCbParam);

static EMcpfRes clientRegister_closeTransChan (TClientRegister *pClientReg, 
											   const McpU8 	uHciMsgType);

static void hciaTxCompl (handle_t hCbHandle, void *pTxn);

static void HciaCommandWatchdog(handle_t hCaller, McpUint uTimerId);

/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/** 
 * \fn     HCIA_Create 
 * \brief  Create HCIA object
 * 
 */ 
handle_t HCIA_Create (const handle_t hMcpf,
					  const McpU32	 uCmdMaxNum,
					  const McpU32	 uOutCmdMaxNum,
					  const McpU8	 uHciTypeMin,
					  const McpU8	 uHciTypeMax)
{
	THciaObj  * pHcia;
	EMcpfRes	eRes;

	pHcia = mcpf_mem_alloc (hMcpf, sizeof(THciaObj));
	MCPF_Assert(pHcia);
	mcpf_mem_zero (hMcpf, pHcia, sizeof(THciaObj));

	pHcia->hMcpf = hMcpf;
	pHcia->uHciTypeMin = uHciTypeMin;
	pHcia->uHciTypeMax = uHciTypeMax;

	pHcia->pCmdQue = cmdQueue_Create (pHcia, uCmdMaxNum, uOutCmdMaxNum);
	MCPF_Assert(pHcia->pCmdQue);

       /* Initialize command watchdogTimer timer */
    pHcia->pCmdQue->watchDogTimerCB = HciaCommandWatchdog;
    pHcia->pCmdQue->watchdogTimer = NULL;
    pHcia->pCmdQue->unackedCommand = MCP_FALSE;

	pHcia->pClientRegister = clientRegister_Create (pHcia, uHciTypeMin, uHciTypeMax);
	MCPF_Assert(pHcia->pClientRegister);

	/* HCIA always opens chan for HCI Events reception for command queue flow control */
	eRes = clientRegister_openTransChan (pHcia->pClientRegister, 
										 HCI_PKT_TYPE_EVENT, 
										 hciaRxInd, 
										 pHcia); 
	MCPF_Assert(eRes == RES_OK);

	return(handle_t) pHcia;
}

/** 
 * \fn     HCIA_Destroy 
 * \brief  Destroy HCIA object
 * 
 */ 
void HCIA_Destroy (handle_t	hHcia)
{
	THciaObj  * pHcia = (THciaObj  *) hHcia;

	clientRegister_closeTransChan (pHcia->pClientRegister, HCI_PKT_TYPE_EVENT);

	clientRegister_Destroy (pHcia->pClientRegister);
#ifdef WATCHDOG_COMMANDS
    if (pHcia->pCmdQue->watchdogTimer != NULL)
    {
        mcpf_timer_stop (pHcia->hMcpf, pHcia->pCmdQue->watchdogTimer);
    }
#endif
	cmdQueue_Destroy (pHcia->pCmdQue);

	mcpf_mem_free (pHcia->hMcpf, pHcia);
}


/** 
 * \fn     HCIA_RegisterClient 
 * \brief  Register the client to HCIA
 * 
 */ 
EMcpfRes HCIA_RegisterClient (const handle_t			hHcia, 
							  const McpU8 				uHciMsgType,
							  const McpU16 				uHciOpcode,
							  const TI_TransRxIndCb 	fCallBack,
							  const handle_t 			hCbParam,
							  const EHcaRegisterMethod 	eRegisterMethod)
{
	THciaObj  * pHcia = (THciaObj  *) hHcia;
	EMcpfRes  eRes = RES_ERROR;

	switch (eRegisterMethod)
	{
	case HCIA_REG_METHOD_NORMAL:

		eRes = clientRegister_add (pHcia->pClientRegister,
								   uHciMsgType,
								   uHciOpcode,
								   fCallBack,
								   hCbParam);
		break;

	case HCIA_REG_METHOD_BYPASS:

		eRes = clientRegister_openTransChan (pHcia->pClientRegister, 
											 uHciMsgType,
											 fCallBack,
											 hCbParam);
		break;

	default:
		MCPF_REPORT_ERROR(pHcia->hMcpf, TRANS_MODULE_LOG, 
						  ("HCIA_RegisterClient: Unknown register method %u", eRegisterMethod));
		break;
	}

	return eRes;
}

/** 
 * \fn     HCIA_RegisterClient 
 * \brief  Register the client to HCIA
 * 
 */ 
EMcpfRes HCIA_UnregisterClient (const handle_t			 hHcia, 
								const McpU8 			 uHciMsgType,
								const McpU16 			 uHciOpcode,
								const EHcaRegisterMethod eRegisterMethod)
{
	THciaObj  * pHcia = (THciaObj  *) hHcia;
	EMcpfRes  eRes = RES_ERROR;

	switch (eRegisterMethod)
	{
	case HCIA_REG_METHOD_NORMAL:

		eRes = clientRegister_delete (pHcia->pClientRegister, uHciMsgType, uHciOpcode);
		break;

	case HCIA_REG_METHOD_BYPASS:

		eRes = clientRegister_closeTransChan (pHcia->pClientRegister, uHciMsgType);
		break;

	default:
		MCPF_REPORT_ERROR(pHcia->hMcpf, TRANS_MODULE_LOG, 
						  ("HCIA_UnregisterClient: Unknown register method %u", eRegisterMethod));
		break;
	}

	return eRes;
}

/** 
 * \fn     HCIA_SendCommand 
 * \brief  Send HCI command
 * 
 */ 
EMcpfRes HCIA_SendCommand (const handle_t			hHcia, 
						   const McpU16 			uHciOpcode,
						   const McpU8 *			pHciCmdParms,
						   const McpU16				uHciCmdParmsLen,
						   const THCIA_CmdComplCb 	fCallBack,
						   const handle_t 			hCbParam)
{
	THciaObj   *pHcia = (THciaObj *) hHcia;
	TTxnStruct *pTxn;
	EMcpfRes  	eRes;

	/* Allocate transaction Structure and a Buffer */
	pTxn = mcpf_txnPool_AllocBuf(pHcia->hMcpf, (McpU16)(uHciCmdParmsLen + HCI_PREAMBLE_SIZE(COMMAND)));

	if(pTxn)
	{
		pTxn->pData[0] = (McpU8) HCI_PKT_TYPE_COMMAND;
		mcpf_endian_HostToLE16( uHciOpcode, &pTxn->pData[1]);
		pTxn->pData[3] = (McpU8) uHciCmdParmsLen;
	
		/* Copy command parameters to buffer */
		if (pHciCmdParms && uHciCmdParmsLen)
		{
			mcpf_mem_copy(pHcia->hMcpf, (void *) &pTxn->pData[4], (void *) pHciCmdParms, uHciCmdParmsLen);
		}
	
		/* Increment length by length of HCI type and header */
		pTxn->aLen[0] = (McpU16) (uHciCmdParmsLen + HCI_PREAMBLE_SIZE(COMMAND));
		pTxn->protocol = PROTOCOL_ID_HCI;
		TXN_PARAM_SET_CHAN_NUM(pTxn, HCI_PKT_TYPE_COMMAND); /* sets channel number */
	
		/* Set TX complete call back*/
		pTxn->fTxnDoneCb = hciaTxCompl;
		pTxn->hCbHandle  = hHcia;
	
		eRes = cmdQueue_add (pHcia->pCmdQue, pTxn, fCallBack, hCbParam, MCP_TRUE);
		if (eRes != RES_OK)
		{
			MCPF_REPORT_ERROR(pHcia->hMcpf, TRANS_MODULE_LOG, 
							  ("HCIA_SendCommand: Failed to send transaction buffer"));
			mcpf_txnPool_FreeBuf(pHcia->hMcpf, pTxn);
		}
	}
	else
	{
		MCPF_REPORT_ERROR(pHcia->hMcpf, TRANS_MODULE_LOG, 
						  ("HCIA_SendCommand: Failed to alloc txn and buffer"));
		eRes = RES_ERROR;
	}

	return eRes;
}


/** 
 * \fn     HCIA_SendData 
 * \brief  Send HCI packet (command, ACL or SCO)
 * 
 */ 
EMcpfRes HCIA_SendData (const handle_t hHcia, TTxnStruct *pTxn)
{
	THciaObj   *pHcia = (THciaObj *) hHcia;
	EMcpfRes  	eRes;

	/* Checks whether HCI packet type is a command */
	if (pTxn->tHeader.tHciHeader.uPktType == HCI_PKT_TYPE_COMMAND)
	{
 /*       MCPF_REPORT_INFORMATION(pHcia->hMcpf, TRANS_MODULE_LOG, ("HCIA_SendData: call cmdQueue_add")); */
		eRes = cmdQueue_add (pHcia->pCmdQue, pTxn, NULL, NULL, MCP_FALSE);
		if (eRes != RES_OK)
		{
			MCPF_REPORT_ERROR(pHcia->hMcpf, TRANS_MODULE_LOG, 
							  ("HCIA_SendData: Failed to send transaction buffer"));
		}
	}
	else
	{
		eRes = mcpf_trans_TxData(pHcia->hMcpf, pTxn);
	}

	return eRes;
}


/** 
 * \fn     HCIA_CmdQueueFlush 
 * \brief  Clear HCIA command queue
 * 
 */ 
EMcpfRes HCIA_CmdQueueFlush (const handle_t	  hHcia) 
{
	THciaObj   *pHcia = (THciaObj *) hHcia;

	cmdQueue_flush (pHcia->pCmdQue);

	return RES_OK;
}

/** 
 * \fn     HCIA_CmdQueueRemove 
 * \brief  remove item from HCIA command queue
 * 
 * The function removes item from HCI command queue. May be used
 * in specific cases when no response is expected for the
 * command
 * 
 * \note
 * \param	hHcia - handle to HCIA object
 * \param   uEvtOpcode  command opcode
 * \param   uEvtParamLen parameters length
 * \param   pEvtParams pointer to command parameters
 * \return 	Returns the status of operation: OK or Error
 * \sa HCIA_SendCommand
 */ 
EMcpfRes HCIA_CmdQueueRemove(const handle_t pHcia,
								   const McpU8	uEvtOpcode,
								   const McpU8	uEvtParamLen,
								   McpU8		*pEvtParams)
{
    McpBool dummy;
    
   return cmdQueue_complete(((THciaObj  *)pHcia)->pCmdQue,uEvtOpcode, uEvtParamLen, pEvtParams, &dummy); 
}

/************************************************************************
 * Module Private Functions
 ************************************************************************/

/** 
 * \fn     hciaRxInd 
 * \brief  HCIA Receive indication
 * 
 * Process Rx indication called by transport layer
 * 
 * \note
 * \param	hTrans 	  - transport layer handle
 * \param	hCbHandle - call back parameter
 * \param  	pTxn 	  - pointer to transaction structure
 * \return 	void
 * \sa     	trans_Init
 */ 
static EMcpfRes hciaRxInd (handle_t hTrans, handle_t hCbParam, TTxnStruct *pTxn)
{
	THciaObj  *pHcia = (THciaObj  *) hCbParam;
	EMcpfRes  eRes;
	McpU16	  uLen, uOpcode;
	McpU8	  uHciType = pTxn->tHeader.tHciHeader.uPktType;
    McpBool      needFreeMem;

	if (uHciType == HCI_PKT_TYPE_EVENT)
	{
		uOpcode = pTxn->pData[1];
		uLen 	= (McpU8)pTxn->pData[2];

		if ((uOpcode == CCMA_HCI_EVENT_COMMAND_COMPLETE) ||(uOpcode == CCMA_HCI_EVENT_COMMAND_STATUS))
		{
			eRes = cmdQueue_complete (pHcia->pCmdQue,
									  (const McpU8) uOpcode,
									  (const McpU8) uLen,
									  &pTxn->pData[3], &needFreeMem);

			switch (eRes)
            {
            case RES_PENDING: 
			{
				/* The command call back function was not used, search items in ClientRegister */
				eRes = clientRegister_searchAndInvoke (pHcia->pClientRegister, uHciType, uOpcode, hTrans, pTxn);
				if (eRes != RES_OK)
				{
					/* the destination was not found, free received Txn */
					mcpf_txnPool_FreeBuf (pHcia->hMcpf, pTxn);
				}
                break;
			}
            case RES_OK:
			{
                if (needFreeMem)
				{
                    mcpf_txnPool_FreeBuf (pHcia->hMcpf, pTxn);
                }
                else
                { /* free just the transaction */
                    mcpf_txnPool_Free(pHcia->hMcpf, pTxn);
                }
                break;
			}
            default: 
			{
				/* free received Txn if completion call back was invoked or error occurred */
				mcpf_txnPool_FreeBuf (pHcia->hMcpf, pTxn);
			}
		}
		}
		else
		{
			/* other events */
			eRes = clientRegister_searchAndInvoke (pHcia->pClientRegister, uHciType, uOpcode, hTrans, pTxn);
			if (eRes != RES_OK)
			{
				/* the destination was not found, free received Txn */
				mcpf_txnPool_FreeBuf (pHcia->hMcpf, pTxn);
			}
		}
	}
	else
	{
		/* ACL, SCO and other packets */
		uOpcode = mcpf_endian_LEtoHost16(&(pTxn->pData[1]));
		eRes = clientRegister_searchAndInvoke (pHcia->pClientRegister, uHciType, uOpcode, hTrans, pTxn);
		if (eRes != RES_OK)
		{
			/* the destination was not found, free received Txn */
			mcpf_txnPool_FreeBuf (pHcia->hMcpf, pTxn);
		}
	}

	return eRes;
}

/** 
 * \fn     cmdQueue_Create 
 * \brief  Create command queue object
 * 
 * Allocates memory and queue
 * 
 * \note
 * \param	pHcia - pointer to HCIA object
 * \return 	void
 * \sa     	cmdQueue_Destroy
 */ 
static TCmdQueue * cmdQueue_Create (const THciaObj 	*pHcia,
									const McpU32  	uCmdMaxNum,
									const McpU32	uOutCmdMaxNum)
{
	TCmdQueue * pCmdQue;
	McpU32 		uNodeHeaderOffset;

	pCmdQue = mcpf_mem_alloc (pHcia->hMcpf, sizeof(TCmdQueue));
	MCPF_Assert(pCmdQue);

	mcpf_mem_zero (pHcia->hMcpf, pCmdQue, sizeof(TCmdQueue));

	pCmdQue->hMcpf 	   	   = pHcia->hMcpf;
	pCmdQue->uCmdMaxNum    = uCmdMaxNum;
	pCmdQue->uOutCmdMaxNum = uOutCmdMaxNum;

	uNodeHeaderOffset = MCPF_FIELD_OFFSET(TCmdNode, tHead);
	pCmdQue->hOutQue     = mcpf_que_Create(pHcia->hMcpf, uCmdMaxNum, uNodeHeaderOffset);
	MCPF_Assert(pCmdQue->hOutQue);

	pCmdQue->hSentQue    = mcpf_que_Create(pHcia->hMcpf, uOutCmdMaxNum + 1, uNodeHeaderOffset);
	MCPF_Assert(pCmdQue->hSentQue);

	pCmdQue->hPool = mcpf_memory_pool_create((handle_t)pHcia->hMcpf, sizeof(TCmdNode), (McpU16) uCmdMaxNum);
	MCPF_Assert(pCmdQue->hPool);

	return (pCmdQue);
}

/** 
 * \fn     cmdQueue_Destroy 
 * \brief  Destroy command queue object
 * 
 * Dealocates memory pool and queue objects
 * 
 * \note
 * \param	pCmdQue - pointer to Command Queue object
 * \return 	void
 * \sa     	cmdQueue_Create
 */ 
static void cmdQueue_Destroy (TCmdQueue * pCmdQue)
{
	cmdQueue_flush (pCmdQue);

	mcpf_que_Destroy (pCmdQue->hOutQue);
	mcpf_que_Destroy (pCmdQue->hSentQue);

	if (pCmdQue->hPool)
	{
		mcpf_memory_pool_destroy(pCmdQue->hMcpf, pCmdQue->hPool);
	}

	mcpf_mem_free (pCmdQue->hMcpf, pCmdQue);
}

/** 
 * \fn     cmdQueue_add 
 * \brief  Add command to command queue
 * 
 * Send the command, if flow control allows or enqueue it, if not.
 * 
 * \note
 * \param	pCmdQue - pointer to Command Queue object
 * \param	pTxn    - pointer to transaction structure to send
 * \param	fCallBack - command completion call back function
 * \param	hCbParam  - command completion function parameter
 * \param   need_dealloc_mem - indication whether free mem
 * \return 	result of operation: RES_OK or RES_ERROR
 * \sa     	cmdQueue_complete
 */ 
static EMcpfRes cmdQueue_add (TCmdQueue 				*pCmdQue,
							  TTxnStruct 				*pTxn,
							  const THCIA_CmdComplCb 	fCallBack,
                              const handle_t            hCbParam,
                              McpBool                   needFreeMem)
{
	TCmdNode *pCmd;
	EMcpfRes  eRes  = RES_ERROR;
	McpBool	  bSend = MCP_FALSE;
#ifdef WATCHDOG_COMMANDS
    McpU16    uOpcode = 0;
#endif

	pCmd = (TCmdNode *) mcpf_mem_alloc_from_pool (pCmdQue->hMcpf, pCmdQue->hPool);
	if (pCmd)
	{
		pCmd->pTxn 		  = pTxn;
		pCmd->fCmdComplCb = fCallBack;
		pCmd->hCbParam 	  = hCbParam;
		pCmd->needFreeMem = needFreeMem;

		/* Check command flow control, whether it is possible to send the command */

		MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
		if (pCmdQue->uOutCmdNum < pCmdQue->uOutCmdMaxNum)
		{
			pCmdQue->uOutCmdNum++;
			bSend = MCP_TRUE;
        /*    MCPF_REPORT_INFORMATION (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
								  ("pCmdQue->uOutCmdNum increased %d", pCmdQue->uOutCmdNum));*/
		}
		MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);

		if (bSend)
		{
#ifdef WATCHDOG_COMMANDS
            /* Check the type of the command */
            if (pTxn->pData)
            {
                uOpcode = pTxn->pData[1];
            }
            else
            {
                MCPF_REPORT_ERROR (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
								  ("cmdQ_add : pTxn->pData null"));
            }
               
            if (uOpcode != CCMA_HCI_HOST_NUM_COMPLETED_PACKETS)
            {
                /* start the watchdogTimer timer. */
                MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
                pCmdQue->unackedCommand = MCP_TRUE;
                MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);
#if 0
                pCmdQue->watchdogTimer = mcpf_timer_start(pCmdQue->hMcpf,COMMAND_COMPL_WAIT_TIME, 0,
                                                          pCmdQue->watchDogTimerCB, (handle_t)pCmdQue);
#endif
            }
#endif
			/* Flow control allows to send the command */
            MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
			eRes = mcpf_trans_TxData(pCmdQue->hMcpf, pCmd->pTxn);
			if (eRes == RES_OK)
			{
				EMcpfRes eResQ;

				eResQ = mcpf_que_Enqueue (pCmdQue->hSentQue, pCmd);  /* add to queue of commands waiting for completion */

				if (eResQ != RES_OK)
				{
					MCPF_REPORT_ERROR (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
									  ("cmdQueue_add: completion queue is full\n"));
				}
			}
			else
			{
				MCPF_REPORT_ERROR (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
								  ("cmdQueue_add: cmd send error\n"));
			}
            MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);
		}
		else
		{
			/* Flow control does not allow to send the command, en-queue it */
            MCPF_REPORT_WARNING (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
                                 ("cmdQueue_add: flow control command not sent out %d max %d ",
                                  pCmdQue->uOutCmdNum ,pCmdQue->uOutCmdMaxNum));
			MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
			eRes = mcpf_que_Enqueue (pCmdQue->hOutQue, pCmd);	/* add to command output queue tail */
			MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);

			if (eRes != RES_OK)
			{
				mcpf_mem_free_from_pool (pCmdQue->hMcpf, pCmd); 

				MCPF_REPORT_ERROR (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
								  ("cmdQueue_add: cmd queue is full\n"));
			}
		}
	}
	else
	{
		MCPF_REPORT_ERROR (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
						  ("cmdQueue_add: cmd node allocation failed\n"));
	}

	return eRes;
}


/** 
 * \fn     cmdQueue_complete 
 * \brief  Command queue complete event
 * 
 * Process command complete event: sent the next command from queue and 
 * invoke completion call back funciton
 * 
 * \note
 * \param	pCmdQue - pointer to Command Queue object
 * \param	uEvtOpcode   - event opcode
 * \param	uEvtParamLen - lenght of event parameters
 * \param	pEvtParams   - pointer to event parameters
 * \return 	result of operation: RES_OK, RES_PENDING or RES_ERROR
 * \sa     	cmdQueue_add
 */ 
static EMcpfRes cmdQueue_complete (TCmdQueue   	*pCmdQue,
								   const McpU8	uEvtOpcode,
								   const McpU8	uEvtParamLen,
								   McpU8		*pEvtParams,
                                   McpBool          *needFreeMem)
{
	TCmdNode 	*pCmd;
	EMcpfRes	eRes 	= RES_ERROR;
    *needFreeMem = MCP_TRUE;
#ifdef WATCHDOG_COMMANDS
    pCmdQue->unackedCommand = MCP_FALSE;
#endif    
	/* Fetch the next command to send from output command queue */
	MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
	pCmd = (TCmdNode *) mcpf_que_Dequeue (pCmdQue->hOutQue);
	if (!pCmd)
	{
		/* The output command queue is empty, nothing to send, decrement flow control counter */
		if (pCmdQue->uOutCmdNum > 0)
		{
		    pCmdQue->uOutCmdNum--;
		}
		else
		{
		    MCPF_REPORT_ERROR (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
		                                         ("cmdQueue_complete: trying to decrement zero uOutCmdNum!\n"));
		}
/*        MCPF_REPORT_INFORMATION (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
								  ("cmdQueue_complete: pCmdQue->uOutCmdNum decreased %d", pCmdQue->uOutCmdNum));*/
	}
	MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);

	if (pCmd)
	{
		eRes = mcpf_trans_TxData(pCmdQue->hMcpf, pCmd->pTxn);
		if (eRes == RES_OK)
		{
			MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
			eRes = mcpf_que_Enqueue (pCmdQue->hSentQue, pCmd);   /* items waiting for completion */
			MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);

			if (eRes != RES_OK)
			{
				mcpf_mem_free_from_pool(pCmdQue->hMcpf, pCmd); 
				MCPF_REPORT_ERROR (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
								  ("cmdQueue_complete: sent cmd queue is full\n"));
			}
		}
		else
		{
			cmdQueue_freeCmdTxn (pCmdQue, pCmd);
			MCPF_REPORT_ERROR (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
							  ("cmdQueue_add: cmd send error\n"));
		}
	}

	/* Dequeue command pending for completion and invoke completion function */
	MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
	pCmd = mcpf_que_Dequeue (pCmdQue->hSentQue);
	MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);

	if (pCmd)
	{
		if (pCmd->fCmdComplCb)
		{
			THciaCmdComplEvent tEvent;

			tEvent.eResult 	  	= RES_OK;
			tEvent.uEvtOpcode 	= uEvtOpcode;
			tEvent.uEvtParamLen = uEvtParamLen;
			tEvent.pEvtParams 	= pEvtParams;

			pCmd->fCmdComplCb (pCmd->hCbParam, &tEvent);
            *needFreeMem = pCmd->needFreeMem;

			mcpf_mem_free_from_pool(pCmdQue->hMcpf, pCmd); 
			eRes = RES_OK;
		}
		else
		{
			/* call back function is not used to deliver completion */
			eRes = RES_PENDING;
			mcpf_mem_free_from_pool(pCmdQue->hMcpf, pCmd); 
		}
	}
	else
	{
		eRes = RES_ERROR;
		MCPF_REPORT_ERROR (pCmdQue->hMcpf, TRANS_MODULE_LOG, 
		    ("cmdQueue_complete: sent queue (handle 0x%08x) is unexpectedly empty\n",
		     (McpU32)pCmdQue->hSentQue));
	}

	return eRes;
}


/** 
 * \fn     cmdQueue_freeCmdTxn 
 * \brief  Free command node and linked transaction structure
 * 
 * Frees command node to pool and txn structure with buffer to pool
 * 
 * \note
 * \param	pCmdQue - pointer to command queue object
 * \param	pCmd    - pointer to command node
 * \return 	void
 * \sa     	cmdQueue_Destroy
 */ 
static void cmdQueue_freeCmdTxn (const TCmdQueue *pCmdQue, TCmdNode *pCmd)
{
	if (pCmd)
	{
		if (pCmd->pTxn)
		{
			mcpf_txnPool_FreeBuf(pCmdQue->hMcpf, pCmd->pTxn);
		}
		mcpf_mem_free_from_pool(pCmdQue->hMcpf, pCmd); 
	}
}


/** 
 * \fn     cmdQueue_flush
 * \brief  Resets the command queue
 * 
 * Flushes the command otput and pending queues, resets the flow control
 * 
 * \note
 * \param	pCmdQue - pointer to command queue object
 * \return 	void
 * \sa     	cmdQueue_Destroy
 */ 
static void cmdQueue_flush (TCmdQueue *pCmdQue)
{
	TCmdNode *pCmd;

	do
	{
		MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
		pCmd = (TCmdNode *) mcpf_que_Dequeue (pCmdQue->hOutQue);
		MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);

		if (pCmd)
		{
			cmdQueue_freeCmdTxn (pCmdQue, pCmd);
		}

	} while (pCmd);

	do
	{
		MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
		pCmd = (TCmdNode *) mcpf_que_Dequeue (pCmdQue->hSentQue);
		MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);

		if (pCmd)
		{
			mcpf_mem_free_from_pool (pCmdQue->hMcpf, pCmd); 
		}

	} while (pCmd);

	pCmdQue->uOutCmdNum = 0;
}

/** 
 * \fn     clientRegister_Create 
 * \brief  Create command queue object
 * 
 * Allocates memory and queue
 * 
 * \note
 * \param	pHcia - pointer to HCIA object
 * \return 	void
 * \sa     	cmdQueue_Destroy
 */ 
static TClientRegister * clientRegister_Create (const THciaObj 	*pHcia,
												const McpU8		uHciTypeMin,
												const McpU8		uHciTypeMax)
{
	TClientRegister * 	pClientReg;
	McpU32				uInx;

	pClientReg = mcpf_mem_alloc (pHcia->hMcpf, sizeof(TClientRegister));
	MCPF_Assert(pClientReg);
	mcpf_mem_zero (pHcia->hMcpf, pClientReg, sizeof(TClientRegister));

	pClientReg->hMcpf 	  = pHcia->hMcpf;
	pClientReg->hHcia 	  = (handle_t) pHcia;
	pClientReg->uListNum  = uHciTypeMax - uHciTypeMin + 1;
	pClientReg->uIndexOfs = uHciTypeMin; 

	pClientReg->pListArray = mcpf_mem_alloc (pHcia->hMcpf, (McpU16) (sizeof(MCP_DL_LIST_Node) * pClientReg->uListNum));
	MCPF_Assert(pClientReg->pListArray);
	mcpf_mem_zero (pHcia->hMcpf, pClientReg->pListArray, sizeof(MCP_DL_LIST_Node) * pClientReg->uListNum);

	pClientReg->pChanStateArray = mcpf_mem_alloc (pHcia->hMcpf, (McpU16) (sizeof(McpBool) * pClientReg->uListNum));
	MCPF_Assert(pClientReg->pListArray);
	mcpf_mem_zero (pHcia->hMcpf, pClientReg->pChanStateArray, sizeof(McpBool) * pClientReg->uListNum);

	for ( uInx=0; uInx < pClientReg->uListNum; uInx++)
	{
		MCP_DL_LIST_InitializeHead (&pClientReg->pListArray[uInx]);
	}

	return (pClientReg);
}



/** 
 * \fn     clientRegister_Destroy 
 * \brief  Destroy Client Register object
 * 
 * Deallocates memory and list objects
 * 
 * \note
 * \param	pHcia - pointer to HCIA object
 * \return 	void
 * \sa     	cmdQueue_Destroy
 */ 
static void clientRegister_Destroy (TClientRegister * pClientReg)
{
	McpU32	uListInx;


	MCPF_ENTER_CRIT_SEC (pClientReg->hMcpf);

	/* Free all the nodes from the lists */
	for (uListInx=0; uListInx < pClientReg->uListNum; uListInx++)
	{
		MCP_DL_LIST_Node * pNode, * pNext;

		for (pNode = pClientReg->pListArray[uListInx].next; pNode != &pClientReg->pListArray[uListInx]; )
		{
			pNext = pNode->next;
			mcpf_mem_free (pClientReg->hMcpf, pNode);
			pNode = pNext;
		}
	}

	MCPF_EXIT_CRIT_SEC (pClientReg->hMcpf);

	mcpf_mem_free (pClientReg->hMcpf, pClientReg->pListArray);
	mcpf_mem_free (pClientReg->hMcpf, pClientReg->pChanStateArray);
	mcpf_mem_free (pClientReg->hMcpf, pClientReg);
}

/** 
 * \fn     clientRegister_add 
 * \brief  Add the client
 * 
 * Adds the client item to the Client Register object
 * 
 * \note
 * \param	pClientReg - pointer to ClientRegister object
 * \param	uHciMsgType - register for message of HCI type
 * \param	uHciOpcode - register for message of HCI opcode
 * \param	fCallBack  - client call back function to deliver the message
 * \param	hCbParam   - client parameter to pass to client call back when invoked
 * \return 	status of operation RES_OK or RES_ERROR
 * \sa     	clientRegister_delete
 */ 
static EMcpfRes clientRegister_add (TClientRegister 	  	*pClientReg, 
									const McpU8 			uHciMsgType,
									const McpU16 			uHciOpcode,
									const TI_TransRxIndCb 	fCallBack,
									const handle_t 			hCbParam)
{
	McpU32	  uList = uHciMsgType - pClientReg->uIndexOfs;
	McpBool	  listEmpty;
	EMcpfRes  eRes = RES_OK;

	if (uList < pClientReg->uListNum)
	{
		TClientNode * pClient;

		pClient = mcpf_mem_alloc (pClientReg->hMcpf, sizeof(TClientNode));
		MCPF_Assert(pClient);
		mcpf_mem_zero (pClientReg->hMcpf, pClient, sizeof(TClientNode));

		pClient->uOpcode  = uHciOpcode;
		pClient->fRxIndCb = fCallBack;
		pClient->hCbParam = hCbParam;

		MCPF_ENTER_CRIT_SEC (pClientReg->hMcpf);

		listEmpty = MCP_DL_LIST_IsEmpty (&pClientReg->pListArray[uList]);

		if (uHciOpcode == HCIA_ANY_OPCODE)
		{
			MCP_DL_LIST_InsertTail (&pClientReg->pListArray[uList], &pClient->tHead);
		}
		else
		{
			MCP_DL_LIST_InsertHead (&pClientReg->pListArray[uList], &pClient->tHead);
		}
		MCPF_EXIT_CRIT_SEC (pClientReg->hMcpf);

		if (listEmpty)
		{
			eRes = clientRegister_openTransChan (pClientReg, 
												 uHciMsgType, 
												 hciaRxInd, 
												 (THciaObj *) pClientReg->hHcia);
		}
	}
	else
	{
		MCPF_REPORT_ERROR(pClientReg->hMcpf, TRANS_MODULE_LOG, 
						  ("clientRegister_add: invalid msgType=%u max=%u", uHciMsgType, pClientReg->uListNum));
		eRes = RES_ERROR;
	}

	return eRes;
}


/** 
 * \fn     clientRegister_delete 
 * Delete the client item from the Client Register object
 * 
 * \note
 * \param	pClientReg - pointer to ClientRegister object
 * \param	uHciMsgType - de-register from message of HCI type
 * \param	uHciOpcode - de-register from message of HCI opcode
 * \return 	status of operation RES_OK or RES_ERROR
 * \sa     	clientRegister_add
 */ 
static EMcpfRes clientRegister_delete (TClientRegister *pClientReg, 
									   const McpU8 		uHciMsgType,
									   const McpU16 	uHciOpcode)
{
	McpU32	  uList = uHciMsgType - pClientReg->uIndexOfs;
	McpBool	  listEmpty;
	EMcpfRes  eRes  = RES_ERROR;

	if (uList < pClientReg->uListNum)
	{
		MCP_DL_LIST_Node  *pNode;

		MCPF_ENTER_CRIT_SEC (pClientReg->hMcpf);

		/* traverse the list to find the item */
		MCP_DL_LIST_ITERATE(&pClientReg->pListArray[uList], pNode)
		{
			if ( ((TClientNode *)pNode)->uOpcode == uHciOpcode)
			{
				/* Item found */
				MCP_DL_LIST_RemoveNode (pNode);
				eRes = RES_OK;
				break;
			}
		}
		listEmpty = MCP_DL_LIST_IsEmpty (&pClientReg->pListArray[uList]);
		MCPF_EXIT_CRIT_SEC (pClientReg->hMcpf);

		if (eRes == RES_OK)
		{
			mcpf_mem_free (pClientReg->hMcpf, pNode);
		}

		/* Close the channel if the client list for HCI type is emptpy.
		 * Event channel is always left opened as used for HCIA command flow control 
		 */
		if (listEmpty && (uHciMsgType != HCI_PKT_TYPE_EVENT))
		{
			eRes = clientRegister_closeTransChan (pClientReg, uHciMsgType);
		}
	}
	else
	{
		MCPF_REPORT_ERROR(pClientReg->hMcpf, TRANS_MODULE_LOG, 
						  ("clientRegister_delete: invalid msgType=%u max=%u", uHciMsgType, pClientReg->uListNum));
	}

	return eRes;
}

/** 
 * \fn     clientRegister_searchAndInvoke 
 * \brief  Search for client node and invoke the call back function
 * 
 * Searches for client node by type & opcode and, if found, invokes the call back function
 * 
 * \note
 * \param	pClientReg - pointer to ClientRegister object
 * \param	uHciMsgType - de-register from message of HCI type
 * \param	uHciOpcode - de-register from message of HCI opcode
 * \return 	status of operation RES_OK or RES_ERROR
 * \sa     	clientRegister_delete
 */ 
static EMcpfRes clientRegister_searchAndInvoke (TClientRegister 	*pClientReg, 
												const McpU8 		uHciMsgType,
												const McpU16 		uHciOpcode,
												handle_t			hTrans,
												TTxnStruct 			*pTxn)
{
	McpU32	  uList  = uHciMsgType - pClientReg->uIndexOfs;
	EMcpfRes  eRes   = RES_ERROR;
	McpBool	  bFound = MCP_FALSE;

	if (uList < pClientReg->uListNum)
	{
		MCP_DL_LIST_Node  *pNode;

		MCPF_ENTER_CRIT_SEC (pClientReg->hMcpf);

		/* traverse the list to find the item */
		MCP_DL_LIST_ITERATE(&pClientReg->pListArray[uList], pNode)
		{
			if ( ((TClientNode *)pNode)->uOpcode == uHciOpcode   ||  
				 ((TClientNode *)pNode)->uOpcode == HCIA_ANY_OPCODE)
			{
				/* Item found */
				bFound = MCP_TRUE;
				break;
			}
		}
		MCPF_EXIT_CRIT_SEC (pClientReg->hMcpf);

		if (bFound)
		{
			TClientNode * pClient =  (TClientNode *) pNode;

			eRes = pClient->fRxIndCb (hTrans, pClient->hCbParam, pTxn);
		}
		else
		{
			MCPF_REPORT_ERROR(pClientReg->hMcpf, TRANS_MODULE_LOG, 
							  ("clientRegister_searchAndInvoke: item not found type=%u opcode=%u", uHciMsgType, uHciOpcode));
		}
	}
	else
	{
		MCPF_REPORT_ERROR(pClientReg->hMcpf, TRANS_MODULE_LOG, 
						  ("clientRegister_searchAndInvoke: invalid msgType=%u ofs=%u max=%u", 
						   uHciMsgType, pClientReg->uIndexOfs, pClientReg->uListNum));
	}

	return eRes;
}


/** 
 * \fn     clientRegister_openTransChan 
 * \brief  Open transport channel
 * 
 * Opens transport channel and update channel state to opened
 * 
 * \note
 * \param	pClientReg  - pointer to ClientRegister object
 * \param	uHciMsgType - register for message of HCI type
 * \param	fCallBack   - client call back function to deliver the message
 * \param	hCbParam    - client parameter to pass to client call back when invoked
 * \return 	status of operation RES_OK or RES_ERROR
 * \sa     	clientRegister_closeTransChan
 */ 
static EMcpfRes clientRegister_openTransChan (TClientRegister 	  		*pClientReg, 
											  const McpU8 				uHciMsgType,
											  const TI_TransRxIndCb 	fCallBack,
											  const handle_t 			hCbParam)
{
	McpU32	  uIndx = uHciMsgType - pClientReg->uIndexOfs;
	EMcpfRes  eRes  = RES_ERROR;

	if (uIndx < pClientReg->uListNum)
	{
		if (!pClientReg->pChanStateArray[uIndx])
		{
			eRes = mcpf_trans_ChOpen(pClientReg->hMcpf, uHciMsgType, fCallBack, hCbParam);
			pClientReg->pChanStateArray[uIndx] = MCP_TRUE;
		}
        else
        {
            eRes = RES_OK;
        }
	}
	else
	{
		MCPF_REPORT_ERROR(pClientReg->hMcpf, TRANS_MODULE_LOG, 
						  ("clientRegister_add: invalid msgType=%u max=%u", uHciMsgType, pClientReg->uListNum));
		eRes = RES_ERROR;
	}

	return eRes;
}

/** 
 * \fn     clientRegister_closeTransChan 
 * \brief  Close transport channel
 * 
 * Closes transport channel and update channel state to closed
 * 
 * \note
 * \param	pClientReg  - pointer to ClientRegister object
 * \param	uHciMsgType - register for message of HCI type
 * \return 	status of operation RES_OK or RES_ERROR
 * \sa     	clientRegister_closeTransChan
 */ 
static EMcpfRes clientRegister_closeTransChan (TClientRegister *pClientReg, 
											   const McpU8 		uHciMsgType)
{
	McpU32	  uIndx = uHciMsgType - pClientReg->uIndexOfs;
	EMcpfRes  eRes  = RES_ERROR;

	if (uIndx < pClientReg->uListNum)
	{
		if (pClientReg->pChanStateArray[uIndx])
		{
			eRes = mcpf_trans_ChClose(pClientReg->hMcpf, uHciMsgType);
			pClientReg->pChanStateArray[uIndx] = MCP_FALSE;
		}
	}
	else
	{
		MCPF_REPORT_ERROR(pClientReg->hMcpf, TRANS_MODULE_LOG, 
						  ("clientRegister_add: invalid msgType=%u max=%u", uHciMsgType, pClientReg->uListNum));
		eRes = RES_ERROR;
	}

	return eRes;
}

/** 
 * \fn     hciaTxCompl
 * \brief  HCIA transmit complete function
 * 
 *  The function frees Txn structure with data buffer.
 * 
 * \note
 * \param	hCbHandle  	- handle parameter form sent packet TxnQ struncture 
 * \param	pTxn 		- pointer to the transaction structure of the sent packet
 * \return 	void
 * \sa     	HCIA_SendCommand/Data
 */
static void hciaTxCompl (handle_t hCbHandle, void *pTxn)
{
	THciaObj  * pHcia = (THciaObj  *) hCbHandle;

	mcpf_txnPool_FreeBuf(pHcia->hMcpf, (TTxnStruct*) pTxn);

}
/* typedef void (*mcpf_timer_cb)(handle_t hCaller, McpUint uTimerId);*/

/**
 * \fn HciaCommandWatchdog
 *  \brief    This function is called COMMAND_COMPL_WAIT_TIME
 *     seconds after the last command issued.
 * \note
 * \param	timer  	- the expired timer
 * \return 	void
 * \sa     	HciaCommandWatchdog
 */
static void HciaCommandWatchdog(handle_t hCaller, McpUint uTimerId)
{

    MCP_UNUSED_PARAMETER(hCaller);
    MCP_UNUSED_PARAMETER(uTimerId);
    
#ifdef WATCHDOG_COMMANDS
    TCmdQueue 	*pCmdQue = (TCmdQueue 	*)hCaller;

    if (pCmdQue->unackedCommand == MCP_TRUE)
    {
        MCPF_OS_REPORT(("HCIA: HCI Command was not acknowledged with an event"));
     /*   HCI_TransportError(HCI_TRANSERROR_CMD_TIMEOUT);*/
        /* currently action done in HCI, not tools in HCIA. Add it here???? tbd */
        MCPF_ENTER_CRIT_SEC (pCmdQue->hMcpf);
        pCmdQue->unackedCommand = MCP_FALSE;
        pCmdQue->watchdogTimer = NULL;
        MCPF_EXIT_CRIT_SEC (pCmdQue->hMcpf);
    }
#endif
}



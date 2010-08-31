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

 

/** \file   mcp_uartBusDrv.c 
 *  \brief  HCI UART Bus Driver module implementation
 * 
 *  HCI UART Bus Driver provides:
 * - internal data structure initialization
 * - OS HAL UART port opening and configuration
 * - accomplishment of transmit transaction
 * - processing of transmit completion indication event from OS HAL UART
 * - handling of read ready indication event from OS HAL UART
 * - Rx congestion state handling
 * 
 * The HCI UART Bus Driver is OS agnostic and Bus dependent module.  
 * 
 *
 *  \see    BusDrv.h, mcp_hal_uart.[ch]
 */


#include "mcpf_mem.h"
#include "mcpf_services.h"
#include "mcpf_report.h"
#include "mcp_hal_os.h"
#include "mcp_hal_log.h"
#include "mcp_defs.h"
#include "TxnDefs.h"
#include "BusDrv.h"
#include "mcp_hciDefs.h"
#include "mcp_IfSlpMng.h"
#include "mcp_txnpool.h"
#include "mcp_hal_st.h"


/************************************************************************
 * Defines
 ************************************************************************/

#define RX_LEN_ERROR			0xFFFF

/************************************************************************
 * Types
 ************************************************************************/

/* Packet process result types */
typedef enum
{
    PktType_Normal,
	PktType_IfSlpMng,
	PktType_Err
} EPktType;


/* The busDrv module Object */
typedef struct _TBusDrvObj
{
    	handle_t	     hMcpf;		   	    

	TBusDrvTxnDoneCb fTxnDoneCb;    /* The callback to call upon full transaction completion */
	handle_t         hCbHandle;     /* The callback handle */
	TI_BusDvrRxIndCb fRxIndCb;      /* The callback to invoke after HCI packet has been received */
	handle_t 		 hRxIndHandle;
	TI_TxnEventHandlerCb fEventCb;  /* Event callback function (init/destroy complete, error ind) */
	handle_t         hEventCbHandle;/* The event handler callback parameter */
	TBusDrvCfg       busConf;		/* Bus driver configuration */
	handle_t		 hHalUart;
	handle_t       	 hIfSlpMng;	 	/* Interface Sleep Manager handle */

	/* Tx state */
	TTxnStruct 		*pOutTxn;       /* The transaction currently being transmitted */
	McpS16         	 iOutBufNum;   	/* Current transaction buffer number for transmission */
	McpU16        	 uOutLen;   	/* Current number of bytes sent from buffer */
	McpU8       	*pOutData;   	/* Current data pointer for transmission */

	/* Rx state */
	TTxnStruct 		*pInTxn;        /* The transaction currently being received */
	EBusDrvRxState   eRxState;      /* Current Rx state */ 
	McpU16		 	 uInLen;      	/* Expected input length (header/data) */ 
	McpU8       	*pInData;   	/* Current data pointer for transmission */
	McpU16        	 uInDataLen;   	/* Length of accumulated data */
	McpBool        	 restart;   	/* Restart receiver exiting from congestion state */

	TBusDrvStat		 stat;			/* Statistics counters */

} TBusDrvObj;


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/

static void drvEventHandler(handle_t hBusDrv, EHalStEvent eEvent);
static void drvTxnDone (TBusDrvObj  *pBusDrv);
static ETxnStatus startTxnSend (TBusDrvObj * pBusDrv, TTxnStruct * pTxn);
static ETxnStatus startIfSlpMngSend (TBusDrvObj * pBusDrv, TTxnStruct * pTxn);
static ETxnStatus drvSendData (TBusDrvObj * pBusDrv);
static void drvRxInd (TBusDrvObj *pBusDrv);
static EMcpfRes prepareNextPacketRx (TBusDrvObj *pBusDrv);
/* static McpU16 getHeaderLenFromPktType (McpU8 pktType); */
static void resetRxState (TBusDrvObj *pBusDrv);
static void transportError (TBusDrvObj *pBusDrv, ETxnErrId);
static void setIfSlpMngTxn (TTxnStruct * pTxn, McpU8 pktType);
static EPktType processPktType (McpU8 pktType, McpU16 *pLen);
static McpU16 getDataLenFromHeader (McpU8 pktType, McpU8 *pHeader);

/* Debug stuff */
#ifdef TRAN_DBG
TBusDrvObj * g_pUartBusDrvObj;
#endif


/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/** 
 * \fn     busDrv_Create 
 * \brief  Create the module
 * 
 * Create and clear the bus driver's object, 
 * allocates transaction structure & buf for receiver.
 * 
 * \note   
 * \param  hMcpf - Handle to MCPF
 * \return Handle of the allocated object, NULL if allocation failed 
 * \sa     busDrv_Destroy
 */ 
handle_t busDrv_Create (const handle_t hMcpf)
{
    TBusDrvObj *pBusDrv;

    pBusDrv = (TBusDrvObj *) mcpf_mem_alloc(hMcpf, sizeof(TBusDrvObj));
    if (pBusDrv == NULL)
    {
        return NULL;
    }
#ifdef TRAN_DBG
	g_pUartBusDrvObj = pBusDrv;
#endif

    mcpf_mem_zero(hMcpf, pBusDrv, sizeof(TBusDrvObj));
    
    pBusDrv->hMcpf 	= hMcpf;

	pBusDrv->pInTxn = mcpf_txnPool_AllocBuf (hMcpf, POOL_RX_MAX_BUF_SIZE);
	if (pBusDrv->pInTxn == NULL)
	{
		mcpf_mem_free (pBusDrv->hMcpf, pBusDrv);
		return NULL;
	}

	pBusDrv->hHalUart = HAL_ST_Create (pBusDrv->hMcpf, "Transport");
	if (pBusDrv->hHalUart == NULL)
	{
		mcpf_txnPool_FreeBuf (pBusDrv->hMcpf, pBusDrv->pInTxn);
		mcpf_mem_free (pBusDrv->hMcpf, pBusDrv);
		return NULL;
	}
    return (handle_t) pBusDrv;
}


/** 
 * \fn     busDrv_Destroy
 * \brief  Destroy the module. 
 * 
 * Close bus driver, free rx transaction structure and buffer,
 * free the module's object.
 * 
 * \note   
 * \param  The module's object
 * \return RES_OK on success or RES_ERROR on failure 
 * \sa     busDrv_Create
 */ 
EMcpfRes busDrv_Destroy (handle_t hBusDrv)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;

	if (pBusDrv->hHalUart)
	{
		HAL_ST_Destroy (pBusDrv->hHalUart);
	}
	if (pBusDrv->pInTxn)
	{
		mcpf_txnPool_FreeBuf (pBusDrv->hMcpf, pBusDrv->pInTxn);     
	}
	if (pBusDrv)
    {
        mcpf_mem_free (pBusDrv->hMcpf, pBusDrv);     
    }
    return RES_OK;
}


/** 
 * \fn     busDrv_Init
 * \brief  Init bus driver 
 * 
 * Init module parameters.

 * \note   
 * \param  hBusDrv - The module's handle
 * \return void
 * \sa     
 */ 
void busDrv_Init (handle_t hBusDrv)
{
	MCPF_UNUSED_PARAMETER(hBusDrv);
}


/** 
 * \fn     busDrv_ConnectBus
 * \brief  Configure bus driver
 * 
 * Called by TxnQ.
 * Configure the bus driver with its connection configuration (such as baud-rate, bus width etc) 
 * and create HAL UART port. 
 * Done once upon init (and not per functional driver startup). 
 * 
 * \note   
 * \param  hBusDrv    - the bus driver handler
 * \param  pBusDrvCfg - A union used for per-bus specific configuration. 
 * \param  fCbFunc    - CB function for Async transaction completion (after all txn parts are completed).
 * \param  hCbArg     - The CB function handle
 * \param  fRxIndCb   - Receive indication callback function
 * \param  fEventCb   - Event indication callback function
 * \param  hEventHandle - interface sleep manager handle
 * \return RES_OK / RES_ERROR
 * \sa     
 */ 
EMcpfRes busDrv_ConnectBus (handle_t        			hBusDrv, 
                             const TBusDrvCfg       	*pBusDrvCfg,
                             const TBusDrvTxnDoneCb 	fTxComplCb,
                             const handle_t        		hCbArg,
                             const TBusDrvTxnDoneCb		fConnectCbFunc,
							 const TI_BusDvrRxIndCb   	fRxIndCb,
							 const handle_t 			hRxIndHandle, 
							 const TI_TxnEventHandlerCb fEventCb,
							 const handle_t 			hEventHandle,
							 const handle_t				hIfSlpMng)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
    int         iStatus;
	TBusDrvTxnDoneCb unused = (TBusDrvTxnDoneCb)fConnectCbFunc;

	MCPF_UNUSED_PARAMETER(unused);

    /* Save the parameters (TxnQ callback for TxnDone events, and block-size) */
    pBusDrv->fTxnDoneCb    = fTxComplCb;
    pBusDrv->hCbHandle     = hCbArg;

	pBusDrv->fRxIndCb 	   = fRxIndCb;
	pBusDrv->hRxIndHandle  = hRxIndHandle;

	pBusDrv->fEventCb 	   = fEventCb;
	pBusDrv->hEventCbHandle= hEventHandle;

	pBusDrv->hIfSlpMng	   = hIfSlpMng;

	pBusDrv->busConf.tUartCfg.uBaudRate = pBusDrvCfg->tUartCfg.uBaudRate;
	pBusDrv->busConf.tUartCfg.uFlowCtrl = pBusDrvCfg->tUartCfg.uFlowCtrl;

	/* prepare to receive the first byte - packet type */
	pBusDrv->eRxState = BUSDRV_STATE_RX_PKT_TYPE;

	pBusDrv->uInLen	  = HCI_PKT_TYPE_LEN;			   	/* One byte of packet type is expected */
	pBusDrv->pInData  = pBusDrv->pInTxn->aBuf[0];	/* Current data pointer for Rx */
	pBusDrv->uInDataLen = 0;

	/* Initialize HAL Uart module */
    iStatus = HAL_ST_Init (pBusDrv->hHalUart,
							 drvEventHandler,
							 pBusDrv,
							 (THalSt_PortCfg *)&pBusDrvCfg->tUartCfg,
							 NULL,
							 pBusDrv->pInData,
							 pBusDrv->uInLen,
							 MCP_FALSE);
    if (iStatus != RES_OK) 
    {
        MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
            ("%s: HAL Uart initialization failed\n", __FUNCTION__));
        return RES_ERROR;
    }
	return RES_OK;
}


/** 
 * \fn     busDrv_DisconnectBus
 * \brief  Disconnect UART driver
 * 
 * Called by TxnQ. Disconnect the HAL UART port.
 *  
 * \note   
 * \param  hBusDrv - The module's object
 * \return RES_OK / RES_ERROR
 * \sa     
 */ 
EMcpfRes busDrv_DisconnectBus (handle_t hBusDrv)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;


	if (pBusDrv->hHalUart)
	{
		HAL_ST_Deinit (pBusDrv->hHalUart);
	}

    return RES_OK;
}


/** 
 * \fn     busDrv_Transact
 * \brief  Send transaction processing 
 * 
 * Called by the TxnQ module to initiate a new transaction.
 * Prepare the transaction buffers and send them one by one to the HAL UART.
 * 
 * \note   It's assumed that this function is called only when idle (i.e. previous Txn is done).
 * \param  hBusDrv - The bus driver handle
 * \param  pTxn    - The transaction object 
 * \return PENDING (all send transactoions are asynch), ERROR if failed
 * \sa
 */ 
ETxnStatus busDrv_Transact (handle_t hBusDrv, TTxnStruct *pTxn)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
	ETxnStatus  eStatus;

#if TI_BUSDRV_DBG
	if (pBusDrv->pOutTxn)
	{
        MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
						  ("%s: Tx is not idle!\n", __FUNCTION__));
	}
#endif

	/* Check transaction single step bit: normal or single step (IfSlpMng) transaction */
	if (!TXN_PARAM_GET_SINGLE_STEP(pTxn))
	{
		/* Normal transaction */
		eStatus = startTxnSend (pBusDrv, pTxn);
	}
	else
	{
		/* transaction initiated by interface sleep manager */
		eStatus = startIfSlpMngSend (pBusDrv, pTxn);
	}
	pBusDrv->stat.Tx++;

	if (eStatus == TXN_STATUS_ERROR)
	{
		/* Transport Error */
        MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
						  ("%s: Tx failed!\n", __FUNCTION__));
		pBusDrv->stat.TxErr++;
		transportError (pBusDrv, Txn_TxErr);
	}

	return eStatus;
}


/** 
 * \fn     busDrv_BufAvailable
 * \brief  Buffer available indication 
 * 
 * The function indicates the driver to resume Rx processing 
 * if Rx is in congestion state
 * 
 * \note
 * \param  hBusDrv - the bus driver handle
 * \return void
 * \sa
 */ 
void busDrv_BufAvailable (handle_t hBusDrv)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
	ETxnStatus  eStatus;
	McpBool		congested = MCP_FALSE;

	MCPF_ENTER_CRIT_SEC (pBusDrv->hMcpf);
	if (pBusDrv->eRxState == BUSDRV_STATE_RX_CONGESTION)
	{
		pBusDrv->eRxState = BUSDRV_STATE_RX_PKT_TYPE;
		congested = MCP_TRUE;
	}
	MCPF_EXIT_CRIT_SEC (pBusDrv->hMcpf);

	if (congested)
	{
        pBusDrv->stat.BufAvail++;
		if (pBusDrv->pInTxn)
		{
			/* Received packet is not delivered yet, invoke rx ind callback */
			eStatus = pBusDrv->fRxIndCb (pBusDrv->hRxIndHandle, (void *) pBusDrv->pInTxn);
			if (eStatus != TXN_STATUS_OK) 
			{
                pBusDrv->eRxState = BUSDRV_STATE_RX_CONGESTION;
				pBusDrv->stat.RxIndErr++;
				return;
			}
		}
		/* Received packet has been forwarded to upper layer, allocate new rx buffer */
		if (prepareNextPacketRx (pBusDrv) != RES_OK)
		{
            pBusDrv->eRxState = BUSDRV_STATE_RX_CONGESTION;
			return;
		}
		pBusDrv->restart  = MCP_TRUE;

		HAL_ST_RestartRead (pBusDrv->hHalUart);
	}
}


/** 
 * \fn     busDrv_RxReset
 * \brief  Reset Rx state
 * 
 * The function resets bus driver and HAL UART Rx buffer, starts a new receiption
 * 
 * \note   The function is to be called from the Rx (HAL UART) context, otherwise 
 * 		   critical section is to be provied by caller
 * \param  hBusDrv - the bus driver handle
 * \return operation result: OK or ERROR
 * \sa
 */ 
EMcpfRes busDrv_RxReset (handle_t hBusDrv)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
	EMcpfRes  result;

	resetRxState (pBusDrv);
	result = HAL_ST_ResetRead (pBusDrv->hHalUart, pBusDrv->pInData, pBusDrv->uInLen);

	return result;
}


/** 
 * \fn     busDrv_GetStat
 * \brief  Gets bus driver statistics and state
 * 
 * The function returns the driver statistics and Rx state
 * 
 * \note
 * \param  hBusDrv - the bus driver handle
 * \param  pStat   - pointer to driver statistics to fill couters
 * \param  pState  - pointer to state variable to return Rx state
 * \return void
 * \sa
 */ 
void busDrv_GetStat (const handle_t hBusDrv, TBusDrvStat  * pStat, EBusDrvRxState * pState)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;

	mcpf_mem_copy(pBusDrv->hMcpf, pStat, &pBusDrv->stat, sizeof(TBusDrvStat));
	*pState = pBusDrv->eRxState;
}

/** 
 * \fn     busDrv_SetSpeed
 * \brief  Sets bus driver speed
 * 
 * The function sets bus driver baudrate
 * 
 * \note
 * \param  hBusDrv  - the bus driver handle
 * \param  baudrate - baudrate to set
 * \param  pState   - pointer to state variable to return Rx state
 * \return void
 * \sa
 */ 
void busDrv_SetSpeed (const handle_t hBusDrv, const McpU16 baudrate)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
	TBusDrvCfg 	busConf;

	busConf.tUartCfg.uBaudRate = baudrate;
	HAL_ST_Set (pBusDrv->hHalUart, (THalSt_PortCfg *)&busConf.tUartCfg);
}

/* tbd */
EMcpfRes busDrv_Reset (handle_t hBusDrv, const McpU16 baudrate)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;
    
    return HAL_ST_Port_Reset(pBusDrv->hHalUart,  pBusDrv->pInData, pBusDrv->uInLen, baudrate);
}

/** 
 * \fn     drvEventHandler
 * \brief  Driver event handler
 * 
 * Process events received from HAL UART
 * 
 * \note   
 * \param  hBusDrv - the bus driver handler
 * \param  eEvent  - HAL UART event
 * \return void
 * \sa
 */ 
static void drvEventHandler(handle_t hBusDrv, EHalStEvent eEvent)
{
    TBusDrvObj *pBusDrv = (TBusDrvObj*)hBusDrv;

	switch (eEvent)
	{
	case HalStEvent_WriteComplInd:
		drvTxnDone(pBusDrv);
		pBusDrv->stat.TxCompl++;
		break;

	case HalStEvent_ReadReadylInd:
		drvRxInd(hBusDrv);
		pBusDrv->stat.RxInd++;
		break;

    default:
		/* Error --> HAL ST only support Write Complete or Read Ready indications. */
		MCPF_REPORT_ERROR(pBusDrv->hMcpf, BUS_DRV_MODULE_LOG, 
			("drvEventHandler: unexpected event"));
		break;
	}
}

/** 
 * \fn     drvTxnDone
 * \brief  Continue async send transaction processing
 * 
 * Called by the driver event handler upon Async transaction completion (TxnDone ISR).
 * Continue sending the remained transaction parts and 
 * indicate Tx completion if the all parts are sent
 * 
 * \note   
 * \param  hBusDrv - the bus driver handle
 * \return void
 * \sa     drvSendData
 */ 
static void drvTxnDone (TBusDrvObj  *pBusDrv)
{
	ETxnStatus  eStatus;

	MCPF_ENTER_CRIT_SEC (pBusDrv->hMcpf);

	if (pBusDrv->uOutLen == 0)
	{
		/* the entire pBusDrv->uOutLen chunk of data has been sent, prepare the next buffer */
		pBusDrv->iOutBufNum++;
	
		if ((pBusDrv->iOutBufNum < MAX_XFER_BUFS) &&
			pBusDrv->pOutTxn->aBuf[pBusDrv->iOutBufNum] &&  
			pBusDrv->pOutTxn->aLen[pBusDrv->iOutBufNum])
		{
			pBusDrv->pOutData = pBusDrv->pOutTxn->aBuf[pBusDrv->iOutBufNum];   
			pBusDrv->uOutLen  = pBusDrv->pOutTxn->aLen[pBusDrv->iOutBufNum];

			MCPF_EXIT_CRIT_SEC (pBusDrv->hMcpf);
	
			eStatus = drvSendData (pBusDrv);
		}
		else
		{
			MCPF_EXIT_CRIT_SEC (pBusDrv->hMcpf);

			/* transmit transaction is complete */
			pBusDrv->fTxnDoneCb (pBusDrv->hCbHandle, pBusDrv->pOutTxn);
			eStatus = RES_OK;
		}
	}
	else
	{
		MCPF_EXIT_CRIT_SEC (pBusDrv->hMcpf);

		/* transmitting of the current data chunk is not complete, continue from the same buffer */
		eStatus = drvSendData (pBusDrv);
	}

	if (eStatus == TXN_STATUS_ERROR)
	{
		/* Transport Error */
        MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
						  ("%s: Error: Tx done failed!\n", __FUNCTION__));
		pBusDrv->stat.TxErr++;
		transportError (pBusDrv, Txn_TxErr);
	}
}


/** 
 * \fn     startTxnSend
 * \brief  Strat transaction sending
 * 
 *  Initiates sending of normal transaction
 * 
 * \note   
 * \param  pBusDrv - pointer to bus driver object
 * \param  pTxn    - The transaction object 
 * \return PENDING (all send transactoions are asynch), ERROR if failed
 * \sa     busDrv_Transact
 */ 
static ETxnStatus startTxnSend (TBusDrvObj * pBusDrv, TTxnStruct * pTxn)
{
	ETxnStatus  eStatus;

	/* Check transaction flag: start transmitting from buffer or from txn header */
	if (!TXN_PARAM_GET_TXN_FLAG(pTxn))
	{
		/* start from buf[0] */
		pBusDrv->pOutTxn 	= pTxn;        
		pBusDrv->iOutBufNum = 0; 
		pBusDrv->pOutData 	= pTxn->pData;   
		pBusDrv->uOutLen  	= pTxn->aLen[0];
	}
	else
	{
		/* start from txn header */
		pBusDrv->pOutTxn 	= pTxn;        
		pBusDrv->iOutBufNum = -1;     /* buf[0] will be sent the next after the header */
		pBusDrv->pOutData 	= &(pTxn->tHeader.tHciHeader.uPktType);   
		pBusDrv->uOutLen  	= pTxn->tHeader.tHciHeader.uLen;
	}

	eStatus = drvSendData (pBusDrv);

	return eStatus;
}


/** 
 * \fn     startIfSlpMngSend
 * \brief  
 * 
 *  Initiates sending of normal transaction
 * 
 * \note   
 * \param  pBusDrv - pointer to bus driver object
 * \param  pTxn    - The transaction object 
 * \return PENDING (all send transactoions are asynch), ERROR if failed
 * \sa     busDrv_Transact
 */ 
static ETxnStatus startIfSlpMngSend (TBusDrvObj * pBusDrv, TTxnStruct * pTxn)
{
	McpU8	pktType;
	McpU8	slpMngOp;
	ETxnStatus  eStatus;


	slpMngOp = (McpU8)TXN_PARAM_GET_IFSLPMNG_OP(pTxn);
	switch (slpMngOp)
	{
	case TXN_PARAM_IFSLPMNG_OP_SLEEP:
		 pktType = HCI_PKT_TYPE_SLEEP_IND;
		 break;
	case TXN_PARAM_IFSLPMNG_OP_SLEEP_ACK:
		 pktType = HCI_PKT_TYPE_SLEEP_ACK;
		 break;
	case TXN_PARAM_IFSLPMNG_OP_AWAKE:
		 pktType = HCI_PKT_TYPE_WAKEUP_IND;
		 break;
	case TXN_PARAM_IFSLPMNG_OP_AWAKE_ACK:
		 pktType = HCI_PKT_TYPE_WAKEUP_ACK;
		 break;
	default:
#if TI_BUSDRV_DBG
		MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
						  ("%s: Invalid Txn IfSlpMng opcode=%u\n", __FUNCTION__, slpMngOp));
#endif
        transportError (pBusDrv, Txn_TxErr);
		return TXN_STATUS_ERROR;
		break;
	}

	/* Prepare txn header */
	pTxn->tHeader.tHciHeader.uPktType = pktType;
	pTxn->tHeader.tHciHeader.uLen = 1;
	TXN_PARAM_SET_TXN_FLAG(pTxn,1);

	/* start from txn header */
	pBusDrv->pOutTxn 	= pTxn;        
	pBusDrv->iOutBufNum = -1;     /* buf[0] is not sent now */
	pBusDrv->pOutData 	= &(pTxn->tHeader.tHciHeader.uPktType);   
	pBusDrv->uOutLen  	= pTxn->tHeader.tHciHeader.uLen;

	eStatus = drvSendData (pBusDrv);

	return eStatus;
}


/** 
 * \fn     drvSendData
 * \brief  Data sending via HAL UART
 * 
 *  Initiates data sending by calling of HAL UART function
 * 
 * \note   
 * \param  pBusDrv - pointer to bus driver object
 * \return PENDING (all send transactoions are asynch), ERROR if failed
 * \sa     busDrv_Transact
 */ 
static ETxnStatus drvSendData (TBusDrvObj * pBusDrv)
{
	McpU16 	sentLen;
	EMcpfRes  	eStatus;
	
    MCPF_REPORT_DUMP_BUFFER(pBusDrv->hMcpf,
                            BUS_DRV_MODULE_LOG,
                            "Tx",
                            pBusDrv->pOutData,
                            pBusDrv->uOutLen);

	MCPF_ENTER_CRIT_SEC (pBusDrv->hMcpf);

    eStatus = HAL_ST_Write (pBusDrv->hHalUart, pBusDrv->pOutData, pBusDrv->uOutLen, &sentLen);

	if (eStatus != RES_ERROR)
	{
		pBusDrv->pOutData += sentLen;
		pBusDrv->uOutLen  = (McpU16)(pBusDrv->uOutLen - sentLen);
		MCPF_EXIT_CRIT_SEC (pBusDrv->hMcpf);
		return TXN_STATUS_PENDING;
	}
	else
	{
		MCPF_EXIT_CRIT_SEC (pBusDrv->hMcpf);
		return TXN_STATUS_ERROR;
		
	}
}


/** 
 * \fn     drvRxInd
 * \brief  Data receive indication
 * 
 * Called by event handler upon Rx event, indicating received data is ready.
 * 
 * \note   
 * \param  hBusDrv - The module's object
 * \return void
 * \sa     busDrv_SendTxnParts
 */ 
static void drvRxInd (TBusDrvObj *pBusDrv)
{
	ETxnStatus  eCbStatus;
	EMcpfRes	    eReadStatus;
	McpU16 	len = 0;
	McpU16 	uRxLen=0;

#if TI_BUSDRV_DBG
	if (!pBusDrv->pInTxn || !pBusDrv->pInData )
	{
        MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
						  ("%s: Rx buffer is NULL!\n", __FUNCTION__));
		transportError (pBusDrv, Txn_RxErr);
	}
#endif

	if (ifSlpMng_handleRxReady(pBusDrv->hIfSlpMng) == RES_PENDING)
	{
		/* The PM state was sleeping */
		return;
	}

	if (pBusDrv->eRxState == BUSDRV_STATE_RX_CONGESTION)
	{
		/* The Rx state is congestion */
		return;
	}

	do
	{
		if (!pBusDrv->restart)
		{
			if (uRxLen == 0)
			{
				HAL_ST_ReadResult (pBusDrv->hHalUart, &uRxLen);
			}

            MCPF_REPORT_DUMP_BUFFER(pBusDrv->hMcpf,
                                    BUS_DRV_MODULE_LOG,
                                    "Rx",
                                    pBusDrv->pInData,
                                    uRxLen);
	
			pBusDrv->uInDataLen = (McpU16)(pBusDrv->uInDataLen + uRxLen);
			if (pBusDrv->uInDataLen < pBusDrv->uInLen)
			{
				/* More data is expected, continue Rx */
				pBusDrv->pInData += uRxLen;
			}
			else
			{
				/* The all expected data has been received, process according to Rx state */
				switch (pBusDrv->eRxState)
				{
				case BUSDRV_STATE_RX_PKT_TYPE:
					{
						EPktType ePktType;
	
						ePktType = processPktType (*pBusDrv->pInData, &len);
	
						MCPF_REPORT_DEBUG_RX(pBusDrv->hMcpf, BUS_DRV_MODULE_LOG, ("drvRxInd: BUSDRV_STATE_RX_PKT_TYPE pktType=%u expHeadLen=%u\n", 
															  *pBusDrv->pInData, len));
						if (ePktType == PktType_Normal)
						{
							/* Normal packet, prepare to receive packet header */
							pBusDrv->pInTxn->tHeader.tHciHeader.uPktType = *pBusDrv->pInData;
							pBusDrv->uInLen	  	=  len;			/* Expected header len */
							pBusDrv->uInDataLen =  0;			/* Accumulated data length */
							pBusDrv->pInData  	+= uRxLen;		/* Current data pointer for Rx */
							pBusDrv->eRxState 	=  BUSDRV_STATE_RX_HEADER;
						}
						else if (ePktType == PktType_IfSlpMng)
						{
							/* Interface sleep management packet is received, Rx is complete */
							setIfSlpMngTxn (pBusDrv->pInTxn, *pBusDrv->pInData);
	
							eCbStatus = pBusDrv->fRxIndCb (pBusDrv->hRxIndHandle, pBusDrv->pInTxn);
							if ((eCbStatus != TXN_STATUS_OK) ||
								(prepareNextPacketRx (pBusDrv) != RES_OK))
							{
								MCPF_REPORT_DEBUG_RX(pBusDrv->hMcpf, BUS_DRV_MODULE_LOG, ("drvRxInd: BUSDRV_STATE_RX_CONGESTION!\n"));
								pBusDrv->eRxState = BUSDRV_STATE_RX_CONGESTION;
								pBusDrv->stat.RxIndErr++;
								return;
							}
						}
						else
						{
							MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
											  ("%s: Invalid Rx packet type=%u\n", 
											   __FUNCTION__, *pBusDrv->pInData));
	
							transportError (pBusDrv, Txn_RxErr);
						}
						break;
					}
				case BUSDRV_STATE_RX_HEADER:
	
					pBusDrv->pInTxn->tHeader.tHciHeader.uLen = (McpU8)(pBusDrv->uInLen + HCI_PKT_TYPE_LEN);
					TXN_PARAM_SET_CHAN_NUM (pBusDrv->pInTxn, 
											pBusDrv->pInTxn->tHeader.tHciHeader.uPktType);
	
					len = getDataLenFromHeader (pBusDrv->pInTxn->tHeader.tHciHeader.uPktType,
												pBusDrv->pInTxn->aBuf[0] + HCI_PKT_TYPE_LEN); 
	
					MCPF_REPORT_DEBUG_RX(pBusDrv->hMcpf, BUS_DRV_MODULE_LOG, ("drvRxInd: BUSDRV_STATE_RX_HEADER expDataLen=%u\n", len));

					if (len == 0)
					{
						pBusDrv->pInTxn->aLen[0] = 0;
						eCbStatus = pBusDrv->fRxIndCb (pBusDrv->hRxIndHandle, pBusDrv->pInTxn);
						if ((eCbStatus != TXN_STATUS_OK) ||
							(prepareNextPacketRx (pBusDrv) != RES_OK))
						{
							MCPF_REPORT_DEBUG_RX(pBusDrv->hMcpf, BUS_DRV_MODULE_LOG, ("drvRxInd: BUSDRV_STATE_RX_CONGESTION!\n"));
							pBusDrv->eRxState = BUSDRV_STATE_RX_CONGESTION;
							pBusDrv->stat.RxIndErr++;
							return;
						}
					}
					else
					{
						if ((len + pBusDrv->uInDataLen) < POOL_RX_MAX_BUF_SIZE)
						{
							pBusDrv->uInLen	  	=  len;				/* Expected data length */
							pBusDrv->uInDataLen =  0;               /* Accumulated data length */
							pBusDrv->pInData  	+= uRxLen;			/* Current data pointer for Rx */
							pBusDrv->eRxState 	=  BUSDRV_STATE_RX_DATA;
						}
						else
						{
							MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
								("%s: Rx len=%u is too big\n", __FUNCTION__, uRxLen));
							pBusDrv->stat.RxErr++;
							transportError (pBusDrv, Txn_RxErr);
						}
					}
					break;
	
				case BUSDRV_STATE_RX_DATA:
	
					pBusDrv->pInTxn->aLen[0] = (McpU16) (pBusDrv->uInLen + 
												pBusDrv->pInTxn->tHeader.tHciHeader.uLen);
	
					MCPF_REPORT_DEBUG_RX(pBusDrv->hMcpf, BUS_DRV_MODULE_LOG, ("drvRxInd: BUSDRV_STATE_RX_DATA Len=%u\n", 
														  pBusDrv->pInTxn->aLen[0]));
	
					eCbStatus = pBusDrv->fRxIndCb (pBusDrv->hRxIndHandle, pBusDrv->pInTxn);
					if ((eCbStatus != TXN_STATUS_OK) ||
						(prepareNextPacketRx (pBusDrv) != RES_OK))
					{
						MCPF_REPORT_DEBUG_RX(pBusDrv->hMcpf, BUS_DRV_MODULE_LOG, ("drvRxInd: BUSDRV_STATE_RX_CONGESTION!\n"));
						pBusDrv->eRxState = BUSDRV_STATE_RX_CONGESTION;
						pBusDrv->stat.RxIndErr++;
						return;
					}
					break;
	
				case BUSDRV_STATE_RX_CONGESTION:
					MCPF_REPORT_DEBUG_RX(pBusDrv->hMcpf, BUS_DRV_MODULE_LOG, ("drvRxInd: BUSDRV_STATE_RX_CONGESTION\n"));
					break;
	
				default:
				#if TI_BUSDRV_DBG
					MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
									  ("%s: invalid Rx state=%u!\n", __FUNCTION__, pBusDrv->eRxState));
					pBusDrv->stat.RxErr++;
					transportError (pBusDrv, Txn_RxErr);
				#endif
					break;
				}
			}
	    } /* if restart */

		pBusDrv->restart = MCP_FALSE;
		eReadStatus = HAL_ST_Read (pBusDrv->hHalUart, 
								 pBusDrv->pInData, 
								 (McpU16)(pBusDrv->uInLen - pBusDrv->uInDataLen), 
								 &uRxLen);

		if (eReadStatus == RES_ERROR)
		{
			/* Transport Error */
			MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
							  ("%s: Rx failed, status=%u len=%u!\n", 
							   __FUNCTION__, eReadStatus, uRxLen));
			pBusDrv->stat.RxErr++;
			transportError (pBusDrv, Txn_RxErr);
		}

	} while (uRxLen);  /* Repeat Rx process while read length is not zero */
}

/** 
 * \fn     setIfSlpMngTxn
 * \brief  
 * 
 *  Sets transaction structure fields for Interface Sleep Management packet
 * 
 * \note   
 * \param  pTxn    - The transaction object 
 * \param  pktType - Packet type
 * \return void
 * \sa     drvRxInd
 */ 
static void setIfSlpMngTxn (TTxnStruct * pTxn, McpU8 pktType)
{
	switch (pktType)
	{
	case HCI_PKT_TYPE_SLEEP_IND:
		TXN_PARAM_SET_IFSLPMNG_OP (pTxn, TXN_PARAM_IFSLPMNG_OP_SLEEP); 
		break;

	case HCI_PKT_TYPE_SLEEP_ACK:
		TXN_PARAM_SET_IFSLPMNG_OP (pTxn, TXN_PARAM_IFSLPMNG_OP_SLEEP_ACK); 
		break;

	case HCI_PKT_TYPE_WAKEUP_IND:
		TXN_PARAM_SET_IFSLPMNG_OP (pTxn, TXN_PARAM_IFSLPMNG_OP_AWAKE); 
		break;

	case HCI_PKT_TYPE_WAKEUP_ACK:
		TXN_PARAM_SET_IFSLPMNG_OP (pTxn, TXN_PARAM_IFSLPMNG_OP_AWAKE_ACK); 
		break;
	}
	/* Prepare txn header */
	pTxn->tHeader.tHciHeader.uPktType = pktType;
	pTxn->tHeader.tHciHeader.uLen = HCI_PKT_TYPE_LEN;
	pTxn->aLen[0] = HCI_PKT_TYPE_LEN;
	TXN_PARAM_SET_TXN_FLAG (pTxn,1);
	TXN_PARAM_SET_SINGLE_STEP (pTxn,1);
}

/** 
 * \fn     prepareNextPacketRx
 * \brief  Prepare for the next packet reception
 * 
 *  Allocates the new packet buffer from the pool and sets Rx state
 * 
 * \note   
 * \param  pBusDrv - pointer to bus driver object
 * \return status of operation: OK or NOK
 * \sa     drvRxInd
 */ 
static EMcpfRes prepareNextPacketRx (TBusDrvObj *pBusDrv)
{
	EMcpfRes result;

	
	pBusDrv->pInTxn = mcpf_txnPool_AllocBuf (pBusDrv->hMcpf, POOL_RX_MAX_BUF_SIZE);
	if (pBusDrv->pInTxn)
	{
 /*       MCPF_REPORT_INFORMATION(pBusDrv->hMcpf, BUS_DRV_MODULE_LOG, ("prepareNextPacketRx: Alloc Txn & buf 0x%x", (pBusDrv->pInTxn)->aBuf[0]));*/
		resetRxState (pBusDrv);
		result = RES_OK;
	}
	else
	{
		/* Pool is exausted */
		pBusDrv->stat.AllocErr++;
		result = RES_ERROR;
	}
	return result;
}


/** 
 * \fn     resetRxState
 * \brief  Resets Rx state
 * 
 *  Set Rx to initial state to receive new packet
 * 
 * \note   
 * \param  pBusDrv - pointer to bus driver object
 * \return void
 * \sa     drvRxInd
 */ 
static void resetRxState (TBusDrvObj *pBusDrv)
{
	/* Reset receiver state */
	pBusDrv->eRxState 	= BUSDRV_STATE_RX_PKT_TYPE;
	pBusDrv->uInLen	  	= HCI_PKT_TYPE_LEN;		/* Expected data length is one byte of packet type */
	pBusDrv->uInDataLen = 0;
	pBusDrv->pInData  	= pBusDrv->pInTxn->aBuf[0];
	pBusDrv->pInTxn->pData = pBusDrv->pInTxn->aBuf[0];
}

/** 
 * \fn     processPktType
 * \brief  Gets HCI header length by packet type
 * 
 *  Returns HCI header length depending on the packet type
 * 
 * \note   
 * \param  pktType - Packet type
 * \param  pLen    - Pointer to length variable to return the lenght value
 * \return valid packet type (normal, IfSlpMng or Err)
 * \sa     drvRxInd
 */ 
static EPktType processPktType (McpU8 pktType, McpU16 *pLen)
{
	EPktType    ePktType = PktType_Normal;

	switch (pktType)
	{
	case HCI_PKT_TYPE_ACL:
		*pLen = HCI_HEADER_SIZE_ACL;
		break;

	case HCI_PKT_TYPE_SCO:
		*pLen = HCI_HEADER_SIZE_SCO;
		break;

	case HCI_PKT_TYPE_EVENT:
		*pLen = HCI_HEADER_SIZE_EVENT;
		break;

	case HCI_PKT_TYPE_NAVC:
		*pLen = HCI_HEADER_SIZE_NAVC;
		break;

	case HCI_PKT_TYPE_SLEEP_IND:
	case HCI_PKT_TYPE_SLEEP_ACK:
	case HCI_PKT_TYPE_WAKEUP_IND:
	case HCI_PKT_TYPE_WAKEUP_ACK:
		*pLen = HCI_HEADER_SIZE_IFSLPMNG;
		ePktType = PktType_IfSlpMng;
		break;

	default:
		ePktType = PktType_Err;
	}
	return ePktType;
}


/** 
 * \fn     getDataLenFromHeader
 * \brief  Gets data length from HCI header
 * 
 *  Gets data length field from HCI header depending on packet type
 * 
 * \note   
 * \param  pktType - Packet type
 * \param  pHeader - Pointer to packet header
 * \return data length
 * \sa     drvRxInd
 */ 
static McpU16 getDataLenFromHeader (McpU8 pktType, McpU8 *pHeader)
{
	McpU16 	len;

	switch (pktType)
	{
	case HCI_PKT_TYPE_ACL:
		len = (McpU16)(mcpf_endian_LEtoHost16(pHeader + 2));
		break;

	case HCI_PKT_TYPE_SCO:
		len = (McpU16)(pHeader[2]);
		break;

	case HCI_PKT_TYPE_EVENT:
		len = (McpU16)(pHeader[1]);
		break;

	case HCI_PKT_TYPE_NAVC:
		len = (McpU16)(mcpf_endian_LEtoHost16(pHeader + 1));
		break;

	default:
		len = RX_LEN_ERROR;
	}
	return len;
}

void transportError (TBusDrvObj *pBusDrv, ETxnErrId eErrId)
{
	TTxnEvent 	tEvent;

	if(pBusDrv->pInTxn !=NULL)
	resetRxState (pBusDrv);
	MCPF_REPORT_ERROR (pBusDrv->hMcpf, BUS_DRV_MODULE_LOG,
					  ("%s: Transport Error!\n", __FUNCTION__));

	/* report error to the upper layer */

	tEvent.eEventType = Txn_ErrorInd;
	tEvent.tEventParams.err.eErrId 	= eErrId;
	tEvent.tEventParams.err.eErrSeverity = TxnCritical;
	tEvent.tEventParams.err.eModuleId = Txn_BusDrvModule;
	tEvent.tEventParams.err.pErrStr = NULL;

	pBusDrv->fEventCb (pBusDrv->hEventCbHandle, &tEvent);
}


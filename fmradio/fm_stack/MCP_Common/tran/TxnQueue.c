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

 
/** \file   TxnQueue.c 
 *  \brief  The transaction-queue module. 
 *
 * The Transaction Queue encapsulates the bus access from a functional driver (WLAN, BT, FM and NAVC).
 * This TI proprietary module presents the same interface and same behavior for different 
 *     bus configuration: SDIO (multi or single function), SPI or UART and for different modes 
 *     of operation: Synchronous, a-synchronous or combination of both. 
 * It will also be used over the RS232 interface (using wUART protocol) which is applicable 
 *     for RS applications (on PC).
 * 
 * The TxnQ module provides the following requirements:
 *     Inter process protection on queue's internal database and synchronization between 
 *         functional drivers that share the bus.
 *     Support multiple queues per function, handled by priority. 
 *     Support the TTxnStruct API (as the Bus Driver) with the ability to manage commands 
 *         queuing of multiple functions on top of the Bus Driver. 
 *     The TxnQ (as well as the layers above it) is agnostic to the bus driver used beneath it 
 *         (SDIO, WSPI or UART), since all bus drivers introduce the same API and hide bus details. 
 *     The TxnQ has no OS dependencies. It supports access from multiple OS threads.
 * Note: It is assumed that any transaction forwarded to the TxnQ has enough resources in HW. 
 * 
 *  \see    TxnQueue.h
 */

#include "mcpf_mem.h"
#include "mcpf_services.h"
#include "mcpf_report.h"
#include "mcp_hal_os.h"
#include "TxnDefs.h"
#include "BusDrv.h"
#include "TxnQueue.h"



/************************************************************************
 * Defines
 ************************************************************************/
#define TXN_MAX_FUNCTIONS   4   /* Maximum 4 functional drivers (including Func 0 which is for bus control) */
#define TXN_MAX_PRIORITY    2   /* Maximum 2 prioritys per functional driver */
#define TXN_QUE_SIZE        64  /* Txn-queue size */
#define TXN_DONE_QUE_SIZE   64  /* TxnDone-queue size */


/************************************************************************
 * Types
 ************************************************************************/

/* Functional driver's SM States */
typedef enum
{
    FUNC_STATE_NONE,              /* Function not registered */
	FUNC_STATE_STOPPED,           /* Queues are stopped */
	FUNC_STATE_RUNNING,           /* Queues are running */
	FUNC_STATE_RESTART            /* Wait for current Txn to finish before restarting queues */
} EFuncState;

/* The functional drivers registered to TxnQ */
typedef struct 
{
    EFuncState      eState;             /* Function crrent state */
    McpU32       	uNumPrios;          /* Number of queues (priorities) for this function */
	TTxnQueueDoneCb fTxnQueueDoneCb;    /* The CB called by the TxnQueue upon full transaction completion. */
	handle_t       	hCbHandle;          /* The callback handle */
    TTxnStruct *    pSingleStep;        /* A single step transaction waiting to be sent */

} TFuncInfo;


/* The TxnQueue module Object */
typedef struct _TTxnQObj
{
    handle_t	    hMcpf;		   	 
	handle_t	    hBusDrv;

    TFuncInfo       aFuncInfo[TXN_MAX_FUNCTIONS];  /* Registered functional drivers - see above */
    handle_t       	aTxnQueues[TXN_MAX_FUNCTIONS][TXN_MAX_PRIORITY];  /* Handle of the Transactions-Queue */
    handle_t       	hTxnDoneQueue;      /* Queue for completed transactions not reported to yet to the upper layer */
    TTxnStruct *    pCurrTxn;           /* The transaction currently processed in the bus driver (NULL if none) */
    McpU32       	uMinFuncId;         /* The minimal function ID actually registered (through txnQ_Open) */
    McpU32      	uMaxFuncId;         /* The maximal function ID actually registered (through txnQ_Open) */
    
    /* Environment dependent: TRUE if needed and allowed to protect TxnDone in critical section */
    McpBool         bProtectTxnDone; 
    TTxnDoneCb      fConnectCb;
    handle_t       	hConnectCb;

} TTxnQObj;


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/
static void         txnQ_TxnDoneCb    (handle_t hTxnQ, void *hTxn);
static ETxnStatus   txnQ_RunScheduler (TTxnQObj *pTxnQ, TTxnStruct *pCurrTxn, McpBool bExternalContext);
static TTxnStruct  *txnQ_SelectTxn    (TTxnQObj *pTxnQ);
static void         txnQ_ClearQueues  (TTxnQObj *pTxnQ, McpU32 uFuncId, McpBool bExternalContext);
static void         txnQ_ConnectCB    (handle_t hTxnQ, void *hTxn);



/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/** 
 * \fn     txnQ_Create 
 * \brief  Create the module
 * 
 * Allocate and clear the module's object.
 * 
 * \note   
 * \param  hMcpf - Handle to Os framework
 * \return Handle of the allocated object, NULL if allocation failed 
 * \sa     txnQ_Destroy
 */ 
handle_t txnQ_Create (const handle_t hMcpf)
{
    handle_t  hTxnQ;
    TTxnQObj  *pTxnQ;
    McpU32  i;

    hTxnQ = mcpf_mem_alloc(hMcpf, sizeof(TTxnQObj));
    if (hTxnQ == NULL)
        return NULL;
    
    pTxnQ = (TTxnQObj *)hTxnQ;

    mcpf_mem_zero(hMcpf, hTxnQ, sizeof(TTxnQObj));
    
    pTxnQ->hMcpf           = hMcpf;
    pTxnQ->pCurrTxn        = NULL;
    pTxnQ->uMinFuncId      = TXN_MAX_FUNCTIONS; /* Start at maximum and save minimal value in txnQ_Open */
    pTxnQ->uMaxFuncId      = 0;             /* Start at minimum and save maximal value in txnQ_Open */
    pTxnQ->bProtectTxnDone = MCP_TRUE;

    for (i = 0; i < TXN_MAX_FUNCTIONS; i++)
    {
        pTxnQ->aFuncInfo[i].eState          = FUNC_STATE_NONE;
        pTxnQ->aFuncInfo[i].uNumPrios       = 0;
        pTxnQ->aFuncInfo[i].pSingleStep     = NULL;
        pTxnQ->aFuncInfo[i].fTxnQueueDoneCb = NULL;
        pTxnQ->aFuncInfo[i].hCbHandle       = NULL;
    }
    
    /* Create the Bus-Driver module */
    pTxnQ->hBusDrv = busDrv_Create (hMcpf);
    if (pTxnQ->hBusDrv == NULL)
    {
        MCPF_OS_REPORT(("%s: Error - failed to create BusDrv\n", __FUNCTION__));
        txnQ_Destroy (hTxnQ);
        return NULL;
    }

    return pTxnQ;
}


/** 
 * \fn     txnQ_Destroy
 * \brief  Destroy the module. 
 * 
 * Destroy bus driver and free the module's object.
 * 
 * \note   
 * \param  The module's object
 * \return RES_OK on success or RES_ERROR on failure 
 * \sa     txnQ_Create
 */ 
EMcpfRes txnQ_Destroy (handle_t hTxnQ)
{
    TTxnQObj *pTxnQ = (TTxnQObj*)hTxnQ;

    if (pTxnQ)
    {
        if (pTxnQ->hBusDrv) 
        {
            busDrv_Destroy (pTxnQ->hBusDrv);
        }
        if (pTxnQ->hTxnDoneQueue) 
        {
            que_Destroy (pTxnQ->hTxnDoneQueue);
        }
        mcpf_mem_free (pTxnQ->hMcpf, pTxnQ);     
    }
    return RES_OK;
}


/** 
 * \fn     txnQ_Init 
 * \brief  Init module 
 * 
 * Init required handles and module variables, and create the TxnDone-queue.
 * 
 * \note    
 * \param  hTxnQ     - The module's object
 * \param  hMcpf     - Handle to Os framework
 * \return void        
 * \sa     
 */ 
void txnQ_Init (handle_t hTxnQ, const handle_t hMcpf)
{
    TTxnQObj  *pTxnQ = (TTxnQObj*)hTxnQ;
    McpU32  uNodeHeaderOffset;

    pTxnQ->hMcpf = hMcpf;

    /* Create the TxnDone queue. */
    uNodeHeaderOffset = MCPF_FIELD_OFFSET(TTxnStruct, tTxnQNode); 
    pTxnQ->hTxnDoneQueue = que_Create (pTxnQ->hMcpf, TXN_DONE_QUE_SIZE, uNodeHeaderOffset);
    if (pTxnQ->hTxnDoneQueue == NULL)
    {
        MCPF_REPORT_ERROR(pTxnQ->hMcpf, QUEUE_MODULE_LOG,
            ("%s: TxnDone queue creation failed!\n", __FUNCTION__));
    }

    busDrv_Init (pTxnQ->hBusDrv);
}


/** 
 * \fn     txnQ_ConnectBus
 * \brief  Configure bus driver
 * 
 * Called by upper layer initialization function.
 * Configure the bus driver with its connection configuration (such as baud-rate, bus width etc) 
 *     and establish the physical connection. Done once (and not per functional driver startup). 
 * 
 * \note   
 * \param  hTxnQ      - The module's object
 * \param  pBusDrvCfg - A union used for per-bus specific configuration. 
 * \return RES_OK / RES_ERROR
 * \sa     
 */ 
EMcpfRes   txnQ_ConnectBus (handle_t 				hTxnQ, 
							 const TBusDrvCfg 		*pBusDrvCfg,
							 const TTxnDoneCb 		fConnectCb,
							 const handle_t 		hConnectCb, 
							 const TI_BusDvrRxIndCb fRxIndCb,
							 const handle_t 		hRxIndHandle, 
							 const TI_TxnEventHandlerCb fEventHandlerCb,
							 const handle_t 		hEventHandle,
							 const handle_t 		hIfSlpMng)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

    pTxnQ->fConnectCb = fConnectCb;
    pTxnQ->hConnectCb = hConnectCb;

    return busDrv_ConnectBus (pTxnQ->hBusDrv, 
							  pBusDrvCfg, 
							  txnQ_TxnDoneCb, 
							  hTxnQ, 
							  txnQ_ConnectCB,
							  fRxIndCb, hRxIndHandle,
							  fEventHandlerCb, hEventHandle,
							  hIfSlpMng);
}

/** 
 * \fn     txnQ_ResetBus
 * \brief  reset Bus driver
 * 
 * Called by upper layer on reset - must reset the baudrate.
 * 
 * \note   
 * \param  hTxnQ      - The module's object
 * \param  baudrate - the baudrate to set configuration.
 * \return RES_OK / RES_ERROR
 * \sa
 */ 
EMcpfRes   txnQ_ResetBus (handle_t hTxnQ, const McpU16 baudrate)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

    return busDrv_Reset(pTxnQ->hBusDrv, baudrate);
    
}

/** 
 * \fn     txnQ_DisconnectBus
 * \brief  Disconnect bus driver
 * 
 * Called by upper layer initialization function.
 * Disconnect the bus driver.
 *  
 * \note   
 * \param  hTxnQ      - The module's object
 * \return RES_OK / RES_ERROR
 * \sa     
 */ 
EMcpfRes txnQ_DisconnectBus (const handle_t hTxnQ)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

    return busDrv_DisconnectBus (pTxnQ->hBusDrv);
}


/** 
 * \fn     txnQ_Open
 * \brief  Register functional driver to TxnQ
 * 
 * Called by each functional driver using the TxnQ.
 * Save driver's info and create its queues. 
 * Perform in critical section to prevent preemption from TxnDone.
 * 
 * \note   
 * \param  hTxnQ           - The module's object
 * \param  uFuncId         - The calling functional driver
 * \param  uNumPrios       - The number of queues/priorities
 * \param  fTxnQueueDoneCb - The callback to call upon full transaction completion.
 * \param  hCbHandle       - The callback handle                              
 * \return RES_OK / RES_ERROR
 * \sa     txnQ_Close
 */ 
EMcpfRes txnQ_Open (handle_t       		hTxnQ, 
                     const McpU32       	uFuncId, 
                     const McpU32       	uNumPrios, 
                     const TTxnQueueDoneCb 	fTxnQueueDoneCb,
                     const handle_t       	hCbHandle)
{
    TTxnQObj     *pTxnQ = (TTxnQObj*) hTxnQ;
    McpU32     uNodeHeaderOffset;
    McpU32     i;

    if (uFuncId > TXN_MAX_FUNCTIONS  ||  uNumPrios > TXN_MAX_PRIORITY) 
    {
        MCPF_REPORT_ERROR(pTxnQ->hMcpf, QUEUE_MODULE_LOG,
            ("%s: Invalid Params!  uFuncId = %d, uNumPrios = %d\n", __FUNCTION__, uFuncId, uNumPrios));
        return RES_ERROR;
    }

    MCPF_ENTER_CRIT_SEC (pTxnQ->hMcpf);

    /* Save functional driver info */
    pTxnQ->aFuncInfo[uFuncId].uNumPrios       = uNumPrios;
    pTxnQ->aFuncInfo[uFuncId].fTxnQueueDoneCb = fTxnQueueDoneCb;
    pTxnQ->aFuncInfo[uFuncId].hCbHandle       = hCbHandle;

    /* Set state as running, since the chip init state is awake. */
    pTxnQ->aFuncInfo[uFuncId].eState = FUNC_STATE_RUNNING;
    
    /* Create the functional driver's queues. */
    uNodeHeaderOffset = MCPF_FIELD_OFFSET(TTxnStruct, tTxnQNode); 
    for (i = 0; i < uNumPrios; i++)
    {
        pTxnQ->aTxnQueues[uFuncId][i] = que_Create (pTxnQ->hMcpf, TXN_QUE_SIZE, uNodeHeaderOffset);
        if (pTxnQ->aTxnQueues[uFuncId][i] == NULL)
        {
            MCPF_REPORT_ERROR(pTxnQ->hMcpf, QUEUE_MODULE_LOG,
                ("%s: Queues creation failed!\n", __FUNCTION__));
            return RES_ERROR;
        }
    }

    /* Update functions actual range (to optimize Txn selection loops - see txnQ_SelectTxn) */
    if (uFuncId < pTxnQ->uMinFuncId) 
    {
        pTxnQ->uMinFuncId = uFuncId;
    }
    if (uFuncId > pTxnQ->uMaxFuncId) 
    {
        pTxnQ->uMaxFuncId = uFuncId;
    }

    MCPF_EXIT_CRIT_SEC (pTxnQ->hMcpf);

    MCPF_REPORT_INFORMATION(pTxnQ->hMcpf, QUEUE_MODULE_LOG,
        ("%s: Function %d registered successfully, uNumPrios = %d\n", __FUNCTION__, uFuncId, uNumPrios));

    return RES_OK;
}


/** 
 * \fn     txnQ_Close
 * \brief  Unregister functional driver from TxnQ
 * 
 * Called by registered functional driver that uses the TxnQ to unregister.
 * Clear the function's data and destroy its queues. 
 * Perform in critical section to prevent preemption from TxnDone.
 * 
 * \note   
 * \param  hTxnQ      - The module's object
 * \param  uFuncId    - The calling functional driver
 * \return void
 * \sa     txnQ_Open
 */ 
void txnQ_Close (handle_t  hTxnQ, const McpU32 uFuncId)
{
    TTxnQObj     *pTxnQ = (TTxnQObj*)hTxnQ;
    McpU32     i;

    MCPF_ENTER_CRIT_SEC (pTxnQ->hMcpf);

    /* Destroy the functional driver's queues.  TODO(954): empty the queues first??. */
    for (i = 0; i < pTxnQ->aFuncInfo[uFuncId].uNumPrios; i++)
    {
        que_Destroy (pTxnQ->aTxnQueues[uFuncId][i]);
    }

    /* Clear functional driver info */
    pTxnQ->aFuncInfo[uFuncId].uNumPrios       = 0;
    pTxnQ->aFuncInfo[uFuncId].fTxnQueueDoneCb = NULL;
    pTxnQ->aFuncInfo[uFuncId].hCbHandle       = NULL;
    pTxnQ->aFuncInfo[uFuncId].eState          = FUNC_STATE_NONE;
    
    /* Update functions actual range (to optimize Txn selection loops - see txnQ_SelectTxn) */
    pTxnQ->uMinFuncId      = TXN_MAX_FUNCTIONS; 
    pTxnQ->uMaxFuncId      = 0;             
    for (i = 0; i < TXN_MAX_FUNCTIONS; i++) 
    {
        if (pTxnQ->aFuncInfo[i].eState != FUNC_STATE_NONE) 
        {
            if (i < pTxnQ->uMinFuncId) 
            {
                pTxnQ->uMinFuncId = i;
            }
            if (i > pTxnQ->uMaxFuncId) 
            {
                pTxnQ->uMaxFuncId = i;
            }
        }
    }

    MCPF_EXIT_CRIT_SEC (pTxnQ->hMcpf);

    MCPF_REPORT_INFORMATION(pTxnQ->hMcpf, QUEUE_MODULE_LOG,
        ("%s: Function %d Unregistered\n", __FUNCTION__, uFuncId));
}


/** 
 * \fn     txnQ_Restart
 * \brief  Restart caller's queues
 * 
 * Called upon functional driver stop command or upon recovery. 
 * If no transaction in progress for the calling function, clear its queues (call the CBs). 
 * If a transaction from this function is in progress, just set state to RESTART and when 
 *     called back upon TxnDone clear the queues.
 * Perform in critical section to prevent preemption from TxnDone.
 * Note that the Restart applies only to the calling function's queues.
 * 
 * \note   
 * \param  hTxnQ      - The module's object
 * \param  uFuncId    - The calling functional driver
 * \return COMPLETE if queues were restarted, PENDING if waiting for TxnDone to restart queues
 * \sa     txnQ_ClearQueues
 */ 
ETxnStatus txnQ_Restart (handle_t hTxnQ, const McpU32 uFuncId)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

#ifdef TRAN_DBG
    if (pTxnQ->aFuncInfo[uFuncId].eState != FUNC_STATE_RUNNING) 
    {
        MCPF_REPORT_ERROR(pTxnQ->hMcpf, QUEUE_MODULE_LOG,
            ("%s: Called while func %d state is %d!\n",
              __FUNCTION__, uFuncId, pTxnQ->aFuncInfo[uFuncId].eState));
    }
#endif

    MCPF_ENTER_CRIT_SEC (pTxnQ->hMcpf);

    /* If a Txn from the calling function is in progress, set state to RESTART return PENDING */
    if (pTxnQ->pCurrTxn) 
    {
        if (TXN_PARAM_GET_FUNC_ID(pTxnQ->pCurrTxn) == uFuncId)
        {
            pTxnQ->aFuncInfo[uFuncId].eState = FUNC_STATE_RESTART;

            MCPF_EXIT_CRIT_SEC (pTxnQ->hMcpf);

            /* Return PENDING to indicate that the restart will be completed later (in TxnDone) */
            return TXN_STATUS_PENDING;
        }
    }

    /* Clear the calling function's queues (call function CB with status=RECOVERY) */
    txnQ_ClearQueues (pTxnQ, uFuncId, MCP_FALSE);

    MCPF_EXIT_CRIT_SEC (pTxnQ->hMcpf);

    /* Return COMPLETE to indicate that the restart was completed */
    return TXN_STATUS_COMPLETE;
}


/** 
 * \fn     txnQ_Run
 * \brief  Run caller's queues
 * 
 * Enable TxnQ scheduler to process transactions from the calling function's queues.
 * Run scheduler to issue transactions as possible.
 * Run in critical section to protect from preemption by TxnDone.
 * 
 * \note   
 * \param  hTxnQ   - The module's object
 * \param  uFuncId - The calling functional driver
 * \return void
 * \sa     
 */ 
void txnQ_Run (handle_t hTxnQ, const McpU32 uFuncId)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

#ifdef TRAN_DBG
    if (pTxnQ->aFuncInfo[uFuncId].eState != FUNC_STATE_STOPPED) 
    {
        MCPF_REPORT_ERROR(pTxnQ->hMcpf, QUEUE_MODULE_LOG,
            ("%s: Called while func %d state is %d!\n",
              __FUNCTION__, uFuncId, pTxnQ->aFuncInfo[uFuncId].eState));
    }
#endif

    MCPF_ENTER_CRIT_SEC (pTxnQ->hMcpf);

    /* Enable function's queues */
    pTxnQ->aFuncInfo[uFuncId].eState = FUNC_STATE_RUNNING;

    /* Send queued transactions as possible */
    txnQ_RunScheduler (pTxnQ, NULL, MCP_FALSE); 

    MCPF_EXIT_CRIT_SEC (pTxnQ->hMcpf);
}


/** 
 * \fn     txnQ_Stop
 * \brief  Stop caller's queues
 * 
 * Disable TxnQ scheduler to process transactions from the calling function's queues.
 * 
 * \note   
 * \param  hTxnQ   - The module's object
 * \param  uFuncId - The calling functional driver
 * \return void
 * \sa     
 */ 
void txnQ_Stop (handle_t hTxnQ, const McpU32 uFuncId)
{
    TTxnQObj *pTxnQ = (TTxnQObj*) hTxnQ;

#ifdef TRAN_DBG
    if (pTxnQ->aFuncInfo[uFuncId].eState != FUNC_STATE_RUNNING) 
    {
        MCPF_REPORT_ERROR(pTxnQ->hMcpf, QUEUE_MODULE_LOG,
            ("%s: Called while func %d state is %d!\n",
              __FUNCTION__, uFuncId, pTxnQ->aFuncInfo[uFuncId].eState));
    }
#endif

    /* Enable function's queues */
    pTxnQ->aFuncInfo[uFuncId].eState = FUNC_STATE_STOPPED;
}


/** 
 * \fn     txnQ_Transact
 * \brief  Issue a new transaction 
 * 
 * Called by the functional driver to initiate a new transaction.
 * In critical section save transaction and call scheduler.
 * 
 * \note   
 * \param  hTxnQ - The module's object
 * \param  pTxn  - The transaction object 
 * \return COMPLETE if input pTxn completed in this context, PENDING if not, ERROR if failed
 * \sa     
 */ 
ETxnStatus txnQ_Transact (handle_t hTxnQ, TTxnStruct *pTxn)
{
    TTxnQObj    *pTxnQ   = (TTxnQObj*)hTxnQ;
    McpU32    uFuncId = TXN_PARAM_GET_FUNC_ID(pTxn);
    ETxnStatus   rc;

    MCPF_ENTER_CRIT_SEC (pTxnQ->hMcpf);

    if (TXN_PARAM_GET_SINGLE_STEP(pTxn)) 
    {
        pTxnQ->aFuncInfo[uFuncId].pSingleStep = pTxn;
    }
    else 
    {
        handle_t hQueue = pTxnQ->aTxnQueues[uFuncId][TXN_PARAM_GET_PRIORITY(pTxn)];
        que_Enqueue (hQueue, (handle_t)pTxn);
    }

    /* Send queued transactions as possible */
    rc = txnQ_RunScheduler (pTxnQ, pTxn, MCP_FALSE); 

    MCPF_EXIT_CRIT_SEC (pTxnQ->hMcpf);

    return rc;
}


/** 
 * \fn     txnQ_ConnectCB
 * \brief  Pending Connection completion CB
 * 
 *  txnQ_ConnectBus CB
 * 
 * \note   
 * \param  hTxnQ - The module's object
 * \param  pTxn  - The completed transaction object 
 * \return void
 * \sa     
 */ 
static void txnQ_ConnectCB (handle_t hTxnQ, void *hTxn)
{
    TTxnQObj   *pTxnQ   = (TTxnQObj*)hTxnQ;

	MCPF_UNUSED_PARAMETER(hTxn);

	/* Call the Client Connect CB */
    pTxnQ->fConnectCb (pTxnQ->hConnectCb, NULL);
}


/** 
 * \fn     txnQ_TxnDoneCb
 * \brief  Pending Transaction completion CB
 * 
 * Called back by bus-driver upon pending transaction completion in TxnDone context (external!).
 * Enqueue completed transaction in TxnDone queue and call scheduler to send queued transactions.
 * 
 * \note   
 * \param  hTxnQ - The module's object
 * \param  pTxn  - The completed transaction object 
 * \return void
 * \sa     
 */ 
static void txnQ_TxnDoneCb (handle_t hTxnQ, void *hTxn)
{
    TTxnQObj   *pTxnQ   = (TTxnQObj*)hTxnQ;
    TTxnStruct *pTxn    = (TTxnStruct *)hTxn;
    McpU32   uFuncId = TXN_PARAM_GET_FUNC_ID(pTxn);

#ifdef TRAN_DBG
    if (pTxn != pTxnQ->pCurrTxn) 
    {
        MCPF_REPORT_ERROR(pTxnQ->hMcpf, QUEUE_MODULE_LOG,
            ("%s: CB returned pTxn 0x%p  while pCurrTxn is 0x%p !!\n", __FUNCTION__, pTxn, pTxnQ->pCurrTxn));
    }
#endif

    /* Enter critical section if configured to do so */
    if (pTxnQ->bProtectTxnDone)
    {
        MCPF_ENTER_CRIT_SEC (pTxnQ->hMcpf);
    }

    /* If the function of the completed Txn is waiting for restart */
    if (pTxnQ->aFuncInfo[uFuncId].eState == FUNC_STATE_RESTART) 
    {
        /* Call function CB for current Txn with recovery indication */
        TXN_PARAM_SET_STATUS(pTxn, TXN_STATUS_RECOVERY);
        pTxnQ->aFuncInfo[uFuncId].fTxnQueueDoneCb (pTxnQ->aFuncInfo[uFuncId].hCbHandle, pTxn, MCP_TRUE);

        /* Clear the restarted function queues (call function CB with status=RECOVERY) */
        txnQ_ClearQueues (pTxnQ, uFuncId, MCP_TRUE);
    }

    /* In the normal case (no restart), enqueue completed transaction in TxnDone queue */
    else 
    {
        que_Enqueue (pTxnQ->hTxnDoneQueue, (handle_t)pTxn);
    }

    /* Indicate that no transaction is currently processed in the bus-driver */
    pTxnQ->pCurrTxn = NULL;

    /* Send queued transactions as possible (TRUE indicates we are in external context) */
    txnQ_RunScheduler (pTxnQ, NULL, MCP_TRUE); 

    /* Leave critical section if configured to do so */
    if (pTxnQ->bProtectTxnDone)
    {
        MCPF_EXIT_CRIT_SEC (pTxnQ->hMcpf);
    }
}


/** 
 * \fn     txnQ_RunScheduler
 * \brief  Send queued transactions
 * 
 * Issue transactions as long as they are available and the bus is not occupied.
 * Call CBs of completed transactions, except conpletion of pCurrTxn (covered by the return value).
 * Note that this function can't be preempted, since it is always called in critical section 
 * See txnQ_Transact(), txnQ_SendCtrlByte(), txnQ_Run() and txnQ_TxnDoneCb().
 * 
 * \note   
 * \param  pTxnQ            - The module's object
 * \param  pCurrTxn         - The transaction inserted in the current context (NULL if none)
 * \param  bExternalContext - TRUE if called in external context (TxnDone)
 * \return COMPLETE if pCurrTxn completed in this context, PENDING if not, ERROR if failed
 * \sa     
 */ 
static ETxnStatus txnQ_RunScheduler (TTxnQObj *pTxnQ, TTxnStruct *pInputTxn, McpBool bExternalContext)
{
    /* Use as return value the status of the input transaction (PENDING unless sent and completed here) */
    ETxnStatus eInputTxnStatus = TXN_STATUS_PENDING; 
	McpBool loopCond = MCP_TRUE;

    /* if a previous transaction is in progress, return PENDING */
    if (pTxnQ->pCurrTxn)
    {
        return TXN_STATUS_PENDING;
    }

    /* Loop while transactions are available and can be sent to bus driver */
    while (loopCond)
    {
        TTxnStruct   *pSelectedTxn;
        ETxnStatus    eStatus;

#ifdef TRAN_DBG
#define TXN_MAX_LOOP_ITERATES 	100
		int			  loopIterates = 0;

		if (loopIterates++ >= TXN_MAX_LOOP_ITERATES)
		{
			MCPF_REPORT_WARNING (pTxnQ->hMcpf, QUEUE_MODULE_LOG, 
								 ("txnQ_RunScheduler loop reached max iterates=%d\n", loopIterates));
		}
#endif

        /* Get next enabled transaction by priority. If none, exit loop. */
        pSelectedTxn = txnQ_SelectTxn (pTxnQ);
        if (pSelectedTxn == NULL)
        {
            break;
        }

        /* Send selected transaction to bus driver */
        eStatus = busDrv_Transact (pTxnQ->hBusDrv, pSelectedTxn);

        /* If we've just sent the input transaction, use the status as the return value */
        if (pSelectedTxn == pInputTxn)
        {
            eInputTxnStatus = eStatus;
        }

        /* If transaction completed */
        if (eStatus == TXN_STATUS_COMPLETE)
        {
            /* If it's not the input transaction, enqueue it in TxnDone queue */
            if (pSelectedTxn != pInputTxn)
            {
                que_Enqueue (pTxnQ->hTxnDoneQueue, (handle_t)pSelectedTxn);
            }
        }

        /* If pending or error */
        else 
        {
            /* If transaction pending, save it to indicate that the bus driver is busy */
            if (eStatus == TXN_STATUS_PENDING)
            {
                pTxnQ->pCurrTxn = pSelectedTxn;
            }

            /* Exit loop! */
            break;
        }
    }

    /* Dequeue completed transactions and call their functional driver CB */
    /* Note that it's the functional driver CB and not the specific CB in the Txn! */
    while (loopCond)
    {
        TTxnStruct      *pCompletedTxn;
        McpU32        uFuncId;
        TTxnQueueDoneCb  fTxnQueueDoneCb;
        handle_t        hCbHandle;

        pCompletedTxn   = (TTxnStruct *) que_Dequeue (pTxnQ->hTxnDoneQueue);
        if (pCompletedTxn == NULL)
        {
            /* Return the status of the input transaction (PENDING unless sent and completed here) */
            return eInputTxnStatus;
        }

        uFuncId         = TXN_PARAM_GET_FUNC_ID(pCompletedTxn);
        fTxnQueueDoneCb = pTxnQ->aFuncInfo[uFuncId].fTxnQueueDoneCb;
        hCbHandle       = pTxnQ->aFuncInfo[uFuncId].hCbHandle;

        fTxnQueueDoneCb (hCbHandle, pCompletedTxn, bExternalContext);
    }

	return TXN_STATUS_PENDING;
}


/** 
 * \fn     txnQ_SelectTxn
 * \brief  Select transaction to send
 * 
 * Called from txnQ_RunScheduler() which is protected in critical section.
 * Select the next enabled transaction by priority.
 * 
 * \note   
 * \param  pTxnQ - The module's object
 * \return The selected transaction to send (NULL if none available)
 * \sa     
 */ 
static TTxnStruct *txnQ_SelectTxn (TTxnQObj *pTxnQ)
{
    TTxnStruct *pSelectedTxn;
    McpU32   uFunc;
    McpU32   uPrio;

    /* For all functions, if single-step Txn waiting, return it (sent even if function is stopped) */
    for (uFunc = pTxnQ->uMinFuncId; uFunc <= pTxnQ->uMaxFuncId; uFunc++)
    {
        pSelectedTxn = pTxnQ->aFuncInfo[uFunc].pSingleStep;
        if (pSelectedTxn != NULL)
        {
            pTxnQ->aFuncInfo[uFunc].pSingleStep = NULL;
            return pSelectedTxn;
        }
    }

    /* For all priorities from high to low */
    for (uPrio = 0; uPrio < TXN_MAX_PRIORITY; uPrio++)
    {
        /* For all functions */
        for (uFunc = pTxnQ->uMinFuncId; uFunc <= pTxnQ->uMaxFuncId; uFunc++)
        {
            /* If function running and uses this priority */
            if (pTxnQ->aFuncInfo[uFunc].eState == FUNC_STATE_RUNNING  &&
                pTxnQ->aFuncInfo[uFunc].uNumPrios > uPrio)
            {
                /* Dequeue Txn from current func and priority queue, and if not NULL return it */
                pSelectedTxn = (TTxnStruct *) que_Dequeue (pTxnQ->aTxnQueues[uFunc][uPrio]);
                if (pSelectedTxn != NULL)
                {
                    return pSelectedTxn;
                }
            }
        }
    }

    /* If no transaction was selected, return NULL */
    return NULL;
}


/** 
 * \fn     txnQ_ClearQueues
 * \brief  Clear the function queues
 * 
 * Clear the specified function queues and call its CB for each Txn with status=RECOVERY.
 * 
 * \note   Called in critical section.
 * \param  pTxnQ            - The module's object
 * \param  uFuncId          - The calling functional driver
 * \param  bExternalContext - TRUE if called in external context (TxnDone)
 * \return void
 * \sa     
 */ 
static void txnQ_ClearQueues (TTxnQObj *pTxnQ, McpU32 uFuncId, McpBool bExternalContext)
{
    TTxnStruct      *pTxn;
    McpU32        uPrio;
    TTxnQueueDoneCb  fTxnQueueDoneCb = pTxnQ->aFuncInfo[uFuncId].fTxnQueueDoneCb; /* function TxnDone CB */
    handle_t        hCbHandle       = pTxnQ->aFuncInfo[uFuncId].hCbHandle;
	McpBool loopCond = MCP_TRUE;

    /* If single step waiting in this function, call function CB with recovery indication */
    pTxn = pTxnQ->aFuncInfo[uFuncId].pSingleStep;
    if (pTxn != NULL)
    {
        TXN_PARAM_SET_STATUS(pTxn, TXN_STATUS_RECOVERY);
        fTxnQueueDoneCb (hCbHandle, pTxn, bExternalContext);
    }

    /* For all function priorities */
    for (uPrio = 0; uPrio < pTxnQ->aFuncInfo[uFuncId].uNumPrios; uPrio++)
    {
        while (loopCond) 
        {
            /* Dequeue Txn from current priority queue */
            pTxn = (TTxnStruct *) que_Dequeue (pTxnQ->aTxnQueues[uFuncId][uPrio]);

            /* If NULL Txn (queue empty), exit while loop */
            if (pTxn == NULL)
            {
                break;
            }

            /* Call function CB with recovery indication in the Txn */
            TXN_PARAM_SET_STATUS(pTxn, TXN_STATUS_RECOVERY);
            fTxnQueueDoneCb (hCbHandle, pTxn, bExternalContext);
        }
    }
}


/** 
 * \fn     txnQ_GetBusDrvHandle
 * \brief  Gets bus driver handle
 * 
 * Returns bus driver handler registered to Transaction queue object
 * 
 * \note   
 * \param  pTxnQ            - The module's object
 * \return bus driver handle
 * \sa     
 */ 
handle_t	txnQ_GetBusDrvHandle (handle_t hTxnQ)
{
    TTxnQObj		*pTxnQ   = (TTxnQObj*)hTxnQ;

	return pTxnQ->hBusDrv;
}

/** 
 * \fn     txnQ_IsQueueEmpty
 * \brief  Return transaction queue status whether it is empty or not
 * 
 * The function return TRUE, if all the priority queues for specified function ID  are empty,
 * otherwise - TRUE
 * 
 * \note   critical section is required in caller context
 * \param  pTxnQ   - Pointer to transaction queue object
 * \param  uFuncId - Function ID
 * \return TRUE - queues are empty, FALSE - at least one queue is not empty
 * \sa     
 */ 
McpBool txnQ_IsQueueEmpty (const handle_t hTxnQ, const McpU32 uFuncId)
{
    TTxnQObj *pTxnQ = (TTxnQObj*)hTxnQ;
    McpU32   uPrio;
	McpU32	 uLen;
	McpBool	 bEmpty = MCP_TRUE;

    /* For all function priorities */
    for (uPrio = 0; uPrio < pTxnQ->aFuncInfo[uFuncId].uNumPrios; uPrio++)
    {
		uLen = que_Size (pTxnQ->aTxnQueues[uFuncId][uPrio]);
		if (uLen)
		{
			bEmpty = MCP_FALSE;
			break;
		}
    }
	return bEmpty;
}


#ifdef TRAN_DBG

void txnQ_PrintQueues (handle_t hTxnQ)
{
    TTxnQObj    *pTxnQ   = (TTxnQObj*)hTxnQ;

    que_Print(pTxnQ->aTxnQueues[TXN_FUNC_ID_CTRL][TXN_LOW_PRIORITY]);
}


void txnQ_testTx (handle_t hTxnQ, TTxnStruct *pTxn)
{
    TTxnQObj  *pTxnQ = (TTxnQObj*)hTxnQ;

	busDrv_Transact (pTxnQ->hBusDrv, pTxn);
}
#endif /* TRAN_DBG */



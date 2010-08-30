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


/** \file   transTest.c
 *  \brief  Test program for Shared Transport Layer
 * 
 *  \see    mcp_transport.c, mcp_transport.h
 */

#include <Windows.h>
#include "mcpf_main.h"
#include "mcpf_report.h"
#include "mcp_hal_os.h"
#include "mcp_hal_pm.h"
#include "TxnDefs.h"
#include "TxnQueue.h"
#include "BusDrv.h"
#include "mcp_hciDefs.h"
#include "mcp_hal_uart.h"
#include "mcp_txnpool.h"
#include "mcp_IfSlpMng.h"
#include "mcp_transport.h"


/************************************************************************
 * Defines
 ************************************************************************/

#define TRANS_TXNQ_FUN_ID	0

#define TX_SIZE 			100
#define TEST_CHAN			4


handle_t	 hMcpf;
handle_t	 hTrans;
handle_t	 hPool;
TTransConfig tConf;
TBusDrvCfg   tBusCfg;

/************************************************************************
 * Types
 ************************************************************************/

/************************************************************************
 * Internal functions prototypes
 ************************************************************************/

void 		eventHandlerFun (handle_t hHandleCb, TTxnEvent * pEvent);
EMcpfRes 	receiveFun (handle_t hTrans, handle_t hCbParam, TTxnStruct *pTxn);
void		transmitDoneFun(handle_t hCbHandle, void *pTxn);
void 		testSendEvent(void);
void		testSendCommand (void);
void		testSendACL (void);
void		testSendSCO (void);
void 		testSendEventMultipleBuf (void);
void 		testSendNavc(void);
void		testSendHeadBuf(void);
void 		initTransport (void);
void 		transTestTx (void);
void 		showStat (void);
void 		testSendSleep (void);
void 		testSendAwake (void);
void		testBufAvailable(void);
void 		testTransDestroy(void);
void 		initDestroyTest( void );
void 		uartDrvTxTest(void);



/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/


/** 
 * \fn     main
 * \brief  Transport Layer module test main function
 * 
 */
int main(int argc, char* argv[])
{
	int			 cmd;
	McpBool		 doFlag = MCP_TRUE;

	MCPF_UNUSED_PARAMETER(argc);
	MCPF_UNUSED_PARAMETER(argv[0]);

	printf("Transport Test start\n");

	printf("Commands:\n");
	printf("0  - Init\n");
	printf("1  - Send Commnad\n");
	printf("2  - Send ACL\n");
	printf("3  - Send SCO\n");
	printf("4  - Send Event\n");
	printf("5  - Send Event Multiple Buf\n");
	printf("6  - Send Test\n");
	printf("7  - Send Head & Buf Test\n");
	printf("9  - Send Navc\n");
	printf("10 - State & Statistics\n");
	printf("11 - Send Sleep Ind\n");
	printf("12 - Send Awake Ind\n");
	printf("13 - Buf Available\n");
	printf("14 - Init-destroy test\n");
	printf("15 - Uart Tx Test\n");
	printf("20 - Destroy\n");
	printf("Exit 99\n");
	printf("\nEnter: ");

	while (doFlag)
	{
		scanf("%d", &cmd);
		switch (cmd)
		{
		case 0:
			printf("Initialization\n");
			initTransport();
			break;

		case 1:
			printf("Sending Command\n");
			testSendCommand();
			break;

		case 2:
			printf("Sending ACL data\n");
			testSendACL();
			break;

		case 3:
			printf("Sending SCO data\n");
			testSendSCO();
			break;

		case 4:
			printf("Sending Event\n");
			testSendEvent();
			break;

		case 5:
			printf("Sending event with 4 buffers\n");
			testSendEventMultipleBuf();
			break;

		case 6:
			printf("Trans Send Test\n");
			transTestTx();
			break;

		case 7:
			printf("Sending Header & Buf\n");
			testSendHeadBuf();
			break;

		case 9:
			printf("Sending Navc\n");
			testSendNavc();
			break;

		case 10:
			printf("State & Statistics\n");
			showStat();
			break;

		case 11:
			printf("Sending Sleep Ind\n");
			testSendSleep();
			break;

		case 12:
			printf("Sending Wakeup Ind\n");
			testSendAwake();
			break;

		case 13:
			printf("Buffer Available\n");
			testBufAvailable();
			break;

		case 14:
			printf("Init-destroy test\n");
			initDestroyTest();
			break;

		case 15:
			printf("Uart Tx Test\n");
			uartDrvTxTest();
			break;

		case 20:
			printf("Destroy\n");
			testTransDestroy();
			break;

		case 99:
			printf("Exiting\n");
			doFlag = MCP_FALSE;
			break;
		default:
			printf("Unknown command!\n");
		}
	}
	printf("Transport Test End\n");

	return 0;
}


void eventHandlerFun (handle_t hHandleCb, TTxnEvent * pEvent)
{
	MCPF_UNUSED_PARAMETER(hHandleCb);

	switch (pEvent->eEventType)
	{
	case Txn_InitComplInd:
		printf("eventHandlerFun: Txn_InitComplInd\n");
		break;

	case Txn_DestroyComplInd:
		printf("eventHandlerFun: Txn_DestroyComplInd\n");
		break;

	case Txn_WriteComplInd:
		printf("eventHandlerFun: Txn_WriteComplInd\n");
		break;

case Txn_ReadReadylInd:
		printf("eventHandlerFun: Txn_ReadReadylInd\n");
		break;

	case Txn_ErrorInd:
		printf("eventHandlerFun: Txn_ErrorInd, errId=%u modId=%u\n", 
			   pEvent->tEventParams.err.eErrId,
			   pEvent->tEventParams.err.eModuleId);
		break;

	default:

		printf("eventHandlerFun: unknown event received\n");


	}
}




/** 
 * \fn     initTransport
 * \brief  Transport Layer initialization
 * 
 */
void initTransport (void)
{
	EMcpfRes	 	status;
	TtxnPoolConfig  tPoolCfg;

	hMcpf = mcpf_create();

	hPool = txnPool_Create ( hMcpf, &tPoolCfg); 	/* TBD */

	tConf.fEventCb  = eventHandlerFun;
	tConf.hHandleCb = (void *) 0x33;
	tConf.maxChanNum= 40;
	tConf.maxPrioNum= 2;
	tConf.tChipId	= MCP_HAL_CHIP_ID_0;
	tConf.tTransId	= MCP_HAL_TRAN_ID_0;

	MCP_HAL_PM_Init();

	hTrans = trans_Create (&tConf, hMcpf, hPool);

	if (hTrans == NULL)
	{
		printf("Transport create error\n");
	}

	tBusCfg.tUartCfg.portNum = 1;
	tBusCfg.tUartCfg.uFlowCtrl = 1;
	tBusCfg.tUartCfg.uBaudRate = HAL_UART_SPEED_9600;
	tBusCfg.tUartCfg.XoffLimit = 1000;
	tBusCfg.tUartCfg.XonLimit  = 1000;


	status = trans_Init (hTrans, &tBusCfg);

	if (status != RES_OK)
	{
		printf("Transport initialization error\n");
	}

	status = trans_ChOpen (hTrans, 9, receiveFun, (handle_t) 9 /* param */);
	if (status != RES_OK)
	{
		printf("Transport channel open error\n");
	}
	
	status = trans_ChOpen (hTrans, 2, receiveFun, (handle_t) 2 /* param */);
	if (status != RES_OK)
	{
		printf("Transport channel open error\n");
	}

	status = trans_ChOpen (hTrans, 3, receiveFun, (handle_t) 3 /* param */);
	if (status != RES_OK)
	{
		printf("Transport channel open error\n");
	}

	status = trans_ChOpen (hTrans, 4, receiveFun, (handle_t) 4 /* param */);
	if (status != RES_OK)
	{
		printf("Transport channel open error\n");
	}

}


/** 
 * \fn     testSendData
 * \brief  Transport Layer initialization
 * 
 */

void testSendEvent (void)
{

	EMcpfRes	 status;
	TTxnStruct  *pTxn;
	McpU16 	 i, len=10, totLen;
	McpU8	*pCh;

	totLen = (McpU16) (len + HCI_PREAMBLE_SIZE(EVENT));

	pTxn = (TTxnStruct *) txnPool_AllocBuf (hPool, totLen);

	pCh = pTxn->aBuf[0];
	*(pCh++) = HCI_PKT_TYPE_EVENT;  /* pkt type */
	*(pCh++) = 1;  					/* opcode */
	*(pCh++) = (McpU8) len;
	
	for (i=0; i<len; i++, pCh++)
	{
		*pCh = (McpU8) (0x30 + i);
	}
	pTxn->aLen[0] = totLen;
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;


	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,4);

	status = trans_TxData (hTrans, pTxn);

	if (status != RES_OK)
	{
		printf("Transport send error\n");
	}
}

void testSendCommand (void)
{

	EMcpfRes	 status;
	TTxnStruct  *pTxn;
	McpU16	i, len=10, totLen;
	McpU8	*pCh;

	totLen = (McpU16) (len + HCI_PREAMBLE_SIZE(COMMAND));

	pTxn = (TTxnStruct *) txnPool_AllocBuf (hPool, totLen);

	pCh = pTxn->aBuf[0];
	*(pCh++) = HCI_PKT_TYPE_COMMAND;  	/* pkt type */
	*(pCh++) = 1;  						/* opcode 	*/
	*(pCh++) = 2;  						/* opcode 	*/
	*(pCh++) = (McpU8) len;
	
	for (i=0; i<len; i++, pCh++)
	{
		*pCh = (McpU8) (0x30 + i);
	}
	pTxn->aLen[0] = totLen;
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;


	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,1);

	status = trans_TxData (hTrans, pTxn);

	if (status != RES_OK)
	{
		printf("Transport send error\n");
	}
}

void testSendACL(void)
{

	EMcpfRes	 status;
	TTxnStruct  *pTxn;
	McpU16	i, len=10, totLen;
	McpU8	*pCh;

	totLen = (McpU16) (len + HCI_PREAMBLE_SIZE(ACL));

	pTxn = (TTxnStruct *) txnPool_AllocBuf (hPool, totLen);

	pCh = pTxn->aBuf[0];
	*(pCh++) = HCI_PKT_TYPE_ACL;  		/* pkt type */
	*(pCh++) = 1;  						/* opcode 	*/
	*(pCh++) = 2;  						/* opcode 	*/
	*(pCh++) = (McpU8) (0xFF & len);
	*(pCh++) = (McpU8) (len >> 8);
	
	for (i=0; i<len; i++, pCh++)
	{
		*pCh = (McpU8) (0x30 + i);
	}
	pTxn->aLen[0] = totLen;
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;


	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,2);

	status = trans_TxData (hTrans, pTxn);

	if (status != RES_OK)
	{
		printf("Transport send error\n");
	}
}


void testSendSCO (void)
{

	EMcpfRes	 status;
	TTxnStruct  *pTxn;
	McpU8	 i, len=10, totLen;
	McpU8	*pCh;

	totLen = (McpU8)(len + HCI_PREAMBLE_SIZE(SCO));

	pTxn = (TTxnStruct *) txnPool_AllocBuf (hPool, totLen);

	pCh = pTxn->aBuf[0];
	*(pCh++) = HCI_PKT_TYPE_SCO;  	/* pkt type */
	*(pCh++) = 1;  					/* opcode */
	*(pCh++) = 2;  					/* opcode */
	*(pCh++) = len;
	
	for (i=0; i<len; i++, pCh++)
	{
		*pCh = (McpU8) (0x30 + i);
	}
	pTxn->aLen[0] = totLen;
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;


	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,3);

	status = trans_TxData (hTrans, pTxn);

	if (status != RES_OK)
	{
		printf("Transport send error\n");
	}
}

void testSendEventMultipleBuf (void)
{

	EMcpfRes	 status;
	TTxnStruct  *pTxn;
	McpU16 	 aSizeN[4];
	int			 i=0,j;
	McpU8	*pCh;
	McpU8	 base = 0x10;
	McpU8	 bufN = 4;
	McpU16	 len = 0;

	aSizeN[i++] = 10;
	aSizeN[i++] = 20;
	aSizeN[i++] = 30;
	aSizeN[i++] = 40;

	for (i=0; i < bufN; i++) len = (McpU16)(len + aSizeN[i]);

	pTxn = txnPool_AllocNBuf (hPool, bufN, aSizeN);

	/* Build HCI header of event message */
	pCh = pTxn->aBuf[0];
	*(pCh++) = HCI_PKT_TYPE_EVENT;  /* pkt type */
	*(pCh++) = 1;  /* opcode */
	*(pCh++) = (McpU8)(len - HCI_PREAMBLE_SIZE(EVENT));
	
	/* buf 0 */
	for (i=0; i < (aSizeN[0] - HCI_PREAMBLE_SIZE(EVENT)); i++, pCh++)
	{
		*pCh = (McpU8) (base + i);
	}
	pTxn->aLen[0] = aSizeN[0];

	/* bufs 1 - 3 */
	for (j=1; j < bufN; j++)
	{
		pCh = pTxn->aBuf[j];
		pTxn->aLen[j] = aSizeN[j];

		for (i=0; i < pTxn->aLen[j]; i++, pCh++)
		{
			*pCh = (McpU8) (base + i);
		}
	}

	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;


	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,4);

	status = trans_TxData (hTrans, pTxn);

	if (status != RES_OK)
	{
		printf("Transport send error\n");
	}
}



void testSendNavc (void)
{

	EMcpfRes	 status;
	TTxnStruct  *pTxn;
	McpU16 	 i, len=40, totLen;
	McpU8	*pCh;

	totLen = (McpU16) (len + HCI_PREAMBLE_SIZE(NAVC));

	pTxn = (TTxnStruct *) txnPool_AllocBuf (hPool, totLen);

	pCh = pTxn->aBuf[0];
	*(pCh++) = HCI_PKT_TYPE_NAVC;  				/* pkt type */
	*(pCh++) = 1;  								/* opcode */
	*(pCh++) = (McpU8) (0xFF & len);  		/* little endian, len low byte */
	*(pCh++) = (McpU8) (0xFF & len >> 8);  	/* little endian, len high  byte*/
	
	for (i=0; i<len; i++, pCh++)
	{
		*pCh = (McpU8) (0x30 + i);
	}

	pTxn->aLen[0] = totLen;
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;


	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,9);

	status = trans_TxData (hTrans, pTxn);

	if (status != RES_OK)
	{
		printf("Transport send error\n");
	}
}

void testSendHeadBuf(void)
{

	EMcpfRes	 status;
	TTxnStruct  *pTxn;
	McpU16 	 i=0, len=40;
	McpU8	*pCh;

	pTxn = (TTxnStruct *) txnPool_AllocBuf (hPool, len);

	pTxn->tHeader.tHciHeader.uPktType 	  = HCI_PKT_TYPE_EVENT;
	pTxn->tHeader.tHciHeader.aHeader[i++] = 1;  				/* opcode */
	pTxn->tHeader.tHciHeader.aHeader[i++] = (McpU8) len;
	pTxn->tHeader.tHciHeader.uLen		  = HCI_PREAMBLE_SIZE(EVENT);

	pCh = pTxn->aBuf[0];
	
	for (i=0, pCh = pTxn->aBuf[0]; i < len; i++, pCh++)
	{
		*pCh = (McpU8) (0x30 + i);
	}
	pTxn->aLen[0] = len;
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;


	TXN_PARAM_SET_TXN_FLAG(pTxn,1);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,4);

	status = trans_TxData (hTrans, pTxn);

	if (status != RES_OK)
	{
		printf("Transport send error\n");
	}
}


void transTestTx (void)
{
	TTxnStruct  *pTxn;
	int			 i, len=10;
	McpU8		*pCh;

	pTxn = txnPool_AllocBuf (hPool, TX_SIZE);

	pCh = pTxn->aBuf[0];
	*(pCh++) = 4;  /* pkt type */
	*(pCh++) = 1;  /* opcode */
	*(pCh++) = (McpU8) len;
	
	for (i=0; i<len; i++, pCh++)
	{
		*pCh = (McpU8) (0x30 + i);
	}
	pTxn->aLen[0] = (McpU16) (len+3);
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;


	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,TEST_CHAN);

	trans_testTx (hTrans, pTxn);

}

extern /* TBusDrvObj * */ g_pUartBusDrvObj;

void uartDrvTxTest(void)
{
	TTxnStruct  *pTxn;
	int			 i, len=10;
	McpU8		*pCh;

	pTxn = txnPool_AllocBuf (hPool, TX_SIZE);

	pCh = pTxn->aBuf[0];

	for (i=0; i<len; i++, pCh++)
	{
		*pCh = (McpU8) (0x60 + i);
	}

	pTxn->aLen[0] = (McpU16) len;
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;


	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,TEST_CHAN);

	busDrv_Transact ((handle_t) g_pUartBusDrvObj, pTxn);
}



char * ifSlpMngStateName[IFSLPMNG_STATE_MAX] = 
{
	"None",
	"Asleep",
	"WaitForAwakeAck",
	"Awake",
	"WaitForSleepAckTxCompl"
};

/* UART Bus Driver receiver state */
char * busDrvStateName[BUSDRV_STATE_RX_MAX] =
{
	"None",
    "PktType",
    "Header",
	"Data",
	"Congestion"
};

void showStat (void)
{
	TTransAllStat stat;

	trans_GetStat (hTrans, &stat);

	printf("------------ Transport Statistics --------------\n");
	printf("Tx=%u TxCompl=%u\n", 
		   stat.transStat.Tx, stat.transStat.TxCompl);

	printf("RxInd=%u BufAvl=%u Errors=%u\n",
		   stat.transStat.RxInd, stat.transStat.BufAvail, stat.transStat.Error);

	printf("------------ Interace Sleep Mng Statistics------\n");
	printf("Tx: Awake=%u AwakeAck=%u SleepAck=%u SleepAckTxCompl=%u\n",
		   stat.ifSlpMngStat.AwakeInd_Tx,
		   stat.ifSlpMngStat.AwakeAck_Tx,
		   stat.ifSlpMngStat.SleepAck_Tx,
		   stat.ifSlpMngStat.SleepAckTxCompl);

	printf("Rx: Awake=%u AwakeAck=%u Sleep=%u SleepAck=%u\n",
		   stat.ifSlpMngStat.AwakeInd_Rx,
		   stat.ifSlpMngStat.AwakeAck_Rx,
		   stat.ifSlpMngStat.SleepInd_Rx,
		   stat.ifSlpMngStat.SleepAck_Rx);
	printf("State = %s\n", ifSlpMngStateName[stat.ifSlpMngState]);

	printf("------------ Bus Driver Statistics -------------\n");
	printf("Tx=%u TxCompl=%u TxErr=%u\n",
		   stat.busDrvStat.Tx, stat.busDrvStat.TxCompl, stat.busDrvStat.TxErr);
	printf("Rx=%u RxIndErr=%u RxErr=%u\n",
		   stat.busDrvStat.RxInd, stat.busDrvStat.RxIndErr, stat.busDrvStat.RxErr );
	printf("BufAvl=%u AllocErr=%u\n",
		   stat.busDrvStat.BufAvail, stat.busDrvStat.AllocErr);
	printf("Rx State = %s\n", busDrvStateName[stat.busDrvState]);
}


void testSendSleep (void)
{
	EMcpfRes	 status;
	TTxnStruct  *pTxn;
	McpU16 	 len=1;

	pTxn = (TTxnStruct *) txnPool_AllocBuf (hPool, len);

	*(pTxn->aBuf[0]) = HCI_PKT_TYPE_SLEEP_IND;
	pTxn->aLen[0] = len;
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;

	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,0x30);

	status = trans_TxData (hTrans, pTxn);

	if (status != RES_OK)
	{
		printf("Transport send error\n");
	}
}

void testSendAwake (void)
{
	EMcpfRes	 status;
	TTxnStruct  *pTxn;
	McpU16 	 len=1;

	pTxn = (TTxnStruct *) txnPool_AllocBuf (hPool, len);

	*(pTxn->aBuf[0]) = HCI_PKT_TYPE_WAKEUP_IND;
	pTxn->aLen[0] = len;
	pTxn->pData = pTxn->aBuf[0];
	pTxn->fTxnDoneCb = (TTxnDoneCb) transmitDoneFun;
	pTxn->hCbHandle  = pTxn;

	pTxn->uTxnParams = 0;

	TXN_PARAM_SET_TXN_FLAG(pTxn,0);
	TXN_PARAM_SET_IFSLPMNG_OP(pTxn,0);
	TXN_PARAM_SET_CHAN_NUM(pTxn,0x32);

	status = trans_TxData (hTrans, pTxn);

	if (status != RES_OK)
	{
		printf("Transport send error\n");
	}
}


void testBufAvailable(void)
{
	trans_BufAvailable (hTrans);
}

void testTransDestroy(void)
{
	trans_Destroy (hTrans);

	MCP_HAL_PM_Deinit ();

	txnPool_Destroy (hPool);

	mcpf_destroy (hMcpf);
}



EMcpfRes receiveFun (handle_t hTrans, handle_t hCbParam, TTxnStruct *pTxn)
{
	static int congestCount=0;

	printf("receiveFun: hTran=%u param=%u pTxn=%p\n", hTrans, hCbParam, pTxn);
	
	congestCount++;

	if ((McpU32)hCbParam == 3 && congestCount%2)
	{
		/* Congestion test for SCO data */
		return RES_ERROR;
	}
	else
	{
		txnPool_FreeBuf (hPool, pTxn);
		return RES_OK;
	}
}


void transmitDoneFun(handle_t hCbHandle, void *pTxn)
{
	MCPF_UNUSED_PARAMETER(hCbHandle);

	printf("transmitDoneFun: pTxn=%p\n");
	txnPool_FreeBuf (hPool, pTxn);
}


void initDestroyTest( void )
{
	int i;

	for( i=0; i<10; i++ )
	{
		initTransport();
		Sleep(1000);
		testTransDestroy();
	}

}


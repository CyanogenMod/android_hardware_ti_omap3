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



/** \file   ccm_adapt.c 
 *  \brief  Connectivite Chip Manager (CCM) adapter for Host Controller Interface (HCI)
 * 		    implementation. 
 * 
 *  \see    ccm_adapt.h, mcp_hci_adapt.h
 */

#include "mcpf_mem.h"
#include "mcpf_services.h"
#include "mcpf_report.h"
#include "mcp_txnpool.h"
#include "mcp_transport.h"
#include "mcp_hciDefs.h"
#include "mcp_hci_adapt.h"
#include "ccm_adapt.h"
#ifdef MCP_STK_ENABLE
#include "mcp_hal_config.h"
#include "mcp_hal_hci.h"
#endif

MCP_HAL_LOG_SET_MODULE(MCP_HAL_LOG_MODULE_TYPE_CCM);
/*******************************************************************************
 * Defines
 ******************************************************************************/

#ifdef MCP_STK_ENABLE

#define CCMA_NAME                                       "CCMA ST"

/* Types of Rx header */
#define CCMA_HEADER_TYPE_RX                             (0)
#define CCMA_HEADER_TYPE_ERROR                          (1)
#define CCMA_HEADER_TYPE_TX_COMPLETE                    (2)

/* HCI event to register with HAL ST module */
#define CCMA_HCI_EVENT_COMMAND_COMPLETE                 (0x0e)

/* Max HCI event packet size */
#define CCMA_HCI_EVENT_PACKET_MAX_SIZE                  (257)

/* HCI event packet offsets */
#define CCMA_HCI_EVENT_PACKET_OFFSET_TO_TYPE            (1)
#define CCMA_HCI_EVENT_PACKET_OFFSET_TO_PARAMS_LEN      (2)
#define CCMA_HCI_EVENT_PACKET_OFFSET_TO_PARAMS          (3)

/* HCI Command Complete event offset to command opcode */
#define CCMA_HCI_CMD_CMPLT_EVENT_OFFSET_TO_CMD_OPCODE   (4)

/* Bit Mask Packet Type for channel filtering */
#define CCMA_HCI_PKT_TYPE_EVT		0x00000010

#endif


/*******************************************************************************
 * Types
 ******************************************************************************/

/* Registered client node */
typedef struct
{
    MCP_DL_LIST_Node tHead;          	/* Header for link list */
    handle_t hCcma;
    
    CcmaClientCb clientCb;
    handle_t hClientUserData;

} TClientNode;

/* CCMA object type */
typedef struct
{
    handle_t hMcpf;
    McpHalChipId eChipId;
    CcmaMngrCb fMngrCb;
    MCP_DL_LIST_Node tClientList;        /* list of registered clients */

#ifdef MCP_STK_ENABLE
    /* Handle to underlying transport module */
    handle_t hHalHci;
    
    /* Chip's version */
    THalHci_ChipVersion chipVersion;   
#endif

} TCcmaObj;


/*******************************************************************************
 * Internal functions prototypes
 ******************************************************************************/

#ifdef MCP_STK_ENABLE
static EMcpfRes ccmaReceiveCb(handle_t hCcma, McpU8 *pkt, McpU32 len, eHalHciStatus status);
static EMcpfRes clientCmdComplete(handle_t hHandle, McpU8 *pkt, McpU32 len, eHalHciStatus status);
#else
static void clientCmdComplete(handle_t hHandle, THciaCmdComplEvent *pEvent);
static void transportOnComplete(handle_t hHandle, THciaCmdComplEvent *pEvent);
#endif
static void mngrCmdComplete(handle_t hHandle, THciaCmdComplEvent *pEvent);
static CcmaStatus ccmStatus(EMcpfRes eRes);


/*******************************************************************************
 *
 *   Module functions implementation
 *
 ******************************************************************************/

/** 
 * \fn     CCMA_Create 
 * \brief  Create CCM adapter object
 * 
 */ 
handle_t CCMA_Create(const handle_t hMcpf,
                     const McpHalChipId chipId,
                     const CcmaMngrCb mngrCb)
{
    TCcmaObj *pCcma;

    pCcma = mcpf_mem_alloc(hMcpf, sizeof(TCcmaObj));
    MCPF_Assert(pCcma);
    mcpf_mem_zero(hMcpf, pCcma, sizeof(TCcmaObj));

    pCcma->hMcpf = hMcpf;
    pCcma->eChipId = chipId;
    pCcma->fMngrCb = mngrCb;

    MCP_DL_LIST_InitializeHead(&pCcma->tClientList);

    return(handle_t) pCcma;
}

/** 
 * \fn     CCMA_Destroy 
 * \brief  Destroy CCM adapter object
 * 
 */ 
void CCMA_Destroy (handle_t hCcma)
{
	TCcmaObj *pCcma = (TCcmaObj *)hCcma;
	MCP_DL_LIST_Node *pNode, *pNext;

	MCPF_ENTER_CRIT_SEC(pCcma->hMcpf);

	/* Free all the nodes from the client list */
	for (pNode = pCcma->tClientList.next; pNode != &pCcma->tClientList;)
	{
		pNext = pNode->next;
		mcpf_mem_free (pCcma->hMcpf, pNode);
		pNode = pNext;
	}
	MCPF_EXIT_CRIT_SEC(pCcma->hMcpf);

	mcpf_mem_free(pCcma->hMcpf, pCcma);    
}

/** 
 * \fn     CCMA_changeCallBackFun 
 * \brief  Change manager call back function
 * 
 */ 
void CCMA_changeCallBackFun(handle_t hCcma, CcmaMngrCb pNewCb, CcmaMngrCb *pOldCb)
{
	TCcmaObj *pCcma = (TCcmaObj *)hCcma;

    if (pOldCb != NULL)
    {
        *pOldCb = pCcma->fMngrCb;
    }
    /* Set the new cb */
    pCcma->fMngrCb = pNewCb;
}

/** 
 * \fn     CCMA_GetChipId
 * \brief  return ccmA chipId
 * 
 * The function provides chipId that corresponds to CCMA handle
 * 
 * \note
 * \param	hCcma - pointer to client handle 
 * \return 	chipId registered
 * \sa     	CCMA_GetChipId
 */ 
McpHalChipId CCMA_GetChipId(handle_t hCcma)
{
    return ((TCcmaObj *)hCcma)->eChipId;
}

/** 
 * \fn     CCMA_TransportOn 
 * \brief  Start transport layer initialization 
 * 
 */ 
EMcpfRes CCMA_TransportOn(handle_t hCcma)
{
    TCcmaObj *pCcma = (TCcmaObj *)hCcma;
    EMcpfRes eRes;

    MCP_FUNC_START("CCMA_TransportOn");


#ifdef MCP_STK_ENABLE

    pCcma->hHalHci = hal_hci_OpenSocket(pCcma->hMcpf, 0, CCMA_HCI_PKT_TYPE_EVT, NULL, ccmaReceiveCb, hCcma);


    /* Chip is now on and STK wrote its version into the file, get it */
    eRes = hal_hci_GetChipVersion(pCcma->hHalHci, &pCcma->chipVersion);
    MCPF_Assert(RES_OK == eRes);
    
    MCP_LOG_INFO(("CCMA_GetChipVersion: %d.%d.%d, full - 0x%04x",
                  pCcma->chipVersion.projectType,
                  pCcma->chipVersion.major,
                  pCcma->chipVersion.minor,
                  pCcma->chipVersion.full));
    
#else

    eRes = HCIA_SendCommand (((Tmcpf *)pCcma->hMcpf)->hHcia,
							 CCMA_HCI_CMD_HCC_RESET, 
							 NULL, 0,
							 transportOnComplete, 
							 hCcma);
#endif


    MCP_FUNC_END();

    return eRes;
}

/** 
 * \fn     CCMA_MngrSendRadioCommand 
 * \brief  Send HCI command over transport layer
 * 
 */ 
EMcpfRes CCMA_MngrSendHciCommand(handle_t *hCcma,    
                                 CcmaHciOpcode hciOpcode, 
                                 McpU8 *hciCmdParms, 
                                 McpU8 hciCmdParmsLen)
{
    TCcmaObj *pCcma = (TCcmaObj *)hCcma;
    EMcpfRes eRes;

    MCP_FUNC_START("CCMA_MngrSendHciCommand");

    eRes = HCIA_SendCommand(((Tmcpf *)pCcma->hMcpf)->hHcia,
			    (McpU16) hciOpcode, 
			    hciCmdParms, hciCmdParmsLen,
			    mngrCmdComplete, 
			    hCcma);

    MCP_FUNC_END();

    return eRes;
}

/** 
 * \fn     CCMA_SetTranParms 
 * \brief  Configure transport layer
 * 
 */ 
EMcpfRes CCMA_SetTranParms (handle_t hCcma, CcmaTranParms *pTranParms)
{
	TCcmaObj *pCcma = (TCcmaObj *)hCcma;
    EMcpfRes eRes;
    McpU16 uSpeedInKbps;
    
    MCP_FUNC_START("CCMA_SetTranParms");

    uSpeedInKbps = (McpU16)(pTranParms->parms.uartParms.speed / 1000);

    MCP_LOG_INFO(("Setting UART Speed to %d Kbps", uSpeedInKbps));

    eRes = mcpf_trans_SetSpeed(pCcma->hMcpf, uSpeedInKbps);
        
    MCP_FUNC_END();

    return eRes;
}

/** 
 * \fn     CCMA_TransportOff 
 * \brief  Stops hcia adapter
 * 
 */ 
EMcpfRes CCMA_TransportOff(handle_t hCcma)
{
    EMcpfRes eRes = RES_OK;
    TCcmaObj *pCcma = (TCcmaObj *)hCcma;
#ifndef MCP_STK_ENABLE
    CcmaMngrEvent ccmaMngrEvent;

    eRes = HCIA_CmdQueueFlush (((Tmcpf *)pCcma->hMcpf)->hHcia);

    ccmaMngrEvent.reportingObj = hCcma;
    ccmaMngrEvent.type = CCMA_MNGR_EVENT_TRANPORT_OFF_COMPLETED;
    ccmaMngrEvent.status = CCMA_STATUS_SUCCESS;
    
    (pCcma->fMngrCb)(&ccmaMngrEvent);
#else

    hal_hci_CloseSocket(pCcma->hHalHci);

#endif

	return eRes;
}

/** 
 * \fn     CCMA_RegisterClient 
 * \brief  Register a client to CCMA
 * 
 */ 
EMcpfRes CCMA_RegisterClient(handle_t hCcma,
                             CcmaClientCb callBackFun,
                             CcmaClientHandle *pClientHandle)
{
	TCcmaObj *pCcma = (TCcmaObj *)hCcma;
	TClientNode *pClient;

	pClient = mcpf_mem_alloc (pCcma->hMcpf, sizeof(TClientNode));
	MCPF_Assert(pClient);
	mcpf_mem_zero (pCcma->hMcpf, pClient, sizeof(TClientNode));

	pClient->clientCb = callBackFun;
	pClient->hCcma = hCcma;

	MCPF_ENTER_CRIT_SEC(pCcma->hMcpf);

	MCP_DL_LIST_InsertTail (&pCcma->tClientList, &pClient->tHead);

	MCPF_EXIT_CRIT_SEC(pCcma->hMcpf);

	*pClientHandle = pClient;

	return RES_OK;
}

/** 
 * \fn     CCMA_DeregisterClient
 * \brief  Deregister a client from CCMA
 * 
 */ 
EMcpfRes CCMA_DeregisterClient(CcmaClientHandle *pClientHandle)
{
	TClientNode *pClient = (TClientNode *)(*pClientHandle);
	TCcmaObj *pCcma = (TCcmaObj *)pClient->hCcma;

	MCPF_ENTER_CRIT_SEC(pCcma->hMcpf);

	MCP_DL_LIST_RemoveNode(&pClient->tHead);

	MCPF_EXIT_CRIT_SEC(pCcma->hMcpf);

	*pClientHandle = NULL;

	return RES_OK;
}

/** 
 * \fn     CCMA_SendHciCommand 
 * \brief  Send HCI command over transport layer
 * 
 */ 
CcmaStatus CCMA_SendHciCommand(CcmaClientHandle hClientHandle,
                               CcmaHciOpcode eHciOpcode, 
                               McpU8 *pHciCmdParms, 
                               McpU8 uHciCmdParmsLen,
                               McpU8 uEeventType,
                               void *pUserData)
{
    TClientNode *pClient = (TClientNode *)hClientHandle;
    TCcmaObj *pCcma = (TCcmaObj *)pClient->hCcma;
    CcmaStatus eStatus;
#ifdef MCP_STK_ENABLE
    eHalHciStatus eResult;
#else
    EMcpfRes eRes;
#endif

    MCP_FUNC_START("CCMA_SendHciCommand");

    MCP_UNUSED_PARAMETER(uEeventType);

    /* Save user's data for future use */
    pClient->hClientUserData = pUserData;

#ifdef MCP_STK_ENABLE

    eResult = hal_hci_SendCommand(pCcma->hHalHci, eHciOpcode, 
    								  pHciCmdParms, uHciCmdParmsLen, 
		      						  clientCmdComplete, hClientHandle);

    
    
        if (eResult != HAL_HCI_STATUS_PENDING)
        {
            MCP_LOG_ERROR(("Error writing HCI packet"));
            eStatus = CCMA_STATUS_FAILED;
        }
        else
        {
            eStatus = CCMA_STATUS_PENDING;
        }

#else

	eRes = HCIA_SendCommand(((Tmcpf *)pCcma->hMcpf)->hHcia,
                            (McpU16) eHciOpcode, 
                            pHciCmdParms, 
                            uHciCmdParmsLen,
                            clientCmdComplete, 
                            hClientHandle);
    
	if (eRes == RES_OK)
	{
		eStatus = CCMA_STATUS_PENDING;
	}
	else
	{
		eStatus = CCMA_STATUS_FAILED;
	}

#endif
	
	MCP_FUNC_END();

	return eStatus;

}

#ifdef MCP_STK_ENABLE
/** 
 * \fn     CCMA_GetChipVersion
 * \brief  Returns chip's version
 * 
 */ 
CcmaStatus CCMA_GetChipVersion(handle_t hCcma,
                               THalHci_ChipVersion *chipVersion)
{
    TCcmaObj *pCcma = (TCcmaObj *)hCcma;
    
    MCP_FUNC_START("CCMA_GetChipVersion");

    /* Copy chip's version from the local data */
    *chipVersion = pCcma->chipVersion;
    
    MCP_FUNC_END();
    
    return CCMA_STATUS_SUCCESS;
}
#endif


/*******************************************************************************
 * Module Private Functions
 ******************************************************************************/

#ifdef MCP_STK_ENABLE

/** 
 * \fn     ccmaReceiveCb 
 * \brief  Receive callback from the BT driver
 * 
 * The function implements a callback for receiving events from the BT driver
 * 
 * \note
 * \param	hCcma - handle to CCMA object returned by the HAL ST
 * \param	pkt - received packet
 * \param	len - received packet's length
  * \param	status - Receive's action status
 * \return 	RES_OK
 * \sa     	CCMA_SendHciCommand
 */ 
static EMcpfRes ccmaReceiveCb(handle_t hCcma, McpU8 *pkt, McpU32 len, eHalHciStatus status)
{	
	MCPF_UNUSED_PARAMETER(hCcma);
	MCPF_UNUSED_PARAMETER(pkt);
	MCPF_UNUSED_PARAMETER(len);
	MCPF_UNUSED_PARAMETER(status);
	
    	MCP_FUNC_START("ccmaReceiveCb");

    	MCP_LOG_ERROR(("ccmaReceiveCb: ERROR - It is unexpected to received RX other than CMD COMPLETE!"));            

    	MCP_FUNC_END();

	return RES_OK;
}

#else
 
/** 
 * \fn     transportOnComplete 
 * \brief  Transport initialization complete callback
 * 
 * The function sends events to indicate the completion of transport initialization
 * 
 * \note
 * \param	hHandle - handle to CCMA object
 * \param	pEvent  - pointer to event structure
 * \return 	void
 * \sa     	CCMA_TransportOn
 */ 
static void transportOnComplete(handle_t hHandle, THciaCmdComplEvent *pEvent)
{
	TCcmaObj      *pCcma = (TCcmaObj *)hHandle;
    CcmaMngrEvent ccmaMngrEvent;

	MCP_FUNC_START("CCMA: transportOnComplete");
 
    ccmaMngrEvent.reportingObj = (handle_t)pCcma;
    ccmaMngrEvent.type = CCMA_MNGR_EVENT_TRANPORT_ON_COMPLETED;
	ccmaMngrEvent.status = ccmStatus (pEvent->eResult);
    
    (pCcma->fMngrCb)(&ccmaMngrEvent);

    MCP_FUNC_END();
}
#endif

/** 
 * \fn     mngrCmdComplete 
 * \brief  Manager HCI command completion callback
 * 
 * The function sends events to indicate the completion of manager HCI command
 * issued by CCMA_MngrSendHciCommand()
 * 
 * \note
 * \param	hHandle - handle to CCMA object
 * \param	pEvent  - pointer to event structure
 * \return 	void
 * \sa     	CCMA_MngrSendHciCommand
 */ 
static void mngrCmdComplete(handle_t hHandle, THciaCmdComplEvent *pEvent)
{
	TCcmaObj *pCcma = (TCcmaObj *)hHandle;
    CcmaMngrEvent ccmaMngrEvent;
    
    MCP_FUNC_START("CCMA: mngrCmdComplete");

    ccmaMngrEvent.reportingObj = (handle_t) pCcma;
    ccmaMngrEvent.type = CCMA_MNGR_EVENT_SEND_RADIO_CMD_COMPLETED;
    ccmaMngrEvent.data.hciEventData.eventType = pEvent->uEvtOpcode;
    ccmaMngrEvent.data.hciEventData.parmLen   = pEvent->uEvtParamLen;
    ccmaMngrEvent.data.hciEventData.parms     = pEvent->pEvtParams;
	ccmaMngrEvent.status = ccmStatus (pEvent->eResult);

    (pCcma->fMngrCb)(&ccmaMngrEvent);

    MCP_FUNC_END();
}

/** 
 * \fn     clientCmdComplete 
 * \brief  Manager HCI command completion callback
 * 
 * The function sends events to indicate the completion of manager HCI command
 * issued by CCMA_MngrSendHciCommand()
 * 
 * \note
 * \param	hHandle - handle to CCMA object
 * \param	pEvent  - pointer to event structure
 * \return 	RES_OK /void
 * \sa     	CCMA_SendHciCommand
 */ 
#ifdef MCP_STK_ENABLE
static EMcpfRes clientCmdComplete(handle_t hHandle, McpU8 *pkt, McpU32 len, eHalHciStatus status)
#else
static void clientCmdComplete(handle_t hHandle, THciaCmdComplEvent *pEvent)
#endif
{
    TClientNode *pClient = (TClientNode *)hHandle;
    CcmaClientEvent tBtEvent;

#ifdef MCP_STK_ENABLE
    MCPF_UNUSED_PARAMETER(len);
    MCPF_UNUSED_PARAMETER(status);
#endif

    MCP_FUNC_START("CCMA: clientCmdComplete");

    tBtEvent.type = CCMA_CLIENT_EVENT_HCI_CMD_COMPLETE;
    tBtEvent.pUserData = pClient->hClientUserData;
    
#ifdef MCP_STK_ENABLE
    tBtEvent.status = CCMA_STATUS_SUCCESS;
    tBtEvent.data.hciEventData.eventType = pkt[CCMA_HCI_EVENT_PACKET_OFFSET_TO_TYPE];
    tBtEvent.data.hciEventData.parmLen = pkt[CCMA_HCI_EVENT_PACKET_OFFSET_TO_PARAMS_LEN];
    tBtEvent.data.hciEventData.parms = &pkt[CCMA_HCI_EVENT_PACKET_OFFSET_TO_PARAMS];
#else
    tBtEvent.status = ccmStatus(pEvent->eResult);
    tBtEvent.data.hciEventData.eventType = pEvent->uEvtOpcode;
    tBtEvent.data.hciEventData.parmLen = pEvent->uEvtParamLen;
    tBtEvent.data.hciEventData.parms = pEvent->pEvtParams;
#endif

    (pClient->clientCb)(&tBtEvent); 

    MCP_FUNC_END();

#ifdef MCP_STK_ENABLE
    return RES_OK;
#endif
}

/** 
 * \fn     ccmStatus 
 * \brief  Convert result to CCM status
 * 
 * The function converts MCPF result enumeration type to CCM status type
 * 
 * \note
 * \param	eRes - MCPF result
 * \return 	CCM status
 * \sa     	clientCmdComplete
 */ 
static CcmaStatus ccmStatus(EMcpfRes eRes)
{
	CcmaStatus eStatus;

	if (eRes == RES_OK)
	{
		eStatus = CCMA_STATUS_SUCCESS;
	}
	else
	{
		eStatus = CCMA_STATUS_FAILED;
	}
	return eStatus;
}


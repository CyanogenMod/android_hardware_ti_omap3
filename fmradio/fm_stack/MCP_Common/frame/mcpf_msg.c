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



/** \file   mcp_msg.c 
 *  \brief  MCPF message module implementation
 *
 *  \see    msp_msg.h
 */

#include "mcpf_msg.h"
#include "mcpf_mem.h"
#include "mcpf_services.h"
#include "mcpf_report.h"
#include "pla_os.h"

/************************************************************************
 * Defines
 ************************************************************************/


/************************************************************************
 * Types
 ************************************************************************/


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/


/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/** 
 * \fn     mcpf_SendMsg
 * \brief  Send MCPF message
 * 
 */ 
EMcpfRes	mcpf_SendMsg (handle_t		hMcpf,
						  EmcpTaskId	eDestTaskId,
						  McpU8			uDestQId,
						  EmcpTaskId	eSrcTaskId,
						  McpU8			uSrcQId,
						  McpU16		uOpcode,
						  McpU32		uLen,
						  McpU32		uUserDefined,
						  void 			*pData)
{
	Tmcpf		*pMcpf = (Tmcpf	*) hMcpf;
	TmcpfMsg	*pMsg;

	pMsg = (TmcpfMsg *)mcpf_mem_alloc_from_pool (hMcpf, pMcpf->hMsgPool);

	if (pMsg != NULL)
	{
		pMsg->eSrcTaskId = eSrcTaskId;
		pMsg->uSrcQId = uSrcQId;
		pMsg->uOpcode = uOpcode;
		pMsg->uLen 	  = uLen;
		pMsg->uUserDefined = uUserDefined;
		pMsg->pData   = pData;

		if (pMcpf->tClientsTable[eDestTaskId].fCb) 
		{
			pMcpf->tClientsTable[eDestTaskId].fCb (pMcpf->tClientsTable[eDestTaskId].hCb, 
													eDestTaskId, 
													uDestQId, 
													pMsg);
		}
		else
		{
			MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, 
								("mcpf_SendMsg: ERROR - Client's Cb is empty, freeing msg..."));
			mcpf_mem_free_from_pool(hMcpf, pMsg);
			return (RES_ERROR);
		}
	}
	else
	{
		return (RES_MEM_ERROR);
	}

	return (RES_OK);
}


/** 
 * \fn     mcpf_MsgqEnable
 * \brief  Enable message queue
 * 
 */ 
EMcpfRes	mcpf_MsgqEnable (handle_t 	 hMcpf, 
							 EmcpTaskId eDestTaskId,
							 McpU8		 uDestQId)
{
	Tmcpf		*pMcpf = (Tmcpf	*) hMcpf;
	TMcpfTask   *pTask = pMcpf->tTask[ eDestTaskId ];
	McpBool		bQueFull = MCP_FALSE;
	EMcpfRes   res = RES_OK;
	
	mcpf_critSec_Enter (hMcpf, pTask->hCritSecObj, MCPF_INFINIT);

	pTask->bQueueFlag[ uDestQId ] = MCP_TRUE; 
	if (que_Size (pTask->hQueue[ uDestQId ]))
	{
		bQueFull = MCP_TRUE;
	}

	mcpf_critSec_Exit (hMcpf, pTask->hCritSecObj);

	if (bQueFull)
	{
		res = os_sigobj_set (pMcpf->hPla, pTask->hSignalObj);
	}

	return res;
}

/** 
 * \fn     mcpf_MsgqDisable
 * \brief  Enable message queue
 * 
 */ 
EMcpfRes	mcpf_MsgqDisable (handle_t 	 	hMcpf, 
							  EmcpTaskId 	eDestTaskId,
							  McpU8		 	uDestQId)
{
	Tmcpf		*pMcpf = (Tmcpf	*) hMcpf;
	TMcpfTask   *pTask = pMcpf->tTask[ eDestTaskId ];
	
	mcpf_critSec_Enter (hMcpf, pTask->hCritSecObj, MCPF_INFINIT);

	pTask->bQueueFlag[ uDestQId ] = MCP_FALSE; 

	mcpf_critSec_Exit (hMcpf, pTask->hCritSecObj);

	return RES_OK;
}

/** 
 * \fn     mcpf_SetEvent
 * \brief  Set event
 * 
 */ 
EMcpfRes	mcpf_SetEvent (handle_t		hMcpf, 
						   EmcpTaskId	eDestTaskId,
						   McpU32		uEvent)
{
	Tmcpf		*pMcpf = (Tmcpf	*) hMcpf;
	TMcpfTask   *pTask = pMcpf->tTask[ eDestTaskId ];
	McpBool		bEvtChange = MCP_FALSE;
	EMcpfRes  res = RES_OK;
	McpU32		uEventMask = 0x01 << uEvent;
	
	mcpf_critSec_Enter (hMcpf, pTask->hCritSecObj, MCPF_INFINIT);

	if (!(pTask->uEvntBitmap & uEventMask))
	{
		pTask->uEvntBitmap |= uEventMask;
		bEvtChange = MCP_TRUE;
	}

	mcpf_critSec_Exit (hMcpf, pTask->hCritSecObj);

	if (bEvtChange)
	{
		res = os_sigobj_set (pMcpf->hPla, pTask->hSignalObj);
	}

	return res;
}


/** 
 * \fn     mcpf_RegisterClientCb
 * \brief  Register external send message handler function
 *
 */ 
EMcpfRes	mcpf_RegisterClientCb (handle_t			hMcpf,
								   McpU32			*pDestTaskId,
								   tClientSendCb 		fCb,
								   handle_t		 	hCb)
{
	EMcpfRes res = RES_ERROR;
	Tmcpf		*pMcpf = (Tmcpf	*) hMcpf;
	McpU32		taskId;

	for(taskId=TASK_MAX_ID; taskId<TASK_MAX_ID+MAX_EXTERNAL_CLIENTS; taskId++)
	{
		if(pMcpf->tClientsTable[taskId].fCb == NULL)
		{
			pMcpf->tClientsTable[taskId].fCb = fCb;
			pMcpf->tClientsTable[taskId].hCb = hCb;
			*pDestTaskId = taskId; 
			res = RES_OK;
			break;
		}
	}
	return (res);
}

/** 
 * \fn     mcpf_UnRegisterClientCb
 * \brief  Unregister MCPF external send message handler
 * 
 */ 
EMcpfRes	mcpf_UnRegisterClientCb (handle_t	hMcpf,
									 McpU32		uDestTaskId)
{
	EMcpfRes res = RES_ERROR;
	Tmcpf		*pMcpf = (Tmcpf	*) hMcpf;

	if(pMcpf->tClientsTable[uDestTaskId].fCb)
	{
		pMcpf->tClientsTable[uDestTaskId].fCb = NULL;
		pMcpf->tClientsTable[uDestTaskId].hCb = NULL;
		res = RES_OK;
	}
	return res;
}

/** 
 * \fn     mcpf_EnqueueMsg 
 * \brief  Enqueue MCPF message
 * 
 */
EMcpfRes mcpf_EnqueueMsg (handle_t		hMcpf,
								  EmcpTaskId	eDestTaskId,
								  McpU8			uDestQId,
								  TmcpfMsg  	*pMsg)
{
	Tmcpf		*pMcpf = (Tmcpf	*) hMcpf;
	TMcpfTask   *pTask = pMcpf->tTask[ eDestTaskId ];
	EMcpfRes   res;

	if (uDestQId >= pMcpf->tTask[eDestTaskId]->uNumOfQueues)
	{
		/* Destination queue is not valid */
		mcpf_mem_free_from_pool (hMcpf, pMsg);
		return RES_ERROR;
	}
	
	mcpf_critSec_Enter (hMcpf, pTask->hCritSecObj, MCPF_INFINIT);

	res = que_Enqueue ( pTask->hQueue[ uDestQId ], (handle_t) pMsg);

	mcpf_critSec_Exit (hMcpf, pTask->hCritSecObj);

	if (res == RES_OK)
	{
#ifdef DEBUG
		 MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, 
								("ENQUEUED MSG FOR TASK #%d, TO QUEUE #%d", eDestTaskId, uDestQId));
#endif
		
        res = os_sigobj_set (pMcpf->hPla, pTask->hSignalObj);
	}
	else
	{
		return RES_ERROR;
	}
	return res;
}

/************************************************************************
 * Module Private Functions
 ************************************************************************/

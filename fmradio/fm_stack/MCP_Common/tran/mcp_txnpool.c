/******************************************************************************\
##                                                                            *
## Unpublished Proprietary and Confidential Information of Texas Instruments  *
## Israel Ltd. Do Not Disclose.                                               *
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



/** \file   mcp_txnpool.c 
 *  \brief  Transaction structure and buffer pool handling
 *
 *  \see    mcp_txnpool.h
 */



#include "mcpf_mem.h"
#include "mcpf_report.h"
#include "mcp_hal_os.h"
#include "TxnDefs.h"
#include "BusDrv.h"
#include "mcp_txnpool.h"
#include "mcpf_services.h"


/** 
 * \fn     mcpf_txnPool_Create
 * \brief  Create pool
 * 
 * TBD
 * 
 */ 
handle_t 	mcpf_txnPool_Create (handle_t  hMcpf)
{	
	return mcpf_memory_pool_create(hMcpf, sizeof(TTxnStruct), MCPF_TXN_POOL_SIZE);
}


/** 
 * \fn     mcpf_txnPool_Destroy
 * \brief  Destroy pool
 * 
 * TBD
 * 
 */ 
EMcpfRes 	mcpf_txnPool_Destroy (handle_t hMcpf)
{
	Tmcpf *pMcpf = (Tmcpf *)hMcpf;

	return mcpf_memory_pool_destroy(hMcpf, pMcpf->hTxnPool);
}


/** 
 * \fn     mcpf_txnPool_Alloc
 * \brief  Allocate transaction structure
 * 
 */ 
TTxnStruct * mcpf_txnPool_Alloc (handle_t hMcpf)
{
	TTxnStruct * pTxn;
	Tmcpf *pMcpf = (Tmcpf *)hMcpf;

	pTxn = (TTxnStruct *) mcpf_mem_alloc_from_pool(hMcpf, pMcpf->hTxnPool);

	if (pTxn)
	{
		mcpf_mem_zero(hMcpf, pTxn, sizeof(TTxnStruct));
/*		MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, ("mcpf_txnPool_Alloc: pTxn = %x", (McpU32) pTxn));*/
	   return pTxn;
	}
	else
	{
	   return NULL;
	}
}


/** 
 * \fn     mcpf_txnPool_AllocBuf
 * \brief  Allocate transaction structure and buffer
 * 
 */ 
TTxnStruct * mcpf_txnPool_AllocBuf (handle_t hMcpf, McpU16 size)
{
	TTxnStruct   *pTxn;
	McpU8	 *pBuf;
	Tmcpf *pMcpf = (Tmcpf *)hMcpf;
	McpU32 uSize;
	
	pTxn = mcpf_txnPool_Alloc(hMcpf);
	if (pTxn)
	{
		if(size <= MCPF_TXN_SHORT_BUFF_SIZE)
		{
			pBuf = mcpf_mem_alloc_from_pool (hMcpf, pMcpf->hTxnBufPool_short);
			uSize = MCPF_TXN_SHORT_BUFF_SIZE;
		}
		else if(size <= MCPF_TXN_LONG_BUFF_SIZE)
		{
			pBuf = mcpf_mem_alloc_from_pool (hMcpf, pMcpf->hTxnBufPool_long);
			uSize = MCPF_TXN_LONG_BUFF_SIZE;
		}
		else
		{
			mcpf_txnPool_Free (hMcpf, pTxn);
			return NULL;
		}

	   if (pBuf)
	   {
	   	   mcpf_mem_zero(hMcpf, pBuf, uSize);
/*		   MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, 
			   ("mcpf_txnPool_AllocBuf: pTxn->aBuf[0] = %x", (McpU32) pBuf)); */
		   pTxn->aBuf[0] = pBuf;
		   pTxn->pData   = pBuf;
 /*          MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, ("mcpf_txnPool_allocBuf: pTxn = 0x%x buf 0x%x",  pTxn, pBuf));*/
		   return pTxn;
	   }
	   else
	   {
		   mcpf_txnPool_Free (hMcpf, pTxn);
		   return NULL;
	   }
	}
	else
	{
	   return NULL;
	}
}


/** 
 * \fn     mcpf_txnPool_AllocNBuf
 * \brief  Allocate transaction structure and buffer
 * 
 */ 
TTxnStruct * mcpf_txnPool_AllocNBuf (handle_t hMcpf, McpU32 uBufN, McpU16 aSizeN[] )
{
	TTxnStruct   *pTxn;
	McpU8	 *pBuf;
	McpU32	  i;
	Tmcpf *pMcpf = (Tmcpf *)hMcpf;
	McpU32 uSize;
	
	pTxn = mcpf_txnPool_Alloc (hMcpf);
	if (pTxn)
	{
		for (i=0; i < uBufN; i++)
		{
		   if(aSizeN[i] <= MCPF_TXN_SHORT_BUFF_SIZE)
		   {
			pBuf = mcpf_mem_alloc_from_pool (hMcpf, pMcpf->hTxnBufPool_short);
			uSize = MCPF_TXN_SHORT_BUFF_SIZE;
		   }
		   else if(aSizeN[i] <= MCPF_TXN_LONG_BUFF_SIZE)
		   {
			pBuf = mcpf_mem_alloc_from_pool (hMcpf, pMcpf->hTxnBufPool_long);
			uSize = MCPF_TXN_LONG_BUFF_SIZE;
		   }
		   else
		   {
			mcpf_txnPool_Free (hMcpf, pTxn);
			return NULL;
		   }

		   if (pBuf)
		   {
		   	   mcpf_mem_zero(hMcpf, pBuf, uSize);
			   MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, 
				   ("mcpf_txnPool_AllocNBuf: pTxn->aBuf[%d] = %x", i, (McpU32) pBuf));
			   pTxn->aBuf[i] = pBuf;
			   pTxn->aLen[i] = aSizeN[i];
		   }
		   else
		   {
			   mcpf_txnPool_FreeBuf (hMcpf, pTxn);
			   return NULL;
		   }
		}

		pTxn->pData = pTxn->aBuf[0];
		return pTxn;
	}
	else
	{
	   return NULL;
	}
}


/** 
 * \fn     mcpf_txnPool_FreeBuf
 * \brief  Free transaction structure and buffer[s]
 * 
 */ 
void mcpf_txnPool_FreeBuf (handle_t hMcpf, TTxnStruct * pTxn)
{
	McpU32	  i;
	
	if (pTxn)
	{
	   for (i=0; i < MAX_XFER_BUFS; i++)
	   {
		   if (pTxn->aBuf[i])
		   {
/*			   MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, 
				   ("mcpf_txnPool_FreeBuf: pTxn->aBuf[%d] = 0x%p", i, pTxn->aBuf[i])); */
			   mcpf_mem_free_from_pool(hMcpf, pTxn->aBuf[i]);
		   }
	   }
/*	   MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, ("mcpf_txnPool_FreeBuf: pTxn = 0x%p",  pTxn)); */
	   mcpf_mem_free_from_pool(hMcpf, pTxn);
	}
}


/** 
 * \fn     mcpf_txnPool_Free
 * \brief  Free transaction structure
 * 
 */ 
void mcpf_txnPool_Free (handle_t hMcpf, TTxnStruct * pTxn)
{
	if (pTxn)
	{
	/*	MCPF_REPORT_INFORMATION(hMcpf, MCPF_MODULE_LOG, ("mcpf_txnPool_Free: pTxn = 0x%p", pTxn)); */
		mcpf_mem_free_from_pool(hMcpf, pTxn);
	}
}


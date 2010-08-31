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



/** \file   msp_txnpool.h 
 *  \brief  Transaction structure and buffer pool handling
 *
 *  \see    msp_txnpool.c
 */


#ifndef _MCP_POOL_API_H_
#define _MCP_POOL_API_H_

#define POOL_MAX_BUF_SIZE 		1028

#define POOL_RX_MAX_BUF_SIZE 	POOL_MAX_BUF_SIZE


/** 
 * \fn     mcpf_txnPool_Create
 * \brief  Create pool
 * 
 * Allocate memory for pool
 * 
 * \note
 * \param	hMcpf   - pointer to OS Framework
 * \return 	handle_t hPool
 * \sa     	mcpf_txnPool_Create
 */ 
handle_t 	mcpf_txnPool_Create (handle_t  hMcpf);


/** 
 * \fn     mcpf_txnPool_Destroy
 * \brief  Destroy pool
 * 
 * Allocate memory for pool according to specified configuration
 * 
 * \note
 * \param	hMcpf   - pointer to OS Framework
 * \return 	status of operation OK or Error
 * \sa     	mcpf_txnPool_Destroy
 */ 
EMcpfRes 	mcpf_txnPool_Destroy (handle_t hMcpf);


/** 
 * \fn     mcpf_txnPool_Alloc
 * \brief  Allocate transaction structure
 * 
 * Allocate transaction structure
 * 
 * \note
 * \param	hMcpf   - pointer to OS Framework
 * \return 	pointer to allocated TTxnStruct or NULL in the case of error
 * \sa     	mcpf_txnPool_Alloc 
 */ 
TTxnStruct * mcpf_txnPool_Alloc (handle_t hMcpf);

/** 
 * \fn     mcpf_txnPool_AllocBuf
 * \brief  Allocate transaction structure and buffer
 * 
 * Allocate transaction structure and buffer of specified size
 * 
 * \note
 * \param	hMcpf   - pointer to OS Framework
 * \param	size   - buffer size
 * \return 	pointer to allocated TTxnStruct or NULL in the case of error
 * \sa     	mcpf_txnPool_AllocBuf 
 */ 
TTxnStruct * mcpf_txnPool_AllocBuf (handle_t hMcpf, McpU16 size);


/** 
 * \fn     mcpf_txnPool_AllocNBuf
 * \brief  Allocate transaction structure and buffer
 * 
 * Allocate transaction structure and buffers of specified size
 * 
 * \note
 * \param	hMcpf   - pointer to OS Framework
 * \param	uBufN  - number of required buffers, is to be less or equal to MAX_XFER_BUFS
 * \param	aSizeN - array of buffer sizes, number of items is to be equal to uBufN
 * \return 	pointer to allocated TTxnStruct or NULL in the case of error
 * \sa     	mcpf_txnPool_AllocNBuf 
 */ 
TTxnStruct * mcpf_txnPool_AllocNBuf (handle_t hMcpf, McpU32 uBufN, McpU16 aSizeN[]);

/** 
 * \fn     mcpf_txnPool_FreeBuf
 * \brief  Free transaction structure and buffer[s]
 * 
 * Free transaction structure and buffer or buffers associated with txn structure
 * 
 * \note
 * \param	hMcpf   - pointer to OS Framework
 * \param	pTxn  - pointer to TTxnStruct 
 * \return 	void
 * \sa     	mcpf_txnPool_FreeBuf 
 */ 
void mcpf_txnPool_FreeBuf (handle_t hMcpf, TTxnStruct * pTxn);

/** 
 * \fn     mcpf_txnPool_Free
 * \brief  Free transaction structure
 * 
 * Free transaction structure only without buffer or buffers associated with txn structure
 * 
 * \note
 * \param	hMcpf   - pointer to OS Framework
 * \param	pTxn  - pointer to TTxnStruct 
 * \return 	void
 * \sa     	mcpf_txnPool_Free 
 */ 
void mcpf_txnPool_Free (handle_t hMcpf, TTxnStruct * pTxn);


#endif  /* _MCP_POOL_API_H_ */

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


#ifndef __MCPF_MEM_H
#define __MCPF_MEM_H

#ifdef __cplusplus
extern "C" {
#endif 

#include "mcpf_defs.h"

/** STRUCTURES **/
typedef struct
{	
	void		*pFirstFree;
	void		*pPoolHead;
	McpU16	uElementSize;	
	McpU16	uElementNum;
} TMcpfPool;


/** Memory Alloc **/

/** 
 * \fn     mcpf_mem_alloc
 * \brief  Memory Allocation
 * 
 * This function will allocate memory from the OS, in the requested length.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	uLength - The length of the buffer to allocate.
 * \return 	pointer to the allocated buffer.
 * \sa     	mcpf_mem_alloc
 */ 
void		*mcpf_mem_alloc (handle_t hMcpf, McpU32 uLength); 

/** 
 * \fn     mcpf_mem_free
 * \brief  Memory Free
 * 
 * This function will free the given memory to the OS.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	*pBuf - The buffer to free.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_mem_free
 */ 

EMcpfRes mcpf_mem_free(handle_t hMcpf, void *pBuf);


/** 
 * \fn     mcpf_mem_set
 * \brief  Memory Set
 * 
 * This function will set the memory to the given value.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	*pMemPtr - pointer to the memory.
 * \param	uValue - The value to set.
 * \param	uLength - The length to set.
 * \return 	None
 * \sa     	mcpf_mem_set
 */ 
void		mcpf_mem_set (handle_t hMcpf, void *pMemPtr, McpS32 uValue, McpU32 uLength);

/** 
 * \fn     mcpf_mem_zero
 * \brief  Memory Zero
 * 
 * This function will set the memory to zero.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	*pMemPtr - pointer to the memory.
 * \param	uLength - The length to set.
 * \return 	None
 * \sa     	mcpf_mem_zero
 */ 
void		mcpf_mem_zero (handle_t hMcpf,void *pMemPtr, McpU32 uLength );

/** 
 * \fn     mcpf_mem_copy
 * \brief  Memory Copy
 * 
 * This function will copy from dest to src.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	*pDestination - pointer to the destination buffer.
 * \param	*pSource - pointer to the source buffer.
 * \param	usize - The length to copy.
 * \return 	None
 * \sa     	mcpf_mem_copy
 */ 
void		mcpf_mem_copy (handle_t hMcpf, void *pDestination, void *pSource, McpU32 uSize);


/** Memory Pools Management **/

/** 
 * \fn     mcpf_memory_pool_create
 * \brief  Create a memory Pool
 * 
 * This function will create a pool according to the given sizes.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	uElementSize - size of the elements in the pool.
 * \param	eElementNum - number of elements in the pool.
 * \return 	Handler to the Pool that was created
 * \sa     	mcpf_memory_pool_create
 */ 
handle_t 		mcpf_memory_pool_create(handle_t hMcpf, McpU16 uElementSize, McpU16 eElementNum);

/** 
 * \fn     mcpf_memory_pool_destroy
 * \brief  Destroy a memory Pool
 * 
 * This function will destroy(free) a given pool.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	hMemPool - Pool's handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_memory_pool_destroy
 */ 
EMcpfRes  	mcpf_memory_pool_destroy(handle_t hMcpf,  handle_t hMemPool);

/** 
 * \fn     mcpf_mem_alloc_from_pool
 * \brief  Allocation from a memory Pool
 * 
 * This function will return a free buffer from a given pool.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	hMemPool - Pool's handler.
 * \return 	pointer to the allocated buffer.
 * \sa     	mcpf_mem_alloc_from_pool
 */ 
McpU8		*mcpf_mem_alloc_from_pool(handle_t hMcpf, handle_t hMemPool);

/** 
 * \fn     mcpf_mem_free_from_pool
 * \brief  free to a memory Pool
 * 
 * This function will free the given buffer back to its pool.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	*pBuf - The buffer to free.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_mem_free_from_pool
 */ 
EMcpfRes  	mcpf_mem_free_from_pool (handle_t  hMcpf, void *pBuf);

#ifdef __cplusplus
}
#endif 

#endif

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


/** Include Files **/
#include "mcpf_mem.h"
#include "pla_os.h"
#include "mcpf_report.h"
#include "mcpf_services.h"

/** Internal Structures **/
typedef struct
{	
	McpU16		uPreamble; /* 0xAA - when Free, 0xBB - When in use */
	McpU16		uRefCount; /* 0xA0 - When Free, 0xB1 - When in use */
void			*pData;
} TMcpfBufDesc;

McpU32 uMcpfDynamicMemCnt = 0;

/** APIs **/

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
void		*mcpf_mem_alloc (handle_t hMcpf, McpU32 uLength)
{
	Tmcpf *pMcpf;

	uMcpfDynamicMemCnt += uLength;
	
	if(hMcpf)
	{
		pMcpf = (Tmcpf *)hMcpf;	
		return os_malloc(pMcpf->hPla, uLength);
	}
	else
		return os_malloc(hMcpf, uLength);
}

/** 
 * \fn     mcpf_mem_free
 * \brief  Memory Free
 * 
 * This function will free the given memory to the OS.
 * 
 * \note
 * \param	hMcpf     - MCPF handler.
 * \param	*pBuf - The buffer to free.
 * \return 	EMcpfRes
 * \sa     	mcpf_mem_free
 */ 
EMcpfRes mcpf_mem_free(handle_t hMcpf, void *pBuf)
{
	Tmcpf *pMcpf;
	
	if(hMcpf)
	{
		pMcpf = (Tmcpf *)hMcpf;
		os_free(pMcpf->hPla, (McpU8 *)pBuf);
	}
	else
		os_free(hMcpf, pBuf);

	return RES_OK;
}

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
void		mcpf_mem_set (handle_t hMcpf, void *pMemPtr, McpS32 uValue, McpU32 uLength)
{
	Tmcpf *pMcpf;
	
	if(hMcpf)
	{
		pMcpf = (Tmcpf *)hMcpf;
		os_memory_set(pMcpf->hPla, pMemPtr, uValue, uLength);
	}
	else
		os_memory_set(hMcpf, pMemPtr, uValue, uLength);
}

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
void		mcpf_mem_zero (handle_t hMcpf,void *pMemPtr, McpU32 uLength )
{
	Tmcpf *pMcpf;

	if(hMcpf)
	{
		pMcpf = (Tmcpf *)hMcpf;
		os_memory_zero(pMcpf->hPla, pMemPtr, uLength);
	}
	else
		os_memory_zero(hMcpf, pMemPtr, uLength);
}

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
void		mcpf_mem_copy (handle_t hMcpf, void *pDestination, void *pSource, McpU32 uSize)
{
	Tmcpf *pMcpf;
	
	if(hMcpf)
	{
		pMcpf = (Tmcpf *)hMcpf;
		os_memory_copy(pMcpf->hPla, pDestination, pSource, uSize);
	}
	else
		os_memory_copy(hMcpf, pDestination, pSource, uSize);
}


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
handle_t 		mcpf_memory_pool_create(handle_t hMcpf, McpU16 uElementSize, McpU16 eElementNum)
{

	TMcpfPool		*pMcpfPool;
	TMcpfBufDesc	*pPoolBufDesc;
	McpU32			uAllocatedSize;     
	McpU8			count;
	void *			allocMemBlock;
	McpU32			uStepSize;

	if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return NULL;	
	}

    /* Calculate & alloc mem block for pool */
    uAllocatedSize = (McpU32)((uElementSize + sizeof(TMcpfBufDesc)) * (eElementNum));
    allocMemBlock = mcpf_mem_alloc(hMcpf, uAllocatedSize);

    if ( NULL == allocMemBlock)
	{
		MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, ("mcpf_memory_pool_create: memory allocation FAILED\n"));
		return NULL;
	}

    /* Allocate & fill pool data structure */
    pMcpfPool = (TMcpfPool*) mcpf_mem_alloc(hMcpf, sizeof(TMcpfPool));

	if(NULL == pMcpfPool)
	{
		mcpf_mem_free(hMcpf, allocMemBlock);
		MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, ("mcpf_memory_pool_create: memory allocation FAILED\n"));
		return NULL;
	}
   
    pMcpfPool->pFirstFree	= allocMemBlock;
    pMcpfPool->pPoolHead	= allocMemBlock;
    pMcpfPool->uElementSize = uElementSize;
    pMcpfPool->uElementNum	= eElementNum;
   
    pPoolBufDesc = (TMcpfBufDesc*)allocMemBlock;   
	
    /* Fill all BufDescriptor, except the last one */
	for(count = 0; count < (eElementNum - 1); count++)
	{
		pPoolBufDesc->uPreamble	= 0xAA;
		pPoolBufDesc->uRefCount	= 0xA0;
		uStepSize = (sizeof(TMcpfBufDesc) + uElementSize) * (count + 1);
		pPoolBufDesc->pData		= (McpU8*)(allocMemBlock) + uStepSize;
	    pPoolBufDesc = pPoolBufDesc->pData;				  
	}
   
	/* Fill the last BufDescriptor pointing to NULL */
	pPoolBufDesc->uPreamble	= 0xAA;
	pPoolBufDesc->uRefCount	= 0xA0;
    pPoolBufDesc->pData = NULL;
       
	return (handle_t) pMcpfPool;
}

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
EMcpfRes  	mcpf_memory_pool_destroy(handle_t hMcpf,  handle_t hMemPool)
{
	EMcpfRes eResult = RES_COMPLETE;
	TMcpfPool	*pMemPool;

	if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return RES_ERROR;	
	}

	if (!hMemPool) 
	{
		MCPF_OS_REPORT(("MemPool handler is NULL\n"));
		return RES_ERROR;	
	}
	
	pMemPool = (TMcpfPool*)hMemPool;

	eResult = mcpf_mem_free(hMcpf, pMemPool->pPoolHead);

	if (RES_COMPLETE == eResult)
	{
		if( RES_ERROR ==  mcpf_mem_free(hMcpf, hMemPool))
		{
			MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, ("mcpf_memory_pool_destroy: Mcpf_mem_free FAILED\n"));
			return RES_ERROR;	
		}
		else
			return RES_COMPLETE;
	}
	else
	{
		return RES_ERROR;
	}	

}

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
McpU8		*mcpf_mem_alloc_from_pool(handle_t hMcpf, handle_t hMemPool)
{
	TMcpfPool 		*pMcpfPool;
	TMcpfBufDesc	*pPoolBufDesc;	

	if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return NULL;	
	}

	pMcpfPool = (TMcpfPool*)hMemPool;

	MCPF_ENTER_CRIT_SEC(hMcpf);
	/* Return if pool is FULL */
	if (NULL == pMcpfPool->pFirstFree)
	{
		MCPF_EXIT_CRIT_SEC(hMcpf);
		MCPF_REPORT_ERROR(hMcpf, MCPF_MODULE_LOG, ("mcpf_mem_alloc_from_pool: Cannot Allocate, Memory Pool is FULL\n"));
	    return NULL;
	}
	
	/* Get free BufDescp */
	pPoolBufDesc = (TMcpfBufDesc*) pMcpfPool->pFirstFree;
		
	if ((pPoolBufDesc->uPreamble != 0xAA) &&
					  (pPoolBufDesc->uRefCount != 0xA0))
	{
		MCPF_REPORT_ERROR(hMcpf,MCPF_MODULE_LOG,("ERROR: mem_alloc_from_pool: Invalid buffer Ptr:%x", pMcpfPool->pFirstFree));
	}
		
	pPoolBufDesc->uPreamble = 0xBB;
	pPoolBufDesc->uRefCount = 0xB1;
	
	/* Update first free ptr of pool data structure */
    pMcpfPool->pFirstFree = pPoolBufDesc->pData;

	/* Update the pData of the buffer descriptor to address of mempool */
	pPoolBufDesc->pData =  hMemPool;
	MCPF_EXIT_CRIT_SEC(hMcpf);

	return ((McpU8*)pPoolBufDesc + sizeof(TMcpfBufDesc));  

}

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
EMcpfRes  	mcpf_mem_free_from_pool (handle_t  hMcpf, void *pBuf)
{
	TMcpfPool		*pMcpfPool;
	TMcpfBufDesc	*pPoolBufDesc;

	if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return RES_ERROR;	
	}

	MCPF_ENTER_CRIT_SEC(hMcpf);

    pPoolBufDesc	= (TMcpfBufDesc*)((McpU8*)pBuf -  sizeof(TMcpfBufDesc));
	pMcpfPool		= (TMcpfPool*) pPoolBufDesc->pData;
	
	if ((pPoolBufDesc->uPreamble != 0xBB) &&
				  (pPoolBufDesc->uRefCount != 0xB1))
	{
		MCPF_REPORT_ERROR(hMcpf,MCPF_MODULE_LOG,("mem_free_from_pool: INVALID BUF Ptr:%x", pPoolBufDesc));
	}

	
	/* Update the freed Buf Descriptor */
	pPoolBufDesc->uPreamble = 0xAA;
	pPoolBufDesc->uRefCount = 0xA0;

	pPoolBufDesc->pData = pMcpfPool->pFirstFree;
	pMcpfPool->pFirstFree = pPoolBufDesc;
	MCPF_EXIT_CRIT_SEC(hMcpf);
	
	return RES_COMPLETE;
}


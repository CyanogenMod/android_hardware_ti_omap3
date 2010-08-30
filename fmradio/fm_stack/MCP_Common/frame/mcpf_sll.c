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
#include "mcpf_sll.h"
#include "mcpf_report.h"
#include "mcpf_mem.h"


/** APIs **/

/** 
 * \fn     mcpf_SLL_Create
 * \brief  Create a sorted linked list
 * 
 * This function creates a sorted linked list
 * 
 * \note
 * \param	hMcpf - MCPF handler.
 * \param	uLimit - max list length.
 * \param	uNodeHeaderOffset - Offset of node's header field in stored structure.
 * \param	uSortByFeildOffset - Offset of the field to sort by, in stored structure.
 * \param	tSortType - sort type, UP or DOWN.
 * \return 	List handler.
 * \sa     	mcpf_SLL_Create
 */ 
handle_t mcpf_SLL_Create (handle_t hMcpf, McpU32 uLimit, McpU32 uNodeHeaderOffset, 
							McpU32 uSortByFeildOffset, TSllSortType	tSortType)
{
    mcpf_SLL   *pSortedList;
    if (!hMcpf) 
	{
		MCPF_OS_REPORT(("Mcpf handler is NULL\n"));
		return NULL;	
	}
        
    /* Allocate memory for the sorted Linked List */
    pSortedList = mcpf_mem_alloc(hMcpf, sizeof(mcpf_SLL));

    if (NULL == pSortedList)
	{
		MCPF_REPORT_ERROR(pSortedList->hMcpf, MCPF_MODULE_LOG, ("mcpf_mem_alloc returned NULL\n"));
		return NULL;
	}
    
    /* Initialize the memory block to zero */
    mcpf_mem_zero(hMcpf, pSortedList, sizeof(mcpf_SLL));

    /* Initialize the list header */
	MCP_DL_LIST_InitializeHead (&pSortedList->tHead);

    /* Set the List paramaters */
	pSortedList->hMcpf = hMcpf;                  
	pSortedList->tSortType = tSortType;
	pSortedList->uSortByFieldOffset = (McpU16)uSortByFeildOffset;
	pSortedList->uNodeHeaderOffset  = (McpU16)uNodeHeaderOffset;
	pSortedList->uOverflow = 0;
	pSortedList->uLimit = (McpU16)uLimit;
	pSortedList->uCount = 0;
        
    return (handle_t)pSortedList;
}

/** 
 * \fn     mcpf_SLL_Destroy
 * \brief  Destroy the sorted linked list
 * 
 * This function frees the resources of the SLL.
 * 
 * \note
 * \param	hList - List handler.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_SLL_Destroy
 */ 
EMcpfRes mcpf_SLL_Destroy (handle_t hList)
{
    mcpf_SLL   *pSortedList;    
    handle_t     *hMcpf;

    /* Get the list handler from hList */
    pSortedList = (mcpf_SLL*) hList;    
    
	hMcpf = pSortedList->hMcpf;
    
    if (pSortedList-> uCount >=1) /* List is not Empty */
	{
		MCPF_REPORT_ERROR(pSortedList->hMcpf, MCPF_MODULE_LOG, ("mcpf_SLL_Destroy: Cannot Destroy List\n"));
		return RES_ERROR;
	}
	else
	{
		mcpf_mem_free(hMcpf, pSortedList);
	}

    return RES_OK;  
}

/** 
 * \fn     mcpf_SLL_Insert
 * \brief  Insert item to list
 * 
 * This function inserts the item to the list in its correct place.
 * 
 * \note
 * \param	hList - List handler.
 * \param	hItem - item to put in the List.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_SLL_Insert
 */ 
EMcpfRes mcpf_SLL_Insert (handle_t hList, handle_t hItem)
{
    mcpf_SLL     		*pSortedList = (mcpf_SLL*) hList;
    MCP_DL_LIST_Node  	*pItemNodeHeader, *pListNodeHeader;
    McpU32       		uItemSortBy_field, uListSortBy_field;
    MCP_DL_LIST_Node 	*pNode;

    pSortedList = (mcpf_SLL*) hList;

	/* If list is not full */
    if (pSortedList->uCount < pSortedList->uLimit)
	{
		/* Get the Node Header in the given Item */
		pItemNodeHeader = (MCP_DL_LIST_Node*) ((McpU8 *)hItem + pSortedList->uNodeHeaderOffset);

		/* Get content of field, to sort by, of the new Item */
		uItemSortBy_field = * ( (McpU32*)((McpU8*)hItem + pSortedList->uSortByFieldOffset));

		/* Get the address of the list header */
		pListNodeHeader = &pSortedList->tHead;      

		MCP_DL_LIST_ITERATE(pListNodeHeader, pNode)
		{
			uListSortBy_field = *((McpU32*)((McpU8*)pNode - 
											pSortedList->uNodeHeaderOffset + 
											pSortedList->uSortByFieldOffset));

			if ( ((SLL_UP   == pSortedList->tSortType) && (uItemSortBy_field < uListSortBy_field)) ||
				 ((SLL_DOWN == pSortedList->tSortType) && (uItemSortBy_field > uListSortBy_field)) )
			{
				break;
			}
		}

		MCP_DL_LIST_InsertNode (pItemNodeHeader, pNode->prev, pNode);

		pSortedList->uCount++;

	#ifdef _DEBUG
		if (pSortedList->uCount > pSortedList->uMaxCount)
		{
			pSortedList->uMaxCount = pSortedList->uCount;
		}		
	#endif
	}
	else
	{
		MCPF_REPORT_ERROR(pSortedList->hMcpf, MCPF_MODULE_LOG, ("mcpf_SLL_Insert: List is FULL \n"));
		return RES_ERROR;
	}

    return RES_OK;
}

/** 
 * \fn     mcpf_SLL_Remove
 * \brief  Remove item from list
 * 
 * This function delete requested item from list.
 * 
 * \note
 * \param	hList - List handler.
 * \param	hItem - item to remove from the List.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_SLL_Remove
 */ 
EMcpfRes mcpf_SLL_Remove (handle_t hList, handle_t hItem)
{
    mcpf_SLL     		*pSortedList = (mcpf_SLL *)hList;
    MCP_DL_LIST_Node  	*pItemHdr, *pListHdr, *pNode;
    
    pListHdr = &(pSortedList->tHead);

	pItemHdr = (MCP_DL_LIST_Node*) ((McpU8 *)hItem + pSortedList->uNodeHeaderOffset);

    /* Check if the list is not empty */
    if(pSortedList->uCount)
    {
		/* loop list item until item is found & list is traversed till end */
		MCP_DL_LIST_ITERATE(pListHdr, pNode)
		{
			if (pNode == pItemHdr)
			{
				break;
			}
		}
		if (pNode == pItemHdr )  /* Verify if the item was found */
		{
			MCP_DL_LIST_RemoveNode (pNode);

			/* Reduce value of count in the list header structure */
			pSortedList->uCount = (McpU16)((pSortedList->uCount) - 1);
			return RES_OK;
		}
		else /* Item was not found */
		{
			MCPF_REPORT_ERROR(pSortedList->hMcpf, MCPF_MODULE_LOG, ("mcpf_SLL_Remove: ITEM WAS NOT FOUND\n"));
			return RES_ERROR;
		}
	}
	else
	{
		MCPF_REPORT_ERROR(pSortedList->hMcpf, MCPF_MODULE_LOG, ("mcpf_SLL_Remove: List is EMPTY \n"));
		return RES_ERROR;
	}
}

/** 
 * \fn     mcpf_SLL_Retrieve
 * \brief  Retrieve item from list
 * 
 * This function retrieves item from head of the list and return it.
 * 
 * \note
 * \param	hList - List handler.
 * \return 	Handler of the item from the head of the list.
 * \sa     	mcpf_SLL_Retrieve
 */ 
handle_t mcpf_SLL_Retrieve (handle_t hList)
{
   mcpf_SLL     	*pSortedList = (mcpf_SLL*) hList;
   MCP_DL_LIST_Node *pNode, *pListHdr;
   McpU8			*pItem;

   pListHdr = &(pSortedList->tHead);

   if(pSortedList->uCount)
   {
	   pNode = MCP_DL_LIST_RemoveHead (pListHdr);
	   pItem = (McpU8 *)pNode - pSortedList->uNodeHeaderOffset;

	   /* Reduce value of count in the list header structure */
       pSortedList->uCount = (McpU16)((pSortedList->uCount) - 1);
	   return ((handle_t)pItem);
   }
   else /* List is empty */
   {
	   MCPF_REPORT_ERROR(pSortedList->hMcpf, MCPF_MODULE_LOG, ("mcpf_SLL_Retrieve: List is EMPTY\n"));
       return NULL;
   }
}

/** 
 * \fn     mcpf_SLL_Get
 * \brief  Get item from list
 * 
 * This function return's the first item from head of the list.
 * 
 * \note
 * \param	hList - List handler.
  * \return 	Handler of the item from the head of the list.
 * \sa     	mcpf_SLL_Get
 */ 
handle_t mcpf_SLL_Get (handle_t hList)
{
   mcpf_SLL     	*pSortedList = (mcpf_SLL*) hList;     
   MCP_DL_LIST_Node *pNode, *pListHdr;
   McpU8			*pItem;
   
   pListHdr = &(pSortedList->tHead);
            
   if(((mcpf_SLL*)hList)->uCount)
   {
	   pNode = MCP_DL_LIST_GetHead (pListHdr);
	   pItem = (McpU8 *)pNode - pSortedList->uNodeHeaderOffset;
	   return ((handle_t)pItem);
   }
   else
   {
	   MCPF_REPORT_ERROR(pSortedList->hMcpf, MCPF_MODULE_LOG, ("mcpf_SLL_Get: List is EMPTY\n"));
	   return NULL;
   }   
}

/** 
 * \fn     mcpf_SLL_Size
 * \brief  Return number of items in the list
 * 
 * This function return's the the number of item in the list.
 * 
 * \note
 * \param	hList - List handler.
  * \return 	Number of item in the list.
 * \sa     	mcpf_SLL_Size
 */ 
McpU32 mcpf_SLL_Size (handle_t hList)
{
   return(((mcpf_SLL*)hList)->uCount);	
}

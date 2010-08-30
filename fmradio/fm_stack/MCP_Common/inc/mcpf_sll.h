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


#ifndef __MCPF_SLL_H
#define __MCPF_SLL_H


#include "mcpf_defs.h"
#include "mcp_utils_dl_list.h"


/** Definitions **/
typedef enum
{
	SLL_UP,
	SLL_DOWN
} TSllSortType;


/** Structures **/

/* A List node header structure */                        
typedef MCP_DL_LIST_Node TSllNodeHdr; 

typedef struct 
{
    MCP_DL_LIST_Node 	tHead;          /* The List first node */
    McpU16  	 		uCount;         /* Current number of nodes in list */
    McpU16   			uLimit;         /* Upper limit of nodes in list */
    McpU16  		 	uMaxCount;      /* Maximum uCount value (for debug) */
    McpU16  		 	uOverflow;      /* Number of overflow occurrences - couldn't insert node (for debug) */
    McpU16  		 	uNodeHeaderOffset; /* Offset of NodeHeader field from the entry of the listed item */
    McpU16  		 	uSortByFieldOffset; /* Offset of the field you sort by from the entry of the listed item */
    TSllSortType		tSortType;
    handle_t   			hMcpf;
} mcpf_SLL;


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
handle_t mcpf_SLL_Create (handle_t 		hMcpf, 
						  McpU32 		uLimit, 
						  McpU32 		uNodeHeaderOffset, 
						  McpU32 		uSortByFeildOffset, 
						  TSllSortType	tSortType);

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
EMcpfRes mcpf_SLL_Destroy (handle_t hList);

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
EMcpfRes mcpf_SLL_Insert (handle_t hList, handle_t hItem);

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
EMcpfRes mcpf_SLL_Remove (handle_t hList, handle_t hItem);

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
handle_t mcpf_SLL_Retrieve (handle_t hList);

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
handle_t mcpf_SLL_Get (handle_t hList);

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
McpU32 mcpf_SLL_Size (handle_t hList);


#endif

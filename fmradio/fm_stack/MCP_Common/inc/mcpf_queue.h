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


/** \file   queue.h 
 *  \brief  queue module header file.                                  
 *
 *  \see    queue.c
 */


#ifndef _MCPF_QUEUE_H_
#define _MCPF_QUEUE_H_

#include "mcp_hal_types.h"
#include "mcpf_defs.h"
#include "mcp_utils_dl_list.h"


/*			Definitions			 */
/* ============================= */
#define QUE_MAX_LIMIT	0xFFFFFFFF

/* External Functions Prototypes */
/* ============================= */
handle_t que_Create  (handle_t hMcpf, McpU32 uLimit, McpU32 uNodeHeaderOffset);
EMcpfRes que_Destroy (handle_t hQue);
EMcpfRes que_Enqueue (handle_t hQue, handle_t hItem);
handle_t que_Dequeue (handle_t hQue);
EMcpfRes que_Requeue (handle_t hQue, handle_t hItem);
McpU32 que_Size    (handle_t hQue);

#ifdef _DEBUG
void      que_Print   (handle_t hQue);
#endif /* _DEBUG */



#endif  /* _MCPF_QUEUE_H_ */

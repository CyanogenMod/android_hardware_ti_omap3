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


#ifndef __MCPF_DEFS_H
#define __MCPF_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif 

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>

#include "mcp_defs.h"
#include "mcp_hal_types.h"


#define INLINE __inline

/* Definitions */
#define MCPF_INFINIT MCP_UINT_MAX

#define MCPF_FIELD_OFFSET(type,field)  	((McpU32)(&(((type*)0)->field)))

#define MCPF_UNUSED_PARAMETER(par)     	((par) = (par))

#define TRANS_MAX_NUMBER_OF_CHANNELS	40

#define TRANS_MAX_NUMBER_OF_PRIORITIES	2

#define MCPF_Assert(exp)  (((exp) != 0) ? (void)0 : MCP_HAL_MISC_Assert(#exp,__FILE__,(McpU16)__LINE__))

#define MCPF_MSG_POOL_SIZE				100 

#define MCPF_TIMER_POOL_SIZE				20 

#define MCPF_TXN_POOL_SIZE					55 /* BT Rx Desc - 46 are currently defined, need more Txn for Transmit */

#define MCPF_TXN_LONG_POOL_SIZE			50 /* BT Rx Desc - 46 are currently defined, need more for NAVC */

#define MCPF_TXN_SHORT_POOL_SIZE			50 /* BT Rx Desc - 46 are currently defined, need more for NAVC */

#define MCPF_TXN_LONG_BUFF_SIZE			1038

#define MCPF_TXN_SHORT_BUFF_SIZE			200

#define MAX_EXTERNAL_CLIENTS 				10

#define	MCPF_TASK_NAME_MAX_LEN			16

/* Discrete signals bit definition */
#define BIT_VDD_CORE 		0
#define BIT_GPS_EN_RESET 	1
#define BIT_PA_EN 			2
#define BIT_SLEEPX 			3
#define BIT_TIMESTAMP 		4
#define BIT_I2C_UART 		5
#define BIT_RESET_INT 		6

/* Types Definition */
typedef void * handle_t;	/* abstraction for module/component context */
 
/* Enums Definition */
typedef enum 
{
	RES_COMPLETE, 	/* synchronous operation (completed successfully) */
	RES_OK = RES_COMPLETE,
	RES_PENDING, 	/* a-synchronous operation (completion event will follow) */
	RES_ERROR,
	RES_MEM_ERROR,	/* failure in memory allocation */
	RES_FILE_ERROR,	/* failure in file access */
	RES_STATE_ERROR,	/* invalid state */		
	RES_UNKNOWN_OPCODE  /* unknown opcode */
} EMcpfRes;


/* Task event handler function prototype */
typedef void (*mcpf_event_cb)(handle_t hCaller, McpU32 uEvntBitmap);

/* Task procedure function prototype */
typedef EMcpfRes (*mcpf_TaskProcedure) (handle_t	hMcpf, EmcpTaskId	eTaskId);

/* List node structure */
typedef struct  _MCP_DL_LIST_Node 
{
    struct _MCP_DL_LIST_Node *next;
    struct _MCP_DL_LIST_Node *prev;
} MCP_DL_LIST_Node;


/* A queue node header structure */                        
typedef MCP_DL_LIST_Node TQueNodeHdr;

/* MCPF intertask message structure definition */
typedef struct
{
	TQueNodeHdr		tMsgQNode; 
	EmcpTaskId		eSrcTaskId;
	McpU8			uSrcQId;
	McpU16			uOpcode;
	McpU32			uLen;
	McpU32			uUserDefined;	
	void 			*pData;

} TmcpfMsg;

/* MCPF task queue handle callback */
typedef void (*mcpf_msg_handler_cb)(handle_t hCaller,  TmcpfMsg *tMsg);


/* MCPF Task's internal data structure */
typedef struct
{
	McpUint 			uNumOfQueues;
	handle_t		 	*hQueue; 		/* pointers to queue handlers  */
	mcpf_msg_handler_cb	*fQueueCb; 		/* pointers to queue callbacks */
    handle_t			*hQueCbHandler; /* pointers to queue callback handlers */
	McpBool				*bQueueFlag;    /* pointer to array of queue enabled/disabled flags */ 
	handle_t			hCritSecObj;
	handle_t		 	hSignalObj;
	McpU32				uEvntBitmap;
	mcpf_event_cb		fEvntCb;
    handle_t			hEvtCbHandler;
	McpBool				bDestroyTask;
	char 				cName[MCPF_TASK_NAME_MAX_LEN];
	handle_t			hOsTaskHandle;

} TMcpfTask;


/* External destination function handler type */
typedef EMcpfRes (*tClientSendCb)(handle_t, McpU32, McpU8, TmcpfMsg*);

/* External destination handler table entry type */
typedef struct 
{
	tClientSendCb fCb;
	handle_t		 hCb;
} tCleintParams;


/* MCPF Main Structure */
typedef struct
{
	McpUint			uNumOfRegisteredTasks;
	TMcpfTask 		*tTask[TASK_MAX_ID];
	handle_t		hPla;
	handle_t 		hReport;
	handle_t		hCcmObj;
	tCleintParams tClientsTable[TASK_MAX_ID + MAX_EXTERNAL_CLIENTS];	
	
	/* Timer related Params */
	McpU32			uTimerId;
	handle_t		hTimerList;
	handle_t		hTimerPool;
	
	handle_t 		hTrans;
	handle_t 		hHcia;
	handle_t 		hTxnPool;
	handle_t 		hTxnBufPool_long; 	/* Buf Size = 1024, Pool Size = 50 */
	handle_t 		hTxnBufPool_short; 	/* buf Size = 200,  Pool Size = 50 */
	handle_t 		hMsgPool;
	handle_t		hMcpfCritSecObj;

} Tmcpf; 		

#ifdef __cplusplus
}
#endif 

#endif

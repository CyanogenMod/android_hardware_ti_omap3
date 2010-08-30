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



/** \file   mcp_hal_socket.h 
 *  \brief  Hardware/OS Adaptation Layer for Socket Interface API-s
 * 
 * 
 * The HAL Socket is OS and Bus dependent module.  
 *
 *  \see    mcp_hal_socket.c
 */

#ifndef __MCP_HAL_SOCKET_H
#define __MCP_HAL_SOCKET_H

#include "mcpf_defs.h"

/************************************************************************/
/*							 Definitions                                */
/************************************************************************/
#define	SOCKET_CLIENT_NUM 5

typedef handle_t	(*tHalSockOnAcceptCb) (handle_t, McpU32, McpU8 *);
typedef McpU16		(*tHalSockOnRecvCb) (handle_t, McpS8 *, McpS16);
typedef void		(*tHalSockOnCloseCb) (handle_t);


/************************************************************************/
/*							Structure                                   */
/************************************************************************/


/************************************************************************/
/*						APIs Definitions                                */
/************************************************************************/
/** 
 * \fn     mcp_hal_socket_CreateServerSocket
 * \brief  Socket Initializing and Binding at the Server side
 * 
 */
handle_t mcp_hal_socket_CreateServerSocket(handle_t hMcpf, handle_t hHostSocket, 
										   McpS16 port, tHalSockOnAcceptCb onAcceptCb, 
										   tHalSockOnRecvCb onRecvCb, tHalSockOnCloseCb onCloseCb);

/** 
 * \fn     mcp_hal_socket_Accept
 * \brief  Socket Accept at the Server side
 * 
 */
void mcp_hal_socket_Accept(handle_t hHalSocket);


/** 
 * \fn     mcp_hal_socket_DestroyServerSocket
 * \brief  Socket Initializing and Binding at the Server side
 * 
 */
void mcp_hal_socket_DestroyServerSocket(handle_t hHalSocket);


/** 
 * \fn     mcp_hal_socket_Send 
 * \brief  Send data back to the client
 * 
 */
EMcpfRes mcp_hal_socket_Send(handle_t hHalSocket, McpU32 clientSockId, McpS8 *buf, McpU16 bufLen);


#endif /* __MCP_HAL_SOCKET_H */

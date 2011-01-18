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

/** \file   mcp_hal_socket.c 
 *  \brief  Linux OS Adaptation Layer for Socket Interface implementation
 * 
 *  \see    mcp_hal_socket.h
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include "mcpf_services.h"
#include "mcp_hal_os.h"
#include "mcp_hal_fs.h"
#include "mcp_hal_socket.h"
#include "mcp_hal_types.h"
#include "mcpf_mem.h"
#include "mcpf_report.h"

/************************************************************************/
/* Internal Structures			                                        */
/************************************************************************/
typedef struct {
	handle_t				hMcpf;
	handle_t				hHostSocket;
	handle_t				hClientsPool;
	tHalSockOnAcceptCb		onAcceptCb;
	tHalSockOnRecvCb		onRecvCb;
	tHalSockOnCloseCb		onCloseCb;
	McpU32					sockId;

} hal_socket_t;

typedef struct  
{
	McpU32		clientSockId;
	handle_t	hHalSock;
	handle_t	hCaller;
	McpU8		uNumOfBytesToRead;
} client_handle_t;


/************************************************************************/
/* Internal Functions Definitions                                       */
/************************************************************************/
void serverSocket_ThreadFunc();
void clientSocket_ThreadFunc();


/************************************************************************/
/* APIs Functions Implementation                                        */
/************************************************************************/
/** 
 * \fn     mcp_hal_socket_CreateServerSocket
 * \brief  Socket Initializing and Binding at the Server side
 * 
 */
handle_t mcp_hal_socket_CreateServerSocket(handle_t hMcpf, handle_t hHostSocket, 
										   McpS16 port, tHalSockOnAcceptCb onAcceptCb, 
										   tHalSockOnRecvCb onRecvCb, tHalSockOnCloseCb onCloseCb) 
{
	struct sockaddr_in  lSockAddr;
	//WSADATA wsaData;
	//unsigned long q_Handle;
	//HANDLE hThread;
	McpU32 hThread;
	hal_socket_t *pHalSocket;
	pthread_t     thread1;

	pHalSocket = mcpf_mem_alloc(hMcpf, sizeof(hal_socket_t));
	if(pHalSocket == NULL)
	{
		MCPF_REPORT_ERROR(hMcpf, HAL_SOCKET_MODULE_LOG,
			("mcp_hal_socket_CreateServerSocket: Create Hal Socket Failed!"));
		return NULL;
	}

	pHalSocket->hClientsPool = mcpf_memory_pool_create(hMcpf, sizeof(client_handle_t), SOCKET_CLIENT_NUM);
	if(pHalSocket->hClientsPool == NULL)
	{
		MCPF_REPORT_ERROR(hMcpf, HAL_SOCKET_MODULE_LOG,
			("mcp_hal_socket_CreateServerSocket: Create Pool Failed!"));
		mcpf_mem_free(hMcpf, pHalSocket);
		return NULL;
	}

	pHalSocket->hMcpf = hMcpf;
	pHalSocket->hHostSocket = hHostSocket;
	pHalSocket->onAcceptCb = onAcceptCb;
	pHalSocket->onRecvCb = onRecvCb;
	pHalSocket->onCloseCb = onCloseCb;

	pHalSocket->sockId = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(pHalSocket->sockId < 0)
	{
		MCPF_REPORT_ERROR(hMcpf, HAL_SOCKET_MODULE_LOG,
			("mcp_hal_socket_CreateServerSocket: socket() Failed!"));
		mcpf_memory_pool_destroy(hMcpf, pHalSocket->hClientsPool);
		mcpf_mem_free(hMcpf, pHalSocket);
		return NULL;
	}

	memset(&lSockAddr,0, sizeof(lSockAddr));
	lSockAddr.sin_family = AF_INET;
	lSockAddr.sin_port = htons(port);
	lSockAddr.sin_addr.s_addr = INADDR_ANY;

	/* Bind - links the socket we just created with the sockaddr_in
	   structure. Basically it connects the socket with
	   the local address and a specified port.
	   If it returns non-zero - quit, as this indicates error. */
   if(bind(pHalSocket->sockId,(struct sockaddr *)&lSockAddr,sizeof(lSockAddr)) != 0)
    {
		MCPF_REPORT_ERROR(hMcpf, HAL_SOCKET_MODULE_LOG,
			("mcp_hal_socket_CreateServerSocket: bind() Failed!"));
		close(pHalSocket->sockId);
		mcpf_memory_pool_destroy(hMcpf, pHalSocket->hClientsPool);
		mcpf_mem_free(hMcpf, pHalSocket);
        return NULL;
    }

    /* Listen - instructs the socket to listen for incoming
       connections from clients. The second arg is the backlog. */
    if(listen(pHalSocket->sockId,10) != 0)
    {
		MCPF_REPORT_ERROR(hMcpf, HAL_SOCKET_MODULE_LOG,
			("mcp_hal_socket_CreateServerSocket: listen() Failed!"));
		close(pHalSocket->sockId);
		mcpf_memory_pool_destroy(hMcpf, pHalSocket->hClientsPool);
		mcpf_mem_free(hMcpf, pHalSocket);
        return NULL;
    }
	//hThread = pthread_create( &thread1, NULL, serverSocket_ThreadFunc, pHalSocket);

	return (handle_t)pHalSocket;
}

/** 
 * \fn     mcp_hal_socket_Accept
 * \brief  Socket Accept at the Server side
 * 
 */
void mcp_hal_socket_Accept(handle_t hHalSocket) 
{
	hal_socket_t	*pHalSock = (hal_socket_t *)hHalSocket;
	McpU32			hThread;
	pthread_t		thread1;

	/* This function is blocking, and is running in the application main thread */
	serverSocket_ThreadFunc(pHalSock);
}

/** 
 * \fn     mcp_hal_socket_DestroyServerSocket
 * \brief  Socket Initializing and Binding at the Server side
 * 
 */
void mcp_hal_socket_DestroyServerSocket(handle_t hHalSocket) 
{
	hal_socket_t	*pHalSock = (hal_socket_t *)hHalSocket;

		/* TODO: Need to destroy Thread */

	close(pHalSock->sockId);
}

/** 
 * \fn     mcp_hal_socket_Send 
 * \brief  Send data back to the client
 * 
 */
EMcpfRes mcp_hal_socket_Send(handle_t hHalSocket, McpU32 clientSockId, McpS8 *buf, McpU16 bufLen)
{
	hal_socket_t *pHalSocket = (hal_socket_t *)hHalSocket;
	McpU32	clientSock = (McpU32)clientSockId;
	McpU32	bytesSent = 0;

	do
	{
		bytesSent += send(clientSock, &buf[bytesSent], bufLen-bytesSent, 0);
	} while ( (bytesSent > 0) && (bytesSent < bufLen) );

	if(bytesSent == 0)
	{
		MCPF_REPORT_ERROR(pHalSocket->hMcpf, HAL_SOCKET_MODULE_LOG,
			("mcp_hal_socket_Send: Send Failed!"));
		return RES_ERROR;
	}
	else
	{
		MCPF_REPORT_INFORMATION(pHalSocket->hMcpf, HAL_SOCKET_MODULE_LOG,
			("mcp_hal_socket_Send: Send was succesful! bytesSent = %d", bytesSent));
	return RES_OK;
}
}


/************************************************************************/
/* Internal Functions Implementation                                    */
/************************************************************************/
/** 
 * \fn     serverSocket_ThreadFuncThreadFunc 
 * \brief  Server Socket Thread Function, where the 
 *		   Incoming connections From Socket is handled
 * 
 **/
void serverSocket_ThreadFunc(void* hSocket)
{
	hal_socket_t	*pHalSock = (hal_socket_t *)hSocket;
	struct sockaddr_in 	from;
	int				fromlen = sizeof(from);
	McpU32			clientThread;
	unsigned long	client_Handle;
	client_handle_t	*hClientHandle;
	McpU32			clienSockId;
	McpBool			bInLoop = MCP_TRUE;
	pthread_t       thread2;

	MCPF_REPORT_INFORMATION(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
				("serverSocket_ThreadFunc: Entering..."));
	
	while(bInLoop)
	{
		clienSockId = accept(pHalSock->sockId,(struct sockaddr *)&from,&fromlen);
		MCPF_REPORT_INFORMATION(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
			("serverSocket_ThreadFunc: accept() for sockId %d, socket descriptor is %d", pHalSock->sockId, clienSockId ));

		if(clienSockId < 0)
		{
			MCPF_REPORT_INFORMATION(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
				("serverSocket_ThreadFunc: accept() returned INVALID_SOCKET! clienSockId is %d", clienSockId ));
			continue;
		}

		hClientHandle = (client_handle_t *)mcpf_mem_alloc_from_pool(pHalSock->hMcpf, pHalSock->hClientsPool);
		if(hClientHandle == NULL)
		{
			MCPF_REPORT_ERROR(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
				("serverSocket_ThreadFunc: Failed to allocate client structure!"));
			break;
		}

		hClientHandle->hHalSock = hSocket;
		hClientHandle->clientSockId = clienSockId;

		hClientHandle->hCaller = pHalSock->onAcceptCb(pHalSock->hHostSocket, hClientHandle->clientSockId,
														&hClientHandle->uNumOfBytesToRead);

		if(hClientHandle->hCaller == NULL)
		{
			MCPF_REPORT_ERROR(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
				("serverSocket_ThreadFunc: OnAcceptCb() Failed!"));
			mcpf_mem_free_from_pool(pHalSock->hMcpf, hClientHandle);
			break;
		}
		clientThread = pthread_create( &thread2, NULL, clientSocket_ThreadFunc, hClientHandle);
	}
}

/** 
 * \fn     clientSocket_ThreadFunc 
 * \brief  Call Back Function Where the Multiple Clients are handled
 * 
 * 
 */
void clientSocket_ThreadFunc(void* hClientHandle)
{
	client_handle_t	*pClientHandle = (client_handle_t *)hClientHandle;
	hal_socket_t	*pHalSock = (hal_socket_t *)pClientHandle->hHalSock;
	McpS8		IncData[9000];
	McpS32		recvLen = 0;
	McpU32		uMoreBytesToRead;
	McpBool			bInLoop = MCP_TRUE;

	while(bInLoop)
	{
		recvLen = 0;
		do
		{
			recvLen += recv(pClientHandle->clientSockId, &IncData[recvLen],
				pClientHandle->uNumOfBytesToRead-recvLen, 0);

			MCPF_REPORT_INFORMATION(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
				("serverSocket_ThreadFunc: recv() is passed the clienSockId of %d", pClientHandle->clientSockId));

		} while ( (recvLen > 0) && (recvLen < pClientHandle->uNumOfBytesToRead) );

		if(recvLen < 0)
		{
			MCPF_REPORT_ERROR(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
				("clientSocket_ThreadFunc: Error on recv() 1"));
			break;
		}
		else if(recvLen == 0)
		{
			MCPF_REPORT_ERROR(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
				("clientSocket_ThreadFunc: sokcet was closed by peer 1"));
			break;
		}
		else
		{
			uMoreBytesToRead = pHalSock->onRecvCb(pClientHandle->hCaller, IncData, (McpS16)recvLen);


			memset((void*)&IncData, 0, 9000);

			if(uMoreBytesToRead != 0)
			{
				recvLen = 0;
				do
				{
					recvLen += recv(pClientHandle->clientSockId, &IncData[recvLen],
						uMoreBytesToRead-recvLen, 0);

				} while ( (recvLen > 0) && ((McpU32)recvLen < uMoreBytesToRead) );

				if(recvLen < 0)
				{
					MCPF_REPORT_ERROR(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
						("clientSocket_ThreadFunc: Error on recv() 2, Error code returned is %s",strerror(errno) ));
					break;
				}
				else if(recvLen == 0)
				{
					MCPF_REPORT_ERROR(pHalSock->hMcpf, HAL_SOCKET_MODULE_LOG,
						("clientSocket_ThreadFunc: sokcet was closed by peer 2"));
					break;
				}
				else
				{
					uMoreBytesToRead = pHalSock->onRecvCb(pClientHandle->hCaller, IncData, (McpS16)recvLen);

					memset((void*)&IncData, 0, 9000);
				}
			}
		}
	}

	pHalSock->onCloseCb(pClientHandle->hCaller);
	mcpf_mem_free_from_pool(pHalSock->hMcpf, hClientHandle);
}

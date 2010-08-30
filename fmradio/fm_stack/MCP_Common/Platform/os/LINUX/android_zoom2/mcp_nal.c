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

#include "mcpf_nal.h"

/* For Socket Programming. */
#include <sys/ioctl.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>


/* #define ENABLE_NAL_DEBUG */
#ifdef ENABLE_NAL_DEBUG
    #define LOG_TAG "mcpf_nal"
    #include <utils/Log.h>
    #define  DEBUG_MCPF_NAL(...)   LOGD(__VA_ARGS__)
#else
    #define  DEBUG_MCPF_NAL(...)   ((void)0)
#endif /* ENABLE_NAL_DEBUG */

#define HS_PORT 4000
#define HS_HOST_ADDR "localhost"

#define MAX_LENGTH 256

static int connectWithHelperService(const int u16_inPortNumber, const int *const p_inHostName);

static int sendDataToHelperService(void *p_buffer, int size, eNetCommands cmd);



int          g_socketDescriptor;              /* */


EMcpfRes NAL_executeCommand(eNetCommands cmd,void *data)
{
    s_NAL_CmdParams s_nalCmdParams;
    s_NAL_subscriberId *p_subscriberId = NULL;

    int retVal = 0;

    retVal = connectWithHelperService(HS_PORT, HS_HOST_ADDR);
    if (0 != retVal)
    {
        DEBUG_MCPF_NAL(" NAL_executeCommand: connectWithHelperService FAILED !!! \n");
        return RES_ERROR;
    }

    s_nalCmdParams.e_nalCommand = cmd;
    switch(cmd)
    {
        case MCPF_NAL_TLS_SEND:
        {
            if (data == NULL)
            {
                DEBUG_MCPF_NAL(" NAL_executeCommand*: data NULL !!! \n");
            }
            else
            {
                memcpy((void *)&s_nalCmdParams.u_payload, data, sizeof(s_NAL_slpData) );
            }
        }
        break;

        case MCPF_NAL_TLS_CREATE_CONNECTION:
        {
            if (data == NULL)
            {
                DEBUG_MCPF_NAL(" NAL_executeCommand: data NULL !!! \n");
            }
            else
            {
                s_NAL_createConnection *p_data = NULL;
                p_data  = (s_NAL_createConnection *)data;

                DEBUG_MCPF_NAL(" NAL_executeCommand: a_hostPort = %s \n", p_data->a_hostPort);


                DEBUG_MCPF_NAL(" NAL_executeCommand: FAILED IN!!!!!!!!!!!!!!!!!!! \n");
                memcpy((void *)&s_nalCmdParams.u_payload, data, sizeof(s_NAL_createConnection) );
                DEBUG_MCPF_NAL(" NAL_executeCommand: FAILED OUT!!!!!!!!!!!!!!!!!!! \n");
            }
        }
        break;

        default:
        {
        }
        break;

    }

    retVal = sendDataToHelperService((void *)&s_nalCmdParams,
                                     sizeof(s_nalCmdParams),
                                     s_nalCmdParams.e_nalCommand  );
    if (0 > retVal)
    {
        DEBUG_MCPF_NAL(" NAL_executeCommand: sendDataToHelperService FAILED !!! \n");
        close(g_socketDescriptor);
        return RES_ERROR;
    }
    if (0 < retVal)
    {
        DEBUG_MCPF_NAL(" NAL_executeCommand: sendDataToHelperService FAILED !!! \n");
        close(g_socketDescriptor);
        return RES_PENDING; /* Temporary. Add proper enum. */

    }

    /* APK: Temporary Hack. */
    if (MCPF_NAL_GET_SUBSCRIBER_ID == s_nalCmdParams.e_nalCommand)
    {
        /* In this case, 'data' is an out parameter. Handle it accordingly. */

        p_subscriberId = (s_NAL_subscriberId *)data;

        if (!p_subscriberId->a_imsiData || !s_nalCmdParams.u_payload.s_subscriberId.a_imsiData)
        {
            DEBUG_MCPF_NAL(" NAL_executeCommand: sendDataToHelperService NULL ptr \n");
            return RES_ERROR;
        }

        memcpy((void *)p_subscriberId->a_imsiData,
               (void *)s_nalCmdParams.u_payload.s_subscriberId.a_imsiData,
               sizeof(p_subscriberId->a_imsiData));
        p_subscriberId->s32_size = s_nalCmdParams.u_payload.s_subscriberId.s32_size;

        DEBUG_MCPF_NAL(" NAL_executeCommand: s32_size = %d \n", p_subscriberId->s32_size);

    }

    if (MCPF_NAL_GET_CELL_INFO == s_nalCmdParams.e_nalCommand)
    {
        s_NAL_GsmCellInfo *p_gsmCellInfo = NULL;
        McpU8 count = 0;


        /* In this case, 'data' is an out parameter. Handle it accordingly. */
        p_gsmCellInfo = (s_NAL_subscriberId *)data;

        for (count = 0; count < MAX_CELL_PARAMS; count++)
        {
            p_gsmCellInfo->a_cellInfo[count] = s_nalCmdParams.u_payload.s_cellInfo.a_cellInfo[count];
            DEBUG_MCPF_NAL(" NAL_executeCommand:  a_cellInfo[%d] is %d \n", count,
                                                        p_gsmCellInfo->a_cellInfo[count]);
        }


    }


    DEBUG_MCPF_NAL(" NAL_executeCommand: Closing the socket %d \n", HS_PORT);
    close(g_socketDescriptor);

    DEBUG_MCPF_NAL(" NAL_executeCommand: Entering. Received cmd = %d from SUPL Core \n", cmd);
    return RES_COMPLETE;
}

static int sendDataToHelperService(void *p_buffer, int size, eNetCommands cmd)
{
    char read_buffer[MAX_LENGTH] = {'\0'};

    DEBUG_MCPF_NAL(" sendDataToHelperService: Entering \n");

    /* Just in case... */
    if (g_socketDescriptor == 0)
    {
        DEBUG_MCPF_NAL(" sendDataToHelperService: Connection with MCPF not yet established. \n");
        return -1;
    }

    /* Send the command to MCPF. */
    if ( send(g_socketDescriptor, p_buffer, size, 0  ) < 0)
    {
        DEBUG_MCPF_NAL(" sendDataToHelperService: Message Sending FAILED !!! \n");
        return -2;
    }

#if 1
    DEBUG_MCPF_NAL(" ===>>> sendDataToHelperService: Blocked on read \n");
    if ( read(g_socketDescriptor, read_buffer, MAX_LENGTH) < 0)
    {
        DEBUG_MCPF_NAL(" sendDataToHelperService: Message reading FAILED !!! \n");
        return -3;
    }
    /* APK: Temporary Hack to handle SUBSCRIBER_ID request. */
    if ( ( cmd == MCPF_NAL_GET_SUBSCRIBER_ID) ||
         (cmd == MCPF_NAL_GET_CELL_INFO) )
    {
        s_NAL_CmdParams *p_nalCmdParams = NULL;

        p_nalCmdParams = (s_NAL_CmdParams *)p_buffer;
        memcpy((void *)&p_nalCmdParams->u_payload, (void *)read_buffer, sizeof(p_nalCmdParams->u_payload) );
        return 0;
    }

    DEBUG_MCPF_NAL(" ===>>> sendDataToHelperService: Response received. read_buffer = %s === %d \n", read_buffer, atoi(read_buffer));
    if ( atoi(read_buffer) < 0)
    {
        DEBUG_MCPF_NAL(" sendDataToHelperService: Response FAILED !!! \n");
        return -4;
    }

    if ( atoi(read_buffer) > 0)
    {
        DEBUG_MCPF_NAL(" sendDataToHelperService: Response SUCCESS !!! \n");
        return 1;

    }
#endif

    return 0;
}

static int connectWithHelperService(const int u16_inPortNumber,
                                     const int *const p_inHostName)
{
    struct sockaddr_in serverAddress;       /* Internet Address of Server. */
    struct hostent *p_host = NULL;          /* The host (server) info. */
    int u16_sockDescriptor = 0;
    int retVal = 0;

    DEBUG_MCPF_NAL(" connectWithHelperService: Entering \n");

    /* Obtain host information. */
    p_host = gethostbyname((char *)p_inHostName);
    if (p_host == (struct hostent *) NULL )
    {
        DEBUG_MCPF_NAL(" connectWithHelperService: gethostbyname FAILED !!!");
        return -1;
    }

    /* Clear the structure. */
    memset( &serverAddress, 0, sizeof(serverAddress) );

    /* Set address type. */
    serverAddress.sin_family = AF_INET;
    memcpy(&serverAddress.sin_addr, p_host->h_addr, p_host->h_length);
    serverAddress.sin_port = htons(u16_inPortNumber);

    /* Create a new socket. */
    u16_sockDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (u16_sockDescriptor < 0)
    {
        DEBUG_MCPF_NAL(" connectWithHelperService: Socket creation FAILED !!!");
        return -2;
    }

    /* Connect with MCPF. */
    retVal = connect(u16_sockDescriptor,
                     (struct sockaddr *)&serverAddress,
                     sizeof(serverAddress) );
    if (retVal < 0)
    {
        DEBUG_MCPF_NAL(" connectWithHelperService: Connection with MCPF FAILED !!! \n");
        return -3;
    }

    /* Maintain a global variable for keeping the socket descriptor. */
    g_socketDescriptor = u16_sockDescriptor;

    DEBUG_MCPF_NAL(" connectWithHelperService: Exiting Successfully. \n");
    return 0;

}


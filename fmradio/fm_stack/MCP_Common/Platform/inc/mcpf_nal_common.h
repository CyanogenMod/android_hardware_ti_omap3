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
#ifndef __SUPLC_NAL_COMMON_H__
#define __SUPLC_NAL_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MAX_BUFFER_LENGTH   128
#define MAX_SIZE_IMSI_BUFFER    32
#define MAX_CELL_PARAMS 4

typedef enum{
    NAL_TRUE,
    NAL_FALSE,
    NAL_UNKNOWN

}eNAL_Result;


typedef enum
{
    MCPF_NAL_NO_REQUEST = 0,

    MCPF_NAL_START_SMS_LISTEN,
    MCPF_NAL_STOP_SMS_LISTEN,

    MCPF_NAL_START_WAP_LISTEN,
    MCPF_NAL_STOP_WAP_LISTEN,

    MCPF_NAL_TLS_INIT,
    MCPF_NAL_TLS_IS_ACTIVE,

    MCPF_NAL_TLS_CREATE_CONNECTION,
    MCPF_NAL_TLS_FREE_CONNECTION,

    MCPF_NAL_TLS_SEND,
    MCPF_NAL_TLS_START_RECEIVE,

    MCPF_NAL_GET_SUBSCRIBER_ID,
    MCPF_NAL_GET_CELL_INFO,

    MCPF_NAL_MSGBOX, /* Added for NI */
    MCPF_NAL_CMD_MAX

}eNetCommands;


typedef struct
{
}NwkCmdDataCreateTlsConnection;

typedef struct
{
}NwkCmdDataTlsSLPData;

typedef struct slpData
{
    char a_buffer[MAX_BUFFER_LENGTH];
    int length;

}s_NAL_slpData;


/*
typedef struct
{
}NwkCmdDataTlsInit;

typedef struct
{
}NwkCmdDataTlsIsActive;

typedef struct
{
}NwkCmdDataTlsStartListenSMS;

typedef struct
{
}NwkCmdDataTlsStartListenWAP;

typedef struct
{
}NwkCmdDataTlsRlsConnection;

*/

typedef struct createConnection{
    char a_hostPort[32];
}s_NAL_createConnection;

typedef struct getSubscriberId{
    char a_imsiData[MAX_SIZE_IMSI_BUFFER];
    int s32_size;

}s_NAL_subscriberId;

typedef struct gsmCellInfo{
    int a_cellInfo[MAX_CELL_PARAMS];

}s_NAL_GsmCellInfo;

/* Added for NI */
typedef struct notificationBytes{
    char * bytes;
    int size;
    int encoding;
} s_NAL_NotificationBytes;

/* Added for NI */
typedef struct msgBox
{
     int verification;
     s_NAL_NotificationBytes strings[2]; //SMSWAP_MESSAGE_STRINGS
     int size;
     int Id;
     int timeout;
} s_NAL_msgBox;

/* Added for NI */
typedef struct msgBoxResp
{
	int res;
	int tid;
} s_NAL_msgBoxResp;
typedef union cmdParams
{
    s_NAL_slpData s_slpData;
    s_NAL_createConnection s_createConnection;
    s_NAL_subscriberId s_subscriberId;
    s_NAL_GsmCellInfo s_cellInfo;
    s_NAL_msgBox s_msgBox; /* Added for NI */
	s_NAL_msgBoxResp s_msgBoxResp; /* Added for NI */
}TNAL_cmdPaylaod;

typedef struct nalCmd
{
    eNetCommands e_nalCommand;
    TNAL_cmdPaylaod u_payload;

}s_NAL_CmdParams;





typedef eNAL_Result (*NALCallback)(const eNetCommands cmd, void *data);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __SUPLC_NAL_COMMON_H__ */


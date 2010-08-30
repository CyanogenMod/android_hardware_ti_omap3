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


#ifndef __MCPF_GLOBAL_DEFS__
#define __MCPF_GLOBAL_DEFS__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* #define ENABLE_MCPF_DEBUG */
#ifdef ENABLE_MCPF_DEBUG
    #define LOG_TAG "mcpf_socket_server"
#include <utils/Log.h>
    #define  MCPF_DEBUG(...)   LOGD(__VA_ARGS__)
#else
    #define  MCPF_DEBUG(...)   ((void)0)
#endif /* ENABLE_MCPF_DEBUG */

#define MCPF_SOCKET_PORT        2000
#define MCPF_HOST_NAME          "127.0.6.1"

typedef unsigned char   uInt8;
typedef unsigned short  uInt16;
typedef unsigned long   uInt32;

typedef signed char     sInt8;
typedef signed short    sInt16;
typedef signed long     sInt32;


/** A module has to register with MCPF with one of the following tokens before
  * starting the communication.
  * Note: Each token has its own messaging structures.
  * It's the sole responsibility of a client to include corresponding structures.*/
typedef enum registrationToken{
    MCPF_ADAPTATION_LAYER = 1,
    MCPF_HOST_TOOLKIT = 2,

    MCPF_MAX_MEMBERS

}eMCPF_RegistrationToken;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MCPF_GLOBAL_DEFS__ */

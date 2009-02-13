/*
 * TI's FM Stack
 *
 * Copyright 2001-2008 Texas Instruments, Inc. - http://www.ti.com/
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

#ifndef __FMC_TYPES_H
#define __FMC_TYPES_H

#include "mcp_hal_types.h"

/* -------------------------------------------------------------
 *                  8 Bits Types
 */
typedef McpU8       FMC_U8;
typedef McpS8       FMC_S8;

/* -------------------------------------------------------------
 *                  16 Bits Types
 */
typedef McpU16      FMC_U16;
typedef McpS16      FMC_S16;

/* -------------------------------------------------------------
 *                  32 Bits Types
 */
typedef McpU32      FMC_U32;
typedef McpS16      FMC_S32;


/* -------------------------------------------------------------
 *          Native Integer Types (# of bits irrelevant)
 */
typedef McpInt      FMC_INT;
typedef McpUint     FMC_UINT;


typedef McpU32 FmcOsEvent;
/* TODO ronen: consider moving to fmc_os.h */
    
/* --------------------------------------------------------------
 *                  Boolean Definitions                          
 */

/* --------------------------------------------------------------
 *                  Boolean Definitions                          
 */
typedef McpBool FMC_BOOL;

#define FMC_TRUE    MCP_TRUE
#define FMC_FALSE   MCP_FALSE

/*
*/
#define FMC_NO_VALUE                                    ((FMC_U32)0xFFFFFFFFUL)

/*-------------------------------------------------------------------------------
 * FmcStatus Type
 *
 *  Status codes have 3 ranges:
 *  1. Common codes
 *  2. RX-specific codes
 *  3. Tx-specific codes
 *
 *  The common codes are defined in this file. Rx-specific codes are defined in fm_rx.h and
 *  Tx-specific codes in fm_tx.h
 *  
 *  For consistency, the common codes are duplicated in RX & TX. They have the same value
 *  and the same meaning as the corresponding common code. That way when a status code 
 *  0 (for example) is returned form any FM stack function (Rx / Tx / Common) it always means SUCCESS.
 *
 *  Codes that are specific to Rx start from FMC_FIRST_FM_RX_STATUS_CODE
 *  Codes that are specific to Tx start from FMC_FIRST_FM_TX_STATUS_CODE
 */
typedef FMC_UINT FmcStatus;

#define FMC_STATUS_SUCCESS                          ((FmcStatus)0)
#define FMC_STATUS_FAILED                               ((FmcStatus)1)
#define FMC_STATUS_PENDING                              ((FmcStatus)2)
#define FMC_STATUS_INVALID_PARM                     ((FmcStatus)3)
#define FMC_STATUS_IN_PROGRESS                      ((FmcStatus)4)
#define FMC_STATUS_NOT_APPLICABLE                       ((FmcStatus)5)
#define FMC_STATUS_NOT_SUPPORTED                        ((FmcStatus)6)
#define FMC_STATUS_INTERNAL_ERROR                       ((FmcStatus)7)
#define FMC_STATUS_TRANSPORT_INIT_ERR                   ((FmcStatus)8)
#define FMC_STATUS_HARDWARE_ERR                     ((FmcStatus)9)
#define FMC_STATUS_NO_VALUE_AVAILABLE               ((FmcStatus)10)
#define FMC_STATUS_CONTEXT_DOESNT_EXIST             ((FmcStatus)11)
#define FMC_STATUS_CONTEXT_NOT_DESTROYED            ((FmcStatus)12)
#define FMC_STATUS_CONTEXT_NOT_ENABLED              ((FmcStatus)13)
#define FMC_STATUS_CONTEXT_NOT_DISABLED             ((FmcStatus)14)
#define FMC_STATUS_NOT_DE_INITIALIZED                   ((FmcStatus)15)
#define FMC_STATUS_NOT_INITIALIZED                      ((FmcStatus)16)
#define FMC_STATUS_TOO_MANY_PENDING_CMDS            ((FmcStatus)17)
#define FMC_STATUS_DISABLING_IN_PROGRESS                ((FmcStatus)18)
#define FMC_STATUS_NO_RESOURCES                     ((FmcStatus)19)
#define FMC_STATUS_FM_COMMAND_FAILED                ((FmcStatus)20)
#define FMC_STATUS_SCRIPT_EXEC_FAILED                   ((FmcStatus)21)
#define FMC_STATUS_PROCESS_TIMEOUT_FAILURE                  ((FmcStatus)22)

#define FMC_STATUS_FAILED_BT_NOT_INITIALIZED                                ((FmcStatus)23)
#define FMC_STATUS_AUDIO_OPERATION_UNAVAILIBLE_RESOURCES            ((FmcStatus)24)
#define FMC_STATUS_LAST                             ((FmcStatus)FMC_STATUS_AUDIO_OPERATION_UNAVAILIBLE_RESOURCES)

#define FMC_FIRST_FM_RX_STATUS_CODE                 ((FmRxStatus)100)
#define FMC_FIRST_FM_TX_STATUS_CODE                 ((FmTxStatus)200)

#endif  /* __FMC_TYPES_H */


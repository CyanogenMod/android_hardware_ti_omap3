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


#ifndef __MCPF_SERVICES_H
#define __MCPF_SERVICES_H

#ifdef __cplusplus
extern "C" {
#endif 

#include <stdio.h>

#include "mcpf_defs.h"
#include "mcp_endian.h"
#include "mcpf_queue.h"
#include "BusDrv.h"
#include "mcp_IfSlpMng.h"
#include "mcp_transport.h"


/** APIs **/

/* Transport */
#define	mcpf_trans_ChOpen(hMcpf, chanNum, fRxIndCb, hCbParam) 	trans_ChOpen(((Tmcpf *)hMcpf)->hTrans, chanNum, fRxIndCb, hCbParam) 

#define	mcpf_trans_ChClose(hMcpf, chanNum) 						trans_ChClose(((Tmcpf *)hMcpf)->hTrans, chanNum)

#define	mcpf_trans_TxData(hMcpf, pTxn)							trans_TxData(((Tmcpf *)hMcpf)->hTrans, pTxn)

#define	mcpf_trans_BufAvailable(hMcpf)							trans_BufAvailable(((Tmcpf *)hMcpf)->hTrans)

#define	mcpf_trans_SetSpeed(hMcpf, speed)						trans_SetSpeed(((Tmcpf *)hMcpf)->hTrans, speed)


/* Queue Handling */
#define	mcpf_que_Create  		que_Create

#define	mcpf_que_Destroy 		que_Destroy

#define	mcpf_que_Enqueue 		que_Enqueue

#define	mcpf_que_Dequeue 		que_Dequeue

#define	mcpf_que_Requeue 		que_Requeue

#define	mcpf_que_Size    		que_Size


/* Little/Big endianess conversion (LEtoHost16/32…) */
#define mcpf_endian_BEtoHost16 		MCP_ENDIAN_BEtoHost16

#define mcpf_endian_BEtoHost32 		MCP_ENDIAN_BEtoHost32

#define mcpf_endian_LEtoHost16 		MCP_ENDIAN_LEtoHost16

#define mcpf_endian_LEtoHost32 		MCP_ENDIAN_LEtoHost32

#define mcpf_endian_HostToLE16 		MCP_ENDIAN_HostToLE16

#define mcpf_endian_HostToLE32 		MCP_ENDIAN_HostToLE32

#define mcpf_endian_HostToBE16 		MCP_ENDIAN_HostToBE16

#define mcpf_endian_HostToBE32 		MCP_ENDIAN_HostToBE32


/** 
 * \fn     mcpf_getBits
 * \brief  Get bits field
 * 
 * Get bits field of specified length from sorce buffer
 * 
 * \note
 * \param	pBuf - source buffer
 * \param	bitOffset - offset in bits from pBuf to start read
 * \param	bitLength - number of bits to read
 * \return 	bits field
 * \sa     	mcpf_getSignedBits
 */ 
McpU32 mcpf_getBits(McpU8* pBuf, McpU32 *pBitOffset, McpU32 uBitLength);

/** 
 * \fn     mcpf_getBits
 * \brief  Get signed bits field
 * 
 * Get signed bits field of specified length from sorce buffer
 * 
 * \note
 * \param	pBuf - source buffer
 * \param	bitOffset - offset in bits from pBuf to start read
 * \param	bitLength - number of bits to read
 * \return 	signed bits field
 * \sa     	mcpf_getSignedBits
 */ 
McpS32 mcpf_getSignedBits(McpU8* pBuf, McpU32 *pBitOffset, McpU32 uBitLength);

/** 
 * \fn     mcpf_putBits
 * \brief  Put bits field
 * 
 * Put bits field into output buffer
 * 
 * \note
 * \param	pBuf 		- output buffer
 * \param	pBitOffset 	- pointer to offset in bits from pBuf to start write bits (in/out)
 * \param	uInputVal  	- input value to put as bit field into pBuf starting from pBitOffset
 * \param	uInputBitLength  - number of bits of uInputVal to write
 * \return 	bits field
 * \sa     	mcpf_getBits
 */ 
void mcpf_putBits(McpU8* pBuf, McpU32 *pBitOffset, McpU32 uInputVal, McpU32 uInputBitLength);


/* Critical Section */

/** 
 * \fn     mcpf_critSec_CreateObj
 * \brief  Create critical section object
 * 
 * This function creates a critical section object.
 * 
 * \note
 * \param	hMcpf - MCPF handler. 
 * \param	*sCritSecName - name of critical section object.
 * \param	*pCritSecHandle - Handler to the created critical section object.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_critSec_CreateObj
 */ 
EMcpfRes mcpf_critSec_CreateObj(handle_t hMcpf, const char *sCritSecName, handle_t *pCritSecHandle); 

/** 
 * \fn     mcpf_critSec_DestroyObj
 * \brief  Destroy the critical section object
 * 
 * This function destroys the critical section object.
 * 
 * \note
 * \param	hMcpf - MCPF handler. 
 * \param	*pCritSecHandle - Handler to the created critical section object.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_critSec_DestroyObj
 */ 
EMcpfRes mcpf_critSec_DestroyObj(handle_t hMcpf, handle_t *pCritSecHandle); 

/** 
 * \fn     mcpf_critSec_Enter
 * \brief  Enter critical section
 * 
 * This function tries to enter the critical section, be locking a semaphore.
 * 
 * \note
 * \param	hMcpf - MCPF handler. 
 * \param	hCritSecHandle - Handler to the created critical section object.
 * \param	uTimeout - How long to wait for the semaphore.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_critSec_Enter
 */ 
EMcpfRes mcpf_critSec_Enter(handle_t hMcpf, handle_t hCritSecHandle, McpUint uTimeout);

/** 
 * \fn     mcpf_critSec_Exit
 * \brief  Exit critical section
 * 
 * This function exits the critical section, be un-locking the semaphore.
 * 
 * \note
 * \param	hMcpf - MCPF handler. 
 * \param	hCritSecHandle - Handler to the created critical section object.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_critSec_Exit
 */ 
EMcpfRes mcpf_critSec_Exit(handle_t hMcpf, handle_t hCritSecHandle);

/* MCPF Global Critical Section */
#define MCPF_ENTER_CRIT_SEC(hMcpf) 	mcpf_critSec_Enter(hMcpf, ((Tmcpf *)hMcpf)->hMcpfCritSecObj, MCPF_INFINIT)

#define MCPF_EXIT_CRIT_SEC(hMcpf) 		mcpf_critSec_Exit(hMcpf, ((Tmcpf *)hMcpf)->hMcpfCritSecObj)


/* Files */
EMcpfRes	mcpf_file_open(handle_t hMcpf, McpU8 *pFileName, McpU32 uFlags, McpS32 *pFd);

EMcpfRes	mcpf_file_close(handle_t hMcpf, McpS32 iFd);

McpU32		mcpf_file_read(handle_t hMcpf, McpS32 iFd, McpU8 *pMem, McpU16 uNumBytes);

McpU32		mcpf_file_write(handle_t hMcpf, McpS32 iFd, McpU8 *pMem, McpU16 uNumBytes);

EMcpfRes	mcpf_file_empty(handle_t hMcpf, McpS32 iFd);

McpU32  	mcpf_getFileSize  (handle_t hMcpf, McpS32 iFd);

void mcpf_ExtractDateAndTime(McpU32 st_time, McpHalDateAndTime* dateAndTimeStruct);


/* HW services*/

/** 
 * \fn     mcpf_hwGpioSet
 * \brief  Set HW GPIOs
 * 
 * This function will set the HW GPIOs.
 * 
 * \note
 * \param	hMcpf - MCPF handler. 
 * \param	index - GPIO index.
 * \param	value - value to set.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_hwGpioSet
 */ 
EMcpfRes  	mcpf_hwGpioSet(handle_t hMcpf, McpU32 index, McpU8 value);

/** 
 * \fn     mcpf_hwRefClkSet
 * \brief  Enable/Disable Ref Clock
 * 
 * This function will Enable or Disable the Reference clock.
 * 
 * \note
 * \param	hMcpf - MCPF handler. 
 * \param	uIsEnable - Enable/Disable the ref clk.
 * \return 	Result of operation: OK or ERROR
 * \sa     	mcpf_hwRefClkSet
 */ 
EMcpfRes  	mcpf_hwRefClkSet(handle_t hMcpf, McpU8 uIsEnable);

/* Math */
/** 
 * \fn     mcpf_Cos
*/
McpDBL	mcpf_Cos (McpDBL A);


/** 
 * \fn     mcpf_Sin
*/
McpDBL	mcpf_Sin (McpDBL A);


/** 
 * \fn     mcpf_Atan2
*/
McpDBL	mcpf_Atan2 (McpDBL A, McpDBL B);


/** 
 * \fn     mcpf_Sqrt
*/
McpDBL	mcpf_Sqrt (McpDBL A);


/** 
 * \fn     mcpf_Fabs
*/
McpDBL	mcpf_Fabs (McpDBL A);

/** 
 * \fn     mcpf_Pow
*/
McpDBL	mcpf_Pow (McpDBL A, McpDBL B);


#ifdef __cplusplus
}
#endif

#endif

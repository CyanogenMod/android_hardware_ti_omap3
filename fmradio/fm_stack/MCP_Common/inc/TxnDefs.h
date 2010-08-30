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


/** \file   TxnDefs.h 
 *  \brief  Common TXN definitions                                  
 *
 * These defintions are used also by the SDIO/SPI adapters, so they shouldn't
 *     base on any non-standart types (e.g. use unsigned int and not McpU32)
 * 
 *  \see    
 */

#ifndef __TXN_DEFS_API_H__
#define __TXN_DEFS_API_H__


/************************************************************************
 * Defines
 ************************************************************************/
#define TXN_FUNC_ID_CTRL         0
#define TXN_FUNC_ID_BT           1
#define TXN_FUNC_ID_WLAN         2


/************************************************************************
 * Types
 ************************************************************************/
/* Transactions status (shouldn't override RES_OK and RES_ERROR values) */
typedef enum
{
    TXN_STATUS_NONE = 2,
    TXN_STATUS_OK,         
    TXN_STATUS_COMPLETE,   
    TXN_STATUS_PENDING,    
    TXN_STATUS_ERROR     

} ETxnStatus;


/*  Module definition */
typedef enum
{
    Txn_HalBusModule = 1,
	Txn_BusDrvModule,
	Txn_TxnQModule,
	Txn_TransModule,
	Txn_TransAdaptModule

} ETxnModuleId;


/*  Error identification */
typedef enum
{
    Txn_RxErr = 1,
	Txn_TxErr,
	Txn_MemErr,

} ETxnErrId;

/*  Error sevirity */
typedef enum
{
    TxnInfo = 1,
	TxnNormal,
	TxnMajor,
	TxnCritical

} ETxnErrSevirity;

/* Bus driver events type */
typedef enum
{
    Txn_InitComplInd = 1,
	Txn_DestroyComplInd,
	Txn_WriteComplInd,
	Txn_ReadReadylInd,
	Txn_ErrorInd

} ETxnEventType;

typedef struct
{
    ETxnModuleId 	eModuleId;
	ETxnErrId    	eErrId;
	ETxnErrSevirity eErrSeverity;
    char           *pErrStr;

} TTxnErrParams; 


typedef union
{
    TTxnErrParams err;       

} TTxnEventParams;


typedef struct
{
    ETxnEventType 	eEventType;
	TTxnEventParams tEventParams;

} TTxnEvent; 


typedef void (*TI_TxnEventHandlerCb)(handle_t hHandleCb, TTxnEvent * pEvent);



#endif /*__TXN_DEFS_API_H__*/

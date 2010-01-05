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


#include "mcp_hal_os.h"

#include "mcp_ver_defs.h"
#include "mcp_hal_string.h"
#include "ccm_hal_pwr_up_dwn.h"
#include "ccm_imi.h"
#include "ccm_imi_common.h"
#include "ccm_imi_bt_tran_mngr.h"
#include "mcp_hal_log.h"

MCP_HAL_LOG_SET_MODULE(MCP_HAL_LOG_MODULE_TYPE_CCM_IM);

typedef enum _tagCcmIm_StackEvent {
    _CCM_IM_STACK_EVENT_OFF,
    _CCM_IM_STACK_EVENT_ON_ABORT,
    _CCM_IM_STACK_EVENT_ON,

    _CCM_IM_STACK_EVENT_BT_TRAN_OFF_COMPLETE,
    _CCM_IM_STACK_EVENT_BT_TRAN_OFF_FAILED,
    _CCM_IM_STACK_EVENT_BT_TRAN_ON_COMPLETE,
    _CCM_IM_STACK_EVENT_BT_TRAN_ON_FAILED,
    _CCM_IM_STACK_EVENT_BT_TRAN_ON_ABORT_COMPLETE,

    _CCM_IM_STACK_EVENT_BT_TRAN_ON_NULL_EVENT,
    
    _CCM_IM_NUM_OF_STACK_EVENTS,
    _CCM_IM_INVALID_STACK_EVENT
} _CcmIm_StackEvent;

typedef struct _tagCcmImStackData _CcmImStackData;

typedef _CcmImStatus (*_CCM_IM_StackSmAction)(_CcmImStackData *stackData);

typedef struct _tagCcmIm_StackSmEntry {
    _CCM_IM_StackSmAction   action;
    CcmImStackState         nextState;
    _CcmIm_StackEvent       syncNextEvent;
} _CcmIm_StackSmEntry;

struct _tagCcmImStackData {
    CcmImObj            *containingImObj;
    CcmImStackId            stackId;
    CcmImEventCb        callback;
    CcmImStackState     state;
    McpBool             asyncCompletion;
};

struct tagCcmImObj {
    McpHalChipId                    chipId;
    McpHalOsSemaphoreHandle     mutexHandle;
    _CcmIm_BtTranMngr_Obj       *tranMngrObj;
    
    _CcmImStackData             stacksData[CCM_IM_MAX_NUM_OF_STACKS];
    
    CcmImEvent                  stackEvent;
    CcmChipOnNotificationCb     chipOnNotificationCB;
    void                        *userData;
    McpBool                     chipOnNotificationCalled;
};

typedef struct _tagCcmStaticData {
    CcmImObj                imObjs[MCP_HAL_MAX_NUM_OF_CHIPS];
    _CcmIm_StackSmEntry         stackSm[CCM_IM_NUM_OF_STACK_STATES][_CCM_IM_NUM_OF_STACK_EVENTS];
} _CcmImStaticData;

/*
    A single instance that holds the MCP_STATIC ("class") CCM data 
*/
MCP_STATIC  _CcmImStaticData _ccmIm_StaticData;

MCP_STATIC void _CCM_IM_StaticInitStackSm(void);

MCP_STATIC void _CCM_IM_InitObjData(CcmImObj *thisObj, McpHalChipId chipId);
MCP_STATIC void _CCM_IM_InitStackData(_CcmImStackData *stackData);

MCP_STATIC CcmImStatus _CCM_IM_HandleStackClientCall(_CcmImStackData *stackData, _CcmIm_StackEvent stackEvent);

MCP_STATIC _CcmImStatus _CCM_IM_StackSmHandleEvent(_CcmImStackData *stackData, _CcmIm_StackEvent stackEvent);

MCP_STATIC _CcmImStatus _CCM_IM_StackSmHandler_On(_CcmImStackData *stackData);
MCP_STATIC _CcmImStatus _CCM_IM_StackSmHandler_OnFailed(_CcmImStackData *stackData);
MCP_STATIC _CcmImStatus _CCM_IM_StackSmHandler_OnAbort(_CcmImStackData *stackData);
MCP_STATIC _CcmImStatus _CCM_IM_StackSmHandler_Off(_CcmImStackData *stackData);
MCP_STATIC _CcmImStatus _CCM_IM_StackSmHandler_OffFailed(_CcmImStackData *stackData);
MCP_STATIC _CcmImStatus _CCM_IM_StackSmHandler_OnComplete(_CcmImStackData *stackData);
MCP_STATIC _CcmImStatus _CCM_IM_StackSmHandler_OnAbortComplete(_CcmImStackData *stackData);
MCP_STATIC _CcmImStatus _CCM_IM_StackSmHandler_OffComplete(_CcmImStackData *stackData);

MCP_STATIC _CcmImStatus _CCM_IM_NotifyStack(    _CcmImStackData *stackData, 
                                                    CcmImEventType  eventType,
                                                    CcmImStatus         completionStatus);


MCP_STATIC void _CCM_IM_BtTranMngrCb(_CcmIm_BtTranMngr_CompletionEvent *event);

MCP_STATIC void _CCM_IM_GenerateMutexName(McpHalChipId chipId, char *mutexName);

MCP_STATIC McpHalCoreId _CCM_IM_StackId2CoreId(CcmImStackId stackId);
MCP_STATIC McpBool _CCM_IM_IsStatusExternal(_CcmImStatus status);

MCP_STATIC const char *_CCM_IM_DebugStackIdStr(CcmImStackState stackId);
MCP_STATIC const char *_CCM_IM_DebugStackStateStr(CcmImStackState stackState);
MCP_STATIC const char *_CCM_IM_DebugStackEventStr(_CcmIm_StackEvent stackEvent);

CcmImStatus CCM_IM_StaticInit(void)
{  
    MCP_FUNC_START("CCM_IM_StaticInit");

    MCP_FUNC_END();
    
    return CCM_IM_STATUS_SUCCESS;
}

CcmImStatus CCM_IM_Create(McpHalChipId chipId, CcmImObj **thisObj,  CcmChipOnNotificationCb notificationCB, void *userData)
{
    MCP_FUNC_START("CCM_IM_Create");
    
    MCP_FUNC_END();
    
    return CCM_IM_STATUS_SUCCESS;
}

CcmImStatus CCM_IM_Destroy(CcmImObj **thisObj)
{
    MCP_FUNC_START("CCM_IM_Destroy");
 
    MCP_FUNC_END();
    
    return CCM_IM_STATUS_SUCCESS;
}


BtHciIfObj *CCM_IMI_GetBtHciIfObj(CcmImObj *thisObj)
{
    return 0xdeadbeef;
}

CcmImStatus CCM_IM_RegisterStack(CcmImObj           *thisObj, 
                                        CcmImStackId        stackId,
                                        CcmImEventCb        callback,
                                        CCM_IM_StackHandle  *stackHandle)
{
    MCP_FUNC_START("CCM_IM_RegisterClient");

    MCP_FUNC_END();

    return CCM_IM_STATUS_SUCCESS;
}

CcmImStatus CCM_IM_DeregisterStack(CCM_IM_StackHandle *stackHandle)
{   
    MCP_FUNC_START("CCM_IM_DeregisterStack");

    MCP_FUNC_END();

    return CCM_IM_STATUS_SUCCESS;
}

CcmImStatus CCM_IM_StackOn(CCM_IM_StackHandle stackHandle)
{
    MCP_FUNC_START("CCM_IM_StackOn");

    MCP_FUNC_END();

    return CCM_IM_STATUS_SUCCESS;
}

CcmImStatus CCM_IM_StackOnAbort(CCM_IM_StackHandle stackHandle)
{
    CcmImStatus     status;
    _CcmImStackData     *stackData;
    
    MCP_FUNC_START("CCM_IM_StackOnAbort");

    MCP_FUNC_END();

    return CCM_IM_STATUS_SUCCESS;
}

CcmImStatus CCM_IM_StackOff(CCM_IM_StackHandle stackHandle)
{
    CcmImStatus     status;
    _CcmImStackData     *stackData;
    
    MCP_FUNC_START("CCM_IM_StackOff");

    MCP_FUNC_END();

    return CCM_IM_STATUS_SUCCESS;
}

CcmImObj *CCM_IM_GetImObj(CCM_IM_StackHandle stackHandle)
{
    return 0xdeadbeef;
}

CcmImStackState CCM_IM_GetStackState(CCM_IM_StackHandle *stackHandle)
{ 
    MCP_FUNC_START("CCM_IM_GetStackState");
    
    MCP_FUNC_END();

    return CCM_IM_STACK_STATE_ON;
}

void CCM_IM_GetChipVersion(CCM_IM_StackHandle *stackHandle,
                           McpU16 *projectType,
                           McpU16 *versionMajor,
                           McpU16 *versionMinor)
{
    MCP_FUNC_START("CCM_IM_GetChipVersion");
  
    MCP_FUNC_END();
}

void _CCM_IM_StaticInitStackSm(void)
{
}

void _CCM_IM_InitObjData(CcmImObj *thisObj, McpHalChipId chipId)
{
}

void _CCM_IM_InitStackData(_CcmImStackData *stackData)
{   
}

CcmImStatus _CCM_IM_HandleStackClientCall(_CcmImStackData *stackData, _CcmIm_StackEvent stackEvent)
{ 
    MCP_FUNC_START("_CCM_IM_HandleStackClientCall");
   
    MCP_FUNC_END();

    return CCM_IM_STATUS_SUCCESS;
}

_CcmImStatus _CCM_IM_StackSmHandleEvent(_CcmImStackData *stackData, _CcmIm_StackEvent stackEvent)
{   
    MCP_FUNC_START("_CCM_IM_StackSmHandleEvent");
    
    MCP_FUNC_END();
   
    return _CCM_IM_STATUS_SUCCESS;
}

_CcmImStatus _CCM_IM_StackSmHandler_On(_CcmImStackData *stackData)
{
    MCP_FUNC_START("_CCM_IM_StackSmHandler_On");
        
    MCP_FUNC_END();
    
    return _CCM_IM_STATUS_SUCCESS;
}

_CcmImStatus _CCM_IM_StackSmHandler_OnAbort(_CcmImStackData *stackData)
{
    MCP_FUNC_START("_CCM_IM_StackSmHandler_OnAbort");
    
    MCP_FUNC_END();
    
    return _CCM_IM_STATUS_SUCCESS;
}

_CcmImStatus _CCM_IM_StackSmHandler_Off(_CcmImStackData *stackData)
{
    MCP_FUNC_START("_CCM_IM_StackSmHandler_Off");
        
    MCP_FUNC_END();
    
    return _CCM_IM_STATUS_SUCCESS;
}

_CcmImStatus _CCM_IM_StackSmHandler_OnComplete(_CcmImStackData *stackData)
{
    MCP_FUNC_START("_CCM_IM_StackSmHandler_OnComplete");
   
    MCP_FUNC_END();

    return _CCM_IM_STATUS_SUCCESS;
}

_CcmImStatus _CCM_IM_StackSmHandler_OnFailed(_CcmImStackData *stackData)
{
    MCP_FUNC_START("_CCM_IM_StackSmHandler_OnFailed");
   
    MCP_FUNC_END();

    return _CCM_IM_STATUS_SUCCESS;
}

_CcmImStatus _CCM_IM_StackSmHandler_OnAbortComplete(_CcmImStackData *stackData)
{
    MCP_FUNC_START("_CCM_IM_StackSmHandler_OnAbortComplete");

    MCP_FUNC_END();

    return _CCM_IM_STATUS_SUCCESS;
}

_CcmImStatus _CCM_IM_StackSmHandler_OffComplete(_CcmImStackData *stackData)
{
    MCP_FUNC_START("_CCM_IM_StackSmHandler_OffComplete");

    MCP_FUNC_END();

    return _CCM_IM_STATUS_SUCCESS;
}


_CcmImStatus _CCM_IM_StackSmHandler_OffFailed(_CcmImStackData *stackData)
{
    MCP_FUNC_START("_CCM_IM_StackSmHandler_OffFailed");

    MCP_FUNC_END();

    return _CCM_IM_STATUS_SUCCESS;
}

_CcmImStatus _CCM_IM_NotifyStack(   _CcmImStackData *stackData, 
                                        CcmImEventType  eventType, 
                                        CcmImStatus         completionStatus)
{
    MCP_FUNC_START("_CCM_IM_NotifyStack");

    MCP_FUNC_END();

    return _CCM_IM_STATUS_SUCCESS;
}

void _CCM_IM_BtTranMngrCb(_CcmIm_BtTranMngr_CompletionEvent *event)
{
    MCP_FUNC_START("_CCM_IM_BtTranMngrCb");

    MCP_FUNC_END();
}

void _CCM_IM_GenerateMutexName(McpHalChipId chipId, char *mutexName)
{
    MCP_HAL_STRING_Sprintf(mutexName, "%s%d", CCM_IM_CONFIG_MUTEX_NAME, chipId);
}

McpHalCoreId _CCM_IM_StackId2CoreId(CcmImStackId stackId)
{
    McpHalCoreId    coreId;
    
    MCP_FUNC_START("_CCM_IM_StackId2CoreId");
    
    switch (stackId)
    {
        case CCM_IM_STACK_ID_BT:                
            coreId = MCP_HAL_CORE_ID_BT;
        break;
        
        case CCM_IM_STACK_ID_FM:    
            coreId = MCP_HAL_CORE_ID_FM;
        break;
        
        case CCM_IM_STACK_ID_GPS:
            coreId = MCP_HAL_CORE_ID_GPS;
        break;
        
        default:
            MCP_FATAL_SET_RETVAR((coreId =MCP_HAL_INVALID_CORE_ID), ("_CCM_IM_StackId2CoreId: Invalid stackId (%d)", stackId));
    }

    MCP_FUNC_END();

    return coreId;
}

McpBool _CCM_IM_IsStatusExternal(_CcmImStatus status)
{
    if (status < _CCM_IM_FIRST_INTERNAL_STATUS)
    {
        return MCP_TRUE;
    }
    else
    {
        return MCP_FALSE;
    }
}

const char *_CCM_IM_DebugStackIdStr(CcmImStackState stackId)
{
    switch (stackId)
    {
        case CCM_IM_STACK_ID_BT:    return "BT";
        case CCM_IM_STACK_ID_FM:    return "FM";
        case CCM_IM_STACK_ID_GPS:   return "GPS";
        
        default:                        return "INVALID STACK ID";
    }
}

const char *_CCM_IM_DebugStackStateStr(CcmImStackState stackState)
{
    switch (stackState)
    {
        case CCM_IM_STACK_STATE_OFF:                        return "STACK_STATE_OFF";
        case CCM_IM_STACK_STATE_OFF_IN_PROGRESS:            return "STACK_STATE_OFF_IN_PROGRESS";
        case CCM_IM_STACK_STATE_ON_IN_PROGRESS:             return "STACK_STATE_ON_IN_PROGRESS";
        case CCM_IM_STACK_STATE_ON_ABORT_IN_PROGRESS:   return "STACK_STATE_ON_ABORT_IN_PROGRESS";
        case CCM_IM_STACK_STATE_ON:                         return "STACK_STATE_ON";
        case CCM_IM_STACK_FATAL_ERROR:                      return "STACK_FATAL_ERROR";
        default:                                                return "INVALID STACK STATE";
    }
}

const char *_CCM_IM_DebugStackEventStr(_CcmIm_StackEvent stackEvent)
{
    switch (stackEvent)
    {
        case _CCM_IM_STACK_EVENT_OFF:                           return "STACK_EVENT_OFF";
        case _CCM_IM_STACK_EVENT_ON_ABORT:                  return "STACK_EVENT_ON_ABORT";
        case _CCM_IM_STACK_EVENT_ON:                            return "STACK_EVENT_ON";
        case _CCM_IM_STACK_EVENT_BT_TRAN_OFF_COMPLETE:      return "STACK_EVENT_BT_TRAN_OFF_COMPLETE";
        case _CCM_IM_STACK_EVENT_BT_TRAN_OFF_FAILED:            return "STACK_EVENT_BT_TRAN_OFF_FAILED";
        case _CCM_IM_STACK_EVENT_BT_TRAN_ON_COMPLETE:       return "STACK_EVENT_BT_TRAN_ON_COMPLETE";
        case _CCM_IM_STACK_EVENT_BT_TRAN_ON_FAILED:             return "STACK_EVENT_BT_TRAN_ON_FAILED";
        case _CCM_IM_STACK_EVENT_BT_TRAN_ON_ABORT_COMPLETE:     return "STACK_EVENT_BT_TRAN_ON_ABORT_COMPLETE";
        default:                                                    return "INVALID STACK EVENT";
    }
}


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


#include <assert.h>
#include "ccm_imi.h"

CcmImObj *imObj = NULL;

void VerifyStackState(CcmImStackId stackId, CcmImStackState expectedState)
{
    CcmImStackState     actualStackState;

    actualStackState = CCM_IM_GetStackState(imObj, stackId);
    
    assert(actualStackState == expectedState);
}

void CcmImBtCb(CcmImEvent *event)
{
    int x;

    x++;
}

void main()
{
    CcmImStatus imStatus;
    CcmImObj    *imObj = NULL;
    
    imStatus = CCM_IM_StaticInit();
    assert(imStatus == CCM_IM_STATUS_SUCCESS);

    imStatus = CCM_IM_Create(MCP_HAL_CHIP_ID_0, &imObj, NULL, NULL);
    assert(imStatus == CCM_IM_STATUS_SUCCESS);

    imStatus = CCM_IM_RegisterStack(imObj, CCM_IM_STACK_ID_BT, CcmImBtCb);
    assert(imStatus == CCM_IM_STATUS_SUCCESS);
    
    VerifyStackState(CCM_IM_STACK_ID_BT, CCM_IM_STACK_STATE_OFF);

    imStatus = CCM_IM_BtOn(imObj);
    assert(imStatus == CCM_IM_STATUS_SUCCESS);

    VerifyStackState(CCM_IM_STACK_ID_BT, CCM_IM_STACK_STATE_ON);

    imStatus = CCM_IM_BtOnAbort(imObj);
    assert(imStatus == CCM_IM_STATUS_IMPROPER_STATE);

    VerifyStackState(CCM_IM_STACK_ID_BT, CCM_IM_STACK_STATE_ON);

    imStatus = CCM_IM_BtOff(imObj);
    assert(imStatus == CCM_IM_STATUS_SUCCESS);

    VerifyStackState(CCM_IM_STACK_ID_BT, CCM_IM_STACK_STATE_OFF);

    imStatus = CCM_IM_DeregisterStack(imObj, CCM_IM_STACK_ID_BT);
    assert(imStatus == CCM_IM_STATUS_SUCCESS);

    imStatus = CCM_IM_Destroy(&imObj);
    assert(imStatus == CCM_IM_STATUS_SUCCESS);
}


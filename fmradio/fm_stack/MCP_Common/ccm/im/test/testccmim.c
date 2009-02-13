/******************************************************************************\
##                                                                            *
## Unpublished Proprietary and Confidential Information of Texas Instruments  *
## Israel Ltd. Do Not Disclose.                                               *
## Copyright 2008 Texas Instruments Israel Ltd.                               *
## All rights reserved. All unpublished rights reserved.                      *
##                                                                            *
## No part of this work may be used or reproduced in any form or by any       *
## means, or stored in a database or retrieval system, without prior written  *
## permission of Texas Instruments Israel Ltd. or its parent company Texas    *
## Instruments Incorporated.                                                  *
## Use of this work is subject to a license from Texas Instruments Israel     *
## Ltd. or its parent company Texas Instruments Incorporated.                 *
##                                                                            *
## This work contains Texas Instruments Israel Ltd. confidential and          *
## proprietary information which is protected by copyright, trade secret,     *
## trademark and other intellectual property rights.                          *
##                                                                            *
## The United States, Israel  and other countries maintain controls on the    *
## export and/or import of cryptographic items and technology. Unless prior   *
## authorization is obtained from the U.S. Department of Commerce and the     *
## Israeli Government, you shall not export, reexport, or release, directly   *
## or indirectly, any technology, software, or software source code received  *
## from Texas Instruments Incorporated (TI) or Texas Instruments Israel,      *
## or export, directly or indirectly, any direct product of such technology,  *
## software, or software source code to any destination or country to which   *
## the export, reexport or release of the technology, software, software      *
## source code, or direct product is prohibited by the EAR. The subject items *
## are classified as encryption items under Part 740.17 of the Commerce       *
## Control List (“CCL”). The assurances provided for herein are furnished in  *
## compliance with the specific encryption controls set forth in Part 740.17  *
## of the EAR -Encryption Commodities and Software (ENC).                     *
##                                                                            *
## NOTE: THE TRANSFER OF THE TECHNICAL INFORMATION IS BEING MADE UNDER AN     *
## EXPORT LICENSE ISSUED BY THE ISRAELI GOVERNMENT AND THE APPLICABLE EXPORT  *
## LICENSE DOES NOT ALLOW THE TECHNICAL INFORMATION TO BE USED FOR THE        *
## MODIFICATION OF THE BT ENCRYPTION OR THE DEVELOPMENT OF ANY NEW ENCRYPTION.*
## UNDER THE ISRAELI GOVERNMENT'S EXPORT LICENSE, THE INFORMATION CAN BE USED *
## FOR THE INTERNAL DESIGN AND MANUFACTURE OF TI PRODUCTS THAT WILL CONTAIN   *
## THE BT IC.                                                                 *
##                                                                            *
\******************************************************************************/

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


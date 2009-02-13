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
/*******************************************************************************\
*
*   FILE NAME:      bthal_mc.c
*
*   DESCRIPTION:    Implementation of the BTHAL MC Module.
*
*   AUTHOR:         Itay Klein
*
\*******************************************************************************/

/********************************************************************************/

#include "btl_config.h"
#include "bthal_mc.h"
#include "osapi.h"

#if BTL_CONFIG_VG == BTL_CONFIG_ENABLED

/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/


BthalStatus BTHAL_MC_Init(BthalCallBack callback)
{
    UNUSED_PARAMETER(callback);

    return BTHAL_STATUS_SUCCESS;
}


BthalStatus BTHAL_MC_Deinit(void)
{
    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_Register(BthalMcCallback callback,
                              void *userData,
                              BthalMcContext **context)
{
    UNUSED_PARAMETER(callback);
    UNUSED_PARAMETER(userData);
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_Unregister(BthalMcContext **context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_AnswerCall(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);
    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_DialNumber(BthalMcContext *context,
                                const BTHAL_U8 *number,
                                BTHAL_U8 length)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(number);
    UNUSED_PARAMETER(length);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_DialMemory(BthalMcContext *context,
                                const BTHAL_U8 *entry,
                                BTHAL_U8 length)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(entry);
    UNUSED_PARAMETER(length);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_HangupCall(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_GenerateDTMF(BthalMcContext *context,
                                  BTHAL_I32 dtmf)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(dtmf);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_DialLastNumber(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_HandleCallHoldAndMultiparty(BthalMcContext *context,
                                                 const HfgHold *holdOp)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(holdOp);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_SetNetworkOperatorStringFormat(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_SetExtendedErrors(BthalMcContext *context,
                                       BTHAL_BOOL enabled)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(enabled);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_SetClipNotification(BthalMcContext *context,
                                         BTHAL_BOOL enabled)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(enabled);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_SetIndicatorEventsReporting(BthalMcContext *context,
                                                 BTHAL_BOOL enabled)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(enabled);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_RequestCallHoldAndMultipartyOptions(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_RequestCurrentIndicatorsValue(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_RequestSupportedIndicatorsRange(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_RequestCurrentCallsList(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_RequestNetworkOperatorString(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_RequestSubscriberNumberInformation(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_ScheduleRingClipEvent(void)
{
    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_RequestResponseAndHoldStatus(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_SetResponseAndHold(BthalMcContext *context,
                                           HfgResponseHold option)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(option);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_SetPhonebook(BthalMcContext *context,
                                        AtPbStorageType phonebook)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(phonebook);

    return BTHAL_STATUS_SUCCESS;
}
                                    

BthalStatus BTHAL_MC_RequestSelectedPhonebook(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);
    
    return BTHAL_STATUS_SUCCESS;
}
    

BthalStatus BTHAL_MC_RequestSupportedPhonebooks(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);
    
    return BTHAL_STATUS_SUCCESS;
}
    

BthalStatus BTHAL_MC_ReadPhonebook(BthalMcContext *context,
                                            BTHAL_U16 index1,
                                            BTHAL_U16 index2)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(index1);
    UNUSED_PARAMETER(index2);
    
    return BTHAL_STATUS_SUCCESS;
}
                                            

BthalStatus BTHAL_MC_RequestPhonebookSupportedIndices(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);
    
    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_MC_FindPhonebook(BthalMcContext *context,
                                            const char *findText)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(findText);
    
    return BTHAL_STATUS_SUCCESS;
}                                           

BthalStatus BTHAL_MC_SetCharSet(BthalMcContext *context,
                            const char *charSet)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(charSet);
    
    return BTHAL_STATUS_SUCCESS;
}                                           
                            

BthalStatus BTHAL_MC_RequestSelectedCharSet(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);
    
    return BTHAL_STATUS_SUCCESS;
}                                           
    

BthalStatus BTHAL_MC_RequestSupportedCharSets(BthalMcContext *context)
{
    UNUSED_PARAMETER(context);
    
    return BTHAL_STATUS_SUCCESS;
}                                           
    

#else /*BTL_CONFIG_VG == BTL_CONFIG_ENABLED*/


BthalStatus BTHAL_MC_Init(BthalCallBack callback)
{
    UNUSED_PARAMETER(callback);

    Report(("BTHAL_MC_Init()  -  BTL_CONFIG_VG Disabled "));

    return BTHAL_STATUS_SUCCESS;
}


BthalStatus BTHAL_MC_Deinit(void)
{

    Report(("BTHAL_MC_Deinit()  -  BTL_CONFIG_VG Disabled "));

    return BTHAL_STATUS_SUCCESS;
}


#endif /*BTL_CONFIG_VG == BTL_CONFIG_ENABLED*/







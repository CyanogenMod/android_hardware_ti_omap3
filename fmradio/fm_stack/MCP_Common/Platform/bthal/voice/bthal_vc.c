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
*   FILE NAME:      bthal_vc.c
*
*   DESCRIPTION:    This file defines the implementation of the BTHAL Modem Control.
*
*   AUTHOR:         Udi Ron
*
\*******************************************************************************/



#include "btl_config.h"
#include "bthal_mc.h"
#include "osapi.h"
#include "bthal_log.h"

#if BTL_CONFIG_VG == BTL_CONFIG_ENABLED

/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/

#include "bthal_vc.h"
#include "utils.h"

/********************************************************************************
 *
 * Function Reference
 *
 *******************************************************************************/

BthalStatus BTHAL_VC_Init(BthalCallBack callback)
{
    UNUSED_PARAMETER(callback);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_VC_Deinit(void)
{
    return BTHAL_STATUS_SUCCESS;
}


BthalStatus BTHAL_VC_Register(BthalVcCallback callback,void *userData, BthalVcContext **context)
{
    UNUSED_PARAMETER(callback);
    UNUSED_PARAMETER(userData);
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_VC_Unregister(BthalVcContext **context)
{
    UNUSED_PARAMETER(context);

    return BTHAL_STATUS_SUCCESS;
}

#if BTL_CONFIG_VG == BTL_CONFIG_ENABLED

BthalStatus BTHAL_VC_SetVoicePath(BthalVcContext *context,
                                  BthalVcAudioSource source,
                                  BthalVcAudioPath path)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(source);
    UNUSED_PARAMETER(path);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_VC_SetVoiceRecognition(BthalVcContext *context,
                                         BTHAL_BOOL enable)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(enable);

    return BTHAL_STATUS_SUCCESS;
}

BthalStatus BTHAL_VC_SetNoiseReductionAndEchoCancelling(BthalVcContext *context,
                                                        BTHAL_BOOL enable)
{
    UNUSED_PARAMETER(context);
    UNUSED_PARAMETER(enable);

    return BTHAL_STATUS_SUCCESS;
}

#endif /* BTL_CONFIG_VG == BTL_CONFIG_ENABLED */


#else /*BTL_CONFIG_VG == BTL_CONFIG_ENABLED*/


BthalStatus BTHAL_VC_Init(BthalCallBack callback)
{
    UNUSED_PARAMETER(callback);

    Report(("BTHAL_VC_Init()  -  BTL_CONFIG_VG Disabled "));

    return BTHAL_STATUS_SUCCESS;
}


BthalStatus BTHAL_VC_Deinit(void)
{

    Report(("BTHAL_VC_Init()  -  BTL_CONFIG_VG Disabled "));

    return BTHAL_STATUS_SUCCESS;
}


#endif /*BTL_CONFIG_VG == BTL_CONFIG_ENABLED*/



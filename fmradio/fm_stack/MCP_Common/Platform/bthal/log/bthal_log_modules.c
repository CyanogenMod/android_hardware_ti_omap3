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
*   FILE NAME:      bthal_log.c
*
*   DESCRIPTION:    This file implements the API of the BTHAL log utilities.
*
*   AUTHOR:         Keren Ferdman
*
\*******************************************************************************/

#include "bthal_common.h"
#include "bthal_log_modules.h"

/****************************************************************************
 *
 * Public Functions
 *
 ***************************************************************************/

/*-------------------------------------------------------------------------------
 * ModuleTypeToModuleName()
 *
 *      return a sting represent the name of the module given as input
 *
 * Type:
 *      Synchronous 
 *
 * Parameters:
 *
 *      moduleType [in]- module type enum
 *
 * Returns:
 *     Returns pointer to string contains the module name
 *
 */
const char *BTHAL_LOG_ModuleTypeToModuleName(BthalLogModuleType moduleType)
{
    switch(moduleType)
    {       
        case (BTHAL_LOG_MODULE_TYPE_BMG):
            return ("BTL_BMG");
        
        case (BTHAL_LOG_MODULE_TYPE_SPP):
            return ("BTL_SPP");

        case (BTHAL_LOG_MODULE_TYPE_OPPC):
            return ("BTL_OPPC");

        case (BTHAL_LOG_MODULE_TYPE_OPPS):
            return ("BTL_OPPS");

        case (BTHAL_LOG_MODULE_TYPE_BPPSND):
            return ("BTL_BPPSND");

        case (BTHAL_LOG_MODULE_TYPE_PBAPS):
            return ("BTL_PBAPS");

        case (BTHAL_LOG_MODULE_TYPE_PAN):
            return ("BTL_PAN");

        case (BTHAL_LOG_MODULE_TYPE_AVRCPTG):
            return ("BTL_AVRCPTG");

        case (BTHAL_LOG_MODULE_TYPE_FTPS):
            return ("BTL_FTPS");

        case (BTHAL_LOG_MODULE_TYPE_FTPC):
            return ("BTL_FTPC");

        case (BTHAL_LOG_MODULE_TYPE_VG):
            return ("BTL_VG");

        case (BTHAL_LOG_MODULE_TYPE_AG):
            return ("BTL_AG");

        case (BTHAL_LOG_MODULE_TYPE_RFCOMM):
            return ("BTL_RFCOMM");

        case (BTHAL_LOG_MODULE_TYPE_A2DP):
            return ("BTL_A2DP");

        case (BTHAL_LOG_MODULE_TYPE_HID):
            return ("BTL_HID");

        case (BTHAL_LOG_MODULE_TYPE_MDG):
            return ("BTL_MDG");

        case (BTHAL_LOG_MODULE_TYPE_BIPINT):
            return ("BTL_BIPINT");

        case (BTHAL_LOG_MODULE_TYPE_BIPRSP):
            return ("BTL_BIPRSP");

        case(BTHAL_LOG_MODULE_TYPE_SAPS):
            return ("BTL_SAPS");

        case (BTHAL_LOG_MODULE_TYPE_COMMON):
            return ("BTL_COMMON");

        case (BTHAL_LOG_MODULE_TYPE_L2CAP):
            return ("BTL_L2CAP");

        case (BTHAL_LOG_MODULE_TYPE_OPP):
            return ("BTL_OPP");

        case (BTHAL_LOG_MODULE_TYPE_FTP):
            return ("BTL_FTP");

        case (BTHAL_LOG_MODULE_TYPE_PM):
            return ("PM");

        case (BTHAL_LOG_MODULE_TYPE_BTDRV):
            return ("BTDRV");

        case (BTHAL_LOG_MODULE_TYPE_UNICODE):
            return ("UNICODE");

        case (BTHAL_LOG_MODULE_TYPE_BSC):
            return ("BSC");

        case (BTHAL_LOG_MODULE_TYPE_BTSTACK):
            return ("BTSTACK");

        case (BTHAL_LOG_MODULE_TYPE_FMSTACK):
            return ("FMSTACK");

        case (BTHAL_LOG_MODULE_TYPE_FS):
            return ("FS");

        case (BTHAL_LOG_MODULE_TYPE_MODEM):
            return ("MODEM");

        case (BTHAL_LOG_MODULE_TYPE_OS):
            return ("OS");

        default:
            return ("UNKNOWN BTL MODULE TYPE");
    }

    return ("UNKNOWN BTL MODULE TYPE");
}


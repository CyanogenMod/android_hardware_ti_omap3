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
*   FILE NAME:      bthal_log_modules.h
*
*   BRIEF:          This file defines the module types used in BTHAL log.
*
*   DESCRIPTION:    This file defines the module types which can send trace 
*					messages via the MACROs defined in bthal_log.h file.
*
*   AUTHOR:         Keren Ferdman 
*
\*******************************************************************************/


#ifndef __BTHAL_LOG_MODULES_H
#define __BTHAL_LOG_MODULES_H


/********************************************************************************
 *
 * Types
 *
 *******************************************************************************/


/*---------------------------------------------------------------------------
 * BthalLogModuleType type
 *
 *	   Represents different modules, which can output traces.
 */
typedef BTHAL_U8 BthalLogModuleType;

#define BTHAL_LOG_MODULE_TYPE_BMG			(0)
#define BTHAL_LOG_MODULE_TYPE_SPP			(1)
#define BTHAL_LOG_MODULE_TYPE_OPPC			(2)
#define BTHAL_LOG_MODULE_TYPE_OPPS			(3)
#define BTHAL_LOG_MODULE_TYPE_BPPSND		(4)
#define BTHAL_LOG_MODULE_TYPE_PBAPS			(5)
#define BTHAL_LOG_MODULE_TYPE_PAN			(6)
#define BTHAL_LOG_MODULE_TYPE_AVRCPTG		(7)
#define BTHAL_LOG_MODULE_TYPE_FTPS			(8)
#define BTHAL_LOG_MODULE_TYPE_FTPC			(9)
#define BTHAL_LOG_MODULE_TYPE_VG			(10)
#define BTHAL_LOG_MODULE_TYPE_AG			(11)
#define BTHAL_LOG_MODULE_TYPE_RFCOMM		(12)
#define BTHAL_LOG_MODULE_TYPE_A2DP			(13)
#define BTHAL_LOG_MODULE_TYPE_HID			(14)
#define BTHAL_LOG_MODULE_TYPE_MDG			(15)
#define BTHAL_LOG_MODULE_TYPE_BIPINT		(16)
#define BTHAL_LOG_MODULE_TYPE_BIPRSP		(17)
#define BTHAL_LOG_MODULE_TYPE_SAPS			(18)
#define BTHAL_LOG_MODULE_TYPE_COMMON		(19)
#define BTHAL_LOG_MODULE_TYPE_L2CAP			(20)
#define BTHAL_LOG_MODULE_TYPE_OPP			(21)
#define BTHAL_LOG_MODULE_TYPE_FTP			(22)
#define BTHAL_LOG_MODULE_TYPE_PM			(23)
#define BTHAL_LOG_MODULE_TYPE_BTDRV			(24)
#define BTHAL_LOG_MODULE_TYPE_UNICODE		(25)
#define BTHAL_LOG_MODULE_TYPE_BSC			(26)
#define BTHAL_LOG_MODULE_TYPE_BTSTACK		(27)
#define BTHAL_LOG_MODULE_TYPE_FMSTACK		(28)
#define BTHAL_LOG_MODULE_TYPE_FS			(29)
#define BTHAL_LOG_MODULE_TYPE_MODEM			(30)
#define BTHAL_LOG_MODULE_TYPE_OS			(31)
#define BTHAL_LOG_MODULE_TYPE_SCRIPT_PROCESSOR	(32)

const char *BTHAL_LOG_ModuleTypeToModuleName(BthalLogModuleType moduleType);

#endif /* __BTHAL_LOG_MODULES_H */



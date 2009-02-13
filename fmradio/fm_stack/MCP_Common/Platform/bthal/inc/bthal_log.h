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
*   FILE NAME:      bthal_log.h
*
*   BRIEF:          This file defines the API of the BTHAL log MACROs.
*
*   DESCRIPTION:    The BTHAL LOG API implements platform dependent logging
*                   MACROs which should be used for logging messages by 
*					different layers, such as the transport, stack, BTL and BTHAL.
*					The following 5 Macros mast be implemented according to the platform:
*					BTHAL_LOG_DEBUG
*					BTHAL_LOG_INFO
*					BTHAL_LOG_ERROR
*					BTHAL_LOG_FATAL
*					BTHAL_LOG_FUNCTION
*
*   AUTHOR:         Keren Ferdman 
*
\*******************************************************************************/


#ifndef __BTHAL_LOG_H
#define __BTHAL_LOG_H


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "mcp_hal_log.h"
#include "bthal_log_modules.h"

/*-------------------------------------------------------------------------------
 * BTHAL_LOG_DEBUG
 *
 *     	Defines trace message in debug level, which should not be used in release build.
 *		This MACRO is not used when EBTIPS_RELEASE is enabled.
 */
#define BTHAL_LOG_DEBUG(file, line, moduleType, msg)		\
		MCP_HAL_LOG_DEBUG(file, line, BTHAL_LOG_ModuleTypeToModuleName(moduleType), msg)


/*-------------------------------------------------------------------------------
 * BTHAL_LOG_INFO
 *
 *     	Defines trace message in info level.
 */
#define BTHAL_LOG_INFO(file, line, moduleType, msg)		\
		MCP_HAL_LOG_INFO(file, line, BTHAL_LOG_ModuleTypeToModuleName(moduleType), msg)


/*-------------------------------------------------------------------------------
 * BTHAL_LOG_ERROR
 *
 *     	Defines trace message in error level.
 */
#define BTHAL_LOG_ERROR(file, line, moduleType, msg)		\
		MCP_HAL_LOG_ERROR(file, line, BTHAL_LOG_ModuleTypeToModuleName(moduleType), msg)


/*-------------------------------------------------------------------------------
 * BTHAL_LOG_FATAL
 *
 *     	Defines trace message in fatal level.
 */
#define BTHAL_LOG_FATAL(file, line, moduleType, msg)		\
		MCP_HAL_LOG_FATAL(file, line, BTHAL_LOG_ModuleTypeToModuleName(moduleType), msg)


/*-------------------------------------------------------------------------------
 * BTHAL_LOG_FUNCTION
 *
 *     	Defines trace message in function level, meaning it is used when entering
 *		and exiting a function.
 *		It should not be used in release build.
 *		This MACRO is not used when EBTIPS_RELEASE is enabled.
 */
#define BTHAL_LOG_FUNCTION(file, line, moduleType, msg)		\
		MCP_HAL_LOG_FUNCTION(file, line, BTHAL_LOG_ModuleTypeToModuleName(moduleType), msg)


#endif /* __BTHAL_LOG_H */



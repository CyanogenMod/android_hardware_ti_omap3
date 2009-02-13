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
*   FILE NAME:      bthal_common.h
*
*   BRIEF:    		This file defines the common types, defines, and prototypes
*					for the BTHAL component.
*
*   DESCRIPTION:    General
*   
*					The file holds common types , defines and prototypes, 
*					used by the BTHAL layer.
*
*   AUTHOR:         V. Abram
*
\*******************************************************************************/

#ifndef __BTHAL_COMMON_H
#define __BTHAL_COMMON_H


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "bthal_config.h"

/********************************************************************************
 *
 * Types
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BthalStatus type
 *
 *     Defines status of BTHAL operation.
 */
typedef unsigned char BthalStatus;

#define BTHAL_STATUS_SUCCESS           (0)
#define BTHAL_STATUS_FAILED            (1)
#define BTHAL_STATUS_PENDING           (2)
#define BTHAL_STATUS_BUSY              (3)
#define BTHAL_STATUS_NO_RESOURCES      (4)
#define BTHAL_STATUS_TIMEOUT           (8)
#define BTHAL_STATUS_INVALID_PARM      (10)
#define BTHAL_STATUS_NOT_FOUND         (13)
#define BTHAL_STATUS_NOT_SUPPORTED     (15)
#define BTHAL_STATUS_IN_USE            (16)
#define BTHAL_STATUS_NO_CONNECTION     (17)
#define BTHAL_STATUS_IMPROPER_STATE    (31)


typedef struct _BthalEvent BthalEvent;

/*-------------------------------------------------------------------------------
 * BthalCallBack type
 *
 *     A function of this type is called to indicate Common BTHAL events
 */
typedef void (*BthalCallBack)(const BthalEvent	*event);


/*---------------------------------------------------------------------------
 * BthalEventType type
 *
 *     All indications and confirmations are sent through a callback
 *     function. The event types are defined below.
 */
typedef BTHAL_U8 BthalEventType;

#define BTHAL_EVENT_INIT_COMPLETE			(1)
#define BTHAL_EVENT_DEINIT_COMPLETE		(2)

/*---------------------------------------------------------------------------
 * BthalErrorType type
 */
typedef BTHAL_U8 BthalErrorType;

#define BTHAL_ERROR_TYPE_NONE				(0)
#define BTHAL_ERROR_TYPE_UNSPECIFIED		(1)

/*---------------------------------------------------------------------------
 * BthalModuleType type
 */
typedef BTHAL_U8 BthalModuleType;

#define BTHAL_MODULE_TYPE_OS			(1)
#define BTHAL_MODULE_TYPE_PM			(2)
#define BTHAL_MODULE_TYPE_DRV			(3)
#define BTHAL_MODULE_TYPE_UART		(4)
#define BTHAL_MODULE_TYPE_UTILS		(5)
#define BTHAL_MODULE_TYPE_SPPOS		(6)

/********************************************************************************
 *
 * Data Structures
 *
 *******************************************************************************/

/*---------------------------------------------------------------------------
 * BthalEvent structure
 * 
 *     This structure is sent to the callback specified for BTHAL_Init. It contains
 *	BTHAL events and associated information elements.
 */
struct _BthalEvent
{
	BthalModuleType		module;
	BthalEventType		type;
	BthalErrorType		errCode;
};

/********************************************************************************
 *
 * Function declarations
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BTHAL_StatusName()
 *
 * Brief:  
 *      Get the status name.
 *
 * Description:
 *      The function returns the status string name.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		status - BTHAL status.
 *
 * Returns:
 *		char * - String with the status name
  */
const char *BTHAL_StatusName(int status);


#endif /* __BTHAL_COMMON_H */



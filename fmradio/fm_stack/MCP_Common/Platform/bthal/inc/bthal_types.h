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
*   FILE NAME:      bthal_types.h
*
*   BRIEF:    		Definitions for basic BTHAL Types.
*
*   DESCRIPTION:	This file defines the BASIC bthal types. These would be used
*					as base types for upper layers (such as ESI stack etc.)
*
*   AUTHOR:         Udi Ron
*
\*******************************************************************************/

#ifndef __BTHAL_TYPES_H
#define __BTHAL_TYPES_H

#include "mcp_hal_types.h"

typedef McpU32 	BTHAL_U32;
typedef McpU16	BTHAL_U16;
typedef McpU8	BTHAL_U8;

typedef  McpS32	BTHAL_S32;
typedef  McpS16	BTHAL_S16;
typedef	McpS8	BTHAL_S8;


/* -------------------------------------------------------------
 *			Native Integer Types (# of bits irrelevant)
 */
typedef McpInt		BTHAL_INT;
typedef McpUint	BTHAL_UINT;


/* -------------------------------------------------------------
 *					UTF8,16 types
 */
typedef McpUtf8	BthalUtf8;
typedef McpUtf16	BthalUtf16;
	
/* -------------------------------------------------------------
 *					Types for Performance							
 *																
 *	Variable sized integers. Used to optimize processor efficiency by		
 *  using the most efficient data size for counters, arithmatic, etc.			
 */
typedef unsigned long  BTHAL_I32;

#if MCP_INT_SIZE == 4

typedef unsigned long  BTHAL_I16;
typedef unsigned long  BTHAL_I8;

#elif MCP_INT_SIZE == 2

typedef unsigned short BTHAL_I16;
typedef unsigned short BTHAL_I8;

#elif MCP_INT_SIZE == 1

typedef unsigned short BTHAL_I16;
typedef unsigned char  BTHAL_I8;

#else

#error Unsupported MCP_INT_SIZE Value!

#endif

/* --------------------------------------------------------------
 *					Boolean Definitions							 
 */
typedef McpBool BTHAL_BOOL;

#define BTHAL_TRUE  	MCP_TRUE
#define BTHAL_FALSE MCP_FALSE 

/* --------------------------------------------------------------
 *					Null Definition							 
 */
#ifndef NULL
#define NULL    0
#endif


/* -------------------------------------------------------------
 *					Platform Dependent Definitions							
 *																
* 		For Platforms where there is a conflict between the BT stack definitions 			
*		and platform definitions  the developer can use the following 
*		BTHAL_XATYPES_CONFLICT definition in order to take the definitions from 
*		the platform instead of the BT stack.
*
*		In windows we don't define this definition  since we don't have a conflict.  
* 
*		e.g. in Locosto we add theses two lines:
* 		#include "typedefs.h" 
* 		#define BTHAL_XATYPES_CONFLICT 
 *
 */

#undef BTHAL_XATYPES_CONFLICT  



#endif /* __BTHAL_TYPES_H */


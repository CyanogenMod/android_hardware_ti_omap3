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
*   FILE NAME:      bthal_pragmas.h
*
*   BRIEF:          BTIPS Hardware Adaptation Layer Pragma Directive Definitions.
*
*   DESCRIPTION:    
*
*     The pragma directives in this file configure the BTHAL layer for a specific 
*     platform and project.
*
*     The pragma directives offer a way for each compiler to offer machine- and 
*     operating-system-specific features while retaining overall compatibility with 
*     the C language.
*
*     The pragma directives will place the defined function or variables in an 
*     internal RAM for faster execution.
*
*	The values in this specific file are tailored for a Windows distribution. To add a 
*	pragma directive, simply add it in the corresponding section.
*
*   AUTHOR:         Ronen Levy
*
*****************************************************************************/

#ifndef __BTHAL_PRAGMAS_H
#define __BTHAL_PRAGMAS_H


#ifdef WIN_WARNING_PRAGMAS
/*-------------------------------------------------------------------------------
 * WARNINGS HANDLING
 *
 *	Disable the "Conditional Expression is Constant" warning that is to be ignored
 */
#pragma warning( disable :  4127) 
#undef WIN_WARNING_PRAGMAS
#endif


#ifdef BTL_A2DP_PRAGMAS
/*-------------------------------------------------------------------------------
 * BTL_A2DP.c 
 *
 *     Represents pragma directives in btl_a2dp.c.
 *     These pragmas will place the function in internal RAM for faster execution 
 */
#undef BTL_A2DP_PRAGMAS
#endif


#ifdef UTILS_PRAGMAS
/*-------------------------------------------------------------------------------
 * Utils.c 
 *
 *     Represents pragma directives in utils.c.
 *     These pragmas will place the function in internal RAM for faster execution 
 */
#undef UTILS_PRAGMAS
#endif


#ifdef BTHAL_UART_PRAGMAS
/*-------------------------------------------------------------------------------
 * BTHAL_UART.c 
 *
 *     Represents pragma directives in bthal_uart.c.
 *     These pragmas will place the function in internal RAM for faster execution 
 */
#undef BTHAL_UART_PRAGMAS
#endif

#ifdef BTHAL_OS_PRAGMAS
/*-------------------------------------------------------------------------------
 * BTHAL_OS.c 
 *
 *     Represents pragma directives in bthal_os.c.
 *     These pragmas will place the function in internal RAM for faster execution 
 */
#undef BTHAL_OS_PRAGMAS
#endif

#ifdef BTHAL_UTILS_PRAGMAS
/*-------------------------------------------------------------------------------
 * BTHAL_Utils.c 
 *
 *     Represents pragma directives in bthal_utils.c.
 *     These pragmas will place the function in internal RAM for faster execution 
 */
#undef BTHAL_UTILS_PRAGMAS
#endif


#ifdef UARTTRAN_PRAGMAS
/*-------------------------------------------------------------------------------
 * Uarttran.c 
 *
 *     Represents pragma directives in uarttran.c.
 *     These pragmas will place the function in internal RAM for faster execution 
 */
#undef UARTTRAN_PRAGMAS
#endif


#endif /* __BTHAL_PRAGMAS_H */







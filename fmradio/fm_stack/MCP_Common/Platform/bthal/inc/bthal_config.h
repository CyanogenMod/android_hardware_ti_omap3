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
*   FILE NAME:      bthal_config.h
*
*   BRIEF:          BTIPS Hardware Adaptation Layer Configuration Parameters
*
*   DESCRIPTION:    
*
*     The constants in this file configure the BTHAL layer for a specific platform and project.
*
*     Some constants are numeric, and others indicate whether a feature
*     is enabled (defined as BTL_CONFIG_ENABLED) or disabled (defined as
*     BTL_CONFIG_DISABLED).

*   The values in this specific file are tailored for a Windows distribution. To change a constant, 
*   simply change its value in this file and recompile the entire BTIPS package.
*
*   AUTHOR:         Yaniv Rabin
*
*****************************************************************************/

#ifndef __BTHAL_CONFIG_H
#define __BTHAL_CONFIG_H


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "mcp_hal_types.h"
#include "mcp_hal_config.h"

#include <stdlib.h>
#include "bthal_types.h"
#include "btl_config.h"
#include "EBTIPS_version.h"

/* Comment this line for embedded Neptune & Locosto platforms */
#define WIN_WARNING_PRAGMAS
#include "bthal_pragmas.h"


/********************************************************************************
 *
 * Constants
 *
 *******************************************************************************/



/*-------------------------------------------------------------------------------
 * Common
 *
 *     Represents common configuration parameters.
 */

#define BTHAL_PLATFORM                          PLATFORM_WINDOWS


#define BTHAL_REPORT_STRING_MAX_LENGTH          100


/*-------------------------------------------------------------------------------
 * FS
 *
 *     Represents configuration parameters for FS module.
 */

/*
*   Specific the path of TI init script file name on the OS system.
*   The init script locate on "\EBTIPS\bthal\init script\TIInit_X.X.XX.bts" in the delivery packet.
*/
#define BTHAL_FS_TI_INIT_SCRIPT_PATH                                    (MCP_HAL_CONFIG_FS_SCRIPT_FOLDER)

/*
*   Specifies the FS path of a temporary image file that is created when BIP Responder receives
*   images from a BIP responder.
*/
#define BTHAL_FS_BIPRSP_TEMP_PUSHED_IMAGE_PATH                  ("c:\\images\\bip\\xml\\TempPushedImage.jpg")

/*
 * Folders for temporary OPP files
 */
#define BTHAL_FS_OPPS_PUSH_TEMP_FILE_PATH                       ("c:\\")
#define BTHAL_FS_OPPC_PULL_TEMP_FILE_PATH                       ("c:\\")

/*
    The FFS path in which the Device Database will be saved. 
    The Device Database is used by the Management Entity and by the applicaiton to store link
    keys and other information about peer devices in a non-volatile way. 
*/
#define BTHAL_FS_DDB_FILE_NAME                                          ("C:\\TI\\BtDeviceDb.ddb")

/*
    The test mode init script file name
*/
#define BTHAL_FS_BMG_ENABLE_TEST_MODE_SCRIPT_NAME       ("TIInitRf")

/*
 *  The file name of the CORTEX script (either AVPR or ULP)
 */
#define BTHAL_FS_BMG_CORTEX_SCRIPT_NAME                 ("avpr.bts")

/*
*   The maximum length of a file system path (in bytes !!!)
*   In case of UTF8 this value should be reconsidered (char can ocupate 1-4 bytes)
*/
#define BTHAL_FS_MAX_PATH_LENGTH                                        (MCP_HAL_CONFIG_FS_MAX_PATH_LEN_CHARS)

/*
*   The maximum name of the file name part on the local file system (in bytes!!!)
*   In case of UTF8 this value should be reconsidered (char can ocupate 1-4 bytes)
*/
#define BTHAL_FS_MAX_FILE_NAME_LENGTH                               (MCP_HAL_CONFIG_FS_MAX_FILE_NAME_LEN_CHARS)

/*
*   The folder separator character of files on the file system
*/
#define BTHAL_FS_PATH_DELIMITER                                         (MCP_HAL_CONFIG_FS_PATH_DELIMITER)

/*
*   Status Mask
*/
#define BTHAL_FS_STAT_MASK                                                  (BTHAL_FS_S_ALL_FLAG)

/*
*   define if the file system is case sensitive
*/
#define BTHAL_FS_CASE_SENSITIVE                                         (MCP_HAL_CONFIG_FS_CASE_SENSITIVE)

/*-------------------------------------------------------------------------------
 * OS
 *
 *     Represents configuration parameters for OS module.
 */

/* 
*   The maximum number of stack-related events.
*
*   Currently, the configuration accomodates a total of the following events:
*   1. process event
*   2. timer event
*   3. phonebook task (PBAP profile)
*   4. MDG event
*   5. Init
*   6. Radio Off 
*/
#define BTHAL_OS_MAX_NUM_OF_EVENTS_STACK                        (6)

/*
*   The maximum number of Bluetooth transport-related events.
*
*   Currently the configuration accomodates 1 Rx event and 1 Tx event
*/
#define BTHAL_OS_MAX_NUM_OF_EVENTS_TRANSPORT                    (2)

/* 
*   The maximum number of A2DP-Specific events
*
*   Currently, the configuration accomodates a total of 4:
*   1. 1 data ind event 
*   2. 1 data sent event 
*   3. 1 config ind event 
*   4. 1 timer event
*/
#define BTHAL_OS_MAX_NUM_OF_EVENTS_A2DP                         (4)

/*
 *  The maximum number of tasks used by BT.
 *
 *  Currently: 1 BT stack task, 1 A2DP task, possible UART transport task 
 */
#define BTHAL_OS_MAX_NUM_OF_TASKS                               (3)
                            
/* 1 (stack) + 2 (A2DP) + 1 (MDG) semaphores */
#define BTHAL_OS_MAX_NUM_OF_SEMAPHORES                          (4)

/* 
 *  The maximum number of OS timers used by BTIPS.
 *   
 *	Currently, the configuration accommodates 1 stack timer + 1 A2DP timer +
 *  1 VG handover timer
 */
#define BTHAL_OS_MAX_NUM_OF_TIMERS                              (3)


/*-------------------------------------------------------------------------------
 * MD
 *
 *     Represents configuration parameters for the MD module.
 */

/*
 *  A type of buffer owner for data downloaded from the network:
 *
 *  Case 0: modem has not its own buffers and data is read into caller's buffer.
 *  Case 1: modem supplies downloaded data with its buffer, which should be freed later
 */
#define BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM                   (BTL_CONFIG_ENABLED)

/* 
 *  A type of buffer owner for data to be uploaded to the network:
 *
 *  Case 0: modem does not expose its own buffers and data is written to modem by
 *          means of MD function from application's buffer which should be returned later
 *          with event BTHAL_MD_EVENT_UPLOAD_BUF_FROM_MODEM.
 *
 *  Case 1: modem supplies its buffer to be filled with data to be uploaded to
 *          the network using event BTHAL_MD_EVENT_UPLOAD_BUF_FROM_MODEM
 *          later
 */
#define BTHAL_MD_UPLOAD_BUF_OWNER_MODEM                     (BTL_CONFIG_ENABLED)


/*-------------------------------------------------------------------------------
 * PB
 *
 *     Represents configuration parameters for PB module.
 */

/*
 *  Defines the maximum number of phonebook entries in a single phonebook. 
 */
#define BTHAL_PB_MAX_ENTRIES_NUM                                (300)

/*
 *  Defines the maximum phonebook entry length that is requested in the 
 *  BTHAL_PB_GetEntryData function.
 */
#define BTHAL_PB_MAX_ENTRY_LEN                                      (500)

/*
 *  Maximum number of bytes allowed for entry name + 1 (null-terminating
 *  character). Please note that the entry name may be in Unicode, where a
 *  character may be represented by more than one byte.
 */
#define BTHAL_PB_MAX_ENTRY_NAME                                     (32)


/*-------------------------------------------------------------------------------
 * Secure Simple Pairing (SSP)
 *
 *     Represents configuration parameters for SSP.
 */

/*
 *  Defines the platform's IO capability which will be used for selection of
 *  association model used for authentication during SSP. 
 */
#define BTHAL_CONFIG_SSP_IO_CAPABILITY                              (BTL_CONFIG_SSP_IO_CAPABILITY_DISPLAY_YES_NO)


/********************************************************************************
 *
 * Platform dependent timing macros
 *
 *******************************************************************************/
 
/*
 *  Macro that converts OS ticks to milliseconds
 */
#define BTHAL_OS_TICKS_TO_MS(ticks)                                 (ticks)

/*
 *  Macro that converts milliseconds to OS ticks
 */
#define BTHAL_OS_MS_TO_TICKS(ms)                                    (ms)


#endif /* __BTHAL_CONFIG_H */



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
*   FILE NAME:    bthal_sim.h
*
*   BRIEF:          Type definitions and function prototypes for the
*					bthal_sim.c module implementing the interface to the SIM card
*                   subscruption module. This interface is used by the BTL_SAPS module..
*  
*   DESCRIPTION:    General
*
*                   This BTHAL interface does support an asynchronous  and
*                   synchronous interface to the SIM card. The return value does tell
*                   the caller whether the call was processed synchronous or not:
*
*                   Returning: BTHAL_STATUS_PENDING:
*                   Handling is asynchronous.
*                   The caller will be notified with an event to the callback
*                   (as registered via BTHAL_SIM_Init) when the result of the processing
*                   is ready.
*         
*                   Returning: not BTHAL_STATUS_PENDING:
*                   Handling is synchronous.
*                   If the return = BTHAL_STATUS_SUCCESS, then the processing is
*                   completely finished (including calling the callback)
*
*                   It is easy to see if a function can be asynchronous because you
*                   see that it can return with a BTHAL_STATUS_PENDING status.
*    
*   AUTHOR:     Gerrit Slot
*
\*******************************************************************************/

#ifndef __BTHAL_SIM_H
#define __BTHAL_SIM_H

/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "xatypes.h"
#include "bthal_common.h"
#include "sim.h"

/*-------------------------------------------------------------------------------
 * BthalSimEvent type
 *
 *     Define the events from the BTHAL_SIM
 */
typedef U8 BthalSimEvent;

/* 
 * APDU response data available to read
 *  Read it via BTHAL_SIM_ReadSApduRsp.  
 */
#define BTHAL_SIM_EVENT_APDU_RSP        ((U8) 0)

/* 
 * Status of the SIM card has been changed. 
 *  Read new Status via BTHAL_SIM_GetStatus 
 */
#define BTHAL_SIM_EVENT_STATUS_CHANGED  ((U8) 1)

/********************************************************************************
 *
 * Types
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_CallBack type
 *
 *     A function of this type is called to indicate BTHAL_SIM events.
 */
typedef void (*BTHAL_SIM_CallBack)(BthalSimEvent event, SimResultCode result);

/********************************************************************************
 *
 * Function declarations
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_Init()
 *
 * Brief:
 *    Startup of the communication with the SIM module.
 *
 * Description:
 *		Startup of the communication with the SIM module. 
 *
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *    callback[in] - Callback function for notifying changes in the SIM status.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the initialization was successful.
 *
 *		BTHAL_STATUS_FAILED - if the initialization failed.
 */
BthalStatus BTHAL_SIM_Init(BTHAL_SIM_CallBack callback);

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_Deinit()
 *
 * Brief:
 *    Stop communication with the SIM module.
 *
 * Description:
 *    Stop communication with the SIM module.
 *
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *    None.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - De initialization was successful.
 */
BthalStatus BTHAL_SIM_Deinit(void);

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_Reset()
 *
 * Brief:
 *    Reset the SIM module.
 *
 * Description:
 *    Reset the SIM module.
 *    Result is replied to the caller via the registered callback
 *    This result can be:
 *        SIM_RESULT_OK
 *        SIM_RESULT_NO_REASON
 *        SIM_RESULT_CARD_NOT_ACCESSIBLE
 *        SIM_RESULT_CARD_ALREADY_OFF
 *        SIM_RESULT_CARD_REMOVED
 *
 * Type:
 *		Synchronous/Asynchronous.
 *
 * Parameters:
 *		void.
 *
 * Generated Events:
 *    BTHAL_SIM_EVENT_STATUS_CHANGED
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - successful.
 *
 *    BTHAL_STATUS_PENDING - successful (finishing is published
 *        via BTHAL_SIM_EVENT_STATUS_CHANGED event).
 */
BthalStatus BTHAL_SIM_Reset(void);

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_PowerOn()
 *
 * Brief:
 *    Turn on the SIM module.
 *
 * Description:
 *    Turn on the SIM module.
 *    Result is replied to the caller via the registered callback
 *    This result can be:
 *        SIM_RESULT_OK
 *        SIM_RESULT_NO_REASON
 *        SIM_RESULT_CARD_NOT_ACCESSIBLE
 *        SIM_RESULT_CARD_REMOVED
 *        SIM_RESULT_CARD_ALREADY_ON
 *
 * Type:
 *		Synchronous/Asynchronous.
 *
 * Parameters:
 *		void.
 *
 * Generated Events:
 *    BTHAL_SIM_EVENT_STATUS_CHANGED
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - successful.
 *
 *    BTHAL_STATUS_PENDING - successful (finishing is published
 *        via BTHAL_SIM_EVENT_STATUS_CHANGED event).
 */
BthalStatus BTHAL_SIM_PowerOn(void);

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_PowerOff()
 *
 * Brief:
 *    Turn off the SIM module.
 *
 * Description:
 *    Turn off the SIM module.
 *    Result is replied to the caller via the registered callback
 *    This result can be:
 *        SIM_RESULT_OK
 *        SIM_RESULT_NO_REASON
 *        SIM_RESULT_CARD_ALREADY_OFF
 *        SIM_RESULT_CARD_REMOVED
 *
 * Type:
 *		Synchronous/Asynchronous.
 *
 * Parameters:
 *		void.
 *
 * Generated Events:
 *    BTHAL_SIM_EVENT_STATUS_CHANGED
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - successful.
 *
 *    BTHAL_STATUS_PENDING - successful (finishing is published
 *        via BTHAL_SIM_EVENT_STATUS_CHANGED event).
 */
BthalStatus BTHAL_SIM_PowerOff(void);


/*-------------------------------------------------------------------------------
 * BTHAL_SIM_WriteApduReq()
 *
 * Brief:
 *    Process an APDU data packet.
 *
 * Description:
 *    Process an APDU data packet.
 *    The APDU does contain a GSM 11.11 command for the SIM.
 *    So the SIM card will be requested to handle this GSM 11.11 command.
 *
 *    When it is handled synchronously (return = BTHAL_STATUS_SUCCESS),
 *    the caller can read the response via SIM_ReadApduRsp immediately.
 *
 *    When it is handled asynchronously (return = BTHAL_STATUS_PENDING),
 *    the caller will be triggered via the BTHAL_SIM_EVENT_APDU_RSP event when
 *    the SIM card has the response ready.
 *    Now the caller can call BTHAL_SIM_ReadApduRsp to get the
 *    APDU response that is already stored in BTHAL_SIM.
 *
 *    Result is replied to the caller via the registered callback
 *    This result can be:
 *        SIM_RESULT_OK
 *        SIM_RESULT_NO_REASON
 *        SIM_RESULT_CARD_NOT_ACCESSIBLE
 *        SIM_RESULT_CARD_ALREADY_OFF
 *        SIM_RESULT_CARD_REMOVED
 *
 *    Only in case SIM_RESULT_OK is replied, then a valid RSP is present
 *    to be read via BTHAL_SIM_ReadApduRsp.
 *
 * Type:
 *    Synchronous/Asynchronous
 *
 * Parameters:
 *    len[in] - Length (bytes) of the APDU data (max 263)
 *
 *    data[in] - Pointer to the APDU data.
 *
 * Generated Events:
 *    BTHAL_SIM_EVENT_APDU_RSP
 * 
 * Returns:
 *		BTHAL_STATUS_SUCCESS - successful
 *
 *    BTHAL_STATUS_PENDING - successful (APDU response will be published
 *        via BTHAL_SIM_EVENT_APDU_RSP event).
 *
 *    BTHAL_STATUS_FAILED - 'len' field exceeds maximum of MAX_APDU_SIZE
 */
BthalStatus BTHAL_SIM_WriteApduReq(U16 len,
                                   U8 *data);

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_ReadApduRsp()
 *
 * Brief:
 *    Read the APDU response from the SIM card.
 *
 * Description:
 *    Read the APDU response from the SIM card.
 *    This function is triggered by the BTHAL_SIM_EVENT_APDU_RSP event from the SIM.
 *    This APDU response does contain the response on the previous given
 *    GSM 11.11 command to the SIM.
 *
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *    len[out] - Length of APDU response returned 
 *
 *    apduRsp[in/out] - pointer to the APDU response data, stored in this BTHAL_SIM module
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - successful.
 */
BthalStatus BTHAL_SIM_ReadApduRsp(U16 *len,
                                  U8 **apduRsp);

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_GetStatus()
 *
 * Brief:
 *    Read the actual status of the SIM card.
 *
 * Description:
 *    Read the actual status of the SIM card.
 *    The caller is normally trigger via the BTHAL_SIM_EVENT_STATUS_CHANGED event
 *
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *    cardStatus[out] - SIM card status.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - successful.
 */
BthalStatus BTHAL_SIM_GetStatus(SimCardStatus *cardStatus);

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_GetAtr()
 *
 * Brief:
 *    Get the ATR (Answer To Reset) information from the SIM module.
 *
 * Description:
 *    Get the ATR (Answer To Reset) information from the SIM module.
 *
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *    result[out] - Return the result of this action. It can be:
 *        SIM_RESULT_OK
 *        SIM_RESULT_NO_REASON
 *        SIM_RESULT_CARD_ALREADY_OFF
 *        SIM_RESULT_CARD_REMOVED
 *        SIM_RESULT_DATA_NOT_AVAILABLE
 *
 *    len[out] - Length of ATR data returned (max 33)
 *
 *    atr[in/out] - pointer to the ATR data, stored in this BTHAL_SIM module.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - successful.
 */
BthalStatus BTHAL_SIM_GetAtr(U16 *len,
                             U8 **atr,
                             SimResultCode *result);

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_GetCardReaderStatus()
 *
 * Brief:
 *    Get the status of the CardReader.
 *
 * Description:
 *    Get the status of the CardReader.
 *
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *    result[out] - Return the result of this action. It can be:
 *        SIM_RESULT_OK
 *        SIM_RESULT_NO_REASON
 *        SIM_RESULT_DATA_NOT_AVAILABLE
 *
 *    len[out] - Length of CardReaderStatus returned.
 *
 *    data[in/out] - pointer to data that will be filled with CardReaderStatus data.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - successful.
 */
BthalStatus BTHAL_SIM_GetCardReaderStatus(SimCardReaderStatus *cardReaderStatus,
                                          SimResultCode *result);

#endif /* __BTHAL_SIM_H */

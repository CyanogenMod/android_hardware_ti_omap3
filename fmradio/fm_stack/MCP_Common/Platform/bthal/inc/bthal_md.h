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
*   FILE NAME:		bthal_md.h
*
*   BRIEF:          This file defines the API of the BTHAL Modem Data service.
*
*   DESCRIPTION:	The BTHAL MD provides fixed interface for accessing modem
*                   data path with different options of buffers location for
*                   both downloading and uploading paths - buffers may be
*                   located in the modem's software or in the BTL layer.
*
*					It is assumed that only one BT application may use modem
*					data service via MD interface at a time.
*
*   AUTHOR:			V. Abram
*
\*******************************************************************************/

#ifndef __BTHAL_MD_H
#define __BTHAL_MD_H

 
/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "btl_spp.h"
#include "bthal_types.h"
#include "bthal_common.h"
#include "bthal_config.h"


/********************************************************************************
 *
 * Types
 *
 *******************************************************************************/
 
/*---------------------------------------------------------------------------
 * BthalMdEventType type
 *
 *	   Represents possible types of events which may be received by application
 *	   using modem's data service via MD
 */
typedef BTHAL_U8 BthalMdEventType;

/* 
 * Application has successfully initialized modem to use its data service and
 * initial configuration was written.
 */
#define BTHAL_MD_EVENT_INITIALIZED		(1)

/* Application has successfully registered with the modem. */
#define BTHAL_MD_EVENT_REGISTERED		(2)

/* Application has successfully reconfigured the modem. */
#define BTHAL_MD_EVENT_CONFIGURED		(3)

/* Application has been successfully connected to the modem's data service */
#define BTHAL_MD_EVENT_CONNECTED		(4)

/* Application has been disconnected from the modem's data service */
#define BTHAL_MD_EVENT_DISCONNECTED		(5)

/* Modem has downloaded data from the network */
#define BTHAL_MD_EVENT_DOWNLOAD_DATA	(6)

/* Modem has got new buffer for uploading data to the network */
#define BTHAL_MD_EVENT_UPLOAD_BUF		(7)

/* Modem has downloaded data from the network */
#define BTHAL_MD_EVENT_CONTROL_SIGNALS	(8)


/* Forward declaration */
typedef struct _BthalMdEvent BthalMdEvent;

/*-------------------------------------------------------------------------------
 * BthalMdCallback type
 *
 *      A function of this type is called to indicate MD events.
 */
typedef void (*BthalMdCallback)(const BthalMdEvent *mdEvent);

 
/********************************************************************************
 *
 * Data Structures
 *
 *******************************************************************************/
 
/*-------------------------------------------------------------------------------
 * BthalMdEvent type
 *
 *		Events expected from MD. They need to be passed to the BTL Modem Data
 *		Gateway	module for processing of its state machine.
 */
struct _BthalMdEvent
{
	BthalMdEventType		eventType;			/* Type of event */
	BTHAL_INT				downloadDataLen;	/* Length of downloaded data */
		
};


/********************************************************************************
 *
 * Function declarations
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Init()
 *
 * Brief:  
 *		Initializes modem's data service.
 *
 * Description:
 *	   	Initializes and allocates all resources to be used by the application
 *      with modem's data service.
 *      
 * Type:
 *		Asynchronous or synchronous.
 *
 * Parameters:
 *     	None.
 *
 * Generated Events:
 *      BTHAL_MD_EVENT_INITIALIZED
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the itialization was successful.
 *
 *      BTHAL_STATUS_PENDING - if the initialization has been successfully
 *          started.
 *
 *          Event BTHAL_MD_EVENT_INITIALIZED will be delivered via the callback
 *          function registered with function BTHAL_MD_Init(), when the
 *          initialization is finished.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_Init(BthalCallBack	callback);

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Deinit()
 *
 * Brief:  
 *		Releases modem's data service and resources.
 *
 * Description:
 *	   	All resources, which were used by caller application, will be realeased.
 *      
 * Type:
 *		Asynchronous or synchronous.
 *
 * Parameters:
 *		None.
 *
 * Generated Events:
 *      BTHAL_MD_EVENT_DEINITIALIZED
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the deitialization was successful.
 *
 *		BTHAL_STATUS_PENDING - if the deinitialization has been successfully
 *          started.
 *
 *          Event BTHAL_MD_EVENT_DEINITIALIZED will be delivered via the callback
 *          function registered with function BTHAL_MD_Init(), when the
 *          deinitialization is finished.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_Deinit(void);

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Register()
 *
 * Brief:  
 *		Registers with the modem's data service.
 *
 * Description:
 *	   	Stores callback function for delivering modem's events to the caller and
 *      communication settings to be established in serial port.
 *      
 * Type:
 *		Asynchronous or synchronous.
 *
 * Parameters:
 *     	mdCallback [in] - callback funtion for receiving MD events.
 *
 *	 	comSettings [in] - pointer to a structure with communication settings of
 *          the connection to modem.
 *
 * Generated Events:
 *      BTHAL_MD_EVENT_REGISTERED
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the itialization was successful.
 *
 *		BTHAL_STATUS_PENDING - if the operation has been successfully started.
 *
 *			Event BTHAL_MD_EVENT_REGISTERED will be delivered via the callback
 *          function registered with function BTHAL_MD_Init(), when the
 *          registration is finished.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_Register(const BthalMdCallback mdCallback,
						      const SppComSettings *comSettings);

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Deregister()
 *
 * Brief:  
 *		Deregisters from the modem's data service.
 *
 * Description:
 *	   	Clear registration of the callback function and communication settings
 *      the serial port.
 *      
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *     	None.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the itialization was successful.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed.
 */
BthalStatus BTHAL_MD_Deregister(void);

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Configure()
 *
 * Brief:  
 *		Configures the modem's data service.
 *
 * Description:
 *	   	Stores communication settings to be established in serial port.
 *      
 * Type:
 *		Asynchronous or synchronous.
 *
 * Parameters:
 *	 	comSettings [in] - pointer to a structure with communication settings of
 *          the connection to modem.
 *
 * Generated Events:
 *      BTHAL_MD_EVENT_CONFIGURED
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the configuration process was successful.
 *
 *		BTHAL_STATUS_PENDING - if the configuration has been successfully
 *          started.
 *
 *          Event BTHAL_MD_EVENT_CONFIGURED will be delivered via the callback
 *          function registered with function BTHAL_MD_Init(), when the
 *          configuration is finished.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_Configure(const SppComSettings *comSettings);

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Connect()
 *
 * Brief:  
 *		Connects a caller application to modem's data service.
 *
 * Description:
 *	   	Establishes logical connection between the modem's data service and the
 *      application.
 *      
 * Type:
 *		Asynchronous or synchronous.
 *
 * Parameters:
 *     	None.
 *
 * Generated Events:
 *      BTHAL_MD_EVENT_CONNECTED
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the connection to the modem data service was
 *			successful.
 *
 *		BTHAL_STATUS_PENDING - if the operation has been successfully started.
 *
 *			Event BTHAL_MD_EVENT_CONNECTED will be delivered via the callback
 *          function registered with function BTHAL_MD_Init(), when the
 *          connection is established.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_Connect(void);

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Disconnect()
 *
 * Brief:  
 *		Disconnects a caller application from modem's data service.
 *
 * Description:
 *		Disconnects a logical link between the modem's data service and the
 *      application. This may include GPRS detaching.
 *
 * Type:
 *		Asynchronous or synchronous.
 *
 * Parameters:
 *     	None.
 *
 * Generated Events:
 *      BTHAL_MD_EVENT_DISCONNECTED
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the disconnection to the modem data service
 *			was	successful.
 *
 *		BTHAL_STATUS_PENDING - if the operation has been successfully started.
 *
 *			Event BTHAL_MD_EVENT_DISCONNECTED will be delivered via the callback
 *          function registered with function BTHAL_MD_Init(), when the
 *          disconnection is finished.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_Disconnect(void);
       
#if BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM == BTL_CONFIG_ENABLED

/*-------------------------------------------------------------------------------
 * BTHAL_MD_GetDownloadBuf()
 *
 * Brief:  
 *		Gets a buffer with downloaded data.
 *
 * Description:
 *      Gets a pointer to modem's buffer with downloaded data and its length.
 *      Requires compilation switch BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM is enabled.
 *      
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *     	buffer [out] - pointer to buffer containing downloaded data.
 *
 *     	len [out] - pointer to length of downloaded data.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the information was successfully returned.
 *
 *		BTHAL_STATUS_FAILED - if no downloaded data exists.
 */
BthalStatus BTHAL_MD_GetDownloadBuf(BTHAL_U8 **buffer, BTHAL_INT *len);

/*-------------------------------------------------------------------------------
 * BTHAL_MD_ReturnDownloadBuf()
 *
 * Brief:  
 *		Returns a download buffer.
 *
 * Description:
 *      Returns the buffer to modem after downloaded data was copied from it.
 *      Requires compilation switch BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM is enabled.
 *      
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *     	buffer [in] - pointer to buffer to be returned.
 *
 *     	len [in] - length of buffer to be returned.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the buffer was successfully returned.
 *
 *		BTHAL_STATUS_FAILED - if the operation failed.
 */
BthalStatus BTHAL_MD_ReturnDownloadBuf(BTHAL_U8 *buffer, BTHAL_INT len);

#else

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Read()
 *
 * Brief:  
 *		Reads data from modem.
 *
 * Description:
 *		It may be called by application, when either indication is received:
 *			- data is received by the modem,
 *			- SPP buffer is ready in order to write data to SPP port.
 *      Requires compilation switch BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM is disabled.
 *      
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *     	buffer [in] - pointer to buffer in which to receive data,
 *
 *     	maxBytes [in]	- maximum number of bytes to place in buffer,
 *
 *     	readBytes [out] - pointer to amount of actually read bytes.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the read operation was successful.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_Read(BTHAL_U8 *buffer,
						  BTHAL_INT maxBytes,
						  BTHAL_INT *readBytes);

#endif /* #if BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM == BTL_CONFIG_ENABLED */

#if BTHAL_MD_UPLOAD_BUF_OWNER_MODEM == BTL_CONFIG_ENABLED

/*-------------------------------------------------------------------------------
 * BTHAL_MD_GetUploadBuf()
 *
 * Brief:  
 *		Gets buffer for uploading data.
 *
 * Description:
 *      Gets pointer to modem's buffer and its size for data to be uploaded.
 *      Requires compilation switch BTHAL_MD_UPLOAD_BUF_OWNER_MODEM is enabled.
 *      
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *     	size [in] - requested size of the buffer.
 *
 *     	buffer [out] - pointer to buffer.
 *
 *     	len [out] - pointer to length of the buffer.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the information was successfully returned.
 *
 *		BTHAL_STATUS_FAILED - if no downloaded data exists.
 */
BthalStatus BTHAL_MD_GetUploadBuf(BTHAL_INT size, BTHAL_U8 **buffer, BTHAL_INT *len);

/*-------------------------------------------------------------------------------
 * BTHAL_MD_UploadDataReady()
 *
 * Brief:  
 *		Notifies the modem that uploading data is ready.
 *
 * Description:
 *      Notifies that modem's upload buffer is filled with data.
 *      Requires compilation switch BTHAL_MD_UPLOAD_BUF_OWNER_MODEM is enabled.
 *      
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *     	buffer [in] - pointer to buffer filled with data to be uploaded.
 *
 *     	len [in] - actual length of filled data.
 *
 *     	endOfData [in] - marks end of data chunk in order to process even not
 *          full buffer.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the notification was successfully done.
 *
 *		BTHAL_STATUS_FAILED - if the notification failed.
 */
BthalStatus BTHAL_MD_UploadDataReady(BTHAL_U8 *buffer, BTHAL_INT len, BTHAL_BOOL endOfData);

#else

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Write()
 *
 * Brief:  
 *		Writes data to the modem.
 *
 * Description:
 *		It may be called by application, when the following indication is received:
 *			- data is received by the SPP port.
 *      Requires compilation switch BTHAL_MD_UPLOAD_BUF_OWNER_MODEM is disabled.
 *      
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *     	buffer [in] - pointer to buffer containing data to write.
 *
 *     	maxBytes [in] - number of bytes to write.
 *
 *     	writtenBytes [out] - pointer to amount of actually written bytes.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the write operation was successful.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_Write(const BTHAL_U8 *buffer,
						   BTHAL_INT maxBytes,
						   BTHAL_INT *writtenBytes);

#endif /* #if BTHAL_MD_UPLOAD_BUF_OWNER_MODEM == BTL_CONFIG_ENABLED */

/*-------------------------------------------------------------------------------
 * BTHAL_MD_TranslateBtSignalsToModem()
 *
 * Brief:  
 *		Translates BT serial signals to the modem's rules.
 *
 * Description:
 *      Translates serial signals from bits used by SPP port to those used by the
 *      modem.
 *      
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *		btSignals [in] - pointer to SPP control signal to be translated.
 *
 *		escSeqFound [in] - whether escape sequence was found.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the translation was successful.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_TranslateBtSignalsToModem(const SppControlSignals *btSignals,
                                               BTHAL_BOOL escSeqFound);
						    
/*-------------------------------------------------------------------------------
 * BTHAL_MD_TranslateModemSignalsToBt()
 *
 * Brief:  
 *		Translates modem's serial signals to the BT rules.
 *
 * Description:
 *      Translates serial signals from bits used by the modem port to those used
 *      by the SPP.
 *      
 * Type:
 *		Synchronous.
 *
 * Parameters:
 *		btSignals [out] - pointer to SPP control signals got after translation.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - if the translation was successful.
 *
 *		BTHAL_STATUS_FAILED or any specific error defined in BthalStatus type,
 *			if the operation failed to start.
 */
BthalStatus BTHAL_MD_TranslateModemSignalsToBt(SppControlSignals *btSignals);


#endif /* __BTHAL_MD_H */


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
*   FILE NAME:      bthal_md.c
*
*   DESCRIPTION:    Implementation of BTHAL Modem Data API.
*
*                   Connection to Modem Data service is established via generic
*                   DIO (Device Input/Output) interface which sees connected
*                   party as a device, in our case, as single BT device, and
*                   implementation of BTHAL MD interface - as BT device driver.
*
*                   Names of constants, types, and function names, which relate
*                   to the specific DIO BT driver implementation, start with
*                   prefix DIO_BT.
*
*   AUTHOR:         V. Abram
*
\*******************************************************************************/



#include "btl_config.h"
#include "bthal_md.h"
#include "osapi.h"

#if BTL_CONFIG_MDG == BTL_CONFIG_ENABLED

/********************************************************************************/


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include <string.h>
#include "bthal_types.h"
//#include "bthal_log.h"


/********************************************************************************
 *
 * Stubbing definitions for logging functionality - should be removed later 
 *
 *******************************************************************************/
#define BTHAL_LOG_Function  Report
#define BTHAL_LOG_Error     Report
#define BTHAL_LOG_Info      Report

 /********************************************************************************
 *
 * Constants 
 *
 *******************************************************************************/



/********************************************************************************
 *
 * Internal Types
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BthalMdState type
 *
 *     Defines the BTHAL MD states.
 */
typedef BTHAL_U8 BthalMdState;

#define BTHAL_MD_STATE_IDLE       	    (0x00)
#define BTHAL_MD_STATE_INITIALIZED   	(0x01)
#define BTHAL_MD_STATE_REGISTERED       (0x02)
#define BTHAL_MD_STATE_CONNECTED		(0x03)


/********************************************************************************
 *
 * Internal Data Structures
 *
 *******************************************************************************/
 
/*-------------------------------------------------------------------------------
 * BthalMdContext structure
 *
 *	   Represents BTHAL MD internal data used by an implementer of the specific
 *	   modem data interface.
 */
typedef struct _BthalMdContext
{
    /* State of the MD */
    BTHAL_U8        state;

    /* Callback for delivering BTHAL MD events */
	BthalMdCallback mdCallback;

    /* Communication settings */
    SppComSettings  comSettings;
	
} BthalMdContext;


/********************************************************************************
 *
 * Internal function prototypes
 *
 *******************************************************************************/

static void BthalMdSetState(BTHAL_U8 state);
static const char *BthalMdStateName(void);


/********************************************************************************
 *
 * Globals
 *
 *******************************************************************************/

/* BTHAL MD context with internal data */
BthalMdContext bthalMdContext = {BTHAL_MD_STATE_IDLE};


/********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/
/*-------------------------------------------------------------------------------
 * BTHAL_MD_Init()
 *
 *	   	Initializes modem's data service to be used by caller application.
 */
BthalStatus BTHAL_MD_Init(BthalCallBack	callback)
{
	UNUSED_PARAMETER(callback);

	BTHAL_LOG_Function(("BTHAL_MD_Init"));

    if (bthalMdContext.state >= BTHAL_MD_STATE_INITIALIZED)
    {
	    BTHAL_LOG_Error(("BTHAL MD module is already initialized"));
        return (BTHAL_STATUS_FAILED);
    }
    
    BthalMdSetState(BTHAL_MD_STATE_INITIALIZED);
    return (BTHAL_STATUS_SUCCESS);
}

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Deinit()
 *
 *	   	Releases modem's data service and resources which were used by caller
 *		application.
 */
BthalStatus BTHAL_MD_Deinit(void)
{
	BTHAL_LOG_Function(("BTHAL_MD_Deinit"));

    if (bthalMdContext.state < BTHAL_MD_STATE_INITIALIZED)
    {
	    BTHAL_LOG_Error(("BTHAL MD module is already deinitialized"));
        return (BTHAL_STATUS_FAILED);
    }

    BthalMdSetState(BTHAL_MD_STATE_IDLE);
    return (BTHAL_STATUS_SUCCESS);
}

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Register()
 *
 *	   	Registers with modem's data service to be used by caller application.
 */
BthalStatus BTHAL_MD_Register(const BthalMdCallback mdCallback,
						      const SppComSettings *comSettings)
{
	BTHAL_LOG_Function(("BTHAL_MD_Register"));

    if (bthalMdContext.state >= BTHAL_MD_STATE_REGISTERED)
    {
	    BTHAL_LOG_Error(("BTHAL MD module is already registered"));
        return (BTHAL_STATUS_FAILED);
    }

    /* Save callback function and configuration settings and set new state */
    bthalMdContext.mdCallback = mdCallback;
    bthalMdContext.comSettings = *comSettings;
    BthalMdSetState(BTHAL_MD_STATE_REGISTERED);

    return (BTHAL_STATUS_SUCCESS);
}

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Deregister()
 *
 *	   	Deregisters from the modem's data service.
 */
BthalStatus BTHAL_MD_Deregister()
{
	BTHAL_LOG_Function(("BTHAL_MD_Deregister"));

    if (bthalMdContext.state < BTHAL_MD_STATE_REGISTERED)
    {
	    BTHAL_LOG_Error(("BTHAL MD module is already deregistered"));
        return (BTHAL_STATUS_FAILED);
    }

    /* Clear callback function and configuration settings and set new state */
    bthalMdContext.mdCallback = NULL;
    memset((void *)&bthalMdContext.comSettings,
           0,
           sizeof(bthalMdContext.comSettings));
    BthalMdSetState(BTHAL_MD_STATE_INITIALIZED);

    return (BTHAL_STATUS_SUCCESS);
}

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Configure()
 */
BthalStatus BTHAL_MD_Configure(const SppComSettings *comSettings)
{
	BTHAL_LOG_Function(("BTHAL_MD_Configure"));

    if (bthalMdContext.state < BTHAL_MD_STATE_REGISTERED)
    {
	    BTHAL_LOG_Error(("BTHAL MD module is not registered"));
        return (BTHAL_STATUS_FAILED);
    }
    
    /* Save configuration settings */
    bthalMdContext.comSettings = *comSettings;

    return (BTHAL_STATUS_SUCCESS);
}

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Connect()
 *
 *     Connects to modem's data service.
 */
BthalStatus BTHAL_MD_Connect()
{
	BTHAL_LOG_Function(("BTHAL_MD_Connect"));
	
    /* Check, if MD is initialized */
    if (bthalMdContext.state < BTHAL_MD_STATE_INITIALIZED)
    {
        BTHAL_LOG_Error(("BTHAL MD is not initialized"));
        return (BTHAL_STATUS_FAILED);
    }

    /* Check, whether MD is not already connected */
    if (bthalMdContext.state >= BTHAL_MD_STATE_CONNECTED)
    {
        BTHAL_LOG_Error(("BTHAL MD is already connected"));
        return (BTHAL_STATUS_FAILED);
    }
    
    /* Connect to modem data service */
    
    /* Update the state */
    BthalMdSetState(BTHAL_MD_STATE_CONNECTED);

    return (BTHAL_STATUS_SUCCESS);
}

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Disconnect()
 *
 *     Disconnects from modem's data service.
 */
BthalStatus BTHAL_MD_Disconnect()
{
	BTHAL_LOG_Function(("BTHAL_MD_Disconnect"));

    /* Check, if MD is connected */
    if (bthalMdContext.state < BTHAL_MD_STATE_CONNECTED)
    {
        BTHAL_LOG_Error(("BTHAL MD is not connected"));
        return (BTHAL_STATUS_NO_CONNECTION);
    }
    
    /* Disconnect the modem data service */

    /* Set new state */
    BthalMdSetState(BTHAL_MD_STATE_REGISTERED);
        
    return (BTHAL_STATUS_SUCCESS);
}

#if BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM == BTL_CONFIG_ENABLED
 /*-------------------------------------------------------------------------------
 * BTHAL_MD_GetDownloadBuf()
 *
 *     Gets pointer to buffer with downloaded data and its length.
 */
BthalStatus BTHAL_MD_GetDownloadBuf(BTHAL_U8 **buffer, BTHAL_INT *len)
{
	UNUSED_PARAMETER(buffer);
	UNUSED_PARAMETER(len);

    BTHAL_LOG_Function(("BTHAL_MD_GetDownloadBuf"));

    /* Check, if MD is connected */
    if (bthalMdContext.state < BTHAL_MD_STATE_CONNECTED)
    {
        BTHAL_LOG_Error(("BTHAL MD is not connected"));
        return (BTHAL_STATUS_FAILED);
    }

    /* Check whether there is any unread downloaded data */

    /* Fill return values: pointer to data buffer and its length */


    return (BTHAL_STATUS_SUCCESS);
}

/*-------------------------------------------------------------------------------
 * BTHAL_MD_ReturnDownloadBuf()
 *
 *     Returns buffer to modem after downloaded data was copied from it.
 */
BthalStatus BTHAL_MD_ReturnDownloadBuf(BTHAL_U8 *buffer, BTHAL_INT len)
{
	UNUSED_PARAMETER(buffer);
	UNUSED_PARAMETER(len);

	BTHAL_LOG_Function(("BTHAL_MD_ReturnDownloadBuf"));

    /* Check, if MD is connected */
    if (bthalMdContext.state < BTHAL_MD_STATE_CONNECTED)
    {
        BTHAL_LOG_Error(("BTHAL MD is not connected"));
        return (BTHAL_STATUS_FAILED);
    }

    
    return (BTHAL_STATUS_SUCCESS);
}
#endif /* BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM == BTL_CONFIG_ENABLED */

#if BTHAL_MD_UPLOAD_BUF_OWNER_MODEM == BTL_CONFIG_ENABLED
/*-------------------------------------------------------------------------------
 * BTHAL_MD_GetUploadBuf()
 *
 *     Gets pointer to buffer and its size for data to be uploaded.
 */
BthalStatus BTHAL_MD_GetUploadBuf(BTHAL_INT size, BTHAL_U8 **buffer, BTHAL_INT *len)
{
	UNUSED_PARAMETER(size);
	UNUSED_PARAMETER(buffer);
	UNUSED_PARAMETER(len);

	BTHAL_LOG_Function(("BTHAL_MD_GetUploadBuf"));

    /* Check, if MD is connected */
    if (bthalMdContext.state < BTHAL_MD_STATE_CONNECTED)
    {
        BTHAL_LOG_Error(("BTHAL MD is not connected"));
        return (BTHAL_STATUS_FAILED);
    }

    /* Check whether there is any free buffer for uploading data. Note that
     * we send buffer to modem with any amount of data, if reading from BT is
     * finished. */

    /* Fill return values: pointer to data buffer and its length */
    
    return (BTHAL_STATUS_SUCCESS);
}

/*-------------------------------------------------------------------------------
 * BTHAL_MD_UploadDataReady()
 *
 *     Notifies that upload buffer is filled with data.
 */
BthalStatus BTHAL_MD_UploadDataReady(BTHAL_U8 *buffer,
                                     BTHAL_INT len,
                                     BTHAL_BOOL endOfData)
{
	BthalStatus mdStatus = BTHAL_STATUS_SUCCESS;
    BTHAL_BOOL escSequenceFound = BTHAL_FALSE;

	UNUSED_PARAMETER(endOfData);

	BTHAL_LOG_Function(("BTHAL_MD_UploadDataReady"));

    /* Check, if MD is connected */
    if (bthalMdContext.state < BTHAL_MD_STATE_CONNECTED)
    {
        BTHAL_LOG_Error(("BTHAL MD is not connected"));
        return (BTHAL_STATUS_FAILED);
    }

    /* Check data for the escape sequence.
     * Assume, at least for now, that it may appear only at the beginning of
     * buffer. This means that we always have enough DIO buffers for reading
     * data from BT or, and this often happens, that escape sequence is sent
     * after some timeout without data. We also assume that there is no data
     * after the sequence, at least, we throw it away */
    if ((len >= 3) && (NULL != buffer) && 
        (*buffer++ == '+') && (*buffer++ == '+') && (*buffer == '+'))
    {
	    BTHAL_LOG_Info(("BTHAL MD: ESC sequence detected"));

        /* Notify modem that escape sequence was detected */
//        mdStatus = BTHAL_MD_TranslateBtSignalsToModem(&btControlSignals,
//                                                      BTHAL_TRUE);
		if (BTHAL_STATUS_SUCCESS != mdStatus)
		{
			BTHAL_LOG_Info(("BTHAL_MD_TranslateBtSignalsToModem status: %s",
							BTHAL_StatusName(mdStatus)));
		}
        else
        {
            escSequenceFound = BTHAL_TRUE;
        }
    }
    else
    {
        /* Update amount of passed data for uploading */
    }

    /* Check whether all buffer is filled or this is the end of data chunk for
     * uploading or escape sequence was detected or only control information was
     * passed */

    return (mdStatus);
}
#endif /* BTHAL_MD_UPLOAD_BUF_OWNER_MODEM == BTL_CONFIG_ENABLED */

/*-------------------------------------------------------------------------------
 * BTHAL_MD_TranslateBtSignalsToModem()
 *
 *     Translates serial signals from bits used by SPP port to modem rules.
 */
BthalStatus BTHAL_MD_TranslateBtSignalsToModem(const SppControlSignals *btSignals,
                                               BTHAL_BOOL escSeqFound)
{

    /* Check whether new control signals were passed */
    if (NULL == btSignals)
    {
        return (BTHAL_STATUS_INVALID_PARM);
    }

    if (!(btSignals->signals & SPP_CONTROL_SIGNAL_DTR))
    {
    }

    if (!(btSignals->signals & SPP_CONTROL_SIGNAL_RTS))
    {
    }

    /* Check whether escape sequence was found */
    if (escSeqFound)
    {
    }
    

    return (BTHAL_STATUS_SUCCESS);
}
						    
/*-------------------------------------------------------------------------------
 * BTHAL_MD_TranslateModemSignalsToBt()
 *
 *     Translates serial signals from bits used by modem port to SPP rules.
 */
BthalStatus BTHAL_MD_TranslateModemSignalsToBt(SppControlSignals *btSignals)
{
    btSignals->signals = 0;

    BTHAL_LOG_Info(("BTHAL_MD_TranslateModemSignalsToBt: signals 0x%02x, breakLen %d",
                    btSignals->signals,
                    btSignals->breakLen));

    return (BTHAL_STATUS_SUCCESS);
}

/*-------------------------------------------------------------------------------
 * BthalMdSetState()
 *
 *		Gets state's name.
 */
static void BthalMdSetState(BTHAL_U8 state)
{
    bthalMdContext.state = state;
    BTHAL_LOG_Info(("BTHAL MD: state %s", BthalMdStateName()));
}

/*-------------------------------------------------------------------------------
 * BthalMdStateName()
 *
 *		Gets state's name.
 */
static const char *BthalMdStateName()
{
    switch (bthalMdContext.state)
    {
        case BTHAL_MD_STATE_IDLE:
            return "BTHAL_MD_STATE_IDLE";

        case BTHAL_MD_STATE_INITIALIZED:
            return "BTHAL_MD_STATE_INITIALIZED";

        case BTHAL_MD_STATE_REGISTERED:
            return "BTHAL_MD_STATE_REGISTERED";

        case BTHAL_MD_STATE_CONNECTED:
            return "BTHAL_MD_STATE_CONNECTED";

        default:
            return "BTHAL MD unknown state";
    }
}




#else /*BTL_CONFIG_MDG ==   BTL_CONFIG_ENABLED*/

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Init() - When  BTL_CONFIG_MDG  is disabled.
 */
BthalStatus BTHAL_MD_Init(BthalCallBack	callback)
{
	UNUSED_PARAMETER(callback);

       Report(("BTHAL_MD_Init()  -  BTL_CONFIG_MDG Disabled"));
  

    return BT_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * BTHAL_MD_Deinit() - When  BTL_CONFIG_MDG is disabled.
 */
BthalStatus BTHAL_MD_Deinit(void)
{
    Report(("BTHAL_MD_Deinit() -  BTL_CONFIG_MDG Disabled"));

    return BT_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------*/



#endif /*BTL_CONFIG_FTPC ==   BTL_CONFIG_ENABLED*/



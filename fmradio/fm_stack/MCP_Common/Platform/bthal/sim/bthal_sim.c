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
*   FILE NAME:      bthal_sim.c
*
*   DESCRIPTION:    This file contain implementation of the BTHAL for SIM on PC
*                   Here the SIM is simulated via a card.c and card.h file
*                   as provided with the Blue SDK.
*                   These simulation calls have all teh CRD_ prefix.
*
*   AUTHOR:         Gerrit Slot
*
\*******************************************************************************/

/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "xatypes.h" /* for types such as U8, U16, S8, S16,... */
#include <stdio.h>
#include "debug.h"
#include "sim.h"
#include "btl_log_modules.h"
#include "btl_defs.h"
#include "btl_saps.h"
#include "bthal_sim.h"
#include "card.h"        /* SIM card simulation interface on PC. */
#include "sim_server.h"  /* SIM card simulation GUI              */


BTL_LOG_SET_MODULE(BTL_LOG_MODULE_TYPE_SAPS);

/********************************************************************************
 *
 * Macros
 *
 *******************************************************************************/

#define MAX_APDU_SIZE ((U16) 263) /* Maximum size (bytes) of an APDU packet */
#define MAX_ATR_SIZE  ((U8)   33) /* Maximum size (bytes) of an ATR packet. */

/* Some debug logging features. */
#if (XA_DEBUG == XA_ENABLED) || (XA_DEBUG_PRINT == XA_ENABLED)
#define MAX_LOG_BUFFER ((U8)   60)
#endif /* (XA_DEBUG == XA_ENABLED) || (XA_DEBUG_PRINT == XA_ENABLED) */

/********************************************************************************
 *
 * Constants
 *
 *******************************************************************************/

/********************************************************************************
 *
 * Data Structures
 *
 *******************************************************************************/
/*-------------------------------------------------------------------------------
 * SimAtr type
 *
 *     ATR (Answer To Reset) data from the SIM.
 */
typedef struct _BthalSimAtr
{
  U8 len;                 /* Number of bytes in next array.*/
  U8 data[MAX_ATR_SIZE];  /* ATR data.                     */
} BthalSimAtr;

/*-------------------------------------------------------------------------------
 * SimApdu type
 *
 *     Latest received APDU (GSM 11.11) response received from the SIM.
 */
typedef struct _BthalSimApdu
{
  U16 len;                 /* Number of bytes in next array.*/
  U8  data[MAX_APDU_SIZE]; /* APDU response data.           */
} BthalSimApdu;

/********************************************************************************
 *
 * Globals
 *
 *******************************************************************************/
/* The ATR (Answer To Reset) is administrated for later reading. */
/*  Readable via via BTHAL_SIM_GetAtr                            */ 
static BthalSimAtr simAtr;

/* APDU response from the SIM card for later reading */
/*  Readable via BTHAL_SIM_ReadApduRsp               */ 
static BthalSimApdu simApduRsp;

/* Actual status of the SIM card for later reading */
/*  Readable via BTHAL_SIM_GetStatus               */ 
static SimCardStatus simStatus;

/* Status of the SIM card reader as determined during startup, for later reading */
/*  Readable via BTHAL_SIM_GetCardReaderStatus.                                  */ 
static SimCardReaderStatus cardReaderStatus;

/* Administrated callback during the BTHAL_SIM_Init. */
static BTHAL_SIM_CallBack bthalCallback;

/* Some debug logging features. */
#if (XA_DEBUG == XA_ENABLED) || (XA_DEBUG_PRINT == XA_ENABLED)
/* The logging buffer for logging sim messages */
static char logBuffer[MAX_LOG_BUFFER + 1];
#endif /* (XA_DEBUG == XA_ENABLED) || (XA_DEBUG_PRINT == XA_ENABLED) */

/********************************************************************************
 *
 * Internal function prototypes
 *
 *******************************************************************************/

/* Some debug logging features. */
#if (XA_DEBUG == XA_ENABLED) || (XA_DEBUG_PRINT == XA_ENABLED)
static void LogSimWriteApduReq(U8 *apduPtr, U16 len);
static void LogSimReadApduRsp(U8 *apduPtr, U16 len);

#define CLA	           ((U8)    0)
#define INS            ((U8)    1)
#define P1             ((U8)    2)
#define P2             ((U8)    3)
#define P3             ((U8)    4)
#define START_DATA_REQ ((U8)    5)
#define MIN_LENGTH_RSP ((U8)    2)
#define SW1(Len)       ((U8)Len-2)
#define SW2(Len)       ((U8)Len-1)

#define DATA_LENGTH    ((U8)    3)

#define BTL_SIM_WRITE_APDU_REQ(apduPtr, len) LogSimWriteApduReq(apduPtr, len)
#define BTL_SIM_READ_APDU_RSP(apduPtr, len) LogSimReadApduRsp(apduPtr, len)
#else
/* Debugging disabled --> define empty macros */
#define BTL_SIM_WRITE_APDU_REQ(apduPtr, len)
#define BTL_SIM_READ_APDU_RSP(apduPtr, len) 
#endif /* (XA_DEBUG == XA_ENABLED) || (XA_DEBUG_PRINT == XA_ENABLED) */

/********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_Init()
 */
BthalStatus BTHAL_SIM_Init(BTHAL_SIM_CallBack callback)
{
  BthalStatus status = BTHAL_STATUS_SUCCESS;

  /* Store it in the global admin. */
  bthalCallback = callback;

  /* Initialize the SIM card simulation module */
  CRD_Init();

  return status;
}

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_Deinit()
 */
BthalStatus BTHAL_SIM_Deinit(void)
{
  BthalStatus status = BTHAL_STATUS_SUCCESS;
  
  /* Clean the global admin. */
  bthalCallback = NULL;

   /* Initialize the SIM card simulation module */
  CRD_Deinit();

  return status;
}


/*-------------------------------------------------------------------------------
 * BTHAL_SIM_Reset()
 */
BthalStatus BTHAL_SIM_Reset()
{
  BthalStatus    status = BTHAL_STATUS_SUCCESS;
  U8            *simCardFlags;
  SimResultCode  result;

  SIM_SERVER_GetSimCardFlags(&simCardFlags);

  /* No card available --> tell it */
  if (!(*simCardFlags & SIM_CARD_FLAG_INSERTED))
  {
    result = SIM_RESULT_CARD_REMOVED;
  }
  /* Card available, not accessible --> tell it */
  else if (!(*simCardFlags & SIM_CARD_FLAG_ACCESSIBLE))
  {
    result = SIM_RESULT_CARD_NOT_ACCESSIBLE;
  }
  /* Card available, accessible and power already off --> tell it */
  else if (!(*simCardFlags & SIM_CARD_FLAG_POWERED))
  {
    result = SIM_RESULT_CARD_ALREADY_OFF;
  }
  /* Card available, accessible and powered --> Dot it */
  else
  {
    result = SIM_RESULT_OK;
  }
  
  /* I make it a synchronous interface, so I give immediate reply. */
  bthalCallback(BTHAL_SIM_EVENT_STATUS_CHANGED, result);
  
  return status;
}

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_PowerOn()
 */
BthalStatus BTHAL_SIM_PowerOn()
{
  BthalStatus    status = BTHAL_STATUS_SUCCESS;
  U8            *simCardFlags;
  SimResultCode  result;

  SIM_SERVER_GetSimCardFlags(&simCardFlags);

  /* No card available --> tell it */
  if (!(*simCardFlags & SIM_CARD_FLAG_INSERTED))
  {
    result = SIM_RESULT_CARD_REMOVED;
  }
  /* Card available, not accessible --> tell it */
  else if (!(*simCardFlags & SIM_CARD_FLAG_ACCESSIBLE))
  {
    result = SIM_RESULT_CARD_NOT_ACCESSIBLE;
  }
  /* Card available, accessible and already powered --> tell it */
  else if (*simCardFlags & SIM_CARD_FLAG_POWERED)
  {
    result = SIM_RESULT_CARD_ALREADY_ON;
  }
  /* Card available, accessible and not powered --> Dot it */
  else
  {
    *simCardFlags |= SIM_CARD_FLAG_POWERED;

    /* Update the GUI. */
    SIM_SERVER_UpdateGui(TRUE);

    result = SIM_RESULT_OK;
  }
  
  /* I make it a synchronous interface, so I give immediate reply. */
  bthalCallback(BTHAL_SIM_EVENT_STATUS_CHANGED, result);

  return status;
}

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_PowerOff()
 */
BthalStatus BTHAL_SIM_PowerOff(void)
{
  BthalStatus    status = BTHAL_STATUS_SUCCESS;
  U8            *simCardFlags;
  SimResultCode  result;

  SIM_SERVER_GetSimCardFlags(&simCardFlags);

  /* No card available --> tell it */
  if (!(*simCardFlags & SIM_CARD_FLAG_INSERTED))
  {
    result = SIM_RESULT_CARD_REMOVED;
  }
  /* Card available and power already off --> tell it */
  else if (!(*simCardFlags & SIM_CARD_FLAG_POWERED))
  {
    result = SIM_RESULT_CARD_ALREADY_OFF;
  }
  /* Card available and power on --> Dol it */
  else
  {
    *simCardFlags &= ~SIM_CARD_FLAG_POWERED;

    /* Update the GUI. */
    SIM_SERVER_UpdateGui(TRUE);

    result = SIM_RESULT_OK;
  }

  /* I make it a synchronous interface, so I give immediate reply. */
  bthalCallback(BTHAL_SIM_EVENT_STATUS_CHANGED, result);

  return status;
}

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_WriteApduReq()
 */
BthalStatus BTHAL_SIM_WriteApduReq(U16 len,
                                   U8 *data)
{
  BthalStatus    status = BTHAL_STATUS_SUCCESS;
  U8            *RspApduPtr;
  U16            rspLen;
  U8            *simCardFlags;
  SimResultCode  result;

  SIM_SERVER_GetSimCardFlags(&simCardFlags);

  /* Powered? --> process APDU */
  if (*simCardFlags & SIM_CARD_FLAG_POWERED)
  {
    result = SIM_RESULT_OK;
  }
  /* Not powered + accessible? --> tell it. */
  else if (*simCardFlags & SIM_CARD_FLAG_ACCESSIBLE)
  {
    result = SIM_RESULT_CARD_ALREADY_OFF;
  }
  /* Not powered + not accessible + inserted? --> tell it. */
  else if (*simCardFlags & SIM_CARD_FLAG_INSERTED)
  {
    result = SIM_RESULT_CARD_NOT_ACCESSIBLE;
  }
  /* Not powered + not accessible + not inserted? --> tell it. */
  else
  {
    result = SIM_RESULT_CARD_REMOVED;
  }

  /* Everything OK? --> process APDU request. */
  if (SIM_RESULT_OK == result)
  {
   	/* DEBUG: do some logging. */
	BTL_SIM_WRITE_APDU_REQ(data, len);
  
    CRD_ParseAPDU(data, len, &RspApduPtr, &rspLen);

    /* Store  APDU response for later reading. */
    OS_MemCopy(simApduRsp.data, RspApduPtr, rspLen);
    simApduRsp.len = rspLen;
  }

  /* I make it a synchronous interface, so I give immediate reply. */
  bthalCallback(BTHAL_SIM_EVENT_APDU_RSP, result);

  return status;
}

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_ReadApduRsp()
 */
BthalStatus BTHAL_SIM_ReadApduRsp(U16 *len,
                                  U8 **apduRsp)
{
  BthalStatus status = BTHAL_STATUS_SUCCESS;

  /* Reply the already stored ApduRsp */
  *len = simApduRsp.len;
  *apduRsp = simApduRsp.data;

  /* DEBUG: do some logging. */
  BTL_SIM_READ_APDU_RSP(*apduRsp, *len);
  
  return status;
}

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_GetStatus()
 */
BthalStatus BTHAL_SIM_GetStatus(SimCardStatus *cardStatus)
{
  BthalStatus  status = BTHAL_STATUS_SUCCESS;
  U8          *simCardFlags;;
  
  SIM_SERVER_GetSimCardFlags(&simCardFlags);
  
  if (*simCardFlags & SIM_CARD_FLAG_POWERED)
  {
    *cardStatus = SIM_CARD_STATUS_RESET;
  }
  else
  {
    if (!(*simCardFlags & SIM_CARD_FLAG_INSERTED))
    {
      *cardStatus = SIM_CARD_STATUS_REMOVED;
    }
    else
    {
      *cardStatus =  SIM_CARD_STATUS_NOT_ACCESSIBLE;
    }
  }
  
  return status;
}

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_GetAtr()
 */
BthalStatus BTHAL_SIM_GetAtr(U16 *len,
                             U8 **atr,
                             SimResultCode *result)
{
  BthalStatus  status = BTHAL_STATUS_SUCCESS;
  U8          *simCardFlags;

  SIM_SERVER_GetSimCardFlags(&simCardFlags);

  /* Powered? --> Get ATR info as stored in the SIM-card simulator. */
  if (*simCardFlags & SIM_CARD_FLAG_POWERED)
  {
    *result = SIM_RESULT_OK;
  }
  /* Not powered + accessible? --> tell it. */
  else if (*simCardFlags & SIM_CARD_FLAG_ACCESSIBLE)
  {
    *result = SIM_RESULT_CARD_ALREADY_OFF;
  }
  /* Not powered + not accessible? --> tell it. */
  else
  {
    *result = SIM_RESULT_CARD_REMOVED;
  }


  /* Everything OK? --> get ATR info as stored in the SIM-card simulator. */
  if (SIM_RESULT_OK == *result)
  {
    CRD_GetAtr(atr, len);
  }
  /* ATR cannot be replied! --> make ATR info empty. */
  else
  {
    *len = 0;
    *atr = NULL;

    /* ATTENTION: Workaround.                             */
    /* The BlueSDK has a problem with the values above.   */
    /* (See routine SIM_ServerAtrRsp that will be called) */
    /* A problem report has been made, in the meantime    */
    /* the next workaround is made.                       */
    CRD_GetAtr(atr, len);
  }
  
  return status;
}

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_GetCardReaderStatus()
 */
BthalStatus BTHAL_SIM_GetCardReaderStatus(SimCardReaderStatus *cardReaderStatus,
                                          SimResultCode *result)
{
  BthalStatus status = BTHAL_STATUS_SUCCESS;

  *cardReaderStatus = SIM_CRS_ID0 | SIM_CRS_REMOVABLE | SIM_CRS_PRESENT |
                      SIM_CRS_ID1_SIZE | SIM_CRS_CARD_PRESENT | SIM_CRS_CARD_POWERED;
  
  /* Fill SimResultCode. */
  *result = SIM_RESULT_OK;

  return status;
}

/*-------------------------------------------------------------------------------
 * BTHAL_SIM_StatusUpdated()
 *  
 *    PC simulation implementation only !!!
 *
 *    This routine is only added for the implementation of the SIM simulator on PC
 *    The changing of the CardStatus is simulated via a GUI. When the GUI does
 *    make a change, it will call this function.
 *
 *    This function will trigger teh SAPS moduel via the registerd callback that
 *    the status has changed.
 */
void BTHAL_SIM_StatusUpdated()
{
  /* Result value is not relevant in this case. */
  bthalCallback(BTHAL_SIM_EVENT_STATUS_CHANGED, SIM_RESULT_OK);
}

/* Some debug logging routines. */
#if (XA_DEBUG == XA_ENABLED) || (XA_DEBUG_PRINT == XA_ENABLED)
/*-------------------------------------------------------------------------------
 * LogSimWriteApduReq()
 *
 *    Display the sim write apdu request information in the logging window as readable text.
 *
 * Parameters:
 *    apduPtr[in] - The sim apdu request bytes.
 *    len[in] - The number of bytes.
 *
 * Returns:
 */
 static void LogSimWriteApduReq(U8 *apduPtr, U16 len)
{
    int i;
    
    switch (apduPtr[INS]) 
    {
       case INS_SELECT: BTL_LOG_INFO(("LogSimWriteApduReq: INS = SELECT, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_STATUS: BTL_LOG_INFO(("LogSimWriteApduReq: INS = STATUS, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_READ_BINARY: BTL_LOG_INFO(("LogSimWriteApduReq: INS = READ_BINARY, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_UPDATE_BINARY: BTL_LOG_INFO(("LogSimWriteApduReq: INS = UPDATE_BINARY, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_READ_RECORD: BTL_LOG_INFO(("LogSimWriteApduReq: INS = READ_RECORD, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_UPDATE_RECORD: BTL_LOG_INFO(("LogSimWriteApduReq: INS = UPDATE_RECORD, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_SEEK: BTL_LOG_INFO(("LogSimWriteApduReq: INS = SEEK, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_INCREASE: BTL_LOG_INFO(("LogSimWriteApduReq: INS = INCREASE, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_VERIFY_CHV: BTL_LOG_INFO(("LogSimWriteApduReq: INS = VERIFY_CHV, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_CHANGE_CHV: BTL_LOG_INFO(("LogSimWriteApduReq: INS = CHANGE_CHV, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break; 
       case INS_DISABLE_CHV: BTL_LOG_INFO(("LogSimWriteApduReq: INS = DISABLE_CHV, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_ENABLE_CHV: BTL_LOG_INFO(("LogSimWriteApduReq: INS = ENABLE_CHV, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_UNBLOCK_CHV: BTL_LOG_INFO(("LogSimWriteApduReq: INS = UNBLOCK_CHV, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_INVALIDATE: BTL_LOG_INFO(("LogSimWriteApduReq: INS = INVALIDATE, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_GSM_ALGORITHM: BTL_LOG_INFO(("LogSimWriteApduReq: INS = GSM_ALGORITHM, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_SLEEP: BTL_LOG_INFO(("LogSimWriteApduReq: INS = SLEEP, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_RESPONSE: BTL_LOG_INFO(("LogSimWriteApduReq: INS = RESPONSE, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_TERMINAL_PROFILE: BTL_LOG_INFO(("LogSimWriteApduReq: INS = TERMINAL_PROFILE, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_ENVELOPE: BTL_LOG_INFO(("LogSimWriteApduReq: INS = ENVELOPE, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_FETCH: BTL_LOG_INFO(("LogSimWriteApduReq: INS = FETCH, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       case INS_TERMINAL_RESPONSE: BTL_LOG_INFO(("LogSimWriteApduReq: INS = TERMINAL_RESPONSE, P1 = %02x, P2 = %02x, P3 = %02x.", apduPtr[P1], apduPtr[P2], apduPtr[P3])); break;
       default: BTL_LOG_INFO(("LogSimWriteApduReq: INS = DEFAULT. Request not defined!!")); break;
    }

    if (len > START_DATA_REQ)
    {      
        for (i = START_DATA_REQ; (i < len) && (i <= ((MAX_LOG_BUFFER - DATA_LENGTH) / DATA_LENGTH)); i++)
        {
            sprintf (&logBuffer[(i - START_DATA_REQ) * DATA_LENGTH], "%02x ", apduPtr[i]);
        }

        if (i > ((MAX_LOG_BUFFER - DATA_LENGTH) / DATA_LENGTH))
        {
            sprintf (&logBuffer[(i - START_DATA_REQ) * DATA_LENGTH], "...");
        }
        
        BTL_LOG_INFO(("LogSimWriteApduReq: Data = %s", logBuffer));
    }
}


/*-------------------------------------------------------------------------------
 * LogSimReadApduRsp()
 *
 *    Display the sim read apdu response information in the logging window as readable text.
 *
 * Parameters:
 *    aduPtr[in] - The sim apdu response bytes.
 *    len[in] - The number of bytes.
 *
 * Returns:
 */
 static void LogSimReadApduRsp(U8 *apduPtr, U16 len)
{
    int i;

    BTL_LOG_INFO(("LogSimReadApduRsp: Status SW1 = %02x, SW2 = %02x.", apduPtr[SW1(len)], apduPtr[SW2(len)]));

    if (len > MIN_LENGTH_RSP)
    {       
        for (i = 0; (i < len) && (i <= ((MAX_LOG_BUFFER - DATA_LENGTH) / DATA_LENGTH)); i++)
        {
            sprintf (&logBuffer[i * DATA_LENGTH], "%02x ", apduPtr[i]);
        }
        
        if (i > ((MAX_LOG_BUFFER - DATA_LENGTH) / DATA_LENGTH))
        {
            sprintf (&logBuffer[i * DATA_LENGTH], "...");
        }
        
        BTL_LOG_INFO(("LogSimReadApduRsp: Data = %s", logBuffer));
    }
}
#endif /* (XA_DEBUG == XA_ENABLED) || (XA_DEBUG_PRINT == XA_ENABLED) */

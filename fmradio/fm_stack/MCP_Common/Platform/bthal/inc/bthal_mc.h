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
*   FILE NAME:      bthal_mc.h
*
*   BRIEF:          This file defines the API of the BTHAL Modem Control.
*
*   DESCRIPTION:    The module handles all modem control operation.
*                   1)  handles sending AT commands to the modem. 
*                       It builds, sends and manage the sending of AT commands
*                       to the modem.
*                   2)  Handles all events coming from modem. 
*                       - AT command responses (AT commands reply from the modem)
*                       - AT command results (AT commands initiated by the modem)
*
*   AUTHOR:         Itay Klein
*
\*******************************************************************************/

#ifndef __BTHAL_MC_H
#define __BTHAL_MC_H

/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/

#include "bthal_types.h"
#include "bthal_common.h"
#include "hfg.h"

/********************************************************************************
 *
 * Constants
 *
 *******************************************************************************/

/* Max length for phone number */
#define BTHAL_MC_MAX_NUMBER_LENGTH          100

/* Max length for operator's name */
#define BTHAL_MC_MAX_OPERATOR_NAME_LENGTH   30

#define BTHAL_MC_MAX_ENTRY_TEXT_LENGTH      30

/*  The maximal length of a character set name */
#define BTHAL_MC_CHARSET_NAME_MAX_LENGTH    7


/********************************************************************************
 *
 * Types
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BthalMcEventType type
 *
 *  Indicating the type of event sent by the BTHAL MC.
 */
typedef BTHAL_I32 BthalMcEventType;

/* 
 * OK sent from the modem.
 * Indicates proper execution of a command.
 */
#define BTHAL_MC_EVENT_OK                               0

/* 
 * ERROR sent from the modem.
 * Indicating improper execution of or an unknown command.
 */
#define BTHAL_MC_EVENT_ERROR                            1

/* 
 * NO CARRIER Signal received from the modem.
 */
#define BTHAL_MC_EVENT_NO_CARRIER                       2

/* 
 * BUSY Signal received from the modem.
 */
#define BTHAL_MC_EVENT_BUSY                             3

/* 
 * NO ANSWER Signal received from the modem
 */
#define BTHAL_MC_EVENT_NO_ANSWER                        4

/* 
 * DELAYED Signal received from the modem.
 */
#define BTHAL_MC_EVENT_DELAYED                          5

/* 
 * BLACKLISTED Signal received from the modem.
 */
#define BTHAL_MC_EVENT_BLACKLISTED                      6

/* 
 * RING received from the modem.
 */
#define BTHAL_MC_EVENT_RING                             7

/* 
 * +CME ERROR: <n> received from the modem
 * The "BthalMcEvent.p.err" field contains
 * the extended error value.
 */
#define BTHAL_MC_EVENT_EXTENDED_ERROR                   8

/* 
 * Call waiting notification received.
 * The "BthalMcEvent.p.callwait" field is valid.
 */
#define BTHAL_MC_EVENT_CALL_WAIT_NOTIFY                 9

/* 
 * A list of current calls received.
 * The "BthalMcEvent.p.calllist" field is valid.
 */
#define BTHAL_MC_EVENT_CURRENT_CALLS_LIST_RESPONSE      10

/* 
 * Calling line identification notification received
 * The "BthalMcEvent.p.number" field is valid.
 */
#define BTHAL_MC_EVENT_CALLING_LINE_ID_NOTIFY           11

/* 
 * Standard indicator reporting.
 * The "BthalMcEvent.p.indicator" field is valid. 
 */
#define BTHAL_MC_EVENT_INDICATOR_EVENT                  12

/* 
 * Subscriber number information response received.
 * The "BthalMcEvent.p.subscriber" field is valid.
 */
#define BTHAL_MC_EVENT_SUBSCRIBER_NUMBER_RESPONSE       13

/* 
 * Available options for call hold and multiparty
 * received.
 * TBD - not implemented yet.
 */
#define BTHAL_MC_EVENT_CALL_HOLD_AND_MULTIPARTY_OPTIONS 14

/* 
 * Current value of indicators received from the modem.
 * The "BthalMcEvent.p.indicatorsValue" field is valid.
 */
#define BTHAL_MC_EVENT_CURRENT_INDICATORS_STATUS        15

/* 
 * Indicators mapping to index and range received.
 * TBD - not implemented yet.
 */
#define BTHAL_MC_EVENT_INDICATORS_RANGE                 16

/* 
 * Network operator string received from the modem.
 * The "BthalMcEvent.p.oper" field is valid.
 */
#define BTHAL_MC_EVENT_NETWORK_OPERATOR_RESPONSE        17

/* 
 * The current status of the response and hold features
 * received.
 * The "BthalMcEvent.p.responseHold" field is valid
 */
#define BTHAL_MC_EVENT_READ_RESPONSE_AND_HOLD_RES       18


/*
 * Event indicating the result of a AT+
 */
#define BTHAL_MC_EVENT_SET_RESPONSE_AND_HOLD_RES        19

/* 
 * The link with the modem is established
 */
#define BTHAL_MC_EVENT_MODEM_LINK_ESTABLISHED           20

/* 
 * The link with the modem is released.
 */
#define BTHAL_MC_EVENT_MODEM_LINK_RELEASED              21

/*
 * The currently selected phonebook
 * The "BthalMcEvent.p.selectedPhonebook" field is valid
 */
#define BTHAL_MC_EVENT_SELECTED_PHONEBOOK               22

/*
 * The locally support phonebooks that can be used with
 * BTHAL_MC_SetPhonebook().
 * The "BthalMcEvent.p.supportedPhonebooks" field is a valid bit mask.
 */
#define BTHAL_MC_EVENT_SUPPORTED_PHONEBOOKS         23

/*
 *
 */
#define BTHAL_MC_EVENT_READ_PHONEBOOK_ENTRIES_RES       24

/*
 *
 */
#define BTHAL_MC_EVENT_TST_READ_PHONEBOOK_ENTRIES_RES   25

/*
 *
 */
#define BTHAL_MC_EVENT_FIND_PHONEBOOK_ENTRIES_RES       26

/*
 *
 */
#define BTHAL_MC_EVENT_SELECTED_CHAR_SET                27

/* Forward declaration: */
typedef struct _BthalMcEvent BthalMcEvent;

/*-------------------------------------------------------------------------------
 * BthalMcCallback type
 *
 *  A function of this type is called to indicate BTHAL MC Events.
 */
typedef void (*BthalMcCallback)(const BthalMcEvent *event);

/********************************************************************************
 *
 * Data Structures
 *
 *******************************************************************************/

/* forward declaration */
typedef struct _BthalMcContext BthalMcContext;
/* The field 'event' defines which union field is valid,
           look at the list of MC events for description on the various fields. */
/*---------------------------------------------------------------------------
 * BthalMcEvent structure
 * 
 *     This structure is used during modem events.
 */
struct _BthalMcEvent
{
    /* 
     * The type of the event.
     * Defines which union field is valid.
     */
    BthalMcEventType type;      

    /* The status of the event. */
    BthalStatus status;         

    /* The event's context */
    BthalMcContext *context;    

    /* The argument supplied in BTL_MC_Register */
    void *userData;             

    union
    {
        HfgCmeError err;

        struct
        {
            /* the callwait number */
            char number[BTHAL_MC_MAX_NUMBER_LENGTH+1];

            /* the length of the number field */
            BTHAL_U8 length;

            /* the type of the number */
            BTHAL_U8 type;

            /* Voice parameters */
            BTHAL_U8 classmap;
        } callwait;

        struct
        {
            /* Index of the call in the modem */
            BTHAL_U8 index;

            /* 0 - Mobile Originated, 1 = Mobile Terminated */
            BTHAL_U8 dir;

            /* Call state (see HfgCallStatus) */
            HfgCallStatus state;

            /* Call mode (see HfgCallMode) */
            HfgCallMode mode;

            /* 0 - Not Multiparty, 1 - Multiparty */
            BTHAL_U8 multiParty;
    
            /* the callwait number */
            char number[BTHAL_MC_MAX_NUMBER_LENGTH+1];

            /* the length of the number field */
            BTHAL_U8 length;

            /* Type of address */
            BTHAL_U8 type;

        } calllist;

        struct
        {
            char number[BTHAL_MC_MAX_NUMBER_LENGTH+1];

            BTHAL_U8 type;

        } number;

        struct
        {
            HfgIndicator ind;

            BTHAL_U8 val;
        }indicator;

        struct
        {
            BTHAL_BOOL service;

            BTHAL_BOOL call;

            HfgCallSetupState setup;

            HfgHoldState hold;

            BTHAL_U8 battery;

            BTHAL_U8 signal;

            BTHAL_BOOL roaming;

        }indicatorsValue;

        struct
        {
            char number[BTHAL_MC_MAX_NUMBER_LENGTH+1];
            
            BTHAL_U8 length;

            /* Phone number format */
            AtNumberFormat type;

            /* Service related to the phone number. */
            BTHAL_U8 service;

        } subscriber;

        struct
        {
            /* 
             * 0 = automatic, 1 = manual, 2 = deregister, 3 = set format only, 
             * 4 = manual/automatic.
             */
            BTHAL_U8 mode;

            /* Format of "oper" parameter (should be set to 0) */

            AtOperFormat format;


            char name[BTHAL_MC_MAX_OPERATOR_NAME_LENGTH+1];

            BTHAL_U8 length;

        } oper;


#if HFG_USE_RESP_HOLD == XA_ENABLED

        HfgResponseHold respHold;

#endif /*HFG_USE_RESP_HOLD == XA_ENABLED*/

        struct
        {
            BTHAL_S16   used;

            BTHAL_S16   total;
        
#if AT_PHONEBOOK == XA_ENABLED

            AtPbStorageType selected;

#endif /*AT_PHONEBOOK == XA_ENABLED*/
            
        } selectedPhonebook;

#if AT_PHONEBOOK == XA_ENABLED

        AtPbStorageType supportedPhonebooks;

#endif /*AT_PHONEBOOK == XA_ENABLED*/

        struct
        {
            BTHAL_U8    index;

            BTHAL_U8    type;

            char            number[BTHAL_MC_MAX_NUMBER_LENGTH+1];

            char            text[BTHAL_MC_MAX_ENTRY_TEXT_LENGTH+1];
            
        }phonebookEntry;

        struct
        {
            BTHAL_U8 firstIndex;

            BTHAL_U8 lastIndex;

            BTHAL_S16 numberLength;

            BTHAL_S16 textLength;

        }phonebookData;

        char charsetType[BTHAL_MC_CHARSET_NAME_MAX_LENGTH];
    }p;     
};

/********************************************************************************
 *
 * Function Reference
 *
 *******************************************************************************/

/*---------------------------------------------------------------------------
 * BTHAL_MC_Init()
 *
 * Brief:   Initializes the BTHAL Modem Control module.  
 *      
 * Description:
 *          Initializes the BTHAL Modem Control module.
 *
 * Type:
 *  Synchronous.
 *
 * Parameters:  callback function.
 *          
 * Returns:
 *  BTHAL_STATUS_SUCCESS - The operation succeeded.
 *
 *  BT_STATUS_PENDING - Operation started successfully. Completion will be signalled 
 *                              via an event to the callback
 *  BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_MC_Init(BthalCallBack callback);

/*---------------------------------------------------------------------------
 * BTHAL_MC_Deinit()
 *
 * Brief:   Deinitializes the BTHAL Modem Control module.  
 *      
 * Description:
 *          Deinitializes the BTHAL Modem Control module.
 * 
 * Type:
 *  Synchronous.
 *
 * Parameters:
 *
 * Returns:
 *      BTHAL_STATUS_SUCCESS - The operation succeeded.
 *      BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_MC_Deinit(void);

/*---------------------------------------------------------------------------
 * BTHAL_MC_Register()
 *
 * Brief:   Register a callback for receiving modem related events.  
 *      
 * Description:
 *          Register a callback for receiving modem related events.
 * 
 * Type:
 *  Asynchronous or Synchronous
 *
 * Parameters:
 *  callback [in] - A callback used to inform of MC Events.
 *  userData [in] - 'userData' will be returned to the user upon every BTHAL MC Event.
 *  context [out] - A BTHAL MC Context.
 *
 * Returns:
 *      BTHAL_STATUS_NO_RESOURCES - No more BTHAL MC contexts left.
 *      BTHAL_STATUS_PENDING - The operation has started successfully.
 *      BTHAL_STATUS_FAILED - The operation failed.
 *      BTHAL_STATUS_INVALID_PARM - The 'callback' argument is invalid.
 */
BthalStatus BTHAL_MC_Register(BthalMcCallback callback,
                              void *userData,
                              BthalMcContext **context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_Unregister()
 *
 * Brief:   The user will no longer be notified of modem events.  
 *      
 * Description:
 *          The user will no longer be notified of modem events.
 *
 * Type:
 *  Asynchronous or Synchronous
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context, "context" is set to 0
 *      when Unregister returns.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation has started successfully.
 *  BTHAL_STATUS_FAILED - The operation failed.
 *  BTHAL_STATUS_INVALID_PARM - Invalid parameter.
 */
BthalStatus BTHAL_MC_Unregister(BthalMcContext **context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_AnswerCall()
 *
 * Brief:   Answer an incoming call, "ATA"..  
 *      
 * Description:
 *          Answer an incoming call, "ATA".
 * 
 * Type:
 *  Asynchronous or Synchronous
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_AnswerCall(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_DialNumber()
 *
 * Brief:   Dial the given number, "ATDdd...d".
 *      
 * Description:
 *          Dial the given number, "ATDdd...d".
 *  
 * Type:
 *  Asynchronous or Synchronous
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *  number [in] - The number to dial.
 *  length [in] - The number of characters in the number string.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 *  BTHAL_STATUS_INVALID_PARM - The 'number' argument is invalid.
 */
BthalStatus BTHAL_MC_DialNumber(BthalMcContext *context,
                                const BTHAL_U8 *number,
                                BTHAL_U8 length);

/*---------------------------------------------------------------------------
 * BTHAL_MC_DialMemory()
 *
 * Brief:   Dial a number from a specific memory location.  
 *      
 * Description:
 *          Dial a number from a specific memory location, "ATD>nnn".
 *          NOTE: The phonebook from which the number is taken is platform dependant.
 * 
 * Type:
 *  Asynchronous or Synchronous
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *  entry [in] - A string representing the memory location.
 *  length [in] - The string length.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_DialMemory(BthalMcContext *context,
                                const BTHAL_U8 *entry,
                                BTHAL_U8 length);

/*---------------------------------------------------------------------------
 * BTHAL_MC_HangupCall()
 *
 * Brief:   hang-up active call or reject incoming call.  
 *      
 * Description:
 *          hang-up active call or reject incoming call, "AT+CHUP".
 * 
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_HangupCall(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_GenerateDTMF()
 *
 * Brief:   Generate and send over the network a DTMF tone.  
 *      
 * Description:
 *          Generate and send over the network a DTMF tone, "AT+VTS=<DTMF>".
 * 
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *  dtmf [in] - A value specifying a DTMF code.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_GenerateDTMF(BthalMcContext *context,
                                  BTHAL_I32 dtmf);

/*---------------------------------------------------------------------------
 * BTHAL_MC_DialLastNumber()
 *
 * Brief:   Dial the last number dialed.  
 *      
 * Description:
 *          Dial the last number dialed, "ATD>LD 1"
 * 
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_DialLastNumber(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_HandleCallHoldAndMultiparty()
 *
 * Brief:   Handle hold state of current calls, adding held calls to a multiparty call
 *          and explicit call transfer.
 *      
 * Description:
 *          Handle hold state of current calls, adding held calls to a multiparty call
 *          and explicit call transfer, "AT+CHLD=<n>".
 *  
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *  hfgHold [in] - Describes the desired Hold/Multiparty option.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_HandleCallHoldAndMultiparty(BthalMcContext *context,
                                                 const HfgHold *holdOp);

/*---------------------------------------------------------------------------
 * BTHAL_MC_SetNetworkOperatorStringFormat()
 *
 * Brief:   Sets the format of the Network Operator.  
 *      
 * Description:
 *          Sets the format of the Network Operator string received 
 *          after calling BTHAL_MC_RequestNetworkOperatorString().
 *          The format is long alphanumeric. "AT+COPS=3,0"
 *
 * Type:
 *  Asynchronous or Synchronous
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.    
 */
BthalStatus BTHAL_MC_SetNetworkOperatorStringFormat(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_SetExtendedErrors()
 *
 * Brief:   Enables or disables the usage of extended errors.  
 *      
 * Description:
 *          Enables or disables the usage of extended errors, "AT+CMEE=<n>"
 * 
 * Type:
 *  Asynchronous or synchronous
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *  enabled [in] - If TRUE extended errors are enabled,
 *      otherwise, disabled.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.    
 */
BthalStatus BTHAL_MC_SetExtendedErrors(BthalMcContext *context,
                                       BTHAL_BOOL enabled);

/*---------------------------------------------------------------------------
 * BTHAL_MC_SetClipNotification()
 *
 * Brief:   Enables or disabled calling line identification presentation.
 *      
 * Description:
 *          Enables or disabled calling line identification presentation.
 *          "AT+CLIP=<n>".
 * 
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 *  enabled [in] - If TRUE calling line identification notification
 *      is enabled, otherwise, disabled.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.    
 */
BthalStatus BTHAL_MC_SetClipNotification(BthalMcContext *context,
                                         BTHAL_BOOL enabled);

/*---------------------------------------------------------------------------
 * BTHAL_MC_SetIndicatorEventsReporting()
 *
 * Brief:   Enables or disables indicator events reporting.
 *      
 * Description:
 *          Enables or disables indicator events reporting,
 *          "AT+CMER=3,0,0,1" or "AT+CMER=3,0,0,0".
 * 
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *  enabled [in] - If TRUE indicator events reporting is enabled,
 *      otherwise, disabled.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.    
 */
BthalStatus BTHAL_MC_SetIndicatorEventsReporting(BthalMcContext *context,
                                                 BTHAL_BOOL enabled);

/*---------------------------------------------------------------------------
 * BTHAL_MC_RequestCallHoldAndMultipartyOptions()
 *
 * Brief:   Requests the available options for call hold and multiparty handling.
 *      
 * Description:
 *          Requests the available options for call hold and multiparty handling,
 *          Used in BTHAL_MC_HandleCallHoldAndMultiParty(), "AT+CHLD=?".
 * 
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.    
 */
BthalStatus BTHAL_MC_RequestCallHoldAndMultipartyOptions(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_RequestCurrentIndicatorsValue()
 *
 * Brief:   Requests the current status of the VG Indicators.
 *      
 * Description:
 *          Requests the current status of the VG Indicators, "AT+CIND?".
 * 
 * Type:
 *  Asynchronous or Synchronous
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_RequestCurrentIndicatorsValue(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_RequestSupportedIndicatorsRange()
 *
 * Brief:   Request a mapping between each indicator supported by the VG
 *          And his matching index and value range.
 *      
 * Description:
 *          Request a mapping between each indicator supported by the VG
 *          And his matching index and value range.
 * 
 * Type:
 *  Asynchronous or Synchronous
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_RequestSupportedIndicatorsRange(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_RequestCurrentCallsList()
 *
 * Brief:   Request a list of current calls in the VG
 *      
 * Description:
 *          Request a list of current calls in the VG. "AT+CLCC".
 * 
 * Type:
 *  Asynchronous or Synchronous
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_RequestCurrentCallsList(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_RequestNetworkOperatorString()
 *
 * Brief:   Request a string specifying the network operator.
 *      
 * Description:
 *          Request a string specifying the network operator.
 * 
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_RequestNetworkOperatorString(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_RequestSubscriberNumberInformation()
 *
 * Brief:   Requests information regarding subscriber number.
 *      
 * Description:
 *          Requests information regarding subscriber number, "AT+CNUM"
 * 
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_RequestSubscriberNumberInformation(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_ScheduleRingClipEvent()
 *
 * Brief:   Schedule ring & clip events.
 *      
 * Description:
 *          Schedule ring & clip events for modems that don't generate
 *          more then 1 clip & ring for an incoming call.
 *          The clip & ring will be generated every 4 sec' untill 
 *          the call will be answered or hanged.
 * 
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *
 * Returns:
 *  BTHAL_STATUS_SUCCESS - The event was scheduled OK.
 *  BTHAL_STATUS_FAILED - failure in scheduling the event.
 */
BthalStatus BTHAL_MC_ScheduleRingClipEvent(void);

/*---------------------------------------------------------------------------
 * BTHAL_MC_RequestResponseAndHoldStatus()
 *
 * Brief:   Request the current status of "Response and Hold".
 *      
 * Description:
 *          Request the current status of "Response and Hold", "AT+BTRH?".
 *
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_RequestResponseAndHoldStatus(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_SetResponseAndHold()
 *
 * Brief:   Sets the response and hold status for incoming calls.
 *      
 * Description:
 *          Sets the response and hold status for incoming calls,
 *          "AT+BTRH=<n>".
 *
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *  option [in] - Define which operation to perform.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */

#if HFG_USE_RESP_HOLD == XA_ENABLED

BthalStatus BTHAL_MC_SetResponseAndHold(BthalMcContext *context,
                                           HfgResponseHold option);

#endif /*HFG_USE_RESP_HOLD == XA_ENABLED*/

/*---------------------------------------------------------------------------
 * BTHAL_MC_SetPhonebook()
 *
 * Brief:
 *  Selects a phonebook
 *      
 * Description:
 *  Selects a phonebook to be used with read and find phonebook entries commands.
 *  "AT+CPBS=<phonebook>".
 *
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *  phonebook [in] - The phonebook to select.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */

#if AT_PHONEBOOK == XA_ENABLED

BthalStatus BTHAL_MC_SetPhonebook(BthalMcContext *context,
                                        AtPbStorageType phonebook);

#endif /*AT_PHONEBOOK == XA_ENABLED*/

/*---------------------------------------------------------------------------
 * BTHAL_MC_RequestSelectedPhonebook()
 *
 * Brief:
 *  Requests the currently selected phonebook
 *      
 * Description:
 *  Requests the currently selected phonebook: "AT+CPBS?"
 *
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_RequestSelectedPhonebook(BthalMcContext *context);

/*---------------------------------------------------------------------------
 * BTHAL_MC_RequestSelectedPhonebook()
 *
 * Brief:
 *  Requests the legal phonebook values
 *      
 * Description:
 *  Requests the legal phonebook values that can be used with BTHAL_MC_SetPhonebook(),
 *  "AT+CPBS=?"
 *
 * Type:
 *  Asynchronous or Synchronous.
 *
 * Parameters:
 *  context [in] - A BTHAL MC Context.
 *
 * Returns:
 *  BTHAL_STATUS_PENDING - The operation started successfully.
 *  BTHAL_STATUS_BUSY - The context is still processing another request. 
 *  BTHAL_STATUS_FAILED - The operation failed to start.
 */
BthalStatus BTHAL_MC_RequestSupportedPhonebooks(BthalMcContext *context);

BthalStatus BTHAL_MC_ReadPhonebook(BthalMcContext *context,
                                            BTHAL_U16 index1,
                                            BTHAL_U16 index2);

BthalStatus BTHAL_MC_RequestPhonebookSupportedIndices(BthalMcContext *context);

BthalStatus BTHAL_MC_FindPhonebook(BthalMcContext *context,
                                            const char *findText);

BthalStatus BTHAL_MC_SetCharSet(BthalMcContext *context,
                            const char *charSet);

BthalStatus BTHAL_MC_RequestSelectedCharSet(BthalMcContext *context);

BthalStatus BTHAL_MC_RequestSupportedCharSets(BthalMcContext *context);

#endif

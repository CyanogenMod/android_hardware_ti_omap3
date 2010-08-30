/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



/** \file   ccm_adapt.h 
 *  \brief  Connectivite Chip Manager (CCM) adapter for Host Controller
 *          Interface (HCI)	API-s definition
 * 
 *  HCI Adapter module provides:
 * 
 * 	- Interface between CCM and HCI Adapter.
 *  - Provides clients registration for HCI message (ACL/SCO/Events) reception.
 *  - Forwards the event message to registered client
 *
 *  \see    ccm_adapt.c and mcp_hci_adapt.c
 */

#ifndef __CCM_ADAPT_API_H__
#define __CCM_ADAPT_API_H__

#include "mcpf_defs.h"
#include "mcp_hal_types.h"
#include "mcp_hal_defs.h"
#include "mcp_hal_hci.h"


/******************************************************************************
 * Defines
 ******************************************************************************/


/******************************************************************************
 * Macros
 ******************************************************************************/


/******************************************************************************
 * Types
 ******************************************************************************/

/*------------------------------------------------------------------------------
 * CcmaHciOpcode type
 *
 *     An HCI opcode that is part of an HCI command.
 *
 *  These opcodes are passed in the calls to CCMA_MngrSendRadioCommand and
 *  CCMA_SendHciCommand.
 */
typedef enum tagCcmaHciOpcode
{
	CCMA_HCI_CMD_HCC_RESET =                                    0x0C03,
    CCMA_HCI_HOST_NUM_COMPLETED_PACKETS =                       0x0C35,
    CCMA_HCI_CMD_READ_LOCAL_VERSION =                           0x1001,
    CCMA_HCI_CMD_VS_WRITE_CODEC_CONFIGURE =                     0xfd06,
    CCMA_HCI_CMD_READ_MODIFY_WRITE_HARDWARE_REGISTER =          0xfd09,
    CCMA_HCI_CMD_WRITE_TO_FM =                                  0xfd35,
    CCMA_HCI_CMD_VS_WRITE_I2C_REGISTER_ENHANCED =               0xfd36,
    CCMA_HCI_CMD_FM_OVER_BT_6350 =                              0xfd61,
    CCMA_HCI_CMD_SET_NARROW_BAND_VOICE_PATH =                   0xfd94,
    CCMA_HCI_CMD_VS_BT_IP1_1_SET_FM_AUDIO_PATH =                0xfd95,
    /*  Assisted A2DP (A3DP) vendor-specific HCI commands */
    CCMA_HCI_CMD_VS_A3DP_AVPR_ENABLE =                          0xfd92,
    CCMA_HCI_CMD_VS_A3DP_OPEN_STREAM =                          0xfd8c,
    CCMA_HCI_CMD_VS_A3DP_CLOSE_STREAM =                         0xfd8d,
    CCMA_HCI_CMD_VS_A3DP_CODEC_CONFIGURATION =                  0xfd8e,
    CCMA_HCI_CMD_VS_A3DP_MULTI_SNK_CONFIGURATION =              0xfd96,
    CCMA_HCI_CMD_VS_A3DP_START_STREAM =                         0xfd8f,
    CCMA_HCI_CMD_VS_A3DP_STOP_STREAM =                          0xfd90
} CcmaHciOpcode;

/*---------------------------------------------------------------------------
 * CcmaHciEventType type
 *
 *     The type of HCI event that is received from the HCI layer.
 */
typedef enum tagCcmaHciEventType
{
    CCMA_HCI_EVENT_INQUIRY_COMPLETE =                           0x01,
    CCMA_HCI_EVENT_INQUIRY_RESULT =                             0x02,
    CCMA_HCI_EVENT_CONNECT_COMPLETE =                           0x03,
    CCMA_HCI_EVENT_CONNECT_REQUEST =                            0x04,
    CCMA_HCI_EVENT_DISCONNECT_COMPLETE =                        0x05,
    CCMA_HCI_EVENT_AUTH_COMPLETE =                              0x06,
    CCMA_HCI_EVENT_REMOTE_NAME_REQ_COMPLETE =                   0x07,
    CCMA_HCI_EVENT_ENCRYPT_CHNG =                               0x08,
    CCMA_HCI_EVENT_CHNG_CONN_LINK_KEY_COMPLETE =                0x09,
    CCMA_HCI_EVENT_MASTER_LINK_KEY_COMPLETE =                   0x0a,
    CCMA_HCI_EVENT_READ_REMOTE_FEATURES_COMPLETE =              0x0b,
    CCMA_HCI_EVENT_READ_REMOTE_VERSION_COMPLETE =               0x0c,
    CCMA_HCI_EVENT_QOS_SETUP_COMPLETE =                         0x0d,
    CCMA_HCI_EVENT_COMMAND_COMPLETE =                           0x0e,
    CCMA_HCI_EVENT_COMMAND_STATUS =                             0x0f,
    CCMA_HCI_EVENT_HARDWARE_ERROR =                             0x10,
    CCMA_HCI_EVENT_FLUSH_OCCURRED =                             0x11,
    CCMA_HCI_EVENT_ROLE_CHANGE =                                0x12,
    CCMA_HCI_EVENT_NUM_COMPLETED_PACKETS =                      0x13,
    CCMA_HCI_EVENT_MODE_CHNG =                                  0x14,
    CCMA_HCI_EVENT_RETURN_LINK_KEYS =                           0x15,
    CCMA_HCI_EVENT_PIN_CODE_REQ =                               0x16,
    CCMA_HCI_EVENT_LINK_KEY_REQ =                               0x17,
    CCMA_HCI_EVENT_LINK_KEY_NOTIFY =                            0x18,
    CCMA_HCI_EVENT_LOOPBACK_COMMAND =                           0x19,
    CCMA_HCI_EVENT_DATA_BUFFER_OVERFLOW =                       0x1a,
    CCMA_HCI_EVENT_MAX_SLOTS_CHNG =                             0x1b,
    CCMA_HCI_EVENT_READ_CLOCK_OFFSET_COMPLETE =                 0x1c,
    CCMA_HCI_EVENT_CONN_PACKET_TYPE_CHNG =                      0x1d,
    CCMA_HCI_EVENT_QOS_VIOLATION =                              0x1e,
    CCMA_HCI_EVENT_PAGE_SCAN_MODE_CHANGE =                      0x1f,
    CCMA_HCI_EVENT_PAGE_SCAN_REPETITION_MODE =                  0x20,
    CCMA_HCI_EVENT_FLOW_SPECIFICATION_COMPLETE =                0x21,
    CCMA_HCI_EVENT_INQUIRY_RESULT_WITH_RSSI =                   0x22,
    CCMA_HCI_EVENT_READ_REMOTE_EXT_FEAT_COMPLETE =              0x23,
    CCMA_HCI_EVENT_FIXED_ADDRESS =                              0x24,
    CCMA_HCI_EVENT_ALIAS_ADDRESS =                              0x25,
    CCMA_HCI_EVENT_GENERATE_ALIAS_REQ =                         0x26,
    CCMA_HCI_EVENT_ACTIVE_ADDRESS =                             0x27,
    CCMA_HCI_EVENT_ALLOW_PRIVATE_PAIRING =                      0x28,
    CCMA_HCI_EVENT_ALIAS_ADDRESS_REQ =                          0x29,
    CCMA_HCI_EVENT_ALIAS_NOT_RECOGNIZED =                       0x2a,
    CCMA_HCI_EVENT_FIXED_ADDRESS_ATTEMPT =                      0x2b,
    CCMA_HCI_EVENT_SYNC_CONNECT_COMPLETE =                      0x2c,
    CCMA_HCI_EVENT_SYNC_CONN_CHANGED =                          0x2d,
    CCMA_HCI_EVENT_HCI_SNIFF_SUBRATING =                        0x2e,
    CCMA_HCI_EVENT_EXTENDED_INQUIRY_RESULT =                    0x2f,
    CCMA_HCI_EVENT_ENCRYPT_KEY_REFRESH_COMPLETE =               0x30,
    CCMA_HCI_EVENT_IO_CAPABILITY_REQUEST =                      0x31,
    CCMA_HCI_EVENT_IO_CAPABILITY_RESPONSE =                     0x32,
    CCMA_HCI_EVENT_USER_CONFIRMATION_REQUEST =                  0x33,
    CCMA_HCI_EVENT_USER_PASSKEY_REQUEST =                       0x34,
    CCMA_HCI_EVENT_REMOTE_OOB_DATA_REQUEST =                    0x35,
    CCMA_HCI_EVENT_SIMPLE_PAIRING_COMPLETE =                    0x36,
    CCMA_HCI_EVENT_LINK_SUPERV_TIMEOUT_CHANGED =                0x38,
    CCMA_HCI_EVENT_ENHANCED_FLUSH_COMPLETE =                    0x39,
    CCMA_HCI_EVENT_USER_PASSKEY_NOTIFICATION =                  0x3a,
    CCMA_HCI_EVENT_KEYPRESS_NOTIFICATION =                      0x3b,
    CCMA_HCI_EVENT_REMOTE_HOST_SUPPORTED_FEATURES =             0x3c,
                                                                
    CCMA_HCI_EVENT_FM_EVENT =                                   0xF0,
    CCMA_HCI_EVENT_BLUETOOTH_LOGO =                             0xFE,
    CCMA_HCI_EVENT_VENDOR_SPECIFIC =                            0xFF
} CcmaHciEventType;

/*---------------------------------------------------------------------------
 * CcmaHciEventStatus type
 *
 *     The first parameter in an HCI event often contains a "status" value.
 *     0x00 means pending or success, according to the event type, but
 *     other values provide a specific reason for the failure. These
 *     values are listed below.
 */
typedef enum tagCcmaHciEventStatus
{
    CCMA_HCI_EVENT_STATUS_SUCCESS =                             0x00,
    CCMA_HCI_EVENT_STATUS_UNKNOWN_HCI_CMD =                     0x01,
    CCMA_HCI_EVENT_STATUS_NO_CONNECTION =                       0x02,
    CCMA_HCI_EVENT_STATUS_HARDWARE_FAILURE =                    0x03,
    CCMA_HCI_EVENT_STATUS_PAGE_TIMEOUT =                        0x04,
    CCMA_HCI_EVENT_STATUS_AUTH_FAILURE =                        0x05,
    CCMA_HCI_EVENT_STATUS_KEY_MISSING =                         0x06,
    CCMA_HCI_EVENT_STATUS_MEMORY_FULL =                         0x07,
    CCMA_HCI_EVENT_STATUS_CONN_TIMEOUT =                        0x08,
    CCMA_HCI_EVENT_STATUS_MAX_NUM_CONNS =                       0x09,
    CCMA_HCI_EVENT_STATUS_MAX_SCO_CONNS =                       0x0a,
    CCMA_HCI_EVENT_STATUS_ACL_ALREADY_EXISTS =                  0x0b,
    CCMA_HCI_EVENT_STATUS_CMD_DISALLOWED =                      0x0c,
    CCMA_HCI_EVENT_STATUS_HOST_REJ_NO_RESOURCES =               0x0d,
    CCMA_HCI_EVENT_STATUS_HOST_REJ_SECURITY =                   0x0e,
    CCMA_HCI_EVENT_STATUS_HOST_REJ_PERSONAL_DEV =               0x0f,
    CCMA_HCI_EVENT_STATUS_HOST_TIMEOUT =                        0x10,
    CCMA_HCI_EVENT_STATUS_UNSUPP_FEATUR_PARM_VAL =              0x11,
    CCMA_HCI_EVENT_STATUS_INVAL_HCI_PARM_VAL =                  0x12,
    CCMA_HCI_EVENT_STATUS_CONN_TERM_USER_REQ =                  0x13,
    CCMA_HCI_EVENT_STATUS_CONN_TERM_LOW_RESOURCES =             0x14,
    CCMA_HCI_EVENT_STATUS_CONN_TERM_POWER_OFF =                 0x15,
    CCMA_HCI_EVENT_STATUS_CONN_TERM_LOCAL_HOST =                0x16,
    CCMA_HCI_EVENT_STATUS_REPEATED_ATTEMPTS =                   0x17,
    CCMA_HCI_EVENT_STATUS_PAIRING_DISALLOWED =                  0x18,
    CCMA_HCI_EVENT_STATUS_UNKNOWN_LMP_PDU =                     0x19,
    CCMA_HCI_EVENT_STATUS_UNSUPP_REMOTE_FEATURE =               0x1a,
    CCMA_HCI_EVENT_STATUS_SCO_OFFSET_REJECTED =                 0x1b,
    CCMA_HCI_EVENT_STATUS_SCO_INTERVAL_REJECTED =               0x1c,
    CCMA_HCI_EVENT_STATUS_SCO_AIR_MODE_REJECTED =               0x1d,
    CCMA_HCI_EVENT_STATUS_INVALID_LMP_PARM =                    0x1e,
    CCMA_HCI_EVENT_STATUS_UNSPECIFIED_ERROR =                   0x1f,
    CCMA_HCI_EVENT_STATUS_UNSUPP_LMP_PARM =                     0x20,
    CCMA_HCI_EVENT_STATUS_ROLE_CHANGE_DISALLOWED =              0x21,
    CCMA_HCI_EVENT_STATUS_LMP_RESPONSE_TIMEDOUT =               0x22,
    CCMA_HCI_EVENT_STATUS_LMP_ERR_TRANSACT_COLL =               0x23,
    CCMA_HCI_EVENT_STATUS_LMP_PDU_DISALLOWED =                  0x24,
    CCMA_HCI_EVENT_STATUS_ENCRYPTN_MODE_UNACCEPT =              0x25,
    CCMA_HCI_EVENT_STATUS_UNIT_KEY_USED =                       0x26,
    CCMA_HCI_EVENT_STATUS_QOS_NOT_SUPPORTED =                   0x27,
    CCMA_HCI_EVENT_STATUS_INSTANT_PASSED =                      0x28,
    CCMA_HCI_EVENT_STATUS_PAIRING_W_UNIT_KEY_UNSUPP =           0x29,
    CCMA_HCI_EVENT_STATUS_DIFFERENT_TRANSACTION_COLLISION =     0x2a,
    CCMA_HCI_EVENT_STATUS_INSUFF_RESOURCES_FOR_SCATTER_MODE =   0x2b,
    CCMA_HCI_EVENT_STATUS_QOS_UNACCEPTABLE_PARAMETER =          0x2c,
    CCMA_HCI_EVENT_STATUS_QOS_REJECTED =                        0x2d,
    CCMA_HCI_EVENT_STATUS_CHANNEL_CLASSIF_NOT_SUPPORTED =       0x2e,
    CCMA_HCI_EVENT_STATUS_INSUFFICIENT_SECURITY =               0x2f,
    CCMA_HCI_EVENT_STATUS_PARAMETER_OUT_OF_MANDATORY_RANGE =    0x30,
    CCMA_HCI_EVENT_STATUS_SCATTER_MODE_NO_LONGER_REQUIRED =     0x31,
    CCMA_HCI_EVENT_STATUS_ROLE_SWITCH_PENDING =                 0x32,
    CCMA_HCI_EVENT_STATUS_SCATTER_MODE_PARM_CHNG_PENDING =      0x33,
    CCMA_HCI_EVENT_STATUS_RESERVED_SLOT_VIOLATION =             0x34,
    CCMA_HCI_EVENT_STATUS_SWITCH_FAILED =                       0x35,
    CCMA_HCI_EVENT_STATUS_EXTENDED_INQ_RESP_TOO_LARGE =         0x36,
    CCMA_HCI_EVENT_STATUS_SECURE_SIMPLE_PAIR_NOT_SUPPORTED =    0x37
} CcmaHciEventStatus;

/*---------------------------------------------------------------------------
 * CcmaTranParms type
 *
 *  Used to store transport-specific configuration parameters that are set
 *  during initialization
 */
typedef struct tagCcmaTranParms
{
    McpUint tranType;
    
    union
    {
        struct tagUartParms
        {
            McpU32      speed;
            McpUint     flowControl;
        } uartParms;
    } parms;
} CcmaTranParms;

/*---------------------------------------------------------------------------
 * CcmaStatus type
 *
 *  Status codes that are used to indicate operations results
 */
typedef enum tagCcmaStatus
{
    CCMA_STATUS_SUCCESS = MCP_HAL_STATUS_SUCCESS,
    CCMA_STATUS_FAILED = MCP_HAL_STATUS_FAILED,
    CCMA_STATUS_PENDING = MCP_HAL_STATUS_PENDING,
    CCMA_STATUS_NO_RESOURCES = MCP_HAL_STATUS_NO_RESOURCES,
    CCMA_STATUS_INTERNAL_ERROR = MCP_HAL_STATUS_INTERNAL_ERROR,
    CCMA_STATUS_IMPROPER_STATE = MCP_HAL_STATUS_IMPROPER_STATE
} CcmaStatus;

/*---------------------------------------------------------------------------
 * CcmaMngrEventType type
 *
 *  Type of event that the layer sends its manager
 */
typedef enum tagCcmaMngrEventType
{
    CCMA_MNGR_EVENT_TRANPORT_ON_COMPLETED,
    CCMA_MNGR_EVENT_SET_TRANS_PARMS_COMPLETED,
    CCMA_MNGR_EVENT_SEND_RADIO_CMD_COMPLETED,
    CCMA_MNGR_EVENT_HCI_CONFIG_COMPLETED,
    CCMA_MNGR_EVENT_TRANPORT_OFF_COMPLETED 
} CcmaMngrEventType;

/*---------------------------------------------------------------------------
 * CcmaHciEventData type
 *
 *  HCI data that was received in an HCI event that is to be forwarded 
 *  to a layer user (manager or client)
 */
typedef struct tagCcmaHciEventData
{
    CcmaHciEventType    eventType;       /* Event ending HCI operation */
    McpU8               parmLen;         /* Length of HCI event parameters */
    McpU8               *parms;          /* HCI event parameters */
} CcmaHciEventData;

/*---------------------------------------------------------------------------
 * CcmaMngrEvent type
 *
 *  The data that is passed in an event to a manager
 */
typedef struct tagCcmaMngrEvent
{
    handle_t            reportingObj;
    CcmaMngrEventType   type;
    CcmaStatus          status;

    union
    {
        /* Applicable when type == CCMA_MNGR_EVENT_SEND_RADIO_CMD_COMPLETED */
        CcmaHciEventData hciEventData;
    } data;
} CcmaMngrEvent;

/*---------------------------------------------------------------------------
 * CcmaClientEventType type
 *
 *  Type of event that the layer sends a client
 */
typedef enum tagCcmaClientEventType
{
    CCMA_CLIENT_EVENT_HCI_CMD_COMPLETE
} CcmaClientEventType;

/*---------------------------------------------------------------------------
 * CcmaClientEvent type
 *
 *  The data that is passed in an event to a client 
 */
typedef struct tagCcmaClientEvent
{
    CcmaClientEventType  type;
    CcmaStatus           status;

     /* This field is used to attach user information to HCI command  */
    void                 *pUserData;

    union
    {
        /* Applicable when type == CCMA_CLIENT_EVENT_HCI_CMD_COMPLETE */
        CcmaHciEventData hciEventData;
    } data;
} CcmaClientEvent;

/*---------------------------------------------------------------------------
 * CcmaMngrCb type
 *
 *  Function prototype for a manager callback function
 */
typedef void (*CcmaMngrCb)(CcmaMngrEvent *event);

/*---------------------------------------------------------------------------
 * CcmaClientHandle type
 *
 *  A handle that uniquely identifies a client in the interaction with this
 *  layer
 */
typedef void *CcmaClientHandle;

/*--------------------------------------------------------------------------
 * CcmaClientCb type
 *
 *  Function prototype for a client callback function
 */
typedef void (*CcmaClientCb)(CcmaClientEvent *event);


/******************************************************************************
 * Functions
 ******************************************************************************/


/** 
 * \fn     CCMA_Create 
 * \brief  Create CCM adapter object
 * 
 * Allocate and clear the module's object
 * 
 * \note
 * \param	McpHalChipId  - chip id 
 * \param	mngrCb 		  - manager call back
 * \return 	Handle of the allocated CCMA object, NULL if allocation failed 
 * \sa     	CCMA_Destroy
 */ 
handle_t CCMA_Create(const handle_t hMcpf,
                     const McpHalChipId chipId,
                     const CcmaMngrCb mngrCb);

/** 
 * \fn     CCMA_Destroy 
 * \brief  Destroy CCM adapter object
 * 
 * Deallocate CCM adapter object
 * 
 * \note
 * \param	hCcma - handle to CCMA object
 * \return 	void
 * \sa     	CCMA_Create
 */ 
void CCMA_Destroy (handle_t hCcmaObj);

/** 
 * \fn     CCMA_changeCallBackFun 
 * \brief  Change manager call back function
 * 
 * The function changes the registered mngrCb callback function in CCMA object
 * 
 * \note
 * \param	hCcma  - handle to CCMA object
 * \param	newCb  - new call back function to set 
 * \param	oldCb  - pointer to call back function to return the current call back 
 * \return 	void
 * \sa     	CCMA_Create
 */ 
void CCMA_changeCallBackFun(handle_t hCcma, CcmaMngrCb newCb, CcmaMngrCb *oldCb);

/** 
 * \fn     CCMA_TransportOn 
 * \brief  Start transport layer initialization 
 * 
 * The function starts transport initialization sequence by sending HCI reset command 
 * through HCI Adapter. 
 * 
 * \note
 * \param	hCcma  - handle to CCMA object
 * \return 	status of operation OK or ERROR
 * \sa     	CCMA_Create
 */ 
EMcpfRes CCMA_TransportOn(handle_t hCcma);

/** 
 * \fn     CCMA_TransportOff 
 * \brief  Stops hcia adapter
 * 
 * TThe function clears HCI Adapter queue, invokes mngrCb function to report 
 * De-init Complete event to CCM upon function termination. 
 * 
 * \note
 * \param	hCcma  - handle to CCMA object
 * \return 	status of operation OK or ERROR
 * \sa     	CCMA_Create
 */ 
EMcpfRes CCMA_TransportOff(handle_t hCcma);

/** 
 * \fn     CCMA_MngrSendRadioCommand 
 * \brief  Send HCI command over transport layer
 * 
 * The function sends HCI command with specified parameter to HCI Adapter module 
 * with mngrCb call back function for command completion event 
 * 
 * \note
 * \param	hCcma  - handle to CCMA object
 * \return 	status of operation OK or ERROR
 * \sa     	CCMA_Create
 */ 
EMcpfRes CCMA_MngrSendHciCommand(handle_t *hCcma,    
                                 CcmaHciOpcode hciOpcode, 
                                 McpU8 *hciCmdParms, 
                                 McpU8 hciCmdParmsLen);

/** 
 * \fn     CCMA_SetTranParms 
 * \brief  Configure transport layer
 * 
 * The function configures specified baud rate and flow control of transport layer. 
 * 
 * \note
 * \param	hCcma  - handle to CCMA object
 * \return 	status of operation OK or ERROR
 * \sa     	CCMA_Create
 */ 
EMcpfRes CCMA_SetTranParms(handle_t hCcma, CcmaTranParms *pTranParms);

/** 
 * \fn     CCMA_RegisterClient 
 * \brief  Register a client to CCMA
 * 
 * The function registers a client to send HCI commands and receive command completion 
 * indication by calling of specified client callback function 
 * 
 * \note
 * \param	hCcma  - handle to CCMA object
 * \param	callBackFun  - call back function to invoke on command completion
 * \param	pClientHandle - pointer to client handle to return the value
 * \return 	status of operation OK or ERROR
 * \sa     	CCMA_DeregisterClient
 */ 
EMcpfRes CCMA_RegisterClient(handle_t hCcma,
                             CcmaClientCb callBackFun,
                             CcmaClientHandle *pClientHandle);

/** 
 * \fn     CCMA_DeregisterClient
 * \brief  Deregister a client from CCMA
 * 
 * The function deregisters a client from CCMA. 
 * 
 * \note
 * \param	pClientHandle - pointer to client handle to return the value
 * \return 	status of operation OK or ERROR
 * \sa     	CCMA_RegisterClient
 */ 
EMcpfRes CCMA_DeregisterClient(CcmaClientHandle *pClientHandle);

/** 
 * \fn     CCMA_SendHciCommand 
 * \brief  Send HCI command over transport layer
 * 
 * The function sends HCI command with specified parameter to HCI Adapter module 
 * with client's call back function for command completion event 
 * 
 * \note
 * \param	hClientHandle - handle to registered client
 * \param	eHciOpcode    - HCI command opcode
 * \param	hciCmdParms  - pointer to command parameters
 * \param	uHciCmdParmsLen  - handle to CCMA object
 * \param	uEeventType  - completion event
 * \param	pUserData    - user handle to pass to client callback on completion
 * \return 	status of operation OK or ERROR
 * \sa     	CCMA_Create
 */ 
CcmaStatus CCMA_SendHciCommand(CcmaClientHandle hClientHandle,
							   CcmaHciOpcode eHciOpcode, 
							   McpU8 *pHciCmdParms, 
							   McpU8 uHciCmdParmsLen,
							   McpU8 uEeventType,
							   void *pUserData);

/** 
 * \fn     CCMA_GetChipId
 * \brief  return ccmA chipId
 * 
 * The function provides chipId that corresponds to CCMA handle
 * 
 * \note
 * \param	hCcma - pointer to client handle 
 * \return 	chipId registered
 * \sa     	CCMA_GetChipId
 */ 
McpHalChipId  CCMA_GetChipId(handle_t hCcma);

#ifdef MCP_STK_ENABLE
/** 
 * \fn     CCMA_GetChipVersion 
 * \brief  Returns chip's version
 * 
 * The function returns chip's version: project type, version major, and version
 * minor
 * 
 * \note
 * \param	hCcma  - handle to CCMA object
 * \param	chipVersion  - pointer to a struct for chip's version to return a value
 * \return 	status of operation OK or ERROR
 * \sa     	CCMA_TransportOn
 */ 
CcmaStatus  CCMA_GetChipVersion(handle_t hCcma,
                                THalHci_ChipVersion *chipVersion);
#endif

#endif /*__CCM_ADAPT_API_H__*/


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
/*******************************************************************************\
*
*   FILE NAME:      btl_config.h
*
*   BRIEF:          BTIPS configuration parameters
*
*   DESCRIPTION:
*
*     The constants in this file configure the layers of the BTIPS
*      package. To change a constant, simply change its value in this file
*      and recompile the entire BTIPS package.
*
*     Some constants are numeric, and others indicate whether a feature
*     is enabled (defined as BTL_CONFIG_ENABLED) or disabled (defined as
*     BTL_CONFIG_DISABLED).
*
*     Configuration constants are used to make the package more appropriate for a
*     particular environment. Constants can be modified to allow tradeoffs
*     between code size, RAM usage, functionality, and throughput. 
*
*   AUTHOR:   Udi Ron
*
*****************************************************************************/
#ifndef __BTL_CONFIG_H
#define __BTL_CONFIG_H

/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/

#include "bthal_types.h"


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

/*
*   Indicates that a feature is enabled
*/
#define BTL_CONFIG_ENABLED                                      (1)

/*
*   Indicates that a feature is disabled
*/
#define BTL_CONFIG_DISABLED                                     (0)

/*
 *  Enabled profiles configuration
 */
#define BTL_CONFIG_SPP                  BTL_CONFIG_ENABLED
#define BTL_CONFIG_MDG                  BTL_CONFIG_DISABLED
/* Merge OPPC  and OPPS  to one #define BTL_CONFIG_OPP*/
#define BTL_CONFIG_OPP                  BTL_CONFIG_ENABLED
#define BTL_CONFIG_BPPSND               BTL_CONFIG_ENABLED
#define BTL_CONFIG_PBAPS                BTL_CONFIG_ENABLED
#define BTL_CONFIG_AVRCPTG              BTL_CONFIG_ENABLED
#define BTL_CONFIG_FTPS                 BTL_CONFIG_ENABLED
#define BTL_CONFIG_FTPC                 BTL_CONFIG_DISABLED
#define BTL_CONFIG_BTL_RFCOMM           BTL_CONFIG_ENABLED
#define BTL_CONFIG_BTL_L2CAP            BTL_CONFIG_ENABLED
#define BTL_CONFIG_A2DP                 BTL_CONFIG_ENABLED
#define BTL_CONFIG_SAPS                 BTL_CONFIG_DISABLED
#define BTL_CONFIG_AG                   BTL_CONFIG_ENABLED
#define BTL_CONFIG_VG                   BTL_CONFIG_DISABLED
#define BTL_CONFIG_PAN                  BTL_CONFIG_ENABLED
/* Merge BIP_RSP and BIP_INT to one #define BTL_CONFIG_BIP*/
#define BTL_CONFIG_BIP                  BTL_CONFIG_ENABLED

/* HIDH and HID_BRIDGE should not be enabled together */
#define BTL_CONFIG_HID_BRIDGE           BTL_CONFIG_ENABLED
#define BTL_CONFIG_HIDH                 BTL_CONFIG_DISABLED


/*
*
*   The maximum number of registered BTIPS applications
*   New applications may be registered by calling BTL_RegisterApp.
*/
#define BTL_CONFIG_MAX_NUM_OF_APPS                              (2)

/*
*
*   The maximum number of contexts of any profile that a single
*   application may create.
*/
#define BTL_CONFIG_MAX_NUM_OF_CONTEXTS_PER_APP                  (50)

/*
*
*   The maximum length of an application name (used when creating
*   a new application)
*/
#define BTL_CONFIG_MAX_APP_NAME                                 (20)

/*
 *
 *     The number of devices with which we can connect. This value
 *     represents the maximum size of the piconet in which this device is
 *     the master, plus any master devices for which we are concurrently a
 *     slave. This value includes devices which may be parked, holding,
 *     or sniffing as well any active devices. The value excludes the local
 *     device.
 */
 #define BTL_CONFIG_MAX_NUM_OF_CONNECTED_DEVICES                (3)

/*
 *
 *     Specifies the number of seconds to elapse before a service-specific
 *     access request (see SEC_AccessRequest) is automatically cancelled.
 *     The stack's security manager will cancel the request regardless of
 *     what is causing the delay. Cancelling the request will cause the
 *     security request to fail, which in most cases will prevent the
 *     requesting service from connecting.
 *
 *     This value does not affect authentication or encryption operations
 *     requested directly through APIs such as SEC_AuthenticateLink or
 *     SEC_SetLinkEncryption; it only affects operations initated through
 *     SEC_AccessRequest.
 *
 *     Set this value to 0 to disable security timeouts. When disabled, it
 *     may be necessary to drop the ACL connection in order to safely cancel
 *     the security requeest.
 *
 *     By default, this value is set to 80 seconds. Although any time value
 *     may be used, sufficient time should be allowed for the user of both
 *     the local and remote devices to enter PIN codes and select
 *     authorization settings if required.
 */
#define BTL_CONFIG_SECURITY_TIMEOUT                             (30)

/*
 *     Defines the number of SCO connections supported by this device.
 *     When BTL_CONFIG_NUM_SCO_CONNS is set to 0, SCO capabilities are disabled
 *     to save code size.
 *
 *     The value must be between 0 and 3.
 */
#define BTL_CONFIG_NUM_SCO_CONNS                                (1)

/*
 *  This constant determines whether low power modes (i.e. SNIFF, PARK and 
 *  HOLD) are handled in a non-standard way, including three modifications:
 *  1. Low power modes are disabled during A2DP streaming (the BB rejects all 
 *     SNIFF/PARK/HOLD requests from the peer device)
 *  2. An attempt to exit sniff is done when start A2DP stream is requested
 *  3. An attempt to exit sniff is done before disconnecting A2DP / HF/HS-SLC
 *     connections
 *
 *  By default, this constant is DISABLED, since it's recommended by the SIG white paper 
 *  to enter SNIFF mode during A2DP streaming (even with data).
 *
 *  This flag is enabled for smotther handling of some SEMC devices
 */
#define BTL_CONFIG_LOW_POWER_MODES_NON_STANDARD_HANDLING                (BTL_CONFIG_DISABLED)

/*
 * BMG
 *
 *     Represents configuration parameters for BMG module.
 */

/*
 *   The maximum number of BMG contexts to be shared among all of the 
 *   applications (rather than per application)
 */
#define BTL_CONFIG_BMG_MAX_NUM_OF_BMG_CONTEXTS                  (10)

/*
 *  The configured value limits the number of concurrent calls to
 *  BTL_BMG_SendHciCommand
 */
#define BTL_CONFIG_BMG_MAX_NUM_OF_CONCURRENT_HCI_COMMANDS       (3 + \
                                                                 BTL_CONFIG_BMG_MAX_NUM_OF_CONCURRENT_FLUSH_REQUESTS)

/*
 *  The configured value limits the number of filtered responses that may be
 *  specified in the call to BTL_BMG_SearchByCod
 */
#define BTL_CONFIG_BMG_MAX_NUM_OF_RESPONSES_FOR_SEARCH_BY_COD   (20)

/*
 *  The configured value limits the number of devices that may be specified for
 *  filtering in the call to BTL_BMG_SearchByDevices
 */
#define BTL_CONFIG_BMG_MAX_NUM_OF_DEVICES_FOR_SEARCH_BY_DEVICES (20)

/*
 *  The size (in bytes) of the user data section in a device record.
 *  This is the size of the userData field in the BtlBmgDeviceRecord structure.
 */
#define BTL_BMG_DDB_CONFIG_DEVICE_REC_USER_DATA_SIZE             (10)

/*
 *  The maximum number of entries in the device DB
 */
#define BTL_BMG_DDB_CONFIG_MAX_ENTRIES                           (20)

/*
 *  The default security mode of BTIPS
 *  This value may be changed after BTL BMG initialization by calling
 *  BTL_BMG_SetSecurityMode
 */
#define BTL_CONFIG_BMG_DEFAULT_SECURITY_MODE                     (BSM_SEC_MODE_4)

/*
 *  The configured value limits the number of concurrent calls to BTL_BMG_Bond
 */
#define BTL_CONFIG_BMG_MAX_NUM_OF_CONCURRENT_BOND_REQUESTS       (3)

/*
 *  The configured value limits the number of concurrent calls to BTL_BMG_Flush
 */
#define BTL_CONFIG_BMG_MAX_NUM_OF_CONCURRENT_FLUSH_REQUESTS      (NUM_BT_DEVICES)

/*
 *  The maximum number of attributes in a single record in the Service Discovery
 *  Database.
 */
#define BTL_CONFIG_BMG_MAX_ATTRIBUTES_PER_SERVICE_RECORD        (40)

/*  
 *  The maximum size (in bytes) of a single attribute in a Service Discovery
 *  Database record size. 
 *  The value includes the value field size.
 */
#define BTL_CONFIG_BMG_MAX_ATTRIBUTES_VALUE_SIZE                (100)

/* 
 *  The maximum number of attributes in all of the records in the Service
 *  Discovery Database
 */
#define BTL_CONFIG_BMG_MAX_NUM_OF_ATTRIBUTES                    (99)

/* 
 *  The maximum number of records in the Service Discovery Database. 
 *  This number excludes records that are added internally by the BTIPS package.
 */
#define BTL_CONFIG_BMG_MAX_NUM_OF_USER_SERVICE_RECORDS          (10)

/*
 *  The maximum number of attributes that a single service record result to an
 *  SDP query may contain.
 *  This number limits the value of attributeIdListLen in the calls to
 *  BTL_BMG_ServiceSearchAttributeRequest and BTL_BMG_ServiceAttributeRequest.
 */
#define BTL_CONFIG_BMG_MAX_NUM_OF_ATTRIBUTES_TO_SEARCH          (20)    

/*
 *  The default local device name
 */
#define BTL_CONFIG_BMG_DFLT_DEVICE_NAME                         ((U8*)"TI BT")


/*
 *  The default Accessibility Mode when the local device is CONNECTED to a peer
 *  device
 */
#define BTL_CONFIG_BMG_DFLT_ACCESSIBILITY_MODE_C                (BAM_CONNECTABLE_ONLY)

/*
 *  The default Accessibility Mode when the local device is NOT CONNECTED  to a
 *  peer device
 */
#define BTL_CONFIG_BMG_DFLT_ACCESSIBILITY_MODE_NC               (BAM_CONNECTABLE_ONLY)

/*
 *  The default Inquiry Scan Interval value (N) when the local device is
 *  CONNECTED to a peer device.
 *  The actual value in milliseconds is: N * 0.625 milliseconds
 */
#define BTL_CONFIG_BMG_DFLT_INQ_SCAN_INTERVAL_C                 (0x1000)

/*
 *  The default Inquiry Scan Window value (N) when the local device is CONNECTED
 *  to a peer device.
 *  The actual value in milliseconds is: N * 0.625 milliseconds
 */
#define BTL_CONFIG_BMG_DFLT_INQ_SCAN_WINDOW_C                   (0x12)

/*
 *  The default Page Scan Interval value (N) when the local device is CONNECTED
 *  to a peer device.
 *  The actual value in milliseconds is: N * 0.625 milliseconds
 */
#define BTL_CONFIG_BMG_DFLT_PAGE_SCAN_INTERVAL_C                (0x700)

/*
 *  The default Page Scan Window value (N) when the local device is CONNECTED
 *  to a peer device.
 *  The actual value in milliseconds is: N * 0.625 milliseconds
 */
#define BTL_CONFIG_BMG_DFLT_PAGE_SCAN_WINDOW_C                  (0x12)

/*
 *  The default Inquiry Scan Interval value (N) when the local device is NOT
 *  CONNECTED to a peer device.
 *  The actual value in milliseconds is: N * 0.625 milliseconds
 */
#define BTL_CONFIG_BMG_DFLT_INQ_SCAN_INTERVAL_NC                (0x1000)
/*
 *  If enabled increases the inquiry length parameter while streaming
 */
#define BTL_CONFIG_BMG_MUL_INQ_LENGTH_DURING_STREAMING          (BTL_CONFIG_ENABLED)

/*
 *  The default Inquiry Scan Window value (N) when the local device is NOT
 *  CONNECTED to a peer device.
 *  The actual value in milliseconds is: N * 0.625 milliseconds
 */
#define BTL_CONFIG_BMG_DFLT_INQ_SCAN_WINDOW_NC                  (0x12)

/*
 *  The default Page Scan Interval value (N) when the local device is NOT
 *  CONNECTED to a peer device.
 *  The actual value in milliseconds is: N * 0.625 milliseconds
 */
#define BTL_CONFIG_BMG_DFLT_PAGE_SCAN_INTERVAL_NC               (0x800)

/*
 *  The default Page Scan Window value (N) when the local device is NOT
 *  CONNECTED to a peer device.
 *  The actual value in milliseconds is: N * 0.625 milliseconds
 */
#define BTL_CONFIG_BMG_DFLT_PAGE_SCAN_WINDOW_NC                 (0x12)
/*
 *  LSTO value (N) for AG/VG connections (only!)
 *  The actual value in milliseconds is: N * 0.625 milliseconds
 */
#define BTL_CONFIG_BMG_LINK_SUPERVISION_TIMEOUT_FOR_AG_VG_CON   (0x1f40)
/*
 *  Number of Frames that there will be no override of voice
 */
#define BTL_CONFIG_BMG_NUM_OF_FRAMES_FOR_NO_OVERRIDE_BY_VOICE   (0x0010)
/*
 *  The default Major Service Class of the local device.
 *  Additional Major Service Class bits will be set when services are enabled
 *  in the local device
 */
#define BTL_CONFIG_BMG_DEFAULT_MAJOR_SERVICE_CLASS              (COD_TELEPHONY)

/*
 *  The default Major Device Class of the local device
 */
#define BTL_CONFIG_BMG_DEFAULT_MAJOR_DEVICE_CLASS               (COD_MAJOR_PHONE)

/*
 *  The default Minor Device Class of the local device
 */
#define BTL_CONFIG_BMG_DEFAULT_MINOR_DEVICE_CLASS               (COD_MINOR_PHONE_CELLULAR)


/*
 *  Defines the maximum number of devices that the ME Device Selection
 *  manager can track. If this value is zero, the MEDEV component is
 *  disabled, resulting in a code size savings.
 */
#define BTL_CONFIG_MAX_NUM_KNOWN_DEVICES                        (300)

/*-------------------------------------------------------------------------------
 * Secure Simple Pairing (SSP)
 *
 *     Represents configuration parameters for SSP.
 */

/*
 *  Defines the default authentication requirements which will be used, if no
 *  security record exists for a particular connection. 
 */
#define BTL_CONFIG_SSP_DEFAULT_AUTHENTICATION_REQUIREMENTS      (MITM_PROTECT_NOT_REQUIRED)

/*
 *  If enabled, local device demand MITM protection on incoming authentication 
 *  requests (not incoming connections!), if local and remote IO capabilities 
 *  can support it.
 *
 *  By default, this constant is disabled.
 */
#define BTL_CONFIG_SSP_TRY_MITM_ON_INCOMING_AUTH_REQUESTS       (BTL_CONFIG_ENABLED)

/*
 *  Define the platform's IO capability which will be used for selection of
 *  association model used for authentication during SSP. 
 */
#define BTL_CONFIG_SSP_IO_CAPABILITY_DISPLAY_ONLY               (IO_DISPLAY_ONLY)
#define BTL_CONFIG_SSP_IO_CAPABILITY_DISPLAY_YES_NO             (IO_DISPLAY_YESNO)
#define BTL_CONFIG_SSP_IO_CAPABILITY_KEYBOARD_ONLY              (IO_KEYBOARD_ONLY)
#define BTL_CONFIG_SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT         (IO_NO_IO)

/*-------------------------------------------------------------------------------
 * EIR response (Inquiry scan)
 *
 *     Represents configuration parameters for EIR response.
 */

/*
 *  BTL_CONFIG_BMG_EIR_RSP_ENABLED
 *
 *  On Inquiry scan, if enabled local device will send an EIR. 
 */
#define BTL_CONFIG_BMG_EIR_RSP_ENABLED                          (BTL_CONFIG_ENABLED)

/*
 *  BTL_CONFIG_BMG_EIR_RELIABILITY
 *
 *  Indicates whether using FEC (Forward Error Correction in DM1, DM3, DM5 packets)
 *  in EIR is enabled.
 *  Using FEC increases reliability of the response packet but decreases possible
 *  payload size
 */
#define BTL_CONFIG_BMG_EIR_RELIABILITY                          (BTL_CONFIG_DISABLED)

/*
 *  BTL_CONFIG_BMG_EIR_MAN_SPEC_DATA_ENABLED
 *  
 *  If Enabled the EIR will contain the Manufacturer specific data. 
 */  
#define  BTL_CONFIG_BMG_EIR_MAN_SPEC_DATA_ENABLED               (BTL_CONFIG_ENABLED)

/*
 *  BTL_CONFIG_BMG_EIR_MAN_SPEC_DATA_DEFAULT
 *
 *  The default value of the Manufacturer specific data. 
 *  Manufacturer can change this value. First 2 octets should contain a company
 *  identifier code from the Assigned Numbers - Company Identifiers document.
 */
#define  BTL_CONFIG_BMG_EIR_MAN_SPEC_DATA_DEFAULT               {0x0d, 0x00, 0x20, 0x08}

/*
 *  BTL_CONFIG_BMG_EIR_MAN_SPEC_DATA_MAX_LENGTH
 *
 *  The maximum allowed length of the manufacturer specific data.
 *  If there is manufacturer specific data, size has to be at least 2  
 *  (company identifier code) and maximum size is 238 (max EIR buffer is 240 and
 *  first 2 octets are reserved).
 *  If manufacturer specific data buffer will not be used the size should
 *   be 1.
 */
#define BTL_CONFIG_BMG_EIR_MAN_SPEC_DATA_MAX_LENGTH             (10)

/*
 *  BTL_CONFIG_BMG_EIR_TX_POWER_LEVEL_ENABLED
 *
 *  If Enabled the EIR will contain the TX power level.
 */
#define BTL_CONFIG_BMG_EIR_TX_POWER_LEVEL_ENABLED               (BTL_CONFIG_ENABLED)

/*
 *  BTL_CONFIG_BMG_EIR_DEVICE_NAME_ENABLED
 *
 *  If Enabled the EIR will contain the device name
 */
#define BTL_CONFIG_BMG_EIR_DEVICE_NAME_ENABLED                  (BTL_CONFIG_ENABLED)

/*
 *  BTL_CONFIG_BMG_EIR_SERVICE_CLASS_ENABLED
 *
 *  If Enabled the EIR will contain the service class
 */
#define BTL_CONFIG_BMG_EIR_SERVICE_CLASS_ENABLED                (BTL_CONFIG_ENABLED)

/*-------------------------------------------------------------------------------
 * EIR response (Inquiry)
 *
 *     Represents configuration parameters for EIR inquiry.
 */

/*
 *  BTL_CONFIG_BMG_EIR_INQUIRY_MODE
 *
 *  The Inquiry_Mode configuration parameter defines the inquiry result format:
 *  INQ_MODE_NORMAL = Standard Inquiry Result event format.
 *  INQ_MODE_RSSI = Inquiry Result format with RSSI.
 *  INQ_MODE_EXTENDED = Inquiry Result with RSSI format or Extended Inquiry Result
 *  format.
 *
 *  By default EIR format is supported (INQ_MODE_EXTENDED).
 */
#define BTL_CONFIG_BMG_INQUIRY_MODE                             (INQ_MODE_EXTENDED)

 
/*-------------------------------------------------------------------------------
 * Bluetooth System Coordinator (BSC)
 *
 *     Represents configuration parameters for BSC module.
 */

/*
 *  The maximum number of active services to be shared among all of the 
 *  devices (rather than per device) simultaneously.
 *  Active service is A2DP SRC, AVRCP TG, FTP Client etc.  
 */
#define BTL_CONFIG_BSC_MAX_NUM_OF_ACTIVE_SERVICES               (10)


/*---------------------------------------------------------------------------
 * BTL_HCI_HOST_FLOW_CONTROL constant
 *
 *     Controls whether HCI applies flow control to data received from the
 *     host controller.
 *
 *     When BTL_HCI_HOST_FLOW_CONTROL is enabled, the HCI layer uses
 *     HCC_HOST_NUM_COMPLETED_PACKET commands to periodically tell the
 *     radio hardware how many receive packets it can accept. This is
 *     necessary if the HCI driver's receive buffers could overflow
 *     with incoming data.
 *
 *     When BTL_HCI_HOST_FLOW_CONTROL is disabled, the HCI driver assumes
 *     that it can process data faster than the radio hardware can generate
 *     it.
 */
#define BTL_HCI_HOST_FLOW_CONTROL                               BTL_CONFIG_ENABLED


/*-------------------------------------------------------------------------------
 * A2DP
 *
 *     Represents configuration parameters for A2DP module.
 */

/*
 *  The maximum number of A2DP contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_A2DP_MAX_NUM_OF_CONTEXTS                         (1)

/*
 *  The maximum number of simultaneous SBC streams per A2DP context. 
 *  This actually means how many sinks can be connected simultaneously 
 */
#define BTL_CONFIG_A2DP_MAX_NUM_SBC_STREAMS_PER_CONTEXT             (1)

/*
 *  The maximum number of simultaneous MP3 streams per A2DP context. 
 *  This actually means how many sinks can be connected simultaneously
 */
#define BTL_CONFIG_A2DP_MAX_NUM_MPEG1_2_AUDIO_STREAMS_PER_CONTEXT   (0)

/*
 *  The maximum number of raw media blocks used. 
 *  The media block is from type PCM/SBC/MP3 depending on the type of the
 *  current stream(s)
 *  This number should be greater than the maximal number of blocks defined
 *  in BTHAL_MM.
 */
#define BTL_CONFIG_A2DP_MAX_NUM_RAW_BLOCKS_PER_CONTEXT              (12)

/*
 *  The maximum number of SBC packets in the internal SBC queue 
 */
#define BTL_CONFIG_A2DP_MAX_NUM_SBC_PACKETS_PER_CONTEXT             (40)

/*
 *  The maximum number of MP3 packets in the internal MP3 queue
 */
#define BTL_CONFIG_A2DP_MAX_NUM_MPEG1_2_AUDIO_PACKETS_PER_CONTEXT   (0)

/*
 *      The maximum size of an SBC packet.
 */
#define BTL_A2DP_SBC_MAX_DATA_SIZE                                  (L2CAP_MTU)

/*
 *      The following constants are used ONLY with varying bitpool (with 
 *      built-in SBC encoder).
 *
 *      Each group below determine the following:
 *      1) A threshold from which the bitpool should be adjusted.
 *          The 'moving' value is the number of SBC packets sent to 
 *          A2DP layer (to a certain sink) and NOT returned yet to 
 *          BTL A2DP layer.
 *          If this value is in the range defined by the thresholds, then
 *          the bitpool should be adjusted.
 *      2) The time unit (in ms) for which the bitpool is adjusted 
 *          (e.g. on every 100ms, the bitpool is adjusted).
 *          This value represents the amount of time the 'moving' value
 *          remains in the current range (defined by the thresholds).
 *      3) The amount of change in the bitpool for the specific range.
 *
 *      Note: the values were fined tuned for a specific value of 
 *      BTL_CONFIG_A2DP_MAX_NUM_SBC_PACKETS_PER_CONTEXT = 20 packets.
 *      If this value is changed, the constants MUST be tuned again!
 */
#define BTL_A2DP_LOWER_THRESHOLD                                    (2)
#define BTL_A2DP_LOWER_THRESHOLD_TIME_UNIT                          (1000)
#define BTL_A2DP_LOWER_THRESHOLD_BITPOOL_INCREMENT_DELTA            (2)

#define BTL_A2DP_UPPER_THRESHOLD                                    (3)
#define BTL_A2DP_UPPER_THRESHOLD_TIME_UNIT                          (100)
#define BTL_A2DP_UPPER_THRESHOLD_BITPOOL_DECREMENT_DELTA            (-1)

#define BTL_A2DP_UPPER_THRESHOLD_FAST                               (9)
#define BTL_A2DP_UPPER_THRESHOLD_FAST_TIME_UNIT                     (100)
#define BTL_A2DP_UPPER_THRESHOLD_FAST_BITPOOL_DECREMENT_DELTA       (-2)

#define BTL_A2DP_UPPER_THRESHOLD_URGENT                             (14)
#define BTL_A2DP_UPPER_THRESHOLD_URGENT_TIME_UNIT                   (100)
#define BTL_A2DP_UPPER_THRESHOLD_URGENT_BITPOOL_DECREMENT_DELTA     (-5)

/*
 *  Defines how much playback time (in ms) we should accumulate in Q before
 *  start sending SBC/MP3 packets
 */
#define BTL_A2DP_PREBUFFER_BEFORE_SEND_THRESHOLD                    (100)


/*
 *  Defines whether the internal SBC encoder is included in the build.
 *  In case BTL_CONFIG_A2DP is disabled SBC Encoder code will not be compiled
 */
#if BTL_CONFIG_A2DP == BTL_CONFIG_ENABLED
#define BTL_CONFIG_A2DP_SBC_BUILT_IN_ENCODER                        (BTL_CONFIG_ENABLED)
#else
/* This value should never be change */
#define BTL_CONFIG_A2DP_SBC_BUILT_IN_ENCODER                        (BTL_CONFIG_DISABLED)
#endif /* BTL_CONFIG_A2DP == BTL_CONFIG_ENABLED */

/*
 *  Defines whether the internal sample rate converter is included in the build.
 *  The SRC can covert the following sample frequencies:
 *  1) 8000Hz, 12000Hz, 16000Hz, 24000Hz, 32000Hz to mandatory 48000Hz.
 *  2) 11025Hz, 22050Hz to mandatory 44100Hz.
 *  In case BTL_CONFIG_A2DP is disabled Sample Rate Converter code will not be compiled
 */
#if BTL_CONFIG_A2DP == BTL_CONFIG_ENABLED
#define BTL_CONFIG_A2DP_SAMPLE_RATE_CONVERTER                       (BTL_CONFIG_ENABLED)
#else
/* This value should never be change */
#define BTL_CONFIG_A2DP_SAMPLE_RATE_CONVERTER                       (BTL_CONFIG_DISABLED)
#endif /* BTL_CONFIG_A2DP == BTL_CONFIG_ENABLED */

/*
 *  Defines SRC PCM output block max length in bytes.
 *  This constant is relevant only if BTL_CONFIG_A2DP_SAMPLE_RATE_CONVERTER is enabled.
 *  It should be chosen according to the input PCM block length and the length needed
 *  to contain the output PCM block (see ratios below):
 *  8000Hz  -> 48000Hz (ratio is 6)
 *  12000Hz -> 48000Hz (ratio is 4)
 *  16000Hz -> 48000Hz (ratio is 3)
 *  24000Hz -> 48000Hz (ratio is 2)
 *  32000Hz -> 48000Hz (ratio is 1.5)
 *  11025Hz -> 44100Hz (ratio is 4)
 *  22050Hz -> 44100Hz (ratio is 2)
 *
 *  Explanation: If input PCM block length is X in F frequency with Y ratio, then the 
 *  max PCM block output length should be greater than (X * Y).
 *  Example: Input PCM block length = 1024 bytes
 *           Input PCM block sample frequency = 22050 Hz
 *           => Output PCM block length >= (1024 * 2) = 2048 bytes
 */
#define BTL_CONFIG_A2DP_SRC_PCM_OUTPUT_BLOCK_MAX_LEN                 (40000)

/*-------------------------------------------------------------------------------
 * AVRCPTG
 *
 *     Represents configuration parameters for AVRCPTG module.
 */

/*
 *  The maximum number of AVRCPTG contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_AVRCPTG_MAX_NUM_OF_CONTEXTS                  (1)

/*
 *  The maximum number of AVRCPTG  response packet.
 */
#define BTL_CONFIG_AVRCPTG_MAX_NUM_OF_RSP_PACKET                    (2)

/*
 *  The maximum number of AVRCPTG  MDA PDU response packet.
 */
#define BTL_CONFIG_AVRCPTG_MAX_NUM_OF_MDA_PACKET                    (5)

/*
 *  The maximum number of AVRCPTG channels per context
 */
#define BTL_CONFIG_AVRCPTG_MAX_NUM_CHANNELS_PER_CONTEXT         (1)

/*
 *  This constant determines whether the each AVRCP channel will receive 
 *  AVRCP_EVENT_RECOMMENDED_PAUSE_PLAYING and AVRCP_EVENT_RECOMMENDED_RESUME_PLAYING
 *  events when a new audio channel is about to be opened or was just opened during 
 *  A2DP streaming and when an audio channel was just closed.
 */
#define BTL_CONFIG_AVRCPTG_RECOMMEND_PAUSE_PLAYING_ON_AUDIO     (BTL_CONFIG_ENABLED)

/*
 *  The maximum length of a meta-data media info element (title, album, etc). 
 *
 *  The spec allows up to 65535
 */
#define BTL_CONFIG_AVRCPTG_MAX_MDA_MEDIA_INFO_ELEMENT_SIZE               (128)

/*
 *  The mask for the supported AVRCP events for Androdid platform
 *  The supported events are: AVRCP_ENABLE_PLAY_STATUS_CHANGED
 *  						  AVRCP_ENABLE_TRACK_CHANGED
 *  						  AVRCP_ENABLE_PLAY_POS_CHANGED
 *  						  AVRCP_ENABLE_BATT_STATUS_CHANGED
 *  						  AVRCP_ENABLE_APP_SETTING_CHANGED
 *
 */
#define BTL_AVRCPTG_SUPPORTED_EVENTS_MASK								0x00b3

/*-------------------------------------------------------------------------------
 * BIPINT
 *
 *     Represents configuration parameters for BIP Initiator (BIPINT) module.
 */

/* 
 *  The maximum number of BIP Initiator contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_BIPINT_MAX_NUM_OF_CONTEXTS                   (1)

/*-------------------------------------------------------------------------------
 * Define the memory size, used for parsing the received object data:
 * - Imaging Capabilities
 * - Imaging Properties
 */

/* Max number of Image Format elements in an Imaging Capabilities Object */
#define BTL_CONFIG_BIPINT_MAX_NUM_OF_IMAGE_FORMATS              (5)

/* Max number of Attachment Format elements in an Imaging Capabilities Object */
#define BTL_CONFIG_BIPINT_MAX_NUM_OF_ATTACHMENT_FORMATS         (2)

/* Max number of Variant Encoding elements in an Imaging Properties Object */
#define BTL_CONFIG_BIPINT_MAX_NUM_OF_VARIANT_ENCODINGS          (5)

/* Max number of Attachment elements in an Imaging Properties Object */
#define BTL_CONFIG_BIPINT_MAX_NUM_OF_ATTACHMENTS                (2)

/* Max size (bytes) of a friendly name to be parsed from ImageProperties response 
 * In case of UTF8 this value should be reconsidered as one char in Unicode
 * is represented by 1-4 bytes */
#define BTL_CONFIG_BIPINT_MAX_SIZE_FRIENDLY_NAME                (40)

/* Max size (bytes) of the file name.
 * In case of UTF8 this value should be reconsidered as one char in Unicode
 * is represented by 1-4 bytes */
#define BTL_CONFIG_BIP_MAX_SIZE_FILE_NAME                       (40)

/* 
 *  The maximum length of charset.
 */
#define BTL_CONFIG_BIP_MAX_SIZE_CHARSET                         (40)

/* 
 *  The maximum length of content type
 */
#define BTL_CONFIG_BIP_MAX_SIZE_CONTENT_TYPE                    (40)

/* 
 *  The maximum length of image handle
 */
#define BTL_CONFIG_BIP_IMAGE_HANDLE_LEN                         (7)

 
/*-------------------------------------------------------------------------------
 * BIPRSP
 *
 *     Represents configuration parameters for BIP Responder (BIPRSP) module.
 */

/*
 *  The maximum number of BIP Responder contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_BIPRSP_MAX_NUM_OF_CONTEXTS                   (1)


/*-------------------------------------------------------------------------------
 * BPPSND
 *
 *     Represents configuration parameters for BPPSND module.
 */

/*
 *   The maximum number of BPP Sender contexts to be shared among all of the 
 *   applications (rather than per application)
 */
#define BTL_CONFIG_BPPSND_MAX_NUM_OF_CONTEXTS                   (1)

/*
 *   The printer might open an object channel for referenced objects.
 *   So, before we disconnect ACL, we wait for BTL_CONFIG_BPPSND_DISCONNECT_TIMEOUT ms
 *   for the printer to establish the object channel.
 *   If such channel is established, we disconnect only after the printer disconnect
 *   this object channel.
 *   If no such channel is established, we disconnect when the timer expired.
 *   
 *   This constant can be set to 0, if the sender does not want to wait before 
 *   disconnecting ACL.
 *
 *   Default value is 1 sec.
 */
#define BTL_CONFIG_BPPSND_DISCONNECT_TIMEOUT                    (1000)

/*
 *  The printer name length.
 */
#define BTL_CONFIG_BPPSND_MAX_PRINTER_NAME_LEN                  (20)

/*
 *  The printer location length.
 */
#define BTL_CONFIG_BPPSND_MAX_PRINTER_LOCATION_LEN              (20)

/*
 *  The job name length.
 */
#define BTL_CONFIG_BPPSND_MAX_JOB_NAME_LEN                      (20)

/*
 *  The originating use name length.
 */
#define BTL_CONFIG_BPPSND_MAX_JOB_ORIGINATING_USER_NAME_LEN     (20)

/*
 *  The supported format of the document.
 */
#define BTL_CONFIG_BPPSND_MAX_NUM_OF_SUPPORTED_DOCUMENT_FORMATS (20)

/*
 *  The number of supported format of the image.
 */
#define BTL_CONFIG_BPPSND_MAX_NUM_OF_SUPPORTED_IMAGE_FORMATS    (6)

/*
 *  The maximum supported number of sides.
 */
#define BTL_CONFIG_BPPSND_MAX_NUM_OF_SUPPORTED_SIDES            (4)

/*
 *  The number of supported printer quality.
 */
#define BTL_CONFIG_BPPSND_MAX_NUM_OF_SUPPORTED_PRINT_QUALITY    (4)

/*
 *  The printer name length.
 */
#define BTL_CONFIG_BPPSND_MAX_NUM_OF_SUPPORTED_ORIENTATIONS     (5)

/*
 *  The printer general operator.
 */
#define BTL_CONFIG_BPPSND_MAX_PRINTER_GENERAL_CURRENT_OPERATOR  (20)

/*
 *  The maximum media size.
 */
#define BTL_CONFIG_BPPSND_MAX_MEDIA_SIZE_LEN                    (30)

/*
 *  The maximum length of media type.
 */
#define BTL_CONFIG_BPPSND_MAX_MEDIA_TYPE_LEN                    (30)

/*
 *  The number of supported media sizes.
 */
#define BTL_CONFIG_BPPSND_MAX_NUM_OF_SUPPORTED_MEDIA_SIZES      (10)

/*
 *  The printer name length.
 */
#define BTL_CONFIG_BPPSND_MAX_NUM_OF_SUPPORTED_MEDIA_TYPES      (10)

/*
 *  The maximum length of reference object identifier.
 */
#define BTL_CONFIG_BPPSND_MAX_REF_OBJ_IDENTIFIER_LEN            (255)

/*
 *  The maximum number of media loaded.
 */
#define BTL_CONFIG_BPPSND_MAX_NUM_OF_MEDIA_LOADED               (6)


/*-------------------------------------------------------------------------------
 * FTPS
 *
 *     Represents configuration parameters for FTPS module.
 */

/*
 *  The maximum number of objects (files or folders) that the local FTP Server
 *  may hide from peer FTP clients
 */
#define BTL_CONFIG_FTPS_MAX_NUM_OF_CONTEXTS                     (1)

/*
 *  The maximum number of objects (files or folders) that the local FTP Server
 *  may hide from peer FTP clients
 */
#define BTL_CONFIG_FTPS_MAX_HIDE_OBJECT                         (10)


/*-------------------------------------------------------------------------------
 * FTPC
 *
 *     Represents configuration parameters for FTPC module.
 */
 
/*
 *  The maximum number of FTP Client contexts to be shared among all of the 
 *   applications (rather than per application)
 */
#define BTL_CONFIG_FTPC_MAX_NUM_OF_CONTEXTS                     (1)

/*
 *  The maximum length of a single line in the XML folder listing object that
 *  a peer FTP server returns
 */
#define BTL_CONFIG_FTPC_XML_LINE_SIZE                           (256)


/*-------------------------------------------------------------------------------
 * HFAG + HSAG
 *
 *     Represents configuration parameters for the BTL AG module.
 */

/*------------------------------------------------------------------------------
 * BTL_CONFIG_AG_MAXIMUM_NUMBER_OF_CHANNELS constant
 *
 *  The maximal number of available AG Channels.
 *  Generally more then one connections is not possible.
 */
#define BTL_CONFIG_AG_MAXIMUM_NUMBER_OF_CHANNELS                (2)

 /*
 *  The maximum number of BTL AG contexts that can be created.
 */
#define BTL_CONFIG_AG_MAX_CONTEXTS                              (2)

/*
 *  The maximum number of responses to AT commands that can be concurrently sent.
 */
#define BTL_CONFIG_AG_MAX_RESPONSES                             (10)

/*
 *  The default audio parms to use when establishing audio connection.
 */
#define BTL_CONFIG_AG_DEFAULT_AUDIO_PARMS                       (CMGR_AUDIO_PARMS_S3)

/*
 *  The default audio format used by the HFAG module.
 */
#define BTL_CONFIG_AG_DEFAULT_AUDIO_FORMAT                      (BSAS_DEFAULT)

/* 
 *  Sets the sniff mode, if sniff mode is set to BTL_CONFIG_ENABLED the sniff is
 *  activated, otherwise, sniff is disabled.
 *  This definition is per channel and enabling of the sniff mode requires that
 *  BTL_CONFIG_AG_SNIFF_TIMER >= 0.
 */
#define BTL_CONFIG_AG_SNIFF_MODE                                (BTL_CONFIG_ENABLED)

/* 
 *  Sets the policy for exiting sniff mode on the specified channel. When enabled,
 *  exit sniff mode whenever there is a data to send.
 *  This definition is per channel.
 */
#define BTL_CONFIG_AG_SNIFF_EXIT_ON_SEND_POLICY                 (BTL_CONFIG_DISABLED)

/* 
 *  Sets the policy for exiting sniff mode on the specified channel. When enabled,
 *  exit sniff mode whenever there is a data to send.
 *  This definition is per channel.
 */
#define BTL_CONFIG_AG_SNIFF_EXIT_ON_AUDIO_LINK_POLICY           (BTL_CONFIG_ENABLED)

/* 
 *  Enable sniff mode after a defined timeout in milliseconds.
 *  This is a power saving feature. If this value is defined to -1, then, sniff
 *  mode is disabled.
 */
#define BTL_CONFIG_AG_SNIFF_TIMER                               (2000)

/* 
 *  Minimum interval in BT slots of 0.625 ms for sniff mode, if enabled
 *  (BTL_CONFIG_AG_SNIFF_TIMER >= 0).
 */
#define BTL_CONFIG_AG_SNIFF_MIN_INTERVAL                        (400)

/* 
 *  Minimum interval in BT slots of 0.625 ms for sniff mode, if enabled
 *  (BTL_CONFIG_AG_SNIFF_TIMER >= 0).
 */
#define BTL_CONFIG_AG_SNIFF_MAX_INTERVAL                        (800)

/* 
 *  Number of BT receive slots in 1.25 ms units for sniff attempts in sniff mode,
 *  if enabled (BTL_CONFIG_AG_SNIFF_TIMER >= 0).
 */
#define BTL_CONFIG_AG_SNIFF_ATTEMPT                             (4)

/* 
 *  Number of BT receive slots in 1.25 ms units for sniff timeout in sniff mode,
 *  if enabled (BTL_CONFIG_AG_SNIFF_TIMER >= 0).
 */
#define BTL_CONFIG_AG_SNIFF_TIMEOUT                             (1)

/* 
 *  Maximum Latency parameter in BT slots of 0.625 ms, used to calculate the
 *  maximum sniff subrate that the remote device may use.
 */
#define BTL_CONFIG_AG_SNIFF_SUBRATING_MAX_LATENCY               (1600)

/* 
 *  Minimum base sniff subrate timeout in BT slots of 0.625 ms that the remote
 *  device may use.
 */
#define BTL_CONFIG_AG_SNIFF_SUBRATING_MIN_REMOTE_TIMEOUT        (1600)

/* 
 *  Minimum base sniff subrate timeout in BT slots of 0.625 ms that the local
 *  device may use.
 */
#define BTL_CONFIG_AG_SNIFF_SUBRATING_MIN_LOCAL_TIMEOUT         (1600)

/*  Note: setting both subrate values:
 *  BTL_CONFIG_AG_SNIFF_SUBRATING_MIN_REMOTE_TIMEOUT and
 *  BTL_CONFIG_AG_SNIFF_SUBRATING_MIN_LOCAL_TIMEOUT
 *  to 0 means that sniff mode is enabled without subrating.
 */


/*-------------------------------------------------------------------------------
 * HID
 *
 *     Represents configuration parameters for HID module.
 */

/*
 *  The maximum number of HID contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_HID_MAX_NUM_OF_CONTEXTS                      (1)

/*
 *  The maximum number of HID channels per context
 */
#define BTL_CONFIG_HID_MAX_NUM_CHANNELS_PER_CONTEXT             (3)

/*
 *  The maximum number of interrupts that the HID Host may send the 
 *  peer HID device without receiving an acknowledgment.
 */ 
#define BTL_CONFIG_HID_MAX_NUM_OF_TX_INTERRUPTS                 (20)

/*
 *  The maximum number of transactions that the HID Host may send the 
 *  peer HID device without receiving an acknowledgment.
 */
#define BTL_CONFIG_HID_MAX_NUM_OF_TX_TRANSACTIONS               (20)

/*
 *  The maximum number of TX Reports that the HID Host may send the 
 *  peer HID device without receiving an acknowledgment.
 */
#define BTL_CONFIG_HID_MAX_NUM_OF_TX_REPORTS                    (3)

/*
 *  The maximum number of TX Report Requests that the HID Host may send the 
 *  peer HID device without receiving an acknowledgment.
 */
#define BTL_CONFIG_HID_MAX_NUM_OF_TX_REPORTREQ                  (3)

/*
 *  The maximum number of reconnection attempts that the HID Host will make when
 *  the connection to the HID Device was disconnected.
 */
#define BTL_CONFIG_HID_MAX_NUMBER_OF_RECONNECTION               (3)

/*
 *  The time period in milliseconds between consecutive reconnection attempts
 */
#define BTL_CONFIG_HID_MAX_INTERVAL_RECONNECTION_IN_MS          (5000)


/*-------------------------------------------------------------------------------
 * L2CAP
 *
 *     Represents configuration parameters for L2CAP module.
 */

/*
 *  The maximum number of L2CAP contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_L2CAP_MAX_NUM_OF_CONTEXTS                    (2)

/* 
 *  The total no. of connections (L2CAP channels) that L2CAP can handle
 *  (incoming + outgoing together)                                  
 */
#define BTL_CONFIG_L2CAP_MAX_NUM_CHANNELS                       (8)

/*
 *  The maximum number of channels per context
 */
#define BTL_CONFIG_L2CAP_MAX_NUM_CHANNELS_PER_CONTEXT           (4)

/*
 *  The maximum number of TX packets
 */
#define BTL_CONFIG_L2CAP_MAX_NUM_OF_TX_PACKETS                  (10)

/*---------------------------------------------------------------------------
 * BTL_CONFIG_L2CAP_MTU constant
 *
 *  Defines the largest receivable L2CAP data packet payload, in bytes.
 *  This limitation applies only to packets received from the remote device.
 *
 *  This constant also affects the L2CAP Connectionless MTU. Some profiles
 *  require a minimum Connectionless MTU that is greater than the L2CAP minimum
 *  of 48 bytes. When implementing such a profile it is important that the
 *  BTL_CONFIG_L2CAP_MTU constant be at least as large as the specified
 *  Connectionless MTU size.
 *
 *  This value may range from 48 to 65529. The default value is 672 bytes.
 *
 *  The current value of 2038 was chosen in order to achieve optimized EDR
 *  throughput
 */
#define BTL_CONFIG_L2CAP_MTU                                    (2038)


/*-------------------------------------------------------------------------------
 * MDG
 *
 *     Represents configuration parameters for MDG module.
 */
 
/* 
 *  The maximum number of MDG contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_MDG_MAX_NUM_OF_CONTEXTS                      (1)


/* 
 *  Size of buffer for downloaded data - for the best performance should be not
 *  less than SPPOS_TX_BUF_SIZE which is used for writing data to BT SPP port.
 */
/* Should it be here? Ask Vladi */
#if (BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM == BTL_CONFIG_DISABLED)

#define BTL_CONFIG_MDG_DOWNLOAD_BUF_SIZE                        (SPPBUF_TX_BUF_SIZE)

#endif /* BTHAL_MD_DOWNLOAD_BUF_OWNER_MODEM == BTL_CONFIG_DISABLED */


/*-------------------------------------------------------------------------------
 * OBEX
 *
 *     Represents configuration parameters common for OBEX modules.
 */

/*------------------------------------------------------------------------------
 * BTL_CONFIG_OBEX_OBJECT_NAME_MAX_LEN constant
 *
 *  Defines the maximum number of characters possible in an object, folder name,
 *  or queued Unicode header (including the null-terminator). 
 *  The maximum value is 32,767 (or 0x7FFF). This value must be greater than zero,
 *  however, it will in all likelihood be larger than one, since most filenames
 *  exceed one byte in length.
 *
 *  BTL_CONFIG_OBEX_OBJECT_NAME_MAX_LEN must be less or equal to
 *  BTHAL_FS_MAX_FILE_NAME_LENGTH_CHARS!
 */
#define BTL_CONFIG_OBEX_OBJECT_NAME_MAX_LEN                     (BTHAL_FS_MAX_FILE_NAME_LENGTH_CHARS)

/*------------------------------------------------------------------------------
 * BTL_CONFIG_OBEX_AUTHENTICATION constant
 *
 *  This option enables OBEX layer support for OBEX Authentication Challenge
 *  and Response headers. OBEX Authentication support includes functions 
 *  necessary to build and parse authentication challenge and response
 *  headers. There is also a function for verifying that an authentication
 *  response used the correct password.
 */

#define BTL_CONFIG_OBEX_AUTHENTICATION                          (BTL_CONFIG_ENABLED)

#if BTL_CONFIG_OBEX_AUTHENTICATION == BTL_CONFIG_ENABLED
/*-------------------------------------------------------------------------------
 * OBEX AUTHENTICATION
 *
 *  Represents configuration parameters for OBEX  modules that use OBEX AUTHENTICATION
 *  All the values below are in bytes. In case of UTF8 the size must be reconsidered
 *  according to the size of a char (can be between 1-4)
 */

/* 
 *  The maximum OBEX USER_ID len that can be used in bytes!!!
 *  
 */
#define BTL_CONFIG_OBEX_MAX_USER_ID_LEN                         (20)

/* 
 *  The maximum OBEX PASSWROD  len that can be used 
 *   
 */
#define BTL_CONFIG_OBEX_MAX_PASS_LEN                            (11)

/* 
 *  The maximum OBEX REALM len that can be used 
 *   
 */
#define BTL_CONFIG_OBEX_MAX_REALM_LEN                           (20)

#endif /*BTL_CONFIG_OBEX_AUTHENTICATION == BTL_CONFIG_ENABLED*/


/*-------------------------------------------------------------------------------
 * OPPS
 *
 *     Represents configuration parameters for OPPS module.
 */
 
/* 
 *  The maximum number of OPP Server contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_OPPS_MAX_NUM_OF_CONTEXTS                     (1)


/*-------------------------------------------------------------------------------
 * OPPC
 *
 *     Represents configuration parameters for OPPC module.
 */
 
/*
 *  The maximum number of OPP Client contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_OPPC_MAX_NUM_OF_CONTEXTS                     (1)

/*
 *  Defines how much the OPP client should wait for OBEX Pull or Push operations
 *  (in seconds).
 *  When 0, OPPC OBEX Push or Pull timer is disabled
 */
#define BTL_CONFIG_OPPC_PUSH_PULL_TIMER                         (30)    


/*-------------------------------------------------------------------------------
 * PBAPS
 *
 *     Represents configuration parameters for PBAP module.
 */

/*
 *  The maximum number of PBAP Server contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_PBAPS_MAX_NUM_OF_CONTEXTS                    (1)


/*-------------------------------------------------------------------------------
 * SAPS
 *
 *     Represents configuration parameters for SAP server module.
 */
 
/* 
 *  The maximum number of SAP Server contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_SAPS_MAX_NUM_OF_CONTEXTS                     (1) 

/* 
 *  The maximum number of bytes in a serviceName.    
 */
#define BTL_CONFIG_SAPS_MAX_SERVICE_NAME                        (20) 


/*-------------------------------------------------------------------------------
 * SPP
 *
 *     Represents configuration parameters for SPP module.
 */
 
/* 
 *  The maximum number of SPP ports
 */
#define BTL_CONFIG_SPP_MAX_NUM_OF_PORTS_CONTEXTS                (7)

/* Define whether security will be applied to SPP client ports
 * Currently, it may be disabled because of a problem with D-Link USB dongle with
 * Broadcom's protocol stack
 */
#define BTL_CONFIG_SPP_SECURITY_CLIENT                          (BTL_CONFIG_ENABLED)

/* Configuration constant to enable allocation interim buffer for Tx Sync data
 * path.
 */
#define BTL_CONFIG_SPP_USE_INTERIM_TX_BUFFER                    (BTL_CONFIG_ENABLED)

/* Configuration constant to enable allocation interim buffer for Rx Async data
 * path.
 */
#define BTL_CONFIG_SPP_USE_INTERIM_RX_BUFFER                    (BTL_CONFIG_ENABLED)

#if BTL_CONFIG_SPP_USE_INTERIM_TX_BUFFER == BTL_CONFIG_ENABLED

/* Size of interim Tx buffer measured in RFCOMM packets of maximal size */
#define BTL_CONFIG_SPP_SIZE_OF_INTERIM_TX_BUFFER                (2)

#endif /* BTL_CONFIG_SPP_USE_INTERIM_TX_BUFFER == BTL_CONFIG_ENABLED */

#if BTL_CONFIG_SPP_USE_INTERIM_RX_BUFFER == BTL_CONFIG_ENABLED

/* Size of interim Rx buffer measured in RFCOMM packets of maximal size */
#define BTL_CONFIG_SPP_SIZE_OF_INTERIM_RX_BUFFER                (2)

#endif /* BTL_CONFIG_SPP_USE_INTERIM_RX_BUFFER == BTL_CONFIG_ENABLED */

#if (BTL_CONFIG_SPP_USE_INTERIM_TX_BUFFER == BTL_CONFIG_ENABLED) || (BTL_CONFIG_SPP_USE_INTERIM_RX_BUFFER == BTL_CONFIG_ENABLED)
/*  The maximum number of SPPBUF. The SPPBUF struct holds 2 buffers which will
 *  be used for TX_SYNC and RX_ASYNC port configuration, therefor if a port
 *  opened with TX_ASYNC and RX_SYNC this buffer will not be used. 
 *  One can Open BTL_CONFIG_SPP_MAX_NUM_OF_PORTS_CONTEXTS ports from which only
 *  BTL_CONFIG_SPP_MAX_NUM_OF_SPP_BUFFERS can use TX_SYNC or RX_ASYNC (or both). 
 */
#define BTL_CONFIG_SPP_MAX_NUM_OF_INTERIM_BUFFERS               (BTL_CONFIG_SPP_MAX_NUM_OF_PORTS_CONTEXTS)
#endif
/* Size of a pool with BtPacket type structures used for sending Tx data to
 * RFCOMM
 */
#define BTL_CONFIG_SPP_TX_MAX_NUM_OF_RFCOMM_PACKETS             (20)


/*-------------------------------------------------------------------------------
 * RFCOMM
 *
 *     Represents configuration parameters for RFCOMM module.
 */

/*
 *  The maximum number of RFCOMM contexts to be shared among all of the 
 *  applications (rather than per application)
 */
#define BTL_CONFIG_RFCOMM_MAX_NUM_OF_CONTEXTS                   (2)

/*
 *  The maximum number of channels per context
 */
#define BTL_CONFIG_RFCOMM_MAX_NUM_CHANNELS_PER_CONTEXT          (4)

/*
 *  The maximum number of TX packets
 */
#define BTL_CONFIG_RFCOMM_MAX_NUM_OF_TX_PACKETS                 (10)


/*-------------------------------------------------------------------------------
 * VG
 *
 *     Represents configuration parameters for VG module.
 */

/*------------------------------------------------------------------------------
 * BTL_CONFIG_VG_MAXIMUM_NUMBER_OF_CHANNELS constant
 *
 *  The maximal number of available VG Channels.
 *  Generally more then one connections is not possible, only during handover
 *  procedure 2 connections are possible.
 */
#define BTL_CONFIG_VG_MAXIMUM_NUMBER_OF_CHANNELS                (2)

/*
 *  The timer for user's response to additional SLC connection request
 */
#define BTL_CONFIG_VG_USE_HANDOVER_TIMER                        (BTL_CONFIG_ENABLED)

/*
 *  The default value of the handover timeout
 */
#define BTL_CONFIG_VG_HANDOVER_TIMEOUT_DEFAULT_VALUE            (60000)

/* 
 *  Sets the sniff mode, if sniff mode is set to BTL_CONFIG_ENABLED the sniff is
 *  activated, otherwise, sniff is disabled.
 *  This definition is per channel and enabling of the sniff mode requires that
 *  BTL_CONFIG_VG_SNIFF_TIMER >= 0.
 */
#define BTL_CONFIG_VG_SNIFF_MODE                                (BTL_CONFIG_ENABLED)

/* 
 *  Sets the policy for exiting sniff mode on the specified channel. When enabled,
 *  exit sniff mode whenever there is a data to send.
 *  This definition is per channel.
 */
#define BTL_CONFIG_VG_SNIFF_EXIT_ON_SEND_POLICY                 (BTL_CONFIG_DISABLED)

/* 
 *  Sets the policy for exiting sniff mode on the specified channel. When enabled,
 *  exit sniff mode whenever there is a data to send.
 *  This definition is per channel.
 */
#define BTL_CONFIG_VG_SNIFF_EXIT_ON_AUDIO_LINK_POLICY           (BTL_CONFIG_ENABLED)

/* 
 *  Enable sniff mode after a defined timeout in milliseconds.
 *  This is a power saving feature. If this value is defined to -1, then, sniff
 *  mode is disabled.
 */
#define BTL_CONFIG_VG_SNIFF_TIMER                               (2000)

/*
 *  Minimum interval in BT slots of 0.625 ms for sniff mode, if enabled
 *  (BTL_CONFIG_VG_SNIFF_TIMER >= 0).
 */
#define BTL_CONFIG_VG_SNIFF_MIN_INTERVAL                        (400)

/* 
 *  Minimum interval in BT slots of 0.625 ms for sniff mode, if enabled
 *  (BTL_CONFIG_VG_SNIFF_TIMER >= 0).
 */
#define BTL_CONFIG_VG_SNIFF_MAX_INTERVAL                        (800)

/* 
 *  Number of BT receive slots in 1.25 ms units for sniff attempts in sniff mode,
 *  if enabled (BTL_CONFIG_VG_SNIFF_TIMER >= 0).
 */
#define BTL_CONFIG_VG_SNIFF_ATTEMPT                             (4)

/* 
 *  Number of BT receive slots in 1.25 ms units for sniff timeout in sniff mode,
 *  if enabled (BTL_CONFIG_VG_SNIFF_TIMER >= 0).
 */
#define BTL_CONFIG_VG_SNIFF_TIMEOUT                             (1)

/* 
 *  Maximum Latency parameter in BT slots of 0.625 ms, used to calculate the
 *  maximum sniff subrate that the remote device may use.
 */
#define BTL_CONFIG_VG_SNIFF_SUBRATING_MAX_LATENCY               (1600)

/* 
 *  Minimum base sniff subrate timeout in BT slots of 0.625 ms that the remote
 *  device may use.
 */
#define BTL_CONFIG_VG_SNIFF_SUBRATING_MIN_REMOTE_TIMEOUT        (1600)

/* 
 *  Minimum base sniff subrate timeout in BT slots of 0.625 ms that the local
 *  device may use.
 */
#define BTL_CONFIG_VG_SNIFF_SUBRATING_MIN_LOCAL_TIMEOUT         (1600)

/*  Note: setting both subrate values:
 *  BTL_CONFIG_VG_SNIFF_SUBRATING_MIN_REMOTE_TIMEOUT and
 *  BTL_CONFIG_VG_SNIFF_SUBRATING_MIN_LOCAL_TIMEOUT
 *  to 0 means that sniff mode is enabled without subrating.
 */

/*-------------------------------------------------------------------------------
 * PAN
 *
 *     Represents configuration parameters for the PAN module.
 */

/*
 *  The maximum number of PAN contexts to be shared among all of the 
 *   applications (rather than per application)
 */
#define BTL_CONFIG_PAN_MAX_NUM_OF_CONTEXTS                     (1)

/*
 *  The maximum number of simultaneous PAN connections per PAN context. 
 *  This actually means how many peer devices can be connected simultaneously 
 */
#define BTL_CONFIG_PAN_MAX_NUM_PEER_DEVICES_PER_CONTEXT        (3)

#endif /* __BTL_CONFIG_H */





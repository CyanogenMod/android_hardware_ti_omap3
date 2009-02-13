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
*   FILE NAME:      bthal_mm.h
*
*   BRIEF:          This file defines the API of the BTHAL multimedia.
*                  
*
*   DESCRIPTION:    General
*
*					The BTHAL multimedia is the interface between the BTL_A2DP
*					module and the platform dependent multimedia module.
*
*					It provides access to platform dependant multimedia services:
*
*					1) Receiving media data (in different formats) from the MM, 
*					and hand them over to BTL_A2DP module.
*
*					2) Access and set codec capabilites and configuration.
*
*					3) Indicate the current state of the transport channel to the MM.
*
*
*					Interface
*					---------
*					BTL_A2DP module access the MM via direct function calls,
*					declared in this file.
*
*					MM module access BTL_A2DP via a MM callback function, 
*					registered with the function BTHAL_MM_Register.
*
*
*					Media data types
*					----------------
*					BTHAL MM can handle 3 types of media data:
*
*					1) PCM data, meaning that the BTL_A2DP module encodes the 
*					input data to SBC via an internal built-in SBC encoder 
*					(BTL_CONFIG_A2DP_SBC_BUILT_IN_ENCODER must be enabled), 
*					before it transmits the music over the air.
*
*					2) SBC data, meaning that the BTL_A2DP module transmits the 
*					music over the air as is.
*
*					3) MP3 data, meaning that the BTL_A2DP module transmits the 
*					music over the air as is.
*
*
*					Codec capabilites and configuration
*					-----------------------------------
*					BTL_A2DP module request the local codec capabilites from the MM
*					via the function BTHAL_MM_GetLocalCodecCapabilities.
*
*					BTL_A2DP module inform the MM of the remote codec capabilites
*					via the function BTHAL_MM_RemoteCodecCapabilitiesInd.
*
*					BTL_A2DP module request the selected configuration via the 
*					function BTHAL_MM_GetSelectedConfigInfo.
*					The MM should decide the correct configuration according to the 
*					local codec capabilites, the remote codec capabilites and the 
*					current song it wish to play.
*
*					BTL_A2DP module inform the MM of the selected configuration 
*					after it was negotiated (and now the transport channel is configured) 
*					via the function BTHAL_MM_CodecInfoConfiguredInd.
*
*					The MM inform the BTL_A2DP module that a new configuration (probably 
*					because of a new song) is needed by sending the event 
*					BTHAL_MM_EVENT_CONFIG_IND to BTL_A2DP module via the MM callback.
*					As a result, BTL_A2DP module will request the new selected 
*					configuration via the function BTHAL_MM_GetSelectedConfigInfo (only 
*					if the transport channel is in 'open' state, since only then the 
*					channel can be configured).
*
*
*					Streaming media data
*					--------------------
*					The media data can be given to BTL_A2DP module in 3 possible 
*					ways:
*
*					1) Push mode, meaning that the MM push the media data to
*					BTL_A2DP module whenever the MM wants.
*					This is done via the event BTHAL_MM_EVENT_DATA_BUFFER_IND, 
*					which is sent to BTL_A2DP module via the MM callback.
*
*					2) Pull sync mode, meaning that BTL_A2DP pull media data
*					from the MM via the function BTHAL_MM_RequestMoreData 
*					(which returns BTHAL_STATUS_SUCCESS) whenever BTL_A2DP wants, 
*					and the data is returned synchronously in the function argument.
*					This process is started once the event BTHAL_MM_EVENT_START_PULL 
*					is sent to BTL_A2DP module via the MM callback. 
*					This process is stoped once the event BTHAL_MM_EVENT_STOP_PULL 
*					is sent to BTL_A2DP module via the MM callback.
*
*					3) Pull async mode, meaning that BTL_A2DP pull media data
*					from the MM via the function BTHAL_MM_RequestMoreData 
*					(which returns BTHAL_STATUS_PENDING) whenever BTL_A2DP wants, 
*					and the data is returned asynchronously via the event 
*					BTHAL_MM_EVENT_DATA_BUFFER_IND, which is sent to BTL_A2DP module 
*					via the MM callback.
*					This process is started once the event BTHAL_MM_EVENT_START_PULL 
*					is sent to BTL_A2DP module via the MM callback. 
*					This process is stoped once the event BTHAL_MM_EVENT_STOP_PULL 
*					is sent to BTL_A2DP module via the MM callback.
*
*
*					In all cases, the media data is associated with a data descriptor 
*					(called BthalMmDataBufferDescriptor) given with the buffer.
*					This descriptor is given by the MM and it can be a number or a pointer.
*					The media data pointer must remain valid while the BTL_A2DP use it.
*					It can be freed only when the function BTHAL_MM_FreeDataBuf is 
*					called with the data descriptor.
*
*					
*					Transport channel state
*					-----------------------
*					BTL_A2DP inform the MM with the current state of the transport 
*					channel via the function BTHAL_MM_TransportChannelStateChanged.
*					
*					Media data can be transmitted over the transport channel only after
*					it is in 'streaming' state (otherwise, they will be dropped).
*					This means that:
*
*					1) To start playing music, the transport channel must first be 
*					in 'streaming' state, and only then media data can be sent.
*
*					2) To stop playing music, first stop sending media data, and only 
*					then the transport channel state can be changed.
*
*
*   AUTHOR:         Keren Ferdman
*
\*******************************************************************************/


#ifndef __BTHAL_MM_H
#define __BTHAL_MM_H


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include <bthal_common.h>


/*-------------------------------------------------------------------------------
 * BthalMmTransportChannelState type
 *
 *     	Describes the current state of the transport channel.
 */
typedef BTHAL_U8 BthalMmTransportChannelState;

#define BTHAL_MM_TRANSPORT_CHANNEL_STATE_CLOSED			    		(0x00)
#define BTHAL_MM_TRANSPORT_CHANNEL_STATE_OPEN    					(0x01)
#define BTHAL_MM_TRANSPORT_CHANNEL_STATE_STREAMING 					(0x02)


/********************************************************************************
 *
 * SBC specific information elements
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BthalMmSbcSamplingFreqMask type
 *
 *     	Describes the sampling frequency in Hz of the SBC stream.
 *		SRC must support at least one of the following: FREQ_44100 or FREQ_48000.
 *		The first 4 frequencies can be OR'ed together.
 *
 */
typedef BTHAL_U8 BthalMmSbcSamplingFreqMask;

#define BTHAL_MM_SBC_SAMPLING_FREQ_16000			    			(0x80)
#define BTHAL_MM_SBC_SAMPLING_FREQ_32000    						(0x40)
#define BTHAL_MM_SBC_SAMPLING_FREQ_44100	  						(0x20)
#define BTHAL_MM_SBC_SAMPLING_FREQ_48000    						(0x10)

/* 
*  The additional following frequencies are supported only when the MM provides 
*  PCM data in non-standard frequencies, and sample rate converter is included 
*  in the build (BTL_CONFIG_A2DP_SAMPLE_RATE_CONVERTER is enabled).
*  They should be used only when the function BTHAL_MM_GetSelectedConfigInfo is 
*  called to get the PCM sample frequency.
*  They can NOT be OR'ed together, since only one such frequency can be selected 
*  during the call to BTHAL_MM_GetSelectedConfigInfo.
*/
#define BTHAL_MM_SBC_SAMPLING_FREQ_8000_EXT		    				(0x01)
#define BTHAL_MM_SBC_SAMPLING_FREQ_11025_EXT   						(0x02)
#define BTHAL_MM_SBC_SAMPLING_FREQ_12000_EXT  						(0x03)
#define BTHAL_MM_SBC_SAMPLING_FREQ_22050_EXT   						(0x04)
#define BTHAL_MM_SBC_SAMPLING_FREQ_24000_EXT   						(0x05)


/*-------------------------------------------------------------------------------
 * BthalMmSbcChannelModeMask type
 *
 *     	Describes the channel mode used to encode a stream.
 *		SRC must support MONO and at least one of the following: DUAL_CHANNEL, 
 *		STEREO or JOINT_STEREO.
 *		Can be OR'ed together.
 */
typedef BTHAL_U8 BthalMmSbcChannelModeMask;

#define BTHAL_MM_SBC_CHANNEL_MODE_MONO        						(0x08)
#define BTHAL_MM_SBC_CHANNEL_MODE_DUAL_CHANNEL 						(0x04)
#define BTHAL_MM_SBC_CHANNEL_MODE_STEREO      						(0x02)	
#define BTHAL_MM_SBC_CHANNEL_MODE_JOINT_STEREO 						(0x01)			


/*-------------------------------------------------------------------------------
 * BthalMmSbcBlockLengthMask type
 *
 *     	Describes the block length used to encode a stream.
 *		SRC must support all options.
 *		Can be OR'ed together.
 */
typedef BTHAL_U8 BthalMmSbcBlockLengthMask;

#define BTHAL_MM_SBC_BLOCK_LENGTH_4          						(0x80)			
#define BTHAL_MM_SBC_BLOCK_LENGTH_8 	    						(0x40)	
#define BTHAL_MM_SBC_BLOCK_LENGTH_12    	    					(0x20)	
#define BTHAL_MM_SBC_BLOCK_LENGTH_16  								(0x10)


/*-------------------------------------------------------------------------------
 * BthalMmSbcSubbandsMask type
 *
 *     	Describes the subbands used to encode a stream.
 *		SRC must support SUBBANDS_8.
 *		Can be OR'ed together.
 */
typedef BTHAL_U8 BthalMmSbcSubbandsMask;

#define BTHAL_MM_SBC_SUBBANDS_4          							(0x08)		
#define BTHAL_MM_SBC_SUBBANDS_8 	    							(0x04)			


/*-------------------------------------------------------------------------------
 * BthalMmSbcAllocationMethodMask type
 *
 *     	Describes allocation method used to encode a stream.
 *		SRC must support LOUDNESS.
 *		Can be OR'ed together.
 */
typedef BTHAL_U8 BthalMmSbcAllocationMethodMask;

#define BTHAL_MM_SBC_ALLOCATION_METHOD_SNR   						(0x02)
#define BTHAL_MM_SBC_ALLOCATION_METHOD_LOUDNESS        				(0x01)


/*-------------------------------------------------------------------------------
 * BthalMmSbcBitpoolValue type
 *
 *     	Describes the bitpool used to encode a stream, ranging from 2 to 250.
 */
typedef BTHAL_U8 BthalMmSbcBitpoolValue;

/*-------------------------------------------------------------------------------
 * BthalMmAudioQuality type
 *
 *     	Denotes recommended sets of SBC parameters for different audio qualities
 */
typedef BTHAL_U8 BthalMmSbcAudioQuality;
#define BTHAL_MM_SBC_AUDIO_QUALITY_HIGH						(0x01)
#define BTHAL_MM_SBC_AUDIO_QUALITY_MIDDLE					(0x02)


/*-------------------------------------------------------------------------------
 * BthalMmSbcInfo type
 *
 *     CODEC specific information elements for SBC.
 */
typedef struct _BthalMmSbcInfo
{
	BthalMmSbcSamplingFreqMask samplingFreq;

	BthalMmSbcChannelModeMask channelMode;

	BthalMmSbcBlockLengthMask blockLength;

	BthalMmSbcSubbandsMask subbands;

	BthalMmSbcAllocationMethodMask allocationMethod;

	BthalMmSbcBitpoolValue minBitpoolValue;

	BthalMmSbcBitpoolValue maxBitpoolValue;

} BthalMmSbcInfo;


/*-------------------------------------------------------------------------------
 * BthalMmSbcEncoderLocation type
 *
 *     Defines whether Mm has the SBC encoder
 */
typedef BTHAL_U8 BthalMmSbcEncoderLocation;

/*  
 * The SBC encoder is in the MM. MM provides SBC frames samples to BTL
 */
#define BTHAL_MM_SBC_ENCODER_EXTERNAL							(0x00)

/*  
 * The SBC encoder is built-in inside BTL. MM provides PCM samples to BTL 
 */
#define BTHAL_MM_SBC_ENCODER_BUILT_IN							(0x01)


/********************************************************************************
 *
 * MPEG-1,2 Audio specific information elements
 *
 *******************************************************************************/


/*-------------------------------------------------------------------------------
 * BthalMmMpeg1_2_audioLayerMask type
 *
 *     	Describes the layer of the MPEG-1,2 Audio stream.
 *		SRC must support at least one of the following: 
 *		LAYER_1 (mp1) or LAYER_2 (mp2) or LAYER_3 (mp3).
 *		Can be OR'ed together.
 */
typedef BTHAL_U8 BthalMmMpeg1_2_audioLayerMask;

#define BTHAL_MM_MPEG1_2_AUDIO_LAYER_1			    				(0x80)
#define BTHAL_MM_MPEG1_2_AUDIO_LAYER_2    							(0x40)
#define BTHAL_MM_MPEG1_2_AUDIO_LAYER_3  							(0x20)


/*-------------------------------------------------------------------------------
 * BthalMmMpeg1_2_audioCrcProtection type
 *
 *     	Describes if CRC protection is supported in the MPEG-1,2 Audio stream.
 *		It is optional to support CRC protection in the SRC.
 */
typedef BTHAL_U8 BthalMmMpeg1_2_audioCrcProtection;

#define BTHAL_MM_MPEG1_2_AUDIO_CRC_PROTECTION_NOT_SUPPORTED   		(0x00)
#define BTHAL_MM_MPEG1_2_AUDIO_CRC_PROTECTION_SUPPORTED    			(0x10)


/*-------------------------------------------------------------------------------
 * BthalMmMpeg1_2_audioChannelModeMask type
 *
 *     	Describes the channel mode used to encode a stream.
 *		SRC must support at least one of the following: 
 *		MONO, DUAL_CHANNEL, STEREO or JOINT_STEREO.
 *		Can be OR'ed together.
 */
typedef BTHAL_U8 BthalMmMpeg1_2_audioChannelModeMask;

#define BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_MONO 					(0x08)
#define BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_DUAL_CHANNEL			(0x04)
#define BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_STEREO      			(0x02)
#define BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_JOINT_STEREO			(0x01)


/*-------------------------------------------------------------------------------
 * BthalMmMpeg1_2_audioMPF2 type
 *
 *		MPF field indicates the support of media payload format for MPEG-1,2 Audio.
 *		It is mandatory to support MPF-1.
 *		MPF-2 provides more error-robustness for MPEG-1,2 Audio layer 3.
 *		The MPF field is set to SUPPORTED if MPF-2 is also supported, or if MPF-2 
 *		is configured as a transferred media payload format, otherwise it is set 
 *		to NOT_SUPPORTED.
 */
typedef BTHAL_U8 BthalMmMpeg1_2_audioMPF2;

#define BTHAL_MM_MPEG1_2_AUDIO_MPF2_NOT_SUPPORTED  					(0x00)
#define BTHAL_MM_MPEG1_2_AUDIO_MPF2_SUPPORTED    					(0x40)


/*-------------------------------------------------------------------------------
 * BthalMmMpeg1_2_audioSamplingFreqMask type
 *
 *     	Describes the sampling frequency in Hz of the MPEG-1,2 Audio stream.
 *		SRC must support at least one of the following: FREQ_44100 or FREQ_48000.
 *		Can be OR'ed together.
 */
typedef BTHAL_U8 BthalMmMpeg1_2_audioSamplingFreqMask;

#define BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_16000			    	(0x20)
#define BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_22050 					(0x10)
#define BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_24000					(0x08)
#define BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_32000    				(0x04)
#define BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_44100					(0x02)
#define BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_48000    				(0x01)


/*-------------------------------------------------------------------------------
 * BthalMmMpeg1_2_audioVBR type
 *
 *		VBR field is Variable Bit Rate for MPEG-1,2 Audio.
 *		It is optional to support in SRC.
 */
typedef BTHAL_U16 BthalMmMpeg1_2_audioVBR;

#define BTHAL_MM_MPEG1_2_AUDIO_VBR_NOT_SUPPORTED  					(0x0000)
#define BTHAL_MM_MPEG1_2_AUDIO_VBR_SUPPORTED    					(0x8000)


/*-------------------------------------------------------------------------------
 * BthalMmMpeg1_2_audioBitRateIndexMask type
 *
 *     	Describes the index value which represents the actual bit rate value.
 *		SRC must support at least one of the bit rate values (except for index 0, 
 *		which is optional). 
 */
typedef BTHAL_U16 BthalMmMpeg1_2_audioBitRateIndexMask;

#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_0			    		(0x0001)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_1 						(0x0002)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_2						(0x0004)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_3    					(0x0008)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_4						(0x0010)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_5    					(0x0020)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_6			    		(0x0040)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_7 						(0x0080)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_8						(0x0100)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_9    					(0x0200)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_10						(0x0400)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_11    					(0x0800)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_12			    		(0x1000)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_13 					(0x2000)
#define BTHAL_MM_MPEG1_2_AUDIO_BITRATE_INDEX_14						(0x4000)


/*-------------------------------------------------------------------------------
 * BthalMmMpeg1_2_audioInfo type
 *
 *     CODEC specific information elements for MPEG-1,2 Audio.
 */
typedef struct _BthalMmMpeg1_2_audioInfo
{
	BthalMmMpeg1_2_audioLayerMask layer;

	BthalMmMpeg1_2_audioCrcProtection crcProtection;

	BthalMmMpeg1_2_audioChannelModeMask channelMode;

	BthalMmMpeg1_2_audioMPF2 mpf2; 

	BthalMmMpeg1_2_audioSamplingFreqMask samplingFreq;

	BthalMmMpeg1_2_audioVBR vbr;

	BthalMmMpeg1_2_audioBitRateIndexMask bitRate;

	BTHAL_U32 curBitRate; /* the calculated actual bit rate */

} BthalMmMpeg1_2_audioInfo;



/********************************************************************************
 *
 * PCM specific information elements
 *
 *******************************************************************************/


/*-------------------------------------------------------------------------------
 * BthalMmNumberOfChannels type
 *
 *     	Describe how many channels there are.
 *		One for MONO and 2 for stereo.
 */
typedef BTHAL_U8 BthalMmNumberOfChannels;

#define BTHAL_MM_ONE_CHANNELS        								(0x01)
#define BTHAL_MM_TWO_CHANNELS										(0x02)


/*-------------------------------------------------------------------------------
 * BthalMmPcmInfo type
 *
 *     CODEC specific information elements for PCM.
 */
typedef struct _BthalMmPcmInfo
{
	BthalMmSbcSamplingFreqMask samplingFreq;

	BthalMmNumberOfChannels numOfchannels;

	BthalMmSbcAudioQuality	audioQuality;

	BTHAL_BOOL				varyBitPool;	/* whether to apply varying bitpool scheme */
	
} BthalMmPcmInfo;


/*-------------------------------------------------------------------------------
 * BthalMmStreamType type
 *
 *     Defines the sent stream data type from MM to stack.
 */
typedef BTHAL_U8 BthalMmStreamType;

#define BTHAL_MM_STREAM_TYPE_SBC									(0x00)
#define BTHAL_MM_STREAM_TYPE_MPEG1_2_AUDIO							(0x01)
#define BTHAL_MM_STREAM_TYPE_PCM									(0x02)


/*-------------------------------------------------------------------------------
 * BthalMmDataBufferDescriptor type
 *
 *     Defines a unique descriptor for data buffer.
 */
typedef	BTHAL_U32 BthalMmDataBufferDescriptor;


/*-------------------------------------------------------------------------------
 * BthalMmDataBuffer type
 *
 *     The data buffer sent from MM to stack.
 */
typedef struct _BthalMmDataBuffer
{
	/* This descriptor will be returned with BTHAL_MM_FreeDataBuf when the stack 
	does not need the data buffer anymore */
	BthalMmDataBufferDescriptor	descriptor;

	/* This data pointer is owned by the stack until the function 
	BTHAL_MM_FreeDataBuf indicates it can be freed */
	BTHAL_U8  					*data;

	/* Length of the 'data' field */
	BTHAL_U16 					dataLength;

	/* Stream type (PCM or SBC or MPEG-1,2 Audio) */
	BthalMmStreamType			streamType;

	/* Frame offset (relevant only for MPEG-1,2 Audio) */
	BTHAL_U16					frameOffset;

} BthalMmDataBuffer;


/*---------------------------------------------------------------------------
 * 	BthalMmEvent
 *      
 *     Those events are sent by MM to the stack.
 */
typedef BTHAL_U8 BthalMmEvent;
                                        
/*
 * This event is sent by MM to the stack to provide data buffer.
 * During this event, the field 'p.dataBuffer' is valid. 
 */
#define BTHAL_MM_EVENT_DATA_BUFFER_IND           				(0x00)

/* 
 * This event is sent by MM to the stack to indicate that new 
 * configuration information is available.
 * The stack will call BTHAL_MM_GetSelectedConfigInfo to get the new 
 * configuration information. 
 */
#define BTHAL_MM_EVENT_CONFIG_IND          						(0x01)

/*
 * This event is sent by MM to the stack to indicate that the stack 
 * should start pulling data from the MM.
 * The stack will call BTHAL_MM_RequestMoreData to get the data. 
 */
#define BTHAL_MM_EVENT_START_PULL          						(0x03)

/*
 * This event is sent by MM to the stack to indicate that the stack 
 * should stop pulling data from the MM.
 * The stack will stop calling BTHAL_MM_RequestMoreData. 
 */
#define BTHAL_MM_EVENT_STOP_PULL          						(0x04)


typedef struct _BthalMmCallbackParms BthalMmCallbackParms;


/*---------------------------------------------------------------------------
 * BthalMmConfigParams structure
 *
 * Contains information of stream config required by the MM.
 */
typedef	struct _BthalMmConfigParams
{
	/* The type of the data which will arrive from the MM (PCM / SBC / MPEG-1,2 Audio) */
	BthalMmStreamType streamType;

	union
	{	
		/* Valid if  'streamType' is BTHAL_MM_STREAM_TYPE_PCM */
		BthalMmPcmInfo pcmInfo;

		/* Valid if  'streamType' is BTHAL_MM_STREAM_TYPE_SBC */
		BthalMmSbcInfo sbcInfo;

		/* Valid if  'streamType' is BTHAL_MM_STREAM_TYPE_MPEG1_2_AUDIO */
		BthalMmMpeg1_2_audioInfo mpeg1_2_audioInfo;
	} p;

} BthalMmConfigParams;


/*---------------------------------------------------------------------------
 * BthalMmCallbackParms structure
 *
 * Contains information for the BTHAL mm callback event.
 */
struct _BthalMmCallbackParms 
{
    /* BTHAL MM event */
    BthalMmEvent event;

    /* Callback parameter object */
    union 
    {
		/* data buffer received during BTHAL_MM_EVENT_DATA_BUFFER_IND */
		BthalMmDataBuffer dataBuffer;
    } p;
};


/*-------------------------------------------------------------------------------
 * BthalMmCallback type
 *
 *     A function of this type is called to indicate BTHAL MM events.
 */
typedef void (*BthalMmCallback)(BthalMmCallbackParms *callbackParams);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_Init()
 *
 * Brief:  
 *		Initialize the BTHAL MM.
 *
 * Description:	
 *		This function should only be called once at system-startup.
 *		
 *
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 * 		callback [in] - if this operation is async, completion will be notified 
 *			via the given callback. 
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 *
 *		BTHAL_STATUS_PENDING - Operation has started.
 *			Completion will be notified via the callback.
 */
BthalStatus BTHAL_MM_Init(BthalCallBack	callback);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_Deinit()
 *
  * Brief:  
 *		Deinitialize the BTHAL MM.  
 *
 * Description:
 *		This function should only be called once at system-shutdown.
 *
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 *		void.
 *		
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 *
 *		BTHAL_STATUS_PENDING - Operation has started.
 *			Completion will be notified via the callback (given in BTHAL_MM_Init).
 */
BthalStatus BTHAL_MM_Deinit(void);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_Register()
 *
 * Brief:  
 *		Register BTHAL MM callback.
 *
 * Description:
 *		Registers a callback function with which the MM will notify the stack about events
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *
 *		mmCallback[in] - call back function.
 *		
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_MM_Register(BthalMmCallback mmCallback);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_GetLocalCodecCapabilities()
 *
 * Brief:  
 *           Get the codec capabilities of the local device.
 *		
 * Description:
 *		This function is called by stack to get the codec capabilities of 
 *		the	local device.
 *		This function will be called on each codec, and the field 'streamType' 
 *		will specify the exact codec which its capabilities are needed.
 *		Note that one or more bits may be defined/set in each field, since
 *		the bits specify ALL supported capabilities.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		codecInfo [in/out] - codec specific information elements supported by the 
 *			local device.
 *
 *		sbcEncoderLocation[out] - location of SBC encoder (built-in or external)
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_MM_GetLocalCodecCapabilities(BthalMmConfigParams *codecInfo, 
											BthalMmSbcEncoderLocation *sbcEncoderLocation);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_RemoteCodecCapabilitiesInd()
 *
 * Brief: 
 *           Indicate the codec capabilities of the remote device
 *
 * Description:
 *		This function is called by stack to indicate the codec capabilities of 
 *		the	remote device.
 *		Note that one or more bits may be defined/set in each field, since
 *		the bits specify ALL supported capabilities.
 *		The MM can cache or verify the specific elements of these capabilities.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		codecInfo [in] - codec specific information elements supported by the 
 *			remote device.
 *
 *		streamId [in] - represents the ID of the remote stream.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_MM_RemoteCodecCapabilitiesInd(BthalMmConfigParams *codecInfo, 
												BTHAL_U32 streamId);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_GetSelectedConfigInfo()
 *
 * Brief:  
 *           Get the selected config information for the specified stream
 *
 * Description:
 *		The stack will call this function to get the selected config information
 *		about the stream specified in 'streamType' field in the argument 'configInfo'.
 *		The MM will select a codec (1 bit per capability) according to the local and 
 *		remote device capabilities.
 *		Note that only one bit at the codec may be defined/set in each field, 
 *		since the bits specify only one desired capability.
 *		The desired codec information is selected based on the capabilities
 *      received in the BTHAL_MM_RemoteCodecCapabilitiesInd.
 *
 *      Note: When streamType is BTHAL_MM_STREAM_TYPE_SBC, the fields minBitpoolValue and 
 *      maxBitpoolValue must be identical and equal to the selected bitpool.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		configInfo [in/out] - selected config information.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_MM_GetSelectedConfigInfo(BthalMmConfigParams *configInfo);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_CodecInfoConfiguredInd()
 *
 * Brief:  
 *           Indicate the selected codec configuration
 *
 * Description:
 *		After the stack has finished configuring this function is called 
 *		to indicate that the codec is configured.
 *		Note that only one bit at the codec may be defined/set in each field, 
 *		since the bits specify only one selected capability.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		codecInfo [in] - configured codec specific information.
 *
 *		packetSize [in] - maximum size of media packet 
 *
 *		sbcFrameLen [in] - SBC frame length. 
 *						used only when SBC encoding is built-in in the stack
 *
 *		streamId [in] - represents the ID of the remote stream.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_MM_CodecInfoConfiguredInd(BthalMmConfigParams *codecInfo, 
											BTHAL_U16 packetSize, 
											BTHAL_U16 sbcFrameLen, 
											BTHAL_U32 streamId);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_FreeDataBuf()
 *
 * Brief: 
 *           Indicate that a data buffer can be freed
 *
 * Description:
 *		This function is called by stack to indicate the buffer can be freed.
 *		This buffer was previously received with BTHAL_MM_EVENT_DATA_BUFFER_IND 
 *		event or when BTHAL_MM_RequestMoreData returned synchronously.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		dataIndicator [in] - a number indicating on data.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_MM_FreeDataBuf(BthalMmDataBufferDescriptor dataDescriptor);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_RequestMoreData()
 *
 * Brief:  
 *           Request more media blocks from MM
 *
 * Description:
 *		The stack can request the MM for more data.
 *		This function is used only after the event BTHAL_MM_EVENT_START_PULL was 
 *		sent from MM to the stack to indicate that the stack should start 
 *		pulling data from the MM.
 *
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 *		buffer [out] - returned data buffer, relevant only if BTHAL_STATUS_SUCCESS 
 *			is returned.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *			In this case, the data buffer is provided in the output argument 'buffer'.
 *
 *		BTHAL_STATUS_PENDING - Operation has started. Data buffer will be provided
 *			via the callback on BTHAL_MM_EVENT_DATA_BUFFER_IND event.
 *			In this case, the output argument 'buffer' is not used.
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_MM_RequestMoreData(BthalMmDataBuffer *buffer);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_TransportChannelStateChanged()
 *
 * Brief: 
 *           Indicate the new state of the transport channel.
 *
 * Description:
 *		This function is called by stack to indicate current state of the 
 *		transport channel.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *
 *		streamId [in] - represents the ID of the remote stream.
 *
 *		newState [in] - new transport channel state.
 *
 * Returns:
 *		void.
 */
void BTHAL_MM_TransportChannelStateChanged(BTHAL_U32 streamId, BthalMmTransportChannelState newState);


#endif /* __BTHAL_MM_H */

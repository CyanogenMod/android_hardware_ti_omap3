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
*   FILE NAME:      bthal_mm.c
*
*   DESCRIPTION:    This file defines the API of the BTHAL multimedia.
*
*   AUTHOR:         Keren Ferdman
*
\*******************************************************************************/



#include "btl_config.h"
#include "btl_defs.h"

BTL_LOG_SET_MODULE(BTL_LOG_MODULE_TYPE_A2DP);

#if BTL_CONFIG_A2DP  ==   BTL_CONFIG_ENABLED


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include <windows.h>
#include <stdlib.h>    
#include  <stdio.h>
#include <malloc.h>
#include <osapi.h>
#include <utils.h>
#include "bthal_mm.h"
#include "sys/debug.h"

#include "btl_log.h"
#include "sbc.h"
#include "audio_config.h"


extern void AppReport(char *format,...);

/********************************************************************************
 *
 * Constants 
 *
 *******************************************************************************/


/*-------------------------------------------------------------------------------
 * NUM_PCM_BLOCKS type
 *
 *     Audio playback constant
 */
#define NUM_PCM_BLOCKS                              (10)

/*-------------------------------------------------------------------------------
 * NUM_DATA_BUFFERS 
 *
 *     number of elements in the data buffer list used in the pull mode
 
 */
#define NUM_DATA_BUFFERS                            (10)


/*-------------------------------------------------------------------------------
 * bthalPcmBlock type
 *
 *     Structure for PCM output/input 
 */           
typedef struct _bthalPcmBlock 
{
    ListEntry   node;
    BOOL        prepared;
    WAVEHDR     wavHdr;
    BTHAL_U8    sampleFreq;
    BTHAL_U8    numChannels;
    BTHAL_U16   dataLen;
    BTHAL_U8    *data;
    BthalMmDataBufferDescriptor dataDescriptor;
} bthalPcmBlock;


/*-------------------------------------------------------------------------------
 * bthalDataBlock type
 *
 *     an element in the data buffer list, used in the pull mode
 */           
typedef struct _bthalDataBlock 
{
    ListEntry   node;
    BthalMmDataBuffer dataBuf;
} bthalDataBlock;

/*-------------------------------------------------------------------------------
 * bthalcodecAndFrameInfo type
 *
 *     SBC info
 */           
typedef struct _bthalcodecAndFrameInfo 
{
  BTHAL_U8  bthalMmNumSubBands;
  BTHAL_U8  bthalMmNumBlocks;
  BTHAL_U16 bthalMmPacketSize;
  BTHAL_U16 bthalMmSbcFrameLen;
} bthalcodecAndFrameInfo;


/********************************************************************************
 *
 * Globals
 *
 *******************************************************************************/
 
BthalMmCallback             bthalMmCallback = NULL;
static bthalPcmBlock        inBlock[NUM_PCM_BLOCKS];
static ListEntry            inBlockPool;
static CRITICAL_SECTION     waveInCriticalSection;
static WAVEFORMATEX         wfx;
static HWAVEIN              hWaveIn;
static BTHAL_BOOL           audioInOpen = FALSE;
static BTHAL_BOOL           startAudio = FALSE;
static BTHAL_U32            bthalMmInputDevice;

static BOOL emulatePullMode = FALSE;        /* whether to emulate pull mode */

/* variables for pull mode */
static bthalDataBlock   dataBuffers[NUM_DATA_BUFFERS];
static ListEntry            dataBufferPool;
static ListEntry            dataBufferQueue;
static CRITICAL_SECTION     dataBufferCriticalSection;
static int buffersInQ = 0; /* how many buffer are stored in dataBufferQueue,
            waiting to be provided to the stack via BTHAL_MM_RequestMoreData.
            the number can be negative, indicating there are pendig requests for data */
static U32 waveBlocksCnt = 0; /* how many blocks arrived from  waveIn  */


/*-------------------------------------------------------------------------------
 * SelectedSbcCodec
 *
 *     Represents the selected sbc codec (one bit per capability)
 */
static BthalMmSbcInfo SelectedSbcCodec;

/*-------------------------------------------------------------------------------
 * bthalMmOperetionalMode
 *
 *     Represents the selected cbs codec (one bit per capability)
 */
static bthalcodecAndFrameInfo CodecAndFrameInfo;

/*-------------------------------------------------------------------------------
 * platformPcmSuportedCodec
 *
 *     Represents the PCM platform capabilities
 */
static BthalMmSbcInfo platformPcmSuportedCodec;


/*-------------------------------------------------------------------------------
 * platformMp3SuportedCodec
 *
 *     Represents the MP3 platform capabilities
 */
static BthalMmMpeg1_2_audioInfo platformMp3SuportedCodec;


/* PCM selected information */
static BthalMmPcmInfo selectedPcmInfo;

/* MP3 */


/*-------------------------------------------------------------------------------
 * NUM_MP3_BLOCKS type
 *
 *     Number of MP3 blocks.
 */
#define NUM_MP3_BLOCKS                              (1)


#define MP3_MAX_READ_BUFFER  2048

/*-------------------------------------------------------------------------------
 * NUM_SBC_BLOCKS type
 *
 *     Number of SBC blocks.
 */
#define NUM_SBC_BLOCKS                              (3)

/*-------------------------------------------------------------------------------
 * SBC_FIRST_DESCRIPTOR, SBC_LAST_DESCRIPTOR 
 *
 *     descriptors of SBC blocks (rather than PCM or MP3 blocks)
 *
 */
 #define SBC_FIRST_DESCRIPTOR           (NUM_PCM_BLOCKS + NUM_MP3_BLOCKS)
 #define SBC_LAST_DESCRIPTOR            (SBC_FIRST_DESCRIPTOR + NUM_SBC_BLOCKS - 1)



/* MP3 bitrate tables */

static const U32 v1l1[16] = {
         0,  32000,  64000,  96000, 128000, 160000, 192000, 224000, 
    256000, 288000, 320000, 352000, 384000, 416000, 448000, 0
};

static const U32 v1l2[16] = {
         0,  32000,  48000,  56000,  64000,  80000,  96000, 112000, 
    128000, 160000, 192000, 224000, 256000, 320000, 384000, 0
};

static const U32 v1l3[16] = {
         0,  32000,  40000,  48000,  56000,  64000,  80000,  96000, 
    112000, 128000, 160000, 192000, 224000, 256000, 320000,  0
};

static const U32 v2l1[16] = {
         0,  32000,  48000,  56000,  64000,  80000,  96000, 112000, 
    128000, 144000, 160000, 176000, 192000, 224000, 256000, 0
};

static const U32 v2l23[16] = {
         0,   8000,  16000,  24000,  32000,  40000,  48000,  56000,  
     64000,  80000,  96000, 112000, 128000, 144000, 160000,  0
};


/* MP3 sample rate table (the 1 values in each raw are instead of zeros) */

static const U32 sampleRates[4][4] = {
   { 11025, 12000, 8000, 1 } ,      /* MP3_VERSION_25 */
   { 1, 1, 1, 1 } ,                 /* reserved */
   { 22050, 24000, 16000, 1 } ,     /* MP3_VERSION_2 */
   { 44100, 48000, 32000, 1 }       /* MP3_VERSION_1 */
};


/*---------------------------------------------------------------------------
 * A2dpMp3Version type
 *     
 *     Defines the MP3 spec version as contained in an MP3 audio header.
 */
typedef U8 A2dpMp3Version;

#define MP3_VERSION_1  0x03
#define MP3_VERSION_2  0x02
#define MP3_VERSION_25 0x00

/* End of A2dpMp3Version */

/*---------------------------------------------------------------------------
 * A2dpMp3Layer type
 *     
 *     Defines the MP3 layer as contained in an MP3 audio header.
 */
typedef U8 A2dpMp3Layer;

#define MP3_LAYER_1    0x03
#define MP3_LAYER_2    0x02
#define MP3_LAYER_3    0x01

/* End of A2dpMp3Layer */

/*---------------------------------------------------------------------------
 * A2dpMp3ChannelMode type
 *     
 *     Defines the MP3 channel mode as contained in an MP3 audio header.
 */
typedef U8 A2dpMp3ChannelMode;

#define MP3_MODE_STEREO  0x00
#define MP3_MODE_JOINT   0x01
#define MP3_MODE_DUAL    0x02
#define MP3_MODE_MONO    0x03

/* End of A2dpMp3Mode */


/*---------------------------------------------------------------------------
 * A2DP_SyncSafetoHost32()
 *
 *    Converts a SynchSafe number used by MP3 to a 32 bit unsigned value.
 *
 * Parameters:
 *     ptr - pointer to memory that contains a 4 byte SynchSafe number.
 *
 * Returns:
 *     A 32 bit (U32) value.
 */
U32 A2dP_SyncSafetoHost32(U8 *ptr);
#define A2DP_SynchSafetoHost32(ptr)  (U32)( ((U32) *((U8*)(ptr))   << 21) | \
                                            ((U32) *((U8*)(ptr)+1) << 14) | \
                                            ((U32) *((U8*)(ptr)+2) << 7)  | \
                                            ((U32) *((U8*)(ptr)+3)) )


/*---------------------------------------------------------------------------
 * Mp3StreamInfo structure
 *
 * Contains MP3 audio stream information that can be optained from the frame
 * header.
 */
typedef struct _Mp3StreamInfo {
    A2dpMp3Version      version;     /* MPEG version                           */
    A2dpMp3Layer        layer;       /* MPEG audio layer                       */
    A2dpMp3ChannelMode  channelMode; /* Channel Mode                           */
    BTHAL_U8            brIndex;     /* Bit rate index                         */
    BTHAL_U8            srIndex;     /* Sample rate index                      */
    BOOL                crc;         /* Specifies whether CRC protection is used
                                      *     0 specifies false
                                      *     A non-zero number specifies true
                                      */

    /* TODO: Put in an MP3 packet strucuture */
    BTHAL_U8            numFrames;   /* Number of frames in the current buffer */
    BTHAL_U16           bytesToSend; /* Bytes left to send                     */
    BTHAL_U16           offset;      /* Offset into current frame              */
    BTHAL_U16           frameLen;      /* Frame length of the current frame      */
    BTHAL_U16           totalDataLen;

} Mp3StreamInfo;


/*---------------------------------------------------------------------------
 * Mp3Block structure
 *
 * Contains MP3 audio data.
 */
typedef struct _bthalMp3Block 
{
    BOOL used;
    U8 mp3FrameBuffer[MP3_MAX_READ_BUFFER];
    
} bthalMp3Block;

/*---------------------------------------------------------------------------
 * SbcBlock structure
 *
 * Contains SBC audio data.
 */
typedef struct _bthalSbcBlock 
{
    BOOL used;
    U8 sbcFrameBuffer[L2CAP_MTU];
    
} bthalSbcBlock;


/*-------------------------------------------------------------------------------
 * BthalRequestMoreData type
 *
 *     A function of this type is called to request a block of a certain type 
 *     (PCM, SBC or MP3).
 */
typedef BthalStatus (*BthalRequestMoreData)(BthalMmDataBuffer *buffer);


/*-------------------------------------------------------------------------------
 * BthalCurrentStream type
 *
 *     Defines the current BTHAL MM stream.
 */
typedef U8 BthalCurrentStream;

#define BTHAL_A2DP_STREAM_PCM                                   (0x00)
#define BTHAL_A2DP_STREAM_SBC                                   (0x01)
#define BTHAL_A2DP_STREAM_MP3                                   (0x02)

/* Remote codec capabilities (0/1 are SBC and 2/3 are MP3) */
static BthalMmConfigParams remoteCodecCapabilities[4];
static BTHAL_BOOL remoteCodecCapabilitiesValid[4];

static Mp3StreamInfo mp3StreamInfo;
static bthalMp3Block mp3Blocks[NUM_MP3_BLOCKS];
static FILE *fpMp3 = 0;
static BTHAL_U16 mp3MaxPacketSize;

static FILE *fpSbc = 0;
static SbcStreamInfo sbcStreamInfo = {0};
static bthalSbcBlock sbcBlocks[NUM_SBC_BLOCKS];
static int sbcBlockCnt = 0;

/*-------------------------------------------------------------------------------
 *  externalSbcEncoder
 *
 *     when TRUE, the SBC encoder is external to BTL (we read encoded SBC frames from an SBC file in this BTHAL)
 *     when FALSE - the SBC encoder is built-in inside BTL (and BTHAL provides PCM samples)
 */
#if SBC_ENCODER == XA_ENABLED
static BOOL externalSbcEncoder = FALSE;
#else
static BOOL externalSbcEncoder = TRUE;
#endif /* SBC_ENCODER == XA_ENABLED */


/*-------------------------------------------------------------------------------
 *  Assisted A2DP globals
 *
 */
/* Host platform's CODEC configuration */
AcmConfig bthalMmAcmConfig = {ACM_CODEC_INPUT_LINE_IN, ACM_CODEC_SAMPLE_RATE_48000};

void bthalMmA3dpHciCB(const BtEvent *pEvent);
static void bthalMmAcmCallback(const AcmEvent *acmEvent);
void appA3dpPcmStreamStarted(void);


/********************************************************************************
 *
 * Internal function prototypes
 *
 *******************************************************************************/
static void CALLBACK WaveInProc(HWAVEIN hWaveIn, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
BthalStatus bthalMmOpenAudio(void);
BthalStatus bthalMmCloseAudio(void);
void bthalMmStartRecording(void);
void bthalMmStopRecording(void);
void bthalMmSetPcmPlatformSupportedCapabilities(BthalMmSbcInfo *supportedCap);
void bthalMmSetMp3PlatformSupportedCapabilities(BthalMmMpeg1_2_audioInfo *supportedCap);
void bthalMmSetInputDevice(BTHAL_U32 inputDevice);
static BthalStatus bthalMmSelectMp3Parameters(BthalMmConfigParams *configInfo);
static BthalStatus  bthalMmSelectSbcParameters(BthalMmConfigParams *configInfo);

void bthalMmSetExternalSbcEncoder(BOOL externalEncoder);

void bthalMmSetOperationalMode(BOOL pullMode);
static U16 AppMp3ReadFrames(FILE *fp, U8 *buffer, U32 len, Mp3StreamInfo *streamInfo);
static U16 AppMp3GetFrameLen(U8 *frame, Mp3StreamInfo *streamInfo);
static U16 bthalMmReadSbcFrames(FILE *fp, U8 *buffer, U16 bufferLen, SbcStreamInfo *streamInfo);
static BthalStatus BTHAL_MM_RequestMoreDataMp3(BthalMmDataBuffer *buffer);
static BthalStatus BTHAL_MM_RequestMoreDataPcm(BthalMmDataBuffer *buffer);
static BthalStatus BTHAL_MM_RequestMoreDataSbc(BthalMmDataBuffer *buffer);
static U32 AppMp3GetBitRate(Mp3StreamInfo *streamInfo);
static BOOL SbcParseHeaderInfo(U8 *Buffer );

static BthalCurrentStream currentStream;

/* Function table to process raw block of a certain type (PCM, SBC or MP3) */
static BthalRequestMoreData requestMoreDataFuncTable[3];

FILE * AppMp3OpenFile(const char *FileName);
BOOL AppSbcOpenFile(const char *FileName);


/*-------------------------------------------------------------------------------
 * BTHAL_MM_Init()
 */
BthalStatus BTHAL_MM_Init(BthalCallBack callback)
{
    int i;

    UNUSED_PARAMETER(callback);
    
    //BTL_LOG_DEBUG(("BTHAL_MM_Init, externalSbcEncoder=%d", externalSbcEncoder));

    InitializeCriticalSection(&waveInCriticalSection);
    InitializeCriticalSection(&dataBufferCriticalSection);

    /* Set random initial MP3 values */
    mp3StreamInfo.version = MP3_VERSION_1;
    mp3StreamInfo.layer = MP3_LAYER_3;
    mp3StreamInfo.channelMode = MP3_MODE_JOINT;
    mp3StreamInfo.brIndex = 9;
    mp3StreamInfo.srIndex = 0;
    mp3StreamInfo.crc = 1;

    mp3StreamInfo.numFrames = 0;
    mp3StreamInfo.bytesToSend = 0;
    mp3StreamInfo.offset = 0;

    /* Set random initial SBC values */
    sbcStreamInfo.allocMethod       = SBC_ALLOC_METHOD_LOUDNESS;
    sbcStreamInfo.sampleFreq        = SBC_CHNL_SAMPLE_FREQ_44_1;
    sbcStreamInfo.channelMode   = SBC_CHNL_MODE_JOINT_STEREO;
    sbcStreamInfo.numChannels   = 2;
    sbcStreamInfo.numBlocks     = 16;
    sbcStreamInfo.numSubBands   = 8;
    sbcStreamInfo.bitPool           = 30;
        
    currentStream = BTHAL_A2DP_STREAM_PCM;

    requestMoreDataFuncTable[BTHAL_A2DP_STREAM_PCM] = BTHAL_MM_RequestMoreDataPcm;
    requestMoreDataFuncTable[BTHAL_A2DP_STREAM_SBC] = BTHAL_MM_RequestMoreDataSbc;
    requestMoreDataFuncTable[BTHAL_A2DP_STREAM_MP3] = BTHAL_MM_RequestMoreDataMp3;

    for (i = 0; i < NUM_PCM_BLOCKS; i++) 
    {
        inBlock[i].data = malloc(1024);
    }
    
    for (i = 0; i < 4; i++) 
    {
        remoteCodecCapabilitiesValid[i] = FALSE;
    }
    
    for (i = 0; i < NUM_SBC_BLOCKS; i++) 
    {
        sbcBlocks[i].used = FALSE;
    }

    for (i = 0; i < NUM_MP3_BLOCKS; i++) 
    {
        mp3Blocks[i].used = FALSE;
    }
    
    return BTHAL_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * BTHAL_MM_Deinit()
 */
BthalStatus BTHAL_MM_Deinit(void)
{
    int i;
    for (i = 0; i < NUM_PCM_BLOCKS; i++) 
    {
        free(inBlock[i].data);
    }
    DeleteCriticalSection(&waveInCriticalSection);
    DeleteCriticalSection(&dataBufferCriticalSection);
    
    return BTHAL_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * BTHAL_MM_GetLocalCodecCapabilities()
 */
BthalStatus BTHAL_MM_GetLocalCodecCapabilities(BthalMmConfigParams *codecInfo, 
                                            BthalMmSbcEncoderLocation *sbcEncoderLocation)
{
    switch (codecInfo->streamType)
    {
        case (BTHAL_MM_STREAM_TYPE_SBC):

            codecInfo->p.sbcInfo.samplingFreq = platformPcmSuportedCodec.samplingFreq;
            codecInfo->p.sbcInfo.channelMode = platformPcmSuportedCodec.channelMode;
            codecInfo->p.sbcInfo.blockLength = platformPcmSuportedCodec.blockLength;
            codecInfo->p.sbcInfo.subbands = platformPcmSuportedCodec.subbands;
            codecInfo->p.sbcInfo.allocationMethod = platformPcmSuportedCodec.allocationMethod;
            codecInfo->p.sbcInfo.minBitpoolValue = platformPcmSuportedCodec.minBitpoolValue;
            codecInfo->p.sbcInfo.maxBitpoolValue = platformPcmSuportedCodec.maxBitpoolValue;

            break;

        case (BTHAL_MM_STREAM_TYPE_MPEG1_2_AUDIO):

            codecInfo->p.mpeg1_2_audioInfo.layer = platformMp3SuportedCodec.layer;
            codecInfo->p.mpeg1_2_audioInfo.crcProtection = platformMp3SuportedCodec.crcProtection;
            codecInfo->p.mpeg1_2_audioInfo.channelMode = platformMp3SuportedCodec.channelMode;
            codecInfo->p.mpeg1_2_audioInfo.mpf2 = platformMp3SuportedCodec.mpf2;
            codecInfo->p.mpeg1_2_audioInfo.samplingFreq = platformMp3SuportedCodec.samplingFreq;
            codecInfo->p.mpeg1_2_audioInfo.vbr = platformMp3SuportedCodec.vbr;
            codecInfo->p.mpeg1_2_audioInfo.bitRate = platformMp3SuportedCodec.bitRate;

            break;
            
        default:

            Assert(0);
            break;
    }
    
    //BTL_LOG_DEBUG(("BTHAL_MM_GetLocalCodecCapabilities, externalSbcEncoder=%d", externalSbcEncoder));
    *sbcEncoderLocation = (BthalMmSbcEncoderLocation)
        (externalSbcEncoder ? BTHAL_MM_SBC_ENCODER_EXTERNAL : BTHAL_MM_SBC_ENCODER_BUILT_IN);
    
    return BTHAL_STATUS_SUCCESS;

}

/*-------------------------------------------------------------------------------
 * BTHAL_MM_Register()
 */
BthalStatus BTHAL_MM_Register(BthalMmCallback mmCallback)
{
    bthalMmCallback = mmCallback;

    return BTHAL_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * BTHAL_MM_RemoteCodecCapabilitiesInd()
 */
BthalStatus BTHAL_MM_RemoteCodecCapabilitiesInd(BthalMmConfigParams *codecInfo, BTHAL_U32 streamId)
{
    switch(codecInfo->streamType)
    {   
        case (BTHAL_MM_STREAM_TYPE_SBC):
        case (BTHAL_MM_STREAM_TYPE_MPEG1_2_AUDIO):

            remoteCodecCapabilities[streamId] = *codecInfo;
            break;

        default:

            Assert(0);
            break;
    }

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_MM_GetSelectedConfigInfo()
 */
BthalStatus BTHAL_MM_GetSelectedConfigInfo(BthalMmConfigParams *configInfo)
{
    BthalStatus bthalStatus = BTHAL_STATUS_SUCCESS;
    
    switch(configInfo->streamType)
    {
        case (BTHAL_MM_STREAM_TYPE_PCM):
            configInfo->p.pcmInfo = selectedPcmInfo;
            break;
            
        case (BTHAL_MM_STREAM_TYPE_SBC):
            bthalStatus = bthalMmSelectSbcParameters(configInfo);
            break;

        case (BTHAL_MM_STREAM_TYPE_MPEG1_2_AUDIO):
            bthalStatus = bthalMmSelectMp3Parameters(configInfo);
            break;

        default:

            Assert(0);
            break;
    }
    
    return bthalStatus;
}


/*-------------------------------------------------------------------------------
 * BTHAL_MM_CodecInfoConfiguredInd()
 */
BthalStatus BTHAL_MM_CodecInfoConfiguredInd(BthalMmConfigParams *codecInfo, 
                                            BTHAL_U16 packetSize, 
                                            BTHAL_U16 sbcFrameLen, 
                                            BTHAL_U32 streamId)
{
    streamId = streamId;

    switch(codecInfo->streamType)
    {
        case (BTHAL_MM_STREAM_TYPE_SBC):
            BTL_LOG_DEBUG(("BTHAL_MM_CodecInfoConfiguredInd: packetSize=%d, sbcFrameLen=%d, streamId=%d",
                packetSize, sbcFrameLen, streamId));
            OS_MemCopy((U8 *)(&SelectedSbcCodec), (U8 *)(&(codecInfo->p.sbcInfo)), sizeof(BthalMmSbcInfo));
            CodecAndFrameInfo.bthalMmPacketSize = packetSize; 
            CodecAndFrameInfo.bthalMmSbcFrameLen = sbcFrameLen;

            switch(codecInfo->p.sbcInfo.blockLength)
            {
                case BTHAL_MM_SBC_BLOCK_LENGTH_16:
                    CodecAndFrameInfo.bthalMmNumBlocks = 16;
                    break;

                case BTHAL_MM_SBC_BLOCK_LENGTH_12:
                    CodecAndFrameInfo.bthalMmNumBlocks = 12;
                    break;

                case BTHAL_MM_SBC_BLOCK_LENGTH_8:
                    CodecAndFrameInfo.bthalMmNumBlocks = 8;
                    break;

                case BTHAL_MM_SBC_BLOCK_LENGTH_4:
                    CodecAndFrameInfo.bthalMmNumBlocks = 4;
                    break;
            }

            if (codecInfo->p.sbcInfo.subbands == BTHAL_MM_SBC_SUBBANDS_8)
                CodecAndFrameInfo.bthalMmNumSubBands = 8;
            else
                CodecAndFrameInfo.bthalMmNumSubBands = 4;
    
            break;

        case (BTHAL_MM_STREAM_TYPE_MPEG1_2_AUDIO):
            mp3MaxPacketSize = packetSize;

            break;

        default:
            Assert(0);
            break;
    }
    
    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_MM_RequestMoreData()
 */
BthalStatus BTHAL_MM_RequestMoreData(BthalMmDataBuffer *buffer)
{
    return (requestMoreDataFuncTable[currentStream](buffer));
}


#if 0

/* 
This function is reading whole MP3 frames into fragments of L2CAP packets.
It was replaced by a function reading raw MP3 data in order to use the L2CAP
MTU of the peer, thus better utilising the air BW.
*/

/*-------------------------------------------------------------------------------
 * BTHAL_MM_RequestMoreDataMp3()
 */
static BthalStatus BTHAL_MM_RequestMoreDataMp3(BthalMmDataBuffer *buffer)
{
    BTHAL_U16 len;
    BTHAL_U8 *ptr, i;
    BTHAL_U16 frameLen;
    BTHAL_U16 numFramesPerPacket;
    bthalMp3Block *block = &(mp3Blocks[0]);
    BTHAL_U8 *mp3FrameBuffer = block->mp3FrameBuffer;

    if (block->used == TRUE)
        return (BTHAL_STATUS_FAILED);

    /* For now, assume only MP3 can be pulled sync from MM */
    buffer->streamType = BTHAL_MM_STREAM_TYPE_MPEG1_2_AUDIO;
    buffer->descriptor = NUM_PCM_BLOCKS;    /* MP3 descriptors must be greater than (NUM_PCM_BLOCKS-1) */

    if (mp3StreamInfo.bytesToSend == 0)
    {
REPEAT_MP3: 
        /* Read whole frames */
        len = AppMp3ReadFrames(fpMp3, mp3FrameBuffer, MP3_MAX_READ_BUFFER, &mp3StreamInfo);

        if (len)
        {
            buffer->data = mp3FrameBuffer;
            //buffer->dataLength = len;
            
            mp3StreamInfo.totalDataLen = 0;
        }
        else
        {
            /* Start playing the file from the beginning */
            AppMp3OpenFile(0);
            goto REPEAT_MP3;
        }
    }
    else
    {
        buffer->data = mp3FrameBuffer + mp3StreamInfo.totalDataLen;
    }

    buffer->frameOffset = 0;
    
    /* MP3 data is ready */
    if ((mp3StreamInfo.offset != 0) || 
        ((mp3StreamInfo.bytesToSend / mp3StreamInfo.numFrames) > mp3MaxPacketSize)) 
    {
        /* Fragmented frames */
        buffer->frameOffset = mp3StreamInfo.offset;

        if (mp3StreamInfo.offset == 0)
        {
            /* First part of frame */
            frameLen = AppMp3GetFrameLen(buffer->data, &mp3StreamInfo);
            mp3StreamInfo.frameLen = frameLen;
        }
        else
        {
            /* Frame cont', recover current frame length */
            frameLen = mp3StreamInfo.frameLen;
        }

        if ((frameLen - mp3StreamInfo.offset) <= mp3MaxPacketSize) 
        {
            buffer->dataLength = (BTHAL_U16)(frameLen - mp3StreamInfo.offset);
            mp3StreamInfo.offset = (BTHAL_U16)0;
            mp3StreamInfo.numFrames--;
        } 
        else 
        {
            buffer->dataLength = mp3MaxPacketSize;
            mp3StreamInfo.offset = (BTHAL_U16)(mp3StreamInfo.offset + mp3MaxPacketSize);
        }
    } 
    else 
    {
        /* Whole frames */
        ptr = buffer->data;
        buffer->dataLength = 0;
        frameLen = AppMp3GetFrameLen(ptr, &mp3StreamInfo);
        numFramesPerPacket = (BTHAL_U16)(min(mp3StreamInfo.numFrames, mp3MaxPacketSize / frameLen));
        for (i = 0; i < numFramesPerPacket; i++) 
        {
            frameLen = AppMp3GetFrameLen(ptr, &mp3StreamInfo);
            ptr += frameLen;
            buffer->dataLength = (BTHAL_U16)(buffer->dataLength + frameLen);
            mp3StreamInfo.numFrames--;
        }
    }

    mp3StreamInfo.bytesToSend = (BTHAL_U16)(mp3StreamInfo.bytesToSend - min(mp3StreamInfo.bytesToSend, buffer->dataLength));

    mp3StreamInfo.totalDataLen = (BTHAL_U16)(mp3StreamInfo.totalDataLen + buffer->dataLength);

    block->used = TRUE;

    return (BTHAL_STATUS_SUCCESS);
}
#endif


/*-------------------------------------------------------------------------------
 * BTHAL_MM_RequestMoreDataMp3()
 */
static BthalStatus BTHAL_MM_RequestMoreDataMp3(BthalMmDataBuffer *buffer)
{
    bthalMp3Block *block = &(mp3Blocks[0]);
    BTHAL_U8 *mp3FrameBuffer = block->mp3FrameBuffer;
	S32 length;

    if (block->used == TRUE)
        return (BTHAL_STATUS_FAILED);

	/* Reading raw MP3 data (with no regard to the MP3 frames) */

REPEAT_MP3:

	length = fread(mp3FrameBuffer, 1, min(mp3MaxPacketSize, MP3_MAX_READ_BUFFER), fpMp3);

	if (length > 0)
	{
		block->used = TRUE;

		/* For now, assume only MP3 can be pulled sync from MM */
		buffer->descriptor = NUM_PCM_BLOCKS;    /* MP3 descriptors must be greater than (NUM_PCM_BLOCKS-1) */
		buffer->data = mp3FrameBuffer;
		buffer->dataLength = (BTHAL_U16)length;
	    buffer->streamType = BTHAL_MM_STREAM_TYPE_MPEG1_2_AUDIO;
	    
		/* Set frame offset of the beggining of the buffer */
		buffer->frameOffset = mp3StreamInfo.offset;

		/* Calculate the offset of the last frame in the buffer */
		mp3StreamInfo.offset =  (BTHAL_U16)(mp3StreamInfo.offset + (length % mp3StreamInfo.frameLen));
		mp3StreamInfo.offset = (BTHAL_U16)(mp3StreamInfo.offset % (BTHAL_U16)(mp3StreamInfo.frameLen));

		return (BTHAL_STATUS_SUCCESS);
	}

	/* Start playing the file from the beginning */
    AppMp3OpenFile(0);
    goto REPEAT_MP3;
}


/*-------------------------------------------------------------------------------
 * BTHAL_MM_RequestMoreDataPcm()
 */
static BthalStatus BTHAL_MM_RequestMoreDataPcm(BthalMmDataBuffer *buffer)
{
    /* if we are in Pull mode: */
    if (emulatePullMode )
    {
        EnterCriticalSection(&dataBufferCriticalSection);
        buffersInQ--;
        if ( !IsListEmpty(&dataBufferQueue) )
        {
            bthalDataBlock *dataBlock =
                    (bthalDataBlock *)RemoveHeadList(&dataBufferQueue);
            *buffer = dataBlock->dataBuf ;
            InsertTailList(&dataBufferPool, &(dataBlock->node));
                    LeaveCriticalSection(&dataBufferCriticalSection);
            BTL_LOG_DEBUG(("RequestMoreData, left in Q: %d", buffersInQ));
            return BTHAL_STATUS_SUCCESS;
        }
        else
        {
                LeaveCriticalSection(&dataBufferCriticalSection);
            BTL_LOG_DEBUG(("RequestMoreData: dataBufferQueue is empty, left in Q: %d", buffersInQ));

            return BTHAL_STATUS_PENDING;
        }
    }
    else
    {
        static int errCnt = 0;
        if ( (++errCnt < 5) || ((errCnt % 64) == 0))
        {
            BTL_LOG_INFO(("BTHAL_MM_RequestMoreData not supported in Push mode"));
            AppReport("BTHAL_MM_RequestMoreData not supported in Push mode");
        }
        return BTHAL_STATUS_NOT_SUPPORTED;
    }
}

/*-------------------------------------------------------------------------------
 * BTHAL_MM_RequestMoreDataSbc()
 */
static BthalStatus BTHAL_MM_RequestMoreDataSbc(BthalMmDataBuffer *buffer)
{
    U16 len;
    U32 idx;
    bthalSbcBlock *block = NULL;

    for (idx = 0; idx < NUM_SBC_BLOCKS; idx++)
    {
        block = &(sbcBlocks[idx]);
        if (block->used == FALSE)
            break;
    }

    if (idx == NUM_SBC_BLOCKS)
    {
        /* No more free SBC blocks */
//      BTL_LOG_ERROR(("BTHAL_MM_RequestMoreDataSbc failure - all SBC blocks are in use"));
        return BTHAL_STATUS_FAILED;
    }

REPEAT_SBC: 
    
    len = bthalMmReadSbcFrames(
        fpSbc, block->sbcFrameBuffer, CodecAndFrameInfo.bthalMmPacketSize, &sbcStreamInfo);

    if ( len > 0 )
    {
        buffer->streamType  = BTHAL_MM_STREAM_TYPE_SBC;
        buffer->descriptor  = SBC_FIRST_DESCRIPTOR + idx;
        buffer->data            = block->sbcFrameBuffer;
        buffer->dataLength  = len;
        block->used = TRUE;
        sbcBlockCnt++;
        return (BTHAL_STATUS_SUCCESS);
    }
    
    /* Start playing the file from the beginning */
    AppSbcOpenFile(0);
    goto REPEAT_SBC;
}


/*-------------------------------------------------------------------------------
 * BTHAL_MM_FreeDataBuf()
 */
BthalStatus BTHAL_MM_FreeDataBuf(BthalMmDataBufferDescriptor dataDescriptor)
{
    /* PCM descriptors range is [0..NUM_PCM_BLOCKS-1] */ 
    /* MP3 blocks descriptors range is [NUM_PCM_BLOCKS, (NUM_PCM_BLOCKS + NUM_MP3_BLOCKS - 1)] */
    MMRESULT  res;

    if (dataDescriptor < NUM_PCM_BLOCKS)    /* PCM block */
    {
        /* Unprepare the block */
        if ((res = waveInUnprepareHeader(hWaveIn, 
                                  &inBlock[dataDescriptor].wavHdr, 
                                  sizeof(WAVEHDR))) == MMSYSERR_NOERROR) {
            //  BTL_LOG_DEBUG(("BTHAL_MM_FreeDataBuf waveInUnprepareHeader(%d) success", dataDescriptor));
            inBlock[dataDescriptor].prepared = FALSE;
        if ( startAudio ) /* don't prepare the next block if we are aready told to stop playing */
        {
                OS_MemSet((U8*)&inBlock[dataDescriptor].wavHdr, 0, sizeof(WAVEHDR));
                inBlock[dataDescriptor].wavHdr.dwBufferLength = inBlock[dataDescriptor].dataLen;
                inBlock[dataDescriptor].wavHdr.lpData = (LPSTR)(inBlock[dataDescriptor].data);
                if (waveInPrepareHeader(hWaveIn, 
                                        &inBlock[dataDescriptor].wavHdr, 
                                        sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
                    /* Add it back into the audio input queue */
                    inBlock[dataDescriptor].prepared = TRUE;
                    EnterCriticalSection(&waveInCriticalSection);
                    InsertTailList(&inBlockPool, &inBlock[dataDescriptor].node);
                    LeaveCriticalSection(&waveInCriticalSection);
                    if (waveInAddBuffer(hWaveIn, 
                                        &inBlock[dataDescriptor].wavHdr, 
                                        sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
                    }
                }
        }
        }
        else    
            BTL_LOG_ERROR(("BTHAL_MM_FreeDataBuf: waveInUnprepareHeader(%d) failed, result = %d", dataDescriptor, res));

    }

    if ( (dataDescriptor >= SBC_FIRST_DESCRIPTOR) && (dataDescriptor <=  SBC_LAST_DESCRIPTOR))
    {
        //BTL_LOG_INFO(("BTHAL_MM_FreeDataBuf block idx %d", dataDescriptor - SBC_FIRST_DESCRIPTOR));
        sbcBlocks[(dataDescriptor - SBC_FIRST_DESCRIPTOR)].used = FALSE;
    }
    else    /* MP3 block */
    {
        mp3Blocks[(dataDescriptor - NUM_PCM_BLOCKS)].used = FALSE;
    }

    return BTHAL_STATUS_SUCCESS;
}


/*-------------------------------------------------------------------------------
 * BTHAL_MM_TransportChannelStateChanged()
 */
void BTHAL_MM_TransportChannelStateChanged(BTHAL_U32 streamId, BthalMmTransportChannelState newState)
{
    streamId = streamId;
    newState = newState;
}


/*-------------------------------------------------------------------------------
 * WaveInProc()
 *
 *      Wave input callback procedure.  Handles PCM filled data blocks.
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      
 *
 * Returns:
 *      void
 */
static void CALLBACK WaveInProc(HWAVEIN hWaveIn, UINT uMsg, 
                                DWORD dwInstance, DWORD dwParam1, 
                                DWORD dwParam2)
{
    bthalPcmBlock *pBlock;

    UNUSED_PARAMETER(hWaveIn);
    UNUSED_PARAMETER(dwInstance);
    UNUSED_PARAMETER(dwParam1);
    UNUSED_PARAMETER(dwParam2);
    
    /* Ignore calls that occur when openining and closing the device */
    if (uMsg == WIM_DATA) {
    if ( ! startAudio )
    {
        //BTL_LOG_INFO(("WaveInProc skipping callback because audio is stopped"));
        return;
    }
        EnterCriticalSection(&waveInCriticalSection);
        if (!IsListEmpty(&inBlockPool)) {
            
        BthalMmCallbackParms callbackParams;

            /* Put this block on the ready list */
            pBlock = (bthalPcmBlock *)RemoveHeadList(&inBlockPool);
            if (pBlock->dataLen == pBlock->wavHdr.dwBytesRecorded) {
                OS_MemSet((U8*)&pBlock->data[pBlock->wavHdr.dwBytesRecorded],
                          0, pBlock->dataLen - pBlock->wavHdr.dwBytesRecorded);
            }
            LeaveCriticalSection(&waveInCriticalSection);
            
        /*  we will send a callback in two cases:
            either we are in push mode,
            or if we are emulating pull mode, but  already have pending requests  for data */
        if ( ! emulatePullMode ||   (buffersInQ < 0) )
        {
            callbackParams.event = BTHAL_MM_EVENT_DATA_BUFFER_IND;
            callbackParams.p.dataBuffer.descriptor = pBlock->dataDescriptor;
            callbackParams.p.dataBuffer.dataLength = pBlock->dataLen;
            callbackParams.p.dataBuffer.data = pBlock->data;
            callbackParams.p.dataBuffer.streamType = BTHAL_MM_STREAM_TYPE_PCM;
            if ( emulatePullMode )
            {
                    EnterCriticalSection(&dataBufferCriticalSection);
                buffersInQ++;
                        LeaveCriticalSection(&dataBufferCriticalSection);
                BTL_LOG_DEBUG(("callback instead of inserting to Q, left in Q: %d",  buffersInQ));
            };
            /* Pass the event to btl_a2dp */
            bthalMmCallback(&callbackParams);
        }
        else
        {   /* we re in pull mode, and don't have any pending requests -> add to queue */
            bthalDataBlock *dataBlock;
                EnterCriticalSection(&dataBufferCriticalSection);
            if ( !IsListEmpty(&dataBufferPool) )
            {
                dataBlock = (bthalDataBlock *)RemoveHeadList(&dataBufferPool);
                dataBlock->dataBuf.descriptor   = pBlock->dataDescriptor;
                dataBlock->dataBuf.dataLength   = pBlock->dataLen;
                dataBlock->dataBuf.data     = pBlock->data;
                dataBlock->dataBuf.streamType   = BTHAL_MM_STREAM_TYPE_PCM;
                InsertTailList(&dataBufferQueue, &(dataBlock->node));
                buffersInQ++;
                        LeaveCriticalSection(&dataBufferCriticalSection);
                BTL_LOG_DEBUG(("inserted to dataBufferQueue, left in Q: %d", buffersInQ));
                if (++waveBlocksCnt == 2 )
                {
                    if (NULL != bthalMmCallback) 
                    {
                        callbackParams.event = BTHAL_MM_EVENT_START_PULL;
                        bthalMmCallback(&callbackParams);
                        BTL_LOG_INFO(("sent BTHAL_MM_EVENT_START_PULL to stack"));
                    }
                    else    /* wait until stack registers with the MM */
                        waveBlocksCnt = 0;
                }

            }
            else    
            {
                        LeaveCriticalSection(&dataBufferCriticalSection);
                BTL_LOG_INFO(("WaveInProc: dataBufferPool is empty !!!"));
            }
        }
        } 
        else {
            LeaveCriticalSection(&waveInCriticalSection);
            Assert(0);
        }
    }
    else if(uMsg == WIM_CLOSE)
    {
        AppReport("uMsg == WIM_CLOSE");
    }
    else if (uMsg == WIM_OPEN)
    {
        AppReport("uMsg == WIM_OPEN");
    }
    else
    {
        AppReport("uMsg == unknown %d", uMsg);
    }
}


/*-------------------------------------------------------------------------------
 * bthalMmSetPcmPlatformSupportedCapabilities()
 *
 *      Set the PCM specific SBC capabilities supported in this platform
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      supportedCap - supported capabilities
 *
 * Returns:
 *      void
 */
void bthalMmSetPcmPlatformSupportedCapabilities(BthalMmSbcInfo *supportedCap)
{
    platformPcmSuportedCodec.samplingFreq = supportedCap->samplingFreq;
    platformPcmSuportedCodec.channelMode = supportedCap->channelMode;
    platformPcmSuportedCodec.blockLength = supportedCap->blockLength;
    platformPcmSuportedCodec.subbands = supportedCap->subbands;
    platformPcmSuportedCodec.allocationMethod = supportedCap->allocationMethod;
    platformPcmSuportedCodec.minBitpoolValue = supportedCap->minBitpoolValue;
    platformPcmSuportedCodec.maxBitpoolValue = supportedCap->maxBitpoolValue;
}


/*-------------------------------------------------------------------------------
 * bthalMmSetMp3PlatformSupportedCapabilities()
 *
 *      Set the MP3 specific SBC capabilities supported in this platform
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      supportedCap - supported capabilities
 *
 * Returns:
 *      void
 */
void bthalMmSetMp3PlatformSupportedCapabilities(BthalMmMpeg1_2_audioInfo *supportedCap)
{
    if(supportedCap->layer != 0)
    {
        platformMp3SuportedCodec.layer = supportedCap->layer;
    }
    else
    {
        platformMp3SuportedCodec.layer = (BTHAL_MM_MPEG1_2_AUDIO_LAYER_1 | BTHAL_MM_MPEG1_2_AUDIO_LAYER_2 | BTHAL_MM_MPEG1_2_AUDIO_LAYER_3);
        AppReport("Illigal layer, default value (1, 2, 3) was set");
    }

    if(supportedCap->channelMode!= 0)
    {
        platformMp3SuportedCodec.channelMode = supportedCap->channelMode;
    }
    else
    {
        platformMp3SuportedCodec.channelMode = (BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_MONO | BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_DUAL_CHANNEL | BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_STEREO | BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_JOINT_STEREO);
        AppReport("Illigal channel mode, default value (all) was set");
    }

    if(supportedCap->bitRate!= 0)
    {
        platformMp3SuportedCodec.bitRate = supportedCap->bitRate;
    }
    else
    {
        platformMp3SuportedCodec.bitRate= 0x7FFF;;
        AppReport("Illigal bit rate, default value (all values) was set");
    }
        
    platformMp3SuportedCodec.crcProtection = supportedCap->crcProtection;
    platformMp3SuportedCodec.mpf2 = supportedCap->mpf2;
    platformMp3SuportedCodec.samplingFreq = supportedCap->samplingFreq;
    platformMp3SuportedCodec.vbr = supportedCap->vbr;
}


/*-------------------------------------------------------------------------------
 * bthalMmSetSbcMmConfiguration()
 *
 *      Set channel mode,  sample frequency, varying bitpool, and requested audio quality, specific for current audio streaming 
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      samplingFreq - sample frequency (one bit)
 *
 *      numOfChannels - One channel / Two channels (one bit)
 *
 * Returns:
 *      void
 */
void bthalMmSetSbcMmConfiguration(BthalMmPcmInfo  *reqPcmInfo)
{
    BthalMmCallbackParms callbackParams;

    /* Save selected PCM information */
    selectedPcmInfo = *reqPcmInfo;

    callbackParams.event = BTHAL_MM_EVENT_CONFIG_IND;
    
    /* Pass the event to btl_a2dp */
    bthalMmCallback(&callbackParams);
}


void bthalMmSetInputDevice(BTHAL_U32 inputDevice)
{
    bthalMmInputDevice = inputDevice;
}

/*-------------------------------------------------------------------------------
 * bthalMmSetOperationalMode()
 *
 *      set operationl mode to Push or Pull. by default we are in Push mode in Windows.
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      pullMode [in] - when TRUE - set to Pull mode emulation. else set to Push mode       
 *
 * Returns:
 *      None
 *
 */
 void bthalMmSetOperationalMode(BOOL pullMode)
{
    emulatePullMode = pullMode;
}

/*-------------------------------------------------------------------------------
 * bthalMmOpenAudio()
 *
 *      Open audio 
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      None        
 *
 * Returns:
 *      BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *      BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus bthalMmOpenAudio(void)
{
    BTHAL_U16 maxFrames, i;
    BthalStatus status = BTHAL_STATUS_SUCCESS;
    
    if (audioInOpen)
    {
        return BTHAL_STATUS_FAILED;
    }

    /* Set up the WAVEFORMATEX structure */
    switch (selectedPcmInfo.samplingFreq) 
    {
    
        case BTHAL_MM_SBC_SAMPLING_FREQ_16000:
            wfx.nSamplesPerSec = 16000;
            break;
            
        case BTHAL_MM_SBC_SAMPLING_FREQ_32000:
            wfx.nSamplesPerSec = 32000;
            break;
            
        case BTHAL_MM_SBC_SAMPLING_FREQ_44100:
            wfx.nSamplesPerSec = 44100;
            break;
            
        case BTHAL_MM_SBC_SAMPLING_FREQ_48000:
            wfx.nSamplesPerSec = 48000;
            break;

#if (BTL_CONFIG_A2DP_SAMPLE_RATE_CONVERTER == BTL_CONFIG_ENABLED)

        case BTHAL_MM_SBC_SAMPLING_FREQ_8000_EXT:
            wfx.nSamplesPerSec = 8000;
            break;

        case BTHAL_MM_SBC_SAMPLING_FREQ_11025_EXT:
            wfx.nSamplesPerSec = 11025;
            break;

        case BTHAL_MM_SBC_SAMPLING_FREQ_12000_EXT:
            wfx.nSamplesPerSec = 12000;
            break;

        case BTHAL_MM_SBC_SAMPLING_FREQ_22050_EXT:
            wfx.nSamplesPerSec = 22050;
            break;

        case BTHAL_MM_SBC_SAMPLING_FREQ_24000_EXT:
            wfx.nSamplesPerSec = 24000;
            break;
            
#endif  /* BTL_CONFIG_A2DP_SAMPLE_RATE_CONVERTER == BTL_CONFIG_ENABLED */
    }

    if (SelectedSbcCodec.channelMode == BTHAL_MM_SBC_CHANNEL_MODE_MONO)
        wfx.nChannels = 1;
    else
        wfx.nChannels = 2;

    wfx.wBitsPerSample = 16;
    wfx.cbSize = 0;
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nBlockAlign = (WORD)((wfx.wBitsPerSample >> 3) * wfx.nChannels);
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
            
    /* Open the default wave device. */
    if (waveInOpen(&hWaveIn, bthalMmInputDevice, &wfx, (DWORD)WaveInProc, 0, 
                   CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        Assert(0);
    }

    InitializeListHead(&inBlockPool);

    for (i = 0; i < NUM_PCM_BLOCKS; i++) 
    {
        /* Make the PCM block size a multiple of the stream's block
         * size requirements.  This number is based on the number of
         * samples in the SBC frame.  The multiple is based on the size of
         * the L2CAP packet and the SBC frame length.  The max number of
         * frames in one L2CAP packet is 15.
         */
        maxFrames = (BTHAL_U16)(min(15, CodecAndFrameInfo.bthalMmPacketSize / CodecAndFrameInfo.bthalMmSbcFrameLen));
        if (!maxFrames) maxFrames++;
        inBlock[i].dataLen = (BTHAL_U16)(wfx.nChannels * CodecAndFrameInfo.bthalMmNumSubBands * CodecAndFrameInfo.bthalMmNumBlocks * 2 * maxFrames);
    //  inBlock[i].dataLen -= 128; // Uzi - for testing of fractions of SBC frames
        inBlock[i].data = realloc(inBlock[i].data, inBlock[i].dataLen);
        inBlock[i].numChannels = (U8)(wfx.nChannels);
        inBlock[i].dataDescriptor = i;
        if (!inBlock[i].data) {
            return BTHAL_STATUS_FAILED;
        }
        
        InsertTailList(&inBlockPool, &inBlock[i].node);

        /* Prep the header */
        OS_MemSet((U8*)&inBlock[i].wavHdr, 0, sizeof(WAVEHDR));
        inBlock[i].wavHdr.dwBufferLength = inBlock[i].dataLen;
        inBlock[i].wavHdr.lpData = (LPSTR)(inBlock[i].data);
        if (waveInPrepareHeader(hWaveIn, &inBlock[i].wavHdr, sizeof(WAVEHDR))
            != MMSYSERR_NOERROR) {
            return BTHAL_STATUS_FAILED;
        }
        inBlock[i].prepared = TRUE;

        if (waveInAddBuffer(hWaveIn, &inBlock[i].wavHdr, sizeof(WAVEHDR)) 
            != MMSYSERR_NOERROR) {
            return BTHAL_STATUS_FAILED;
        }
    }

    /* initialize buffers requested fror pull mode */
    EnterCriticalSection(&dataBufferCriticalSection);
    InitializeListHead(&dataBufferPool);
    InitializeListHead(&dataBufferQueue);
    for (i = 0; i < NUM_DATA_BUFFERS; i++)
    {
        InsertTailList(&dataBufferPool, &(dataBuffers[i].node));
    }
    LeaveCriticalSection(&dataBufferCriticalSection);

    audioInOpen = TRUE;

    currentStream = BTHAL_A2DP_STREAM_PCM;

    return status;
}


/*-------------------------------------------------------------------------------
 * bthalMmCloseAudio()
 *
 *      Close audio 
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      None        
 *
 * Returns:
 *      BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *      BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus bthalMmCloseAudio(void)
{
    BTHAL_U16 i;
    MMRESULT  res;
    
    if (audioInOpen) {

    if ( emulatePullMode )
    {
        BthalMmCallbackParms callbackParams;
        callbackParams.event = BTHAL_MM_EVENT_STOP_PULL;
        /* Pass the event to btl_a2dp */
        bthalMmCallback(&callbackParams);
            EnterCriticalSection(&dataBufferCriticalSection);
        buffersInQ = 0;
                LeaveCriticalSection(&dataBufferCriticalSection);
        waveBlocksCnt = 0;
    }

        if (waveInReset(hWaveIn) != MMSYSERR_NOERROR) {
            return BTHAL_STATUS_FAILED;
        }

        /* Unprepare any blocks */
        for (i = 0; i < NUM_PCM_BLOCKS; i++) {
            if (inBlock[i].prepared == TRUE) {
         int cnt = 0;
               while ((res = waveInUnprepareHeader(hWaveIn, &inBlock[i].wavHdr, 
                                             sizeof(WAVEHDR))) != MMSYSERR_NOERROR)
                {
            BTL_LOG_INFO(("waveInUnprepareHeader(%d) returned %d", i, res));
            Sleep(10);
            if (cnt++ >= 10) // prevent inifinite loop. should never get here
                break;
          }
                inBlock[i].prepared = FALSE;
            }
        }

        /* Close the device */
        res = waveInClose(hWaveIn);
        if ((res != MMSYSERR_NOERROR)) {
        BTL_LOG_ERROR(("waveInClose failed, result = %d",  res));
            return BTHAL_STATUS_FAILED;
        }

        /* Free blocks was moved to BTHAL_MM_Deinit */

    audioInOpen = FALSE;
    return BTHAL_STATUS_SUCCESS;
    }
    else
    {
        BTL_LOG_INFO(("bthalMmCloseAudio() - not audioInOpen"));
        return BTHAL_STATUS_FAILED;
    }
}


/*-------------------------------------------------------------------------------
 * bthalMmStartRecording()
 *
 *      start streaming audio 
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      None        
 *
 * Returns:
 *      void
 */
void bthalMmStartRecording(void)
{
    BTL_LOG_INFO(("START bthalMmStartRecording"));
    if (!startAudio)
    {
        waveInStart(hWaveIn);
        startAudio = TRUE;
        waveBlocksCnt = 0;  
    }

    BTL_LOG_INFO(("END bthalMmStartRecording"));
}


/*-------------------------------------------------------------------------------
 * bthalMmStopRecording()
 *
 *      Stop streaming audio 
 *
 * Type:
 *      Synchronous
 *
 * Parameters:
 *      None        
 *
 * Returns:
 *      void
 */
void bthalMmStopRecording(void)
{
    BTL_LOG_INFO(("START bthalMmStopRecording"));
    if (startAudio)
    {
        startAudio = FALSE;
        waveInReset(hWaveIn);

    }
    BTL_LOG_INFO(("END bthalMmStopRecording"));
}



/* MP3 */



/*---------------------------------------------------------------------------
 *            AppMp3GetBitRate()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Returns the bit rate based on the stream info structure.
 *
 */
static U32 AppMp3GetBitRate(Mp3StreamInfo *streamInfo)
{
    U32 bitRate = 0;

    if (streamInfo->version == MP3_VERSION_1) {
        /* Version 1 */
        switch (streamInfo->layer) {
        case MP3_LAYER_1:
            bitRate = v1l1[streamInfo->brIndex];
            break;
        case MP3_LAYER_2:
            bitRate = v1l2[streamInfo->brIndex];
            break;
        case MP3_LAYER_3:
            bitRate = v1l3[streamInfo->brIndex];
            break;
        }
    } else {
        /* Version 2 or 2.5 */
        switch (streamInfo->layer) {
        case MP3_LAYER_1:
            bitRate = v2l1[streamInfo->brIndex];
            break;
        case MP3_LAYER_2:
        case MP3_LAYER_3:
            bitRate = v2l23[streamInfo->brIndex];
            break;
        }
    }

    return bitRate;
}


/*---------------------------------------------------------------------------
 *            MpGetFrameLen()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Returns the frame length based on the stream info structure.
 *
 */
static U16 AppMp3GetFrameLen(U8 *frame, Mp3StreamInfo *streamInfo)
{
    U16 frameLen;
    U32 bitRate;
    U32 sampleRate;
    U8  padding;
    U32 header = BEtoHost32(frame);

    /* Update current frame info */
    streamInfo->version = (U8)((header & 0x00180000) >> 19);
    streamInfo->layer = (U8)((header & 0x00060000) >> 17);
    streamInfo->brIndex = (U8)((header & 0x0000F000) >> 12);
    streamInfo->srIndex = (U8)((header & 0x00000C00) >> 10);
    padding = (U8)((header & 0x00000200) >> 9);

    bitRate = AppMp3GetBitRate(streamInfo);
    sampleRate = sampleRates[streamInfo->version][streamInfo->srIndex];
    
    switch (streamInfo->layer) {
    case MP3_LAYER_1:
        frameLen = (BTHAL_U16)((12 * bitRate / sampleRate + padding) * 4);
        break;
    case MP3_LAYER_2:
    case MP3_LAYER_3:
        frameLen = (U16)(144 * bitRate / sampleRate + padding);
        break;
    default:
        frameLen = 1;   /* Zero value might cause divide by zero */
        break;
    }

    return frameLen;
}


/*---------------------------------------------------------------------------
 *            AppMp3GetStreamInfo()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Gets the MP3 stream information based on the 4 byte header.
 *
 */
static BtStatus AppMp3GetStreamInfo(U8 *frame, Mp3StreamInfo *streamInfo)
{
    BtStatus status = BT_STATUS_SUCCESS;
    U32 header = BEtoHost32(frame);

    streamInfo->version     = (U8)((header & 0x00180000) >> 19);
    streamInfo->layer       = (U8)((header & 0x00060000) >> 17);
    streamInfo->crc         =  (U8) ! ((header & 0x00010000) >> 16);
    streamInfo->brIndex     = (U8)((header & 0x0000F000) >> 12);
    streamInfo->srIndex     = (U8)((header & 0x00000C00) >> 10);
    streamInfo->channelMode = (U8)((header & 0x000000C0) >> 6);
    
    return status;
}


/*---------------------------------------------------------------------------
 *            AppMp3ReadFrames()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Reads whole frames from an MP3 file.  Returns the amount of data
 *            actually read.  If at least one entire frame cannot be read, 
 *            this function returns 0.  If at least 4 bytes can be read, 
 *            this function will return valid information in the streamInfo
 *            parameter.  The streamInfo parameter applies to the last frame
 *            that was read.
 *
 */
static U16 AppMp3ReadFrames(FILE *fp, U8 *buffer, U32 len, Mp3StreamInfo *streamInfo)
{
    S32 length = 0;
    U16 totalLen = 0;
    U32 header;
    U16 frameLen;

    streamInfo->numFrames = 0;

    do { 
        /* Search for the beginning of the audio frame */
        if (fread(buffer, 1, 4, fp) == 4) {
            header = BEtoHost32(buffer);
            while ((header & 0xFFE00000) != 0xFFE00000) {
                fseek(fp, -3, SEEK_CUR);
                if (fread(buffer, 1, 4, fp) != 4) {
                    goto error_exit;
                }
                header = BEtoHost32(buffer);
            }
    
            /* Set to the beginning of the frame */
            fseek(fp, -4, SEEK_CUR);
        } else {
            /* End of data */
            goto error_exit;
        }

        if (AppMp3GetStreamInfo(buffer, streamInfo) == BT_STATUS_SUCCESS) {
            frameLen = AppMp3GetFrameLen(buffer, streamInfo);
			streamInfo->frameLen = frameLen;	/* Save the frame len */
        } else {
            goto error_exit;
        }

        /* Read the frame */
        if (len >= frameLen) {
            if ((U32)(length = fread(buffer, 1, frameLen, fp)) != frameLen) {
                fseek(fp, -length, SEEK_CUR);
                goto error_exit;
            }

        len -= length;
        buffer += length;
        totalLen = (U16)(totalLen + length);
        streamInfo->numFrames++;
            }

        streamInfo->offset = 0;

    } while (len > frameLen);  

    goto exit;

error_exit:

    length = 0;

exit:

    streamInfo->bytesToSend = totalLen;

    return totalLen;
}


/*---------------------------------------------------------------------------
 *            AppMp3OpenFile()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Opens an MP3 file and jumps to the beginning of the audio
 *            data if ID3 metadata exists.  Fills the streamInfo of the
 *            first frame in the file.
 *
 */
FILE * AppMp3OpenFile(const char *FileName)
{
    FILE *fp;
    U8    buffer[4];
    U8    flags;
    U32   length;
    BOOL  id3 = FALSE;
    Mp3StreamInfo *streamInfo;

    streamInfo = &mp3StreamInfo;

    mp3StreamInfo.numFrames = 0;
    mp3StreamInfo.bytesToSend = 0;
    mp3StreamInfo.offset = 0;
    mp3StreamInfo.totalDataLen= 0;

    if (FileName != 0)
    {
        fp = fopen(FileName, "rb");
    }
    else
    {
        fp = fpMp3;
        rewind(fp);
    }
    
    if (fp) {

        /* Look for the ID3 tag */
        if (fread(buffer, 1, 4, fp) == 4) {
            while ((BEtoHost32(buffer) & 0xFFE00000) != 0xFFE00000) {
                /* Not MP3 audio data */
                buffer[3] = 0;
                if (strcmp((BTHAL_S8 *)buffer, "ID3") == 0) {
                    /* Found the tag */
                    id3 = TRUE;
                    break;
                } else {
                    fseek(fp, -3, SEEK_CUR);
                    if (fread(buffer, 1, 4, fp) != 4) {
                        /* End of data */
                        goto error_exit;
                    }
                }
            }

            if (id3) {
                /* Get the ID3 tag flags */
                if (!fseek(fp, 5, SEEK_SET)) {
                    if (fread(&flags, 1, 1, fp) == 0) {
                       /* End of data */
                       goto error_exit;
                    }
                } else {
                    /* End of data */
                    goto error_exit;
                }

                /* Get the length of the ID3 tag */
                if (fread(buffer, 1, 4, fp) == 4) {
                    length = A2DP_SynchSafetoHost32(buffer);

                    if (flags & 0x10) {
                        /* Footer is present */
                        length += 20;
                    } else {
                        length += 10;
                    }

                    /* Go past the tag data */
                    if (fseek(fp, length, SEEK_SET)) {
                        /* End of data */
                        goto error_exit;
                    }
                }
            } else {
                fseek(fp, 0, SEEK_SET);
            }
        } else {
            /* End of data */
            goto error_exit;
        }
    } else {
        /* End of data */
        goto exit;
    }

    /* Search for the first audio frame */
    if (fread(buffer, 1, 4, fp) == 4) {
        while ((BEtoHost32(buffer) & 0xFFE00000) != 0xFFE00000) {
            fseek(fp, -3, SEEK_CUR);
            if (fread(buffer, 1, 4, fp) != 4) {
                goto error_exit;
            }
        }

        /* Set to the beginning of audio data */
        fseek(fp, -4, SEEK_CUR);
    } else {
        /* End of data */
        goto error_exit;
    }

    AppMp3ReadFrames(fp, buffer, 4, streamInfo);

    goto exit;

error_exit:

    /* Error, close the file and return 0 */
    fclose(fp);
    fp = 0;

exit:

    fpMp3 = fp;

    if (fpMp3)
    {
        BthalMmCallbackParms callbackParams;
        BthalMmConfigParams configInfo;

        if (bthalMmSelectMp3Parameters(&configInfo) == BTHAL_STATUS_SUCCESS)
        {
        callbackParams.event = BTHAL_MM_EVENT_CONFIG_IND;
        
        /* Pass the event to btl_a2dp */
        bthalMmCallback(&callbackParams);

        currentStream = BTHAL_A2DP_STREAM_MP3;
    }
        else
        {
            /* Error, close the file and return 0 */
            fclose(fp);
            fp = 0;
        }
    }
    
    return fp;
}


/*---------------------------------------------------------------------------
 *            AppMp3CloseFile()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Close an MP3 file.
 *
 */
void AppMp3CloseFile(FILE *fp)
{
    fclose(fp);
    fpMp3 = fp = 0;
}


/*---------------------------------------------------------------------------
 *            AppMp3PlayFile()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Play an MP3 file.
 *
 */
void AppMp3PlayFile(void)
{
    BthalMmCallbackParms callbackParams;

    callbackParams.event = BTHAL_MM_EVENT_START_PULL;
    
    /* Pass the event to btl_a2dp */
    bthalMmCallback(&callbackParams);
}


/*---------------------------------------------------------------------------
 *            AppMp3StopFile()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Stop playing an MP3 file.
 *
 */
void AppMp3StopFile(void)
{
    BthalMmCallbackParms callbackParams;

    callbackParams.event = BTHAL_MM_EVENT_STOP_PULL;
    
    /* Pass the event to btl_a2dp */
    bthalMmCallback(&callbackParams);
}


/*---------------------------------------------------------------------------
 *            bthalMmStreamStateIsConnected()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  update the BTHAL MM with the current stream state.
 *
 */
void bthalMmStreamStateIsConnected(U32 streamId, BOOL isConnected)
{
    remoteCodecCapabilitiesValid[streamId] = isConnected;
}


/*---------------------------------------------------------------------------
 *            bthalMmSetExternalSbcEncoder()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  set whether we are with external SBC encoder (i.e. read SBC file in BTHAL),
 *                 or with built-in SBC encoder (inside BTL)
 *
 */
void bthalMmSetExternalSbcEncoder(BOOL externalEncoder)
{
#if SBC_ENCODER == XA_ENABLED

    externalSbcEncoder = externalEncoder;

    //BTL_LOG_DEBUG(("bthalMmSetExternalSbcEncoder, new externalSbcEncoder=%d", externalSbcEncoder));

#else

    /* don't change anything, it must be an external encoder */
    externalEncoder = externalEncoder;

#endif /* SBC_ENCODER == XA_ENABLED */
}

/*---------------------------------------------------------------------------
 *            bthalMmReadSbcFrames()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Reads whole frames from an SBC file.  Returns the amount of data
 *            actually read.  If at least one entire frame cannot be read, 
 *            this function returns 0.
 */
U16 bthalMmReadSbcFrames(FILE *fp, U8 *buffer, U16 bufferLen, SbcStreamInfo *streamInfo)
{
    U16 numOfFramesRead = 0;
    U16 numOfFrames = 0;
    U16 frameLen = SBC_FrameLen(streamInfo);


    if (sbcBlockCnt == 0)
        BTL_LOG_INFO(("SBC_FrameLen = %d", frameLen));

    /* numOfFrames will contain the integer number of frames to read from the file */
    /* We assume fixed bit pool */
    numOfFrames = (U16)(bufferLen/frameLen);

    /* Read few frames from the file.  numOfFramesRead will hold the amount of whole frames actually read */
    numOfFramesRead = (U16)(fread(buffer, frameLen, numOfFrames, fp));

    /* return the number of send bytes which are a whole number of frames */
    return (U16)(numOfFramesRead * frameLen);
}

/*---------------------------------------------------------------------------
 *            SbcParseHeader()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Parses an SBC header for frame information.
 *          info is put in global variable sbcStreamInfo
 */
static BOOL SbcParseHeaderInfo(U8 *Buffer )
{
    U8 *ptr = Buffer + 1; /* to skip the frame sync byte 0x9C */
    SbcStreamInfo *streamInfo = &sbcStreamInfo;

    /* Sampling Frequency */
    streamInfo->sampleFreq = (SbcSampleFreq)(*ptr >> 6);

    /* Number of blocks */
    switch ((*ptr >> 4) & 0x03)
    {
        case 0:
            streamInfo->numBlocks = 4;
            break;
        case 1:
            streamInfo->numBlocks = 8;
            break;
        case 2:
            streamInfo->numBlocks = 12;
            break;
        case 3:
            streamInfo->numBlocks = 16;
            break;
        default:
            return FALSE;
    }

    /* Channel mode and number of channels */
    streamInfo->channelMode =  (SbcChannelMode)(((*ptr >> 2) & 0x03));
    switch (streamInfo->channelMode)
    {
        case 0:
            streamInfo->numChannels = 1;
            break;
        case 1:
        case 2:
        case 3:
            streamInfo->numChannels = 2;
            break;
        default:
            return FALSE;
    }

    /* Allocation Method */
    streamInfo->allocMethod = (SbcAllocMethod)((*ptr >> 1) & 0x01);

    /* Subbands */
    switch (*ptr++ & 0x01)
    {
        case 0:
            streamInfo->numSubBands = 4;
            break;
        case 1:
            streamInfo->numSubBands = 8;
            break;
        default:
            return FALSE;
    }

    /* Bitpool */
    streamInfo->bitPool = *ptr++;

    streamInfo->lastSbcFrameLen = 0;

    return TRUE;

}

/*---------------------------------------------------------------------------
 *            AppSbcOpenFile()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Opens an SBC file and finds the streamInfo of the first frame 
 *            in the file.
 *
 *  return TRUE upon success, else FALSE
 *
 */
 BOOL AppSbcOpenFile(const char *FileName)
{
    U8          headerBuffer[8]; /* actually 3 bytes are enough */
    FILE       *fp;
    BthalMmCallbackParms callbackParams;
    BthalMmConfigParams configInfo;

    if (FileName != 0)
    {
        /* Read the first header to get stream settings */
        fp = fopen(FileName, "rb");
    }
    else
    {
        fp = fpSbc;
        rewind(fp);
    }
    
    if (!fp) 
    {
        return FALSE;
    }

    fread(headerBuffer, 1, sizeof(headerBuffer), fp);
    if ( ! SbcParseHeaderInfo(headerBuffer) )
    {
        fclose(fp);
        AppReport("failed to parse header in SBC file");
        return FALSE;
    }
    else
    {
        BTL_LOG_INFO(("SBC header: freq %d,ChnlMode %d, BlkLen %d, subbands %d, AllocMethod %d, BitPool %d",
            sbcStreamInfo.sampleFreq, sbcStreamInfo.channelMode, sbcStreamInfo.numBlocks,
            sbcStreamInfo.numSubBands,  sbcStreamInfo.allocMethod, sbcStreamInfo.bitPool));
    }

    fseek(fp, 0, SEEK_SET);

    if (bthalMmSelectSbcParameters(&configInfo) == BTHAL_STATUS_SUCCESS)
    {
        fpSbc = fp;

        callbackParams.event = BTHAL_MM_EVENT_CONFIG_IND;
        /* Pass the event to btl_a2dp */
        bthalMmCallback(&callbackParams);
        currentStream = BTHAL_A2DP_STREAM_SBC;
    }
    else
    {
        /* Error, close the file and return fail */
        fclose(fp);
        return FALSE;
    }

    return (fp != 0);
}


/*---------------------------------------------------------------------------
 *            AppSbcCloseFile()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Close an SBC file.
 *
 */
void AppSbcCloseFile(void)
{
    Assert(fpSbc != 0);
    fclose(fpSbc);
    fpSbc = 0;
}


/*---------------------------------------------------------------------------
 *            AppSbcPlayFile()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Play an SBC file.
 *
 */
void AppSbcPlayFile(void)
{
    BthalMmCallbackParms callbackParams;
    
    Assert(fpSbc != 0);

    callbackParams.event = BTHAL_MM_EVENT_START_PULL;

    sbcBlockCnt = 0;

    /* Pass the event to btl_a2dp */
    bthalMmCallback(&callbackParams);
}


/*---------------------------------------------------------------------------
 *            AppSbcStopFile()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Stop playing an SBC file.
 *
 */
void AppSbcStopFile(void)
{
    BthalMmCallbackParms callbackParams;

    Assert(fpSbc != 0);

    callbackParams.event = BTHAL_MM_EVENT_STOP_PULL;
    
    /* Pass the event to btl_a2dp */
    bthalMmCallback(&callbackParams);

}
    

/*---------------------------------------------------------------------------
 *            bthalMmSelectMp3Parameters()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Fills the given struct with the selected MP3 parameters.
 *
 */
static BthalStatus bthalMmSelectMp3Parameters(BthalMmConfigParams *configInfo)
{
    BthalStatus bthalStatus = BTHAL_STATUS_SUCCESS;
    U32 idx;
    
    /* Set the configuration based on the stream info in the file. 
    Implicity select no CRC, no MPF-2 and no VBR. */
                     
    switch (mp3StreamInfo.layer) 
    {
    case MP3_LAYER_1:
        configInfo->p.mpeg1_2_audioInfo.layer = BTHAL_MM_MPEG1_2_AUDIO_LAYER_1;
        break;
    case MP3_LAYER_2:
        configInfo->p.mpeg1_2_audioInfo.layer = BTHAL_MM_MPEG1_2_AUDIO_LAYER_2;
        break;
    case MP3_LAYER_3:
        configInfo->p.mpeg1_2_audioInfo.layer = BTHAL_MM_MPEG1_2_AUDIO_LAYER_3;
        break;
    default:
        Assert(0);
        break;
    }
    
    switch (mp3StreamInfo.channelMode) 
    {
    case MP3_MODE_STEREO: 
        configInfo->p.mpeg1_2_audioInfo.channelMode = BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_STEREO;
        break;
    case MP3_MODE_JOINT:
        configInfo->p.mpeg1_2_audioInfo.channelMode = BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_JOINT_STEREO;
        break;
    case MP3_MODE_DUAL:
        configInfo->p.mpeg1_2_audioInfo.channelMode = BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_DUAL_CHANNEL;
        break;
    case MP3_MODE_MONO:
        configInfo->p.mpeg1_2_audioInfo.channelMode = BTHAL_MM_MPEG1_2_AUDIO_CHANNEL_MODE_MONO;
        break;
    default:
        Assert(0);
        break;
    }

    switch (sampleRates[mp3StreamInfo.version][mp3StreamInfo.srIndex]) 
    {
        case 48000:
            configInfo->p.mpeg1_2_audioInfo.samplingFreq = BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_48000;
            break;
        case 44100:
            configInfo->p.mpeg1_2_audioInfo.samplingFreq = BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_44100;
            break;
        case 32000:
            configInfo->p.mpeg1_2_audioInfo.samplingFreq = BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_32000;
            break;
        case 24000:
            configInfo->p.mpeg1_2_audioInfo.samplingFreq = BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_24000;
            break;
        case 22050:
            configInfo->p.mpeg1_2_audioInfo.samplingFreq = BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_22050;
            break;
        case 16000:
            configInfo->p.mpeg1_2_audioInfo.samplingFreq = BTHAL_MM_MPEG1_2_AUDIO_SAMPLING_FREQ_16000;
            break;
        default:
            Assert(0);
            break;
    }

    if (mp3StreamInfo.brIndex < 15) 
    {
        configInfo->p.mpeg1_2_audioInfo.bitRate = (U16)(1 << mp3StreamInfo.brIndex);
        //StoreBE16((U8*)(&(configInfo->p.mpeg1_2_audioInfo.bitRate)), (U16)(1 << mp3StreamInfo.brIndex));
    } 
    else 
    {
        Assert(0);
    }

    configInfo->p.mpeg1_2_audioInfo.curBitRate = AppMp3GetBitRate(&mp3StreamInfo);

    if (mp3StreamInfo.crc)
        configInfo->p.mpeg1_2_audioInfo.crcProtection = BTHAL_MM_MPEG1_2_AUDIO_CRC_PROTECTION_SUPPORTED;
    else
        configInfo->p.mpeg1_2_audioInfo.crcProtection = BTHAL_MM_MPEG1_2_AUDIO_CRC_PROTECTION_NOT_SUPPORTED;

    configInfo->p.mpeg1_2_audioInfo.mpf2 = BTHAL_MM_MPEG1_2_AUDIO_MPF2_NOT_SUPPORTED;
    configInfo->p.mpeg1_2_audioInfo.vbr = BTHAL_MM_MPEG1_2_AUDIO_VBR_NOT_SUPPORTED;

    /* Verify the selected codec is supported with local capablities */
    configInfo->p.mpeg1_2_audioInfo.layer &= platformMp3SuportedCodec.layer;
    configInfo->p.mpeg1_2_audioInfo.channelMode &= platformMp3SuportedCodec.channelMode;
    configInfo->p.mpeg1_2_audioInfo.samplingFreq &= platformMp3SuportedCodec.samplingFreq;
    configInfo->p.mpeg1_2_audioInfo.bitRate &= platformMp3SuportedCodec.bitRate;

    /* Verify the selected codec is supported with remote capablities for streams 2/3 */
    for (idx=2; idx<4; idx++)
    {
        if (remoteCodecCapabilitiesValid[idx] == TRUE)
        {
            configInfo->p.mpeg1_2_audioInfo.layer &= remoteCodecCapabilities[idx].p.mpeg1_2_audioInfo.layer;
            configInfo->p.mpeg1_2_audioInfo.channelMode &= remoteCodecCapabilities[idx].p.mpeg1_2_audioInfo.channelMode;
            configInfo->p.mpeg1_2_audioInfo.samplingFreq &= remoteCodecCapabilities[idx].p.mpeg1_2_audioInfo.samplingFreq;
            configInfo->p.mpeg1_2_audioInfo.bitRate &= remoteCodecCapabilities[idx].p.mpeg1_2_audioInfo.bitRate;
        }
    }

    if ((!configInfo->p.mpeg1_2_audioInfo.layer) || 
        (!configInfo->p.mpeg1_2_audioInfo.channelMode) || 
        (!configInfo->p.mpeg1_2_audioInfo.samplingFreq) || 
        (!configInfo->p.mpeg1_2_audioInfo.bitRate))
    {
        AppReport("Invalid MP3 file parameters!");
        bthalStatus =  BTHAL_STATUS_FAILED;
    }

    return bthalStatus;
}


/*---------------------------------------------------------------------------
 *            bthalMmSelectSbcParameters()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Fills the given struct with the selected SBC parameters.
 *
 */
BthalStatus  bthalMmSelectSbcParameters(BthalMmConfigParams *configInfo)
{
    BthalStatus bthalStatus = BTHAL_STATUS_SUCCESS;
    U8 maxBitpool, minBitpool;
    U32 idx;

    switch (sbcStreamInfo.channelMode) 
    {
        case SBC_CHNL_MODE_MONO: 
            configInfo->p.sbcInfo.channelMode = BTHAL_MM_SBC_CHANNEL_MODE_MONO;
            break;
        case SBC_CHNL_MODE_DUAL_CHNL:
            configInfo->p.sbcInfo.channelMode = BTHAL_MM_SBC_CHANNEL_MODE_DUAL_CHANNEL;
            break;
        case SBC_CHNL_MODE_STEREO:
            configInfo->p.sbcInfo.channelMode = BTHAL_MM_SBC_CHANNEL_MODE_STEREO;
            break;
        case SBC_CHNL_MODE_JOINT_STEREO:
            configInfo->p.sbcInfo.channelMode = BTHAL_MM_SBC_CHANNEL_MODE_JOINT_STEREO;
            break;
        default:
            Assert(0);
            break;
    }

    switch (sbcStreamInfo.sampleFreq) 
    {
        case SBC_CHNL_SAMPLE_FREQ_16:
            configInfo->p.sbcInfo.samplingFreq = BTHAL_MM_SBC_SAMPLING_FREQ_16000;
            break;
        case SBC_CHNL_SAMPLE_FREQ_32:
            configInfo->p.sbcInfo.samplingFreq = BTHAL_MM_SBC_SAMPLING_FREQ_32000;
            break;
        case SBC_CHNL_SAMPLE_FREQ_44_1:
            configInfo->p.sbcInfo.samplingFreq = BTHAL_MM_SBC_SAMPLING_FREQ_44100;
            break;
        case SBC_CHNL_SAMPLE_FREQ_48:
            configInfo->p.sbcInfo.samplingFreq = BTHAL_MM_SBC_SAMPLING_FREQ_48000;
            break;
        default:
            Assert(0);
            break;
    }
    
    switch (sbcStreamInfo.numSubBands) 
    {
        case 4:
            configInfo->p.sbcInfo.subbands = BTHAL_MM_SBC_SUBBANDS_4;
            break;
        case 8:
            configInfo->p.sbcInfo.subbands = BTHAL_MM_SBC_SUBBANDS_8;
            break;
        default:
            Assert(0);
            break;
    }
    
    switch (sbcStreamInfo.numBlocks) 
    {
        case 4:
            configInfo->p.sbcInfo.blockLength = BTHAL_MM_SBC_BLOCK_LENGTH_4;
            break;
        case 8:
            configInfo->p.sbcInfo.blockLength = BTHAL_MM_SBC_BLOCK_LENGTH_8;
            break;
        case 12:
            configInfo->p.sbcInfo.blockLength = BTHAL_MM_SBC_BLOCK_LENGTH_12;
            break;
        case 16:
            configInfo->p.sbcInfo.blockLength = BTHAL_MM_SBC_BLOCK_LENGTH_16;
            break;
        default:
            Assert(0);
            break;
    }

    switch (sbcStreamInfo.allocMethod) 
    {
        case SBC_ALLOC_METHOD_LOUDNESS:
            configInfo->p.sbcInfo.allocationMethod = BTHAL_MM_SBC_ALLOCATION_METHOD_LOUDNESS;
            break;
        case SBC_ALLOC_METHOD_SNR:
            configInfo->p.sbcInfo.allocationMethod = BTHAL_MM_SBC_ALLOCATION_METHOD_SNR;
            break;
        default:
            Assert(0);
            break;
    }
    
    configInfo->p.sbcInfo.maxBitpoolValue = sbcStreamInfo.bitPool;
    configInfo->p.sbcInfo.minBitpoolValue  = sbcStreamInfo.bitPool;

    /* Verify the selected codec is supported with local capablities */
    configInfo->p.sbcInfo.channelMode &= platformPcmSuportedCodec.channelMode;
    configInfo->p.sbcInfo.samplingFreq &= platformPcmSuportedCodec.samplingFreq;
    configInfo->p.sbcInfo.blockLength &= platformPcmSuportedCodec.blockLength;
    configInfo->p.sbcInfo.subbands &= platformPcmSuportedCodec.subbands;
    configInfo->p.sbcInfo.allocationMethod &= platformPcmSuportedCodec.allocationMethod;
    minBitpool = platformPcmSuportedCodec.minBitpoolValue;
    maxBitpool = platformPcmSuportedCodec.maxBitpoolValue;

    /* Verify the selected codec is supported with remote capablities for streams 0/1 */
    for (idx=0; idx<2; idx++)
    {
        if (remoteCodecCapabilitiesValid[idx] == TRUE)
        {
            configInfo->p.sbcInfo.channelMode &= remoteCodecCapabilities[idx].p.sbcInfo.channelMode;
            configInfo->p.sbcInfo.samplingFreq &= remoteCodecCapabilities[idx].p.sbcInfo.samplingFreq;
            configInfo->p.sbcInfo.blockLength &= remoteCodecCapabilities[idx].p.sbcInfo.blockLength;
            configInfo->p.sbcInfo.subbands &= remoteCodecCapabilities[idx].p.sbcInfo.subbands;
            configInfo->p.sbcInfo.allocationMethod &= remoteCodecCapabilities[idx].p.sbcInfo.allocationMethod;
            minBitpool = (BTHAL_U8)(max(remoteCodecCapabilities[idx].p.sbcInfo.minBitpoolValue, minBitpool));
            maxBitpool = (BTHAL_U8)(min(remoteCodecCapabilities[idx].p.sbcInfo.maxBitpoolValue, maxBitpool));
        }
    }


    if (    (!configInfo->p.sbcInfo.channelMode)    ||
        (!configInfo->p.sbcInfo.samplingFreq)   ||
        (!configInfo->p.sbcInfo.blockLength)    ||
        (!configInfo->p.sbcInfo.subbands)       ||
        (!configInfo->p.sbcInfo.allocationMethod)||
        ( sbcStreamInfo.bitPool < minBitpool )  ||
        ( sbcStreamInfo.bitPool > maxBitpool ))
    {
        bthalStatus =  BTHAL_STATUS_FAILED;
        BTL_LOG_ERROR(("bthalMmSelectSbcParameters failed. masks: "
                "chnlMode %x, Freq %x, blkLen %x, subbands %x, allocMethod %x. bp: %d, min %d, max %d",
            (configInfo->p.sbcInfo.channelMode),
            (configInfo->p.sbcInfo.samplingFreq),
            (configInfo->p.sbcInfo.blockLength),
            (configInfo->p.sbcInfo.subbands),
            (configInfo->p.sbcInfo.allocationMethod),
            sbcStreamInfo.bitPool, minBitpool, maxBitpool));
    }
    return bthalStatus;

}

/*---------------------------------------------------------------------------
 *            bthalMmStartPcmStream()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Configures and start the PCM stream for assisted A2DP
 *
 */
void bthalMmStartPcmStream (void)
{
    BtStatus status;

    switch (selectedPcmInfo.samplingFreq) 
    {
        case BTHAL_MM_SBC_SAMPLING_FREQ_16000:
            bthalMmAcmConfig.sampleRate = ACM_CODEC_SAMPLE_RATE_16000;
            break;
        
        case BTHAL_MM_SBC_SAMPLING_FREQ_32000:
            bthalMmAcmConfig.sampleRate = ACM_CODEC_SAMPLE_RATE_32000;
            break;
        
        case BTHAL_MM_SBC_SAMPLING_FREQ_44100:
            bthalMmAcmConfig.sampleRate = ACM_CODEC_SAMPLE_RATE_44100;
            break;
        
        case BTHAL_MM_SBC_SAMPLING_FREQ_48000:
            bthalMmAcmConfig.sampleRate = ACM_CODEC_SAMPLE_RATE_48000;
            break;
        
        case BTHAL_MM_SBC_SAMPLING_FREQ_8000_EXT:
            bthalMmAcmConfig.sampleRate = ACM_CODEC_SAMPLE_RATE_8000;
            break;
        
        case BTHAL_MM_SBC_SAMPLING_FREQ_11025_EXT:
            bthalMmAcmConfig.sampleRate = ACM_CODEC_SAMPLE_RATE_11025;
            break;
        
        case BTHAL_MM_SBC_SAMPLING_FREQ_12000_EXT:
            bthalMmAcmConfig.sampleRate = ACM_CODEC_SAMPLE_RATE_12000;
            break;
        
        case BTHAL_MM_SBC_SAMPLING_FREQ_22050_EXT:
            bthalMmAcmConfig.sampleRate = ACM_CODEC_SAMPLE_RATE_22050;
            break;
        
        case BTHAL_MM_SBC_SAMPLING_FREQ_24000_EXT:
            bthalMmAcmConfig.sampleRate = ACM_CODEC_SAMPLE_RATE_24000;
            break;
        default:
            BTL_LOG_ERROR(("bthalMmStartPcmStream: unrecognized PCM sample freq %d",
                          selectedPcmInfo.samplingFreq));
            break;
    }

    /* Configure host platform's CODEC */
    status = ACM_ConfigureResource(ACM_AUDIO_RESOURCE_ID_CODEC,
                                   &bthalMmAcmConfig,
                                   bthalMmAcmCallback);
    
    if ((BT_STATUS_SUCCESS != status) && (BT_STATUS_PENDING != status))
    {
        BTL_LOG_ERROR(("bthalMmStartPcmStream: configuring host platform's CODEC failed"));
    }
}

/*---------------------------------------------------------------------------
 *            bthalMmAcmCallback()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Callback function for receiving notifications from Audio
 *            Configuration Manager.
 *
 * Return:    None.
 *
 */
static void bthalMmAcmCallback(const AcmEvent *acmEvent)
{
    switch(acmEvent->type)
    {
        case ACM_EVENT_RESOURCE_CONFIGURED:
            /* Notify the A2DP app that the assisted A2DP platform specific
             * configuration is done */
            appA3dpPcmStreamStarted();
    	    break;

        case ACM_EVENT_RESOURCE_CONFIGURATION_FAILED:
            BTL_LOG_ERROR(("bthalMmAcmCallback: CODEC configuration failed"));
            break;

        default:
            BTL_LOG_ERROR(("bthalMmAcmCallback: unknown ACM event type %d",
                           acmEvent->type));
            break;
    }
}

/*---------------------------------------------------------------------------
 *            bthalMmA3dpHciCB()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  CB for BTHAL MM HCI commands
 *
 */
void bthalMmA3dpHciCB(const BtEvent *pEvent)
{
    BTL_LOG_INFO(("bthalMmA3dpHciCB: command status %d", pEvent->p.meToken->p.general.out.status));

    /* notify the A2DP app that the assisted A2DP platform specific configuration is done */
    appA3dpPcmStreamStarted();
}

/* TODO ronenk: add stop PCM stream functions? */

#else /*BTL_CONFIG_A2DP ==   BTL_CONFIG_ENABLED*/

/*-------------------------------------------------------------------------------
 * BTHAL_MM_Init() - When  BTL_CONFIG_A2DP is disabled.
 */
BthalStatus  BTHAL_MM_Init(void)
{
    
   BTL_LOG_INFO(("BTHAL_MM_Init()  -  BTL_CONFIG_A2DP Disabled"));
  

    return BT_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * BTHAL_MM_Deinit() - When  BTL_CONFIG_A2DP is disabled.
 */
BthalStatus BTHAL_MM_Deinit(void)
{
    BTL_LOG_INFO(("BTHAL_MM_Deinit() -  BTL_CONFIG_A2DP Disabled"));

    return BT_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------*/



#endif /*BTL_CONFIG_A2DP ==   BTL_CONFIG_ENABLED*/






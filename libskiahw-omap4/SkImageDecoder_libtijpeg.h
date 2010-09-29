
/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */

#include "SkBitmap.h"
#include "SkStream.h"
#include "SkAllocator.h"
#include "SkImageDecoder.h"
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <utils/threads.h>
#include "SkTime.h"
#include <utils/Log.h>


extern "C" {
#include "OMX_Component.h"
#include "OMX_IVCommon.h"
#include "OMX_TI_IVCommon.h"
#include "OMX_TI_Index.h"
#include "memmgr.h"
#include "tiler.h"
#include <timm_osal_interfaces.h>
#include <timm_osal_trace.h>


};


namespace android {
    Mutex       gTIJpegDecMutex;
}; //namespace android

///LIBSKIAHW Logging Functions
#define ENABLE_LOGD
#ifdef ENABLE_LOGD
#define LIBSKIAHW_LOGDA(str) LOGD("%s:%d %s - " str,__FILE__, __LINE__,__FUNCTION__);
#define LIBSKIAHW_LOGDB(str, ...) LOGD("%s:%d %s - " str,__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);
#define LIBSKIAHW_LOGEA(str) LOGE("%s:%d %s - " str,__FILE__, __LINE__, __FUNCTION__);
#define LIBSKIAHW_LOGEB(str, ...) LOGE("%s:%d %s - " str,__FILE__, __LINE__,__FUNCTION__, __VA_ARGS__);
#define LOG_FUNCTION_NAME         LOGD("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    LOGD("%d: %s() EXIT", __LINE__, __FUNCTION__);
#else
#define LIBSKIAHW_LOGDA(str)    TIMM_OSAL_InfoExt(TIMM_OSAL_TRACEGRP_OMXIMGDEC, "%s:%d %s - " str,__FILE__, __LINE__,__FUNCTION__);
#define LIBSKIAHW_LOGDB(str, ...) TIMM_OSAL_InfoExt(TIMM_OSAL_TRACEGRP_OMXIMGDEC, "%s:%d %s - " str,__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);
#define LIBSKIAHW_LOGEA(str) TIMM_OSAL_InfoExt(TIMM_OSAL_TRACEGRP_OMXIMGDEC, "%s:%d %s - " str,__FILE__, __LINE__, __FUNCTION__);
#define LIBSKIAHW_LOGEB(str, ...) TIMM_OSAL_InfoExt(TIMM_OSAL_TRACEGRP_OMXIMGDEC, "%s:%d %s - " str,__FILE__, __LINE__,__FUNCTION__, __VA_ARGS__);
#define LOG_FUNCTION_NAME         TIMM_OSAL_InfoExt(TIMM_OSAL_TRACEGRP_OMXIMGDEC, "%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    TIMM_OSAL_InfoExt(TIMM_OSAL_TRACEGRP_OMXIMGDEC, "%d: %s() EXIT", __LINE__, __FUNCTION__);
#endif

/* Maximum Number of ports for the Jpeg Decoder Comp */
#define  OMX_JPEGD_TEST_NUM_PORTS   2

/* Input port Index for the Jpeg Decoder OMX Comp */
#define OMX_JPEGD_TEST_INPUT_PORT   0

/* Output port Index for the Jpeg Decoder OMX Comp */
#define OMX_JPEGD_TEST_OUTPUT_PORT  1

#define OMX_JPEGD_TEST_PIPE_SIZE      1024
#define OMX_JPEGD_TEST_PIPE_MSG_SIZE  4 //128 /* Do not want to use this so check */


#define M_SOF0  0xC0            /* nStart Of Frame N*/
#define M_SOF1  0xC1            /* N indicates which compression process*/
#define M_SOF2  0xC2            /* Only SOF0-SOF2 are now in common use*/
#define M_SOF3  0xC3
#define M_SOF5  0xC5            /* NB: codes C4 and CC are NOT SOF markers*/
#define M_SOF6  0xC6
#define M_SOF7  0xC7
#define M_SOF9  0xC9
#define M_SOF10 0xCA
#define M_SOF11 0xCB
#define M_SOF13 0xCD
#define M_SOF14 0xCE
#define M_SOF15 0xCF
#define M_SOI   0xD8            /* nStart Of Image (beginning of datastream)*/
#define M_EOI   0xD9            /* End Of Image (end of datastream)*/
#define M_SOS   0xDA            /* nStart Of Scan (begins compressed data)*/
#define M_JFIF  0xE0            /* Jfif marker*/
#define M_EXIF  0xE1            /* Exif marker*/
#define M_COM   0xFE            /* COMment */
#define M_DQT   0xDB
#define M_DHT   0xC4
#define M_DRI   0xDD

class AutoTimeMillis;

class SkJPEGImageDecoder : public SkImageDecoder {
public:
    virtual Format getFormat() const {
        return kJPEG_Format;
    }

protected:
    virtual bool onDecode(SkStream* stream, SkBitmap* bm,
                          Mode);
};


class SkTIJPEGImageDecoder :public SkImageDecoder
{
protected:
    virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode);

public:

    enum JPEGDEC_State
    {
        STATE_LOADED, // 0
        STATE_IDLE, // 1
        STATE_EXECUTING, // 2
        STATE_FILL_BUFFER_DONE_CALLED, // 3
        STATE_EMPTY_BUFFER_DONE_CALLED, // 4
        STATE_ERROR, // 5
        STATE_EXIT // 6
    };

    typedef struct JpegDecoderParams
    {
        // Quatization Table
        // Huffman Table
        // SectionDecode;
        // SubRegionDecode
    }JpegDecoderParams;

    SkTIJPEGImageDecoder();
    ~SkTIJPEGImageDecoder();
    bool SetJpegDecodeParameters(JpegDecoderParams * jdp) {memcpy(&jpegDecParams, jdp, sizeof(JpegDecoderParams)); return true;}
    virtual Format getFormat() const { return kJPEG_Format; }
    void Run();
    void PrintState();
    void FillBufferDone(OMX_U8* pBuffer, OMX_U32 nFilledLen);
    void EventHandler(OMX_HANDLETYPE hComponent,
                                        OMX_EVENTTYPE eEvent,
                                        OMX_U32 nData1,
                                        OMX_U32 nData2,
                                        OMX_PTR pEventData);

private:

    typedef struct JPEG_HEADER_INFO {
        int nWidth;
        int nHeight ;
        int nFormat;
        int nProgressive;
    } JPEG_HEADER_INFO;

        OMX_HANDLETYPE pOMXHandle;
        SkJPEGImageDecoder *pARMHandle;
        AutoTimeMillis *pDecodeTime, *pBeforeDecodeTime, *pAfterDecodeTime;
        OMX_BUFFERHEADERTYPE *pInBuffHead;
        OMX_BUFFERHEADERTYPE *pOutBuffHead;
        OMX_PARAM_PORTDEFINITIONTYPE InPortDef;
        OMX_PARAM_PORTDEFINITIONTYPE OutPortDef;
        JpegDecoderParams jpegDecParams;
        SkStream* inStream;
        SkBitmap* bitmap;
        TIHeapAllocator allocator;

    OMX_S16 GetYUVformat(OMX_U8 * Data);
    OMX_S16 Get16m(const void * Short);
    OMX_S32 ParseJpegHeader (SkStream* stream, JPEG_HEADER_INFO* JpegHeaderInfo);
    OMX_S32 fill_data(OMX_BUFFERHEADERTYPE *pBuf, SkStream* stream, OMX_S32 bufferSize);
    void FixFrameSize(JPEG_HEADER_INFO* JpegHeaderInfo);
    bool IsHwFormat(SkStream* stream);
    bool IsHwAvailable();
    bool onDecodeOmx(SkStream* stream, SkBitmap* bm, Mode);
    bool onDecodeArm(SkStream* stream, SkBitmap* bm, Mode);

public:
    sem_t *semaphore;
    JPEGDEC_State iState;
    JPEGDEC_State iLastState;
    JPEG_HEADER_INFO JpegHeaderInfo;
    OMX_S32 inputFileSize;

};

class AutoTimeMillis {
public:
    AutoTimeMillis(const char label[]) : fLabel(label) {
        LOG_FUNCTION_NAME

        if (!fLabel) {
            fLabel = "";
        }
        fNow = SkTime::GetMSecs();
        this->width = 0;
        this->height = 0;

        LOG_FUNCTION_NAME_EXIT
    }

    ~AutoTimeMillis() {
        LOG_FUNCTION_NAME

        LIBSKIAHW_LOGDB("---- Input file Resolution :%dx%d",width,height);
        LIBSKIAHW_LOGDB("---- TI JPEG Decode Time (ms): %s %d\n", fLabel, SkTime::GetMSecs() - fNow);

        LOG_FUNCTION_NAME_EXIT
    }

    void setResolution(int width, int height){
        LOG_FUNCTION_NAME

        this->width=width;
        this->height=height;

        LOG_FUNCTION_NAME_EXIT
    }

private:
    const char* fLabel;
    SkMSec      fNow;
    int width;
    int height;
};

typedef struct OMX_CUSTOM_RESOLUTION
{
    OMX_U32 nWidth;
    OMX_U32 nHeight;

} OMX_CUSTOM_RESOLUTION;




OMX_ERRORTYPE OMX_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead);
OMX_ERRORTYPE OMX_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE OMX_EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_PTR pAppData,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData);


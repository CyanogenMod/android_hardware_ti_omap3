
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

#ifndef SKIAHW_ENCODER_H
#define SKIAHW_ENCODER_H

#include <string.h>
#include <time.h>
#include "SkBitmap.h"
#include "SkStream.h"
#include "SkImageEncoder.h"
#include "SkImageUtility.h"
#include <stdio.h>
#include <semaphore.h>
#include <utils/threads.h>

extern "C" {
    #include "OMX_Component.h"
    #include "OMX_IVCommon.h"
}

#define OPTIMIZE 1
#define WVGA_RESOLUTION (854*480)

class SkJPEGImageEncoder : public SkImageEncoder {
protected:
virtual bool onEncode(SkWStream* stream, const SkBitmap& bm, int quality);
};

class SkTIJPEGImageEncoder
{
public:
    enum JPEGENC_State
    {
        STATE_LOADED,
        STATE_IDLE,
        STATE_EXECUTING,
        STATE_FILL_BUFFER_DONE_CALLED,
        STATE_EMPTY_BUFFER_DONE_CALLED,
        STATE_ERROR,
        STATE_EXIT
    };

    typedef struct JpegEncoderParams
    {
        //nWidth;
        //nHeight;
        //nQuality;
        //nComment;
        //nThumbnailWidth;
        //nThumbnailHeight;
        //APP0
        //APP1
        //APP13
        //CustomQuantizationTable
        //CustomHuffmanTable
        //nCropWidth
        //nCropHeight
    }JpegEncoderParams;

    sem_t *semaphore;
    JPEGENC_State iState;
    JPEGENC_State iLastState;
    int jpegSize;

    SkTIJPEGImageEncoder();
    void _SkTIJPEGImageEncoder();
    ~SkTIJPEGImageEncoder();
    bool onEncode(SkImageEncoder* enc_impl, SkWStream* stream, const SkBitmap& bm, int quality);
    bool encodeImage(int outBuffSize, void *inputBuffer, int inBuffSize, int width, int height, int quality, SkBitmap::Config config);
    bool SetJpegEncodeParameters(JpegEncoderParams * jep) {memcpy(&jpegEncParams, jep, sizeof(JpegEncoderParams)); return true;}
    void Run();
    void PrintState();
    void FillBufferDone(OMX_U8* pBuffer, OMX_U32 size);
    void EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData);
    int GetLoad(){ return mLoad; }
    void IncDeleteAttempts() {mDeleteAttempts++;}
    void ResetDeleteAttempts() {mDeleteAttempts = 0;}
    int GetDeleteAttempts() {return mDeleteAttempts;}

private:

    OMX_HANDLETYPE pOMXHandle;
    OMX_BUFFERHEADERTYPE *pInBuffHead;
    OMX_BUFFERHEADERTYPE *pOutBuffHead;
    OMX_PARAM_PORTDEFINITIONTYPE InPortDef;
    OMX_PARAM_PORTDEFINITIONTYPE OutPortDef;
    JpegEncoderParams jpegEncParams;
    OMX_U8* pEncodedOutputBuffer;
    OMX_U32 nEncodedOutputFilledLen;
    android::Mutex gTIJpegEncMutex;
    int mLoad;
    int mDeleteAttempts;

     bool onEncodeSW(SkWStream* stream, const SkBitmap& bm, int quality);

};

OMX_ERRORTYPE OMX_JPEGE_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead);
OMX_ERRORTYPE OMX_JPEGE_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE OMX_JPEGE_EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_PTR pAppData,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData);
#endif

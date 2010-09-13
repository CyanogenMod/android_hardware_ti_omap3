
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

#ifndef SKIAHW_DECODER_H
#define SKIAHW_DECODER_H

#include "SkBitmap.h"
#include "SkStream.h"
#include "SkAllocator.h"
#include "SkImageDecoder.h"
#include "SkImageUtility.h"
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <utils/threads.h>

extern "C" {
	#include "OMX_Component.h"
	#include "OMX_IVCommon.h"
}

#define OPTIMIZE 1

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

#define JPEGDEC_PROG_MAXRESOLUTION 12032000       //(4000*3008)
#define JPEGDEC_BASELINE_MAXRESOLUTION 25044736   //(5776*4336)
#define PROG_WIDTH	4000
#define PROG_HEIGHT 3008
#define SEQ_WIDTH   5776
#define SEQ_HEIGHT  4336

class SkTIJPEGImageDecoder
{

public:

	enum JPEGDEC_State
	{
		STATE_LOADED, // 0
		STATE_IDLE, // 1
		STATE_EXECUTING, // 2
		STATE_FILL_BUFFER_DONE_CALLED, // 3
		STATE_EMPTY_BUFFER_DONE_CALLED, // 4
		STATE_ERROR, // 5
        STATE_INVALID, //6
		STATE_EXIT // 7
	};

	typedef struct JpegDecoderParams
	{
		// Quatization Table
		// Huffman Table
		// SectionDecode;
		// SubRegionDecode
        OMX_U32 nXOrg;         /* X origin*/
        OMX_U32 nYOrg;         /* Y origin*/
        OMX_U32 nXLength;      /* X length*/
        OMX_U32 nYLength;      /* Y length*/

	}JpegDecoderParams;

    sem_t *semaphore;
	JPEGDEC_State iState;
	JPEGDEC_State iLastState;

    SkTIJPEGImageDecoder();
    ~SkTIJPEGImageDecoder();
    bool onDecode(SkImageDecoder* dec_impl, SkStream* stream, SkBitmap* bm, SkBitmap::Config pref, SkImageDecoder::Mode);

    bool SetJpegDecodeParameters(void *jdp) {
         memcpy((void*)&jpegDecParams, jdp, sizeof(JpegDecoderParams));
         return true;
    }

    void Run();
    void PrintState();
    void FillBufferDone(OMX_U8* pBuffer, OMX_U32 nFilledLen);
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

	typedef struct JPEG_HEADER_INFO {
		int nWidth;
		int nHeight ;
		int nFormat;
		int nProgressive;
	} JPEG_HEADER_INFO;

        OMX_HANDLETYPE pOMXHandle;
        OMX_BUFFERHEADERTYPE *pInBuffHead;
        OMX_BUFFERHEADERTYPE *pOutBuffHead;
        OMX_PARAM_PORTDEFINITIONTYPE InPortDef;
        OMX_PARAM_PORTDEFINITIONTYPE OutPortDef;
        JpegDecoderParams jpegDecParams;
        SkStream* inStream;
        SkBitmap* bitmap;
        TIHeapAllocator allocator;
        android::Mutex       gTIJpegDecMutex;
    int mLoad;
    int mDeleteAttempts;
    int mProgressive;
    bool nSubRegDecode;
    bool inStateTransition;
	OMX_S16 GetYUVformat(OMX_U8 * Data);
	OMX_S16 Get16m(const void * Short);
	OMX_S32 ParseJpegHeader (SkStream* stream, JPEG_HEADER_INFO* JpegHeaderInfo);
	OMX_S32 ParseJpegHeader (OMX_U8* JpgBuffer, OMX_S32 lSize, JPEG_HEADER_INFO* JpgHdrInfo);
	OMX_U32 JpegHeader_GetMarkerInfo (OMX_U32 Marker, OMX_U8* MarkerData, JPEG_HEADER_INFO* JpgHdrInfo);
	OMX_S32 fill_data(OMX_U8* pBuf, SkStream* stream, OMX_S32 bufferSize);
	void FixFrameSize(JPEG_HEADER_INFO* JpegHeaderInfo);

};

OMX_ERRORTYPE OMX_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead);
OMX_ERRORTYPE OMX_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE OMX_EventHandler(OMX_HANDLETYPE hComponent,
											OMX_PTR pAppData,
											OMX_EVENTTYPE eEvent,
											OMX_U32 nData1,
											OMX_U32 nData2,
											OMX_PTR pEventData);
#endif

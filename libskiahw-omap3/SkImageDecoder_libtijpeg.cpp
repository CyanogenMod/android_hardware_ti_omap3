
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
/**
* @file SkImageDecoder_libtijpeg.cpp
*
* This file implements SkTIJPEGImageDecoder
*
*/

#include "SkImageDecoder_libtijpeg.h"

#define PRINTF // SkDebugf
//#define PRINTF printf
#define TIME_DECODE
#define JPEG_DECODER_DUMP_INPUT_AND_OUTPUT 0 // set directory persmissions for /temp as 777

#if JPEG_DECODER_DUMP_INPUT_AND_OUTPUT
    int dOutputCount = 0;
    int dInputCount = 0;
#endif

//////////////////////////////////////////////////////////////////////////

#include "SkTime.h"

class AutoTimeMillis {
public:
    AutoTimeMillis(const char label[]) : fLabel(label) {
        if (!fLabel) {
            fLabel = "";
        }
        fNow = SkTime::GetMSecs();
        width = 0;
        height = 0;
    }
    ~AutoTimeMillis() {
        SkDebugf("---- Input file Resolution :%dx%d",width,height);
        SkDebugf("---- TI JPEG Decode Time (ms): %s %d\n", fLabel, SkTime::GetMSecs() - fNow);
    }

    void setResolution(int width, int height){
        this->width=width;
        this->height=height;
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

typedef struct OMX_CUSTOM_IMAGE_DECODE_SUBREGION
{
    OMX_U32 nSize;
    OMX_VERSIONTYPE nVersion;
    OMX_U32 nXOrg;         /*Sectional decoding: X origin*/
    OMX_U32 nYOrg;         /*Sectional decoding: Y origin*/
    OMX_U32 nXLength;      /*Sectional decoding: X length*/
    OMX_U32 nYLength;      /*Sectional decoding: Y length*/
}OMX_CUSTOM_IMAGE_DECODE_SUBREGION;

void* ThreadDecoderWrapper(void* me)
{
    SkTIJPEGImageDecoder *decoder = static_cast<SkTIJPEGImageDecoder *>(me);

    decoder->Run();

    return NULL;
}


OMX_ERRORTYPE OMX_EventHandler(OMX_HANDLETYPE hComponent,
                                    OMX_PTR pAppData,
                                    OMX_EVENTTYPE eEvent,
                                    OMX_U32 nData1,
                                    OMX_U32 nData2,
                                    OMX_PTR pEventData)
{
    ((SkTIJPEGImageDecoder *)pAppData)->EventHandler(hComponent, eEvent, nData1, nData2, pEventData);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMX_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
    SkTIJPEGImageDecoder * ImgDec = (SkTIJPEGImageDecoder *)ptr;
    ImgDec->iLastState = ImgDec->iState;
    ImgDec->iState = SkTIJPEGImageDecoder::STATE_EMPTY_BUFFER_DONE_CALLED;
    sem_post(ImgDec->semaphore) ;
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMX_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead)
{
    //PRINTF("\nOMX_FillBufferDone:: nFilledLen = %ld \n", pBuffHead->nFilledLen);
    ((SkTIJPEGImageDecoder *)ptr)->FillBufferDone(pBuffHead->pBuffer, pBuffHead->nFilledLen);

    return OMX_ErrorNone;
}


SkTIJPEGImageDecoder::~SkTIJPEGImageDecoder()
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
#if OPTIMIZE
    if( ( iLastState || iState ) && ( NULL != pOMXHandle ) ){
        if(iState == STATE_EXIT)
        {
            iState = STATE_EMPTY_BUFFER_DONE_CALLED;
            eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
            if ( eError != OMX_ErrorNone ) {
                PRINTF("\nError from SendCommand-Idle(nStop) State function\n");
                iState = STATE_ERROR;
                PRINTF("sem_post at %d", __LINE__);
                sem_post(semaphore) ;
            }
            else {
                inStateTransition = true;
            }
            Run();
        }else{
            iState = STATE_EXIT;
            sem_post(semaphore);
        }
    }
#endif

    gTIJpegDecMutex.lock();
    sem_destroy(semaphore);
    if (semaphore != NULL) {
        free(semaphore) ;
        semaphore = NULL;
    }
    gTIJpegDecMutex.unlock();
}

SkTIJPEGImageDecoder::SkTIJPEGImageDecoder()
{
    mLoad = 0;
    mDeleteAttempts = 0;
    pInBuffHead = NULL;
    pOutBuffHead = NULL;
    pOMXHandle = NULL;
    mProgressive = 0;
    semaphore = NULL;
    nSubRegDecode = false;
    inStateTransition = false;

    //Initialize JpegDecoderParams structure
    memset((void*)&jpegDecParams, 0, sizeof(JpegDecoderParams));

    semaphore = (sem_t*)malloc(sizeof(sem_t)) ;
    sem_init(semaphore, 0x00, 0x00);

}


OMX_S16 SkTIJPEGImageDecoder::Get16m(const void * Short)
{
    return(((OMX_U8 *)Short)[0] << 8) | ((OMX_U8 *)Short)[1];
}

void SkTIJPEGImageDecoder::FixFrameSize(JPEG_HEADER_INFO* JpegHeaderInfo)
{

    OMX_U32 nWidth=JpegHeaderInfo->nWidth, nHeight=JpegHeaderInfo->nHeight;

    /*round up if nWidth is not multiple of 32*/
    ( (nWidth%32 ) !=0 ) ?  nWidth=32 * (  (  nWidth/32 ) + 1 )  : nWidth;
    //PRINTF("new nWidth %d \n", nWidth);

    /*round up if nHeight is not multiple of 16*/
    ( (nHeight%16) !=0 ) ?  nHeight=16 * (  (  nHeight/16 ) + 1 )  : nHeight;
    //PRINTF("new nHeight %d \n", nHeight);

    JpegHeaderInfo->nWidth = nWidth;
    JpegHeaderInfo->nHeight = nHeight;
}


OMX_S16 SkTIJPEGImageDecoder::GetYUVformat(OMX_U8 * Data)
{
    unsigned char Nf;
    OMX_U32 j;
    OMX_U32 temp_index;
    OMX_U32 temp;
    OMX_U32 image_format;
    short H[4],V[4];

    Nf = Data[7];

    for (j = 0; j < Nf; j++)
    {
       temp_index = j * 3 + 7 + 2;
        /*---------------------------------------------------------*/
       /* H[j]: upper 4 bits of a byte, horizontal sampling fator.                  */
       /* V[j]: lower 4 bits of a byte, vertical sampling factor.                    */
       /*---------------------------------------------------------*/
         H[j] = (0x0f & (Data[temp_index] >> 4));
         V[j] = (0x0f & Data[temp_index]);
    }

    /*------------------------------------------------------------------*/
    /* Set grayscale flag, namely if image is gray then set it to 1,    */
    /* else set it to 0.                                                */
    /*------------------------------------------------------------------*/
    image_format = -1;

    if (Nf == 1){
      image_format = OMX_COLOR_FormatL8;
    }


    if (Nf == 3)
    {
       temp = (V[0]*H[0])/(V[1]*H[1]) ;

      if (temp == 4 && H[0] == 2)
        image_format = OMX_COLOR_FormatYUV420Planar;

      if (temp == 4 && H[0] == 4)
        image_format = OMX_COLOR_FormatYUV411Planar;

      if (temp == 2)
        image_format = OMX_COLOR_FormatCbYCrY; /* YUV422 interleaved, little endian */

      if (temp == 1)
        image_format = OMX_COLOR_FormatYUV444Interleaved;
    }

    return (image_format);

}


OMX_S32 SkTIJPEGImageDecoder::ParseJpegHeader (SkStream* stream, JPEG_HEADER_INFO* JpgHdrInfo)
{
    OMX_U8 a = 0;
    OMX_S32 lSize = 0;
    lSize = stream->getLength();
    stream->rewind();
    JpgHdrInfo->nProgressive = 0; /*Default value is non progressive*/

    a = stream->readU8();
    if ( a != 0xff || stream->readU8() != M_SOI )  {
        return 0;
    }
    for ( ;; )  {
        OMX_U32 itemlen = 0;
        OMX_U32 marker = 0;
        OMX_U32 ll = 0,lh = 0, got = 0, err = 0;
        OMX_U8 *data = NULL;

        for ( a=0;a<15 /* 7 originally */;a++ ) {
            marker = stream->readU8();
            //PRINTF("MARKER IS %x\n",marker);
            if ( marker != 0xff )
                break;
        }
        if ( marker == 0xff )   {
            /* 0xff is legal padding, but if we get that many, something's wrong.*/
            PRINTF("too many padding bytes!");
            return 0;
        }

        /* Read the length of the section.*/
        lh = stream->readU8();
        ll = stream->readU8();

        itemlen = (lh << 8) | ll;

        if ( itemlen < 2 )  {
            PRINTF("invalid marker");
            return 0;
        }

        data = (OMX_U8 *)malloc(itemlen);
        if ( data == NULL ) {
            PRINTF("Could not allocate memory");
            break;
        }

        /* Store first two pre-read bytes. */
        data[0] = (OMX_U8)lh;
        data[1] = (OMX_U8)ll;

        got = stream->read(data+2, itemlen-2); /* Read the whole section.*/

        if ( got != itemlen-2 ) {
            PRINTF("Premature end of file?");
            if ( data != NULL ) {
                free(data);
                data=NULL;
            }
            return 0;
        }

        //PRINTF("Jpeg section marker 0x%02x size %d\n",marker, itemlen);
        err = JpegHeader_GetMarkerInfo(marker, data, JpgHdrInfo);

        if ( data != NULL ) {
            free(data);
            data=NULL;
        }

        if(err ==  M_SOS)
            return lSize;
        else if(err == M_EOI)
            return 0;
    }


    return 0;
}

OMX_S32 SkTIJPEGImageDecoder::ParseJpegHeader (OMX_U8* JpgBuffer, OMX_S32 lSize, JPEG_HEADER_INFO* JpgHdrInfo)
{
    OMX_U8 a = 0;
    OMX_U32 pos = 0;
    JpgHdrInfo->nProgressive = 0; /*Default value is non progressive*/

    a = JpgBuffer[pos++];
    if ( a != 0xff || JpgBuffer[pos++] != M_SOI )  {
        return 0;
    }
    for ( ;; )
    {
        OMX_U32 itemlen = 0;
        OMX_U32 marker = 0;
        OMX_U32 ll = 0,lh = 0, err = 0;
        OMX_U8 *data = NULL;

        for ( a=0;a<15 /* 7 originally */;a++ ) {
            marker = JpgBuffer[pos++];
            //PRINTF("MARKER IS %x\n",marker);
            if ( marker != 0xff )
                break;
        }
        if ( marker == 0xff )   {
            /* 0xff is legal padding, but if we get that many, something's wrong.*/
            PRINTF("too many padding bytes!");
            return 0;
        }

        /* Read the length of the section.*/
        data = &JpgBuffer[pos];
        lh = data[0];
        ll = data[1];

        itemlen = (lh << 8) | ll;

        if ( itemlen < 2 )  {
            PRINTF("invalid marker");
            return 0;
        }

        pos += itemlen; /* Move position by the whole section.*/

        //PRINTF("Jpeg section marker 0x%02x size %d\n",marker, itemlen);
        err = JpegHeader_GetMarkerInfo(marker, data, JpgHdrInfo);

        if(err ==  M_SOS)
            return lSize;
        else if(err == M_EOI)
            return 0;
    }
    return 0;
}
OMX_U32 SkTIJPEGImageDecoder::JpegHeader_GetMarkerInfo (OMX_U32 Marker, OMX_U8* MarkerData, JPEG_HEADER_INFO* JpgHdrInfo)
{
    switch ( Marker )
    {
        case M_SOS:
            return M_SOS;

        case M_EOI:
            PRINTF("No image in jpeg!\n");
            return M_EOI;

        case M_COM: /* Comment section  */
        case M_JFIF:
        case M_EXIF:
            break;

        case M_SOF2:
            PRINTF("nProgressive IMAGE!\n");
            JpgHdrInfo->nProgressive=1;
        case M_SOF0:
        case M_SOF1:
        case M_SOF3:
        case M_SOF5:
        case M_SOF6:
        case M_SOF7:
        case M_SOF9:
        case M_SOF10:
        case M_SOF11:
        case M_SOF13:
        case M_SOF14:
        case M_SOF15:

            JpgHdrInfo->nHeight = Get16m(MarkerData+3);
            JpgHdrInfo->nWidth = Get16m(MarkerData+5);
            JpgHdrInfo->nFormat = GetYUVformat(MarkerData);
            switch (JpgHdrInfo->nFormat) {
            case OMX_COLOR_FormatYUV420Planar:
                PRINTF("Image chroma format is OMX_COLOR_FormatYUV420Planar\n");
                break;
            case OMX_COLOR_FormatYUV411Planar:
                PRINTF("Image chroma format is OMX_COLOR_FormatYUV411Planar\n");
                break;
            case OMX_COLOR_FormatCbYCrY:
                PRINTF("Image chroma format is OMX_COLOR_FormatYUV422Interleaved\n");
                break;
            case OMX_COLOR_FormatYUV444Interleaved:
                 PRINTF("Image chroma format is OMX_COLOR_FormatYUV444Interleaved\n");
                 break;
            case OMX_COLOR_FormatL8:
                PRINTF("Image chroma format is OMX_COLOR_FormatL8 \n");
                break;
            default:
                 PRINTF("Cannot find Image chroma format \n");
                 JpgHdrInfo->nFormat = OMX_COLOR_FormatUnused;
                 break;
            }
            PRINTF("Image Width x Height = %u * %u\n", Get16m(MarkerData+5), Get16m(MarkerData+3)  );
            /*
            PRINTF("JPEG image is %uw * %uh,\n", Get16m(Data+3), Get16m(Data+5)  );

            if ( *(Data+9)==0x41 )  {
                PRINTF("THIS IS A YUV 411 ENCODED IMAGE \n");
                JpgHdrInfo->format= 1;
            }
            */
            break;
        default:
            /* Skip any other sections.*/
            break;
    }
    return 0;
}
void SkTIJPEGImageDecoder::FillBufferDone(OMX_U8* pBuffer, OMX_U32 nFilledLen)
{
    char strOutResolution[] = "OMX.TI.JPEG.decoder.Param.OutputResolution";
    OMX_INDEXTYPE nCustomIndex = OMX_IndexMax;
    OMX_CUSTOM_RESOLUTION OutputResolution;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    // algo might have changed our desired output width and height
    // check here and update bitmap accordingly
    eError = OMX_GetExtensionIndex(pOMXHandle, strOutResolution, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        PRINTF ("Error getting output resolution %d:error= %x\n", __LINE__, eError);
    } else {
        eError = OMX_GetConfig (pOMXHandle, nCustomIndex, &OutputResolution);
        if ( eError != OMX_ErrorNone ) {
            PRINTF ("Error getting output resolution %d:error= %x\n", __LINE__, eError);
        } else{
            PRINTF("output resolution: %dx%d", OutputResolution.nWidth, OutputResolution.nHeight);
            bitmap->setConfig(bitmap->config(), OutputResolution.nWidth, OutputResolution.nHeight);
        }
    }
    bitmap->setPixelRef(new TISkMallocPixelRef(pBuffer, nFilledLen, NULL))->unref();
    bitmap->lockPixels();

#if JPEG_DECODER_DUMP_INPUT_AND_OUTPUT

    char path[50];
    snprintf(path, sizeof(path), "/temp/JDO_%d_%d_%dx%d.raw", dOutputCount, bitmap->config(), bitmap->width(), bitmap->height());

    PRINTF("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");

    SkFILEWStream tempFile(path);
    if (tempFile.write(pBuffer, nFilledLen) == false)
        PRINTF("Writing to %s failed\n", path);
    else
        PRINTF("Writing to %s succeeded\n", path);

    dOutputCount++;
    PRINTF("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
#endif

    iLastState = iState;
    iState = STATE_FILL_BUFFER_DONE_CALLED;

}


void SkTIJPEGImageDecoder::EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData)
{

    PRINTF("\nEventHandler:: eEvent = %x, nData1 = %x, nData2 = %x\n\n", eEvent, (unsigned int)nData1, (unsigned int)nData2);

    switch ( eEvent ) {

        case OMX_EventCmdComplete:
            /* Do not move the common stmts in these conditionals outside. */
            /* We do not want to apply them in cases when these conditions are not met. */
            if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateIdle))
            {
                PRINTF ("Component State Changed To OMX_StateIdle\n");
                inStateTransition = false;
                iLastState = iState;
                iState = STATE_IDLE;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateExecuting))
            {
                PRINTF ("Component State Changed To OMX_StateExecuting\n");
                inStateTransition = false;
                iLastState = iState;
                iState = STATE_EXECUTING;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateLoaded))
            {
                PRINTF ("Component State Changed To OMX_StateLoaded\n");
                inStateTransition = false;
                iLastState = iState;
                iState = STATE_LOADED;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateInvalid))
            {
                PRINTF ("Component State Changed To OMX_StateInvalid\n");
                inStateTransition = false;
                iState = STATE_INVALID;
                sem_post(semaphore) ;

#if OPTIMIZE
                // Run() is not running under this condition. We won't clean anything without this.
                // We also don't want to block here because we could deadlock the system
                // OMX calls into this function, and Run() will call into OMX
                if (iLastState == STATE_EXIT){
                    pthread_t thread;
                    pthread_attr_t attr;
                    pthread_attr_init(&attr);
                    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                    pthread_create(&thread, &attr, ThreadDecoderWrapper, this);
                    pthread_attr_destroy(&attr);
                }
#endif
            }
            break;


        case OMX_EventError:
            PRINTF ("\n\n\nOMX Component  reported an Error!!!!\n\n\n");
            if(iState != STATE_ERROR)
            {
                iLastState = iState;
                iState = STATE_ERROR;
                if (iLastState == STATE_EXIT) {
                    OMX_SendCommand(hComponent, OMX_CommandStateSet, OMX_StateInvalid, NULL);
                    inStateTransition = true;
                    //call sem_post(semaphore) at OMX_StateInvalid state set event.
                }
                else {
                    sem_post(semaphore);
                }
            }else{
                PRINTF ("Libskiahw(decoder) already handling Error!!!");
            }
            break;

        default:
            break;
    }

}


OMX_S32 SkTIJPEGImageDecoder::fill_data(OMX_U8* pBuf, SkStream* stream, OMX_S32 bufferSize)
{
    OMX_U32 nFilledLen = stream->read(pBuf, bufferSize);
    PRINTF ("Populated Input Buffer: nAllocLen = %d, nFilledLen =%ld\n", bufferSize, nFilledLen);

#if JPEG_DECODER_DUMP_INPUT_AND_OUTPUT

    char path[50];
    snprintf(path, sizeof(path), "/temp/JDI_%d.jpg", dInputCount);

    PRINTF("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");

    SkFILEWStream tempFile(path);
    if (tempFile.write(pBuf, nFilledLen) == false)
        PRINTF("Writing to %s failed\n", path);
    else
        PRINTF("Writing to %s succeeded\n", path);

    dInputCount++;
    PRINTF("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
#endif

    return nFilledLen;
}

bool SkTIJPEGImageDecoder::onDecode(SkImageDecoder* dec_impl, SkStream* stream, SkBitmap* bm, SkBitmap::Config prefConfig, SkImageDecoder::Mode mode)
{

    gTIJpegDecMutex.lock();
    /* Critical section */
    PRINTF("Entering Critical Section \n");

    AutoTrackLoad autotrack(mLoad);
#ifdef TIME_DECODE
    AutoTimeMicros atm("TI JPEG Decode");
#endif

    int reuseHandle = 0;
    int nRetval;
    int nIndex1;
    int nIndex2;
    int scaleFactor;
    int bitsPerPixel;
    int nRead = 0;
    int nOutWidth, nInWidth;
    int nOutHeight, nInHeight;
    OMX_S32 inputFileSize;
    OMX_S32 nCompId = 100;
    OMX_U32 outBuffSize;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    JPEG_HEADER_INFO JpegHeaderInfo;
    OMX_PORT_PARAM_TYPE PortType;
    OMX_CUSTOM_RESOLUTION MaxResolution;
    OMX_CONFIG_SCALEFACTORTYPE ScaleFactor;
    OMX_CUSTOM_IMAGE_DECODE_SUBREGION SubRegionDecode;
    OMX_INDEXTYPE nCustomIndex = OMX_IndexMax;
    void* inputBuffer = NULL;
    void* outputBuffer = NULL;
    char strTIJpegDec[] = "OMX.TI.JPEG.decoder";
    char strColorFormat[] = "OMX.TI.JPEG.decoder.Config.OutputColorFormat";
    char strProgressive[] = "OMX.TI.JPEG.decoder.Config.ProgressiveFactor";
    char strMaxResolution[] = "OMX.TI.JPEG.decoder.Param.SetMaxResolution";
    char strSubRegionDecode[] = "OMX.TI.JPEG.decoder.Param.SubRegionDecode";

    OMX_CALLBACKTYPE JPEGCallBack ={OMX_EventHandler, OMX_EmptyBufferDone, OMX_FillBufferDone};

    PRINTF("\nUsing TI Image Decoder.\n");

    bitmap = bm;
    inStream = stream;
    SkBitmap::Config config = prefConfig;
    scaleFactor = dec_impl->getSampleSize();

    PRINTF("config = %d\n", config);
    PRINTF("mode = %d\n", mode);
    PRINTF("scaleFactor = %d ", scaleFactor);
    //PRINTF("File size = %d\n", stream->getLength());

    if (scaleFactor == 3)
        scaleFactor = 2;
    else if ((scaleFactor > 4) && (scaleFactor < 8))
        scaleFactor = 4;
    else if (scaleFactor > 8)
        scaleFactor = 8;

    PRINTF ("Modified scaleFactor = %d\n", scaleFactor);
    if (SkImageDecoder::kDecodeBounds_Mode == mode) {
        inputFileSize = ParseJpegHeader(stream , &JpegHeaderInfo);
    }else{
        inputFileSize = stream->getLength();
        inputFileSize = (OMX_U32)((inputFileSize + ALIGN_128_BYTE - 1) & ~(ALIGN_128_BYTE - 1));
        inputBuffer = memalign(ALIGN_128_BYTE, inputFileSize);
        if (inputBuffer == NULL) {
            PRINTF("%s():%d::ERROR!!!: Could not allocate memory for inputBuffer.\n",__FUNCTION__,__LINE__);
            goto EXIT;
        }

        stream->rewind();
        nRead = fill_data((OMX_U8*)inputBuffer, stream, stream->getLength());

        inputFileSize = ParseJpegHeader((OMX_U8*)inputBuffer, inputFileSize, &JpegHeaderInfo);
    }

#ifdef TIME_DECODE
    atm.setResolution(JpegHeaderInfo.nWidth , JpegHeaderInfo.nHeight);
    atm.setScaleFactor(scaleFactor);
#endif

    if (inputFileSize == 0) {
        PRINTF("The file size is 0. Maybe the format of the file is not correct\n");
        goto EXIT;
    }

    // if no user preference, see what the device recommends
    if (config == SkBitmap::kNo_Config)
        config = SkImageDecoder::GetDeviceConfig();

    //Configure the bitmap with the required W x H
    if (jpegDecParams.nXOrg || jpegDecParams.nYOrg ||
        jpegDecParams.nXLength || jpegDecParams.nYLength ) {

        PRINTF ("\njpegDecParams.nXOrg = %d\n",jpegDecParams.nXOrg);
        PRINTF ("jpegDecParams.nYOrg = %d\n",jpegDecParams.nYOrg);
        PRINTF ("jpegDecParams.nXLength = %d\n",jpegDecParams.nXLength);
        PRINTF ("jpegDecParams.nYLength = %d\n",jpegDecParams.nYLength);

        nOutWidth = jpegDecParams.nXLength;
        nOutHeight = jpegDecParams.nYLength;

        //set the subregion decode flag So that the current decode will create a new OMX handle.
        //This is required because the Subregion Decode parameters can't be changed in Execute
        //state of the OMX component.
        nSubRegDecode = true;
    }
    else {
        nOutWidth = JpegHeaderInfo.nWidth;
        nOutHeight = JpegHeaderInfo.nHeight;
    }

    switch(scaleFactor){
        case(8):
            //For scale factor 8, instead of 12.5% use 13%.
            nOutWidth = nOutWidth * 13 / 100;
            nOutHeight = nOutHeight * 13 / 100;
        break;
        default:
            nOutWidth = nOutWidth/scaleFactor;
            nOutHeight = nOutHeight/scaleFactor;
        break;
    }

    nInWidth = JpegHeaderInfo.nWidth;
    nInHeight = JpegHeaderInfo.nHeight;
    /*check for the maximum resolution and set to its upper limit which is supported by the codec.
      So that TI DSP Codec will take care of scaling down before decoding and updates the
      outputresolution parameter in UALGOutParams structure.
    */
    if(JpegHeaderInfo.nProgressive &&
        (nOutWidth * nOutHeight > JPEGDEC_PROG_MAXRESOLUTION)) //progressive
    {
        if(nOutWidth > nOutHeight)
        {
            nOutWidth = nInWidth = PROG_WIDTH;
            nOutHeight = nInHeight = PROG_HEIGHT;
        }else
        {
            nOutWidth = nInWidth = PROG_HEIGHT;
            nOutHeight = nInHeight = PROG_WIDTH;
        }
    }
    else if(!JpegHeaderInfo.nProgressive &&
        (nOutWidth * nOutHeight > JPEGDEC_BASELINE_MAXRESOLUTION)) //sequential image
    {
        if(nOutWidth > nOutHeight)
        {
            nOutWidth = nInWidth = SEQ_WIDTH;
            nOutHeight = nInHeight = SEQ_HEIGHT;
        }else
        {
            nOutWidth = nInWidth = SEQ_HEIGHT;
            nOutHeight = nInHeight = SEQ_WIDTH;
        }
    }
    bm->setConfig(config, nOutWidth, nOutHeight);

    if (bm->config() == SkBitmap::kARGB_8888_Config)
        bm->setIsOpaque(false);
    else
        bm->setIsOpaque(true);

    PRINTF("bm->width() = %d\n", bm->width());
    PRINTF("bm->height() = %d\n", bm->height());
    PRINTF("bm->config() = %d\n", bm->config());
    PRINTF("bm->getSize() = %d\n", bm->getSize());
    PRINTF("InPortDef.format.image.nFrameWidth = %d\n", InPortDef.format.image.nFrameWidth);
    PRINTF("InPortDef.format.image.nFrameHeight = %d\n", InPortDef.format.image.nFrameHeight);

    if (SkImageDecoder::kDecodeBounds_Mode == mode) {
        if(inputBuffer != NULL)
            free(inputBuffer);
        gTIJpegDecMutex.unlock();
        PRINTF("Jpeg Header Parsing Done.\n");
        PRINTF("Leaving Critical Section 1 \n");
        return true;
    }

    FixFrameSize(&JpegHeaderInfo);

    if (config == SkBitmap::kA8_Config) { /* 8-bits per pixel, with only alpha specified (0 is transparent, 0xFF is opaque) */
        bitsPerPixel = 1;
    }
    else if (config == SkBitmap::kRGB_565_Config) { /* 16-bits per pixel*/
        bitsPerPixel = 2;
    }
    else if (config == SkBitmap::kARGB_8888_Config) { /* 32-bits per pixel */
        bitsPerPixel = 4;
    }
    else { /*Set DEFAULT color format*/
        bitsPerPixel = 2;
    }

    outBuffSize = (nOutWidth * nOutHeight) * bitsPerPixel ;

#if OPTIMIZE
    if(pOMXHandle == NULL ||
            nSubRegDecode == true ||
            (OMX_U32) nInWidth > InPortDef.format.image.nFrameWidth ||
            (OMX_U32) nInHeight > InPortDef.format.image.nFrameHeight ||
            OutPortDef.format.image.eColorFormat != SkBitmapToOMXColorFormat(bm->config()) ||
            mProgressive != JpegHeaderInfo.nProgressive ||
            InPortDef.format.image.eColorFormat != JPEGToOMXColorFormat(JpegHeaderInfo.nFormat) ||
            outBuffSize > OutPortDef.nBufferSize ||
            (OMX_U32) inputFileSize > InPortDef.nBufferSize)
    {
        //reset the subregion decode flag for next decode to use the current OMX handle
        nSubRegDecode = false;
#endif
        if (pOMXHandle != NULL) {
            eError = TIOMX_FreeHandle(pOMXHandle);
            if ( (eError != OMX_ErrorNone) )    {
                PRINTF("Error in Free Handle function\n");
            }
        } else {
            eError = TIOMX_Init();
            if ( eError != OMX_ErrorNone ) {
                PRINTF("%d :: Error returned by OMX_Init()\n",__LINE__);
                goto EXIT;
            }
        }
        /* Load the JPEGDecoder Component */

        //PRINTF("Calling OMX_GetHandle\n");
        eError = TIOMX_GetHandle(&pOMXHandle, strTIJpegDec, (void *)this, &JPEGCallBack);
        if ( (eError != OMX_ErrorNone) ||  (pOMXHandle == NULL) ) {
            PRINTF ("Error in Get Handle function\n");
            iState = STATE_ERROR;
            goto EXIT;
        }

        // reset semaphore if we previously used it
        if(iState == STATE_EXIT){
            sem_destroy(semaphore);
            sem_init(semaphore, 0x00, 0x00);
        }

        iLastState = STATE_LOADED;
        iState = STATE_LOADED;

        PortType.nSize = sizeof(OMX_PORT_PARAM_TYPE);
        PortType.nVersion.s.nVersionMajor = 0x1;
        PortType.nVersion.s.nVersionMinor = 0x0;
        PortType.nVersion.s.nRevision = 0x0;
        PortType.nVersion.s.nStep = 0x0;

        eError = OMX_GetParameter(pOMXHandle, OMX_IndexParamImageInit, &PortType);
        if ( eError != OMX_ErrorNone ) {
            iState = STATE_ERROR;
            goto EXIT;
        }

        nIndex1 = PortType.nStartPortNumber;
        nIndex2 = nIndex1 + 1;

        /**********************************************************************/
        /* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (INPUT) */
        /**********************************************************************/

        InPortDef.nPortIndex = PortType.nStartPortNumber;
        InPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
        InPortDef.nVersion.s.nVersionMajor = 0x1;
        InPortDef.nVersion.s.nVersionMinor = 0x0;
        InPortDef.nVersion.s.nRevision = 0x0;
        InPortDef.nVersion.s.nStep = 0x0;

        eError = OMX_GetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &InPortDef);
        if ( eError != OMX_ErrorNone ) {
            eError = OMX_ErrorBadParameter;
            iState = STATE_ERROR;
            goto EXIT;
        }

        InPortDef.eDir = OMX_DirInput;
        InPortDef.nBufferCountActual =1;
        InPortDef.nBufferCountMin = 1;
        InPortDef.bEnabled = OMX_TRUE;
        InPortDef.bPopulated = OMX_FALSE;
        InPortDef.eDomain = OMX_PortDomainImage;
        InPortDef.format.image.pNativeRender = NULL;
        InPortDef.format.image.nFrameWidth = nInWidth;
        InPortDef.format.image.nFrameHeight = nInHeight;
        InPortDef.format.image.nStride = -1;
        InPortDef.format.image.nSliceHeight = -1;
        InPortDef.format.image.bFlagErrorConcealment = OMX_FALSE;
        InPortDef.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
        InPortDef.nBufferSize = inputFileSize;

        if (InPortDef.eDir == nIndex1 ) {
            InPortDef.nPortIndex = nIndex1;
        }
        else {
            InPortDef.nPortIndex = nIndex2;
        }

        InPortDef.format.image.eColorFormat = JPEGToOMXColorFormat(JpegHeaderInfo.nFormat);

        //PRINTF("Calling OMX_SetParameter for Input Port\n");
        eError = OMX_SetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &InPortDef);
        if ( eError != OMX_ErrorNone ) {
            eError = OMX_ErrorBadParameter;
            iState = STATE_ERROR;
            goto EXIT;
        }

        /***********************************************************************/
        /* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (OUTPUT) */
        /***********************************************************************/

        OutPortDef.nPortIndex = PortType.nStartPortNumber;
        OutPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
        OutPortDef.nVersion.s.nVersionMajor = 0x1;
        OutPortDef.nVersion.s.nVersionMinor = 0x0;
        OutPortDef.nVersion.s.nRevision = 0x0;
        OutPortDef.nVersion.s.nStep = 0x0;

        eError = OMX_GetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &OutPortDef);
        if ( eError != OMX_ErrorNone ) {
            eError = OMX_ErrorBadParameter;
            iState = STATE_ERROR;
            goto EXIT;
        }

        OutPortDef.eDir = OMX_DirOutput;
        OutPortDef.nBufferCountActual = 1;
        OutPortDef.nBufferCountMin = 1;
        OutPortDef.bEnabled = OMX_TRUE;
        OutPortDef.bPopulated = OMX_FALSE;
        OutPortDef.eDomain = OMX_PortDomainImage;
        OutPortDef.format.image.pNativeRender = NULL;
        OutPortDef.format.image.nFrameWidth = JpegHeaderInfo.nWidth;
        OutPortDef.format.image.nFrameHeight = JpegHeaderInfo.nHeight;
        OutPortDef.format.image.nStride = -1;
        OutPortDef.format.image.nSliceHeight = -1;
        OutPortDef.format.image.bFlagErrorConcealment = OMX_FALSE;
        OutPortDef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
        OutPortDef.format.image.eColorFormat = SkBitmapToOMXColorFormat(config);

        if (OutPortDef.eDir == nIndex1 ) {
            OutPortDef.nPortIndex = nIndex1;
        }
        else {
            OutPortDef.nPortIndex = nIndex2;
        }

        OutPortDef.nBufferSize = outBuffSize;

        //PRINTF("Output buffer size is %ld\n", OutPortDef.nBufferSize);

        eError = OMX_SetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &OutPortDef);
        if ( eError != OMX_ErrorNone ) {
            eError = OMX_ErrorBadParameter;
            iState = STATE_ERROR;
            goto EXIT;
        }
        /* Set max width & height value*/
        eError = OMX_GetExtensionIndex(pOMXHandle, strMaxResolution, (OMX_INDEXTYPE*)&nCustomIndex);
        if ( eError != OMX_ErrorNone ) {
            PRINTF ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        iState = STATE_ERROR;
            goto EXIT;
        }

        MaxResolution.nWidth = JpegHeaderInfo.nWidth;
        MaxResolution.nHeight = JpegHeaderInfo.nHeight;

        eError = OMX_SetParameter (pOMXHandle, nCustomIndex, &MaxResolution);
        if ( eError != OMX_ErrorNone ) {
            PRINTF ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        iState = STATE_ERROR;
            goto EXIT;
        }

        if (jpegDecParams.nXOrg || jpegDecParams.nYOrg ||
            jpegDecParams.nXLength || jpegDecParams.nYLength ) {

            eError = OMX_GetExtensionIndex(pOMXHandle, strSubRegionDecode, (OMX_INDEXTYPE*)&nCustomIndex);
            if ( eError != OMX_ErrorNone ) {
                PRINTF ("JPEGDec:: %d:Error received from OMX_GetExtensionIndex(): error= %x\n", __LINE__, eError);
                iState = STATE_ERROR;
                goto EXIT;
            }

            SubRegionDecode.nSize = sizeof(OMX_CUSTOM_IMAGE_DECODE_SUBREGION);
            eError = OMX_GetParameter (pOMXHandle, nCustomIndex, &SubRegionDecode);
            if ( eError != OMX_ErrorNone ) {
                PRINTF ("JPEGDec::%d:Error received from OMX_GetParameter(): error= %x\n", __LINE__, eError);
                iState = STATE_ERROR;
                goto EXIT;
            }
            SubRegionDecode.nXOrg = jpegDecParams.nXOrg;
            SubRegionDecode.nYOrg = jpegDecParams.nYOrg;
            SubRegionDecode.nXLength = jpegDecParams.nXLength;
            SubRegionDecode.nYLength = jpegDecParams.nYLength;

            eError = OMX_SetParameter (pOMXHandle, nCustomIndex, &SubRegionDecode);
            if ( eError != OMX_ErrorNone ) {
                PRINTF ("JPEGDec::%d:Error received from OMX_SetParameter(): error= %x\n", __LINE__, eError);
                iState = STATE_ERROR;
                goto EXIT;
            }
            //set the subregion decode flag So that next decode will create a new OMX handle. This is required
            //because the Subregion Decode parameters can't be changed in Execute state of the OMX component.
            nSubRegDecode = true;
            PRINTF ("JPEGDec::%d:SubRegionDecode is set.\n", __LINE__ );
        }

        outputBuffer = tisk_malloc_flags(outBuffSize, 0);  // returns NULL on failure
        if (NULL == outputBuffer) {
            PRINTF("xxxxxxxxxxxxxxxxxxxx tisk_malloc_flags failed\n");
            iState = STATE_ERROR;
            goto EXIT;
        }

        eError = OMX_UseBuffer(pOMXHandle, &pInBuffHead,  InPortDef.nPortIndex,  (void *)&nCompId, InPortDef.nBufferSize, (OMX_U8*)inputBuffer);
        if ( eError != OMX_ErrorNone ) {
            PRINTF ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        iState = STATE_ERROR;
            goto EXIT;
        }
        // assign nFilledLen to actual amount read during fill_data
        pInBuffHead->nFilledLen = nRead;

        eError = OMX_UseBuffer(pOMXHandle, &pOutBuffHead,  OutPortDef.nPortIndex,  (void *)&nCompId, OutPortDef.nBufferSize, (OMX_U8*)outputBuffer);
        if ( eError != OMX_ErrorNone ) {
            PRINTF ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        iState = STATE_ERROR;
            goto EXIT;
        }

#if OPTIMIZE
    } else {
        reuseHandle = 1;
    }
#endif

    if (scaleFactor == 2){
        ScaleFactor.xWidth = 50;
        ScaleFactor.xHeight = 50;
    }
    else if (scaleFactor == 4){
        ScaleFactor.xWidth = 25;
        ScaleFactor.xHeight = 25;
    }
    else if (scaleFactor == 8){
        ScaleFactor.xWidth = 13;
        ScaleFactor.xHeight = 13;
    }
    else{
        ScaleFactor.xWidth = 100;
        ScaleFactor.xHeight = 100;
    }

    eError = OMX_SetConfig (pOMXHandle, OMX_IndexConfigCommonScale, &ScaleFactor);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        iState = STATE_ERROR;
        goto EXIT;
    }

    mProgressive = JpegHeaderInfo.nProgressive;
    eError = OMX_GetExtensionIndex(pOMXHandle, strProgressive, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        PRINTF ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }
    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &(JpegHeaderInfo.nProgressive));
    if ( eError != OMX_ErrorNone ) {
        PRINTF ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }

    eError = OMX_GetExtensionIndex(pOMXHandle, strColorFormat, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        PRINTF ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }

    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &(OutPortDef.format.image.eColorFormat));
    if ( eError != OMX_ErrorNone ) {
        PRINTF ("JPEGDec test:: %d:error= %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }

    if(!reuseHandle)
    {
        eError = OMX_SendCommand(pOMXHandle, OMX_CommandStateSet, OMX_StateIdle ,NULL);
        if ( eError != OMX_ErrorNone ) {
            PRINTF ("Error from SendCommand-Idle(Init) State function\n");
        iState = STATE_ERROR;
            goto EXIT;
        }
        else{
            inStateTransition = true;
        }
    }
#if OPTIMIZE
    else
    {
        PRINTF("REUSING HANDLE current state %d", iState);

        outputBuffer = tisk_malloc_flags(outBuffSize, 0);  // returns NULL on failure
        if (NULL == outputBuffer) {
            PRINTF("xxxxxxxxxxxxxxxxxxxx tisk_malloc_flags failed\n");
            iState = STATE_ERROR;
            goto EXIT;
        }
        pOutBuffHead->pBuffer = (OMX_U8*)outputBuffer;
        pOutBuffHead->nAllocLen = outBuffSize;

        // assign nFilledLen to actual amount read during fill_data
        pInBuffHead->nFilledLen = nRead;
        pInBuffHead->pBuffer = (OMX_U8*)inputBuffer;

        iState = STATE_EXECUTING;
        {
            int sem_val;
            if(sem_getvalue(semaphore, &sem_val) == 0)
                PRINTF("line %d: sem value = %d", __LINE__, sem_val);
        }
        PRINTF("sem_post at %d", __LINE__);
        sem_post(semaphore);
    }
#endif

    Run();
    if(inputBuffer != NULL)
        free(inputBuffer);
    gTIJpegDecMutex.unlock();
    PRINTF("Leaving Critical Section 2 \n");
    return true;

EXIT:
    if (iState == STATE_ERROR) {
        sem_post(semaphore);
        Run();
    }
    if(inputBuffer != NULL)
        free(inputBuffer);

    tisk_free(outputBuffer);

    gTIJpegDecMutex.unlock();
    PRINTF("Leaving Critical Section 3 \n");
    return false;
}

void SkTIJPEGImageDecoder::PrintState()
{
    //PRINTF("\niLastState = %x\n", iLastState);
    switch(iState)
    {
        case STATE_LOADED:
            PRINTF("Current State is STATE_LOADED.\n");
            break;

        case STATE_IDLE:
            PRINTF("Current State is STATE_IDLE.\n");
            break;

        case STATE_EXECUTING:
            PRINTF("Current State is STATE_EXECUTING.\n");
            break;

        case STATE_EMPTY_BUFFER_DONE_CALLED:
            PRINTF("Current State is STATE_EMPTY_BUFFER_DONE_CALLED.\n");
            break;

        case STATE_FILL_BUFFER_DONE_CALLED:
            PRINTF("Current State is STATE_FILL_BUFFER_DONE_CALLED.\n");
            break;

        case STATE_ERROR:
            PRINTF("Current State is STATE_ERROR.\n");
            break;

        case STATE_EXIT:
            PRINTF("Current State is STATE_EXIT.\n");
            break;

        default:
            PRINTF("Current State is Invalid.\n");
            break;

    }
}

void SkTIJPEGImageDecoder::Run()
{
    int sem_retval;
    int num_timeout = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    struct timespec timeout;
    clock_gettime( CLOCK_REALTIME, &timeout );
    timeout.tv_sec += 15; // set intial timeout to be arbitrarily high
    timeout.tv_nsec += 0;

while(1){
    while ((sem_retval = sem_timedwait(semaphore, &timeout)) == -1 && errno == EINTR)
        continue;       // sem_wait interrupted try again

    PrintState();

    if(sem_retval == -1 && errno == ETIMEDOUT) {
        if(inStateTransition) {
            SkDebugf("\n%s():%d::Semaphore timedout. Decoder waiting for State Transition complete event.",__FUNCTION__,__LINE__);
            clock_gettime( CLOCK_REALTIME, &timeout );
            timeout.tv_sec += 15;
            num_timeout++;
            if(num_timeout >= 2) {    //exit if it timedout two times
                iState = STATE_INVALID;
            }
            else {
                continue;                 //Continue on sem wait untill the OMX-JPEG state get changed
            }

        }
        else{
            //timed out in a definite state like execute state, exit gracefully
            SkDebugf("\n%s():%d::Decoder semwait timedout...",__FUNCTION__,__LINE__);
            iState = STATE_ERROR;
        }
    }
    else if(sem_retval == -1){
        //semaphore unknown error
        iState = STATE_ERROR;
        SkDebugf("\n%s():%d::Decoder waiting for semaphore unknown error...",__FUNCTION__,__LINE__);
    }

    switch(iState)
    {
        case STATE_IDLE:
            if (iLastState == STATE_LOADED)
            {
                eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateExecuting, NULL);
                if ( eError != OMX_ErrorNone ) {
                    PRINTF("eError from SendCommand-Executing State function\n");
                    iState = STATE_ERROR;
                    break;
                }
                else{
                    inStateTransition = true;
                }
            }
            else if ((iLastState == STATE_EMPTY_BUFFER_DONE_CALLED) || (iLastState==STATE_FILL_BUFFER_DONE_CALLED))
            {
                    eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("Error from SendCommand-Idle State function\n");
                        iState = STATE_ERROR;
                        break;
                    }
                    else{
                        inStateTransition = true;
                    }

                    eError = OMX_SendCommand(pOMXHandle, OMX_CommandPortDisable, 0x0, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("Error from SendCommand-PortDisable function. Input port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    eError = OMX_SendCommand(pOMXHandle, OMX_CommandPortDisable, 0x1, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("Error from SendCommand-PortDisable function. Output port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    /* Free buffers */
                    eError = OMX_FreeBuffer(pOMXHandle, InPortDef.nPortIndex, pInBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("Error from OMX_FreeBuffer. Input port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    eError = OMX_FreeBuffer(pOMXHandle, OutPortDef.nPortIndex, pOutBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("Error from OMX_FreeBuffer. Output port.\n");
                        iState = STATE_ERROR;
                        break;
                    }
            }
            else
            {
                iState = STATE_ERROR;
            }
            clock_gettime( CLOCK_REALTIME, &timeout );
            timeout.tv_sec += 15;
            break;

        case STATE_EXECUTING:
            pInBuffHead->nFlags = OMX_BUFFERFLAG_EOS;
            OMX_EmptyThisBuffer(pOMXHandle, pInBuffHead);
            OMX_FillThisBuffer(pOMXHandle, pOutBuffHead);
            clock_gettime( CLOCK_REALTIME, &timeout );
            timeout.tv_sec += 7;
            break;

        case STATE_EMPTY_BUFFER_DONE_CALLED:
#if OPTIMIZE
            iState = STATE_EXIT;
#else
            eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
            if ( eError != OMX_ErrorNone ) {
                PRINTF("Error from SendCommand-Idle(nStop) State function\n");
                iState = STATE_ERROR;
            }
            else{
                inStateTransition = true;
            }
#endif
            clock_gettime( CLOCK_REALTIME, &timeout );
            timeout.tv_sec += 15;
            break;

        case STATE_ERROR:
            eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateInvalid, NULL);
            if ( eError != OMX_ErrorNone ) {
                PRINTF("Error from SendCommand-Invalid State function\n");
                iState = STATE_ERROR;
            }
            else{
                inStateTransition = true;
            }
            break;

        case STATE_LOADED:
        case STATE_INVALID:
            /*### Assume that the inStream will be closed by some upper layer */
            /*### Assume that the Bitmap object/file need not be closed. */
            /*### More needs to be done here */
            /*### Do different things based on iLastState */

            if ( iState == STATE_INVALID ) {
                if (pInBuffHead != NULL) {
                    /* Free buffers if it got allocated */
                    eError = OMX_FreeBuffer(pOMXHandle, InPortDef.nPortIndex, pInBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("Error from OMX_FreeBuffer. Input port.\n");
                    }
                }
                if (pOutBuffHead != NULL) {
                    eError = OMX_FreeBuffer(pOMXHandle, OutPortDef.nPortIndex, pOutBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("Error from OMX_FreeBuffer. Output port.\n");
                    }
                }
            }

            if (pOMXHandle) {
                eError = TIOMX_FreeHandle(pOMXHandle);
                if ( (eError != OMX_ErrorNone) )    {
                    PRINTF("Error in Free Handle function\n");
                }
                pOMXHandle = NULL;
            }

            eError = TIOMX_Deinit();
            if ( eError != OMX_ErrorNone ) {
                PRINTF("Error returned by TIOMX_Deinit()\n");
            }

            iState = STATE_EXIT;
            sem_post(semaphore);

            break;

        default:
            break;
    }

    if (iState == STATE_EXIT) break;
    }
}


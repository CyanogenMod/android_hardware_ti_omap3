
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
* @file SkImageEncoder_libtijpeg.cpp
*
* This file implements SkTIJPEGImageEncoder
*
*/

#include "SkImageEncoder_libtijpeg.h"
#include "SkTime.h"

#define PRINTF // SkDebugf
//#define PRINTF printf
#define TIME_ENCODE
#define EVEN_RES_ONLY 1   //true if odd resolution encode support enabled using SW codec
#define MULTIPLE 2 //image width must be a multiple of this number

#define JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT 0

#if JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT
	int eOutputCount = 0;
	int eInputCount = 0;
#endif

void* ThreadEncoderWrapper(void* me)
{
    SkTIJPEGImageEncoder *encoder = static_cast<SkTIJPEGImageEncoder *>(me);

    encoder->Run();

    return NULL;
}

OMX_ERRORTYPE OMX_JPEGE_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead)
{
    //PRINTF("OMX_FillBufferDone: pBuffHead = %p, pBuffer = %p, n    FilledLen = %ld \n", pBuffHead, pBuffHead->pBuffer, pBuffHead->nFilledLen);
    ((SkTIJPEGImageEncoder *)ptr)->FillBufferDone(pBuffHead->pBuffer,  pBuffHead->nFilledLen);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMX_JPEGE_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
    SkTIJPEGImageEncoder * ImgEnc = (SkTIJPEGImageEncoder *)ptr;
    ImgEnc->iLastState = ImgEnc->iState;
    ImgEnc->iState = SkTIJPEGImageEncoder::STATE_EMPTY_BUFFER_DONE_CALLED;
    sem_post(ImgEnc->semaphore) ;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMX_JPEGE_EventHandler(OMX_HANDLETYPE hComponent,
                                    OMX_PTR pAppData,
                                    OMX_EVENTTYPE eEvent,
                                    OMX_U32 nData1,
                                    OMX_U32 nData2,
                                    OMX_PTR pEventData)
{
    ((SkTIJPEGImageEncoder *)pAppData)->EventHandler(hComponent, eEvent, nData1, nData2, pEventData);
    return OMX_ErrorNone;
}


SkTIJPEGImageEncoder::SkTIJPEGImageEncoder()
{
    mLoad = 0;
    mDeleteAttempts = 0;
    pInBuffHead = NULL;
    pOutBuffHead = NULL;
    pOMXHandle = NULL;
    pEncodedOutputBuffer = NULL;
    semaphore = NULL;
    semaphore = (sem_t*)malloc(sizeof(sem_t)) ;
    sem_init(semaphore, 0x00, 0x00);

}

SkTIJPEGImageEncoder::~SkTIJPEGImageEncoder()
{
#if OPTIMIZE
    OMX_ERRORTYPE eError = OMX_ErrorNone;
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
            Run();
        }else{
            iState = STATE_EXIT;
            sem_post(semaphore);
        }
    }
#endif

    gTIJpegEncMutex.lock();
    if(pEncodedOutputBuffer != NULL)
    {
        free(pEncodedOutputBuffer);
        pEncodedOutputBuffer = NULL;
    }
    sem_destroy(semaphore);
    if (semaphore != NULL) {
        free(semaphore) ;
        semaphore = NULL;
    }
    gTIJpegEncMutex.unlock();

}

void SkTIJPEGImageEncoder::FillBufferDone(OMX_U8* pBuffer, OMX_U32 size)
{

#if JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT

    char path[50];
    snprintf(path, sizeof(path), "/temp/JEO_%d.jpg", eOutputCount);

    PRINTF("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");

    SkFILEWStream tempFile(path);
    if (tempFile.write(pBuffer, size) == false)
        PRINTF("Writing to %s failed\n", path);
    else
        PRINTF("Writing to %s succeeded\n", path);

    eOutputCount++;
    PRINTF("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
#endif

    iLastState = iState;
    iState = STATE_FILL_BUFFER_DONE_CALLED;
    jpegSize = size;

    pEncodedOutputBuffer = pBuffer;
    nEncodedOutputFilledLen = size;
    //sem_post(semaphore) ;
}


void SkTIJPEGImageEncoder::EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData)
{

    //PRINTF("\nEventHandler:: eEvent = %x, nData1 = %x, nData2 = %x\n\n", eEvent, (unsigned int)nData1, (unsigned int)nData2);

    switch ( eEvent ) {

        case OMX_EventCmdComplete:
            /* Do not move the common stmts in these conditionals outside. */
            /* We do not want to apply them in cases when these conditions are not met. */
            if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateIdle))
            {
                //PRINTF ("Component State Changed To OMX_StateIdle\n");
                iLastState = iState;
                iState = STATE_IDLE;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateExecuting))
            {
                //PRINTF ("Component State Changed To OMX_StateExecuting\n");
                iLastState = iState;
                iState = STATE_EXECUTING;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateLoaded))
            {
                //PRINTF ("Component State Changed To OMX_StateLoaded\n");
                iLastState = iState;
                iState = STATE_LOADED;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateInvalid))
            {
                //PRINTF ("Component State Changed To OMX_StateInvalid\n");
                iState = STATE_ERROR;
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
                    pthread_create(&thread, &attr, ThreadEncoderWrapper, this);
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
                OMX_SendCommand(hComponent, OMX_CommandStateSet, OMX_StateInvalid, NULL);
                // call sem_post(semaphore) at OMX_StateInvalid state set event.
            }else{
                PRINTF ("Libskiahw(encoder) already handling Error!!!");
            }
            break;

        default:
            break;
    }

}


bool SkTIJPEGImageEncoder::onEncodeSW(SkWStream* stream, const SkBitmap& bm, int quality) {

    bool result = false;
    SkJPEGImageEncoder* pSWJPGEHandle = new SkJPEGImageEncoder;
    if (!pSWJPGEHandle) {
        PRINTF("\nERROR:: !!Could not get the SW codec handle!!\n");
        return false;
    }

    SkDebugf("\n#### Using SW(ARM) Image Encoder ####\n\n");
    result = pSWJPGEHandle->encodeStream(stream, bm, quality);
    delete pSWJPGEHandle;
    return result;
}

bool SkTIJPEGImageEncoder::onEncode(SkImageEncoder* enc_impl, SkWStream* stream, const SkBitmap& bm, int quality)
{

    int w, h, nWidthNew, nHeightNew, nMultFactor, nBytesPerPixel;
    OMX_U32 inBuffSize;

    w = bm.width();
    h = bm.height();

    PRINTF("\n******* WxH = %d x %d ******\n",w,h);
    /*decide to choose the codec based on resolution W*H */
    //If W or H is ODD or W*H < WVGA resolution then invoke SW codec (ARM)
    if ( (w % 2) || (h % 2) || (w * h <= WVGA_RESOLUTION) ) {

        return onEncodeSW (stream, bm, quality);
    }

    PRINTF("\nUsing TI Image Encoder.\n");
    AutoTrackLoad autotrack(mLoad); //increase load here, should be fine even though it's done outside of critical section as long as we keep it 1 to 1 in this function

    SkAutoLockPixels alp(bm);
    if (NULL == bm.getPixels()) {
        return false;
    }

#ifdef TIME_ENCODE
    AutoTimeMicros atm("TI JPEG Encode");
    atm.setResolution(w , h);
#endif

    nMultFactor = (w + 16 - 1)/16;
    nWidthNew = (int)(nMultFactor) * 16;

    nMultFactor = (h + 16 - 1)/16;
    nHeightNew = (int)(nMultFactor) * 16;

    if (bm.config() == SkBitmap::kRGB_565_Config)
        nBytesPerPixel = 2;
    else if (bm.config() == SkBitmap::kARGB_8888_Config)
        nBytesPerPixel = 4;
    else{
        PRINTF("This format is not supported!");
        return false;
    }

    inBuffSize = nWidthNew * nHeightNew * nBytesPerPixel;
    if (inBuffSize < 1600)
        inBuffSize = 1600;

    inBuffSize = (OMX_U32)((inBuffSize + ALIGN_128_BYTE - 1) & ~(ALIGN_128_BYTE - 1));
    void *inputBuffer = memalign(ALIGN_128_BYTE, inBuffSize);
    if ( inputBuffer == NULL) {
        PRINTF("\n %s():%d:: ERROR:: inputBuffer Allocation Failed. \n", __FUNCTION__,__LINE__);
        return false;
    }

#if EVEN_RES_ONLY
    memcpy(inputBuffer, bm.getPixels(), bm.getSize());
#else
    //This padding logic can be used for the odd sized image encoding
    //when TI DSP codec used
    int pad_width = w%MULTIPLE;
    int pad_height = h%2;
    int pixels_to_pad = 0;
    if (pad_width)
        pixels_to_pad = MULTIPLE - pad_width;
    int bytes_to_pad = pixels_to_pad * nBytesPerPixel;
    printf("\npixels_to_pad  = %d\n", pixels_to_pad );
    int src = (int)inputBuffer;
    int dst = (int)bm.getPixels();
    int i, j;
    int row = w * nBytesPerPixel;

    if (pad_height && pad_width)
    {
        printf("\ndealing with pad_width");
        for(i = 0; i < h; i++)
        {
            memcpy((void *)src, (void *)dst, row);
            dst = dst + row;
            src = src + row;
            for (j = 0; j < bytes_to_pad; j++){
                *((char *)src) = 0;
                src++;
            }
        }

        printf("\ndealing with odd height");
        memset((void *)src, 0, row+bytes_to_pad);

        w+=pixels_to_pad;
        h++;
    }
    else if (pad_height)
    {
        memcpy((void *)src, (void*)dst, (w*h*nBytesPerPixel));
        printf("\nadding one line and making it zero");
        memset((void *)(src + (w*h*nBytesPerPixel)), 0, row);
        h++;
    }
    else if (pad_width)
    {
        printf("\ndealing with odd width");

        for(i = 0; i < h; i++)
        {
            memcpy((void *)src, (void *)dst, row);
            dst = dst + row;
            src = src + row;
            for (j = 0; j < bytes_to_pad; j++){
                *((char *)src) = 0;
                src++;
            }
        }
        w+=pixels_to_pad;
    }
    else
    {
        printf("\nno padding");
        memcpy((void *)src, (void*)dst, (w*h*nBytesPerPixel));
    }

#endif
    OMX_U32 outBuffSize = bm.width() * bm.height();
    if( quality < 10){
        outBuffSize /=10;
    }
    else if (quality <100){
        outBuffSize /= (100/quality);
    }
    /*Adding memory to include Thumbnail, comments & markers information and header (depends on the app)*/
    outBuffSize += 12288;
    outBuffSize = (OMX_U32)((outBuffSize + ALIGN_128_BYTE - 1) & ~(ALIGN_128_BYTE - 1));

    PRINTF("\nOriginal: w = %d, h = %d", bm.width(), bm.height());
    if (encodeImage(outBuffSize,inputBuffer, inBuffSize, w, h, quality, bm.config())){
        stream->write(pEncodedOutputBuffer, nEncodedOutputFilledLen);
        free(inputBuffer);
        return true;
    }

    free(inputBuffer);
    return false;
}


bool SkTIJPEGImageEncoder::encodeImage(int outBuffSize, void *inputBuffer, int inBuffSize, int width, int height, int quality, SkBitmap::Config config)
{

    gTIJpegEncMutex.lock();
    /* Critical section */
    PRINTF("Entering Critical Section \n");

    int reuseHandle = 0;
    int nRetval;
    int nIndex1;
    int nIndex2;
    int bitsPerPixel;
    int nMultFactor = 0;
    int nHeightNew, nWidthNew;
    char strTIJpegEnc[] = "OMX.TI.JPEG.encoder";
    char strQFactor[] = "OMX.TI.JPEG.encoder.Config.QFactor";
    char strInputWidth[] = "OMX.TI.JPEG.encoder.Config.InputFrameWidth";
    char strInputHeight[] = "OMX.TI.JPEG.encoder.Config.InputFrameHeight";

    /*Minimum buffer size requirement */
    void *outputBuffer = NULL;
    OMX_S32 nCompId = 300;
    OMX_PORT_PARAM_TYPE PortType;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_PARAM_QFACTORTYPE QfactorType;
    OMX_INDEXTYPE nCustomIndex = OMX_IndexMax;
    OMX_CALLBACKTYPE JPEGCallBack ={OMX_JPEGE_EventHandler, OMX_JPEGE_EmptyBufferDone, OMX_JPEGE_FillBufferDone};

    PRINTF("width = %d", width);
    PRINTF("height = %d", height);
    PRINTF("config = %d", config);
    PRINTF("quality = %d", quality);
    //PRINTF("inBuffSize = %d", inBuffSize);
    //PRINTF("outBuffSize = %d", outBuffSize);


#if JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT

    char path[50];
    snprintf(path, sizeof(path), "/temp/JEI_%d_%d_%dx%d.raw", eInputCount, config, width, height);

    PRINTF("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");

    SkFILEWStream tempFile(path);
    if (tempFile.write(inputBuffer, inBuffSize) == false)
        PRINTF("Writing to %s failed\n", path);
    else
        PRINTF("Writing to %s succeeded\n", path);

    eInputCount++;
    PRINTF("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
#endif

#if OPTIMIZE
    if(pOMXHandle == NULL ||
            (OMX_U32) width > InPortDef.format.image.nFrameWidth ||
            (OMX_U32) height > InPortDef.format.image.nFrameHeight ||
            InPortDef.format.image.eColorFormat != SkBitmapToOMXColorFormat(config) ||
            (OMX_U32) outBuffSize > OutPortDef.nBufferSize)
    {
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

        /* Load the JPEGEncoder Component */

        //PRINTF("Calling OMX_GetHandle\n");
        eError = TIOMX_GetHandle(&pOMXHandle, strTIJpegEnc, (void *)this, &JPEGCallBack);
        if ( (eError != OMX_ErrorNone) ||  (pOMXHandle == NULL) ) {
            PRINTF ("Error in Get Handle function\n");
            iState = STATE_ERROR;
            goto EXIT;
        }

        if(pEncodedOutputBuffer != NULL)
        {
            free(pEncodedOutputBuffer);
            pEncodedOutputBuffer = NULL;
        }
        outputBuffer = memalign(ALIGN_128_BYTE, outBuffSize);
        if ( outputBuffer == NULL) {
            PRINTF("\n %s():%d:: ERROR:: outputBuffer Allocation Failed. \n", __FUNCTION__,__LINE__);
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
        InPortDef.format.image.nFrameWidth = width;
        InPortDef.format.image.nFrameHeight = height;
        InPortDef.format.image.nStride = -1;
        InPortDef.format.image.nSliceHeight = -1;
        InPortDef.format.image.bFlagErrorConcealment = OMX_FALSE;
        InPortDef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
        InPortDef.nBufferSize = inBuffSize;
            InPortDef.format.image.eColorFormat = SkBitmapToOMXColorFormat(config);

        if (InPortDef.eDir == nIndex1 ) {
            InPortDef.nPortIndex = nIndex1;
        }
        else {
            InPortDef.nPortIndex = nIndex2;
        }

        //PRINTF("Calling OMX_SetParameter\n");

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
        OutPortDef.format.image.nFrameWidth = width;
        OutPortDef.format.image.nFrameHeight = height;
        OutPortDef.format.image.nStride = -1;
        OutPortDef.format.image.nSliceHeight = -1;
        OutPortDef.format.image.bFlagErrorConcealment = OMX_FALSE;
        OutPortDef.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
        OutPortDef.format.image.eColorFormat = OMX_COLOR_FormatCbYCrY;
        OutPortDef.nBufferSize = outBuffSize;

        if (OutPortDef.eDir == nIndex1 ) {
            OutPortDef.nPortIndex = nIndex1;
        }
        else {
            OutPortDef.nPortIndex = nIndex2;
        }

        eError = OMX_SetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &OutPortDef);
        if ( eError != OMX_ErrorNone ) {
            eError = OMX_ErrorBadParameter;
            iState = STATE_ERROR;
            goto EXIT;
        }

        eError = OMX_UseBuffer(pOMXHandle, &pInBuffHead,  InPortDef.nPortIndex,  (void *)&nCompId, InPortDef.nBufferSize, (OMX_U8*)inputBuffer);
        if ( eError != OMX_ErrorNone ) {
            PRINTF ("JPEGEnc test:: %d:error= %x\n", __LINE__, eError);
            iState = STATE_ERROR;
            goto EXIT;
        }

        eError = OMX_UseBuffer(pOMXHandle, &pOutBuffHead,  OutPortDef.nPortIndex,  (void *)&nCompId, OutPortDef.nBufferSize, (OMX_U8*)outputBuffer);
        if ( eError != OMX_ErrorNone ) {
            PRINTF ("JPEGEnc test:: %d:error= %x\n", __LINE__, eError);
            iState = STATE_ERROR;
            goto EXIT;
        }
#if OPTIMIZE
    } else {
        reuseHandle = 1;
    }
#endif
    QfactorType.nSize = sizeof(OMX_IMAGE_PARAM_QFACTORTYPE);
    QfactorType.nQFactor = (OMX_U32) quality;
    QfactorType.nVersion.s.nVersionMajor = 0x1;
    QfactorType.nVersion.s.nVersionMinor = 0x0;
    QfactorType.nVersion.s.nRevision = 0x0;
    QfactorType.nVersion.s.nStep = 0x0;
    QfactorType.nPortIndex = 0x0;

    eError = OMX_GetExtensionIndex(pOMXHandle, strQFactor, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }

    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &QfactorType);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }

    eError = OMX_GetExtensionIndex(pOMXHandle, strInputWidth, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }

    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &width);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }

    eError = OMX_GetExtensionIndex(pOMXHandle, strInputHeight, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }

    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &height);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
        iState = STATE_ERROR;
        goto EXIT;
    }
    pInBuffHead->nFilledLen = pInBuffHead->nAllocLen;
    pInBuffHead->nFlags = OMX_BUFFERFLAG_EOS;

    if(!reuseHandle)
    {
    eError = OMX_SendCommand(pOMXHandle, OMX_CommandStateSet, OMX_StateIdle ,NULL);
    if ( eError != OMX_ErrorNone ) {
        PRINTF ("Error from SendCommand-Idle(Init) State function\n");
        iState = STATE_ERROR;
        goto EXIT;
        }
    }
#if OPTIMIZE
    else
    {
        PRINTF("REUSING HANDLE current state %d", iState);

        pInBuffHead->pBuffer = (OMX_U8*)inputBuffer;
        pInBuffHead->nFilledLen  = inBuffSize;
        pInBuffHead->nAllocLen  = inBuffSize;

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

    gTIJpegEncMutex.unlock();
    PRINTF("Leaving Critical Section 2 \n");

    return true;

EXIT:
    if(outputBuffer != NULL)
        free(outputBuffer);

    if (iState == STATE_ERROR) {
        sem_post(semaphore);
        Run();
    }

    gTIJpegEncMutex.unlock();
    PRINTF("Leaving Critical Section 3 \n");

    return false;

}



void SkTIJPEGImageEncoder::PrintState()
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

void SkTIJPEGImageEncoder::Run()
{
    int nRead;
    int sem_retval;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    struct timespec timeout;
    clock_gettime( CLOCK_REALTIME, &timeout );
    timeout.tv_sec += 15; // set intial timeout to be arbitrarily high
    timeout.tv_nsec += 0;

    while(1){
        while ((sem_retval = sem_timedwait(semaphore, &timeout)) == -1 && errno == EINTR)
            continue;       // sem_wait interrupted try again
        if(sem_retval == -1)
        {
            iState = STATE_ERROR;
            if(errno == ETIMEDOUT)
                SkDebugf("Encoder waiting for semaphore timedout...");
            else
                SkDebugf("Encoder waiting for semaphore unknown error...");
        }

        PrintState();

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
                }
                else if (iLastState == STATE_EMPTY_BUFFER_DONE_CALLED || iLastState == STATE_FILL_BUFFER_DONE_CALLED)
                {
                        eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
                        if ( eError != OMX_ErrorNone ) {
                            PRINTF("Error from SendCommand-Idle State function\n");
                            iState = STATE_ERROR;
                            break;
                        }
                        /*
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
                        */
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
#endif
                clock_gettime( CLOCK_REALTIME, &timeout );
                timeout.tv_sec += 15;
                break;

            case STATE_LOADED:
            case STATE_ERROR:
                /*### Assume that the Bitmap object/file need not be closed. */
                /*### More needs to be done here */
                /*### Do different things based on iLastState */

                if ( iState == STATE_ERROR ) {
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



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
#include <string.h>

#define JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT 0

#if JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT
	int eOutputCount = 0;
	int eInputCount = 0;
#endif

OMX_ERRORTYPE OMX_JPEGE_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead)
{
    SkDebugf("OMX_FillBufferDone: pBuffHead = %p, pBuffer = %p, n    FilledLen = %ld \n", pBuffHead, pBuffHead->pBuffer, pBuffHead->nFilledLen);
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
    pInBuffHead = NULL;
    pOutBuffHead = NULL;
    semaphore = NULL;
    semaphore = (sem_t*)malloc(sizeof(sem_t)) ;
    sem_init(semaphore, 0x00, 0x00);

}

SkTIJPEGImageEncoder::~SkTIJPEGImageEncoder()
{
    free(semaphore) ;
    semaphore=NULL;

}

void SkTIJPEGImageEncoder::FillBufferDone(OMX_U8* pBuffer, OMX_U32 size)
{

#if JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT
    
    char path[50];
    snprintf(path, sizeof(path), "/temp/JEO_%d.jpg", eOutputCount);

    SkDebugf("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
    
    SkFILEWStream tempFile(path);
    if (tempFile.write(pBuffer, size) == false)
        SkDebugf("Writing to %s failed\n", path);
    else
        SkDebugf("Writing to %s succeeded\n", path);

    eOutputCount++;
    SkDebugf("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
#endif

    iLastState = iState;
    iState = STATE_FILL_BUFFER_DONE_CALLED;

    pEncodedOutputBuffer = pBuffer;
    nEncodedOutputFilledLen = size;
    sem_post(semaphore) ;
}


void SkTIJPEGImageEncoder::EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData)
{

    //SkDebugf("\nEventHandler:: eEvent = %x, nData1 = %x, nData2 = %x\n\n", eEvent, (unsigned int)nData1, (unsigned int)nData2);

    switch ( eEvent ) {

        case OMX_EventCmdComplete:
            /* Do not move the common stmts in these conditionals outside. */
            /* We do not want to apply them in cases when these conditions are not met. */
            if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateIdle))
            {
                //SkDebugf ("Component State Changed To OMX_StateIdle\n");
                iLastState = iState;
                iState = STATE_IDLE;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateExecuting))
            {
                //SkDebugf ("Component State Changed To OMX_StateExecuting\n");
                iLastState = iState;
                iState = STATE_EXECUTING;
                sem_post(semaphore) ;
            }
            else if ((nData1 == OMX_CommandStateSet) && (nData2 == OMX_StateLoaded))
            {
                //SkDebugf ("Component State Changed To OMX_StateLoaded\n");
                iLastState = iState;
                iState = STATE_LOADED;
                sem_post(semaphore) ;
            }
            break;

        case OMX_EventError:
            SkDebugf ("\n\n\nOMX Component  reported an Error!!!!\n\n\n");
            iLastState = iState;
            iState = STATE_ERROR;
            OMX_SendCommand(hComponent, OMX_CommandStateSet, OMX_StateInvalid, NULL);
            sem_post(semaphore) ;
            break;

        default:
            break;
    }

}



bool SkTIJPEGImageEncoder::onEncode(SkWStream* stream, const SkBitmap& bm, int quality)
{

    SkDebugf("\nUsing TI Image Encoder.\n");

    SkAutoLockPixels alp(bm);
    if (NULL == bm.getPixels()) {
        return false;
    }

    void* inputBuffer = malloc(bm.getSize() + 256);
    inputBuffer = (void*)((OMX_U32)inputBuffer + 128);
    memcpy(inputBuffer, bm.getPixels(), bm.getSize());

    /*Minimum buffer size requirement */
    OMX_U32 outBuffSize = bm.width() * bm.height();
    if( quality < 10){
        outBuffSize /=10;
    }
    else if (quality <100){
        outBuffSize /= (100/quality);
    }

    /*Adding memory to include Thumbnail, comments & markers information and header (depends on the app)*/
    outBuffSize += 12288;

    void* outputBuffer = malloc(outBuffSize + 256);
    outputBuffer = (void*)((OMX_U32)outputBuffer + 128);

    if (encodeImage(outputBuffer, outBuffSize, inputBuffer, bm.getSize(), bm.width(), bm.height(), quality, bm.config())){
        stream->write(pEncodedOutputBuffer, nEncodedOutputFilledLen);
        return true;        
    }            

    return false;
}


bool SkTIJPEGImageEncoder::encodeImage(void* outputBuffer, int outBuffSize, void *inputBuffer, int inBuffSize, int width, int height, int quality, SkBitmap::Config config)
{

    int nRetval;
    int nIndex1;
    int nIndex2;
    int bitsPerPixel;
    int nMultFactor = 0;
    int nHeightNew, nWidthNew;
    char strTIJpegEnc[] = "OMX.TI.JPEG.Encoder";
    char strQFactor[] = "OMX.TI.JPEG.encoder.Config.QFactor";

    OMX_S32 nCompId = 300;
    OMX_PORT_PARAM_TYPE PortType;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_PARAM_QFACTORTYPE QfactorType;
    OMX_INDEXTYPE nCustomIndex = OMX_IndexMax;
    OMX_CALLBACKTYPE JPEGCallBack ={OMX_JPEGE_EventHandler, OMX_JPEGE_EmptyBufferDone, OMX_JPEGE_FillBufferDone};

    SkDebugf("width = %d", width);
    SkDebugf("height = %d", height);
    SkDebugf("config = %d", config);
    SkDebugf("quality = %d", quality);			
    SkDebugf("inBuffSize = %d", inBuffSize);				
    SkDebugf("outBuffSize = %d", outBuffSize);			

#if JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT

    char path[50];
    snprintf(path, sizeof(path), "/temp/JEI_%d_%d_%dx%d.raw", eInputCount, config, width, height);

    SkDebugf("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
    
    SkFILEWStream tempFile(path);
    if (tempFile.write(inputBuffer, inBuffSize) == false)
        SkDebugf("Writing to %s failed\n", path);
    else
        SkDebugf("Writing to %s succeeded\n", path);

    eInputCount++;
    SkDebugf("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
#endif


    eError = TIOMX_Init();
    if ( eError != OMX_ErrorNone ) {
        SkDebugf("%d :: Error returned by OMX_Init()\n",__LINE__);
        goto EXIT;
    }

    /* Load the JPEGEncoder Component */

    SkDebugf("Calling OMX_GetHandle\n");
    eError = TIOMX_GetHandle(&pOMXHandle, strTIJpegEnc, (void *)this, &JPEGCallBack);
    if ( (eError != OMX_ErrorNone) ||  (pOMXHandle == NULL) ) {
        SkDebugf ("Error in Get Handle function\n");
        goto EXIT;
    }

    iLastState = STATE_LOADED;
    iState = STATE_LOADED;

    eError = OMX_GetParameter(pOMXHandle, OMX_IndexParamImageInit, &PortType);
    if ( eError != OMX_ErrorNone ) {
        goto EXIT;
    }

    nIndex1 = PortType.nStartPortNumber;
    nIndex2 = nIndex1 + 1;

    /**********************************************************************/
    /* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (INPUT) */
    /**********************************************************************/

    InPortDef.nPortIndex = PortType.nStartPortNumber;
    eError = OMX_GetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &InPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    InPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    InPortDef.nVersion.s.nVersionMajor = 0x1;
    InPortDef.nVersion.s.nVersionMinor = 0x0;
    InPortDef.nVersion.s.nRevision = 0x0;
    InPortDef.nVersion.s.nStep = 0x0;
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
    if (config == SkBitmap::kNo_Config)
        InPortDef.format.image.eColorFormat = OMX_COLOR_FormatCbYCrY;	
    else if (config == SkBitmap::kRGB_565_Config)
        InPortDef.format.image.eColorFormat = OMX_COLOR_Format16bitRGB565;	
    else if (config == SkBitmap::kARGB_8888_Config)
        InPortDef.format.image.eColorFormat = OMX_COLOR_Format32bitARGB8888;	


    if (InPortDef.eDir == nIndex1 ) {
        InPortDef.nPortIndex = nIndex1;
    }
    else {
        InPortDef.nPortIndex = nIndex2;
    }
    
    SkDebugf("Calling OMX_SetParameter\n");
    
    eError = OMX_SetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &InPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    /***********************************************************************/
    /* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (OUTPUT) */
    /***********************************************************************/

    OutPortDef.nPortIndex = PortType.nStartPortNumber;
    eError = OMX_GetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &OutPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    OutPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    OutPortDef.nVersion.s.nVersionMajor = 0x1;
    OutPortDef.nVersion.s.nVersionMinor = 0x0;
    OutPortDef.nVersion.s.nRevision = 0x0;
    OutPortDef.nVersion.s.nStep = 0x0;
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
        goto EXIT;
    }

    QfactorType.nSize = sizeof(OMX_IMAGE_PARAM_QFACTORTYPE);
    QfactorType.nQFactor = (OMX_U32) quality;
    QfactorType.nVersion.s.nVersionMajor = 0x1;
    QfactorType.nVersion.s.nVersionMinor = 0x0;
    QfactorType.nVersion.s.nRevision = 0x0;
    QfactorType.nVersion.s.nStep = 0x0;
    QfactorType.nPortIndex = 0x0;

    eError = OMX_GetExtensionIndex(pOMXHandle, strQFactor, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf("%d::APP_Error at function call: %x\n", __LINE__, eError);
        goto EXIT;
    }
    
    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &QfactorType);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf("%d::APP_Error at function call: %x\n", __LINE__, eError);
        goto EXIT;
    }

    eError = OMX_UseBuffer(pOMXHandle, &pInBuffHead,  InPortDef.nPortIndex,  (void *)&nCompId, InPortDef.nBufferSize, (OMX_U8*)inputBuffer);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGEnc test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    eError = OMX_UseBuffer(pOMXHandle, &pOutBuffHead,  OutPortDef.nPortIndex,  (void *)&nCompId, OutPortDef.nBufferSize, (OMX_U8*)outputBuffer);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("JPEGEnc test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    pInBuffHead->nFilledLen = pInBuffHead->nAllocLen;
    pInBuffHead->nFlags = OMX_BUFFERFLAG_EOS;

    eError = OMX_SendCommand(pOMXHandle, OMX_CommandStateSet, OMX_StateIdle ,NULL);
    if ( eError != OMX_ErrorNone ) {
        SkDebugf ("Error from SendCommand-Idle(Init) State function\n");
        goto EXIT;
    }

    Run();

    return true;

EXIT:

    return false;

}



void SkTIJPEGImageEncoder::PrintState()
{
    //SkDebugf("\niLastState = %x\n", iLastState);
    switch(iState)
    {
        case STATE_LOADED:
            SkDebugf("\nCurrent State is STATE_LOADED.\n");
            break;

        case STATE_IDLE:
            SkDebugf("\nCurrent State is STATE_IDLE.\n");
            break;

        case STATE_EXECUTING:
            SkDebugf("\nCurrent State is STATE_EXECUTING.\n");
            break;

        case STATE_EMPTY_BUFFER_DONE_CALLED:
            SkDebugf("\nCurrent State is STATE_EMPTY_BUFFER_DONE_CALLED.\n");
            break;

        case STATE_FILL_BUFFER_DONE_CALLED:
            SkDebugf("\nCurrent State is STATE_FILL_BUFFER_DONE_CALLED.\n");
            break;

        case STATE_ERROR:
            SkDebugf("\nCurrent State is STATE_ERROR.\n");
            break;

        case STATE_EXIT:
            SkDebugf("\nCurrent State is STATE_EXIT.\n");
            break;

        default:
            SkDebugf("\nCurrent State is Invalid.\n");
            break;

    }
}

void SkTIJPEGImageEncoder::Run()
{
    int nRead;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

while(1){
    if (sem_wait(semaphore))
    {
        SkDebugf("sem_wait returned the error: %d", errno);
        continue;
    }

    PrintState();

    switch(iState)
    {
        case STATE_IDLE:
            if (iLastState == STATE_LOADED)
            {
                eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateExecuting, NULL);
                if ( eError != OMX_ErrorNone ) {
                    SkDebugf("eError from SendCommand-Executing State function\n");
                    iState = STATE_ERROR;
                    break;
                }
            }
            else if (iLastState == STATE_EMPTY_BUFFER_DONE_CALLED)
            {
                    eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("Error from SendCommand-Idle State function\n");
                        iState = STATE_ERROR;
                        break;
                    }
                    /*
                    eError = OMX_SendCommand(pOMXHandle, OMX_CommandPortDisable, 0x0, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("Error from SendCommand-PortDisable function. Input port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    eError = OMX_SendCommand(pOMXHandle, OMX_CommandPortDisable, 0x1, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("Error from SendCommand-PortDisable function. Output port.\n");
                        iState = STATE_ERROR;
                        break;
                    }
                    */
                    /* Free buffers */
                    eError = OMX_FreeBuffer(pOMXHandle, InPortDef.nPortIndex, pInBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("Error from OMX_FreeBuffer. Input port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    eError = OMX_FreeBuffer(pOMXHandle, OutPortDef.nPortIndex, pOutBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        SkDebugf("Error from OMX_FreeBuffer. Output port.\n");
                        iState = STATE_ERROR;
                        break;
                    }
            }
            else
            {
                iState = STATE_ERROR;
            }
            break;

        case STATE_EXECUTING:

            OMX_EmptyThisBuffer(pOMXHandle, pInBuffHead);
            OMX_FillThisBuffer(pOMXHandle, pOutBuffHead);
            break;

        case STATE_EMPTY_BUFFER_DONE_CALLED:

            eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
            if ( eError != OMX_ErrorNone ) {
                SkDebugf("Error from SendCommand-Idle(nStop) State function\n");
                iState = STATE_ERROR;
            }
            break;

        case STATE_LOADED:
        case STATE_ERROR:
            /*### Assume that the Bitmap object/file need not be closed. */
            /*### More needs to be done here */
            /*### Do different things based on iLastState */

            if (pOMXHandle) {
                eError = TIOMX_FreeHandle(pOMXHandle);
                if ( (eError != OMX_ErrorNone) )    {
                    SkDebugf("Error in Free Handle function\n");
                }
            }

            eError = TIOMX_Deinit();
            if ( eError != OMX_ErrorNone ) {
                SkDebugf("Error returned by TIOMX_Deinit()\n");
            }

            iState = STATE_EXIT;

            break;

        default:
            break;
    }

    if (iState == STATE_ERROR) sem_post(semaphore) ;
    if (iState == STATE_EXIT) break;
    }

}


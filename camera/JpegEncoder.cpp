
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
* This file implements JpegEncoder
*
*/

#include "JpegEncoder.h"
#include <utils/Log.h>
#include <OMX_JpegEnc_CustomCmd.h>

#define PRINTF LOGD

#define JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT 0
#define OPTIMIZE 0

#if JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT
	int eOutputCount = 0;
	int eInputCount = 0;
#endif

OMX_ERRORTYPE OMX_JPEGE_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead)
{
    PRINTF("\nOMX_FillBufferDone: pBuffHead = %p, pBuffer = %p, n    FilledLen = %ld \n", pBuffHead, pBuffHead->pBuffer, pBuffHead->nFilledLen);
    ((JpegEncoder *)ptr)->FillBufferDone(pBuffHead->pBuffer,  pBuffHead->nFilledLen);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMX_JPEGE_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
    JpegEncoder * ImgEnc = (JpegEncoder *)ptr;
    ImgEnc->iLastState = ImgEnc->iState;
    ImgEnc->iState = JpegEncoder::STATE_EMPTY_BUFFER_DONE_CALLED;
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
    ((JpegEncoder *)pAppData)->EventHandler(hComponent, eEvent, nData1, nData2, pEventData);
    return OMX_ErrorNone;
}

JpegEncoder::JpegEncoder()
{
    thumb_width = 0;
    thumb_height = 0;
    mexif_buf = NULL;
    pInBuffHead = NULL;
    pOutBuffHead = NULL;
    semaphore = NULL;
    pOMXHandle = NULL;
    semaphore = (sem_t*)malloc(sizeof(sem_t)) ;
    sem_init(semaphore, 0x00, 0x00);
}

JpegEncoder::~JpegEncoder()
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
#if OPTIMIZE
    if( ( iLastState || iState ) && ( NULL != pOMXHandle ) ){
        eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
        if ( eError != OMX_ErrorNone ) {
            PRINTF("\nError from SendCommand-Idle(nStop) State function\n");
            iState = STATE_ERROR;
            sem_post(semaphore) ;
        }

        Run();
    }
#endif    
	sem_destroy(semaphore);
	free(semaphore) ;	
    semaphore=NULL;
}

void JpegEncoder::FillBufferDone(OMX_U8* pBuffer, OMX_U32 size)
{

#if JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT
    
    char path[50];
    snprintf(path, sizeof(path), "/temp/JEO_%d.jpg", eOutputCount);

    PRINTF("\nrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
    
    SkFILEWStream tempFile(path);
    if (tempFile.write(pBuffer, size) == false)
        PRINTF("\nWriting to %s failed\n", path);
    else
        PRINTF("\nWriting to %s succeeded\n", path);

    eOutputCount++;
    PRINTF("\nrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
#endif

    iLastState = iState;
    iState = STATE_FILL_BUFFER_DONE_CALLED;
    jpegSize = size;
   // sem_post(semaphore) ;
}


void JpegEncoder::EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData)
{

    //PRINTF("\n\nEventHandler:: eEvent = %x, nData1 = %x, nData2 = %x\n\n", eEvent, (unsigned int)nData1, (unsigned int)nData2);

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
            break;

        case OMX_EventError:
            //PRINTF ("\n\n\nOMX Component  reported an Error!!!!\n\n\n");
            iLastState = iState;
            iState = STATE_ERROR;
            OMX_SendCommand(hComponent, OMX_CommandStateSet, OMX_StateInvalid, NULL);
            sem_post(semaphore) ;
            break;

        default:
            break;
    }

}


bool JpegEncoder::StartFromLoadedState()
{

    int nRetval;
    int nIndex1;
    int nIndex2;
    int bitsPerPixel;
    int nMultFactor = 0;
    int nHeightNew, nWidthNew;
    char strTIJpegEnc[] = "OMX.TI.JPEG.encoder";
    char strQFactor[] = "OMX.TI.JPEG.encoder.Config.QFactor";
	char strColorFormat[] = "OMX.TI.JPEG.encoder.Config.ColorFormatConvertion_420pTo422i";
	char strPPLibEnable[] = "OMX.TI.JPEG.encoder.Config.PPLibEnable";

    char indexCustom[] = "OMX.TI.JPEG.encoder.Config.APP1";
    /** Exif */
    JPEG_APPTHUMB_MARKER jpeg_marker;
    OMX_BOOL bAPP1 = OMX_TRUE;

    OMX_S32 nCompId = 300;
    OMX_PORT_PARAM_TYPE PortType;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_PARAM_QFACTORTYPE QfactorType;
	OMX_BOOL b420pTo422i;
	OMX_BOOL bPPLibEnable;
    OMX_INDEXTYPE nCustomIndex = OMX_IndexMax;
    OMX_CALLBACKTYPE JPEGCallBack ={OMX_JPEGE_EventHandler, OMX_JPEGE_EmptyBufferDone, OMX_JPEGE_FillBufferDone};

    eError = TIOMX_Init();
    if ( eError != OMX_ErrorNone ) {
        PRINTF("\n%d :: Error returned by OMX_Init()\n",__LINE__);
        goto EXIT;
    }

    /* Load the JPEGEncoder Component */

    PRINTF("\nCalling OMX_GetHandle\n");
    eError = TIOMX_GetHandle(&pOMXHandle, strTIJpegEnc, (void *)this, &JPEGCallBack);
    if ( (eError != OMX_ErrorNone) ||  (pOMXHandle == NULL) ) {
        PRINTF ("Error in Get Handle function\n");
        goto EXIT;
    }

    PortType.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    PortType.nVersion.s.nVersionMajor = 0x1;
    PortType.nVersion.s.nVersionMinor = 0x0;
    PortType.nVersion.s.nRevision = 0x0;
    PortType.nVersion.s.nStep = 0x0;
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
    InPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    InPortDef.nVersion.s.nVersionMajor = 0x1;
    InPortDef.nVersion.s.nVersionMinor = 0x0;
    InPortDef.nVersion.s.nRevision = 0x0;
    InPortDef.nVersion.s.nStep = 0x0;
    eError = OMX_GetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &InPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    InPortDef.eDir = OMX_DirInput;
    InPortDef.nBufferCountActual =1;
    InPortDef.nBufferCountMin = 1;
    InPortDef.bEnabled = OMX_TRUE;
    InPortDef.bPopulated = OMX_FALSE;
    InPortDef.eDomain = OMX_PortDomainImage;
//	InPortDef.format.image.cMIMEType = "JPEGENC"
    InPortDef.format.image.pNativeRender = NULL;
    InPortDef.format.image.nFrameWidth = mWidth;
    InPortDef.format.image.nFrameHeight = mHeight;
    InPortDef.format.image.nStride = -1;
    InPortDef.format.image.nSliceHeight = -1;
    InPortDef.format.image.bFlagErrorConcealment = OMX_FALSE;
    InPortDef.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
    InPortDef.nBufferSize = mInBuffSize;
	if(mIsPixelFmt420p){
		InPortDef.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;	
	}
	else{
		InPortDef.format.image.eColorFormat = OMX_COLOR_FormatCbYCrY;
	}

    if (InPortDef.eDir == nIndex1 ) {
        InPortDef.nPortIndex = nIndex1;
    }
    else {
        InPortDef.nPortIndex = nIndex2;
    }

    PRINTF("\nCalling OMX_SetParameter\n");
    
    eError = OMX_SetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &InPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    /***********************************************************************/
    /* Set the component's OMX_PARAM_PORTDEFINITIONTYPE structure (OUTPUT) */
    /***********************************************************************/
	PortType.nStartPortNumber++;
    OutPortDef.nPortIndex = PortType.nStartPortNumber;
    OutPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    OutPortDef.nVersion.s.nVersionMajor = 0x1;
    OutPortDef.nVersion.s.nVersionMinor = 0x0;
    OutPortDef.nVersion.s.nRevision = 0x0;
    OutPortDef.nVersion.s.nStep = 0x0;
    eError = OMX_GetParameter (pOMXHandle, OMX_IndexParamPortDefinition, &OutPortDef);
    if ( eError != OMX_ErrorNone ) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    OutPortDef.eDir = OMX_DirOutput;
    OutPortDef.nBufferCountActual = 1;
    OutPortDef.nBufferCountMin = 1;
    OutPortDef.bEnabled = OMX_TRUE;
    OutPortDef.bPopulated = OMX_FALSE;
    OutPortDef.eDomain = OMX_PortDomainImage;
    OutPortDef.format.image.pNativeRender = NULL;
    OutPortDef.format.image.nFrameWidth = mWidth;
    OutPortDef.format.image.nFrameHeight = mHeight;
    OutPortDef.format.image.nStride = -1;
    OutPortDef.format.image.nSliceHeight = -1;
    OutPortDef.format.image.bFlagErrorConcealment = OMX_FALSE;
    OutPortDef.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
	if(mIsPixelFmt420p){
		OutPortDef.format.image.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;	
	}
	else{
		OutPortDef.format.image.eColorFormat = OMX_COLOR_FormatCbYCrY;
	}

    OutPortDef.nBufferSize = mOutBuffSize;
    
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
    QfactorType.nQFactor = (OMX_U32) mQuality;
    QfactorType.nVersion.s.nVersionMajor = 0x1;
    QfactorType.nVersion.s.nVersionMinor = 0x0;
    QfactorType.nVersion.s.nRevision = 0x0;
    QfactorType.nVersion.s.nStep = 0x0;
    QfactorType.nPortIndex = 0x0;

    eError = OMX_GetExtensionIndex(pOMXHandle, strQFactor, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("\n%d::APP_Error at function call: %x\n", __LINE__, eError);
        goto EXIT;
    }
    
    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &QfactorType);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("\n%d::APP_Error at function call: %x\n", __LINE__, eError);
        goto EXIT;
    }

	if(mIsPixelFmt420p){
		b420pTo422i = OMX_TRUE;

		eError = OMX_GetExtensionIndex(pOMXHandle, strColorFormat, (OMX_INDEXTYPE*)&nCustomIndex);
		if ( eError != OMX_ErrorNone ) {
		    PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
		    goto EXIT;
		}

		eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &b420pTo422i);
		if ( eError != OMX_ErrorNone ) {
		    PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
		    goto EXIT;
		}
	}

/*
    bPPLibEnable = OMX_FALSE;
    eError = OMX_GetExtensionIndex(pOMXHandle, strPPLibEnable, (OMX_INDEXTYPE*)&nCustomIndex);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
        goto EXIT;
    }

    eError = OMX_SetConfig (pOMXHandle, nCustomIndex, &bPPLibEnable);
    if ( eError != OMX_ErrorNone ) {
        PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
        goto EXIT;
    }
*/

    if( NULL != mexif_buf){
        jpeg_marker.nThumbnailWidth = thumb_width;
        jpeg_marker.nThumbnailHeight = thumb_height;
        jpeg_marker.bMarkerEnabled = bAPP1;
        jpeg_marker.nMarkerSize = mexif_buf->size + 4;
        jpeg_marker.pMarkerBuffer = (OMX_U8 *) calloc (1, jpeg_marker.nMarkerSize); /* FIXME */
        memcpy (jpeg_marker.pMarkerBuffer + 4, mexif_buf->data, mexif_buf->size);
        PRINTF("%d::OMX_GetExtensionIndex \n", __LINE__);
        eError = OMX_GetExtensionIndex(pOMXHandle, indexCustom, (OMX_INDEXTYPE*)&nCustomIndex);
        if ( eError != OMX_ErrorNone ) {
            PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
            goto EXIT;
        }
        PRINTF("%d::OMX_SetConfig \n", __LINE__);
        eError = OMX_SetConfig(pOMXHandle, nCustomIndex, &jpeg_marker);
        if ( eError != OMX_ErrorNone ) {
            PRINTF("%d::APP_Error at function call: %x\n", __LINE__, eError);
            goto EXIT;
        }
    	
        free(jpeg_marker.pMarkerBuffer);
    }

    eError = OMX_UseBuffer(pOMXHandle, &pInBuffHead,  InPortDef.nPortIndex,  (void *)&nCompId, InPortDef.nBufferSize, (OMX_U8*)mInputBuffer);
    if ( eError != OMX_ErrorNone ) {
        PRINTF ("JPEGEnc test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    eError = OMX_UseBuffer(pOMXHandle, &pOutBuffHead,  OutPortDef.nPortIndex,  (void *)&nCompId, OutPortDef.nBufferSize, (OMX_U8*)mOutputBuffer);
    if ( eError != OMX_ErrorNone ) {
        PRINTF ("JPEGEnc test:: %d:error= %x\n", __LINE__, eError);
        goto EXIT;
    }

    pInBuffHead->nFilledLen = pInBuffHead->nAllocLen;
    //pInBuffHead->nFlags = OMX_BUFFERFLAG_EOS;

    eError = OMX_SendCommand(pOMXHandle, OMX_CommandStateSet, OMX_StateIdle ,NULL);
    if ( eError != OMX_ErrorNone ) {
        PRINTF ("Error from SendCommand-Idle(Init) State function\n");
        goto EXIT;
    }

    Run();

    return true;

EXIT:

    return false;

}

bool JpegEncoder::encodeImage(void* outputBuffer, int outBuffSize, void *inputBuffer, int inBuffSize, int width, int height, int quality, exif_buffer *exif_buf, int isPixelFmt420p, int th_width, int th_height)
{

    bool ret = true;

    PRINTF("\niState = %d", iState);
    PRINTF("\nwidth = %d", width);
    PRINTF("\nheight = %d", height);
    PRINTF("\nquality = %d", quality);			
    PRINTF("\ninBuffSize = %d", inBuffSize);				
    PRINTF("\noutBuffSize = %d", outBuffSize);	
	PRINTF("\nisPixelFmt420p = %d", isPixelFmt420p);			

#if JPEG_ENCODER_DUMP_INPUT_AND_OUTPUT

    char path[50];
    snprintf(path, sizeof(path), "/temp/JEI_%d_%dx%d.raw", eInputCount, width, height);

    PRINTF("\nrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
    
    SkFILEWStream tempFile(path);
    if (tempFile.write(inputBuffer, inBuffSize) == false)
        PRINTF("\nWriting to %s failed\n", path);
    else
        PRINTF("\nWriting to %s succeeded\n", path);

    eInputCount++;
    PRINTF("\nrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
#endif
#if OPTIMIZE
    if (
        (mWidth == width) &&
        (mHeight == height) &&
        (mQuality == quality) &&
        (iState == STATE_EMPTY_BUFFER_DONE_CALLED)
    )
    {
        PRINTF("\nI am already in EXECUTE state and all input params are same as before.");

        iState = STATE_EXECUTING;
        pInBuffHead->pBuffer = (OMX_U8*)inputBuffer;
        pInBuffHead->nFilledLen  = inBuffSize;
        //pInBuffHead->nFlags = OMX_BUFFERFLAG_EOS;
        pOutBuffHead->pBuffer = (OMX_U8*)outputBuffer;
        sem_post(semaphore);
        Run();

    }
    else
#endif    
    {
        mOutputBuffer = outputBuffer;
        mOutBuffSize = outBuffSize;
        mInputBuffer = inputBuffer;
        mInBuffSize = inBuffSize;
        mWidth = width;
        mHeight = height;
        mQuality = quality;
		mIsPixelFmt420p = isPixelFmt420p;
		mexif_buf = exif_buf;
        iLastState = STATE_LOADED;
        iState = STATE_LOADED;
        thumb_width = th_width;
        thumb_height = th_height;
    
        ret = StartFromLoadedState();
        if (ret == false)
        {
            PRINTF("\nThe image cannot be encoded for some reason");
            return  false;
        }
    }

    return true;

EXIT:

    return false;

}



void JpegEncoder::PrintState()
{
    //PRINTF("\n\niLastState = %x\n", iLastState);
    switch(iState)
    {
        case STATE_LOADED:
            PRINTF("\n\nCurrent State is STATE_LOADED.\n");
            break;

        case STATE_IDLE:
            PRINTF("\n\nCurrent State is STATE_IDLE.\n");
            break;

        case STATE_EXECUTING:
            PRINTF("\n\nCurrent State is STATE_EXECUTING.\n");
            break;

        case STATE_EMPTY_BUFFER_DONE_CALLED:
            PRINTF("\n\nCurrent State is STATE_EMPTY_BUFFER_DONE_CALLED.\n");
            break;

        case STATE_FILL_BUFFER_DONE_CALLED:
            PRINTF("\n\nCurrent State is STATE_FILL_BUFFER_DONE_CALLED.\n");
            break;

        case STATE_ERROR:
            PRINTF("\n\nCurrent State is STATE_ERROR.\n");
            break;

        case STATE_EXIT:
            PRINTF("\n\nCurrent State is STATE_EXIT.\n");
            break;

        default:
            PRINTF("\n\nCurrent State is Invalid.\n");
            break;

    }
}

void JpegEncoder::Run()
{
    int nRead;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

while(1){
    if (sem_wait(semaphore))
    {
        PRINTF("\nsem_wait returned the error");
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
                    PRINTF("\neError from SendCommand-Executing State function\n");
                    iState = STATE_ERROR;
                    break;
                }
            }
            else if (iLastState == STATE_EMPTY_BUFFER_DONE_CALLED)
            {
                    eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("\nError from SendCommand-Idle State function\n");
                        iState = STATE_ERROR;
                        break;
                    }
                    /*
                    eError = OMX_SendCommand(pOMXHandle, OMX_CommandPortDisable, 0x0, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("\nError from SendCommand-PortDisable function. Input port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    eError = OMX_SendCommand(pOMXHandle, OMX_CommandPortDisable, 0x1, NULL);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("\nError from SendCommand-PortDisable function. Output port.\n");
                        iState = STATE_ERROR;
                        break;
                    }
                    */
                    /* Free buffers */
                    eError = OMX_FreeBuffer(pOMXHandle, InPortDef.nPortIndex, pInBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("\nError from OMX_FreeBuffer. Input port.\n");
                        iState = STATE_ERROR;
                        break;
                    }

                    eError = OMX_FreeBuffer(pOMXHandle, OutPortDef.nPortIndex, pOutBuffHead);
                    if ( eError != OMX_ErrorNone ) {
                        PRINTF("\nError from OMX_FreeBuffer. Output port.\n");
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
#if OPTIMIZE        
            // do nothing
#else
            eError = OMX_SendCommand(pOMXHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
            if ( eError != OMX_ErrorNone ) {
                PRINTF("Error from SendCommand-Idle(nStop) State function\n");
                iState = STATE_ERROR;
            }
#endif        
            break;

        case STATE_LOADED:
        case STATE_ERROR:
            /*### Assume that the Bitmap object/file need not be closed. */
            /*### More needs to be done here */
            /*### Do different things based on iLastState */

            if (pOMXHandle) {
                eError = TIOMX_FreeHandle(pOMXHandle);
                if ( (eError != OMX_ErrorNone) )    {
                    PRINTF("\nError in Free Handle function\n");
                }
            }

            eError = TIOMX_Deinit();
            if ( eError != OMX_ErrorNone ) {
                PRINTF("\nError returned by TIOMX_Deinit()\n");
            }

            iState = STATE_EXIT;

            break;

        default:
            break;
    }

    if (iState == STATE_ERROR) sem_post(semaphore) ;
    if (iState == STATE_EXIT) break;
#if OPTIMIZE
    if (iState == STATE_EMPTY_BUFFER_DONE_CALLED) break ;    
#endif    

    }

}


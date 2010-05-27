#include "CameraHal.h"
#include "OMXCameraAdapter.h"
#include "mmplatform.h"
#include "ErrorUtils.h"



#define HERE(Msg) {CAMHAL_LOGEB("--===line %d, %s===--\n", __LINE__, Msg);}

namespace android {

#define LOG_TAG "OMXCameraAdapter"


/*--------------------Camera Adapter Class STARTS here-----------------------------*/

status_t OMXCameraAdapter::initialize()
{
    LOG_FUNCTION_NAME

    TIMM_OSAL_ERRORTYPE osalError = OMX_ErrorNone;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    status_t ret = NO_ERROR;

    ///Event semaphore used for
    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    ///Initialize the OMX Core
    eError = OMX_Init();

    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_Init - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    ///Setup key parameters to send to Ducati during init
    OMX_CALLBACKTYPE oCallbacks;

    /* Initialize the callback handles */
    oCallbacks.EventHandler    = android::OMXCameraAdapterEventHandler;
    oCallbacks.EmptyBufferDone = android::OMXCameraAdapterEmptyBufferDone;
    oCallbacks.FillBufferDone  = android::OMXCameraAdapterFillBufferDone;

    ///Update the preview and image capture port indexes
    mCameraAdapterParameters.mPrevPortIndex = OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW;
    // temp changed in order to build OMX_CAMERA_PORT_VIDEO_OUT_IMAGE;
    mCameraAdapterParameters.mImagePortIndex = OMX_CAMERA_PORT_IMAGE_OUT_IMAGE;
    mCameraAdapterParameters.mVideoPortIndex = OMX_CAMERA_PORT_VIDEO_OUT_VIDEO;

    ///Get the handle to the OMX Component
    mCameraAdapterParameters.mHandleComp = NULL;
    eError = OMX_GetHandle(&(mCameraAdapterParameters.mHandleComp), //     previously used: OMX_GetHandle
                                (OMX_STRING)"OMX.TI.DUCATI1.VIDEO.CAMERA" ///@todo Use constant instead of hardcoded name
                                , this
                                , &oCallbacks);

    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_GetHandle - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                 OMX_CommandPortDisable,
                                 OMX_ALL,
                                 NULL);

    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandPortDisable) - %x", eError);
        }

    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    ///Register for port enable event
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandPortEnable,
                                mCameraAdapterParameters.mPrevPortIndex,
                                eventSem,
                                -1 ///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }

    ///Enable PREVIEW Port
    eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                OMX_CommandPortEnable,
                                mCameraAdapterParameters.mPrevPortIndex,
                                NULL);

    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandPortEnable) - %x", eError);
        }

    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    //Wait for the port enable event to occur
    eventSem.Wait();

    CAMHAL_LOGDA("-Port enable event arrived");

    mPreviewing     = false;
    mCapturing      = false;
    mFlushBuffers   = false;
    mWaitingForSnapshot = false;
    mComponentState = OMX_StateLoaded;

    return ErrorUtils::omxToAndroidError(eError);

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);
    if(mCameraAdapterParameters.mHandleComp)
        {
        ///Free the OMX component handle in case of error
        OMX_FreeHandle(mCameraAdapterParameters.mHandleComp);
        }

    ///De-init the OMX
    OMX_Deinit();

    LOG_FUNCTION_NAME_EXIT
    return ErrorUtils::omxToAndroidError(eError);
}


void OMXCameraAdapter::returnFrame(void* frameBuf, CameraFrame::FrameType frameType)
{
    OMXCameraPortParameters * mPreviewData = NULL;
    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if ( NULL != frameBuf )
        {
        ///@todo Return the frame to OMXCamera here
        ///Get the buffer header for this pBuffer
        for(int i=0;i<mPreviewData->mNumBufs;i++)
            {
            if(mPreviewData->mBufferHeader[i]->pBuffer==frameBuf)
                {
                eError =OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,mPreviewData->mBufferHeader[i]);
                if(eError!=OMX_ErrorNone)
                    {
                    CAMHAL_LOGEB("OMX_FillThisBuffer %d", eError);
                    return;
                    }
                }
            }
        }
}

int OMXCameraAdapter::setErrorHandler(ErrorNotifier *errorNotifier)
{
    LOG_FUNCTION_NAME
    int ret = NO_ERROR;
    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t OMXCameraAdapter::getCaps()
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;
    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t OMXCameraAdapter::setParameters(const CameraParameters &params)
{
    LOG_FUNCTION_NAME

    status_t ret = NO_ERROR;

    mParams = params;

    ///@todo Include more camera parameters
    int w, h;
    OMX_COLOR_FORMATTYPE pixFormat;
    if ( params.getPreviewFormat() != NULL )
        {
        if (strcmp(params.getPreviewFormat(), (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) == 0)
            {
            CAMHAL_LOGDA("YCbCr format selected");
            pixFormat = OMX_COLOR_FormatYCbYCr;
            }
        else if(strcmp(params.getPreviewFormat(), (const char *) CameraParameters::PIXEL_FORMAT_YUV420SP) == 0)
            {
            CAMHAL_LOGDA("YUV420SP format selected");
            pixFormat = OMX_COLOR_FormatYUV420SemiPlanar;
            }
        else if(strcmp(params.getPreviewFormat(), (const char *) CameraParameters::PIXEL_FORMAT_RGB565) == 0)
            {
            CAMHAL_LOGDA("RGB565 format selected");
            pixFormat = OMX_COLOR_Format16bitRGB565;
            }
        else
            {
            CAMHAL_LOGDA("Invalid format, YCbCr format selected as default");
            pixFormat = OMX_COLOR_FormatYCbYCr;
            }
        }
    else
        {
        CAMHAL_LOGEA("Preview format is NULL, defaulting to YCbCr");
        pixFormat = OMX_COLOR_FormatYCbYCr;
        }

    params.getPreviewSize(&w, &h);
    int frameRate = params.getPreviewFrameRate();

    CAMHAL_LOGDB("Preview frame rate %d", frameRate);

    OMXCameraPortParameters *cap;
    cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

    cap->mColorFormat = pixFormat;
    cap->mWidth = w;
    cap->mHeight = h;
    cap->mFrameRate = frameRate;

    CAMHAL_LOGDB("Prev: cap.mColorFormat = %d", cap->mColorFormat);
    CAMHAL_LOGDB("Prev: cap.mWidth = %d", cap->mWidth);
    CAMHAL_LOGDB("Prev: cap.mHeight = %d", cap->mHeight);
    CAMHAL_LOGDB("Prev: cap.mFrameRate = %d", cap->mFrameRate);

    ///mStride is set from setBufs() while passing the APIs
    cap->mStride = 4096;
    cap->mBufSize = cap->mStride * cap->mHeight;

    cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    params.getPictureSize(&w, &h);
    cap->mWidth = w;
    cap->mHeight = h;
    //TODO: Support more pixelformats
    cap->mStride = w*2;
    cap->mBufSize = cap->mStride * cap->mHeight;
    cap->mNumBufs = mCaptureBuffersCount;

    CAMHAL_LOGDB("Image: cap.mWidth = %d", cap->mWidth);
    CAMHAL_LOGDB("Image: cap.mHeight = %d", cap->mHeight);

    if ( params.getInt(KEY_ROTATION) != -1 )
        {
        mPictureRotation = params.getInt(KEY_ROTATION);
        }

    LOG_FUNCTION_NAME_EXIT
    return ret;
}


status_t OMXCameraAdapter::setFormat(OMX_U32 port, OMXCameraPortParameters &portParams)
{
    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portCheck;

    OMX_INIT_STRUCT_PTR (&portCheck, OMX_PARAM_PORTDEFINITIONTYPE);

    portCheck.nPortIndex = port;

    eError = OMX_GetParameter (mCameraAdapterParameters.mHandleComp,
                                OMX_IndexParamPortDefinition, &portCheck);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_GetParameter - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    if ( OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW == port )
        {
        portCheck.format.video.nFrameWidth      = portParams.mWidth;
        portCheck.format.video.nFrameHeight     = portParams.mHeight;
        portCheck.format.video.eColorFormat     = portParams.mColorFormat;
        portCheck.format.video.nStride          = portParams.mStride;
        portCheck.format.video.xFramerate       = portParams.mFrameRate<<16;
        portCheck.nBufferSize                   = portParams.mStride * portParams.mHeight;
        portCheck.nBufferCountActual = portParams.mNumBufs;
        }
    else if ( OMX_CAMERA_PORT_IMAGE_OUT_IMAGE == port )
        {
        portCheck.format.image.nFrameWidth      = portParams.mWidth;
        portCheck.format.image.nFrameHeight     = portParams.mHeight;
        portCheck.format.image.eColorFormat     = OMX_COLOR_FormatCbYCrY;
        portCheck.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
        portCheck.format.image.nStride          = portParams.mStride;
        portCheck.nBufferSize                   = mCaptureBuffersLength;
        portCheck.nBufferCountActual = 1;
        }

    eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp,
                            OMX_IndexParamPortDefinition, &portCheck);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SetParameter - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    /* check if parameters are set correctly by calling GetParameter() */
    eError = OMX_GetParameter(mCameraAdapterParameters.mHandleComp,
                                        OMX_IndexParamPortDefinition, &portCheck);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_GetParameter - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    LOGD("\n *** PRV Width = %ld", portCheck.format.video.nFrameWidth);
    LOGD("\n ***PRV Height = %ld", portCheck.format.video.nFrameHeight);

    LOGD("\n ***PRV IMG FMT = %x", portCheck.format.video.eColorFormat);
    LOGD("\n ***PRV portCheck.nBufferSize = %ld\n",portCheck.nBufferSize);
    LOGD("\n ***PRV portCheck.nBufferCountMin = %ld\n",
                                            portCheck.nBufferCountMin);
    LOGD("\n ***PRV portCheck.nBufferCountActual = %ld\n",
                                            portCheck.nBufferCountActual);
    LOGD("\n ***PRV portCheck.format.video.nStride = %ld\n",
                                            portCheck.format.video.nStride);

    LOG_FUNCTION_NAME_EXIT
    return ErrorUtils::omxToAndroidError(eError);

    ///If there is any failure, we reach here.
    ///Here, we do any resource freeing and convert from OMX error code to Camera Hal error code
    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, eError);
    LOG_FUNCTION_NAME_EXIT
    return ErrorUtils::omxToAndroidError(eError);

}


status_t OMXCameraAdapter::flushBuffers()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    TIMM_OSAL_ERRORTYPE err;
    TIMM_OSAL_U32 uRequestedEvents = OMXCameraAdapter::CAMERA_PORT_FLUSH;
    TIMM_OSAL_U32 pRetrievedEvents;

    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    LOG_FUNCTION_NAME


    OMXCameraPortParameters * mPreviewData = NULL;
    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

    Mutex::Autolock lock(mLock);

    if(!mPreviewing || mFlushBuffers)
        {
        LOG_FUNCTION_NAME_EXIT
        return NO_ERROR;
        }

    ///If preview is ongoing and we get a new set of buffers, flush the o/p queue,
    ///wait for all buffers to come back and then queue the new buffers in one shot
    ///Stop all callbacks when this is ongoing
    mFlushBuffers = true;

    ///Register for the FLUSH event
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandFlush,
                                OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW,
                                eventSem,
                                -1///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }

    ///Send FLUSH command to preview port
    eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp, OMX_CommandFlush,
                                                    OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW,
                                                    NULL);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandFlush)- %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    ///Wait for the FLUSH event to occur
    eventSem.Wait();

    CAMHAL_LOGDA("Flush event received");

    LOG_FUNCTION_NAME_EXIT

    return (ret | ErrorUtils::omxToAndroidError(eError));

    EXIT:
    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);
    LOG_FUNCTION_NAME_EXIT
    ///@todo Handle both OMX and Camera HAL errors together correctly
    return (ret | ErrorUtils::omxToAndroidError(eError));
}

///API to give the buffers to Adapter
status_t OMXCameraAdapter::useBuffers(CameraMode mode, void* bufArr, int num)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    switch(mode)
        {
        case CAMERA_PREVIEW:
            ret = UseBuffersPreview(bufArr, num);
            break;

        case CAMERA_IMAGE_CAPTURE:
            ret = UseBuffersCapture(bufArr, num);
            break;

        case CAMERA_VIDEO:
            ///@todo when video capture is supported
            break;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::UseBuffersPreview(void* bufArr, int num)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    LOG_FUNCTION_NAME

    if(mComponentState!=OMX_StateLoaded)
        {
        CAMHAL_LOGEA("Calling UseBuffersPreview() when not in LOADED state");
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
        }

    if(!bufArr)
        {
        CAMHAL_LOGEA("NULL pointer passed for buffArr");
        LOG_FUNCTION_NAME_EXIT
        return BAD_VALUE;
        }

    OMXCameraPortParameters * mPreviewData = NULL;
    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
    mPreviewData->mNumBufs = num ;
    uint32_t *buffers = (uint32_t*)bufArr;

    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    if(mPreviewing && (mPreviewData->mNumBufs!=num))
        {
        CAMHAL_LOGEA("Current number of buffers doesnt equal new num of buffers passed!");
        LOG_FUNCTION_NAME_EXIT
        return BAD_VALUE;
        }

    ///If preview is ongoing
    if(mPreviewing)
        {
        ///If preview is ongoing and we get a new set of buffers, flush the o/p queue,
        ///wait for all buffers to come back and then queue the new buffers in one shot
        ///Stop all callbacks when this is ongoing
        mFlushBuffers = true;

        ///Register for the FLUSH event
        ///This method just inserts a message in Event Q, which is checked in the callback
        ///The sempahore passed is signalled by the callback
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                    OMX_EventCmdComplete,
                                    OMX_CommandFlush,
                                    OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW,
                                    eventSem,
                                    -1 ///Infinite timeout
                                    );
        if(ret!=NO_ERROR)
            {
            CAMHAL_LOGEB("Error in registering for event %d", ret);
            goto EXIT;
            }
        ///Send FLUSH command to preview port
        eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp, OMX_CommandFlush,
                                                        OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW,
                                                        NULL);
        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandFlush)- %x", eError);
            }
        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

        ///Wait for the FLUSH event to occur
        eventSem.Wait();
        CAMHAL_LOGDA("Flush event received");


        ///If flush has already happened, we need to update the pBuffer pointer in
        ///the buffer header and call OMX_FillThisBuffer to queue all the new buffers
        for(int index=0;index<num;index++)
            {
            CAMHAL_LOGDB("Queuing new buffers to Ducati 0x%x",((int32_t*)bufArr)[index]);

            mPreviewData->mBufferHeader[index]->pBuffer = (OMX_U8*)((int32_t*)bufArr)[index];

            eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                        (OMX_BUFFERHEADERTYPE*)mPreviewData->mBufferHeader[index]);

            if(eError!=OMX_ErrorNone)
                {
                CAMHAL_LOGEB("OMX_FillThisBuffer- %x", eError);
                }
            GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

            }
        ///Once we queued new buffers, we set the flushBuffers flag to false
        mFlushBuffers = false;

        ret = ErrorUtils::omxToAndroidError(eError);

        ///Return from here
        return ret;
        }

    ret = setFormat(OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW, *mPreviewData);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("setFormat() failed %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    ///Register for IDLE state switch event
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandStateSet,
                                OMX_StateIdle,
                                eventSem,
                                -1 ///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }


    ///Once we get the buffers, move component state to idle state and pass the buffers to OMX comp using UseBuffer
    eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp , OMX_CommandStateSet, OMX_StateIdle, NULL);

    CAMHAL_LOGDB("OMX_SendCommand(OMX_CommandStateSet) 0x%x", eError);
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    for(int index=0;index<num;index++)
        {
        OMX_BUFFERHEADERTYPE *pBufferHdr;
        CAMHAL_LOGDB("OMX_UseBuffer(0x%x)", buffers[index]);
        eError = OMX_UseBuffer( mCameraAdapterParameters.mHandleComp,
                                &pBufferHdr,
                                mCameraAdapterParameters.mPrevPortIndex,
                                0,
                                mPreviewData->mBufSize,
                                (OMX_U8*)buffers[index]);
        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_UseBuffer- %x", eError);
            }
        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

        pBufferHdr->pAppPrivate = (OMX_PTR)pBufferHdr;
        pBufferHdr->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pBufferHdr->nVersion.s.nVersionMajor = 1 ;
        pBufferHdr->nVersion.s.nVersionMinor = 1 ;
        pBufferHdr->nVersion.s.nRevision = 0 ;
        pBufferHdr->nVersion.s.nStep =  0;
        mPreviewData->mBufferHeader[index] = pBufferHdr;
        }

    CAMHAL_LOGDA("LOADED->IDLE state changed");
    ///Wait for state to switch to idle
    eventSem.Wait();
    CAMHAL_LOGDA("LOADED->IDLE state changed");

    mComponentState = OMX_StateIdle;


    LOG_FUNCTION_NAME_EXIT
    return (ret | ErrorUtils::omxToAndroidError(eError));

    ///If there is any failure, we reach here.
    ///Here, we do any resource freeing and convert from OMX error code to Camera Hal error code
    EXIT:
    LOG_FUNCTION_NAME_EXIT
    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);
    return (ret | ErrorUtils::omxToAndroidError(eError));
}

status_t OMXCameraAdapter::UseBuffersCapture(void* bufArr, int num)
{
    LOG_FUNCTION_NAME

    status_t ret;
    OMX_ERRORTYPE eError;
    OMXCameraPortParameters * imgCaptureData = NULL;
    uint32_t *buffers = (uint32_t*)bufArr;
    Semaphore camSem;
    OMXCameraPortParameters cap;

    imgCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];
    imgCaptureData->mNumBufs = num;

    camSem.Create();

    if ( mCapturing )
        {

        ///Register for Image port Disable event
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                    OMX_EventCmdComplete,
                                    OMX_CommandPortDisable,
                                    mCameraAdapterParameters.mImagePortIndex,
                                    camSem,
                                    -1);

        ///Disable Capture Port
        eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                    OMX_CommandPortDisable,
                                    mCameraAdapterParameters.mImagePortIndex,
                                    NULL);

        CAMHAL_LOGDA("Waiting for port disable");
        //Wait for the image port enable event
        camSem.Wait();
        CAMHAL_LOGDA("Port disabled");

        }

    //TODO: Support more pixelformats

    LOGE("Params Width = %d", imgCaptureData->mWidth);
    LOGE("Params Height = %d", imgCaptureData->mWidth);

    ret = setFormat(OMX_CAMERA_PORT_IMAGE_OUT_IMAGE, *imgCaptureData);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("setFormat() failed %d", ret);
        LOG_FUNCTION_NAME_EXIT
         return ret;
        }

    ///Register for Image port ENABLE event
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandPortEnable,
                                mCameraAdapterParameters.mImagePortIndex,
                                camSem,
                                -1);

    ///Enable Capture Port
    eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                OMX_CommandPortEnable,
                                mCameraAdapterParameters.mImagePortIndex,
                                NULL);

    for(int index=0;index<num;index++)
    {
        OMX_BUFFERHEADERTYPE *pBufferHdr;
        CAMHAL_LOGDB("OMX_UseBuffer Capture address: 0x%x, size = %d", buffers[index], imgCaptureData->mBufSize);

        eError = OMX_UseBuffer( mCameraAdapterParameters.mHandleComp,
                                &pBufferHdr,
                                mCameraAdapterParameters.mImagePortIndex,
                                0,
                                mCaptureBuffersLength,
                                (OMX_U8*)buffers[index]);

        CAMHAL_LOGDB("OMX_UseBuffer = 0x%x", eError);

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

        pBufferHdr->pAppPrivate = (OMX_PTR)pBufferHdr;
        pBufferHdr->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pBufferHdr->nVersion.s.nVersionMajor = 1 ;
        pBufferHdr->nVersion.s.nVersionMinor = 1 ;
        pBufferHdr->nVersion.s.nRevision = 0;
        pBufferHdr->nVersion.s.nStep =  0;
        imgCaptureData->mBufferHeader[index] = pBufferHdr;
    }

    //Wait for the image port enable event
    CAMHAL_LOGDA("Waiting for port enable");
    camSem.Wait();
    CAMHAL_LOGDA("Port enabled");

    mCapturing = true;

    EXIT:
    LOG_FUNCTION_NAME_EXIT
    return ret;
}


CameraParameters OMXCameraAdapter::getParameters() const
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT
    return mParameters;
}

//API to send a command to the camera
status_t OMXCameraAdapter::sendCommand(int operation, int value1, int value2, int value3)
{
    LOG_FUNCTION_NAME

    status_t ret = NO_ERROR;
    CameraAdapter::CameraMode mode;
    struct timeval *refTimestamp;
    BuffersDescriptor *desc = NULL;
    Message msg;

    switch ( operation ) {
        case CameraAdapter::CAMERA_USE_BUFFERS:
                {
                CAMHAL_LOGDA("Use Buffers");
                mode = ( CameraAdapter::CameraMode ) value1;
                desc = ( BuffersDescriptor * ) value2;

                if ( CameraAdapter::CAMERA_PREVIEW == mode )
                    {
                    if ( NULL == desc )
                        {
                        CAMHAL_LOGEA("Invalid preview buffers!");
                        ret = -1;
                        }

                    if ( ret == NO_ERROR )
                        {
                        Mutex::Autolock lock(mPreviewBufferLock);

                        mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex].mNumBufs =  desc->mCount;
                        mPreviewBuffers = (int *) desc->mBuffers;

                        mPreviewBuffersAvailable.clear();
                        for ( int i = 0 ; i < desc->mCount ; i++ )
                            {
                            mPreviewBuffersAvailable.add(mPreviewBuffers[i], true);
                            }
                        }
                    }
                else if( CameraAdapter::CAMERA_IMAGE_CAPTURE == mode )
                    {
                    if ( NULL == desc )
                        {
                        CAMHAL_LOGEA("Invalid capture buffers!");
                        ret = -1;
                        }
                    if ( ret == NO_ERROR )
                        {
                        Mutex::Autolock lock(mCaptureBufferLock);
                        mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex].mNumBufs = desc->mCount;
                        mCaptureBuffers = (int *) desc->mBuffers;
                        mCaptureBuffersLength = desc->mLength;
                        mCaptureBuffersAvailable.clear();
                        for ( int i = 0 ; i < desc->mCount ; i++ )
                            {
                            mCaptureBuffersAvailable.add(mCaptureBuffers[i], true);
                            }
                        }
                    }
                else
                    {
                    CAMHAL_LOGEB("Camera Mode %x still not supported!", mode);
                    }

                if ( NULL != desc )
                    {
                    useBuffers(mode, desc->mBuffers, desc->mCount);
                    }
                break;
            }

        case CameraAdapter::CAMERA_START_PREVIEW:
                {
                CAMHAL_LOGDA("Start Preview");
                ret = startPreview();

                break;
            }

        case CameraAdapter::CAMERA_STOP_PREVIEW:
            {
            CAMHAL_LOGDA("Stop Preview");
            stopPreview();
            break;
            }

        case CameraAdapter::CAMERA_PREVIEW_FLUSH_BUFFERS:
            {
            flushBuffers();
            break;
            }

        case CameraAdapter::CAMERA_START_IMAGE_CAPTURE:
            {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            refTimestamp = ( struct timeval * ) value1;
            if ( NULL != refTimestamp )
                {
                memcpy( &mStartCapture, refTimestamp, sizeof(struct timeval));
                }

#endif

            ret = startImageCapture();
            break;
            }
        case CameraAdapter::CAMERA_STOP_IMAGE_CAPTURE:
            {
            ret = stopImageCapture();
            break;
            }
         case CameraAdapter::CAMERA_PERFORM_AUTOFOCUS:

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            refTimestamp = ( struct timeval * ) value1;
            if ( NULL != refTimestamp )
                {
                memcpy( &mStartFocus, refTimestamp, sizeof(struct timeval));
                }

#endif

            ret = doAutoFocus();

            break;

        default:
            CAMHAL_LOGEB("Command 0x%x unsupported!", operation);
            break;
    };

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t OMXCameraAdapter::startPreview()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }


    if(mComponentState!=OMX_StateIdle)
        {
        CAMHAL_LOGEA("Calling UseBuffersPreview() when not in IDLE state");
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
        }

    LOG_FUNCTION_NAME

    OMXCameraPortParameters * mPreviewData = NULL;
    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

    ///Register for EXECUTING state transition.
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandStateSet,
                                OMX_StateExecuting,
                                eventSem,
                                -1 ///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }


    ///Switch to EXECUTING state
    ret = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                OMX_CommandStateSet,
                                OMX_StateExecuting,
                                NULL);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_StateExecuting)- %x", eError);
        }
    if( NO_ERROR == ret)
        {
        mPreviewing = true;
        }
    else
        {
        goto EXIT;
        }


    CAMHAL_LOGDA("+Waiting for component to go into EXECUTING state");
    ///Perform the wait for Executing state transition
    ///@todo Replace with a timeout
    eventSem.Wait();
    CAMHAL_LOGDA("+Great. Component went into executing state!!");


    ///Queue all the buffers on preview port
    for(int index=0;index< mPreviewData->mNumBufs;index++)
        {
        CAMHAL_LOGDB("Queuing buffer on Preview port - 0x%x", mPreviewData->mBufferHeader[index]->pBuffer);
        eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                    (OMX_BUFFERHEADERTYPE*)mPreviewData->mBufferHeader[index]);
        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_FillThisBuffer- %x", eError);
            }
        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
        }

    mComponentState = OMX_StateExecuting;

    LOG_FUNCTION_NAME_EXIT
    return ret;

    EXIT:
      CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);
      LOG_FUNCTION_NAME_EXIT
      return (ret | ErrorUtils::omxToAndroidError(eError));

}

status_t OMXCameraAdapter::stopPreview()
{
    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    status_t ret = NO_ERROR;

    OMXCameraPortParameters *mCaptureData , * mPreviewData;
    mCaptureData = mPreviewData = NULL;

    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
    mCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    Mutex::Autolock lock(mPreviewBufferLock);

    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }


    ///Register for EXECUTING state transition.
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandStateSet,
                                OMX_StateIdle,
                                eventSem,
                                -1 ///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }


    ret = OMX_SendCommand (mCameraAdapterParameters.mHandleComp,
                                OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_StateIdle) - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    ///Wait for the EXECUTING ->IDLE transition to arrive
    eventSem.Wait();

    ///Register for LOADED state transition.
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandStateSet,
                                OMX_StateLoaded,
                                eventSem,
                                -1 ///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }


    eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp,
                            OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_StateLoaded) - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    ///Free the OMX Buffers
    for(int i=0;i<mPreviewData->mNumBufs;i++)
        {
        eError = OMX_FreeBuffer(mCameraAdapterParameters.mHandleComp,
                            mCameraAdapterParameters.mPrevPortIndex,
                            mPreviewData->mBufferHeader[i]);
        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_FreeBuffer - %x", eError);
            }
        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
        }

    ///Wait for the IDLE -> LOADED transition to arrive
    eventSem.Wait();

    mComponentState = OMX_StateLoaded;

    ///Clear all the available preview buffers
    mPreviewBuffersAvailable.clear();

    ///Clear the previewing flag, we are no longer previewing
    mPreviewing = false;

    LOG_FUNCTION_NAME_EXIT

    return (ret | ErrorUtils::omxToAndroidError(eError));

    EXIT:
        CAMHAL_LOGEB("Exiting function because of eError= %x", eError);
        LOG_FUNCTION_NAME_EXIT
        return (ret | ErrorUtils::omxToAndroidError(eError));

}

status_t OMXCameraAdapter::setPictureRotation(unsigned int degree)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_ROTATIONTYPE rotation;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT(rotation, OMX_CONFIG_ROTATIONTYPE);
        rotation.nRotation = degree;
        rotation.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonRotate, &rotation);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEA("Error while configuring rotation");
            ret = -1;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::doAutoFocus()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE focusControl;

    LOG_FUNCTION_NAME

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component not in executing state");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {
        focusControl.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
        focusControl.eFocusControl = OMX_IMAGE_FocusControlOn;
        focusControl.nSize = sizeof(OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE);
        focusControl.nVersion.s.nVersionMajor = 0x1 ;
        focusControl.nVersion.s.nVersionMinor = 0x1 ;
        focusControl.nVersion.s.nRevision = 0x0;
        focusControl.nVersion.s.nStep =  0x0;

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigFocusControl, &focusControl);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEA("Error while starting focus");
            ret = -1;
            }
        else
            {
            mFocusStarted = true;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::checkFocus(OMX_FOCUSSTATUSTYPE *eFocusStatus)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_FOCUSSTATUSTYPE focusStatus;

    LOG_FUNCTION_NAME

    if ( NULL == eFocusStatus )
        {
        CAMHAL_LOGEA("Invalid focus status");
        ret = -1;
        }

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component not in executing state");
        ret = -1;
        }

    if ( !mFocusStarted )
        {
        CAMHAL_LOGEA("Focus was not started");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {
        focusStatus.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
        focusStatus.nSize = sizeof(OMX_PARAM_FOCUSSTATUSTYPE);
        focusStatus.nVersion.s.nVersionMajor = 0x1 ;
        focusStatus.nVersion.s.nVersionMinor = 0x1 ;
        focusStatus.nVersion.s.nRevision = 0x0;
        focusStatus.nVersion.s.nStep =  0x0;

        eError = OMX_GetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonFocusStatus, &focusStatus);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while retrieving focus status: 0x%x", eError);
            ret = -1;
            }
        }

    if ( NO_ERROR == ret )
        {
        *eFocusStatus = focusStatus.eFocusStatus;
        CAMHAL_LOGDB("Focus Status: %d", focusStatus.eFocusStatus);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::notifyFocusSubscribers()
{
    event_callback eventCb;
    CameraHalEvent focusEvent;
    CameraHalEvent::FocusEventData focusData;
    OMX_FOCUSSTATUSTYPE eFocusStatus;
    bool focusStatus = false;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( mFocusStarted )
        {
        ret = checkFocus(&eFocusStatus);

        if ( NO_ERROR == ret )
            {

            switch(eFocusStatus)
                {
                case OMX_FocusStatusReached:
                    //AF success
                    focusStatus = true;
                    mFocusStarted = false;
                    break;

                case OMX_FocusStatusUnableToReach:
                case OMX_FocusStatusLost:
                case OMX_FocusStatusOff:
                    //AF fail
                    focusStatus = false;
                    mFocusStarted = false;
                    break;

                case OMX_FocusStatusRequest:
                default:
                    //do nothing
                    break;

                };
            }
        }
    else
        {
        return NO_ERROR;
        }

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

     //dump the AF latency
     CameraHal::PPM("Focus finished in: ", &mStartFocus);

#endif

    focusData.focusLocked = focusStatus;
    focusData.focusError = !focusStatus;
    focusEvent.mEventType = CameraHalEvent::EVENT_FOCUS_LOCKED;
    focusEvent.mEventData = (CameraHalEvent::CameraHalEventData *) &focusData;

        //Start looking for event subscribers
        {
        Mutex::Autolock lock(mSubscriberLock);

            if ( mFocusSubscribers.size() == 0 )
                CAMHAL_LOGDA("No Focus Subscribers!");

            for (unsigned int i = 0 ; i < mFocusSubscribers.size(); i++ )
                {
                focusEvent.mCookie = (void *) mFocusSubscribers.keyAt(i);
                eventCb = (event_callback) mFocusSubscribers.valueAt(i);

                eventCb ( &focusEvent );
                }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::startImageCapture()
{
    LOG_FUNCTION_NAME

    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMXCameraPortParameters * capData = NULL;
    OMX_CONFIG_BOOLEANTYPE bOMX;

    ret = setPictureRotation(mPictureRotation);

    capData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    memset(&bOMX, 0x0, sizeof(OMX_CONFIG_BOOLEANTYPE));
    bOMX.nVersion.s.nVersionMajor = capData->mBufferHeader[0]->nVersion.s.nVersionMajor ;//0x1;
    bOMX.nVersion.s.nVersionMinor = capData->mBufferHeader[0]->nVersion.s.nVersionMinor ;//0x1;
    bOMX.nVersion.s.nRevision = capData->mBufferHeader[0]->nVersion.s.nRevision ;//0x0;
    bOMX.nVersion.s.nStep = capData->mBufferHeader[0]->nVersion.s.nStep ;//0x0;
    bOMX.nSize = sizeof(OMX_CONFIG_BOOLEANTYPE);

    ///Queue all the buffers on capture port
    for(int index=0;index< capData->mNumBufs;index++)
        {
        CAMHAL_LOGDB("Queuing buffer on Capture port - 0x%x", capData->mBufferHeader[index]->pBuffer);
        eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                    (OMX_BUFFERHEADERTYPE*)capData->mBufferHeader[index]);

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
        }

    /// sending Capturing Command to th ecomponent
    bOMX.bEnabled = OMX_TRUE;
    eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCapturing, &bOMX);

    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    mCapturing = true;
    mWaitingForSnapshot = true;

    EXIT:
    return ret;
}

status_t OMXCameraAdapter::stopImageCapture()
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;
    OMXCameraPortParameters * capData = NULL;
    OMX_ERRORTYPE eError;
    Semaphore camSem;

    camSem.Create();

    OMX_CONFIG_BOOLEANTYPE bOMX;

    capData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    memset(&bOMX, 0x0, sizeof(OMX_CONFIG_BOOLEANTYPE));
    bOMX.nVersion.s.nVersionMajor = capData->mBufferHeader[0]->nVersion.s.nVersionMajor ;//0x1;
    bOMX.nVersion.s.nVersionMinor = capData->mBufferHeader[0]->nVersion.s.nVersionMinor ;//0x1;
    bOMX.nVersion.s.nRevision = capData->mBufferHeader[0]->nVersion.s.nRevision ;//0x0;
    bOMX.nVersion.s.nStep = capData->mBufferHeader[0]->nVersion.s.nStep ; //0x0;
    bOMX.nSize = sizeof(OMX_CONFIG_BOOLEANTYPE);

    bOMX.bEnabled = OMX_FALSE;
    eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCapturing, &bOMX);

    ///Free all the buffers on capture port
    for ( int index = 0 ; index < capData->mNumBufs ; index++)
        {
        CAMHAL_LOGDB("Freeing buffer on Capture port - 0x%x", capData->mBufferHeader[index]->pBuffer);
        eError = OMX_FreeBuffer(mCameraAdapterParameters.mHandleComp,
                        mCameraAdapterParameters.mImagePortIndex,
                        (OMX_BUFFERHEADERTYPE*)capData->mBufferHeader[index]);

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
        }

    mWaitingForSnapshot = false;

EXIT:

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

//API to cancel a currently executing command
status_t OMXCameraAdapter::cancelCommand(int operation)
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR;
}

//API to get the frame size required to be allocated. This size is used to override the size passed
//by camera service when VSTAB/VNF is turned ON for example
void OMXCameraAdapter::getFrameSize(int &width, int &height)
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT
}

/* Application callback Functions */
/*========================================================*/
/* @ fn SampleTest_EventHandler :: Application callback   */
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapterEventHandler(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_PTR pAppData,
                                          OMX_IN OMX_EVENTTYPE eEvent,
                                          OMX_IN OMX_U32 nData1,
                                          OMX_IN OMX_U32 nData2,
                                          OMX_IN OMX_PTR pEventData)
{
    LOG_FUNCTION_NAME

    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMXCameraAdapter *oca = (OMXCameraAdapter*)pAppData;
    ret = oca->OMXCameraAdapterEventHandler(hComponent, eEvent, nData1, nData2, pEventData);

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

/* Application callback Functions */
/*========================================================*/
/* @ fn SampleTest_EventHandler :: Application callback   */
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapter::OMXCameraAdapterEventHandler(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_EVENTTYPE eEvent,
                                          OMX_IN OMX_U32 nData1,
                                          OMX_IN OMX_U32 nData2,
                                          OMX_IN OMX_PTR pEventData)
{

    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    switch (eEvent) {
        case OMX_EventCmdComplete:
            CAMHAL_LOGDB("+OMX_EventCmdComplete %d %d", nData1, nData2);

            if (OMX_CommandStateSet == nData1) {
                mCameraAdapterParameters.mState = (OMX_STATETYPE) nData2;

            } else if (OMX_CommandFlush == nData1) {
                CAMHAL_LOGDB("OMX_CommandFlush received for port %d", (int)nData2);

            } else if (OMX_CommandPortDisable == nData1) {
                CAMHAL_LOGDB("OMX_CommandPortDisable received for port %d", (int)nData2);

            } else if (OMX_CommandPortEnable == nData1) {
                CAMHAL_LOGDB("OMX_CommandPortEnable received for port %d", (int)nData2);

            } else if (OMX_CommandMarkBuffer == nData1) {
                ///This is not used currently
            }

            CAMHAL_LOGDA("-OMX_EventCmdComplete");
        break;

        case OMX_EventError:
            CAMHAL_LOGDB("OMX interface failed to execute OMX command %d", nData1);
            CAMHAL_LOGDA("See OMX_INDEXTYPE for reference");
        break;

        case OMX_EventMark:
        break;

        case OMX_EventPortSettingsChanged:
        break;

        case OMX_EventBufferFlag:
        break;

        case OMX_EventResourcesAcquired:
        break;

        case OMX_EventComponentResumed:
        break;

        case OMX_EventDynamicResourcesAvailable:
        break;

        case OMX_EventPortFormatDetected:
        break;

        default:
        break;
    }

    ///Signal to the thread(s) waiting that the event has occured
    SignalEvent(hComponent, eEvent, nData1, nData2, pEventData);

   LOG_FUNCTION_NAME_EXIT
   return eError;

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of eError=%x", __FUNCTION__, eError);
    LOG_FUNCTION_NAME_EXIT
    return eError;
}

OMX_ERRORTYPE OMXCameraAdapter::SignalEvent(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_EVENTTYPE eEvent,
                                          OMX_IN OMX_U32 nData1,
                                          OMX_IN OMX_U32 nData2,
                                          OMX_IN OMX_PTR pEventData)
{
    LOG_FUNCTION_NAME
    if(!mEventSignalQ.isEmpty())
        {
        CAMHAL_LOGDA("Event queue not empty");
        Message msg;
        mEventSignalQ.get(&msg);
        ///If any of the message parameters are not set, then that is taken as listening for all events/event parameters
        if((msg.command!=0 || msg.command == (unsigned int)(eEvent))
            && (!msg.arg1 || (OMX_U32)msg.arg1 == nData1)
            && (!msg.arg2 || (OMX_U32)msg.arg2 == nData2)
            && msg.arg3)
            {
            Semaphore *sem  = (Semaphore*) msg.arg3;
            CAMHAL_LOGDA("Event matched, signalling sem");
            ///Signal the semaphore provided
            sem->Signal();
            }
        else
            {
            ///Put the message back in the queue
            CAMHAL_LOGDA("Event didnt match, putting the message back in Q");
            mEventSignalQ.put(&msg);
            }
        }

    LOG_FUNCTION_NAME_EXIT
    return OMX_ErrorNone;
}

status_t OMXCameraAdapter::RegisterForEvent(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_EVENTTYPE eEvent,
                                          OMX_IN OMX_U32 nData1,
                                          OMX_IN OMX_U32 nData2,
                                          OMX_IN Semaphore &semaphore,
                                          OMX_IN OMX_U32 timeout)
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = (unsigned int)eEvent;
    msg.arg1 = (void*)nData1;
    msg.arg2 = (void*)nData2;
    msg.arg3 = (void*)&semaphore;
    msg.arg4 = (void*)hComponent;

    LOG_FUNCTION_NAME_EXIT
    return mEventSignalQ.put(&msg);
}

/*========================================================*/
/* @ fn SampleTest_EmptyBufferDone :: Application callback*/
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapterEmptyBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_PTR pAppData,
                                   OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader)
{
    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMXCameraAdapter *oca = (OMXCameraAdapter*)pAppData;
    eError = oca->OMXCameraAdapterEmptyBufferDone(hComponent, pBuffHeader);

    LOG_FUNCTION_NAME_EXIT
    return eError;
}


/*========================================================*/
/* @ fn SampleTest_EmptyBufferDone :: Application callback*/
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapter::OMXCameraAdapterEmptyBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader)
{

   LOG_FUNCTION_NAME

   LOG_FUNCTION_NAME_EXIT

   return OMX_ErrorNone;
}

/*========================================================*/
/* @ fn SampleTest_FillBufferDone ::  Application callback*/
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapterFillBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_PTR pAppData,
                                   OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMXCameraAdapter *oca = (OMXCameraAdapter*)pAppData;
    eError = oca->OMXCameraAdapterFillBufferDone(hComponent, pBuffHeader);

    return eError;
}

/*========================================================*/
/* @ fn SampleTest_FillBufferDone ::  Application callback*/
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapter::OMXCameraAdapterFillBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader)
{

    status_t  ret = NO_ERROR;
    OMXCameraPortParameters  *pPortParam;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int typeOfFrame;

    pPortParam = &(mCameraAdapterParameters.mCameraPortParams[pBuffHeader->nOutputPortIndex]);
    if (pBuffHeader->nOutputPortIndex == OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW)
        {
        if( mWaitingForSnapshot )
            {
            ret = sendFrameToSubscribers(pBuffHeader, CameraFrame::SNAPSHOT_FRAME );
            mWaitingForSnapshot = false;
            }
        else
            {

            notifyFocusSubscribers();

            ret = sendFrameToSubscribers(pBuffHeader, CameraFrame::PREVIEW_FRAME_SYNC);
            ///Send the frame to subscribers, if no subscribers, queue the frame back
            }
        }
    else if( pBuffHeader->nOutputPortIndex == OMX_CAMERA_PORT_IMAGE_OUT_IMAGE )
        {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

        CameraHal::PPM("Shot to Jpeg: ", &mStartCapture);

#endif

        ret = sendFrameToSubscribers(pBuffHeader, CameraFrame::IMAGE_FRAME);
        }
    else
        {
        CAMHAL_LOGEA("Frame received for non-(preview/capture) port. This is yet to be supported");
        goto EXIT;
        }

    if(ret != NO_ERROR)
        {
        CAMHAL_LOGEA("Error in sending frames to subscribers");
        CAMHAL_LOGDB("sendFrameToSubscribers error: %d", ret);
        }
    return eError;

    EXIT:
    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);
    return eError;
}

status_t OMXCameraAdapter::sendFrameToSubscribers(OMX_IN OMX_BUFFERHEADERTYPE *pBuffHeader, int typeOfFrame)
{
    frame_callback callback;
    CameraFrame cFrame;
//    LOG_FUNCTION_NAME

     if(CameraFrame::IMAGE_FRAME == typeOfFrame )
     {
        for (int i = 0 ; i < mImageSubscribers.size(); i++ )
        {
            cFrame.mBuffer = pBuffHeader->pBuffer;
            cFrame.mFrameType = typeOfFrame;
            cFrame.mLength = pBuffHeader->nFilledLen;
            cFrame.mTimestamp = pBuffHeader->nTimeStamp;
            cFrame.mCookie = (void *) mImageSubscribers.keyAt(i);
            callback = (frame_callback) mImageSubscribers.valueAt(i);
            callback(&cFrame);
        }
        stopImageCapture();
     }
     else
     {
        for(int i = 0 ; i < mFrameSubscribers.size(); i++ )
        {
            cFrame.mFrameType = typeOfFrame;
            cFrame.mBuffer = pBuffHeader->pBuffer;
            cFrame.mCookie = (void *) mFrameSubscribers.keyAt(i);
            callback = (frame_callback) mFrameSubscribers.valueAt(i);
            callback(&cFrame);
        }
     }
//    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR;


}

OMXCameraAdapter::OMXCameraAdapter():mComponentState (OMX_StateInvalid)
{
    LOG_FUNCTION_NAME

    mFocusStarted = false;
    mPictureRotation = 0;

    LOG_FUNCTION_NAME_EXIT
}

OMXCameraAdapter::~OMXCameraAdapter()
{
    LOG_FUNCTION_NAME

    ///Free the handle for the Camera component
    if(mCameraAdapterParameters.mHandleComp)
        {
        OMX_FreeHandle(mCameraAdapterParameters.mHandleComp);
        }

    ///De-init the OMX
    if(mComponentState==OMX_StateLoaded)
        {
        OMX_Deinit();
        }

    LOG_FUNCTION_NAME_EXIT
}

extern "C" CameraAdapter* CameraAdapter_Factory() {

    OMXCameraAdapter *ca;

    LOG_FUNCTION_NAME

    ca = new OMXCameraAdapter();


    LOG_FUNCTION_NAME_EXIT
    return ca;
}




};


/*--------------------Camera Adapter Class ENDS here-----------------------------*/


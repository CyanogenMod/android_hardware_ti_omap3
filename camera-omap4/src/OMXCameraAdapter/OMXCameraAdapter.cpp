#include "CameraHal.h"
#include "OMXCameraAdapter.h"
#include "ErrorUtils.h"



#define HERE(Msg) {CAMHAL_LOGEB("--===line %d, %s===--\n", __LINE__, Msg);}

namespace android {

///Maintain a separate tag for OMXCameraAdapter logs to isolate issues OMX specific
#undef LOG_TAG
#define LOG_TAG "OMXCameraAdapter"

//frames skipped before recalculating the framerate
#define FPS_PERIOD 30

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
    //currently not supported use preview port instead
    mCameraAdapterParameters.mVideoPortIndex = OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW;
 //OMX_CAMERA_PORT_VIDEO_OUT_VIDEO;

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

    mPreviewing = false;
    mCapturing = false;
    mRecording = false;
    mFlushBuffers = false;
    mWaitingForSnapshot = false;
    mComponentState = OMX_StateLoaded;
    mCapMode = HIGH_QUALITY;
    mBurstFrames = 1;
    mCapturedFrames = 0;
    mPictureQuality = 100;

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
    status_t res = NO_ERROR;
    int refCount = -1;
    OMXCameraPortParameters *port = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if ( mComponentState != OMX_StateExecuting )
        {
        return;
        }

    if ( CameraFrame::IMAGE_FRAME == frameType )
        {
        port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];
        }
    else
        {
        port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
        }

    if ( NULL == frameBuf )
        {
        CAMHAL_LOGEA("Invalid frameBuf");
        res = -EINVAL;
        }

    if ( NO_ERROR == res)
        {

        if ( CameraFrame::VIDEO_FRAME_SYNC == frameType )
            {

                {
                //Update refCount accordingly
                Mutex::Autolock lock(mVideoBufferLock);
                refCount = mVideoBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                //CAMHAL_LOGDB("Video Frame 0x%x returned refCount %d -> %d", frameBuf, refCount, refCount-1);

                if ( 0 >= refCount )
                    {
                    CAMHAL_LOGEB("Error trying to decrement refCount %d for buffer 0x%x", (uint32_t)refCount, (uint32_t)frameBuf);
                    return;
                    }

                refCount--;
                mVideoBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);
                }

            if ( mRecording )
                {
                //Query preview subscribers for this buffer
                Mutex::Autolock lock(mVideoBufferLock);
                refCount += mPreviewBuffersAvailable.valueFor( ( unsigned int ) frameBuf);
                }

            port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

            }
        else if ( CameraFrame::PREVIEW_FRAME_SYNC == frameType )
            {

                {
                //Update refCoung accordingly
                Mutex::Autolock lock(mPreviewBufferLock);
                refCount = mPreviewBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                //CAMHAL_LOGDB("Preview Frame 0x%x returned refCount %d -> %d", frameBuf, refCount, refCount-1);

                if ( 0 >= refCount )
                    {
                    CAMHAL_LOGEB("Error trying to decrement refCount %d for buffer 0x%x", refCount, (unsigned int)frameBuf);
                    return;
                    }
                refCount--;
                mPreviewBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);
                }

            if ( mRecording )
                {
                //Query video subscribers for this buffer
                Mutex::Autolock lock(mVideoBufferLock);
                refCount += mVideoBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                }

            port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
            }
        else
            {
            refCount = 0;
            }

        //check if someone is holding this buffer
        if ( 0 == refCount )
            {
            for ( int i = 0 ; i < port->mNumBufs ; i++)
                {

                if ( port->mBufferHeader[i]->pBuffer == frameBuf )
                    {
                    eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp, port->mBufferHeader[i]);
                    if ( eError != OMX_ErrorNone )
                        {
                        CAMHAL_LOGEB("OMX_FillThisBuffer %d", eError);
                        return;
                        }
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
            CAMHAL_LOGDA("CbYCrY format selected");
            pixFormat = OMX_COLOR_FormatCbYCrY;
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
            CAMHAL_LOGDA("Invalid format, CbYCrY format selected as default");
            pixFormat = OMX_COLOR_FormatCbYCrY;
            }
        }
    else
        {
        CAMHAL_LOGEA("Preview format is NULL, defaulting to CbYCrY");
        pixFormat = OMX_COLOR_FormatCbYCrY;
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

    CAMHAL_LOGDB("Prev: cap.mColorFormat = %d", (int)cap->mColorFormat);
    CAMHAL_LOGDB("Prev: cap.mWidth = %d", (int)cap->mWidth);
    CAMHAL_LOGDB("Prev: cap.mHeight = %d", (int)cap->mHeight);
    CAMHAL_LOGDB("Prev: cap.mFrameRate = %d", (int)cap->mFrameRate);

    //TODO: Add an additional parameter for video resolution
   //use preview resolution for now
    cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
    cap->mColorFormat = pixFormat;
    cap->mWidth = w;
    cap->mHeight = h;
    cap->mFrameRate = frameRate;

    CAMHAL_LOGDB("Video: cap.mColorFormat = %d", (int)cap->mColorFormat);
    CAMHAL_LOGDB("Video: cap.mWidth = %d", (int)cap->mWidth);
    CAMHAL_LOGDB("Video: cap.mHeight = %d", (int)cap->mHeight);
    CAMHAL_LOGDB("Video: cap.mFrameRate = %d", (int)cap->mFrameRate);


    ///mStride is set from setBufs() while passing the APIs
    cap->mStride = 4096;
    cap->mBufSize = cap->mStride * cap->mHeight;

    cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    params.getPictureSize(&w, &h);
    cap->mWidth = w;
    cap->mHeight = h;
    //TODO: Support more pixelformats
    cap->mStride = 2;
    cap->mBufSize = cap->mStride * cap->mHeight;
    cap->mNumBufs = mCaptureBuffersCount;

    CAMHAL_LOGDB("Image: cap.mWidth = %d", (int)cap->mWidth);
    CAMHAL_LOGDB("Image: cap.mHeight = %d", (int)cap->mHeight);

    if ( params.getInt(KEY_ROTATION) != -1 )
        {
        mPictureRotation = params.getInt(KEY_ROTATION);
        }
    else
        {
        mPictureRotation = 0;
        }

    CAMHAL_LOGDB("Picture Rotation set %d", mPictureRotation);

    if ( params.getInt(KEY_CAP_MODE) != -1 )
        {
        mCapMode = ( OMXCameraAdapter::CaptureMode ) params.getInt(KEY_CAP_MODE);
        }
    else
        {
        mCapMode = OMXCameraAdapter::HIGH_QUALITY;
        }

    CAMHAL_LOGDB("Capture Mode set %d", mCapMode);

    if ( params.getInt(KEY_BURST)  >= 1 )
        {
        mBurstFrames = params.getInt(KEY_BURST);

        //always use high speed when doing burst
        mCapMode = OMXCameraAdapter::HIGH_SPEED;
        }
    else
        {
        mBurstFrames = 1;
        }

    CAMHAL_LOGDB("Burst Frames set %d", mBurstFrames);

    if ( ( params.getInt(CameraParameters::KEY_JPEG_QUALITY)  >= MIN_JPEG_QUALITY ) &&
         ( params.getInt(CameraParameters::KEY_JPEG_QUALITY)  <= MAX_JPEG_QUALITY ) )
        {
        mPictureQuality = params.getInt(CameraParameters::KEY_JPEG_QUALITY);
        }
    else
        {
        mPictureQuality = MAX_JPEG_QUALITY;
        }

    CAMHAL_LOGDB("Picture Quality set %d", mPictureQuality);

    if ( params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH)  > 0 )
        {
        mThumbWidth = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
        }
    else
        {
        mThumbWidth = DEFAULT_THUMB_WIDTH;
        }

    CAMHAL_LOGDB("Picture Thumb width set %d", mThumbWidth);

    if ( params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT)  > 0 )
        {
        mThumbHeight = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
        }
    else
        {
        mThumbHeight = DEFAULT_THUMB_HEIGHT;
        }

    CAMHAL_LOGDB("Picture Thumb height set %d", mThumbHeight);

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t OMXCameraAdapter::setFormat(OMX_U32 port, OMXCameraPortParameters &portParams)
{
    size_t bufferCount;

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
        portCheck.format.image.nStride          = portParams.mStride * portParams.mWidth;
        portCheck.nBufferSize                   =  portParams.mStride * portParams.mWidth * portParams.mHeight;
        portCheck.nBufferCountActual = CameraHal::NO_BUFFERS_IMAGE_CAPTURE;
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

    if ( OMX_CAMERA_PORT_IMAGE_OUT_IMAGE == port )
        {
        LOGD("\n *** IMG Width = %ld", portCheck.format.image.nFrameWidth);
        LOGD("\n ***IMG Height = %ld", portCheck.format.image.nFrameHeight);

        LOGD("\n ***IMG IMG FMT = %x", portCheck.format.image.eColorFormat);
        LOGD("\n ***IMG portCheck.nBufferSize = %ld\n",portCheck.nBufferSize);
        LOGD("\n ***IMG portCheck.nBufferCountMin = %ld\n",
                                                portCheck.nBufferCountMin);
        LOGD("\n ***IMG portCheck.nBufferCountActual = %ld\n",
                                                portCheck.nBufferCountActual);
        LOGD("\n ***IMG portCheck.format.image.nStride = %ld\n",
                                                portCheck.format.image.nStride);

        }
    else
        {
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
        }

    LOG_FUNCTION_NAME_EXIT
    return ErrorUtils::omxToAndroidError(eError);

    ///If there is any failure, we reach here.
    ///Here, we do any resource freeing and convert from OMX error code to Camera Hal error code
    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of eError=%x", __FUNCTION__, eError);
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
            ret = UseBuffersPreview(bufArr, num);
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

    ret = setCaptureMode(mCapMode);
    if ( NO_ERROR != ret )
        {
        CAMHAL_LOGEB("setCaptureMode() failed %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    ret = setFormat(OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW, *mPreviewData);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEB("setFormat() failed %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    ret = setImageQuality(mPictureQuality);
    if ( NO_ERROR != ret)
        {
        CAMHAL_LOGEB("Error configuring image quality %x", ret);
        return ret;
        }

    ret = setThumbnailSize(mThumbWidth, mThumbHeight);
    if ( NO_ERROR != ret)
        {
        CAMHAL_LOGEB("Error configuring thumbnail size %x", ret);
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

    imgCaptureData->mNumBufs = num;

    //TODO: Support more pixelformats

    LOGE("Params Width = %d", (int)imgCaptureData->mWidth);
    LOGE("Params Height = %d", (int)imgCaptureData->mWidth);

    ret = setFormat(OMX_CAMERA_PORT_IMAGE_OUT_IMAGE, *imgCaptureData);
    if ( ret != NO_ERROR )
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

    for ( int index = 0 ; index < imgCaptureData->mNumBufs ; index++ )
    {
        OMX_BUFFERHEADERTYPE *pBufferHdr;
        CAMHAL_LOGDB("OMX_UseBuffer Capture address: 0x%x, size = %d", (unsigned int)buffers[index], (int)imgCaptureData->mBufSize);

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

    mCapturedFrames = mBurstFrames;
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
                        for ( uint32_t i = 0 ; i < desc->mCount ; i++ )
                            {
                            mPreviewBuffersAvailable.add(mPreviewBuffers[i], 0);
                            }
                        }
                    }
                else if( CameraAdapter::CAMERA_VIDEO == mode )
                    {
                    if ( NULL == desc )
                        {
                        CAMHAL_LOGEA("Invalid video buffers!");
                        ret = -1;
                        }
                    if ( ret == NO_ERROR )
                        {
                        Mutex::Autolock lock(mVideoBufferLock);
                        mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex].mNumBufs = desc->mCount;
                        mVideoBuffers = (int *) desc->mBuffers;
                        mVideoBuffersLength = desc->mLength;
                        mVideoBuffersAvailable.clear();
                        mPreviewBuffersAvailable.clear();
                        for ( uint32_t i = 0 ; i < desc->mCount ; i++ )
                            {
                            mVideoBuffersAvailable.add(mVideoBuffers[i], 0);
                            mPreviewBuffersAvailable.add(mVideoBuffers[i], 0);
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
                        CAMHAL_LOGEB("mCaputreBuffersLength %d", mCaptureBuffersLength);
                        mCaptureBuffersAvailable.clear();
                        for ( uint32_t i = 0 ; i < desc->mCount ; i++ )
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
        case CameraAdapter::CAMERA_START_VIDEO:
            {
            CAMHAL_LOGDA("Start video recording");
            startVideoCapture();
            break;
            }
        case CameraAdapter::CAMERA_STOP_VIDEO:
            {
            CAMHAL_LOGDA("Stop video recording");
            stopVideoCapture();
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
        Mutex::Autolock lock(mLock);
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
        CAMHAL_LOGDB("Queuing buffer on Preview port - 0x%x", (uint32_t)mPreviewData->mBufferHeader[index]->pBuffer);
        eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                    (OMX_BUFFERHEADERTYPE*)mPreviewData->mBufferHeader[index]);
        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_FillThisBuffer- %x", eError);
            }
        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
        }

    mComponentState = OMX_StateExecuting;

    //reset frame rate estimates
    mFPS = 0.0f;
    mLastFPS = 0.0f;

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

    CAMHAL_LOGEB("Average framerate: %f", mFPS);

    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    if ( mComponentState != OMX_StateExecuting )
        {
        CAMHAL_LOGEA("Calling StopPreview() when not in EXECUTING state");
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
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

    CAMHAL_LOGDA("EXECUTING->IDLE state changed");

    eventSem.Wait();

    CAMHAL_LOGDA("EXECUTING->IDLE state changed");

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
    CAMHAL_LOGDA("IDLE->LOADED state changed");
    eventSem.Wait();
    CAMHAL_LOGDA("IDLE->LOADED state changed");


    mComponentState = OMX_StateLoaded;

        {
        Mutex::Autolock lock(mPreviewBufferLock);
        ///Clear all the available preview buffers
        mPreviewBuffersAvailable.clear();
        }

    ///Clear the previewing flag, we are no longer previewing
    mPreviewing = false;

    LOG_FUNCTION_NAME_EXIT

    return (ret | ErrorUtils::omxToAndroidError(eError));

    EXIT:
        CAMHAL_LOGEB("Exiting function because of eError= %x", eError);
        LOG_FUNCTION_NAME_EXIT
        return (ret | ErrorUtils::omxToAndroidError(eError));

}

status_t OMXCameraAdapter::setThumbnailSize(unsigned int width, unsigned int height)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_THUMBNAILTYPE thumbConf;

    LOG_FUNCTION_NAME

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in loaded state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT(thumbConf, OMX_PARAM_THUMBNAILTYPE);
        thumbConf.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        eError = OMX_GetParameter(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexParamThumbnail, &thumbConf);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while retrieving thumbnail size 0x%x", eError);
            ret = -1;
            }

        thumbConf.nWidth = width;
        thumbConf.nHeight = height;

        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexParamThumbnail, &thumbConf);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring thumbnail size 0x%x", eError);
            ret = -1;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setImageQuality(unsigned int quality)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_PARAM_QFACTORTYPE jpegQualityConf;

    LOG_FUNCTION_NAME

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in loaded state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT(jpegQualityConf, OMX_IMAGE_PARAM_QFACTORTYPE);
        jpegQualityConf.nQFactor = quality;
        jpegQualityConf.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp, OMX_IndexParamQFactor, &jpegQualityConf);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring jpeg Quality 0x%x", eError);
            ret = -1;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
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

status_t OMXCameraAdapter::setCaptureMode(OMXCameraAdapter::CaptureMode mode)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_CAMOPERATINGMODETYPE camMode;


    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {

        OMX_INIT_STRUCT_PTR (&camMode, OMX_CONFIG_CAMOPERATINGMODETYPE);

        if ( OMXCameraAdapter::HIGH_SPEED == mode )
            {
            CAMHAL_LOGDA("Camera mode: HIGH SPEED");
            camMode.eCamOperatingMode = OMX_CaptureImageHighSpeedTemporalBracketing;
            }
        else
            {
            CAMHAL_LOGDA("Camera mode: HIGH QUALITY");
            camMode.eCamOperatingMode = OMX_CaptureImageProfileBase;
            }

        eError =  OMX_SetParameter(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexCameraOperatingMode, &camMode);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring camera mode 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDA("Camera mode configured successfully");
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
        OMX_INIT_STRUCT_PTR (&focusControl, OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE);
        focusControl.eFocusControl = OMX_IMAGE_FocusControlAutoLock;

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigFocusControl, &focusControl);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while starting focus 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDA("Autofocus started successfully");
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
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMXCameraPortParameters * capData = NULL;
    OMX_CONFIG_BOOLEANTYPE bOMX;

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        ret = setPictureRotation(mPictureRotation);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error configuring image rotation %x", ret);
            }
        }

    if ( NO_ERROR == ret )
        {
        capData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

        ///Queue all the buffers on capture port
        for ( int index = 0 ; index < capData->mNumBufs ; index++ )
            {
            CAMHAL_LOGDB("Queuing buffer on Capture port - 0x%x", capData->mBufferHeader[index]->pBuffer);
            eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                        (OMX_BUFFERHEADERTYPE*)capData->mBufferHeader[index]);

            GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
            }

        OMX_INIT_STRUCT_PTR (&bOMX, OMX_CONFIG_BOOLEANTYPE);
        bOMX.bEnabled = OMX_TRUE;

        /// sending Capturing Command to the component
        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCapturing, &bOMX);

        CAMHAL_LOGDB("Capture set - 0x%x", eError);

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

        mCapturing = true;
        mWaitingForSnapshot = true;
        }

    EXIT:

    return ret;
}

status_t OMXCameraAdapter::stopImageCapture()
{
    status_t ret = NO_ERROR;
    OMXCameraPortParameters * capData = NULL;
    OMX_ERRORTYPE eError;
    OMX_CONFIG_BOOLEANTYPE bOMX;

    LOG_FUNCTION_NAME

    mWaitingForSnapshot = false;

    capData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    ///Free all the buffers on capture port
    for ( int index = 0 ; index < capData->mNumBufs ; index++)
        {
        CAMHAL_LOGDB("Freeing buffer on Capture port - 0x%x", (uint32_t)capData->mBufferHeader[index]->pBuffer);
        eError = OMX_FreeBuffer(mCameraAdapterParameters.mHandleComp,
                        mCameraAdapterParameters.mImagePortIndex,
                        (OMX_BUFFERHEADERTYPE*)capData->mBufferHeader[index]);

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
        }

    OMX_INIT_STRUCT_PTR (&bOMX, OMX_CONFIG_BOOLEANTYPE);
    bOMX.bEnabled = OMX_FALSE;

    eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCapturing, &bOMX);

    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGDB("Error during SetConfig- 0x%x", eError);
        ret = -1;
        }

    CAMHAL_LOGDB("Capture set - 0x%x", eError);

    LOG_FUNCTION_NAME_EXIT

    EXIT:

    return ret;
}

status_t OMXCameraAdapter::startVideoCapture()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    Semaphore eventSem;
    OMXCameraPortParameters * videoData = NULL;

    LOG_FUNCTION_NAME

    ret = eventSem.Create(0);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        }

    if ( mComponentState != OMX_StateIdle )
        {
        CAMHAL_LOGEA("Calling startVideoCapture() when not in IDLE state");

        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        videoData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                    OMX_EventCmdComplete,
                                    OMX_CommandStateSet,
                                    OMX_StateExecuting,
                                    eventSem,
                                    -1 ///Infinite timeout
                                    );

        if ( ret != NO_ERROR )
            {
            CAMHAL_LOGEB("Error in registering for event %d", ret);
            }
        }

    if ( NO_ERROR == ret )
        {
        eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                    OMX_CommandStateSet,
                                    OMX_StateExecuting,
                                    NULL);
        if ( eError != OMX_ErrorNone )
            {
            CAMHAL_LOGEB("OMX_SendCommand(OMX_StateExecuting)- %x", eError);
            ret = -1;
            }
        }

    if ( NO_ERROR == ret )
        {

        CAMHAL_LOGDA("Before executing state");

        eventSem.Wait();

        CAMHAL_LOGDA("After executing state!");

        for ( int index=0 ; index < videoData->mNumBufs ; index++ )
            {
            CAMHAL_LOGDB("Queuing buffer on Preview port - 0x%x",(uint32_t) videoData->mBufferHeader[index]->pBuffer);
            eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                        (OMX_BUFFERHEADERTYPE*) videoData->mBufferHeader[index]);

            if( eError != OMX_ErrorNone )
                {
                CAMHAL_LOGEB("OMX_FillThisBuffer- %x", eError);
                }

            GOTO_EXIT_IF(( eError != OMX_ErrorNone ), eError);

            }

        mRecording = true;

        mComponentState = OMX_StateExecuting;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);

     LOG_FUNCTION_NAME_EXIT

      return (ret | ErrorUtils::omxToAndroidError(eError));

}

status_t OMXCameraAdapter::stopVideoCapture()
{

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    status_t ret = NO_ERROR;
    OMXCameraPortParameters  *videoData = NULL;
    Mutex::Autolock lock(mPreviewBufferLock);
    Semaphore eventSem;

    LOG_FUNCTION_NAME

    videoData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

    ret = eventSem.Create(0);
    if ( NO_ERROR != ret )
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        }

    if ( mComponentState != OMX_StateExecuting )
        {
        CAMHAL_LOGEA("Calling stopVideoCapture() when not in executing state");

        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                    OMX_EventCmdComplete,
                                    OMX_CommandStateSet,
                                    OMX_StateIdle,
                                    eventSem,
                                    -1 ///Infinite timeout
                                    );

        if ( ret != NO_ERROR )
            {
            CAMHAL_LOGEB("Error in registering for event %d", ret);
            }
        }


    if ( NO_ERROR == ret )
        {

        eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp,
                                    OMX_CommandStateSet, OMX_StateIdle, NULL);

        if ( eError != OMX_ErrorNone )
            {
            CAMHAL_LOGEB("OMX_SendCommand(OMX_StateIdle) - %x", eError);
            }

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

        eventSem.Wait();

       }

    if ( NO_ERROR == ret )
        {

        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                    OMX_EventCmdComplete,
                                    OMX_CommandStateSet,
                                    OMX_StateLoaded,
                                    eventSem,
                                    -1 ///Infinite timeout
                                    );

        if ( ret != NO_ERROR )
            {
            CAMHAL_LOGEB("Error in registering for event %d", ret);
            }
        }

    if ( NO_ERROR == ret )
        {

        eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp,
                                OMX_CommandStateSet, OMX_StateLoaded, NULL);

        if ( eError != OMX_ErrorNone )
            {
            CAMHAL_LOGEB("OMX_SendCommand(OMX_StateLoaded) - %x", eError);
            }

        GOTO_EXIT_IF(( eError != OMX_ErrorNone ), eError);

        for( int i = 0 ; i < videoData->mNumBufs ; i++ )
            {

            eError = OMX_FreeBuffer(mCameraAdapterParameters.mHandleComp,
                                mCameraAdapterParameters.mPrevPortIndex,
                                videoData->mBufferHeader[i]);

            if( eError != OMX_ErrorNone )
                {
                CAMHAL_LOGEB("OMX_FreeBuffer - %x", eError);
                }

            GOTO_EXIT_IF(( eError != OMX_ErrorNone ), eError);

            }

        ///Wait for the IDLE -> LOADED transition to arrive
        eventSem.Wait();

        mComponentState = OMX_StateLoaded;

        ///Clear all the available preview buffers
        mVideoBuffersAvailable.clear();

        mRecording = false;

        }

    LOG_FUNCTION_NAME_EXIT

    CAMHAL_LOGEB("Average framerate: %f", mFPS);

    return (ret | ErrorUtils::omxToAndroidError(eError));

    EXIT:

        CAMHAL_LOGEB("Exiting function because of eError= %x", eError);

        LOG_FUNCTION_NAME_EXIT

        return (ret | ErrorUtils::omxToAndroidError(eError));
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
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_RECTTYPE tFrameDim;

    LOG_FUNCTION_NAME

    OMX_INIT_STRUCT_PTR (&tFrameDim, OMX_CONFIG_RECTTYPE);
    tFrameDim.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("Calling queryBufferPreviewResolution() when not in LOADED state");
        width = -1;
        height = -1;
        goto exit;
        }

    if ( NO_ERROR == ret )
        {
        ret = setCaptureMode(mCapMode);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("setCaptureMode() failed %d", ret);
            }
        }

    if ( NO_ERROR == ret )
        {
        ret = setFormat (mCameraAdapterParameters.mPrevPortIndex, mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex]);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("setFormat() failed %d", ret);
            }
        }

    if ( NO_ERROR == ret )
        {
        eError = OMX_GetParameter(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_TI_IndexParam2DBufferAllocDimension, &tFrameDim);
        if ( OMX_ErrorNone == eError)
            {
            width = tFrameDim.nWidth;
            height = tFrameDim.nHeight;
            }
        else
            {
            width = -1;
            height = -1;
            }
        }
    else
        {
        width = -1;
        height = -1;
        }
exit:

    CAMHAL_LOGDB("Required frame size %dx%d", width, height);

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
            CAMHAL_LOGDB("+OMX_EventCmdComplete %d %d", (int)nData1, (int)nData2);

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
            CAMHAL_LOGDB("OMX interface failed to execute OMX command %d", (int)nData1);
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
    status_t  res1, res2;
    OMXCameraPortParameters  *pPortParam;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    CameraFrame::FrameType typeOfFrame = CameraFrame::ALL_FRAMES;

    res1 = res2 = -1;
    pPortParam = &(mCameraAdapterParameters.mCameraPortParams[pBuffHeader->nOutputPortIndex]);
    if (pBuffHeader->nOutputPortIndex == OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW)
        {
        if( mWaitingForSnapshot )
            {
            typeOfFrame = CameraFrame::SNAPSHOT_FRAME;
            ret = sendFrameToSubscribers(pBuffHeader, typeOfFrame, pPortParam);
            }
        else
            {
            notifyFocusSubscribers();
            recalculateFPS();

            typeOfFrame = CameraFrame::PREVIEW_FRAME_SYNC;

                {
                Mutex::Autolock lock(mPreviewBufferLock);
                //CAMHAL_LOGDB("Preview Frame 0x%x refCount start %d", (uint32_t)pBuffHeader->pBuffer,(int) mFrameSubscribers.size());
                mPreviewBuffersAvailable.replaceValueFor(  ( unsigned int ) pBuffHeader->pBuffer, mFrameSubscribers.size());
                }

            res1 = sendFrameToSubscribers(pBuffHeader, typeOfFrame, pPortParam);

            if ( mRecording )
                {
                typeOfFrame = CameraFrame::VIDEO_FRAME_SYNC;

                    {
                    Mutex::Autolock lock(mVideoBufferLock);
                    //CAMHAL_LOGDB("Video Frame 0x%x refCount start %d", (uint32_t)pBuffHeader->pBuffer, (int)mVideoSubscribers.size());
                    mVideoBuffersAvailable.replaceValueFor( ( unsigned int ) pBuffHeader->pBuffer, mVideoSubscribers.size());
                    }

                res2  = sendFrameToSubscribers(pBuffHeader, typeOfFrame, pPortParam);
                }

            ret = ( ( NO_ERROR == res1) || ( NO_ERROR == res2 ) ) ? ( (uint32_t)NO_ERROR ) : ( -1 );

            }
        }
    else if( pBuffHeader->nOutputPortIndex == OMX_CAMERA_PORT_IMAGE_OUT_IMAGE )
        {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

        CameraHal::PPM("Shot to Jpeg: ", &mStartCapture);

#endif

        typeOfFrame = CameraFrame::IMAGE_FRAME;

        ret = sendFrameToSubscribers(pBuffHeader, typeOfFrame, pPortParam);

        CAMHAL_LOGDB("Captured Frames: %d", mCapturedFrames);

        mCapturedFrames--;

        if ( 0 == mCapturedFrames )
            {
            stopImageCapture();
            }
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

        returnFrame(pBuffHeader->pBuffer, typeOfFrame);

        }
    return eError;

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);
    return eError;
}

status_t OMXCameraAdapter::recalculateFPS()
{
    static int frameCount = 0;
    static unsigned int iter = 1;
    static int lastFrameCount = 0;
    static nsecs_t lastFPSTime = 0;
    float currentFPS;

    frameCount++;

    if ( ( frameCount % FPS_PERIOD ) == 0 )
        {
        nsecs_t now = systemTime();
        nsecs_t diff = now - lastFPSTime;
        currentFPS =  ((frameCount - lastFrameCount) * float(s2ns(1))) / diff;
        lastFPSTime = now;
        lastFrameCount = frameCount;

        if ( 1 == iter )
            {
            mFPS = currentFPS;
            }
        else
            {
            //cumulative moving average
            mFPS = mLastFPS + (currentFPS - mLastFPS)/iter;
            }

        mLastFPS = mFPS;
        iter++;
        }

    return NO_ERROR;
}


status_t OMXCameraAdapter::sendFrameToSubscribers(OMX_IN OMX_BUFFERHEADERTYPE *pBuffHeader, int typeOfFrame, OMXCameraPortParameters *port)
{
    status_t ret = NO_ERROR;
    int refCount;

    frame_callback callback;
    CameraFrame cFrame;

//    LOG_FUNCTION_NAME

    if ( NULL == port)
        {
        CAMHAL_LOGEA("Invalid portParam");
        ret = -EINVAL;
        }

    if ( NULL == pBuffHeader )
        {
        CAMHAL_LOGEA("Invalid Buffer header");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        cFrame.mFrameType = typeOfFrame;
        cFrame.mBuffer = pBuffHeader->pBuffer;
        cFrame.mLength = pBuffHeader->nFilledLen;
        cFrame.mAlignment =port->mStride;
        cFrame.mTimestamp = pBuffHeader->nTimeStamp;
        cFrame.mOffset = pBuffHeader->nOffset;

        if(CameraFrame::IMAGE_FRAME == typeOfFrame )
            {
            for (uint32_t i = 0 ; i < mImageSubscribers.size(); i++ )
                {
                cFrame.mCookie = (void *) mImageSubscribers.keyAt(i);
                callback = (frame_callback) mImageSubscribers.valueAt(i);
                callback(&cFrame);
                }
            }
        else if ( CameraFrame::VIDEO_FRAME_SYNC == typeOfFrame )
            {
            if ( 0 < mVideoSubscribers.size() )
                {

                for(uint32_t i = 0 ; i < mVideoSubscribers.size(); i++ )
                    {
                    cFrame.mCookie = (void *) mVideoSubscribers.keyAt(i);
                    callback = (frame_callback) mVideoSubscribers.valueAt(i);
                    callback(&cFrame);

                    }
                }
            else
                {
                ret = -EINVAL;
                }
            }
        else if ( ( CameraFrame::PREVIEW_FRAME_SYNC == typeOfFrame ) || ( CameraFrame::SNAPSHOT_FRAME == typeOfFrame ) )
            {
            if ( 0 < mFrameSubscribers.size() )
                {
                for(uint32_t i = 0 ; i < mFrameSubscribers.size(); i++ )
                    {
                    cFrame.mCookie = (void *) mFrameSubscribers.keyAt(i);
                    callback = (frame_callback) mFrameSubscribers.valueAt(i);
                    callback(&cFrame);
                    }
                }
            else
                {
                ret = -EINVAL;
                }
            }
        else
            {
            ret = -EINVAL;
            }
        }

//    LOG_FUNCTION_NAME_EXIT

    return ret;
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


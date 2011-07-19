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


#define LOG_TAG "CameraHal"

#include "FakeCameraAdapter.h"

namespace android {

#define DEFAULT_PICTURE_BUFFER_SIZE 0x1000

/*--------------------Camera Adapter Class STARTS here-----------------------------*/

FakeCameraAdapter::FakeCameraAdapter()
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT
}

FakeCameraAdapter::~FakeCameraAdapter()
{
    Message msg;

    LOG_FUNCTION_NAME

    msg.command = BaseCameraAdapter::FRAME_EXIT;
    mFrameQ.put(&msg);
    mAdapterQ.get(&msg);

    msg.command = FakeCameraAdapter::CALLBACK_EXIT;
    mCallbackQ.put(&msg);

    mCallbackThread->requestExitAndWait();
    mFrameThread->requestExitAndWait();

    mCallbackThread.clear();
    mFrameThread.clear();;

    LOG_FUNCTION_NAME_EXIT
}

int FakeCameraAdapter::setErrorHandler(ErrorNotifier *errorNotifier)
{
    LOG_FUNCTION_NAME
    return 0;
}

status_t FakeCameraAdapter::getCaps(CameraParameters &params)
{
    LOG_FUNCTION_NAME
    return NO_ERROR;
}

int FakeCameraAdapter::getRevision()
{
    LOG_FUNCTION_NAME


    LOG_FUNCTION_NAME_EXIT

    return 0;
}

status_t FakeCameraAdapter::initialize(int sensor_index)
{
    LOG_FUNCTION_NAME

    //Create the frame thread
    mFrameThread = new FramePreview(this);
    if ( NULL == mFrameThread.get() )
        {
        CAMHAL_LOGEA("Couldn't create frame thread");
        return NO_MEMORY;
        }

    //Start the frame thread
    status_t ret = mFrameThread->run("FrameThread", PRIORITY_URGENT_DISPLAY);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEA("Couldn't run frame thread");
        return ret;
        }

    //Create the callback thread
    mCallbackThread = new FrameCallback(this);
    if ( NULL == mCallbackThread.get() )
        {
        CAMHAL_LOGEA("Couldn't create callback thread");
        return NO_MEMORY;
        }

    //Start the callbackthread
    ret = mCallbackThread->run("CallbackThread", PRIORITY_URGENT_DISPLAY);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEA("Couldn't run callback thread");
        return ret;
        }

    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR;
}


status_t FakeCameraAdapter::setParameters(const CameraParameters& params)
{
    LOG_FUNCTION_NAME

    params.getPreviewSize(&mPreviewWidth, &mPreviewHeight);
    params.getPictureSize(&mCaptureWidth, &mCaptureHeight);

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

void FakeCameraAdapter::getParameters(CameraParameters& params)
{
    LOG_FUNCTION_NAME
    LOG_FUNCTION_NAME_EXIT
}

status_t FakeCameraAdapter::takePicture()
{
    status_t ret = NO_ERROR;
    Message msg;

    LOG_FUNCTION_NAME

    msg.command = BaseCameraAdapter::TAKE_PICTURE;
    mFrameQ.put(&msg);
    MessageQueue::waitForMsg(&mAdapterQ, NULL, NULL, -1);
    mAdapterQ.get(&msg);

    if ( FakeCameraAdapter::ERROR == msg.command )
        {
        ret = -1;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t FakeCameraAdapter::startPreview()
{
    status_t ret = NO_ERROR;
    Message msg;

    LOG_FUNCTION_NAME

    msg.command = BaseCameraAdapter::START_PREVIEW;

    mFrameQ.put(&msg);
    MessageQueue::waitForMsg(&mAdapterQ, NULL, NULL, -1);
    mAdapterQ.get(&msg);

    if ( FakeCameraAdapter::ERROR == msg.command )
        {
        ret = -1;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t FakeCameraAdapter::stopPreview()
{
    status_t ret = NO_ERROR;
    Message msg;

    LOG_FUNCTION_NAME

    msg.command = BaseCameraAdapter::STOP_PREVIEW;

    mFrameQ.put(&msg);
    MessageQueue::waitForMsg(&mAdapterQ, NULL, NULL, -1);
    mAdapterQ.get(&msg);

    if ( FakeCameraAdapter::ERROR == msg.command )
        {
        ret = -1;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t FakeCameraAdapter::autoFocus()
{
    status_t ret = NO_ERROR;
    Message msg;

    LOG_FUNCTION_NAME

    msg.command = BaseCameraAdapter::DO_AUTOFOCUS;
    mFrameQ.put(&msg);
    MessageQueue::waitForMsg(&mAdapterQ, NULL, NULL, -1);
    mAdapterQ.get(&msg);

    if ( FakeCameraAdapter::ERROR == msg.command )
        {
        ret = -1;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t FakeCameraAdapter::cancelAutoFocus()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

//API to get the frame size required to be allocated.
void FakeCameraAdapter::getFrameSize(int &width, int &height)
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

}

status_t FakeCameraAdapter::getFrameDataSize(size_t &dataFrameSize, size_t bufferCount)
{
    LOG_FUNCTION_NAME

    dataFrameSize = DEFAULT_PICTURE_BUFFER_SIZE;

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}


status_t FakeCameraAdapter::getPictureBufferSize(size_t &length, size_t bufferCount)
{
    LOG_FUNCTION_NAME

    length = DEFAULT_PICTURE_BUFFER_SIZE;

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

status_t FakeCameraAdapter::useBuffers(CameraMode mode, void* bufArr, int num, size_t length)
{
    status_t ret = NO_ERROR;
    uint32_t *buffers = (uint32_t*)bufArr;

    LOG_FUNCTION_NAME

    switch(mode)
        {
        case CAMERA_PREVIEW:
            {
            Mutex::Autolock lock(mPreviewVectorLock);

            mFreePreviewBuffers.clear();
            for ( int i = 0 ; i < num ; i++ )
                {
                mFreePreviewBuffers.push(buffers[i]);
                }
            break;
            }
        case CAMERA_IMAGE_CAPTURE:
            {
            Mutex::Autolock lock(mImageVectorLock);

            mFreeImageBuffers.clear();
            for ( int i = 0 ; i < num ; i++ )
                {
                mFreeImageBuffers.push(buffers[i]);
                }
            break;
            }
        case CAMERA_VIDEO:
            {
            Mutex::Autolock lock(mVideoVectorLock);

            mFreeVideoBuffers.clear();
            for ( int i = 0 ; i < num ; i++ )
                {
                mFreeVideoBuffers.push(buffers[i]);
                }
            break;
            }
        case CAMERA_MEASUREMENT:
            {
            Mutex::Autolock lock(mPreviewDataVectorLock);

            mFreePreviewDataBuffers.clear();
            for ( int i = 0 ; i < num ; i++ )
                {
                mFreePreviewDataBuffers.push(buffers[i]);
                }
            break;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

void FakeCameraAdapter::frameCallbackThread()
{
    bool shouldLive = true;
    Message msg;
    CameraFrame *frame;

    LOG_FUNCTION_NAME

    while ( shouldLive )
        {
        MessageQueue::waitForMsg(&mCallbackQ, NULL, NULL, -1);
        mCallbackQ.get(&msg);

        if ( FakeCameraAdapter::CALL_CALLBACK == msg.command )
            {
            frame = ( CameraFrame * ) msg.arg1;
            if ( NULL != frame )
                {
                resetFrameRefCount(*frame);
                sendFrameToSubscribers(frame);
                }
            }
        else if ( FakeCameraAdapter::CALLBACK_EXIT == msg.command  )
            {
            shouldLive = false;
            }
        }

    LOG_FUNCTION_NAME_EXIT
}

/**
   @brief

   Fills a given overlay buffer with a solid color depending on their index
   and type
   Only YUV422I supported for now
   TODO: Add additional pixelformat support

   @param previewBuffer - pointer to the preview buffer
   @param index - index of the overlay buffer
   @param width - width of the buffer
   @param height - height of the buffer
   @param pixelformat - pixelFormat of the buffer
   @param frame - type of the frame (normal for preview, snapshot for capture)

   @return none

 */
void FakeCameraAdapter::setBuffer(void *previewBuffer, int index, int width, int height, int pixelFormat, PreviewFrameType frame)
{
    unsigned int alignedRow;
    unsigned char *buffer;
    uint16_t data;

    buffer = ( unsigned char * ) previewBuffer;
    //rows are page aligned
    alignedRow = ( width * 2 + ( PAGE_SIZE -1 ) ) & ( ~ ( PAGE_SIZE -1 ) );

    if ( SNAPSHOT_FRAME == frame )
        {
        data = 0x0;
        }
    else
        {
        if ( 2 <= index )
            data = 0x00C8; //Two alternating colors depending on the buffer indexes
        else
            data = 0x0;
        }

    //iterate through each row
    for ( int i = 0 ; i < height ; i++,  buffer += alignedRow)
        memset(buffer, data, sizeof(data)*width);
}

 void FakeCameraAdapter::sendNextFrame(PreviewFrameType frameType)
{
    void *previewBuffer = NULL;
    CameraFrame frame;
    Message msg;
    size_t count;
    int i = 0;

        {
        Mutex::Autolock lock(mPreviewVectorLock);
        //check for any buffers available
        if ( !mFreePreviewBuffers.isEmpty() )
            {
            previewBuffer = ( void * ) mFreePreviewBuffers.top();
            }
        }

    if ( NULL == previewBuffer )
        return;

    //TODO: add pixelformat
    setBuffer(previewBuffer, i, mPreviewWidth, mPreviewHeight, 0, frameType);

    frame.mBuffer = previewBuffer;
    frame.mAlignment = PAGE_SIZE;
    frame.mWidth = mPreviewWidth;
    frame.mHeight = mPreviewHeight;
    frame.mLength = PAGE_SIZE*mPreviewHeight;
    frame.mOffset = 0;
    frame.mTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);
    frame.mFrameType = CameraFrame::PREVIEW_FRAME_SYNC;

    msg.command = FakeCameraAdapter::CALL_CALLBACK;
    msg.arg1 = ( void * ) &frame;

    mCallbackQ.put(&msg);

    frame.mFrameType = CameraFrame::VIDEO_FRAME_SYNC;

    mCallbackQ.put(&msg);
}

status_t FakeCameraAdapter::startImageCapture()
{
    status_t ret = NO_ERROR;
    CameraHalEvent shutterEvent;
    event_callback eventCb;
    Message msg;
    CameraFrame frame;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mImageVectorLock);

    if ( mFreeImageBuffers.isEmpty() )
        {
        return -1;
        }

    void *imageBuf = ( void * ) mFreeImageBuffers.top();

    notifyShutterSubscribers();

    //simulate Snapshot
    sendNextFrame(SNAPSHOT_FRAME);

    frame.mBuffer = imageBuf;
    frame.mLength = mCaptureWidth*mCaptureHeight;
    frame.mFrameType = CameraFrame::RAW_FRAME;

    msg.command = FakeCameraAdapter::CALL_CALLBACK;
    msg.arg1 = ( void * ) &frame;

    //RAW Capture
    mCallbackQ.put(&msg);

    frame.mFrameType = CameraFrame::IMAGE_FRAME;

    //Jpeg encoding done
    mCallbackQ.put(&msg);

    //Release image buffers
    if ( NULL != mReleaseImageBuffersCallback )
        {
        mReleaseImageBuffersCallback(mReleaseData);
        }

    //Signal end of image capture
    if ( NULL != mEndImageCaptureCallback)
        {
        mEndImageCaptureCallback(mEndCaptureData);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t FakeCameraAdapter::doAutofocus()
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return notifyFocusSubscribers(false);
}

void FakeCameraAdapter::frameThread()
{
    bool shouldLive = true;
    int state = FakeCameraAdapter::STOPPED;
    Message msg;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    while ( shouldLive )
        {
        switch ( state )
            {
            case FakeCameraAdapter::RUNNING:

                if ( mFrameQ.isEmpty() )
                    {
                    sendNextFrame(NORMAL_FRAME);
                    }
                else
                    {
                    if ( mFrameQ.get( &msg ) < 0 )
                        {
                        CAMHAL_LOGDA("Error while retrieving frameThread message!");
                        shouldLive = false;
                        }
                    else
                        {
                        if ( BaseCameraAdapter::STOP_PREVIEW == msg.command )
                            {
                            state = FakeCameraAdapter::STOPPED;
                            }
                        else if ( BaseCameraAdapter::RETURN_FRAME== msg.command )
                            {
                                {
                                Mutex::Autolock lock(mPreviewVectorLock);
                                mFreePreviewBuffers.push( ( unsigned int ) msg.arg1);
                                }
                            }
                        else if ( BaseCameraAdapter::DO_AUTOFOCUS == msg.command )
                            {
                            ret = doAutofocus();
                            }
                        else if ( BaseCameraAdapter::TAKE_PICTURE == msg.command )
                            {
                            ret = startImageCapture();
                            }
                        else if ( BaseCameraAdapter::FRAME_EXIT == msg.command )
                            {
                            shouldLive = false;
                            }
                        else
                            {
                            CAMHAL_LOGDA("Invalid frameThread command!");
                            ret = -1;
                            }

                        if ( NO_ERROR == ret )
                            {
                            msg.command = BaseCameraAdapter::ACK;
                            }
                        else
                            {
                            msg.command = BaseCameraAdapter::ERROR;
                            }

                        mAdapterQ.put(&msg);
                        }

                    }

                break;

            case FakeCameraAdapter::STOPPED:

                MessageQueue::waitForMsg(&mFrameQ, NULL, NULL, -1);

                if ( mFrameQ.get( &msg ) < 0 )
                    {
                    CAMHAL_LOGEA("Error while retrieving frameThread message!");
                    shouldLive = false;
                    }
                else
                    {
                    if ( BaseCameraAdapter::STOP_PREVIEW == msg.command )
                        {
                        CAMHAL_LOGDA("frameThread is already in stopped state!");
                        }
                    else if ( BaseCameraAdapter::START_PREVIEW == msg.command )
                        {
                        CAMHAL_LOGDA("State set to running!");
                        state = BaseCameraAdapter::RUNNING;
                        }
                    else if ( BaseCameraAdapter::RETURN_FRAME== msg.command )
                        {
                            {
                            Mutex::Autolock lock(mPreviewVectorLock);
                            mFreePreviewBuffers.push( ( unsigned int ) msg.arg1);
                            }
                        }
                    else if ( BaseCameraAdapter::DO_AUTOFOCUS == msg.command )
                        {
                        CAMHAL_LOGEA("Invalid AutoFocus command during stopped preview!");
                        }
                    else if ( BaseCameraAdapter::TAKE_PICTURE == msg.command )
                        {
                        CAMHAL_LOGEA("Invalid TakePicture command during stopped preview!");
                        }
                    else if ( BaseCameraAdapter::FRAME_EXIT == msg.command )
                        {
                        shouldLive = false;
                        }
                    else
                        {
                        CAMHAL_LOGEA("Invalid frameThread command!");
                        ret = -1;
                        }

                    if ( NO_ERROR == ret )
                        {
                        msg.command = BaseCameraAdapter::ACK;
                        }
                    else
                        {
                        msg.command = BaseCameraAdapter::ERROR;
                        }

                    mAdapterQ.put(&msg);
                    }

                break;

            default:

                CAMHAL_LOGEA("Invalid CameraAdapter frameThread state!");
                shouldLive = false;
        };
    };

    LOG_FUNCTION_NAME_EXIT
}

status_t FakeCameraAdapter::fillThisBuffer(void* frameBuf, CameraFrame::FrameType frameType)
{
    status_t ret = NO_ERROR;
    Message msg;

    LOG_FUNCTION_NAME

    if ( NULL != frameBuf )
        {
        msg.command = BaseCameraAdapter::RETURN_FRAME;
        msg.arg1 = frameBuf;
        msg.arg2 = ( void * ) frameType;

        mFrameQ.put(&msg);

        MessageQueue::waitForMsg(&mAdapterQ, NULL, NULL, -1);
        mAdapterQ.get(&msg);

        if ( BaseCameraAdapter::ERROR == msg.command )
            {
            CAMHAL_LOGEA("Error while returning preview frame!");
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

extern "C" CameraAdapter* CameraAdapter_Factory() {
    FakeCameraAdapter *ret = NULL;

    LOG_FUNCTION_NAME

    ret = new FakeCameraAdapter();

    return ret;
}

};


/*--------------------Camera Adapter Class ENDS here-----------------------------*/


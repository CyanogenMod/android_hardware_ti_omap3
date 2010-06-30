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

    mPreviewBuffersRefCount.clear();
    mFrameSubscribers.clear();

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

status_t FakeCameraAdapter::getCaps()
{
    LOG_FUNCTION_NAME
    return NO_ERROR;
}

status_t FakeCameraAdapter::initialize()
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

CameraParameters FakeCameraAdapter::getParameters() const
{
    LOG_FUNCTION_NAME
    return mParameters;
}

//API to send a command to the camera
status_t FakeCameraAdapter::sendCommand(int operation, int value1, int value2, int value3)
{
    status_t ret = NO_ERROR;
    CameraAdapter::CameraMode mode;
    BuffersDescriptor *desc;
    Message msg;

    LOG_FUNCTION_NAME

    switch ( operation )
        {
        case CameraAdapter::CAMERA_USE_BUFFERS:

            CAMHAL_LOGDA("Use Buffers");
            mode = ( CameraAdapter::CameraMode ) value1;
            desc = ( BuffersDescriptor * ) value2;

            if ( CameraAdapter::CAMERA_PREVIEW == mode )
                {
                if ( NULL == desc )
                    {
                    CAMHAL_LOGEA("Invalid buffers descriptor!");
                    ret = -1;
                    }

                if ( ret == NO_ERROR )
                    {
                    Mutex::Autolock lock(mPreviewBufferLock);
                    mPreviewBufferCount = desc->mCount;
                    mPreviewBuffers = (int *) desc->mBuffers;
                    mPreviewOffsets = desc->mOffsets;
                    mPreviewFd = desc->mFd;

                    mPreviewBuffersRefCount.clear();
                    for ( int i = 0 ; i < mPreviewBufferCount ; i++ )
                        {
                        mPreviewBuffersRefCount.add(mPreviewBuffers[i], 0);
                        }
                    }
                }
            else if ( CameraAdapter::CAMERA_IMAGE_CAPTURE == mode )
                {
                if ( NULL == desc )
                   {

                    CAMHAL_LOGEA("Invalid buffers descriptor!");
                    ret = -1;
                    break;
                   }

                mImageBufferCount = desc->mCount;
                mImageBuffers = (int *) desc->mBuffers;
                mImageOffsets = desc->mOffsets;
                mImageFd = desc->mFd;
                }
            else if ( CameraAdapter::CAMERA_VIDEO == mode )
                {
                //Don't do anything here, same buffers are
                 //used for capture as also recording
                }
            else
                {
                CAMHAL_LOGEB("Camera Mode %x still not supported!", mode);
                }

            break;

        case CameraAdapter::CAMERA_PREVIEW_FLUSH_BUFFERS:

            CAMHAL_LOGDA("Flush Buffers");

                {
                Mutex::Autolock lock(mPreviewBufferLock);

                mPreviewBuffersRefCount.clear();
                mPreviewBufferCount = 0;
                mPreviewBuffers = NULL;
                mPreviewOffsets = NULL;
                }

            break;

        //Preview and recording are equivalent in fake camea adapter
        //They use same buffers, functionality etc.
        //NOTE: Real camera adapters should implement the
        //needed functionality for this.
        case CameraAdapter::CAMERA_START_VIDEO:
        case CameraAdapter::CAMERA_STOP_VIDEO:
            break;

        case CameraAdapter::CAMERA_START_PREVIEW:

            CAMHAL_LOGDA("Start Preview");
            msg.command = BaseCameraAdapter::START_PREVIEW;

            mFrameQ.put(&msg);
            MessageQueue::waitForMsg(&mAdapterQ, NULL, NULL, -1);
            mAdapterQ.get(&msg);

            if ( FakeCameraAdapter::ERROR == msg.command )
                {
                ret = -1;
                }

            break;

        case CameraAdapter::CAMERA_STOP_PREVIEW:

            CAMHAL_LOGDA("Stop Preview");
            msg.command = BaseCameraAdapter::STOP_PREVIEW;

            mFrameQ.put(&msg);
            MessageQueue::waitForMsg(&mAdapterQ, NULL, NULL, -1);
            mAdapterQ.get(&msg);

            if ( FakeCameraAdapter::ERROR == msg.command )
                {
                ret = -1;
                }

            break;

         case CameraAdapter::CAMERA_PERFORM_AUTOFOCUS:

            CAMHAL_LOGDA("Start AutoFocus");
            msg.command = BaseCameraAdapter::DO_AUTOFOCUS;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            msg.arg1 = (void *) value1;

#endif

            mFrameQ.put(&msg);
            MessageQueue::waitForMsg(&mAdapterQ, NULL, NULL, -1);
            mAdapterQ.get(&msg);

            if ( FakeCameraAdapter::ERROR == msg.command )
                {
                ret = -1;
                }

            break;

         case CameraAdapter::CAMERA_START_IMAGE_CAPTURE:

            CAMHAL_LOGDA("Start TakePicture");
            msg.command = BaseCameraAdapter::TAKE_PICTURE;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            msg.arg1 = (void *) value1;

#endif


            mFrameQ.put(&msg);
            MessageQueue::waitForMsg(&mAdapterQ, NULL, NULL, -1);
            mAdapterQ.get(&msg);

            if ( FakeCameraAdapter::ERROR == msg.command )
                {
                ret = -1;
                }

            break;

         default:
            CAMHAL_LOGEB("Command 0x%x unsupported!", operation);

            break;

    };

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

//API to cancel a currently executing command
status_t FakeCameraAdapter::cancelCommand(int operation)
{
    LOG_FUNCTION_NAME
    return NO_ERROR;
}

//API to get the frame size required to be allocated. This size is used to override the size passed
//by camera service when VSTAB/VNF is turned ON for example
void FakeCameraAdapter::getFrameSize(int &width, int &height)
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

}

status_t FakeCameraAdapter::getPictureBufferSize(size_t &length)
{
    LOG_FUNCTION_NAME

    length = DEFAULT_PICTURE_BUFFER_SIZE;

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}


void FakeCameraAdapter::frameCallbackThread()
{
    bool shouldLive = true;
    Message msg;
    frame_callback callback;
    CameraFrame *frame;

    LOG_FUNCTION_NAME

    while ( shouldLive )
        {
        MessageQueue::waitForMsg(&mCallbackQ, NULL, NULL, -1);
        mCallbackQ.get(&msg);

        if ( FakeCameraAdapter::CALL_CALLBACK == msg.command )
            {
            callback = ( frame_callback ) msg.arg1;
            frame = ( CameraFrame * ) msg.arg2;

            callback(frame);


            if ( NULL != frame )
                {
                delete frame;
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
    void *previewBuffer;
    CameraFrame *frame;
    uint32_t offset;
    int fd;
    Message msg;
    size_t count;
    int i = 0;

    offset = 0;
    fd = -1;
    previewBuffer = NULL;
        {
        Mutex::Autolock lock(mPreviewBufferLock);
        //check for any buffers available
        for ( i = 0 ; i < mPreviewBufferCount ; i++ )
            {
            if ( ( 0 == mPreviewBuffersRefCount.valueAt(i) ) && ( NULL != mPreviewBuffers ) )
                {
                previewBuffer = (void *) mPreviewBuffers[i];
                offset = mPreviewOffsets[i];
                fd = mPreviewFd;
                break;
                }
            }
        }

    if ( NULL == previewBuffer )
        return;

    //TODO: add pixelformat
    setBuffer(previewBuffer, i, mPreviewWidth, mPreviewHeight, 0, frameType);

        {
        Mutex::Autolock lock(mSubscriberLock);
        //check for any preview subscribers
        for (unsigned int i = 0 ; i < mFrameSubscribers.size(); i++ )
            {

            frame = new CameraFrame();
            if ( NULL == frame )
                {
                CAMHAL_LOGDA("No resources to allocate CameraFrame");
                break;
                }

            frame->mBuffer = previewBuffer;
            frame->mAlignment = PAGE_SIZE;
            frame->mWidth = mPreviewWidth;
            frame->mHeight = mPreviewHeight;
            frame->mLength = PAGE_SIZE*mPreviewHeight;
            frame->mOffset =offset;
            frame->mFd = fd;
            frame->mCookie = ( void * ) mFrameSubscribers.keyAt(i);
            frame->mTimestamp = 0;
            frame->mFrameType = CameraFrame::PREVIEW_FRAME_SYNC;

            msg.command = FakeCameraAdapter::CALL_CALLBACK;
            msg.arg1 = (void *) mFrameSubscribers.valueAt(i);
            msg.arg2 = (void *) frame;

                {
                Mutex::Autolock lock(mPreviewBufferLock);
                count = mPreviewBuffersRefCount.valueFor( ( unsigned int ) previewBuffer);
                count++;
                mPreviewBuffersRefCount.replaceValueFor( ( unsigned int ) previewBuffer, count);
                }

            mCallbackQ.put(&msg);
            }

        //check for any video subscribers
        for (unsigned int i = 0 ; i < mVideoSubscribers.size(); i++ )
            {

            frame = new CameraFrame();
            if ( NULL == frame )
                {
                CAMHAL_LOGDA("No resources to allocate CameraFrame");
                break;
                }

            frame->mBuffer = previewBuffer;
            frame->mAlignment = PAGE_SIZE;
            frame->mWidth = mPreviewWidth;
            frame->mHeight = mPreviewHeight;
            frame->mLength = PAGE_SIZE*mPreviewHeight;
            frame->mOffset =offset;
            frame->mFd = fd;
            frame->mCookie = ( void * ) mVideoSubscribers.keyAt(i);
            frame->mTimestamp = 0;
            frame->mFrameType = CameraFrame::VIDEO_FRAME_SYNC;

            msg.command = FakeCameraAdapter::CALL_CALLBACK;
            msg.arg1 = ( void * ) mVideoSubscribers.valueAt(i);
            msg.arg2 = ( void * ) frame;

                {
                Mutex::Autolock lock(mPreviewBufferLock);
                count = mPreviewBuffersRefCount.valueFor( ( unsigned int ) previewBuffer);
                count++;
                mPreviewBuffersRefCount.replaceValueFor( ( unsigned int ) previewBuffer, count);
                }

            mCallbackQ.put(&msg);
            }

        }
}

status_t FakeCameraAdapter::takePicture()
{
    status_t ret = NO_ERROR;
    CameraHalEvent shutterEvent;
    event_callback eventCb;
    Message msg;
    CameraFrame *frame;

    if ( mImageBufferCount <= 0 )
        {
        return -1;
        }

    void *imageBuf = ( void * ) mImageBuffers[0];

    LOG_FUNCTION_NAME

     shutterEvent.mEventType = CameraHalEvent::EVENT_SHUTTER;
        {
        Mutex::Autolock lock(mSubscriberLock);

        //Use the focus event subscribers as shutter event subscribers also
        //TODO: handle the different event types separately
        if ( mFocusSubscribers.size() == 0 )
            CAMHAL_LOGDA("No Shutter Subscribers!");

        for (unsigned int i = 0 ; i < mFocusSubscribers.size(); i++ )
            {
            shutterEvent.mCookie = (void *) mFocusSubscribers.keyAt(i);
            eventCb = (event_callback) mFocusSubscribers.valueAt(i);

            eventCb ( &shutterEvent );
            }

         }

    //simulate Snapshot
    sendNextFrame(SNAPSHOT_FRAME);

        //Start looking for frame subscribers
        {
        Mutex::Autolock lock(mSubscriberLock);

            if ( mRawSubscribers.size() == 0 )
                CAMHAL_LOGDA("No RAW Subscribers!");

            for (unsigned int i = 0 ; i < mRawSubscribers.size(); i++ )
                {

                frame = new CameraFrame();
                if ( NULL == frame )
                    {
                    CAMHAL_LOGDA("No resources to allocate CameraFrame");
                    break;
                    }

                frame->mBuffer = imageBuf;
                frame->mLength = mCaptureWidth*mCaptureHeight;
                frame->mCookie = ( void * ) mRawSubscribers.keyAt(i);
                frame->mFrameType = CameraFrame::RAW_FRAME;

                msg.command = FakeCameraAdapter::CALL_CALLBACK;
                msg.arg1 = ( void * ) mRawSubscribers.valueAt(i);
                msg.arg2 = ( void * ) frame;

                //RAW Capture
                mCallbackQ.put(&msg);
                }

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            //Dumps the shot to jpeg and shot to save measurements
            CameraHal::PPM("Shot to Jpeg: ", mStartCapture);
            CameraHal::PPM("Shot to Save: ", mStartCapture);

#endif


            if ( mImageSubscribers.size() == 0 )
                CAMHAL_LOGDA("No Image Subscribers!");

            for (unsigned int i = 0 ; i < mImageSubscribers.size(); i++ )
                {

                frame = new CameraFrame();
                if ( NULL == frame )
                    {
                    CAMHAL_LOGDA("No resources to allocate CameraFrame");
                    break;
                    }

                frame->mBuffer = imageBuf;
                frame->mLength = mCaptureWidth*mCaptureHeight;
                frame->mCookie = ( void * ) mImageSubscribers.keyAt(i);
                frame->mFrameType = CameraFrame::IMAGE_FRAME;

                msg.command = FakeCameraAdapter::CALL_CALLBACK;
                msg.arg1 = ( void * ) mImageSubscribers.valueAt(i);
                msg.arg2 = ( void * ) frame;

                //Jpeg encoding done
                mCallbackQ.put(&msg);
                }

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t FakeCameraAdapter::doAutofocus()
{
    event_callback eventCb;
    CameraHalEvent focusEvent;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

     //dump the AF latency
     CameraHal::PPM("Focus finished in: ", mStartFocus);

#endif


    //Just return success everytime
    focusEvent.mEventType = CameraHalEvent::EVENT_FOCUS_LOCKED;
    focusEvent.mEventData.focusEvent.focusLocked = true;

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

void FakeCameraAdapter::frameThread()
{
    bool shouldLive = true;
    int timeout;
    int state = FakeCameraAdapter::STOPPED;
    Message msg;
    size_t count;
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

                                    Mutex::Autolock lock(mPreviewBufferLock);

                                    count = mPreviewBuffersRefCount.valueFor( ( unsigned int ) msg.arg1);
                                    if ( count >= 1 )
                                        {
                                        count--;
                                        mPreviewBuffersRefCount.replaceValueFor( ( unsigned int ) msg.arg1, count);
                                        }
                                    else
                                        {
                                        CAMHAL_LOGEB("Negative refcount for preview buffer 0x%x!", (unsigned int)msg.arg1);
                                        }

                                }
                            }
                        else if ( BaseCameraAdapter::DO_AUTOFOCUS == msg.command )
                            {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

                            mStartFocus = ( struct timeval * ) msg.arg1;

#endif

                            ret = doAutofocus();
                            }
                        else if ( BaseCameraAdapter::TAKE_PICTURE == msg.command )
                            {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

                            mStartCapture = ( struct timeval * ) msg.arg1;

#endif

                            ret = takePicture();
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

                            Mutex::Autolock lock(mPreviewBufferLock);

                            count = mPreviewBuffersRefCount.valueFor( ( unsigned int ) msg.arg1);
                            if ( count >= 1 )
                                {
                                count--;
                                mPreviewBuffersRefCount.replaceValueFor( ( unsigned int ) msg.arg1, count);
                                }
                            else
                                {
                                CAMHAL_LOGEB("Negative refcount for preview buffer 0x%x!", (unsigned int)msg.arg1);
                                }

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

void FakeCameraAdapter::returnFrame(void* frameBuf, CameraFrame::FrameType frameType)
{
    Message msg;

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

}

extern "C" CameraAdapter* CameraAdapter_Factory() {
    FakeCameraAdapter *ret;

    LOG_FUNCTION_NAME

    ret = new FakeCameraAdapter();

    return ret;
}



};


/*--------------------Camera Adapter Class ENDS here-----------------------------*/



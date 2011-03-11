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

#include "BaseCameraAdapter.h"

namespace android {

/*--------------------Camera Adapter Class STARTS here-----------------------------*/

BaseCameraAdapter::BaseCameraAdapter()
{
    mReleaseImageBuffersCallback = NULL;
    mEndImageCaptureCallback = NULL;
    mErrorNotifier = NULL;
    mEndCaptureData = NULL;
    mReleaseData = NULL;
    mRecording = false;

    mPreviewBuffers = NULL;
    mPreviewBufferCount = 0;
    mPreviewBuffersLength = 0;

    mVideoBuffers = NULL;
    mVideoBuffersCount = 0;
    mVideoBuffersLength = 0;

    mCaptureBuffers = NULL;
    mCaptureBuffersCount = 0;
    mCaptureBuffersLength = 0;

    mPreviewDataBuffers = NULL;
    mPreviewDataBuffersCount = 0;
    mPreviewDataBuffersLength = 0;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
    mStartFocus.tv_sec = 0;
    mStartFocus.tv_usec = 0;
    mStartCapture.tv_sec = 0;
    mStartCapture.tv_usec = 0;
#endif

}

status_t BaseCameraAdapter::registerImageReleaseCallback(release_image_buffers_callback callback, void *user_data)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    mReleaseImageBuffersCallback = callback;
    mReleaseData = user_data;

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::registerEndCaptureCallback(end_image_capture_callback callback, void *user_data)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    mEndImageCaptureCallback= callback;
    mEndCaptureData = user_data;

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::setErrorHandler(ErrorNotifier *errorNotifier)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NULL == errorNotifier )
        {
        CAMHAL_LOGEA("Invalid Error Notifier reference");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        mErrorNotifier = errorNotifier;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

void BaseCameraAdapter::enableMsgType(int32_t msgs, frame_callback callback, event_callback eventCb, void* cookie)
{
    LOG_FUNCTION_NAME

    if ( CameraFrame::PREVIEW_FRAME_SYNC == msgs )
        {
            {
            Mutex::Autolock lock(mSubscriberLock);
            mFrameSubscribers.add((int) cookie, callback);
            }
        }
    else if ( CameraFrame::FRAME_DATA_SYNC == msgs )
        {
            {
            Mutex::Autolock lock(mSubscriberLock);
            CAMHAL_LOGEA("Adding FrameData Subscriber");
            mFrameDataSubscribers.add((int) cookie, callback);
            }
        }
    else if ( CameraFrame::IMAGE_FRAME == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mImageSubscribers.add((int) cookie, callback);
            }
        }
    else if ( CameraFrame::RAW_FRAME == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mRawSubscribers.add((int) cookie, callback);
            }
        }
    else if ( CameraFrame::VIDEO_FRAME_SYNC == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mVideoSubscribers.add((int) cookie, callback);
            }
        }
    else if ( CameraHalEvent::ALL_EVENTS == msgs)
        {
         //Subscribe only for focus
         //TODO: Process case by case
            {
                Mutex::Autolock lock(mSubscriberLock);
                mFocusSubscribers.add((int) cookie, eventCb);
            }

            {
                Mutex::Autolock lock(mSubscriberLock);
                mShutterSubscribers.add((int) cookie, eventCb);
            }

            {
                Mutex::Autolock lock(mSubscriberLock);
                mZoomSubscribers.add((int) cookie, eventCb);
            }
        }
    else
        {
        CAMHAL_LOGEA("Message type subscription no supported yet!");
        }

    LOG_FUNCTION_NAME_EXIT
}

void BaseCameraAdapter::disableMsgType(int32_t msgs, void* cookie)
{
    LOG_FUNCTION_NAME

    if ( CameraFrame::PREVIEW_FRAME_SYNC == msgs )
        {
            {
            Mutex::Autolock lock(mSubscriberLock);
            mFrameSubscribers.removeItem((int) cookie);
            }
        }
    else if ( CameraFrame::FRAME_DATA_SYNC == msgs )
        {
            {
            Mutex::Autolock lock(mSubscriberLock);
            mFrameDataSubscribers.removeItem((int) cookie);
            }
        }
    else if ( CameraFrame::IMAGE_FRAME == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mImageSubscribers.removeItem((int) cookie);
            }
        }
    else if ( CameraFrame::RAW_FRAME == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mRawSubscribers.removeItem((int) cookie);
            }
        }
    else if ( CameraFrame::VIDEO_FRAME_SYNC == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mVideoSubscribers.removeItem((int) cookie);
            }
        }
    else if ( CameraFrame::ALL_FRAMES  == msgs )
        {

            {
            Mutex::Autolock lock(mSubscriberLock);

            mFrameSubscribers.removeItem((int) cookie);
            mFrameDataSubscribers.removeItem((int) cookie);
            mImageSubscribers.removeItem((int) cookie);
            mRawSubscribers.removeItem((int) cookie);
            mVideoSubscribers.removeItem((int) cookie);
            }

        }
    else if ( CameraHalEvent::ALL_EVENTS == msgs)
        {
         //Subscribe only for focus
         //TODO: Process case by case
            {
            Mutex::Autolock lock(mSubscriberLock);

            mFocusSubscribers.removeItem((int) cookie);
            mShutterSubscribers.removeItem((int) cookie);
            mZoomSubscribers.removeItem((int) cookie);
            }
        }
    else
        {
        CAMHAL_LOGEB("Message type 0x%x subscription no supported yet!", msgs);
        }

    LOG_FUNCTION_NAME_EXIT
}

void BaseCameraAdapter::returnFrame(void* frameBuf, CameraFrame::FrameType frameType)
{
    status_t res = NO_ERROR;
    size_t subscriberCount = 0;
    int refCount = -1;

    if ( NULL == frameBuf )
        {
        CAMHAL_LOGEA("Invalid frameBuf");
        res = -EINVAL;
        }

    if ( NO_ERROR == res)
        {

        refCount = getFrameRefCount(frameBuf,  frameType);
        subscriberCount = getSubscriberCount(frameType);

        if ( 0 < refCount )
            {

            refCount--;
            setFrameRefCount(frameBuf, frameType, refCount);

            if ( ( mRecording ) && (  CameraFrame::VIDEO_FRAME_SYNC == frameType ) )
                {
                refCount += getFrameRefCount(frameBuf, CameraFrame::PREVIEW_FRAME_SYNC);
                }
            else if ( ( mRecording ) && (  CameraFrame::PREVIEW_FRAME_SYNC == frameType ) )
                {
                refCount += getFrameRefCount(frameBuf, CameraFrame::VIDEO_FRAME_SYNC);
                }

            }
        else
            {
             if ( 0 < subscriberCount )
                {
                CAMHAL_LOGEB("Error trying to decrement refCount %d for buffer 0x%x", ( uint32_t ) refCount, ( uint32_t ) frameBuf);
                }
            return;
            }
        }


    if ( NO_ERROR == res )
        {
        //check if someone is holding this buffer
        if ( 0 == refCount )
            {
            res = fillThisBuffer(frameBuf, frameType);
            }
        }

}

status_t BaseCameraAdapter::sendCommand(int operation, int value1, int value2, int value3)
{
    status_t ret = NO_ERROR;
    CameraAdapter::CameraMode mode;
    struct timeval *refTimestamp;
    BuffersDescriptor *desc = NULL;

    LOG_FUNCTION_NAME

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
                        mPreviewBuffers = (int *) desc->mBuffers;
                        mPreviewBuffersLength = desc->mLength;
                        mPreviewBuffersAvailable.clear();
                        for ( uint32_t i = 0 ; i < desc->mCount ; i++ )
                            {
                            mPreviewBuffersAvailable.add(mPreviewBuffers[i], 0);
                            }
                        }
                    }
                else if( CameraAdapter::CAMERA_MEASUREMENT == mode )
                    {
                    if ( NULL == desc )
                        {
                        CAMHAL_LOGEA("Invalid preview data buffers!");
                        ret = -1;
                        }
                    if ( ret == NO_ERROR )
                        {
                        Mutex::Autolock lock(mPreviewDataBufferLock);
                        mPreviewDataBuffers = (int *) desc->mBuffers;
                        mPreviewDataBuffersLength = desc->mLength;
                        mPreviewDataBuffersAvailable.clear();
                        for ( uint32_t i = 0 ; i < desc->mCount ; i++ )
                            {
                            mPreviewDataBuffersAvailable.add(mPreviewDataBuffers[i], true);
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
                        mCaptureBuffers = (int *) desc->mBuffers;
                        mCaptureBuffersLength = desc->mLength;
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
                    useBuffers(mode, desc->mBuffers, desc->mCount, desc->mLength);
                    }
                break;
            }

        case CameraAdapter::CAMERA_START_SMOOTH_ZOOM:
            {
            ret = startSmoothZoom(value1);
            break;
            }

        case CameraAdapter::CAMERA_STOP_SMOOTH_ZOOM:
            {
            ret = stopSmoothZoom();
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
            ret = stopPreview();
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
        case CameraAdapter::CAMERA_SET_TIMEOUT:
            {
            CAMHAL_LOGDA("Set time out");
            setTimeOut(value1);
            break;
            }
        case CameraAdapter::CAMERA_CANCEL_TIMEOUT:
            {
            CAMHAL_LOGDA("Cancel time out");
            cancelTimeOut();
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
                memcpy( &mStartCapture, refTimestamp, sizeof( struct timeval ));
                }

#endif

            ret = takePicture();
            break;
            }
        case CameraAdapter::CAMERA_STOP_IMAGE_CAPTURE:
            {
            ret = stopImageCapture();
            break;
            }
        case CameraAdapter::CAMERA_START_BRACKET_CAPTURE:
            {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            refTimestamp = ( struct timeval * ) value2;
            if ( NULL != refTimestamp )
                {
                memcpy( &mStartCapture, refTimestamp, sizeof( struct timeval ));
                }

#endif

            ret = startBracketing(value1);
            break;
            }
        case CameraAdapter::CAMERA_STOP_BRACKET_CAPTURE:
            {
            ret = stopBracketing();
            break;
            }
         case CameraAdapter::CAMERA_PERFORM_AUTOFOCUS:

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            refTimestamp = ( struct timeval * ) value1;
            if ( NULL != refTimestamp )
                {
                memcpy( &mStartFocus, refTimestamp, sizeof( struct timeval ));
                }

#endif

            ret = autoFocus();
            break;
         case CameraAdapter::CAMERA_CANCEL_AUTOFOCUS:
            ret = cancelAutoFocus();
            break;

        default:
            CAMHAL_LOGEB("Command 0x%x unsupported!", operation);
            break;
    };

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t BaseCameraAdapter::cancelCommand(int operation)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::notifyFocusSubscribers(bool status)
{
    event_callback eventCb;
    CameraHalEvent focusEvent;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        if ( mFocusSubscribers.size() == 0 )
            {
            CAMHAL_LOGDA("No Focus Subscribers!");
            ret = -1;
            }
        }

    if ( NO_ERROR == ret )
        {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

         //dump the AF latency
         CameraHal::PPM("Focus finished in: ", &mStartFocus);

#endif

        focusEvent.mEventType = CameraHalEvent::EVENT_FOCUS_LOCKED;
        focusEvent.mEventData.focusEvent.focusLocked = status;
        focusEvent.mEventData.focusEvent.focusError = !status;

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

status_t BaseCameraAdapter::notifyShutterSubscribers()
{
    CameraHalEvent shutterEvent;
    event_callback eventCb;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {

        if ( mShutterSubscribers.size() == 0 )
            {
            CAMHAL_LOGEA("No shutter Subscribers!");
            return ret;
            }

        shutterEvent.mEventType = CameraHalEvent::EVENT_SHUTTER;
        shutterEvent.mEventData.shutterEvent.shutterClosed = true;

        for (unsigned int i = 0 ; i < mShutterSubscribers.size() ; i++ )
            {
            shutterEvent.mCookie = ( void * ) mShutterSubscribers.keyAt(i);
            eventCb = ( event_callback ) mShutterSubscribers.valueAt(i);

            CAMHAL_LOGEA("Sending shutter callback");

            eventCb ( &shutterEvent );
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t BaseCameraAdapter::notifyZoomSubscribers(int zoomIdx, bool targetReached)
{
    event_callback eventCb;
    CameraHalEvent zoomEvent;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    zoomEvent.mEventType = CameraHalEvent::EVENT_ZOOM_INDEX_REACHED;
    zoomEvent.mEventData.zoomEvent.currentZoomIndex = zoomIdx;
    zoomEvent.mEventData.zoomEvent.targetZoomIndexReached = targetReached;

    if ( mZoomSubscribers.size() == 0 )
        {
        CAMHAL_LOGDA("No Focus Subscribers!");
        }

    for (unsigned int i = 0 ; i < mZoomSubscribers.size(); i++ )
        {
        zoomEvent.mCookie = (void *) mZoomSubscribers.keyAt(i);
        eventCb = (event_callback) mZoomSubscribers.valueAt(i);

        eventCb ( &zoomEvent );
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::sendFrameToSubscribers(CameraFrame *frame)
{
    status_t ret = NO_ERROR;
    frame_callback callback;
    uint32_t i = 0;
    KeyedVector<int, frame_callback> *subscribers = NULL;

    LOG_FUNCTION_NAME

    if ( NULL == frame )
        {
        CAMHAL_LOGEA("Invalid CameraFrame");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {

        switch(frame->mFrameType)
            {
                case CameraFrame::IMAGE_FRAME:
                    {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

                    CameraHal::PPM("Shot to Jpeg: ", &mStartCapture);

#endif

                    subscribers = &mImageSubscribers;
                    break;
                    }
                case CameraFrame::RAW_FRAME:
                    {
                    subscribers = &mRawSubscribers;
                    break;
                    }
                case CameraFrame::VIDEO_FRAME_SYNC:
                    {
                    subscribers = &mVideoSubscribers;
                    break;
                    }
                case CameraFrame::FRAME_DATA_SYNC:
                    {
                    subscribers = &mFrameDataSubscribers;
                    break;
                    }
                case CameraFrame::PREVIEW_FRAME_SYNC:
                case CameraFrame::SNAPSHOT_FRAME:
                    {
                    subscribers = &mFrameSubscribers;
                    break;
                    }
                default:
                    {
                    ret = -EINVAL;
                    break;
                    }
            };

        }

    if ( ( NO_ERROR == ret ) &&
         ( NULL != subscribers ) )
        {
        for ( i = 0 ; i < subscribers->size(); i++ )
            {
            frame->mCookie = ( void * ) subscribers->keyAt(i);
            callback = (frame_callback) subscribers->valueAt(i);
            callback(frame);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    if ( 0 == i )
        {
        //No subscribers for this frame
        ret = -1;
        }

    return ret;
}

status_t BaseCameraAdapter::resetFrameRefCount(CameraFrame &frame)
{
    status_t ret = NO_ERROR;
    size_t refCount = 0;

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        refCount = getSubscriberCount(( CameraFrame::FrameType ) frame.mFrameType);
        CAMHAL_LOGVB("Type of Frame: 0x%x address: 0x%x refCount start %d",
                                    frame.mFrameType,
                                    ( uint32_t ) frame.mBuffer,
                                    refCount);

        setFrameRefCount(frame.mBuffer, (  CameraFrame::FrameType ) frame.mFrameType, refCount);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

int BaseCameraAdapter::getFrameRefCount(void* frameBuf, CameraFrame::FrameType frameType)
{
    int res = -1;

    LOG_FUNCTION_NAME

    switch ( frameType )
        {
        case CameraFrame::IMAGE_FRAME:
        case CameraFrame::RAW_FRAME:
                {
                Mutex::Autolock lock(mCaptureBufferLock);
                res = mCaptureBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                }
            break;
        case CameraFrame::PREVIEW_FRAME_SYNC:
        case CameraFrame::SNAPSHOT_FRAME:
                {
                Mutex::Autolock lock(mPreviewBufferLock);
                res = mPreviewBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                }
            break;
        case CameraFrame::FRAME_DATA_SYNC:
                {
                Mutex::Autolock lock(mPreviewDataBufferLock);
                res = mPreviewDataBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                }
            break;
        case CameraFrame::VIDEO_FRAME_SYNC:
                {
                Mutex::Autolock lock(mVideoBufferLock);
                res = mVideoBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                }
            break;
        default:
            break;
        };

    LOG_FUNCTION_NAME_EXIT

    return res;
}

void BaseCameraAdapter::setFrameRefCount(void* frameBuf, CameraFrame::FrameType frameType, int refCount)
{

    LOG_FUNCTION_NAME

    switch ( frameType )
        {
        case CameraFrame::IMAGE_FRAME:
        case CameraFrame::RAW_FRAME:
                {
                Mutex::Autolock lock(mCaptureBufferLock);
                mCaptureBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);
                }
            break;
        case CameraFrame::PREVIEW_FRAME_SYNC:
        case CameraFrame::SNAPSHOT_FRAME:
                {
                Mutex::Autolock lock(mPreviewBufferLock);
                mPreviewBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);
                }
            break;
        case CameraFrame::FRAME_DATA_SYNC:
                {
                Mutex::Autolock lock(mPreviewDataBufferLock);
                mPreviewDataBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);
                }
            break;
        case CameraFrame::VIDEO_FRAME_SYNC:
                {
                Mutex::Autolock lock(mVideoBufferLock);
                mVideoBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);
                }
            break;
        default:
            break;
        };

    LOG_FUNCTION_NAME_EXIT

}

size_t BaseCameraAdapter::getSubscriberCount(CameraFrame::FrameType frameType)
{
    size_t ret = 0;

    LOG_FUNCTION_NAME

    switch ( frameType )
        {
        case CameraFrame::IMAGE_FRAME:
        case CameraFrame::RAW_FRAME:
                {
                Mutex::Autolock lock(mSubscriberLock);
                ret = mImageSubscribers.size();
                }
            break;
        case CameraFrame::PREVIEW_FRAME_SYNC:
        case CameraFrame::SNAPSHOT_FRAME:
                {
                Mutex::Autolock lock(mSubscriberLock);
                ret = mFrameSubscribers.size();
                }
            break;
        case CameraFrame::FRAME_DATA_SYNC:
                {
                Mutex::Autolock lock(mSubscriberLock);
                ret = mFrameDataSubscribers.size();
                }
            break;
        case CameraFrame::VIDEO_FRAME_SYNC:
                {
                Mutex::Autolock lock(mSubscriberLock);
                ret = mVideoSubscribers.size();
                }
            break;
        default:
            break;
        };

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::setTimeOut(int sec)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    //Subscriptions are also invalid
    Mutex::Autolock lock(mSubscriberLock);

    mFrameSubscribers.clear();
    mImageSubscribers.clear();
    mRawSubscribers.clear();
    mVideoSubscribers.clear();
    mFocusSubscribers.clear();
    mShutterSubscribers.clear();
    mZoomSubscribers.clear();

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::startVideoCapture()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mVideoBufferLock);

    //If the capture is already ongoing, return from here.
    if ( mRecording )
        {
        ret = NO_INIT;
        }


    if ( NO_ERROR == ret )
        {

        for ( unsigned int i = 0 ; i < mPreviewBuffersAvailable.size() ; i++ )
            {
            mVideoBuffersAvailable.add(mPreviewBuffersAvailable.keyAt(i), 0);
            }

        mRecording = true;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::stopVideoCapture()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( !mRecording )
        {
        ret = NO_INIT;
        }

    if ( NO_ERROR == ret )
        {
        for ( unsigned int i = 0 ; i < mVideoBuffersAvailable.size() ; i++ )
            {
            void *frameBuf = ( void * ) mVideoBuffersAvailable.keyAt(i);
            if( getFrameRefCount(frameBuf,  CameraFrame::VIDEO_FRAME_SYNC) > 0)
                {
                returnFrame(frameBuf, CameraFrame::VIDEO_FRAME_SYNC);
                }
            }

        mVideoBuffersAvailable.clear();

        mRecording = false;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

//-----------------Stub implementation of the interface ------------------------------

status_t BaseCameraAdapter::takePicture()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::stopImageCapture()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::startBracketing(int range)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::stopBracketing()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::autoFocus()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    notifyFocusSubscribers(false);

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::cancelAutoFocus()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::cancelTimeOut()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::startSmoothZoom(int targetIdx)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::stopSmoothZoom()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::startPreview()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::stopPreview()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::useBuffers(CameraMode mode, void* bufArr, int num, size_t length)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t BaseCameraAdapter::fillThisBuffer(void* frameBuf, CameraFrame::FrameType frameType)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

//-----------------------------------------------------------------------------



};

/*--------------------Camera Adapter Class ENDS here-----------------------------*/


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


#include "CameraHal.h"


namespace android {

const int AppCallbackNotifier::NOTIFIER_TIMEOUT = -1;
const size_t AppCallbackNotifier::EMPTY_RAW_SIZE = 1;

/*--------------------NotificationHandler Class STARTS here-----------------------------*/

/**
  * NotificationHandler class
  */


///Initialization function for AppCallbackNotifier
status_t AppCallbackNotifier::initialize()
{
    LOG_FUNCTION_NAME

    mMeasurementEnabled = false;

    ///Create the app notifier thread
    mNotificationThread = new NotificationThread(this);
    if(!mNotificationThread.get())
        {
        CAMHAL_LOGEA("Couldn't create Notification thread");
        return NO_MEMORY;
        }

    ///Start the display thread
    status_t ret = mNotificationThread->run("NotificationThread", PRIORITY_URGENT_DISPLAY);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEA("Couldn't run NotificationThread");
        mNotificationThread.clear();
        return ret;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

void AppCallbackNotifier::setCallbacks(CameraHardwareInterface * cameraHal,
                                                              notify_callback notifyCb,
                                                              data_callback dataCb,
                                                              data_callback_timestamp dataCbTimestamp,
                                                              void * user)
{
    Mutex::Autolock lock(mLock);

    LOG_FUNCTION_NAME

    mCameraHal = cameraHal;
    mNotifyCb = notifyCb;
    mDataCb = dataCb;
    mDataCbTimestamp = dataCbTimestamp;
    mCallbackCookie = user;

    LOG_FUNCTION_NAME_EXIT
}

void AppCallbackNotifier::setMeasurements(bool enable)
{
    Mutex::Autolock lock(mLock);

    LOG_FUNCTION_NAME

    mMeasurementEnabled = enable;

    if (  enable  )
        {
         mFrameProvider->enableFrameNotification(CameraFrame::FRAME_DATA_SYNC);
        }

    LOG_FUNCTION_NAME_EXIT
}


//All sub-components of Camera HAL call this whenever any error happens
void AppCallbackNotifier::errorNotify(int error)
{
    LOG_FUNCTION_NAME

    CAMHAL_LOGEB("AppCallbackNotifier received error 0x%x", error);

    ///Notify errors to application in callback thread. Post error event to event queue
    Message msg;
    msg.command = AppCallbackNotifier::NOTIFIER_CMD_PROCESS_ERROR;
    msg.arg1 = (void*)error;

    mEventQ.put(&msg);

    LOG_FUNCTION_NAME_EXIT
}

void AppCallbackNotifier::notificationThread()
{
    bool shouldLive = true;
    status_t ret;

    LOG_FUNCTION_NAME

    while(shouldLive)
        {
        //CAMHAL_LOGDA("Notification Thread waiting for message");
        ret = MessageQueue::waitForMsg(&mNotificationThread->msgQ()
                                                        , &mEventQ
                                                        , &mFrameQ
                                                        , AppCallbackNotifier::NOTIFIER_TIMEOUT);

        //CAMHAL_LOGDA("Notification Thread received message");

        if(!mNotificationThread->msgQ().isEmpty())
            {
            ///Received a message from CameraHal, process it
            CAMHAL_LOGDA("Notification Thread received message from Camera HAL");
            shouldLive = processMessage();
            if(!shouldLive)
                {
                CAMHAL_LOGDA("Notification Thread exiting.");
                }
            }
        else if(!mEventQ.isEmpty())
            {
            ///Received an event from one of the event providers
            CAMHAL_LOGDA("Notification Thread received an event from event provider (CameraAdapter)");
            notifyEvent();
            }
        else if(!mFrameQ.isEmpty())
            {
            ///Received a frame from one of the frame providers
            //CAMHAL_LOGDA("Notification Thread received a frame from frame provider (CameraAdapter)");
            notifyFrame();
            }
        else
            {
            ///Timeout case
            ///@todo: May have to signal an error
            CAMHAL_LOGDA("Notification Thread timed out");
            continue;
            }

        }

    CAMHAL_LOGDA("Notification Thread exited.");
    LOG_FUNCTION_NAME_EXIT

}

void AppCallbackNotifier::notifyEvent()
{
    ///Receive and send the event notifications to app
    Message msg;
    LOG_FUNCTION_NAME
    mEventQ.get(&msg);
    bool ret = true;
    CameraHalEvent *evt = NULL;
    CameraHalEvent::FocusEventData *focusEvtData;
    CameraHalEvent::ZoomEventData *zoomEvtData;

    if(mNotifierState != AppCallbackNotifier::NOTIFIER_STARTED)
    {
        return;
    }

    switch(msg.command)
        {
        case AppCallbackNotifier::NOTIFIER_CMD_PROCESS_EVENT:

            evt = ( CameraHalEvent * ) msg.arg1;

            if ( NULL == evt )
                {
                CAMHAL_LOGEA("Invalid CameraHalEvent");
                return;
                }

            switch(evt->mEventType)
                {
                case CameraHalEvent::EVENT_SHUTTER:

                    if ( ( NULL != mCameraHal.get() ) &&
                          ( NULL != mNotifyCb ) &&
                          ( mCameraHal->msgTypeEnabled(CAMERA_MSG_SHUTTER) ) )
                        {
                            mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0, mCallbackCookie);
                        }

                    break;

                case CameraHalEvent::EVENT_FOCUS_LOCKED:
                case CameraHalEvent::EVENT_FOCUS_ERROR:

                    focusEvtData = &evt->mEventData.focusEvent;
                    if ( ( focusEvtData->focusLocked ) &&
                          ( NULL != mCameraHal.get() ) &&
                          ( NULL != mNotifyCb ) &&
                          ( mCameraHal->msgTypeEnabled(CAMERA_MSG_FOCUS) ) )
                        {
                         mNotifyCb(CAMERA_MSG_FOCUS, true, 0, mCallbackCookie);
                        }
                    else if ( focusEvtData->focusError &&
                                ( NULL != mCameraHal.get() ) &&
                                ( NULL != mNotifyCb ) &&
                                ( mCameraHal->msgTypeEnabled(CAMERA_MSG_FOCUS) ) )
                        {
                         mNotifyCb(CAMERA_MSG_FOCUS, false, 0, mCallbackCookie);
                        }

                    break;

                case CameraHalEvent::EVENT_ZOOM_INDEX_REACHED:

                    zoomEvtData = &evt->mEventData.zoomEvent;

                    if ( ( NULL != mCameraHal.get() ) &&
                         ( NULL != mNotifyCb) &&
                         ( mCameraHal->msgTypeEnabled(CAMERA_MSG_ZOOM) ) )
                        {
                        mNotifyCb(CAMERA_MSG_ZOOM, zoomEvtData->currentZoomIndex, zoomEvtData->targetZoomIndexReached, mCallbackCookie);
                        }

                    break;

                case CameraHalEvent::ALL_EVENTS:
                    break;
                default:
                    break;
                }

            break;

        case AppCallbackNotifier::NOTIFIER_CMD_PROCESS_ERROR:

            if (  ( NULL != mCameraHal.get() ) &&
                  ( NULL != mNotifyCb ) &&
                  ( mCameraHal->msgTypeEnabled(CAMERA_MSG_ERROR) ) )
                {
                mNotifyCb(CAMERA_MSG_ERROR, CAMERA_ERROR_UKNOWN, 0, mCallbackCookie);
                }

            break;

        }

    if ( NULL != evt )
        {
        delete evt;
        }


    LOG_FUNCTION_NAME_EXIT

}

static void copy2Dto1D(void *dst, void *src, int width, int height, size_t stride, uint32_t offset, unsigned int bytesPerPixel, size_t length,
    const char *pixelFormat)
{
    unsigned int alignedRow, row;
    unsigned char *bufferDst, *bufferSrc;
    unsigned char *bufferDstEnd, *bufferSrcEnd;
    uint16_t *bufferDst_UV, *bufferSrc_UV;

    if(pixelFormat!=NULL)
        {
        if(strcmp(pixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) == 0)
            {
            bytesPerPixel = 2;
            }
        else if(strcmp(pixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_YUV420SP) == 0)
            {
            //Convert here from NV12 to NV21 and return
            bytesPerPixel = 1;
            uint32_t xOff = offset % PAGE_SIZE;
            uint32_t yOff = offset / PAGE_SIZE;
            bufferDst = ( unsigned char * ) dst;
            bufferDstEnd = ( unsigned char * ) ( (uint32_t) bufferDst + width*height*bytesPerPixel );
            bufferSrc = ( unsigned char * ) ( (uint32_t) src + offset );
            bufferSrcEnd = ( unsigned char * ) ( (uint32_t) bufferSrc + length );
            row = width*bytesPerPixel;
            alignedRow = stride-width;
            int stride_bytes = stride / 8;
            size_t bufferSize_Y = 0;

            //iterate through each row
            for ( int i = 0 ; i < height ; i++)
                {
                memcpy(bufferDst, bufferSrc, row);

                bufferSrc += stride;
                bufferDst += row;
                if ( ( bufferSrc > bufferSrcEnd ) || ( bufferDst > bufferDstEnd ) )
                    {
                    break;
                    }

                }

            ///Convert NV21 to NV12 by swapping U & V
            bufferDst_UV = (uint16_t *) (((uint8_t*)dst)+row*height);
            bufferSize_Y = (( length + offset ) / 3) * 2;
            bufferSrc_UV = ( uint16_t * ) (((uint8_t*)src) + bufferSize_Y + (stride/2)*yOff + xOff);

            for(int i = 0 ; i < height/2 ; i++, bufferSrc_UV += alignedRow/2)
            {
                int n = width;
                asm volatile (
                "   pld [%[src], %[src_stride], lsl #2]                         \n\t"
                "   cmp %[n], #32                                               \n\t"
                "   blt 1f                                                      \n\t"
                "0: @ 32 byte swap                                              \n\t"
                "   sub %[n], %[n], #32                                         \n\t"
                "   vld2.8  {q0, q1} , [%[src]]!                                \n\t"
                "   vswp q0, q1                                                 \n\t"
                "   cmp %[n], #32                                               \n\t"
                "   vst2.8  {q0,q1},[%[dst]]!                                   \n\t"
                "   bge 0b                                                      \n\t"
                "1: @ Is there enough data?                                     \n\t"
                "   cmp %[n], #16                                               \n\t"
                "   blt 3f                                                      \n\t"
                "2: @ 16 byte swap                                              \n\t"
                "   sub %[n], %[n], #16                                         \n\t"
                "   vld2.8  {d0, d1} , [%[src]]!                                \n\t"
                "   vswp d0, d1                                                 \n\t"
                "   cmp %[n], #16                                               \n\t"
                "   vst2.8  {d0,d1},[%[dst]]!                                   \n\t"
                "   bge 2b                                                      \n\t"
                "3: @ Is there enough data?                                     \n\t"
                "   cmp %[n], #8                                                \n\t"
                "   blt 5f                                                      \n\t"
                "4: @ 8 byte swap                                               \n\t"
                "   sub %[n], %[n], #8                                          \n\t"
                "   vld2.8  {d0, d1} , [%[src]]!                                \n\t"
                "   vswp d0, d1                                                 \n\t"
                "   cmp %[n], #8                                                \n\t"
                "   vst2.8  {d0[0],d1[0]},[%[dst]]!                             \n\t"
                "   bge 4b                                                      \n\t"
                "5: @ end                                                       \n\t"
#ifdef NEEDS_ARM_ERRATA_754319_754320
                "   vmov s0,s0  @ add noop for errata item                      \n\t"
#endif
                : [dst] "+r" (bufferDst_UV), [src] "+r" (bufferSrc_UV), [n] "+r" (n)
                : [src_stride] "r" (stride_bytes)
                : "cc", "memory", "q0", "q1"
                );
            }

            return ;

            }
        else if(strcmp(pixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_RGB565) == 0)
            {
            bytesPerPixel = 2;
            }
    }

    bufferDst = ( unsigned char * ) dst;
    bufferSrc = ( unsigned char * ) src;
    row = width*bytesPerPixel;
    alignedRow = ( row + ( stride -1 ) ) & ( ~ ( stride -1 ) );

    //iterate through each row
    for ( int i = 0 ; i < height ; i++,  bufferSrc += alignedRow, bufferDst += row)
        {
        memcpy(bufferDst, bufferSrc, row);
        }
}

void AppCallbackNotifier::notifyFrame()
{
    ///Receive and send the frame notifications to app
    Message msg;
    CameraFrame *frame;
    MemoryHeapBase *heap;
    MemoryBase *buffer = NULL;
    sp<MemoryBase> memBase;
    void *buf = NULL;

    LOG_FUNCTION_NAME

    if(!mFrameQ.isEmpty())
        {
    mFrameQ.get(&msg);
        }
    else
        {
        return;
        }

    bool ret = true;

    if(mNotifierState != AppCallbackNotifier::NOTIFIER_STARTED)
    {
        return;
    }

    frame = NULL;
    switch(msg.command)
        {
        case AppCallbackNotifier::NOTIFIER_CMD_PROCESS_FRAME:

                frame = (CameraFrame *) msg.arg1;
                if(!frame)
                    {
                    break;
                    }

                if ( (CameraFrame::RAW_FRAME == frame->mFrameType )&&
                    ( NULL != mCameraHal.get() ) &&
                    ( NULL != mDataCb) &&
                    ( mCameraHal->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE) ) )
                    {

#ifdef COPY_IMAGE_BUFFER

                    //MTS tests always require a raw callback during image capture.
                    //In some cases raw data is not available. Currently empty raw callbacks
                    //are the only remedy for these cases.
                    if ( 0 < frame->mLength )
                        {
                        sp<MemoryHeapBase> RAWPictureHeap = new MemoryHeapBase(frame->mLength);
                        sp<MemoryBase> RAWPictureMemBase = new MemoryBase(RAWPictureHeap, 0, frame->mLength);
                        buf = RAWPictureMemBase->pointer();
                        if (buf)
                          memcpy(buf, ( void * ) ( (unsigned int) frame->mBuffer + frame->mOffset) , frame->mLength);

                        mDataCb(CAMERA_MSG_RAW_IMAGE, RAWPictureMemBase, mCallbackCookie);

                        mFrameProvider->returnFrame(frame->mBuffer,  ( CameraFrame::FrameType ) frame->mFrameType);
                        }
                    else
                        {
                        sp<MemoryHeapBase> RAWPictureHeap = new MemoryHeapBase(EMPTY_RAW_SIZE);
                        sp<MemoryBase> RAWPictureMemBase = new MemoryBase(RAWPictureHeap, 0, EMPTY_RAW_SIZE);

                        mDataCb(CAMERA_MSG_RAW_IMAGE, RAWPictureMemBase, mCallbackCookie);
                        }

#else

                     //TODO: Find a way to map a Tiler buffer to a MemoryHeapBase

#endif


                    }
                else if ( ( CameraFrame::IMAGE_FRAME == frame->mFrameType ) &&
                             ( NULL != mCameraHal.get() ) &&
                             ( NULL != mDataCb) )
                    {
                    Mutex::Autolock lock(mLock);

#ifdef COPY_IMAGE_BUFFER

                        sp<MemoryHeapBase> JPEGPictureHeap = new MemoryHeapBase(frame->mLength);
                        sp<MemoryBase> JPEGPictureMemBase = new MemoryBase(JPEGPictureHeap, 0, frame->mLength);
                        buf = JPEGPictureMemBase->pointer();
                        if (buf)
                          memcpy(buf, ( void * ) ( (unsigned int) frame->mBuffer + frame->mOffset) , frame->mLength);

                        {
                            Mutex::Autolock lock(mBurstLock);
                            if ( mBurst )
                                {
                                mDataCb(CAMERA_MSG_BURST_IMAGE, JPEGPictureMemBase, mCallbackCookie);
                                }
                            else
                                {
                                mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, JPEGPictureMemBase, mCallbackCookie);
                                }
                        }
#else

                     //TODO: Find a way to map a Tiler buffer to a MemoryHeapBase

#endif

                        mFrameProvider->returnFrame(frame->mBuffer,  ( CameraFrame::FrameType ) frame->mFrameType);

                    }
                else if ( ( CameraFrame::VIDEO_FRAME_SYNC == frame->mFrameType ) &&
                             ( NULL != mCameraHal.get() ) &&
                             ( NULL != mDataCb) &&
                             ( mCameraHal->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)  ) )
                    {

                    mRecordingLock.lock();
                    if(mRecording)
                        {
                        buffer = ( MemoryBase * ) mVideoBuffers.valueFor( ( unsigned int ) frame->mBuffer );

                        if( (NULL == buffer) || ( NULL == frame->mBuffer) )
                            {
                            CAMHAL_LOGDA("Error! One of the video buffer is NULL");
                            mRecordingLock.unlock();
                            break;
                            }
                        }
                    mRecordingLock.unlock();

                    if(buffer)
                        {
                        //CAMHAL_LOGDB("+CB 0x%x buffer 0x%x", frame->mBuffer, buffer);
#ifdef OMAP_ENHANCEMENT
                        mDataCbTimestamp(frame->mTimestamp, CAMERA_MSG_VIDEO_FRAME, buffer, mCallbackCookie
                                                , frame->mOffset, PAGE_SIZE);
#else
                        mDataCbTimestamp(frame->mTimestamp, CAMERA_MSG_VIDEO_FRAME, buffer, mCallbackCookie);
#endif
                        //CAMHAL_LOGDA("-CB");
                        }

                    }
                else if(( CameraFrame::SNAPSHOT_FRAME == frame->mFrameType ) &&
                             ( NULL != mCameraHal.get() ) &&
                             ( NULL != mDataCb) &&
                             ( NULL != mNotifyCb))
                    {

                    Mutex::Autolock lock(mLock);
                    //When enabled, measurement data is sent instead of video data
                    if ( !mMeasurementEnabled )
                        {

                        if(!mAppSupportsStride)
                            {
                            buffer = mPreviewBuffers[mPreviewBufCount].get();
                            if(!buffer || !frame->mBuffer)
                                {
                                CAMHAL_LOGDA("Error! One of the buffer is NULL");
                                break;
                                }
                            }
                        else
                            {

                            memBase = mSharedPreviewBuffers.valueFor( ( unsigned int ) frame->mBuffer );

                            if( (NULL == memBase.get() ) || ( NULL == frame->mBuffer) )
                                {
                                CAMHAL_LOGDA("Error! One of the preview buffer is NULL");
                                break;
                                }
                            }

                        if(!mAppSupportsStride)
                            {
                              buf = (buffer->pointer());

                              CAMHAL_LOGVB("%d:copy2Dto1D(%p, %p, %d, %d, %d, %d, %d,%s)", __LINE__, buf, frame->mBuffer,
                                        frame->mWidth, frame->mHeight, frame->mAlignment, 2, frame->mLength, mPreviewPixelFormat);

                              if (buf)
                                copy2Dto1D(buf, frame->mBuffer, frame->mWidth, frame->mHeight, frame->mAlignment, frame->mOffset, 2, frame->mLength,
                                           mPreviewPixelFormat);

                            mPreviewBufCount = (mPreviewBufCount+1) % AppCallbackNotifier::MAX_BUFFERS;

                            memBase = buffer;
                            }


                        if(mCameraHal->msgTypeEnabled(CAMERA_MSG_SHUTTER))
                            {
                            //activate shutter sound
                            mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0, mCallbackCookie);
                            }

                        if(mCameraHal->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME))
                            {

                            ///Give preview callback to app
                            mDataCb(CAMERA_MSG_POSTVIEW_FRAME, memBase, mCallbackCookie);
                            }

                        }

                    mFrameProvider->returnFrame(frame->mBuffer,  ( CameraFrame::FrameType ) frame->mFrameType);

                    }
                else if(( CameraFrame::PREVIEW_FRAME_SYNC== frame->mFrameType ) &&
                             ( NULL != mCameraHal.get() ) &&
                             ( NULL != mDataCb) &&
                             ( mCameraHal->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)  ))
                    {

                    Mutex::Autolock lock(mLock);
                    //When enabled, measurement data is sent instead of video data
                    if ( !mMeasurementEnabled )
                        {

                        if(!mAppSupportsStride)
                            {
                            buffer = mPreviewBuffers[mPreviewBufCount].get();
                            if(!buffer || !frame->mBuffer)
                                {
                                CAMHAL_LOGDA("Error! One of the buffer is NULL");
                                break;
                                }
                            }
                        else
                            {

                            memBase = mSharedPreviewBuffers.valueFor( ( unsigned int ) frame->mBuffer );

                            if( (NULL == memBase.get() ) || ( NULL == frame->mBuffer) )
                                {
                                CAMHAL_LOGDA("Error! One of the preview buffer is NULL");
                                break;
                                }
                            }

                        if(!mAppSupportsStride)
                            {
                            ///CAMHAL_LOGDB("+Copy 0x%x to 0x%x frame-%dx%d", frame->mBuffer, buffer->pointer(), frame->mWidth,frame->mHeight );
                            ///Copy the data into 1-D buffer
                            buf = buffer->pointer();

                            CAMHAL_LOGVB("%d:copy2Dto1D(%p, %p, %d, %d, %d, %d, %d,%s)", __LINE__, buf, frame->mBuffer,
                                      frame->mWidth, frame->mHeight, frame->mAlignment, 2, frame->mLength, mPreviewPixelFormat);

                            if (buf)
                              copy2Dto1D(buf, frame->mBuffer, frame->mWidth, frame->mHeight, frame->mAlignment, frame->mOffset, 2, frame->mLength,
                                         mPreviewPixelFormat);
                            ///CAMHAL_LOGDA("-Copy");

                            //Increment the buffer count
                            mPreviewBufCount = (mPreviewBufCount+1) % AppCallbackNotifier::MAX_BUFFERS;

                            memBase = buffer;
                            }

                        ///Give preview callback to app
                        mDataCb(CAMERA_MSG_PREVIEW_FRAME, memBase, mCallbackCookie);

                        }

                    mFrameProvider->returnFrame(frame->mBuffer,  ( CameraFrame::FrameType ) frame->mFrameType);

                    }
                else if(( CameraFrame::FRAME_DATA_SYNC == frame->mFrameType ) &&
                             ( NULL != mCameraHal.get() ) &&
                             ( NULL != mDataCb) &&
                             ( mCameraHal->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)  ))
                    {

                    if(!mAppSupportsStride)
                        {
                        buffer = mPreviewBuffers[mPreviewBufCount].get();
                        if(!buffer || !frame->mBuffer)
                            {
                            CAMHAL_LOGDA("Error! One of the buffer is NULL");
                            break;
                            }
                        }
                    else
                        {

                        memBase = mSharedPreviewBuffers.valueFor( ( unsigned int ) frame->mBuffer );

                        if( (NULL == memBase.get() ) || ( NULL == frame->mBuffer) )
                            {
                            CAMHAL_LOGDA("Error! One of the preview buffer is NULL");
                            break;
                            }
                        }

                    if(!mAppSupportsStride)
                        {

                        if ( buffer->size () >= frame->mLength )
                            {
                              buf = buffer->pointer();
                              if (buf)
                                memcpy(buf, ( void * )  frame->mBuffer, frame->mLength);
                            }
                        else
                            {
                              buf = buffer->pointer();
                              if (buf)
                                memset(buf, 0, buffer->size());
                            }

                        //Increment the buffer count
                        mPreviewBufCount = (mPreviewBufCount+1) % AppCallbackNotifier::MAX_BUFFERS;

                        memBase = buffer;
                        }

                    ///Give preview callback to app
                    mDataCb(CAMERA_MSG_PREVIEW_FRAME, memBase, mCallbackCookie);

                    mFrameProvider->returnFrame(frame->mBuffer,  ( CameraFrame::FrameType ) frame->mFrameType);

                    }
                else
                    {
                    mFrameProvider->returnFrame(frame->mBuffer,  ( CameraFrame::FrameType ) frame->mFrameType);
                    CAMHAL_LOGDB("Frame type 0x%x is still unsupported!", frame->mFrameType);
                    }

                break;

        default:

            break;

        };

exit:

    if ( NULL != frame )
        {
        delete frame;
        }

    LOG_FUNCTION_NAME_EXIT
}

void AppCallbackNotifier::frameCallbackRelay(CameraFrame* caFrame)
{
    LOG_FUNCTION_NAME
    AppCallbackNotifier *appcbn = (AppCallbackNotifier*) (caFrame->mCookie);
    appcbn->frameCallback(caFrame);
    LOG_FUNCTION_NAME_EXIT
}

void AppCallbackNotifier::frameCallback(CameraFrame* caFrame)
{
    ///Post the event to the event queue of AppCallbackNotifier
    Message msg;
    CameraFrame *frame;

    LOG_FUNCTION_NAME

    if ( NULL != caFrame )
        {

        frame = new CameraFrame(*caFrame);
        if ( NULL != frame )
            {
            msg.command = AppCallbackNotifier::NOTIFIER_CMD_PROCESS_FRAME;
            msg.arg1 = frame;
            mFrameQ.put(&msg);
            }
        else
            {
            CAMHAL_LOGEA("Not enough resources to allocate CameraFrame");
            }

        }

    LOG_FUNCTION_NAME_EXIT
}


void AppCallbackNotifier::eventCallbackRelay(CameraHalEvent* chEvt)
{
    LOG_FUNCTION_NAME
    AppCallbackNotifier *appcbn = (AppCallbackNotifier*) (chEvt->mCookie);
    appcbn->eventCallback(chEvt);
    LOG_FUNCTION_NAME_EXIT
}

void AppCallbackNotifier::eventCallback(CameraHalEvent* chEvt)
{

    ///Post the event to the event queue of AppCallbackNotifier
    Message msg;
    CameraHalEvent *event;


    LOG_FUNCTION_NAME

    if ( NULL != chEvt )
        {

        event = new CameraHalEvent(*chEvt);
        if ( NULL != event )
            {
            msg.command = AppCallbackNotifier::NOTIFIER_CMD_PROCESS_EVENT;
            msg.arg1 = event;
            mEventQ.put(&msg);
            }
        else
            {
            CAMHAL_LOGEA("Not enough resources to allocate CameraHalEvent");
            }

        }

    LOG_FUNCTION_NAME_EXIT
}


bool AppCallbackNotifier::processMessage()
{
    ///Retrieve the command from the command queue and process it
    Message msg;

    LOG_FUNCTION_NAME

    CAMHAL_LOGDA("+Msg get...");
    mNotificationThread->msgQ().get(&msg);
    CAMHAL_LOGDA("-Msg get...");
    bool ret = true;

    switch(msg.command)
        {
        case NotificationThread::NOTIFIER_START:
            {
            CAMHAL_LOGDA("Received NOTIFIER_START command from Camera HAL");
            mNotifierState = AppCallbackNotifier::NOTIFIER_STARTED;
            break;
            }
        case NotificationThread::NOTIFIER_STOP:
            {
            CAMHAL_LOGDA("Received NOTIFIER_STOP command from Camera HAL");
            mNotifierState = AppCallbackNotifier::NOTIFIER_STOPPED;
            break;
            }
        case NotificationThread::NOTIFIER_EXIT:
            {
            CAMHAL_LOGDA("Received NOTIFIER_EXIT command from Camera HAL");
            mNotifierState = AppCallbackNotifier::NOTIFIER_EXITED;
            ret = false;
            break;
            }
        }


    ///Signal the semaphore if it is sent as part of the message
    if(msg.arg1)
        {
        CAMHAL_LOGDA("+Signalling semaphore from CameraHAL..");
        Semaphore &sem = *((Semaphore*)msg.arg1);
        sem.Signal();
        CAMHAL_LOGDA("-Signalling semaphore from CameraHAL..");
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;


}

AppCallbackNotifier::~AppCallbackNotifier()
{
    LOG_FUNCTION_NAME

    ///Stop app callback notifier if not already stopped
    stop();

    ///Unregister with the frame provider
    if ( NULL != mFrameProvider )
        {
        mFrameProvider->disableFrameNotification(CameraFrame::ALL_FRAMES);
        }

    //unregister with the event provider
    if ( NULL != mEventProvider )
        {
        mEventProvider->disableEventNotification(CameraHalEvent::ALL_EVENTS);
        }

    ///Kill the display thread
    Semaphore sem;
    sem.Create();
    Message msg;
    msg.command = NotificationThread::NOTIFIER_EXIT;

    //Send the semaphore to signal once the command is completed
    msg.arg1 = &sem;

    ///Post the message to display thread
    mNotificationThread->msgQ().put(&msg);

    ///Wait for the ACK - implies that the thread is now started and waiting for frames
    sem.Wait();

    //Exit and cleanup the thread
    mNotificationThread->requestExitAndWait();

    //Delete the display thread
    mNotificationThread.clear();


    ///Free the event and frame providers
    if ( NULL != mEventProvider )
        {
        ///Deleting the event provider
        CAMHAL_LOGDA("Stopping Event Provider");
        delete mEventProvider;
        mEventProvider = NULL;
        }

    if ( NULL != mFrameProvider )
        {
        ///Deleting the frame provider
        CAMHAL_LOGDA("Stopping Frame Provider");
        delete mFrameProvider;
        mFrameProvider = NULL;
        }

    releaseSharedVideoBuffers();

    LOG_FUNCTION_NAME_EXIT
}

//Free all video heaps and buffers
void AppCallbackNotifier::releaseSharedVideoBuffers()
{
    MemoryHeapBase *heap;
    MemoryBase *buffer;

    LOG_FUNCTION_NAME

    for ( unsigned int i = 0 ; i < mVideoHeaps.size() ; i ++ )
        {
        ((MemoryHeapBase*)mVideoHeaps.valueFor(mVideoHeaps.keyAt(i)))->decStrong(this);
        }

    for ( unsigned int i = 0 ; i < mVideoBuffers.size() ; i ++ )
        {
        ((MemoryBase *)mVideoBuffers.valueFor(mVideoBuffers.keyAt(i)))->decStrong(this);
        }

    mVideoHeaps.clear();
    mVideoBuffers.clear();
    mVideoMap.clear();

    LOG_FUNCTION_NAME_EXIT
}


void AppCallbackNotifier::setEventProvider(int32_t eventMask, MessageNotifier * eventNotifier)
{

    LOG_FUNCTION_NAME
    ///@remarks There is no NULL check here. We will check
    ///for NULL when we get start command from CameraHal
    ///@Remarks Currently only one event provider (CameraAdapter) is supported
    ///@todo Have an array of event providers for each event bitmask
    mEventProvider = new EventProvider(eventNotifier, this, eventCallbackRelay);
    if ( NULL == mEventProvider )
        {
        CAMHAL_LOGEA("Error in creating EventProvider");
        }
    else
        {
        mEventProvider->enableEventNotification(eventMask);
        }

    LOG_FUNCTION_NAME_EXIT
}

void AppCallbackNotifier::setFrameProvider(FrameNotifier *frameNotifier)
{
    LOG_FUNCTION_NAME
    ///@remarks There is no NULL check here. We will check
    ///for NULL when we get the start command from CameraAdapter
    mFrameProvider = new FrameProvider(frameNotifier, this, frameCallbackRelay);
    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Error in creating FrameProvider");
        }
    else
        {
        //Register only for captured images and RAW for now
        //TODO: Register for and handle all types of frames
        mFrameProvider->enableFrameNotification(CameraFrame::IMAGE_FRAME);
        mFrameProvider->enableFrameNotification(CameraFrame::RAW_FRAME);
        }

    LOG_FUNCTION_NAME_EXIT
}

status_t AppCallbackNotifier::startPreviewCallbacks(CameraParameters &params, void *buffers, uint32_t *offsets, int fd, size_t length, size_t count)
{
    sp<MemoryHeapBase> heap;
    sp<MemoryBase> buffer;
    status_t ret = NO_ERROR;
    unsigned int *bufArr;
    size_t size = 0;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to start video recording without FrameProvider");
        return -1;
        }

    if ( mPreviewing )
        {
        CAMHAL_LOGEA("+Already previewing");
        return -1;
        }

    CAMHAL_LOGDB("app-stride-aware %d", params.getInt("app-stride-aware"));

     if(params.getInt("app-stride-aware")!=-1)
         {
         CAMHAL_LOGDA("App is stride aware");
         mAppSupportsStride = true;
         }
     else
         {
         CAMHAL_LOGDA("App is not stride aware");
         mAppSupportsStride = false;
         }


    int w,h;
    ///Get preview size
    params.getPreviewSize(&w, &h);

    //Get the preview pixel format
    mPreviewPixelFormat = params.getPreviewFormat();

     if(strcmp(mPreviewPixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) == 0)
        {
        size = w*h*2;
        mPreviewPixelFormat = CameraParameters::PIXEL_FORMAT_YUV422I;
        }
    else if(strcmp(mPreviewPixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_YUV420SP) == 0)
        {
        size = (w*h*3)/2;
        mPreviewPixelFormat = CameraParameters::PIXEL_FORMAT_YUV420SP;
        }
    else if(strcmp(mPreviewPixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_RGB565) == 0)
        {
        size = w*h*2;
        mPreviewPixelFormat = CameraParameters::PIXEL_FORMAT_RGB565;
        }

   if(!mAppSupportsStride)
       {
        mPreviewHeap = new MemoryHeapBase(size*AppCallbackNotifier::MAX_BUFFERS);
        if(!mPreviewHeap.get())
            {
            return NO_MEMORY;
            }

        for(int i=0;i<AppCallbackNotifier::MAX_BUFFERS;i++)
            {
            mPreviewBuffers[i] = new MemoryBase(mPreviewHeap,size*i, size);
            if(!mPreviewBuffers[i].get())
                {
                for(int j=0;j<i-1;j++)
                    {
                    mPreviewBuffers[j].clear();
                    }
                mPreviewHeap.clear();
                return NO_MEMORY;
                }
            }
        }
    else
        {
        bufArr = ( unsigned int * ) buffers;
        for ( unsigned int i = 0 ; i < count ; i ++ )
            {
            heap = new MemoryHeapBase(fd, length, 0, offsets[i]);
            if ( NULL == heap.get() )
                {
                CAMHAL_LOGEB("Unable to map a memory heap to preview frame 0x%x", ( unsigned int ) bufArr[i]);
                ret = -1;
                goto exit;
                }

#ifdef DEBUG_LOG

            CAMHAL_LOGEB("New memory heap 0x%x for preview frame 0x%x", ( unsigned int ) heap.get(), ( unsigned int ) bufArr[i]);

#endif

            buffer = new MemoryBase(heap, 0, length);
            if ( NULL == buffer.get() )
                {
                CAMHAL_LOGEB("Unable to initialize a memory base to preview frame 0x%x", ( unsigned int ) bufArr[i]);
                heap->dispose();
                heap.clear();
                ret = -1;
                goto exit;
                }

#ifdef DEBUG_LOG

            CAMHAL_LOGEB("New memory buffer 0x%x for preview frame 0x%x ", ( unsigned int ) buffer.get(), ( unsigned int ) bufArr[i]);

#endif

            mSharedPreviewHeaps.add( bufArr[i], heap);
            mSharedPreviewBuffers.add( bufArr[i], buffer);
            }
        }

    if ( (NO_ERROR == ret)  && mCameraHal->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME))
        {
         mFrameProvider->enableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);
        }

    mPreviewBufCount = 0;

    mPreviewing = true;

    LOG_FUNCTION_NAME

    exit:

    return ret;

}

sp<IMemoryHeap> AppCallbackNotifier::getPreviewHeap()
{

    CAMHAL_LOGDA("getPreviewHeap");
    if(mAppSupportsStride)
        {
        return NULL; //mPreviewHeap; We dont have one single heap
        }
    else
        {
        return mPreviewHeap;
        }
}

void AppCallbackNotifier::setBurst(bool burst)
{
    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mBurstLock);

    mBurst = burst;

    LOG_FUNCTION_NAME_EXIT
}

status_t AppCallbackNotifier::stopPreviewCallbacks()
{
    status_t ret = NO_ERROR;
    sp<MemoryHeapBase> heap;
    sp<MemoryBase> buffer;

    LOG_FUNCTION_NAME

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to stop preview callbacks without FrameProvider");
        ret = -1;
        }

    if(!mPreviewing)
        {
        return -1;
        }

    if ( NO_ERROR == ret )
        {
         mFrameProvider->disableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);
        }

    bool alreadyStopped = false;

    if(mAppSupportsStride)
        {
        for ( unsigned int i = 0 ; i < mSharedPreviewHeaps.size() ; i++ )
            {
            //Delete the instance
            heap = mSharedPreviewHeaps.valueAt(i);
            buffer = mSharedPreviewBuffers.valueAt(i);
            heap.clear();
            buffer.clear();
            }

        mSharedPreviewHeaps.clear();
        mSharedPreviewBuffers.clear();
        }
    else
        {
        for(int i=0;i<AppCallbackNotifier::MAX_BUFFERS;i++)
            {
            //Delete the instance
            mPreviewBuffers[i].clear();
            }

        mPreviewHeap.clear();
        }

    mPreviewing = false;

    LOG_FUNCTION_NAME_EXIT

    CAMHAL_LOGDA("- Exiting stopPreviewCallbacks");
    return ret;

}

status_t AppCallbackNotifier::startRecording()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

   Mutex::Autolock lock(mRecordingLock);

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to start video recording without FrameProvider");
        ret = -1;
        }

    if(mRecording)
        {
        return NO_INIT;
        }

    if ( NO_ERROR == ret )
        {
         mFrameProvider->enableFrameNotification(CameraFrame::VIDEO_FRAME_SYNC);
        }

    mRecording = true;

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t AppCallbackNotifier::initSharedVideoBuffers(void *buffers, uint32_t *offsets, int fd, size_t length, size_t count)
{
    MemoryHeapBase *heap;
    MemoryBase *buffer;
    status_t ret = NO_ERROR;
    unsigned int *bufArr;

    LOG_FUNCTION_NAME

    bufArr = ( unsigned int * ) buffers;
    for ( unsigned int i = 0 ; i < count ; i ++ )
        {
        heap = new MemoryHeapBase(fd, length, 0, offsets[i]);
        if ( NULL == heap )
            {
            CAMHAL_LOGEB("Unable to map a memory heap to frame 0x%x", bufArr[i]);
            ret = -1;
            goto exit;
            }
        heap->incStrong(this);

#ifdef DEBUG_LOG

        CAMHAL_LOGEB("New memory heap 0x%x for frame 0x%x", (unsigned int)heap, bufArr[i]);

#endif

        buffer = new MemoryBase(heap, 0, length);
        if ( NULL == buffer )
            {
            CAMHAL_LOGEB("Unable to initialize a memory base to frame 0x%x", bufArr[i]);
            CAMHAL_LOGEB("Unable to initialize a memory base to frame 0x%x", ( unsigned int ) bufArr[i]);
            heap->dispose();
            ret = -1;
            goto exit;
            }
        buffer->incStrong(this);

#ifdef DEBUG_LOG

        CAMHAL_LOGEB("New memory buffer 0x%x for frame 0x%x ", (unsigned int)buffer, bufArr[i]);

#endif

        mVideoHeaps.add( bufArr[i], ( unsigned int ) heap);
        mVideoBuffers.add( bufArr[i], ( unsigned int ) buffer);
        mVideoMap.add( ( unsigned int ) buffer->pointer(), bufArr[i]);
        }

exit:

    LOG_FUNCTION_NAME_EXIT

    return ret;
}


status_t AppCallbackNotifier::stopRecording()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mRecordingLock);

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to stop video recording without FrameProvider");
        ret = -1;
        }

    if(!mRecording)
        {
        return NO_INIT;
        }

    if ( NO_ERROR == ret )
        {
         mFrameProvider->disableFrameNotification(CameraFrame::VIDEO_FRAME_SYNC);
        }

    ///Release the shared video buffers
    releaseSharedVideoBuffers();

    mRecording = false;

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t AppCallbackNotifier::releaseRecordingFrame(const sp < IMemory > & mem)
{
    status_t ret = NO_ERROR;
    void *frame;

    LOG_FUNCTION_NAME

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to stop video recording without FrameProvider");
        ret = -1;
        }

    if ( NULL == mem.get() )
        {
        CAMHAL_LOGEA("Video Frame released is invalid");
        ret = -1;
        }

    frame = ( void * ) mVideoMap.valueFor( (unsigned int) mem->pointer());

    if ( NO_ERROR == ret )
        {
         mFrameProvider->returnFrame(frame, CameraFrame::VIDEO_FRAME_SYNC);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t AppCallbackNotifier::enableMsgType(int32_t msgType)
{
    if(msgType & CAMERA_MSG_POSTVIEW_FRAME)
        {
        mFrameProvider->enableFrameNotification(CameraFrame::SNAPSHOT_FRAME);
        }

    if(msgType & CAMERA_MSG_PREVIEW_FRAME)
        {
         mFrameProvider->enableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);
        }

    return NO_ERROR;
}

status_t AppCallbackNotifier::disableMsgType(int32_t msgType)
{
    if(msgType & CAMERA_MSG_POSTVIEW_FRAME)
        {
        mFrameProvider->disableFrameNotification(CameraFrame::SNAPSHOT_FRAME);
        }

    if(msgType & CAMERA_MSG_PREVIEW_FRAME)
        {
        mFrameProvider->disableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);
        }

    return NO_ERROR;

}

status_t AppCallbackNotifier::start()
{
    LOG_FUNCTION_NAME
    if(mNotifierState==AppCallbackNotifier::NOTIFIER_STARTED)
        {
        CAMHAL_LOGDA("AppCallbackNotifier already running");
        LOG_FUNCTION_NAME_EXIT
        return ALREADY_EXISTS;
        }

    ///Check whether initial conditions are met for us to start
    ///A frame provider should be available, if not return error
    if(!mFrameProvider)
        {
        ///AppCallbackNotifier not properly initialized
        CAMHAL_LOGEA("AppCallbackNotifier not properly initialized - Frame provider is NULL");
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
        }

    ///At least one event notifier should be available, if not return error
    ///@todo Modify here when there is an array of event providers
    if(!mEventProvider)
        {
        CAMHAL_LOGEA("AppCallbackNotifier not properly initialized - Event provider is NULL");
        LOG_FUNCTION_NAME_EXIT
        ///AppCallbackNotifier not properly initialized
        return NO_INIT;
        }

    ///Send a message to the callback notifier thread to start listening for messages
    //Send START_DISPLAY COMMAND to display thread. Display thread will start and then wait for a message
    Semaphore sem;
    sem.Create();
    Message msg;
    msg.command = NotificationThread::NOTIFIER_START;

    //Send the semaphore to signal once the command is completed
    msg.arg1 = &sem;

    ///Post the message to display thread
    mNotificationThread->msgQ().put(&msg);

    ///Wait for the ACK - implies that the thread is now started and waiting for frames
    CAMHAL_LOGDA("Waiting for ACK from Notification thread");
    sem.Wait();
    CAMHAL_LOGDA("Got ACK from Notification thread");

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;

}

status_t AppCallbackNotifier::stop()
{
    LOG_FUNCTION_NAME

    if(mNotifierState!=AppCallbackNotifier::NOTIFIER_STARTED)
        {
        CAMHAL_LOGEA("AppCallbackNotifier already in stopped state");
        LOG_FUNCTION_NAME_EXIT
        return ALREADY_EXISTS;
        }

    ///Send a notification to the callback thread to stop sending the notifications to the
    ///application

    Semaphore sem;
    sem.Create();
    Message msg;
    msg.command = NotificationThread::NOTIFIER_STOP;

    //Send the semaphore to signal once the command is completed
    msg.arg1 = &sem;

    ///Post the message to display thread
    mNotificationThread->msgQ().put(&msg);

    ///Wait for the ACK for display to be disabled
    sem.Wait();
    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR;
}


/*--------------------NotificationHandler Class ENDS here-----------------------------*/



};

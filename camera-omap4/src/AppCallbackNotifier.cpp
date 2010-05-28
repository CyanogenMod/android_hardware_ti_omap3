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

const int32_t AppCallbackNotifier::MAX_BUFFERS = 20;



/*--------------------NotificationHandler Class STARTS here-----------------------------*/

/**
  * NotificationHandler class
  */


///Initialization function for AppCallbackNotifier
status_t AppCallbackNotifier::initialize()
{
    LOG_FUNCTION_NAME
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

//All sub-components of Camera HAL call this whenever any error happens
void AppCallbackNotifier::errorNotify(int error)
{
    LOG_FUNCTION_NAME

    CAMHAL_LOGEB("AppCallbackNotifier received error %d", error);

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
        CAMHAL_LOGDA("Notification Thread waiting for message");
        if(mNotifierState==AppCallbackNotifier::NOTIFIER_STARTED)
            {
        ret = MessageQueue::waitForMsg(&mNotificationThread->msgQ()
                                                        , &mEventQ
                                                        , &mFrameQ
                                                        , AppCallbackNotifier::NOTIFIER_TIMEOUT);
            }
        else
            {
            ret = MessageQueue::waitForMsg(&mNotificationThread->msgQ()
                                                            , NULL
                                                            , NULL
                                                            , AppCallbackNotifier::NOTIFIER_TIMEOUT);

            }

        CAMHAL_LOGDA("Notification Thread received message");

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
            CAMHAL_LOGDA("Notification Thread received a frame from frame provider (CameraAdapter)");
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
    CameraHalEvent* evt;
    CameraHalEvent::FocusEventData *focusEvtData;
    CameraHalEvent::ZoomEventData *zoomEvtData;

    switch(msg.command)
        {
        case AppCallbackNotifier::NOTIFIER_CMD_PROCESS_EVENT:

            evt = (CameraHalEvent*)msg.arg1;

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

                    focusEvtData = (CameraHalEvent::FocusEventData *) evt->mEventData;
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

                case CameraHalEvent::EVENT_ZOOM_LEVEL_REACHED:

                    zoomEvtData = (CameraHalEvent::ZoomEventData *) evt->mEventData;
                    ///@todo Send callback to application for zoom level
                    ///zoomEvtData.currentZoomValue;
                    ///zoomEvtData.targetZoomLevelReached;

                    break;

                case CameraHalEvent::ALL_EVENTS:
                    break;
                default:
                    break;
                }

            break;
        case AppCallbackNotifier::NOTIFIER_CMD_PROCESS_ERROR:
            ///@todo send error notification to the app, if error notification flag is enabled
            break;
        }

    LOG_FUNCTION_NAME_EXIT

}

static void copy2Dto1D(void *dst, void *src, int width, int height, size_t stride, unsigned int bytesPerPixel, size_t length,
    const char *pixelFormat)
{
    unsigned int alignedRow, row;
    unsigned char *bufferDst, *bufferSrc;
    uint16_t *bufferDst_UV;


    if(pixelFormat!=NULL)
        {
        if(strcmp(pixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) == 0)
            {
            CAMHAL_LOGDA("YUV422I");
            bytesPerPixel = 2;
            }
        else if(strcmp(pixelFormat, (const char *) CameraParameters::PIXEL_FORMAT_YUV420SP) == 0)
            {
            //Convert here from NV12 to NV21 and return
            bytesPerPixel = 1;
    bufferDst = ( unsigned char * ) dst;
    bufferSrc = ( unsigned char * ) src;
    row = width*bytesPerPixel;
    alignedRow = ( row + ( stride -1 ) ) & ( ~ ( stride -1 ) );

    //iterate through each row
    for ( int i = 0 ; i < height ; i++,  bufferSrc += alignedRow, bufferDst += row)
        {
        memcpy(bufferDst, bufferSrc, row);
        }

            ///Convert NV21 to NV12 by swapping U & V
            bufferDst_UV = (uint16_t *) bufferDst;
            for(int i = 0 ; i < height/2 ; i++)
                {
                for(int j=0;j<width/2;j++)
                    {
                    *bufferDst_UV++ = (*bufferSrc<<16) | *(bufferSrc+1);
                     bufferSrc +=2;
                    }
                bufferSrc += alignedRow;
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
    MemoryBase *buffer;

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
                    //TODO: Find a way to map a Tiler buffer to a MemoryHeapBase
                    //Send NULL for now
                    mDataCb(CAMERA_MSG_RAW_IMAGE, NULL,mCallbackCookie);
                    }
                else if ( ( CameraFrame::IMAGE_FRAME == frame->mFrameType ) &&
                             ( NULL != mCameraHal.get() ) &&
                             ( NULL != mDataCb) &&
                             ( mCameraHal->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE)  ) )
                    {

#ifdef COPY_IMAGE_BUFFER

                    sp<MemoryHeapBase> JPEGPictureHeap = new MemoryHeapBase(frame->mLength);
                    sp<MemoryBase> JPEGPictureMemBase = new MemoryBase(JPEGPictureHeap, 0, frame->mLength);
                    memcpy(JPEGPictureMemBase->pointer(), frame->mBuffer, frame->mLength);

#else

                     //TODO: Find a way to map a Tiler buffer to a MemoryHeapBase
                     //Send NULL for now
                     sp<MemoryHeapBase> JPEGPictureHeap = new MemoryHeapBase( 256);
                     sp<MemoryBase> JPEGPictureMemBase = new MemoryBase(JPEGPictureHeap, 0, 256);

#endif

                    mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, JPEGPictureMemBase, mCallbackCookie);
                    }
                else if ( ( CameraFrame::VIDEO_FRAME_SYNC == frame->mFrameType ) &&
                             ( NULL != mCameraHal.get() ) &&
                             ( NULL != mDataCb) &&
                             ( mCameraHal->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)  ) )
                    {

#ifdef COPY_VIDEO_BUFFER

                        {
                        Mutex::Autolock lock(mLock);
                        if ( mBufferReleased )
                            {

                            heap = new MemoryHeapBase(frame->mLength);
                            if ( NULL == heap )
                                {
                                CAMHAL_LOGEA("Unable to allocate memory heap");
                                ret = -1;
                                goto exit;
                                }

                            buffer = new MemoryBase(heap, 0, frame->mLength);
                            if ( NULL == buffer )
                                {
                                CAMHAL_LOGEA("Unable to allocate a memory buffer");
                                ret = -1;
                                goto exit;
                                }

                            //TODO: Suport other pixelformats
                            copy2Dto1D(buffer->pointer(), frame->mBuffer, frame->mWidth, frame->mHeight, frame->mAlignment, 2
                                                                                                , frame->mLength, NULL);

                            mDataCbTimestamp(frame->mTimestamp, CAMERA_MSG_VIDEO_FRAME, buffer, mCallbackCookie);

                            mBufferReleased = false;
                            }
                        }

                    mFrameProvider->returnFrame(frame->mBuffer,  ( CameraFrame::FrameType ) frame->mFrameType);

#else

                    buffer = ( MemoryBase * ) mVideoBuffers.valueFor( ( unsigned int ) frame->mBuffer );

                    mDataCbTimestamp(frame->mTimestamp, CAMERA_MSG_VIDEO_FRAME, buffer, mCallbackCookie);

#endif

                    }
                else if(( CameraFrame::PREVIEW_FRAME_SYNC== frame->mFrameType ) &&
                             ( NULL != mCameraHal.get() ) &&
                             ( NULL != mDataCb) &&
                             ( mCameraHal->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)  ))
                    {
                    buffer = mPreviewBuffers.valueAt(mPreviewBufCount);
                    if(!buffer || !frame->mBuffer)
                        {
                        CAMHAL_LOGDA("Error! One of the buffer is NULL");
                        break;
                        }

                    ///CAMHAL_LOGDB("+Copy 0x%x to 0x%x", buffer->pointer(), frame->mBuffer);
                    ///Copy the data into 1-D buffer
                    copy2Dto1D(buffer->pointer(), frame->mBuffer, frame->mWidth, frame->mHeight, frame->mAlignment, 2, frame->mLength,
                                mPreviewPixelFormat);
                    ///CAMHAL_LOGDA("-Copy");

                    //Increment the buffer count
                    mPreviewBufCount = (mPreviewBufCount+1) % AppCallbackNotifier::MAX_BUFFERS;

                    ///Give preview callback to app
                    mDataCb(CAMERA_MSG_PREVIEW_FRAME, buffer, mCallbackCookie);

                    }
                else
                    {
                    CAMHAL_LOGEB("Frame type 0x%x is still unsupported!", frame->mFrameType);
                    mFrameProvider->returnFrame(frame->mBuffer,  ( CameraFrame::FrameType ) frame->mFrameType);
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

    LOG_FUNCTION_NAME

    msg.command = AppCallbackNotifier::NOTIFIER_CMD_PROCESS_EVENT;
    msg.arg1 = chEvt;

    if(mNotifierState == AppCallbackNotifier::NOTIFIER_STARTED)
        {
        mEventQ.put(&msg);
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

    for ( unsigned int i = 0 ; i < mVideoHeaps.size() ; i++ )
        {
        heap = ( MemoryHeapBase * ) mVideoHeaps.valueAt(i);
        heap->dispose();
        }

    mVideoHeaps.clear();
    mVideoBuffers.clear();

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

status_t AppCallbackNotifier::startPreviewCallbacks(CameraParameters &params)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to start video recording without FrameProvider");
        return -1;
        }

    if(mPreviewing)
        {
        return -1;
        }

    int w,h;
    params.getPreviewSize(&w, &h);
    mPreviewHeap = new MemoryHeapBase(w*h*2*AppCallbackNotifier::MAX_BUFFERS);
    mPreviewHeap->incStrong(this);
    if(!mPreviewHeap)
        {
        return NO_MEMORY;
        }

    for(int i=0;i<AppCallbackNotifier::MAX_BUFFERS;i++)
        {
            MemoryBase * mBase = new MemoryBase(mPreviewHeap,w*h*2*i, w*h*2 );
            if(!mBase)
                {
                mPreviewHeap->dispose();
                for(int i=0;i<mPreviewBuffers.size();i++)
                    {
                    MemoryBase *mBase = mPreviewBuffers.valueAt(i);
                    //Delete the instance
                    mBase->decStrong(this);

                    }
                mPreviewHeap->decStrong(this);
                return NO_MEMORY;
                }
            mBase->incStrong(this);
            mPreviewBuffers.add(i, mBase);
        }

    //Get the preview pixel format
    mPreviewPixelFormat = params.getPreviewFormat();

    if ( NO_ERROR == ret )
        {
         mFrameProvider->enableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);
        }

    mPreviewBufCount = 0;

    mPreviewing = true;

    return ret;

}

sp<IMemoryHeap> AppCallbackNotifier::getPreviewHeap()
{

    CAMHAL_LOGDA("getPreviewHeap");

    return mPreviewHeap;
}


status_t AppCallbackNotifier::stopPreviewCallbacks()
{
    status_t ret = NO_ERROR;

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

    mPreviewHeap = NULL;
    mPreviewBuffers.clear();

    mPreviewing = false;

    LOG_FUNCTION_NAME_EXIT

    return ret;

}

status_t AppCallbackNotifier::startRecording()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to start video recording without FrameProvider");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {
         mFrameProvider->enableFrameNotification(CameraFrame::VIDEO_FRAME_SYNC);
        }

#ifdef COPY_VIDEO_BUFFER

    mBufferReleased = true;

#endif

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

    releaseSharedVideoBuffers();

#ifndef COPY_VIDEO_BUFFER

    bufArr = ( unsigned int * ) buffers;
    for ( unsigned int i = 0 ; i < count ; i ++ )
        {
        heap = new MemoryHeapBase(fd, length, 0, offsets[i]);
        if ( NULL == heap )
            {
            CAMHAL_LOGEB("Unable to map a memory heap to frame 0x%x", ( void * ) bufArr[i]);
            ret = -1;
            goto exit;
            }

#ifdef DEBUG_LOG

        CAMHAL_LOGEB("New memory heap 0x%x for frame 0x%x", heap, ( void * ) bufArr[i]);

#endif

        buffer = new MemoryBase(heap, 0, length);
        if ( NULL == buffer )
            {
            CAMHAL_LOGEB("Unable to initialize a memory base to frame 0x%x", ( void * ) bufArr[i]);
            heap->dispose();
            ret = -1;
            goto exit;
            }

#ifdef DEBUG_LOG

        CAMHAL_LOGEB("New memory buffer 0x%x for frame 0x%x ", buffer, ( void * ) bufArr[i]);

#endif

        mVideoHeaps.add( bufArr[i], ( unsigned int ) heap);
        mVideoBuffers.add( bufArr[i], ( unsigned int ) buffer);
        }

#endif

exit:

    LOG_FUNCTION_NAME_EXIT

    return ret;
}


status_t AppCallbackNotifier::stopRecording()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NULL == mFrameProvider )
        {
        CAMHAL_LOGEA("Trying to stop video recording without FrameProvider");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {
         mFrameProvider->disableFrameNotification(CameraFrame::VIDEO_FRAME_SYNC);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t AppCallbackNotifier::releaseRecordingFrame(const sp < IMemory > & mem)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

#ifdef COPY_VIDEO_BUFFER

        {
        Mutex::Autolock lock(mLock);
        mBufferReleased = true;
        }

#else

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

    if ( NO_ERROR == ret )
        {
         mFrameProvider->returnFrame(mem->pointer(), CameraFrame::VIDEO_FRAME_SYNC);
        }

#endif

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t AppCallbackNotifier::start()
{
    LOG_FUNCTION_NAME
    if(mNotifierState==AppCallbackNotifier::NOTIFIER_STARTED)
        {
        CAMHAL_LOGEA("AppCallbackNotifier already running");
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

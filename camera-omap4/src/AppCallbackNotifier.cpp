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
        ret = MessageQueue::waitForMsg(&mNotificationThread->msgQ()
                                                        , &mEventQ
                                                        , &mFrameQ
                                                        , AppCallbackNotifier::NOTIFIER_TIMEOUT);

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

void AppCallbackNotifier::notifyFrame()
{
    ///Receive and send the frame notifications to app
    Message msg;
    CameraFrame *frame;

    LOG_FUNCTION_NAME

    mFrameQ.get(&msg);
    bool ret = true;

    switch(msg.command)
        {
        case AppCallbackNotifier::NOTIFIER_CMD_PROCESS_FRAME:

                frame = (CameraFrame *) msg.arg1;
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
                     //TODO: Find a way to map a Tiler buffer to a MemoryHeapBase
                     //Send NULL for now
                     sp<MemoryHeapBase> JPEGPictureHeap = new MemoryHeapBase( 256);
                     sp<MemoryBase> JPEGPictureMemBase = new MemoryBase(JPEGPictureHeap, 0, 256);

                    mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, JPEGPictureMemBase, mCallbackCookie);
                    }
                else
                    {
                    CAMHAL_LOGEB("Frame type 0x%x is still unsupported!", frame->mFrameType);
                    }

                break;

        default:

            break;

        };

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
    LOG_FUNCTION_NAME
    msg.command = AppCallbackNotifier::NOTIFIER_CMD_PROCESS_FRAME;
    msg.arg1 = caFrame;
    mFrameQ.put(&msg);
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
    mFrameProvider->disableFrameNotification(CameraFrame::ALL_FRAMES);

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
    if(mEventProvider)
        {
        ///Deleting the event provider
        CAMHAL_LOGDA("Stopping Event Provider");
        delete mEventProvider;
        mEventProvider = NULL;
        }

    if(mFrameProvider)
        {
        ///Deleting the frame provider
        CAMHAL_LOGDA("Stopping Frame Provider");
        delete mFrameProvider;
        mFrameProvider = NULL;
        }


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
    if(!mEventProvider)
        {
        CAMHAL_LOGEA("Error in creating EventProvider");
        }

    mEventProvider->enableEventNotification(eventMask);

    LOG_FUNCTION_NAME_EXIT
}

void AppCallbackNotifier::setFrameProvider(FrameNotifier *frameNotifier)
{
    LOG_FUNCTION_NAME
    ///@remarks There is no NULL check here. We will check
    ///for NULL when we get the start command from CameraAdapter
    mFrameProvider = new FrameProvider(frameNotifier, this, frameCallbackRelay);
    if(!mFrameProvider)
        {
        CAMHAL_LOGEA("Error in creating FrameProvider");
        }

    //Register only for captured images and RAW for now
    //TODO: Register for and handle all types of frames
    mFrameProvider->enableFrameNotification(CameraFrame::IMAGE_FRAME);
    mFrameProvider->enableFrameNotification(CameraFrame::RAW_FRAME);

    LOG_FUNCTION_NAME_EXIT
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

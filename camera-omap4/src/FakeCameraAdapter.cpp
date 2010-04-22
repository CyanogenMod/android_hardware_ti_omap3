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


/*--------------------Camera Adapter Class STARTS here-----------------------------*/

FakeCameraAdapter::FakeCameraAdapter()
{
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

    mPreviewBuffersAvailable.clear();
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
    int *bufArr, num;
    Message msg;

    LOG_FUNCTION_NAME

    switch ( operation )
        {
        case CameraAdapter::CAMERA_USE_BUFFERS:

            CAMHAL_LOGDA("Use Buffers");
            mode = ( CameraAdapter::CameraMode ) value1;
            bufArr = (int *) value2;
            num = value3;

            if ( CameraAdapter::CAMERA_PREVIEW == mode )
                {
                if ( NULL == bufArr )
                    {
                    CAMHAL_LOGEA("Invalid preview buffers!");
                    ret = -1;
                    }

                if ( ret == NO_ERROR )
                    {
                    Mutex::Autolock lock(mPreviewBufferLock);
                    mPreviewBufferCount = num;
                    mPreviewBuffers = (int *) bufArr;

                    mPreviewBuffersAvailable.clear();
                    for ( int i = 0 ; i < mPreviewBufferCount ; i++ )
                        {
                        mPreviewBuffersAvailable.add(mPreviewBuffers[i], true);
                        }
                    }

                }
            else
                {
                CAMHAL_LOGEB("Camera Mode %x still not supported!", mode);
                }

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
}

void FakeCameraAdapter::frameCallbackThread()
{
    bool shouldLive = true;
    Message msg;
    frame_callback callback;
    CameraFrame cFrame;

    LOG_FUNCTION_NAME

    while ( shouldLive )
        {
        MessageQueue::waitForMsg(&mCallbackQ, NULL, NULL, -1);
        mCallbackQ.get(&msg);

        if ( FakeCameraAdapter::CALL_CALLBACK == msg.command )
            {
            cFrame.mBuffer = msg.arg1;
            cFrame.mCookie = msg.arg2;
            callback = (frame_callback) msg.arg3;

            callback(&cFrame);

            }
        else if ( FakeCameraAdapter::CALLBACK_EXIT == msg.command  )
            {
            shouldLive = false;
            }
        }
    LOG_FUNCTION_NAME_EXIT
}

void FakeCameraAdapter::frameThread()
{
    bool shouldLive = true;
    int timeout;
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
                    sendNextFrame();
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
                                mPreviewBuffersAvailable.replaceValueFor((int) msg.arg1, true);
                                }
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
                            mPreviewBuffersAvailable.replaceValueFor((int) msg.arg1, true);
                            }
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


void FakeCameraAdapter::sendNextFrame()
{
    void *previewBuffer;
    Message msg;
    int i = 0;

    previewBuffer = NULL;
        {
        Mutex::Autolock lock(mPreviewBufferLock);
        //check for any buffers available
        for ( int i = 0 ; i < mPreviewBufferCount ; i++ )
            {
            if ( mPreviewBuffersAvailable.valueAt(i) )
                {
                previewBuffer = (void *) mPreviewBuffers[i];
                mPreviewBuffersAvailable.replaceValueAt(i, false);
                break;
                }
            }
        }

    if ( NULL == previewBuffer )
        {
        return;
        }

        {
        Mutex::Autolock lock(mSubscriberLock);
        //check for any subscribers
        if ( 0 == mFrameSubscribers.size() )
            {
                {
                Mutex::Autolock lock(mPreviewBufferLock);
                //release buffers in case nobody subscribed for it
                mPreviewBuffersAvailable.replaceValueAt(i, true);
                }
            }
        else
            {
            for (unsigned int i = 0 ; i < mFrameSubscribers.size(); i++ )
                {
                msg.command = FakeCameraAdapter::CALL_CALLBACK;
                msg.arg1 = previewBuffer;
                msg.arg2 = (void *) mFrameSubscribers.keyAt(i);
                msg.arg3 = (void *) mFrameSubscribers.valueAt(i);

                mCallbackQ.put(&msg);
                }
            }
        }
}

extern "C" CameraAdapter* CameraAdapter_Factory() {
    FakeCameraAdapter *ret;

    LOG_FUNCTION_NAME

    ret = new FakeCameraAdapter();

    if ( NULL != ret )
        {
        ret->initialize();
        }

    return ret;
}



};


/*--------------------Camera Adapter Class ENDS here-----------------------------*/



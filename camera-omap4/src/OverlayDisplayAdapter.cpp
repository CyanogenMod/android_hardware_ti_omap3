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

#include "OverlayDisplayAdapter.h"

namespace android {

///Constant declarations
///@todo Check the time units
const int OverlayDisplayAdapter::DISPLAY_TIMEOUT = 1000; //seconds


/*--------------------OverlayDisplayAdapter Class STARTS here-----------------------------*/


/**
 * Display Adapter class STARTS here..
 */
OverlayDisplayAdapter::OverlayDisplayAdapter():mDisplayThread(NULL),
                                        mDisplayState(OverlayDisplayAdapter::DISPLAY_INIT),
                                        mFramesWithDisplay(0),
                                        mDisplayEnabled(false)


{
    LOG_FUNCTION_NAME
    LOG_FUNCTION_NAME_EXIT
}

OverlayDisplayAdapter::~OverlayDisplayAdapter()
{
    Semaphore sem;
    Message msg;

    LOG_FUNCTION_NAME

    ///If Frame provider exists
    if(mFrameProvider)
        {
        //Unregister with the frame provider
        mFrameProvider->disableFrameNotification(CameraFrame::ALL_FRAMES);
        }

    ///If Display thread exists
    if(mDisplayThread.get())
        {
        ///Kill the display thread
        sem.Create();
        msg.command = DisplayThread::DISPLAY_EXIT;

        //Send the semaphore to signal once the command is completed
        msg.arg1 = &sem;

        ///Post the message to display thread
        mDisplayThread->msgQ().put(&msg);

        ///Wait for the ACK - implies that the thread is now started and waiting for frames
        sem.Wait();

        //Exit and cleanup the thread
        mDisplayThread->requestExitAndWait();

        //Delete the display thread
        mDisplayThread.clear();
        }

    ///The overlay object will get destroyed here
    destroy();

    LOG_FUNCTION_NAME_EXIT

}

status_t OverlayDisplayAdapter::initialize()
{
    LOG_FUNCTION_NAME

    ///Create the display thread
    mDisplayThread = new DisplayThread(this);
    if ( !mDisplayThread.get() )
        {
        CAMHAL_LOGEA("Couldn't create display thread");
        LOG_FUNCTION_NAME_EXIT
        return NO_MEMORY;
        }

    ///Start the display thread
    status_t ret = mDisplayThread->run("DisplayThread", PRIORITY_URGENT_DISPLAY);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEA("Couldn't run display thread");
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}


int OverlayDisplayAdapter::setOverlay(const sp<Overlay> &overlay)
{
    LOG_FUNCTION_NAME
    ///Note that Display Adapter cannot work without a valid overlay object
    if ( !overlay.get() )
        {
        CAMHAL_LOGEA("NULL overlay object passed to DisplayAdapter");
        LOG_FUNCTION_NAME_EXIT
        return BAD_VALUE;
        }

    ///Destroy the existing overlay object, if it exists
    destroy();

    ///Move to new overlay obj
    mOverlay = overlay;

    ///Set the display queue file descriptor
    overlay_handle_t overlayhandle = mOverlay->getHandleRef();
    overlay_true_handle_t true_handle;
    if ( overlayhandle == NULL )
        {
        CAMHAL_LOGEA("Overlay handle is NULL");
        LOG_FUNCTION_NAME_EXIT
        return UNKNOWN_ERROR;
        }

    memcpy(&true_handle,overlayhandle,sizeof(overlay_true_handle_t));
    int overlayfd = true_handle.ctl_fd;

    mDisplayQ.setInFd(overlayfd);

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

int OverlayDisplayAdapter::setFrameProvider(FrameNotifier *frameProvider)
{
    LOG_FUNCTION_NAME

    //Check for NULL pointer
    if ( !frameProvider )
        {
        CAMHAL_LOGEA("NULL passed for frame provider");
        LOG_FUNCTION_NAME_EXIT
        return BAD_VALUE;
        }

    /** Dont do anything here, Just save the pointer for use when display is
         actually enabled or disabled
    */
    mFrameProvider = new FrameProvider(frameProvider, this, frameCallbackRelay);

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

int OverlayDisplayAdapter::setErrorHandler(ErrorNotifier *errorNotifier)
{
    LOG_FUNCTION_NAME

    //Check for NULL pointer
    if ( !mErrorNotifier )
        {
        CAMHAL_LOGEA("NULL passed for error handler");
        LOG_FUNCTION_NAME_EXIT

        return BAD_VALUE;
        }

    //Save the error notifier pointer to give notifications when the errors occur
    mErrorNotifier = errorNotifier;

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

int OverlayDisplayAdapter::enableDisplay()
{
    Semaphore sem;
    Message msg;

    LOG_FUNCTION_NAME

    if ( mDisplayEnabled )
        {
        CAMHAL_LOGDA("Display is already enabled");
        LOG_FUNCTION_NAME_EXIT

        return NO_ERROR;
        }

    //Send START_DISPLAY COMMAND to display thread. Display thread will start and then wait for a message
    sem.Create();
    msg.command = DisplayThread::DISPLAY_START;

    //Send the semaphore to signal once the command is completed
    msg.arg1 = &sem;

    ///Post the message to display thread
    mDisplayThread->msgQ().put(&msg);

    ///Wait for the ACK - implies that the thread is now started and waiting for frames
    sem.Wait();

    //Register with the frame provider for frames
    mFrameProvider->enableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);

    mDisplayEnabled = true;

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

int OverlayDisplayAdapter::disableDisplay()
{
    LOG_FUNCTION_NAME

    if(!mDisplayEnabled)
        {
        CAMHAL_LOGDA("Display is already disabled");
        LOG_FUNCTION_NAME_EXIT

        return ALREADY_EXISTS;
        }

    //Unregister with the frame provider here
    mFrameProvider->disableFrameNotification(CameraFrame::PREVIEW_FRAME_SYNC);

    //Send STOP_DISPLAY COMMAND to display thread. Display thread will stop and dequeue all messages
    //and then wait for message
    Semaphore sem;
    sem.Create();
    Message msg;
    msg.command = DisplayThread::DISPLAY_STOP;

    //Send the semaphore to signal once the command is completed
    msg.arg1 = &sem;

    ///Post the message to display thread
    mDisplayThread->msgQ().put(&msg);

    ///Wait for the ACK for display to be disabled
    sem.Wait();

    ///Reset the display enabled flag
    mDisplayEnabled = false;

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

void OverlayDisplayAdapter::destroy()
{
    LOG_FUNCTION_NAME

    ///Check if the display is disabled, if not disable it
    if ( mDisplayEnabled )
        {

        CAMHAL_LOGDA("WARNING: Calling destroy of Display adapter when display enabled. Disabling display..");
        disableDisplay();

        }

    ///Destroy the overlay object here
    if ( mOverlay != NULL )
        {
        mOverlay->destroy();
        }
    mOverlay = NULL;

    LOG_FUNCTION_NAME_EXIT
}

//Implementation of inherited interfaces
void* OverlayDisplayAdapter::allocateBuffer(int width, int height, const char* format, int bytes, int numBufs)
{
    LOG_FUNCTION_NAME

    ///We just return the buffers from Overlay, if the width and height are same, else (vstab, vnf case)
    ///re-allocate buffers using overlay and then get them
    ///@todo - Re-allocate buffers for vnf and vstab using the width, height, format, numBufs etc
    const int lnumBufs = (int)mOverlay->getBufferCount();
    int32_t *buffers = new int32_t[lnumBufs];
    if ( NULL == buffers )
        {
        CAMHAL_LOGEA("Couldn't create array for overlay buffers");
        LOG_FUNCTION_NAME_EXIT

        return NULL;
        }

    for ( int i=0; i < lnumBufs; i++ )
        {
        mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress((void*)i);
        if ( data == NULL )
            {
            CAMHAL_LOGEA(" getBufferAddress returned NULL");
            goto fail_loop;
            }

            buffers[i] = (int) data->ptr;
            mPreviewBufferMap.add(buffers[i], i);
        }

    return buffers;

fail_loop:

    CAMHAL_LOGEA("Error occurred, performing cleanup");
    if ( buffers )
        {
        delete [] buffers;
        }

    LOG_FUNCTION_NAME_EXIT
    return NULL;

}

int OverlayDisplayAdapter::freeBuffer(void* buf)
{
    LOG_FUNCTION_NAME

    ///@todo check whether below way of deleting is correct, else try to use malloc
    int *buffers = (int *) buf;
    if ( NULL != buf )
        {
        delete [] buffers;
        }

    mPreviewBufferMap.clear();

    //We don't have to do anything for free buffer since the buffers are managed by overlay
    return NO_ERROR;
}


bool OverlayDisplayAdapter::supportsExternalBuffering()
{
    ///@todo get this capability from the overlay reference and return it
    return false;
}

int OverlayDisplayAdapter::useBuffers(void *bufArr, int num)
{
    ///@todo implement logic to register the buffers with overlay, when external buffering support is added
    return NO_ERROR;
}

void OverlayDisplayAdapter::displayThread()
{
    bool shouldLive = true, noFramesLeft = false;
    int timeout = 0;
    status_t ret;

    LOG_FUNCTION_NAME

    while(shouldLive)
        {
        ret = MessageQueue::waitForMsg(&mDisplayThread->msgQ()
                                                                ,  &mDisplayQ
                                                                , NULL
                                                                , OverlayDisplayAdapter::DISPLAY_TIMEOUT);

        if ( !mDisplayThread->msgQ().isEmpty() )
            {
            ///Received a message from CameraHal, process it
            shouldLive = processHalMsg();

            }
        else  if( !mDisplayQ.isEmpty() && mFramesWithDisplay>NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE)
            {
            if ( mDisplayState== OverlayDisplayAdapter::DISPLAY_INIT )
                {

                ///If display adapter is not started, continue
                continue;

                }
            else
                {
                noFramesLeft = handleFrameReturn();

                if ( ( mDisplayState == OverlayDisplayAdapter::DISPLAY_EXITED ) && noFramesLeft)
                    {
                    ///we exit the thread only after all the frames have been returned from display
                    shouldLive = false;
                    }
                }
            }
        }

    LOG_FUNCTION_NAME_EXIT
}


bool OverlayDisplayAdapter::processHalMsg()
{
    Message msg;

    LOG_FUNCTION_NAME

    mDisplayThread->msgQ().get(&msg);
    bool ret = true, invalidCommand = false;

    switch ( msg.command )
        {

        case DisplayThread::DISPLAY_START:

            CAMHAL_LOGDA("Display thread received DISPLAY_START command from Camera HAL");
            mDisplayState = OverlayDisplayAdapter::DISPLAY_STARTED;

            break;

        case DisplayThread::DISPLAY_STOP:

            ///@bug There is no API to disable overlay without destroying it
            ///@bug Buffers might still be w/ display and will get displayed
            ///@remarks Ideal seqyence should be something like this
            ///mOverlay->setParameter("enabled", false);
            ///mFramesWithDisplay=0;
            CAMHAL_LOGDA("Display thread received DISPLAY_STOP command from Camera HAL");
            mDisplayState = OverlayDisplayAdapter::DISPLAY_STOPPED;

            break;

        case DisplayThread::DISPLAY_EXIT:

            CAMHAL_LOGDA("Display thread received DISPLAY_EXIT command from Camera HAL.");
            CAMHAL_LOGDA("Stopping display thread...");
            mDisplayState = OverlayDisplayAdapter::DISPLAY_EXITED;
            ///Note that the overlay can have pending buffers when we disable the display
            ///This is normal and the expectation is that they may not be displayed.
            ///This is to ensure that the user experience is not impacted
            ret = false;
            break;

        default:

            CAMHAL_LOGEB("Invalid Display Thread Command 0x%x.", msg.command);
            invalidCommand = true;

            break;
   }

    ///Signal the semaphore if it is sent as part of the message
    if ( ( msg.arg1 ) && ( !invalidCommand ) )
        {
        CAMHAL_LOGDA("+Signalling display semaphore");
        Semaphore &sem = *((Semaphore*)msg.arg1);
        sem.Signal();
        CAMHAL_LOGDA("-Signalled display semaphore");
        }

    LOG_FUNCTION_NAME_EXIT
    return ret;
}


status_t OverlayDisplayAdapter::PostFrame(OverlayDisplayAdapter::DisplayFrame &dispFrame)
{
    status_t ret = NO_ERROR;

    ///@todo Do cropping based on the stabilized frame coordinates
    ///@todo Insert logic to drop frames here based on refresh rate of
    ///display or rendering rate whichever is lower
    ///Queue the buffer to overlay
    overlay_buffer_t buf = (overlay_buffer_t) mPreviewBufferMap.valueFor((int) dispFrame.mBuffer);
    CAMHAL_LOGDB("buf = 0x%x", buf);
    if ( mDisplayState == OverlayDisplayAdapter::DISPLAY_STARTED )
        {
        //Post it to display via Overlay
        ret = mOverlay->queueBuffer(buf);
        if ( ret != NO_ERROR )
            {
            CAMHAL_LOGEB("Posting error 0x%x", ret);

            }
        else
            { // scope for the lock
            Mutex::Autolock lock(mLock);
            mFramesWithDisplay++;
            }

        }
    else
        {
        ///Drop the frame, return it back to the provider (Camera Adapter)
        mFrameProvider->returnFrame(dispFrame.mBuffer);
        }

    return ret;
}


bool OverlayDisplayAdapter::handleFrameReturn()
{
    overlay_buffer_t buf = NULL;

        {
        Mutex::Autolock lock(mLock);

        if ( 0 == mFramesWithDisplay )
            {
            return true;
            }
        }

    if ( mOverlay->dequeueBuffer(&buf) < 0 )
        {
        return true;
        }

    ///Return the frame back to the provider (Camera Adapter)
    mFrameProvider->returnFrame( (void *) mPreviewBufferMap.keyAt((int) buf));

    //Decrement the frames with display count
        {
        Mutex::Autolock lock(mLock);

        mFramesWithDisplay--;
        if ( 0 == mFramesWithDisplay )
            {
            CAMHAL_LOGDA("Received all frames back from Display");
            return true;
            }
        }

    return false;
}

void OverlayDisplayAdapter::frameCallbackRelay(CameraFrame* caFrame)
{

    if ( NULL != caFrame )
        {
        if ( NULL != caFrame->mCookie )
            {
            OverlayDisplayAdapter *da = (OverlayDisplayAdapter*) caFrame->mCookie;
            da->frameCallback(caFrame);
            }
        else
            {
            CAMHAL_LOGEB("Invalid Cookie in Camera Frame = %p, Cookie = %p", caFrame, caFrame->mCookie);
            }
        }
    else
        {
        CAMHAL_LOGEB("Invalid Camera Frame = %p", caFrame);
        }

}

void OverlayDisplayAdapter::frameCallback(CameraFrame* caFrame)
{
    ///Call queueBuffer of overlay in the context of the callback thread
    DisplayFrame df;
    df.mBuffer = caFrame->mBuffer;
    PostFrame(df);
}


/*--------------------OverlayDisplayAdapter Class ENDS here-----------------------------*/

};


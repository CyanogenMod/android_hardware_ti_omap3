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

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    mShotToShot = false;

#endif

    mPaused = false;

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

    ///The overlay object will get destroyed here
    destroy();

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

    ///Set the optimal buffer count to 0 since we have a display thread which monitors the
    ///fd of overlay
    mOverlay->setParameter(OPTIMAL_QBUF_CNT, 0x0);

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

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

status_t OverlayDisplayAdapter::setSnapshotTimeRef(struct timeval *refTime)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NULL != refTime )
        {
        Mutex::Autolock lock(mLock);
        memcpy(&mStartCapture, refTime, sizeof( struct timeval ));
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

#endif


int OverlayDisplayAdapter::enableDisplay(struct timeval *refTime)
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

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    if ( NULL != refTime )
        {
        Mutex::Autolock lock(mLock);
        memcpy(&mStandbyToShot, refTime, sizeof( struct timeval ));
        mMeasureStandby = true;
        }

#endif

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

    if ( NULL != mDisplayThread.get() )
        {
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
        }

    ///Reset the display enabled flag
    mDisplayEnabled = false;

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

status_t OverlayDisplayAdapter::pauseDisplay(bool pause)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    {
        Mutex::Autolock lock(mLock);
        mPaused = pause;
    }

    LOG_FUNCTION_NAME_EXIT

    return ret;
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
void* OverlayDisplayAdapter::allocateBuffer(int width, int height, const char* format, int &bytes, int numBufs)
{
    LOG_FUNCTION_NAME

    if(!numBufs)
        {
        ///if numBufs is zero, we take the default number of buffers allocated by overlay
        numBufs = (int)mOverlay->getBufferCount();
        }
    else
        {
        ///Set the number of buffers to be created by overlay
        mOverlay->setParameter(OVERLAY_NUM_BUFFERS, numBufs);
        mOverlay->resizeInput(width, height);
        }

    CAMHAL_LOGDB("Configuring %d buffers for overlay", numBufs);

    ///We just return the buffers from Overlay, if the width and height are same, else (vstab, vnf case)
    ///re-allocate buffers using overlay and then get them
    ///@todo - Re-allocate buffers for vnf and vstab using the width, height, format, numBufs etc
    const int lnumBufs = numBufs;
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
            bytes =  data->length;
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

uint32_t * OverlayDisplayAdapter::getOffsets()
{
    uint32_t *offsets;
    size_t bufferCount;
    mapping_data_t * data;
    LOG_FUNCTION_NAME

    offsets = NULL;
    if ( NULL == mOverlay.get() )
        {
        CAMHAL_LOGEA("Overlay reference is missing");
        goto fail;
        }

    bufferCount = ( size_t ) mOverlay->getBufferCount();

    offsets = new uint32_t[bufferCount];
    if ( NULL == offsets )
        {
        CAMHAL_LOGEA("No resources to allocate offsets array");
        goto fail;
        }

    for ( unsigned int i = 0 ; i < bufferCount ; i ++ )
        {
        data =  ( mapping_data_t * ) mOverlay->getBufferAddress( ( void * ) i);
        if ( NULL == data )
            {
            CAMHAL_LOGEA("Invalid Overlay mapping data");
            goto fail;
            }

        offsets[i] = ( uint32_t ) data->offset;
        }

    LOG_FUNCTION_NAME_EXIT

    return offsets;

fail:

    if ( NULL != offsets )
        {
        delete [] offsets;
        }

    LOG_FUNCTION_NAME_EXIT

    return NULL;
}

int OverlayDisplayAdapter::getFd()
{
    int fd;
    mapping_data_t * data;

    LOG_FUNCTION_NAME

    if ( NULL == mOverlay.get() )
        {
        CAMHAL_LOGEA("Overlay reference is missing");
        goto fail;
        }

    data =  ( mapping_data_t * ) mOverlay->getBufferAddress( ( void * ) 0);
    if ( NULL == data )
    {
        CAMHAL_LOGEA("Invalid Overlay mapping data");
        goto fail;
    }

    fd = ( uint32_t ) data->fd;

    LOG_FUNCTION_NAME_EXIT

    return fd;

fail:

    LOG_FUNCTION_NAME_EXIT

    return -1;

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
        /// @bug With mFramesWithDisplay>2, we will have always 2 buffers with overlay.
        ///          Ideally, we should remove this check and dequeue immediately when mDisplayQ is not empty
        ///          But there is some bug in OMX which is causing a crash when performing OMX_FillThisBuffer call
        else  if( !mDisplayQ.isEmpty() & (mFramesWithDisplay>4))
            {
            if ( mDisplayState== OverlayDisplayAdapter::DISPLAY_INIT )
                {

                ///If display adapter is not started, continue
                continue;

                }
            else
                {
                ///There is a frame from overlay for us to dequeue
                ///We dequeue and return the frame back to Camera adapter
                ///Below method returns true of there are no frames left with display after the dequeue
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
    int actualFramesWithDisplay = 0;

    ///@todo Do cropping based on the stabilized frame coordinates
    ///@todo Insert logic to drop frames here based on refresh rate of
    ///display or rendering rate whichever is lower
    ///Queue the buffer to overlay
    overlay_buffer_t buf = (overlay_buffer_t) mPreviewBufferMap.valueFor((int) dispFrame.mBuffer);

    //If paused state is set, and we have a preview frame, then return buffer
    {
        Mutex::Autolock lock(mLock);
        if ( ( mPaused ) && ( CameraFrame::CameraFrame::PREVIEW_FRAME_SYNC == dispFrame.mType ) )
            {
            mFrameProvider->returnFrame(dispFrame.mBuffer, CameraFrame::PREVIEW_FRAME_SYNC);

            return NO_ERROR;
            }

    }

    if ( mDisplayState == OverlayDisplayAdapter::DISPLAY_STARTED )
        {
        //Post it to display via Overlay
        actualFramesWithDisplay = ret = mOverlay->queueBuffer(buf);
        if ( ret < NO_ERROR )
            {
            CAMHAL_LOGEB("Posting error 0x%x", ret);

            }
        else
            { // scope for the lock
            Mutex::Autolock lock(mLock);
            mFramesWithDisplay++;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            if ( mMeasureStandby )
                {
                CameraHal::PPM("Standby to first shot: ", &mStandbyToShot);
                mMeasureStandby = false;
                }
            else if (CameraFrame::CameraFrame::SNAPSHOT_FRAME == dispFrame.mType)
                {
                CameraHal::PPM("Shot to snapshot: ", &mStartCapture);
                mShotToShot = true;
                }
            else if ( mShotToShot )
                {
                CameraHal::PPM("Shot to shot: ", &mStartCapture);
                mShotToShot = false;
                }

#endif
            if(actualFramesWithDisplay!=mFramesWithDisplay)
                {
                ///Skew detected. Overlay has gone through a stream off sequence due to trigger from Surface flinger
                /// Reclaim all the buffers back except the one we posted
                for(int i=0;i<mFramesWithDisplayMap.size();i++)
                    {
                    ///Return the reclaimed frames back to the provider (Camera Adapter)
                    mFrameProvider->returnFrame( (void *) mFramesWithDisplayMap.keyAt(i), CameraFrame::PREVIEW_FRAME_SYNC);
                    }
                ///Update the mFramesWithDisplay with the updated count.
                ///Note that actualFramesWithDisplay will always be 1 with the first queueBuffer after stream off
                mFramesWithDisplay = actualFramesWithDisplay;
                }

            mFramesWithDisplayMap.add((int) dispFrame.mBuffer, mFramesWithDisplay);
            ret = NO_ERROR;

            }

        }
    else
        {
        ///Drop the frame, return it back to the provider (Camera Adapter)
        mFrameProvider->returnFrame(dispFrame.mBuffer, CameraFrame::PREVIEW_FRAME_SYNC);
        }

    return ret;
}


bool OverlayDisplayAdapter::handleFrameReturn()
{
    overlay_buffer_t buf = NULL;
    status_t ret = NO_ERROR;

    Mutex::Autolock lock(mLock);

    ///If there are no frames with the display we return true so that dipslay thread can exit
    ///after receiving the exit command from camera hal.
    if ( 0 == mFramesWithDisplay )
        {
        return true;
        }


    ///This case implies that a stream off happened. In this case, there are no buffers to dequeue
    ///Buffers already queued will  be dequeued by PostFrame when it detects skew in the count
    ///of frames with overlay and internal count value
    if ( mOverlay->dequeueBuffer(&buf) == -2 )
        {
        CAMHAL_LOGEA("Looks like STREAM OFF happened inside overlay");
        return true;
        }

    ///Return the frame back to the provider (Camera Adapter)
    mFrameProvider->returnFrame( (void *) mPreviewBufferMap.keyAt((int) buf), CameraFrame::PREVIEW_FRAME_SYNC);

    ///Remove the frame from the display frame list
    mFramesWithDisplayMap.removeItem(mPreviewBufferMap.keyAt((int) buf));

    mFramesWithDisplay--;
    if ( 0 == mFramesWithDisplay )
        {
        CAMHAL_LOGDA("Received all frames back from Display");
        return true;
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
    df.mType = ( CameraFrame::FrameType ) caFrame->mFrameType;
    PostFrame(df);
}


/*--------------------OverlayDisplayAdapter Class ENDS here-----------------------------*/

};


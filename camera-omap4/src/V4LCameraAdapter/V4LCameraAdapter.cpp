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
/**
* @file V4LCameraAdapter.cpp
*
* This file maps the Camera Hardware Interface to V4L2.
*
*/


#include "V4LCameraAdapter.h"
#include "CameraHal.h"
#include "TICameraParameters.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev.h>


#include <cutils/properties.h>
#define UNLIKELY( exp ) (__builtin_expect( (exp) != 0, false ))
static int mDebugFps = 0;

#define Q16_OFFSET 16

#define HERE(Msg) {CAMHAL_LOGEB("--===line %d, %s===--\n", __LINE__, Msg);}

namespace android {

#undef LOG_TAG
///Maintain a separate tag for V4LCameraAdapter logs to isolate issues OMX specific
#define LOG_TAG "V4LCameraAdapter"

//frames skipped before recalculating the framerate
#define FPS_PERIOD 30

static V4LCameraAdapter *gCameraAdapter = NULL;
Mutex gAdapterLock;
const char *device = DEVICE;


/*--------------------Camera Adapter Class STARTS here-----------------------------*/

status_t V4LCameraAdapter::initialize(int sensor_index)
{
    LOG_FUNCTION_NAME

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.camera.showfps", value, "0");
    mDebugFps = atoi(value);

    int ret = NO_ERROR;

    // Allocate memory for video info structure
    mVideoInfo = (struct VideoInfo *) calloc (1, sizeof (struct VideoInfo));
    if(!mVideoInfo)
        {
        return NO_MEMORY;
        }

    if ((mCameraHandle = open(device, O_RDWR)) == -1)
        {
        CAMHAL_LOGEB("Error while opening handle to V4L2 Camera: %s", strerror(errno));
        return -EINVAL;
        }

    ret = ioctl (mCameraHandle, VIDIOC_QUERYCAP, &mVideoInfo->cap);
    if (ret < 0)
        {
        CAMHAL_LOGEA("Error when querying the capabilities of the V4L Camera");
        return -EINVAL;
        }

    if ((mVideoInfo->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0)
        {
        CAMHAL_LOGEA("Error while adapter initialization: video capture not supported.");
        return -EINVAL;
        }

    if (!(mVideoInfo->cap.capabilities & V4L2_CAP_STREAMING))
        {
        CAMHAL_LOGEA("Error while adapter initialization: Capture device does not support streaming i/o");
        return -EINVAL;
        }

    // Initialize flags
    mPreviewing = false;
    mVideoInfo->isStreaming = false;
    mRecording = false;

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

int V4LCameraAdapter::getFrameRefCount(void* frameBuf, CameraFrame::FrameType frameType)
{
    int res = -1;

    switch ( frameType )
        {
        case CameraFrame::PREVIEW_FRAME_SYNC:
                {
                Mutex::Autolock lock(mPreviewBufferLock);
                res = mPreviewBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
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

    return res;
}

void V4LCameraAdapter::setFrameRefCount(void* frameBuf, CameraFrame::FrameType frameType, int refCount)
{

    switch ( frameType )
        {
        case CameraFrame::PREVIEW_FRAME_SYNC:
                {
                Mutex::Autolock lock(mPreviewBufferLock);
                mPreviewBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);
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

}

size_t V4LCameraAdapter::getSubscriberCount(CameraFrame::FrameType frameType)
{
    size_t ret = 0;

    switch ( frameType )
        {
        case CameraFrame::PREVIEW_FRAME_SYNC:
                {
                Mutex::Autolock lock(mSubscriberLock);
                ret = mFrameSubscribers.size();
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

    return ret;
}

void V4LCameraAdapter::returnFrame(void* frameBuf, CameraFrame::FrameType frameType)
{
    status_t res = NO_ERROR;
    size_t subscriberCount = 0;
    int refCount = -1;

    if ( !mVideoInfo->isStreaming )
        {
        return;
        }

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
        else if ( 0 < subscriberCount )
            {
            CAMHAL_LOGEB("Error trying to decrement refCount %d for buffer 0x%x", ( uint32_t ) refCount, ( uint32_t ) frameBuf);
            return;
            }
        }


    if ( NO_ERROR == res )
        {
        //check if someone is holding this buffer
        if ( 0 == refCount )
            {
            //This is not working somehow currently, the camera preview hangs after displaying frames for 10 secs or so
            //For now releasing the buffers in the callback itself, after copying the data
            //res = FillThisBuffer(frameBuf);
            }
        }
    return;

}


status_t V4LCameraAdapter::FillThisBuffer(void* frameBuf)
{

    status_t ret = NO_ERROR;

    int i = mPreviewBufs.valueFor(( unsigned int )frameBuf);
    if(i<0)
        {
        return BAD_VALUE;
        }

    mVideoInfo->buf.index = i;
    mVideoInfo->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mVideoInfo->buf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(mCameraHandle, VIDIOC_QBUF, &mVideoInfo->buf);
    if (ret < 0) {
       CAMHAL_LOGEA("Init: VIDIOC_QBUF Failed");
       return -1;
    }

     nQueued++;

    return ret;

}


status_t V4LCameraAdapter::getCaps(CameraParameters &params)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;
    // For now do not populate anything
    //@todo Enhance this method to populate the capabilities queried from the V4L device.
    LOG_FUNCTION_NAME_EXIT
    return ret;
}

int V4LCameraAdapter::getRevision()
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    // For now return version 0
    //@todo Need to query and report the version of the V4L driver
    return 0;
}


status_t V4LCameraAdapter::setParameters(const CameraParameters &params)
{
    LOG_FUNCTION_NAME

    status_t ret = NO_ERROR;

    int width, height;

    params.getPreviewSize(&width, &height);

    CAMHAL_LOGDB("Width * Height %d x %d format 0x%x", width, height, DEFAULT_PIXEL_FORMAT);

    mVideoInfo->width = width;
    mVideoInfo->height = height;
    mVideoInfo->framesizeIn = (width * height << 1);
    mVideoInfo->formatIn = DEFAULT_PIXEL_FORMAT;

    mVideoInfo->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mVideoInfo->format.fmt.pix.width = width;
    mVideoInfo->format.fmt.pix.height = height;
    mVideoInfo->format.fmt.pix.pixelformat = DEFAULT_PIXEL_FORMAT;

    ret = ioctl(mCameraHandle, VIDIOC_S_FMT, &mVideoInfo->format);
    if (ret < 0) {
        CAMHAL_LOGEB("Open: VIDIOC_S_FMT Failed: %s", strerror(errno));
        return ret;
    }

    // Udpate the current parameter set
    mParams = params;

    LOG_FUNCTION_NAME_EXIT
    return ret;
}


void V4LCameraAdapter::getParameters(CameraParameters& params)
{
    LOG_FUNCTION_NAME

    // Return the current parameter set
    params = mParams;

    LOG_FUNCTION_NAME_EXIT
}


///API to give the buffers to Adapter
status_t V4LCameraAdapter::useBuffers(CameraMode mode, void* bufArr, int num)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    switch(mode)
        {
        case CAMERA_PREVIEW:
            ret = UseBuffersPreview(bufArr, num);
            break;

        //@todo Insert Image capture case here

        case CAMERA_VIDEO:
            //@warn Video capture is not fully supported yet
            ret = UseBuffersPreview(bufArr, num);
            break;

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t V4LCameraAdapter::UseBuffersPreview(void* bufArr, int num)
{
    int ret = NO_ERROR;

    if(NULL == bufArr)
        {
        return BAD_VALUE;
        }

    //First allocate adapter internal buffers at V4L level for USB Cam
    //These are the buffers from which we will copy the data into overlay buffers
    /* Check if camera can handle NB_BUFFER buffers */
    mVideoInfo->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mVideoInfo->rb.memory = V4L2_MEMORY_MMAP;
    mVideoInfo->rb.count = num;

    ret = ioctl(mCameraHandle, VIDIOC_REQBUFS, &mVideoInfo->rb);
    if (ret < 0) {
        CAMHAL_LOGEB("VIDIOC_REQBUFS failed: %s", strerror(errno));
        return ret;
    }

    for (int i = 0; i < num; i++) {

        memset (&mVideoInfo->buf, 0, sizeof (struct v4l2_buffer));

        mVideoInfo->buf.index = i;
        mVideoInfo->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        mVideoInfo->buf.memory = V4L2_MEMORY_MMAP;

        ret = ioctl (mCameraHandle, VIDIOC_QUERYBUF, &mVideoInfo->buf);
        if (ret < 0) {
            CAMHAL_LOGEB("Unable to query buffer (%s)", strerror(errno));
            return ret;
        }

        mVideoInfo->mem[i] = mmap (0,
               mVideoInfo->buf.length,
               PROT_READ | PROT_WRITE,
               MAP_SHARED,
               mCameraHandle,
               mVideoInfo->buf.m.offset);

        if (mVideoInfo->mem[i] == MAP_FAILED) {
            CAMHAL_LOGEB("Unable to map buffer (%s)", strerror(errno));
            return -1;
        }

        uint32_t *ptr = (uint32_t*) bufArr;

        //Associate each Camera internal buffer with the one from Overlay
        mPreviewBufs.add((int)ptr[i], i);

    }

    // Update the preview buffer count
    mPreviewBufferCount = num;

    return ret;
}

//API to send a command to the camera
status_t V4LCameraAdapter::sendCommand(int operation, int value1, int value2, int value3)
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

                        mPreviewBuffers = (int *) desc->mBuffers;

                        mPreviewBuffersAvailable.clear();
                        for ( uint32_t i = 0 ; i < desc->mCount ; i++ )
                            {
                            mPreviewBuffersAvailable.add(mPreviewBuffers[i], 0);
                            }
                        }
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

        //@todo Image capture is not supported yet.

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
            break;
            }

        default:
            CAMHAL_LOGEB("Command 0x%x unsupported!", operation);
            break;
    };

    LOG_FUNCTION_NAME_EXIT
    return ret;

}

status_t V4LCameraAdapter::startPreview()
{
    status_t ret = NO_ERROR;

  Mutex::Autolock lock(mPreviewBufferLock);

  if(mPreviewing)
    {
    return BAD_VALUE;
    }

   for (int i = 0; i < mPreviewBufferCount; i++) {

       mVideoInfo->buf.index = i;
       mVideoInfo->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       mVideoInfo->buf.memory = V4L2_MEMORY_MMAP;

       ret = ioctl(mCameraHandle, VIDIOC_QBUF, &mVideoInfo->buf);
       if (ret < 0) {
           CAMHAL_LOGEA("VIDIOC_QBUF Failed");
           return -EINVAL;
       }

       nQueued++;
   }

    enum v4l2_buf_type bufType;
   if (!mVideoInfo->isStreaming) {
       bufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;

       ret = ioctl (mCameraHandle, VIDIOC_STREAMON, &bufType);
       if (ret < 0) {
           CAMHAL_LOGEB("StartStreaming: Unable to start capture: %s", strerror(errno));
           return ret;
       }

       mVideoInfo->isStreaming = true;
   }

   // Create and start preview thread for receiving buffers from V4L Camera
   mPreviewThread = new PreviewThread(this);

   CAMHAL_LOGDA("Created preview thread");


   //Update the flag to indicate we are previewing
   mPreviewing = true;

   return ret;

}

status_t V4LCameraAdapter::stopPreview()
{
    enum v4l2_buf_type bufType;
    int ret = NO_ERROR;

    Mutex::Autolock lock(mPreviewBufferLock);

    if(!mPreviewing)
        {
        return NO_INIT;
        }

    if (mVideoInfo->isStreaming) {
        bufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        ret = ioctl (mCameraHandle, VIDIOC_STREAMOFF, &bufType);
        if (ret < 0) {
            CAMHAL_LOGEB("StopStreaming: Unable to stop capture: %s", strerror(errno));
            return ret;
        }

        mVideoInfo->isStreaming = false;
    }

    mVideoInfo->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mVideoInfo->buf.memory = V4L2_MEMORY_MMAP;

    /* Dequeue everything */
    int DQcount = nQueued - nDequeued;

    for (int i = 0; i < DQcount-1; i++) {
        ret = ioctl(mCameraHandle, VIDIOC_DQBUF, &mVideoInfo->buf);
        if (ret < 0)
            CAMHAL_LOGEA("IDIOC_DQBUF Failed");
    }
    nQueued = 0;
    nDequeued = 0;

    /* Unmap buffers */
    for (int i = 0; i < mPreviewBufferCount; i++)
        if (munmap(mVideoInfo->mem[i], mVideoInfo->buf.length) < 0)
            CAMHAL_LOGEA("Unmap failed");

    {
    ///Clear all the available preview buffers
    mPreviewBuffersAvailable.clear();

    mPreviewBufs.clear();
    }

    mPreviewThread->requestExitAndWait();
    mPreviewThread.clear();

    return ret;

}

char * V4LCameraAdapter::GetFrame(int &index)
{
    int ret;

    mVideoInfo->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mVideoInfo->buf.memory = V4L2_MEMORY_MMAP;

    /* DQ */
    ret = ioctl(mCameraHandle, VIDIOC_DQBUF, &mVideoInfo->buf);
    if (ret < 0) {
        CAMHAL_LOGEA("GetFrame: VIDIOC_DQBUF Failed");
        return NULL;
    }
    nDequeued++;

    index = mVideoInfo->buf.index;

    return (char *)mVideoInfo->mem[mVideoInfo->buf.index];
}

status_t V4LCameraAdapter::setTimeOut(unsigned int sec)
{
    status_t ret = NO_ERROR;

    gCameraAdapter = NULL;
    delete this;

    return ret;
}

status_t V4LCameraAdapter::cancelTimeOut()
{
    status_t ret = NO_ERROR;

    //Do nothing as this adapter doesn't support the timeout functionality yet

    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t V4LCameraAdapter::startVideoCapture()
{
    //@todo Video capture to be supported
    return NO_ERROR;
}

status_t V4LCameraAdapter::stopVideoCapture()
{
    //@todo Video capture to be supported
    return NO_ERROR;
}


//API to cancel a currently executing command
status_t V4LCameraAdapter::cancelCommand(int operation)
{
    LOG_FUNCTION_NAME

    //@todo We dont support this method, there is no need for it now.

    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR;
}

//API to get the frame size required to be allocated. This size is used to override the size passed
//by camera service when VSTAB/VNF is turned ON for example
void V4LCameraAdapter::getFrameSize(int &width, int &height)
{
    // Just return the current preview size, nothing more to do here.
    mParams.getPreviewSize(&width, &height);

    LOG_FUNCTION_NAME_EXIT
}

status_t V4LCameraAdapter::getFrameDataSize(size_t &dataFrameSize, size_t bufferCount)
{
    // We don't support meta data, so simply return
    return NO_ERROR;
}

status_t V4LCameraAdapter::getPictureBufferSize(size_t &length, size_t bufferCount)
{
    // We don't support image capture yet, safely return from here without messing up
    return NO_ERROR;
}

static void debugShowFPS()
{
    static int mFrameCount = 0;
    static int mLastFrameCount = 0;
    static nsecs_t mLastFpsTime = 0;
    static float mFps = 0;
    mFrameCount++;
    if (!(mFrameCount & 0x1F)) {
        nsecs_t now = systemTime();
        nsecs_t diff = now - mLastFpsTime;
        mFps = ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
        LOGD("Camera %d Frames, %f FPS", mFrameCount, mFps);
    }
    // XXX: mFPS has the value we want
}

status_t V4LCameraAdapter::recalculateFPS()
{
    float currentFPS;

    mFrameCount++;

    if ( ( mFrameCount % FPS_PERIOD ) == 0 )
        {
        nsecs_t now = systemTime();
        nsecs_t diff = now - mLastFPSTime;
        currentFPS =  ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFPSTime = now;
        mLastFrameCount = mFrameCount;

        if ( 1 == mIter )
            {
            mFPS = currentFPS;
            }
        else
            {
            //cumulative moving average
            mFPS = mLastFPS + (currentFPS - mLastFPS)/mIter;
            }

        mLastFPS = mFPS;
        mIter++;
        }

    return NO_ERROR;
}


V4LCameraAdapter::V4LCameraAdapter()
{
    LOG_FUNCTION_NAME

    // Nothing useful to do in the constructor

    LOG_FUNCTION_NAME_EXIT
}

V4LCameraAdapter::~V4LCameraAdapter()
{
    LOG_FUNCTION_NAME

    // Close the camera handle and free the video info structure
    close(mCameraHandle);
    free(mVideoInfo);

    LOG_FUNCTION_NAME_EXIT
}

/* Preview Thread */
// ---------------------------------------------------------------------------

int V4LCameraAdapter::previewThread()
{
    status_t ret = NO_ERROR;

     CameraFrame::FrameType typeOfFrame = CameraFrame::ALL_FRAMES;
     unsigned int refCount = 0;

    if (mPreviewing)
        {
        int index = 0;
        char *fp = this->GetFrame(index);

        uint8_t* ptr = (uint8_t*) mPreviewBufs.keyAt(index);

        int width, height;
        uint16_t* dest = (uint16_t*)ptr;
        uint16_t* src = (uint16_t*) fp;
        mParams.getPreviewSize(&width, &height);
        for(int i=0;i<height;i++)
            {
            for(int j=0;j<width;j++)
                {
                //*dest = *src;
                //convert from YUYV to UYVY supported in Camera service
                *dest = (((*src & 0xFF000000)>>24)<<16)|(((*src & 0x00FF0000)>>16)<<24) |
                        (((*src & 0xFF00)>>8)<<0)|(((*src & 0x00FF)>>0)<<8);
                src++;
                dest++;
                }
                dest += 4096/2-width;
            }

        // Call FillThisBuffer here itself as calling it inside returnFrame doesnt work
        //@todo Need to look into this issue
        FillThisBuffer(ptr);

        typeOfFrame = CameraFrame::PREVIEW_FRAME_SYNC;
        refCount = getSubscriberCount(typeOfFrame);
        setFrameRefCount((void*) ptr, typeOfFrame, refCount);
        CAMHAL_LOGDB("Preview Frame 0x%x refCount start %d", ( uint32_t ) ptr, refCount);

        ret = sendFrameToSubscribers(ptr, typeOfFrame);

        }

    return ret;
}

status_t V4LCameraAdapter::sendFrameToSubscribers(void* frame, int typeOfFrame)
{
    status_t ret = NO_ERROR;
    int refCount;

    frame_callback callback;
    CameraFrame cFrame;
    uint32_t i = 0;

    if ( NO_ERROR == ret )
        {
        int width, height;
        mParams.getPreviewSize(&width, &height);
        cFrame.mFrameType = typeOfFrame;
        cFrame.mBuffer = frame;
        cFrame.mLength = width*height*2;
        cFrame.mAlignment = width*2;
        cFrame.mOffset = 0;


        cFrame.mTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);;

        if ( ( CameraFrame::PREVIEW_FRAME_SYNC == typeOfFrame ) || ( CameraFrame::SNAPSHOT_FRAME == typeOfFrame ) )
            {


            cFrame.mWidth = width;
            cFrame.mHeight = height;
            for( i = 0 ; i < mFrameSubscribers.size(); i++ )
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

    if ( 0 == i )
        {
        //No subscribers for this frame
        ret = -1;
        }

    return ret;
}


extern "C" CameraAdapter* CameraAdapter_Factory()
{
    Mutex::Autolock lock(gAdapterLock);

    LOG_FUNCTION_NAME

    if ( NULL == gCameraAdapter )
        {
        CAMHAL_LOGDA("Creating new Camera adapter instance");
        gCameraAdapter= new V4LCameraAdapter();
        }
    else
        {
        CAMHAL_LOGDA("Reusing existing Camera adapter instance");
        }


    LOG_FUNCTION_NAME_EXIT

    return gCameraAdapter;
}

};


/*--------------------Camera Adapter Class ENDS here-----------------------------*/


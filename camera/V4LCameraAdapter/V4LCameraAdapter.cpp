/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

//added by sasken - start
#define ENABLE_GRALLOC_BUFFERS
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <ion.h>

#include "../../omap3/hwc/hal_public.h"
//added by sasken - end

#define HERE(Msg) {CAMHAL_LOGEB("--===line %d, %s===--\n", __LINE__, Msg);}

namespace android {

//Added by sasken start

#define COMPENSATION_MIN        -20
#define COMPENSATION_MAX        20
#define COMPENSATION_STEP       "0.1"
#define PARM_ZOOM_SCALE         100

#define ZOOM_STAGES 41
#define IMX046_FOCALLENGTH 4.68
#define IMX046_HORZANGLE 62.9
#define IMX046_VERTANGLE 24.8
#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)

static const char supportedPictureSizes [] = "3264x2448,2560x2048,2048x1536,1600x1200,1280x1024,1152x968,1280x960,800x600,640x480,320x240";
const char supportedPreviewSizes [] = "1280x720,992x560,864x480,800x480,720x576,720x480,768x576,640x480,320x240,352x288,240x160,176x144,128x96";
const char supportedFPS [] = "33,30,25,24,20,15,10";
const char supportedThumbnailSizes []= "320x240,80x60,0x0";
const char supportedFpsRanges [] = "(8000,8000),(8000,10000),(10000,10000),(8000,15000),(15000,15000),(8000,20000),(20000,20000),(24000,24000),(25000,25000),(8000,30000),(30000,30000)";
const char PARAMS_DELIMITER []= ",";

const supported_resolution supportedPictureRes[] = { {3264, 2448} , {2560, 2048} ,
                                                     {2048, 1536} , {1600, 1200} ,
                                                     {1280, 1024} , {1152, 968} ,
                                                     {1280, 960} , {800, 600},
                                                     {640, 480}   , {320, 240} };

const supported_resolution supportedPreviewRes[] = { {1280, 720}, {800, 480},
                                                     {720, 576}, {720, 480},
                                                     {992, 560}, {864, 480}, {848, 480},
                                                     {768, 576}, {640, 480},
                                                     {320, 240}, {352, 288}, {240, 160},
                                                     {176, 144}, {128, 96}};

static float zoom_step [ZOOM_STAGES] = {  1.0000, 1.0353, 1.0718, 1.1096, 1.1487, 1.1892, 1.2311, 1.2746,
                                          1.3195, 1.3660, 1.4142, 1.4641, 1.5157, 1.5692, 1.6245, 1.6818,
                                          1.7411, 1.8025, 1.8661, 1.9319, 2.0000, 2.0705, 2.1435, 2.2191,
                                          2.2974, 2.3784, 2.4623, 2.5491, 2.6390, 2.7321, 2.8284, 2.9282,
                                          3.0314, 3.1383, 3.2490, 3.3636, 3.4822, 3.6050, 3.7321, 3.8637,
                                          4.0000};
//added by sasken end

#undef LOG_TAG
///Maintain a separate tag for V4LCameraAdapter logs to isolate issues OMX specific
//#define LOG_TAG "CameraHAL" //Kirti Commented

#define LOG_TAG "V4LCameraAdapter"

//frames skipped before recalculating the framerate
#define FPS_PERIOD 30

Mutex gAdapterLock;
const char *device = DEVICE;


/*--------------------Camera Adapter Class STARTS here-----------------------------*/

status_t V4LCameraAdapter::initialize(int CameraHandle)
{
    LOG_FUNCTION_NAME;

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

    mCameraHandle = CameraHandle;

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

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

status_t V4LCameraAdapter::fillThisBuffer(void* frameBuf, CameraFrame::FrameType frameType)
{

    status_t ret = NO_ERROR;

    if ( !mVideoInfo->isStreaming )
    {
        return NO_ERROR;
    }

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

status_t V4LCameraAdapter::setParameters(const CameraParameters &params)
{
    LOG_FUNCTION_NAME;

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

    LOG_FUNCTION_NAME_EXIT;
    return ret;
}


void V4LCameraAdapter::getParameters(CameraParameters& params)
{
    LOG_FUNCTION_NAME;

    // Return the current parameter set
    params = mParams;

    LOG_FUNCTION_NAME_EXIT;
}


///API to give the buffers to Adapter
status_t V4LCameraAdapter::useBuffers(CameraMode mode, void* bufArr, int num, size_t length, unsigned int queueable)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME;

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

    LOG_FUNCTION_NAME_EXIT;

    return ret;
}

status_t V4LCameraAdapter::UseBuffersPreview(void* bufArr, int num)
{
    int ret = NO_ERROR;
    int width, height;

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

    /******* added by sasken - saving ion handles **********/

#ifdef ENABLE_GRALLOC_BUFFERS
    void* buff_t;
    int map_fd;
    struct ion_map_gralloc_to_ionhandle_data data;
#endif

    int fd = ion_open();
    LOGE(" Ion open success ");
    LOGE("ion_fd = %d", fd);
    /******* added by sasken - end **********/

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

        /******* added by sasken - start - saving ion handles **********/
        mParams.getPreviewSize(&width, &height);

	data.gralloc_handle = (int*)((IMG_native_handle_t*)ptr[i])->fd[0];  // your gralloc fd;
	LOGE("data.gralloc_handle = %d", data.gralloc_handle);

	if (ion_ioctl(fd, ION_IOC_MAP_GRALLOC, &data)) {
		LOGE("ion_ioctl fail");
	}

	LOGE("data.handleY = %x", data.handleY);

	if (ion_map(fd, data.handleY, (width*height*2), PROT_READ | PROT_WRITE,
				MAP_SHARED, 0, (unsigned char **)&buff_t, &map_fd) < 0) {
		LOGE("ION map failed");
	}
	mIonHandle.add((void*)buff_t,i);

        /******* added by sasken - end **********/
    }

    // Update the preview buffer count
    mPreviewBufferCount = num;

    return ret;
}

/*status_t V4LCameraAdapter::queueToCamera(int index)
{
   status_t ret = NO_ERROR;

   Mutex::Autolock lock(mPreviewBufsLock);

   mVideoInfo->buf.index = index;
   mVideoInfo->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   mVideoInfo->buf.memory = V4L2_MEMORY_MMAP;

   ret = ioctl(mCameraHandle, VIDIOC_QBUF, &mVideoInfo->buf);
   if (ret < 0) {
	   CAMHAL_LOGEA("VIDIOC_QBUF Failed");
	   return -EINVAL;
   }

   return ret;

}*/

status_t V4LCameraAdapter::startPreview()
{
	LOGE(" V4LCameraAdapter::startPreview");
    status_t ret = NO_ERROR;

  Mutex::Autolock lock(mPreviewBufsLock);

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

   //Update the flag to indicate we are previewing
   mPreviewing = true;

   return ret;

}

status_t V4LCameraAdapter::stopPreview()
{
    enum v4l2_buf_type bufType;
    int ret = NO_ERROR;

    Mutex::Autolock lock(mPreviewBufsLock);

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

    nQueued = 0;
    nDequeued = 0;

    /* Unmap buffers */
    for (int i = 0; i < mPreviewBufferCount; i++)
        if (munmap(mVideoInfo->mem[i], mVideoInfo->buf.length) < 0)
            CAMHAL_LOGEA("Unmap failed");

    mPreviewBufs.clear();

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

//API to get the frame size required to be allocated. This size is used to override the size passed
//by camera service when VSTAB/VNF is turned ON for example
status_t V4LCameraAdapter::getFrameSize(size_t &width, size_t &height)
{
    status_t ret = NO_ERROR;

    // Just return the current preview size, nothing more to do here.
    mParams.getPreviewSize(( int * ) &width,
                           ( int * ) &height);

    LOG_FUNCTION_NAME_EXIT;

    return ret;
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

void V4LCameraAdapter::onOrientationEvent(uint32_t orientation, uint32_t tilt)
{
    LOG_FUNCTION_NAME;

    LOG_FUNCTION_NAME_EXIT;
}


V4LCameraAdapter::V4LCameraAdapter(size_t sensor_index)
{
    LOG_FUNCTION_NAME;

    // Nothing useful to do in the constructor

    LOG_FUNCTION_NAME_EXIT;
}

V4LCameraAdapter::~V4LCameraAdapter()
{
    LOG_FUNCTION_NAME;

    // Close the camera handle and free the video info structure
    close(mCameraHandle);

    if (mVideoInfo)
      {
        free(mVideoInfo);
        mVideoInfo = NULL;
      }

    LOG_FUNCTION_NAME_EXIT;
}

/* Preview Thread */
// ---------------------------------------------------------------------------

int V4LCameraAdapter::queueToGralloc(int index, char* fp)
{
    status_t ret = NO_ERROR;
    int width, height;
    CameraFrame frame;
    int i;
    VideoInfo* buf;

    if(!fp)
    {
	return BAD_VALUE;
    }

    uint16_t* src = (uint16_t*) fp;
    mParams.getPreviewSize(&width, &height);
    LOGE(" Inside V4LCameraAdapter width = %d , height = %d ",width, height);

#if 0
    LOGE(" dumping source data at queuetogralloc ");
    FILE *fileptr = fopen("/data/camera_src_new.raw", "ab");
    fwrite((const void *)src,1, width*height*2,  fileptr);
    fclose(fileptr);
    LOGE(" dumping source data complete queuetogralloc ");
#endif

//added by sasken - start

    uint8_t* grallocPtr = (uint8_t*) mPreviewBufs.keyAt(index);
    uint16_t* dest = (uint16_t*)mIonHandle.keyAt(index);

    for(int i=0;i<height;i++)
    {
	for(int j=0;j<width;j++)
	{
	  //Renuka remove
		*dest = *src;
		//convert from YUYV to UYVY supported in Camera service
		/**dest = (((*src & 0xFF000000)>>24)<<16)|(((*src & 0x00FF0000)>>16)<<24) |
				(((*src & 0xFF00)>>8)<<0)|(((*src & 0x00FF)>>0)<<8);*/

		src++;
	    dest++;
	}
		//dest += 4096/2-width;
    }

    LOGE(" Inside V4LCameraAdapter completed frame conversion");

	recalculateFPS();

   // Mutex::Autolock lock(mSubscriberLock);

	mParams.getPreviewSize(&width, &height);
	frame.mFrameType = CameraFrame::PREVIEW_FRAME_SYNC;
	frame.mFrameMask = CameraFrame::PREVIEW_FRAME_SYNC;
	frame.mBuffer = grallocPtr;
	frame.mLength = width*height*2;
	frame.mAlignment = width*2;
	frame.mOffset = 0;
	frame.mWidth = width;
	frame.mHeight = height;
	frame.mYuv[0] = NULL;
	frame.mYuv[1] = NULL;
	frame.mTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);

	ret = setInitFrameRefCount(frame.mBuffer, frame.mFrameMask);

	LOGE(" setInitFrameRefCount is %d", ret);

	LOGE(" Before sendFrameToSubscribers");
	ret = sendFrameToSubscribers(&frame);
	LOGE(" After sendFrameToSubscribers");

    return ret;
}

extern "C" CameraAdapter* CameraAdapter_Factory(size_t sensor_index)
{
    CameraAdapter *adapter = NULL;
    Mutex::Autolock lock(gAdapterLock);

    LOG_FUNCTION_NAME;

    adapter = new V4LCameraAdapter(sensor_index);
    if ( adapter ) {
        CAMHAL_LOGDB("New OMX Camera adapter instance created for sensor %d",sensor_index);
    } else {
        CAMHAL_LOGEA("Camera adapter create failed!");
    }

    LOG_FUNCTION_NAME_EXIT;

    return adapter;
}

extern "C" int CameraAdapter_Capabilities(CameraProperties::Properties* properties_array,
                                          const unsigned int starting_camera,
                                          const unsigned int max_camera) {
    int num_cameras_supported = 0;
    CameraProperties::Properties* properties = NULL;

    LOG_FUNCTION_NAME;

    if(!properties_array)
    {
        return -EINVAL;
    }

    // TODO: Need to tell camera properties what other cameras we can support
    if (starting_camera + num_cameras_supported < max_camera) {
        num_cameras_supported++;
        properties = properties_array + starting_camera;
        properties->set(CameraProperties::CAMERA_NAME, "USBCamera");

        //added by sasken start - setting the default properties
        properties->set(CameraProperties::PREVIEW_FRAME_RATE, 30);
        properties->set(CameraProperties::PREVIEW_FORMAT,CameraParameters::PIXEL_FORMAT_YUV422I );
        properties->set(CameraProperties::PICTURE_FORMAT, CameraParameters::PIXEL_FORMAT_JPEG);
        properties->set(CameraProperties::JPEG_QUALITY, 100);
        properties->set(CameraProperties::EV_COMPENSATION, 0);
        properties->set(CameraProperties::ZOOM, 0);
        char fpsRange[32];
        sprintf(fpsRange, "%d,%d", 30000,30000);
        properties->set(CameraProperties::FOCAL_LENGTH, STRINGIZE(IMX046_FOCALLENGTH));
        properties->set(CameraProperties::HOR_ANGLE, STRINGIZE(IMX046_HORZANGLE) );
        properties->set(CameraProperties::VER_ANGLE, STRINGIZE(IMX046_VERTANGLE));
        properties->set(CameraProperties::FRAMERATE_RANGE, fpsRange);
        properties->set(CameraProperties::JPEG_THUMBNAIL_QUALITY, 100);
        char str[32];
        sprintf(str, "%dx%d", PICTURE_WIDTH, PICTURE_HEIGHT);
        properties->set(CameraProperties::PICTURE_SIZE , str);
        char strsize[32];
        sprintf(strsize, "%dx%d", MIN_WIDTH, MIN_HEIGHT);
        properties->set(CameraProperties::PREVIEW_SIZE, strsize);

        properties->set(CameraProperties::SUPPORTED_PICTURE_SIZES, supportedPictureSizes );
        properties->set(CameraProperties::SUPPORTED_PICTURE_FORMATS, CameraParameters::PIXEL_FORMAT_JPEG);
        properties->set(CameraProperties::SUPPORTED_PREVIEW_SIZES, supportedPreviewSizes);
        properties->set(CameraProperties::SUPPORTED_PREVIEW_FORMATS, CameraParameters::PIXEL_FORMAT_YUV422I);
        properties->set(CameraProperties::SUPPORTED_PREVIEW_FRAME_RATES, supportedFPS);
        properties->set(CameraProperties::SUPPORTED_THUMBNAIL_SIZES, supportedThumbnailSizes);

        properties->set(CameraProperties::SUPPORTED_EV_MAX, COMPENSATION_MAX);
        properties->set(CameraProperties::SUPPORTED_EV_MIN, COMPENSATION_MIN);
        properties->set(CameraProperties::SUPPORTED_EV_STEP, COMPENSATION_STEP);

        properties->set(CameraProperties::SUPPORTED_ZOOM_STAGES, ZOOM_STAGES-1);
        properties->set(CameraProperties::ZOOM_SUPPORTED, "true");
        properties->set(CameraProperties::SMOOTH_ZOOM_SUPPORTED, "true");
        properties->set(CameraProperties::FRAMERATE_RANGE_SUPPORTED, supportedFpsRanges);

        //added by sasken end
    }

    LOG_FUNCTION_NAME_EXIT;

    return num_cameras_supported;
}

};


/*--------------------Camera Adapter Class ENDS here-----------------------------*/


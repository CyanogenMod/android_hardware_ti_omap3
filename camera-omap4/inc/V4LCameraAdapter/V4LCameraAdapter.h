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


#ifndef V4L_CAMERA_ADAPTER_H
#define V4L_CAMERA_ADAPTER_H

#include "CameraHal.h"
#include "BaseCameraAdapter.h"
#include "DebugUtils.h"

namespace android {

#define DEFAULT_PIXEL_FORMAT V4L2_PIX_FMT_YUYV
#define NB_BUFFER 10
#define DEVICE "/dev/video4"


struct VideoInfo {
    struct v4l2_capability cap;
    struct v4l2_format format;
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers rb;
    void *mem[NB_BUFFER];
    bool isStreaming;
    int width;
    int height;
    int formatIn;
    int framesizeIn;
};


/**
  * Class which completely abstracts the camera hardware interaction from camera hal
  * TODO: Need to list down here, all the message types that will be supported by this class
                Need to implement BufferProvider interface to use AllocateBuffer of OMX if needed
  */
class V4LCameraAdapter : public BaseCameraAdapter
{
public:

    /*--------------------Constant declarations----------------------------------------*/
    static const int32_t MAX_NO_BUFFERS = 20;

    ///@remarks OMX Camera has six ports - buffer input, time input, preview, image, video, and meta data
    static const int MAX_NO_PORTS = 6;

    ///Five second timeout
    static const int CAMERA_ADAPTER_TIMEOUT = 5000*1000;

public:

    V4LCameraAdapter();
    ~V4LCameraAdapter();


    ///Initialzes the camera adapter creates any resources required
    virtual status_t initialize(int sensor_index=0);

    //APIs to configure Camera adapter and get the current parameter set
    virtual status_t setParameters(const CameraParameters& params);
    virtual void getParameters(CameraParameters& params);

    //API to get the caps
    virtual status_t getCaps(CameraParameters &params);

    //Used together with capabilities
    virtual int getRevision();

    // API
    virtual status_t UseBuffersPreview(void* bufArr, int num);

    //API to flush the buffers for preview
    status_t flushBuffers();

    //API to get the frame size required to be allocated. This size is used to override the size passed
    //by camera service when VSTAB/VNF is turned ON for example
    virtual void getFrameSize(int &width, int &height);

    virtual status_t getPictureBufferSize(size_t &length, size_t bufferCount);

    virtual status_t getFrameDataSize(size_t &dataFrameSize, size_t bufferCount);

protected:

//----------Parent class method implementation------------------------------------
    virtual status_t setTimeOut(unsigned int sec);
    virtual status_t startPreview();
    virtual status_t stopPreview();
    virtual status_t useBuffers(CameraMode mode, void* bufArr, int num, size_t length);
    virtual status_t fillThisBuffer(void* frameBuf, CameraFrame::FrameType frameType);
//-----------------------------------------------------------------------------


private:

    class PreviewThread : public Thread {
            V4LCameraAdapter* mAdapter;
        public:
            PreviewThread(V4LCameraAdapter* hw) :
#ifdef SINGLE_PROCESS
                // In single process mode this thread needs to be a java thread,
                // since we won't be calling through the binder.
                Thread(true),
#else
                Thread(false),
#endif
                  mAdapter(hw) { }
            virtual void onFirstRef() {
                run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);
            }
            virtual bool threadLoop() {
                mAdapter->previewThread();
                // loop until we need to quit
                return true;
            }
        };

    //Used for calculation of the average frame rate during preview
    status_t recalculateFPS();

    char * GetFrame(int &index);

    int previewThread();

public:

private:
    int mPreviewBufferCount;
    KeyedVector<int, int> mPreviewBufs;
    mutable Mutex mPreviewBufsLock;

    CameraParameters mParams;

    bool mPreviewing;
    bool mCapturing;
    Mutex mLock;

    int mFrameCount;
    int mLastFrameCount;
    unsigned int mIter;
    nsecs_t mLastFPSTime;

    //variables holding the estimated framerate
    float mFPS, mLastFPS;

    int mSensorIndex;

     // protected by mLock
     sp<PreviewThread>   mPreviewThread;

     struct VideoInfo *mVideoInfo;
     int mCameraHandle;


    int nQueued;
    int nDequeued;

};
}; //// namespace
#endif //V4L_CAMERA_ADAPTER_H


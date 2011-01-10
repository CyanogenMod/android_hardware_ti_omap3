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


#ifndef FAKE_CAMERA_ADAPTER_H
#define FAKE_CAMERA_ADAPTER_H

#include "BaseCameraAdapter.h"

namespace android {


class FakeCameraAdapter : public BaseCameraAdapter
{

    enum PreviewFrameType
        {
            SNAPSHOT_FRAME = 0,
            NORMAL_FRAME
        };

public:

    FakeCameraAdapter();
    ~FakeCameraAdapter();

    ///Initialzes the camera adapter creates any resources required
    virtual status_t initialize(int sensor_index=0);

    virtual void returnFrame(void* frameBuf, CameraFrame::FrameType frameType);

    virtual int setErrorHandler(ErrorNotifier *errorNotifier);

    //APIs to configure Camera adapter and get the current parameter set
    virtual status_t setParameters(const CameraParameters& params);
    virtual void getParameters(CameraParameters& params);

    //API to get the caps
    virtual status_t getCaps(CameraParameters &params);

    //Used together with capabilities
    virtual int getRevision();

    //API to send a command to the camera
    virtual status_t sendCommand(int operation, int value1=0, int value2=0, int value3=0);

    //API to cancel a currently executing command
    virtual status_t cancelCommand(int operation);

    //API to get the frame size required to be allocated. This size is used to override the size passed
    //by camera service when VSTAB/VNF is turned ON for example
    virtual void getFrameSize(int &width, int &height);

    virtual status_t getFrameDataSize(size_t &dataFrameSize, size_t bufferCount);

    virtual status_t getPictureBufferSize(size_t &length, size_t bufferCount);


protected:
    status_t takePicture();
    status_t doAutofocus();
    virtual void frameThread();
    virtual void frameCallbackThread();
    //Fills a given overlay buffer with a solid color depending on their index
    // and type
    void setBuffer(void *previewBuffer, int index, int width, int height, int pixelFormat, PreviewFrameType frame);
    virtual void sendNextFrame(PreviewFrameType frame);

//Internal class definitions

class FrameCallback : public Thread {
    FakeCameraAdapter* mCameraAdapter;
    public:
        FrameCallback(FakeCameraAdapter* ca)
            : Thread(false), mCameraAdapter(ca) { }

        virtual bool threadLoop() {
            mCameraAdapter->frameCallbackThread();
            return false;
        }
};

class FramePreview : public Thread {
    FakeCameraAdapter* mCameraAdapter;
    public:
        FramePreview(FakeCameraAdapter* ca)
            : Thread(false), mCameraAdapter(ca) { }

        virtual bool threadLoop() {
            mCameraAdapter->frameThread();
            return false;
        }
};

    //friend declarations
    friend class FramePreview;
    friend class FrameCallback;

    enum FrameCallbackCommands {
        CALL_CALLBACK = 0,
        CALLBACK_EXIT
    };

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    //startPreview timestamp from CameraHAL
    struct timeval *mStartPreview;
    //autoFocus timestamp from CameraHAL
    struct timeval *mStartFocus;
    //takePicture timestamp from CameraHAL
    struct timeval *mStartCapture;

#endif

    sp<FramePreview> mFrameThread;
    sp<FrameCallback> mCallbackThread;
    int mImageBufferCount;
    int *mImageBuffers;
    uint32_t *mImageOffsets;
    int mImageFd;
    int mPreviewBufferCount;
    int *mPreviewBuffers;
    uint32_t *mPreviewOffsets;
    int mPreviewFd;
    KeyedVector<int, int> mPreviewBuffersRefCount;
    int mPreviewWidth, mPreviewHeight, mPreviewFormat;
    int mCaptureWidth, mCaptureHeight, mCaptureFormat;
    int mFrameRate;
    CameraParameters mParameters;
    MessageQueue mCallbackQ;
    mutable Mutex mPreviewBufferLock;
    MessageQueue mFrameQ;
    MessageQueue mAdapterQ;



};

};

#endif //FAKE_CAMERA_ADAPTER_H


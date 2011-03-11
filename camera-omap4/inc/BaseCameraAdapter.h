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


#ifndef BASE_CAMERA_ADAPTER_H
#define BASE_CAMERA_ADAPTER_H

#include "CameraHal.h"

namespace android {

class BaseCameraAdapter : public CameraAdapter
{

public:

    BaseCameraAdapter();

    ///Initialzes the camera adapter creates any resources required
    virtual status_t initialize(int sensor_index = 0) = 0;

    virtual int setErrorHandler(ErrorNotifier *errorNotifier);

    //Message/Frame notification APIs
    virtual void enableMsgType(int32_t msgs, frame_callback callback=NULL, event_callback eventCb=NULL, void* cookie=NULL);
    virtual void disableMsgType(int32_t msgs, void* cookie);
    virtual void returnFrame(void * frameBuf, CameraFrame::FrameType frameType);

    //APIs to configure Camera adapter and get the current parameter set
    virtual status_t setParameters(const CameraParameters& params) = 0;
    virtual void getParameters(CameraParameters& params)  = 0;

    //API to get the caps
    virtual status_t getCaps(CameraParameters &params) = 0;

    //Used together with capabilities
    virtual int getRevision() = 0;

    //API to send a command to the camera
    virtual status_t sendCommand(int operation, int value1 = 0, int value2 = 0, int value3 = 0 );

    //API to cancel a currently executing command
    virtual status_t cancelCommand(int operation);

    //API to get the frame size required to be allocated. This size is used to override the size passed
    //by camera service when VSTAB/VNF is turned ON for example
    virtual void getFrameSize(int &width, int &height) = 0;

    virtual status_t getFrameDataSize(size_t &dataFrameSize, size_t bufferCount) = 0;

    virtual status_t getPictureBufferSize(size_t &length, size_t bufferCount) = 0;

    virtual status_t registerImageReleaseCallback(release_image_buffers_callback callback, void *user_data);

    virtual status_t registerEndCaptureCallback(end_image_capture_callback callback, void *user_data);

protected:

    //-----------Interface that needs to be implemented by deriving classes --------------------

    //Should be implmented by deriving classes in order to start image capture
    virtual status_t takePicture();

    //Should be implmented by deriving classes in order to start image capture
    virtual status_t stopImageCapture();

    //Should be implmented by deriving classes in order to start temporal bracketing
    virtual status_t startBracketing(int range);

    //Should be implemented by deriving classes in order to stop temporal bracketing
    virtual status_t stopBracketing();

    //Should be implemented by deriving classes in oder to initiate autoFocus
    virtual status_t autoFocus();

    //Should be implemented by deriving classes in oder to initiate autoFocus
    virtual status_t cancelAutoFocus();

    //Should be called by deriving classes in order to deinit base class
    virtual status_t setTimeOut(int sec);

    //Should be called by deriving classes in order to cancel deinit timeout
    virtual status_t cancelTimeOut();

    //Should be called by deriving classes in order to do some bookkeeping
    virtual status_t startVideoCapture();

    //Should be called by deriving classes in order to do some bookkeeping
    virtual status_t stopVideoCapture();

    //Should be implemented by deriving classes in order to start camera preview
    virtual status_t startPreview();

    //Should be implemented by deriving classes in order to stop camera preview
    virtual status_t stopPreview();

    //Should be implemented by deriving classes in order to start smooth zoom
    virtual status_t startSmoothZoom(int targetIdx);

    //Should be implemented by deriving classes in order to stop smooth zoom
    virtual status_t stopSmoothZoom();

    //Should be implemented by deriving classes in order to stop smooth zoom
    virtual status_t useBuffers(CameraMode mode, void* bufArr, int num, size_t length);

    //Should be implemented by deriving classes in order queue a released buffer in CameraAdapter
    virtual status_t fillThisBuffer(void* frameBuf, CameraFrame::FrameType frameType);

    // ---------------------Interface ends-----------------------------------

    status_t notifyFocusSubscribers(bool status);
    status_t notifyShutterSubscribers();
    status_t notifyZoomSubscribers(int zoomIdx, bool targetReached);

    //Send the frame to subscribers
    status_t sendFrameToSubscribers(CameraFrame *frame);

    //Resets the refCount for this particular frame
    status_t resetFrameRefCount(CameraFrame &frame);

    //A couple of helper functions
    void setFrameRefCount(void* frameBuf, CameraFrame::FrameType frameType, int refCount);
    int getFrameRefCount(void* frameBuf, CameraFrame::FrameType frameType);
    size_t getSubscriberCount(CameraFrame::FrameType frameType);

    enum FrameState {
        STOPPED = 0,
        RUNNING
    };

    enum FrameCommands {
        START_PREVIEW = 0,
        START_RECORDING,
        RETURN_FRAME,
        STOP_PREVIEW,
        STOP_RECORDING,
        DO_AUTOFOCUS,
        TAKE_PICTURE,
        FRAME_EXIT
    };

    enum AdapterCommands {
        ACK = 0,
        ERROR
    };

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    struct timeval mStartFocus;
    struct timeval mStartCapture;

#endif

    //Different frame subscribers get stored using these
    KeyedVector<int, frame_callback> mFrameSubscribers;
    KeyedVector<int, frame_callback> mFrameDataSubscribers;
    KeyedVector<int, frame_callback> mVideoSubscribers;
    KeyedVector<int, frame_callback> mImageSubscribers;
    KeyedVector<int, frame_callback> mRawSubscribers;
    KeyedVector<int, event_callback> mFocusSubscribers;
    KeyedVector<int, event_callback> mZoomSubscribers;
    KeyedVector<int, event_callback> mShutterSubscribers;

    //Preview buffer management data
    int *mPreviewBuffers;
    int mPreviewBufferCount;
    size_t mPreviewBuffersLength;
    KeyedVector<int, int> mPreviewBuffersAvailable;
    mutable Mutex mPreviewBufferLock;

    //Video buffer management data
    int *mVideoBuffers;
    KeyedVector<int, int> mVideoBuffersAvailable;
    int mVideoBuffersCount;
    size_t mVideoBuffersLength;
    mutable Mutex mVideoBufferLock;

    //Image buffer management data
    int *mCaptureBuffers;
    KeyedVector<int, bool> mCaptureBuffersAvailable;
    int mCaptureBuffersCount;
    size_t mCaptureBuffersLength;
    mutable Mutex mCaptureBufferLock;

    //Metadata buffermanagement
    int *mPreviewDataBuffers;
    KeyedVector<int, bool> mPreviewDataBuffersAvailable;
    int mPreviewDataBuffersCount;
    size_t mPreviewDataBuffersLength;
    mutable Mutex mPreviewDataBufferLock;

    MessageQueue mFrameQ;
    MessageQueue mAdapterQ;
    mutable Mutex mSubscriberLock;
    ErrorNotifier *mErrorNotifier;
    release_image_buffers_callback mReleaseImageBuffersCallback;
    end_image_capture_callback mEndImageCaptureCallback;
    void *mReleaseData;
    void *mEndCaptureData;
    bool mRecording;
};

};

#endif //BASE_CAMERA_ADAPTER_H



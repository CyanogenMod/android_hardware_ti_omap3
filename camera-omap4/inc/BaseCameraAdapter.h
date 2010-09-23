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
    virtual status_t initialize(int sensor_index=0) = 0;

    virtual int setErrorHandler(ErrorNotifier *errorNotifier);

    //Message/Frame notification APIs
    virtual void enableMsgType(int32_t msgs, frame_callback callback=NULL, event_callback eventCb=NULL, void* cookie=NULL);
    virtual void disableMsgType(int32_t msgs, void* cookie);
    virtual void returnFrame(void* frameBuf, CameraFrame::FrameType frameType) = 0;

    //APIs to configure Camera adapter and get the current parameter set
    virtual status_t setParameters(const CameraParameters& params) = 0;
    virtual void getParameters(CameraParameters& params) const = 0;

    //API to get the caps
    virtual status_t getCaps() = 0;

    //API to send a command to the camera
    virtual status_t sendCommand(int operation, int value1=0, int value2=0, int value3=0) = 0;

    //API to cancel a currently executing command
    virtual status_t cancelCommand(int operation) = 0;

    //API to get the frame size required to be allocated. This size is used to override the size passed
    //by camera service when VSTAB/VNF is turned ON for example
    virtual void getFrameSize(int &width, int &height) = 0;

    virtual status_t getPictureBufferSize(size_t &length, size_t bufferCount) = 0;

    virtual status_t registerImageReleaseCallback(release_image_buffers_callback callback, void *user_data);

    virtual status_t registerEndCaptureCallback(end_image_capture_callback callback, void *user_data);

protected:

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

    KeyedVector<int, frame_callback> mFrameSubscribers;
    KeyedVector<int, frame_callback> mVideoSubscribers;
    KeyedVector<int, frame_callback> mImageSubscribers;
    KeyedVector<int, frame_callback> mRawSubscribers;
    KeyedVector<int, event_callback> mFocusSubscribers;
    KeyedVector<int, event_callback> mZoomSubscribers;
    KeyedVector<int, event_callback> mShutterSubscribers;
    MessageQueue mFrameQ;
    MessageQueue mAdapterQ;
    mutable Mutex mSubscriberLock;
    ErrorNotifier *mErrorNotifier;
    release_image_buffers_callback mReleaseImageBuffersCallback;
    end_image_capture_callback mEndImageCaptureCallback;
    void *mReleaseData;
    void *mEndCaptureData;
};

};

#endif //BASE_CAMERA_ADAPTER_H



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


#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_H

#include <stdio.h>
#include <V4L2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <utils/Log.h>
#include <utils/threads.h>
#include <utils/MemoryBase.h>
#include <utils/MemoryHeapBase.h>
#include <utils/threads.h>
#include <ui/CameraHardwareInterface.h>
#if HARDWARE_OMX
#include "SkImageEncoder_libtijpeg.h"
#include "SkBitmap.h"
#endif

namespace android {

class CameraHal : public CameraHardwareInterface {
public:
    virtual sp<IMemoryHeap> getPreviewHeap() const;

    virtual status_t    startPreview(preview_callback cb, void* user);
    virtual void        stopPreview();
    virtual bool        previewEnabled();

    virtual status_t    autoFocus(autofocus_callback, void *user);
    virtual status_t    takePicture(shutter_callback,
                                    raw_callback,
                                    jpeg_callback,
                                    void* user);
    virtual status_t    cancelPicture(bool cancel_shutter,
                                      bool cancel_raw,
                                      bool cancel_jpeg);
    virtual status_t    dump(int fd, const Vector<String16>& args) const;
    virtual status_t    setParameters(const CameraParameters& params);
    virtual CameraParameters  getParameters() const;
    virtual void release();

    static sp<CameraHardwareInterface> createInstance();

private:
                        CameraHal();
    virtual             ~CameraHal();

    static wp<CameraHardwareInterface> singleton;
    static int camera_device;
    static const int kBufferCount = 4;

    class PreviewThread : public Thread {
        CameraHal* mHardware;
    public:
        PreviewThread(CameraHal* hw)
            : Thread(false), mHardware(hw) { }
        virtual void onFirstRef() {
            run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);
        }
        virtual bool threadLoop() {
            mHardware->previewThread();
            // loop until we need to quit
            return true;
        }
    };

    void initDefaultParameters();
    bool initHeapLocked();

    int previewThread();

    static int beginAutoFocusThread(void *cookie);
    int autoFocusThread();

    static int beginPictureThread(void *cookie);
    int pictureThread();

    int validateSize(int w, int h);
    void* cropImage(unsigned long buffer);
    void convertYUYVtoYUV422SP(uint8_t *inputBuffer, uint8_t *outputBuffer, int width, int height);
#if HARDWARE_OMX
    void convertYUYVtoUYVY(uint8_t *inputBuffer, uint8_t *outputBuffer, int width, int height);
    sp<MemoryBase> encodeImage(void *buffer, uint32_t bufflen);
#endif

    int fcount;
    mutable Mutex       mLock;

    CameraParameters    mParameters;

    sp<MemoryHeapBase>  mHeap;
    sp<MemoryHeapBase>  mSurfaceFlingerHeap;
    sp<MemoryHeapBase>  mPictureHeap;
    sp<MemoryBase>      mBuffers[kBufferCount];
    sp<MemoryBase>      mSurfaceFlingerBuffer;

    bool                mPreviewRunning;
    int                 mPreviewFrameSize;

    shutter_callback    mShutterCallback;
    raw_callback        mRawPictureCallback;
    jpeg_callback       mJpegPictureCallback;
    void                *mPictureCallbackCookie;

    // protected by mLock
    sp<PreviewThread>   mPreviewThread;
    preview_callback    mPreviewCallback;
    void                *mPreviewCallbackCookie;

    autofocus_callback  mAutoFocusCallback;
    void                *mAutoFocusCallbackCookie;

    // only used from PreviewThread
    int                 mCurrentPreviewFrame;

    int nQueued;
    int nDequeued;
    bool previewStopped;
    bool doubledPreviewWidth;
    bool doubledPreviewHeight;
};

}; // namespace android

#endif


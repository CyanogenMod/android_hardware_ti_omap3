/*
 * Copyright (C) 2009 The Android Open Source Project
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

#define LOG_TAG "TIHardwareRenderer"
#include <utils/Log.h>
#include "TIHardwareRenderer.h"
#include <media/stagefright/MediaDebug.h>
#include <surfaceflinger/ISurface.h>
#include <ui/Overlay.h>
#include <cutils/properties.h>

#define UNLIKELY( exp ) (__builtin_expect( (exp) != 0, false ))

namespace android {

static int mDebugFps = 0;

/*
    To print the FPS, type this command on the console before starting playback:
    setprop debug.video.showfps 1
    To disable the prints, type:
    setprop debug.video.showfps 0

*/

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
        LOGD("%d Frames, %f FPS", mFrameCount, mFps);
    }
    // XXX: mFPS has the value we want
}

TIHardwareRenderer::TIHardwareRenderer(
        const sp<ISurface> &surface,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight,
        OMX_COLOR_FORMATTYPE colorFormat)
    : mISurface(surface),
      mDisplayWidth(displayWidth),
      mDisplayHeight(displayHeight),
      mDecodedWidth(decodedWidth),
      mDecodedHeight(decodedHeight),
      mColorFormat(colorFormat),
      mInitCheck(NO_INIT),
      mFrameSize(mDecodedWidth * mDecodedHeight * 2),
      mIsFirstFrame(true),
      mIndex(0) {

    sp<IMemory> mem;
    mapping_data_t *data;

    CHECK(mISurface.get() != NULL);
    CHECK(mDecodedWidth > 0);
    CHECK(mDecodedHeight > 0);

    sp<OverlayRef> ref = mISurface->createOverlay(
            mDecodedWidth, mDecodedHeight, OVERLAY_FORMAT_CbYCrY_422_I, 0);

    if (ref.get() == NULL) {
        return;
    }

    mOverlay = new Overlay(ref);
    
    for (size_t i = 0; i < (size_t)mOverlay->getBufferCount(); ++i) {
        data = (mapping_data_t *)mOverlay->getBufferAddress((void *)i);
        mVideoHeaps[i] = new MemoryHeapBase(data->fd,data->length, 0, data->offset);
        mem = new MemoryBase(mVideoHeaps[i], 0, data->length);
        CHECK(mem.get() != NULL);
        LOGV("mem->pointer[%d] = %p", i, mem->pointer());
        mOverlayAddresses.push(mem);
    }

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.video.showfps", value, "0");
    mDebugFps = atoi(value);
    LOGD_IF(mDebugFps, "showfps enabled");

    mInitCheck = OK;
}

TIHardwareRenderer::~TIHardwareRenderer() {
    sp<IMemory> mem;

    if (mOverlay.get() != NULL) {

        for (size_t i = 0; i < mOverlayAddresses.size(); ++i) {
            //(mOverlayAddresses[i]).clear();
            mVideoHeaps[i].clear();
        }

        mOverlay->destroy();
        mOverlay.clear();

        // XXX apparently destroying an overlay is an asynchronous process...
        sleep(1);
    }
}

// return a byte offset from any pointer
static inline const void *byteOffset(const void* p, size_t offset) {
    return ((uint8_t*)p + offset);
}

static void convertYuv420ToYuv422(
        int width, int height, const void *src, void *dst) {
    // calculate total number of pixels, and offsets to U and V planes
    int pixelCount = height * width;
    int srcLineLength = width / 4;
    int destLineLength = width / 2;
    uint32_t* ySrc = (uint32_t*) src;
    const uint16_t* uSrc = (const uint16_t*) byteOffset(src, pixelCount);
    const uint16_t* vSrc = (const uint16_t*) byteOffset(uSrc, pixelCount >> 2);
    uint32_t *p = (uint32_t*) dst;

    // convert lines
    for (int i = 0; i < height; i += 2) {

        // upsample by repeating the UV values on adjacent lines
        // to save memory accesses, we handle 2 adjacent lines at a time
        // convert 4 pixels in 2 adjacent lines at a time
        for (int j = 0; j < srcLineLength; j++) {

            // fetch 4 Y values for each line
            uint32_t y0 = ySrc[0];
            uint32_t y1 = ySrc[srcLineLength];
            ySrc++;

            // fetch 2 U/V values
            uint32_t u = *uSrc++;
            uint32_t v = *vSrc++;

            // assemble first U/V pair, leave holes for Y's
            uint32_t uv = (u | (v << 16)) & 0x00ff00ff;

            // OR y values and write to memory
            p[0] = ((y0 & 0xff) << 8) | ((y0 & 0xff00) << 16) | uv;
            p[destLineLength] = ((y1 & 0xff) << 8) | ((y1 & 0xff00) << 16) | uv;
            p++;

            // assemble second U/V pair, leave holes for Y's
            uv = ((u >> 8) | (v << 8)) & 0x00ff00ff;

            // OR y values and write to memory
            p[0] = ((y0 >> 8) & 0xff00) | (y0 & 0xff000000) | uv;
            p[destLineLength] = ((y1 >> 8) & 0xff00) | (y1 & 0xff000000) | uv;
            p++;
        }

        // skip the next y line, we already converted it
        ySrc += srcLineLength;
        p += destLineLength;
    }
}

void TIHardwareRenderer::render(
        const void *data, size_t size, void *platformPrivate) {

    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
    }

    overlay_buffer_t overlay_buffer;
    size_t i = 0;
    
    if (mOverlay.get() == NULL) {
        return;
    }

    if (mColorFormat == OMX_COLOR_FormatYUV420Planar) {
        convertYuv420ToYuv422(mDecodedWidth, mDecodedHeight, data, mOverlayAddresses[mIndex]->pointer());
    } 
    else {

        CHECK_EQ(mColorFormat, OMX_COLOR_FormatCbYCrY);

        for (; i < mOverlayAddresses.size(); ++i) {
            if (mOverlayAddresses[i]->pointer() == data) {
                break;
            }
        }

        if (i == mOverlayAddresses.size()) {
            LOGE("Doing a memcpy. Report this issue.");
            memcpy(mOverlayAddresses[mIndex]->pointer(), data, size);
        }
        else{
            mIndex = i;
        }
    }

    mOverlay->queueBuffer((void *)mIndex);
    mOverlay->dequeueBuffer(&overlay_buffer);
    
    if (++mIndex == mOverlayAddresses.size()) mIndex = 0;

}

}  // namespace android


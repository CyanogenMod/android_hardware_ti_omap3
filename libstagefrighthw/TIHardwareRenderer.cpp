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

#define ARMPAGESIZE 4096

////////////////////////////////////////////////////////////////////////////////

static int Calculate_TotalRefFrames(int nWidth, int nHeight) {
    LOGD("Calculate_TotalRefFrames");
    uint32_t ref_frames = 0;
    uint32_t MaxDpbMbs;
    uint32_t PicWidthInMbs;
    uint32_t FrameHeightInMbs;

    MaxDpbMbs = 32768; //Maximum value for upto level 4.1

    PicWidthInMbs = nWidth / 16;

    FrameHeightInMbs = nHeight / 16;

    ref_frames =  (uint32_t)(MaxDpbMbs / (PicWidthInMbs * FrameHeightInMbs));

    LOGD("nWidth [%d] PicWidthInMbs [%d] nHeight [%d] FrameHeightInMbs [%d] ref_frames [%d]",
        nWidth, PicWidthInMbs, nHeight, FrameHeightInMbs, ref_frames);

    ref_frames = (ref_frames > 16) ? 16 : ref_frames;

    LOGD("Final ref_frames [%d]", ref_frames);

    return (ref_frames + 3 + 2*NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE);

}

//S3D
TIHardwareRenderer::TIHardwareRenderer(
        const sp<ISurface> &surface,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight,
        OMX_COLOR_FORMATTYPE colorFormat,
        int32_t rotationDegrees,
        int isS3D, int numOfOpBuffers)
    : mISurface(surface),
      mDisplayWidth(displayWidth),
      mDisplayHeight(displayHeight),
      mDecodedWidth(decodedWidth),
      mDecodedHeight(decodedHeight),
      mColorFormat(colorFormat),
      mInitCheck(NO_INIT),
      mFrameSize(mDecodedWidth * mDecodedHeight * 2),
      nOverlayBuffersQueued(0),
      mIndex(0),
      release_frame_cb(0),
      mCropX(-1),
      mCropY(-1) {

    sp<IMemory> mem;
    mapping_data_t *data;

    CHECK(mISurface.get() != NULL);
    CHECK(mDecodedWidth > 0);
    CHECK(mDecodedHeight > 0);

    int videoFormat = OVERLAY_FORMAT_CbYCrY_422_I;
    if ((colorFormat == OMX_COLOR_FormatYUV420PackedSemiPlanar)||(colorFormat == OMX_COLOR_FormatYUV420SemiPlanar))
   {
    videoFormat = OVERLAY_FORMAT_YCbCr_420_SP;
   }
    else if ((colorFormat == OMX_TI_COLOR_FormatYUV420PackedSemiPlanar_Sequential_TopBottom))
   {
    videoFormat = OVERLAY_FORMAT_YCbCr_420_SP_SEQ_TB;
   }
    else if(colorFormat == OMX_COLOR_FormatCbYCrY)
   {
   videoFormat =  OVERLAY_FORMAT_CbYCrY_422_I;
   }
    else if (colorFormat == OMX_COLOR_FormatYCbYCr)
   {
   videoFormat = OVERLAY_FORMAT_YCbYCr_422_I;
   }
    else if (colorFormat == OMX_COLOR_FormatYUV420Planar)
   {
#ifdef TARGET_OMAP4
   videoFormat = OVERLAY_FORMAT_YCbCr_420_SP;
#else
   videoFormat = OVERLAY_FORMAT_CbYCrY_422_I;
#endif
   LOGI("Use YUV420_PLANAR -> YUV422_INTERLEAVED_UYVY converter or NV-12 converter needed");
    }
    else
   {
   LOGI("Not Supported format, and no coverter available");
   return;
    }

    uint32_t orientation;
    switch (rotationDegrees) {
        case 0: orientation = ISurface::BufferHeap::ROT_0; break;
        case 90: orientation = ISurface::BufferHeap::ROT_90; break;
        case 180: orientation = ISurface::BufferHeap::ROT_180; break;
        case 270: orientation = ISurface::BufferHeap::ROT_270; break;
        default: orientation = ISurface::BufferHeap::ROT_0; break;
    }

//S3D
#ifdef TARGET_OMAP4
    sp<OverlayRef> ref = mISurface->createOverlay(
        mDecodedWidth, mDecodedHeight, videoFormat, orientation, isS3D);
#else
    sp<OverlayRef> ref = mISurface->createOverlay(
        mDecodedWidth, mDecodedHeight, videoFormat, orientation);
#endif

    if (ref.get() == NULL) {
        return;
    }

    mOverlay = new Overlay(ref);

#ifdef TARGET_OMAP4
    if(numOfOpBuffers == -1) {
        /* Calculate the number of overlay buffers required, based on the video resolution
        * and resize the overlay for the new number of buffers
        */
        int overlaybuffcnt = Calculate_TotalRefFrames(mDisplayWidth, mDisplayHeight);
        int initialcnt = mOverlay->getBufferCount();
        if (overlaybuffcnt != initialcnt) {
            mOverlay->setParameter(OVERLAY_NUM_BUFFERS, overlaybuffcnt);
            mOverlay->resizeInput(mDecodedWidth, mDecodedHeight);
        }
        mOverlay->setParameter(OPTIMAL_QBUF_CNT, 2*NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE);
    } else {
       /* Number of buffers will be set as recommended by user or codec */
       LOGD("Overlay Buffer Count [as recommneded] %d",numOfOpBuffers);
       mOverlay->setParameter(OVERLAY_NUM_BUFFERS, numOfOpBuffers);
       mOverlay->resizeInput(mDecodedWidth, mDecodedHeight);
       mOverlay->setParameter(OPTIMAL_QBUF_CNT, NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE*2);
    }
#endif

    for (size_t i = 0; i < (size_t)mOverlay->getBufferCount(); ++i) {
        data = (mapping_data_t *)mOverlay->getBufferAddress((void *)i);
        CHECK(data != NULL);
        mVideoHeaps[i] = new MemoryHeapBase(data->fd,data->length, 0, data->offset);
        mem = new MemoryBase(mVideoHeaps[i], 0, data->length);
        CHECK(mem.get() != NULL);
        LOGV("mem->pointer[%d] = %p", i, mem->pointer());
        mOverlayAddresses.push(mem);
        buffers_queued_to_dss[i] = 0;
    }
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.video.showfps", value, "0");
    mDebugFps = atoi(value);
    LOGD_IF(mDebugFps, "showfps enabled");

    mInitCheck = OK;
}

void TIHardwareRenderer::resizeRenderer(void* resize_params) {
    render_resize_params* resizeparams;
    resizeparams = (render_resize_params*)(resize_params);
    LOGD("resizeRenderer %dx%d buff(%d)",resizeparams->decoded_width, resizeparams->decoded_height, \
                                         resizeparams->buffercount);
    //first check if the size is different or not
    if ((mDecodedWidth != resizeparams->decoded_width) || (mDecodedHeight != resizeparams->decoded_height) || \
         (mDisplayWidth != resizeparams->display_width) || (mDisplayHeight != resizeparams->display_height)) {
        //update the renderer's new width and height
        mDecodedWidth = resizeparams->decoded_width;
        mDecodedHeight = resizeparams->decoded_height;
        mDisplayWidth = resizeparams->display_width;
        mDisplayHeight = resizeparams->display_height;
        mCropX = mCropY = -1;

        //unmap and delete the heap space for the old buffers
        sp<IMemory> mem;
        mapping_data_t *data;
        sp<IMemory> mem_tobe_deleted;
        unsigned int sz = mOverlayAddresses.size();
        if (mOverlay.get() != NULL) {
            for (size_t i = 0; i < sz; ++i) {
                mem_tobe_deleted = mOverlayAddresses[i];
                mem_tobe_deleted.clear();
                (mVideoHeaps[i].get())->dispose();
                mVideoHeaps[i].clear();
            }
            mOverlayAddresses.clear();
        }

        if(sz != resizeparams->buffercount){
            mOverlay->setParameter(OVERLAY_NUM_BUFFERS, resizeparams->buffercount);
        }

        //resize the overlay for the new width and height
        mOverlay->resizeInput(mDecodedWidth, mDecodedHeight);
        //create imem for the new buffers
        for (size_t i = 0; i < (size_t)mOverlay->getBufferCount(); ++i) {
            data = (mapping_data_t *)mOverlay->getBufferAddress((void *)i);
            CHECK(data != NULL);
            mVideoHeaps[i] = new MemoryHeapBase(data->fd,data->length, 0, data->offset);
            mem = new MemoryBase(mVideoHeaps[i], 0, data->length);
            CHECK(mem.get() != NULL);
            LOGV("mem->pointer[%d] = %p", i, mem->pointer());
            mOverlayAddresses.push(mem);
            buffers_queued_to_dss[i] = 0;
        }
    }
}

TIHardwareRenderer::~TIHardwareRenderer() {
    sp<IMemory> mem;
    unsigned int sz = mOverlayAddresses.size();

    if (mOverlay.get() != NULL) {

        for (size_t i = 0; i < sz; ++i) {
            mem = mOverlayAddresses[i];
            mem.clear();
            //dispose the memory allocated on heap explicitly
            (mVideoHeaps[i].get())->dispose();
            mVideoHeaps[i].clear();
        }
        mOverlayAddresses.clear();
        mOverlay->destroy();
        mOverlay.clear();

        // XXX apparently destroying an overlay is an asynchronous process...
        //sleep(1);
    }
}

// return a byte offset from any pointer
static inline const void *byteOffset(const void* p, size_t offset) {
    return ((uint8_t*)p + offset);
}

static void convertYuv420ToNV12(
        int width, int height, const void *src, void *dst) {
    //LOGI("Coverting YUV420 to NV-12 height %d and Width %d", height, width);
   uint32_t stride = 4096;
    //copy y-buffer, almost bytewise copy, except for stride jumps.
    {
        uint8_t* p1y = (uint8_t*) src;
        uint8_t* p2y = (uint8_t*) dst;
        for(int i=0;i<height;i++)
        {
            //copy whole row of Y pixels. source and desination will point to new row each time.
            memcpy(p2y+i*stride, p1y+i*width,width);
        }
    }

//copy uv buffers
//rearrange from  planar [uuu][vvvv] to packed planar [uvuvuv]  packages pixel wise
    {
        uint8_t* p2uv  = NULL;
        p2uv  = (uint8_t*) dst;
        p2uv  += stride * height;

        uint8_t* p1u = ((uint8_t*) src + (width*height));
        uint8_t* p1v = ((uint8_t*) p1u + ((width/2)*(height/2)));

        for(int i=0;(i<height/2);i++)
        {
            for(int j=0,j1=0;(j<width/2);j++,j1+=2)
            {
                p2uv[j1] = p1u[j];
                p2uv[j1+1] = p1v[j];
            }
            p2uv+=stride;
            p1u+=width/2;
            p1v+=width/2;
        }
    }
}

static void convertYuv420ToYuv422(
        int width, int height, const void *src, void *dst) {

#ifdef TARGET_OMAP4
  // calculate total number of pixels, and offsets to U and V planes
    uint32_t pixelCount =  height * width;

    uint8_t* ySrc = (uint8_t*) src;
    uint8_t* uSrc = (uint8_t*) ((uint8_t*)src + pixelCount);
    uint8_t* vSrc = (uint8_t*) ((uint8_t*)src + pixelCount + pixelCount/4);
    uint8_t *p = (uint8_t*) dst;
    uint32_t page_width = (width * 2   + 4096 -1) & ~(4096 -1);  // width rounded to the 4096 bytes

   //LOGI("Coverting YUV420 to YUV422 - Height %d and Width %d", height, width);

     // convert lines
    for (int i = 0; i < height  ; i += 2) {
        for (int j = 0; j < width; j+= 2) {

         //  These Y have the same CR and CRB....
         //  Y0 Y01......
         //  Y1 Y11......

         // SRC buffer from the algorithm might be giving YVU420 as well
         *(uint32_t *)(p) = (   ((uint32_t)(ySrc[1] << 16)   | (uint32_t)(ySrc[0]))  & 0x00ff00ff ) |
                                    (  ((uint32_t)(*uSrc << 8) | (uint32_t)(*vSrc << 24))  & 0xff00ff00 ) ;

         *(uint32_t *)(p + page_width) = (   ((uint32_t)(ySrc[width +1] << 16)   | (uint32_t)(ySrc[width]))    & 0x00ff00ff ) |
                                                            (  ((uint32_t)(*uSrc++ << 8) | (uint32_t)(*vSrc++ << 24))   & 0xff00ff00 );

            p += 4;
            ySrc += 2;
         }

        // skip the next y line, we already converted it
        ySrc += width;     // skip the next row as it was already filled above
        p    += 2* page_width - width * 2; //go to the beginning of the next row
    }

#else
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
#endif
}

void TIHardwareRenderer::render(
        const void *data, size_t size, void *platformPrivate) {

    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
    }

    overlay_buffer_t overlay_buffer;
    size_t i = 0;
    int cropX = 0;
    int cropY = 0;

    if (mOverlay.get() == NULL) {
        return;
    }

    if (mColorFormat == OMX_COLOR_FormatYUV420Planar) {
#ifdef TARGET_OMAP4
        convertYuv420ToNV12(mDecodedWidth, mDecodedHeight, data, mOverlayAddresses[mIndex]->pointer());
#else
        convertYuv420ToYuv422(mDecodedWidth, mDecodedHeight, data, mOverlayAddresses[mIndex]->pointer());
#endif
    }
    else {
        for (; i < mOverlayAddresses.size(); ++i) {
            /**
            *In order to support the offset from the decoded buffers, we have to check for
            * the range of offset with in the buffer. Here we can't check for the base address
            * and also, the offset should be used for crop window position calculation
            * we are getting the Baseaddress + offset
            **/
            unsigned int offsetinPixels = (char*)data - (char*)mOverlayAddresses[i]->pointer();
            if(offsetinPixels < size){
                cropY = (offsetinPixels)/ARMPAGESIZE;
                cropX = (offsetinPixels)%ARMPAGESIZE;
                if( (cropY != mCropY) || (cropX != mCropX))
                {
                    mCropY = cropY;
                    mCropX = cropX;
                    mOverlay->setCrop((uint32_t)cropX, (uint32_t)cropY, mDisplayWidth, mDisplayHeight);
                }
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

    int nBuffers_queued_to_dss = mOverlay->queueBuffer((void *)mIndex);
    if (release_frame_cb) {
        if (nBuffers_queued_to_dss < 0){
            release_frame_cb(mOverlayAddresses[mIndex], cookie);
        }
        else
        {
            nOverlayBuffersQueued++;
            buffers_queued_to_dss[mIndex] = 1; 
            if (nBuffers_queued_to_dss != nOverlayBuffersQueued) // STREAM OFF occurred
            {
                LOGD("nBuffers_queued_to_dss = %d, nOverlayBuffersQueued = %d", nBuffers_queued_to_dss, nOverlayBuffersQueued);
                LOGD("buffers in DSS \n %d %d %d  %d %d %d", buffers_queued_to_dss[0], buffers_queued_to_dss[1],
                buffers_queued_to_dss[2], buffers_queued_to_dss[3], buffers_queued_to_dss[4], buffers_queued_to_dss[5]);
                //Release all the buffers that were discarded by DSS upon STREAM OFF.
                for(unsigned int k = 0; k < (unsigned int)mOverlay->getBufferCount(); k++)
                {
                    if ((buffers_queued_to_dss[k] == 1) && (k != mIndex))
                    {
                        buffers_queued_to_dss[k] = 0;
                        nOverlayBuffersQueued--;
                        release_frame_cb(mOverlayAddresses[k], cookie);
                        LOGD("Reclaiming the buffer [%d] from Overlay", k);
                    }
                }
            }
        }
    }    
    
    int err = mOverlay->dequeueBuffer(&overlay_buffer);
    if (err == 0) {
        nOverlayBuffersQueued--;
        buffers_queued_to_dss[(int)overlay_buffer] = 0;
        if (release_frame_cb)
            release_frame_cb(mOverlayAddresses[(int)overlay_buffer], cookie);
    }

    if (++mIndex == mOverlayAddresses.size()) mIndex = 0;
}

bool TIHardwareRenderer::setCallback(release_rendered_buffer_callback cb, void *c)
{
    release_frame_cb = cb;
    cookie = c;
    return true;
}

void TIHardwareRenderer::set_s3d_frame_layout(uint32_t s3d_mode, uint32_t s3d_fmt, uint32_t s3d_order, uint32_t s3d_subsampling)
{
    LOGD("Setting the frameLayout  s3d_mode=%d s3d_fmt=%d, s3d_order=%d, s3d_subsampling=%d,  \n", s3d_mode, s3d_fmt, s3d_order, s3d_subsampling);
    if(mOverlay->set_s3d_params(s3d_mode, s3d_fmt, s3d_order, s3d_subsampling))
        LOGE("Error Setting S3D params \n");
}

void TIHardwareRenderer::requestRendererClone(bool enable) {
    LOGD("requestRendererClone[%d]", enable);
    int clonefd = mISurface->requestOverlayClone(enable);
    //this one would return the duped fd for the clone device
    //set this with the data context
    mOverlay->setParameter(SET_CLONE_FD, clonefd);
    if (enable) {
        mOverlay->setCrop((uint32_t)mCropX, (uint32_t)mCropY, mDisplayWidth, mDisplayHeight);
    }
}

}  // namespace android


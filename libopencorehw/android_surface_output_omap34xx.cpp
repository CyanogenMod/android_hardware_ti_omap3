/*
 * OMAP3430 support
 *
 * Author: Srini Gosangi <srini.gosangi@windriver.com>
 * Author: Michael Barabanov <michael.barabanov@windriver.com>

 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 */

/* ------------------------------------------------------------------
 * Copyright (C) 2008 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */


//#define LOG_NDEBUG 0
#ifdef LOG_NDEBUG
#warning LOG_NDEBUG ##LOG_NDEBUG##
#endif

#define LOG_TAG "VideoMio34xx"
#include <utils/Log.h>

#include <cutils/properties.h>
#define UNLIKELY( exp ) (__builtin_expect( (exp) != 0, false ))
static int mDebugFps = 0;

#include "android_surface_output_omap34xx.h"
#include "pv_mime_string_utils.h"
#include "pvmf_video.h"
#include <media/PVPlayer.h>

#define CACHEABLE_BUFFERS 0x1

using namespace android;


OSCL_EXPORT_REF AndroidSurfaceOutputOmap34xx::AndroidSurfaceOutputOmap34xx() :
    AndroidSurfaceOutput()
{
    mUseOverlay = true;
    mOverlay = NULL;
}

OSCL_EXPORT_REF AndroidSurfaceOutputOmap34xx::~AndroidSurfaceOutputOmap34xx()
{
    mUseOverlay = false;
}

OSCL_EXPORT_REF bool AndroidSurfaceOutputOmap34xx::initCheck()
{
    LOGV("Calling Vendor(34xx) Specific initCheck");
    
    // reset flags in case display format changes in the middle of a stream
    resetVideoParameterFlags();
    bufEnc = 0;

    // copy parameters in case we need to adjust them
    int displayWidth = iVideoDisplayWidth;
    int displayHeight = iVideoDisplayHeight;
    int frameWidth = iVideoWidth;
    int frameHeight = iVideoHeight;
    int frameSize;
    int videoFormat = OVERLAY_FORMAT_CbYCrY_422_I;
    LOGV("Use Overlays");

    if (mUseOverlay) {
        if(mOverlay == NULL){
            LOGV("using Vendor Speicifc(34xx) codec");
            sp<OverlayRef> ref = mSurface->createOverlay(displayWidth, displayHeight,videoFormat);
            if(ref != NULL)LOGV("Vendor Speicifc(34xx)MIO: overlay created ");
            else LOGV("Vendor Speicifc(34xx)MIO: Creating overlay failed");
            mOverlay = new Overlay(ref);
            mOverlay->setParameter(CACHEABLE_BUFFERS, 0);
        }
        else
        {
            mOverlay->resizeInput(displayWidth, displayHeight);
        }

        mbufferAlloc.maxBuffers = mOverlay->getBufferCount();
        mbufferAlloc.bufferSize = iBufferSize;
        LOGV("number of buffers = %d\n", mbufferAlloc.maxBuffers);
        for (int i = 0; i < mbufferAlloc.maxBuffers; i++) {
            mbufferAlloc.buffer_address[i] = mOverlay->getBufferAddress((void*)i);
            strcpy((char *)mbufferAlloc.buffer_address[i], "hello");
            if (strcmp((char *)mbufferAlloc.buffer_address[i], "hello")) {
                LOGI("problem with buffer\n");
                return mInitialized;
            }else{
                LOGV("buffer = %d allocated addr=%#lx\n", i, (unsigned long) mbufferAlloc.buffer_address[i]);
            }
        }        
    }
    mInitialized = true;
    LOGV("sendEvent(MEDIA_SET_VIDEO_SIZE, %d, %d)", iVideoDisplayWidth, iVideoDisplayHeight);
    mPvPlayer->sendEvent(MEDIA_SET_VIDEO_SIZE, iVideoDisplayWidth, iVideoDisplayHeight);

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.video.showfps", value, "0");
    mDebugFps = atoi(value);
    LOGV_IF(mDebugFps, "showfps enabled");

    return mInitialized;
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
        LOGV("%d Frames, %f FPS", mFrameCount, mFps);
    }
    // XXX: mFPS has the value we want
}

PVMFStatus AndroidSurfaceOutputOmap34xx::writeFrameBuf(uint8* aData, uint32 aDataLen, const PvmiMediaXferHeader& data_header_info)
{
    LOGV(" calling Vendor Speicifc(34xx) writeFrameBuf call");
    if (mSurface == 0) return PVMFFailure;

    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
    }

    if (mUseOverlay) {
        int i;
        for (i = 0; i < mbufferAlloc.maxBuffers; i++) {
            if (mbufferAlloc.buffer_address[i] == aData) {
                break;
            }

        }
        if (i == mbufferAlloc.maxBuffers) {
            LOGE("aData does not match any v4l buffer address\n");
            return PVMFSuccess;
        }
        LOGV("queueBuffer %d\n", i);
        bufEnc = i;
        mOverlay->queueBuffer((void*)bufEnc);
        overlay_buffer_t overlay_buffer;
        if (mOverlay->dequeueBuffer(&overlay_buffer) != NO_ERROR) {
            LOGE("Video (34xx)MIO dequeue buffer failed");
            return false;
        }
    }
    return PVMFSuccess;
}


#define USE_BUFFER_ALLOC 1

/* based on test code in pvmi/media_io/pvmiofileoutput/src/pvmi_media_io_fileoutput.cpp */
void AndroidSurfaceOutputOmap34xx::setParametersSync(PvmiMIOSession aSession,
        PvmiKvp* aParameters,
        int num_elements,
        PvmiKvp * & aRet_kvp)
{
    OSCL_UNUSED_ARG(aSession);
    aRet_kvp = NULL;

#ifndef USE_BUFFER_ALLOC
    AndroidSurfaceOutput::setParametersSync(aSession, aParameters, num_elements, aRet_kvp);
    return;
#endif

    for (int32 i = 0;i < num_elements;i++)
    {
        if (pv_mime_strcmp(aParameters[i].key, MOUT_VIDEO_FORMAT_KEY) == 0)
        {
            iVideoFormatString=aParameters[i].value.pChar_value;
            iVideoFormat=iVideoFormatString.get_str();
            LOGV("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Format Key, Value %s",iVideoFormatString.get_str());
        }
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV) == 0)
        {
            uint8* data = (uint8*)aParameters->value.key_specific_value;
            PVMFYuvFormatSpecificInfo0* yuvInfo = (PVMFYuvFormatSpecificInfo0*)data;

            iVideoWidth = (int32)yuvInfo->width;
            iVideoParameterFlags |= VIDEO_WIDTH_VALID;
            LOGV("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Width, Value %d", iVideoWidth);

            iVideoHeight = (int32)yuvInfo->height;
            iVideoParameterFlags |= VIDEO_HEIGHT_VALID;
            LOGV("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Height, Value %d", iVideoHeight);

            iVideoDisplayHeight = (int32)yuvInfo->display_height;
            iVideoParameterFlags |= DISPLAY_HEIGHT_VALID;
            LOGV("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Display Height, Value %d", iVideoDisplayHeight);


            iVideoDisplayWidth = (int32)yuvInfo->display_width;
            iVideoParameterFlags |= DISPLAY_WIDTH_VALID;
            LOGV("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Display Width, Value %d", iVideoDisplayWidth);

            iNumberOfBuffers = (int32)yuvInfo->num_buffers;
            LOGV("AndroidSurfaceOutputOmap34xx::setParametersSync() Number of Buffer, Value %d", iNumberOfBuffers);

            iBufferSize = (int32)yuvInfo->buffer_size;
            LOGV("AndroidSurfaceOutputOmap34xx::setParametersSync() Buffer Size, Value %d", iBufferSize);
            /* we're not being passed the subformat info */
            iVideoSubFormat = "X-YUV-420";
            iVideoParameterFlags |= VIDEO_SUBFORMAT_VALID;
        }
        else
        {
            //if we get here the key is unrecognized.
            LOGV("AndroidSurfaceOutputOmap34xx::setParametersSync() Error, unrecognized key %s ", aParameters[i].key);

            //set the return value to indicate the unrecognized key
            //and return.
            aRet_kvp = &aParameters[i];
            return;
        }
    }
    /* Copy Code from base class. Ideally we'd just call base class's setParametersSync, but we can't as it will not get to initCheck if it encounters an unrecognized parameter such as the one we're handling here */
    uint32 mycache = iVideoParameterFlags ;
    if( checkVideoParameterFlags() ) {
        initCheck();
    }
    iVideoParameterFlags = mycache;
    if(!iIsMIOConfigured && checkVideoParameterFlags() )
    {
        iIsMIOConfigured = true;
        if(iObserver)
        {
            iObserver->ReportInfoEvent(PVMFMIOConfigurationComplete);
        }
    }
}

/* based on test code in pvmi/media_io/pvmiofileoutput/src/pvmi_media_io_fileoutput.cpp */
PVMFStatus AndroidSurfaceOutputOmap34xx::getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier,
        PvmiKvp*& aParameters, int& num_parameter_elements,
        PvmiCapabilityContext aContext)
{
#ifdef USE_BUFFER_ALLOC
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aContext);
    aParameters=NULL;

    if (strcmp(aIdentifier, PVMF_BUFFER_ALLOCATOR_KEY) == 0)
    {
        int32 err;
        aParameters = (PvmiKvp*)oscl_malloc(sizeof(PvmiKvp));
        if (!aParameters)
        {
            return PVMFErrNoMemory;
        }
        aParameters[0].value.key_specific_value = (PVInterface*)&mbufferAlloc;
        return PVMFSuccess;
    }

#endif
    return AndroidSurfaceOutput::getParametersSync(aSession, aIdentifier, aParameters, num_parameter_elements, aContext);
}

// post the last video frame to refresh screen after pause
void AndroidSurfaceOutputOmap34xx::postLastFrame()
{
    //do nothing here, this is only for override the Android_Surface_output::PostLastFrame()
    LOGV("AndroidSurfaceOutputOmap34xx::postLastFrame()");
    //mSurface->postBuffer(mOffset);
}

void AndroidSurfaceOutputOmap34xx::closeFrameBuf()
{
    LOGV("Vendor(34xx) Speicif CloseFrameBuf");
    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
    }
    if (mSurface.get()) {
        LOGV("unregisterBuffers");
        mSurface->unregisterBuffers();
    }
    if(mOverlay != NULL){
        mOverlay->destroy();
        mOverlay = NULL;
    }
    if (!mInitialized) return;
    mInitialized = false;
}

// factory function for playerdriver linkage
extern "C" AndroidSurfaceOutputOmap34xx* createVideoMio()
{
    LOGV("Creating Vendor(34xx) Specific MIO component");
    return new AndroidSurfaceOutputOmap34xx();
}

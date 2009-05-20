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

#define LOG_NDEBUG 0
#define LOG_TAG "VideoMio34xx"
#include <utils/Log.h>

#include "android_surface_output_omap34xx.h"
#include <media/PVPlayer.h>

using namespace android;


OSCL_EXPORT_REF AndroidSurfaceOutputOmap34xx::AndroidSurfaceOutputOmap34xx() :
    AndroidSurfaceOutput()
{
	mUseOverlay = true;
}

OSCL_EXPORT_REF AndroidSurfaceOutputOmap34xx::~AndroidSurfaceOutputOmap34xx()
{
	mUseOverlay = false;
}

OSCL_EXPORT_REF bool AndroidSurfaceOutputOmap34xx::initCheck()
{
	LOGD("Calling Vendor(34xx) Specific initCheck");
#if 0
    // initialize only when we have all the required parameters
    if (((iVideoParameterFlags & VIDEO_SUBFORMAT_VALID) == 0) || !checkVideoParameterFlags())
        return mInitialized;
#endif
    // reset flags in case display format changes in the middle of a stream
    resetVideoParameterFlags();

    // copy parameters in case we need to adjust them
    int displayWidth = iVideoDisplayWidth;
    int displayHeight = iVideoDisplayHeight;
    int frameWidth = iVideoWidth;
    int frameHeight = iVideoHeight;
    int frameSize;
  	int videoFormat = OVERLAY_FORMAT_YCbCr_422_I;
    LOGD("Use Overlays");
   
    #if 0 //info : Begin :this code copied from camera for reference
    const char *format = params.getPreviewFormat();
    int fmt;
    LOGD("Use Overlays");
    if (!strcmp(format, "yuv422i"))
            fmt = OVERLAY_FORMAT_YCbCr_422_I;
    else if (!strcmp(format, "rgb565"))
            fmt = OVERLAY_FORMAT_RGB_565;
         else {
            LOGE("Invalid preview format for overlays");
            return -EINVAL;
         }
     #endif //info : end
     
    if (mUseOverlay) {
        LOGV("using Vendor Speicifc(34xx) codec");
        sp<OverlayRef> ref = mSurface->createOverlay(displayWidth, displayHeight,videoFormat);
        mOverlay = new Overlay(ref);
    
        mBuffer_count = mRecycle_buffer_count = mOverlay->getBufferCount();
        
        LOGD("number of buffers = %d\n", mBuffer_count);
        for (int i = 0; i < mBuffer_count; i++) {
    	//    overlay_buffer_t overlay_buffer;
        /*	if (mOverlay->dequeueBuffer(&overlay_buffer) != NO_ERROR) {
            	LOGE("Video MIO dequeue buffer failed");
            	return false;
        	}*/
       		mOverlay_buffer_address[i] = mOverlay->getBufferAddress((void*)i);
        	strcpy((char *)mOverlay_buffer_address[i], "hello");
        	if (strcmp((char *)mOverlay_buffer_address[i], "hello")) {
            	LOGI("problem with buffer\n");
            	return mInitialized;
        	}else{
        		LOGD("buffer = %d	allocated\n", i);
        	}
        }        
    }
    mInitialized = true;
    LOGV("sendEvent(MEDIA_SET_VIDEO_SIZE, %d, %d)", iVideoDisplayWidth, iVideoDisplayHeight);
    mPvPlayer->sendEvent(MEDIA_SET_VIDEO_SIZE, iVideoDisplayWidth, iVideoDisplayHeight);
    return mInitialized;
}

PVMFStatus AndroidSurfaceOutputOmap34xx::WriteFrameBuf(uint8* aData, uint32 aDataLen, const PvmiMediaXferHeader& data_header_info)
{
	//LOGD(" calling Vendor Speicifc(34xx) writeFrameBuf call");
    if (mSurface == 0) return PVMFFailure;
  
    if (mUseOverlay) {
    	if(mBuffer_count == 0) {
    		mBuffer_count=mRecycle_buffer_count;
    		//FIX ME : there is no more buffers left for data copy
    		 //LOGD("buffer number = %d\n", mBuffer_count);
    	 }
    	memcpy(mOverlay_buffer_address[mBuffer_count-1],aData,aDataLen);
    	mOverlay->queueBuffer((void*)(mBuffer_count-1));
    	mBuffer_count--;
     /*   overlay_buffer_t overlay_buffer;
       if (mOverlay->dequeueBuffer(&overlay_buffer) != NO_ERROR) {
            	LOGE("Video MIO dequeue buffer failed");
            	return false;
        }
       	mOverlay_buffer_address[index] = mOverlay->getBufferAddress(overlay_buffer);
       	index--;
        strcpy((char *)mOverlay_buffer_address, "hello");
        if (strcmp((char *)mOverlay_buffer_address, "hello")) {
           	LOGI("problem with buffer\n");
        }*/
    }
    return PVMFSuccess;
}

#if 0
// post the last video frame to refresh screen after pause
void AndroidSurfaceOutputOmap34xx::postLastFrame()
{
    //mSurface->postBuffer(mOffset);
}
#endif

void AndroidSurfaceOutputOmap34xx::CloseFrameBuf()
{
	LOGV("Vendor(34xx) Speicif CloseFrameBuf");
	if (!mInitialized) return;
	mInitialized = false;
	//Free the buffers
	for (int i = 0; i < mBuffer_count; i++) {
        mOverlay_buffer_address[i] = 0;
    }
	if(mOverlay != NULL)
		mOverlay->destroy();
}

// factory function for playerdriver linkage
extern "C" AndroidSurfaceOutputOmap34xx* createVideoMio()
{
	LOGD("Creating Vendor(34xx) Specific MIO component");
    return new AndroidSurfaceOutputOmap34xx();
}


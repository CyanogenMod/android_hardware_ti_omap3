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

/*
 * Author: Srini Gosangi <srini.gosangi@windriver.com>
 * Author: Michael Barabanov <michael.barabanov@windriver.com>
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

 typedef struct WriteResponseData{
     PvmiCapabilityContext aContext;
     PVMFTimestamp aTimestamp;
     PVMFCommandId cmdid;
     bool bInDSSQueue;
 }WriteResponseData;
 
 static WriteResponseData sWriteRespData[4];
 int iDequeueIndex;

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
    LOGD("Calling Vendor(34xx) Specific initCheck");

    // reset flags in case display format changes in the middle of a stream
    resetVideoParameterFlags();
    bufEnc = 0;

    // copy parameters in case we need to adjust them
    int displayWidth = iVideoDisplayWidth;
    int displayHeight = iVideoDisplayHeight;
    int frameWidth = iVideoWidth;
    int frameHeight = iVideoHeight;
    int frameSize;
    int videoFormat = OVERLAY_FORMAT_YCbCr_422_I;
    LOGD("Use Overlays");

    if (mUseOverlay) {
    	if(mOverlay == NULL){
        	LOGV("using Vendor Speicifc(34xx) codec");
        	sp<OverlayRef> ref = mSurface->createOverlay(displayWidth, displayHeight,videoFormat);
        	if(ref != NULL)LOGV("Vendor Speicifc(34xx)MIO: overlay created ");
        	else LOGV("Vendor Speicifc(34xx)MIO: Creating overlay failed");
        	mOverlay = new Overlay(ref);
			mOverlay->setAttributes(CACHEABLE_BUFFERS, 0);
    	}

        LOGI("Actual resolution: %dx%d", frameWidth, frameHeight);
        LOGI("Video resolution: %dx%d", iVideoWidth, iVideoHeight);
        mOverlay->resizeInput(frameWidth, frameHeight);

        mbufferAlloc.maxBuffers = mOverlay->getBufferCount();
        mbufferAlloc.bufferSize = iBufferSize;
        LOGD("number of buffers = %d\n", mbufferAlloc.maxBuffers);
        for (int i = 0; i < mbufferAlloc.maxBuffers; i++) {
            mbufferAlloc.buffer_address[i] = mOverlay->getBufferAddress((void*)i);
            strcpy((char *)mbufferAlloc.buffer_address[i], "hello");
            if (strcmp((char *)mbufferAlloc.buffer_address[i], "hello")) {
                LOGI("problem with buffer\n");
                return mInitialized;
            }else{
                LOGD("buffer = %d allocated addr=%#lx\n", i, (unsigned long) mbufferAlloc.buffer_address[i]);
            }
        }        
    }
    mInitialized = true;
    LOGV("sendEvent(MEDIA_SET_VIDEO_SIZE, %d, %d)", iVideoDisplayWidth, iVideoDisplayHeight);
    mPvPlayer->sendEvent(MEDIA_SET_VIDEO_SIZE, iVideoDisplayWidth, iVideoDisplayHeight);
    
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.video.showfps", value, "0");
    mDebugFps = atoi(value);
    LOGD_IF(mDebugFps, "showfps enabled");
 	
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
        LOGD("%d Frames, %f FPS", mFrameCount, mFps);
    }
    // XXX: mFPS has the value we want
}
 
 
PVMFCommandId  AndroidSurfaceOutputOmap34xx::writeAsync(uint8 aFormatType, int32 aFormatIndex, uint8* aData, uint32 aDataLen,
                                         const PvmiMediaXferHeader& data_header_info, OsclAny* aContext)
{
     bool bDequeueFail = false;
     bool bQueueFail = false;
     //LOGD("Calling Vendor(34xx) Specific writeAsync");
     
     // Do a leave if MIO is not configured except when it is an EOS
     if (!iIsMIOConfigured
             &&
             !((PVMI_MEDIAXFER_FMT_TYPE_NOTIFICATION == aFormatType)
               && (PVMI_MEDIAXFER_FMT_INDEX_END_OF_STREAM == aFormatIndex)))
     {
         LOGE("data is pumped in before MIO is configured");
         OSCL_LEAVE(OsclErrInvalidState);
         return -1;
     }
 
     uint32 aSeqNum=data_header_info.seq_num;
     PVMFTimestamp aTimestamp=data_header_info.timestamp;
     uint32 flags=data_header_info.flags;
     PVMFCommandId cmdid=iCommandCounter++;
 
     if (aSeqNum < 6)
     {
         LOGD("AndroidSurfaceOutputOmap34xx::writeAsync() seqnum %d ts %d context %d",aSeqNum,aTimestamp, (int)aContext);
 
         LOGD("AndroidSurfaceOutputOmap34xx::writeAsync() Format Type %d Format Index %d length %d",aFormatType,aFormatIndex,aDataLen);
     }
 
     PVMFStatus status=PVMFFailure;
 
     switch(aFormatType)
     {
     case PVMI_MEDIAXFER_FMT_TYPE_COMMAND :
         LOGD("AndroidSurfaceOutputOmap34xx::writeAsync() called with Command info.");
         //ignore
         status= PVMFSuccess;
         break;
 
     case PVMI_MEDIAXFER_FMT_TYPE_NOTIFICATION :
         LOGD("AndroidSurfaceOutputOmap34xx::writeAsync() called with Notification info.");
         switch(aFormatIndex)
         {
         case PVMI_MEDIAXFER_FMT_INDEX_END_OF_STREAM:
             iEosReceived = true;
             break;
         default:
             break;
         }
         //ignore
         status= PVMFSuccess;
         break;
 
     case PVMI_MEDIAXFER_FMT_TYPE_DATA :
         switch(aFormatIndex)
         {
         case PVMI_MEDIAXFER_FMT_INDEX_FMT_SPECIFIC_INFO:
             //format-specific info contains codec headers.
             LOGD("AndroidSurfaceOutputOmap34xx::writeAsync() called with format-specific info.");
 
             if (iState<STATE_INITIALIZED)
             {
                 LOGE("AndroidSurfaceOutputOmap34xx::writeAsync: Error - Invalid state");
                 status=PVMFErrInvalidState;
             }
             else
             {
                 status= PVMFSuccess;
             }
 
             break;
 
         case PVMI_MEDIAXFER_FMT_INDEX_DATA:
             //data contains the media bitstream.
 
             //Verify the state
             if (iState!=STATE_STARTED)
             {
                 LOGE("AndroidSurfaceOutputOmap34xx::writeAsync: Error - Invalid state");
                 status=PVMFErrInvalidState;
             }
             else
             {
                 int i=0;
                 //printf("V WriteAsync { seq=%d, ts=%d }\n", data_header_info.seq_num, data_header_info.timestamp);
 
                 // Call playback to send data to IVA for Color Convert
                 if(mUseOverlay)
                 { 
                     for (i = 0; i < mbufferAlloc.maxBuffers; i++) {
                         if (mbufferAlloc.buffer_address[i] == aData) {
                             break;
                         }
                     }
                     if (i == mbufferAlloc.maxBuffers) {
                         LOGE("AndroidSurfaceOutputOmap34xx::writeAsync: aData does not match any v4l buffer address\n");
                         status = PVMFFailure;
                         WriteResponse resp(status, cmdid, aContext, aTimestamp);
                         iWriteResponseQueue.push_back(resp);
                         RunIfNotReady();
                         return cmdid;
                     }
                     LOGV("AndroidSurfaceOutputOmap34xx::writeAsync: Saving context, index=%d", i);
 
                     sWriteRespData[i].aContext = aContext;
                     sWriteRespData[i].aTimestamp  = aTimestamp;
                     sWriteRespData[i].cmdid = cmdid;
                     sWriteRespData[i].bInDSSQueue = true;
                 }
                 /* writeFrameBuf() call will return:
                  *  if success: PVMFSuccess
                  *  Queue fail: PVMFErrArgument
                  *  Dequque fail: PVMFErrInvalidState
                  *  Queue and Dequeue fails: PVMFFailure
                  */
                 status = writeFrameBuf(aData, aDataLen, data_header_info);
                 switch (status)
                 {
                     case PVMFSuccess:
                         LOGV("writeFrameBuf Success");
                     break;
                     case PVMFErrArgument:
                         sWriteRespData[i].bInDSSQueue = false;
                         bQueueFail = true;
                         LOGW("Queue FAIL from writeFrameBuf");
                     break;
                     case PVMFErrInvalidState:
                         bDequeueFail = true;
                         LOGI("Dequeue FAIL from writeFrameBuf");
                     break;
                     case PVMFFailure:
                         sWriteRespData[i].bInDSSQueue = false;
                         bDequeueFail = true;
                         bQueueFail = true;
                         LOGW("Queue & Dequeue FAIL");
                     break;
                     default: //Compiler requirement
                         LOGE("No such case!!!!!!!!!");
                     break;
                 }
                 LOGV("AndroidSurfaceOutputOmap34xx::writeAsync: Playback Progress - frame %lu",iFrameNumber++);
             }
             break;
 
         default:
             LOGE("AndroidSurfaceOutputOmap34xx::writeAsync: Error - unrecognized format index");
             status= PVMFFailure;
             break;
         }
         break;
 
     default:
         LOGE("AndroidSurfaceOutputOmap34xx::writeAsync: Error - unrecognized format type");
         status= PVMFFailure;
         break;
     }
 
     //Schedule asynchronous response
     if(iEosReceived){
         int i;
         LOGI("iEosReceived");
         for (i = 0; i < mbufferAlloc.maxBuffers; i++) {
             if (sWriteRespData[i].bInDSSQueue) {
                 LOGI("Sending dequeue response, %d", i);
                 WriteResponse resp(status,
                                 sWriteRespData[i].cmdid,
                                 sWriteRespData[i].aContext,
                                 sWriteRespData[i].aTimestamp);
                 iWriteResponseQueue.push_back(resp);
                 RunIfNotReady();
                 //Don't return cmdid here
             }
         }
     }
     else if(bQueueFail){
         //Send default response
     }
     else if(bDequeueFail){
         return cmdid;
     }
     else if(bDequeueFail == false){
         LOGV("AndroidSurfaceOutputOmap34xx::writeAsync: iDequeueIndex = %d", iDequeueIndex);
         cmdid = sWriteRespData[iDequeueIndex].cmdid;
         status = PVMFSuccess; //Clear posible error while queueing
         WriteResponse resp(status,
                             sWriteRespData[iDequeueIndex].cmdid,
                             sWriteRespData[iDequeueIndex].aContext,
                             sWriteRespData[iDequeueIndex].aTimestamp);
         iWriteResponseQueue.push_back(resp);
 
         sWriteRespData[iDequeueIndex].bInDSSQueue = false;
         RunIfNotReady();
         return cmdid;
     }
 
     WriteResponse resp(status, cmdid, aContext, aTimestamp);
     iWriteResponseQueue.push_back(resp);
     RunIfNotReady();
 
     return cmdid;
 }
 


PVMFStatus AndroidSurfaceOutputOmap34xx::writeFrameBuf(uint8* aData, uint32 aDataLen, const PvmiMediaXferHeader& data_header_info)
{
    //LOGD(" calling Vendor Speicifc(34xx) writeFrameBuf call");
    static int nBuffToStartDQ = 2; /*DSS2: 2 buffers need to be queue before enable streaming*/
    PVMFStatus eStatus = PVMFSuccess;
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
        if(mOverlay->queueBuffer((void*)bufEnc) != NO_ERROR){
            LOGE("Video (34xx)MIO queue buffer failed");
            eStatus = PVMFErrArgument; //Only Queue fail
            if(nBuffToStartDQ){ //If error try to dequeue if possible
                eStatus = PVMFFailure; //Queue fail & dequeue not posible so fails
                return eStatus;
            }
        }

        //The driver will not be able to dequeue if only have
        //one buffer queue. We'll wait for two buffers been queue before
        //calling dequeue.
        if(nBuffToStartDQ){
            nBuffToStartDQ--;
        }
 
        if(nBuffToStartDQ == 0){
            overlay_buffer_t overlay_buffer;
            if (mOverlay->dequeueBuffer(&overlay_buffer) != NO_ERROR) {
                    LOGE("Video (34xx)MIO dequeue buffer failed");
                    if(eStatus != PVMFSuccess){
                        eStatus = PVMFFailure; //Queue & Dequeue fail
                    }
                    else{
                        eStatus = PVMFErrInvalidState; //Dequeue fail
                    }
                    return eStatus;
            }
            iDequeueIndex = (int)overlay_buffer;
        }
        else{ //Not ready to return dequeue buffer
             return PVMFErrInvalidState;
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
            LOGD("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Format Key, Value %s",iVideoFormatString.get_str());
        }
        else if (pv_mime_strcmp(aParameters[i].key, PVMF_FORMAT_SPECIFIC_INFO_KEY_YUV) == 0)
        {
            uint8* data = (uint8*)aParameters->value.key_specific_value;
            PVMFYuvFormatSpecificInfo0* yuvInfo = (PVMFYuvFormatSpecificInfo0*)data;

            iVideoWidth = (int32)yuvInfo->width;
            iVideoParameterFlags |= VIDEO_WIDTH_VALID;
            LOGD("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Width, Value %d", iVideoWidth);

            iVideoHeight = (int32)yuvInfo->height;
            iVideoParameterFlags |= VIDEO_HEIGHT_VALID;
            LOGD("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Height, Value %d", iVideoHeight);

            iVideoDisplayHeight = (int32)yuvInfo->display_height;
            iVideoParameterFlags |= DISPLAY_HEIGHT_VALID;
            LOGD("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Display Height, Value %d", iVideoDisplayHeight);


            iVideoDisplayWidth = (int32)yuvInfo->display_width;
            iVideoParameterFlags |= DISPLAY_WIDTH_VALID;
            LOGD("AndroidSurfaceOutputOmap34xx::setParametersSync() Video Display Width, Value %d", iVideoDisplayWidth);

            iNumberOfBuffers = (int32)yuvInfo->num_buffers;
            LOGD("AndroidSurfaceOutputOmap34xx::setParametersSync() Number of Buffer, Value %d", iNumberOfBuffers);

            iBufferSize = (int32)yuvInfo->buffer_size;
            LOGD("AndroidSurfaceOutputOmap34xx::setParametersSync() Buffer Size, Value %d", iBufferSize);
            /* we're not being passed the subformat info */
            iVideoSubFormat = "X-YUV-420";
            iVideoParameterFlags |= VIDEO_SUBFORMAT_VALID;
        }
        else
        {
            //if we get here the key is unrecognized.
            LOGD("AndroidSurfaceOutputOmap34xx::setParametersSync() Error, unrecognized key %s ", aParameters[i].key);

            //set the return value to indicate the unrecognized key
            //and return.
            aRet_kvp = &aParameters[i];
            return;
        }
    }
    /* Copy Code from base class. Ideally we'd just call base class's setParametersSync, but we can't as it will not get to initCheck if it encounters an unrecognized parameter such as the one we're handling here */
    uint32 mycache = iVideoParameterFlags ;
    if( checkVideoParameterFlags() ) {
   // CloseFrameBuf();	
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
    LOGD("AndroidSurfaceOutputOmap34xx::postLastFrame()");
    //mSurface->postBuffer(mOffset);
}

void AndroidSurfaceOutputOmap34xx::closeFrameBuf()
{
    LOGD("Vendor(34xx) Speicif CloseFrameBuf");
    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
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
    LOGD("Creating Vendor(34xx) Specific MIO component");
    return new AndroidSurfaceOutputOmap34xx();
}


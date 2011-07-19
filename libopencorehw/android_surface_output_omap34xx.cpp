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
#include "v4l2_utils.h"

using namespace android;

#define ARMPAGESIZE 4096
typedef struct WriteResponseData{
     PvmiCapabilityContext aContext;
     PVMFTimestamp aTimestamp;
     PVMFCommandId cmdid;
     bool bInDSSQueue;
 }WriteResponseData;

 static WriteResponseData sWriteRespData[NUM_OVERLAY_BUFFERS_REQUESTED];
 int iDequeueIndex;


static void convertYuv420pToYuv422i(int width, int height, void* src, void* dst);

static int Calculate_TotalRefFrames(int nWidth, int nHeight)
{
    LOGD("Calculate_TotalRefFrames");
    int ref_frames = 0;
    int spec_computation;
    if(nWidth > MAX_OVERLAY_WIDTH_VAL || nHeight > MAX_OVERLAY_HEIGHT_VAL)
    {
       return 0;
    }
    nWidth = nWidth - 128 - 2 * 36;

    if (nWidth > 1280) {
        nWidth = 1920;
    } else if (nWidth > 720) {
        nWidth = 1280;
    } else {
        nWidth = (nWidth + 16) & ~12;
    }

    nHeight = nHeight - 4 * 24;

    /* 12288 is the value for Profile 4.1 */
    spec_computation = ((1024 * 12288)/((nWidth/16)*(nHeight/16)*384));
    ref_frames = (spec_computation > 16)?16:(spec_computation/1);
    ref_frames = ref_frames + 1 + NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE;
    LOGD("**Calculated buf %d",ref_frames);
    return ref_frames;
}

AndroidSurfaceOutputOmap34xx::AndroidSurfaceOutputOmap34xx() :
    AndroidSurfaceOutput()
{
    mUseOverlay = true;
    mOverlay = NULL;
    mConvert = false;
    mBuffersQueuedToDSS = 0;
    /**
    * Initialize cropx/cropy to negative values to allow setcrop to get called
    * at least once, even for the cases where offset is zero.
    */
    icropX = -1;
    icropY = -1;
    /**In the base class, this variable is initilized to 2, which is required for
    * surface flinger based rendering. But on overlay-based platforms, its value should be overwritten.
    * In order to get the same behavior as the previous releases, lets reset it to 0.
    */
    mNumberOfFramesToHold = 0;
}

AndroidSurfaceOutputOmap34xx::~AndroidSurfaceOutputOmap34xx()
{
    mUseOverlay = false;
    mInitialized = false;
    if(mOverlay.get() != NULL){
        mOverlay->destroy();
        mOverlay.clear();
    }
}

bool AndroidSurfaceOutputOmap34xx::initCheck()
{
    LOGD("Calling Vendor(34xx) Specific initCheck");
     mInitialized = false;
    // reset flags in case display format changes in the middle of a stream
    resetVideoParameterFlags();
    bufEnc = 0;
    mOptimalQBufCnt = NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE;
    mBuffersQueuedToDSS = 0;

    // copy parameters in case we need to adjust them
    int displayWidth = iVideoDisplayWidth;
    int displayHeight = iVideoDisplayHeight;
    int frameWidth = iVideoWidth;
    int frameHeight = iVideoHeight;
    int frameSize;
    int videoFormat = OVERLAY_FORMAT_CbYCrY_422_I;
    //as the sub format is a string, switch case logic cant be used here
    if ((iVideoSubFormat == PVMF_MIME_YUV420_SEMIPLANAR)||(iVideoSubFormat == PVMF_MIME_YUV420_PACKEDSEMIPLANAR))
   {
    videoFormat = OVERLAY_FORMAT_YCbCr_420_SP;
    iBytesperPixel = 1;
    mOptimalQBufCnt = 4;
   }
   else if (iVideoSubFormat == PVMF_MIME_YUV420_PACKEDSEMIPLANAR_SEQ_TOPBOTTOM)
   {
    videoFormat = OVERLAY_FORMAT_YCbCr_420_SP_SEQ_TB;
    iBytesperPixel = 1;
    mOptimalQBufCnt = 4;
   }
    else if(iVideoSubFormat == PVMF_MIME_YUV422_INTERLEAVED_UYVY)
   {
   videoFormat =  OVERLAY_FORMAT_CbYCrY_422_I;
   iBytesperPixel = 2;
   }
    else if (iVideoSubFormat == PVMF_MIME_YUV422_INTERLEAVED_YUYV)
   {
   videoFormat = OVERLAY_FORMAT_YCbYCr_422_I;
   iBytesperPixel = 2;
   }
    else if (iVideoSubFormat == PVMF_MIME_YUV420_PLANAR)
   {
   videoFormat = OVERLAY_FORMAT_YCbYCr_422_I;
   LOGI("Use YUV420_PLANAR -> YUV422_INTERLEAVED_UYVY converter or RGB565 converter needed");
   mConvert = true;
   mOptimalQBufCnt = NUM_BUFFERS_TO_BE_QUEUED_FOR_ARM_CODECS;
   iBytesperPixel = 1;
    }
    else
   {
   LOGI("Not Supported format, and no coverter available");
   return mInitialized;
    }

    mapping_data_t *data;
    if (mUseOverlay) {
        if(mOverlay.get() == NULL){
            LOGV("using Vendor Speicifc(34xx) codec");
            sp<OverlayRef> ref = mSurface->createOverlay(frameWidth, frameHeight,videoFormat,0);
            if(ref != NULL)LOGV("Vendor Speicifc(34xx)MIO: overlay created ");
            else LOGV("Vendor Speicifc(34xx)MIO: Creating overlay failed");
            if ( ref.get() == NULL )
            {
                LOGE("Overlay Creation Failed!");
                return mInitialized;
            }
            mOverlay = new Overlay(ref);

            mOverlay->setParameter(CACHEABLE_BUFFERS, 0);
            mOverlay->setParameter(OPTIMAL_QBUF_CNT, mOptimalQBufCnt);
#ifdef TARGET_OMAP4
            //calculate the number of buffers to be allocated for overlay
            int overlaybuffcnt = Calculate_TotalRefFrames(frameWidth, frameHeight);
            overlaybuffcnt = (overlaybuffcnt > iNumberOfBuffers) ? overlaybuffcnt : iNumberOfBuffers;
            if (overlaybuffcnt < 20) {
                overlaybuffcnt = 20;
            }
            int initialcnt = mOverlay->getBufferCount();
            if (overlaybuffcnt != initialcnt) {
                mOverlay->setParameter(OVERLAY_NUM_BUFFERS, overlaybuffcnt);
                mOverlay->resizeInput(frameWidth, frameHeight);
            }
#endif
        }
        else
        {
            LOGD("Before resizeInput()");
            LOGD("sWriteRespData[0].inQ = %d", sWriteRespData[0].bInDSSQueue);
            LOGD("sWriteRespData[1].inQ = %d", sWriteRespData[1].bInDSSQueue);
            LOGD("sWriteRespData[2].inQ = %d", sWriteRespData[2].bInDSSQueue);
            LOGD("sWriteRespData[3].inQ = %d", sWriteRespData[3].bInDSSQueue);

            mOverlay->resizeInput(frameWidth, frameHeight);
        }
        LOGI("Actual resolution: %dx%d", frameWidth, frameHeight);
        LOGI("Video resolution: %dx%d", iVideoWidth, iVideoHeight);

        mbufferAlloc.maxBuffers = mOverlay->getBufferCount();
        mbufferAlloc.bufferSize = iBufferSize;
        LOGD("number of buffers = %d\n", mbufferAlloc.maxBuffers);
        if (mbufferAlloc.maxBuffers < 0)
        {
            LOGE("problem with bufferallocations\n");
             return mInitialized;
   }
        for (int i = 0; i < mbufferAlloc.maxBuffers; i++) {
            data = (mapping_data_t *)mOverlay->getBufferAddress((void*)i);
            if (data == NULL)
            {
                LOGE("Vendor Speicifc(34xx)MIO: Failed to get the overlay Buffer address. ");
                mPvPlayer->sendEvent(MEDIA_ERROR,MEDIA_ERROR_UNKNOWN,PVMFErrNoResources);
                return mInitialized;
            }
            mbufferAlloc.buffer_address[i] = data->ptr;
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
    int cropX = 0;
    int cropY = 0;
     //LOGD("Calling Vendor(34xx) Specific writeAsync[%d]", aData);

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
         LOGV("AndroidSurfaceOutputOmap34xx::writeAsync() seqnum %d ts %d context %d",aSeqNum,aTimestamp, (int)aContext);

         LOGV("AndroidSurfaceOutputOmap34xx::writeAsync() Format Type %d Format Index %d length %d",aFormatType,aFormatIndex,aDataLen);
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
                 //printf("V WriteAsync { seq=%d, ts=%d }\n", data_header_info.seq_num, data_header_info.timestamp);

                 // Call playback to send data to IVA for Color Convert
                 if(mUseOverlay)
                 {
            // Convert from YUV420 to YUV422 for software codec
            if (mConvert) {
                convertYuv420pToYuv422i(iVideoWidth, iVideoHeight, aData, mbufferAlloc.buffer_address[bufEnc]);
            } else {
                int i;
                for (i = 0; i < mbufferAlloc.maxBuffers; i++) {
                    /**
                    *In order to support the offset from the decoded buffers, we have to check for
                    * the range of offset with in the buffer. Here we can't check for the base address
                    * and also, the offset should be used for crop window position calculation
                    **/
                    uint32 offsetinPixels = data_header_info.nOffset;
                    if(mbufferAlloc.buffer_address[i] == aData - offsetinPixels){
                        cropY = (offsetinPixels)/ARMPAGESIZE;
                        cropX = (offsetinPixels)%ARMPAGESIZE;
                        if( (cropY != icropY) || (cropX != icropX))
                        {
                            icropY = cropY;
                            icropX = cropX;
                            mOverlay->setCrop((uint32_t)cropX, (uint32_t)cropY, iVideoDisplayWidth, iVideoDisplayHeight);
                        }
                        break;
                    }
                }
            bufEnc = i;
            }
            if (bufEnc == mbufferAlloc.maxBuffers) {
                LOGE("AndroidSurfaceOutputOmap34xx::writeAsync: aData does not match any v4l buffer address\n");
                status = PVMFFailure;
                WriteResponse resp(status, cmdid, aContext, aTimestamp);
                iWriteResponseQueue.push_back(resp);
                RunIfNotReady();
                return cmdid;
            }
            LOGV("AndroidSurfaceOutputOmap34xx::writeAsync: Saving context, index=%d", bufEnc);

            sWriteRespData[bufEnc].aContext = aContext;
            sWriteRespData[bufEnc].aTimestamp  = aTimestamp;
            sWriteRespData[bufEnc].cmdid = cmdid;
            sWriteRespData[bufEnc].bInDSSQueue = false;
            }
                bDequeueFail = false;
                bQueueFail = false;
                 status = writeFrameBuf((uint8*)mbufferAlloc.buffer_address[bufEnc], aDataLen, data_header_info, bufEnc);
                 switch (status)
                 {
                     case PVMFSuccess:
                         LOGV("writeFrameBuf Success");
                     break;
                     case PVMFErrArgument:
                         bQueueFail = true;
                         LOGW("Queue FAIL from writeFrameBuf");
                     break;
                     case PVMFErrInvalidState:
                         bDequeueFail = true;
                         LOGV("Dequeue FAIL from writeFrameBuf");
                     break;
                     case PVMFFailure:
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
         for (i = 0; i < mbufferAlloc.maxBuffers; i++) {
             if (sWriteRespData[i].bInDSSQueue) {
                mBuffersQueuedToDSS--;
                sWriteRespData[i].bInDSSQueue = false;

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
         status = PVMFFailure; //Set proper error for the caller.
     }
     else if(bDequeueFail == false){
         status = PVMFSuccess; //Clear posible error while queueing
         WriteResponse resp(status,
                             sWriteRespData[iDequeueIndex].cmdid,
                             sWriteRespData[iDequeueIndex].aContext,
                             sWriteRespData[iDequeueIndex].aTimestamp);
         iWriteResponseQueue.push_back(resp);
         RunIfNotReady();
         return cmdid;
     }

     WriteResponse resp(status, cmdid, aContext, aTimestamp);
     iWriteResponseQueue.push_back(resp);
     RunIfNotReady();

     return cmdid;
 }



PVMFStatus AndroidSurfaceOutputOmap34xx::writeFrameBuf(uint8* aData, uint32 aDataLen, const PvmiMediaXferHeader& data_header_info, int aIndex)
{
    PVMFStatus eStatus = PVMFSuccess;
    int nReturnDSSBufIndex = 0;
    int nError = 0;
    int nActualBuffersInDSS = 0;

    if (mSurface == 0) return PVMFFailure;

    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
    }

    if (mUseOverlay == 0) return PVMFSuccess;
    
    bufEnc = aIndex;
    nActualBuffersInDSS = mOverlay->queueBuffer((void*)bufEnc);
    if(nActualBuffersInDSS < 0){
        eStatus = PVMFErrArgument; // Queue failed
    }
    else // Queue succeeded
    {
        mBuffersQueuedToDSS++;
        sWriteRespData[bufEnc].bInDSSQueue = true;

        // This code will make sure that whenever a Stream OFF occurs in overlay...
        // a response is sent for each of the buffer.. so that buffers are not lost 
        if (mBuffersQueuedToDSS != nActualBuffersInDSS)
        {
            for(nReturnDSSBufIndex = 0; nReturnDSSBufIndex < mbufferAlloc.maxBuffers; nReturnDSSBufIndex++){
                if(nReturnDSSBufIndex == bufEnc){
                    LOGD("Skip this buffer as it is the current buffer %d", bufEnc);
                    continue;
                }
                if(sWriteRespData[nReturnDSSBufIndex].bInDSSQueue) {
                    LOGD("Sending dequeue response, %d", nReturnDSSBufIndex);
                    mBuffersQueuedToDSS--;
                    sWriteRespData[nReturnDSSBufIndex].bInDSSQueue = false;
                    WriteResponse resp(PVMFFailure,
                        sWriteRespData[nReturnDSSBufIndex].cmdid,
                        sWriteRespData[nReturnDSSBufIndex].aContext,
                        sWriteRespData[nReturnDSSBufIndex].aTimestamp);
                    iWriteResponseQueue.push_back(resp);
                    RunIfNotReady();
                }
                else{
                    LOGD("Skip this buffer %d, not in DSS", nReturnDSSBufIndex);
                }
            }
        }
    }

    // advance the overlay index if using color conversion
    if (mConvert) {
        if (++bufEnc == mbufferAlloc.maxBuffers) {
            bufEnc = 0;
        }
    }

    overlay_buffer_t overlay_buffer;
    nError = mOverlay->dequeueBuffer(&overlay_buffer);
    if(nError == NO_ERROR){
        mBuffersQueuedToDSS--;
        iDequeueIndex = (int)overlay_buffer;
        sWriteRespData[iDequeueIndex].bInDSSQueue = false;
        if (eStatus == PVMFSuccess)
            return PVMFSuccess; // both Queue and Dequeue succeeded
        else return PVMFErrArgument; // Only Queue failed
    }

    if (eStatus == PVMFSuccess)
        return PVMFErrInvalidState; // Only Dequeue failed
    else return PVMFFailure; // both Queue and Dequeue failed

}


/* based on test code in pvmi/media_io/pvmiofileoutput/src/pvmi_media_io_fileoutput.cpp */
void AndroidSurfaceOutputOmap34xx::setParametersSync(PvmiMIOSession aSession,
                                        PvmiKvp* aParameters,
                                        int num_elements,
                                        PvmiKvp * & aRet_kvp)
{
    OSCL_UNUSED_ARG(aSession);
    aRet_kvp = NULL;

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
            iVideoSubFormat = yuvInfo->video_format.getMIMEStrPtr();
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
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aContext);
    aParameters=NULL;

    if (strcmp(aIdentifier, PVMF_BUFFER_ALLOCATOR_KEY) == 0)
    {
        if((iVideoSubFormat != PVMF_MIME_YUV422_INTERLEAVED_UYVY) && (iVideoSubFormat != PVMF_MIME_YUV422_INTERLEAVED_YUYV) \
          && (iVideoSubFormat != PVMF_MIME_YUV420_SEMIPLANAR)&&(iVideoSubFormat != PVMF_MIME_YUV420_PACKEDSEMIPLANAR) \
          && (iVideoSubFormat != PVMF_MIME_YUV420_PACKEDSEMIPLANAR_SEQ_TOPBOTTOM))
        {
            LOGI("Ln %d iVideoSubFormat %s. do NOT allocate decoder buffer from overlay", __LINE__, iVideoSubFormat.getMIMEStrPtr() );
            OSCL_LEAVE(OsclErrNotSupported);
            return PVMFErrNotSupported;
        }

        int32 err;
        aParameters = (PvmiKvp*)oscl_malloc(sizeof(PvmiKvp));
        if (!aParameters)
        {
            return PVMFErrNoMemory;
        }
        aParameters[0].value.key_specific_value = (PVInterface*)&mbufferAlloc;
        return PVMFSuccess;
    }

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
    LOGV("Vendor(34xx) Specific CloseFrameBuf");
    /* This should be the first line. */
    if (!mInitialized) return;

    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
    }
 }


static void convertYuv420pToYuv422i (int width, int height, void* src, void* dst)
{
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
}


// factory function for playerdriver linkage
extern "C" AndroidSurfaceOutputOmap34xx* createVideoMio()
{
    LOGD("Creating Vendor(34xx) Specific MIO component");
    return new AndroidSurfaceOutputOmap34xx();
}





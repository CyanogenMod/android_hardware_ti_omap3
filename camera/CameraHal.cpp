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
/**
* @file CameraHal.cpp
*
* This file maps the Camera Hardware Interface to V4L2.
*
*/
#include "CameraHal.h"


namespace android {
/*****************************************************************************/

/*
 * This is the overlay_t object, it is returned to the user and represents
 * an overlay. here we use a subclass, where we can store our own state.
 * This handles will be passed across processes and possibly given to other
 * HAL modules (for instance video decode modules).
 */
struct overlay_true_handle_t : public native_handle {
    /* add the data fields we need here, for instance: */
    int ctl_fd;
    int shared_fd;
    int width;
    int height;
    int format;
    int num_buffers;
    int shared_size;
};

#define LOG_TAG "CameraHal"

int CameraHal::camera_device = 0;
wp<CameraHardwareInterface> CameraHal::singleton;

CameraHal::CameraHal()
            :mParameters(),
            mRawPictureCallback(0),
            mJpegPictureCallback(0),
            mPictureCallbackCookie(0),
	    mOverlay(NULL),
	    mPreviewRunning(0),
            mRecordingFrameCount(0),
            mRecordingFrameSize(0),
            mRecordingCallback(0),
            mRecordingCallbackCookie(0),
            mVideoBufferCount(0),
            mVideoHeap(0),
            mAutoFocusCallback(0),
            mAutoFocusCallbackCookie(0),
	    nOverlayBuffersQueued(0),
	    nCameraBuffersQueued(0),            
#ifdef FW3A
            fobj(NULL),
#endif
            file_index(0),
  	    mflash(2),
  	    mcapture_mode(1),
            mcaf(0),
            j(0),
	    
            myuv(3),            
            mMMSApp(0)		    					
{

	gettimeofday(&ppm_start, NULL);

    isStart_FW3A = false;
    isStart_FW3A_AF = false;
    isStart_FW3A_CAF = false;
    isStart_FW3A_AEWB = false;

    int i = 0;
    for(i = 0; i < VIDEO_FRAME_COUNT_MAX; i++)
    {
        mVideoBuffer[i] = 0;
        mVideoBufferUsing[i] = 0;
    }

    CameraCreate();

    initDefaultParameters();

    CameraConfigure();

#ifdef FW3A
    FW3A_Create();
#endif

    ICaptureCreate();

    mPreviewThread = new PreviewThread(this);
    mPreviewThread->run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);

	if( sem_init(&mIppVppSem,0,0)!=0 ){
		LOGE("Error creating semaphore\n");
	}

	if(pipe(vppPipe) != 0 ){
		LOGD("NO_ERROR= %d\n",NO_ERROR);
		LOGE("Failed creating pipe");
	}
	
	mVPPThread = new VPPThread(this);
    mVPPThread->run("CameraVPPThread", PRIORITY_URGENT_DISPLAY);
	LOGD("STARTING VPP THREAD \n");
    
#ifdef FW3A
    if (fobj!=NULL) 
    {
        FW3A_DefaultSettings();
    }
#endif

}

void CameraHal::initDefaultParameters()
{
    CameraParameters p;
 
    LOG_FUNCTION_NAME
	
    p.setPreviewSize(MIN_WIDTH, MIN_HEIGHT);
    p.setPreviewFrameRate(30);
    p.setPreviewFormat("yuv422i");

    p.setPictureSize(PICTURE_WIDTH, PICTURE_HEIGHT);
    p.setPictureFormat("jpeg");
    p.set("jpeg-quality", 100);
    
    if (setParameters(p) != NO_ERROR) {
        LOGE("Failed to set default parameters?!");
    }

    LOG_FUNCTION_NAME_EXIT

}


CameraHal::~CameraHal()
{
    int err = 0;
	int vppMessage =0;
	sp<VPPThread> vppThread;
	  
    LOG_FUNCTION_NAME
	

    if(mPreviewThread != NULL) {
        Message msg;
        msg.command = PREVIEW_KILL;
        previewThreadCommandQ.put(&msg);
        previewThreadAckQ.get(&msg);
    }       
  
    sp<PreviewThread> previewThread;
    
    { // scope for the lock
        Mutex::Autolock lock(mLock);
        previewThread = mPreviewThread;
    }

    // don't hold the lock while waiting for the thread to quit
    if (previewThread != 0) {
        previewThread->requestExitAndWait();
    }

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        mPreviewThread.clear();
    }

	sem_destroy(&mIppVppSem);

	vppMessage = VPP_THREAD_EXIT;
	write(vppPipe[1], &vppMessage,sizeof(int));

	{ // scope for the lock
        Mutex::Autolock lock(mLock);
        vppThread = mVPPThread;
    }

    // don't hold the lock while waiting for the thread to quit
    if (vppThread != 0) {
        vppThread->requestExitAndWait();
    }

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        mVPPThread.clear();
    }


#ifdef ICAP
    ICaptureDestroy();
#endif

#ifdef FW3A
    FW3A_Destroy();
#endif
    CameraDestroy();

    if ( mOverlay.get() != NULL )
    {
        LOGD("Destroying current overlay");
        mOverlay->destroy();
    }
    
    LOGD("<<< Release");

    singleton.clear();
}

void CameraHal::facetrackingThread()
{
}

void CameraHal::previewThread()
{
    Message msg;
    bool  shouldLive = true;
    bool has_message;
    int err;
    
    LOG_FUNCTION_NAME
    
    while(shouldLive) {
    
        has_message = false;

        if( mPreviewRunning )
        {	
            //process 1 preview frame
            nextPreview();

#ifdef FW3A
            if (isStart_FW3A_AF) {
                err = fobj->cam_iface_2a->ReadSatus(fobj->cam_iface_2a->pPrivateHandle, &fobj->status_2a);
                if ((err == 0) && (AF_STATUS_RUNNING != fobj->status_2a.af.status)) {
                    LOGD(" AF Completed");
                    FW3A_Stop_AF();
                    mAutoFocusCallback( true, mAutoFocusCallbackCookie );
                }
            }
#endif

            if( !previewThreadCommandQ.isEmpty() ) {
                previewThreadCommandQ.get(&msg);
                has_message = true;
            }
        }
        else
        {
            //block for message
            previewThreadCommandQ.get(&msg);
            has_message = true;
        }

        if( !has_message )
            continue;

        switch(msg.command)
        {
            case PREVIEW_START:
            {
                LOGD("Receive Command: PREVIEW_START");
                LOGD("mPreviewRunning =%d",mPreviewRunning);
                err = 0;
                if( ! mPreviewRunning ) {

                CameraConfigure();
#ifdef FW3A
                FW3A_Start();
#endif
                CameraStart();
				PPM("PREVIEW STARTED");
                } 
                else
                {
                    err = -1;
                }

                LOGD("PREVIEW_START %s", err ? "NACK" : "ACK"); 
                msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;

                if( !err ){
                    LOGD("Preview Started!");
                    mPreviewRunning = true;
                }

                previewThreadAckQ.put(&msg);
            }
            break;

            case PREVIEW_STOP:
            {
                LOGD("Receive Command: PREVIEW_STOP");
                if( mPreviewRunning ) {
#ifdef FW3A
                    FW3A_Stop_AF();
                    FW3A_Stop_CAF();
                    FW3A_Stop();
                    FW3A_GetSettings();
#endif
                    err = CameraStop();
                    if (err) {
                        LOGE("Cannot deinit preview.");
                    }
                    LOGD("PREVIEW_STOP %s", err ? "NACK" : "ACK");
                    msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;
                } 
                else
                {
                    msg.command = PREVIEW_NACK;
                }

                mPreviewRunning = false;

                previewThreadAckQ.put(&msg);
            }
            break;
	
            case PREVIEW_AF_START:
            {
                LOGD("Receive Command: PREVIEW_AF_START");

                if( !mPreviewRunning ){
                    LOGD("WARNING PREVIEW RUNNING!");
                    msg.command = PREVIEW_NACK;
                }
                else
                {
                    mAutoFocusCallback       = (autofocus_callback) msg.arg1;
                    mAutoFocusCallbackCookie = msg.arg2;
#ifdef FW3A   
                    FW3A_Stop_CAF();
                    FW3A_Start_AF();
#endif  
                    msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;
                    mAutoFocusCallback( true, mAutoFocusCallbackCookie ); //JJ
                }
                LOGD("Receive Command: PREVIEW_AF_START %s", msg.command == PREVIEW_NACK ? "NACK" : "ACK"); 
                previewThreadAckQ.put(&msg);
            }
            break;
		
	    case PREVIEW_CAF_START:
                LOGD("Receive Command: PREVIEW_CAF_START");

                if( !mPreviewRunning )
                    msg.command = PREVIEW_NACK;
                else
                {
#ifdef FW3A    
                    err = FW3A_Start_CAF();
#endif
                    msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;
                }
                LOGD("Receive Command: PREVIEW_CAF_START %s", msg.command == PREVIEW_NACK ? "NACK" : "ACK"); 
                previewThreadAckQ.put(&msg);

                break;
                
            case PREVIEW_CAF_STOP:
                LOGD("Receive Command: PREVIEW_CAF_STOP");

                if( !mPreviewRunning )
                    msg.command = PREVIEW_NACK;
                else
                {
#ifdef FW3A    
                    err = FW3A_Stop_CAF();
#endif
                    msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;
                }
                LOGD("Receive Command: PREVIEW_CAF_STOP %s", msg.command == PREVIEW_NACK ? "NACK" : "ACK"); 
                previewThreadAckQ.put(&msg);
                break;
	
            case PREVIEW_CAPTURE:
            {
                int flg_AF;
                int flg_CAF;

                LOGD("ENTER OPTION PREVIEW_CAPTURE");
                
				PPM("RECEIVED COMMAND TO TAKE A PICTURE");
                
                mShutterCallback    = (shutter_callback)msg.arg1;
                mRawPictureCallback = (raw_callback)msg.arg2;
                mJpegPictureCallback= (jpeg_callback)msg.arg3;
                mPictureCallbackCookie = msg.arg4;

                msg.command = mPreviewRunning ? PREVIEW_ACK : PREVIEW_NACK;
                previewThreadAckQ.put(&msg);

#ifdef FW3A                
                flg_AF  = FW3A_Stop_AF();
                flg_CAF = FW3A_Stop_CAF();
                FW3A_Stop();
#endif
                CameraStop();
                mPreviewRunning =false;
#ifdef FW3A
                FW3A_GetSettings();
#endif
                
                PPM("STOPPED PREVIEW");
#ifdef ICAP
                err = ICapturePerform();
#else
                err = CapturePicture();
#endif
                if( err ) {
                    LOGE("Capture failed.");
                } 

                if(mMMSApp){ //restart Preview
                    
                    PPM("TRYING TO RESTART PREVIEW");
                    CameraConfigure();
#ifdef FW3A 
                    FW3A_Start();
                    FW3A_SetSettings();
#endif
                    CameraStart();
                    mPreviewRunning =true;
                    
                    PPM("PREVIEW STARTED AFTER CAPTURING");
#ifdef FW3A 
                    if (flg_AF){
                        FW3A_Start_AF();
                        PPM("STARTED AUTO FOCUS");
                    }

                    if (flg_CAF){
                        FW3A_Start_CAF();
                        PPM("STARTED CONTINOUS AUTO FOCUS");
                    }
#endif   		
                }
                LOGD("EXIT OPTION PREVIEW_CAPTURE");
            }
            break;

            case PREVIEW_CAPTURE_CANCEL:
            {
                LOGD("Receive Command: PREVIEW_CAPTURE_CANCEL");
                msg.command = PREVIEW_NACK;
                previewThreadAckQ.put(&msg);
            }
            break;                

            case PREVIEW_KILL:
            {
                LOGD("Receive Command: PREVIEW_KILL");

                if (mPreviewRunning) {
#ifdef FW3A 
                    FW3A_Stop_AF();
                    FW3A_Stop_CAF();
                    FW3A_Stop();
#endif 
                    CameraStop();
                    mPreviewRunning = false;
                }

                msg.command = PREVIEW_ACK;
                previewThreadAckQ.put(&msg);
                shouldLive = false;
            }
            break;
        }
    }

   LOG_FUNCTION_NAME_EXIT
}

int CameraHal::CameraCreate()
{
    int err;

    LOG_FUNCTION_NAME

    camera_device = open(VIDEO_DEVICE, O_RDWR);
    if (camera_device < 0) {
        LOGE ("Could not open the camera device: %s",  strerror(errno) );
        goto exit;
    }

    LOG_FUNCTION_NAME_EXIT
    return 0;

exit:
    return err;
}


int CameraHal::CameraDestroy()
{
    int err;

    close(camera_device);
    camera_device = -1;

    if (mOverlay != NULL) {
        mOverlay->destroy();
        mOverlay = NULL;
    }

    LOGD("Camera Destroy - Success");
    return 0;
}

int CameraHal::CameraConfigure()
{
    int w, h, framerate;
    int image_width, image_height;
    int err;
    struct v4l2_format format;
    enum v4l2_buf_type type;
    struct v4l2_control vc;
    struct v4l2_streamparm parm;

    LOG_FUNCTION_NAME

    mParameters.getPreviewSize(&w, &h);
   
    /* Set preview format */
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = w;
    format.fmt.pix.height = h;
    format.fmt.pix.pixelformat = PIXEL_FORMAT;

    err = ioctl(camera_device, VIDIOC_S_FMT, &format);
    if ( err < 0 ){
        LOGE ("Failed to set VIDIOC_S_FMT.");
        goto s_fmt_fail;
    }

    LOGI("CameraConfigure PreviewFormat: w=%d h=%d", format.fmt.pix.width, format.fmt.pix.height);	

    framerate = mParameters.getPreviewFrameRate();
    
    LOGD("CameraConfigure: framerate=%d",framerate);
  
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    err = ioctl(camera_device, VIDIOC_G_PARM, &parm);
    if(err != 0) {
	LOGD("VIDIOC_G_PARM ");
	return -1;
    }
    
    LOGD("Old frame rate is %d/%d  fps\n",
		parm.parm.capture.timeperframe.denominator,
		parm.parm.capture.timeperframe.numerator);
   

   
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = framerate;
    err = ioctl(camera_device, VIDIOC_S_PARM, &parm);
    if(err != 0) {
	LOGE("VIDIOC_S_PARM ");
	return -1;
    }
    
    LOGI("CameraConfigure Preview fps:%d/%d",parm.parm.capture.timeperframe.denominator,parm.parm.capture.timeperframe.numerator);

    zoom_width = sensor_width = 820;
    zoom_height = sensor_height = 616;

    LOG_FUNCTION_NAME_EXIT
    return 0;

s_fmt_fail:
    return -1;
}

int CameraHal::CameraStart()
{
    int w, h;
    int err;
    int nSizeBytes;
    int buffer_count;
    struct v4l2_format format;
    enum v4l2_buf_type type;
    struct v4l2_requestbuffers creqbuf;
    LOG_FUNCTION_NAME
    
    mParameters.getPreviewSize(&w, &h);
    LOGD("**CaptureQBuffers: preview size=%dx%d", w, h);

    if(mOverlay != NULL){
	LOGD("OK,mOverlay not NULL");
    }
    else{
	LOGD("WARNING, mOverlay is NULL!!");
    }
  
    mPreviewFrameSize = w * h * 2;
    if (mPreviewFrameSize & 0xfff)
    {
        mPreviewFrameSize = (mPreviewFrameSize & 0xfffff000) + 0x1000;
    }
    LOGD("mPreviewFrameSize = 0x%x = %d", mPreviewFrameSize, mPreviewFrameSize);

    buffer_count = mOverlay->getBufferCount(); 
    LOGD("number of buffers = %d\n", buffer_count);

    creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    creqbuf.memory = V4L2_MEMORY_USERPTR;
    creqbuf.count  =  buffer_count /*- 1*/ ; //We will use the last buffer for snapshots.
    if (ioctl(camera_device, VIDIOC_REQBUFS, &creqbuf) < 0) {
        LOGE ("VIDIOC_REQBUFS Failed. %s", strerror(errno));
        goto fail_reqbufs;
    }

    for (int i = 0; i < (int)creqbuf.count; i++) {

        struct v4l2_buffer buffer;
        buffer.type = creqbuf.type;
        buffer.memory = creqbuf.memory;
        buffer.index = i;

        if (ioctl(camera_device, VIDIOC_QUERYBUF, &buffer) < 0) {
            LOGE("VIDIOC_QUERYBUF Failed");
            goto fail_loop;
        }

        buffer.length= mPreviewFrameSize;
        LOGD("buffer.length = %d", buffer.length);
        buffer.m.userptr = (unsigned long) mOverlay->getBufferAddress((void*)i);
        strcpy((char *)buffer.m.userptr, "hello");
        if (strcmp((char *)buffer.m.userptr, "hello")) {
            LOGI("problem with buffer\n");
            goto fail_loop;
        }

        LOGD("User Buffer [%d].start = %p  length = %d\n", i,
             (void*)buffer.m.userptr, buffer.length);
        nCameraBuffersQueued++;
        if (ioctl(camera_device, VIDIOC_QBUF, &buffer) < 0) {
            LOGE("CameraStart VIDIOC_QBUF Failed");
            goto fail_loop;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    err = ioctl(camera_device, VIDIOC_STREAMON, &type);
    if ( err < 0) {
        LOGE("VIDIOC_STREAMON Failed");
        goto fail_loop;
    }

    LOG_FUNCTION_NAME_EXIT
    return 0;

fail_bufalloc:
fail_loop:
fail_reqbufs:

    return -1;
}

int CameraHal::CameraStop()
{
    LOG_FUNCTION_NAME

    int ret;
    struct v4l2_requestbuffers creqbuf;
    struct v4l2_buffer cfilledbuffer;
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;

    while(nCameraBuffersQueued){
	LOGD("DQUEUING UNDQUEUED BUFFERS enter = %d",nCameraBuffersQueued);
        nCameraBuffersQueued--;
        if (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
        LOGE("VIDIOC_DQBUF Failed!!!");
        }
	LOGD("DQUEUING UNDQUEUED BUFFERS exit = %d",nCameraBuffersQueued);
    }

    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) == -1) {
        LOGE("VIDIOC_STREAMOFF Failed");
        goto fail_streamoff;
    }

    LOG_FUNCTION_NAME_EXIT
    return 0;

fail_streamoff:

    return -1;
}



void CameraHal::nextPreview()
{
    struct v4l2_buffer cfilledbuffer;
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;
    int w, h, ret;
    overlay_buffer_t overlaybuffer; 
    int index;
    recording_callback cb = NULL;

    mParameters.getPreviewSize(&w, &h);

    /* De-queue the next avaliable buffer */ 
    nCameraBuffersQueued--;    
    if (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
        LOGE("VIDIOC_DQBUF Failed!!!");
    }

    index = cfilledbuffer.index;
//    LOGE("Buffer index[%d]",index);

#ifdef FW3A
	//if(AF_STATUS_RUNNING == fobj->status_2a.af.status)
		//drawRect( (uint8_t *) cfilledbuffer.m.userptr, 200,  220, 180, 420, 300, w, h); 	
#endif
 
    // Notify overlay of a new frame.
    ret = mOverlay->queueBuffer((void*)cfilledbuffer.index);
    if (ret)
    {
        LOGD("qbuf failed. May be bcos stream was not turned on yet. So try again");
    }
    else
    {
        nOverlayBuffersQueued++;
    }

    if (nOverlayBuffersQueued)
    {
        mOverlay->dequeueBuffer(&overlaybuffer);
        nOverlayBuffersQueued--;
        cfilledbuffer.index = (int)overlaybuffer;
    }
    else
    {
        //cfilledbuffer.index = whatever was in there before..
        //That is, queue the same buffer that was dequeued
    }
    
    if (nOverlayBuffersQueued)
    {
        mOverlay->dequeueBuffer(&overlaybuffer);
        nOverlayBuffersQueued--;
        cfilledbuffer.index = (int)overlaybuffer;
    }
    else
    {
        //cfilledbuffer.index = whatever was in there before..
        //That is, queue the same buffer that was dequeued
    }

    nCameraBuffersQueued++;

    mRecordingLock.lock();
    cb = mRecordingCallback;

    if(cb){
//        LOGD("### recording:index:%d,pointer:0x%x,len:%d",cfilledbuffer.index,(void*)(cfilledbuffer.m.userptr),cfilledbuffer.length);
        memcpy(&mfilledbuffer[cfilledbuffer.index],&cfilledbuffer,sizeof(v4l2_buffer));
//        if(cfilledbuffer.index==0)
        cb(mVideoBuffer[cfilledbuffer.index], mRecordingCallbackCookie);
    } else {
//        LOGD("[O] without recording:index:%d,pointer:0x%x,len:%d",cfilledbuffer.index,(void*)(cfilledbuffer.m.userptr),cfilledbuffer.length);
        /* queue the buffer back to camera */
        while (ioctl(camera_device, VIDIOC_QBUF, &cfilledbuffer) < 0) {
            LOGE("VIDIOC_QBUF Failed.");
        }
    }
    mRecordingLock.unlock();
    return ;
}
	

int  CameraHal::ICapturePerform()
{

#ifdef ICAP
    int err;
    int status = 0;
    int jpegSize;
    void *outBuffer;
    Message procMsg;
    struct timeval tv1, tv2, tv;
    void* snapshot_buffer;
    ancillary_mms *mk_note;
    unsigned long base, offset;
    int snapshot_buffer_index;    
    enum v4l2_buf_type prv_buf_type;
    int image_width, image_height;
    int preview_width, preview_height;
    sp<MemoryBase> mPictureBuffer ;
    sp<MemoryBase> mJPEGPictureMemBase;    
    sp<MemoryHeapBase>  mJPEGPictureHeap;
    struct manual_parameters  manual_config;
    unsigned short ipp_ee_q, ipp_ew_ts, ipp_es_ts, ipp_luma_nf, ipp_chroma_nf; 
	int vppMessage = 0;

    LOG_FUNCTION_NAME

#ifdef ICAP
    if (iobj==NULL) {
      LOGE("Doesn't exist ICapture");
      return -1;
    }
#endif
    
    PPM("START OF ICapturePerform");
    
    if( mShutterCallback ) {
        mShutterCallback(mPictureCallbackCookie );
    }   

    PPM("CALLED SHUTTER CALLBACK");
    
    mParameters.getPictureSize(&image_width, &image_height);
    mParameters.getPreviewSize(&preview_width, &preview_height);
    LOGD("ICapturePerform image_width=%d image_height=%d",image_width,image_height);

    mk_note = (ancillary_mms *) malloc(sizeof(*mk_note));
    memset(mk_note, 0, sizeof(*mk_note));
    if( NULL == mk_note){
        LOGE("Maker Note malloc failed");
        goto fail_mk_note;
    }
    memset(&manual_config, 0 ,sizeof(manual_config));

#ifdef FW3A
    err = fobj->cam_iface_2a->ReadSatus(fobj->cam_iface_2a->pPrivateHandle, &fobj->status_2a);

    LOGD("shutter_cap = %d ; again_cap = %d ; awb_index = %d; %d %d %d %d\n", 
        (int)fobj->status_2a.ae.shutter_cap, (int)fobj->status_2a.ae.again_cap,
        (int)fobj->status_2a.awb.awb_index, (int)fobj->status_2a.awb.gain_Gr,
        (int)fobj->status_2a.awb.gain_R, (int)fobj->status_2a.awb.gain_B,
        (int)fobj->status_2a.awb.gain_Gb); 
    manual_config.shutter_usec      = fobj->status_2a.ae.shutter_cap;
    manual_config.analog_gain       = fobj->status_2a.ae.again_cap;
    manual_config.color_temparature = fobj->status_2a.awb.awb_index;
    manual_config.gain_Gr           = fobj->status_2a.awb.gain_Gr;
    manual_config.gain_R            = fobj->status_2a.awb.gain_R;
    manual_config.gain_B            = fobj->status_2a.awb.gain_B;
    manual_config.gain_Gb           = fobj->status_2a.awb.gain_Gb;
    manual_config.digital_gain      = fobj->status_2a.awb.dgain;
    manual_config.scene = fobj->settings_2a.general.scene;
    manual_config.effect = fobj->settings_2a.general.effects;
    PPM("SETUP SOME 3A STUFF");
#else
    manual_config.shutter_usec          = 60000;
    manual_config.analog_gain           = 20;
    manual_config.color_temparature     = 4500;
    mk_note->exposure.shutter_cap_msec  = 60;
    mk_note->exposure.gain_cap          = 20;
    mk_note->balance.awb_idx            = 4500;
#endif

#ifdef OPEN_CLOSE_WORKAROUND
    close(camera_device);
    camera_device = open(VIDEO_DEVICE, O_RDWR);
    if (camera_device < 0) {
        LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!",  strerror(errno) );
    }   
    PPM("CLOSED AND REOPENED CAMERA");
#endif

#ifdef ICAP
    iobj->cfg.image_width   = image_width;
    iobj->cfg.image_height  = image_height;
    iobj->cfg.lsc_type      = LSC_UPSAMPLED_BY_SOFTWARE;
    iobj->cfg.cam_dev       = camera_device;
    iobj->cfg.mknote        = mk_note;
    iobj->cfg.manual        = &manual_config;
    iobj->cfg.priv          = NULL;//this;         
    iobj->cfg.cb_write_h3a  = NULL;//onSaveH3A;
    iobj->cfg.cb_write_lsc  = NULL;//onSaveLSC;
    iobj->cfg.cb_write_raw  = NULL;//onSaveRAW;
    manual_config.pre_flash = 0;

    //if(mcapture_mode == 0){
        LOGD("Capture mode= HP");
        iobj->cfg.capture_mode  =  CAPTURE_MODE_HI_PERFORMANCE;	
    //}

    status = (capture_status_t) iobj->lib.Config(iobj->lib_private, &iobj->cfg);
    if( ICAPTURE_FAIL == status){
        LOGE ("ICapture Config function failed");
        goto fail_config;
    } 
    LOGD("ICapture config OK");

    yuv_len=iobj->cfg.sizeof_img_buf;
    PPM("MORE SETUP - HI PERFORMANCE");
#else
    ipp_ee_q =100;
    ipp_ew_ts=50;
    ipp_es_ts =50; 
    ipp_luma_nf =1;
    ipp_chroma_nf = 1;
    yuv_len=image_width*image_height*2;
#endif

    /*compute yuv size, allocate memory and take picture*/
    mPictureHeap = new MemoryHeapBase(yuv_len + 0x20 + 256);  
    base = (unsigned long)mPictureHeap->getBase(); 				
    /*Align buffer to 32 byte boundary */
    while ((base & 0x1f) != 0)							
    {				
        base++;
    }

    /* Buffer pointer shifted to avoid DSP cache issues */
    base += 128;			
    offset = base - (unsigned long)mPictureHeap->getBase();
    mPictureBuffer = new MemoryBase(mPictureHeap, offset, yuv_len);
    yuv_buffer = (uint8_t *) (mPictureHeap->getBase()) + offset;

#ifdef ICAP
    iobj->proc.img_buf[0].start =yuv_buffer; 
    iobj->proc.img_buf[0].length = yuv_len ; 
    iobj->proc.img_bufs_count = 1;

    status = (capture_status_t) iobj->lib.Process(iobj->lib_private, &iobj->proc);
    if( ICAPTURE_FAIL == status){
        LOGE("ICapture Process failed");
        goto fail_process;
    } else {
        LOGD("ICapture process OK");
    }
		
    ipp_ee_q =iobj->proc.eenf.ee_q,
    ipp_ew_ts=iobj->proc.eenf.ew_ts,
    ipp_es_ts=iobj->proc.eenf.es_ts, 
    ipp_luma_nf=iobj->proc.eenf.luma_nf,
    ipp_chroma_nf=iobj->proc.eenf.chroma_nf;

	iobj->proc.out_img_h &= 0xFFFFFFF8;

    LOGD("iobj->proc.out_img_w = %d iobj->proc.out_img_h=%u", (int)iobj->proc.out_img_w, (int)iobj->proc.out_img_h);
    LOGD("iobj->cfg.image_width = %d iobj->cfg.image_height=%d", (int)iobj->cfg.image_width, (int)iobj->cfg.image_height);
    LOGD("image_width = %d image_height=%d", (int)image_width, (int) image_height);

    //somehow have a different value from mParameters.getPictureSize(&image_width, &image_height);
    //updating values 
    image_width = (int)iobj->proc.out_img_w;
    image_height =(int)iobj->proc.out_img_h;

#endif
    PPM("IMAGE CAPTURED");
    mRawPictureCallback(mPictureBuffer,mPictureCallbackCookie);
    PPM("RAW CALLBACK CALLED");
    
#ifdef OPEN_CLOSE_WORKAROUND
    close(camera_device);
    camera_device = open(VIDEO_DEVICE, O_RDWR);
    if (camera_device < 0) {
        LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!", strerror(errno) );
    }
    PPM("CLOSED AND REOPENED CAMERA");
#endif	

#ifdef HARDWARE_OMX
	LOGD("SENDING MESSAGE TO VPP THREAD \n");
	vpp_buffer =  yuv_buffer;
	vppMessage = VPP_THREAD_PROCESS;
	write(vppPipe[1], &vppMessage,sizeof(int));	
#endif

#ifdef IMAGE_PROCESSING_PIPELINE    
    PPM("BEFORE IPP");

    LOGD("Calling ProcessBufferIPP(buffer=%p , len=0x%x)", yuv_buffer, yuv_len);
    err = ProcessBufferIPP(yuv_buffer, yuv_len,
                    ipp_ee_q,
                    ipp_ew_ts,
                    ipp_es_ts, 
                    ipp_luma_nf,
                    ipp_chroma_nf);

   	if(!(pIPP.ippconfig.isINPLACE)){
		yuv_buffer = pIPP.pIppOutputBuffer;
	}
    PPM("AFTER IPP"); 

	#if !YUV422I 
		yuv_len=  ((image_width * image_height *3)/2);
	#endif
		
    
#endif

#ifdef HARDWARE_OMX
#if JPEG
    err = 0;    
    jpegSize = image_width*image_height + 13000;
    mJPEGPictureHeap = new MemoryHeapBase(jpegSize+ 256);   
    outBuffer = (void *)((unsigned long)(mJPEGPictureHeap->getBase()) + 128);

    PPM("BEFORE ENCODE IMAGE");		    
    if (!( jpegEncoder->encodeImage((uint8_t *)outBuffer , jpegSize, yuv_buffer, yuv_len,
                                 image_width, image_height, quality)))
    {        
        err = -1;
        LOGE("JPEG Encoding failed");
    }

    PPM("AFTER ENCODE IMAGE");
    mJPEGPictureMemBase = new MemoryBase(mJPEGPictureHeap, 128, jpegEncoder->jpegSize);
#endif

    if(mJpegPictureCallback) {

#if JPEG
		mJpegPictureCallback(mJPEGPictureMemBase, mPictureCallbackCookie); 
#else
		mJpegPictureCallback(NULL, mPictureCallbackCookie); 
#endif

    }

#if JPEG 
	PPM("CALLED JPEG CALLBACK");
    LOGD("jpegEncoder->jpegSize=%d jpegSize=%d",jpegEncoder->jpegSize,jpegSize);

    if ((err==0) && (mMMSApp))
    {
        SaveFile(NULL, (char*)"jpeg", outBuffer, jpegEncoder->jpegSize); 
        SaveFile(NULL, (char*)"mknote", mk_note, sizeof(*mk_note));
    }    
    PPM("SAVED ENCODED FILE TO DISK");

    mJPEGPictureMemBase.clear();		
    mJPEGPictureHeap.clear();
#endif
#endif

	free(mk_note);
    mPictureBuffer.clear();
    mPictureHeap.clear();

	LOGD("CameraHal thread before waiting increment in semaphore\n");
	sem_wait(&mIppVppSem);
	LOGD("CameraHal thread after waiting increment in semaphore\n");
    
    PPM("END OF ICapturePerform");
    LOG_FUNCTION_NAME_EXIT
    return 0;

fail_process:
fail_config:
#ifdef ICAP
    iobj->lib.Delete(iobj->lib_private);
#endif
fail_create:
fail_icapture:
#ifdef ICAP
    free(mk_note);
#endif
fail_mk_note:

fail_iobj:
fail_open:
fail_deinit_preview:
fail_deinit_3afw:
    return -1;
#endif
    return 0;
}

void CameraHal::vppThread(){

	LOG_FUNCTION_NAME

	int snapshot_buffer_index;
	void* snapshot_buffer;
	int status;
    int image_width, image_height;
    int preview_width, preview_height;
	overlay_buffer_t overlaybuffer;
	fd_set descriptorSet;	
	int max_fd;
	int error;
	unsigned long int pipeMessage;
	max_fd =vppPipe[0] +1;

	FD_ZERO(&descriptorSet);
	FD_SET(vppPipe[0], &descriptorSet);
	
	while(1){

		error= select(max_fd,  &descriptorSet, NULL, NULL, NULL);

		LOGD("VPP THREAD SELECT RECEIVED A MESSAGE\n");
		if (error<1){
			LOGE("Error in select");
		}

		if(FD_ISSET(vppPipe[0], &descriptorSet)){

			read(vppPipe[0], &pipeMessage, sizeof(int));

			if(pipeMessage == VPP_THREAD_PROCESS){

				LOGD("VPP_THREAD_PROCESS_RECEIVED\n");
		
				mParameters.getPictureSize(&image_width, &image_height);
				mParameters.getPreviewSize(&preview_width, &preview_height);
		
				//snapshot buffer is the last overlay buffer
				snapshot_buffer_index = mOverlay->getBufferCount() - 1; //JJ-remove -1
				snapshot_buffer = mOverlay->getBufferAddress( (void*)snapshot_buffer_index );

				status = scale_process(vpp_buffer, image_width, image_height,
						             snapshot_buffer, preview_width, preview_height);
				if( status ){
					LOGE("scale_process() failed");
				}
				else {
					LOGD("scale_process() OK");
				}

				PPM("SCALED DOWN RAW IMAGE TO PREVIEW SIZE");

				mOverlay->queueBuffer((void*)snapshot_buffer_index);  //JJ-try removing dequeue buffer
				mOverlay->dequeueBuffer(&overlaybuffer);

				PPM("DISPLAYED SNAPSHOT ON THE SCREEN");
								
				sem_post(&mIppVppSem);
			}

			else if( pipeMessage == VPP_THREAD_EXIT ){
				LOGD("VPP_THREAD_EXIT_RECEIVED\n");
				break;
			}
		}
	}

	LOG_FUNCTION_NAME_EXIT
}

int CameraHal::ICaptureCreate(void)
{
    int res;
    overlay_buffer_t overlaybuffer;
    int image_width, image_height;

    LOG_FUNCTION_NAME
    
#ifdef ICAP
    iobj = (libtest_obj *) malloc( sizeof( *iobj));
    if( NULL == iobj) {
        LOGE("libtest_obj malloc failed");
        goto exit;
    }
#endif

    mParameters.getPictureSize(&image_width, &image_height);
    LOGD("ICaptureCreate: Picture Size %d x %d", image_width, image_height);
#ifdef ICAP
    memset(iobj, 0 , sizeof(*iobj));

    iobj->lib.lib_handle = dlopen(LIBICAPTURE_NAME, RTLD_LAZY);
    if( NULL == iobj->lib.lib_handle){
        LOGE("Can not open ICapture Library: %s", dlerror());
        goto exit;
    }

    if( res >= 0){error:
        iobj->lib.Create = ( int (*) (void **, int) ) dlsym(iobj->lib.lib_handle, "capture_create");
        if(NULL == iobj->lib.Create){
            res = -1;
        } 	
    }

    if( res >= 0){
        iobj->lib.Delete = ( int (*) (void *) ) dlsym(iobj->lib.lib_handle, "capture_delete");
        if( NULL == iobj->lib.Delete){
            res = -1;
        }
    }

    if( res >= 0){
        iobj->lib.Config = ( int (*) (void *, capture_config_t *) ) dlsym(iobj->lib.lib_handle, "capture_config");
        if( NULL == iobj->lib.Config){
            res = -1;
        }
    }

    if( res >= 0){
        iobj->lib.Process = ( int (*) (void *, capture_process_t *) ) dlsym(iobj->lib.lib_handle, "capture_process");
        if( NULL == iobj->lib.Config){
            res = -1;
        }
    }

    res = (capture_status_t) iobj->lib.Create(&iobj->lib_private, camera_device);
    if( ( ICAPTURE_FAIL == res) || NULL == iobj->lib_private){
        LOGE("ICapture Create function failed");
        goto fail_icapture;
    } 
    LOGD("ICapture create OK");

#endif

#ifdef HARDWARE_OMX

    res = scale_init();
    if( res ) {
        LOGE("scale_init() failed");
        goto fail_scale;
    } 
    LOGD("scale_init() OK");

    jpegEncoder = new JpegEncoder;

#endif

#ifdef IMAGE_PROCESSING_PIPELINE

    res = InitIPP(image_width,image_height);
    if( res ) {
        LOGE("InitIPP() failed");
        goto fail_ipp_init;
    }
    LOGD("InitIPP() OK");

    res = PopulateArgsIPP(image_width,image_height);
    if( res ) {
        LOGE("PopulateArgsIPP() failed");
        goto fail_ipp_populate;
    } 
    LOGD("PopulateArgsIPP() OK");

#endif

    LOG_FUNCTION_NAME_EXIT
    return res;

fail_jpeg_buffer:
fail_yuv_buffer:

#ifdef HARDWARE_OMX
    delete jpegEncoder;
#endif    

#ifdef IMAGE_PROCESSING_PIPELINE
fail_ipp_populate:
    DeInitIPP();
fail_ipp_init:
#endif

#ifdef HARDWARE_OMX    
    scale_deinit();
fail_scale:
#endif

#ifdef ICAP
    iobj->lib.Delete(&iobj->lib_private);
#endif

fail_icapture:
exit:
    return -1;
}



int CameraHal::ICaptureDestroy(void)
{
    int err;
#ifdef HARDWARE_OMX

    err = scale_deinit();
    if( err ) LOGE("scale_deinit() failed");
    else LOGD("scale_deinit() OK");

#endif

#ifdef IMAGE_PROCESSING_PIPELINE

    err = DeInitIPP();
    if( err ) LOGE("DeInitIPP() failed");
    else LOGD("DeInitIPP() OK");
    
#endif

#ifdef HARDWARE_OMX
    delete jpegEncoder;
#endif
    
#ifdef ICAP
    if (iobj->lib_private) {
        err = (capture_status_t) iobj->lib.Delete(iobj->lib_private);
        if(ICAPTURE_FAIL == err){
            LOGE("ICapture Delete failed");
        } else {
            LOGD("ICapture delete OK");
        }
    }

    iobj->lib_private = NULL;
 
    dlclose(iobj->lib.lib_handle);
    free(iobj);
    iobj = NULL;
#endif

    return 0;
}

status_t CameraHal::setOverlay(const sp<Overlay> &overlay) 
{
    Mutex::Autolock lock(mLock);

    LOGD("CameraHal setOverlay/1/%08lx/%08lx", (long unsigned int)overlay.get(), (long unsigned int)mOverlay.get());
    // De-alloc any stale data
    if ( mOverlay.get() != NULL )
    {
        LOGD("Destroying current overlay");
        mOverlay->destroy();
    }

    mOverlay = overlay;
    if (mOverlay == NULL)
    {
        LOGE("Trying to set overlay, but overlay is null!");
    }
    return NO_ERROR;
} 

status_t CameraHal::startPreview(preview_callback cb, void* user)
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = PREVIEW_START;
    msg.arg1 = (void*)cb;
    msg.arg2 = (void*)user;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

    LOG_FUNCTION_NAME_EXIT
    return msg.command == PREVIEW_ACK ? NO_ERROR : INVALID_OPERATION;
}

void CameraHal::stopPreview()
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = PREVIEW_STOP;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);
}

status_t CameraHal::autoFocus(autofocus_callback af_cb,
                                       void *user)
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = PREVIEW_AF_START;
    msg.arg1 = (void*)af_cb;
    msg.arg2 = (void*)user;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

   LOG_FUNCTION_NAME_EXIT
    return msg.command == PREVIEW_ACK ? NO_ERROR : INVALID_OPERATION;        
}

bool CameraHal::previewEnabled()
{
    return mPreviewRunning;
}

status_t CameraHal::startRecording(recording_callback cb, void* user)
{
    LOG_FUNCTION_NAME
    int w,h;
    int i = 0;

    mParameters.getPreviewSize(&w, &h);

    // Just for the same size case
    mRecordingFrameSize = w * h * 2;

    overlay_handle_t overlayhandle = mOverlay->getHandleRef();

    overlay_true_handle_t true_handle;

    memcpy(&true_handle,overlayhandle,sizeof(overlay_true_handle_t));

    int overlayfd = true_handle.ctl_fd;

    LOGD("#Overlay driver FD:%d ",overlayfd);

    mVideoBufferCount =  mOverlay->getBufferCount();

    for(i = 0; i < mVideoBufferCount; i++)
    {
        mVideoBufferPtr[i] = (unsigned long)mOverlay->getBufferAddress((void*)i);
        LOGD("mVideoBufferPtr[%d] = 0x%x", i,mVideoBufferPtr[i]);
    }

    if(cb)
    {
        LOGD("Clear the old memory ");
        mVideoHeap.clear();
        for(i = 0; i < mVideoBufferCount; i++)
        {
            mVideoBuffer[i].clear();
        }
        LOGD("Mmap the video Memory %d", mPreviewFrameSize);
        mVideoHeap = new MemoryHeapBase(overlayfd,  mPreviewFrameSize * mVideoBufferCount);
        LOGD("mVideoHeap ID:%d , Base:[%x],size:%d", mVideoHeap->getHeapID(),
                                       mVideoHeap->getBase(),mVideoHeap->getSize());
        for(i = 0; i < mVideoBufferCount; i++)
        {
            LOGD("Init Video Buffer:%d ",i);
            mVideoBuffer[i] = new MemoryBase(mVideoHeap, mPreviewFrameSize*i, mRecordingFrameSize);
            LOGD("pointer:[%x],size:%d,offset:%d", mVideoBuffer[i]->pointer(),mVideoBuffer[i]->size(),mVideoBuffer[i]->offset());
        }
    }
    mRecordingLock.lock();
    mRecordingCallback = cb;
    mRecordingCallbackCookie = user;
    mRecordingLock.unlock();
    return NO_ERROR;
}


void CameraHal::stopRecording()
{
    LOG_FUNCTION_NAME
    mRecordingLock.lock();
    mRecordingCallback = NULL;
    mRecordingCallbackCookie = NULL;
    mRecordingLock.unlock();
}

bool CameraHal::recordingEnabled()
{
    LOG_FUNCTION_NAME
    return mRecordingCallback !=0;
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
        mFps =  ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        mLastFrameCount = mFrameCount;
    }
	LOGD("####### [%d] Frames, %f FPS", mFrameCount, mFps);
    // XXX: mFPS has the value we want
 }

void CameraHal::releaseRecordingFrame(const sp<IMemory>& mem)
{
    LOG_FUNCTION_NAME
    ssize_t offset;
    size_t  size;
    int index;
    int time = 0;

    offset = mem->offset();
    size   = mem->size();
    index = offset / size;

    mRecordingFrameCount++;
//    LOGD("Buffer[%d] pointer=0x%x",index,mem->pointer());
    debugShowFPS();

    /* queue the buffer back to camera */
    while (ioctl(camera_device, VIDIOC_QBUF, &mfilledbuffer[index]) < 0) {
        LOGE("Recording VIDIOC_QBUF Failed.");
        if(++time >= 4)break;
    }
    return;
}

sp<IMemoryHeap>  CameraHal::getRawHeap() const
{
    return mPictureHeap;
}

status_t CameraHal::takePicture(shutter_callback shutter_cb,
                                         raw_callback raw_cb,
                                         jpeg_callback jpeg_cb,
                                         void* user)
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = PREVIEW_CAPTURE;
    msg.arg1    = (void*)shutter_cb;
    msg.arg2    = (void*)raw_cb;
    msg.arg3    = (void*)jpeg_cb;
    msg.arg4    = user;
    previewThreadCommandQ.put(&msg);

    LOG_FUNCTION_NAME_EXIT    
    return NO_ERROR;
}

status_t CameraHal::cancelPicture(bool cancel_shutter,
                                           bool cancel_raw,
                                           bool cancel_jpeg)
{
    LOG_FUNCTION_NAME

    mRawPictureCallback = NULL;
    mJpegPictureCallback = NULL;
    mPictureCallbackCookie = NULL;

    LOGE("Callbacks set to null");
    return -1;
}

int CameraHal::validateSize(int w, int h)
{
    if ((w < MIN_WIDTH) || (h < MIN_HEIGHT)){
        return false;
    }
    return true;
}

status_t CameraHal::setParameters(const CameraParameters &params)
{
    LOG_FUNCTION_NAME

    int w, h;
    int framerate;
    int iso, af, mode, zoom, wb, exposure, scene;
    int effects, compensation, saturation, sharpness;
    int contrast, brightness, flash, caf;

    Mutex::Autolock lock(mLock);
      
    LOGD("PreviewFormat %s", params.getPreviewFormat());

    if (strcmp(params.getPreviewFormat(), "yuv422i") != 0) {
        LOGE("Only yuv422i preview is supported");
        return -1;
    }
    
    LOGD("PictureFormat %s", params.getPictureFormat());
    if (strcmp(params.getPictureFormat(), "jpeg") != 0) {
        LOGE("Only jpeg still pictures are supported");
        return -1;
    }

    params.getPreviewSize(&w, &h);
    if (!validateSize(w, h)) {
        LOGE("Preview size not supported");
        return -1;
    }
    LOGD("PreviewSize %d x %d", w, h);

    params.getPictureSize(&w, &h);
    if (!validateSize(w, h)) {
        LOGE("Picture size not supported");
        return -1;
    }
    LOGD("Picture Size %d x %d", w, h);
    
    framerate = params.getPreviewFrameRate(); 
    LOGD("FRAMERATE %d", framerate);

    mParameters = params;
    /* This is a hack. MMS APP is not setting the resolution correctly. So hardcoding it. */
    mParameters.setPictureSize(PICTURE_WIDTH, PICTURE_HEIGHT);
    mMMSApp = mParameters.getInt("MMS_APP");
    if (mMMSApp != 10) mMMSApp = 0;
    LOGD("mMMSApp = %d", mMMSApp);
    
    if (mMMSApp == 0) quality = params.getInt("jpeg-quality");
    else quality = 85;
    if (quality < 10) quality = 85;
    LOGD("quality = %d", quality);




#ifdef FW3A

    if (( NULL != fobj) && (mMMSApp)){	
        iso = mParameters.getInt("iso");
        af = mParameters.getInt("af");
        mcapture_mode = mParameters.getInt("mode");
        zoom = mParameters.getInt("zoom");
        wb = mParameters.getInt("wb");
        exposure = mParameters.getInt("exposure");
        scene = mParameters.getInt("scene");
        effects = mParameters.getInt("effects");
        compensation = mParameters.getInt("compensation");
        saturation = mParameters.getInt("saturation");
        sharpness = mParameters.getInt("sharpness");
        contrast = mParameters.getInt("contrast");
        brightness = mParameters.getInt("brightness");
        mred_eye = mParameters.getInt("red");
        flash = mParameters.getInt("flash");
        caf = mParameters.getInt("caf");
        myuv = mParameters.getInt("yuv");

        FW3A_GetSettings();	

        fobj->settings_2a.general.contrast = contrast;
        fobj->settings_2a.general.brightness = brightness;
        fobj->settings_2a.general.saturation = saturation;
        fobj->settings_2a.general.sharpness = sharpness;
        fobj->settings_2a.general.scene = (FW3A_SCENE_MODE) scene;
        fobj->settings_2a.general.effects = (FW3A_CONFIG_EFFECTS) effects;
        fobj->settings_2a.awb.mode = (WHITE_BALANCE_MODE_VALUES) wb;
        fobj->settings_2a.ae.iso = (EXPOSURE_ISO_VALUES) iso;
        fobj->settings_2a.af.focus_mode = (FOCUS_MODE_VALUES) af;
        fobj->settings_2a.ae.mode = (EXPOSURE_MODE_VALUES) exposure;
        fobj->settings_2a.ae.compensation = compensation;

        if(mflash != flash){
            mflash = flash;
            fobj->settings_2a.general.flash_mode = (FW3A_FLASH_MODE) mflash;		
        }

        fobj->settings_2a.af.focus_mode = FOCUS_MODE_AF_AUTO;
        fobj->settings_2a.af.spot_weighting = FOCUS_SPOT_MULTI_NORMAL;

        FW3A_SetSettings();

        if( mzoom != zoom){
            ZoomPerform(zoom);
            mzoom = zoom;		
        }

        LOGD("mcapture_mode = %d", mcapture_mode);
        
        if(mcaf != caf){
            mcaf = caf;
            Message msg;
            msg.command = mcaf ? PREVIEW_CAF_START : PREVIEW_CAF_STOP;
            previewThreadCommandQ.put(&msg);
            previewThreadAckQ.get(&msg);
            return msg.command == PREVIEW_ACK ? NO_ERROR : INVALID_OPERATION;
        }
        
    }

#endif
    
    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR;
}

CameraParameters CameraHal::getParameters() const
{
    LOG_FUNCTION_NAME
    CameraParameters params;
    int iso, af, wb, exposure, scene, effects, compensation;
    int saturation, sharpness, contrast, brightness;

    {
        Mutex::Autolock lock(mLock);
        params = mParameters;
    }

#if 0

    if (NULL != fobj){
        fobj->cam_iface_2a->ReadSettings(fobj->cam_iface_2a->pPrivateHandle, &fobj->settings_2a);

        contrast = fobj->settings_2a.general.contrast;
        brightness = fobj->settings_2a.general.brightness;
        saturation = fobj->settings_2a.general.saturation;
        sharpness = fobj->settings_2a.general.sharpness;
        scene = fobj->settings_2a.general.scene;
        effects = fobj->settings_2a.general.effects;
        wb = fobj->settings_2a.awb.mode;
        iso = fobj->settings_2a.ae.iso;
        af = fobj->settings_2a.af.focus_mode;
        exposure = fobj->settings_2a.ae.mode;
        compensation = fobj->settings_2a.ae.compensation;

        params.set("iso", iso);
        params.set("af", af);
        params.set("wb", wb);
        params.set("exposure", exposure);
        params.set("scene", scene);
        params.set("effects", effects);
        params.set("compensation", compensation);
        params.set("saturation", saturation);
        params.set("sharpness", sharpness);
        params.set("contrast", contrast);
        params.set("brightness", brightness);
    }

#endif

    LOG_FUNCTION_NAME_EXIT
    return params;
}

status_t  CameraHal::dump(int fd, const Vector<String16>& args) const
{
    return 0;
}

void CameraHal::dumpFrame(void *buffer, int size, char *path)
{
    FILE* fIn = NULL;    

    fIn = fopen(path, "w");
    if ( fIn == NULL ) {
        LOGE("\n\n\n\nError: failed to open the file %s for writing\n\n\n\n", path);
        return;
    }
    
    fwrite((void *)buffer, 1, size, fIn);
    fclose(fIn);

}

void CameraHal::release()
{
}

int CameraHal::onSaveH3A(void *priv, void *buf, int size)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOGD("Observer onSaveH3A\n");
    camHal->SaveFile(NULL, (char*)"h3a", buf, size);

    return 0;
}

int CameraHal::onSaveLSC(void *priv, void *buf, int size)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOGD("Observer onSaveLSC\n");
    camHal->SaveFile(NULL, (char*)"lsc", buf, size);

    return 0;
}

int CameraHal::onSaveRAW(void *priv, void *buf, int size)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOGD("Observer onSaveRAW\n");
    camHal->SaveFile(NULL, (char*)"raw", buf, size);

    return 0;
}


int CameraHal::SaveFile(char *filename, char *ext, void *buffer, int size)
{
    LOG_FUNCTION_NAME
    //Store image
    char fn [512];

    if (filename) {
      strcpy(fn,filename);
    } else {
      if (ext==NULL) ext = (char*)"tmp";
      sprintf(fn, PHOTO_PATH, file_index, ext);
    }
    file_index++;
    LOGD("Writing to file: %s", fn);
    int fd = open(fn, O_RDWR | O_CREAT | O_SYNC);
    if (fd < 0) {
        LOGE("Cannot open file %s : %s", fn, strerror(errno));
        return -1;
    } else {
    
        int written = 0;
        int page_block, page = 0;
        int cnt = 0;
        int nw;
        char *wr_buff = (char*)buffer;
        LOGD("Jpeg size %d", size);
        page_block = size / 20;
        while (written < size ) {
          nw = size - written;
          nw = (nw>512*1024)?8*1024:nw;
          
          nw = ::write(fd, wr_buff, nw);
          if (nw<0) {
              LOGD("write fail nw=%d, %s", nw, strerror(errno));
            break;
          }
          wr_buff += nw;
          written += nw;
          cnt++;
          
          page    += nw;
          if (page>=page_block){
              page = 0;
              LOGD("Percent %6.2f, wn=%5d, total=%8d, jpeg_size=%8d", 
                  ((float)written)/size, nw, written, size);
          }
        }

        close(fd);
     
        return 0;
    }
}


sp<IMemoryHeap> CameraHal::getPreviewHeap() const
{
    LOG_FUNCTION_NAME
    return 0;
}

sp<CameraHardwareInterface> CameraHal::createInstance()
{
    LOG_FUNCTION_NAME

    if (singleton != 0) {
        sp<CameraHardwareInterface> hardware = singleton.promote();
        if (hardware != 0) {
            return hardware;
        }
    }

    sp<CameraHardwareInterface> hardware(new CameraHal());

    singleton = hardware;
    return hardware;
} 

extern "C" sp<CameraHardwareInterface> openCameraHardware()
{
    LOGD("opening ti camera hal\n");
    return CameraHal::createInstance();
}


}; // namespace android

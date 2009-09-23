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

#define USE_MEMCOPY_FOR_VIDEO_FRAME 1

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
			mMMSApp(0),
			pictureNumber(0),
			mfirstTime(0)
{

	gettimeofday(&ppm_start, NULL);

    isStart_FW3A = false;
    isStart_FW3A_AF = false;
    isStart_FW3A_CAF = false;
    isStart_FW3A_AEWB = false;

#ifdef FOCUS_RECT
    focus_rect_set = 0;
#endif

    int i = 0;
    for(i = 0; i < VIDEO_FRAME_COUNT_MAX; i++)
    {
        mVideoBuffer[i] = 0;
        buffers_queued_to_dss[i] = 0;
    }

    CameraCreate();

    initDefaultParameters();

    CameraConfigure();

#ifdef CAMERA_ALGO
    camAlgos = new CameraAlgo();
#endif

#ifdef FW3A
    FW3A_Create();
#endif

    ICaptureCreate();

    mPreviewThread = new PreviewThread(this);
    mPreviewThread->run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);

#if VPP_THREAD
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
#endif
    
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
	int vppMessage [1];
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
#if VPP_THREAD
	sem_destroy(&mIppVppSem);

	vppMessage[0] = VPP_THREAD_EXIT;
	write(vppPipe[1], vppMessage,sizeof(unsigned int));

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
#endif

#ifdef CAMERA_ALGO
    camAlgos->unInitFaceTracking();
    
    delete camAlgos;
#endif

    ICaptureDestroy();

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
                    int delay;
                    gettimeofday(&focus_after, NULL);
                    delay = (focus_after.tv_sec - focus_before.tv_sec)*1000;
                    delay += (focus_after.tv_usec - focus_before.tv_usec)/1000;
                    LOGD(" AF Completed in %d [msec.]", delay);
                    fobj->cam_iface_2a->ReadMakerNote(fobj->cam_iface_2a->pPrivateHandle, ancillary_buffer, (uint32 *) &ancillary_len);
                    if (FW3A_Stop_AF() < 0){
						LOGE("ERROR FW3A_Stop_AF()");						
					}
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
                err = 0;

                if( ! mPreviewRunning ) {

                    PPM("CONFIGURING CAMERA TO RESTART PREVIEW");
					if (CameraConfigure() < 0){
						LOGE("ERROR CameraConfigure()");
						err= -1;
					}
#ifdef FW3A
                    if (FW3A_Start() < 0){
						LOGE("ERROR FW3A_Start()");
						err= -1;
					}
                    if (FW3A_SetSettings() < 0){
						LOGE("ERROR FW3A_SetSettings()");
						err= -1;
					}
                    
#endif
                    if (CameraStart() < 0){
						LOGE("ERROR CameraStart()");
						err = -1;
					}                   
                    
                    PPM("PREVIEW STARTED AFTER CAPTURING");

#ifdef CAMERA_ALGO
		            if( initAlgos() < 0 )
		                LOGE("Error while initializing Camera Algorithms");
#endif
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
				err = 0;
                if( mPreviewRunning ) {
#ifdef FW3A					
                    if( FW3A_Stop_AF() < 0){
						LOGE("ERROR FW3A_Stop_AF()");
						err= -1;
					}
					if( FW3A_Stop_CAF() < 0){
						LOGE("ERROR FW3A_Stop_CAF()");
						err= -1;
					}
					if( FW3A_Stop() < 0){
						LOGE("ERROR FW3A_Stop()");
						err= -1;
					}
					if( FW3A_GetSettings() < 0){
						LOGE("ERROR FW3A_GetSettings()");
						err= -1;
					}               
#endif
					if( CameraStop() < 0){
						LOGE("ERROR CameraStop()");
						err= -1;
					}
                   
                    if (err) {
                        LOGE("ERROR Cannot deinit preview.");
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
				err = 0;

                if( !mPreviewRunning ){
                    LOGD("WARNING PREVIEW NOT RUNNING!");
                    msg.command = PREVIEW_NACK;
                }
                else
                {
                    mAutoFocusCallback       = (autofocus_callback) msg.arg1;
                    mAutoFocusCallbackCookie = msg.arg2;
#ifdef FW3A   

					if (isStart_FW3A_CAF!= 0){
						if( FW3A_Stop_CAF() < 0){
							LOGE("ERROR FW3A_Stop_CAF();");
							err = -1;
						}
                    }

					gettimeofday(&focus_before, NULL);
					if (isStart_FW3A_AF == 0){
						if( FW3A_Start_AF() < 0){
							LOGE("ERROR FW3A_Start_AF()");
							err = -1;
						}
        
					}
#endif  
					mAutoFocusCallback( true, mAutoFocusCallbackCookie );
                    msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;   
					                
                }
                LOGD("Receive Command: PREVIEW_AF_START %s", msg.command == PREVIEW_NACK ? "NACK" : "ACK"); 
                previewThreadAckQ.put(&msg);
            }
            break;
		
			case PREVIEW_CAF_START:
			{	
                LOGD("Receive Command: PREVIEW_CAF_START");
				err=0;

                if( !mPreviewRunning )
                    msg.command = PREVIEW_NACK;
                else
                {
#ifdef FW3A    
					if( FW3A_Start_CAF() < 0){
						LOGE("ERROR FW3A_Start_CAF()");
						err = -1;
					} 
#endif
                    msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;
                }
                LOGD("Receive Command: PREVIEW_CAF_START %s", msg.command == PREVIEW_NACK ? "NACK" : "ACK"); 
                previewThreadAckQ.put(&msg);
			}
			break;
                
			case PREVIEW_CAF_STOP:
			{
                LOGD("Receive Command: PREVIEW_CAF_STOP");
				err = 0;

                if( !mPreviewRunning )
                    msg.command = PREVIEW_NACK;
                else
                {
#ifdef FW3A    
					if( FW3A_Stop_CAF() < 0){
						LOGE("ERROR FW3A_Stop_CAF()");
						err = -1;
					} 
#endif
                    msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;
                }
                LOGD("Receive Command: PREVIEW_CAF_STOP %s", msg.command == PREVIEW_NACK ? "NACK" : "ACK"); 
                previewThreadAckQ.put(&msg);
			}
			break;
	
            case PREVIEW_CAPTURE:
            {
                int flg_AF;
                int flg_CAF;

                LOGD("ENTER OPTION PREVIEW_CAPTURE");
                err = 0;
				PPM("RECEIVED COMMAND TO TAKE A PICTURE");
                
                mShutterCallback    = (shutter_callback)msg.arg1;
                mRawPictureCallback = (raw_callback)msg.arg2;
                mJpegPictureCallback= (jpeg_callback)msg.arg3;
                mPictureCallbackCookie = msg.arg4;

                //In burst mode the preview is not reconfigured between each picture 
                //so it can not be based on it to decide wheter the state is incorrect or not
                msg.command = PREVIEW_ACK;
                previewThreadAckQ.put(&msg);
        
                if( mPreviewRunning ) {

#ifdef FW3A     
				    if( (flg_AF = FW3A_Stop_AF()) < 0){
					    LOGE("ERROR FW3A_Stop_AF()");
					    err = -1;
				    }            
				    if( (flg_CAF = FW3A_Stop_CAF()) < 0){
					    LOGE("ERROR FW3A_Stop_CAF()");
					    err = -1;
				    }
				    if( FW3A_Stop() < 0){
					    LOGE("ERROR FW3A_Stop()");
					    err = -1;
				    }          
#endif
				    if( CameraStop() < 0){
					    LOGE("ERROR CameraStop()");
					    err = -1;
				    }          

                    mPreviewRunning =false;

                }
#ifdef FW3A
				if( FW3A_GetSettings() < 0){
					LOGE("ERROR FW3A_GetSettings()");
					err = -1;
				}
#endif
                
                PPM("STOPPED PREVIEW");
#ifdef ICAP
				if( ICapturePerform() < 0){
					LOGE("ERROR ICapturePerform()");
					err = -1;
				}     
#else
				if( CapturePicture() < 0){
					LOGE("ERROR CapturePicture()");
					err = -1;
				}   
#endif
                if( err ) {
                    LOGE("Capture failed.");
					err -1;
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

            case ZOOM_UPDATE:
            {
                LOGD("Receive Command: ZOOM_UPDATE");                          
                if(ZoomPerform(mzoom) < 0)
                    LOGE("Error setting the zoom");

            }
            break;

            case PREVIEW_KILL:
            {
                LOGD("Receive Command: PREVIEW_KILL");
				err = 0;				
	
                if (mPreviewRunning) {
#ifdef FW3A 
					if( FW3A_Stop_AF() < 0){
						LOGE("ERROR FW3A_Stop_AF()");
						err = -1;
					}
					if( FW3A_Stop_CAF() < 0){
						LOGE("ERROR FW3A_Stop_CAF()");
						err = -1;
					}
					if( FW3A_Stop() < 0){
						LOGE("ERROR FW3A_Stop()");
						err = -1;
					}
#endif 
					if( CameraStop() < 0){
						LOGE("ERROR FW3A_Stop()");
						err = -1;
					}
                    mPreviewRunning = false;
                }
	
				msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;
                LOGD("Receive Command: PREVIEW_CAF_STOP %s", msg.command == PREVIEW_NACK ? "NACK" : "ACK"); 

                previewThreadAckQ.put(&msg);
                shouldLive = false;
            }
            break;
        }
    }

   LOG_FUNCTION_NAME_EXIT
}

#ifdef CAMERA_ALGO

status_t CameraHal::initAlgos()
{
    int w,h;
    
    mParameters.getPreviewSize(&w, &h);
    
    return camAlgos->initFaceTracking(FACE_COUNT, w, h, AMFPAF_OPF_EQUAL, FRAME_SKIP, STABILITY, RATIO);
}

#endif

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

	LOG_FUNCTION_NAME

    close(camera_device);
    camera_device = -1;

    if (mOverlay != NULL) {
        mOverlay->destroy();
        mOverlay = NULL;
        nOverlayBuffersQueued = 0;
    }

    LOG_FUNCTION_NAME_EXIT
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
    
    LOGD("CameraConfigure: framerate to set = %d",framerate);
  
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    err = ioctl(camera_device, VIDIOC_G_PARM, &parm);
    if(err != 0) {
		LOGD("VIDIOC_G_PARM ");
		return -1;
    }
    
    LOGD("CameraConfigure: Old frame rate is %d/%d  fps",
		parm.parm.capture.timeperframe.denominator,
		parm.parm.capture.timeperframe.numerator);
  
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = framerate;
	err = ioctl(camera_device, VIDIOC_S_PARM, &parm);
	if(err != 0) {
		LOGE("VIDIOC_S_PARM ");
		return -1;
	}
    
    LOGI("CameraConfigure: New frame rate is %d/%d fps",parm.parm.capture.timeperframe.denominator,parm.parm.capture.timeperframe.numerator);

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

    nCameraBuffersQueued = 0;  

    mParameters.getPreviewSize(&w, &h);
    LOGD("**CaptureQBuffers: preview size=%dx%d", w, h);
  
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
    creqbuf.count  =  buffer_count ; 
    if (ioctl(camera_device, VIDIOC_REQBUFS, &creqbuf) < 0) {
        LOGE ("VIDIOC_REQBUFS Failed. %s", strerror(errno));
        goto fail_reqbufs;
    }

    for (int i = 0; i < (int)creqbuf.count; i++) {

        v4l2_cam_buffer[i].type = creqbuf.type;
        v4l2_cam_buffer[i].memory = creqbuf.memory;
        v4l2_cam_buffer[i].index = i;

        if (ioctl(camera_device, VIDIOC_QUERYBUF, &v4l2_cam_buffer[i]) < 0) {
            LOGE("VIDIOC_QUERYBUF Failed");
            goto fail_loop;
        }
      
        v4l2_cam_buffer[i].m.userptr = (unsigned long) mOverlay->getBufferAddress((void*)i);
        strcpy((char *)v4l2_cam_buffer[i].m.userptr, "hello");
        if (strcmp((char *)v4l2_cam_buffer[i].m.userptr, "hello")) {
            LOGI("problem with buffer\n");
            goto fail_loop;
        }

        LOGD("User Buffer [%d].start = %p  length = %d\n", i,
             (void*)v4l2_cam_buffer[i].m.userptr, v4l2_cam_buffer[i].length);

        if (buffers_queued_to_dss[i] == 0)
        {
            nCameraBuffersQueued++;
            if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[i]) < 0) {
                LOGE("CameraStart VIDIOC_QBUF Failed: %s", strerror(errno) );
                goto fail_loop;
            }
         }
         else LOGI("CameraStart::Could not queue buffer %d to Camera because it is being held by Overlay", i);
         
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

#if !OPEN_CLOSE_WORKAROUND
    while(nCameraBuffersQueued){
        nCameraBuffersQueued--;
        if (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
	        LOGE("VIDIOC_DQBUF Failed!!!");
			return -1;
        }
		LOGD("After Cam DQBUF. Buffers Queued=%d Buffer Dequeued=%d",nCameraBuffersQueued, cfilledbuffer.index);
    }
#endif
	LOGD("Done dequeuing from Camera!");

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
    int w, h, ret, queue_to_dss_failed;
    overlay_buffer_t overlaybuffer;// contains the index of the buffer dque
    int overlaybufferindex = -1; //contains the last buffer dque or -1 if dque failed
    int index;
    recording_callback cb = NULL;
	
	//LOG_FUNCTION_NAME

    mParameters.getPreviewSize(&w, &h);	

    /* De-queue the next avaliable buffer */	
    if (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
        LOGE("VIDIOC_DQBUF Failed!!!");
		goto EXIT;
    }else{
	    nCameraBuffersQueued--;
	}

#ifdef CAMERA_ALGO
	int delay;
    AMFPAF_FACERES *faces;
    
    gettimeofday(&algo_before, 0);
    faces = camAlgos->detectFaces( (uint8_t *) cfilledbuffer.m.userptr);
    gettimeofday(&algo_after, 0);
    delay = (algo_after.tv_sec - algo_before.tv_sec)*1000;
    delay += (algo_after.tv_usec - algo_before.tv_usec)/1000;
    //LOGD("Facetracking Completed in %d [msec.]", delay);

    if( faces->nFace != 0){
        for( int i = 0; i < faces->nFace; i++){
            drawRect( (uint8_t *) cfilledbuffer.m.userptr, FOCUS_RECT_GREEN,  faces->rcFace[i].left, faces->rcFace[i].top, faces->rcFace[i].right, faces->rcFace[i].bottom, w, h);
        }
    }
    
    lastOverlayIndex = cfilledbuffer.index;
#endif

#ifdef FW3A
#ifdef FOCUS_RECT
	/* Setting the color before driving the rectangle */
	if (focus_rect_set) {
		if(AF_STATUS_RUNNING == fobj->status_2a.af.status)
			focus_rect_color = FOCUS_RECT_WHITE;
		else if (AF_STATUS_SUCCESS == fobj->status_2a.af.status)
			focus_rect_color = FOCUS_RECT_GREEN;
		else if (AF_STATUS_FAIL == fobj->status_2a.af.status)
			focus_rect_color = FOCUS_RECT_RED;

		drawRect( (uint8_t *) cfilledbuffer.m.userptr, focus_rect_color,  220, 180, 440, 340, w, h);
	}
#endif
#endif

    queue_to_dss_failed = mOverlay->queueBuffer((void*)cfilledbuffer.index);
    if (queue_to_dss_failed)
	{
		LOGE("nextPreview(): mOverlay->queueBuffer() failed");
	}
	else
	{
	    nOverlayBuffersQueued++;
	    buffers_queued_to_dss[cfilledbuffer.index] = 1; //queued
	}

    if (nOverlayBuffersQueued >= NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE)
    {
		
        if(mOverlay->dequeueBuffer(&overlaybuffer)){
			LOGE("nextPreview(): mOverlay->dequeueBuffer() failed");
		}
		else{
        	overlaybufferindex = (int)overlaybuffer;
            nOverlayBuffersQueued--;
            buffers_queued_to_dss[(int)overlaybuffer] = 0;
            lastOverlayBufferDQ = (int)overlaybuffer;	
        }
    }
    else
    {
        //cfilledbuffer.index = whatever was in there before..
        //That is, queue the same buffer that was dequeued
    }

    mRecordingLock.lock();
    cb = mRecordingCallback;

    if(cb){

#if USE_MEMCOPY_FOR_VIDEO_FRAME
        for(int i = 0 ; i < VIDEO_FRAME_COUNT_MAX; i++ ){
            if(0 == mVideoBufferUsing[i]){
                memcpy(mVideoBuffer[i]->pointer(),(void *)cfilledbuffer.m.userptr, mRecordingFrameSize);
                mVideoBufferUsing[i] = 1;
                cb(0, mVideoBuffer[i], mRecordingCallbackCookie);
                break;
            }else {
                LOGE("No Buffer Can be used!");
            }
        }

        if (queue_to_dss_failed) {		
            if (ioctl(camera_device, VIDIOC_QBUF, &cfilledbuffer) < 0) {
                LOGE("VIDIOC_QBUF Failed, line=%d",__LINE__);
            }else{
	            nCameraBuffersQueued++;
			}
        }
        else if (overlaybufferindex != -1) {				
            if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[(int)overlaybuffer]) < 0) {
                LOGE("VIDIOC_QBUF Failed. line=%d",__LINE__);
            }else{
	            nCameraBuffersQueued++;
			}
        }

#else
        memcpy(&mfilledbuffer[cfilledbuffer.index],&cfilledbuffer,sizeof(v4l2_buffer));
        cb(0, mVideoBuffer[cfilledbuffer.index], mRecordingCallbackCookie);
#endif
    } 
    else {
        if (queue_to_dss_failed) {			
            if (ioctl(camera_device, VIDIOC_QBUF, &cfilledbuffer) < 0) {
                LOGE("VIDIOC_QBUF Failed, line=%d",__LINE__);
            }else{
	            nCameraBuffersQueued++;
			}
        }
        else if (overlaybufferindex != -1) {		
            if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[(int)overlaybuffer]) < 0) {
                LOGE("VIDIOC_QBUF Failed. line=%d",__LINE__);
            }else{
	            nCameraBuffersQueued++;
			}
        }
    }

    mRecordingLock.unlock();

EXIT:

    return ;
}
	
#ifdef ICAP
int  CameraHal::ICapturePerform()
{

    int err;
    int status = 0;
    int jpegSize;
    void *outBuffer;  
    void* snapshot_buffer;
    ancillary_mms *mk_note;
    unsigned long base, offset, jpeg_offset;
    int snapshot_buffer_index;
    int image_width, image_height;
    int preview_width, preview_height;
    sp<MemoryBase> mPictureBuffer ;
    sp<MemoryBase> mJPEGPictureMemBase;
    sp<MemoryHeapBase>  mJPEGPictureHeap;
    struct manual_parameters  manual_config;
    unsigned short ipp_ee_q, ipp_ew_ts, ipp_es_ts, ipp_luma_nf, ipp_chroma_nf; 
    int delay;
	unsigned int vppMessage[3];
	overlay_buffer_t overlaybuffer;
	

    LOG_FUNCTION_NAME

    if (iobj==NULL) {
      LOGE("Doesn't exist ICapture");
      return -1;
    }
    
    PPM("START OF ICapturePerform");

	LOGD("\n\n\n PICTURE NUMBER =%d\n\n\n",++pictureNumber);
    
    if( mShutterCallback ) {
        mShutterCallback(mPictureCallbackCookie );
    }

    PPM("CALLED SHUTTER CALLBACK");
    
    mParameters.getPictureSize(&image_width, &image_height);
    mParameters.getPreviewSize(&preview_width, &preview_height);
    LOGD("ICapturePerform image_width=%d image_height=%d",image_width,image_height);

    memset(&manual_config, 0 ,sizeof(manual_config));

#ifdef FW3A
	LOGD("ICapturePerform beforeReadStatus");
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
    manual_config.scene             = fobj->settings_2a.general.scene;
    manual_config.effect            = fobj->settings_2a.general.effects;
    manual_config.awb_mode          = fobj->settings_2a.awb.mode;
    manual_config.sharpness         = fobj->settings_2a.general.sharpness;
    manual_config.brightness        = fobj->settings_2a.general.brightness;
    manual_config.contrast          = fobj->settings_2a.general.contrast;
    manual_config.saturation        = fobj->settings_2a.general.saturation;
    
    PPM("SETUP SOME 3A STUFF");
#else
    manual_config.shutter_usec          = 60000;
    manual_config.analog_gain           = 20;
    manual_config.color_temparature     = 4500;
    mk_note->exposure.shutter_cap_msec  = 60;
    mk_note->exposure.gain_cap          = 20;
    mk_note->balance.awb_idx            = 4500;
#endif

#if OPEN_CLOSE_WORKAROUND
    close(camera_device);
    camera_device = open(VIDEO_DEVICE, O_RDWR);
    if (camera_device < 0) {
        LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!",  strerror(errno) );
    }   
    PPM("CLOSED AND REOPENED CAMERA");
#endif

    iobj->cfg.image_width   = image_width/*PICTURE_WIDTH*/;
    iobj->cfg.image_height  = image_height/*PICTURE_HEIGHT*/;
    iobj->cfg.lsc_type      = LSC_UPSAMPLED_BY_SOFTWARE;
    iobj->cfg.cam_dev       = camera_device;
    iobj->cfg.mknote        = ancillary_buffer;
    iobj->cfg.manual        = &manual_config;
    iobj->cfg.priv          = NULL;//this;
    iobj->cfg.cb_write_h3a  = NULL;//onSaveH3A;
    iobj->cfg.cb_write_lsc  = NULL;//onSaveLSC;
    iobj->cfg.cb_write_raw  = NULL;//onSaveRAW;
    manual_config.pre_flash = 0;

    if(mcapture_mode == 1){
        LOGD("Capture mode= HP");
        iobj->cfg.capture_mode  =  CAPTURE_MODE_HI_PERFORMANCE;
    } else {
        LOGD("Capture mode= HQ");
        iobj->cfg.capture_mode  =  CAPTURE_MODE_HI_QUALITY;
    }   

	PPM("Before ICapture Config");

    status = (capture_status_t) iobj->lib.Config(iobj->lib_private, &iobj->cfg);
    if( ICAPTURE_FAIL == status){
        LOGE ("ICapture Config function failed");
        goto fail_config;
    }
    PPM("ICapture config OK");

	LOGD("iobj->cfg.image_width = %d = 0x%x iobj->cfg.image_height=%d = 0x%x , iobj->cfg.sizeof_img_buf = %d", (int)iobj->cfg.image_width, (int)iobj->cfg.image_width,(int)iobj->cfg.image_height,(int)iobj->cfg.image_height, iobj->cfg.sizeof_img_buf);

    yuv_len = iobj->cfg.sizeof_img_buf;

    /*compute yuv size, allocate memory and take picture*/
#define ALIGMENT 1
#if ALIGMENT
	mPictureHeap = new MemoryHeapBase(yuv_len);
#else
    mPictureHeap = new MemoryHeapBase(yuv_len + 0x20 + 256);
#endif

    base = (unsigned long)mPictureHeap->getBase();

    /*Align buffer to 32 byte boundary */
#if ALIGMENT
	base = (base + 0xfff) & 0xfffff000;
#else
    while ((base & 0x1f) != 0)
    {
        base++;
    }
    /* Buffer pointer shifted to avoid DSP cache issues */
    base += 128;
#endif

    offset = base - (unsigned long)mPictureHeap->getBase();
	jpeg_offset = offset;

    mPictureBuffer = new MemoryBase(mPictureHeap, offset, yuv_len);

	yuv_buffer =(uint8_t*) base;      

    iobj->proc.img_buf[0].start = yuv_buffer; 
    iobj->proc.img_buf[0].length = yuv_len ; 
    iobj->proc.img_bufs_count = 1;

	PPM("BEFORE ICapture Process");
    status = (capture_status_t) iobj->lib.Process(iobj->lib_private, &iobj->proc);
    if( ICAPTURE_FAIL == status){
        LOGE("ICapture Process failed");
        goto fail_process;
    } else {
        PPM("ICapture process OK");
    }

	//SaveFile(NULL, (char*)"yuv", yuv_buffer, yuv_len); 
	//SaveFile(NULL, (char*)"mknote", ancillary_buffer, sizeof(*mk_note));
		
    ipp_ee_q   =   iobj->proc.eenf.ee_q,
    ipp_ew_ts  =   iobj->proc.eenf.ew_ts,
    ipp_es_ts  =   iobj->proc.eenf.es_ts, 
    ipp_luma_nf =  iobj->proc.eenf.luma_nf,
    ipp_chroma_nf= iobj->proc.eenf.chroma_nf;

	iobj->proc.out_img_h &= 0xFFFFFFF8;

    LOGD("iobj->proc.out_img_w = %d = 0x%x iobj->proc.out_img_h=%u = 0x%x", (int)iobj->proc.out_img_w,(int)iobj->proc.out_img_w, (int)iobj->proc.out_img_h,(int)iobj->proc.out_img_h);
    
#ifdef HARDWARE_OMX
#if  JPEG    

    jpegSize = image_width*image_height*2;
	//jpegSize = image_width*image_height + 13000;

    mJPEGPictureHeap = new MemoryHeapBase(jpegSize + 0x20 + 256);
    base = (unsigned long)mJPEGPictureHeap->getBase();
    /*Align buffer to 32 byte boundary */
    while ((base & 0x1f) != 0)
    {
        base++;
    }

    /* Buffer pointer shifted to avoid DSP cache issues */
    base += 128;
    offset = base - (unsigned long)mJPEGPictureHeap->getBase();
    outBuffer = (uint8_t *) base;

#if VPP

#if RESIZER
    
    if( (image_width != iobj->proc.out_img_w) || (image_height != iobj->proc.out_img_h)){
        
        LOGI("Process VPP ( %d x %d -> %d x %d ) - starting", iobj->proc.out_img_w, iobj->proc.out_img_h, (int) image_width, (int) image_height);

		err = scale_process(yuv_buffer, iobj->proc.out_img_w, iobj->proc.out_img_h, outBuffer, image_width, image_height);

        void *tmpBuffer = outBuffer;
        outBuffer = yuv_buffer;
        yuv_buffer = (unsigned char *)tmpBuffer;
        
        sp<MemoryHeapBase> tmpHeap = mJPEGPictureHeap;
        mJPEGPictureHeap = mPictureHeap;
        mPictureHeap = tmpHeap;
        
        int tmpSize = jpegSize;
        jpegSize = yuv_len;
        yuv_len = tmpSize;
        
        mPictureBuffer.clear();
        mPictureBuffer = new MemoryBase(mPictureHeap, offset, yuv_len);

		if( err) {
            LOGE("Process Resizer VPP - failed");
        } else {
	    	LOGE("Process Resizer VPP - OK");
        }
    } else {
        jpeg_offset = offset;
    }
#else
    image_width = (int)iobj->proc.out_img_w;
    image_height =(int)iobj->proc.out_img_h;		
	jpeg_offset = offset;
#endif //RESIZER	
#else	
    image_width = (int)iobj->proc.out_img_w;
    image_height =(int)iobj->proc.out_img_h;
	jpeg_offset = offset;
#endif //VPP

#endif //JPEG
#endif //HARDWARE_OMX

#ifdef CAMERA_ALGO

    gettimeofday(&algo_before, 0);
    camAlgos->removeRedeye( (uint8_t *) yuv_buffer, image_width, image_height);
    gettimeofday(&algo_after, 0);
    delay = (algo_after.tv_sec - algo_before.tv_sec)*1000;
    delay += (algo_after.tv_usec - algo_before.tv_usec)/1000;
    LOGD("Red Eye Removal Completed in %d [msec.]", delay);

    gettimeofday(&algo_before, 0);
    camAlgos->deBlur( (uint8_t *) yuv_buffer, (uint8_t *) mOverlay->getBufferAddress( (void *) lastOverlayIndex), image_width, image_height, preview_width, preview_height);
    gettimeofday(&algo_after, 0);
    delay = (algo_after.tv_sec - algo_before.tv_sec)*1000;
    delay += (algo_after.tv_usec - algo_before.tv_usec)/1000;
    LOGD("Antishaking Completed in %d [msec.]", delay);

#endif

    PPM("IMAGE CAPTURED");
    mRawPictureCallback(mPictureBuffer,mPictureCallbackCookie);
    PPM("RAW CALLBACK CALLED");
    
#if OPEN_CLOSE_WORKAROUND
    close(camera_device);
    camera_device = open(VIDEO_DEVICE, O_RDWR);
    if (camera_device < 0) {
        LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!", strerror(errno) );
    }
    PPM("CLOSED AND REOPENED CAMERA");
#endif	

#ifdef HARDWARE_OMX
#if VPP
#if VPP_THREAD
	LOGD("SENDING MESSAGE TO VPP THREAD \n");
	vpp_buffer =  yuv_buffer;
	vppMessage[0] = VPP_THREAD_PROCESS;
	vppMessage[1] = image_width;
	vppMessage[2] = image_height;			

	write(vppPipe[1],&vppMessage,sizeof(vppMessage));	
#else
	//snapshot_buffer_index = mLastOverlayBufferIndex;
	snapshot_buffer = mOverlay->getBufferAddress( (void*)snapshot_buffer_index );

    PPM("BEFORE SCALED DOWN RAW IMAGE TO PREVIEW SIZE"); 
	status = scale_process(yuv_buffer, image_width, image_height,
                         snapshot_buffer, preview_width, preview_height);
	if( status ) LOGE("scale_process() failed");
	else LOGD("scale_process() OK");
	 
	PPM("SCALED DOWN RAW IMAGE TO PREVIEW");

    err = mOverlay->queueBuffer((void*)snapshot_buffer_index);
    if (err) {
        LOGD("qbuf failed. May be bcos stream was not turned on yet. So try again");
    } else   {
        nOverlayBuffersQueued++;
    }  

    if (nOverlayBuffersQueued > 1) {
        mOverlay->dequeueBuffer(&overlaybuffer);
        nOverlayBuffersQueued--;
    }
	
	PPM("DISPLAYED RAW IMAGE ON SCREEN");

#endif //VPP_THREAD

#endif //VPP
#endif //HARDWARE_OMX

	//SaveFile(NULL, (char*)"yuv", yuv_buffer, yuv_len); 

#ifdef IMAGE_PROCESSING_PIPELINE  	
#if 1
	if(mippMode ==-1 ){
		mippMode=IPP_EdgeEnhancement_Mode;
	}

#else 	
	if(mippMode ==-1){
		mippMode=IPP_CromaSupression_Mode;
	}		
	if(mippMode == IPP_CromaSupression_Mode){
		mippMode=IPP_EdgeEnhancement_Mode;
	}
	else if(mippMode == IPP_EdgeEnhancement_Mode){
		mippMode=IPP_CromaSupression_Mode;
	}	

#endif

	LOGD("IPPmode=%d",mippMode);
	if(mippMode == IPP_CromaSupression_Mode){
		LOGD("IPP_CromaSupression_Mode");
	}
	else if(mippMode == IPP_EdgeEnhancement_Mode){
		LOGD("IPP_EdgeEnhancement_Mode");
	}
	else if(mippMode == IPP_Disabled_Mode){
		LOGD("IPP_Disabled_Mode");
	}

	if(mippMode){

		if(mippMode != IPP_CromaSupression_Mode && mippMode != IPP_EdgeEnhancement_Mode){
			LOGE("ERROR ippMode unsupported");
			return -1;
		}		
		PPM("Before init IPP");

		err = InitIPP(image_width,image_height);
		if( err ) {
			LOGE("ERROR InitIPP() failed");	
			return -1;	   
		}
		PPM("After IPP Init");
		err = PopulateArgsIPP(image_width,image_height);
		if( err ) {
			LOGE("ERROR PopulateArgsIPP() failed");		   
			return -1;
		} 
		PPM("BEFORE IPP Process Buffer");
		
		LOGD("Calling ProcessBufferIPP(buffer=%p , len=0x%x)", yuv_buffer, yuv_len);
		err = ProcessBufferIPP(yuv_buffer, yuv_len,
				        ipp_ee_q,
				        ipp_ew_ts,
				        ipp_es_ts, 
				        ipp_luma_nf,
				        ipp_chroma_nf);
		if( err ) {
			LOGE("ERROR ProcessBufferIPP() failed");		   
			return -1;
		}
		PPM("AFTER IPP Process Buffer");

		if(pIPP.hIPP != NULL){
			err = DeInitIPP();
			if( err ){
				LOGE("ERROR DeInitIPP() failed");
				return -1;
			} 
			pIPP.hIPP = NULL;
		}

	PPM("AFTER IPP Deinit");
   	if(!(pIPP.ippconfig.isINPLACE)){ 
		yuv_buffer = pIPP.pIppOutputBuffer;
	}
	 
	#if !( IPP_YUV422P )
		yuv_len=  ((image_width * image_height *3)/2);
	#else
        mippMode = 0;
    #endif
		
	}
	//SaveFile(NULL, (char*)"yuv", yuv_buffer, yuv_len); 
    
#endif

#ifdef IMAGE_PROCESSING_PIPELINE_MMS

    err = InitIPPMMSDefault(image_width, image_height);
    if( err ) {
        LOGE("InitIPPMMS() failed");
        goto fail_init;
    }
    LOGD("InitIPPMMS() OK");

    LOGD("PPM: BEFORE IPP");    

    LOGD("Calling ProcessBufferIPPMMS(buffer=%p , len=0x%x)", yuv_buffer, yuv_len);
   	PPM("Before IPP Process buffer");
    err = ProcessBufferIPPMMS(yuv_buffer);
   	PPM("IPP Process buffer done");
    
    err = DeInitIPPMMS();
    if( err )
        LOGE("DeInitIPPMMS() failed");
    else
        LOGD("DeInitIPPMMS() OK");
        
#endif

	//SaveFile(NULL, (char*)"yuv", yuv_buffer, yuv_len); 

#ifdef HARDWARE_OMX
#if JPEG
    err = 0;    
    
	PPM("BEFORE JPEG Encode Image");	
	LOGE(" outbuffer = 0x%x, jpegSize = %d, yuv_buffer = 0x%x, yuv_len = %d, image_width = %d, image_height = %d, quality = %d, mippMode =%d", outBuffer , jpegSize, yuv_buffer, yuv_len, image_width, image_height, quality,mippMode);	     
    if (!( jpegEncoder->encodeImage((uint8_t *)outBuffer , jpegSize, yuv_buffer, yuv_len,
                                 image_width, image_height, quality,mippMode)))
    {        
        err = -1;
        LOGE("JPEG Encoding failed");
    }

    PPM("AFTER JPEG Encode Image");
    mJPEGPictureMemBase = new MemoryBase(mJPEGPictureHeap, jpeg_offset, jpegEncoder->jpegSize);
#endif

    if(mJpegPictureCallback) {

#if JPEG
		mJpegPictureCallback(mJPEGPictureMemBase, mPictureCallbackCookie); 
#else
		mJpegPictureCallback(NULL, mPictureCallbackCookie); 
#endif

    }
	PPM("AFTER SAVING JPEG FILE");

#if JPEG 	
    LOGD("jpegEncoder->jpegSize=%d jpegSize=%d",jpegEncoder->jpegSize,jpegSize);   

    mJPEGPictureMemBase.clear();		
    mJPEGPictureHeap.clear();
#endif
#endif

#ifdef FOCUS_RECT
    focus_rect_set = 0;
#endif

    mPictureBuffer.clear();
    mPictureHeap.clear();

#if VPP_THREAD
	LOGD("CameraHal thread before waiting increment in semaphore\n");
	sem_wait(&mIppVppSem);
	LOGD("CameraHal thread after waiting increment in semaphore\n");
#endif

	LOG_FUNCTION_NAME_EXIT
	return 0;

fail_process:
fail_config:

	iobj->lib.Delete(iobj->lib_private);

fail_create:
fail_icapture:

	free(mk_note);

fail_mk_note:

fail_init:
fail_iobj:
fail_open:
fail_deinit_preview:
fail_deinit_3afw:

    return -1;   
}
#endif

#if VPP_THREAD
void CameraHal::vppThread(){

	LOG_FUNCTION_NAME
	
	void* snapshot_buffer;
	void* snapshot_buffer_temp;
	int status;
    int image_width, image_height;
    int preview_width, preview_height;
	overlay_buffer_t overlaybuffer;
	fd_set descriptorSet;	
	int max_fd;
	int error;
	unsigned int vppMessage [3];
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

			read(vppPipe[0], &vppMessage, sizeof(vppMessage));
			
			if(vppMessage[0] == VPP_THREAD_PROCESS){

				LOGD("VPP_THREAD_PROCESS_RECEIVED\n");		
				
				image_width = vppMessage[1];
				image_height = vppMessage[2];	

				mParameters.getPreviewSize(&preview_width, &preview_height);
				
                LOGD("lastOverlayBufferDQ=%d",lastOverlayBufferDQ);

				snapshot_buffer = mOverlay->getBufferAddress( (void*)(lastOverlayBufferDQ) );
	
				PPM("BEFORE SCALED DOWN RAW IMAGE TO PREVIEW SIZE"); 
				status = scale_process(yuv_buffer, image_width, image_height,
						             snapshot_buffer, preview_width, preview_height);
				if( status ) LOGE("scale_process() failed");
				else LOGD("scale_process() OK");
				 
				PPM("SCALED DOWN RAW IMAGE TO PREVIEW");						

				error = mOverlay->queueBuffer((void*)(lastOverlayBufferDQ));
                if (error){
    				LOGE("mOverlay->queueBuffer() failed!!!!");
                }
                else{
                    buffers_queued_to_dss[lastOverlayBufferDQ]=1;
                    nOverlayBuffersQueued++;
                }

                error = mOverlay->dequeueBuffer(&overlaybuffer);
                if(error){
                    LOGE("mOverlay->dequeueBuffer() failed!!!!");
                }
                else{
                    nOverlayBuffersQueued--;
                    buffers_queued_to_dss[(int)overlaybuffer] = 0;
                    lastOverlayBufferDQ = (int)overlaybuffer;	
	            }				

				LOGD("AFTER QUEUE OVERLAY BUFFERS TO DISPLAY SNAPSHOT!!");
								
				sem_post(&mIppVppSem);
			}

			else if( vppMessage[0] == VPP_THREAD_EXIT ){
				LOGD("VPP_THREAD_EXIT_RECEIVED\n");
				break;
			}
		}
	}

	LOG_FUNCTION_NAME_EXIT
}
#endif

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
        if( NULL == iobj->lib.Process){
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
#if VPP
    res = scale_init();
    if( res ) {
        LOGE("scale_init() failed");
        scale_deinit();
    }
    LOGD("scale_init() OK");
#endif

#ifdef IMAGE_PROCESSING_PIPELINE
	pIPP.hIPP=NULL;
#endif

	mippMode=0;

#if JPEG
    jpegEncoder = new JpegEncoder;
#endif
#endif

    LOG_FUNCTION_NAME_EXIT
    return res;

fail_jpeg_buffer:
fail_yuv_buffer:
fail_init:

#ifdef HARDWARE_OMX
#if JPEG
    delete jpegEncoder;
#endif
#endif    

#ifdef HARDWARE_OMX

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
#if VPP
    err = scale_deinit();
    if( err ) LOGE("scale_deinit() failed");
    else LOGD("scale_deinit() OK");
#endif

#ifdef IMAGE_PROCESSING_PIPELINE 
LOGD("IPP Evaluate IPP pIPP.hIPP=%p",pIPP.hIPP);
	if(pIPP.hIPP != NULL){
		err = DeInitIPP();
		if( err != 0){
			LOGE("ERROR DeInitIPP() failed");
		} 
	}
#endif    

#if JPEG
    delete jpegEncoder;
#endif
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
        
        int buffer_count = mOverlay->getBufferCount();
        for(int i =0; i < buffer_count ; i++){
            buffers_queued_to_dss[i] = 0;
        }

        mOverlay->destroy();
        nOverlayBuffersQueued = 0;
    }

    mOverlay = overlay;
    if (mOverlay == NULL)
    {
        LOGE("Trying to set overlay, but overlay is null!, line:%d",__LINE__);
        return NO_ERROR;
    }

    // Restart the preview (Only for Overlay Case)
    LOGD("Restart the preview ");
    startPreview(NULL,NULL);

	LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

status_t CameraHal::startPreview(preview_callback cb, void* user)
{
    LOG_FUNCTION_NAME

    if (mOverlay == NULL)
    {
        LOGE("WARNING: Trying to set overlay, but overlay is null!, line:%d",__LINE__);
        return NO_ERROR;
    }

    if (mPreviewRunning){
        LOGE("WARNING: Trying to startPreview but already started!, line:%d",__LINE__);
        return NO_ERROR;
    }

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

    for(i = 0; i < VIDEO_FRAME_COUNT_MAX; i++)
    {
        mVideoBufferUsing[i] = 0;
    }

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
#if USE_MEMCOPY_FOR_VIDEO_FRAME
        mVideoHeap = new MemoryHeapBase(mPreviewFrameSize * mVideoBufferCount);
#else
        mVideoHeap = new MemoryHeapBase(overlayfd,mPreviewFrameSize * mVideoBufferCount);
#endif
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
    //LOG_FUNCTION_NAME
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
#if USE_MEMCOPY_FOR_VIDEO_FRAME
    mVideoBufferUsing[index] = 0;
#else
    /* queue the buffer back to camera */
    while (ioctl(camera_device, VIDIOC_QBUF, &mfilledbuffer[index]) < 0) {
        LOGE("Recording VIDIOC_QBUF Failed.");
        if(++time >= 4)break;
    }
#endif
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

	gettimeofday(&take_before, NULL);
    Message msg;
    msg.command = PREVIEW_CAPTURE;
    msg.arg1    = (void*)shutter_cb;
    msg.arg2    = (void*)raw_cb;
    msg.arg3    = (void*)jpeg_cb;
    msg.arg4    = user;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

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
	int error;
    Message msg;

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
    LOGD("PreviewSize by App %d x %d", w, h);

    params.getPictureSize(&w, &h);
    if (!validateSize(w, h)) {
        LOGE("Picture size not supported");
        return -1;
    }
    LOGD("Picture Size by App %d x %d", w, h);
    
    framerate = params.getPreviewFrameRate();
    LOGD("FRAMERATE %d", framerate);

	mMMSApp= params.getInt("MMS_APP");	
	if (mMMSApp != 10){
		mMMSApp=0;
	}
	LOGD("mMMSApp =%d",mMMSApp);

    mParameters = params;

#ifdef IMAGE_PROCESSING_PIPELINE	
	mippMode=mParameters.getInt("ippMode");
	LOGD("mippMode=%d",mippMode);
#endif

	mParameters.getPictureSize(&w, &h);
	LOGD("Picture Size by CamHal %d x %d", w, h);
	mParameters.getPreviewSize(&w, &h);
	LOGD("Preview Size by CamHal %d x %d", w, h);

    quality = params.getInt("jpeg-quality");
    if ( ( quality < 0 ) || (quality > 100) ){
        quality = 100;
    } 

    zoom = mParameters.getInt("zoom");
    
    if( mzoom != zoom){
        Message msg;
        mzoom = zoom;
        msg.command = ZOOM_UPDATE;                     
        previewThreadCommandQ.put(&msg);         
    }

#ifdef FW3A

    if ( NULL != fobj ){
        iso = mParameters.getInt("iso");
        af = mParameters.getInt("af");
        mcapture_mode = mParameters.getInt("mode");        
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

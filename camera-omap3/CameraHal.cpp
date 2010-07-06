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

#define LOG_TAG "CameraHal"

#include "CameraHal.h"
#include <poll.h>
#include "zoom_step.inc"

#include <math.h>

#include <cutils/properties.h>
#define UNLIKELY( exp ) (__builtin_expect( (exp) != 0, false ))
static int mDebugFps = 0;

#define RES_720P    1280

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

/* Defined in liboverlay */
typedef struct {
    int fd;
    size_t length;
    uint32_t offset;
    void *ptr;
} mapping_data_t;

int CameraHal::camera_device = 0;
wp<CameraHardwareInterface> CameraHal::singleton;
const char CameraHal::supportedPictureSizes [] = "3280x2464,2560x2048,2048x1536,1600x1200,1280x1024,1152x964,640x480,320x240";
const char CameraHal::supportedPreviewSizes [] = "1280x720,800x480,720x576,720x480,768x576,640x480,320x240,352x288,176x144,128x96";
const char CameraHal::supportedFPS [] = "33,30,25,24,20,15,10";
const char CameraHal::supprotedThumbnailSizes []= "80x60";
const char CameraHal::PARAMS_DELIMITER []= ",";

int camerahal_strcat(char *dst, const char *src, size_t size)
{
    size_t actual_size;

    actual_size = strlcat(dst, src, size);
    if(actual_size > size)
    {
        LOGE("Unexpected truncation from camerahal_strcat dst=%s src=%s", dst, src);
        return actual_size;
    }

    return 0;
}

CameraHal::CameraHal()
			:mParameters(),
			mOverlay(NULL),
			mPreviewRunning(0),
			mRecordingFrameSize(0),
			mVideoBufferCount(0),
			nOverlayBuffersQueued(0),
			nCameraBuffersQueued(0),
			mfirstTime(0),
			pictureNumber(0),
#ifdef FW3A
			fobj(NULL),
#endif
			file_index(0),
			mflash(2),
			mcapture_mode(1),
			mcaf(0),
			j(0)
{
#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
    gettimeofday(&ppm_start, NULL);
#endif

    isStart_FW3A = false;
    isStart_FW3A_AF = false;
    isStart_FW3A_CAF = false;
    isStart_FW3A_AEWB = false;
    isStart_VPP = false;
    isStart_JPEG = false;
    mPictureHeap = NULL;
    mIPPInitAlgoState = false;
    mIPPToEnable = false;
    mRecordEnabled = 0;
    mNotifyCb = 0;
    mDataCb = 0;
    mDataCbTimestamp = 0;
    mCallbackCookie = 0;
    mMsgEnabled = 0 ;
    mFalsePreview = false;  //Eclair HAL
    mZoomSpeed = 1;
    mZoomTargetIdx = 0;
    mZoomCurrentIdx = 0;
    mReturnZoomStatus = false;
    rotation = 0;
#ifdef HARDWARE_OMX
    gpsLocation = NULL;
#endif
    mShutterEnable = true;
    mCAFafterPreview = false;
    ancillary_len = 8092;

#ifdef IMAGE_PROCESSING_PIPELINE

    pIPP.hIPP = NULL;

#endif

    int i = 0;
    for(i = 0; i < VIDEO_FRAME_COUNT_MAX; i++)
    {
        mVideoBuffer[i] = 0;
        buffers_queued_to_dss[i] = 0;
        buffers_queued_to_dss_after_stream_off[i] = 0;
    }

    CameraCreate();

    initDefaultParameters();
     
    /* Avoiding duplicate call of cameraconfigure(). It is now called in previewstart() */     
    //CameraConfigure();

    allocatePictureBuffer(PICTURE_WIDTH, PICTURE_HEIGHT);

#ifdef FW3A
    FW3A_Create();
    FW3A_Init();
#endif

    ICaptureCreate();

    mPreviewThread = new PreviewThread(this);
    mPreviewThread->run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);

    if( pipe(procPipe) != 0 ){
        LOGE("Failed creating pipe");
    }

    if( pipe(shutterPipe) != 0 ){
        LOGE("Failed creating pipe");
    }
	
    if( pipe(rawPipe) != 0 ){
        LOGE("Failed creating pipe");
    }

    if( pipe(snapshotPipe) != 0 ){
        LOGE("Failed creating pipe");
    }

    if( pipe(snapshotReadyPipe) != 0 ){
        LOGE("Failed creating pipe");
    }

    mPROCThread = new PROCThread(this);
    mPROCThread->run("CameraPROCThread", PRIORITY_URGENT_DISPLAY);
    LOGD("STARTING PROC THREAD \n");
    
    mShutterThread = new ShutterThread(this);
    mShutterThread->run("CameraShutterThread", PRIORITY_URGENT_DISPLAY);
    LOGD("STARTING Shutter THREAD \n");

    mRawThread = new RawThread(this);
    mRawThread->run("CameraRawThread", PRIORITY_URGENT_DISPLAY);
    LOGD("STARTING Raw THREAD \n");

    mSnapshotThread = new SnapshotThread(this);
    mSnapshotThread->run("CameraSnapshotThread", PRIORITY_URGENT_DISPLAY);
    LOGD("STARTING Snapshot THREAD \n");

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.image.showfps", value, "0");
    mDebugFps = atoi(value);
    LOGD_IF(mDebugFps, "showfps enabled");


}

void CameraHal::initDefaultParameters()
{
    CameraParameters p;
    char tmpBuffer[PARAM_BUFFER], zoomStageBuffer[PARAM_BUFFER];
    unsigned int zoomStage;
 
    LOG_FUNCTION_NAME

    p.setPreviewSize(MIN_WIDTH, MIN_HEIGHT);
    p.setPreviewFrameRate(30);
    p.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV422I);

    p.setPictureSize(PICTURE_WIDTH, PICTURE_HEIGHT);
    p.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
    p.set(CameraParameters::KEY_JPEG_QUALITY, 100);

    //Eclair extended parameters

    p.set(CameraParameters::KEY_GPS_LATITUDE, "42.6976246");
    p.set(CameraParameters::KEY_GPS_LONGITUDE, "23.3222924");
    p.set(CameraParameters::KEY_GPS_ALTITUDE, "500");

    p.set(KEY_GPS_LATITUDE_REF, "N");
    p.set(KEY_GPS_LONGITUDE_REF, "E");
    p.set(KEY_GPS_ALTITUDE_REF, "0");
    p.set(KEY_GPS_VERSION, "2200");
    p.set(KEY_GPS_MAPDATUM, "WGS-84");
    p.set(CameraParameters::KEY_GPS_TIMESTAMP, "834720834");

    memset(tmpBuffer, '\0', PARAM_BUFFER);
    for ( int i = 0 ; i < ZOOM_STAGES ; i++ ) {
        zoomStage =  (unsigned int ) ( zoom_step[i]*ZOOM_SCALE );
        snprintf(zoomStageBuffer, PARAM_BUFFER, "%d", zoomStage);

        if(camerahal_strcat((char*) tmpBuffer, (const char*) zoomStageBuffer, PARAM_BUFFER)) return;
        if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    }
    p.set(CameraParameters::KEY_ZOOM_RATIOS, tmpBuffer);
    p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "true");
    p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "true");
    p.set(CameraParameters::KEY_MAX_ZOOM, ZOOM_STAGES);
    p.set(CameraParameters::KEY_ZOOM, 0);

    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, CameraHal::supportedPictureSizes);
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, CameraParameters::PIXEL_FORMAT_JPEG);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, CameraHal::supportedPreviewSizes);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, CameraParameters::PIXEL_FORMAT_YUV422I);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, CameraHal::supportedFPS);
    p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, CameraHal::supprotedThumbnailSizes);

    memset(tmpBuffer, '\0', PARAM_BUFFER);
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_AUTO, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_INCANDESCENT, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_FLUORESCENT, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_DAYLIGHT, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_SHADE, PARAM_BUFFER)) return;
    p.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, tmpBuffer);
    p.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);

    memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_NONE, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_MONO, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_NEGATIVE, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_SOLARIZE,  PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_SEPIA, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_WHITEBOARD,  PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_BLACKBOARD, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) EFFECT_COOL, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) EFFECT_EMBOSS, PARAM_BUFFER)) return;
    p.set(CameraParameters::KEY_SUPPORTED_EFFECTS, tmpBuffer);
    p.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NONE);

    memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_AUTO, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_PORTRAIT, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_LANDSCAPE, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_SPORTS, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_NIGHT_PORTRAIT, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_FIREWORKS, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_NIGHT, PARAM_BUFFER)) return;
    p.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, tmpBuffer);
    p.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_AUTO);

    memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::FOCUS_MODE_AUTO, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::FOCUS_MODE_INFINITY, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::FOCUS_MODE_MACRO, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::FOCUS_MODE_FIXED, PARAM_BUFFER)) return;
    p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, tmpBuffer);
    p.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_AUTO);

    memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::ANTIBANDING_50HZ, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::ANTIBANDING_60HZ, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::ANTIBANDING_OFF, PARAM_BUFFER)) return;
    p.set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, tmpBuffer);
    p.set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);

    memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
    if(camerahal_strcat( (char*) tmpBuffer, (const char*) CameraParameters::FLASH_MODE_OFF, PARAM_BUFFER)) return;
    p.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, tmpBuffer);
    p.set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_OFF);
    //
    p.set(CameraParameters::KEY_ROTATION, 0);
    p.set(KEY_ROTATION_TYPE, ROTATION_PHYSICAL);

    if (setParameters(p) != NO_ERROR) {
        LOGE("Failed to set default parameters?!");
    }

    LOG_FUNCTION_NAME_EXIT

}


CameraHal::~CameraHal()
{
    int err = 0;
	int procMessage [1];
	sp<PROCThread> procThread;
	sp<RawThread> rawThread;
	sp<ShutterThread> shutterThread;
	sp<SnapshotThread> snapshotThread;
	  
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

    procMessage[0] = PROC_THREAD_EXIT;
    write(procPipe[1], procMessage, sizeof(unsigned int));

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        procThread = mPROCThread;
    }

    // don't hold the lock while waiting for the thread to quit
    if (procThread != 0) {
        procThread->requestExitAndWait();
    }

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        mPROCThread.clear();
        close(procPipe[0]);
        close(procPipe[1]);
    }

    procMessage[0] = SHUTTER_THREAD_EXIT;
    write(shutterPipe[1], procMessage, sizeof(unsigned int));

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        shutterThread = mShutterThread;
    }

    // don't hold the lock while waiting for the thread to quit
    if (shutterThread != 0) {
        shutterThread->requestExitAndWait();
    }

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        mShutterThread.clear();
        close(shutterPipe[0]);
        close(shutterPipe[1]);
    }
    
    procMessage[0] = RAW_THREAD_EXIT;
    write(rawPipe[1], procMessage, sizeof(unsigned int));

	{ // scope for the lock
        Mutex::Autolock lock(mLock);
        rawThread = mRawThread;
    }

    // don't hold the lock while waiting for the thread to quit
    if (rawThread != 0) {
        rawThread->requestExitAndWait();
    }

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        mRawThread.clear();
        close(rawPipe[0]);
        close(rawPipe[1]);
    }

    procMessage[0] = SNAPSHOT_THREAD_EXIT;
    write(snapshotPipe[1], procMessage, sizeof(unsigned int));

    {
        Mutex::Autolock lock(mLock);
        snapshotThread = mSnapshotThread;
    }

    if (snapshotThread != 0 ) {
        snapshotThread->requestExitAndWait();
    }

    {
        Mutex::Autolock lock(mLock);
        mSnapshotThread.clear();
        close(snapshotPipe[0]);
        close(snapshotPipe[1]);
        close(snapshotReadyPipe[0]);
        close(snapshotReadyPipe[1]);
    }

    ICaptureDestroy();

#ifdef FW3A
    FW3A_Release();
    FW3A_Destroy();
#endif

    CameraDestroy(true);


#ifdef IMAGE_PROCESSING_PIPELINE

    if(pIPP.hIPP != NULL){
        err = DeInitIPP(mippMode);
        if( err )
            LOGE("ERROR DeInitIPP() failed");

        pIPP.hIPP = NULL;
    }

#endif

    if ( mOverlay.get() != NULL )
    {
        LOGD("Destroying current overlay");
        mOverlay->destroy();
    }

    free((void *) ( ((unsigned int) mYuvBuffer) - mPictureOffset) );
    singleton.clear();

    LOGD("<<< Release");
}

void CameraHal::previewThread()
{
    Message msg;
    int parm;
    bool  shouldLive = true;
    bool has_message;
    int err; 
    struct pollfd pfd[2];

    LOG_FUNCTION_NAME
    
    while(shouldLive) {
    
        has_message = false;

        if( mPreviewRunning )
        {

            pfd[0].fd = previewThreadCommandQ.getInFd();
            pfd[0].events = POLLIN;
            pfd[1].fd = camera_device;
            pfd[1].events = POLLIN;

            if (poll(pfd, 2, 1000) == 0) {
                continue;
            }

            if (pfd[0].revents & POLLIN) {
                previewThreadCommandQ.get(&msg);
                has_message = true;
            }

            if (pfd[1].revents & POLLIN) {
                nextPreview();
            }

#ifdef FW3A
            if (isStart_FW3A_AF) {
                err = fobj->cam_iface_2a->ReadSatus(fobj->cam_iface_2a->pPrivateHandle, &fobj->status_2a);
                if ((err == 0) && (AF_STATUS_RUNNING != fobj->status_2a.af.status)) {
#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
                    PPM("AF Completed in ",&focus_before);
#endif

                    if (FW3A_Stop_AF() < 0){
                        LOGE("ERROR FW3A_Stop_AF()");
                    }

                    bool focus_flag;
                    if ( fobj->status_2a.af.status == AF_STATUS_SUCCESS ) {
                        focus_flag = true;
                        LOGE("AF Success");
                    } else {
                        focus_flag = false;
                        LOGE("AF Fail");
                    }

                    if( msgTypeEnabled(CAMERA_MSG_FOCUS) )
                        mNotifyCb(CAMERA_MSG_FOCUS, focus_flag, 0, mCallbackCookie);
                }
            }
#endif

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

                    if( CameraCreate() < 0){
                        LOGE("ERROR CameraCreate()");
                        err = -1;
                    }

                    PPM("CONFIGURING CAMERA TO RESTART PREVIEW");
                    if (CameraConfigure() < 0){
                        LOGE("ERROR CameraConfigure()");
                        err = -1;
                    }
#ifdef FW3A
                    if (FW3A_Start() < 0){
                        LOGE("ERROR FW3A_Start()");
                        err = -1;
                    }
                    if (FW3A_SetSettings() < 0){
                        LOGE("ERROR FW3A_SetSettings()");
                        err = -1;
                    }
                    
#endif

                    if ( CorrectPreview() < 0 )
                        LOGE("Error during CorrectPreview()");

                    if (CameraStart() < 0){
                        LOGE("ERROR CameraStart()");
                        err = -1;
                    }

#ifdef FW3A
                    if ( mCAFafterPreview ) {
                        mCAFafterPreview = false;
                        if( FW3A_Start_CAF() < 0 )
                            LOGE("Error while starting CAF");
                    }
#endif

                    if(!mfirstTime){
                        PPM("Standby to first shot");
                        mfirstTime++;
                    }
                    else{
#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
                        PPM("Shot to Shot", &ppm_receiveCmdToTakePicture);
#endif
                    }
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

                    if( CameraDestroy(false) < 0){
                        LOGE("ERROR CameraDestroy()");
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

            case START_SMOOTH_ZOOM:

                parm = ( int ) msg.arg1;

                LOGD("Receive Command: START_SMOOTH_ZOOM %d", parm);

                if ( ( parm >= 0 ) && ( parm <= ZOOM_STAGES) ) {
                    mZoomTargetIdx = parm;
                    msg.command = PREVIEW_ACK;
                } else {
                    msg.command = PREVIEW_NACK;
                }

                previewThreadAckQ.put(&msg);

                break;

            case STOP_SMOOTH_ZOOM:

                LOGD("Receive Command: STOP_SMOOTH_ZOOM");
                mZoomTargetIdx = mZoomCurrentIdx;
                mReturnZoomStatus = true;
                msg.command = PREVIEW_ACK;

                previewThreadAckQ.put(&msg);

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

//Disable Autofocus in Eclair for now
#if 1 

#ifdef FW3A   

                   if (isStart_FW3A_CAF!= 0){
                        if( FW3A_Stop_CAF() < 0){
                            LOGE("ERROR FW3A_Stop_CAF();");
                            err = -1;
                        }
                   }

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

                    gettimeofday(&focus_before, NULL);

#endif

                    if (isStart_FW3A_AF == 0){
                        if( FW3A_Start_AF() < 0){
                            LOGE("ERROR FW3A_Start_AF()");
                            err = -1;
                         }
 
                    }
#endif

                    msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;

#else

                    if( msgTypeEnabled(CAMERA_MSG_FOCUS) )
                        mNotifyCb(CAMERA_MSG_FOCUS, true, 0, mCallbackCookie);

                    msg.command = PREVIEW_ACK;

#endif

                }
                LOGD("Receive Command: PREVIEW_AF_START %s", msg.command == PREVIEW_NACK ? "NACK" : "ACK"); 
                previewThreadAckQ.put(&msg);
            }
            break;
		
            case PREVIEW_CAF_START:
            {
                LOGD("Receive Command: PREVIEW_CAF_START");
                err=0;

                if( !mPreviewRunning ) {
                    msg.command = PREVIEW_ACK;
                    mCAFafterPreview = true;
                }
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
                    msg.command = PREVIEW_ACK;
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
                err = 0;

#ifdef DEBUG_LOG

                LOGD("ENTER OPTION PREVIEW_CAPTURE");

                PPM("RECEIVED COMMAND TO TAKE A PICTURE");

#endif

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
                gettimeofday(&ppm_receiveCmdToTakePicture, NULL);
#endif

                if( mPreviewRunning ) {

                    if( CameraStop() < 0){
                        LOGE("ERROR CameraStop()");
                        err = -1;
                    }

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

                   mPreviewRunning = false;

               }
#ifdef FW3A
               if( FW3A_GetSettings() < 0){
                   LOGE("ERROR FW3A_GetSettings()");
                   err = -1;
               }
#endif

#ifdef DEBUG_LOG

               PPM("STOPPED PREVIEW");

#endif

               if( ICapturePerform() < 0){
                   LOGE("ERROR ICapturePerform()");
                   err = -1;
               }

               if( err )
                   LOGE("Capture failed.");

               //restart the preview
#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
                gettimeofday(&ppm_restartPreview, NULL);
#endif

#ifdef DEBUG_LOG

               PPM("CONFIGURING CAMERA TO RESTART PREVIEW");

#endif

               if (CameraConfigure() < 0)
                   LOGE("ERROR CameraConfigure()");

#ifdef FW3A

               if (FW3A_Start() < 0)
                   LOGE("ERROR FW3A_Start()");

#endif

               if ( CorrectPreview() < 0 )
                   LOGE("Error during CorrectPreview()");

               if (CameraStart() < 0)
                   LOGE("ERROR CameraStart()");

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

               PPM("Capture mode switch", &ppm_restartPreview);
               PPM("Shot to Shot", &ppm_receiveCmdToTakePicture);

#endif

#ifdef ICAP_EXPERIMENTAL

               allocatePictureBuffer(iobj->cfg.sizeof_img_buf);

#else

               allocatePictureBuffer(PICTURE_WIDTH, PICTURE_HEIGHT);

#endif

               mPreviewRunning = true;

               msg.command = PREVIEW_ACK;
               previewThreadAckQ.put(&msg);
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

int CameraHal::CameraCreate()
{
    int err = 0;

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


int CameraHal::CameraDestroy(bool destroyOverlay)
{
    int err, buffer_count;

    LOG_FUNCTION_NAME

    close(camera_device);
    camera_device = -1;

    if ((mOverlay != NULL) && (destroyOverlay)) {
        buffer_count = mOverlay->getBufferCount();

        for ( int i = 0 ; i < buffer_count ; i++ )
        {
            // need to free buffers and heaps mapped using overlay fd before it is destroyed
            // otherwise we could create a resource leak
            // a segfault could occur if we try to free pointers after overlay is destroyed
            mPreviewBuffers[i].clear();
            mPreviewHeaps[i].clear();
            mVideoBuffer[i].clear();
            mVideoHeaps[i].clear();
            buffers_queued_to_dss[i] = 0;
            buffers_queued_to_dss_after_stream_off[i] = 0;
        }
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
      
        mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress((void*)i);
        if ( data == NULL ) {
            LOGE(" getBufferAddress returned NULL");
            goto fail_loop;
        }

        v4l2_cam_buffer[i].m.userptr = (unsigned long) data->ptr;

        strcpy((char *)v4l2_cam_buffer[i].m.userptr, "hello");
        if (strcmp((char *)v4l2_cam_buffer[i].m.userptr, "hello")) {
            LOGI("problem with buffer\n");
            goto fail_loop;
        }

        LOGD("User Buffer [%d].start = %p  length = %d\n", i,
             (void*)v4l2_cam_buffer[i].m.userptr, v4l2_cam_buffer[i].length);

        if (buffers_queued_to_dss[i] == 0)
        {
            if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[i]) < 0) {
                LOGE("CameraStart VIDIOC_QBUF Failed: %s", strerror(errno) );
                goto fail_loop;
            }else{
                nCameraBuffersQueued++;
            }
         }
         else LOGI("CameraStart::Could not queue buffer %d to Camera because it is being held by Overlay", i);

        // ensure we release any stale ref's to sp
        mPreviewBuffers[i].clear();
        mPreviewHeaps[i].clear();

        mPreviewHeaps[i] = new MemoryHeapBase(data->fd,mPreviewFrameSize, 0, data->offset);
        mPreviewBuffers[i] = new MemoryBase(mPreviewHeaps[i], 0, mPreviewFrameSize);
         
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    err = ioctl(camera_device, VIDIOC_STREAMON, &type);
    if ( err < 0) {
        LOGE("VIDIOC_STREAMON Failed");
        goto fail_loop;
    }

    if( ioctl(camera_device, VIDIOC_G_CROP, &mInitialCrop) < 0 ){
        LOGE("[%s]: ERROR VIDIOC_G_CROP failed", strerror(errno));
        return -1;
    }

    LOGE("Initial Crop: crop_top = %d, crop_left = %d, crop_width = %d, crop_height = %d", mInitialCrop.c.top, mInitialCrop.c.left, mInitialCrop.c.width, mInitialCrop.c.height);

    if ( mZoomTargetIdx != mZoomCurrentIdx ) {
        
        if( ZoomPerform(zoom_step[mZoomTargetIdx]) < 0 )
            LOGE("Error while applying zoom");   
        
        mZoomCurrentIdx = mZoomTargetIdx;
        mParameters.set("zoom", ((int) zoom_step[mZoomCurrentIdx]));
        mNotifyCb(CAMERA_MSG_ZOOM, ((int) zoom_step[mZoomCurrentIdx] - 1), 1, mCallbackCookie);
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

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME

#endif

    int ret;
    struct v4l2_requestbuffers creqbuf;
    struct v4l2_buffer cfilledbuffer;
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;

    while(nCameraBuffersQueued){
        nCameraBuffersQueued--;
    }

#ifdef DEBUG_LOG

	LOGD("Done dequeuing from Camera!");

#endif

    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) == -1) {
        LOGE("VIDIOC_STREAMOFF Failed");
        goto fail_streamoff;
    }

	//Force the zoom to be updated next time preview is started.
	mZoomCurrentIdx = 0;

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME_EXIT

#endif

    return 0;

fail_streamoff:

    return -1;
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
        LOGD("####### [%d] Frames, %f FPS", mFrameCount, mFps);
    }
 }

void CameraHal::nextPreview()
{
    struct v4l2_buffer cfilledbuffer;
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;
    int w, h, ret, nBuffers_queued_to_dss, dequeue_from_dss_failed;
    static int frame_count = 0;
    int zoom_inc, err;
    struct timeval lowLightTime;

    overlay_buffer_t overlaybuffer;// contains the index of the buffer dque
    int overlaybufferindex = -1; //contains the last buffer dque or -1 if dque failed
    int index;
    
    mParameters.getPreviewSize(&w, &h);

    //Zoom
    frame_count++;
    if( mZoomCurrentIdx != mZoomTargetIdx){

        if( mZoomCurrentIdx < mZoomTargetIdx)
            zoom_inc = 1;
        else
            zoom_inc = -1;

        if ( mZoomSpeed > 0 ){
            if( (frame_count % mZoomSpeed) == 0){
                mZoomCurrentIdx += zoom_inc;
            }
        } else {
            mZoomCurrentIdx = mZoomTargetIdx;
        }

        ZoomPerform(zoom_step[mZoomCurrentIdx]);
        mParameters.set("zoom", mZoomCurrentIdx);

        if( mZoomCurrentIdx == mZoomTargetIdx )
            mNotifyCb(CAMERA_MSG_ZOOM, mZoomCurrentIdx, 1, mCallbackCookie);
        else
            mNotifyCb(CAMERA_MSG_ZOOM, mZoomCurrentIdx, 0, mCallbackCookie);

    }

    if ( mReturnZoomStatus ) {
        mNotifyCb(CAMERA_MSG_ZOOM, mZoomCurrentIdx, 1, mCallbackCookie);
        mReturnZoomStatus = false;
    }

#ifdef FW3A

    //Low light notification
    if( ( 0 == fobj->settings_2a.ae.framerate ) && ( ( frame_count % 10) == 0 ) ) {

#if ( PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS ) && DEBUG_LOG

        gettimeofday(&lowLightTime, NULL);

#endif

        err = fobj->cam_iface_2a->ReadSatus(fobj->cam_iface_2a->pPrivateHandle, &fobj->status_2a);
        if (err == 0) {
            if (fobj->status_2a.ae.camera_shake == SHAKE_HIGH_RISK) {
                mParameters.set("low-light", "1");
            } else {
                mParameters.set("low-light", "0");
            }
         }

#if ( PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS ) && DEBUG_LOG

        PPM("Low-light delay", &lowLightTime);

#endif

    }

#endif

    /* De-queue the next avaliable buffer */
    if (nCameraBuffersQueued == 0)
    {
        LOGD("Camera has 0 buffers at the moment.");
    }
    
    if (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
        LOGE("VIDIOC_DQBUF Failed!!!");
        goto EXIT;
    }else{
        nCameraBuffersQueued--;
    }
    mCurrentTime[cfilledbuffer.index] = s2ns(cfilledbuffer.timestamp.tv_sec) + us2ns(cfilledbuffer.timestamp.tv_usec);

    //SaveFile(NULL, (char*)"yuv", (void *)cfilledbuffer.m.userptr, mPreviewFrameSize);

    if( msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) )
        mDataCb(CAMERA_MSG_PREVIEW_FRAME, mPreviewBuffers[cfilledbuffer.index], mCallbackCookie);

    nBuffers_queued_to_dss = mOverlay->queueBuffer((void*)cfilledbuffer.index);
    if (nBuffers_queued_to_dss < 0)
    {
        LOGE("nextPreview(): mOverlay->queueBuffer(%d) failed",cfilledbuffer.index);

        if (ioctl(camera_device, VIDIOC_QBUF, &cfilledbuffer) < 0) {
            LOGE("VIDIOC_QBUF Failed, line=%d",__LINE__);
        }
        else nCameraBuffersQueued++;
    }
    else
    {
        nOverlayBuffersQueued++;
        buffers_queued_to_dss[cfilledbuffer.index] = 1; //queued
        if (nBuffers_queued_to_dss != nOverlayBuffersQueued)
        {
            LOGD("nBuffers_queued_to_dss = %d, nOverlayBuffersQueued = %d", nBuffers_queued_to_dss, nOverlayBuffersQueued);
            LOGD("buffers in DSS \n %d %d %d  %d %d %d", buffers_queued_to_dss[0], buffers_queued_to_dss[1],
            buffers_queued_to_dss[2], buffers_queued_to_dss[3], buffers_queued_to_dss[4], buffers_queued_to_dss[5]);
            if (nBuffers_queued_to_dss != 0)
            {
                buffers_queued_to_dss_after_stream_off[cfilledbuffer.index] = 1;
                LOGD("buffers_queued_to_dss_after_stream_off[%d] = 1", cfilledbuffer.index);
            }
        }
    }
    
    if (nOverlayBuffersQueued >= NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE)
    {
        dequeue_from_dss_failed = mOverlay->dequeueBuffer(&overlaybuffer);
        if(dequeue_from_dss_failed){
            if (dequeue_from_dss_failed == -2)
            {
                LOGE("nextPreview(): mOverlay->dequeueBuffer() failed. Stream disabled.");
                //Queue all the buffers that were discarded by DSS upon STREAM OFF, back to camera.
                for(int k = 0; k < MAX_CAMERA_BUFFERS; k++)
                {
                    if ((buffers_queued_to_dss[k] == 1) &&
                         (buffers_queued_to_dss_after_stream_off[k] != 1))
                    {
                        buffers_queued_to_dss[k] = 0;
                        nOverlayBuffersQueued--;
                        if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[k]) < 0) {
                            LOGE("VIDIOC_QBUF Failed, line=%d. Buffer lost forever.",__LINE__);
                        }else{
                            nCameraBuffersQueued++;
                            LOGD("Reclaiming the buffer [%d] from Overlay", k);
                        }
                    }
                    buffers_queued_to_dss_after_stream_off[k] = 0;
                }
            }
        }
        else{
            overlaybufferindex = (int)overlaybuffer;
            nOverlayBuffersQueued--;
            buffers_queued_to_dss[(int)overlaybuffer] = 0;
            lastOverlayBufferDQ = (int)overlaybuffer;	
        }
    }

    mRecordingLock.lock();

    if(mRecordEnabled)
    {
        if(overlaybufferindex != -1) // dequeued a valid buffer from overlay
        {
            if (nCameraBuffersQueued == 0)
            {
                /* Drop the frame. Camera is starving. */
                if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[(int)overlaybuffer]) < 0) {
                    LOGE("VIDIOC_QBUF Failed, line=%d",__LINE__);
                }
                else nCameraBuffersQueued++;
                LOGD("Didnt queue this buffer to VE.");
                if (mPrevTime < mCurrentTime[(int)overlaybuffer])
                    mPrevTime = mCurrentTime[(int)overlaybuffer];
                else
                    mPrevTime += frameInterval;
            }
            else
            {
                buffers_queued_to_ve[(int)overlaybuffer] = 1;
                if (mPrevTime > mCurrentTime[(int)overlaybuffer])
                {
                    //LOGW("Had to adjust the timestamp. Clock went back in time. mCurrentTime = %lld, mPrevTime = %llu", mCurrentTime[(int)overlaybuffer], mPrevTime);
                    mCurrentTime[(int)overlaybuffer] = mPrevTime + frameInterval;
                }
                mDataCbTimestamp(mCurrentTime[(int)overlaybuffer], CAMERA_MSG_VIDEO_FRAME, mVideoBuffer[(int)overlaybuffer], mCallbackCookie);
                mPrevTime = mCurrentTime[(int)overlaybuffer];
            }
        }
    } 
    else 
    {
        if (overlaybufferindex != -1) {	// dequeued a valid buffer from overlay	
            if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[(int)overlaybuffer]) < 0) {
                LOGE("VIDIOC_QBUF Failed. line=%d",__LINE__);
            }else{
                nCameraBuffersQueued++;
            }
        }
    }

    mRecordingLock.unlock();


    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
    }

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
    unsigned long base, offset, jpeg_offset;
    int image_width, image_height;
    struct manual_parameters  manual_config;
    unsigned int procMessage[PROC_THREAD_NUM_ARGS],
            shutterMessage[SHUTTER_THREAD_NUM_ARGS],
            rawMessage[RAW_THREAD_NUM_ARGS];
    int pixelFormat;
    unsigned short EdgeEnhancementStrength, WeakEdgeThreshold, StrongEdgeThreshold,
                   LowFreqLumaNoiseFilterStrength, MidFreqLumaNoiseFilterStrength, HighFreqLumaNoiseFilterStrength,
                   LowFreqCbNoiseFilterStrength, MidFreqCbNoiseFilterStrength, HighFreqCbNoiseFilterStrength,
                   LowFreqCrNoiseFilterStrength, MidFreqCrNoiseFilterStrength, HighFreqCrNoiseFilterStrength,
                   shadingVertParam1, shadingVertParam2, shadingHorzParam1, shadingHorzParam2, shadingGainScale,
                   shadingGainOffset, shadingGainMaxValue, ratioDownsampleCbCr;
    exif_buffer *exif_buf;

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME

    PPM("START OF ICapturePerform");

#endif

    mParameters.getPictureSize(&image_width, &image_height);

#ifdef DEBUG_LOG

    LOGD("ICapturePerform image_width=%d image_height=%d",image_width,image_height);

#endif

    memset(&manual_config, 0 ,sizeof(manual_config));

#ifdef DEBUG_LOG

	LOGD("ICapturePerform beforeReadStatus");

#endif

    err = fobj->cam_iface_2a->ReadSatus(fobj->cam_iface_2a->pPrivateHandle, &fobj->status_2a);

#ifdef DEBUG_LOG

    LOGD("shutter_cap = %d ; again_cap = %d ; awb_index = %d; %d %d %d %d\n",
        (int)fobj->status_2a.ae.shutter_cap, (int)fobj->status_2a.ae.again_cap,
        (int)fobj->status_2a.awb.awb_index, (int)fobj->status_2a.awb.gain_Gr,
        (int)fobj->status_2a.awb.gain_R, (int)fobj->status_2a.awb.gain_B,
        (int)fobj->status_2a.awb.gain_Gb);

#endif

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


#ifdef DEBUG_LOG

    PPM("SETUP SOME 3A STUFF");

#endif

    iobj->cfg.lsc_type      = LSC_UPSAMPLED_BY_SOFTWARE;
    iobj->cfg.mknote        = ancillary_buffer;
    iobj->cfg.manual        = &manual_config;
    iobj->cfg.priv          = this;
    iobj->cfg.cb_write_h3a  = NULL;//onSaveH3A;
    iobj->cfg.cb_write_lsc  = NULL;//onSaveLSC;
    iobj->cfg.cb_write_raw  = NULL;//onSaveRAW;
    iobj->cfg.cb_picture_done = onSnapshot;
    iobj->cfg.preview_rect.top = fobj->status_2a.preview.top;
    iobj->cfg.preview_rect.left = fobj->status_2a.preview.left;
    iobj->cfg.preview_rect.width = fobj->status_2a.preview.width;
    iobj->cfg.preview_rect.height = fobj->status_2a.preview.height;
    iobj->cfg.capture_format = CAPTURE_FORMAT_UYVY;
    manual_config.pre_flash = 0;

    if(mcapture_mode == 1)
    {
        iobj->cfg.capture_mode  =  CAPTURE_MODE_HI_PERFORMANCE;
        iobj->cfg.image_width   = image_width;
        iobj->cfg.image_height  = image_height;
    }else
    {
        iobj->cfg.capture_mode  =  CAPTURE_MODE_HI_QUALITY;
        iobj->cfg.image_width   = PICTURE_WIDTH;
        iobj->cfg.image_height  = PICTURE_HEIGHT;
    }
#if DEBUG_LOG

	PPM("Before ICapture Config");

#endif

    status = (capture_status_t) iobj->lib.Config(iobj->lib_private, &iobj->cfg);

    if( ICAPTURE_FAIL == status){
        LOGE ("ICapture Config function failed");
        goto fail_config;
    }

#if DEBUG_LOG

    PPM("ICapture config OK");

	LOGD("iobj->cfg.image_width = %d = 0x%x iobj->cfg.image_height=%d = 0x%x , iobj->cfg.sizeof_img_buf = %d", (int)iobj->cfg.image_width, (int)iobj->cfg.image_width, (int)iobj->cfg.image_height, (int)iobj->cfg.image_height, (int)iobj->cfg.sizeof_img_buf);

#endif

    yuv_buffer = (uint8_t *) mYuvBuffer;
    offset = mPictureOffset;
    yuv_len = mPictureLength;

    iobj->proc.img_buf[0].start = yuv_buffer; 
    iobj->proc.img_buf[0].length = iobj->cfg.sizeof_img_buf;
    iobj->proc.img_bufs_count = 1;

#if DEBUG_LOG

	PPM("BEFORE ICapture Process");

#endif

    status = (capture_status_t) iobj->lib.Process(iobj->lib_private, &iobj->proc);

    if( ICAPTURE_FAIL == status){
        LOGE("ICapture Process failed");
        goto fail_process;
    }

#if DEBUG_LOG

    else {
        PPM("ICapture process OK");
    }

    LOGD("iobj->proc.out_img_w = %d = 0x%x iobj->proc.out_img_h=%u = 0x%x", (int)iobj->proc.out_img_w,(int)iobj->proc.out_img_w, (int)iobj->proc.out_img_h,(int)iobj->proc.out_img_h);
    
#endif

    pixelFormat = PIX_YUV422I;

//block until snapshot is ready
    fd_set descriptorSet;
    int max_fd;
    unsigned int snapshotReadyMessage;

    max_fd = snapshotReadyPipe[0] + 1;

    FD_ZERO(&descriptorSet);
    FD_SET(snapshotReadyPipe[0], &descriptorSet);

#ifdef DEBUG_LOG

    LOGD("Waiting on SnapshotThread ready message");

#endif

    err = select(max_fd,  &descriptorSet, NULL, NULL, NULL);
    if (err < 1) {
       LOGE("Error in select");
    }

    if(FD_ISSET(snapshotReadyPipe[0], &descriptorSet))
        read(snapshotReadyPipe[0], &snapshotReadyMessage, sizeof(snapshotReadyMessage));
//

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

        PPM("SENDING MESSAGE TO PROCESSING THREAD");

#endif

    mExifParams.exposure = fobj->status_2a.ae.shutter_cap;
    mExifParams.zoom = zoom_step[mZoomTargetIdx];
    exif_buf = get_exif_buffer(&mExifParams, gpsLocation);

    if( NULL != gpsLocation ) {
        free(gpsLocation);
        gpsLocation = NULL;
    }

    if (  iobj->proc.ipp.type == ICAP_IMAGEPOSTPROC_VER1_9 ) {
        EdgeEnhancementStrength = iobj->proc.ipp.ipp19.EdgeEnhancementStrength;
        WeakEdgeThreshold = iobj->proc.ipp.ipp19.WeakEdgeThreshold;
        StrongEdgeThreshold = iobj->proc.ipp.ipp19.StrongEdgeThreshold;
        LowFreqLumaNoiseFilterStrength = iobj->proc.ipp.ipp19.LowFreqLumaNoiseFilterStrength;
        MidFreqLumaNoiseFilterStrength = iobj->proc.ipp.ipp19.MidFreqLumaNoiseFilterStrength;
        HighFreqLumaNoiseFilterStrength = iobj->proc.ipp.ipp19.HighFreqLumaNoiseFilterStrength;
        LowFreqCbNoiseFilterStrength = iobj->proc.ipp.ipp19.LowFreqCbNoiseFilterStrength;
        MidFreqCbNoiseFilterStrength = iobj->proc.ipp.ipp19.MidFreqCbNoiseFilterStrength;
        HighFreqCbNoiseFilterStrength = iobj->proc.ipp.ipp19.HighFreqCbNoiseFilterStrength;
        LowFreqCrNoiseFilterStrength = iobj->proc.ipp.ipp19.LowFreqCrNoiseFilterStrength;
        MidFreqCrNoiseFilterStrength = iobj->proc.ipp.ipp19.MidFreqCrNoiseFilterStrength;
        HighFreqCrNoiseFilterStrength = iobj->proc.ipp.ipp19.HighFreqCrNoiseFilterStrength;
        shadingVertParam1 = iobj->proc.ipp.ipp19.shadingVertParam1;
        shadingVertParam2 = iobj->proc.ipp.ipp19.shadingVertParam2;
        shadingHorzParam1 = iobj->proc.ipp.ipp19.shadingHorzParam1;
        shadingHorzParam2 = iobj->proc.ipp.ipp19.shadingHorzParam2;
        shadingGainScale = iobj->proc.ipp.ipp19.shadingGainScale;
        shadingGainOffset = iobj->proc.ipp.ipp19.shadingGainOffset;
        shadingGainMaxValue = iobj->proc.ipp.ipp19.shadingGainMaxValue;
        ratioDownsampleCbCr = iobj->proc.ipp.ipp19.ratioDownsampleCbCr;
    } else if ( iobj->proc.ipp.type == ICAP_IMAGEPOSTPROC_VER1_8 ) {
            EdgeEnhancementStrength = iobj->proc.ipp.ipp18.ee_q;
            WeakEdgeThreshold = iobj->proc.ipp.ipp18.ew_ts;
            StrongEdgeThreshold = iobj->proc.ipp.ipp18.es_ts;
            LowFreqLumaNoiseFilterStrength = iobj->proc.ipp.ipp18.luma_nf;
            MidFreqLumaNoiseFilterStrength = iobj->proc.ipp.ipp18.luma_nf;
            HighFreqLumaNoiseFilterStrength = iobj->proc.ipp.ipp18.luma_nf;
            LowFreqCbNoiseFilterStrength = iobj->proc.ipp.ipp18.chroma_nf;
            MidFreqCbNoiseFilterStrength = iobj->proc.ipp.ipp18.chroma_nf;
            HighFreqCbNoiseFilterStrength = iobj->proc.ipp.ipp18.chroma_nf;
            LowFreqCrNoiseFilterStrength = iobj->proc.ipp.ipp18.chroma_nf;
            MidFreqCrNoiseFilterStrength = iobj->proc.ipp.ipp18.chroma_nf;
            HighFreqCrNoiseFilterStrength = iobj->proc.ipp.ipp18.chroma_nf;
            shadingVertParam1 = -1;
            shadingVertParam2 = -1;
            shadingHorzParam1 = -1;
            shadingHorzParam2 = -1;
            shadingGainScale = -1;
            shadingGainOffset = -1;
            shadingGainMaxValue = -1;
            ratioDownsampleCbCr = -1;
    } else {
            EdgeEnhancementStrength = 220;
            WeakEdgeThreshold = 8;
            StrongEdgeThreshold = 200;
            LowFreqLumaNoiseFilterStrength = 5;
            MidFreqLumaNoiseFilterStrength = 10;
            HighFreqLumaNoiseFilterStrength = 15;
            LowFreqCbNoiseFilterStrength = 20;
            MidFreqCbNoiseFilterStrength = 30;
            HighFreqCbNoiseFilterStrength = 10;
            LowFreqCrNoiseFilterStrength = 10;
            MidFreqCrNoiseFilterStrength = 25;
            HighFreqCrNoiseFilterStrength = 15;
            shadingVertParam1 = 10;
            shadingVertParam2 = 400;
            shadingHorzParam1 = 10;
            shadingHorzParam2 = 400;
            shadingGainScale = 128;
            shadingGainOffset = 2048;
            shadingGainMaxValue = 16384;
            ratioDownsampleCbCr = 1;
    }

    procMessage[0] = PROC_THREAD_PROCESS;
    procMessage[1] = iobj->proc.out_img_w;
    procMessage[2] = __ALIGN(iobj->proc.out_img_h, 16);
    procMessage[3] = image_width;
    procMessage[4] = image_height;
    procMessage[5] = pixelFormat;
    procMessage[6]  = EdgeEnhancementStrength;
    procMessage[7]  = WeakEdgeThreshold;
    procMessage[8]  = StrongEdgeThreshold;
    procMessage[9]  = LowFreqLumaNoiseFilterStrength;
    procMessage[10] = MidFreqLumaNoiseFilterStrength;
    procMessage[11] = HighFreqLumaNoiseFilterStrength;
    procMessage[12] = LowFreqCbNoiseFilterStrength;
    procMessage[13] = MidFreqCbNoiseFilterStrength;
    procMessage[14] = HighFreqCbNoiseFilterStrength;
    procMessage[15] = LowFreqCrNoiseFilterStrength;
    procMessage[16] = MidFreqCrNoiseFilterStrength;
    procMessage[17] = HighFreqCrNoiseFilterStrength;
    procMessage[18] = shadingVertParam1;
    procMessage[19] = shadingVertParam2;
    procMessage[20] = shadingHorzParam1;
    procMessage[21] = shadingHorzParam2;
    procMessage[22] = shadingGainScale;
    procMessage[23] = shadingGainOffset;
    procMessage[24] = shadingGainMaxValue;
    procMessage[25] = ratioDownsampleCbCr;
    procMessage[26] = (unsigned int) yuv_buffer;
    procMessage[27] = offset;
    procMessage[28] = yuv_len;
    procMessage[29] = rotation;
    procMessage[30] = mZoomTargetIdx;
    procMessage[31] = mippMode;
    procMessage[32] = mIPPToEnable;
    procMessage[33] = quality;
    procMessage[34] = (unsigned int) mDataCb;
    procMessage[35] = 0;
    procMessage[36] = (unsigned int) mCallbackCookie;
    procMessage[37] = iobj->proc.aspect_rect.top;
    procMessage[38] = iobj->proc.aspect_rect.left;
    procMessage[39] = iobj->proc.aspect_rect.width;
    procMessage[40] = iobj->proc.aspect_rect.height;
    procMessage[41] = (unsigned int) exif_buf;

    write(procPipe[1], &procMessage, sizeof(procMessage));

    mIPPToEnable = false; // reset ipp enable after sending to proc thread

#ifdef DEBUG_LOG

    LOGD("\n\n\n PICTURE NUMBER =%d\n\n\n",++pictureNumber);

#endif

    if( ( msgTypeEnabled(CAMERA_MSG_SHUTTER) ) && (mShutterEnable) ) {

#ifdef DEBUG_LOG

        PPM("SENDING MESSAGE TO SHUTTER THREAD");

#endif

        shutterMessage[0] = SHUTTER_THREAD_CALL;
        shutterMessage[1] = (unsigned int) mNotifyCb;
        shutterMessage[2] = (unsigned int) mCallbackCookie;
        write(shutterPipe[1], &shutterMessage, sizeof(shutterMessage));
    }

#ifdef DEBUG_LOG

    PPM("IMAGE CAPTURED");

#endif

    if( msgTypeEnabled(CAMERA_MSG_RAW_IMAGE) ) {

#ifdef DEBUG_LOG

        PPM("SENDING MESSAGE TO RAW THREAD");

#endif

        rawMessage[0] = RAW_THREAD_CALL;
        rawMessage[1] = (unsigned int) mDataCb;
        rawMessage[2] = (unsigned int) mCallbackCookie;
        rawMessage[3] = (unsigned int) NULL;
        write(rawPipe[1], &rawMessage, sizeof(rawMessage));
    }

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME_EXIT

#endif

    return 0;

fail_config :
fail_process:

    return -1;
}

#else

//TODO: Update the normal in according the PPM Changes
int CameraHal::ICapturePerform(){

    int image_width, image_height;
    int image_rotation;
    double image_zoom;
	int preview_width, preview_height;
    unsigned long base, offset;
    struct v4l2_buffer buffer;
    struct v4l2_format format;
    struct v4l2_buffer cfilledbuffer;
    struct v4l2_requestbuffers creqbuf;
    sp<MemoryBase> mPictureBuffer;
    sp<MemoryBase> memBase;
    int jpegSize;
    void * outBuffer;
    sp<MemoryHeapBase>  mJPEGPictureHeap;
    sp<MemoryBase>          mJPEGPictureMemBase;
	unsigned int vppMessage[3];
    int err, i;
    overlay_buffer_t overlaybuffer;
    int snapshot_buffer_index;
    void* snapshot_buffer;
    int ipp_reconfigure=0;
    int ippTempConfigMode;
    int jpegFormat = PIX_YUV422I;
    v4l2_streamparm parm;

    LOG_FUNCTION_NAME

    LOGD("\n\n\n PICTURE NUMBER =%d\n\n\n",++pictureNumber);

	if( ( msgTypeEnabled(CAMERA_MSG_SHUTTER) ) && mShutterEnable)
		mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0, mCallbackCookie);

    mParameters.getPictureSize(&image_width, &image_height);
    mParameters.getPreviewSize(&preview_width, &preview_height);

    LOGD("Picture Size: Width = %d \tHeight = %d", image_width, image_height);

    image_rotation = rotation;
    image_zoom = zoom_step[mZoomTargetIdx];

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = image_width;
    format.fmt.pix.height = image_height;
    format.fmt.pix.pixelformat = PIXEL_FORMAT;

    /* set size & format of the video image */
    if (ioctl(camera_device, VIDIOC_S_FMT, &format) < 0){
        LOGE ("Failed to set VIDIOC_S_FMT.");
        return -1;
    }

    //Set 10 fps for 8MP case
    if( ( image_height == CAPTURE_8MP_HEIGHT ) && ( image_width == CAPTURE_8MP_WIDTH ) ) {
        LOGE("8MP Capture setting framerate to 10");
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        err = ioctl(camera_device, VIDIOC_G_PARM, &parm);
        if(err != 0) {
            LOGD("VIDIOC_G_PARM ");
            return -1;
        }

        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = 10;
        err = ioctl(camera_device, VIDIOC_S_PARM, &parm);
        if(err != 0) {
            LOGE("VIDIOC_S_PARM ");
            return -1;
        }
    }

    /* Check if the camera driver can accept 1 buffer */
    creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    creqbuf.memory = V4L2_MEMORY_USERPTR;
    creqbuf.count  = 1;
    if (ioctl(camera_device, VIDIOC_REQBUFS, &creqbuf) < 0){
        LOGE ("VIDIOC_REQBUFS Failed. errno = %d", errno);
        return -1;
    }

    yuv_len = image_width * image_height * 2;
    if (yuv_len & 0xfff)
    {
        yuv_len = (yuv_len & 0xfffff000) + 0x1000;
    }
    LOGD("pictureFrameSize = 0x%x = %d", yuv_len, yuv_len);
#define ALIGMENT 1
#if ALIGMENT
    mPictureHeap = new MemoryHeapBase(yuv_len);
#else
    // Make a new mmap'ed heap that can be shared across processes.
    mPictureHeap = new MemoryHeapBase(yuv_len + 0x20 + 256);
#endif

    base = (unsigned long)mPictureHeap->getBase();

#if ALIGMENT
	base = (base + 0xfff) & 0xfffff000;
#else
    /*Align buffer to 32 byte boundary */
    while ((base & 0x1f) != 0)
    {
        base++;
    }
    /* Buffer pointer shifted to avoid DSP cache issues */
    base += 128;
#endif

    offset = base - (unsigned long)mPictureHeap->getBase();

    buffer.type = creqbuf.type;
    buffer.memory = creqbuf.memory;
    buffer.index = 0;

    if (ioctl(camera_device, VIDIOC_QUERYBUF, &buffer) < 0) {
        LOGE("VIDIOC_QUERYBUF Failed");
        return -1;
    }

    yuv_len = buffer.length;

    buffer.m.userptr = base;
    mPictureBuffer = new MemoryBase(mPictureHeap, offset, yuv_len);
    LOGD("Picture Buffer: Base = %p Offset = 0x%x", (void *)base, (unsigned int)offset);

    if (ioctl(camera_device, VIDIOC_QBUF, &buffer) < 0) {
        LOGE("CAMERA VIDIOC_QBUF Failed");
        return -1;
    }

    /* turn on streaming */
    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMON, &creqbuf.type) < 0) {
        LOGE("VIDIOC_STREAMON Failed");
        return -1;
    }

    LOGD("De-queue the next avaliable buffer");

    /* De-queue the next avaliable buffer */
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

#if ALIGMENT
    cfilledbuffer.memory = creqbuf.memory;
#else
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;
#endif
    while (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
        LOGE("VIDIOC_DQBUF Failed");
    }

    PPM("AFTER CAPTURE YUV IMAGE");

    /* turn off streaming */
    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) < 0) {
        LOGE("VIDIOC_STREAMON Failed");
        return -1;
    }

	if( msgTypeEnabled(CAMERA_MSG_RAW_IMAGE) ){
		mDataCb(CAMERA_MSG_RAW_IMAGE, mPictureBuffer,mCallbackCookie);
	}

    yuv_buffer = (uint8_t*)buffer.m.userptr;
    LOGD("PictureThread: generated a picture, yuv_buffer=%p yuv_len=%d",yuv_buffer,yuv_len);

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

    mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress( (void*) (lastOverlayBufferDQ) );
    if ( data == NULL ) {
        LOGE(" getBufferAddress returned NULL skipping snapshot");
    } else{
        snapshot_buffer = data->ptr;

        scale_init(image_width, image_height, preview_width, preview_height, PIX_YUV422I, PIX_YUV422I);

        err = scale_process(yuv_buffer, image_width, image_height,
                             snapshot_buffer, preview_width, preview_height, 0, PIX_YUV422I, zoom_step[mZoomTargetIdx], 0, 0, image_width, image_height);

#ifdef DEBUG_LOG
       PPM("After vpp downscales:");
       if( err )
            LOGE("scale_process() failed");
       else
            LOGD("scale_process() OK");
#endif

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
        PPM("Shot to Snapshot", &ppm_receiveCmdToTakePicture);
#endif
        scale_deinit();

        err = mOverlay->queueBuffer((void*)(lastOverlayBufferDQ));
        if (err < 0) {
            LOGE("mOverlay->queueBuffer() failed!!!!");
        } else {
            buffers_queued_to_dss[lastOverlayBufferDQ]=1;
            nOverlayBuffersQueued++;
        }

        err = mOverlay->dequeueBuffer(&overlaybuffer);
        if (err) {
            LOGE("mOverlay->dequeueBuffer() failed!!!!");
        } else {
            nOverlayBuffersQueued--;
            buffers_queued_to_dss[(int)overlaybuffer] = 0;
            lastOverlayBufferDQ = (int)overlaybuffer;
         }
    }
#endif
#endif
#endif

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

        if(mIPPToEnable)
        {
            err = InitIPP(image_width,image_height, jpegFormat, mippMode);
            if( err ) {
                LOGE("ERROR InitIPP() failed");
                return -1;
            }
            PPM("After IPP Init");
            mIPPToEnable = false;
        }

		err = PopulateArgsIPP(image_width,image_height, jpegFormat, mippMode);
		if( err ) {
			LOGE("ERROR PopulateArgsIPP() failed");
			return -1;
		}
		PPM("BEFORE IPP Process Buffer");

		LOGD("Calling ProcessBufferIPP(buffer=%p , len=0x%x)", yuv_buffer, yuv_len);
	err = ProcessBufferIPP(yuv_buffer, yuv_len,
                    jpegFormat,
                    mippMode,
                    -1, // EdgeEnhancementStrength
                    -1, // WeakEdgeThreshold
                    -1, // StrongEdgeThreshold
                    -1, // LowFreqLumaNoiseFilterStrength
                    -1, // MidFreqLumaNoiseFilterStrength
                    -1, // HighFreqLumaNoiseFilterStrength
                    -1, // LowFreqCbNoiseFilterStrength
                    -1, // MidFreqCbNoiseFilterStrength
                    -1, // HighFreqCbNoiseFilterStrength
                    -1, // LowFreqCrNoiseFilterStrength
                    -1, // MidFreqCrNoiseFilterStrength
                    -1, // HighFreqCrNoiseFilterStrength
                    -1, // shadingVertParam1
                    -1, // shadingVertParam2
                    -1, // shadingHorzParam1
                    -1, // shadingHorzParam2
                    -1, // shadingGainScale
                    -1, // shadingGainOffset
                    -1, // shadingGainMaxValue
                    -1); // ratioDownsampleCbCr
		if( err ) {
			LOGE("ERROR ProcessBufferIPP() failed");
			return -1;
		}
		PPM("AFTER IPP Process Buffer");

  	if(!(pIPP.ippconfig.isINPLACE)){
		yuv_buffer = pIPP.pIppOutputBuffer;
	}

	#if ( IPP_YUV422P || IPP_YUV420P_OUTPUT_YUV422I )
		jpegFormat = PIX_YUV422I;
		LOGD("YUV422 !!!!");
	#else
		yuv_len=  ((image_width * image_height *3)/2);
        jpegFormat = PIX_YUV420P;
		LOGD("YUV420 !!!!");
    #endif

	}
	//SaveFile(NULL, (char*)"yuv", yuv_buffer, yuv_len);

#endif

	if ( msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) )
	{

#ifdef HARDWARE_OMX
#if JPEG
        int jpegSize = (image_width * image_height) + 12288;
        mJPEGPictureHeap = new MemoryHeapBase(jpegSize+ 256);
        outBuffer = (void *)((unsigned long)(mJPEGPictureHeap->getBase()) + 128);

        exif_buffer *exif_buf = get_exif_buffer(&mExifParams, gpsLocation);

		PPM("BEFORE JPEG Encode Image");
		LOGE(" outbuffer = 0x%x, jpegSize = %d, yuv_buffer = 0x%x, yuv_len = %d, image_width = %d, image_height = %d, quality = %d, mippMode =%d", outBuffer , jpegSize, yuv_buffer, yuv_len, image_width, image_height, quality,mippMode);
		jpegEncoder->encodeImage((uint8_t *)outBuffer , jpegSize, yuv_buffer, yuv_len,
				image_width, image_height, quality, exif_buf, jpegFormat, THUMB_WIDTH, THUMB_HEIGHT, image_width, image_height,
				image_rotation, image_zoom, 0, 0, image_width, image_height);
		PPM("AFTER JPEG Encode Image");

		mJPEGPictureMemBase = new MemoryBase(mJPEGPictureHeap, 128, jpegEncoder->jpegSize);

	if ( msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ){
		mDataCb(CAMERA_MSG_COMPRESSED_IMAGE,mJPEGPictureMemBase,mCallbackCookie);
    }


		PPM("Shot to Save", &ppm_receiveCmdToTakePicture);
        if((exif_buf != NULL) && (exif_buf->data != NULL))
            exif_buf_free (exif_buf);

        if( NULL != gpsLocation ) {
            free(gpsLocation);
            gpsLocation = NULL;
        }
        mJPEGPictureMemBase.clear();
        mJPEGPictureHeap.clear();

#else

	if ( msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) )
		mDataCb(CAMERA_MSG_COMPRESSED_IMAGE,NULL,mCallbackCookie);

#endif

#endif

    }

    mPictureBuffer.clear();
    mPictureHeap.clear();

#ifdef HARDWARE_OMX
#if VPP_THREAD
	LOGD("CameraHal thread before waiting increment in semaphore\n");
	sem_wait(&mIppVppSem);
	LOGD("CameraHal thread after waiting increment in semaphore\n");
#endif
#endif

    PPM("END OF ICapturePerform");
    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;

}

#endif

void CameraHal::snapshotThread()
{
    fd_set descriptorSet;
    int max_fd;
    int err, status;
    unsigned int snapshotMessage[9], snapshotReadyMessage;
    int image_width, image_height, pixelFormat, preview_width, preview_height;
    int crop_top, crop_left, crop_width, crop_height;
    overlay_buffer_t overlaybuffer;
    void *yuv_buffer, *snapshot_buffer;
    double ZoomTarget;

    LOG_FUNCTION_NAME

    pixelFormat = PIX_YUV422I;
    max_fd = snapshotPipe[0] + 1;

    FD_ZERO(&descriptorSet);
    FD_SET(snapshotPipe[0], &descriptorSet);

    while(1) {
        err = select(max_fd,  &descriptorSet, NULL, NULL, NULL);

#ifdef DEBUG_LOG

       LOGD("SNAPSHOT THREAD SELECT RECEIVED A MESSAGE\n");

#endif

       if (err < 1) {
           LOGE("Snapshot: Error in select");
       }

       if(FD_ISSET(snapshotPipe[0], &descriptorSet)){

           read(snapshotPipe[0], &snapshotMessage, sizeof(snapshotMessage));

           if(snapshotMessage[0] == SNAPSHOT_THREAD_START){

#ifdef DEBUG_LOG

                LOGD("SNAPSHOT_THREAD_START RECEIVED\n");

#endif

                yuv_buffer = (void *) snapshotMessage[1];
                image_width = snapshotMessage[2];
                image_height = snapshotMessage[3];
                ZoomTarget = zoom_step[snapshotMessage[4]];
                crop_top = snapshotMessage[5];
                crop_left = snapshotMessage[6];
                crop_width = snapshotMessage[7];
                crop_height = snapshotMessage[8];

                mParameters.getPreviewSize(&preview_width, &preview_height);

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

                PPM("Before vpp downscales:");

#endif

                mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress( (void*) (lastOverlayBufferDQ) );
                if ( data == NULL ) {
                    LOGE(" getBufferAddress returned NULL");
		    snapshot_buffer = NULL;	
                }
		else
		{
                snapshot_buffer = data->ptr;
		}		
#ifdef HARDWARE_OMX
                scale_init(image_width, image_height, preview_width, preview_height, PIX_YUV422I, PIX_YUV422I);

                status = scale_process(yuv_buffer, image_width, image_height,
                         snapshot_buffer, preview_width, preview_height, 0, PIX_YUV422I, 1, crop_top, crop_left, crop_width, crop_height);
#endif
#ifdef DEBUG_LOG

               PPM("After vpp downscales:");

               if( status )
                   LOGE("scale_process() failed");
               else
                   LOGD("scale_process() OK");

#endif

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

               PPM("Shot to Snapshot", &ppm_receiveCmdToTakePicture);

#endif
#ifdef HARDWARE_OMX
                scale_deinit();
#endif
                status = mOverlay->queueBuffer((void*)(lastOverlayBufferDQ));
                if (status < 0) {
                     LOGE("mOverlay->queueBuffer() failed!!!!");
                } else {
                     buffers_queued_to_dss[lastOverlayBufferDQ]=1;
                     nOverlayBuffersQueued++;
                }

                status = mOverlay->dequeueBuffer(&overlaybuffer);
                if (status) {
                    LOGE("mOverlay->dequeueBuffer() failed!!!!");
                } else {
                    nOverlayBuffersQueued--;
                    buffers_queued_to_dss[(int)overlaybuffer] = 0;
                    lastOverlayBufferDQ = (int)overlaybuffer;
                }

                write(snapshotReadyPipe[1], &snapshotReadyMessage, sizeof(snapshotReadyMessage));

          } else if (snapshotMessage[0] == SNAPSHOT_THREAD_EXIT) {
                LOGD("SNAPSHOT_THREAD_EXIT RECEIVED");

                break;
          }
        }
    }

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::rawThread()
{
    LOG_FUNCTION_NAME

    fd_set descriptorSet;
    int max_fd;
    int err;
    unsigned int rawMessage[RAW_THREAD_NUM_ARGS];
    data_callback RawCallback;
    void *PictureCallbackCookie;
    sp<MemoryBase> rawData;

    max_fd = rawPipe[0] + 1;

    FD_ZERO(&descriptorSet);
    FD_SET(rawPipe[0], &descriptorSet);

    while(1) {
        err = select(max_fd,  &descriptorSet, NULL, NULL, NULL);

#ifdef DEBUG_LOG

            LOGD("RAW THREAD SELECT RECEIVED A MESSAGE\n");

#endif

            if (err < 1) {
                LOGE("Raw: Error in select");
            }

            if(FD_ISSET(rawPipe[0], &descriptorSet)){

                read(rawPipe[0], &rawMessage, sizeof(rawMessage));

                if(rawMessage[0] == RAW_THREAD_CALL){

#ifdef DEBUG_LOG

                LOGD("RAW_THREAD_CALL RECEIVED\n");

#endif

                RawCallback = (data_callback) rawMessage[1];
                PictureCallbackCookie = (void *) rawMessage[2];
                rawData = (MemoryBase *) rawMessage[3];

                RawCallback(CAMERA_MSG_RAW_IMAGE, rawData, PictureCallbackCookie);

#ifdef DEBUG_LOG

                PPM("RAW CALLBACK CALLED");

#endif

            } else if (rawMessage[0] == RAW_THREAD_EXIT) {
                LOGD("RAW_THREAD_EXIT RECEIVED");

                break;
            }
        }
    }

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::shutterThread()
{
    LOG_FUNCTION_NAME

    fd_set descriptorSet;
    int max_fd;
    int err;
    unsigned int shutterMessage[SHUTTER_THREAD_NUM_ARGS];
    notify_callback ShutterCallback;
    void *PictureCallbackCookie;

    max_fd = shutterPipe[0] + 1;

    FD_ZERO(&descriptorSet);
    FD_SET(shutterPipe[0], &descriptorSet);

    while(1) {
        err = select(max_fd,  &descriptorSet, NULL, NULL, NULL);

#ifdef DEBUG_LOG

        LOGD("SHUTTER THREAD SELECT RECEIVED A MESSAGE\n");

#endif

        if (err < 1) {
            LOGE("Shutter: Error in select");
        }

        if(FD_ISSET(shutterPipe[0], &descriptorSet)){

           read(shutterPipe[0], &shutterMessage, sizeof(shutterMessage));

           if(shutterMessage[0] == SHUTTER_THREAD_CALL){

#ifdef DEBUG_LOG

                LOGD("SHUTTER_THREAD_CALL_RECEIVED\n");

#endif

                ShutterCallback = (notify_callback) shutterMessage[1];
                PictureCallbackCookie = (void *) shutterMessage[2];

                ShutterCallback(CAMERA_MSG_SHUTTER, 0, 0, PictureCallbackCookie);

#ifdef DEBUG_LOG

                PPM("CALLED SHUTTER CALLBACK");

#endif

            } else if (shutterMessage[0] == SHUTTER_THREAD_EXIT) {
                LOGD("SHUTTER_THREAD_EXIT RECEIVED");

                break;
            }
        }
    }

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::procThread()
{
    LOG_FUNCTION_NAME

    int status;
    int capture_width, capture_height, image_width, image_height;
    fd_set descriptorSet;
    int max_fd;
    int err;
    int pixelFormat;
    unsigned int procMessage [PROC_THREAD_NUM_ARGS];
    unsigned int jpegQuality, jpegSize, size, base, tmpBase, offset, yuv_offset, yuv_len, image_rotation, ippMode;
    unsigned int crop_top, crop_left, crop_width, crop_height;
    double image_zoom;
    bool ipp_to_enable;
    sp<MemoryHeapBase> JPEGPictureHeap;
    sp<MemoryBase> JPEGPictureMemBase;
    data_callback RawPictureCallback;
    data_callback JpegPictureCallback;
    void *yuv_buffer, *outBuffer, *PictureCallbackCookie;
    int thumb_width, thumb_height;
#ifdef HARDWARE_OMX
    exif_buffer *exif_buf;
#endif
    unsigned short EdgeEnhancementStrength, WeakEdgeThreshold, StrongEdgeThreshold,
                   LowFreqLumaNoiseFilterStrength, MidFreqLumaNoiseFilterStrength, HighFreqLumaNoiseFilterStrength,
                   LowFreqCbNoiseFilterStrength, MidFreqCbNoiseFilterStrength, HighFreqCbNoiseFilterStrength,
                   LowFreqCrNoiseFilterStrength, MidFreqCrNoiseFilterStrength, HighFreqCrNoiseFilterStrength,
                   shadingVertParam1, shadingVertParam2, shadingHorzParam1, shadingHorzParam2, shadingGainScale,
                   shadingGainOffset, shadingGainMaxValue, ratioDownsampleCbCr;

    void* input_buffer;
    unsigned int input_length;

    sp<MemoryHeapBase> tmpHeap;
    unsigned int tmpLength, tmpOffset;
    void *tmpBuffer;

    max_fd = procPipe[0] + 1;

    FD_ZERO(&descriptorSet);
    FD_SET(procPipe[0], &descriptorSet);

    mJPEGLength  = PICTURE_WIDTH*PICTURE_HEIGHT + ((2*PAGE) - 1);
    mJPEGLength &= ~((2*PAGE) - 1);
    mJPEGLength  += 2*PAGE;
    mJPEGPictureHeap = new MemoryHeapBase(mJPEGLength);

    base = (unsigned long) mJPEGPictureHeap->getBase();
    base = (base + 0xfff) & 0xfffff000;
    mJPEGOffset = base - (unsigned long) mJPEGPictureHeap->getBase();
    mJPEGBuffer = (void *) base;

    tmpLength  = PICTURE_WIDTH*PICTURE_HEIGHT*2 + ((2*PAGE) - 1);
    tmpLength &= ~((2*PAGE) - 1);
    tmpLength  += 2*PAGE;

    base = (unsigned int) malloc(tmpLength);
    tmpBase = base;
    base = (base + 0xfff) & 0xfffff000;
    tmpOffset = base - tmpBase;
    tmpBuffer = (void *) base;

    while(1){

        err = select(max_fd,  &descriptorSet, NULL, NULL, NULL);

#ifdef DEBUG_LOG

        LOGD("PROCESSING THREAD SELECT RECEIVED A MESSAGE\n");

#endif

        if (err < 1) {
            LOGE("Proc: Error in select");
        }

        if(FD_ISSET(procPipe[0], &descriptorSet)){

            read(procPipe[0], &procMessage, sizeof(procMessage));
			
            if(procMessage[0] == PROC_THREAD_PROCESS){

#ifdef DEBUG_LOG

                LOGD("PROC_THREAD_PROCESS_RECEIVED\n");
				
#endif

                capture_width = procMessage[1];
                capture_height = procMessage[2];
                image_width = procMessage[3];
                image_height = procMessage[4];
                pixelFormat = procMessage[5];
                EdgeEnhancementStrength = procMessage[6];
                WeakEdgeThreshold = procMessage[7];
                StrongEdgeThreshold = procMessage[8];
                LowFreqLumaNoiseFilterStrength = procMessage[9];
                MidFreqLumaNoiseFilterStrength = procMessage[10];
                HighFreqLumaNoiseFilterStrength = procMessage[11];
                LowFreqCbNoiseFilterStrength = procMessage[12];
                MidFreqCbNoiseFilterStrength = procMessage[13];
                HighFreqCbNoiseFilterStrength = procMessage[14];
                LowFreqCrNoiseFilterStrength = procMessage[15];
                MidFreqCrNoiseFilterStrength = procMessage[16];
                HighFreqCrNoiseFilterStrength = procMessage[17];
                shadingVertParam1 = procMessage[18];
                shadingVertParam2 = procMessage[19];
                shadingHorzParam1 = procMessage[20];
                shadingHorzParam2 = procMessage[21];
                shadingGainScale = procMessage[22];
                shadingGainOffset = procMessage[23];
                shadingGainMaxValue = procMessage[24];
                ratioDownsampleCbCr = procMessage[25];
                yuv_buffer = (void *) procMessage[26];
                yuv_offset =  procMessage[27];
                yuv_len = procMessage[28];
                image_rotation = procMessage[29];
                image_zoom = zoom_step[procMessage[30]];
                ippMode = procMessage[31];
                ipp_to_enable = procMessage[32];
                jpegQuality = procMessage[33];
                JpegPictureCallback = (data_callback) procMessage[34];
                RawPictureCallback = (data_callback) procMessage[35];
                PictureCallbackCookie = (void *) procMessage[36];
                crop_top = procMessage[37];
                crop_left = procMessage[38];
                crop_width = procMessage[39];
                crop_height = procMessage[40];
#ifdef HARDWARE_OMX
                exif_buf = (exif_buffer *) procMessage[41];
#endif
                jpegSize = mJPEGLength;
                JPEGPictureHeap = mJPEGPictureHeap;
                outBuffer = mJPEGBuffer;
                offset = mJPEGOffset;
                thumb_width = THUMB_WIDTH;
                thumb_height = THUMB_HEIGHT;
                pixelFormat = PIX_YUV422I;

                input_buffer = yuv_buffer;
                input_length = yuv_len;

#ifdef IMAGE_PROCESSING_PIPELINE

#ifdef DEBUG_LOG

                LOGD("IPPmode=%d",ippMode);
                if(ippMode == IPP_CromaSupression_Mode){
                    LOGD("IPP_CromaSupression_Mode");
                }
                else if(ippMode == IPP_EdgeEnhancement_Mode){
                    LOGD("IPP_EdgeEnhancement_Mode");
                }
                else if(ippMode == IPP_Disabled_Mode){
                    LOGD("IPP_Disabled_Mode");
                }

#endif

	        if( (ippMode == IPP_CromaSupression_Mode) || (ippMode == IPP_EdgeEnhancement_Mode) ){

                    if(ipp_to_enable) {

#ifdef DEBUG_LOG
                         PPM("Before init IPP");
#endif

                         err = InitIPP(capture_width, capture_height, pixelFormat, ippMode);
                         if( err )
                             LOGE("ERROR InitIPP() failed");

#ifdef DEBUG_LOG
                             PPM("After IPP Init");
#endif

                     }

                   err = PopulateArgsIPP(capture_width, capture_height, pixelFormat, ippMode);
                    if( err )
                         LOGE("ERROR PopulateArgsIPP() failed");

#ifdef DEBUG_LOG
                     PPM("BEFORE IPP Process Buffer");
                     LOGD("Calling ProcessBufferIPP(buffer=%p , len=0x%x)", yuv_buffer, yuv_len);
#endif
                    // TODO: Need to add support for new EENF 1.9 parameters from proc messages
                     err = ProcessBufferIPP(input_buffer, input_length,
                            pixelFormat,
                            ippMode,
                            EdgeEnhancementStrength,
                            WeakEdgeThreshold,
                            StrongEdgeThreshold,
                            LowFreqLumaNoiseFilterStrength,
                            MidFreqLumaNoiseFilterStrength,
                            HighFreqLumaNoiseFilterStrength,
                            LowFreqCbNoiseFilterStrength,
                            MidFreqCbNoiseFilterStrength,
                            HighFreqCbNoiseFilterStrength,
                            LowFreqCrNoiseFilterStrength,
                            MidFreqCrNoiseFilterStrength,
                            HighFreqCrNoiseFilterStrength,
                            shadingVertParam1,
                            shadingVertParam2,
                            shadingHorzParam1,
                            shadingHorzParam2,
                            shadingGainScale,
                            shadingGainOffset,
                            shadingGainMaxValue,
                            ratioDownsampleCbCr);
                    if( err )
                         LOGE("ERROR ProcessBufferIPP() failed");

#ifdef DEBUG_LOG
                    PPM("AFTER IPP Process Buffer");
#endif
                    pixelFormat = PIX_YUV422I; //output of IPP is always 422I
                    if(!(pIPP.ippconfig.isINPLACE)){
                        input_buffer = pIPP.pIppOutputBuffer;
                        input_length = pIPP.outputBufferSize;
                    }
               }
#endif

#if JPEG
                err = 0;

#ifdef DEBUG_LOG
                LOGD(" outbuffer = %p, jpegSize = %d, input_buffer = %p, yuv_len = %d, image_width = %d, image_height = %d, quality = %d, ippMode =%d", outBuffer , jpegSize, input_buffer/*yuv_buffer*/, input_length/*yuv_len*/, image_width, image_height, jpegQuality, ippMode);
#endif

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
                PPM("BEFORE JPEG Encode Image");
#endif
                if (!( jpegEncoder->encodeImage((uint8_t *)outBuffer , jpegSize, input_buffer, input_length,
                                             capture_width, capture_height, jpegQuality, exif_buf, pixelFormat, thumb_width, thumb_height, image_width, image_height,
                                             image_rotation, image_zoom, crop_top, crop_left, crop_width, crop_height)))
                {
                    err = -1;
                    LOGE("JPEG Encoding failed");
                }
#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
                PPM("AFTER JPEG Encode Image");
                if ( 0 != image_rotation )
                    PPM("Shot to JPEG with %d deg rotation", &ppm_receiveCmdToTakePicture, image_rotation);
                else
                    PPM("Shot to JPEG", &ppm_receiveCmdToTakePicture);
#endif

                JPEGPictureMemBase = new MemoryBase(JPEGPictureHeap, offset, jpegEncoder->jpegSize);
#endif
                /* Disable the jpeg message enabled check for now */
                if( /*msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) && */ JpegPictureCallback ) {

#if JPEG

                    JpegPictureCallback(CAMERA_MSG_COMPRESSED_IMAGE, JPEGPictureMemBase, PictureCallbackCookie);

#else

                    JpegPictureCallback(CAMERA_MSG_COMPRESSED_IMAGE, NULL, PictureCallbackCookie);

#endif

                }

#ifdef DEBUG_LOG

                LOGD("jpegEncoder->jpegSize=%d jpegSize=%d", jpegEncoder->jpegSize, jpegSize);

#endif
#ifdef HARDWARE_OMX
                if((exif_buf != NULL) && (exif_buf->data != NULL))
                    exif_buf_free(exif_buf);
#endif
                JPEGPictureMemBase.clear();
                free((void *) ( ((unsigned int) yuv_buffer) - yuv_offset) );

            } else if( procMessage[0] == PROC_THREAD_EXIT ) {
                LOGD("PROC_THREAD_EXIT_RECEIVED");

                mJPEGPictureHeap->dispose();
                mJPEGPictureHeap.clear();
                free((void *) ( ((unsigned int) tmpBuffer) - tmpOffset) );

                break;
            }
        }
    }

    JPEGPictureHeap.clear();

    LOG_FUNCTION_NAME_EXIT
}

#ifdef ICAP_EXPERIMENTAL

int CameraHal::allocatePictureBuffer(size_t length)
{
    unsigned int base, tmpBase;

    mPictureLength  = length + ((2*PAGE) - 1) + 10*PAGE;
    mPictureLength &= ~((2*PAGE) - 1);
    mPictureLength  += 2*PAGE;

    base = (unsigned int) malloc(mPictureLength);
    tmpBase = base;
    base = (base + 0xfff) & 0xfffff000;
    mPictureOffset = base - tmpBase;
    mYuvBuffer = (uint8_t *) base;
    mPictureLength -= mPictureOffset;

    return 0;
}

#else

int CameraHal::allocatePictureBuffer(int width, int height)
{
    unsigned int base, tmpBase;

    mPictureLength  = width*height*2 + ((2*PAGE) - 1) + 10*PAGE;
    mPictureLength &= ~((2*PAGE) - 1);
    mPictureLength  += 2*PAGE;

    base = (unsigned int) malloc(mPictureLength);
    tmpBase = base;
    base = (base + 0xfff) & 0xfffff000;
    mPictureOffset = base - tmpBase;
    mYuvBuffer = (uint8_t *) base;
    mPictureLength -= mPictureOffset;

    return 0;
}

#endif

int CameraHal::ICaptureCreate(void)
{
    int res;
    overlay_buffer_t overlaybuffer;
    int image_width, image_height;

    LOG_FUNCTION_NAME
    
    res = 0;
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
        res = -1;
    }

    if( res >= 0){
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
#ifdef IMAGE_PROCESSING_PIPELINE

	mippMode = IPP_Disabled_Mode;

#endif

#if JPEG
    jpegEncoder = new JpegEncoder;

    if( NULL != jpegEncoder )
        isStart_JPEG = true;
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
#if JPEG
    if( isStart_JPEG )
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
    int w,h;

    LOGD("CameraHal setOverlay/1/%08lx/%08lx", (long unsigned int)overlay.get(), (long unsigned int)mOverlay.get());
    // De-alloc any stale data
    if ( mOverlay.get() != NULL )
    {
        LOGD("Destroying current overlay");
        
        int buffer_count = mOverlay->getBufferCount();
        for(int i =0; i < buffer_count ; i++){
            // need to free buffers and heaps mapped using overlay fd before it is destroyed
            // otherwise we could create a resource leak
            // a segfault could occur if we try to free pointers after overlay is destroyed
            mPreviewBuffers[i].clear();
            mPreviewHeaps[i].clear();
            mVideoBuffer[i].clear();
            mVideoHeaps[i].clear();
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

    mParameters.getPreviewSize(&w, &h);
    if ((w == RES_720P) || (h == RES_720P))
    {
        mOverlay->setParameter(CACHEABLE_BUFFERS, 1);
        mOverlay->setParameter(MAINTAIN_COHERENCY, 0);
        mOverlay->resizeInput(w, h);
    }

    if (mFalsePreview)   // Eclair HAL
    {
     // Restart the preview (Only for Overlay Case)
	//LOGD("In else overlay");
        mPreviewRunning = false;
        Message msg;
        msg.command = PREVIEW_START;
        previewThreadCommandQ.put(&msg);
        previewThreadAckQ.get(&msg);
    }	// Eclair HAL

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

status_t CameraHal::startPreview()
{
    LOG_FUNCTION_NAME

    if(mOverlay == NULL)	//Eclair HAL
    {
	    LOGD("Return from camera Start Preview");
	    mPreviewRunning = true;
	    mFalsePreview = true;
	    return NO_ERROR;
    }      //Eclair HAL

    mFalsePreview = false;   //Eclair HAL

    Message msg;
    msg.command = PREVIEW_START;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

    LOG_FUNCTION_NAME_EXIT
    return msg.command == PREVIEW_ACK ? NO_ERROR : INVALID_OPERATION;
}

void CameraHal::stopPreview()
{
    LOG_FUNCTION_NAME

    mFalsePreview = false;  //Eclair HAL
    Message msg;
    msg.command = PREVIEW_STOP;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);
}


status_t CameraHal::autoFocus()
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = PREVIEW_AF_START;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

    LOG_FUNCTION_NAME_EXIT
    return msg.command == PREVIEW_ACK ? NO_ERROR : INVALID_OPERATION;
}

bool CameraHal::previewEnabled()
{
    return mPreviewRunning;
}


status_t CameraHal::startRecording( )
{
    LOG_FUNCTION_NAME
    int w,h;
    int i = 0;

    mPrevTime = systemTime(SYSTEM_TIME_MONOTONIC);
    int framerate = mParameters.getPreviewFrameRate();
    frameInterval = 1000000000LL / framerate ;
    mParameters.getPreviewSize(&w, &h);
    mRecordingFrameSize = w * h * 2;
    overlay_handle_t overlayhandle = mOverlay->getHandleRef();
    overlay_true_handle_t true_handle;
    if ( overlayhandle == NULL ) {
        LOGD("overlayhandle is received as NULL. ");
        return UNKNOWN_ERROR;
    }

    memcpy(&true_handle,overlayhandle,sizeof(overlay_true_handle_t));
    int overlayfd = true_handle.ctl_fd;
    LOGD("#Overlay driver FD:%d ",overlayfd);

    mVideoBufferCount =  mOverlay->getBufferCount();

    if (mVideoBufferCount > VIDEO_FRAME_COUNT_MAX)
    {
        LOGD("Error: mVideoBufferCount > VIDEO_FRAME_COUNT_MAX");
        return UNKNOWN_ERROR;
    }

    mRecordingLock.lock();

    for(i = 0; i < mVideoBufferCount; i++)
    {
        mVideoHeaps[i].clear();
        mVideoBuffer[i].clear();
        buffers_queued_to_ve[i] = 0;
    }

    for(i = 0; i < mVideoBufferCount; i++)
    {
        mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress((void*)i);
        mVideoHeaps[i] = new MemoryHeapBase(data->fd,mPreviewFrameSize, 0, data->offset);
        mVideoBuffer[i] = new MemoryBase(mVideoHeaps[i], 0, mRecordingFrameSize);
        LOGV("mVideoHeaps[%d]: ID:%d,Base:[%p],size:%d", i, mVideoHeaps[i]->getHeapID(), mVideoHeaps[i]->getBase() ,mVideoHeaps[i]->getSize());
        LOGV("mVideoBuffer[%d]: Pointer[%p]", i, mVideoBuffer[i]->pointer());
    }
    
    mRecordEnabled =true;
    mRecordingLock.unlock();
    return NO_ERROR;
}

void CameraHal::stopRecording()
{
    LOG_FUNCTION_NAME
    mRecordingLock.lock();
    mRecordEnabled = false;

    for(int i = 0; i < MAX_CAMERA_BUFFERS; i ++)
    {
        if (buffers_queued_to_ve[i] == 1)
        {
            if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[i]) < 0) {
                LOGE("VIDIOC_QBUF Failed. line=%d",__LINE__);
            }
            else nCameraBuffersQueued++;
            buffers_queued_to_ve[i] = 0;
            LOGD("Buffer #%d was not returned by VE. Reclaiming it !!!!!!!!!!!!!!!!!!!!!!!!", i);        
        }
    }

    mRecordingLock.unlock();
}

bool CameraHal::recordingEnabled()
{
    LOG_FUNCTION_NAME
    return (mRecordEnabled);
}
void CameraHal::releaseRecordingFrame(const sp<IMemory>& mem)
{
    int index;

    for(index = 0; index <mVideoBufferCount; index ++){
        if(mem->pointer() == mVideoBuffer[index]->pointer()) {
            break;
        }
    }

    if (index == mVideoBufferCount)
    {
        LOGD("Encoder returned wrong buffer address");
        return;
    }
    
    debugShowFPS();
    
    if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[index]) < 0) 
    {
        LOGE("VIDIOC_QBUF Failed, index [%d] line=%d",index,__LINE__);
    } 
    else 
    {
        nCameraBuffersQueued++;
    }

    return;
}

sp<IMemoryHeap>  CameraHal::getRawHeap() const
{
    return mPictureHeap;
}


status_t CameraHal::takePicture( )
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = PREVIEW_CAPTURE;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME_EXIT

#endif

    return NO_ERROR;
}

status_t CameraHal::cancelPicture( )
{
    LOG_FUNCTION_NAME
	disableMsgType(CAMERA_MSG_RAW_IMAGE);
	disableMsgType(CAMERA_MSG_COMPRESSED_IMAGE);
//	mCallbackCookie = NULL;   // done in destructor

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

status_t CameraHal::convertGPSCoord(double coord, int *deg, int *min, int *sec)
{
    double tmp;
    
    LOG_FUNCTION_NAME

    if ( coord == 0 ) {
        
        LOGE("Invalid GPS coordinate");
        
        return EINVAL;
    }

    *deg = (int) floor(coord);
    tmp = ( coord - floor(coord) )*60;
    *min = (int) floor(tmp);
    tmp = ( tmp - floor(tmp) )*60;
    *sec = (int) floor(tmp);

    if( *sec >= 60 ) {
        *sec = 0;
        *min += 1;
    }

    if( *min >= 60 ) {
        *min = 0;
        *deg += 1;
    }

    LOG_FUNCTION_NAME_EXIT
    
    return NO_ERROR;
}

status_t CameraHal::setParameters(const CameraParameters &params)
{
    LOG_FUNCTION_NAME

    int w, h;
    int w_orig, h_orig, rot_orig;
    int framerate;
    int iso, af, mode, zoom, wb, exposure, scene;
    int effects, compensation, saturation, sharpness;
    int contrast, brightness, flash, caf;
	int error;
	int base;
    const char *valstr;
    char *af_coord;
    Message msg;

    Mutex::Autolock lock(mLock);
      
    LOGD("PreviewFormat %s", params.getPreviewFormat());

    if ( params.getPreviewFormat() != NULL ) {
        if (strcmp(params.getPreviewFormat(), (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) != 0) {
            LOGE("Only yuv422i preview is supported");
            return -1;
        }
    }
    
    LOGD("PictureFormat %s", params.getPictureFormat());
    if ( params.getPictureFormat() != NULL ) {
        if (strcmp(params.getPictureFormat(), (const char *) CameraParameters::PIXEL_FORMAT_JPEG) != 0) {
            LOGE("Only jpeg still pictures are supported");
            return -1;
        }
    }

    params.getPreviewSize(&w, &h);
    if (!validateSize(w, h)) {
        LOGE("Preview size not supported");
        return -1;
    }
    LOGD("PreviewResolution by App %d x %d", w, h);

    params.getPictureSize(&w, &h);
    if (!validateSize(w, h)) {
        LOGE("Picture size not supported");
        return -1;
    }
    LOGD("Picture Size by App %d x %d", w, h);

#ifdef HARDWARE_OMX
    mExifParams.width = w;
    mExifParams.height = h;
#endif

    framerate = params.getPreviewFrameRate();
    LOGD("FRAMERATE %d", framerate);

    rot_orig = rotation;
    rotation = params.getInt(CameraParameters::KEY_ROTATION);

    mParameters.getPictureSize(&w_orig, &h_orig);

    mParameters = params;

#ifdef IMAGE_PROCESSING_PIPELINE

    if((mippMode != mParameters.getInt("ippMode")) || (w != w_orig) || (h != h_orig) ||
            ((rot_orig != 180) && (rotation == 180)) ||
            ((rot_orig == 180) && (rotation != 180))) // in the current setup, IPP uses a different setup when rotation is 180 degrees
    {
        if(pIPP.hIPP != NULL){
            LOGD("pIPP.hIPP=%p", pIPP.hIPP);
            if(DeInitIPP(mippMode)) // deinit here to save time
                LOGE("ERROR DeInitIPP() failed");
            pIPP.hIPP = NULL;
        }

        mippMode = mParameters.getInt("ippMode");
        LOGD("mippMode=%d", mippMode);

        mIPPToEnable = true;
    }

#endif

	mParameters.getPictureSize(&w, &h);
	LOGD("Picture Size by CamHal %d x %d", w, h);

	mParameters.getPreviewSize(&w, &h);
	LOGD("Preview Resolution by CamHal %d x %d", w, h);

    quality = params.getInt(CameraParameters::KEY_JPEG_QUALITY);
    if ( ( quality < 0 ) || (quality > 100) ){
        quality = 100;
    } 

    zoom = mParameters.getInt("zoom");
    if( (zoom >= 0) && ( zoom <= ZOOM_STAGES) ){
        mZoomTargetIdx = zoom;
    } else {
        mZoomTargetIdx = 0;
    }
    LOGD("Zoom by App %d", zoom);
#ifdef HARDWARE_OMX
    mExifParams.zoom = zoom;
#endif
    if ( ( params.get(CameraParameters::KEY_GPS_LATITUDE) != NULL ) && ( params.get(CameraParameters::KEY_GPS_LONGITUDE) != NULL ) && ( params.get(CameraParameters::KEY_GPS_ALTITUDE) != NULL )) {

        double gpsCoord;

#ifdef HARDWARE_OMX
        if( NULL == gpsLocation )
            gpsLocation = (gps_data *) malloc( sizeof(gps_data));

        if( NULL != gpsLocation ) {
            LOGE("initializing gps_data structure");

            memset(gpsLocation, 0, sizeof(gps_data));

            gpsCoord = strtod( params.get(CameraParameters::KEY_GPS_LATITUDE), NULL);
            convertGPSCoord(gpsCoord, &gpsLocation->latDeg, &gpsLocation->latMin, &gpsLocation->latSec);
            
            gpsCoord = strtod( params.get(CameraParameters::KEY_GPS_LONGITUDE), NULL);
            convertGPSCoord(gpsCoord, &gpsLocation->longDeg, &gpsLocation->longMin, &gpsLocation->longSec);

            gpsCoord = strtod( params.get(CameraParameters::KEY_GPS_ALTITUDE), NULL);
            gpsLocation->altitude = gpsCoord;

            if ( NULL != params.get(CameraParameters::KEY_GPS_TIMESTAMP) )
                gpsLocation->timestamp = strtol( params.get(CameraParameters::KEY_GPS_TIMESTAMP), NULL, 10);

            gpsLocation->altitudeRef = params.getInt(KEY_GPS_ALTITUDE_REF);
            gpsLocation->latRef = (char *) params.get(KEY_GPS_LATITUDE_REF);
            gpsLocation->longRef = (char *) params.get(KEY_GPS_LONGITUDE_REF);
            gpsLocation->mapdatum = (char *) params.get(KEY_GPS_MAPDATUM);
            gpsLocation->versionId = (char *) params.get(KEY_GPS_VERSION);

        } else {
            LOGE("Not enough memory to allocate gps_data structure");
        }
#endif
    }

    if ( params.get(KEY_SHUTTER_ENABLE) != NULL ) {
        if ( strcmp(params.get(KEY_SHUTTER_ENABLE), (const char *) "true") == 0 )
            mShutterEnable = true;
        else if ( strcmp(params.get(KEY_SHUTTER_ENABLE), (const char *) "false") == 0 )
            mShutterEnable = false;
    }

#ifdef FW3A

    if ( NULL != fobj ){

        if ( params.get("meter-mode") != NULL ) {
            if (strcmp(params.get("meter-mode"), (const char *) "center") == 0) {
                    fobj->settings_2a.af.spot_weighting = FOCUS_SPOT_SINGLE_CENTER;
                    fobj->settings_2a.ae.spot_weighting = EXPOSURE_SPOT_CENTER;
                    mExifParams.metering_mode = EXIF_CENTER;
            } else if (strcmp(params.get("meter-mode"), (const char *) "average") == 0) {
                fobj->settings_2a.af.spot_weighting = FOCUS_SPOT_MULTI_AVERAGE;
                fobj->settings_2a.ae.spot_weighting = EXPOSURE_SPOT_WIDE;
                 mExifParams.metering_mode = EXIF_AVERAGE;
            }
        } 

        if ( params.get("capture") != NULL ) {
            if (strcmp(params.get("capture"), (const char *) "still") == 0) {
                //Set 3A config to enable variable fps
                fobj->settings_2a.ae.framerate = 0;
            } else if (strcmp(params.get("capture"), (const char *) "video") == 0) {
                //Set 3A config to disable variable fps
                fobj->settings_2a.ae.framerate = framerate;
            }
        }else {
            //disable variable fps by default
            fobj->settings_2a.ae.framerate = framerate;
        }

        if ( params.get(CameraParameters::KEY_SCENE_MODE) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_AUTO) == 0) {
                fobj->settings_2a.general.scene = CONFIG_SCENE_MODE_MANUAL;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_PORTRAIT) == 0) {

                fobj->settings_2a.general.scene = CONFIG_SCENE_MODE_PORTRAIT;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_LANDSCAPE) == 0) {

                fobj->settings_2a.general.scene = CONFIG_SCENE_MODE_LANDSCAPE;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_NIGHT) == 0) {

                fobj->settings_2a.general.scene = CONFIG_SCENE_MODE_NIGHT;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_NIGHT_PORTRAIT) == 0) {

                fobj->settings_2a.general.scene = CONFIG_SCENE_MODE_NIGHT_PORTRAIT;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_FIREWORKS) == 0) {

                fobj->settings_2a.general.scene = CONFIG_SCENE_MODE_FIREWORKS;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_SPORTS) == 0) {

                fobj->settings_2a.general.scene = CONFIG_SCENE_MODE_SPORT;

            }
        }

        if ( params.get(CameraParameters::KEY_WHITE_BALANCE) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_AUTO ) == 0) {

                fobj->settings_2a.awb.mode = WHITE_BALANCE_MODE_WB_AUTO;
                 mExifParams.wb = EXIF_WB_AUTO;
            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_INCANDESCENT) == 0) {

                 fobj->settings_2a.awb.mode = WHITE_BALANCE_MODE_WB_INCANDESCENT;

            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_FLUORESCENT) == 0) {

                fobj->settings_2a.awb.mode = WHITE_BALANCE_MODE_WB_FLUORESCENT;

            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_DAYLIGHT) == 0) {

                fobj->settings_2a.awb.mode = WHITE_BALANCE_MODE_WB_DAYLIGHT;

            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_SHADE) == 0) {

                fobj->settings_2a.awb.mode = WHITE_BALANCE_MODE_WB_SHADOW;

            }
        }

        if ( params.get(CameraParameters::KEY_EFFECT) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_NONE ) == 0) {

                fobj->settings_2a.general.effects = CONFIG_EFFECT_NORMAL;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_MONO) == 0) {

                fobj->settings_2a.general.effects = CONFIG_EFFECT_GRAYSCALE;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_NEGATIVE) == 0) {

                fobj->settings_2a.general.effects = CONFIG_EFFECT_NEGATIVE;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_SOLARIZE) == 0) {

                fobj->settings_2a.general.effects = CONFIG_EFFECT_SOLARIZE;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_SEPIA) == 0) {

                fobj->settings_2a.general.effects = CONFIG_EFFECT_SEPIA;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_WHITEBOARD) == 0) {

                fobj->settings_2a.general.effects = CONFIG_EFFECT_WHITEBOARD;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_BLACKBOARD) == 0) {

                fobj->settings_2a.general.effects = CONFIG_EFFECT_BLACKBOARD;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) EFFECT_COOL) == 0) {

                fobj->settings_2a.general.effects = CONFIG_EFFECT_COOL;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) EFFECT_EMBOSS) == 0) {

                fobj->settings_2a.general.effects = CONFIG_EFFECT_EMBOSS;

            }
        }

        if ( params.get(CameraParameters::KEY_ANTIBANDING) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_ANTIBANDING), (const char *) CameraParameters::ANTIBANDING_50HZ ) == 0) {

                fobj->settings_2a.general.flicker_avoidance = CONFIG_FLICKER_AVOIDANCE_50HZ;

            } else if (strcmp(params.get(CameraParameters::KEY_ANTIBANDING), (const char *) CameraParameters::ANTIBANDING_60HZ) == 0) {

                fobj->settings_2a.general.flicker_avoidance = CONFIG_FLICKER_AVOIDANCE_60HZ;

            } else if (strcmp(params.get(CameraParameters::KEY_ANTIBANDING), (const char *) CameraParameters::ANTIBANDING_OFF) == 0) {

                fobj->settings_2a.general.flicker_avoidance = CONFIG_FLICKER_AVOIDANCE_NO;

            }
        }

        if ( params.get(CameraParameters::KEY_FOCUS_MODE) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) CameraParameters::FOCUS_MODE_AUTO ) == 0) {

                fobj->settings_2a.af.focus_mode = FOCUS_MODE_AF_AUTO;

            } else if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) CameraParameters::FOCUS_MODE_INFINITY) == 0) {

                fobj->settings_2a.af.focus_mode = FOCUS_MODE_AF_INFINY;

            } else if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) CameraParameters::FOCUS_MODE_MACRO) == 0) {

                fobj->settings_2a.af.focus_mode = FOCUS_MODE_AF_MACRO;

            } else if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) CameraParameters::FOCUS_MODE_FIXED) == 0) {

                fobj->settings_2a.af.focus_mode = FOCUS_MODE_AF_MANUAL;

            }
        }

        valstr = mParameters.get("touch-focus");
        if( NULL != valstr ){
            if ( strcmp(valstr, (const char *) "disabled") != 0) {

                int af_x = 0;
                int af_y = 0;

                af_coord = strtok((char *) valstr, PARAMS_DELIMITER);

                if( NULL != af_coord){
                    af_x = atoi(af_coord);
                }

                af_coord = strtok(NULL, PARAMS_DELIMITER);

                if( NULL != af_coord){
                    af_y = atoi(af_coord);
                }

                fobj->settings_2a.general.face_tracking.enable = 1;
                fobj->settings_2a.general.face_tracking.count = 1;
                fobj->settings_2a.general.face_tracking.update = 1;
                fobj->settings_2a.general.face_tracking.faces[0].top = af_y;
                fobj->settings_2a.general.face_tracking.faces[0].left = af_x;
                fobj->settings_2a.af.focus_mode = FOCUS_MODE_AF_EXTENDED;

                LOGD("NEW PARAMS: af_x = %d, af_y = %d", af_x, af_y);
            }
        }


        iso = mParameters.getInt("iso");

        switch ( iso ) {
            case EXPOSURE_ISO_AUTO:
                mExifParams.iso = EXIF_ISO_AUTO;
                break;
            case EXPOSURE_ISO_100:
                mExifParams.iso = EXIF_ISO_100;
                break;
            case EXPOSURE_ISO_200:
                mExifParams.iso = EXIF_ISO_200;
                break;
            case EXPOSURE_ISO_400:
                mExifParams.iso = EXIF_ISO_400;
                break;
        };

        mcapture_mode = mParameters.getInt("mode");        
        exposure = mParameters.getInt("exposure");
        compensation = mParameters.getInt("compensation");
        saturation = mParameters.getInt("saturation");
        sharpness = mParameters.getInt("sharpness");
        contrast = mParameters.getInt("contrast");
        brightness = mParameters.getInt("brightness");
        caf = mParameters.getInt("caf");

        if(contrast != -1)
        {
            contrast -= CONTRAST_OFFSET;
            fobj->settings_2a.general.contrast = contrast;
        }

        if(brightness != -1) 
        {
            brightness -= BRIGHTNESS_OFFSET;
            fobj->settings_2a.general.brightness = brightness;
        }

        if(saturation != -1)
            fobj->settings_2a.general.saturation = saturation;
        
        if(sharpness != -1)
            fobj->settings_2a.general.sharpness = sharpness;

        if(iso != -1)
            fobj->settings_2a.ae.iso = (EXPOSURE_ISO_VALUES) iso;

        if(exposure != -1)
            fobj->settings_2a.ae.mode = (EXPOSURE_MODE_VALUES) exposure;

        if(compensation != -1) {

            compensation -= COMPENSATION_OFFSET;
            fobj->settings_2a.ae.compensation = compensation;

        }

        FW3A_SetSettings();
        LOGD("mcapture_mode = %d", mcapture_mode);

        if(mParameters.getInt(KEY_ROTATION_TYPE) == ROTATION_EXIF) {
            mExifParams.rotation = rotation;
            rotation = 0; // reset rotation so encoder doesn't not perform any rotation
        } else {
            mExifParams.rotation = -1;
        }

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

#ifdef FW3A

int CameraHal::onSnapshot(void *priv, void *buf, int width, int height, capture_rect_t snapshot_rect, capture_rect_t aspect_rect)
{
    unsigned int snapshotMessage[9];

    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOG_FUNCTION_NAME

    snapshotMessage[0] = SNAPSHOT_THREAD_START;
    snapshotMessage[1] = (unsigned int) buf;
    snapshotMessage[2] = width;
    snapshotMessage[3] = __ALIGN(height, 16);
    snapshotMessage[4] = camHal->mZoomTargetIdx;
    snapshotMessage[5] = snapshot_rect.top;
    snapshotMessage[6] = snapshot_rect.left;
    snapshotMessage[7] = snapshot_rect.width;
    snapshotMessage[8] = snapshot_rect.height;

    write(camHal->snapshotPipe[1], &snapshotMessage, sizeof(snapshotMessage));

    LOG_FUNCTION_NAME_EXIT

    return 0;
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

#endif

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

/*--------------------Eclair HAL---------------------------------------*/
void CameraHal::setCallbacks(notify_callback notify_cb,
                                      data_callback data_cb,
                                      data_callback_timestamp data_cb_timestamp,
                                      void* user)
{
    Mutex::Autolock lock(mLock);
    mNotifyCb = notify_cb;
    mDataCb = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mCallbackCookie = user;
}

void CameraHal::enableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(mLock);
    mMsgEnabled |= msgType;
}

void CameraHal::disableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(mLock);
    mMsgEnabled &= ~msgType;
}

bool CameraHal::msgTypeEnabled(int32_t msgType)
{
    Mutex::Autolock lock(mLock);
    return (mMsgEnabled & msgType);
}

status_t CameraHal::cancelAutoFocus()
{
    return NO_ERROR;
}

/*--------------------Eclair HAL---------------------------------------*/

status_t CameraHal::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2)
{
    Message msg;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    switch(cmd) {
        case CAMERA_CMD_START_SMOOTH_ZOOM:
            msg.command = START_SMOOTH_ZOOM;
            msg.arg1 = ( void * ) arg1;
            previewThreadCommandQ.put(&msg);
            previewThreadAckQ.get(&msg);

            if ( PREVIEW_ACK != msg.command ) {
                ret = -EINVAL;
            }

            break;
        case CAMERA_CMD_STOP_SMOOTH_ZOOM:
            msg.command = STOP_SMOOTH_ZOOM;
            previewThreadCommandQ.put(&msg);
            previewThreadAckQ.get(&msg);

            if ( PREVIEW_ACK != msg.command ) {
                ret = -EINVAL;
            }

            break;
        default:
            break;
    };

    LOG_FUNCTION_NAME_EXIT

    return ret;
}


extern "C" sp<CameraHardwareInterface> openCameraHardware()
{
    LOGD("opening ti camera hal\n");
    return CameraHal::createInstance();
}


}; // namespace android

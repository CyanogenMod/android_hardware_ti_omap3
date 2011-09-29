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

//#define LOG_NDEBUG 0
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
wp<CameraHardwareInterface> CameraHal::singleton[];
const char CameraHal::supportedPictureSizes [] = "3264x2448,2560x2048,2048x1536,1600x1200,1280x1024,1152x968,1280x960,800x600,640x480,320x240";
const char CameraHal::supportedPictureSizesSecondary [] = "640x480";
const char CameraHal::supportedPreviewSizes [] = "1280x720,992x560,864x480,800x480,720x576,720x480,768x576,640x480,320x240,352x288,240x160,176x144,128x96";
const char CameraHal::supportedPreviewSizesSecondary [] = "800x480,720x576,720x480,768x576,640x480,320x240,352x288,240x160,176x144,128x96";
const char CameraHal::supportedFPS [] = "33,30,25,24,20,15,10";
const char CameraHal::supportedThumbnailSizes []= "320x240,80x60,0x0";
const char CameraHal::supportedFpsRanges [] = "(8000,8000),(8000,10000),(10000,10000),(8000,15000),(15000,15000),(8000,20000),(20000,20000),(24000,24000),(25000,25000),(8000,30000),(30000,30000)";
const char CameraHal::PARAMS_DELIMITER []= ",";

const supported_resolution CameraHal::supportedPictureRes[] = { {3264, 2448} , {2560, 2048} ,
                                                     {2048, 1536} , {1600, 1200} ,
                                                     {1280, 1024} , {1152, 968} ,
                                                     {1280, 960} , {800, 600},
                                                     {640, 480}   , {320, 240} };

const supported_resolution CameraHal::supportedPreviewRes[] = { {1280, 720}, {800, 480},
                                                     {720, 576}, {720, 480},
                                                     {992, 560}, {864, 480}, {848, 480},
                                                     {768, 576}, {640, 480},
                                                     {320, 240}, {352, 288}, {240, 160},
                                                     {176, 144}, {128, 96}};

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

CameraHal::CameraHal(int cameraId)
                     :mParameters(),
                     mOverlay(NULL),
                     mPreviewRunning(0),
                     mRecordingFrameSize(0),
                     mVideoBufferCount(0),
                     nOverlayBuffersQueued(0),
                     nCameraBuffersQueued(0),
                     mfirstTime(0),
                     pictureNumber(0),
                     mCaptureRunning(0),
#ifdef FW3A
                     fobj(NULL),
#endif
                     file_index(0),
                     mflash(2),
                     mcapture_mode(1),
                     mcaf(0),
                     j(0),
                     useFramerateRange(0)
{
#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
    gettimeofday(&ppm_start, NULL);
#endif

    mCameraIndex = cameraId;
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
    mZoomSpeed = 0;
    mZoomTargetIdx = 0;
    mZoomCurrentIdx = 0;
    mSmoothZoomStatus = SMOOTH_STOP;
    rotation = 0;

#ifdef HARDWARE_OMX

    gpsLocation = NULL;

#endif

    // Disable ISP resizer on overlay (use DSS resizer)
    system("echo manual > "
            "/sys/devices/platform/omap_vout/video4linux/video1/isprsz_mode");

    mShutterEnable = true;
    sStart_FW3A_CAF:mCAFafterPreview = false;
    ancillary_len = 8092;

#ifdef IMAGE_PROCESSING_PIPELINE

    pIPP.hIPP = NULL;

#endif

    int i = 0;
    for(i = 0; i < VIDEO_FRAME_COUNT_MAX; i++) {
        mVideoBuffer[i] = 0;
        mVideoBufferStatus[i] = BUFF_IDLE;
    }

    for (i = 0; i < MAX_BURST; i++) {
        mYuvBuffer[i] = 0;
        mYuvBufferLen[i] = 0;
    }

    CameraCreate();

    initDefaultParameters();

    /* Avoiding duplicate call of cameraconfigure(). It is now called in previewstart() */
    //CameraConfigure();

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

bool CameraHal::validateSize(size_t width, size_t height, const supported_resolution *supRes, size_t count)
{
    bool ret = false;
    status_t stat = NO_ERROR;
    unsigned int size;

    LOG_FUNCTION_NAME

    if ( NULL == supRes ) {
        LOGE("Invalid resolutions array passed");
        stat = -EINVAL;
    }

    if ( NO_ERROR == stat ) {
        for ( unsigned int i = 0 ; i < count; i++ ) {
            LOGD( "Validating %d, %d and %d, %d", supRes[i].width, width, supRes[i].height, height);
            if ( ( supRes[i].width == width ) && ( supRes[i].height == height ) ) {
                ret = true;
                break;
            }
        }
    }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

bool CameraHal::validateRange(int min, int max, const char *supRang)
{
    bool ret = false;
    char * myRange = NULL;
    char supRang_copy[strlen(supRang)];
    int myMin = 0;
    int myMax = 0;

    LOG_FUNCTION_NAME

    if ( NULL == supRang ) {
        LOGE("Invalid range array passed");
        return ret;
    }

    //make a copy of supRang
    strcpy(supRang_copy, supRang);
    LOGE("Range: %s", supRang_copy);

    myRange = strtok((char *) supRang_copy, ",");
    if (NULL != myRange) {
        myMin = atoi(myRange + 1);
    }

    myRange = strtok(NULL, ",");
    if (NULL != myRange) {
        myRange[strlen(myRange)]='\0';
        myMax = atoi(myRange);
    }

    LOGE("Validating range: %d,%d with %d,%d", myMin, myMax, min, max);

    if ( ( myMin == min )&&( myMax == max ) ) {
        LOGE("Range supported!");
        return true;
    }

    for (;;) {
        myRange = strtok(NULL, ",");
        if (NULL != myRange) {
            myMin = atoi(myRange + 1);
        }
        else break;

        myRange = strtok(NULL, ",");
        if (NULL != myRange) {
            myRange[strlen(myRange)]='\0';
            myMax = atoi(myRange);
        }
        else break;
        LOGE("Validating range: %d,%d with %d,%d", myMin, myMax, min, max);
        if ( ( myMin == min )&&( myMax == max ) ) {
            LOGE("Range found");
            ret = true;
            break;
        }
    }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

void CameraHal::initDefaultParameters()
{
    CameraParameters p;
    char tmpBuffer[PARAM_BUFFER], zoomStageBuffer[PARAM_BUFFER];
    unsigned int zoomStage;

    LOG_FUNCTION_NAME

    p.setPreviewSize(MIN_WIDTH, MIN_HEIGHT);

    //We must initialize both framerate and fps range.
    //Application will decide which to use.
    //If application does not decide, framerate will be used in CameraHAL
    char fpsRange[32];
    sprintf(fpsRange, "%d,%d", 30000, 30000);
    p.set(KEY_PREVIEW_FPS_RANGE, fpsRange);
    p.setPreviewFrameRate(30);

    p.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV422I);

    p.setPictureSize(PICTURE_WIDTH, PICTURE_HEIGHT);
    p.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
    p.set(CameraParameters::KEY_JPEG_QUALITY, 100);

    //extended parameters

    memset(tmpBuffer, '\0', PARAM_BUFFER);
    for ( int i = 0 ; i < ZOOM_STAGES ; i++ ) {
        zoomStage =  (unsigned int ) ( zoom_step[i]*PARM_ZOOM_SCALE );
        snprintf(zoomStageBuffer, PARAM_BUFFER, "%d", zoomStage);

        if(camerahal_strcat((char*) tmpBuffer, (const char*) zoomStageBuffer, PARAM_BUFFER)) return;
        if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    }
    p.set(CameraParameters::KEY_ZOOM_RATIOS, tmpBuffer);
    p.set(CameraParameters::KEY_ZOOM_SUPPORTED, "true");
    p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, "true");
    // zoom goes from 0..MAX_ZOOM so send array size minus one
    p.set(CameraParameters::KEY_MAX_ZOOM, ZOOM_STAGES-1);
    p.set(CameraParameters::KEY_ZOOM, 0);

    p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, COMPENSATION_MAX);
    p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, COMPENSATION_MIN);
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, COMPENSATION_STEP);
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, 0);

    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, mCameraIndex ? CameraHal::supportedPictureSizesSecondary : CameraHal::supportedPictureSizes);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, CameraHal::supportedFpsRanges);
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, CameraParameters::PIXEL_FORMAT_JPEG);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, mCameraIndex ? CameraHal::supportedPreviewSizesSecondary : CameraHal::supportedPreviewSizes);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, CameraParameters::PIXEL_FORMAT_YUV422I);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, CameraHal::supportedFPS);
    p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, CameraHal::supportedThumbnailSizes);
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, STRINGIZE(DEFAULT_THUMB_WIDTH));
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, STRINGIZE(DEFAULT_THUMB_HEIGHT));

    p.set(CameraParameters::KEY_FOCAL_LENGTH, STRINGIZE(IMX046_FOCALLENGTH));
    p.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, STRINGIZE(IMX046_HORZANGLE));
    p.set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, STRINGIZE(IMX046_VERTANGLE));

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
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) WHITE_BALANCE_HORIZON, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) WHITE_BALANCE_TUNGSTEN, PARAM_BUFFER)) return;
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
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_ACTION, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_NIGHT_PORTRAIT, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_FIREWORKS, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_NIGHT, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_SNOW, PARAM_BUFFER)) return;
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
    if(camerahal_strcat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, PARAM_BUFFER)) return;
    if(camerahal_strcat((char*) tmpBuffer, (const char*) CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO, PARAM_BUFFER)) return;
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

    p.set(CameraParameters::KEY_ROTATION, 0);
    p.set(KEY_ROTATION_TYPE, ROTATION_PHYSICAL);
    //set the video frame format needed by video capture framework
    p.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV422I);

    //Set focus distances near=0.5, optimal=1.5, far="Infinity"
    //Once when fw3A supports focus distances, update them in CameraHal::GetParameters()
    sprintf(CameraHal::focusDistances, "%f,%f,%s", FOCUS_DISTANCE_NEAR, FOCUS_DISTANCE_OPTIMAL, CameraParameters::FOCUS_DISTANCE_INFINITY);
    p.set(CameraParameters::KEY_FOCUS_DISTANCES, CameraHal::focusDistances);
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, 100);

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

    freePictureBuffers();

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

    singleton[mCameraIndex].clear();

    // Re-enable ISP resizer on overlay for 720P playback
    system("echo auto > "
            "/sys/devices/platform/omap_vout/video4linux/video1/isprsz_mode");

    LOGD("<<< Release");
}

void CameraHal::previewThread()
{
    Message msg;
    int parm;
    bool  shouldLive = true;
    bool has_message;
    int err;
    int flg_AF = 0;
    int flg_CAF = 0;
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

            if ( isStart_FW3A_AF ) {
                err = ICam_ReadStatus(fobj->hnd, &fobj->status);
                //ICAM_AF_STATUS_IDLE is the state when AF algorithm is not working,
                //but waiting for the lens to go to start position.
                //In this case, AF is running, so we are waiting for AF to finish like
                //in ICAM_AF_STATUS_RUNNING state.
                if ( (err == 0) && ( ICAM_AF_STATUS_RUNNING != fobj->status.af.status ) && ( ICAM_AF_STATUS_IDLE != fobj->status.af.status ) ) {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

                    PPM("AF Completed in ",&focus_before);

#endif

                    ICam_ReadMakerNote(fobj->hnd, &fobj->mnote);

                    if (FW3A_Stop_AF() < 0){
                        LOGE("ERROR FW3A_Stop_AF()");
                    }

                    bool focus_flag;
                    if ( fobj->status.af.status == ICAM_AF_STATUS_SUCCESS ) {
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

                if ( mCaptureRunning ) {

#ifdef FW3A

                    if ( flg_CAF ) {
                        if( FW3A_Start_CAF() < 0 ) {
                            LOGE("Error while starting CAF");
                            err = -1;
                        }
                    }

#endif
                }

                else if( ! mPreviewRunning ) {

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

                    if ( !mCameraIndex && CorrectPreview() < 0 )
                        LOGE("Error during CorrectPreview()");

                    if ( CameraStart() < 0 ) {
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
                    } else {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
                        PPM("Shot to Shot", &ppm_receiveCmdToTakePicture);

#endif

                    }
                } else {
                    err = -1;
                }

                LOGD("PREVIEW_START %s", err ? "NACK" : "ACK");
                msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;

                if ( !err ) {
                    LOGD("Preview Started!");
                    mPreviewRunning = true;
                    mCaptureRunning = false;
                    debugShowBufferStatus();
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
                    } else {
                        mcaf = 0;
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

                if ( ( parm >= 0 ) && ( parm < ZOOM_STAGES) ) {
                    mZoomTargetIdx = parm;
                    mZoomSpeed = 1;
                    mSmoothZoomStatus = SMOOTH_START;
                    msg.command = PREVIEW_ACK;
                } else {
                    msg.command = PREVIEW_NACK;
                }

                previewThreadAckQ.put(&msg);

                break;

            case STOP_SMOOTH_ZOOM:

                LOGD("Receive Command: STOP_SMOOTH_ZOOM");
                if(mSmoothZoomStatus == SMOOTH_START)
                {
                    mSmoothZoomStatus = SMOOTH_NOTIFY_AND_STOP;
                }
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
                } else {

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
                    if (isStart_FW3A){
                    if (isStart_FW3A_AF == 0){
                        if( FW3A_Start_AF() < 0){
                            LOGE("ERROR FW3A_Start_AF()");
                            err = -1;
                         }

                    }
                    } else {
                        if(msgTypeEnabled(CAMERA_MSG_FOCUS))
                            mNotifyCb(CAMERA_MSG_FOCUS, true, 0, mCallbackCookie);
                    }

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

            case PREVIEW_AF_STOP:
            {
                LOGD("Receive Command: PREVIEW_AF_STOP");
                err = 0;

                if( !mPreviewRunning ){
                    LOGD("WARNING PREVIEW NOT RUNNING!");
                    msg.command = PREVIEW_NACK;
                } else {

#ifdef FW3A

                    if (isStart_FW3A&&(isStart_FW3A_AF != 0)){
                        if( FW3A_Stop_AF() < 0){
                            LOGE("ERROR FW3A_Stop_AF()");
                            err = -1;
                        }
                        else
                            isStart_FW3A_AF = 0;
                    }
                    else {
                        err = -1;
                    }

                    msg.command = err ? PREVIEW_NACK : PREVIEW_ACK;

#else

                    msg.command = PREVIEW_ACK;

#endif
                }
                LOGD("Receive Command: PREVIEW_AF_STOP %s", msg.command == PREVIEW_NACK ? "NACK" : "ACK");
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
                if (isStart_FW3A_CAF == 0){
                    if( FW3A_Start_CAF() < 0){
                        LOGE("ERROR FW3A_Start_CAF()");
                        err = -1;
                    }
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

                if ( mCaptureRunning ) {
                    LOGE("Capture is already running. Returning.");
                    msg.command = PREVIEW_NACK;
                    previewThreadAckQ.put(&msg);
                    break;
                }

                err = 0;

#ifdef DEBUG_LOG

                LOGD("ENTER OPTION PREVIEW_CAPTURE");

                PPM("RECEIVED COMMAND TO TAKE A PICTURE");

#endif

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
                gettimeofday(&ppm_receiveCmdToTakePicture, NULL);
#endif

                // Boost DSP OPP to highest level
                SetDSPKHz(DSP3630_KHZ_MAX);

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
                   } else {
                       mcaf = 0;
                   }

                   if ( ICam_ReadStatus(fobj->hnd, &fobj->status) < 0 ) {
                   LOGE("ICam_ReadStatus failed");
                   err = -1;
                   }

                   if ( ICam_ReadMakerNote(fobj->hnd, &fobj->mnote) < 0 ) {
                   LOGE("ICam_ReadMakerNote failed");
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

               if ( !mCameraIndex && CorrectPreview() < 0 )
                   LOGE("Error during CorrectPreview()");

               if (CameraStart() < 0)
                   LOGE("ERROR CameraStart()");

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

               PPM("Capture mode switch", &ppm_restartPreview);
               PPM("Shot to Shot", &ppm_receiveCmdToTakePicture);

#endif

               mCaptureRunning = true;

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

    camera_device = open(mCameraIndex ? "/dev/video0" : VIDEO_DEVICE, O_RDWR);
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
            mVideoBufferStatus[i] = BUFF_IDLE;
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
    int framerate_min = 0, framerate_max = 0;
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

    if (useFramerateRange) {
        mParameters.getPreviewFpsRange(&framerate_min, &framerate_max);
        LOGD("CameraConfigure: framerate to set: min = %d, max = %d",framerate_min, framerate_max);
    }
    else {
        framerate_max = mParameters.getPreviewFrameRate();
        LOGD("CameraConfigure: framerate to set = %d", framerate_max);
    }

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

    if (useFramerateRange) {
        //Gingerbread changes for FPS - set range of min and max fps
        if ( (MIN_FPS*1000 <= framerate_min)&&(framerate_min <= framerate_max)&&(framerate_max<=MAX_FPS*1000) ) {
            parm.parm.capture.timeperframe.denominator = framerate_max/1000;
        }
        else parm.parm.capture.timeperframe.denominator = 30;
    }
    else {
        if ( framerate_max != 0 ) parm.parm.capture.timeperframe.denominator = framerate_max;
        else  parm.parm.capture.timeperframe.denominator  = 30;
    }

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
        v4l2_cam_buffer[i].length = data->length;

        strcpy((char *)v4l2_cam_buffer[i].m.userptr, "hello");
        if (strcmp((char *)v4l2_cam_buffer[i].m.userptr, "hello")) {
            LOGI("problem with buffer\n");
            goto fail_loop;
        }

        LOGD("User Buffer [%d].start = %p  length = %d\n", i,
             (void*)v4l2_cam_buffer[i].m.userptr, v4l2_cam_buffer[i].length);

        if (mVideoBufferStatus[i] == BUFF_IDLE) {
            if (false == queueToCamera(i))
                goto fail_loop;
        } else {
            LOGI("CameraStart::Could not queue buffer %d to Camera because it is being held by Overlay", i);
        }

        // ensure we release any stale ref's to sp
        mPreviewBuffers[i].clear();
        mPreviewHeaps[i].clear();

        mPreviewHeaps[i] = new MemoryHeapBase(data->fd,mPreviewFrameSize, 0, data->offset);
        mPreviewBuffers[i] = new MemoryBase(mPreviewHeaps[i], 0, mPreviewFrameSize);
    }

    if( !mCameraIndex && ioctl(camera_device, VIDIOC_G_CROP, &mInitialCrop) < 0 ){
        LOGE("[%s]: ERROR VIDIOC_G_CROP failed", strerror(errno));
        return -1;
    }

    LOGE("Initial Crop: crop_top = %d, crop_left = %d, crop_width = %d, crop_height = %d", mInitialCrop.c.top, mInitialCrop.c.left, mInitialCrop.c.width, mInitialCrop.c.height);

    if ( mZoomTargetIdx != mZoomCurrentIdx ) {

        if( ZoomPerform(zoom_step[mZoomTargetIdx]) < 0 )
            LOGE("Error while applying zoom");

        mZoomCurrentIdx = mZoomTargetIdx;
        mParameters.set("zoom", (int) mZoomCurrentIdx);
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

    nCameraBuffersQueued = 0;

    struct v4l2_requestbuffers creqbuf;
    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) == -1) {
        LOGE("VIDIOC_STREAMOFF Failed");
        return -1;
    }

    //Force the zoom to be updated next time preview is started.
    mZoomCurrentIdx = 0;

    LOG_FUNCTION_NAME_EXIT

    return 0;
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

void CameraHal::queueToOverlay(int index)
{
    int nBuffers_queued_to_dss = mOverlay->queueBuffer((void*)index);
    if (nBuffers_queued_to_dss < 0) {
        LOGE("Failed queue buffer#%d to overlay! Queue it back to camera.", index);
        debugShowBufferStatus();
        queueToCamera(index);
        return;
    }

    nOverlayBuffersQueued++;
    mVideoBufferStatus[index] |= BUFF_Q2DSS;

    if (nBuffers_queued_to_dss == nOverlayBuffersQueued) {
        // No error.
        return;
    }

    LOGW("Found some buffers discarded by DSS upon STREAM OFF!");
    LOGD("nOverlayBuffersQueued=%d, nBuffers_queued_to_dss=%d",
            nOverlayBuffersQueued, nBuffers_queued_to_dss);
    debugShowBufferStatus();
    //Queue all the buffers that were discarded by DSS upon STREAM OFF, back to camera.
    for(int k = 0; k < MAX_CAMERA_BUFFERS; k++) {
        if (k == index)
            continue;

        if (mVideoBufferStatus[k] & BUFF_Q2DSS) {
            mVideoBufferStatus[k] &= ~BUFF_Q2DSS;
            nOverlayBuffersQueued--;

            queueToCamera(k);
        }
    }
}

int CameraHal::dequeueFromOverlay()
{
    overlay_buffer_t overlaybuffer;// contains the index of the buffer dque
    if (nOverlayBuffersQueued < NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE) {
        LOGV("skip dequeue. nOverlayBuffersQueued = %d", nOverlayBuffersQueued);
        return -1;
    }

    int dequeue_from_dss_failed = mOverlay->dequeueBuffer(&overlaybuffer);
    if(dequeue_from_dss_failed) {
        LOGD("no buffer to dequeue in overlay");
        return -1;
    }

    nOverlayBuffersQueued--;
    mVideoBufferStatus[(int)overlaybuffer] &= ~BUFF_Q2DSS;
    lastOverlayBufferDQ = (int)overlaybuffer;

    return (int)overlaybuffer;
}

bool CameraHal::__queueToCamera(int index, int line)
{
    if (0 > index || MAX_CAMERA_BUFFERS < index) {
        LOGE("wrong index %d, line=%d", index, line);
        return false;
    }

    if (mVideoBufferStatus[index] != BUFF_IDLE) {
        LOGW("ignore trying queue non-idle buffer#%d(stat=%d) to camera. line=%d",
                index, mVideoBufferStatus[index], line);
        return false;
    }

    if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[index]) < 0) {
        LOGE("VIDIOC_QBUF Failed. buffer#%d(stat=%d), line=%d", index, mVideoBufferStatus[index],line);
        return false;
    }

    nCameraBuffersQueued++;
    return true;
}

int CameraHal::dequeueFromCamera(nsecs_t *timestamp)
{
    struct v4l2_buffer cfilledbuffer;
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;

    if (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
        LOGE("VIDIOC_DQBUF Failed!!!");
        return -1;
    }

    nCameraBuffersQueued--;

    int index = cfilledbuffer.index;
    if (NULL != timestamp) {
        *timestamp = s2ns(cfilledbuffer.timestamp.tv_sec) + us2ns(cfilledbuffer.timestamp.tv_usec);
    }
    //SaveFile(NULL, (char*)"yuv", (void *)cfilledbuffer.m.userptr, mPreviewFrameSize);

    return index;
}

void CameraHal::nextPreview()
{
    static int frame_count = 0;
    int zoom_inc, err;
    struct timeval lowLightTime;

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

        // Update mParameters with current zoom position only if smooth zoom is used
        // Immediate zoom should not generate callbacks.
        if ( mZoomSpeed > 0 ){
            //Avoid segfault. mParameters may be used somewhere else, e.g. in SetParameters()
            {
                Mutex::Autolock lock(mLock);
                mParameters.set("zoom", mZoomCurrentIdx);
            }

            if ( mSmoothZoomStatus == SMOOTH_START ||  mSmoothZoomStatus == SMOOTH_NOTIFY_AND_STOP)  {
                if(mSmoothZoomStatus == SMOOTH_NOTIFY_AND_STOP) {
                    mZoomTargetIdx = mZoomCurrentIdx;
                    mSmoothZoomStatus = SMOOTH_STOP;
                }
                if( mZoomCurrentIdx == mZoomTargetIdx ){
                    mNotifyCb(CAMERA_MSG_ZOOM, mZoomCurrentIdx, 1, mCallbackCookie);
                } else {
                    mNotifyCb(CAMERA_MSG_ZOOM, mZoomCurrentIdx, 0, mCallbackCookie);
                }
            }
        }
    }

#ifdef FW3A
    if (isStart_FW3A != 0){
    //Low light notification
    if( ( fobj->settings.ae.framerate == 0 ) && ( ( frame_count % 10) == 0 ) ) {
#if ( PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS ) && DEBUG_LOG

        gettimeofday(&lowLightTime, NULL);

#endif
        err = ICam_ReadStatus(fobj->hnd, &fobj->status);
        if (err == 0) {
            if (fobj->status.ae.camera_shake == ICAM_SHAKE_HIGH_RISK2) {

                //Avoid segfault. mParameters may be used somewhere else, e.g. in SetParameters()
                {
                    Mutex::Autolock lock(mLock);
                    mParameters.set("low-light", "1");
                }

            } else {

                //Avoid segfault. mParameters may be used somewhere else, e.g. in SetParameters()
                {
                    Mutex::Autolock lock(mLock);
                    mParameters.set("low-light", "0");
                }

            }
         }

#if ( PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS ) && DEBUG_LOG

        PPM("Low-light delay", &lowLightTime);

#endif

    }
    }
#endif

    nsecs_t timestamp;
    int index = dequeueFromCamera(&timestamp);
    if (-1 == index) {
        return;
    }

    if(msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME))
        mDataCb(CAMERA_MSG_PREVIEW_FRAME,
                mPreviewBuffers[index], mCallbackCookie);

    mRecordingLock.lock();
    if (nCameraBuffersQueued == 0) {
        LOGV("Drop the frame. Camera is starving");
        queueToCamera(index);
        goto queue_and_exit;
    }

    if(mRecordEnabled) {
        mVideoBufferStatus[index] |= BUFF_Q2VE;

        // unlock mRecordingLock before sending frame to CameraService to avoid
        // deadlock when in the same time releaseRecordingFrame is invoked.
        mRecordingLock.unlock();
        mDataCbTimestamp(timestamp, CAMERA_MSG_VIDEO_FRAME,
                mVideoBuffer[index], mCallbackCookie, 0, 0);
        mRecordingLock.lock();
    }

    queueToOverlay(index);

queue_and_exit:
    index = dequeueFromOverlay();
    if(-1 == index) {
        goto exit;
    }

    if (mVideoBufferStatus[index] == BUFF_IDLE)
        queueToCamera(index);

exit:
    mRecordingLock.unlock();

    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
    }

    return;
}

#ifdef ICAP

int  CameraHal::ICapturePerform()
{

    int err;
    int status = 0;
    int jpegSize;
    uint32_t fixedZoom;
    void *outBuffer;
    unsigned long base, offset, jpeg_offset;
    unsigned int preview_width, preview_height;
    icap_buffer_t snapshotBuffer;
    unsigned int image_width, image_height, thumb_width, thumb_height;
    SICap_ManualParameters  manual_config;
    SICam_Settings config;
    icap_tuning_params_t cap_tuning;
    icap_resolution_cap_t spec_res;
    icap_buffer_t capture_buffer;
    icap_process_mode_e mode;
    unsigned int procMessage[PROC_MSG_IDX_MAX],
            shutterMessage[SHUTTER_THREAD_NUM_ARGS],
            rawMessage[RAW_THREAD_NUM_ARGS];
    int pixelFormat;
    exif_buffer *exif_buf;
    mode = ICAP_PROCESS_MODE_CONTINUOUS;

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME

    PPM("START OF ICapturePerform");

#endif

    mParameters.getPictureSize((int *) &image_width, (int *) &image_height);
    mParameters.getPreviewSize((int *) &preview_width,(int *) &preview_height);
    thumb_width = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
    thumb_height = mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);

#ifdef DEBUG_LOG

    LOGD("ICapturePerform image_width=%d image_height=%d",image_width,image_height);

#endif

    memset(&manual_config, 0 ,sizeof(manual_config));
    memset(&iobj->cfg, 0, sizeof(icap_configure_t));

#ifdef DEBUG_LOG

    LOGD("MakerNote 0x%x, size %d", ( unsigned int ) fobj->mnote.buffer,fobj->mnote.filled_size);

    LOGD("shutter_cap = %d ; again_cap = %d ; awb_index = %d; %d %d %d %d\n",
        (int)fobj->status.ae.shutter_cap, (int)fobj->status.ae.again_cap,
        (int)fobj->status.awb.awb_index, (int)fobj->status.awb.gain_Gr,
        (int)fobj->status.awb.gain_R, (int)fobj->status.awb.gain_B,
        (int)fobj->status.awb.gain_Gb);

#endif

    snapshotBuffer.buffer = NULL;
    snapshotBuffer.alloc_size = 0;
    LOGE("mBurstShots %d", mBurstShots);
    if ( 1 >= mBurstShots ) {
        if ( mcapture_mode == 1 ) {
            spec_res.capture_mode = ICAP_CAPTURE_MODE_HP;
            iobj->cfg.capture_mode = ICAP_CAPTURE_MODE_HP;
            iobj->cfg.notify.cb_capture = onSnapshot;
            iobj->cfg.notify.cb_snapshot_crop = onCrop;
            fixedZoom = (uint32_t) (zoom_step[0]*ZOOM_SCALE);

            iobj->cfg.zoom.enable = ICAP_ENABLE;
            iobj->cfg.zoom.vertical = fixedZoom;
            iobj->cfg.zoom.horizontal = fixedZoom;
        } else {
            snapshotBuffer.buffer = getLastOverlayAddress();
            snapshotBuffer.alloc_size = getLastOverlayLength();
            spec_res.capture_mode = ICAP_CAPTURE_MODE_HQ;
            iobj->cfg.capture_mode = ICAP_CAPTURE_MODE_HQ;
            iobj->cfg.notify.cb_snapshot = onGeneratedSnapshot;
            iobj->cfg.snapshot_width = preview_width;
            iobj->cfg.snapshot_height = preview_height;
            fixedZoom = (uint32_t) (zoom_step[0]*ZOOM_SCALE);

            iobj->cfg.zoom.enable = ICAP_ENABLE;
            iobj->cfg.zoom.vertical = fixedZoom;
            iobj->cfg.zoom.horizontal = fixedZoom;
        }
        iobj->cfg.notify.cb_mknote = onMakernote;
#ifdef IMAGE_PROCESSING_PIPELINE
        iobj->cfg.notify.cb_ipp = (mippMode != IPP_Disabled_Mode) ? onIPP : NULL;
#else
        iobj->cfg.notify.cb_ipp = NULL;
#endif
    } else {
        spec_res.capture_mode  = ICAP_CAPTURE_MODE_HP;
        iobj->cfg.capture_mode = ICAP_CAPTURE_MODE_HP;
        iobj->cfg.notify.cb_capture = onSnapshot;
        iobj->cfg.notify.cb_snapshot_crop = onCrop;
        fixedZoom = (uint32_t) (zoom_step[0]*ZOOM_SCALE);

        iobj->cfg.zoom.enable = ICAP_ENABLE;
        iobj->cfg.zoom.vertical = fixedZoom;
        iobj->cfg.zoom.horizontal = fixedZoom;
    }
    iobj->cfg.notify.cb_shutter = onShutter;
    spec_res.res.width = image_width;
    spec_res.res.height = image_height;
    spec_res.capture_format = ICAP_CAPTURE_FORMAT_UYVY;

    status = icap_query_resolution(iobj->lib_private, 0, &spec_res);

    if( ICAP_STATUS_FAIL == status){
        LOGE ("First icapture query resolution failed");

        int tmp_width, tmp_height;
        tmp_width = image_width;
        tmp_height = image_height;
        status = icap_query_enumerate(iobj->lib_private, 0, &spec_res);
        if ( ICAP_STATUS_FAIL == status ) {
            LOGE("icapture enumerate failed");
            goto fail_config;
        }

        if ( image_width  < spec_res.res_range.width_min )
            tmp_width = spec_res.res_range.width_min;

        if ( image_height < spec_res.res_range.height_min )
            tmp_height = spec_res.res_range.height_min;

        if ( image_width > spec_res.res_range.width_max )
            tmp_width = spec_res.res_range.width_max;

        if ( image_height > spec_res.res_range.height_max )
            tmp_height = spec_res.res_range.height_max;

        spec_res.res.width = tmp_width;
        spec_res.res.height = tmp_height;
        status = icap_query_resolution(iobj->lib_private, 0, &spec_res);
        if ( ICAP_STATUS_FAIL == status ) {
            LOGE("icapture query failed again, giving up");
            goto fail_config;
        }
    }

    iobj->cfg.notify.priv   = this;

    iobj->cfg.crop.enable = ICAP_ENABLE;
    iobj->cfg.crop.top = fobj->status.preview.top;
    iobj->cfg.crop.left = fobj->status.preview.left;
    iobj->cfg.crop.width = fobj->status.preview.width;
    iobj->cfg.crop.height = fobj->status.preview.height;

    iobj->cfg.capture_format = ICAP_CAPTURE_FORMAT_UYVY;
    iobj->cfg.width = spec_res.res.width;
    iobj->cfg.height = spec_res.res.height;
    mImageCropTop = 0;
    mImageCropLeft = 0;
    mImageCropWidth = spec_res.res.width;
    mImageCropHeight = spec_res.res.height;

    iobj->cfg.notify.cb_h3a  = NULL; //onSaveH3A;
    iobj->cfg.notify.cb_lsc  = NULL; //onSaveLSC;
    iobj->cfg.notify.cb_raw  = NULL; //onSaveRAW;

#if DEBUG_LOG

    PPM("Before ICapture Config");

#endif

    LOGE("Zoom set to %d", fixedZoom);

    status = icap_config_common(iobj->lib_private, &iobj->cfg);

    if( ICAP_STATUS_FAIL == status){
        LOGE ("ICapture Config function failed");
        goto fail_config;
    }

#if DEBUG_LOG

    PPM("ICapture config OK");

    LOGD("iobj->cfg.image_width = %d = 0x%x iobj->cfg.image_height=%d = 0x%x , iobj->cfg.sizeof_img_buf = %d", (int)iobj->cfg.width, (int)iobj->cfg.width, (int)iobj->cfg.height, (int)iobj->cfg.height, (int) spec_res.buffer_size);

#endif

    cap_tuning.icam_makernote = ( void * ) &fobj->mnote;
    cap_tuning.icam_settings = ( void * ) &fobj->settings;
    cap_tuning.icam_status = ( void * ) &fobj->status;

#ifdef DEBUG_LOG

    PPM("SETUP SOME 3A STUFF");

#endif

    status = icap_config_tuning(iobj->lib_private, &cap_tuning);
    if( ICAP_STATUS_FAIL == status){
        LOGE ("ICapture tuning function failed");
        goto fail_config;
    }

    allocatePictureBuffers(spec_res.buffer_size, mBurstShots);

    for ( int i = 0; i < mBurstShots; i++ ) {
        capture_buffer.buffer = (void *) NEXT_4K_ALIGN_ADDR((unsigned int) mYuvBuffer[i]);
        capture_buffer.alloc_size = spec_res.buffer_size;

        LOGE ("ICapture push buffer 0x%x, len %d",
                ( unsigned int ) capture_buffer.buffer, capture_buffer.alloc_size);
        status = icap_push_buffer(iobj->lib_private, &capture_buffer, &snapshotBuffer);
        if( ICAP_STATUS_FAIL == status){
            LOGE ("ICapture push buffer function failed");
            goto fail_config;
        }

    }

    for ( int i = 0; i < mBurstShots; i++ ) {

#if DEBUG_LOG

    PPM("BEFORE ICapture Process");

#endif

        status = icap_process(iobj->lib_private, mode, NULL);

    if( ICAP_STATUS_FAIL == status ) {
        LOGE("ICapture Process failed");
        goto fail_process;
    }

#if DEBUG_LOG

    else {
        PPM("ICapture process OK");
    }

    LOGD("iobj->proc.out_img_w = %d = 0x%x iobj->proc.out_img_h=%u = 0x%x", (int) iobj->cfg.width,(int) iobj->cfg.width, (int) iobj->cfg.height,(int)iobj->cfg.height);

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

    }

    for ( int i = 0; i < mBurstShots; i++ ) {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    PPM("SENDING MESSAGE TO PROCESSING THREAD");

#endif

    mExifParams.exposure = fobj->status.ae.shutter_cap;
    mExifParams.zoom = zoom_step[mZoomTargetIdx];
    exif_buf = get_exif_buffer(&mExifParams, gpsLocation);

    if( NULL != gpsLocation ) {
        free(gpsLocation);
        gpsLocation = NULL;
    }

    procMessage[PROC_MSG_IDX_ACTION] = PROC_THREAD_PROCESS;
    procMessage[PROC_MSG_IDX_CAPTURE_W] = iobj->cfg.width;
    procMessage[PROC_MSG_IDX_CAPTURE_H] = iobj->cfg.height;
    procMessage[PROC_MSG_IDX_IMAGE_W] = image_width;
    procMessage[PROC_MSG_IDX_IMAGE_H] = image_height;
    procMessage[PROC_MSG_IDX_PIX_FMT] = pixelFormat;

#ifdef IMAGE_PROCESSING_PIPELINE
    procMessage[PROC_MSG_IDX_IPP_MODE] = mippMode;
    if (mippMode != IPP_Disabled_Mode) {
        procMessage[PROC_MSG_IDX_IPP_TO_ENABLE] = mIPPToEnable;
        procMessage[PROC_MSG_IDX_IPP_EES]  = mIPPParams.EdgeEnhancementStrength;
        procMessage[PROC_MSG_IDX_IPP_WET]  = mIPPParams.WeakEdgeThreshold;
        procMessage[PROC_MSG_IDX_IPP_SET]  = mIPPParams.StrongEdgeThreshold;
        procMessage[PROC_MSG_IDX_IPP_LFLNFS]  = mIPPParams.LowFreqLumaNoiseFilterStrength;
        procMessage[PROC_MSG_IDX_IPP_MFLNFS] = mIPPParams.MidFreqLumaNoiseFilterStrength;
        procMessage[PROC_MSG_IDX_IPP_HFLNFS] = mIPPParams.HighFreqLumaNoiseFilterStrength;
        procMessage[PROC_MSG_IDX_IPP_LFCBNFS] = mIPPParams.LowFreqCbNoiseFilterStrength;
        procMessage[PROC_MSG_IDX_IPP_MFCBNFS] = mIPPParams.MidFreqCbNoiseFilterStrength;
        procMessage[PROC_MSG_IDX_IPP_HFCBNFS] = mIPPParams.HighFreqCbNoiseFilterStrength;
        procMessage[PROC_MSG_IDX_IPP_LFCRNFS] = mIPPParams.LowFreqCrNoiseFilterStrength;
        procMessage[PROC_MSG_IDX_IPP_MFCRNFS] = mIPPParams.MidFreqCrNoiseFilterStrength;
        procMessage[PROC_MSG_IDX_IPP_HFCRNFS] = mIPPParams.HighFreqCrNoiseFilterStrength;
        procMessage[PROC_MSG_IDX_IPP_SVP1] = mIPPParams.shadingVertParam1;
        procMessage[PROC_MSG_IDX_IPP_SVP2] = mIPPParams.shadingVertParam2;
        procMessage[PROC_MSG_IDX_IPP_SHP1] = mIPPParams.shadingHorzParam1;
        procMessage[PROC_MSG_IDX_IPP_SHP2] = mIPPParams.shadingHorzParam2;
        procMessage[PROC_MSG_IDX_IPP_SGS] = mIPPParams.shadingGainScale;
        procMessage[PROC_MSG_IDX_IPP_SGO] = mIPPParams.shadingGainOffset;
        procMessage[PROC_MSG_IDX_IPP_SGMV] = mIPPParams.shadingGainMaxValue;
        procMessage[PROC_MSG_IDX_IPP_RDSCBCR] = mIPPParams.ratioDownsampleCbCr;
    }
#endif

    procMessage[PROC_MSG_IDX_YUV_BUFF] = (unsigned int) mYuvBuffer[i];
    procMessage[PROC_MSG_IDX_YUV_BUFFLEN] = mYuvBufferLen[i];
    procMessage[PROC_MSG_IDX_ROTATION] = rotation;

    procMessage[PROC_MSG_IDX_ZOOM] = mZoomTargetIdx;

    procMessage[PROC_MSG_IDX_JPEG_QUALITY] = quality;
    procMessage[PROC_MSG_IDX_JPEG_CB] = (unsigned int) mDataCb;
    procMessage[PROC_MSG_IDX_RAW_CB] = 0;
    procMessage[PROC_MSG_IDX_CB_COOKIE] = (unsigned int) mCallbackCookie;
    procMessage[PROC_MSG_IDX_CROP_L] = 0;
    procMessage[PROC_MSG_IDX_CROP_T] = 0;
    procMessage[PROC_MSG_IDX_CROP_W] = iobj->cfg.width;
    procMessage[PROC_MSG_IDX_CROP_H] = iobj->cfg.height;
    procMessage[PROC_MSG_IDX_THUMB_W] = thumb_width;
    procMessage[PROC_MSG_IDX_THUMB_H] = thumb_height;
#ifdef HARDWARE_OMX
    procMessage[PROC_MSG_IDX_EXIF_BUFF] = (unsigned int) exif_buf;
#endif

    write(procPipe[1], &procMessage, sizeof(procMessage));

    mIPPToEnable = false; // reset ipp enable after sending to proc thread

#ifdef DEBUG_LOG

    LOGD("\n\n\n PICTURE NUMBER =%d\n\n\n",++pictureNumber);

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
    base = NEXT_4K_ALIGN_ADDR(base);
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
                             snapshot_buffer, preview_width, preview_height, 0, PIX_YUV422I, zoom_step[/*mZoomTargetIdx*/ 0], 0, 0, image_width, image_height);

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

        queueToOverlay(lastOverlayBufferDQ);
        dequeueFromOverlay();
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
        LOGE(" outbuffer = 0x%x, jpegSize = %d, yuv_buffer = 0x%x, yuv_len = %d, "
                "image_width = %d, image_height = %d,  quality = %d, mippMode = %d",
                (unsigned int)outBuffer , jpegSize, (unsigned int)yuv_buffer, yuv_len,
                image_width, image_height, quality, mippMode);
        jpegEncoder->encodeImage((uint8_t *)outBuffer , jpegSize, yuv_buffer, yuv_len,
                image_width, image_height, quality, exif_buf, jpegFormat, DEFAULT_THUMB_WIDTH, DEFAULT_THUMB_HEIGHT, image_width, image_height,
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

    // Release constraint to DSP OPP by setting lowest Hz
    SetDSPKHz(DSP3630_KHZ_MIN);

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

void *CameraHal::getLastOverlayAddress()
{
    void *res = NULL;

    mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress( (void*) (lastOverlayBufferDQ) );
    if ( data == NULL ) {
        LOGE(" getBufferAddress returned NULL");
    } else {
        res = data->ptr;
    }

    LOG_FUNCTION_NAME_EXIT

    return res;
}

size_t CameraHal::getLastOverlayLength()
{
    size_t res = 0;

    mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress( (void*) (lastOverlayBufferDQ) );
    if ( data == NULL ) {
        LOGE(" getBufferAddress returned NULL");
    } else {
        res = data->length;
    }

    LOG_FUNCTION_NAME_EXIT

    return res;
}

void CameraHal::snapshotThread()
{
    fd_set descriptorSet;
    int max_fd;
    int err, status;
    unsigned int snapshotMessage[9], snapshotReadyMessage;
    int image_width, image_height, pixelFormat, preview_width, preview_height;
    int crop_top, crop_left, crop_width, crop_height;
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
                crop_left = snapshotMessage[5];
                crop_top = snapshotMessage[6];
                crop_width = snapshotMessage[7];
                crop_height = snapshotMessage[8];

                mParameters.getPreviewSize(&preview_width, &preview_height);

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

                PPM("Before vpp downscales:");

#endif

                snapshot_buffer = getLastOverlayAddress();
                if ( NULL == snapshot_buffer )
                    goto EXIT;

                LOGE("Snapshot buffer 0x%x, yuv_buffer = 0x%x, zoomTarget = %5.2f", ( unsigned int ) snapshot_buffer, ( unsigned int ) yuv_buffer, ZoomTarget);

                status = scale_init(image_width, image_height, preview_width, preview_height, PIX_YUV422I, PIX_YUV422I);

                if ( status < 0 ) {
                    LOGE("VPP init failed");
                    goto EXIT;
                }

                status = scale_process(yuv_buffer, image_width, image_height,
                         snapshot_buffer, preview_width, preview_height, 0, PIX_YUV422I, zoom_step[0], crop_top, crop_left, crop_width, crop_height);

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

                scale_deinit();

                queueToOverlay(lastOverlayBufferDQ);
                dequeueFromOverlay();

EXIT:

                write(snapshotReadyPipe[1], &snapshotReadyMessage, sizeof(snapshotReadyMessage));
          } else if (snapshotMessage[0] == SNAPSHOT_THREAD_START_GEN) {

                queueToOverlay(lastOverlayBufferDQ);
                mParameters.getPreviewSize(&preview_width, &preview_height);

#ifdef DUMP_SNAPSHOT
                SaveFile(NULL, (char*)"snp", getLastOverlayAddress(), preview_width*preview_height*2);
#endif

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

               PPM("Shot to Snapshot", &ppm_receiveCmdToTakePicture);

#endif
                dequeueFromOverlay();
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
    unsigned int procMessage[PROC_MSG_IDX_MAX];
    unsigned int jpegQuality, jpegSize, size, base, tmpBase, offset, yuv_len, image_rotation;
    unsigned int crop_top, crop_left, crop_width, crop_height;
    double image_zoom;
    sp<MemoryHeapBase> JPEGPictureHeap;
    sp<MemoryBase> JPEGPictureMemBase;
    data_callback RawPictureCallback;
    data_callback JpegPictureCallback;
    void *yuv_buffer, *outBuffer, *PictureCallbackCookie;
    int thumb_width, thumb_height;

#ifdef HARDWARE_OMX
    exif_buffer *exif_buf;
#endif

#ifdef IMAGE_PROCESSING_PIPELINE
    unsigned int ippMode = IPP_Disabled_Mode;
    bool ipp_to_enable;
    unsigned short EdgeEnhancementStrength, WeakEdgeThreshold, StrongEdgeThreshold,
                   LowFreqLumaNoiseFilterStrength, MidFreqLumaNoiseFilterStrength, HighFreqLumaNoiseFilterStrength,
                   LowFreqCbNoiseFilterStrength, MidFreqCbNoiseFilterStrength, HighFreqCbNoiseFilterStrength,
                   LowFreqCrNoiseFilterStrength, MidFreqCrNoiseFilterStrength, HighFreqCrNoiseFilterStrength,
                   shadingVertParam1, shadingVertParam2, shadingHorzParam1, shadingHorzParam2, shadingGainScale,
                   shadingGainOffset, shadingGainMaxValue, ratioDownsampleCbCr;
#endif

    void* input_buffer;
    unsigned int input_length;

    max_fd = procPipe[0] + 1;

    FD_ZERO(&descriptorSet);
    FD_SET(procPipe[0], &descriptorSet);

    mJPEGLength  = MAX_THUMB_WIDTH*MAX_THUMB_HEIGHT + PICTURE_WIDTH*PICTURE_HEIGHT + ((2*PAGE) - 1);
    mJPEGLength &= ~((2*PAGE) - 1);
    mJPEGLength  += 2*PAGE;
    JPEGPictureHeap = new MemoryHeapBase(mJPEGLength);

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

            if(procMessage[PROC_MSG_IDX_ACTION] == PROC_THREAD_PROCESS) {

#ifdef DEBUG_LOG

                LOGD("PROC_THREAD_PROCESS_RECEIVED\n");

#endif

                capture_width = procMessage[PROC_MSG_IDX_CAPTURE_W];
                capture_height = procMessage[PROC_MSG_IDX_CAPTURE_H];
                image_width = procMessage[PROC_MSG_IDX_IMAGE_W];
                image_height = procMessage[PROC_MSG_IDX_IMAGE_H];
                pixelFormat = procMessage[PROC_MSG_IDX_PIX_FMT];
#ifdef IMAGE_PROCESSING_PIPELINE
                ippMode = procMessage[PROC_MSG_IDX_IPP_MODE];
                if (ippMode != IPP_Disabled_Mode) {
                    ipp_to_enable = procMessage[PROC_MSG_IDX_IPP_TO_ENABLE];
                    EdgeEnhancementStrength = procMessage[PROC_MSG_IDX_IPP_EES];
                    WeakEdgeThreshold = procMessage[PROC_MSG_IDX_IPP_WET];
                    StrongEdgeThreshold = procMessage[PROC_MSG_IDX_IPP_SET];
                    LowFreqLumaNoiseFilterStrength = procMessage[PROC_MSG_IDX_IPP_LFLNFS];
                    MidFreqLumaNoiseFilterStrength = procMessage[PROC_MSG_IDX_IPP_MFLNFS];
                    HighFreqLumaNoiseFilterStrength = procMessage[PROC_MSG_IDX_IPP_HFLNFS];
                    LowFreqCbNoiseFilterStrength = procMessage[PROC_MSG_IDX_IPP_LFCBNFS];
                    MidFreqCbNoiseFilterStrength = procMessage[PROC_MSG_IDX_IPP_MFCBNFS];
                    HighFreqCbNoiseFilterStrength = procMessage[PROC_MSG_IDX_IPP_HFCBNFS];
                    LowFreqCrNoiseFilterStrength = procMessage[PROC_MSG_IDX_IPP_LFCRNFS];
                    MidFreqCrNoiseFilterStrength = procMessage[PROC_MSG_IDX_IPP_MFCRNFS];
                    HighFreqCrNoiseFilterStrength = procMessage[PROC_MSG_IDX_IPP_HFCRNFS];
                    shadingVertParam1 = procMessage[PROC_MSG_IDX_IPP_SVP1];
                    shadingVertParam2 = procMessage[PROC_MSG_IDX_IPP_SVP2];
                    shadingHorzParam1 = procMessage[PROC_MSG_IDX_IPP_SHP1];
                    shadingHorzParam2 = procMessage[PROC_MSG_IDX_IPP_SHP2];
                    shadingGainScale = procMessage[PROC_MSG_IDX_IPP_SGS];
                    shadingGainOffset = procMessage[PROC_MSG_IDX_IPP_SGO];
                    shadingGainMaxValue = procMessage[PROC_MSG_IDX_IPP_SGMV];
                    ratioDownsampleCbCr = procMessage[PROC_MSG_IDX_IPP_RDSCBCR];
                }
#endif
                yuv_buffer = (void *) procMessage[PROC_MSG_IDX_YUV_BUFF];
                yuv_len = procMessage[PROC_MSG_IDX_YUV_BUFFLEN];
                image_rotation = procMessage[PROC_MSG_IDX_ROTATION];
                image_zoom = zoom_step[procMessage[PROC_MSG_IDX_ZOOM]];
                jpegQuality = procMessage[PROC_MSG_IDX_JPEG_QUALITY];
                JpegPictureCallback = (data_callback) procMessage[PROC_MSG_IDX_JPEG_CB];
                RawPictureCallback = (data_callback) procMessage[PROC_MSG_IDX_RAW_CB];
                PictureCallbackCookie = (void *) procMessage[PROC_MSG_IDX_CB_COOKIE];
                crop_left = procMessage[PROC_MSG_IDX_CROP_L];
                crop_top = procMessage[PROC_MSG_IDX_CROP_T];
                crop_width = procMessage[PROC_MSG_IDX_CROP_W];
                crop_height = procMessage[PROC_MSG_IDX_CROP_H];
                thumb_width = procMessage[PROC_MSG_IDX_THUMB_W];
                thumb_height = procMessage[PROC_MSG_IDX_THUMB_H];

#ifdef HARDWARE_OMX
                exif_buf = (exif_buffer *)procMessage[PROC_MSG_IDX_EXIF_BUFF];
#endif

                LOGD("JPEGPictureHeap->getStrongCount() = %d, base = 0x%x",
                        JPEGPictureHeap->getStrongCount(), (unsigned int)JPEGPictureHeap->getBase());
                jpegSize = mJPEGLength;

                if(JPEGPictureHeap->getStrongCount() > 1 )
                {
                    JPEGPictureHeap.clear();
                    JPEGPictureHeap = new MemoryHeapBase(jpegSize);
                }

                base = (unsigned long) JPEGPictureHeap->getBase();
                base = (unsigned long) NEXT_4K_ALIGN_ADDR(base);
                offset = base - (unsigned long) JPEGPictureHeap->getBase();
                outBuffer = (void *) base;

                pixelFormat = PIX_YUV422I;

                input_buffer = (void *) NEXT_4K_ALIGN_ADDR(yuv_buffer);
                input_length = yuv_len - ((unsigned int) input_buffer - (unsigned int) yuv_buffer);

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
                     LOGD("Calling ProcessBufferIPP(buffer=%p , len=0x%x)", input_buffer, input_length);
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
                LOGD(" outbuffer = %p, jpegSize = %d, input_buffer = %p, input_length = %d, "
                        "image_width = %d, image_height = %d, quality = %d",
                        outBuffer , jpegSize, input_buffer, input_length,
                        image_width, image_height, jpegQuality);
#endif
                //workaround for thumbnail size  - it should be smaller than captured image
                if ((image_width<thumb_width) || (image_height<thumb_width) ||
                    (image_width<thumb_height) || (image_height<thumb_height)) {
                     thumb_width = MIN_THUMB_WIDTH;
                     thumb_height = MIN_THUMB_HEIGHT;
                }

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
                if(/*JpegPictureCallback*/ true) {

#if JPEG

                    if ( mBurstShots > 1 ){
                        JpegPictureCallback(CAMERA_MSG_BURST_IMAGE, JPEGPictureMemBase, PictureCallbackCookie);
                    }
                    else {
                        JpegPictureCallback(CAMERA_MSG_COMPRESSED_IMAGE, JPEGPictureMemBase, PictureCallbackCookie);
                    }
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

                // Release constraint to DSP OPP by setting lowest Hz
                SetDSPKHz(DSP3630_KHZ_MIN);

            } else if(procMessage[PROC_MSG_IDX_ACTION] == PROC_THREAD_EXIT) {
                LOGD("PROC_THREAD_EXIT_RECEIVED");
                JPEGPictureHeap.clear();
                break;
            }
        }
    }

    JPEGPictureHeap.clear();

    LOG_FUNCTION_NAME_EXIT
}

int CameraHal::allocatePictureBuffers(size_t length, int burstCount)
{
    if (burstCount > MAX_BURST) {
        LOGE("Can't handle burstCount(%d) larger then MAX_BURST(%d)",
                burstCount, MAX_BURST);
        return -1;
    }

    length += ((2*PAGE) - 1) + 10*PAGE;
    length &= ~((2*PAGE) - 1);
    length += 2*PAGE;

    for (int i = 0; i < burstCount; i++) {
        if (mYuvBuffer[i] != NULL && mYuvBufferLen[i] == length) {
            // proper buffer already allocated. skip alloc.
            continue;
        }

        if (mYuvBuffer[i])
            free(mYuvBuffer[i]);

        mYuvBuffer[i] = (uint8_t *) malloc(length);
        mYuvBufferLen[i] = length;

        if (mYuvBuffer[i] == NULL) {
            LOGE("mYuvBuffer[%d] malloc failed", i);
            return -1;
        }
    }

    return NO_ERROR;
}

int CameraHal::freePictureBuffers(void)
{
    for (int i = 0; i < MAX_BURST; i++) {
        if (mYuvBuffer[i]) {
            free(mYuvBuffer[i]);
            mYuvBuffer[i] = NULL;
            mYuvBufferLen[i] = 0;
        }
    }

    return NO_ERROR;
}

int CameraHal::ICaptureCreate(void)
{
    int res;

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME

#endif

    res = 0;

#ifdef ICAP

    iobj = (libtest_obj *) malloc( sizeof( *iobj));
    if( NULL == iobj) {
        LOGE("libtest_obj malloc failed");
        goto exit;
    }

    memset(iobj, 0 , sizeof(*iobj));

    res = icap_create(&iobj->lib_private);
    if ( ICAP_STATUS_FAIL == res) {
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

#ifdef ICAP

   icap_destroy(iobj->lib_private);

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
    {
        isStart_JPEG = false;
        delete jpegEncoder;
        jpegEncoder = NULL;
    }
#endif
#endif

#ifdef ICAP
    if(iobj != NULL)
    {
        err = icap_destroy(iobj->lib_private);
        if ( ICAP_STATUS_FAIL == err )
            LOGE("ICapture Delete failed");
        else
            LOGD("ICapture delete OK");

        free(iobj);
        iobj = NULL;
    }

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
            mVideoBufferStatus[i] = BUFF_IDLE;
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
    }    // Eclair HAL

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

status_t CameraHal::startPreview()
{
    LOG_FUNCTION_NAME

    if(mOverlay == NULL)    //Eclair HAL
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

    mVideoBufferCount = mOverlay->getBufferCount();

    if (mVideoBufferCount > VIDEO_FRAME_COUNT_MAX) {
        LOGD("Error: mVideoBufferCount > VIDEO_FRAME_COUNT_MAX");
        return UNKNOWN_ERROR;
    }

    mRecordingLock.lock();

    for(i = 0; i < mVideoBufferCount; i++) {
        mVideoHeaps[i].clear();
        mVideoBuffer[i].clear();
        mVideoBufferStatus[i] &= ~BUFF_Q2VE;
    }

    debugShowBufferStatus();

    for(i = 0; i < mVideoBufferCount; i++)
    {
        mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress((void*)i);
        // make sure data if valid, if not clear all previously allocated memory and return
        if(data != NULL)
        {
            mVideoHeaps[i] = new MemoryHeapBase(data->fd,mPreviewFrameSize, 0, data->offset);
            mVideoBuffer[i] = new MemoryBase(mVideoHeaps[i], 0, mRecordingFrameSize);
            LOGV("mVideoHeaps[%d]: ID:%d,Base:[%p],size:%d", i, mVideoHeaps[i]->getHeapID(), mVideoHeaps[i]->getBase() ,mVideoHeaps[i]->getSize());
            LOGV("mVideoBuffer[%d]: Pointer[%p]", i, mVideoBuffer[i]->pointer());
        } else{
            for(int j = 0; j < i+1; j++)
            {
                mVideoHeaps[j].clear();
                mVideoBuffer[j].clear();
                mVideoBufferStatus[i] &= ~BUFF_Q2VE;
            }
            LOGD("Error: data from overlay returned null");
            return UNKNOWN_ERROR;
        }
    }

    mRecordEnabled =true;
    mRecordingLock.unlock();
    return NO_ERROR;
}

void CameraHal::stopRecording()
{
    LOG_FUNCTION_NAME
    mRecordingLock.lock();
    debugShowBufferStatus();
    mRecordEnabled = false;

    // Return all buffers queued to VE back to camera. Otherwise, after few
    // recordings, camera is left without buffers.
    for (int i = 0; i < mVideoBufferCount; i++) {
        if ((mVideoBufferStatus[i]&BUFF_Q2VE) == BUFF_Q2VE) {
            LOGD("Recovering Buffer[%d] status[%d] \n",i,mVideoBufferStatus[i]);
            mVideoBufferStatus[i] &= ~BUFF_Q2VE;
            queueToCamera(i);
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
//LOG_FUNCTION_NAME
    int index;

    for(index = 0; index < mVideoBufferCount; index++){
        if(mem->pointer() == mVideoBuffer[index]->pointer()) {
            break;
        }
    }

    if (index == mVideoBufferCount)
    {
        LOGD("Encoder returned wrong buffer address");
        return;
    }

    if (0 == (mVideoBufferStatus[index] & BUFF_Q2VE)) {
        LOGW("Buffer#%d(stat=%d) not queued to VE.", index, mVideoBufferStatus[index]);
        return;
    }

    debugShowFPS();

    mRecordingLock.lock();

    mVideoBufferStatus[index] &= ~BUFF_Q2VE;
    queueToCamera(index);

    mRecordingLock.unlock();



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
//    mCallbackCookie = NULL;   // done in destructor

    LOGE("Callbacks set to null");
    return -1;
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
    int framerate_min, framerate_max;
    int zoom, compensation, saturation, sharpness;
    int zoom_save;
    int contrast, brightness;
    int error;
    int base;
#ifdef FW3A
    static int mLastFocusMode = ICAM_FOCUS_MODE_AF_AUTO;
#endif
    const char *valstr;
    char *af_coord;
    Message msg;

    Mutex::Autolock lock(mLock);

    LOGD("PreviewFormat %s", params.getPreviewFormat());

    if ( params.getPreviewFormat() != NULL ) {
        if (strcmp(params.getPreviewFormat(), (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) != 0) {
            LOGE("Only yuv422i preview is supported");
            //return -EINVAL;
        }
    }

    LOGD("PictureFormat %s", params.getPictureFormat());
    if ( params.getPictureFormat() != NULL ) {
        if (strcmp(params.getPictureFormat(), (const char *) CameraParameters::PIXEL_FORMAT_JPEG) != 0) {
            LOGE("Only jpeg still pictures are supported");
            return -EINVAL;
        }
    }

    params.getPreviewSize(&w, &h);
    if ( validateSize(w, h, supportedPreviewRes, ARRAY_SIZE(supportedPreviewRes)) == false ) {
        LOGE("Preview size not supported");
        return -EINVAL;
    }
    LOGD("PreviewResolution by App %d x %d", w, h);

    params.getPictureSize(&w, &h);
    if (validateSize(w, h, supportedPictureRes, ARRAY_SIZE(supportedPictureRes)) == false ) {
        LOGE("Picture size not supported");
        return -EINVAL;
    }
    LOGD("Picture Size by App %d x %d", w, h);

#ifdef HARDWARE_OMX

    mExifParams.width = w;
    mExifParams.height = h;

#endif

    //Try to get preview framerate.
    //If we do not get framerate, try with fps range.
    //This way we can support both fps or fps range in CameraHAL.
    //Old applications set fps only and this is used as default in CameraHAL.
    //New applications can use fps range, but should remove fps with
    //cameraParameters.remove(KEY_FRAME_RATE) in order to use it.
    //If both are not present in cameraParameters structure,
    //or fps range is not supported then exit.
    framerate_max = params.getPreviewFrameRate();
    if ( framerate_max == -1 ) useFramerateRange = true;
    else useFramerateRange = false;

    //Camera documentation says that fps range given in cameraParameters must be present
    //and must be in supported range. So no matter if we use fps or fps range, we check
    //for correct parameters.
    params.getPreviewFpsRange(&framerate_min, &framerate_max);

    if ( validateRange(framerate_min, framerate_max, supportedFpsRanges) == false ) {
        LOGE("Range Not Supported");
        return -EINVAL;
    }

    if (useFramerateRange){
        LOGD("Setparameters(): Framerate range: MIN %d, MAX %d", framerate_min, framerate_max);
    }
    else
    {
        //Get the framerate again:
        framerate_max = params.getPreviewFrameRate();
        LOGD("Setparameters(): Framerate: %d", framerate_max);
    }

    rot_orig = rotation;
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.video.force_rotation", value, "-1");
    rotation = atoi(value);
    if (0 > rotation) {
        rotation = params.getInt(CameraParameters::KEY_ROTATION);
    } else {
        LOGI("Rotation force changed to %d", rotation);
    }

    mParameters.getPictureSize(&w_orig, &h_orig);
    zoom_save = mParameters.getInt(CameraParameters::KEY_ZOOM);
    mParameters = params;

#ifdef IMAGE_PROCESSING_PIPELINE

    if((mippMode != mParameters.getInt(KEY_IPP)) || (w != w_orig) || (h != h_orig) ||
            ((rot_orig != 180) && (rotation == 180)) ||
            ((rot_orig == 180) && (rotation != 180))) // in the current setup, IPP uses a different setup when rotation is 180 degrees
    {
        if(pIPP.hIPP != NULL){
            LOGD("pIPP.hIPP=%p", pIPP.hIPP);
            if(DeInitIPP(mippMode)) // deinit here to save time
                LOGE("ERROR DeInitIPP() failed");
            pIPP.hIPP = NULL;
        }

        mippMode = mParameters.getInt(KEY_IPP);
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
    //Keep the JPEG thumbnail quality same as JPEG quality.
    //JPEG encoder uses the same quality for thumbnail as for the main image.
    mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, quality);

    zoom = mParameters.getInt(CameraParameters::KEY_ZOOM);
    if( (zoom >= 0) && ( zoom < ZOOM_STAGES) ){
        // immediate zoom
        mZoomSpeed = 0;
        mZoomTargetIdx = zoom;
    } else if(zoom>= ZOOM_STAGES){
        mParameters.set(CameraParameters::KEY_ZOOM, zoom_save);
        return -EINVAL;
    } else {
        mZoomTargetIdx = 0;
    }
    LOGD("Zoom by App %d", zoom);

#ifdef HARDWARE_OMX

    mExifParams.zoom = zoom;

#endif

    if ( ( params.get(CameraParameters::KEY_GPS_LATITUDE) != NULL ) && ( params.get(CameraParameters::KEY_GPS_LONGITUDE) != NULL ) && ( params.get(CameraParameters::KEY_GPS_ALTITUDE) != NULL )) {

        double gpsCoord;
        struct tm* timeinfo;

#ifdef HARDWARE_OMX

        if( NULL == gpsLocation )
            gpsLocation = (gps_data *) malloc( sizeof(gps_data));

        if( NULL != gpsLocation ) {
            LOGE("initializing gps_data structure");

            memset(gpsLocation, 0, sizeof(gps_data));
            gpsLocation->datestamp[0] = '\0';

            gpsCoord = strtod( params.get(CameraParameters::KEY_GPS_LATITUDE), NULL);
            convertGPSCoord(gpsCoord, &gpsLocation->latDeg, &gpsLocation->latMin, &gpsLocation->latSec);
            gpsLocation->latRef = (gpsCoord < 0) ? (char*) "S" : (char*) "N";

            gpsCoord = strtod( params.get(CameraParameters::KEY_GPS_LONGITUDE), NULL);
            convertGPSCoord(gpsCoord, &gpsLocation->longDeg, &gpsLocation->longMin, &gpsLocation->longSec);
            gpsLocation->longRef = (gpsCoord < 0) ? (char*) "W" : (char*) "E";

            gpsCoord = strtod( params.get(CameraParameters::KEY_GPS_ALTITUDE), NULL);
            gpsLocation->altitude = gpsCoord;

            if ( NULL != params.get(CameraParameters::KEY_GPS_TIMESTAMP) ){
                gpsLocation->timestamp = strtol( params.get(CameraParameters::KEY_GPS_TIMESTAMP), NULL, 10);
                timeinfo = localtime((time_t*)&(gpsLocation->timestamp));
                if(timeinfo != NULL)
                    strftime(gpsLocation->datestamp, 11, "%Y:%m:%d", timeinfo);
            }

            gpsLocation->altitudeRef = params.getInt(KEY_GPS_ALTITUDE_REF);
            gpsLocation->mapdatum = (char *) params.get(KEY_GPS_MAPDATUM);
            gpsLocation->versionId = (char *) params.get(KEY_GPS_VERSION);
            gpsLocation->procMethod = (char *) params.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);

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

    if ( params.get(KEY_CAPTURE_MODE) != NULL ) {
        if (strcmp(params.get(KEY_CAPTURE_MODE), (const char *) HIGH_QUALITY) == 0) {
            mcapture_mode = 2;
        } else if (strcmp(params.get(KEY_CAPTURE_MODE), (const char *) HIGH_PERFORMANCE) == 0) {
            mcapture_mode = 1;
        } else {
            mcapture_mode = 1;
        }
    } else {
        mcapture_mode = 1;
    }

    int burst_capture = params.getInt(KEY_BURST);
    if ( ( 0 >= burst_capture ) ){
        mBurstShots = 1;
    } else {
        // hardcoded in HP mode
        mcapture_mode = 1;
        mBurstShots = burst_capture;
    }

    LOGD("Capture Mode set %d, Burst Shots set %d", mcapture_mode, burst_capture);
    LOGD("mBurstShots %d", mBurstShots);

    if ( NULL != fobj ){

        if ( params.get(KEY_METER_MODE) != NULL ) {
            if (strcmp(params.get(KEY_METER_MODE), (const char *) METER_MODE_CENTER) == 0) {
                fobj->settings.af.spot_weighting = ICAM_FOCUS_SPOT_SINGLE_CENTER;
                fobj->settings.ae.spot_weighting = ICAM_EXPOSURE_SPOT_CENTER;

#ifdef HARDWARE_OMX

                mExifParams.metering_mode = EXIF_CENTER;

#endif

            } else if (strcmp(params.get(KEY_METER_MODE), (const char *) METER_MODE_AVERAGE) == 0) {
                fobj->settings.af.spot_weighting = ICAM_FOCUS_SPOT_MULTI_AVERAGE;
                fobj->settings.ae.spot_weighting = ICAM_EXPOSURE_SPOT_NORMAL;

#ifdef HARDWARE_OMX

                mExifParams.metering_mode = EXIF_AVERAGE;

#endif

            }
        }

        if ( params.get(KEY_CAPTURE) != NULL ) {
            if (strcmp(params.get(KEY_CAPTURE), (const char *) CAPTURE_STILL) == 0) {
                //Set 3A config to enable variable fps
                fobj->settings.general.view_finder_mode = ICAM_VFMODE_STILL_CAPTURE;
            }
           else {
               fobj->settings.general.view_finder_mode = ICAM_VFMODE_VIDEO_RECORD;
           }
        }
        else {
            fobj->settings.general.view_finder_mode = ICAM_VFMODE_STILL_CAPTURE;
        }

        if (useFramerateRange) {
            //Gingerbread changes for FPS - set range of min and max fps. Minimum is 7.8 fps, maximum is 30 fps.
            if ( (MIN_FPS*1000 <= framerate_min)&&(framerate_min < framerate_max)&&(framerate_max<=MAX_FPS*1000) ) {
                fobj->settings.ae.framerate = 0;
            }
            else if ( (MIN_FPS*1000 <= framerate_min)&&(framerate_min == framerate_max)&&(framerate_max <= MAX_FPS*1000) )
            {
                fobj->settings.ae.framerate = framerate_max/1000;
            }
            else {
                fobj->settings.ae.framerate = 0;
            }
        }
        //using deprecated previewFrameRate
        else {
            fobj->settings.ae.framerate = framerate_max;
        }

        if ( params.get(CameraParameters::KEY_SCENE_MODE) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_AUTO) == 0) {
                fobj->settings.general.scene = ICAM_SCENE_MODE_MANUAL;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_PORTRAIT) == 0) {

                fobj->settings.general.scene = ICAM_SCENE_MODE_PORTRAIT;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_LANDSCAPE) == 0) {

                fobj->settings.general.scene = ICAM_SCENE_MODE_LANDSCAPE;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_NIGHT) == 0) {

                fobj->settings.general.scene = ICAM_SCENE_MODE_NIGHT;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_NIGHT_PORTRAIT) == 0) {

                fobj->settings.general.scene = ICAM_SCENE_MODE_NIGHT_PORTRAIT;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_FIREWORKS) == 0) {

                fobj->settings.general.scene = ICAM_SCENE_MODE_FIREWORKS;

            } else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_ACTION) == 0) {

                fobj->settings.general.scene = ICAM_SCENE_MODE_SPORT;

            }
              else if (strcmp(params.get(CameraParameters::KEY_SCENE_MODE), (const char *) CameraParameters::SCENE_MODE_SNOW) == 0) {

                fobj->settings.general.scene = ICAM_SCENE_MODE_SNOW_BEACH;

            }
        }

        if ( params.get(CameraParameters::KEY_WHITE_BALANCE) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_AUTO ) == 0) {

                fobj->settings.awb.mode = ICAM_WHITE_BALANCE_MODE_WB_AUTO;

#ifdef HARDWARE_OMX

                mExifParams.wb = EXIF_WB_AUTO;

#endif

            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_INCANDESCENT) == 0) {

                fobj->settings.awb.mode = ICAM_WHITE_BALANCE_MODE_WB_INCANDESCENT;

#ifdef HARDWARE_OMX

                mExifParams.wb = EXIF_WB_MANUAL;

#endif

            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_FLUORESCENT) == 0) {

                fobj->settings.awb.mode = ICAM_WHITE_BALANCE_MODE_WB_FLUORESCENT;

#ifdef HARDWARE_OMX

                mExifParams.wb = EXIF_WB_MANUAL;

#endif

            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_DAYLIGHT) == 0) {

                fobj->settings.awb.mode = ICAM_WHITE_BALANCE_MODE_WB_DAYLIGHT;

#ifdef HARDWARE_OMX

                mExifParams.wb = EXIF_WB_MANUAL;

#endif

            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT) == 0) {

                fobj->settings.awb.mode = ICAM_WHITE_BALANCE_MODE_WB_CLOUDY;

#ifdef HARDWARE_OMX

                mExifParams.wb = EXIF_WB_MANUAL;

#endif

            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) CameraParameters::WHITE_BALANCE_SHADE) == 0) {

                fobj->settings.awb.mode = ICAM_WHITE_BALANCE_MODE_WB_SHADOW;

#ifdef HARDWARE_OMX

                mExifParams.wb = EXIF_WB_MANUAL;

#endif

            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) WHITE_BALANCE_HORIZON) == 0) {

                fobj->settings.awb.mode = ICAM_WHITE_BALANCE_MODE_WB_HORIZON;

#ifdef HARDWARE_OMX

                mExifParams.wb = EXIF_WB_MANUAL;

#endif
            } else if (strcmp(params.get(CameraParameters::KEY_WHITE_BALANCE), (const char *) WHITE_BALANCE_TUNGSTEN) == 0) {

                fobj->settings.awb.mode = ICAM_WHITE_BALANCE_MODE_WB_TUNGSTEN;

#ifdef HARDWARE_OMX

                mExifParams.wb = EXIF_WB_MANUAL;

#endif
            }

        }

        if ( params.get(CameraParameters::KEY_EFFECT) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_NONE ) == 0) {

                fobj->settings.general.effects = ICAM_EFFECT_NORMAL;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_MONO) == 0) {

                fobj->settings.general.effects = ICAM_EFFECT_GRAYSCALE;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_NEGATIVE) == 0) {

                fobj->settings.general.effects = ICAM_EFFECT_NEGATIVE;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_SOLARIZE) == 0) {

                fobj->settings.general.effects = ICAM_EFFECT_SOLARIZE;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_SEPIA) == 0) {

                fobj->settings.general.effects = ICAM_EFFECT_SEPIA;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_WHITEBOARD) == 0) {

                fobj->settings.general.effects = ICAM_EFFECT_WHITEBOARD;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) CameraParameters::EFFECT_BLACKBOARD) == 0) {

                fobj->settings.general.effects = ICAM_EFFECT_BLACKBOARD;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) EFFECT_COOL) == 0) {

                fobj->settings.general.effects = ICAM_EFFECT_COOL;

            } else if (strcmp(params.get(CameraParameters::KEY_EFFECT), (const char *) EFFECT_EMBOSS) == 0) {

                fobj->settings.general.effects = ICAM_EFFECT_EMBOSS;

            }
        }

        if ( params.get(CameraParameters::KEY_ANTIBANDING) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_ANTIBANDING), (const char *) CameraParameters::ANTIBANDING_50HZ ) == 0) {

                fobj->settings.general.flicker_avoidance = ICAM_FLICKER_AVOIDANCE_50HZ;

            } else if (strcmp(params.get(CameraParameters::KEY_ANTIBANDING), (const char *) CameraParameters::ANTIBANDING_60HZ) == 0) {

                fobj->settings.general.flicker_avoidance = ICAM_FLICKER_AVOIDANCE_60HZ;

            } else if (strcmp(params.get(CameraParameters::KEY_ANTIBANDING), (const char *) CameraParameters::ANTIBANDING_OFF) == 0) {

                fobj->settings.general.flicker_avoidance = ICAM_FLICKER_AVOIDANCE_NO;

            }
        }

        if ( params.get(CameraParameters::KEY_FOCUS_MODE) != NULL ) {
            if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) CameraParameters::FOCUS_MODE_AUTO ) == 0) {

                fobj->settings.af.focus_mode = ICAM_FOCUS_MODE_AF_AUTO;

            } else if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) CameraParameters::FOCUS_MODE_INFINITY) == 0) {

                fobj->settings.af.focus_mode = ICAM_FOCUS_MODE_AF_INFINY;

            } else if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) CameraParameters::FOCUS_MODE_MACRO) == 0) {

                fobj->settings.af.focus_mode = ICAM_FOCUS_MODE_AF_MACRO;

            } else if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) CameraParameters::FOCUS_MODE_FIXED) == 0) {

                //FOCUS_MODE_FIXED in CameraParameters is actually HYPERFOCAL
                //according to api
                fobj->settings.af.focus_mode = ICAM_FOCUS_MODE_AF_HYPERFOCAL;

            } else if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO) == 0) {

                fobj->settings.af.focus_mode = ICAM_FOCUS_MODE_AF_CONTINUOUS;

            } else if (strcmp(params.get(CameraParameters::KEY_FOCUS_MODE), (const char *) FOCUS_MODE_MANUAL) == 0) {

                fobj->settings.af.focus_mode = ICAM_FOCUS_MODE_AF_MANUAL;
                fobj->settings.af.focus_manual = MANUAL_FOCUS_DEFAULT_POSITION;

            }

            //Disable touch focus if enabled
            fobj->settings.general.face_tracking.enable = 0;
            fobj->settings.general.face_tracking.count = 0;
            fobj->settings.general.face_tracking.update = 1;

        }

        valstr = mParameters.get(KEY_TOUCH_FOCUS);
        if(NULL != valstr) {
            char *valstr_copy = (char *)malloc(ARRAY_SIZE(valstr) + 1);
            if(NULL != valstr_copy){
                if ( strcmp(valstr, (const char *) TOUCH_FOCUS_DISABLED) != 0) {

                    //make a copy of valstr, because strtok() overrides the whole mParameters structure
                    strcpy(valstr_copy, valstr);
                    int af_x = 0;
                    int af_y = 0;

                    af_coord = strtok((char *) valstr_copy, PARAMS_DELIMITER);

                    if( NULL != af_coord){
                        af_x = atoi(af_coord);
                    }

                    af_coord = strtok(NULL, PARAMS_DELIMITER);

                    if( NULL != af_coord){
                        af_y = atoi(af_coord);
                    }

                    fobj->settings.general.face_tracking.enable = 1;
                    fobj->settings.general.face_tracking.count = 1;
                    fobj->settings.general.face_tracking.update = 1;
                    fobj->settings.general.face_tracking.faces[0].top = af_y;
                    fobj->settings.general.face_tracking.faces[0].left = af_x;
                    fobj->settings.af.focus_mode = ICAM_FOCUS_MODE_AF_EXTENDED;

                    LOGD("NEW PARAMS: af_x = %d, af_y = %d", af_x, af_y);
                }
                free(valstr_copy);
                valstr_copy = NULL;
            }
            //Disable touch focus if enabled.
            mParameters.set(KEY_TOUCH_FOCUS, TOUCH_FOCUS_DISABLED);
        }

        if ( params.get(KEY_ISO) != NULL ) {
            if (strcmp(params.get(KEY_ISO), (const char *) ISO_AUTO ) == 0) {

                fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_AUTO;

#ifdef HARDWARE_OMX

                mExifParams.iso = EXIF_ISO_AUTO;

#endif

            } else if (strcmp(params.get(KEY_ISO), (const char *) ISO_100 ) == 0) {

                fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_100;

#ifdef HARDWARE_OMX

                mExifParams.iso = EXIF_ISO_100;

#endif

            } else if (strcmp(params.get(KEY_ISO), (const char *) ISO_200 ) == 0) {

                fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_200;

#ifdef HARDWARE_OMX

                mExifParams.iso = EXIF_ISO_200;

#endif

            } else if (strcmp(params.get(KEY_ISO), (const char *) ISO_400 ) == 0) {

                fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_400;

#ifdef HARDWARE_OMX

                mExifParams.iso = EXIF_ISO_400;

#endif

            } else if (strcmp(params.get(KEY_ISO), (const char *) ISO_800 ) == 0) {

                fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_800;

#ifdef HARDWARE_OMX

                mExifParams.iso = EXIF_ISO_800;

#endif

            } else if (strcmp(params.get(KEY_ISO), (const char *) ISO_1000 ) == 0) {

                fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_1000;

#ifdef HARDWARE_OMX

                mExifParams.iso = EXIF_ISO_1000;

#endif

            } else if (strcmp(params.get(KEY_ISO), (const char *) ISO_1200 ) == 0) {

                fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_1600;

#ifdef HARDWARE_OMX

                mExifParams.iso = EXIF_ISO_1200;

#endif

            } else if (strcmp(params.get(KEY_ISO), (const char *) ISO_1600 ) == 0) {

                fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_1600;

#ifdef HARDWARE_OMX

                mExifParams.iso = EXIF_ISO_1600;

#endif

            } else {

                fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_AUTO;

#ifdef HARDWARE_OMX

                mExifParams.iso = EXIF_ISO_AUTO;

#endif

            }
        } else {

            fobj->settings.ae.iso = ICAM_EXPOSURE_ISO_AUTO;

#ifdef HARDWARE_OMX

            mExifParams.iso = EXIF_ISO_AUTO;

#endif

        }

        if ( params.get(KEY_EXPOSURE_MODE) != NULL ) {
            if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_AUTO ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_AUTO;

            } else if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_MACRO ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_MACRO;

            } else if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_PORTRAIT ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_PORTRAIT;

            } else if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_LANDSCAPE ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_LANDSCAPE;

            } else if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_SPORTS ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_SPORTS;

            } else if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_NIGHT ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_NIGHT;

            } else if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_NIGHT_PORTRAIT ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_NIGHT_PORTRAIT;

            } else if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_BACKLIGHTING ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_BACKLIGHTING;

            } else if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_MANUAL ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_MANUAL;

            } else if (strcmp(params.get(KEY_EXPOSURE_MODE), (const char *) EXPOSURE_VERYLONG ) == 0) {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_VERYLONG;

            } else {

                fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_AUTO;

            }
        } else {

            fobj->settings.ae.mode = ICAM_EXPOSURE_MODE_EXP_AUTO;

        }

        compensation = mParameters.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
        saturation = mParameters.getInt(KEY_SATURATION);
        sharpness = mParameters.getInt(KEY_SHARPNESS);
        contrast = mParameters.getInt(KEY_CONTRAST);
        brightness = mParameters.getInt(KEY_BRIGHTNESS);

        if(contrast != -1) {
            contrast -= CONTRAST_OFFSET;
            fobj->settings.general.contrast = contrast;
        }

        if(brightness != -1) {
            brightness -= BRIGHTNESS_OFFSET;
            fobj->settings.general.brightness = brightness;
            LOGE("Brightness passed to 3A %d", brightness);
        }

        if(saturation!= -1) {
            saturation -= SATURATION_OFFSET;
            fobj->settings.general.saturation = saturation;
            LOGE("Saturation passed to 3A %d", saturation);
        }
        if(sharpness != -1)
            fobj->settings.general.sharpness = sharpness;

        fobj->settings.ae.compensation = compensation;

        FW3A_SetSettings();

        if(mParameters.getInt(KEY_ROTATION_TYPE) == ROTATION_EXIF) {
            mExifParams.rotation = rotation;
            rotation = 0; // reset rotation so encoder doesn't not perform any rotation
        } else {
            mExifParams.rotation = -1;
        }

        if ( mLastFocusMode != fobj->settings.af.focus_mode ) {
            Message msg;
            if ( mLastFocusMode == ICAM_FOCUS_MODE_AF_CONTINUOUS ) {
                mcaf = false;
                    msg.command = PREVIEW_CAF_STOP;
                    previewThreadCommandQ.put(&msg);
                    mLock.unlock();
                    previewThreadAckQ.get(&msg);
                    if ( msg.command != PREVIEW_ACK ) return INVALID_OPERATION;
            }

            if ( fobj->settings.af.focus_mode == ICAM_FOCUS_MODE_AF_CONTINUOUS ) {
                mcaf = true;
                    msg.command = PREVIEW_CAF_START;
                    previewThreadCommandQ.put(&msg);
                    mLock.unlock();
                    previewThreadAckQ.get(&msg);
                    if ( msg.command != PREVIEW_ACK ) return INVALID_OPERATION;
            }

            if (( fobj->settings.af.focus_mode == ICAM_FOCUS_MODE_AF_HYPERFOCAL )||
                ( fobj->settings.af.focus_mode == ICAM_FOCUS_MODE_AF_INFINY )) {
                if (mPreviewRunning) {
                    msg.command = PREVIEW_AF_START;
                    previewThreadCommandQ.put(&msg);
                    mLock.unlock();
                    previewThreadAckQ.get(&msg);
                    if ( msg.command != PREVIEW_ACK ) return INVALID_OPERATION;
                }
            }

            mLastFocusMode = fobj->settings.af.focus_mode;
        }

    }

#endif

    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR;
}

CameraParameters CameraHal::getParameters() const
{
    CameraParameters params;

    LOG_FUNCTION_NAME

    {
        Mutex::Autolock lock(mLock);
        params = mParameters;
    }

#ifdef FW3A

    //check if fobj is created in order to get settings from 3AFW
    if ( NULL != fobj ) {

        if( FW3A_GetSettings() < 0 ) {
            LOGE("ERROR FW3A_GetSettings()");
            goto exit;
        }

        switch ( fobj->settings.general.scene ) {
            case ICAM_SCENE_MODE_MANUAL:
                params.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_AUTO);
                break;
            case ICAM_SCENE_MODE_PORTRAIT:
                params.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_PORTRAIT);
                break;
            case ICAM_SCENE_MODE_LANDSCAPE:
                params.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_LANDSCAPE);
                break;
            case ICAM_SCENE_MODE_NIGHT:
                params.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_NIGHT);
                break;
            case ICAM_SCENE_MODE_FIREWORKS:
                params.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_FIREWORKS);
                break;
            case ICAM_SCENE_MODE_SPORT:
                params.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_ACTION);
                break;
            //TODO: Extend support for those
            case ICAM_SCENE_MODE_CLOSEUP:
                break;
            case ICAM_SCENE_MODE_UNDERWATER:
                break;
            case ICAM_SCENE_MODE_SNOW_BEACH:
                params.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_SNOW);
                break;
            case ICAM_SCENE_MODE_MOOD:
                break;
            case ICAM_SCENE_MODE_NIGHT_INDOOR:
                break;
            case ICAM_SCENE_MODE_NIGHT_PORTRAIT:
                params.set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_NIGHT_PORTRAIT);
                break;
            case ICAM_SCENE_MODE_INDOOR:
                break;
            case ICAM_SCENE_MODE_AUTO:
                break;
        };

        switch ( fobj->settings.ae.mode ) {
            case ICAM_EXPOSURE_MODE_EXP_AUTO:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_AUTO);
                break;
            case ICAM_EXPOSURE_MODE_EXP_MACRO:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_MACRO);
                break;
            case ICAM_EXPOSURE_MODE_EXP_PORTRAIT:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_PORTRAIT);
                break;
            case ICAM_EXPOSURE_MODE_EXP_LANDSCAPE:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_LANDSCAPE);
                break;
            case ICAM_EXPOSURE_MODE_EXP_SPORTS:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_SPORTS);
                break;
            case ICAM_EXPOSURE_MODE_EXP_NIGHT:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_NIGHT);
                break;
            case ICAM_EXPOSURE_MODE_EXP_NIGHT_PORTRAIT:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_NIGHT_PORTRAIT);
                break;
            case ICAM_EXPOSURE_MODE_EXP_BACKLIGHTING:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_BACKLIGHTING);
                break;
            case ICAM_EXPOSURE_MODE_EXP_MANUAL:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_MANUAL);
                break;
            case ICAM_EXPOSURE_MODE_EXP_VERYLONG:
                params.set(KEY_EXPOSURE_MODE, EXPOSURE_VERYLONG);
                break;
        };

        switch ( fobj->settings.ae.iso ) {
            case ICAM_EXPOSURE_ISO_AUTO:
                params.set(KEY_ISO, ISO_AUTO);
                break;
            case ICAM_EXPOSURE_ISO_100:
                params.set(KEY_ISO, ISO_100);
                break;
            case ICAM_EXPOSURE_ISO_200:
                params.set(KEY_ISO, ISO_200);
                break;
            case ICAM_EXPOSURE_ISO_400:
                params.set(KEY_ISO, ISO_400);
                break;
            case ICAM_EXPOSURE_ISO_800:
                params.set(KEY_ISO, ISO_800);
                break;
            case ICAM_EXPOSURE_ISO_1000:
                params.set(KEY_ISO, ISO_1000);
                break;
            case ICAM_EXPOSURE_ISO_1200:
                params.set(KEY_ISO, ISO_1200);
                break;
            case ICAM_EXPOSURE_ISO_1600:
                params.set(KEY_ISO, ISO_1600);
                break;
        };

        switch ( fobj->settings.af.focus_mode ) {
            case ICAM_FOCUS_MODE_AF_AUTO:
                params.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_AUTO);
                break;
            case ICAM_FOCUS_MODE_AF_INFINY:
                params.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_INFINITY);
                break;
            case ICAM_FOCUS_MODE_AF_MACRO:
                params.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_MACRO);
                break;
            case ICAM_FOCUS_MODE_AF_MANUAL:
                params.set(CameraParameters::KEY_FOCUS_MODE, FOCUS_MODE_MANUAL);
                break;
            //TODO: Extend support for those
            case ICAM_FOCUS_MODE_AF_CONTINUOUS:
                params.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_CONTINUOUS_VIDEO);
                break;
            case ICAM_FOCUS_MODE_AF_CONTINUOUS_NORMAL:
                break;
            case ICAM_FOCUS_MODE_AF_PORTRAIT:
                break;
            case ICAM_FOCUS_MODE_AF_HYPERFOCAL:
                params.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_FIXED);
                break;
            case ICAM_FOCUS_MODE_AF_EXTENDED:
                break;
            case ICAM_FOCUS_MODE_AF_CONTINUOUS_EXTENDED:
                break;
            default:
                params.set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_AUTO);
                break;
        };

        switch ( fobj->settings.general.flicker_avoidance ) {
            case ICAM_FLICKER_AVOIDANCE_50HZ:
                params.set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_50HZ);
                break;
            case ICAM_FLICKER_AVOIDANCE_60HZ:
                params.set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_60HZ);
                break;
            case ICAM_FLICKER_AVOIDANCE_NO:
                params.set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);
                break;
        };

        switch ( fobj->settings.general.effects ) {
            case ICAM_EFFECT_EMBOSS:
                params.set(CameraParameters::KEY_EFFECT, EFFECT_EMBOSS);
                break;
            case ICAM_EFFECT_COOL:
                params.set(CameraParameters::KEY_EFFECT, EFFECT_COOL);
                break;
            case ICAM_EFFECT_BLACKBOARD:
                params.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_BLACKBOARD);
                break;
            case ICAM_EFFECT_WHITEBOARD:
                params.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_WHITEBOARD);
                break;
            case ICAM_EFFECT_SEPIA:
                params.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_SEPIA);
                break;
            case ICAM_EFFECT_SOLARIZE:
                params.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_SOLARIZE);
                break;
            case ICAM_EFFECT_NEGATIVE:
                params.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NEGATIVE);
                break;
            case ICAM_EFFECT_GRAYSCALE:
                params.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_MONO);
                break;
            case ICAM_EFFECT_NORMAL:
                params.set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NONE);
                break;
            //TODO: Add support for those
            case ICAM_EFFECT_NATURAL:
                break;
            case ICAM_EFFECT_VIVID:
                break;
            case ICAM_EFFECT_COLORSWAP:
                break;
            case ICAM_EFFECT_OUT_OF_FOCUS:
                break;
            case ICAM_EFFECT_NEGATIVE_SEPIA:
                break;
        };

        switch ( fobj->settings.awb.mode ) {
            case ICAM_WHITE_BALANCE_MODE_WB_SHADOW:
                params.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_SHADE);
                break;
            case ICAM_WHITE_BALANCE_MODE_WB_CLOUDY:
                params.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT);
                break;
            case ICAM_WHITE_BALANCE_MODE_WB_DAYLIGHT:
                params.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_DAYLIGHT);
                break;
            case ICAM_WHITE_BALANCE_MODE_WB_FLUORESCENT:
                params.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_FLUORESCENT);
                break;
            case ICAM_WHITE_BALANCE_MODE_WB_INCANDESCENT:
                params.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_INCANDESCENT);
                break;
            case ICAM_WHITE_BALANCE_MODE_WB_AUTO:
                params.set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);
                break;
            case ICAM_WHITE_BALANCE_MODE_WB_HORIZON:
                params.set(CameraParameters::KEY_WHITE_BALANCE, WHITE_BALANCE_HORIZON);
                break;
            //TODO: Extend support for those
            case ICAM_WHITE_BALANCE_MODE_WB_MANUAL:
                break;
            case ICAM_WHITE_BALANCE_MODE_WB_TUNGSTEN:
                params.set(CameraParameters::KEY_WHITE_BALANCE, WHITE_BALANCE_TUNGSTEN);
                break;
            case ICAM_WHITE_BALANCE_MODE_WB_OFFICE:
                break;
            case ICAM_WHITE_BALANCE_MODE_WB_FLASH:
                break;
        };

        switch ( fobj->settings.ae.spot_weighting ) {
            case ICAM_EXPOSURE_SPOT_NORMAL:
                params.set(KEY_METER_MODE, METER_MODE_AVERAGE);
                break;
            case ICAM_EXPOSURE_SPOT_CENTER:
                params.set(KEY_METER_MODE, METER_MODE_CENTER);
                break;
            //TODO: support this also
            case ICAM_EXPOSURE_SPOT_WIDE:
                break;
        };

        params.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, fobj->settings.ae.compensation);
        params.set(KEY_SATURATION, ( fobj->settings.general.saturation + SATURATION_OFFSET ));
        params.set(KEY_SHARPNESS, fobj->settings.general.sharpness);
        params.set(KEY_CONTRAST, ( fobj->settings.general.contrast + CONTRAST_OFFSET ));
        params.set(KEY_BRIGHTNESS, ( fobj->settings.general.brightness + BRIGHTNESS_OFFSET ));

        if (useFramerateRange) {
            //Gingerbread
            int framerate_min, framerate_max;
            params.getPreviewFpsRange(&framerate_min, &framerate_max);
            char fpsrang[32];
            //Scene may change the fps, so return fps range to upper layers. If VBR is set:
            if ( 0 == fobj->settings.ae.framerate) {
                sprintf(fpsrang, "%d000,%d", ((fobj->settings.ae.framerate<MIN_FPS)?MIN_FPS:fobj->settings.ae.framerate), framerate_max);
                params.set(KEY_PREVIEW_FPS_RANGE, fpsrang);
            }
            //If CBR is set, fps range is e.g. (30,30):
            else {
                sprintf(fpsrang, "%d000,%d000", ((fobj->settings.ae.framerate<MIN_FPS)?MIN_FPS:fobj->settings.ae.framerate), ((fobj->settings.ae.framerate<MIN_FPS)?MIN_FPS:fobj->settings.ae.framerate));
                params.set(KEY_PREVIEW_FPS_RANGE, fpsrang);
            }
        }
        else {
            if ( 0 != fobj->settings.ae.framerate )
            params.setPreviewFrameRate((fobj->settings.ae.framerate<MIN_FPS)?MIN_FPS:fobj->settings.ae.framerate);
        }
    }
#endif

exit:

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
#ifdef IMAGE_PROCESSING_PIPELINE
void CameraHal::onIPP(void *priv, icap_ipp_parameters_t *ipp_params)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOG_FUNCTION_NAME

    if ( ipp_params->type == ICAP_IPP_PARAMETERS_VER1_9 ) {
        camHal->mIPPParams.EdgeEnhancementStrength = ipp_params->ipp19.EdgeEnhancementStrength;
        camHal->mIPPParams.WeakEdgeThreshold = ipp_params->ipp19.WeakEdgeThreshold;
        camHal->mIPPParams.StrongEdgeThreshold = ipp_params->ipp19.StrongEdgeThreshold;
        camHal->mIPPParams.LowFreqLumaNoiseFilterStrength = ipp_params->ipp19.LowFreqLumaNoiseFilterStrength;
        camHal->mIPPParams.MidFreqLumaNoiseFilterStrength = ipp_params->ipp19.MidFreqLumaNoiseFilterStrength;
        camHal->mIPPParams.HighFreqLumaNoiseFilterStrength = ipp_params->ipp19.HighFreqLumaNoiseFilterStrength;
        camHal->mIPPParams.LowFreqCbNoiseFilterStrength = ipp_params->ipp19.LowFreqCbNoiseFilterStrength;
        camHal->mIPPParams.MidFreqCbNoiseFilterStrength = ipp_params->ipp19.MidFreqCbNoiseFilterStrength;
        camHal->mIPPParams.HighFreqCbNoiseFilterStrength = ipp_params->ipp19.HighFreqCbNoiseFilterStrength;
        camHal->mIPPParams.LowFreqCrNoiseFilterStrength = ipp_params->ipp19.LowFreqCrNoiseFilterStrength;
        camHal->mIPPParams.MidFreqCrNoiseFilterStrength = ipp_params->ipp19.MidFreqCrNoiseFilterStrength;
        camHal->mIPPParams.HighFreqCrNoiseFilterStrength = ipp_params->ipp19.HighFreqCrNoiseFilterStrength;
        camHal->mIPPParams.shadingVertParam1 = ipp_params->ipp19.shadingVertParam1;
        camHal->mIPPParams.shadingVertParam2 = ipp_params->ipp19.shadingVertParam2;
        camHal->mIPPParams.shadingHorzParam1 = ipp_params->ipp19.shadingHorzParam1;
        camHal->mIPPParams.shadingHorzParam2 = ipp_params->ipp19.shadingHorzParam2;
        camHal->mIPPParams.shadingGainScale = ipp_params->ipp19.shadingGainScale;
        camHal->mIPPParams.shadingGainOffset = ipp_params->ipp19.shadingGainOffset;
        camHal->mIPPParams.shadingGainMaxValue = ipp_params->ipp19.shadingGainMaxValue;
        camHal->mIPPParams.ratioDownsampleCbCr = ipp_params->ipp19.ratioDownsampleCbCr;
    } else if ( ipp_params->type == ICAP_IPP_PARAMETERS_VER1_8 ) {
        camHal->mIPPParams.EdgeEnhancementStrength = ipp_params->ipp18.ee_q;
        camHal->mIPPParams.WeakEdgeThreshold = ipp_params->ipp18.ew_ts;
        camHal->mIPPParams.StrongEdgeThreshold = ipp_params->ipp18.es_ts;
        camHal->mIPPParams.LowFreqLumaNoiseFilterStrength = ipp_params->ipp18.luma_nf;
        camHal->mIPPParams.MidFreqLumaNoiseFilterStrength = ipp_params->ipp18.luma_nf;
        camHal->mIPPParams.HighFreqLumaNoiseFilterStrength = ipp_params->ipp18.luma_nf;
        camHal->mIPPParams.LowFreqCbNoiseFilterStrength = ipp_params->ipp18.chroma_nf;
        camHal->mIPPParams.MidFreqCbNoiseFilterStrength = ipp_params->ipp18.chroma_nf;
        camHal->mIPPParams.HighFreqCbNoiseFilterStrength = ipp_params->ipp18.chroma_nf;
        camHal->mIPPParams.LowFreqCrNoiseFilterStrength = ipp_params->ipp18.chroma_nf;
        camHal->mIPPParams.MidFreqCrNoiseFilterStrength = ipp_params->ipp18.chroma_nf;
        camHal->mIPPParams.HighFreqCrNoiseFilterStrength = ipp_params->ipp18.chroma_nf;
        camHal->mIPPParams.shadingVertParam1 = -1;
        camHal->mIPPParams.shadingVertParam2 = -1;
        camHal->mIPPParams.shadingHorzParam1 = -1;
        camHal->mIPPParams.shadingHorzParam2 = -1;
        camHal->mIPPParams.shadingGainScale = -1;
        camHal->mIPPParams.shadingGainOffset = -1;
        camHal->mIPPParams.shadingGainMaxValue = -1;
        camHal->mIPPParams.ratioDownsampleCbCr = -1;
    } else {
        camHal->mIPPParams.EdgeEnhancementStrength = 220;
        camHal->mIPPParams.WeakEdgeThreshold = 8;
        camHal->mIPPParams.StrongEdgeThreshold = 200;
        camHal->mIPPParams.LowFreqLumaNoiseFilterStrength = 5;
        camHal->mIPPParams.MidFreqLumaNoiseFilterStrength = 10;
        camHal->mIPPParams.HighFreqLumaNoiseFilterStrength = 15;
        camHal->mIPPParams.LowFreqCbNoiseFilterStrength = 20;
        camHal->mIPPParams.MidFreqCbNoiseFilterStrength = 30;
        camHal->mIPPParams.HighFreqCbNoiseFilterStrength = 10;
        camHal->mIPPParams.LowFreqCrNoiseFilterStrength = 10;
        camHal->mIPPParams.MidFreqCrNoiseFilterStrength = 25;
        camHal->mIPPParams.HighFreqCrNoiseFilterStrength = 15;
        camHal->mIPPParams.shadingVertParam1 = 10;
        camHal->mIPPParams.shadingVertParam2 = 400;
        camHal->mIPPParams.shadingHorzParam1 = 10;
        camHal->mIPPParams.shadingHorzParam2 = 400;
        camHal->mIPPParams.shadingGainScale = 128;
        camHal->mIPPParams.shadingGainOffset = 2048;
        camHal->mIPPParams.shadingGainMaxValue = 16384;
        camHal->mIPPParams.ratioDownsampleCbCr = 1;
    }

    LOG_FUNCTION_NAME_EXIT
}
#endif

void CameraHal::onGeneratedSnapshot(void *priv, icap_image_buffer_t *buf)
{
    unsigned int snapshotMessage[1];

    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOG_FUNCTION_NAME

    snapshotMessage[0] = SNAPSHOT_THREAD_START_GEN;
    write(camHal->snapshotPipe[1], &snapshotMessage, sizeof(snapshotMessage));

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::onSnapshot(void *priv, icap_image_buffer_t *buf)
{
    unsigned int snapshotMessage[9];

    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOG_FUNCTION_NAME

    snapshotMessage[0] = SNAPSHOT_THREAD_START;
    snapshotMessage[1] = (unsigned int) buf->buffer.buffer;
    snapshotMessage[2] = buf->width;
    snapshotMessage[3] = buf->height;
    snapshotMessage[4] = camHal->mZoomTargetIdx;
    snapshotMessage[5] = camHal->mImageCropLeft;
    snapshotMessage[6] = camHal->mImageCropTop;
    snapshotMessage[7] = camHal->mImageCropWidth;
    snapshotMessage[8] = camHal->mImageCropHeight;

    write(camHal->snapshotPipe[1], &snapshotMessage, sizeof(snapshotMessage));

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::onShutter(void *priv, icap_image_buffer_t *image_buf)
{
    LOG_FUNCTION_NAME

    unsigned int shutterMessage[3];

    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    if( ( camHal->msgTypeEnabled(CAMERA_MSG_SHUTTER) ) && (camHal->mShutterEnable) ) {

#ifdef DEBUG_LOG

        camHal->PPM("SENDING MESSAGE TO SHUTTER THREAD");

#endif

        shutterMessage[0] = SHUTTER_THREAD_CALL;
        shutterMessage[1] = (unsigned int) camHal->mNotifyCb;
        shutterMessage[2] = (unsigned int) camHal->mCallbackCookie;
        write(camHal->shutterPipe[1], &shutterMessage, sizeof(shutterMessage));
    }

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::onCrop(void *priv,  icap_crop_rect_t *crop_rect)
{
    LOG_FUNCTION_NAME

    CameraHal *camHal = reinterpret_cast<CameraHal *> (priv);

    camHal->mImageCropTop = crop_rect->top;
    camHal->mImageCropLeft = crop_rect->left;
    camHal->mImageCropWidth = crop_rect->width;
    camHal->mImageCropHeight = crop_rect->height;

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::onMakernote(void *priv, void *mknote_ptr)
{
    LOG_FUNCTION_NAME

    CameraHal *camHal = reinterpret_cast<CameraHal *>(priv);
    SICam_MakerNote *makerNote = reinterpret_cast<SICam_MakerNote *>(mknote_ptr);

#ifdef DUMP_MAKERNOTE

    camHal->SaveFile(NULL, (char*)"mnt", makerNote->buffer, makerNote->filled_size);

#endif

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::onSaveH3A(void *priv, icap_image_buffer_t *buf)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOGD("Observer onSaveH3A\n");
    camHal->SaveFile(NULL, (char*)"h3a", buf->buffer.buffer, buf->buffer.filled_size);
}

void CameraHal::onSaveLSC(void *priv, icap_image_buffer_t *buf)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOGD("Observer onSaveLSC\n");
    camHal->SaveFile(NULL, (char*)"lsc", buf->buffer.buffer, buf->buffer.filled_size);
}

void CameraHal::onSaveRAW(void *priv, icap_image_buffer_t *buf)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOGD("Observer onSaveRAW\n");
    camHal->SaveFile(NULL, (char*)"raw", buf->buffer.buffer, buf->buffer.filled_size);
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
        LOGD("Jpeg size %d buffer 0x%x", size, ( unsigned int ) buffer);
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

sp<CameraHardwareInterface> CameraHal::createInstance(int cameraId)
{
    LOG_FUNCTION_NAME

    if (singleton[cameraId] != 0) {
        sp<CameraHardwareInterface> hardware = singleton[cameraId].promote();
        if (hardware != 0) {
            return hardware;
        }
    }

    sp<CameraHardwareInterface> hardware(new CameraHal(cameraId));

    singleton[cameraId] = hardware;
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
    LOG_FUNCTION_NAME

    CameraParameters p;
    Message msg;
    const char * mFocusMode;

    // Disable focus messaging here. When application requests cancelAutoFocus(),
    // it does not expect any AF callbacks. AF should be done in order to return the lens
    // back to "default" position. In this case we set an average manual focus position.
    // From the other side, in previewthread() state machine we always send callback to
    // application when we focus, so disable focus messages.
    disableMsgType(CAMERA_MSG_FOCUS);

    msg.command = PREVIEW_AF_STOP;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

    if ( msg.command != PREVIEW_ACK )
        LOGE("AF Stop Failed or AF already stopped");
    else
        LOGD("AF Stopped");

    p = getParameters();

    // Get current focus mode
    mFocusMode = p.get(CameraParameters::KEY_FOCUS_MODE);
    p.set(CameraParameters::KEY_FOCUS_MODE, FOCUS_MODE_MANUAL);

    if (setParameters(p) != NO_ERROR) {
        LOGE("Failed to set parameters");
    }

    msg.command = PREVIEW_AF_START;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

    if ( msg.command != PREVIEW_ACK )
        LOGE("Lens didn't go to default position");
    else
        LOGD("Lens went to default position");

    // Return back the focus mode which is set in CameraParamerers
    p.set(CameraParameters::KEY_FOCUS_MODE, mFocusMode);

    if (setParameters(p) != NO_ERROR) {
        LOGE("Failed to set parameters");
    }

    enableMsgType(CAMERA_MSG_FOCUS);

    LOG_FUNCTION_NAME_EXIT

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

#ifdef DEBUG_LOG
void CameraHal::debugShowBufferStatus()
{
    LOGE("nOverlayBuffersQueued=%d", nOverlayBuffersQueued);
    LOGE("nCameraBuffersQueued=%d", nCameraBuffersQueued);
    LOGE("mVideoBufferCount=%d", mVideoBufferCount);
    for (int i=0; i< VIDEO_FRAME_COUNT_MAX; i++) {
        LOGE("mVideoBufferStatus[%d]=%d", i, mVideoBufferStatus[i]);
    }
}
#endif

static const CameraInfo sCameraInfo[] = {
    {
        CAMERA_FACING_BACK,
        90,  /* orientation */
    },
#ifdef SECONDARY_CAMERA
    {
        CAMERA_FACING_FRONT,
        270, /* orientation */
    }
#endif
};


extern "C" int HAL_getNumberOfCameras()
{
    int numCameras = 0;

    // FIXME: Zoom3 has only one camera device.
    numCameras = sizeof(sCameraInfo) / sizeof(sCameraInfo[0]);
    if ( 0 == numCameras )
    {
        LOGE("No cameras supported in Camera HAL implementation");
        return 0;
    }
    else
    {
        LOGD("Cameras found %d", numCameras);
    }

    return numCameras;
}

extern "C" void HAL_getCameraInfo(int cameraId, struct CameraInfo* cameraInfo)
{
    int face_value = CAMERA_FACING_BACK;
    int orientation = 0;
    char *valstr = NULL;

    memcpy(cameraInfo, &sCameraInfo[cameraId], sizeof(CameraInfo));
}

extern "C" sp<CameraHardwareInterface> HAL_openCameraHardware(int cameraId)
{
    LOGD("opening ti camera hal (cameraId:%d)\n", cameraId);
    return CameraHal::createInstance(cameraId);
}


}; // namespace android


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
 *
 * This file contains material that is available under the Apache License
 */


#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/Log.h>
#include <utils/threads.h>
#include <linux/videodev2.h>
#include "binder/MemoryBase.h"
#include "binder/MemoryHeapBase.h"
#include <utils/threads.h>
#include <camera/CameraParameters.h>
#include <hardware/camera.h>
#include <MessageQueue.h>
#include "Semaphore.h"
#include <hardware/camera.h>
#include "DebugUtils.h"
#include "CameraProperties.h"

#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBuffer.h>
#include "JpegEncoder.h"
#include "TICameraParameters.h"

#ifdef HARDWARE_OMX
#include <JpegEncoderEXIF.h>
#endif

//Uncomment to enable more verbose/debug logs
//#define DEBUG_LOG 0

///Camera HAL Logging Functions
#ifndef DEBUG_LOG

#define CAMHAL_LOGDA(str)
#define CAMHAL_LOGDB(str, ...)
#define CAMHAL_LOGVA(str)
#define CAMHAL_LOGVB(str, ...)

#define CAMHAL_LOGEA ALOGE
#define CAMHAL_LOGEB ALOGE

#else

#define CAMHAL_LOGDA DBGUTILS_LOGDA
#define CAMHAL_LOGDB DBGUTILS_LOGDB
#define CAMHAL_LOGVA DBGUTILS_LOGVA
#define CAMHAL_LOGVB DBGUTILS_LOGVB

#define CAMHAL_LOGEA DBGUTILS_LOGEA
#define CAMHAL_LOGEB DBGUTILS_LOGEB

#endif

namespace android {

///Forward declarations
class CameraHal;
class CameraFrame;
class CameraHalEvent;
class DisplayFrame;

class CameraArea : public RefBase
{
public:

    CameraArea(ssize_t top,
               ssize_t left,
               ssize_t bottom,
               ssize_t right,
               size_t weight) : mTop(top),
                                mLeft(left),
                                mBottom(bottom),
                                mRight(right),
                                mWeight(weight) {}

    status_t transfrom(size_t width,
                       size_t height,
                       size_t &top,
                       size_t &left,
                       size_t &areaWidth,
                       size_t &areaHeight);

    bool isValid()
        {
        return ( ( 0 != mTop ) || ( 0 != mLeft ) || ( 0 != mBottom ) || ( 0 != mRight) );
        }

    bool isZeroArea()
    {
        return  ( (0 == mTop ) && ( 0 == mLeft ) && ( 0 == mBottom )
                 && ( 0 == mRight ) && ( 0 == mWeight ));
    }

    size_t getWeight()
        {
        return mWeight;
        }

    bool compare(const sp<CameraArea> &area);

    static status_t parseAreas(const char *area,
                               size_t areaLength,
                               Vector< sp<CameraArea> > &areas);

    static status_t checkArea(ssize_t top,
                              ssize_t left,
                              ssize_t bottom,
                              ssize_t right,
                              ssize_t weight);

    static bool areAreasDifferent(Vector< sp<CameraArea> > &, Vector< sp<CameraArea> > &);

protected:
    static const ssize_t TOP = -1000;
    static const ssize_t LEFT = -1000;
    static const ssize_t BOTTOM = 1000;
    static const ssize_t RIGHT = 1000;
    static const ssize_t WEIGHT_MIN = 1;
    static const ssize_t WEIGHT_MAX = 1000;

    ssize_t mTop;
    ssize_t mLeft;
    ssize_t mBottom;
    ssize_t mRight;
    size_t mWeight;
};

class CameraFDResult : public RefBase
{
public:

    CameraFDResult() : mFaceData(NULL) {};
    CameraFDResult(camera_frame_metadata_t *faces) : mFaceData(faces) {};

    virtual ~CameraFDResult() {
        if ( ( NULL != mFaceData ) && ( NULL != mFaceData->faces ) ) {
            free(mFaceData->faces);
            free(mFaceData);
            mFaceData=NULL;
        }

        if(( NULL != mFaceData ))
            {
            free(mFaceData);
            mFaceData = NULL;
            }
    }

    camera_frame_metadata_t *getFaceResult() { return mFaceData; };

    static const ssize_t TOP = -1000;
    static const ssize_t LEFT = -1000;
    static const ssize_t BOTTOM = 1000;
    static const ssize_t RIGHT = 1000;
    static const ssize_t INVALID_DATA = -2000;

private:

    camera_frame_metadata_t *mFaceData;
};

#define RESIZER 1
#define JPEG 1
#define VPP 1

#define PPM_INSTRUMENTATION 1

#define CAMHAL_GRALLOC_USAGE GRALLOC_USAGE_HW_TEXTURE | \
                             GRALLOC_USAGE_HW_RENDER | \
                             GRALLOC_USAGE_SW_READ_RARELY | \
                             GRALLOC_USAGE_SW_WRITE_NEVER
#define LOCK_BUFFER_TRIES 5

static const int32_t MAX_BUFFERS = 6; //check this - in ICS it is 8

//#undef FW3A
//#undef ICAP
//#undef IMAGE_PROCESSING_PIPELINE

#ifdef FW3A
extern "C" {
#include "icam_icap/icamera.h"
#include "icam_icap/icapture_v2.h"
}
#endif

#ifdef HARDWARE_OMX
#include "JpegEncoder.h"
#endif

#ifdef IMAGE_PROCESSING_PIPELINE
#include "imageprocessingpipeline.h"
#include "ipp_algotypes.h"
#include "capdefs.h"

#define MAXIPPDynamicParams 10

#endif
#define MAX_BURST 15
#define DSP_CACHE_ALIGNMENT 128
#define BUFF_MAP_PADDING_TEST 256
#define DSP_CACHE_ALIGN_MEM_ALLOC(__size__) \
    memalign(DSP_CACHE_ALIGNMENT, __size__ + BUFF_MAP_PADDING_TEST)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define VIDEO_DEVICE        "/dev/video5"
#define MIN_WIDTH           128
#define MIN_HEIGHT          96
#define PICTURE_WIDTH   3264 /* 5mp - 2560. 8mp - 3280 */ /* Make sure it is a multiple of 16. */
#define PICTURE_HEIGHT  2448 /* 5mp - 2048. 8mp - 2464 */ /* Make sure it is a multiple of 16. */
#define PREVIEW_WIDTH 176
#define PREVIEW_HEIGHT 144
#define CAPTURE_8MP_WIDTH        3280
#define CAPTURE_8MP_HEIGHT       2464
#define PIXEL_FORMAT           V4L2_PIX_FMT_UYVY
#define LOG_FUNCTION_NAME    ALOGV("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    ALOGV("%d: %s() EXIT", __LINE__, __FUNCTION__);
#define VIDEO_FRAME_COUNT_MAX    MAX_BUFFERS
#define MAX_CAMERA_BUFFERS    MAX_BUFFERS
#define COMPENSATION_OFFSET 20
#define CONTRAST_OFFSET 100
#define BRIGHTNESS_OFFSET 100
#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define DEFAULT_THUMB_WIDTH     320
#define DEFAULT_THUMB_HEIGHT    240
#define MAX_THUMB_WIDTH     512
#define MAX_THUMB_HEIGHT    384
#define MIN_THUMB_WIDTH    80
#define MIN_THUMB_HEIGHT   60
#define IMX046_FOCALLENGTH 4.68
#define IMX046_HORZANGLE 62.9
#define IMX046_VERTANGLE 24.8
#define MIN_FPS 8
#define MAX_FPS 30
#define FOCUS_DISTANCE_NEAR 0.500000
#define FOCUS_DISTANCE_OPTIMAL 1.500000
#define FOCUS_DISTANCE_BUFFER_SIZE  30
#define MANUAL_FOCUS_DEFAULT_POSITION 50

#define ZOOM_SCALE (1<<16)

#define PIX_YUV422I 0
#define PIX_YUV420P 1
#define KEY_ROTATION_TYPE       "rotation-type"
#define ROTATION_PHYSICAL       0
#define ROTATION_EXIF           1

#define DSP3630_KHZ_MIN 260000
#define DSP3630_KHZ_MAX 800000

#define __ALIGN(x,a) ( (x) & (~((a) - 1)))
#define NEXT_4K_ALIGN_ADDR(x) (((unsigned int) x + 0xfff) & 0xfffff000)

#define NONNEG_ASSIGN(x,y) \
    if(x > -1) \
        y = x

#ifdef IMAGE_PROCESSING_PIPELINE
    #define INPLACE_ON    1
    #define INPLACE_OFF    0
    #define IPP_Disabled_Mode 0
    #define IPP_CromaSupression_Mode 1
    #define IPP_EdgeEnhancement_Mode 2
typedef struct OMX_IPP
{
    IPP_Handle hIPP;
    IPP_ConfigurationTypes ippconfig;
    IPP_CRCBSAlgoCreateParams CRCBptr;
    IPP_EENFAlgoCreateParams EENFcreate;
    IPP_YUVCAlgoCreateParams YUVCcreate;
    void* dynStar;
    void* dynCRCB;
    void* dynEENF;
    IPP_StarAlgoInArgs* iStarInArgs;
    IPP_StarAlgoOutArgs* iStarOutArgs;
    IPP_CRCBSAlgoInArgs* iCrcbsInArgs;
    IPP_CRCBSAlgoOutArgs* iCrcbsOutArgs;
    IPP_EENFAlgoInArgs* iEenfInArgs;
    IPP_EENFAlgoOutArgs* iEenfOutArgs;
    IPP_YUVCAlgoInArgs*  iYuvcInArgs1;
    IPP_YUVCAlgoOutArgs* iYuvcOutArgs1;
    IPP_YUVCAlgoInArgs* iYuvcInArgs2;
    IPP_YUVCAlgoOutArgs* iYuvcOutArgs2;
    IPP_StarAlgoStatus starStatus;
    IPP_CRCBSAlgoStatus CRCBSStatus;
    IPP_EENFAlgoStatus EENFStatus;
    IPP_StatusDesc statusDesc;
    IPP_ImageBufferDesc iInputBufferDesc;
    IPP_ImageBufferDesc iOutputBufferDesc;
    IPP_ProcessArgs iInputArgs;
    IPP_ProcessArgs iOutputArgs;
    int outputBufferSize;
    unsigned char* pIppOutputBuffer;
} OMX_IPP;

typedef struct {
    // Edge Enhancement Parameters
    uint16_t  EdgeEnhancementStrength;
    uint16_t  WeakEdgeThreshold;
    uint16_t  StrongEdgeThreshold;

    // Noise filter parameters
    // LumaNoiseFilterStrength
    uint16_t  LowFreqLumaNoiseFilterStrength;
    uint16_t  MidFreqLumaNoiseFilterStrength;
    uint16_t  HighFreqLumaNoiseFilterStrength;

    // CbNoiseFilterStrength
    uint16_t  LowFreqCbNoiseFilterStrength;
    uint16_t  MidFreqCbNoiseFilterStrength;
    uint16_t  HighFreqCbNoiseFilterStrength;

    // CrNoiseFilterStrength
    uint16_t  LowFreqCrNoiseFilterStrength;
    uint16_t  MidFreqCrNoiseFilterStrength;
    uint16_t  HighFreqCrNoiseFilterStrength;

    uint16_t  shadingVertParam1;
    uint16_t  shadingVertParam2;
    uint16_t  shadingHorzParam1;
    uint16_t  shadingHorzParam2;
    uint16_t  shadingGainScale;
    uint16_t  shadingGainOffset;
    uint16_t  shadingGainMaxValue;

    uint16_t  ratioDownsampleCbCr;
} IPP_PARAMS;

#endif

//icapture
#define DTP_FILE_NAME         "/data/dyntunn.enc"
#define EEPROM_FILE_NAME    "/data/eeprom.hex"
#define LIBICAPTURE_NAME    "libicapture.so"

// 3A FW
#define LIB3AFW             "libMMS3AFW.so"

#define PHOTO_PATH          "/tmp/photo_%02d.%s"

enum {
    PROC_MSG_IDX_ACTION = 0,
    PROC_MSG_IDX_CAPTURE_W,
    PROC_MSG_IDX_CAPTURE_H,
    PROC_MSG_IDX_IMAGE_W,
    PROC_MSG_IDX_IMAGE_H,
    PROC_MSG_IDX_PIX_FMT,
#ifdef IMAGE_PROCESSING_PIPELINE
    PROC_MSG_IDX_IPP_EES,
    PROC_MSG_IDX_IPP_WET,
    PROC_MSG_IDX_IPP_SET,
    PROC_MSG_IDX_IPP_LFLNFS,
    PROC_MSG_IDX_IPP_MFLNFS,
    PROC_MSG_IDX_IPP_HFLNFS,
    PROC_MSG_IDX_IPP_LFCBNFS,
    PROC_MSG_IDX_IPP_MFCBNFS,
    PROC_MSG_IDX_IPP_HFCBNFS,
    PROC_MSG_IDX_IPP_LFCRNFS,
    PROC_MSG_IDX_IPP_MFCRNFS,
    PROC_MSG_IDX_IPP_HFCRNFS,
    PROC_MSG_IDX_IPP_SVP1,
    PROC_MSG_IDX_IPP_SVP2,
    PROC_MSG_IDX_IPP_SHP1,
    PROC_MSG_IDX_IPP_SHP2,
    PROC_MSG_IDX_IPP_SGS,
    PROC_MSG_IDX_IPP_SGO,
    PROC_MSG_IDX_IPP_SGMV,
    PROC_MSG_IDX_IPP_RDSCBCR,
    PROC_MSG_IDX_IPP_MODE,
    PROC_MSG_IDX_IPP_TO_ENABLE,
#endif
    PROC_MSG_IDX_YUV_BUFF,
    PROC_MSG_IDX_YUV_BUFFLEN,
    PROC_MSG_IDX_ROTATION,
    PROC_MSG_IDX_ZOOM,
    PROC_MSG_IDX_JPEG_QUALITY,
    PROC_MSG_IDX_JPEG_CB,
    PROC_MSG_IDX_RAW_CB,
    PROC_MSG_IDX_CB_COOKIE,
    PROC_MSG_IDX_CROP_L,
    PROC_MSG_IDX_CROP_T,
    PROC_MSG_IDX_CROP_W,
    PROC_MSG_IDX_CROP_H,
    PROC_MSG_IDX_THUMB_W,
    PROC_MSG_IDX_THUMB_H,
#ifdef HARDWARE_OMX
    PROC_MSG_IDX_EXIF_BUFF,
#endif
    PROC_MSG_IDX_MAX
};
#define PROC_THREAD_PROCESS         0x5
#define PROC_THREAD_EXIT            0x6

#define SHUTTER_THREAD_CALL         0x1
#define SHUTTER_THREAD_EXIT         0x2
#define SHUTTER_THREAD_NUM_ARGS     3
#define RAW_THREAD_CALL             0x1
#define RAW_THREAD_EXIT             0x2
#define RAW_THREAD_NUM_ARGS         4
#define SNAPSHOT_THREAD_START       0x1
#define SNAPSHOT_THREAD_EXIT        0x2
#define SNAPSHOT_THREAD_START_GEN   0x3

#define PAGE                    0x1000
#define PARAM_BUFFER            512

#ifdef FW3A

typedef struct {
    TICam_Handle hnd;

    /* hold 2A settings */
    SICam_Settings settings;

    /* hold 2A status */
    SICam_Status   status;

    /* hold MakerNote */
    SICam_MakerNote mnote;
} lib3atest_obj;
#endif

#ifdef ICAP
typedef struct{
    void           *lib_private;
    icap_configure_t    cfg;
    icap_tuning_params_t tuning_pars;
    SICap_ManualParameters manual;
    icap_buffer_t   img_buf;
    icap_buffer_t   lsc_buf;
} libtest_obj;
#endif

typedef struct {
    size_t width;
    size_t height;
} supported_resolution;


class CameraFrame
{
    public:

    enum FrameType
        {
            PREVIEW_FRAME_SYNC = 0x1, ///SYNC implies that the frame needs to be explicitly returned after consuming in order to be filled by camera again
            PREVIEW_FRAME = 0x2   , ///Preview frame includes viewfinder and snapshot frames
            IMAGE_FRAME_SYNC = 0x4, ///Image Frame is the image capture output frame
            IMAGE_FRAME = 0x8,
            VIDEO_FRAME_SYNC = 0x10, ///Timestamp will be updated for these frames
            VIDEO_FRAME = 0x20,
            FRAME_DATA_SYNC = 0x40, ///Any extra data assosicated with the frame. Always synced with the frame
            FRAME_DATA= 0x80,
            RAW_FRAME = 0x100,
            SNAPSHOT_FRAME = 0x200,
            ALL_FRAMES = 0xFFFF   ///Maximum of 16 frame types supported
        };

    enum FrameQuirks
    {
        ENCODE_RAW_YUV422I_TO_JPEG = 0x1 << 0,
        HAS_EXIF_DATA = 0x1 << 1,
    };

    //default contrustor
    CameraFrame():
    mCookie(NULL),
    mCookie2(NULL),
    mBuffer(NULL),
    mFrameType(0),
    mTimestamp(0),
    mWidth(0),
    mHeight(0),
    mOffset(0),
    mAlignment(0),
    mFd(0),
    mLength(0),
    mFrameMask(0),
    mQuirks(0) {

      mYuv[0] = NULL;
      mYuv[1] = NULL;
    }

    //copy constructor
    CameraFrame(const CameraFrame &frame) :
    mCookie(frame.mCookie),
    mCookie2(frame.mCookie2),
    mBuffer(frame.mBuffer),
    mFrameType(frame.mFrameType),
    mTimestamp(frame.mTimestamp),
    mWidth(frame.mWidth),
    mHeight(frame.mHeight),
    mOffset(frame.mOffset),
    mAlignment(frame.mAlignment),
    mFd(frame.mFd),
    mLength(frame.mLength),
    mFrameMask(frame.mFrameMask),
    mQuirks(frame.mQuirks) {

      mYuv[0] = frame.mYuv[0];
      mYuv[1] = frame.mYuv[1];
    }

    void *mCookie;
    void *mCookie2;
    void *mBuffer;
    int mFrameType;
    nsecs_t mTimestamp;
    unsigned int mWidth, mHeight;
    uint32_t mOffset;
    unsigned int mAlignment;
    int mFd;
    size_t mLength;
    unsigned mFrameMask;
    unsigned int mQuirks;
    unsigned int mYuv[2];
    ///@todo add other member vars like  stride etc
};

enum CameraHalError
{
    CAMERA_ERROR_FATAL = 0x1, //Fatal errors can only be recovered by restarting media server
    CAMERA_ERROR_HARD = 0x2,  // Hard errors are hardware hangs that may be recoverable by resetting the hardware internally within the adapter
    CAMERA_ERROR_SOFT = 0x4, // Soft errors are non fatal errors that can be recovered from without needing to stop use-case
};

class CameraHalEvent
{
public:
    //Enums
    enum CameraHalEventType {
        NO_EVENTS = 0x0,
        EVENT_FOCUS_LOCKED = 0x1,
        EVENT_FOCUS_ERROR = 0x2,
        EVENT_ZOOM_INDEX_REACHED = 0x4,
        EVENT_SHUTTER = 0x8,
        EVENT_FACE = 0x10,
        ///@remarks Future enum related to display, like frame displayed event, could be added here
        ALL_EVENTS = 0xFFFF ///Maximum of 16 event types supported
    };

    ///Class declarations
    ///@remarks Add a new class for a new event type added above

    //Shutter event specific data
    typedef struct ShutterEventData_t {
        bool shutterClosed;
    }ShutterEventData;

    ///Focus event specific data
    typedef struct FocusEventData_t {
        bool focusLocked;
        bool focusError;
        int currentFocusValue;
    } FocusEventData;

    ///Zoom specific event data
    typedef struct ZoomEventData_t {
        int currentZoomIndex;
        bool targetZoomIndexReached;
    } ZoomEventData;

    typedef struct FaceData_t {
        ssize_t top;
        ssize_t left;
        ssize_t bottom;
        ssize_t right;
        size_t score;
    } FaceData;

    typedef sp<CameraFDResult> FaceEventData;

    class CameraHalEventData : public RefBase{

    public:

        CameraHalEvent::FocusEventData focusEvent;
        CameraHalEvent::ZoomEventData zoomEvent;
        CameraHalEvent::ShutterEventData shutterEvent;
        CameraHalEvent::FaceEventData faceEvent;
    };

    //default contrustor
    CameraHalEvent():
    mCookie(NULL),
    mEventType(NO_EVENTS) {}

    //copy constructor
    CameraHalEvent(const CameraHalEvent &event) :
        mCookie(event.mCookie),
        mEventType(event.mEventType),
        mEventData(event.mEventData) {};

    void* mCookie;
    CameraHalEventType mEventType;
    sp<CameraHalEventData> mEventData;

};

///      Have a generic callback class based on template - to adapt CameraFrame and Event
typedef void (*frame_callback) (CameraFrame *cameraFrame);
typedef void (*event_callback) (CameraHalEvent *event);

//signals CameraHAL to relase image buffers
typedef void (*release_image_buffers_callback) (void *userData);
typedef void (*end_image_capture_callback) (void *userData);


class MessageNotifier
{
public:
    static const uint32_t EVENT_BIT_FIELD_POSITION;
    static const uint32_t FRAME_BIT_FIELD_POSITION;

    ///@remarks Msg type comes from CameraFrame and CameraHalEvent classes
    ///           MSB 16 bits is for events and LSB 16 bits is for frame notifications
    ///         FrameProvider and EventProvider classes act as helpers to event/frame
    ///         consumers to call this api
    virtual void enableMsgType(int32_t msgs, frame_callback frameCb=NULL, event_callback eventCb=NULL, void* cookie=NULL) = 0;
    virtual void disableMsgType(int32_t msgs, void* cookie) = 0;

    virtual ~MessageNotifier() {};
};

class ErrorNotifier : public virtual RefBase
{
public:
    virtual void errorNotify(int error) = 0;

    virtual ~ErrorNotifier() {};
};


/**
  * Interace class abstraction for Camera Adapter to act as a frame provider
  * This interface is fully implemented by Camera Adapter
  */
class FrameNotifier : public MessageNotifier
{
public:
    virtual void returnFrame(void* frameBuf, CameraFrame::FrameType frameType) = 0;
    virtual void addFramePointers(void *frameBuf, void *buf) = 0;
    virtual void removeFramePointers() = 0;

    virtual ~FrameNotifier() {};
};

/**   * Wrapper class around Frame Notifier, which is used by display and notification classes for interacting with Camera Adapter
  */
class FrameProvider
{
    FrameNotifier* mFrameNotifier;
    void* mCookie;
    frame_callback mFrameCallback;

public:
    FrameProvider(FrameNotifier *fn, void* cookie, frame_callback frameCallback)
        :mFrameNotifier(fn), mCookie(cookie),mFrameCallback(frameCallback) { }

    int enableFrameNotification(int32_t frameTypes);
    int disableFrameNotification(int32_t frameTypes);
    int returnFrame(void *frameBuf, CameraFrame::FrameType frameType);
    void addFramePointers(void *frameBuf, void *buf);
    void removeFramePointers();
};

/** Wrapper class around MessageNotifier, which is used by display and notification classes for interacting with
   *  Camera Adapter
  */
class EventProvider
{
public:
    MessageNotifier* mEventNotifier;
    void* mCookie;
    event_callback mEventCallback;

public:
    EventProvider(MessageNotifier *mn, void* cookie, event_callback eventCallback)
        :mEventNotifier(mn), mCookie(cookie), mEventCallback(eventCallback) {}

    int enableEventNotification(int32_t eventTypes);
    int disableEventNotification(int32_t eventTypes);
};

/*
  * Interface for providing buffers
  */
class BufferProvider
{
public:
    virtual void* allocateBuffer(int width, int height, const char* format, int &bytes, int numBufs) = 0;

    //additional methods used for memory mapping
    virtual uint32_t * getOffsets() = 0;
    virtual int getFd() = 0;

    virtual int freeBuffer(void* buf) = 0;

    virtual ~BufferProvider() {}
};

/**
  * Class for handling data and notify callbacks to application
  */
class   AppCallbackNotifier: public ErrorNotifier , public virtual RefBase
{

public:

    ///Constants
    static const int NOTIFIER_TIMEOUT;
    static const int32_t MAX_BUFFERS = 8;

    enum NotifierCommands
        {
        NOTIFIER_CMD_PROCESS_EVENT,
        NOTIFIER_CMD_PROCESS_FRAME,
        NOTIFIER_CMD_PROCESS_ERROR
        };

    enum NotifierState
        {
        NOTIFIER_STOPPED,
        NOTIFIER_STARTED,
        NOTIFIER_EXITED
        };

public:

    ~AppCallbackNotifier();

    ///Initialzes the callback notifier, creates any resources required
    status_t initialize();

    ///Starts the callbacks to application
    status_t start();

    ///Stops the callbacks from going to application
    status_t stop();

    void setEventProvider(int32_t eventMask, MessageNotifier * eventProvider);
    void setFrameProvider(FrameNotifier *frameProvider);

    //All sub-components of Camera HAL call this whenever any error happens
    virtual void errorNotify(int error);

    status_t startPreviewCallbacks(CameraParameters &params, void *buffers, uint32_t *offsets, int fd, size_t length, size_t count);
    status_t stopPreviewCallbacks();

    status_t enableMsgType(int32_t msgType);
    status_t disableMsgType(int32_t msgType);

    //API for enabling/disabling measurement data
    void setMeasurements(bool enable);

    //thread loops
    bool notificationThread();

    ///Notification callback functions
    static void frameCallbackRelay(CameraFrame* caFrame);
    static void eventCallbackRelay(CameraHalEvent* chEvt);
    void frameCallback(CameraFrame* caFrame);
    void eventCallback(CameraHalEvent* chEvt);
    void flushAndReturnFrames();

    void setCallbacks(CameraHal *cameraHal,
                        camera_notify_callback notify_cb,
                        camera_data_callback data_cb,
                        camera_data_timestamp_callback data_cb_timestamp,
                        camera_request_memory get_memory,
                        void *user);

    //Set Burst mode
    void setBurst(bool burst);

    int setParameters(const CameraParameters& params);

    //Notifications from CameraHal for video recording case
    status_t startRecording();
    status_t stopRecording();
    status_t initSharedVideoBuffers(void *buffers, uint32_t *offsets, int fd, size_t length, size_t count, void **vidBufs);
    status_t releaseRecordingFrame(const void *opaque);

    status_t useMetaDataBufferMode(bool enable);

    void EncoderDoneCb(void*, void*, CameraFrame::FrameType type, void* cookie1, void* cookie2);

    void useVideoBuffers(bool useVideoBuffers);

    bool getUesVideoBuffers();
    void setVideoRes(int width, int height);

    //Internal class definitions
    class NotificationThread : public Thread {
        AppCallbackNotifier* mAppCallbackNotifier;
        TIUTILS::MessageQueue mNotificationThreadQ;
    public:
        enum NotificationThreadCommands
        {
        NOTIFIER_START,
        NOTIFIER_STOP,
        NOTIFIER_EXIT,
        };
    public:
        NotificationThread(AppCallbackNotifier* nh)
            : Thread(false), mAppCallbackNotifier(nh) { }
        virtual bool threadLoop() {
            return mAppCallbackNotifier->notificationThread();
        }

        TIUTILS::MessageQueue &msgQ() { return mNotificationThreadQ;}
    };

    //Friend declarations
    friend class NotificationThread;

private:
    void notifyEvent();
    void notifyFrame();
    bool processMessage();
    void releaseSharedVideoBuffers();
    status_t dummyRaw();
    void copyAndSendPictureFrame(CameraFrame* frame, int32_t msgType);
    void copyAndSendPreviewFrame(CameraFrame* frame, int32_t msgType);

private:
    mutable Mutex mLock;
    mutable Mutex mBurstLock;
    CameraHal* mCameraHal;
    camera_notify_callback mNotifyCb;
    camera_data_callback   mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    camera_request_memory mRequestMemory;
    void *mCallbackCookie;

    //Keeps Video MemoryHeaps and Buffers within
    //these objects
    KeyedVector<unsigned int, unsigned int> mVideoHeaps;
    KeyedVector<unsigned int, unsigned int> mVideoBuffers;
    KeyedVector<unsigned int, unsigned int> mVideoMap;

    //Keeps list of Gralloc handles and associated Video Metadata Buffers
    KeyedVector<uint32_t, uint32_t> mVideoMetadataBufferMemoryMap;
    KeyedVector<uint32_t, uint32_t> mVideoMetadataBufferReverseMap;

    bool mBufferReleased;

    sp< NotificationThread> mNotificationThread;
    EventProvider *mEventProvider;
    FrameProvider *mFrameProvider;
    TIUTILS::MessageQueue mEventQ;
    TIUTILS::MessageQueue mFrameQ;
    NotifierState mNotifierState;

    bool mPreviewing;
    camera_memory_t* mPreviewMemory;
    unsigned char* mPreviewBufs[MAX_BUFFERS];
    int mPreviewBufCount;
    const char *mPreviewPixelFormat;
    KeyedVector<unsigned int, sp<MemoryHeapBase> > mSharedPreviewHeaps;
    KeyedVector<unsigned int, sp<MemoryBase> > mSharedPreviewBuffers;

    //Burst mode active
    bool mBurst;
    mutable Mutex mRecordingLock;
    bool mRecording;
    bool mMeasurementEnabled;

    bool mUseMetaDataBufferMode;
    bool mRawAvailable;

    CameraParameters mParameters;

    bool mUseVideoBuffers;

    int mVideoWidth;
    int mVideoHeight;
    KeyedVector<uint32_t, uint32_t> mPreviewBufIndex;
    KeyedVector<char*,uint32_t > mVirtualBuffer;

};


/**
  * Class used for allocating memory for JPEG bit stream buffers, output buffers of camera in no overlay case
  */
class MemoryManager : public BufferProvider, public virtual RefBase
{
public:
    MemoryManager():mIonFd(0){ }

    ///Initializes the memory manager creates any resources required
    status_t initialize() { return NO_ERROR; }

    int setErrorHandler(ErrorNotifier *errorNotifier);
    virtual void* allocateBuffer(int width, int height, const char* format, int &bytes, int numBufs);
    virtual uint32_t * getOffsets();
    virtual int getFd() ;
    virtual int freeBuffer(void* buf);

private:

    sp<ErrorNotifier> mErrorNotifier;
    int mIonFd;
    KeyedVector<unsigned int, unsigned int> mIonHandleMap;
    KeyedVector<unsigned int, unsigned int> mIonFdMap;
    KeyedVector<unsigned int, unsigned int> mIonBufLength;
};




/**
  * CameraAdapter interface class
  * Concrete classes derive from this class and provide implementations based on the specific camera h/w interface
  */

class CameraAdapter: public FrameNotifier, public virtual RefBase
{
protected:
    enum AdapterActiveStates {
        INTIALIZED_ACTIVE =     1 << 0,
        LOADED_PREVIEW_ACTIVE = 1 << 1,
        PREVIEW_ACTIVE =        1 << 2,
        LOADED_CAPTURE_ACTIVE = 1 << 3,
        CAPTURE_ACTIVE =        1 << 4,
        BRACKETING_ACTIVE =     1 << 5,
        AF_ACTIVE =             1 << 6,
        ZOOM_ACTIVE =           1 << 7,
        VIDEO_ACTIVE =          1 << 8,
    };
public:
    typedef struct
        {
         void *mBuffers;
         uint32_t *mOffsets;
         int mFd;
         size_t mLength;
         size_t mCount;
         size_t mMaxQueueable;
        } BuffersDescriptor;

    enum CameraCommands
        {
        CAMERA_START_PREVIEW                        = 0,
        CAMERA_STOP_PREVIEW                         = 1,
        CAMERA_START_VIDEO                          = 2,
        CAMERA_STOP_VIDEO                           = 3,
        CAMERA_START_IMAGE_CAPTURE                  = 4,
        CAMERA_STOP_IMAGE_CAPTURE                   = 5,
        CAMERA_PERFORM_AUTOFOCUS                    = 6,
        CAMERA_CANCEL_AUTOFOCUS                     = 7,
        CAMERA_PREVIEW_FLUSH_BUFFERS                = 8,
        CAMERA_START_SMOOTH_ZOOM                    = 9,
        CAMERA_STOP_SMOOTH_ZOOM                     = 10,
        CAMERA_USE_BUFFERS_PREVIEW                  = 11,
        CAMERA_SET_TIMEOUT                          = 12,
        CAMERA_CANCEL_TIMEOUT                       = 13,
        CAMERA_START_BRACKET_CAPTURE                = 14,
        CAMERA_STOP_BRACKET_CAPTURE                 = 15,
        CAMERA_QUERY_RESOLUTION_PREVIEW             = 16,
        CAMERA_QUERY_BUFFER_SIZE_IMAGE_CAPTURE      = 17,
        CAMERA_QUERY_BUFFER_SIZE_PREVIEW_DATA       = 18,
        CAMERA_USE_BUFFERS_IMAGE_CAPTURE            = 19,
        CAMERA_USE_BUFFERS_PREVIEW_DATA             = 20,
        CAMERA_TIMEOUT_EXPIRED                      = 21,
        CAMERA_START_FD                             = 22,
        CAMERA_STOP_FD                              = 23,
        CAMERA_SWITCH_TO_EXECUTING                  = 24,
        };

    enum CameraMode
        {
        CAMERA_PREVIEW,
        CAMERA_IMAGE_CAPTURE,
        CAMERA_VIDEO,
        CAMERA_MEASUREMENT
        };

    enum AdapterState {
        INTIALIZED_STATE           = INTIALIZED_ACTIVE,
        LOADED_PREVIEW_STATE       = LOADED_PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        PREVIEW_STATE              = PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        LOADED_CAPTURE_STATE       = LOADED_CAPTURE_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        CAPTURE_STATE              = CAPTURE_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        BRACKETING_STATE           = BRACKETING_ACTIVE | CAPTURE_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE ,
        AF_STATE                   = AF_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        ZOOM_STATE                 = ZOOM_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        VIDEO_STATE                = VIDEO_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        VIDEO_AF_STATE             = VIDEO_ACTIVE | AF_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        VIDEO_ZOOM_STATE           = VIDEO_ACTIVE | ZOOM_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        VIDEO_LOADED_CAPTURE_STATE = VIDEO_ACTIVE | LOADED_CAPTURE_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        VIDEO_CAPTURE_STATE        = VIDEO_ACTIVE | CAPTURE_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        AF_ZOOM_STATE              = AF_ACTIVE | ZOOM_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
        BRACKETING_ZOOM_STATE      = BRACKETING_ACTIVE | ZOOM_ACTIVE | PREVIEW_ACTIVE | INTIALIZED_ACTIVE,
    };

public:

    ///Initialzes the camera adapter creates any resources required
    virtual int initialize(int CameraHandle) = 0;

    virtual int setErrorHandler(ErrorNotifier *errorNotifier) = 0;

    //Message/Frame notification APIs
    virtual void enableMsgType(int32_t msgs,
                               frame_callback callback = NULL,
                               event_callback eventCb = NULL,
                               void *cookie = NULL) = 0;
    virtual void disableMsgType(int32_t msgs, void* cookie) = 0;
    virtual void returnFrame(void* frameBuf, CameraFrame::FrameType frameType) = 0;
    virtual void addFramePointers(void *frameBuf, void *buf) = 0;
    virtual void removeFramePointers() = 0;

    //APIs to configure Camera adapter and get the current parameter set
    virtual int setParameters(const CameraParameters& params) = 0;
    virtual void getParameters(CameraParameters& params) = 0;

    //API to flush the buffers from Camera
     status_t flushBuffers()
        {
        return sendCommand(CameraAdapter::CAMERA_PREVIEW_FLUSH_BUFFERS);
        }

    //Registers callback for returning image buffers back to CameraHAL
    virtual int registerImageReleaseCallback(release_image_buffers_callback callback, void *user_data) = 0;

    //Registers callback, which signals a completed image capture
    virtual int registerEndCaptureCallback(end_image_capture_callback callback, void *user_data) = 0;

    //API to send a command to the camera
    virtual status_t sendCommand(CameraCommands operation, int value1=0, int value2=0, int value3=0) = 0;
    virtual int queueToGralloc(int index, char* fp, int frameType) = 0;
    virtual char** getVirtualAddress(int count) = 0;
    virtual char * GetFrame(int &index) = 0;

    virtual ~CameraAdapter() {};

    //Retrieves the current Adapter state
    virtual AdapterState getState() = 0;

    //Retrieves the next Adapter state
    virtual AdapterState getNextState() = 0;

    // Receive orientation events from CameraHal
    virtual void onOrientationEvent(uint32_t orientation, uint32_t tilt) = 0;
protected:
    //The first two methods will try to switch the adapter state.
    //Every call to setState() should be followed by a corresponding
    //call to commitState(). If the state switch fails, then it will
    //get reset to the previous state via rollbackState().
    virtual status_t setState(CameraCommands operation) = 0;
    virtual status_t commitState() = 0;
    virtual status_t rollbackState() = 0;

    // Retrieves the current Adapter state - for internal use (not locked)
    virtual status_t getState(AdapterState &state) = 0;
    // Retrieves the next Adapter state - for internal use (not locked)
    virtual status_t getNextState(AdapterState &state) = 0;
};



class DisplayAdapter : public BufferProvider, public virtual RefBase
{
public:
    typedef struct S3DParameters_t
    {
        int mode;
        int framePacking;
        int order;
        int subSampling;
    } S3DParameters;

    ///Initializes the display adapter creates any resources required
    virtual int initialize() = 0;

    virtual int setPreviewWindow(struct preview_stream_ops *window) = 0;
    virtual int setFrameProvider(FrameNotifier *frameProvider) = 0;
    virtual int setErrorHandler(ErrorNotifier *errorNotifier) = 0;
    virtual int enableDisplay(int width, int height, struct timeval *refTime = NULL, S3DParameters *s3dParams = NULL) = 0;
    virtual int disableDisplay(bool cancel_buffer = true) = 0;
    //Used for Snapshot review temp. pause
    virtual int pauseDisplay(bool pause) = 0;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
    //Used for shot to snapshot measurement
    virtual int setSnapshotTimeRef(struct timeval *refTime = NULL) = 0;
#endif

    virtual int useBuffers(void *bufArr, int num) = 0;
    virtual bool supportsExternalBuffering() = 0;

    // Get max queueable buffers display supports
    // This function should only be called after
    // allocateBuffer
    virtual int maxQueueableBuffers(unsigned int& queueable) = 0;
};

static void releaseImageBuffers(void *userData);
static void endImageCapture(void *userData);

class CameraHal /*: public CameraHardwareInterface */  {
public:

    /** Set the notification and data callbacks */
    void setCallbacks(camera_notify_callback notify_cb,
                        camera_data_callback data_cb,
                        camera_data_timestamp_callback data_cb_timestamp,
                        camera_request_memory get_memory,
                        void *user);

    status_t storeMetaDataInBuffers(bool enable);
    void putParameters(char *parms);

    virtual sp<IMemoryHeap> getRawHeap() const;
    virtual void stopRecording();

#if JPEG
    void getCaptureSize(int *jpegSize);
    void copyJpegImage(void *imageBuf);
#endif

    virtual status_t startRecording();  // Eclair HAL
    virtual int recordingEnabled();
    virtual void releaseRecordingFrame(const void *opaque);
    virtual sp<IMemoryHeap> getPreviewHeap() const ;

    virtual status_t startPreview();   //  Eclair HAL
 //   virtual bool useOverlay() { return true; }
 //   virtual status_t setOverlay(const sp<Overlay> &overlay);
    virtual void stopPreview();
    virtual bool previewEnabled();

    virtual status_t autoFocus();  // Eclair HAL


    virtual status_t takePicture();        // Eclair HAL

    virtual status_t cancelPicture();    // Eclair HAL
    virtual status_t cancelAutoFocus();

    virtual status_t dump(int fd) const;
    void dumpFrame(void *buffer, int size, char *path);
    int setParameters(const char* params); //Kirti Added
    virtual status_t setParameters(const CameraParameters& params);
    virtual CameraParameters getParameters() const;
    virtual void release();
    void initDefaultParameters();

/*--------------------Eclair HAL---------------------------------------*/
    void        enableMsgType(int32_t msgType);
    void        disableMsgType(int32_t msgType);
    int         msgTypeEnabled(int32_t msgType);
/*--------------------Eclair HAL---------------------------------------*/

    virtual status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);

    class PreviewThread : public Thread {
        CameraHal* mHardware;
    public:
        PreviewThread(CameraHal* hw)
            : Thread(false), mHardware(hw) { }

        virtual bool threadLoop() {
            mHardware->previewThread();

            return false;
        }
    };

    class SnapshotThread : public Thread {
        CameraHal* mHardware;
    public:
        SnapshotThread(CameraHal* hw)
            : Thread(false), mHardware(hw) { }

        virtual bool threadLoop() {
            mHardware->snapshotThread();
            return false;
        }
    };

    class PROCThread : public Thread {
        CameraHal* mHardware;
    public:
        PROCThread(CameraHal* hw)
            : Thread(false), mHardware(hw) { }

        virtual bool threadLoop() {
            mHardware->procThread();
            return false;
        }
    };

    class ShutterThread : public Thread {
        CameraHal* mHardware;
    public:
        ShutterThread(CameraHal* hw)
            : Thread(false), mHardware(hw) { }

        virtual bool threadLoop() {
            mHardware->shutterThread();
            return false;
        }
    };

    class RawThread : public Thread {
        CameraHal* mHardware;
    public:
        RawThread(CameraHal* hw)
            : Thread(false), mHardware(hw) { }

        virtual bool threadLoop() {
            mHardware->rawThread();
            return false;
        }
    };

   CameraHal(int cameraId);
    virtual ~CameraHal();

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    //Uses the constructor timestamp as a reference to calcluate the
    // elapsed time
    static void PPM(const char *);
    //Uses a user provided timestamp as a reference to calcluate the
    // elapsed time
    static void PPM(const char *, struct timeval*, ...);

#endif

    /** Initialize CameraHal */
    status_t initialize(CameraProperties::Properties*);

    void previewThread();
    bool validateSize(size_t width, size_t height, const supported_resolution *supRes, size_t count);
    bool validateRange(int min, int max, const char *supRang);
    void procThread();
    void shutterThread();
    void rawThread();
    void snapshotThread();
    void *getLastOverlayAddress();
    size_t getLastOverlayLength();

   int setPreviewWindow(struct preview_stream_ops *window);
   bool mSetPreviewWindowCalled;

#ifdef IMAGE_PROCESSING_PIPELINE

    IPP_PARAMS mIPPParams;

#endif

#ifdef FW3A
#ifdef IMAGE_PROCESSING_PIPELINE
    static void onIPP(void *priv, icap_ipp_parameters_t *ipp_params);
#endif
    static void onMakernote(void *priv, void *mknote_ptr);
    static void onShutter(void *priv, icap_image_buffer_t *image_buf);
    static void onSaveH3A(void *priv, icap_image_buffer_t *buf);
    static void onSaveLSC(void *priv, icap_image_buffer_t *buf);
    static void onSaveRAW(void *priv, icap_image_buffer_t *buf);
    static void onSnapshot(void *priv, icap_image_buffer_t *buf);
    static void onGeneratedSnapshot(void *priv, icap_image_buffer_t *buf);
    static void onCrop(void *priv,  icap_crop_rect_t *crop_rect);

    int FW3A_Create();
    int FW3A_Init();
    int FW3A_Release();
    int FW3A_Destroy();
    int FW3A_Start();
    int FW3A_Stop();
    int FW3A_Start_CAF();
    int FW3A_Stop_CAF();
    int FW3A_Start_AF();
    int FW3A_Stop_AF();
    int FW3A_GetSettings() const;
    int FW3A_SetSettings();

#endif

    int CorrectPreview();
    int ZoomPerform(float zoom);
    void nextPreview();
    void queueToOverlay(int index);
    int dequeueFromOverlay();
    int dequeueFromCamera(nsecs_t *timestamp);
    char * GetFrame(int &index);
    status_t UseBuffersPreview();
    int ICapturePerform();
    int ICaptureCreate(void);
    int ICaptureDestroy(void);
    void SetDSPKHz(unsigned int KHz);

    status_t convertGPSCoord(double coord, int *deg, int *min, int *sec);

#ifdef IMAGE_PROCESSING_PIPELINE

    int DeInitIPP(int ippMode);
    int InitIPP(int w, int h, int fmt, int ippMode);
    int PopulateArgsIPP(int w, int h, int fmt, int ippMode);
    int ProcessBufferIPP(void *pBuffer, long int nAllocLen, int fmt, int ippMode,
                       int EdgeEnhancementStrength, int WeakEdgeThreshold, int StrongEdgeThreshold,
                        int LowFreqLumaNoiseFilterStrength, int MidFreqLumaNoiseFilterStrength, int HighFreqLumaNoiseFilterStrength,
                        int LowFreqCbNoiseFilterStrength, int MidFreqCbNoiseFilterStrength, int HighFreqCbNoiseFilterStrength,
                        int LowFreqCrNoiseFilterStrength, int MidFreqCrNoiseFilterStrength, int HighFreqCrNoiseFilterStrength,
                        int shadingVertParam1, int shadingVertParam2, int shadingHorzParam1, int shadingHorzParam2,
                        int shadingGainScale, int shadingGainOffset, int shadingGainMaxValue,
                        int ratioDownsampleCbCr);
    OMX_IPP pIPP;

#endif   

    int CameraCreate();

    int CameraDestroy(bool destroyWindow);
    int CameraConfigure();
    int CameraSetFrameRate();
    int CameraStart();
    int CameraStop();

    /** Allocate preview data buffers */
    status_t allocPreviewBufs(int width, int height, const char* previewFormat,
                                        unsigned int buffercount, unsigned int &max_queueable);

    /** Free preview data buffers */
    status_t freePreviewBufs();
    void forceStopPreview();

    //Signals the end of image capture
    status_t signalEndImageCapture();

    int allocatePictureBuffers(size_t length, int burstCount);
    int freePictureBuffers(void);

    int SaveFile(char *filename, char *ext, void *buffer, int jpeg_size);
  
    int isStart_FW3A;
    int isStart_FW3A_AF;
    int isStart_FW3A_CAF;
    int isStart_FW3A_AEWB;
    int isStart_VPP;
    int isStart_JPEG;
    int FW3A_AF_TimeOut;

    mutable Mutex mLock;
    int mBurstShots;
    struct v4l2_crop mInitialCrop;

#ifdef HARDWARE_OMX

    gps_data *gpsLocation;
    exif_params mExifParams;

#endif

    bool mShutterEnable;
    bool mCAFafterPreview;
    bool useMaxCrop;
    float zoomAspRatio;
    CameraParameters mParameters;
    sp<MemoryHeapBase> mPictureHeap;
    int mJPEGOffset, mJPEGLength;
    unsigned int mYuvBufferLen[MAX_BURST];
    void *mYuvBuffer[MAX_BURST];
    bool mPreviewBuffersAllocated;
    CameraProperties::Properties* mCameraProperties;
    uint32_t *mPreviewOffsets;
    int mPreviewFd;
    int mPreviewLength;
    int32_t *mPreviewBufs;
    int mPreviewBufCount;
	sp<MemoryBase> mPictureBuffer;

    BufferProvider *mBufProvider;
    sp<DisplayAdapter> mDisplayAdapter; 
    CameraAdapter *mCameraAdapter;
    sp<AppCallbackNotifier> mAppCallbackNotifier;
    sp<PreviewThread>  mPreviewThread;
    sp<PROCThread>  mPROCThread;
    sp<ShutterThread> mShutterThread;
    sp<RawThread> mRawThread;
    sp<SnapshotThread> mSnapshotThread;
	int mTotalJpegsize;
	void* mJpegBuffAddr;
	//sp<MemoryBase> mJPEGPictureMemBase;
	sp<MemoryHeapBase> mJPEGPictureHeap;

    bool mPreviewRunning;
    bool mDisplayPaused;
    bool mIPPInitAlgoState;
    bool mIPPToEnable;
    Mutex mRecordingLock;
    int mRecordingFrameSize;
    // Video Frame Begin
    int mVideoBufferCount;
    sp<MemoryHeapBase> mVideoHeaps[VIDEO_FRAME_COUNT_MAX];
    sp<MemoryBase> mVideoBuffer[VIDEO_FRAME_COUNT_MAX];

#define BUFF_IDLE       (0)
#define BUFF_Q2DSS      (1)
#define BUFF_Q2VE       (1<<1)
    int                 mVideoBufferStatus[MAX_CAMERA_BUFFERS];
#ifdef DEBUG_LOG
    void debugShowBufferStatus();
#else
#define debugShowBufferStatus()
#endif

    //Index of current camera adapter
    int mCameraIndex;
    // ...
    int nOverlayBuffersQueued;
    int nCameraBuffersQueued;
    struct v4l2_buffer v4l2_cam_buffer[MAX_CAMERA_BUFFERS];
    sp<MemoryHeapBase> mPreviewHeaps[MAX_CAMERA_BUFFERS];
    sp<MemoryBase> mPreviewBuffers[MAX_CAMERA_BUFFERS];
    int mfirstTime;
  //  static wp<CameraHardwareInterface> singleton[MAX_CAMERAS_SUPPORTED];
    static int camera_device;
    static const supported_resolution supportedPreviewRes[];
    static const supported_resolution supportedPictureRes[];
    static const char supportedPictureSizes[];
    static const char supportedPreviewSizes[];
    static const char supportedFPS[];
    static const char supportedFpsRanges[];
    static const char supportedThumbnailSizes[];
    char focusDistances[FOCUS_DISTANCE_BUFFER_SIZE];
    static const char PARAMS_DELIMITER[];
    int procPipe[2], shutterPipe[2], rawPipe[2], snapshotPipe[2], snapshotReadyPipe[2];
    int mippMode;
    int pictureNumber;
    bool mCaptureRunning;
    int rotation;
#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
    struct timeval ppm;
    static struct timeval ppm_start;
    struct timeval ppm_receiveCmdToTakePicture;
    struct timeval ppm_restartPreview;
    struct timeval focus_before, focus_after;
    struct timeval ppm_before, ppm_after;
    struct timeval ipp_before, ipp_after;

    //Timestamp of the autoFocus command
    static struct timeval mStartFocus;
    //Timestamp of the startPreview command
    static struct timeval mStartPreview;
    //Timestamp of the takePicture command
    static struct timeval mStartCapture;
#endif

    int lastOverlayBufferDQ;

/*--------------Eclair Camera HAL---------------------*/

    camera_notify_callback mNotifyCb;
    camera_data_callback   mDataCb;
    camera_data_timestamp_callback mDataCbTimestamp;
    void                 *mCallbackCookie;

    int32_t             mMsgEnabled;
    bool                mRecordingEnabled;
    bool mFalsePreview;


/*--------------Eclair Camera HAL---------------------*/

#ifdef HARDWARE_OMX
   JpegEncoder*    jpegEncoder;
#endif

#ifdef FW3A
      lib3atest_obj *fobj;
#endif

#ifdef ICAP
    libtest_obj   *iobj;
#endif

    int file_index;
    int cmd;
    int quality;
    unsigned int sensor_width;
    unsigned int sensor_height;
    unsigned int zoom_width;
    unsigned int zoom_height;
    int32_t mImageCropTop, mImageCropLeft, mImageCropWidth, mImageCropHeight;
    int mflash;
    int mred_eye;
    int mcapture_mode;
    int mZoomCurrentIdx, mZoomTargetIdx, mZoomSpeed;
    int mcaf;
    int j;
    bool useFramerateRange;

    enum SmoothZoomStatus {
        SMOOTH_START = 0,
        SMOOTH_NOTIFY_AND_STOP,
        SMOOTH_STOP
    } mSmoothZoomStatus;

    enum PreviewThreadCommands {

        // Comands        
        PREVIEW_START,
        PREVIEW_STOP,
        PREVIEW_AF_START,
        PREVIEW_AF_STOP,
        PREVIEW_CAPTURE,
        PREVIEW_CAPTURE_CANCEL,
        PREVIEW_KILL,
        PREVIEW_CAF_START,
        PREVIEW_CAF_STOP,
        PREVIEW_FPS,
        START_SMOOTH_ZOOM,
        STOP_SMOOTH_ZOOM,
        // ACKs        
        PREVIEW_ACK,
        PREVIEW_NACK,

    };

    enum ProcessingThreadCommands {

        // Comands        
        PROCESSING_PROCESS,
        PROCESSING_CANCEL,
        PROCESSING_KILL,

        // ACKs        
        PROCESSING_ACK,
        PROCESSING_NACK,
    };    

    TIUTILS::MessageQueue    previewThreadCommandQ;
    TIUTILS::MessageQueue    previewThreadAckQ;    
    TIUTILS::MessageQueue    processingThreadCommandQ;
    TIUTILS::MessageQueue    processingThreadAckQ;

    mutable Mutex takephoto_lock;
    uint8_t *yuv_buffer, *jpeg_buffer, *vpp_buffer, *ancillary_buffer;
    
    int yuv_len, jpeg_len, ancillary_len;

    FILE *foutYUV;
    FILE *foutJPEG;
    bool mCaptureMode;
};

}; // namespace android

extern "C" {

    int aspect_ratio_calc(
        unsigned int sens_width,  unsigned int sens_height,
        unsigned int pix_width,   unsigned int pix_height,
        unsigned int src_width,   unsigned int src_height,
        unsigned int dst_width,   unsigned int dst_height,
        unsigned int align_crop_width, unsigned int align_crop_height,
        unsigned int align_pos_width,  unsigned int align_pos_height,
        int *crop_src_left,  int *crop_src_top,
        int *crop_src_width, int *crop_src_height,
        int *pos_dst_left,   int *pos_dst_top,
        int *pos_dst_width,  int *pos_dst_height,
        unsigned int flags);

    int scale_init(int inWidth, int inHeight, int outWidth, int outHeight, int inFmt, int outFmt);
    int scale_deinit();
    int scale_process(void* inBuffer, int inWidth, int inHeight, void* outBuffer, int outWidth, int outHeight, int rotation, int fmt, float zoom, int crop_top, int crop_left, int crop_width, int crop_height);
}

#endif

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


#ifndef OMX_CAMERA_ADAPTER_H
#define OMX_CAMERA_ADAPTER_H

#include "CameraHal.h"
#include "OMX_Types.h"
#include "OMX_Core.h"
#include "OMX_CoreExt.h"
#include "OMX_IVCommon.h"
#include "OMX_Component.h"
#include "OMX_Index.h"
#include "OMX_IndexExt.h"
#include "OMX_TI_Index.h"
#include "OMX_TI_IVCommon.h"
#include "OMX_TI_Common.h"
#include "OMX_TI_Image.h"
#include "General3A_Settings.h"

#include "BaseCameraAdapter.h"
#include "DebugUtils.h"


extern "C"
{
#include "timm_osal_error.h"
#include "timm_osal_events.h"
#include "timm_osal_trace.h"
#include "timm_osal_semaphores.h"
}

namespace android {

#define FOCUS_THRESHOLD             5 //[s.]

#define MIN_JPEG_QUALITY            1
#define MAX_JPEG_QUALITY            100
#define EXP_BRACKET_RANGE           10

#define FOCUS_DIST_SIZE             100
#define FOCUS_DIST_BUFFER_SIZE      500

#define TOUCH_DATA_SIZE             200
#define DEFAULT_THUMB_WIDTH         160
#define DEFAULT_THUMB_HEIGHT        120
#define FRAME_RATE_FULL_HD          27
#define ZOOM_STAGES                 61

#define FACE_DETECTION_BUFFER_SIZE  0x1000

#define EXIF_MODEL_SIZE             100
#define EXIF_MAKE_SIZE              100
#define EXIF_DATE_TIME_SIZE         20

#define GPS_TIMESTAMP_SIZE          6
#define GPS_DATESTAMP_SIZE          11
#define GPS_REF_SIZE                2
#define GPS_MAPDATUM_SIZE           100
#define GPS_PROCESSING_SIZE         100
#define GPS_VERSION_SIZE            4
#define GPS_NORTH_REF               "N"
#define GPS_SOUTH_REF               "S"
#define GPS_EAST_REF                "E"
#define GPS_WEST_REF                "W"

/* Default portstartnumber of Camera component */
#define OMX_CAMERA_DEFAULT_START_PORT_NUM 0

/* Define number of ports for differt domains */
#define OMX_CAMERA_PORT_OTHER_NUM 1
#define OMX_CAMERA_PORT_VIDEO_NUM 4
#define OMX_CAMERA_PORT_IMAGE_NUM 1
#define OMX_CAMERA_PORT_AUDIO_NUM 0
#define OMX_CAMERA_NUM_PORTS (OMX_CAMERA_PORT_OTHER_NUM + OMX_CAMERA_PORT_VIDEO_NUM + OMX_CAMERA_PORT_IMAGE_NUM + OMX_CAMERA_PORT_AUDIO_NUM)

/* Define start port number for differt domains */
#define OMX_CAMERA_PORT_OTHER_START OMX_CAMERA_DEFAULT_START_PORT_NUM
#define OMX_CAMERA_PORT_VIDEO_START (OMX_CAMERA_PORT_OTHER_START + OMX_CAMERA_PORT_OTHER_NUM)
#define OMX_CAMERA_PORT_IMAGE_START (OMX_CAMERA_PORT_VIDEO_START + OMX_CAMERA_PORT_VIDEO_NUM)
#define OMX_CAMERA_PORT_AUDIO_START (OMX_CAMERA_PORT_IMAGE_START + OMX_CAMERA_PORT_IMAGE_NUM)

/* Port index for camera component */
#define OMX_CAMERA_PORT_OTHER_IN (OMX_CAMERA_PORT_OTHER_START + 0)
#define OMX_CAMERA_PORT_VIDEO_IN_VIDEO (OMX_CAMERA_PORT_VIDEO_START + 0)
#define OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW (OMX_CAMERA_PORT_VIDEO_START + 1)
#define OMX_CAMERA_PORT_VIDEO_OUT_VIDEO (OMX_CAMERA_PORT_VIDEO_START + 2)
#define OMX_CAMERA_PORT_VIDEO_OUT_MEASUREMENT (OMX_CAMERA_PORT_VIDEO_START + 3)
#define OMX_CAMERA_PORT_IMAGE_OUT_IMAGE (OMX_CAMERA_PORT_IMAGE_START + 0)


#define OMX_INIT_STRUCT(_s_, _name_)	\
    memset(&(_s_), 0x0, sizeof(_name_));	\
    (_s_).nSize = sizeof(_name_);		\
    (_s_).nVersion.s.nVersionMajor = 0x1;	\
    (_s_).nVersion.s.nVersionMinor = 0x1;	\
    (_s_).nVersion.s.nRevision = 0x0;		\
    (_s_).nVersion.s.nStep = 0x0

#define OMX_INIT_STRUCT_PTR(_s_, _name_)   \
    memset((_s_), 0x0, sizeof(_name_));         \
    (_s_)->nSize = sizeof(_name_);              \
    (_s_)->nVersion.s.nVersionMajor = 0x1;      \
    (_s_)->nVersion.s.nVersionMinor = 0x1;      \
    (_s_)->nVersion.s.nRevision = 0x0;          \
    (_s_)->nVersion.s.nStep = 0x0

#define GOTO_EXIT_IF(_CONDITION,_ERROR) {                                       \
    if ((_CONDITION)) {                                                         \
        eError = (_ERROR);                                                      \
        goto EXIT;                                                              \
    }                                                                           \
}

///OMX Specific Functions
static OMX_ERRORTYPE OMXCameraAdapterEventHandler(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_PTR pAppData,
                                        OMX_IN OMX_EVENTTYPE eEvent,
                                        OMX_IN OMX_U32 nData1,
                                        OMX_IN OMX_U32 nData2,
                                        OMX_IN OMX_PTR pEventData);

static OMX_ERRORTYPE OMXCameraAdapterEmptyBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_PTR pAppData,
                                        OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE OMXCameraAdapterFillBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_PTR pAppData,
                                        OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader);

struct CapResolution {
    size_t width, height;
    const char *param;
};

struct CapPixelformat {
    OMX_COLOR_FORMATTYPE pixelformat;
    const char *param;
};

struct CapFramerate {
    OMX_U32 framerate;
    const char *param;
};

struct CapZoom {
    OMX_S32 zoomStage;
    const char *param;
};

struct CapEVComp {
    OMX_S32 evComp;
    const char *param;
};

struct CapISO {
    OMX_U32 iso;
    const char *param;
};

/**
  * Class which completely abstracts the camera hardware interaction from camera hal
  * TODO: Need to list down here, all the message types that will be supported by this class
                Need to implement BufferProvider interface to use AllocateBuffer of OMX if needed
  */
class OMXCameraAdapter : public BaseCameraAdapter
{
public:

    /*--------------------Constant declarations----------------------------------------*/
    static const int32_t MAX_NO_BUFFERS = 20;

    ///@remarks OMX Camera has six ports - buffer input, time input, preview, image, video, and meta data
    static const int MAX_NO_PORTS = 6;

    ///Five second timeout
    static const int CAMERA_ADAPTER_TIMEOUT = 5000*1000;

    //EXIF ASCII prefix
    static const char EXIFASCIIPrefix[];

    enum OMXCameraEvents
        {
        CAMERA_PORT_ENABLE  = 0x1,
        CAMERA_PORT_FLUSH   = 0x2,
        CAMERA_PORT_DISABLE = 0x4,
        };

    enum CaptureMode
        {
        HIGH_SPEED = 1,
        HIGH_QUALITY = 2,
        VIDEO_MODE = 3,
        };

    enum IPPMode
        {
        IPP_NULL = -1,
        IPP_NONE = 0,
        IPP_NSF,
        IPP_LDC,
        IPP_LDCNSF,
        };

    enum S3DFrameLayout
        {
        S3D_NONE = 0,
        S3D_TB_FULL,
        S3D_SS_FULL,
        S3D_TB_SUBSAMPLED,
        S3D_SS_SUBSAMPLED,
        };

    enum CodingMode
        {
        CodingNone = 0,
        CodingJPS,
        CodingMPO,
        CodingRAWJPEG,
        CodingRAWMPO,
        };

    enum Algorithm3A
        {
        WHITE_BALANCE_ALGO = 0x1,
        EXPOSURE_ALGO = 0x2,
        FOCUS_ALGO = 0x4,
        };

    enum AlgoPriority
        {
        FACE_PRIORITY = 0,
        REGION_PRIORITY,
        };

    enum BrightnessMode
        {
        BRIGHTNESS_OFF = 0,
        BRIGHTNESS_ON,
        BRIGHTNESS_AUTO,
        };

    class GPSData
    {
        public:
                int mLongDeg, mLongMin, mLongSec;
                char mLongRef[GPS_REF_SIZE];
                bool mLongValid;
                int mLatDeg, mLatMin, mLatSec;
                char mLatRef[GPS_REF_SIZE];
                bool mLatValid;
                int mAltitude;
                char mAltitudeRef[GPS_REF_SIZE];
                bool mAltitudeValid;
                char mMapDatum[GPS_MAPDATUM_SIZE];
                bool mMapDatumValid;
                char mVersionId[GPS_VERSION_SIZE];
                bool mVersionIdValid;
                char mProcMethod[GPS_PROCESSING_SIZE];
                bool mProcMethodValid;
                char mDatestamp[GPS_DATESTAMP_SIZE];
                bool mDatestampValid;
                uint32_t mTimeStampHour;
                uint32_t mTimeStampMin;
                uint32_t mTimeStampSec;
                bool mTimeStampValid;
    };

    class EXIFData
    {
        public:
            GPSData mGPSData;
            bool mMakeValid;
            bool mModelValid;
    };

    ///Parameters specific to any port of the OMX Camera component
    class OMXCameraPortParameters
    {
        public:
            OMX_U32                         mHostBufaddr[MAX_NO_BUFFERS];
            OMX_BUFFERHEADERTYPE           *mBufferHeader[MAX_NO_BUFFERS];
            OMX_U32                         mWidth;
            OMX_U32                         mHeight;
            OMX_U32                         mStride;
            OMX_U8                          mNumBufs;
            OMX_U32                         mBufSize;
            OMX_COLOR_FORMATTYPE            mColorFormat;
            OMX_PARAM_VIDEONOISEFILTERTYPE  mVNFMode;
            OMX_PARAM_VIDEOYUVRANGETYPE     mYUVRange;
            OMX_CONFIG_BOOLEANTYPE          mVidStabParam;
            OMX_CONFIG_FRAMESTABTYPE        mVidStabConfig;
            OMX_U32                         mCapFrame;
            OMX_U32                         mFrameRate;
            OMX_S32                         mMinFrameRate;
            OMX_S32                         mMaxFrameRate;
            CameraFrame::FrameType mImageType;
    };

    ///Context of the OMX Camera component
    class OMXCameraAdapterComponentContext
    {
        public:
            OMX_HANDLETYPE              mHandleComp;
            OMX_U32                     mNumPorts;
            OMX_STATETYPE               mState ;
            OMX_U32                     mVideoPortIndex;
            OMX_U32                     mPrevPortIndex;
            OMX_U32                     mImagePortIndex;
            OMX_U32                     mMeasurementPortIndex;
            OMXCameraPortParameters     mCameraPortParams[MAX_NO_PORTS];
    };

    // MEASUREMENT HEADER SMALC
    struct OMXDebugDataHeader
    {
        OMX_U32 mRecordID;
        OMX_U32 mSectionID;
        OMX_U32 mTimeStamp;
        OMX_U32 mRecordSize;
        OMX_U32 mPayloadSize;
        OMX_U32 mHeaderSize;
    };

public:

    OMXCameraAdapter();
    ~OMXCameraAdapter();

    ///Initialzes the camera adapter creates any resources required
    virtual status_t initialize(int sensor_index=0);

    //APIs to configure Camera adapter and get the current parameter set
    virtual status_t setParameters(const CameraParameters& params);
    virtual void getParameters(CameraParameters& params);

    //API to get the caps
    virtual status_t getCaps(CameraParameters &params);

    //Used together with capabilities
    virtual int getRevision();


    // API
    virtual status_t UseBuffersPreview(void* bufArr, int num);

    //API to flush the buffers for preview
    status_t flushBuffers();

    // API
    virtual status_t setFormat(OMX_U32 port, OMXCameraPortParameters &cap);

    //API to get the frame size required to be allocated. This size is used to override the size passed
    //by camera service when VSTAB/VNF is turned ON for example
    virtual void getFrameSize(int &width, int &height);

    virtual status_t getPictureBufferSize(size_t &length, size_t bufferCount);

    virtual status_t getFrameDataSize(size_t &dataFrameSize, size_t bufferCount);

 OMX_ERRORTYPE OMXCameraAdapterEventHandler(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_EVENTTYPE eEvent,
                                    OMX_IN OMX_U32 nData1,
                                    OMX_IN OMX_U32 nData2,
                                    OMX_IN OMX_PTR pEventData);

 OMX_ERRORTYPE OMXCameraAdapterEmptyBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);

 OMX_ERRORTYPE OMXCameraAdapterFillBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                    OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader);
protected:

    //Parent class method implementation
    virtual status_t takePicture();
    virtual status_t stopImageCapture();
    virtual status_t startBracketing(int range);
    virtual status_t stopBracketing();
    virtual status_t autoFocus();
    virtual status_t cancelAutoFocus();
    virtual status_t setTimeOut(int sec);
    virtual status_t cancelTimeOut();
    virtual status_t startSmoothZoom(int targetIdx);
    virtual status_t stopSmoothZoom();
    virtual status_t startVideoCapture();
    virtual status_t stopVideoCapture();
    virtual status_t startPreview();
    virtual status_t stopPreview();
    virtual status_t useBuffers(CameraMode mode, void* bufArr, int num, size_t length);
    virtual status_t fillThisBuffer(void* frameBuf, CameraFrame::FrameType frameType);

private:

    FILE * parseDCCsubDir(DIR *pDir, char *path);
    FILE * fopenCameraDCC(const char *dccFolderPath);
    int fseekDCCuseCasePos(FILE *pFile);
    status_t switchToLoaded();

    OMXCameraPortParameters *getPortParams(CameraFrame::FrameType frameType);

    OMX_ERRORTYPE SignalEvent(OMX_IN OMX_HANDLETYPE hComponent,
                                                  OMX_IN OMX_EVENTTYPE eEvent,
                                                  OMX_IN OMX_U32 nData1,
                                                  OMX_IN OMX_U32 nData2,
                                                  OMX_IN OMX_PTR pEventData);

    status_t RegisterForEvent(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_EVENTTYPE eEvent,
                                          OMX_IN OMX_U32 nData1,
                                          OMX_IN OMX_U32 nData2,
                                          OMX_IN Semaphore &semaphore,
                                          OMX_IN OMX_U32 timeout);

    status_t setPictureRotation(unsigned int degree);
    status_t setSensorOrientation(unsigned int degree);
    status_t setImageQuality(unsigned int quality);
    status_t setThumbnailParams(unsigned int width, unsigned int height, unsigned int quality);

    //Geo-tagging
    status_t convertGPSCoord(double coord, int *deg, int *min, int *sec);
    status_t setupEXIF();

    //Focus functionality
    status_t doAutoFocus();
    status_t stopAutoFocus();
    status_t checkFocus(OMX_PARAM_FOCUSSTATUSTYPE *eFocusStatus);
    status_t returnFocusStatus(bool timeoutReached);

    //Focus distances
    status_t addFocusDistances(OMX_U32 &near,
                               OMX_U32 &optimal,
                               OMX_U32 &far,
                               CameraParameters& params);
    status_t encodeFocusDistance(OMX_U32 dist, char *buffer, size_t length);
    status_t getFocusDistances(OMX_U32 &near,OMX_U32 &optimal, OMX_U32 &far);

    //VSTAB and VNF Functionality
    status_t enableVideoNoiseFilter(bool enable);
    status_t enableVideoStabilization(bool enable);

    //Digital zoom
    status_t doZoom(int index);
    status_t advanceZoom();

    //Scenes
    OMX_ERRORTYPE setScene(Gen3A_settings& Gen3A);

    //Flash modes
    OMX_ERRORTYPE setFlashMode(Gen3A_settings& Gen3A);

    //Exposure Modes
    OMX_ERRORTYPE setExposureMode(Gen3A_settings& Gen3A);

    //API to set FrameRate using VFR interface
    status_t setVFramerate(OMX_U32 minFrameRate,OMX_U32 maxFrameRate);

    //Manual Exposure
    status_t setManualExposure(Gen3A_settings& Gen3A);

    //Manual Gain
    status_t setManualGain(Gen3A_settings& Gen3A);

    //Noise filtering
    status_t setNSF(OMXCameraAdapter::IPPMode mode);

    //Stereo 3D
    status_t setS3DFrameLayout(OMX_U32 port);

    //LDC
    status_t setLDC(OMXCameraAdapter::IPPMode mode);

    //GLBCE
    status_t setGLBCE(OMXCameraAdapter::BrightnessMode mode);

    //GBCE
    status_t setGBCE(OMXCameraAdapter::BrightnessMode mode);

    status_t printComponentVersion(OMX_HANDLETYPE handle);

    //Touch AF
    status_t parseTouchPosition(const char *pos, unsigned int &posX, unsigned int &posY);
    status_t setTouchFocus(unsigned int posX, unsigned int posY, size_t width, size_t height);

    //Face detection
    status_t updateFocusDistances(CameraParameters &params);
    status_t setFaceDetection(bool enable);
    status_t detectFaces(OMX_BUFFERHEADERTYPE* pBuffHeader);
    status_t encodeFaceCoordinates(const OMX_FACEDETECTIONTYPE *faceData, char *faceString, size_t faceStringSize);

    //3A Algorithms priority configuration
    status_t setAlgoPriority(AlgoPriority priority, Algorithm3A algo, bool enable);

    //Sensor overclocking
    status_t setSensorOverclock(bool enable);

    //OMX capabilities query
    status_t getOMXCaps(OMX_TI_CAPTYPE *caps);

    //Utility methods for OMX Capabilities
    status_t insertCapabilities(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t encodeSizeCap(OMX_TI_CAPRESTYPE &res, const CapResolution *cap, size_t capCount, char * buffer, size_t bufferSize);
    status_t encodeISOCap(OMX_U32 maxISO, const CapISO *cap, size_t capCount, char * buffer, size_t bufferSize);
    size_t encodeZoomCap(OMX_S32 maxZoom, const CapZoom *cap, size_t capCount, char * buffer, size_t bufferSize);
    status_t encodeFramerateCap(OMX_U32 framerateMax, OMX_U32 framerateMin, const CapFramerate *cap, size_t capCount, char * buffer, size_t bufferSize);
    status_t encodeVFramerateCap(OMX_TI_CAPTYPE &caps, char * buffer, size_t bufferSize);
    status_t encodePixelformatCap(OMX_COLOR_FORMATTYPE format, const CapPixelformat *cap, size_t capCount, char * buffer, size_t bufferSize);
    status_t insertImageSizes(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertPreviewSizes(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertThumbSizes(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertZoomStages(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertImageFormats(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertPreviewFormats(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertFramerates(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertVFramerates(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertEVs(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertISOModes(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertIPPModes(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertWBModes(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertEffects(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertExpModes(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertSceneModes(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertFocusModes(CameraParameters &params, OMX_TI_CAPTYPE &caps);
    status_t insertFlickerModes(CameraParameters &params, OMX_TI_CAPTYPE &caps);

    //Exposure Bracketing
    status_t setExposureBracketing(int *evValues, size_t evCount, size_t frameCount);
    status_t parseExpRange(const char *rangeStr, int * expRange, size_t count, size_t &validEntries);

    //Temporal Bracketing
    status_t doBracketing(OMX_BUFFERHEADERTYPE *pBuffHeader, CameraFrame::FrameType typeOfFrame);
    status_t sendBracketFrames();

    // Image Capture Service
    status_t startImageCapture();

    //Shutter callback notifications
    status_t setShutterCallback(bool enabled);

    //Sets eithter HQ or HS mode and the frame count
    status_t setCaptureMode(OMXCameraAdapter::CaptureMode mode);
    status_t UseBuffersCapture(void* bufArr, int num);
    status_t UseBuffersPreviewData(void* bufArr, int num);

    //Used for calculation of the average frame rate during preview
    status_t recalculateFPS();

    //Helper method for initializing a CameFrame object
    status_t initCameraFrame(CameraFrame &frame, OMX_IN OMX_BUFFERHEADERTYPE *pBuffHeader, int typeOfFrame, OMXCameraPortParameters *port);

    //Prepare the frame to be sent to subscribers
    status_t prepareFrame(OMX_IN OMX_BUFFERHEADERTYPE *pBuffHeader,
                                      int typeOfFrame,
                                      OMXCameraPortParameters *port,
                                      CameraFrame &frame);


    //Sends the incoming OMX buffer header to subscribers
    status_t sendFrame(CameraFrame &frame);

    //Sends empty raw frames in case there aren't any during image capture
    status_t sendEmptyRawFrame();

    const char* getLUTvalue_OMXtoHAL(int OMXValue, LUTtype LUT);
    int getLUTvalue_HALtoOMX(const char * HalValue, LUTtype LUT);
    OMX_ERRORTYPE apply3Asettings( Gen3A_settings& Gen3A );

    // AutoConvergence
    status_t setAutoConvergence(OMX_TI_AUTOCONVERGENCEMODETYPE pACMode, OMX_S32 pManualConverence);
    status_t getAutoConvergence(OMX_TI_AUTOCONVERGENCEMODETYPE *pACMode, OMX_S32 *pManualConverence);

    class CommandHandler : public Thread {
        public:
            CommandHandler(OMXCameraAdapter* ca)
                : Thread(false), mCameraAdapter(ca) { }

            virtual bool threadLoop() {
                bool ret;
                ret = Handler();
                return ret;
            }

            status_t put(Message* msg){
                return mCommandMsgQ.put(msg);
            }

            enum {
                COMMAND_EXIT = -1,
                CAMERA_START_IMAGE_CAPTURE = 0,
                CAMERA_PERFORM_AUTOFOCUS
            };

        private:
            bool Handler();
            MessageQueue mCommandMsgQ;
            OMXCameraAdapter* mCameraAdapter;
    };
    sp<CommandHandler> mCommandHandler;

public:

private:

    //AF callback
    status_t setFocusCallback(bool enabled);

    //OMX Capabilities data
    static const CapResolution mImageCapRes [];
    static const CapResolution mPreviewRes [];
    static const CapResolution mThumbRes [];
    static const CapPixelformat mPixelformats [];
    static const CapFramerate mFramerates [];
    static const CapZoom mZoomStages [];
    static const CapEVComp mEVCompRanges [];
    static const CapISO mISOStages [];

    OMX_VERSIONTYPE mCompRevision;

    //OMX Component UUID
    OMX_UUIDTYPE mCompUUID;

    //Current Focus distances
    char mFocusDistNear[FOCUS_DIST_SIZE];
    char mFocusDistOptimal[FOCUS_DIST_SIZE];
    char mFocusDistFar[FOCUS_DIST_SIZE];
    char mFocusDistBuffer[FOCUS_DIST_BUFFER_SIZE];

    char mTouchCoords[TOUCH_DATA_SIZE];
    unsigned int mTouchPosX;
    unsigned int mTouchPosY;

    CaptureMode mCapMode;
    size_t mBurstFrames;
    size_t mCapturedFrames;

    bool mMeasurementEnabled;

    //Exposure Bracketing
    int mExposureBracketingValues[EXP_BRACKET_RANGE];
    size_t mExposureBracketingValidEntries;

    mutable Mutex mFaceDetectionLock;
    //Face detection status
    bool mFaceDetectionRunning;
    //Buffer for storing face detection results
    char mFaceDectionResult [FACE_DETECTION_BUFFER_SIZE];
    //Face detection threshold
    static const uint32_t FACE_THRESHOLD_DEFAULT = 100;
    uint32_t mFaceDetectionThreshold;

    //Geo-tagging
    EXIFData mEXIFData;

    //Image post-processing
    IPPMode mIPP;

    //jpeg Picture Quality
    unsigned int mPictureQuality;

    //thumbnail resolution
    unsigned int mThumbWidth, mThumbHeight;

    //thumbnail quality
    unsigned int mThumbQuality;

    //variables holding the estimated framerate
    float mFPS, mLastFPS;

    //automatically disable AF after a given amount of frames
    unsigned int mFocusThreshold;

    //This is needed for the CTS tests. They falsely assume, that during
    //smooth zoom the current zoom stage will not change within the
    //zoom callback scope, which in a real world situation is not always the
    //case. This variable will "simulate" the expected behavior
    unsigned int mZoomParameterIdx;

    //current zoom
    Mutex mZoomLock;
    bool mSmoothZoomEnabled;
    unsigned int mCurrentZoomIdx, mTargetZoomIdx;
    int mZoomInc;
    bool mReturnZoomStatus;
    static const int32_t ZOOM_STEPS [];

     //local copy
    OMX_VERSIONTYPE mLocalVersionParam;

    unsigned int mPending3Asettings;
    Gen3A_settings mParameters3A;

    CameraParameters mParams;
    unsigned int mPictureRotation;
    bool mFocusStarted;
    Mutex mFocusLock;
    bool mWaitingForSnapshot;
    int mSnapshotCount;
    bool mCaptureConfigured;

    //Temporal bracketing management data
    mutable Mutex mBracketingLock;
    bool *mBracketingBuffersQueued;
    int mBracketingBuffersQueuedCount;
    int mLastBracetingBufferIdx;
    bool mBracketingEnabled;
    int mBracketingRange;

    CameraParameters mParameters;
    OMXCameraAdapterComponentContext mCameraAdapterParameters;
    bool mFirstTimeInit;

    S3DFrameLayout mS3DImageFormat;

    ///Semaphores used internally
    Vector<struct Message *> mEventSignalQ;
    Mutex mLock;
    bool mPreviewing;
    bool mCapturing;

    OMX_STATETYPE mComponentState;

    bool mVnfEnabled;
    bool mVstabEnabled;

    int mSensorOrientation;
    bool mSensorOverclock;

    //Indicates if we should leave
    //OMX_Executing state during
    //stop-/startPreview
    bool mOMXStateSwitch;

    int mFrameCount;
    int mLastFrameCount;
    unsigned int mIter;
    nsecs_t mLastFPSTime;

    int mSensorIndex;
    CodingMode mCodingMode;
    Mutex mEventLock;

    // Time source delta of ducati & system time
    OMX_TICKS mTimeSourceDelta;
    bool onlyOnce;


    Semaphore mCaptureSem;
    bool mCaptureSignalled;

    void *mSMALCDataRecord;
    unsigned int mSMALCDataSize;
    void *mSMALC_DCCFileDesc;
    unsigned int mSMALC_DCCDescSize;
    mutable Mutex mSMALCLock;
};
}; //// namespace
#endif //OMX_CAMERA_ADAPTER_H


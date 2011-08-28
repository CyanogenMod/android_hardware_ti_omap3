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
* @file OMXCameraAdapter.cpp
*
* This file maps the Camera Hardware Interface to OMX.
*
*/

#include "CameraHal.h"
#include "OMXCameraAdapter.h"
#include "ErrorUtils.h"
#include "TICameraParameters.h"
#include <signal.h>
#include <math.h>

#include <cutils/properties.h>
#define UNLIKELY( exp ) (__builtin_expect( (exp) != 0, false ))
static int mDebugFps = 0;

#define Q16_OFFSET 16

#define TOUCH_FOCUS_RANGE 0xFF

#define AF_CALLBACK_TIMEOUT 10000000 //10 seconds timeout


#define IMAGE_CAPTURE_TIMEOUT 3000000 //3 // 3 second timeout

#define HERE(Msg) {CAMHAL_LOGEB("--===line %d, %s===--\n", __LINE__, Msg);}
#define HERD(Msg) {CAMHAL_LOGEB("--===line %d, 0x%lx===--\n", __LINE__, (OMX_U32) Msg);}

#define DBG_SMALC_CFG               (2050)
#define DBG_SMALC_DCC_FILE_DESC     (2051)
#define DML_END_MARKER              (0xBABACECA)

namespace android {

#undef LOG_TAG
///Maintain a separate tag for OMXCameraAdapter logs to isolate issues OMX specific
#define LOG_TAG "OMXCameraAdapter"

//frames skipped before recalculating the framerate
#define FPS_PERIOD 30

static OMXCameraAdapter *gCameraAdapter = NULL;
Mutex gAdapterLock;

//Signal handler
static void SigHandler(int sig)
{
    Mutex::Autolock lock(gAdapterLock);

    if ( SIGTERM == sig )
        {
        CAMHAL_LOGDA("SIGTERM has been received");
        if ( NULL != gCameraAdapter )
            {
            delete gCameraAdapter;
            gCameraAdapter = NULL;
            }
        exit(0);
        }
    else if (SIGALRM == sig)
        {
        CAMHAL_LOGDA("SIGALRM has been received");
        if ( NULL != gCameraAdapter )
            {
            delete gCameraAdapter;
            gCameraAdapter = NULL;
            }
        }
}

/*--------------------Camera Adapter Class STARTS here-----------------------------*/

const char OMXCameraAdapter::EXIFASCIIPrefix [] = { 0x41, 0x53, 0x43, 0x49, 0x49, 0x0, 0x0, 0x0 };

const int32_t OMXCameraAdapter::ZOOM_STEPS [ZOOM_STAGES] =  {
                                                                            65536, 68157, 70124, 72745,
                                                                            75366, 77988, 80609, 83231,
                                                                            86508, 89784, 92406, 95683,
                                                                            99615, 102892, 106168, 110100,
                                                                            114033, 117965, 122552, 126484,
                                                                            131072, 135660, 140247, 145490,
                                                                            150733, 155976, 161219, 167117,
                                                                            173015, 178913, 185467, 192020,
                                                                            198574, 205783, 212992, 220201,
                                                                            228065, 236585, 244449, 252969,
                                                                            262144, 271319, 281149, 290980,
                                                                            300810, 311951, 322437, 334234,
                                                                            346030, 357827, 370934, 384041,
                                                                            397148, 411566, 425984, 441057,
                                                                            456131, 472515, 488899, 506593,
                                                                            524288 };

status_t OMXCameraAdapter::initialize(int sensor_index)
{
    LOG_FUNCTION_NAME

    char value[PROPERTY_VALUE_MAX];
    property_get("debug.camera.showfps", value, "0");
    mDebugFps = atoi(value);

    TIMM_OSAL_ERRORTYPE osalError = OMX_ErrorNone;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    status_t ret = NO_ERROR;


    mLocalVersionParam.s.nVersionMajor = 0x1;
    mLocalVersionParam.s.nVersionMinor = 0x1;
    mLocalVersionParam.s.nRevision = 0x0 ;
    mLocalVersionParam.s.nStep =  0x0;

    mPending3Asettings = 0;//E3AsettingsAll;

    mCaptureSem.Create(0);

    ///Event semaphore used for
    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    if ( mComponentState != OMX_StateExecuting ){
        ///Update the preview and image capture port indexes
        mCameraAdapterParameters.mPrevPortIndex = OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW;
        // temp changed in order to build OMX_CAMERA_PORT_VIDEO_OUT_IMAGE;
        mCameraAdapterParameters.mImagePortIndex = OMX_CAMERA_PORT_IMAGE_OUT_IMAGE;
        mCameraAdapterParameters.mMeasurementPortIndex = OMX_CAMERA_PORT_VIDEO_OUT_MEASUREMENT;
        //currently not supported use preview port instead
        mCameraAdapterParameters.mVideoPortIndex = OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW;

        if(!mCameraAdapterParameters.mHandleComp)
            {
            ///Initialize the OMX Core
            eError = OMX_Init();

            if(eError!=OMX_ErrorNone)
                {
                CAMHAL_LOGEB("OMX_Init -0x%x", eError);
                }
            GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

            ///Setup key parameters to send to Ducati during init
            OMX_CALLBACKTYPE oCallbacks;

            /* Initialize the callback handles */
            oCallbacks.EventHandler    = android::OMXCameraAdapterEventHandler;
            oCallbacks.EmptyBufferDone = android::OMXCameraAdapterEmptyBufferDone;
            oCallbacks.FillBufferDone  = android::OMXCameraAdapterFillBufferDone;

            ///Get the handle to the OMX Component
            mCameraAdapterParameters.mHandleComp = NULL;
            eError = OMX_GetHandle(&(mCameraAdapterParameters.mHandleComp), //     previously used: OMX_GetHandle
                                        (OMX_STRING)"OMX.TI.DUCATI1.VIDEO.CAMERA" ///@todo Use constant instead of hardcoded name
                                        , this
                                        , &oCallbacks);

            if(eError!=OMX_ErrorNone)
                {
                CAMHAL_LOGEB("OMX_GetHandle -0x%x", eError);
                }
            GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);


             eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                          OMX_CommandPortDisable,
                                          OMX_ALL,
                                          NULL);

             if(eError!=OMX_ErrorNone)
                 {
                 CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandPortDisable) -0x%x", eError);
                 }

             GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

             ///Register for port enable event
             ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                         OMX_EventCmdComplete,
                                         OMX_CommandPortEnable,
                                         mCameraAdapterParameters.mPrevPortIndex,
                                         eventSem,
                                         -1 ///Infinite timeout
                                         );
             if(ret!=NO_ERROR)
                 {
                 CAMHAL_LOGEB("Error in registering for event %d", ret);
                 goto EXIT;
                 }

            ///Enable PREVIEW Port
             eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                         OMX_CommandPortEnable,
                                         mCameraAdapterParameters.mPrevPortIndex,
                                         NULL);

             if(eError!=OMX_ErrorNone)
                 {
                 CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandPortEnable) -0x%x", eError);
                 }

             GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

             //Wait for the port enable event to occur
             eventSem.Wait();

             CAMHAL_LOGDA("-Port enable event arrived");

            }
        else
            {
            OMXCameraPortParameters * mPreviewData =
                &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

            //Apply default configs before trying to swtich to a new sensor
            ret = setFormat(OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW, *mPreviewData);
            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("Error 0x%x while applying defaults", ret);
                goto EXIT;
                }
            }
    }
    ///Select the sensor
    OMX_CONFIG_SENSORSELECTTYPE sensorSelect;
    OMX_INIT_STRUCT_PTR (&sensorSelect, OMX_CONFIG_SENSORSELECTTYPE);
    sensorSelect.eSensor = (OMX_SENSORSELECT)sensor_index;
    eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_TI_IndexConfigSensorSelect, &sensorSelect);

    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while selecting the sensor index as %d - 0x%x", sensor_index, eError);
        goto EXIT;
        }
    else
        {
        CAMHAL_LOGEB("Sensor %d selected successfully", sensor_index);
        }

    printComponentVersion(mCameraAdapterParameters.mHandleComp);

    mSensorIndex = sensor_index;
    mBracketingEnabled = false;
    mBracketingBuffersQueuedCount = 0;
    mBracketingRange = 1;
    mLastBracetingBufferIdx = 0;
    mOMXStateSwitch = false;

    if ( mComponentState != OMX_StateExecuting ){
        mPreviewing = false;
        mCapturing = false;
        mCaptureSignalled = false;
        mCaptureConfigured = false;
        mRecording = false;
        mWaitingForSnapshot = false;
        mSnapshotCount = 0;
        mComponentState = OMX_StateLoaded;

        mCapMode = HIGH_QUALITY;
        mIPP = IPP_NULL;
        mBurstFrames = 1;
        mCapturedFrames = 0;
        mPictureQuality = 100;
        mCurrentZoomIdx = 0;
        mTargetZoomIdx = 0;
        mReturnZoomStatus = false;
        mSmoothZoomEnabled = false;
        mZoomInc = 1;
        mZoomParameterIdx = 0;
        mExposureBracketingValidEntries = 0;
        mFaceDetectionThreshold = FACE_THRESHOLD_DEFAULT;
        mSensorOverclock = false;
        mS3DImageFormat = S3D_NONE;
        mEXIFData.mGPSData.mAltitudeValid = false;
        mEXIFData.mGPSData.mDatestampValid = false;
        mEXIFData.mGPSData.mLatValid = false;
        mEXIFData.mGPSData.mLongValid = false;
        mEXIFData.mGPSData.mMapDatumValid = false;
        mEXIFData.mGPSData.mProcMethodValid = false;
        mEXIFData.mGPSData.mVersionIdValid = false;
        mEXIFData.mGPSData.mTimeStampValid = false;
        mEXIFData.mModelValid = false;
        mEXIFData.mMakeValid = false;

        // initialize command handling thread
        if(mCommandHandler.get() == NULL)
            mCommandHandler = new CommandHandler(this);

        if ( NULL == mCommandHandler.get() )
        {
            CAMHAL_LOGEA("Couldn't create command handler");
            return NO_MEMORY;
        }

        ret = mCommandHandler->run("CallbackThread", PRIORITY_URGENT_DISPLAY);
        if ( ret != NO_ERROR )
        {
            if( ret == INVALID_OPERATION){
                CAMHAL_LOGDA("command handler thread already runnning!!");
            }
            else {
                CAMHAL_LOGEA("Couldn't run command handlerthread");
                return ret;
            }
        }

        //Remove any unhandled events
        if ( !mEventSignalQ.isEmpty() )
            {
            for (unsigned int i = 0 ; i < mEventSignalQ.size() ; i++ )
                {
                Message *msg = mEventSignalQ.itemAt(i);
                //remove from queue and free msg
                mEventSignalQ.removeAt(i);
                if ( NULL != msg )
                    {
                    free(msg);
                    }
                }
            }

        //Setting this flag will that the first setParameter call will apply all 3A settings
        //and will not conditionally apply based on current values.
        mFirstTimeInit = true;

        memset(mExposureBracketingValues, 0, EXP_BRACKET_RANGE*sizeof(int));
        mTouchPosX = 0;
        mTouchPosY = 0;

        mMeasurementEnabled = false;
        mFaceDetectionRunning = false;

        if ( NULL != mFaceDectionResult )
            {
            memset(mFaceDectionResult, '\0', FACE_DETECTION_BUFFER_SIZE);
            }

        memset(&mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex], 0, sizeof(OMXCameraPortParameters));
        memset(&mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex], 0, sizeof(OMXCameraPortParameters));
    }
    return ErrorUtils::omxToAndroidError(eError);

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of ret %d eError = %x", __FUNCTION__, ret, eError);

    if ( eError != OMX_ErrorNone )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    if(mCameraAdapterParameters.mHandleComp)
        {
        ///Free the OMX component handle in case of error
        OMX_FreeHandle(mCameraAdapterParameters.mHandleComp);
        }

    ///De-init the OMX
    OMX_Deinit();

    LOG_FUNCTION_NAME_EXIT


    return ErrorUtils::omxToAndroidError(eError);
}

OMXCameraAdapter::OMXCameraPortParameters *OMXCameraAdapter::getPortParams(CameraFrame::FrameType frameType)
{
    OMXCameraAdapter::OMXCameraPortParameters *ret = NULL;

    switch ( frameType )
    {
    case CameraFrame::IMAGE_FRAME:
    case CameraFrame::RAW_FRAME:
        ret = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];
        break;
    case CameraFrame::PREVIEW_FRAME_SYNC:
    case CameraFrame::SNAPSHOT_FRAME:
    case CameraFrame::VIDEO_FRAME_SYNC:
        ret = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
        break;
    case CameraFrame::FRAME_DATA_SYNC:
        ret = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mMeasurementPortIndex];
        break;
    default:
        break;
    };

    return ret;
}

status_t OMXCameraAdapter::fillThisBuffer(void* frameBuf, CameraFrame::FrameType frameType)
{
    status_t ret = NO_ERROR;
    OMXCameraPortParameters *port = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if ( !mPreviewing )
        {
        ret = -EINVAL;
        }

    if ( NULL == frameBuf )
        {
        ret = -EINVAL;
        }

    if ( ( NO_ERROR == ret ) && ( ( CameraFrame::IMAGE_FRAME == frameType ) ||
         ( CameraFrame::RAW_FRAME == frameType ) ) )
        {
        if ( ( 1 > mCapturedFrames ) && ( !mBracketingEnabled ) )
            {
            stopImageCapture();
            return NO_ERROR;
            }
        }

    if ( NO_ERROR == ret )
        {
        port = getPortParams(frameType);
        if ( NULL == port )
            {
            CAMHAL_LOGEB("Invalid frameType 0x%x", frameType);
            ret = -EINVAL;
            }
        }

    if ( NO_ERROR == ret )
        {

        for ( int i = 0 ; i < port->mNumBufs ; i++)
            {
            if ( port->mBufferHeader[i]->pBuffer == frameBuf )
                {
                eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp, port->mBufferHeader[i]);
                if ( eError != OMX_ErrorNone )
                    {
                    CAMHAL_LOGDB("OMX_FillThisBuffer 0x%x", eError);
                    ret = -1;
                    }
                break;
                }
            }

        }

    return ret;
}

status_t OMXCameraAdapter::setParameters(const CameraParameters &params)
{
    LOG_FUNCTION_NAME

    const char * str = NULL;
    int mode = 0;
    int expVal, gainVal;
    status_t ret = NO_ERROR;
    bool updateImagePortParams = false;
    int minFramerate, maxFramerate, frameRate;
    const char *valstr = NULL;
    const char *oldstr = NULL;
    int w, h;
    OMX_COLOR_FORMATTYPE pixFormat;

    ///@todo Include more camera parameters
    if ( (valstr = params.getPreviewFormat()) != NULL )
        {
        if (strcmp(valstr, (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) == 0)
            {
            CAMHAL_LOGDA("CbYCrY format selected");
            pixFormat = OMX_COLOR_FormatCbYCrY;
            }
        else if(strcmp(valstr, (const char *) CameraParameters::PIXEL_FORMAT_YUV420SP) == 0)
            {
            CAMHAL_LOGDA("YUV420SP format selected");
            pixFormat = OMX_COLOR_FormatYUV420SemiPlanar;
            }
        else if(strcmp(valstr, (const char *) CameraParameters::PIXEL_FORMAT_RGB565) == 0)
            {
            CAMHAL_LOGDA("RGB565 format selected");
            pixFormat = OMX_COLOR_Format16bitRGB565;
            }
        else
            {
            CAMHAL_LOGDA("Invalid format, CbYCrY format selected as default");
            pixFormat = OMX_COLOR_FormatCbYCrY;
            }
        }
    else
        {
        CAMHAL_LOGEA("Preview format is NULL, defaulting to CbYCrY");
        pixFormat = OMX_COLOR_FormatCbYCrY;
        }

    str = params.get(TICameraParameters::KEY_S3D_FRAME_LAYOUT);
    if (str != NULL)
        {
        if (strcmp(str, TICameraParameters::S3D_TB_FULL) == 0)
            {
            mS3DImageFormat = S3D_TB_FULL;
            }
        else if (strcmp(str, TICameraParameters::S3D_SS_FULL) == 0)
            {
            mS3DImageFormat = S3D_SS_FULL;
            }
        else if (strcmp(str, TICameraParameters::S3D_TB_SUBSAMPLED) == 0)
            {
            mS3DImageFormat = S3D_TB_SUBSAMPLED;
            }
        else if (strcmp(str, TICameraParameters::S3D_SS_SUBSAMPLED) == 0)
            {
            mS3DImageFormat = S3D_SS_SUBSAMPLED;
            }
        else
            {
            mS3DImageFormat = S3D_NONE;
            }
        }
    else
        {
        mS3DImageFormat = S3D_NONE;
        }

    OMXCameraPortParameters *cap;
    cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

    params.getPreviewSize(&w, &h);
    frameRate = params.getPreviewFrameRate();
    minFramerate = params.getInt(TICameraParameters::KEY_MINFRAMERATE);
    maxFramerate = params.getInt(TICameraParameters::KEY_MAXFRAMERATE);
    if ( ( 0 < minFramerate ) &&
         ( 0 < maxFramerate ) )
        {
        if ( minFramerate > maxFramerate )
            {
             CAMHAL_LOGEA(" Min FPS set higher than MAX. So setting MIN and MAX to the higher value");
             maxFramerate = minFramerate;
            }

        if ( 0 >= frameRate )
            {
            frameRate = maxFramerate;
            }

        if( ( cap->mMinFrameRate != minFramerate ) ||
            ( cap->mMaxFrameRate != maxFramerate ) )
            {
            cap->mMinFrameRate = minFramerate;
            cap->mMaxFrameRate = maxFramerate;
            setVFramerate(cap->mMinFrameRate, cap->mMaxFrameRate);
            }
        }

    if ( 0 < frameRate )
        {
        cap->mColorFormat = pixFormat;
        cap->mWidth = w;
        cap->mHeight = h;
        cap->mFrameRate = frameRate;

        CAMHAL_LOGVB("Prev: cap.mColorFormat = %d", (int)cap->mColorFormat);
        CAMHAL_LOGVB("Prev: cap.mWidth = %d", (int)cap->mWidth);
        CAMHAL_LOGVB("Prev: cap.mHeight = %d", (int)cap->mHeight);
        CAMHAL_LOGVB("Prev: cap.mFrameRate = %d", (int)cap->mFrameRate);

        //TODO: Add an additional parameter for video resolution
       //use preview resolution for now
        cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
        cap->mColorFormat = pixFormat;
        cap->mWidth = w;
        cap->mHeight = h;
        cap->mFrameRate = frameRate;

        CAMHAL_LOGVB("Video: cap.mColorFormat = %d", (int)cap->mColorFormat);
        CAMHAL_LOGVB("Video: cap.mWidth = %d", (int)cap->mWidth);
        CAMHAL_LOGVB("Video: cap.mHeight = %d", (int)cap->mHeight);
        CAMHAL_LOGVB("Video: cap.mFrameRate = %d", (int)cap->mFrameRate);

        ///mStride is set from setBufs() while passing the APIs
        cap->mStride = 4096;
        cap->mBufSize = cap->mStride * cap->mHeight;
        }

    if ( ( cap->mWidth >= 1920 ) &&
         ( cap->mHeight >= 1080 ) &&
         ( cap->mFrameRate >= FRAME_RATE_FULL_HD ) &&
         ( !mSensorOverclock ) )
        {
        mOMXStateSwitch = true;
        }
    else if ( ( ( cap->mWidth < 1920 ) ||
               ( cap->mHeight < 1080 ) ||
               ( cap->mFrameRate < FRAME_RATE_FULL_HD ) ) &&
               ( mSensorOverclock ) )
        {
        mOMXStateSwitch = true;
        }

    cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    params.getPictureSize(&w, &h);

    if ( ( w != ( int ) cap->mWidth ) ||
          ( h != ( int ) cap->mHeight ) )
        {
        updateImagePortParams = true;
        }

    cap->mWidth = w;
    cap->mHeight = h;
    //TODO: Support more pixelformats
    cap->mStride = 2;

    CAMHAL_LOGVB("Image: cap.mWidth = %d", (int)cap->mWidth);
    CAMHAL_LOGVB("Image: cap.mHeight = %d", (int)cap->mHeight);

    if ( (valstr = params.getPictureFormat()) != NULL )
        {
        if (strcmp(valstr, (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) == 0)
            {
            CAMHAL_LOGDA("CbYCrY format selected");
            pixFormat = OMX_COLOR_FormatCbYCrY;
            }
        else if(strcmp(valstr, (const char *) CameraParameters::PIXEL_FORMAT_YUV420SP) == 0)
            {
            CAMHAL_LOGDA("YUV420SP format selected");
            pixFormat = OMX_COLOR_FormatYUV420SemiPlanar;
            }
        else if(strcmp(valstr, (const char *) CameraParameters::PIXEL_FORMAT_RGB565) == 0)
            {
            CAMHAL_LOGDA("RGB565 format selected");
            pixFormat = OMX_COLOR_Format16bitRGB565;
            }
        else if(strcmp(valstr, (const char *) CameraParameters::PIXEL_FORMAT_JPEG) == 0)
            {
            CAMHAL_LOGDA("JPEG format selected");
            pixFormat = OMX_COLOR_FormatUnused;
            mCodingMode = CodingNone;
            }
        else if(strcmp(valstr, (const char *) TICameraParameters::PIXEL_FORMAT_JPS) == 0)
            {
            CAMHAL_LOGDA("JPS format selected");
            pixFormat = OMX_COLOR_FormatUnused;
            mCodingMode = CodingJPS;
            }
        else if(strcmp(valstr, (const char *) TICameraParameters::PIXEL_FORMAT_MPO) == 0)
            {
            CAMHAL_LOGDA("MPO format selected");
            pixFormat = OMX_COLOR_FormatUnused;
            mCodingMode = CodingMPO;
            }
        else if(strcmp(valstr, (const char *) TICameraParameters::PIXEL_FORMAT_RAW_JPEG) == 0)
            {
            CAMHAL_LOGDA("RAW + JPEG format selected");
            pixFormat = OMX_COLOR_FormatUnused;
            mCodingMode = CodingRAWJPEG;
            }
        else if(strcmp(valstr, (const char *) TICameraParameters::PIXEL_FORMAT_RAW_MPO) == 0)
            {
            CAMHAL_LOGDA("RAW + MPO format selected");
            pixFormat = OMX_COLOR_FormatUnused;
            mCodingMode = CodingRAWMPO;
            }
        else if(strcmp(valstr, (const char *) TICameraParameters::PIXEL_FORMAT_RAW) == 0)
            {
            CAMHAL_LOGDA("RAW Picture format selected");
            pixFormat = OMX_COLOR_FormatRawBayer10bit;
            }
        else
            {
            CAMHAL_LOGEA("Invalid format, JPEG format selected as default");
            pixFormat = OMX_COLOR_FormatUnused;
            }
        }
    else
        {
        CAMHAL_LOGEA("Picture format is NULL, defaulting to JPEG");
        pixFormat = OMX_COLOR_FormatUnused;
        }

    if ( pixFormat != cap->mColorFormat )
        {
        updateImagePortParams = true;
        cap->mColorFormat = pixFormat;
        }

    if ( updateImagePortParams )
        {
        Mutex::Autolock lock(mLock);
        if ( !mCapturing )
            {
            setFormat(OMX_CAMERA_PORT_IMAGE_OUT_IMAGE, *cap);
            }
        }

    str = params.get(TICameraParameters::KEY_EXPOSURE_MODE);
    mode = getLUTvalue_HALtoOMX( str, ExpLUT);
    if ( ( str != NULL ) && ( mParameters3A.Exposure != mode ) )
        {
        mParameters3A.Exposure = mode;
        CAMHAL_LOGEB("Exposure mode %d", mode);
        if ( 0 <= mParameters3A.Exposure )
            {
            mPending3Asettings |= SetExpMode;
            }
        }

    expVal = params.getInt(TICameraParameters::KEY_MANUAL_EXPOSURE_LEFT);
    if (expVal >= 0 && expVal < MAX_MANUAL_EXPOSURE_MS)
        {
        mParameters3A.ExposureValueLeft = expVal;
        mPending3Asettings |= SetManualExposure;
        CAMHAL_LOGEB("Manual Exposure Left value %d ms", (int) expVal);
        }

    expVal = params.getInt(TICameraParameters::KEY_MANUAL_EXPOSURE_RIGHT);
    if (expVal >= 0 && expVal < MAX_MANUAL_EXPOSURE_MS)
        {
        mParameters3A.ExposureValueRight = expVal;
        mPending3Asettings |= SetManualExposure;
        CAMHAL_LOGEB("Manual Exposure Right value %d ms", (int) expVal);
        }

    gainVal = params.getInt(TICameraParameters::KEY_MANUAL_GAIN_ISO_LEFT);
    if (gainVal >= 0 && gainVal <= MAX_MANUAL_GAIN_ISO)
        {
        mParameters3A.ManualGainISOLeft = gainVal;
        mPending3Asettings |= SetManualGain;
        CAMHAL_LOGEB("Manual Gain Left ISO %d ", (int) gainVal);
        }

    gainVal = params.getInt(TICameraParameters::KEY_MANUAL_GAIN_ISO_RIGHT);
    if (gainVal >= 0 && gainVal <= MAX_MANUAL_GAIN_ISO)
        {
        mParameters3A.ManualGainISORight = gainVal;
        mPending3Asettings |= SetManualGain;
        CAMHAL_LOGEB("Manual Gain Right ISO %d ", (int) gainVal);
        }

    str = params.get(CameraParameters::KEY_WHITE_BALANCE);
    mode = getLUTvalue_HALtoOMX( str, WBalLUT);
    if ( ( mFirstTimeInit || ( str != NULL ) ) && ( mode != mParameters3A.WhiteBallance ) )
        {
        mParameters3A.WhiteBallance = mode;
        CAMHAL_LOGEB("Whitebalance mode %d", mode);
        if ( 0 <= mParameters3A.WhiteBallance )
            {
            mPending3Asettings |= SetWhiteBallance;
            }
        }

    if ( 0 <= params.getInt(TICameraParameters::KEY_CONTRAST) )
        {
        if ( mFirstTimeInit || ( (mParameters3A.Contrast  + CONTRAST_OFFSET) != params.getInt(TICameraParameters::KEY_CONTRAST)) )
            {
            mParameters3A.Contrast = params.getInt(TICameraParameters::KEY_CONTRAST) - CONTRAST_OFFSET;
            CAMHAL_LOGEB("Contrast %d", mParameters3A.Contrast);
            mPending3Asettings |= SetContrast;
            }
        }

    if ( 0 <= params.getInt(TICameraParameters::KEY_SHARPNESS) )
        {
        if ( mFirstTimeInit || ((mParameters3A.Sharpness + SHARPNESS_OFFSET) != params.getInt(TICameraParameters::KEY_SHARPNESS)))
            {
            mParameters3A.Sharpness = params.getInt(TICameraParameters::KEY_SHARPNESS) - SHARPNESS_OFFSET;
            CAMHAL_LOGEB("Sharpness %d", mParameters3A.Sharpness);
            mPending3Asettings |= SetSharpness;
            }
        }

    if ( 0 <= params.getInt(TICameraParameters::KEY_SATURATION) )
        {
        if ( mFirstTimeInit || ((mParameters3A.Saturation + SATURATION_OFFSET) != params.getInt(TICameraParameters::KEY_SATURATION)) )
            {
            mParameters3A.Saturation = params.getInt(TICameraParameters::KEY_SATURATION) - SATURATION_OFFSET;
            CAMHAL_LOGEB("Saturation %d", mParameters3A.Saturation);
            mPending3Asettings |= SetSaturation;
            }
        }

    if ( 0 <= params.getInt(TICameraParameters::KEY_BRIGHTNESS) )
        {
        if ( mFirstTimeInit || (( mParameters3A.Brightness !=  ( unsigned int ) params.getInt(TICameraParameters::KEY_BRIGHTNESS))) )
            {
            mParameters3A.Brightness = (unsigned)params.getInt(TICameraParameters::KEY_BRIGHTNESS);
            CAMHAL_LOGEB("Brightness %d", mParameters3A.Brightness);
            mPending3Asettings |= SetBrightness;
            }
        }

    str = params.get(CameraParameters::KEY_ANTIBANDING);
    mode = getLUTvalue_HALtoOMX(str,FlickerLUT);
    if ( mFirstTimeInit || ( ( str != NULL ) && ( mParameters3A.Flicker != mode ) ))
        {
        mParameters3A.Flicker = mode;
        CAMHAL_LOGEB("Flicker %d", mParameters3A.Flicker);
        if ( 0 <= mParameters3A.Flicker )
            {
            mPending3Asettings |= SetFlicker;
            }
        }

    str = params.get(TICameraParameters::KEY_ISO);
    mode = getLUTvalue_HALtoOMX(str, IsoLUT);
    CAMHAL_LOGEB("ISO mode arrived in HAL : %s", str);
    if ( mFirstTimeInit || (  ( str != NULL ) && ( mParameters3A.ISO != mode )) )
        {
        mParameters3A.ISO = mode;
        CAMHAL_LOGEB("ISO %d", mParameters3A.ISO);
        if ( 0 <= mParameters3A.ISO )
            {
            mPending3Asettings |= SetISO;
            }
        }

    str = params.get(CameraParameters::KEY_FOCUS_MODE);
    mode = getLUTvalue_HALtoOMX(str, FocusLUT);
    if ( mFirstTimeInit || ( ( str != NULL ) && ( mParameters3A.Focus != mode ) ) )
        {
        //Apply focus mode immediatly only if  CAF  or Inifinity are selected
        if ( ( mode == OMX_IMAGE_FocusControlAuto ) ||
             ( mode == OMX_IMAGE_FocusControlAutoInfinity ) )
            {
            mPending3Asettings |= SetFocus;
            mParameters3A.Focus = mode;
            }
        else if ( mParameters3A.Focus == OMX_IMAGE_FocusControlAuto )
            {
            //If we switch from CAF to something else, then disable CAF
            mPending3Asettings |= SetFocus;
            mParameters3A.Focus = OMX_IMAGE_FocusControlOff;
            }

        mParameters3A.Focus = mode;
        CAMHAL_LOGEB("Focus %x", mParameters3A.Focus);
        }

    str = params.get(TICameraParameters::KEY_TOUCH_POS);
    if ( NULL != str ) {
        strncpy(mTouchCoords, str, TOUCH_DATA_SIZE-1);
        parseTouchPosition(mTouchCoords, mTouchPosX, mTouchPosY);
        CAMHAL_LOGDB("Touch position %d,%d", mTouchPosX, mTouchPosY);
    }

    str = params.get(TICameraParameters::KEY_EXP_BRACKETING_RANGE);
    if ( NULL != str ) {
        parseExpRange(str, mExposureBracketingValues, EXP_BRACKET_RANGE, mExposureBracketingValidEntries);
    } else {
        mExposureBracketingValidEntries = 0;
    }

    str = params.get(CameraParameters::KEY_EXPOSURE_COMPENSATION);
    if ( mFirstTimeInit || (( str != NULL ) && (mParameters3A.EVCompensation != params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION))))
        {
        CAMHAL_LOGEB("Setting EV Compensation to %d", params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION));

        mParameters3A.EVCompensation = params.getInt(CameraParameters::KEY_EXPOSURE_COMPENSATION);
        mPending3Asettings |= SetEVCompensation;
        }

    str = params.get(CameraParameters::KEY_SCENE_MODE);
    mode = getLUTvalue_HALtoOMX( str, SceneLUT);
    if (  mFirstTimeInit || (( str != NULL ) && ( mParameters3A.SceneMode != mode )) )
        {
        if ( 0 <= mode )
            {
            mParameters3A.SceneMode = mode;
            mPending3Asettings |= SetSceneMode;
            }
        else
            {
            mParameters3A.SceneMode = OMX_Manual;
            }

        CAMHAL_LOGEB("SceneMode %d", mParameters3A.SceneMode);
        }

    str = params.get(CameraParameters::KEY_FLASH_MODE);
    mode = getLUTvalue_HALtoOMX( str, FlashLUT);
    if (  mFirstTimeInit || (( str != NULL ) && ( mParameters3A.FlashMode != mode )) )
        {
        if ( 0 <= mode )
            {
            mParameters3A.FlashMode = mode;
            mPending3Asettings |= SetFlash;
            }
        else
            {
            mParameters3A.FlashMode = OMX_Manual;
            }
        }

    LOGE("Flash Setting %s", str);
    LOGE("FlashMode %d", mParameters3A.FlashMode);

    str = params.get(CameraParameters::KEY_EFFECT);
    mode = getLUTvalue_HALtoOMX( str, EffLUT);
    if (  mFirstTimeInit || (( str != NULL ) && ( mParameters3A.Effect != mode )) )
        {
        mParameters3A.Effect = mode;
        CAMHAL_LOGEB("Effect %d", mParameters3A.Effect);
        if ( 0 <= mParameters3A.Effect )
            {
            mPending3Asettings |= SetEffect;
            }
        }

    if ( params.getInt(CameraParameters::KEY_ROTATION) != -1 )
        {
        mPictureRotation = params.getInt(CameraParameters::KEY_ROTATION);
        }
    else
        {
        mPictureRotation = 0;
        }

    CAMHAL_LOGVB("Picture Rotation set %d", mPictureRotation);

    CaptureMode capMode;
    if ( (valstr = params.get(TICameraParameters::KEY_CAP_MODE)) != NULL )
        {
        if (strcmp(valstr, (const char *) TICameraParameters::HIGH_PERFORMANCE_MODE) == 0)
            {
            capMode = OMXCameraAdapter::HIGH_SPEED;
            }
        else if (strcmp(valstr, (const char *) TICameraParameters::HIGH_QUALITY_MODE) == 0)
            {
            capMode = OMXCameraAdapter::HIGH_QUALITY;
            }
        else if (strcmp(valstr, (const char *) TICameraParameters::VIDEO_MODE) == 0)
            {
            capMode = OMXCameraAdapter::VIDEO_MODE;
            }
        else
            {
            capMode = OMXCameraAdapter::HIGH_QUALITY;
            }
        }
    else
        {
        capMode = OMXCameraAdapter::HIGH_QUALITY;
        }

    if ( mCapMode != capMode )
        {
        mCapMode = capMode;
        mOMXStateSwitch = true;
        }

    CAMHAL_LOGDB("Capture Mode set %d", mCapMode);

    // Read Sensor Orientation and set it based on perating mode

    if (( params.getInt(TICameraParameters::KEY_SENSOR_ORIENTATION) != -1 ) && (mCapMode == OMXCameraAdapter::VIDEO_MODE))
        {
        mSensorOrientation = params.getInt(TICameraParameters::KEY_SENSOR_ORIENTATION);
        if (mSensorOrientation == 270 ||mSensorOrientation==90)
            {
            CAMHAL_LOGEA(" Orientation is 270/90. So setting counter rotation  to Ducati");
            mSensorOrientation +=180;
            mSensorOrientation%=360;
            }
        }
    else
        {
        mSensorOrientation = 0;
        }

    CAMHAL_LOGEB("Sensor Orientation  set : %d", mSensorOrientation);

    /// Configure IPP, LDCNSF, GBCE and GLBCE only in HQ mode
    IPPMode ipp;
    if(mCapMode == OMXCameraAdapter::HIGH_QUALITY)
        {
          if ( (valstr = params.get(TICameraParameters::KEY_IPP)) != NULL )
            {
            if (strcmp(valstr, (const char *) TICameraParameters::IPP_LDCNSF) == 0)
                {
                ipp = OMXCameraAdapter::IPP_LDCNSF;
                }
            else if (strcmp(valstr, (const char *) TICameraParameters::IPP_LDC) == 0)
                {
                ipp = OMXCameraAdapter::IPP_LDC;
                }
            else if (strcmp(valstr, (const char *) TICameraParameters::IPP_NSF) == 0)
                {
                ipp = OMXCameraAdapter::IPP_NSF;
                }
            else if (strcmp(valstr, (const char *) TICameraParameters::IPP_NONE) == 0)
                {
                ipp = OMXCameraAdapter::IPP_NONE;
                }
            else
                {
                ipp = OMXCameraAdapter::IPP_NONE;
                }
            }
        else
            {
            ipp = OMXCameraAdapter::IPP_NONE;
            }

        CAMHAL_LOGEB("IPP Mode set %d", ipp);

        if (((valstr = params.get(TICameraParameters::KEY_GBCE)) != NULL) )
            {
            // Configure GBCE only if the setting has changed since last time
            oldstr = mParams.get(TICameraParameters::KEY_GBCE);
            bool cmpRes = true;
            if ( NULL != oldstr )
                {
                cmpRes = strcmp(valstr, oldstr) != 0;
                }
            else
                {
                cmpRes = true;
                }


            if( cmpRes )
                {
                if (strcmp(valstr, ( const char * ) TICameraParameters::GBCE_ENABLE ) == 0)
                    {
                    setGBCE(OMXCameraAdapter::BRIGHTNESS_ON);
                    }
                else if (strcmp(valstr, ( const char * ) TICameraParameters::GBCE_DISABLE ) == 0)
                    {
                    setGBCE(OMXCameraAdapter::BRIGHTNESS_OFF);
                    }
                else
                    {
                    setGBCE(OMXCameraAdapter::BRIGHTNESS_OFF);
                    }
                }
            }
        else
            {
            //Disable GBCE by default
            setGBCE(OMXCameraAdapter::BRIGHTNESS_OFF);
            }

        if ( ( valstr = params.get(TICameraParameters::KEY_GLBCE) ) != NULL )
            {
            // Configure GLBCE only if the setting has changed since last time

            oldstr = mParams.get(TICameraParameters::KEY_GLBCE);
            bool cmpRes = true;
            if ( NULL != oldstr )
                {
                cmpRes = strcmp(valstr, oldstr) != 0;
                }
            else
                {
                cmpRes = true;
                }


            if( cmpRes )
                {
                if (strcmp(valstr, ( const char * ) TICameraParameters::GLBCE_ENABLE ) == 0)
                    {
                    setGLBCE(OMXCameraAdapter::BRIGHTNESS_ON);
                    }
                else if (strcmp(valstr, ( const char * ) TICameraParameters::GLBCE_DISABLE ) == 0)
                    {
                    setGLBCE(OMXCameraAdapter::BRIGHTNESS_OFF);
                    }
                else
                    {
                    setGLBCE(OMXCameraAdapter::BRIGHTNESS_OFF);
                    }
                }
            }
        else
            {
            //Disable GLBCE by default
            setGLBCE(OMXCameraAdapter::BRIGHTNESS_OFF);
            }
        }
    else
        {
        ipp = OMXCameraAdapter::IPP_NONE;
        }

    if ( mIPP != ipp )
        {
        mIPP = ipp;
        mOMXStateSwitch = true;
        }

    if ( params.getInt(TICameraParameters::KEY_BURST)  >= 1 )
        {
        mBurstFrames = params.getInt(TICameraParameters::KEY_BURST);
        }
    else
        {
        mBurstFrames = 1;
        }

    CAMHAL_LOGVB("Burst Frames set %d", mBurstFrames);

    if ( ((valstr = params.get(TICameraParameters::KEY_FACE_DETECTION_ENABLE)) != NULL) )
     {
      // Configure FD only if the setting has changed since last time
      oldstr = mParams.get(TICameraParameters::KEY_FACE_DETECTION_ENABLE);
      bool cmpRes = true;
      if ( NULL != oldstr )
           {
           cmpRes = strcmp(valstr, oldstr) != 0;
           }
      else
           {
           cmpRes = true;
           }
      if ( cmpRes )
           {
      if (strcmp(valstr, (const char *) TICameraParameters::FACE_DETECTION_ENABLE) == 0)
           {
           setFaceDetection(true);
           }
      else if (strcmp(valstr, (const char *) TICameraParameters::FACE_DETECTION_DISABLE) == 0)
           {
           setFaceDetection(false);
           }
      else
           {
           setFaceDetection(false);
           }
       }
   }

    if ( 0 <= params.getInt(TICameraParameters::KEY_FACE_DETECTION_THRESHOLD ) )
        {
        mFaceDetectionThreshold = params.getInt(TICameraParameters::KEY_FACE_DETECTION_THRESHOLD);
        }
    else
        {
        mFaceDetectionThreshold = FACE_THRESHOLD_DEFAULT;
        }

    if ( (valstr = params.get(TICameraParameters::KEY_MEASUREMENT_ENABLE)) != NULL )
        {
        if (strcmp(valstr, (const char *) TICameraParameters::MEASUREMENT_ENABLE) == 0)
            {
            mMeasurementEnabled = true;
            }
        else if (strcmp(valstr, (const char *) TICameraParameters::MEASUREMENT_DISABLE) == 0)
            {
            mMeasurementEnabled = false;
            }
        else
            {
            mMeasurementEnabled = false;
            }
        }
    else
        {
        //Disable measurement data by default
        mMeasurementEnabled = false;
        }

    if ( ( params.getInt(CameraParameters::KEY_JPEG_QUALITY)  >= MIN_JPEG_QUALITY ) &&
         ( params.getInt(CameraParameters::KEY_JPEG_QUALITY)  <= MAX_JPEG_QUALITY ) )
        {
        mPictureQuality = params.getInt(CameraParameters::KEY_JPEG_QUALITY);
        }
    else
        {
        mPictureQuality = MAX_JPEG_QUALITY;
        }

    CAMHAL_LOGVB("Picture Quality set %d", mPictureQuality);

    if ( params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH)  >= 0 )
        {
        mThumbWidth = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
        }
    else
        {
        mThumbWidth = DEFAULT_THUMB_WIDTH;
        }


    CAMHAL_LOGVB("Picture Thumb width set %d", mThumbWidth);

    if ( params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT)  >= 0 )
        {
        mThumbHeight = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
        }
    else
        {
        mThumbHeight = DEFAULT_THUMB_HEIGHT;
        }


    CAMHAL_LOGVB("Picture Thumb height set %d", mThumbHeight);


    ///Set VNF Configuration
    bool vnfEnabled = false;
    if ( params.getInt(TICameraParameters::KEY_VNF)  > 0 )
        {
        CAMHAL_LOGDA("VNF Enabled");
        vnfEnabled = true;
        }
    else
        {
        CAMHAL_LOGDA("VNF Disabled");
        vnfEnabled = false;
        }

    if ( mVnfEnabled != vnfEnabled )
        {
        mVnfEnabled = vnfEnabled;
        mOMXStateSwitch = true;
        }

    ///Set VSTAB Configuration
    bool vstabEnabled = false;
    if ( params.getInt(TICameraParameters::KEY_VSTAB)  > 0 )
        {
        CAMHAL_LOGDA("VSTAB Enabled");
        vstabEnabled = true;
        }
    else
        {
        CAMHAL_LOGDA("VSTAB Disabled");
        vstabEnabled = false;
        }

    if ( mVstabEnabled != vstabEnabled )
        {
        mVstabEnabled = vstabEnabled;
        mOMXStateSwitch = true;
        }

    //A work-around for a failing call to OMX flush buffers
    if ( ( capMode = OMXCameraAdapter::VIDEO_MODE ) &&
         ( mVstabEnabled ) )
        {
        mOMXStateSwitch = true;
        }

    //Set Auto Convergence Mode
    str = params.get((const char *) TICameraParameters::KEY_AUTOCONVERGENCE);
    if ( str != NULL )
        {
        // Set ManualConvergence default value
        OMX_S32 manualconvergence = -30;
        if ( strcmp (str, (const char *) TICameraParameters::AUTOCONVERGENCE_MODE_DISABLE) == 0 )
            {
            setAutoConvergence(OMX_TI_AutoConvergenceModeDisable, manualconvergence);
            }
        else if ( strcmp (str, (const char *) TICameraParameters::AUTOCONVERGENCE_MODE_FRAME) == 0 )
                {
                setAutoConvergence(OMX_TI_AutoConvergenceModeFrame, manualconvergence);
                }
        else if ( strcmp (str, (const char *) TICameraParameters::AUTOCONVERGENCE_MODE_CENTER) == 0 )
                {
                setAutoConvergence(OMX_TI_AutoConvergenceModeCenter, manualconvergence);
                }
        else if ( strcmp (str, (const char *) TICameraParameters::AUTOCONVERGENCE_MODE_FFT) == 0 )
                {
                setAutoConvergence(OMX_TI_AutoConvergenceModeFocusFaceTouch, manualconvergence);
                }
        else if ( strcmp (str, (const char *) TICameraParameters::AUTOCONVERGENCE_MODE_MANUAL) == 0 )
                {
                manualconvergence = (OMX_S32)params.getInt(TICameraParameters::KEY_MANUALCONVERGENCE_VALUES);
                setAutoConvergence(OMX_TI_AutoConvergenceModeManual, manualconvergence);
                }
        CAMHAL_LOGEB("AutoConvergenceMode %s, value = %d", str, (int) manualconvergence);
        }

    if ( ( params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY)  >= MIN_JPEG_QUALITY ) &&
         ( params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY)  <= MAX_JPEG_QUALITY ) )
        {
        mThumbQuality = params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY);
        }
    else
        {
        mThumbQuality = MAX_JPEG_QUALITY;
        }

    CAMHAL_LOGDB("Thumbnail Quality set %d", mThumbQuality);

        {
        Mutex::Autolock lock(mZoomLock);

        //Immediate zoom should not be avaialable while smooth zoom is running
        if ( !mSmoothZoomEnabled )
            {
            int zoom = params.getInt(CameraParameters::KEY_ZOOM);
            if( ( zoom >= 0 ) && ( zoom < ZOOM_STAGES ) )
                {
                mTargetZoomIdx = zoom;

                //Immediate zoom should be applied instantly ( CTS requirement )
                mCurrentZoomIdx = mTargetZoomIdx;
                doZoom(mCurrentZoomIdx);

                CAMHAL_LOGDB("Zoom by App %d", zoom);
                }
            }
        }

    double gpsPos;

    if( ( valstr = params.get(CameraParameters::KEY_GPS_LATITUDE) ) != NULL )
        {
        gpsPos = strtod(valstr, NULL);

        if ( convertGPSCoord( gpsPos, &mEXIFData.mGPSData.mLatDeg,
                                        &mEXIFData.mGPSData.mLatMin,
                                        &mEXIFData.mGPSData.mLatSec ) == NO_ERROR )
            {

            if ( 0 < gpsPos )
                {
                strncpy(mEXIFData.mGPSData.mLatRef, GPS_NORTH_REF, GPS_REF_SIZE);
                }
            else
                {
                strncpy(mEXIFData.mGPSData.mLatRef, GPS_SOUTH_REF, GPS_REF_SIZE);
                }


            mEXIFData.mGPSData.mLatValid = true;
            }
        else
            {
            mEXIFData.mGPSData.mLatValid = false;
            }
        }
    else
        {
        mEXIFData.mGPSData.mLatValid = false;
        }

    if( ( valstr = params.get(CameraParameters::KEY_GPS_LONGITUDE) ) != NULL )
        {
        gpsPos = strtod(valstr, NULL);

        if ( convertGPSCoord( gpsPos, &mEXIFData.mGPSData.mLongDeg,
                                        &mEXIFData.mGPSData.mLongMin,
                                        &mEXIFData.mGPSData.mLongSec ) == NO_ERROR )
            {

            if ( 0 < gpsPos )
                {
                strncpy(mEXIFData.mGPSData.mLongRef, GPS_EAST_REF, GPS_REF_SIZE);
                }
            else
                {
                strncpy(mEXIFData.mGPSData.mLongRef, GPS_WEST_REF, GPS_REF_SIZE);
                }

            mEXIFData.mGPSData.mLongValid= true;
            }
        else
            {
            mEXIFData.mGPSData.mLongValid = false;
            }
        }
    else
        {
        mEXIFData.mGPSData.mLongValid = false;
        }

    if( ( valstr = params.get(CameraParameters::KEY_GPS_ALTITUDE) ) != NULL )
        {
        gpsPos = strtod(valstr, NULL);
        mEXIFData.mGPSData.mAltitude = gpsPos;
        mEXIFData.mGPSData.mAltitudeValid = true;
        }
    else
        {
        mEXIFData.mGPSData.mAltitudeValid= false;
        }

    if( (valstr = params.get(CameraParameters::KEY_GPS_TIMESTAMP)) != NULL )
        {
        long gpsTimestamp = strtol(valstr, NULL, 10);
        struct tm *timeinfo = localtime( ( time_t * ) & (gpsTimestamp) );
        if ( NULL != timeinfo )
            {
            mEXIFData.mGPSData.mTimeStampHour = timeinfo->tm_hour;
            mEXIFData.mGPSData.mTimeStampMin = timeinfo->tm_min;
            mEXIFData.mGPSData.mTimeStampSec = timeinfo->tm_sec;
            mEXIFData.mGPSData.mTimeStampValid = true;
            }
        else
            {
            mEXIFData.mGPSData.mTimeStampValid = false;
            }
        }
    else
        {
        mEXIFData.mGPSData.mTimeStampValid = false;
        }

    if( ( valstr = params.get(CameraParameters::KEY_GPS_TIMESTAMP) ) != NULL )
        {
        long gpsDatestamp = strtol(valstr, NULL, 10);
        struct tm *timeinfo = localtime( ( time_t * ) & (gpsDatestamp) );
        if ( NULL != timeinfo )
            {
            strftime(mEXIFData.mGPSData.mDatestamp, GPS_DATESTAMP_SIZE, "%Y:%m:%d", timeinfo);
            mEXIFData.mGPSData.mDatestampValid = true;
            }
        else
            {
            mEXIFData.mGPSData.mDatestampValid = false;
            }
        }
    else
        {
        mEXIFData.mGPSData.mDatestampValid = false;
        }

    if( ( valstr = params.get(CameraParameters::KEY_GPS_PROCESSING_METHOD) ) != NULL )
        {
        strncpy(mEXIFData.mGPSData.mProcMethod, valstr, GPS_PROCESSING_SIZE-1);
        mEXIFData.mGPSData.mProcMethodValid = true;
        }
    else
        {
        mEXIFData.mGPSData.mProcMethodValid = false;
        }

    if( ( valstr = params.get(TICameraParameters::KEY_GPS_ALTITUDE_REF) ) != NULL )
        {
        strncpy(mEXIFData.mGPSData.mAltitudeRef, valstr, GPS_REF_SIZE-1);
        mEXIFData.mGPSData.mAltitudeValid = true;
        }
    else
        {
        mEXIFData.mGPSData.mAltitudeValid = false;
        }

    if( ( valstr = params.get(TICameraParameters::KEY_GPS_MAPDATUM) ) != NULL )
        {
        strncpy(mEXIFData.mGPSData.mMapDatum, valstr, GPS_MAPDATUM_SIZE-1);
        mEXIFData.mGPSData.mMapDatumValid = true;
        }
    else
        {
        mEXIFData.mGPSData.mMapDatumValid = false;
        }

    if( ( valstr = params.get(TICameraParameters::KEY_GPS_VERSION) ) != NULL )
        {
        strncpy(mEXIFData.mGPSData.mVersionId, valstr, GPS_VERSION_SIZE-1);
        mEXIFData.mGPSData.mVersionIdValid = true;
        }
    else
        {
        mEXIFData.mGPSData.mVersionIdValid = false;
        }

    if( ( valstr = params.get(TICameraParameters::KEY_EXIF_MODEL ) ) != NULL )
        {
        CAMHAL_LOGEB("EXIF Model: %s", valstr);
        mEXIFData.mModelValid= true;
        }
    else
        {
        mEXIFData.mModelValid= false;
        }

    if( ( valstr = params.get(TICameraParameters::KEY_EXIF_MAKE ) ) != NULL )
        {
        CAMHAL_LOGEB("EXIF Make: %s", valstr);
        mEXIFData.mMakeValid = true;
        }
    else
        {
        mEXIFData.mMakeValid= false;
        }

    mParams = params;
    mFirstTimeInit = false;

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

//Only Geo-tagging is currently supported
status_t OMXCameraAdapter::setupEXIF()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_CONFIG_SHAREDBUFFER sharedBuffer;
    OMX_TI_CONFIG_EXIF_TAGS *exifTags;
    unsigned char *sharedPtr = NULL;
    struct timeval sTv;
    struct tm *pTime;
    OMXCameraPortParameters * capData = NULL;

    LOG_FUNCTION_NAME

    sharedBuffer.pSharedBuff = NULL;
    capData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&sharedBuffer, OMX_TI_CONFIG_SHAREDBUFFER);
        sharedBuffer.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        //We allocate the shared buffer dynamically based on the
        //requirements of the EXIF tags. The additional buffers will
        //get stored after the EXIF configuration structure and the pointers
        //will contain offsets within the shared buffer itself.
        sharedBuffer.nSharedBuffSize = sizeof(OMX_TI_CONFIG_EXIF_TAGS) +
                                                       ( EXIF_MODEL_SIZE ) +
                                                       ( EXIF_MAKE_SIZE ) +
                                                       ( EXIF_DATE_TIME_SIZE ) +
                                                       ( GPS_MAPDATUM_SIZE ) +
                                                       ( GPS_PROCESSING_SIZE );

        sharedBuffer.pSharedBuff =  ( OMX_U8 * ) malloc (sharedBuffer.nSharedBuffSize);
        if ( NULL == sharedBuffer.pSharedBuff )
            {
            CAMHAL_LOGEA("No resources to allocate OMX shared buffer");
            ret = -1;
            }

        //Extra data begins right after the EXIF configuration structure.
        sharedPtr = sharedBuffer.pSharedBuff + sizeof(OMX_TI_CONFIG_EXIF_TAGS);
        }

    if ( NO_ERROR == ret )
        {
        exifTags = ( OMX_TI_CONFIG_EXIF_TAGS * ) sharedBuffer.pSharedBuff;
        OMX_INIT_STRUCT_PTR (exifTags, OMX_TI_CONFIG_EXIF_TAGS);
        exifTags->nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        eError = OMX_GetConfig( mCameraAdapterParameters.mHandleComp,
                                                ( OMX_INDEXTYPE ) OMX_TI_IndexConfigExifTags,
                                                &sharedBuffer );
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while retrieving EXIF configuration structure 0x%x", eError);
            ret = -1;
            }
        }

    if ( NO_ERROR == ret )
        {

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusModel ) &&
              ( mEXIFData.mModelValid ) )
            {
            strncpy( ( char * ) sharedPtr,
                         ( char * ) mParams.get(TICameraParameters::KEY_EXIF_MODEL ),
                         EXIF_MODEL_SIZE - 1);

            exifTags->pModelBuff = ( OMX_S8 * ) ( sharedPtr - sharedBuffer.pSharedBuff );
            sharedPtr += EXIF_MODEL_SIZE;
            exifTags->ulModelBuffSizeBytes = EXIF_MODEL_SIZE;
            exifTags->eStatusModel = OMX_TI_TagUpdated;
            }

         if ( ( OMX_TI_TagReadWrite == exifTags->eStatusMake) &&
               ( mEXIFData.mMakeValid ) )
             {
             strncpy( ( char * ) sharedPtr,
                          ( char * ) mParams.get(TICameraParameters::KEY_EXIF_MAKE ),
                          EXIF_MAKE_SIZE - 1);

             exifTags->pMakeBuff = ( OMX_S8 * ) ( sharedPtr - sharedBuffer.pSharedBuff );
             sharedPtr += EXIF_MAKE_SIZE;
             exifTags->ulMakeBuffSizeBytes = EXIF_MAKE_SIZE;
             exifTags->eStatusMake = OMX_TI_TagUpdated;
             }

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusFocalLength ))
        {
                char *ctx;
                int len;
                char* temp = (char*) mParams.get(CameraParameters::KEY_FOCAL_LENGTH);
                char * tempVal = NULL;
                if(temp != NULL)
                {
                    len = strlen(temp);
                    tempVal = (char*) malloc( sizeof(char) * (len + 1));
                }
                if(tempVal != NULL)
                {
                    memset(tempVal, '\0', len + 1);
                    strncpy(tempVal, temp, len);
                    CAMHAL_LOGDB("KEY_FOCAL_LENGTH = %s", tempVal);

                    // convert the decimal string into a rational
                    size_t den_len;
                    OMX_U32 numerator = 0;
                    OMX_U32 denominator = 0;
                    char* temp = strtok_r(tempVal, ".", &ctx);

                    if(temp != NULL)
                        numerator = atoi(temp);

                    temp = strtok_r(NULL, ".", &ctx);
                    if(temp != NULL)
                    {
                        den_len = strlen(temp);
                        if(HUGE_VAL == den_len )
                            {
                            den_len = 0;
                            }
                        denominator = static_cast<OMX_U32>(pow(10, den_len));
                        numerator = numerator*denominator + atoi(temp);
                    }else{
                        denominator = 1;
                    }

                    free(tempVal);

                    exifTags->ulFocalLength[0] = numerator;
                    exifTags->ulFocalLength[1] = denominator;
                    CAMHAL_LOGVB("exifTags->ulFocalLength = [%u] [%u]", (unsigned int)(exifTags->ulFocalLength[0]), (unsigned int)(exifTags->ulFocalLength[1]));
                    exifTags->eStatusFocalLength = OMX_TI_TagUpdated;
                }
        }

         if ( OMX_TI_TagReadWrite == exifTags->eStatusDateTime )
             {
             int status = gettimeofday (&sTv, NULL);
             pTime = localtime (&sTv.tv_sec);
             if ( ( 0 == status ) && ( NULL != pTime ) )
                {
                snprintf( ( char * ) sharedPtr, EXIF_DATE_TIME_SIZE, "%04d:%02d:%02d %02d:%02d:%02d",
                  pTime->tm_year + 1900,
                  pTime->tm_mon + 1,
                  pTime->tm_mday,
                  pTime->tm_hour,
                  pTime->tm_min,
                  pTime->tm_sec );
                }

             exifTags->pDateTimeBuff = ( OMX_S8 * ) ( sharedPtr - sharedBuffer.pSharedBuff );
             sharedPtr += EXIF_DATE_TIME_SIZE;
             exifTags->ulDateTimeBuffSizeBytes = EXIF_DATE_TIME_SIZE;
             exifTags->eStatusDateTime = OMX_TI_TagUpdated;
             }

         if ( OMX_TI_TagReadWrite == exifTags->eStatusImageWidth )
             {
             exifTags->ulImageWidth = capData->mWidth;
             exifTags->eStatusImageWidth = OMX_TI_TagUpdated;
             }

         if ( OMX_TI_TagReadWrite == exifTags->eStatusImageHeight )
             {
             exifTags->ulImageHeight = capData->mHeight;
             exifTags->eStatusImageHeight = OMX_TI_TagUpdated;
             }

         if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpsLatitude ) &&
              ( mEXIFData.mGPSData.mLatValid ) )
            {
            exifTags->ulGpsLatitude[0] = abs(mEXIFData.mGPSData.mLatDeg);
            exifTags->ulGpsLatitude[2] = abs(mEXIFData.mGPSData.mLatMin);
            exifTags->ulGpsLatitude[4] = abs(mEXIFData.mGPSData.mLatSec);
            exifTags->ulGpsLatitude[1] = 1;
            exifTags->ulGpsLatitude[3] = 1;
            exifTags->ulGpsLatitude[5] = 1;
            exifTags->eStatusGpsLatitude = OMX_TI_TagUpdated;
            }

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpslatitudeRef ) &&
             ( mEXIFData.mGPSData.mLatValid ) )
            {
            exifTags->cGpslatitudeRef[0] = ( OMX_S8 ) mEXIFData.mGPSData.mLatRef[0];
            exifTags->cGpslatitudeRef[1] = '\0';
            exifTags->eStatusGpslatitudeRef = OMX_TI_TagUpdated;
            }

         if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpsLongitude ) &&
              ( mEXIFData.mGPSData.mLongValid ) )
            {
            exifTags->ulGpsLongitude[0] = abs(mEXIFData.mGPSData.mLongDeg);
            exifTags->ulGpsLongitude[2] = abs(mEXIFData.mGPSData.mLongMin);
            exifTags->ulGpsLongitude[4] = abs(mEXIFData.mGPSData.mLongSec);
            exifTags->ulGpsLongitude[1] = 1;
            exifTags->ulGpsLongitude[3] = 1;
            exifTags->ulGpsLongitude[5] = 1;
            exifTags->eStatusGpsLongitude = OMX_TI_TagUpdated;
            }

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpsLongitudeRef ) &&
             ( mEXIFData.mGPSData.mLongValid ) )
            {
            exifTags->cGpsLongitudeRef[0] = ( OMX_S8 ) mEXIFData.mGPSData.mLongRef[0];
            exifTags->cGpsLongitudeRef[1] = '\0';
            exifTags->eStatusGpsLongitudeRef = OMX_TI_TagUpdated;
            }

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpsAltitude ) &&
             ( mEXIFData.mGPSData.mAltitudeValid) )
            {
            exifTags->ulGpsAltitude[0] = ( OMX_U32 ) mEXIFData.mGPSData.mAltitude;
            exifTags->eStatusGpsAltitude = OMX_TI_TagUpdated;
            }

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpsMapDatum ) &&
             ( mEXIFData.mGPSData.mMapDatumValid ) )
            {
            memcpy(sharedPtr, mEXIFData.mGPSData.mMapDatum, GPS_MAPDATUM_SIZE);

            exifTags->pGpsMapDatumBuff = ( OMX_S8 * ) ( sharedPtr - sharedBuffer.pSharedBuff );
            exifTags->ulGpsMapDatumBuffSizeBytes = GPS_MAPDATUM_SIZE;
            exifTags->eStatusGpsMapDatum = OMX_TI_TagUpdated;
            sharedPtr += GPS_MAPDATUM_SIZE;
            }

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpsProcessingMethod ) &&
             ( mEXIFData.mGPSData.mProcMethodValid ) )
            {
            exifTags->pGpsProcessingMethodBuff = ( OMX_S8 * ) ( sharedPtr - sharedBuffer.pSharedBuff );
            memcpy(sharedPtr, EXIFASCIIPrefix, sizeof(EXIFASCIIPrefix));
            sharedPtr += sizeof(EXIFASCIIPrefix);

            memcpy(sharedPtr, mEXIFData.mGPSData.mProcMethod, ( GPS_PROCESSING_SIZE - sizeof(EXIFASCIIPrefix) ) );
            exifTags->ulGpsProcessingMethodBuffSizeBytes = GPS_PROCESSING_SIZE;
            exifTags->eStatusGpsProcessingMethod = OMX_TI_TagUpdated;
            sharedPtr += GPS_PROCESSING_SIZE;
            }

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpsVersionId ) &&
             ( mEXIFData.mGPSData.mVersionIdValid ) )
            {
            exifTags->ucGpsVersionId[0] = ( OMX_U8 ) mEXIFData.mGPSData.mVersionId[0];
            exifTags->ucGpsVersionId[1] =  ( OMX_U8 ) mEXIFData.mGPSData.mVersionId[1];
            exifTags->ucGpsVersionId[2] = ( OMX_U8 ) mEXIFData.mGPSData.mVersionId[2];
            exifTags->ucGpsVersionId[3] = ( OMX_U8 ) mEXIFData.mGPSData.mVersionId[3];
            exifTags->eStatusGpsVersionId = OMX_TI_TagUpdated;
            }

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpsTimeStamp ) &&
             ( mEXIFData.mGPSData.mTimeStampValid ) )
            {
            exifTags->ulGpsTimeStamp[0] = mEXIFData.mGPSData.mTimeStampHour;
            exifTags->ulGpsTimeStamp[2] = mEXIFData.mGPSData.mTimeStampMin;
            exifTags->ulGpsTimeStamp[4] = mEXIFData.mGPSData.mTimeStampSec;
            exifTags->ulGpsTimeStamp[1] = 1;
            exifTags->ulGpsTimeStamp[3] = 1;
            exifTags->ulGpsTimeStamp[5] = 1;
            exifTags->eStatusGpsTimeStamp = OMX_TI_TagUpdated;
            }

        if ( ( OMX_TI_TagReadWrite == exifTags->eStatusGpsDateStamp ) &&
             ( mEXIFData.mGPSData.mDatestampValid ) )
            {
            strncpy( ( char * ) exifTags->cGpsDateStamp,
                         ( char * ) mEXIFData.mGPSData.mDatestamp,
                         GPS_DATESTAMP_SIZE );
            exifTags->eStatusGpsDateStamp = OMX_TI_TagUpdated;
            }

        eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
                                               ( OMX_INDEXTYPE ) OMX_TI_IndexConfigExifTags,
                                               &sharedBuffer );
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while setting EXIF configuration 0x%x", eError);
            ret = -1;
            }
        }

    if ( NULL != sharedBuffer.pSharedBuff )
        {
        free(sharedBuffer.pSharedBuff);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::convertGPSCoord(double coord, int *deg, int *min, int *sec)
{
    double tmp;

    LOG_FUNCTION_NAME

    if ( coord == 0 ) {

        LOGE("Invalid GPS coordinate");

        return -EINVAL;
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

void saveFile(unsigned char   *buff, int width, int height, int format) {
    static int      counter = 1;
    int             fd = -1;
    char            fn[256];

    LOG_FUNCTION_NAME

    fn[0] = 0;
    sprintf(fn, "/preview%03d.yuv", counter);
    fd = open(fn, O_CREAT | O_WRONLY | O_SYNC | O_TRUNC, 0777);
    if(fd < 0) {
        LOGE("Unable to open file %s: %s", fn, strerror(fd));
        return;
    }

    CAMHAL_LOGVB("Copying from 0x%x, size=%d x %d", buff, width, height);

    //method currently supports only nv12 dumping
    int stride = width;
    uint8_t *bf = (uint8_t*) buff;
    for(int i=0;i<height;i++)
        {
        write(fd, bf, width);
        bf += 4096;
        }

    for(int i=0;i<height/2;i++)
        {
        write(fd, bf, stride);
        bf += 4096;
        }

    close(fd);


    counter++;

    LOG_FUNCTION_NAME_EXIT
}

status_t OMXCameraAdapter::updateFocusDistances(CameraParameters &params)
{
    OMX_U32 focusNear, focusOptimal, focusFar;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    ret = getFocusDistances(focusNear, focusOptimal, focusFar);
    if ( NO_ERROR == ret)
        {
        ret = addFocusDistances(focusNear, focusOptimal, focusFar, params);
            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("Error in call to addFocusDistances() 0x%x", ret);
                }
        }
    else
        {
        CAMHAL_LOGEB("Error in call to getFocusDistances() 0x%x", ret);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::getFocusDistances(OMX_U32 &near,OMX_U32 &optimal, OMX_U32 &far)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError;

    OMX_TI_CONFIG_FOCUSDISTANCETYPE focusDist;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = UNKNOWN_ERROR;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR(&focusDist, OMX_TI_CONFIG_FOCUSDISTANCETYPE);
        focusDist.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

        eError = OMX_GetConfig(mCameraAdapterParameters.mHandleComp,
                               ( OMX_INDEXTYPE ) OMX_TI_IndexConfigFocusDistance,
                               &focusDist);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while querying focus distances 0x%x", eError);
            ret = UNKNOWN_ERROR;
            }

        }

    if ( NO_ERROR == ret )
        {
        near = focusDist.nFocusDistanceNear;
        optimal = focusDist.nFocusDistanceOptimal;
        far = focusDist.nFocusDistanceFar;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::encodeFocusDistance(OMX_U32 dist, char *buffer, size_t length)
{
    status_t ret = NO_ERROR;
    uint32_t focusScale = 1000;
    float distFinal;

    LOG_FUNCTION_NAME

    if(mParameters3A.Focus == OMX_IMAGE_FocusControlAutoInfinity)
        {
        dist=0;
        }

    if ( NO_ERROR == ret )
        {
        if ( 0 == dist )
            {
            strncpy(buffer, CameraParameters::FOCUS_DISTANCE_INFINITY, ( length - 1 ));
            }
        else
            {
            distFinal = dist;
            distFinal /= focusScale;
            snprintf(buffer, ( length - 1 ) , "%5.3f", distFinal);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::addFocusDistances(OMX_U32 &near,
                                             OMX_U32 &optimal,
                                             OMX_U32 &far,
                                             CameraParameters& params)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        ret = encodeFocusDistance(near, mFocusDistNear, FOCUS_DIST_SIZE);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error encoding near focus distance 0x%x", ret);
            }
        }

    if ( NO_ERROR == ret )
        {
        ret = encodeFocusDistance(optimal, mFocusDistOptimal, FOCUS_DIST_SIZE);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error encoding near focus distance 0x%x", ret);
            }
        }

    if ( NO_ERROR == ret )
        {
        ret = encodeFocusDistance(far, mFocusDistFar, FOCUS_DIST_SIZE);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error encoding near focus distance 0x%x", ret);
            }
        }

    if ( NO_ERROR == ret )
        {
        snprintf(mFocusDistBuffer, ( FOCUS_DIST_BUFFER_SIZE - 1) ,"%s,%s,%s", mFocusDistNear,
                                                                              mFocusDistOptimal,
                                                                              mFocusDistFar);

        params.set(CameraParameters::KEY_FOCUS_DISTANCES, mFocusDistBuffer);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

void OMXCameraAdapter::getParameters(CameraParameters& params)
{
    OMX_CONFIG_EXPOSUREVALUETYPE exp;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    LOG_FUNCTION_NAME

#ifdef PARAM_FEEDBACK

    OMX_CONFIG_WHITEBALCONTROLTYPE wb;
    OMX_CONFIG_FLICKERCANCELTYPE flicker;
    OMX_CONFIG_SCENEMODETYPE scene;
    OMX_IMAGE_PARAM_FLASHCONTROLTYPE flash;
    OMX_CONFIG_BRIGHTNESSTYPE brightness;
    OMX_CONFIG_CONTRASTTYPE contrast;
    OMX_IMAGE_CONFIG_PROCESSINGLEVELTYPE procSharpness;
    OMX_CONFIG_SATURATIONTYPE saturation;
    OMX_CONFIG_IMAGEFILTERTYPE effect;
    OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE focus;

    exp.nSize = sizeof(OMX_CONFIG_EXPOSURECONTROLTYPE);
    exp.nVersion = mLocalVersionParam;
    exp.nPortIndex = OMX_ALL;

    expValues.nSize = sizeof(OMX_CONFIG_EXPOSUREVALUETYPE);
    expValues.nVersion = mLocalVersionParam;
    expValues.nPortIndex = OMX_ALL;

    wb.nSize = sizeof(OMX_CONFIG_WHITEBALCONTROLTYPE);
    wb.nVersion = mLocalVersionParam;
    wb.nPortIndex = OMX_ALL;

    flicker.nSize = sizeof(OMX_CONFIG_FLICKERCANCELTYPE);
    flicker.nVersion = mLocalVersionParam;
    flicker.nPortIndex = OMX_ALL;

    scene.nSize = sizeof(OMX_CONFIG_SCENEMODETYPE);
    scene.nVersion = mLocalVersionParam;
    scene.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

    flash.nSize = sizeof(OMX_IMAGE_PARAM_FLASHCONTROLTYPE);
    flash.nVersion = mLocalVersionParam;
    flash.nPortIndex = OMX_ALL;


    brightness.nSize = sizeof(OMX_CONFIG_BRIGHTNESSTYPE);
    brightness.nVersion = mLocalVersionParam;
    brightness.nPortIndex = OMX_ALL;

    contrast.nSize = sizeof(OMX_CONFIG_CONTRASTTYPE);
    contrast.nVersion = mLocalVersionParam;
    contrast.nPortIndex = OMX_ALL;

    procSharpness.nSize = sizeof( OMX_IMAGE_CONFIG_PROCESSINGLEVELTYPE );
    procSharpness.nVersion = mLocalVersionParam;
    procSharpness.nPortIndex = OMX_ALL;

    saturation.nSize = sizeof(OMX_CONFIG_SATURATIONTYPE);
    saturation.nVersion = mLocalVersionParam;
    saturation.nPortIndex = OMX_ALL;

    effect.nSize = sizeof(OMX_CONFIG_IMAGEFILTERTYPE);
    effect.nVersion = mLocalVersionParam;
    effect.nPortIndex = OMX_ALL;

    focus.nSize = sizeof(OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE);
    focus.nVersion = mLocalVersionParam;
    focus.nPortIndex = OMX_ALL;

    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposure, &exp);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonWhiteBalance, &wb);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexConfigFlickerCancel, &flicker );
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexParamSceneMode, &scene);
    OMX_GetParameter( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexParamFlashControl, &flash);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonBrightness, &brightness);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonContrast, &contrast);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexConfigSharpeningLevel, &procSharpness);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonSaturation, &saturation);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonImageFilter, &effect);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigFocusControl, &focus);

    char * str = NULL;

    for(int i = 0; i < ExpLUT.size; i++)
        if( ExpLUT.Table[i].omxDefinition == exp.eExposureControl )
            str = (char*)ExpLUT.Table[i].userDefinition;
    params.set( TICameraParameters::KEY_EXPOSURE_MODE , str);

    for(int i = 0; i < WBalLUT.size; i++)
        if( WBalLUT.Table[i].omxDefinition == wb.eWhiteBalControl )
            str = (char*)WBalLUT.Table[i].userDefinition;
    params.set( CameraParameters::KEY_WHITE_BALANCE , str );

    for(int i = 0; i < FlickerLUT.size; i++)
        if( FlickerLUT.Table[i].omxDefinition == flicker.eFlickerCancel )
            str = (char*)FlickerLUT.Table[i].userDefinition;
    params.set( CameraParameters::KEY_ANTIBANDING , str );

    for(int i = 0; i < SceneLUT.size; i++)
        if( SceneLUT.Table[i].omxDefinition == scene.eSceneMode )
            str = (char*)SceneLUT.Table[i].userDefinition;
    params.set( CameraParameters::KEY_SCENE_MODE , str );

    for(int i = 0; i < FlashLUT.size; i++)
        if( FlashLUT.Table[i].omxDefinition == flash.eFlashControl )
            str = (char*)FlashLUT.Table[i].userDefinition;
    params.set( CameraParameters::KEY_FLASH_MODE, str );

    for(int i = 0; i < EffLUT.size; i++)
        if( EffLUT.Table[i].omxDefinition == effect.eImageFilter )
            str = (char*)EffLUT.Table[i].userDefinition;
    params.set( CameraParameters::KEY_EFFECT , str );

    for(int i = 0; i < FocusLUT.size; i++)
        if( FocusLUT.Table[i].omxDefinition == focus.eFocusControl )
            str = (char*)FocusLUT.Table[i].userDefinition;

    params.set( CameraParameters::KEY_FOCUS_MODE , str );

    int comp = ((expValues.xEVCompensation * 10) >> Q16_OFFSET);

    params.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, comp );
    params.set( TICameraParameters::KEY_MANUAL_EXPOSURE, expValues.nShutterSpeedMsec);
    params.set( TICameraParameters::KEY_BRIGHTNESS, brightness.nBrightness);
    params.set( TICameraParameters::KEY_CONTRAST, contrast.nContrast );
    params.set( TICameraParameters::KEY_SHARPNESS, procSharpness.nLevel);
    params.set( TICameraParameters::KEY_SATURATION, saturation.nSaturation);

#else

    //Query focus distances only during CAF, Infinity
    //or when focus is running
    if ( mFocusStarted || ( mParameters3A.Focus == OMX_IMAGE_FocusControlAuto )  ||
         ( mParameters3A.Focus == OMX_IMAGE_FocusControlAutoInfinity ) ||
         ( NULL == mParameters.get(CameraParameters::KEY_FOCUS_DISTANCES) ) )
        {
        updateFocusDistances(params);
        }
    else
        {
        params.set(CameraParameters::KEY_FOCUS_DISTANCES,
                   mParameters.get(CameraParameters::KEY_FOCUS_DISTANCES));
        }

    OMX_INIT_STRUCT_PTR (&exp, OMX_CONFIG_EXPOSUREVALUETYPE);
    exp.nPortIndex = OMX_ALL;

    eError = OMX_GetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &exp);
    if ( OMX_ErrorNone == eError )
        {
        params.set(TICameraParameters::KEY_CURRENT_ISO, exp.nSensitivity);
        }
    else
        {
        CAMHAL_LOGEB("OMX error 0x%x, while retrieving current ISO value", eError);
        }

        {
        Mutex::Autolock lock(mZoomLock);
        if ( ( mSmoothZoomEnabled ) )
            {
            if ( mZoomParameterIdx != mCurrentZoomIdx )
                {
                mZoomParameterIdx += mZoomInc;
                }

            params.set( CameraParameters::KEY_ZOOM, mZoomParameterIdx);
            if ( ( mCurrentZoomIdx == mTargetZoomIdx ) && ( mZoomParameterIdx == mCurrentZoomIdx ) )
                {
                mSmoothZoomEnabled = false;
                }
            CAMHAL_LOGDB("CameraParameters Zoom = %d , mSmoothZoomEnabled = %d", mCurrentZoomIdx, mSmoothZoomEnabled);
            }
        else
            {
            params.set( CameraParameters::KEY_ZOOM, mCurrentZoomIdx);
            }
        }

        //Face detection coordinates go in here
        {
        Mutex::Autolock lock(mFaceDetectionLock);
        if ( mFaceDetectionRunning )
            {
            params.set( TICameraParameters::KEY_FACE_DETECTION_DATA, mFaceDectionResult);
            }
        else
            {
            params.set( TICameraParameters::KEY_FACE_DETECTION_DATA, NULL);
            }
        }

#endif

    LOG_FUNCTION_NAME_EXIT
}


status_t OMXCameraAdapter::setVFramerate(OMX_U32 minFrameRate, OMX_U32 maxFrameRate)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_CONFIG_VARFRMRANGETYPE vfr;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&vfr, OMX_TI_CONFIG_VARFRMRANGETYPE);

        vfr.xMin = minFrameRate<<16;
        vfr.xMax = maxFrameRate<<16;

        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_TI_IndexConfigVarFrmRange, &vfr);
        if(OMX_ErrorNone != eError)
            {
            CAMHAL_LOGEB("Error while setting VFR min = %d, max = %d, error = 0x%x",
                         ( unsigned int ) minFrameRate,
                         ( unsigned int ) maxFrameRate,
                         eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGEB("VFR Configured Successfully [%d:%d]",
                        ( unsigned int ) minFrameRate,
                        ( unsigned int ) maxFrameRate);
            }
        }

    return ret;
 }

status_t OMXCameraAdapter::setFormat(OMX_U32 port, OMXCameraPortParameters &portParams)
{
    status_t ret = NO_ERROR;
    size_t bufferCount;

    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE portCheck;

    OMX_INIT_STRUCT_PTR (&portCheck, OMX_PARAM_PORTDEFINITIONTYPE);

    portCheck.nPortIndex = port;

    eError = OMX_GetParameter (mCameraAdapterParameters.mHandleComp,
                                OMX_IndexParamPortDefinition, &portCheck);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_GetParameter - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    if ( OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW == port )
        {
        portCheck.format.video.nFrameWidth      = portParams.mWidth;
        portCheck.format.video.nFrameHeight     = portParams.mHeight;
        portCheck.format.video.eColorFormat     = portParams.mColorFormat;
        portCheck.format.video.nStride          = portParams.mStride;
        if( ( portCheck.format.video.nFrameWidth >= 1920 ) &&
            ( portCheck.format.video.nFrameHeight >= 1080 ) &&
            ( portParams.mFrameRate >= FRAME_RATE_FULL_HD ) )
            {
            setSensorOverclock(true);
            }
        else
            {
            setSensorOverclock(false);
            }

        portCheck.format.video.xFramerate       = portParams.mFrameRate<<16;
        portCheck.nBufferSize                   = portParams.mStride * portParams.mHeight;
        portCheck.nBufferCountActual = portParams.mNumBufs;
        mFocusThreshold = FOCUS_THRESHOLD * portParams.mFrameRate;
        }
    else if ( OMX_CAMERA_PORT_IMAGE_OUT_IMAGE == port )
        {
        portCheck.format.image.nFrameWidth      = portParams.mWidth;
        portCheck.format.image.nFrameHeight     = portParams.mHeight;
        if ( OMX_COLOR_FormatUnused == portParams.mColorFormat && mCodingMode == CodingNone )
            {
            portCheck.format.image.eColorFormat     = OMX_COLOR_FormatCbYCrY;
            portCheck.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
            }
        else if ( OMX_COLOR_FormatUnused == portParams.mColorFormat && mCodingMode == CodingJPS )
            {
            portCheck.format.image.eColorFormat       = OMX_COLOR_FormatCbYCrY;
            portCheck.format.image.eCompressionFormat = (OMX_IMAGE_CODINGTYPE) OMX_TI_IMAGE_CodingJPS;
            }
        else if ( OMX_COLOR_FormatUnused == portParams.mColorFormat && mCodingMode == CodingMPO )
            {
            portCheck.format.image.eColorFormat       = OMX_COLOR_FormatCbYCrY;
            portCheck.format.image.eCompressionFormat = (OMX_IMAGE_CODINGTYPE) OMX_TI_IMAGE_CodingMPO;
            }
        else if ( OMX_COLOR_FormatUnused == portParams.mColorFormat && mCodingMode == CodingRAWJPEG )
            {
            //TODO: OMX_IMAGE_CodingJPEG should be changed to OMX_IMAGE_CodingRAWJPEG when
            // RAW format is supported
            portCheck.format.image.eColorFormat       = OMX_COLOR_FormatCbYCrY;
            portCheck.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
            }
        else if ( OMX_COLOR_FormatUnused == portParams.mColorFormat && mCodingMode == CodingRAWMPO )
            {
            //TODO: OMX_IMAGE_CodingJPEG should be changed to OMX_IMAGE_CodingRAWMPO when
            // RAW format is supported
            portCheck.format.image.eColorFormat       = OMX_COLOR_FormatCbYCrY;
            portCheck.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
            }
        else
            {
            portCheck.format.image.eColorFormat     = portParams.mColorFormat;
            portCheck.format.image.eCompressionFormat = OMX_IMAGE_CodingUnused;
            }

        portCheck.format.image.nStride          = portParams.mStride * portParams.mWidth;
        portCheck.nBufferSize                   =  portParams.mStride * portParams.mWidth * portParams.mHeight;
        portCheck.nBufferCountActual = portParams.mNumBufs;
        }
    else
        {
        CAMHAL_LOGEB("Unsupported port index 0x%x", (unsigned int)port);
        }

    if ( mSensorIndex == OMX_TI_StereoSensor &&
        (OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW == port || OMX_CAMERA_PORT_IMAGE_OUT_IMAGE == port) )
        {
        ret = setS3DFrameLayout(port);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEA("Error configuring stereo 3D frame layout");
            return ret;
            }
        }

    eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp,
                            OMX_IndexParamPortDefinition, &portCheck);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SetParameter - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    /* check if parameters are set correctly by calling GetParameter() */
    eError = OMX_GetParameter(mCameraAdapterParameters.mHandleComp,
                                        OMX_IndexParamPortDefinition, &portCheck);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_GetParameter - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    portParams.mBufSize = portCheck.nBufferSize;

    if ( OMX_CAMERA_PORT_IMAGE_OUT_IMAGE == port )
        {
        LOGD("\n *** IMG Width = %ld", portCheck.format.image.nFrameWidth);
        LOGD("\n *** IMG Height = %ld", portCheck.format.image.nFrameHeight);

        LOGD("\n *** IMG IMG FMT = %x", portCheck.format.image.eColorFormat);
        LOGD("\n *** IMG portCheck.nBufferSize = %ld\n",portCheck.nBufferSize);
        LOGD("\n *** IMG portCheck.nBufferCountMin = %ld\n",
                                                portCheck.nBufferCountMin);
        LOGD("\n *** IMG portCheck.nBufferCountActual = %ld\n",
                                                portCheck.nBufferCountActual);
        LOGD("\n *** IMG portCheck.format.image.nStride = %ld\n",
                                                portCheck.format.image.nStride);
        }
    else
        {
        LOGD("\n *** PRV Width = %ld", portCheck.format.video.nFrameWidth);
        LOGD("\n *** PRV Height = %ld", portCheck.format.video.nFrameHeight);

        LOGD("\n *** PRV IMG FMT = %x", portCheck.format.video.eColorFormat);
        LOGD("\n *** PRV portCheck.nBufferSize = %ld\n",portCheck.nBufferSize);
        LOGD("\n *** PRV portCheck.nBufferCountMin = %ld\n",
                                                portCheck.nBufferCountMin);
        LOGD("\n *** PRV portCheck.nBufferCountActual = %ld\n",
                                                portCheck.nBufferCountActual);
        LOGD("\n *** PRV portCheck.format.video.nStride = %ld\n",
                                                portCheck.format.video.nStride);
        }

    LOG_FUNCTION_NAME_EXIT

    return ErrorUtils::omxToAndroidError(eError);

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of eError = %x", __FUNCTION__, eError);

    if ( eError != OMX_ErrorNone )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ErrorUtils::omxToAndroidError(eError);
}


status_t OMXCameraAdapter::enableVideoStabilization(bool enable)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_FRAMESTABTYPE frameStabCfg;


    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        OMX_CONFIG_BOOLEANTYPE vstabp;
        OMX_INIT_STRUCT_PTR (&vstabp, OMX_CONFIG_BOOLEANTYPE);
        if(enable)
            {
            vstabp.bEnabled = OMX_TRUE;
            }
        else
            {
            vstabp.bEnabled = OMX_FALSE;
            }

        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexParamFrameStabilisation, &vstabp);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring video stabilization param 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDA("Video stabilization param configured successfully");
            }

        }

    if ( NO_ERROR == ret )
        {

        OMX_INIT_STRUCT_PTR (&frameStabCfg, OMX_CONFIG_FRAMESTABTYPE);


        eError =  OMX_GetConfig(mCameraAdapterParameters.mHandleComp,
                                            ( OMX_INDEXTYPE ) OMX_IndexConfigCommonFrameStabilisation, &frameStabCfg);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while getting video stabilization mode 0x%x", (unsigned int)eError);
            ret = -1;
            }

        CAMHAL_LOGDB("VSTAB Port Index = %d", (int)frameStabCfg.nPortIndex);

        frameStabCfg.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
        if ( enable )
            {
            CAMHAL_LOGDA("VSTAB is enabled");
            frameStabCfg.bStab = OMX_TRUE;
            }
        else
            {
            CAMHAL_LOGDA("VSTAB is disabled");
            frameStabCfg.bStab = OMX_FALSE;

            }

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp,
                                            ( OMX_INDEXTYPE ) OMX_IndexConfigCommonFrameStabilisation, &frameStabCfg);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring video stabilization mode 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDA("Video stabilization mode configured successfully");
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}


status_t OMXCameraAdapter::enableVideoNoiseFilter(bool enable)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_VIDEONOISEFILTERTYPE vnfCfg;


    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&vnfCfg, OMX_PARAM_VIDEONOISEFILTERTYPE);

        if ( enable )
            {
            CAMHAL_LOGDA("VNF is enabled");
            vnfCfg.eMode = OMX_VideoNoiseFilterModeOn;
            }
        else
            {
            CAMHAL_LOGDA("VNF is disabled");
            vnfCfg.eMode = OMX_VideoNoiseFilterModeOff;
            }

        eError =  OMX_SetParameter(mCameraAdapterParameters.mHandleComp,
                                            ( OMX_INDEXTYPE ) OMX_IndexParamVideoNoiseFilter, &vnfCfg);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring video noise filter 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDA("Video noise filter is configured successfully");
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;

}


status_t OMXCameraAdapter::flushBuffers()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    TIMM_OSAL_ERRORTYPE err;
    TIMM_OSAL_U32 uRequestedEvents = OMXCameraAdapter::CAMERA_PORT_FLUSH;
    TIMM_OSAL_U32 pRetrievedEvents;

    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    LOG_FUNCTION_NAME


    OMXCameraPortParameters * mPreviewData = NULL;
    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

    if ( !mPreviewing )
        {
        LOG_FUNCTION_NAME_EXIT
        return NO_ERROR;
        }

    ///Register for the FLUSH event
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandFlush,
                                OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW,
                                eventSem,
                                -1///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }

    ///Send FLUSH command to preview port
    eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp,
                              OMX_CommandFlush,
                              mCameraAdapterParameters.mPrevPortIndex,
                              NULL);

    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandFlush)-0x%x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    CAMHAL_LOGDA("Waiting for flush event");

    ///Wait for the FLUSH event to occur
    eventSem.Wait();

    CAMHAL_LOGDA("Flush event received");

    LOG_FUNCTION_NAME_EXIT

    return (ret | ErrorUtils::omxToAndroidError(eError));

    EXIT:
    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);
    LOG_FUNCTION_NAME_EXIT

    if ( eError != OMX_ErrorNone )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    return (ret | ErrorUtils::omxToAndroidError(eError));
}

///API to give the buffers to Adapter
status_t OMXCameraAdapter::useBuffers(CameraMode mode, void* bufArr, int num, size_t length)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    switch(mode)
        {
        case CAMERA_PREVIEW:
            mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex].mNumBufs =  num;
            ret = UseBuffersPreview(bufArr, num);
            break;

        case CAMERA_IMAGE_CAPTURE:
            mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex].mNumBufs = num;
            ret = UseBuffersCapture(bufArr, num);
            break;

        case CAMERA_VIDEO:
            mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex].mNumBufs =  num;
            ret = UseBuffersPreview(bufArr, num);
            break;

        case CAMERA_MEASUREMENT:
            mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mMeasurementPortIndex].mNumBufs = num;
            ret = UseBuffersPreviewData(bufArr, num);
            break;

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::UseBuffersPreviewData(void* bufArr, int num)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMXCameraPortParameters * measurementData = NULL;
    uint32_t *buffers = (uint32_t*) bufArr;
    Semaphore eventSem;
    Mutex::Autolock lock(mPreviewDataBufferLock);

    LOG_FUNCTION_NAME

    if ( mComponentState != OMX_StateLoaded )
        {
        CAMHAL_LOGEA("Calling UseBuffersPreviewData() when not in LOADED state");
        ret = -1;
        }

    if ( NULL == bufArr )
        {
        CAMHAL_LOGEA("NULL pointer passed for bufArr");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        measurementData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mMeasurementPortIndex];
        measurementData->mNumBufs = num;
        ret = eventSem.Create(0);
        }

    if ( NO_ERROR == ret )
        {
         ///Register for port enable event on measurement port
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                      OMX_EventCmdComplete,
                                      OMX_CommandPortEnable,
                                      mCameraAdapterParameters.mMeasurementPortIndex,
                                      eventSem,
                                     -1 ///Infinite timeout
                                      );
        if ( ret != NO_ERROR )
            {
            CAMHAL_LOGEB("Error in registering for event %d", ret);
            }
        }

    if ( NO_ERROR == ret )
        {
         ///Enable MEASUREMENT Port
        eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                      OMX_CommandPortEnable,
                                      mCameraAdapterParameters.mMeasurementPortIndex,
                                      NULL);
        if ( eError != OMX_ErrorNone )
            {
            CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandPortEnable) -0x%x", eError);
            ret = -1;
            }
        }

    if ( NO_ERROR == ret )
        {
        //Wait for the port enable event to occur
        CAMHAL_LOGDA("Waiting for Port enable on Measurement port");
        eventSem.Wait();
        CAMHAL_LOGDA("Port enable event arrived on measurement port");
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::switchToLoaded()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    Semaphore eventSem;

    LOG_FUNCTION_NAME

    if ( mComponentState == OMX_StateLoaded )
        {
        CAMHAL_LOGEA("Already in OMX_Loaded state");
        goto EXIT;
        }

    eventSem.Create(0);
    ///Register for EXECUTING state transition.
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                           OMX_EventCmdComplete,
                           OMX_CommandStateSet,
                           OMX_StateIdle,
                           eventSem,
                           -1);

    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }

    eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp,
                              OMX_CommandStateSet,
                              OMX_StateIdle,
                              NULL);

    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_StateIdle) - %x", eError);
        }

    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    ///Wait for the EXECUTING ->IDLE transition to arrive

    CAMHAL_LOGDA("EXECUTING->IDLE state changed");
    ret = eventSem.Wait();
    CAMHAL_LOGDA("EXECUTING->IDLE state changed");

    ///Register for LOADED state transition.
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                           OMX_EventCmdComplete,
                           OMX_CommandStateSet,
                           OMX_StateLoaded,
                           eventSem,
                           -1);

    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }

    eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp,
                              OMX_CommandStateSet,
                              OMX_StateLoaded,
                              NULL);

    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_StateLoaded) - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    CAMHAL_LOGDA("IDLE->LOADED state changed");
    ret = eventSem.Wait();
    CAMHAL_LOGDA("IDLE->LOADED state changed");


    mComponentState = OMX_StateLoaded;

    ///Register for Preview port ENABLE event
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                           OMX_EventCmdComplete,
                           OMX_CommandPortEnable,
                           mCameraAdapterParameters.mPrevPortIndex,
                           eventSem,
                           -1);

    if ( NO_ERROR != ret )
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }

    ///Enable Preview Port
    eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                             OMX_CommandPortEnable,
                             mCameraAdapterParameters.mPrevPortIndex,
                             NULL);

    CAMHAL_LOGDB("OMX_SendCommand(OMX_CommandStateSet) 0x%x", eError);

    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    CAMHAL_LOGDA("Enabling Preview port");
    ///Wait for state to switch to idle
    ret = eventSem.Wait();
    CAMHAL_LOGDA("Preview port enabled!");

    EXIT:

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::UseBuffersPreview(void* bufArr, int num)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int tmpHeight, tmpWidth;

    LOG_FUNCTION_NAME

    ///Flag to determine whether it is 3D camera or not
    bool isS3d = false;
    const char *valstr = NULL;
    if ( (valstr = mParams.get(TICameraParameters::KEY_S3D_SUPPORTED)) != NULL) {
        isS3d = (strcmp(valstr, "true") == 0);
    }

    if(!bufArr)
        {
        CAMHAL_LOGEA("NULL pointer passed for buffArr");
        LOG_FUNCTION_NAME_EXIT
        return BAD_VALUE;
        }

    OMXCameraPortParameters *mPreviewData = NULL;
    OMXCameraPortParameters *measurementData = NULL;
    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
    measurementData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mMeasurementPortIndex];
    mPreviewData->mNumBufs = num;
    uint32_t *buffers = (uint32_t*)bufArr;
    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore 0x%x", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    if ( mPreviewing )
        {
        CAMHAL_LOGEA("Trying to change preview buffers while preview is running!");
        return NO_INIT;
        }
    else if(mPreviewData->mNumBufs != num)
        {
        CAMHAL_LOGEA("Current number of buffers doesnt equal new num of buffers passed!");
        LOG_FUNCTION_NAME_EXIT
        return BAD_VALUE;
        }

    if ( mComponentState == OMX_StateLoaded )
        {

        ret = setLDC(mIPP);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("setLDC() failed %d", ret);
            LOG_FUNCTION_NAME_EXIT
            return ret;
            }

        ret = setNSF(mIPP);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("setNSF() failed %d", ret);
            LOG_FUNCTION_NAME_EXIT
            return ret;
            }

        ret = setCaptureMode(mCapMode);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("setCaptureMode() failed %d", ret);
            LOG_FUNCTION_NAME_EXIT
            return ret;
            }

        CAMHAL_LOGDB("Camera Mode = %d", mCapMode);

        if( ( mCapMode == OMXCameraAdapter::VIDEO_MODE ) ||
            ( isS3d && (mCapMode == OMXCameraAdapter::HIGH_QUALITY)) )
            {
            ///Enable/Disable Video Noise Filter
            ret = enableVideoNoiseFilter(mVnfEnabled);
            if ( NO_ERROR != ret)
                {
                CAMHAL_LOGEB("Error configuring VNF %x", ret);
                return ret;
                }

            ///Enable/Disable Video Stabilization
            ret = enableVideoStabilization(mVstabEnabled);
            if ( NO_ERROR != ret)
                {
                CAMHAL_LOGEB("Error configuring VSTAB %x", ret);
                return ret;
                }
            }
        else
            {
            ret = enableVideoNoiseFilter(false);
            if ( NO_ERROR != ret)
                {
                CAMHAL_LOGEB("Error configuring VNF %x", ret);
                return ret;
                }
            ///Enable/Disable Video Stabilization
            ret = enableVideoStabilization(false);
            if ( NO_ERROR != ret)
                {
                CAMHAL_LOGEB("Error configuring VSTAB %x", ret);
                return ret;
                }
            }
        }

    ret = setSensorOrientation(mSensorOrientation);
    if ( NO_ERROR != ret )
        {
        CAMHAL_LOGEB("Error configuring Sensor Orientation %x", ret);
        mSensorOrientation = 0;
        }

    ret = setVFramerate(mPreviewData->mMinFrameRate, mPreviewData->mMaxFrameRate);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEB("VFR configuration failed 0x%x", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    if ( mComponentState == OMX_StateLoaded )
        {
        ///Register for IDLE state switch event
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                               OMX_EventCmdComplete,
                               OMX_CommandStateSet,
                               OMX_StateIdle,
                               eventSem,
                               -1);

        if(ret!=NO_ERROR)
            {
            CAMHAL_LOGEB("Error in registering for event %d", ret);
            goto EXIT;
            }

        ///Once we get the buffers, move component state to idle state and pass the buffers to OMX comp using UseBuffer
        eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                  OMX_CommandStateSet,
                                  OMX_StateIdle,
                                  NULL);

        CAMHAL_LOGDB("OMX_SendCommand(OMX_CommandStateSet) 0x%x", eError);

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

        mComponentState = OMX_StateIdle;
        }
    else
        {
        ///Register for Preview port ENABLE event
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                               OMX_EventCmdComplete,
                               OMX_CommandPortEnable,
                               mCameraAdapterParameters.mPrevPortIndex,
                               eventSem,
                               -1);

        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error in registering for event %d", ret);
            goto EXIT;
            }

        ///Enable Preview Port
        eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                 OMX_CommandPortEnable,
                                 mCameraAdapterParameters.mPrevPortIndex,
                                 NULL);
        }

    for (int index = 0; index < num; index++)
        {
        OMX_BUFFERHEADERTYPE *pBufferHdr;
        CAMHAL_LOGDB("OMX_UseBuffer(0x%x)", buffers[index]);
        eError = OMX_UseBuffer(mCameraAdapterParameters.mHandleComp,
                                &pBufferHdr,
                                mCameraAdapterParameters.mPrevPortIndex,
                                0,
                                mPreviewData->mBufSize,
                                (OMX_U8*)buffers[index]);
        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_UseBuffer-0x%x", eError);
            }
        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

        //pBufferHdr->pAppPrivate =  (OMX_PTR)pBufferHdr;
        pBufferHdr->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pBufferHdr->nVersion.s.nVersionMajor = 1;
        pBufferHdr->nVersion.s.nVersionMinor = 1;
        pBufferHdr->nVersion.s.nRevision = 0;
        pBufferHdr->nVersion.s.nStep = 0;
        mPreviewData->mBufferHeader[index] = pBufferHdr;
        }

    if ( mMeasurementEnabled )
        {
        for (int i = 0; i < measurementData->mNumBufs; i++)
            {
            OMX_BUFFERHEADERTYPE *pBufHdr;
            eError = OMX_UseBuffer( mCameraAdapterParameters.mHandleComp,
                                    &pBufHdr,
                                    mCameraAdapterParameters.mMeasurementPortIndex,
                                    0,
                                    measurementData->mBufSize,
                                    (OMX_U8*)(mPreviewDataBuffers[i]));

            if ( eError == OMX_ErrorNone )
                {
                pBufHdr->nSize = sizeof(OMX_BUFFERHEADERTYPE);
                pBufHdr->nVersion.s.nVersionMajor = 1;
                pBufHdr->nVersion.s.nVersionMinor = 1;
                pBufHdr->nVersion.s.nRevision = 0;
                pBufHdr->nVersion.s.nStep = 0;
                measurementData->mBufferHeader[i] = pBufHdr;
                }
            else
                {
                CAMHAL_LOGEB("OMX_UseBuffer -0x%x", eError);
                ret = -1;
                break;
                }
            }
        }

    CAMHAL_LOGDA("Registering preview buffers");
    ret = eventSem.Wait();
    CAMHAL_LOGDA("Preview buffer registration successfull");

    LOG_FUNCTION_NAME_EXIT

    return (ret | ErrorUtils::omxToAndroidError(eError));

    ///If there is any failure, we reach here.
    ///Here, we do any resource freeing and convert from OMX error code to Camera Hal error code
    EXIT:
    LOG_FUNCTION_NAME_EXIT
    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);

    if ( eError != OMX_ErrorNone )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    return (ret | ErrorUtils::omxToAndroidError(eError));
}

status_t OMXCameraAdapter::UseBuffersCapture(void* bufArr, int num)
{
    LOG_FUNCTION_NAME

    status_t ret;
    OMX_ERRORTYPE eError;
    OMXCameraPortParameters * imgCaptureData = NULL;
    uint32_t *buffers = (uint32_t*)bufArr;
    Semaphore camSem;
    OMXCameraPortParameters cap;

    imgCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    camSem.Create();

    if ( mCapturing )
        {
        ///Register for Image port Disable event
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                    OMX_EventCmdComplete,
                                    OMX_CommandPortDisable,
                                    mCameraAdapterParameters.mImagePortIndex,
                                    camSem,
                                    -1);

        ///Disable Capture Port
        eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                    OMX_CommandPortDisable,
                                    mCameraAdapterParameters.mImagePortIndex,
                                    NULL);

        CAMHAL_LOGDA("Waiting for port disable");
        //Wait for the image port enable event
        camSem.Wait();
        CAMHAL_LOGDA("Port disabled");
        }

    imgCaptureData->mNumBufs = num;

    //TODO: Support more pixelformats

    LOGD("Params Width = %d", (int)imgCaptureData->mWidth);
    LOGD("Params Height = %d", (int)imgCaptureData->mHeight);

    ret = setFormat(OMX_CAMERA_PORT_IMAGE_OUT_IMAGE, *imgCaptureData);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEB("setFormat() failed %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    ret = setThumbnailParams(mThumbWidth, mThumbHeight, mThumbQuality);
    if ( NO_ERROR != ret)
        {
        CAMHAL_LOGEB("Error configuring thumbnail size %x", ret);
        return ret;
        }

    ret = setExposureBracketing( mExposureBracketingValues,
                                 mExposureBracketingValidEntries, mBurstFrames);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEB("setExposureBracketing() failed %d", ret);
        return ret;
        }

    ret = setImageQuality(mPictureQuality);
    if ( NO_ERROR != ret)
        {
        CAMHAL_LOGEB("Error configuring image quality %x", ret);
        return ret;
        }

    ///Register for Image port ENABLE event
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandPortEnable,
                                mCameraAdapterParameters.mImagePortIndex,
                                camSem,
                                -1);

    ///Enable Capture Port
    eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                OMX_CommandPortEnable,
                                mCameraAdapterParameters.mImagePortIndex,
                                NULL);

    for ( int index = 0 ; index < imgCaptureData->mNumBufs ; index++ )
    {
        OMX_BUFFERHEADERTYPE *pBufferHdr;
        CAMHAL_LOGDB("OMX_UseBuffer Capture address: 0x%x, size = %d", (unsigned int)buffers[index], (int)imgCaptureData->mBufSize);

        eError = OMX_UseBuffer( mCameraAdapterParameters.mHandleComp,
                                &pBufferHdr,
                                mCameraAdapterParameters.mImagePortIndex,
                                0,
                                mCaptureBuffersLength,
                                (OMX_U8*)buffers[index]);

        CAMHAL_LOGDB("OMX_UseBuffer = 0x%x", eError);

        GOTO_EXIT_IF(( eError != OMX_ErrorNone ), eError);

        pBufferHdr->pAppPrivate = (OMX_PTR) index;
        pBufferHdr->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pBufferHdr->nVersion.s.nVersionMajor = 1 ;
        pBufferHdr->nVersion.s.nVersionMinor = 1 ;
        pBufferHdr->nVersion.s.nRevision = 0;
        pBufferHdr->nVersion.s.nStep =  0;
        imgCaptureData->mBufferHeader[index] = pBufferHdr;
    }

    //Wait for the image port enable event
    CAMHAL_LOGDA("Waiting for port enable");
    camSem.Wait();
    CAMHAL_LOGDA("Port enabled");

    if ( NO_ERROR == ret )
        {
        ret = setupEXIF();
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error configuring EXIF Buffer %x", ret);
            }
        }

    mCapturedFrames = mBurstFrames;
    mCaptureConfigured = true;

    EXIT:

    if ( eError != OMX_ErrorNone )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::startSmoothZoom(int targetIdx)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mZoomLock);

    CAMHAL_LOGDB("Start smooth zoom target = %d, mCurrentIdx = %d", targetIdx, mCurrentZoomIdx);

    if ( ( targetIdx >= 0 ) && ( targetIdx < ZOOM_STAGES ) )
        {
        mTargetZoomIdx = targetIdx;
        mZoomParameterIdx = mCurrentZoomIdx;
        mSmoothZoomEnabled = true;
        }
    else
        {
        CAMHAL_LOGEB("Smooth value out of range %d!", targetIdx);
        ret = -EINVAL;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::stopSmoothZoom()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mZoomLock);

    if ( mSmoothZoomEnabled )
        {
        mTargetZoomIdx = mCurrentZoomIdx;
        mReturnZoomStatus = true;
        CAMHAL_LOGDB("Stop smooth zoom mCurrentZoomIdx = %d, mTargetZoomIdx = %d", mCurrentZoomIdx, mTargetZoomIdx);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::startPreview()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMXCameraPortParameters *mPreviewData = NULL;
    OMXCameraPortParameters *measurementData = NULL;

    LOG_FUNCTION_NAME

    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }


    if ( mPreviewing )
        {
        CAMHAL_LOGEA("Calling startPreview() when preview is running");
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
        }

    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
    measurementData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mMeasurementPortIndex];

    if( OMX_StateIdle == mComponentState )
        {
        ///Register for EXECUTING state transition.
        ///This method just inserts a message in Event Q, which is checked in the callback
        ///The sempahore passed is signalled by the callback
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                               OMX_EventCmdComplete,
                               OMX_CommandStateSet,
                               OMX_StateExecuting,
                               eventSem,
                               -1);

        if(ret!=NO_ERROR)
            {
            CAMHAL_LOGEB("Error in registering for event %d", ret);
            goto EXIT;
            }

        ///Switch to EXECUTING state
        eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                 OMX_CommandStateSet,
                                 OMX_StateExecuting,
                                 NULL);

        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_SendCommand(OMX_StateExecuting)-0x%x", eError);
            }

        CAMHAL_LOGDA("+Waiting for component to go into EXECUTING state");
        ret = eventSem.Wait();

        CAMHAL_LOGDA("+Great. Component went into executing state!!");

        mComponentState = OMX_StateExecuting;

        }

   ///Queue all the buffers on preview port
    for (int index = 0; index < mPreviewData->mNumBufs; index++)
        {
        CAMHAL_LOGDB("Queuing buffer on Preview port - 0x%x", (uint32_t)mPreviewData->mBufferHeader[index]->pBuffer);
        eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                    (OMX_BUFFERHEADERTYPE*)mPreviewData->mBufferHeader[index]);
        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_FillThisBuffer-0x%x", eError);
            }
        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
        }

    if (mMeasurementEnabled)
        {
        for (int index = 0; index < measurementData->mNumBufs; index++)
            {
            CAMHAL_LOGDB("Queuing buffer on Measurement port - 0x%x", (uint32_t) measurementData->mBufferHeader[index]->pBuffer);
            eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                            (OMX_BUFFERHEADERTYPE*) measurementData->mBufferHeader[index]);
            if(eError!=OMX_ErrorNone)
                {
                CAMHAL_LOGEB("OMX_FillThisBuffer-0x%x", eError);
                }
            GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
            }
        }

    mPreviewing = true;

    if ( mPending3Asettings )
        apply3Asettings(mParameters3A);

    //Query current focus distance after
    //starting the preview
    updateFocusDistances(mParameters);

    //reset frame rate estimates
    mFPS = 0.0f;
    mLastFPS = 0.0f;
    mFrameCount = 0;
    mLastFrameCount = 0;
    mIter = 1;
    mLastFPSTime = systemTime();

    LOG_FUNCTION_NAME_EXIT

    return ret;

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);

    if ( eError != OMX_ErrorNone )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    LOG_FUNCTION_NAME_EXIT
    return (ret | ErrorUtils::omxToAndroidError(eError));

}

status_t OMXCameraAdapter::stopPreview()
{
    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    status_t ret = NO_ERROR;

    OMXCameraPortParameters *mCaptureData , *mPreviewData, *measurementData;
    mCaptureData = mPreviewData = measurementData = NULL;

    Mutex::Autolock lock(mLock);

    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
    mCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];
    measurementData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mMeasurementPortIndex];

    Semaphore eventSem;

    if ( !mPreviewing )
        {
        return NO_ERROR;
        }

    if ( mComponentState != OMX_StateExecuting )
        {
        CAMHAL_LOGEA("Calling StopPreview() when not in EXECUTING state");
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
        }

    ret = cancelAutoFocus();
    if(ret!=NO_ERROR)
    {
        CAMHAL_LOGEB("Error canceling autofocus %d", ret);
        // Error, but we probably still want to continue to stop preview
    }

    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    CAMHAL_LOGEB("Average framerate: %f", mFPS);

    //Avoid state switching of the OMX Component
    ret = flushBuffers();
    if ( NO_ERROR != ret )
        {
        CAMHAL_LOGEB("Flush Buffers failed 0x%x", ret);
        goto EXIT;
        }

    ///Clear the previewing flag, we are no longer previewing
    mPreviewing = false;

    ///Register for Preview port Disable event
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                           OMX_EventCmdComplete,
                           OMX_CommandPortDisable,
                           mCameraAdapterParameters.mPrevPortIndex,
                           eventSem,
                           -1);

    ///Disable Preview Port
    eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                             OMX_CommandPortDisable,
                             mCameraAdapterParameters.mPrevPortIndex,
                             NULL);

    ///Free the OMX Buffers
    for (int i = 0; i < mPreviewData->mNumBufs; i++)
        {
        eError = OMX_FreeBuffer(mCameraAdapterParameters.mHandleComp,
                                mCameraAdapterParameters.mPrevPortIndex,
                                mPreviewData->mBufferHeader[i]);

        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_FreeBuffer - %x", eError);
            }
        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
        }

    if (mMeasurementEnabled)
        {
        Semaphore measurementSem;

        ret = measurementSem.Create(0);
        if (ret != NO_ERROR)
            {
            CAMHAL_LOGEB("Error in creating semaphore %d", ret);
            goto EXIT_SEMA_WAIT;
            }

        ///Register for Measurement port Disable event
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                               OMX_EventCmdComplete,
                               OMX_CommandPortDisable,
                               mCameraAdapterParameters.mMeasurementPortIndex,
                               measurementSem,
                               -1);

        ///Disable Measurement Port
        eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                 OMX_CommandPortDisable,
                                 mCameraAdapterParameters.mMeasurementPortIndex,
                                 NULL);

        for (int i = 0; i < measurementData->mNumBufs; i++)
            {
            eError = OMX_FreeBuffer(mCameraAdapterParameters.mHandleComp,
                                    mCameraAdapterParameters.mMeasurementPortIndex,
                                    measurementData->mBufferHeader[i]);
            if(eError!=OMX_ErrorNone)
                {
                CAMHAL_LOGEB("OMX_FreeBuffer - %x", eError);
                }
            GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
            }

        CAMHAL_LOGDA("Disabling measurement port");
        measurementSem.Wait();
        CAMHAL_LOGDA("Measurement port disabled");

        {
        Mutex::Autolock lock(mPreviewDataBufferLock);
        mPreviewDataBuffersAvailable.clear();
        }

        Mutex::Autolock lock(mSMALCLock);

        // Saving SMALC data to DCC File
        if (mSensorIndex == OMX_TI_StereoSensor && mSMALCDataRecord != NULL && mSMALC_DCCFileDesc != NULL)
            {
            FILE *fd = fopenCameraDCC(DCC_PATH);

            if (fd)
                {
                if (!fseekDCCuseCasePos(fd))
                    {
                    if (fwrite(mSMALCDataRecord, mSMALCDataSize, 1, fd) != 1)
                        {
                        CAMHAL_LOGEA("ERROR: Writing to DCC file failed");
                        }
                    else
                        {
                        CAMHAL_LOGDA("DCC file successfully updated");
                        }
                    }
                fclose(fd);
                }
            else
                {
                CAMHAL_LOGEA("ERROR: Correct DCC file not found or failed to open for modification");
                }
            }
        }

    EXIT_SEMA_WAIT:

    CAMHAL_LOGDA("Disabling preview port");
    eventSem.Wait();
    CAMHAL_LOGDA("Preview port disabled");

    EXIT:

    {
    Mutex::Autolock lock(mPreviewBufferLock);
    ///Clear all the available preview buffers
    mPreviewBuffersAvailable.clear();
    }

    if ( eError != OMX_ErrorNone )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return (ret | ErrorUtils::omxToAndroidError(eError));

}

// Recursively searches given directory contents for the correct DCC file.
// The directory must be opened and its stream pointer + path passed
// as arguments. As this function is called recursively, to avoid excessive
// stack usage the path param is reused -> this MUST be char array with
// enough length!!! (260 should suffice). Path must end with "/".
// The directory must also be closed in the caller function.
// If the correct camera DCC file is found (based on the OMX measurement data)
// its file stream pointer is returned. NULL is returned otherwise
FILE * OMXCameraAdapter::parseDCCsubDir(DIR *pDir, char *path)
{
    FILE *pFile;
	DIR *pSubDir;
	struct dirent *dirEntry;
    int initialPathLength = strlen(path);

    /* check each directory entry */
    while ((dirEntry = readdir(pDir)) != NULL)
    {
        if (dirEntry->d_name[0] == '.')
            continue;

        strcat(path, dirEntry->d_name);
        // dirEntry might be sub directory -> check it
        pSubDir = opendir(path);
        if (pSubDir) {
            // dirEntry is sub directory -> parse it
            strcat(path, "/");
            pFile = parseDCCsubDir(pSubDir, path);
            closedir(pSubDir);
            if (pFile) {
                // the correct DCC file found!
                return pFile;
            }
        } else {
            // dirEntry is file -> open it
            pFile = fopen(path, "rb");
            if (pFile) {
                // now check if this is the correct DCC file for that camera
                OMX_U32 dccFileIDword;
                OMX_U32 *dccFileDesc = (OMX_U32 *) mSMALC_DCCFileDesc;
                int i;

                // DCC file ID is 3 4-byte words
                for (i = 0; i < 3; i++) {
                    if (fread(&dccFileIDword, sizeof(OMX_U32), 1, pFile) != 1) {
                        // file too short
                        break;
                    }
                    if (dccFileIDword != dccFileDesc[i]) {
                        // DCC file ID word i does not match
                        break;
                    }
                }

                fclose(pFile);
                if (i == 3) {
                    // the correct DCC file found!
                    CAMHAL_LOGDB("DCC file to be updated: %s", path);
                    // reopen it for modification
                    pFile = fopen(path, "rb+");
                    if (!pFile)
                        CAMHAL_LOGEB("ERROR: DCC file %s failed to open for modification", path);
                    return pFile;
                }
            } else {
                CAMHAL_LOGEB("ERROR: Failed to open file %s for reading", path);
            }
        }
        // restore original path
        path[initialPathLength] = '\0';
    }

    // DCC file not found in this directory tree
    return NULL;
}

// Finds the DCC file corresponding to the current camera based on the
// OMX measurement data, opens it and returns the file stream pointer
// (NULL on error or if file not found).
// The folder string dccFolderPath must end with "/"
FILE * OMXCameraAdapter::fopenCameraDCC(const char *dccFolderPath)
{
    FILE *pFile;
	DIR *pDir;
    char dccPath[260];

    strcpy(dccPath, dccFolderPath);

    pDir = opendir(dccPath);
    if (!pDir) {
        CAMHAL_LOGEB("ERROR: Opening DCC directory %s failed", dccPath);
        return NULL;
    }

    pFile = parseDCCsubDir(pDir, dccPath);
    closedir(pDir);
    if (pFile) {
        CAMHAL_LOGDB("DCC file %s opened for modification", dccPath);
    }

    return pFile;
}

// Positions the DCC file stream pointer to the correct offset within the
// correct usecase based on the OMX mesurement data. Returns 0 on success
int OMXCameraAdapter::fseekDCCuseCasePos(FILE *pFile)
{
    OMX_U32 *dccFileDesc = (OMX_U32 *) mSMALC_DCCFileDesc;
    OMX_U32 dccNumUseCases = 0;
    OMX_U32 dccUseCaseData[3];
    OMX_U32 i;

    // position the file pointer to the DCC use cases section
    if (fseek(pFile, 80, SEEK_SET)) {
        CAMHAL_LOGEA("ERROR: Unexpected end of DCC file");
        return -1;
    }

    if (fread(&dccNumUseCases, sizeof(OMX_U32), 1, pFile) != 1 ||
        dccNumUseCases == 0) {
        CAMHAL_LOGEA("ERROR: DCC file contains 0 use cases");
        return -1;
    }

    for (i = 0; i < dccNumUseCases; i++) {
        if (fread(dccUseCaseData, sizeof(OMX_U32), 3, pFile) != 3) {
            CAMHAL_LOGEA("ERROR: Unexpected end of DCC file");
            return -1;
        }

        if (dccUseCaseData[0] == dccFileDesc[3]) {
            // DCC use case match!
            break;
        }
    }

    if (i == dccNumUseCases) {
        CAMHAL_LOGEB("ERROR: Use case ID %lu not found in DCC file", dccFileDesc[3]);
        return -1;
    }

    // dccUseCaseData[1] is the offset to the beginning of the actual use case
    // from the beginning of the file
    // dccFileDesc[4] is the offset within the actual use case (from the
    // beginning of the use case to the data to be modified)

    if (fseek(pFile,dccUseCaseData[1] + dccFileDesc[4], SEEK_SET ))
    {
        CAMHAL_LOGEA("ERROR: Error setting the correct offset");
        return -1;
    }

    return 0;
}

status_t OMXCameraAdapter::setTimeOut(int sec)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if( mComponentState == OMX_StateInvalid)
      {
        delete this;
        gCameraAdapter = NULL;
        return 0;
      }
    else
      {
      switchToLoaded();
      ret = alarm(sec);
      }
    //At this point ErrorNotifier becomes invalid
    mErrorNotifier = NULL;

    LOG_FUNCTION_NAME_EXIT

    return BaseCameraAdapter::setTimeOut(sec);
}

status_t OMXCameraAdapter::cancelTimeOut()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    ret = alarm(0);

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setThumbnailParams(unsigned int width, unsigned int height, unsigned int quality)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_THUMBNAILTYPE thumbConf;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT(thumbConf, OMX_PARAM_THUMBNAILTYPE);
        thumbConf.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        eError = OMX_GetParameter(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexParamThumbnail, &thumbConf);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while retrieving thumbnail size 0x%x", eError);
            ret = -1;
            }

        //CTS Requirement: width or height equal to zero should
        //result in absent EXIF thumbnail
        if ( ( 0 == width ) || ( 0 == height ) )
            {
            thumbConf.nWidth = mThumbRes[0].width;
            thumbConf.nHeight = mThumbRes[0].height;
            thumbConf.eCompressionFormat = OMX_IMAGE_CodingUnused;
            }
        else
            {
            thumbConf.nWidth = width;
            thumbConf.nHeight = height;
            thumbConf.nQuality = quality;
            thumbConf.eCompressionFormat = OMX_IMAGE_CodingJPEG;
            }

        CAMHAL_LOGDB("Thumbnail width = %d, Thumbnail Height = %d", width, height);

        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexParamThumbnail, &thumbConf);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring thumbnail size 0x%x", eError);
            ret = -1;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setImageQuality(unsigned int quality)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_PARAM_QFACTORTYPE jpegQualityConf;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT(jpegQualityConf, OMX_IMAGE_PARAM_QFACTORTYPE);
        jpegQualityConf.nQFactor = quality;
        jpegQualityConf.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp, OMX_IndexParamQFactor, &jpegQualityConf);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring jpeg Quality 0x%x", eError);
            ret = -1;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

OMX_ERRORTYPE OMXCameraAdapter::setExposureMode(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_EXPOSURECONTROLTYPE exp;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        eError = OMX_ErrorInvalidState;
        }

    if ( OMX_ErrorNone == eError )
        {

        if ( EXPOSURE_FACE_PRIORITY == Gen3A.Exposure )
                {
                //Disable Region priority and enable Face priority
                setAlgoPriority(REGION_PRIORITY, EXPOSURE_ALGO, false);
                setAlgoPriority(FACE_PRIORITY, EXPOSURE_ALGO, true);

                //Then set the mode to auto
                Gen3A.WhiteBallance = OMX_ExposureControlAuto;
                }
            else
                {
                //Disable Face and Region priority
                setAlgoPriority(FACE_PRIORITY, EXPOSURE_ALGO, false);
                setAlgoPriority(REGION_PRIORITY, EXPOSURE_ALGO, false);
                }

        OMX_INIT_STRUCT_PTR (&exp, OMX_CONFIG_EXPOSURECONTROLTYPE);
        exp.nPortIndex = OMX_ALL;
        exp.eExposureControl = (OMX_EXPOSURECONTROLTYPE)Gen3A.Exposure;

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonExposure, &exp);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring exposure mode 0x%x", eError);
            }
        else
            {
            CAMHAL_LOGDA("Camera exposure mode configured successfully");
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return eError;
}

status_t OMXCameraAdapter::setAlgoPriority(AlgoPriority priority, Algorithm3A algo, bool enable)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_CONFIG_3A_REGION_PRIORITY regionPriority;
    OMX_TI_CONFIG_3A_FACE_PRIORITY facePriority;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {

        if ( FACE_PRIORITY == priority )
            {
            OMX_INIT_STRUCT_PTR (&facePriority, OMX_TI_CONFIG_3A_FACE_PRIORITY);
            facePriority.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

            if ( algo & WHITE_BALANCE_ALGO )
                {
                if ( enable )
                    {
                    facePriority.bAwbFaceEnable = OMX_TRUE;
                    }
                else
                    {
                    facePriority.bAwbFaceEnable = OMX_FALSE;
                    }
                }

            if ( algo & EXPOSURE_ALGO )
                {
                if ( enable )
                    {
                    facePriority.bAeFaceEnable = OMX_TRUE;
                    }
                else
                    {
                    facePriority.bAeFaceEnable = OMX_FALSE;
                    }
                }

            if ( algo & FOCUS_ALGO )
                {
                if ( enable )
                    {
                    facePriority.bAfFaceEnable= OMX_TRUE;
                    }
                else
                    {
                    facePriority.bAfFaceEnable = OMX_FALSE;
                    }
                }

            eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_TI_IndexConfigFacePriority3a, &facePriority);
            if ( OMX_ErrorNone != eError )
                {
                CAMHAL_LOGEB("Error while configuring face priority 0x%x", eError);
                }
            else
                {
                CAMHAL_LOGEA("Face priority for algorithms set successfully");
                }

            }
        else if ( REGION_PRIORITY == priority )
            {

            OMX_INIT_STRUCT_PTR (&regionPriority, OMX_TI_CONFIG_3A_REGION_PRIORITY);
            regionPriority.nPortIndex =  mCameraAdapterParameters.mImagePortIndex;

            if ( algo & WHITE_BALANCE_ALGO )
                {
                if ( enable )
                    {
                    regionPriority.bAwbRegionEnable= OMX_TRUE;
                    }
                else
                    {
                    regionPriority.bAwbRegionEnable = OMX_FALSE;
                    }
                }

            if ( algo & EXPOSURE_ALGO )
                {
                if ( enable )
                    {
                    regionPriority.bAeRegionEnable = OMX_TRUE;
                    }
                else
                    {
                    regionPriority.bAeRegionEnable = OMX_FALSE;
                    }
                }

            if ( algo & FOCUS_ALGO )
                {
                if ( enable )
                    {
                    regionPriority.bAfRegionEnable = OMX_TRUE;
                    }
                else
                    {
                    regionPriority.bAfRegionEnable = OMX_FALSE;
                    }
                }

            eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_TI_IndexConfigRegionPriority3a, &regionPriority);
            if ( OMX_ErrorNone != eError )
                {
                CAMHAL_LOGEB("Error while configuring region priority 0x%x", eError);
                }
            else
                {
                CAMHAL_LOGEA("Region priority for algorithms set successfully");
                }

            }

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::parseExpRange(const char *rangeStr, int * expRange, size_t count, size_t &validEntries)
{
    status_t ret = NO_ERROR;
    char *ctx, *expVal;
    char *tmp = NULL;
    size_t i = 0;

    LOG_FUNCTION_NAME

    if ( NULL == rangeStr )
        {
        return -EINVAL;
        }

    if ( NULL == expRange )
        {
        return -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        tmp = ( char * ) malloc( strlen(rangeStr) + 1 );

        if ( NULL == tmp )
            {
            CAMHAL_LOGEA("No resources for temporary buffer");
            return -1;
            }
        memset(tmp, '\0', strlen(rangeStr) + 1);

        }

    if ( NO_ERROR == ret )
        {
        strncpy(tmp, rangeStr, strlen(rangeStr) );
        expVal = strtok_r( (char *) tmp, CameraHal::PARAMS_DELIMITER, &ctx);

        i = 0;
        while ( ( NULL != expVal ) && ( i < count ) )
            {
            expRange[i] = atoi(expVal);
            expVal = strtok_r(NULL, CameraHal::PARAMS_DELIMITER, &ctx);
            i++;
            }
        validEntries = i;
        }

    if ( NULL != tmp )
        {
        free(tmp);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setExposureBracketing(int *evValues, size_t evCount, size_t frameCount)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_CAPTUREMODETYPE expCapMode;
    OMX_CONFIG_EXTCAPTUREMODETYPE extExpCapMode;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NULL == evValues )
        {
        CAMHAL_LOGEA("Exposure compensation values pointer is invalid");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&expCapMode, OMX_CONFIG_CAPTUREMODETYPE);
        expCapMode.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        /// If frameCount>0 but evCount<=0, then this is the case of HQ burst. Otherwise, it is normal HQ capture
        ///If frameCount>0 and evCount>0 then this is the cause of HQ Exposure bracketing.
        if ( 0 == evCount && 0 == frameCount )
            {
            expCapMode.bFrameLimited = OMX_FALSE;
            }
        else
            {
            expCapMode.bFrameLimited = OMX_TRUE;
            expCapMode.nFrameLimit = frameCount;
            }

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCaptureMode, &expCapMode);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring capture mode 0x%x", eError);
            }
        else
            {
            CAMHAL_LOGDA("Camera capture mode configured successfully");
            }
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&extExpCapMode, OMX_CONFIG_EXTCAPTUREMODETYPE);
        extExpCapMode.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        if ( 0 == evCount )
            {
            extExpCapMode.bEnableBracketing = OMX_FALSE;
            }
        else
            {
            extExpCapMode.bEnableBracketing = OMX_TRUE;
            extExpCapMode.tBracketConfigType.eBracketMode = OMX_BracketExposureRelativeInEV;
            extExpCapMode.tBracketConfigType.nNbrBracketingValues = evCount - 1;
            }

        for ( unsigned int i = 0 ; i < evCount ; i++ )
            {
            extExpCapMode.tBracketConfigType.nBracketValues[i]  =  ( evValues[i] * ( 1 << Q16_OFFSET ) )  / 10;
            }

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexConfigExtCaptureMode, &extExpCapMode);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring extended capture mode 0x%x", eError);
            }
        else
            {
            CAMHAL_LOGDA("Extended camera capture mode configured successfully");
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setManualExposure(Gen3A_settings& Gen3A)
{
    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMX_CONFIG_EXPOSUREVALUETYPE expValuesLeft;
    expValuesLeft.nSize = sizeof(OMX_CONFIG_EXPOSUREVALUETYPE);
    expValuesLeft.nVersion = mLocalVersionParam;
    expValuesLeft.nPortIndex = OMX_ALL;

    OMX_TI_CONFIG_EXPOSUREVALUERIGHTTYPE expValuesRight;
    expValuesRight.nSize = sizeof(OMX_TI_CONFIG_EXPOSUREVALUERIGHTTYPE);
    expValuesRight.nVersion = mLocalVersionParam;
    expValuesRight.nPortIndex = OMX_ALL;

    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &expValuesLeft);
    if( 0 >= Gen3A.ExposureValueLeft )
        {
        expValuesLeft.bAutoShutterSpeed = OMX_TRUE;
        }
    else
        {
        expValuesLeft.bAutoShutterSpeed = OMX_FALSE;
        expValuesLeft.nShutterSpeedMsec = Gen3A.ExposureValueLeft;
        }

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &expValuesLeft);

    if ( eError != OMX_ErrorNone )
        {
        CAMHAL_LOGEB("Error while configuring manual exposure left 0x%x", eError);
        }
    else
        {
        CAMHAL_LOGDB("Manual Exposure Left for Hal and OMX = %d ms", (int) Gen3A.ExposureValueLeft);
        CAMHAL_LOGDA("Manual exposure left configured successfully");
        }

    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,
        (OMX_INDEXTYPE) OMX_TI_IndexConfigRightExposureValue, &expValuesRight);
    if( 0 >= Gen3A.ExposureValueRight )
        {
        expValuesLeft.bAutoShutterSpeed = OMX_TRUE;
        }
    else
        {
        expValuesLeft.bAutoShutterSpeed = OMX_FALSE;
        expValuesRight.nShutterSpeedMsec = Gen3A.ExposureValueRight;
        }

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
        (OMX_INDEXTYPE) OMX_TI_IndexConfigRightExposureValue, &expValuesRight);

    if ( eError != OMX_ErrorNone )
        {
        CAMHAL_LOGEB("Error while configuring manual exposure right 0x%x", eError);
        }
    else
        {
        CAMHAL_LOGDB("Manual Exposure Right for Hal and OMX = %d ms", (int) Gen3A.ExposureValueRight);
        CAMHAL_LOGDA("Manual exposure right configured successfully");
        }

    LOG_FUNCTION_NAME_EXIT

    return (ErrorUtils::omxToAndroidError (eError));
}

status_t OMXCameraAdapter::setManualGain(Gen3A_settings& Gen3A)
{
    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMX_CONFIG_EXPOSUREVALUETYPE expValuesLeft;
    expValuesLeft.nSize = sizeof(OMX_CONFIG_EXPOSUREVALUETYPE);
    expValuesLeft.nVersion = mLocalVersionParam;
    expValuesLeft.nPortIndex = OMX_ALL;

    OMX_TI_CONFIG_EXPOSUREVALUERIGHTTYPE expValuesRight;
    expValuesRight.nSize = sizeof(OMX_TI_CONFIG_EXPOSUREVALUERIGHTTYPE);
    expValuesRight.nVersion = mLocalVersionParam;
    expValuesRight.nPortIndex = OMX_ALL;

    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &expValuesLeft);
    if( 0 == Gen3A.ManualGainISOLeft )
        {
        expValuesLeft.bAutoSensitivity = OMX_TRUE;
        }
    else
        {
        expValuesLeft.bAutoSensitivity = OMX_FALSE;
        expValuesLeft.nSensitivity = Gen3A.ManualGainISOLeft;
        }

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &expValuesLeft);

    if ( eError != OMX_ErrorNone )
        {
        CAMHAL_LOGEB("Error while configuring manual gain left 0x%x", eError);
        }
    else
        {
        CAMHAL_LOGDB("Manual Gain Left for Hal and OMX= %d", (int)Gen3A.ManualGainISOLeft);
        CAMHAL_LOGDA("Manual gain left configured successfully");
        }

    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,
        (OMX_INDEXTYPE) OMX_TI_IndexConfigRightExposureValue, &expValuesRight);
    if( 0 == Gen3A.ManualGainISORight )
        {
        expValuesLeft.bAutoSensitivity = OMX_TRUE;
        }
    else
        {
        expValuesLeft.bAutoSensitivity = OMX_FALSE;
        expValuesRight.nSensitivity = Gen3A.ManualGainISORight;
        }

    eError = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,
        (OMX_INDEXTYPE) OMX_TI_IndexConfigRightExposureValue, &expValuesRight);

    if ( eError != OMX_ErrorNone )
        {
        CAMHAL_LOGEB("Error while configuring manual gain right 0x%x", eError);
        }
    else
        {
        CAMHAL_LOGDB("Manual Gain Right for Hal and OMX= %d", (int)Gen3A.ManualGainISORight);
        CAMHAL_LOGDA("Manual gain right configured successfully");
        }

    LOG_FUNCTION_NAME_EXIT

    return (ErrorUtils::omxToAndroidError (eError));
}

status_t OMXCameraAdapter::setFaceDetection(bool enable)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_EXTRADATATYPE extraDataControl;
    OMX_CONFIG_OBJDETECTIONTYPE objDetection;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&objDetection, OMX_CONFIG_OBJDETECTIONTYPE);
        objDetection.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
        if  ( enable )
            {
            objDetection.bEnable = OMX_TRUE;
            }
        else
            {
            objDetection.bEnable = OMX_FALSE;
            }

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexConfigImageFaceDetection, &objDetection);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring face detection 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGEA("Face detection configuring successfully");
            }
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&extraDataControl, OMX_CONFIG_EXTRADATATYPE);
        extraDataControl.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
        extraDataControl.eExtraDataType = OMX_FaceDetection;
        extraDataControl.eCameraView = OMX_2D;
        if  ( enable )
            {
            extraDataControl.bEnable = OMX_TRUE;
            }
        else
            {
            extraDataControl.bEnable = OMX_FALSE;
            }

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexConfigOtherExtraDataControl, &extraDataControl);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring face detection extra data 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGEA("Face detection extra data configuring successfully");
            }
        }

    if ( NO_ERROR == ret )
        {
        Mutex::Autolock lock(mFaceDetectionLock);
        mFaceDetectionRunning = enable;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::detectFaces(OMX_BUFFERHEADERTYPE* pBuffHeader)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_FACERESULT *faceResult;
    OMX_OTHER_EXTRADATATYPE *extraData;
    OMX_FACEDETECTIONTYPE *faceData;
    OMX_TI_PLATFORMPRIVATE *platformPrivate;

    LOG_FUNCTION_NAME

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in executing state");
        ret = -1;
        }

    if ( NULL == pBuffHeader )
        {
        CAMHAL_LOGEA("Invalid Buffer header");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {

        platformPrivate = (OMX_TI_PLATFORMPRIVATE *) (pBuffHeader->pPlatformPrivate);
        if ( NULL != platformPrivate )
            {

            if ( sizeof(OMX_TI_PLATFORMPRIVATE) == platformPrivate->nSize )
                {
                CAMHAL_LOGVB("Size = %d, sizeof = %d, pAuxBuf = 0x%x, pAuxBufSize= %d, pMetaDataBufer = 0x%x, nMetaDataSize = %d",
                                        platformPrivate->nSize,
                                        sizeof(OMX_TI_PLATFORMPRIVATE),
                                        platformPrivate->pAuxBuf1,
                                        platformPrivate->pAuxBufSize1,
                                        platformPrivate->pMetaDataBuffer,
                                        platformPrivate->nMetaDataSize
                                        );
                }
            else
                {
                CAMHAL_LOGEB("OMX_TI_PLATFORMPRIVATE size mismatch: expected = %d, received = %d",
                                            ( unsigned int ) sizeof(OMX_TI_PLATFORMPRIVATE),
                                            ( unsigned int ) platformPrivate->nSize);
                ret = -EINVAL;
                }
            }
        else
            {
            CAMHAL_LOGEA("Invalid OMX_TI_PLATFORMPRIVATE");
            ret = -EINVAL;
            }

    }

    if ( NO_ERROR == ret )
        {

        if ( 0 >= platformPrivate->nMetaDataSize )
            {
            CAMHAL_LOGEB("OMX_TI_PLATFORMPRIVATE nMetaDataSize is size is %d",
                                            ( unsigned int ) platformPrivate->nMetaDataSize);
            ret = -EINVAL;
            }

        }

    if ( NO_ERROR == ret )
        {

        extraData = (OMX_OTHER_EXTRADATATYPE *) (platformPrivate->pMetaDataBuffer);
        if ( NULL != extraData )
            {
            CAMHAL_LOGVB("Size = %d, sizeof = %d, eType = 0x%x, nDataSize= %d, nPortIndex = 0x%x, nVersion = 0x%x",
                                        extraData->nSize,
                                        sizeof(OMX_OTHER_EXTRADATATYPE),
                                        extraData->eType,
                                        extraData->nDataSize,
                                        extraData->nPortIndex,
                                        extraData->nVersion
                                        );
            }
        else
            {
            CAMHAL_LOGEA("Invalid OMX_OTHER_EXTRADATATYPE");
            ret = -EINVAL;
            }
        }

    if ( NO_ERROR == ret )
        {

        faceData = ( OMX_FACEDETECTIONTYPE * ) extraData->data;
        if ( NULL != faceData )
            {
            if ( sizeof(OMX_FACEDETECTIONTYPE) == faceData->nSize )
                {
                CAMHAL_LOGVB("Faces detected %d",
                                            faceData->ulFaceCount,
                                            faceData->nSize,
                                            sizeof(OMX_FACEDETECTIONTYPE),
                                            faceData->eCameraView,
                                            faceData->nPortIndex,
                                            faceData->nVersion);
                }
            else
                {
                CAMHAL_LOGEB("OMX_FACEDETECTIONTYPE size mismatch: expected = %d, received = %d",
                                            ( unsigned int ) sizeof(OMX_FACEDETECTIONTYPE),
                                            ( unsigned int ) faceData->nSize);
                ret = -EINVAL;
                }

            }
        else
            {
            CAMHAL_LOGEA("Invalid OMX_FACEDETECTIONTYPE");
            ret = -EINVAL;
            }

        }

    if ( NO_ERROR == ret )
        {
        ret = encodeFaceCoordinates(faceData, mFaceDectionResult, FACE_DETECTION_BUFFER_SIZE);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::encodeFaceCoordinates(const OMX_FACEDETECTIONTYPE *faceData, char *faceString, size_t faceStringSize)
{
    status_t ret = NO_ERROR;
    OMX_TI_FACERESULT *faceResult;
    size_t faceResultSize;
    int count = 0;
    char *p;

    LOG_FUNCTION_NAME

    if ( NULL == faceData )
        {
        CAMHAL_LOGEA("Invalid OMX_FACEDETECTIONTYPE parameter");
        ret = -EINVAL;
        }

    if ( NULL == faceString )
        {
        CAMHAL_LOGEA("Invalid faceString parameter");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {

        p = mFaceDectionResult;
        faceResultSize = faceStringSize;
        faceResult = ( OMX_TI_FACERESULT * ) faceData->tFacePosition;
        if ( 0 < faceData->ulFaceCount )
            {
            for ( int i = 0  ; i < faceData->ulFaceCount ; i++)
                {

                if ( mFaceDetectionThreshold <= faceResult->nScore )
                    {
                    CAMHAL_LOGVB("Face %d: left = %d, top = %d, width = %d, height = %d", i,
                                                           ( unsigned int ) faceResult->nLeft,
                                                           ( unsigned int ) faceResult->nTop,
                                                           ( unsigned int ) faceResult->nWidth,
                                                           ( unsigned int ) faceResult->nHeight);

                    count = snprintf(p, faceResultSize, "%d,%dx%d,%dx%d,",
                                                           ( unsigned int ) faceResult->nOrientationRoll,
                                                           ( unsigned int ) faceResult->nLeft,
                                                           ( unsigned int ) faceResult->nTop,
                                                           ( unsigned int ) faceResult->nWidth,
                                                           ( unsigned int ) faceResult->nHeight);
                    }

                p += count;
                faceResultSize -= count;
                faceResult++;
                }
            }
        else
            {
            memset(mFaceDectionResult, '\0', faceStringSize);
            }
        }
    else
        {
        memset(mFaceDectionResult, '\0', faceStringSize);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

OMX_ERRORTYPE OMXCameraAdapter::setScene(Gen3A_settings& Gen3A)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_SCENEMODETYPE scene;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        eError = OMX_ErrorInvalidState;
        }

    if ( OMX_ErrorNone == eError )
        {
        OMX_INIT_STRUCT_PTR (&scene, OMX_CONFIG_SCENEMODETYPE);
        scene.nPortIndex = OMX_ALL;
        scene.eSceneMode = ( OMX_SCENEMODETYPE ) Gen3A.SceneMode;

        CAMHAL_LOGEB("Configuring scene mode 0x%x", scene.eSceneMode);
        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_TI_IndexConfigSceneMode, &scene);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring scene mode 0x%x", eError);
            }
        else
            {
            CAMHAL_LOGDA("Camera scene configured successfully");
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return eError;
}

OMX_ERRORTYPE OMXCameraAdapter::setFlashMode(Gen3A_settings& Gen3A)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_PARAM_FLASHCONTROLTYPE flash;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&flash, OMX_IMAGE_PARAM_FLASHCONTROLTYPE);
        flash.nPortIndex = OMX_ALL;
        flash.eFlashControl = ( OMX_IMAGE_FLASHCONTROLTYPE ) Gen3A.FlashMode;

        CAMHAL_LOGEB("Configuring flash mode 0x%x", flash.eFlashControl);
        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE) OMX_IndexConfigFlashControl, &flash);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring flash mode 0x%x", eError);
            }
        else
            {
            CAMHAL_LOGDA("Camera flash mode configured successfully");
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return eError;
}


status_t OMXCameraAdapter::setPictureRotation(unsigned int degree)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_ROTATIONTYPE rotation;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT(rotation, OMX_CONFIG_ROTATIONTYPE);
        rotation.nRotation = degree;
        rotation.nPortIndex = mCameraAdapterParameters.mImagePortIndex;

        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonRotate, &rotation);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring rotation 0x%x", eError);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setSensorOrientation(unsigned int degree)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_ROTATIONTYPE sensorOrientation;
    int tmpHeight, tmpWidth;
    OMXCameraPortParameters *mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

    LOG_FUNCTION_NAME
    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -1;
        }

    /* Set Temproary Port resolution.
    * For resolution with height > 1008,resolution cannot be set without configuring orientation.
    * So we first set a temp resolution. We have used VGA
    */
    tmpHeight = mPreviewData->mHeight;
    tmpWidth = mPreviewData->mWidth;
    mPreviewData->mWidth = 640;
    mPreviewData->mHeight = 480;
    ret = setFormat(OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW, *mPreviewData);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEB("setFormat() failed %d", ret);
        }

    /* Now set Required Orientation*/
    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT(sensorOrientation, OMX_CONFIG_ROTATIONTYPE);
        sensorOrientation.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
        eError = OMX_GetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonRotate, &sensorOrientation);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while Reading Sensor Orientation :  0x%x", eError);
            }
        CAMHAL_LOGEB(" Currently Sensor Orientation is set to : %d", ( unsigned int ) sensorOrientation.nRotation);
        sensorOrientation.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
        sensorOrientation.nRotation = degree;
        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonRotate, &sensorOrientation);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring rotation 0x%x", eError);
            }
        CAMHAL_LOGEA(" Read the Parameters that are set");
        eError = OMX_GetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonRotate, &sensorOrientation);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while Reading Sensor Orientation :  0x%x", eError);
            }
        CAMHAL_LOGEB(" Currently Sensor Orientation is set to : %d", ( unsigned int ) sensorOrientation.nRotation);
        CAMHAL_LOGEB(" Sensor Configured for Port : %d", ( unsigned int ) sensorOrientation.nPortIndex);
        }

    /* Now set the required resolution as requested */

    mPreviewData->mWidth = tmpWidth;
    mPreviewData->mHeight = tmpHeight;
    if ( NO_ERROR == ret )
        {
        ret = setFormat (mCameraAdapterParameters.mPrevPortIndex, mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex]);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("setFormat() failed %d", ret);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}


status_t OMXCameraAdapter::setSensorOverclock(bool enable)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_BOOLEANTYPE bOMX;

    LOG_FUNCTION_NAME

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGDA("OMX component is not in loaded state");
        return ret;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&bOMX, OMX_CONFIG_BOOLEANTYPE);

        if ( enable )
            {
            bOMX.bEnabled = OMX_TRUE;
            }
        else
            {
            bOMX.bEnabled = OMX_FALSE;
            }

        CAMHAL_LOGEB("Configuring Sensor overclock mode 0x%x", bOMX.bEnabled);
        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_TI_IndexParamSensorOverClockMode, &bOMX);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while setting Sensor overclock 0x%x", eError);
            ret = -1;
            }
        else
            {
            mSensorOverclock = enable;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setLDC(OMXCameraAdapter::IPPMode mode)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_BOOLEANTYPE bOMX;

    LOG_FUNCTION_NAME

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in loaded state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&bOMX, OMX_CONFIG_BOOLEANTYPE);

        switch ( mode )
            {
            case OMXCameraAdapter::IPP_LDCNSF:
            case OMXCameraAdapter::IPP_LDC:
                {
                bOMX.bEnabled = OMX_TRUE;
                break;
                }
            case OMXCameraAdapter::IPP_NONE:
            case OMXCameraAdapter::IPP_NSF:
            default:
                {
                bOMX.bEnabled = OMX_FALSE;
                break;
                }
            }

        CAMHAL_LOGEB("Configuring LDC mode 0x%x", bOMX.bEnabled);
        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexParamLensDistortionCorrection, &bOMX);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEA("Error while setting LDC");
            ret = -1;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setNSF(OMXCameraAdapter::IPPMode mode)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_ISONOISEFILTERTYPE nsf;

    LOG_FUNCTION_NAME

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in loaded state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&nsf, OMX_PARAM_ISONOISEFILTERTYPE);

        switch ( mode )
            {
            case OMXCameraAdapter::IPP_LDCNSF:
            case OMXCameraAdapter::IPP_NSF:
                {
                nsf.eMode = OMX_ISONoiseFilterModeOn;
                break;
                }
            case OMXCameraAdapter::IPP_LDC:
            case OMXCameraAdapter::IPP_NONE:
            default:
                {
                nsf.eMode = OMX_ISONoiseFilterModeOff;
                break;
                }
            }

        CAMHAL_LOGEB("Configuring NSF mode 0x%x", nsf.eMode);
        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexParamHighISONoiseFiler, &nsf);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEA("Error while setting NSF");
            ret = -1;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

int OMXCameraAdapter::getRevision()
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return mCompRevision.nVersion;
}

status_t OMXCameraAdapter::printComponentVersion(OMX_HANDLETYPE handle)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_VERSIONTYPE compVersion;
    char compName[OMX_MAX_STRINGNAME_SIZE];
    char *currentUUID = NULL;
    size_t offset = 0;

    LOG_FUNCTION_NAME

    if ( NULL == handle )
        {
        CAMHAL_LOGEB("Invalid OMX Handle =0x%x",  ( unsigned int ) handle);
        ret = -EINVAL;
        }

    mCompUUID[0] = 0;

    if ( NO_ERROR == ret )
        {
        eError = OMX_GetComponentVersion(handle,
                                      compName,
                                      &compVersion,
                                      &mCompRevision,
                                      &mCompUUID
                                    );
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("OMX_GetComponentVersion returned 0x%x", eError);
            ret = -1;
            }
        }

    if ( NO_ERROR == ret )
        {
        CAMHAL_LOGEB("OMX Component name: [%s]", compName);
        CAMHAL_LOGEB("OMX Component version: [%u]", ( unsigned int ) compVersion.nVersion);
        CAMHAL_LOGEB("Spec version: [%u]", ( unsigned int ) mCompRevision.nVersion);
        CAMHAL_LOGEB("Git Commit ID: [%s]", mCompUUID);
        currentUUID = ( char * ) mCompUUID;
        }

    if ( NULL != currentUUID )
        {
        offset = strlen( ( const char * ) mCompUUID) + 1;
        if ( (int)currentUUID + (int)offset - (int)mCompUUID < OMX_MAX_STRINGNAME_SIZE )
            {
            currentUUID += offset;
            CAMHAL_LOGEB("Git Branch: [%s]", currentUUID);
            }
        else
            {
            ret = -1;
            }
    }

    if ( NO_ERROR == ret )
        {
        offset = strlen( ( const char * ) currentUUID) + 1;

        if ( (int)currentUUID + (int)offset - (int)mCompUUID < OMX_MAX_STRINGNAME_SIZE )
            {
            currentUUID += offset;
            CAMHAL_LOGEB("Build date and time: [%s]", currentUUID);
            }
        else
            {
            ret = -1;
            }
        }

    if ( NO_ERROR == ret )
        {
        offset = strlen( ( const char * ) currentUUID) + 1;

        if ( (int)currentUUID + (int)offset - (int)mCompUUID < OMX_MAX_STRINGNAME_SIZE )
            {
            currentUUID += offset;
            CAMHAL_LOGEB("Build description: [%s]", currentUUID);
            }
        else
            {
            ret = -1;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setGBCE(OMXCameraAdapter::BrightnessMode mode)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_CONFIG_LOCAL_AND_GLOBAL_BRIGHTNESSCONTRASTTYPE bControl;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&bControl,
                                               OMX_TI_CONFIG_LOCAL_AND_GLOBAL_BRIGHTNESSCONTRASTTYPE);

        bControl.nPortIndex = OMX_ALL;

        switch ( mode )
            {
            case OMXCameraAdapter::BRIGHTNESS_ON:
                {
                bControl.eControl = OMX_TI_BceModeOn;
                break;
                }
            case OMXCameraAdapter::BRIGHTNESS_AUTO:
                {
                bControl.eControl = OMX_TI_BceModeAuto;
                break;
                }
            case OMXCameraAdapter::BRIGHTNESS_OFF:
            default:
                {
                bControl.eControl = OMX_TI_BceModeOff;
                break;
                }
            }

        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp,
                                               ( OMX_INDEXTYPE ) OMX_TI_IndexConfigGlobalBrightnessContrastEnhance,
                                               &bControl);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while setting GBCE 0x%x", eError);
            }
        else
            {
            CAMHAL_LOGDB("GBCE configured successfully 0x%x", mode);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setGLBCE(OMXCameraAdapter::BrightnessMode mode)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_CONFIG_LOCAL_AND_GLOBAL_BRIGHTNESSCONTRASTTYPE bControl;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&bControl,
                                               OMX_TI_CONFIG_LOCAL_AND_GLOBAL_BRIGHTNESSCONTRASTTYPE);

        bControl.nPortIndex = OMX_ALL;

        switch ( mode )
            {
            case OMXCameraAdapter::BRIGHTNESS_ON:
                {
                bControl.eControl = OMX_TI_BceModeOn;
                break;
                }
            case OMXCameraAdapter::BRIGHTNESS_AUTO:
                {
                bControl.eControl = OMX_TI_BceModeAuto;
                break;
                }
            case OMXCameraAdapter::BRIGHTNESS_OFF:
            default:
                {
                bControl.eControl = OMX_TI_BceModeOff;
                break;
                }
            }

        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp,
                                               ( OMX_INDEXTYPE ) OMX_TI_IndexConfigLocalBrightnessContrastEnhance,
                                               &bControl);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configure GLBCE 0x%x", eError);
            }
        else
            {
            CAMHAL_LOGDA("GLBCE configured successfully");
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setS3DFrameLayout(OMX_U32 port)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_FRAMELAYOUTTYPE frameLayout;
    OMX_TI_STEREOFRAMELAYOUTTYPE frameLayoutType;
    OMX_U32 subsampleRatio;

    LOG_FUNCTION_NAME

    switch (mS3DImageFormat)
        {
        case S3D_TB_FULL: // Top bottom full
            {
            frameLayoutType = OMX_TI_StereoFrameLayoutTopBottom;
            subsampleRatio = 1; // for full
            break;
            }
        case S3D_SS_FULL: // Side by side full
            {
            frameLayoutType = OMX_TI_StereoFrameLayoutLeftRight;
            subsampleRatio = 1; // for full
            break;
            }
        case S3D_TB_SUBSAMPLED: // Top bottom subsampled
            {
            frameLayoutType = OMX_TI_StereoFrameLayoutTopBottom;
            subsampleRatio = 2; // for subsample
            break;
            }
        case S3D_SS_SUBSAMPLED: // Side by side subsampled
            {
            frameLayoutType = OMX_TI_StereoFrameLayoutLeftRight;
            subsampleRatio = 2; // for subsample
            break;
            }
        default:
            {
            frameLayoutType = OMX_TI_StereoFrameLayout2D;
            subsampleRatio = 1; // for full
            }
        }
    subsampleRatio = subsampleRatio << 7;

    OMX_INIT_STRUCT_PTR (&frameLayout, OMX_TI_FRAMELAYOUTTYPE);
    frameLayout.nPortIndex = port;
    eError = OMX_GetParameter(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_TI_IndexParamStereoFrmLayout, &frameLayout);
    if ( eError != OMX_ErrorNone ) {
        CAMHAL_LOGEB("Error while getting S3D frame layout: 0x%x", eError);
        }
    else {
        frameLayout.eFrameLayout = frameLayoutType;
        frameLayout.nSubsampleRatio = subsampleRatio;
        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp,
                (OMX_INDEXTYPE)OMX_TI_IndexParamStereoFrmLayout, &frameLayout);
        if ( eError != OMX_ErrorNone )
            {
            CAMHAL_LOGEB("Error while setting S3D frame layout: 0x%x", eError);
            ret = -EINVAL;
            }
        else
            {
            CAMHAL_LOGDA("S3D frame layout applied successfully");
            }
        }

EXIT:

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t OMXCameraAdapter::setCaptureMode(OMXCameraAdapter::CaptureMode mode)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_CAMOPERATINGMODETYPE camMode;
    OMX_CONFIG_BOOLEANTYPE bCAC;

    LOG_FUNCTION_NAME

    OMX_INIT_STRUCT_PTR (&bCAC, OMX_CONFIG_BOOLEANTYPE);
    bCAC.bEnabled = OMX_FALSE;

    if ( NO_ERROR == ret )
        {

        OMX_INIT_STRUCT_PTR (&camMode, OMX_CONFIG_CAMOPERATINGMODETYPE);
        if ( mSensorIndex == OMX_TI_StereoSensor )
            {
            CAMHAL_LOGDA("Camera mode: STEREO");
            camMode.eCamOperatingMode = OMX_CaptureStereoImageCapture;
            }
        else if ( OMXCameraAdapter::HIGH_SPEED == mode )
            {
            CAMHAL_LOGDA("Camera mode: HIGH SPEED");
            camMode.eCamOperatingMode = OMX_CaptureImageHighSpeedTemporalBracketing;
            }
        else if( OMXCameraAdapter::HIGH_QUALITY == mode )
            {
            CAMHAL_LOGDA("Camera mode: HIGH QUALITY");
            camMode.eCamOperatingMode = OMX_CaptureImageProfileBase;
            bCAC.bEnabled = OMX_TRUE;
            }
        else if( OMXCameraAdapter::VIDEO_MODE == mode )
            {
            CAMHAL_LOGDA("Camera mode: VIDEO MODE");
            camMode.eCamOperatingMode = OMX_CaptureVideo;
            }
        else
            {
            CAMHAL_LOGEA("Camera mode: INVALID mode passed!");
            return BAD_VALUE;
            }

        if ( NO_ERROR == ret )
            {
            eError =  OMX_SetParameter(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexCameraOperatingMode, &camMode);
            if ( OMX_ErrorNone != eError )
                {
                CAMHAL_LOGEB("Error while configuring camera mode 0x%x", eError);
                ret = -1;
                }
            else
                {
                CAMHAL_LOGDA("Camera mode configured successfully");
                }
            }

        if ( NO_ERROR == ret )
            {
            //Configure CAC
            eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp,
                                    ( OMX_INDEXTYPE ) OMX_IndexConfigChromaticAberrationCorrection, &bCAC);
            if ( OMX_ErrorNone != eError )
                {
                CAMHAL_LOGEB("Error while configuring CAC 0x%x", eError);
                ret = -1;
                }
            else
                {
                CAMHAL_LOGDA("CAC configured successfully");
                }
            }
        }

    if ( eError != OMX_ErrorNone )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::doZoom(int index)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_SCALEFACTORTYPE zoomControl;
    static int prevIndex = 0;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -1;
        }

    if (  ( 0 > index) || ( ( ZOOM_STAGES - 1 ) < index ) )
        {
        CAMHAL_LOGEB("Zoom index %d out of range", index);
        ret = -EINVAL;
        }

    if ( prevIndex == index )
        {
        return NO_ERROR;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&zoomControl, OMX_CONFIG_SCALEFACTORTYPE);
        zoomControl.nPortIndex = OMX_ALL;
        zoomControl.xHeight = ZOOM_STEPS[index];
        zoomControl.xWidth = ZOOM_STEPS[index];

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonDigitalZoom, &zoomControl);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while applying digital zoom 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDA("Digital zoom applied successfully");
            prevIndex = index;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::parseTouchPosition(const char *pos, unsigned int &posX, unsigned int &posY)
{

    status_t ret = NO_ERROR;
    char *ctx, *pX, *pY;
    const char *sep = ",";

    LOG_FUNCTION_NAME

    if ( NULL == pos )
        {
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        pX = strtok_r( (char *) pos, sep, &ctx);

        if ( NULL != pX )
            {
            posX = atoi(pX);
            }
        else
            {
            CAMHAL_LOGEB("Invalid touch position %s", pos);
            ret = -EINVAL;
            }
        }

    if ( NO_ERROR == ret )
        {
        pY = strtok_r(NULL, sep, &ctx);

        if ( NULL != pY )
            {
            posY = atoi(pY);
            }
        else
            {
            CAMHAL_LOGEB("Invalid touch position %s", pos);
            ret = -EINVAL;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setTouchFocus(unsigned int posX, unsigned int posY, size_t width, size_t height)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_EXTFOCUSREGIONTYPE touchControl;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&touchControl, OMX_CONFIG_EXTFOCUSREGIONTYPE);
        touchControl.nLeft = ( posX * TOUCH_FOCUS_RANGE ) / width;
        touchControl.nTop =  ( posY * TOUCH_FOCUS_RANGE ) / height;
        touchControl.nWidth = 0;
        touchControl.nHeight = 0;

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_IndexConfigExtFocusRegion, &touchControl);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while configuring touch focus 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDB("Touch focus nLeft = %d, nTop = %d configured successfuly", ( int ) touchControl.nLeft, ( int ) touchControl.nTop);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::autoFocus()
{
    status_t ret = NO_ERROR;
    Message msg;

    LOG_FUNCTION_NAME

    {
        Mutex::Autolock lock(mFocusLock);
        mFocusStarted = true;
    }

    msg.command = CommandHandler::CAMERA_PERFORM_AUTOFOCUS;
    mCommandHandler->put(&msg);

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::doAutoFocus()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE focusControl;
    Semaphore eventSem;

    LOG_FUNCTION_NAME

    mFocusLock.lock();

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component not in executing state");
        mFocusStarted = false;
        returnFocusStatus(false);
        mFocusLock.unlock();
        ret = -1;
        }

    if ( !mFocusStarted )
        {
        CAMHAL_LOGVA("Focus canceled before we could start");
        ret = NO_ERROR;
        mFocusLock.unlock();
        return ret;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&focusControl, OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE);
        focusControl.eFocusControl = ( OMX_IMAGE_FOCUSCONTROLTYPE ) mParameters3A.Focus;
        //If touch AF is set, then necessary configuration first
        if ( FOCUS_REGION_PRIORITY == focusControl.eFocusControl )
            {

            //Disable face priority first
            setAlgoPriority(FACE_PRIORITY, FOCUS_ALGO, false);

            //Enable region algorithm priority
            setAlgoPriority(REGION_PRIORITY, FOCUS_ALGO, true);

            //Set position
            OMXCameraPortParameters * mPreviewData = NULL;
            mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
            OMX_U32 w = mPreviewData->mWidth;
            OMX_U32 h = mPreviewData->mHeight;

            if (mS3DImageFormat == S3D_TB_FULL) {
                h = h / 2;
            } else if (mS3DImageFormat == S3D_SS_FULL) {
                w = w / 2;
            }

            setTouchFocus(mTouchPosX, mTouchPosY, w, h);

            //Do normal focus afterwards
            focusControl.eFocusControl = ( OMX_IMAGE_FOCUSCONTROLTYPE ) OMX_IMAGE_FocusControlExtended;

            }
        else if ( FOCUS_FACE_PRIORITY == focusControl.eFocusControl )
            {

            //Disable region priority first
            setAlgoPriority(REGION_PRIORITY, FOCUS_ALGO, false);

            //Enable face algorithm priority
            setAlgoPriority(FACE_PRIORITY, FOCUS_ALGO, true);

            //Do normal focus afterwards
            focusControl.eFocusControl = ( OMX_IMAGE_FOCUSCONTROLTYPE ) OMX_IMAGE_FocusControlExtended;

            }
        else
            {

            //Disable both region and face priority
            setAlgoPriority(REGION_PRIORITY, FOCUS_ALGO, false);

            setAlgoPriority(FACE_PRIORITY, FOCUS_ALGO, false);

            }

        if ( ( mParameters3A.Focus != OMX_IMAGE_FocusControlAuto )  &&
             ( mParameters3A.Focus != OMX_IMAGE_FocusControlAutoInfinity ) )
            {

            if ( NO_ERROR == ret )
                {
                ret = eventSem.Create(0);
                }

            if ( NO_ERROR == ret )
                {
                ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                            (OMX_EVENTTYPE) OMX_EventIndexSettingChanged,
                                            OMX_ALL,
                                            OMX_IndexConfigCommonFocusStatus,
                                            eventSem,
                                            -1 );
                }

            if ( NO_ERROR == ret )
                {
                ret = setFocusCallback(true);
                }
            }

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigFocusControl, &focusControl);

        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while starting focus 0x%x", eError);
            mFocusStarted = false;
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDA("Autofocus started successfully");
            }
        }

    // unlock before waiting...
    mFocusLock.unlock();

    if ( ( mParameters3A.Focus != OMX_IMAGE_FocusControlAuto )  &&
         ( mParameters3A.Focus != OMX_IMAGE_FocusControlAutoInfinity ) )
        {

        if ( NO_ERROR == ret )
            {
            ret = eventSem.WaitTimeout(AF_CALLBACK_TIMEOUT);
            //Disable auto focus callback from Ducati
            ret |= setFocusCallback(false);
            //Signal a dummy AF event so that in case the callback from ducati does come then it doesnt crash after
            //exiting this function since eventSem will go out of scope.
            if(ret != NO_ERROR)
                {
                ret |= SignalEvent(mCameraAdapterParameters.mHandleComp,
                                            (OMX_EVENTTYPE) OMX_EventIndexSettingChanged,
                                            OMX_ALL,
                                            OMX_IndexConfigCommonFocusStatus,
                                            NULL );
                }
            }

        {
        //Acquire the lock again here as we have relinquished it above before waiting
        Mutex::Autolock lock(mFocusLock);
        if ( NO_ERROR == ret )
            {
            CAMHAL_LOGDA("Autofocus callback received");
            ret = returnFocusStatus(false);
            }
        else
            {
            CAMHAL_LOGEA("Autofocus callback timeout expired");
            ret = returnFocusStatus(true);
            }
            }

        }
    else
        {
        Mutex::Autolock lock(mFocusLock);
        if ( NO_ERROR == ret )
            {
            ret = returnFocusStatus(true);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::stopAutoFocus()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE focusControl;

    LOG_FUNCTION_NAME

    if ( mFocusStarted )
        {
        if ( OMX_StateExecuting != mComponentState )
            {
            CAMHAL_LOGEA("OMX component not in executing state");
            ret = -1;
            }

        if ( NO_ERROR == ret )
           {
           //Disable the callback first
           ret = setFocusCallback(false);
           }

        if ( NO_ERROR == ret )
            {
            OMX_INIT_STRUCT_PTR (&focusControl, OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE);
            focusControl.eFocusControl = OMX_IMAGE_FocusControlOff;

            eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigFocusControl, &focusControl);
            if ( OMX_ErrorNone != eError )
                {
                CAMHAL_LOGEB("Error while stopping focus 0x%x", eError);
                ret = -1;
                }
            else
                {
                CAMHAL_LOGDA("Autofocus stopped successfully");
                }
            }

        mFocusStarted = false;

        //Query current focus distance after AF is complete
        updateFocusDistances(mParameters);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;

}

status_t OMXCameraAdapter::cancelAutoFocus()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mFocusLock);

    if(mFocusStarted)
    {
        stopAutoFocus();
        //Signal a dummy AF event so that in case the callback from ducati does come then it doesnt crash after
        //exiting this function since eventSem will go out of scope.
        ret |= SignalEvent(mCameraAdapterParameters.mHandleComp,
                                    (OMX_EVENTTYPE) OMX_EventIndexSettingChanged,
                                    OMX_ALL,
                                    OMX_IndexConfigCommonFocusStatus,
                                    NULL );
    }

    LOG_FUNCTION_NAME_EXIT

    return ret;

}

status_t OMXCameraAdapter::setFocusCallback(bool enabled)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_CALLBACKREQUESTTYPE focusRequstCallback;

    LOG_FUNCTION_NAME

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component not in executing state");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {

        OMX_INIT_STRUCT_PTR (&focusRequstCallback, OMX_CONFIG_CALLBACKREQUESTTYPE);
        focusRequstCallback.nPortIndex = OMX_ALL;
        focusRequstCallback.nIndex = OMX_IndexConfigCommonFocusStatus;

        if ( enabled )
            {
            focusRequstCallback.bEnable = OMX_TRUE;
            }
        else
            {
            focusRequstCallback.bEnable = OMX_FALSE;
            }

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp,  (OMX_INDEXTYPE) OMX_IndexConfigCallbackRequest, &focusRequstCallback);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error registering focus callback 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDB("Autofocus callback for index 0x%x registered successfully", OMX_IndexConfigCommonFocusStatus);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::returnFocusStatus(bool timeoutReached)
{
    status_t ret = NO_ERROR;
    OMX_PARAM_FOCUSSTATUSTYPE eFocusStatus;
    bool focusStatus = false;

    LOG_FUNCTION_NAME

    OMX_INIT_STRUCT(eFocusStatus, OMX_PARAM_FOCUSSTATUSTYPE);

    if(!mFocusStarted)
       {
        /// We don't send focus callback if focus was not started
       return NO_ERROR;
       }

    if ( NO_ERROR == ret )
        {

        if ( ( !timeoutReached ) &&
             ( mFocusStarted ) )
            {
            ret = checkFocus(&eFocusStatus);

            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEA("Focus status check failed!");
                }
            }
        }

    if ( NO_ERROR == ret )
        {

        if ( timeoutReached )
            {
            focusStatus = false;
            }
        else
            {

            switch (eFocusStatus.eFocusStatus)
                {
                    case OMX_FocusStatusReached:
                        {
                        focusStatus = true;
                        break;
                        }
                    case OMX_FocusStatusOff:
                    case OMX_FocusStatusUnableToReach:
                    case OMX_FocusStatusRequest:
                    default:
                        {
                        focusStatus = false;
                        break;
                        }
                }

            stopAutoFocus();
            }
        }

    if ( NO_ERROR == ret )
        {
        ret = notifyFocusSubscribers(focusStatus);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::checkFocus(OMX_PARAM_FOCUSSTATUSTYPE *eFocusStatus)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    LOG_FUNCTION_NAME

    if ( NULL == eFocusStatus )
        {
        CAMHAL_LOGEA("Invalid focus status");
        ret = -EINVAL;
        }

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component not in executing state");
        ret = -EINVAL;
        }

    if ( !mFocusStarted )
        {
        CAMHAL_LOGEA("Focus was not started");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (eFocusStatus, OMX_PARAM_FOCUSSTATUSTYPE);

        eError = OMX_GetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonFocusStatus, eFocusStatus);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while retrieving focus status: 0x%x", eError);
            ret = -1;
            }
        }

    if ( NO_ERROR == ret )
        {
        CAMHAL_LOGDB("Focus Status: %d", eFocusStatus->eFocusStatus);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::setShutterCallback(bool enabled)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_CALLBACKREQUESTTYPE shutterRequstCallback;

    LOG_FUNCTION_NAME

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component not in executing state");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {

        OMX_INIT_STRUCT_PTR (&shutterRequstCallback, OMX_CONFIG_CALLBACKREQUESTTYPE);
        shutterRequstCallback.nPortIndex = OMX_ALL;

        if ( enabled )
            {
            shutterRequstCallback.bEnable = OMX_TRUE;
            shutterRequstCallback.nIndex = ( OMX_INDEXTYPE ) OMX_TI_IndexConfigShutterCallback;
            CAMHAL_LOGDA("Enabling shutter callback");
            }
        else
            {
            shutterRequstCallback.bEnable = OMX_FALSE;
            shutterRequstCallback.nIndex = ( OMX_INDEXTYPE ) OMX_TI_IndexConfigShutterCallback;
            CAMHAL_LOGDA("Disabling shutter callback");
            }

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp,  ( OMX_INDEXTYPE ) OMX_IndexConfigCallbackRequest, &shutterRequstCallback);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error registering shutter callback 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDB("Shutter callback for index 0x%x registered successfully", OMX_TI_IndexConfigShutterCallback);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::startBracketing(int range)
{
    status_t ret = NO_ERROR;
    OMXCameraPortParameters * imgCaptureData = NULL;

    LOG_FUNCTION_NAME

    imgCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in executing state");
        ret = -EINVAL;
        }

        {
        Mutex::Autolock lock(mBracketingLock);

        if ( mBracketingEnabled )
            {
            return ret;
            }
        }

    if ( 0 == imgCaptureData->mNumBufs )
        {
        CAMHAL_LOGEB("Image capture buffers set to %d", imgCaptureData->mNumBufs);
        ret = -EINVAL;
        }

    if ( mPending3Asettings )
        apply3Asettings(mParameters3A);

    if ( NO_ERROR == ret )
        {
        Mutex::Autolock lock(mBracketingLock);

        mBracketingRange = range;
        mBracketingBuffersQueued = new bool[imgCaptureData->mNumBufs];
        if ( NULL == mBracketingBuffersQueued )
            {
            CAMHAL_LOGEA("Unable to allocate bracketing management structures");
            ret = -1;
            }

        if ( NO_ERROR == ret )
            {
            mBracketingBuffersQueuedCount = imgCaptureData->mNumBufs;
            mLastBracetingBufferIdx = mBracketingBuffersQueuedCount - 1;

            for ( int i = 0 ; i  < imgCaptureData->mNumBufs ; i++ )
                {
                mBracketingBuffersQueued[i] = true;
                }

            }
        }

    if ( NO_ERROR == ret )
        {

        ret = startImageCapture();
            {
            Mutex::Autolock lock(mBracketingLock);

            if ( NO_ERROR == ret )
                {
                mBracketingEnabled = true;
                }
            else
                {
                mBracketingEnabled = false;
                }
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::doBracketing(OMX_BUFFERHEADERTYPE *pBuffHeader, CameraFrame::FrameType typeOfFrame)
{
    status_t ret = NO_ERROR;
    int currentBufferIdx, nextBufferIdx;
    OMXCameraPortParameters * imgCaptureData = NULL;

    LOG_FUNCTION_NAME

    imgCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in executing state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        currentBufferIdx = ( unsigned int ) pBuffHeader->pAppPrivate;

        if ( currentBufferIdx >= imgCaptureData->mNumBufs)
            {
            CAMHAL_LOGEB("Invalid bracketing buffer index 0x%x", currentBufferIdx);
            ret = -EINVAL;
            }
        }

    if ( NO_ERROR == ret )
        {
        mBracketingBuffersQueued[currentBufferIdx] = false;
        mBracketingBuffersQueuedCount--;

        if ( 0 >= mBracketingBuffersQueuedCount )
            {
            nextBufferIdx = ( currentBufferIdx + 1 ) % imgCaptureData->mNumBufs;
            mBracketingBuffersQueued[nextBufferIdx] = true;
            mBracketingBuffersQueuedCount++;
            mLastBracetingBufferIdx = nextBufferIdx;
            setFrameRefCount(imgCaptureData->mBufferHeader[nextBufferIdx]->pBuffer, typeOfFrame, 1);
            returnFrame(imgCaptureData->mBufferHeader[nextBufferIdx]->pBuffer, typeOfFrame);
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::sendBracketFrames()
{
    status_t ret = NO_ERROR;
    int currentBufferIdx;
    OMXCameraPortParameters * imgCaptureData = NULL;

    LOG_FUNCTION_NAME

    imgCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in executing state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {

        currentBufferIdx = mLastBracetingBufferIdx;
        do
            {
            currentBufferIdx++;
            currentBufferIdx %= imgCaptureData->mNumBufs;
            if (!mBracketingBuffersQueued[currentBufferIdx] )
                {
                CameraFrame cameraFrame;
                prepareFrame(imgCaptureData->mBufferHeader[currentBufferIdx], imgCaptureData->mImageType, imgCaptureData, cameraFrame);
                sendFrame(cameraFrame);
                }
            } while ( currentBufferIdx != mLastBracetingBufferIdx );

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::stopBracketing()
{
  status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mBracketingLock);

    if ( mBracketingEnabled )
        {

        if ( NULL != mBracketingBuffersQueued )
        {
            delete [] mBracketingBuffersQueued;
        }

        ret = stopImageCapture();
        mBracketingBuffersQueued = NULL;
        mBracketingEnabled = false;
        mBracketingBuffersQueuedCount = 0;
        mLastBracetingBufferIdx = 0;

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::takePicture()
{
    status_t ret = NO_ERROR;
    Message msg;

    LOG_FUNCTION_NAME

    msg.command = CommandHandler::CAMERA_START_IMAGE_CAPTURE;
    ret = mCommandHandler->put(&msg);

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::startImageCapture()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMXCameraPortParameters * capData = NULL;
    Semaphore eventSem;
    OMX_CONFIG_BOOLEANTYPE bOMX;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    if(!mCaptureConfigured)
        {
        ///Image capture was cancelled before we could start
        return NO_ERROR;
        }

    //During bracketing image capture is already active
    {
    Mutex::Autolock lock(mBracketingLock);
    if ( mBracketingEnabled )
        {
        //Stop bracketing, activate normal burst for the remaining images
        mBracketingEnabled = false;
        mCapturedFrames = mBracketingRange;
        ret = sendBracketFrames();
        goto EXIT;
        }
    }

    if ( NO_ERROR == ret )
        {
        ret = setPictureRotation(mPictureRotation);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error configuring image rotation %x", ret);
            }
        }

    //OMX shutter callback events are only available in hq mode
    if ( HIGH_QUALITY == mCapMode )
        {
        if ( NO_ERROR == ret )
            {
            ret = eventSem.Create(0);
            }

        if ( NO_ERROR == ret )
            {
            ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                        (OMX_EVENTTYPE) OMX_EventIndexSettingChanged,
                                        OMX_ALL,
                                        OMX_TI_IndexConfigShutterCallback,
                                        eventSem,
                                        -1 );
            }

        if ( NO_ERROR == ret )
            {
            ret = setShutterCallback(true);
            }

        }

    if ( NO_ERROR == ret )
        {
        capData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

        ///Queue all the buffers on capture port
        for ( int index = 0 ; index < capData->mNumBufs ; index++ )
            {
            CAMHAL_LOGDB("Queuing buffer on Capture port - 0x%x", ( unsigned int ) capData->mBufferHeader[index]->pBuffer);
            eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                        (OMX_BUFFERHEADERTYPE*)capData->mBufferHeader[index]);

            GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
            }

        mWaitingForSnapshot = true;
        mCapturing = true;
        mCaptureSignalled = false;

        OMX_INIT_STRUCT_PTR (&bOMX, OMX_CONFIG_BOOLEANTYPE);
        bOMX.bEnabled = OMX_TRUE;

        /// sending Capturing Command to the component
        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCapturing, &bOMX);

        CAMHAL_LOGDB("Capture set - 0x%x", eError);

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

        }

    //OMX shutter callback events are only available in hq mode
    if ( HIGH_QUALITY == mCapMode )
        {

        if ( NO_ERROR == ret )
            {
            ret = eventSem.Wait();
            }

        if ( NO_ERROR == ret )
            {
            CAMHAL_LOGDA("Autofocus callback received");
            ret = notifyShutterSubscribers();
            }

        }

    EXIT:

    if ( eError != OMX_ErrorNone )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }

        ret = -1;
        mWaitingForSnapshot = false;
        mCapturing = false;
        mCaptureSignalled = false;

        }
    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t OMXCameraAdapter::stopImageCapture()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError;
    OMX_CONFIG_BOOLEANTYPE bOMX;
    OMXCameraPortParameters *imgCaptureData = NULL;

    LOG_FUNCTION_NAME

    {
    Mutex::Autolock lock(mLock);

    if(!mCaptureConfigured)
        {
        //Capture is not ongoing, return from here
        return NO_ERROR;
        }

    if ( mCapturing )
    {
        //Disable the callback first
        mWaitingForSnapshot = false;
        mSnapshotCount = 0;
        mCapturing = false;

        //Disable the callback first
        ret = setShutterCallback(false);

        //Wait here for the capture to be done, in worst case timeout and proceed with cleanup
        mCaptureSem.WaitTimeout(IMAGE_CAPTURE_TIMEOUT);

        //Disable image capture
        OMX_INIT_STRUCT_PTR (&bOMX, OMX_CONFIG_BOOLEANTYPE);
        bOMX.bEnabled = OMX_FALSE;
        imgCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];
        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCapturing, &bOMX);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGDB("Error during SetConfig- 0x%x", eError);
            ret = -1;
            }

        CAMHAL_LOGDB("Capture set - 0x%x", eError);
     }

        mCaptureSignalled = true; //set this to true if we exited because of timeout
    }

    mCaptureConfigured = false;

    Semaphore camSem;

    camSem.Create();

    ///Register for Image port Disable event
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandPortDisable,
                                mCameraAdapterParameters.mImagePortIndex,
                                camSem,
                                -1);
    ///Disable Capture Port
    eError = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                OMX_CommandPortDisable,
                                mCameraAdapterParameters.mImagePortIndex,
                                NULL);
    ///Free all the buffers on capture port
    if (imgCaptureData)
    {
        CAMHAL_LOGDB("Freeing buffer on Capture port - %d", imgCaptureData->mNumBufs);
        for ( int index = 0 ; index < imgCaptureData->mNumBufs ; index++)
            {
            CAMHAL_LOGDB("Freeing buffer on Capture port - 0x%x", ( unsigned int ) imgCaptureData->mBufferHeader[index]->pBuffer);
            eError = OMX_FreeBuffer(mCameraAdapterParameters.mHandleComp,
                        mCameraAdapterParameters.mImagePortIndex,
                        (OMX_BUFFERHEADERTYPE*)imgCaptureData->mBufferHeader[index]);

            GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
            }
    }
    CAMHAL_LOGDA("Waiting for port disable");
    //Wait for the image port enable event
    camSem.Wait();
    CAMHAL_LOGDA("Port disabled");

    //Release image buffers
    if ( NULL != mReleaseImageBuffersCallback )
        {
        mReleaseImageBuffersCallback(mReleaseData);
        }
    //Signal end of image capture
    if ( NULL != mEndImageCaptureCallback)
        {
        mEndImageCaptureCallback(mEndCaptureData);
        }

    EXIT:

    if(eError != OMX_ErrorNone)
        {
        CAMHAL_LOGEB("Error occured when disabling image capture port %x",eError);

        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::startVideoCapture()
{
    return BaseCameraAdapter::startVideoCapture();
}

status_t OMXCameraAdapter::stopVideoCapture()
{
    return BaseCameraAdapter::stopVideoCapture();
}

//API to get the frame size required to be allocated. This size is used to override the size passed
//by camera service when VSTAB/VNF is turned ON for example
void OMXCameraAdapter::getFrameSize(int &width, int &height)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_RECTTYPE tFrameDim;

    LOG_FUNCTION_NAME

    OMX_INIT_STRUCT_PTR (&tFrameDim, OMX_CONFIG_RECTTYPE);
    tFrameDim.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

    if ( mOMXStateSwitch || mMeasurementEnabled )
        {

        ret = switchToLoaded();
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("switchToLoaded() failed 0x%x", ret);
            goto exit;
            }

        mOMXStateSwitch = false;
        }

    if ( OMX_StateLoaded == mComponentState )
        {

        ret = setLDC(mIPP);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("setLDC() failed %d", ret);
            LOG_FUNCTION_NAME_EXIT
            goto exit;
            }

        ret = setNSF(mIPP);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("setNSF() failed %d", ret);
            LOG_FUNCTION_NAME_EXIT
            goto exit;
            }

        if ( NO_ERROR == ret )
            {
            ret = setCaptureMode(mCapMode);
            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("setCaptureMode() failed %d", ret);
                }
            }

        if ( mCapMode == OMXCameraAdapter::VIDEO_MODE )
            {
            if ( NO_ERROR == ret )
                {
                ///Enable/Disable Video Noise Filter
                ret = enableVideoNoiseFilter(mVnfEnabled);
                }

            if ( NO_ERROR != ret)
                {
                CAMHAL_LOGEB("Error configuring VNF %x", ret);
                }

            if ( NO_ERROR == ret )
                {
                ///Enable/Disable Video Stabilization
                ret = enableVideoStabilization(mVstabEnabled);
                }

            if ( NO_ERROR != ret)
                {
                CAMHAL_LOGEB("Error configuring VSTAB %x", ret);
                }
             }
        else
            {
            if ( NO_ERROR == ret )
                {
                ///Enable/Disable Video Noise Filter
                ret = enableVideoNoiseFilter(false);
                }

            if ( NO_ERROR != ret)
                {
                CAMHAL_LOGEB("Error configuring VNF %x", ret);
                }

            if ( NO_ERROR == ret )
                {
                ///Enable/Disable Video Stabilization
                ret = enableVideoStabilization(false);
                }

            if ( NO_ERROR != ret)
                {
                CAMHAL_LOGEB("Error configuring VSTAB %x", ret);
                }
            }

        }

    ret = setSensorOrientation(mSensorOrientation);
    if ( NO_ERROR != ret )
        {
        CAMHAL_LOGEB("Error configuring Sensor Orientation %x", ret);
        mSensorOrientation = 0;
        }

    if ( NO_ERROR == ret )
        {
        eError = OMX_GetParameter(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_TI_IndexParam2DBufferAllocDimension, &tFrameDim);
        if ( OMX_ErrorNone == eError)
            {
            width = tFrameDim.nWidth;
            height = tFrameDim.nHeight;
            }
       else
            {
            width = -1;
            height = -1;
            }
        }
    else
        {
        width = -1;
        height = -1;
        }
exit:

    CAMHAL_LOGDB("Required frame size %dx%d", width, height);

    if ( OMX_ErrorNone != eError )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    LOG_FUNCTION_NAME_EXIT
}

status_t OMXCameraAdapter::getFrameDataSize(size_t &dataFrameSize, size_t bufferCount)
{
    status_t ret = NO_ERROR;
    OMX_PARAM_PORTDEFINITIONTYPE portCheck;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    LOG_FUNCTION_NAME

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("Calling getFrameDataSize() when not in LOADED state");
        dataFrameSize = 0;
        ret = -1;
        }

    if ( NO_ERROR == ret  )
        {
        OMX_INIT_STRUCT_PTR(&portCheck, OMX_PARAM_PORTDEFINITIONTYPE);
        portCheck.nPortIndex = mCameraAdapterParameters.mMeasurementPortIndex;

        eError = OMX_GetParameter(mCameraAdapterParameters.mHandleComp, OMX_IndexParamPortDefinition, &portCheck);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("OMX_GetParameter on OMX_IndexParamPortDefinition returned: 0x%x", eError);
            dataFrameSize = 0;
            ret = -1;
            }
        }

    if ( NO_ERROR == ret )
        {
        portCheck.nBufferCountActual = bufferCount;
        eError = OMX_SetParameter(mCameraAdapterParameters.mHandleComp, OMX_IndexParamPortDefinition, &portCheck);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("OMX_SetParameter on OMX_IndexParamPortDefinition returned: 0x%x", eError);
            dataFrameSize = 0;
            ret = -1;
            }
        }

    if ( NO_ERROR == ret  )
        {
        eError = OMX_GetParameter(mCameraAdapterParameters.mHandleComp, OMX_IndexParamPortDefinition, &portCheck);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("OMX_GetParameter on OMX_IndexParamPortDefinition returned: 0x%x", eError);
            ret = -1;
            }
        else
            {
            mCameraAdapterParameters.mCameraPortParams[portCheck.nPortIndex].mBufSize = portCheck.nBufferSize;
            dataFrameSize = portCheck.nBufferSize;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::getPictureBufferSize(size_t &length, size_t bufferCount)
{
    status_t ret = NO_ERROR;
    OMXCameraPortParameters *imgCaptureData = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    LOG_FUNCTION_NAME

    if ( mCapturing )
        {
        CAMHAL_LOGEA("getPictureBufferSize() called during image capture");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        imgCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

        imgCaptureData->mNumBufs = bufferCount;
        ret = setFormat(OMX_CAMERA_PORT_IMAGE_OUT_IMAGE, *imgCaptureData);
        if ( ret == NO_ERROR )
            {
            length = imgCaptureData->mBufSize;
            }
        else
            {
            CAMHAL_LOGEB("setFormat() failed 0x%x", ret);
            length = 0;
            }
        }

    CAMHAL_LOGDB("getPictureBufferSize %d", length);

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

/* Application callback Functions */
/*========================================================*/
/* @ fn SampleTest_EventHandler :: Application callback   */
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapterEventHandler(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_PTR pAppData,
                                          OMX_IN OMX_EVENTTYPE eEvent,
                                          OMX_IN OMX_U32 nData1,
                                          OMX_IN OMX_U32 nData2,
                                          OMX_IN OMX_PTR pEventData)
{
    LOG_FUNCTION_NAME

    CAMHAL_LOGDB("Event %d", eEvent);

    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMXCameraAdapter *oca = (OMXCameraAdapter*)pAppData;
    ret = oca->OMXCameraAdapterEventHandler(hComponent, eEvent, nData1, nData2, pEventData);

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

/* Application callback Functions */
/*========================================================*/
/* @ fn SampleTest_EventHandler :: Application callback   */
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapter::OMXCameraAdapterEventHandler(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_EVENTTYPE eEvent,
                                          OMX_IN OMX_U32 nData1,
                                          OMX_IN OMX_U32 nData2,
                                          OMX_IN OMX_PTR pEventData)
{

    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    CAMHAL_LOGDB("+OMX_Event %x, %d %d", eEvent, (int)nData1, (int)nData2);

    switch (eEvent) {
        case OMX_EventCmdComplete:
            CAMHAL_LOGDB("+OMX_EventCmdComplete %d %d", (int)nData1, (int)nData2);

            if (OMX_CommandStateSet == nData1) {
                mCameraAdapterParameters.mState = (OMX_STATETYPE) nData2;

            } else if (OMX_CommandFlush == nData1) {
                CAMHAL_LOGDB("OMX_CommandFlush received for port %d", (int)nData2);

            } else if (OMX_CommandPortDisable == nData1) {
                CAMHAL_LOGDB("OMX_CommandPortDisable received for port %d", (int)nData2);

            } else if (OMX_CommandPortEnable == nData1) {
                CAMHAL_LOGDB("OMX_CommandPortEnable received for port %d", (int)nData2);

            } else if (OMX_CommandMarkBuffer == nData1) {
                ///This is not used currently
            }

            CAMHAL_LOGDA("-OMX_EventCmdComplete");
        break;

        case OMX_EventIndexSettingChanged:
            CAMHAL_LOGDB("OMX_EventIndexSettingChanged event received data1 0x%x, data2 0x%x",
                            ( unsigned int ) nData1, ( unsigned int ) nData2);
            break;

        case OMX_EventError:
            CAMHAL_LOGDB("OMX interface failed to execute OMX command %d", (int)nData1);
            CAMHAL_LOGDA("See OMX_INDEXTYPE for reference");
            if ( NULL != mErrorNotifier && ( ( OMX_U32 ) OMX_ErrorHardware == nData1 ) && mComponentState != OMX_StateInvalid)
              {
                LOGD("***Got Fatal Error Notification***\n");
                mComponentState = OMX_StateInvalid;
                ///Report Error to App
                mErrorNotifier->errorNotify((int)nData1);
              }
            break;

        case OMX_EventMark:
        break;

        case OMX_EventPortSettingsChanged:
        break;

        case OMX_EventBufferFlag:
        break;

        case OMX_EventResourcesAcquired:
        break;

        case OMX_EventComponentResumed:
        break;

        case OMX_EventDynamicResourcesAvailable:
        break;

        case OMX_EventPortFormatDetected:
        break;

        default:
        break;
    }

    ///Signal to the thread(s) waiting that the event has occured
    SignalEvent(hComponent, eEvent, nData1, nData2, pEventData);

   LOG_FUNCTION_NAME_EXIT
   return eError;

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of eError=%x", __FUNCTION__, eError);
    LOG_FUNCTION_NAME_EXIT
    return eError;
}

OMX_ERRORTYPE OMXCameraAdapter::SignalEvent(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_EVENTTYPE eEvent,
                                          OMX_IN OMX_U32 nData1,
                                          OMX_IN OMX_U32 nData2,
                                          OMX_IN OMX_PTR pEventData)
{
    Mutex::Autolock lock(mEventLock);
    Message *msg;

    LOG_FUNCTION_NAME

    if ( !mEventSignalQ.isEmpty() )
        {
        CAMHAL_LOGDA("Event queue not empty");

        for ( unsigned int i = 0 ; i < mEventSignalQ.size() ; i++ )
            {
            msg = mEventSignalQ.itemAt(i);
            if ( NULL != msg )
                {
                if( ( msg->command != 0 || msg->command == ( unsigned int ) ( eEvent ) )
                    && ( !msg->arg1 || ( OMX_U32 ) msg->arg1 == nData1 )
                    && ( !msg->arg2 || ( OMX_U32 ) msg->arg2 == nData2 )
                    && msg->arg3)
                    {
                    Semaphore *sem  = (Semaphore*) msg->arg3;
                    CAMHAL_LOGDA("Event matched, signalling sem");
                    mEventSignalQ.removeAt(i);
                    //Signal the semaphore provided
                    sem->Signal();
                    free(msg);
                    break;
                    }
                }
            }
        }
    else
        {
        CAMHAL_LOGEA("Event queue empty!!!");
        }

    LOG_FUNCTION_NAME_EXIT

    return OMX_ErrorNone;
}

status_t OMXCameraAdapter::RegisterForEvent(OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN OMX_EVENTTYPE eEvent,
                                          OMX_IN OMX_U32 nData1,
                                          OMX_IN OMX_U32 nData2,
                                          OMX_IN Semaphore &semaphore,
                                          OMX_IN OMX_U32 timeout)
{
    status_t ret = NO_ERROR;
    ssize_t res;
    Mutex::Autolock lock(mEventLock);

    LOG_FUNCTION_NAME

    Message * msg = ( struct Message * ) malloc(sizeof(struct Message));
    if ( NULL != msg )
        {
        msg->command = ( unsigned int ) eEvent;
        msg->arg1 = ( void * ) nData1;
        msg->arg2 = ( void * ) nData2;
        msg->arg3 = ( void * ) &semaphore;
        msg->arg4 =  ( void * ) hComponent;
        res = mEventSignalQ.add(msg);
        if ( NO_MEMORY == res )
            {
            CAMHAL_LOGEA("No ressources for inserting OMX events");
            ret = -ENOMEM;
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

/*========================================================*/
/* @ fn SampleTest_EmptyBufferDone :: Application callback*/
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapterEmptyBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_PTR pAppData,
                                   OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader)
{
    LOG_FUNCTION_NAME

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMXCameraAdapter *oca = (OMXCameraAdapter*)pAppData;
    eError = oca->OMXCameraAdapterEmptyBufferDone(hComponent, pBuffHeader);

    LOG_FUNCTION_NAME_EXIT
    return eError;
}


/*========================================================*/
/* @ fn SampleTest_EmptyBufferDone :: Application callback*/
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapter::OMXCameraAdapterEmptyBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader)
{

   LOG_FUNCTION_NAME

   LOG_FUNCTION_NAME_EXIT

   return OMX_ErrorNone;
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
        LOGD("Camera %d Frames, %f FPS", mFrameCount, mFps);
    }
    // XXX: mFPS has the value we want
}

/*========================================================*/
/* @ fn SampleTest_FillBufferDone ::  Application callback*/
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapterFillBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_PTR pAppData,
                                   OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (UNLIKELY(mDebugFps)) {
        debugShowFPS();
    }

    OMXCameraAdapter *oca = (OMXCameraAdapter*)pAppData;
    eError = oca->OMXCameraAdapterFillBufferDone(hComponent, pBuffHeader);

    return eError;
}

/*========================================================*/
/* @ fn SampleTest_FillBufferDone ::  Application callback*/
/*========================================================*/
OMX_ERRORTYPE OMXCameraAdapter::OMXCameraAdapterFillBufferDone(OMX_IN OMX_HANDLETYPE hComponent,
                                   OMX_IN OMX_BUFFERHEADERTYPE* pBuffHeader)
{

    status_t  stat = NO_ERROR;
    status_t  res1, res2;
    OMXCameraPortParameters  *pPortParam;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    CameraFrame::FrameType typeOfFrame = CameraFrame::ALL_FRAMES;
    unsigned int refCount = 0;

    res1 = res2 = -1;
    pPortParam = &(mCameraAdapterParameters.mCameraPortParams[pBuffHeader->nOutputPortIndex]);
    if (pBuffHeader->nOutputPortIndex == OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW)
        {

        if ( !mPreviewing )
            {
            return OMX_ErrorNone;
            }

        recalculateFPS();

            {
            Mutex::Autolock lock(mFaceDetectionLock);
            if ( mFaceDetectionRunning )
                {
                detectFaces(pBuffHeader);
                CAMHAL_LOGVB("Faces detected: %s", mFaceDectionResult);
                }
            }

        stat |= advanceZoom();

        ///On the fly update to 3A settings not working
        if( mPending3Asettings )
            {
            apply3Asettings(mParameters3A);
            }

        ///Prepare the frames to be sent - initialize CameraFrame object and reference count
        CameraFrame cameraFrameVideo, cameraFramePreview;
        if ( mRecording )
            {
            res1 = prepareFrame(pBuffHeader, CameraFrame::VIDEO_FRAME_SYNC, pPortParam, cameraFrameVideo);
            }

        if( mWaitingForSnapshot )
            {
            typeOfFrame = CameraFrame::SNAPSHOT_FRAME;
            }
        else
            {
            typeOfFrame = CameraFrame::PREVIEW_FRAME_SYNC;
            }

        res2 = prepareFrame(pBuffHeader, typeOfFrame, pPortParam, cameraFramePreview);

        stat |= ( ( NO_ERROR == res1 ) || ( NO_ERROR == res2 ) ) ? ( ( int ) NO_ERROR ) : ( -1 );

        if ( mRecording )
            {
            res1  = sendFrame(cameraFrameVideo);
            }

        if( mWaitingForSnapshot )
            {
            mSnapshotCount++;

            if (  ( mSnapshotCount == 1 ) &&
                ( HIGH_SPEED == mCapMode ) )
                {
                notifyShutterSubscribers();
                }
            }

        res2 = sendFrame(cameraFramePreview);

        stat |= ( ( NO_ERROR == res1 ) || ( NO_ERROR == res2 ) ) ? ( ( int ) NO_ERROR ) : ( -1 );

        }
    else if (pBuffHeader->nOutputPortIndex == OMX_CAMERA_PORT_VIDEO_OUT_MEASUREMENT)
        {
        typeOfFrame = CameraFrame::FRAME_DATA_SYNC;
        CameraFrame cameraFrame;

        if (mSensorIndex == OMX_TI_StereoSensor)
        {
            OMXDebugDataHeader *sMALCDataHeader;
            OMX_U32 bufferEnd = (OMX_U32)pBuffHeader->pBuffer + pBuffHeader->nFilledLen - sizeof(OMXDebugDataHeader);
            sMALCDataHeader = (OMXDebugDataHeader*) ((OMX_U32)pBuffHeader->pBuffer + pBuffHeader->nOffset);

            Mutex::Autolock lock(mSMALCLock);

            while ( ((OMX_U32)sMALCDataHeader < bufferEnd)
                && ((OMX_U32)pBuffHeader->pBuffer <= (OMX_U32)sMALCDataHeader)
                && (((OMX_U32)sMALCDataHeader & 0x3) == 0)
                && (sMALCDataHeader->mPayloadSize != 0) )
            {
                if (DBG_SMALC_CFG == sMALCDataHeader->mRecordID)
                {
                    if (mSMALCDataSize != sMALCDataHeader->mPayloadSize)
                    {
                        mSMALCDataSize = sMALCDataHeader->mPayloadSize;
                        if (mSMALCDataRecord)
                            free(mSMALCDataRecord);
                        mSMALCDataRecord = (int*)malloc(mSMALCDataSize);
                        if (!mSMALCDataRecord) {
                            CAMHAL_LOGEA("ERROR: Allocating memory for SMALC data failed");
                            return OMX_ErrorInsufficientResources;
                        }
                    }
                    memcpy(mSMALCDataRecord,
                            (void *)((OMX_U32)sMALCDataHeader +
                            sMALCDataHeader->mHeaderSize), mSMALCDataSize);
                }
                else if (DBG_SMALC_DCC_FILE_DESC == sMALCDataHeader->mRecordID)
                {
                    if (mSMALC_DCCDescSize != sMALCDataHeader->mPayloadSize)
                    {
                        mSMALC_DCCDescSize = sMALCDataHeader->mPayloadSize;
                        if (mSMALC_DCCFileDesc)
                            free(mSMALC_DCCFileDesc);
                        mSMALC_DCCFileDesc = (int*)malloc(mSMALC_DCCDescSize);
                        if (!mSMALC_DCCFileDesc) {
                            CAMHAL_LOGEA("ERROR: Allocating memory for SMALC data failed");
                            return OMX_ErrorInsufficientResources;
                        }
                    }
                    memcpy(mSMALC_DCCFileDesc,
                            (void *)((OMX_U32)sMALCDataHeader +
                            sMALCDataHeader->mHeaderSize), mSMALC_DCCDescSize);
                }
                else if (DML_END_MARKER == sMALCDataHeader->mRecordID)
                {
                    break;
                }
                else
                {
                    HERD(pBuffHeader->nFilledLen);
                    HERD(pBuffHeader->pBuffer);
                    HERD(pBuffHeader->nOffset);
                    HERD(sMALCDataHeader);

                    HERD(sMALCDataHeader->mPayloadSize);
                    HERD(sMALCDataHeader->mRecordSize);
                    HERD(sMALCDataHeader->mRecordID);
                    HERD(sMALCDataHeader->mSectionID);
                }

                sMALCDataHeader = (OMXDebugDataHeader *)((OMX_U32)sMALCDataHeader + sMALCDataHeader->mRecordSize);
            }
        }

        stat |= prepareFrame(pBuffHeader, typeOfFrame, pPortParam, cameraFrame);
        stat |= sendFrame(cameraFrame);
       }
    else if (pBuffHeader->nOutputPortIndex == OMX_CAMERA_PORT_IMAGE_OUT_IMAGE)
        {

        if ( OMX_COLOR_FormatUnused == mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex].mColorFormat )
            {
            typeOfFrame = CameraFrame::IMAGE_FRAME;
            }
        else
            {
            typeOfFrame = CameraFrame::RAW_FRAME;
            }

            pPortParam->mImageType = typeOfFrame;

            if((mCapturedFrames>0) && !mCaptureSignalled)
                {
                mCaptureSignalled = true;
                mCaptureSem.Signal();
                }

            {
             Mutex::Autolock lock(mLock);
            if(!mCapturing)
                {
                goto EXIT;
                }
             }

            {
            Mutex::Autolock lock(mBracketingLock);
            if ( mBracketingEnabled )
                {
                doBracketing(pBuffHeader, typeOfFrame);
                return eError;
                }
            }

        if ( 1 > mCapturedFrames )
            {
            goto EXIT;
            }

        CAMHAL_LOGDB("Captured Frames: %d", mCapturedFrames);

        mCapturedFrames--;

        //The usual jpeg capture does not include raw data.
        //Use empty raw frames intead.
        if ( ( CodingNone == mCodingMode ) &&
             ( typeOfFrame == CameraFrame::IMAGE_FRAME ) )
            {
            sendEmptyRawFrame();
            }


        CameraFrame cameraFrame;
        stat |= prepareFrame(pBuffHeader, typeOfFrame, pPortParam, cameraFrame);
        stat |= sendFrame(cameraFrame);

        }
    else
        {
        CAMHAL_LOGEA("Frame received for non-(preview/capture/measure) port. This is yet to be supported");
        goto EXIT;
        }

    if ( NO_ERROR != stat )
        {
        CAMHAL_LOGDB("sendFrameToSubscribers error: %d", stat);

        returnFrame(pBuffHeader->pBuffer, typeOfFrame);
        }

    return eError;

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, stat, eError);

    if ( NO_ERROR != stat )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    return eError;
}

status_t OMXCameraAdapter::advanceZoom()
{
    status_t ret = NO_ERROR;
    Mutex::Autolock lock(mZoomLock);

    if ( mReturnZoomStatus )
        {
        mTargetZoomIdx = mCurrentZoomIdx;
        mReturnZoomStatus = false;
        mSmoothZoomEnabled = false;
        ret = doZoom(mCurrentZoomIdx);
        notifyZoomSubscribers(mCurrentZoomIdx, true);
        }
    else if ( mCurrentZoomIdx != mTargetZoomIdx )
        {
        if ( mSmoothZoomEnabled )
            {
            if ( mCurrentZoomIdx < mTargetZoomIdx )
                {
                mZoomInc = 1;
                }
            else
                {
                mZoomInc = -1;
                }

            mCurrentZoomIdx += mZoomInc;
            }
        else
            {
            mCurrentZoomIdx = mTargetZoomIdx;
            }

        ret = doZoom(mCurrentZoomIdx);

        if ( mSmoothZoomEnabled )
            {
            if ( mCurrentZoomIdx == mTargetZoomIdx )
                {
                CAMHAL_LOGDB("[Goal Reached] Smooth Zoom notify currentIdx = %d, targetIdx = %d", mCurrentZoomIdx, mTargetZoomIdx);
                mSmoothZoomEnabled = false;
                notifyZoomSubscribers(mCurrentZoomIdx, true);
                }
            else
                {
                CAMHAL_LOGDB("[Advancing] Smooth Zoom notify currentIdx = %d, targetIdx = %d", mCurrentZoomIdx, mTargetZoomIdx);
                notifyZoomSubscribers(mCurrentZoomIdx, false);
                }
            }
        }
    else if ( mSmoothZoomEnabled )
        {
        mSmoothZoomEnabled = false;
        }

    return ret;
}

status_t OMXCameraAdapter::recalculateFPS()
{
    float currentFPS;

    mFrameCount++;

    if ( ( mFrameCount % FPS_PERIOD ) == 0 )
        {
        nsecs_t now = systemTime();
        nsecs_t diff = now - mLastFPSTime;
        currentFPS =  ((mFrameCount - mLastFrameCount) * float(s2ns(1))) / diff;
        mLastFPSTime = now;
        mLastFrameCount = mFrameCount;

        if ( 1 == mIter )
            {
            mFPS = currentFPS;
            }
        else
            {
            //cumulative moving average
            mFPS = mLastFPS + (currentFPS - mLastFPS)/mIter;
            }

        mLastFPS = mFPS;
        mIter++;
        }

    return NO_ERROR;
}

status_t OMXCameraAdapter::sendEmptyRawFrame()
{
    status_t ret = NO_ERROR;
    CameraFrame frame;

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(&frame, 0, sizeof(CameraFrame));
        frame.mFrameType = CameraFrame::RAW_FRAME;
        ret = sendFrameToSubscribers(&frame);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}


status_t OMXCameraAdapter::prepareFrame(OMX_IN OMX_BUFFERHEADERTYPE *pBuffHeader,
                                      int typeOfFrame,
                                      OMXCameraPortParameters *port, CameraFrame &frame)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME


    if ( NO_ERROR == ret )
        {
        ret = initCameraFrame(frame, pBuffHeader, typeOfFrame, port);
        }

    if ( NO_ERROR == ret )
        {
        ret = resetFrameRefCount(frame);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;

}

status_t OMXCameraAdapter::sendFrame(CameraFrame &frame)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME


    if ( NO_ERROR == ret )
        {
        ret = sendFrameToSubscribers(&frame);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::initCameraFrame( CameraFrame &frame,
                                            OMX_IN OMX_BUFFERHEADERTYPE *pBuffHeader,
                                            int typeOfFrame,
                                            OMXCameraPortParameters *port)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NULL == port)
        {
        CAMHAL_LOGEA("Invalid portParam");
        ret = -EINVAL;
        }

    if ( NULL == pBuffHeader )
        {
        CAMHAL_LOGEA("Invalid Buffer header");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        frame.mFrameType = typeOfFrame;
        frame.mBuffer = pBuffHeader->pBuffer;
        frame.mLength = pBuffHeader->nFilledLen;
        frame.mAlignment = port->mStride;
        frame.mOffset = pBuffHeader->nOffset;
        frame.mWidth = port->mWidth;
        frame.mHeight = port->mHeight;

        // Calculating the time source delta of Ducati & system time only once at the start of camera.
        // It's seen that there is a one-time constant diff between the ducati source clock &
        // System monotonic timer, although both derived from the same 32KHz clock.
        // This delta is offsetted to/from ducati timestamp to match with system time so that
        // video timestamps are aligned with Audio with a periodic timestamp intervals.
        if ( onlyOnce )
            {
            mTimeSourceDelta = (pBuffHeader->nTimeStamp * 1000) - systemTime(SYSTEM_TIME_MONOTONIC);
            onlyOnce = false;
            }

        // Calculating the new video timestamp based on offset from ducati source.
        frame.mTimestamp = (pBuffHeader->nTimeStamp * 1000) - mTimeSourceDelta;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

OMX_ERRORTYPE OMXCameraAdapter::apply3Asettings( Gen3A_settings& Gen3A )
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    unsigned int currSett; // 32 bit
    int portIndex;

    /*
     * Scenes have a priority during the process
     * of applying 3A related parameters.
     * They can override pretty much all other 3A
     * settings and similarly get overridden when
     * for instance the focus mode gets switched.
     * There is only one exception to this rule,
     * the manual a.k.a. auto scene.
     */
    if ( ( SetSceneMode & mPending3Asettings ) )
        {
        mPending3Asettings &= ~SetSceneMode;
        return setScene(Gen3A);
        }
    else if ( OMX_Manual != Gen3A.SceneMode )
        {
        mPending3Asettings = 0;
        return OMX_ErrorNone;
        }

    for( currSett = 1; currSett < E3aSettingMax; currSett <<= 1)
        {
        if( currSett & mPending3Asettings )
            {
            switch( currSett )
                {
                case SetEVCompensation:
                    {
                    OMX_CONFIG_EXPOSUREVALUETYPE expValues;
                    OMX_INIT_STRUCT_PTR (&expValues, OMX_CONFIG_EXPOSUREVALUETYPE);
                    expValues.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

                    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &expValues);
                    CAMHAL_LOGDB("old EV Compensation for OMX = 0x%x", (int)expValues.xEVCompensation);
                    CAMHAL_LOGDB("EV Compensation for HAL = %d", Gen3A.EVCompensation);

                    expValues.xEVCompensation = ( Gen3A.EVCompensation * ( 1 << Q16_OFFSET ) )  / 10;
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &expValues);
                    CAMHAL_LOGDB("new EV Compensation for OMX = 0x%x", (int)expValues.xEVCompensation);
                    break;
                    }

                case SetWhiteBallance:
                    {


                    if ( WB_FACE_PRIORITY == Gen3A.WhiteBallance )
                        {
                        //Disable Region priority and enable Face priority
                        setAlgoPriority(REGION_PRIORITY, WHITE_BALANCE_ALGO, false);
                        setAlgoPriority(FACE_PRIORITY, WHITE_BALANCE_ALGO, true);

                        //Then set the mode to auto
                        Gen3A.WhiteBallance = OMX_WhiteBalControlAuto;
                        }
                    else
                        {
                        //Disable Face and Region priority
                        setAlgoPriority(FACE_PRIORITY, WHITE_BALANCE_ALGO, false);
                        setAlgoPriority(REGION_PRIORITY, WHITE_BALANCE_ALGO, false);
                        }

                    OMX_CONFIG_WHITEBALCONTROLTYPE wb;
                    OMX_INIT_STRUCT_PTR (&wb, OMX_CONFIG_WHITEBALCONTROLTYPE);
                    wb.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
                    wb.eWhiteBalControl = (OMX_WHITEBALCONTROLTYPE)Gen3A.WhiteBallance;

                    CAMHAL_LOGDB("White Ballance for Hal = %d", Gen3A.WhiteBallance);
                    CAMHAL_LOGDB("White Ballance for OMX = %d", (int)wb.eWhiteBalControl);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonWhiteBalance, &wb);
                    break;
                    }

                case SetFlicker:
                    {
                    OMX_CONFIG_FLICKERCANCELTYPE flicker;
                    OMX_INIT_STRUCT_PTR (&flicker, OMX_CONFIG_FLICKERCANCELTYPE);
                    flicker.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
                    flicker.eFlickerCancel = (OMX_COMMONFLICKERCANCELTYPE)Gen3A.Flicker;

                    CAMHAL_LOGDB("Flicker for Hal = %d", Gen3A.Flicker);
                    CAMHAL_LOGDB("Flicker for  OMX= %d", (int)flicker.eFlickerCancel);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexConfigFlickerCancel, &flicker );
                    break;
                    }

                case SetBrightness:
                    {
                    OMX_CONFIG_BRIGHTNESSTYPE brightness;
                    OMX_INIT_STRUCT_PTR (&brightness, OMX_CONFIG_BRIGHTNESSTYPE);
                    brightness.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
                    brightness.nBrightness = Gen3A.Brightness;

                    CAMHAL_LOGDB("Brightness for Hal and OMX= %d", (int)Gen3A.Brightness);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonBrightness, &brightness);
                    break;
                    }

                case SetContrast:
                    {
                    OMX_CONFIG_CONTRASTTYPE contrast;
                    OMX_INIT_STRUCT_PTR (&contrast, OMX_CONFIG_CONTRASTTYPE);
                    contrast.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
                    contrast.nContrast = Gen3A.Contrast;

                    CAMHAL_LOGDB("Contrast for Hal and OMX= %d", (int)Gen3A.Contrast);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonContrast, &contrast);
                    break;
                    }

                case SetSharpness:
                    {
                    OMX_IMAGE_CONFIG_PROCESSINGLEVELTYPE procSharpness;
                    OMX_INIT_STRUCT_PTR (&procSharpness, OMX_IMAGE_CONFIG_PROCESSINGLEVELTYPE);
                    procSharpness.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
                    procSharpness.nLevel = Gen3A.Sharpness;

                    if( procSharpness.nLevel == 0 )
                        {
                        procSharpness.bAuto = OMX_TRUE;
                        }
                    else
                        {
                        procSharpness.bAuto = OMX_FALSE;
                        }

                    CAMHAL_LOGDB("Sharpness for Hal and OMX= %d", (int)Gen3A.Sharpness);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexConfigSharpeningLevel, &procSharpness);
                    break;
                    }

                case SetSaturation:
                    {
                    OMX_CONFIG_SATURATIONTYPE saturation;
                    OMX_INIT_STRUCT_PTR (&saturation, OMX_CONFIG_SATURATIONTYPE);
                    saturation.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
                    saturation.nSaturation = Gen3A.Saturation;

                    CAMHAL_LOGDB("Saturation for Hal and OMX= %d", (int)Gen3A.Saturation);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonSaturation, &saturation);
                    break;
                    }

                case SetISO:
                    {
                    OMX_CONFIG_EXPOSUREVALUETYPE expValues;
                    OMX_INIT_STRUCT_PTR (&expValues, OMX_CONFIG_EXPOSUREVALUETYPE);
                    expValues.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

                    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &expValues);
                    if( 0 == Gen3A.ISO )
                        {
                        expValues.bAutoSensitivity = OMX_TRUE;
                        }
                    else
                        {
                        expValues.bAutoSensitivity = OMX_FALSE;
                        expValues.nSensitivity = Gen3A.ISO;
                        }
                    CAMHAL_LOGDB("ISO for Hal and OMX= %d", (int)Gen3A.ISO);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &expValues);
                    }
                    break;

                case SetEffect:
                    {
                    OMX_CONFIG_IMAGEFILTERTYPE effect;
                    OMX_INIT_STRUCT_PTR (&effect, OMX_CONFIG_IMAGEFILTERTYPE);
                    effect.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;
                    effect.eImageFilter = (OMX_IMAGEFILTERTYPE)Gen3A.Effect;

                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonImageFilter, &effect);
                    CAMHAL_LOGDB("effect for OMX = 0x%x", (int)effect.eImageFilter);
                    CAMHAL_LOGDB("effect for Hal = %d", Gen3A.Effect);
                    break;
                    }

                case SetFocus:
                    {

                    //First Disable Face and Region priority
                    setAlgoPriority(FACE_PRIORITY, FOCUS_ALGO, false);
                    setAlgoPriority(REGION_PRIORITY, FOCUS_ALGO, false);

                    OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE focus;
                    OMX_INIT_STRUCT_PTR (&focus, OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE);
                    focus.nPortIndex = mCameraAdapterParameters.mPrevPortIndex;

                    focus.eFocusControl = (OMX_IMAGE_FOCUSCONTROLTYPE)Gen3A.Focus;

                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigFocusControl, &focus);
                    CAMHAL_LOGDB("Focus type in hal , OMX : %d , 0x%x", Gen3A.Focus, focus.eFocusControl );

                    break;
                    }

                case SetExpMode:
                    {
                    ret = setExposureMode(Gen3A);
                    break;
                    }
                case SetManualExposure:
                    {
                    setManualExposure (Gen3A);
                    break;
                    }
                case SetManualGain:
                    {
                    setManualGain (Gen3A);
                    break;
                    }
                case SetFlash:
                    {
                    ret = setFlashMode(mParameters3A);
                    break;
                    }

                default:
                    CAMHAL_LOGEB("this setting (0x%x) is still not supported in CameraAdapter ", currSett);
                    break;
                }
                mPending3Asettings &= ~currSett;
            }
        }

        return ret;
}

int OMXCameraAdapter::getLUTvalue_HALtoOMX(const char * HalValue, LUTtype LUT)
{
    int LUTsize = LUT.size;
    if( HalValue )
        for(int i = 0; i < LUTsize; i++)
            if( 0 == strcmp(LUT.Table[i].userDefinition, HalValue) )
                return LUT.Table[i].omxDefinition;

    return -1;
}

const char* OMXCameraAdapter::getLUTvalue_OMXtoHAL(int OMXValue, LUTtype LUT)
{
    int LUTsize = LUT.size;
    for(int i = 0; i < LUTsize; i++)
        if( LUT.Table[i].omxDefinition == OMXValue )
            return LUT.Table[i].userDefinition;

    return NULL;
}

status_t OMXCameraAdapter::setAutoConvergence(OMX_TI_AUTOCONVERGENCEMODETYPE pACMode, OMX_S32 pManualConverence)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_CONFIG_CONVERGENCETYPE ACParams;

    LOG_FUNCTION_NAME

    ACParams.nSize = (OMX_U32)sizeof(OMX_TI_CONFIG_CONVERGENCETYPE);
    ACParams.nVersion = mLocalVersionParam;
    ACParams.nPortIndex = OMX_ALL;

    OMX_GetConfig(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_TI_IndexConfigAutoConvergence, &ACParams);

    OMXCameraPortParameters *mPreviewData;
    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

    OMX_U32 w = mPreviewData->mWidth;
    OMX_U32 h = mPreviewData->mHeight;

    if (mS3DImageFormat == S3D_TB_FULL) {
        h = h / 2;
    } else if (mS3DImageFormat == S3D_SS_FULL) {
        w = w / 2;
    }

    ACParams.nManualConverence = pManualConverence;
    ACParams.eACMode = pACMode;
    ACParams.nACProcWinStartX = (OMX_U32) mTouchPosX;
    ACParams.nACProcWinStartY = (OMX_U32) mTouchPosY;
    ACParams.nACProcWinWidth = (OMX_U32) w;
    ACParams.nACProcWinHeight = (OMX_U32) h;

    CAMHAL_LOGDB("nSize %d", ACParams.nSize);
    CAMHAL_LOGDB("nPortIndex %d", (int)ACParams.nPortIndex);
    CAMHAL_LOGDB("nManualConverence %d", (int)ACParams.nManualConverence);
    CAMHAL_LOGDB("eACMode %d", (int)ACParams.eACMode);
    CAMHAL_LOGDB("nACProcWinStartX %d", (int)ACParams.nACProcWinStartX);
    CAMHAL_LOGDB("nACProcWinStartY %d", (int)ACParams.nACProcWinStartY);
    CAMHAL_LOGDB("nACProcWinWidth %d", (int)ACParams.nACProcWinWidth);
    CAMHAL_LOGDB("nACProcWinHeight %d", (int)ACParams.nACProcWinHeight);
    CAMHAL_LOGDB("bACStatus %d", (int)ACParams.bACStatus);

    eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_TI_IndexConfigAutoConvergence, &ACParams);
    if ( eError != OMX_ErrorNone )
        {
        CAMHAL_LOGEB("Error while setting AutoConvergence 0x%x", eError);
        ret = -EINVAL;
        }
    else
        {
        CAMHAL_LOGDA("AutoConvergence applied successfully");
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

// Get AutoConvergence
status_t OMXCameraAdapter::getAutoConvergence(OMX_TI_AUTOCONVERGENCEMODETYPE *pACMode, OMX_S32 *pManualConverence)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_CONFIG_CONVERGENCETYPE ACParams;

    ACParams.nSize = sizeof(OMX_TI_CONFIG_CONVERGENCETYPE);
    ACParams.nVersion = mLocalVersionParam;
    ACParams.nPortIndex = OMX_ALL;

    LOG_FUNCTION_NAME

    eError =  OMX_GetConfig(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_TI_IndexConfigAutoConvergence, &ACParams);
    if ( eError != OMX_ErrorNone )
        {
        CAMHAL_LOGEB("Error while getting AutoConvergence 0x%x", eError);
        ret = -EINVAL;
        }
    else
        {
        *pManualConverence = ACParams.nManualConverence;
        *pACMode = ACParams.eACMode;
        CAMHAL_LOGDA("AutoConvergence got successfully");
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

bool OMXCameraAdapter::CommandHandler::Handler()
{
    Message msg;
    volatile int forever = 1;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    while(forever){
        CAMHAL_LOGDA("waiting for messsage...");
        MessageQueue::waitForMsg(&mCommandMsgQ, NULL, NULL, -1);
        mCommandMsgQ.get(&msg);
        CAMHAL_LOGDB("msg.command = %d", msg.command);
        switch ( msg.command ) {
            case CommandHandler::CAMERA_START_IMAGE_CAPTURE:
            {
                ret = mCameraAdapter->startImageCapture();
                break;
            }
            case CommandHandler::CAMERA_PERFORM_AUTOFOCUS:
            {
                ret = mCameraAdapter->doAutoFocus();
                break;
            }
            case CommandHandler::COMMAND_EXIT:
            {
                CAMHAL_LOGEA("Exiting command handler");
                forever = 0;
                break;
            }
        }
    }

    LOG_FUNCTION_NAME_EXIT
    return false;
}

OMXCameraAdapter::OMXCameraAdapter():mComponentState (OMX_StateInvalid)
{
    LOG_FUNCTION_NAME

    mFocusStarted = false;
    mSMALCDataRecord = NULL;
    mSMALCDataSize = 0;
    mSMALC_DCCFileDesc = NULL;
    mSMALC_DCCDescSize = 0;

    mPictureRotation = 0;
    // Initial values
    mTimeSourceDelta = 0;
    onlyOnce = true;

    mCameraAdapterParameters.mHandleComp = 0;
    signal(SIGTERM, SigHandler);
    signal(SIGALRM, SigHandler);

    LOG_FUNCTION_NAME_EXIT
}

OMXCameraAdapter::~OMXCameraAdapter()
{
    LOG_FUNCTION_NAME

    if (mSMALCDataRecord)
        free (mSMALCDataRecord);
    if (mSMALC_DCCFileDesc)
        free (mSMALC_DCCFileDesc);

   ///Free the handle for the Camera component
    if(mCameraAdapterParameters.mHandleComp)
        {
        OMX_FreeHandle(mCameraAdapterParameters.mHandleComp);
        }

    ///De-init the OMX
    if( (mComponentState==OMX_StateLoaded) || (mComponentState==OMX_StateInvalid))
        {
        OMX_Deinit();
        }

    //Exit and free ref to command handling thread
    if ( NULL != mCommandHandler.get() )
    {
        Message msg;
        msg.command = CommandHandler::COMMAND_EXIT;
        mCommandHandler->put(&msg);
        mCommandHandler->requestExitAndWait();
        mCommandHandler.clear();
    }

    LOG_FUNCTION_NAME_EXIT
}

extern "C" CameraAdapter* CameraAdapter_Factory()
{
    Mutex::Autolock lock(gAdapterLock);

    LOG_FUNCTION_NAME

    if ( NULL == gCameraAdapter )
        {
        CAMHAL_LOGDA("Creating new Camera adapter instance");
        gCameraAdapter= new OMXCameraAdapter();
        }
    else
        {
        CAMHAL_LOGDA("Reusing existing Camera adapter instance");
        }


    LOG_FUNCTION_NAME_EXIT

    return gCameraAdapter;
}

};


/*--------------------Camera Adapter Class ENDS here-----------------------------*/


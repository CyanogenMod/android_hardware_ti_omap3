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

#include <cutils/properties.h>
#define UNLIKELY( exp ) (__builtin_expect( (exp) != 0, false ))
static int mDebugFps = 0;

#define Q16_OFFSET 16

#define HERE(Msg) {CAMHAL_LOGEB("--===line %d, %s===--\n", __LINE__, Msg);}

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
    else if (SIGALRM )
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

    ///Event semaphore used for
    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    ///Update the preview and image capture port indexes
    mCameraAdapterParameters.mPrevPortIndex = OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW;
    // temp changed in order to build OMX_CAMERA_PORT_VIDEO_OUT_IMAGE;
    mCameraAdapterParameters.mImagePortIndex = OMX_CAMERA_PORT_IMAGE_OUT_IMAGE;
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

    ///Select the sensor
    OMX_CONFIG_SENSORSELECTTYPE sensorSelect;
    OMX_INIT_STRUCT_PTR (&sensorSelect, OMX_CONFIG_SENSORSELECTTYPE);
    sensorSelect.eSensor = (OMX_SENSORSELECT)sensor_index;
    eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_TI_IndexConfigSensorSelect, &sensorSelect);

    if ( OMX_ErrorNone != eError )
        {
        CAMHAL_LOGEB("Error while selecting the sensor index as %d - 0x%x", sensor_index, eError);
        ret = -1;
        }
    else
        {
        CAMHAL_LOGEB("Sensor %d selected successfully", sensor_index);
        }

    mSensorIndex = sensor_index;
    mBracketingEnabled = false;
    mBracketingBuffersQueuedCount = 0;
    mBracketingRange = 1;
    mLastBracetingBufferIdx = 0;
    mPreviewing = false;
    mCapturing = false;
    mRecording = false;
    mFlushBuffers = false;
    mWaitingForSnapshot = false;
    mSnapshotCount = 0;
    mComponentState = OMX_StateLoaded;
    mCapMode = HIGH_QUALITY;
    mBurstFrames = 1;
    mCapturedFrames = 0;
    mPictureQuality = 100;
    mCurrentZoomIdx = 0;
    mTargetZoomIdx = 0;
    mReturnZoomStatus = false;
    mSmoothZoomEnabled = false;
    mZoomInc = 1;
    mZoomParameterIdx = 0;

    //Setting this flag will that the first setParameter call will apply all 3A settings
    //and will not conditionally apply based on current values.
    mFirstTimeInit = true;

    memset(&mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex], 0, sizeof(OMXCameraPortParameters));
    memset(&mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex], 0, sizeof(OMXCameraPortParameters));

    return ErrorUtils::omxToAndroidError(eError);

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);

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


void OMXCameraAdapter::returnFrame(void* frameBuf, CameraFrame::FrameType frameType)
{
    status_t res = NO_ERROR;
    int refCount = -1;
    OMXCameraPortParameters *port = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if ( mComponentState != OMX_StateExecuting )
        {
        return;
        }

    if ( ( CameraFrame::IMAGE_FRAME == frameType ) ||
         ( CameraFrame::RAW_FRAME == frameType ) )
        {
        if ( ( 1 > mCapturedFrames ) && ( !mBracketingEnabled ) )
            {
            stopImageCapture();
            return;
            }
        else
            {
            port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];
            }
        }
    else if (  ( CameraFrame::PREVIEW_FRAME_SYNC == frameType ) ||
               ( CameraFrame::SNAPSHOT_FRAME == frameType ) )
        {
        port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
        }
    else if ( CameraFrame::VIDEO_FRAME_SYNC == frameType )
        {
        port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
        }
    else
        {
        return;
        }

    if ( NULL == frameBuf )
        {
        CAMHAL_LOGEA("Invalid frameBuf");
        res = -EINVAL;
        }

    if ( NO_ERROR == res)
        {

        if ( (CameraFrame::VIDEO_FRAME_SYNC == frameType) && mRecording )
            {

                {
                //Update refCount accordingly
                Mutex::Autolock lock(mVideoBufferLock);
                refCount = mVideoBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                //CAMHAL_LOGEB("Video Frame 0x%x returned refCount %d -> %d", frameBuf, refCount, refCount-1);

                if ( 0 >= refCount )
                    {
                    CAMHAL_LOGEB("Error trying to decrement refCount %d for buffer 0x%x", (uint32_t)refCount, (uint32_t)frameBuf);
                    return;
                    }

                refCount--;
                mVideoBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);

                //Query preview subscribers for this buffer
                refCount += mPreviewBuffersAvailable.valueFor( ( unsigned int ) frameBuf);

                }

            port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

            }
        else if ( ( CameraFrame::PREVIEW_FRAME_SYNC == frameType ) ||
                  ( CameraFrame::SNAPSHOT_FRAME == frameType ))
            {

                {
                //Update refCoung accordingly
                Mutex::Autolock lock(mPreviewBufferLock);
                refCount = mPreviewBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                //CAMHAL_LOGDB("Preview Frame 0x%x returned refCount %d -> %d", frameBuf, refCount, refCount-1);

                if ( 0 >= refCount )
                    {
                    CAMHAL_LOGEB("Error trying to decrement refCount %d for buffer 0x%x", refCount, (unsigned int)frameBuf);
                    return;
                    }
                refCount--;
                mPreviewBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);
                }

            if ( mRecording )
                {
                //Query video subscribers for this buffer
                Mutex::Autolock lock(mVideoBufferLock);
                refCount += mVideoBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
                }

            port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
            }
        else
            {
            refCount = 0;
            }

        //check if someone is holding this buffer
        if ( 0 == refCount )
            {
            for ( int i = 0 ; i < port->mNumBufs ; i++)
                {

                if ( port->mBufferHeader[i]->pBuffer == frameBuf )
                    {
                    //CAMHAL_LOGDB("Sending Frame 0x%x back to Ducati for filling", (unsigned int) frameBuf);
                    eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp, port->mBufferHeader[i]);
                    if ( eError != OMX_ErrorNone )
                        {
                        CAMHAL_LOGEB("OMX_FillThisBuffer %d", eError);
                        return;
                        }
                    }
                }
            }
        }

}

status_t OMXCameraAdapter::getCaps()
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;
    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t OMXCameraAdapter::setParameters(const CameraParameters &params)
{
    LOG_FUNCTION_NAME

    const char * str = NULL;
    int mode = 0;
    status_t ret = NO_ERROR;

    mParams = params;

    ///@todo Include more camera parameters
    int w, h;
    OMX_COLOR_FORMATTYPE pixFormat;
    if ( params.getPreviewFormat() != NULL )
        {
        if (strcmp(params.getPreviewFormat(), (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) == 0)
            {
            CAMHAL_LOGDA("CbYCrY format selected");
            pixFormat = OMX_COLOR_FormatCbYCrY;
            }
        else if(strcmp(params.getPreviewFormat(), (const char *) CameraParameters::PIXEL_FORMAT_YUV420SP) == 0)
            {
            CAMHAL_LOGDA("YUV420SP format selected");
            pixFormat = OMX_COLOR_FormatYUV420SemiPlanar;
            }
        else if(strcmp(params.getPreviewFormat(), (const char *) CameraParameters::PIXEL_FORMAT_RGB565) == 0)
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

    params.getPreviewSize(&w, &h);
    int frameRate = params.getPreviewFrameRate();

    CAMHAL_LOGVB("Preview frame rate %d", frameRate);

    OMXCameraPortParameters *cap;
    cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

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

    cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];

    params.getPictureSize(&w, &h);

    cap->mWidth = w;
    cap->mHeight = h;
    //TODO: Support more pixelformats
    cap->mStride = 2;
    cap->mBufSize = cap->mStride * cap->mHeight;

    CAMHAL_LOGVB("Image: cap.mWidth = %d", (int)cap->mWidth);
    CAMHAL_LOGVB("Image: cap.mHeight = %d", (int)cap->mHeight);

    if ( params.getPictureFormat() != NULL )
        {
        if (strcmp(params.getPictureFormat(), (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) == 0)
            {
            CAMHAL_LOGDA("CbYCrY format selected");
            pixFormat = OMX_COLOR_FormatCbYCrY;
            }
        else if(strcmp(params.getPictureFormat(), (const char *) CameraParameters::PIXEL_FORMAT_YUV420SP) == 0)
            {
            CAMHAL_LOGDA("YUV420SP format selected");
            pixFormat = OMX_COLOR_FormatYUV420SemiPlanar;
            }
        else if(strcmp(params.getPictureFormat(), (const char *) CameraParameters::PIXEL_FORMAT_RGB565) == 0)
            {
            CAMHAL_LOGDA("RGB565 format selected");
            pixFormat = OMX_COLOR_Format16bitRGB565;
            }
        else if(strcmp(params.getPictureFormat(), (const char *) CameraParameters::PIXEL_FORMAT_JPEG) == 0)
            {
            CAMHAL_LOGDA("JPEG format selected");
            pixFormat = OMX_COLOR_FormatUnused;
            }
        else if(strcmp(params.getPictureFormat(), (const char *) TICameraParameters::PIXEL_FORMAT_RAW) == 0)
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

    cap->mColorFormat = pixFormat;

    str = params.get(TICameraParameters::KEY_EXPOSURE_MODE);
    mode = getLUTvalue_HALtoOMX( str, ExpLUT);
    if ( ( str != NULL ) && ( mParameters3A.Exposure != mode ) )
        {
        mParameters3A.Exposure = mode;
        CAMHAL_LOGEB("Exposure mode %d", mode);
        }

    str = params.get(CameraParameters::KEY_WHITE_BALANCE);
    mode = getLUTvalue_HALtoOMX( str, WBalLUT);
    if ( mFirstTimeInit || ( str != NULL ) && ( mode != mParameters3A.WhiteBallance ) )
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
    if ( mFirstTimeInit || ( ( str != NULL ) && ( mParameters3A.Focus != mode )) )
        {
        mParameters3A.Focus = mode;
        CAMHAL_LOGEB("Focus %d", mParameters3A.Focus);
        //Apply focus mode immediatly only if  CAF  or Inifinity are selected
        if ( ( 0 <= mParameters3A.Focus ) && ( ( mode == OMX_IMAGE_FocusControlAuto ) ||
                ( mode == OMX_IMAGE_FocusControlAutoInfinity )) )
            {
            mPending3Asettings |= SetFocus;
            }
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
            }
        else
            {
            mParameters3A.SceneMode = OMX_Manual;
            }

        CAMHAL_LOGEB("SceneMode %d", mParameters3A.SceneMode);
        }

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

    if ( NULL != params.get(TICameraParameters::KEY_CAP_MODE) )
        {
        if (strcmp(params.get(TICameraParameters::KEY_CAP_MODE), (const char *) TICameraParameters::HIGH_PERFORMANCE_MODE) == 0)
            {
            mCapMode = OMXCameraAdapter::HIGH_SPEED;
            }
        else if (strcmp(params.get(TICameraParameters::KEY_CAP_MODE), (const char *) TICameraParameters::HIGH_QUALITY_MODE) == 0)
            {
            mCapMode = OMXCameraAdapter::HIGH_QUALITY;
            }
        else if (strcmp(params.get(TICameraParameters::KEY_CAP_MODE), (const char *) TICameraParameters::VIDEO_MODE) == 0)
            {
            mCapMode = OMXCameraAdapter::VIDEO_MODE;
            }
        else
            {
            mCapMode = OMXCameraAdapter::HIGH_QUALITY;
            }
        }
    else
        {
        mCapMode = OMXCameraAdapter::HIGH_QUALITY;
        }

    CAMHAL_LOGVB("Capture Mode set %d", mCapMode);

    if(mCapMode == OMXCameraAdapter::HIGH_QUALITY)
        {
        if ( NULL != params.get(TICameraParameters::KEY_IPP) )
            {
            if (strcmp(params.get(TICameraParameters::KEY_IPP), (const char *) TICameraParameters::IPP_LDCNSF) == 0)
                {
                mIPP = OMXCameraAdapter::IPP_LDCNSF;
                }
            else if (strcmp(params.get(TICameraParameters::KEY_IPP), (const char *) TICameraParameters::IPP_LDC) == 0)
                {
                mIPP = OMXCameraAdapter::IPP_LDC;
                }
            else if (strcmp(params.get(TICameraParameters::KEY_IPP), (const char *) TICameraParameters::IPP_NSF) == 0)
                {
                mIPP = OMXCameraAdapter::IPP_NSF;
                }
            else if (strcmp(params.get(TICameraParameters::KEY_IPP), (const char *) TICameraParameters::IPP_NONE) == 0)
                {
                mIPP = OMXCameraAdapter::IPP_NONE;
                }
            else
                {
                mIPP = OMXCameraAdapter::IPP_NONE;
                }
            }
        else
            {
            mIPP = OMXCameraAdapter::IPP_NONE;
            }

        CAMHAL_LOGEB("IPP Mode set %d", mIPP);
        }
    else
        {
        mIPP = OMXCameraAdapter::IPP_NONE;
        }

    if ( params.getInt(TICameraParameters::KEY_BURST)  >= 1 )
        {
        mBurstFrames = params.getInt(TICameraParameters::KEY_BURST);

        //always use high speed when doing burst
        mCapMode = OMXCameraAdapter::HIGH_SPEED;
        }
    else
        {
        mBurstFrames = 1;
        }

    CAMHAL_LOGVB("Burst Frames set %d", mBurstFrames);

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


    CAMHAL_LOGVB("Picture Thumb width set %d", mThumbWidth);


    ///Set VNF Configuration
    if ( params.getInt(TICameraParameters::KEY_VNF)  > 0 )
        {
        CAMHAL_LOGDA("VNF Enabled");
        mVnfEnabled = true;
        }
    else
        {
        CAMHAL_LOGDA("VNF Disabled");
        mVnfEnabled = false;
        }

    ///Set VSTAB Configuration
    if ( params.getInt(TICameraParameters::KEY_VSTAB)  > 0 )
        {
        CAMHAL_LOGDA("VSTAB Enabled");
        mVstabEnabled = true;
        }
    else
        {
        CAMHAL_LOGDA("VSTAB Disabled");
        mVstabEnabled = false;
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

    int zoom = params.getInt(CameraParameters::KEY_ZOOM);
    if( (zoom >= 0) && ( zoom < ZOOM_STAGES) ){
        mTargetZoomIdx = zoom;
    } else {
        mTargetZoomIdx = 0;
    }
    //Immediate zoom should be applied instantly ( CTS requirement )
    mCurrentZoomIdx = mTargetZoomIdx;
    doZoom(mCurrentZoomIdx);

    CAMHAL_LOGDB("Zoom by App %d", zoom);

    mFirstTimeInit = false;

    LOG_FUNCTION_NAME_EXIT
    return ret;
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

void OMXCameraAdapter::getParameters(CameraParameters& params)
{
    LOG_FUNCTION_NAME

#ifdef PARAM_FEEDBACK

    OMX_CONFIG_EXPOSURECONTROLTYPE exp;
    OMX_CONFIG_EXPOSUREVALUETYPE expValues;
    OMX_CONFIG_WHITEBALCONTROLTYPE wb;
    OMX_CONFIG_FLICKERCANCELTYPE flicker;
    OMX_CONFIG_SCENEMODETYPE scene;
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
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp,OMX_IndexConfigCommonExposureValue, &expValues);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonWhiteBalance, &wb);
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexConfigFlickerCancel, &flicker );
    OMX_GetConfig( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexParamSceneMode, &scene);
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

    for(int i = 0; i < EffLUT.size; i++)
        if( EffLUT.Table[i].omxDefinition == effect.eImageFilter )
            str = (char*)EffLUT.Table[i].userDefinition;
    params.set( CameraParameters::KEY_EFFECT , str );

    for(int i = 0; i < FocusLUT.size; i++)
        if( FocusLUT.Table[i].omxDefinition == focus.eFocusControl )
            str = (char*)FocusLUT.Table[i].userDefinition;

    params.set( CameraParameters::KEY_FOCUS_MODE , str );

    for(int i = 0; i < IsoLUT.size; i++)
        if( IsoLUT.Table[i].omxDefinition == ( int ) expValues.nSensitivity )
            str = (char*)IsoLUT.Table[i].userDefinition;

    params.set( TICameraParameters::KEY_ISO , str );

    int comp = ((expValues.xEVCompensation * 10) >> Q16_OFFSET);

    params.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, comp );
    params.set( TICameraParameters::KEY_MAN_EXPOSURE, expValues.nShutterSpeedMsec);
    params.set( TICameraParameters::KEY_BRIGHTNESS, brightness.nBrightness);
    params.set( TICameraParameters::KEY_CONTRAST, contrast.nContrast );
    params.set( TICameraParameters::KEY_SHARPNESS, procSharpness.nLevel);
    params.set( TICameraParameters::KEY_SATURATION, saturation.nSaturation);

#else

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

#endif

    LOG_FUNCTION_NAME_EXIT
}

status_t OMXCameraAdapter::setFormat(OMX_U32 port, OMXCameraPortParameters &portParams)
{
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
        if((portCheck.format.video.nFrameWidth==1920) && (portCheck.format.video.nFrameHeight==1080))
            {
                if(portParams.mFrameRate>=FRAME_RATE_FULL_HD)
                    {
                    ///Limit the frame rate at full hd resolution to maximum supported rate.
                    portParams.mFrameRate = FRAME_RATE_FULL_HD;
                    }
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
        if ( OMX_COLOR_FormatUnused == portParams.mColorFormat )
            {
            portCheck.format.image.eColorFormat     = OMX_COLOR_FormatCbYCrY;
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
        LOGD("\n ***IMG Height = %ld", portCheck.format.image.nFrameHeight);

        LOGD("\n ***IMG IMG FMT = %x", portCheck.format.image.eColorFormat);
        LOGD("\n ***IMG portCheck.nBufferSize = %ld\n",portCheck.nBufferSize);
        LOGD("\n ***IMG portCheck.nBufferCountMin = %ld\n",
                                                portCheck.nBufferCountMin);
        LOGD("\n ***IMG portCheck.nBufferCountActual = %ld\n",
                                                portCheck.nBufferCountActual);
        LOGD("\n ***IMG portCheck.format.image.nStride = %ld\n",
                                                portCheck.format.image.nStride);
        }
    else
        {
        LOGD("\n *** PRV Width = %ld", portCheck.format.video.nFrameWidth);
        LOGD("\n ***PRV Height = %ld", portCheck.format.video.nFrameHeight);

        LOGD("\n ***PRV IMG FMT = %x", portCheck.format.video.eColorFormat);
        LOGD("\n ***PRV portCheck.nBufferSize = %ld\n",portCheck.nBufferSize);
        LOGD("\n ***PRV portCheck.nBufferCountMin = %ld\n",
                                                portCheck.nBufferCountMin);
        LOGD("\n ***PRV portCheck.nBufferCountActual = %ld\n",
                                                portCheck.nBufferCountActual);
        LOGD("\n ***PRV portCheck.format.video.nStride = %ld\n",
                                                portCheck.format.video.nStride);
        }

    LOG_FUNCTION_NAME_EXIT

    return ErrorUtils::omxToAndroidError(eError);

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of eError=%x", __FUNCTION__, eError);

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

    Mutex::Autolock lock(mLock);

    if(!mPreviewing || mFlushBuffers)
        {
        LOG_FUNCTION_NAME_EXIT
        return NO_ERROR;
        }

    ///If preview is ongoing and we get a new set of buffers, flush the o/p queue,
    ///wait for all buffers to come back and then queue the new buffers in one shot
    ///Stop all callbacks when this is ongoing
    mFlushBuffers = true;

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
    eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp, OMX_CommandFlush,
                                                    OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW,
                                                    NULL);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandFlush)-0x%x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

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
status_t OMXCameraAdapter::useBuffers(CameraMode mode, void* bufArr, int num)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    switch(mode)
        {
        case CAMERA_PREVIEW:
            ret = UseBuffersPreview(bufArr, num);
            break;

        case CAMERA_IMAGE_CAPTURE:
            ret = UseBuffersCapture(bufArr, num);
            break;

        case CAMERA_VIDEO:
            ret = UseBuffersPreview(bufArr, num);
            break;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::UseBuffersPreview(void* bufArr, int num)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    LOG_FUNCTION_NAME

    if(mComponentState!=OMX_StateLoaded)
        {
        CAMHAL_LOGEA("Calling UseBuffersPreview() when not in LOADED state");
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
        }

    if(!bufArr)
        {
        CAMHAL_LOGEA("NULL pointer passed for buffArr");
        LOG_FUNCTION_NAME_EXIT
        return BAD_VALUE;
        }

    OMXCameraPortParameters * mPreviewData = NULL;
    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
    mPreviewData->mNumBufs = num ;
    uint32_t *buffers = (uint32_t*)bufArr;
    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    if(mPreviewing && (mPreviewData->mNumBufs!=num))
        {
        CAMHAL_LOGEA("Current number of buffers doesnt equal new num of buffers passed!");
        LOG_FUNCTION_NAME_EXIT
        return BAD_VALUE;
        }
    ///If preview is ongoing
    if(mPreviewing)
        {
        ///If preview is ongoing and we get a new set of buffers, flush the o/p queue,
        ///wait for all buffers to come back and then queue the new buffers in one shot
        ///Stop all callbacks when this is ongoing
        mFlushBuffers = true;

        ///Register for the FLUSH event
        ///This method just inserts a message in Event Q, which is checked in the callback
        ///The sempahore passed is signalled by the callback
        ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                    OMX_EventCmdComplete,
                                    OMX_CommandFlush,
                                    OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW,
                                    eventSem,
                                    -1 ///Infinite timeout
                                    );
        if(ret!=NO_ERROR)
            {
            CAMHAL_LOGEB("Error in registering for event %d", ret);
            goto EXIT;
            }
        ///Send FLUSH command to preview port
        eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp, OMX_CommandFlush,
                                                        OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW,
                                                        NULL);
        if(eError!=OMX_ErrorNone)
            {
            CAMHAL_LOGEB("OMX_SendCommand(OMX_CommandFlush)- %x", eError);
            }
        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

        ///Wait for the FLUSH event to occur
        eventSem.Wait();
        CAMHAL_LOGDA("Flush event received");


        ///If flush has already happened, we need to update the pBuffer pointer in
        ///the buffer header and call OMX_FillThisBuffer to queue all the new buffers
        for(int index=0;index<num;index++)
            {
            CAMHAL_LOGDB("Queuing new buffers to Ducati 0x%x",((int32_t*)bufArr)[index]);

            mPreviewData->mBufferHeader[index]->pBuffer = (OMX_U8*)((int32_t*)bufArr)[index];

            eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp,
                        (OMX_BUFFERHEADERTYPE*)mPreviewData->mBufferHeader[index]);

            if(eError!=OMX_ErrorNone)
                {
                CAMHAL_LOGEB("OMX_FillThisBuffer-0x%x", eError);
                }
            GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

            }
        ///Once we queued new buffers, we set the flushBuffers flag to false
        mFlushBuffers = false;

        ret = ErrorUtils::omxToAndroidError(eError);

        ///Return from here
        return ret;
        }

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

    ret = setFormat(OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW, *mPreviewData);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEB("setFormat() failed %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    ret = setImageQuality(mPictureQuality);
    if ( NO_ERROR != ret)
        {
        CAMHAL_LOGEB("Error configuring image quality %x", ret);
        return ret;
        }

    ret = setThumbnailParams(mThumbWidth, mThumbHeight, mThumbQuality);
    if ( NO_ERROR != ret)
        {
        CAMHAL_LOGEB("Error configuring thumbnail size %x", ret);
        return ret;
        }

    ret = setExposureMode(mParameters3A);
    if ( NO_ERROR != ret )
        {
        CAMHAL_LOGEB("Error configuring exposure mode %x", ret);
        return ret;
        }

    ret = setScene(mParameters3A);
    if ( NO_ERROR != ret )
        {
        CAMHAL_LOGEB("Error configuring scene mode %x", ret);
        return ret;
        }

    if(mCapMode == OMXCameraAdapter::VIDEO_MODE)
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
        mVnfEnabled = false;
        ///Enable/Disable Video Noise Filter
        ret = enableVideoNoiseFilter(mVnfEnabled);
        if ( NO_ERROR != ret)
            {
            CAMHAL_LOGEB("Error configuring VNF %x", ret);
            return ret;
            }

        mVstabEnabled = false;
        ///Enable/Disable Video Stabilization
        ret = enableVideoStabilization(mVstabEnabled);
        if ( NO_ERROR != ret)
            {
            CAMHAL_LOGEB("Error configuring VSTAB %x", ret);
            return ret;
            }
        }
    ///Register for IDLE state switch event
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandStateSet,
                                OMX_StateIdle,
                                eventSem,
                                -1 ///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }


    ///Once we get the buffers, move component state to idle state and pass the buffers to OMX comp using UseBuffer
    eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp , OMX_CommandStateSet, OMX_StateIdle, NULL);

    CAMHAL_LOGDB("OMX_SendCommand(OMX_CommandStateSet) 0x%x", eError);
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    for(int index=0;index<num;index++)
        {
        OMX_BUFFERHEADERTYPE *pBufferHdr;
        CAMHAL_LOGDB("OMX_UseBuffer(0x%x)", buffers[index]);
        eError = OMX_UseBuffer( mCameraAdapterParameters.mHandleComp,
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

        pBufferHdr->pAppPrivate = (OMX_PTR)pBufferHdr;
        pBufferHdr->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pBufferHdr->nVersion.s.nVersionMajor = 1 ;
        pBufferHdr->nVersion.s.nVersionMinor = 1 ;
        pBufferHdr->nVersion.s.nRevision = 0 ;
        pBufferHdr->nVersion.s.nStep =  0;
        mPreviewData->mBufferHeader[index] = pBufferHdr;
        }

    CAMHAL_LOGDA("Waiting LOADED->IDLE state transition");
    ///Wait for state to switch to idle
    eventSem.Wait();
    CAMHAL_LOGDA("LOADED->IDLE state changed");

    mComponentState = OMX_StateIdle;


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

    LOGE("Params Width = %d", (int)imgCaptureData->mWidth);
    LOGE("Params Height = %d", (int)imgCaptureData->mWidth);

    ret = setFormat(OMX_CAMERA_PORT_IMAGE_OUT_IMAGE, *imgCaptureData);
    if ( ret != NO_ERROR )
        {
        CAMHAL_LOGEB("setFormat() failed %d", ret);
        LOG_FUNCTION_NAME_EXIT
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

    mCapturedFrames = mBurstFrames;

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

//API to send a command to the camera
status_t OMXCameraAdapter::sendCommand(int operation, int value1, int value2, int value3)
{
    LOG_FUNCTION_NAME

    status_t ret = NO_ERROR;
    CameraAdapter::CameraMode mode;
    struct timeval *refTimestamp;
    BuffersDescriptor *desc = NULL;
    Message msg;

    switch ( operation ) {
        case CameraAdapter::CAMERA_USE_BUFFERS:
                {
                CAMHAL_LOGDA("Use Buffers");
                mode = ( CameraAdapter::CameraMode ) value1;
                desc = ( BuffersDescriptor * ) value2;

                if ( CameraAdapter::CAMERA_PREVIEW == mode )
                    {
                    if ( NULL == desc )
                        {
                        CAMHAL_LOGEA("Invalid preview buffers!");
                        ret = -1;
                        }

                    if ( ret == NO_ERROR )
                        {
                        Mutex::Autolock lock(mPreviewBufferLock);

                        mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex].mNumBufs =  desc->mCount;
                        mPreviewBuffers = (int *) desc->mBuffers;

                        mPreviewBuffersAvailable.clear();
                        for ( uint32_t i = 0 ; i < desc->mCount ; i++ )
                            {
                            mPreviewBuffersAvailable.add(mPreviewBuffers[i], 0);
                            }
                        }
                    }
                else if( CameraAdapter::CAMERA_IMAGE_CAPTURE == mode )
                    {
                    if ( NULL == desc )
                        {
                        CAMHAL_LOGEA("Invalid capture buffers!");
                        ret = -1;
                        }
                    if ( ret == NO_ERROR )
                        {
                        Mutex::Autolock lock(mCaptureBufferLock);
                        mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex].mNumBufs = desc->mCount;
                        mCaptureBuffers = (int *) desc->mBuffers;
                        mCaptureBuffersLength = desc->mLength;
                        CAMHAL_LOGEB("mCaputreBuffersLength %d", mCaptureBuffersLength);
                        mCaptureBuffersAvailable.clear();
                        for ( uint32_t i = 0 ; i < desc->mCount ; i++ )
                            {
                            mCaptureBuffersAvailable.add(mCaptureBuffers[i], true);
                            }
                        }
                    }
                else
                    {
                    CAMHAL_LOGEB("Camera Mode %x still not supported!", mode);
                    }

                if ( NULL != desc )
                    {
                    useBuffers(mode, desc->mBuffers, desc->mCount);
                    }
                break;
            }

        case CameraAdapter::CAMERA_START_SMOOTH_ZOOM:
            {
            CAMHAL_LOGDB("Start smooth zoom target = %d", value1);

            if ( ( value1 >= 0 ) && ( value1 < ZOOM_STAGES ) )
                {
                mTargetZoomIdx = value1;
                mZoomParameterIdx = mCurrentZoomIdx;
                mSmoothZoomEnabled = true;
                }
            else
                {
                CAMHAL_LOGEB("Smooth value out of range %d!", value1);
                ret = -EINVAL;
                }
            break;
            }

        case CameraAdapter::CAMERA_STOP_SMOOTH_ZOOM:
            {

            if ( mSmoothZoomEnabled )
                {
                mTargetZoomIdx = mCurrentZoomIdx;
                mReturnZoomStatus = true;
                CAMHAL_LOGDB("Stop smooth zoom mCurrentZoomIdx = %d, mTargetZoomIdx = %d", mCurrentZoomIdx, mTargetZoomIdx);
                }

            break;
            }

        case CameraAdapter::CAMERA_START_PREVIEW:
            {
            CAMHAL_LOGDA("Start Preview");

            if( mPending3Asettings )
                Apply3Asettings(mParameters3A);

            ret = startPreview();

            break;
            }

        case CameraAdapter::CAMERA_STOP_PREVIEW:
            {
            CAMHAL_LOGDA("Stop Preview");
            stopPreview();
            break;
            }
        case CameraAdapter::CAMERA_START_VIDEO:
            {
            CAMHAL_LOGDA("Start video recording");
            startVideoCapture();
            break;
            }
        case CameraAdapter::CAMERA_STOP_VIDEO:
            {
            CAMHAL_LOGDA("Stop video recording");
            stopVideoCapture();
            break;
            }
        case CameraAdapter::CAMERA_SET_TIMEOUT:
            {
            CAMHAL_LOGDA("Set time out");
            setTimeOut(value1);
            break;
            }
        case CameraAdapter::CAMERA_CANCEL_TIMEOUT:
            {
            CAMHAL_LOGDA("Cancel time out");
            cancelTimeOut();
            break;
            }
        case CameraAdapter::CAMERA_PREVIEW_FLUSH_BUFFERS:
            {
            flushBuffers();
            break;
            }

        case CameraAdapter::CAMERA_START_IMAGE_CAPTURE:
            {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            refTimestamp = ( struct timeval * ) value1;
            if ( NULL != refTimestamp )
                {
                memcpy( &mStartCapture, refTimestamp, sizeof(struct timeval));
                }

#endif
            if( mPending3Asettings )
                Apply3Asettings(mParameters3A);

            ret = startImageCapture();
            break;
            }
        case CameraAdapter::CAMERA_STOP_IMAGE_CAPTURE:
            {
            ret = stopImageCapture();
            break;
            }
        case CameraAdapter::CAMERA_START_BRACKET_CAPTURE:
            {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            refTimestamp = ( struct timeval * ) value2;
            if ( NULL != refTimestamp )
                {
                memcpy( &mStartCapture, refTimestamp, sizeof(struct timeval));
                }

#endif

            mBracketingRange = value1;

            if( mPending3Asettings )
                Apply3Asettings(mParameters3A);

            ret = startBracketing();
            break;
            }
        case CameraAdapter::CAMERA_STOP_BRACKET_CAPTURE:
            {
            ret = stopBracketing();
            break;
            }
         case CameraAdapter::CAMERA_PERFORM_AUTOFOCUS:

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            refTimestamp = ( struct timeval * ) value1;
            if ( NULL != refTimestamp )
                {
                memcpy( &mStartFocus, refTimestamp, sizeof(struct timeval));
                }

#endif

            ret = doAutoFocus();

            break;

        default:
            CAMHAL_LOGEB("Command 0x%x unsupported!", operation);
            break;
    };

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t OMXCameraAdapter::startPreview()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }


    if(mComponentState!=OMX_StateIdle)
        {
        CAMHAL_LOGEA("Calling UseBuffersPreview() when not in IDLE state");
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
        }

    LOG_FUNCTION_NAME

    OMXCameraPortParameters * mPreviewData = NULL;
    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];

    ///Register for EXECUTING state transition.
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandStateSet,
                                OMX_StateExecuting,
                                eventSem,
                                -1 ///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }


    ///Switch to EXECUTING state
    ret = OMX_SendCommand(mCameraAdapterParameters.mHandleComp,
                                OMX_CommandStateSet,
                                OMX_StateExecuting,
                                NULL);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_StateExecuting)-0x%x", eError);
        }
    if( NO_ERROR == ret)
        {
        Mutex::Autolock lock(mLock);
        mPreviewing = true;
        }
    else
        {
        goto EXIT;
        }


    CAMHAL_LOGDA("+Waiting for component to go into EXECUTING state");
    ///Perform the wait for Executing state transition
    ///@todo Replace with a timeout
    eventSem.Wait();
    CAMHAL_LOGDA("+Great. Component went into executing state!!");

   ///Queue all the buffers on preview port
    for(int index=0;index< mPreviewData->mNumBufs;index++)
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

    mComponentState = OMX_StateExecuting;

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

    OMXCameraPortParameters *mCaptureData , * mPreviewData;
    mCaptureData = mPreviewData = NULL;

    mPreviewData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
    mCaptureData = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex];


    Semaphore eventSem;
    ret = eventSem.Create(0);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in creating semaphore %d", ret);
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

    if ( mComponentState != OMX_StateExecuting )
        {
        CAMHAL_LOGEA("Calling StopPreview() when not in EXECUTING state");
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
        }

    CAMHAL_LOGEB("Average framerate: %f", mFPS);

    ///Register for EXECUTING state transition.
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandStateSet,
                                OMX_StateIdle,
                                eventSem,
                                -1 ///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }



    ret = OMX_SendCommand (mCameraAdapterParameters.mHandleComp,
                                OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_StateIdle) - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    ///Wait for the EXECUTING ->IDLE transition to arrive

    CAMHAL_LOGDA("EXECUTING->IDLE state changed");

    eventSem.Wait();

    CAMHAL_LOGDA("EXECUTING->IDLE state changed");

    mComponentState = OMX_StateIdle;

    ///Register for LOADED state transition.
    ///This method just inserts a message in Event Q, which is checked in the callback
    ///The sempahore passed is signalled by the callback
    ret = RegisterForEvent(mCameraAdapterParameters.mHandleComp,
                                OMX_EventCmdComplete,
                                OMX_CommandStateSet,
                                OMX_StateLoaded,
                                eventSem,
                                -1 ///Infinite timeout
                                );
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("Error in registering for event %d", ret);
        goto EXIT;
        }

    eError = OMX_SendCommand (mCameraAdapterParameters.mHandleComp,
                            OMX_CommandStateSet, OMX_StateLoaded, NULL);
    if(eError!=OMX_ErrorNone)
        {
        CAMHAL_LOGEB("OMX_SendCommand(OMX_StateLoaded) - %x", eError);
        }
    GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

    ///Free the OMX Buffers
    for ( int i = 0 ; i < mPreviewData->mNumBufs ; i++ )
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

    ///Wait for the IDLE -> LOADED transition to arrive
    CAMHAL_LOGDA("IDLE->LOADED state changed");
    eventSem.Wait();
    CAMHAL_LOGDA("IDLE->LOADED state changed");


    mComponentState = OMX_StateLoaded;

        {
        Mutex::Autolock lock(mPreviewBufferLock);
        ///Clear all the available preview buffers
        mPreviewBuffersAvailable.clear();
        }

    ///Clear the previewing flag, we are no longer previewing
    mPreviewing = false;

    LOG_FUNCTION_NAME_EXIT

    return (ret | ErrorUtils::omxToAndroidError(eError));

    EXIT:
    CAMHAL_LOGEB("Exiting function because of eError= %x", eError);

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

status_t OMXCameraAdapter::setTimeOut(unsigned int sec)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    ret = alarm(sec);

    //At this point ErrorNotifier becomes invalid
    mErrorNotifier = NULL;

        //Subscriptions are also invalid
        {
        Mutex::Autolock lock(mSubscriberLock);

        mFrameSubscribers.clear();
        mImageSubscribers.clear();
        mRawSubscribers.clear();
        mVideoSubscribers.clear();
        mFocusSubscribers.clear();
        mShutterSubscribers.clear();
        mZoomSubscribers.clear();
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
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

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in loaded state");
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

        thumbConf.nWidth = width;
        thumbConf.nHeight = height;
        thumbConf.nQuality = quality;

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

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in loaded state");
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

status_t OMXCameraAdapter::setExposureMode(Gen3A_settings& Gen3A)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_EXPOSURECONTROLTYPE exp;

    LOG_FUNCTION_NAME

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in loaded state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&exp, OMX_CONFIG_EXPOSURECONTROLTYPE);
        exp.nPortIndex = OMX_ALL;
        exp.eExposureControl = (OMX_EXPOSURECONTROLTYPE)Gen3A.Exposure;

        CAMHAL_LOGEB("Configuring exposure mode 0x%x", exp.eExposureControl);
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

    return ret;
}

status_t OMXCameraAdapter::setScene(Gen3A_settings& Gen3A)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_SCENEMODETYPE scene;

    LOG_FUNCTION_NAME

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in loaded state");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&scene, OMX_CONFIG_SCENEMODETYPE);
        scene.nPortIndex = OMX_ALL;
        scene.eSceneMode = ( OMX_SCENEMODETYPE ) Gen3A.SceneMode;

        CAMHAL_LOGEB("Configuring scene mode 0x%x", scene.eSceneMode);
        eError =  OMX_SetParameter(mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE) OMX_IndexParamSceneMode, &scene);
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

    return ret;
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
            CAMHAL_LOGEA("Error while configuring rotation");
            ret = -1;
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

status_t OMXCameraAdapter::setCaptureMode(OMXCameraAdapter::CaptureMode mode)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CONFIG_CAMOPERATINGMODETYPE camMode;


    LOG_FUNCTION_NAME

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

        if(ret != -1)
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

status_t OMXCameraAdapter::doAutoFocus()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE focusControl;
    Semaphore eventSem;

    LOG_FUNCTION_NAME

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component not in executing state");
        ret = -1;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (&focusControl, OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE);
        focusControl.eFocusControl = ( OMX_IMAGE_FOCUSCONTROLTYPE ) mParameters3A.Focus;

        eError =  OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigFocusControl, &focusControl);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error while starting focus 0x%x", eError);
            ret = -1;
            }
        else
            {
            CAMHAL_LOGDA("Autofocus started successfully");
            mFocusStarted = true;
            }
        }

    if ( (mParameters3A.Focus != OMX_IMAGE_FocusControlAuto )  &&
                ( mParameters3A.Focus != OMX_IMAGE_FocusControlAutoInfinity) )
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

        if ( NO_ERROR == ret )
            {
            eventSem.Wait();
            CAMHAL_LOGDA("Autofocus callback received successfully");
            }

        if ( NO_ERROR == ret )
            {
            ret = notifyFocusSubscribers(false, false);
            }

        }
    else
        {
        if ( NO_ERROR == ret )
            {
            ret = notifyFocusSubscribers(true, true);
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

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component not in executing state");
        ret = -1;
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

        if ( enabled )
            {
            focusRequstCallback.bEnable = OMX_TRUE;
            focusRequstCallback.nIndex = OMX_IndexConfigCommonFocusStatus;
            }
        else
            {
            focusRequstCallback.bEnable = OMX_FALSE;
            focusRequstCallback.nIndex = OMX_IndexConfigCommonFocusStatus;
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

status_t OMXCameraAdapter::notifyZoomSubscribers(int zoomIdx, bool targetReached)
{
    event_callback eventCb;
    CameraHalEvent zoomEvent;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    zoomEvent.mEventType = CameraHalEvent::EVENT_ZOOM_INDEX_REACHED;
    zoomEvent.mEventData.zoomEvent.currentZoomIndex = zoomIdx;
    zoomEvent.mEventData.zoomEvent.targetZoomIndexReached = targetReached;

        //Start looking for zoom subscribers
        {
        Mutex::Autolock lock(mSubscriberLock);

        if ( mZoomSubscribers.size() == 0 )
            {
            CAMHAL_LOGDA("No Focus Subscribers!");
            }

        for (unsigned int i = 0 ; i < mZoomSubscribers.size(); i++ )
            {
            zoomEvent.mCookie = (void *) mZoomSubscribers.keyAt(i);
            eventCb = (event_callback) mZoomSubscribers.valueAt(i);

            eventCb ( &zoomEvent );
            }

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::notifyFocusSubscribers(bool override, bool status)
{
    static unsigned int frameCounter = 0;
    event_callback eventCb;
    CameraHalEvent focusEvent;
    OMX_PARAM_FOCUSSTATUSTYPE eFocusStatus;
    bool focusStatus = false;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( !override )
        {
        if ( mFocusStarted )
            {

            frameCounter++;

            ret = checkFocus(&eFocusStatus);

            if ( NO_ERROR == ret )
                {

                if ( OMX_FocusStatusReached == eFocusStatus.eFocusStatus)
                    {
                    stopAutoFocus();
                    frameCounter = 0;
                    focusStatus = true;
                    }
                else if((OMX_FocusStatusOff == eFocusStatus.eFocusStatus) || (OMX_FocusStatusUnableToReach == eFocusStatus.eFocusStatus))
                    {
                    stopAutoFocus();
                    frameCounter = 0;
                    focusStatus = false;
                    }
                else if ( (OMX_FocusStatusRequest == eFocusStatus.eFocusStatus) && (frameCounter > mFocusThreshold) )
                    {
                    stopAutoFocus();
                    frameCounter = 0;
                    focusStatus = false;
                    }
                else
                    {
                    return NO_ERROR;
                    }
                }
            }
        else
            {
            return NO_ERROR;
            }
        }
    else
        {
        focusStatus = status;
        }

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

     //dump the AF latency
     CameraHal::PPM("Focus finished in: ", &mStartFocus);

#endif

    focusEvent.mEventType = CameraHalEvent::EVENT_FOCUS_LOCKED;
    focusEvent.mEventData.focusEvent.focusLocked = focusStatus;
    focusEvent.mEventData.focusEvent.focusError = !focusStatus;

        //Start looking for event subscribers
        {
        Mutex::Autolock lock(mSubscriberLock);

            if ( mFocusSubscribers.size() == 0 )
                {
                CAMHAL_LOGDA("No Focus Subscribers!");
                }

            for (unsigned int i = 0 ; i < mFocusSubscribers.size(); i++ )
                {
                focusEvent.mCookie = (void *) mFocusSubscribers.keyAt(i);
                eventCb = (event_callback) mFocusSubscribers.valueAt(i);

                eventCb ( &focusEvent );
                }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::startBracketing()
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

    if ( NO_ERROR == ret )
        {
        Mutex::Autolock lock(mBracketingLock);

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
                sendFrameToSubscribers(imgCaptureData->mBufferHeader[currentBufferIdx], imgCaptureData->mImageType, imgCaptureData);
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

    if ( OMX_StateExecuting != mComponentState )
        {
        CAMHAL_LOGEA("OMX component is not in executing state");
        ret = -EINVAL;
        }

        {
        Mutex::Autolock lock(mBracketingLock);

        if ( !mBracketingEnabled )
            {
            return ret;
            }

        }

    if ( NO_ERROR == ret )
        {

        ret = stopImageCapture();

            {
            Mutex::Autolock lock(mBracketingLock);
            mBracketingEnabled = false;
            }

        }

    if ( NO_ERROR == ret )
        {
        Mutex::Autolock lock(mBracketingLock);
        if ( NULL != mBracketingBuffersQueued )
            {
            delete mBracketingBuffersQueued;
            }

        mBracketingBuffersQueuedCount = 0;
        mLastBracetingBufferIdx = 0;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::startImageCapture()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMXCameraPortParameters * capData = NULL;
    OMX_CONFIG_BOOLEANTYPE bOMX;

    LOG_FUNCTION_NAME

        //During bracketing image capture is already active
        {
        Mutex::Autolock lock(mBracketingLock);
        if ( mBracketingEnabled )
            {
            //Stop bracketing, activate normal burst for the remaining images
            mBracketingEnabled = false;
            mCapturedFrames = mBracketingRange;
            ret = sendBracketFrames();

            if ( NULL != mBracketingBuffersQueued )
                {
                delete mBracketingBuffersQueued;
                }

            return ret;
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

        OMX_INIT_STRUCT_PTR (&bOMX, OMX_CONFIG_BOOLEANTYPE);
        bOMX.bEnabled = OMX_TRUE;

        /// sending Capturing Command to the component
        eError = OMX_SetConfig(mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCapturing, &bOMX);

        CAMHAL_LOGDB("Capture set - 0x%x", eError);

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);

            {
            Mutex::Autolock lock(mLock);
            mWaitingForSnapshot = true;
            mCapturing = true;
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

        }

    return ret;
}

status_t OMXCameraAdapter::stopImageCapture()
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError;
    OMX_CONFIG_BOOLEANTYPE bOMX;
    OMXCameraPortParameters *imgCaptureData = NULL;

    LOG_FUNCTION_NAME

    mWaitingForSnapshot = false;
    mSnapshotCount = 0;

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
    CAMHAL_LOGDB("Freeing buffer on Capture port - %d", imgCaptureData->mNumBufs);
    for ( int index = 0 ; index < imgCaptureData->mNumBufs ; index++)
        {
        CAMHAL_LOGDB("Freeing buffer on Capture port - 0x%x", ( unsigned int ) imgCaptureData->mBufferHeader[index]->pBuffer);
        eError = OMX_FreeBuffer(mCameraAdapterParameters.mHandleComp,
                        mCameraAdapterParameters.mImagePortIndex,
                        (OMX_BUFFERHEADERTYPE*)imgCaptureData->mBufferHeader[index]);

        GOTO_EXIT_IF((eError!=OMX_ErrorNone), eError);
        }

    CAMHAL_LOGDA("Waiting for port disable");
    //Wait for the image port enable event
    camSem.Wait();
    CAMHAL_LOGDA("Port disabled");

    mCapturing = false;

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

    LOG_FUNCTION_NAME_EXIT

    return ret;

    EXIT:

    if(eError != OMX_ErrorNone)
        {
        CAMHAL_LOGEB("Error occured when disabling image capture port %x",eError);

        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }

        }

    return ret;
}

status_t OMXCameraAdapter::startVideoCapture()
{
    Mutex::Autolock lock(mVideoBufferLock);

    ///If the capture mode is not video mode, return immediately
    if((mCapMode != VIDEO_MODE) || (mRecording))
        {
        return NO_INIT;
        }

    for(unsigned int i=0;i<mPreviewBuffersAvailable.size();i++)
        {
        mVideoBuffersAvailable.add(mPreviewBuffersAvailable.keyAt(i), 0);
        }

    ///Do nothing as preview and capture port are one and the same
    mRecording = true;

    ///@todo implement video port on-the-fly enable to provide the native resolution requested

    return NO_ERROR;
}

status_t OMXCameraAdapter::stopVideoCapture()
{
    Mutex::Autolock lock(mVideoBufferLock);
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMXCameraPortParameters *port = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];;

    if(!mRecording)
        {
        return NO_INIT;
        }


    for(unsigned int i=0;i<mVideoBuffersAvailable.size();i++)
        {
        int refCount = 0;
        void *frameBuf = (void*) mVideoBuffersAvailable.keyAt(i);

        refCount = mVideoBuffersAvailable.valueFor( ( unsigned int ) frameBuf );
        if ( 0 >= refCount )
            {
            CAMHAL_LOGEB("Error trying to decrement refCount %d for buffer 0x%x", (uint32_t)refCount, (uint32_t)frameBuf);
            return BAD_VALUE;
            }

        refCount--;
        mVideoBuffersAvailable.replaceValueFor(  ( unsigned int ) frameBuf, refCount);

        //Query preview subscribers for this buffer
        refCount += mPreviewBuffersAvailable.valueFor( ( unsigned int ) frameBuf);

        //check if someone is holding this buffer
        if ( 0 == refCount )
            {
            for ( int i = 0 ; i < port->mNumBufs ; i++)
                {

                if ( port->mBufferHeader[i]->pBuffer == frameBuf )
                    {
                    CAMHAL_LOGDB("Sending Frame 0x%x back to Ducati for filling", (unsigned int) frameBuf);
                    eError = OMX_FillThisBuffer(mCameraAdapterParameters.mHandleComp, port->mBufferHeader[i]);
                    if ( eError != OMX_ErrorNone )
                        {
                        CAMHAL_LOGEB("OMX_FillThisBuffer %d", eError);
                        return BAD_VALUE;
                        }
                    }
                }
              }
        }

    mVideoBuffersAvailable.clear();

    ///Do nothing as preview and capture port are one and the same
    mRecording = false;

    return NO_ERROR;

}


//API to cancel a currently executing command
status_t OMXCameraAdapter::cancelCommand(int operation)
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR;
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

    if ( OMX_StateLoaded != mComponentState )
        {
        CAMHAL_LOGEA("Calling queryBufferPreviewResolution() when not in LOADED state");
        width = -1;
        height = -1;
        goto exit;
        }

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

    if ( NO_ERROR == ret )
        {
        ret = setFormat (mCameraAdapterParameters.mPrevPortIndex, mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex]);
        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("setFormat() failed %d", ret);
            }
        }

    if(mCapMode == OMXCameraAdapter::VIDEO_MODE)
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
            mVnfEnabled = false;
            ///Enable/Disable Video Noise Filter
            ret = enableVideoNoiseFilter(mVnfEnabled);
            }

        if ( NO_ERROR != ret)
            {
            CAMHAL_LOGEB("Error configuring VNF %x", ret);
            }

        if ( NO_ERROR == ret )
            {
            mVstabEnabled = false;
            ///Enable/Disable Video Stabilization
            ret = enableVideoStabilization(mVstabEnabled);
            }

        if ( NO_ERROR != ret)
            {
            CAMHAL_LOGEB("Error configuring VSTAB %x", ret);
            }
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
    LOG_FUNCTION_NAME
    if(!mEventSignalQ.isEmpty())
        {
        CAMHAL_LOGDA("Event queue not empty");
        Message msg;
        mEventSignalQ.get(&msg);
        CAMHAL_LOGVB("msg.command = %d, msg.arg1 = %d, msg.arg2 = %d, msg.arg3 = %d", msg.command
                                                                            , msg.arg1
                                                                            , msg.arg2
                                                                            , msg.arg3);
        ///If any of the message parameters are not set, then that is taken as listening for all events/event parameters
        if((msg.command!=0 || msg.command == (unsigned int)(eEvent))
            && (!msg.arg1 || (OMX_U32)msg.arg1 == nData1)
            && (!msg.arg2 || (OMX_U32)msg.arg2 == nData2)
            && msg.arg3)
            {
            Semaphore *sem  = (Semaphore*) msg.arg3;
            CAMHAL_LOGDA("Event matched, signalling sem");
            ///Signal the semaphore provided
            sem->Signal();
            }
        else
            {
            ///Put the message back in the queue
            CAMHAL_LOGDA("Event didnt match, putting the message back in Q");
            mEventSignalQ.put(&msg);
            }
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
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = (unsigned int)eEvent;
    msg.arg1 = (void*)nData1;
    msg.arg2 = (void*)nData2;
    msg.arg3 = (void*)&semaphore;
    msg.arg4 = (void*)hComponent;

    LOG_FUNCTION_NAME_EXIT
    return mEventSignalQ.put(&msg);
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

    status_t  ret = NO_ERROR;
    status_t  res1, res2;
    OMXCameraPortParameters  *pPortParam;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    CameraFrame::FrameType typeOfFrame = CameraFrame::ALL_FRAMES;
    unsigned int refCount = 0;

    res1 = res2 = -1;
    pPortParam = &(mCameraAdapterParameters.mCameraPortParams[pBuffHeader->nOutputPortIndex]);
    if (pBuffHeader->nOutputPortIndex == OMX_CAMERA_PORT_VIDEO_OUT_PREVIEW)
        {
        recalculateFPS();

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

        ///On the fly update to 3A settings not working
        if( mPending3Asettings )
            Apply3Asettings(mParameters3A);

        if( mWaitingForSnapshot )
            {
            typeOfFrame = CameraFrame::SNAPSHOT_FRAME;
            mSnapshotCount++;
            //CAMHAL_LOGDB("Snapshot Frame 0x%x refCount start %d", (unsigned int) pBuffHeader->pBuffer, mFrameSubscribers.size());
            }
        else
            {
            typeOfFrame = CameraFrame::PREVIEW_FRAME_SYNC;
            //CAMHAL_LOGDB("Preview Frame 0x%x refCount start %d", (unsigned int) pBuffHeader->pBuffer, mFrameSubscribers.size());
            }

                {
                Mutex::Autolock lock(mPreviewBufferLock);
                refCount += mFrameSubscribers.size();
                ///CAMHAL_LOGDB("Preview Frame 0x%x refCount start %d", (uint32_t)pBuffHeader->pBuffer,(int) mFrameSubscribers.size());
                mPreviewBuffersAvailable.replaceValueFor(  ( unsigned int ) pBuffHeader->pBuffer, mFrameSubscribers.size());
                }

            res1 = sendFrameToSubscribers(pBuffHeader, typeOfFrame, pPortParam);

        if ( mRecording )
            {
            typeOfFrame = CameraFrame::VIDEO_FRAME_SYNC;
                {
                Mutex::Autolock lock(mVideoBufferLock);
                refCount += mVideoSubscribers.size();
                //CAMHAL_LOGDB("Video Frame 0x%x refCount start %d", pBuffHeader->pBuffer, mVideoSubscribers.size());
                mVideoBuffersAvailable.replaceValueFor( ( unsigned int ) pBuffHeader->pBuffer, mVideoSubscribers.size());
                }

            res2  = sendFrameToSubscribers(pBuffHeader, typeOfFrame, pPortParam);
            }

        ret = ( ( NO_ERROR == res1) || ( NO_ERROR == res2 ) ) ? ( (int)NO_ERROR ) : ( -1 );

        }
    else if( pBuffHeader->nOutputPortIndex == OMX_CAMERA_PORT_IMAGE_OUT_IMAGE )
        {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

        CameraHal::PPM("Shot to Jpeg: ", &mStartCapture);

#endif

        if ( OMX_COLOR_FormatUnused == mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mImagePortIndex].mColorFormat )
            {
            typeOfFrame = CameraFrame::IMAGE_FRAME;
            }
        else
            {
            typeOfFrame = CameraFrame::RAW_FRAME;
            }

            pPortParam->mImageType = typeOfFrame;

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
        ret = sendFrameToSubscribers(pBuffHeader, typeOfFrame, pPortParam);
        }
    else
        {
        CAMHAL_LOGEA("Frame received for non-(preview/capture) port. This is yet to be supported");
        goto EXIT;
        }

    if(ret != NO_ERROR)
        {

        CAMHAL_LOGEA("Error in sending frames to subscribers");
        CAMHAL_LOGDB("sendFrameToSubscribers error: %d", ret);

        if ( 1 <= refCount )
            {
            returnFrame(pBuffHeader->pBuffer, typeOfFrame);
            }
        }

    return eError;

    EXIT:

    CAMHAL_LOGEB("Exiting function %s because of ret %d eError=%x", __FUNCTION__, ret, eError);

    if ( NO_ERROR != ret )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(eError);
            }
        }

    return eError;
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


status_t OMXCameraAdapter::sendFrameToSubscribers(OMX_IN OMX_BUFFERHEADERTYPE *pBuffHeader, int typeOfFrame, OMXCameraPortParameters *port)
{
    status_t ret = NO_ERROR;
    int refCount;

    frame_callback callback;
    CameraFrame cFrame;

//    LOG_FUNCTION_NAME

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
        cFrame.mFrameType = typeOfFrame;
        cFrame.mBuffer = pBuffHeader->pBuffer;
        cFrame.mLength = pBuffHeader->nFilledLen;
        cFrame.mAlignment =port->mStride;
        cFrame.mOffset = pBuffHeader->nOffset;

        //@todo Do timestamp correction by subtracting IPC delay using timestamp driver
        cFrame.mTimestamp = systemTime(SYSTEM_TIME_MONOTONIC);;
        cFrame.mOffset = pBuffHeader->nOffset;

        if( ( CameraFrame::IMAGE_FRAME == typeOfFrame ) ||
            ( CameraFrame::RAW_FRAME == typeOfFrame ) )
            {
            for (uint32_t i = 0 ; i < mImageSubscribers.size(); i++ )
                {
                cFrame.mCookie = (void *) mImageSubscribers.keyAt(i);
                callback = (frame_callback) mImageSubscribers.valueAt(i);
                callback(&cFrame);
                }
            }
        else if ( CameraFrame::VIDEO_FRAME_SYNC == typeOfFrame )
            {
            OMXCameraPortParameters *cap;
            cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
            cFrame.mWidth = cap->mWidth;
            cFrame.mHeight = cap->mHeight;

            for(uint32_t i = 0 ; i < mVideoSubscribers.size(); i++ )
                {
                cFrame.mCookie = (void *) mVideoSubscribers.keyAt(i);
                callback = (frame_callback) mVideoSubscribers.valueAt(i);
                callback(&cFrame);
                }
            }
        else if ( ( CameraFrame::PREVIEW_FRAME_SYNC == typeOfFrame ) || ( CameraFrame::SNAPSHOT_FRAME == typeOfFrame ) )
            {
            //Currently send the snapshot callback as shutter callback
            if( (CameraFrame::SNAPSHOT_FRAME == typeOfFrame) && (mSnapshotCount==1) ){
                if ( mShutterSubscribers.size() == 0 )
                    {
                    CAMHAL_LOGEA("No shutter Subscribers!");
                    }

                CameraHalEvent shutterEvent;
                event_callback eventCb;
                shutterEvent.mEventType = CameraHalEvent::EVENT_SHUTTER;
                shutterEvent.mEventData.shutterEvent.shutterClosed = true;
                for (unsigned int i = 0 ; i < mShutterSubscribers.size(); i++ )
                    {
                    shutterEvent.mCookie = (void *) mShutterSubscribers.keyAt(i);
                    eventCb = (event_callback) mShutterSubscribers.valueAt(i);
                    CAMHAL_LOGEA("Sending shutter callback");
                    eventCb ( &shutterEvent );
                    }
            }

            OMXCameraPortParameters *cap;
            cap = &mCameraAdapterParameters.mCameraPortParams[mCameraAdapterParameters.mPrevPortIndex];
            cFrame.mWidth = cap->mWidth;
            cFrame.mHeight = cap->mHeight;

            /*static int bufCount =0;
                        if((bufCount>100) && (bufCount<103))
                        saveFile(( unsigned char*) cFrame.mBuffer, cFrame.mWidth, cFrame.mHeight, 0);
                        bufCount++;*/

            for(uint32_t i = 0 ; i < mFrameSubscribers.size(); i++ )
                {
                cFrame.mCookie = (void *) mFrameSubscribers.keyAt(i);
                callback = (frame_callback) mFrameSubscribers.valueAt(i);
                callback(&cFrame);
                }
            }
        else
            {
            ret = -EINVAL;
            }
        }

//    LOG_FUNCTION_NAME_EXIT

    if ( NO_ERROR != ret )
        {
        if ( NULL != mErrorNotifier )
            {
            mErrorNotifier->errorNotify(ret);
            }
        }

    return ret;
}

OMX_ERRORTYPE OMXCameraAdapter::Apply3Asettings( Gen3A_settings& Gen3A )
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    unsigned int currSett; // 32 bit
    int portIndex;

    for( currSett = 1; currSett < E3aSettingMax; currSett <<= 1)
        {
        if( currSett & mPending3Asettings )
            {
            switch( currSett )
                {
                case SetEVCompensation:
                    {
                    OMX_CONFIG_EXPOSUREVALUETYPE expValues;
                    expValues.nSize = sizeof(OMX_CONFIG_EXPOSUREVALUETYPE);
                    expValues.nVersion = mLocalVersionParam;
                    expValues.nPortIndex = OMX_ALL;

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
                    OMX_CONFIG_WHITEBALCONTROLTYPE wb;
                    wb.nSize = sizeof(OMX_CONFIG_WHITEBALCONTROLTYPE);
                    wb.nVersion = mLocalVersionParam;
                    wb.nPortIndex = OMX_ALL;
                    wb.eWhiteBalControl = (OMX_WHITEBALCONTROLTYPE)Gen3A.WhiteBallance;

                    CAMHAL_LOGDB("White Ballance for Hal = %d", Gen3A.WhiteBallance);
                    CAMHAL_LOGDB("White Ballance for OMX = %d", (int)wb.eWhiteBalControl);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonWhiteBalance, &wb);
                    break;
                    }

                case SetFlicker:
                    {
                    OMX_CONFIG_FLICKERCANCELTYPE flicker;
                    flicker.nSize = sizeof(OMX_CONFIG_FLICKERCANCELTYPE);
                    flicker.nVersion = mLocalVersionParam;
                    flicker.nPortIndex = OMX_ALL;
                    flicker.eFlickerCancel = (OMX_COMMONFLICKERCANCELTYPE)Gen3A.Flicker;

                    CAMHAL_LOGDB("Flicker for Hal = %d", Gen3A.Flicker);
                    CAMHAL_LOGDB("Flicker for  OMX= %d", (int)flicker.eFlickerCancel);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexConfigFlickerCancel, &flicker );
                    break;
                    }

                case SetBrightness:
                    {
                    OMX_CONFIG_BRIGHTNESSTYPE brightness;
                    brightness.nSize = sizeof(OMX_CONFIG_BRIGHTNESSTYPE);
                    brightness.nVersion = mLocalVersionParam;
                    brightness.nPortIndex = OMX_ALL;
                    brightness.nBrightness = Gen3A.Brightness;

                    CAMHAL_LOGDB("Brightness for Hal and OMX= %d", (int)Gen3A.Brightness);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonBrightness, &brightness);
                    break;
                    }

                case SetContrast:
                    {
                    OMX_CONFIG_CONTRASTTYPE contrast;
                    contrast.nSize = sizeof(OMX_CONFIG_CONTRASTTYPE);
                    contrast.nVersion = mLocalVersionParam;
                    contrast.nPortIndex = OMX_ALL;
                    contrast.nContrast = Gen3A.Contrast;

                    CAMHAL_LOGDB("Contrast for Hal and OMX= %d", (int)Gen3A.Contrast);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonContrast, &contrast);
                    break;
                    }

                case SetSharpness:
                    {
                    OMX_IMAGE_CONFIG_PROCESSINGLEVELTYPE procSharpness;
                    procSharpness.nSize = sizeof( OMX_IMAGE_CONFIG_PROCESSINGLEVELTYPE );
                    procSharpness.nVersion = mLocalVersionParam;
                    procSharpness.nPortIndex = OMX_ALL;
                    procSharpness.nLevel = Gen3A.Sharpness;

                    if( procSharpness.nLevel == 0 )
                        procSharpness.bAuto = OMX_TRUE;

                    procSharpness.bAuto = OMX_FALSE;
                    CAMHAL_LOGDB("Sharpness for Hal and OMX= %d", (int)Gen3A.Sharpness);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, (OMX_INDEXTYPE)OMX_IndexConfigSharpeningLevel, &procSharpness);
                    break;
                    }

                case SetSaturation:
                    {
                    OMX_CONFIG_SATURATIONTYPE saturation;
                    saturation.nSize = sizeof(OMX_CONFIG_SATURATIONTYPE);
                    saturation.nVersion = mLocalVersionParam;
                    saturation.nPortIndex = OMX_ALL;
                    saturation.nSaturation = Gen3A.Saturation;

                    CAMHAL_LOGDB("Saturation for Hal and OMX= %d", (int)Gen3A.Saturation);
                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonSaturation, &saturation);
                    break;
                    }

                case SetISO:
                    {
                    OMX_CONFIG_EXPOSUREVALUETYPE expValues;
                    expValues.nSize = sizeof(OMX_CONFIG_EXPOSUREVALUETYPE);
                    expValues.nVersion = mLocalVersionParam;
                    expValues.nPortIndex = OMX_ALL;

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
                    effect.nSize = sizeof(OMX_CONFIG_IMAGEFILTERTYPE);
                    effect.nVersion = mLocalVersionParam;
                    effect.nPortIndex = OMX_ALL;
                    effect.eImageFilter = (OMX_IMAGEFILTERTYPE)Gen3A.Effect;

                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigCommonImageFilter, &effect);
                    CAMHAL_LOGDB("effect for OMX = 0x%x", (int)effect.eImageFilter);
                    CAMHAL_LOGDB("effect for Hal = %d", Gen3A.Effect);
                    break;
                    }

                case SetFocus:
                    {
                    OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE focus;
                    focus.nSize = sizeof(OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE);
                    focus.nVersion = mLocalVersionParam;
                    focus.nPortIndex = OMX_ALL;

                    focus.eFocusControl = (OMX_IMAGE_FOCUSCONTROLTYPE)Gen3A.Focus;

                    ret = OMX_SetConfig( mCameraAdapterParameters.mHandleComp, OMX_IndexConfigFocusControl, &focus);
                    CAMHAL_LOGDB("Focus type in hal , OMX : %d , 0x%x", Gen3A.Focus, focus.eFocusControl );
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

OMXCameraAdapter::OMXCameraAdapter():mComponentState (OMX_StateInvalid)
{
    LOG_FUNCTION_NAME

    mFocusStarted = false;
    mPictureRotation = 0;

    mCameraAdapterParameters.mHandleComp = 0;
    signal(SIGTERM, SigHandler);
    signal(SIGALRM, SigHandler);

    LOG_FUNCTION_NAME_EXIT
}

OMXCameraAdapter::~OMXCameraAdapter()
{
    LOG_FUNCTION_NAME

   ///Free the handle for the Camera component
    if(mCameraAdapterParameters.mHandleComp)
        {
        OMX_FreeHandle(mCameraAdapterParameters.mHandleComp);
        }

    ///De-init the OMX
    if(mComponentState==OMX_StateLoaded)
        {
        OMX_Deinit();
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


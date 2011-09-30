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
#include "OverlayDisplayAdapter.h"
#include "TICameraParameters.h"
#include "CameraProperties.h"
#include "overlay_common.h"
#include <cutils/properties.h>


#include <poll.h>
#include <math.h>

#define ADAPTER_TIMEOUT 5 //[s.]

///@remarks added for testing purposes

namespace android {
/*****************************************************************************/

wp<CameraHardwareInterface> CameraHal::singleton[];

////Constant definitions and declarations
////@todo Have a CameraProperties class to store these parameters as constants for every camera
////       Currently, they are hard-coded

const int CameraHal::NO_BUFFERS_PREVIEW = MAX_CAMERA_BUFFERS;
const int CameraHal::NO_BUFFERS_IMAGE_CAPTURE = 2;

const uint32_t MessageNotifier::EVENT_BIT_FIELD_POSITION = 0;
const uint32_t MessageNotifier::FRAME_BIT_FIELD_POSITION = 0;

const char CameraHal::PARAMS_DELIMITER []= ",";


static sp<CameraProperties> gCameraProperties = NULL;

/******************************************************************************/

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

struct timeval CameraHal::ppm_start;
struct timeval CameraHal::mStartPreview;
struct timeval CameraHal::mStartFocus;
struct timeval CameraHal::mStartCapture;

#endif

/*-------------Camera Hal Interface Method definitions STARTS here--------------------*/

/**
   @brief Return the IMemoryHeap for the preview image heap

   @param none
   @return NULL
   @remarks This interface is not supported
   @note This function can be useful for Surface flinger render cases
   @todo Support this function

 */
sp<IMemoryHeap> CameraHal::getPreviewHeap() const
    {
    LOG_FUNCTION_NAME
    return ((mAppCallbackNotifier.get())?mAppCallbackNotifier->getPreviewHeap():NULL);
    }

/**
   @brief Return the IMemoryHeap for the raw image heap

     @param none
     @return NULL
     @remarks This interface is not supported
     @todo Investigate why this function might be needed

*/
sp<IMemoryHeap>  CameraHal::getRawHeap() const
{
    LOG_FUNCTION_NAME
    return NULL;
}

/**
   @brief Set the notification and data callbacks

   @param[in] notify_cb Notify callback for notifying the app about events and errors
   @param[in] data_cb   Buffer callback for sending the preview/raw frames to the app
   @param[in] data_cb_timestamp Buffer callback for sending the video frames w/ timestamp
   @param[in] user  Callback cookie
   @return none

 */
void CameraHal::setCallbacks(notify_callback notify_cb,
                                      data_callback data_cb,
                                      data_callback_timestamp data_cb_timestamp,
                                      void* user)
{
    LOG_FUNCTION_NAME

    if ( NULL != mAppCallbackNotifier.get() )
        {
            mAppCallbackNotifier->setCallbacks(this,
                                                                        notify_cb,
                                                                        data_cb,
                                                                        data_cb_timestamp,
                                                                        user);
        }

    LOG_FUNCTION_NAME_EXIT
}

/**
   @brief Enable a message, or set of messages.

   @param[in] msgtype Bitmask of the messages to enable (defined in include/ui/Camera.h)
   @return none

 */
void CameraHal::enableMsgType(int32_t msgType)
{
    LOG_FUNCTION_NAME

    if ( ( msgType & CAMERA_MSG_SHUTTER ) && ( !mShutterEnabled ) )
        {
        msgType &= ~CAMERA_MSG_SHUTTER;
        }

    {
    Mutex::Autolock lock(mLock);
    mMsgEnabled |= msgType;
    }

    if(mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME)
    {
        if(mDisplayPaused)
        {
            CAMHAL_LOGDA("Preview currently paused...will enable preview callback when restarted");
            msgType &= ~CAMERA_MSG_PREVIEW_FRAME;
        }else
        {
            CAMHAL_LOGDA("Enabling Preview Callback");
        }
    }
    else
    {
        CAMHAL_LOGDB("Preview callback not enabled %x", msgType);
    }


    ///Configure app callback notifier with the message callback required
    mAppCallbackNotifier->enableMsgType (msgType);

    ///@todo once preview callback is supported, enable preview here if preview start in progress
    ///@todo Once the bug in OMXCamera related to flush buffers is removed, and external buffering
    ////in overlay is supported, we can start preview here

    LOG_FUNCTION_NAME_EXIT
}

/**
   @brief Disable a message, or set of messages.

   @param[in] msgtype Bitmask of the messages to disable (defined in include/ui/Camera.h)
   @return none

 */
void CameraHal::disableMsgType(int32_t msgType)
{
    LOG_FUNCTION_NAME

        {
        Mutex::Autolock lock(mLock);
        mMsgEnabled &= ~msgType;
        }

    if( msgType & CAMERA_MSG_PREVIEW_FRAME)
        {
        CAMHAL_LOGDA("Disabling Preview Callback");
        }

    ///Configure app callback notifier
    mAppCallbackNotifier->disableMsgType (msgType);

    LOG_FUNCTION_NAME_EXIT
}

/**
   @brief Query whether a message, or a set of messages, is enabled.

   Note that this is operates as an AND, if any of the messages queried are off, this will
   return false.

   @param[in] msgtype Bitmask of the messages to query (defined in include/ui/Camera.h)
   @return true If all message types are enabled
          false If any message type

 */
bool CameraHal::msgTypeEnabled(int32_t msgType)
{
    LOG_FUNCTION_NAME
    Mutex::Autolock lock(mLock);
    LOG_FUNCTION_NAME_EXIT
    return (mMsgEnabled & msgType);
}

/**
   @brief Set the camera parameters.

   @param[in] params Camera parameters to configure the camera
   @return NO_ERROR
   @todo Define error codes

 */
status_t CameraHal::setParameters(const CameraParameters &params)
{

   LOG_FUNCTION_NAME

    bool isS3d = false;
    int w_coef = 1, h_coef = 1;
    int w, h;
    int framerate,minframerate;
    bool framerateUpdated = true;
    int maxFPS, minFPS;
    int error;
    int base;
    const char *valstr = NULL;
    const char *prevFormat;
    char *af_coord;
    Message msg;
    status_t ret = NO_ERROR;

    Mutex::Autolock lock(mLock);

    //This can only happen when there is a mismatch
    //between the Camera application default
    //CameraHal's own default at the start
    if ( mReloadAdapter )
        {
         // if ( NULL != mCameraAdapter )
            // {
             // Free the camera adapter
             //mCameraAdapter.clear();

             //Close the camera adapter DLL
             //::dlclose(mCameraAdapterHandle);
            // }

        if ( reloadAdapter() < 0 )
            {
            CAMHAL_LOGEA("CameraAdapter reload failed");
            }

        mReloadAdapter = false;
        }

    if ((valstr = params.get(TICameraParameters::KEY_S3D_SUPPORTED)) != NULL)
        {
        isS3d = (!strcmp(valstr, "true"));
        }

    if (isS3d &&
        (valstr = params.get(TICameraParameters::KEY_S3D_FRAME_LAYOUT)) != NULL)
        {
        if (!strcmp(valstr, TICameraParameters::S3D_TB_FULL))
            h_coef = 2;
        else if (!strcmp(valstr, TICameraParameters::S3D_SS_FULL))
            w_coef = 2;
        }

    if (isS3d &&
        (valstr = params.get(TICameraParameters::KEY_SEI_ENCODING_TYPE)) != NULL)
        {
        CAMHAL_LOGDB("SEI encoding type: %s", valstr);
        mParameters.set(TICameraParameters::KEY_SEI_ENCODING_TYPE, valstr);
        }
    else if (isS3d) 
	{
        mParameters.set(TICameraParameters::KEY_SEI_ENCODING_TYPE, TICameraParameters::SEI_ENCODING_2010);
	}
    else
        {
        CAMHAL_LOGDB("SEI encoding type: %s", TICameraParameters::SEI_ENCODING_NONE);
        mParameters.set(TICameraParameters::KEY_SEI_ENCODING_TYPE, TICameraParameters::SEI_ENCODING_NONE);
        }

    ///Ensure that preview is not enabled when the below parameters are changed.
    if (!previewEnabled())
        {

        CAMHAL_LOGDB("PreviewFormat %s", params.getPreviewFormat());

        if ( !isParameterValid(params.getPreviewFormat(), (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_FORMATS]->mPropValue))
            {
            CAMHAL_LOGEB("Invalid preview format %s",  (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_FORMATS]->mPropValue);
            ret = -EINVAL;
            }
        else
            {
            if ( (valstr = params.getPreviewFormat()) != NULL)
                mParameters.setPreviewFormat(valstr);
                // in gingerbread, preview format and video format are still the same
                // so set video format accordingly here.
            if ( (valstr = params.getPreviewFormat()) != NULL)
                mParameters.set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, valstr);
            }

        if (isS3d)
            {
            if((valstr = params.get(TICameraParameters::KEY_S3D2D_PREVIEW)) != NULL)
                {
                CAMHAL_LOGDB("Stereo 3D->2D Preview mode is %s", valstr);
                mParameters.set(TICameraParameters::KEY_S3D2D_PREVIEW, valstr);
                }

            if((valstr = params.get(TICameraParameters::KEY_S3D_FRAME_LAYOUT)) != NULL)
                {
                CAMHAL_LOGDB("Stereo 3D image layout is %s", valstr);
                mParameters.set(TICameraParameters::KEY_S3D_FRAME_LAYOUT, valstr);
                }
            }

        params.getPreviewSize(&w, &h);
        int orientation =0;
        if((valstr = params.get(TICameraParameters::KEY_SENSOR_ORIENTATION)) != NULL)
            {
            CAMHAL_LOGDB("Sensor Orientation is set to %s", params.get(TICameraParameters::KEY_SENSOR_ORIENTATION));
            mParameters.set(TICameraParameters::KEY_SENSOR_ORIENTATION, valstr);
            orientation = params.getInt(TICameraParameters::KEY_SENSOR_ORIENTATION);
            }

        if(orientation == 90 || orientation == 270)
            {
            if ( !isResolutionValid(h / h_coef, w / w_coef, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_SIZES]->mPropValue))
                {
                CAMHAL_LOGEB("Invalid preview resolution %d x %d", w, h);
                ret = -EINVAL;
                }
            else
                {
                mParameters.setPreviewSize(w, h);
                }
            }
        else
            {
            if ( !isResolutionValid(w / w_coef, h / h_coef, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_SIZES]->mPropValue))
                {
                CAMHAL_LOGEB("Invalid preview resolution %d x %d", w, h);
                ret = -EINVAL;
                }
            else
                {
                mParameters.setPreviewSize(w, h);
                }
            }

        CAMHAL_LOGDB("Preview Size by App %d x %d", w, h);

        if(( (valstr = params.get(TICameraParameters::KEY_VNF)) != NULL)
            && ((params.getInt(TICameraParameters::KEY_VNF)==0) || (params.getInt(TICameraParameters::KEY_VNF)==1)))
            {
            CAMHAL_LOGDB("VNF set %s", valstr);
            mParameters.set(TICameraParameters::KEY_VNF, valstr);
            }

        if(( (valstr = params.get(TICameraParameters::KEY_VSTAB)) != NULL)
             && ((params.getInt(TICameraParameters::KEY_VSTAB)==0) || (params.getInt(TICameraParameters::KEY_VSTAB)==1)))
            {
            CAMHAL_LOGDB("VSTAB set %s", valstr);
            mParameters.set(TICameraParameters::KEY_VSTAB, valstr);
            }

        if( (valstr = params.get(TICameraParameters::KEY_CAP_MODE)) != NULL)
            {
            CAMHAL_LOGDB("Capture mode set %s", valstr);
            mParameters.set(TICameraParameters::KEY_CAP_MODE, valstr);
            }

        if((valstr = params.get(TICameraParameters::KEY_IPP)) != NULL)
            {
            CAMHAL_LOGDB("IPP mode set %s", valstr);
            mParameters.set(TICameraParameters::KEY_IPP, valstr);
            }
        }

    ///Below parameters can be changed when the preview is running

    //Manual Exposure and Manual Gain

    if( ( valstr = params.get(TICameraParameters::KEY_MANUAL_EXPOSURE_LEFT) ) != NULL
        && (params.getInt(TICameraParameters::KEY_MANUAL_EXPOSURE_LEFT) <= MAX_MANUAL_EXPOSURE_MS))
        {
        CAMHAL_LOGEB("Manual Exposure Left set %s ms", valstr);
        mParameters.set(TICameraParameters::KEY_MANUAL_EXPOSURE_LEFT, valstr);
        }

    if( ( valstr = params.get(TICameraParameters::KEY_MANUAL_EXPOSURE_RIGHT) ) != NULL
        && (params.getInt(TICameraParameters::KEY_MANUAL_EXPOSURE_RIGHT) <= MAX_MANUAL_EXPOSURE_MS))
        {
        CAMHAL_LOGEB("Manual Exposure Right set %s ms", valstr);
        mParameters.set(TICameraParameters::KEY_MANUAL_EXPOSURE_RIGHT, valstr);
        }

    if( ( valstr = params.get(TICameraParameters::KEY_MANUAL_GAIN_ISO_LEFT) ) != NULL
        && (params.getInt(TICameraParameters::KEY_MANUAL_GAIN_ISO_LEFT) <= MAX_MANUAL_GAIN_ISO))
        {
        CAMHAL_LOGEB("Manual Gain Left set %s ISO", valstr);
        mParameters.set(TICameraParameters::KEY_MANUAL_GAIN_ISO_LEFT, valstr);
        }

    if( ( valstr = params.get(TICameraParameters::KEY_MANUAL_GAIN_ISO_RIGHT) ) != NULL
        && (params.getInt(TICameraParameters::KEY_MANUAL_GAIN_ISO_RIGHT) <= MAX_MANUAL_GAIN_ISO))
        {
        CAMHAL_LOGEB("Manual Gain Right set %s ISO", valstr);
        mParameters.set(TICameraParameters::KEY_MANUAL_GAIN_ISO_RIGHT, valstr);
        }

    if ( !isParameterValid(params.getPictureFormat(),
        (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PICTURE_FORMATS]->mPropValue))
        {
        CAMHAL_LOGEA("Invalid picture format");
        ret = -EINVAL;
        }
    else
        {
        valstr = params.getPictureFormat();
        if (valstr)
            mParameters.setPictureFormat(valstr);
        }

    params.getPictureSize(&w, &h);

    if ( !isResolutionValid(w / w_coef, h / h_coef, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PICTURE_SIZES]->mPropValue))
        {
        CAMHAL_LOGEB("Invalid picture resolution %d(%d) x %d(%d) (from %s)", w, w_coef, h,h_coef, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PICTURE_SIZES]->mPropValue);
        //ret = -EINVAL;
        }
    else
        {
        mParameters.setPictureSize(w, h);
        }

    CAMHAL_LOGDB("Picture Size by App %d x %d", w, h);

    if(( (valstr = params.get(TICameraParameters::KEY_BURST)) != NULL)
        && (params.getInt(TICameraParameters::KEY_BURST) >=0))
        {
        CAMHAL_LOGEB("Burst set %s", valstr);
        mParameters.set(TICameraParameters::KEY_BURST, valstr);
        }

    framerate = params.getPreviewFrameRate();
    if ( isParameterValid(framerate, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_FRAME_RATES]->mPropValue))
        {
        if ( mLastPreviewFramerate != framerate )
            {
            mLastPreviewFramerate = framerate;
            mParameters.setPreviewFrameRate(framerate);
            framerateUpdated = true;
            }
        else
            {
            framerateUpdated = false;
            }
        }
    else
        {
        framerateUpdated = false;
        }

    CAMHAL_LOGEB("FRAMERATE %d", framerate);

    //If client uses fixed framerate than
    //give it a higher piority than VFR.
    if ( framerateUpdated )
        {

        minFPS = framerate;
        maxFPS = framerate;

        CAMHAL_LOGEB("FPS Range [%d, %d]", minFPS, maxFPS);
        mParameters.set(TICameraParameters::KEY_MINFRAMERATE, minFPS);
        mParameters.set(TICameraParameters::KEY_MAXFRAMERATE, maxFPS);
        mParameters.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, params.get(CameraParameters::KEY_PREVIEW_FPS_RANGE));
        }
    else if ( ( valstr = params.get(CameraParameters::KEY_PREVIEW_FPS_RANGE) ) != NULL )
        {
        CAMHAL_LOGEB("FPS Range = %s", valstr);
        params.getPreviewFpsRange(&minFPS, &maxFPS);

        if ( ( 0 > minFPS ) || ( 0 > maxFPS ) )
            {
            CAMHAL_LOGEA("FPS Range is negative!");
            return -EINVAL;
            }

        minFPS /= CameraHal::VFR_SCALE;
        maxFPS /= CameraHal::VFR_SCALE;

        if ( ( 0 == minFPS ) || ( 0 == maxFPS ) )
            {
            CAMHAL_LOGEA("FPS Range is invalid!");
            return -EINVAL;
            }

        if ( maxFPS < minFPS )
            {
            CAMHAL_LOGEA("Max FPS is smaller than Min FPS!");
            return -EINVAL;
            }

        if ( maxFPS > framerate )
            {
            framerate = maxFPS;
            mParameters.setPreviewFrameRate(framerate);
            }

        CAMHAL_LOGEB("FPS Range [%d, %d]", minFPS, maxFPS);
        mParameters.set(TICameraParameters::KEY_MINFRAMERATE, minFPS);
        mParameters.set(TICameraParameters::KEY_MAXFRAMERATE, maxFPS);
        mParameters.set(CameraParameters::KEY_PREVIEW_FPS_RANGE, valstr);
        }

    if( ( valstr = params.get(TICameraParameters::KEY_GBCE) ) != NULL )
        {
        CAMHAL_LOGDB("GBCE Value = %s", valstr);
        mParameters.set(TICameraParameters::KEY_GBCE, valstr);
        }

    if( ( valstr = params.get(TICameraParameters::KEY_GLBCE) ) != NULL )
        {
        CAMHAL_LOGDB("GLBCE Value = %s", valstr);
        mParameters.set(TICameraParameters::KEY_GLBCE, valstr);
        }

    if( (valstr = params.get(TICameraParameters::KEY_AUTOCONVERGENCE)) != NULL )
        {
        CAMHAL_LOGDB("AutoConvergence Mode is set = %s", valstr);
        mParameters.set(TICameraParameters::KEY_AUTOCONVERGENCE, valstr);
        }

    if( (valstr = params.get(TICameraParameters::KEY_MANUALCONVERGENCE_VALUES)) !=NULL )
        {
        CAMHAL_LOGDB("ManualConvergence Value = %s", valstr);
        mParameters.set(TICameraParameters::KEY_MANUALCONVERGENCE_VALUES, valstr);
        }

    if( ((valstr = params.get(TICameraParameters::KEY_EXPOSURE_MODE)) != NULL)
        && isParameterValid(valstr,
        (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EXPOSURE_MODES]->mPropValue))
        {
        CAMHAL_LOGDB("Exposure set = %s", valstr);
        mParameters.set(TICameraParameters::KEY_EXPOSURE_MODE, valstr);
        }

    if( ((valstr = params.get(CameraParameters::KEY_WHITE_BALANCE)) != NULL)
        && isParameterValid(valstr,
        (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_WHITE_BALANCE]->mPropValue))
        {
        CAMHAL_LOGDB("White balance set %s", valstr);
        mParameters.set(CameraParameters::KEY_WHITE_BALANCE, valstr);
        }

    if( ((valstr = params.get(TICameraParameters::KEY_CONTRAST)) != NULL)
            && (params.getInt(TICameraParameters::KEY_CONTRAST) >= 0 ))
        {
        CAMHAL_LOGDB("Contrast set %s", valstr);
        mParameters.set(TICameraParameters::KEY_CONTRAST, valstr);
        }

    if( ((valstr =params.get(TICameraParameters::KEY_SHARPNESS)) != NULL) && params.getInt(TICameraParameters::KEY_SHARPNESS) >= 0 )
        {
        CAMHAL_LOGDB("Sharpness set %s", valstr);
        mParameters.set(TICameraParameters::KEY_SHARPNESS, valstr);
        }


    if( ((valstr = params.get(TICameraParameters::KEY_SATURATION)) != NULL)
        && (params.getInt(TICameraParameters::KEY_SATURATION) >= 0 ) )
        {
        CAMHAL_LOGDB("Saturation set %s", valstr);
        mParameters.set(TICameraParameters::KEY_SATURATION, valstr);
        }

    if( ((valstr = params.get(TICameraParameters::KEY_BRIGHTNESS)) != NULL)
        && (params.getInt(TICameraParameters::KEY_BRIGHTNESS) >= 0 ))
        {
        CAMHAL_LOGDB("Brightness set %s", valstr);
        mParameters.set(TICameraParameters::KEY_BRIGHTNESS, valstr);
        }


    if( ((valstr = params.get(CameraParameters::KEY_ANTIBANDING)) != NULL)
        && isParameterValid(valstr,
        (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ANTIBANDING]->mPropValue))
        {
        CAMHAL_LOGDB("Antibanding set %s", valstr);
        mParameters.set(CameraParameters::KEY_ANTIBANDING, valstr);
        }

    if( ((valstr = params.get(TICameraParameters::KEY_ISO)) != NULL)
        && isParameterValid(valstr,
        (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ISO_VALUES]->mPropValue))
        {
        CAMHAL_LOGDB("ISO set %s", valstr);
        mParameters.set(TICameraParameters::KEY_ISO, valstr);
        }

    if( ((valstr = params.get(CameraParameters::KEY_FOCUS_MODE)) != NULL)
        && isParameterValid(valstr,
        (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_FOCUS_MODES]->mPropValue))
        {
        CAMHAL_LOGDB("Focus mode set %s", valstr);
        mParameters.set(CameraParameters::KEY_FOCUS_MODE, valstr);
        }

    if( (valstr = params.get(TICameraParameters::KEY_TOUCH_POS)) != NULL )
        {
        CAMHAL_LOGDB("Touch Focus position set %s", valstr);
        mParameters.set(TICameraParameters::KEY_TOUCH_POS, valstr);
        }

    if( (valstr = params.get(TICameraParameters::KEY_FACE_DETECTION_ENABLE)) != NULL )
        {
        CAMHAL_LOGDB("Face detection set to %s", valstr);
        mParameters.set(TICameraParameters::KEY_FACE_DETECTION_ENABLE, valstr);
        }

    if( (valstr = params.get(TICameraParameters::KEY_FACE_DETECTION_THRESHOLD)) != NULL )
        {
        CAMHAL_LOGDB("Face detection threshold set to %s", valstr);
        mParameters.set(TICameraParameters::KEY_FACE_DETECTION_THRESHOLD, valstr);
        }

    if (isS3d)
        {
        // Hard-coded -> Measurement should always be enabled in S3D
        mParameters.set(TICameraParameters::KEY_MEASUREMENT_ENABLE, TICameraParameters::MEASUREMENT_ENABLE);
        mMeasurementEnabled = true;
        }
    else if( (valstr = params.get(TICameraParameters::KEY_MEASUREMENT_ENABLE)) != NULL )
        {
        CAMHAL_LOGDB("Measurements set to %s", valstr);
        mParameters.set(TICameraParameters::KEY_MEASUREMENT_ENABLE, valstr);

        if (strcmp(valstr, (const char *) TICameraParameters::MEASUREMENT_ENABLE) == 0)
            {
            mMeasurementEnabled = true;
            }
        else
            {
            mMeasurementEnabled = false;
            }
        }
    else
        {
        mMeasurementEnabled = false;
        }

    if( (valstr = params.get(CameraParameters::KEY_EXPOSURE_COMPENSATION)) != NULL)
        {
        CAMHAL_LOGDB("Exposure compensation set %s", valstr);
        mParameters.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, valstr);
        }

    if(( (valstr = params.get(CameraParameters::KEY_SCENE_MODE)) != NULL)
        && isParameterValid(valstr,
        (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_SCENE_MODES]->mPropValue))
        {
        CAMHAL_LOGDB("Scene mode set %s", valstr);
        mParameters.set(CameraParameters::KEY_SCENE_MODE, valstr);
        }

    if(( (valstr = params.get(CameraParameters::KEY_FLASH_MODE)) != NULL)
        && isParameterValid(valstr,
        (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_FLASH_MODES]->mPropValue))
        {
        CAMHAL_LOGDB("Flash mode set %s", valstr);
        mParameters.set(CameraParameters::KEY_FLASH_MODE, valstr);
        }

    if(( (valstr = params.get(CameraParameters::KEY_EFFECT)) != NULL)
        && isParameterValid(valstr,
        (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EFFECTS]->mPropValue))
        {
        CAMHAL_LOGDB("Effect set %s", valstr);
        mParameters.set(CameraParameters::KEY_EFFECT, valstr);
        }

    if(( (valstr = params.get(CameraParameters::KEY_ROTATION)) != NULL)
        && (params.getInt(CameraParameters::KEY_ROTATION) >=0))
        {
        CAMHAL_LOGDB("Rotation set %s", valstr);
        mParameters.set(CameraParameters::KEY_ROTATION, valstr);
        }

    if(( (valstr = params.get(CameraParameters::KEY_JPEG_QUALITY)) != NULL)
        && (params.getInt(CameraParameters::KEY_JPEG_QUALITY) >=0))
        {
        CAMHAL_LOGDB("Jpeg quality set %s", valstr);
        mParameters.set(CameraParameters::KEY_JPEG_QUALITY, valstr);
        }

    if(( (valstr = params.get(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH)) != NULL)
        && (params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH) >=0))
        {
        CAMHAL_LOGDB("Thumbnail width set %s", valstr);
        mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, valstr);
        }

    if(( (valstr = params.get(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT)) != NULL)
        && (params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT) >=0))
        {
        CAMHAL_LOGDB("Thumbnail width set %s", valstr);
        mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, valstr);
        }

    if(( (valstr = params.get(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY)) != NULL )
        && (params.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY) >=0))
        {
        CAMHAL_LOGDB("Thumbnail quality set %s", valstr);
        mParameters.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, valstr);
        }

    if( (valstr = params.get(CameraParameters::KEY_GPS_LATITUDE)) != NULL )
        {
        CAMHAL_LOGDB("GPS latitude set %s", valstr);
        mParameters.set(CameraParameters::KEY_GPS_LATITUDE, valstr);
        }else{
            mParameters.remove(CameraParameters::KEY_GPS_LATITUDE);
        }

    if( (valstr = params.get(CameraParameters::KEY_GPS_LONGITUDE)) != NULL )
        {
        CAMHAL_LOGDB("GPS longitude set %s", valstr);
        mParameters.set(CameraParameters::KEY_GPS_LONGITUDE, valstr);
        }else{
            mParameters.remove(CameraParameters::KEY_GPS_LONGITUDE);
        }

    if( (valstr = params.get(CameraParameters::KEY_GPS_ALTITUDE)) != NULL )
        {
        CAMHAL_LOGDB("GPS altitude set %s", valstr);
        mParameters.set(CameraParameters::KEY_GPS_ALTITUDE, valstr);
        }else{
            mParameters.remove(CameraParameters::KEY_GPS_ALTITUDE);
        }

    if( (valstr = params.get(CameraParameters::KEY_GPS_TIMESTAMP)) != NULL )
        {
        CAMHAL_LOGDB("GPS timestamp set %s", valstr);
        mParameters.set(CameraParameters::KEY_GPS_TIMESTAMP, valstr);
        }else{
            mParameters.remove(CameraParameters::KEY_GPS_TIMESTAMP);
        }

    if( (valstr = params.get(TICameraParameters::KEY_GPS_DATESTAMP)) != NULL )
        {
        CAMHAL_LOGDB("GPS datestamp set %s", valstr);
        mParameters.set(TICameraParameters::KEY_GPS_DATESTAMP, valstr);
        }else{
            mParameters.remove(TICameraParameters::KEY_GPS_DATESTAMP);
        }

    if( (valstr = params.get(CameraParameters::KEY_GPS_PROCESSING_METHOD)) != NULL )
        {
        CAMHAL_LOGDB("GPS processing method set %s", valstr);
        mParameters.set(CameraParameters::KEY_GPS_PROCESSING_METHOD, valstr);
        }else{
            mParameters.remove(CameraParameters::KEY_GPS_PROCESSING_METHOD);
        }

    if( (valstr = params.get(TICameraParameters::KEY_GPS_ALTITUDE_REF)) != NULL )
        {
        CAMHAL_LOGDB("GPS altitude ref set %s", valstr);
        mParameters.set(TICameraParameters::KEY_GPS_ALTITUDE_REF, valstr);
        }else{
            mParameters.remove(TICameraParameters::KEY_GPS_ALTITUDE_REF);
        }

    if( (valstr = params.get(TICameraParameters::KEY_GPS_MAPDATUM )) != NULL )
        {
        CAMHAL_LOGDB("GPS MAPDATUM set %s", valstr);
        mParameters.set(TICameraParameters::KEY_GPS_MAPDATUM, valstr);
        }else{
            mParameters.remove(TICameraParameters::KEY_GPS_MAPDATUM);
        }

    if( (valstr = params.get(TICameraParameters::KEY_GPS_VERSION)) != NULL )
        {
        CAMHAL_LOGDB("GPS MAPDATUM set %s", valstr);
        mParameters.set(TICameraParameters::KEY_GPS_VERSION, valstr);
        }else{
            mParameters.remove(TICameraParameters::KEY_GPS_VERSION);
        }

    if( (valstr = params.get(TICameraParameters::KEY_EXIF_MODEL)) != NULL )
        {
        CAMHAL_LOGDB("EXIF Model set %s", valstr);
        mParameters.set(TICameraParameters::KEY_EXIF_MODEL, valstr);
        }

    if( (valstr = params.get(TICameraParameters::KEY_EXIF_MAKE)) != NULL )
        {
        CAMHAL_LOGDB("EXIF Make set %s", valstr);
        mParameters.set(TICameraParameters::KEY_EXIF_MAKE, valstr);
        }

    if( (valstr = params.get(TICameraParameters::KEY_EXP_BRACKETING_RANGE)) != NULL )
        {
        CAMHAL_LOGDB("Exposure Bracketing set %s", valstr);
        mParameters.set(TICameraParameters::KEY_EXP_BRACKETING_RANGE, valstr);
        }
    else
        {
        mParameters.remove(TICameraParameters::KEY_EXP_BRACKETING_RANGE);
        }

    if( ( (valstr = params.get(CameraParameters::KEY_ZOOM)) != NULL )
        && (params.getInt(CameraParameters::KEY_ZOOM) >= 0 )
        && (params.getInt(CameraParameters::KEY_ZOOM) <= mMaxZoomSupported ) )
        {
        CAMHAL_LOGDB("Zoom set %s", valstr);
        mParameters.set(CameraParameters::KEY_ZOOM, valstr);
        }
    else
        {
        //CTS requirement: Invalid zoom values should always return an error.
        ret = -EINVAL;
        }

    CameraParameters adapterParams = mParameters;

    //If the app has not set the capture mode, set the capture resolution as preview resolution
    //so that black bars are not displayed in preview.
    //Later in takePicture we will configure the correct picture size
    if(params.get(TICameraParameters::KEY_CAP_MODE) == NULL)
        {
        CAMHAL_LOGDA("Capture mode not set by app, setting picture res to preview res");
        mParameters.getPreviewSize(&w, &h);
        adapterParams.setPictureSize(w, h);
        }

    if ( NULL != mCameraAdapter )
        {
        ret |= mCameraAdapter->setParameters(adapterParams);
        }

    if( NULL != params.get(TICameraParameters::KEY_TEMP_BRACKETING_RANGE_POS) )
        {
        int posBracketRange = params.getInt(TICameraParameters::KEY_TEMP_BRACKETING_RANGE_POS);
        if ( 0 < posBracketRange )
            {
            mBracketRangePositive = posBracketRange;
            }
        }
    CAMHAL_LOGEB("Positive bracketing range %d", mBracketRangePositive);


    if( NULL != params.get(TICameraParameters::KEY_TEMP_BRACKETING_RANGE_NEG) )
        {
        int negBracketRange = params.getInt(TICameraParameters::KEY_TEMP_BRACKETING_RANGE_NEG);
        if ( 0 < negBracketRange )
            {
            mBracketRangeNegative = negBracketRange;
            }
        }
    CAMHAL_LOGEB("Negative bracketing range %d", mBracketRangeNegative);

    if( ( (valstr = params.get(TICameraParameters::KEY_TEMP_BRACKETING)) != NULL) &&
        ( strcmp(valstr, TICameraParameters::BRACKET_ENABLE) == 0 ))
        {
        if ( !mBracketingEnabled )
            {
            CAMHAL_LOGDA("Enabling bracketing");
            mBracketingEnabled = true;

            //Wait for AF events to enable bracketing
            if ( NULL != mCameraAdapter )
                {
                setEventProvider( CameraHalEvent::ALL_EVENTS, mCameraAdapter );
                }
            }
        else
            {
            CAMHAL_LOGDA("Bracketing already enabled");
            }
        }
    else if ( ( (valstr = params.get(TICameraParameters::KEY_TEMP_BRACKETING)) != NULL ) &&
        ( strcmp(valstr, TICameraParameters::BRACKET_DISABLE) == 0 ))
        {
        CAMHAL_LOGDA("Disabling bracketing");

        mBracketingEnabled = false;
        stopImageBracketing();

        //Remove AF events subscription
        if ( NULL != mEventProvider )
            {
            mEventProvider->disableEventNotification( CameraHalEvent::ALL_EVENTS );
            delete mEventProvider;
            mEventProvider = NULL;
            }

        }

    if( ( (valstr = params.get(TICameraParameters::KEY_SHUTTER_ENABLE)) != NULL ) &&
        ( strcmp(valstr, TICameraParameters::SHUTTER_ENABLE) == 0 ))
        {
        CAMHAL_LOGDA("Enabling shutter sound");

        mShutterEnabled = true;
        mMsgEnabled |= CAMERA_MSG_SHUTTER;
        mParameters.set(TICameraParameters::KEY_SHUTTER_ENABLE, valstr);
        }
    else if ( ( (valstr = params.get(TICameraParameters::KEY_SHUTTER_ENABLE)) != NULL ) &&
        ( strcmp(valstr, TICameraParameters::SHUTTER_DISABLE) == 0 ))
        {
        CAMHAL_LOGDA("Disabling shutter sound");

        mShutterEnabled = false;
        mMsgEnabled &= ~CAMERA_MSG_SHUTTER;
        mParameters.set(TICameraParameters::KEY_SHUTTER_ENABLE, valstr);
        }


    CAMHAL_LOGDB("mReloadAdapter %d", (int) mReloadAdapter);

    LOG_FUNCTION_NAME_EXIT

    return ret;

    }

status_t CameraHal::allocPreviewBufs(int width, int height, const char* previewFormat)
{
    status_t ret;
    BufferProvider* newBufProvider;

    LOG_FUNCTION_NAME

    /**
      * Logic for allocation of preview buffers : If an overlay object is available, we allocate the preview buffers using
      * that. If not, then we use Mem Manager to allocate the buffers
      */

    ///If overlay is initialized, allocate buffer using overlay
    if(mDisplayAdapter.get() != NULL)
        {
        CAMHAL_LOGDA("DisplayAdapter present. Choosing DisplayAdapter as buffer allocator");
        newBufProvider = (BufferProvider*) mDisplayAdapter.get();
        mPreviewBufsAllocatedUsingOverlay = true;
        }
    else
        {
        CAMHAL_LOGDA("DisplayAdapter not present. Choosing MemoryManager as buffer allocator");
        newBufProvider = (BufferProvider*) mMemoryManager.get();
        mPreviewBufsAllocatedUsingOverlay = false;
        }

    if(!mPreviewBufs)
        {

        ///@todo Pluralise the name of this method to allocateBuffers
        mPreviewLength = 0;
        mPreviewBufs = (int32_t *) newBufProvider->allocateBuffer(width, height, previewFormat, mPreviewLength, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));
        mPreviewOffsets = (uint32_t *) newBufProvider->getOffsets();
        mPreviewFd = newBufProvider->getFd();
        mBufProvider = newBufProvider;

        }
    else
        {
            ///If the existing buffer provider is not DisplayAdapter but the new one is, free the buffers allocated using previous buffer provider
            ///and allocate buffers using the DisplayAdapter (which in turn will query the buffers from Overlay object and just pass the pointers)
            ///@todo currently having a static value for number of buffers, this may need to be made dynamic/flexible  b/w preview and video modes
            if((mBufProvider!=(BufferProvider*)mDisplayAdapter.get()) && (newBufProvider==(BufferProvider*)mDisplayAdapter.get()))
                {

                freePreviewBufs();

                mPreviewLength = 0;
                mPreviewBufs = (int32_t *)newBufProvider->allocateBuffer(width, height, previewFormat, mPreviewLength, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));
                if ( NULL == mPreviewBufs )
                    {
                    CAMHAL_LOGEA("Couldn't allocate preview buffers using Memory manager");
                    LOG_FUNCTION_NAME_EXIT
                    return NO_MEMORY;
                    }

                mPreviewOffsets = (uint32_t *) newBufProvider->getOffsets();
                mPreviewFd = newBufProvider->getFd();
                mBufProvider = newBufProvider;

                }
        }

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;

}

status_t CameraHal::freePreviewBufs()
{
    status_t ret = NO_ERROR;
    LOG_FUNCTION_NAME

    CAMHAL_LOGDB("mPreviewBufs = 0x%x", (unsigned int)mPreviewBufs);
    if(mPreviewBufs)
        {
        ///@todo Pluralise the name of this method to freeBuffers
        ret = mBufProvider->freeBuffer(mPreviewBufs);
        mPreviewBufs = NULL;
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }
    LOG_FUNCTION_NAME_EXIT;
    return ret;
}


status_t CameraHal::allocPreviewDataBufs(size_t size, size_t bufferCount)
{
    status_t ret = NO_ERROR;
    int bytes;

    LOG_FUNCTION_NAME

    bytes = size;

    if ( NO_ERROR == ret )
        {
        if( NULL != mPreviewDataBufs )
            {
            ret = freePreviewDataBufs();
            }
        }

    if ( NO_ERROR == ret )
        {
        mPreviewDataBufs = (int32_t *)mMemoryManager->allocateBuffer(0, 0, NULL, bytes, bufferCount);

        CAMHAL_LOGEB("Size of Preview data buffer = %d", bytes);
        if( NULL == mPreviewDataBufs )
            {
            CAMHAL_LOGEA("Couldn't allocate image buffers using memory manager");
            ret = -NO_MEMORY;
            }
        else
            {
            bytes = size;
            }
        }

    if ( NO_ERROR == ret )
        {
        mPreviewDataFd = mMemoryManager->getFd();
        mPreviewDataLength = bytes;
        mPreviewDataOffsets = mMemoryManager->getOffsets();
        }
    else
        {
        mPreviewDataFd = -1;
        mPreviewDataLength = 0;
        mPreviewDataOffsets = NULL;
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t CameraHal::freePreviewDataBufs()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {

        if( NULL != mPreviewDataBufs )
            {

            ///@todo Pluralise the name of this method to freeBuffers
            ret = mMemoryManager->freeBuffer(mPreviewDataBufs);
            mPreviewDataBufs = NULL;

            }
        else
            {
            ret = -EINVAL;
            }

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t CameraHal::allocImageBufs(unsigned int width, unsigned int height, size_t size, const char* previewFormat, unsigned int bufferCount)
{
    status_t ret = NO_ERROR;
    int bytes;

    LOG_FUNCTION_NAME

    bytes = size;

    ///@todo Enhance this method allocImageBufs() to take in a flag for burst capture
    ///Always allocate the buffers for image capture using MemoryManager
    if ( NO_ERROR == ret )
        {
        if( ( NULL != mImageBufs ) )
            {
            ret = freeImageBufs();
            }
        }

    if ( NO_ERROR == ret )
        {
        mImageBufs = (int32_t *)mMemoryManager->allocateBuffer(0, 0, previewFormat, bytes, bufferCount);

        CAMHAL_LOGEB("Size of Image cap buffer = %d", bytes);
        if( NULL == mImageBufs )
            {
            CAMHAL_LOGEA("Couldn't allocate image buffers using memory manager");
            ret = -NO_MEMORY;
            }
        else
            {
            bytes = size;
            }
        }

    if ( NO_ERROR == ret )
        {
        mImageFd = mMemoryManager->getFd();
        mImageLength = bytes;
        mImageOffsets = mMemoryManager->getOffsets();
        }
    else
        {
        mImageFd = -1;
        mImageLength = 0;
        mImageOffsets = NULL;
        }

    LOG_FUNCTION_NAME

    return ret;
}

void endImageCapture( void *userData)
{
    LOG_FUNCTION_NAME

    if ( NULL != userData )
        {
        CameraHal *c = reinterpret_cast<CameraHal *>(userData);
        c->signalEndImageCapture();
        }

    LOG_FUNCTION_NAME_EXIT
}

void releaseImageBuffers(void *userData)
{
    LOG_FUNCTION_NAME

    if ( NULL != userData )
        {
        CameraHal *c = reinterpret_cast<CameraHal *>(userData);
        c->freeImageBufs();
        }

    LOG_FUNCTION_NAME_EXIT
}

status_t CameraHal::signalEndImageCapture()
{
    status_t ret = NO_ERROR;
    int w,h;
    bool restartImageCapture = false;
    CameraParameters adapterParams = mParameters;

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        Mutex::Autolock lock(mLock);

        mImageCaptureRunning = false;

        if ( 0 < mTakePictureQueue )
            {
            mTakePictureQueue--;
            restartImageCapture = true;
            }

        }

    if ( restartImageCapture )
        {
        ret = takePicture();
        }
    else
        {

        //If the app has not set the capture mode, restore the capture resolution
        //back to the preview resolution to get rid of the black bars issue
        if(mParameters.get(TICameraParameters::KEY_CAP_MODE) == NULL)
            {
            CAMHAL_LOGDA("Capture mode not set by app, setting picture res back to preview res");
            mParameters.getPreviewSize(&w, &h);
            adapterParams.setPictureSize(w, h);
            ret = mCameraAdapter->setParameters(adapterParams);
            }

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t CameraHal::freeImageBufs()
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {

        if( NULL != mImageBufs )
            {

            ///@todo Pluralise the name of this method to freeBuffers
            ret = mMemoryManager->freeBuffer(mImageBufs);
            mImageBufs = NULL;

            }
        else
            {
            ret = -EINVAL;
            }

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}



/**
   @brief Start preview mode.

   @param none
   @return NO_ERROR Camera switched to VF mode
   @todo Update function header with the different errors that are possible

 */
status_t CameraHal::startPreview()
{

    status_t ret = NO_ERROR;
    int w, h;
    size_t frameDataSize;
    const char *valstr = NULL;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

        gettimeofday(&mStartPreview, NULL);

#endif


    LOG_FUNCTION_NAME

    if( (mDisplayAdapter.get() != NULL) && ( !mPreviewEnabled ) && ( mDisplayPaused ) )
        {
        CAMHAL_LOGDA("Preview is in paused state");

        mDisplayPaused = false;
        mPreviewEnabled = true;
        if ( NO_ERROR == ret )
            {
            ret = mDisplayAdapter->pauseDisplay(mDisplayPaused);

            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("Display adapter resume failed %x", ret);
                }
            }
        //restart preview callbacks
        if(mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME)
        {
            mAppCallbackNotifier->enableMsgType (CAMERA_MSG_PREVIEW_FRAME);
        }
        return ret;

        }
    else if ( mPreviewEnabled )
        {
        CAMHAL_LOGEA("Preview already running");

        LOG_FUNCTION_NAME_EXIT

        return ALREADY_EXISTS;
        }

    ///If we don't have the preview callback enabled and display adapter,
    ///@todo we dont support changing the overlay dynamically when preview is ongoing nor set the overlay object dynamically
    ///@remarks We support preview without overlay. setPreviewDisplay() should be passed with NULL overlay for this path
    ///                 to work
    if(//!(mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) &&
         (!mSetOverlayCalled))
        {
        CAMHAL_LOGEA("Preview not started. Preview in progress flag set");
        mPreviewStartInProgress = true;
        return NO_ERROR;
        }

    /// Ensure that buffers for preview are allocated before we start the camera
    ///Get the updated size from Camera Adapter, to account for padding etc
    mCameraAdapter->getFrameSize(w, h);

    ///Update the current preview width and height
    mPreviewWidth = w;
    mPreviewHeight = h;

    //Update the padded width and height - required for VNF and VSTAB
    mParameters.set(TICameraParameters::KEY_PADDED_WIDTH, mPreviewWidth);
    mParameters.set(TICameraParameters::KEY_PADDED_HEIGHT, mPreviewHeight);

    ///Allocate the preview buffers
    ret = allocPreviewBufs(w, h, mParameters.getPreviewFormat());
    if ( NO_ERROR != ret )
        {
        CAMHAL_LOGEA("Couldn't allocate buffers for Preview");
        goto error;
        }

    if ( mMeasurementEnabled )
        {
        ret = mCameraAdapter->getFrameDataSize(frameDataSize, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));
        if ( NO_ERROR == ret )
            {
            ///Allocate the preview data buffers
            ret = allocPreviewDataBufs(frameDataSize, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));
            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEA("Couldn't allocate preview data buffers");
                goto error;
                }
            else
                {
                mCameraAdapter->useBuffers(CameraAdapter::CAMERA_MEASUREMENT, mPreviewDataBufs, mPreviewDataOffsets, mPreviewDataFd, mPreviewDataLength, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));
                }
            }
        }

    ///Pass the buffers to Camera Adapter
    mCameraAdapter->useBuffers(CameraAdapter::CAMERA_PREVIEW, mPreviewBufs, mPreviewOffsets, mPreviewFd, mPreviewLength, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));

    mAppCallbackNotifier->startPreviewCallbacks(mParameters, mPreviewBufs, mPreviewOffsets, mPreviewFd, mPreviewLength, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));

    ///Start the callback notifier
    ret = mAppCallbackNotifier->start();

    if( ALREADY_EXISTS == ret )
        {
        //Already running, do nothing
        CAMHAL_LOGDA("AppCallbackNotifier already running");
        ret = NO_ERROR;
        }
    else if ( NO_ERROR == ret )
        {
        CAMHAL_LOGDA("Started AppCallbackNotifier..");
        mAppCallbackNotifier->setMeasurements(mMeasurementEnabled);
        }
    else
        {
        CAMHAL_LOGEA("Couldn't start AppCallbackNotifier");
        goto error;
        }

    ///Enable the display adapter if present, actual overlay enable happens when we post the buffer
    if (mDisplayAdapter.get() != NULL)
        {
        bool isS3d = false;
        DisplayAdapter::S3DParameters s3dParams;

        s3dParams.mode = OVERLAY_S3D_MODE_OFF;
        s3dParams.framePacking = OVERLAY_S3D_FORMAT_NONE;
        s3dParams.order = OVERLAY_S3D_ORDER_LF;
        s3dParams.subSampling = OVERLAY_S3D_SS_NONE;

        CAMHAL_LOGDA("Enabling display");
        if ((valstr = mParameters.get(TICameraParameters::KEY_S3D_SUPPORTED)) != NULL)
            {
            isS3d = (!strcmp(valstr, "true"));
            }

        if (isS3d)
            {
            if ((valstr = mParameters.get(TICameraParameters::KEY_S3D2D_PREVIEW)) != NULL)
                {
                if (!strcmp(valstr, "on"))
                    {
                    CAMHAL_LOGEA("Stereo 3D: 2D Preview Mode is On");
                    }
                else
                    {
                    CAMHAL_LOGEA("Stereo 3D: 2D Preview Mode is Off");
                    s3dParams.mode = OVERLAY_S3D_MODE_ON;

                    if ((valstr = mParameters.get(TICameraParameters::KEY_S3D_FRAME_LAYOUT)) != NULL)
                        {
                        if (!strcmp(valstr, TICameraParameters::S3D_TB_FULL))
                            {
                            CAMHAL_LOGEA("Stereo 3D: Frame layout: Top-Bottom Full");
                            s3dParams.framePacking = OVERLAY_S3D_FORMAT_OVERUNDER;
                            s3dParams.subSampling = OVERLAY_S3D_SS_NONE;
                            s3dParams.order = OVERLAY_S3D_ORDER_LF;
                            }
                        else if (!strcmp(valstr, TICameraParameters::S3D_SS_FULL))
                            {
                            CAMHAL_LOGEA("Stereo 3D: Frame layout: Side-by-Side Full");
                            s3dParams.framePacking = OVERLAY_S3D_FORMAT_SIDEBYSIDE;
                            s3dParams.subSampling = OVERLAY_S3D_SS_NONE;
                            s3dParams.order = OVERLAY_S3D_ORDER_LF;
                            }
                        else if (!strcmp(valstr, TICameraParameters::S3D_TB_SUBSAMPLED))
                            {
                            CAMHAL_LOGEA("Stereo 3D: Frame layout: Top-Bottom SubSampled");
                            s3dParams.framePacking = OVERLAY_S3D_FORMAT_OVERUNDER;
                            s3dParams.subSampling = OVERLAY_S3D_SS_VERT;
                            s3dParams.order = OVERLAY_S3D_ORDER_LF;
                            }
                        else if (!strcmp(valstr, TICameraParameters::S3D_SS_SUBSAMPLED))
                            {
                            CAMHAL_LOGEA("Stereo 3D: Frame layout: Side-by-Side SubSampled");
                            s3dParams.framePacking = OVERLAY_S3D_FORMAT_SIDEBYSIDE;
                            s3dParams.subSampling = OVERLAY_S3D_SS_HOR;
                            s3dParams.order = OVERLAY_S3D_ORDER_LF;
                            }
                        }
                    }
                }
            }

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

        ret = mDisplayAdapter->enableDisplay(&mStartPreview, isS3d ? &s3dParams : NULL);

#else

        ret = mDisplayAdapter->enableDisplay(NULL, isS3d ? &s3dParams : NULL);

#endif

        if ( ret != NO_ERROR )
            {
            CAMHAL_LOGEA("Couldn't enable display");
            goto error;
            }

        }

    ///Send START_PREVIEW command to adapter
    CAMHAL_LOGDA("Starting CameraAdapter preview mode");

    ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_START_PREVIEW);

    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEA("Couldn't start preview w/ CameraAdapter");
        goto error;
        }
    CAMHAL_LOGDA("Started preview");


    mPreviewEnabled = true;
    mPreviewStartInProgress = false;
    return ret;

    error:

        CAMHAL_LOGEA("Performing cleanup after error");

        //Do all the cleanup
        freePreviewBufs();
        mCameraAdapter->sendCommand(CameraAdapter::CAMERA_STOP_PREVIEW);
        if(mDisplayAdapter.get() != NULL)
            {
            mDisplayAdapter->disableDisplay();
            }
        mAppCallbackNotifier->stop();
        mPreviewStartInProgress = false;
        mPreviewEnabled = false;
        LOG_FUNCTION_NAME_EXIT

        return ret;
}

/**
   @brief Configures an overlay object to use for direct rendering

   A NULL overlay object will disable direct rendering. In such cases, the application / camera service
   will take care of rendering to the screen.

   @param[in] overlay The overlay object created by Surface flinger, and talks to V4L2 driver for render
   @return NO_ERROR If the overlay object passes validation criteria
   @todo Define validation criteria for overlay object. Define error codes for scenarios

 */
status_t CameraHal::setOverlay(const sp<Overlay> &overlay)
{
    status_t ret = NO_ERROR;
    LOG_FUNCTION_NAME

    mSetOverlayCalled = true;


   ///If the Camera service passes a null overlay, we destroy existing overlay and free the DisplayAdapter
    if(!overlay.get())
        {
        if(mDisplayAdapter.get() != NULL)
            {
            // this case with preview paused and setOverlay(NULL) will only
            // occur when preview is being restarted and overlay dimensions
            // have changed. since preview dimensions have changed preview
            // needs to be stopped for restart
            if(mDisplayPaused)
            {
                stopPreview();
            }

            ///NULL overlay passed, destroy the display adapter if present
            CAMHAL_LOGEA("NULL Overlay passed to setOverlay, destroying display adapter");
            mDisplayAdapter.clear();
            ///@remarks If there was an overlay previously existing, we usually expect another valid overlay to be passed by the client
            ///@remarks so, we will wait until it passes a valid overlay to begin the preview again
            mSetOverlayCalled = false;
            }
        CAMHAL_LOGEA("NULL Overlay passed to setOverlay");
        // Setting the buffer provider to memory manager
        mBufProvider = (BufferProvider*) mMemoryManager.get();
        return ret;
        }

    ///Destroy existing display adapter, if present
    mDisplayAdapter.clear();


    ///Need to create the display adapter since it has not been created
    /// Create display adapter
    mDisplayAdapter = new OverlayDisplayAdapter();
    ret = NO_ERROR;
    if(!mDisplayAdapter.get() || ((ret=mDisplayAdapter->initialize())!=NO_ERROR))
        {
        if(ret!=NO_ERROR)
            {
            mDisplayAdapter.clear();
            CAMHAL_LOGEA("DisplayAdapter initialize failed")
            LOG_FUNCTION_NAME_EXIT
            return ret;
            }
        else
            {
            CAMHAL_LOGEA("Couldn't create DisplayAdapter");
            LOG_FUNCTION_NAME_EXIT
            return NO_MEMORY;
            }
        }

    ///DisplayAdapter needs to know where to get the CameraFrames from inorder to display
    ///Since CameraAdapter is the one that provides the frames, set it as the frame provider for DisplayAdapter
    mDisplayAdapter->setFrameProvider(mCameraAdapter);

    ///Any dynamic errors that happen during the camera use case has to be propagated back to the application
    ///via CAMERA_MSG_ERROR. AppCallbackNotifier is the class that  notifies such errors to the application
    ///Set it as the error handler for the DisplayAdapter
    mDisplayAdapter->setErrorHandler(mAppCallbackNotifier.get());


    ///Update the display adapter with the new overlay that is passed from CameraService
    ret  = mDisplayAdapter->setOverlay(overlay);
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEB("DisplayAdapter setOverlay returned error %d", ret);
        }


    ///If setOverlay was called when preview is already running, then ask CameraAdapter to flush the buffers
    ///and then use the new set of buffers from Overlay
    ///Note that this doesn't require camera to be restarted. But if it is a V4L2 type of interface, it might have to
    ///stop and re-start the camera
    ///@todo In the future, need to extend overlay to use buffers allocated in CameraHal so that we do not
    ///       have to
    if(mPreviewEnabled)
        {
        CAMHAL_LOGDA("setOverlay called when preview running - Handling buffers for overlay");
        if(!mDisplayAdapter->supportsExternalBuffering())
            {
            ///CameraAdapter is expected to relinquish all buffers when this call is made
            mCameraAdapter->flushBuffers();

            int w, h;
            ///Allocate preview buffers using overlay
            mParameters.getPreviewSize(&w, &h);

            ///Allocate buffers
            ret = allocPreviewBufs(w, h, mParameters.getPreviewFormat());
            if(ret!=NO_ERROR)
                {
                CAMHAL_LOGEA("Couldn't allocate buffers for Preview using overlay");
                return ret;
                }

            ///Pass the buffers to Camera Adapter
            mCameraAdapter->useBuffers(CameraAdapter::CAMERA_PREVIEW, mPreviewBufs, mPreviewOffsets, mPreviewFd, mPreviewLength, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));

            }
        else
            {
            ///Overlay supports external buffering, let DisplayAdapter know about the Camera buffers
            mDisplayAdapter->useBuffers(mPreviewBufs, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));
            }

        ///Everything initialized, It's time to start the display of frames through overlay
        ret = mDisplayAdapter->enableDisplay();
        if(ret!=NO_ERROR)
            {
            CAMHAL_LOGEA("Couldn't enable display");
            return ret;
            }

        }
    else if(mPreviewStartInProgress)
        {
        ///Start the preview since the overlay is now available
        ret = startPreview();
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;

}


/**
   @brief Stop a previously started preview.

   @param none
   @return none

 */
void CameraHal::stopPreview()
{
    LOG_FUNCTION_NAME

    if(!previewEnabled() && !mDisplayPaused)
        {
        LOG_FUNCTION_NAME_EXIT
        return;
        }

    if(mDisplayAdapter.get() != NULL)
        {
        ///Stop the buffer display first
        mDisplayAdapter->disableDisplay();
        }

    // stop bracketing if it is running
    stopImageBracketing();

    // since prerequisite for capturing is for camera system
    // to be previewing...cancel all captures before stopping
    // preview
    if(mImageCaptureRunning){
        mCameraAdapter->sendCommand(CameraAdapter::CAMERA_STOP_IMAGE_CAPTURE);
        mImageCaptureRunning = false;
        mTakePictureQueue = 0;
    }

    if ( NULL != mCameraAdapter )
       {
        //Stop the source of frames
        mCameraAdapter->sendCommand(CameraAdapter::CAMERA_STOP_PREVIEW);
        }

    if(mAppCallbackNotifier.get() != NULL)
        {
        //Stop the callback sending
        mAppCallbackNotifier->stop();
        mAppCallbackNotifier->stopPreviewCallbacks();
        }

    freePreviewBufs();
    freePreviewDataBufs();

    mPreviewEnabled = false;
    mDisplayPaused = false;

    LOG_FUNCTION_NAME_EXIT
}

/**
   @brief Returns true if preview is enabled

   @param none
   @return true If preview is running currently
         false If preview has been stopped

 */
bool CameraHal::previewEnabled()
{
    LOG_FUNCTION_NAME

    return (mPreviewEnabled || mPreviewStartInProgress);
}

/**
   @brief Start record mode.

  When a record image is available a CAMERA_MSG_VIDEO_FRAME message is sent with
  the corresponding frame. Every record frame must be released by calling
  releaseRecordingFrame().

   @param none
   @return NO_ERROR If recording could be started without any issues
   @todo Update the header with possible error values in failure scenarios

 */
status_t CameraHal::startRecording( )
{
    int w, h;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME


#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            gettimeofday(&mStartPreview, NULL);

#endif

    if(!previewEnabled())
        {
        return NO_INIT;
        }

    if ( NO_ERROR == ret )
        {
         ret = mAppCallbackNotifier->initSharedVideoBuffers(mPreviewBufs, mPreviewOffsets, mPreviewFd, mPreviewLength, atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_REQUIRED_PREVIEW_BUFS]->mPropValue));
        }

    if ( NO_ERROR == ret )
        {
         ret = mAppCallbackNotifier->startRecording();
        }

    if ( NO_ERROR == ret )
        {
        ///Buffers for video capture (if different from preview) are expected to be allocated within CameraAdapter
         ret =  mCameraAdapter->sendCommand(CameraAdapter::CAMERA_START_VIDEO);
        }

    if ( NO_ERROR == ret )
        {
        mRecordingEnabled = true;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

/**
   @brief Stop a previously started recording.

   @param none
   @return none

 */
void CameraHal::stopRecording()
{
    LOG_FUNCTION_NAME

    if (!mRecordingEnabled )
        {
        return;
        }

    mAppCallbackNotifier->stopRecording();

    mCameraAdapter->sendCommand(CameraAdapter::CAMERA_STOP_VIDEO);

    mRecordingEnabled = false;

    LOG_FUNCTION_NAME_EXIT
}

/**
   @brief Returns true if recording is enabled.

   @param none
   @return true If recording is currently running
         false If recording has been stopped

 */
bool CameraHal::recordingEnabled()
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return mRecordingEnabled;
}

/**
   @brief Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.

   @param[in] mem MemoryBase pointer to the frame being released. Must be one of the buffers
               previously given by CameraHal
   @return none

 */
void CameraHal::releaseRecordingFrame(const sp<IMemory>& mem)
{
    LOG_FUNCTION_NAME

    //CAMHAL_LOGDB(" 0x%x", mem->pointer());

    if ( ( mRecordingEnabled ) && ( NULL != mem.get() ) )
        {
        mAppCallbackNotifier->releaseRecordingFrame(mem);
        }

    LOG_FUNCTION_NAME_EXIT

    return;
}

/**
   @brief Start auto focus

   This call asynchronous.
   The notification callback routine is called with CAMERA_MSG_FOCUS once when
   focusing is complete. autoFocus() will be called again if another auto focus is
   needed.

   @param none
   @return NO_ERROR
   @todo Define the error codes if the focus is not locked

 */
status_t CameraHal::autoFocus()
{
    status_t ret = NO_ERROR;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    gettimeofday(&mStartFocus, NULL);

#endif


    LOG_FUNCTION_NAME

    if ( NULL != mCameraAdapter )
        {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    //pass the autoFocus timestamp along with the command to camera adapter
    ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_PERFORM_AUTOFOCUS, ( int ) &mStartFocus);

#else

    ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_PERFORM_AUTOFOCUS);

#endif

        }
    else
        {
            ret = -1;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

/**
   @brief Cancels auto-focus function.

   If the auto-focus is still in progress, this function will cancel it.
   Whether the auto-focus is in progress or not, this function will return the
   focus position to the default. If the camera does not support auto-focus, this is a no-op.


   @param none
   @return NO_ERROR If the cancel succeeded
   @todo Define error codes if cancel didnt succeed

 */
status_t CameraHal::cancelAutoFocus()
{
    LOG_FUNCTION_NAME
    if( NULL != mCameraAdapter )
    {
        mCameraAdapter->sendCommand(CameraAdapter::CAMERA_CANCEL_AUTOFOCUS);
    }
    LOG_FUNCTION_NAME_EXIT
    return NO_ERROR;
}

void CameraHal::setEventProvider(int32_t eventMask, MessageNotifier * eventNotifier)
{

    LOG_FUNCTION_NAME

    if ( NULL != mEventProvider )
        {
        mEventProvider->disableEventNotification(CameraHalEvent::ALL_EVENTS);
        delete mEventProvider;
        mEventProvider = NULL;
        }

    mEventProvider = new EventProvider(eventNotifier, this, eventCallbackRelay);
    if ( NULL == mEventProvider )
        {
        CAMHAL_LOGEA("Error in creating EventProvider");
        }
    else
        {
        mEventProvider->enableEventNotification(eventMask);
        }

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::eventCallbackRelay(CameraHalEvent* event)
{
    LOG_FUNCTION_NAME

    CameraHal *appcbn = ( CameraHal * ) (event->mCookie);
    appcbn->eventCallback(event );

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::eventCallback(CameraHalEvent* event)
{
    LOG_FUNCTION_NAME

    if ( NULL != event )
        {
        switch( event->mEventType )
            {
            case CameraHalEvent::EVENT_FOCUS_LOCKED:
            case CameraHalEvent::EVENT_FOCUS_ERROR:
                {
                if ( mBracketingEnabled )
                    {
                    startImageBracketing();
                    }
                break;
                }
            default:
                {
                break;
                }
            };
        }

    LOG_FUNCTION_NAME_EXIT
}

status_t CameraHal::startImageBracketing()
{
        status_t ret = NO_ERROR;
        int width, height;
        size_t pictureBufferLength;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

        gettimeofday(&mStartCapture, NULL);

#endif

        LOG_FUNCTION_NAME

        if(!previewEnabled() && !mDisplayPaused)
            {
            LOG_FUNCTION_NAME_EXIT
            return NO_INIT;
            }

        if ( !mBracketingEnabled )
            {
            return ret;
            }

        if ( NO_ERROR == ret )
            {
            mBracketingRunning = true;
            }

        if (  (NO_ERROR == ret) && ( NULL != mCameraAdapter ) )
            {
            ret = mCameraAdapter->getPictureBufferSize(pictureBufferLength, ( mBracketRangeNegative + 1 ));

            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("getPictureBufferSize returned error 0x%x", ret);
                }
            }

        if ( NO_ERROR == ret )
            {
            if ( NULL != mAppCallbackNotifier.get() )
                 {
                 mAppCallbackNotifier->setBurst(true);
                 }
            }

        if ( NO_ERROR == ret )
            {
            mParameters.getPictureSize(&width, &height);

            ret = allocImageBufs(width, height, pictureBufferLength, mParameters.getPictureFormat(), ( mBracketRangeNegative + 1 ));
            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("allocImageBufs returned error 0x%x", ret);
                }
            }

        if (  (NO_ERROR == ret) && ( NULL != mCameraAdapter ) )
            {

            ret = mCameraAdapter->useBuffers(CameraAdapter::CAMERA_IMAGE_CAPTURE, mImageBufs, mImageOffsets, mImageFd, mImageLength, ( mBracketRangeNegative + 1 ));

            if ( NO_ERROR == ret )
                {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

                 //pass capture timestamp along with the camera adapter command
                ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_START_BRACKET_CAPTURE,  ( mBracketRangePositive + 1 ),  (int) &mStartCapture);

#else

                ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_START_BRACKET_CAPTURE, ( mBracketRangePositive + 1 ));

#endif

                }
            }

        return ret;
}

status_t CameraHal::stopImageBracketing()
{
        status_t ret = NO_ERROR;

        LOG_FUNCTION_NAME

        if ( !mBracketingRunning )
            {
            return ret;
            }

        if ( NO_ERROR == ret )
            {
            mBracketingRunning = false;
            }

        if(!previewEnabled() && !mDisplayPaused)
            {
            return NO_INIT;
            }

        if ( NO_ERROR == ret )
            {
            ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_STOP_BRACKET_CAPTURE);
            }

        LOG_FUNCTION_NAME_EXIT

        return ret;
}

/**
   @brief Take a picture.

   @param none
   @return NO_ERROR If able to switch to image capture
   @todo Define error codes if unable to switch to image capture

 */
status_t CameraHal::takePicture( )
{
    status_t ret = NO_ERROR;
    int width, height;
    size_t pictureBufferLength;
    int burst;
    unsigned int bufferCount = 1;

    Mutex::Autolock lock(mLock);

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    gettimeofday(&mStartCapture, NULL);

#endif

    LOG_FUNCTION_NAME

    if(!previewEnabled() && !mDisplayPaused)
        {
        LOG_FUNCTION_NAME_EXIT
        return NO_INIT;
        }

    //If capture has already started, then queue this call for later execution
    if ( mImageCaptureRunning )
        {
        mTakePictureQueue++;
        return ret;
        }
    else
        {
        mImageCaptureRunning = true;
        }

    if ( !mBracketingRunning )
        {

         if ( NO_ERROR == ret )
            {
            burst = mParameters.getInt(TICameraParameters::KEY_BURST);
            }

         //Allocate all buffers only in burst capture case
         if ( burst > 1 )
             {
             bufferCount = CameraHal::NO_BUFFERS_IMAGE_CAPTURE;
             if ( NULL != mAppCallbackNotifier.get() )
                 {
                 mAppCallbackNotifier->setBurst(true);
                 }
             }
         else
             {
             if ( NULL != mAppCallbackNotifier.get() )
                 {
                 mAppCallbackNotifier->setBurst(false);
                 }
             }

        //Pause Preview during capture
        if ( (NO_ERROR == ret) && ( NULL != mDisplayAdapter.get() ) && ( burst < 1 ) )
            {
            mDisplayPaused = true;
            mPreviewEnabled = false;
            ret = mDisplayAdapter->pauseDisplay(mDisplayPaused);

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

            mDisplayAdapter->setSnapshotTimeRef(&mStartCapture);

#endif
            // since preview is paused we should stop sending preview frames too
            if(mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME)
                {
                    mAppCallbackNotifier->disableMsgType (CAMERA_MSG_PREVIEW_FRAME);
                }
            }

        if (  (NO_ERROR == ret) && ( NULL != mCameraAdapter ) )
            {

            //Configure the correct picture resolution now if the capture mode is not set
            if(mParameters.get(TICameraParameters::KEY_CAP_MODE) == NULL)
                {
                ret = mCameraAdapter->setParameters(mParameters);
                }

            if ( NO_ERROR == ret )
                {
                ret = mCameraAdapter->getPictureBufferSize(pictureBufferLength, bufferCount);
                }

            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("getPictureBufferSize returned error 0x%x", ret);
                }
            }

        if ( NO_ERROR == ret )
            {
            mParameters.getPictureSize(&width, &height);

            ret = allocImageBufs(width, height, pictureBufferLength, mParameters.getPictureFormat(), bufferCount);
            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("allocImageBufs returned error 0x%x", ret);
                }
            }

        if (  (NO_ERROR == ret) && ( NULL != mCameraAdapter ) )
            {
            ret = mCameraAdapter->useBuffers(CameraAdapter::CAMERA_IMAGE_CAPTURE, mImageBufs, mImageOffsets, mImageFd, mImageLength, bufferCount);
            }
        }
    else
        {
        mBracketingRunning = false;
        }

    if ( ( NO_ERROR == ret ) && ( NULL != mCameraAdapter ) )
        {

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

         //pass capture timestamp along with the camera adapter command
        ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_START_IMAGE_CAPTURE,  (int) &mStartCapture);

#else

        ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_START_IMAGE_CAPTURE);

#endif

        }

    return ret;
}

/**
   @brief Cancel a picture that was started with takePicture.

   Calling this method when no picture is being taken is a no-op.

   @param none
   @return NO_ERROR If cancel succeeded. Cancel can succeed if image callback is not sent
   @todo Define error codes

 */
status_t CameraHal::cancelPicture( )
{
    bool restartImageCapture = false;
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if (mImageCaptureRunning)
    {
        Mutex::Autolock lock(mLock);

        mCameraAdapter->sendCommand(CameraAdapter::CAMERA_STOP_IMAGE_CAPTURE);
        mImageCaptureRunning = false;

        if ( 0 < mTakePictureQueue )
        {
            mTakePictureQueue--;
            restartImageCapture = true;
        }

    }

    if ( restartImageCapture )
    {
        ret = takePicture();
    }

    return ret;
}

/**
   @brief Return the camera parameters.

   @param none
   @return Currently configured camera parameters

 */
CameraParameters CameraHal::getParameters() const
{
    CameraParameters params;

     LOG_FUNCTION_NAME

     params = mParameters;
     if( NULL != mCameraAdapter )
        {
        mCameraAdapter->getParameters(params);
        }

     LOG_FUNCTION_NAME_EXIT

     ///Return the current set of parameters

     return params;
}

/**
   @brief Send command to camera driver.

   @param none
   @return NO_ERROR If the command succeeds
   @todo Define the error codes that this function can return

 */
status_t CameraHal::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME


    if ( ( NO_ERROR == ret ) && ( NULL == mCameraAdapter ) )
        {
        CAMHAL_LOGEA("No CameraAdapter instance");
        ret = -EINVAL;
        }

    if ( ( NO_ERROR == ret ) && ( !previewEnabled() ))
        {
        CAMHAL_LOGEA("Preview is not running");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        switch(cmd)
            {
            case CAMERA_CMD_START_SMOOTH_ZOOM:

                ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_START_SMOOTH_ZOOM, arg1);

                break;
            case CAMERA_CMD_STOP_SMOOTH_ZOOM:

                ret = mCameraAdapter->sendCommand(CameraAdapter::CAMERA_STOP_SMOOTH_ZOOM);

                break;
            default:
                break;
            };
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

/**
   @brief Release the hardware resources owned by this object.

   Note that this is *not* done in the destructor.

   @param none
   @return none

 */
void CameraHal::release()
{
    LOG_FUNCTION_NAME
    ///@todo Investigate on how release is used by CameraService. Vaguely remember that this is called
    ///just before CameraHal object destruction
    deinitialize();
    LOG_FUNCTION_NAME_EXIT
}


/**
   @brief Dump state of the camera hardware

   @param[in] fd    File descriptor
   @param[in] args  Arguments
   @return NO_ERROR Dump succeeded
   @todo  Error codes for dump fail

 */
status_t  CameraHal::dump(int fd, const Vector<String16>& args) const
{
    LOG_FUNCTION_NAME
    ///Implement this method when the h/w dump function is supported on Ducati side
    return NO_ERROR;
}

/*-------------Camera Hal Interface Method definitions ENDS here--------------------*/




/*-------------Camera Hal Internal Method definitions STARTS here--------------------*/

/**
   @brief Constructor of CameraHal

   Member variables are initialized here.  No allocations should be done here as we
   don't use c++ exceptions in the code.

 */
CameraHal::CameraHal(int cameraId)
{
    LOG_FUNCTION_NAME

    ///Initialize all the member variables to their defaults
    mPreviewBufsAllocatedUsingOverlay = false;
    mPreviewEnabled = false;
    mPreviewBufs = NULL;
    mImageBufs = NULL;
    mBufProvider = NULL;
    mPreviewStartInProgress = false;
    mVideoBufs = NULL;
    mVideoBufProvider = NULL;
    mRecordingEnabled = false;
    mDisplayPaused = false;
    mSetOverlayCalled = false;
    mMsgEnabled = 0;
    mAppCallbackNotifier = NULL;
    mMemoryManager = NULL;
    mCameraAdapter = NULL;
    mTakePictureQueue = 0;
    mImageCaptureRunning = false;
    mBracketingEnabled = false;
    mBracketingRunning = false;
    mEventProvider = NULL;
    mBracketRangePositive = 1;
    mBracketRangeNegative = 1;
    mMaxZoomSupported = 0;
    mShutterEnabled = true;
    mMeasurementEnabled = false;
    mPreviewDataBufs = NULL;
    mCameraAdapterHandle = NULL;
    mCameraPropertiesArr = NULL;
    mCurrentTime = 0;
    mFalsePreview = 0;
    mImageOffsets = NULL;
    mImageLength = 0;
    mImageFd = 0;
    mVideoOffsets = NULL;
    mVideoFd = 0;
    mVideoLength = 0;
    mPreviewDataOffsets = NULL;
    mPreviewDataFd = 0;
    mPreviewDataLength = 0;
    mPreviewFd = 0;
    mPreviewWidth = 0;
    mPreviewHeight = 0;
    mPreviewLength = 0;
    mPreviewOffsets = NULL;
    mPreviewRunning = 0;
    mPreviewStateOld = 0;
    mRecordingEnabled = 0;
    mRecordEnabled = 0;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    //Initialize the CameraHAL constructor timestamp, which is used in the
    // PPM() method as time reference if the user does not supply one.
    gettimeofday(&ppm_start, NULL);

#endif

    mCameraIndex = cameraId;

    mReloadAdapter = false;

    LOG_FUNCTION_NAME_EXIT
}

/**
   @brief Destructor of CameraHal

   This function simply calls deinitialize() to free up memory allocate during construct
   phase
 */
CameraHal::~CameraHal()
{
    LOG_FUNCTION_NAME

    bool deleteProperties = true;

    ///Call de-initialize here once more - it is the last chance for us to relinquish all the h/w and s/w resources
    deinitialize();

    singleton[mCameraIndex].clear();

    // check to see if we are the last open instance of camerahal
    // if so, then go ahead and free gCameraProperties
    for(int i=0; i < gCameraProperties->camerasSupported(); i++)
    {
        if(singleton[i] != 0) deleteProperties = false;
    }
    if(deleteProperties)
        gCameraProperties.clear();

    LOG_FUNCTION_NAME_EXIT
}

/**
   @brief Initialize the Camera HAL

   Creates CameraAdapter, AppCallbackNotifier, DisplayAdapter and MemoryManager

   @param None
   @return NO_ERROR - On success
         NO_MEMORY - On failure to allocate memory for any of the objects
   @remarks Camera Hal internal function

 */
status_t CameraHal::initialize()
{
    LOG_FUNCTION_NAME

    typedef CameraAdapter* (*CameraAdapterFactory)();
    CameraAdapterFactory f = NULL;

    int sensor_index = 0;

    mLastPreviewFramerate = 0;

    ///Initialize the event mask used for registering an event provider for AppCallbackNotifier
    ///Currently, registering all events as to be coming from CameraAdapter
    int32_t eventMask = CameraHalEvent::ALL_EVENTS;

    ///Get my camera properties
    mCameraPropertiesArr = ( CameraProperties::CameraProperty **) gCameraProperties->getProperties(mCameraIndex);

if(!mCameraPropertiesArr)
    {
    goto fail_loop;
    }

#ifdef DEBUG_LOG

    ///Dump the properties of this Camera
    dumpProperties(mCameraPropertiesArr);

#endif

    ///Check if a valid adapter DLL name is present for the first camera
    if ( NULL == mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME] )
        {
        CAMHAL_LOGEA("No Camera adapter DLL set for default camera");
        goto fail_loop;
        }

    if ( NULL != mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_SENSOR_INDEX] )
        {
        sensor_index = atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_SENSOR_INDEX]->mPropValue);
        }

    CAMHAL_LOGEB("Sensor index %d", sensor_index);


   /// Create the camera adapter
    /// @todo Incorporate dynamic loading based on cfg file. For now using a constant definition for the adapter dll name
    mCameraAdapterHandle = ::dlopen( ( const char * ) mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME]->mPropValue, RTLD_NOW);
    if( NULL  == mCameraAdapterHandle )
        {
        CAMHAL_LOGEB("Unable to open CameraAdapter Library %s for default camera", (const char*)mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME]->mPropValue);
        LOG_FUNCTION_NAME_EXIT
        goto fail_loop;
        }


    f = (CameraAdapterFactory) ::dlsym(mCameraAdapterHandle, "CameraAdapter_Factory");
    if(!f)
        {
        CAMHAL_LOGEB("%s does not export required factory method CameraAdapter_Factory"
            ,mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME]->mPropValue
            );
        goto fail_loop;
        }

    mCameraAdapter = f();
    if ( ( NULL == mCameraAdapter ) || (mCameraAdapter->initialize(sensor_index)!=NO_ERROR))
        {
        CAMHAL_LOGEA("Unable to create or initialize CameraAdapter");
        goto fail_loop;
        }
    mCameraAdapter->sendCommand(CameraAdapter::CAMERA_CANCEL_TIMEOUT);
    mCameraAdapter->registerImageReleaseCallback(releaseImageBuffers, (void *) this);
    mCameraAdapter->registerEndCaptureCallback(endImageCapture, (void *)this);

    if(!mAppCallbackNotifier.get())
        {
        /// Create the callback notifier
        mAppCallbackNotifier = new AppCallbackNotifier();
        if( ( NULL == mAppCallbackNotifier.get() ) || ( mAppCallbackNotifier->initialize() != NO_ERROR))
            {
            CAMHAL_LOGEA("Unable to create or initialize AppCallbackNotifier");
            goto fail_loop;
            }
        }

    if(!mMemoryManager.get())
        {
        /// Create Memory Manager
        mMemoryManager = new MemoryManager();
        if( ( NULL == mMemoryManager.get() ) || ( mMemoryManager->initialize() != NO_ERROR))
            {
            CAMHAL_LOGEA("Unable to create or initialize MemoryManager");
            goto fail_loop;
            }
        }

    ///@remarks Note that the display adapter is not created here because it is dependent on whether overlay
    ///is used by the application or not, for rendering. If the application uses Surface flinger, then it doesnt
    ///pass a surface so, we wont create the display adapter. In this case, we just provide the preview callbacks
    ///to the application for rendering the frames via Surface flinger


    ///Setup the class dependencies...

    ///AppCallbackNotifier has to know where to get the Camera frames and the events like auto focus lock etc from.
    ///CameraAdapter is the one which provides those events
    ///Set it as the frame and event providers for AppCallbackNotifier
    ///@remarks  setEventProvider API takes in a bit mask of events for registering a provider for the different events
    ///         That way, if events can come from DisplayAdapter in future, we will be able to add it as provider
    ///         for any event
    mAppCallbackNotifier->setEventProvider(eventMask, mCameraAdapter);
    mAppCallbackNotifier->setFrameProvider(mCameraAdapter);

    ///Any dynamic errors that happen during the camera use case has to be propagated back to the application
    ///via CAMERA_MSG_ERROR. AppCallbackNotifier is the class that  notifies such errors to the application
    ///Set it as the error handler for CameraAdapter
    mCameraAdapter->setErrorHandler(mAppCallbackNotifier.get());

    ///Start the callback notifier
    if(mAppCallbackNotifier->start() != NO_ERROR)
      {
        CAMHAL_LOGEA("Couldn't start AppCallbackNotifier");
        goto fail_loop;
      }

    CAMHAL_LOGDA("Started AppCallbackNotifier..");
    mAppCallbackNotifier->setMeasurements(mMeasurementEnabled);

    ///Initialize default parameters
    initDefaultParameters();


    if ( setParameters(mParameters) != NO_ERROR )
        {
        CAMHAL_LOGEA("Failed to set default parameters?!");
        }

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;

    fail_loop:

        ///Free up the resources because we failed somewhere up
        deinitialize();
        LOG_FUNCTION_NAME_EXIT

        return NO_MEMORY;

}

status_t CameraHal::reloadAdapter()
{

    typedef CameraAdapter* (*CameraAdapterFactory)();
    CameraAdapterFactory f = NULL;
    status_t ret = NO_ERROR;
    int sensor_index = 0;

    LOG_FUNCTION_NAME

    if ( mCameraIndex >= gCameraProperties->camerasSupported() )
        {
        CAMHAL_LOGEA("Camera Index exceeds number of supported cameras!");
        return -EINVAL;
        }

    mCameraPropertiesArr = ( CameraProperties::CameraProperty **) gCameraProperties->getProperties(mCameraIndex);

    if (!mCameraPropertiesArr)
        {
        CAMHAL_LOGEB("getProperties() returned a NULL property set for Camera index %d", mCameraIndex);
        return NO_INIT;
        }
#ifdef DEBUG_LOG

    ///Dump the properties of this Camera
    dumpProperties(mCameraPropertiesArr);

#endif

    ///Check if a valid adapter DLL name is present for the first camera
    if ( NULL == mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME] )
        {
        CAMHAL_LOGEA("No Camera adapter DLL set for default camera");
        return NO_INIT;
        }

    if ( NULL != mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_SENSOR_INDEX] )
        {
        sensor_index = atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_SENSOR_INDEX]->mPropValue);
        }

    /// Create the camera adapter
    mCameraAdapterHandle = ::dlopen( ( const char * ) mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME]->mPropValue, RTLD_NOW);
    if ( NULL == mCameraAdapterHandle )
        {
        CAMHAL_LOGEB("Unable to open CameraAdapter Library %s for default camera", (const char*)mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME]->mPropValue);
        return -1;
        }

    //Delete all existing instances of CameraHAL objects
    //deinitialize();

    f = (CameraAdapterFactory) ::dlsym(mCameraAdapterHandle, "CameraAdapter_Factory");
    if (!f)
        {
        ::dlclose(mCameraAdapterHandle);
        return -1;
        }

    mCameraAdapter = f();
    if (NULL == mCameraAdapter)
        {
        return -1;
        }

    ret = mCameraAdapter->setParameters(mParameters);
    if (NO_ERROR != ret)
        {
        return ret;
        }

    ret = mCameraAdapter->initialize(sensor_index);
    if (NO_ERROR != ret)
        {
        return ret;
        }

    mCameraAdapter->sendCommand(CameraAdapter::CAMERA_CANCEL_TIMEOUT);
    mCameraAdapter->registerImageReleaseCallback(releaseImageBuffers, (void *) this);
    mCameraAdapter->registerEndCaptureCallback(endImageCapture, (void *)this);
    initDefaultParameters();

    if ( NULL != mAppCallbackNotifier.get() )
        {
        mAppCallbackNotifier->setEventProvider(CameraHalEvent::ALL_EVENTS, mCameraAdapter);
        mAppCallbackNotifier->setFrameProvider(mCameraAdapter);
        mCameraAdapter->setErrorHandler(mAppCallbackNotifier.get());
        }

    if ( NULL != mDisplayAdapter.get() )
        {
        mDisplayAdapter->setFrameProvider(mCameraAdapter);
        }

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

void CameraHal::dumpProperties(CameraProperties::CameraProperty** cameraProps)
{
    if(!cameraProps)
        {
        CAMHAL_LOGEA("dumpProperties() passed a NULL pointer");
        return;
        }

    for ( int i = 0 ; i < CameraProperties::PROP_INDEX_MAX; i++)
        {
        CAMHAL_LOGDB("%s = %s", cameraProps[i]->mPropName, cameraProps[i]->mPropValue);
        }
}

bool CameraHal::isResolutionValid(unsigned int width, unsigned int height, const char *supportedResolutions)
{
    bool ret = true;
    status_t status = NO_ERROR;
    char tmpBuffer[PARAM_BUFFER + 1];
    char *pos = NULL;

    LOG_FUNCTION_NAME

    if ( NULL == supportedResolutions )
        {
        CAMHAL_LOGEA("Invalid supported resolutions string");
        ret = false;
        goto exit;
        }

    status = snprintf(tmpBuffer, PARAM_BUFFER, "%dx%d", width, height);
    if ( 0 > status )
        {
        CAMHAL_LOGEA("Error encountered while generating validation string");
        ret = false;
        goto exit;
        }

    pos = strstr(supportedResolutions, tmpBuffer);
    if ( NULL == pos )
        {
        ret = false;
        }
    else
        {
        ret = true;
        }

exit:

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

bool CameraHal::isParameterValid(const char *param, const char *supportedParams)
{
    bool ret = true;
    char *pos = NULL;

    LOG_FUNCTION_NAME

    if ( NULL == supportedParams )
        {
        CAMHAL_LOGEA("Invalid supported parameters string");
        ret = false;
        goto exit;
        }

    if ( NULL == param )
        {
        CAMHAL_LOGEA("Invalid parameter string");
        ret = false;
        goto exit;
        }

    pos = strstr(supportedParams, param);
    if ( NULL == pos )
        {
        ret = false;
        }
    else
        {
        ret = true;
        }

exit:

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

bool CameraHal::isParameterValid(int param, const char *supportedParams)
{
    bool ret = true;
    char *pos = NULL;
    status_t status;
    char tmpBuffer[PARAM_BUFFER + 1];

    LOG_FUNCTION_NAME

    if ( NULL == supportedParams )
        {
        CAMHAL_LOGEA("Invalid supported parameters string");
        ret = false;
        goto exit;
        }

    status = snprintf(tmpBuffer, PARAM_BUFFER, "%d", param);
    if ( 0 > status )
        {
        CAMHAL_LOGEA("Error encountered while generating validation string");
        ret = false;
        goto exit;
        }

    pos = strstr(supportedParams, tmpBuffer);
    if ( NULL == pos )
        {
        ret = false;
        }
    else
        {
        ret = true;
        }

exit:

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t CameraHal::parseResolution(const char *resStr, int &width, int &height)
{
    status_t ret = NO_ERROR;
    char *ctx, *pWidth, *pHeight;
    const char *sep = "x";
    char *tmp = NULL;

    LOG_FUNCTION_NAME

    if ( NULL == resStr )
        {
        return -EINVAL;
        }

    //This fixes "Invalid input resolution"
    char *resStr_copy = (char *)malloc(strlen(resStr) + 1);
    if ( NULL!=resStr_copy ) {
    if ( NO_ERROR == ret )
        {
        strcpy(resStr_copy, resStr);
        pWidth = strtok_r( (char *) resStr_copy, sep, &ctx);

        if ( NULL != pWidth )
            {
            width = atoi(pWidth);
            }
        else
            {
            CAMHAL_LOGEB("Invalid input resolution %s", resStr);
            ret = -EINVAL;
            }
        }

    if ( NO_ERROR == ret )
        {
        pHeight = strtok_r(NULL, sep, &ctx);

        if ( NULL != pHeight )
            {
            height = atoi(pHeight);
            }
        else
            {
            CAMHAL_LOGEB("Invalid input resolution %s", resStr);
            ret = -EINVAL;
            }
        }

        free(resStr_copy);
        resStr_copy = NULL;
     }
    LOG_FUNCTION_NAME_EXIT

    return ret;
}

void CameraHal::insertSupportedParams()
{
    CameraProperties::CameraProperty **currentProp;
    char tmpBuffer[PARAM_BUFFER + 1];

    LOG_FUNCTION_NAME

    CameraParameters &p = mParameters;

    ///Set the name of the camera
    p.set(TICameraParameters::KEY_CAMERA_NAME, mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_NAME]->mPropValue);

    mMaxZoomSupported = atoi(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ZOOM_STAGES]->mPropValue);

    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PICTURE_SIZES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PICTURE_FORMATS]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_SIZES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_FORMATS]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_FRAME_RATES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_THUMBNAIL_SIZES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_WHITE_BALANCE]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_EFFECTS, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EFFECTS]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_SCENE_MODES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_FLASH_MODES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_FOCUS_MODES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ANTIBANDING]->mPropValue);
    p.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EV_MAX]->mPropValue);
    p.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EV_MIN]->mPropValue);
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EV_STEP]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_SCENE_MODES]->mPropValue);
    p.set(TICameraParameters::KEY_SUPPORTED_EXPOSURE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EXPOSURE_MODES]->mPropValue);
    p.set(TICameraParameters::KEY_SUPPORTED_ISO_VALUES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ISO_VALUES]->mPropValue);
    p.set(CameraParameters::KEY_ZOOM_RATIOS, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ZOOM_RATIOS]->mPropValue);
    p.set(CameraParameters::KEY_MAX_ZOOM, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ZOOM_STAGES]->mPropValue);
    p.set(CameraParameters::KEY_ZOOM_SUPPORTED, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_ZOOM_SUPPORTED]->mPropValue);
    p.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SMOOTH_ZOOM_SUPPORTED]->mPropValue);
    p.set(TICameraParameters::KEY_SUPPORTED_IPP, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_IPP_MODES]->mPropValue);
    p.set(TICameraParameters::KEY_S3D_SUPPORTED,(const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_S3D_SUPPORTED]->mPropValue);
    p.set(TICameraParameters::KEY_S3D2D_PREVIEW_MODE,(const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_S3D2D_PREVIEW_MODES]->mPropValue);
    p.set(TICameraParameters::KEY_S3D_FRAME_LAYOUT_VALUES,(const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_S3D_FRAME_LAYOUT_VALUES]->mPropValue);
    p.set(TICameraParameters::KEY_AUTOCONVERGENCE_MODE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_AUTOCONVERGENCE_MODE]->mPropValue);
    p.set(TICameraParameters::KEY_MANUALCONVERGENCE_VALUES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_MANUALCONVERGENCE_VALUES]->mPropValue);
    p.set(TICameraParameters::KEY_VSTAB,(const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_VSTAB]->mPropValue);
    p.set(TICameraParameters::KEY_VSTAB_VALUES,(const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_VSTAB_VALUES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_FRAMERATE_RANGE_SUPPORTED]->mPropValue);
    p.set(TICameraParameters::KEY_SENSOR_ORIENTATION, (const char*)mCameraPropertiesArr[CameraProperties::PROP_INDEX_SENSOR_ORIENTATION]->mPropValue);
    p.set(TICameraParameters::KEY_SENSOR_ORIENTATION_VALUES, (const char*)mCameraPropertiesArr[CameraProperties::PROP_INDEX_SENSOR_ORIENTATION_VALUES]->mPropValue);

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::extractSupportedParams()
{
    const char *pStr = NULL;
    CameraParameters &p = mParameters;

    LOG_FUNCTION_NAME

    /*pStr = p.get(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PICTURE_SIZES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PICTURE_FORMATS]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }*/

    pStr = p.get(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_SIZES]->mPropValue,
                       pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_FORMATS]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_FRAME_RATES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_THUMBNAIL_SIZES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_WHITE_BALANCE]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_EFFECTS);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EFFECTS]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH -1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_SCENE_MODES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_SCENE_MODES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_FOCUS_MODES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_FOCUS_MODES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_ANTIBANDING);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ANTIBANDING]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_FLASH_MODES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_FLASH_MODES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EV_MAX]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EV_MIN]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EV_STEP]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_SCENE_MODES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_SCENE_MODES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(TICameraParameters::KEY_SUPPORTED_EXPOSURE);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EXPOSURE_MODES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(TICameraParameters::KEY_SUPPORTED_ISO_VALUES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ISO_VALUES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_ZOOM_RATIOS);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ZOOM_RATIOS]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_MAX_ZOOM);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ZOOM_STAGES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_ZOOM_SUPPORTED);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_ZOOM_SUPPORTED]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SMOOTH_ZOOM_SUPPORTED]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(TICameraParameters::KEY_SUPPORTED_IPP);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_IPP_MODES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(TICameraParameters::KEY_S3D_SUPPORTED);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_S3D_SUPPORTED]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(TICameraParameters::KEY_MANUALCONVERGENCE_VALUES);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_MANUALCONVERGENCE_VALUES]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    pStr = p.get(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE);
    if ( NULL != pStr )
        {
        strncpy(mCameraPropertiesArr[CameraProperties::PROP_INDEX_FRAMERATE_RANGE_SUPPORTED]->mPropValue,
                      pStr, MAX_PROP_VALUE_LENGTH - 1);
        }

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::initDefaultParameters()
{
    //Purpose of this function is to initialize the default current and supported parameters for the currently
    //selected camera.

    CameraParameters &p = mParameters;
    int currentRevision, adapterRevision;
    status_t ret = NO_ERROR;
    int width, height;

    LOG_FUNCTION_NAME


    if ( NULL != mCameraAdapter &&
            (NO_ERROR == mCameraAdapter->getCaps(mParameters)))
    {
        extractSupportedParams();
    }

    ret = parseResolution((const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_PREVIEW_SIZE]->mPropValue, width, height);

    if ( NO_ERROR == ret )
        {
        p.setPreviewSize(width, height);
        }
    else
        {
        p.setPreviewSize(MIN_WIDTH, MIN_HEIGHT);
        }

    ret = parseResolution((const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_PICTURE_SIZE]->mPropValue, width, height);

    if ( NO_ERROR == ret )
        {
        p.setPictureSize(width, height);
        }
    else
        {
        p.setPictureSize(PICTURE_WIDTH, PICTURE_HEIGHT);
        }

    ret = parseResolution((const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_JPEG_THUMBNAIL_SIZE]->mPropValue, width, height);

    if ( NO_ERROR == ret )
        {
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, width);
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, height);
        }
    else
        {
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, MIN_WIDTH);
        p.set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, MIN_HEIGHT);
        }

    insertSupportedParams();

    //Insert default values
    p.set(TICameraParameters::KEY_S3D2D_PREVIEW, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_S3D2D_PREVIEW]->mPropValue);
    p.set(TICameraParameters::KEY_S3D_FRAME_LAYOUT, (const char*)mCameraPropertiesArr[CameraProperties::PROP_INDEX_S3D_FRAME_LAYOUT]->mPropValue);
    p.setPreviewFrameRate(atoi((const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_PREVIEW_FRAME_RATE]->mPropValue));
    p.setPreviewFormat((const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_PREVIEW_FORMAT]->mPropValue);
    p.setPictureFormat((const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_PICTURE_FORMAT]->mPropValue);
    p.set(CameraParameters::KEY_JPEG_QUALITY, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_JPEG_QUALITY]->mPropValue);
    p.set(CameraParameters::KEY_WHITE_BALANCE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_WHITEBALANCE]->mPropValue);
    p.set(CameraParameters::KEY_EFFECT,  (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_EFFECT]->mPropValue);
    p.set(CameraParameters::KEY_ANTIBANDING, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_ANTIBANDING]->mPropValue);
    p.set(CameraParameters::KEY_FLASH_MODE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_FLASH_MODE]->mPropValue);
    p.set(CameraParameters::KEY_FOCUS_MODE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_FOCUS_MODE]->mPropValue);
    p.set(CameraParameters::KEY_EXPOSURE_COMPENSATION, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_EV_COMPENSATION]->mPropValue);
    p.set(CameraParameters::KEY_SCENE_MODE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SCENE_MODE]->mPropValue);
    p.set(CameraParameters::KEY_FLASH_MODE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_FLASH_MODE]->mPropValue);
    p.set(CameraParameters::KEY_ZOOM, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_ZOOM]->mPropValue);
    p.set(TICameraParameters::KEY_CONTRAST, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_CONTRAST]->mPropValue);
    p.set(TICameraParameters::KEY_SATURATION, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SATURATION]->mPropValue);
    p.set(TICameraParameters::KEY_BRIGHTNESS, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_BRIGHTNESS]->mPropValue);
    p.set(TICameraParameters::KEY_SHARPNESS, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SHARPNESS]->mPropValue);
    p.set(TICameraParameters::KEY_EXPOSURE_MODE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_EXPOSURE_MODE]->mPropValue);
    p.set(TICameraParameters::KEY_ISO, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_ISO_MODE]->mPropValue);
    p.set(TICameraParameters::KEY_IPP, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_IPP]->mPropValue);
    p.set(TICameraParameters::KEY_AUTOCONVERGENCE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_AUTOCONVERGENCE]->mPropValue);
    p.set(TICameraParameters::KEY_MANUALCONVERGENCE_VALUES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_MANUALCONVERGENCE_VALUES]->mPropValue);
    p.set(TICameraParameters::KEY_VSTAB,(const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_VSTAB]->mPropValue);
    p.set(TICameraParameters::KEY_VSTAB_VALUES,(const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_VSTAB_VALUES]->mPropValue);
    p.set(CameraParameters::KEY_FOCAL_LENGTH, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_FOCAL_LENGTH]->mPropValue);
    p.set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_HOR_ANGLE]->mPropValue);
    p.set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_VER_ANGLE]->mPropValue);
    p.set(CameraParameters::KEY_PREVIEW_FPS_RANGE,(const char*)mCameraPropertiesArr[CameraProperties::PROP_INDEX_FRAMERATE_RANGE]->mPropValue);
    p.set(TICameraParameters::KEY_SENSOR_ORIENTATION, (const char*)mCameraPropertiesArr[CameraProperties::PROP_INDEX_SENSOR_ORIENTATION]->mPropValue);
    p.set(TICameraParameters::KEY_SENSOR_ORIENTATION_VALUES, (const char*)mCameraPropertiesArr[CameraProperties::PROP_INDEX_SENSOR_ORIENTATION_VALUES]->mPropValue);
    p.set(TICameraParameters::KEY_EXIF_MAKE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_EXIF_MAKE]->mPropValue);
    p.set(TICameraParameters::KEY_EXIF_MODEL, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_EXIF_MODEL]->mPropValue);
    p.set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_JPEG_THUMBNAIL_QUALITY]->mPropValue);

    LOG_FUNCTION_NAME_EXIT
}

#if PPM_INSTRUMENTATION

/**
   @brief PPM instrumentation

   Dumps the current time offset. The time reference point
   lies within the CameraHAL constructor.

   @param str - log message
   @return none

 */
void CameraHal::PPM(const char* str){
    struct timeval ppm;

    gettimeofday(&ppm, NULL);
    ppm.tv_sec = ppm.tv_sec - ppm_start.tv_sec;
    ppm.tv_sec = ppm.tv_sec * 1000000;
    ppm.tv_sec = ppm.tv_sec + ppm.tv_usec - ppm_start.tv_usec;

    LOGD("PPM: %s :%ld.%ld ms", str, ( ppm.tv_sec /1000 ), ( ppm.tv_sec % 1000 ));
}

#elif PPM_INSTRUMENTATION_ABS

/**
   @brief PPM instrumentation

   Dumps the current time offset. The time reference point
   lies within the CameraHAL constructor. This implemetation
   will also dump the abosolute timestamp, which is useful when
   post calculation is done with data coming from the upper
   layers (Camera application etc.)

   @param str - log message
   @return none

 */
void CameraHal::PPM(const char* str){
    struct timeval ppm;

    unsigned long long elapsed, absolute;
    gettimeofday(&ppm, NULL);
    elapsed = ppm.tv_sec - ppm_start.tv_sec;
    elapsed *= 1000000;
    elapsed += ppm.tv_usec - ppm_start.tv_usec;
    absolute = ppm.tv_sec;
    absolute *= 1000;
    absolute += ppm.tv_usec /1000;

    LOGD("PPM: %s :%llu.%llu ms : %llu ms", str, ( elapsed /1000 ), ( elapsed % 1000 ), absolute);
}

#endif

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

/**
   @brief PPM instrumentation

   Calculates and dumps the elapsed time using 'ppm_first' as
   reference.

   @param str - log message
   @return none

 */
void CameraHal::PPM(const char* str, struct timeval* ppm_first, ...){
    char temp_str[256];
    struct timeval ppm;
    unsigned long long absolute;
    va_list args;

    va_start(args, ppm_first);
    vsprintf(temp_str, str, args);
    gettimeofday(&ppm, NULL);
    absolute = ppm.tv_sec;
    absolute *= 1000;
    absolute += ppm.tv_usec /1000;
    ppm.tv_sec = ppm.tv_sec - ppm_first->tv_sec;
    ppm.tv_sec = ppm.tv_sec * 1000000;
    ppm.tv_sec = ppm.tv_sec + ppm.tv_usec - ppm_first->tv_usec;

    LOGD("PPM: %s :%ld.%ld ms :  %llu ms", temp_str, ( ppm.tv_sec /1000 ), ( ppm.tv_sec % 1000 ), absolute);

    va_end(args);
}

#endif

/**
   @brief Deallocates memory for all the resources held by Camera HAL.

   Frees the following objects- CameraAdapter, AppCallbackNotifier, DisplayAdapter,
   and Memory Manager

   @param none
   @return none

 */
void CameraHal::deinitialize()
{
    LOG_FUNCTION_NAME

    stopPreview();

    freeImageBufs();

    /// Free the callback notifier
    mAppCallbackNotifier.clear();

    /// Free the memory manager
    mMemoryManager.clear();

    /// Free the display adapter
    mDisplayAdapter.clear();

    mSetOverlayCalled = false;

    if ( NULL != mEventProvider )
        {
        mEventProvider->disableEventNotification(CameraHalEvent::ALL_EVENTS);
        delete mEventProvider;
        mEventProvider = NULL;
        }

    if ( NULL != mCameraAdapter )
        {
        mCameraAdapter->sendCommand(CameraAdapter::CAMERA_SET_TIMEOUT, ADAPTER_TIMEOUT);
        mCameraAdapter = NULL;
        }

    ///We dont close the camera adapter DLL here inorder to improve performance
    //::dlclose(mCameraAdapterHandle);

    LOG_FUNCTION_NAME_EXIT

}

/*-------------Camera Hal Internal Method definitions ENDS here--------------------*/

/*--------------------Camera HAL Creation Related---------------------------------------*/

sp<CameraHardwareInterface> CameraHal::createInstance(int cameraId)
{
    LOG_FUNCTION_NAME

    sp<CameraHardwareInterface> hardware(NULL);

    // Create the CameraProperties class
    if(!gCameraProperties.get())
    {
        gCameraProperties = new CameraProperties();
        if(!gCameraProperties.get() || (gCameraProperties->initialize()!=NO_ERROR))
        {
            CAMHAL_LOGEA("Unable to create or initialize CameraProperties");
            return NULL;
        }
    }

    if (singleton[cameraId] != 0)
        {
        hardware = singleton[cameraId].promote();
        if (hardware != 0)
            {
            CAMHAL_LOGDA("Duplicate instance of Camera");
            ((CameraHal *)hardware.get())->initialize();
            return hardware;
            }
        }

    CameraHal *ptr = new CameraHal(cameraId);

    if(!ptr)
        {
        CAMHAL_LOGEA("Couldn't create instance of CameraHal class");
        return NULL;
        }

    int r = ptr->initialize();

    if(r!=NO_ERROR)
        {
        delete ptr;
        return NULL;
        }

    singleton[cameraId] = hardware = ptr;

    LOG_FUNCTION_NAME_EXIT
    return hardware;
}


extern "C" int HAL_getNumberOfCameras()
{
    int numCameras = 0;

    // Create the CameraProperties class
    if(!gCameraProperties.get())
    {
        gCameraProperties = new CameraProperties();
        if(!gCameraProperties.get() || (gCameraProperties->initialize()!=NO_ERROR))
        {
            CAMHAL_LOGEA("Unable to create or initialize CameraProperties");
            return 0;
        }
    }

    // Query the number of cameras supported, minimum we expect is one (at least FakeCameraAdapter)
    numCameras = gCameraProperties->camerasSupported();

    if ( 0 == numCameras )
    {
        CAMHAL_LOGEA("No cameras supported in Camera HAL implementation");
        return 0;
    }
    else
    {
        CAMHAL_LOGDB("Cameras found %d", numCameras);
    }


    return numCameras;
}
extern "C" void HAL_getCameraInfo(int cameraId, struct CameraInfo* cameraInfo)
{
    int face_value = CAMERA_FACING_BACK;
    int orientation = 0;
    char *valstr = NULL;
    CameraProperties::CameraProperty ** properties;

    // Create the CameraProperties class
    if(!gCameraProperties.get())
    {
        gCameraProperties = new CameraProperties();
        if(!gCameraProperties.get() || (gCameraProperties->initialize()!=NO_ERROR))
        {
            CAMHAL_LOGEA("Unable to create or initialize CameraProperties");
            return;
        }
    }

    //Get camera properties for camera index
    properties = ( CameraProperties::CameraProperty **) gCameraProperties->getProperties(cameraId);
    if(properties)
    {

        valstr = properties[CameraProperties::PROP_INDEX_FACING_INDEX]->mPropValue;
        if(valstr != NULL)
        {
            if (strcmp(valstr, (const char *) TICameraParameters::FACING_FRONT) == 0)
            {
                face_value = CAMERA_FACING_FRONT;
            }
            else if (strcmp(valstr, (const char *) TICameraParameters::FACING_BACK) == 0)
            {
                face_value = CAMERA_FACING_BACK;
            }
         }

         valstr = properties[CameraProperties::PROP_INDEX_ORIENTATION_INDEX]->mPropValue;
         if(valstr != NULL)
         {
             orientation = atoi(valstr);
         }
    }
    else
    {
        CAMHAL_LOGEB("getProperties() returned a NULL property set for Camera id %d", cameraId);
    }

    cameraInfo->facing = face_value;
    cameraInfo->orientation = orientation;
}

extern "C" sp<CameraHardwareInterface> HAL_openCameraHardware(int cameraId)
{
    LOG_FUNCTION_NAME
    CAMHAL_LOGDA("opening ti camera hal\n");
    return CameraHal::createInstance(cameraId);
}


};



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

#include <poll.h>
#include <math.h>

///@remarks added for testing purposes

namespace android {
/*****************************************************************************/

wp<CameraHardwareInterface> CameraHal::singleton;

////Constant definitions and declarations
////@todo Have a CameraProperties class to store these parameters as constants for every camera
////       Currently, they are hard-coded

const int CameraHal::NO_BUFFERS_PREVIEW = 6;
const int CameraHal::NO_BUFFERS_IMAGE_CAPTURE = 1;

const uint32_t MessageNotifier::EVENT_BIT_FIELD_POSITION = 16;
const uint32_t MessageNotifier::FRAME_BIT_FIELD_POSITION = 0;



/******************************************************************************/



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
    return 0;
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
    Mutex::Autolock lock(mLock);
    mNotifyCb = notify_cb;
    mDataCb = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mCallbackCookie = user;
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
    Mutex::Autolock lock(mLock);
    mMsgEnabled |= msgType;
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
    Mutex::Autolock lock(mLock);
    mMsgEnabled &= ~msgType;
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

    int w, h;
    int w_orig, h_orig;
    int framerate;
    int error;
    int base;
    const char *valstr;
    const char *prevFormat;
    char *af_coord;
    Message msg;
    status_t ret;

    Mutex::Autolock lock(mLock);

    CAMHAL_LOGDB("PreviewFormat %s", params.getPreviewFormat());

    if ( params.getPreviewFormat() != NULL )
        {
        if (strcmp(params.getPreviewFormat(), (const char *) CameraParameters::PIXEL_FORMAT_YUV422I) != 0)
            {
            CAMHAL_LOGEA("Only yuv422i preview is supported");
            LOG_FUNCTION_NAME_EXIT
            return -1;
            }
    }

    CAMHAL_LOGDB("PictureFormat %s", params.getPictureFormat());
    if ( params.getPictureFormat() != NULL )
        {
        if (strcmp(params.getPictureFormat(), (const char *) CameraParameters::PIXEL_FORMAT_JPEG) != 0)
            {
            CAMHAL_LOGEA("Only jpeg still pictures are supported");
            LOG_FUNCTION_NAME_EXIT
            return -1;
            }
        }

    params.getPreviewSize(&w, &h);
    if (!validateSize(w, h))
        {
        CAMHAL_LOGEA("Preview size not supported");
        LOG_FUNCTION_NAME_EXIT
        return -1;
        }

    CAMHAL_LOGDB("PreviewResolution by App %d x %d", w, h);

    mParameters.getPreviewSize(&w_orig, &h_orig);
    if((w!=w_orig) || (h_orig!=h))
        {
        ///Free the preview buffers if size is different from what is currently configured,buffers will be re-allocated
        ///again in startPreview. For overlay, anyway re-allocation doesn't happen, we just return from freePreviewBufs
        freePreviewBufs();
        }

    params.getPictureSize(&w, &h);
    if (!validateSize(w, h))
        {
        CAMHAL_LOGEA("Picture size not supported");
        LOG_FUNCTION_NAME_EXIT
        return -1;
        }
    CAMHAL_LOGDB("Picture Size by App %d x %d", w, h);

    framerate = params.getPreviewFrameRate();
    CAMHAL_LOGDB("FRAMERATE %d", framerate);

    ///If the current size of image capture buffers doesnt equal the new size, re-allocate
    ///Image Capture buffers will be allocated once during initialize() for default size.
    mParameters.getPictureSize(&w_orig, &h_orig);
    if((w!=w_orig) || (h_orig!=h))
        {
        freeImageBufs();
        ret = allocImageBufs(w, h, params.getPictureFormat());
        if(ret!=NO_ERROR)
            {
            freeImageBufs();
            LOG_FUNCTION_NAME_EXIT
            return ret;
            }
        }

    ///Update the current parameter set
    mParameters = params;

    ///Call Camera adapter for the rest of the configurations
    ret = mCameraAdapter->setParameters(params);

    LOG_FUNCTION_NAME_EXIT

    return ret;

    }

int CameraHal::validateSize(int w, int h)
{
    if ((w < MIN_WIDTH) || (h < MIN_HEIGHT))
        {
        return false;
        }

    return true;
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
    if(mDisplayAdapter.get())
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
        mPreviewBufs = (int32_t *)newBufProvider->allocateBuffer(width, height,previewFormat,0,CameraHal::NO_BUFFERS_PREVIEW);
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
                mPreviewBufs = (int32_t *)newBufProvider->allocateBuffer(width, height,previewFormat,0,CameraHal::NO_BUFFERS_PREVIEW);
                    if(!mPreviewBufs)
                    {
                    CAMHAL_LOGEA("Couldn't allocate preview buffers using Memory manager");
                    LOG_FUNCTION_NAME_EXIT
                    return NO_MEMORY;
                    }
                mBufProvider = newBufProvider;
                }
        }

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;

}

status_t CameraHal::allocImageBufs(int width, int height, const char* previewFormat)
    {

    LOG_FUNCTION_NAME

    ///@todo Enhance this method allocImageBufs() to take in a flag for burst capture
    ///Always allocate the buffers for image capture using MemoryManager
    if(!mImageBufs)
        {
        ///@todo calculate the worst case memory required for JPEG
        int bytes = width*height*2;
        CAMHAL_LOGDB("Size of Image cap buffer = %d", bytes);
        mImageBufs = (int32_t *)mMemoryManager->allocateBuffer(0, 0, previewFormat, bytes,CameraHal::NO_BUFFERS_IMAGE_CAPTURE);
        if(!mImageBufs)
            {
            LOG_FUNCTION_NAME_EXIT
            CAMHAL_LOGEA("Couldn't allocate image buffers using memory manager");
            return NO_MEMORY;
            }
        }

    LOG_FUNCTION_NAME

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

status_t CameraHal::freeImageBufs()
    {
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;
    if(mImageBufs)
        {
        ///@todo Pluralise the name of this method to freeBuffers
        ret = mMemoryManager->freeBuffer(mImageBufs);
        mImageBufs = NULL;
        LOG_FUNCTION_NAME_EXIT
        return ret;
        }

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
    int ret = NO_ERROR;
    int w, h;
    LOG_FUNCTION_NAME

    if(previewEnabled())
        {
        CAMHAL_LOGEA("Preview already running");
        LOG_FUNCTION_NAME_EXIT
        return ALREADY_EXISTS;
        }

    /// Ensure that buffers for preview are allocated before we start the camera
    mParameters.getPreviewSize(&w, &h);

    ///Allocate the preview buffers
    ret = allocPreviewBufs(w, h, mParameters.getPreviewFormat());
    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEA("Couldn't allocate buffers for Preview");
        goto error;
        }

    ///Pass the buffers to Camera Adapter
    mCameraAdapter->useBuffers(CameraAdapter::CAMERA_PREVIEW, mPreviewBufs, CameraHal::NO_BUFFERS_PREVIEW);

    ///Start the callback notifier
    ret = mAppCallbackNotifier->start();

    if(ret!=NO_ERROR)
        {
        CAMHAL_LOGEA("Couldn't start AppCallbackNotifier");
        goto error;
        }

    CAMHAL_LOGDA("Started AppCallbackNotifier..");

    ///Enable the display adapter if present, actual overlay enable happens when we post the buffer
    if(mDisplayAdapter.get())
        {
        CAMHAL_LOGDA("Enabling display");
        ret = mDisplayAdapter->enableDisplay();
        if(ret!=NO_ERROR)
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
    return ret;

    error:
        CAMHAL_LOGEA("Performing cleanup after error");
        //Do all the cleanup
        freePreviewBufs();
        mCameraAdapter->sendCommand(CameraAdapter::CAMERA_STOP_PREVIEW);
        mDisplayAdapter->disableDisplay();
        mAppCallbackNotifier->stop();
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
    ///If the Camera service passes a null overlay, we destroy existing overlay and free the DisplayAdapter
    if(!overlay.get())
        {
        if(mDisplayAdapter.get())
            {
            ///NULL overlay passed, destroy the display adapter if present
            CAMHAL_LOGEA("NULL Overlay passed to setOverlay, destroying display adapter");
            mDisplayAdapter.clear();
            }
        return ret;
        }

    ///CameraService passed a valid overlay
    if(!mDisplayAdapter.get())
        {
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
        mDisplayAdapter->setFrameProvider(mCameraAdapter.get());

        ///Any dynamic errors that happen during the camera use case has to be propagated back to the application
        ///via CAMERA_MSG_ERROR. AppCallbackNotifier is the class that  notifies such errors to the application
        ///Set it as the error handler for the DisplayAdapter
        mDisplayAdapter->setErrorHandler(mAppCallbackNotifier.get());
        }

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
            mCameraAdapter->useBuffers(CameraAdapter::CAMERA_PREVIEW, mPreviewBufs, CameraHal::NO_BUFFERS_PREVIEW);

            }
        else
            {
            ///Overlay supports external buffering, let DisplayAdapter know about the Camera buffers
            mDisplayAdapter->useBuffers(mPreviewBufs, CameraHal::NO_BUFFERS_PREVIEW);
            }

        ///Everything initialized, It's time to start the display of frames through overlay
        ret = mDisplayAdapter->enableDisplay();
        if(ret!=NO_ERROR)
            {
            CAMHAL_LOGEA("Couldn't enable display");
            return ret;
            }

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

    if(!previewEnabled())
        {
        LOG_FUNCTION_NAME_EXIT
        return;
        }

    ///The below sequence of stopping matters
    mDisplayAdapter->disableDisplay();
    mAppCallbackNotifier->stop();
    mCameraAdapter->sendCommand(CameraAdapter::CAMERA_STOP_PREVIEW);
    freePreviewBufs();
    freeImageBufs();
    mPreviewEnabled = false;

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
    return mPreviewEnabled;
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
    LOG_FUNCTION_NAME
    ///@todo Implement this when video mode will be supported
    return NO_ERROR;
}

/**
   @brief Stop a previously started recording.

   @param none
   @return none

 */
void CameraHal::stopRecording()
{
    LOG_FUNCTION_NAME
    ///@todo Implement this when video mode will be supported
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
    ///@todo Implement this when video mode will be supported
    return false;
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
    ///@todo Implement this when video mode will be supported
    return;
}

/**
   @brief Start auto focus

   The notification callback routine is called with CAMERA_MSG_FOCUS once when
   focusing is complete. autoFocus() will be called again if another auto focus is
   needed.

   @param none
   @return NO_ERORR If the focus is locked
   @todo Define the error codes if the focus is not locked

 */
status_t CameraHal::autoFocus()
{
    LOG_FUNCTION_NAME
    ///@todo Implement this when auto focus will be supported
    return NO_ERROR;

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
    ///@todo Implement this when auto focus will be supported
    return NO_ERROR;
}

/**
   @brief Take a picture.

   @param none
   @return NO_ERROR If able to switch to image capture
   @todo Define error codes if unable to switch to image capture

 */
status_t CameraHal::takePicture( )
{
    LOG_FUNCTION_NAME
    ///@todo Implement this when image capture will be supported
    return NO_ERROR;
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
    LOG_FUNCTION_NAME
    ///@todo Implement this when image capture will be supported
    return NO_ERROR;
}

/**
   @brief Return the camera parameters.

   @param none
   @return Currently configured camera parameters

 */
CameraParameters CameraHal::getParameters() const
{
    LOG_FUNCTION_NAME
    ///Return the current set of parameters
    return mParameters;
}

/**
   @brief Send command to camera driver.

   @param none
   @return NO_ERROR If the command succeeds
   @todo Define the error codes that this function can return

 */
status_t CameraHal::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2)
{
    LOG_FUNCTION_NAME
    ///@todo Implement this when smooth zoom feature will be supported
    return NO_ERROR;
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
CameraHal::CameraHal()
{
    LOG_FUNCTION_NAME
    ///Initialize all the member variables to their defaults
    mPreviewBufsAllocatedUsingOverlay = false;
    mPreviewEnabled = false;
    mPreviewBufs = NULL;
    mImageBufs = NULL;
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
    ///Call de-initialize here once more - it is the last chance for us to relinquish all the h/w and s/w resources
    deinitialize();
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

    int numCameras = 0;

    ///Initialize the event mask used for registering an event provider for AppCallbackNotifier
    ///Currently, registering all events as to be coming from CameraAdapter
    int32_t eventMask = CameraHalEvent::ALL_EVENTS;

    ///Create the CameraProperties class
    ///We create the default Camera Adapter (The adapter which is selected by default, if no specific camera is chosen using setParameter)

    mCameraProperties = new CameraProperties();
    if(!mCameraProperties.get() || (mCameraProperties->initialize()!=NO_ERROR))
        {
        CAMHAL_LOGEA("Unable to create or initialize CameraProperties");
        goto fail_loop;
        }

    ///Query the number of cameras supported, minimum we expect is one (at least FakeCameraAdapter)
    numCameras = mCameraProperties->camerasSupported();
    if(!numCameras)
        {
        CAMHAL_LOGEA("No cameras supported in Camera HAL implementation");
        goto fail_loop;
        }
    else
        {
        CAMHAL_LOGDB("Cameras found %d", numCameras);
        }

    ///Get the default Camera, which is Camera 0
    mCameraPropertiesArr = (CameraProperties::CameraProperty**)mCameraProperties->getProperties(0);

    if(!mCameraPropertiesArr)
        {
        goto fail_loop;
        }

    ///Dump the properties of this Camera
    dumpProperties(mCameraPropertiesArr);

    ///Check if a valid adapter DLL name is present for the first camera
    if(!mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME])
        {
        CAMHAL_LOGEA("No Camera adapter DLL set for default camera");
        goto fail_loop;
        }

    /// Create the camera adapter
    /// @todo Incorporate dynamic loading based on cfg file. For now using a constant definition for the adapter dll name
    mCameraAdapterHandle = ::dlopen((const char*)mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME]->mPropValue, RTLD_NOW);
    if( mCameraAdapterHandle == NULL ) {
       CAMHAL_LOGEA("Unable to open CameraAdapter Library for default camera");
       LOG_FUNCTION_NAME_EXIT
       goto fail_loop;
    }

    f = (CameraAdapterFactory) ::dlsym(mCameraAdapterHandle, "CameraAdapter_Factory");
    if(!f)
        {
        CAMHAL_LOGEB("%s does not export required factory method CameraAdapter_Factory"
            ,mCameraPropertiesArr[CameraProperties::PROP_INDEX_CAMERA_ADAPTER_DLL_NAME]
            );
        goto fail_loop;
        }

    mCameraAdapter = f();
    if(!mCameraAdapter.get())
        {
        CAMHAL_LOGEA("Unable to create or initialize CameraAdapter");
        goto fail_loop;
        }

    /// Create the callback notifier
    mAppCallbackNotifier = new AppCallbackNotifier();
    if(!mAppCallbackNotifier.get() || (mAppCallbackNotifier->initialize()!=NO_ERROR))
        {
        CAMHAL_LOGEA("Unable to create or initialize AppCallbackNotifier");
        goto fail_loop;
        }

    /// Create Memory Manager
    mMemoryManager = new MemoryManager();
    if(!mMemoryManager.get() || (mMemoryManager->initialize()!=NO_ERROR))
        {
        CAMHAL_LOGEA("Unable to create or initialize MemoryManager");
        goto fail_loop;
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
    mAppCallbackNotifier->setEventProvider(eventMask, mCameraAdapter.get());
    mAppCallbackNotifier->setFrameProvider(mCameraAdapter.get());



    ///Any dynamic errors that happen during the camera use case has to be propagated back to the application
    ///via CAMERA_MSG_ERROR. AppCallbackNotifier is the class that  notifies such errors to the application
    ///Set it as the error handler for CameraAdapter
    mCameraAdapter->setErrorHandler(mAppCallbackNotifier.get());


    ///Initialize default parameters
    initDefaultParameters();

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;

    fail_loop:
        ///Free up the resources because we failed somewhere up
        deinitialize();
        LOG_FUNCTION_NAME_EXIT
        return NO_MEMORY;

}

void CameraHal::dumpProperties(CameraProperties::CameraProperty** cameraProps)
{
    for ( int i = 0 ; i < CameraProperties::PROP_INDEX_MAX; i++)
        CAMHAL_LOGDB("%s = %s", cameraProps[i]->mPropName, cameraProps[i]->mPropValue);
}

void CameraHal::initDefaultParameters()
{
    //Purpose of this function is to initialize the default current and supported parameters for the currently
    //selected camera.
    //@todo Should query from a CameraProperties class which is maintained per camera supported on the
    //        platform
    CameraParameters p;

    LOG_FUNCTION_NAME

    p.setPreviewSize(MIN_WIDTH, MIN_HEIGHT);
    p.setPreviewFrameRate(30);
    p.setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV422I);

    ///@todo set the maximum capture size from property class based on camera selected
    p.setPictureSize(PICTURE_WIDTH, PICTURE_HEIGHT);
    p.setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
    p.set(CameraParameters::KEY_JPEG_QUALITY, 100);

    //Eclair extended parameters
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PICTURE_SIZES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PICTURE_FORMATS]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_SIZES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_FORMATS]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_PREVIEW_FRAME_RATES]->mPropValue);
    p.set(CameraParameters::KEY_SUPPORTED_THUMBNAIL_SIZES, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_THUMBNAIL_SIZES]->mPropValue);
    p.set(CameraParameters::KEY_WHITE_BALANCE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_WHITE_BALANCE]->mPropValue);
    p.set(CameraParameters::KEY_EFFECT, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_EFFECTS]->mPropValue);
    p.set(CameraParameters::KEY_SCENE_MODE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_SCENE_MODES]->mPropValue);
    p.set(CameraParameters::KEY_FOCUS_MODE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_FOCUS_MODES]->mPropValue);
    p.set(CameraParameters::KEY_ANTIBANDING, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_ANTIBANDING]->mPropValue);
    p.set(CameraParameters::KEY_FLASH_MODE, (const char*) mCameraPropertiesArr[CameraProperties::PROP_INDEX_SUPPORTED_FLASH_MODES]->mPropValue);
    //

    //@todo Define constants for these custom parameters instead of hard-coding them
    //p.set("max-zoom" , MAX_ZOOM);
    //p.set("zoom", 0);

    if ( setParameters(p) != NO_ERROR )
        {
        CAMHAL_LOGEA("Failed to set default parameters?!");
        }

    LOG_FUNCTION_NAME_EXIT
}


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

    /// Free the callback notifier
    mAppCallbackNotifier.clear();

    /// Free the memory manager
    mMemoryManager.clear();

    /// Free the display adapter
    mDisplayAdapter.clear();

    /// Free the camera adapter
    mCameraAdapter.clear();

    ///Close the camera adapter DLL
    ::dlclose(mCameraAdapterHandle);

    LOG_FUNCTION_NAME_EXIT

}

/*-------------Camera Hal Internal Method definitions ENDS here--------------------*/

/*--------------------Camera HAL Creation Related---------------------------------------*/

sp<CameraHardwareInterface> CameraHal::createInstance()
{
    LOG_FUNCTION_NAME

    sp<CameraHardwareInterface> hardware(NULL);
    if (singleton != 0)
        {
        hardware = singleton.promote();
        if (hardware != 0)
            {
            CAMHAL_LOGDA("Duplicate instance of Camera");
            return hardware;
            }
        }

    CameraHal *ptr = new CameraHal();

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


    singleton = hardware = ptr;

    LOG_FUNCTION_NAME_EXIT
    return hardware;
}


extern "C" sp<CameraHardwareInterface> openCameraHardware()
{
    LOG_FUNCTION_NAME
    CAMHAL_LOGDA("opening ti camera hal\n");
    return CameraHal::createInstance();
}


};



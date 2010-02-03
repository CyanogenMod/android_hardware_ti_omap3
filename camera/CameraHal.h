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


#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_H

#include <stdio.h>
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
#include <ui/CameraHardwareInterface.h>
#include "MessageQueue.h"
#include "overlay_common.h"
#include <JpegEncoderEXIF.h>

#define RESIZER 1
#define JPEG 1
#define VPP 1

#define PPM_INSTRUMENTATION 1
//#undef FW3A
//#undef ICAP
//#undef IMAGE_PROCESSING_PIPELINE

#ifdef FW3A
#include "osal/osal_stdtypes.h"
#include "osal/osal_sysdep.h"
#include "fw/icapture/linux/camera_2A/icapture_interface.h"
#include "../modules/fw/include/mk_note/maker_note_mms_priv.h"
#include "fw/vfinder/linux/camera_2A/camera_alg.h"
#endif

#ifdef HARDWARE_OMX
#include "JpegEncoder.h"
#endif

#ifdef IMAGE_PROCESSING_PIPELINE
#include "imageprocessingpipeline.h"
#include "ipp_algotypes.h"
#include "capdefs.h"

#define OPP_OPTIMIZATION 1

#ifdef OPP_OPTIMIZATION

extern "C" {
    #include "ResourceManagerProxyAPI.h"
    #include "Resource_Activity_Monitor.h"
}

#endif

#define BUFF_MAP_PADDING_TEST 256*2
#define PADDING_OFFSET_TEST 256
#define MAXIPPDynamicParams 10
#endif

#define VIDEO_DEVICE        "/dev/video5"
#define MIN_WIDTH           128
#define MIN_HEIGHT          96
#define PICTURE_WIDTH   3296 /* 5mp - 2560. 8mp - 3280 */ /* Make sure it is a multiple of 16. */
#define PICTURE_HEIGHT  2464 /* 5mp - 2048. 8mp - 2464 */ /* Make sure it is a multiple of 16. */
#define PREVIEW_WIDTH 176
#define PREVIEW_HEIGHT 144
#define PIXEL_FORMAT           V4L2_PIX_FMT_UYVY
#define LOG_FUNCTION_NAME    LOGD("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    LOGD("%d: %s() EXIT", __LINE__, __FUNCTION__);
#define VIDEO_FRAME_COUNT_MAX    NUM_OVERLAY_BUFFERS_REQUESTED
#define MAX_CAMERA_BUFFERS    NUM_OVERLAY_BUFFERS_REQUESTED
#define MAX_ZOOM        3
#define THUMB_WIDTH     80
#define THUMB_HEIGHT    60

#define PIX_YUV422I 0
#define PIX_YUV420P 1

namespace android {

#ifdef IMAGE_PROCESSING_PIPELINE
	#define INPLACE_ON	1
	#define INPLACE_OFF	0
	#define IPP_Disabled_Mode 0
	#define IPP_CromaSupression_Mode 1
	#define IPP_EdgeEnhancement_Mode 2
typedef struct OMX_IPP
{
    IPP_Handle hIPP;
    IPP_ConfigurationTypes ippconfig;
    IPP_CRCBSAlgoCreateParams CRCBptr;
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

#endif    

//icapture
#define DTP_FILE_NAME 	    "/data/dyntunn.enc"
#define EEPROM_FILE_NAME    "/data/eeprom.hex"
#define LIBICAPTURE_NAME    "libicapture.so"

// 3A FW
#define LIB3AFW             "libMMS3AFW.so"

#define PHOTO_PATH          "/tmp/photo_%02d.%s"

#define PROC_THREAD_PROCESS     0x5
#define PROC_THREAD_EXIT        0x6
#define PROC_THREAD_NUM_ARGS    22
#define SHUTTER_THREAD_CALL     0x1
#define SHUTTER_THREAD_EXIT     0x2
#define SHUTTER_THREAD_NUM_ARGS 3
#define RAW_THREAD_CALL         0x1
#define RAW_THREAD_EXIT         0x2
#define RAW_THREAD_NUM_ARGS     4
#define SNAPSHOT_THREAD_START   0x1
#define SNAPSHOT_THREAD_EXIT    0x2

#define PAGE                    0x1000
#define PARAM_BUFFER            512

#ifdef FW3A
typedef struct {
    /* shared library handle */
    void *lib_handle;

    /* Init 2A library */
    int (*Init2A) (Camera2AInterface **cam_face, int dev_cam,
           uint8 enable3PTuningAlg);
    /* Release 2A library */
    int (*Release2A) (Camera2AInterface **cam_face);
} lib3a_interface;

typedef struct {
    void              *lib_private;
    lib3a_interface    lib;

    /* hold 2A settings */
    Cam2ASettings      settings_2a;

    /* hold 2A camera interface */
    Camera2AInterface *cam_iface_2a;

#ifdef EXTEND_TI_3A /* still not supported from TI */
    /* hold 2A status */
    Cam2AStatus        status_2a;
#endif

    /* hold camera device descriptor */
    int                cam_dev;

    struct fw3a_preview_obj *prev_handle;

} lib3atest_obj;
#endif

#ifdef ICAP
typedef struct{
	void *lib_handle;
	int (*Create) (void **lib_private,int camera_device);
	int (*Delete) (void *lib_private);
	int (*Config) (void *lib_private, capture_config_t *cfg);
	int (*Process) (void *lib_private, capture_process_t *proc);
} libiCaptureInterface;

typedef struct {
	void	*lib_private;
	capture_config_t cfg;
	capture_process_t proc;
	capture_buffer_t img_buf;
	capture_buffer_t lsc_buff;
	libiCaptureInterface lib;
} libtest_obj;
#endif

typedef struct {
    int longDeg, longMin, longSec;
    int latDeg, latMin, latSec;
    int altitude;
    unsigned long timestamp;
} gps_data;

class CameraHal : public CameraHardwareInterface {
public:
    virtual sp<IMemoryHeap> getRawHeap() const;
    virtual void stopRecording();

	virtual status_t startRecording();  // Eclair HAL
    virtual bool recordingEnabled();
    virtual void releaseRecordingFrame(const sp<IMemory>& mem);
    virtual sp<IMemoryHeap> getPreviewHeap() const ;

	virtual status_t startPreview();   //  Eclair HAL
    virtual bool useOverlay() { return true; }
    virtual status_t setOverlay(const sp<Overlay> &overlay);
    virtual void stopPreview();
    virtual bool previewEnabled();

	virtual status_t autoFocus();  // Eclair HAL


	virtual status_t takePicture();		// Eclair HAL

	virtual status_t cancelPicture();	// Eclair HAL
	virtual status_t cancelAutoFocus();

    virtual status_t dump(int fd, const Vector<String16>& args) const;
    void dumpFrame(void *buffer, int size, char *path);
    virtual status_t setParameters(const CameraParameters& params);
    virtual CameraParameters getParameters() const;
    virtual void release();
    void initDefaultParameters();

/*--------------------Eclair HAL---------------------------------------*/
     virtual void        setCallbacks(notify_callback notify_cb,
                                     data_callback data_cb,
                                     data_callback_timestamp data_cb_timestamp,
                                     void* user);

    virtual void        enableMsgType(int32_t msgType);
    virtual void        disableMsgType(int32_t msgType);
    virtual bool        msgTypeEnabled(int32_t msgType);
/*--------------------Eclair HAL---------------------------------------*/
    static sp<CameraHardwareInterface> createInstance();

    virtual status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);

//private:

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

    static int onSaveH3A(void *priv, void *buf, int size);
    static int onSaveLSC(void *priv, void *buf, int size);
    static int onSaveRAW(void *priv, void *buf, int size);
    static int onSnapshot(void *priv, void *buf, int width, int height);

    CameraHal();
    virtual ~CameraHal();
    void previewThread();
    int validateSize(int w, int h);
    void procThread();
    void shutterThread();
    void rawThread();
    void snapshotThread();
    
#ifdef FW3A
    int FW3A_Create();
    int FW3A_Destroy();
    int FW3A_Start();
    int FW3A_Stop();
    int FW3A_Start_CAF();
    int FW3A_Stop_CAF();
    int FW3A_Start_AF();
    int FW3A_Stop_AF();
    int FW3A_DefaultSettings();
    int FW3A_GetSettings();
    int FW3A_SetSettings();
#endif

    int ZoomPerform(float zoom);
    void nextPreview();
    int ICapturePerform();
    int ICaptureCreate(void);
    int ICaptureDestroy(void);
	void PPM(const char *);
	void PPM(const char *,struct timeval*);
    status_t convertGPSCoord(double coord, int *deg, int *min, int *sec);

#ifndef ICAP
	int CapturePicture();
#endif

#ifdef IMAGE_PROCESSING_PIPELINE
    int DeInitIPP();
    int InitIPP(int w, int h, int fmt);
    int PopulateArgsIPP(int w, int h, int fmt);
    int ProcessBufferIPP(void *pBuffer, long int nAllocLen, int fmt,
                         int EdgeEnhancementStrength, int WeakEdgeThreshold,
                         int StrongEdgeThreshold, int LumaNoiseFilterStrength,
                         int ChromaNoiseFilterStrength);

    OMX_IPP pIPP;	
#endif   

    int CameraCreate();
    int CameraDestroy(bool destroyOverlay);
    int CameraConfigure();	
	int CameraSetFrameRate();
    int CameraStart();
    int CameraStop();

#ifdef ICAP_EXPIREMENTAL

    int allocatePictureBuffer(size_t length);

#else

    int allocatePictureBuffer(int width, int height);

#endif

    int SaveFile(char *filename, char *ext, void *buffer, int jpeg_size);
    
    int isStart_FW3A;
    int isStart_FW3A_AF;
    int isStart_FW3A_CAF;
    int isStart_FW3A_AEWB;
    int isStart_VPP;
    int isStart_JPEG;
    int FW3A_AF_TimeOut;
	    
    mutable Mutex mLock;
    gps_data *gpsLocation;
    CameraParameters mParameters;
    sp<MemoryHeapBase> mPictureHeap, mJPEGPictureHeap;
    int mPictureOffset, mJPEGOffset, mJPEGLength, mPictureLength;
    void *mYuvBuffer, *mJPEGBuffer;
    int  mPreviewFrameSize;
    sp<Overlay>  mOverlay;
    sp<PreviewThread>  mPreviewThread;
	sp<PROCThread>  mPROCThread;
	sp<ShutterThread> mShutterThread;
	sp<RawThread> mRawThread;
	sp<SnapshotThread> mSnapshotThread;
    bool mPreviewRunning;
    bool mIPPInitAlgoState;
    bool mIPPToEnable;
    Mutex mRecordingLock;
    int mRecordingFrameSize;
    // Video Frame Begin
    int mVideoBufferCount;
    sp<MemoryHeapBase> mVideoHeaps[VIDEO_FRAME_COUNT_MAX];
    sp<MemoryBase> mVideoBuffer[VIDEO_FRAME_COUNT_MAX];

    // ...
    int nOverlayBuffersQueued;
    int nCameraBuffersQueued;
    struct v4l2_buffer v4l2_cam_buffer[MAX_CAMERA_BUFFERS];
    int buffers_queued_to_dss[MAX_CAMERA_BUFFERS];
	int mfirstTime;
    static wp<CameraHardwareInterface> singleton;
    static int camera_device;
    static const char supportedPictureSizes[];
    static const char supportedPreviewSizes[];
    static const char supportedFPS[];
    static const char supprotedThumbnailSizes[];
    static const char PARAMS_DELIMITER[];
	int procPipe[2], shutterPipe[2], rawPipe[2], snapshotPipe[2], snapshotReadyPipe[2];
	int mippMode;
	int pictureNumber;
	int rotation;
#if PPM_INSTRUMENTATION
    struct timeval ppm;
	struct timeval ppm_start;
	struct timeval ppm_receiveCmdToTakePicture;
    struct timeval focus_before, focus_after;
    struct timeval ppm_before, ppm_after;
    struct timeval ipp_before, ipp_after;
#endif
	int lastOverlayBufferDQ;

/*--------------Eclair Camera HAL---------------------*/

	notify_callback mNotifyCb;
	data_callback   mDataCb;
	data_callback_timestamp mDataCbTimestamp;
	void                 *mCallbackCookie;

	int32_t             mMsgEnabled;
	bool                mRecordEnabled;
	nsecs_t             mCurrentTime;
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
    int mflash;
    int mred_eye;
    int mcapture_mode;
    int mZoomCurrentIdx, mZoomTargetIdx, mZoomSpeed;
    int mcaf;
    int j;

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
        ZOOM_UPDATE,
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

    MessageQueue    previewThreadCommandQ;
    MessageQueue    previewThreadAckQ;    
    MessageQueue    processingThreadCommandQ;
    MessageQueue    processingThreadAckQ;

    mutable Mutex takephoto_lock;
    uint8_t *yuv_buffer, *jpeg_buffer, *vpp_buffer, *ancillary_buffer;
    
    int yuv_len, jpeg_len, ancillary_len;

    FILE *foutYUV;
    FILE *foutJPEG;
};

}; // namespace android

extern "C" {
    int scale_init(int inWidth, int inHeight, int outWidth, int outHeight, int inFmt, int outFmt);
    int scale_deinit();
    int scale_process(void* inBuffer, int inWidth, int inHeight, void* outBuffer, int outWidth, int outHeight, int rotation, int fmt, float zoom);
}

#endif

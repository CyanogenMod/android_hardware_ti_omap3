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
#include <utils/MemoryBase.h>
#include <utils/MemoryHeapBase.h>
#include <utils/threads.h>
#include <ui/CameraHardwareInterface.h>
#include "MessageQueue.h"

#ifdef FW3A
#include "osal/osal_sysdep.h"
#include "osal/osal_stdtypes.h"
#include "icapture/icapture_interface.h"
#include "mk_note/maker_note_mms.h"
#include "fw/fw_message.h"
#include "camera_alg.h"
#endif

#ifdef HARDWARE_OMX
#include "JpegEncoder.h"
#endif

#ifdef IMAGE_PROCESSING_PIPELINE
#include "imageprocessingpipeline.h"
#include "ipp_algotypes.h"
#include "capdefs.h"

#define BUFF_MAP_PADDING_TEST 256*2
#define PADDING_OFFSET_TEST 256
#define MAXIPPDynamicParams 10
#endif


#define VIDEO_DEVICE        "/dev/video5"
#define MIN_WIDTH           208 // 960 //820
#define MIN_HEIGHT          154 // 800 //616
#define PICTURE_WIDTH   3280 /* 5mp - 2560. 8mp - 3280 */ /* Make sure it is a multiple of 16. */
#define PICTURE_HEIGHT  2464 /* 5mp - 2048. 8mp - 2464 */ /* Make sure it is a multiple of 16. */
#define PIXEL_FORMAT           V4L2_PIX_FMT_UYVY
#define LOG_FUNCTION_NAME    LOGD("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    LOGD("%d: %s() EXIT", __LINE__, __FUNCTION__);
#define VIDEO_FRAME_COUNT_MAX    4
#define OPEN_CLOSE_WORKAROUND	 1

#define JPEG 1

#define PPM(str){ \
	gettimeofday(&ppm, NULL); \
	ppm.tv_sec = ppm.tv_sec - ppm_start.tv_sec; \
	ppm.tv_sec = ppm.tv_sec * 1000000; \
	ppm.tv_sec = ppm.tv_sec + ppm.tv_usec - ppm_start.tv_usec; \
	LOGD("PPM: %s :%d.%d ms",str, ppm.tv_sec/1000, ppm.tv_sec%1000 ); \
}

namespace android {

#ifdef IMAGE_PROCESSING_PIPELINE
	//if YUV422I is 0, we use YUV420P converter in IPP but currently there are issues to encode 420P images.
	#define YUV422I 1
	#define INPLACE_ON	1
	#define INPLACE_OFF	0
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
#define DTP_FILE_NAME 	"/data/dyntunn.enc"
#define EEPROM_FILE_NAME "/data/eeprom.hex"
#define LIBICAPTURE_NAME "libicapture.so"

// 3A FW
#define LIB3AFW "libMMS3AFW.so"

#define PHOTO_PATH "/sdcard/photo_%02d.%s"

#define START_CMD 0x0
#define STOP_CMD 0x1
#define RUN_CMD	0x2
#define TAKE_PICTURE	0x3
#define NOOP	0x4

#define VPP_THREAD_PROCESS 0x5
#define VPP_THREAD_EXIT 0x6

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

class CameraHal : public CameraHardwareInterface {
public:
    virtual sp<IMemoryHeap> getRawHeap() const;
    virtual void stopRecording();
    virtual status_t startRecording(recording_callback cb, void* user);
    virtual bool recordingEnabled();
    virtual void releaseRecordingFrame(const sp<IMemory>& mem);
    virtual sp<IMemoryHeap> getPreviewHeap() const ;
    virtual status_t startPreview(preview_callback cb, void* user);
    virtual bool useOverlay() { return true; }
    virtual status_t setOverlay(const sp<Overlay> &overlay);
    virtual void stopPreview();
    virtual bool previewEnabled();
    virtual status_t autoFocus(autofocus_callback, void *user);
    virtual status_t takePicture(shutter_callback,
                                    raw_callback,
                                    jpeg_callback,
                                    void* user);
    virtual status_t cancelPicture(bool cancel_shutter,
                                      bool cancel_raw,
                                      bool cancel_jpeg);
    virtual status_t dump(int fd, const Vector<String16>& args) const;
    void dumpFrame(void *buffer, int size, char *path);
    virtual status_t setParameters(const CameraParameters& params);
    virtual CameraParameters getParameters() const;
    virtual void release();
    void initDefaultParameters();
    static sp<CameraHardwareInterface> createInstance();

private:

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

    class VPPThread : public Thread {
        CameraHal* mHardware;
    public:
        VPPThread(CameraHal* hw)
            : Thread(false), mHardware(hw) { }

        virtual bool threadLoop() {
            mHardware->vppThread();
            return false;
        }
    };

    class ProcThread : public Thread {
        CameraHal *mHardware;

        public:
            ProcThread(CameraHal *hw) : Thread(false), mHardware(hw) {}
            virtual bool threadLoop(){
                mHardware->procThread();
                return false;			
            }
    };

    class FacetrackingThread : public Thread{
        CameraHal *mHardware;

        public:
            FacetrackingThread(CameraHal *hw) : Thread(false), mHardware(hw) {}
            virtual bool threadLoop(){
                mHardware->facetrackingThread();
                return false;
            }
    };

    static int onSaveH3A(void *priv, void *buf, int size);
    static int onSaveLSC(void *priv, void *buf, int size);
    static int onSaveRAW(void *priv, void *buf, int size);

    CameraHal();
    virtual ~CameraHal();
    void previewThread();
	void vppThread();
    int validateSize(int w, int h);	
    void drawRect(uint8_t *input, uint8_t color, int x1, int y1, int x2, int y2, int width, int height);
    void procThread();
    void facetrackingThread();
    
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

    int ZoomPerform(int zoom);
    void nextPreview();
    int ICapturePerform();
    int ICaptureCreate(void);
    int ICaptureDestroy(void);

#ifndef ICAP
	int CapturePicture();
#endif

#ifdef IMAGE_PROCESSING_PIPELINE
    int DeInitIPP();
    int InitIPP(int w, int h);
    int PopulateArgsIPP(int w, int h);
    int ProcessBufferIPP(void *pBuffer, long int nAllocLen,
                         int EdgeEnhancementStrength, int WeakEdgeThreshold, 
                         int StrongEdgeThreshold, int LumaNoiseFilterStrength,
                         int ChromaNoiseFilterStrength);

    OMX_IPP pIPP;
#endif   

    int CameraCreate();
    int CameraDestroy();
    int CameraConfigure();
    int CameraStart();
    int CameraStop();
    int SaveFile(char *filename, char *ext, void *buffer, int jpeg_size);
    
    int isStart_FW3A;
    int isStart_FW3A_AF;
    int isStart_FW3A_CAF;
    int isStart_FW3A_AEWB;
    int FW3A_AF_TimeOut;
	    
    mutable Mutex mLock;
    CameraParameters mParameters;
    sp<MemoryHeapBase> mPictureHeap;
    int  mPreviewFrameSize;
    shutter_callback mShutterCallback;
    raw_callback mRawPictureCallback;
    jpeg_callback mJpegPictureCallback;
    void *mPictureCallbackCookie;
    sp<Overlay>  mOverlay;
    sp<PreviewThread>  mPreviewThread;
	sp<VPPThread>  mVPPThread;
    bool mPreviewRunning;
    Mutex               mRecordingLock;
    int mRecordingFrameSize;
    recording_callback mRecordingCallback;
    void  *mRecordingCallbackCookie;
    // Video Frame Begin
    int                 mVideoBufferCount;
    sp<MemoryHeapBase>  mVideoHeap;
    sp<MemoryBase>      mVideoBuffer[VIDEO_FRAME_COUNT_MAX];
    v4l2_buffer         mfilledbuffer[VIDEO_FRAME_COUNT_MAX];
    unsigned long       mVideoBufferPtr[VIDEO_FRAME_COUNT_MAX];
    int                 mVideoBufferUsing[VIDEO_FRAME_COUNT_MAX];
    int                 mRecordingFrameCount;
    // ...
    autofocus_callback  mAutoFocusCallback;
    void *mAutoFocusCallbackCookie;
    int nOverlayBuffersQueued;
    int nCameraBuffersQueued;
    static wp<CameraHardwareInterface> singleton;
    static int camera_device;
    struct timeval ppm;
	struct timeval ppm_start;
	int vppPipe[2];
    sem_t mIppVppSem;
    
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
    int mzoom;
    int mcaf;
    int j;
    int myuv;
    int mMMSApp;

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
    uint8_t *yuv_buffer, *jpeg_buffer, *vpp_buffer;
    int yuv_len, jpeg_len;

    FILE *foutYUV;
    FILE *foutJPEG;
	 
};

}; // namespace android

extern "C" {
    int scale_init();
    int scale_deinit();
    int scale_process(void* inBuffer, int inWidth, int inHeight, void* outBuffer, int outWidth, int outHeight);
}

#endif

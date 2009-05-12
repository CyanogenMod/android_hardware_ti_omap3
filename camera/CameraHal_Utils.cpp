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
* @file CameraHal_Utils.cpp
*
* This file maps the Camera Hardware Interface to V4L2.
*
*/

#include "CameraHal.h"

namespace android {

#ifdef FW3A
int CameraHal::FW3A_Create()
{
    int err = 0;

    LOG_FUNCTION_NAME

    fobj = (lib3atest_obj*)malloc(sizeof(*fobj));
    memset(fobj, 0 , sizeof(*fobj));
    if (!fobj) {
        LOGE("cannot alloc fobj");
        goto exit;
    }

  	fobj->lib.lib_handle = dlopen(LIB3AFW, RTLD_LAZY);
    if (NULL == fobj->lib.lib_handle) {
		LOGE("A dynamic linking error occurred: (%s)", dlerror());
        LOGE("ERROR - at dlopen for %s", LIB3AFW);
        goto exit;
    }

    /* get Init2A symbol */
    fobj->lib.Init2A = (int (*)(Camera2AInterface**, int, uint8)) dlsym(fobj->lib.lib_handle, "Init2A");
    if (NULL == fobj->lib.Init2A) {
        LOGE("ERROR - can't get dlsym \"Init2A\"");
        goto exit;
    }

    /* get Release2A symbol */
    fobj->lib.Release2A = (int (*)(Camera2AInterface**)) dlsym(fobj->lib.lib_handle, "Release2A");
    if (NULL == fobj->lib.Release2A) {
       LOGE( "ERROR - can't get dlsym \"Release2A\"");
       goto exit;
    }

    //TODO:
    fobj->cam_dev = 0;
    
    fobj->lib.Init2A(&fobj->cam_iface_2a, fobj->cam_dev, 0);

    LOGD("FW3A Create - %d   fobj=%p", err, fobj);

    LOG_FUNCTION_NAME_EXIT
	
    return 0;

exit:
    LOGD("Can't create 3A FW");
    return -1;
}

int CameraHal::FW3A_Destroy()
{
    int ret;

    LOG_FUNCTION_NAME_EXIT

    ret = fobj->lib.Release2A(&fobj->cam_iface_2a);
  	if (ret < 0) {
    		LOGE("Cannot Release2A");
  	} else {
        LOGD("2A released");
    }

   	dlclose(fobj->lib.lib_handle);
  	free(fobj);
    fobj = NULL;

    LOG_FUNCTION_NAME_EXIT

    return 0;
}

int CameraHal::FW3A_Start()
{
    int ret;

    LOG_FUNCTION_NAME

    if (isStart_FW3A!=0) {
        LOGE("3A FW is already started");
        return -1;
    }
    //Start 3AFW
    ret = fobj->cam_iface_2a->Start2A(fobj->cam_iface_2a->pPrivateHandle);
    if (0 > ret) {
        LOGE("Cannot Start 2A");
        return -1;
    } else {
        LOGE("3A FW Start - success");
    }

    LOG_FUNCTION_NAME_EXIT

    isStart_FW3A = 1;
    return 0;
}

int CameraHal::FW3A_Stop()
{
    int ret;

    LOG_FUNCTION_NAME

    if (isStart_FW3A==0) {
        LOGE("3A FW is already stopped");
        return -1;
    }

    //Stop 3AFW
    ret = fobj->cam_iface_2a->Stop2A(fobj->cam_iface_2a->pPrivateHandle);
    if (0 > ret) {
        LOGE("Cannot Stop 3A");
        return -1;
    } else {
        LOGE("3A FW Stop - success");
    }

    LOG_FUNCTION_NAME_EXIT

    isStart_FW3A = 0;
    return 0;
}

int CameraHal::FW3A_Start_CAF()
{
    int ret;
    
    LOG_FUNCTION_NAME

    if (isStart_FW3A_CAF!=0) {
        LOGE("3A FW CAF is already started");
        return -1;
    }

    if (fobj->settings_2a.af.focus_mode != FOCUS_MODE_AF_CONTINUOUS_NORMAL) {
      FW3A_GetSettings();
      fobj->settings_2a.af.focus_mode = FOCUS_MODE_AF_CONTINUOUS_NORMAL;
      ret = FW3A_SetSettings();
      if (0 > ret) {
          LOGE("Cannot Start CAF");
          return -1;
      } else {
          LOGE("3A FW CAF Start - success");
      }
    }
    if (ret == 0) {
      ret = fobj->cam_iface_2a->StartAF(fobj->cam_iface_2a->pPrivateHandle);
    }

    isStart_FW3A_CAF = 1;

    LOG_FUNCTION_NAME_EXIT

    return 0;
}

int CameraHal::FW3A_Stop_CAF()
{
    int ret, prev = isStart_FW3A_CAF;

    LOG_FUNCTION_NAME

    if (isStart_FW3A_CAF==0) {
        LOGE("3A FW CAF is already stopped");
        return prev;
    }

    ret = fobj->cam_iface_2a->StopAF(fobj->cam_iface_2a->pPrivateHandle);
    if (0 > ret) {
        LOGE("Cannot Stop CAF");
        return -1;
    } else {
        LOGE("3A FW CAF Start - success");
    }

    isStart_FW3A_CAF = 0;

    LOG_FUNCTION_NAME_EXIT

    return prev;
}

//TODO: Add mode argument
int CameraHal::FW3A_Start_AF()
{
    int ret = 0;
    
    LOG_FUNCTION_NAME

    if (isStart_FW3A_AF!=0) {
        LOGE("3A FW AF is already started");
        return -1;
    }

    if (fobj->settings_2a.af.focus_mode != FOCUS_MODE_AF_AUTO) {
      FW3A_GetSettings();
      fobj->settings_2a.af.focus_mode = FOCUS_MODE_AF_AUTO;
      ret = FW3A_SetSettings();
    }
    
    if (ret == 0) {
      ret = fobj->cam_iface_2a->StartAF(fobj->cam_iface_2a->pPrivateHandle);
    }
    if (0 > ret) {
        LOGE("Cannot Start AF");
        return -1;
    } else {
        LOGE("3A FW AF Start - success");
    }

    LOG_FUNCTION_NAME_EXIT

    isStart_FW3A_AF = 1;
    return 0;
}

int CameraHal::FW3A_Stop_AF()
{
    int ret, prev = isStart_FW3A_AF;

    LOG_FUNCTION_NAME

    if (isStart_FW3A_AF==0) {
        LOGE("3A FW AF is already stopped");
        return isStart_FW3A_AF;
    }

    //Stop 3AFW
    ret = fobj->cam_iface_2a->StopAF(fobj->cam_iface_2a->pPrivateHandle);
    if (0 > ret) {
        LOGE("Cannot Stop AF");
        return -1;
    } else {
        LOGE("3A FW AF Stop - success");
    }
    
    isStart_FW3A_AF = 0;

    LOG_FUNCTION_NAME_EXIT

    return prev;
}

int CameraHal::FW3A_DefaultSettings()
{
    CameraParameters p;

    LOG_FUNCTION_NAME

    p.setPreviewSize(MIN_WIDTH, MIN_HEIGHT);
    p.setPreviewFrameRate(15);
    p.setPreviewFormat("yuv422i");

    p.setPictureSize(MIN_WIDTH, MIN_HEIGHT);
    p.setPictureFormat("jpeg");

    if (setParameters(p) != NO_ERROR) {
        LOGE("Failed to set default parameters?!");
        return -1;
    }
   
    FW3A_GetSettings();

    LOG_FUNCTION_NAME_EXIT

    return 0;
}

int CameraHal::FW3A_GetSettings()
{
    int err = 0;

    LOG_FUNCTION_NAME

    err = fobj->cam_iface_2a->ReadSettings(fobj->cam_iface_2a->pPrivateHandle, &fobj->settings_2a);

    LOG_FUNCTION_NAME_EXIT  

    return err;
}

int CameraHal::FW3A_SetSettings()
{
    int err = 0;

    LOG_FUNCTION_NAME

//UGLY Hack
{
	int prev_exp = fobj->settings_2a.ae.exposure_time;

  fobj->settings_2a.ae.exposure_time = 33333;
  err = fobj->cam_iface_2a->WriteSettings(fobj->cam_iface_2a->pPrivateHandle, &fobj->settings_2a);
  fobj->settings_2a.ae.exposure_time = prev_exp;
}

    err = fobj->cam_iface_2a->WriteSettings(fobj->cam_iface_2a->pPrivateHandle, &fobj->settings_2a);
  
    LOG_FUNCTION_NAME_EXIT

    return err;
}
#endif





#ifdef IMAGE_PROCESSING_PIPELINE

static IPP_EENFAlgoDynamicParams IPPEENFAlgoDynamicParamsArray [MAXIPPDynamicParams] = {
 {sizeof(IPP_EENFAlgoDynamicParams), 0, 180, 18, 120, 8, 40}//2
};


int CameraHal::InitIPP(int w, int h)
{
    int eError = 0;

    pIPP.hIPP = IPP_Create();
    LOGD("IPP Handle: %p",pIPP.hIPP);

    pIPP.ippconfig = IPP_CONF_YUV422_INPLACE;
    
    pIPP.CRCBptr.size = sizeof(IPP_CRCBSAlgoCreateParams);
    pIPP.CRCBptr.maxWidth = w;
    pIPP.CRCBptr.maxHeight = h;
    pIPP.CRCBptr.errorCode = 0;
    
    pIPP.YUVCcreate.size = sizeof(IPP_YUVCAlgoCreateParams);
    pIPP.YUVCcreate.maxWidth = w;
    pIPP.YUVCcreate.maxHeight = h;
    pIPP.YUVCcreate.errorCode = 0;
    
    LOGD("IPP_SetProcessingConfiguration");
    IPP_SetProcessingConfiguration(pIPP.hIPP, pIPP.ippconfig);

    LOGD("IPP_SetAlgoConfig");
    IPP_SetAlgoConfig(pIPP.hIPP,
                      IPP_CRCBS_CREATEPRMS_CFGID,
                      &(pIPP.CRCBptr));
    
    LOGD("IPP_SetAlgoConfig");
    IPP_SetAlgoConfig(pIPP.hIPP,
                      IPP_YUVC_422TO420_CREATEPRMS_CFGID,
                      &(pIPP.YUVCcreate));
    
    LOGD("IPP_SetAlgoConfig");
    IPP_SetAlgoConfig(pIPP.hIPP,
                      IPP_YUVC_420TO422_CREATEPRMS_CFGID,
                      &(pIPP.YUVCcreate));
    
    LOGD("IPP_InitializeImagePipe");
    IPP_InitializeImagePipe(pIPP.hIPP);
    
    LOGD("IPP_StartProcessing");
    IPP_StartProcessing(pIPP.hIPP);
    
    pIPP.iStarInArgs = (IPP_StarAlgoInArgs*)((char*)malloc(sizeof(IPP_StarAlgoInArgs) + BUFF_MAP_PADDING_TEST) + PADDING_OFFSET_TEST);
    pIPP.iStarOutArgs = (IPP_StarAlgoOutArgs*)((char*)(malloc(sizeof(IPP_StarAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iCrcbsInArgs = (IPP_CRCBSAlgoInArgs*)((char*)(malloc(sizeof(IPP_CRCBSAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iCrcbsOutArgs = (IPP_CRCBSAlgoOutArgs*)((char*)(malloc(sizeof(IPP_CRCBSAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iEenfInArgs = (IPP_EENFAlgoInArgs*)((char*)(malloc(sizeof(IPP_EENFAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iEenfOutArgs = (IPP_EENFAlgoOutArgs*)((char*)(malloc(sizeof(IPP_EENFAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcInArgs1 = (IPP_YUVCAlgoInArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcOutArgs1 = (IPP_YUVCAlgoOutArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcInArgs2 = (IPP_YUVCAlgoInArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcOutArgs2 = (IPP_YUVCAlgoOutArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.dynEENF = (IPP_EENFAlgoDynamicParams*)((char*)(malloc(sizeof(IPP_EENFAlgoDynamicParams) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  * DeInitIPP() 
  *
  * 
  *
  * @param pComponentPrivate component private data structure.
  *
  * @retval OMX_ErrorNone       success, ready to roll
  *         OMX_ErrorHardware   if video driver API fails
  **/
/*-------------------------------------------------------------------*/
int CameraHal::DeInitIPP()
{
    int eError = 0;

    LOGD("IPP_StopProcessing");
    IPP_StopProcessing(pIPP.hIPP);

    LOGD("IPP_DeinitializePipe");
    IPP_DeinitializePipe(pIPP.hIPP);

    LOGD("IPP_Delete");
    IPP_Delete(&(pIPP.hIPP));
    
    free(((char*)pIPP.iStarInArgs - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iStarOutArgs - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iCrcbsInArgs - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iCrcbsOutArgs - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iEenfInArgs - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iEenfOutArgs - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcInArgs1 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcOutArgs1 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcInArgs2 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcOutArgs2 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.dynEENF - PADDING_OFFSET_TEST));
    
    LOGD("Terminating IPP");
    
    return eError;
}

int CameraHal::PopulateArgsIPP(int w, int h)
{
    int eError = 0;
    
    LOGD("IPP: PopulateArgs ENTER");
    pIPP.iStarInArgs->size = sizeof(IPP_StarAlgoInArgs);
    pIPP.iCrcbsInArgs->size = sizeof(IPP_CRCBSAlgoInArgs);
    pIPP.iEenfInArgs->size = sizeof(IPP_EENFAlgoInArgs);
    pIPP.iYuvcInArgs1->size = sizeof(IPP_YUVCAlgoInArgs);
    pIPP.iYuvcInArgs2->size = sizeof(IPP_YUVCAlgoInArgs);
    
    pIPP.iStarOutArgs->size = sizeof(IPP_StarAlgoOutArgs);
    pIPP.iCrcbsOutArgs->size = sizeof(IPP_CRCBSAlgoOutArgs);
    pIPP.iEenfOutArgs->size = sizeof(IPP_EENFAlgoOutArgs);
    pIPP.iYuvcOutArgs1->size = sizeof(IPP_YUVCAlgoOutArgs);
    pIPP.iYuvcOutArgs2->size = sizeof(IPP_YUVCAlgoOutArgs);
    
    pIPP.iCrcbsInArgs->inputHeight = h;
    pIPP.iCrcbsInArgs->inputWidth = w;
    pIPP.iCrcbsInArgs->inputChromaFormat = IPP_YUV_422P;

    pIPP.iEenfInArgs->inputChromaFormat = IPP_YUV_422P;
    pIPP.iEenfInArgs->inFullWidth = w;
    pIPP.iEenfInArgs->inFullHeight = h;
    pIPP.iEenfInArgs->inOffsetV = 0;
    pIPP.iEenfInArgs->inOffsetH = 0;
    pIPP.iEenfInArgs->inputWidth = w;
    pIPP.iEenfInArgs->inputHeight = h;
    pIPP.iEenfInArgs->inPlace = 0;
    pIPP.iEenfInArgs->NFprocessing = 0;
    
    pIPP.iYuvcInArgs1->inputHeight = h;
    pIPP.iYuvcInArgs1->inputWidth = w;
    pIPP.iYuvcInArgs1->outputChromaFormat = IPP_YUV_422P;
    pIPP.iYuvcInArgs1->inputChromaFormat = IPP_YUV_422ILE;
    
    pIPP.iYuvcInArgs2->inputHeight = h;
    pIPP.iYuvcInArgs2->inputWidth = w;
    pIPP.iYuvcInArgs2->outputChromaFormat = IPP_YUV_422ILE;
    pIPP.iYuvcInArgs2->inputChromaFormat = IPP_YUV_422P;
    
    pIPP.starStatus.size = sizeof(IPP_StarAlgoStatus);
    pIPP.CRCBSStatus.size = sizeof(IPP_CRCBSAlgoStatus);
    pIPP.EENFStatus.size = sizeof(IPP_EENFAlgoStatus);
    
    pIPP.statusDesc.statusPtr[0] = &(pIPP.starStatus);
    pIPP.statusDesc.statusPtr[1] = &(pIPP.CRCBSStatus);
    pIPP.statusDesc.statusPtr[2] = &(pIPP.EENFStatus);
    pIPP.statusDesc.numParams = 3;
    pIPP.statusDesc.algoNum[0] = 0;
    pIPP.statusDesc.algoNum[1] = 1;
    pIPP.statusDesc.algoNum[2] = 2;

    LOGD("IPP: PopulateArgs EXIT");
    
    return eError;
}


int CameraHal::ProcessBufferIPP(void *pBuffer, long int nAllocLen,
                                int EdgeEnhancementStrength, int WeakEdgeThreshold, 
                                int StrongEdgeThreshold, int LumaNoiseFilterStrength,
                                int ChromaNoiseFilterStrength)
{
    int eError = 0;

    IPPEENFAlgoDynamicParamsArray[0].inPlace                    = 0;
    IPPEENFAlgoDynamicParamsArray[0].EdgeEnhancementStrength    = EdgeEnhancementStrength;
    IPPEENFAlgoDynamicParamsArray[0].WeakEdgeThreshold          = WeakEdgeThreshold;
    IPPEENFAlgoDynamicParamsArray[0].StrongEdgeThreshold        = StrongEdgeThreshold;
    IPPEENFAlgoDynamicParamsArray[0].LumaNoiseFilterStrength    = LumaNoiseFilterStrength;
    IPPEENFAlgoDynamicParamsArray[0].ChromaNoiseFilterStrength  = ChromaNoiseFilterStrength;

    LOGD("Set EENF Dynamics Params:");
    LOGD("\tInPlace                      = %d", (int)IPPEENFAlgoDynamicParamsArray[0].inPlace);
    LOGD("\tEdge Enhancement Strength    = %d", (int)IPPEENFAlgoDynamicParamsArray[0].EdgeEnhancementStrength);
    LOGD("\tWeak Edge Threshold          = %d", (int)IPPEENFAlgoDynamicParamsArray[0].WeakEdgeThreshold);
    LOGD("\tStrong Edge Threshold        = %d", (int)IPPEENFAlgoDynamicParamsArray[0].StrongEdgeThreshold);
    LOGD("\tLuma Noise Filter Strength   = %d", (int)IPPEENFAlgoDynamicParamsArray[0].LumaNoiseFilterStrength );
    LOGD("\tChroma Noise Filter Strength = %d", (int)IPPEENFAlgoDynamicParamsArray[0].ChromaNoiseFilterStrength );
        
    /*Set Dynamic Parameter*/
    memcpy(pIPP.dynEENF,
           (void*)&IPPEENFAlgoDynamicParamsArray[0],
           sizeof(IPPEENFAlgoDynamicParamsArray[0]));

    LOGD("IPP_SetAlgoConfig");
    eError = IPP_SetAlgoConfig(pIPP.hIPP, IPP_EENF_DYNPRMS_CFGID, (void*)pIPP.dynEENF);
    if (eError)
        LOGE("IPP_SetAlgoConfig failed");
    else
        LOGD("IPP_SetAlgoConfig OK");

    pIPP.iInputBufferDesc.numBuffers = 1;
    pIPP.iInputBufferDesc.bufPtr[0] = pBuffer;
    pIPP.iInputBufferDesc.bufSize[0] = nAllocLen;
    pIPP.iInputBufferDesc.usedSize[0] = nAllocLen;
    pIPP.iInputBufferDesc.port[0] = 0;
    pIPP.iInputBufferDesc.reuseAllowed[0] = 0;

    pIPP.iOutputBufferDesc.numBuffers = 1;
    pIPP.iOutputBufferDesc.bufPtr[0] = pBuffer;
    pIPP.iOutputBufferDesc.bufSize[0] = nAllocLen;
    pIPP.iOutputBufferDesc.usedSize[0] = nAllocLen;
    pIPP.iOutputBufferDesc.port[0] = 1;
    pIPP.iOutputBufferDesc.reuseAllowed[0] = 0;
    
    pIPP.iInputArgs.numArgs = 5;
    pIPP.iInputArgs.argsArray[0] = pIPP.iStarInArgs;
    pIPP.iInputArgs.argsArray[1] = pIPP.iCrcbsInArgs;
    pIPP.iInputArgs.argsArray[2] = pIPP.iEenfInArgs;
    pIPP.iInputArgs.argsArray[3] = pIPP.iYuvcInArgs1;
    pIPP.iInputArgs.argsArray[4] = pIPP.iYuvcInArgs2;
    
    pIPP.iOutputArgs.numArgs = 5;
    pIPP.iOutputArgs.argsArray[0] = pIPP.iStarOutArgs;
    pIPP.iOutputArgs.argsArray[1] = pIPP.iCrcbsOutArgs;
    pIPP.iOutputArgs.argsArray[2] = pIPP.iEenfOutArgs;
    pIPP.iOutputArgs.argsArray[3] = pIPP.iYuvcOutArgs1;
    pIPP.iOutputArgs.argsArray[4] = pIPP.iYuvcOutArgs2;
    
    LOGD("IPP_ProcessImage");
    IPP_ProcessImage(pIPP.hIPP,
                     &(pIPP.iInputBufferDesc),
                     &(pIPP.iInputArgs),
                     NULL,
                     &(pIPP.iOutputArgs));
    LOGD("IPP_ProcessImage Done");
    
    return eError;
}

#endif

					/***************/

void CameraHal::drawRect(uint8_t *input, uint8_t color, int x1, int y1, int x2, int y2, int width, int height)
{
	int i, start_offset, end_offset ;

	//Top Line
	start_offset = ((width*y1) + x1)*2 + 1;
	end_offset =  start_offset + (x2 - x1)*2 + 1;
	for ( i = start_offset; i < end_offset; i += 2){
		*( input + i ) = color;	
	}

	//Left Line
	start_offset = ((width*y1) + x1)*2 + 1;
	end_offset = ((width*y2) + x1)*2 + 1;
	for ( i = start_offset; i < end_offset; i += 2*width){
		*( input + i ) = color;	
	}	

	//Botom Line
	start_offset = ((width*y2) + x1)*2 + 1;
	end_offset = start_offset + (x2 - x1)*2 + 1;
	for( i = start_offset; i < end_offset; i += 2){
		*( input + i ) = color;	
	}

	//Right Line
	start_offset = ((width*y1) + x1 + (x2 - x1))*2 + 1;
	end_offset = ((width*y2) + x1 + (x2 - x1))*2 + 1;
	for ( i = start_offset; i < end_offset; i += 2*width){
		*( input + i ) = color;	
	}
}

int CameraHal::ZoomPerform(int zoom)
{
    struct v4l2_crop crop;
    int scale[] = {111, 125, 142, 166, 200, 250, 333};
    int ret;

    if (zoom<1) zoom = 1;
    if (zoom>7) zoom = 7;
    
    zoom_width  = sensor_width * 1000 / (1000+(zoom-1)*200);
    zoom_width  = (zoom_width  + 31);
    zoom_width  &= ~31;
    zoom_height = (sensor_height * zoom_width) / sensor_width;

    LOGD("Perform ZOOM: x%d  sw=%5d, sh=%5d, zw=%d, zh=%5d, sx=%d, sy=%5d", 
        zoom, sensor_width, sensor_height, zoom_width, zoom_height,
        sensor_width*100/zoom_width, sensor_height*100/zoom_height);

    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c.left   = (sensor_width - zoom_width ) >> 1;
    crop.c.top    = (sensor_height- zoom_height) >> 1;
    crop.c.width  = zoom_width;
    crop.c.height = zoom_height;

    ret = ioctl(camera_device, VIDIOC_S_CROP, &crop);
    if (ret < 0) {
      LOGE("[%s]: ERROR VIDIOC_S_CROP failed\n", __func__);
      return -1;
    }

    return 0;
}


/************/
#ifndef ICAP
int CameraHal::CapturePicture(){

    int w, h;
    int pictureSize;
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

  LOG_FUNCTION_NAME

    if (mShutterCallback)
        mShutterCallback(mPictureCallbackCookie);

    mParameters.getPictureSize(&w, &h);

    LOGD("Picture Size: Width = %d \tHeight = %d", w, h);

#ifdef OPEN_CLOSE_WORKAROUND
    close(camera_device);
    camera_device = open(VIDEO_DEVICE, O_RDWR);
    if (camera_device < 0) {
          LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!",
         		  strerror(errno) );
  	}
#endif

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = w;
    format.fmt.pix.height = h;
    format.fmt.pix.pixelformat = PIXEL_FORMAT;

    /* set size & format of the video image */
    if (ioctl(camera_device, VIDIOC_S_FMT, &format) < 0){
        LOGE ("Failed to set VIDIOC_S_FMT.");
        return -1;
    }

    /* Check if the camera driver can accept 1 buffer */
    creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    creqbuf.memory = V4L2_MEMORY_USERPTR;
    creqbuf.count  = 1;
    if (ioctl(camera_device, VIDIOC_REQBUFS, &creqbuf) < 0){
        LOGE ("VIDIOC_REQBUFS Failed. errno = %d", errno);
        return -1;
    }

    pictureSize = w * h * 2;
    if (pictureSize & 0xfff)
    {
        pictureSize = (pictureSize & 0xfffff000) + 0x1000;
    }
    LOGD("pictureFrameSize = 0x%x = %d", pictureSize, pictureSize);

    // Make a new mmap'ed heap that can be shared across processes.
    mPictureHeap = new MemoryHeapBase(pictureSize + 0x20 + 256);
    base = (unsigned long)mPictureHeap->getBase();

    /*Align buffer to 32 byte boundary */
    while ((base & 0x1f) != 0)
    {
        base++;
    }

    /* Buffer pointer shifted to avoid DSP cache issues */
    base += 128;
    offset = base - (unsigned long)mPictureHeap->getBase();
    mPictureBuffer = new MemoryBase(mPictureHeap, offset, pictureSize);

    LOGD("Picture Buffer: Base = %p Offset = 0x%x", (void *)base, (unsigned int)offset);

    buffer.type = creqbuf.type;
    buffer.memory = creqbuf.memory;
    buffer.index = 0;

    if (ioctl(camera_device, VIDIOC_QUERYBUF, &buffer) < 0) {
        LOGE("VIDIOC_QUERYBUF Failed");
        return -1;
    }

    buffer.length = pictureSize;
    buffer.m.userptr = (unsigned long) (mPictureHeap->getBase()) + offset;

    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);	
	
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
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;
    while (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
        LOGE("VIDIOC_DQBUF Failed");
    }

    LOGD("pictureThread: generated a picture");

    /* turn off streaming */
    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) < 0) {
        LOGE("VIDIOC_STREAMON Failed");
        return -1;
    }

    if (mRawPictureCallback) {
        mRawPictureCallback(mPictureBuffer, mPictureCallbackCookie);
    }

#ifdef OPEN_CLOSE_WORKAROUND
    close(camera_device);
    camera_device = open(VIDEO_DEVICE, O_RDWR);
    if (camera_device < 0) {
          LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!",
         		  strerror(errno) );
  	}
#endif


    if (mJpegPictureCallback) {

#ifdef HARDWARE_OMX  

        int jpegSize = (w * h) + 12288;
        mJPEGPictureHeap = new MemoryHeapBase(jpegSize+ 256);
        mJPEGPictureMemBase = new MemoryBase(mJPEGPictureHeap, 128, jpegSize);
        outBuffer = (void *)((unsigned long)(mJPEGPictureHeap->getBase()) + 128);

        jpegEncoder->encodeImage(outBuffer, jpegSize, (void*)(cfilledbuffer.m.userptr), cfilledbuffer.length, w, h, quality);

        if(mJpegPictureCallback) {
            mJpegPictureCallback(mJPEGPictureMemBase, mPictureCallbackCookie); 
        }

        mJPEGPictureMemBase.clear();		
        mJPEGPictureHeap.clear();

#endif

        gettimeofday(&tv2, NULL);
        LOGD("Shot to Encode Latency: %ld %ld %ld %ld\n", tv1.tv_sec, tv1.tv_usec, tv2.tv_sec, tv2.tv_usec);
    }

    mPictureBuffer.clear();
    mPictureHeap.clear();
    return NO_ERROR;

}
#endif

};





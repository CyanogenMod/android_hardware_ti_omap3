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

#define LOG_TAG "CameraHalUtils"

#include "CameraHal.h"
#include "zoom_step.inc"

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

    ancillary_buffer = (uint8_t *) malloc(4096);
    if (!ancillary_buffer) {
        LOGE("cannot alloc ancillary_buffer");
        goto exit;
    }
    memset(ancillary_buffer, 0, 4096);

    LOGE("dlopen MMS 3a Library Enter");
  	fobj->lib.lib_handle = dlopen(LIB3AFW, RTLD_LAZY);
  	LOGE("dlopen MMS 3a Library Exit");
    if (NULL == fobj->lib.lib_handle) {
		LOGE("A dynamic linking error occurred: (%s)", dlerror());
        LOGE("ERROR - at dlopen for %s", LIB3AFW);
        goto exit;
    }

    /* get Create2A symbol */
    fobj->lib.Create2A = (int (*)(Camera2AInterface**)) dlsym(fobj->lib.lib_handle, "Create2A");
    if (NULL == fobj->lib.Create2A) {
        LOGE("ERROR - can't get dlsym \"Create2A\"");
        goto exit;
    }
 
    /* get Destroy2A symbol */
    fobj->lib.Destroy2A = (int (*)(Camera2AInterface**)) dlsym(fobj->lib.lib_handle, "Destroy2A");
    if (NULL == fobj->lib.Destroy2A) {
        LOGE("ERROR - can't get dlsym \"Destroy2A\"");
        goto exit;
    }

    /* get Init2A symbol */
    LOGE("dlsym MMS 3a Init2A Enter");
    fobj->lib.Init2A = (int (*)(Camera2AInterface*, int, uint8)) dlsym(fobj->lib.lib_handle, "Init2A");
    LOGE("dlsym MMS 3a Init2A Exit");
    if (NULL == fobj->lib.Init2A) {
        LOGE("ERROR - can't get dlsym \"Init2A\"");
        goto exit;
    }

    /* get Release2A symbol */
    LOGE("dlsym MMS 3a Release2A Enter");
    fobj->lib.Release2A = (int (*)(Camera2AInterface*)) dlsym(fobj->lib.lib_handle, "Release2A");
    LOGE("dlsym MMS 3a Release2A Exit");
    if (NULL == fobj->lib.Release2A) {
       LOGE( "ERROR - can't get dlsym \"Release2A\"");
       goto exit;
    }

    fobj->cam_dev = 0;
    /* Create 2A framework */
    err = fobj->lib.Create2A(&fobj->cam_iface_2a);
    if (err < 0) {
        LOGE("Can't Create2A");
        goto exit;
    }

    LOGD("FW3A Create - %d   fobj=%p", err, fobj);

    LOG_FUNCTION_NAME_EXIT

    return 0;

exit:
    LOGD("Can't create 3A FW");
    return -1;
}

int CameraHal::FW3A_Init()
{
    int err = 0;

    LOG_FUNCTION_NAME

    err = fobj->lib.Init2A(fobj->cam_iface_2a, fobj->cam_dev, 0);
    if (err < 0) {
        LOGE("Can't Init2A, will try to release first");
        err = fobj->lib.Release2A(fobj->cam_iface_2a);
        if (!err) {
            err = fobj->lib.Init2A(fobj->cam_iface_2a, fobj->cam_dev, 0);
            if (err < 0) {
                LOGE("Can't Init2A");

                err = fobj->lib.Destroy2A(&fobj->cam_iface_2a);
                goto exit;
            }
        }
    }

    LOG_FUNCTION_NAME_EXIT

    return 0;

exit:

    return -1;
}

int CameraHal::FW3A_Release()
{
    int ret;

    LOG_FUNCTION_NAME

    ret = fobj->lib.Release2A(fobj->cam_iface_2a);
    if (ret < 0) {
        LOGE("Cannot Release2A");
        goto exit;
    } else {
        LOGD("2A released");
    }

    LOG_FUNCTION_NAME_EXIT

    return 0;

exit:

    return -1;
}

int CameraHal::FW3A_Destroy()
{
    int ret;

    LOG_FUNCTION_NAME

    ret = fobj->lib.Destroy2A(&fobj->cam_iface_2a);
    if (ret < 0) {
        LOGE("Cannot Destroy2A");
    } else {
        LOGD("2A destroyed");
    }

   	dlclose(fobj->lib.lib_handle);
  	free(fobj);
  	free(ancillary_buffer);
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

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME

#endif

    if (isStart_FW3A==0) {
        LOGE("3A FW is already stopped");
        return -1;
    }

    //Stop 3AFW
    ret = fobj->cam_iface_2a->Stop2A(fobj->cam_iface_2a->pPrivateHandle);
    if (0 > ret) {
        LOGE("Cannot Stop 3A");
        return -1;
    }

#ifdef DEBUG_LOG
     else {
        LOGE("3A FW Stop - success");
    }
#endif

    isStart_FW3A = 0;

#ifdef DEBUG_LOG
    LOG_FUNCTION_NAME_EXIT
#endif

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

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME

#endif

    if (isStart_FW3A_CAF==0) {
        LOGE("3A FW CAF is already stopped");
        return prev;
    }

    ret = fobj->cam_iface_2a->StopAF(fobj->cam_iface_2a->pPrivateHandle);
    if (0 > ret) {
        LOGE("Cannot Stop CAF");
        return -1;
    }

#ifdef DEBUG_LOG
     else {
        LOGE("3A FW CAF Start - success");
    }
#endif

    isStart_FW3A_CAF = 0;

#ifdef DEBUG_LOG
    LOG_FUNCTION_NAME_EXIT
#endif

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

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME

#endif

    if (isStart_FW3A_AF == 0) {
        LOGE("3A FW AF is already stopped");
        return isStart_FW3A_AF;
    }

    //Stop 3AFW
    ret = fobj->cam_iface_2a->StopAF(fobj->cam_iface_2a->pPrivateHandle);
    if (0 > ret) {
        LOGE("Cannot Stop AF");
        return -1;
    }

#ifdef DEBUG_LOG
    else {
        LOGE("3A FW AF Stop - success");
    }
#endif

    isStart_FW3A_AF = 0;

#ifdef DEBUG_LOG
    LOG_FUNCTION_NAME_EXIT
#endif
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

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME

#endif

    err = fobj->cam_iface_2a->ReadSettings(fobj->cam_iface_2a->pPrivateHandle, &fobj->settings_2a);

#ifdef DEBUG_LOG

    LOG_FUNCTION_NAME_EXIT  

#endif

    return err;
}

int CameraHal::FW3A_SetSettings()
{
    int err = 0;

    LOG_FUNCTION_NAME

    err = fobj->cam_iface_2a->WriteSettings(fobj->cam_iface_2a->pPrivateHandle, &fobj->settings_2a);
  
    LOG_FUNCTION_NAME_EXIT

    return err;
}
#endif

#ifdef IMAGE_PROCESSING_PIPELINE

int CameraHal::InitIPP(int w, int h, int fmt, int ippMode)
{
    int eError = 0;

	if( (ippMode != IPP_CromaSupression_Mode) && (ippMode != IPP_EdgeEnhancement_Mode) ){
		LOGE("Error unsupported mode=%d", ippMode);
		return -1;
	}

    pIPP.hIPP = IPP_Create();
    LOGD("IPP Handle: %p",pIPP.hIPP);

	if( !pIPP.hIPP ){
		LOGE("ERROR IPP_Create");
		return -1;
	}

    if( fmt == PIX_YUV420P)
	    pIPP.ippconfig.numberOfAlgos = 3;
	else
	    pIPP.ippconfig.numberOfAlgos = 4;

    pIPP.ippconfig.orderOfAlgos[0]=IPP_START_ID;

    if( fmt != PIX_YUV420P ){
        pIPP.ippconfig.orderOfAlgos[1]=IPP_YUVC_422iTO422p_ID;

	    if(ippMode == IPP_CromaSupression_Mode ){
		    pIPP.ippconfig.orderOfAlgos[2]=IPP_CRCBS_ID;
	    }
	    else if(ippMode == IPP_EdgeEnhancement_Mode){
		    pIPP.ippconfig.orderOfAlgos[2]=IPP_EENF_ID;
	    }

        pIPP.ippconfig.orderOfAlgos[3]=IPP_YUVC_422pTO422i_ID;
	} else {
	    if(ippMode == IPP_CromaSupression_Mode ){
		    pIPP.ippconfig.orderOfAlgos[1]=IPP_CRCBS_ID;
	    }
	    else if(ippMode == IPP_EdgeEnhancement_Mode){
		    pIPP.ippconfig.orderOfAlgos[1]=IPP_EENF_ID;
	    }

        pIPP.ippconfig.orderOfAlgos[3]=IPP_YUVC_422pTO422i_ID;
	}

    pIPP.ippconfig.isINPLACE=INPLACE_ON;
	pIPP.outputBufferSize= (w*h*2);

    LOGD("IPP_SetProcessingConfiguration");
    eError = IPP_SetProcessingConfiguration(pIPP.hIPP, pIPP.ippconfig);
	if(eError != 0){
		LOGE("ERROR IPP_SetProcessingConfiguration");
	}	
    
	if(ippMode == IPP_CromaSupression_Mode ){
		pIPP.CRCBptr.size = sizeof(IPP_CRCBSAlgoCreateParams);
		pIPP.CRCBptr.maxWidth = w;
		pIPP.CRCBptr.maxHeight = h;
		pIPP.CRCBptr.errorCode = 0;
	}
  
    pIPP.YUVCcreate.size = sizeof(IPP_YUVCAlgoCreateParams);
    pIPP.YUVCcreate.maxWidth = w;
    pIPP.YUVCcreate.maxHeight = h;
    pIPP.YUVCcreate.errorCode = 0;

	if(ippMode == IPP_CromaSupression_Mode ){
	    LOGD("IPP_SetAlgoConfig");
	    eError = IPP_SetAlgoConfig(pIPP.hIPP, IPP_CRCBS_CREATEPRMS_CFGID, &(pIPP.CRCBptr));
		if(eError != 0){
			LOGE("ERROR IPP_SetAlgoConfig");
		}	
	}
    
    if ( fmt != PIX_YUV420P ) {
        LOGD("IPP_SetAlgoConfig: IPP_YUVC_422TO420_CREATEPRMS_CFGID");
        eError = IPP_SetAlgoConfig(pIPP.hIPP, IPP_YUVC_422TO420_CREATEPRMS_CFGID, &(pIPP.YUVCcreate));
	    if(eError != 0){
		    LOGE("ERROR IPP_SetAlgoConfig: IPP_YUVC_422TO420_CREATEPRMS_CFGID");
	    }
	}

    LOGD("IPP_SetAlgoConfig: IPP_YUVC_420TO422_CREATEPRMS_CFGID");
    eError = IPP_SetAlgoConfig(pIPP.hIPP, IPP_YUVC_420TO422_CREATEPRMS_CFGID, &(pIPP.YUVCcreate));
    if(eError != 0){
        LOGE("ERROR IPP_SetAlgoConfig: IPP_YUVC_420TO422_CREATEPRMS_CFGID");
    }

    LOGD("IPP_InitializeImagePipe");
    eError = IPP_InitializeImagePipe(pIPP.hIPP);
	if(eError != 0){
		LOGE("ERROR IPP_InitializeImagePipe");
	} else {
        mIPPInitAlgoState = true;
    }

    pIPP.iStarInArgs = (IPP_StarAlgoInArgs*)((char*)malloc(sizeof(IPP_StarAlgoInArgs) + BUFF_MAP_PADDING_TEST) + PADDING_OFFSET_TEST);
    pIPP.iStarOutArgs = (IPP_StarAlgoOutArgs*)((char*)(malloc(sizeof(IPP_StarAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);

	if(ippMode == IPP_CromaSupression_Mode ){
    	pIPP.iCrcbsInArgs = (IPP_CRCBSAlgoInArgs*)((char*)(malloc(sizeof(IPP_CRCBSAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    	pIPP.iCrcbsOutArgs = (IPP_CRCBSAlgoOutArgs*)((char*)(malloc(sizeof(IPP_CRCBSAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
	}

	if(ippMode == IPP_EdgeEnhancement_Mode){
    	pIPP.iEenfInArgs = (IPP_EENFAlgoInArgs*)((char*)(malloc(sizeof(IPP_EENFAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
	    pIPP.iEenfOutArgs = (IPP_EENFAlgoOutArgs*)((char*)(malloc(sizeof(IPP_EENFAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
	}

    pIPP.iYuvcInArgs1 = (IPP_YUVCAlgoInArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcOutArgs1 = (IPP_YUVCAlgoOutArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcInArgs2 = (IPP_YUVCAlgoInArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcOutArgs2 = (IPP_YUVCAlgoOutArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);

	if(ippMode == IPP_EdgeEnhancement_Mode){
    	pIPP.dynEENF = (IPP_EENFAlgoDynamicParams*)((char*)(malloc(sizeof(IPP_EENFAlgoDynamicParams) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
	}

	if( !(pIPP.ippconfig.isINPLACE) ){
		pIPP.pIppOutputBuffer= (unsigned char*)malloc(pIPP.outputBufferSize + BUFF_MAP_PADDING_TEST) + PADDING_OFFSET_TEST ; // TODO make it dependent on the output format
	}
    
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
int CameraHal::DeInitIPP(int ippMode)
{
    int eError = 0;

    if(mIPPInitAlgoState){
        LOGD("IPP_DeinitializePipe");
        eError = IPP_DeinitializePipe(pIPP.hIPP);
        LOGD("IPP_DeinitializePipe");
        if( eError != 0){
            LOGE("ERROR IPP_DeinitializePipe");
        }
        mIPPInitAlgoState = false;
    }

    LOGD("IPP_Delete");
    IPP_Delete(&(pIPP.hIPP));

    free(((char*)pIPP.iStarInArgs - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iStarOutArgs - PADDING_OFFSET_TEST));
	
	if(ippMode == IPP_CromaSupression_Mode ){
    	free(((char*)pIPP.iCrcbsInArgs - PADDING_OFFSET_TEST));
	    free(((char*)pIPP.iCrcbsOutArgs - PADDING_OFFSET_TEST));
	}

	if(ippMode == IPP_EdgeEnhancement_Mode){
	    free(((char*)pIPP.iEenfInArgs - PADDING_OFFSET_TEST));
    	free(((char*)pIPP.iEenfOutArgs - PADDING_OFFSET_TEST));
	}

    free(((char*)pIPP.iYuvcInArgs1 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcOutArgs1 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcInArgs2 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcOutArgs2 - PADDING_OFFSET_TEST));
	
	if(ippMode == IPP_EdgeEnhancement_Mode){
	    free(((char*)pIPP.dynEENF - PADDING_OFFSET_TEST));
	}

	if(!(pIPP.ippconfig.isINPLACE)){
		free(pIPP.pIppOutputBuffer - PADDING_OFFSET_TEST);
	}

    LOGD("Terminating IPP");
    
    return eError;
}

int CameraHal::PopulateArgsIPP(int w, int h, int fmt, int ippMode)
{
    int eError = 0;
    
    LOGD("IPP: PopulateArgs ENTER");

	//configuring size of input and output structures
    pIPP.iStarInArgs->size = sizeof(IPP_StarAlgoInArgs);
	if(ippMode == IPP_CromaSupression_Mode ){
	    pIPP.iCrcbsInArgs->size = sizeof(IPP_CRCBSAlgoInArgs);
	}
	if(ippMode == IPP_EdgeEnhancement_Mode){
	    pIPP.iEenfInArgs->size = sizeof(IPP_EENFAlgoInArgs);
	}
	
    pIPP.iYuvcInArgs1->size = sizeof(IPP_YUVCAlgoInArgs);
    pIPP.iYuvcInArgs2->size = sizeof(IPP_YUVCAlgoInArgs);

    pIPP.iStarOutArgs->size = sizeof(IPP_StarAlgoOutArgs);
	if(ippMode == IPP_CromaSupression_Mode ){
    	pIPP.iCrcbsOutArgs->size = sizeof(IPP_CRCBSAlgoOutArgs);
	}
	if(ippMode == IPP_EdgeEnhancement_Mode){
    	pIPP.iEenfOutArgs->size = sizeof(IPP_EENFAlgoOutArgs);
	}
	
    pIPP.iYuvcOutArgs1->size = sizeof(IPP_YUVCAlgoOutArgs);
    pIPP.iYuvcOutArgs2->size = sizeof(IPP_YUVCAlgoOutArgs);
    
	//Configuring specific data of algorithms
	if(ippMode == IPP_CromaSupression_Mode ){
	    pIPP.iCrcbsInArgs->inputHeight = h;
	    pIPP.iCrcbsInArgs->inputWidth = w;	
	    pIPP.iCrcbsInArgs->inputChromaFormat = IPP_YUV_420P;
	}

	if(ippMode == IPP_EdgeEnhancement_Mode){
		pIPP.iEenfInArgs->inputChromaFormat = IPP_YUV_420P;
		pIPP.iEenfInArgs->inFullWidth = w;
		pIPP.iEenfInArgs->inFullHeight = h;
		pIPP.iEenfInArgs->inOffsetV = 0;
		pIPP.iEenfInArgs->inOffsetH = 0;
		pIPP.iEenfInArgs->inputWidth = w;
		pIPP.iEenfInArgs->inputHeight = h;
		pIPP.iEenfInArgs->inPlace = 0;
		pIPP.iEenfInArgs->NFprocessing = 0;
    }

    if ( fmt == PIX_YUV422I ) {
        pIPP.iYuvcInArgs1->inputChromaFormat = IPP_YUV_422ILE;
        pIPP.iYuvcInArgs1->outputChromaFormat = IPP_YUV_420P;
        pIPP.iYuvcInArgs1->inputHeight = h;
        pIPP.iYuvcInArgs1->inputWidth = w;
    }

    pIPP.iYuvcInArgs2->inputChromaFormat = IPP_YUV_420P;
    pIPP.iYuvcInArgs2->outputChromaFormat = IPP_YUV_422ILE;
    pIPP.iYuvcInArgs2->inputHeight = h;
    pIPP.iYuvcInArgs2->inputWidth = w;

	pIPP.iStarOutArgs->extendedError= 0;
	pIPP.iYuvcOutArgs1->extendedError = 0;
	pIPP.iYuvcOutArgs2->extendedError = 0;
	if(ippMode == IPP_EdgeEnhancement_Mode)
		pIPP.iEenfOutArgs->extendedError = 0;
	if(ippMode == IPP_CromaSupression_Mode )
		pIPP.iCrcbsOutArgs->extendedError = 0;

	//Filling ipp status structure
    pIPP.starStatus.size = sizeof(IPP_StarAlgoStatus);
	if(ippMode == IPP_CromaSupression_Mode ){
    	pIPP.CRCBSStatus.size = sizeof(IPP_CRCBSAlgoStatus);
	}
	if(ippMode == IPP_EdgeEnhancement_Mode){
    	pIPP.EENFStatus.size = sizeof(IPP_EENFAlgoStatus);
	}

    pIPP.statusDesc.statusPtr[0] = &(pIPP.starStatus);
	if(ippMode == IPP_CromaSupression_Mode ){
	    pIPP.statusDesc.statusPtr[1] = &(pIPP.CRCBSStatus);
	}
	if(ippMode == IPP_EdgeEnhancement_Mode){
	    pIPP.statusDesc.statusPtr[1] = &(pIPP.EENFStatus);
	}
	
    pIPP.statusDesc.numParams = 2;
    pIPP.statusDesc.algoNum[0] = 0;
    pIPP.statusDesc.algoNum[1] = 1;
    pIPP.statusDesc.algoNum[1] = 1;

    LOGD("IPP: PopulateArgs EXIT");
    
    return eError;
}

int CameraHal::ProcessBufferIPP(void *pBuffer, long int nAllocLen, int fmt, int ippMode,
                               int EdgeEnhancementStrength, int WeakEdgeThreshold, int StrongEdgeThreshold,
                                int LowFreqLumaNoiseFilterStrength, int MidFreqLumaNoiseFilterStrength, int HighFreqLumaNoiseFilterStrength,
                                int LowFreqCbNoiseFilterStrength, int MidFreqCbNoiseFilterStrength, int HighFreqCbNoiseFilterStrength,
                                int LowFreqCrNoiseFilterStrength, int MidFreqCrNoiseFilterStrength, int HighFreqCrNoiseFilterStrength,
                                int shadingVertParam1, int shadingVertParam2, int shadingHorzParam1, int shadingHorzParam2,
                                int shadingGainScale, int shadingGainOffset, int shadingGainMaxValue,
                                int ratioDownsampleCbCr)
{
    int eError = 0;
    IPP_EENFAlgoDynamicParams IPPEENFAlgoDynamicParamsCfg =
    {
        sizeof(IPP_EENFAlgoDynamicParams),
        0,//  inPlace
        150,//  EdgeEnhancementStrength;
        100,//  WeakEdgeThreshold;
        300,//  StrongEdgeThreshold;
        30,//  LowFreqLumaNoiseFilterStrength;
        80,//  MidFreqLumaNoiseFilterStrength;
        20,//  HighFreqLumaNoiseFilterStrength;
        60,//  LowFreqCbNoiseFilterStrength;
        40,//  MidFreqCbNoiseFilterStrength;
        30,//  HighFreqCbNoiseFilterStrength;
        50,//  LowFreqCrNoiseFilterStrength;
        30,//  MidFreqCrNoiseFilterStrength;
        20,//  HighFreqCrNoiseFilterStrength;
        1,//  shadingVertParam1;
        800,//  shadingVertParam2;
        1,//  shadingHorzParam1;
        800,//  shadingHorzParam2;
        128,//  shadingGainScale;
        4096,//  shadingGainOffset;
        24576,//  shadingGainMaxValue;
        1//  ratioDownsampleCbCr;
    };//2

    LOGD("IPP_StartProcessing");
    eError = IPP_StartProcessing(pIPP.hIPP);
    if(eError != 0){
        LOGE("ERROR IPP_SetAlgoConfig");
    }

	if(ippMode == IPP_EdgeEnhancement_Mode){
       IPPEENFAlgoDynamicParamsCfg.inPlace = 0;
        NONNEG_ASSIGN(EdgeEnhancementStrength, IPPEENFAlgoDynamicParamsCfg.EdgeEnhancementStrength);
        NONNEG_ASSIGN(WeakEdgeThreshold, IPPEENFAlgoDynamicParamsCfg.WeakEdgeThreshold);
        NONNEG_ASSIGN(StrongEdgeThreshold, IPPEENFAlgoDynamicParamsCfg.StrongEdgeThreshold);
        NONNEG_ASSIGN(LowFreqLumaNoiseFilterStrength, IPPEENFAlgoDynamicParamsCfg.LowFreqLumaNoiseFilterStrength);
        NONNEG_ASSIGN(MidFreqLumaNoiseFilterStrength, IPPEENFAlgoDynamicParamsCfg.MidFreqLumaNoiseFilterStrength);
        NONNEG_ASSIGN(HighFreqLumaNoiseFilterStrength, IPPEENFAlgoDynamicParamsCfg.HighFreqLumaNoiseFilterStrength);
        NONNEG_ASSIGN(LowFreqCbNoiseFilterStrength, IPPEENFAlgoDynamicParamsCfg.LowFreqCbNoiseFilterStrength);
        NONNEG_ASSIGN(MidFreqCbNoiseFilterStrength, IPPEENFAlgoDynamicParamsCfg.MidFreqCbNoiseFilterStrength);
        NONNEG_ASSIGN(HighFreqCbNoiseFilterStrength, IPPEENFAlgoDynamicParamsCfg.HighFreqCbNoiseFilterStrength);
        NONNEG_ASSIGN(LowFreqCrNoiseFilterStrength, IPPEENFAlgoDynamicParamsCfg.LowFreqCrNoiseFilterStrength);
        NONNEG_ASSIGN(MidFreqCrNoiseFilterStrength, IPPEENFAlgoDynamicParamsCfg.MidFreqCrNoiseFilterStrength);
        NONNEG_ASSIGN(HighFreqCrNoiseFilterStrength, IPPEENFAlgoDynamicParamsCfg.HighFreqCrNoiseFilterStrength);
        NONNEG_ASSIGN(shadingVertParam1, IPPEENFAlgoDynamicParamsCfg.shadingVertParam1);
        NONNEG_ASSIGN(shadingVertParam2, IPPEENFAlgoDynamicParamsCfg.shadingVertParam2);
        NONNEG_ASSIGN(shadingHorzParam1, IPPEENFAlgoDynamicParamsCfg.shadingHorzParam1);
        NONNEG_ASSIGN(shadingHorzParam2, IPPEENFAlgoDynamicParamsCfg.shadingHorzParam2);
        NONNEG_ASSIGN(shadingGainScale, IPPEENFAlgoDynamicParamsCfg.shadingGainScale);
        NONNEG_ASSIGN(shadingGainOffset, IPPEENFAlgoDynamicParamsCfg.shadingGainOffset);
        NONNEG_ASSIGN(shadingGainMaxValue, IPPEENFAlgoDynamicParamsCfg.shadingGainMaxValue);
        NONNEG_ASSIGN(ratioDownsampleCbCr, IPPEENFAlgoDynamicParamsCfg.ratioDownsampleCbCr);

        LOGD("Set EENF Dynamics Params:");
        LOGD("\tInPlace                      = %d", (int)IPPEENFAlgoDynamicParamsCfg.inPlace);
        LOGD("\tEdge Enhancement Strength    = %d", (int)IPPEENFAlgoDynamicParamsCfg.EdgeEnhancementStrength);
        LOGD("\tWeak Edge Threshold          = %d", (int)IPPEENFAlgoDynamicParamsCfg.WeakEdgeThreshold);
        LOGD("\tStrong Edge Threshold        = %d", (int)IPPEENFAlgoDynamicParamsCfg.StrongEdgeThreshold);
        LOGD("\tLuma Noise Filter Low Freq Strength   = %d", (int)IPPEENFAlgoDynamicParamsCfg.LowFreqLumaNoiseFilterStrength );
        LOGD("\tLuma Noise Filter Mid Freq Strength   = %d", (int)IPPEENFAlgoDynamicParamsCfg.MidFreqLumaNoiseFilterStrength );
        LOGD("\tLuma Noise Filter High Freq Strength   = %d", (int)IPPEENFAlgoDynamicParamsCfg.HighFreqLumaNoiseFilterStrength );
        LOGD("\tChroma Noise Filter Low Freq Cb Strength = %d", (int)IPPEENFAlgoDynamicParamsCfg.LowFreqCbNoiseFilterStrength);
        LOGD("\tChroma Noise Filter Mid Freq Cb Strength = %d", (int)IPPEENFAlgoDynamicParamsCfg.MidFreqCbNoiseFilterStrength);
        LOGD("\tChroma Noise Filter High Freq Cb Strength = %d", (int)IPPEENFAlgoDynamicParamsCfg.HighFreqCbNoiseFilterStrength);
        LOGD("\tChroma Noise Filter Low Freq Cr Strength = %d", (int)IPPEENFAlgoDynamicParamsCfg.LowFreqCrNoiseFilterStrength);
        LOGD("\tChroma Noise Filter Mid Freq Cr Strength = %d", (int)IPPEENFAlgoDynamicParamsCfg.MidFreqCrNoiseFilterStrength);
        LOGD("\tChroma Noise Filter High Freq Cr Strength = %d", (int)IPPEENFAlgoDynamicParamsCfg.HighFreqCrNoiseFilterStrength);
        LOGD("\tShading Vert 1 = %d", (int)IPPEENFAlgoDynamicParamsCfg.shadingVertParam1);
        LOGD("\tShading Vert 2 = %d", (int)IPPEENFAlgoDynamicParamsCfg.shadingVertParam2);
        LOGD("\tShading Horz 1 = %d", (int)IPPEENFAlgoDynamicParamsCfg.shadingHorzParam1);
        LOGD("\tShading Horz 2 = %d", (int)IPPEENFAlgoDynamicParamsCfg.shadingHorzParam2);
        LOGD("\tShading Gain Scale = %d", (int)IPPEENFAlgoDynamicParamsCfg.shadingGainScale);
        LOGD("\tShading Gain Offset = %d", (int)IPPEENFAlgoDynamicParamsCfg.shadingGainOffset);
        LOGD("\tShading Gain Max Val = %d", (int)IPPEENFAlgoDynamicParamsCfg.shadingGainMaxValue);
        LOGD("\tRatio Downsample CbCr = %d", (int)IPPEENFAlgoDynamicParamsCfg.ratioDownsampleCbCr);

		    
		/*Set Dynamic Parameter*/
		memcpy(pIPP.dynEENF,
		       (void*)&IPPEENFAlgoDynamicParamsCfg,
		       sizeof(IPPEENFAlgoDynamicParamsCfg));

	
		LOGD("IPP_SetAlgoConfig");
		eError = IPP_SetAlgoConfig(pIPP.hIPP, IPP_EENF_DYNPRMS_CFGID, (void*)pIPP.dynEENF);
		if( eError != 0){
			LOGE("ERROR IPP_SetAlgoConfig");
		}
	}

    pIPP.iInputBufferDesc.numBuffers = 1;
    pIPP.iInputBufferDesc.bufPtr[0] = pBuffer;
    pIPP.iInputBufferDesc.bufSize[0] = nAllocLen;
    pIPP.iInputBufferDesc.usedSize[0] = nAllocLen;
    pIPP.iInputBufferDesc.port[0] = 0;
    pIPP.iInputBufferDesc.reuseAllowed[0] = 0;

	if(!(pIPP.ippconfig.isINPLACE)){
		pIPP.iOutputBufferDesc.numBuffers = 1;
		pIPP.iOutputBufferDesc.bufPtr[0] = pIPP.pIppOutputBuffer;						/*TODO, depend on pix format*/
		pIPP.iOutputBufferDesc.bufSize[0] = pIPP.outputBufferSize; 
		pIPP.iOutputBufferDesc.usedSize[0] = pIPP.outputBufferSize;
		pIPP.iOutputBufferDesc.port[0] = 1;
		pIPP.iOutputBufferDesc.reuseAllowed[0] = 0;
	}

    if ( fmt == PIX_YUV422I){
	    pIPP.iInputArgs.numArgs = 4;
	    pIPP.iOutputArgs.numArgs = 4;

        pIPP.iInputArgs.argsArray[0] = pIPP.iStarInArgs;
        pIPP.iInputArgs.argsArray[1] = pIPP.iYuvcInArgs1;
	    if(ippMode == IPP_CromaSupression_Mode ){
	        pIPP.iInputArgs.argsArray[2] = pIPP.iCrcbsInArgs;
	    }
	    if(ippMode == IPP_EdgeEnhancement_Mode){
		    pIPP.iInputArgs.argsArray[2] = pIPP.iEenfInArgs;
	    }
        pIPP.iInputArgs.argsArray[3] = pIPP.iYuvcInArgs2;

        pIPP.iOutputArgs.argsArray[0] = pIPP.iStarOutArgs;
        pIPP.iOutputArgs.argsArray[1] = pIPP.iYuvcOutArgs1;
        if(ippMode == IPP_CromaSupression_Mode ){
            pIPP.iOutputArgs.argsArray[2] = pIPP.iCrcbsOutArgs;
        }
        if(ippMode == IPP_EdgeEnhancement_Mode){
            pIPP.iOutputArgs.argsArray[2] = pIPP.iEenfOutArgs;
        }
        pIPP.iOutputArgs.argsArray[3] = pIPP.iYuvcOutArgs2;
	 } else {
        pIPP.iInputArgs.numArgs = 3;
        pIPP.iOutputArgs.numArgs = 3;

        pIPP.iInputArgs.argsArray[0] = pIPP.iStarInArgs;
        if(ippMode == IPP_CromaSupression_Mode ){
            pIPP.iInputArgs.argsArray[1] = pIPP.iCrcbsInArgs;
        }
        if(ippMode == IPP_EdgeEnhancement_Mode){
            pIPP.iInputArgs.argsArray[1] = pIPP.iEenfInArgs;
        }
        pIPP.iInputArgs.argsArray[2] = pIPP.iYuvcInArgs2;

        pIPP.iOutputArgs.argsArray[0] = pIPP.iStarOutArgs;
        if(ippMode == IPP_CromaSupression_Mode ){
            pIPP.iOutputArgs.argsArray[1] = pIPP.iCrcbsOutArgs;
        }
        if(ippMode == IPP_EdgeEnhancement_Mode){
            pIPP.iOutputArgs.argsArray[1] = pIPP.iEenfOutArgs;
        }
        pIPP.iOutputArgs.argsArray[2] = pIPP.iYuvcOutArgs2;
    }
   
    LOGD("IPP_ProcessImage");
	if((pIPP.ippconfig.isINPLACE)){
		eError = IPP_ProcessImage(pIPP.hIPP, &(pIPP.iInputBufferDesc), &(pIPP.iInputArgs), NULL, &(pIPP.iOutputArgs));
	}
	else{
		eError = IPP_ProcessImage(pIPP.hIPP, &(pIPP.iInputBufferDesc), &(pIPP.iInputArgs), &(pIPP.iOutputBufferDesc),&(pIPP.iOutputArgs));
	}    
    if( eError != 0){
		LOGE("ERROR IPP_ProcessImage");
	}

    LOGD("IPP_StopProcessing");
    eError = IPP_StopProcessing(pIPP.hIPP);
    if( eError != 0){
        LOGE("ERROR IPP_StopProcessing");
    }

	LOGD("IPP_ProcessImage Done");
    
    return eError;
}

#endif

int CameraHal::ZoomPerform(float zoom)
{
    struct v4l2_crop crop;
    int delta_x, delta_y;
    int ret;

    LOG_FUNCTION_NAME

    memcpy( &crop, &mInitialCrop, sizeof(struct v4l2_crop));

    delta_x = crop.c.width - (crop.c.width /zoom);
    delta_y = crop.c.height - (crop.c.height/zoom);
 
    crop.c.width -= delta_x;
    crop.c.height -= delta_y;
    crop.c.left += delta_x >> 1;
    crop.c.top += delta_y >> 1;

    LOGE("Perform ZOOM: zoom_top = %d, zoom_left = %d, zoom_width = %d, zoom_height = %d", crop.c.top, crop.c.left, crop.c.width, crop.c.height);

    ret = ioctl(camera_device, VIDIOC_S_CROP, &crop);
    if (ret < 0) {
      LOGE("[%s]: ERROR VIDIOC_S_CROP failed", strerror(errno));
      return -1;
    }

    ret = ioctl(camera_device, VIDIOC_G_CROP, &crop);
    if (ret < 0) {
      LOGE("[%s]: ERROR VIDIOC_G_CROP failed", strerror(errno));
      return -1;
    }

    LOGE("Perform ZOOM G_GROP: crop_top = %d, crop_left = %d, crop_width = %d, crop_height = %d", crop.c.top, crop.c.left, crop.c.width, crop.c.height);

    LOG_FUNCTION_NAME_EXIT

    return 0;
}

/************/
#ifndef ICAP

//TODO: Update the normal in according the PPM Changes
int CameraHal::ICapturePerform(){

    int image_width, image_height;
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
    overlay_buffer_t overlaybuffer;
    int snapshot_buffer_index; 
    void* snapshot_buffer;
    int ipp_reconfigure=0;
    int ippTempConfigMode;
    int jpegFormat = PIX_YUV422I;

    LOG_FUNCTION_NAME

    LOGD("\n\n\n PICTURE NUMBER =%d\n\n\n",++pictureNumber);

	if(mMsgEnabled & CAMERA_MSG_SHUTTER)
		mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0, mCallbackCookie);

    mParameters.getPictureSize(&image_width, &image_height);
    mParameters.getPreviewSize(&preview_width, &preview_height);	

    LOGD("Picture Size: Width = %d \tHeight = %d", image_width, image_height);

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = image_width;
    format.fmt.pix.height = image_height;
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
	base = (base + 0xfff) & 0xfffff000;
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

	if(mMsgEnabled & CAMERA_MSG_RAW_IMAGE){
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
    snapshot_buffer = mOverlay->getBufferAddress( (void*)0 );
    LOGD("VPP scale_process() \n");
    err = scale_process(yuv_buffer, image_width, image_height,
                         snapshot_buffer, preview_width, preview_height, 0, jpegFormat, zoom_step[mZoomTargetIdx], 0 , 0, image_width, image_height);
    if( err ) LOGE("scale_process() failed");
    else LOGD("scale_process() OK");

    PPM("SCALED DOWN RAW IMAGE TO PREVIEW SIZE");

    PPM("DISPLAYED RAW IMAGE ON SCREEN");
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

	if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)


	{
#ifdef HARDWARE_OMX  
#if JPEG
        int jpegSize = (image_width * image_height) + 12288;
        mJPEGPictureHeap = new MemoryHeapBase(jpegSize+ 256);
        outBuffer = (void *)((unsigned long)(mJPEGPictureHeap->getBase()) + 128);

        exif_buffer *exif_buf = get_exif_buffer(gpsLocation);

		PPM("BEFORE JPEG Encode Image");
		LOGE(" outbuffer = 0x%x, jpegSize = %d, yuv_buffer = 0x%x, yuv_len = %d, image_width = %d, image_height = %d, quality = %d, mippMode =%d", outBuffer , jpegSize, yuv_buffer, yuv_len, image_width, image_height, quality,mippMode);
		jpegEncoder->encodeImage((uint8_t *)outBuffer , jpegSize, yuv_buffer, yuv_len,
				image_width, image_height, quality, exif_buf, jpegFormat, THUMB_WIDTH, THUMB_HEIGHT, image_width, image_height,
				0, 1.0, 0, 0, image_width, image_height);
		PPM("AFTER JPEG Encode Image");

		mJPEGPictureMemBase = new MemoryBase(mJPEGPictureHeap, 128, jpegEncoder->jpegSize);

	if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE){
		mDataCb(CAMERA_MSG_COMPRESSED_IMAGE,mJPEGPictureMemBase,mCallbackCookie);
    }


		PPM("Shot to Save", &ppm_receiveCmdToTakePicture);
	
        exif_buf_free (exif_buf);
        
        if( NULL != gpsLocation ) {
            free(gpsLocation);
            gpsLocation = NULL;
        }
        mJPEGPictureMemBase.clear();		
        mJPEGPictureHeap.clear();
        
#else


	if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)
		mDataCb(CAMERA_MSG_COMPRESSED_IMAGE,NULL,mCallbackCookie);


#endif

#endif

        
     
    }

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

void CameraHal::PPM(const char* str){
#if PPM_INSTRUMENTATION
	gettimeofday(&ppm, NULL); 
	ppm.tv_sec = ppm.tv_sec - ppm_start.tv_sec; 
	ppm.tv_sec = ppm.tv_sec * 1000000; 
	ppm.tv_sec = ppm.tv_sec + ppm.tv_usec - ppm_start.tv_usec; 
	LOGD("PPM: %s :%ld.%ld ms",str, ppm.tv_sec/1000, ppm.tv_sec%1000 ); 
#endif
}

void CameraHal::PPM(const char* str, struct timeval* ppm_first, ...){
#if PPM_INSTRUMENTATION
    char temp_str[256];
    va_list args;
    va_start(args, ppm_first);
    vsprintf(temp_str, str, args);
	gettimeofday(&ppm, NULL); 
	ppm.tv_sec = ppm.tv_sec - ppm_first->tv_sec; 
	ppm.tv_sec = ppm.tv_sec * 1000000; 
	ppm.tv_sec = ppm.tv_sec + ppm.tv_usec - ppm_first->tv_usec; 
	LOGD("PPM: %s :%ld.%ld ms",temp_str, ppm.tv_sec/1000, ppm.tv_sec%1000 );
    va_end(args);
#endif
}

};





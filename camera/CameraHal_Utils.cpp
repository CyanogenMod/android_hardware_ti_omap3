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

    /* get Init2A symbol */
    LOGE("dlsym MMS 3a Init2A Enter");
    fobj->lib.Init2A = (int (*)(Camera2AInterface**, int, uint8)) dlsym(fobj->lib.lib_handle, "Init2A");
    LOGE("dlsym MMS 3a Init2A Exit");
    if (NULL == fobj->lib.Init2A) {
        LOGE("ERROR - can't get dlsym \"Init2A\"");
        goto exit;
    }

    /* get Release2A symbol */
    LOGE("dlsym MMS 3a Release2A Enter");
    fobj->lib.Release2A = (int (*)(Camera2AInterface**)) dlsym(fobj->lib.lib_handle, "Release2A");
    LOGE("dlsym MMS 3a Release2A Exit");
    if (NULL == fobj->lib.Release2A) {
       LOGE( "ERROR - can't get dlsym \"Release2A\"");
       goto exit;
    }

    //TODO:
    fobj->cam_dev = 0;
	LOGE("Init2A Enter");    
    fobj->lib.Init2A(&fobj->cam_iface_2a, fobj->cam_dev, 0);
    LOGE("Init2A Exit");

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

#ifdef FOCUS_RECT
    focus_rect_set = 0;
#endif

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

#ifdef FOCUS_RECT
    focus_rect_set = 1;
#endif

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

	if( (mippMode != IPP_CromaSupression_Mode) && (mippMode != IPP_EdgeEnhancement_Mode) ){
		LOGE("Error unsupported mode=%d",mippMode);	
		return -1;
	}

    pIPP.hIPP = IPP_Create();
    LOGD("IPP Handle: %p",pIPP.hIPP);

	if( !pIPP.hIPP ){
		LOGE("ERROR IPP_Create");
		return -1;
	}

#if IPP_YUV422P
	pIPP.ippconfig.numberOfAlgos=4;
#else
	pIPP.ippconfig.numberOfAlgos=3;
#endif
    pIPP.ippconfig.orderOfAlgos[0]=IPP_START_ID;
    pIPP.ippconfig.orderOfAlgos[1]=IPP_YUVC_422iTO422p_ID;
	if(mippMode == IPP_CromaSupression_Mode ){
		pIPP.ippconfig.orderOfAlgos[2]=IPP_CRCBS_ID;
	}
	else if(mippMode == IPP_EdgeEnhancement_Mode){
		pIPP.ippconfig.orderOfAlgos[2]=IPP_EENF_ID;
	}
#if IPP_YUV422P
	pIPP.ippconfig.orderOfAlgos[3]=IPP_YUVC_422pTO422i_ID;
#endif    
    pIPP.ippconfig.isINPLACE=INPLACE_ON;   


#if IPP_YUV422P
	pIPP.outputBufferSize= w*h*2;	
#else
	pIPP.outputBufferSize= (w*h*3)/2;	
#endif

    LOGD("IPP_SetProcessingConfiguration");
    eError = IPP_SetProcessingConfiguration(pIPP.hIPP, pIPP.ippconfig);
	if(eError != 0){
		LOGE("ERROR IPP_SetProcessingConfiguration");
	}	
    
	if(mippMode == IPP_CromaSupression_Mode ){
		pIPP.CRCBptr.size = sizeof(IPP_CRCBSAlgoCreateParams);
		pIPP.CRCBptr.maxWidth = w;
		pIPP.CRCBptr.maxHeight = h;
		pIPP.CRCBptr.errorCode = 0;
	}
  
    pIPP.YUVCcreate.size = sizeof(IPP_YUVCAlgoCreateParams);
    pIPP.YUVCcreate.maxWidth = w;
    pIPP.YUVCcreate.maxHeight = h;
    pIPP.YUVCcreate.errorCode = 0;

	if(mippMode == IPP_CromaSupression_Mode ){   
	    LOGD("IPP_SetAlgoConfig");
	    eError = IPP_SetAlgoConfig(pIPP.hIPP, IPP_CRCBS_CREATEPRMS_CFGID, &(pIPP.CRCBptr));
		if(eError != 0){
			LOGE("ERROR IPP_SetAlgoConfig");
		}	
	}
    
    LOGD("IPP_SetAlgoConfig");
    eError = IPP_SetAlgoConfig(pIPP.hIPP, IPP_YUVC_422TO420_CREATEPRMS_CFGID, &(pIPP.YUVCcreate));
	if(eError != 0){
		LOGE("ERROR IPP_SetAlgoConfig");
	}	
#if IPP_YUV422P   
    LOGD("IPP_SetAlgoConfig");
    IPP_SetAlgoConfig(pIPP.hIPP, IPP_YUVC_420TO422_CREATEPRMS_CFGID, &(pIPP.YUVCcreate));
	if(eError != 0){
		LOGE("ERROR IPP_SetAlgoConfig");
	}	
#endif
    
    LOGD("IPP_InitializeImagePipe");
    eError = IPP_InitializeImagePipe(pIPP.hIPP);
	if(eError != 0){
		LOGE("ERROR IPP_SetAlgoConfig");
	}	
    
    LOGD("IPP_StartProcessing");
    eError = IPP_StartProcessing(pIPP.hIPP);
	if(eError != 0){
		LOGE("ERROR IPP_SetAlgoConfig");
	}	
    
    pIPP.iStarInArgs = (IPP_StarAlgoInArgs*)((char*)malloc(sizeof(IPP_StarAlgoInArgs) + BUFF_MAP_PADDING_TEST) + PADDING_OFFSET_TEST);
    pIPP.iStarOutArgs = (IPP_StarAlgoOutArgs*)((char*)(malloc(sizeof(IPP_StarAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);

	if(mippMode == IPP_CromaSupression_Mode ){
    	pIPP.iCrcbsInArgs = (IPP_CRCBSAlgoInArgs*)((char*)(malloc(sizeof(IPP_CRCBSAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    	pIPP.iCrcbsOutArgs = (IPP_CRCBSAlgoOutArgs*)((char*)(malloc(sizeof(IPP_CRCBSAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
	}

	if(mippMode == IPP_EdgeEnhancement_Mode){
    	pIPP.iEenfInArgs = (IPP_EENFAlgoInArgs*)((char*)(malloc(sizeof(IPP_EENFAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
	    pIPP.iEenfOutArgs = (IPP_EENFAlgoOutArgs*)((char*)(malloc(sizeof(IPP_EENFAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
	}

    pIPP.iYuvcInArgs1 = (IPP_YUVCAlgoInArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcOutArgs1 = (IPP_YUVCAlgoOutArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcInArgs2 = (IPP_YUVCAlgoInArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoInArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);
    pIPP.iYuvcOutArgs2 = (IPP_YUVCAlgoOutArgs*)((char*)(malloc(sizeof(IPP_YUVCAlgoOutArgs) + BUFF_MAP_PADDING_TEST)) + PADDING_OFFSET_TEST);

	if(mippMode == IPP_EdgeEnhancement_Mode){
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
int CameraHal::DeInitIPP()
{
    int eError = 0;

    LOGD("IPP_StopProcessing");
	eError = IPP_StopProcessing(pIPP.hIPP);
    if( eError != 0){
		LOGE("ERROR IPP_StopProcessing");
	}
	
	LOGD("IPP_DeinitializePipe");
	eError = IPP_DeinitializePipe(pIPP.hIPP);
    LOGD("IPP_DeinitializePipe");
    if( eError != 0){
		LOGE("ERROR IPP_DeinitializePipe");
	}

    LOGD("IPP_Delete");
    IPP_Delete(&(pIPP.hIPP));
    
    free(((char*)pIPP.iStarInArgs - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iStarOutArgs - PADDING_OFFSET_TEST));
	
	if(mippMode == IPP_CromaSupression_Mode ){
    	free(((char*)pIPP.iCrcbsInArgs - PADDING_OFFSET_TEST));
	    free(((char*)pIPP.iCrcbsOutArgs - PADDING_OFFSET_TEST));
	}

	if(mippMode == IPP_EdgeEnhancement_Mode){
	    free(((char*)pIPP.iEenfInArgs - PADDING_OFFSET_TEST));
    	free(((char*)pIPP.iEenfOutArgs - PADDING_OFFSET_TEST));
	}

    free(((char*)pIPP.iYuvcInArgs1 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcOutArgs1 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcInArgs2 - PADDING_OFFSET_TEST));
    free(((char*)pIPP.iYuvcOutArgs2 - PADDING_OFFSET_TEST));
	
	if(mippMode == IPP_EdgeEnhancement_Mode){
	    free(((char*)pIPP.dynEENF - PADDING_OFFSET_TEST));
	}

	if(!(pIPP.ippconfig.isINPLACE)){
		free(pIPP.pIppOutputBuffer - PADDING_OFFSET_TEST);
	}

    LOGD("Terminating IPP");
    
    return eError;
}

int CameraHal::PopulateArgsIPP(int w, int h)
{
    int eError = 0;
    
    LOGD("IPP: PopulateArgs ENTER");

	//configuring size of input and output structures
    pIPP.iStarInArgs->size = sizeof(IPP_StarAlgoInArgs);
	if(mippMode == IPP_CromaSupression_Mode ){
	    pIPP.iCrcbsInArgs->size = sizeof(IPP_CRCBSAlgoInArgs);
	}
	if(mippMode == IPP_EdgeEnhancement_Mode){
	    pIPP.iEenfInArgs->size = sizeof(IPP_EENFAlgoInArgs);
	}
	
    pIPP.iYuvcInArgs1->size = sizeof(IPP_YUVCAlgoInArgs);
    pIPP.iYuvcInArgs2->size = sizeof(IPP_YUVCAlgoInArgs);
    
    pIPP.iStarOutArgs->size = sizeof(IPP_StarAlgoOutArgs);
	if(mippMode == IPP_CromaSupression_Mode ){
    	pIPP.iCrcbsOutArgs->size = sizeof(IPP_CRCBSAlgoOutArgs);
	}
	if(mippMode == IPP_EdgeEnhancement_Mode){
    	pIPP.iEenfOutArgs->size = sizeof(IPP_EENFAlgoOutArgs);
	}
	
    pIPP.iYuvcOutArgs1->size = sizeof(IPP_YUVCAlgoOutArgs);
    pIPP.iYuvcOutArgs2->size = sizeof(IPP_YUVCAlgoOutArgs);
    
	//Configuring specific data of algorithms
	if(mippMode == IPP_CromaSupression_Mode ){
	    pIPP.iCrcbsInArgs->inputHeight = h;
	    pIPP.iCrcbsInArgs->inputWidth = w;	
#if IPP_YUV422P 
    pIPP.iCrcbsInArgs->inputChromaFormat = IPP_YUV_422P;
#else
	pIPP.iCrcbsInArgs->inputChromaFormat = IPP_YUV_420P;
#endif
	}

	if(mippMode == IPP_EdgeEnhancement_Mode){
#if IPP_YUV422P 
    	pIPP.iEenfInArgs->inputChromaFormat = IPP_YUV_422P;
#else
		pIPP.iEenfInArgs->inputChromaFormat = IPP_YUV_420P;
#endif	
		pIPP.iEenfInArgs->inFullWidth = w;
		pIPP.iEenfInArgs->inFullHeight = h;
		pIPP.iEenfInArgs->inOffsetV = 0;
		pIPP.iEenfInArgs->inOffsetH = 0;
		pIPP.iEenfInArgs->inputWidth = w;
		pIPP.iEenfInArgs->inputHeight = h;
		pIPP.iEenfInArgs->inPlace = 0;
		pIPP.iEenfInArgs->NFprocessing = 0;
    }
	
    pIPP.iYuvcInArgs1->inputHeight = h;
    pIPP.iYuvcInArgs1->inputWidth = w;
	
#if IPP_YUV422P 
    pIPP.iYuvcInArgs1->outputChromaFormat = IPP_YUV_422P;
#else
	pIPP.iYuvcInArgs1->outputChromaFormat = IPP_YUV_420P;
#endif    
    pIPP.iYuvcInArgs1->inputChromaFormat = IPP_YUV_422ILE;

#if IPP_YUV422P     
    pIPP.iYuvcInArgs2->inputHeight = h;
    pIPP.iYuvcInArgs2->inputWidth = w;
    pIPP.iYuvcInArgs2->outputChromaFormat = IPP_YUV_422ILE;
    pIPP.iYuvcInArgs2->inputChromaFormat = IPP_YUV_422P;
#endif
  
	pIPP.iStarOutArgs->extendedError= 0;
	pIPP.iYuvcOutArgs1->extendedError = 0;
	if(mippMode == IPP_EdgeEnhancement_Mode)
		pIPP.iEenfOutArgs->extendedError = 0;
	if(mippMode == IPP_CromaSupression_Mode )	
		pIPP.iCrcbsOutArgs->extendedError = 0;

#if IPP_YUV422P 
    pIPP.iYuvcOutArgs2->extendedError = 0;
#endif

	//Filling ipp status structure
    pIPP.starStatus.size = sizeof(IPP_StarAlgoStatus);
	if(mippMode == IPP_CromaSupression_Mode ){
    	pIPP.CRCBSStatus.size = sizeof(IPP_CRCBSAlgoStatus);
	}
	if(mippMode == IPP_EdgeEnhancement_Mode){
    	pIPP.EENFStatus.size = sizeof(IPP_EENFAlgoStatus);
	}
	  
    pIPP.statusDesc.statusPtr[0] = &(pIPP.starStatus);
	if(mippMode == IPP_CromaSupression_Mode ){
	    pIPP.statusDesc.statusPtr[1] = &(pIPP.CRCBSStatus);
	}
	if(mippMode == IPP_EdgeEnhancement_Mode){
	    pIPP.statusDesc.statusPtr[1] = &(pIPP.EENFStatus);
	}
	
    pIPP.statusDesc.numParams = 2;
    pIPP.statusDesc.algoNum[0] = 0;
    pIPP.statusDesc.algoNum[1] = 1;
    pIPP.statusDesc.algoNum[1] = 1;

    LOGD("IPP: PopulateArgs EXIT");
    
    return eError;
}


int CameraHal::ProcessBufferIPP(void *pBuffer, long int nAllocLen,
                                int EdgeEnhancementStrength, int WeakEdgeThreshold, 
                                int StrongEdgeThreshold, int LumaNoiseFilterStrength,
                                int ChromaNoiseFilterStrength)
{
    int eError = 0;

	if(mippMode == IPP_EdgeEnhancement_Mode){
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


#if IPP_YUV422P 
	pIPP.iInputArgs.numArgs = 4;/*be aware of the algos order*/
#else
	pIPP.iInputArgs.numArgs = 3;
#endif    
    pIPP.iInputArgs.argsArray[0] = pIPP.iStarInArgs;
    pIPP.iInputArgs.argsArray[1] = pIPP.iYuvcInArgs1;
	if(mippMode == IPP_CromaSupression_Mode ){
	    pIPP.iInputArgs.argsArray[2] = pIPP.iCrcbsInArgs;
	}
	if(mippMode == IPP_EdgeEnhancement_Mode){    
		pIPP.iInputArgs.argsArray[2] = pIPP.iEenfInArgs; 
	}
#if IPP_YUV422P 
   pIPP.iInputArgs.argsArray[3] = pIPP.iYuvcInArgs2;
#endif    

#if IPP_YUV422P 
	pIPP.iOutputArgs.numArgs = 4;/*be aware of the algos order*/
#else
	pIPP.iOutputArgs.numArgs = 3;
#endif   
    pIPP.iOutputArgs.argsArray[0] = pIPP.iStarOutArgs;
    pIPP.iOutputArgs.argsArray[1] = pIPP.iYuvcOutArgs1;
	if(mippMode == IPP_CromaSupression_Mode ){
	    pIPP.iOutputArgs.argsArray[2] = pIPP.iCrcbsOutArgs;
	}
	if(mippMode == IPP_EdgeEnhancement_Mode){   
    	pIPP.iOutputArgs.argsArray[2] = pIPP.iEenfOutArgs;
	}
#if IPP_YUV422P
   	pIPP.iOutputArgs.argsArray[3] = pIPP.iYuvcOutArgs2;
#endif
   
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

	LOGD("IPP_ProcessImage Done");
    
    return eError;
}

#endif


#ifdef IMAGE_PROCESSING_PIPELINE_MMS
#include "mms_ipp_cfg.h"

int CameraHal::handle_alg_errors(ARM_ERROR error)
{
    if ( error == ARM_ERROR_FAIL) {
        LOGE(" Algorithm error: The algorithm failed\n");
    }
    if ( error ==  ARM_ERROR_ATTACH_DSP) {
        LOGE(" Algorithm error: Attaching to DSP failed\n");
    }
    if ( error ==  ARM_ERROR_DETACH_DSP) {
        LOGE(" Algorithm error: Detaching from DSP failed\n");
    }
    if ( error ==  ARM_ERROR_NODE_INIT) {
        LOGE(" Algorithm error: Initialization of Node failed\n");
    }
    if ( error ==  ARM_ERROR_NODE_DELETE) {
        LOGE(" Algorithm error: Deleting Node failed\n");
    }
    if ( error ==  ARM_ERROR_DSPMSG_FAIL) {
        LOGE(" Algorithm error: Sending MSG to DSP failed\n");
    }
    if ( error ==  ARM_ERROR_DSP_FAIL) {
        LOGE(" Algorithm error: DSP returned ERROR\n");
    }
    if ( error ==  ARM_ERROR_NOT_ENOUGH_SPACE) {
        LOGE(" Algorithm error: ERROR - Not enough space\n");
    }
    if ( error ==  ARM_ERROR_REGISTER_NODE) {
        LOGE(" Algorithm error: ERROR when registering node with dcd\n");
    }
    if ( error ==  ARM_ERROR_UNREGISTER_NODE) {
        LOGE(" Algorithm error: ERROR when unregistering node with dcd\n");
    }

    return 0;
}

int CameraHal::InitIPPMMSDefault(int w, int h)
{
    ARM_ERROR error;
    pip_omap_ctrl_t pip_omap_ctrl;

    error =  pip_omap_create(&pIPPMMS, 0, 0);

    if (error){
        LOGE("PIP OMAP Algorithm creation failed: ");
        handle_alg_errors(error);

        return -1;
    } else {
        LOGE("PIP OMAP Algorithm created successfully.");
    }

    pip_omap_ctrl.imgwidth = w;
    pip_omap_ctrl.imgheight = h;
    pip_omap_ctrl.input_ppln = pip_omap_ctrl.imgwidth;
    pip_omap_ctrl.output_ppln = pip_omap_ctrl.input_ppln;

    pip_omap_ctrl.padding = 0;
    
    pip_omap_ctrl.fir_lum_coeff = fir_lum_coeff;
    pip_omap_ctrl.fir_lum_den = FIR_LUM_DEN;
    pip_omap_ctrl.ee_lut_ptr = ee_lut_ptr;

    pip_omap_ctrl.fir_chr_coeff = fir_chr_coeff;
    pip_omap_ctrl.fir_chr_den = FIR_CHR_DEN;
    pip_omap_ctrl.use_crcbdenoise = USE_CRCBDENOISE;
    pip_omap_ctrl.denoising_thr = DENOISING_THR;
    pip_omap_ctrl.desaturating_thr = DESATURATING_THR;
    pip_omap_ctrl.detailpreserve = DESATURATING_THR;

    error = pip_omap_control(pIPPMMS, &pip_omap_ctrl);

    if (error){
        printf("PIP OMAP Algorithm control failed: ");
        handle_alg_errors(error);
   
        return -1;
    } else {
        printf("PIP OMAP Algorithm control successfull.");
    }

    return 0;
}

int CameraHal::InitIPPMMS(pip_omap_ctrl_t *cfg)
{
    ARM_ERROR error;
    error =  pip_omap_create(&pIPPMMS, 0, 0);

    if (error){
        LOGE("PIP OMAP Algorithm creation failed: ");
        handle_alg_errors(error);

        return -1;
    } else {
        LOGE("PIP OMAP Algorithm created successfully.");
    }

    error = pip_omap_control(pIPPMMS, cfg);

    if (error){
        printf("PIP OMAP Algorithm control failed: ");
        handle_alg_errors(error);

        return -1;
    } else {
        LOGE("PIP OMAP Algorithm control successfull.");
    }

    return 0;
}

int CameraHal::DeInitIPPMMS()
{
    ARM_ERROR error;
    error =  pip_omap_delete(pIPPMMS);  

    if (error){
        LOGE("PIP OMAP Algorithm delete failed: ");
        handle_alg_errors(error);

        return -1;
    } else {
        LOGE("PIP OMAP Algorithm deleted successfully.");
    }

    return 0;
}

int CameraHal::ProcessBufferIPPMMS(void *pBuffer)
{
    ARM_ERROR error;
    error = pip_omap_process(pIPPMMS, pBuffer, pBuffer);

    if (error){
        LOGE("PIP OMAP Algorithm process failed. ");
        handle_alg_errors(error);

        return -1;
    } else {
        LOGE("PIP OMAP Algorithm applied successfully.");
    }

    return 0;
}

#endif



					/***************/

void CameraHal::drawRect(uint8_t *input, uint8_t color, int x1, int y1, int x2, int y2, int width, int height)
{
	int i, start_offset, end_offset;
	int offset = 0;
	int offset_1 = 1;;
	int mod = 0;
	int cb,cr;

	switch (color){

		case FOCUS_RECT_WHITE:
			cb = 255;
			cr = 255;
			offset = 1;
			offset_1 = 1;

			break;
		case FOCUS_RECT_GREEN:
			cb = 0;
			cr = 0;
			offset = 0;
			offset_1 = 2;
			break;
		case FOCUS_RECT_RED:
			cb = 128;
			cr = 255;
			offset = 0;
			offset_1 = 2;
			break;

		default:
			cb = 255;
			cr = 255;
			offset = 1;
			offset_1 = 1;
	}

	//Top Line
	start_offset = ((width*y1) + x1)*2 + offset;
	end_offset =  start_offset + (x2 - x1)*2 + offset;
	for ( i = start_offset; i < end_offset; i += 2){
		if (mod % 2)
			*( input + i ) = cr;
		else
			*( input + i ) = cb;
		mod++;
	}

	//Left Line
	start_offset = ((width*y1) + x1)*2 + offset_1;
	end_offset = ((width*y2) + x1)*2 + offset_1;
	for ( i = start_offset; i < end_offset; i += 2*width){
			*( input + i ) = cr;
	}

	mod = 0;

	//Botom Line
	start_offset = ((width*y2) + x1)*2 + offset;
	end_offset = start_offset + (x2 - x1)*2 + offset;
	for( i = start_offset; i < end_offset; i += 2){
		if (mod % 2)
			*( input + i ) = cr;
		else
			*( input + i ) = cb;
		mod++;
	}

	//Right Line
	start_offset = ((width*y1) + x1 + (x2 - x1))*2 + offset_1;
	end_offset = ((width*y2) + x1 + (x2 - x1))*2 + offset_1;
	for ( i = start_offset; i < end_offset; i += 2*width){
		*( input + i ) = cr;
	}
}

int CameraHal::ZoomPerform(float zoom)
{
    struct v4l2_crop crop;
    float fwidth, fheight;
    int zoom_left,zoom_top,zoom_width, zoom_height,w,h;
    int ret;

    if (zoom < 1) 
       zoom = 1;
    if (zoom > 8)
       zoom = 8;

    mParameters.getPreviewSize(&w, &h);
    fwidth = w/zoom;
    fheight = h/zoom;
    zoom_width = ((int) fwidth) & (~3);
    zoom_height = ((int) fheight) & (~3);
    zoom_left = w/2 - (zoom_width/2);
    zoom_top = h/2 - (zoom_height/2);

    LOGE("Perform ZOOM: zoom_width = %d, zoom_height = %d, zoom_left = %d, zoom_top = %d", zoom_width, zoom_height, zoom_left, zoom_top); 

    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c.left   = zoom_left; 
    crop.c.top    = zoom_top;
    crop.c.width  = zoom_width;
    crop.c.height = zoom_height;

#if 1
    ret = ioctl(camera_device, VIDIOC_S_CROP, &crop);
    if (ret < 0) {
      LOGE("[%s]: ERROR VIDIOC_S_CROP failed", strerror(errno));
      return -1;
    }
#endif
    return 0;
}


#if 0
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

#endif
#if 0
int CameraHal::ZoomPerform(int zoomFactor)
{
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    int ret;
    int prev_width, prev_height;
    int zoom_left, zoom_top, zoom_width, zoom_height;

    LOG_FUNCTION_NAME

    zoomFactor =mzoom;
    mParameters.getPreviewSize(&prev_width, &prev_height);

    LOGD("Preview width=%d height=%d, zoomFactor: %d",prev_width,prev_height, zoomFactor);

    if (zoomFactor < 1) {
        LOGD("Warning, trying to set zoomFactor < 1");
        zoomFactor = 1;
    }
    else if (zoomFactor > 4) {
        LOGD("Warning, trying to set zoomFactor > 4");
        zoomFactor = 4;
    }

    zoom_width = (prev_width * 10) / (zoomFactor* 10);
	zoom_height = (prev_height * 10) / (zoomFactor* 10);
    zoom_left = (prev_width - zoom_width) / 2;
	zoom_top = (prev_height - zoom_height) / 2;
#if 1
    LOGD("Before VIDIOC_CROPCAP");
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(camera_device, VIDIOC_CROPCAP, &cropcap);
	if (ret != 0) {
		perror("VIDIOC_CROPCAP");
		return -1;
	}
    LOGD("After VIDIOC_CROPCAP");
#endif
    
    LOGD("Old Crop (%d, %d) (%d, %d)\n", crop.c.left, crop.c.top,crop.c.width, crop.c.height);

    LOGD("Wanted Crop (%d, %d) (%d, %d)\n", zoom_left, zoom_top, zoom_width, zoom_height);

    memset(&crop, 0, sizeof(crop));
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c.left   = zoom_left;
    crop.c.top    = zoom_top;
    crop.c.width  = zoom_width;
    crop.c.height = zoom_height;

    LOGD("Before VIDIOC_S_CROP");
    ret = ioctl(camera_device, VIDIOC_S_CROP, &crop);
    if (ret < 0) {
      LOGE("[%s]: ERROR VIDIOC_S_CROP failed\n", __func__);
      return -1;
    }
    LOGD("After VIDIOC_S_CROP");

    LOG_FUNCTION_NAME_EXIT

    return 0;
}
#endif

/************/
#ifndef ICAP
int CameraHal::CapturePicture(){

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
	unsigned short ipp_ee_q, ipp_ew_ts, ipp_es_ts, ipp_luma_nf, ipp_chroma_nf; 
    int err, i;
    overlay_buffer_t overlaybuffer;
    int snapshot_buffer_index; 
    void* snapshot_buffer;
    int ipp_reconfigure=0;
    int ippTempConfigMode;
    int jpegFormat = YUV422;

    LOG_FUNCTION_NAME

    LOGD("\n\n\n PICTURE NUMBER =%d\n\n\n",++pictureNumber);

    if (mShutterCallback)
        mShutterCallback(mPictureCallbackCookie);

    mParameters.getPictureSize(&image_width, &image_height);
    mParameters.getPreviewSize(&preview_width, &preview_height);	

    LOGD("Picture Size: Width = %d \tHeight = %d", image_width, image_height);

#if OPEN_CLOSE_WORKAROUND
    close(camera_device);
    camera_device = open(VIDEO_DEVICE, O_RDWR);
    if (camera_device < 0) {
          LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!",
         		  strerror(errno) );
  	}
#endif

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

    if (mRawPictureCallback) {
        mRawPictureCallback(mPictureBuffer, mPictureCallbackCookie);
    }

#if OPEN_CLOSE_WORKAROUND
    close(camera_device);
    camera_device = open(VIDEO_DEVICE, O_RDWR);
    if (camera_device < 0) {
          LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!",
         		  strerror(errno) );
  	}
#endif
  
    yuv_buffer = (uint8_t*)buffer.m.userptr;	
    LOGD("PictureThread: generated a picture, yuv_buffer=%p yuv_len=%d",yuv_buffer,yuv_len);

    //image_height &= 0xFFFFFFF8;

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
                         snapshot_buffer, preview_width, preview_height);
    if( err ) LOGE("scale_process() failed");
    else LOGD("scale_process() OK");

    PPM("SCALED DOWN RAW IMAGE TO PREVIEW SIZE");

    // At this point, the Surface Flinger would have indirectly called Stream OFF on Overlay
    // which implies that all the overlay buffers will be dequeued.
    // Therefore, we must queue atleast 3 buffers for the overlay to enable streaming and 
    // show the snapshot.
    mOverlay->queueBuffer((void*)2);
    mOverlay->queueBuffer((void*)1);
    mOverlay->queueBuffer((void*)0); //the three buffers are dequeued but the last one is the only one which will remain on the screen. JJ
    // We need not update the array buffers_queued_to_dss because Surface Flinger will
    // once again call Stream OFF on Overlay

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

        if(mippMode == IPP_EdgeEnhancement_Mode){
            ipp_ee_q = 199;
            ipp_ew_ts = 20;
            ipp_es_ts = 240;
            ipp_luma_nf = 2;
            ipp_chroma_nf = 2;
        }

		err = InitIPP(image_width,image_height);
		if( err ) {
			LOGE("ERROR InitIPP() failed");	
			return -1;	   
		}
		PPM("After IPP Init");
		err = PopulateArgsIPP(image_width,image_height);
		if( err ) {
			LOGE("ERROR PopulateArgsIPP() failed");		   
			return -1;
		} 
		PPM("BEFORE IPP Process Buffer");
		
		LOGD("Calling ProcessBufferIPP(buffer=%p , len=0x%x)", yuv_buffer, yuv_len);
		err = ProcessBufferIPP(yuv_buffer, yuv_len,
				        ipp_ee_q,
				        ipp_ew_ts,
				        ipp_es_ts, 
				        ipp_luma_nf,
				        ipp_chroma_nf);
		if( err ) {
			LOGE("ERROR ProcessBufferIPP() failed");		   
			return -1;
		}
		PPM("AFTER IPP Process Buffer");

		if(pIPP.hIPP != NULL){
			err = DeInitIPP();
			if( err ){
				LOGE("ERROR DeInitIPP() failed");
				return -1;
			} 
			pIPP.hIPP = NULL;
		}

	PPM("AFTER IPP Deinit");
   	if(!(pIPP.ippconfig.isINPLACE)){ 
		yuv_buffer = pIPP.pIppOutputBuffer;
	}
	 
	#if !( IPP_YUV422P )
		yuv_len=  ((image_width * image_height *3)/2);
        jpegFormat = YUV420;
		LOGD("YUV420 !!!!");
	#else
        jpegFormat = YUV422;
		LOGD("YUV42 !!!!");
    #endif
		
	}
	//SaveFile(NULL, (char*)"yuv", yuv_buffer, yuv_len); 
    
#endif

    if (mJpegPictureCallback) {
#ifdef HARDWARE_OMX  
#if JPEG
        int jpegSize = (image_width * image_height) + 12288;
        mJPEGPictureHeap = new MemoryHeapBase(jpegSize+ 256);
        outBuffer = (void *)((unsigned long)(mJPEGPictureHeap->getBase()) + 128);

		PPM("BEFORE JPEG Encode Image");
		LOGE(" outbuffer = 0x%x, jpegSize = %d, yuv_buffer = 0x%x, yuv_len = %d, image_width = %d, image_height = %d, quality = %d, mippMode =%d", outBuffer , jpegSize, yuv_buffer, yuv_len, image_width, image_height, quality,mippMode);	  
        jpegEncoder->encodeImage(outBuffer, jpegSize, yuv_buffer, yuv_len, image_width, image_height, quality,jpegFormat);
		PPM("AFTER JPEG Encode Image");

		mJPEGPictureMemBase = new MemoryBase(mJPEGPictureHeap, 128, jpegEncoder->jpegSize);

        if(mJpegPictureCallback) {
            mJpegPictureCallback(mJPEGPictureMemBase, mPictureCallbackCookie); 
        }

#if PPM_INSTRUMENTATION
		PPM("Shot to Save", &ppm_receiveCmdToTakePicture);
#endif	

        mJPEGPictureMemBase.clear();		
        mJPEGPictureHeap.clear();
#else
if(mJpegPictureCallback) {
            mJpegPictureCallback(NULL, mPictureCallbackCookie); 
        }
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

void CameraHal::PPM(char* str){
#if PPM_INSTRUMENTATION
	gettimeofday(&ppm, NULL); 
	ppm.tv_sec = ppm.tv_sec - ppm_start.tv_sec; 
	ppm.tv_sec = ppm.tv_sec * 1000000; 
	ppm.tv_sec = ppm.tv_sec + ppm.tv_usec - ppm_start.tv_usec; 
	LOGD("PPM: %s :%ld.%ld ms",str, ppm.tv_sec/1000, ppm.tv_sec%1000 ); 
#endif
}

void CameraHal::PPM(char* str, struct timeval* ppm_first){
#if PPM_INSTRUMENTATION
	gettimeofday(&ppm, NULL); 
	ppm.tv_sec = ppm.tv_sec - ppm_first->tv_sec; 
	ppm.tv_sec = ppm.tv_sec * 1000000; 
	ppm.tv_sec = ppm.tv_sec + ppm.tv_usec - ppm_first->tv_usec; 
	LOGD("PPM: %s :%ld.%ld ms",str, ppm.tv_sec/1000, ppm.tv_sec%1000 ); 
#endif
}

};





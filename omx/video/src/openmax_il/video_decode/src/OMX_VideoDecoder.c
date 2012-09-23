
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
/* =============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found 
*  in the license agreement under which this software has been supplied.
* ============================================================================*/
/** 
* @file OMX_Video_Decoder.c
*
* This file implements OMX Component for video decoder that 
* is fully compliant with the OMX specification 1.0
*
* @path  $(CSLPATH)\
*
* @rev  0.1
*/
/* ---------------------------------------------------------------------------- 
*! 
*! Revision History 
*! ===================================
*! 24-July-2005 mf:  Initial Version. Change required per OMAPSWxxxxxxxxx
*! to provide _________________.
*!
* ============================================================================*/

/* ------compilation control switches ----------------------------------------*/
/*******************************************************************************
*  INCLUDE FILES                                                  
*******************************************************************************/
/* ----- system and platform files -------------------------------------------*/
#include <wchar.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>


#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <dbapi.h>
#include <OMX_Component.h>
#include "OMX_VideoDecoder.h"
#include "OMX_VideoDec_Utils.h"
#include "OMX_VideoDec_DSP.h"
#include "OMX_VideoDec_Thread.h"
#include "OMX_VidDec_CustomCmd.h"


#define ENABLE_GRALLOC_BUFFERS
#define USE_ION
#ifdef USE_ION
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <ion.h>
#endif

#include "hal_public.h"
#include "OMX_IVCommon.h"


#define LOG_NDEBUG 0
#define LOG_TAG "OMX_VideoDecoder"
#include <utils/Log.h>

#define HAL_NV12_PADDED_PIXEL_FORMAT (OMX_TI_COLOR_FormatYUV420PackedSemiPlanar - OMX_COLOR_FormatVendorStartUnused)

/* For PPM fps measurements */
static int mDebugFps = 0;

#ifdef RESOURCE_MANAGER_ENABLED
#include <ResourceManagerProxyAPI.h>
#endif

/*******************************************************************************
*  EXTERNAL REFERENCES NOTE : only use if not found in header file
*******************************************************************************/
/*--------data declarations --------------------------------------------------*/

/*--------function prototypes ------------------------------------------------*/
OMX_ERRORTYPE VIDDEC_CheckSetParameter(VIDDEC_COMPONENT_PRIVATE* pComponentPrivate,
                                       OMX_PTR pCompParam,
                                       OMX_INDEXTYPE nParamIndex);
extern OMX_ERRORTYPE VIDDEC_Start_ComponentThread(OMX_HANDLETYPE pHandle);
extern OMX_ERRORTYPE VIDDEC_Stop_ComponentThread(OMX_HANDLETYPE pComponent);
extern OMX_ERRORTYPE VIDDEC_Load_Defaults (VIDDEC_COMPONENT_PRIVATE* pComponentPrivate, OMX_S32 nPassing);
extern OMX_ERRORTYPE IncrementCount (OMX_U32 * pCounter, pthread_mutex_t *pMutex);
extern OMX_ERRORTYPE DecrementCount (OMX_U32 * pCounter, pthread_mutex_t *pMutex);

/*******************************************************************************
*  PUBLIC DECLARATIONS Defined here, used elsewhere
*******************************************************************************/
/*--------data declarations --------------------------------------------------*/
OMX_STRING cVideoDecodeName = "OMX.TI.Video.Decoder";

VIDDEC_CUSTOM_PARAM sVideoDecCustomParams[] = {{VIDDEC_CUSTOMPARAM_PROCESSMODE, VideoDecodeCustomParamProcessMode},
                                               {VIDDEC_CUSTOMPARAM_H264BITSTREAMFORMAT, VideoDecodeCustomParamH264BitStreamFormat},
                                               {VIDDEC_CUSTOMPARAM_WMVPROFILE, VideoDecodeCustomParamWMVProfile},
                                               {VIDDEC_CUSTOMPARAM_WMVFILETYPE, VideoDecodeCustomParamWMVFileType},
                                               {VIDDEC_CUSTOMPARAM_PARSERENABLED, VideoDecodeCustomParamParserEnabled},
                                               {VIDDEC_CUSTOMPARAM_ISNALBIGENDIAN, VideoDecodeCustomParamIsNALBigEndian},
#ifdef VIDDEC_SPARK_CODE
                                               {VIDDEC_CUSTOMPARAM_ISSPARKINPUT, VideoDecodeCustomParamIsSparkInput},
#endif
                                               {VIDDEC_CUSTOMCONFIG_DEBUG, VideoDecodeCustomConfigDebug},
                                               {VIDDEC_CUSTOMCONFIG_CACHEABLEBUFFERS, VideoDecodeCustomConfigCacheableBuffers},
                                               {VIDDEC_ENABLE_ANDROID_NATIVE_BUFFERS, VideoDecodeEnableAndroidNativeBuffers},
                                               {VIDDEC_GET_ANDROID_NATIVE_BUFFER_USAGE, VideoDecodeGetAndroidNativeBufferUsage},
                                               {VIDDEC_ANDROID_USE_ANDROID_NATIVE_BUFFER2, VideoDecodeGetAndroiduseAndroidNativeBuffer2}};

/* H.263 Supported Levels & profiles */
VIDEO_PROFILE_LEVEL_TYPE SupportedH263ProfileLevels[] = {
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level10},
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level20},
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level30},
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level40},
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level45},
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level50},
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level60},
  {OMX_VIDEO_H263ProfileBaseline, OMX_VIDEO_H263Level70},
  {-1, -1}};

/* MPEG4 Supported Levels & profiles */
VIDEO_PROFILE_LEVEL_TYPE SupportedMPEG4ProfileLevels[] ={
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0},
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level0b},
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level1},
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level2},
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level3},
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level4},
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level4a},
  {OMX_VIDEO_MPEG4ProfileSimple, OMX_VIDEO_MPEG4Level5},
  {OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level0},
  {OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level0b},
  {OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level1},
  {OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level2},
  {OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level3},
  {OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level4},
  {OMX_VIDEO_MPEG4ProfileAdvancedSimple, OMX_VIDEO_MPEG4Level5},
  {-1,-1}};

/* AVC Supported Levels & profiles */
VIDEO_PROFILE_LEVEL_TYPE SupportedAVCProfileLevels[] ={
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel1b},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel11},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel12},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel13},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel2},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel21},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel22},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel3},
  {OMX_VIDEO_AVCProfileBaseline, OMX_VIDEO_AVCLevel31},
  {-1,-1}};
/*--------function prototypes ------------------------------------------------*/

/*******************************************************************************
*  PRIVATE DECLARATIONS Defined here, used only here
*******************************************************************************/
/*--------data declarations --------------------------------------------------*/
/*SetConfig defines*/
#define SET_CONFIG_MUTEX_BASE           0x0000FF00
#define SET_CONFIG_ONMUTEX              0x0000FF01
#define SET_CONFIG_OFFMUTEX             0x0000FF00

/*--------macro definitions --------------------------------------------------*/

/*-------function prototypes -------------------------------------------------*/
static OMX_ERRORTYPE VIDDEC_SetCallbacks (OMX_HANDLETYPE hComp,
                                          OMX_CALLBACKTYPE* pCallBacks,
                                          OMX_PTR pAppData);

static OMX_ERRORTYPE VIDDEC_GetComponentVersion (OMX_HANDLETYPE hComp,
                                                 OMX_STRING pComponentName,
                                                 OMX_VERSIONTYPE* pComponent,
                                                 OMX_VERSIONTYPE* pSpecVersion,
                                                 OMX_UUIDTYPE* pComponentUUID);

static OMX_ERRORTYPE VIDDEC_SendCommand (OMX_HANDLETYPE hComponent,
                                         OMX_COMMANDTYPE Cmd,
                                         OMX_U32 nParam1,
                                         OMX_PTR pCmdData);

static OMX_ERRORTYPE VIDDEC_GetParameter (OMX_HANDLETYPE hComponent,
                                          OMX_INDEXTYPE nParamIndex,
                                          OMX_PTR ComponentParamStruct);

static OMX_ERRORTYPE VIDDEC_SetParameter (OMX_HANDLETYPE hComp,
                                          OMX_INDEXTYPE nParamIndex,
                                          OMX_PTR ComponentParamStruct);

static OMX_ERRORTYPE VIDDEC_GetConfig (OMX_HANDLETYPE hComp,
                                       OMX_INDEXTYPE nConfigIndex,
                                       OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE VIDDEC_SetConfig (OMX_HANDLETYPE hComp,
                                       OMX_INDEXTYPE nConfigIndex,
                                       OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE VIDDEC_EmptyThisBuffer (OMX_HANDLETYPE hComp,
                                             OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE VIDDEC_FillThisBuffer (OMX_HANDLETYPE hComp,
                                            OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE VIDDEC_GetState (OMX_HANDLETYPE hComp, OMX_STATETYPE* pState);

static OMX_ERRORTYPE VIDDEC_ComponentTunnelRequest (OMX_IN OMX_HANDLETYPE hComp,
                                                    OMX_IN OMX_U32 nPort,
                                                    OMX_IN OMX_HANDLETYPE hTunneledComp,
                                                    OMX_IN OMX_U32 nTunneledPort,
                                                    OMX_INOUT OMX_TUNNELSETUPTYPE* pTunnelSetup);

static OMX_ERRORTYPE VIDDEC_UseBuffer (OMX_IN OMX_HANDLETYPE hComponent,
                                OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                                OMX_IN OMX_U32 nPortIndex,
                                OMX_IN OMX_PTR pAppPrivate,
                                OMX_IN OMX_U32 nSizeBytes,
                                OMX_IN OMX_U8* pBuffer);

static OMX_ERRORTYPE VIDDEC_AllocateBuffer (OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
                                     OMX_IN OMX_U32 nPortIndex,
                                     OMX_IN OMX_PTR pAppPrivate,
                                     OMX_IN OMX_U32 nSizeBytes);

static OMX_ERRORTYPE VIDDEC_FreeBuffer (OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_IN OMX_U32 nPortIndex,
                                        OMX_IN OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE VIDDEC_ComponentDeInit (OMX_HANDLETYPE hComponent);

static OMX_ERRORTYPE VIDDEC_VerifyTunnelConnection (VIDDEC_PORT_TYPE *pPort,
                                                    OMX_HANDLETYPE hTunneledComp,
                                                    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef);

static OMX_ERRORTYPE VIDDEC_Allocate_DSPResources (OMX_IN VIDDEC_COMPONENT_PRIVATE *pComponentPrivate,
                                                   OMX_IN OMX_U32 nPortIndex);

static OMX_ERRORTYPE VIDDEC_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent,
                                              OMX_IN OMX_STRING cParameterName,
                                              OMX_OUT OMX_INDEXTYPE* pIndexType);

#ifdef KHRONOS_1_1
static OMX_ERRORTYPE ComponentRoleEnum(
                OMX_IN OMX_HANDLETYPE hComponent,
                OMX_OUT OMX_U8 *cRole,
                OMX_IN OMX_U32 nIndex);
#endif

/*----------------------------------------------------------------------------*/
/**
  * OMX_ComponentInit() Set the all the function pointers of component
  *
  * This method will update the component function pointer to the handle
  *
  * @param hComp         handle for this instance of the component
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_ErrorInsufficientResources If the malloc fails
  **/
/*----------------------------------------------------------------------------*/

OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_U32 nMemUsage = 0;
#ifdef __ENV_CHANGE__
    char EnvChangeValue[VIDDEC_MAX_NAMESIZE];
    char* EnvChangeValueu = NULL;
#endif

    //#ifdef ANDROID_QUIRK_LOCK_BUFFER
    OMX_U32 err;
    hw_module_t const* module;
    //#endif

    OMX_CONF_CHECK_CMD(hComponent, OMX_TRUE, OMX_TRUE);
    pHandle = (OMX_COMPONENTTYPE *)hComponent;

    OMX_MALLOC_STRUCT(pHandle->pComponentPrivate, VIDDEC_COMPONENT_PRIVATE, nMemUsage);
    if (pHandle->pComponentPrivate == NULL) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate;
    pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0] += nMemUsage;

#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate->pPERF = PERF_Create(PERF_FOURS("VD  "),
                                           PERF_ModuleLLMM | PERF_ModuleVideoDecode);
#endif
    OMX_DBG_INIT(pComponentPrivate->dbg, "OMX_DBG_VIDDEC");

    /* Print to verify that we are running a TI OMX codec*/
    OMX_PRINT4(pComponentPrivate->dbg, "TI Video Decoder \n");

    ((VIDDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->pHandle = pHandle;

    pHandle->SetCallbacks                   = VIDDEC_SetCallbacks;
    pHandle->GetComponentVersion            = VIDDEC_GetComponentVersion;
    pHandle->SendCommand                    = VIDDEC_SendCommand;
    pHandle->GetParameter                   = VIDDEC_GetParameter;
    pHandle->SetParameter                   = VIDDEC_SetParameter;
    pHandle->GetConfig                      = VIDDEC_GetConfig;
    pHandle->SetConfig                      = VIDDEC_SetConfig;
    pHandle->GetState                       = VIDDEC_GetState;
    pHandle->ComponentTunnelRequest         = VIDDEC_ComponentTunnelRequest;
    pHandle->UseBuffer                      = VIDDEC_UseBuffer;
    pHandle->AllocateBuffer                 = VIDDEC_AllocateBuffer;
    pHandle->FreeBuffer                     = VIDDEC_FreeBuffer;
    pHandle->EmptyThisBuffer                = VIDDEC_EmptyThisBuffer;
    pHandle->FillThisBuffer                 = VIDDEC_FillThisBuffer;
    pHandle->ComponentDeInit                = VIDDEC_ComponentDeInit;
    pHandle->GetExtensionIndex              = VIDDEC_GetExtensionIndex;
#ifdef KHRONOS_1_1
    pHandle->ComponentRoleEnum              = ComponentRoleEnum;
#endif
    pComponentPrivate->bUsePortReconfigForCrop = OMX_FALSE;
    pComponentPrivate->bUseThumbnail = OMX_FALSE;
    /*mutex protection*/
    if (pthread_mutex_init(&(pComponentPrivate->mutexInputBFromApp), NULL) != 0) {
        eError = OMX_ErrorUndefined;
        return eError;
    }
    if (pthread_mutex_init(&(pComponentPrivate->mutexOutputBFromApp), NULL) != 0) {
        eError = OMX_ErrorUndefined;
        return eError;
    }
    if (pthread_mutex_init(&(pComponentPrivate->mutexInputBFromDSP), NULL) != 0) {
        eError = OMX_ErrorUndefined;
        return eError;
    }
    if (pthread_mutex_init(&(pComponentPrivate->mutexOutputBFromDSP), NULL) != 0) {
        eError = OMX_ErrorUndefined;
        return eError;
    }
    OMX_MALLOC_STRUCT(pComponentPrivate->pPortParamType, OMX_PORT_PARAM_TYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
#ifdef __STD_COMPONENT__
    OMX_MALLOC_STRUCT(pComponentPrivate->pPortParamTypeAudio, OMX_PORT_PARAM_TYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pPortParamTypeImage, OMX_PORT_PARAM_TYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pPortParamTypeOthers, OMX_PORT_PARAM_TYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
#endif
    OMX_MALLOC_STRUCT(pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT], VIDDEC_PORT_TYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT], VIDDEC_PORT_TYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pInPortDef, OMX_PARAM_PORTDEFINITIONTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pOutPortDef, OMX_PARAM_PORTDEFINITIONTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pInPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pOutPortFormat, OMX_VIDEO_PARAM_PORTFORMATTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pPriorityMgmt, OMX_PRIORITYMGMTTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pInBufSupplier, OMX_PARAM_BUFFERSUPPLIERTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pOutBufSupplier, OMX_PARAM_BUFFERSUPPLIERTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pMpeg4, OMX_VIDEO_PARAM_MPEG4TYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pMpeg2, OMX_VIDEO_PARAM_MPEG2TYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pH264, OMX_VIDEO_PARAM_AVCTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pH263, OMX_VIDEO_PARAM_H263TYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pWMV, OMX_VIDEO_PARAM_WMVTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pDeblockingParamType, OMX_PARAM_DEBLOCKINGTYPE, pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pDeringingParamType, OMX_CONFIG_IMAGEFILTERTYPE, pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    OMX_MALLOC_STRUCT(pComponentPrivate->pPVCapabilityFlags, PV_OMXComponentCapabilityFlagsType, pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]); 

    OMX_MALLOC_STRUCT_SIZED(pComponentPrivate->cComponentName, char, VIDDEC_MAX_NAMESIZE + 1,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]);
    if (pComponentPrivate->cComponentName == NULL) {
        OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    strncpy(pComponentPrivate->cComponentName, cVideoDecodeName, VIDDEC_MAX_NAMESIZE);

    OMX_MALLOC_SIZE_DSPALIGN(pComponentPrivate->pBufferRCV.sStructRCV,
                             VIDDEC_MULTIPLE16(sizeof(VIDDEC_WMV_RCV_struct)),
                             VIDDEC_WMV_RCV_struct);
    if (pComponentPrivate->pBufferRCV.sStructRCV == NULL) {
        OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pComponentPrivate->pBufferRCV.pBuffer = (OMX_U8 *)pComponentPrivate->pBufferRCV.sStructRCV;

    OMX_CONF_INIT_STRUCT( &pComponentPrivate->componentRole, OMX_PARAM_COMPONENTROLETYPE, pComponentPrivate->dbg);
    VIDDEC_Load_Defaults( pComponentPrivate, VIDDEC_INIT_ALL);
#ifdef __ENV_CHANGE__
#ifdef KHRONOS_1_1
    EnvChangeValueu = getenv(ENV_CHANGE_NAME_VALUE);
    if(EnvChangeValueu != NULL) {
        strcpy( EnvChangeValue, EnvChangeValueu);
        if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_H263) == 0) {
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_H263);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)pComponentPrivate->componentRole.cRole, VIDDEC_COMPONENTROLES_H263);
        }
        else if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_H264) == 0 ||  strcmp( EnvChangeValueu, ENV_CHANGE_SET_AVC) == 0) {
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_H264);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)pComponentPrivate->componentRole.cRole, VIDDEC_COMPONENTROLES_H264);
        }
        else if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_MPEG2) == 0) {
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_MPEG2);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)pComponentPrivate->componentRole.cRole, VIDDEC_COMPONENTROLES_MPEG2);
        }
        else if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_WMV9) == 0) {
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_WMV9);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)pComponentPrivate->componentRole.cRole, VIDDEC_COMPONENTROLES_MPEG4);
        }
#ifdef VIDDEC_SPARK_CODE
        else if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_SPARK) == 0) {
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_SPARK);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)pComponentPrivate->componentRole.cRole, VIDDEC_COMPONENTROLES_H263);
        }
#endif
        else {
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_MPEG4);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)pComponentPrivate->componentRole.cRole, VIDDEC_COMPONENTROLES_WMV9);
        }
    }
    else
    {
        if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
        }
        strcpy((char*)pComponentPrivate->componentRole.cRole, VIDDEC_COMPONENTROLES_MPEG4);
    }
#else
    EnvChangeValueu = getenv(ENV_CHANGE_NAME_VALUE);
    if(EnvChangeValueu != NULL) {
        strcpy( EnvChangeValue, EnvChangeValueu);
        if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_H263) == 0) {
            VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_H263);
        }
        else if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_H264) == 0) {
            VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_H264);
        }
        else if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_MPEG2) == 0) {
            VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_MPEG2);
        }
        else if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_WMV9) == 0) {
            VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_WMV9);
        }
#ifdef VIDDEC_SPARK_CODE
        else if( strcmp( EnvChangeValueu, ENV_CHANGE_SET_SPARK) == 0) {
            VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_SPARK);
        }
#endif
        else {
            VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_MPEG4);
        }
        if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
        }
    }
#endif
#endif

#ifdef RESOURCE_MANAGER_ENABLED
    /* load the ResourceManagerProxy thread */
    eError = RMProxy_NewInitalizeEx(OMX_COMPONENTTYPE_VIDEO);
    if (eError != OMX_ErrorNone) {
        pComponentPrivate->eRMProxyState = VidDec_RMPROXY_State_Unload;
        OMX_PRMGR4(pComponentPrivate->dbg, "Error returned from loading ResourceManagerProxy thread\n");
        goto EXIT;
    }
    else{
        pComponentPrivate->eRMProxyState = VidDec_RMPROXY_State_Load;
    }
#endif

#ifdef USE_ION
    pComponentPrivate->bUseIon = OMX_TRUE;
    pComponentPrivate->bMapIonBuffers = OMX_TRUE;

    pComponentPrivate->ion_fd = ion_open();
	if(pComponentPrivate->ion_fd == 0)
	{
		OMX_ERROR4(pComponentPrivate->dbg, "ion_open failed!!!");
		return OMX_ErrorInsufficientResources;
	}

	pComponentPrivate->count_ion_buff = 0;

#endif

//#ifdef ANDROID_QUIRK_LOCK_BUFFER
	err = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module);
    if (err == 0)
	{
    	pComponentPrivate->grallocModule = (gralloc_module_t const *)module;
    }
	else
	{
         //DOMX_ERROR("FATAL: gralloc api hw_get_module() returned error: Can't find \
		 %s module err = %x", GRALLOC_HARDWARE_MODULE_ID, err);
		eError = OMX_ErrorInsufficientResources;
		return eError;
	}
//#endif

    /* Start the component thread */
    eError = VIDDEC_Start_ComponentThread(pHandle);
    if (eError != OMX_ErrorNone) {
        OMX_ERROR4(pComponentPrivate->dbg, "Error returned from the Component\n");
        goto EXIT;
    }
EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_SetCallbacks() Sets application callbacks to the component
  *
  * This method will update application callbacks 
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr           pointer to the appdata structure
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_SetCallbacks (OMX_HANDLETYPE pComponent, 
                                          OMX_CALLBACKTYPE* pCallBacks, 
                                          OMX_PTR pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;

    OMX_CONF_CHECK_CMD(pComponent, pCallBacks, OMX_TRUE);

    /* The following validates none of the callbacks is null.
     * There are 3 call backs defined by Open Max; in case
     * more are added, this code would need to be modified.
     * */
    OMX_CONF_CHECK_CMD(pCallBacks->EmptyBufferDone, pCallBacks->EventHandler,
                       pCallBacks->FillBufferDone);


    pHandle = (OMX_COMPONENTTYPE*)pComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    /* Copy the callbacks of the application to the component private  */
    memcpy (&(pComponentPrivate->cbInfo), pCallBacks, sizeof(OMX_CALLBACKTYPE));

    /* copy the application private data to component memory */
    pHandle->pApplicationPrivate = pAppData;

    pComponentPrivate->eState = OMX_StateLoaded;

EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_GetComponentVersion() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_GetComponentVersion (OMX_HANDLETYPE hComp,
                                                 OMX_STRING pComponentName,
                                                 OMX_VERSIONTYPE* pComponentVersion,
                                                 OMX_VERSIONTYPE* pSpecVersion,
                                                 OMX_UUIDTYPE* pComponentUUID)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;

    if (!hComp || !pComponentName || !pComponentVersion || !pSpecVersion) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pHandle = (OMX_COMPONENTTYPE*)hComp;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate;

    strcpy(pComponentName, pComponentPrivate->cComponentName);
    memcpy(pComponentVersion, &(pComponentPrivate->pComponentVersion.s), sizeof(pComponentPrivate->pComponentVersion.s));
    memcpy(pSpecVersion, &(pComponentPrivate->pSpecVersion.s), sizeof(pComponentPrivate->pSpecVersion.s));

    if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
        memcpy(pComponentUUID, (OMX_UUIDTYPE *)&STRING_H264VDSOCKET_TI_UUID, STRING_UUID_LENGHT);
    }
    else if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingWMV) {
        memcpy(pComponentUUID, (OMX_UUIDTYPE *)&STRING_WMVDSOCKET_TI_UUID, STRING_UUID_LENGHT);
    }
    else if ((pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4) ||
             (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263)) {
        memcpy(pComponentUUID, (OMX_UUIDTYPE *)&STRING_MP4DSOCKET_TI_UUID, STRING_UUID_LENGHT);
    }
    else if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG2) {
        memcpy(pComponentUUID, (OMX_UUIDTYPE *)&STRING_MP2DSOCKET_TI_UUID, STRING_UUID_LENGHT);
    }
#ifdef VIDDEC_SPARK_CODE
    else if (VIDDEC_SPARKCHECK) {
            memcpy(pComponentUUID, (OMX_UUIDTYPE *)&STRING_SPARKDSOCKET_TI_UUID, STRING_UUID_LENGHT);
    }
#endif
    else {
        memcpy(pComponentUUID, (OMX_UUIDTYPE *)&STRING_MP4DSOCKET_TI_UUID, STRING_UUID_LENGHT);
    }
EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_SendCommand() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/
static OMX_ERRORTYPE VIDDEC_SendCommand (OMX_HANDLETYPE hComponent,
                                         OMX_COMMANDTYPE Cmd,
                                         OMX_U32 nParam1,
                                         OMX_PTR pCmdData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_S32 nRet;
    OMX_COMPONENTTYPE* pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    OMX_CONF_CHECK_CMD(hComponent, OMX_TRUE, OMX_TRUE);

    pHandle = (OMX_COMPONENTTYPE*)hComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    if (pComponentPrivate->eState == OMX_StateInvalid) {
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }

#ifdef __PERF_INSTRUMENTATION__
    PERF_SendingCommand(pComponentPrivate->pPERF,
                        Cmd, (Cmd == OMX_CommandMarkBuffer) ? (OMX_U32) pCmdData : nParam1,
                        PERF_ModuleComponent);
#endif

    switch (Cmd) {
        case OMX_CommandStateSet:
            /* Add a pending transition */
            if(AddStateTransition(pComponentPrivate) != OMX_ErrorNone) {
                return OMX_ErrorUndefined;
            }
            pComponentPrivate->eIdleToLoad = nParam1;
            pComponentPrivate->eExecuteToIdle = nParam1;
            nRet = write(pComponentPrivate->cmdPipe[VIDDEC_PIPE_WRITE], &Cmd, sizeof(Cmd));
            if (nRet == -1) {
                if(RemoveStateTransition(pComponentPrivate, OMX_FALSE) != OMX_ErrorNone) {
                   return OMX_ErrorUndefined;
                }
                return OMX_ErrorUndefined;
            }
            nRet = write(pComponentPrivate->cmdDataPipe[VIDDEC_PIPE_WRITE], &nParam1, sizeof(nParam1));
            if (nRet == -1) {
                if(RemoveStateTransition(pComponentPrivate, OMX_FALSE) != OMX_ErrorNone) {
                   return OMX_ErrorUndefined;
                }
                return OMX_ErrorUndefined;
            }
            break;
        case OMX_CommandPortDisable:
            if (nParam1 == VIDDEC_INPUT_PORT) {
                if (pComponentPrivate->pInPortDef->bEnabled == OMX_TRUE) {
                    pComponentPrivate->pInPortDef->bEnabled = OMX_FALSE;
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Disabling VIDDEC_INPUT_PORT 0x%x\n",pComponentPrivate->pInPortDef->bEnabled);
                }
                else {
                    eError = OMX_ErrorIncorrectStateOperation;
                    goto EXIT;
                }
            }
            else if (nParam1 == VIDDEC_OUTPUT_PORT) {
                if (pComponentPrivate->pOutPortDef->bEnabled == OMX_TRUE) {
                    pComponentPrivate->pOutPortDef->bEnabled = OMX_FALSE;
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Disabling VIDDEC_OUTPUT_PORT 0x%x\n",pComponentPrivate->pOutPortDef->bEnabled);
                }
                else {
                    eError = OMX_ErrorIncorrectStateOperation;
                    goto EXIT;
                }
            }
            else if (nParam1 == OMX_ALL) {
                if (pComponentPrivate->pInPortDef->bEnabled == OMX_TRUE &&
                    pComponentPrivate->pOutPortDef->bEnabled == OMX_TRUE) {
                    pComponentPrivate->pInPortDef->bEnabled = OMX_FALSE;
                    pComponentPrivate->pOutPortDef->bEnabled = OMX_FALSE;
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Disabling OMX_ALL IN 0x%x OUT 0x%x\n",pComponentPrivate->pInPortDef->bEnabled,
                        pComponentPrivate->pOutPortDef->bEnabled);
                }
                else {
                    eError = OMX_ErrorIncorrectStateOperation;
                    goto EXIT;
                }
            }
            else {
                eError = OMX_ErrorBadParameter;
                goto EXIT;
            }
            nRet = write(pComponentPrivate->cmdPipe[VIDDEC_PIPE_WRITE], &Cmd, sizeof(Cmd));
            if (nRet == -1) {
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
            nRet = write(pComponentPrivate->cmdDataPipe[VIDDEC_PIPE_WRITE], &nParam1, sizeof(nParam1));
            if (nRet == -1) {
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
           break;
        case OMX_CommandPortEnable:
            if (nParam1 == VIDDEC_INPUT_PORT) {
                if (pComponentPrivate->pInPortDef->bEnabled == OMX_FALSE) {
                    pComponentPrivate->pInPortDef->bEnabled = OMX_TRUE;
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Enabling VIDDEC_INPUT_PORT 0x%x\n",pComponentPrivate->pInPortDef->bEnabled);
                }
                else {
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Port already Enabled VIDDEC_INPUT_PORT 0x%x\n",pComponentPrivate->pInPortDef->bEnabled);
                    goto EXIT;
                }
            }
            else if (nParam1 == VIDDEC_OUTPUT_PORT) {
                if (pComponentPrivate->pOutPortDef->bEnabled == OMX_FALSE) {
                    pComponentPrivate->pOutPortDef->bEnabled = OMX_TRUE;
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Enabling VIDDEC_OUTPUT_PORT 0x%x\n",pComponentPrivate->pOutPortDef->bEnabled);
                }
                else {
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Port already Enabled VIDDEC_OUTPUT_PORT 0x%x\n",pComponentPrivate->pOutPortDef->bEnabled);
                    goto EXIT;
                }
            }
            else if (nParam1 == OMX_ALL) {
                if (pComponentPrivate->pInPortDef->bEnabled == OMX_FALSE &&
                    pComponentPrivate->pOutPortDef->bEnabled == OMX_FALSE) {
                    pComponentPrivate->pInPortDef->bEnabled = OMX_TRUE;
                    pComponentPrivate->pOutPortDef->bEnabled = OMX_TRUE;
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Enabling VIDDEC_INPUT_PORT 0x%x\n",pComponentPrivate->pInPortDef->bEnabled);
                }
                else {
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Ports already Enabled VIDDEC_INPUT_PORT 0x%x\n",pComponentPrivate->pInPortDef->bEnabled);
                    goto EXIT;
                }
            }
            nRet = write(pComponentPrivate->cmdPipe[VIDDEC_PIPE_WRITE], &Cmd, sizeof(Cmd));
            if (nRet == -1) {
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
            nRet = write(pComponentPrivate->cmdDataPipe[VIDDEC_PIPE_WRITE], &nParam1, sizeof(nParam1));
            if (nRet == -1) {
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
            break;
        case OMX_CommandFlush:
            if ( nParam1 > 1 && nParam1 != OMX_ALL ) {
                eError = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
            nRet = write(pComponentPrivate->cmdPipe[VIDDEC_PIPE_WRITE], &Cmd, sizeof(Cmd));
            if (nRet == -1) {
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
            nRet = write(pComponentPrivate->cmdDataPipe[VIDDEC_PIPE_WRITE], &nParam1, sizeof(nParam1));
            if (nRet == -1) {
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
            break;
        case OMX_CommandMarkBuffer:
            if ( nParam1 > VIDDEC_OUTPUT_PORT ){
                eError = OMX_ErrorBadPortIndex;
                goto EXIT;
            }
            nRet = write(pComponentPrivate->cmdPipe[VIDDEC_PIPE_WRITE], &Cmd, sizeof(Cmd));
            if (nRet == -1) {
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
            nRet = write(pComponentPrivate->cmdDataPipe[VIDDEC_PIPE_WRITE], &nParam1, sizeof(nParam1));
            if (nRet == -1) {
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
            nRet = write(pComponentPrivate->cmdDataPipe[VIDDEC_PIPE_WRITE], &pCmdData, sizeof(pCmdData));
            if (nRet == -1) {
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
            break;
        case OMX_CommandMax:
            break;
        default:
            eError = OMX_ErrorUndefined;
    }

EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_GetParameter() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_GetParameter (OMX_IN OMX_HANDLETYPE hComponent,
                                          OMX_IN  OMX_INDEXTYPE nParamIndex,
                                          OMX_INOUT OMX_PTR ComponentParameterStructure)
{
    OMX_COMPONENTTYPE* pComp = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    OMX_TI_PARAMNATIVEBUFFERUSAGE *pUsage = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
#ifdef KHRONOS_1_1
    OMX_PARAM_COMPONENTROLETYPE *pRole = NULL;
#endif
    OMX_CONF_CHECK_CMD(hComponent, ComponentParameterStructure, OMX_TRUE);

    pComp = (OMX_COMPONENTTYPE*)hComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pComp->pComponentPrivate;

    /*add check if state != loaded or port not disabled*/
    if (pComponentPrivate->eState == OMX_StateInvalid) {
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    switch (nParamIndex) {

        case VideoDecodeGetAndroidNativeBufferUsage:
        {
    	   pUsage = (OMX_TI_PARAMNATIVEBUFFERUSAGE*)ComponentParameterStructure;
    	   		if(pComponentPrivate->pCompPort[pUsage->nPortIndex]->VIDDECBufferType == GrallocPointers)
    	   		{
    	   			//PROXY_CHK_VERSION(pParamStruct, OMX_TI_PARAMNATIVEBUFFERUSAGE);
    	   			pUsage->nUsage = GRALLOC_USAGE_HW_RENDER;
    	   			goto EXIT;
    	   		}
    	   	break;
        }
        case VideoDecodeGetAndroiduseAndroidNativeBuffer2 :
    	  break;

        case OMX_IndexConfigVideoMBErrorReporting: /**< reference: OMX_CONFIG_MBERRORREPORTINGTYPE */
        {
            if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
                pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263 ||
                pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
                OMX_CONFIG_MBERRORREPORTINGTYPE* pMBErrorReportTo = ComponentParameterStructure;
                pMBErrorReportTo->bEnabled = pComponentPrivate->eMBErrorReport.bEnabled;
            }
            else {
                eError = OMX_ErrorUnsupportedIndex;
            }
            break;
        }
        case OMX_IndexParamVideoInit:
            memcpy(ComponentParameterStructure, pComponentPrivate->pPortParamType, sizeof(OMX_PORT_PARAM_TYPE));
            break;
#ifdef __STD_COMPONENT__
        case OMX_IndexParamAudioInit:
            memcpy(ComponentParameterStructure, pComponentPrivate->pPortParamTypeAudio, sizeof(OMX_PORT_PARAM_TYPE));
            break;
        case OMX_IndexParamImageInit:
            memcpy(ComponentParameterStructure, pComponentPrivate->pPortParamTypeImage, sizeof(OMX_PORT_PARAM_TYPE));
            break;
        case OMX_IndexParamOtherInit:
            memcpy(ComponentParameterStructure, pComponentPrivate->pPortParamTypeOthers, sizeof(OMX_PORT_PARAM_TYPE));
            break;
#ifdef KHRONOS_1_1
        case OMX_IndexParamVideoMacroblocksPerFrame:/**< reference: OMX_PARAM_MACROBLOCKSTYPE */
        {
            OMX_PARAM_MACROBLOCKSTYPE* pMBBlocksTypeTo = ComponentParameterStructure;
            pMBBlocksTypeTo->nMacroblocks = pComponentPrivate->pOutPortDef->format.video.nFrameWidth *
                                            pComponentPrivate->pOutPortDef->format.video.nFrameHeight / 256;
            break;
        }
        case OMX_IndexParamVideoProfileLevelQuerySupported:
            {
                VIDEO_PROFILE_LEVEL_TYPE* pProfileLevel = NULL;
                OMX_VIDEO_PARAM_PROFILELEVELTYPE *pParamProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)ComponentParameterStructure;
                pParamProfileLevel->nPortIndex = pComponentPrivate->pInPortDef->nPortIndex;

                /* Choose table based on compression format */
                switch(pComponentPrivate->pInPortDef->format.video.eCompressionFormat)
                {
                   case OMX_VIDEO_CodingH263:
                      pProfileLevel = SupportedH263ProfileLevels;
                      break;
                   case OMX_VIDEO_CodingMPEG4:
                      pProfileLevel = SupportedMPEG4ProfileLevels;
                      break;
                   case OMX_VIDEO_CodingAVC:
                      pProfileLevel = SupportedAVCProfileLevels;
                      break;
                   default:
                      break;
                  }

                  /* Point to table entry based on index */
                  pProfileLevel += pParamProfileLevel->nProfileIndex;

                  /* -1 indicates end of table */
                  if(pProfileLevel->nProfile != -1) {
                     pParamProfileLevel->eProfile = pProfileLevel->nProfile;
                     pParamProfileLevel->eLevel = pProfileLevel->nLevel;
                     eError = OMX_ErrorNone;
                  }
                  else {
                     eError = OMX_ErrorNoMore;
                  }
                  break;
                  }
        case OMX_IndexParamVideoProfileLevelCurrent:
        {
           OMX_VIDEO_PARAM_PROFILELEVELTYPE *pParamProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)ComponentParameterStructure;
           if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
           OMX_PRINT2(pComponentPrivate->dbg, " Getparameter OMX_IndexParamVideoProfileLevelCurrent AVC \n");
           pParamProfileLevel->eProfile = pComponentPrivate->pH264->eProfile;
           pParamProfileLevel->eLevel = pComponentPrivate->pH264->eLevel;
        }
        else if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4) {
           OMX_PRINT2(pComponentPrivate->dbg, " Getparameter OMX_IndexParamVideoProfileLevelCurrent MPEG4 \n");
           pParamProfileLevel->eProfile = pComponentPrivate->pMpeg4->eProfile;
           pParamProfileLevel->eLevel = pComponentPrivate->pMpeg4->eLevel;
        }
        else if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263) {
           OMX_PRINT2(pComponentPrivate->dbg, "Getparameter OMX_IndexParamVideoProfileLevelCurrent H263 \n");
           pParamProfileLevel->eProfile = pComponentPrivate->pH263->eProfile;
           pParamProfileLevel->eLevel = pComponentPrivate->pH263->eLevel;
        }
        else if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG2) {
           OMX_PRINT2(pComponentPrivate->dbg, "Getparameter OMX_IndexParamVideoProfileLevelCurrent MPEG2 \n");
           pParamProfileLevel->eProfile = pComponentPrivate->pMpeg2->eProfile;
           pParamProfileLevel->eLevel = pComponentPrivate->pMpeg2->eLevel;
        }
        else {
           OMX_ERROR4(pComponentPrivate->dbg, "Error in Getparameter OMX_IndexParamVideoProfileLevelCurrent \n");
           eError = OMX_ErrorBadParameter;
         }
      }
      break;
        case OMX_IndexParamStandardComponentRole:
            if (ComponentParameterStructure != NULL) {
                pRole = (OMX_PARAM_COMPONENTROLETYPE *)ComponentParameterStructure;
                memcpy( pRole, &pComponentPrivate->componentRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
            }
            else {
                eError = OMX_ErrorBadParameter;
            }
            break;
#endif
#endif
        case OMX_IndexParamPortDefinition:
            {
                if (((OMX_PARAM_PORTDEFINITIONTYPE*)(ComponentParameterStructure))->nPortIndex ==
                        pComponentPrivate->pInPortDef->nPortIndex) {
                    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = pComponentPrivate->pInPortDef;
                    OMX_PARAM_PORTDEFINITIONTYPE *pPortDefParam = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
                    memcpy(pPortDefParam, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                }
                else if (((OMX_PARAM_PORTDEFINITIONTYPE*)(ComponentParameterStructure))->nPortIndex ==
                        pComponentPrivate->pOutPortDef->nPortIndex) {
                    OMX_PARAM_PORTDEFINITIONTYPE *pPortDefParam = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure;
                    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = pComponentPrivate->pOutPortDef;
                    memcpy(pPortDefParam, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }

                OMX_PRBUFFER1(pComponentPrivate->dbg, "CountActual 0x%x CountMin 0x%x Size %d bEnabled %x bPopulated %x compression %x %x-%x\n",
                                (int )((OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure)->nBufferCountActual,
                                (int )((OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure)->nBufferCountMin,
                                (int )((OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure)->nBufferSize,
                                (int )((OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure)->bEnabled,
                                (int )((OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure)->bPopulated,
                                (int )((OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure)->format.video.nFrameWidth,
                                (int )((OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure)->format.video.nFrameHeight,
                                (int )((OMX_PARAM_PORTDEFINITIONTYPE *)ComponentParameterStructure)->format.video.eCompressionFormat);
            }
            break;
        case OMX_IndexParamVideoPortFormat:
            {
                OMX_VIDEO_PARAM_PORTFORMATTYPE* pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)ComponentParameterStructure;
                if (pPortFormat->nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
                    switch (pPortFormat->nIndex) {
                        case VIDDEC_DEFAULT_INPUT_INDEX_H263:
                            OMX_PRINT1(pComponentPrivate->dbg, "eCompressionFormat = OMX_VIDEO_CodingH263\n");
                            pComponentPrivate->pInPortFormat->nIndex                        = VIDDEC_DEFAULT_INPUT_INDEX_H263;
                            pComponentPrivate->pInPortFormat->eCompressionFormat            = OMX_VIDEO_CodingH263;
                            pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingH263;
                            pComponentPrivate->bIsSparkInput                                = OMX_FALSE;
                            break;
                        case VIDDEC_DEFAULT_INPUT_INDEX_H264:
                            OMX_PRINT1(pComponentPrivate->dbg, "eCompressionFormat = OMX_VIDEO_CodingAVC\n");
                            pComponentPrivate->pInPortFormat->nIndex                        = VIDDEC_DEFAULT_INPUT_INDEX_H264;
                            pComponentPrivate->pInPortFormat->eCompressionFormat            = OMX_VIDEO_CodingAVC;
                            pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingAVC;
                            pComponentPrivate->bIsSparkInput                                = OMX_FALSE;
                            break;
                        case VIDDEC_DEFAULT_INPUT_INDEX_MPEG2:
                            pComponentPrivate->pInPortFormat->nIndex                        = VIDDEC_DEFAULT_INPUT_INDEX_MPEG2;
                            pComponentPrivate->pInPortFormat->eCompressionFormat            = OMX_VIDEO_CodingMPEG2;
                            pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingMPEG2;
                            pComponentPrivate->bIsSparkInput                                = OMX_FALSE;
                            break;
                        case VIDDEC_DEFAULT_INPUT_INDEX_MPEG4:
                            OMX_PRINT1(pComponentPrivate->dbg, "eCompressionFormat = VIDDEC_DEFAULT_INPUT_INDEX_MPEG4\n");
                            pComponentPrivate->pInPortFormat->nIndex                        = VIDDEC_DEFAULT_INPUT_INDEX_MPEG4;
                            pComponentPrivate->pInPortFormat->eCompressionFormat            = OMX_VIDEO_CodingMPEG4;
                            pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingMPEG4;
                            pComponentPrivate->bIsSparkInput                                = OMX_FALSE;
                            break;
                        case VIDDEC_DEFAULT_INPUT_INDEX_WMV9:
                            pComponentPrivate->pInPortFormat->nIndex                        = VIDDEC_DEFAULT_INPUT_INDEX_WMV9;
                            pComponentPrivate->pInPortFormat->eCompressionFormat            = OMX_VIDEO_CodingWMV;
                            pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingWMV;
                            pComponentPrivate->bIsSparkInput                                = OMX_FALSE;
                            break;
#ifdef VIDDEC_SPARK_CODE
                        case VIDDEC_DEFAULT_INPUT_INDEX_SPARK:
                            pComponentPrivate->pInPortFormat->nIndex                        = VIDDEC_DEFAULT_INPUT_INDEX_SPARK;
                            pComponentPrivate->pInPortFormat->eCompressionFormat            = OMX_VIDEO_CodingUnused;
                            pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingUnused;
                            pComponentPrivate->bIsSparkInput                                = OMX_TRUE;
                            break;
#endif
                        default:
                            OMX_PRINT1(pComponentPrivate->dbg, "Input Index= %lu; OMX_ErrorNoMore\n", pPortFormat->nIndex);
                            eError = OMX_ErrorNoMore;
                            break;
                    }
                    if(eError == OMX_ErrorNone) {
                        memcpy(ComponentParameterStructure, pComponentPrivate->pInPortFormat, 
                                sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                    }
                }
                else if (pPortFormat->nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
                    switch (pPortFormat->nIndex) {
                        case VIDDEC_DEFAULT_OUTPUT_INDEX_INTERLEAVED422:
                            pComponentPrivate->pOutPortFormat->nIndex                        = VIDDEC_DEFAULT_OUTPUT_INDEX_INTERLEAVED422;
                            pComponentPrivate->pOutPortFormat->eColorFormat                  = VIDDEC_COLORFORMAT422;
                            pComponentPrivate->pOutPortDef->format.video.eColorFormat        = VIDDEC_COLORFORMAT422;
                            break;
                        case VIDDEC_DEFAULT_OUTPUT_INDEX_PLANAR420:
                            pComponentPrivate->pOutPortFormat->nIndex                        = VIDDEC_DEFAULT_OUTPUT_INDEX_PLANAR420;
                            pComponentPrivate->pOutPortFormat->eColorFormat                  = VIDDEC_COLORFORMAT420;
                            pComponentPrivate->pOutPortDef->format.video.eColorFormat        = VIDDEC_COLORFORMAT420;
                            break;
                        default:
                            eError = OMX_ErrorNoMore;
                            break;
                    }
                    if(eError == OMX_ErrorNone) {
                        memcpy(ComponentParameterStructure, pComponentPrivate->pOutPortFormat, 
                            sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                    }
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamPriorityMgmt:
            memcpy(ComponentParameterStructure, pComponentPrivate->pPriorityMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
            break;
        case  OMX_IndexParamVideoWmv:
            {
                if (((OMX_VIDEO_PARAM_WMVTYPE*)(ComponentParameterStructure))->nPortIndex ==
                        pComponentPrivate->pWMV->nPortIndex) {
                    memcpy(ComponentParameterStructure, pComponentPrivate->pWMV, sizeof(OMX_VIDEO_PARAM_WMVTYPE));
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case  OMX_IndexParamVideoMpeg4:
            {
                if (((OMX_VIDEO_PARAM_MPEG4TYPE*)(ComponentParameterStructure))->nPortIndex ==
                        pComponentPrivate->pMpeg4->nPortIndex) {
                    memcpy(ComponentParameterStructure, pComponentPrivate->pMpeg4, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case  OMX_IndexParamVideoMpeg2:
            {
                if (((OMX_VIDEO_PARAM_MPEG2TYPE*)(ComponentParameterStructure))->nPortIndex ==
                        pComponentPrivate->pMpeg2->nPortIndex) {
                    memcpy(ComponentParameterStructure, pComponentPrivate->pMpeg2, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoAvc:
            {

                if (((OMX_VIDEO_PARAM_AVCTYPE*)(ComponentParameterStructure))->nPortIndex ==
                        pComponentPrivate->pH264->nPortIndex) {
                    memcpy(ComponentParameterStructure, pComponentPrivate->pH264, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoH263:
            {

                if (((OMX_VIDEO_PARAM_H263TYPE*)(ComponentParameterStructure))->nPortIndex ==
                        pComponentPrivate->pH263->nPortIndex) {
                    memcpy(ComponentParameterStructure, pComponentPrivate->pH263, sizeof(OMX_VIDEO_PARAM_H263TYPE));
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamCompBufferSupplier:
            {
                OMX_PARAM_BUFFERSUPPLIERTYPE* pBuffSupplierParam = (OMX_PARAM_BUFFERSUPPLIERTYPE*)ComponentParameterStructure;

                if (pBuffSupplierParam->nPortIndex == 1) {
                    pBuffSupplierParam->eBufferSupplier = pComponentPrivate->pCompPort[pBuffSupplierParam->nPortIndex]->eSupplierSetting;
                }
                else if (pBuffSupplierParam->nPortIndex == 0) {
                    pBuffSupplierParam->eBufferSupplier = pComponentPrivate->pCompPort[pBuffSupplierParam->nPortIndex]->eSupplierSetting;
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                    break;
                }
            }
            break;
        case VideoDecodeCustomParamProcessMode:
            *((OMX_U32 *)ComponentParameterStructure) = pComponentPrivate->ProcessMode;
            break;
        case VideoDecodeCustomParamParserEnabled:
            *((OMX_BOOL *)ComponentParameterStructure) = pComponentPrivate->bParserEnabled;
            break;
        case VideoDecodeCustomParamH264BitStreamFormat:
            *((OMX_U32 *)ComponentParameterStructure) = pComponentPrivate->H264BitStreamFormat;
            break;
        case VideoDecodeCustomParamWMVProfile:
            {
                *((VIDDEC_WMV_PROFILES *)ComponentParameterStructure) = pComponentPrivate->wmvProfile;
            }
            break;
        case VideoDecodeCustomParamWMVFileType:
            *((OMX_U32 *)ComponentParameterStructure) = pComponentPrivate->nWMVFileType;
            break;
        case VideoDecodeCustomParamIsNALBigEndian:
            *((OMX_BOOL *)ComponentParameterStructure) = pComponentPrivate->bIsNALBigEndian;

            break;
#ifdef VIDDEC_SPARK_CODE
        case VideoDecodeCustomParamIsSparkInput:
            *((OMX_U32 *)ComponentParameterStructure) = pComponentPrivate->bIsSparkInput;

            break;
#endif
        case OMX_IndexParamCommonDeblocking: /**< reference: OMX_PARAM_DEBLOCKINGTYPE */
        {
            memcpy(ComponentParameterStructure, pComponentPrivate->pDeblockingParamType, sizeof(OMX_PARAM_DEBLOCKINGTYPE));
            break;
        }
        case OMX_IndexConfigCommonImageFilter: /**< reference: OMX_CONFIG_IMAGEFILTERTYPE */
        {
            memcpy(ComponentParameterStructure, pComponentPrivate->pDeringingParamType, sizeof(OMX_CONFIG_IMAGEFILTERTYPE));
            break;
        }
#ifdef ANDROID
        /* Opencore specific */
        case (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX: /** Obtain the capabilities of the OMX component **/
                memcpy(ComponentParameterStructure, pComponentPrivate->pPVCapabilityFlags, 
                        sizeof(PV_OMXComponentCapabilityFlagsType));
                eError = OMX_ErrorNone;
            break;
#endif
        default:
            eError = OMX_ErrorUnsupportedIndex;
            break;
        }

EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_CheckSetParameter() checks when it is valid calling OMX_SetParameter
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComponentPrivate         handle for this instance of the component
  * @param pCompParam                pointer to the parameter structure
  * @param nParamIndex               parameter index
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_ErrorIncorrectStateOperation   if the checks fails
  **/
/*----------------------------------------------------------------------------*/

OMX_ERRORTYPE VIDDEC_CheckSetParameter(VIDDEC_COMPONENT_PRIVATE* pComponentPrivate, OMX_PTR pCompParam, OMX_INDEXTYPE nParamIndex) {
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (pComponentPrivate->eState == OMX_StateInvalid) {
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if (pComponentPrivate->eState != OMX_StateLoaded && pComponentPrivate->eState != OMX_StateWaitForResources) {
        /*using OMX_CONFIG_ROTATIONTYPE because it is smallest structure that contains nPortIndex;*/
        OMX_CONFIG_ROTATIONTYPE* pTempFormat = (OMX_CONFIG_ROTATIONTYPE*)pCompParam;

        switch (nParamIndex) {
            /*the indices corresponding to the parameter structures containing the field "nPortIndex"*/
            case OMX_IndexParamCompBufferSupplier:
            case OMX_IndexParamVideoPortFormat:
            case OMX_IndexParamPortDefinition:
            case OMX_IndexParamVideoWmv:
            case OMX_IndexParamVideoMpeg4:
            case OMX_IndexParamVideoMpeg2:
            case OMX_IndexParamVideoAvc:
            case OMX_IndexParamVideoH263:
            case OMX_IndexConfigVideoMBErrorReporting:
            case OMX_IndexParamCommonDeblocking:
            case OMX_IndexConfigCommonImageFilter:
            case VideoDecodeEnableAndroidNativeBuffers:
            	if (pTempFormat->nPortIndex ==  pComponentPrivate->pInPortDef->nPortIndex) {
                    if (pComponentPrivate->pInPortDef->bEnabled){
                        eError = OMX_ErrorIncorrectStateOperation;
                        goto EXIT;
                    }
                }
                else if (pTempFormat->nPortIndex == pComponentPrivate->pOutPortDef->nPortIndex) {
                    if (pComponentPrivate->pOutPortDef->bEnabled){
                        eError = OMX_ErrorIncorrectStateOperation;
                        goto EXIT;
                    }
                }/*it cannot be -1 because structure assignment will happen on one port*/
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
                break;
            default:
                /*all other cases where pCompParam is integer or it doesn't support nPortIndex*/
                if (!(pComponentPrivate->pInPortDef->bEnabled == OMX_FALSE ||
                    pComponentPrivate->pOutPortDef->bEnabled == OMX_FALSE)) {
                    if(eError != OMX_ErrorNone) {
                        eError = OMX_ErrorIncorrectStateOperation;
                        goto EXIT;
                    }
                }
        }
    }
EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_SetParameter() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_SetParameter (OMX_HANDLETYPE hComp, 
                                          OMX_INDEXTYPE nParamIndex,
                                          OMX_PTR pCompParam)
{
    OMX_COMPONENTTYPE* pHandle= NULL;
    VIDDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

#ifdef KHRONOS_1_1
    OMX_PARAM_COMPONENTROLETYPE *pRole = NULL;
#endif
    OMX_CONF_CHECK_CMD(hComp, pCompParam, OMX_TRUE);
    pHandle= (OMX_COMPONENTTYPE*)hComp;
    pComponentPrivate = pHandle->pComponentPrivate;

#ifdef ENABLE_GRALLOC_BUFFERS
	OMX_TI_PARAMUSENATIVEBUFFER *pParamNativeBuffer = NULL;
#endif

    eError = VIDDEC_CheckSetParameter(pComponentPrivate, pCompParam, nParamIndex);
    if(eError != OMX_ErrorNone) {
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

     switch (nParamIndex) {

#ifdef ENABLE_GRALLOC_BUFFERS
        case VideoDecodeEnableAndroidNativeBuffers:
        {
            ALOGV(" In VideoDecodeEnableAndroidNativeBuffers in VIDDEC_SetParameter\n");
            pParamNativeBuffer = (OMX_TI_PARAMUSENATIVEBUFFER* )pCompParam;
			if(pParamNativeBuffer->bEnable == OMX_TRUE)
			{
				ALOGV("IN VIDDEC_SetParameter, in if-condition\n ");
				pComponentPrivate->pCompPort[pParamNativeBuffer->nPortIndex]->VIDDECBufferType = GrallocPointers;
				pComponentPrivate->pCompPort[pParamNativeBuffer->nPortIndex]->IsBuffer2D = OMX_TRUE;
			}
			break;
        }
#endif

        case OMX_IndexParamVideoPortFormat:
            {
                OMX_VIDEO_PARAM_PORTFORMATTYPE* pPortFormat = (OMX_VIDEO_PARAM_PORTFORMATTYPE*)pCompParam;
                if (pPortFormat->nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
                    if(pPortFormat->eColorFormat == OMX_COLOR_FormatUnused) {
                        switch (pPortFormat->eCompressionFormat) {
                            case OMX_VIDEO_CodingH263:
                                pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingH263;
                                break;
                            case OMX_VIDEO_CodingAVC:
                                pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingAVC;
                                OMX_PRINT1(pComponentPrivate->dbg, "eCompressionFormat = OMX_VIDEO_CodingAVC\n");
                                break;
                            case OMX_VIDEO_CodingMPEG2:
                                pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingMPEG2;
                                break;
                            case OMX_VIDEO_CodingMPEG4:
                                OMX_PRINT1(pComponentPrivate->dbg, "eCompressionFormat = OMX_VIDEO_CodingMPEG4\n");
                                pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingMPEG4;
                                break;
                            case OMX_VIDEO_CodingWMV:
                                OMX_PRINT1(pComponentPrivate->dbg, "eCompressionFormat = OMX_VIDEO_CodingWMV\n");
                                pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingWMV;
                                break;

#ifdef VIDDEC_SPARK_CODE
                            case OMX_VIDEO_CodingUnused:
                                if (pComponentPrivate->bIsSparkInput) {
                                    pComponentPrivate->pInPortDef->format.video.eCompressionFormat  = OMX_VIDEO_CodingUnused;
                                }
                                else {
                                    eError = OMX_ErrorNoMore;
                                }
                                break;
#endif
                            default:
                                eError = OMX_ErrorNoMore;
                                break;
                        }
                    }
                    else {
                        eError = OMX_ErrorBadParameter;
                    }
                    if(eError == OMX_ErrorNone) {
                        memcpy(pComponentPrivate->pInPortFormat, pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                    }
                }
                else if (pPortFormat->nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
                    if(pPortFormat->eCompressionFormat == OMX_VIDEO_CodingUnused) {
                        switch (pPortFormat->eColorFormat) {
                            case OMX_COLOR_FormatYUV420Planar:
                                pComponentPrivate->pOutPortDef->format.video.eColorFormat = VIDDEC_COLORFORMAT420;
                                break;
                            case OMX_COLOR_FormatCbYCrY:
                                pComponentPrivate->pOutPortDef->format.video.eColorFormat = VIDDEC_COLORFORMAT422;
                                break;
                            default:
                                eError = OMX_ErrorNoMore;
                                break;
                        }
                    }
                    else {
                        eError = OMX_ErrorBadParameter;
                    }
                    if(eError == OMX_ErrorNone) {
                        memcpy(pComponentPrivate->pOutPortFormat, pPortFormat, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
                    }
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoInit:
            memcpy(pComponentPrivate->pPortParamType, (OMX_PORT_PARAM_TYPE*)pCompParam, sizeof(OMX_PORT_PARAM_TYPE));
            break;
#ifdef __STD_COMPONENT__
        case OMX_IndexParamAudioInit:
            memcpy(pComponentPrivate->pPortParamTypeAudio, (OMX_PORT_PARAM_TYPE*)pCompParam, sizeof(OMX_PORT_PARAM_TYPE));
            break;
        case OMX_IndexParamImageInit:
            memcpy(pComponentPrivate->pPortParamTypeImage, (OMX_PORT_PARAM_TYPE*)pCompParam, sizeof(OMX_PORT_PARAM_TYPE));
            break;
        case OMX_IndexParamOtherInit:
            memcpy(pComponentPrivate->pPortParamTypeOthers, (OMX_PORT_PARAM_TYPE*)pCompParam, sizeof(OMX_PORT_PARAM_TYPE));
            break;
#endif
        case OMX_IndexParamPortDefinition:
            {
                OMX_PARAM_PORTDEFINITIONTYPE* pComponentParam = (OMX_PARAM_PORTDEFINITIONTYPE*)pCompParam;
                if (pComponentParam->nPortIndex == pComponentPrivate->pInPortDef->nPortIndex) {
                    OMX_PARAM_PORTDEFINITIONTYPE *pPortDefParam = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParam;
                    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = pComponentPrivate->pInPortDef;

                    /* Lets stagefright define the min buffer size of the input data
                       Only if the given size is zero or for WMV where OC is used, VD set the size */
                    if (((pPortDefParam->nBufferSize == pPortDef->nBufferSize) &&
                        (pPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingWMV))||
                        (pPortDefParam->nBufferSize == 0)) {
                            pPortDefParam->nBufferSize = pPortDefParam->format.video.nFrameWidth *
                                                         pPortDefParam->format.video.nFrameHeight;
                    }

                    memcpy(pPortDef, pPortDefParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                    switch (pPortDef->format.video.eCompressionFormat) {
                        case OMX_VIDEO_CodingH263:
                            pComponentPrivate->pInPortFormat->nIndex = VIDDEC_DEFAULT_INPUT_INDEX_H263;
                            pComponentPrivate->pInPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
                            break;
                        case OMX_VIDEO_CodingAVC:
                            pComponentPrivate->pInPortFormat->nIndex = VIDDEC_DEFAULT_INPUT_INDEX_H264;
                            pComponentPrivate->pInPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
                            break;
                        case OMX_VIDEO_CodingMPEG2:
                            pComponentPrivate->pInPortFormat->nIndex = VIDDEC_DEFAULT_INPUT_INDEX_MPEG2;
                            pComponentPrivate->pInPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
                            break;
                        case OMX_VIDEO_CodingMPEG4:
                            pComponentPrivate->pInPortFormat->nIndex = VIDDEC_DEFAULT_INPUT_INDEX_MPEG4;
                            pComponentPrivate->pInPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
                            break;
                        case OMX_VIDEO_CodingWMV:
                            pComponentPrivate->pInPortFormat->nIndex = VIDDEC_DEFAULT_INPUT_INDEX_WMV9;
                            pComponentPrivate->pInPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
                            break;
#ifdef VIDDEC_SPARK_CODE
                        case OMX_VIDEO_CodingUnused:
                            pComponentPrivate->pInPortFormat->nIndex = VIDDEC_DEFAULT_INPUT_INDEX_SPARK;
                            pComponentPrivate->pInPortFormat->eCompressionFormat = pPortDef->format.video.eCompressionFormat;
                            break;
#endif
                        default:
                            break;
                    }
                    /* Verify if the resolution is supported by the
                       component, depending in the format the limitations may vary */
                    eError = IsResolutionSupported(pPortDef->format.video.nFrameWidth,
                                                    pPortDef->format.video.nFrameHeight,
                                                    pComponentPrivate);
                    if (eError != OMX_ErrorNone) {
                        goto EXIT;
                    }
                    OMX_PRINT3(pComponentPrivate->dbg, "Set i/p resolution: %ldx%ld, nBufferSize: %ld",
                                pPortDefParam->format.video.nFrameWidth, pPortDefParam->format.video.nFrameHeight, pPortDef->nBufferSize);
                }
                else if (pComponentParam->nPortIndex == pComponentPrivate->pOutPortDef->nPortIndex) {
                    OMX_U32 nMinOutputBufferSize = 0;
                    OMX_U32 nWidth = 0;
                    OMX_PARAM_PORTDEFINITIONTYPE *pPortDefParam = (OMX_PARAM_PORTDEFINITIONTYPE *)pComponentParam;
                    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = pComponentPrivate->pOutPortDef;
                    if( pPortDefParam->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
                        if (pPortDef->format.video.nFrameWidth < 32) {
                            eError = OMX_ErrorUnsupportedSetting;
                            goto EXIT;
                        }
                    }
                    memcpy(pPortDef, pPortDefParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                    switch (pPortDef->format.video.eColorFormat) {
                        case VIDDEC_COLORFORMAT422:
                            OMX_PRINT3(pComponentPrivate->dbg, "Setting VIDDEC_COLORFORMAT422");
                            pComponentPrivate->pOutPortFormat->nIndex = VIDDEC_DEFAULT_OUTPUT_INDEX_INTERLEAVED422;
                            pComponentPrivate->pOutPortFormat->eColorFormat = pPortDef->format.video.eColorFormat;
                            break;
                        case VIDDEC_COLORFORMAT420:
                            OMX_PRINT3(pComponentPrivate->dbg, "Setting VIDDEC_COLORFORMAT420");
                            pComponentPrivate->pOutPortFormat->nIndex = VIDDEC_DEFAULT_OUTPUT_INDEX_PLANAR420;
                            pComponentPrivate->pOutPortFormat->eColorFormat = pPortDef->format.video.eColorFormat;
                            break;
                        default:
                            OMX_PRINT3(pComponentPrivate->dbg, "The color format is unknow");
                            break;
                    }

                if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingWMV) {
						nWidth = pPortDef->format.video.nFrameWidth;
						nWidth = VIDDEC_MULTIPLE32(nWidth);
						pPortDef->format.video.nFrameWidth = nWidth;
						pComponentPrivate->pOutPortDef->format.video.nFrameWidth = nWidth;
						pPortDefParam->format.video.nFrameWidth = nWidth;
                    }

                    /* OMX VD defines the minimum required output size */
                    nMinOutputBufferSize = pPortDef->format.video.nFrameWidth *
                        pPortDef->format.video.nFrameHeight *
                        ((pPortDef->format.video.eColorFormat == VIDDEC_COLORFORMAT420) ? VIDDEC_FACTORFORMAT420 : VIDDEC_FACTORFORMAT422);
                    pPortDef->nBufferSize = nMinOutputBufferSize;
                    pComponentPrivate->pOutPortDef->nBufferSize = pPortDef->nBufferSize;
                    OMX_PRINT3(pComponentPrivate->dbg, "Set OUT/p resolution: %ldx%ld, nBufferSize: %ld", pPortDefParam->format.video.nFrameWidth, pPortDefParam->format.video.nFrameHeight, pPortDef->nBufferSize);
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
                OMX_PRBUFFER1(pComponentPrivate->dbg, "CountActual 0x%x CountMin 0x%x Size %d bEnabled %x bPopulated %x\n",
                                (int )pComponentParam->nBufferCountActual,
                                (int )pComponentParam->nBufferCountMin,
                                (int )pComponentParam->nBufferSize,
                                (int )pComponentParam->bEnabled,
                                (int )pComponentParam->bPopulated);
            }
            break;
        case OMX_IndexParamVideoWmv:
            {
                OMX_VIDEO_PARAM_WMVTYPE* pComponentParam = (OMX_VIDEO_PARAM_WMVTYPE*)pCompParam;
                if (pComponentParam->nPortIndex == pComponentPrivate->pWMV->nPortIndex) {
                    memcpy(pComponentPrivate->pWMV, pCompParam, sizeof(OMX_VIDEO_PARAM_WMVTYPE));
                } 
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoMpeg4:
            {
                OMX_VIDEO_PARAM_MPEG4TYPE* pComponentParam = (OMX_VIDEO_PARAM_MPEG4TYPE*)pCompParam;
                if (pComponentParam->nPortIndex == pComponentPrivate->pMpeg4->nPortIndex) {
                    memcpy(pComponentPrivate->pMpeg4, pCompParam, sizeof(OMX_VIDEO_PARAM_MPEG4TYPE));
                } 
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoMpeg2:
            {
                OMX_VIDEO_PARAM_MPEG2TYPE* pComponentParam = (OMX_VIDEO_PARAM_MPEG2TYPE*)pCompParam;
                if (pComponentParam->nPortIndex == pComponentPrivate->pMpeg2->nPortIndex) {
                    memcpy(pComponentPrivate->pMpeg2, pCompParam, sizeof(OMX_VIDEO_PARAM_MPEG2TYPE));
                } 
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoAvc:
            {
                OMX_VIDEO_PARAM_AVCTYPE* pComponentParam = (OMX_VIDEO_PARAM_AVCTYPE *)pCompParam;
                if (pComponentParam->nPortIndex == pComponentPrivate->pH264->nPortIndex) {
                    memcpy(pComponentPrivate->pH264, pCompParam, sizeof(OMX_VIDEO_PARAM_AVCTYPE));
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamVideoH263:
            {
                OMX_VIDEO_PARAM_H263TYPE* pComponentParam = (OMX_VIDEO_PARAM_H263TYPE *)pCompParam;
                if (pComponentParam->nPortIndex == pComponentPrivate->pH263->nPortIndex) {
                    memcpy(pComponentPrivate->pH263, pCompParam, sizeof(OMX_VIDEO_PARAM_H263TYPE));
                }
                else {
                    eError = OMX_ErrorBadPortIndex;
                }
            }
            break;
        case OMX_IndexParamPriorityMgmt:
            memcpy(pComponentPrivate->pPriorityMgmt, (OMX_PRIORITYMGMTTYPE*)pCompParam, sizeof(OMX_PRIORITYMGMTTYPE));
            break;
        case OMX_IndexParamCompBufferSupplier:
            {
                OMX_PARAM_BUFFERSUPPLIERTYPE* pBuffSupplierParam = (OMX_PARAM_BUFFERSUPPLIERTYPE*)pCompParam;

                if (pBuffSupplierParam->nPortIndex == 1) {
                    pComponentPrivate->pCompPort[pBuffSupplierParam->nPortIndex]->eSupplierSetting = pBuffSupplierParam->eBufferSupplier;
                } 
                else if (pBuffSupplierParam->nPortIndex == 0) {
                    pComponentPrivate->pCompPort[pBuffSupplierParam->nPortIndex]->eSupplierSetting = pBuffSupplierParam->eBufferSupplier;
                } 
                else {
                    eError = OMX_ErrorBadPortIndex;
                    break;
                }
            }
            break;
        /* Video decode custom parameters */
        case VideoDecodeCustomParamProcessMode:
            pComponentPrivate->ProcessMode = (OMX_U32)(*((OMX_U32 *)pCompParam));
            break;
        case VideoDecodeCustomParamParserEnabled:
            pComponentPrivate->bParserEnabled = (OMX_BOOL)(*((OMX_BOOL *)pCompParam));
            break;
        case VideoDecodeCustomParamH264BitStreamFormat:
            pComponentPrivate->H264BitStreamFormat = (OMX_U32)(*((OMX_U32 *)pCompParam));
            break;
        case VideoDecodeCustomParamWMVProfile:
            {
                pComponentPrivate->wmvProfile = *((VIDDEC_WMV_PROFILES *)pCompParam);
            }
            break;
#ifdef KHRONOS_1_1
        case OMX_IndexParamStandardComponentRole:
            if (pCompParam != NULL) {
                OMX_U8* cTempRole = NULL;
                cTempRole = (OMX_U8*)pComponentPrivate->componentRole.cRole;
                pRole = (OMX_PARAM_COMPONENTROLETYPE *)pCompParam;
                if(strcmp( (char*)pRole->cRole, VIDDEC_COMPONENTROLES_H263) == 0) {
                    eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_H263);
                }
                else if(strcmp( (char*)pRole->cRole, VIDDEC_COMPONENTROLES_H264) == 0) {
                    eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_H264);
                }
                else if(strcmp( (char*)pRole->cRole, VIDDEC_COMPONENTROLES_MPEG2) == 0) {
                    eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_MPEG2);
                }
                else if(strcmp( (char*)pRole->cRole, VIDDEC_COMPONENTROLES_MPEG4) == 0) {
                    eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_MPEG4);
                }
                else if(strcmp( (char*)pRole->cRole, VIDDEC_COMPONENTROLES_WMV9) == 0) {
                    eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_WMV9);
                }
#ifdef VIDDEC_SPARK_CODE
                else if(strcmp( (char*)pRole->cRole, VIDDEC_COMPONENTROLES_SPARK) == 0) {
                    eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_SPARK);
                }
#endif
                else {
                    eError = OMX_ErrorBadParameter;
                }
                if(eError != OMX_ErrorNone) {
                    goto EXIT;
                }
#ifdef ANDROID
                /* Set format according with hw accelerated rendering */
                if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT422) {
                    eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_INTERLEAVED422);
                }
#else
                if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                    eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
                }
#endif
                memcpy( (void *)&pComponentPrivate->componentRole, (void *)pRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
            } 
            else {
                eError = OMX_ErrorBadParameter;
            }
            break;
#endif
        case VideoDecodeCustomParamWMVFileType:
            pComponentPrivate->nWMVFileType = (OMX_U32)(*((OMX_U32 *)pCompParam));

            break;
        case VideoDecodeCustomParamIsNALBigEndian:
            pComponentPrivate->bIsNALBigEndian = (OMX_BOOL)(*((OMX_BOOL *)pCompParam));
            break;
#ifdef VIDDEC_SPARK_CODE
        case VideoDecodeCustomParamIsSparkInput:
            pComponentPrivate->bIsSparkInput = (OMX_BOOL)(*((OMX_BOOL *)pCompParam));
            break;
#endif
#ifdef KHRONOS_1_1
        case OMX_IndexConfigVideoMBErrorReporting:/**< reference: OMX_CONFIG_MBERRORREPORTINGTYPE */
        {
            if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
                pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263 ||
                pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
                OMX_CONFIG_MBERRORREPORTINGTYPE* pMBErrorReportFrom = pCompParam;
                pComponentPrivate->eMBErrorReport.bEnabled = pMBErrorReportFrom->bEnabled;
            }
            else {
                eError = OMX_ErrorUnsupportedIndex;
            }
            break;
        }
        case OMX_IndexParamCommonDeblocking: /**< reference: OMX_PARAM_DEBLOCKINGTYPE */
        {
            char value[PROPERTY_VALUE_MAX];
            property_get("debug.video.showfps", value, "0");
            mDebugFps = atoi(value);
            if (mDebugFps == OMX_FALSE) {
                if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
                        pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263){
                        pComponentPrivate->pDeblockingParamType->bDeblocking =
                            ((OMX_PARAM_DEBLOCKINGTYPE*)pCompParam)->bDeblocking;
                    break;
                }
            }
            else {
                OMX_PRINT4(pComponentPrivate->dbg, "Not setting deblocking to measure fps \n");
                eError = OMX_ErrorUnsupportedIndex;
                break;
            }
        }
        case OMX_IndexConfigCommonImageFilter: /**< reference: OMX_CONFIG_IMAGEFILTERTYPE */
        {
            OMX_CONFIG_IMAGEFILTERTYPE* pDeringingParamType = pCompParam;
            if (pDeringingParamType->nPortIndex == VIDDEC_OUTPUT_PORT) {
                switch (pDeringingParamType->eImageFilter) {
                    case OMX_ImageFilterNone:
                    case OMX_ImageFilterDeRing:
                    {
                        switch (pComponentPrivate->pInPortDef->format.video.eCompressionFormat) {
                            case OMX_VIDEO_CodingMPEG4:
                            case OMX_VIDEO_CodingH263:
                                memcpy (pComponentPrivate->pDeringingParamType, pDeringingParamType, sizeof(OMX_CONFIG_IMAGEFILTERTYPE));
                                break;
                            default:
                                eError = OMX_ErrorUnsupportedIndex;
                                break;
                        }
                        break;
                    }
                    default:
                        eError = OMX_ErrorUnsupportedIndex;
                        break;
                    }
            }
            else {
                eError = OMX_ErrorUnsupportedIndex;
            }
        }

        case OMX_IndexParamVideoMacroblocksPerFrame:
        case OMX_IndexParamNumAvailableStreams:
        case OMX_IndexParamActiveStream:
        case OMX_IndexParamSuspensionPolicy:
        case OMX_IndexParamComponentSuspended:
        case OMX_IndexAutoPauseAfterCapture:
        case OMX_IndexParamCustomContentPipe:
        case OMX_IndexParamDisableResourceConcealment:
#ifdef KHRONOS_1_2
            case OMX_IndexConfigMetadataItemCount:
            case OMX_IndexConfigContainerNodeCount:
            case OMX_IndexConfigMetadataItem:
            case OMX_IndexConfigCounterNodeID:
            case OMX_IndexParamMetadataFilterType:
            case OMX_IndexConfigCommonTransitionEffect:
            case OMX_IndexKhronosExtensions:
#else
            case OMX_IndexConfigMetaDataSize:
            case OMX_IndexConfigMetaDataAtIndex:
            case OMX_IndexConfigMetaDataAtKey:
            case OMX_IndexConfigMetaDataNodeCount:
            case OMX_IndexConfigMetaDataNode:
            case OMX_IndexConfigMetaDataItemCount:
#endif
        case OMX_IndexParamMetadataKeyFilter:
        case OMX_IndexConfigPriorityMgmt:
        case OMX_IndexConfigAudioChannelVolume:
        case OMX_IndexConfigFlashControl:
        case OMX_IndexParamVideoProfileLevelQuerySupported:
           break;
        case OMX_IndexParamVideoProfileLevelCurrent:
        {
           VIDEO_PROFILE_LEVEL_TYPE* pProfileLevel = NULL;
           OMX_VIDEO_PARAM_PROFILELEVELTYPE *pParamProfileLevel = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pCompParam;

           /* Choose table based on compression format */
           switch(pComponentPrivate->pInPortDef->format.video.eCompressionFormat)
           {
              case OMX_VIDEO_CodingH263:
                 pProfileLevel = SupportedH263ProfileLevels;
                 break;
              case OMX_VIDEO_CodingMPEG4:
                 pProfileLevel = SupportedMPEG4ProfileLevels;
                 break;
              case OMX_VIDEO_CodingAVC:
                 pProfileLevel = SupportedAVCProfileLevels;
                 break;
              default:
                 break;
            }

            if (pProfileLevel == NULL) {
                eError = OMX_ErrorBadParameter;
                break;
            }

            /* Check validity of profile & level parameters */
            while((pProfileLevel->nProfile != (OMX_S32)pParamProfileLevel->eProfile) ||
                 (pProfileLevel->nLevel != (OMX_S32)pParamProfileLevel->eLevel)) {
               pProfileLevel++;
               if(pProfileLevel->nProfile == -1) break;
             }

            if(pProfileLevel->nProfile != -1) {
            /* Update profile & level values in the compression format specific structure */
               switch(pComponentPrivate->pInPortDef->format.video.eCompressionFormat) {
                  case OMX_VIDEO_CodingH263:
                     pComponentPrivate->pH263->eProfile = pParamProfileLevel->eProfile;
                     pComponentPrivate->pH263->eLevel = pParamProfileLevel->eLevel;
                     break;
                  case OMX_VIDEO_CodingMPEG4:
                     pComponentPrivate->pMpeg4->eProfile = pParamProfileLevel->eProfile;
                     pComponentPrivate->pMpeg4->eLevel = pParamProfileLevel->eLevel;
                     break;
                  case OMX_VIDEO_CodingAVC:
                     pComponentPrivate->pH264->eProfile = pParamProfileLevel->eProfile;
                     pComponentPrivate->pH264->eLevel = pParamProfileLevel->eLevel;
                     break;
                  default:
                     break;
               }
               eError = OMX_ErrorNone;
            }
            else {
               eError = OMX_ErrorBadParameter;
            }
            break;
            }

#endif
        default:
            eError = OMX_ErrorUnsupportedIndex;
            break;
    }
EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_GetConfig() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_GetConfig (OMX_HANDLETYPE hComp, 
                                       OMX_INDEXTYPE nConfigIndex,
                                       OMX_PTR ComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;

    OMX_CONF_CHECK_CMD(hComp, ComponentConfigStructure, OMX_TRUE);

    pHandle = (OMX_COMPONENTTYPE*)hComp;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    if (pComponentPrivate->eState == OMX_StateInvalid) {
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }
    else {
        switch ((OMX_S32) nConfigIndex)
        {
            case OMX_IndexParamPortDefinition:
                {
                    OMX_PARAM_PORTDEFINITIONTYPE *pPortDefParam = (OMX_PARAM_PORTDEFINITIONTYPE *)ComponentConfigStructure;
                    if (((OMX_PARAM_PORTDEFINITIONTYPE*)(ComponentConfigStructure))->nPortIndex == 
                            pComponentPrivate->pInPortDef->nPortIndex) {
                        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = pComponentPrivate->pInPortDef;
                        memcpy(pPortDefParam, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                    } 
                    else if (((OMX_PARAM_PORTDEFINITIONTYPE*)(ComponentConfigStructure))->nPortIndex == 
                            pComponentPrivate->pOutPortDef->nPortIndex) {
                        OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = pComponentPrivate->pOutPortDef;
                        memcpy(pPortDefParam, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                    } 
                    else {
                        eError = OMX_ErrorBadPortIndex;
                    }
                }
                break;
            case VideoDecodeCustomConfigDebug:/**< reference: struct OMX_TI_Debug */
                OMX_DBG_GETCONFIG(pComponentPrivate->dbg, ComponentConfigStructure);
                break;
#ifdef KHRONOS_1_1
            case OMX_IndexConfigVideoMBErrorReporting:/**< reference: OMX_CONFIG_MBERRORREPORTINGTYPE */
            {
                if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
                    pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263 ||
                    pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
                    OMX_CONFIG_MBERRORREPORTINGTYPE* pMBErrorReportTo = ComponentConfigStructure;
                    pMBErrorReportTo->bEnabled = pComponentPrivate->eMBErrorReport.bEnabled;
                }
                else {
                    eError = OMX_ErrorUnsupportedIndex;
                }
                break;
            }
            case OMX_IndexConfigVideoMacroBlockErrorMap: /**< reference: OMX_CONFIG_MACROBLOCKERRORMAPTYPE */
            {
                if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
                    pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263 ||
                    pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
                    OMX_CONFIG_MACROBLOCKERRORMAPTYPE_TI* pMBErrorMapTypeFrom = &pComponentPrivate->eMBErrorMapType[pComponentPrivate->cMBErrorIndexOut];
                    OMX_CONFIG_MACROBLOCKERRORMAPTYPE_TI* pMBErrorMapTypeTo = ComponentConfigStructure;
                    OMX_U8* ErrMapFrom = pMBErrorMapTypeFrom->ErrMap;
                    OMX_U8* ErrMapTo = pMBErrorMapTypeTo->ErrMap;
                    pMBErrorMapTypeTo->nErrMapSize = pMBErrorMapTypeFrom->nErrMapSize;
                    memcpy(ErrMapTo, ErrMapFrom, pMBErrorMapTypeFrom->nErrMapSize);
                    pComponentPrivate->cMBErrorIndexOut++;
                    pComponentPrivate->cMBErrorIndexOut %= pComponentPrivate->pOutPortDef->nBufferCountActual;
                }
                else {
                    eError = OMX_ErrorUnsupportedIndex;
                }
                break;
            }
            case OMX_IndexParamVideoMacroblocksPerFrame:/**< reference: OMX_PARAM_MACROBLOCKSTYPE */
            {
                if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
                    pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263 ||
                    pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
                    OMX_PARAM_MACROBLOCKSTYPE* pMBBlocksTypeTo = ComponentConfigStructure;
                    pMBBlocksTypeTo->nMacroblocks = pComponentPrivate->pOutPortDef->format.video.nFrameWidth *
                                                    pComponentPrivate->pOutPortDef->format.video.nFrameHeight / 256;
                }
                else {
                    eError = OMX_ErrorUnsupportedIndex;
                }
                break;
            }
            case OMX_IndexConfigCommonOutputCrop:
            {
 		OMX_CONFIG_RECTTYPE *crop2 = (OMX_CONFIG_RECTTYPE*)ComponentConfigStructure;
                if (((OMX_CONFIG_RECTTYPE*)(ComponentConfigStructure))->nPortIndex == 
                            pComponentPrivate->pOutPortDef->nPortIndex) {
     		        crop2->nLeft   =  0;
            		crop2->nWidth  =  pComponentPrivate->pInPortDef->format.video.nFrameWidth - pComponentPrivate->nCropWidth;
     		        crop2->nTop    =  0;
            		crop2->nHeight =  pComponentPrivate->pInPortDef->format.video.nFrameHeight - pComponentPrivate->nCropHeight;
            }
               	break;
	    }
           
           
#endif
            /* Vendor specific structures should be in the range of 0xFF000000
               to 0xFFFFFFFF.  This range is not broken out by vendor, so
               private indexes are not guaranteed unique and therefore should
               only be sent to the appropriate component. */

            default:
                eError = OMX_ErrorUnsupportedIndex;
            break;
        }
    }    
EXIT:
    return eError;
}
/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_SetConfig() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_SetConfig (OMX_HANDLETYPE hComp, 
                                       OMX_INDEXTYPE nConfigIndex,
                                       OMX_PTR ComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE* pComponentConfig = NULL;
    OMX_COMPONENTTYPE* pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    OMX_S32 nConfigIndexTemp = 0;

    OMX_CONF_CHECK_CMD(hComp, ComponentConfigStructure, OMX_TRUE);

    pComponentConfig = (OMX_PARAM_PORTDEFINITIONTYPE*)ComponentConfigStructure;
    pHandle = (OMX_COMPONENTTYPE*)hComp;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    if (pComponentPrivate->eState == OMX_StateInvalid) {
        eError = OMX_ErrorInvalidState;
        OMX_PRSTATE4(pComponentPrivate->dbg, "state invalid for SetConfig...............\n");
        goto EXIT;
    }
    else {
        nConfigIndexTemp = nConfigIndex;
        switch (nConfigIndexTemp) {
            case OMX_IndexParamPortDefinition:
            {
                OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = NULL;
                if (pComponentConfig->nPortIndex == VIDDEC_INPUT_PORT) {
                    OMX_MALLOC_STRUCT(pPortDef, OMX_PARAM_PORTDEFINITIONTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel4]);
                    if (pPortDef == NULL) {
                        OMX_TRACE4(pComponentPrivate->dbg, "malloc failed\n");
                        eError = OMX_ErrorInsufficientResources;
                        goto EXIT;
                    }
                    ((VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate)->pPortDef[VIDDEC_INPUT_PORT] = pPortDef;
                    memcpy(pPortDef, pComponentConfig, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                    free(pPortDef);
                }
                else if (pComponentConfig->nPortIndex == VIDDEC_OUTPUT_PORT) {
                    OMX_MALLOC_STRUCT(pPortDef, OMX_PARAM_PORTDEFINITIONTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel4]);
                    if (pPortDef == NULL) {
                        OMX_TRACE4(pComponentPrivate->dbg, "malloc failed\n");
                        eError = OMX_ErrorInsufficientResources;
                        goto EXIT;
                    }
                    ((VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate)->pPortDef[VIDDEC_OUTPUT_PORT] = pPortDef;
                    memcpy(pPortDef, pComponentConfig, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
                    free(pPortDef);
                }
                break;
            }
            case VideoDecodeCustomConfigDebug:/**< reference: struct OMX_TI_Debug */
                OMX_DBG_SETCONFIG(pComponentPrivate->dbg, ComponentConfigStructure);
                break;
            case VideoDecodeCustomConfigCacheableBuffers: /**<reference: Confirms that output buffers are cachebale; By default they are not*/
            {
                pComponentPrivate->bCacheableOutputBuffers = OMX_TRUE;
                break;
            }

#ifdef KHRONOS_1_1
            case OMX_IndexConfigVideoMBErrorReporting:/**< reference: OMX_CONFIG_MBERRORREPORTINGTYPE */
            {
                if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
                    pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263 ||
                    pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
                    OMX_CONFIG_MBERRORREPORTINGTYPE* pMBErrorReportFrom = ComponentConfigStructure;
                    pComponentPrivate->eMBErrorReport.bEnabled = pMBErrorReportFrom->bEnabled;
                }
                else {
                    eError = OMX_ErrorUnsupportedIndex;
                }
                break;
            }
#endif
            /* Vendor specific structures should be in the range of 0xFF000000
               to 0xFFFFFFFF.  This range is not broken out by vendor, so
               private indexes are not guaranteed unique and therefore should
               only be sent to the appropriate component. */

            default:
                eError = OMX_ErrorUnsupportedIndex;
            break;
        }
    }

EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_GetState() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_GetState (OMX_HANDLETYPE hComponent, 
                                      OMX_STATETYPE* pState)
{
    OMX_ERRORTYPE eError                        = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = NULL;

    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    struct timespec abs_time = {0,0};
    int nPendingStateChangeRequests = 0;
    int errorFromWait = 0;
    /* Set to sufficiently high value */
    int mutex_timeout = 3;
    OMX_CONF_CHECK_CMD(hComponent, pState, OMX_TRUE);
    pHandle = (OMX_COMPONENTTYPE*)hComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate;

    /* Retrieve current state */
    if (pHandle && pHandle->pComponentPrivate) {
        /* Check for any pending state transition requests */
        if(pthread_mutex_lock(&pComponentPrivate->mutexStateChangeRequest)) {
            return OMX_ErrorUndefined;
        }
        nPendingStateChangeRequests = pComponentPrivate->nPendingStateChangeRequests;
        if(!nPendingStateChangeRequests) {
           if(pthread_mutex_unlock(&pComponentPrivate->mutexStateChangeRequest)) {
               return OMX_ErrorUndefined;
           }

           /* No pending state transitions */
          *pState = ((VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate)->eState;
            eError = OMX_ErrorNone;
        }
        else {
                  /* Wait for component to complete state transition */
           clock_gettime(CLOCK_REALTIME, &abs_time);
           abs_time.tv_sec += mutex_timeout;
           abs_time.tv_nsec = 0;
           errorFromWait = pthread_cond_timedwait(&(pComponentPrivate->StateChangeCondition), &(pComponentPrivate->mutexStateChangeRequest), &abs_time);
           if (!errorFromWait) {
              /* Component has completed state transitions*/
              *pState = ((VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate)->eState;
              if(pthread_mutex_unlock(&pComponentPrivate->mutexStateChangeRequest)) {
                  return OMX_ErrorUndefined;
              }
              eError = OMX_ErrorNone;
           }
           else if(errorFromWait == ETIMEDOUT) {
              /* Unlock mutex in case of timeout */
              *pState = ((VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate)->eState;
              pthread_mutex_unlock(&pComponentPrivate->mutexStateChangeRequest);
              return OMX_ErrorTimeout;
           }
           else {
              /* Incase of other errors unlock the mutex*/
              *pState = ((VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate)->eState;
              pthread_mutex_unlock(&pComponentPrivate->mutexStateChangeRequest);
              return OMX_ErrorUndefined;
           }
        }
     }
     else {
        eError = OMX_ErrorInvalidComponent;
        *pState = OMX_StateInvalid;
     }
EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_EmptyThisBuffer() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_EmptyThisBuffer (OMX_HANDLETYPE pComponent, 
                                             OMX_BUFFERHEADERTYPE* pBuffHead)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    VIDDEC_BUFFER_PRIVATE* pBufferPrivate = NULL;
    ssize_t bytesWritten = 0;
    VIDDEC_BUFFER_OWNER oldBufferOwner;

    OMX_CONF_CHECK_CMD(pComponent, pBuffHead, OMX_TRUE);

    pHandle = (OMX_COMPONENTTYPE *)pComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    OMX_PRBUFFER1(pComponentPrivate->dbg, "+++Entering pHandle 0x%p pBuffer 0x%p Index %lu  state %x  nflags  %lx  isfirst %x\n",
                pComponent, pBuffHead, pBuffHead->nInputPortIndex,pComponentPrivate->eState,(OMX_U32)pBuffHead->nFlags,(OMX_BOOL)pComponentPrivate->bFirstHeader);

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedFrame(pComponentPrivate->pPERF,
                       pBuffHead->pBuffer,
                       pBuffHead->nFilledLen,
                       PERF_ModuleHLMM);
#endif

    OMX_PRBUFFER1(pComponentPrivate->dbg, "pComponentPrivate->pInPortDef->bEnabled %d\n",
        pComponentPrivate->pInPortDef->bEnabled);
    if(!pComponentPrivate->pInPortDef->bEnabled){
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if(pBuffHead->nInputPortIndex != VIDDEC_INPUT_PORT){
        eError = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if(pComponentPrivate->eState != OMX_StateExecuting &&
        pComponentPrivate->eState != OMX_StatePause &&
        pComponentPrivate->eState != OMX_StateIdle){
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    OMX_CONF_CHK_VERSION(pBuffHead, OMX_BUFFERHEADERTYPE, eError, pComponentPrivate->dbg);

    if ((pComponentPrivate->bParserEnabled == OMX_FALSE) &&
        (pComponentPrivate->bFirstHeader == OMX_FALSE) && 
        (pBuffHead->nFilledLen > pBuffHead->nAllocLen)) {
        pBuffHead->nFilledLen = pBuffHead->nAllocLen;
    }

    pBufferPrivate = (VIDDEC_BUFFER_PRIVATE* )pBuffHead->pInputPortPrivate;
    oldBufferOwner = pBufferPrivate->eBufferOwner;
    pBufferPrivate->eBufferOwner = VIDDEC_BUFFER_WITH_COMPONENT;
    eError = IncrementCount (&(pComponentPrivate->nCountInputBFromApp), &(pComponentPrivate->mutexInputBFromApp));
    if (eError != OMX_ErrorNone) {
        return eError;
    }

    OMX_PRBUFFER1(pComponentPrivate->dbg, "Writing pBuffer 0x%p OldeBufferOwner %d nAllocLen %lu nFilledLen %lu eBufferOwner %d\n",
        pBuffHead, oldBufferOwner,pBuffHead->nAllocLen,pBuffHead->nFilledLen,pBufferPrivate->eBufferOwner);

    bytesWritten = write (pComponentPrivate->filled_inpBuf_Q[VIDDEC_PIPE_WRITE], &(pBuffHead), sizeof(pBuffHead));
    if (bytesWritten == -1) {
        /*like function returns error buffer still with Client IL*/
        pBufferPrivate->eBufferOwner = VIDDEC_BUFFER_WITH_CLIENT;
        OMX_PRCOMM4(pComponentPrivate->dbg, "Error in Writing to the Data pipe\n");
        DecrementCount (&(pComponentPrivate->nCountInputBFromApp), &(pComponentPrivate->mutexInputBFromApp));
        eError = OMX_ErrorHardware;
        goto EXIT;
    }

EXIT:
    if (pComponentPrivate)
        OMX_PRBUFFER1(pComponentPrivate->dbg, "---Exiting 0x%x\n", eError);
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_FillThisBuffer() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_FillThisBuffer (OMX_HANDLETYPE pComponent,
                                            OMX_BUFFERHEADERTYPE* pBuffHead)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    VIDDEC_BUFFER_PRIVATE* pBufferPrivate = NULL;
	IMG_native_handle_t*  grallocHandle;
	OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    ssize_t bytesWritten = 0;
    VIDDEC_BUFFER_OWNER oldBufferOwner;
    OMX_CONF_CHECK_CMD(pComponent, pBuffHead, OMX_TRUE);

    pHandle = (OMX_COMPONENTTYPE *)pComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    OMX_PRBUFFER1(pComponentPrivate->dbg, "+++Entering pHandle 0x%p pBuffer 0x%p Index %lu\n",pComponent,
            pBuffHead, pBuffHead->nOutputPortIndex);

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedFrame(pComponentPrivate->pPERF,
                       pBuffHead->pBuffer,
                       0,
                       PERF_ModuleHLMM);
#endif

    OMX_PRBUFFER1(pComponentPrivate->dbg, "pComponentPrivate->pOutPortDef->bEnabled %d\n",
        pComponentPrivate->pOutPortDef->bEnabled);
    if(!pComponentPrivate->pOutPortDef->bEnabled){
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    if(pBuffHead->nOutputPortIndex != 0x1){
        eError = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if(pComponentPrivate->eState != OMX_StateExecuting &&
        pComponentPrivate->eState != OMX_StatePause &&
        pComponentPrivate->eState != OMX_StateIdle){
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    OMX_CONF_CHK_VERSION(pBuffHead, OMX_BUFFERHEADERTYPE, eError, pComponentPrivate->dbg);

    if ((pComponentPrivate->bParserEnabled == OMX_FALSE) && 
        (pComponentPrivate->bFirstHeader == OMX_FALSE) && 
        (pBuffHead->nFilledLen > pBuffHead->nAllocLen)) {
        OMX_PRINT1(pComponentPrivate->dbg, "bFirstHeader: nFilledLen= %lu <- nAllocLen= %lu\n", pBuffHead->nFilledLen, pBuffHead->nAllocLen);

        pBuffHead->nFilledLen = pBuffHead->nAllocLen;
    }

  
	if(pComponentPrivate->pCompPort[OMX_VIDEODECODER_OUTPUT_PORT]->VIDDECBufferType
			== GrallocPointers)
	{
		/* Lock the Gralloc buffer till it gets rendered completely */
		/* Extract the Gralloc handle from the Header and then call lock on that */
		/* Note# There is no error check for the pBufferHdr here*/
		grallocHandle = (IMG_native_handle_t*)pBuffHead->pBuffer;
		/*Initializing Structure */
		sPortDef.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
		sPortDef.nVersion.s.nVersionMajor = OMX_VER_MAJOR;
		sPortDef.nVersion.s.nVersionMinor = OMX_VER_MINOR;
		sPortDef.nVersion.s.nRevision = 0x0;
		sPortDef.nVersion.s.nStep = 0x0;
		sPortDef.nPortIndex = OMX_VIDEODECODER_INPUT_PORT;
		eError = VIDDEC_GetParameter(pHandle,OMX_IndexParamPortDefinition,
				&sPortDef);

		pComponentPrivate->grallocModule->lock((gralloc_module_t const *) pComponentPrivate->grallocModule,
				(buffer_handle_t)grallocHandle, GRALLOC_USAGE_HW_RENDER,
				0,0,sPortDef.format.video.nFrameWidth, sPortDef.format.video.nFrameHeight,NULL);
	}
    pBufferPrivate = (VIDDEC_BUFFER_PRIVATE* )pBuffHead->pOutputPortPrivate;
    oldBufferOwner = pBufferPrivate->eBufferOwner;
    pBufferPrivate->eBufferOwner = VIDDEC_BUFFER_WITH_COMPONENT;
    eError = IncrementCount (&(pComponentPrivate->nCountOutputBFromApp), &(pComponentPrivate->mutexOutputBFromApp));
    if (eError != OMX_ErrorNone) {
        return eError;
    }
    pBuffHead->nFilledLen = 0;
    pBuffHead->nFlags = 0;
    OMX_PRBUFFER1(pComponentPrivate->dbg, "Writing pBuffer 0x%p OldeBufferOwner %d eBufferOwner %d nFilledLen %lu\n",
        pBuffHead, oldBufferOwner,pBufferPrivate->eBufferOwner,pBuffHead->nFilledLen);
    bytesWritten = write (pComponentPrivate->free_outBuf_Q[1], &(pBuffHead), sizeof (pBuffHead));
    if (bytesWritten == -1) {
        /*like function returns error buffer still with Client IL*/
        pBufferPrivate->eBufferOwner = VIDDEC_BUFFER_WITH_CLIENT;
        OMX_PRCOMM4(pComponentPrivate->dbg, "Error in Writing to the Data pipe\n");
        DecrementCount (&(pComponentPrivate->nCountOutputBFromApp), &(pComponentPrivate->mutexOutputBFromApp));
        eError = OMX_ErrorHardware;
        goto EXIT;
    }

EXIT:
    if (pComponentPrivate)
        OMX_PRBUFFER1(pComponentPrivate->dbg, "---Exiting 0x%x\n", eError);
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  * VIDDEC_ComponentDeinit() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_ComponentDeInit(OMX_HANDLETYPE hComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    OMX_COMMANDTYPE Cmd = OMX_CommandStateSet;
    OMX_U32 nParam1 = -1;
    OMX_U32 i = 0;
    OMX_U32 count = 0;

    OMX_CONF_CHECK_CMD(hComponent, OMX_TRUE, OMX_TRUE);

    pHandle = (OMX_COMPONENTTYPE*)hComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate;

#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(pComponentPrivate->pPERF,
                  PERF_BoundaryStart | PERF_BoundaryCleanup);
    PERF_SendingCommand(pComponentPrivate->pPERF,
                        Cmd, nParam1, PERF_ModuleComponent);
#endif

    if(pComponentPrivate->eLCMLState != VidDec_LCML_State_Unload &&
        pComponentPrivate->pModLCML != NULL &&
        pComponentPrivate->pLCML != NULL){
        LCML_DSP_INTERFACE *pLcmlHandle = NULL;
        pLcmlHandle = (LCML_DSP_INTERFACE*)pComponentPrivate->pLCML;
        LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle, EMMCodecControlDestroy, NULL);
        pComponentPrivate->eLCMLState = VidDec_LCML_State_Destroy;
    }
    if(pComponentPrivate->eLCMLState != VidDec_LCML_State_Unload &&
        pComponentPrivate->pModLCML != NULL){
        if(pComponentPrivate->pModLCML != NULL){
            dlclose(pComponentPrivate->pModLCML);
            pComponentPrivate->pModLCML = NULL;
            pComponentPrivate->pLCML = NULL;
            pComponentPrivate->eLCMLState = VidDec_LCML_State_Unload;
        }
    }
    eError = write(pComponentPrivate->cmdPipe[VIDDEC_PIPE_WRITE], &Cmd, sizeof(Cmd));
    if (eError == -1) {
        eError = OMX_ErrorUndefined;
    }
    eError = write(pComponentPrivate->cmdDataPipe[VIDDEC_PIPE_WRITE], &nParam1, sizeof(nParam1));
    if (eError == -1) {
       eError = OMX_ErrorUndefined;
    }

    eError = VIDDEC_Stop_ComponentThread(pHandle);
    if (eError != OMX_ErrorNone) {
        OMX_ERROR4(pComponentPrivate->dbg, "Error returned from the Component\n");
    }
    if (pComponentPrivate->pInternalConfigBufferAVC != NULL){
      free(pComponentPrivate->pInternalConfigBufferAVC);
    }
    for (count = 0; count < MAX_PRIVATE_BUFFERS; count++) {
        if(pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT]->pBufferPrivate[count]->pBufferHdr != NULL) {
            OMX_BUFFERHEADERTYPE* pBuffHead = NULL;
            pBuffHead = pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT]->pBufferPrivate[count]->pBufferHdr;
            if(pBuffHead != NULL){
                if(pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT]->pBufferPrivate[count]->bAllocByComponent == OMX_TRUE){
                    VIDDEC_BUFFER_PRIVATE* pBufferPrivate = pBuffHead->pInputPortPrivate;
                    if(pBufferPrivate->pOriginalBuffer != NULL)
                       free(pBufferPrivate->pOriginalBuffer);
                    else
                       OMX_MEMFREE_STRUCT_DSPALIGN(pBuffHead->pBuffer,OMX_U8);
                }
                free(pBuffHead);
            }
        }
    }

    for (count = 0; count < MAX_PRIVATE_BUFFERS; count++) {
        if(pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->pBufferPrivate[count]->pBufferHdr != NULL) {
            OMX_BUFFERHEADERTYPE* pBuffHead = NULL;
            pBuffHead = pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->pBufferPrivate[count]->pBufferHdr;
            if(pBuffHead != NULL){
	         if(pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->pBufferPrivate[count]->bAllocByComponent == OMX_TRUE){
                 VIDDEC_BUFFER_PRIVATE* pBufferPrivate = pBuffHead->pOutputPortPrivate;
                 if(pBufferPrivate->pOriginalBuffer != NULL)
                     free(pBufferPrivate->pOriginalBuffer);
                 else
                     OMX_MEMFREE_STRUCT_DSPALIGN(pBuffHead->pBuffer,OMX_U8);
	         }
                free(pBuffHead);
            }
        }
    }

    if(pComponentPrivate->ion_fd != 0)
	{
           ion_close(pComponentPrivate->ion_fd);
	}

#ifdef RESOURCE_MANAGER_ENABLED
    if(pComponentPrivate->eRMProxyState == VidDec_RMPROXY_State_Registered){
        OMX_PRMGR2(pComponentPrivate->dbg, "memory usage 0 %d : %d bytes\n",(unsigned int)pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0],(unsigned int)VIDDEC_MEMUSAGE);
        if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
            eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle, RMProxy_FreeResource, OMX_H264_Decode_COMPONENT, 0, VIDDEC_MEMUSAGE, NULL);
            if (eError != OMX_ErrorNone) {
                 OMX_PRMGR4(pComponentPrivate->dbg, "Error returned from destroy ResourceManagerProxy thread\n");
            }
        }
        else if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingWMV) {
            eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle, RMProxy_FreeResource, OMX_WMV_Decode_COMPONENT, 0, VIDDEC_MEMUSAGE, NULL);
            if (eError != OMX_ErrorNone) {
                 OMX_PRMGR4(pComponentPrivate->dbg, "Error returned from destroy ResourceManagerProxy thread\n");
            }
        }
        else if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4) {
            eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle, RMProxy_FreeResource, OMX_MPEG4_Decode_COMPONENT, 0, VIDDEC_MEMUSAGE, NULL);
            if (eError != OMX_ErrorNone) {
                 OMX_PRMGR4(pComponentPrivate->dbg, "Error returned from destroy ResourceManagerProxy thread\n");
            }
        }
        else if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingH263) {
            eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle, RMProxy_FreeResource, OMX_H263_Decode_COMPONENT, 0, VIDDEC_MEMUSAGE, NULL);
            if (eError != OMX_ErrorNone) {
                 OMX_PRMGR4(pComponentPrivate->dbg, "Error returned from destroy ResourceManagerProxy thread\n");
            }
        }
        else if (pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG2) {
            eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle, RMProxy_FreeResource, OMX_MPEG2_Decode_COMPONENT, 0, VIDDEC_MEMUSAGE, NULL);
            if (eError != OMX_ErrorNone) {
                 OMX_PRMGR4(pComponentPrivate->dbg, "Error returned from destroy ResourceManagerProxy thread\n");
            }
        }
#ifdef VIDDEC_SPARK_CODE
        else if (VIDDEC_SPARKCHECK) {
            eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle, RMProxy_FreeResource, OMX_MPEG4_Decode_COMPONENT, 0, VIDDEC_MEMUSAGE, NULL);
            if (eError != OMX_ErrorNone) {
                 OMX_PRMGR4(pComponentPrivate->dbg, "Error returned from destroy ResourceManagerProxy thread\n");
            }
        }
#endif
        else {
            eError = OMX_ErrorUnsupportedSetting;
        }
        pComponentPrivate->eRMProxyState = VidDec_RMPROXY_State_Load;
    }
    if(pComponentPrivate->eRMProxyState != VidDec_RMPROXY_State_Unload){
        eError = RMProxy_DeinitalizeEx(OMX_COMPONENTTYPE_VIDEO);
        if (eError != OMX_ErrorNone) {
            OMX_PRMGR4(pComponentPrivate->dbg, "Error returned from destroy ResourceManagerProxy thread\n");
        }
        pComponentPrivate->eRMProxyState = VidDec_RMPROXY_State_Unload;
    }
#endif

    VIDDEC_CircBuf_DeInit(&pComponentPrivate->eStoreTimestamps);
    /* Free Resources */
    if(pComponentPrivate->pPortParamType) {
        free(pComponentPrivate->pPortParamType);
        pComponentPrivate->pPortParamType = NULL;
    }
#ifdef __STD_COMPONENT__
    if(pComponentPrivate->pPortParamTypeAudio) {
        free(pComponentPrivate->pPortParamTypeAudio);
        pComponentPrivate->pPortParamTypeAudio = NULL;
    }
    if(pComponentPrivate->pPortParamTypeImage) {
        free(pComponentPrivate->pPortParamTypeImage);
        pComponentPrivate->pPortParamTypeImage = NULL;
    }
    if(pComponentPrivate->pPortParamTypeOthers) {
        free(pComponentPrivate->pPortParamTypeOthers);
        pComponentPrivate->pPortParamTypeOthers = NULL;
    }
#endif

    for (i = 0; i < MAX_PRIVATE_BUFFERS; i++) {
        if(pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT]->pBufferPrivate[i]) {
            OMX_PRBUFFER1(pComponentPrivate->dbg, "BufferPrivate cleared 0x%p\n",
                    pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT]->pBufferPrivate[i]);
            free(pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT]->pBufferPrivate[i]);
            pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT]->pBufferPrivate[i] = NULL;
        }
    }

    for (i = 0; i < MAX_PRIVATE_BUFFERS; i++) {
        if(pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->pBufferPrivate[i]) {
            OMX_PRBUFFER1(pComponentPrivate->dbg, "BufferPrivate cleared 0x%p\n",
                    pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->pBufferPrivate[i]);
            free(pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->pBufferPrivate[i]);
            pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->pBufferPrivate[i] = NULL;
        }
    }
    if(pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]) {
        free(pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]);
        pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT] = NULL;
    }

    if(pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT]) {
        free(pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT]);
        pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT] = NULL;
    }

    if(pComponentPrivate->pInPortDef) {
        free(pComponentPrivate->pInPortDef);
        pComponentPrivate->pInPortDef = NULL;
    }
    if(pComponentPrivate->pOutPortDef) {
        free(pComponentPrivate->pOutPortDef);
        pComponentPrivate->pOutPortDef = NULL;
    }
    if(pComponentPrivate->pInPortFormat) {
        free(pComponentPrivate->pInPortFormat);
        pComponentPrivate->pInPortFormat = NULL;
    }
    if(pComponentPrivate->pOutPortFormat) {
        free(pComponentPrivate->pOutPortFormat);
        pComponentPrivate->pOutPortFormat = NULL;
    }
    if(pComponentPrivate->pPriorityMgmt) {
        free(pComponentPrivate->pPriorityMgmt);
        pComponentPrivate->pPriorityMgmt = NULL;
    }
    if(pComponentPrivate->pInBufSupplier) {
        free(pComponentPrivate->pInBufSupplier);
        pComponentPrivate->pInBufSupplier = NULL;
    }
    if(pComponentPrivate->pOutBufSupplier) {
        free(pComponentPrivate->pOutBufSupplier);  
        pComponentPrivate->pOutBufSupplier = NULL;
    }
    if(pComponentPrivate->pMpeg4 != NULL) {
        free(pComponentPrivate->pMpeg4);
        pComponentPrivate->pMpeg4 = NULL;
    }
    if(pComponentPrivate->pMpeg2 != NULL) {
        free(pComponentPrivate->pMpeg2);
        pComponentPrivate->pMpeg2 = NULL;
    }
    if(pComponentPrivate->pH264 != NULL) {
        free(pComponentPrivate->pH264);
        pComponentPrivate->pH264 = NULL;
    }
    if(pComponentPrivate->pH263 != NULL) {
        free(pComponentPrivate->pH263);
        pComponentPrivate->pH263 = NULL;
    }
    if(pComponentPrivate->pWMV != NULL) {
        free(pComponentPrivate->pWMV);
        pComponentPrivate->pWMV = NULL;
    }
    if(pComponentPrivate->pDeblockingParamType != NULL) {
        free(pComponentPrivate->pDeblockingParamType);
        pComponentPrivate->pDeblockingParamType = NULL;
    }
    if(pComponentPrivate->pDeringingParamType != NULL) {
        free(pComponentPrivate->pDeringingParamType);
        pComponentPrivate->pDeringingParamType = NULL;
    }
#ifdef ANDROID
    if(pComponentPrivate->pPVCapabilityFlags != NULL) {
        free(pComponentPrivate->pPVCapabilityFlags);
        pComponentPrivate->pPVCapabilityFlags = NULL;
    }
#endif
    if(pComponentPrivate->cComponentName != NULL) {
        free(pComponentPrivate->cComponentName);
        pComponentPrivate->cComponentName = NULL;
    }

    pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel0]  = 0;
    pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel1]  = 0;
    pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel2]  = 0;
    pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel3]  = 0;
    pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel4]  = 0;

#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(pComponentPrivate->pPERF,
                  PERF_BoundaryComplete | PERF_BoundaryCleanup);
    PERF_Done(pComponentPrivate->pPERF);
#endif

    if(pComponentPrivate->eFirstBuffer.pBufferHdr){
        if(pComponentPrivate->eFirstBuffer.pBufferHdr->pBuffer){
            OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->eFirstBuffer.pBufferHdr->pBuffer, OMX_U8);
        }
        OMX_FREE(pComponentPrivate->eFirstBuffer.pBufferHdr);
        pComponentPrivate->eFirstBuffer.bSaveFirstBuffer = OMX_FALSE;
    }

    if(pComponentPrivate->pCodecData){
        free(pComponentPrivate->pCodecData);
        pComponentPrivate->pCodecData = NULL;
        pComponentPrivate->nCodecDataSize = 0;
    }

    OMX_DBG_CLOSE(pComponentPrivate->dbg);

    VIDDEC_PTHREAD_MUTEX_DESTROY(pComponentPrivate->sMutex);
    VIDDEC_PTHREAD_SEMAPHORE_DESTROY(pComponentPrivate->sInSemaphore);
    VIDDEC_PTHREAD_SEMAPHORE_DESTROY(pComponentPrivate->sOutSemaphore);
    pthread_mutex_destroy(&(pComponentPrivate->mutexInputBFromApp));
    pthread_mutex_destroy(&(pComponentPrivate->mutexOutputBFromApp));
    pthread_mutex_destroy(&(pComponentPrivate->mutexInputBFromDSP));
    pthread_mutex_destroy(&(pComponentPrivate->mutexOutputBFromDSP));

    pthread_mutex_destroy(&pComponentPrivate->mutexStateChangeRequest);
    pthread_cond_destroy(&pComponentPrivate->StateChangeCondition);

    OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->pUalgParams,OMX_PTR);

    OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->pBufferRCV.sStructRCV,OMX_PTR);

    if(pHandle->pComponentPrivate != NULL) {
        free(pHandle->pComponentPrivate);
        pHandle->pComponentPrivate = NULL;
        pComponentPrivate = NULL;
    }
EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_UseBuffer() 
  *
  * 
  * 
  *
  * @param 
  * @param 
  * @param 
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                               OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                               OMX_IN OMX_U32 nPortIndex,
                               OMX_IN OMX_PTR pAppPrivate,
                               OMX_IN OMX_U32 nSizeBytes,
                               OMX_IN OMX_U8* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = NULL;
    VIDDEC_PORT_TYPE* pCompPort = NULL;
    OMX_U8 pBufferCnt = 0;

    OMX_CONF_CHECK_CMD(hComponent, pBuffer, OMX_TRUE);

    pHandle = (OMX_COMPONENTTYPE*)hComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate;


#ifdef  ENABLE_GRALLOC_BUFFERS
	OMX_PTR buff_t;
	struct ion_map_gralloc_to_ionhandle_data data;
#endif


    OMX_PRBUFFER1(pComponentPrivate->dbg, "+++Entering pHandle 0x%p ppBufferHdr 0x%p pBuffer 0x%p nPortIndex 0x%lx nSizeBytes 0x%lx\n",
        hComponent, *ppBufferHdr, pBuffer, nPortIndex, nSizeBytes);

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedFrame(pComponentPrivate->pPERF,
                       pBuffer,
                       nSizeBytes,
                       PERF_ModuleHLMM);
#endif

    if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
        pCompPort = pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT];
        pBufferCnt = pCompPort->nBufferCnt;
        pPortDef = pComponentPrivate->pInPortDef;
    }
    else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
        pCompPort = pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT];
        pBufferCnt = pCompPort->nBufferCnt;
        pPortDef = pComponentPrivate->pOutPortDef;
    }
    else {
        eError = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if(!pPortDef->bEnabled){
        OMX_ERROR4(pComponentPrivate->dbg, "Error: port disabled\n");
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }

    OMX_PRBUFFER1(pComponentPrivate->dbg, "pPortDef->nBufferSize %d nSizeBytes %d %d\n", (int )pPortDef->nBufferSize, 
        (int )nSizeBytes,(int )(pPortDef->nBufferSize > nSizeBytes));

    if(nSizeBytes < pPortDef->nBufferSize || pPortDef->bPopulated){
        OMX_PRBUFFER4(pComponentPrivate->dbg, "Error: badparameter\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    OMX_MALLOC_STRUCT(pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr, OMX_BUFFERHEADERTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel1]);
    if (!pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr) {
        OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
        eError = OMX_ErrorInsufficientResources;
        pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                               OMX_EventError,
                                               eError,
                                               OMX_TI_ErrorMajor,
                                               NULL);
        goto EXIT;
    }

    *ppBufferHdr = pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr;
    OMX_CONF_INIT_STRUCT(pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr, OMX_BUFFERHEADERTYPE, pComponentPrivate->dbg);

    (*ppBufferHdr)->pBuffer = pBuffer;
    (*ppBufferHdr)->nAllocLen = nSizeBytes;
    (*ppBufferHdr)->pAppPrivate = pAppPrivate;
    (*ppBufferHdr)->pMarkData = NULL;
#ifndef VIDDEC_WMVPOINTERFIXED
    if (pComponentPrivate->nWMVFileType == VIDDEC_WMV_ELEMSTREAM &&
        pComponentPrivate->pInPortDef->format.video.eCompressionFormat == OMX_VIDEO_CodingWMV && 
        pComponentPrivate->ProcessMode == 0 &&
        nPortIndex == VIDDEC_INPUT_PORT) {
        /* vc-1 fix */
        (*ppBufferHdr)->nOffset = VIDDEC_WMV_BUFFER_OFFSET;
    }
#endif

#ifdef  ENABLE_GRALLOC_BUFFERS

    if (nPortIndex == VIDDEC_OUTPUT_PORT) {

		if(pComponentPrivate->pCompPort[1]->VIDDECBufferType == GrallocPointers) {
			data.gralloc_handle = ((IMG_native_handle_t*)(*ppBufferHdr)->pBuffer)->fd[0];

                   
        if (ion_ioctl(pComponentPrivate->ion_fd, ION_IOC_MAP_GRALLOC, &data)) {
				ALOGV("ion_ioctl fail");
			}

		if (ion_map(pComponentPrivate->ion_fd, data.handleY, (*ppBufferHdr)->nAllocLen, PROT_READ | PROT_WRITE,
								MAP_SHARED, 0, &buff_t, &pComponentPrivate->mmap_fd[pBufferCnt]) < 0) {
						ALOGV("ION map failed");
		}
		(*ppBufferHdr)->pPlatformPrivate = buff_t;
		}
    }
    pComponentPrivate->bUseThumbnail = OMX_TRUE;
#endif

    if (pCompPort->hTunnelComponent != NULL) {
        if (pPortDef->eDir == OMX_DirInput) {
            (*ppBufferHdr)->nInputPortIndex  = nPortIndex;
            (*ppBufferHdr)->nOutputPortIndex = pComponentPrivate->pCompPort[nPortIndex]->nTunnelPort;
        }
        else {
            (*ppBufferHdr)->nInputPortIndex  = pComponentPrivate->pCompPort[nPortIndex]->nTunnelPort;
            (*ppBufferHdr)->nOutputPortIndex = nPortIndex;
        }
    }
    else {
        if (nPortIndex == VIDDEC_INPUT_PORT) {
            (*ppBufferHdr)->nInputPortIndex  = VIDDEC_INPUT_PORT;
            (*ppBufferHdr)->nOutputPortIndex = VIDDEC_NOPORT;
        }
        else {
            (*ppBufferHdr)->nInputPortIndex  = VIDDEC_NOPORT;
            (*ppBufferHdr)->nOutputPortIndex = VIDDEC_OUTPUT_PORT;
        }
    }

    if (nPortIndex == VIDDEC_INPUT_PORT) {
        (*ppBufferHdr)->pInputPortPrivate = pComponentPrivate->pCompPort[nPortIndex]->pBufferPrivate[pBufferCnt];
    }
    else {
        (*ppBufferHdr)->pOutputPortPrivate = pComponentPrivate->pCompPort[nPortIndex]->pBufferPrivate[pBufferCnt];
    }
    pComponentPrivate->pCompPort[nPortIndex]->pBufferPrivate[pBufferCnt]->bAllocByComponent = OMX_FALSE;

    if (pCompPort->hTunnelComponent != NULL) {
        pComponentPrivate->pCompPort[nPortIndex]->pBufferPrivate[pBufferCnt]->eBufferOwner = VIDDEC_BUFFER_WITH_TUNNELEDCOMP;
    }
    else {
        pComponentPrivate->pCompPort[nPortIndex]->pBufferPrivate[pBufferCnt]->eBufferOwner = VIDDEC_BUFFER_WITH_CLIENT;
    }

    OMX_PRBUFFER1(pComponentPrivate->dbg, "ppBufferHdr 0x%p pBuffer 0x%p nAllocLen %lu eBufferOwner %d\n",
        *ppBufferHdr, pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer, (*ppBufferHdr)->nAllocLen,
        pCompPort->pBufferPrivate[pBufferCnt]->eBufferOwner);

    eError = VIDDEC_Allocate_DSPResources(pComponentPrivate, nPortIndex);
    if (eError != OMX_ErrorNone) {
        eError = OMX_ErrorInsufficientResources;
        pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                               OMX_EventError,
                                               eError,
                                               OMX_TI_ErrorMajor,
                                               NULL);
        goto EXIT;
    }

    pCompPort->nBufferCnt++;

    if (pCompPort->nBufferCnt == pPortDef->nBufferCountActual) {
        pPortDef->bPopulated = OMX_TRUE;
        if (nPortIndex == VIDDEC_INPUT_PORT) {
            VIDDEC_PTHREAD_SEMAPHORE_POST(pComponentPrivate->sInSemaphore);
        }
        else {
            VIDDEC_PTHREAD_SEMAPHORE_POST(pComponentPrivate->sOutSemaphore);
        }
    }
EXIT:
    if (pComponentPrivate)
        OMX_PRBUFFER1(pComponentPrivate->dbg, "---Exiting eError 0x%x\n", \
                                                         eError);
    return eError;  
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_FreeBuffer() 
  *
  * 
  * 
  *
  * @param 
  * @param 
  * @param 
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_FreeBuffer (OMX_IN OMX_HANDLETYPE hComponent,
                                 OMX_IN OMX_U32 nPortIndex,
                                 OMX_IN OMX_BUFFERHEADERTYPE* pBuffHead)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    VIDDEC_BUFFER_PRIVATE* pBufferPrivate = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDefIn = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDefOut = NULL;
    VIDDEC_PORT_TYPE* pCompPort = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = NULL;
    OMX_U32 i = 0;
    VIDDEC_PORT_TYPE* pInCompPort = NULL;
    OMX_U8 pInBufferCnt = 0;
    VIDDEC_PORT_TYPE* pOutCompPort = NULL;
    OMX_U8 pOutBufferCnt = 0;
    OMX_STATETYPE TunnelState = OMX_StateInvalid;
    OMX_BOOL bTransIdle = OMX_FALSE;

    OMX_CONF_CHECK_CMD(hComponent, pBuffHead, OMX_TRUE);

    pHandle = (OMX_COMPONENTTYPE*)hComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate;

    OMX_PRBUFFER1(pComponentPrivate->dbg, "+++Entering pHandle 0x%p pBuffHead 0x%p nPortIndex %lu nFilledLen %lx nAllocLen %lx\n",
        hComponent, pBuffHead, nPortIndex,pBuffHead->nFilledLen,pBuffHead->nAllocLen);

    pPortDefIn = pComponentPrivate->pInPortDef;
    pPortDefOut = pComponentPrivate->pOutPortDef;

    pInCompPort = pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT];
    pInBufferCnt = pInCompPort->nBufferCnt;
    pOutCompPort = pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT];
    pOutBufferCnt = pOutCompPort->nBufferCnt;

    if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
        pPortDef = pComponentPrivate->pInPortDef;
        pCompPort = pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT];
        pBufferPrivate = pBuffHead->pInputPortPrivate;
    }
    else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
        pPortDef = pComponentPrivate->pOutPortDef;
        pCompPort = pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT];
        pBufferPrivate = pBuffHead->pOutputPortPrivate;
    }
    else {
        eError = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
        pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingH263) {
        if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        }
        else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        }
        else {
            eError = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
    }
#ifdef VIDDEC_SPARK_CODE
    else if (VIDDEC_SPARKCHECK) {
        if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        }
        else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        }
        else {
            eError = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
    }
#endif
    else if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG2) {
        if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        } 
        else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        }
        else {
            eError = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
    }
    else if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
        if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        } 
        else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        }
        else {
            eError = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
    }
    else if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingWMV) {
        if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        }
        else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferPrivate->pUalgParam,OMX_PTR);
            pBufferPrivate->nUalgParamSize = 0;
        }
        else {
            eError = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
    }
    else {
        OMX_ERROR4(pComponentPrivate->dbg, "Error: Invalid Compression Type\n");
        goto EXIT;
    }
    OMX_PRBUFFER1(pComponentPrivate->dbg, "bAllocByComponent 0x%x pBuffer 0x%p original %p\n", (int )pBufferPrivate->bAllocByComponent,
        pBuffHead->pBuffer,pBufferPrivate->pOriginalBuffer);
    if (pBufferPrivate->bAllocByComponent == OMX_TRUE) {
        if(pBuffHead->pBuffer != NULL){
#ifdef __PERF_INSTRUMENTATION__
           PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                             pBuffHead->pBuffer,
                             pBuffHead->nAllocLen,
                             PERF_ModuleMemory);
#endif

      /* Freeing the original buffer position were data buffer was allocated */
           if(pBufferPrivate->pOriginalBuffer != NULL){
              OMX_FREE(pBufferPrivate->pOriginalBuffer);
              pBuffHead->pBuffer = NULL;
           } else{
              OMX_MEMFREE_STRUCT_DSPALIGN(pBuffHead->pBuffer, OMX_PTR);
           }
        }
    }

#ifdef USE_ION
		if(pComponentPrivate->pCompPort[OMX_VIDEODECODER_OUTPUT_PORT]->VIDDECBufferType == GrallocPointers
				&& nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex){
			if(pComponentPrivate->bUseIon == OMX_TRUE){
				if(pComponentPrivate->bMapIonBuffers == OMX_TRUE){
					munmap(pBuffHead->pPlatformPrivate, pBuffHead->nAllocLen);
					close(pComponentPrivate->mmap_fd[pOutBufferCnt-1]);
				}
			}
		}
#endif

    for(i = 0; i < pPortDef->nBufferCountActual; i++){
        if (pCompPort->pBufferPrivate[i]->pBufferHdr == pBuffHead){
            OMX_PRBUFFER1(pComponentPrivate->dbg, "buffcount %lu eBufferOwner 0x%x\n", i, pCompPort->pBufferPrivate[i]->eBufferOwner);
            free(pCompPort->pBufferPrivate[i]->pBufferHdr);
            pCompPort->pBufferPrivate[i]->pBufferHdr = NULL;
            pBuffHead = NULL;
            break;
        }
    }
    if(pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->hTunnelComponent != NULL){
        eError = OMX_GetState(pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->hTunnelComponent,&TunnelState);
        if(eError != OMX_ErrorNone) {
            OMX_ERROR4(pComponentPrivate->dbg, "GetState Invalid return\n");
            goto EXIT;
        }
        OMX_PRCOMM2(pComponentPrivate->dbg, "TunnelState %d\n", TunnelState);
    }
    if((pComponentPrivate->eState == OMX_StateIdle) && (pComponentPrivate->eIdleToLoad == OMX_StateLoaded)){
        bTransIdle = OMX_TRUE;
    }
    else {
        bTransIdle = OMX_FALSE;
    }
    if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
        pInBufferCnt--;
        pInCompPort->nBufferCnt--;
        if (pInBufferCnt == 0) {
            pPortDefIn->bPopulated = OMX_FALSE;
            if (nPortIndex == VIDDEC_INPUT_PORT) {
                VIDDEC_PTHREAD_SEMAPHORE_POST(pComponentPrivate->sInSemaphore);
            }
            else {
                VIDDEC_PTHREAD_SEMAPHORE_POST(pComponentPrivate->sOutSemaphore);
            }
            if (bTransIdle) {
                i = 0;
            }
            else if ((!pPortDef->bEnabled && (pComponentPrivate->eState == OMX_StateIdle ||
                      pComponentPrivate->eState == OMX_StateExecuting
                      || pComponentPrivate->eState == OMX_StatePause))) {
                i = 0;
            }
            else {
                if (pComponentPrivate->eState != OMX_StateInvalid) {
                pComponentPrivate->cbInfo.EventHandler (pComponentPrivate->pHandle,
                                                        pComponentPrivate->pHandle->pApplicationPrivate,
                                                        OMX_EventError,
                                                        OMX_ErrorPortUnpopulated,
                                                        OMX_TI_ErrorMinor,
                                                        "Input Port Unpopulated");
                }
            }
        }
    }

    else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
		pOutBufferCnt--;
		pOutCompPort->nBufferCnt--;
        if (pOutBufferCnt == 0) {
            pPortDefOut->bPopulated = OMX_FALSE;
            if (nPortIndex == VIDDEC_INPUT_PORT) {
                VIDDEC_PTHREAD_SEMAPHORE_POST(pComponentPrivate->sInSemaphore);
            }
            else {
                VIDDEC_PTHREAD_SEMAPHORE_POST(pComponentPrivate->sOutSemaphore);
            }
            if ((pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->hTunnelComponent == NULL) &&  bTransIdle) {
                i = 0;
            }
            else if ((pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT]->hTunnelComponent != NULL) &&  (TunnelState == OMX_StateIdle)) {
                i = 0;
            }
            else if ((!pPortDef->bEnabled && (pComponentPrivate->eState == OMX_StateIdle ||
                      pComponentPrivate->eState == OMX_StateExecuting
                      || pComponentPrivate->eState == OMX_StatePause))) {
                i = 0;
            }
            else {
                if (pComponentPrivate->eState != OMX_StateInvalid) {
                pComponentPrivate->cbInfo.EventHandler (pComponentPrivate->pHandle,
                                                        pComponentPrivate->pHandle->pApplicationPrivate,
                                                        OMX_EventError,
                                                        OMX_ErrorPortUnpopulated,
                                                        OMX_TI_ErrorMinor,
                                                        "Output Port Unpopulated");
                }
            }
        }
    }

EXIT:
    if (pComponentPrivate)
        OMX_PRBUFFER1(pComponentPrivate->dbg, "---Exiting eError 0x%x\n", \
                                                        eError);
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_AllocateBuffer() 
  *
  * 
  * 
  *
  * @param 
  * @param 
  * @param 
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_AllocateBuffer (OMX_IN OMX_HANDLETYPE hComponent,
                                     OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffHead,
                                     OMX_IN OMX_U32 nPortIndex,
                                     OMX_IN OMX_PTR pAppPrivate,
                                     OMX_IN OMX_U32 nSizeBytes)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = NULL;
    VIDDEC_PORT_TYPE* pCompPort = NULL;
    OMX_U8* pTemp = NULL;
    OMX_U8 pBufferCnt = 0;

    OMX_CONF_CHECK_CMD(hComponent, OMX_TRUE, OMX_TRUE);

    pHandle = (OMX_COMPONENTTYPE*)hComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate;

    OMX_PRBUFFER1(pComponentPrivate->dbg, "+++Entering pHandle 0x%p pBuffHead 0x%p nPortIndex 0x%lx nSizeBytes 0x%lx\n",
        hComponent, *pBuffHead, nPortIndex, nSizeBytes);


    if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
        pCompPort = pComponentPrivate->pCompPort[VIDDEC_INPUT_PORT];
        pBufferCnt = pCompPort->nBufferCnt;
        pPortDef = pComponentPrivate->pInPortDef;
    }
    else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
        pCompPort = pComponentPrivate->pCompPort[VIDDEC_OUTPUT_PORT];
        pBufferCnt = pCompPort->nBufferCnt;
        pPortDef = pComponentPrivate->pOutPortDef;
    }
    else {
        eError = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    OMX_MALLOC_STRUCT(pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr, OMX_BUFFERHEADERTYPE,pComponentPrivate->nMemUsage[VIDDDEC_Enum_MemLevel1]);
    if (!pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr) {
        eError = OMX_ErrorInsufficientResources;
        pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                               OMX_EventError,
                                               eError,
                                               OMX_TI_ErrorMajor,
                                               NULL);
        goto EXIT;
    }

    (*pBuffHead) = (OMX_BUFFERHEADERTYPE*)(pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr);
    memset(pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr, 0, sizeof(OMX_BUFFERHEADERTYPE));
    OMX_CONF_INIT_STRUCT(pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr, OMX_BUFFERHEADERTYPE, pComponentPrivate->dbg);
    /* Allocate Video Decoder buffer */
    /* Have additional size logic here to take care of the WMV internal code requirements
     * i.e. it requires atleast VIDDEC_WMV_BUFFER_OFFSET(251) bytes before actual buffer start size
     * but in that case it would affect 128 bytes alignment. Hence have incrememented by 256 bytes */
    OMX_MALLOC_SIZE_DSPALIGN(pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer,
                             (nSizeBytes + VIDDEC_PADDING_FULL),
                             OMX_U8);
    if (!(pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer)) {
        eError = OMX_ErrorInsufficientResources;
        pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                               OMX_EventError,
                                               eError,
                                               OMX_TI_ErrorMajor,
                                               NULL);
        goto EXIT;
    }
    /* Align and add padding for data buffer */
    pCompPort->pBufferPrivate[pBufferCnt]->pOriginalBuffer = pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer;
    pTemp = (OMX_U8*)pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer;
#ifdef VIDDEC_WMVPOINTERFIXED
    pTemp += VIDDEC_PADDING_FULL;
    pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer = (OMX_U8*)pTemp;
    pCompPort->pBufferPrivate[pBufferCnt]->pTempBuffer = pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer;
    pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->nOffset = 0;
#endif
    pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer = (OMX_U8*)pTemp;

    (*pBuffHead)->pBuffer = pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer;
    (*pBuffHead)->nAllocLen = nSizeBytes;
    (*pBuffHead)->pAppPrivate = pAppPrivate;
    (*pBuffHead)->pPlatformPrivate = NULL;
    (*pBuffHead)->pInputPortPrivate = NULL;
    (*pBuffHead)->pOutputPortPrivate = NULL;
    (*pBuffHead)->nFlags = VIDDEC_CLEARFLAGS;
    (*pBuffHead)->pMarkData = NULL;

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedFrame(pComponentPrivate->pPERFcomp,
                       (*pBuffHead)->pBuffer,
                       (*pBuffHead)->nAllocLen,
                       PERF_ModuleMemory);
#endif


    if (pCompPort->hTunnelComponent != NULL) {
        if (pPortDef->eDir == OMX_DirInput) {
            (*pBuffHead)->nInputPortIndex  = nPortIndex;
            (*pBuffHead)->nOutputPortIndex = pCompPort->nTunnelPort;
        }
        else {
            (*pBuffHead)->nInputPortIndex  = pCompPort->nTunnelPort;
            (*pBuffHead)->nOutputPortIndex = nPortIndex;
        }
    }
    else {
        if (nPortIndex == VIDDEC_INPUT_PORT) {
            (*pBuffHead)->nInputPortIndex  = VIDDEC_INPUT_PORT;
            (*pBuffHead)->nOutputPortIndex = VIDDEC_NOPORT;
        }
        else {
            (*pBuffHead)->nInputPortIndex  = VIDDEC_NOPORT;
            (*pBuffHead)->nOutputPortIndex = VIDDEC_OUTPUT_PORT;
        }
    }

    if (nPortIndex == VIDDEC_INPUT_PORT) {
        (*pBuffHead)->pInputPortPrivate = pCompPort->pBufferPrivate[pBufferCnt];
        (*pBuffHead)->pOutputPortPrivate = NULL;
    }
    else {
        (*pBuffHead)->pOutputPortPrivate = pCompPort->pBufferPrivate[pBufferCnt];
        (*pBuffHead)->pInputPortPrivate = NULL;
    }
    pCompPort->pBufferPrivate[pBufferCnt]->bAllocByComponent = OMX_TRUE;

    if (pCompPort->hTunnelComponent != NULL) {
        pCompPort->pBufferPrivate[pBufferCnt]->eBufferOwner = VIDDEC_BUFFER_WITH_TUNNELEDCOMP;
    }
    else {
        pCompPort->pBufferPrivate[pBufferCnt]->eBufferOwner = VIDDEC_BUFFER_WITH_CLIENT;
    }

    pPortDef->nBufferSize = nSizeBytes;
    OMX_PRBUFFER1(pComponentPrivate->dbg, "pBuffHead 0x%p nAllocLen 0x%lx pBuffer %p eBufferOwner %d\n",
        *pBuffHead, (*pBuffHead)->nAllocLen, pCompPort->pBufferPrivate[pBufferCnt]->pBufferHdr->pBuffer, 
        pCompPort->pBufferPrivate[pBufferCnt]->eBufferOwner);

    eError = VIDDEC_Allocate_DSPResources(pComponentPrivate, nPortIndex);
    if (eError != OMX_ErrorNone) {
        OMX_PRDSP4(pComponentPrivate->dbg, "Error: Allocating DSP resources\n");
        eError = OMX_ErrorInsufficientResources;
        pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                               OMX_EventError,
                                               eError,
                                               OMX_TI_ErrorMajor,
                                               NULL);
        goto EXIT;
    }

    pCompPort->nBufferCnt++;
    pCompPort->pBufferPrivate[pBufferCnt]->nNumber = pCompPort->nBufferCnt;

    OMX_PRBUFFER1(pComponentPrivate->dbg, "eBufferOwner 0x%x nBufferCountActual %lu nBufferCnt %u nnumber %lu\n",
        pCompPort->pBufferPrivate[pBufferCnt]->eBufferOwner, pPortDef->nBufferCountActual, 
        pCompPort->nBufferCnt,pCompPort->pBufferPrivate[pBufferCnt]->nNumber);
    if (pCompPort->nBufferCnt == pPortDef->nBufferCountActual) {
        pPortDef->bPopulated = OMX_TRUE;
            if (nPortIndex == VIDDEC_INPUT_PORT) {
                VIDDEC_PTHREAD_SEMAPHORE_POST(pComponentPrivate->sInSemaphore);
            }
            else {
                VIDDEC_PTHREAD_SEMAPHORE_POST(pComponentPrivate->sOutSemaphore);
            }
    }

EXIT:
    if (pComponentPrivate)
        OMX_PRBUFFER1(pComponentPrivate->dbg, "---Exiting eError 0x%x\n", \
                                                         eError);
    return eError;
}

#ifdef KHRONOS_1_1
/*-------------------------------------------------------------------*/
/**
  * IsTIOMXComponent()
  *
  * Check if the component is TI component.
  *
  * @param hTunneledComp Component Tunnel Pipe
  *  
  * @retval OMX_TRUE   Input is a TI component.
  *         OMX_FALSE  Input is a not a TI component. 
  *
  **/
/*-------------------------------------------------------------------*/
static OMX_BOOL IsTIOMXComponent(OMX_HANDLETYPE hComp, struct OMX_TI_Debug *dbg)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_STRING pTunnelcComponentName = NULL;
    OMX_VERSIONTYPE* pTunnelComponentVersion = NULL;
    OMX_VERSIONTYPE* pSpecVersion = NULL;
    OMX_UUIDTYPE* pComponentUUID = NULL;
    char *pSubstring = NULL;
    OMX_BOOL bResult = OMX_TRUE;

    pTunnelcComponentName = malloc(OMX_MAX_STRINGNAME_SIZE);
    if (pTunnelcComponentName == NULL) {
        eError = OMX_ErrorInsufficientResources;
        OMX_TRACE4(*dbg, "Error in Video Decoder OMX_ErrorInsufficientResources\n");
        goto EXIT;
    }

    pTunnelComponentVersion = malloc(sizeof(OMX_VERSIONTYPE));
    if (pTunnelComponentVersion == NULL) {
        OMX_TRACE4(*dbg, "Error in Video Decoder OMX_ErrorInsufficientResources\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pSpecVersion = malloc(sizeof(OMX_VERSIONTYPE));
    if (pSpecVersion == NULL) {
        OMX_TRACE4(*dbg, "Error in Video Decoder OMX_ErrorInsufficientResources\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pComponentUUID = malloc(sizeof(OMX_UUIDTYPE));
    if (pComponentUUID == NULL) {
        OMX_TRACE4(*dbg, "Error in Video Decoder OMX_ErrorInsufficientResources\n");
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    eError = OMX_GetComponentVersion (hComp, pTunnelcComponentName, pTunnelComponentVersion, pSpecVersion, pComponentUUID);

    /* Check if tunneled component is a TI component */
    pSubstring = strstr(pTunnelcComponentName, "OMX.TI.");
    if(pSubstring == NULL) {
                bResult = OMX_FALSE;
    }

EXIT:
    free(pTunnelcComponentName);
    free(pTunnelComponentVersion);
    free(pSpecVersion);
    free(pComponentUUID);

    return bResult;
} /* End of IsTIOMXComponent */
#endif

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_VerifyTunnelConnection() 
  *
  * 
  * 
  *
  * @param         
  * @param    
  * @param 
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_VerifyTunnelConnection (VIDDEC_PORT_TYPE *pPort, 
                                             OMX_HANDLETYPE hTunneledComp,
                                             OMX_PARAM_PORTDEFINITIONTYPE* pPortDef)
{
    OMX_PARAM_PORTDEFINITIONTYPE sPortDef;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    sPortDef.nSize                    = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    sPortDef.nVersion.s.nVersionMajor = VERSION_MAJOR;
    sPortDef.nVersion.s.nVersionMinor = VERSION_MINOR;
    sPortDef.nVersion.s.nRevision     = VERSION_REVISION;
    sPortDef.nVersion.s.nStep         = VERSION_STEP;
    sPortDef.nPortIndex               = pPort->nTunnelPort;

    eError = OMX_GetParameter(hTunneledComp, OMX_IndexParamPortDefinition, &sPortDef);
    if (eError != OMX_ErrorNone) {
        return eError;
    }

    switch (pPortDef->eDomain) {
        case OMX_PortDomainOther:
            if (sPortDef.format.other.eFormat != pPortDef->format.other.eFormat) {
                pPort->hTunnelComponent = 0;
                pPort->nTunnelPort      = 0;
                return OMX_ErrorPortsNotCompatible;
            }
            break;
        case OMX_PortDomainAudio:
            if (sPortDef.format.audio.eEncoding != pPortDef->format.audio.eEncoding) {
                pPort->hTunnelComponent = 0;
                pPort->nTunnelPort      = 0;
                return OMX_ErrorPortsNotCompatible;
            }
            break;
        case OMX_PortDomainVideo:
            if (sPortDef.format.video.eCompressionFormat != pPortDef->format.video.eCompressionFormat) {
                pPort->hTunnelComponent = 0;
                pPort->nTunnelPort      = 0;
                return OMX_ErrorPortsNotCompatible;
            }
            break;
        case OMX_PortDomainImage:
            if (sPortDef.format.image.eCompressionFormat != pPortDef->format.image.eCompressionFormat) {
                pPort->hTunnelComponent = 0;
                pPort->nTunnelPort      = 0;
                return OMX_ErrorPortsNotCompatible;
            }
            break;
        default: 
            pPort->hTunnelComponent = 0;
            pPort->nTunnelPort      = 0;
            return OMX_ErrorPortsNotCompatible;     /* Our current port is not set up correctly */
    }
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_ComponentTunnelRequest() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * the application.
  *
  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_ComponentTunnelRequest (OMX_IN OMX_HANDLETYPE hComponent,
                                             OMX_IN OMX_U32 nPort,
                                             OMX_IN OMX_HANDLETYPE hTunneledComp,
                                             OMX_IN OMX_U32 nTunneledPort,
                                             OMX_INOUT OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = NULL;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    OMX_PARAM_BUFFERSUPPLIERTYPE sBufferSupplier;
    VIDDEC_PORT_TYPE *pPort = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef = NULL;

    OMX_CONF_CHECK_CMD(hComponent, OMX_TRUE, OMX_TRUE);

    pHandle = (OMX_COMPONENTTYPE*)hComponent;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pHandle->pComponentPrivate;

    pPort = pComponentPrivate->pCompPort[nPort];
    pPortDef = pComponentPrivate->pInPortDef;

    if (nPort == pComponentPrivate->pInPortFormat->nPortIndex) {
        pPortDef = pComponentPrivate->pInPortDef;
    }
    else if (nPort == pComponentPrivate->pOutPortFormat->nPortIndex) {
        pPortDef = pComponentPrivate->pOutPortDef;
    }

    if (pTunnelSetup == NULL || hTunneledComp == 0) {
        pPort->hTunnelComponent = NULL;
        pPort->nTunnelPort = 0;
        pPort->eSupplierSetting = OMX_BufferSupplyUnspecified;
    }
    else {
        if (pPortDef->eDir != OMX_DirInput && pPortDef->eDir != OMX_DirOutput) {
            return OMX_ErrorBadParameter;
        }

#ifdef KHRONOS_1_1
        /* Check if the other component is developed by TI */
        if(IsTIOMXComponent(hTunneledComp, &pComponentPrivate->dbg) != OMX_TRUE) {
            eError = OMX_ErrorTunnelingUnsupported;
            goto EXIT;
        }
#endif
        pPort->hTunnelComponent = hTunneledComp;
        pPort->nTunnelPort = nTunneledPort;

        if (pPortDef->eDir == OMX_DirOutput) {
            pTunnelSetup->eSupplier = pPort->eSupplierSetting;
        }
        else {
            /* Component is the input (sink of data) */
            eError = VIDDEC_VerifyTunnelConnection(pPort, hTunneledComp, pPortDef);
            if (OMX_ErrorNone != eError) {
                OMX_ERROR4(pComponentPrivate->dbg, "Error: PP VerifyTunnelConnection failed\n");
                /* Invalid connection formats. Return eError */
                return OMX_ErrorPortsNotCompatible;
            }

            /* If specified, obey output port's preferences. Otherwise choose output */
            pPort->eSupplierSetting = pTunnelSetup->eSupplier;
            if (OMX_BufferSupplyUnspecified == pPort->eSupplierSetting) {
                pPort->eSupplierSetting = pTunnelSetup->eSupplier = OMX_BufferSupplyOutput;
            }

            /* Tell the output port who the supplier is */
            sBufferSupplier.nSize                    = sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE);
            sBufferSupplier.nVersion.s.nVersionMajor = VERSION_MAJOR;
            sBufferSupplier.nVersion.s.nVersionMinor = VERSION_MAJOR;
            sBufferSupplier.nVersion.s.nRevision     = VERSION_REVISION;
            sBufferSupplier.nVersion.s.nStep         = VERSION_STEP;
            sBufferSupplier.nPortIndex               = nTunneledPort;
            sBufferSupplier.eBufferSupplier          = pPort->eSupplierSetting;

            eError = OMX_SetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &sBufferSupplier);

            eError = OMX_GetParameter(hTunneledComp, OMX_IndexParamCompBufferSupplier, &sBufferSupplier);

            if (sBufferSupplier.eBufferSupplier != pPort->eSupplierSetting) {
                OMX_ERROR4(pComponentPrivate->dbg, "SetParameter: OMX_IndexParamCompBufferSupplier failed to change setting\n" );
                return OMX_ErrorUndefined;
            }
        }
    }

EXIT:
    return eError;
}

/*----------------------------------------------------------------------------*/
/**
  *  VIDDEC_Allocate_DSPResources() 
  *
  * 
  * 
  *
  * @param 
  * @param 
  * @param 
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*----------------------------------------------------------------------------*/

static OMX_ERRORTYPE VIDDEC_Allocate_DSPResources(VIDDEC_COMPONENT_PRIVATE *pComponentPrivate, OMX_IN OMX_U32 nPortIndex)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    /*todo remove array, fix it to variable,
    it means just one variable for both index*/
    void *pUalgOutParams[1];
    void *pUalgInpParams[1];

    OMX_U8 nBufferCnt = pComponentPrivate->pCompPort[nPortIndex]->nBufferCnt;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDefIn = pComponentPrivate->pInPortDef;
    VIDDEC_BUFFER_PRIVATE* pBufferPrivate = NULL;
    OMX_U32 nParamSize = 0;

    pBufferPrivate =
         (VIDDEC_BUFFER_PRIVATE*)(pComponentPrivate->pCompPort[nPortIndex]->pBufferPrivate[nBufferCnt]);

    if (nPortIndex == pComponentPrivate->pInPortFormat->nPortIndex) {
        if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
            pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingH263) {
            nParamSize = sizeof(MP4VD_GPP_SN_UALGInputParams);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgInpParams[0], nParamSize, MP4VD_GPP_SN_UALGInputParams);
            if ((MP4VD_GPP_SN_UALGInputParams*)(!pUalgInpParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (MP4VD_GPP_SN_UALGInputParams*)(pUalgInpParams[0]);
            pBufferPrivate->nUalgParamSize = sizeof(MP4VD_GPP_SN_UALGInputParams);
        }
#ifdef VIDDEC_SPARK_CODE
        else if (VIDDEC_SPARKCHECK) {
            nParamSize = sizeof(SPARKVD_GPP_SN_UALGInputParams);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgInpParams[0], nParamSize, SPARKVD_GPP_SN_UALGInputParams);
            if ((SPARKVD_GPP_SN_UALGInputParams*)(!pUalgInpParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (SPARKVD_GPP_SN_UALGInputParams*)(pUalgInpParams[0]);
            pBufferPrivate->nUalgParamSize = sizeof(SPARKVD_GPP_SN_UALGInputParams);
        }
#endif
        else if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG2) {
            nParamSize = sizeof(MP2VDEC_UALGInputParam);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgInpParams[0], nParamSize, MP2VDEC_UALGInputParam);
            if ((MP2VDEC_UALGInputParam*)(!pUalgInpParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (MP2VDEC_UALGInputParam*)(pUalgInpParams[0]);
            pBufferPrivate->nUalgParamSize = sizeof(MP2VDEC_UALGInputParam);
        }
        else if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
            nParamSize = sizeof(H264VDEC_UALGInputParam);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgInpParams[0], nParamSize, H264VDEC_UALGInputParam);
            if ((H264VDEC_UALGInputParam*)(!pUalgInpParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (H264VDEC_UALGInputParam*)(pUalgInpParams[0]);
            pBufferPrivate->nUalgParamSize = sizeof(H264VDEC_UALGInputParam);
        }
        else if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingWMV) {
            nParamSize = sizeof(WMV9DEC_UALGInputParam);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgInpParams[0], nParamSize, WMV9DEC_UALGInputParam);
            if ((WMV9DEC_UALGInputParam*)(!pUalgInpParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (WMV9DEC_UALGInputParam*)(pUalgInpParams[0]);
            pBufferPrivate->nUalgParamSize = sizeof(WMV9DEC_UALGInputParam);
        }
        else {
            OMX_ERROR4(pComponentPrivate->dbg, "Error: Invalid Compression Type\n");
            goto EXIT;
        }
    }
    else if (nPortIndex == pComponentPrivate->pOutPortFormat->nPortIndex) {
        if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG4 ||
            pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingH263) {
            nParamSize = sizeof(MP4VD_GPP_SN_UALGOutputParams);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgOutParams[0], nParamSize, MP4VD_GPP_SN_UALGOutputParams);
            if ((MP4VD_GPP_SN_UALGOutputParams*)(!pUalgOutParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (MP4VD_GPP_SN_UALGOutputParams*)(pUalgOutParams[0]);
            pBufferPrivate->nUalgParamSize = sizeof(MP4VD_GPP_SN_UALGOutputParams);
        }
#ifdef VIDDEC_SPARK_CODE
        else if (VIDDEC_SPARKCHECK) {
            nParamSize = sizeof(SPARKVD_GPP_SN_UALGOutputParams);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgOutParams[0], nParamSize, SPARKVD_GPP_SN_UALGOutputParams);
            if ((SPARKVD_GPP_SN_UALGOutputParams*)(!pUalgOutParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (SPARKVD_GPP_SN_UALGOutputParams*)(pUalgOutParams[0]);
            pBufferPrivate->nUalgParamSize = sizeof(SPARKVD_GPP_SN_UALGOutputParams);
        }
#endif
        else if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingMPEG2) {
            nParamSize = sizeof(MP2VDEC_UALGOutputParam);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgOutParams[0], nParamSize, MP2VDEC_UALGOutputParam);
            if ((MP2VDEC_UALGOutputParam*)(!pUalgOutParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (MP2VDEC_UALGOutputParam*)(pUalgOutParams[0]); 
            pBufferPrivate->nUalgParamSize = sizeof(MP2VDEC_UALGOutputParam);
        }
        else if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingAVC) {
            nParamSize = sizeof(H264VDEC_UALGOutputParam);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgOutParams[0], nParamSize, H264VDEC_UALGOutputParam);
            if ((H264VDEC_UALGOutputParam*)(!pUalgOutParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (H264VDEC_UALGOutputParam*)(pUalgOutParams[0]); 
            pBufferPrivate->nUalgParamSize = sizeof(H264VDEC_UALGOutputParam);
        }
        else if (pPortDefIn->format.video.eCompressionFormat == OMX_VIDEO_CodingWMV) {
            nParamSize = sizeof(WMV9DEC_UALGOutputParam);
            OMX_MALLOC_SIZE_DSPALIGN(pUalgOutParams[0], nParamSize, WMV9DEC_UALGOutputParam);
            if ((WMV9DEC_UALGOutputParam*)(!pUalgOutParams[0])) {
                OMX_TRACE4(pComponentPrivate->dbg, "Error: Malloc failed\n");
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pBufferPrivate->pUalgParam = (WMV9DEC_UALGOutputParam*)(pUalgOutParams[0]); 
            pBufferPrivate->nUalgParamSize = sizeof(WMV9DEC_UALGOutputParam);
        }
        else {
            OMX_ERROR4(pComponentPrivate->dbg, "Error: Invalid Compression Type\n");
            eError = OMX_ErrorUnsupportedSetting;
            goto EXIT;
        }
    } 
    else {
        eError = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

EXIT:
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  * VIDDEC_GetExtensionIndex() 
  *
  * 
  *
  * @retval OMX_ErrorNone                    Successful operation.
  *         OMX_ErrorBadParameter            Invalid operation.    
  *         OMX_ErrorIncorrectStateOperation If called when port is disabled.
  **/
/*-------------------------------------------------------------------*/
static OMX_ERRORTYPE VIDDEC_GetExtensionIndex(OMX_IN OMX_HANDLETYPE hComponent, OMX_IN OMX_STRING cParameterName, OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    OMX_U32 nIndex;
    OMX_ERRORTYPE eError = OMX_ErrorUndefined;

    OMX_CONF_CHECK_CMD(hComponent, OMX_TRUE, OMX_TRUE);
    for(nIndex = 0; nIndex < sizeof(sVideoDecCustomParams)/sizeof(VIDDEC_CUSTOM_PARAM); nIndex++) {
        if(strcmp((char *)cParameterName, (char *)&(sVideoDecCustomParams[nIndex].cCustomParamName)) == 0) {
            *pIndexType = sVideoDecCustomParams[nIndex].nCustomParamIndex;
            eError = OMX_ErrorNone;
            break;
        }
    }
EXIT:
    return eError;
}

#ifdef KHRONOS_1_1
/*-------------------------------------------------------------------*/
/**
  * ComponentRoleEnum() 
  *
  * 
  *
  * @retval OMX_ErrorNone                    Successful operation.
  *         OMX_ErrorBadParameter            Invalid operation.    
  *         OMX_ErrorIncorrectStateOperation If called when port is disabled.
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE ComponentRoleEnum(
        OMX_IN OMX_HANDLETYPE hComponent,
                OMX_OUT OMX_U8 *cRole,
                OMX_IN OMX_U32 nIndex)
{
    VIDDEC_COMPONENT_PRIVATE *pComponentPrivate;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    memset(cRole, 0x0, OMX_MAX_STRINGNAME_SIZE);
    switch (nIndex) {
        case VIDDEC_DEFAULT_INPUT_INDEX_H263:
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_H263);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)cRole, VIDDEC_COMPONENTROLES_H263);
            break;
        case VIDDEC_DEFAULT_INPUT_INDEX_H264:
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_H264);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)cRole, VIDDEC_COMPONENTROLES_H264);
            break;
        case VIDDEC_DEFAULT_INPUT_INDEX_MPEG2:
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_MPEG2);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)cRole, VIDDEC_COMPONENTROLES_MPEG2);
            break;
        case VIDDEC_DEFAULT_INPUT_INDEX_MPEG4:
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_MPEG4);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)cRole, VIDDEC_COMPONENTROLES_MPEG4);
            break;
        case VIDDEC_DEFAULT_INPUT_INDEX_WMV9:
            eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_WMV9);
            if( pComponentPrivate->pOutPortFormat->eColorFormat != VIDDEC_COLORFORMAT420) {
                eError = VIDDEC_Load_Defaults(pComponentPrivate, VIDDEC_INIT_PLANAR420);
            }
            strcpy((char*)cRole, VIDDEC_COMPONENTROLES_WMV9);
            break;
        /*todo add spark, it was not added because it is not in khronos spec, yet*/
        default:
            eError = OMX_ErrorNoMore;
            break;
    }
    if(eError != OMX_ErrorNone) {
        goto EXIT;
    }

EXIT:
    return eError;
}
#endif


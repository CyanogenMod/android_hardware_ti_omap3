
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
* =========================================================================== */
/**
* @file OMX_iLBCFrEncoder.c
*
* This file implements OpenMAX (TM) 1.0 Specific APIs and its functionality
* that is fully compliant with the Khronos OpenMAX (TM) 1.0 Specification
*
* @path  $(CSLPATH)\OMAPSW_MPU\linux\audio\src\openmax_il\ilbc_enc\src
*
* @rev  1.0
*/
/* ----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 04-Jul-2008 ad: Update for June 08 code review findings.
*! 10-Feb-2008 on: Initial Version
*! This is newest file
* =========================================================================== */
/* ------compilation control switches -------------------------*/
/****************************************************************
*  INCLUDE FILES
****************************************************************/
/* ----- system and platform files ----------------------------*/
#ifdef UNDER_CE
#include <windows.h>
#include <oaf_osal.h>
#include <omx_core.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>
#endif

#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <dbapi.h>

#ifdef DSP_RENDERING_ON
    #include <AudioManagerAPI.h>
#endif

#ifdef RESOURCE_MANAGER_ENABLED
    #include <ResourceManagerProxyAPI.h>
#endif

#ifdef __PERF_INSTRUMENTATION__
    #include "perf.h"
#endif

/*-------program files ----------------------------------------*/
#include <OMX_Component.h>

#include "OMX_iLBCEnc_Utils.h"

/****************************************************************
*  EXTERNAL REFERENCES NOTE : only use if not found in header file
****************************************************************/
/*--------data declarations -----------------------------------*/

/*--------function prototypes ---------------------------------*/

/****************************************************************
*  PUBLIC DECLARATIONS Defined here, used elsewhere
****************************************************************/
/*--------data declarations -----------------------------------*/

/*--------function prototypes ---------------------------------*/

/****************************************************************
*  PRIVATE DECLARATIONS Defined here, used only here
****************************************************************/
/*--------data declarations -----------------------------------*/

/*--------function prototypes ---------------------------------*/
#define AMRNB_DEC_ROLE "audio_encoder.amrnb"
static OMX_ERRORTYPE SetCallbacks (OMX_HANDLETYPE hComp,
        OMX_CALLBACKTYPE* pCallBacks, OMX_PTR pAppData);
static OMX_ERRORTYPE GetComponentVersion (OMX_HANDLETYPE hComp,
                                          OMX_STRING pComponentName,
                                          OMX_VERSIONTYPE* pComponentVersion,
                                          OMX_VERSIONTYPE* pSpecVersion,
                                          OMX_UUIDTYPE* pComponentUUID);
static OMX_ERRORTYPE SendCommand (OMX_HANDLETYPE hComp, OMX_COMMANDTYPE nCommand,
       OMX_U32 nParam,OMX_PTR pCmdData);
static OMX_ERRORTYPE GetParameter(OMX_HANDLETYPE hComp, OMX_INDEXTYPE nParamIndex,
                                  OMX_PTR ComponentParamStruct);
static OMX_ERRORTYPE SetParameter (OMX_HANDLETYPE hComp,
                                   OMX_INDEXTYPE nParamIndex,
                                   OMX_PTR ComponentParamStruct);
static OMX_ERRORTYPE GetConfig (OMX_HANDLETYPE hComp,
                                OMX_INDEXTYPE nConfigIndex,
                                OMX_PTR pComponentConfigStructure);
static OMX_ERRORTYPE SetConfig (OMX_HANDLETYPE hComp,
                                OMX_INDEXTYPE nConfigIndex,
                                OMX_PTR pComponentConfigStructure);

static OMX_ERRORTYPE EmptyThisBuffer (OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE FillThisBuffer (OMX_HANDLETYPE hComp, OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE GetState (OMX_HANDLETYPE hComp, OMX_STATETYPE* pState);
static OMX_ERRORTYPE ComponentTunnelRequest (OMX_HANDLETYPE hComp,
                                             OMX_U32 nPort, OMX_HANDLETYPE hTunneledComp,
                                             OMX_U32 nTunneledPort,
                                             OMX_TUNNELSETUPTYPE* pTunnelSetup);
static OMX_ERRORTYPE ComponentDeInit(OMX_HANDLETYPE pHandle);
static OMX_ERRORTYPE AllocateBuffer (OMX_IN OMX_HANDLETYPE hComponent,
                   OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
                   OMX_IN OMX_U32 nPortIndex,
                   OMX_IN OMX_PTR pAppPrivate,
                   OMX_IN OMX_U32 nSizeBytes);

static OMX_ERRORTYPE FreeBuffer(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_U32 nPortIndex,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE UseBuffer (
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer);

static OMX_ERRORTYPE GetExtensionIndex(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_STRING cParameterName,
            OMX_OUT OMX_INDEXTYPE* pIndexType);

static OMX_ERRORTYPE ComponentRoleEnum(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_OUT OMX_U8 *cRole,
        OMX_IN OMX_U32 nIndex);                 

/* interface with audio manager*/
#define FIFO1 "/dev/fifo.1"
#define FIFO2 "/dev/fifo.2"
#define PERMS 0666



/*-------------------------------------------------------------------*/
/**
  * OMX_ComponentInit() Set the all the function pointers of component
  *
  * This method will update the component function pointer to the handle
  *
  * @param hComp         handle for this instance of the component
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_ErrorInsufficientResources If the newmalloc fails
  **/
/*-------------------------------------------------------------------*/
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComp)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef_ip = NULL, *pPortDef_op = NULL;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_AUDIO_PARAM_PCMMODETYPE *ilbc_ip = NULL;
    OMX_AUDIO_PARAM_GSMEFRTYPE  *ilbc_op = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE*) hComp;
    ILBCENC_PORT_TYPE *pCompPort = NULL;
    OMX_AUDIO_PARAM_PORTFORMATTYPE *pPortFormat = NULL;
    int i = 0;

    ILBCENC_DPRINT("%d Entering OMX_ComponentInit\n", __LINE__);
    /*Set the all component function pointer to the handle */
    pHandle->SetCallbacks = SetCallbacks;
    pHandle->GetComponentVersion = GetComponentVersion;
    pHandle->SendCommand = SendCommand;
    pHandle->GetParameter = GetParameter;
    pHandle->SetParameter = SetParameter;
    pHandle->GetConfig = GetConfig;
    pHandle->SetConfig = SetConfig;
    pHandle->GetState = GetState;
    pHandle->EmptyThisBuffer = EmptyThisBuffer;
    pHandle->FillThisBuffer = FillThisBuffer;
    pHandle->ComponentTunnelRequest = ComponentTunnelRequest;
    pHandle->ComponentDeInit = ComponentDeInit;
    pHandle->AllocateBuffer = AllocateBuffer;
    pHandle->FreeBuffer = FreeBuffer;
    pHandle->UseBuffer = UseBuffer;
    pHandle->GetExtensionIndex = GetExtensionIndex;
    pHandle->ComponentRoleEnum = ComponentRoleEnum;


    /*Allocate the memory for Component private data area */
    OMX_MALLOC_GENERIC(pHandle->pComponentPrivate, ILBCENC_COMPONENT_PRIVATE);

    ((ILBCENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->pHandle = pHandle;
    pComponentPrivate = pHandle->pComponentPrivate;

#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate->pPERF = PERF_Create(PERF_FOURCC('N','B','E','_'),
                                           PERF_ModuleLLMM | PERF_ModuleAudioDecode);
#endif

    OMX_MALLOC_GENERIC(pCompPort, ILBCENC_PORT_TYPE);
    pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT] = pCompPort;

    OMX_MALLOC_GENERIC(pCompPort, ILBCENC_PORT_TYPE);
    pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT] = pCompPort;

    OMX_MALLOC_GENERIC(pComponentPrivate->sPortParam, OMX_PORT_PARAM_TYPE);
    OMX_ILBCCONF_INIT_STRUCT(pComponentPrivate->sPortParam, OMX_PORT_PARAM_TYPE);

    /* Initialize sPortParam data structures to default values */
    pComponentPrivate->sPortParam->nPorts = 0x2;
    pComponentPrivate->sPortParam->nStartPortNumber = 0x0;

    /* Malloc and Set pPriorityMgmt defaults */
    OMX_MALLOC_GENERIC(pComponentPrivate->sPriorityMgmt, OMX_PRIORITYMGMTTYPE);
    OMX_ILBCCONF_INIT_STRUCT(pComponentPrivate->sPriorityMgmt, OMX_PRIORITYMGMTTYPE);

    /* Initialize sPriorityMgmt data structures to default values */
    pComponentPrivate->sPriorityMgmt->nGroupPriority = -1;
    pComponentPrivate->sPriorityMgmt->nGroupID = -1;

    OMX_MALLOC_GENERIC(ilbc_ip, OMX_AUDIO_PARAM_PCMMODETYPE);
    OMX_ILBCCONF_INIT_STRUCT(ilbc_ip, OMX_AUDIO_PARAM_PCMMODETYPE);
    pComponentPrivate->pcmParams = ilbc_ip;
    ilbc_ip->nPortIndex = ILBCENC_INPUT_PORT;

    OMX_MALLOC_GENERIC(ilbc_op, OMX_AUDIO_PARAM_GSMEFRTYPE);
    OMX_ILBCCONF_INIT_STRUCT(ilbc_op, OMX_AUDIO_PARAM_GSMEFRTYPE);
    pComponentPrivate->ilbcParams = ilbc_op;
    ilbc_op->nPortIndex = ILBCENC_OUTPUT_PORT;

    /* newmalloc and initialize number of input buffers */
    OMX_MALLOC_GENERIC(pComponentPrivate->pInputBufferList, ILBCENC_BUFFERLIST);
    pComponentPrivate->pInputBufferList->numBuffers = 0;

    /* newmalloc and initialize number of output buffers */
    OMX_MALLOC_GENERIC(pComponentPrivate->pOutputBufferList, ILBCENC_BUFFERLIST);
    pComponentPrivate->pOutputBufferList->numBuffers = 0;

    for (i=0; i < ILBCENC_MAX_NUM_OF_BUFS; i++) {
        pComponentPrivate->pOutputBufferList->pBufHdr[i] = NULL;
        pComponentPrivate->pInputBufferList->pBufHdr[i] = NULL;
    }

    /* Set input port defaults */
    OMX_MALLOC_GENERIC(pPortDef_ip, OMX_PARAM_PORTDEFINITIONTYPE);
    OMX_ILBCCONF_INIT_STRUCT(pPortDef_ip, OMX_PARAM_PORTDEFINITIONTYPE);
    pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT] = pPortDef_ip;
    pComponentPrivate->bPreempted = OMX_FALSE; 

    pPortDef_ip->nSize                              = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    pPortDef_ip->nPortIndex                         = ILBCENC_INPUT_PORT;
    pPortDef_ip->eDir                               = OMX_DirInput;
    pPortDef_ip->nBufferCountActual                 = ILBCENC_NUM_INPUT_BUFFERS;
    pPortDef_ip->nBufferCountMin                    = ILBCENC_NUM_INPUT_BUFFERS;
    pPortDef_ip->nBufferSize                        = ILBCENC_PRIMARY_INPUT_FRAME_SIZE;
    pPortDef_ip->bEnabled                           = OMX_TRUE;
    pPortDef_ip->bPopulated                         = OMX_FALSE;
    pPortDef_ip->eDomain                            = OMX_PortDomainAudio;
    pPortDef_ip->format.audio.eEncoding             = OMX_AUDIO_CodingPCM;
    pPortDef_ip->format.audio.pNativeRender         = NULL;
    pPortDef_ip->format.audio.bFlagErrorConcealment = OMX_FALSE;

    /* Set output port defaults */
    OMX_MALLOC_GENERIC(pPortDef_op, OMX_PARAM_PORTDEFINITIONTYPE);
    OMX_ILBCCONF_INIT_STRUCT(pPortDef_op, OMX_PARAM_PORTDEFINITIONTYPE);
    pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT] = pPortDef_op;

    pPortDef_op->nSize                              = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
    pPortDef_op->nPortIndex                         = ILBCENC_OUTPUT_PORT;
    pPortDef_op->eDir                               = OMX_DirOutput;
    pPortDef_op->nBufferCountMin                    = ILBCENC_NUM_OUTPUT_BUFFERS;
    pPortDef_op->nBufferCountActual                 = ILBCENC_NUM_OUTPUT_BUFFERS;
    pPortDef_op->nBufferSize                        = ILBCENC_PRIMARY_OUTPUT_FRAME_SIZE;
    pPortDef_op->bEnabled                           = OMX_TRUE;
    pPortDef_op->bPopulated                         = OMX_FALSE;
    pPortDef_op->eDomain                            = OMX_PortDomainAudio;
    pPortDef_op->format.audio.eEncoding             = OMX_IndexCustomiLBCCodingType;
    pPortDef_op->format.audio.pNativeRender         = NULL;
    pPortDef_op->format.audio.bFlagErrorConcealment = OMX_FALSE;

    OMX_MALLOC_GENERIC(pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT]->pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    OMX_ILBCCONF_INIT_STRUCT(pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT]->pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    /* Set input port format defaults */
    pPortFormat = pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT]->pPortFormat;
    OMX_ILBCCONF_INIT_STRUCT(pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    pPortFormat->nPortIndex         = ILBCENC_INPUT_PORT;
    pPortFormat->nIndex             = OMX_IndexParamAudioILBC;
    pPortFormat->eEncoding          = OMX_AUDIO_CodingPCM;  /*Data Expected on Input Port*/
  
    strcpy((char*)pComponentPrivate->componentRole.cRole, "audio_encoder.amrnb");  

    /* Inupt port params */
    ilbc_ip->nPortIndex = ILBCENC_INPUT_PORT; 
    ilbc_ip->nChannels = 2; 
    ilbc_ip->eNumData= OMX_NumericalDataSigned; 
    ilbc_ip->nBitPerSample = 16;  
    ilbc_ip->nSamplingRate = 44100;           
    ilbc_ip->ePCMMode = OMX_AUDIO_PCMModeLinear; 
    ilbc_ip->bInterleaved = OMX_TRUE; /*For Encoders Only*/
    

    /* Output port params, using OMX_AUDIO_PARAM_GSMHRTYPE for compatibility*/
    ilbc_op->nSize = sizeof (OMX_AUDIO_PARAM_GSMHRTYPE);
    ilbc_op->nPortIndex = ILBCENC_OUTPUT_PORT;
    ilbc_op->bDTX = FALSE;
    ilbc_op->bHiPassFilter = FALSE;   
    
    OMX_MALLOC_GENERIC(pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT]->pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    OMX_ILBCCONF_INIT_STRUCT(pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT]->pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    /* Set output port format defaults */
    pPortFormat = pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT]->pPortFormat;
    OMX_ILBCCONF_INIT_STRUCT(pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    pPortFormat->nPortIndex         = ILBCENC_OUTPUT_PORT;
    pPortFormat->nIndex             = OMX_IndexParamAudioILBC;
    pPortFormat->eEncoding          = OMX_IndexCustomiLBCCodingType;

    ILBCENC_DPRINT("%d Setting dasf & acdn & amr and MultiFrame modes to 0\n",__LINE__);

    pComponentPrivate->bPlayCompleteFlag = 0;

    pComponentPrivate->dasfMode = 0;
    pComponentPrivate->dasfMode = 0;
    pComponentPrivate->acdnMode = 0;
    pComponentPrivate->nMultiFrameMode = 0;
    pComponentPrivate->bCompThreadStarted = 0;
    pComponentPrivate->pParams = NULL;
    pComponentPrivate->bInitParamsInitialized = 0;

    pComponentPrivate->pMarkBuf = NULL;
    pComponentPrivate->pMarkData = NULL;
    pComponentPrivate->nEmptyBufferDoneCount = 0;
    pComponentPrivate->nEmptyThisBufferCount = 0;
    pComponentPrivate->nFillBufferDoneCount = 0;
    pComponentPrivate->nFillThisBufferCount = 0;
    pComponentPrivate->strmAttr = NULL;
    pComponentPrivate->bDisableCommandParam = 0;
    pComponentPrivate->iHoldBuffer = NULL;
    pComponentPrivate->pHoldBuffer = NULL;
    pComponentPrivate->nHoldLength = 0;
    for (i=0; i < ILBCENC_MAX_NUM_OF_BUFS; i++) {
        pComponentPrivate->pInputBufHdrPending[i] = NULL;
        pComponentPrivate->pOutputBufHdrPending[i] = NULL;
    }
    pComponentPrivate->nNumInputBufPending = 0;
    pComponentPrivate->nNumOutputBufPending = 0;
    pComponentPrivate->bDisableCommandPending = 0;
    pComponentPrivate->bNoIdleOnStop= OMX_FALSE;
    pComponentPrivate->nRuntimeInputBuffers=0;
    pComponentPrivate->nRuntimeOutputBuffers=0;
    pComponentPrivate->nNumOfFramesSent=0;
    pComponentPrivate->ptrLibLCML = NULL;
    pComponentPrivate->IpBufindex = 0;
    pComponentPrivate->OpBufindex = 0;
    
    pComponentPrivate->codecType = ILBCENC_PRIMARY_CODEC;
    pComponentPrivate->inputFrameSize = ILBCENC_PRIMARY_INPUT_FRAME_SIZE;
    pComponentPrivate->inputBufferSize = ILBCENC_PRIMARY_INPUT_BUFFER_SIZE;
    pComponentPrivate->outputFrameSize = ILBCENC_PRIMARY_OUTPUT_BUFFER_SIZE;
    pComponentPrivate->outputBufferSize = ILBCENC_PRIMARY_OUTPUT_FRAME_SIZE;

    pComponentPrivate->bDspStoppedWhileExecuting = 0;
    pComponentPrivate->bBypassDSP = OMX_FALSE;
    pComponentPrivate->DSPMMUFault = OMX_FALSE;

    OMX_MALLOC_SIZE(pComponentPrivate->sDeviceString, 100*sizeof(char), OMX_STRING);

    /* Initialize device string to the default value */
    strcpy((char*)pComponentPrivate->sDeviceString,":srcul/codec\0");
    
#ifndef UNDER_CE
    pthread_mutex_init(&pComponentPrivate->AlloBuf_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->AlloBuf_threshold, NULL);
    pComponentPrivate->AlloBuf_waitingsignal = 0;

    pthread_mutex_init(&pComponentPrivate->InLoaded_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->InLoaded_threshold, NULL);
    pComponentPrivate->InLoaded_readytoidle = 0;

    pthread_mutex_init(&pComponentPrivate->InIdle_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->InIdle_threshold, NULL);
    pComponentPrivate->InIdle_goingtoloaded = 0;

    pthread_mutex_init(&pComponentPrivate->ToLoaded_mutex, NULL);
#else
    OMX_CreateEvent(&(pComponentPrivate->AlloBuf_event));
    pComponentPrivate->AlloBuf_waitingsignal = 0;

    OMX_CreateEvent(&(pComponentPrivate->InLoaded_event));
    pComponentPrivate->InLoaded_readytoidle = 0;

    OMX_CreateEvent(&(pComponentPrivate->InIdle_event));
    pComponentPrivate->InIdle_goingtoloaded = 0;
#endif
    pComponentPrivate->bIsInvalidState = OMX_FALSE;

#ifdef RESOURCE_MANAGER_ENABLED
    eError = RMProxy_NewInitalize();
    ILBCENC_DPRINT("%d OMX_ComponentInit\n", __LINE__);
    if (eError != OMX_ErrorNone) {
        ILBCENC_EPRINT("%d Error returned from loading ResourceManagerProxy thread\n",__LINE__);
        goto EXIT;
    }
#endif

    eError = ILBCENC_StartComponentThread(pHandle);
    ILBCENC_DPRINT("%d OMX_ComponentInit\n", __LINE__);
    if (eError != OMX_ErrorNone) {
        ILBCENC_EPRINT("%d Error returned from the Component\n",__LINE__);
        goto EXIT;
    }


#ifdef __PERF_INSTRUMENTATION__
    PERF_ThreadCreated(pComponentPrivate->pPERF, pComponentPrivate->ComponentThread,
                       PERF_FOURCC('N','B','E','T'));
#endif

#ifndef UNDER_CE
#ifdef DSP_RENDERING_ON
    /* start Audio Manager to get streamId */
    if((pComponentPrivate->fdwrite=open(FIFO1,O_WRONLY))<0) {
        ILBCENC_EPRINT("%d [ILBC Encoder Component] - failure to open WRITE pipe\n",__LINE__);
    }

    if((pComponentPrivate->fdread=open(FIFO2,O_RDONLY))<0) {
        ILBCENC_EPRINT("%d [ILBC Encoder Component] - failure to open READ pipe\n",__LINE__);
    }    
        
#endif
#endif
 EXIT:
    ILBCENC_DPRINT("%d Exiting OMX_ComponentInit\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  *  SetCallbacks() Sets application callbacks to the component
  *
  * This method will update application callbacks
  * to the component. So that component can make use of those call back
  * while sending buffers to the application. And also it will copy the
  * application private data to component memory
  *
  * @param pComponent    handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param pAppData      Application private data
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE SetCallbacks (OMX_HANDLETYPE pComponent,
                                   OMX_CALLBACKTYPE* pCallBacks,
                                   OMX_PTR pAppData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE*)pComponent;

    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate =
        (ILBCENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    ILBCENC_DPRINT("%d Entering SetCallbacks\n", __LINE__);
    if (pCallBacks == NULL) {
        eError = OMX_ErrorBadParameter;
        ILBCENC_DPRINT("%d Received the empty callbacks from the application\n",__LINE__);
        goto EXIT;
    }

    /*Copy the callbacks of the application to the component private*/
    memcpy (&(pComponentPrivate->cbInfo), pCallBacks, sizeof(OMX_CALLBACKTYPE));

    /*copy the application private data to component memory */
    pHandle->pApplicationPrivate = pAppData;

    pComponentPrivate->curState = OMX_StateLoaded;
 EXIT:
    ILBCENC_DPRINT("%d Exiting SetCallbacks\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  *  GetComponentVersion() This will return the component version
  *
  * This method will retrun the component version
  *
  * @param hComp               handle for this instance of the component
  * @param pCompnentName       Name of the component
  * @param pCompnentVersion    handle for this instance of the component
  * @param pSpecVersion        application callbacks
  * @param pCompnentUUID
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE GetComponentVersion (OMX_HANDLETYPE hComp,
                                          OMX_STRING pComponentName,
                                          OMX_VERSIONTYPE* pComponentVersion,
                                          OMX_VERSIONTYPE* pSpecVersion,
                                          OMX_UUIDTYPE* pComponentUUID)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE*) hComp;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *) pHandle->pComponentPrivate;
    ILBCENC_DPRINT("%d Entering GetComponentVersion\n", __LINE__);

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
#endif

    /* Copy component version structure */
    if(pComponentVersion != NULL && pComponentName != NULL) {
        strcpy(pComponentName, pComponentPrivate->cComponentName);
        memcpy(pComponentVersion, &(pComponentPrivate->ComponentVersion.s), sizeof(pComponentPrivate->ComponentVersion.s));
    }
    else {
        ILBCENC_EPRINT("%d OMX_ErrorBadParameter from GetComponentVersion",__LINE__);
        eError = OMX_ErrorBadParameter;
    }

 EXIT:
    ILBCENC_DPRINT("%d Exiting GetComponentVersion\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}
/*-------------------------------------------------------------------*/
/**
  *  SendCommand() used to send the commands to the component
  *
  * This method will be used by the application.
  *
  * @param phandle         handle for this instance of the component
  * @param Cmd             Command to be sent to the component
  * @param nParam          indicates commmad is sent using this method
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE SendCommand (OMX_HANDLETYPE phandle,
                                  OMX_COMMANDTYPE Cmd,
                                  OMX_U32 nParam,
                                  OMX_PTR pCmdData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)phandle;
    ILBCENC_COMPONENT_PRIVATE *pCompPrivate =
        (ILBCENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    int nRet = 0;
   
#ifdef _ERROR_PROPAGATION__
    if (pCompPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
#else
    ILBCENC_DPRINT("%d Entering SendCommand()\n", __LINE__);
    if(pCompPrivate->curState == OMX_StateInvalid) {
        eError = OMX_ErrorInvalidState;
        ILBCENC_EPRINT("%d Error OMX_ErrorInvalidState Sent to App\n",__LINE__);
        goto EXIT;
    }
#endif
#ifdef __PERF_INSTRUMENTATION__
    PERF_SendingCommand(pCompPrivate->pPERF,
                        Cmd,
                        (Cmd == OMX_CommandMarkBuffer) ? ((OMX_U32) pCmdData) : nParam,
                        PERF_ModuleComponent);
#endif
    switch(Cmd) {
    case OMX_CommandStateSet:
        ILBCENC_DPRINT("%d OMX_CommandStateSet SendCommand\n",__LINE__);
        if (nParam == OMX_StateLoaded) {
            pCompPrivate->bLoadedCommandPending = OMX_TRUE;
        }            
        if(pCompPrivate->curState == OMX_StateLoaded) {
            if((nParam == OMX_StateExecuting) || (nParam == OMX_StatePause)) {
                pCompPrivate->cbInfo.EventHandler ( pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventError,
                                                    OMX_ErrorIncorrectStateTransition,
                                                    0,
                                                    NULL);
                goto EXIT;
            }

            if(nParam == OMX_StateInvalid) {
                ILBCENC_DPRINT("%d OMX_CommandStateSet SendCommand\n",__LINE__);
                pCompPrivate->curState = OMX_StateInvalid;
                pCompPrivate->cbInfo.EventHandler ( pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventError,
                                                    OMX_ErrorInvalidState,
                                                    0,
                                                    NULL);
                goto EXIT;
            }
        }
        break;
    case OMX_CommandFlush:
        ILBCENC_DPRINT("%d OMX_CommandFlush SendCommand\n",__LINE__);
        if(nParam > 1 && nParam != -1) {
            eError = OMX_ErrorBadPortIndex;
            ILBCENC_EPRINT("%d OMX_ErrorBadPortIndex from SendCommand",__LINE__);
            goto EXIT;
        }
        break;
    case OMX_CommandPortDisable:
        ILBCENC_DPRINT("%d OMX_CommandPortDisable SendCommand\n",__LINE__);
        break;
    case OMX_CommandPortEnable:
        ILBCENC_DPRINT("%d OMX_CommandPortEnable SendCommand\n",__LINE__);
        break;
    case OMX_CommandMarkBuffer:
        ILBCENC_DPRINT("%d OMX_CommandMarkBuffer SendCommand\n",__LINE__);
        if (nParam > 0) {
            eError = OMX_ErrorBadPortIndex;
            ILBCENC_EPRINT("%d OMX_ErrorBadPortIndex from SendCommand",__LINE__);
            goto EXIT;
        }
        break;
    default:
        ILBCENC_EPRINT("%d Command Received Default eError\n",__LINE__);
        pCompPrivate->cbInfo.EventHandler ( pHandle,
                                            pHandle->pApplicationPrivate,
                                            OMX_EventError,
                                            OMX_ErrorUndefined,
                                            0,
                                            "Invalid Command");
        break;

    }

    nRet = write (pCompPrivate->cmdPipe[1], &Cmd, sizeof(Cmd));
    if (nRet == -1) {
        eError = OMX_ErrorInsufficientResources;
        ILBCENC_EPRINT("%d OMX_ErrorInsufficientResources from SendCommand",__LINE__);
        goto EXIT;
    }

    if (Cmd == OMX_CommandMarkBuffer) {
        nRet = write(pCompPrivate->cmdDataPipe[1], &pCmdData,
                     sizeof(OMX_PTR));
    } else {
        nRet = write(pCompPrivate->cmdDataPipe[1], &nParam,
                     sizeof(OMX_U32));
    }
    if (nRet == -1) {
        ILBCENC_EPRINT("%d OMX_ErrorInsufficientResources from SendCommand",__LINE__);
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

 EXIT:
    ILBCENC_DPRINT("%d Exiting SendCommand()\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}
/*-------------------------------------------------------------------*/
/**
  *  GetParameter() Gets the current configurations of the component
  *
  * @param hComp         handle for this instance of the component
  * @param nParamIndex
  * @param ComponentParameterStructure
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE GetParameter (OMX_HANDLETYPE hComp,
                                   OMX_INDEXTYPE nParamIndex,
                                   OMX_PTR ComponentParameterStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ILBCENC_COMPONENT_PRIVATE  *pComponentPrivate = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pParameterStructure = NULL;

    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
    pParameterStructure = (OMX_PARAM_PORTDEFINITIONTYPE*)ComponentParameterStructure;

    ILBCENC_DPRINT("%d Entering the GetParameter\n",__LINE__);
    if (pParameterStructure == NULL) {
        eError = OMX_ErrorBadParameter;
        ILBCENC_EPRINT("%d OMX_ErrorBadPortIndex from GetParameter",__LINE__);
        goto EXIT;
    }

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
#else
    if(pComponentPrivate->curState == OMX_StateInvalid) {
        eError = OMX_ErrorIncorrectStateOperation;
        ILBCENC_EPRINT("%d OMX_ErrorIncorrectStateOperation from GetParameter",__LINE__);
        goto EXIT;
    }
#endif
    switch(nParamIndex){
    case OMX_IndexParamAudioInit:
	if (pComponentPrivate->sPortParam == NULL) {
            eError = OMX_ErrorBadParameter;
	    break;
	}
        ILBCENC_DPRINT("%d GetParameter OMX_IndexParamAudioInit \n",__LINE__);
        memcpy(ComponentParameterStructure, pComponentPrivate->sPortParam, sizeof(OMX_PORT_PARAM_TYPE));
        break;

    case OMX_IndexParamPortDefinition:

        ILBCENC_DPRINT("%d GetParameter OMX_IndexParamPortDefinition \n",__LINE__);
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(ComponentParameterStructure))->nPortIndex ==
           pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->nPortIndex) {
            memcpy(ComponentParameterStructure, pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT], sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        }
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(ComponentParameterStructure))->nPortIndex ==
                pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->nPortIndex) {
            memcpy(ComponentParameterStructure, pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT], sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        }
        else {
            ILBCENC_EPRINT("%d OMX_ErrorBadPortIndex from GetParameter \n",__LINE__);
            eError = OMX_ErrorBadPortIndex;
        }
        break;

    case OMX_IndexParamAudioPortFormat:

        ILBCENC_DPRINT("%d GetParameter OMX_IndexParamAudioPortFormat \n",__LINE__);
        if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex ==
           pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->nPortIndex) {
            if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nIndex >
               pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT]->pPortFormat->nPortIndex) {
                eError = OMX_ErrorNoMore;
            }
            else {
                memcpy(ComponentParameterStructure, 
                        pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT]->pPortFormat, 
                        sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            }
        }
        else if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex ==
                pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->nPortIndex){
            if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nIndex >
               pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT]->pPortFormat->nPortIndex) {
                eError = OMX_ErrorNoMore;
            }
            else {
                memcpy(ComponentParameterStructure, 
                        pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT]->pPortFormat, 
                        sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            }
        }
        else {
            ILBCENC_EPRINT("%d OMX_ErrorBadPortIndex from GetParameter \n",__LINE__);
            eError = OMX_ErrorBadPortIndex;
        }
        break;

    case (OMX_INDEXTYPE)OMX_IndexParamAudioILBC:
        if(((OMX_AUDIO_PARAM_GSMEFRTYPE *)(ComponentParameterStructure))->nPortIndex ==
                pComponentPrivate->ilbcParams->nPortIndex){
            memcpy(ComponentParameterStructure, 
                    pComponentPrivate->ilbcParams, 
                    sizeof(OMX_AUDIO_PARAM_GSMEFRTYPE));
        }else {
            ILBCENC_EPRINT("%d OMX_ErrorBadPortIndex from GetParameter \n",__LINE__);
            eError = OMX_ErrorBadPortIndex;
        }
        break;

    case OMX_IndexParamPriorityMgmt:
	if (pComponentPrivate->sPriorityMgmt == NULL) {
            eError = OMX_ErrorBadParameter;
	    break;
	}
        ILBCENC_DPRINT("%d GetParameter OMX_IndexParamPriorityMgmt \n",__LINE__);
        memcpy(ComponentParameterStructure, pComponentPrivate->sPriorityMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
        break;

    case OMX_IndexParamAudioPcm:
	if (pComponentPrivate->pcmParams == NULL) {
            eError = OMX_ErrorBadParameter;
	    break;
	}
        memcpy(ComponentParameterStructure, pComponentPrivate->pcmParams, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        break;

    case OMX_IndexParamCompBufferSupplier:
        if(((OMX_PARAM_BUFFERSUPPLIERTYPE *)(ComponentParameterStructure))->nPortIndex == OMX_DirInput) {
            ILBCENC_DPRINT(":: GetParameter OMX_IndexParamCompBufferSupplier \n");
            /*  memcpy(ComponentParameterStructure, pBufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE)); */                  
        }
        else if(((OMX_PARAM_BUFFERSUPPLIERTYPE *)(ComponentParameterStructure))->nPortIndex == OMX_DirOutput) {
            ILBCENC_DPRINT(":: GetParameter OMX_IndexParamCompBufferSupplier \n");
            /*memcpy(ComponentParameterStructure, pBufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE)); */
        } 
        else {
            ILBCENC_DPRINT(":: OMX_ErrorBadPortIndex from GetParameter");
            eError = OMX_ErrorBadPortIndex;
        }
        break;                

    case OMX_IndexParamVideoInit:
        break;

    case OMX_IndexParamImageInit:
        break;

    case OMX_IndexParamOtherInit:
        break;

    default:
        ILBCENC_EPRINT("%d OMX_ErrorUnsupportedIndex GetParameter \n",__LINE__);
        eError = OMX_ErrorUnsupportedIndex;
        break;
    }
 EXIT:
    ILBCENC_DPRINT("%d Exiting GetParameter\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}
/*-------------------------------------------------------------------*/
/**
  *  SetParameter() Sets configuration paramets to the component
  *
  * @param hComp         handle for this instance of the component
  * @param nParamIndex
  * @param pCompParam
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE SetParameter (OMX_HANDLETYPE hComp,
                                   OMX_INDEXTYPE nParamIndex,
                                   OMX_PTR pCompParam)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle= (OMX_COMPONENTTYPE*)hComp;
    ILBCENC_COMPONENT_PRIVATE  *pComponentPrivate = NULL;
    OMX_AUDIO_PARAM_PORTFORMATTYPE* pComponentParam = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pComponentParamPort = NULL;
    OMX_AUDIO_PARAM_GSMEFRTYPE *pCompiLBCParam = NULL;
    OMX_PARAM_COMPONENTROLETYPE  *pRole = NULL;
    OMX_AUDIO_PARAM_PCMMODETYPE *ilbc_ip = NULL;
    OMX_PARAM_BUFFERSUPPLIERTYPE sBufferSupplier;

    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);


    ILBCENC_DPRINT("%d Entering the SetParameter\n",__LINE__);
    if (pCompParam == NULL) {
        eError = OMX_ErrorBadParameter;
        ILBCENC_EPRINT("%d OMX_ErrorBadParameter from SetParameter",__LINE__);
        goto EXIT;
    }
    if (pComponentPrivate->curState != OMX_StateLoaded) {
        eError = OMX_ErrorIncorrectStateOperation;
        ILBCENC_EPRINT("%d OMX_ErrorIncorrectStateOperation from SetParameter",__LINE__);
        goto EXIT;
    }
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
#endif
    switch(nParamIndex) {
    case OMX_IndexParamAudioPortFormat:
        ILBCENC_DPRINT("%d SetParameter OMX_IndexParamAudioPortFormat \n",__LINE__);
        pComponentParam = (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pCompParam;
        if ( pComponentParam->nPortIndex == pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT]->pPortFormat->nPortIndex ) {
            memcpy(pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT]->pPortFormat, 
                    pComponentParam, 
                    sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
        } else if ( pComponentParam->nPortIndex == pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT]->pPortFormat->nPortIndex ) {
            memcpy(pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT]->pPortFormat, 
                    pComponentParam, 
                    sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
        } else {
            ILBCENC_EPRINT("%d OMX_ErrorBadPortIndex from SetParameter",__LINE__);
            eError = OMX_ErrorBadPortIndex;
        }
        break;
            
    case (OMX_INDEXTYPE)OMX_IndexParamAudioILBC:
        ILBCENC_DPRINT("%d SetParameter OMX_IndexParamAudioILBC \n",__LINE__);
        pCompiLBCParam = (OMX_AUDIO_PARAM_GSMEFRTYPE *)pCompParam;
	if (((ILBCENC_COMPONENT_PRIVATE *)
	    pHandle->pComponentPrivate)->ilbcParams == NULL) {
            eError = OMX_ErrorBadParameter;
	    break;
	}
        if (pCompiLBCParam->nPortIndex == 1) { /* 1 means Output port */
            memcpy(((ILBCENC_COMPONENT_PRIVATE *)
                    pHandle->pComponentPrivate)->ilbcParams, 
                    pCompiLBCParam, 
                    sizeof(OMX_AUDIO_PARAM_GSMEFRTYPE));
        }
        else {
            ILBCENC_EPRINT("%d OMX_ErrorBadPortIndex from SetParameter",__LINE__);
            eError = OMX_ErrorBadPortIndex;
        }
        break;
            
    case OMX_IndexParamPortDefinition:
        pComponentParamPort = (OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam;
        ILBCENC_DPRINT("%d SetParameter OMX_IndexParamPortDefinition \n",__LINE__);
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
           pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->nPortIndex) {
            ILBCENC_DPRINT("%d SetParameter OMX_IndexParamPortDefinition \n",__LINE__);
            memcpy(pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT], pCompParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        }
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
                pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->nPortIndex) {
            ILBCENC_DPRINT("%d SetParameter OMX_IndexParamPortDefinition \n",__LINE__);
            memcpy(pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT], pCompParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        }
        else {
            ILBCENC_EPRINT("%d OMX_ErrorBadPortIndex from SetParameter",__LINE__);
            eError = OMX_ErrorBadPortIndex;
        }
        break;
            
    case OMX_IndexParamPriorityMgmt:
	if (pComponentPrivate->sPriorityMgmt == NULL) {
            eError = OMX_ErrorBadParameter;
	    break;
	}
        ILBCENC_DPRINT("%d SetParameter OMX_IndexParamPriorityMgmt \n",__LINE__);
        memcpy(pComponentPrivate->sPriorityMgmt, (OMX_PRIORITYMGMTTYPE*)pCompParam, sizeof(OMX_PRIORITYMGMTTYPE));
        break;

    case OMX_IndexParamAudioInit:
	if (pComponentPrivate->sPortParam == NULL) {
            eError = OMX_ErrorBadParameter;
	    break;
	}
        ILBCENC_DPRINT("%d SetParameter OMX_IndexParamAudioInit \n",__LINE__);
        memcpy(pComponentPrivate->sPortParam, (OMX_PORT_PARAM_TYPE*)pCompParam, sizeof(OMX_PORT_PARAM_TYPE));
        break;
            
    case OMX_IndexParamStandardComponentRole:
        if (pCompParam) {
            pRole = (OMX_PARAM_COMPONENTROLETYPE *)pCompParam;
            memcpy(&(pComponentPrivate->componentRole), (void *)pRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        } else {
            eError = OMX_ErrorBadParameter;
        }
        break;

    case OMX_IndexParamAudioPcm:
        if(pCompParam){
	    if (pComponentPrivate->pcmParams == NULL) {
                eError = OMX_ErrorBadParameter;
	        break;
	    }
            ilbc_ip = (OMX_AUDIO_PARAM_PCMMODETYPE *)pCompParam;
            memcpy(pComponentPrivate->pcmParams, ilbc_ip, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }
        else{
            eError = OMX_ErrorBadParameter;
        }
        break;

    case OMX_IndexParamCompBufferSupplier:             
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
           pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->nPortIndex) {
            ILBCENC_DPRINT(":: SetParameter OMX_IndexParamCompBufferSupplier \n");
            sBufferSupplier.eBufferSupplier = OMX_BufferSupplyInput;
            memcpy(&sBufferSupplier, pCompParam, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));                                
                    
        }
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
                pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->nPortIndex) {
            ILBCENC_DPRINT(":: SetParameter OMX_IndexParamCompBufferSupplier \n");
            sBufferSupplier.eBufferSupplier = OMX_BufferSupplyOutput;
            memcpy(&sBufferSupplier, pCompParam, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
        } 
        else {
            ILBCENC_DPRINT(":: OMX_ErrorBadPortIndex from SetParameter");
            eError = OMX_ErrorBadPortIndex;
        }
        break;
            
    default:
        ILBCENC_EPRINT("%d SetParameter OMX_ErrorUnsupportedIndex \n",__LINE__);
        eError = OMX_ErrorUnsupportedIndex;
        break;
    }
 EXIT:
    ILBCENC_DPRINT("%d Exiting SetParameter\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  *  GetConfig() Gets the current configuration of to the component
  *
  * @param hComp         handle for this instance of the component
  * @param nConfigIndex
  * @param ComponentConfigStructure
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE GetConfig (OMX_HANDLETYPE hComp,
                                OMX_INDEXTYPE nConfigIndex,
                                OMX_PTR ComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = (OMX_COMPONENTTYPE*)hComp;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate =
        (ILBCENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    TI_OMX_STREAM_INFO *streamInfo = NULL;
    OMX_MALLOC_GENERIC(streamInfo, TI_OMX_STREAM_INFO);
    if(streamInfo == NULL){
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        OMX_MEMFREE_STRUCT(streamInfo);
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
#endif
    if(nConfigIndex == OMX_IndexCustomILBCENCStreamIDConfig){
        streamInfo->streamId = pComponentPrivate->streamID;
        memcpy(ComponentConfigStructure,streamInfo,sizeof(TI_OMX_STREAM_INFO));
    }
    OMX_MEMFREE_STRUCT(streamInfo);
 EXIT:
    ILBCENC_DPRINT("%d Exiting GetConfig. Returning = 0x%x\n",__LINE__,eError);
    return eError;
}
/*-------------------------------------------------------------------*/
/**
  *  SetConfig() Sets the configraiton to the component
  *
  * @param hComp         handle for this instance of the component
  * @param nConfigIndex
  * @param ComponentConfigStructure
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE SetConfig (OMX_HANDLETYPE hComp,
                                OMX_INDEXTYPE nConfigIndex,
                                OMX_PTR ComponentConfigStructure)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE* pHandle = (OMX_COMPONENTTYPE*)hComp;
    TI_OMX_DSP_DEFINITION *pTiDspDefinition = NULL;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_S16 *customFlag = NULL;      
    
    TI_OMX_DATAPATH dataPath;                   
    OMX_AUDIO_CONFIG_VOLUMETYPE *pGainStructure = NULL;
#ifdef DSP_RENDERING_ON
    AM_COMMANDDATATYPE cmd_data;
#endif    
    ILBCENC_DPRINT("%d Entering SetConfig\n", __LINE__);
    if (pHandle == NULL) {
        ILBCENC_DPRINT ("%d Invalid HANDLE OMX_ErrorBadParameter \n",__LINE__);
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
#endif
    switch (nConfigIndex) {

    case (OMX_INDEXTYPE)OMX_IndexCustomILBCENCModeConfig:
        pTiDspDefinition = (TI_OMX_DSP_DEFINITION*)ComponentConfigStructure;
        if (pTiDspDefinition == NULL) {
            eError = OMX_ErrorBadParameter;
            ILBCENC_EPRINT("%d OMX_ErrorBadParameter from SetConfig\n",__LINE__);
            goto EXIT;
        }
        if(pTiDspDefinition->acousticMode == 1)
            pComponentPrivate->acdnMode = pTiDspDefinition->acousticMode;
        else
            pComponentPrivate->acdnMode = 0;
        
        if(pTiDspDefinition->dasfMode == 1)
            pComponentPrivate->dasfMode = pTiDspDefinition->dasfMode;
        else
            pComponentPrivate->dasfMode = 0;

        pComponentPrivate->streamID = pTiDspDefinition->streamId;

        if (pComponentPrivate->ilbcParams->bDTX == OMX_TRUE) { 
            pComponentPrivate->codecType = ILBCENC_SECONDARY_CODEC;
            pComponentPrivate->inputBufferSize = ILBCENC_SECONDARY_INPUT_BUFFER_SIZE;
            pComponentPrivate->inputFrameSize = ILBCENC_SECONDARY_INPUT_FRAME_SIZE;
            pComponentPrivate->outputBufferSize = ILBCENC_SECONDARY_OUTPUT_BUFFER_SIZE;
            pComponentPrivate->outputFrameSize = ILBCENC_SECONDARY_OUTPUT_FRAME_SIZE;                   
        }

        break;
    case  (OMX_INDEXTYPE)OMX_IndexCustomILBCENCDataPath:
        customFlag = (OMX_S16*)ComponentConfigStructure;
        dataPath = *customFlag;            
        switch(dataPath) {
        case DATAPATH_APPLICATION:
            OMX_MMMIXER_DATAPATH(pComponentPrivate->sDeviceString, RENDERTYPE_ENCODER, pComponentPrivate->streamID);
            break;

        case DATAPATH_APPLICATION_RTMIXER:
            strcpy((char*)pComponentPrivate->sDeviceString,(char*)RTM_STRING_ENCODER);
            break;
                    
        case DATAPATH_ACDN:
            strcpy((char*)pComponentPrivate->sDeviceString,(char*)ACDN_STRING_ENCODER);
            break;
        default:
            break;
                    
        }
        break;
    case OMX_IndexConfigAudioVolume:
#ifdef DSP_RENDERING_ON
        pGainStructure = (OMX_AUDIO_CONFIG_VOLUMETYPE *)ComponentConfigStructure;
        cmd_data.hComponent = hComp;
        cmd_data.AM_Cmd = AM_CommandSWGain;
        cmd_data.param1 = pGainStructure->sVolume.nValue;
        cmd_data.param2 = 0;
        cmd_data.streamID = pComponentPrivate->streamID;

        if((write(pComponentPrivate->fdwrite, &cmd_data, sizeof(cmd_data)))<0)
            {   
                ILBCENC_DPRINT("[ILBCENC encoder] - fail to send command to audio manager\n");
            }
        else
            {
                ILBCENC_DPRINT("[ILBCENC encoder] - ok to send command to audio manager\n");
            }
#endif
        break;
    default:
        eError = OMX_ErrorUnsupportedIndex;
        break;
    }
 EXIT:
    ILBCENC_DPRINT("%d Exiting SetConfig\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  *  GetState() Gets the current state of the component
  *
  * @param pCompomponent handle for this instance of the component
  * @param pState
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE GetState (OMX_HANDLETYPE pComponent, OMX_STATETYPE* pState)
{
    OMX_ERRORTYPE eError = OMX_ErrorUndefined;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    ILBCENC_DPRINT("%d Entering GetState\n", __LINE__);
    if (!pState) {
        eError = OMX_ErrorBadParameter;
        ILBCENC_EPRINT("%d OMX_ErrorBadParameter from GetState\n",__LINE__);
        goto EXIT;
    }

    if (pHandle && pHandle->pComponentPrivate) {
        *pState =  ((ILBCENC_COMPONENT_PRIVATE*)
                    pHandle->pComponentPrivate)->curState;
    } else {
        *pState = OMX_StateLoaded;
    }
    eError = OMX_ErrorNone;

 EXIT:
    ILBCENC_DPRINT("%d Exiting GetState\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  *  EmptyThisBuffer() This callback is used to send the input buffer to
  *  component
  *
  * @param pComponent       handle for this instance of the component
  * @param nPortIndex       input port index
  * @param pBuffer          buffer to be sent to codec
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE EmptyThisBuffer (OMX_HANDLETYPE pComponent,
                                      OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int ret = 0;

    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate =
        (ILBCENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    pPortDef = ((ILBCENC_COMPONENT_PRIVATE*)pComponentPrivate)->pPortDef[ILBCENC_INPUT_PORT];

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
#endif
#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedFrame(pComponentPrivate->pPERF,
                       pBuffer->pBuffer,
                       pBuffer->nFilledLen,
                       PERF_ModuleHLMM);
#endif

    ILBCENC_DPRINT("%d Entering EmptyThisBuffer\n", __LINE__);
    if (pBuffer == NULL) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorBadParameter,"OMX_ErrorBadParameter");
    }

    if (pBuffer->nSize != sizeof(OMX_BUFFERHEADERTYPE)) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorBadParameter,"OMX_ErrorBadParameter");
    }

    if (!pPortDef->bEnabled) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorIncorrectStateOperation,"OMX_ErrorIncorrectStateOperation");
    }

    if (pBuffer->nVersion.nVersion != pComponentPrivate->nVersion) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorVersionMismatch,"OMX_ErrorVersionMismatch");
    }

    if (pBuffer->nInputPortIndex != ILBCENC_INPUT_PORT) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorBadPortIndex,"OMX_ErrorBadPortIndex");
    }

    if (pComponentPrivate->curState != OMX_StateExecuting && 
        pComponentPrivate->curState != OMX_StatePause) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorIncorrectStateOperation,"OMX_ErrorIncorrectStateOperation");
    }


    ILBCENC_DPRINT("----------------------------------------------------------------\n");
    ILBCENC_DPRINT("%d Comp Sending Filled ip buff = %p to CompThread\n",__LINE__,pBuffer);
    ILBCENC_DPRINT("----------------------------------------------------------------\n");

    pComponentPrivate->app_nBuf--;  
    pComponentPrivate->pMarkData = pBuffer->pMarkData;
    pComponentPrivate->hMarkTargetComponent = pBuffer->hMarkTargetComponent;

    ret = write (pComponentPrivate->dataPipe[1], &pBuffer, sizeof(OMX_BUFFERHEADERTYPE*));
    if (ret == -1) {
        ILBCENC_EPRINT("%d Error in Writing to the Data pipe\n", __LINE__);
        eError = OMX_ErrorHardware;
        goto EXIT;
    }
 EXIT:
    ILBCENC_DPRINT("%d Exiting EmptyThisBuffer\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}
/*-------------------------------------------------------------------*/
/**
  *  FillThisBuffer() This callback is used to send the output buffer to
  *  the component
  *
  * @param pComponent    handle for this instance of the component
  * @param nPortIndex    output port number
  * @param pBuffer       buffer to be sent to codec
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE FillThisBuffer (OMX_HANDLETYPE pComponent,
                                     OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int ret = 0;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate =
        (ILBCENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    pPortDef = ((ILBCENC_COMPONENT_PRIVATE*)pComponentPrivate)->pPortDef[ILBCENC_OUTPUT_PORT];

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
#endif
#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedFrame(pComponentPrivate->pPERF,
                       pBuffer->pBuffer,
                       0,
                       PERF_ModuleHLMM);
#endif
    ILBCENC_DPRINT("%d Entering FillThisBuffer\n", __LINE__);
    ILBCENC_DPRINT("------------------------------------------------------------------\n");
    ILBCENC_DPRINT("%d Comp Sending Emptied op buff = %p to CompThread\n",__LINE__,pBuffer);
    ILBCENC_DPRINT("------------------------------------------------------------------\n");
    if (pBuffer == NULL) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorBadParameter,"OMX_ErrorBadParameter");
    }

    if (pBuffer->nSize != sizeof(OMX_BUFFERHEADERTYPE)) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorBadParameter,"OMX_ErrorBadParameter");
    }

    if (!pPortDef->bEnabled) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorIncorrectStateOperation,"OMX_ErrorIncorrectStateOperation");
    }

    if (pBuffer->nVersion.nVersion != pComponentPrivate->nVersion) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorVersionMismatch,"OMX_ErrorVersionMismatch");
    }

    if (pBuffer->nOutputPortIndex != ILBCENC_OUTPUT_PORT) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorBadPortIndex,"OMX_ErrorBadPortIndex");
    }

    if(pComponentPrivate->curState != OMX_StateExecuting && 
       pComponentPrivate->curState != OMX_StatePause) {
        ILBCENC_OMX_ERROR_EXIT(eError,OMX_ErrorIncorrectStateOperation,"OMX_ErrorIncorrectStateOperation");
    }

    pComponentPrivate->app_nBuf--;
    if(pComponentPrivate->pMarkBuf != NULL){
        pBuffer->hMarkTargetComponent = pComponentPrivate->pMarkBuf->hMarkTargetComponent;
        pBuffer->pMarkData = pComponentPrivate->pMarkBuf->pMarkData;
        pComponentPrivate->pMarkBuf = NULL;
    }

    if (pComponentPrivate->pMarkData != NULL) {
        pBuffer->hMarkTargetComponent = pComponentPrivate->hMarkTargetComponent;
        pBuffer->pMarkData = pComponentPrivate->pMarkData;
        pComponentPrivate->pMarkData = NULL;
    }

    ret = write (pComponentPrivate->dataPipe[1], &pBuffer, sizeof (OMX_BUFFERHEADERTYPE*));
    if (ret == -1) {
        ILBCENC_EPRINT("%d Error in Writing to the Data pipe\n", __LINE__);
        eError = OMX_ErrorHardware;
        goto EXIT;
    }
    pComponentPrivate->nFillThisBufferCount++;
 EXIT:
    ILBCENC_DPRINT("%d Exiting FillThisBuffer\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}
/*-------------------------------------------------------------------*/
/**
  * OMX_ComponentDeinit() this methold will de init the component
  *
  * @param pComp         handle for this instance of the component
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE ComponentDeInit(OMX_HANDLETYPE pHandle)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMX_COMPONENTTYPE *pComponent = (OMX_COMPONENTTYPE *)pHandle;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate =
        (ILBCENC_COMPONENT_PRIVATE *)pComponent->pComponentPrivate;
    ILBCENC_DPRINT("%d Entering ComponentDeInit\n", __LINE__);

#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(pComponentPrivate->pPERF,
                  PERF_BoundaryStart | PERF_BoundaryCleanup);
#endif

#ifdef DSP_RENDERING_ON
    close(pComponentPrivate->fdwrite);
    close(pComponentPrivate->fdread);
#endif
#ifdef RESOURCE_MANAGER_ENABLED
    eError = RMProxy_NewSendCommand(pHandle, RMProxy_FreeResource, 
                                    OMX_ILBC_Encoder_COMPONENT, 0,
                                    3456, NULL);
    if (eError != OMX_ErrorNone) {
        ILBCENC_EPRINT ("%dError returned from destroy ResourceManagerProxy thread\n",
                        __LINE__);
    }
    eError = RMProxy_Deinitalize();
    if (eError != OMX_ErrorNone) {
        ILBCENC_EPRINT("%d Error from RMProxy_Deinitalize\n",__LINE__);
        goto EXIT;
    }
#endif

    pComponentPrivate->bIsThreadstop = 1;
    eError = ILBCENC_StopComponentThread(pHandle);
    if (eError != OMX_ErrorNone) {
        ILBCENC_EPRINT("%d Error from ILBCENC_StopComponentThread\n",__LINE__);
        goto EXIT;
    }
    /* Wait for thread to exit so we can get the status into "eError" */
    /* close the pipe handles */
    eError = ILBCENC_FreeCompResources(pHandle);
    if (eError != OMX_ErrorNone) {
        ILBCENC_DPRINT("%d Error from ILBCENC_FreeCompResources\n",__LINE__);
        goto EXIT;
    }

#ifndef UNDER_CE
    pthread_mutex_destroy(&pComponentPrivate->InLoaded_mutex);
    pthread_cond_destroy(&pComponentPrivate->InLoaded_threshold);

    pthread_mutex_destroy(&pComponentPrivate->InIdle_mutex);
    pthread_cond_destroy(&pComponentPrivate->InIdle_threshold);

    pthread_mutex_destroy(&pComponentPrivate->AlloBuf_mutex);
    pthread_cond_destroy(&pComponentPrivate->AlloBuf_threshold);
#else
    OMX_DestroyEvent(&(pComponentPrivate->InLoaded_event));
    OMX_DestroyEvent(&(pComponentPrivate->InIdle_event));
    OMX_DestroyEvent(&(pComponentPrivate->AlloBuf_event));
#endif

#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(pComponentPrivate->pPERF,
                  PERF_BoundaryComplete | PERF_BoundaryCleanup);
    PERF_Done(pComponentPrivate->pPERF);
#endif

    OMX_MEMFREE_STRUCT(pComponentPrivate->sDeviceString);

    OMX_MEMFREE_STRUCT(pComponentPrivate);

 EXIT:
    ILBCENC_DPRINT("%d Exiting ComponentDeInit\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  *  ComponentTunnelRequest() this method is not implemented in 1.5
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
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE ComponentTunnelRequest (OMX_HANDLETYPE hComp,
                                             OMX_U32 nPort, OMX_HANDLETYPE hTunneledComp,
                                             OMX_U32 nTunneledPort,
                                             OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    eError = OMX_ErrorNotImplemented;
    ILBCENC_DPRINT("%d Entering ComponentTunnelRequest\n", __LINE__);
    ILBCENC_DPRINT("%d Exiting ComponentTunnelRequest\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  *  AllocateBuffer()

  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE AllocateBuffer (OMX_IN OMX_HANDLETYPE hComponent,
                    OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
                    OMX_IN OMX_U32 nPortIndex,
                    OMX_IN OMX_PTR pAppPrivate,
                    OMX_IN OMX_U32 nSizeBytes)

{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHeader = NULL;

    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    pPortDef = ((ILBCENC_COMPONENT_PRIVATE*)pComponentPrivate)->pPortDef[nPortIndex];
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
#endif
    ILBCENC_DPRINT("%d Entering AllocateBuffer\n", __LINE__);
    ILBCENC_DPRINT("%d pPortDef = %p\n", __LINE__,pPortDef);
    ILBCENC_DPRINT("%d pPortDef->bEnabled = %d\n", __LINE__,pPortDef->bEnabled);

    if(!pPortDef->bEnabled) {
        pComponentPrivate->AlloBuf_waitingsignal = 1;
#ifndef UNDER_CE
        pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex);
        pthread_cond_wait(&pComponentPrivate->AlloBuf_threshold, 
                          &pComponentPrivate->AlloBuf_mutex);
        pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);
#else
        OMX_WaitForEvent(&(pComponentPrivate->AlloBuf_event));
#endif
    }

    OMX_MALLOC_GENERIC(pBufferHeader, OMX_BUFFERHEADERTYPE);
    
    OMX_MALLOC_SIZE_DSPALIGN(pBufferHeader->pBuffer, nSizeBytes,OMX_U8);
    if(NULL == pBufferHeader->pBuffer){
        OMX_MEMFREE_STRUCT(pBufferHeader);
        return OMX_ErrorInsufficientResources;
    }

    if (nPortIndex == ILBCENC_INPUT_PORT) {        
        pBufferHeader->nInputPortIndex = nPortIndex;
        pBufferHeader->nOutputPortIndex = -1;
        pComponentPrivate->pInputBufferList->pBufHdr[pComponentPrivate->pInputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pInputBufferList->bBufferPending[pComponentPrivate->pInputBufferList->numBuffers] = 0;
        pComponentPrivate->pInputBufferList->bufferOwner[pComponentPrivate->pInputBufferList->numBuffers++] = 1;
        if (pComponentPrivate->pInputBufferList->numBuffers == pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
            ILBCENC_DPRINT("%d pPortDef->bPopulated = %d\n", __LINE__, pPortDef->bPopulated);
        }
    }
    else if (nPortIndex == ILBCENC_OUTPUT_PORT) {
        pBufferHeader->nInputPortIndex = -1;
        pBufferHeader->nOutputPortIndex = nPortIndex;
        pComponentPrivate->pOutputBufferList->pBufHdr[pComponentPrivate->pOutputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pOutputBufferList->bBufferPending[pComponentPrivate->pOutputBufferList->numBuffers] = 0;
        pComponentPrivate->pOutputBufferList->bufferOwner[pComponentPrivate->pOutputBufferList->numBuffers++] = 1;
        OMX_MALLOC_GENERIC(pBufferHeader->pOutputPortPrivate, ILBCENC_BUFDATA);
        if (pComponentPrivate->pOutputBufferList->numBuffers == pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
            ILBCENC_DPRINT("%d pPortDef->bPopulated = %d\n", __LINE__, pPortDef->bPopulated);
        }
    }
    else {
        eError = OMX_ErrorBadPortIndex;
        ILBCENC_EPRINT(" %d About to return OMX_ErrorBadPortIndex\n",__LINE__);
        goto EXIT;
    }
    if (((!pComponentPrivate->dasfMode) &&
         ((pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated == 
            pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bEnabled)&&
          (pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bPopulated == 
            pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bEnabled) &&
          (pComponentPrivate->InLoaded_readytoidle)))/*File Mode*/  
        || 
        ((pComponentPrivate->dasfMode) &&
         ((pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated == 
            pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bEnabled)&&
          (pComponentPrivate->InLoaded_readytoidle))))/*Dasf Mode*/ {
        pComponentPrivate->InLoaded_readytoidle = 0;
#ifndef UNDER_CE
        pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex);
        pthread_cond_signal(&pComponentPrivate->InLoaded_threshold);
        pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
#else
        OMX_SignalEvent(&(pComponentPrivate->InLoaded_event));
#endif
    }
    pBufferHeader->pAppPrivate = pAppPrivate;
    pBufferHeader->pPlatformPrivate = pComponentPrivate;
    pBufferHeader->nAllocLen = nSizeBytes;
    pBufferHeader->nVersion.s.nVersionMajor = ILBCENC_MAJOR_VER;
    pBufferHeader->nVersion.s.nVersionMinor = ILBCENC_MINOR_VER;
    pComponentPrivate->nVersion = pBufferHeader->nVersion.nVersion;
    pBufferHeader->nSize = sizeof(OMX_BUFFERHEADERTYPE);
    *pBuffer = pBufferHeader;

    if (pComponentPrivate->bEnableCommandPending && pPortDef->bPopulated){
        SendCommand (pComponentPrivate->pHandle,
                    OMX_CommandPortEnable,
                    pComponentPrivate->bEnableCommandParam,
                    NULL);
    }

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedBuffer(pComponentPrivate->pPERF,
                        (*pBuffer)->pBuffer, nSizeBytes,
                        PERF_ModuleMemory);
#endif

 EXIT:
    if(eError != OMX_ErrorNone){
        if(NULL != pBufferHeader){
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferHeader->pBuffer,OMX_U8);
            OMX_MEMFREE_STRUCT(pBufferHeader);
        }
    }

    ILBCENC_DPRINT("%d Exiting AllocateBuffer\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  *  FreeBuffer()

  * @param hComponent   handle for this instance of the component
  * @param pCallBacks   application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/

static OMX_ERRORTYPE FreeBuffer(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_U32 nPortIndex,
            OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ILBCENC_COMPONENT_PRIVATE * pComponentPrivate = NULL;
    OMX_BUFFERHEADERTYPE* buffHdr = NULL;
    OMX_U32 i;
    int bufferIndex = -1;
    ILBCENC_BUFFERLIST *pBufferList = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
    OMX_COMPONENTTYPE *pHandle = NULL;

    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)
                                (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    ILBCENC_DPRINT("%d Entering FreeBuffer\n", __LINE__);

    if (nPortIndex != OMX_DirInput && nPortIndex != OMX_DirOutput) {
        ILBCENC_EPRINT("%d :: Error - Unknown port index %ld\n", __LINE__, nPortIndex);
        return OMX_ErrorBadParameter;
    }

    pBufferList = ((nPortIndex == OMX_DirInput)? pComponentPrivate->pInputBufferList: pComponentPrivate->pOutputBufferList);
    pPortDef = pComponentPrivate->pPortDef[nPortIndex];
    for (i=0; i < pPortDef->nBufferCountActual; i++) {
        buffHdr = pBufferList->pBufHdr[i];
        if (buffHdr == pBuffer) {
            ILBCENC_DPRINT("%d Found matching %s buffer\n", __LINE__, (nPortIndex == OMX_DirInput)? "input": "output");
            ILBCENC_DPRINT("%d buffHdr = %p\n", __LINE__, buffHdr);
            ILBCENC_DPRINT("%d pBuffer = %p\n", __LINE__, pBuffer);
            bufferIndex = i;
            break;
        }
        else {
            ILBCENC_DPRINT("%d This is not a match\n", __LINE__);
            ILBCENC_DPRINT("%d buffHdr = %p\n", __LINE__, buffHdr);
            ILBCENC_DPRINT("%d pBuffer = %p\n", __LINE__, pBuffer);
        }
    }

    if (bufferIndex == -1) {
        ILBCENC_EPRINT("%d :: Error - could not find match for buffer %p\n", __LINE__, pBuffer);
        return OMX_ErrorBadParameter;
    }

    if (pBufferList->bufferOwner[bufferIndex] == 1) {
        OMX_MEMFREE_STRUCT_DSPALIGN(buffHdr->pBuffer, OMX_U8);
    }

#ifdef __PERF_INSTRUMENTATION__
    PERF_SendingBuffer(pComponentPrivate->pPERF,
                       buffHdr->pBuffer,
                       buffHdr->nAllocLen,
                       PERF_ModuleMemory );
#endif

    if (nPortIndex == OMX_DirOutput) {
        OMX_MEMFREE_STRUCT(buffHdr->pOutputPortPrivate);
    }

    ILBCENC_DPRINT("%d: Freeing: %p Buf Header\n\n", __LINE__, buffHdr);
    OMX_MEMFREE_STRUCT(pBufferList->pBufHdr[bufferIndex]);
    pBufferList->numBuffers--;

    if (pBufferList->numBuffers < pPortDef->nBufferCountMin) {
        ILBCENC_DPRINT("%d :: %s ::setting port %d populated to OMX_FALSE\n", __LINE__, __FUNCTION__, nPortIndex);
        pPortDef->bPopulated = OMX_FALSE;
    }

    ILBCENC_DPRINT("%d ::numBuffers = %d nBufferCountMin = %ld\n", __LINE__, pBufferList->numBuffers, pPortDef->nBufferCountMin);
    if (pPortDef->bEnabled &&
        pComponentPrivate->bLoadedCommandPending == OMX_FALSE &&
        (pComponentPrivate->curState == OMX_StateIdle ||
         pComponentPrivate->curState == OMX_StateExecuting ||
         pComponentPrivate->curState == OMX_StatePause)) {
        pComponentPrivate->cbInfo.EventHandler(pHandle,
                                               pHandle->pApplicationPrivate,
                                               OMX_EventError,
                                               OMX_ErrorPortUnpopulated,
                                               nPortIndex,
                                               NULL);
    }
    
    pthread_mutex_lock(&pComponentPrivate->ToLoaded_mutex);
    ILBCENC_DPRINT("num input buffers es %d, \n num output buffers es %d,\n InIdle_goingtoloaded es %d \n",
                   pComponentPrivate->pInputBufferList->numBuffers, pComponentPrivate->pOutputBufferList->numBuffers, pComponentPrivate->InIdle_goingtoloaded);

    if ((!pComponentPrivate->pInputBufferList->numBuffers &&
         !pComponentPrivate->pOutputBufferList->numBuffers) &&
        pComponentPrivate->InIdle_goingtoloaded) {
        pComponentPrivate->InIdle_goingtoloaded = 0;
#ifndef UNDER_CE
        pthread_mutex_lock(&pComponentPrivate->InIdle_mutex);
        pthread_cond_signal(&pComponentPrivate->InIdle_threshold);
        pthread_mutex_unlock(&pComponentPrivate->InIdle_mutex);
#else
        OMX_SignalEvent(&(pComponentPrivate->InIdle_event));
#endif
    }
    pthread_mutex_unlock(&pComponentPrivate->ToLoaded_mutex);
    if (pComponentPrivate->bDisableCommandPending && 
        (pComponentPrivate->pInputBufferList->numBuffers + 
         pComponentPrivate->pOutputBufferList->numBuffers == 0)) {
        SendCommand (pComponentPrivate->pHandle,
                     OMX_CommandPortDisable,
                     pComponentPrivate->bDisableCommandParam,
                     NULL);
    }
    ILBCENC_DPRINT("%d Exiting FreeBuffer\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
  *  UseBuffer()

  * @param pComp         handle for this instance of the component
  * @param pCallBacks    application callbacks
  * @param ptr
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/
static OMX_ERRORTYPE UseBuffer (
            OMX_IN OMX_HANDLETYPE hComponent,
            OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
            OMX_IN OMX_U32 nPortIndex,
            OMX_IN OMX_PTR pAppPrivate,
            OMX_IN OMX_U32 nSizeBytes,
            OMX_IN OMX_U8* pBuffer)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHeader = NULL;

    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }

#endif
#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedBuffer(pComponentPrivate->pPERF,
                        pBuffer, nSizeBytes,
                        PERF_ModuleHLMM);
#endif

    pPortDef = ((ILBCENC_COMPONENT_PRIVATE*)pComponentPrivate)->pPortDef[nPortIndex];
    ILBCENC_DPRINT("%d Entering UseBuffer\n", __LINE__);
    ILBCENC_DPRINT("%d pPortDef->bPopulated = %d \n",__LINE__,pPortDef->bPopulated);

    if(!pPortDef->bEnabled) {
        ILBCENC_EPRINT("%d About to return OMX_ErrorIncorrectStateOperation\n",__LINE__);
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }
    if(pPortDef->bPopulated) {
        ILBCENC_EPRINT ("%d In UseBuffer\n", __LINE__);
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    OMX_MALLOC_GENERIC(pBufferHeader, OMX_BUFFERHEADERTYPE);
    
    if (nPortIndex == ILBCENC_OUTPUT_PORT) {
        pBufferHeader->nInputPortIndex = -1;
        pBufferHeader->nOutputPortIndex = nPortIndex;
        pComponentPrivate->pOutputBufferList->pBufHdr[pComponentPrivate->pOutputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pOutputBufferList->bBufferPending[pComponentPrivate->pOutputBufferList->numBuffers] = 0;
        pComponentPrivate->pOutputBufferList->bufferOwner[pComponentPrivate->pOutputBufferList->numBuffers++] = 0;
        OMX_MALLOC_GENERIC(pBufferHeader->pOutputPortPrivate, ILBCENC_BUFDATA);
        if (pComponentPrivate->pOutputBufferList->numBuffers == pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
        }
    }
    else {
        pBufferHeader->nInputPortIndex = nPortIndex;
        pBufferHeader->nOutputPortIndex = -1;
        pComponentPrivate->pInputBufferList->pBufHdr[pComponentPrivate->pInputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pInputBufferList->bBufferPending[pComponentPrivate->pInputBufferList->numBuffers] = 0;
        pComponentPrivate->pInputBufferList->bufferOwner[pComponentPrivate->pInputBufferList->numBuffers++] = 0;
        if (pComponentPrivate->pInputBufferList->numBuffers == pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
        }
    }

    if (((!pComponentPrivate->dasfMode) &&
         ((pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bEnabled)&&
          (pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bEnabled) &&
          (pComponentPrivate->InLoaded_readytoidle)))/*File Mode*/  
        || 
        ((pComponentPrivate->dasfMode) &&
         ((pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bEnabled)&&
          (pComponentPrivate->InLoaded_readytoidle))))/*Dasf Mode*/  {
        pComponentPrivate->InLoaded_readytoidle = 0;
#ifndef UNDER_CE
        pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex);
        pthread_cond_signal(&pComponentPrivate->InLoaded_threshold);
        pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
#else
        OMX_SignalEvent(&(pComponentPrivate->InLoaded_event));
#endif
    }
    pBufferHeader->pAppPrivate = pAppPrivate;
    pBufferHeader->pPlatformPrivate = pComponentPrivate;
    pBufferHeader->nAllocLen = nSizeBytes;
    pBufferHeader->nVersion.s.nVersionMajor = ILBCENC_MAJOR_VER;
    pBufferHeader->nVersion.s.nVersionMinor = ILBCENC_MINOR_VER;
    pComponentPrivate->nVersion = pBufferHeader->nVersion.nVersion;
    pBufferHeader->pBuffer = pBuffer;
    pBufferHeader->nSize = sizeof(OMX_BUFFERHEADERTYPE);
    *ppBufferHdr = pBufferHeader;
    
    if (pComponentPrivate->bEnableCommandPending){
        SendCommand (pComponentPrivate->pHandle,
                    OMX_CommandPortEnable,
                    pComponentPrivate->bEnableCommandParam,
                    NULL);
    }
    
 EXIT:
    ILBCENC_DPRINT("%d Exiting UseBuffer\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ================================================================================= */
/**
* @fn GetExtensionIndex() description for GetExtensionIndex
GetExtensionIndex().
Returns index for vendor specific settings.
*
*  @see         OMX_Core.h
*/
/* ================================================================================ */
static OMX_ERRORTYPE GetExtensionIndex(
            OMX_IN  OMX_HANDLETYPE hComponent,
            OMX_IN  OMX_STRING cParameterName,
            OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    ILBCENC_DPRINT("GetExtensionIndex\n");
    
    if (!(strcmp(cParameterName,"OMX.TI.index.config.tispecific"))) {
        *pIndexType = OMX_IndexCustomILBCENCModeConfig;
    }
    else if(!(strcmp(cParameterName,"OMX.TI.index.config.ilbcstreamIDinfo"))) {
        *pIndexType = OMX_IndexCustomILBCENCStreamIDConfig;
    }
    else if(!(strcmp(cParameterName,"OMX.TI.index.config.ilbc.datapath"))) {
        *pIndexType = OMX_IndexCustomILBCENCDataPath;
    }
    else if(!(strcmp(cParameterName,"OMX.TI.index.config.ilbc.paramAudio"))){
        *pIndexType = OMX_IndexParamAudioILBC;
    }
    else if(!(strcmp(cParameterName,"OMX.TI.index.config.ilbc.codingType"))){
        *pIndexType = OMX_IndexCustomiLBCCodingType;
    }
    else {
        eError = OMX_ErrorBadParameter;
    }

    ILBCENC_DPRINT("Exiting GetExtensionIndex\n");
    return eError;
}

#ifdef UNDER_CE
/* ================================================================================= */
/**
* @fns Sleep replace for WIN CE
*/
/* ================================================================================ */
int OMX_CreateEvent(OMX_Event *event){
    int ret = OMX_ErrorNone;
    HANDLE createdEvent = NULL;
    if(event == NULL){
        ret = OMX_ErrorBadParameter;
        goto EXIT;
    }
    event->event  = CreateEvent(NULL, TRUE, FALSE, NULL);
    if(event->event == NULL)
        ret = (int)GetLastError();
EXIT:
    return ret;
}

int OMX_SignalEvent(OMX_Event *event){
     int ret = OMX_ErrorNone;
     if(event == NULL){
        ret = OMX_ErrorBadParameter;
        goto EXIT;
     }
     SetEvent(event->event);
     ret = (int)GetLastError();
EXIT:
    return ret;
}

int OMX_WaitForEvent(OMX_Event *event) {
     int ret = OMX_ErrorNone;
     if(event == NULL){
        ret = OMX_ErrorBadParameter;
        goto EXIT;
     }
     WaitForSingleObject(event->event, INFINITE);
     ret = (int)GetLastError();
EXIT:
     return ret;
}

int OMX_DestroyEvent(OMX_Event *event) {
     int ret = OMX_ErrorNone;
     if(event == NULL){
        ret = OMX_ErrorBadParameter;
        goto EXIT;
     }
     CloseHandle(event->event);
EXIT:
     return ret;
}
#endif

/* ================================================================================= */
/**
* @fn ComponentRoleEnum() description for ComponentRoleEnum()  

Returns the role at the given index
*
*  @see         OMX_Core.h
*/
/* ================================================================================ */
static OMX_ERRORTYPE ComponentRoleEnum(
         OMX_IN OMX_HANDLETYPE hComponent,
      OMX_OUT OMX_U8 *cRole,
      OMX_IN OMX_U32 nIndex)
{
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if(nIndex == 0){
        if (cRole == NULL) {
            eError = OMX_ErrorBadParameter;
	}
	else {
            memcpy(cRole, &pComponentPrivate->componentRole.cRole, sizeof(OMX_U8) * OMX_MAX_STRINGNAME_SIZE); 
            ILBCENC_DPRINT("::::In ComponenetRoleEnum: cRole is set to %s\n",cRole);
	}
    }
    else {
        eError = OMX_ErrorNoMore;
    }
    return eError;
}

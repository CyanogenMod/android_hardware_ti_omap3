
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
 * @file OMX_WbAmrEncoder.c
 *
 * This file implements OpenMAX (TM) 1.0 Specific APIs and its functionality
 * that is fully compliant with the Khronos OpenMAX (TM) 1.0 Specification
 *
 * @path  $(CSLPATH)\OMAPSW_MPU\linux\audio\src\openmax_il\wbamr_enc\src
 *
 * @rev  1.0
 */
/* ----------------------------------------------------------------------------
 *!
 *! Revision History
 *! ===================================
 *! 21-sept-2006 bk: updated review findings for alpha release
 *! 24-Aug-2006 bk: Khronos OpenMAX (TM) 1.0 Conformance tests some more
 *! 18-July-2006 bk: Khronos OpenMAX (TM) 1.0 Conformance tests validated for few cases
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
#include <wchar.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>
#include <pthread.h>

#include <semaphore.h>
#endif

#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <dbapi.h>
#include <dlfcn.h>

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
#include <TIDspOmx.h>
#include "OMX_WbAmrEncoder.h"
#include "OMX_WbAmrEnc_Utils.h"

#ifdef WBAMRENC_DEBUGMEM
extern void * mymalloc(int line, char *s, int size);
extern int myfree(void *dp, int line, char *s);

#define newmalloc(x) mymalloc(__LINE__,__FILE__,x)
#define newfree(z) myfree(z,__LINE__,__FILE__)
#else
#define newmalloc(x) malloc(x)
#define newfree(z) free(z)
#endif

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

#define AMRWB_ENC_ROLE "audio_encoder.amrwb"

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

static OMX_ERRORTYPE FreeBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
                                OMX_IN  OMX_U32 nPortIndex,
                                OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer);

static OMX_ERRORTYPE UseBuffer (OMX_IN OMX_HANDLETYPE hComponent,
                                OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                                OMX_IN OMX_U32 nPortIndex,
                                OMX_IN OMX_PTR pAppPrivate,
                                OMX_IN OMX_U32 nSizeBytes,
                                OMX_IN OMX_U8* pBuffer);

static OMX_ERRORTYPE GetExtensionIndex(OMX_IN  OMX_HANDLETYPE hComponent,
                                       OMX_IN  OMX_STRING cParameterName,
                                       OMX_OUT OMX_INDEXTYPE* pIndexType);
            
static OMX_ERRORTYPE ComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent,
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
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef_ip, *pPortDef_op;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate;
    OMX_AUDIO_PARAM_PCMMODETYPE *amr_ip;
    OMX_AUDIO_PARAM_AMRTYPE  *amr_op;
    
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE*) hComp;
    WBAMRENC_PORT_TYPE *pCompPort = NULL;
    OMX_AUDIO_PARAM_PORTFORMATTYPE *pPortFormat = NULL;
    int i = 0;

    WBAMRENC_DPRINT("%d :: Entering OMX_ComponentInit\n", __LINE__);
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
    WBAMRENC_OMX_MALLOC(pHandle->pComponentPrivate, WBAMRENC_COMPONENT_PRIVATE);
    memset(pHandle->pComponentPrivate, 0x0, sizeof(WBAMRENC_COMPONENT_PRIVATE));

    ((WBAMRENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->pHandle = pHandle;
    pComponentPrivate = pHandle->pComponentPrivate;

#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate->pPERF = PERF_Create(PERF_FOURCC('W','B','_','E'),
                                           PERF_ModuleLLMM |
                                           PERF_ModuleAudioDecode);
#endif

    WBAMRENC_OMX_MALLOC(pCompPort, WBAMRENC_PORT_TYPE);
    pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT] = pCompPort;

    WBAMRENC_OMX_MALLOC(pCompPort, WBAMRENC_PORT_TYPE);
    pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT] = pCompPort;

    WBAMRENC_OMX_MALLOC(pComponentPrivate->sPortParam, OMX_PORT_PARAM_TYPE);
    OMX_WBCONF_INIT_STRUCT(pComponentPrivate->sPortParam, OMX_PORT_PARAM_TYPE);

    /* Initialize sPortParam data structures to default values */
    pComponentPrivate->sPortParam->nPorts = 0x2;
    pComponentPrivate->sPortParam->nStartPortNumber = 0x0;

    /* Malloc and Set pPriorityMgmt defaults */
    WBAMRENC_OMX_MALLOC(pComponentPrivate->sPriorityMgmt, OMX_PRIORITYMGMTTYPE);
    OMX_WBCONF_INIT_STRUCT(pComponentPrivate->sPriorityMgmt, OMX_PRIORITYMGMTTYPE);

    /* Initialize sPriorityMgmt data structures to default values */
    pComponentPrivate->sPriorityMgmt->nGroupPriority = -1;
    pComponentPrivate->sPriorityMgmt->nGroupID = -1;

    WBAMRENC_OMX_MALLOC(amr_ip, OMX_AUDIO_PARAM_PCMMODETYPE);
    OMX_WBCONF_INIT_STRUCT(amr_ip, OMX_AUDIO_PARAM_PCMMODETYPE);
    pComponentPrivate->pcmParams = amr_ip;

    WBAMRENC_OMX_MALLOC(amr_op, OMX_AUDIO_PARAM_AMRTYPE);
    OMX_WBCONF_INIT_STRUCT(amr_op, OMX_AUDIO_PARAM_AMRTYPE);
    pComponentPrivate->amrParams = amr_op;

    /* newmalloc and initialize number of input buffers */
    WBAMRENC_OMX_MALLOC(pComponentPrivate->pInputBufferList, WBAMRENC_BUFFERLIST);
    pComponentPrivate->pInputBufferList->numBuffers = 0;

    /* newmalloc and initialize number of output buffers */
    WBAMRENC_OMX_MALLOC(pComponentPrivate->pOutputBufferList, WBAMRENC_BUFFERLIST);
    pComponentPrivate->pOutputBufferList->numBuffers = 0;

    for (i=0; i < WBAMRENC_MAX_NUM_OF_BUFS; i++) {
        pComponentPrivate->pOutputBufferList->pBufHdr[i] = NULL;
        pComponentPrivate->pInputBufferList->pBufHdr[i] = NULL;
    }

    WBAMRENC_OMX_MALLOC_SIZE(pComponentPrivate->sDeviceString, 100*sizeof(OMX_STRING),OMX_STRING);
    
    /* Initialize device string to the default value */
    strcpy((char*)pComponentPrivate->sDeviceString,":srcul/codec\0");

    /* Set input port defaults */
    WBAMRENC_OMX_MALLOC(pPortDef_ip, OMX_PARAM_PORTDEFINITIONTYPE);
    OMX_WBCONF_INIT_STRUCT(pPortDef_ip, OMX_PARAM_PORTDEFINITIONTYPE);
    pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT] = pPortDef_ip;

    pPortDef_ip->nPortIndex                         = WBAMRENC_INPUT_PORT;
    pPortDef_ip->eDir                               = OMX_DirInput;
    pPortDef_ip->nBufferCountActual                 = WBAMRENC_NUM_INPUT_BUFFERS;
    pPortDef_ip->nBufferCountMin                    = WBAMRENC_NUM_INPUT_BUFFERS;
    pPortDef_ip->nBufferSize                        = WBAMRENC_INPUT_FRAME_SIZE;
    pPortDef_ip->bEnabled                           = OMX_TRUE;
    pPortDef_ip->bPopulated                         = OMX_FALSE;
    pPortDef_ip->eDomain                            = OMX_PortDomainAudio;
    pPortDef_ip->format.audio.eEncoding             = OMX_AUDIO_CodingPCM;
    pPortDef_ip->format.audio.cMIMEType             = NULL;
    pPortDef_ip->format.audio.pNativeRender         = NULL;
    pPortDef_ip->format.audio.bFlagErrorConcealment = OMX_FALSE;

    /* Set output port defaults */
    WBAMRENC_OMX_MALLOC(pPortDef_op, OMX_PARAM_PORTDEFINITIONTYPE);
    OMX_WBCONF_INIT_STRUCT(pPortDef_op, OMX_PARAM_PORTDEFINITIONTYPE);
    pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT] = pPortDef_op;

    pPortDef_op->nPortIndex                         = WBAMRENC_OUTPUT_PORT;
    pPortDef_op->eDir                               = OMX_DirOutput;
    pPortDef_op->nBufferCountMin                    = WBAMRENC_NUM_OUTPUT_BUFFERS;
    pPortDef_op->nBufferCountActual                 = WBAMRENC_NUM_OUTPUT_BUFFERS;
    pPortDef_op->nBufferSize                        = WBAMRENC_OUTPUT_FRAME_SIZE;
    pPortDef_op->bEnabled                           = OMX_TRUE;
    pPortDef_op->bPopulated                         = OMX_FALSE;
    pPortDef_op->eDomain                            = OMX_PortDomainAudio;
    pPortDef_op->format.audio.eEncoding             = OMX_AUDIO_CodingAMR;
    pPortDef_op->format.audio.cMIMEType             = NULL;
    pPortDef_op->format.audio.pNativeRender         = NULL;
    pPortDef_op->format.audio.bFlagErrorConcealment = OMX_FALSE;

    WBAMRENC_OMX_MALLOC(pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT]->pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    OMX_WBCONF_INIT_STRUCT(pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT]->pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);

    pComponentPrivate->bPreempted = OMX_FALSE; 
    
    /* Set input port format defaults */
    pPortFormat = pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT]->pPortFormat;
    OMX_WBCONF_INIT_STRUCT(pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    pPortFormat->nPortIndex         = WBAMRENC_INPUT_PORT;
    pPortFormat->nIndex             = OMX_IndexParamAudioAmr;
    pPortFormat->eEncoding          = OMX_AUDIO_CodingPCM;  /*Data Expected on Input Port*/
    
    amr_ip->nPortIndex = WBAMRENC_INPUT_PORT; 
    amr_ip->nChannels = 2; 
    amr_ip->eNumData= OMX_NumericalDataSigned; 
    amr_ip->nBitPerSample = 16;  
    amr_ip->nSamplingRate = 44100;           
    amr_ip->ePCMMode = OMX_AUDIO_PCMModeLinear; 
    amr_ip->bInterleaved = OMX_TRUE; /*For Encoders Only*/


    amr_op->nPortIndex = WBAMRENC_OUTPUT_PORT;
    amr_op->nChannels = 1; 
    amr_op->eAMRBandMode = OMX_AUDIO_AMRBandModeWB0;
    amr_op->eAMRDTXMode= OMX_AUDIO_AMRDTXModeOff;
    amr_op->eAMRFrameFormat =OMX_AUDIO_AMRFrameFormatConformance;

    WBAMRENC_OMX_MALLOC(pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT]->pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    OMX_WBCONF_INIT_STRUCT(pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT]->pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
   
    /* Set output port format defaults */
    pPortFormat = pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT]->pPortFormat;
    OMX_WBCONF_INIT_STRUCT(pPortFormat, OMX_AUDIO_PARAM_PORTFORMATTYPE);
    pPortFormat->nPortIndex         = WBAMRENC_OUTPUT_PORT;
    pPortFormat->nIndex             = OMX_IndexParamAudioAmr;
    pPortFormat->eEncoding          = OMX_AUDIO_CodingAMR;

    WBAMRENC_DPRINT("%d :: Setting dasf & mime & efr & acdn & amr and MultiFrame modes to 0\n",__LINE__);
    pComponentPrivate->dasfMode = 0;

    pComponentPrivate->frameMode = 0;
    pComponentPrivate->acdnMode = 0;
    pComponentPrivate->efrMode  = 0;
    pComponentPrivate->amrMode  = 0;
    pComponentPrivate->nMultiFrameMode = 0;
    pComponentPrivate->bCompThreadStarted = 0;
    pComponentPrivate->pParams = NULL;
    pComponentPrivate->pAlgParam = NULL;
    pComponentPrivate->bInitParamsInitialized = 0;
    pComponentPrivate->amrMimeBytes[0]  = WBAMRENC_FRAME_SIZE_18;
    pComponentPrivate->amrMimeBytes[1]  = WBAMRENC_FRAME_SIZE_24;
    pComponentPrivate->amrMimeBytes[2]  = WBAMRENC_FRAME_SIZE_33;
    pComponentPrivate->amrMimeBytes[3]  = WBAMRENC_FRAME_SIZE_37;
    pComponentPrivate->amrMimeBytes[4]  = WBAMRENC_FRAME_SIZE_41;
    pComponentPrivate->amrMimeBytes[5]  = WBAMRENC_FRAME_SIZE_47;
    pComponentPrivate->amrMimeBytes[6]  = WBAMRENC_FRAME_SIZE_51;
    pComponentPrivate->amrMimeBytes[7]  = WBAMRENC_FRAME_SIZE_59;
    pComponentPrivate->amrMimeBytes[8]  = WBAMRENC_FRAME_SIZE_61;
    pComponentPrivate->amrMimeBytes[9]  = WBAMRENC_FRAME_SIZE_6;
    pComponentPrivate->amrMimeBytes[10] = WBAMRENC_FRAME_SIZE_0;
    pComponentPrivate->amrMimeBytes[11] = WBAMRENC_FRAME_SIZE_0;
    pComponentPrivate->amrMimeBytes[12] = WBAMRENC_FRAME_SIZE_0;
    pComponentPrivate->amrMimeBytes[13] = WBAMRENC_FRAME_SIZE_0;
    pComponentPrivate->amrMimeBytes[14] = WBAMRENC_FRAME_SIZE_1;
    pComponentPrivate->amrMimeBytes[15] = WBAMRENC_FRAME_SIZE_1;

    pComponentPrivate->amrIf2Bytes[0]  = WBAMRENC_FRAME_SIZE_18;
    pComponentPrivate->amrIf2Bytes[1]  = WBAMRENC_FRAME_SIZE_23;
    pComponentPrivate->amrIf2Bytes[2]  = WBAMRENC_FRAME_SIZE_33;
    pComponentPrivate->amrIf2Bytes[3]  = WBAMRENC_FRAME_SIZE_37;
    pComponentPrivate->amrIf2Bytes[4]  = WBAMRENC_FRAME_SIZE_41;
    pComponentPrivate->amrIf2Bytes[5]  = WBAMRENC_FRAME_SIZE_47;
    pComponentPrivate->amrIf2Bytes[6]  = WBAMRENC_FRAME_SIZE_51;
    pComponentPrivate->amrIf2Bytes[7]  = WBAMRENC_FRAME_SIZE_59;
    pComponentPrivate->amrIf2Bytes[8]  = WBAMRENC_FRAME_SIZE_61;
    pComponentPrivate->amrIf2Bytes[9]  = WBAMRENC_FRAME_SIZE_6;
    pComponentPrivate->amrIf2Bytes[10] = WBAMRENC_FRAME_SIZE_0;
    pComponentPrivate->amrIf2Bytes[11] = WBAMRENC_FRAME_SIZE_0;
    pComponentPrivate->amrIf2Bytes[12] = WBAMRENC_FRAME_SIZE_0;
    pComponentPrivate->amrIf2Bytes[13] = WBAMRENC_FRAME_SIZE_0;
    pComponentPrivate->amrIf2Bytes[14] = WBAMRENC_FRAME_SIZE_1;
    pComponentPrivate->amrIf2Bytes[15] = WBAMRENC_FRAME_SIZE_1;
    
    pComponentPrivate->pMarkBuf = NULL;
    pComponentPrivate->pMarkData = NULL;
    pComponentPrivate->nEmptyBufferDoneCount = 0;
    pComponentPrivate->nEmptyThisBufferCount = 0;
    pComponentPrivate->nFillBufferDoneCount = 0;
    pComponentPrivate->nFillThisBufferCount = 0;
    pComponentPrivate->strmAttr = NULL;
    pComponentPrivate->bDisableCommandParam = 0;
    pComponentPrivate->iHoldLen = 0;
    pComponentPrivate->iHoldBuffer = NULL;
    pComponentPrivate->pHoldBuffer = NULL;
    pComponentPrivate->nHoldLength = 0;
    for (i=0; i < WBAMRENC_MAX_NUM_OF_BUFS; i++) {
        pComponentPrivate->pInputBufHdrPending[i] = NULL;
        pComponentPrivate->pOutputBufHdrPending[i] = NULL;
        pComponentPrivate->arrTickCount[i] = 0;
        pComponentPrivate->arrBufIndex[i] = 0;
    }
    pComponentPrivate->nNumInputBufPending = 0;
    pComponentPrivate->nNumOutputBufPending = 0;
    pComponentPrivate->bDisableCommandPending = 0;
    pComponentPrivate->nNumOfFramesSent = 0;

    pComponentPrivate->bNoIdleOnStop= OMX_FALSE;
    pComponentPrivate->bDspStoppedWhileExecuting    = OMX_FALSE;
    pComponentPrivate->nOutStandingFillDones = 0;
    pComponentPrivate->nOutStandingEmptyDones = 0;    
    pComponentPrivate->bNoIdleOnStop = OMX_FALSE;
    pComponentPrivate->IpBufindex = 0;
    pComponentPrivate->OpBufindex = 0;
    pComponentPrivate->ptrLibLCML = NULL;
    pComponentPrivate->ProcessingInputBuf = 0;
    pComponentPrivate->ProcessingOutputBuf = 0;
    strcpy((char*)pComponentPrivate->componentRole.cRole, "audio_encoder.amrwb");  
    
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
    WBAMRENC_DPRINT("%d :: OMX_ComponentInit\n", __LINE__);
    if (eError != OMX_ErrorNone) {
        WBAMRENC_EPRINT("%d :: Error returned from loading ResourceManagerProxy thread\n",__LINE__);
        goto EXIT;
    }
#endif

    eError = WBAMRENC_StartComponentThread(pHandle);
    WBAMRENC_DPRINT("%d :: OMX_ComponentInit\n", __LINE__);
    if (eError != OMX_ErrorNone) {
        WBAMRENC_EPRINT("%d :: Error returned from the Component\n",__LINE__);
        goto EXIT;
    }

#ifdef __PERF_INSTRUMENTATION__
    PERF_ThreadCreated(pComponentPrivate->pPERF, pComponentPrivate->ComponentThread,
                       PERF_FOURCC('W','B','E','T'));
#endif

#ifndef UNDER_CE

    if((pComponentPrivate->fdwrite=open(FIFO1,O_WRONLY))<0) {
        WBAMRENC_DPRINT("%d :: [WBAMRENC Encoder Component] - failure to open WRITE pipe\n",__LINE__);
    }

    if((pComponentPrivate->fdread=open(FIFO2,O_RDONLY))<0) {
        WBAMRENC_DPRINT("%d :: [WBAMRENC Encoder Component] - failure to open READ pipe\n",__LINE__);
    }
    
#endif

 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting OMX_ComponentInit\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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

    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate =
        (WBAMRENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    WBAMRENC_DPRINT("%d :: Entering SetCallbacks\n", __LINE__);
    if (pCallBacks == NULL) {
        eError = OMX_ErrorBadParameter;
        WBAMRENC_DPRINT("%d :: Received the empty callbacks from the application\n",__LINE__);
        goto EXIT;
    }

    /*Copy the callbacks of the application to the component private*/
    memcpy (&(pComponentPrivate->cbInfo), pCallBacks, sizeof(OMX_CALLBACKTYPE));

    /*copy the application private data to component memory */
    pHandle->pApplicationPrivate = pAppData;

    pComponentPrivate->curState = OMX_StateLoaded;

 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting SetCallbacks\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *) pHandle->pComponentPrivate;
    WBAMRENC_DPRINT("%d :: Entering GetComponentVersion\n", __LINE__);
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
        WBAMRENC_EPRINT("%d :: OMX_ErrorBadParameter from GetComponentVersion",__LINE__);
        eError = OMX_ErrorBadParameter;
    }
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting GetComponentVersion\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    WBAMRENC_COMPONENT_PRIVATE *pCompPrivate =
        (WBAMRENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    int nRet = 0;

    WBAMRENC_DPRINT("%d :: Entering SendCommand()\n", __LINE__);
    if(pCompPrivate->curState == OMX_StateInvalid) {
        eError = OMX_ErrorInvalidState;
        WBAMRENC_EPRINT("%d :: Error OMX_ErrorInvalidState Sent to App\n",__LINE__);
        goto EXIT;
    }
#ifdef __PERF_INSTRUMENTATION__
    PERF_SendingCommand(pCompPrivate->pPERF,
                        Cmd,
                        (Cmd == OMX_CommandMarkBuffer) ? ((OMX_U32) pCmdData) : nParam,
                        PERF_ModuleComponent);
#endif
    switch(Cmd) {
    case OMX_CommandStateSet:
        WBAMRENC_DPRINT("%d :: OMX_CommandStateSet SendCommand\n",__LINE__);
        if (nParam == OMX_StateLoaded) {
            pCompPrivate->bLoadedCommandPending = OMX_TRUE;
        }                   
        if(pCompPrivate->curState == OMX_StateLoaded) {
            if((nParam == OMX_StateExecuting) || (nParam == OMX_StatePause)) {
                pCompPrivate->cbInfo.EventHandler ( pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventError,
                                                    OMX_ErrorIncorrectStateTransition,
                                                    OMX_TI_ErrorMinor,
                                                    NULL);
                goto EXIT;
            }

            if(nParam == OMX_StateInvalid) {
                WBAMRENC_DPRINT("%d :: OMX_CommandStateSet SendCommand\n",__LINE__);
                pCompPrivate->curState = OMX_StateInvalid;
                pCompPrivate->cbInfo.EventHandler ( pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventError,
                                                    OMX_ErrorInvalidState,
                                                    OMX_TI_ErrorMinor,
                                                    NULL);
                goto EXIT;
            }
        }
        break;
    case OMX_CommandFlush:
        WBAMRENC_DPRINT("%d :: OMX_CommandFlush SendCommand\n",__LINE__);
        if(nParam > 1 && nParam != -1) {
            eError = OMX_ErrorBadPortIndex;
            WBAMRENC_EPRINT("%d :: OMX_ErrorBadPortIndex from SendCommand",__LINE__);
            goto EXIT;
        }
        break;
    case OMX_CommandPortDisable:
        WBAMRENC_DPRINT("%d :: OMX_CommandPortDisable SendCommand\n",__LINE__);
        break;
    case OMX_CommandPortEnable:
        WBAMRENC_DPRINT("%d :: OMX_CommandPortEnable SendCommand\n",__LINE__);
        break;
    case OMX_CommandMarkBuffer:
        WBAMRENC_DPRINT("%d :: OMX_CommandMarkBuffer SendCommand\n",__LINE__);
        if (nParam > 0) {
            eError = OMX_ErrorBadPortIndex;
            WBAMRENC_EPRINT("%d :: OMX_ErrorBadPortIndex from SendCommand",__LINE__);
            goto EXIT;
        }
        break;
    default:
        WBAMRENC_DPRINT("%d :: Command Received Default eError\n",__LINE__);
        pCompPrivate->cbInfo.EventHandler ( pHandle,
                                            pHandle->pApplicationPrivate,
                                            OMX_EventError,
                                            OMX_ErrorUndefined,
                                            OMX_TI_ErrorMinor,
                                            "Invalid Command");
        break;

    }

    nRet = write (pCompPrivate->cmdPipe[1], &Cmd, sizeof(Cmd));
    if (nRet == -1) {
        eError = OMX_ErrorInsufficientResources;
        WBAMRENC_EPRINT("%d :: OMX_ErrorInsufficientResources from SendCommand",__LINE__);
        goto EXIT;
    }

    if (Cmd == OMX_CommandMarkBuffer) {
        nRet = write(pCompPrivate->cmdDataPipe[1], &pCmdData,sizeof(OMX_PTR));
    } else {
        nRet = write(pCompPrivate->cmdDataPipe[1], &nParam,sizeof(OMX_U32));
    }
    if (nRet == -1) {
        WBAMRENC_EPRINT("%d :: OMX_ErrorInsufficientResources from SendCommand",__LINE__);
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting SendCommand()\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    WBAMRENC_COMPONENT_PRIVATE  *pComponentPrivate;
    OMX_PARAM_PORTDEFINITIONTYPE *pParameterStructure;
    OMX_AUDIO_PARAM_AMRTYPE *pCompAmrParam = NULL;

    pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
    pParameterStructure = (OMX_PARAM_PORTDEFINITIONTYPE*)ComponentParameterStructure;
    WBAMRENC_DPRINT("%d :: Entering the GetParameter\n",__LINE__);
    if (pParameterStructure == NULL) {
        eError = OMX_ErrorBadParameter;
        WBAMRENC_EPRINT("%d :: OMX_ErrorBadPortIndex from GetParameter",__LINE__);
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
        WBAMRENC_EPRINT("%d :: OMX_ErrorIncorrectStateOperation from GetParameter",__LINE__);
        goto EXIT;
    }
#endif
    switch(nParamIndex){
    case OMX_IndexParamAudioInit:
        {
            WBAMRENC_DPRINT("%d :: GetParameter OMX_IndexParamAudioInit \n",__LINE__);
            memcpy(ComponentParameterStructure, pComponentPrivate->sPortParam, sizeof(OMX_PORT_PARAM_TYPE));
        }
        break;

    case OMX_IndexParamPortDefinition:
        {
            WBAMRENC_DPRINT("%d :: GetParameter OMX_IndexParamPortDefinition \n",__LINE__);
            if(((OMX_PARAM_PORTDEFINITIONTYPE *)(ComponentParameterStructure))->nPortIndex ==
               pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->nPortIndex) {
                memcpy(ComponentParameterStructure, pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT], sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            } else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(ComponentParameterStructure))->nPortIndex ==
                      pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->nPortIndex) {
                memcpy(ComponentParameterStructure, pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT], sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            } else {
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadPortIndex from GetParameter \n",__LINE__);
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;

    case OMX_IndexParamAudioPortFormat:
        {
            WBAMRENC_DPRINT("%d :: GetParameter OMX_IndexParamAudioPortFormat \n",__LINE__);
            if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex ==
               pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->nPortIndex) {
                if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nIndex >
                   pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT]->pPortFormat->nPortIndex) {
                    eError = OMX_ErrorNoMore;
                }
                else {
                    memcpy(ComponentParameterStructure, pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT]->pPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
                }
            }
            else if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex ==
                    pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->nPortIndex){
                if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nIndex >
                   pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT]->pPortFormat->nPortIndex) {
                    eError = OMX_ErrorNoMore;
                }
                else {
                    memcpy(ComponentParameterStructure, pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT]->pPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
                }
            }
            else {
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadPortIndex from GetParameter \n",__LINE__);
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;

    case OMX_IndexParamAudioAmr:
        {                            
            WBAMRENC_DPRINT("%d :: GetParameter OMX_IndexParamAudioAmr \n",__LINE__);
            memcpy(ComponentParameterStructure, pComponentPrivate->amrParams, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
            pCompAmrParam = (OMX_AUDIO_PARAM_AMRTYPE *)ComponentParameterStructure;
            switch (pCompAmrParam->eAMRBandMode)
                {
                case SN_AUDIO_BR660:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB0;
                    break;
                case SN_AUDIO_BR885:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB1;
                    break;
                case SN_AUDIO_BR1265:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB2;
                    break;
                case SN_AUDIO_BR1425:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB3;
                    break;
                case SN_AUDIO_BR1585:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB4;
                    break;
                case SN_AUDIO_BR1825:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB5;
                    break;
                case SN_AUDIO_BR1985:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB6;
                    break;
                case SN_AUDIO_BR2305:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB7;
                    break;
                case SN_AUDIO_BR2385:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB8;
                    break;
                default:
                    pCompAmrParam->eAMRBandMode = OMX_AUDIO_AMRBandModeWB0;
                    break;
                }

        }
        break;

    case OMX_IndexParamPriorityMgmt:
        {
            WBAMRENC_DPRINT("%d :: GetParameter OMX_IndexParamPriorityMgmt \n",__LINE__);
            memcpy(ComponentParameterStructure, pComponentPrivate->sPriorityMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
        }
        break;

    case OMX_IndexParamAudioPcm:
        memcpy(ComponentParameterStructure, pComponentPrivate->pcmParams, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        break;

    case OMX_IndexParamCompBufferSupplier:
        if(((OMX_PARAM_BUFFERSUPPLIERTYPE *)(ComponentParameterStructure))->nPortIndex == WBAMRENC_INPUT_PORT) {
            WBAMRENC_DPRINT(":: GetParameter OMX_IndexParamCompBufferSupplier \n");
        }
        else if(((OMX_PARAM_BUFFERSUPPLIERTYPE *)(ComponentParameterStructure))->nPortIndex == WBAMRENC_OUTPUT_PORT) {
            WBAMRENC_DPRINT(":: GetParameter OMX_IndexParamCompBufferSupplier \n");
        } 
        else {
            WBAMRENC_DPRINT(":: OMX_ErrorBadPortIndex from GetParameter");
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
        {
            WBAMRENC_EPRINT("%d :: OMX_ErrorUnsupportedIndex GetParameter \n",__LINE__);
            eError = OMX_ErrorUnsupportedIndex;
        }
        break;
    }
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting GetParameter\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    WBAMRENC_COMPONENT_PRIVATE  *pComponentPrivate;
    OMX_AUDIO_PARAM_PORTFORMATTYPE* pComponentParam = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pComponentParamPort = NULL;
    OMX_AUDIO_PARAM_AMRTYPE *pCompAmrParam = NULL;
    OMX_PARAM_COMPONENTROLETYPE  *pRole;
    OMX_AUDIO_PARAM_PCMMODETYPE *amr_ip;
    OMX_PARAM_BUFFERSUPPLIERTYPE sBufferSupplier;
    
    pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);


    WBAMRENC_DPRINT("%d :: Entering the SetParameter\n",__LINE__);
    if (pCompParam == NULL) {
        eError = OMX_ErrorBadParameter;
        WBAMRENC_EPRINT("%d :: OMX_ErrorBadParameter from SetParameter",__LINE__);
        goto EXIT;
    }

    if (pComponentPrivate->curState != OMX_StateLoaded) {
        eError = OMX_ErrorIncorrectStateOperation;
        WBAMRENC_EPRINT("%d :: OMX_ErrorIncorrectStateOperation from SetParameter",__LINE__);
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
        {
            WBAMRENC_DPRINT("%d :: SetParameter OMX_IndexParamAudioPortFormat \n",__LINE__);
            pComponentParam = (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pCompParam;
            if ( pComponentParam->nPortIndex == pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT]->pPortFormat->nPortIndex ) {
                memcpy(pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT]->pPortFormat, pComponentParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            } else if ( pComponentParam->nPortIndex == pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT]->pPortFormat->nPortIndex ) {
                memcpy(pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT]->pPortFormat, pComponentParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            } else {
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadPortIndex from SetParameter",__LINE__);
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;
    case OMX_IndexParamAudioAmr:
        {
            WBAMRENC_DPRINT("%d :: SetParameter OMX_IndexParamAudioAmr \n",__LINE__);
            pCompAmrParam = (OMX_AUDIO_PARAM_AMRTYPE *)pCompParam;
            if(pCompAmrParam->nPortIndex == 0) {         /* 0 means Input port */
                memcpy(((WBAMRENC_COMPONENT_PRIVATE*)
                        pHandle->pComponentPrivate)->pcmParams, pCompAmrParam, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
            } else if (pCompAmrParam->nPortIndex == 1) { /* 1 means Output port */
                switch (pCompAmrParam->eAMRBandMode)
                    {
                    case OMX_AUDIO_AMRBandModeWB0:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR660;
                        break;
                    case OMX_AUDIO_AMRBandModeWB1:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR885;
                        break;
                    case OMX_AUDIO_AMRBandModeWB2:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR1265;
                        break;
                    case OMX_AUDIO_AMRBandModeWB3:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR1425;
                        break;
                    case OMX_AUDIO_AMRBandModeWB4:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR1585;
                        break;
                    case OMX_AUDIO_AMRBandModeWB5:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR1825;
                        break;
                    case OMX_AUDIO_AMRBandModeWB6:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR1985;
                        break;
                    case OMX_AUDIO_AMRBandModeWB7:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR2305;
                        break;
                    case OMX_AUDIO_AMRBandModeWB8:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR2385;
                        break;
                    default:
                        pCompAmrParam->eAMRBandMode = SN_AUDIO_BR660;
                        break;   
                    }
                memcpy(((WBAMRENC_COMPONENT_PRIVATE *)
                        pHandle->pComponentPrivate)->amrParams, pCompAmrParam, sizeof(OMX_AUDIO_PARAM_AMRTYPE));

                if (pCompAmrParam->eAMRFrameFormat == OMX_AUDIO_AMRFrameFormatConformance) {
                    pComponentPrivate->frameMode = WBAMRENC_FORMATCONFORMANCE;
                } 
                else if(pCompAmrParam->eAMRFrameFormat == OMX_AUDIO_AMRFrameFormatFSF){
                    pComponentPrivate->frameMode = WBAMRENC_MIMEMODE;
                }
                else {
                    pComponentPrivate->frameMode = WBAMRENC_IF2;
                }
                    
            }
            else {
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadPortIndex from SetParameter",__LINE__);
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;
    case OMX_IndexParamPortDefinition:
        {
            pComponentParamPort = (OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam;
            WBAMRENC_DPRINT("%d :: SetParameter OMX_IndexParamPortDefinition \n",__LINE__);
            if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
               pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->nPortIndex) {
                WBAMRENC_DPRINT("%d :: SetParameter OMX_IndexParamPortDefinition \n",__LINE__);
                memcpy(pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT], pCompParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            }
            else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
                    pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->nPortIndex) {
                WBAMRENC_DPRINT("%d :: SetParameter OMX_IndexParamPortDefinition \n",__LINE__);
                memcpy(pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT], pCompParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            }
            else {
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadPortIndex from SetParameter",__LINE__);
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;
    case OMX_IndexParamPriorityMgmt:
        {
            WBAMRENC_DPRINT("%d :: SetParameter OMX_IndexParamPriorityMgmt \n",__LINE__);
            memcpy(pComponentPrivate->sPriorityMgmt, (OMX_PRIORITYMGMTTYPE*)pCompParam, sizeof(OMX_PRIORITYMGMTTYPE));
        }
        break;

    case OMX_IndexParamAudioInit:
        {
            WBAMRENC_DPRINT("%d :: SetParameter OMX_IndexParamAudioInit \n",__LINE__);
            memcpy(pComponentPrivate->sPortParam, (OMX_PORT_PARAM_TYPE*)pCompParam, sizeof(OMX_PORT_PARAM_TYPE));
        }
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
            amr_ip = (OMX_AUDIO_PARAM_PCMMODETYPE *)pCompParam;
            memcpy(pComponentPrivate->pcmParams, amr_ip, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }
        else{
            eError = OMX_ErrorBadParameter;
        }
        break;

    case OMX_IndexParamCompBufferSupplier:             
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
           pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->nPortIndex) {
            WBAMRENC_DPRINT(":: SetParameter OMX_IndexParamCompBufferSupplier \n");
            sBufferSupplier.eBufferSupplier = OMX_BufferSupplyInput;
            memcpy(&sBufferSupplier, pCompParam, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));                                
                    
        }
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
                pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->nPortIndex) {
            WBAMRENC_DPRINT(":: SetParameter OMX_IndexParamCompBufferSupplier \n");
            sBufferSupplier.eBufferSupplier = OMX_BufferSupplyOutput;
            memcpy(&sBufferSupplier, pCompParam, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
        } 
        else {
            WBAMRENC_DPRINT(":: OMX_ErrorBadPortIndex from SetParameter");
            eError = OMX_ErrorBadPortIndex;
        }
        break;


    default:
        {
            WBAMRENC_EPRINT("%d :: SetParameter OMX_ErrorUnsupportedIndex \n",__LINE__);
            eError = OMX_ErrorUnsupportedIndex;
        }
        break;
    }
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting SetParameter\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate =
        (WBAMRENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    TI_OMX_STREAM_INFO *streamInfo;
    WBAMRENC_OMX_MALLOC(streamInfo, TI_OMX_STREAM_INFO);

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        OMX_WBMEMFREE_STRUCT(streamInfo);
        goto EXIT;
    }   
#endif  
    if(nConfigIndex == OMX_IndexCustomWbAmrEncStreamIDConfig){
        streamInfo->streamId = pComponentPrivate->streamID;
        memcpy(ComponentConfigStructure,streamInfo,sizeof(TI_OMX_STREAM_INFO));
    }
    OMX_WBMEMFREE_STRUCT(streamInfo);
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting GetConfig. Returning = 0x%x\n",__LINE__,eError);
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
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate =
        (WBAMRENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    OMX_S16 *customFlag = NULL;
    TI_OMX_DSP_DEFINITION *configData;
    
    TI_OMX_DATAPATH dataPath;
    OMX_AUDIO_CONFIG_VOLUMETYPE *pGainStructure = NULL;
#ifdef DSP_RENDERING_ON
    AM_COMMANDDATATYPE cmd_data;
#endif
    
    WBAMRENC_DPRINT("%d :: Entering SetConfig\n", __LINE__);
    if (pHandle == NULL) {
        WBAMRENC_EPRINT ("%d :: Invalid HANDLE OMX_ErrorBadParameter \n",__LINE__);
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }   
#endif
    switch (nConfigIndex) {
    case OMX_IndexCustomWbAmrEncHeaderInfoConfig:
        {                 
            WBAMRENC_DPRINT("%d :: SetConfig OMX_IndexCustomWbAmrEncHeaderInfoConfig \n",__LINE__);
            configData = (TI_OMX_DSP_DEFINITION*)ComponentConfigStructure;
            if (configData == NULL) {
                eError = OMX_ErrorBadParameter;
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadParameter from SetConfig\n",__LINE__);
                goto EXIT;
            }
            pComponentPrivate->acdnMode = configData->acousticMode;
            pComponentPrivate->dasfMode = configData->dasfMode;     
            if( 2 == pComponentPrivate->dasfMode ){
                pComponentPrivate->dasfMode--;
            }
            pComponentPrivate->streamID = configData->streamId;
            break;
        }
    case OMX_WbIndexCustomModeAmrConfig:
        {
            WBAMRENC_DPRINT("%d :: SetConfig OMX_IndexCustomModeAmrConfig \n",__LINE__);
            customFlag = (OMX_S16*)ComponentConfigStructure;
            if (customFlag == NULL) {
                eError = OMX_ErrorBadParameter;
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadParameter from SetConfig\n",__LINE__);
                goto EXIT;
            }
            pComponentPrivate->amrMode = *customFlag;
            break;
        }
    
    case OMX_WbIndexCustomModeDasfConfig:
        {
            WBAMRENC_DPRINT("%d :: SetConfig OMX_IndexCustomModeDasfConfig \n",__LINE__);
            customFlag = (OMX_S16*)ComponentConfigStructure;
            if (customFlag == NULL) {
                eError = OMX_ErrorBadParameter;
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadParameter from SetConfig\n",__LINE__);
                goto EXIT;
            }
            pComponentPrivate->dasfMode = *customFlag;
            if( 2 == pComponentPrivate->dasfMode ){
                pComponentPrivate->dasfMode--;
            }           
            break;
        }
    case OMX_WbIndexCustomModeMimeConfig:
        {
            WBAMRENC_DPRINT("%d :: SetConfig OMX_IndexCustomModeMimeConfig \n",__LINE__);
            customFlag = (OMX_S16*)ComponentConfigStructure;
            if (customFlag == NULL) {
                eError = OMX_ErrorBadParameter;
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadParameter from SetConfig\n",__LINE__);
                goto EXIT;
            }
            pComponentPrivate->frameMode = *customFlag;
            break;
        }
    case OMX_WbIndexCustomModeMultiFrameConfig:
        {
            WBAMRENC_DPRINT("%d :: SetConfig OMX_WbIndexCustomModeMultiFrameConfig \n",__LINE__);
            customFlag = (OMX_S16*)ComponentConfigStructure;
            if (customFlag == NULL) {
                eError = OMX_ErrorBadParameter;
                WBAMRENC_EPRINT("%d :: OMX_ErrorBadParameter from SetConfig\n",__LINE__);
                goto EXIT;
            }
            pComponentPrivate->nMultiFrameMode = *customFlag;
            break;
        }
        
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
                WBAMRENC_EPRINT("[WBAMRENC encoder] - fail to send command to audio manager\n");
            }
        else
            {
                WBAMRENC_DPRINT("[WBAMRENC encoder] - ok to send command to audio manager\n");
            }
#endif
        break;      
              
    case  OMX_WbIndexCustomDataPath:
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
        
    default:
        eError = OMX_ErrorUnsupportedIndex;
        break;
    }
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting SetConfig\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    WBAMRENC_DPRINT("%d :: Entering GetState\n", __LINE__);
    if (!pState) {
        eError = OMX_ErrorBadParameter;
        WBAMRENC_EPRINT("%d :: OMX_ErrorBadParameter from GetState\n",__LINE__);
        goto EXIT;
    }

    if (pHandle && pHandle->pComponentPrivate) {
        *pState =  ((WBAMRENC_COMPONENT_PRIVATE*)
                    pHandle->pComponentPrivate)->curState;
    } else {
        *pState = OMX_StateLoaded;
    }
    eError = OMX_ErrorNone;

 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting GetState\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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

    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate =
        (WBAMRENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    pPortDef = ((WBAMRENC_COMPONENT_PRIVATE*)pComponentPrivate)->pPortDef[WBAMRENC_INPUT_PORT];
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

    WBAMRENC_DPRINT("%d :: Entering EmptyThisBuffer\n", __LINE__);
    if (pBuffer == NULL) {
        eError = OMX_ErrorBadParameter;
        WBAMRENC_EPRINT("%d :: About to return OMX_ErrorBadParameter\n",__LINE__);
        goto EXIT;
    }

    if (pBuffer->nSize != sizeof(OMX_BUFFERHEADERTYPE)) {
        eError = OMX_ErrorBadParameter;
        WBAMRENC_EPRINT("%d :: About to return OMX_ErrorBadParameter\n",__LINE__);
        goto EXIT;
    }

    if (!pPortDef->bEnabled) {
        eError  = OMX_ErrorIncorrectStateOperation;
        WBAMRENC_EPRINT("%d :: About to return OMX_ErrorIncorrectStateOperation\n",__LINE__);
        goto EXIT;
    }

    if (pBuffer->nVersion.nVersion != pComponentPrivate->nVersion) {
        eError = OMX_ErrorVersionMismatch;
        WBAMRENC_EPRINT("%d :: About to return OMX_ErrorVersionMismatch\n",__LINE__);
        goto EXIT;
    }

    if (pBuffer->nInputPortIndex != WBAMRENC_INPUT_PORT) {
        eError  = OMX_ErrorBadPortIndex;
        WBAMRENC_EPRINT("%d :: About to return OMX_ErrorBadPortIndex\n",__LINE__);
        goto EXIT;
    }

    if (pComponentPrivate->curState != OMX_StateExecuting && pComponentPrivate->curState != OMX_StatePause) {
        eError= OMX_ErrorIncorrectStateOperation;
        WBAMRENC_DPRINT("%d :: About to return OMX_ErrorIncorrectStateOperation\n",__LINE__);
        goto EXIT;
    }


    WBAMRENC_DPRINT("----------------------------------------------------------------\n");
    WBAMRENC_DPRINT("%d :: Comp Sending Filled ip buff = %p to CompThread\n",__LINE__,pBuffer);
    WBAMRENC_DPRINT("----------------------------------------------------------------\n");

    pComponentPrivate->app_nBuf--;
    pComponentPrivate->ProcessingInputBuf++;
    pComponentPrivate->pMarkData = pBuffer->pMarkData;
    pComponentPrivate->hMarkTargetComponent = pBuffer->hMarkTargetComponent;
    ret = write (pComponentPrivate->dataPipe[1], &pBuffer, sizeof(OMX_BUFFERHEADERTYPE*));
    if (ret == -1) {
        WBAMRENC_EPRINT("%d :: Error in Writing to the Data pipe\n", __LINE__);
        eError = OMX_ErrorHardware;
        goto EXIT;
    }
    pComponentPrivate->nEmptyThisBufferCount++;
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting EmptyThisBuffer\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate =
        (WBAMRENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    pPortDef = ((WBAMRENC_COMPONENT_PRIVATE*)pComponentPrivate)->pPortDef[WBAMRENC_OUTPUT_PORT];

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
    
    WBAMRENC_DPRINT("%d :: Entering FillThisBuffer\n", __LINE__);
    WBAMRENC_DPRINT("------------------------------------------------------------------\n");
    WBAMRENC_DPRINT("%d :: Comp Sending Emptied op buff = %p to CompThread\n",__LINE__,pBuffer);
    WBAMRENC_DPRINT("------------------------------------------------------------------\n");
    if (pBuffer == NULL) {
        eError = OMX_ErrorBadParameter;
        WBAMRENC_EPRINT(" %d :: About to return OMX_ErrorBadParameter\n",__LINE__);
        goto EXIT;
    }

    if (pBuffer->nSize != sizeof(OMX_BUFFERHEADERTYPE)) {
        eError = OMX_ErrorBadParameter;
        WBAMRENC_DPRINT(" %d :: About to return OMX_ErrorBadParameter\n",__LINE__);
        goto EXIT;
    }

    if (!pPortDef->bEnabled) {
        eError  = OMX_ErrorIncorrectStateOperation;
        WBAMRENC_EPRINT("%d :: About to return OMX_ErrorIncorrectStateOperation\n",__LINE__);
        goto EXIT;
    }

    if (pBuffer->nVersion.nVersion != pComponentPrivate->nVersion) {
        eError = OMX_ErrorVersionMismatch;
        WBAMRENC_EPRINT(" %d :: About to return OMX_ErrorVersionMismatch\n",__LINE__);
        goto EXIT;
    }

    if (pBuffer->nOutputPortIndex != WBAMRENC_OUTPUT_PORT) {
        eError  = OMX_ErrorBadPortIndex;
        WBAMRENC_EPRINT(" %d :: About to return OMX_ErrorBadPortIndex\n",__LINE__);
        goto EXIT;
    }

    if(pComponentPrivate->curState != OMX_StateExecuting && pComponentPrivate->curState != OMX_StatePause) {
        eError = OMX_ErrorIncorrectStateOperation;
        WBAMRENC_EPRINT("%d :: About to return OMX_ErrorIncorrectStateOperation\n",__LINE__);
        goto EXIT;
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
    pComponentPrivate->ProcessingOutputBuf++;
    ret = write (pComponentPrivate->dataPipe[1], &pBuffer, sizeof (OMX_BUFFERHEADERTYPE*));
    if (ret == -1) {
        WBAMRENC_EPRINT("%d :: Error in Writing to the Data pipe\n", __LINE__);
        eError = OMX_ErrorHardware;
        goto EXIT;
    }
    pComponentPrivate->nFillThisBufferCount++;
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting FillThisBuffer\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate =
        (WBAMRENC_COMPONENT_PRIVATE *)pComponent->pComponentPrivate;
    WBAMRENC_DPRINT("%d :: Entering ComponentDeInit\n", __LINE__);

#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(pComponentPrivate->pPERF,
                  PERF_BoundaryStart | PERF_BoundaryCleanup);
#endif  
    
#ifdef DSP_RENDERING_ON
    close(pComponentPrivate->fdwrite);
    close(pComponentPrivate->fdread);
#endif
#ifdef RESOURCE_MANAGER_ENABLED
    eError = RMProxy_NewSendCommand(pHandle, RMProxy_FreeResource, OMX_WBAMR_Encoder_COMPONENT, 0, 3456, NULL);

    if (eError != OMX_ErrorNone) {
        WBAMRENC_EPRINT ("%d ::Error returned from destroy ResourceManagerProxy thread\n",
                         __LINE__);
    }

    eError = RMProxy_Deinitalize();
    if (eError != OMX_ErrorNone) {
        WBAMRENC_DPRINT("%d :: Error from RMProxy_Deinitalize\n",__LINE__);
        goto EXIT;
    }
#endif
    
    pComponentPrivate->bIsThreadstop = 1;
    eError = WBAMRENC_StopComponentThread(pHandle);
    if (eError != OMX_ErrorNone) {
        WBAMRENC_EPRINT("%d :: Error from WBAMRENC_StopComponentThread\n",__LINE__);
        goto EXIT;
    }
    /* Wait for thread to exit so we can get the status into "eError" */
    /* close the pipe handles */
    eError = WBAMRENC_FreeCompResources(pHandle);
    if (eError != OMX_ErrorNone) {
        WBAMRENC_EPRINT("%d :: Error from WBAMRENC_FreeCompResources\n",__LINE__);
        goto EXIT;
    }
    
#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(pComponentPrivate->pPERF,
                  PERF_BoundaryComplete | PERF_BoundaryCleanup);
    PERF_Done(pComponentPrivate->pPERF);
#endif  

    if (pComponentPrivate->sDeviceString != NULL) {
        newfree(pComponentPrivate->sDeviceString);
    }

    OMX_WBMEMFREE_STRUCT(pComponentPrivate);
    
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting ComponentDeInit\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    WBAMRENC_DPRINT("%d :: Entering ComponentTunnelRequest\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Exiting ComponentTunnelRequest\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHeader;

    pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    pPortDef = ((WBAMRENC_COMPONENT_PRIVATE*)pComponentPrivate)->pPortDef[nPortIndex];
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }   
#endif    
    WBAMRENC_DPRINT("%d :: Entering AllocateBuffer\n", __LINE__);
    WBAMRENC_DPRINT("%d :: pPortDef = %p\n", __LINE__,pPortDef);
    WBAMRENC_DPRINT("%d :: pPortDef->bEnabled = %d\n", __LINE__,pPortDef->bEnabled);

    while (!pPortDef->bEnabled) {
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

    WBAMRENC_OMX_MALLOC(pBufferHeader, OMX_BUFFERHEADERTYPE);
    WBAMRENC_OMX_MALLOC_SIZE(pBufferHeader->pBuffer, nSizeBytes + 256,OMX_U8);

    if (pBufferHeader->pBuffer == NULL) {
        /* Free previously allocated memory before bailing */
        if (pBufferHeader) {
            newfree(pBufferHeader);
            pBufferHeader = NULL;
        }
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pBufferHeader->pBuffer += 128;

    if (nPortIndex == WBAMRENC_INPUT_PORT) {
        pBufferHeader->nInputPortIndex = nPortIndex;
        pBufferHeader->nOutputPortIndex = -1;
        pComponentPrivate->pInputBufferList->pBufHdr[pComponentPrivate->pInputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pInputBufferList->bBufferPending[pComponentPrivate->pInputBufferList->numBuffers] = 0;
        pComponentPrivate->pInputBufferList->bufferOwner[pComponentPrivate->pInputBufferList->numBuffers++] = 1;
        if (pComponentPrivate->pInputBufferList->numBuffers == pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
            WBAMRENC_DPRINT("%d :: pPortDef->bPopulated = %d\n", __LINE__, pPortDef->bPopulated);
        }
    }
    else if (nPortIndex == WBAMRENC_OUTPUT_PORT) {
        pBufferHeader->nInputPortIndex = -1;
        pBufferHeader->nOutputPortIndex = nPortIndex;
        pComponentPrivate->pOutputBufferList->pBufHdr[pComponentPrivate->pOutputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pOutputBufferList->bBufferPending[pComponentPrivate->pOutputBufferList->numBuffers] = 0;
        pComponentPrivate->pOutputBufferList->bufferOwner[pComponentPrivate->pOutputBufferList->numBuffers++] = 1;

        if (pComponentPrivate->pOutputBufferList->numBuffers == pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
            WBAMRENC_DPRINT("%d :: pPortDef->bPopulated = %d\n", __LINE__, pPortDef->bPopulated);
        }
    }
    else {
        eError = OMX_ErrorBadPortIndex;
        WBAMRENC_EPRINT(" %d :: About to return OMX_ErrorBadPortIndex\n",__LINE__);
        goto EXIT;
    }

    if (((pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated == 
          pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled)&&
         (pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated == 
          pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bEnabled) &&
         (pComponentPrivate->InLoaded_readytoidle)))
        {
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
    pBufferHeader->nVersion.s.nVersionMajor = WBAMRENC_MAJOR_VER;
    pBufferHeader->nVersion.s.nVersionMinor = WBAMRENC_MINOR_VER;
    pComponentPrivate->nVersion = pBufferHeader->nVersion.nVersion;
    pBufferHeader->nSize = sizeof(OMX_BUFFERHEADERTYPE);
    *pBuffer = pBufferHeader;
    
    

    if (pComponentPrivate->bEnableCommandPending && pPortDef->bPopulated) {
        SendCommand (pComponentPrivate->pHandle,
                     OMX_CommandPortEnable,
                     pComponentPrivate->bEnableCommandParam,NULL);
    }

 EXIT:

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedBuffer(pComponentPrivate->pPERF,
                        (*pBuffer)->pBuffer, nSizeBytes,
                        PERF_ModuleMemory);
#endif 

    WBAMRENC_DPRINT("%d :: Exiting AllocateBuffer\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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

static OMX_ERRORTYPE FreeBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
                                OMX_IN  OMX_U32 nPortIndex,
                                OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    WBAMRENC_COMPONENT_PRIVATE * pComponentPrivate = NULL;
    OMX_BUFFERHEADERTYPE* buff;
    OMX_U8* tempBuff;
    int i = 0;
    int inputIndex = -1;
    int outputIndex = -1;
    OMX_COMPONENTTYPE *pHandle;

    pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;

    WBAMRENC_DPRINT("%d :: Entering FreeBuffer\n", __LINE__);
    for (i=0; i < WBAMRENC_MAX_NUM_OF_BUFS; i++) {
        buff = pComponentPrivate->pInputBufferList->pBufHdr[i];
        if (buff == pBuffer) {
            WBAMRENC_DPRINT("%d :: Found matching input buffer\n",__LINE__);
            WBAMRENC_DPRINT("%d :: buff = %p\n",__LINE__,buff);
            WBAMRENC_DPRINT("%d :: pBuffer = %p\n",__LINE__,pBuffer);
            inputIndex = i;
            break;
        }
        else {
            WBAMRENC_DPRINT("%d :: This is not a match\n",__LINE__);
            WBAMRENC_DPRINT("%d :: buff = %p\n",__LINE__,buff);
            WBAMRENC_DPRINT("%d :: pBuffer = %p\n",__LINE__,pBuffer);
        }
    }

    for (i=0; i < WBAMRENC_MAX_NUM_OF_BUFS; i++) {
        buff = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        if (buff == pBuffer) {
            WBAMRENC_DPRINT("%d :: Found matching output buffer\n",__LINE__);
            WBAMRENC_DPRINT("%d :: buff = %p\n",__LINE__,buff);
            WBAMRENC_DPRINT("%d :: pBuffer = %p\n",__LINE__,pBuffer);
            outputIndex = i;
            break;
        }
        else {
            WBAMRENC_DPRINT("%d :: This is not a match\n",__LINE__);
            WBAMRENC_DPRINT("%d :: buff = %p\n",__LINE__,buff);
            WBAMRENC_DPRINT("%d :: pBuffer = %p\n",__LINE__,pBuffer);
        }
    }


    if (inputIndex != -1) {
        if (pComponentPrivate->pInputBufferList->bufferOwner[inputIndex] == 1) {
            tempBuff = pComponentPrivate->pInputBufferList->pBufHdr[inputIndex]->pBuffer;
            if (tempBuff != 0){
                tempBuff -= 128;
            }
            OMX_WBMEMFREE_STRUCT(tempBuff);
        }

#ifdef __PERF_INSTRUMENTATION__
        PERF_SendingBuffer(pComponentPrivate->pPERF,
                           pComponentPrivate->pInputBufferList->pBufHdr[inputIndex]->pBuffer, 
                           pComponentPrivate->pInputBufferList->pBufHdr[inputIndex]->nAllocLen,
                           PERF_ModuleMemory );
#endif
        OMX_WBMEMFREE_STRUCT(pComponentPrivate->pInputBufferList->pBufHdr[inputIndex]);

        pComponentPrivate->pInputBufferList->numBuffers--;
        if (pComponentPrivate->pInputBufferList->numBuffers <
            pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->nBufferCountMin) {

            pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated = OMX_FALSE;
        }
        if(pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bEnabled &&
           pComponentPrivate->bLoadedCommandPending == OMX_FALSE &&     
           (pComponentPrivate->curState == OMX_StateIdle ||
            pComponentPrivate->curState == OMX_StateExecuting ||
            pComponentPrivate->curState == OMX_StatePause)) {
            pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventError,
                                                    OMX_ErrorPortUnpopulated,
                                                    OMX_TI_ErrorMinor,
                                                    "Input Port Unpopulated");
        }
    }
    else if (outputIndex != -1) {
        if (pComponentPrivate->pOutputBufferList->bufferOwner[outputIndex] == 1) {
            tempBuff = pComponentPrivate->pOutputBufferList->pBufHdr[outputIndex]->pBuffer;
            if (tempBuff != 0){
                tempBuff -= 128;
            }
            OMX_WBMEMFREE_STRUCT(tempBuff);
        }
#ifdef __PERF_INSTRUMENTATION__
        PERF_SendingBuffer(pComponentPrivate->pPERF,
                           pComponentPrivate->pOutputBufferList->pBufHdr[outputIndex]->pBuffer, 
                           pComponentPrivate->pOutputBufferList->pBufHdr[outputIndex]->nAllocLen,
                           PERF_ModuleMemory );
#endif      
        OMX_WBMEMFREE_STRUCT(pComponentPrivate->pOutputBufferList->pBufHdr[outputIndex]);

        pComponentPrivate->pOutputBufferList->numBuffers--;
        if (pComponentPrivate->pOutputBufferList->numBuffers <
            pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->nBufferCountMin) {
            pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated = OMX_FALSE;
        }
        if(pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled &&
           pComponentPrivate->bLoadedCommandPending == OMX_FALSE &&     
           (pComponentPrivate->curState == OMX_StateIdle ||
            pComponentPrivate->curState == OMX_StateExecuting ||
            pComponentPrivate->curState == OMX_StatePause)) {
            pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventError,
                                                    OMX_ErrorPortUnpopulated,
                                                    OMX_TI_ErrorMinor,
                                                    "Output Port Unpopulated");
        }
    }
    else {
        WBAMRENC_EPRINT("%d :: Returning OMX_ErrorBadParameter\n",__LINE__);
        eError = OMX_ErrorBadParameter;
    }

    WBAMRENC_DPRINT("num input buffers %d, \n num output buffers %d,\n InIdle_goingtoloaded %d\n",
                    (int)pComponentPrivate->pInputBufferList->numBuffers, (int)pComponentPrivate->pOutputBufferList->numBuffers, pComponentPrivate->InIdle_goingtoloaded);

    if ((!pComponentPrivate->pInputBufferList->numBuffers &&
         !pComponentPrivate->pOutputBufferList->numBuffers) &&
        pComponentPrivate->InIdle_goingtoloaded)
        {
    
            pComponentPrivate->InIdle_goingtoloaded = 0;                  
#ifndef UNDER_CE
            pthread_mutex_lock(&pComponentPrivate->InIdle_mutex);
            pthread_cond_signal(&pComponentPrivate->InIdle_threshold);
            pthread_mutex_unlock(&pComponentPrivate->InIdle_mutex);
#else
            OMX_SignalEvent(&(pComponentPrivate->InIdle_event));
#endif
        }

    if (pComponentPrivate->bDisableCommandPending && 
        (pComponentPrivate->pInputBufferList->numBuffers + 
         pComponentPrivate->pOutputBufferList->numBuffers == 0)) {
        
        SendCommand (pComponentPrivate->pHandle,
                     OMX_CommandPortDisable,
                     pComponentPrivate->bDisableCommandParam,
                     NULL);
    }

    WBAMRENC_DPRINT("%d :: Exiting FreeBuffer\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHeader;

    pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
        goto EXIT;
    }
    
#endif
    pPortDef = ((WBAMRENC_COMPONENT_PRIVATE*)pComponentPrivate)->pPortDef[nPortIndex];
    WBAMRENC_DPRINT("%d :: Entering UseBuffer\n", __LINE__);
    WBAMRENC_DPRINT("%d :: pPortDef->bPopulated = %d \n",__LINE__,pPortDef->bPopulated);

    if(!pPortDef->bEnabled) {
        WBAMRENC_EPRINT("%d :: About to return OMX_ErrorIncorrectStateOperation\n",__LINE__);
        eError = OMX_ErrorIncorrectStateOperation;
        goto EXIT;
    }


    WBAMRENC_OMX_MALLOC(pBufferHeader, OMX_BUFFERHEADERTYPE);

    if (nPortIndex == WBAMRENC_OUTPUT_PORT) {
        pBufferHeader->nInputPortIndex = -1;
        pBufferHeader->nOutputPortIndex = nPortIndex;
        pComponentPrivate->pOutputBufferList->pBufHdr[pComponentPrivate->pOutputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pOutputBufferList->bBufferPending[pComponentPrivate->pOutputBufferList->numBuffers] = 0;
        pComponentPrivate->pOutputBufferList->bufferOwner[pComponentPrivate->pOutputBufferList->numBuffers++] = 0;

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
         ((pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled)&&
          (pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bEnabled) &&
          (pComponentPrivate->InLoaded_readytoidle)))/*File Mode*/  
        || 
        ((pComponentPrivate->dasfMode) &&
         ((pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled)&&
          (pComponentPrivate->InLoaded_readytoidle))))/*Dasf Mode*/
        {
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
    pBufferHeader->nVersion.s.nVersionMajor = WBAMRENC_MAJOR_VER;
    pBufferHeader->nVersion.s.nVersionMinor = WBAMRENC_MINOR_VER;
    pComponentPrivate->nVersion = pBufferHeader->nVersion.nVersion;
    pBufferHeader->pBuffer = pBuffer;
    pBufferHeader->nSize = sizeof(OMX_BUFFERHEADERTYPE);
    *ppBufferHdr = pBufferHeader;

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedBuffer(pComponentPrivate->pPERF,
                        pBuffer, nSizeBytes,
                        PERF_ModuleHLMM);
#endif
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting UseBuffer\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    if(!(strcmp(cParameterName,"OMX.TI.index.config.wbamrheaderinfo"))) {
        *pIndexType = OMX_IndexCustomWbAmrEncHeaderInfoConfig;
    }
    else if(!(strcmp(cParameterName,"OMX.TI.index.config.wbamrstreamIDinfo"))) {
        *pIndexType = OMX_IndexCustomWbAmrEncStreamIDConfig;
    }
    else if(!(strcmp(cParameterName,"OMX.TI.index.config.wbamr.datapath"))) 
        {                 
            *pIndexType = OMX_WbIndexCustomDataPath;
        }    
    else {
        eError = OMX_ErrorBadParameter;
    }

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
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate;
    
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if(nIndex == 0){
        memcpy(cRole, &pComponentPrivate->componentRole.cRole, sizeof(OMX_U8) * OMX_MAX_STRINGNAME_SIZE); 
        WBAMRENC_DPRINT("::::In ComponenetRoleEnum: cRole is set to %s\n",cRole);
    }
    else {
        eError = OMX_ErrorNoMore;
    }
    return eError;
}

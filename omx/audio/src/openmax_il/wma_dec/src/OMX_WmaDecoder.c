
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

/* ==============================================================================
 *             Texas Instruments OMAP (TM) Platform Software
 *  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
 *
 *  Use of this software is controlled by the terms and conditions found
 *  in the license agreement under which this software has been supplied.
 * ============================================================================ */
/**
 * @file OMX_WmaDecoder.c
 *
 * This file implements OMX Component for WMA decoder that
 * is fully compliant with the OMX Audio specification 1.5.
 *
 * @path  $(CSLPATH)\
 *
 * @rev  0.1
 */
/* ----------------------------------------------------------------------------
 *!
 *! Revision History
 *! ===================================
 *! 12-Sept-2005 mf:  Initial Version. Change required per OMAPSWxxxxxxxxx
 *! to provide _________________.
 *!
 * ============================================================================= */


/* ------compilation control switches -------------------------*/
/****************************************************************
 *  INCLUDE FILES
 ****************************************************************/
/* ----- system and platform files ----------------------------*/
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

/*-------program files ----------------------------------------*/
#include "OMX_WmaDec_Utils.h"
/* interface with audio manager */
#define FIFO1 "/dev/fifo.1"
#define FIFO2 "/dev/fifo.2"

#ifdef DSP_RENDERING_ON
AM_COMMANDDATATYPE cmd_data;
int fdwrite, fdread;
int errno;
OMX_U32 streamID;
#endif
#define WMA_DEC_ROLE "audio_decoder.wma"

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

static OMX_ERRORTYPE SetCallbacks (OMX_HANDLETYPE hComp,
                                   OMX_CALLBACKTYPE* pCallBacks, OMX_PTR pAppData);
static OMX_ERRORTYPE GetComponentVersion (OMX_HANDLETYPE hComp,
                                          OMX_STRING pComponentName,
                                          OMX_VERSIONTYPE* pComponentVersion,
                                          OMX_VERSIONTYPE* pSpecVersion,
                                          OMX_UUIDTYPE* pComponentUUID);

static OMX_ERRORTYPE SendCommand (OMX_HANDLETYPE hComp, OMX_COMMANDTYPE nCommand,
                                  OMX_U32 nParam, OMX_PTR pCmdData);

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

/*-------------------------------------------------------------------*/
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
/*-------------------------------------------------------------------*/
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComp)
{

    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef_ip = NULL, *pPortDef_op = NULL;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_AUDIO_PARAM_WMATYPE *wma_ip = NULL; 
    OMX_AUDIO_PARAM_PCMMODETYPE *wma_op = NULL;
    RCA_HEADER *rcaheader=NULL;
      
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE*) hComp;
    int i;

    OMXDBG_PRINT(stderr, PRINT, 1, 0, "%d ::OMX_ComponentInit\n", __LINE__);

    /*Set the all component function pointer to the handle */
    pHandle->SetCallbacks = SetCallbacks;
    pHandle->GetComponentVersion = GetComponentVersion;
    pHandle->SendCommand = SendCommand;
    pHandle->GetParameter = GetParameter;
    pHandle->SetParameter = SetParameter;
    pHandle->GetConfig = GetConfig;
    pHandle->GetState = GetState;
    pHandle->EmptyThisBuffer = EmptyThisBuffer;
    pHandle->FillThisBuffer = FillThisBuffer;
    pHandle->ComponentTunnelRequest = ComponentTunnelRequest;
    pHandle->ComponentDeInit = ComponentDeInit;
    pHandle->AllocateBuffer = AllocateBuffer;
    pHandle->FreeBuffer = FreeBuffer;
    pHandle->UseBuffer = UseBuffer;
    pHandle->SetConfig = SetConfig;
    pHandle->GetExtensionIndex = GetExtensionIndex;
    pHandle->ComponentRoleEnum = ComponentRoleEnum;
  

 
    /*Allocate the memory for Component private data area */
    OMX_MALLOC_GENERIC(pHandle->pComponentPrivate, WMADEC_COMPONENT_PRIVATE);
    if(NULL==pHandle->pComponentPrivate)
        return OMX_ErrorInsufficientResources;

    ((WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->pHandle = pHandle;

    /* Initialize component data structures to default values */
    ((WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->sPortParam.nPorts = 0x2;
    ((WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->sPortParam.nStartPortNumber = 0x0;

    eError = OMX_ErrorNone;
 
    OMX_MALLOC_GENERIC(wma_ip, OMX_AUDIO_PARAM_WMATYPE);
    if(NULL==wma_ip){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }

    OMX_MALLOC_GENERIC(wma_op, OMX_AUDIO_PARAM_PCMMODETYPE);
    if(NULL==wma_op){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }

    OMX_MALLOC_GENERIC(rcaheader, RCA_HEADER);
    if(NULL==rcaheader){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }

    ((WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->wma_op=wma_op;
    ((WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->rcaheader=rcaheader;

    ((WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->wmaParams[INPUT_PORT] = wma_ip;
    ((WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->wmaParams[OUTPUT_PORT] = (OMX_AUDIO_PARAM_WMATYPE*)wma_op;

 
    pComponentPrivate = pHandle->pComponentPrivate;
    OMX_MALLOC_GENERIC(pComponentPrivate->pInputBufferList, BUFFERLIST);
    if(NULL==pComponentPrivate->pInputBufferList){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }

    OMX_DBG_INIT(pComponentPrivate->dbg, "OMX_DBG_WMADEC");

#ifdef __PERF_INSTRUMENTATION__
    OMX_PRDSP1(pComponentPrivate->dbg, "PERF %d :: OMX_WmaDecoder.c\n", __LINE__);
    pComponentPrivate->pPERF = PERF_Create(PERF_FOURCC('W','M','A','_'),
                                           PERF_ModuleLLMM |
                                           PERF_ModuleAudioDecode);
#endif

#ifdef ANDROID 
    pComponentPrivate->iPVCapabilityFlags.iIsOMXComponentMultiThreaded = OMX_TRUE;
    pComponentPrivate->iPVCapabilityFlags.iOMXComponentNeedsNALStartCode = OMX_FALSE;
    pComponentPrivate->iPVCapabilityFlags.iOMXComponentSupportsExternalOutputBufferAlloc = OMX_FALSE;
    pComponentPrivate->iPVCapabilityFlags.iOMXComponentSupportsExternalInputBufferAlloc = OMX_FALSE;
    pComponentPrivate->iPVCapabilityFlags.iOMXComponentSupportsMovableInputBuffers = OMX_FALSE; 
    pComponentPrivate->iPVCapabilityFlags.iOMXComponentSupportsPartialFrames = OMX_FALSE; 
    pComponentPrivate->iPVCapabilityFlags.iOMXComponentCanHandleIncompleteFrames = OMX_FALSE; 
#endif
    
    pComponentPrivate->pInputBufferList->numBuffers = 0; /* initialize number of buffers */
    pComponentPrivate->bDspStoppedWhileExecuting = OMX_FALSE;
    OMX_MALLOC_GENERIC(pComponentPrivate->pOutputBufferList, BUFFERLIST);
    if(NULL==pComponentPrivate->pOutputBufferList){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }

     pComponentPrivate->pOutputBufferList->numBuffers = 0; /* initialize number of buffers */
    OMX_MALLOC_GENERIC(pComponentPrivate->pHeaderInfo, WMA_HeadInfo);
    if(NULL==pComponentPrivate->pHeaderInfo){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }

    OMX_MALLOC_GENERIC(pComponentPrivate->pDspDefinition, TI_OMX_DSP_DEFINITION);
    if(NULL==pComponentPrivate->pDspDefinition){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }

    /* Temporarily treat these as default values for Khronos tests */
    pComponentPrivate->pHeaderInfo->iPackets.dwLo           =   WMADEC_DEFAULT_PACKETS_DWLO ;
    pComponentPrivate->pHeaderInfo->iPlayDuration.dwHi      =   0   ;
    pComponentPrivate->pHeaderInfo->iPlayDuration.dwLo      =   WMADEC_DEFAULT_PLAYDURATION_DWLO ;
    pComponentPrivate->pHeaderInfo->iMaxPacketSize          =   WMADEC_DEFAULT_MAXPACKETSIZE ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data1       =  WMADEC_DEFAULT_STREAMTYPE_DATA1 ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data2       =   WMADEC_DEFAULT_STREAMTYPE_DATA2 ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data3       =   WMADEC_DEFAULT_STREAMTYPE_DATA3 ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data4[0]    =   WMADEC_DEFAULT_STREAMTYPE_DATA40 ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data4[1]    =   WMADEC_DEFAULT_STREAMTYPE_DATA41 ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data4[2]    =   WMADEC_DEFAULT_STREAMTYPE_DATA42   ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data4[3]    =   WMADEC_DEFAULT_STREAMTYPE_DATA43 ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data4[4]    =   WMADEC_DEFAULT_STREAMTYPE_DATA44  ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data4[5]    =   WMADEC_DEFAULT_STREAMTYPE_DATA45  ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data4[6]    =   WMADEC_DEFAULT_STREAMTYPE_DATA46  ;
    pComponentPrivate->pHeaderInfo->iStreamType.Data4[7]    =   WMADEC_DEFAULT_STREAMTYPE_DATA47  ;
    pComponentPrivate->pHeaderInfo->iTypeSpecific           =   WMADEC_DEFAULT_TYPESPECIFIC  ;
    pComponentPrivate->pHeaderInfo->iStreamNum              =   WMADEC_DEFAULT_STREAMNUM   ;
    pComponentPrivate->pHeaderInfo->iFormatTag              =   WMADEC_DEFAULT_FORMATTAG ;
    pComponentPrivate->pHeaderInfo->iBlockAlign             =   WMADEC_DEFAULT_BLOCKALIGN  ;
    pComponentPrivate->pHeaderInfo->iSamplePerSec           =   WMADEC_DEFAULT_SAMPLEPERSEC;
    pComponentPrivate->pHeaderInfo->iAvgBytesPerSec         =   WMADEC_DEFAULT_AVGBYTESPERSEC ;
    pComponentPrivate->pHeaderInfo->iChannel                =   WMADEC_DEFAULT_CHANNEL ;
    pComponentPrivate->pHeaderInfo->iValidBitsPerSample     =   WMADEC_DEFAULT_VALIDBITSPERSAMPLE  ;
    pComponentPrivate->pHeaderInfo->iSizeWaveHeader         =   WMADEC_DEFAULT_SIZEWAVEHEADER  ;
    pComponentPrivate->pHeaderInfo->iEncodeOptV             =   WMADEC_DEFAULT_ENCODEOPTV   ;
    pComponentPrivate->pHeaderInfo->iValidBitsPerSample     =   WMADEC_DEFAULT_VALIDBITSPERSAMPLE  ;
    pComponentPrivate->pHeaderInfo->iChannelMask            =   WMADEC_DEFAULT_CHANNELMASK   ;
    pComponentPrivate->pHeaderInfo->iSamplePerBlock         =   WMADEC_DEFAULT_SAMPLEPERBLOCK;
    

    pComponentPrivate->bConfigData = 0;  /* assume the first buffer received will contain only config data, need to use bufferFlag instead */
    pComponentPrivate->reconfigInputPort = 0; 
    pComponentPrivate->reconfigOutputPort = 0;
     
    for (i=0; i < MAX_NUM_OF_BUFS; i++) {
        pComponentPrivate->pOutputBufferList->pBufHdr[i] = NULL;
        pComponentPrivate->pInputBufferList->pBufHdr[i] = NULL;
    }
    pComponentPrivate->dasfmode = 0;
    pComponentPrivate->bPortDefsAllocated = 0;
    pComponentPrivate->bCompThreadStarted = 0;
    pComponentPrivate->bInitParamsInitialized = 0;
    pComponentPrivate->pMarkBuf = NULL;
    pComponentPrivate->pMarkData = NULL;
    pComponentPrivate->nEmptyBufferDoneCount = 0;
    pComponentPrivate->nEmptyThisBufferCount = 0;
    pComponentPrivate->nFillBufferDoneCount = 0;
    pComponentPrivate->nFillThisBufferCount = 0;
    pComponentPrivate->strmAttr = NULL;
    pComponentPrivate->bDisableCommandParam = 0;
    pComponentPrivate->bEnableCommandParam = 0;
    
    pComponentPrivate->nUnhandledFillThisBuffers=0;
    pComponentPrivate->nHandledFillThisBuffers=0;
    pComponentPrivate->nUnhandledEmptyThisBuffers = 0;
    pComponentPrivate->nHandledEmptyThisBuffers = 0;
    pComponentPrivate->SendAfterEOS = 0;
    
    pComponentPrivate->bFlushOutputPortCommandPending = OMX_FALSE;
    pComponentPrivate->bFlushInputPortCommandPending = OMX_FALSE;
    pComponentPrivate->DSPMMUFault = OMX_FALSE;

    
    for (i=0; i < MAX_NUM_OF_BUFS; i++) {
        pComponentPrivate->pInputBufHdrPending[i] = NULL;
        pComponentPrivate->pOutputBufHdrPending[i] = NULL;
    }
    pComponentPrivate->nInvalidFrameCount = 0;
    pComponentPrivate->nNumInputBufPending = 0;
    pComponentPrivate->nNumOutputBufPending = 0;
    pComponentPrivate->bDisableCommandPending = 0;
    pComponentPrivate->bEnableCommandPending = 0;
    pComponentPrivate->bBypassDSP = OMX_FALSE;
    pComponentPrivate->bNoIdleOnStop= OMX_FALSE;
    pComponentPrivate->bIdleCommandPending = OMX_FALSE;
    pComponentPrivate->nOutStandingFillDones = 0;
    
    pComponentPrivate->sInPortFormat.eEncoding = OMX_AUDIO_CodingWMA;
    pComponentPrivate->sInPortFormat.nIndex = 0;
    pComponentPrivate->sInPortFormat.nPortIndex = INPUT_PORT;
    pComponentPrivate->bPreempted = OMX_FALSE; 
    
    pComponentPrivate->sOutPortFormat.eEncoding = OMX_AUDIO_CodingPCM;
    pComponentPrivate->sOutPortFormat.nIndex = 1;
    pComponentPrivate->sOutPortFormat.nPortIndex = OUTPUT_PORT;
    
    OMX_MALLOC_SIZE(pComponentPrivate->sDeviceString, 100*sizeof(OMX_STRING), OMX_STRING);
    if(NULL==pComponentPrivate->sDeviceString){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }
    
    /* PCM format defaults */
    wma_op->nPortIndex = 1;
    wma_op->nChannels = 2; 
    wma_op->eNumData= OMX_NumericalDataSigned; 
    wma_op->nBitPerSample = 16;  
    wma_op->nSamplingRate = 44100;          
    wma_op->ePCMMode = OMX_AUDIO_PCMModeLinear; 
    

    /* WMA format defaults */
    wma_ip->nPortIndex = 0;
    wma_ip->nChannels = 2;
    wma_ip->nBitRate = 32000;
    wma_ip->nSamplingRate = 44100;
   
    /* initialize role name */
    strcpy((char *) pComponentPrivate->componentRole.cRole, WMA_DEC_ROLE);
    /* Initialize device string to the default value */
    strcpy((char*)pComponentPrivate->sDeviceString,"/eteedn:i0:o0/codec\0");

    /* Removing sleep() calls. Initialization.*/
    pthread_mutex_init(&pComponentPrivate->AlloBuf_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->AlloBuf_threshold, NULL);
    pComponentPrivate->AlloBuf_waitingsignal = 0;
            
    pthread_mutex_init(&pComponentPrivate->InLoaded_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->InLoaded_threshold, NULL);
    pComponentPrivate->InLoaded_readytoidle = 0;
    
    pthread_mutex_init(&pComponentPrivate->InIdle_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->InIdle_threshold, NULL);
    pComponentPrivate->InIdle_goingtoloaded = 0;

    pthread_mutex_init(&pComponentPrivate->codecStop_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->codecStop_threshold, NULL);
    pComponentPrivate->codecStop_waitingsignal = 0;

    pthread_mutex_init(&pComponentPrivate->codecFlush_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->codecFlush_threshold, NULL);
    pComponentPrivate->codecFlush_waitingsignal = 0;
    /* Removing sleep() calls. Initialization.*/        
    
    OMX_MALLOC_GENERIC(pPortDef_ip, OMX_PARAM_PORTDEFINITIONTYPE);
    if(NULL==pPortDef_ip){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }

    OMX_MALLOC_GENERIC(pPortDef_op,OMX_PARAM_PORTDEFINITIONTYPE );
    if(NULL==pPortDef_ip){
        WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorInsufficientResources;
    }
    OMX_PRCOMM2(pComponentPrivate->dbg, "%d ::pPortDef_ip = %p\n", __LINE__,pPortDef_ip);
    OMX_PRCOMM2(pComponentPrivate->dbg, "%d ::pPortDef_op = %p\n", __LINE__,pPortDef_op);

    ((WMADEC_COMPONENT_PRIVATE*) pHandle->pComponentPrivate)->pPortDef[INPUT_PORT]
        = pPortDef_ip;

    ((WMADEC_COMPONENT_PRIVATE*) pHandle->pComponentPrivate)->pPortDef[OUTPUT_PORT]
        = pPortDef_op;

    pPortDef_ip->nPortIndex = 0x0;
    pPortDef_ip->nBufferCountActual = NUM_WMADEC_INPUT_BUFFERS;
    pPortDef_ip->nBufferCountMin = NUM_WMADEC_INPUT_BUFFERS;
    pPortDef_ip->eDir = OMX_DirInput;
    pPortDef_ip->bEnabled = OMX_TRUE;
    pPortDef_ip->nBufferAlignment = DSP_CACHE_ALIGNMENT;
    pPortDef_ip->nBufferSize = INPUT_WMADEC_BUFFER_SIZE;
    pPortDef_ip->bPopulated = 0;
    pPortDef_ip->eDomain = OMX_PortDomainAudio;
    pPortDef_ip->format.audio.eEncoding = OMX_AUDIO_CodingWMA;
    
    pPortDef_op->nPortIndex = 0x1;
    pPortDef_op->nBufferCountActual = NUM_WMADEC_OUTPUT_BUFFERS;
    pPortDef_op->nBufferCountMin = NUM_WMADEC_OUTPUT_BUFFERS;
    pPortDef_op->nBufferAlignment = DSP_CACHE_ALIGNMENT;
    pPortDef_op->eDir = OMX_DirOutput;
    pPortDef_op->bEnabled = OMX_TRUE;
    if(pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferSize == 0)
    {
        pPortDef_op->nBufferSize = OUTPUT_WMADEC_BUFFER_SIZE;
    }
    else
    {
        pPortDef_op->nBufferSize = pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferSize;
    }
    pPortDef_op->bPopulated = 0;
    pPortDef_op->eDomain = OMX_PortDomainAudio;
    pComponentPrivate->bIsInvalidState = OMX_FALSE;
    pPortDef_op->format.audio.eEncoding = OMX_AUDIO_CodingPCM;

#ifdef RESOURCE_MANAGER_ENABLED
    OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: Initialize RM Proxy... \n", __LINE__);
    eError = RMProxy_NewInitalize();
    OMX_PRINT2(pComponentPrivate->dbg, "%d ::OMX_ComponentInit\n", __LINE__);
    if (eError != OMX_ErrorNone) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d ::Error returned from loading ResourceManagerProxy thread\n",
                       __LINE__);
        WMADEC_FreeCompResources(pHandle);
        return eError;
    }
#endif  
    OMX_PRINT1(pComponentPrivate->dbg, "%d ::Start Component Thread \n", __LINE__);
    eError = WMADEC_StartComponentThread(pHandle);
    /*OMX_PRINT1(pComponentPrivate->dbg, "%d ::OMX_ComponentInit\n", __LINE__);*/
    if (eError != OMX_ErrorNone) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d ::Error returned from the Component\n",__LINE__);
        WMADEC_FreeCompResources(pHandle);
        return eError;
    }
    OMX_PRINT1(pComponentPrivate->dbg, "%d ::OMX_ComponentInit\n", __LINE__);


  
    
#ifdef DSP_RENDERING_ON
    OMX_PRINT1(pComponentPrivate->dbg, "%d ::OMX_ComponentInit\n", __LINE__);
    if((fdwrite=open(FIFO1,O_WRONLY))<0) {
        OMX_TRACE4(pComponentPrivate->dbg, "[WMA Dec Component] - failure to open WRITE pipe\n");
	 WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorHardware;
    }

    OMX_PRINT1(pComponentPrivate->dbg, "%d ::OMX_ComponentInit\n", __LINE__);
    if((fdread=open(FIFO2,O_RDONLY))<0) {
        OMX_TRACE4(pComponentPrivate->dbg, "[WMA Dec Component] - failure to open READ pipe\n");
	 WMADEC_FreeCompResources(pHandle);
        return OMX_ErrorHardware;
    }
    OMX_PRINT1(pComponentPrivate->dbg, "%d ::OMX_ComponentInit\n", __LINE__);

#endif
#ifdef __PERF_INSTRUMENTATION__
    OMX_PRINT1(pComponentPrivate->dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    PERF_ThreadCreated(pComponentPrivate->pPERF, pComponentPrivate->ComponentThread,
                       PERF_FOURCC('W','M','A','T'));
#endif
 EXIT:
    if (eError != OMX_ErrorNone) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d ::Error returned from the Component\n",__LINE__);
        WMADEC_FreeCompResources(pHandle);
        return eError;
    }
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

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE*)pComponent;

    WMADEC_COMPONENT_PRIVATE *pComponentPrivate =
        (WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    if (pCallBacks == NULL) {
        OMX_PRINT1(pComponentPrivate->dbg, "About to return OMX_ErrorBadParameter on line %d\n",__LINE__);
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Received the empty callbacks from the \
                application\n",__LINE__);
        return  OMX_ErrorBadParameter;
    }

    /*Copy the callbacks of the application to the component private */
    memcpy (&(pComponentPrivate->cbInfo), pCallBacks, sizeof(OMX_CALLBACKTYPE));

    /*copy the application private data to component memory */
    pHandle->pApplicationPrivate = pAppData;

    pComponentPrivate->curState = OMX_StateLoaded;
#ifdef __PERF_INSTRUMENTATION__
    OMX_PRDSP1(pComponentPrivate->dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    /*               PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundaryCleanup);*/
#endif

    return OMX_ErrorNone;
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

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE*) hComp;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *) pHandle->pComponentPrivate;
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: curState is  OMX_StateInvalid\n",__LINE__);
        return OMX_ErrorInvalidState;
    }   
#endif

    OMX_PRINT1(pComponentPrivate->dbg, "Inside the GetComponentVersion\n");
    return OMX_ErrorNotImplemented;

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
                                  OMX_U32 nParam,OMX_PTR pCmdData)
{

    int nRet;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)phandle;
    WMADEC_COMPONENT_PRIVATE *pCompPrivate =
        (WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

#ifdef _ERROR_PROPAGATION__
    if (pCompPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pCompPrivate->dbg, "%d :: curState is  OMX_StateInvalid\n",__LINE__);
        return OMX_ErrorInvalidState;
    }   
#else
    OMX_PRINT1(pCompPrivate->dbg, "%d:::Inside SendCommand\n",__LINE__);
    if(pCompPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pCompPrivate->dbg, "%d :: WMADEC: Error Notofication Sent to App\n",__LINE__);
        pCompPrivate->cbInfo.EventHandler (
                                           pHandle, pHandle->pApplicationPrivate,
                                           OMX_EventError, OMX_ErrorInvalidState,0,
                                           "Invalid State");

        return OMX_ErrorInvalidState;
    }
#endif

#ifdef __PERF_INSTRUMENTATION__
    OMX_PRDSP1(pCompPrivate->dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    PERF_SendingCommand(pCompPrivate->pPERF,
                        Cmd,
                        (Cmd == OMX_CommandMarkBuffer) ? ((OMX_U32) pCmdData) : nParam,
                        PERF_ModuleComponent);
#endif

    switch(Cmd) {
    case OMX_CommandStateSet:
        OMX_PRINT1(pCompPrivate->dbg, "%d:::Inside SendCommand\n",__LINE__);
        OMX_PRSTATE2(pCompPrivate->dbg, "%d:::pCompPrivate->curState = %d\n",__LINE__,pCompPrivate->curState);
        if (nParam == OMX_StateLoaded) {
            pCompPrivate->bLoadedCommandPending = OMX_TRUE;
        }
        if(pCompPrivate->curState == OMX_StateLoaded) {
            if((nParam == OMX_StateExecuting) || (nParam == OMX_StatePause)) {
                pCompPrivate->cbInfo.EventHandler (
                                                   pHandle,
                                                   pHandle->pApplicationPrivate,
                                                   OMX_EventError,
                                                   OMX_ErrorIncorrectStateTransition,
                                                   0,
                                                   NULL);
                return OMX_ErrorIncorrectStateTransition;
            }

            if(nParam == OMX_StateInvalid) {
                OMX_PRINT1(pCompPrivate->dbg, "%d:::Inside SendCommand\n",__LINE__);
                pCompPrivate->curState = OMX_StateInvalid;
                pCompPrivate->cbInfo.EventHandler (
                                                   pHandle,
                                                   pHandle->pApplicationPrivate,
                                                   OMX_EventError,
                                                   OMX_ErrorInvalidState,
                                                   0,
                                                   NULL);
                return OMX_ErrorInvalidState;
            }
        }
        break;
    case OMX_CommandFlush:
        if(nParam > 1 && (OMX_S32)nParam != -1) {
            return OMX_ErrorBadPortIndex;
        }

        break;
    case OMX_CommandPortDisable:
        break;
    case OMX_CommandPortEnable:
        break;
    case OMX_CommandMarkBuffer:
        if (nParam > 0) {
            return OMX_ErrorBadPortIndex;
        }
        break;
    default:
        OMX_PRDSP2(pCompPrivate->dbg, "%d :: WMADEC: Command Received Default \
                                                      error\n",__LINE__);
        pCompPrivate->cbInfo.EventHandler (
                                           pHandle, pHandle->pApplicationPrivate,
                                           OMX_EventError,
                                           OMX_ErrorUndefined,0,
                                           "Invalid Command");
        break;

    }

    OMX_PRINT1(pCompPrivate->dbg, "%d:::Inside SendCommand\n",__LINE__);
    nRet = write (pCompPrivate->cmdPipe[1], &Cmd, sizeof(Cmd));
    if (nRet == -1) {
        OMX_PRINT1(pCompPrivate->dbg, "%d:::Inside SendCommand\n",__LINE__);
        return OMX_ErrorInsufficientResources;
    }

    if (Cmd == OMX_CommandMarkBuffer) {
        nRet = write(pCompPrivate->cmdDataPipe[1],&pCmdData,sizeof(OMX_PTR));
    }
    else {
        nRet = write(pCompPrivate->cmdDataPipe[1], &nParam,
                     sizeof(OMX_U32));
    }

    OMX_PRINT1(pCompPrivate->dbg, "%d:::Inside SendCommand\n",__LINE__);
    OMX_PRINT2(pCompPrivate->dbg, "%d:::nRet = %d\n",__LINE__,nRet);
    if (nRet == -1) {
        OMX_ERROR4(pCompPrivate->dbg, "%d:::Inside SendCommand\n",__LINE__);
        return OMX_ErrorInsufficientResources;
    }
    return OMX_ErrorNone;
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
    WMADEC_COMPONENT_PRIVATE  *pComponentPrivate;
    OMX_PARAM_PORTDEFINITIONTYPE *pParameterStructure;
    /*OMX_PARAM_BUFFERSUPPLIERTYPE *pBufferSupplier;*/

    pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
    pParameterStructure = (OMX_PARAM_PORTDEFINITIONTYPE*)ComponentParameterStructure;

    if (pParameterStructure == NULL) {
        return OMX_ErrorBadParameter;

    }
    OMX_PRINT1(pComponentPrivate->dbg, "pParameterStructure = %p\n",pParameterStructure);
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: curState is  OMX_StateInvalid\n",__LINE__);
        return OMX_ErrorInvalidState;
    }   
#else
    if(pComponentPrivate->curState == OMX_StateInvalid) {
        pComponentPrivate->cbInfo.EventHandler(
                                               hComp,
                                               ((OMX_COMPONENTTYPE *)hComp)->pApplicationPrivate,
                                               OMX_EventError,
                                               OMX_ErrorIncorrectStateOperation, 
                                               0,
                                               NULL);
    
    }
#endif
    switch(nParamIndex){
    case OMX_IndexParamAudioInit:
    
        OMX_PRINT1(pComponentPrivate->dbg, "OMX_IndexParamAudioInit\n");
        memcpy(ComponentParameterStructure, &pComponentPrivate->sPortParam, sizeof(OMX_PORT_PARAM_TYPE));
        break;
    case OMX_IndexParamPortDefinition:
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(ComponentParameterStructure))->nPortIndex == 
           pComponentPrivate->pPortDef[INPUT_PORT]->nPortIndex) {
    

            memcpy(ComponentParameterStructure,
                   pComponentPrivate->pPortDef[INPUT_PORT], 
                   sizeof(OMX_PARAM_PORTDEFINITIONTYPE)
                   );
        } 
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(ComponentParameterStructure))->nPortIndex == 
                pComponentPrivate->pPortDef[OUTPUT_PORT]->nPortIndex) {
    

            memcpy(ComponentParameterStructure, 
                   pComponentPrivate->pPortDef[OUTPUT_PORT], 
                   sizeof(OMX_PARAM_PORTDEFINITIONTYPE)
                   );
 
        } 
        else {
            eError = OMX_ErrorBadPortIndex;
        }
        break;

    case OMX_IndexParamAudioPortFormat:
        if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex == 
           pComponentPrivate->pPortDef[INPUT_PORT]->nPortIndex) {

            if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nIndex > 
               pComponentPrivate->sInPortFormat.nIndex) {

                eError = OMX_ErrorNoMore;
            } 
            else {
                memcpy(ComponentParameterStructure, &pComponentPrivate->sInPortFormat, 
                       sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            }
        }
        else if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex == 
                pComponentPrivate->pPortDef[OUTPUT_PORT]->nPortIndex){

            
            if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nIndex > 
               pComponentPrivate->sOutPortFormat.nIndex) {
                
                eError = OMX_ErrorNoMore;
            } 
            else {
                
                memcpy(ComponentParameterStructure, &pComponentPrivate->sOutPortFormat, 
                       sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            }
        } 
        else {
            
            eError = OMX_ErrorBadPortIndex;
        }
        break;

    case OMX_IndexParamPriorityMgmt:
        break;
    case OMX_IndexParamAudioWma:
        OMX_PRINT2(pComponentPrivate->dbg, "%d :: GetParameter OMX_IndexParamAudioWma \n",__LINE__);
        OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: GetParameter nPortIndex %ld\n",__LINE__, ((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex);
        OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: GetParameter wmaParams->nPortIndex %ld\n",__LINE__, pComponentPrivate->wmaParams[INPUT_PORT]->nPortIndex);
        if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex ==
           pComponentPrivate->wmaParams[INPUT_PORT]->nPortIndex) 
        {
            memcpy(ComponentParameterStructure, pComponentPrivate->wmaParams[INPUT_PORT], sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
        } 
        else if(((OMX_AUDIO_PARAM_PORTFORMATTYPE*)(ComponentParameterStructure))->nPortIndex ==
                pComponentPrivate->wmaParams[OUTPUT_PORT]->nPortIndex)
        {
            memcpy(ComponentParameterStructure, pComponentPrivate->wmaParams[OUTPUT_PORT], sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));

        }
        else
        {

            eError = OMX_ErrorBadPortIndex;
        }
        break;
    case OMX_IndexParamAudioPcm:
        memcpy(ComponentParameterStructure, 
               (OMX_AUDIO_PARAM_PCMMODETYPE*)pComponentPrivate->wma_op, 
               sizeof(OMX_AUDIO_PARAM_PCMMODETYPE)
               );                    
        break;
            
    case OMX_IndexParamCompBufferSupplier:
        if(((OMX_PARAM_BUFFERSUPPLIERTYPE *)(ComponentParameterStructure))->nPortIndex == OMX_DirInput) {
            OMX_PRBUFFER2(pComponentPrivate->dbg, ":: GetParameter OMX_IndexParamCompBufferSupplier \n");
            /*  memcpy(ComponentParameterStructure, pBufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE)); */
                    
        }
        else if(((OMX_PARAM_BUFFERSUPPLIERTYPE *)(ComponentParameterStructure))->nPortIndex == OMX_DirOutput) {
            OMX_PRBUFFER2(pComponentPrivate->dbg, ":: GetParameter OMX_IndexParamCompBufferSupplier \n"); 
            /*memcpy(ComponentParameterStructure, pBufferSupplier, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE)); */
        } 
        else {
            OMX_ERROR2(pComponentPrivate->dbg, ":: OMX_ErrorBadPortIndex from GetParameter");
            eError = OMX_ErrorBadPortIndex;
        }
        break;

    case OMX_IndexParamVideoInit:
        break;

    case OMX_IndexParamImageInit:
        break;

    case OMX_IndexParamOtherInit:
        /*#ifdef ANDROID
          OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering OMX_IndexParamVideoInit\n", __LINE__);
          OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering OMX_IndexParamImageInit/OtherInit\n", __LINE__);
          memcpy(ComponentParameterStructure,pComponentPrivate->sPortParam, sizeof(OMX_PORT_PARAM_TYPE));

        
          eError = OMX_ErrorNone;
          #else
          eError = OMX_ErrorUnsupportedIndex;
          #endif*/
        break;

#ifdef ANDROID
    case (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX:
        {
            OMX_PRDSP1(pComponentPrivate->dbg, "Entering PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX::%d\n", __LINE__);
            PV_OMXComponentCapabilityFlagsType* pCap_flags = (PV_OMXComponentCapabilityFlagsType *) ComponentParameterStructure;
            if (NULL == pCap_flags)
            {
                OMX_ERROR4(pComponentPrivate->dbg, "%d :: ERROR PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX\n", __LINE__);
                return OMX_ErrorBadParameter;
            }
            OMX_ERROR2(pComponentPrivate->dbg, "%d :: Copying PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX\n", __LINE__);
            memcpy(pCap_flags, &(pComponentPrivate->iPVCapabilityFlags), sizeof(PV_OMXComponentCapabilityFlagsType));
            eError = OMX_ErrorNone;
        }
        break;
#endif
    default:
        eError = OMX_ErrorUnsupportedIndex;
        break;
    }
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting GetParameter:: %x\n",__LINE__,nParamIndex);
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
    WMADEC_COMPONENT_PRIVATE  *pComponentPrivate;
    OMX_PARAM_PORTDEFINITIONTYPE *pComponentParam = NULL;
    OMX_PARAM_COMPONENTROLETYPE  *pRole;
    OMX_AUDIO_PARAM_PCMMODETYPE *wma_op;
    OMX_PARAM_BUFFERSUPPLIERTYPE sBufferSupplier;       
    pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);



    if (pCompParam == NULL) {
        return OMX_ErrorBadParameter;
    }

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: curState is  OMX_StateInvalid\n",__LINE__);
        return OMX_ErrorInvalidState;
    }   
#endif

    switch(nParamIndex) {
    case OMX_IndexParamAudioPortFormat:
        pComponentParam = (OMX_PARAM_PORTDEFINITIONTYPE *)pCompParam;

        /* 0 means Input port */
        if (pComponentParam->nPortIndex == 0) {
            if (pComponentParam->eDir != OMX_DirInput) {
                OMX_PRBUFFER4(pComponentPrivate->dbg, "%d :: Invalid input buffer Direction\n",__LINE__);
                OMX_ERROR4(pComponentPrivate->dbg, "About to return OMX_ErrorBadParameter on line %d\n",__LINE__);
                return OMX_ErrorBadParameter;
            }
               
        } 
        else if (pComponentParam->nPortIndex == 1) {
            /* 1 means Output port */

            if (pComponentParam->eDir != OMX_DirOutput) {
                OMX_ERROR4(pComponentPrivate->dbg, "About to return OMX_ErrorBadParameter on line %d\n",__LINE__);
                return OMX_ErrorBadParameter;
            }
            pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferSize = pComponentParam->nBufferSize;
        }
        else {
            return OMX_ErrorBadParameter;
        }
        break;
    case OMX_IndexParamAudioWma:
        {
            OMX_AUDIO_PARAM_WMATYPE *pCompWmaParam =
                (OMX_AUDIO_PARAM_WMATYPE *)pCompParam;

            /* 0 means Input port */
            if(pCompWmaParam->nPortIndex == 0) {
                OMX_PRCOMM2(pComponentPrivate->dbg, "pCompWmaParam->nPortIndex == 0\n");
                memcpy(((WMADEC_COMPONENT_PRIVATE*)
                        pHandle->pComponentPrivate)->wmaParams[INPUT_PORT],
                       pCompWmaParam, sizeof(OMX_AUDIO_PARAM_WMATYPE));

            } else if (pCompWmaParam->nPortIndex == 1) {
                pComponentPrivate->wmaParams[OUTPUT_PORT]->nSize = pCompWmaParam->nSize;
                pComponentPrivate->wmaParams[OUTPUT_PORT]->nPortIndex = pCompWmaParam->nPortIndex;
                pComponentPrivate->wmaParams[OUTPUT_PORT]->nBitRate = pCompWmaParam->nBitRate;
                pComponentPrivate->wmaParams[OUTPUT_PORT]->eFormat = pCompWmaParam->eFormat;
            }
            else {
                eError = OMX_ErrorBadPortIndex;
            }
        }
        break;
    case OMX_IndexParamPortDefinition:
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex == 
           pComponentPrivate->pPortDef[INPUT_PORT]->nPortIndex) {

            memcpy(pComponentPrivate->pPortDef[INPUT_PORT], 
                   pCompParam,
                   sizeof(OMX_PARAM_PORTDEFINITIONTYPE)
                   );

        } 
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex == 
                pComponentPrivate->pPortDef[OUTPUT_PORT]->nPortIndex) {

            memcpy(pComponentPrivate->pPortDef[OUTPUT_PORT], 
                   pCompParam, 
                   sizeof(OMX_PARAM_PORTDEFINITIONTYPE)
                   );

        } 
        else {
            eError = OMX_ErrorBadPortIndex;
        }
        break;
    case OMX_IndexParamPriorityMgmt:
        if (pComponentPrivate->curState != OMX_StateLoaded) {
            eError = OMX_ErrorIncorrectStateOperation;
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
            wma_op = (OMX_AUDIO_PARAM_PCMMODETYPE *)pCompParam;
            memcpy(pComponentPrivate->wma_op, wma_op, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }
        else{
            eError = OMX_ErrorBadParameter;
        }
        break;

    case OMX_IndexParamCompBufferSupplier:
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
           pComponentPrivate->pPortDef[INPUT_PORT]->nPortIndex) {
            OMX_PRINT2(pComponentPrivate->dbg, ":: SetParameter OMX_IndexParamCompBufferSupplier \n");
            sBufferSupplier.eBufferSupplier = OMX_BufferSupplyInput;
            memcpy(&sBufferSupplier, pCompParam, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));                                                            
        }
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
                pComponentPrivate->pPortDef[OUTPUT_PORT]->nPortIndex) {
            OMX_PRINT2(pComponentPrivate->dbg, ":: SetParameter OMX_IndexParamCompBufferSupplier \n");
            sBufferSupplier.eBufferSupplier = OMX_BufferSupplyOutput;
            memcpy(&sBufferSupplier, pCompParam, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
        } 
        else {
            OMX_ERROR2(pComponentPrivate->dbg, ":: OMX_ErrorBadPortIndex from SetParameter");
            eError = OMX_ErrorBadPortIndex;
        }                       
        break;
    default:
        break;

    }
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

    WMADEC_COMPONENT_PRIVATE *pComponentPrivate;
    TI_OMX_STREAM_INFO *streamInfo;

    pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);

    OMX_PRINT1(pComponentPrivate->dbg, "Inside   GetConfig\n");

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        return OMX_ErrorInvalidState;
    }   
#endif

    if(nConfigIndex == OMX_IndexCustomWmaDecStreamIDConfig)
    {
        OMX_MALLOC_GENERIC(streamInfo, TI_OMX_STREAM_INFO);
        if(NULL==streamInfo)
            return OMX_ErrorInsufficientResources;

        /* copy component info */
        streamInfo->streamId = pComponentPrivate->streamID;
        memcpy(ComponentConfigStructure,streamInfo,sizeof(TI_OMX_STREAM_INFO));
    } else if (nConfigIndex == OMX_IndexCustomDebug) {
	OMX_DBG_GETCONFIG(pComponentPrivate->dbg, ComponentConfigStructure);
    }

 EXIT:
    OMX_PRINT1(pComponentPrivate->dbg, "Exiting  GetConfig: %x\n",eError);
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
    OMX_COMPONENTTYPE* pHandle = (OMX_COMPONENTTYPE*)hComp;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate;
    WMA_HeadInfo* headerInfo = NULL;
    TI_OMX_DSP_DEFINITION* pDspDefinition = (TI_OMX_DSP_DEFINITION*)ComponentConfigStructure;
    /*int flagValue = 0;*/
    OMX_S16* deviceString;
    TI_OMX_DATAPATH dataPath;    
    OMXDBG_PRINT(stderr, PRINT, 1, 0, "%d :: Entering SetConfig\n", __LINE__);
    if (pHandle == NULL) {
        OMXDBG_PRINT(stderr, ERROR, 4, 0, "%d :: Invalid HANDLE OMX_ErrorBadParameter \n",__LINE__);
        return OMX_ErrorBadParameter;
    }
    pComponentPrivate =  (WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    OMX_PRSTATE1(pComponentPrivate->dbg, "Set Config %d\n",__LINE__);

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: returning curState is StateInvalid \n",__LINE__);
        return OMX_ErrorInvalidState;
    }   
#endif

    switch ((OMX_WMADEC_INDEXAUDIOTYPE)nConfigIndex) {
    case OMX_IndexCustomWMADECHeaderInfoConfig:
        memcpy(pComponentPrivate->pDspDefinition,pDspDefinition,sizeof(TI_OMX_DSP_DEFINITION));
        headerInfo = pDspDefinition->wmaHeaderInfo;
        memcpy(pComponentPrivate->pHeaderInfo,headerInfo,sizeof(WMA_HeadInfo));
        if(pComponentPrivate->pDspDefinition->dasfMode == 0){
            pComponentPrivate->dasfmode = 0;
        }
        else if (pComponentPrivate->pDspDefinition->dasfMode == 1) {
            pComponentPrivate->dasfmode = 1;
        }
        else if(pComponentPrivate->pDspDefinition->dasfMode == 2) {
            pComponentPrivate->dasfmode = 1;
        }
        pComponentPrivate->streamID = pDspDefinition->streamId;
        break;
    case  OMX_IndexCustomWmaDecDataPath:
        deviceString = (OMX_S16*)ComponentConfigStructure;
        if (deviceString == NULL)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: returning ComponentConfigStructure is NULL \n",__LINE__);
            return OMX_ErrorBadParameter;
        }

        dataPath = *deviceString;
        switch(dataPath) 
        {
        case DATAPATH_APPLICATION:
            OMX_MMMIXER_DATAPATH(pComponentPrivate->sDeviceString, RENDERTYPE_DECODER, pComponentPrivate->streamID);
            break;

        case DATAPATH_APPLICATION_RTMIXER:
            strcpy((char*)pComponentPrivate->sDeviceString,(char*)RTM_STRING);
            break;

        case DATAPATH_ACDN:
            strcpy((char*)pComponentPrivate->sDeviceString,(char*)ACDN_STRING);
            break;

        default:
            break;
        }
        break;
    case OMX_IndexCustomDebug: 
	OMX_DBG_SETCONFIG(pComponentPrivate->dbg, ComponentConfigStructure);
	break;
    default:
        return OMX_ErrorUnsupportedIndex;
    }

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting SetConfig Returning = OMX_ErrorNone \n",__LINE__);
    return OMX_ErrorNone;

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

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;

    if (!pState) {
        OMXDBG_PRINT(stderr, ERROR, 4, 0, "About to return OMX_ErrorBadParameter on line %d\n",__LINE__);
        return OMX_ErrorBadParameter;
    }

    if (pHandle && pHandle->pComponentPrivate) {
        *pState =  ((WMADEC_COMPONENT_PRIVATE*)
                    pHandle->pComponentPrivate)->curState;
    } else {
        *pState = OMX_StateLoaded;
    }

    return OMX_ErrorNone;
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
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate =
        (WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    int ret=0;
    pPortDef = ((WMADEC_COMPONENT_PRIVATE*)
                pComponentPrivate)->pPortDef[INPUT_PORT];
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: curState is  OMX_StateInvalid\n",__LINE__);
        return OMX_ErrorInvalidState;
    }   
#endif
#ifdef __PERF_INSTRUMENTATION__
    OMX_PRINT1(pComponentPrivate->dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    PERF_ReceivedFrame(pComponentPrivate->pPERF,
                       pBuffer->pBuffer,
                       pBuffer->nFilledLen,
                       PERF_ModuleHLMM);
#endif
    if(!pPortDef->bEnabled) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Port is not eanbled\n",__LINE__);
        return OMX_ErrorIncorrectStateOperation;
    }   

    if (pBuffer == NULL) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: pBuffer is null \n",__LINE__);
        return OMX_ErrorBadParameter;
    }
    
    if (pBuffer->nSize != sizeof(OMX_BUFFERHEADERTYPE)) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: pBuffer size is not matching with Buffer header size \n",__LINE__);
        return OMX_ErrorBadParameter;
    }    

    if (pBuffer->nVersion.nVersion != pComponentPrivate->nVersion) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Version mismatch \n",__LINE__);
        return OMX_ErrorVersionMismatch;
    }
    
    if (pBuffer->nInputPortIndex != INPUT_PORT) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: BadPortIndex \n",__LINE__);
        return OMX_ErrorBadPortIndex;
    }    

    if(pComponentPrivate->curState != OMX_StateExecuting && pComponentPrivate->curState != OMX_StatePause
       && pComponentPrivate->curState != OMX_StateIdle) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: State mismatch \n",__LINE__);
        return OMX_ErrorIncorrectStateOperation;
    }

    OMX_PRBUFFER1(pComponentPrivate->dbg, "\n------------------------------------------\n\n");
    OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: Component Sending Filled ip buff %p to Component Thread\n",
                  __LINE__,pBuffer);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "\n------------------------------------------\n\n");

    if (pComponentPrivate->bBypassDSP == 0) {
        pComponentPrivate->app_nBuf--;
    }

    pComponentPrivate->pMarkData = pBuffer->pMarkData;
    pComponentPrivate->hMarkTargetComponent = pBuffer->hMarkTargetComponent;

    ret = write (pComponentPrivate->dataPipe[1], &pBuffer, sizeof(OMX_BUFFERHEADERTYPE*));

    if (ret == -1) {
        OMX_TRACE4(pComponentPrivate->dbg, "%d :: Error in Writing to the Data pipe\n", __LINE__);
        return OMX_ErrorHardware;
    }
    else {
        pComponentPrivate->nUnhandledEmptyThisBuffers++;
        pComponentPrivate->nEmptyThisBufferCount++;
    }

    return OMX_ErrorNone;
}   
/**-------------------------------------------------------------------*
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
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate =
        (WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    int nRet=0;
    
    OMX_PRBUFFER1(pComponentPrivate->dbg, "\n------------------------------------------\n\n");
    OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: Component Sending Emptied op buff %p to Component Thread\n",
                   __LINE__,pBuffer);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "\n------------------------------------------\n\n");

    pPortDef = ((WMADEC_COMPONENT_PRIVATE*) 
                pComponentPrivate)->pPortDef[OUTPUT_PORT];
#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: curState is  OMX_StateInvalid\n",__LINE__);
        return OMX_ErrorInvalidState;
    }   
#endif
#ifdef __PERF_INSTRUMENTATION__
    OMX_PRDSP1(pComponentPrivate->dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    PERF_ReceivedFrame(pComponentPrivate->pPERF,
                       pBuffer->pBuffer,
                       0,
                       PERF_ModuleHLMM);
#endif

    if(!pPortDef->bEnabled) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Port is not eanbled\n",__LINE__);
        return OMX_ErrorIncorrectStateOperation;
    }

    if (pBuffer == NULL) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: pBuffer is null \n",__LINE__);
        return OMX_ErrorBadParameter;
    }

    if (pBuffer->nSize != sizeof(OMX_BUFFERHEADERTYPE)) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: pBuffer size is not matching with Buffer header size \n",__LINE__);
        return OMX_ErrorBadParameter;
    }

    if (pBuffer->nVersion.nVersion != pComponentPrivate->nVersion) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Version mismatch \n",__LINE__);
        return OMX_ErrorVersionMismatch;
    }

    if (pBuffer->nOutputPortIndex != OUTPUT_PORT) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: BadPortIndex \n",__LINE__);
        return OMX_ErrorBadPortIndex;
    }

    if(pComponentPrivate->curState != OMX_StateExecuting && pComponentPrivate->curState != OMX_StatePause) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: State mismatch \n",__LINE__);
        return OMX_ErrorIncorrectStateOperation;
    }

    OMX_PRBUFFER1(pComponentPrivate->dbg, "FillThisBuffer Line %d\n",__LINE__);
    pBuffer->nFilledLen = 0;
    OMX_PRBUFFER1(pComponentPrivate->dbg, "FillThisBuffer Line %d\n",__LINE__);
    /*Filling the Output buffer with zero */
    
    memset (pBuffer->pBuffer,0,pBuffer->nAllocLen);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "FillThisBuffer Line %d\n",__LINE__);

    OMX_PRBUFFER1(pComponentPrivate->dbg, "pComponentPrivate->pMarkBuf = %p\n",pComponentPrivate->pMarkBuf);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "pComponentPrivate->pMarkData = %p\n",pComponentPrivate->pMarkData);
    if(pComponentPrivate->pMarkBuf){
        OMX_PRBUFFER1(pComponentPrivate->dbg, "FillThisBuffer Line %d\n",__LINE__);
        pBuffer->hMarkTargetComponent = pComponentPrivate->pMarkBuf->hMarkTargetComponent;
        pBuffer->pMarkData = pComponentPrivate->pMarkBuf->pMarkData;
        pComponentPrivate->pMarkBuf = NULL;
    }

    if (pComponentPrivate->pMarkData) {
        OMX_PRBUFFER1(pComponentPrivate->dbg, "FillThisBuffer Line %d\n",__LINE__);
        pBuffer->hMarkTargetComponent = pComponentPrivate->hMarkTargetComponent;
        pBuffer->pMarkData = pComponentPrivate->pMarkData;
        pComponentPrivate->pMarkData = NULL;
    }

    nRet = write (pComponentPrivate->dataPipe[1], &pBuffer,
                  sizeof (OMX_BUFFERHEADERTYPE*));

    if (nRet == -1) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error in Writing to the Data pipe\n", __LINE__);
        return OMX_ErrorHardware;
    }
    else {
        pComponentPrivate->nUnhandledFillThisBuffers++;
        pComponentPrivate->nFillThisBufferCount++;
    }

    return OMX_ErrorNone;
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

    /* inform audio manager to remove the streamID*/
    /* compose the data */
    OMX_COMPONENTTYPE *pComponent = (OMX_COMPONENTTYPE *)pHandle;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate =
        (WMADEC_COMPONENT_PRIVATE *)pComponent->pComponentPrivate;
    struct OMX_TI_Debug dbg;
    dbg = pComponentPrivate->dbg;


    OMX_PRINT1(dbg, "%d ::ComponentDeInit\n",__LINE__);
    
#ifdef __PERF_INSTRUMENTATION__
    OMX_PRDSP1(dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    PERF_Boundary(pComponentPrivate->pPERF,
                  PERF_BoundaryStart | PERF_BoundaryCleanup);
#endif
    
#ifdef DSP_RENDERING_ON 
    close(fdwrite);
    close(fdread);
#endif

#ifdef RESOURCE_MANAGER_ENABLED  
    eError =  RMProxy_NewSendCommand(pHandle, RMProxy_FreeResource, 
                                     OMX_WMA_Decoder_COMPONENT, 0, 1234, NULL);
    if (eError != OMX_ErrorNone) {
        OMX_ERROR4(dbg, "%d ::OMX_AmrDecoder.c :: Error returned from destroy ResourceManagerProxy thread\n",
                       __LINE__);
    }
    eError = RMProxy_Deinitalize();
    if (eError != OMX_ErrorNone) {
        OMX_ERROR4(dbg, "%d ::Error returned from destroy ResourceManagerProxy thread\n",
                       __LINE__);
    }
#endif    
    OMX_PRINT1(dbg, "%d ::ComponentDeInit\n",__LINE__);
    pComponentPrivate->bIsStopping = 1;
    eError = WMADEC_StopComponentThread(pHandle);
    OMX_PRINT1(dbg, "%d ::ComponentDeInit\n",__LINE__);
    /* Wait for thread to exit so we can get the status into "error" */

#ifdef __PERF_INSTRUMENTATION__
    OMX_PRDSP1(dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    PERF_Boundary(pComponentPrivate->pPERF,
                  PERF_BoundaryComplete | PERF_BoundaryCleanup);
    PERF_Done(pComponentPrivate->pPERF);
#endif
    /* close the pipe handles and all resources */
    WMADEC_FreeCompResources(pHandle);
    OMX_PRINT1(dbg, "%d ::After free(pComponentPrivate)\n",__LINE__);
    OMX_DBG_CLOSE(dbg);
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
    OMXDBG_PRINT(stderr, PRINT, 1, 0, "===========================Inside the ComponentTunnelRequest==============================\n");
    OMXDBG_PRINT(stderr, ERROR, 4, 0, "Inside the ComponentTunnelRequest\n");
    return OMX_ErrorNotImplemented;
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
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHeader;

    pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    pPortDef = ((WMADEC_COMPONENT_PRIVATE*) 
                pComponentPrivate)->pPortDef[nPortIndex];

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: curState is  OMX_StateInvalid\n",__LINE__);
        return OMX_ErrorInvalidState;
    }   
#endif

    OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: pPortDef = %p\n", __LINE__,pPortDef);
    OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: pPortDef->bEnabled = %d\n", __LINE__,pPortDef->bEnabled);

    OMX_PRDSP2(pComponentPrivate->dbg, "pPortDef->bEnabled = %d\n", pPortDef->bEnabled);

    if(!pPortDef->bEnabled) {
        pComponentPrivate->AlloBuf_waitingsignal = 1;
        pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
        pthread_cond_wait(&pComponentPrivate->AlloBuf_threshold, &pComponentPrivate->AlloBuf_mutex);
        pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);

    }

    OMX_MALLOC_GENERIC(pBufferHeader, OMX_BUFFERHEADERTYPE);
    if(NULL==pBufferHeader)
        return OMX_ErrorInsufficientResources;

    memset(pBufferHeader, 0x0, sizeof(OMX_BUFFERHEADERTYPE));

    OMX_MALLOC_SIZE_DSPALIGN(pBufferHeader->pBuffer, nSizeBytes, OMX_U8);
    if(NULL == (pBufferHeader->pBuffer)) {
        OMX_MEMFREE_STRUCT(pBufferHeader);
        return OMX_ErrorInsufficientResources;
    }

    if (nPortIndex == INPUT_PORT)
    {
        pBufferHeader->nInputPortIndex = nPortIndex;
        pBufferHeader->nOutputPortIndex = -1; 
        pComponentPrivate->pInputBufferList->pBufHdr[pComponentPrivate->pInputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pInputBufferList->bBufferPending[pComponentPrivate->pInputBufferList->numBuffers] = 0;
        OMX_PRBUFFER1(pComponentPrivate->dbg, "pComponentPrivate->pInputBufferList->pBufHdr[%d] = %p\n",pComponentPrivate->pInputBufferList->numBuffers,pComponentPrivate->pInputBufferList->pBufHdr[pComponentPrivate->pInputBufferList->numBuffers]);
        pComponentPrivate->pInputBufferList->bufferOwner[pComponentPrivate->pInputBufferList->numBuffers++] = 1;
        OMX_PRBUFFER1(pComponentPrivate->dbg, "Allocate Buffer Line %d\n",__LINE__);
        OMX_PRBUFFER1(pComponentPrivate->dbg, "pComponentPrivate->pInputBufferList->numBuffers = %d\n",pComponentPrivate->pInputBufferList->numBuffers);
        OMX_PRBUFFER1(pComponentPrivate->dbg, "pPortDef->nBufferCountMin = %ld\n",pPortDef->nBufferCountMin);
        if (pComponentPrivate->pInputBufferList->numBuffers == pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
        }
    }
    else if (nPortIndex == OUTPUT_PORT) {
        pBufferHeader->nInputPortIndex = -1;
        pBufferHeader->nOutputPortIndex = nPortIndex; 
        pComponentPrivate->pOutputBufferList->pBufHdr[pComponentPrivate->pOutputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pOutputBufferList->bBufferPending[pComponentPrivate->pOutputBufferList->numBuffers] = 0;
        OMX_PRBUFFER1(pComponentPrivate->dbg, "pComponentPrivate->pOutputBufferList->pBufHdr[%d] = %p\n",pComponentPrivate->pOutputBufferList->numBuffers,pComponentPrivate->pOutputBufferList->pBufHdr[pComponentPrivate->pOutputBufferList->numBuffers]);
        pComponentPrivate->pOutputBufferList->bufferOwner[pComponentPrivate->pOutputBufferList->numBuffers++] = 1;
        if (pComponentPrivate->pOutputBufferList->numBuffers == pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
        }
    }
    else {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: BadPortIndex\n",__LINE__);
        OMX_MEMFREE_STRUCT_DSPALIGN(pBufferHeader->pBuffer,OMX_U8);
        OMX_MEMFREE_STRUCT(pBufferHeader);
        return OMX_ErrorBadPortIndex;
    }
    
    if((pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled)&&
       (pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled) &&
       (pComponentPrivate->InLoaded_readytoidle))


    {
        pComponentPrivate->InLoaded_readytoidle = 0;
        pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex);
        pthread_cond_signal(&pComponentPrivate->InLoaded_threshold);
        pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
    }

    
    pBufferHeader->pAppPrivate = pAppPrivate;
    pBufferHeader->pPlatformPrivate = pComponentPrivate;
    pBufferHeader->nAllocLen = nSizeBytes;
    pBufferHeader->nVersion.s.nVersionMajor = WMADEC_MAJOR_VER;
    pBufferHeader->nVersion.s.nVersionMinor = WMADEC_MINOR_VER;
    pComponentPrivate->nVersion = pBufferHeader->nVersion.nVersion;


    pBufferHeader->nSize = sizeof(OMX_BUFFERHEADERTYPE);
    OMX_PRINT2(pComponentPrivate->dbg, "Line %d\n",__LINE__); 
    *pBuffer = pBufferHeader;

    if (pComponentPrivate->bEnableCommandPending && pPortDef->bPopulated) {
        SendCommand (pComponentPrivate->pHandle,
                     OMX_CommandPortEnable,
                     pComponentPrivate->bEnableCommandParam,NULL);
    }
    
#ifdef __PERF_INSTRUMENTATION__
    OMX_PRDSP1(pComponentPrivate->dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    PERF_ReceivedBuffer(pComponentPrivate->pPERF,
                        (*pBuffer)->pBuffer, nSizeBytes,
                        PERF_ModuleMemory);
#endif

 EXIT:
    if (eError != OMX_ErrorNone) {
        if(NULL!=pBufferHeader) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferHeader->pBuffer,OMX_U8);
        }
        OMX_MEMFREE_STRUCT(pBufferHeader);
    }
    OMX_PRINT1(pComponentPrivate->dbg, "AllocateBuffer returning %x\n",eError);
    return eError;
}


/* ================================================================================= */
/**
 * @fn FreeBuffer() description for FreeBuffer  
 FreeBuffer().  
 Called by the OMX IL client to free a buffer. 
 *
 *  @see         OMX_Core.h
 */
/* ================================================================================ */
static OMX_ERRORTYPE FreeBuffer(
                                OMX_IN  OMX_HANDLETYPE hComponent,
                                OMX_IN  OMX_U32 nPortIndex,
                                OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    WMADEC_COMPONENT_PRIVATE * pComponentPrivate = NULL;
    OMX_BUFFERHEADERTYPE* buffHdr = NULL;
    OMX_U32 i;
    int bufferIndex = -1;
    BUFFERLIST *pBufferList = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
    OMX_COMPONENTTYPE *pHandle;

    pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;

    if (nPortIndex != OMX_DirInput && nPortIndex != OMX_DirOutput) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error - Unknown port index %ld\n",__LINE__, nPortIndex);
        return OMX_ErrorBadParameter;
    }

    pBufferList = ((nPortIndex == OMX_DirInput)? pComponentPrivate->pInputBufferList: pComponentPrivate->pOutputBufferList);
    pPortDef = pComponentPrivate->pPortDef[nPortIndex];
    for (i=0; i < pPortDef->nBufferCountActual; i++) {
        buffHdr = pBufferList->pBufHdr[i];
        if (buffHdr == pBuffer) {
            OMX_PRBUFFER2(pComponentPrivate->dbg, "Found matching %s buffer\n", (nPortIndex == OMX_DirInput)? "input": "output");
            OMX_PRBUFFER2(pComponentPrivate->dbg, "buffHdr = %p\n", buffHdr);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "pBuffer = %p\n", pBuffer);
            bufferIndex = i;
            break;
        }
        else {
            OMX_PRBUFFER2(pComponentPrivate->dbg, "This is not a match\n");
            OMX_PRBUFFER2(pComponentPrivate->dbg, "buffHdr = %p\n", buffHdr);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "pBuffer = %p\n", pBuffer);
        }
    }

    if (bufferIndex == -1) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error - could not find match for buffer %p\n",__LINE__, pBuffer);
        return OMX_ErrorBadParameter;
    }

    if (pBufferList->bufferOwner[bufferIndex] == 1) {
        OMX_MEMFREE_STRUCT_DSPALIGN(buffHdr->pBuffer, OMX_U8);
    }
#ifdef __PERF_INSTRUMENTATION__
    OMX_PRDSP1(pComponentPrivate->dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    PERF_SendingBuffer(pComponentPrivate->pPERF,
                       buffHdr->pBuffer,
                       buffHdr->nAllocLen,
                       PERF_ModuleMemory);
#endif

    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[FREE] %p\n", __LINE__, buffHdr);
    OMX_MEMFREE_STRUCT(pBufferList->pBufHdr[bufferIndex]);
    pBufferList->numBuffers--;

    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d ::numBuffers = %d nBufferCountMin = %ld\n", __LINE__, pBufferList->numBuffers, pPortDef->nBufferCountMin);

    if (pBufferList->numBuffers < pPortDef->nBufferCountMin) {
        OMX_PRCOMM2(pComponentPrivate->dbg, "%d ::OMX_AmrDecoder.c ::setting port %ld populated to OMX_FALSE\n", __LINE__, nPortIndex);
        pPortDef->bPopulated = OMX_FALSE;
    }

    if (pPortDef->bEnabled &&
        pComponentPrivate->bLoadedCommandPending == OMX_FALSE &&
        (pComponentPrivate->curState == OMX_StateIdle ||
         pComponentPrivate->curState == OMX_StateExecuting ||
         pComponentPrivate->curState == OMX_StatePause)) {
        pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                               OMX_EventError, OMX_ErrorPortUnpopulated,nPortIndex, NULL);
    }

    if ((!pComponentPrivate->pInputBufferList->numBuffers &&
         !pComponentPrivate->pOutputBufferList->numBuffers) &&
        pComponentPrivate->InIdle_goingtoloaded)
    {
        pComponentPrivate->InIdle_goingtoloaded = 0;                  
        pthread_mutex_lock(&pComponentPrivate->InIdle_mutex);
        pthread_cond_signal(&pComponentPrivate->InIdle_threshold);
        pthread_mutex_unlock(&pComponentPrivate->InIdle_mutex);
    }

    if (pComponentPrivate->bDisableCommandPending && 
        (pBufferList->numBuffers == 0)) {
        pComponentPrivate->bDisableCommandPending = OMX_FALSE;
        pComponentPrivate->cbInfo.EventHandler( pComponentPrivate->pHandle,
                                                pComponentPrivate->pHandle->pApplicationPrivate,
                                                OMX_EventCmdComplete,
                                                OMX_CommandPortDisable,
                                                nPortIndex,
                                                NULL);

    }

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting FreeBuffer\n", __LINE__);
    return eError;
}


/* ================================================================================= */
/**
 * @fn UseBuffer() description for UseBuffer  
 UseBuffer().  
 Called by the OMX IL client to pass a buffer to be used.   
 *
 *  @see         OMX_Core.h
 */
/* ================================================================================ */
static OMX_ERRORTYPE UseBuffer (
                                OMX_IN OMX_HANDLETYPE hComponent,
                                OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                                OMX_IN OMX_U32 nPortIndex,
                                OMX_IN OMX_PTR pAppPrivate,
                                OMX_IN OMX_U32 nSizeBytes,
                                OMX_IN OMX_U8* pBuffer)
{

    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHeader;

    pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: curState is  OMX_StateInvalid\n",__LINE__);
        return OMX_ErrorInvalidState;
    }
    
#endif

#ifdef __PERF_INSTRUMENTATION__
    OMX_PRDSP1(pComponentPrivate->dbg, "PERF %d :: OMX_WmaDecoder.c\n",__LINE__);
    PERF_ReceivedBuffer(pComponentPrivate->pPERF,
                        pBuffer, nSizeBytes,
                        PERF_ModuleHLMM);
#endif

    pPortDef = ((WMADEC_COMPONENT_PRIVATE*) 
                pComponentPrivate)->pPortDef[nPortIndex];
    if(!pPortDef->bEnabled){
        pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
        pComponentPrivate->AlloBuf_waitingsignal = 1;
        //wait for the port to be enabled before we accept buffers
        pthread_cond_wait(&pComponentPrivate->AlloBuf_threshold, &pComponentPrivate->AlloBuf_mutex);
        pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);
    }
    OMX_PRCOMM1(pComponentPrivate->dbg, "%d :: pPortDef = %p\n", __LINE__,pPortDef);
    OMX_PRCOMM1(pComponentPrivate->dbg, "%d :: pPortDef->bEnabled = %d\n", __LINE__,pPortDef->bEnabled);

    OMX_PRINT1(pComponentPrivate->dbg, "Line %d\n",__LINE__); 
    if(!pPortDef->bEnabled) {
	 OMX_ERROR4(pComponentPrivate->dbg, "%d :: Port is not eanbled\n",__LINE__);
        return OMX_ErrorIncorrectStateOperation;
    }

    OMX_PRINT1(pComponentPrivate->dbg, "Line %d\n",__LINE__); 
    OMX_PRINT1(pComponentPrivate->dbg, "pPortDef->bPopulated =%d\n",pPortDef->bPopulated);
    OMX_PRINT1(pComponentPrivate->dbg, "nSizeBytes =%ld\n",nSizeBytes);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "pPortDef->nBufferSize =%ld\n",pPortDef->nBufferSize);

    if(pPortDef->bPopulated) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Port is Populated\n",__LINE__);
        return OMX_ErrorBadParameter;
    }

    OMX_PRINT1(pComponentPrivate->dbg, "Line %d\n",__LINE__); 
    OMX_MALLOC_GENERIC(pBufferHeader, OMX_BUFFERHEADERTYPE);
    if(NULL==pBufferHeader)
        return OMX_ErrorInsufficientResources;

    memset((pBufferHeader), 0x0, sizeof(OMX_BUFFERHEADERTYPE));

    OMX_PRINT1(pComponentPrivate->dbg, "Line %d\n",__LINE__); 
    if (nPortIndex == OUTPUT_PORT) {
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

    if((pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled)&&
       (pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated == pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled) &&
       (pComponentPrivate->InLoaded_readytoidle))
    {
        pComponentPrivate->InLoaded_readytoidle = 0;                  
        pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex);
        pthread_cond_signal(&pComponentPrivate->InLoaded_threshold);
        pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
    }
    
    OMX_PRINT1(pComponentPrivate->dbg, "Line %d\n",__LINE__); 
    pBufferHeader->pAppPrivate = pAppPrivate;
    pBufferHeader->pPlatformPrivate = pComponentPrivate;
    pBufferHeader->nAllocLen = nSizeBytes;
    pBufferHeader->nVersion.s.nVersionMajor = WMADEC_MAJOR_VER;
    pBufferHeader->nVersion.s.nVersionMinor = WMADEC_MINOR_VER;
    pComponentPrivate->nVersion = pBufferHeader->nVersion.nVersion;
    pBufferHeader->pBuffer = pBuffer;
    pBufferHeader->nSize = sizeof(OMX_BUFFERHEADERTYPE);
    OMX_PRINT1(pComponentPrivate->dbg, "Line %d\n",__LINE__); 
    *ppBufferHdr = pBufferHeader;
    OMX_PRBUFFER1(pComponentPrivate->dbg, "pBufferHeader = %p\n",pBufferHeader);
    if (pComponentPrivate->bEnableCommandPending && pPortDef->bPopulated){
        OMX_PRCOMM2(pComponentPrivate->dbg, "Sending command before exiting usbuffer\n");
        SendCommand (pComponentPrivate->pHandle,
                     OMX_CommandPortEnable,
                     pComponentPrivate->bEnableCommandParam,NULL);
    }    
    OMX_PRINT1(pComponentPrivate->dbg, "exiting Use buffer\n");      
 EXIT:
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

    OMXDBG_PRINT(stderr, PRINT, 1, 0, "GetExtensionIndex\n");
    if (!(strcmp(cParameterName,"OMX.TI.index.config.wmaheaderinfo"))) {
        *pIndexType = OMX_IndexCustomWMADECHeaderInfoConfig;
        OMXDBG_PRINT(stderr, DSP, 2, 0, "OMX_IndexCustomWMADECHeaderInfoConfig\n");
    }
    else if(!(strcmp(cParameterName,"OMX.TI.index.config.wmastreamIDinfo"))) 
    {
        *pIndexType = OMX_IndexCustomWmaDecStreamIDConfig;
        
    }
    else if(!(strcmp(cParameterName,"OMX.TI.index.config.wmadec.datapath"))) 
    {
        *pIndexType = OMX_IndexCustomWmaDecDataPath;
    }    
    else if(!(strcmp(cParameterName,"OMX.TI.WMA.Decode.Debug"))) 
    {
	*pIndexType = OMX_IndexCustomDebug;
    }    
    else {
        eError = OMX_ErrorBadParameter;
    }

    OMXDBG_PRINT(stderr, PRINT, 1, 0, "Exiting GetExtensionIndex\n");
    return eError;
}

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
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate;
    
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)(((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if(nIndex == 0){
        memcpy(cRole, &pComponentPrivate->componentRole.cRole, sizeof(OMX_U8) * OMX_MAX_STRINGNAME_SIZE); 
        OMX_PRINT1(pComponentPrivate->dbg, "::::In ComponenetRoleEnum: cRole is set to %s\n",cRole);
    }
    else {
        eError = OMX_ErrorNoMore;
    }
    return eError;
};

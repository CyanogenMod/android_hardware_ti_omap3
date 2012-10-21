
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
*             Texas Instruments OMAP(TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* =========================================================================== */
/**
* @file OMX_iLBCDecoder.c
*
* This file implements OMX Component for iLBC decoder that
* is fully compliant with the OMX Audio specification.
*
* @path  $(CSLPATH)\
*
* @rev  0.1
*/
/* ---------------------------------------------------------------------------*/
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
#include <dlfcn.h>

#ifdef DSP_RENDERING_ON
#include <AudioManagerAPI.h>
#endif

/*-------program files ----------------------------------------*/
#include <OMX_Component.h>
#include <TIDspOmx.h>
#include "OMX_iLBCDec_Utils.h"

#define iLBC_DEC_ROLE "audio_decoder.xxxx"


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

static OMX_ERRORTYPE SendCommand (OMX_HANDLETYPE hComp, 
                                  OMX_COMMANDTYPE nCommand, OMX_U32 nParam, 
                                  OMX_PTR pCmdData);
static OMX_ERRORTYPE GetParameter(OMX_HANDLETYPE hComp, 
                                  OMX_INDEXTYPE nParamIndex,
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

static OMX_ERRORTYPE EmptyThisBuffer (OMX_HANDLETYPE hComp, 
                                      OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE FillThisBuffer (OMX_HANDLETYPE hComp, 
                                     OMX_BUFFERHEADERTYPE* pBuffer);
static OMX_ERRORTYPE GetState (OMX_HANDLETYPE hComp, OMX_STATETYPE* pState);
static OMX_ERRORTYPE ComponentTunnelRequest (OMX_HANDLETYPE hComp,
                                             OMX_U32 nPort, 
                                             OMX_HANDLETYPE hTunneledComp,
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
                                       OMX_OUT OMX_INDEXTYPE *pIndexType);

static OMX_ERRORTYPE ComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_OUT OMX_U8 *cRole,
                                       OMX_IN OMX_U32 nIndex);            


/* ========================================================================== */
/**
 * @def    PERMS                      Define Read and Write Permisions.
 */
/* ========================================================================== */
#define PERMS 0666

#ifdef DSP_RENDERING_ON
AM_COMMANDDATATYPE cmd_data;
#endif

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
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef_ip       = NULL, 
                                 *pPortDef_op       = NULL;
    iLBCDEC_COMPONENT_PRIVATE    *pComponentPrivate = NULL;
    OMX_AUDIO_PARAM_ILBCTYPE     *iLBC_ip           = NULL;
    OMX_AUDIO_PARAM_PCMMODETYPE  *iLBC_op           = NULL;
    OMX_ERRORTYPE                 eError            = OMX_ErrorNone;
    OMX_COMPONENTTYPE            *pHandle           = (OMX_COMPONENTTYPE*) hComp;
    OMX_S16 i = 0;

    iLBCDEC_DPRINT ("%d :: %s\n", __LINE__,__FUNCTION__);

    if (!pHandle) {
        return (OMX_ErrorBadParameter);
    }

    /*Set the all component function pointer to the handle */
    pHandle->SetCallbacks           = SetCallbacks;
    pHandle->GetComponentVersion    = GetComponentVersion;
    pHandle->SendCommand            = SendCommand;
    pHandle->GetParameter           = GetParameter;
    pHandle->SetParameter           = SetParameter;
    pHandle->GetConfig              = GetConfig;
    pHandle->SetConfig              = SetConfig;
    pHandle->GetState               = GetState;
    pHandle->EmptyThisBuffer        = EmptyThisBuffer;
    pHandle->FillThisBuffer         = FillThisBuffer;
    pHandle->ComponentTunnelRequest = ComponentTunnelRequest;
    pHandle->ComponentDeInit        = ComponentDeInit;
    pHandle->AllocateBuffer         = AllocateBuffer;
    pHandle->FreeBuffer             = FreeBuffer;
    pHandle->UseBuffer              = UseBuffer;
    pHandle->GetExtensionIndex      = GetExtensionIndex;
    pHandle->ComponentRoleEnum      = ComponentRoleEnum;

    /*Allocate the memory for Component private data area */
    OMX_MALLOC_GENERIC(pHandle->pComponentPrivate,iLBCDEC_COMPONENT_PRIVATE);

    pComponentPrivate = pHandle->pComponentPrivate;
    pComponentPrivate->pHandle = pHandle;
    pComponentPrivate->bMutexInitialized = OMX_FALSE;

    OMX_MALLOC_GENERIC(iLBC_ip,OMX_AUDIO_PARAM_ILBCTYPE);
    OMX_MALLOC_GENERIC(iLBC_op,OMX_AUDIO_PARAM_PCMMODETYPE);

    ((iLBCDEC_COMPONENT_PRIVATE *)
     pHandle->pComponentPrivate)->iLBCParams = iLBC_ip;
    ((iLBCDEC_COMPONENT_PRIVATE *)
     pHandle->pComponentPrivate)->pcmParams = iLBC_op;

    iLBCDEC_DPRINT("%d :: %s ::Malloced iLBCParams = 0x%p\n",__LINE__,
                  __FUNCTION__,
                  ((iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->iLBCParams);
    iLBCDEC_DPRINT("%d :: %s ::Malloced pcmParams = 0x%p\n",__LINE__,
                  __FUNCTION__,
                  ((iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->pcmParams);

    /* Initialize component data structures to default values */
    OMX_MALLOC_GENERIC(pComponentPrivate->sPortParam, OMX_PORT_PARAM_TYPE);
    OMX_CONF_INIT_STRUCT(pComponentPrivate->sPortParam, OMX_PORT_PARAM_TYPE);
    ((iLBCDEC_COMPONENT_PRIVATE *)
     pHandle->pComponentPrivate)->sPortParam->nPorts = 0x2;
    ((iLBCDEC_COMPONENT_PRIVATE *)
     pHandle->pComponentPrivate)->sPortParam->nStartPortNumber = 0x0;

    OMX_MALLOC_GENERIC(pComponentPrivate->pInputBufferList,iLBCD_BUFFERLIST);
    /* initialize number of buffers */
    pComponentPrivate->pInputBufferList->numBuffers = 0; 

    OMX_MALLOC_GENERIC(pComponentPrivate->pOutputBufferList,iLBCD_BUFFERLIST);
    /* initialize number of buffers */
    pComponentPrivate->pOutputBufferList->numBuffers = 0; 
    
    for (i=0; i < MAX_NUM_OF_BUFS; i++) {
        pComponentPrivate->pOutputBufferList->pBufHdr[i] = NULL;
        pComponentPrivate->pInputBufferList->pBufHdr[i] = NULL;
    }

    OMX_MALLOC_GENERIC(pComponentPrivate->pPriorityMgmt,OMX_PRIORITYMGMTTYPE);
    OMX_CONF_INIT_STRUCT(pComponentPrivate->pPriorityMgmt,OMX_PRIORITYMGMTTYPE);
         
    pComponentPrivate->bPlayCompleteFlag         = 0;
    pComponentPrivate->bInitParamsInitialized    = 0;
    pComponentPrivate->dasfmode                  = 0;
    pComponentPrivate->mimemode                  = 0;
    pComponentPrivate->bCompThreadStarted        = 0;
    pComponentPrivate->nHoldLength               = 0;
    pComponentPrivate->pHoldBuffer               = NULL;
    pComponentPrivate->pMarkBuf                  = NULL;
    pComponentPrivate->pMarkData                 = NULL;
    pComponentPrivate->nEmptyBufferDoneCount     = 0;
    pComponentPrivate->nEmptyThisBufferCount     = 0;
    pComponentPrivate->nFillBufferDoneCount      = 0;
    pComponentPrivate->nFillThisBufferCount      = 0;
    pComponentPrivate->strmAttr                  = NULL;
    pComponentPrivate->bIdleCommandPending       = 0;
    pComponentPrivate->bDisableCommandParam      = 0;
    pComponentPrivate->IpBufindex                = 0;
    pComponentPrivate->OpBufindex                = 0;
    pComponentPrivate->ptrLibLCML                = NULL;
    pComponentPrivate->nPendingOutPausedBufs     = 0;
    pComponentPrivate->bDspStoppedWhileExecuting = 0;
    
    for (i=0; i < MAX_NUM_OF_BUFS; i++) {
        pComponentPrivate->pInputBufHdrPending[i]  = NULL;
        pComponentPrivate->pOutputBufHdrPending[i] = NULL;
    }
    pComponentPrivate->nNumInputBufPending    = 0;
    pComponentPrivate->nNumOutputBufPending   = 0;
    pComponentPrivate->bDisableCommandPending = 0;
    pComponentPrivate->nOutStandingFillDones  = 0;
    pComponentPrivate->bStopSent              = 0;
    pComponentPrivate->bBypassDSP             = OMX_FALSE;
    pComponentPrivate->nRuntimeInputBuffers   = 0;
    pComponentPrivate->nRuntimeOutputBuffers  = 0;
    pComponentPrivate->bNoIdleOnStop          = OMX_FALSE;
    pComponentPrivate->pParams                = NULL;
    pComponentPrivate->LastOutbuf             = NULL;
    pComponentPrivate->DSPMMUFault            = OMX_FALSE;
    
    OMX_MALLOC_SIZE(pComponentPrivate->sDeviceString,
                           (100*sizeof(char)),OMX_STRING);
    strcpy((char*)pComponentPrivate->sDeviceString,"/eteedn:i0:o0/codec\0");
    
    /* Set input port format defaults */
    pComponentPrivate->sInPortFormat.nPortIndex  = iLBCD_INPUT_PORT;
    pComponentPrivate->sInPortFormat.nIndex      = OMX_IndexParamAudioILBC; /* <<<  Gsm_FR;    */
    pComponentPrivate->sInPortFormat.eEncoding   = OMX_AUDIO_CodingiLBC;

    /* Set output port format defaults */
    pComponentPrivate->sOutPortFormat.nPortIndex = iLBCD_OUTPUT_PORT;
    pComponentPrivate->sOutPortFormat.nIndex     = OMX_IndexParamAudioPcm;
    pComponentPrivate->sOutPortFormat.eEncoding  = OMX_AUDIO_CodingPCM;
    
    pComponentPrivate->iLBCcodecType = iLBC_PrimaryCodec;

    iLBC_ip->nSize = sizeof (OMX_AUDIO_PARAM_ILBCTYPE);
    iLBC_ip->nPortIndex = OMX_DirInput;

    /* PCM format defaults - These values are required to pass StdAudioDecoderTest*/
    iLBC_op->nSize = sizeof (OMX_AUDIO_PARAM_PCMMODETYPE);
    iLBC_op->nPortIndex = OMX_DirOutput;
    iLBC_op->nChannels = 1; 
    iLBC_op->eNumData= OMX_NumericalDataSigned;
    iLBC_op->nSamplingRate = 8000;           
    iLBC_op->nBitPerSample = 16;
    iLBC_op->ePCMMode = OMX_AUDIO_PCMModeLinear;

    strcpy((char*)pComponentPrivate->componentRole.cRole, "audio_decoder.xxx");
    
    pthread_mutex_init(&pComponentPrivate->AlloBuf_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->AlloBuf_threshold, NULL);
    pComponentPrivate->AlloBuf_waitingsignal = 0;

    pthread_mutex_init(&pComponentPrivate->InLoaded_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->InLoaded_threshold, NULL);
    pComponentPrivate->InLoaded_readytoidle = 0;

    pthread_mutex_init(&pComponentPrivate->InIdle_mutex, NULL);
    pthread_cond_init (&pComponentPrivate->InIdle_threshold, NULL);
    pComponentPrivate->InIdle_goingtoloaded = 0;
    pComponentPrivate->bMutexInitialized = OMX_TRUE;

    OMX_MALLOC_GENERIC(pPortDef_ip,OMX_PARAM_PORTDEFINITIONTYPE);
    OMX_MALLOC_GENERIC(pPortDef_op,OMX_PARAM_PORTDEFINITIONTYPE);

    iLBCDEC_DPRINT ("%d :: %s ::pPortDef_ip = 0x%p\n", __LINE__,
                   __FUNCTION__,pPortDef_ip);
    iLBCDEC_DPRINT ("%d :: %s ::pPortDef_op = 0x%p\n", __LINE__,
                   __FUNCTION__,pPortDef_op);

    pComponentPrivate->pPortDef[iLBCD_INPUT_PORT] = pPortDef_ip;
    pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]= pPortDef_op;
/* Define Input Port Definition*/
    pPortDef_ip->eDomain = OMX_PortDomainAudio;
    pPortDef_ip->nSize = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
    pPortDef_ip->nPortIndex = 0x0;
    pPortDef_ip->nBufferCountActual = iLBCD_NUM_INPUT_BUFFERS;
    
    pPortDef_ip->nBufferCountMin = iLBCD_NUM_INPUT_BUFFERS;
    pPortDef_ip->eDir = OMX_DirInput;
    pPortDef_ip->bEnabled = OMX_TRUE;
    pPortDef_ip->nBufferSize = STD_iLBCDEC_BUF_SIZE;
    pPortDef_ip->bPopulated = 0;   
    pPortDef_ip->format.audio.eEncoding = OMX_AUDIO_CodingiLBC;

/* Define Output Port Definition*/
    pPortDef_op->eDomain = OMX_PortDomainAudio;
    pPortDef_op->nSize = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
    pPortDef_op->nPortIndex = 0x1;
    pPortDef_op->nBufferCountActual = iLBCD_NUM_OUTPUT_BUFFERS;
    pPortDef_op->nBufferCountMin = iLBCD_NUM_OUTPUT_BUFFERS;
    pPortDef_op->eDir = OMX_DirOutput;
    pPortDef_op->bEnabled = OMX_TRUE;
    pPortDef_op->nBufferSize = iLBCD_OUTPUT_BUFFER_SIZE;
    pPortDef_op->bPopulated = 0;
    pPortDef_op->format.audio.eEncoding = OMX_AUDIO_CodingPCM;

    pComponentPrivate->bIsInvalidState = OMX_FALSE;
    pComponentPrivate->bPortDefsAllocated = 1;
    

#ifdef RESOURCE_MANAGER_ENABLED    
    eError =RMProxy_NewInitalize ();

    iLBCDEC_DPRINT ("%d :: %s \n", __LINE__,__FUNCTION__);
    if (eError != OMX_ErrorNone) {
        iLBCDEC_EPRINT ("%d :: %s ::Error returned from loading ResourceManagerProxy thread\n", __LINE__,__FUNCTION__);
        goto EXIT;
    }
#endif

#ifdef DSP_RENDERING_ON
    iLBCDEC_DPRINT ("%d :: %s \n", __LINE__,__FUNCTION__);
    if((pComponentPrivate->fdwrite=open(FIFO1,O_WRONLY))<0) {
        iLBCDEC_EPRINT("[iLBC Dec Component] - failure to open WRITE pipe\n");
    }

    iLBCDEC_DPRINT ("%d :: %s \n", __LINE__,__FUNCTION__);
    if((pComponentPrivate->fdread=open(FIFO2,O_RDONLY))<0) {
        iLBCDEC_EPRINT("[iLBC Dec Component] - failure to open READ pipe\n");
        goto EXIT;
    }
#endif

    eError = iLBCDEC_StartComponentThread(pHandle);
    if (eError != OMX_ErrorNone) {
        iLBCDEC_EPRINT ("%d :: %s ::Error returned from the Component\n",
                       __LINE__,__FUNCTION__);
        goto EXIT;
    }

EXIT:
    if(OMX_ErrorNone != eError) {
        iLBCDEC_EPRINT(":: ********* ERROR: Freeing Other Malloced Resources\n");
        iLBCDEC_FreeCompResources(pHandle);
        // Free pComponentPrivate
        OMX_MEMFREE_STRUCT(pComponentPrivate);
    }

    iLBCDEC_DPRINT ("%d :: %s :: returning %d\n", __LINE__,__FUNCTION__,eError);
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
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;

    if (!pHandle || !pCallBacks) {
        return (OMX_ErrorBadParameter);
    }
    if (!pCallBacks->FillBufferDone ||
        !pCallBacks->EventHandler ||
        !pCallBacks->EmptyBufferDone) {
        return (OMX_ErrorBadParameter);
    }
    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    /*Copy the callbacks of the application to the component private */
    memcpy (&(pComponentPrivate->cbInfo), pCallBacks, sizeof(OMX_CALLBACKTYPE));

    /*copy the application private data to component memory */
    pHandle->pApplicationPrivate = pAppData;

    pComponentPrivate->curState = OMX_StateLoaded;
    iLBCDEC_DPRINT ("%d :: %s :: Component set to OMX_StateLoaded = %d\n",
                __LINE__,__FUNCTION__,pComponentPrivate->curState);

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

    eError = OMX_ErrorNotImplemented;
    iLBCDEC_DPRINT ("Inside the GetComponentVersion\n");
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
                                  OMX_U32 nParam,OMX_PTR pCmdData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ssize_t nRet = 0;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)phandle;
    iLBCDEC_COMPONENT_PRIVATE *pCompPrivate =
        (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);

    pCompPrivate->pHandle = phandle;

    if(pCompPrivate->curState == OMX_StateInvalid){
        eError = OMX_ErrorInvalidState;
#ifndef _ERROR_PROPAGATION__                  
        iLBCDEC_EPRINT("%d :: %s ::  Error Notofication Sent to App\n",
                    __LINE__,__FUNCTION__);
        pCompPrivate->cbInfo.EventHandler(pHandle, 
                                          pHandle->pApplicationPrivate,
                                          OMX_EventError, OMX_ErrorInvalidState,0,
                                          "Invalid State");
#endif                                          
        goto EXIT;
    }

    switch(Cmd) {
    case OMX_CommandStateSet:
        iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);
        if (nParam == OMX_StateLoaded) {
            pCompPrivate->bLoadedCommandPending = OMX_TRUE;
        }            
        if(pCompPrivate->curState == OMX_StateLoaded) {
            if((nParam == OMX_StateExecuting) || 
               (nParam == OMX_StatePause)) {
                pCompPrivate->cbInfo.EventHandler (
                                                   pHandle,
                                                   pHandle->pApplicationPrivate,
                                                   OMX_EventError,
                                                   OMX_ErrorIncorrectStateTransition,
                                                   0,
                                                   NULL);
                goto EXIT;
            }

            if(nParam == OMX_StateInvalid) {
                iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);
                pCompPrivate->curState = OMX_StateInvalid;
                pCompPrivate->cbInfo.EventHandler (pHandle,
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
        iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);
        if(nParam > 1 && nParam != OMX_ALL) {
            eError = OMX_ErrorBadPortIndex;
            goto EXIT;
        }

        break;
    case OMX_CommandPortDisable:
        iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);
        break;
    case OMX_CommandPortEnable:
        iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);
        break;
    case OMX_CommandMarkBuffer:
        if (nParam > 0) {
            eError = OMX_ErrorBadPortIndex;
            goto EXIT;
        }
        break;
    default:
        iLBCDEC_DPRINT("%d :: %s Command Received Default error\n",
                      __LINE__,__FUNCTION__);
        pCompPrivate->cbInfo.EventHandler (pHandle, pHandle->pApplicationPrivate,
                                           OMX_EventError,
                                           OMX_ErrorUndefined,0,
                                           "Invalid Command");
        break;

    }

    iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);
    nRet = write (pCompPrivate->cmdPipe[1], &Cmd, sizeof(Cmd));
    if (nRet == -1) {
        iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    if (Cmd == OMX_CommandMarkBuffer) {
        nRet = write(pCompPrivate->cmdDataPipe[1], &pCmdData,
                     sizeof(OMX_PTR));
    }
    else {
        nRet = write(pCompPrivate->cmdDataPipe[1], &nParam,
                     sizeof(OMX_U32));
    }

    iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);
    iLBCDEC_DPRINT ("%d :: %s ::nRet = %d\n",__LINE__,__FUNCTION__,nRet);
    if (nRet == -1) {
        iLBCDEC_DPRINT ("%d :: %s ::\n",__LINE__,__FUNCTION__);
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

#ifdef DSP_RENDERING_ON
    if(Cmd == OMX_CommandStateSet && nParam == OMX_StateExecuting) {
        /* enable Tee device command*/
        cmd_data.hComponent = pHandle;
        cmd_data.AM_Cmd = AM_CommandTDNDownlinkMode;
        cmd_data.param1 = 0;
        cmd_data.param2 = 0;
        cmd_data.streamID = 0;                  
        if((write(pCompPrivate->fdwrite, &cmd_data, sizeof(cmd_data)))<0) {
            eError = OMX_ErrorHardware;
            goto EXIT;
        }
    }
#endif     
    
 EXIT:
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
    iLBCDEC_COMPONENT_PRIVATE  *pComponentPrivate = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pParameterStructure = NULL;

    iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);

    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);
    pParameterStructure = 
        (OMX_PARAM_PORTDEFINITIONTYPE*)ComponentParameterStructure;

    if (pParameterStructure == NULL) {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
    iLBCDEC_DPRINT("%d :: %s ::pParameterStructure = %p\n",
                  __LINE__,__FUNCTION__,pParameterStructure);
    iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);

    if(pComponentPrivate->curState == OMX_StateInvalid) {
#ifndef _ERROR_PROPAGATION__    
        pComponentPrivate->cbInfo.EventHandler(hComp,
                                               ((OMX_COMPONENTTYPE *)hComp)->pApplicationPrivate,
                                               OMX_EventError,
                                               OMX_ErrorIncorrectStateOperation,
                                               0,
                                               NULL);
#else                                               
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorInvalidState\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorInvalidState);
#endif                             
    }
    iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);

    switch(nParamIndex){
    case OMX_IndexParamAudioInit:
        iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
        memcpy(ComponentParameterStructure, &pComponentPrivate->sPortParam,
               sizeof(OMX_PORT_PARAM_TYPE));
        break;

    case OMX_IndexParamPortDefinition:
        iLBCDEC_DPRINT ("%d :: %s ::pParameterStructure->nPortIndex = %ld\n",
                       __LINE__,__FUNCTION__,pParameterStructure->nPortIndex);
        iLBCDEC_DPRINT ("%d :: %s ::pComponentPrivate->pPortDef[iLBCDEC_INPUT_P\
ORT]->nPortIndex = %ld\n",__LINE__,__FUNCTION__,
                       pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->nPortIndex);

        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(ComponentParameterStructure))->nPortIndex == 
           pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->nPortIndex) {
            iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
            memcpy(ComponentParameterStructure,
                   pComponentPrivate->pPortDef[iLBCD_INPUT_PORT], 
                   sizeof(OMX_PARAM_PORTDEFINITIONTYPE));                                       
        } 
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(ComponentParameterStructure))->nPortIndex == 
                pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->nPortIndex) {
            iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
            memcpy(ComponentParameterStructure, 
                   pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT], 
                   sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        } 
        else {
            eError = OMX_ErrorBadPortIndex;
        }
        break;
                
    case OMX_IndexParamAudioPortFormat:         
        iLBCDEC_DPRINT ("%d :: %s ::((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex = %ld\n",__LINE__,__FUNCTION__,
                       ((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex);
        iLBCDEC_DPRINT ("%d :: %s ::pComponentPrivate->sInPortFormat.nPortIndex = %ld\n",__LINE__,__FUNCTION__,pComponentPrivate->sInPortFormat.nPortIndex);
        iLBCDEC_DPRINT ("%d :: %s ::pComponentPrivate->sOutPortFormat.nPortIndex= %ld\n",__LINE__,__FUNCTION__,pComponentPrivate->sOutPortFormat.nPortIndex);

        if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex == pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->nPortIndex) {
            if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nIndex > pComponentPrivate->sInPortFormat.nPortIndex) {
                eError = OMX_ErrorNoMore;
            } 
            else {
                memcpy(ComponentParameterStructure, 
                       &pComponentPrivate->sInPortFormat,
                       sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            }
        }
        else if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nPortIndex == pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->nPortIndex){
            iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
            if(((OMX_AUDIO_PARAM_PORTFORMATTYPE *)(ComponentParameterStructure))->nIndex > pComponentPrivate->sOutPortFormat.nPortIndex) {
                eError = OMX_ErrorNoMore;
            } 
            else {
                iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
                memcpy(ComponentParameterStructure, 
                       &pComponentPrivate->sOutPortFormat, 
                       sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            }
        } 
        else {
            iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
            eError = OMX_ErrorBadPortIndex;
        }                
        break;

    case (OMX_INDEXTYPE)OMX_IndexParamAudioILBC: /* <<<< 0j0 Gsm_FR: */
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(ComponentParameterStructure))->nPortIndex == 
           pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->nPortIndex) {
            iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
            memcpy(ComponentParameterStructure,
                   pComponentPrivate->iLBCParams, 
                   sizeof(OMX_AUDIO_PARAM_ILBCTYPE));                                       
        } else {
            eError = OMX_ErrorBadPortIndex;
        }
        break;

    case OMX_IndexParamAudioPcm:
        if(((OMX_AUDIO_PARAM_ILBCTYPE *)(ComponentParameterStructure))->nPortIndex == iLBCD_OUTPUT_PORT){
            memcpy(ComponentParameterStructure, 
                   pComponentPrivate->pcmParams, 
                   sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }
        else {
            eError = OMX_ErrorBadPortIndex;
        }
        break;

             
    case OMX_IndexParamPriorityMgmt:
        memcpy(ComponentParameterStructure, 
               pComponentPrivate->pPriorityMgmt, 
               sizeof(OMX_PRIORITYMGMTTYPE));
        break;
            
    case OMX_IndexParamCompBufferSupplier:
        if(((OMX_PARAM_BUFFERSUPPLIERTYPE *)(ComponentParameterStructure))->nPortIndex == OMX_DirInput) {
            iLBCDEC_DPRINT(":: GetParameter OMX_IndexParamCompBufferSupplier\n");
        }
        else if(((OMX_PARAM_BUFFERSUPPLIERTYPE *)(ComponentParameterStructure))->nPortIndex == OMX_DirOutput) {
            iLBCDEC_DPRINT(":: GetParameter OMX_IndexParamCompBufferSupplier\n");
        } 
        else {
            iLBCDEC_DPRINT(":: OMX_ErrorBadPortIndex from GetParameter");
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
        eError = OMX_ErrorUnsupportedIndex;
        
        break;
    }
 EXIT:
    iLBCDEC_DPRINT("%d :: %s :: Exiting GetParameter:: %x\n",
                  __LINE__,__FUNCTION__,nParamIndex);
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
    /*    OMX_BOOL temp_bEnabled, temp_bPopulated;*/
    OMX_COMPONENTTYPE* pHandle= (OMX_COMPONENTTYPE*)hComp;
    iLBCDEC_COMPONENT_PRIVATE  *pComponentPrivate = NULL;
    OMX_AUDIO_PARAM_PORTFORMATTYPE *pComponentParam = NULL;
    OMX_PARAM_COMPONENTROLETYPE  *pRole = NULL;
    OMX_AUDIO_PARAM_PCMMODETYPE *iLBC_op = NULL;
    OMX_AUDIO_PARAM_ILBCTYPE *iLBC_ip = NULL;
    OMX_PARAM_BUFFERSUPPLIERTYPE sBufferSupplier;
    
    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);

    if (!pCompParam) {
        return (OMX_ErrorBadParameter);
    }

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorInvalidState\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorInvalidState);
    }
#endif                             

    switch(nParamIndex) {
    case OMX_IndexParamAudioPortFormat:
        pComponentParam = (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pCompParam;

        iLBCDEC_DPRINT("%d :: %s ::pComponentParam->nPortIndex = %ld\n",
                      __LINE__,__FUNCTION__,pComponentParam->nPortIndex);

        /* 0 means Input port */
        if (pComponentParam->nPortIndex == 0) {                  
            memcpy(&pComponentPrivate->sInPortFormat, 
                   pComponentParam, 
                   sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
        } else if (pComponentParam->nPortIndex == 1) {
            /* 1 means Output port */
            memcpy(&pComponentPrivate->sOutPortFormat, 
                   pComponentParam, 
                   sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));                    
        }else {
            iLBCDEC_EPRINT ("%d :: %s Wrong Port Index Parameter\n",__LINE__,
                        __FUNCTION__);
            iLBCDEC_EPRINT("%d :: %s :: About to return OMX_ErrorBadParameter\n",
                        __LINE__,__FUNCTION__);
            eError = OMX_ErrorBadParameter;
            iLBCDEC_DPRINT("WARNING: %s %d\n", __FUNCTION__,__LINE__);                   
            goto EXIT;
        }
        break;
    case (OMX_INDEXTYPE)OMX_IndexParamAudioILBC: /* <<<< 0j0 Gsm_FR: */
        iLBC_ip = (OMX_AUDIO_PARAM_ILBCTYPE *)pCompParam;
	if (((iLBCDEC_COMPONENT_PRIVATE*)
                    pHandle->pComponentPrivate)->iLBCParams == NULL)
	{
	    eError = OMX_ErrorBadParameter;
	    break;
	}
        /* 0 means Input port */
        if(iLBC_ip->nPortIndex == iLBCD_INPUT_PORT) {
            memcpy(((iLBCDEC_COMPONENT_PRIVATE*)
                    pHandle->pComponentPrivate)->iLBCParams,
                   iLBC_ip, sizeof(OMX_AUDIO_PARAM_ILBCTYPE));

        } 
        else {
            eError = OMX_ErrorBadPortIndex;
        }
        break;
    case OMX_IndexParamPortDefinition:
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
           pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->nPortIndex) {
            memcpy(pComponentPrivate->pPortDef[iLBCD_INPUT_PORT],
                   pCompParam,
                   sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        }
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
                pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->nPortIndex) {
            memcpy(pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT],
                   pCompParam,
                   sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        }
        else {
            eError = OMX_ErrorBadPortIndex;
        }
        break;
    case OMX_IndexParamPriorityMgmt:
        if (pComponentPrivate->curState == OMX_StateIdle ){
            eError = OMX_ErrorIncorrectStateOperation;
            break;
        }
	if (pComponentPrivate->pPriorityMgmt == NULL)
	{
	    eError = OMX_ErrorBadParameter;
	    break;
	}

        memcpy(pComponentPrivate->pPriorityMgmt, 
               (OMX_PRIORITYMGMTTYPE*)pCompParam, 
               sizeof(OMX_PRIORITYMGMTTYPE));
        break;

    case OMX_IndexParamStandardComponentRole: 
        if (pCompParam) {
            pRole = (OMX_PARAM_COMPONENTROLETYPE *)pCompParam;
            memcpy(&(pComponentPrivate->componentRole),
                   (void *)pRole, sizeof(OMX_PARAM_COMPONENTROLETYPE));
        } else {
            eError = OMX_ErrorBadParameter;
        }
        break;

    case OMX_IndexParamAudioPcm:
        iLBC_op = (OMX_AUDIO_PARAM_PCMMODETYPE *)pCompParam;
	if (pComponentPrivate->pcmParams == NULL)
	{
	    eError = OMX_ErrorBadParameter;
	    break;
	}
        /* 1 means Output port */
        if (iLBC_op->nPortIndex == 1) {
            memcpy(pComponentPrivate->pcmParams, iLBC_op, 
                   sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }else{
            eError = OMX_ErrorBadPortIndex;
        }
        break;

    case OMX_IndexParamCompBufferSupplier:             
        if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
           pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->nPortIndex) {
            sBufferSupplier.eBufferSupplier = OMX_BufferSupplyInput;
            memcpy(&sBufferSupplier, pCompParam, 
                   sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
        }
        else if(((OMX_PARAM_PORTDEFINITIONTYPE *)(pCompParam))->nPortIndex ==
                pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->nPortIndex) {
            sBufferSupplier.eBufferSupplier = OMX_BufferSupplyOutput;
            memcpy(&sBufferSupplier, pCompParam, 
                   sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
        } 
        else {
            iLBCDEC_DPRINT(":: OMX_ErrorBadPortIndex from SetParameter");
            eError = OMX_ErrorBadPortIndex;
        }
        break;
        
    case (OMX_INDEXTYPE)OMX_IndexCustomConfigILBCCodec:
        ((iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->iLBCcodecType = (int) pCompParam; /*((iLBCDEC_COMPONENT_PRIVATE *) ComponentConfigStructure)->iLBCcodecType;*/
        break;

    default:
        break;

    }
 EXIT:
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

    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    TI_OMX_STREAM_INFO *streamInfo = NULL;

    OMX_MALLOC_GENERIC(streamInfo,TI_OMX_STREAM_INFO);

    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComp)->pComponentPrivate);

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorInvalidState\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorInvalidState);
    }
#endif

    if(nConfigIndex == OMX_IndexCustomiLBCDecStreamIDConfig){
        /* copy component info */
        streamInfo->streamId = pComponentPrivate->streamID;
	if (ComponentConfigStructure == NULL)
	    eError = OMX_ErrorBadParameter;
	else
            memcpy(ComponentConfigStructure,streamInfo,sizeof(TI_OMX_STREAM_INFO));
    }

    OMX_MEMFREE_STRUCT(streamInfo);

 EXIT:
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
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_S16 *customFlag = NULL;
    TI_OMX_DSP_DEFINITION *configData = NULL;
    int flagValue=0;
    TI_OMX_DATAPATH dataPath;

    iLBCDEC_DPRINT("%d :: %s :: Entering SetConfig\n", __LINE__,__FUNCTION__);
    if (!pHandle) {
        return (OMX_ErrorBadParameter);
    }

    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorInvalidState\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorInvalidState);
    }
#endif

    switch (nConfigIndex) {
    case  (OMX_INDEXTYPE)OMX_IndexCustomiLBCDecHeaderInfoConfig:
        iLBCDEC_DPRINT("%d :: %s :: SetConfig OMX_IndexCustomiLBCDecHeaderInfoC\
onfig \n",__LINE__,__FUNCTION__);

        configData = (TI_OMX_DSP_DEFINITION*)ComponentConfigStructure;

        if (configData) {
            pComponentPrivate->acdnmode = configData->acousticMode;
            if (configData->dasfMode == 0) {
                pComponentPrivate->dasfmode = 0;
            }
            else if (configData->dasfMode == 1) {
                pComponentPrivate->dasfmode = 1;
            }

            if (pComponentPrivate->dasfmode ){
                pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled = 0;
            }

            pComponentPrivate->streamID = configData->streamId;
        }
        break;

    case (OMX_INDEXTYPE)OMX_IndexCustomiLBCDecDataPath:
        customFlag = (OMX_S16*)ComponentConfigStructure;

        if (customFlag) {
            dataPath = *customFlag;

            switch(dataPath) {
            case DATAPATH_APPLICATION:
                OMX_MMMIXER_DATAPATH(pComponentPrivate->sDeviceString, 
                                     RENDERTYPE_DECODER, 
                                     pComponentPrivate->streamID);
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
        }
        break;

    case (OMX_INDEXTYPE)OMX_IndexCustomiLBCDecModeEfrConfig:
        customFlag = (OMX_S16*)ComponentConfigStructure;

        if (customFlag) {
            pComponentPrivate->iAmrMode = *customFlag;
        }
        break;

    case (OMX_INDEXTYPE)OMX_IndexCustomiLBCDecModeDasfConfig:
        customFlag = (OMX_S16*)ComponentConfigStructure;

        if (customFlag) {
            flagValue = *customFlag;
            if (flagValue == 0) {
                pComponentPrivate->dasfmode = 0;
            }
            else if (flagValue == 1) {
                pComponentPrivate->dasfmode = 1;
            }
            else if (flagValue == 2) {
                pComponentPrivate->dasfmode = 1;
            }
            iLBCDEC_DPRINT("%d :: %s ::pComponentPrivate->dasfmode = %d\n",
                          __LINE__,__FUNCTION__,pComponentPrivate->dasfmode);
            if (pComponentPrivate->dasfmode ){
                pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled = 0;
            }
        }
        break;

    case (OMX_INDEXTYPE)OMX_IndexCustomiLBCDecModeMimeConfig:
        customFlag = (OMX_S16*)ComponentConfigStructure;

        if (customFlag) {
            pComponentPrivate->mimemode = *customFlag;
            iLBCDEC_DPRINT("%d :: %s ::pComponentPrivate->mimemode = %d\n",__LINE__,
                          __FUNCTION__,pComponentPrivate->mimemode);
        }
        break;

    case OMX_IndexConfigAudioMute:
#ifdef DSP_RENDERING_ON
        pMuteStructure = (OMX_AUDIO_CONFIG_MUTETYPE *)ComponentConfigStructure;
        iLBCDEC_DPRINT("Set Mute/Unmute for playback stream\n"); 
        cmd_data.hComponent = hComp;
        if(pMuteStructure->bMute == OMX_TRUE) {
            iLBCDEC_DPRINT("Mute the playback stream\n");
            cmd_data.AM_Cmd = AM_CommandStreamMute;
        }
        else {
            iLBCDEC_DPRINT("unMute the playback stream\n");
            cmd_data.AM_Cmd = AM_CommandStreamUnMute;
        }
        cmd_data.param1 = 0;
        cmd_data.param2 = 0;
        cmd_data.streamID = pComponentPrivate->streamID;
        if((write(pComponentPrivate->fdwrite, &cmd_data, 
                  sizeof(cmd_data)))<0) {
            iLBCDEC_EPRINT("%d :: %s ::[iLBC decoder] - fail to send Mute command to audio manager\n",__LINE__,__FUNCTION__);
        }

        break;
#endif

    case OMX_IndexConfigAudioVolume:
#ifdef DSP_RENDERING_ON
        pVolumeStructure = (OMX_AUDIO_CONFIG_VOLUMETYPE *)
            ComponentConfigStructure;
        iLBCDEC_DPRINT("Set volume for playback stream\n"); 
        cmd_data.hComponent = hComp;
        cmd_data.AM_Cmd = AM_CommandSWGain;
        cmd_data.param1 = pVolumeStructure->sVolume.nValue;
        cmd_data.param2 = 0;
        cmd_data.streamID = pComponentPrivate->streamID;

        if((write(pComponentPrivate->fdwrite, &cmd_data, sizeof(cmd_data)))<0) {
            iLBCDEC_EPRINT("%d :: %s ::[iLBC decoder] - fail to send Volume com\
mand to audio manager\n",__LINE__,__FUNCTION__);
        }

        break;
#endif
        
    default:
        eError = OMX_ErrorUnsupportedIndex;
        break;
    }

    iLBCDEC_DPRINT("%d :: %s :: Exiting SetConiLBCDEC_DPRINTfig\n", __LINE__,__FUNCTION__);
    iLBCDEC_DPRINT("%d :: %s :: Returning = 0x%x\n",__LINE__,__FUNCTION__,
                  eError);
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
    OMX_ERRORTYPE      eError  = OMX_ErrorUndefined;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;

    if (!pHandle) {
        return (OMX_ErrorBadParameter);
    }
    if (!pState){
        iLBCDEC_DPRINT("%d :: %s :: OMX_ErrorBadParameter\n", __LINE__,__FUNCTION__);
        return (eError);
    }

    if (pHandle && pHandle->pComponentPrivate) {
        *pState = ((iLBCDEC_COMPONENT_PRIVATE*)
                    pHandle->pComponentPrivate)->curState;
    } else {
        *pState = OMX_StateLoaded;
    }

    eError = OMX_ErrorNone;

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
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate =
        (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
    ssize_t ret  = 0;

    pPortDef = ((iLBCDEC_COMPONENT_PRIVATE*)
                pComponentPrivate)->pPortDef[iLBCD_INPUT_PORT];


#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        iLBCDEC_DPRINT("%d :: %s :: ErrorInvalidState \n", __LINE__,__FUNCTION__);
        return (OMX_ErrorStateInvalid);
    }
#endif

    if(!pPortDef->bEnabled){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorIncorrectStateOperation\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorIncorrectStateOperation);
    }

    if (pBuffer == NULL){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorBadParameter\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorBadParameter);
    }


    if (pBuffer->nSize != sizeof(OMX_BUFFERHEADERTYPE)) {
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorBadParameter\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorBadParameter);
    }

    if (pBuffer->nVersion.nVersion != pComponentPrivate->nVersion){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorVersionMismatch\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorVersionMismatch);
    }

    if (pBuffer->nInputPortIndex != iLBCD_INPUT_PORT){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorBadPortIndex\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorBadPortIndex);
    }
    iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->curState = %d\n",__LINE__,
                  __FUNCTION__,pComponentPrivate->curState);
    if(pComponentPrivate->curState != OMX_StateExecuting && 
        pComponentPrivate->curState != OMX_StatePause) {
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorIncorrectStateOperation\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorIncorrectStateOperation);
    }

    iLBCDEC_DPRINT("\n------------------------------------------\n\n");
    iLBCDEC_DPRINT ("%d :: %s :: Component Sending Filled ip buff %p \
                             to Component Thread\n",__LINE__,__FUNCTION__,
                             pBuffer);
    iLBCDEC_DPRINT("\n------------------------------------------\n\n");

    pComponentPrivate->app_nBuf--;

    pComponentPrivate->pMarkData = pBuffer->pMarkData;
    pComponentPrivate->hMarkTargetComponent = pBuffer->hMarkTargetComponent;
    
    ret = write (pComponentPrivate->dataPipe[1], &pBuffer,
                 sizeof(OMX_BUFFERHEADERTYPE*));
    if (ret == -1) {
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorHardware\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorHardware);
    }
    pComponentPrivate->nEmptyThisBufferCount++;

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
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate =
        (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
    ssize_t ret = 0;


    pPortDef = ((iLBCDEC_COMPONENT_PRIVATE*)
                pComponentPrivate)->pPortDef[iLBCD_OUTPUT_PORT];

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorInvalidState\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorInvalidState);
    }
#endif
    if(!pPortDef->bEnabled){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorIncorrectStateOperation\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorIncorrectStateOperation);
    }
    if (pBuffer == NULL){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorBadParameter\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorBadParameter);
    }
    if (pBuffer->nSize != sizeof(OMX_BUFFERHEADERTYPE)){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorBadParameter\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorBadParameter);
    }
    if (pBuffer->nVersion.nVersion != pComponentPrivate->nVersion){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorVersionMismatch\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorVersionMismatch);
    }
    if (pBuffer->nOutputPortIndex != iLBCD_OUTPUT_PORT){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorBadPortIndex\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorBadPortIndex);
    }
    iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->curState = %d\n",__LINE__,
                  __FUNCTION__,pComponentPrivate->curState);

    if(pComponentPrivate->curState != OMX_StateExecuting && 
       pComponentPrivate->curState != OMX_StatePause){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorIncorrectStateOperation\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorIncorrectStateOperation);
    }
    iLBCDEC_DPRINT("\n------------------------------------------\n\n");
    iLBCDEC_DPRINT ("%d :: %s :: Component Sending Emptied op buff %p \
                             to Component Thread\n",__LINE__,__FUNCTION__,
                             pBuffer);
    iLBCDEC_DPRINT("\n------------------------------------------\n\n");

    pComponentPrivate->app_nBuf--;

    if(pComponentPrivate->pMarkBuf){
        pBuffer->hMarkTargetComponent = pComponentPrivate->pMarkBuf->hMarkTargetComponent;
        pBuffer->pMarkData = pComponentPrivate->pMarkBuf->pMarkData;
        pComponentPrivate->pMarkBuf = NULL;
    }

    if (pComponentPrivate->pMarkData) {
        pBuffer->hMarkTargetComponent = pComponentPrivate->hMarkTargetComponent;
        pBuffer->pMarkData = pComponentPrivate->pMarkData;
        pComponentPrivate->pMarkData = NULL;
    }
    ret = write (pComponentPrivate->dataPipe[1], &pBuffer,
           sizeof (OMX_BUFFERHEADERTYPE*));
    if (ret == -1){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorHardware\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorHardware);
    }
    pComponentPrivate->nFillThisBufferCount++;

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
    int k=0, k2 = 0;

    /* inform audio manager to remove the streamID*/
    /* compose the data */
    OMX_COMPONENTTYPE *pComponent = (OMX_COMPONENTTYPE *)pHandle;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate =
        (iLBCDEC_COMPONENT_PRIVATE *)pComponent->pComponentPrivate;

    iLBCDEC_DPRINT ("%d :: %s ::ComponentDeInit\n",__LINE__,__FUNCTION__);

#ifdef DSP_RENDERING_ON
    close(pComponentPrivate->fdwrite);
    close(pComponentPrivate->fdread);
#endif

#ifdef RESOURCE_MANAGER_ENABLED
    eError = RMProxy_NewSendCommand(pHandle, RMProxy_FreeResource, OMX_ILBC_Decoder_COMPONENT, 0, NEWSENDCOMMAND_MEMORY, NULL);

    if (eError != OMX_ErrorNone) {
         iLBCDEC_EPRINT ("%d :: %s :: Error returned from destroy ResourceManagerProxy thread\n", __LINE__,__FUNCTION__);
    }
    eError = RMProxy_Deinitalize();
    if (eError != OMX_ErrorNone) {
         iLBCDEC_EPRINT ("%d :: %s :: Error from RMProxy_Deinitalize\n",  __LINE__,__FUNCTION__);
    }
#endif /*RESOURCE_MANAGER_ENABLED END*/

    iLBCDEC_DPRINT ("%d :: %s \n",__LINE__,__FUNCTION__);

    pComponentPrivate->bIsStopping = 1;

    /* Wait for thread to exit so we can get the status into "error" */
    k = pthread_join(pComponentPrivate->ComponentThread, (void*)&k2);
/*    eError = iLBCDEC_StopComponentThread(pHandle);*/
    iLBCDEC_DPRINT ("%d :: %s \n",__LINE__,__FUNCTION__);

    /* close the pipe handles */
    iLBCDEC_FreeCompResources(pHandle);
        
    // Free pComponentPrivate
    OMX_MEMFREE_STRUCT(pComponentPrivate);
    iLBCDEC_DPRINT ("%d :: %s \n",__LINE__,__FUNCTION__);
      
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
    iLBCDEC_DPRINT ("Inside the ComponentTunnelRequest\n");
    eError = OMX_ErrorNotImplemented;
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
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHeader = NULL;

    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    pPortDef = ((iLBCDEC_COMPONENT_PRIVATE*)
                pComponentPrivate)->pPortDef[nPortIndex];

#ifdef _ERROR_PROPAGATION__
    if (pComponentPrivate->curState == OMX_StateInvalid){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorInvalidState\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorInvalidState);
    }
#endif

    iLBCDEC_DPRINT ("%d :: %s :: pPortDef = 0x%p\n", __LINE__,
                   __FUNCTION__,pPortDef);
    iLBCDEC_DPRINT ("%d :: %s :: pPortDef->bEnabled = %d\n", __LINE__,
                   __FUNCTION__,pPortDef->bEnabled);

    if(!pPortDef->bEnabled) {
        pComponentPrivate->AlloBuf_waitingsignal = 1;
        pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex);
        pthread_cond_wait(&pComponentPrivate->AlloBuf_threshold,
                          &pComponentPrivate->AlloBuf_mutex);
        pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);
    }

    OMX_MALLOC_GENERIC(pBufferHeader,OMX_BUFFERHEADERTYPE);
    if (pBufferHeader == NULL) {
        iLBCDEC_EPRINT(":: ********* ERROR: OMX_ErrorInsufficientResources\n");
        return OMX_ErrorInsufficientResources;
    }

    OMX_MALLOC_SIZE_DSPALIGN(pBufferHeader->pBuffer, nSizeBytes, OMX_U8);
    if (pBufferHeader->pBuffer == NULL) {
        iLBCDEC_EPRINT(":: ********* ERROR: OMX_ErrorInsufficientResources\n");
        OMX_MEMFREE_STRUCT(pBufferHeader);
        return OMX_ErrorInsufficientResources;
    }

    if (nPortIndex == iLBCD_INPUT_PORT) {
        pBufferHeader->nInputPortIndex = nPortIndex;
        pBufferHeader->nOutputPortIndex = -1;
        pComponentPrivate->pInputBufferList->pBufHdr[pComponentPrivate->pInputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pInputBufferList->bBufferPending[pComponentPrivate->pInputBufferList->numBuffers] = 0;

        iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->pInputBufferList->pBufHdr[%d] = %p\n",__LINE__,
                      __FUNCTION__,
                      (int) pComponentPrivate->pInputBufferList->numBuffers,
                      pComponentPrivate->pInputBufferList->pBufHdr[pComponentPrivate->pInputBufferList->numBuffers]);

        pComponentPrivate->pInputBufferList->bufferOwner[pComponentPrivate->pInputBufferList->numBuffers++] = 1;

        iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
        iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->pInputBufferList->numBuffers = %d\n",__LINE__,
                      __FUNCTION__,
                      (int) pComponentPrivate->pInputBufferList->numBuffers);
        iLBCDEC_DPRINT("%d :: %s :: pPortDef->nBufferCountMin = %ld\n",__LINE__,
                      __FUNCTION__,pPortDef->nBufferCountMin);

        if (pComponentPrivate->pInputBufferList->numBuffers == 
            pPortDef->nBufferCountActual) {
            iLBCDEC_DPRINT("%d :: %s :: Setting pPortDef->bPopulated = OMX_TRUE for input port\n",__LINE__,__FUNCTION__);
            pPortDef->bPopulated = OMX_TRUE;
        }
    }
    else if (nPortIndex == iLBCD_OUTPUT_PORT) {
        pBufferHeader->nInputPortIndex = -1;
        pBufferHeader->nOutputPortIndex = nPortIndex; 

        OMX_MALLOC_GENERIC(pBufferHeader->pOutputPortPrivate,iLBCDEC_BUFDATA);
        if (pBufferHeader->pOutputPortPrivate == NULL) {
            iLBCDEC_EPRINT(":: ********* ERROR: OMX_ErrorInsufficientResources\n");
            OMX_MEMFREE_STRUCT_DSPALIGN(pBufferHeader->pBuffer, OMX_U8);
            OMX_MEMFREE_STRUCT(pBufferHeader);
            return OMX_ErrorInsufficientResources;
        }

        pComponentPrivate->pOutputBufferList->pBufHdr[pComponentPrivate->pOutputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pOutputBufferList->bBufferPending[pComponentPrivate->pOutputBufferList->numBuffers] = 0;

        iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->pOutputBufferList->pBufHdr[%d] = %p\n",__LINE__,__FUNCTION__,
                      pComponentPrivate->pOutputBufferList->numBuffers,
                      pComponentPrivate->pOutputBufferList->pBufHdr[pComponentPrivate->pOutputBufferList->numBuffers]);

        pComponentPrivate->pOutputBufferList->bufferOwner[pComponentPrivate->pOutputBufferList->numBuffers++] = 1;
        if (pComponentPrivate->pOutputBufferList->numBuffers == 
            pPortDef->nBufferCountActual) {
            iLBCDEC_DPRINT("%d :: %s :: Setting pPortDef->bPopulated = OMX_TRUE for input port\n",__LINE__,__FUNCTION__);
            pPortDef->bPopulated = OMX_TRUE;
        }
    }
    else {
        iLBCDEC_DPRINT("%d :: %s :: ERROR OMX_ErrorBadPortIndex\n",__LINE__,__FUNCTION__);
        OMX_MEMFREE_STRUCT_DSPALIGN(pBufferHeader->pBuffer, OMX_U8);
        OMX_MEMFREE_STRUCT(pBufferHeader);
        eError = OMX_ErrorBadPortIndex;
        goto EXIT;
    }

    /* Removing sleep() calls. Input buffer enabled and populated as well as output buffer. */
    if((pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bPopulated == 
        pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled)&&
       (pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bPopulated == 
        pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bEnabled) &&
       (pComponentPrivate->InLoaded_readytoidle)){
        pComponentPrivate->InLoaded_readytoidle = 0;
        pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex);
        pthread_cond_signal(&pComponentPrivate->InLoaded_threshold);
        pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
    }

    pBufferHeader->pAppPrivate = pAppPrivate;
    pBufferHeader->pPlatformPrivate = pComponentPrivate;
    pBufferHeader->nAllocLen = nSizeBytes;
    pBufferHeader->nVersion.s.nVersionMajor = iLBCDEC_MAJOR_VER;
    pBufferHeader->nVersion.s.nVersionMinor = iLBCDEC_MINOR_VER;
    pBufferHeader->nSize = sizeof(OMX_BUFFERHEADERTYPE);

    pComponentPrivate->nVersion = pBufferHeader->nVersion.nVersion;

    iLBCDEC_DPRINT("%d :: %s \n",__LINE__,__FUNCTION__);
    *pBuffer = pBufferHeader;

 EXIT:
    iLBCDEC_DPRINT("%d :: %s :: AllocateBuffer returning %d\n",__LINE__,
                  __FUNCTION__,eError);
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
    iLBCDEC_COMPONENT_PRIVATE * pComponentPrivate = NULL;
    OMX_BUFFERHEADERTYPE* buffHdr = NULL;
    OMX_U32 i;
    int bufferIndex = -1;
    iLBCD_BUFFERLIST *pBufferList = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
    OMX_COMPONENTTYPE *pHandle = NULL;

    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;

    if (nPortIndex != OMX_DirInput && nPortIndex != OMX_DirOutput) {
        iLBCDEC_EPRINT("%d :: Error - Unknown port index %ld\n", __LINE__, nPortIndex);
        return OMX_ErrorBadParameter;
    }

    pBufferList = ((nPortIndex == OMX_DirInput)? pComponentPrivate->pInputBufferList: pComponentPrivate->pOutputBufferList);
    pPortDef = pComponentPrivate->pPortDef[nPortIndex];
    for (i=0; i < pPortDef->nBufferCountActual; i++) {
        buffHdr = pBufferList->pBufHdr[i];
        if (buffHdr == pBuffer) {
            iLBCDEC_DPRINT("Found matching %s buffer\n", (nPortIndex == OMX_DirInput)? "input": "output");
            iLBCDEC_DPRINT("buffHdr = %p\n", buffHdr);
            iLBCDEC_DPRINT("pBuffer = %p\n", pBuffer);
            bufferIndex = i;
            break;
        }
        else {
            iLBCDEC_DPRINT("This is not a match\n");
            iLBCDEC_DPRINT("buffHdr = %p\n", buffHdr);
            iLBCDEC_DPRINT("pBuffer = %p\n", pBuffer);
        }
    }

    if (bufferIndex == -1) {
        iLBCDEC_EPRINT("%d :: Error - could not find match for buffer %p\n", __LINE__, pBuffer);
        return OMX_ErrorBadParameter;
    }

    if (pBufferList->bufferOwner[bufferIndex] == 1){
        OMX_MEMFREE_STRUCT_DSPALIGN(buffHdr->pBuffer, OMX_U8);
    }

    if (nPortIndex == OMX_DirOutput) {
        OMX_MEMFREE_STRUCT(buffHdr->pOutputPortPrivate);
    }

    iLBCDEC_DPRINT("%d: Freeing: %p Buf Header\n\n", __LINE__, buffHdr);
    OMX_MEMFREE_STRUCT(pBufferList->pBufHdr[bufferIndex]);
    pBufferList->numBuffers--;

    iLBCDEC_DPRINT("%d ::numBuffers = %d nBufferCountMin = %ld\n", __LINE__, pBufferList->numBuffers, pPortDef->nBufferCountMin);
    if (pBufferList->numBuffers < pPortDef->nBufferCountActual) {
        iLBCDEC_DPRINT("%d :: %s ::setting port %d populated to OMX_FALSE\n",__LINE__,__FUNCTION__, nPortIndex);
        pPortDef->bPopulated = OMX_FALSE;
    }

    if (pPortDef->bEnabled &&
        pComponentPrivate->bLoadedCommandPending == OMX_FALSE &&
        (pComponentPrivate->curState == OMX_StateIdle ||
         pComponentPrivate->curState == OMX_StateExecuting ||
         pComponentPrivate->curState == OMX_StatePause)) {
        pComponentPrivate->cbInfo.EventHandler(pHandle,
                                               pHandle->pApplicationPrivate,
                                               OMX_EventError,
                                               OMX_ErrorPortUnpopulated,
                                               nPortIndex, NULL);
    }

    if ((!pComponentPrivate->pInputBufferList->numBuffers &&
         !pComponentPrivate->pOutputBufferList->numBuffers) &&
        pComponentPrivate->InIdle_goingtoloaded){
        pComponentPrivate->InIdle_goingtoloaded = 0;
        pthread_mutex_lock(&pComponentPrivate->InIdle_mutex);
        pthread_cond_signal(&pComponentPrivate->InIdle_threshold);
        pthread_mutex_unlock(&pComponentPrivate->InIdle_mutex);
    }

    iLBCDEC_DPRINT("%d :: %s ::pComponentPrivate->bDisableCommandPending = %ld\n" ,__LINE__,__FUNCTION__,pComponentPrivate->bDisableCommandPending);

    if (pComponentPrivate->bDisableCommandPending && 
        (pComponentPrivate->pInputBufferList->numBuffers + 
         pComponentPrivate->pOutputBufferList->numBuffers == 0)) {
        SendCommand (pComponentPrivate->pHandle,
                     OMX_CommandPortDisable,
                     pComponentPrivate->bDisableCommandParam,NULL);
    }
    iLBCDEC_DPRINT ("%d :: %s :: Exiting FreeBuffer\n", __LINE__,__FUNCTION__);
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

static OMX_ERRORTYPE UseBuffer (OMX_IN OMX_HANDLETYPE hComponent,
                                OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                                OMX_IN OMX_U32 nPortIndex,
                                OMX_IN OMX_PTR pAppPrivate,
                                OMX_IN OMX_U32 nSizeBytes,
                                OMX_IN OMX_U8* pBuffer)
{
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef = NULL;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufferHeader = NULL;

    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);

    if (pComponentPrivate->curState == OMX_StateInvalid){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorInvalidState\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorInvalidState);
    }
    pPortDef = ((iLBCDEC_COMPONENT_PRIVATE*)
                pComponentPrivate)->pPortDef[nPortIndex];
    iLBCDEC_DPRINT("%d :: %s ::pPortDef->bPopulated = %d\n",
                  __LINE__,__FUNCTION__,pPortDef->bPopulated);

    if(!pPortDef->bEnabled){
        iLBCDEC_DPRINT("%d :: %s ::  OMX_ErrorIncorrectStateOperation\n", __LINE__,__FUNCTION__);
        return (OMX_ErrorIncorrectStateOperation);
    }

    OMX_MALLOC_GENERIC(pBufferHeader,OMX_BUFFERHEADERTYPE);

    if (nPortIndex == iLBCD_OUTPUT_PORT) {
        pBufferHeader->nInputPortIndex = -1;
        pBufferHeader->nOutputPortIndex = nPortIndex;

        OMX_MALLOC_GENERIC(pBufferHeader->pOutputPortPrivate,iLBCDEC_BUFDATA);

        pComponentPrivate->pOutputBufferList->pBufHdr[pComponentPrivate->pOutputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pOutputBufferList->bBufferPending[pComponentPrivate->pOutputBufferList->numBuffers] = 0;
        pComponentPrivate->pOutputBufferList->bufferOwner[pComponentPrivate->pOutputBufferList->numBuffers++] = 0;
        if (pComponentPrivate->pOutputBufferList->numBuffers == 
            pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
        }
    }
    else {
        pBufferHeader->nInputPortIndex = nPortIndex;
        pBufferHeader->nOutputPortIndex = -1;
        pComponentPrivate->pInputBufferList->pBufHdr[pComponentPrivate->pInputBufferList->numBuffers] = pBufferHeader;
        pComponentPrivate->pInputBufferList->bBufferPending[pComponentPrivate->pInputBufferList->numBuffers] = 0;
        pComponentPrivate->pInputBufferList->bufferOwner[pComponentPrivate->pInputBufferList->numBuffers++] = 0;
        if (pComponentPrivate->pInputBufferList->numBuffers == 
            pPortDef->nBufferCountActual) {
            pPortDef->bPopulated = OMX_TRUE;
        }
    }
    /* Removing sleep() calls. All enabled buffers are populated. */
    if((pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bPopulated == 
        pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled)&&
       (pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bPopulated == 
        pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bEnabled) &&
       (pComponentPrivate->InLoaded_readytoidle)){

        pComponentPrivate->InLoaded_readytoidle = 0;
        pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex);
        pthread_cond_signal(&pComponentPrivate->InLoaded_threshold);
        pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
    }

    pBufferHeader->pAppPrivate = pAppPrivate;
    pBufferHeader->pPlatformPrivate = pComponentPrivate;
    pBufferHeader->nAllocLen = nSizeBytes;
    pBufferHeader->nVersion.s.nVersionMajor = iLBCDEC_MAJOR_VER;
    pBufferHeader->nVersion.s.nVersionMinor = iLBCDEC_MINOR_VER;
    pBufferHeader->pBuffer = pBuffer;
    pBufferHeader->nSize = sizeof(OMX_BUFFERHEADERTYPE);
    pComponentPrivate->nVersion = pBufferHeader->nVersion.nVersion;

    *ppBufferHdr = pBufferHeader;
    iLBCDEC_DPRINT("pBufferHeader = %p\n",pBufferHeader);
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
static OMX_ERRORTYPE GetExtensionIndex(OMX_IN  OMX_HANDLETYPE hComponent,
                                       OMX_IN  OMX_STRING cParameterName,
                                       OMX_OUT OMX_INDEXTYPE *pIndexType)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if(!(strcmp(cParameterName, STRING_iLBC_HEADERINFO))) {
        *pIndexType = OMX_IndexCustomiLBCDecHeaderInfoConfig;
    }
    else if(!(strcmp(cParameterName,STRING_iLBC_STREAMIDINFO))){
        *pIndexType = OMX_IndexCustomiLBCDecStreamIDConfig;
    }
    else if(!(strcmp(cParameterName,STRING_iLBC_DATAPATHINFO))){
        *pIndexType = OMX_IndexCustomiLBCDecDataPath;
    }
    else{
        eError = OMX_ErrorBadParameter;
    }
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
static OMX_ERRORTYPE ComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent,
                                       OMX_OUT OMX_U8 *cRole,
                                       OMX_IN OMX_U32 nIndex)
{
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE *)
        (((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate);
    if (cRole == NULL)
    	eError = OMX_ErrorBadParameter;
    else if(nIndex == 0){
            memcpy(cRole, &pComponentPrivate->componentRole.cRole, 
                   sizeof(OMX_U8) * OMX_MAX_STRINGNAME_SIZE); 
            iLBCDEC_DPRINT("::::In ComponenetRoleEnum: cRole is set to %s\n",cRole);
    }
    else {
        eError = OMX_ErrorNoMore;
    }
    return eError;
}


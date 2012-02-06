
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
* @file OMX_iLBCEnc_Utils.c
*
* This file implements iLBC Encoder Component Specific APIs and its functionality
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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <malloc.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>
#endif

#include <dbapi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/*-------program files ----------------------------------------*/
#include "OMX_iLBCEnc_Utils.h"
#include "ilbcencsocket_ti.h"
#include "encode_common_ti.h"
#include "usn.h"
#include "LCML_DspCodec.h"
#ifdef RESOURCE_MANAGER_ENABLED
    #include <ResourceManagerProxyAPI.h>
#endif

/* ========================================================================== */
/**
* @ILBCENC_FillLCMLInitParams () This function is used by the component thread to
* fill the all of its initialization parameters, buffer deatils  etc
* to LCML structure,
*
* @param pComponent  handle for this instance of the component
* @param plcml_Init  pointer to LCML structure to be filled
*
* @pre
*
* @post
*
* @return none
*/
/* ========================================================================== */
OMX_ERRORTYPE ILBCENC_FillLCMLInitParams(OMX_HANDLETYPE pComponent,
                                  LCML_DSP *plcml_Init, OMX_U16 arr[])
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0,nIpBufSize = 0,nOpBuf = 0,nOpBufSize = 0;
    OMX_BUFFERHEADERTYPE *pTemp = NULL;
    LCML_DSP_INTERFACE *pHandle = (LCML_DSP_INTERFACE *)pComponent;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = pHandle->pComponentPrivate;
    ILBCENC_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;
    OMX_U16 i = 0;
    OMX_U32 size_lcml = 0;

    ILBCENC_DPRINT("%d Entering ILBCENC_FillLCMLInitParams\n",__LINE__);

    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nIpBufSize = pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->nBufferSize;
    pComponentPrivate->nRuntimeInputBuffers = nIpBuf;

    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    nOpBufSize = pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->nBufferSize;
    pComponentPrivate->nRuntimeOutputBuffers = nOpBuf;

    ILBCENC_DPRINT("%d ------ Buffer Details -----------\n",__LINE__);
    ILBCENC_DPRINT("%d Input  Buffer Count = %ld\n",__LINE__,nIpBuf);
    ILBCENC_DPRINT("%d Input  Buffer Size = %ld\n",__LINE__,nIpBufSize);
    ILBCENC_DPRINT("%d Output Buffer Count = %ld\n",__LINE__,nOpBuf);
    ILBCENC_DPRINT("%d Output Buffer Size = %ld\n",__LINE__,nOpBufSize);
    ILBCENC_DPRINT("%d ------ Buffer Details ------------\n",__LINE__);

    /* Fill Input Buffers Info for LCML */
    plcml_Init->In_BufInfo.nBuffers = nIpBuf;
    plcml_Init->In_BufInfo.nSize = nIpBufSize;
    plcml_Init->In_BufInfo.DataTrMethod = DMM_METHOD;

    /* Fill Output Buffers Info for LCML */
    plcml_Init->Out_BufInfo.nBuffers = nOpBuf;
    plcml_Init->Out_BufInfo.nSize = nOpBufSize;
    plcml_Init->Out_BufInfo.DataTrMethod = DMM_METHOD;

    /*Copy the node information*/
    plcml_Init->NodeInfo.nNumOfDLLs = 3;

    plcml_Init->NodeInfo.AllUUIDs[0].uuid = &ILBCENCSOCKET_TI_UUID;
    strcpy ((char *) plcml_Init->NodeInfo.AllUUIDs[0].DllName,ILBCENC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[0].eDllType = DLL_NODEOBJECT;

    plcml_Init->NodeInfo.AllUUIDs[1].uuid = &ILBCENCSOCKET_TI_UUID;
    strcpy ((char *) plcml_Init->NodeInfo.AllUUIDs[1].DllName,ILBCENC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[1].eDllType = DLL_DEPENDENT;

    plcml_Init->NodeInfo.AllUUIDs[2].uuid = &USN_TI_UUID;
    strcpy ((char *) plcml_Init->NodeInfo.AllUUIDs[2].DllName,ILBCENC_USN_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[2].eDllType = DLL_DEPENDENT;
    plcml_Init->DeviceInfo.TypeofDevice = 0;

    if(pComponentPrivate->dasfMode == 1) {
        ILBCENC_DPRINT("%d Codec is configuring to DASF mode\n",__LINE__);
        OMX_MALLOC_GENERIC(pComponentPrivate->strmAttr, LCML_STRMATTR);
        pComponentPrivate->strmAttr->uSegid = ILBCENC_DEFAULT_SEGMENT;
        pComponentPrivate->strmAttr->uAlignment = 0;
        pComponentPrivate->strmAttr->uTimeout = ILBCENC_SN_TIMEOUT;
        pComponentPrivate->strmAttr->uBufsize = pComponentPrivate->inputBufferSize;
        pComponentPrivate->strmAttr->uNumBufs = ILBCENC_NUM_INPUT_BUFFERS_DASF;
        pComponentPrivate->strmAttr->lMode = STRMMODE_PROCCOPY;
        /* Device is Configuring to DASF Mode */
        plcml_Init->DeviceInfo.TypeofDevice = 1;
        /* Device is Configuring to Record Mode */
        plcml_Init->DeviceInfo.TypeofRender = 1;
        if(pComponentPrivate->acdnMode == 1) {
            /* ACDN mode */
        }
        else {
            /* DASF/TeeDN mode */
            plcml_Init->DeviceInfo.AllUUIDs[0].uuid = &DCTN_TI_UUID;
        }
        plcml_Init->DeviceInfo.DspStream = pComponentPrivate->strmAttr;
    }
    

    /*copy the other information*/
    plcml_Init->SegID = ILBCENC_DEFAULT_SEGMENT;
    plcml_Init->Timeout = ILBCENC_SN_TIMEOUT;
    plcml_Init->Alignment = 0;
    plcml_Init->Priority = ILBCENC_SN_PRIORITY;
    plcml_Init->ProfileID = 0;
    
    /* Setting Creat Phase Parameters here */
    arr[0] = ILBCENC_STREAM_COUNT;
    arr[1] = ILBCENC_INPUT_PORT;

    if(pComponentPrivate->dasfMode == 1) {
        arr[2] = ILBCENC_INSTRM;
        arr[3] = ILBCENC_NUM_INPUT_BUFFERS_DASF;
    }
    else {
        arr[2] = ILBCENC_DMM;
        if (pComponentPrivate->pInputBufferList->numBuffers) {
            arr[3] = (OMX_U16) pComponentPrivate->pInputBufferList->numBuffers;
        }
        else {
            arr[3] = 1;
        }
    }

    arr[4] = ILBCENC_OUTPUT_PORT;
    arr[5] = ILBCENC_DMM;
    if (pComponentPrivate->pOutputBufferList->numBuffers) {
        arr[6] = (OMX_U16) pComponentPrivate->pOutputBufferList->numBuffers;
    }
    else {
        arr[6] = 1;
    }

    arr[7] = (OMX_U16) pComponentPrivate->codecType;

    arr[8] = END_OF_CR_PHASE_ARGS;

    plcml_Init->pCrPhArgs = arr;

    /* Allocate memory for all input buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = nIpBuf * sizeof(ILBCENC_LCML_BUFHEADERTYPE);
    
    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml,ILBCENC_LCML_BUFHEADERTYPE);

    pComponentPrivate->pLcmlBufHeader[ILBCENC_INPUT_PORT] = pTemp_lcml;
    for (i=0; i<nIpBuf; i++) {
        ILBCENC_DPRINT("%d INPUT--------- Inside Ip Loop\n",__LINE__);
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);

        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = ILBCENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = ILBCENC_MINOR_VER;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = ILBCENC_NOT_USED;
        pTemp_lcml->buffer = pTemp;
        ILBCENC_DPRINT("%d pTemp_lcml->buffer->pBuffer = %p \n",__LINE__,pTemp_lcml->buffer->pBuffer);
        pTemp_lcml->eDir = OMX_DirInput;

        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pBufferParam, sizeof(ILBCENC_ParamStruct), ILBCENC_ParamStruct);
        if(NULL == pTemp_lcml->pBufferParam){
            ILBCENC_CleanupInitParams(pComponent);
            return OMX_ErrorInsufficientResources;
        }
        pTemp_lcml->pBufferParam->usNbFrames=0;
        pTemp_lcml->pBufferParam->pParamElem=NULL;
        pTemp_lcml->pFrameParam=NULL;
        
        OMX_MALLOC_GENERIC(pTemp_lcml->pDmmBuf, DMM_BUFFER_OBJ);

        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */
        pTemp->nFlags = ILBCENC_NORMAL_BUFFER;
        pTemp_lcml++;
    }

    /* Allocate memory for all output buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = nOpBuf * sizeof(ILBCENC_LCML_BUFHEADERTYPE);

    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml,ILBCENC_LCML_BUFHEADERTYPE);
    ILBCENC_MEMPRINT("%d [ALLOC] %p\n",__LINE__,pTemp_lcml);

    pComponentPrivate->pLcmlBufHeader[ILBCENC_OUTPUT_PORT] = pTemp_lcml;
    for (i=0; i<nOpBuf; i++) {
        ILBCENC_DPRINT("%d OUTPUT--------- Inside Op Loop\n",__LINE__);
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        /*        pTemp->nAllocLen = nOpBufSize;*/
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = ILBCENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = ILBCENC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = ILBCENC_NOT_USED;
        pTemp_lcml->buffer = pTemp;
        ILBCENC_DPRINT("%d pTemp_lcml->buffer->pBuffer = %p \n",__LINE__,pTemp_lcml->buffer->pBuffer);
        pTemp_lcml->eDir = OMX_DirOutput;
        
        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pBufferParam,sizeof(ILBCENC_ParamStruct),ILBCENC_ParamStruct);
        if(NULL == pTemp_lcml->pBufferParam){
            ILBCENC_CleanupInitParams(pComponent);
            return OMX_ErrorInsufficientResources;
        }
        pTemp_lcml->pBufferParam->usNbFrames=0;
        pTemp_lcml->pBufferParam->pParamElem=NULL;
        pTemp_lcml->pFrameParam=NULL;
        
        OMX_MALLOC_GENERIC(pTemp_lcml->pDmmBuf, DMM_BUFFER_OBJ);

        pTemp->nFlags = ILBCENC_NORMAL_BUFFER;
        pTemp_lcml++;
    }
#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate->nLcml_nCntIp = 0;
    pComponentPrivate->nLcml_nCntOpReceived = 0;
#endif

    pComponentPrivate->bInitParamsInitialized = 1;
 EXIT:
    if(eError != OMX_ErrorNone)
        ILBCENC_CleanupInitParams(pComponent);
    ILBCENC_DPRINT("%d Exiting ILBCENC_FillLCMLInitParams\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
* @ILBCENC_StartComponentThread() This function is called by the component to create
* the component thread, command pipes, data pipes and LCML Pipes.
*
* @param pComponent  handle for this instance of the component
*
* @pre
*
* @post
*
* @return none
*/
/* ========================================================================== */

OMX_ERRORTYPE ILBCENC_StartComponentThread(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate =
        (ILBCENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
#ifdef UNDER_CE
    pthread_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.__inheritsched = PTHREAD_EXPLICIT_SCHED;
    attr.__schedparam.__sched_priority = OMX_AUDIO_ENCODER_THREAD_PRIORITY;
#endif

    ILBCENC_DPRINT ("%d Entering  ILBCENC_StartComponentThread\n", __LINE__);
    /* Initialize all the variables*/
    pComponentPrivate->bIsThreadstop = 0;
    pComponentPrivate->lcml_nOpBuf = 0;
    pComponentPrivate->lcml_nIpBuf = 0;
    pComponentPrivate->app_nBuf = 0;

    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->cmdDataPipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        ILBCENC_EPRINT("%d Error while creating cmdDataPipe\n",__LINE__);
        goto EXIT;
    }
    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->dataPipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        ILBCENC_EPRINT("%d Error while creating dataPipe\n",__LINE__);
        goto EXIT;
    }

    /* create the pipe used to send commands to the thread */
    eError = pipe (pComponentPrivate->cmdPipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        ILBCENC_EPRINT("%d Error while creating cmdPipe\n",__LINE__);
        goto EXIT;
    }

    /* Create the Component Thread */
#ifdef UNDER_CE
    eError = pthread_create (&(pComponentPrivate->ComponentThread), &attr, ILBCENC_CompThread, pComponentPrivate);
#else
    eError = pthread_create (&(pComponentPrivate->ComponentThread), NULL, ILBCENC_CompThread, pComponentPrivate);
#endif
    if (eError || !pComponentPrivate->ComponentThread) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pComponentPrivate->bCompThreadStarted = 1;
 EXIT:
    ILBCENC_DPRINT("%d Exiting ILBCENC_StartComponentThread\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
* @ILBCENC_FreeCompResources() This function is called by the component during
* de-init , to newfree Command pipe, data pipe & LCML pipe.
*
* @param pComponent  handle for this instance of the component
*
* @pre
*
* @post
*
* @return none
*/
/* ========================================================================== */

OMX_ERRORTYPE ILBCENC_FreeCompResources(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;
    ILBCENC_DPRINT("%d Entering ILBCENC_FreeCompResources\n",__LINE__);

    if (pComponentPrivate->bCompThreadStarted) {
        OMX_ILBCCLOSE_PIPE(pComponentPrivate->dataPipe[0],err);
        OMX_ILBCCLOSE_PIPE(pComponentPrivate->dataPipe[1],err);
        OMX_ILBCCLOSE_PIPE(pComponentPrivate->cmdPipe[0],err);
        OMX_ILBCCLOSE_PIPE(pComponentPrivate->cmdPipe[1],err);
        OMX_ILBCCLOSE_PIPE(pComponentPrivate->cmdDataPipe[0],err);
        OMX_ILBCCLOSE_PIPE(pComponentPrivate->cmdDataPipe[1],err);
    }

    OMX_MEMFREE_STRUCT(pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pcmParams);
    OMX_MEMFREE_STRUCT(pComponentPrivate->ilbcParams);

    OMX_MEMFREE_STRUCT(pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT]->pPortFormat);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT]->pPortFormat);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pCompPort[ILBCENC_INPUT_PORT]);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pCompPort[ILBCENC_OUTPUT_PORT]);

    OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->pParams, ILBCENC_AudioCodecParams);

    OMX_MEMFREE_STRUCT(pComponentPrivate->sPortParam);
    OMX_MEMFREE_STRUCT(pComponentPrivate->sPriorityMgmt);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pInputBufferList);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pOutputBufferList);
    if (pComponentPrivate->pLcmlHandle != NULL) {
        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle)->pCodecinterfacehandle,
                               EMMCodecControlDestroy, NULL);

        if (eError != OMX_ErrorNone){
            ILBCENC_DPRINT("%d : Error: in Destroying the codec: no.  %x\n",__LINE__, eError);
        }
        if (pComponentPrivate->ptrLibLCML != NULL)
            {
            ILBCENC_DPRINT("%d OMX_iLBCEncoder.c Closing LCML library\n",__LINE__);
            dlclose( pComponentPrivate->ptrLibLCML);
            pComponentPrivate->ptrLibLCML = NULL;
        }
        pComponentPrivate->pLcmlHandle = NULL;
    }

 EXIT:
    ILBCENC_DPRINT("%d Exiting ILBCENC_FreeCompResources()\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
* @ILBCENC_CleanupInitParams() This function is called by the component during
* de-init to newfree structues that are been allocated at intialization stage
*
* @param pComponent  handle for this instance of the component
*
* @pre
*
* @post
*
* @return none
*/
/* ========================================================================== */

OMX_ERRORTYPE ILBCENC_CleanupInitParams(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0;
    OMX_U32 nOpBuf = 0;
    OMX_U16 i = 0;
    ILBCENC_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;
    LCML_DSP_INTERFACE *pLcmlHandle = NULL;
    LCML_DSP_INTERFACE *pLcmlHandleAux = NULL;

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;
    ILBCENC_DPRINT("%d Entering ILBCENC_CleanupInitParams()\n", __LINE__);

    if(pComponentPrivate->dasfMode == 1) {
        OMX_MEMFREE_STRUCT(pComponentPrivate->strmAttr);
    }

    pComponentPrivate->nHoldLength = 0;
    OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer2);
    OMX_MEMFREE_STRUCT(pComponentPrivate->iHoldBuffer);

    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[ILBCENC_INPUT_PORT];
    nIpBuf = pComponentPrivate->nRuntimeInputBuffers;

    for(i=0; i<nIpBuf; i++) {
        if(pTemp_lcml->pFrameParam!=NULL){
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
            pLcmlHandleAux = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);
            OMX_DmmUnMap(pLcmlHandleAux->dspCodec->hProc,
                         (void*)pTemp_lcml->pBufferParam->pParamElem,
                         pTemp_lcml->pDmmBuf->pReserved);

            OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pFrameParam, ILBCENC_FrameStruct);
        }

        OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pBufferParam, ILBCENC_ParamStruct);

        OMX_MEMFREE_STRUCT(pTemp_lcml->pDmmBuf);

        pTemp_lcml++;
    }


    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[ILBCENC_OUTPUT_PORT];
    nOpBuf = pComponentPrivate->nRuntimeOutputBuffers;

    for(i=0; i<nOpBuf; i++) {
        if(pTemp_lcml->pFrameParam!=NULL){
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
            pLcmlHandleAux = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);
#ifndef UNDER_CE
            OMX_DmmUnMap(pLcmlHandleAux->dspCodec->hProc,
                         (void*)pTemp_lcml->pBufferParam->pParamElem,
                         pTemp_lcml->pDmmBuf->pReserved);
#endif
              
            OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pFrameParam, ILBCENC_FrameStruct);
        }

        OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pBufferParam,ILBCENC_ParamStruct);
        OMX_MEMFREE_STRUCT(pTemp_lcml->pDmmBuf);

        pTemp_lcml++;
    }


    OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[ILBCENC_INPUT_PORT]);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[ILBCENC_OUTPUT_PORT]);

    ILBCENC_DPRINT("%d Exiting ILBCENC_CleanupInitParams()\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
* @ILBCENC_StopComponentThread() This function is called by the component during
* de-init to close component thread.
*
* @param pComponent  handle for this instance of the component
*
* @pre
*
* @post
*
* @return none
*/
/* ========================================================================== */

OMX_ERRORTYPE ILBCENC_StopComponentThread(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE threadError = OMX_ErrorNone;
    int pthreadError = 0;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;
    ILBCENC_DPRINT("%d Entering ILBCENC_StopComponentThread\n",__LINE__);
    pComponentPrivate->bIsThreadstop = 1;
    ILBCENC_DPRINT("%d About to call pthread_join\n",__LINE__);
    pthreadError = pthread_join (pComponentPrivate->ComponentThread,
                                 (void*)&threadError);
    if (0 != pthreadError) {
        eError = OMX_ErrorHardware;
        ILBCENC_EPRINT("%d Error closing ComponentThread - pthreadError = %d\n",__LINE__,pthreadError);
        goto EXIT;
    }
    if (OMX_ErrorNone != threadError && OMX_ErrorNone != eError) {
        eError = OMX_ErrorInsufficientResources;
        ILBCENC_EPRINT("%d Error while closing Component Thread\n",__LINE__);
        goto EXIT;
    }
 EXIT:
    ILBCENC_DPRINT("%d Exiting ILBCENC_StopComponentThread\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}


/* ========================================================================== */
/**
* @ILBCENC_HandleCommand() This function is called by the component when ever it
* receives the command from the application
*
* @param pComponentPrivate  Component private data
*
* @pre
*
* @post
*
* @return none
*/
/* ========================================================================== */

OMX_U32 ILBCENC_HandleCommand (ILBCENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMMANDTYPE command;
    OMX_STATETYPE commandedState = OMX_StateInvalid;
    OMX_HANDLETYPE pLcmlHandle;
#ifdef RESOURCE_MANAGER_ENABLED
    OMX_ERRORTYPE rm_error = OMX_ErrorNone;
#endif
    LCML_CALLBACKTYPE cb;
    LCML_DSP *pLcmlDsp = NULL;
    OMX_U32 pValues[4] = {0};
    OMX_U32 commandData = 0;
    OMX_U16 arr[100] = {0};
    char *p = "";
    OMX_U16 i = 0;
    OMX_S32 ret = 0;
    OMX_U8 inputPortFlag=0,outputPortFlag=0;    
    ILBCENC_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    pLcmlHandle = pComponentPrivate->pLcmlHandle;

    ILBCENC_DPRINT("%d Entering GMSFRENCHandleCommand Function \n",__LINE__);
    ILBCENC_DPRINT("%d pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);
    ret = read(pComponentPrivate->cmdPipe[0], &command, sizeof (command));
    if (ret == -1) {
        ILBCENC_EPRINT("%d Error in Reading from the Data pipe\n", __LINE__);
        eError = OMX_ErrorHardware;
        goto EXIT;
    }
    ret = read(pComponentPrivate->cmdDataPipe[0], &commandData, sizeof (commandData));
    if (ret == -1) {
        ILBCENC_EPRINT("%d Error in Reading from the Data pipe\n", __LINE__);
        eError = OMX_ErrorHardware;
        goto EXIT;
    }

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedCommand(pComponentPrivate->pPERFcomp,
                         command,
                         commandData,
                         PERF_ModuleLLMM);
#endif
    if (command == OMX_CommandStateSet) {
        commandedState = (OMX_STATETYPE)commandData;
        if (pComponentPrivate->curState == commandedState){
            pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                     pHandle->pApplicationPrivate,
                                                     OMX_EventError,
                                                     OMX_ErrorSameState,
                                                     0,
                                                     NULL);
            ILBCENC_EPRINT("%d Error: Same State Given by Application\n",__LINE__);
        }else {
            switch(commandedState) {
            case OMX_StateIdle:
                ILBCENC_DPRINT("%d ILBCENC_HandleCommand :: OMX_StateIdle \n",__LINE__);
                ILBCENC_DPRINT("%d pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);
                if (pComponentPrivate->curState == OMX_StateLoaded ||
                    pComponentPrivate->curState == OMX_StateWaitForResources) {
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundarySetup);
#endif

                    if(pComponentPrivate->dasfMode == 1) {
                        if(pComponentPrivate->streamID == 0){
                            ILBCENC_EPRINT("**************************************\n");
                            ILBCENC_EPRINT(":: Error = OMX_ErrorInsufficientResources\n");
                            ILBCENC_EPRINT("**************************************\n");
                            eError = OMX_ErrorInsufficientResources;
                            pComponentPrivate->curState = OMX_StateInvalid;
                            pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                                pHandle->pApplicationPrivate,
                                                               OMX_EventError, 
                                                               OMX_ErrorInvalidState,
                                                               0, 
                                                               NULL);
                            goto EXIT;
                        }
                    }

                    if (pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bPopulated &&  
                        pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bEnabled)  {
                        inputPortFlag = 1;
                    }
                    if (pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated && 
                        pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bEnabled) {
                        outputPortFlag = 1;
                    }

                    if (!pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bPopulated && 
                        !pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bEnabled) {
                        inputPortFlag = 1;
                    }               
                    
                    if (!pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated && 
                        !pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bEnabled) {
                        outputPortFlag = 1;
                    }

                    if(!(inputPortFlag && outputPortFlag)){
                        /* Sleep for a while, so the application thread can allocate buffers */
                        ILBCENC_DPRINT("%d Sleeping...\n",__LINE__);
                        pComponentPrivate->InLoaded_readytoidle = 1;
#ifndef UNDER_CE
                        pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex); 
                        pthread_cond_wait(&pComponentPrivate->InLoaded_threshold, 
                                            &pComponentPrivate->InLoaded_mutex);
                        pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
#else
                        OMX_WaitForEvent(&(pComponentPrivate->InLoaded_event));
#endif
                    }   

                    pLcmlHandle = (OMX_HANDLETYPE) ILBCENC_GetLCMLHandle(pComponentPrivate);
                    if (pLcmlHandle == NULL) {
                        ILBCENC_DPRINT("%d LCML Handle is NULL........exiting..\n",__LINE__);
                        goto EXIT;
                    }
                    pComponentPrivate->pLcmlHandle = (LCML_DSP_INTERFACE *)pLcmlHandle;

                    /* Got handle of dsp via phandle filling information about DSP Specific things */
                    pLcmlDsp = (((LCML_DSP_INTERFACE*)pLcmlHandle)->dspCodec);
                    eError = ILBCENC_FillLCMLInitParams(pHandle, pLcmlDsp, arr);
                    if(eError != OMX_ErrorNone) {
                        ILBCENC_EPRINT("%d Error from ILBCENCFill_LCMLInitParams()\n",__LINE__);
                        goto EXIT;
                    }

#ifdef RESOURCE_MANAGER_ENABLED
                    /* Need check the resource with RM */
                    pComponentPrivate->rmproxyCallback.RMPROXY_Callback = 
                        (void *) ILBCENC_ResourceManagerCallback;
                    if (pComponentPrivate->curState != OMX_StateWaitForResources) {
                        rm_error = RMProxy_NewSendCommand(pHandle, 
                                                          RMProxy_RequestResource, 
                                                          OMX_ILBC_Encoder_COMPONENT, 
                                                          ILBCENC_CPU_LOAD, 
                                                          3456, 
                                                          NULL);
                        if (rm_error == OMX_ErrorInsufficientResources) {
                            /* resource is not available, need set state to OMX_StateWaitForResources */
                            ILBCENC_EPRINT("%d Comp: OMX_ErrorInsufficientResources\n", __LINE__);
                            pComponentPrivate->curState = OMX_StateWaitForResources;
                            pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                                    pHandle->pApplicationPrivate,
                                                                    OMX_EventCmdComplete,
                                                                    OMX_CommandStateSet,
                                                                    pComponentPrivate->curState,
                                                                    NULL);
                            return rm_error;
                        } else if (rm_error != OMX_ErrorNone) {
                            ILBCENC_EPRINT("%d Comp: OMX_StateInvalid\n", __LINE__);
                            eError = OMX_ErrorInsufficientResources;
                            pComponentPrivate->curState = OMX_StateInvalid;
                            pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                                    pHandle->pApplicationPrivate,
                                                                    OMX_EventError,
                                                                    eError,
                                                                    OMX_TI_ErrorSevere,
                                                                    NULL);
                            return eError;
                        }
                    }
#endif

                    cb.LCML_Callback = (void *) ILBCENC_LCMLCallback;
#ifndef UNDER_CE
                    eError = LCML_InitMMCodecEx(((LCML_DSP_INTERFACE *)pLcmlHandle)->pCodecinterfacehandle,
                                                p,&pLcmlHandle,(void *)p,&cb, (OMX_STRING)pComponentPrivate->sDeviceString);

#else
                    eError = LCML_InitMMCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                              p,&pLcmlHandle, (void *)p, &cb);
#endif

                    if (eError != OMX_ErrorNone) {
                        ILBCENC_EPRINT("%d Error returned from LCML_Init()\n",__LINE__);
                        ILBCENC_FatalErrorRecover(pComponentPrivate);
                        goto EXIT;
                    }

#ifdef RESOURCE_MANAGER_ENABLED
                    /* resource is available */
                    ILBCENC_DPRINT("%d %s Resource available, set component to OMX_StateIdle\n", __LINE__, __FUNCTION__);
                    rm_error = RMProxy_NewSendCommand(pHandle,
                                                      RMProxy_StateSet,
                                                      OMX_ILBC_Encoder_COMPONENT,
                                                      OMX_StateIdle,
                                                      3456,
                                                      NULL);
#endif
                    pComponentPrivate->curState = OMX_StateIdle;
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventCmdComplete,
                                                            OMX_CommandStateSet,
                                                            pComponentPrivate->curState,
                                                            NULL);
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySetup);
#endif
                }
                else if (pComponentPrivate->curState == OMX_StateExecuting) {
                    ILBCENC_DPRINT("%d Setting Component to OMX_StateIdle\n",__LINE__);
                    ILBCENC_DPRINT("%d ILBCENC: About to Call MMCodecControlStop\n", __LINE__);
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif
                    pComponentPrivate->InBuf_Eos_alreadysent  =0;
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               MMCodecControlStop,(void *)p);
        
                    OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->pParams, ILBCENC_AudioCodecParams);

                    if(eError != OMX_ErrorNone) {
                        ILBCENC_EPRINT("%d Error from LCML_ControlCodec MMCodecControlStop..\n",__LINE__);
                        goto EXIT;
                    }
                }
                else if(pComponentPrivate->curState == OMX_StatePause) {
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif
#ifdef RESOURCE_MANAGER_ENABLED
                    rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_ILBC_Encoder_COMPONENT, OMX_StateIdle, 3456, NULL);
#endif
                    ILBCENC_DPRINT ("%d The component is stopped\n",__LINE__);
                    pComponentPrivate->curState = OMX_StateIdle;
                    pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                             pHandle->pApplicationPrivate,
                                                             OMX_EventCmdComplete,
                                                             OMX_CommandStateSet,
                                                             pComponentPrivate->curState,
                                                             NULL);
                } else {    /* This means, it is invalid state from application */
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventError,
                                                            OMX_ErrorIncorrectStateTransition,
                                                            0,
                                                            NULL);
                    ILBCENC_EPRINT("%d Comp: OMX_ErrorIncorrectStateTransition\n",__LINE__);
                }

                break;

            case OMX_StateExecuting:
                ILBCENC_DPRINT("%d ILBCENC_HandleCommand :: OMX_StateExecuting \n",__LINE__);
                if (pComponentPrivate->curState == OMX_StateIdle) {
                    /* Sending commands to DSP via LCML_ControlCodec third argument
                       is not used for time being */
                    pComponentPrivate->nNumInputBufPending = 0;
                    pComponentPrivate->nNumOutputBufPending = 0;
                    pComponentPrivate->nNumOfFramesSent=0;
                    pComponentPrivate->nEmptyBufferDoneCount = 0;
                    pComponentPrivate->nEmptyThisBufferCount =0;

                    if(pComponentPrivate->dasfMode == 1) {
                        ILBCENC_DPRINT("%d ---- Comp: DASF Functionality is ON ---\n",__LINE__);

                        OMX_MALLOC_SIZE_DSPALIGN(pComponentPrivate->pParams, sizeof(ILBCENC_AudioCodecParams), ILBCENC_AudioCodecParams);
                        if(NULL == pComponentPrivate->pParams){
                            return OMX_ErrorInsufficientResources;
                        }
                        pComponentPrivate->pParams->iAudioFormat = 1;
                        pComponentPrivate->pParams->iStrmId = pComponentPrivate->streamID;
                        pComponentPrivate->pParams->iSamplingRate = ILBCENC_SAMPLING_FREQUENCY;
                        pValues[0] = USN_STRMCMD_SETCODECPARAMS;
                        pValues[1] = (OMX_U32)pComponentPrivate->pParams;
                        pValues[2] = sizeof(ILBCENC_AudioCodecParams);

                        /* Sending StrmCtrl MESSAGE to DSP via LCML_ControlCodec*/
                        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                   EMMCodecControlStrmCtrl,(void *)pValues);
                        if(eError != OMX_ErrorNone) {
                            ILBCENC_EPRINT("%d Error from LCML_ControlCodec EMMCodecControlStrmCtrl = %x\n",__LINE__,eError);
                            goto EXIT;
                        }
                    }
                    /* Sending START MESSAGE to DSP via LCML_ControlCodec*/
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStart, (void *)p);
                    if(eError != OMX_ErrorNone) {
                        ILBCENC_EPRINT("%d Error from LCML_ControlCodec EMMCodecControlStart = %x\n",__LINE__,eError);
                        goto EXIT;
                    }

                } else if (pComponentPrivate->curState == OMX_StatePause) {
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStart, (void *)p);
                    if (eError != OMX_ErrorNone) {
                        ILBCENC_EPRINT("%d Error While Resuming the codec = %x\n",__LINE__,eError);
                        goto EXIT;
                    }                               

                    for (i=0; i < pComponentPrivate->nNumInputBufPending; i++) {
                        if (pComponentPrivate->pInputBufHdrPending[i]) {
                            ILBCENC_GetCorrespondingLCMLHeader(pComponentPrivate, pComponentPrivate->pInputBufHdrPending[i]->pBuffer, OMX_DirInput, &pLcmlHdr);
                            ILBCENC_SetPending(pComponentPrivate,pComponentPrivate->pInputBufHdrPending[i],OMX_DirInput,__LINE__);

                            eError = LCML_QueueBuffer(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                      EMMCodecInputBuffer,
                                                      pComponentPrivate->pInputBufHdrPending[i]->pBuffer,
                                                      pComponentPrivate->pInputBufHdrPending[i]->nAllocLen,
                                                      pComponentPrivate->pInputBufHdrPending[i]->nFilledLen,
                                                      (OMX_U8 *) pLcmlHdr->pBufferParam,
                                                      sizeof(ILBCENC_ParamStruct),
                                                      NULL);
                            if (eError != OMX_ErrorNone) {
                                ILBCENC_EPRINT("%d :: %s Queue Input buffer, error\n", __LINE__, __FUNCTION__);
                                ILBCENC_FatalErrorRecover(pComponentPrivate);
                                goto EXIT;
                            }
                        }
                    }
                    pComponentPrivate->nNumInputBufPending = 0;

                    for (i=0; i < pComponentPrivate->nNumOutputBufPending; i++) {
                        if (pComponentPrivate->pOutputBufHdrPending[i]) {
                            ILBCENC_GetCorrespondingLCMLHeader(pComponentPrivate,pComponentPrivate->pOutputBufHdrPending[i]->pBuffer, OMX_DirOutput, &pLcmlHdr);
                            ILBCENC_SetPending(pComponentPrivate,pComponentPrivate->pOutputBufHdrPending[i],OMX_DirOutput,__LINE__);
                            eError = LCML_QueueBuffer(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                      EMMCodecOuputBuffer,
                                                      pComponentPrivate->pOutputBufHdrPending[i]->pBuffer,
                                                      pComponentPrivate->pOutputBufHdrPending[i]->nAllocLen,
                                                      pComponentPrivate->pOutputBufHdrPending[i]->nFilledLen,
                                                      (OMX_U8 *) pLcmlHdr->pBufferParam,
                                                      sizeof(ILBCENC_ParamStruct),
                                                      NULL);
                            if (eError != OMX_ErrorNone) {
                                ILBCENC_EPRINT("%d :: %s Queue Output buffer, error\n", __LINE__, __FUNCTION__);
                                ILBCENC_FatalErrorRecover(pComponentPrivate);
                                goto EXIT;
                            }
                        }
                    }
                    pComponentPrivate->nNumOutputBufPending = 0;
                } else {
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventError,
                                                            OMX_ErrorIncorrectStateTransition,
                                                            0,
                                                            NULL);
                    ILBCENC_EPRINT("%d Comp: OMX_ErrorIncorrectStateTransition Given by Comp\n",__LINE__);
                    goto EXIT;

                }
#ifdef RESOURCE_MANAGER_ENABLED
                rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, 
                                                  OMX_ILBC_Encoder_COMPONENT, 
                                                  OMX_StateExecuting, 3456, NULL);
#endif
                pComponentPrivate->curState = OMX_StateExecuting;
#ifdef __PERF_INSTRUMENTATION__
                PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundarySteadyState);
#endif
                pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete,
                                                        OMX_CommandStateSet,
                                                        pComponentPrivate->curState,
                                                        NULL);
                ILBCENC_DPRINT("%d Comp: OMX_CommandStateSet Given by Comp\n",__LINE__);
                break;

            case OMX_StateLoaded:
                ILBCENC_DPRINT("%d ILBCENC_HandleCommand :: OMX_StateLoaded\n",__LINE__);
                if (pComponentPrivate->curState == OMX_StateWaitForResources){
                    ILBCENC_DPRINT("%d ILBCENC_HandleCommand :: OMX_StateWaitForResources\n",__LINE__);
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundaryCleanup);
#endif
                    pComponentPrivate->curState = OMX_StateLoaded;
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundaryCleanup);
#endif
                    pComponentPrivate->curState = OMX_StateLoaded;
                    pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                             pHandle->pApplicationPrivate,
                                                             OMX_EventCmdComplete,
                                                             OMX_CommandStateSet,
                                                             pComponentPrivate->curState,
                                                             NULL);
                    ILBCENC_DPRINT("%d Comp: OMX_CommandStateSet Given by Comp\n",__LINE__);
                    break;
                }
                if (pComponentPrivate->curState != OMX_StateIdle &&
                    pComponentPrivate->curState != OMX_StateWaitForResources) {
                    ILBCENC_DPRINT("%d ILBCENC_HandleCommand :: OMX_StateIdle && OMX_StateWaitForResources\n",__LINE__);
                    pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                             pHandle->pApplicationPrivate,
                                                             OMX_EventError,
                                                             OMX_ErrorIncorrectStateTransition,
                                                             0,
                                                             NULL);
                    ILBCENC_EPRINT("%d Error: OMX_ErrorIncorrectStateTransition Given by Comp\n",__LINE__);
                    goto EXIT;
                }
#ifdef __PERF_INSTRUMENTATION__
                PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundaryCleanup);
#endif
                ILBCENC_DPRINT("%d ILBCENC_HandleCommand - evaluating if all buffers are free\n",__LINE__);
                
                if (pComponentPrivate->pInputBufferList->numBuffers ||
                    pComponentPrivate->pOutputBufferList->numBuffers) {

                    pthread_mutex_lock(&pComponentPrivate->ToLoaded_mutex);
                    pComponentPrivate->InIdle_goingtoloaded = 1;
                    pthread_mutex_unlock(&pComponentPrivate->ToLoaded_mutex);

#ifndef UNDER_CE
                    pthread_mutex_lock(&pComponentPrivate->InIdle_mutex);
                    pthread_cond_wait(&pComponentPrivate->InIdle_threshold, &pComponentPrivate->InIdle_mutex);
                    pthread_mutex_unlock(&pComponentPrivate->InIdle_mutex);
                    
#else
                    OMX_WaitForEvent(&(pComponentPrivate->InIdle_event));
#endif
                
                }
                

                /* Now Deinitialize the component No error should be returned from
                 * this function. It should clean the system as much as possible */
                ILBCENC_CleanupInitParams(pComponentPrivate->pHandle);
                if (pLcmlHandle != NULL) {
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlDestroy, (void *)p);
                    if (eError != OMX_ErrorNone) {
                        ILBCENC_EPRINT("%d Error: LCML_ControlCodec EMMCodecControlDestroy = %x\n",__LINE__, eError);
                        goto EXIT;
                    }

                    /*Closing LCML Lib*/
                    if (pComponentPrivate->ptrLibLCML != NULL){
                            ILBCENC_DPRINT("%d OMX_iLBCEncoder.c Closing LCML library\n",__LINE__);
                            dlclose( pComponentPrivate->ptrLibLCML);
                            pComponentPrivate->ptrLibLCML = NULL;
                    }
                    pLcmlHandle = NULL;
                    pComponentPrivate->pLcmlHandle = NULL;
                }

#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingCommand(pComponentPrivate->pPERF, -1, 0, PERF_ModuleComponent);
#endif
                eError = ILBCENC_EXIT_COMPONENT_THRD;
                pComponentPrivate->bInitParamsInitialized = 0;
                pComponentPrivate->bLoadedCommandPending = OMX_FALSE;

                break;

            case OMX_StatePause:
                ILBCENC_DPRINT("%d ILBCENC_HandleCommand :: OMX_StatePause\n",__LINE__);
                if (pComponentPrivate->curState != OMX_StateExecuting &&
                    pComponentPrivate->curState != OMX_StateIdle) {
                    pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                             pHandle->pApplicationPrivate,
                                                             OMX_EventError,
                                                             OMX_ErrorIncorrectStateTransition,
                                                             0,
                                                             NULL);
                    ILBCENC_EPRINT("%d Error: OMX_ErrorIncorrectStateTransition Given by Comp\n",__LINE__);
                    goto EXIT;
                }
#ifdef __PERF_INSTRUMENTATION__
                PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif
                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlPause, (void *)p);
                if (eError != OMX_ErrorNone) {
                    ILBCENC_EPRINT("%d Error: LCML_ControlCodec EMMCodecControlPause = %x\n",__LINE__,eError);
                    goto EXIT;
                }
                ILBCENC_DPRINT("%d Comp: OMX_CommandStateSet Given by Comp\n",__LINE__);
                break;

            case OMX_StateWaitForResources:
                if (pComponentPrivate->curState == OMX_StateLoaded) {
#ifdef RESOURCE_MANAGER_ENABLED         
                    rm_error = RMProxy_NewSendCommand(pHandle, 
                                                    RMProxy_StateSet, 
                                                    OMX_ILBC_Encoder_COMPONENT, 
                                                    OMX_StateWaitForResources, 
                                                    3456,
                                                    NULL);
#endif                  
                    pComponentPrivate->curState = OMX_StateWaitForResources;
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventCmdComplete,
                                                            OMX_CommandStateSet,
                                                            pComponentPrivate->curState,
                                                            NULL);
                    ILBCENC_DPRINT("%d Comp: OMX_CommandStateSet Given by Comp\n",__LINE__);
                } else {
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventError,
                                                            OMX_ErrorIncorrectStateTransition,
                                                            0,
                                                            NULL);
                    ILBCENC_EPRINT("%d Error: OMX_ErrorIncorrectStateTransition Given by Comp\n",__LINE__);
                }
                break;

            case OMX_StateInvalid:
                ILBCENC_DPRINT("%d: HandleCommand: Cmd OMX_StateInvalid:\n",__LINE__);
                if (pComponentPrivate->curState != OMX_StateWaitForResources && 
                    pComponentPrivate->curState != OMX_StateLoaded) {

                    if (pLcmlHandle != NULL) {
                        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlDestroy, (void *)p);
                        if (eError != OMX_ErrorNone) {
                            ILBCENC_EPRINT("%d Error: LCML_ControlCodec EMMCodecControlDestroy = %x\n",__LINE__, eError);
                        }
                        pLcmlHandle = NULL;
                        pComponentPrivate->pLcmlHandle = NULL;
                    }
                }
                pComponentPrivate->curState = OMX_StateInvalid;
                pComponentPrivate->cbInfo.EventHandler( pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventError, 
                                                        OMX_ErrorInvalidState,
                                                        0, 
                                                        NULL);

                ILBCENC_EPRINT("%d Comp: OMX_ErrorInvalidState Given by Comp\n",__LINE__);
                ILBCENC_CleanupInitParams(pHandle);
                break;

            case OMX_StateMax:
                ILBCENC_DPRINT("%d ILBCENC_HandleCommand :: Cmd OMX_StateMax\n",__LINE__);
                break;

            default:
                ILBCENC_DPRINT("%d ILBCENC_HandleCommand :: invalid commandedState :0x%x\n",__LINE__,commandedState);
                break;
            } /* End of Switch */
        }
    } else if (command == OMX_CommandMarkBuffer) {
        ILBCENC_DPRINT("%d command OMX_CommandMarkBuffer received\n",__LINE__);
        if(!pComponentPrivate->pMarkBuf){
            pComponentPrivate->pMarkBuf = (OMX_MARKTYPE *)(commandData);
        }
    } else if (command == OMX_CommandPortDisable) {
        if (!pComponentPrivate->bDisableCommandPending) {
            if(commandData == 0x0 || (OMX_S32)commandData == -1){
                pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bEnabled = OMX_FALSE;
            }
            if(commandData == 0x1 || (OMX_S32)commandData == -1){
                pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bEnabled = OMX_FALSE;
                if (pComponentPrivate->curState == OMX_StateExecuting) {
                    pComponentPrivate->bNoIdleOnStop = OMX_TRUE;
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               MMCodecControlStop,(void *)p);
                }
            }
        }
        if(commandData == 0x0) {
            if(!pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bPopulated){
                /* return cmdcomplete event if input unpopulated */
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                    pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, 
                                                   OMX_CommandPortDisable,
                                                   ILBCENC_INPUT_PORT, 
                                                   NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else{
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

        if(commandData == 0x1) {
            if (!pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated){
                /* return cmdcomplete event if output unpopulated */
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventCmdComplete, 
                                                    OMX_CommandPortDisable,
                                                    ILBCENC_OUTPUT_PORT, 
                                                    NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

        if((OMX_S32)commandData == -1) {
            if (!pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bPopulated &&
                !pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated){

                /* return cmdcomplete event if inout & output unpopulated */
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                    pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, 
                                                   OMX_CommandPortDisable,
                                                   ILBCENC_INPUT_PORT, 
                                                   NULL);

                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventCmdComplete, 
                                                    OMX_CommandPortDisable,
                                                    ILBCENC_OUTPUT_PORT, 
                                                    NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }
    }
    else if (command == OMX_CommandPortEnable) {
        if (!pComponentPrivate->bEnableCommandPending) {
            if(commandData == 0x0 || (OMX_S32)commandData == -1){
                /* enable in port */
                ILBCENC_DPRINT("setting input port to enabled\n");
                pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bEnabled = OMX_TRUE;
                if(pComponentPrivate->AlloBuf_waitingsignal){
                    pComponentPrivate->AlloBuf_waitingsignal = 0;
                }
                ILBCENC_DPRINT("pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]\
->bEnabled = %d\n",pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bEnabled);
            }
            if(commandData == 0x1 || (OMX_S32)commandData == -1){
                /* enable out port */
                if(pComponentPrivate->AlloBuf_waitingsignal) {
                    pComponentPrivate->AlloBuf_waitingsignal = 0;
#ifndef UNDER_CE
                    pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex);
                    pthread_cond_signal(&pComponentPrivate->AlloBuf_threshold);
                    pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);
#else
                    OMX_SignalEvent(&(pComponentPrivate->AlloBuf_event));
#endif
                }
                if (pComponentPrivate->curState == OMX_StateExecuting) {
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStart,(void *)p);
                }
                ILBCENC_DPRINT("setting output port to enabled\n");
                pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bEnabled = OMX_TRUE;
                ILBCENC_DPRINT("pComponentPrivate->pPortDef[ILBCENC_OUTPUT_POR\
T]->bEnabled = %d\n",pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bEnabled);
            }
        }

        if(commandData == 0x0) {
            if (pComponentPrivate->curState == OMX_StateLoaded || 
                pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bPopulated){
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventCmdComplete, 
                                                    OMX_CommandPortEnable,
                                                    ILBCENC_INPUT_PORT, 
                                                    NULL);
                pComponentPrivate->bEnableCommandPending = 0;
            }else {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->bEnableCommandParam = commandData;
            }
        }
        else if(commandData == 0x1) {
            if (pComponentPrivate->curState == OMX_StateLoaded || 
                pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated){
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventCmdComplete, 
                                                    OMX_CommandPortEnable,
                                                    ILBCENC_OUTPUT_PORT, 
                                                    NULL);
                pComponentPrivate->bEnableCommandPending = 0;
            }
            else {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->bEnableCommandParam = commandData;
            }
        }
        else if((OMX_S32)commandData == -1){
            if (pComponentPrivate->curState == OMX_StateLoaded || 
                (pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->bPopulated && 
                pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->bPopulated)){
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventCmdComplete, 
                                                    OMX_CommandPortEnable,
                                                    ILBCENC_INPUT_PORT, 
                                                    NULL);

                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventCmdComplete, 
                                                    OMX_CommandPortEnable,
                                                    ILBCENC_OUTPUT_PORT, 
                                                    NULL);
                pComponentPrivate->bEnableCommandPending = 0;
                ILBCENC_FillLCMLInitParamsEx(pHandle);
            }
            else{
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->bEnableCommandParam = commandData;
            }
        }
#ifndef UNDER_CE                             
        pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
        pthread_cond_signal(&pComponentPrivate->AlloBuf_threshold);
        pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);    
#else
        OMX_SignalEvent(&(pComponentPrivate->AlloBuf_event));
#endif  

    } else if (command == OMX_CommandFlush) {
        if(commandData == 0x0 || (OMX_S32)commandData == -1){/*input*/
            ILBCENC_DPRINT("Flushing input port %d\n",__LINE__);
            for (i=0; i < ILBCENC_MAX_NUM_OF_BUFS; i++) {
                pComponentPrivate->pInputBufHdrPending[i] = NULL;
            }
            pComponentPrivate->nNumInputBufPending=0;

            for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
                         
                pComponentPrivate->cbInfo.EmptyBufferDone (
                                                           pComponentPrivate->pHandle,
                                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                                           pComponentPrivate->pInputBufferList->pBufHdr[i]
                                                           );
                pComponentPrivate->nEmptyBufferDoneCount++;
            }
            pComponentPrivate->cbInfo.EventHandler(
                                                   pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, OMX_CommandFlush,ILBCENC_INPUT_PORT, NULL);
        }
      
        if(commandData == 0x1 || (OMX_S32)commandData == -1){/*output*/
            for (i=0; i < ILBCENC_MAX_NUM_OF_BUFS; i++) {
                pComponentPrivate->pOutputBufHdrPending[i] = NULL;
            }
            pComponentPrivate->nNumOutputBufPending=0;

            for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                  pComponentPrivate->pOutputBufferList->pBufHdr[i]->pBuffer,
                                  pComponentPrivate->pOutputBufferList->pBufHdr[i]->nFilledLen,
                                  PERF_ModuleHLMM);
#endif
                pComponentPrivate->cbInfo.FillBufferDone (
                                                          pComponentPrivate->pHandle,
                                                          pComponentPrivate->pHandle->pApplicationPrivate,
                                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]
                                                          );
                pComponentPrivate->nFillBufferDoneCount++;                                       
            }
            pComponentPrivate->cbInfo.EventHandler(
                                                   pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, OMX_CommandFlush,ILBCENC_OUTPUT_PORT, NULL);
        }

    }

 EXIT:
    ILBCENC_DPRINT("%d Exiting ILBCENC_HandleCommand Function\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
* @ILBCENC_HandleDataBufFromApp() This function is called by the component when ever it
* receives the buffer from the application
*
* @param pComponentPrivate  Component private data
* @param pBufHeader Buffer from the application
*
* @pre
*
* @post
*
* @return none
*/
/* ========================================================================== */
OMX_ERRORTYPE ILBCENC_HandleDataBufFromApp(OMX_BUFFERHEADERTYPE* pBufHeader,
                                           ILBCENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_DIRTYPE eDir;
    ILBCENC_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;
    LCML_DSP_INTERFACE *pLcmlHandle = (LCML_DSP_INTERFACE *)
        pComponentPrivate->pLcmlHandle;
    OMX_U32 frameLength = 0, remainingBytes = 0;
    OMX_U8* pExtraData = NULL;
    OMX_U8 nFrames=0,i = 0;
    LCML_DSP_INTERFACE * phandle = NULL;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDefIn = NULL;
    ILBCENC_BUFDATA* OutputFrames = NULL;
    ILBCENC_DPRINT ("%d Entering ILBCENC_HandleDataBufFromApp Function\n",__LINE__);
    /*Find the direction of the received buffer from buffer list */

    eError = ILBCENC_GetBufferDirection(pBufHeader, &eDir);
    if (eError != OMX_ErrorNone) {
        ILBCENC_EPRINT ("%d The pBufHeader is not found in the list\n", __LINE__);
        goto EXIT;
    }

    if (eDir == OMX_DirInput) {
        pComponentPrivate->nEmptyThisBufferCount++;
        pPortDefIn = pComponentPrivate->pPortDef[OMX_DirInput];
        if  (pBufHeader->nFilledLen > 0) {
            if (pComponentPrivate->nHoldLength == 0) {
                frameLength = pComponentPrivate->inputFrameSize;
                nFrames = (OMX_U8)(pBufHeader->nFilledLen / frameLength);

                if ( nFrames>=1 ) { /*At least there is 1 frame in the buffer*/
                    pComponentPrivate->nHoldLength = pBufHeader->nFilledLen - frameLength*nFrames;
                    if (pComponentPrivate->nHoldLength > 0) {/* something need to be hold in pHoldBuffer */
                        OMX_MALLOC_SIZE(pComponentPrivate->pHoldBuffer, pComponentPrivate->inputFrameSize,OMX_U8);/*glen*/
                        /* Copy the extra data into pHoldBuffer. Size will be nHoldLength. */
                        pExtraData = pBufHeader->pBuffer + frameLength*nFrames;
                        memcpy(pComponentPrivate->pHoldBuffer, pExtraData,  pComponentPrivate->nHoldLength);
                        pBufHeader->nFilledLen-=pComponentPrivate->nHoldLength;
                    }
                }
                else {
                    if( !pComponentPrivate->InBuf_Eos_alreadysent ){
                        /* received buffer with less than 1 GSM FR frame length. Save the data in pHoldBuffer.*/
                        pComponentPrivate->nHoldLength = pBufHeader->nFilledLen;
                        /* save the data into pHoldBuffer */
                        OMX_MALLOC_SIZE(pComponentPrivate->pHoldBuffer, pComponentPrivate->inputFrameSize,OMX_U8);
                        memcpy(pComponentPrivate->pHoldBuffer, pBufHeader->pBuffer, pComponentPrivate->nHoldLength);
                    }

                    /* since not enough data, we shouldn't send anything to SN, but instead request to EmptyBufferDone again.*/
                    if (pComponentPrivate->curState != OMX_StatePause ) {
                        ILBCENC_DPRINT("%d Calling EmptyBufferDone\n",__LINE__);
                                            
#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                          pBufHeader->pBuffer,
                                          0,
                                          PERF_ModuleHLMM);
#endif

                        pComponentPrivate->cbInfo.EmptyBufferDone( pComponentPrivate->pHandle,
                                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                                   pBufHeader);
                        pComponentPrivate->nEmptyBufferDoneCount++;

                    }
                    else {
                        pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pBufHeader;
                    }

                    goto EXIT;
                }
            }
            else {
                if((pComponentPrivate->nHoldLength+pBufHeader->nFilledLen) > pBufHeader->nAllocLen){
                    /*means that a second Acumulator must be used to insert holdbuffer to pbuffer and save remaining bytes
                      into hold buffer*/
                    remainingBytes = pComponentPrivate->nHoldLength+pBufHeader->nFilledLen-pBufHeader->nAllocLen;
                    OMX_MALLOC_SIZE(pComponentPrivate->pHoldBuffer2, pComponentPrivate->inputFrameSize,OMX_U8);
                    pExtraData = (pBufHeader->pBuffer)+(pBufHeader->nFilledLen-remainingBytes);
                    memcpy(pComponentPrivate->pHoldBuffer2,pExtraData,remainingBytes);
                    pBufHeader->nFilledLen-=remainingBytes;
                    memmove(pBufHeader->pBuffer+ pComponentPrivate->nHoldLength,pBufHeader->pBuffer,pBufHeader->nFilledLen);
                    memcpy(pBufHeader->pBuffer,pComponentPrivate->pHoldBuffer,pComponentPrivate->nHoldLength);
                    pBufHeader->nFilledLen+=pComponentPrivate->nHoldLength;
                    memcpy(pComponentPrivate->pHoldBuffer, pComponentPrivate->pHoldBuffer2, remainingBytes);
                    pComponentPrivate->nHoldLength=remainingBytes;
                }
                else{
                    memmove(pBufHeader->pBuffer+pComponentPrivate->nHoldLength, pBufHeader->pBuffer, pBufHeader->nFilledLen);
                    memcpy(pBufHeader->pBuffer,pComponentPrivate->pHoldBuffer, pComponentPrivate->nHoldLength);
                    pBufHeader->nFilledLen+=pComponentPrivate->nHoldLength;
                    pComponentPrivate->nHoldLength=0;
                }
                frameLength = pComponentPrivate->inputFrameSize;
                nFrames = (OMX_U8)(pBufHeader->nFilledLen / frameLength);
                pComponentPrivate->nHoldLength = pBufHeader->nFilledLen - frameLength*nFrames;
                pExtraData = pBufHeader->pBuffer + pBufHeader->nFilledLen-pComponentPrivate->nHoldLength;
                memcpy(pComponentPrivate->pHoldBuffer, pExtraData,  pComponentPrivate->nHoldLength);
                pBufHeader->nFilledLen-=pComponentPrivate->nHoldLength;
                if(nFrames < 1 ){
                    if (pComponentPrivate->curState != OMX_StatePause ) {
                        ILBCENC_DPRINT("line %d:: Calling EmptyBufferDone\n",__LINE__);
                                                
#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                          pBufHeader->pBuffer,
                                          0,
                                          PERF_ModuleHLMM);
#endif
                        pComponentPrivate->cbInfo.EmptyBufferDone( pComponentPrivate->pHandle,
                                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                                   pBufHeader);
                        pComponentPrivate->nEmptyBufferDoneCount++;

                    }
                    else if(pComponentPrivate->curState == OMX_StatePause){
                        pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pBufHeader;
                    }
                    goto EXIT;
                }
            }
        }else{
            if(pBufHeader->nFlags != OMX_BUFFERFLAG_EOS){
                if (pComponentPrivate->dasfMode == 0 && !pBufHeader->pMarkData) {
#ifdef __PERF_INSTRUMENTATION__
                    PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                      pComponentPrivate->pInputBufferList->pBufHdr[0]->pBuffer,
                                      0,
                                      PERF_ModuleHLMM);
#endif

                    pComponentPrivate->cbInfo.EmptyBufferDone( pComponentPrivate->pHandle,
                                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                                               pComponentPrivate->pInputBufferList->pBufHdr[0]);
                    pComponentPrivate->nEmptyBufferDoneCount++;

                }
            }
            else{
                frameLength = pComponentPrivate->inputFrameSize;
                nFrames=1;
            }
        }
        if(nFrames >= 1){
            eError = ILBCENC_GetCorrespondingLCMLHeader(pComponentPrivate,pBufHeader->pBuffer, OMX_DirInput, &pLcmlHdr);
            if (eError != OMX_ErrorNone) {
                ILBCENC_EPRINT("%d Error: Invalid Buffer Came ...\n",__LINE__);
                goto EXIT;
            }

#ifdef __PERF_INSTRUMENTATION__
            /*For Steady State Instumentation*/
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                              PREF(pBufHeader,pBuffer),
                              pPortDefIn->nBufferSize,
                              PERF_ModuleCommonLayer);
#endif

            pComponentPrivate->nNumOfFramesSent = nFrames;

            phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);

            if( (pLcmlHdr->pBufferParam->usNbFrames < nFrames) && (pLcmlHdr->pFrameParam!=NULL) ){
                OMX_MEMFREE_STRUCT_DSPALIGN(pLcmlHdr->pFrameParam, ILBCENC_FrameStruct);
                OMX_DmmUnMap(phandle->dspCodec->hProc, /*Unmap DSP memory used*/
                             (void*)pLcmlHdr->pBufferParam->pParamElem,
                             pLcmlHdr->pDmmBuf->pReserved);
                pLcmlHdr->pBufferParam->pParamElem = NULL;
            }

            if(pLcmlHdr->pFrameParam==NULL ){
                OMX_MALLOC_SIZE_DSPALIGN(pLcmlHdr->pFrameParam, (sizeof(ILBCENC_FrameStruct)*nFrames), ILBCENC_FrameStruct);
                if(NULL == pLcmlHdr->pFrameParam){
                    OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer);
                    OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer2);
                    return OMX_ErrorInsufficientResources;
                }
                eError = OMX_DmmMap(phandle->dspCodec->hProc,
                                    nFrames*sizeof(ILBCENC_FrameStruct),
                                    (void*)pLcmlHdr->pFrameParam,
                                    (pLcmlHdr->pDmmBuf));
                if (eError != OMX_ErrorNone){
                    ILBCENC_EPRINT("OMX_DmmMap ERRROR!!!!\n\n");
                    ILBCENC_FatalErrorRecover(pComponentPrivate);
                    goto EXIT;
                }
                pLcmlHdr->pBufferParam->pParamElem = (ILBCENC_FrameStruct *)pLcmlHdr->pDmmBuf->pMapped; /*DSP Address*/
            }

            for(i=0;i<nFrames;i++){
                (pLcmlHdr->pFrameParam+i)->usLastFrame = 0;
            }

            if(pBufHeader->nFlags == OMX_BUFFERFLAG_EOS) {
                (pLcmlHdr->pFrameParam+(nFrames-1))->usLastFrame = OMX_BUFFERFLAG_EOS;
                pComponentPrivate->InBuf_Eos_alreadysent = 1;
                pBufHeader->nFlags = 0;
            }
            pLcmlHdr->pBufferParam->usNbFrames = nFrames;

            /* Store time stamp information */
            pComponentPrivate->arrBufIndex[pComponentPrivate->IpBufindex] = pBufHeader->nTimeStamp;
            pComponentPrivate->IpBufindex++;
            pComponentPrivate->IpBufindex %= pPortDefIn->nBufferCountActual;

            if (pComponentPrivate->curState == OMX_StateExecuting) {
                if (!ILBCENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirInput)) {
                    ILBCENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirInput,__LINE__);
                    eError = LCML_QueueBuffer( pLcmlHandle->pCodecinterfacehandle,
                                               EMMCodecInputBuffer,
                                               (OMX_U8 *)pBufHeader->pBuffer,
                                               pBufHeader->nAllocLen,
                                               pBufHeader->nFilledLen,
                                               (OMX_U8 *) pLcmlHdr->pBufferParam,
                                               sizeof(ILBCENC_ParamStruct),
                                               NULL);
                    if (eError != OMX_ErrorNone) {
                        ILBCENC_EPRINT("%d :: %s Queue Input buffer, error\n", __LINE__, __FUNCTION__);
                        ILBCENC_FatalErrorRecover(pComponentPrivate);
                        eError = OMX_ErrorHardware;
                        goto EXIT;
                    }
                    pComponentPrivate->lcml_nIpBuf++;
                }
            }
            else if(pComponentPrivate->curState == OMX_StatePause){
                pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pBufHeader;
            }
        }
        if(pBufHeader->pMarkData){
            if(pComponentPrivate->pOutputBufferList->pBufHdr[0]!=NULL) {
                /* copy mark to output buffer header */
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->pMarkData = pBufHeader->pMarkData;
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->hMarkTargetComponent = pBufHeader->hMarkTargetComponent;
            }
            /* trigger event handler if we are supposed to */
            if(pBufHeader->hMarkTargetComponent == pComponentPrivate->pHandle && pBufHeader->pMarkData){
                pComponentPrivate->cbInfo.EventHandler( pComponentPrivate->pHandle,
                                                        pComponentPrivate->pHandle->pApplicationPrivate,
                                                        OMX_EventMark,
                                                        0,
                                                        0,
                                                        pBufHeader->pMarkData);
            }
            if (pComponentPrivate->curState != OMX_StatePause && !ILBCENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirInput)) {
                ILBCENC_DPRINT("line %d:: Calling EmptyBufferDone\n",__LINE__);
                
#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                  pBufHeader->pBuffer,
                                  0,
                                  PERF_ModuleHLMM);
#endif

                pComponentPrivate->cbInfo.EmptyBufferDone( pComponentPrivate->pHandle,
                                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                                           pBufHeader);
                pComponentPrivate->nEmptyBufferDoneCount++;

            }                
        }

    } else if (eDir == OMX_DirOutput) {
        nFrames = pComponentPrivate->nNumOfFramesSent;

        if(nFrames == 0)
            nFrames = 1;
        /*frameLength = ILBCENC_PRIMARY_OUTPUT_FRAME_SIZE;*/
        OutputFrames = pBufHeader->pOutputPortPrivate;
        OutputFrames->nFrames = nFrames;
                            
        eError = ILBCENC_GetCorrespondingLCMLHeader(pComponentPrivate,pBufHeader->pBuffer, OMX_DirOutput, &pLcmlHdr);

        phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);

        if( (pLcmlHdr->pBufferParam->usNbFrames < nFrames) && (pLcmlHdr->pFrameParam!=NULL) ){
            OMX_MEMFREE_STRUCT_DSPALIGN(pLcmlHdr->pFrameParam, ILBCENC_FrameStruct);
#ifndef UNDER_CE
            OMX_DmmUnMap(phandle->dspCodec->hProc,
                         (void*)pLcmlHdr->pBufferParam->pParamElem,
                         pLcmlHdr->pDmmBuf->pReserved);
#endif

            pLcmlHdr->pBufferParam->pParamElem = NULL;
        }

        if(pLcmlHdr->pFrameParam==NULL ){
            OMX_MALLOC_SIZE_DSPALIGN(pLcmlHdr->pFrameParam, (sizeof(ILBCENC_FrameStruct)*nFrames ), ILBCENC_FrameStruct);
            if(NULL == pLcmlHdr->pFrameParam){
                OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer);
                OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer2);
                return OMX_ErrorInsufficientResources;
            }
            pLcmlHdr->pBufferParam->pParamElem = NULL;
#ifndef UNDER_CE
            eError = OMX_DmmMap(phandle->dspCodec->hProc,
                                nFrames*sizeof(ILBCENC_FrameStruct),
                                (void*)pLcmlHdr->pFrameParam,
                                (pLcmlHdr->pDmmBuf));

            if (eError != OMX_ErrorNone){
                ILBCENC_EPRINT("OMX_DmmMap ERRROR!!!!\n");
                goto EXIT;
            }
            pLcmlHdr->pBufferParam->pParamElem = (ILBCENC_FrameStruct *)pLcmlHdr->pDmmBuf->pMapped; /*DSP Address*/
#endif
        }

        pLcmlHdr->pBufferParam->usNbFrames = nFrames;
        ILBCENC_DPRINT ("%d: Sending Empty OUTPUT BUFFER to Codec = %p\n",__LINE__,pBufHeader->pBuffer);

#ifdef __PERF_INSTRUMENTATION__
        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                          PREF(pBufHeader,pBuffer),
                          0,
                          PERF_ModuleCommonLayer);
#endif
        if (pComponentPrivate->curState == OMX_StateExecuting) {
            if (!ILBCENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirOutput)) {
                ILBCENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirOutput,__LINE__);
                eError = LCML_QueueBuffer( pLcmlHandle->pCodecinterfacehandle,
                                           EMMCodecOuputBuffer,
                                           (OMX_U8 *)pBufHeader->pBuffer,
                                           pComponentPrivate->outputFrameSize * nFrames,
                                           0,
                                           (OMX_U8 *) pLcmlHdr->pBufferParam,
                                           sizeof(ILBCENC_ParamStruct),
                                           NULL);
                ILBCENC_DPRINT("After QueueBuffer Line %d\n",__LINE__);
                if (eError != OMX_ErrorNone ) {
                    ILBCENC_EPRINT("%d :: %s Queue Output buffer, error\n", __LINE__, __FUNCTION__);
                    ILBCENC_FatalErrorRecover(pComponentPrivate);
                    eError = OMX_ErrorHardware;
                    goto EXIT;
                }
                pComponentPrivate->lcml_nOpBuf++;
            }
        }
        else if(pComponentPrivate->curState == OMX_StatePause){
            pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = pBufHeader;
        }   
    }
    else {
        eError = OMX_ErrorBadParameter;
    }

 EXIT:

    if (eError!=OMX_ErrorNone) {
        /* Fress all the memory allocated in this funtion. */
        OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer2);
        if (pLcmlHdr!=NULL)
            OMX_MEMFREE_STRUCT_DSPALIGN(pLcmlHdr->pFrameParam,ILBCENC_FrameStruct);
    }
    ILBCENC_DPRINT("%d Exiting from  ILBCENC_HandleDataBufFromApp \n",__LINE__);
    ILBCENC_DPRINT("%d Returning error %d\n",__LINE__,eError);

    return eError;
}

/*-------------------------------------------------------------------*/
/**
* ILBCENC_GetBufferDirection () This function is used by the component
* to get the direction of the buffer
* @param eDir pointer will be updated with buffer direction
* @param pBufHeader pointer to the buffer to be requested to be filled
*
* @retval none
**/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE ILBCENC_GetBufferDirection(OMX_BUFFERHEADERTYPE *pBufHeader,
                                         OMX_DIRTYPE *eDir)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = pBufHeader->pPlatformPrivate;
    OMX_U32 nBuf = 0;
    OMX_BUFFERHEADERTYPE *pBuf = NULL;
    OMX_U16 flag = 1,i = 0;
    ILBCENC_DPRINT("%d Entering ILBCENC_GetBufferDirection Function\n",__LINE__);
    /*Search this buffer in input buffers list */
    nBuf = pComponentPrivate->pInputBufferList->numBuffers;
    for(i=0; i<nBuf; i++) {
        pBuf = pComponentPrivate->pInputBufferList->pBufHdr[i];
        if(pBufHeader == pBuf) {
            *eDir = OMX_DirInput;
            ILBCENC_DPRINT("%d pBufHeader = %p is INPUT BUFFER pBuf = %p\n",__LINE__,pBufHeader,pBuf);
            flag = 0;
            goto EXIT;
        }
    }
    /*Search this buffer in output buffers list */
    nBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    for(i=0; i<nBuf; i++) {
        pBuf = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        if(pBufHeader == pBuf) {
            *eDir = OMX_DirOutput;
            ILBCENC_DPRINT("%d pBufHeader = %p is OUTPUT BUFFER pBuf = %p\n",__LINE__,pBufHeader,pBuf);
            flag = 0;
            goto EXIT;
        }
    }

    if (flag == 1) {
        ILBCENC_EPRINT("%d Buffer %p is Not Found in the List\n",__LINE__, pBufHeader);
        eError = OMX_ErrorUndefined;
        goto EXIT;
    }
 EXIT:
    ILBCENC_DPRINT("%d Exiting ILBCENC_GetBufferDirection Function\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* -------------------------------------------------------------------*/
/**
  * ILBCENC_GetCorrespondingLCMLHeader() function will be called by LCML_Callback
  * component to write the msg
  * @param *pBuffer,          Event which gives to details about USN status
  * @param ILBCENC_LCML_BUFHEADERTYPE **ppLcmlHdr
  * @param  OMX_DIRTYPE eDir this gives direction of the buffer
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
 **/
/* -------------------------------------------------------------------*/
OMX_ERRORTYPE ILBCENC_GetCorrespondingLCMLHeader(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                                                  OMX_U8 *pBuffer,
                                                  OMX_DIRTYPE eDir,
                                                 ILBCENC_LCML_BUFHEADERTYPE **ppLcmlHdr)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    ILBCENC_LCML_BUFHEADERTYPE *pLcmlBufHeader = NULL;
    OMX_S16 nIpBuf = 0;
    OMX_S16 nOpBuf = 0;
    OMX_S16 i = 0;

    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE*) pComponentPrivate;
    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;

    ILBCENC_DPRINT("%d Entering ILBCENC_GetCorrespondingLCMLHeader..\n",__LINE__);
    while (!pComponentPrivate->bInitParamsInitialized) {
        ILBCENC_DPRINT("%d Waiting for init to complete........\n",__LINE__);
#ifndef UNDER_CE
        sched_yield();
#else
        Sleep(1);
#endif
    }
    if(eDir == OMX_DirInput) {
        ILBCENC_DPRINT("%d Entering ILBCENC_GetCorrespondingLCMLHeader..\n",__LINE__);
        pLcmlBufHeader = pComponentPrivate->pLcmlBufHeader[ILBCENC_INPUT_PORT];
        for(i = 0; i < nIpBuf; i++) {
            ILBCENC_DPRINT("%d pBuffer = %p\n",__LINE__,pBuffer);
            ILBCENC_DPRINT("%d pLcmlBufHeader->buffer->pBuffer = %p\n",__LINE__,pLcmlBufHeader->buffer->pBuffer);
            if(pBuffer == pLcmlBufHeader->buffer->pBuffer) {
                *ppLcmlHdr = pLcmlBufHeader;
                ILBCENC_DPRINT("%d Corresponding Input LCML Header Found = %p\n",__LINE__,pLcmlBufHeader);
                eError = OMX_ErrorNone;
                goto EXIT;
            }
            pLcmlBufHeader++;
        }
    } else if (eDir == OMX_DirOutput) {
        ILBCENC_DPRINT("%d Entering ILBCENC_GetCorrespondingLCMLHeader..\n",__LINE__);
        pLcmlBufHeader = pComponentPrivate->pLcmlBufHeader[ILBCENC_OUTPUT_PORT];
        for(i = 0; i < nOpBuf; i++) {
            ILBCENC_DPRINT("%d pBuffer = %p\n",__LINE__,pBuffer);
            ILBCENC_DPRINT("%d pLcmlBufHeader->buffer->pBuffer = %p\n",__LINE__,pLcmlBufHeader->buffer->pBuffer);
            if(pBuffer == pLcmlBufHeader->buffer->pBuffer) {
                *ppLcmlHdr = pLcmlBufHeader;
                ILBCENC_DPRINT("%d Corresponding Output LCML Header Found = %p\n",__LINE__,pLcmlBufHeader);
                eError = OMX_ErrorNone;
                goto EXIT;
            }
            pLcmlBufHeader++;
        }
    } else {
        ILBCENC_EPRINT("%d Invalid Buffer Type :: exiting...\n",__LINE__);
        eError = OMX_ErrorUndefined;
    }

 EXIT:
    ILBCENC_DPRINT("%d Exiting ILBCENC_GetCorrespondingLCMLHeader..\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* -------------------------------------------------------------------*/
/**
  *  ILBCENC_LCMLCallback() will be called LCML component to write the msg
  *
  * @param event                 Event which gives to details about USN status
  * @param void * args        //    args [0] //bufType;
                              //    args [1] //arm address fpr buffer
                              //    args [2] //BufferSize;
                              //    args [3]  //arm address for param
                              //    args [4] //ParamSize;
                              //    args [6] //LCML Handle
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
 **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE ILBCENC_LCMLCallback (TUsnCodecEvent event,void * args[10])
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U8 *pBuffer = args[1];
    ILBCENC_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;
    OMX_U16 i = 0;
    OMX_COMPONENTTYPE *pHandle = NULL;
    LCML_DSP_INTERFACE *pLcmlHandle = NULL;
    ILBCENC_COMPONENT_PRIVATE* pComponentPrivate = NULL;

    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE*)((LCML_DSP_INTERFACE*)args[6])->pComponentPrivate;
    pHandle = pComponentPrivate->pHandle;

#ifdef ILBCENC_DEBUG
    switch(event) {

    case EMMCodecDspError:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecDspError\n");
        break;

    case EMMCodecInternalError:
        ILBCENC_EPRINT("[LCML CALLBACK EVENT]  EMMCodecInternalError\n");
        break;

    case EMMCodecInitError:
        ILBCENC_EPRINT("[LCML CALLBACK EVENT]  EMMCodecInitError\n");
        break;

    case EMMCodecDspMessageRecieved:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecDspMessageRecieved\n");
        break;

    case EMMCodecBufferProcessed:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecBufferProcessed\n");
        break;

    case EMMCodecProcessingStarted:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingStarted\n");
        break;

    case EMMCodecProcessingPaused:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingPaused\n");
        break;

    case EMMCodecProcessingStoped:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingStoped\n");
        break;

    case EMMCodecProcessingEof:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingEof\n");
        break;

    case EMMCodecBufferNotProcessed:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecBufferNotProcessed\n");
        break;

    case EMMCodecAlgCtrlAck:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecAlgCtrlAck\n");
        break;

    case EMMCodecStrmCtrlAck:
        ILBCENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecStrmCtrlAck\n");
        break;
    }
#endif

    if(event == EMMCodecBufferProcessed){
        if((OMX_U32)args[0] == EMMCodecInputBuffer) {
            ILBCENC_DPRINT("%d INPUT: pBuffer = %p\n",__LINE__, pBuffer);
            eError = ILBCENC_GetCorrespondingLCMLHeader(pComponentPrivate,pBuffer, OMX_DirInput, &pLcmlHdr);
            if (eError != OMX_ErrorNone) {
                ILBCENC_EPRINT("%d Error: Invalid Buffer Came ...\n",__LINE__);
                goto EXIT;
            }
#ifdef __PERF_INSTRUMENTATION__
            PERF_ReceivedFrame(pComponentPrivate->pPERFcomp,
                               PREF(pLcmlHdr->buffer,pBuffer),
                               0,
                               PERF_ModuleCommonLayer);
#endif

            ILBCENC_ClearPending(pComponentPrivate,pLcmlHdr->buffer,OMX_DirInput,__LINE__);

            ILBCENC_DPRINT("%d Calling EmptyBufferDone\n",__LINE__);
#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                  pLcmlHdr->buffer->pBuffer,
                                  0,
                                  PERF_ModuleHLMM);
#endif
            pComponentPrivate->cbInfo.EmptyBufferDone(pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    pLcmlHdr->buffer);
            pComponentPrivate->nEmptyBufferDoneCount++;
            pComponentPrivate->lcml_nIpBuf--;
            pComponentPrivate->app_nBuf++;
            
        } else if((OMX_U32)args[0] == EMMCodecOuputBuffer) {

            ILBCENC_DPRINT("%d OUTPUT: pBuffer = %p\n",__LINE__, pBuffer);
            eError = ILBCENC_GetCorrespondingLCMLHeader(pComponentPrivate, pBuffer, OMX_DirOutput, &pLcmlHdr);
            if (eError != OMX_ErrorNone) {
                ILBCENC_EPRINT("%d Error: Invalid Buffer Came ...\n",__LINE__);
                goto EXIT;
            }

            ILBCENC_DPRINT("%d Output: pLcmlHdr->buffer->pBuffer = %p\n",__LINE__, pLcmlHdr->buffer->pBuffer);
            pLcmlHdr->buffer->nFilledLen = (OMX_U32)args[8];
            ILBCENC_DPRINT("%d Output: nFilledLen = %ld\n",__LINE__, pLcmlHdr->buffer->nFilledLen);
#ifdef __PERF_INSTRUMENTATION__
            PERF_ReceivedFrame(pComponentPrivate->pPERFcomp,
                               PREF(pLcmlHdr->buffer,pBuffer),
                               PREF(pLcmlHdr->buffer,nFilledLen),
                               PERF_ModuleCommonLayer);

            pComponentPrivate->nLcml_nCntOpReceived++;

            if ((pComponentPrivate->nLcml_nCntIp >= 1) && 
                (pComponentPrivate->nLcml_nCntOpReceived == 1)) {
                PERF_Boundary(pComponentPrivate->pPERFcomp,
                              PERF_BoundaryStart | PERF_BoundarySteadyState);
            }
#endif

            ILBCENC_ClearPending(pComponentPrivate,pLcmlHdr->buffer,OMX_DirOutput,__LINE__);

            pComponentPrivate->LastOutbuf = pLcmlHdr->buffer;
            if(!pLcmlHdr->pBufferParam->usNbFrames){
                pLcmlHdr->pBufferParam->usNbFrames++;
            }
            if(pComponentPrivate->InBuf_Eos_alreadysent) {
                pLcmlHdr->buffer->nFlags |= OMX_BUFFERFLAG_EOS;
                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventBufferFlag,
                                                    pLcmlHdr->buffer->nOutputPortIndex,
                                                    pLcmlHdr->buffer->nFlags, 
                                                    NULL);
                pComponentPrivate->InBuf_Eos_alreadysent = 0;                                                    
            }
            if( !pComponentPrivate->dasfMode){
                /* Copying time stamp information to output buffer */
                pLcmlHdr->buffer->nTimeStamp = (OMX_TICKS)pComponentPrivate->arrBufIndex[pComponentPrivate->OpBufindex];
                pComponentPrivate->OpBufindex++;
                pComponentPrivate->OpBufindex %= pComponentPrivate->pPortDef[OMX_DirInput]->nBufferCountActual;           
            }

#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingBuffer(pComponentPrivate->pPERFcomp,
                               pLcmlHdr->buffer->pBuffer,
                               pLcmlHdr->buffer->nFilledLen,
                               PERF_ModuleHLMM);
#endif
            /* Non Multi Frame Mode has been tested here */
            pComponentPrivate->cbInfo.FillBufferDone( pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    pLcmlHdr->buffer);
            pComponentPrivate->nFillBufferDoneCount++;
            pComponentPrivate->lcml_nOpBuf--;
            pComponentPrivate->app_nBuf++;
            
            ILBCENC_DPRINT("%d Incrementing app_nBuf = %ld\n",__LINE__,pComponentPrivate->app_nBuf);
        }
    }
    else if (event == EMMCodecStrmCtrlAck) {
        ILBCENC_DPRINT("%d GOT MESSAGE USN_DSPACK_STRMCTRL \n",__LINE__);
        if (args[1] == (void *)USN_STRMCMD_FLUSH) {
            pHandle = pComponentPrivate->pHandle;
            if ( args[2] == (void *)EMMCodecInputBuffer) {
                if (args[0] == (void *)USN_ERR_NONE ) {
                    ILBCENC_DPRINT("Flushing input port %d\n",__LINE__);
                    for (i=0; i < ILBCENC_MAX_NUM_OF_BUFS; i++) {
                        pComponentPrivate->pInputBufHdrPending[i] = NULL;
                    }
                    pComponentPrivate->nNumInputBufPending=0;

                    for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {

#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                          pComponentPrivate->pInputBufferList->pBufHdr[i]->pBuffer,
                                          0,
                                          PERF_ModuleHLMM);
#endif
                        pComponentPrivate->cbInfo.EmptyBufferDone (pHandle,
                                                                pHandle->pApplicationPrivate,
                                                                pComponentPrivate->pInputBufferList->pBufHdr[i]);
                        pComponentPrivate->nEmptyBufferDoneCount++;
                    }

                    pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventCmdComplete, 
                                                            OMX_CommandFlush,
                                                            ILBCENC_INPUT_PORT, 
                                                            NULL);
                } else {
                    ILBCENC_EPRINT("LCML reported error while flushing input port\n");
                    goto EXIT;
                }
            }
            else if ( args[2] == (void *)EMMCodecOuputBuffer) {
                if (args[0] == (void *)USN_ERR_NONE ) {
                    ILBCENC_DPRINT("Flushing output port %d\n",__LINE__);
                    for (i=0; i < ILBCENC_MAX_NUM_OF_BUFS; i++) {
                        pComponentPrivate->pOutputBufHdrPending[i] = NULL;
                    }
                    pComponentPrivate->nNumOutputBufPending=0;

                    for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]->pBuffer,
                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]->nFilledLen,
                                          PERF_ModuleHLMM);
#endif
                        pComponentPrivate->cbInfo.FillBufferDone (pHandle,
                                                                pHandle->pApplicationPrivate,
                                                                pComponentPrivate->pOutputBufferList->pBufHdr[i]);
                        pComponentPrivate->nFillBufferDoneCount++; 
                    }

                    pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete, 
                                                        OMX_CommandFlush,
                                                        ILBCENC_OUTPUT_PORT, 
                                                        NULL);
                } else {
                    ILBCENC_EPRINT("LCML reported error while flushing output port\n");
                    goto EXIT;
                }
            }
        }
    }
    else if(event == EMMCodecProcessingStoped) {
        ILBCENC_DPRINT("%d GOT MESSAGE USN_DSPACK_STOP \n",__LINE__);
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pComponentPrivate->pInputBufferList->bBufferPending[i]) {
#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                  pComponentPrivate->pInputBufferList->pBufHdr[i]->pBuffer,
                                  0,
                                  PERF_ModuleHLMM);
#endif
                pComponentPrivate->cbInfo.EmptyBufferDone (pHandle,
                                                        pHandle->pApplicationPrivate,
                                                        pComponentPrivate->pInputBufferList->pBufHdr[i]);
                pComponentPrivate->nEmptyBufferDoneCount++;
                ILBCENC_ClearPending(pComponentPrivate, pComponentPrivate->pInputBufferList->pBufHdr[i], OMX_DirInput,__LINE__);
            }
        }

        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pComponentPrivate->pOutputBufferList->bBufferPending[i]) {
#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                  pComponentPrivate->pOutputBufferList->pBufHdr[i]->pBuffer,
                                  pComponentPrivate->pOutputBufferList->pBufHdr[i]->nFilledLen,
                                  PERF_ModuleHLMM);
#endif
                pComponentPrivate->cbInfo.FillBufferDone(pHandle,
                                                        pHandle->pApplicationPrivate,
                                                        pComponentPrivate->pOutputBufferList->pBufHdr[i]);
                pComponentPrivate->nFillBufferDoneCount++;     

                ILBCENC_ClearPending(pComponentPrivate, pComponentPrivate->pOutputBufferList->pBufHdr[i], OMX_DirOutput,__LINE__);
            }
        }
        
        if (!pComponentPrivate->bNoIdleOnStop) {        
            pComponentPrivate->nNumOutputBufPending=0;
            pComponentPrivate->nHoldLength = 0;
            pComponentPrivate->InBuf_Eos_alreadysent =0;
       
            OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer);
       
#ifdef RESOURCE_MANAGER_ENABLED
            eError = RMProxy_NewSendCommand(pHandle, 
                                            RMProxy_StateSet, 
                                            OMX_ILBC_Encoder_COMPONENT, 
                                            OMX_StateIdle, 
                                            3456, 
                                            NULL);
#endif
            pComponentPrivate->curState = OMX_StateIdle;
            if (pComponentPrivate->bPreempted == 0) {
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                          pComponentPrivate->pHandle->pApplicationPrivate,
                                                          OMX_EventCmdComplete,
                                                          OMX_CommandStateSet,
                                                          pComponentPrivate->curState,
                                                          NULL);
            } else {
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                          pComponentPrivate->pHandle->pApplicationPrivate,
                                                          OMX_EventError,
                                                          OMX_ErrorResourcesPreempted,
                                                          0,
                                                          NULL);
            }
        }else{
            pComponentPrivate->bNoIdleOnStop= OMX_FALSE;
        }
    }
    else if(event == EMMCodecDspMessageRecieved) {
        ILBCENC_DPRINT("%d commandedState  = %ld\n",__LINE__,(OMX_U32)args[0]);
        ILBCENC_DPRINT("%d arg1 = %ld\n",__LINE__,(OMX_U32)args[1]);
        ILBCENC_DPRINT("%d arg2 = %ld\n",__LINE__,(OMX_U32)args[2]);

        if(0x0500 == (OMX_U32)args[2]) {
            ILBCENC_DPRINT("%d EMMCodecDspMessageRecieved\n",__LINE__);
        }
    }
    else if(event == EMMCodecAlgCtrlAck) {
        ILBCENC_DPRINT("%d GOT MESSAGE USN_DSPACK_ALGCTRL \n",__LINE__);
    }
    else if (event == EMMCodecDspError) {
#ifdef _ERROR_PROPAGATION__
        /* Cheking for MMU_fault */
        if(((int)args[4] == USN_ERR_UNKNOWN_MSG) && (args[5] ==(void*) NULL)) {
            pComponentPrivate->bIsInvalidState=OMX_TRUE;
            pComponentPrivate->curState = OMX_StateInvalid;
            pHandle = pComponentPrivate->pHandle;
            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                      pHandle->pApplicationPrivate,
                                                      OMX_EventError,
                                                      OMX_ErrorInvalidState,
                                                      0x2,
                                                      NULL);
        }
#endif
        if(((int)args[4] == USN_ERR_WARNING) && ((int)args[5] == IUALG_WARN_PLAYCOMPLETED)) {
            ILBCENC_DPRINT("IUALG_WARN_PLAYCOMPLETED Received\n");
            if(pComponentPrivate->LastOutbuf!=NULL){
                pComponentPrivate->LastOutbuf->nFlags = OMX_BUFFERFLAG_EOS;
            }
            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                pHandle->pApplicationPrivate,
                                                OMX_EventBufferFlag,
                                                (OMX_U32)NULL,
                                                OMX_BUFFERFLAG_EOS,
                                                NULL);
        }
        if(((OMX_U32)args[4] == USN_ERR_NONE) && (args[5] == (void*)NULL)){
                OMXDBG_PRINT(stderr, ERROR, 4, 0, "%d :: UTIL: MMU_Fault \n",__LINE__);
                ILBCENC_FatalErrorRecover(pComponentPrivate);
        }
        if((int)args[5] == IUALG_ERR_GENERAL) {
            char *pArgs = "";
            ILBCENC_EPRINT( "Algorithm error. Cannot continue" );
            ILBCENC_EPRINT("%d arg5 = %p\n",__LINE__,args[5]);                        
            ILBCENC_EPRINT("%d LCML_Callback: IUALG_ERR_GENERAL\n",__LINE__);
            pHandle = pComponentPrivate->pHandle;
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
#ifndef UNDER_CE
            eError = LCML_ControlCodec(
                                       ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                       MMCodecControlStop,(void *)pArgs);
            if(eError != OMX_ErrorNone) {
                ILBCENC_EPRINT("%d: Error Occurred in Codec Stop..\n",
                               __LINE__);
                goto EXIT;
            }
#ifdef RESOURCE_MANAGER_ENABLED
            eError = RMProxy_NewSendCommand(pHandle,
                                            RMProxy_StateSet,
                                            OMX_ILBC_Encoder_COMPONENT,
                                            OMX_StateIdle,
                                            3456,
                                            NULL);
#endif
            ILBCENC_DPRINT("%d ILBCENC: Codec has been Stopped here\n",__LINE__);
            pComponentPrivate->curState = OMX_StateIdle;
            pComponentPrivate->cbInfo.EventHandler(
                                                      pHandle, pHandle->pApplicationPrivate,
                                                      OMX_EventCmdComplete, OMX_ErrorNone,0, NULL);
#else
            pComponentPrivate->cbInfo.EventHandler(
                                                      pHandle, pHandle->pApplicationPrivate,
                                                      OMX_EventError, OMX_ErrorUndefined,0, NULL);
#endif
        }
        if((int)args[5] == IUALG_ERR_DATA_CORRUPT ){
            char *pArgs = "";
            ILBCENC_EPRINT( "Algorithm error. Corrupt data" );
            ILBCENC_EPRINT("%d arg5 = %p\n",__LINE__,args[5]);
            ILBCENC_EPRINT("%d LCML_Callback: IUALG_ERR_DATA_CORRUPT\n",__LINE__);
            pHandle = pComponentPrivate->pHandle;
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
#ifndef UNDER_CE
            eError = LCML_ControlCodec(
                                       ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                       MMCodecControlStop,(void *)pArgs);
            if(eError != OMX_ErrorNone) {
                ILBCENC_EPRINT("%d: Error Occurred in Codec Stop..\n",__LINE__);
                goto EXIT;
            }
#ifdef RESOURCE_MANAGER_ENABLED
            eError = RMProxy_NewSendCommand(pHandle,
                                            RMProxy_StateSet,
                                            OMX_ILBC_Encoder_COMPONENT,
                                            OMX_StateIdle,
                                            3456,
                                            NULL);
#endif
            ILBCENC_DPRINT("%d ILBCENC: Codec has been Stopped here\n",__LINE__);
            pComponentPrivate->curState = OMX_StateIdle;
            pComponentPrivate->cbInfo.EventHandler(
                                                      pHandle, pHandle->pApplicationPrivate,
                                                      OMX_EventCmdComplete, OMX_ErrorNone,0, NULL);
#else
            pComponentPrivate->cbInfo.EventHandler(
                                                      pHandle, pHandle->pApplicationPrivate,
                                                      OMX_EventError, OMX_ErrorUndefined,0, NULL);
#endif
        }
    }
    else if (event == EMMCodecProcessingPaused) {
#ifdef RESOURCE_MANAGER_ENABLED
            eError = RMProxy_NewSendCommand(pHandle,
                                            RMProxy_StateSet,
                                            OMX_ILBC_Encoder_COMPONENT,
                                            OMX_StatePause,
                                            3456,
                                            NULL);
#endif
        pComponentPrivate->curState = OMX_StatePause;
        pComponentPrivate->cbInfo.EventHandler( pComponentPrivate->pHandle,
                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete,
                                                   OMX_CommandStateSet,
                                                   pComponentPrivate->curState,
                                                   NULL);
    }
#ifdef _ERROR_PROPAGATION__
    else if (event ==EMMCodecInitError){
        /* Cheking for MMU_fault */
        if(((int)args[4] == USN_ERR_UNKNOWN_MSG) && (args[5] == (void*)NULL)) {
            pComponentPrivate->bIsInvalidState=OMX_TRUE;
            pComponentPrivate->curState = OMX_StateInvalid;
            pHandle = pComponentPrivate->pHandle;
            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                      pHandle->pApplicationPrivate,
                                                      OMX_EventError,
                                                      OMX_ErrorInvalidState,
                                                      0x2,
                                                      NULL);
        }
    }
    else if (event ==EMMCodecInternalError){
        /* Cheking for MMU_fault */
        if(((int)args[4] == USN_ERR_UNKNOWN_MSG) && (args[5] == (void*)NULL)) {
            ILBCENC_EPRINT("%d UTIL: MMU_Fault \n",__LINE__);
            pComponentPrivate->bIsInvalidState=OMX_TRUE;
            pComponentPrivate->curState = OMX_StateInvalid;
            pHandle = pComponentPrivate->pHandle;
            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                      pHandle->pApplicationPrivate,
                                                      OMX_EventError,
                                                      OMX_ErrorInvalidState,
                                                      0x2,
                                                      NULL);
        }

    }
#endif
 EXIT:
    ILBCENC_DPRINT("%d Exiting the ILBCENC_LCMLCallback Function\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);

    return eError;
}

/* ================================================================================= */
/**
  *  ILBCENC_GetLCMLHandle()
  *
  * @retval OMX_HANDLETYPE
  */
/* ================================================================================= */
#ifndef UNDER_CE
OMX_HANDLETYPE ILBCENC_GetLCMLHandle(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE (*fpGetHandle)(OMX_HANDLETYPE);
    OMX_HANDLETYPE pHandle = NULL;
    void *handle = NULL;
    const char *error = NULL;

    ILBCENC_DPRINT("%d Entering ILBCENC_GetLCMLHandle..\n",__LINE__);
    handle = dlopen("libLCML.so", RTLD_LAZY);
    if (!handle) {
        fputs(dlerror(), stderr);
        goto EXIT;
    }
    fpGetHandle = dlsym (handle, "GetHandle");
    if ((error = dlerror()) != NULL) {
        fputs(error, stderr);
        goto EXIT;
    }
    eError = (*fpGetHandle)(&pHandle);
    if(eError != OMX_ErrorNone) {
        eError = OMX_ErrorUndefined;
        ILBCENC_EPRINT("%d OMX_ErrorUndefined...\n",__LINE__);
        pHandle = NULL;
        goto EXIT;
    }
    pComponentPrivate = (ILBCENC_COMPONENT_PRIVATE*)pComponentPrivate;
    ((LCML_DSP_INTERFACE*)pHandle)->pComponentPrivate = pComponentPrivate;
    
    pComponentPrivate->ptrLibLCML=handle;           /* saving LCML lib pointer  */
    
 EXIT:
    ILBCENC_DPRINT("%d Exiting ILBCENC_GetLCMLHandle..\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return pHandle;
}

#else
/*WINDOWS Explicit dll load procedure*/
OMX_HANDLETYPE ILBCENC_GetLCMLHandle(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    typedef OMX_ERRORTYPE (*LPFNDLLFUNC1)(OMX_HANDLETYPE);
    OMX_HANDLETYPE pHandle = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    LPFNDLLFUNC1 fpGetHandle1;
        
    if (!fpGetHandle1) {
        // handle the error
        return pHandle;
    }
    // call the function
    eError = fpGetHandle1(&pHandle);
    if(eError != OMX_ErrorNone) {
        eError = OMX_ErrorUndefined;
        ILBCENC_EPRINT("eError != OMX_ErrorNone...\n");
        pHandle = NULL;
        return pHandle;
    }

    ((LCML_DSP_INTERFACE*)pHandle)->pComponentPrivate = pComponentPrivate;
    return pHandle;
}
#endif

/* ================================================================================= */
/**
* @fn ILBCENC_SetPending() description for ILBCENC_SetPending
ILBCENC_SetPending().
This component is called when a buffer is queued to the LCML
* @param pComponent  handle for this instance of the component
*
* @pre
*
* @post
*
* @return OMX_ERRORTYPE
*/
/* ================================================================================ */
void ILBCENC_SetPending(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                         OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir, OMX_U32 lineNumber)
{
    OMX_U16 i = 0;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 1;
                ILBCENC_DPRINT("****INPUT BUFFER %d IS PENDING Line %ld******\n",i,lineNumber);
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 1;
                ILBCENC_DPRINT("****OUTPUT BUFFER %d IS PENDING Line %ld*****\n",i,lineNumber);
            }
        }
    }
}
/* ================================================================================= */
/**
* @fn ILBCENC_ClearPending() description for ILBCENC_ClearPending
ILBCENC_ClearPending().
This component is called when a buffer is returned from the LCML
* @param pComponent  handle for this instance of the component
*
* @pre
*
* @post
*
* @return OMX_ERRORTYPE
*/
/* ================================================================================ */
void ILBCENC_ClearPending(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                           OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir, OMX_U32 lineNumber)
{
    OMX_U16 i = 0;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 0;
                ILBCENC_DPRINT("****INPUT BUFFER %d IS RECLAIMED Line %ld*****\n",i,lineNumber);
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 0;
                ILBCENC_DPRINT("****OUTPUT BUFFER %d IS RECLAIMED Line %ld*****\n",i,lineNumber);
            }
        }
    }
}
/* ================================================================================= */
/**
* @fn ILBCENC_IsPending() description for ILBCENC_IsPending
ILBCENC_IsPending().
This method returns the pending status to the buffer
* @param pComponent  handle for this instance of the component
*
* @pre
*
* @post
*
* @return OMX_ERRORTYPE
*/
/* ================================================================================ */
OMX_U32 ILBCENC_IsPending(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                           OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir)
{
    OMX_U16 i = 0;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                return pComponentPrivate->pInputBufferList->bBufferPending[i];
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            ILBCENC_DPRINT("pBufHdr = %p\n",pBufHdr);
            ILBCENC_DPRINT("pOutputBufferList->pBufHdr[i] = %p\n",pComponentPrivate->pOutputBufferList->pBufHdr[i]);
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                ILBCENC_DPRINT("returning %d\n",pComponentPrivate->pOutputBufferList->bBufferPending[i]);
                return pComponentPrivate->pOutputBufferList->bBufferPending[i];
            }
        }
    }
    return -1;
}
/* ================================================================================= */
/**
* @fn ILBCENC_IsValid() description for ILBCENC_IsValid
ILBCENC_IsValid().
This method checks to see if a buffer returned from the LCML is valid.
* @param pComponent  handle for this instance of the component
*
* @pre
*
* @post
*
* @return OMX_ERRORTYPE
*/
/* ================================================================================ */
OMX_U32 ILBCENC_IsValid(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                         OMX_U8 *pBuffer, OMX_DIRTYPE eDir)
{
    OMX_U16 i = 0;
    int found=0;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBuffer == pComponentPrivate->pInputBufferList->pBufHdr[i]->pBuffer) {
                found = 1;
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBuffer == pComponentPrivate->pOutputBufferList->pBufHdr[i]->pBuffer) {
                found = 1;
            }
        }
    }
    return found;
}
/* ========================================================================== */
/**
* @ILBCENC_FillLCMLInitParamsEx() This function is used by the component thread to
* fill the all of its initialization parameters, buffer deatils  etc
* to LCML structure,
*
* @param pComponent  handle for this instance of the component
* @param plcml_Init  pointer to LCML structure to be filled
*
* @pre
*
* @post
*
* @return none
*/
/* ========================================================================== */
OMX_ERRORTYPE ILBCENC_FillLCMLInitParamsEx(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0,nIpBufSize = 0,nOpBuf = 0,nOpBufSize = 0;
    OMX_BUFFERHEADERTYPE *pTemp = NULL;
    LCML_DSP_INTERFACE *pHandle = (LCML_DSP_INTERFACE *)pComponent;
    ILBCENC_COMPONENT_PRIVATE *pComponentPrivate = pHandle->pComponentPrivate;
    ILBCENC_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;
    OMX_U16 i = 0;
    OMX_U32 size_lcml = 0;

    ILBCENC_DPRINT("%d ILBCENC_FillLCMLInitParamsEx\n",__LINE__);
    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nIpBufSize = pComponentPrivate->pPortDef[ILBCENC_INPUT_PORT]->nBufferSize;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    nOpBufSize = pComponentPrivate->pPortDef[ILBCENC_OUTPUT_PORT]->nBufferSize;
    ILBCENC_DPRINT("%d ------ Buffer Details -----------\n",__LINE__);
    ILBCENC_DPRINT("%d Input  Buffer Count = %ld\n",__LINE__,nIpBuf);
    ILBCENC_DPRINT("%d Input  Buffer Size = %ld\n",__LINE__,nIpBufSize);
    ILBCENC_DPRINT("%d Output Buffer Count = %ld\n",__LINE__,nOpBuf);
    ILBCENC_DPRINT("%d Output Buffer Size = %ld\n",__LINE__,nOpBufSize);
    ILBCENC_DPRINT("%d ------ Buffer Details ------------\n",__LINE__);
    /* Allocate memory for all input buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = nIpBuf * sizeof(ILBCENC_LCML_BUFHEADERTYPE);
    
    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml,ILBCENC_LCML_BUFHEADERTYPE);

    pComponentPrivate->pLcmlBufHeader[ILBCENC_INPUT_PORT] = pTemp_lcml;
    for (i=0; i<nIpBuf; i++) {
        ILBCENC_EPRINT("%d INPUT--------- Inside Ip Loop\n",__LINE__);
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        /*        pTemp->nAllocLen = nIpBufSize;*/
        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = ILBCENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = ILBCENC_MINOR_VER;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = ILBCENC_NOT_USED;
        pTemp_lcml->buffer = pTemp;
        ILBCENC_DPRINT("%d pTemp_lcml->buffer->pBuffer = %p \n",__LINE__,pTemp_lcml->buffer->pBuffer);
        pTemp_lcml->eDir = OMX_DirInput;

        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pBufferParam, sizeof(ILBCENC_ParamStruct), ILBCENC_ParamStruct);
        if(NULL == pTemp_lcml->pBufferParam){
            ILBCENC_CleanupInitParams(pComponent);
            return OMX_ErrorInsufficientResources;
        }

        pTemp_lcml->pBufferParam->usNbFrames=0;
        pTemp_lcml->pBufferParam->pParamElem=NULL;
        pTemp_lcml->pFrameParam=NULL;
        
        OMX_MALLOC_GENERIC(pTemp_lcml->pDmmBuf, DMM_BUFFER_OBJ);

        pTemp->nFlags = ILBCENC_NORMAL_BUFFER;
        pTemp_lcml++;
    }

    /* Allocate memory for all output buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = nOpBuf * sizeof(ILBCENC_LCML_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml,ILBCENC_LCML_BUFHEADERTYPE);
    
    pComponentPrivate->pLcmlBufHeader[ILBCENC_OUTPUT_PORT] = pTemp_lcml;
    for (i=0; i<nOpBuf; i++) {
        ILBCENC_DPRINT("%d OUTPUT--------- Inside Op Loop\n",__LINE__);
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        /*        pTemp->nAllocLen = nOpBufSize;*/
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = ILBCENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = ILBCENC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = ILBCENC_NOT_USED;
        pTemp_lcml->buffer = pTemp;
        ILBCENC_DPRINT("%d pTemp_lcml->buffer->pBuffer = %p \n",__LINE__,pTemp_lcml->buffer->pBuffer);
        pTemp_lcml->eDir = OMX_DirOutput;

        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pBufferParam,sizeof(ILBCENC_ParamStruct),ILBCENC_ParamStruct);
        if(NULL == pTemp_lcml->pBufferParam){
            ILBCENC_CleanupInitParams(pComponent);
            return OMX_ErrorInsufficientResources;
        }
        pTemp_lcml->pBufferParam->usNbFrames=0;
        pTemp_lcml->pBufferParam->pParamElem=NULL;
        pTemp_lcml->pFrameParam=NULL;
        
        OMX_MALLOC_GENERIC(pTemp_lcml->pDmmBuf, DMM_BUFFER_OBJ);

        pTemp->nFlags = ILBCENC_NORMAL_BUFFER;
        pTemp_lcml++;
    }

    pComponentPrivate->bInitParamsInitialized = 1;
 EXIT:
    if(eError != OMX_ErrorNone)
        ILBCENC_CleanupInitParams(pComponent);
    ILBCENC_DPRINT("%d Exiting ILBCENC_FillLCMLInitParamsEx\n",__LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
    return eError;
}


/** ========================================================================
*  OMX_DmmMap () method is used to allocate the memory using DMM.
*
*  @param ProcHandle -  Component identification number
*  @param size  - Buffer header address, that needs to be sent to codec
*  @param pArmPtr - Message used to send the buffer to codec
*  @param pDmmBuf - buffer id
*
*  @retval OMX_ErrorNone  - Success
*          OMX_ErrorHardware  -  Hardware Error
** ==========================================================================*/
OMX_ERRORTYPE OMX_DmmMap(DSP_HPROCESSOR ProcHandle,
                     int size,
                     void* pArmPtr,
                     DMM_BUFFER_OBJ* pDmmBuf)
{
    OMX_ERRORTYPE eError = OMX_ErrorUndefined;
    int status = 0;
    int nSizeReserved = 0;

    if(pDmmBuf == NULL)
    {
        ILBCENC_EPRINT("pBuf is NULL\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if(pArmPtr == NULL)
    {
        ILBCENC_EPRINT("pBuf is NULL\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if(ProcHandle == NULL)
    {
        ILBCENC_EPRINT("ProcHandle is NULL\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    /* Allocate */
    pDmmBuf->pAllocated = pArmPtr;

    /* Reserve */
    nSizeReserved = ROUND_TO_PAGESIZE(size) + 2*DMM_PAGE_SIZE ;


    status = DSPProcessor_ReserveMemory(ProcHandle, nSizeReserved, &(pDmmBuf->pReserved));
/*    printf("OMX Reserve DSP: %p\n",pDmmBuf->pReserved);*/

    if(DSP_FAILED(status))
    {
        ILBCENC_EPRINT("DSPProcessor_ReserveMemory() failed - error 0x%x", (int)status);
        eError = OMX_ErrorHardware;
        goto EXIT;
    }
    pDmmBuf->nSize = size;
    ILBCENC_DPRINT(" DMM MAP Reserved: %p, size 0x%x (%d)\n", pDmmBuf->pReserved,nSizeReserved,nSizeReserved);

    /* Map */
    status = DSPProcessor_Map(ProcHandle,
                              pDmmBuf->pAllocated,/* Arm addres of data to Map on DSP*/
                              OMX_GET_SIZE_DSPALIGN(size), /* size to Map on DSP*/
                              pDmmBuf->pReserved, /* reserved space */
                              &(pDmmBuf->pMapped), /* returned map pointer */
                              ALIGNMENT_CHECK);
    if(DSP_FAILED(status))
    {
        ILBCENC_EPRINT("DSPProcessor_Map() failed - error 0x%x", (int)status);
        eError = OMX_ErrorHardware;
        goto EXIT;
    }
    ILBCENC_DPRINT("DMM Mapped: %p, size 0x%x (%d)\n",pDmmBuf->pMapped, size,size);

    /* Issue an initial memory flush to ensure cache coherency */
    status = DSPProcessor_FlushMemory(ProcHandle, pDmmBuf->pAllocated, size, 0);
    if(DSP_FAILED(status))
    {
        ILBCENC_EPRINT("Unable to flush mapped buffer: error 0x%x",(int)status);
        goto EXIT;
    }
    eError = OMX_ErrorNone;

EXIT:
   return eError;
}

/** ========================================================================
*  OMX_DmmUnMap () method is used to de-allocate the memory using DMM.
*
*  @param ProcHandle -  Component identification number
*  @param pMapPtr  - Map address
*  @param pResPtr - reserve adress
*
*  @retval OMX_ErrorNone  - Success
*          OMX_ErrorHardware  -  Hardware Error
** ==========================================================================*/
OMX_ERRORTYPE OMX_DmmUnMap(DSP_HPROCESSOR ProcHandle, void* pMapPtr, void* pResPtr)
{
    int status = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
/*    printf("OMX UnReserve DSP: %p\n",pResPtr);*/
    if(pMapPtr == NULL)
    {
        ILBCENC_EPRINT("pMapPtr is NULL\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if(pResPtr == NULL)
    {
        ILBCENC_EPRINT("pResPtr is NULL\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if(ProcHandle == NULL)
    {
        ILBCENC_EPRINT("--ProcHandle is NULL\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    status = DSPProcessor_UnMap(ProcHandle,pMapPtr);
    if(DSP_FAILED(status))
    {
        ILBCENC_EPRINT("DSPProcessor_UnMap() failed - error 0x%x",(int)status);
    }

    ILBCENC_DPRINT("unreserving  structure =0x%p\n",pResPtr );
    status = DSPProcessor_UnReserveMemory(ProcHandle,pResPtr);
    if(DSP_FAILED(status))
    {
        printf("DSPProcessor_UnReserveMemory() failed - error 0x%x", (int)status);
    }

EXIT:
    return eError;
}   
#ifdef RESOURCE_MANAGER_ENABLED
void ILBCENC_ResourceManagerCallback(RMPROXY_COMMANDDATATYPE cbData)
{
    OMX_COMMANDTYPE Cmd = OMX_CommandStateSet;
    OMX_STATETYPE state = OMX_StateIdle;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)cbData.hComponent;
    ILBCENC_COMPONENT_PRIVATE *pCompPrivate = NULL;

    pCompPrivate = (ILBCENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    if (*(cbData.RM_Error) == OMX_RmProxyCallback_ResourcesPreempted) {
        if (pCompPrivate->curState == OMX_StateExecuting || 
            pCompPrivate->curState == OMX_StatePause) {
            write (pCompPrivate->cmdPipe[1], &Cmd, sizeof(Cmd));
            write (pCompPrivate->cmdDataPipe[1], &state ,sizeof(OMX_U32));

            pCompPrivate->bPreempted = 1;
        }
    }
    else if (*(cbData.RM_Error) == OMX_RmProxyCallback_ResourcesAcquired){
        pCompPrivate->cbInfo.EventHandler (
                                           pHandle, pHandle->pApplicationPrivate,
                                           OMX_EventResourcesAcquired, 0,0,
                                           NULL);        
    }
    else if (*(cbData.RM_Error) == OMX_RmProxyCallback_FatalError) {
        ILBCENC_EPRINT("%d :RM Fatal Error:\n", __LINE__);
        ILBCENC_FatalErrorRecover(pCompPrivate);
    }
}
#endif

void ILBCENC_FatalErrorRecover(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate) {
#ifdef RESOURCE_MANAGER_ENABLED
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle,
                                    RMProxy_FreeResource,
                                    OMX_ILBC_Encoder_COMPONENT, 0, 3456, NULL);

    eError = RMProxy_Deinitalize();
    if (eError != OMX_ErrorNone) {
        ILBCENC_EPRINT ("%d ::RMProxy_Deinitalize error = %d\n", __LINE__, eError);
    }
#endif

    pComponentPrivate->curState = OMX_StateInvalid;
    pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                           OMX_EventError,
                                           OMX_ErrorInvalidState,
                                           OMX_TI_ErrorSevere,
                                           NULL);
    if (pComponentPrivate->DSPMMUFault == OMX_FALSE){
        pComponentPrivate->DSPMMUFault = OMX_TRUE;
        ILBCENC_CleanupInitParams(pComponentPrivate->pHandle);
    }
    ILBCENC_DPRINT ("%d ::Completed FatalErrorRecover Entering Invalid State\n", __LINE__);
}

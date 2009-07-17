
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
 * @file OMX_WbAmrEnc_Utils.c
 *
 * This file implements WBAMR Encoder Component Specific APIs and its functionality
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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <malloc.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>

#include <semaphore.h>
#endif

#include <dbapi.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/*-------program files ----------------------------------------*/
#include "OMX_WbAmrEncoder.h"
#include "OMX_WbAmrEnc_Utils.h"
#include "wbamrencsocket_ti.h"
#include <encode_common_ti.h>
#include "OMX_WbAmrEnc_CompThread.h"
#include "usn.h"
#include "LCML_DspCodec.h"

#ifdef RESOURCE_MANAGER_ENABLED
#include <ResourceManagerProxyAPI.h>
#endif

#ifdef WBAMRENC_DEBUGMEM
 void *arr[500];
 int lines[500];
 int bytes[500];
 char file[500][50];


void * mymalloc(int line, char *s, int size);
int mynewfree(void *dp, int line, char *s);

#define newmalloc(x) mymalloc(__LINE__,__FILE__,x)
#define newfree(z) myfree(z,__LINE__,__FILE__)

void * mymalloc(int line, char *s, int size)
{
    void *p;    
    int e=0;
    /*   p = malloc(size);*/
    p = calloc(1,size);
    if(p==NULL){
        WBAMRENC_DPRINT("Memory not available\n");
        exit(1);
    }
    else{
        while((lines[e]!=0)&& (e<500) ){
            e++;
        }
        arr[e]=p;
        lines[e]=line;
        bytes[e]=size;
        strcpy(file[e],s);
        WBAMRENC_DPRINT("Allocating %d bytes on address %p, line %d file %s\n", size, p, line, s);
        return p;
    }
}

int myfree(void *dp, int line, char *s){
    int q;
    if(dp==NULL){
        WBAMRENC_DPRINT("NULL can't be deleted\n");
        return 0;
    }
    for(q=0;q<500;q++){
        if(arr[q]==dp){
            WBAMRENC_DPRINT("Deleting %d bytes on address %p, line %d file %s\n", bytes[q],dp, line, s);
            free(dp);
            dp = NULL;
            lines[q]=0;
            strcpy(file[q],"");
            break;
        }            
    }    
    if(500==q)
        WBAMRENC_DPRINT("\n\nPointer not found. Line:%d    File%s!!\n\n",line, s);
    return 1;
}
#else
#define newmalloc(x) malloc(x)
#define newfree(z) free(z)
#endif

/* ========================================================================== */
/**
 * @WBAMRENC_FillLCMLInitParams () This function is used by the component thread to
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

OMX_ERRORTYPE WBAMRENC_FillLCMLInitParams(OMX_HANDLETYPE pComponent,
                                          LCML_DSP *plcml_Init, OMX_U16 arr[])
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf,nIpBufSize,nOpBuf,nOpBufSize;
    OMX_BUFFERHEADERTYPE *pTemp;
    LCML_DSP_INTERFACE *pHandle = (LCML_DSP_INTERFACE *)pComponent;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate = pHandle->pComponentPrivate;
    WBAMRENC_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;
    OMX_U32 i;
    OMX_U32 size_lcml;
    OMX_U8 *pBufferParamTemp;
    char *pTemp_char = NULL;

    WBAMRENC_DPRINT("%d :: Entering WBAMRENC_FillLCMLInitParams\n",__LINE__);
    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    pComponentPrivate->nRuntimeInputBuffers = nIpBuf;
    
    nIpBufSize = pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->nBufferSize;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    pComponentPrivate->nRuntimeOutputBuffers = nOpBuf;
    
    nOpBufSize = pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->nBufferSize;
    WBAMRENC_DPRINT("%d :: ------ Buffer Details -----------\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Input  Buffer Count = %ld\n",__LINE__,nIpBuf);
    WBAMRENC_DPRINT("%d :: Input  Buffer Size = %ld\n",__LINE__,nIpBufSize);
    WBAMRENC_DPRINT("%d :: Output Buffer Count = %ld\n",__LINE__,nOpBuf);
    WBAMRENC_DPRINT("%d :: Output Buffer Size = %ld\n",__LINE__,nOpBufSize);
    WBAMRENC_DPRINT("%d :: ------ Buffer Details ------------\n",__LINE__);
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

    plcml_Init->NodeInfo.AllUUIDs[0].uuid = &WBAMRENCSOCKET_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[0].DllName,WBAMRENC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[0].eDllType = DLL_NODEOBJECT;

    plcml_Init->NodeInfo.AllUUIDs[1].uuid = &WBAMRENCSOCKET_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[1].DllName,WBAMRENC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[1].eDllType = DLL_DEPENDENT;

    plcml_Init->NodeInfo.AllUUIDs[2].uuid = &USN_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[2].DllName,WBAMRENC_USN_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[2].eDllType = DLL_DEPENDENT;
    plcml_Init->DeviceInfo.TypeofDevice = 0;

    if(pComponentPrivate->dasfMode == 1) {
        WBAMRENC_DPRINT("%d :: Codec is configuring to DASF mode\n",__LINE__);
        WBAMRENC_OMX_MALLOC(pComponentPrivate->strmAttr, LCML_STRMATTR);
        pComponentPrivate->strmAttr->uSegid = WBAMRENC_DEFAULT_SEGMENT;
        pComponentPrivate->strmAttr->uAlignment = 0;
        pComponentPrivate->strmAttr->uTimeout = WBAMRENC_SN_TIMEOUT;
        pComponentPrivate->strmAttr->uBufsize = WBAMRENC_INPUT_BUFFER_SIZE_DASF;
        pComponentPrivate->strmAttr->uNumBufs = WBAMRENC_NUM_INPUT_BUFFERS_DASF;
        pComponentPrivate->strmAttr->lMode = STRMMODE_PROCCOPY;
        /* Device is Configuring to DASF Mode */
        plcml_Init->DeviceInfo.TypeofDevice = 1;
        /* Device is Configuring to Record Mode */
        plcml_Init->DeviceInfo.TypeofRender = 1;

        if(pComponentPrivate->acdnMode == 1) {
            /* ACDN mode */
            plcml_Init->DeviceInfo.AllUUIDs[0].uuid = &ACDN_TI_UUID;
        }
        else {
            /* DASF/TeeDN mode */
            plcml_Init->DeviceInfo.AllUUIDs[0].uuid = &DCTN_TI_UUID;
        }
        plcml_Init->DeviceInfo.DspStream = pComponentPrivate->strmAttr;
    }

    /*copy the other information*/
    plcml_Init->SegID = WBAMRENC_DEFAULT_SEGMENT;
    plcml_Init->Timeout = WBAMRENC_SN_TIMEOUT;
    plcml_Init->Alignment = 0;
    plcml_Init->Priority = WBAMRENC_SN_PRIORITY;
    plcml_Init->ProfileID = -1;

    /* Setting Creat Phase Parameters here */
    arr[0] = WBAMRENC_STREAM_COUNT;
    arr[1] = WBAMRENC_INPUT_PORT;

    if(pComponentPrivate->dasfMode == 1) {
        arr[2] = WBAMRENC_INSTRM;
        arr[3] = WBAMRENC_NUM_INPUT_BUFFERS_DASF;
    }
    else {
        arr[2] = WBAMRENC_DMM;
        if (pComponentPrivate->pInputBufferList->numBuffers) {
            arr[3] = (OMX_U16) pComponentPrivate->pInputBufferList->numBuffers;
        }
        else {
            arr[3] = 1;
        }
    }

    arr[4] = WBAMRENC_OUTPUT_PORT;
    arr[5] = WBAMRENC_DMM;
    if (pComponentPrivate->pOutputBufferList->numBuffers) {
        arr[6] = (OMX_U16) pComponentPrivate->pOutputBufferList->numBuffers;
    }
    else {
        arr[6] = 1;
    }

    WBAMRENC_DPRINT("%d :: Codec is configuring to WBAMR mode\n",__LINE__);
    arr[7] = WBAMRENC_WBAMR;

    if(pComponentPrivate->frameMode == WBAMRENC_MIMEMODE) {
        WBAMRENC_DPRINT("%d :: Codec is configuring MIME mode\n",__LINE__);
        arr[8] = WBAMRENC_MIMEMODE;
    }
    else if(pComponentPrivate->frameMode == WBAMRENC_IF2 ){
        WBAMRENC_DPRINT("%d :: Codec is configuring IF2 mode\n",__LINE__);
        arr[8] = WBAMRENC_IF2;  
    }
    else {
        WBAMRENC_DPRINT("%d :: Codec is configuring FORMAT CONFORMANCE mode\n",__LINE__);
        arr[8] = WBAMRENC_FORMATCONFORMANCE;
    }

    arr[9] = END_OF_CR_PHASE_ARGS;

    plcml_Init->pCrPhArgs = arr;

    /* Allocate memory for all input buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = nIpBuf * sizeof(WBAMRENC_LCML_BUFHEADERTYPE);
    

    WBAMRENC_OMX_MALLOC_SIZE(pTemp_lcml,size_lcml,WBAMRENC_LCML_BUFHEADERTYPE);
    
    pComponentPrivate->pLcmlBufHeader[WBAMRENC_INPUT_PORT] = pTemp_lcml;
    for (i=0; i<nIpBuf; i++) {
        WBAMRENC_DPRINT("%d :: INPUT--------- Inside Ip Loop\n",__LINE__);
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = WBAMRENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = WBAMRENC_MINOR_VER;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = WBAMRENC_NOT_USED;
        pTemp_lcml->buffer = pTemp;
        WBAMRENC_DPRINT("%d :: pTemp_lcml->buffer->pBuffer = %p \n",__LINE__,pTemp_lcml->buffer->pBuffer);
        pTemp_lcml->eDir = OMX_DirInput;

        WBAMRENC_OMX_MALLOC_SIZE(pBufferParamTemp,(sizeof(WBAMRENC_ParamStruct) + DSP_CACHE_ALIGNMENT),
                                 OMX_U8);

        pTemp_lcml->pBufferParam =  (WBAMRENC_ParamStruct*)(pBufferParamTemp + EXTRA_BYTES);
 
        pTemp_lcml->pBufferParam->usNbFrames=0;
        pTemp_lcml->pBufferParam->pParamElem=NULL;
        pTemp_lcml->pFrameParam=NULL;
        
        WBAMRENC_OMX_MALLOC(pTemp_lcml->pDmmBuf, DMM_BUFFER_OBJ);
         
        pTemp->nFlags = WBAMRENC_NORMAL_BUFFER;
        pTemp++;
        pTemp_lcml++;
    }

    /* Allocate memory for all output buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = nOpBuf * sizeof(WBAMRENC_LCML_BUFHEADERTYPE);

    WBAMRENC_OMX_MALLOC_SIZE(pTemp_lcml, size_lcml,WBAMRENC_LCML_BUFHEADERTYPE);
    
    pComponentPrivate->pLcmlBufHeader[WBAMRENC_OUTPUT_PORT] = pTemp_lcml;
    for (i=0; i<nOpBuf; i++) {
        WBAMRENC_DPRINT("%d :: OUTPUT--------- Inside Op Loop\n",__LINE__);
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = WBAMRENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = WBAMRENC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = WBAMRENC_NOT_USED;
        pTemp_lcml->buffer = pTemp;
        WBAMRENC_DPRINT("%d :: pTemp_lcml->buffer->pBuffer = %p \n",__LINE__,pTemp_lcml->buffer->pBuffer);
        pTemp_lcml->eDir = OMX_DirOutput;
   
        WBAMRENC_OMX_MALLOC_SIZE(pTemp_lcml->pBufferParam,
                                 (sizeof(WBAMRENC_ParamStruct)+DSP_CACHE_ALIGNMENT),
                                 WBAMRENC_ParamStruct); 

        pTemp_char = (char*)pTemp_lcml->pBufferParam;
        pTemp_char += EXTRA_BYTES;
        pTemp_lcml->pBufferParam = (WBAMRENC_ParamStruct*)pTemp_char;
        

        pTemp_lcml->pBufferParam->usNbFrames=0;
        pTemp_lcml->pBufferParam->pParamElem=NULL;
        pTemp_lcml->pFrameParam=NULL;
        
        WBAMRENC_OMX_MALLOC(pTemp_lcml->pDmmBuf, DMM_BUFFER_OBJ);
   
        pTemp->nFlags = WBAMRENC_NORMAL_BUFFER;
        pTemp++;
        pTemp_lcml++;
    }

#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate->nLcml_nCntIp = 0;
    pComponentPrivate->nLcml_nCntOpReceived = 0;
#endif       


    pComponentPrivate->bInitParamsInitialized = 1;
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_FillLCMLInitParams\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
 * @WBAMRENC_StartComponentThread() This function is called by the component to create
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

OMX_ERRORTYPE WBAMRENC_StartComponentThread(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate =
        (WBAMRENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
#ifdef UNDER_CE     
    pthread_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.__inheritsched = PTHREAD_EXPLICIT_SCHED;
    attr.__schedparam.__sched_priority = OMX_AUDIO_ENCODER_THREAD_PRIORITY;
#endif

    WBAMRENC_DPRINT ("%d :: Entering  WBAMRENC_StartComponentThread\n", __LINE__);

    /* Initialize all the variables*/
    pComponentPrivate->bIsThreadstop = 0;
    pComponentPrivate->lcml_nOpBuf = 0;
    pComponentPrivate->lcml_nIpBuf = 0;
    pComponentPrivate->app_nBuf = 0;

    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->cmdDataPipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        WBAMRENC_DPRINT("%d :: Error while creating cmdDataPipe\n",__LINE__);
        goto EXIT;
    }
    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->dataPipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        WBAMRENC_DPRINT("%d :: Error while creating dataPipe\n",__LINE__);
        goto EXIT;
    }

    /* create the pipe used to send commands to the thread */
    eError = pipe (pComponentPrivate->cmdPipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        WBAMRENC_DPRINT("%d :: Error while creating cmdPipe\n",__LINE__);
        goto EXIT;
    }

    /* Create the Component Thread */
#ifdef UNDER_CE     
    eError = pthread_create (&(pComponentPrivate->ComponentThread), &attr, WBAMRENC_CompThread, pComponentPrivate);
#else   
    eError = pthread_create (&(pComponentPrivate->ComponentThread),NULL, WBAMRENC_CompThread, pComponentPrivate);
#endif
    if (eError || !pComponentPrivate->ComponentThread) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    pComponentPrivate->bCompThreadStarted = 1;
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_StartComponentThread\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
 * @WBAMRENC_FreeCompResources() This function is called by the component during
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

OMX_ERRORTYPE WBAMRENC_FreeCompResources(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;
    WBAMRENC_DPRINT("%d :: Entering WBAMRENC_FreeCompResources\n",__LINE__);

    if (pComponentPrivate->bCompThreadStarted) {
        OMX_WBCLOSE_PIPE(pComponentPrivate->dataPipe[0],err);
        OMX_WBCLOSE_PIPE(pComponentPrivate->dataPipe[1],err);
        OMX_WBCLOSE_PIPE(pComponentPrivate->cmdPipe[0],err);
        OMX_WBCLOSE_PIPE(pComponentPrivate->cmdPipe[1],err);
        OMX_WBCLOSE_PIPE(pComponentPrivate->cmdDataPipe[0],err);
        OMX_WBCLOSE_PIPE(pComponentPrivate->cmdDataPipe[1],err);
    }

    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pcmParams);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->amrParams);

    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT]->pPortFormat);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT]->pPortFormat);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pCompPort[WBAMRENC_INPUT_PORT]);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pCompPort[WBAMRENC_OUTPUT_PORT]);

    OMX_WBMEMFREE_STRUCT(pComponentPrivate->sPortParam);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->sPriorityMgmt);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pInputBufferList);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pOutputBufferList);

    
#ifndef UNDER_CE
    WBAMRENC_DPRINT("\n\n FreeCompResources: Destroying mutexes\n\n");
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
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_FreeCompResources()\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
 * @WBAMRENC_CleanupInitParams() This function is called by the component during
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

OMX_ERRORTYPE WBAMRENC_CleanupInitParams(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0;
    OMX_U32 nOpBuf = 0;
    OMX_U32 i = 0;
    WBAMRENC_LCML_BUFHEADERTYPE *pTemp_lcml;
    OMX_U8* pAlgParmTemp;
    OMX_U8* pBufParmsTemp;
    char *pTemp = NULL;

    LCML_DSP_INTERFACE *pLcmlHandle;
    LCML_DSP_INTERFACE *pLcmlHandleAux;
    
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;
    WBAMRENC_DPRINT("%d :: Entering WBAMRENC_CleanupInitParams()\n", __LINE__);
 
    if(pComponentPrivate->dasfMode == 1) {  
        OMX_WBMEMFREE_STRUCT(pComponentPrivate->strmAttr);
    }
    pAlgParmTemp = (OMX_U8*)pComponentPrivate->pAlgParam;
    if (pAlgParmTemp != NULL){
        pAlgParmTemp -= EXTRA_BYTES;
    }
    pComponentPrivate->pAlgParam = (WBAMRENC_TALGCtrl*)pAlgParmTemp;
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pAlgParam);

    if(pComponentPrivate->nMultiFrameMode == 1) {
        OMX_WBMEMFREE_STRUCT(pComponentPrivate->pHoldBuffer);
        OMX_WBMEMFREE_STRUCT(pComponentPrivate->iHoldBuffer);
        OMX_WBMEMFREE_STRUCT(pComponentPrivate->iMMFDataLastBuffer);
    }

    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[WBAMRENC_INPUT_PORT];
    nIpBuf = pComponentPrivate->nRuntimeInputBuffers;

    for(i=0; i<nIpBuf; i++) {       
        if(pTemp_lcml->pFrameParam!=NULL){
              
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;             
            pLcmlHandleAux = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);             
            OMX_DmmUnMap(pLcmlHandleAux->dspCodec->hProc,
                         (void*)pTemp_lcml->pBufferParam->pParamElem,
                         pTemp_lcml->pDmmBuf->pReserved);

            pBufParmsTemp = (OMX_U8*)pTemp_lcml->pFrameParam;
            pBufParmsTemp -= EXTRA_BYTES;
            OMX_WBMEMFREE_STRUCT(pBufParmsTemp);
            pBufParmsTemp = NULL;
            pTemp_lcml->pFrameParam = NULL;                               
        }        
 
        pTemp = (char*)pTemp_lcml->pBufferParam;
        if (pTemp != NULL) {        
            pTemp -= EXTRA_BYTES;
        }
        pTemp_lcml->pBufferParam = (WBAMRENC_ParamStruct*)pTemp;
        OMX_WBMEMFREE_STRUCT(pTemp_lcml->pBufferParam);
    

        if(pTemp_lcml->pDmmBuf!=NULL){
            OMX_WBMEMFREE_STRUCT(pTemp_lcml->pDmmBuf);
            pTemp_lcml->pDmmBuf = NULL;
        }
        pTemp_lcml++;
    }

    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[WBAMRENC_OUTPUT_PORT];
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
                               
            pBufParmsTemp = (OMX_U8*)pTemp_lcml->pFrameParam;
            pBufParmsTemp -= EXTRA_BYTES;
            OMX_WBMEMFREE_STRUCT(pBufParmsTemp);
            pBufParmsTemp = NULL;
            pTemp_lcml->pFrameParam = NULL;
                                             
        }
 
        pTemp = (char*)pTemp_lcml->pBufferParam;
        if (pTemp != NULL) {        
            pTemp -= EXTRA_BYTES;
        }
        pTemp_lcml->pBufferParam = (WBAMRENC_ParamStruct*)pTemp;
        OMX_WBMEMFREE_STRUCT(pTemp_lcml->pBufferParam);
    
        if(pTemp_lcml->pDmmBuf!=NULL){
            OMX_WBMEMFREE_STRUCT(pTemp_lcml->pDmmBuf);
            pTemp_lcml->pDmmBuf = NULL;
        }
        pTemp_lcml++;   
    }

    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[WBAMRENC_INPUT_PORT]);
    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[WBAMRENC_OUTPUT_PORT]);

    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_CleanupInitParams()\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
 * @WBAMRENC_StopComponentThread() This function is called by the component during
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

OMX_ERRORTYPE WBAMRENC_StopComponentThread(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE threadError = OMX_ErrorNone;
    int pthreadError = 0;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;
    WBAMRENC_DPRINT("%d :: Entering WBAMRENC_StopComponentThread\n",__LINE__);
    pComponentPrivate->bIsThreadstop = 1;
    WBAMRENC_DPRINT("%d :: About to call pthread_join\n",__LINE__);
    write (pComponentPrivate->cmdPipe[1], &pComponentPrivate->bIsThreadstop, sizeof(OMX_U16));    
    pthreadError = pthread_join (pComponentPrivate->ComponentThread,
                                 (void*)&threadError);
    if (0 != pthreadError) {
        eError = OMX_ErrorHardware;
        WBAMRENC_DPRINT("%d :: Error closing ComponentThread - pthreadError = %d\n",__LINE__,pthreadError);
        goto EXIT;
    }
    if (OMX_ErrorNone != threadError && OMX_ErrorNone != eError) {
        eError = OMX_ErrorInsufficientResources;
        WBAMRENC_EPRINT("%d :: Error while closing Component Thread\n",__LINE__);
        goto EXIT;
    }
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_StopComponentThread\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}


/* ========================================================================== */
/**
 * @WBAMRENC_HandleCommand() This function is called by the component when ever it
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

OMX_U32 WBAMRENC_HandleCommand (WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate,
                                OMX_COMMANDTYPE cmd,
                                OMX_U32 cmdData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMMANDTYPE command = cmd;
    OMX_STATETYPE commandedState;   
    OMX_HANDLETYPE pLcmlHandle;
#ifdef RESOURCE_MANAGER_ENABLED    
    OMX_ERRORTYPE rm_error;
#endif  
    LCML_CALLBACKTYPE cb;
    LCML_DSP *pLcmlDsp;
    OMX_U32 cmdValues[4];
    OMX_U32 pValues[4];
    OMX_U32 commandData = cmdData;
    OMX_U16 arr[100];
    char *pArgs = "damedesuStr";
    char *p = "damedesuStr";  
    OMX_U8* pParmsTemp;
    OMX_U8* pAlgParmTemp;
    OMX_U32 i = 0;
    OMX_U8 inputPortFlag=0,outputPortFlag=0;    
    WBAMRENC_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;
    
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    pLcmlHandle = pComponentPrivate->pLcmlHandle;

    WBAMRENC_DPRINT("%d :: Entering WBAMRENCHandleCommand Function \n",__LINE__);
    WBAMRENC_DPRINT("%d :: pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);
    
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
                                                     OMX_TI_ErrorMinor,
                                                     NULL);
            WBAMRENC_EPRINT("%d :: Comp: OMX_ErrorSameState Given by Comp\n",__LINE__);
        }else{
            switch(commandedState) {
            case OMX_StateIdle:
                WBAMRENC_DPRINT("%d :: WBAMRENC_HandleCommand :: OMX_StateIdle \n",__LINE__);
                WBAMRENC_DPRINT("%d :: pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);
                if (pComponentPrivate->curState == OMX_StateLoaded) {
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundarySetup);
#endif

                    if (pComponentPrivate->dasfMode == 1) {
                        pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bEnabled= FALSE;
                        pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated= FALSE;
                        if(pComponentPrivate->streamID == 0) { 
                            WBAMRENC_EPRINT("**************************************\n");
                            WBAMRENC_EPRINT(":: Error = OMX_ErrorInsufficientResources\n");
                            WBAMRENC_EPRINT("**************************************\n");
                            eError = OMX_ErrorInsufficientResources; 
                            pComponentPrivate->curState = OMX_StateInvalid; 
                            pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate, 
                                                                   OMX_EventError, OMX_ErrorInvalidState,OMX_TI_ErrorMajor, "No Stream ID Available");              
                            goto EXIT; 
                        }                                                       
                    }

                    if (pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated &&  
                        pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bEnabled)  {
                        inputPortFlag = 1;
                    }
                    if (pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated && 
                        pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled) {
                        outputPortFlag = 1;
                    }
                    if (!pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated &&  
                        !pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bEnabled)  {
                        inputPortFlag = 1;
                    }
                    if (!pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated && 
                        !pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled) {
                        outputPortFlag = 1;
                    }

                    if(!(inputPortFlag && outputPortFlag)){
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

                    cb.LCML_Callback = (void *) WBAMRENC_LCMLCallback;
                    pLcmlHandle = (OMX_HANDLETYPE) WBAMRENC_GetLCMLHandle(pComponentPrivate);
                    if (pLcmlHandle == NULL) {
                        WBAMRENC_EPRINT("%d :: LCML Handle is NULL........exiting..\n",__LINE__);
                        goto EXIT;
                    }

                    /* Got handle of dsp via phandle filling information about DSP Specific things */
                    pLcmlDsp = (((LCML_DSP_INTERFACE*)pLcmlHandle)->dspCodec);
                    eError = WBAMRENC_FillLCMLInitParams(pHandle, pLcmlDsp, arr);
                    if(eError != OMX_ErrorNone) {
                        WBAMRENC_EPRINT("%d :: Error from WBAMRENCFill_LCMLInitParams()\n",__LINE__);
                        goto EXIT;
                    }

                    pComponentPrivate->pLcmlHandle = (LCML_DSP_INTERFACE *)pLcmlHandle;
                    cb.LCML_Callback = (void *) WBAMRENC_LCMLCallback;

#ifndef UNDER_CE
                                        
                    WBAMRENC_DPRINT("%d :: Calling LCML_InitMMCodecEx...\n",__LINE__);

                    eError = LCML_InitMMCodecEx(((LCML_DSP_INTERFACE *)pLcmlHandle)->pCodecinterfacehandle,
                                                p,&pLcmlHandle,(void *)p,&cb, (OMX_STRING)pComponentPrivate->sDeviceString);
#else
                    eError = LCML_InitMMCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                              p,&pLcmlHandle, (void *)p, &cb);
#endif

                    if(eError != OMX_ErrorNone) {
                        WBAMRENC_EPRINT("%d :: Error returned from LCML_Init()\n",__LINE__);
                        goto EXIT;
                    }
#ifdef RESOURCE_MANAGER_ENABLED
                    /* Need check the resource with RM */

                    pComponentPrivate->rmproxyCallback.RMPROXY_Callback = (void *) WBAMRENC_ResourceManagerCallback;

                    if (pComponentPrivate->curState != OMX_StateWaitForResources){
                    
                        rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_RequestResource, OMX_WBAMR_Encoder_COMPONENT, WBAMRENC_CPU_LOAD, 3456, &(pComponentPrivate->rmproxyCallback));
                        if(rm_error == OMX_ErrorNone) {
                            /* resource is available */
#ifdef __PERF_INSTRUMENTATION__
                            PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySetup);
#endif
                            pComponentPrivate->curState = OMX_StateIdle;
                            pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                                    pHandle->pApplicationPrivate,
                                                                    OMX_EventCmdComplete,
                                                                    OMX_CommandStateSet,
                                                                    pComponentPrivate->curState,
                                                                    NULL);
    
                            rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_WBAMR_Encoder_COMPONENT, OMX_StateIdle, 3456, NULL);
                        }
                        else if(rm_error == OMX_ErrorInsufficientResources) {
                            /* resource is not available, need set state to OMX_StateWaitForResources */
                            pComponentPrivate->curState = OMX_StateWaitForResources;
                            pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                                    pHandle->pApplicationPrivate,
                                                                    OMX_EventCmdComplete,
                                                                    OMX_CommandStateSet,
                                                                    pComponentPrivate->curState,
                                                                    NULL);
                            WBAMRENC_EPRINT("%d :: Comp: OMX_ErrorInsufficientResources\n", __LINE__);
                        }
                    }else{

                        pComponentPrivate->curState = OMX_StateIdle;
                        pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                                pHandle->pApplicationPrivate,
                                                                OMX_EventCmdComplete,
                                                                OMX_CommandStateSet,
                                                                pComponentPrivate->curState,
                                                                NULL);
                        rm_error = RMProxy_NewSendCommand(pHandle,
                                                          RMProxy_StateSet,
                                                          OMX_WBAMR_Encoder_COMPONENT,
                                                          OMX_StateIdle,
                                                          3456,
                                                          NULL);

                    }
                    
#else
                    pComponentPrivate->curState = OMX_StateIdle;
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventCmdComplete,
                                                            OMX_CommandStateSet,
                                                            pComponentPrivate->curState,
                                                            NULL);
#endif

                                                           
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySetup);
#endif


                }
                else if (pComponentPrivate->curState == OMX_StateExecuting) {
                    WBAMRENC_DPRINT("%d :: Setting Component to OMX_StateIdle\n",__LINE__);
                    WBAMRENC_DPRINT("%d :: AMRENC: About to Call MMCodecControlStop\n", __LINE__);
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               MMCodecControlStop,(void *)pArgs);

                    pAlgParmTemp = (OMX_U8*)pComponentPrivate->pAlgParam;
                    if (pAlgParmTemp != NULL)
                        pAlgParmTemp -= EXTRA_BYTES;
                    pComponentPrivate->pAlgParam = (WBAMRENC_TALGCtrl*)pAlgParmTemp;
                    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pAlgParam);
                    if (pComponentPrivate->pHoldBuffer) {
                        OMX_WBMEMFREE_STRUCT(pComponentPrivate->pHoldBuffer);
                        pComponentPrivate->pHoldBuffer = NULL;
                    }                      
                    pComponentPrivate->nOutStandingFillDones = 0;
                    pComponentPrivate->nOutStandingEmptyDones = 0;                                                
                    pComponentPrivate->nHoldLength = 0;
                    pComponentPrivate->InBuf_Eos_alreadysent = 0;
                    pParmsTemp = (OMX_U8*)pComponentPrivate->pParams;
                    if (pParmsTemp != NULL){
                        pParmsTemp -= EXTRA_BYTES;
                    }
                    pComponentPrivate->pParams = (WBAMRENC_AudioCodecParams*)pParmsTemp;
                    OMX_WBMEMFREE_STRUCT(pComponentPrivate->pParams);

                    if(eError != OMX_ErrorNone) {
                        WBAMRENC_EPRINT("%d :: Error from LCML_ControlCodec MMCodecControlStop..\n",__LINE__);
                        goto EXIT;
                    }                   
                    
                    if(eError != OMX_ErrorNone) {
                        WBAMRENC_EPRINT(": Error Occurred in Codec Stop..\n");
                        pComponentPrivate->curState = OMX_StateInvalid;
                        pComponentPrivate->cbInfo.EventHandler( pHandle, 
                                                                pHandle->pApplicationPrivate,
                                                                OMX_EventError, 
                                                                eError,
                                                                OMX_TI_ErrorSevere, 
                                                                NULL);
                        goto EXIT;
                    }

                }
                else if(pComponentPrivate->curState == OMX_StatePause) {

                    pComponentPrivate->curState = OMX_StateIdle;
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif
#ifdef RESOURCE_MANAGER_ENABLED
                    rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_WBAMR_Encoder_COMPONENT, OMX_StateIdle, 3456, NULL);
#endif              
                    WBAMRENC_DPRINT ("%d :: The component is stopped\n",__LINE__);
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
                                                            OMX_TI_ErrorMinor,
                                                            "Invalid State");
                    WBAMRENC_EPRINT("%d :: Comp: OMX_ErrorIncorrectStateTransition\n",__LINE__);
                }
                break;

            case OMX_StateExecuting:
                WBAMRENC_DPRINT("%d :: WBAMRENC_HandleCommand :: OMX_StateExecuting \n",__LINE__);
                if (pComponentPrivate->curState == OMX_StateIdle) {
                    /* Sending commands to DSP via LCML_ControlCodec third argument
                       is not used for time being */
                    if( pComponentPrivate->pAlgParam == NULL){
                        WBAMRENC_OMX_MALLOC_SIZE(pAlgParmTemp, sizeof(WBAMRENC_TALGCtrl) + DSP_CACHE_ALIGNMENT,OMX_U8);
                        pComponentPrivate->pAlgParam = (WBAMRENC_TALGCtrl*)(pAlgParmTemp + EXTRA_BYTES);
                        WBAMRENC_MEMPRINT("%d :: [ALLOC] %p\n",__LINE__,pComponentPrivate->pAlgParam);
                    
                    }
                    pComponentPrivate->nNumInputBufPending = 0;
                    pComponentPrivate->nNumOutputBufPending = 0;
                    
                    pComponentPrivate->nNumOfFramesSent=0;

                    pComponentPrivate->nEmptyBufferDoneCount = 0;
                    pComponentPrivate->nEmptyThisBufferCount =0;
                
                    pComponentPrivate->pAlgParam->iBitrate = pComponentPrivate->amrParams->eAMRBandMode;
                    pComponentPrivate->pAlgParam->iDTX = pComponentPrivate->amrParams->eAMRDTXMode;
                    pComponentPrivate->pAlgParam->iSize = sizeof (WBAMRENC_TALGCtrl);

                    WBAMRENC_DPRINT("%d :: pAlgParam->iBitrate = %d\n",__LINE__,pComponentPrivate->pAlgParam->iBitrate);
                    WBAMRENC_DPRINT("%d :: pAlgParam->iDTX  = %d\n",__LINE__,pComponentPrivate->pAlgParam->iDTX);

                    cmdValues[0] = ALGCMD_BITRATE;                  /*setting the bit-rate*/ 
                    cmdValues[1] = (OMX_U32)pComponentPrivate->pAlgParam;
                    cmdValues[2] = sizeof (WBAMRENC_TALGCtrl);   
                    p = (void *)&cmdValues;
                    WBAMRENC_DPRINT("%d :: EMMCodecControlAlgCtrl-1 Sending...\n",__LINE__);
                    /* Sending ALGCTRL MESSAGE DTX to DSP via LCML_ControlCodec*/
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlAlgCtrl, (void *)p);
                    if (eError != OMX_ErrorNone) {
                        WBAMRENC_EPRINT("%d :: Error from LCML_ControlCodec EMMCodecControlAlgCtrl-1 failed = %x\n",__LINE__,eError);
                        goto EXIT;
                    }
                
                    cmdValues[0] = ALGCMD_DTX;                  /*setting DTX mode*/
                    cmdValues[1] = (OMX_U32)pComponentPrivate->pAlgParam;
                    cmdValues[2] = sizeof (WBAMRENC_TALGCtrl);
                    p = (void *)&cmdValues;
                    WBAMRENC_DPRINT("%d :: EMMCodecControlAlgCtrl-2 Sending...\n",__LINE__);
                    /* Sending ALGCTRL MESSAGE BITRATE to DSP via LCML_ControlCodec*/
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlAlgCtrl, (void *)p);
                    if (eError != OMX_ErrorNone) {
                        WBAMRENC_EPRINT("%d :: Error from LCML_ControlCodec EMMCodecControlAlgCtrl-2 failed = %x\n",__LINE__,eError);
                        goto EXIT;
                    }
                    if(pComponentPrivate->dasfMode == 1) {
                        WBAMRENC_DPRINT("%d :: ---- Comp: DASF Functionality is ON ---\n",__LINE__);

                        WBAMRENC_OMX_MALLOC_SIZE(pParmsTemp, sizeof(WBAMRENC_AudioCodecParams) + DSP_CACHE_ALIGNMENT,OMX_U8);

                        pComponentPrivate->pParams = (WBAMRENC_AudioCodecParams*)(pParmsTemp + EXTRA_BYTES);
                        WBAMRENC_MEMPRINT("%d :: [ALLOC] %p\n",__LINE__,pComponentPrivate->pParams);
                    
                        pComponentPrivate->pParams->iAudioFormat = 1;
                        pComponentPrivate->pParams->iStrmId = pComponentPrivate->streamID;
                        pComponentPrivate->pParams->iSamplingRate = WBAMRENC_SAMPLING_FREQUENCY;
                        pValues[0] = USN_STRMCMD_SETCODECPARAMS;
                        pValues[1] = (OMX_U32)pComponentPrivate->pParams; 
                        pValues[2] = sizeof(WBAMRENC_AudioCodecParams);
                        /* Sending STRMCTRL MESSAGE to DSP via LCML_ControlCodec*/
                        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                   EMMCodecControlStrmCtrl,(void *)pValues);
                        if(eError != OMX_ErrorNone) {
                            WBAMRENC_EPRINT("%d :: Error from LCML_ControlCodec EMMCodecControlStrmCtrl = %x\n",__LINE__,eError);
                            goto EXIT;
                        }
                    }
                    /* Sending START MESSAGE to DSP via LCML_ControlCodec*/
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStart, (void *)p);
                    if(eError != OMX_ErrorNone) {
                        WBAMRENC_EPRINT("%d :: Error from LCML_ControlCodec EMMCodecControlStart = %x\n",__LINE__,eError);
                        goto EXIT;
                    }

                } else if (pComponentPrivate->curState == OMX_StatePause) {
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStart, (void *)p);
                    if (eError != OMX_ErrorNone) {
                        WBAMRENC_EPRINT("%d :: Error While Resuming the codec = %x\n",__LINE__,eError);
                        goto EXIT;
                    }

                    for (i=0; i < pComponentPrivate->nNumInputBufPending; i++) {
                        if (pComponentPrivate->pInputBufHdrPending[i]) {
                            WBAMRENC_GetCorrespondingLCMLHeader(pComponentPrivate, pComponentPrivate->pInputBufHdrPending[i]->pBuffer, OMX_DirInput, &pLcmlHdr);
                            WBAMRENC_SetPending(pComponentPrivate,pComponentPrivate->pInputBufHdrPending[i],OMX_DirInput,__LINE__);

                            eError = LCML_QueueBuffer(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                      EMMCodecInputBuffer,
                                                      pComponentPrivate->pInputBufHdrPending[i]->pBuffer,
                                                      pComponentPrivate->pInputBufHdrPending[i]->nAllocLen,
                                                      pComponentPrivate->pInputBufHdrPending[i]->nFilledLen,
                                                      (OMX_U8 *) pLcmlHdr->pBufferParam,
                                                      sizeof(WBAMRENC_ParamStruct),
                                                      NULL);
                        }
                    }
                    pComponentPrivate->nNumInputBufPending = 0;

                    for (i=0; i < pComponentPrivate->nNumOutputBufPending; i++) {
                        if (pComponentPrivate->pOutputBufHdrPending[i]) {
                            WBAMRENC_GetCorrespondingLCMLHeader(pComponentPrivate,pComponentPrivate->pOutputBufHdrPending[i]->pBuffer, OMX_DirOutput, &pLcmlHdr);
                            WBAMRENC_SetPending(pComponentPrivate,pComponentPrivate->pOutputBufHdrPending[i],OMX_DirOutput,__LINE__);
                            eError = LCML_QueueBuffer(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                      EMMCodecOuputBuffer,
                                                      pComponentPrivate->pOutputBufHdrPending[i]->pBuffer,
                                                      pComponentPrivate->pOutputBufHdrPending[i]->nAllocLen,
                                                      pComponentPrivate->pOutputBufHdrPending[i]->nFilledLen,
                                                      (OMX_U8 *) pLcmlHdr->pBufferParam,
                                                      sizeof(WBAMRENC_ParamStruct),
                                                      NULL);
                        }
                    }
                    pComponentPrivate->nNumOutputBufPending = 0;
                } else {
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventError,
                                                            OMX_ErrorIncorrectStateTransition,
                                                            OMX_TI_ErrorMinor,
                                                            "Incorrect State Transition");
                    WBAMRENC_EPRINT("%d :: Comp: OMX_ErrorIncorrectStateTransition Given by Comp\n",__LINE__);
                    goto EXIT;

                }
#ifdef RESOURCE_MANAGER_ENABLED            
                rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_WBAMR_Encoder_COMPONENT, OMX_StateExecuting, 3456, NULL);
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
                WBAMRENC_DPRINT("%d :: Comp: OMX_CommandStateSet Given by Comp\n",__LINE__);
                break;

            case OMX_StateLoaded:
                WBAMRENC_DPRINT("%d :: WBAMRENC_HandleCommand :: OMX_StateLoaded\n",__LINE__);
                if (pComponentPrivate->curState == OMX_StateWaitForResources){
                    WBAMRENC_DPRINT("%d :: WBAMRENC_HandleCommand :: OMX_StateWaitForResources\n",__LINE__);
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundaryCleanup); 
#endif              
                    pComponentPrivate->curState = OMX_StateLoaded;
#ifdef __PERF_INSTRUMENTATION__
                    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundaryCleanup);
#endif
                    pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                             pHandle->pApplicationPrivate,
                                                             OMX_EventCmdComplete,
                                                             OMX_CommandStateSet,
                                                             pComponentPrivate->curState,
                                                             NULL);
                    WBAMRENC_DPRINT("%d :: Comp: OMX_CommandStateSet Given by Comp\n",__LINE__);
                    break;
                }
                if (pComponentPrivate->curState != OMX_StateIdle) {
                    WBAMRENC_DPRINT("%d :: WBAMRENC_HandleCommand :: OMX_StateIdle\n",__LINE__);
                    pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                             pHandle->pApplicationPrivate,
                                                             OMX_EventError,
                                                             OMX_ErrorIncorrectStateTransition,
                                                             OMX_TI_ErrorMinor,
                                                             "Incorrect State Transition");
                    WBAMRENC_EPRINT("%d :: Error: OMX_ErrorIncorrectStateTransition Given by Comp\n",__LINE__);
                    goto EXIT;
                }
#ifdef __PERF_INSTRUMENTATION__
                PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundaryCleanup);
#endif
                WBAMRENC_DPRINT("%d :: WBAMRENC_HandleCommand - evaluating if all buffers are free\n",__LINE__);
                
                if (pComponentPrivate->pInputBufferList->numBuffers ||
                    pComponentPrivate->pOutputBufferList->numBuffers) {
                    pComponentPrivate->InIdle_goingtoloaded = 1;
#ifndef UNDER_CE
                    pthread_mutex_lock(&pComponentPrivate->InIdle_mutex); 
                    pthread_cond_wait(&pComponentPrivate->InIdle_threshold, 
                                      &pComponentPrivate->InIdle_mutex);
                    pthread_mutex_unlock(&pComponentPrivate->InIdle_mutex);
#else
                    OMX_WaitForEvent(&(pComponentPrivate->InIdle_event));
#endif
                    pComponentPrivate->bLoadedCommandPending = OMX_FALSE;
                }
                /* Now Deinitialize the component No error should be returned from
                 * this function. It should clean the system as much as possible */
                WBAMRENC_CleanupInitParams(pHandle);
                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlDestroy, (void *)p);
                if (eError != OMX_ErrorNone) {
                    WBAMRENC_EPRINT("%d :: Error: LCML_ControlCodec EMMCodecControlDestroy = %x\n",__LINE__, eError);
                    goto EXIT;
                }
                /*Closing LCML Lib*/
                if (pComponentPrivate->ptrLibLCML != NULL){
                    WBAMRENC_DPRINT("%d OMX_AmrDecoder.c Closing LCML library\n",__LINE__);
                    dlclose( pComponentPrivate->ptrLibLCML);
                    pComponentPrivate->ptrLibLCML = NULL;
                }

#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingCommand(pComponentPrivate->pPERF, -1, 0, PERF_ModuleComponent);
#endif            
                eError = WBAMRENC_EXIT_COMPONENT_THRD;
                pComponentPrivate->bInitParamsInitialized = 0;
                pComponentPrivate->bLoadedCommandPending = OMX_FALSE;            
                break;

            case OMX_StatePause:
                WBAMRENC_DPRINT("%d :: WBAMRENC_HandleCommand :: OMX_StatePause\n",__LINE__);
                if (pComponentPrivate->curState != OMX_StateExecuting &&
                    pComponentPrivate->curState != OMX_StateIdle) {
                    pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                             pHandle->pApplicationPrivate,
                                                             OMX_EventError,
                                                             OMX_ErrorIncorrectStateTransition,
                                                             OMX_TI_ErrorMinor,
                                                             "Incorrect State Transition");
                    WBAMRENC_EPRINT("%d :: Error: OMX_ErrorIncorrectStateTransition Given by Comp\n",__LINE__);
                    goto EXIT;
                }
#ifdef __PERF_INSTRUMENTATION__
                PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif
                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlPause, (void *)p);
                if (eError != OMX_ErrorNone) {
                    WBAMRENC_DPRINT("%d :: Error: LCML_ControlCodec EMMCodecControlPause = %x\n",__LINE__,eError);
                    goto EXIT;
                }
                WBAMRENC_DPRINT("%d :: Comp: OMX_CommandStateSet Given by Comp\n",__LINE__);
                break;

            case OMX_StateWaitForResources:
                if (pComponentPrivate->curState == OMX_StateLoaded) {

#ifdef RESOURCE_MANAGER_ENABLED         
                    rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_WBAMR_Encoder_COMPONENT, OMX_StateWaitForResources, 3456, NULL);
#endif 
                    
                    pComponentPrivate->curState = OMX_StateWaitForResources;
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventCmdComplete,
                                                            OMX_CommandStateSet,
                                                            pComponentPrivate->curState,
                                                            NULL);
                    WBAMRENC_DPRINT("%d :: Comp: OMX_CommandStateSet Given by Comp\n",__LINE__);
                } else {
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventError,
                                                            OMX_ErrorIncorrectStateTransition,
                                                            OMX_TI_ErrorMinor,
                                                            "Incorrect State Transition");
                    WBAMRENC_EPRINT("%d :: Error: OMX_ErrorIncorrectStateTransition Given by Comp\n",__LINE__);
                }
                break;

            case OMX_StateInvalid:
                WBAMRENC_DPRINT("%d: HandleCommand: Cmd OMX_StateInvalid:\n",__LINE__);
                if (pComponentPrivate->curState != OMX_StateWaitForResources && 
                    pComponentPrivate->curState != OMX_StateInvalid && 
                    pComponentPrivate->curState != OMX_StateLoaded) {

                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlDestroy, (void *)pArgs);
                }
                pComponentPrivate->curState = OMX_StateInvalid;
                pComponentPrivate->cbInfo.EventHandler( pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventError, 
                                                        OMX_ErrorInvalidState,
                                                        OMX_TI_ErrorSevere, 
                                                        "Incorrect State Transition");

                WBAMRENC_EPRINT("%d :: Comp: OMX_ErrorInvalidState Given by Comp\n",__LINE__);
                WBAMRENC_CleanupInitParams(pHandle);
                break;

            case OMX_StateMax:
                WBAMRENC_DPRINT("%d :: WBAMRENC_HandleCommand :: Cmd OMX_StateMax\n",__LINE__);
                break;
            } /* End of Switch */
        }
    } else if (command == OMX_CommandMarkBuffer) {
        WBAMRENC_DPRINT("%d :: command OMX_CommandMarkBuffer received\n",__LINE__);
        if(!pComponentPrivate->pMarkBuf){
            /* TODO Need to handle multiple marks */
            pComponentPrivate->pMarkBuf = (OMX_MARKTYPE *)(commandData);
        }
    } else if (command == OMX_CommandPortDisable) {
        if (!pComponentPrivate->bDisableCommandPending) {
            if(commandData == 0x0 || commandData == -1){
                pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bEnabled = OMX_FALSE;                
            }
            if(commandData == 0x1 || commandData == -1){
                pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled = OMX_FALSE;
                if (pComponentPrivate->curState == OMX_StateExecuting) {
                    pComponentPrivate->bNoIdleOnStop = OMX_TRUE;
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               MMCodecControlStop,(void *)pArgs);
                }
            }
        }
        WBAMRENC_DPRINT("commandData = %ld\n",commandData);
        WBAMRENC_DPRINT("pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated = %d\n",pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated);
        WBAMRENC_DPRINT("pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated = %d\n",pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated);
    
        if(commandData == 0x0) {
            if(!pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated){
                /* return cmdcomplete event if input unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortDisable,WBAMRENC_INPUT_PORT, NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else{
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }
        
        if(commandData == 0x1) {
            if (!pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated){
                /* return cmdcomplete event if output unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortDisable,WBAMRENC_OUTPUT_PORT, NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

        if(commandData == -1) {
            if (!pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated && 
                !pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated){

                /* return cmdcomplete event if inout & output unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortDisable,WBAMRENC_INPUT_PORT, NULL);

                pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortDisable,WBAMRENC_OUTPUT_PORT, NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

    } 
    else if (command == OMX_CommandPortEnable) {
        if(!pComponentPrivate->bEnableCommandPending) {
            if(commandData == 0x0 || commandData == -1){
                /* enable in port */
                WBAMRENC_DPRINT("setting input port to enabled\n");
                pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bEnabled = OMX_TRUE;

                if(pComponentPrivate->AlloBuf_waitingsignal)
                    {
                        pComponentPrivate->AlloBuf_waitingsignal = 0;
                    }
            }
            if(commandData == 0x1 || commandData == -1){
                /* enable out port */
                if(pComponentPrivate->AlloBuf_waitingsignal)
                    {
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
                    pComponentPrivate->bDspStoppedWhileExecuting = OMX_FALSE;
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStart,(void *)pArgs);
                }
                WBAMRENC_DPRINT("%d :: setting output port to enabled\n",__LINE__);
                pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled = OMX_TRUE;
                WBAMRENC_DPRINT("pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled = %d\n",pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bEnabled);
            }
        }
        
        if(commandData == 0x0){
            if (pComponentPrivate->curState == OMX_StateLoaded || 
                pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated) {
                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortEnable,
                                                       WBAMRENC_INPUT_PORT,
                                                       NULL);
                pComponentPrivate->bEnableCommandPending = 0;
            }
            else {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->bEnableCommandParam = commandData;
            }
        }
        else if(commandData == 0x1) {
            if (pComponentPrivate->curState == OMX_StateLoaded || 
                pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated){
                pComponentPrivate->cbInfo.EventHandler( pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete,
                                                        OMX_CommandPortEnable,
                                                        WBAMRENC_OUTPUT_PORT, 
                                                        NULL);
                pComponentPrivate->bEnableCommandPending = 0;
            }
            else {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->bEnableCommandParam = commandData;
            }
        }
        else if(commandData == -1) {
            if (pComponentPrivate->curState == OMX_StateLoaded || 
                (pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->bPopulated
                 && pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->bPopulated)){
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortEnable,
                                                       WBAMRENC_INPUT_PORT, 
                                                       NULL);
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortEnable,
                                                       WBAMRENC_OUTPUT_PORT, 
                                                       NULL);
                pComponentPrivate->bEnableCommandPending = 0;
                WBAMRENC_FillLCMLInitParamsEx(pHandle);
            }
            else {
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
        if(commandData == 0x0 || commandData == -1){
            WBAMRENC_DPRINT("Flushing input port %d\n",__LINE__);
            for (i=0; i < WBAMRENC_MAX_NUM_OF_BUFS; i++) {
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

                pComponentPrivate->cbInfo.EmptyBufferDone (pComponentPrivate->pHandle,
                                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                                           pComponentPrivate->pInputBufferList->pBufHdr[i]
                                                           );
                pComponentPrivate->nEmptyBufferDoneCount++;
                pComponentPrivate->nOutStandingEmptyDones--;
            }
            pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, OMX_CommandFlush,WBAMRENC_INPUT_PORT, NULL);   
        }
  
        if(commandData == 0x1 || commandData == -1){       
            WBAMRENC_DPRINT("Flushing output port %d\n",__LINE__);
            for (i=0; i < WBAMRENC_MAX_NUM_OF_BUFS; i++) {
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
                pComponentPrivate->cbInfo.FillBufferDone (pComponentPrivate->pHandle,
                                                          pComponentPrivate->pHandle->pApplicationPrivate,
                                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]);
                pComponentPrivate->nFillBufferDoneCount++;   
                pComponentPrivate->nOutStandingFillDones--;             
            }
            pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, OMX_CommandFlush,WBAMRENC_OUTPUT_PORT, NULL);
        }
    }

 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_HandleCommand Function\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
 * @WBAMRENC_HandleDataBufFromApp() This function is called by the component when ever it
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
OMX_ERRORTYPE WBAMRENC_HandleDataBufFromApp(OMX_BUFFERHEADERTYPE* pBufHeader,
                                            WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_DIRTYPE eDir;
    WBAMRENC_LCML_BUFHEADERTYPE *pLcmlHdr;
    LCML_DSP_INTERFACE *pLcmlHandle = (LCML_DSP_INTERFACE *)
        pComponentPrivate->pLcmlHandle;
    OMX_U32 frameLength, remainingBytes;
    OMX_U8* pExtraData;
    OMX_U8 nFrames=0,i; 
    LCML_DSP_INTERFACE * phandle;
    OMX_U8* pBufParmsTemp;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDefIn = NULL;    

    WBAMRENC_DPRINT ("%d :: Entering WBAMRENC_HandleDataBufFromApp Function\n",__LINE__);
    /*Find the direction of the received buffer from buffer list */
    eError = WBAMRENC_GetBufferDirection(pBufHeader, &eDir);
    if (eError != OMX_ErrorNone) {
        WBAMRENC_EPRINT ("%d :: The pBufHeader is not found in the list\n", __LINE__);
        goto EXIT;
    }

    if (eDir == OMX_DirInput) {
        pComponentPrivate->nEmptyThisBufferCount++;
        pPortDefIn = pComponentPrivate->pPortDef[OMX_DirInput];
        if (pBufHeader->nFilledLen > 0) {
            if (pComponentPrivate->nHoldLength == 0) {
                frameLength = WBAMRENC_INPUT_FRAME_SIZE;
                nFrames = (OMX_U8)(pBufHeader->nFilledLen / frameLength);

                if ( nFrames>=1 ) {/* At least there is 1 frame in the buffer */

                    pComponentPrivate->nHoldLength = pBufHeader->nFilledLen - frameLength*nFrames;
                    if (pComponentPrivate->nHoldLength > 0) {/* something need to be hold in pHoldBuffer */
                        if (pComponentPrivate->pHoldBuffer == NULL) {
                            WBAMRENC_OMX_MALLOC_SIZE(pComponentPrivate->pHoldBuffer, WBAMRENC_INPUT_BUFFER_SIZE,OMX_U8);
                        }
                        memset(pComponentPrivate->pHoldBuffer, 0, WBAMRENC_INPUT_BUFFER_SIZE);
                        /* Copy the extra data into pHoldBuffer. Size will be nHoldLength. */
                        pExtraData = pBufHeader->pBuffer + frameLength*nFrames;

                        if(pComponentPrivate->nHoldLength <= WBAMRENC_INPUT_BUFFER_SIZE) {
                            memcpy(pComponentPrivate->pHoldBuffer, pExtraData,  pComponentPrivate->nHoldLength);
                        }
                        else {
                            WBAMRENC_EPRINT ("%d :: Error: pHoldLenght is bigger than the input frame size\n", __LINE__);
                            goto EXIT;
                        }
                        pBufHeader->nFilledLen-=pComponentPrivate->nHoldLength;
                    }
                } 
                else {
                    if ( !pComponentPrivate->InBuf_Eos_alreadysent ){
                        /* received buffer with less than 1 AMR frame length. Save the data in pHoldBuffer.*/
                        pComponentPrivate->nHoldLength = pBufHeader->nFilledLen;
                        /* save the data into pHoldBuffer */
                        if (pComponentPrivate->pHoldBuffer == NULL) {
                            WBAMRENC_OMX_MALLOC_SIZE(pComponentPrivate->pHoldBuffer, WBAMRENC_INPUT_BUFFER_SIZE,OMX_U8);
                        }
                        /* Not enough data to be sent. Copy all received data into pHoldBuffer.*/
                        /* Size to be copied will be iHoldLen == mmData->BufferSize() */
                        memset(pComponentPrivate->pHoldBuffer, 0, WBAMRENC_INPUT_BUFFER_SIZE);
                        if(pComponentPrivate->nHoldLength <= WBAMRENC_INPUT_BUFFER_SIZE) {
                            memcpy(pComponentPrivate->pHoldBuffer, pBufHeader->pBuffer, pComponentPrivate->nHoldLength);
                            /* since not enough data, we shouldn't send anything to SN, but instead request to EmptyBufferDone again.*/
                        }
                        else {
                            WBAMRENC_EPRINT ("%d :: Error: pHoldLenght is bigger than the input frame size\n", __LINE__);
                            goto EXIT;
                        }
                    }
                    if (pComponentPrivate->curState != OMX_StatePause) {
                        WBAMRENC_DPRINT("%d :: Calling EmptyBufferDone\n",__LINE__);
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

                    pComponentPrivate->ProcessingInputBuf--;
                    goto EXIT;
                }
            } else {
                if((pComponentPrivate->nHoldLength+pBufHeader->nFilledLen) > pBufHeader->nAllocLen){
                    /*means that a second Acumulator must be used to insert holdbuffer to pbuffer and save remaining bytes
                      into hold buffer*/
                    remainingBytes = pComponentPrivate->nHoldLength+pBufHeader->nFilledLen-pBufHeader->nAllocLen;
                    if (pComponentPrivate->pHoldBuffer2 == NULL) {
                        WBAMRENC_OMX_MALLOC_SIZE(pComponentPrivate->pHoldBuffer2, WBAMRENC_INPUT_BUFFER_SIZE,OMX_U8);


                    }
                    pExtraData = (pBufHeader->pBuffer)+(pBufHeader->nFilledLen-remainingBytes);
                    memcpy(pComponentPrivate->pHoldBuffer2,pExtraData,remainingBytes);
                    pBufHeader->nFilledLen-=remainingBytes;
                    memmove(pBufHeader->pBuffer+ pComponentPrivate->nHoldLength,pBufHeader->pBuffer,pBufHeader->nFilledLen);
                    memcpy(pBufHeader->pBuffer,pComponentPrivate->pHoldBuffer,pComponentPrivate->nHoldLength);
                    pBufHeader->nFilledLen+=pComponentPrivate->nHoldLength;
                    memcpy(pComponentPrivate->pHoldBuffer, pComponentPrivate->pHoldBuffer2, remainingBytes);
                    pComponentPrivate->nHoldLength=remainingBytes;
                    remainingBytes=0;
                }
                else{
                    memmove(pBufHeader->pBuffer+pComponentPrivate->nHoldLength, pBufHeader->pBuffer, pBufHeader->nFilledLen);
                    memcpy(pBufHeader->pBuffer,pComponentPrivate->pHoldBuffer, pComponentPrivate->nHoldLength);
                    pBufHeader->nFilledLen+=pComponentPrivate->nHoldLength;
                    pComponentPrivate->nHoldLength=0;
                }
                frameLength = WBAMRENC_INPUT_BUFFER_SIZE;
                nFrames = (OMX_U8)(pBufHeader->nFilledLen / frameLength);
                pComponentPrivate->nHoldLength = pBufHeader->nFilledLen - frameLength*nFrames;
                pExtraData = pBufHeader->pBuffer + pBufHeader->nFilledLen-pComponentPrivate->nHoldLength;
                memcpy(pComponentPrivate->pHoldBuffer, pExtraData,  pComponentPrivate->nHoldLength);
                pBufHeader->nFilledLen-=pComponentPrivate->nHoldLength;

                if(nFrames < 1 ){
                    if (pComponentPrivate->curState != OMX_StatePause) {
                        WBAMRENC_DPRINT("line %d:: Calling EmptyBufferDone\n",__LINE__);
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
                        goto EXIT;
                    }
                    else {
                        pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pBufHeader;
                    }
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
                    pComponentPrivate->ProcessingInputBuf--;
                }
            }
            else{ 
                frameLength = WBAMRENC_INPUT_FRAME_SIZE;
                nFrames=1;
            }
        }

        if(nFrames >= 1){

            eError = WBAMRENC_GetCorrespondingLCMLHeader(pComponentPrivate,pBufHeader->pBuffer, OMX_DirInput, &pLcmlHdr);
            if (eError != OMX_ErrorNone) {
                WBAMRENC_DPRINT("%d :: Error: Invalid Buffer Came ...\n",__LINE__);
                goto EXIT;
            }

#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                              PREF(pBufHeader,pBuffer),
                              pPortDefIn->nBufferSize, 
                              PERF_ModuleCommonLayer);
#endif
            /*---------------------------------------------------------------*/
 
            pComponentPrivate->nNumOfFramesSent = nFrames;
            phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);
           
            if( (pLcmlHdr->pBufferParam->usNbFrames < nFrames) && (pLcmlHdr->pFrameParam!=NULL) ){
                pBufParmsTemp = (OMX_U8*)pLcmlHdr->pFrameParam; /*This means that more memory need to be used*/
                pBufParmsTemp -=EXTRA_BYTES;
                OMX_WBMEMFREE_STRUCT(pBufParmsTemp);  
                pLcmlHdr->pFrameParam = NULL;
                pBufParmsTemp =NULL;

                OMX_DmmUnMap(phandle->dspCodec->hProc, /*Unmap DSP memory used*/
                             (void*)pLcmlHdr->pBufferParam->pParamElem,
                             pLcmlHdr->pDmmBuf->pReserved);
                pLcmlHdr->pBufferParam->pParamElem = NULL;                   
            }

            if(pLcmlHdr->pFrameParam==NULL ){
                WBAMRENC_OMX_MALLOC_SIZE(pBufParmsTemp, (sizeof(WBAMRENC_FrameStruct)*(nFrames)) + DSP_CACHE_ALIGNMENT,OMX_U8);
                pLcmlHdr->pFrameParam =  (WBAMRENC_FrameStruct*)(pBufParmsTemp + EXTRA_BYTES);

                eError = OMX_DmmMap(phandle->dspCodec->hProc, 
                                    (nFrames*sizeof(WBAMRENC_FrameStruct)),
                                    (void*)pLcmlHdr->pFrameParam, 
                                    (pLcmlHdr->pDmmBuf));
                if (eError != OMX_ErrorNone){
                    WBAMRENC_EPRINT("OMX_DmmMap ERRROR!!!!\n\n");
                    goto EXIT;
                }                         
                pLcmlHdr->pBufferParam->pParamElem = (WBAMRENC_FrameStruct *) pLcmlHdr->pDmmBuf->pMapped;/*DSP Address*/
            }
           
            for(i=0;i<nFrames;i++){
                (pLcmlHdr->pFrameParam+i)->usLastFrame = 0;
            }
            if(pBufHeader->nFlags == OMX_BUFFERFLAG_EOS) {
                pComponentPrivate->InBuf_Eos_alreadysent = 1; /*TRUE*/
                pBufHeader->nFlags = 0;
            }   
            pLcmlHdr->pBufferParam->usNbFrames = nFrames;
            /* Store time stamp information */
            pComponentPrivate->arrBufIndex[pComponentPrivate->IpBufindex] = pBufHeader->nTimeStamp;
            /* Store nTickCount information */
            pComponentPrivate->arrTickCount[pComponentPrivate->IpBufindex] = pBufHeader->nTickCount;
            pComponentPrivate->IpBufindex++;
            pComponentPrivate->IpBufindex %= pComponentPrivate->pPortDef[OMX_DirOutput]->nBufferCountActual;            

            if (pComponentPrivate->curState == OMX_StateExecuting) {
                if(!pComponentPrivate->bDspStoppedWhileExecuting) {
                    if (!WBAMRENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirInput)) {
                        WBAMRENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirInput,__LINE__);
                        eError = LCML_QueueBuffer( pLcmlHandle->pCodecinterfacehandle,
                                                   EMMCodecInputBuffer,
                                                   (OMX_U8 *)pBufHeader->pBuffer,
                                                   frameLength*nFrames,
                                                   pBufHeader->nFilledLen,
                                                   (OMX_U8 *) pLcmlHdr->pBufferParam,
                                                   sizeof(WBAMRENC_ParamStruct),
                                                   NULL);
                        if (eError != OMX_ErrorNone) {
                            eError = OMX_ErrorHardware;
                            goto EXIT;
                        }
                        pComponentPrivate->lcml_nCntIp++;
                        pComponentPrivate->lcml_nIpBuf++;
                    }
                }
            }
            else if (pComponentPrivate->curState == OMX_StatePause){
                pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pBufHeader;
            }
            pComponentPrivate->ProcessingInputBuf--;
        }
        
        if(pBufHeader->pMarkData){
            if(pComponentPrivate->pOutputBufferList->pBufHdr[0]!=NULL) {
                /* copy mark to output buffer header */
                WBAMRENC_DPRINT("pComponentPrivate->pOutputBufferList->pBufHdr[0]!=NULL %d\n",__LINE__);
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->pMarkData = pBufHeader->pMarkData;
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->hMarkTargetComponent = pBufHeader->hMarkTargetComponent;
            }
            else{
                WBAMRENC_DPRINT("Buffer Mark on NULL %d\n",__LINE__);
            }
            /* trigger event handler if we are supposed to */
            if((pBufHeader->hMarkTargetComponent == pComponentPrivate->pHandle) && pBufHeader->pMarkData){
                WBAMRENC_DPRINT("OMX Event Mark %d\n",__LINE__);
                pComponentPrivate->cbInfo.EventHandler( pComponentPrivate->pHandle,
                                                        pComponentPrivate->pHandle->pApplicationPrivate,
                                                        OMX_EventMark,
                                                        0,
                                                        0,
                                                        pBufHeader->pMarkData);
            }

            if (pComponentPrivate->curState != OMX_StatePause && !WBAMRENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirInput)) {
                WBAMRENC_DPRINT("line %d:: Calling EmptyBufferDone\n",__LINE__);
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
        /* Make sure that output buffer is issued to output stream only when
         * there is an outstanding input buffer already issued on input stream
         */
         
        /***--------------------------------------****/

        nFrames = pComponentPrivate->nNumOfFramesSent;
        if(nFrames == 0)
            nFrames = 1;
 
        eError = WBAMRENC_GetCorrespondingLCMLHeader(pComponentPrivate,pBufHeader->pBuffer, OMX_DirOutput, &pLcmlHdr);
     
        phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);

        if( (pLcmlHdr->pBufferParam->usNbFrames < nFrames) && (pLcmlHdr->pFrameParam!=NULL) ){
            pBufParmsTemp = (OMX_U8*)pLcmlHdr->pFrameParam; /*This means that more memory need to be used*/
            pBufParmsTemp -=EXTRA_BYTES;
            OMX_WBMEMFREE_STRUCT(pBufParmsTemp);  
            pLcmlHdr->pFrameParam = NULL;
            pBufParmsTemp =NULL;
#ifndef UNDER_CE                    
            OMX_DmmUnMap(phandle->dspCodec->hProc,
                         (void*)pLcmlHdr->pBufferParam->pParamElem,
                         pLcmlHdr->pDmmBuf->pReserved);
            pLcmlHdr->pBufferParam->pParamElem = NULL;
#endif                   
        }

        if(pLcmlHdr->pFrameParam==NULL ){
            WBAMRENC_OMX_MALLOC_SIZE(pBufParmsTemp, (sizeof(WBAMRENC_FrameStruct)*nFrames) + DSP_CACHE_ALIGNMENT,OMX_U8);

            pLcmlHdr->pFrameParam =  (WBAMRENC_FrameStruct*)(pBufParmsTemp + EXTRA_BYTES );
            pLcmlHdr->pBufferParam->pParamElem = NULL;
#ifndef UNDER_CE 
            eError = OMX_DmmMap(phandle->dspCodec->hProc, 
                                (nFrames*sizeof(WBAMRENC_FrameStruct)),
                                (void*)pLcmlHdr->pFrameParam, 
                                (pLcmlHdr->pDmmBuf));                        
            if (eError != OMX_ErrorNone)
                {
                    WBAMRENC_EPRINT("OMX_DmmMap ERRROR!!!!\n");
                    goto EXIT;
                }
                   
            pLcmlHdr->pBufferParam->pParamElem = (WBAMRENC_FrameStruct *)pLcmlHdr->pDmmBuf->pMapped; /*DSP Address*/
#endif
        }

        pLcmlHdr->pBufferParam->usNbFrames = nFrames;

        WBAMRENC_DPRINT ("%d: Sending Empty OUTPUT BUFFER to Codec = %p\n",__LINE__,pBufHeader->pBuffer);
#ifdef __PERF_INSTRUMENTATION__
        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                          PREF(pBufHeader,pBuffer),
                          0,
                          PERF_ModuleCommonLayer);
#endif
        if (pComponentPrivate->curState == OMX_StateExecuting) {
            if (!WBAMRENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirOutput)) {
                WBAMRENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirOutput,__LINE__);
                eError = LCML_QueueBuffer( pLcmlHandle->pCodecinterfacehandle,
                                           EMMCodecOuputBuffer,
                                           (OMX_U8 *)pBufHeader->pBuffer,
                                           WBAMRENC_OUTPUT_FRAME_SIZE * nFrames,
                                           0,
                                           (OMX_U8 *) pLcmlHdr->pBufferParam,
                                           sizeof(WBAMRENC_ParamStruct),
                                           NULL);
                if (eError != OMX_ErrorNone ) {
                    WBAMRENC_EPRINT ("%d :: IssuingDSP OP: Error Occurred\n",__LINE__);
                    eError = OMX_ErrorHardware;
                    goto EXIT;
                }
                pComponentPrivate->lcml_nOpBuf++;
            }
        }
        else if(pComponentPrivate->curState == OMX_StatePause){
            pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = pBufHeader;
        }

        pComponentPrivate->ProcessingOutputBuf--;
    }
    else {
        eError = OMX_ErrorBadParameter;
    }
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting from  WBAMRENC_HandleDataBufFromApp \n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning error %d\n",__LINE__,eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
 * WBAMRENC_GetBufferDirection () This function is used by the component
 * to get the direction of the buffer
 * @param eDir pointer will be updated with buffer direction
 * @param pBufHeader pointer to the buffer to be requested to be filled
 *
 * @retval none
 **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE WBAMRENC_GetBufferDirection(OMX_BUFFERHEADERTYPE *pBufHeader,
                                          OMX_DIRTYPE *eDir)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate = pBufHeader->pPlatformPrivate;
    OMX_U32 nBuf = 0;
    OMX_BUFFERHEADERTYPE *pBuf = NULL;
    OMX_U32 flag = 1,i = 0;
    WBAMRENC_DPRINT("%d :: Entering WBAMRENC_GetBufferDirection Function\n",__LINE__);
    /*Search this buffer in input buffers list */
    nBuf = pComponentPrivate->pInputBufferList->numBuffers;
    for(i=0; i<nBuf; i++) {
        pBuf = pComponentPrivate->pInputBufferList->pBufHdr[i];
        if(pBufHeader == pBuf) {
            *eDir = OMX_DirInput;
            WBAMRENC_DPRINT("%d :: pBufHeader = %p is INPUT BUFFER pBuf = %p\n",__LINE__,pBufHeader,pBuf);
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
            WBAMRENC_DPRINT("%d :: pBufHeader = %p is OUTPUT BUFFER pBuf = %p\n",__LINE__,pBufHeader,pBuf);
            flag = 0;
            goto EXIT;
        }
    }

    if (flag == 1) {
        WBAMRENC_EPRINT("%d :: Buffer %p is Not Found in the List\n",__LINE__, pBufHeader);
        eError = OMX_ErrorUndefined;
        goto EXIT;
    }
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_GetBufferDirection Function\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* -------------------------------------------------------------------*/
/**
 * WBAMRENC_GetCorrespondingLCMLHeader() function will be called by LCML_Callback
 *  component to write the msg
 * @param *pBuffer,          Event which gives to details about USN status
 * @param WBAMRENC_LCML_BUFHEADERTYPE **ppLcmlHdr
 * @param  OMX_DIRTYPE eDir this gives direction of the buffer
 * @retval OMX_NoError              Success, ready to roll
 *         OMX_Error_BadParameter   The input parameter pointer is null
 **/
/* -------------------------------------------------------------------*/
OMX_ERRORTYPE WBAMRENC_GetCorrespondingLCMLHeader(WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate,
                                                  OMX_U8 *pBuffer,
                                                  OMX_DIRTYPE eDir,
                                                  WBAMRENC_LCML_BUFHEADERTYPE **ppLcmlHdr)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    WBAMRENC_LCML_BUFHEADERTYPE *pLcmlBufHeader;
    
    OMX_U32 nIpBuf;
    OMX_U32 nOpBuf;
    OMX_U16 i;

    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;

    WBAMRENC_DPRINT("%d :: Entering WBAMRENC_GetCorrespondingLCMLHeader..\n",__LINE__);
    while (!pComponentPrivate->bInitParamsInitialized) {
        WBAMRENC_DPRINT("%d :: Waiting for init to complete........\n",__LINE__);
#ifndef UNDER_CE
        sched_yield();
#else
        Sleep(0);
#endif
    }
    if(eDir == OMX_DirInput) {
        WBAMRENC_DPRINT("%d :: Entering WBAMRENC_GetCorrespondingLCMLHeader..\n",__LINE__);
        pLcmlBufHeader = pComponentPrivate->pLcmlBufHeader[WBAMRENC_INPUT_PORT];
        for(i = 0; i < nIpBuf; i++) {
            WBAMRENC_DPRINT("%d :: pBuffer = %p\n",__LINE__,pBuffer);
            WBAMRENC_DPRINT("%d :: pLcmlBufHeader->buffer->pBuffer = %p\n",__LINE__,pLcmlBufHeader->buffer->pBuffer);
            if(pBuffer == pLcmlBufHeader->buffer->pBuffer) {
                *ppLcmlHdr = pLcmlBufHeader;
                WBAMRENC_DPRINT("%d :: Corresponding Input LCML Header Found = %p\n",__LINE__,pLcmlBufHeader);
                eError = OMX_ErrorNone;
                goto EXIT;
            }
            pLcmlBufHeader++;
        }
    } else if (eDir == OMX_DirOutput) {
        WBAMRENC_DPRINT("%d :: Entering WBAMRENC_GetCorrespondingLCMLHeader..\n",__LINE__);
        pLcmlBufHeader = pComponentPrivate->pLcmlBufHeader[WBAMRENC_OUTPUT_PORT];
        for(i = 0; i < nOpBuf; i++) {
            WBAMRENC_DPRINT("%d :: pBuffer = %p\n",__LINE__,pBuffer);
            WBAMRENC_DPRINT("%d :: pLcmlBufHeader->buffer->pBuffer = %p\n",__LINE__,pLcmlBufHeader->buffer->pBuffer);
            if(pBuffer == pLcmlBufHeader->buffer->pBuffer) {
                *ppLcmlHdr = pLcmlBufHeader;
                WBAMRENC_DPRINT("%d :: Corresponding Output LCML Header Found = %p\n",__LINE__,pLcmlBufHeader);
                eError = OMX_ErrorNone;
                goto EXIT;
            }
            pLcmlBufHeader++;
        }
    } else {
        WBAMRENC_EPRINT("%d :: Invalid Buffer Type :: exiting...\n",__LINE__);
        eError = OMX_ErrorUndefined;
    }

 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_GetCorrespondingLCMLHeader..\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* -------------------------------------------------------------------*/
/**
 *  WBAMRENC_LCMLCallback() will be called LCML component to write the msg
 *
 * @param event                 Event which gives to details about USN status
 * @param void * args         //    args [0] //bufType;
                              //    args [1] //arm address fpr buffer
                              //    args [2] //BufferSize;
                              //    args [3]  //arm address for param
                              //    args [4] //ParamSize;
                              //    args [6] //LCML Handle
                              * @retval OMX_NoError              Success, ready to roll
                              *         OMX_Error_BadParameter   The input parameter pointer is null
                              **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE WBAMRENC_LCMLCallback (TUsnCodecEvent event,void * args[10])
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U8 *pBuffer = args[1];
    WBAMRENC_LCML_BUFHEADERTYPE *pLcmlHdr;

    OMX_U16 i,index,frameLength, length;
    OMX_COMPONENTTYPE *pHandle;
    LCML_DSP_INTERFACE *pLcmlHandle;
    OMX_U8 nFrames;

    WBAMRENC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE*)((LCML_DSP_INTERFACE*)args[6])->pComponentPrivate;
    pHandle = pComponentPrivate->pHandle;

    WBAMRENC_DPRINT("%d :: Entering the WBAMRENC_LCMLCallback Function\n",__LINE__);

    switch(event) {

    case EMMCodecDspError:
        WBAMRENC_EPRINT("[LCML CALLBACK EVENT]  EMMCodecDspError\n");
        break;

    case EMMCodecInternalError:
        WBAMRENC_EPRINT("[LCML CALLBACK EVENT]  EMMCodecInternalError\n");
        break;

    case EMMCodecInitError:
        WBAMRENC_EPRINT("[LCML CALLBACK EVENT]  EMMCodecInitError\n");
        break;

    case EMMCodecDspMessageRecieved:
        WBAMRENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecDspMessageRecieved\n");
        break;

    case EMMCodecBufferProcessed:
        WBAMRENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecBufferProcessed\n");
        break;

    case EMMCodecProcessingStarted:
        WBAMRENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingStarted\n");
        break;

    case EMMCodecProcessingPaused:
        WBAMRENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingPaused\n");
        break;

    case EMMCodecProcessingStoped:
        WBAMRENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingStoped\n");
        break;

    case EMMCodecProcessingEof:
        WBAMRENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingEof\n");
        break;

    case EMMCodecBufferNotProcessed:
        WBAMRENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecBufferNotProcessed\n");
        break;

    case EMMCodecAlgCtrlAck:
        WBAMRENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecAlgCtrlAck\n");
        break;

    case EMMCodecStrmCtrlAck:
        WBAMRENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecStrmCtrlAck\n");
        break;
    }

    if(event == EMMCodecBufferProcessed)
        {
            if((OMX_U32)args[0] == EMMCodecInputBuffer) {
                pComponentPrivate->nOutStandingEmptyDones++;
                eError = WBAMRENC_GetCorrespondingLCMLHeader(pComponentPrivate,pBuffer, OMX_DirInput, &pLcmlHdr);            
                if (eError != OMX_ErrorNone) {
                    WBAMRENC_EPRINT("%d :: Error: Invalid Buffer Came ...\n",__LINE__);
                    goto EXIT;
                }
#ifdef __PERF_INSTRUMENTATION__
                PERF_ReceivedFrame(pComponentPrivate->pPERFcomp,
                                   PREF(pLcmlHdr->buffer,pBuffer),
                                   0,
                                   PERF_ModuleCommonLayer);
#endif

                WBAMRENC_ClearPending(pComponentPrivate,pLcmlHdr->buffer,OMX_DirInput,__LINE__);

                WBAMRENC_DPRINT("%d :: Calling EmptyBufferDone\n",__LINE__);
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
                pComponentPrivate->nOutStandingEmptyDones--;
                pComponentPrivate->lcml_nIpBuf--;
                pComponentPrivate->app_nBuf++;
            } 
            else if((OMX_U32)args[0] == EMMCodecOuputBuffer) {

                if (!WBAMRENC_IsValid(pComponentPrivate,pBuffer,OMX_DirOutput)) {

                    for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]->pBuffer,
                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]->nFilledLen,
                                          PERF_ModuleHLMM);
#endif
                        pComponentPrivate->cbInfo.FillBufferDone (pComponentPrivate->pHandle,
                                                                  pComponentPrivate->pHandle->pApplicationPrivate,
                                                                  pComponentPrivate->pOutputBufferList->pBufHdr[i++]
                                                                  );
                    }
                } else {
                    WBAMRENC_DPRINT("%d :: OUTPUT: pBuffer = %p\n",__LINE__, pBuffer);
                    pComponentPrivate->nOutStandingFillDones++;
                    eError = WBAMRENC_GetCorrespondingLCMLHeader(pComponentPrivate, pBuffer, OMX_DirOutput, &pLcmlHdr);
                    if (eError != OMX_ErrorNone) {
                        WBAMRENC_EPRINT("%d :: Error: Invalid Buffer Came ...\n",__LINE__);
                        goto EXIT;
                    }
    
                    WBAMRENC_DPRINT("%d :: Output: pLcmlHdr->buffer->pBuffer = %p\n",__LINE__, pLcmlHdr->buffer->pBuffer);
                    pLcmlHdr->buffer->nFilledLen = (OMX_U32)args[8];
                    pComponentPrivate->lcml_nCntOpReceived++;
#ifdef __PERF_INSTRUMENTATION__
                    PERF_ReceivedFrame(pComponentPrivate->pPERFcomp,
                                       PREF(pLcmlHdr->buffer,pBuffer),
                                       PREF(pLcmlHdr->buffer,nFilledLen),
                                       PERF_ModuleCommonLayer);
                    pComponentPrivate->nLcml_nCntOpReceived++;
                    if ((pComponentPrivate->nLcml_nCntIp >= 1) && (pComponentPrivate->nLcml_nCntOpReceived == 1)) {
                        PERF_Boundary(pComponentPrivate->pPERFcomp,
                                      PERF_BoundaryStart | PERF_BoundarySteadyState);
                    }
#endif            
                    WBAMRENC_DPRINT("%d :: Output: pBuffer = %ld\n",__LINE__, pLcmlHdr->buffer->nFilledLen);

                    WBAMRENC_ClearPending(pComponentPrivate,pLcmlHdr->buffer,OMX_DirOutput,__LINE__);

                    pComponentPrivate->LastOutbuf = pLcmlHdr->buffer;  
                    if((pComponentPrivate->frameMode == WBAMRENC_MIMEMODE)&&(pLcmlHdr->buffer->nFilledLen)) {
                        nFrames = (OMX_U8)(pLcmlHdr->buffer->nFilledLen / WBAMRENC_OUTPUT_BUFFER_SIZE_MIME);
                        frameLength=0;
                        length=0;
                        for(i=0;i<nFrames;i++){
                            index = (pLcmlHdr->buffer->pBuffer[i*WBAMRENC_OUTPUT_BUFFER_SIZE_MIME] >> 3) & 0x0F;
                            if(pLcmlHdr->buffer->nFilledLen == 0)
                                length = 0;
                            else 
                                length = (OMX_U16)pComponentPrivate->amrMimeBytes[index];
                            if (i){
                                memmove( pLcmlHdr->buffer->pBuffer + frameLength,
                                         pLcmlHdr->buffer->pBuffer + (i * WBAMRENC_OUTPUT_BUFFER_SIZE_MIME),
                                         length);
                            }
                            frameLength += length;
                        }
                        pLcmlHdr->buffer->nFilledLen= frameLength;

                    }

                    else if((pComponentPrivate->frameMode == WBAMRENC_IF2)&&(pLcmlHdr->buffer->nFilledLen)) {
                        nFrames = (OMX_U8)( pLcmlHdr->buffer->nFilledLen / WBAMRENC_OUTPUT_BUFFER_SIZE_IF2);
                        frameLength=0;
                        length=0;
                        for(i=0;i<nFrames;i++){
                            index = (pLcmlHdr->buffer->pBuffer[i*WBAMRENC_OUTPUT_BUFFER_SIZE_IF2] >> 4) & 0x0F;
                            if(pLcmlHdr->buffer->nFilledLen == 0)
                                length = 0;
                            else
                                length = (OMX_U16)pComponentPrivate->amrIf2Bytes[index];
                            if (i){
                                memmove( pLcmlHdr->buffer->pBuffer + frameLength,
                                         pLcmlHdr->buffer->pBuffer + (i * WBAMRENC_OUTPUT_BUFFER_SIZE_IF2),
                                         length);
                            }

                            frameLength += length;
                        }
                        pLcmlHdr->buffer->nFilledLen= frameLength;
                    }
                    else{
                        nFrames = pLcmlHdr->buffer->nFilledLen/WBAMRENC_OUTPUT_FRAME_SIZE;
                    }
            
                    if( !pComponentPrivate->dasfMode ){
                        /* Copying time stamp information to output buffer */
                        pLcmlHdr->buffer->nTimeStamp = (OMX_TICKS)pComponentPrivate->arrBufIndex[pComponentPrivate->OpBufindex];
                        /* Copying nTickCount information to output buffer */
                        pLcmlHdr->buffer->nTickCount = pComponentPrivate->arrTickCount[pComponentPrivate->OpBufindex];
                        pComponentPrivate->OpBufindex++;
                        pComponentPrivate->OpBufindex %= pComponentPrivate->pPortDef[OMX_DirOutput]->nBufferCountActual;  
                    }    

                    if(pComponentPrivate->InBuf_Eos_alreadysent){
                        if(!pLcmlHdr->buffer->nFilledLen){
                            pLcmlHdr->buffer->nFlags |= OMX_BUFFERFLAG_EOS;
                        }
                        pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                               pHandle->pApplicationPrivate,
                                                               OMX_EventBufferFlag,
                                                               pLcmlHdr->buffer->nOutputPortIndex,
                                                               pLcmlHdr->buffer->nFlags, 
                                                               NULL);
                        pComponentPrivate->InBuf_Eos_alreadysent = 0;
                    }
        
                    /* Non Multi Frame Mode has been tested here */         
#ifdef __PERF_INSTRUMENTATION__
                    PERF_SendingBuffer(pComponentPrivate->pPERFcomp,
                                       pLcmlHdr->buffer->pBuffer,
                                       pLcmlHdr->buffer->nFilledLen,
                                       PERF_ModuleHLMM);
#endif



                    pComponentPrivate->nFillBufferDoneCount++;
                    pComponentPrivate->nOutStandingFillDones--;
                    pComponentPrivate->lcml_nOpBuf--;
                    pComponentPrivate->app_nBuf++;

                    pComponentPrivate->cbInfo.FillBufferDone(pHandle,
                                                             pHandle->pApplicationPrivate,
                                                             pLcmlHdr->buffer);
            
                    WBAMRENC_DPRINT("%d :: Incrementing app_nBuf = %ld\n",__LINE__,pComponentPrivate->app_nBuf);
                }
            }
        }
    else if (event == EMMCodecStrmCtrlAck) {
        WBAMRENC_DPRINT("%d :: GOT MESSAGE USN_DSPACK_STRMCTRL \n",__LINE__);
        if (args[1] == (void *)USN_STRMCMD_FLUSH) {
            pHandle = pComponentPrivate->pHandle;                              
            if ( args[2] == (void *)EMMCodecInputBuffer) {
                if (args[0] == (void *)USN_ERR_NONE ) {
                    WBAMRENC_DPRINT("Flushing input port %d\n",__LINE__);
                    for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {

#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                          pComponentPrivate->pInputBufferList->pBufHdr[i]->pBuffer,
                                          0,
                                          PERF_ModuleHLMM);
#endif                        

                        pComponentPrivate->cbInfo.EmptyBufferDone (
                                                                   pHandle,
                                                                   pHandle->pApplicationPrivate,
                                                                   pComponentPrivate->pInputBufHdrPending[i]
                                                                   );
                        pComponentPrivate->nEmptyBufferDoneCount++;
                        pComponentPrivate->nOutStandingEmptyDones--;

                    }

                    pComponentPrivate->cbInfo.EventHandler(
                                                           pHandle, pHandle->pApplicationPrivate,
                                                           OMX_EventCmdComplete, OMX_CommandFlush,WBAMRENC_INPUT_PORT, NULL);    
                } else {
                    WBAMRENC_EPRINT("LCML reported error while flushing input port\n");
                    goto EXIT;                            
                }
            }
            else if ( args[2] == (void *)EMMCodecOuputBuffer) { 
                if (args[0] == (void *)USN_ERR_NONE ) {                      
                    WBAMRENC_DPRINT("Flushing output port %d\n",__LINE__);
                    for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]->pBuffer,
                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]->nFilledLen,
                                          PERF_ModuleHLMM);
#endif                        
                        pComponentPrivate->cbInfo.FillBufferDone (
                                                                  pHandle,
                                                                  pHandle->pApplicationPrivate,
                                                                  pComponentPrivate->pOutputBufHdrPending[i]
                                                                  );
                        pComponentPrivate->nFillBufferDoneCount++;                                       
                        pComponentPrivate->nOutStandingFillDones--;                     
                        
                    }
                    pComponentPrivate->cbInfo.EventHandler(
                                                           pHandle, pHandle->pApplicationPrivate,
                                                           OMX_EventCmdComplete, OMX_CommandFlush,WBAMRENC_OUTPUT_PORT, NULL);
                } else {
                    WBAMRENC_EPRINT("LCML reported error while flushing output port\n");
                    goto EXIT;                            
                }
            }
        }
    }
    else if(event == EMMCodecProcessingStoped) {
        WBAMRENC_DPRINT("%d :: GOT MESSAGE USN_DSPACK_STOP \n",__LINE__);

        if((pComponentPrivate->nMultiFrameMode == 1) && (pComponentPrivate->frameMode == WBAMRENC_MIMEMODE)) {
            /*Sending Last Buufer Data which on iHoldBuffer to App */
            WBAMRENC_DPRINT("%d :: Sending iMMFDataLastBuffer Data which on iHoldBuffer to App\n",__LINE__);
            WBAMRENC_OMX_MALLOC_SIZE(pComponentPrivate->iMMFDataLastBuffer, WBAMRENC_OUTPUT_BUFFER_SIZE_MIME * (pComponentPrivate->pOutputBufferList->numBuffers + 1),OMX_BUFFERHEADERTYPE);

            WBAMRENC_DPRINT("%d :: pComponentPrivate->iHoldLen = %ld \n",__LINE__,pComponentPrivate->iHoldLen);
            /* Copy the data from iHoldBuffer to dataPtr */
            memcpy(pComponentPrivate->iMMFDataLastBuffer, pComponentPrivate->iHoldBuffer, pComponentPrivate->iHoldLen);
            pComponentPrivate->iMMFDataLastBuffer->nFilledLen = pComponentPrivate->iHoldLen;
            WBAMRENC_DPRINT("%d :: pComponentPrivate->iMMFDataLastBuffer->nFilledLen = %ld \n",__LINE__,pComponentPrivate->iMMFDataLastBuffer->nFilledLen);
            /* Remove the copied data to dataPtr from iHoldBuffer. */
            /*memset(pComponentPrivate->iHoldBuffer, 0, pComponentPrivate->iHoldLen);*/
            pComponentPrivate->iHoldLen = 0;
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                              pComponentPrivate->iMMFDataLastBuffer->pBuffer,
                              pComponentPrivate->iMMFDataLastBuffer->nFilledLen,
                              PERF_ModuleHLMM);
#endif          
            pComponentPrivate->cbInfo.FillBufferDone( pComponentPrivate->pHandle,
                                                      pComponentPrivate->pHandle->pApplicationPrivate,
                                                      pComponentPrivate->iMMFDataLastBuffer);
            pComponentPrivate->nFillBufferDoneCount++;
            pComponentPrivate->nOutStandingFillDones--;
            pComponentPrivate->lcml_nOpBuf--;
            pComponentPrivate->app_nBuf++;
            
            WBAMRENC_DPRINT("%d :: Incrementing app_nBuf = %ld\n",__LINE__,pComponentPrivate->app_nBuf);
        }

        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {

            if (pComponentPrivate->pInputBufferList->bBufferPending[i]) {
#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                  pComponentPrivate->pInputBufferList->pBufHdr[i]->pBuffer,
                                  0,
                                  PERF_ModuleHLMM);
#endif

                pComponentPrivate->cbInfo.EmptyBufferDone (
                                                           pComponentPrivate->pHandle,
                                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                                           pComponentPrivate->pInputBufferList->pBufHdr[i]
                                                           );
                pComponentPrivate->nEmptyBufferDoneCount++;

                pComponentPrivate->nOutStandingEmptyDones--;
                WBAMRENC_ClearPending(pComponentPrivate, pComponentPrivate->pInputBufferList->pBufHdr[i], OMX_DirInput,__LINE__);
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
                pComponentPrivate->cbInfo.FillBufferDone (
                                                          pComponentPrivate->pHandle,
                                                          pComponentPrivate->pHandle->pApplicationPrivate,
                                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]
                                                          );
                pComponentPrivate->nFillBufferDoneCount++;                                       
                pComponentPrivate->nOutStandingFillDones--;

                WBAMRENC_ClearPending(pComponentPrivate, pComponentPrivate->pOutputBufferList->pBufHdr[i], OMX_DirOutput,__LINE__);
            }
        }


        if (!pComponentPrivate->bNoIdleOnStop) {
            pComponentPrivate->nNumOutputBufPending=0;
        
            pComponentPrivate->ProcessingInputBuf=0;
            pComponentPrivate->ProcessingOutputBuf=0;
            pComponentPrivate->InBuf_Eos_alreadysent  =0;
            pComponentPrivate->curState = OMX_StateIdle;
#ifdef RESOURCE_MANAGER_ENABLED        
            eError = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_WBAMR_Encoder_COMPONENT, OMX_StateIdle, 3456, NULL);
#endif

            if (pComponentPrivate->bPreempted == 0) {
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandStateSet,
                                                       pComponentPrivate->curState,
                                                       NULL);
            }
            else {
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                                       OMX_EventError,
                                                       OMX_ErrorResourcesPreempted,
                                                       OMX_TI_ErrorSevere,
                                                       NULL);
            }

        }else{
            pComponentPrivate->bNoIdleOnStop= OMX_FALSE;
            pComponentPrivate->bDspStoppedWhileExecuting = OMX_TRUE;            
        }
    }

    else if(event == EMMCodecDspMessageRecieved) {
        WBAMRENC_DPRINT("%d :: commandedState  = %ld\n",__LINE__,(OMX_U32)args[0]);
        WBAMRENC_DPRINT("%d :: arg1 = %ld\n",__LINE__,(OMX_U32)args[1]);
        WBAMRENC_DPRINT("%d :: arg2 = %ld\n",__LINE__,(OMX_U32)args[2]);

        if(0x0500 == (OMX_U32)args[2]) {
            WBAMRENC_DPRINT("%d :: EMMCodecDspMessageRecieved\n",__LINE__);
        }
    }
    else if(event == EMMCodecAlgCtrlAck) {
        WBAMRENC_DPRINT("%d :: GOT MESSAGE USN_DSPACK_ALGCTRL \n",__LINE__);
    }
    else if (event == EMMCodecDspError) {
#ifdef _ERROR_PROPAGATION__
        /* Cheking for MMU_fault */
        if(((int)args[4] == USN_ERR_UNKNOWN_MSG) && (args[5] == (void*) NULL)) {
            pComponentPrivate->bIsInvalidState=OMX_TRUE;
            pComponentPrivate->curState = OMX_StateInvalid;
            pHandle = pComponentPrivate->pHandle;
            pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                   pHandle->pApplicationPrivate,
                                                   OMX_EventError,
                                                   OMX_ErrorInvalidState, 
                                                   OMX_TI_ErrorSevere,
                                                   NULL);
        }
#endif               
        if(((int)args[4] == USN_ERR_WARNING) && ((int)args[5] == IUALG_WARN_PLAYCOMPLETED)) {
            pHandle = pComponentPrivate->pHandle;
            WBAMRENC_DPRINT("%d :: GOT MESSAGE IUALG_WARN_PLAYCOMPLETED\n",__LINE__);
            if(pComponentPrivate->LastOutbuf)
                pComponentPrivate->LastOutbuf->nFlags = OMX_BUFFERFLAG_EOS;

            pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,                  
                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                   OMX_EventBufferFlag,
                                                   (OMX_U32)NULL,
                                                   OMX_BUFFERFLAG_EOS,
                                                   NULL);
        }
        if((int)args[5] == IUALG_ERR_GENERAL) {
            char *pArgs = "damedesuStr";
            WBAMRENC_EPRINT( "Algorithm error. Cannot continue" );
            WBAMRENC_EPRINT("%d :: LCML_Callback: IUALG_ERR_GENERAL\n",__LINE__);
            pHandle = pComponentPrivate->pHandle;
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
#ifndef UNDER_CE
            eError = LCML_ControlCodec(
                                       ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                       MMCodecControlStop,(void *)pArgs);
            if(eError != OMX_ErrorNone) {
                WBAMRENC_EPRINT("%d: Error Occurred in Codec Stop..\n",__LINE__);
                goto EXIT;
            }
            WBAMRENC_DPRINT("%d :: AMRENC: Codec has been Stopped here\n",__LINE__);
            pComponentPrivate->curState = OMX_StateIdle;
            pComponentPrivate->cbInfo.EventHandler(
                                                   pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, OMX_ErrorNone,0, NULL);
#else
            pComponentPrivate->cbInfo.EventHandler(
                                                   pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventError, OMX_ErrorUndefined,OMX_TI_ErrorSevere, NULL);
#endif
        }
        if( (int)args[5] == IUALG_ERR_DATA_CORRUPT ){
            char *pArgs = "damedesuStr";
            WBAMRENC_EPRINT( "Algorithm error. Corrupt data" );
            WBAMRENC_EPRINT("%d :: LCML_Callback: IUALG_ERR_DATA_CORRUPT\n",__LINE__);
            pHandle = pComponentPrivate->pHandle;
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
#ifndef UNDER_CE
            eError = LCML_ControlCodec(
                                       ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                       MMCodecControlStop,(void *)pArgs);
            if(eError != OMX_ErrorNone) {
                WBAMRENC_EPRINT("%d: Error Occurred in Codec Stop..\n",__LINE__);
                goto EXIT;
            }
            WBAMRENC_DPRINT("%d :: AMRENC: Codec has been Stopped here\n",__LINE__);
            pComponentPrivate->curState = OMX_StateIdle;
            pComponentPrivate->cbInfo.EventHandler(
                                                   pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, OMX_ErrorNone,0, NULL);
#else
            pComponentPrivate->cbInfo.EventHandler(
                                                   pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventError, OMX_ErrorUndefined,OMX_TI_ErrorSevere, NULL);
#endif
        }
    }
    else if (event == EMMCodecProcessingPaused) { 
        pComponentPrivate->curState = OMX_StatePause;
        pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                pHandle->pApplicationPrivate,
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
                                                   OMX_TI_ErrorSevere,
                                                   NULL);
        }   
    }
    else if (event ==EMMCodecInternalError){
        /* Cheking for MMU_fault */
        if(((int)args[4] == USN_ERR_UNKNOWN_MSG) && (args[5] == (void*)NULL)) {
            WBAMRENC_EPRINT("%d :: UTIL: MMU_Fault \n",__LINE__);
            pComponentPrivate->bIsInvalidState=OMX_TRUE;
            pComponentPrivate->curState = OMX_StateInvalid;
            pHandle = pComponentPrivate->pHandle;
            pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                   pHandle->pApplicationPrivate,
                                                   OMX_EventError,
                                                   OMX_ErrorInvalidState, 
                                                   OMX_TI_ErrorSevere,
                                                   NULL);
        }

    }
#endif  
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting the WBAMRENC_LCMLCallback Function\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);

    return eError;
}

/* ================================================================================= */
/**
 *  WBAMRENC_GetLCMLHandle()
 *
 * @retval OMX_HANDLETYPE
 */
/* ================================================================================= */
#ifndef UNDER_CE
OMX_HANDLETYPE WBAMRENC_GetLCMLHandle(WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE (*fpGetHandle)(OMX_HANDLETYPE);
    OMX_HANDLETYPE pHandle = NULL;
    void *handle;
    char *error;

    WBAMRENC_DPRINT("%d :: Entering WBAMRENC_GetLCMLHandle..\n",__LINE__);
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
        WBAMRENC_EPRINT("%d :: OMX_ErrorUndefined...\n",__LINE__);
        pHandle = NULL;
        goto EXIT;
    }
 
    pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE*)pComponentPrivate;
    ((LCML_DSP_INTERFACE*)pHandle)->pComponentPrivate = pComponentPrivate; 
     
    pComponentPrivate->ptrLibLCML=handle;           /* saving LCML lib pointer  */
    
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_GetLCMLHandle..\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return pHandle;
}

#else
/*WINDOWS Explicit dll load procedure*/
OMX_HANDLETYPE WBAMRENC_GetLCMLHandle(WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    typedef OMX_ERRORTYPE (*LPFNDLLFUNC1)(OMX_HANDLETYPE);
    OMX_HANDLETYPE pHandle = NULL;
    OMX_ERRORTYPE eError;
    HINSTANCE hDLL;               // Handle to DLL
    LPFNDLLFUNC1 fpGetHandle1;
    hDLL = LoadLibraryEx(TEXT("OAF_BML.dll"), NULL,0);
    if (hDLL == NULL) {

        WBAMRENC_EPRINT("BML Load Failed!!!\n");
        return pHandle;
    }
    fpGetHandle1 = (LPFNDLLFUNC1)GetProcAddress(hDLL,TEXT("GetHandle"));
    if (!fpGetHandle1) {
        // handle the error
        FreeLibrary(hDLL);

        return pHandle;
    }
    // call the function
    eError = fpGetHandle1(&pHandle);
    if(eError != OMX_ErrorNone) {
        eError = OMX_ErrorUndefined;
        WBAMRENC_EPRINT("eError != OMX_ErrorNone...\n");


        pHandle = NULL;
        return pHandle;
    }
    ((LCML_DSP_INTERFACE*)pHandle)->pComponentPrivate = pComponentPrivate; 
    return pHandle;
}
#endif

/* ================================================================================= */
/**
 * @fn WBAMRENC_SetPending() description for WBAMRENC_SetPending
 WBAMRENC_SetPending().
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
void WBAMRENC_SetPending(WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate,
                         OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir, OMX_U32 lineNumber)
{
    OMX_U16 i;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 1;
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
            
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 1;
            }
        }
    }
}
/* ================================================================================= */
/**
 * @fn WBAMRENC_ClearPending() description for WBAMRENC_ClearPending
 WBAMRENC_ClearPending().
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
void WBAMRENC_ClearPending(WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate,
                           OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir, OMX_U32 lineNumber)
{
    OMX_U16 i;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 0;
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 0;
            }
        }
    }
}
/* ================================================================================= */
/**
 * @fn WBAMRENC_IsPending() description for WBAMRENC_IsPending
 WBAMRENC_IsPending().
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
OMX_U32 WBAMRENC_IsPending(WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate,
                           OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir)
{
    OMX_U16 i;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                return pComponentPrivate->pInputBufferList->bBufferPending[i];
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                return pComponentPrivate->pOutputBufferList->bBufferPending[i];
            }
        }
    }
    return -1;
}
/* ================================================================================= */
/**
 * @fn WBAMRENC_IsValid() description for WBAMRENC_IsValid
 WBAMRENC_IsValid().
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
OMX_U32 WBAMRENC_IsValid(WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate,
                         OMX_U8 *pBuffer, OMX_DIRTYPE eDir)
{
    OMX_U16 i;
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
 * @WBAMRENC_FillLCMLInitParamsEx() This function is used by the component thread to
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
OMX_ERRORTYPE WBAMRENC_FillLCMLInitParamsEx(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf,nIpBufSize,nOpBuf,nOpBufSize;
    OMX_BUFFERHEADERTYPE *pTemp;
    char *ptr;
    LCML_DSP_INTERFACE *pHandle = (LCML_DSP_INTERFACE *)pComponent;
    WBAMRENC_COMPONENT_PRIVATE *pComponentPrivate = pHandle->pComponentPrivate;
    WBAMRENC_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;
    OMX_U16 i;
    OMX_U32 size_lcml;
    OMX_U8 *pBufferParamTemp;
        
    WBAMRENC_DPRINT("%d :: WBAMRENC_FillLCMLInitParamsEx\n",__LINE__);
    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nIpBufSize = pComponentPrivate->pPortDef[WBAMRENC_INPUT_PORT]->nBufferSize;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    nOpBufSize = pComponentPrivate->pPortDef[WBAMRENC_OUTPUT_PORT]->nBufferSize;
    WBAMRENC_DPRINT("%d :: ------ Buffer Details -----------\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Input  Buffer Count = %ld\n",__LINE__,nIpBuf);
    WBAMRENC_DPRINT("%d :: Input  Buffer Size = %ld\n",__LINE__,nIpBufSize);
    WBAMRENC_DPRINT("%d :: Output Buffer Count = %ld\n",__LINE__,nOpBuf);
    WBAMRENC_DPRINT("%d :: Output Buffer Size = %ld\n",__LINE__,nOpBufSize);
    WBAMRENC_DPRINT("%d :: ------ Buffer Details ------------\n",__LINE__);
    /* Allocate memory for all input buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = nIpBuf * sizeof(WBAMRENC_LCML_BUFHEADERTYPE);
    WBAMRENC_OMX_MALLOC_SIZE(pTemp_lcml, size_lcml,WBAMRENC_LCML_BUFHEADERTYPE);


    pComponentPrivate->pLcmlBufHeader[WBAMRENC_INPUT_PORT] = pTemp_lcml;
    for (i=0; i<nIpBuf; i++) {
        WBAMRENC_DPRINT("%d :: INPUT--------- Inside Ip Loop\n",__LINE__);
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = WBAMRENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = WBAMRENC_MINOR_VER;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = WBAMRENC_NOT_USED;
        pTemp_lcml->buffer = pTemp;
        WBAMRENC_DPRINT("%d :: pTemp_lcml->buffer->pBuffer = %p \n",__LINE__,pTemp_lcml->buffer->pBuffer);
        pTemp_lcml->eDir = OMX_DirInput;

        WBAMRENC_OMX_MALLOC_SIZE(pBufferParamTemp, sizeof(WBAMRENC_ParamStruct) + DSP_CACHE_ALIGNMENT,OMX_U8);

        pTemp_lcml->pBufferParam =  (WBAMRENC_ParamStruct*)(pBufferParamTemp + EXTRA_BYTES);     
        
        pTemp_lcml->pBufferParam->usNbFrames=0;
        pTemp_lcml->pBufferParam->pParamElem=NULL;
        pTemp_lcml->pFrameParam=NULL;

        WBAMRENC_OMX_MALLOC(pTemp_lcml->pDmmBuf, DMM_BUFFER_OBJ);

        
        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */
        pTemp->nFlags = WBAMRENC_NORMAL_BUFFER;
        pTemp++;
        pTemp_lcml++;
    }

    /* Allocate memory for all output buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = nOpBuf * sizeof(WBAMRENC_LCML_BUFHEADERTYPE);

    WBAMRENC_OMX_MALLOC_SIZE(pTemp_lcml, size_lcml,WBAMRENC_LCML_BUFHEADERTYPE);


    pComponentPrivate->pLcmlBufHeader[WBAMRENC_OUTPUT_PORT] = pTemp_lcml;
    for (i=0; i<nOpBuf; i++) {
        WBAMRENC_DPRINT("%d :: OUTPUT--------- Inside Op Loop\n",__LINE__);
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = WBAMRENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = WBAMRENC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = WBAMRENC_NOT_USED;
        pTemp_lcml->buffer = pTemp;
        WBAMRENC_DPRINT("%d :: pTemp_lcml->buffer->pBuffer = %p \n",__LINE__,pTemp_lcml->buffer->pBuffer);
        pTemp_lcml->eDir = OMX_DirOutput;
        
        WBAMRENC_OMX_MALLOC_SIZE(pTemp_lcml->pBufferParam,
                                 (sizeof(WBAMRENC_ParamStruct)+DSP_CACHE_ALIGNMENT),
                                 WBAMRENC_ParamStruct); 
                                
        ptr = (char*)pTemp_lcml->pBufferParam;
        ptr += EXTRA_BYTES;
        pTemp_lcml->pBufferParam = (WBAMRENC_ParamStruct*)ptr;

        
        pTemp_lcml->pBufferParam->usNbFrames=0;
        pTemp_lcml->pBufferParam->pParamElem=NULL;
        pTemp_lcml->pFrameParam=NULL;

        
        WBAMRENC_OMX_MALLOC(pTemp_lcml->pDmmBuf, DMM_BUFFER_OBJ);
        
        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */
        pTemp->nFlags = WBAMRENC_NORMAL_BUFFER;
        pTemp++;
        pTemp_lcml++;
    }
    pComponentPrivate->bInitParamsInitialized = 1;
 EXIT:
    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_FillLCMLInitParamsEx\n",__LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
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
    DSP_STATUS status;
    int nSizeReserved = 0;

    if(pDmmBuf == NULL)
        {
            WBAMRENC_EPRINT("pBuf is NULL\n");
            eError = OMX_ErrorBadParameter;
            goto EXIT;
        }

    if(pArmPtr == NULL)
        {
            WBAMRENC_EPRINT("pBuf is NULL\n");
            eError = OMX_ErrorBadParameter;
            goto EXIT;
        }

    /* Allocate */
    pDmmBuf->pAllocated = pArmPtr;

    /* Reserve */
    nSizeReserved = ROUND_TO_PAGESIZE(size) + 2*DMM_PAGE_SIZE ;
    status = DSPProcessor_ReserveMemory(ProcHandle, nSizeReserved, &(pDmmBuf->pReserved));
    
    if(DSP_FAILED(status))
        {
            WBAMRENC_EPRINT("DSPProcessor_ReserveMemory() failed - error 0x%x", (int)status);
            eError = OMX_ErrorHardware;
            goto EXIT;
        }
    pDmmBuf->nSize = size;
    WBAMRENC_DPRINT(" DMM MAP Reserved: %p, size 0x%x (%d)\n", pDmmBuf->pReserved,nSizeReserved,nSizeReserved);
    
    /* Map */
    status = DSPProcessor_Map(ProcHandle,
                              pDmmBuf->pAllocated,/* Arm addres of data to Map on DSP*/
                              size , /* size to Map on DSP*/
                              pDmmBuf->pReserved, /* reserved space */
                              &(pDmmBuf->pMapped), /* returned map pointer */
                              0); /* final param is reserved.  set to zero. */
    if(DSP_FAILED(status))
        {
            WBAMRENC_EPRINT("DSPProcessor_Map() failed - error 0x%x", (int)status);
            eError = OMX_ErrorHardware;
            goto EXIT;
        }
    WBAMRENC_DPRINT("DMM Mapped: %p, size 0x%x (%d)\n",pDmmBuf->pMapped, size,size);

    /* Issue an initial memory flush to ensure cache coherency */
    status = DSPProcessor_FlushMemory(ProcHandle, pDmmBuf->pAllocated, size, 0);
    if(DSP_FAILED(status))
        {
            WBAMRENC_EPRINT("Unable to flush mapped buffer: error 0x%x",(int)status);
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
    DSP_STATUS status = DSP_SOK;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    /*    printf("OMX UnReserve DSP: %p\n",pResPtr);*/

    if(pMapPtr == NULL)
        {
            WBAMRENC_EPRINT("pMapPtr is NULL\n");
            eError = OMX_ErrorBadParameter;
            goto EXIT;
        }
    if(pResPtr == NULL)
        {
            WBAMRENC_EPRINT("pResPtr is NULL\n");
            eError = OMX_ErrorBadParameter;
            goto EXIT;
        }
    status = DSPProcessor_UnMap(ProcHandle,pMapPtr);
    if(DSP_FAILED(status))
        {
            WBAMRENC_EPRINT("DSPProcessor_UnMap() failed - error 0x%x",(int)status);
        }

    WBAMRENC_DPRINT("unreserving  structure =0x%p\n",pResPtr );
    status = DSPProcessor_UnReserveMemory(ProcHandle,pResPtr);
    if(DSP_FAILED(status))
        {
            WBAMRENC_EPRINT("DSPProcessor_UnReserveMemory() failed - error 0x%x", (int)status);
        }

 EXIT:
    return eError;
}

/* void WBAMRENC_ResourceManagerCallback(RMPROXY_COMMANDDATATYPE cbData)
{
    OMX_COMMANDTYPE Cmd = OMX_CommandStateSet;
    OMX_STATETYPE state = OMX_StateIdle;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)cbData.hComponent;
    WBAMRENC_COMPONENT_PRIVATE *pCompPrivate = NULL;

    pCompPrivate = (WBAMRENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

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

} */

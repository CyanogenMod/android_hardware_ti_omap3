
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
 * @file OMX_G722Enc_Utils.c
 *
 * This file implements OMX Component for G722 Encoder that
 * is fully compliant with the OMX Audio specification .
 *
 * @path  $(OMAPSW_MPU)\linux\audio\src\openmax_il\g722_enc\src
 *
 * @rev  0.1
 */
/* ----------------------------------------------------------------------------- 
 *! 
 *! Revision History 
 *! ===================================
 *! Date         Author(s)            Version  Description
 *! ---------    -------------------  -------  ---------------------------------
 *! 08-Mar-2007  A.Donjon             0.1      Code update for G722 ENCODER
 *! 
 *!
 * ================================================================================= */


/* ------compilation control switches -------------------------*/
/****************************************************************
 *  INCLUDE FILES
 ****************************************************************/
/* ----- system and platform files ----------------------------*/
#include <unistd.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <malloc.h>
#include <memory.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <sys/time.h>

#include <pthread.h>
#include <dbapi.h>
#include <string.h>
#include <stdio.h>

#include "OMX_Component.h"
#include "LCML_DspCodec.h"
#include "OMX_G722Encoder.h"
#include "OMX_G722Enc_Utils.h"
#include "g722encsocket_ti.h"
#include <encode_common_ti.h>
#include "usn.h"

int FillBufferDoneCount = 0;
static int gLcmlPipeWr = 0;
OMX_U32 pValues[4] ={0};
static G722ENC_COMPONENT_PRIVATE *pComponentPrivate_CC = NULL;

/* ================================================================================= */
/**
 * @fn ComponentThread() Component thread
 *
 *  @see         OMX_ComponentThread.h
 */
/* ================================================================================ */
void* ComponentThread (void* pThreadData)
{
    int status = 0;
    struct timespec tv; /*timeval tv;*/
    int fdmax = 0;
    fd_set rfds;
    OMX_U32 nRet = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBufHeader = NULL;
    int ret = 0;


    G722ENC_COMPONENT_PRIVATE* pComponentPrivate = (G722ENC_COMPONENT_PRIVATE*)pThreadData;
    OMX_COMPONENTTYPE *pHandle = pComponentPrivate->pHandle;

    fdmax = pComponentPrivate->cmdPipe[0];

    if (pComponentPrivate->dataPipe[0] > fdmax) {
        fdmax = pComponentPrivate->dataPipe[0];
    }

    while (1) {
        FD_ZERO (&rfds);
        FD_SET (pComponentPrivate->cmdPipe[0], &rfds);
        FD_SET (pComponentPrivate->dataPipe[0], &rfds);
        tv.tv_sec = 1;
        tv.tv_nsec = 0; /*usec = 0;*/

        sigset_t set;
        sigemptyset (&set);
        sigaddset (&set, SIGALRM);
        status = pselect (fdmax+1, &rfds, NULL, NULL, &tv, &set);

        if (0 == status) {
            G722ENC_DPRINT("%d : bIsStopping = %ld\n",__LINE__,
                           pComponentPrivate->bIsStopping);

            G722ENC_DPRINT("%d : app_nBuf = %ld\n",__LINE__,
                           pComponentPrivate->app_nBuf);

            if (pComponentPrivate->bIsStopping == 1) {

                G722ENC_DPRINT ("%d :: Component stopping\n",__LINE__);
                pComponentPrivate->lcml_nOpBuf = 0;
                pComponentPrivate->lcml_nIpBuf = 0;
                pComponentPrivate->app_nBuf = 0;
                pComponentPrivate->bIsStopping = 0;
                pComponentPrivate->bIsEOFSent = 0;
                if (pComponentPrivate->curState != OMX_StateIdle) {
                    goto EXIT;
                }
            }
        } else if (-1 == status) {
            G722ENC_DPRINT ("%d :: Error in Select - errno = %d\n", __LINE__,errno);
            pComponentPrivate->cbInfo.EventHandler (
                                                    pHandle,pHandle->pApplicationPrivate,
                                                    OMX_EventError,OMX_ErrorInsufficientResources, 0,
                                                    "Error from COmponent Thread in select");
            eError = OMX_ErrorInsufficientResources;

        } else if (FD_ISSET (pComponentPrivate->cmdPipe[0], &rfds)) {
            G722ENC_DPRINT ("%d :: CMD pipe is set in Component Thread\n",__LINE__);
            G722ENC_DPRINT ("%d :: pComponentPrivate = %p\n",__LINE__,pComponentPrivate);
            nRet = G722ENC_HandleCommand (pComponentPrivate);
            if (nRet == EXIT_COMPONENT_THRD) {
                G722ENC_DPRINT ("Exiting from Component thread\n");
                G722ENC_CleanupInitParams(pHandle);
                if(eError != OMX_ErrorNone) {
                    G722ENC_DPRINT("%d :: Function G722Enc_FreeCompResources returned\
                                                                    error\n",__LINE__);
                    goto EXIT;
                }
                G722ENC_DPRINT("%d :: ARM Side Resources Have Been Freed\n",__LINE__);

                pComponentPrivate->curState = OMX_StateLoaded;
                if (pComponentPrivate->bPreempted == 0) { 
                    pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                           pHandle->pApplicationPrivate,
                                                           OMX_EventCmdComplete,
                                                           OMX_ErrorNone,
                                                           pComponentPrivate->curState, 
                                                           NULL);
                    G722ENC_DPRINT("%d %s Component loaded (OMX_StateLoaded)\n",
                                   __LINE__,__FUNCTION__);
                }else{
                    pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                           pHandle->pApplicationPrivate,
                                                           OMX_EventError,
                                                           OMX_ErrorResourcesLost,
                                                           pComponentPrivate->curState, 
                                                           NULL);
                    pComponentPrivate->bPreempted = 0;
                }
            }
        }  
        else if (FD_ISSET (pComponentPrivate->dataPipe[0], &rfds)) {
            
            G722ENC_DPRINT ("%d :: DATA pipe is set in Component Thread\n",__LINE__);
            ret = read(pComponentPrivate->dataPipe[0], &pBufHeader, sizeof(pBufHeader));
            if (ret == -1) {
                G722ENC_DPRINT ("%d :: Error while reading from the pipe\n",__LINE__);
            }

            eError = G722ENC_HandleDataBuf_FromApp (pBufHeader,pComponentPrivate);
            if (eError != OMX_ErrorNone) {
                G722ENC_DPRINT ("%d :: Error From G722ENC_HandleDataBuf_FromApp\n",__LINE__);
                break;
            }
        }
    }
 EXIT:
    G722ENC_DPRINT ("%d :: Exiting ComponentThread \n",__LINE__);
    return (void*)eError;
}

/* ================================================================================= */
/**
 * @fn G722ENC_Fill_LCMLInitParams() description for G722ENC_Fill_LCMLInitParams  
 G722ENC_Fill_LCMLInitParams().  
 This function is used by the component thread to
 ill the all of its initialization parameters, buffer deatils  etc
 to LCML structure
 * @param pComponent  handle for this instance of the component
 * @param plcml_Init  pointer to LCML structure to be filled
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ================================================================================ */
OMX_ERRORTYPE G722ENC_Fill_LCMLInitParams(OMX_HANDLETYPE pComponent,
                                          LCML_DSP *plcml_Init)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0,nIpBufSize = 0,nOpBuf = 0,nOpBufSize = 0;
    OMX_U16 i = 0;
    OMX_BUFFERHEADERTYPE *pTemp = NULL;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    int size_lcml = 0;
    LCML_STRMATTR *strmAttr = NULL;
    OMX_U16 *arr = NULL;
    G722ENC_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;
    G722ENC_COMPONENT_PRIVATE *pComponentPrivate = pHandle->pComponentPrivate;

    G722ENC_DPRINT("%d :: Entered G722ENC_Fill_LCMLInitParams",__LINE__);

    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nIpBufSize = pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->nBufferSize;
    nOpBufSize = pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->nBufferSize;
    G722ENC_DPRINT("pComponentPrivate->pOutputBufferList->numBuffers = %d\n",pComponentPrivate->pOutputBufferList->numBuffers);
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    pComponentPrivate->noInitInputBuf = nIpBuf;
    pComponentPrivate->noInitOutputBuf = nOpBuf;
    plcml_Init->In_BufInfo.nBuffers = nIpBuf;
    plcml_Init->In_BufInfo.nSize = nIpBufSize;
    plcml_Init->In_BufInfo.DataTrMethod = DMM_METHOD;
    plcml_Init->Out_BufInfo.nBuffers = nOpBuf;
    plcml_Init->Out_BufInfo.nSize = nOpBufSize;
    plcml_Init->Out_BufInfo.DataTrMethod = DMM_METHOD;
    plcml_Init->NodeInfo.nNumOfDLLs = 3;

    plcml_Init->NodeInfo.AllUUIDs[0].uuid = &G722ENCSOCKET_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[0].DllName, G722ENC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[0].eDllType = DLL_NODEOBJECT;
    
    plcml_Init->NodeInfo.AllUUIDs[1].uuid = &G722ENCSOCKET_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[1].DllName, G722ENC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[1].eDllType = DLL_DEPENDENT;

    plcml_Init->NodeInfo.AllUUIDs[2].uuid = &ENCODE_COMMON_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[2].DllName,USN_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[2].eDllType = DLL_DEPENDENT;
    
    plcml_Init->SegID = OMX_G722ENC_DEFAULT_SEGMENT;
    plcml_Init->Timeout = OMX_G722ENC_SN_TIMEOUT;
    plcml_Init->Alignment = 0;
    plcml_Init->Priority = OMX_G722ENC_SN_PRIORITY;
    plcml_Init->DeviceInfo.TypeofDevice = 0;
    plcml_Init->ProfileID = -1;

    if(pComponentPrivate->dasfmode == 1) {
        G722ENC_DPRINT("%d :: G722 ENCODER IS RUNNING UNDER DASF MODE \n",__LINE__);
        
        OMX_MALLOC_SIZE_DSPALIGN(pComponentPrivate->pParams, sizeof(G722ENC_AudioCodecParams), G722ENC_AudioCodecParams);
        if(pComponentPrivate->pParams == NULL) {
            G722ENC_DPRINT("%d :: %s OMX_ErrorInsufficientResources\n",__LINE__,__FUNCTION__);
            G722ENC_CleanupInitParams(pComponentPrivate->pHandle);
            return OMX_ErrorInsufficientResources;
        }
        OMX_MALLOC_GENERIC (strmAttr, LCML_STRMATTR);
        pComponentPrivate->strmAttr = strmAttr;
        G722ENC_MEMPRINT("%d:::[ALLOC] %p\n",__LINE__,strmAttr);
        strmAttr->uSegid = 0;
        strmAttr->uAlignment = 0;
        strmAttr->uTimeout = OMX_G722ENC_SN_TIMEOUT;
        strmAttr->uBufsize = G722ENC_OUTPUT_BUFFER_SIZE_BYTES; 
        strmAttr->uNumBufs = G722ENC_NUM_INPUT_BUFFERS + G722ENC_NUM_OUTPUT_BUFFERS;
        strmAttr->lMode = STRMMODE_PROCCOPY;
        plcml_Init->DeviceInfo.TypeofDevice = 1;
        plcml_Init->DeviceInfo.TypeofRender = 1;  /* set for record */
        /* add to support Tee Device Node */
        if(pComponentPrivate->teemode == 1) {
            plcml_Init->DeviceInfo.AllUUIDs[0].uuid = &TEEDN_TI_UUID;
        }
        else {
            plcml_Init->DeviceInfo.AllUUIDs[0].uuid = &DCTN_TI_UUID;
        }
        plcml_Init->DeviceInfo.DspStream = strmAttr;
    }
    
    OMX_MALLOC_SIZE(arr, 10 * sizeof(OMX_U16), OMX_U16);
    pComponentPrivate->pCreatePhaseArgs = arr;
    if (pComponentPrivate->dasfmode == 1){
        G722ENC_DPRINT("%d :: G722 ENCODER CREATE PHASE UNDER DASF MODE \n",__LINE__);
        arr[0] = G722ENC_NUM_STREAMS;
        arr[1] = G722ENC_INPUT_PORT;                                        /*Stream ID*/
        arr[2] = G722ENCSTREAMINPUT;                                       /*DMM based input stream DMM - 0 INPUT - 1 */
        arr[3] = G722ENC_NUM_INPUT_DASF_BUFFERS;                            /*Number of buffers on input stream*/
        arr[4] = G722ENC_OUTPUT_PORT;                                       /*Stream ID*/
        arr[5] = G722ENCSTREAMDMM;                                         /*DMM based output stream DMM - 0 OUTPUT - 2*/
        arr[6] = (OMX_U16)pComponentPrivate->pOutputBufferList->numBuffers;  /*Number of buffers on output stream*/
    } 
    else {
        G722ENC_DPRINT("%d :: G722 ENCODER CREATE PHASE UNDER FILE MODE \n",__LINE__);
        arr[0] = G722ENC_NUM_STREAMS;
        arr[1] = G722ENC_INPUT_PORT;                                        /*Stream ID*/
        arr[2] = G722ENCSTREAMDMM;                                         /*DMM based input stream DMM - 0 INPUT - 1*/

        if (pComponentPrivate->pInputBufferList->numBuffers) {
            arr[3] = (OMX_U16)pComponentPrivate->pInputBufferList->numBuffers;   /*Number of buffers on input stream*/
        }
        else {
            arr[3] = 1;
        }
        arr[4] = G722ENC_OUTPUT_PORT;                                       /*Stream ID*/
        arr[5] = G722ENCSTREAMDMM;                                         /*DMM based output stream DMM - 0 OUTPUT - 2*/

        if (pComponentPrivate->pOutputBufferList->numBuffers) {
            arr[6] = (OMX_U16)pComponentPrivate->pOutputBufferList->numBuffers;  /*Number of buffers on output stream*/
        }
        else {
            arr[6] = 1;
        }
    }
    arr[7] = (OMX_U16)pComponentPrivate->g722Params->nSampleRate;   /* Rate selection */
    arr[8] = G722ENC_INPUT_BUFFER_SIZE; 
    arr[9] = END_OF_CR_PHASE_ARGS;
    plcml_Init->pCrPhArgs = arr;
    /* Allocate memory for all input buffer headers..
     * This memory pointer will be sent to LCML */

    size_lcml = nIpBuf * sizeof(G722ENC_LCML_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, G722ENC_LCML_BUFHEADERTYPE);
    pComponentPrivate->pLcmlBufHeader[G722ENC_INPUT_PORT] = pTemp_lcml;

    for (i=0; i<nIpBuf; i++) {
        G722ENC_DPRINT("%d :: --------- Inside Ip Loop\n",__LINE__);
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nIpBufSize;
        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = G722ENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = G722ENC_MINOR_VER;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        pTemp_lcml->pBufHdr = pTemp;
        pTemp_lcml->eDir = OMX_DirInput;
        pTemp_lcml->pOtherParams[i] = NULL;
        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pIpParam, sizeof(G722ENC_UAlgInBufParamStruct), G722ENC_UAlgInBufParamStruct);
        if (pTemp_lcml->pIpParam == NULL) {
            OMX_MEMFREE_STRUCT(arr);
            G722ENC_CleanupInitParams(pComponent);
            G722ENC_DPRINT("%d :: Exiting AllocateBuffer\n",__LINE__);
            return OMX_ErrorInsufficientResources;
         }

        pTemp_lcml->pIpParam->bLastBuffer = 0;     
             
        pTemp->nFlags = NORMAL_BUFFER;
        ((G722ENC_COMPONENT_PRIVATE *) pTemp->pPlatformPrivate)->pHandle = pHandle;
        G722ENC_DPRINT("%d ::Comp: InBuffHeader[%d] = %p\n", __LINE__, i, pTemp);
        G722ENC_DPRINT("%d ::Comp:  >>>> InputBuffHeader[%d]->pBuffer = %p\n",
                       __LINE__, i, pTemp->pBuffer);
        G722ENC_DPRINT("%d ::Comp: Ip : pTemp_lcml[%d] = %p\n", __LINE__, i, pTemp_lcml);

        pTemp_lcml++;
    }

    pTemp = pComponentPrivate->pBufHeader[G722ENC_OUTPUT_PORT];
    size_lcml = nOpBuf * sizeof(G722ENC_LCML_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, G722ENC_LCML_BUFHEADERTYPE);
    pComponentPrivate->pLcmlBufHeader[G722ENC_OUTPUT_PORT] = pTemp_lcml;


    G722ENC_DPRINT("nOpBuf = %d\n",nOpBuf);

    for (i=0; i<nOpBuf; i++) {
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nOpBufSize;
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = G722ENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = G722ENC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        pTemp_lcml->pBufHdr = pTemp;
        pTemp_lcml->eDir = OMX_DirOutput;
        pTemp_lcml->pOtherParams[i] = NULL;

        pTemp->nFlags = NORMAL_BUFFER;
        ((G722ENC_COMPONENT_PRIVATE *)pTemp->pPlatformPrivate)->pHandle = pHandle;
        G722ENC_DPRINT("%d ::Comp:  >>>>>>>>>>>>> OutBuffHeader[%d] = %p\n",
                       __LINE__, i, pTemp);
        G722ENC_DPRINT("%d ::Comp:  >>>> OutBuffHeader[%d]->pBuffer = %p\n",
                       __LINE__, i, pTemp->pBuffer);
        G722ENC_DPRINT("%d ::Comp: Op : pTemp_lcml[%d] = %p\n", __LINE__, i, pTemp_lcml);
        pTemp++;
        pTemp_lcml++;
    }
    pComponentPrivate->bPortDefsAllocated = 1;

    G722ENC_DPRINT("%d :: Exiting G722ENC_Fill_LCMLInitParams",__LINE__);

    pComponentPrivate->bInitParamsInitialized = 1;
 EXIT:

    if(eError != OMX_ErrorNone)
    {
        OMX_MEMFREE_STRUCT(arr);
        G722ENC_CleanupInitParams(pComponent);
    }

    return eError;
}

/* ================================================================================= */
/**
 * @fn G722Enc_StartCompThread() description for G722Enc_StartCompThread  
 G722Enc_StartCompThread().  
 This function is called by the component to create
 the component thread, command pipe, data pipe and LCML Pipe
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ================================================================================ */
OMX_ERRORTYPE G722Enc_StartCompThread(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    G722ENC_COMPONENT_PRIVATE *pComponentPrivate =
        (G722ENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    G722ENC_DPRINT ("%d :: Enetering  G722Enc_StartCompThread\n", __LINE__);
    /* initialize values */
    pComponentPrivate->bIsStopping = 0;
    pComponentPrivate->lcml_nOpBuf = 0;
    pComponentPrivate->lcml_nIpBuf = 0;
    pComponentPrivate->app_nBuf = 0;
    pComponentPrivate->bIsEOFSent = 0;
    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->dataPipe);
    G722ENC_DPRINT ("%d :: G722Enc_StartCompThread() - error = %d\n", __LINE__,eError);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* create the pipe used to send commands to the thread */
    eError = pipe (pComponentPrivate->cmdPipe);
    G722ENC_DPRINT ("%d :: G722Enc_StartCompThread() - error = %d\n", __LINE__,eError);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    eError = pipe (pComponentPrivate->cmdDataPipe);
    G722ENC_DPRINT ("%d :: G722Dec_StartCompThread() - error = %d\n", __LINE__,eError);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* create the pipe used to send commands to the thread */
    eError = pipe (pComponentPrivate->lcml_Pipe);
    G722ENC_DPRINT ("%d :: G722Enc_StartCompThread() - error = %d\n", __LINE__,eError);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    gLcmlPipeWr = pComponentPrivate->lcml_Pipe[1];

    /* Create the Component Thread */
    eError = pthread_create (&(pComponentPrivate->ComponentThread), NULL,
                             ComponentThread, pComponentPrivate);
    G722ENC_DPRINT ("%d :: G722Enc_StartCompThread() - error = %d\n", __LINE__,eError);
    if (eError || !pComponentPrivate->ComponentThread) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    G722ENC_DPRINT ("%d :: G722Enc_StartCompThread() - error = %d\n", __LINE__,eError);

    pComponentPrivate_CC = pComponentPrivate;
    pComponentPrivate->bCompThreadStarted = 1;
    G722ENC_DPRINT ("%d :: G722Enc_StartCompThread() - error = %d\n", __LINE__,eError);

 EXIT:
    G722ENC_DPRINT ("%d :: G722Enc_StartCompThread() - error = %d\n", __LINE__,eError);
    return eError;
}


/* ========================================================================== */
/**
 * G722Enc_FreeCompResources() This function is called by the component during
 * de-init to close component thread, Command pipe, data pipe & LCML pipe.
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
OMX_ERRORTYPE G722Enc_FreeCompResources(OMX_HANDLETYPE pComponent)
{
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    G722ENC_COMPONENT_PRIVATE *pComponentPrivate = (G722ENC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE err = OMX_ErrorNone;

    G722ENC_DPRINT ("%d :: G722Enc_FreeCompResources()\n", __LINE__);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pCreatePhaseArgs);
    if (pComponentPrivate->bCompThreadStarted) {
        err = close (pComponentPrivate->dataPipe[0]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            G722ENC_DPRINT ("%d :: Error while closing dataPipe\n",__LINE__);
        }
        err = close (pComponentPrivate->dataPipe[1]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            G722ENC_DPRINT ("%d :: Error while closing dataPipe\n",__LINE__);
        }

        err = close (pComponentPrivate->cmdDataPipe[0]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->cmdDataPipe[1]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->cmdPipe[0]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            G722ENC_DPRINT ("%d :: Error while closing cmdPipe\n",__LINE__);
        }

        err = close (pComponentPrivate->cmdPipe[1]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            G722ENC_DPRINT ("%d :: Error while closing cmdPipe\n",__LINE__);
        }

        err = close (pComponentPrivate->lcml_Pipe[0]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            G722ENC_DPRINT ("%d :: Error while closing lcml_Pipe\n",__LINE__);
        }

        err = close (pComponentPrivate->lcml_Pipe[1]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            G722ENC_DPRINT ("%d :: Error while closing lcml_Pipe\n",__LINE__);
        }
    }

    if (pComponentPrivate->bPortDefsAllocated != 0 /*NULL*/) {
        OMX_MEMFREE_STRUCT(pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pcmParams);
        OMX_MEMFREE_STRUCT(pComponentPrivate->g722Params);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pOutputBufferList);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pInputBufferList);
        OMX_MEMFREE_STRUCT(pComponentPrivate->sDeviceString);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pOutPortFormat);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pInPortFormat);
        OMX_MEMFREE_STRUCT(pComponentPrivate->sPriorityMgmt);
    } 
    pComponentPrivate->bPortDefsAllocated = 0;

    if (pComponentPrivate->bMutexInitDone) {

        pthread_mutex_destroy(&pComponentPrivate->InLoaded_mutex);
        pthread_cond_destroy(&pComponentPrivate->InLoaded_threshold);

        pthread_mutex_destroy(&pComponentPrivate->InIdle_mutex);
        pthread_cond_destroy(&pComponentPrivate->InIdle_threshold);

        pthread_mutex_destroy(&pComponentPrivate->AlloBuf_mutex);
        pthread_cond_destroy(&pComponentPrivate->AlloBuf_threshold);

        pthread_mutex_destroy(&pComponentPrivate->codecStop_mutex);
        pthread_cond_destroy(&pComponentPrivate->codecStop_threshold);
    }
    if (pComponentPrivate->pLcmlHandle != NULL) {
        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle)->pCodecinterfacehandle,
                               EMMCodecControlDestroy, NULL);

        if (eError != OMX_ErrorNone){
            G722ENC_DPRINT("%d : Error: in Destroying the codec: no.  %x\n",__LINE__, eError);
        }
        G722ENC_FreeLCMLHandle();
        pComponentPrivate->pLcmlHandle = NULL;
    }

    G722ENC_DPRINT ("Exiting Successfully G722ENC_FreeCompResources()\n");
    return eError;
}

/* ================================================================================= */
/**
 * @fn G722ENC_CleanupInitParams() description for G722ENC_CleanupInitParams  
 G722ENC_CleanupInitParams().  
 This function is called by the component to cleanup initialization parameters
 upon component exit.
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ================================================================================ */
OMX_ERRORTYPE G722ENC_CleanupInitParams(OMX_HANDLETYPE pComponent)
{
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    G722ENC_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;
    OMX_U16 i = 0;
    G722ENC_COMPONENT_PRIVATE *pComponentPrivate = (G722ENC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;
    
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    G722ENC_DPRINT ("%d :: G722ENC_CleanupInitParams()\n", __LINE__);
    G722ENC_DPRINT ("%d :: pComponentPrivate = %p\n", __LINE__,pComponentPrivate);
    G722ENC_DPRINT ("%d :: pComponentPrivate->strmAttr = %p\n", __LINE__,pComponentPrivate->strmAttr);
    
    if ((pComponentPrivate->dasfmode == 1) && (pComponentPrivate->pParams)) {
        OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->pParams, G722ENC_AudioCodecParams);
    }    
    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[G722ENC_INPUT_PORT];
    for(i=0; i<pComponentPrivate->noInitInputBuf; i++) {
        OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pIpParam, G722ENC_UAlgInBufParamStruct);
        pTemp_lcml++;
    }
    OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[G722ENC_INPUT_PORT]);

    OMX_MEMFREE_STRUCT(pComponentPrivate->strmAttr);
    G722ENC_DPRINT ("%d :: G722ENC_CleanupInitParams()\n", __LINE__);
    G722ENC_DPRINT ("Exiting Successfully G722ENC_CleanupInitParams()\n");
    return eError;
}


/* ========================================================================== */
/**
 * @G722ENC_StopComponentThread() This function is called by the component during
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

OMX_ERRORTYPE G722ENC_StopComponentThread(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE threadError = OMX_ErrorNone;
    int pthreadError = 0;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    G722ENC_COMPONENT_PRIVATE *pComponentPrivate = (G722ENC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;

    pComponentPrivate->bIsStopping = 1;
    pthreadError = pthread_join (pComponentPrivate->ComponentThread,
                                 (void*)&threadError);
                              
  
    pComponentPrivate->bCompThreadStarted = 0;
    if (0 != pthreadError)
    {
        eError = OMX_ErrorHardware;
        G722ENC_DPRINT("OMX_ErrorHardware.\n");
        goto EXIT;
    }
    if (OMX_ErrorNone != threadError && OMX_ErrorNone != eError)
    {
        eError = OMX_ErrorInsufficientResources;
        G722ENC_DPRINT("OMX_ErrorInsufficientResources.\n");
        goto EXIT;
    }
 EXIT:
    G722ENC_DPRINT("Exiting StopComponentThread - Returning = 0x%x\n", eError);
    return eError;
}

/* ========================================================================== */
/**
 * @G722ENC_HandleCommand() This function is called by the component when ever it
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

OMX_U32 G722ENC_HandleCommand (G722ENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_U16 i = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle =
        (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    OMX_COMMANDTYPE command;
    OMX_STATETYPE commandedState = OMX_StateInvalid;
    OMX_U32 commandData = 0;
    LCML_DSP_INTERFACE *pLcmlHandle = (LCML_DSP_INTERFACE *)
        pComponentPrivate->pLcmlHandle;

    G722ENC_DPRINT ("%d :: >>> Entering G722ENC_HandleCommand Function3\n",__LINE__);


    read (pComponentPrivate->cmdPipe[0], &command, sizeof (command));
    read (pComponentPrivate->cmdDataPipe[0], &commandData, sizeof (commandData));

    if (command == OMX_CommandStateSet) {
        commandedState = (OMX_STATETYPE)commandData;
        switch(commandedState) {
        case OMX_StateIdle:
            eError = G722ENC_CommandToIdle(pComponentPrivate);
            break;

        case OMX_StateExecuting:
            eError = G722ENC_CommandToExecuting(pComponentPrivate);
            break;

        case OMX_StateLoaded:
            eError = G722ENC_CommandToLoaded(pComponentPrivate);
            break;

        case OMX_StatePause:
            eError = G722ENC_CommandToPause(pComponentPrivate);
            break;


        case OMX_StateWaitForResources:
            eError = G722ENC_CommandToWaitForResources(pComponentPrivate);
            break;

        case OMX_StateInvalid:
            G722ENC_DPRINT("%d: G722ENC_HandleCommand: Cmd OMX_StateInvalid:\n",__LINE__);
            if (pComponentPrivate->curState == commandedState){
                pComponentPrivate->cbInfo.EventHandler (
                                                        pHandle, pHandle->pApplicationPrivate,
                                                        OMX_EventError, OMX_ErrorSameState,0,
                                                        NULL);
                G722ENC_DPRINT("%d :: Error: Same State Given by \
                           Application\n",__LINE__);
                break;
            }
            else {
                pComponentPrivate->curState = OMX_StateInvalid;
                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventError, OMX_ErrorInvalidState,0, NULL);
            }
            break;

        case OMX_StateMax:
            G722ENC_DPRINT("%d: G722ENC_HandleCommand: Cmd OMX_StateMax::\n",__LINE__);
            break;

        default:
            G722ENC_DPRINT("%d: G722ENC_HandleCommand: Invalid Command: 0x%x\n",__LINE__,commandedState);
            break;
        } /* End of Switch */
    }
    else if (command == OMX_CommandMarkBuffer) {
        G722ENC_DPRINT("command OMX_CommandMarkBuffer received %d\n",__LINE__);
        if(!pComponentPrivate->pMarkBuf){
            G722ENC_DPRINT("command OMX_CommandMarkBuffer received %d\n",__LINE__);
            /* TODO Need to handle multiple marks */
            pComponentPrivate->pMarkBuf = (OMX_MARKTYPE *)(commandData);
        }
    }
    else if (command == OMX_CommandPortDisable) {
        if (!pComponentPrivate->bDisableCommandPending) {
            G722ENC_DPRINT("I'm here Line %d\n",__LINE__);
            if(commandData == 0x0 || (OMX_S32)commandData == -1){
                /* disable port */
                pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bEnabled = OMX_FALSE;
            }
            if(commandData == 0x1 || (OMX_S32)commandData == -1){

                char *pArgs = "damedesuStr";


                pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bEnabled = OMX_FALSE;

                if (pComponentPrivate->curState == OMX_StateExecuting) {
                    pComponentPrivate->bNoIdleOnStop = OMX_TRUE;
                    pComponentPrivate->bIsStopping = 1;
                    eError = LCML_ControlCodec(
                                               ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               MMCodecControlStop,(void *)pArgs);
                    if (OMX_ErrorNone != eError) {
                        G722ENC_FatalErrorRecover(pComponentPrivate);
                        G722ENC_DPRINT("%d :: Error from LCML_ControlCodec EMMCodecControlStop = %x\n",__LINE__,eError);
                        return eError;
                    }
                    omx_mutex_wait(&pComponentPrivate->codecStop_mutex, &pComponentPrivate->codecStop_threshold,
                                   &pComponentPrivate->codecStop_waitingsignal);
                }


            }

        }
        if(commandData == 0x0) {
            if(!pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bPopulated){
                /* return cmdcomplete event if input unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortDisable,G722ENC_INPUT_PORT, NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else{
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

        if(commandData == 0x1) {
            if (!pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bPopulated){
                /* return cmdcomplete event if output unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortDisable,G722ENC_OUTPUT_PORT, NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

        if((OMX_S32)commandData == -1) {
            if (!pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bPopulated && 
                !pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bPopulated){

                /* return cmdcomplete event if inout & output unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortDisable,G722ENC_INPUT_PORT, NULL);

                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortDisable,G722ENC_OUTPUT_PORT, NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }

        }
        
    }
    else if (command == OMX_CommandPortEnable) {
        if(commandData == 0x0 || (OMX_S32)commandData == -1){
            /* enable in port */
            G722ENC_DPRINT("setting input port to enabled\n");
            pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bEnabled = OMX_TRUE;
            G722ENC_DPRINT("WAKE UP!! HandleCommand: En utils setting output port to enabled. \n");
            if(pComponentPrivate->AlloBuf_waitingsignal)
            {
                pComponentPrivate->AlloBuf_waitingsignal = 0;
                pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
                pthread_cond_signal(&pComponentPrivate->AlloBuf_threshold);
                /* Sending signal to Allocate Buffer Task. */ 
                pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);   
            }
            G722ENC_DPRINT("pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bEnabled = %d\n",pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bEnabled);
        }
        if(commandData == 0x1 || (OMX_S32)commandData == -1){
            /* enable out port */
            if(pComponentPrivate->AlloBuf_waitingsignal)
            {
                pComponentPrivate->AlloBuf_waitingsignal = 0;
                pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
                pthread_cond_signal(&pComponentPrivate->AlloBuf_threshold);
                /* Sending signal to Allocate Buffer Task. */ 
                pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);    
            }
            
            if (pComponentPrivate->curState == OMX_StateExecuting) {
                char *pArgs = "damedesuStr";
                pComponentPrivate->bDspStoppedWhileExecuting = OMX_FALSE;
                eError = LCML_ControlCodec(
                                           ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlStart,(void *)pArgs);
            }
            G722ENC_DPRINT("setting output port to enabled\n");
            pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bEnabled = OMX_TRUE;
            G722ENC_DPRINT("pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bEnabled = %d\n",pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bEnabled);
        }

        while (1) {
            G722ENC_DPRINT("commandData = %d\n",commandData);
            G722ENC_DPRINT("pComponentPrivate->curState = %d\n",pComponentPrivate->curState);
            G722ENC_DPRINT("pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bPopulated = %d\n",pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bPopulated);
            if(commandData == 0x0 && (pComponentPrivate->curState == OMX_StateLoaded || pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bPopulated)){

                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortEnable,G722ENC_INPUT_PORT, NULL);
                break;
            }
            else if(commandData == 0x1 && (pComponentPrivate->curState == OMX_StateLoaded || pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bPopulated)){

                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortEnable,G722ENC_OUTPUT_PORT, NULL);
                break;
            }
            else if((OMX_S32)commandData == -1 && (pComponentPrivate->curState == OMX_StateLoaded || (pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bPopulated && pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bPopulated))){

                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortEnable,G722ENC_INPUT_PORT, NULL);

                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, OMX_CommandPortEnable,G722ENC_OUTPUT_PORT, NULL);

                G722ENC_Fill_LCMLInitParamsEx(pHandle);
                break;
            }
        }
    }
    else if (command == OMX_CommandFlush) {
        if(commandData == 0x0 || (OMX_S32)commandData == -1){
            for (i=0; i < G722ENC_MAX_NUM_OF_BUFS; i++) {
                pComponentPrivate->pInputBufHdrPending[i] = NULL;
            }
            pComponentPrivate->nNumInputBufPending=0;
            
            for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
                pComponentPrivate->cbInfo.EmptyBufferDone (
                                                           pComponentPrivate->pHandle,
                                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                                           pComponentPrivate->pInputBufferList->pBufHdr[i]
                                                           );

            }

            /* return all input buffers */
            pComponentPrivate->cbInfo.EventHandler(
                                                   pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, OMX_CommandFlush,G722ENC_INPUT_PORT, NULL);
        }
        if(commandData == 0x1 || (OMX_S32)commandData == -1){
            for (i=0; i < G722ENC_MAX_NUM_OF_BUFS; i++) {
                pComponentPrivate->pOutputBufHdrPending[i] = NULL;
            }
            pComponentPrivate->nNumOutputBufPending=0;

            /* return all output buffers */
            for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
                G722ENC_DPRINT("[FillBufferDone] Call #%d Line %d\n",++FillBufferDoneCount,__LINE__);
                pComponentPrivate->cbInfo.FillBufferDone (
                                                          pComponentPrivate->pHandle,
                                                          pComponentPrivate->pHandle->pApplicationPrivate,
                                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]
                                                          );
            }


            pComponentPrivate->cbInfo.EventHandler(
                                                   pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, OMX_CommandFlush,G722ENC_OUTPUT_PORT, NULL);
        }
    }
    G722ENC_DPRINT ("%d :: Exiting NBAMRDECG722ENC_HandleCommand Function\n",__LINE__);
    G722ENC_DPRINT ("%d :: Returning %x\n",__LINE__,eError);
    return eError;
}


/* ========================================================================== */
/**
 * @G722ENC_HandleDataBuf_FromApp() This function is called by the component when ever it
 * receives the buffer from the application
 *
 * @param pComponentPrivate  Component private data
 * @param pBufHeader Buffer from the application
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ========================================================================== */

OMX_ERRORTYPE G722ENC_HandleDataBuf_FromApp(OMX_BUFFERHEADERTYPE* pBufHeader,
                                            G722ENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_DIRTYPE eDir;
    LCML_DSP_INTERFACE *pLcmlHandle = NULL;
    G722ENC_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;


    G722ENC_DPRINT ("%d :: Entering G722ENC_HandleDataBuf_FromApp Function\n",__LINE__);
    eError = G722ENC_GetBufferDirection(pBufHeader, &eDir, pComponentPrivate);
    if (eError != OMX_ErrorNone) {
        G722ENC_DPRINT ("%d :: The pBufHeader is not found in the list\n",__LINE__);
        goto EXIT;
    }

    if (eDir == OMX_DirInput) {
        if(pComponentPrivate->dasfmode == 0) {
            if ((pBufHeader->nFilledLen > 0) || (pBufHeader->nFlags == OMX_BUFFERFLAG_EOS)) {
                pComponentPrivate->bBypassDSP = 0;
                pLcmlHandle = (LCML_DSP_INTERFACE *) pComponentPrivate->pLcmlHandle;
                G722ENC_DPRINT ("%d Comp:: Sending Filled Input buffer = %p, %p to LCML\n", __LINE__,pBufHeader,pBufHeader->pBuffer);
                eError = G722ENC_GetCorresponding_LCMLHeader(pBufHeader->pBuffer, OMX_DirInput, &pLcmlHdr);
                if (eError != OMX_ErrorNone) {
                    G722ENC_DPRINT("%d :: Error: Invalid Buffer Came ...\n",__LINE__);
                    goto EXIT;
                }

                /* Store time stamp information */ 
                pComponentPrivate->arrTimestamp[pComponentPrivate->IpBufindex] = pBufHeader->nTimeStamp; 
                /* Store nTickCount information */ 
                pComponentPrivate->arrTickCount[pComponentPrivate->IpBufindex] = pBufHeader->nTickCount; 
                pComponentPrivate->IpBufindex++; 
                pComponentPrivate->IpBufindex %= pComponentPrivate->pPortDef[OMX_DirOutput]->nBufferCountActual; 
                             

                if(pBufHeader->nFlags == OMX_BUFFERFLAG_EOS) {
                    pLcmlHdr->pIpParam->bLastBuffer = 1;
                }
                else{
                    pLcmlHdr->pIpParam->bLastBuffer = 0;
                }

                if (pComponentPrivate->curState == OMX_StateExecuting) {
                    if(!pComponentPrivate->bDspStoppedWhileExecuting) {
                        if (!G722ENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirInput)) {
                            G722ENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirInput);
                            eError = LCML_QueueBuffer( pLcmlHandle->pCodecinterfacehandle,
                                                       EMMCodecInputBuffer,
                                                       pBufHeader->pBuffer,
                                                       pBufHeader->nAllocLen,
                                                       pBufHeader->nFilledLen,
                                                       (OMX_U8 *) pLcmlHdr->pIpParam,
                                                       sizeof(G722ENC_UAlgInBufParamStruct),
                                                       pBufHeader->pBuffer);

                            if (eError != OMX_ErrorNone) {
                                G722ENC_DPRINT ("%d ::Comp: SetBuff: IP: Error Occurred\n",__LINE__);
                                eError = OMX_ErrorHardware;
                                G722ENC_FatalErrorRecover(pComponentPrivate);
                                return eError;
                            }
                        }
                    }
                    else{
                        pComponentPrivate->cbInfo.EmptyBufferDone (
                                                                   pComponentPrivate->pHandle,
                                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                                   pBufHeader
                                                                   );
                    }
                    pComponentPrivate->lcml_nIpBuf++;
                    G722ENC_DPRINT ("Sending Input buffer to Codec\n");
                }
                else {
                    G722ENC_DPRINT("Queueing pending input buffers\n");
                    pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pBufHeader;
                }
            }
            else {
                pComponentPrivate->bBypassDSP = 1;
                G722ENC_DPRINT("[FillBufferDone] Call #%d Line %d\n",++FillBufferDoneCount,__LINE__);
                pComponentPrivate->cbInfo.FillBufferDone (
                                                          pComponentPrivate->pHandle,
                                                          pComponentPrivate->pHandle->pApplicationPrivate,
                                                          pComponentPrivate->pOutputBufferList->pBufHdr[0]
                                                          );



                G722ENC_DPRINT ("Forcing EmptyBufferDone\n");
                pComponentPrivate->cbInfo.EmptyBufferDone (
                                                           pComponentPrivate->pHandle,
                                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                                           pComponentPrivate->pInputBufferList->pBufHdr[0]
                                                           );

            }
            if(pBufHeader->nFlags == OMX_BUFFERFLAG_EOS) {
                G722ENC_DPRINT("%d :: Comp: EOF Flag Has Been Set Here\n",__LINE__);
                if(pComponentPrivate->dasfmode == 0) {
                    pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags |= OMX_BUFFERFLAG_EOS;
                    pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle, 
                                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                                           OMX_EventBufferFlag, 
                                                           pComponentPrivate->pOutputBufferList->pBufHdr[0]->nOutputPortIndex,
                                                           pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags, NULL);
                    G722ENC_DPRINT("[FillBufferDone] Call #%d Line %d\n",++FillBufferDoneCount,__LINE__);
                }
                /* pBufHeader->nFlags = 0; */
            }
            if(pBufHeader->pMarkData){
                /* copy mark to output buffer header */ 
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->pMarkData = pBufHeader->pMarkData;
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->hMarkTargetComponent = pBufHeader->hMarkTargetComponent;

                /* trigger event handler if we are supposed to */ 
                if(pBufHeader->hMarkTargetComponent == pComponentPrivate->pHandle && pBufHeader->pMarkData){
                    pComponentPrivate_CC->cbInfo.EventHandler(pComponentPrivate->pHandle, 
                                                              pComponentPrivate->pHandle->pApplicationPrivate, 
                                                              OMX_EventMark, 
                                                              0, 
                                                              0, 
                                                              pBufHeader->pMarkData);
                }
            }
        }
    } 
    else if (eDir == OMX_DirOutput) {
        G722ENC_DPRINT ("%d Comp:: Sending Emptied Output buffer=%p to LCML\n",
                        __LINE__,pBufHeader);
        G722ENC_DPRINT("%d : pComponentPrivate->lcml_nOpBuf = %ld\n",__LINE__,
                       pComponentPrivate->lcml_nOpBuf);
        G722ENC_DPRINT("%d : pComponentPrivate->lcml_nIpBuf = %ld\n",__LINE__,
                       pComponentPrivate->lcml_nIpBuf);
        pLcmlHandle = (LCML_DSP_INTERFACE *) pComponentPrivate->pLcmlHandle;

        eError = G722ENC_GetCorresponding_LCMLHeader(pBufHeader->pBuffer, OMX_DirOutput, &pLcmlHdr);
    
        if (eError != OMX_ErrorNone) {
            G722ENC_DPRINT("%d :: Error: Invalid Buffer Came ...\n",__LINE__);
            goto EXIT;
        }
        
        if (!(pComponentPrivate->bIsStopping)) {
            if (pComponentPrivate->bBypassDSP == 0) {
                if (pComponentPrivate->curState == OMX_StateExecuting) {
                    if (!G722ENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirOutput)) {
                        G722ENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirOutput);
                        eError = LCML_QueueBuffer(
                                                  pLcmlHandle->pCodecinterfacehandle,
                                                  EMMCodecOuputBuffer,
                                                  pBufHeader->pBuffer,
                                                  pBufHeader->nAllocLen,
                                                  0,
                                                  NULL,
                                                  0,
                                                  pBufHeader->pBuffer);


                        if (eError != OMX_ErrorNone ) {
                            G722ENC_DPRINT ("%d :: Comp:: SetBuff OP: Error Occurred\n",__LINE__);
                            eError = OMX_ErrorHardware;
                            G722ENC_FatalErrorRecover(pComponentPrivate);
                            return eError;
                        }
                        pComponentPrivate->lcml_nOpBuf++;
                    }
                }
                else {
                    pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = pBufHeader;
                }
            }
        }
        else {
            if (pComponentPrivate->curState == OMX_StateExecuting) {
                if (!G722ENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirOutput)) {
                    G722ENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirOutput);
                    eError = LCML_QueueBuffer(
                                              pLcmlHandle->pCodecinterfacehandle,
                                              EMMCodecOuputBuffer,
                                              pBufHeader->pBuffer,
                                              pBufHeader->nAllocLen,
                                              0,
                                              NULL,
                                              0,
                                              pBufHeader->pBuffer);


                    if (eError != OMX_ErrorNone ) {
                        G722ENC_DPRINT ("%d :: Comp:: SetBuff: OP: Error Occurred\n",__LINE__);
                        eError = OMX_ErrorHardware;
                        G722ENC_FatalErrorRecover(pComponentPrivate);
                        return eError;
                    }
        
                    pComponentPrivate->lcml_nOpBuf++;
                }
            }
            else {
                G722ENC_DPRINT("Queueing output buffer\n");
                pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = pBufHeader;
            }
        }
    } 
    else {
        G722ENC_DPRINT("%d : BufferHeader %p, Buffer %p Unknown ..........\n",
                       __LINE__,pBufHeader, pBufHeader->pBuffer);
        eError = OMX_ErrorBadParameter;
    }
 EXIT:
    G722ENC_DPRINT("%d : Exiting from  G722ENC_HandleDataBuf_FromApp \n",__LINE__);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
 * G722ENC_GetBufferDirection () This function is used by the component thread to
 * request a buffer from the application.  Since it was called from 2 places,
 * it made sense to turn this into a small function.
 *
 * @param pData pointer to G722 Encoder Context Structure
 * @param pCur pointer to the buffer to be requested to be filled
 *
 * @retval none
 **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE G722ENC_GetBufferDirection(OMX_BUFFERHEADERTYPE *pBufHeader,
                                         OMX_DIRTYPE *eDir,
                                         G722ENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_BUFFERHEADERTYPE *pBuf = NULL;
    OMX_U16 flag = 1,i = 0;
    OMX_U32 nBuf = 0;


    nBuf = pComponentPrivate->pInputBufferList->numBuffers;
    for(i=0; i<nBuf; i++) {
        pBuf = pComponentPrivate->pInputBufferList->pBufHdr[i];
        if(pBufHeader == pBuf) {
            *eDir = OMX_DirInput;
            G722ENC_DPRINT ("%d :: Buffer %p is INPUT BUFFER\n",__LINE__, pBufHeader);
            flag = 0;
            goto EXIT;
        }
    }
    /*Search this buffer in input buffers list */
    nBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    for(i=0; i<nBuf; i++) {
        pBuf = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        if(pBufHeader == pBuf) {
            *eDir = OMX_DirOutput;
            G722ENC_DPRINT ("%d :: Buffer %p is OUTPUT BUFFER\n",__LINE__, pBufHeader);
            flag = 0;
            goto EXIT;
        }
    }
    if (flag == 1) {
        G722ENC_DPRINT ("%d :: Buffer %p is Not Found in the List\n",__LINE__,pBufHeader);
        eError = OMX_ErrorUndefined;
        goto EXIT;
    }
 EXIT:
    G722ENC_DPRINT ("%d :: Exiting G722ENC_GetBufferDirection Function\n",__LINE__);
    return eError;
}

/* -------------------------------------------------------------------*/
/**
 *  Callback() function will be called LCML component to write the msg
 *
 * @param msgBuffer                 This buffer will be returned by the LCML
 *
 * @retval OMX_NoError              Success, ready to roll
 *         OMX_Error_BadParameter   The input parameter pointer is null
 **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE G722ENC_LCML_Callback (TUsnCodecEvent event,void * args [10])
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U8 *pBuffer = args[1];
    G722ENC_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;
    OMX_COMPONENTTYPE *pHandle = pComponentPrivate_CC->pHandle;
    
    LCML_DSP_INTERFACE *pLcmlHandle = NULL;

    switch(event) {
        
    case EMMCodecDspError:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecDspError\n");
        break;

    case EMMCodecInternalError:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecInternalError\n");
        break;

    case EMMCodecInitError:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecInitError\n");
        break;

    case EMMCodecDspMessageRecieved:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecDspMessageRecieved\n");
        break;

    case EMMCodecBufferProcessed:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecBufferProcessed\n");
        break;

    case EMMCodecProcessingStarted:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingStarted\n");
        break;
            
    case EMMCodecProcessingPaused:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingPaused\n");
        break;

    case EMMCodecProcessingStoped:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingStoped\n");
        break;

    case EMMCodecProcessingEof:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecProcessingEof\n");
        break;

    case EMMCodecBufferNotProcessed:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecBufferNotProcessed\n");
        break;

    case EMMCodecAlgCtrlAck:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecAlgCtrlAck\n");
        break;

    case EMMCodecStrmCtrlAck:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  EMMCodecStrmCtrlAck\n");
        break;

    default:
        G722ENC_DPRINT("[LCML CALLBACK EVENT]  Got event = %d\n",event);
        break;
    }


    G722ENC_DPRINT(" %d EMMCodecInputBuffer = %p \n",__LINE__,(void *)EMMCodecInputBuffer);
    G722ENC_DPRINT(" %d EMMCodecOuputBuffer = %p \n",__LINE__,(void *)EMMCodecOuputBuffer);
    G722ENC_DPRINT ("%d :: Entering the G722ENC_LCML_Callback() : event = %d\n",__LINE__,event);
    if(event == EMMCodecBufferProcessed) {
        if( args[0] == (void *)EMMCodecInputBuffer) {
            G722ENC_DPRINT (" :: Inside the G722ENC_LCML_Callback EMMCodecInputBuffer\n");
            G722ENC_DPRINT("%d :: Input: pBufferr = %p\n",__LINE__, pBuffer);
            eError = G722ENC_GetCorresponding_LCMLHeader(pBuffer, OMX_DirInput, &pLcmlHdr);
            G722ENC_ClearPending(pComponentPrivate_CC,pLcmlHdr->pBufHdr,OMX_DirInput);

            if (eError != OMX_ErrorNone) {
                G722ENC_DPRINT("%d :: Error: Invalid Buffer Came ...\n",__LINE__);
                goto EXIT;
            }
            G722ENC_DPRINT("%d :: Input: pLcmlHeader = %p\n",__LINE__, pLcmlHdr);
            /* >>>>>>>>>>>>>>>>>>>>>>>>>>> */
            if(pComponentPrivate_CC->dasfmode == 0) {
                pComponentPrivate_CC->lcml_nIpBuf--;          
                G722ENC_DPRINT("%d: Component Sending Empty Input buffer%p to App\n",__LINE__, pLcmlHdr->pBufHdr->pBuffer);
                pComponentPrivate_CC->cbInfo.EmptyBufferDone (
                                                              pHandle,
                                                              pHandle->pApplicationPrivate,
                                                              pLcmlHdr->pBufHdr
                                                              );
                pComponentPrivate_CC->app_nBuf++;
               
            }
            /* <<<<<<<<<<<<<<<<<<<<<<<<<<< */            
        } 
        else if (args[0] == (void *)EMMCodecOuputBuffer) {
            if (!G722ENC_IsValid(pComponentPrivate_CC,pBuffer,OMX_DirOutput)) {
                /* If the buffer we get back from the DSP is not valid call FillBufferDone
                   on a valid buffer */
                pComponentPrivate_CC->cbInfo.FillBufferDone (
                                                             pComponentPrivate_CC->pHandle,
                                                             pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                             pComponentPrivate_CC->pOutputBufferList->pBufHdr[pComponentPrivate_CC->nInvalidFrameCount++]
                                                             );
            }
            else {

                pComponentPrivate_CC->nOutStandingFillDones++;
                G722ENC_DPRINT("Incrementing pComponentPrivate->nOutStandingFillDones Line %d\n",__LINE__);
                G722ENC_DPRINT("%d::pComponentPrivate_CC->nOutStandingFillDones = %d\n",__LINE__,pComponentPrivate_CC->nOutStandingFillDones);
                eError = G722ENC_GetCorresponding_LCMLHeader(pBuffer, OMX_DirOutput, &pLcmlHdr);
                G722ENC_ClearPending(pComponentPrivate_CC,pLcmlHdr->pBufHdr,OMX_DirOutput);

                if (eError != OMX_ErrorNone) {
                    G722ENC_DPRINT("%d :: Error: Invalid Buffer Came ...\n",__LINE__);
                    goto EXIT;
                }
                pLcmlHdr->pBufHdr->nFilledLen = (int)args[8];
                G722ENC_DPRINT("%d :: Output: pLcmlHeader = %p\n",__LINE__, pLcmlHdr);

                pComponentPrivate_CC->lcml_nCntOpReceived++;
                /* >>>>>>>>>>>>>>>>>>>>>>> */
                pComponentPrivate_CC->lcml_nOpBuf--;
                G722ENC_DPRINT("%d: Component Sending Filled Output buffer%p to App\n",__LINE__,pLcmlHdr->pBufHdr->pBuffer);
                G722ENC_DPRINT("[FillBufferDone] Call #%d Line %d\n",++FillBufferDoneCount,__LINE__);
                pComponentPrivate_CC->cbInfo.FillBufferDone (
                                                             pHandle,
                                                             pHandle->pApplicationPrivate,
                                                             pLcmlHdr->pBufHdr
                                                             );
                pComponentPrivate_CC->app_nBuf++;
                pComponentPrivate_CC->nOutStandingFillDones--;
                G722ENC_DPRINT("Decrementing pComponentPrivate->nOutStandingFillDones Line %d\n",__LINE__);
                G722ENC_DPRINT("%d::pComponentPrivate->nOutStandingFillDones = %d\n",__LINE__,pComponentPrivate_CC->nOutStandingFillDones);
                /* <<<<<<<<<<<<<<<<<<<<<<< */                                
            }
        }
    }
    else if(event == EMMCodecProcessingStoped) {

        omx_mutex_signal(&pComponentPrivate_CC->codecStop_mutex,&pComponentPrivate_CC->codecStop_threshold,
                         &pComponentPrivate_CC->codecStop_waitingsignal);

        if (!pComponentPrivate_CC->bNoIdleOnStop) {
            pComponentPrivate_CC->bIdleCommandPending = OMX_TRUE;
            pComponentPrivate_CC->curState = OMX_StateIdle;

#ifdef RESOURCE_MANAGER_ENABLED
            eError = RMProxy_NewSendCommand(pComponentPrivate_CC->pHandle,
                                            RMProxy_StateSet,
                                            OMX_G722_Encoder_COMPONENT,
                                            OMX_StateIdle,
                                            1234,
                                            NULL);
#endif

            if (pComponentPrivate_CC->bPreempted == 0) {
                pComponentPrivate_CC->cbInfo.EventHandler(pHandle,
                                                          pHandle->pApplicationPrivate,
                                                          OMX_EventCmdComplete,
                                                          OMX_CommandStateSet,
                                                          pComponentPrivate_CC->curState,
                                                          NULL);
            }
            else {
                pComponentPrivate_CC->cbInfo.EventHandler(pHandle,
                                                          pHandle->pApplicationPrivate,
                                                          OMX_EventError,
                                                          OMX_ErrorResourcesPreempted,
                                                          0,
                                                          NULL);
            }
            pComponentPrivate_CC->bIdleCommandPending = OMX_FALSE;
        }
        else {
            G722ENC_DPRINT("setting bDspStoppedWhileExecuting\n");
            pComponentPrivate_CC->bDspStoppedWhileExecuting = OMX_TRUE;
        }
        pComponentPrivate_CC->bNoIdleOnStop= OMX_FALSE;
    }
    else if(event == EMMCodecAlgCtrlAck) {
        G722ENC_DPRINT ("GOT MESSAGE USN_DSPACK_ALGCTRL \n");
    }
    else if (event == EMMCodecDspError) {
        if(((int)args[4] == 1) && ((int)args[5] == 0x500)) {
            G722ENC_DPRINT ("%d :: Comp: Inside the G722ENC_LCML_Callback: USN_DSPMSG_ERROR \n", __LINE__);
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate_CC->pLcmlHandle;
            G722ENC_DPRINT("Setting current state Idle Line %d\n",__LINE__);
#ifdef RESOURCE_MANAGER_ENABLED
            eError = RMProxy_NewSendCommand(pComponentPrivate_CC->pHandle,
                                            RMProxy_StateSet,
                                            OMX_G722_Encoder_COMPONENT,
                                            OMX_StateIdle,
                                            1234,
                                            NULL);
#endif
            pComponentPrivate_CC->curState = OMX_StateIdle;

            pComponentPrivate_CC->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                      OMX_EventCmdComplete, OMX_CommandStateSet,
                                                      pComponentPrivate_CC->curState, NULL);
            G722ENC_DPRINT("%d :: G722ENC: State has been Set to Idle\n", __LINE__);            
        }
        if(((int)args[4] == 1) && ((int)args[5] == 0x0300)) {
            G722ENC_DPRINT ("%d :: Comp: Inside the G722ENC_LCML_Callback: USN_DSPMSG_ERROR \n", __LINE__);
            pComponentPrivate_CC->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                      OMX_EventError, OMX_ErrorOverflow,
                                                      pComponentPrivate_CC->curState, NULL);
                

        } 
        if(((int)args[4] == USN_ERR_WARNING) && 
           ((int)args[5] == IUALG_WARN_PLAYCOMPLETED)) {
            pComponentPrivate_CC->cbInfo.EventHandler(pHandle,                  
                                                      pHandle->pApplicationPrivate,
                                                      OMX_EventBufferFlag,
                                                      (OMX_U32)NULL,
                                                      OMX_BUFFERFLAG_EOS,
                                                      NULL);
        }
        if(((OMX_U32)args[4] == USN_ERR_NONE) && (args[5] == (void*)NULL)){
                OMXDBG_PRINT(stderr, ERROR, 4, 0, "%d :: UTIL: MMU_Fault \n",__LINE__);
                G722ENC_FatalErrorRecover(pComponentPrivate_CC);
        }
    }
    else if (event == EMMCodecStrmCtrlAck) {
        G722ENC_DPRINT("%d :: GOT MESSAGE USN_DSPACK_STRMCTRL ----\n",__LINE__);
        pComponentPrivate_CC->bStreamCtrlCalled = 1;
    
    }
    else if (event == EMMCodecProcessingPaused) {
#ifdef RESOURCE_MANAGER_ENABLED
        eError = RMProxy_NewSendCommand(pComponentPrivate_CC->pHandle,
                                        RMProxy_StateSet,
                                        OMX_G722_Encoder_COMPONENT,
                                        OMX_StatePause,
                                        1234,
                                        NULL);
#endif

        pComponentPrivate_CC->curState = OMX_StatePause;
        pComponentPrivate_CC->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                  OMX_EventCmdComplete, OMX_CommandStateSet,
                                                  pComponentPrivate_CC->curState, NULL);

    }
 EXIT:
    G722ENC_DPRINT ("%d :: Exiting the G722ENC_LCML_Callback() \n",__LINE__);
    return eError;
}
/* -------------------------------------------------------------------*/
/**
 *  G722ENC_GetCorresponding_LCMLHeader() function will be called by G722ENC_LCML_Callback
 *                                 component to write the msg
 * @param *pBuffer,          Event which gives to details about USN status
 * @param LCML_NBAMRENC_BUFHEADERTYPE **ppLcmlHdr          

 * @retval OMX_NoError              Success, ready to roll
 *         OMX_Error_BadParameter   The input parameter pointer is null
 **/
/* -------------------------------------------------------------------*/

OMX_ERRORTYPE G722ENC_GetCorresponding_LCMLHeader(OMX_U8 *pBuffer,
                                                  OMX_DIRTYPE eDir,
                                                  G722ENC_LCML_BUFHEADERTYPE **ppLcmlHdr)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    G722ENC_LCML_BUFHEADERTYPE *pLcmlBufHeader = NULL;
    int nIpBuf = pComponentPrivate_CC->pInputBufferList->numBuffers;
    int nOpBuf = pComponentPrivate_CC->pOutputBufferList->numBuffers;
    OMX_U16 i = 0;

    G722ENC_DPRINT ("%d :: Entering the G722ENC_GetCorresponding_LCMLHeader()\n",__LINE__);
    if(eDir == OMX_DirInput) {
        pLcmlBufHeader = pComponentPrivate_CC->pLcmlBufHeader[G722ENC_INPUT_PORT];
        for(i=0; i<nIpBuf; i++) {
            if(pBuffer == pLcmlBufHeader->pBufHdr->pBuffer) {
                *ppLcmlHdr = pLcmlBufHeader;
                G722ENC_DPRINT("%d::Corresponding LCML Header Found\n",__LINE__);
                goto EXIT;
            }
            pLcmlBufHeader++;
        }
    } 
    else if (eDir == OMX_DirOutput) {
        pLcmlBufHeader = pComponentPrivate_CC->pLcmlBufHeader[G722ENC_OUTPUT_PORT];
        for(i=0; i<nOpBuf; i++) {
            if(pBuffer == pLcmlBufHeader->pBufHdr->pBuffer) {
                *ppLcmlHdr = pLcmlBufHeader;
                G722ENC_DPRINT("%d::Corresponding LCML Header Found\n",__LINE__);
                goto EXIT;
            }
            pLcmlBufHeader++;
        }
    } 
    else {
        G722ENC_DPRINT("%d:: Invalid Buffer Type :: exiting...\n",__LINE__);
    }

 EXIT:
    G722ENC_DPRINT ("%d :: Exiting the G722ENC_GetCorresponding_LCMLHeader() \n",__LINE__);
    return eError;
}


/* -------------------------------------------------------------------*/
/**
 *  G722ENC_GetLCMLHandle() 
 * 
 * @retval OMX_HANDLETYPE
 *
 -------------------------------------------------------------------*/

OMX_HANDLETYPE G722ENC_GetLCMLHandle()
{
    void *handle = NULL;
    OMX_ERRORTYPE (*fpGetHandle)(OMX_HANDLETYPE);
    OMX_HANDLETYPE pHandle = NULL;
    const char *error = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    handle = dlopen("libLCML.so", RTLD_LAZY);
    if (!handle) {
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
        }
        return pHandle;
    }

    fpGetHandle = dlsym (handle, "GetHandle");
    if ((error = dlerror()) != NULL) {
        fputs(error, stderr);
        dlclose(handle);
        return pHandle;
    }

    if (NULL != fpGetHandle) {
        eError = (*fpGetHandle)(&pHandle);

        if(eError != OMX_ErrorNone) {
            eError = OMX_ErrorUndefined;
            G722ENC_DPRINT("eError != OMX_ErrorNone...\n");
            pHandle = NULL;
            dlclose(handle);
            return pHandle;
        }

    }

    pComponentPrivate_CC->lcml_handle = handle;
    pComponentPrivate_CC->bLcmlHandleOpened = 1;

    return pHandle;
}

OMX_ERRORTYPE G722ENC_FreeLCMLHandle()
{

    int retValue = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    if (pComponentPrivate_CC->bLcmlHandleOpened) {
        retValue = dlclose(pComponentPrivate_CC->lcml_handle);

        if (retValue != 0) {
            eError = OMX_ErrorUndefined;
        }
        pComponentPrivate_CC->bLcmlHandleOpened = 0;
    }
    return eError;
}

/* ================================================================================= */
/**
 * @fn G722ENC_CommandToIdle() description for G722ENC_CommandToIdle  
 G722ENC_CommandToIdle().  
 This component is called by HandleCommand() when the component is commanded to Idle
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ================================================================================ */
OMX_ERRORTYPE G722ENC_CommandToIdle(G722ENC_COMPONENT_PRIVATE *pComponentPrivate)
{

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    char *pArgs = "damedesuStr";
    OMX_COMPONENTTYPE *pHandle =
        (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    LCML_CALLBACKTYPE cb;
    LCML_DSP *pLcmlDsp =  NULL;
    char *p = "damedesuStr";
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;
#ifdef RESOURCE_MANAGER_ENABLED
    OMX_ERRORTYPE rm_error = OMX_ErrorNone;
#endif

#ifdef G722ENC_DEBUG
    LCML_CODEC_INTERFACE *pp = NULL;
#endif



    G722ENC_DPRINT("%d: G722ENC_HandleCommand: Cmd Idle \n",__LINE__);
    if (pComponentPrivate->curState == OMX_StateIdle){
        pComponentPrivate->cbInfo.EventHandler (
                                                pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorSameState,0,
                                                NULL);
        G722ENC_DPRINT("%d :: Error: Same State Given by Application\n",__LINE__);
    }
    else if (pComponentPrivate->curState == OMX_StateLoaded || 
             pComponentPrivate->curState == OMX_StateWaitForResources) {
        if (pComponentPrivate->dasfmode == 1)
        {
            if(pComponentPrivate->streamID == 0)
            { 
                eError = OMX_ErrorInsufficientResources; 
                G722ENC_DPRINT("OMX_ErrorInsufficientResources.\n");
                pComponentPrivate->curState = OMX_StateInvalid; 
                pComponentPrivate->cbInfo.EventHandler( 
                                                       pHandle, pHandle->pApplicationPrivate, 
                                                       OMX_EventError, OMX_ErrorInvalidState,0, NULL);
                goto EXIT; 
            }
        }
        if (pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->bEnabled == OMX_TRUE
            && pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bEnabled == OMX_TRUE) 
        {
            if (!(pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bPopulated)
                && !(pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->bPopulated))
            {
                pComponentPrivate->InLoaded_readytoidle = 1;
                pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex); 
                pthread_cond_wait(&pComponentPrivate->InLoaded_threshold,
                                  &pComponentPrivate->InLoaded_mutex);
                pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
            }
        }

        pLcmlHandle = (OMX_HANDLETYPE) G722ENC_GetLCMLHandle();
        
        if (pLcmlHandle == NULL)
        {
            G722ENC_DPRINT("LCML Handle is NULL........exiting..\n");
            goto EXIT;
        }

        /* Got handle of dsp via phandle filling information about DSP Specific things */
        pLcmlDsp = (((LCML_DSP_INTERFACE*)pLcmlHandle)->dspCodec);

        eError = G722ENC_Fill_LCMLInitParams(pHandle, pLcmlDsp);
        if(eError != OMX_ErrorNone)
        {
            G722ENC_DPRINT("from G722ENCFill_LCMLInitParams().\n");
            goto EXIT;
        }

#ifdef __PERF_INSTRUMENTATION__
        PERF_Boundary(pComponentPrivate->pPERFcomp,
                      PERF_BoundaryComplete | PERF_BoundarySetup);
#endif

#ifdef RESOURCE_MANAGER_ENABLED
         /* Need check the resource with RM */
         pComponentPrivate->rmproxyCallback.RMPROXY_Callback =
                      (void *) G722ENC_ResourceManagerCallback;
         if (pComponentPrivate->curState != OMX_StateWaitForResources) {
             rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_RequestResource,
                                               OMX_G722_Encoder_COMPONENT,
                                               G722ENC_CPU,
                                               1234,
                                               &(pComponentPrivate->rmproxyCallback));
             if(rm_error == OMX_ErrorInsufficientResources) {
                 /* resource is not available, need set state to OMX_StateWaitForResources */
                 pComponentPrivate->curState = OMX_StateWaitForResources;
                 pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                         pHandle->pApplicationPrivate,
                                                         OMX_EventCmdComplete,
                                                         OMX_CommandStateSet,
                                                         pComponentPrivate->curState,
                                                         NULL);
                  G722ENC_DPRINT("%d :: Comp: OMX_ErrorInsufficientResources\n", __LINE__);
                  return rm_error;
             }
         }
#endif

        pComponentPrivate->pLcmlHandle = (LCML_DSP_INTERFACE *)pLcmlHandle;
        cb.LCML_Callback = (void *) G722ENC_LCML_Callback;

        eError = LCML_InitMMCodecEx(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                    p, &pLcmlHandle, (void *)p,
                                    &cb, (OMX_STRING)pComponentPrivate->sDeviceString);

        if(eError != OMX_ErrorNone)
        {
            G722ENC_DPRINT("from LCML_Init().\n");
            G722ENC_FatalErrorRecover(pComponentPrivate);
            goto EXIT;
        }

#ifdef RESOURCE_MANAGER_ENABLED
        if (rm_error == OMX_ErrorNone){
            rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet,
                                              OMX_G722_Encoder_COMPONENT,
                                              OMX_StateIdle, 1234, NULL);
        }
        else {
            eError = OMX_ErrorInsufficientResources;
            pComponentPrivate->curState = OMX_StateInvalid;
            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                   pHandle->pApplicationPrivate,
                                                   OMX_EventError,
                                                   eError,
                                                   OMX_TI_ErrorSevere,
                                                   NULL);
        }

#endif

#ifdef HASHINGENABLE
        /* Enable the Hashing Code */
        eError = LCML_SetHashingState(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle, OMX_TRUE);
        if (eError != OMX_ErrorNone) {
            G722ENC_DPRINT("Failed to set Mapping State\n");
            goto EXIT;
        }
#endif

        pComponentPrivate->curState = OMX_StateIdle;

#ifdef __PERF_INSTRUMENTATION__
        PERF_Boundary(pComponentPrivate->pPERFcomp,
                      PERF_BoundaryComplete | PERF_BoundarySetup);
#endif
        pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                pHandle->pApplicationPrivate,
                                                OMX_EventCmdComplete,
                                                OMX_CommandStateSet,
                                                pComponentPrivate->curState,
                                                NULL);

    } 
    else if (pComponentPrivate->curState == OMX_StateExecuting) {
#ifdef HASHINGENABLE
        /*Hashing Change*/
        pLcmlHandle = (LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle;
        /* clear out any mappings that might have accumulated */
        eError = LCML_FlushHashes(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle);
        if (eError != OMX_ErrorNone) {
            prinft("Error occurred in Codec mapping flush!\n");
            goto EXIT;
        }
#endif

        G722ENC_DPRINT("%d :: In G722ENC_HandleCommand: Stopping the codec\n",__LINE__);
        G722ENC_DPRINT("%d: G722ENCUTILS::About to call LCML_ControlCodec\n",__LINE__);
        G722ENC_DPRINT("%d: Calling Codec Stop..\n");
        pComponentPrivate->bIsStopping = 1;

        eError = LCML_ControlCodec(
                                   ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                   MMCodecControlStop,(void *)pArgs);
        G722ENC_DPRINT("Called Codec Stop..\n");
        if (OMX_ErrorNone != eError) {
            G722ENC_FatalErrorRecover(pComponentPrivate);
            G722ENC_DPRINT("%d :: Error from LCML_ControlCodec EMMCodecControlStop = %x\n",__LINE__,eError);
            return eError;
        }

        omx_mutex_wait(&pComponentPrivate->codecStop_mutex, &pComponentPrivate->codecStop_threshold,
                       &pComponentPrivate->codecStop_waitingsignal);

        OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->pParams, G722ENC_AudioCodecParams);

        G722ENC_DPRINT ("%d :: The component is stopped\n",__LINE__);
        /*      pComponentPrivate->bIdleCommandPending = 1;*/
    } 
    else if(pComponentPrivate->curState == OMX_StatePause) {
#ifdef HASHINGENABLE
        /*Hashing Change*/
        pLcmlHandle = (LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle;
        /* clear out any mappings that might have accumulated */
        eError = LCML_FlushHashes(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle);
        if (eError != OMX_ErrorNone) {
            G722ENC_DPRINT("Error occurred in Codec mapping flush!\n");
            goto EXIT;
        }
#endif

        G722ENC_DPRINT("%d :: Comp: Stop Command Received\n",__LINE__);
        G722ENC_DPRINT("%d: G722ENCUTILS::About to call LCML_ControlCodec\n",__LINE__);
        pComponentPrivate->curState = OMX_StateIdle;
        G722ENC_DPRINT ("%d :: The component is stopped\n",__LINE__);
#ifdef RESOURCE_MANAGER_ENABLED
        rm_error = RMProxy_NewSendCommand(pHandle,
                                          RMProxy_StateSet,
                                          OMX_G722_Encoder_COMPONENT,
                                          OMX_StateIdle, 1234, NULL);
#endif

        pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                               OMX_EventCmdComplete, OMX_CommandStateSet, pComponentPrivate->curState, NULL);
    
    } 
    else {
        G722ENC_DPRINT("%d: Comp: Sending ErrorNotification: Invalid State\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler(
                                               pHandle,
                                               pHandle->pApplicationPrivate,
                                               OMX_EventError,
                                               OMX_ErrorIncorrectStateTransition, 0,
                                               "Invalid State Error");
    }
 EXIT:
    return eError;
}

/* ================================================================================= */
/**
 * @fn G722ENC_CommandToLoaded() description for G722ENC_CommandToLoaded  
 G722ENC_CommandToLoaded().  
 This component is called by HandleCommand() when the component is commanded to Loaded
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ================================================================================ */
OMX_ERRORTYPE G722ENC_CommandToLoaded(G722ENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    char *pArgs = "damedesuStr";
    OMX_COMPONENTTYPE *pHandle =
        (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;

    G722ENC_DPRINT("%d: G722ENC_HandleCommand: Cmd Loaded\n",__LINE__);
    if (pComponentPrivate->curState == OMX_StateLoaded){
        pComponentPrivate->cbInfo.EventHandler (
                                                pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorSameState,0,
                                                NULL);
        G722ENC_DPRINT("%d :: Error: Same State Given by Application\n",__LINE__);
        goto EXIT;
    }
    if (pComponentPrivate->curState == OMX_StateWaitForResources ) {
        pComponentPrivate->curState = OMX_StateLoaded;
        pComponentPrivate->cbInfo.EventHandler (
                                                pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventCmdComplete, OMX_CommandStateSet,pComponentPrivate->curState,
                                                NULL);
        G722ENC_DPRINT("%d :: Transitioning from WaitFor to Loaded\n",__LINE__);
        goto EXIT;
    }

    if (pComponentPrivate->curState != OMX_StateIdle) {
        pComponentPrivate->cbInfo.EventHandler (
                                                pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorIncorrectStateTransition, 0,
                                                "Invalid State");
        G722ENC_DPRINT("%d :: Error: Invalid State Given by Application\n",__LINE__);
        goto EXIT;
    }

    G722ENC_DPRINT("%d: G722ENCUTILS::About to call LCML_ControlCodec\n",__LINE__);
    G722ENC_DPRINT("%d: G722ENCUTILS::Current State = %d\n",__LINE__,pComponentPrivate->curState);
    G722ENC_DPRINT("pComponentPrivate->pInputBufferList->numBuffers = %d\n",pComponentPrivate->pInputBufferList->numBuffers);
    G722ENC_DPRINT("pComponentPrivate->pOutputBufferList->numBuffers = %d\n",pComponentPrivate->pOutputBufferList->numBuffers);
 
    if (pComponentPrivate->pInputBufferList->numBuffers ||
        pComponentPrivate->pOutputBufferList->numBuffers){
        pComponentPrivate->InIdle_goingtoloaded = 1;
        pthread_mutex_lock(&pComponentPrivate->InIdle_mutex); 
        pthread_cond_wait(&pComponentPrivate->InIdle_threshold, &pComponentPrivate->InIdle_mutex);
        pthread_mutex_unlock(&pComponentPrivate->InIdle_mutex);
        pComponentPrivate->bLoadedCommandPending = OMX_FALSE;
    }

    pComponentPrivate->curState = OMX_StateLoaded;
    if (pLcmlHandle != NULL) {
        G722ENC_DPRINT("About to destroy codec\n");
        eError = LCML_ControlCodec(
                               ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                               EMMCodecControlDestroy,(void *)pArgs);
        G722ENC_DPRINT("Finished destroying codec\n");
        G722ENC_FreeLCMLHandle();
        pComponentPrivate->pLcmlHandle = NULL;
        pLcmlHandle = NULL;
    }

    pComponentPrivate->bStreamCtrlCalled = 0;

    if (eError != OMX_ErrorNone) {
        G722ENC_DPRINT("%d : Error: in Destroying the codec\n",__LINE__);

        goto EXIT;
    }

    OMX_MEMFREE_STRUCT(pComponentPrivate->pCreatePhaseArgs);
    G722ENC_DPRINT("freeing pComponentPrivate->pLcmlBufHeader[G722ENC_OUTPUT_PORT] = 0x%x\n",pComponentPrivate->pLcmlBufHeader[G722ENC_OUTPUT_PORT]);
    G722ENC_DPRINT("pComponentPrivate->pLcmlBufHeader[G722ENC_OUTPUT_PORT] = %p\n",pComponentPrivate->pLcmlBufHeader[G722ENC_OUTPUT_PORT]);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[G722ENC_OUTPUT_PORT]);
    OMX_MEMFREE_STRUCT(pComponentPrivate->strmAttr);

    eError = EXIT_COMPONENT_THRD;
    pComponentPrivate->bInitParamsInitialized = 0;
    
 EXIT:
    return eError;
}

/* ================================================================================= */
/**
 * @fn G722ENC_CommandToExecuting() description for G722ENC_CommandToExecuting  
 G722ENC_CommandToExecuting().  
 This component is called by HandleCommand() when the component is commanded to Executing
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ================================================================================ */
OMX_ERRORTYPE G722ENC_CommandToExecuting(G722ENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_U16 i = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    char *pArgs = "damedesuStr";
    OMX_BUFFERHEADERTYPE *pBufHdr = NULL;
    OMX_U32 nBuf = 0;
    OMX_COMPONENTTYPE *pHandle =
        (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;
    G722ENC_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;
#ifdef RESOURCE_MANAGER_ENABLED
    OMX_ERRORTYPE rm_error = OMX_ErrorNone;
#endif

    G722ENC_DPRINT("%d: G722ENC_HandleCommand: Cmd Executing \n",__LINE__);
    pComponentPrivate->bBypassDSP = 0;
    if (pComponentPrivate->curState == OMX_StateExecuting){
        pComponentPrivate->cbInfo.EventHandler (
                                                pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorSameState,0,   
                                                NULL);
        G722ENC_DPRINT("%d :: Error: Same State Given by \
                               Application\n",__LINE__);
    }
    else if (pComponentPrivate->curState == OMX_StateIdle) {
        if(pComponentPrivate->dasfmode == 1 && pComponentPrivate->bStreamCtrlCalled == 0) {
            G722ENC_DPRINT("%d :: G722 ENCODER IS RUNNING UNDER DASF MODE\n",__LINE__);
            G722ENC_DPRINT("%d :: ---- Comp: DASF Functionality is ON ---\n",__LINE__);

            pComponentPrivate->pParams->iAudioFormat = (unsigned short)pComponentPrivate->g722Params->nChannels;
            pComponentPrivate->pParams->iStrmId = pComponentPrivate->streamID;
            pComponentPrivate->pParams->iSamplingRate = G722ENC_SAMPLE_RATE;

            G722ENC_DPRINT("%d ::pComponentPrivate->pParams->iAudioFormat   = %d\n",__LINE__,pComponentPrivate->pParams->iAudioFormat);
            G722ENC_DPRINT("%d ::pComponentPrivate->pParams->iSamplingRate  = %d\n",__LINE__,pComponentPrivate->pParams->iSamplingRate);
            G722ENC_DPRINT("%d ::pComponentPrivate->pParams->iStrmId = %d\n",__LINE__,pComponentPrivate->pParams->iStrmId);

            pValues[0] = USN_STRMCMD_SETCODECPARAMS;
            pValues[1] = (OMX_U32)pComponentPrivate->pParams;
            pValues[2] = sizeof(G722ENC_AudioCodecParams);

            G722ENC_DPRINT("%d: G722ENCUTILS::About to call LCML_ControlCodec\n",__LINE__);
            eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                       EMMCodecControlStrmCtrl,(void *)pValues);
            if(eError != OMX_ErrorNone) {
                G722ENC_DPRINT("%d: Error Occurred in Codec StreamControl..\n",__LINE__);
                goto EXIT;
            }
        }

        G722ENC_DPRINT("%d: G722ENCUTILS::About to call LCML_ControlCodec\n",__LINE__);
        eError = LCML_ControlCodec(
                                   ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                   EMMCodecControlStart,(void *)pArgs);
        if(eError != OMX_ErrorNone) {
            G722ENC_DPRINT("%d: Error Occurred in Codec Start..\n",__LINE__);
            goto EXIT;
        }
        nBuf = pComponentPrivate->pInputBufferList->numBuffers;

        if(pComponentPrivate->dasfmode == 0) {
            G722ENC_DPRINT ("%d :: Comp :: After LCML_StartCodec function \n",__LINE__);
            nBuf = pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->nBufferCountActual;
            pBufHdr = pComponentPrivate->pBufHeader[G722ENC_INPUT_PORT];
            G722ENC_DPRINT ("%d :: Comp :: After LCML_StartCodec function \n",__LINE__);
            G722ENC_DPRINT ("Sending Input buffer to Application\n");
            G722ENC_DPRINT ("%d :: Component Sending Input buffer to Application %p\n",__LINE__, pBufHdr);
            pLcmlHdr = pComponentPrivate->pLcmlBufHeader[G722ENC_INPUT_PORT];
        }
    }
    else if (pComponentPrivate->curState == OMX_StatePause) {
        G722ENC_DPRINT("%d: Comp: Resume Command Came from App\n",__LINE__);
        G722ENC_DPRINT("%d: G722ENCUTILS::About to call LCML_ControlCodec\n",__LINE__);
        eError = LCML_ControlCodec(
                                   ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                   EMMCodecControlStart,(void *)pArgs);

        if (eError != OMX_ErrorNone) {
            G722ENC_DPRINT ("Error While Resuming the codec\n");
            goto EXIT;
        }

        G722ENC_DPRINT("pComponentPrivate->nNumInputBufPending = %d\n",pComponentPrivate->nNumInputBufPending);
        if (pComponentPrivate->nNumInputBufPending < pComponentPrivate->pInputBufferList->numBuffers) {
            pComponentPrivate->nNumInputBufPending = pComponentPrivate->pInputBufferList->numBuffers;
        }

        for (i=0; i < pComponentPrivate->nNumInputBufPending; i++) {
            if (pComponentPrivate->pInputBufHdrPending[i]) {
                if (!G722ENC_IsPending(pComponentPrivate, pComponentPrivate->pInputBufHdrPending[i], OMX_DirInput)) {
                    G722ENC_GetCorresponding_LCMLHeader(pComponentPrivate->pInputBufHdrPending[i]->pBuffer, OMX_DirInput, &pLcmlHdr);
                    G722ENC_SetPending(pComponentPrivate,pComponentPrivate->pInputBufHdrPending[i],OMX_DirInput);

                    eError = LCML_QueueBuffer( 
                                              ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                              EMMCodecInputBuffer,
                                              pComponentPrivate->pInputBufHdrPending[i]->pBuffer,
                                              pComponentPrivate->pInputBufHdrPending[i]->nAllocLen,
                                              pComponentPrivate->pInputBufHdrPending[i]->nFilledLen,
                                              (OMX_U8 *) pLcmlHdr->pIpParam,
                                              sizeof(G722ENC_UAlgInBufParamStruct),
                                              pComponentPrivate->pInputBufHdrPending[i]->pBuffer);
                }
            }
        }

        for (i=0; i < pComponentPrivate->nNumOutputBufPending; i++) {
            if (pComponentPrivate->pOutputBufHdrPending[i]) {
                if (!G722ENC_IsPending(pComponentPrivate, pComponentPrivate->pOutputBufHdrPending[i], OMX_DirOutput)) {
                    G722ENC_GetCorresponding_LCMLHeader(pComponentPrivate->pOutputBufHdrPending[i]->pBuffer, OMX_DirOutput, &pLcmlHdr);
                    G722ENC_SetPending(pComponentPrivate,pComponentPrivate->pOutputBufHdrPending[i],OMX_DirOutput);

                    eError = LCML_QueueBuffer(
                                              ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                              EMMCodecOuputBuffer,
                                              pComponentPrivate->pOutputBufHdrPending[i]->pBuffer,
                                              pComponentPrivate->pOutputBufHdrPending[i]->nAllocLen,
                                              pComponentPrivate->pOutputBufHdrPending[i]->nFilledLen,
                                              NULL,
                                              0,
                                              pComponentPrivate->pOutputBufHdrPending[i]->pBuffer);  


                }
            }
        }
    }
    else {
        pComponentPrivate->cbInfo.EventHandler (
                                                pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorIncorrectStateTransition, 0,
                                                "Invalid State");
        G722ENC_DPRINT("%d :: Error: Invalid State Given by Application\n",__LINE__);
        goto EXIT;
    }
    pComponentPrivate->curState = OMX_StateExecuting;

#ifdef RESOURCE_MANAGER_ENABLED
    rm_error = RMProxy_NewSendCommand(pHandle,
                                      RMProxy_StateSet,
                                      OMX_G722_Encoder_COMPONENT,
                                      OMX_StateExecuting, 1234, NULL);
#endif


    pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                           pHandle->pApplicationPrivate,
                                           OMX_EventCmdComplete,
                                           OMX_CommandStateSet, pComponentPrivate->curState, NULL);
 EXIT:
    return eError;
}

/* ================================================================================= */
/**
 * @fn G722ENC_CommandToPause() description for G722ENC_CommandToPause  
 G722ENC_CommandToPause().  
 This component is called by HandleCommand() when the component is commanded to Paused
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ================================================================================ */
OMX_ERRORTYPE G722ENC_CommandToPause(G722ENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    char *pArgs = "damedesuStr";
    OMX_COMPONENTTYPE *pHandle =
        (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;

    G722ENC_DPRINT("%d: G722ENC_HandleCommand: Cmd Pause: Cur State = %d\n",__LINE__,pComponentPrivate->curState);
    if (pComponentPrivate->curState == OMX_StatePause){
        pComponentPrivate->cbInfo.EventHandler (
                                                pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorSameState,0,
                                                NULL);
        G722ENC_DPRINT("%d :: Error: Same State Given by Application\n",__LINE__);
        goto EXIT;
    }
    if ((pComponentPrivate->curState != OMX_StateExecuting) &&
        (pComponentPrivate->curState != OMX_StateIdle)) {

        pComponentPrivate->cbInfo.EventHandler (
                                                pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorIncorrectStateTransition, 0,
                                                "Invalid State");

        G722ENC_DPRINT("%d :: Error: Invalid State Given by Application\n",__LINE__);
        goto EXIT;
    }

    G722ENC_DPRINT("%d: G722ENCUTILS::About to call LCML_ControlCodec\n",__LINE__);
    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                               EMMCodecControlPause,(void *)pArgs);

    if (eError != OMX_ErrorNone) {
        G722ENC_DPRINT("%d : Error: in Pausing the codec\n",__LINE__);
        goto EXIT;
    }

    pComponentPrivate->curState = OMX_StatePause;
#ifdef RESOURCE_MANAGER_ENABLED
    eError = RMProxy_NewSendCommand(pComponentPrivate_CC->pHandle,
                                    RMProxy_StateSet,
                                    OMX_G722_Encoder_COMPONENT,
                                    OMX_StatePause,
                                    1234,
                                    NULL);
#endif

    /*    pComponentPrivate->cbInfo.EventHandler(pHandle,
          pHandle->pApplicationPrivate,
          OMX_EventCmdComplete,
          OMX_CommandStateSet,
          pComponentPrivate->curState,
          NULL); */
    G722ENC_DPRINT("%d :: Component: Codec Is Paused\n",__LINE__);
 EXIT:
    return eError;
}



/* ================================================================================= */
/**
 * @fn G722ENC_CommandToWaitForResources() description for G722ENC_CommandToWaitForResources  
 G722ENC_CommandToWaitForResources().  
 This component is called by HandleCommand() when the component is commanded 
 to WaitForResources
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ================================================================================ */
OMX_ERRORTYPE G722ENC_CommandToWaitForResources(G722ENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle =
        (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;

    G722ENC_DPRINT("%d: G722ENC_HandleCommand: Cmd : OMX_StateWaitForResources\n",__LINE__);
    if (pComponentPrivate->curState == OMX_StateWaitForResources){
        G722ENC_DPRINT("%d :: Comp: OMX_AmrDecUtils.c\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler (
                                                pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorSameState,0,
                                                NULL);
        G722ENC_DPRINT("%d :: Error: Same State Given by Application\n",__LINE__);
        goto EXIT;
    }
    else if (pComponentPrivate->curState == OMX_StateLoaded) {
        pComponentPrivate->curState = OMX_StateWaitForResources;
        G722ENC_DPRINT("%d: Transitioning from Loaded to OMX_StateWaitForResources\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler(
                                               pHandle, pHandle->pApplicationPrivate,
                                               OMX_EventCmdComplete,
                                               OMX_CommandStateSet,pComponentPrivate->curState, NULL);
        goto EXIT;
    } 
    else {
        pComponentPrivate->cbInfo.EventHandler(
                                               pHandle, pHandle->pApplicationPrivate,
                                               OMX_EventError, OMX_ErrorIncorrectStateTransition,0, NULL);
    }
 EXIT:
    return eError;
}


/* ================================================================================= */
/**
 * @fn G722ENC_SetPending() description for G722ENC_SetPending  
 G722ENC_SetPending().  
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
void G722ENC_SetPending(G722ENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir) 
{
    OMX_U16 i = 0;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 1;
                G722ENC_DPRINT("*******************INPUT BUFFER %d IS PENDING ******************************\n",i);
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 1;
                G722ENC_DPRINT("*******************OUTPUT BUFFER %d IS PENDING******************************\n",i);
            }
        }
    }
}

/* ================================================================================= */
/**
 * @fn G722ENC_ClearPending() description for G722ENC_ClearPending  
 G722ENC_ClearPending().  
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
void G722ENC_ClearPending(G722ENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir) 
{
    OMX_U16 i = 0;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 0;
                G722ENC_DPRINT("*******************INPUT BUFFER %d IS RECLAIMED******************************\n",i);
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 0;
                G722ENC_DPRINT("*******************OUTPUT BUFFER %d IS RECLAIMED ******************************\n",i);
            }
        }
    }
}

/* ================================================================================= */
/**
 * @fn G722ENC_IsPending() description for G722ENC_IsPending  
 G722ENC_IsPending().  
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
OMX_U32 G722ENC_IsPending(G722ENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir) 
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
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                return pComponentPrivate->pOutputBufferList->bBufferPending[i];
            }
        }
    }
    return -1;
}

/* ================================================================================= */
/**
 * @fn G722ENC_Fill_LCMLInitParamsEx() description for G722ENC_Fill_LCMLInitParamsEx  
 G722ENC_Fill_LCMLInitParamsEx().  
 This method fills the LCML init parameters.
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return OMX_ERRORTYPE
 */
/* ================================================================================ */
OMX_ERRORTYPE G722ENC_Fill_LCMLInitParamsEx(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0,nIpBufSize = 0,nOpBuf = 0,nOpBufSize = 0;
    OMX_BUFFERHEADERTYPE *pTemp = NULL;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    G722ENC_COMPONENT_PRIVATE *pComponentPrivate = pHandle->pComponentPrivate;
    G722ENC_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;
    OMX_U16 i = 0;
    OMX_U32 size_lcml = 0;

    G722ENC_DPRINT("%d :: G722ENCFill_LCMLInitParamsEx\n ",__LINE__);
    G722ENC_DPRINT("[G722ENC_Fill_LCMLInitParamsEx] pComponent = %p\n",pComponent);
    G722ENC_DPRINT("[G722ENC_Fill_LCMLInitParamsEx] pComponentPrivate = %p\n",pComponentPrivate);


    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nIpBufSize = pComponentPrivate->pPortDef[G722ENC_INPUT_PORT]->nBufferSize;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    nOpBufSize = pComponentPrivate->pPortDef[G722ENC_OUTPUT_PORT]->nBufferSize;

    G722ENC_DPRINT("%d :: ------ Buffer Details -----------\n",__LINE__);
    G722ENC_DPRINT("%d :: Input  Buffer Count = %ld\n",__LINE__,nIpBuf);
    G722ENC_DPRINT("%d :: Input  Buffer Size = %ld\n",__LINE__,nIpBufSize);
    G722ENC_DPRINT("%d :: Output Buffer Count = %ld\n",__LINE__,nOpBuf);
    G722ENC_DPRINT("%d :: Output Buffer Size = %ld\n",__LINE__,nOpBufSize);
    G722ENC_DPRINT("%d :: ------ Buffer Details ------------\n",__LINE__);

    /* Allocate memory for all input buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = nIpBuf * sizeof(G722ENC_LCML_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, G722ENC_LCML_BUFHEADERTYPE);
    pComponentPrivate->pLcmlBufHeader[G722ENC_INPUT_PORT] = pTemp_lcml;
    for (i=0; i<nIpBuf; i++) {
        G722ENC_DPRINT("%d :: INPUT--------- Inside Ip Loop\n",__LINE__);
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nIpBufSize;
        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = G722ENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = G722ENC_MINOR_VER;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        pTemp_lcml->pBufHdr = pTemp;
        G722ENC_DPRINT("%d :: pTemp_lcml->pBufHdr->pBuffer = %p \n",__LINE__,pTemp_lcml->pBufHdr->pBuffer);
        pTemp_lcml->eDir = OMX_DirInput;
        pTemp_lcml->pOtherParams[i] = NULL;
        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pIpParam, sizeof(G722ENC_UAlgInBufParamStruct), G722ENC_UAlgInBufParamStruct);
        if (pTemp_lcml->pIpParam == NULL) {
            G722ENC_CleanupInitParams(pComponent);
            G722ENC_DPRINT("%d :: Exiting AllocateBuffer\n",__LINE__);
            return OMX_ErrorInsufficientResources;
        }

        pTemp_lcml->pIpParam->bLastBuffer = 0;

        pTemp->nFlags = NORMAL_BUFFER;
        ((G722ENC_COMPONENT_PRIVATE *) pTemp->pPlatformPrivate)->pHandle = pHandle;
        G722ENC_DPRINT("%d ::Comp: InBuffHeader[%d] = %p\n", __LINE__, i, pTemp);
        G722ENC_DPRINT("%d ::Comp:  >>>> InputBuffHeader[%d]->pBuffer = %p\n",
                       __LINE__, i, pTemp->pBuffer);
        G722ENC_DPRINT("%d ::Comp: Ip : pTemp_lcml[%d] = %p\n", __LINE__, i, pTemp_lcml);

        pTemp_lcml++;
    }

    pTemp = pComponentPrivate->pBufHeader[G722ENC_OUTPUT_PORT];
    size_lcml = nOpBuf * sizeof(G722ENC_LCML_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, G722ENC_LCML_BUFHEADERTYPE);
    pComponentPrivate->pLcmlBufHeader[G722ENC_OUTPUT_PORT] = pTemp_lcml;

    for (i=0; i<nOpBuf; i++) {
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nOpBufSize;
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = G722ENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = G722ENC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        pTemp_lcml->pBufHdr = pTemp;
        pTemp_lcml->eDir = OMX_DirOutput;
        pTemp_lcml->pOtherParams[i] = NULL;

        pTemp->nFlags = NORMAL_BUFFER;
        ((G722ENC_COMPONENT_PRIVATE *)pTemp->pPlatformPrivate)->pHandle = pHandle;
        G722ENC_DPRINT("%d ::Comp:  >>>>>>>>>>>>> OutBuffHeader[%d] = %p\n",
                       __LINE__, i, pTemp);
        G722ENC_DPRINT("%d ::Comp:  >>>> OutBuffHeader[%d]->pBuffer = %p\n",
                       __LINE__, i, pTemp->pBuffer);
        G722ENC_DPRINT("%d ::Comp: Op : pTemp_lcml[%d] = %p\n", __LINE__, i, pTemp_lcml);
        pTemp++;
        pTemp_lcml++;
    }
    pComponentPrivate->bPortDefsAllocated = 1;

    G722ENC_DPRINT("%d :: Exiting G722ENC_Fill_LCMLInitParams",__LINE__);

    pComponentPrivate->bInitParamsInitialized = 1;
 EXIT:
    if (eError != OMX_ErrorNone) {
       G722ENC_CleanupInitParams(pComponent);
    }
    return eError;
}


/* ================================================================================= */
/**
 * @fn G722ENC_IsValid() description for G722ENC_Fill_LCMLInitParamsEx  
 G722ENC_IsValid().  
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
OMX_U32 G722ENC_IsValid(G722ENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U8 *pBuffer, OMX_DIRTYPE eDir) 
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

#ifdef RESOURCE_MANAGER_ENABLED
void G722ENC_ResourceManagerCallback(RMPROXY_COMMANDDATATYPE cbData)
{
    OMX_COMMANDTYPE Cmd = OMX_CommandStateSet;
    OMX_STATETYPE state = OMX_StateIdle;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)cbData.hComponent;
    G722ENC_COMPONENT_PRIVATE *pCompPrivate = NULL;

    pCompPrivate = (G722ENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    if (*(cbData.RM_Error) == OMX_RmProxyCallback_ResourcesPreempted){
        if (pCompPrivate->curState == OMX_StateExecuting ||
            pCompPrivate->curState == OMX_StatePause) {

            write (pCompPrivate->cmdPipe[1], &Cmd, sizeof(Cmd));
            write (pCompPrivate->cmdDataPipe[1], &state ,sizeof(OMX_U32));

            pCompPrivate->bPreempted = 1;
        }
    }
    else if (*(cbData.RM_Error) == OMX_RmProxyCallback_ResourcesAcquired){
        pCompPrivate->cbInfo.EventHandler ( pHandle,
                                            pHandle->pApplicationPrivate,
                                            OMX_EventResourcesAcquired,
                                            0, 0, NULL);
    }
    else if (*(cbData.RM_Error) == OMX_RmProxyCallback_FatalError){
        G722ENC_FatalErrorRecover(pCompPrivate);
    }
}
#endif

void G722ENC_FatalErrorRecover(G722ENC_COMPONENT_PRIVATE *pComponentPrivate){
#ifdef RESOURCE_MANAGER_ENABLED
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle,
             RMProxy_FreeResource,
             OMX_G722_Encoder_COMPONENT, 0, 3456, NULL);

    eError = RMProxy_Deinitalize();
    if (eError != OMX_ErrorNone) {
        G722ENC_DPRINT("%d :: From RMProxy_Deinitalize\n",__LINE__);
    }
#endif

    pComponentPrivate->curState = OMX_StateInvalid;
    pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                       OMX_EventError,
                                       OMX_ErrorInvalidState,
                                       OMX_TI_ErrorSevere,
                                       NULL);
    if(pComponentPrivate->DSPMMUFault == OMX_FALSE){
        pComponentPrivate->DSPMMUFault = OMX_TRUE;
        G722ENC_CleanupInitParams(pComponentPrivate->pHandle);
    }
    G722ENC_DPRINT("%d :: Completed FatalErrorRecover \
                   \nEntering Invalid State\n",__LINE__);
}


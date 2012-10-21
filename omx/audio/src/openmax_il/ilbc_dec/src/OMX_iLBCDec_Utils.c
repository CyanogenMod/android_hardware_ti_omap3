
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
* @file OMX_iLBCDec_Utils.c
*
* This file implements various utilitiy functions for various activities
* like handling command from application, callback from LCML etc.
*
* @path  $(CSLPATH)\OMAPSW_MPU\linux\audio\src\openmax_il\iLBC_dec\src
*
* @rev  1.0
*/
/* =========================================================================== */

/* ------compilation control switches -------------------------*/
/****************************************************************
*  INCLUDE FILES
****************************************************************/
/* ----- system and platform files ----------------------------*/

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <malloc.h>
#include <memory.h>
#include <fcntl.h>

#include <pthread.h>
#include <dbapi.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>

/*------- Program Header Files -----------------------------------------------*/
#include "OMX_iLBCDec_Utils.h"
#include "iLBCdecsocket_ti.h"
#include "usn_ti.h"
#include "usn.h"
#include "LCML_DspCodec.h"

/*******************************************************
 * Temp Defines
 *******************************************************/
#define OMX_iLBC_SN_TIMEOUT -1

/* ================================================================================= * */
/**
* @fn iLBCDEC_ComponentThread() This is component thread that keeps listening for
* commands or event/messages/buffers from application or from LCML.
*
* @param pThreadData This is thread argument.
*
* @pre          None
*
* @post         None
*
*  @return      OMX_ErrorNone = Always
*
*  @see         None
*/
/* ================================================================================ * */
void* iLBCDEC_ComponentThread (void* pThreadData)
{
    OMX_S16 status = 0;
    struct timespec tv;
    OMX_S16 fdmax = 0;
    fd_set rfds;
    OMX_U32 nRet = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    iLBCDEC_COMPONENT_PRIVATE* pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE*)pThreadData;
    OMX_COMPONENTTYPE *pHandle = pComponentPrivate->pHandle;

    iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
    fdmax = pComponentPrivate->cmdPipe[0];

    if (pComponentPrivate->dataPipe[0] > fdmax) {
        fdmax = pComponentPrivate->dataPipe[0];
    }

    while (1) {
        FD_ZERO (&rfds);
        FD_SET (pComponentPrivate->cmdPipe[0], &rfds);
        FD_SET (pComponentPrivate->dataPipe[0], &rfds);
        tv.tv_sec = 1;
        tv.tv_nsec = 0; 

        sigset_t set;
        sigemptyset (&set);
        sigaddset (&set, SIGALRM);
        status = pselect (fdmax+1, &rfds, NULL, NULL, &tv, &set);

        if (0 == status) {
            if (pComponentPrivate->bIsStopping == 1)  {
            /* Do not accept any command when the component is stopping */
                pComponentPrivate->bIsStopping = 0;
                pComponentPrivate->lcml_nOpBuf = 0;
                pComponentPrivate->lcml_nIpBuf = 0;
                pComponentPrivate->app_nBuf = 0;
                pComponentPrivate->num_Reclaimed_Op_Buff = 0;

                if (pComponentPrivate->curState != OMX_StateIdle) {
                    goto EXIT;
                }
            }
            iLBCDEC_DPRINT ("%d :: %s :: Component Time Out !!!!!!!!!!!! \n", __LINE__,__FUNCTION__);
        } else if (-1 == status) {
            iLBCDEC_EPRINT ("%d :: %s :: Error in Select\n", __LINE__, __FUNCTION__);
            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                   pHandle->pApplicationPrivate,
                                                   OMX_EventError,
                                                   OMX_ErrorInsufficientResources,
                                                   0,
                                                   "Error from COmponent Thread in select");
            eError = OMX_ErrorInsufficientResources;

        } else if ((FD_ISSET (pComponentPrivate->dataPipe[0], &rfds))
                 && (pComponentPrivate->curState != OMX_StatePause)) {
            OMX_BUFFERHEADERTYPE *pBufHeader = NULL;
            iLBCDEC_DPRINT ("%d :: %s :: DATA pipe is set in Component Thread\n",__LINE__,__FUNCTION__);

            int ret = read(pComponentPrivate->dataPipe[0], &pBufHeader, sizeof(pBufHeader));
            if (ret == -1) {
                iLBCDEC_EPRINT ("%d :: %s :: Error while reading from the pipe\n",__LINE__,__FUNCTION__);
            }
            eError = iLBCDEC_HandleDataBuf_FromApp(pBufHeader, pComponentPrivate);
            if (eError != OMX_ErrorNone) {
                iLBCDEC_EPRINT ("%d :: %s :: Error From iLBCDECHandleDataBuf_FromApp\n",__LINE__,__FUNCTION__);
                break;
            }

        } else if (FD_ISSET (pComponentPrivate->cmdPipe[0], &rfds)) {
            iLBCDEC_DPRINT ("%d :: %s :: CMD pipe is set in Component Thread\n",__LINE__,__FUNCTION__);
            nRet = iLBCDEC_HandleCommand (pComponentPrivate);
            if (nRet == EXIT_COMPONENT_THRD) {
                iLBCDEC_DPRINT ("%d :: %s :: Exiting from Component thread\n",
                                 __LINE__,__FUNCTION__);

                if(eError != OMX_ErrorNone) {
                    iLBCDEC_DPRINT("%d :: %s :: Function Mp3Dec_FreeCompResources returned error\n",__LINE__,__FUNCTION__);
                    goto EXIT;
                }
                iLBCDEC_DPRINT("%d :: %s :: ARM Side Resources Have Been Freed\n",__LINE__,__FUNCTION__);

                pComponentPrivate->curState = OMX_StateLoaded;
                pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_ErrorNone,pComponentPrivate->curState, NULL);
            }
        }
    }
EXIT:
    iLBCDEC_DPRINT("%d :: %s :: Exiting ComponentThread\n",__LINE__,  __FUNCTION__);
    return (void*)eError;
}

/* ========================================================================== */
/**
* @iLBCDEC_Fill_LCMLInitParams () This function is used by the component thread
* to fill the all of its initialization parameters, buffer deatils  etc to 
* LCML structure,
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
OMX_ERRORTYPE iLBCDEC_Fill_LCMLInitParams(OMX_HANDLETYPE pComponent,
                                  LCML_DSP *plcml_Init, OMX_U16 arr[])
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0,nIpBufSize = 0,nOpBuf = 0,nOpBufSize = 0;
    OMX_U16 i = 0;
    OMX_BUFFERHEADERTYPE *pTemp = NULL;
    OMX_U32 size_lcml = 0;
    LCML_STRMATTR *strmAttr = NULL;
    LCML_DSP_INTERFACE *pHandle = (LCML_DSP_INTERFACE *)pComponent;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    iLBCD_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;

    iLBCDEC_DPRINT("%d :: %s\n ",__LINE__,__FUNCTION__);
    iLBCDEC_DPRINT("%d :: %s :: pHandle = %p\n",__LINE__,__FUNCTION__,pHandle);
    iLBCDEC_DPRINT("%d :: %s :: pHandle->pComponentPrivate = %p\n",__LINE__, __FUNCTION__,pHandle->pComponentPrivate);

    pComponentPrivate = pHandle->pComponentPrivate;

    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    pComponentPrivate->nRuntimeInputBuffers = nIpBuf;
    
    OMX_U16 iLBCcodecType = pComponentPrivate->iLBCcodecType;
    nIpBufSize = STD_iLBCDEC_BUF_SIZE;              
    if (iLBCcodecType == 1) {
        nIpBufSize = INPUT_iLBCDEC_SECBUF_SIZE;
    }

    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    pComponentPrivate->nRuntimeOutputBuffers = nOpBuf;

    OMX_U16 codecMode = pComponentPrivate->iLBCcodecType;
    nOpBufSize = iLBCD_OUTPUT_BUFFER_SIZE;
    if (codecMode == 1) {
        nOpBufSize = OUTPUT_iLBCDEC_SECBUF_SIZE;
        nIpBufSize = INPUT_iLBCDEC_SECBUF_SIZE;
    }

    /* Fill Input Buffers Info for LCML */
    plcml_Init->In_BufInfo.nBuffers = nIpBuf;
    plcml_Init->In_BufInfo.nSize = nIpBufSize;
    plcml_Init->In_BufInfo.DataTrMethod = DMM_METHOD;


    /* Fill Output Buffers Info for LCML */
    plcml_Init->Out_BufInfo.nBuffers = nOpBuf;
    plcml_Init->Out_BufInfo.nSize = nOpBufSize;
    plcml_Init->Out_BufInfo.DataTrMethod = DMM_METHOD;

    /*Copy the node information */
    plcml_Init->NodeInfo.nNumOfDLLs = 3;

    plcml_Init->NodeInfo.AllUUIDs[0].uuid = &iLBCDECSOCKET_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[0].DllName, iLBCDEC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[0].eDllType = DLL_NODEOBJECT;

    plcml_Init->NodeInfo.AllUUIDs[1].uuid = &iLBCDECSOCKET_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[1].DllName, iLBCDEC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[1].eDllType = DLL_DEPENDENT;

    plcml_Init->NodeInfo.AllUUIDs[2].uuid = &USN_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[2].DllName,
            USN_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[2].eDllType = DLL_DEPENDENT;


    if(pComponentPrivate->dasfmode == 1) {
        iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->dasfmode = %d\n",__LINE__,
                      __FUNCTION__,pComponentPrivate->dasfmode);

        OMX_MALLOC_GENERIC(strmAttr,LCML_STRMATTR);

        pComponentPrivate->strmAttr = strmAttr;
        OMX_U16 codecType = pComponentPrivate->iLBCcodecType;

        strmAttr->uSegid = 0;
        strmAttr->uAlignment = 0;
        strmAttr->uTimeout = OMX_iLBC_SN_TIMEOUT; /* -1 */
        strmAttr->uBufsize =(codecType == 0)? iLBCD_OUTPUT_BUFFER_SIZE: OUTPUT_iLBCDEC_SECBUF_SIZE; /* 320 */
        strmAttr->uNumBufs = 2;
        strmAttr->lMode = STRMMODE_PROCCOPY;
        plcml_Init->DeviceInfo.TypeofDevice =1;
        plcml_Init->DeviceInfo.TypeofRender =0;

        plcml_Init->DeviceInfo.AllUUIDs[0].uuid = &DCTN_TI_UUID;
        plcml_Init->DeviceInfo.DspStream = strmAttr;
    }
    else {
        pComponentPrivate->strmAttr = NULL;
    }


    /*copy the other information */
    plcml_Init->SegID = OMX_iLBCDEC_DEFAULT_SEGMENT; /* 0 */
    plcml_Init->Timeout = OMX_iLBCDEC_SN_TIMEOUT; /* -1 */
    plcml_Init->Alignment = 0;
    plcml_Init->Priority = OMX_iLBCDEC_SN_PRIORITY; /* 10 */
    plcml_Init->ProfileID = -1;

    /* Set DSP static create parameters */
    
    arr[0] = STREAM_COUNT; /* 2 */
    arr[1] = iLBCD_INPUT_PORT; /* 0 */
    arr[3] = pComponentPrivate->pInputBufferList->numBuffers;
    arr[4] = iLBCD_OUTPUT_PORT; /* 1 */
    arr[6] = pComponentPrivate->pOutputBufferList->numBuffers;
    arr[7] = pComponentPrivate->iLBCcodecType;  /* 0   * GSM FR: RTP packet format; iLBC: Primary/Secondary codec */
    arr[8] = END_OF_CR_PHASE_ARGS;
    
    if(pComponentPrivate->dasfmode == 1){
        iLBCDEC_DPRINT("%d :: %s : DASF MODE Parameters selected\n",__LINE__,
                __FUNCTION__);
        arr[2] =  iLBCDEC_DMM; /* 0 */
        arr[5] =  iLBCDEC_OUTSTRM; /* 2 */
    }else{
        iLBCDEC_DPRINT("%d :: %s : F2F MODE Parameters selected\n",__LINE__,
                __FUNCTION__);
        arr[2] = iLBCDEC_DMM; /* 0 */
        arr[5] = iLBCDEC_DMM; /* 0 */
    }

    plcml_Init->pCrPhArgs = arr;

    size_lcml = nIpBuf * sizeof(iLBCD_LCML_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml,size_lcml,iLBCD_LCML_BUFHEADERTYPE);

    pComponentPrivate->pLcmlBufHeader[iLBCD_INPUT_PORT] = pTemp_lcml;

    for (i=0; i<nIpBuf; i++) {
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
/*        pTemp->nAllocLen = nIpBufSize;*/
        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = iLBCDEC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = iLBCDEC_MINOR_VER;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = 0;
        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirInput;

        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pBufferParam, sizeof(iLBCDEC_ParamStruct), iLBCDEC_ParamStruct);
        if (pTemp_lcml->pBufferParam == NULL) {
            iLBCDEC_EPRINT ("%d :: %s OMX_ErrorInsufficientResources\n",  __LINE__,__FUNCTION__);
            iLBCDEC_CleanupInitParams(pComponentPrivate->pHandle);
            return OMX_ErrorInsufficientResources;
        }

        pTemp_lcml->pBufferParam->usNbFrames =0;
        pTemp_lcml->pBufferParam->pParamElem = NULL;
        pTemp_lcml->pFrameParam = NULL;
        
        OMX_MALLOC_GENERIC(pTemp_lcml->pDmmBuf,DMM_BUFFER_OBJ);

        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */
        pTemp->nFlags = NORMAL_BUFFER;

        pTemp_lcml++;
    }

    /* Allocate memory for all output buffer headers..
       * This memory pointer will be sent to LCML */
    size_lcml = nOpBuf * sizeof(iLBCD_LCML_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml,size_lcml, iLBCD_LCML_BUFHEADERTYPE);

    pComponentPrivate->pLcmlBufHeader[iLBCD_OUTPUT_PORT] = pTemp_lcml;

    for (i=0; i<nOpBuf; i++) {
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
/*        pTemp->nAllocLen = nOpBufSize;*/
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = iLBCDEC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = iLBCDEC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */

        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirOutput;
        pTemp_lcml->pFrameParam = NULL;

        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pBufferParam, sizeof(iLBCDEC_ParamStruct), iLBCDEC_ParamStruct);
        if (pTemp_lcml->pBufferParam == NULL) {
            iLBCDEC_EPRINT ("%d :: %s OMX_ErrorInsufficientResources\n",  __LINE__,__FUNCTION__);
            iLBCDEC_CleanupInitParams(pComponentPrivate->pHandle);
            return OMX_ErrorInsufficientResources;
        }

        pTemp_lcml->pBufferParam->usNbFrames =0;
        pTemp_lcml->pBufferParam->pParamElem = NULL;        

        OMX_MALLOC_GENERIC(pTemp_lcml->pDmmBuf, DMM_BUFFER_OBJ);

        iLBCDEC_DPRINT("%d :: %s ::pTemp_lcml = %p\n",__LINE__,
                      __FUNCTION__,pTemp_lcml);
        iLBCDEC_DPRINT("%d :: %s ::pTemp_lcml->buffer = %p\n",__LINE__,
                      __FUNCTION__,pTemp_lcml->buffer);

        pTemp->nFlags = NORMAL_BUFFER;

        pTemp_lcml++;
    }
    pComponentPrivate->bPortDefsAllocated = 1;
    pComponentPrivate->bInitParamsInitialized = 1; 


    iLBCDEC_DPRINT("%d :: %s :: Exiting iLBCDEC_Fill_LCMLInitParams\n",
                  __LINE__,__FUNCTION__);

EXIT:

    return eError;
}

/* ========================================================================== */
/**
* @iLBCDEC_StartComponentThread() This function is called by the component to 
* create the component thread, command pipe, data pipe and LCML Pipe.
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
OMX_ERRORTYPE iLBCDEC_StartComponentThread(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate =
        (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    iLBCDEC_DPRINT ("%d :: %s :: Inside\n", __LINE__,__FUNCTION__);

    /* Initialize all the variables*/
    pComponentPrivate->bIsStopping = 0;
    pComponentPrivate->lcml_nOpBuf = 0;
    pComponentPrivate->lcml_nIpBuf = 0;
    pComponentPrivate->app_nBuf = 0;
    pComponentPrivate->num_Reclaimed_Op_Buff = 0;

    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->cmdDataPipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->dataPipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* create the pipe used to send commands to the thread */
    eError = pipe (pComponentPrivate->cmdPipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* create the pipe used to send commands to the thread */
    eError = pipe (pComponentPrivate->lcml_Pipe);
    if (eError) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* Create the Component Thread */
    eError = pthread_create (&(pComponentPrivate->ComponentThread), NULL, 
                             iLBCDEC_ComponentThread, pComponentPrivate);
    if (eError || !pComponentPrivate->ComponentThread) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pComponentPrivate->bCompThreadStarted = 1;
 EXIT:
    return eError;
}

/* ========================================================================== */
/**
* @iLBCDEC_FreeCompResources() This function is called by the component during
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

OMX_ERRORTYPE iLBCDEC_FreeCompResources(OMX_HANDLETYPE pComponent)
{
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = 
        (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0;
    OMX_U32 nOpBuf = 0;

    iLBCDEC_DPRINT ("%d :: %s\n", __LINE__,__FUNCTION__);

    if (pComponentPrivate->bPortDefsAllocated) {
        nIpBuf = pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->nBufferCountActual;
        nOpBuf = pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->nBufferCountActual;
    }

    if (pComponentPrivate->bCompThreadStarted) {
        err = close (pComponentPrivate->dataPipe[0]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->dataPipe[1]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->cmdPipe[0]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->cmdPipe[1]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->cmdDataPipe[0]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->cmdDataPipe[1]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->lcml_Pipe[0]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->lcml_Pipe[1]);
        if (0 != err && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
        }
    }

    if (pComponentPrivate->bPortDefsAllocated){
        OMX_MEMFREE_STRUCT(pComponentPrivate->pPriorityMgmt);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]);
        OMX_MEMFREE_STRUCT(pComponentPrivate->iLBCParams);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pcmParams);
        OMX_MEMFREE_STRUCT(pComponentPrivate->sPortParam);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pInputBufferList);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pOutputBufferList);
        OMX_MEMFREE_STRUCT(pComponentPrivate->sDeviceString);
    }
        
    pComponentPrivate->bPortDefsAllocated = 0;

    if (pComponentPrivate->bMutexInitialized) {
        iLBCDEC_DPRINT("\n\n FreeCompResources: Destroying mutexes.\n\n");
        pComponentPrivate->bMutexInitialized = OMX_FALSE;

        pthread_mutex_destroy(&pComponentPrivate->InLoaded_mutex);
        pthread_cond_destroy(&pComponentPrivate->InLoaded_threshold);
    
        pthread_mutex_destroy(&pComponentPrivate->InIdle_mutex);
        pthread_cond_destroy(&pComponentPrivate->InIdle_threshold);
    
        pthread_mutex_destroy(&pComponentPrivate->AlloBuf_mutex);
        pthread_cond_destroy(&pComponentPrivate->AlloBuf_threshold);
    }
    if (pComponentPrivate->pLcmlHandle != NULL) {
        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle)->pCodecinterfacehandle,
                               EMMCodecControlDestroy, NULL);

        if (eError != OMX_ErrorNone){
            iLBCDEC_DPRINT("%d : Error: in Destroying the codec: no.  %x\n",__LINE__, eError);
        }
        dlclose(pComponentPrivate->ptrLibLCML);
        pComponentPrivate->ptrLibLCML = NULL;
        iLBCDEC_DPRINT("%d :: Closed LCML \n",__LINE__);
        pComponentPrivate->pLcmlHandle = NULL;
    }

    return eError;
}
/**
 *
 */
OMX_ERRORTYPE iLBCDEC_CleanupInitParams(OMX_HANDLETYPE pComponent)
{
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = 
        (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    iLBCD_LCML_BUFHEADERTYPE *pTemp_lcml = NULL;    

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U16 i=0;
    LCML_DSP_INTERFACE *pLcmlHandle = NULL;
    LCML_DSP_INTERFACE *pLcmlHandleAux = NULL;
    
    iLBCDEC_DPRINT ("%d :: %s\n", __LINE__,__FUNCTION__);

    OMX_MEMFREE_STRUCT(pComponentPrivate->strmAttr);
   
    OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->pParams, iLBCD_AudioCodecParams);

    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[iLBCD_INPUT_PORT];

    for(i=0; i<pComponentPrivate->nRuntimeInputBuffers; i++) {
        if(pTemp_lcml->pFrameParam != NULL) {             
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
            pLcmlHandleAux = (LCML_DSP_INTERFACE *)
                (((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);

            OMX_DmmUnMap(pLcmlHandleAux->dspCodec->hProc,
                         (void*)pTemp_lcml->pBufferParam->pParamElem,
                         pTemp_lcml->pDmmBuf->pReserved);
                               
            OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pFrameParam, OMX_U8);
        }

        OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pBufferParam, iLBCDEC_ParamStruct);
        OMX_MEMFREE_STRUCT(pTemp_lcml->pDmmBuf);

        pTemp_lcml++;
    }

    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[iLBCD_OUTPUT_PORT];

    for(i=0; i<pComponentPrivate->nRuntimeOutputBuffers; i++){
        if(pTemp_lcml->pFrameParam!=NULL){
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
            pLcmlHandleAux = (LCML_DSP_INTERFACE *)
                (((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);
            pTemp_lcml->pFrameParam = NULL;                     
        }
    
        OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pBufferParam, iLBCDEC_ParamStruct);
        OMX_MEMFREE_STRUCT(pTemp_lcml->pDmmBuf);

        pTemp_lcml++;
    }   


    OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[iLBCD_INPUT_PORT]);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[iLBCD_OUTPUT_PORT]);

    return eError;
}

/* ========================================================================== */
/**
* @iLBCDEC_StopComponentThread() This function is called by the component 
* during de-init to close component thread, Command pipe, data pipe & LCML pipe.
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
OMX_ERRORTYPE iLBCDEC_StopComponentThread(OMX_HANDLETYPE pComponent)
{
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = 
        (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE threadError = OMX_ErrorNone;
    OMX_S16 pthreadError = 0;

    /*Join the component thread */
    pComponentPrivate->bIsStopping = 1;
    pthreadError = pthread_join (pComponentPrivate->ComponentThread,
                                 (void*)&threadError);
    if (0 != pthreadError) {
        eError = OMX_ErrorHardware;
    }

    /*Check for the errors */
    if (OMX_ErrorNone != threadError && OMX_ErrorNone != eError) {
        eError = OMX_ErrorInsufficientResources;
        iLBCDEC_EPRINT ("%d :: %s :: Error while closing Component Thread\n",
                       __LINE__,__FUNCTION__);
    }
    return eError;
}

/* ========================================================================== */
/**
* @iLBCDECHandleCommand() This function is called by the component when ever it
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
OMX_U32 iLBCDEC_HandleCommand (iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_COMPONENTTYPE *pHandle = NULL;
    OMX_COMMANDTYPE command;
    OMX_STATETYPE commandedState = OMX_StateInvalid;
    OMX_U32 commandData = 0;
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;

    OMX_U16 i = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMX_U32 nBuf = 0;
    OMX_U16 arr[100] = {0};
    char *p = "";
    LCML_CALLBACKTYPE cb;
    LCML_DSP *pLcmlDsp = NULL;
    ssize_t ret = 0;
    iLBCD_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;
    int inputPortFlag=0,outputPortFlag=0;

#ifdef RESOURCE_MANAGER_ENABLED
    OMX_ERRORTYPE rm_error = OMX_ErrorNone;
#endif

    pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;

    iLBCDEC_DPRINT ("%d :: %s curState = %d\n",  __LINE__,__FUNCTION__,pComponentPrivate->curState);
    iLBCDEC_DPRINT ("%d :: %s :: pComponentPrivate = %p\n",__LINE__, __FUNCTION__,pComponentPrivate);
    iLBCDEC_DPRINT ("%d :: %s :: pHandle = %p\n",__LINE__, __FUNCTION__,pHandle);

    ret = read (pComponentPrivate->cmdPipe[0], &command, sizeof (command));
    if (ret == -1){
        iLBCDEC_EPRINT ("%d :: %s OMX_ErrorHardware\n",  __LINE__,__FUNCTION__);
        return (OMX_ErrorHardware);
    }

    ret = read (pComponentPrivate->cmdDataPipe[0], &commandData, sizeof (commandData));
    if (ret == -1){
        iLBCDEC_EPRINT ("%d :: %s OMX_ErrorHardware\n",  __LINE__,__FUNCTION__);
        return (OMX_ErrorHardware);
    }

    if (command == OMX_CommandStateSet) {
        commandedState = (OMX_STATETYPE)commandData;
        if (pComponentPrivate->curState == commandedState) {
            pComponentPrivate->cbInfo.EventHandler (pHandle, pHandle->pApplicationPrivate,
                                                    OMX_EventError, 
                                                    OMX_ErrorSameState,0,
                                                    NULL);
            iLBCDEC_DPRINT(":: Error: Same State Given by Application\n");
        }else{
            switch(commandedState) {
            case OMX_StateIdle:
                if (pComponentPrivate->curState == OMX_StateLoaded || pComponentPrivate->curState == OMX_StateWaitForResources){
                    if (pComponentPrivate->dasfmode == 1) {
                        pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled= FALSE;
                        pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bPopulated= FALSE;
                        if(pComponentPrivate->streamID == 0) { 
                            iLBCDEC_DPRINT("**************************************\n"); 
                            iLBCDEC_DPRINT(":: Error = OMX_ErrorInsufficientResources\n"); 
                            iLBCDEC_DPRINT("**************************************\n"); 
                            eError = OMX_ErrorInsufficientResources; 
                            pComponentPrivate->curState = OMX_StateInvalid; 
                            pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                                   pHandle->pApplicationPrivate, 
                                                                   OMX_EventError, OMX_ErrorInvalidState,
                                                                   0, 
                                                                   NULL);
                            goto EXIT; 
                        }
                    }
                    
                    if (pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bPopulated &&  
                        pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bEnabled)  {
                        inputPortFlag = 1;
                    }

                    if (!pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bPopulated && 
                        !pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bEnabled) {
                        inputPortFlag = 1;
                    }

                    if (pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bPopulated && 
                        pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled) {
                        outputPortFlag = 1;
                    }

                    if (!pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bPopulated && 
                        !pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled) {
                        outputPortFlag = 1;
                    }

                    if(!(inputPortFlag && outputPortFlag)) {
                        pComponentPrivate->InLoaded_readytoidle = 1;
                        pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex); 
                        pthread_cond_wait(&pComponentPrivate->InLoaded_threshold, 
                                          &pComponentPrivate->InLoaded_mutex);
                        pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
                    }
 
                    iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
                    cb.LCML_Callback = (void *) iLBCDEC_LCML_Callback;
                    pLcmlHandle = (OMX_HANDLETYPE) 
                        iLBCDEC_GetLCMLHandle(pComponentPrivate);
                    if (pLcmlHandle == NULL) {
                        iLBCDEC_EPRINT("%d :: %s :: LCML Handle is NULL........exiting..\n",__LINE__,__FUNCTION__);
                        goto EXIT;
                    }

                    iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
                    iLBCDEC_DPRINT("%d :: %s :: pLcmlHandle = %p\n",
                                  __LINE__,__FUNCTION__,pLcmlHandle);

                    /* Got handle of dsp via phandle filling information about
                       DSP specific things */
                    pLcmlDsp = (((LCML_DSP_INTERFACE*)pLcmlHandle)->dspCodec);
                    iLBCDEC_DPRINT("%d :: %s :: pLcmlDsp = %p\n",
                                  __LINE__,__FUNCTION__,pLcmlDsp);

                    iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
                    eError = iLBCDEC_Fill_LCMLInitParams(pHandle,pLcmlDsp,arr);
                    if(eError != OMX_ErrorNone) {
                        iLBCDEC_EPRINT("%d :: %s :: Error returned from iLBCDEC_Fill_LCMLInitParams()\n",__LINE__, __FUNCTION__);
                        goto EXIT;
                    }
                    iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
#ifdef RESOURCE_MANAGER_ENABLED
                    /* need check the resource with RM */
                    iLBCDEC_DPRINT("%d %s Calling RMProxy_NewSendCommand\n", __LINE__,__FUNCTION__);
                    pComponentPrivate->rmproxyCallback.RMPROXY_Callback = 
                                        (void *) iLBCD_ResourceManagerCallback;
                    if (pComponentPrivate->curState != OMX_StateWaitForResources) {
                        rm_error = RMProxy_NewSendCommand(pHandle, 
                                                       RMProxy_RequestResource, 
                                                       OMX_ILBC_Decoder_COMPONENT,
                                                       iLBCDEC_CPU_LOAD,
                                                       3456,
                                                       &(pComponentPrivate->rmproxyCallback));
                        if (rm_error == OMX_ErrorInsufficientResources) {
                            /* resource is not available, need set state to 
                               OMX_StateWaitForResources */
                            iLBCDEC_DPRINT("%d :: %s Resource not available, set component to OMX_StateWaitForResources\n", __LINE__,__FUNCTION__);
                            pComponentPrivate->curState = OMX_StateWaitForResources;
                            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                                   pHandle->pApplicationPrivate,
                                                                   OMX_EventCmdComplete,
                                                                   OMX_CommandStateSet,
                                                                   pComponentPrivate->curState,
                                                                   NULL);
                            return rm_error;
                        }
                        else if (rm_error != OMX_ErrorNone) {
                            /* resource is not available, need set state to
                               OMX_StateInvalid */
                            iLBCDEC_DPRINT("%d :: %s Resource not available, set component to OMX_StateInvalid\n", __LINE__,__FUNCTION__);
                            eError = OMX_ErrorInsufficientResources;
                            pComponentPrivate->curState = OMX_StateInvalid;
                            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                                   pHandle->pApplicationPrivate,
                                                                   OMX_EventError,
                                                                   eError,
                                                                   OMX_TI_ErrorSevere,
                                                                   NULL);
                            return eError;
                        }
                    }
#endif
                    iLBCDEC_DPRINT ("%d :: %s\n",__LINE__,__FUNCTION__);
                    pComponentPrivate->pLcmlHandle = (LCML_DSP_INTERFACE *)pLcmlHandle;
                    /*filling create phase params */
                    /*cb.LCML_Callback = (void *) iLBCDECLCML_Callback;*/
                    iLBCDEC_DPRINT("%d :: %s :: Calling LCML_InitMMCodec...\n", __LINE__,__FUNCTION__);

                    /* TeeDN will be default for decoder component */
                    iLBCDEC_DPRINT("%d :: %s :: iLBC decoder support TeeDN\n", __LINE__,__FUNCTION__);
                    eError = LCML_InitMMCodecEx(((LCML_DSP_INTERFACE *)
                                                 pLcmlHandle)->pCodecinterfacehandle,
                                                p,&pLcmlHandle,(void *)p,&cb,
                                                (OMX_STRING)pComponentPrivate->sDeviceString);

                    if(eError != OMX_ErrorNone) {
                        iLBCDEC_EPRINT("%d :: %s :: Error returned from LCML_Init function\n",__LINE__,__FUNCTION__);
                        iLBCDEC_FatalErrorRecover(pComponentPrivate);
                        goto EXIT;
                    }
#ifdef RESOURCE_MANAGER_ENABLED
                    /* resource is available */
                    iLBCDEC_DPRINT("%d %s Resource available, set component to OMX_StateIdle\n",__LINE__,__FUNCTION__);
                    rm_error = RMProxy_NewSendCommand(pHandle,
                                                      RMProxy_StateSet,
                                                      OMX_ILBC_Decoder_COMPONENT,
                                                      OMX_StateIdle,
                                                      3456,
                                                      NULL);
#endif
                    iLBCDEC_DPRINT("%d :: %s :: Setting to OMX_StateIdle\n", __LINE__,__FUNCTION__);
                    pComponentPrivate->curState = OMX_StateIdle;
                    pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                           pHandle->pApplicationPrivate,
                                                           OMX_EventCmdComplete, 
                                                           OMX_CommandStateSet,
                                                           pComponentPrivate->curState,
                                                           NULL);

                    if(pComponentPrivate->dasfmode == 1) {
                        OMX_U32 pValues[4];
                        iLBCDEC_DPRINT("%d :: %s DASF Functionality is ON ---\n",
                                      __LINE__,__FUNCTION__);
                    
                        if(pComponentPrivate->streamID == 0) { 
                            iLBCDEC_EPRINT("**************************************\n");
                            iLBCDEC_EPRINT("Error = OMX_ErrorInsufficientResources\n");
                            iLBCDEC_EPRINT("**************************************\n");
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

                        OMX_MALLOC_SIZE_DSPALIGN(pComponentPrivate->pParams,
                                                 sizeof(iLBCD_AudioCodecParams),
                                                 iLBCD_AudioCodecParams);
                        if (pComponentPrivate->pParams == NULL) {
                            iLBCDEC_EPRINT ("%d :: %s OMX_ErrorInsufficientResources\n",  __LINE__,__FUNCTION__);
                            return OMX_ErrorInsufficientResources;
                        }
                        /* Configure audio codec params */
                        pComponentPrivate->pParams->unAudioFormat = 1; /* iLBC mono */
                        pComponentPrivate->pParams->ulSamplingFreq =
                            pComponentPrivate->pcmParams->nSamplingRate;
                        pComponentPrivate->pParams->unUUID = pComponentPrivate->streamID;

                        pValues[0] = USN_STRMCMD_SETCODECPARAMS;
                        pValues[1] = (OMX_U32)(pComponentPrivate->pParams);
                        pValues[2] = sizeof(iLBCD_AudioCodecParams);
                        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                   EMMCodecControlStrmCtrl,(void *)pValues);
                        if (eError != OMX_ErrorNone) {
                            iLBCDEC_EPRINT("%d :: %s :: Error Occurred in CodecStreamControl..\n",__LINE__,__FUNCTION__);
                            goto EXIT;
                        }
                    }
                } 
                else if (pComponentPrivate->curState == OMX_StateExecuting) {
                    char *pArgs = "";
                    /*Set the bIsStopping bit */
                    iLBCDEC_DPRINT("%d :: %s About to set bIsStopping bit\n", __LINE__,__FUNCTION__);
                    iLBCDEC_DPRINT("%d :: %s :: About to call LCML_ControlCodec(STOP)\n",__LINE__,__FUNCTION__);
                
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)
                                                pLcmlHandle)->pCodecinterfacehandle,
                                               MMCodecControlStop,(void*)pArgs);
                    if(eError != OMX_ErrorNone) {
                        iLBCDEC_EPRINT("%d :: %s :: Error Occurred in Codec Stop..\n",__LINE__,__FUNCTION__);
                        goto EXIT;
                    }
                    pComponentPrivate->bStopSent=1;
                    OMX_MEMFREE_STRUCT(pComponentPrivate->pHoldBuffer);
                    pComponentPrivate->nHoldLength = 0;
                } 
                else if(pComponentPrivate->curState == OMX_StatePause) {
                    iLBCDEC_DPRINT("%d :: %s :: Comp: Stop Command Received\n",
                                  __LINE__,__FUNCTION__);
                    iLBCDEC_DPRINT("%d :: %s :: Setting to OMX_StateIdle\n",
                                  __LINE__,__FUNCTION__);

#ifdef RESOURCE_MANAGER_ENABLED
                    rm_error = RMProxy_NewSendCommand(pHandle,
                                                   RMProxy_StateSet,
                                                   OMX_ILBC_Decoder_COMPONENT,
                                                   OMX_StateIdle,
                                                   NEWSENDCOMMAND_MEMORY,
                                                   NULL);                    
#endif
                    iLBCDEC_DPRINT ("%d :: %s :: The component is stopped\n",
                                   __LINE__,__FUNCTION__);
                    pComponentPrivate->curState = OMX_StateIdle;
                    pComponentPrivate->cbInfo.EventHandler (pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventCmdComplete,
                                                            OMX_CommandStateSet,
                                                            pComponentPrivate->curState,
                                                            NULL);
                }
                else {
                    /* This means, it is invalid state from application */
                    iLBCDEC_DPRINT("%d :: %s \n",__LINE__,__FUNCTION__);
                    pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                           pHandle->pApplicationPrivate,
                                                           OMX_EventError,
                                                           OMX_ErrorIncorrectStateTransition,
                                                           0,
                                                           NULL);
                }
                break;

            case OMX_StateExecuting:
                iLBCDEC_DPRINT("%d :: %s :: Cmd Executing \n",__LINE__,
                              __FUNCTION__);            
                if (pComponentPrivate->curState == OMX_StateIdle) {
                    iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
                    /* Sending commands to DSP via LCML_ControlCodec third 
                       argument is not used for time being */
                    pComponentPrivate->nFillBufferDoneCount = 0;  
                    pComponentPrivate->bStopSent=0;
                    pComponentPrivate->nEmptyBufferDoneCount = 0;  
                    pComponentPrivate->nEmptyThisBufferCount = 0;
                    pComponentPrivate->nFillBufferDoneCount =0;
 
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)
                                                pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStart, (void *)p);
                    if(eError != OMX_ErrorNone) {
                        iLBCDEC_EPRINT("%d :: %s :: Error Occurred in Codec Start..\n",__LINE__,__FUNCTION__);
                        goto EXIT;
                    }
                    /* Send input buffers to application */
                    nBuf = pComponentPrivate->pInputBufferList->numBuffers;
                    iLBCDEC_DPRINT ("%d :: %s :: nBuf =  %ld\n", __LINE__,__FUNCTION__,nBuf);
                    /* Send output buffers to codec */
                }
                else if (pComponentPrivate->curState == OMX_StatePause) {
                    iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)
                                                pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStart,
                                               (void *)p);
                    if (eError != OMX_ErrorNone) {
                        iLBCDEC_EPRINT ("%d :: %s :: Error While Resuming the codec\n",__LINE__,__FUNCTION__);
                        goto EXIT;
                    }
        
                    if (pComponentPrivate->nNumInputBufPending < 
                        pComponentPrivate->pInputBufferList->numBuffers) {
                        pComponentPrivate->nNumInputBufPending = 
                            pComponentPrivate->pInputBufferList->numBuffers;
                    }

                    for (i=0; i < pComponentPrivate->nPendingOutPausedBufs ; i++) {
                        pComponentPrivate->nFillBufferDoneCount++;                    
                        pComponentPrivate->cbInfo.FillBufferDone(pHandle,
                                                                 pHandle->pApplicationPrivate,
                                                                 pComponentPrivate->pOutBufHdrWhilePaused[i]);
                        pComponentPrivate->lcml_nOpBuf--;
                        pComponentPrivate->app_nBuf++;
                        pComponentPrivate->nOutStandingFillDones--;
                    }
                    pComponentPrivate->nPendingOutPausedBufs = 0;

                    for (i=0; i < pComponentPrivate->nNumInputBufPending; i++) {
                        if (pComponentPrivate->pInputBufHdrPending[i]) {
                            iLBCDEC_GetCorresponding_LCMLHeader(pComponentPrivate,
                                                                pComponentPrivate->pInputBufHdrPending[i]->pBuffer,
                                                                OMX_DirInput,
                                                                &pLcmlHdr);
                            iLBCDEC_SetPending(pComponentPrivate,
                                                pComponentPrivate->pInputBufHdrPending[i],
                                                OMX_DirInput, 0);
                            eError = LCML_QueueBuffer(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                      EMMCodecInputBuffer,  
                                                      pComponentPrivate->pInputBufHdrPending[i]->pBuffer,
                                                      pComponentPrivate->pInputBufHdrPending[i]->nAllocLen,
                                                      pComponentPrivate->pInputBufHdrPending[i]->nFilledLen,
                                                      (OMX_U8 *) pLcmlHdr->pBufferParam,
                                                      sizeof(iLBCDEC_ParamStruct),
                                                      NULL);
                            if (eError != OMX_ErrorNone) {
                                iLBCDEC_EPRINT("%d :: %s Queue Input buffer, error\n", __LINE__, __FUNCTION__);
                                iLBCDEC_FatalErrorRecover(pComponentPrivate);
                                goto EXIT;
                            }
                        }
                    }
                    pComponentPrivate->nNumInputBufPending = 0;

                    if (pComponentPrivate->nNumOutputBufPending < 
                        pComponentPrivate->pOutputBufferList->numBuffers){
                        pComponentPrivate->nNumOutputBufPending = 
                            pComponentPrivate->pOutputBufferList->numBuffers;
                    }

                    for (i=0; i < pComponentPrivate->nNumOutputBufPending; i++){
                        if (pComponentPrivate->pOutputBufHdrPending[i]) {
                            iLBCDEC_GetCorresponding_LCMLHeader(pComponentPrivate,
                                                                pComponentPrivate->pOutputBufHdrPending[i]->pBuffer,
                                                                OMX_DirOutput,
                                                                &pLcmlHdr);
                            iLBCDEC_SetPending(pComponentPrivate,
                                                pComponentPrivate->pOutputBufHdrPending[i],
                                                OMX_DirOutput, 0);
                            eError = LCML_QueueBuffer(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                      EMMCodecOuputBuffer,  
                                                      pComponentPrivate->pOutputBufHdrPending[i]->pBuffer,
                                                      pComponentPrivate->pOutputBufHdrPending[i]->nAllocLen,
                                                      pComponentPrivate->pOutputBufHdrPending[i]->nFilledLen,
                                                      (OMX_U8 *) pLcmlHdr->pBufferParam,
                                                      sizeof(iLBCDEC_ParamStruct),
                                                      NULL);
                            if (eError != OMX_ErrorNone) {
                                iLBCDEC_EPRINT("%d :: %s Queue Output buffer, error\n", __LINE__, __FUNCTION__);
                                iLBCDEC_FatalErrorRecover(pComponentPrivate);
                                goto EXIT;
                            }
                        }
                    
                    }
                    pComponentPrivate->nNumOutputBufPending = 0;
                }
                else {
                    iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
                    pComponentPrivate->cbInfo.EventHandler (pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventError,
                                                            OMX_ErrorIncorrectStateTransition,
                                                            0,
                                                            NULL);
                    iLBCDEC_EPRINT("%d :: %s :: Error: Invalid State Given by Application\n",__LINE__,__FUNCTION__);
                    goto EXIT;

                }
#ifdef RESOURCE_MANAGER_ENABLED
                rm_error = RMProxy_NewSendCommand(pHandle, 
                                               RMProxy_StateSet,
                                               OMX_ILBC_Decoder_COMPONENT,
                                               OMX_StateExecuting,
                                               NEWSENDCOMMAND_MEMORY,
                                               NULL);
#endif
                pComponentPrivate->curState = OMX_StateExecuting;
                /*Send state change notificaiton to Application */
                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandStateSet,
                                                       pComponentPrivate->curState, 
                                                       NULL);

                break;

            case OMX_StateLoaded:
                iLBCDEC_DPRINT("%d :: %s Cmd Loaded - curState = %d\n", __LINE__,__FUNCTION__,pComponentPrivate->curState);
                iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->pInputBufferList->numBuffers = %d\n",__LINE__, __FUNCTION__,pComponentPrivate->pInputBufferList->numBuffers);
                iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->pOutputBufferList->numBuffers = %d\n",__LINE__, __FUNCTION__,pComponentPrivate->pOutputBufferList->numBuffers);

                if (pComponentPrivate->curState == OMX_StateWaitForResources){
                    iLBCDEC_DPRINT("%d :: %s Cmd Loaded\n", __LINE__,__FUNCTION__);
                    pComponentPrivate->curState = OMX_StateLoaded;
                    pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                           pHandle->pApplicationPrivate,
                                                           OMX_EventCmdComplete,
                                                           OMX_CommandStateSet,
                                                           pComponentPrivate->curState,
                                                           NULL);
                }
                iLBCDEC_DPRINT("%d :: %s :: In side OMX_StateLoaded State: \n", __LINE__,__FUNCTION__);
                if (pComponentPrivate->curState != OMX_StateIdle &&
                    pComponentPrivate->curState != OMX_StateWaitForResources) {
                    iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
                    pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                           pHandle->pApplicationPrivate,
                                                           OMX_EventError,
                                                           OMX_ErrorIncorrectStateTransition,
                                                           0,
                                                           NULL);
                    iLBCDEC_DPRINT("%d :: %s :: Error: Invalid State Given by Application\n",__LINE__,__FUNCTION__);
                    goto EXIT;
                }

                iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->pInputBufferList->numBuffers = %d\n",__LINE__, __FUNCTION__,pComponentPrivate->pInputBufferList->numBuffers);
                iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->pOutputBufferList->numBuffers = %d\n",__LINE__, __FUNCTION__,pComponentPrivate->pOutputBufferList->numBuffers);

                if (pComponentPrivate->pInputBufferList->numBuffers ||
                    pComponentPrivate->pOutputBufferList->numBuffers) {

                    pComponentPrivate->InIdle_goingtoloaded = 1;
                    pthread_mutex_lock(&pComponentPrivate->InIdle_mutex); 
                    pthread_cond_wait(&pComponentPrivate->InIdle_threshold,
                                      &pComponentPrivate->InIdle_mutex);
                    pthread_mutex_unlock(&pComponentPrivate->InIdle_mutex);
                }

                /* Now Deinitialize the component No error should be returned 
                   from this function. It should clean the system as much as 
                   possible */
                iLBCDEC_DPRINT("%d :: %s :: In side OMX_StateLoaded State: \n",
                              __LINE__,__FUNCTION__);

                iLBCDEC_CleanupInitParams(pComponentPrivate->pHandle);
            
                if (pLcmlHandle != NULL) {
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)
                                            pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlDestroy,
                                           (void *)p);      
                    if (eError != OMX_ErrorNone) {
                        iLBCDEC_DPRINT("%d :: %s :: Error: in Destroying the codec: no.  %x\n",__LINE__, __FUNCTION__,eError);
                        goto EXIT;
                    }
                    else{
                        dlclose(pComponentPrivate->ptrLibLCML);
                        pComponentPrivate->ptrLibLCML = NULL;
                        iLBCDEC_DPRINT("%d :: Closed LCML \n",__LINE__);
                        pLcmlHandle = NULL;
                        pComponentPrivate->pLcmlHandle = NULL;
                    }
                }
                /*Closing LCML Lib*/
                iLBCDEC_DPRINT("%d :: %s :: In side OMX_StateLoaded State: \n", __LINE__,__FUNCTION__);
                iLBCDEC_DPRINT("%d :: %s: Cmd Loaded\n",__LINE__,__FUNCTION__);
                eError = EXIT_COMPONENT_THRD;
                pComponentPrivate->bInitParamsInitialized = 0;
                pComponentPrivate->bLoadedCommandPending = OMX_FALSE;

                break;

            case OMX_StatePause:
                iLBCDEC_DPRINT("%d :: %s Cmd Pause\n",__LINE__,__FUNCTION__);
                if (pComponentPrivate->curState != OMX_StateExecuting && pComponentPrivate->curState != OMX_StateIdle) {
                    iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
                    pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                           pHandle->pApplicationPrivate,
                                                           OMX_EventError,
                                                           OMX_ErrorIncorrectStateTransition,
                                                           0,
                                                           NULL);
                    iLBCDEC_EPRINT("%d :: %s :: Error: Invalid State Given by Application\n",__LINE__,__FUNCTION__);
                    goto EXIT;
                }
                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlPause,
                                           (void *)p);
                if (eError != OMX_ErrorNone) {
                    iLBCDEC_EPRINT("%d :: %s :: Error: in Pausing the codec\n",
                                  __LINE__,__FUNCTION__);
                    goto EXIT;
                }

                iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
                break;

            case OMX_StateWaitForResources:
                if (pComponentPrivate->curState == OMX_StateLoaded) {
                    iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
                    pComponentPrivate->curState = OMX_StateWaitForResources;
                    pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                           pHandle->pApplicationPrivate,
                                                           OMX_EventCmdComplete,
                                                           OMX_CommandStateSet,
                                                           pComponentPrivate->curState,
                                                           NULL);
                }
                else{
                    iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
                    pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                           OMX_EventError,
                                                           OMX_ErrorIncorrectStateTransition,
                                                           0,
                                                           NULL);
                }
                break;

            case OMX_StateInvalid:
                iLBCDEC_DPRINT("%d :: %s Cmd OMX_StateInvalid:\n", __LINE__,__FUNCTION__);
                if (pComponentPrivate->curState != OMX_StateWaitForResources && 
                    pComponentPrivate->curState != OMX_StateLoaded) {
                    iLBCDEC_DPRINT("%d :: %s\n",__LINE__,__FUNCTION__);
                    if (pLcmlHandle != NULL) {
                        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlDestroy,
                                               (void *)p);
                        dlclose(pComponentPrivate->ptrLibLCML);
                        pComponentPrivate->ptrLibLCML = NULL;
                        iLBCDEC_DPRINT("%d :: Closed LCML \n",__LINE__);
                        pLcmlHandle = NULL;
                        pComponentPrivate->pLcmlHandle = NULL;
                    }
                } 
                pComponentPrivate->curState = OMX_StateInvalid;
                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventError,
                                                       OMX_ErrorInvalidState,
                                                       0,
                                                       NULL);
                iLBCDEC_CleanupInitParams(pHandle);
                break;

            case OMX_StateMax:
                iLBCDEC_DPRINT("%d :: %s: Cmd OMX_StateMax::\n",
                              __LINE__,__FUNCTION__);
                break;

            default:
                break;
            } /* End of Switch */
        }
    }
    else if (command == OMX_CommandMarkBuffer) {
        iLBCDEC_DPRINT("%d :: %s :Command OMX_CommandMarkBuffer received\n", __LINE__,__FUNCTION__);
        if(!pComponentPrivate->pMarkBuf){
            /* TODO Need to handle multiple marks */
            pComponentPrivate->pMarkBuf = (OMX_MARKTYPE *)(commandData);
        }
    }
    else if (command == OMX_CommandPortDisable) {
        iLBCDEC_DPRINT("%d :: %s ::\n",__LINE__,__FUNCTION__);
        
        if (!pComponentPrivate->bDisableCommandPending) { 
            if(commandData == 0x0 || commandData == OMX_ALL){   /*Input*/
                /* disable port */
                pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bEnabled = OMX_FALSE;
                for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
                    if (iLBCDEC_IsPending(pComponentPrivate,
                                           pComponentPrivate->pInputBufferList->pBufHdr[i],
                                           OMX_DirInput)) {
                        /* Real solution is flush buffers from DSP.  Until 
                           we have the ability to do that 
                           we just call EmptyBufferDone() on any pending
                           buffers */
                        iLBCDEC_ClearPending(pComponentPrivate,
                                              pComponentPrivate->pInputBufferList->pBufHdr[i],
                                              OMX_DirInput);
                        pComponentPrivate->nEmptyBufferDoneCount++;  
                        pComponentPrivate->cbInfo.EmptyBufferDone (pComponentPrivate->pHandle,
                                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                                   pComponentPrivate->pInputBufferList->pBufHdr[i]);
                    }
                }           
            }
            if(commandData == 0x1 || commandData == OMX_ALL){      /*Output*/
                char *pArgs = "";
                pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled = OMX_FALSE;
                if (pComponentPrivate->curState == OMX_StateExecuting) {
                    pComponentPrivate->bNoIdleOnStop = OMX_TRUE;
                    iLBCDEC_DPRINT("%d :: %s :: Calling LCML_ControlCodec()\n",
                            __LINE__,__FUNCTION__);
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               MMCodecControlStop,
                                               (void *)pArgs);
                }
            }
        }
        
        if(commandData == 0x0) {
            if(!pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bPopulated){
                /* return cmdcomplete event if input unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortDisable,
                                                       iLBCD_INPUT_PORT,
                                                       NULL);
                iLBCDEC_DPRINT("%d :: %s :: Clearing bDisableCommandPending\n", __LINE__,__FUNCTION__);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else{
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

        if(commandData == 0x1) {
            if (!pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bPopulated){
                /* return cmdcomplete event if output unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortDisable,iLBCD_OUTPUT_PORT,
                                                       NULL);
                iLBCDEC_DPRINT("%d :: %s :: Clearing bDisableCommandPending\n", __LINE__,__FUNCTION__);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

        if(commandData == OMX_ALL) {
            if (!pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bPopulated && 
                !pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bPopulated){

                /* return cmdcomplete event if inout & output unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortDisable,
                                                       iLBCD_INPUT_PORT,
                                                       NULL);

                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortDisable,
                                                       iLBCD_OUTPUT_PORT,
                                                       NULL);
                iLBCDEC_DPRINT("%d :: %s :: Clearing bDisableCommandPending\n", __LINE__,__FUNCTION__);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }
    }
    else if (command == OMX_CommandPortEnable) {
        if(commandData == 0x0 || commandData == OMX_ALL){
            /* enable in port */
            iLBCDEC_DPRINT("%d :: %s :: setting input port to enabled\n", __LINE__,__FUNCTION__);
            pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bEnabled = OMX_TRUE;
            if(pComponentPrivate->AlloBuf_waitingsignal){
                pComponentPrivate->AlloBuf_waitingsignal = 0;
                pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
                pthread_cond_signal(&pComponentPrivate->AlloBuf_threshold);
                pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);    
            }

            iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->pPortDef[iLBCDEC_INPUT_PORT]->bEnabled = %d\n",__LINE__,__FUNCTION__, pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bEnabled);
        }
        if(commandData == 0x1 || commandData == OMX_ALL){
            /* enable out port */
            if(pComponentPrivate->AlloBuf_waitingsignal) {
                pComponentPrivate->AlloBuf_waitingsignal = 0;
                pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
                pthread_cond_signal(&pComponentPrivate->AlloBuf_threshold);
                pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);    
            }

            if (pComponentPrivate->curState == OMX_StateExecuting) {
                char *pArgs = "";
                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlStart,
                                           (void *)pArgs);
            }
            iLBCDEC_DPRINT("%d :: %s :: setting output port to enabled\n", __LINE__,__FUNCTION__);
            pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled = OMX_TRUE;
            iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->pPortDef[iLBCDEC_OUTPUT_PORT]->bEnabled = %d\n",__LINE__,__FUNCTION__, pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bEnabled);
        }

        while (1) {
            if(commandData == 0x0 && 
               (pComponentPrivate->curState == OMX_StateLoaded || 
                pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bPopulated)){

                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortEnable,
                                                       iLBCD_INPUT_PORT,
                                                       NULL);
                break;
            }
            else if(commandData == 0x1 && 
                    (pComponentPrivate->curState == OMX_StateLoaded || 
                     pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bPopulated)){

                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortEnable,
                                                       iLBCD_OUTPUT_PORT,
                                                       NULL);
                break;
            }
            else if(commandData == OMX_ALL &&
                    (pComponentPrivate->curState == OMX_StateLoaded || 
                     (pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bPopulated 
                      && pComponentPrivate->pPortDef[iLBCD_OUTPUT_PORT]->bPopulated))){

                pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortEnable,
                                                       iLBCD_INPUT_PORT,
                                                       NULL);

                pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortEnable,
                                                       iLBCD_OUTPUT_PORT,
                                                       NULL);

                iLBCDECFill_LCMLInitParamsEx(pComponentPrivate->pHandle);

            }
        }
    }
    else if (command == OMX_CommandFlush) {
        if(commandData == 0x0 || commandData == OMX_ALL){
            for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
                iLBCDEC_DPRINT("%d :: %s :: Calling EmptyBufferDone\n",__LINE__,
                        __FUNCTION__);
                pComponentPrivate->nEmptyBufferDoneCount++;   
                pComponentPrivate->cbInfo.EmptyBufferDone (pComponentPrivate->pHandle,
                                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                                           pComponentPrivate->pInputBufferList->pBufHdr[i]);
                pComponentPrivate->nNumInputBufPending = 0;
            }

            /* return all input buffers */
            pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete,
                                                   OMX_CommandFlush,
                                                   iLBCD_INPUT_PORT,
                                                   NULL);
        }
        if(commandData == 0x1 || commandData == OMX_ALL){

            /* return all output buffers */
            for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
                iLBCDEC_DPRINT("%d :: %s :: Calling FillBufferDone From\n",
                              __LINE__,__FUNCTION__);
                pComponentPrivate->nFillBufferDoneCount++;
                pComponentPrivate->cbInfo.FillBufferDone (pComponentPrivate->pHandle,
                                                          pComponentPrivate->pHandle->pApplicationPrivate,
                                                          pComponentPrivate->pOutputBufferList->pBufHdr[i]);
                pComponentPrivate->nNumOutputBufPending = 0;
            }

            pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete,
                                                   OMX_CommandFlush,
                                                   iLBCD_OUTPUT_PORT,
                                                   NULL);
        }
    }
EXIT:
    if (eError != OMX_ErrorNone) {
        OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->pParams, iLBCD_AudioCodecParams);
    }
    iLBCDEC_DPRINT ("%d :: %s :: Exiting\n",__LINE__,__FUNCTION__);
    iLBCDEC_DPRINT ("%d :: %s :: Returning %d\n",__LINE__,__FUNCTION__,eError);
    return eError;
}


/* ========================================================================== */
/**
* @iLBCDEC_HandleDataBuf_FromApp() This function is called by the component when ever it
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
OMX_ERRORTYPE iLBCDEC_HandleDataBuf_FromApp(OMX_BUFFERHEADERTYPE* pBufHeader,
                                    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_DIRTYPE eDir;
    iLBCD_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;
    LCML_DSP_INTERFACE *pLcmlHandle = (LCML_DSP_INTERFACE *)
        pComponentPrivate->pLcmlHandle;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDefIn = NULL;

    OMX_U16 i = 0;
    OMX_U8 nFrames = 0;
    OMX_U8 *frameType = NULL;
    LCML_DSP_INTERFACE * phandle = NULL;
    OMX_U8 bufSize=0;

    OMX_U16 iLBCcodecType = pComponentPrivate->iLBCcodecType;
    bufSize = STD_iLBCDEC_BUF_SIZE;
    if (iLBCcodecType == 1) {
        bufSize = INPUT_iLBCDEC_SECBUF_SIZE;
    }
    
    
    iLBCDEC_DPRINT ("%d :: %s Entering\n",__LINE__,__FUNCTION__);

    /*Find the direction of the received buffer from buffer list */
    eError = iLBCDEC_GetBufferDirection(pBufHeader, &eDir);
    if (eError != OMX_ErrorNone) {
        iLBCDEC_DPRINT ("%d :: %s :: The pBufHeader is not found in the list\n",
                       __LINE__,__FUNCTION__);
        goto EXIT;
    }   
    
    if (eDir == OMX_DirInput) {
        pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
        pPortDefIn = pComponentPrivate->pPortDef[OMX_DirInput];

        eError = iLBCDEC_GetCorresponding_LCMLHeader(pComponentPrivate,
                                                     pBufHeader->pBuffer,
                                                     OMX_DirInput,
                                                     &pLcmlHdr);
        if (eError != OMX_ErrorNone) {
            iLBCDEC_EPRINT("%d :: %s :: Error: Invalid Buffer Came ...\n", __LINE__,__FUNCTION__);
            goto EXIT;
        }

        if (pBufHeader->nFilledLen > 0 || pBufHeader->nFlags == OMX_BUFFERFLAG_EOS) {
            phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec); 
            pComponentPrivate->bBypassDSP = 0;

            nFrames = (OMX_U8)(pBufHeader->nFilledLen / RTP_Framesize);
            frameType = pBufHeader->pBuffer;
            frameType += RTP_Framesize - 1;

            if( (pLcmlHdr->pBufferParam->usNbFrames < nFrames) && 
                (pLcmlHdr->pFrameParam != NULL)){
                OMX_DmmUnMap(phandle->dspCodec->hProc,
                             (void*)pLcmlHdr->pBufferParam->pParamElem,
                             pLcmlHdr->pDmmBuf->pReserved);
                pLcmlHdr->pBufferParam->pParamElem = NULL;

                OMX_MEMFREE_STRUCT_DSPALIGN(pLcmlHdr->pFrameParam, iLBCDEC_FrameStruct);
            }
            if(pLcmlHdr->pFrameParam == NULL ){
                OMX_MALLOC_SIZE_DSPALIGN(pLcmlHdr->pFrameParam,
                                         sizeof(iLBCDEC_FrameStruct)*nFrames,
                                         iLBCDEC_FrameStruct);
                if (pLcmlHdr->pFrameParam == NULL) {
                    iLBCDEC_EPRINT ("%d :: %s OMX_ErrorInsufficientResources\n",  __LINE__,__FUNCTION__);
                    return OMX_ErrorInsufficientResources;
                }
                eError = OMX_DmmMap(phandle->dspCodec->hProc, 
                                nFrames*sizeof(iLBCDEC_FrameStruct),
                                (void*)pLcmlHdr->pFrameParam, 
                                (pLcmlHdr->pDmmBuf));
                if (eError != OMX_ErrorNone){
                    iLBCDEC_EPRINT("%d :: %s :: Error: OMX_DmmMap.\n", __LINE__,__FUNCTION__);
                    iLBCDEC_FatalErrorRecover(pComponentPrivate);
                    goto EXIT;
                }
                pLcmlHdr->pBufferParam->pParamElem = 
                    (iLBCDEC_FrameStruct *)pLcmlHdr->pDmmBuf->pMapped;
            } 

            for(i=0;i<nFrames;i++){
                (pLcmlHdr->pFrameParam+i)->usLastFrame = 0;
                (pLcmlHdr->pFrameParam+i)->usFrameLost = 0;
/*                (pLcmlHdr->pFrameParam+i)->frameType = *(frameType + i*RTP_Framesize); <<<<<< 0j0 iLBC specific */
            }
            if(pBufHeader->nFlags == OMX_BUFFERFLAG_EOS) {
                (pLcmlHdr->pFrameParam+(nFrames-1))->usLastFrame = OMX_BUFFERFLAG_EOS;
                pComponentPrivate->bPlayCompleteFlag = 1;
                pBufHeader->nFlags = 0;
            }

            /* Store time stamp information */
            pComponentPrivate->arrBufIndex[pComponentPrivate->IpBufindex] = pBufHeader->nTimeStamp;
            pComponentPrivate->IpBufindex++;
            pComponentPrivate->IpBufindex %= pPortDefIn->nBufferCountActual;

            pLcmlHdr->pBufferParam->usNbFrames = nFrames;

            if (pComponentPrivate->curState == OMX_StateExecuting) {
                if (!iLBCDEC_IsPending(pComponentPrivate,
                                        pBufHeader,
                                        OMX_DirInput)) {
                    if(!pComponentPrivate->bDspStoppedWhileExecuting) {
                        iLBCDEC_SetPending(pComponentPrivate,pBufHeader,
                                            OMX_DirInput, 0);  
                        eError = LCML_QueueBuffer(pLcmlHandle->pCodecinterfacehandle,
                                                  EMMCodecInputBuffer,  
                                                  (OMX_U8 *)pBufHeader->pBuffer, 
                                                  bufSize*nFrames,
                                                  bufSize*nFrames,                                       
                                                  (OMX_U8 *) pLcmlHdr->pBufferParam,
                                                  sizeof(iLBCDEC_ParamStruct),
                                                  NULL);
                        if (eError != OMX_ErrorNone) {
                            iLBCDEC_EPRINT("%d :: %s Queue Output buffer, error\n", __LINE__, __FUNCTION__);
                            iLBCDEC_FatalErrorRecover(pComponentPrivate);
                            eError = OMX_ErrorHardware;
                            goto EXIT;
                        }
                        pComponentPrivate->lcml_nCntIp++;
                        pComponentPrivate->lcml_nIpBuf++;                   
                    }else {
                        iLBCDEC_DPRINT("Calling EmptyBufferDone from line %d\n",__LINE__);
                        pComponentPrivate->cbInfo.EmptyBufferDone (pComponentPrivate->pHandle,
                                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                                   pBufHeader);
                    }
                }
            }else {
                pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pBufHeader;
            }
        }else {
            pComponentPrivate->bBypassDSP = 1;
            iLBCDEC_DPRINT ("Forcing EmptyBufferDone\n");
            if(pComponentPrivate->dasfmode == 0) {
                pComponentPrivate->cbInfo.EmptyBufferDone (pComponentPrivate->pHandle,
                                                           pComponentPrivate->pHandle->pApplicationPrivate,
                                                           pComponentPrivate->pInputBufferList->pBufHdr[0]);
                pComponentPrivate->nEmptyBufferDoneCount++;
            }
        }
        if(pBufHeader->pMarkData){
            /* copy mark to output buffer header */ 
            if(pComponentPrivate->pOutputBufferList->pBufHdr[0]!= NULL){
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->pMarkData = pBufHeader->pMarkData;
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->hMarkTargetComponent = pBufHeader->hMarkTargetComponent;
            }

            /* trigger event handler if we are supposed to */ 
            if(pBufHeader->hMarkTargetComponent == pComponentPrivate->pHandle && pBufHeader->pMarkData){
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle, 
                                                       pComponentPrivate->pHandle->pApplicationPrivate, 
                                                       OMX_EventMark, 
                                                       0, 
                                                       0, 
                                                       pBufHeader->pMarkData);
            }
        }
    }
    else if (eDir == OMX_DirOutput) {
        /* Make sure that output buffer is issued to output stream only when
         * there is an outstanding input buffer already issued on input stream
         */
        eError = iLBCDEC_GetCorresponding_LCMLHeader(pComponentPrivate, pBufHeader->pBuffer, OMX_DirOutput, &pLcmlHdr);     
        phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)pLcmlHandle->pCodecinterfacehandle)->pCodec);

        if (pComponentPrivate->bBypassDSP == 0) {
            iLBCDEC_DPRINT ("%d :: %s :: Sending Empty OUTPUT BUFFER to Codec = %p\n",__LINE__,__FUNCTION__,pBufHeader->pBuffer);
            if (pComponentPrivate->curState == OMX_StateExecuting) {
                if (!iLBCDEC_IsPending(pComponentPrivate,pBufHeader,OMX_DirOutput)) {
                    iLBCDEC_SetPending(pComponentPrivate,pBufHeader,OMX_DirOutput, 0);
                    if(!pComponentPrivate->bDspStoppedWhileExecuting){
                        eError = LCML_QueueBuffer(pLcmlHandle->pCodecinterfacehandle,
                                                  EMMCodecOuputBuffer, 
                                                  pBufHeader->pBuffer,
                                                  pBufHeader->nAllocLen,
                                                  0,
                                                  NULL,
                                                  sizeof(iLBCDEC_ParamStruct),
                                                  pBufHeader->pBuffer);

                        if (eError != OMX_ErrorNone) {
                            iLBCDEC_EPRINT("%d :: %s Queue Output buffer, error\n", __LINE__, __FUNCTION__);
                            iLBCDEC_FatalErrorRecover(pComponentPrivate);
                            eError = OMX_ErrorHardware;
                            goto EXIT;
                        }
                        pComponentPrivate->lcml_nOpBuf++;
                    }
                }
            }
            else {
                pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = pBufHeader;
            }
        }
    } 
    else {
        eError = OMX_ErrorBadParameter;
    }

 EXIT:
    if (eError != OMX_ErrorNone && pLcmlHdr != NULL) {
        OMX_MEMFREE_STRUCT_DSPALIGN(pLcmlHdr->pFrameParam, iLBCDEC_FrameStruct);
    }
    iLBCDEC_DPRINT("%d :: %s :: Exiting\n",__LINE__,__FUNCTION__);
    iLBCDEC_DPRINT("%d :: %s :: Returning error %d\n",__LINE__,__FUNCTION__, eError);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
* iLBCDECGetBufferDirection () This function is used by the component thread to
* request a buffer from the application.  Since it was called from 2 places,
* it made sense to turn this into a small function.
*
* @param pData pointer to iLBC Decoder Context Structure
* @param pCur pointer to the buffer to be requested to be filled
*
* @retval none
**/
/*-------------------------------------------------------------------*/
OMX_ERRORTYPE iLBCDEC_GetBufferDirection(OMX_BUFFERHEADERTYPE *pBufHeader,
                                                         OMX_DIRTYPE *eDir)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = 
        pBufHeader->pPlatformPrivate;
    OMX_U32 nBuf = 0;
    OMX_BUFFERHEADERTYPE *pBuf = NULL;
    OMX_U16 flag = 1,i = 0;

    iLBCDEC_DPRINT ("%d :: %s :: Entering\n",__LINE__,__FUNCTION__);

    /*Search this buffer in input buffers list */
    nBuf = pComponentPrivate->pInputBufferList->numBuffers;
    for(i=0; i<nBuf; i++) {
        pBuf = pComponentPrivate->pInputBufferList->pBufHdr[i];
        if(pBufHeader == pBuf) {
            *eDir = OMX_DirInput;
            iLBCDEC_DPRINT ("%d :: %s :: Buffer %p is INPUT BUFFER\n",__LINE__, __FUNCTION__,pBufHeader);
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
            iLBCDEC_DPRINT ("%d :: %s :: Buffer %p is OUTPUT BUFFER\n",__LINE__, __FUNCTION__,pBufHeader);
            flag = 0;
            goto EXIT;
        }
    }

    if (flag == 1) {
        iLBCDEC_DPRINT ("%d :: %s :: Buffer %p is Not Found in the List\n", __LINE__,__FUNCTION__,pBufHeader);
        eError = OMX_ErrorUndefined;
        goto EXIT;
    }
EXIT:
    iLBCDEC_DPRINT ("%d :: %s :: Exiting iLBCDECGetBufferDirection Function\n", __LINE__,__FUNCTION__);
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
OMX_ERRORTYPE iLBCDEC_LCML_Callback (TUsnCodecEvent event,void * args [10])
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U8 *pBuffer = args[1];
    iLBCD_LCML_BUFHEADERTYPE *pLcmlHdr = NULL;
    OMX_ERRORTYPE rm_error = OMX_ErrorNone;
/*    ssize_t ret;*/
    OMX_COMPONENTTYPE *pHandle = NULL;
    LCML_DSP_INTERFACE *pLcmlHandle = NULL;
    /*OMX_U8 i;*/
    iLBCDEC_BUFDATA *OutputFrames = NULL;
    
    iLBCDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE*)
        ((LCML_DSP_INTERFACE*)args[6])->pComponentPrivate;

    iLBCDEC_DPRINT("%d :: %s :: Entering\n",__LINE__,__FUNCTION__);
    iLBCDEC_DPRINT("%d :: %s :: args = %p\n",__LINE__,__FUNCTION__,args[0]);
    iLBCDEC_DPRINT("%d :: %s :: event = %d\n",__LINE__,__FUNCTION__,event);
#ifdef iLBCDEC_DEBUG
    printEmmEvent(event);
#endif       
    if(event == EMMCodecBufferProcessed){
        if( (OMX_U32)args [0] == EMMCodecInputBuffer) {
            iLBCDEC_DPRINT("%d :: %s :: Input: pBufferr = %p\n",__LINE__, __FUNCTION__,pBuffer);
            if( pComponentPrivate->pPortDef[iLBCD_INPUT_PORT]->bEnabled != OMX_FALSE){
                eError = iLBCDEC_GetCorresponding_LCMLHeader(pComponentPrivate, 
                                                             pBuffer,
                                                             OMX_DirInput,
                                                             &pLcmlHdr);
                if (eError != OMX_ErrorNone) {
                    iLBCDEC_DPRINT("%d :: %s :: Error: Invalid Buffer Came ...\n"
                                  ,__LINE__,__FUNCTION__);
                    goto EXIT;
                }
                iLBCDEC_ClearPending(pComponentPrivate,pLcmlHdr->buffer,
                                      OMX_DirInput);
/*>>>>>>>>>>>>> LCML >>>>>>>>>>>>>*/                        
                iLBCDEC_DPRINT("%d :: %s :: Calling EmptyBufferDone\n", __LINE__,__FUNCTION__);
                iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->nHoldLength = %ld\n", __LINE__, __FUNCTION__, pComponentPrivate->nHoldLength);
        if (pComponentPrivate->curState != OMX_StatePause) {
                    pComponentPrivate->nEmptyBufferDoneCount++;   
                    pComponentPrivate->cbInfo.EmptyBufferDone (pHandle,
                                                               pComponentPrivate, /*pHandle->pApplicationPrivate,*/
                                                               pLcmlHdr->buffer);
                    pComponentPrivate->lcml_nIpBuf--;
                    pComponentPrivate->app_nBuf++;
        }
        else {
                    pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pLcmlHdr->buffer;
        }
/*<<<<<<<<<<<<<<<<<<<<<<<<<<*/ 
            }
        } else if ((OMX_U32)args [0] == EMMCodecOuputBuffer) {
            iLBCDEC_DPRINT("%d :: %s :: Output: pBufferr = %p\n",__LINE__, __FUNCTION__,pBuffer);

            eError = iLBCDEC_GetCorresponding_LCMLHeader(pComponentPrivate,
                                                         pBuffer,
                                                         OMX_DirOutput,
                                                         &pLcmlHdr);
            if (eError != OMX_ErrorNone) {
                iLBCDEC_DPRINT("%d :: %s :: Error: Invalid Buffer Came ...\n", __LINE__,__FUNCTION__);
                goto EXIT;
            }

            if (!pComponentPrivate->bStopSent) {
                pLcmlHdr->buffer->nFilledLen = (OMX_U32)args[8];
            }
            else     
                pLcmlHdr->buffer->nFilledLen = 0;

            iLBCDEC_DPRINT("%d :: %s pLcmlHdr->buffer->nFilledLen = %ld\n", __LINE__,__FUNCTION__,pLcmlHdr->buffer->nFilledLen);
           
            OutputFrames = (pLcmlHdr->buffer)->pOutputPortPrivate;
            OMX_U16 codecType = pComponentPrivate->iLBCcodecType;
            int nOutBufSize = iLBCD_OUTPUT_BUFFER_SIZE;
            if (codecType == 1) {
                nOutBufSize = OUTPUT_iLBCDEC_SECBUF_SIZE;
            }
            OutputFrames->nFrames = (OMX_U8) ((OMX_U32)args[8] / nOutBufSize); /*iLBCD_OUTPUT_BUFFER_SIZE);*/

            iLBCDEC_ClearPending(pComponentPrivate,pLcmlHdr->buffer, OMX_DirOutput);
            pComponentPrivate->nOutStandingFillDones++;                 
/*>>>>>>>> LCML >>>>>>>>>>*/
            pComponentPrivate->num_Reclaimed_Op_Buff++;

            if (pComponentPrivate->bPlayCompleteFlag){
                iLBCDEC_DPRINT ("Adding EOS flag to the output buffer\n");
                pLcmlHdr->buffer->nFlags |= OMX_BUFFERFLAG_EOS;
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                                       OMX_EventBufferFlag,
                                                       pLcmlHdr->buffer->nOutputPortIndex,
                                                       pLcmlHdr->buffer->nFlags, NULL);
                pComponentPrivate->bPlayCompleteFlag = 0;
            }
            /* Copying time stamp information to output buffer */
            pLcmlHdr->buffer->nTimeStamp = (OMX_TICKS)pComponentPrivate->arrBufIndex[pComponentPrivate->OpBufindex];
            pComponentPrivate->OpBufindex++;
            pComponentPrivate->OpBufindex %= pComponentPrivate->pPortDef[OMX_DirInput]->nBufferCountActual;           

            iLBCDEC_DPRINT("%d :: %s :: Calling FillBufferDone\n",__LINE__, __FUNCTION__);

            if (pComponentPrivate->curState != OMX_StatePause){
                pComponentPrivate->nFillBufferDoneCount++;

            pHandle = pComponentPrivate->pHandle;
                pComponentPrivate->cbInfo.FillBufferDone (pHandle,
                                                          pHandle->pApplicationPrivate,
                                                          pLcmlHdr->buffer);

                pComponentPrivate->lcml_nOpBuf--;
                pComponentPrivate->app_nBuf++;
                pComponentPrivate->nOutStandingFillDones--;
                iLBCDEC_DPRINT("%d :: %s :: Incrementing app_nBuf = %ld\n",__LINE__, __FUNCTION__,pComponentPrivate->app_nBuf);
            }
            else{
                /*In case an OutputBufCallback arrives on Paused State */
                pComponentPrivate->pOutBufHdrWhilePaused[pComponentPrivate->nPendingOutPausedBufs++] = pLcmlHdr->buffer;
            }
/*<<<<<<<<<<<<<<<<<<*/  
        }
    } else if (event == EMMCodecStrmCtrlAck) {
        iLBCDEC_DPRINT("%d :: %s :: GOT MESSAGE USN_DSPACK_STRMCTRL ----\n", __LINE__,__FUNCTION__);
    }
    else if(event == EMMCodecProcessingStoped) {
        iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate->bNoIdleOnStop = %ld\n", __LINE__,__FUNCTION__, pComponentPrivate->bNoIdleOnStop);
/*>>>>>>>>> LCML >>>>>>>>>>>*/
#ifdef RESOURCE_MANAGER_ENABLED
        rm_error = RMProxy_NewSendCommand(pHandle, 
                                       RMProxy_StateSet, 
                                       OMX_ILBC_Decoder_COMPONENT,
                                       OMX_StateIdle,
                                       NEWSENDCOMMAND_MEMORY,
                                       NULL);
#endif
        pComponentPrivate->curState = OMX_StateIdle;
        pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                               OMX_EventCmdComplete,
                                               OMX_CommandStateSet,
                                               pComponentPrivate->curState,
                                               NULL);
/*<<<<<<<<<<<<<<<<<<<<*/        
        pComponentPrivate->bNoIdleOnStop= OMX_FALSE;
    }
    else if (event == EMMCodecProcessingPaused) {
        pComponentPrivate->curState = OMX_StatePause;
        /* Send StateChangeNotification to application */
        pComponentPrivate->cbInfo.EventHandler(
                                               pComponentPrivate->pHandle,
                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                               OMX_EventCmdComplete, OMX_CommandStateSet, pComponentPrivate->curState,NULL);
    
    }
    else if (event == EMMCodecDspError) {
        iLBCDEC_DPRINT("EMMCodecDspError: args[4] %d args[5] %d\n",
                      (int)args[4],(int)args[5]);
#ifdef _ERROR_PROPAGATION__
        /* Cheking for MMU_fault */
        if(((int) args[4] == USN_ERR_UNKNOWN_MSG) && (args[4] ==(void*) NULL)) {
            iLBCDEC_DPRINT("%d :: UTIL: MMU_Fault \n",__LINE__);
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
        if(((int)args[4] == USN_ERR_WARNING) && 
           ((int)args[5] == IUALG_WARN_PLAYCOMPLETED)) {
            pHandle = pComponentPrivate->pHandle;
            iLBCDEC_DPRINT("%d :: GOT MESSAGE IUALG_WARN_PLAYCOMPLETED\n",
                          __LINE__);
                 
            /* add callback to application to indicate SN/USN has completed playing of current set of date */
            pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,                  
                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                   OMX_EventBufferFlag,
                                                   (OMX_U32)NULL,
                                                   OMX_BUFFERFLAG_EOS,
                                                   NULL);
        }
        if((((OMX_U32) args [4]) == USN_ERR_NONE) && (args[5] == (void*)NULL))
        {
            OMXDBG_PRINT(stderr, ERROR, 4, 0, "%d :: UTIL: MMU_Fault \n",__LINE__);
            iLBCDEC_FatalErrorRecover(pComponentPrivate);
        }
        if((int)args[5] == IUALG_WARN_CONCEALED) {
            iLBCDEC_DPRINT( "Algorithm issued a warning. But can continue");
        }
        if((int)args[5] == IUALG_ERR_GENERAL) {
            char *pArgs = "";
            iLBCDEC_EPRINT( "Algorithm error. Cannot continue" );
            iLBCDEC_EPRINT("%d :: arg5 = %x\n",__LINE__,(int)args[5]);
            iLBCDEC_EPRINT("%d :: LCML_Callback: IUALG_ERR_GENERAL\n",__LINE__);
            pHandle = pComponentPrivate->pHandle;
            pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
            eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                       MMCodecControlStop,(void *)pArgs);
            if(eError != OMX_ErrorNone) {
                iLBCDEC_DPRINT("%d: Error Occurred in Codec Stop..\n",
                              __LINE__);
                goto EXIT;
            }
            iLBCDEC_DPRINT("%d :: Codec has been Stopped here\n",__LINE__);
#ifdef RESOURCE_MANAGER_ENABLED
            rm_error = RMProxy_NewSendCommand(pHandle,
                                              RMProxy_StateSet,
                                              OMX_ILBC_Decoder_COMPONENT,
                                              OMX_StateIdle,
                                              NEWSENDCOMMAND_MEMORY,
                                              NULL);
#endif
            pComponentPrivate->curState = OMX_StateIdle;
            pComponentPrivate->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete, 
                                                   OMX_ErrorNone,
                                                   0, 
                                                   NULL);
        }
        if( (int)args[5] == IUALG_ERR_DATA_CORRUPT ) {
                char *pArgs = "";
                iLBCDEC_DPRINT( "Algorithm error. Corrupt data" );
                iLBCDEC_DPRINT("%d :: arg5 = %x\n",__LINE__,(int)args[5]);
                iLBCDEC_DPRINT("%d :: LCML_Callback: IUALG_ERR_DATA_CORRUPT\n",
                              __LINE__);
                pHandle = pComponentPrivate->pHandle;
                pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           MMCodecControlStop,(void *)pArgs);
                if(eError != OMX_ErrorNone) {
                    iLBCDEC_DPRINT("%d: Error Occurred in Codec Stop..\n",
                                  __LINE__);
                    goto EXIT;
                }
                iLBCDEC_DPRINT("%d :: Codec has been Stopped here\n",__LINE__);
#ifdef RESOURCE_MANAGER_ENABLED
                rm_error = RMProxy_NewSendCommand(pHandle,
                                                  RMProxy_StateSet,
                                                  OMX_ILBC_Decoder_COMPONENT,
                                                  OMX_StateIdle,
                                                  NEWSENDCOMMAND_MEMORY,
                                                  NULL);
#endif
                pComponentPrivate->curState = OMX_StateIdle;
                pComponentPrivate->cbInfo.EventHandler(
                                                       pHandle, pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_ErrorNone,
                                                       0, 
                                                       NULL);
            }                                       
        if( (int)args[5] == IUALG_WARN_OVERFLOW ){
            iLBCDEC_DPRINT( "Algorithm error. Overflow, sending to Idle\n" );
#ifdef RESOURCE_MANAGER_ENABLED         
            rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, 
                                           OMX_ILBC_Decoder_COMPONENT,
                                           OMX_StateIdle,
                                           NEWSENDCOMMAND_MEMORY,
                                           NULL);
#endif                  
        }
        if( (int)args[5] == IUALG_WARN_UNDERFLOW ){
            iLBCDEC_DPRINT( "Algorithm error. Underflow, sending to Idle\n" );
#ifdef RESOURCE_MANAGER_ENABLED         
            rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, 
                                           OMX_ILBC_Decoder_COMPONENT,
                                           OMX_StateIdle,
                                           NEWSENDCOMMAND_MEMORY,
                                           NULL);
#endif         
        }
    }

    if(event == EMMCodecDspMessageRecieved) {
        iLBCDEC_DPRINT("%d :: %s :: commandedState  = %p\n",__LINE__,
                      __FUNCTION__,args[0]);
        iLBCDEC_DPRINT("%d :: %s :: arg1 = %p\n",__LINE__,__FUNCTION__,args[1]);
        iLBCDEC_DPRINT("%d :: %s :: arg2 = %p\n",__LINE__,__FUNCTION__,args[2]);
    }

#ifdef _ERROR_PROPAGATION__

    else if (event == EMMCodecInitError){
        /* Cheking for MMU_fault */
        if(((int) args[4] == USN_ERR_UNKNOWN_MSG) && (args[5] == (void*) NULL)){
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
    else if (event == EMMCodecInternalError){
        /* Cheking for MMU_fault */
        if(((int)args[4] == USN_ERR_UNKNOWN_MSG) && (args[5] == (void*) NULL)){
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
    iLBCDEC_DPRINT ("%d :: %s :: Exiting\n",__LINE__,__FUNCTION__);
    return eError;
}


/* ================================================================================= * */
/**
* @fn iLBCDEC_GetCorresponding_LCMLHeader() function gets the corresponding LCML
* header from the actual data buffer for required processing.
*
* @param *pBuffer This is the data buffer pointer.
*
* @param eDir   This is direction of buffer. Input/Output.
*
* @param *iLBCD_LCML_BUFHEADERTYPE  This is pointer to LCML Buffer Header.
*
* @pre          None
*
* @post         None
*
*  @return      OMX_ErrorNone = Successful Inirialization of the component\n
*               OMX_ErrorHardware = Hardware error has occured.
*
*  @see         None
*/
/* ================================================================================ * */
OMX_ERRORTYPE iLBCDEC_GetCorresponding_LCMLHeader(iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate,
                                                  OMX_U8 *pBuffer,
                                                  OMX_DIRTYPE eDir,
                                                  iLBCD_LCML_BUFHEADERTYPE **ppLcmlHdr)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    iLBCD_LCML_BUFHEADERTYPE *pLcmlBufHeader = NULL;
    
    OMX_S16 nIpBuf = 0;
    OMX_S16 nOpBuf = 0;
    OMX_S16 i = 0;

    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    
    while (!pComponentPrivate->bInitParamsInitialized) {
        iLBCDEC_DPRINT("%d :: %s :: Waiting for init to complete\n",__LINE__,
                      __FUNCTION__);
        sched_yield();
    }
    iLBCDEC_DPRINT("%d :: %s :: pComponentPrivate = %p\n",__LINE__,__FUNCTION__, pComponentPrivate);
    iLBCDEC_DPRINT("%d :: %s :: eDir = %d\n",__LINE__,__FUNCTION__,eDir);

    if(eDir == OMX_DirInput) {
        pLcmlBufHeader = pComponentPrivate->pLcmlBufHeader[iLBCD_INPUT_PORT];
        for(i=0; i<nIpBuf; i++) {
            iLBCDEC_DPRINT("%d :: %s :: pBuffer = %p\n",__LINE__,__FUNCTION__, pBuffer);
            iLBCDEC_DPRINT("%d :: %s :: pLcmlBufHeader->buffer->pBuffer = %p\n", __LINE__,__FUNCTION__,pLcmlBufHeader->buffer->pBuffer);

            if(pBuffer == pLcmlBufHeader->buffer->pBuffer) {
                *ppLcmlHdr = pLcmlBufHeader;
                 iLBCDEC_DPRINT("%d :: %s :: Corresponding LCML Header Found\n", __LINE__,__FUNCTION__);
                 goto EXIT;
            }
            pLcmlBufHeader++;
        }
    } else if (eDir == OMX_DirOutput) {
        pLcmlBufHeader = pComponentPrivate->pLcmlBufHeader[iLBCD_OUTPUT_PORT];
        for(i=0; i<nOpBuf; i++) {
            iLBCDEC_DPRINT("%d :: %s :: pBuffer = %p\n",__LINE__,__FUNCTION__, pBuffer);
            iLBCDEC_DPRINT("%d :: %s :: pLcmlBufHeader->buffer->pBuffer = %p\n", __LINE__,__FUNCTION__, pLcmlBufHeader->buffer->pBuffer);
            if(pBuffer == pLcmlBufHeader->buffer->pBuffer) {
                *ppLcmlHdr = pLcmlBufHeader;
                iLBCDEC_DPRINT("%d :: %s :: Corresponding LCML Header Found\n", __LINE__,__FUNCTION__);
                goto EXIT;
            }
            pLcmlBufHeader++;
        }
    } else {
        iLBCDEC_DPRINT("%d :: %s :: Invalid Buffer Type :: exiting...\n",
                      __LINE__,__FUNCTION__);
    }
    
    iLBCDEC_DPRINT("%d :: %s Exiting\n",__LINE__,__FUNCTION__);

EXIT:
    return eError;
}

/* ================================================================================= * */
/**
* @fn iLBCDEC_GetLCMLHandle() function gets the LCML handle and interacts with LCML
* by using this LCML Handle.
*
* @param *pBufHeader This is the buffer header that needs to be processed.
*
* @param *pComponentPrivate  This is component's private date structure.
*
* @pre          None
*
* @post         None
*
*  @return      OMX_HANDLETYPE = Successful loading of LCML library.
*               OMX_ErrorHardware = Hardware error has occured.
*
*  @see         None
*/
/* ================================================================================ * */
OMX_HANDLETYPE iLBCDEC_GetLCMLHandle(iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate)
{
    void *handle = NULL;
    OMX_ERRORTYPE (*fpGetHandle)(OMX_HANDLETYPE);
    OMX_HANDLETYPE pHandle = NULL;
    const char *error = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    iLBCDEC_DPRINT("%d :: %s Entering\n",__LINE__,__FUNCTION__);
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
        iLBCDEC_EPRINT("%d :: %s eError != OMX_ErrorNone...\n",__LINE__, __FUNCTION__);
        pHandle = NULL;
        goto EXIT;
    }
    pComponentPrivate = (iLBCDEC_COMPONENT_PRIVATE*)pComponentPrivate;
    pComponentPrivate->bLcmlHandleOpened = 1;
 
    ((LCML_DSP_INTERFACE*)pHandle)->pComponentPrivate = pComponentPrivate;
 
    pComponentPrivate->ptrLibLCML=handle;/* saving LCML lib pointer  */

EXIT:
    iLBCDEC_DPRINT("%d :: %s Returning %p\n",__LINE__,__FUNCTION__,pHandle);

    return pHandle;
}

/* ========================================================================== */
/**
* @iLBCDEC_SetPending() This function marks the buffer as pending when it is sent
* to DSP/
*
* @param pComponentPrivate This is component's private date area.
*
* @param pBufHdr This is poiter to OMX Buffer header whose buffer is sent to DSP
*
* @param eDir This is direction of buffer i.e. input or output.
*
* @pre None
*
* @post None
*
* @return none
*/
/* ========================================================================== */
void iLBCDEC_SetPending(iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir, OMX_U32 lineNumber)
{
    OMX_U16 i = 0;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 1;
                iLBCDEC_DPRINT("%d :: %s :: *****INPUT BUFFER %d IS PENDING****\n",__LINE__,__FUNCTION__,i);
            }
        }
    } else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 1;
                iLBCDEC_DPRINT("%d :: %s :: *****OUTPUT BUFFER %d IS PENDING****\n",__LINE__,__FUNCTION__,i);
            }
        }
    }
}

/* ========================================================================== */
/**
* @iLBCDEC_IsPending() This function checks whether or not a buffer is pending.
*
* @param pComponentPrivate This is component's private date area.
*
* @param pBufHdr This is poiter to OMX Buffer header of interest.
*
* @param eDir This is direction of buffer i.e. input or output.
*
* @pre None
*
* @post None
*
* @return none
*/
/* ========================================================================== */

OMX_U32 iLBCDEC_IsPending(iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir)
{
    OMX_U16 i = 0;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                return pComponentPrivate->pInputBufferList->bBufferPending[i];
            }
        }
    } else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                return pComponentPrivate->pOutputBufferList->bBufferPending[i];
            }
        }
    }
    return -1;
}

/**/
/**/
/* ========================================================================== */
/**
* @iLBCDEC_ClearPending() This function clears the buffer status from pending
* when it is received back from DSP.
*
* @param pComponentPrivate This is component's private date area.
*
* @param pBufHdr This is poiter to OMX Buffer header that is received from
* DSP/LCML.
*
* @param eDir This is direction of buffer i.e. input or output.
*
* @pre None
*
* @post None
*
* @return none
*/
/* ========================================================================== */

void iLBCDEC_ClearPending(iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir)
{
    OMX_U16 i = 0;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 0;
                iLBCDEC_DPRINT("%d :: %s :: ****INPUT BUFFER %d IS RECLAIMED****\n",__LINE__,__FUNCTION__,i);
            }
        }
    } else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 0;
                iLBCDEC_DPRINT("%d :: %s :: ****OUTPUT BUFFER %d IS RECLAIMED***\n",__LINE__,__FUNCTION__,i);
            }
        }
    }
}

/* ========================================================================== */
/**
* @iLBCDEC_IsValid() This function identifies whether or not buffer recieved from
* LCML is valid. It searches in the list of input/output buffers to do this.
*
* @param pComponentPrivate This is component's private date area.
*
* @param pBufHdr This is poiter to OMX Buffer header of interest.
*
* @param eDir This is direction of buffer i.e. input or output.
*
* @pre None
*
* @post None
*
* @return status of the buffer.
*/
/* ========================================================================== */

OMX_U32 iLBCDEC_IsValid(iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U8 *pBuffer, OMX_DIRTYPE eDir)
{
    OMX_U16 i = 0;
    OMX_S16 found=0;

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
* @iLBCDECFill_LCMLInitParamsEx() This function initializes the init parameter of
* the LCML structure when a port is enabled and component is in idle state.
*
* @param pComponent This is component handle.
*
* @pre None
*
* @post None
*
* @return appropriate OMX Error.
*/
/* ========================================================================== */

OMX_ERRORTYPE iLBCDECFill_LCMLInitParamsEx(OMX_HANDLETYPE pComponent)

{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0,nIpBufSize = 0,nOpBuf = 0,nOpBufSize = 0;
    OMX_U16 i = 0;
    OMX_BUFFERHEADERTYPE *pTemp = NULL;
    OMX_U32 size_lcml = 0;

    LCML_DSP_INTERFACE *pHandle = (LCML_DSP_INTERFACE *)pComponent;
    iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate = NULL;
    iLBCD_LCML_BUFHEADERTYPE *pTemp_lcml  = NULL;

    iLBCDEC_DPRINT("%d :: %s\n ",__LINE__,__FUNCTION__);
    iLBCDEC_DPRINT("%d :: %s :: pHandle = %p\n",__LINE__,__FUNCTION__,pHandle);
    iLBCDEC_DPRINT("%d :: %s :: pHandle->pComponentPrivate = %p\n",__LINE__,
                  __FUNCTION__,pHandle->pComponentPrivate);
    pComponentPrivate = pHandle->pComponentPrivate;

    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    
    OMX_U16 iLBCcodecType = pComponentPrivate->iLBCcodecType;
    nIpBufSize = STD_iLBCDEC_BUF_SIZE;
    nOpBufSize = iLBCD_OUTPUT_BUFFER_SIZE;
    if (iLBCcodecType == 1) {
        nIpBufSize = INPUT_iLBCDEC_SECBUF_SIZE;
        nOpBufSize = OUTPUT_iLBCDEC_SECBUF_SIZE;
    }
    
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;


    size_lcml = nIpBuf * sizeof(iLBCD_LCML_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml,size_lcml,iLBCD_LCML_BUFHEADERTYPE);
    pComponentPrivate->pLcmlBufHeader[iLBCD_INPUT_PORT] = pTemp_lcml;

    for (i=0; i<nIpBuf; i++) {
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
/*        pTemp->nAllocLen = nIpBufSize;*/
        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = iLBCDEC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = iLBCDEC_MINOR_VER;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirInput;
    
        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pBufferParam, sizeof(iLBCDEC_ParamStruct), iLBCDEC_ParamStruct);
        if (pTemp_lcml->pBufferParam == NULL) {
            iLBCDEC_EPRINT ("%d :: %s OMX_ErrorInsufficientResources\n",  __LINE__,__FUNCTION__);
            iLBCDEC_CleanupInitParams(pComponentPrivate->pHandle);
            return OMX_ErrorInsufficientResources;
        }

        OMX_MALLOC_GENERIC(pTemp_lcml->pDmmBuf,DMM_BUFFER_OBJ);
        
        pTemp_lcml->pFrameParam = NULL;
        pTemp_lcml->pBufferParam->usNbFrames =0;
        pTemp_lcml->pBufferParam->pParamElem = NULL;

        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */
        pTemp->nFlags = NORMAL_BUFFER;

        pTemp_lcml++;
    }

    /* Allocate memory for all output buffer headers..
       * This memory pointer will be sent to LCML */
    size_lcml = nOpBuf * sizeof(iLBCD_LCML_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml,size_lcml,iLBCD_LCML_BUFHEADERTYPE);
    pComponentPrivate->pLcmlBufHeader[iLBCD_OUTPUT_PORT] = pTemp_lcml;

    for (i=0; i<nOpBuf; i++) {
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
/*        pTemp->nAllocLen = nOpBufSize;*/
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = iLBCDEC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = iLBCDEC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        pTemp_lcml->pFrameParam = NULL;
       
        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pBufferParam, sizeof(iLBCDEC_ParamStruct), iLBCDEC_ParamStruct);
        if (pTemp_lcml->pBufferParam == NULL) {
            iLBCDEC_EPRINT ("%d :: %s OMX_ErrorInsufficientResources\n",  __LINE__,__FUNCTION__);
            iLBCDEC_CleanupInitParams(pComponentPrivate->pHandle);
            return OMX_ErrorInsufficientResources;
        }

        pTemp_lcml->pBufferParam->usNbFrames =0;
        pTemp_lcml->pBufferParam->pParamElem = NULL;
        
        OMX_MALLOC_GENERIC(pTemp_lcml->pDmmBuf,DMM_BUFFER_OBJ);

        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirOutput;
        iLBCDEC_DPRINT("%d :: %s :: pTemp_lcml = %p\n",__LINE__,__FUNCTION__, pTemp_lcml);
        iLBCDEC_DPRINT("%d :: %s :: pTemp_lcml->buffer = %p\n",__LINE__,
                      __FUNCTION__,pTemp_lcml->buffer);

        pTemp->nFlags = NORMAL_BUFFER;

        pTemp++;
        pTemp_lcml++;
    }
    pComponentPrivate->bPortDefsAllocated = 1;

    iLBCDEC_DPRINT("%d :: %s Exiting\n",__LINE__,__FUNCTION__);

    pComponentPrivate->bInitParamsInitialized = 1;
EXIT:
    return eError;
}


/** =========================================================================*/
/*  OMX_DmmMap () method is used to allocate the memory using DMM.
*
*  @param ProcHandle -  Component identification number
*  @param size  - Buffer header address, that needs to be sent to codec
*  @param pArmPtr - Message used to send the buffer to codec
*  @param pDmmBuf - buffer id
*
*  @retval OMX_ErrorNone  - Success
*          OMX_ErrorHardware  -  Hardware Error
**/
/** =========================================================================*/
OMX_ERRORTYPE OMX_DmmMap(DSP_HPROCESSOR ProcHandle,
                     int size,
                     void* pArmPtr,
                     DMM_BUFFER_OBJ* pDmmBuf)
{
    int status = 0;
    int nSizeReserved = 0;

    if(pDmmBuf == NULL){
        iLBCDEC_EPRINT ("%d :: %s pBuf is NULL\n", __LINE__, __FUNCTION__);
        return (OMX_ErrorBadParameter);
    }

    if(pArmPtr == NULL){
        iLBCDEC_EPRINT ("%d :: %s pBuf is NULL\n", __LINE__, __FUNCTION__);
        return (OMX_ErrorBadParameter);
    }

    /* Allocate */
    pDmmBuf->pAllocated = pArmPtr;

    /* Reserve */
    nSizeReserved = ROUND_TO_PAGESIZE(size) + 2*DMM_PAGE_SIZE ;
    status = DSPProcessor_ReserveMemory(ProcHandle, 
                                        nSizeReserved, 
                                        &(pDmmBuf->pReserved));
    iLBCDEC_DPRINT("\nOMX Reserve DSP: %p\n",pDmmBuf->pReserved);
    
    if(DSP_FAILED(status)){
        iLBCDEC_EPRINT("DSPProcessor_ReserveMemory() failed - error 0x%x", (int)status);
        return (OMX_ErrorHardware);
    }
    pDmmBuf->nSize = size;
    iLBCDEC_DPRINT(" DMM MAP Reserved: %p, size 0x%x (%d)\n", 
                  pDmmBuf->pReserved,nSizeReserved,nSizeReserved);
    
    /* Map */
    status = DSPProcessor_Map(ProcHandle,
                              pDmmBuf->pAllocated,/* Arm addres of data to Map on DSP*/
                              OMX_GET_SIZE_DSPALIGN(size), /* size to Map on DSP*/
                              pDmmBuf->pReserved, /* reserved space */
                              &(pDmmBuf->pMapped), /* returned map pointer */
                              ALIGNMENT_CHECK);
    if(DSP_FAILED(status)){
        iLBCDEC_EPRINT ("DSPProcessor_Map() failed - error 0x%x", (int)status);
        return (OMX_ErrorHardware);
    }
    iLBCDEC_DPRINT("DMM Mapped: %p, size 0x%x (%d)\n",pDmmBuf->pMapped, 
                  size,size);

    /* Issue an initial memory flush to ensure cache coherency */
    status = DSPProcessor_FlushMemory(ProcHandle, pDmmBuf->pAllocated, size, 0);
    if(DSP_FAILED(status)){
        iLBCDEC_EPRINT ("Unable to flush mapped buffer: error 0x%x",(int)status);
        return (OMX_ErrorHardware);
    }
    return (OMX_ErrorNone);  
}
#ifdef RESOURCE_MANAGER_ENABLED
/** =========================================================================*/
/*  iLBCD_ResourceManagerCallback 
**/
/** =========================================================================*/
void iLBCD_ResourceManagerCallback(RMPROXY_COMMANDDATATYPE cbData)
{
    OMX_COMMANDTYPE Cmd = OMX_CommandStateSet;
    OMX_STATETYPE state = OMX_StateIdle;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)cbData.hComponent;
    iLBCDEC_COMPONENT_PRIVATE *pCompPrivate = NULL;

    pCompPrivate = (iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    if (*(cbData.RM_Error) == OMX_RmProxyCallback_ResourcesPreempted){
        if (pCompPrivate->curState == OMX_StateExecuting || 
            pCompPrivate->curState == OMX_StatePause) {

            write (pCompPrivate->cmdPipe[1], &Cmd, sizeof(Cmd));
            write (pCompPrivate->cmdDataPipe[1], &state ,sizeof(OMX_U32));

/* ACA           pCompPrivate->bPreempted = 1;*/
        }
    }
    else if (*(cbData.RM_Error) == OMX_RmProxyCallback_ResourcesAcquired){
        pCompPrivate->cbInfo.EventHandler ( pHandle, 
                                            pHandle->pApplicationPrivate,
                                            OMX_EventResourcesAcquired, 
                                            0,
                                            0,
                                            NULL);
    }
    else if (*(cbData.RM_Error) == OMX_RmProxyCallback_FatalError) {
        iLBCDEC_EPRINT("%d :RM Fatal Error:\n", __LINE__);
        iLBCDEC_FatalErrorRecover(pCompPrivate);
    }
}
#endif

void iLBCDEC_FatalErrorRecover(iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate){
#ifdef RESOURCE_MANAGER_ENABLED
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle,
                                    RMProxy_FreeResource,
                                    OMX_ILBC_Decoder_COMPONENT, 0, 3456, NULL);

    eError = RMProxy_Deinitalize();
    if (eError != OMX_ErrorNone) {
        iLBCDEC_EPRINT ("%d ::RMProxy_Deinitalize error = %d\n", __LINE__, eError);
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
        iLBCDEC_CleanupInitParams(pComponentPrivate->pHandle);
    }
    iLBCDEC_DPRINT ("%d ::Completed FatalErrorRecover Entering Invalid State\n", __LINE__);
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
OMX_ERRORTYPE OMX_DmmUnMap(DSP_HPROCESSOR ProcHandle, 
                           void* pMapPtr, 
                           void* pResPtr)
{
    int status = 0;
    iLBCDEC_DPRINT("\nOMX UnReserve DSP: %p\n",pResPtr);

    if(pMapPtr == NULL){
        iLBCDEC_EPRINT ("pMapPtr is NULL\n");
        return (OMX_ErrorBadParameter);
    }
    if(pResPtr == NULL){
        iLBCDEC_EPRINT ("pResPtr is NULL\n");
        return (OMX_ErrorBadParameter);
    }
    status = DSPProcessor_UnMap(ProcHandle,pMapPtr);
    if(DSP_FAILED(status)) {
        iLBCDEC_DPRINT ("DSPProcessor_UnMap() failed - error 0x%x",(int)status);
        return (OMX_ErrorHardware);
    }

    iLBCDEC_DPRINT("unreserving  structure =0x%p\n",pResPtr );
    status = DSPProcessor_UnReserveMemory(ProcHandle,pResPtr);
    if(DSP_FAILED(status)){
        iLBCDEC_DPRINT ("DSPProcessor_UnReserveMemory() failed - error 0x%x", (int)status);
        return (OMX_ErrorHardware);
    }

    return (OMX_ErrorNone);
}

void printEmmEvent (TUsnCodecEvent event)
{
    switch(event) {
        
    case EMMCodecDspError:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecDspError\n",  __LINE__,__FUNCTION__);
        break;

    case EMMCodecInternalError:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecInternalError\n",__LINE__,__FUNCTION__);
        break;

    case EMMCodecInitError:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecInitError\n", __LINE__,__FUNCTION__);
        break;

    case EMMCodecDspMessageRecieved:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecDspMessageRecieved\n",__LINE__,__FUNCTION__);
        break;

    case EMMCodecBufferProcessed:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecBufferProcessed\n",__LINE__,__FUNCTION__);
        break;

    case EMMCodecProcessingStarted:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecProcessingStarted\n",__LINE__,__FUNCTION__);
        break;
            
    case EMMCodecProcessingPaused:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecProcessingPaused\n",__LINE__,__FUNCTION__);
        break;

    case EMMCodecProcessingStoped:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecProcessingStoped\n",__LINE__,__FUNCTION__);
        break;

    case EMMCodecProcessingEof:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecProcessingEof\n",__LINE__,__FUNCTION__);
        break;

    case EMMCodecBufferNotProcessed:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecBufferNotProcessed\n",__LINE__,__FUNCTION__);
        break;

    case EMMCodecAlgCtrlAck:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecAlgCtrlAck\n", __LINE__,__FUNCTION__);
        break;

    case EMMCodecStrmCtrlAck:
        iLBCDEC_DPRINT("%d :: %s :: [LCML CALLBACK EVENT]  EMMCodecStrmCtrlAck\n" ,__LINE__,__FUNCTION__);
        break;
    }
    return;
}

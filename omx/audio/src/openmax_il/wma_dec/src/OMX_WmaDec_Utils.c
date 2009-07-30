
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
 * @file OMX_WmaDec_Utils.c
 *
 * This file implements OMX Component for WMA decoder that
 * is fully compliant with the OMX Audio specification 1.0.
 *
 * @path  $(CSLPATH)\
 *
 * @rev  0.1
 */
/* ----------------------------------------------------------------------------
 *!
 *! Revision History
 *! ===================================
 *! 10-Sept-2005 mf:  Initial Version. Change required per OMAPSWxxxxxxxxx
 *! to provide _________________.
 *!
 *!
 *! 10-Sept-2005 mf:
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
#include <stdlib.h>
#else
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
#endif
#include <dbapi.h>
#include <string.h>
#include <stdio.h>

#include "OMX_WmaDec_Utils.h"
#include "wmadecsocket_ti.h"
#include <decode_common_ti.h>
#include "usn.h"

#ifdef UNDER_CE
#define HASHINGENABLE 1


void sleep(DWORD Duration)
{
    Sleep(Duration);
}
#endif

#ifdef WMADEC_MEMDEBUG
#define newmalloc(x) mymalloc(__LINE__,__FILE__,x)
#define newfree(z) myfree(z,__LINE__,__FILE__)
#else
#define newmalloc(x) malloc(x)
#define newfree(z) free(z)
#endif

/* ========================================================================== */
/**
 * @WMADECFill_LCMLInitParams () This function is used by the component thread to
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
OMX_ERRORTYPE WMADECFill_LCMLInitParams(OMX_COMPONENTTYPE* pComponent,
                                        LCML_DSP *plcml_Init, OMX_U16 arr[])
{ 
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf,nIpBufSize,nOpBuf,nOpBufSize;
    OMX_U16 i;
    OMX_BUFFERHEADERTYPE *pTemp;
    int size_lcml;
    OMX_U16 index;
    LCML_WMADEC_BUFHEADERTYPE *pTemp_lcml = NULL;
    LCML_STRMATTR *strmAttr = NULL;
    char *pTemp_char = NULL;
    LCML_DSP_INTERFACE *pHandle;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate, *pComponentPrivate_CC;

    pComponentPrivate = pComponent->pComponentPrivate;
    pComponentPrivate_CC = pComponentPrivate;
    pHandle = (LCML_DSP_INTERFACE *)pComponent;

    OMX_PRINT2(pComponentPrivate->dbg, "[WMADECFill_LCMLInitParams] pComponent = %p\n",pComponent);
    OMX_PRINT2(pComponentPrivate->dbg, "[WMADECFill_LCMLInitParams] pComponentPrivate = %p\n",
                  pComponentPrivate);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entered WMADECFill_LCMLInitParams\n",__LINE__);
    OMX_PRINT2(pComponentPrivate->dbg, "%d :: WMADECFill_LCMLInitParams - pComponentPrivate = %p\n",
                  __LINE__,pComponentPrivate);
    OMX_PRINT2(pComponentPrivate->dbg, "%d :: WMADECFill_LCMLInitParams - pHandle = %p\n",__LINE__,
                  pHandle);

    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    pComponentPrivate->nRuntimeInputBuffers = nIpBuf;
    nIpBufSize = INPUT_WMADEC_BUFFER_SIZE;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    pComponentPrivate->nRuntimeOutputBuffers = nOpBuf;
    nOpBufSize = pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferSize;

    /* Fill Input Buffers Info for LCML */
    plcml_Init->In_BufInfo.nBuffers = nIpBuf;
    plcml_Init->In_BufInfo.nSize = nIpBufSize;
    plcml_Init->In_BufInfo.DataTrMethod = DMM_METHOD;


    /* Fill Output Buffers Info for LCML */
    plcml_Init->Out_BufInfo.nBuffers = pComponentPrivate_CC->pOutputBufferList->numBuffers;
    plcml_Init->Out_BufInfo.nSize = nOpBufSize;
    plcml_Init->Out_BufInfo.DataTrMethod = DMM_METHOD;

    /*Copy the node information */
    plcml_Init->NodeInfo.nNumOfDLLs = 3;

    plcml_Init->NodeInfo.AllUUIDs[0].uuid = &WMADECSOCKET_TI_UUID;

    strcpy ((char *) plcml_Init->NodeInfo.AllUUIDs[0].DllName,WMADEC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[0].eDllType = DLL_NODEOBJECT;

    plcml_Init->NodeInfo.AllUUIDs[1].uuid = &WMADECSOCKET_TI_UUID;
    strcpy ((char *) plcml_Init->NodeInfo.AllUUIDs[1].DllName,WMADEC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[1].eDllType = DLL_DEPENDENT;

    plcml_Init->NodeInfo.AllUUIDs[2].uuid = &USN_TI_UUID;
    strcpy ((char *) plcml_Init->NodeInfo.AllUUIDs[2].DllName,WMADEC_USN_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[2].eDllType = DLL_DEPENDENT;


    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
    plcml_Init->DeviceInfo.TypeofDevice =0;
    
    WMAD_OMX_MALLOC_SIZE(pComponentPrivate->pDynParams, sizeof(WMADEC_UALGParams) + 256,WMADEC_UALGParams);
    pTemp_char = (char*)pComponentPrivate->pDynParams;
    pTemp_char += 128;
    pComponentPrivate->pDynParams = (WMADEC_UALGParams*)pTemp_char;    

    pComponentPrivate->first_buffer=1;
    
    if(pComponentPrivate_CC->dasfmode == 1)
    {
        WMAD_OMX_MALLOC_SIZE(pComponentPrivate->pParams, sizeof(WMADEC_AudioCodecParams) + 256, WMADEC_AudioCodecParams);
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[ALLOC] %p\n",__LINE__,pComponentPrivate->pParams); 
        if(NULL == pComponentPrivate->pParams)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "Memory Allocation Failed\n");
            eError = OMX_ErrorInsufficientResources;
            goto EXIT;                          
        }
        pTemp_char = (char*)pComponentPrivate->pParams;
        pTemp_char += 128;
        pComponentPrivate->pParams = (WMADEC_AudioCodecParams*)pTemp_char;
    
        WMAD_OMX_MALLOC(strmAttr, LCML_STRMATTR);
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[ALLOC] %p\n",__LINE__,strmAttr);

        if (strmAttr == NULL)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "strmAttr - failed to malloc\n");
            eError = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        pComponentPrivate_CC->strmAttr = strmAttr;
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[ALLOC] %p\n",__LINE__,strmAttr);
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);

        strmAttr->uSegid = 0;
        strmAttr->uAlignment = 0;
        strmAttr->uTimeout = -1;
        strmAttr->uBufsize = pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferSize;
        strmAttr->uNumBufs = NUM_WMADEC_OUTPUT_BUFFERS_DASF;
        strmAttr->lMode = STRMMODE_PROCCOPY;
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);

        plcml_Init->DeviceInfo.TypeofDevice =1;
        plcml_Init->DeviceInfo.AllUUIDs[0].uuid = &DCTN_TI_UUID;
        plcml_Init->DeviceInfo.DspStream = strmAttr;
    }

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);

    /*copy the other information */
    plcml_Init->SegID = OMX_WMADEC_DEFAULT_SEGMENT;
    plcml_Init->Timeout = OMX_WMADEC_SN_TIMEOUT;
    plcml_Init->Alignment = 0;
    plcml_Init->Priority = OMX_WMADEC_SN_PRIORITY;
    plcml_Init->ProfileID = -1;
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
    pComponentPrivate_CC->iWmaSamplingFrequeny = WMADEC_DEFAULT_SAMPLING_FREQUENCY;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);


    /*Accessing these 2 has the problem/creates problem in state transition tests*/
    pComponentPrivate_CC->iWmaChannels =
        pComponentPrivate_CC->wmaParams[OUTPUT_PORT]->nChannels;
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);

    arr[0] = WMADEC_STREAM_COUNT;
    arr[1] = INPUT_PORT;
    arr[2] = DMM;
    if (pComponentPrivate_CC->pInputBufferList->numBuffers)
    {
        arr[3] = pComponentPrivate_CC->pInputBufferList->numBuffers;
    }
    else
    {
        arr[3] = 1;
    }

    arr[4] = OUTPUT_PORT;

    if(pComponentPrivate_CC->dasfmode == 1)
    {
        arr[5] = OUTSTRM;
        arr[6] = NUM_WMADEC_OUTPUT_BUFFERS_DASF;
    }
    else
    {
        OMX_PRDSP2(pComponentPrivate->dbg, "FILE MODE\n");
        arr[5] = DMM;
        if (pComponentPrivate_CC->pOutputBufferList->numBuffers)
        {
            arr[6] = pComponentPrivate_CC->pOutputBufferList->numBuffers;
        }
        else
        {
            arr[6] = 1;
        }
    }
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);

    index = WMADEC_STREAM_COUNT * 3 + 1;        
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);

 
#ifndef UNDER_CE
    arr[index] = END_OF_CR_PHASE_ARGS;

    for (i=0; i < index+1; i++) {
        OMX_PRINT2(pComponentPrivate->dbg, "arr[%d] = %d\n",i,arr[i]);
    }
    
#else
    arr[7] = END_OF_CR_PHASE_ARGS;
#endif

    plcml_Init->pCrPhArgs = arr;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
    size_lcml = nIpBuf * sizeof(LCML_WMADEC_BUFHEADERTYPE);
    WMAD_OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, LCML_WMADEC_BUFHEADERTYPE);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[ALLOC] %p\n",__LINE__,pTemp_lcml);
    OMX_PRDSP2(pComponentPrivate->dbg, "Line %d::pTemp_lcml = %p\n",__LINE__,pTemp_lcml);
    if(pTemp_lcml == NULL)
    {
        /* Free previously allocated memory before bailing */
        OMX_WMADECMEMFREE_STRUCT(strmAttr);
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pComponentPrivate_CC->pLcmlBufHeader[INPUT_PORT] = pTemp_lcml;

    OMX_PRBUFFER2(pComponentPrivate->dbg, "nIpBuf = %ld\n",nIpBuf);
    for (i=0; i<nIpBuf; i++)
    {
        pTemp = pComponentPrivate_CC->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nIpBufSize;
        if(pComponentPrivate_CC->dasfmode == 0)
        {
            pTemp->nFilledLen = nIpBufSize;
        } 
        else
        {
            pTemp->nFilledLen = 0;
        }
        pTemp->nVersion.s.nVersionMajor = WMADEC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = WMADEC_MINOR_VER;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirInput;
        WMAD_OMX_MALLOC_SIZE(pTemp_lcml->pIpParam, 
                             sizeof(WMADEC_UAlgInBufParamStruct) + 256,
                             WMADEC_UAlgInBufParamStruct);
        pTemp_char = (char*)pTemp_lcml->pIpParam;
        pTemp_char += 128;
        pTemp_lcml->pIpParam = (WMADEC_UAlgInBufParamStruct*)pTemp_char;
        
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[ALLOC] %p\n",__LINE__,pTemp_lcml->pIpParam);
        if (pTemp_lcml->pIpParam == NULL)
        {
            /* Free previously allocated memory before bailing */
            OMX_WMADECMEMFREE_STRUCT(strmAttr);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:::[FREE] %p\n",__LINE__,pTemp_lcml);
            OMX_WMADECMEMFREE_STRUCT(pTemp_lcml );

            goto EXIT;
        }
        pTemp_lcml->pIpParam->bLastBuffer = 0;

        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */
        pTemp->nFlags = NORMAL_BUFFER;

        pTemp_lcml++;
    }

    /* Allocate memory for all output buffer headers..
     * This memory pointer will be sent to LCML */
    size_lcml = pComponentPrivate_CC->pOutputBufferList->numBuffers * sizeof(LCML_WMADEC_BUFHEADERTYPE);
    WMAD_OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, LCML_WMADEC_BUFHEADERTYPE);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[ALLOC] %p\n",__LINE__,pTemp_lcml);
    if(pTemp_lcml == NULL)
    {
        /* Free previously allocated memory before bailing */
        OMX_WMADECMEMFREE_STRUCT(strmAttr);
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pComponentPrivate_CC->pLcmlBufHeader[OUTPUT_PORT] = pTemp_lcml;
    for (i=0; i<nOpBuf; i++)
    {
        pTemp = pComponentPrivate_CC->pOutputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nOpBufSize;
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = WMADEC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = WMADEC_MINOR_VER;
        pComponentPrivate_CC->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */

        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirOutput;

        pTemp->nFlags = NORMAL_BUFFER;

        pTemp++;
        pTemp_lcml++;
    }
    pComponentPrivate_CC->bPortDefsAllocated = 1;
    
#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate_CC->nLcml_nCntIp = 0;
    pComponentPrivate_CC->nLcml_nCntOpReceived = 0;
#endif
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting WMADECFill_LCMLInitParams",__LINE__);

    pComponentPrivate_CC->bInitParamsInitialized = 1;   
 EXIT:
    return eError;
}


/* ========================================================================== */
/**
 * @WMADEC_StartComponentThread() This function is called by the component to create
 * the component thread, command pipe, data pipe and LCML Pipe.
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
OMX_ERRORTYPE WMADEC_StartComponentThread(OMX_HANDLETYPE pComponent)
{

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate =
        (WMADEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
#ifdef UNDER_CE     
    pthread_attr_t attr;
    memset(&attr, 0, sizeof(attr));
    attr.__inheritsched = PTHREAD_EXPLICIT_SCHED;
    attr.__schedparam.__sched_priority = OMX_AUDIO_DECODER_THREAD_PRIORITY;
#endif

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Inside  WMADEC_StartComponentThread\n", __LINE__);
    /* Initialize all the variables*/
    pComponentPrivate->bIsStopping = 0;
    pComponentPrivate->bIsEOFSent = 0;



    OMX_PRINT2(pComponentPrivate->dbg, "WMADEC_StartComponentThread  pComponentPrivate = %p\n",
                  pComponentPrivate);
    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->cmdDataPipe);
    if (eError)
    {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->dataPipe);
    if (eError)
    {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* create the pipe used to send commands to the thread */
    eError = pipe (pComponentPrivate->cmdPipe);
    if (eError)
    {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* Create the Component Thread */
#ifdef UNDER_CE     
    eError = pthread_create (&(pComponentPrivate->ComponentThread), &attr, 
                             ComponentThread, pComponentPrivate);
#else   
    eError = pthread_create (&(pComponentPrivate->ComponentThread), NULL,
                             ComponentThread, pComponentPrivate);
#endif
    if (eError || !pComponentPrivate->ComponentThread)
    {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pComponentPrivate->bCompThreadStarted = 1;
 EXIT:
    return eError;
}

/* ========================================================================== */
/**
 * @Mp3Dec_FreeCompResources() This function is called by the component during
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

OMX_ERRORTYPE WMADEC_FreeCompResources(OMX_HANDLETYPE pComponent)
{                                                       
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0;
    OMX_U32 nOpBuf = 0;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: WMADEC_FreeCompResources\n", __LINE__);

    OMX_PRBUFFER1(pComponentPrivate->dbg, "%d:::pComponentPrivate->bPortDefsAllocated = %ld\n", 
                  __LINE__,pComponentPrivate->bPortDefsAllocated);
    if (pComponentPrivate->bPortDefsAllocated)
    {
        nIpBuf = pComponentPrivate->pPortDef[INPUT_PORT]->nBufferCountActual;
        nOpBuf = pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferCountActual;
    }

    if (pComponentPrivate->bCompThreadStarted)
    {
        err = close (pComponentPrivate->dataPipe[0]);

        if (0 != err && OMX_ErrorNone == eError)
        {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->dataPipe[1]);
        if (0 != err && OMX_ErrorNone == eError)
        {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->cmdPipe[0]);
        if (0 != err && OMX_ErrorNone == eError)
        {
            eError = OMX_ErrorHardware;
        }

        err = close (pComponentPrivate->cmdPipe[1]);
        if (0 != err && OMX_ErrorNone == eError)
        {
            eError = OMX_ErrorHardware;
        }

        OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);
        err = close (pComponentPrivate->cmdDataPipe[0]);
        OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);
        if (0 != err && OMX_ErrorNone == eError)
        {
            OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);
            eError = OMX_ErrorHardware;
            OMX_ERROR2(pComponentPrivate->dbg, "%d :: Error while closing cmdPipe\n",__LINE__);
        }

        OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);
        err = close (pComponentPrivate->cmdDataPipe[1]);
        OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);
        if (0 != err && OMX_ErrorNone == eError)
        {
            OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);
            eError = OMX_ErrorHardware;
            OMX_ERROR2(pComponentPrivate->dbg, "%d :: Error while closing cmdPipe\n",__LINE__);
        }
 
    }

    if (pComponentPrivate->bPortDefsAllocated) 
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);
        if (pComponentPrivate->pPortDef[INPUT_PORT])
        {
            OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);
            OMX_PRCOMM2(pComponentPrivate->dbg, "freeing pComponentPrivate->pPortDef[INPUT_PORT] = \
                          %p\n",pComponentPrivate->pPortDef[INPUT_PORT]);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[FREE] %p\n",__LINE__,
                            pComponentPrivate->pPortDef[INPUT_PORT]);
            OMX_WMADECMEMFREE_STRUCT(pComponentPrivate->pPortDef[INPUT_PORT]);
        }
        OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);

        if (pComponentPrivate->pPortDef[OUTPUT_PORT])
        {
            OMX_PRINT1(pComponentPrivate->dbg, "%d:::[WMADEC_FreeCompResources] \n", __LINE__);
            OMX_PRCOMM2(pComponentPrivate->dbg, "pComponentPrivate->pPortDef[OUTPUT_PORT] = %p\n",
                          pComponentPrivate->pPortDef[OUTPUT_PORT]);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[FREE] %p\n",__LINE__,
                            pComponentPrivate->pPortDef[OUTPUT_PORT]);
            OMX_WMADECMEMFREE_STRUCT(pComponentPrivate->pPortDef[OUTPUT_PORT]);
        }

        if (pComponentPrivate->wmaParams[INPUT_PORT])
        {
            OMX_PRCOMM2(pComponentPrivate->dbg, "pComponentPrivate->wmaParams[INPUT_PORT] = %p\n",
                          pComponentPrivate->wmaParams[INPUT_PORT]);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[FREE] %p\n",__LINE__,
                            pComponentPrivate->wmaParams[INPUT_PORT]);
            OMX_WMADECMEMFREE_STRUCT(pComponentPrivate->wmaParams[INPUT_PORT]);
        }

        if (pComponentPrivate->wmaParams[OUTPUT_PORT])
        {
            OMX_PRCOMM2(pComponentPrivate->dbg, "pComponentPrivate->wmaParams[OUTPUT_PORT] = %p\n",
                          pComponentPrivate->wmaParams[OUTPUT_PORT]);
            OMX_PRCOMM2(pComponentPrivate->dbg, "wmaParams[OUTPUT_PORT]->nPortIndex = %ld\n",
                          pComponentPrivate->wmaParams[OUTPUT_PORT]->nPortIndex);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[FREE] %p\n",__LINE__,
                            pComponentPrivate->wmaParams[OUTPUT_PORT]);
            OMX_WMADECMEMFREE_STRUCT(pComponentPrivate->wmaParams[OUTPUT_PORT]);
            OMX_PRCOMM2(pComponentPrivate->dbg, "after pComponentPrivate->wmaParams[OUTPUT_PORT] = \
                          %p\n",pComponentPrivate->wmaParams[OUTPUT_PORT]);
        }
    }
    pComponentPrivate->bPortDefsAllocated = 0;
#ifndef UNDER_CE
    OMX_PRDSP2(pComponentPrivate->dbg, "\n\n FreeCompResources: Destroying mutexes.\n\n");
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

    OMX_PRINT1(pComponentPrivate->dbg, "Exiting Successfully WMADEC_FreeCompResources()\n");
    return eError;
}



/* ========================================================================== */
/**
 * WMADEC_CleanupInitParams() This functions cleans up initialization 
 * parameters when shutting down
 *
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return OMX Error code
 */
/* ========================================================================== */
OMX_ERRORTYPE WMADEC_CleanupInitParams(OMX_HANDLETYPE pComponent)
{

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;

    LCML_WMADEC_BUFHEADERTYPE *pTemp_lcml;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0;
    OMX_U16 i=0;
    char *pTemp = NULL;    

    if (pComponentPrivate->strmAttr)
    {
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[FREE] %p\n",__LINE__,pComponentPrivate->strmAttr);

        OMX_WMADECMEMFREE_STRUCT(pComponentPrivate->strmAttr);
    }

    if(pComponentPrivate->pDynParams)
    {
        OMX_PRBUFFER2(pComponentPrivate->dbg, ":: [FREE]: pComponentPrivate->pDynParams = %p\n",
                        pComponentPrivate->pDynParams);
                        
        pTemp = (char*)pComponentPrivate->pDynParams;
        if (pTemp != NULL)
        {
            pTemp -= 128;
        }
        pComponentPrivate->pDynParams = (WMADEC_UALGParams*)pTemp;
        OMX_WMADECMEMFREE_STRUCT(pComponentPrivate->pDynParams);
    }
    if (pComponentPrivate->dasfmode == 1)
    {
        OMX_PRBUFFER2(pComponentPrivate->dbg, ":: [FREE]: pComponentPrivate->pParams = %p\n",
                        pComponentPrivate->pParams);
                        
        pTemp = (char*)pComponentPrivate->pParams;
        if (pTemp != NULL)
        {       
            pTemp -= 128;
        }
        pComponentPrivate->pParams = (WMADEC_AudioCodecParams*)pTemp;        
        OMX_WMADECMEMFREE_STRUCT(pComponentPrivate->pParams);
    }    
    
    nIpBuf = pComponentPrivate->nRuntimeInputBuffers;

    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[INPUT_PORT];
    if(pTemp_lcml)
    {
        for(i=0; i<nIpBuf; i++)
        {
            OMX_PRDSP2(pComponentPrivate->dbg, "freeing pTemp_lcml->pIpParam = %p\n", pTemp_lcml->pIpParam);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[FREE] %p\n",__LINE__,pTemp_lcml->pIpParam);
            
            pTemp = (char*)pTemp_lcml->pIpParam;
            if(pTemp != NULL){
                pTemp -= 128;
            }
            pTemp_lcml->pIpParam = (WMADEC_UAlgInBufParamStruct*)pTemp;
       
            OMX_WMADECMEMFREE_STRUCT(pTemp_lcml->pIpParam);
            pTemp_lcml++;
        }
    }
    OMX_PRCOMM2(pComponentPrivate->dbg, "freeing pComponentPrivate->pLcmlBufHeader[INPUT_PORT] = %p\n",
                  pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);
                  
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[FREE] %p\n",__LINE__,
                    pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);
                    
    OMX_WMADECMEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);
    OMX_PRCOMM2(pComponentPrivate->dbg, "freeing pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT] = %p\n",
                  pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT]);
                  
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[FREE] %p\n",__LINE__,
                    pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT]);
                    
    OMX_WMADECMEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT]);
    OMX_PRINT1(pComponentPrivate->dbg, "Exiting Successfully WMADEC_CleanupInitParams()\n");
    return eError;
}

/* ========================================================================== */
/**
 * @WMADEC_StopComponentThread() This function is called by the component during
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

OMX_ERRORTYPE WMADEC_StopComponentThread(OMX_HANDLETYPE pComponent)
{

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate = (WMADEC_COMPONENT_PRIVATE *)
        pHandle->pComponentPrivate;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE threadError = OMX_ErrorNone;
    int pthreadError = 0;

    /*Join the component thread */
    pComponentPrivate->bIsStopping = 1;
    write (pComponentPrivate->cmdPipe[1], &pComponentPrivate->bIsStopping, sizeof(OMX_U16));    
    OMX_PRINT1(pComponentPrivate->dbg, "%d:::About to call pthread_join\n",__LINE__);
    pthreadError = pthread_join (pComponentPrivate->ComponentThread,
                                 (void*)&threadError);
    OMX_PRDSP2(pComponentPrivate->dbg, "%d:::called pthread_join\n",__LINE__);
    if (0 != pthreadError)
    {
        eError = OMX_ErrorHardware;
        OMX_ERROR2(pComponentPrivate->dbg, "%d :: Error while closing Component Thread - pthreadError = %d\n",
                       __LINE__,pthreadError);
    }

    /*Check for the errors */
    if (OMX_ErrorNone != threadError && OMX_ErrorNone != eError)
    {
        eError = OMX_ErrorInsufficientResources;
        OMX_ERROR2(pComponentPrivate->dbg, "%d :: Error while closing Component Thread\n",__LINE__);
    }
    OMX_PRINT1(pComponentPrivate->dbg, "Exiting Successfully from Stop Component thread\n");
    return eError;
}


/* ========================================================================== */
/**
 * @WMADECHandleCommand() This function is called by the component when ever it
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

OMX_U32 WMADECHandleCommand (WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{

    OMX_COMMANDTYPE command;
    OMX_STATETYPE commandedState;
    OMX_U32 commandData;
    char *pArgs = "damedesuStr";
#ifndef UNDER_CE
    OMX_U16 i,j;
    int numCalls;
#endif
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    LCML_DSP_INTERFACE *pLcmlHandle;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    pLcmlHandle = (LCML_DSP_INTERFACE *) pComponentPrivate->pLcmlHandle;

    OMX_PRINT1(pComponentPrivate->dbg, "WMADECHandleCommand\n");
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering WMADECHandleCommand Function - curState = %d\n",
                   __LINE__,pComponentPrivate->curState);
    read (pComponentPrivate->cmdPipe[0], &command, sizeof (command));
    if ((int)(read) == -1){
        pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
    }
    OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: Just read from the command pipe\n",__LINE__);
    OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: command = %d\n",__LINE__,command);
    read (pComponentPrivate->cmdDataPipe[0], &commandData, sizeof (commandData));
    if ((int)(read) == -1){
        pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
    }    

    OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: command = %d\n",__LINE__,command);
    OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: commandData = %ld\n",__LINE__,commandData);
    
#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedCommand(pComponentPrivate->pPERFcomp,
                         command,
                         commandData,
                         PERF_ModuleLLMM);
#endif  

    
    if (command == OMX_CommandStateSet)
    {
        commandedState = (OMX_STATETYPE)commandData;
        switch(commandedState)
        {
        case OMX_StateIdle:
            eError = WMADEC_CommandToIdle(pComponentPrivate);
            break;

        case OMX_StateExecuting:

            eError = WMADEC_CommandToExecuting(pComponentPrivate);
            break;

        case OMX_StateLoaded:

            eError = WMADEC_CommandToLoaded(pComponentPrivate);
            break;

        case OMX_StatePause:

            eError = WMADEC_CommandToPause(pComponentPrivate);
            break;


        case OMX_StateWaitForResources:

            eError = WMADEC_CommandToWaitForResources(pComponentPrivate);
            break;


        case OMX_StateInvalid:

            OMX_PRSTATE2(pComponentPrivate->dbg, "%d: WMADECHandleCommand: Cmd OMX_StateInvalid:\n",
                          __LINE__);
                
            if (pComponentPrivate->curState == commandedState)
            {
                pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventError,
                                                        OMX_ErrorSameState,
                                                        OMX_TI_ErrorMinor,
                                                        NULL);
                OMX_ERROR2(pComponentPrivate->dbg, "%d :: Error: Same State Given by Application\n",
                              __LINE__);
            }
            else
            {
                if (pComponentPrivate->curState != OMX_StateWaitForResources && 
                    pComponentPrivate->curState != OMX_StateInvalid && 
                    pComponentPrivate->curState != OMX_StateLoaded)
                {

                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlDestroy, (void *)pArgs);
                }
                OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
                pComponentPrivate->curState = OMX_StateInvalid;

                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventError, 
                                                       OMX_ErrorInvalidState,
                                                       OMX_TI_ErrorMajor, 
                                                       NULL);

                WMADEC_CleanupInitParams(pHandle);
            }
            break;

        case OMX_StateMax:
            OMX_PRSTATE2(pComponentPrivate->dbg, "%d: WMADECHandleCommand: Cmd OMX_StateMax::\n",
                          __LINE__);
            break;
        } /* End of Switch */

    }
    else if (command == OMX_CommandMarkBuffer)
    {
        OMX_PRBUFFER2(pComponentPrivate->dbg, "command OMX_CommandMarkBuffer received %d\n",__LINE__);
        if(!pComponentPrivate->pMarkBuf)
        {
            OMX_PRBUFFER2(pComponentPrivate->dbg, "command OMX_CommandMarkBuffer received %d\n",__LINE__);
            /* TODO Need to handle multiple marks */
            pComponentPrivate->pMarkBuf = (OMX_MARKTYPE *)(commandData);
        }
    }
    else if (command == OMX_CommandPortDisable)
    {
        if (!pComponentPrivate->bDisableCommandPending)
        if(commandData == 0x0){
            /* disable port */
            for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
                OMX_PRBUFFER2(pComponentPrivate->dbg, "pComponentPrivate->pInputBufferList->bBufferPending[%ld] = %d\n",i,
                              pComponentPrivate->pInputBufferList->bBufferPending[i]);
                if (WMADEC_IsPending(pComponentPrivate,pComponentPrivate->pInputBufferList->pBufHdr[i],OMX_DirInput)) {
                    /* Real solution is flush buffers from DSP.  Until we have the ability to do that 
                       we just call EmptyBufferDone() on any pending buffers */
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "Forcing EmptyBufferDone\n");
#ifdef __PERF_INSTRUMENTATION__
                    PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                      PREF(pComponentPrivate->pInputBufferList->pBufHdr[i], pBuffer),
                                      0,
                                      PERF_ModuleHLMM);
#endif                  
                    pComponentPrivate->cbInfo.EmptyBufferDone (pComponentPrivate->pHandle,
                                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                                               pComponentPrivate->pInputBufferList->pBufHdr[i]);
                    pComponentPrivate->nEmptyBufferDoneCount++;
                }
            }
            pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled = OMX_FALSE;
        }            
        
        if(commandData == -1){
            /* disable port */
            pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled = OMX_FALSE;
        }
        if(commandData == 0x1 || commandData == -1)
        {
                
            pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled = OMX_FALSE;
            if (pComponentPrivate->curState == OMX_StateExecuting) 
            {
                pComponentPrivate->bNoIdleOnStop = OMX_TRUE;
                if (pComponentPrivate->codecStop_waitingsignal == 0){
                    pthread_mutex_lock(&pComponentPrivate->codecStop_mutex); 
                } 
                eError = LCML_ControlCodec(
                                           ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           MMCodecControlStop,(void *)pArgs);
                if (pComponentPrivate->codecStop_waitingsignal == 0){
                    pthread_cond_wait(&pComponentPrivate->codecStop_threshold, &pComponentPrivate->codecStop_mutex);
                    pComponentPrivate->codecStop_waitingsignal = 0;
                    pthread_mutex_unlock(&pComponentPrivate->codecStop_mutex);
                }
            }
        }
        OMX_PRCOMM2(pComponentPrivate->dbg, "I'm here Line %d\n",__LINE__);
        OMX_PRCOMM2(pComponentPrivate->dbg, "commandData = %ld\n",commandData);
        OMX_PRCOMM2(pComponentPrivate->dbg, "pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated = %d\n",
                      pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated);
        OMX_PRCOMM2(pComponentPrivate->dbg, "pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated = %d\n",
                      pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated);
    
        if(commandData == 0x0)
        {
            if(!pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated)
            {
                /* return cmdcomplete event if input unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortDisable,
                                                       INPUT_PORT, 
                                                       NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else
            {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }
        
        if(commandData == 0x1)
        {
            if (!pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated)
            {
                /* return cmdcomplete event if output unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortDisable,
                                                       OUTPUT_PORT, 
                                                       NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else
            {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

        if(commandData == -1){
            if (!pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated && 
                !pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated)
            {

                /* return cmdcomplete event if inout & output unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortDisable,
                                                       INPUT_PORT, 
                                                       NULL);

                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortDisable,
                                                       OUTPUT_PORT, 
                                                       NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else
            {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }
    }
    else if (command == OMX_CommandPortEnable) 
    {
        if(!pComponentPrivate->bEnableCommandPending){
            if(commandData == 0x0 || commandData == -1)
            {
                /* enable in port */
                OMX_PRCOMM2(pComponentPrivate->dbg, "setting input port to enabled\n");
                pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled = OMX_TRUE;
                OMX_PRCOMM2(pComponentPrivate->dbg, "pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled = %d\n",
                              pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled);
            }
            if(commandData == 0x1 || commandData == -1)
            {
                /* enable out port */
                char *pArgs = "damedesuStr";
                OMX_PRCOMM2(pComponentPrivate->dbg, "============SETTING OUTPUT PORT TO ENABLED====================\n");
                pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled = OMX_TRUE;
                OMX_PRCOMM2(pComponentPrivate->dbg, "pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled = %d\n",
                              pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled);
                
                if (pComponentPrivate->curState == OMX_StateExecuting && pComponentPrivate->bDspStoppedWhileExecuting){
                    pComponentPrivate->bDspStoppedWhileExecuting = OMX_FALSE;
                    OMX_PRDSP2(pComponentPrivate->dbg, "=========================CALLING CONTROL START AT POT ENABLE===========================\n");
                    eError = LCML_ControlCodec(
                                               ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStart,(void *)pArgs);
                }                
            }
        }
 
        OMX_PRSTATE2(pComponentPrivate->dbg, "commandData = %ld\n",commandData);
        OMX_PRSTATE2(pComponentPrivate->dbg, "pComponentPrivate->curState = %d\n",
                      pComponentPrivate->curState);
        OMX_PRCOMM2(pComponentPrivate->dbg, "pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated = %d\n",
                      pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated);
        if(commandData == 0x0 )
        {
            if(pComponentPrivate->curState == OMX_StateLoaded ||  
               pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated)
            {
                if(pComponentPrivate->AlloBuf_waitingsignal)
                {
                    pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
                    pComponentPrivate->AlloBuf_waitingsignal = 0;
                    pthread_cond_signal(&pComponentPrivate->AlloBuf_threshold);
                    pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);
                }
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortEnable,
                                                       INPUT_PORT, NULL);
                pComponentPrivate->bEnableCommandPending = 0;
            }
            else
            {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->bEnableCommandParam = commandData;
            }
        }       
        else if(commandData == 0x1) 
        {
            if(pComponentPrivate->curState == 
               OMX_StateLoaded || 
               pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated)
            {
                if(pComponentPrivate->AlloBuf_waitingsignal){
                    pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
                    pComponentPrivate->AlloBuf_waitingsignal = 0;
                    pthread_cond_signal(&pComponentPrivate->AlloBuf_threshold);
                    pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);    
                } 
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortEnable,
                                                       OUTPUT_PORT, NULL);
                if(pComponentPrivate->reconfigOutputPort){
                    OMX_PRCOMM2(pComponentPrivate->dbg, "==============================We can send input buffers againg!=========================\n");
                    pComponentPrivate->reconfigOutputPort = 0;
                    WMADECFill_LCMLInitParamsEx(pHandle,commandData);
                }          

                OMX_PRBUFFER2(pComponentPrivate->dbg, "=======Before sending pending buffers==============\n");
                OMX_PRBUFFER2(pComponentPrivate->dbg, "pComponentPrivate->nNumInputBufPending=%ld\n",pComponentPrivate->nNumInputBufPending);    
                for (i=0; i < pComponentPrivate->nNumInputBufPending; i++){                
                    if (pComponentPrivate->pInputBufHdrPending[i]){
                        LCML_WMADEC_BUFHEADERTYPE *pLcmlHdr;
                        if (!WMADEC_IsPending(pComponentPrivate, 
                                              pComponentPrivate->pInputBufHdrPending[i], 
                                              OMX_DirInput) ){
                            WMADECGetCorresponding_LCMLHeader(
                                                              pComponentPrivate,
                                                              pComponentPrivate->pInputBufHdrPending[i]->pBuffer, 
                                                              OMX_DirInput, 
                                                              &pLcmlHdr);                                                       
                            WMADEC_SetPending(pComponentPrivate,
                                              pComponentPrivate->pInputBufHdrPending[i],
                                              OMX_DirInput);
                            OMX_PRBUFFER2(pComponentPrivate->dbg, "QUEUEING INPUT BUFFER\n");                    
                            OMX_PRBUFFER2(pComponentPrivate->dbg, "nfilledLen= %ld\n",pComponentPrivate->pInputBufHdrPending[i]->nFilledLen);                     
                            eError = LCML_QueueBuffer(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                      EMMCodecInputBuffer,  
                                                      (OMX_U8 *)pComponentPrivate->pInputBufHdrPending[i]->pBuffer,
                                                      pComponentPrivate->pInputBufHdrPending[i]->nAllocLen,
                                                      pComponentPrivate->pInputBufHdrPending[i]->nFilledLen,
                                                      (OMX_U8 *) pLcmlHdr->pIpParam,
                                                      sizeof(WMADEC_UAlgInBufParamStruct),
                                                      NULL);                            
                            if (eError != OMX_ErrorNone)
                            {
                                OMX_ERROR4(pComponentPrivate->dbg, "ERROR IN QUEUEBUFFER\n");
                                eError = OMX_ErrorHardware;
                                goto EXIT;
                            }                                           
                            pComponentPrivate->lcml_nCntIp++;                   

                        }                    
                    }                
                }    
                pComponentPrivate->nNumInputBufPending=0; 
                pComponentPrivate->bEnableCommandPending = 0;
            }
            else
            {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->bEnableCommandParam = commandData;
            }                
        }    
        else if(commandData == -1 )
        {
            if(pComponentPrivate->curState == OMX_StateLoaded || 
               (pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated && 
                pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated))
            {
                if(pComponentPrivate->AlloBuf_waitingsignal)
                {
                    pthread_mutex_lock(&pComponentPrivate->AlloBuf_mutex); 
                    pComponentPrivate->AlloBuf_waitingsignal = 0;
                    pthread_cond_signal(&pComponentPrivate->AlloBuf_threshold);
                    pthread_mutex_unlock(&pComponentPrivate->AlloBuf_mutex);                  
                }
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortEnable,
                                                       INPUT_PORT, NULL);

                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortEnable,
                                                       OUTPUT_PORT, NULL);

                pComponentPrivate->bEnableCommandPending = 0;
                pComponentPrivate->reconfigOutputPort = 0;
                WMADECFill_LCMLInitParamsEx(pHandle,commandData);     
            }
            else 
            {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->bEnableCommandParam = commandData;
            }
        }
    }
    else if (command == OMX_CommandFlush)
    {
        OMX_U32 aParam[3] = {0};
        if(commandData == 0x0 || commandData == -1)
        {
            if (pComponentPrivate->nUnhandledEmptyThisBuffers == 0)  {
                pComponentPrivate->bFlushInputPortCommandPending = OMX_FALSE;
         
                aParam[0] = USN_STRMCMD_FLUSH; 
                aParam[1] = 0x0; 
                aParam[2] = 0x0; 

                if (pComponentPrivate->codecFlush_waitingsignal == 0){
                    pthread_mutex_lock(&pComponentPrivate->codecFlush_mutex);
                }
                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlStrmCtrl, (void*)aParam);
                
                if (pComponentPrivate->codecFlush_waitingsignal == 0){
                    pthread_cond_wait(&pComponentPrivate->codecFlush_threshold, &pComponentPrivate->codecFlush_mutex);
                    pComponentPrivate->codecFlush_waitingsignal = 0;
                    pthread_mutex_unlock(&pComponentPrivate->codecFlush_mutex);
                }
                if (eError != OMX_ErrorNone) {
                    goto EXIT;
                }
            }else {
                pComponentPrivate->bFlushInputPortCommandPending = OMX_TRUE;
            }   

        }


        if(commandData == 0x1 || commandData == -1)
        {
            if (pComponentPrivate->nUnhandledFillThisBuffers == 0)  {
                pComponentPrivate->bFlushOutputPortCommandPending = OMX_FALSE;
                //ComponentPrivate->first_buff = 0;

                aParam[0] = USN_STRMCMD_FLUSH; 
                aParam[1] = 0x1; 
                aParam[2] = 0x0; 

                OMX_PRCOMM2(pComponentPrivate->dbg, "Flushing output port\n");
                if (pComponentPrivate->codecFlush_waitingsignal == 0){
                    pthread_mutex_lock(&pComponentPrivate->codecFlush_mutex);
                }
                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlStrmCtrl, (void*)aParam);
                
                if (pComponentPrivate->codecFlush_waitingsignal == 0){
                    pthread_cond_wait(&pComponentPrivate->codecFlush_threshold, &pComponentPrivate->codecFlush_mutex);
                    pComponentPrivate->codecFlush_waitingsignal = 0;
                    pthread_mutex_unlock(&pComponentPrivate->codecFlush_mutex);
                }
                if (eError != OMX_ErrorNone) {
                    goto EXIT;
                }
            } else {
                pComponentPrivate->bFlushOutputPortCommandPending = OMX_TRUE; 
            }
        } 

    }
 EXIT:
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting WMADECHandleCommand Function\n",__LINE__);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Returning %d\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
 * @WMADECHandleDataBuf_FromApp() This function is called by the component when ever it
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

OMX_ERRORTYPE WMADECHandleDataBuf_FromApp(OMX_BUFFERHEADERTYPE* pBufHeader,
                                          WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_PRBUFFER2(pComponentPrivate->dbg, "At handle data buff from app device string is =%p\n",pComponentPrivate->sDeviceString);
    OMX_PRCOMM2(pComponentPrivate->dbg, "At handle databuff from app pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated =%d\n",
                  pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated);
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDefIn = NULL;
    OMX_ERRORTYPE eError                                         = OMX_ErrorNone;
    OMX_DIRTYPE eDir;
    LCML_WMADEC_BUFHEADERTYPE *pLcmlHdr;
    LCML_DSP_INTERFACE *pLcmlHandle;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate_CC;
    OMX_U32 cmdValues[3];
    char *pArgs = "damedesuStr";
    char *p = "hello";

    pComponentPrivate_CC = pComponentPrivate;   
    pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering WMADECHandleDataBuf_FromApp Function\n",__LINE__);

    /*Find the direction of the received buffer from buffer list */
    eError = WMADECGetBufferDirection(pBufHeader, &eDir);
    if (eError != OMX_ErrorNone)
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: The PBufHeader is not found in the list\n", __LINE__);
        goto EXIT;
    }

    if (eDir == OMX_DirInput)
    {
        pComponentPrivate->nUnhandledEmptyThisBuffers--;    
        pPortDefIn = pComponentPrivate->pPortDef[OMX_DirInput];
        if ( (pBufHeader->nFilledLen > 0) || (pBufHeader->nFlags & OMX_BUFFERFLAG_EOS))
        {
            pComponentPrivate->bBypassDSP = 0;
            eError = WMADECGetCorresponding_LCMLHeader(pComponentPrivate,
                                                       pBufHeader->pBuffer, 
                                                       OMX_DirInput, 
                                                       &pLcmlHdr);
            if (eError != OMX_ErrorNone)
            {
                OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Invalid Buffer Came ...\n",__LINE__);
                goto EXIT;
            }

            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d Comp:: Sending Filled Input buffer = %p, %p to LCML\n", 
                           __LINE__,pBufHeader,pBufHeader->pBuffer);
                           
            OMX_PRBUFFER2(pComponentPrivate->dbg, "pBufHeader->nFilledLen :%ld \n",pBufHeader->nFilledLen);                      
            pLcmlHdr->pIpParam->bLastBuffer = 0;
            
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                              PREF(pBufHeader,pBuffer),
                              pPortDefIn->nBufferSize, 
                              PERF_ModuleCommonLayer);
#endif
            /** ring tone**/
            if(pComponentPrivate->SendAfterEOS == 1){
                pComponentPrivate->nNumInputBufPending = 0;
                pComponentPrivate->nNumOutputBufPending = 0;
                if(pComponentPrivate->dasfmode == 1)
                {
                    OMX_U32 pValues[4];
                    OMX_PRDSP2(pComponentPrivate->dbg, "%d :: ---- Comp: DASF Functionality is ON ---\n",__LINE__);
                    if (pComponentPrivate->pHeaderInfo->iChannel == 1)
                    {
                        pComponentPrivate->pParams->iAudioFormat = WMA_MONO_CHANNEL;
                    }
                    else
                    {
                        pComponentPrivate->pParams->iAudioFormat = WMA_STEREO_NON_INTERLEAVED;
                    }
                    OMX_PRINT1(pComponentPrivate->dbg, "pParams->iAudioFormat = %d\n", pComponentPrivate->pParams->iAudioFormat);
                    pComponentPrivate->pParams->iStrmId = pComponentPrivate->streamID;
                    pComponentPrivate->pParams->iSamplingRate = 
                        pComponentPrivate->pHeaderInfo->iSamplePerSec;

                    OMX_PRINT1(pComponentPrivate->dbg, "pParams->iSamplingRate = %ld\n",
                                  pComponentPrivate->pParams->iSamplingRate);
                    OMX_PRINT1(pComponentPrivate->dbg, "pParams->iStrmId = %ld\n",
                                  pComponentPrivate->pParams->iStrmId);
                    OMX_PRINT1(pComponentPrivate->dbg, "pParams->iAudioFormat = %d\n",
                                  pComponentPrivate->pParams->iAudioFormat);

                    pValues[0] = USN_STRMCMD_SETCODECPARAMS;
                    pValues[1] = (OMX_U32)pComponentPrivate->pParams;
                    pValues[2] = sizeof(WMADEC_AudioCodecParams);
                    pValues[3] = OUTPUT_PORT;
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                               EMMCodecControlStrmCtrl,(void *)pValues);
                    if(eError != OMX_ErrorNone)
                    {
                        OMX_ERROR4(pComponentPrivate->dbg, "%d: Error Occurred in Codec StreamControl..\n",
                                      __LINE__);
                        goto EXIT;
                    }
                }                               
#ifdef ANDROID //this is used to separate changes made for integration with opencore 2.1 or later versions

                /* check if the buffer contains config data, it should only be the case in the fist input buffer*/
                if(pBufHeader->nFlags & OMX_BUFFERFLAG_CODECCONFIG){
                    pComponentPrivate->bConfigData = 1; /* changed the if statement to use the buffer flag, now this flag doesnt do anything
                                                           we will leave it for now incase it becomes useful */
                    OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: UTIL: Before parsing \n",__LINE__);
                    eError=WMADEC_Parser( pBufHeader->pBuffer,pComponentPrivate->rcaheader);
                    OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: UTIL: After parsing \n", __LINE__);
                    if (eError != OMX_ErrorNone)
                    {
                        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Parsing invalid ...\n",__LINE__);
                        goto EXIT;
                    }                                   
                    /*******************Updating if needed*********************/
                    OMX_ERROR4(pComponentPrivate->dbg, "OLD: pComponentPrivate->wmaParams[OUTPUT_PORT]->nSamplingRate=%ld \n",pComponentPrivate->wmaParams[OUTPUT_PORT]->nSamplingRate);
                    /* if(pComponentPrivate->wma_op->nSamplingRate != iSamplePerSec){
                       pComponentPrivate->wma_op->nSamplingRate=iSamplePerSec;
                       pComponentPrivate->reconfigOutputPort=1;
                       OMX_ERROR4(pComponentPrivate->dbg, "=================RECONFIGURATION NEEDED===============\n");
                       OMX_ERROR4(pComponentPrivate->dbg, "NEW: pComponentPrivate->wmaParams[OUTPUT_PORT]->nSamplingRate=%d \n",pComponentPrivate->wmaParams[OUTPUT_PORT]->nSamplingRate);
                       }
                       if(pComponentPrivate->wma_op->nChannels != iChannel){
                       pComponentPrivate->wma_op->nChannels = iChannel;
                       pComponentPrivate->reconfigOutputPort=1;
                       OMX_ERROR4(pComponentPrivate->dbg, "=================RECONFIGURATION NEEDED===============\n");
                       }
                       if(pComponentPrivate->reconfigOutputPort){                    
                       pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                       pComponentPrivate->pHandle->pApplicationPrivate,
                       OMX_EventPortSettingsChanged,
                       OUTPUT_PORT,
                       0,
                       NULL);                        
                       OMX_ERROR4(pComponentPrivate->dbg, "================= sent reconfig event to client===============\n"); */
                    pBufHeader->nFilledLen = 0;
                    pComponentPrivate->cbInfo.EmptyBufferDone (
                                                               pComponentPrivate->pHandle,
                                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                                               pBufHeader
                                                               );
                    pComponentPrivate->nEmptyBufferDoneCount++;

                    // }
                    pComponentPrivate->bConfigData = 0;
                    goto EXIT;
                }
#endif
                if( pComponentPrivate->dasfmode )
                {
                    pComponentPrivate->pDynParams->iOutputFormat = WMA_IAUDIO_BLOCK; /* EAUDIO_BLOCKED */
                }
                else
                {
                    pComponentPrivate->pDynParams->iOutputFormat = WMA_IAUDIO_INTERLEAVED; /* EAUDIO_INTERLEAVED */
                }
                pComponentPrivate->pDynParams->size = sizeof( WMADEC_UALGParams );
                
                cmdValues[0] = WMA_IUALG_CMD_SETSTATUS;
                cmdValues[1] = (OMX_U32)( pComponentPrivate->pDynParams );
                cmdValues[2] = sizeof( WMADEC_UALGParams );

                p = (void *)&cmdValues;
                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                           EMMCodecControlAlgCtrl, (void *)p);
                if(eError != OMX_ErrorNone){
                    OMX_ERROR4(pComponentPrivate->dbg, "%d: Error Occurred in Codec algctrl..\n", __LINE__);
                    goto EXIT;
                }
                pComponentPrivate->bBypassDSP = 0;

                eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)(pComponentPrivate->pLcmlHandle))->pCodecinterfacehandle,
                                           EMMCodecControlStart, (void *)pArgs);

                if(eError != OMX_ErrorNone){
                    OMX_ERROR4(pComponentPrivate->dbg, "%d: Error Occurred in Codec Start..\n", __LINE__);
                    goto EXIT;
                }
                pComponentPrivate->SendAfterEOS = 0;                            
            }
            if (pBufHeader->nFlags & OMX_BUFFERFLAG_EOS){
                pLcmlHdr->pIpParam->bLastBuffer = 1;
                pComponentPrivate->SendAfterEOS = 1;
                OMX_PRINT1(pComponentPrivate->dbg, "%d :: OMX_WmaDec_Utils.c : pComponentPrivate->SendAfterEOS %d\n",__LINE__,pComponentPrivate->SendAfterEOS);                      
            }
            /*Store tick count information*/
            pComponentPrivate->arrBufIndexTick[pComponentPrivate->IpBufindex] = pBufHeader->nTickCount;
            /* Store time stamp information */
            pComponentPrivate->arrBufIndex[pComponentPrivate->IpBufindex] = pBufHeader->nTimeStamp;
            pComponentPrivate->IpBufindex++;
            pComponentPrivate->IpBufindex %= pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferCountActual;
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: Output Buffer TimeStamp %lld\n", __LINE__, pComponentPrivate->arrBufIndex[pComponentPrivate->IpBufindex]);

            if(pComponentPrivate->first_buffer)
            {
                pComponentPrivate->rcaheader->iPayload=pBufHeader->nFilledLen;
                pComponentPrivate->rcaheader->iMaxPacketSize = pComponentPrivate->rcaheader->iPayload+31;
                memmove(pBufHeader->pBuffer+75,pBufHeader->pBuffer,pBufHeader->nFilledLen);
                memset(pBufHeader->pBuffer,0x00,75);                                                    
                memcpy(pBufHeader->pBuffer + 16, &pComponentPrivate->rcaheader->iMaxPacketSize, 2);
                memcpy(pBufHeader->pBuffer + 36, &pComponentPrivate->rcaheader->iTypeSpecific, 4);
                memcpy(pBufHeader->pBuffer + 42, &pComponentPrivate->rcaheader->iFormatTag, 2);
                memcpy(pBufHeader->pBuffer + 44, &pComponentPrivate->rcaheader->iChannel, 2);
                memcpy(pBufHeader->pBuffer + 46, &pComponentPrivate->rcaheader->iSamplePerSec, 4);
                memcpy(pBufHeader->pBuffer + 50, &pComponentPrivate->rcaheader->iAvgBytesPerSec, 4);
                memcpy(pBufHeader->pBuffer + 54, &pComponentPrivate->rcaheader->iBlockAlign, 2);
                memcpy(pBufHeader->pBuffer + 56, &pComponentPrivate->rcaheader->iValidBitsPerSample, 2);
                memcpy(pBufHeader->pBuffer + 64, &pComponentPrivate->rcaheader->iEncodeOptV, 2);
                memcpy(pBufHeader->pBuffer + 71, &pComponentPrivate->rcaheader->iPayload, 4);
                pBufHeader->nFilledLen+=75;
                pComponentPrivate->first_buffer=0;
            }
            else
            {                                           
                memmove(pBufHeader->pBuffer+5,pBufHeader->pBuffer,pBufHeader->nFilledLen);
                memset(pBufHeader->pBuffer,0x00,5);
                memcpy(pBufHeader->pBuffer + 1, &pComponentPrivate->rcaheader->iPayload, 4);
                pBufHeader->nFilledLen+=5;                                                      
            }   
            OMX_PRBUFFER2(pComponentPrivate->dbg, "Before sending input buffer\n");
            OMX_PRBUFFER2(pComponentPrivate->dbg, "pBufHeader->nFilledLen=%ld\n",pBufHeader->nFilledLen);                                                

            if (pComponentPrivate->curState == OMX_StateExecuting)
            {
                if(!pComponentPrivate->bDspStoppedWhileExecuting)
                {
                    if (!WMADEC_IsPending(pComponentPrivate,pBufHeader,OMX_DirInput))
                    {
                        if(!pComponentPrivate->reconfigOutputPort){
                            WMADEC_SetPending(pComponentPrivate,pBufHeader,OMX_DirInput);
                            eError = LCML_QueueBuffer(pLcmlHandle->pCodecinterfacehandle,
                                                      EMMCodecInputBuffer,  
                                                      (OMX_U8 *)pBufHeader->pBuffer, 
                                                      pBufHeader->nAllocLen,
                                                      pBufHeader->nFilledLen,
                                                      (OMX_U8 *) pLcmlHdr->pIpParam,
                                                      sizeof(WMADEC_UAlgInBufParamStruct),
                                                      NULL);                                    
                            if (eError != OMX_ErrorNone)
                            {
                                OMX_ERROR4(pComponentPrivate->dbg, "ERROR IN QUEUEBUFFER\n");
                                OMX_ERROR4(pComponentPrivate->dbg, "%d ::IssuingDSP IP: Error Occurred\n",
                                               __LINE__);
                                eError = OMX_ErrorHardware;
                                goto EXIT;
                            }                                           
                            pComponentPrivate->lcml_nCntIp++;
                        }
                        else
                        {
                            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: Bypassing the sending of input buffers to lcml\n",__LINE__);
                            pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pBufHeader;
                            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->nNumInputBufPending=%ld\n",__LINE__,
                                          pComponentPrivate->nNumInputBufPending);
                            /*pComponentPrivate->cbInfo.EmptyBufferDone (pComponentPrivate->pHandle,
                              pComponentPrivate->pHandle->pApplicationPrivate,
                              pBufHeader
                              );
                            */
                            OMX_PRBUFFER2(pComponentPrivate->dbg, "Don't queue buffers during a reconfig\n");
                        }                                 
                    }
                }
                else
                {
#ifdef __PERF_INSTRUMENTATION__
                    PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                      PREF(pBufHeader,pBuffer),
                                      0,
                                      PERF_ModuleHLMM);
#endif          
                    pBufHeader->nFilledLen = 0;
                    pComponentPrivate->cbInfo.EmptyBufferDone (pComponentPrivate->pHandle,
                                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                                               pBufHeader);
            
                }
            }
            else
            {
                pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = 
                    pBufHeader;
            }
        }
        else 
        {
            pComponentPrivate->bBypassDSP = 1;
            OMX_PRBUFFER2(pComponentPrivate->dbg, "Forcing EmptyBufferDone\n");
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                              PREF(pComponentPrivate->pInputBufferList->pBufHdr[0], pBuffer),
                              0, PERF_ModuleHLMM);
#endif                  
            pBufHeader->nFilledLen = 0;
            pComponentPrivate->cbInfo.EmptyBufferDone (pComponentPrivate->pHandle,
                                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                                       pComponentPrivate->pInputBufferList->pBufHdr[0]);
                
            pComponentPrivate->nEmptyBufferDoneCount++;
                
        }
        if(pBufHeader->nFlags & OMX_BUFFERFLAG_EOS)
        {
            pComponentPrivate->bIsEOFSent = 1;
            OMX_PRCOMM2(pComponentPrivate->dbg, "Setting pComponentPrivate->bIsEOFSent\n");
            if(pComponentPrivate->dasfmode == 0)
            {
                /* copy flag to output buffer header */
                OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: UTIL: Copying Flag from input to output \n",__LINE__);
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags |= OMX_BUFFERFLAG_EOS;
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->pOutputBufferList->pBufHdr[0] = %p \n",__LINE__,pComponentPrivate->pOutputBufferList->pBufHdr[0]);
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags = %x \n",__LINE__,(int)pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags);
               
#ifndef UNDER_CE               
                pComponentPrivate->cbInfo.EventHandler( pComponentPrivate->pHandle,
                                                        pComponentPrivate->pHandle->pApplicationPrivate,
                                                        OMX_EventBufferFlag,
                                                        pComponentPrivate->pOutputBufferList->pBufHdr[0]->nOutputPortIndex,
                                                        pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags,
                                                        NULL);
#endif                                                  
                pComponentPrivate->cbInfo.FillBufferDone (pComponentPrivate->pHandle,
                                                          pComponentPrivate->pHandle->pApplicationPrivate,
                                                          pComponentPrivate->pOutputBufferList->pBufHdr[0]);
                                      
                pComponentPrivate->nOutStandingFillDones--;
                OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: Flags has been propagated from input to output port\n",__LINE__);
                pBufHeader->nFlags = NORMAL_BUFFER;
                pLcmlHdr->pIpParam->bLastBuffer = 0;
            }
        }

        if(pBufHeader->pMarkData)
        {            
            /* copy mark to output buffer header */ 
            pComponentPrivate->pOutputBufferList->pBufHdr[0]->pMarkData = 
                pBufHeader->pMarkData;
            pComponentPrivate->pOutputBufferList->pBufHdr[0]->hMarkTargetComponent = 
                pBufHeader->hMarkTargetComponent;

            /* trigger event handler if we are supposed to */ 
            if (pBufHeader->hMarkTargetComponent == pComponentPrivate->pHandle && 
                pBufHeader->pMarkData)
            {
                pComponentPrivate_CC->cbInfo.EventHandler(pComponentPrivate->pHandle, 
                                                          pComponentPrivate->pHandle->pApplicationPrivate, 
                                                          OMX_EventMark, 0, 0, pBufHeader->pMarkData);
            }
        }
        if (pComponentPrivate->bFlushInputPortCommandPending)
        {
            OMX_SendCommand(pComponentPrivate->pHandle,
                            OMX_CommandFlush,0,NULL);
        }
    } 
    else if (eDir == OMX_DirOutput) 
    {
        pComponentPrivate->nUnhandledFillThisBuffers--;
        OMX_PRBUFFER2(pComponentPrivate->dbg, "Sending output buffer to LCML \n");
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d Comp:: Sending Emptied Output buffer=%p to LCML\n", 
                       __LINE__,pBufHeader);
        /* Make sure that output buffer is issued to output stream only when
         * there is an outstanding input buffer already issued on input stream */
#ifdef __PERF_INSTRUMENTATION__
        PERF_SendingFrame(pComponentPrivate_CC->pPERFcomp,
                          PREF(pBufHeader,pBuffer),
                          0,
                          PERF_ModuleCommonLayer);
#endif
        if (!(pComponentPrivate->bIsStopping))
        {
            if (pComponentPrivate->bBypassDSP == 0)
            {
                OMX_PRDSP2(pComponentPrivate->dbg, "%d: Sending Empty OUTPUT BUFFER to Codec = %p\n",
                               __LINE__,pBufHeader->pBuffer);
                if (pComponentPrivate->curState == OMX_StateExecuting) 
                {
                    if(!pComponentPrivate->bDspStoppedWhileExecuting) 
                    {
                        if(!pComponentPrivate->reconfigOutputPort){
                            if (!WMADEC_IsPending(pComponentPrivate,pBufHeader,
                                                  OMX_DirOutput))
                            {
                                WMADEC_SetPending(pComponentPrivate,pBufHeader,
                                                  OMX_DirOutput);
                                pComponentPrivate->LastOutputBufferHdrQueued = 
                                    pBufHeader;
                                eError = LCML_QueueBuffer(pLcmlHandle->pCodecinterfacehandle,
                                                          EMMCodecOuputBuffer, 
                                                          (OMX_U8 *)pBufHeader->pBuffer, 
                                                          pBufHeader->nAllocLen,
                                                          pBufHeader->nAllocLen,
                                                          NULL, 0, NULL);

                                if (eError != OMX_ErrorNone )
                                {
                                    OMX_ERROR4(pComponentPrivate->dbg, "%d :: IssuingDSP OP: Error Occurred\n", 
                                                   __LINE__);
                                    eError = OMX_ErrorHardware;
                                    goto EXIT;
                                }
                            }
                        }
                        else{
                            OMX_PRSTATE2(pComponentPrivate->dbg, "Bypassing the sending of output buffers\n");
                            pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = pBufHeader;
                            OMX_PRBUFFER2(pComponentPrivate->dbg, "Don't queue while doing a reconfig:: output buffer\n");
                        }
                    }
                    else
                    {
#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                          pBufHeader->pBuffer,
                                          pBufHeader->nFilledLen,
                                          PERF_ModuleHLMM);
#endif
                        pComponentPrivate->cbInfo.FillBufferDone (
                                                                  pComponentPrivate->pHandle,
                                                                  pComponentPrivate->pHandle->pApplicationPrivate,
                                                                  pBufHeader);
                        pComponentPrivate->nOutStandingFillDones--;

                    }
                }
                else
                {
                    pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = 
                        pBufHeader;
                }
            }
        }
        else
        {
            if (pComponentPrivate->curState == OMX_StateExecuting)
            {
                if (!WMADEC_IsPending(pComponentPrivate,pBufHeader,OMX_DirOutput))
                {
                    WMADEC_SetPending(pComponentPrivate,pBufHeader,OMX_DirOutput);
                    pComponentPrivate->LastOutputBufferHdrQueued = pBufHeader;
                    eError = LCML_QueueBuffer(pLcmlHandle->pCodecinterfacehandle,
                                              EMMCodecOuputBuffer, 
                                              (OMX_U8 *)pBufHeader->pBuffer, 
                                              pBufHeader->nAllocLen,
                                              pBufHeader->nAllocLen,
                                              NULL, 0, NULL);
                                              
                    if (eError != OMX_ErrorNone )
                    {
                        OMX_ERROR4(pComponentPrivate->dbg, "%d :: IssuingDSP OP: Error Occurred\n", 
                                       __LINE__);
                        eError = OMX_ErrorHardware;
                        goto EXIT;
                    }
                }
            }
            else
            {
                pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = pBufHeader;
            }
        }
        if (pComponentPrivate->bFlushOutputPortCommandPending) {
            OMX_SendCommand( pComponentPrivate->pHandle, OMX_CommandFlush, 1, NULL);
        }
    } 
    else
    {
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d : BufferHeader %p, Buffer %p Unknown ..........\n",
                      __LINE__,pBufHeader, pBufHeader->pBuffer);
        eError = OMX_ErrorBadParameter;
    }
 EXIT:
    OMX_PRINT1(pComponentPrivate->dbg, "%d : Exiting from  WMADECHandleDataBuf_FromApp \n",__LINE__);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
 * WMADECGetBufferDirection () This function is used by the component thread to
 * request a buffer from the application.  Since it was called from 2 places,
 * it made sense to turn this into a small function.
 *
 * @param pData pointer to WMA Decoder Context Structure
 * @param pCur pointer to the buffer to be requested to be filled
 *
 * @retval none
 **/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE WMADECGetBufferDirection(OMX_BUFFERHEADERTYPE *pBufHeader, OMX_DIRTYPE *eDir)
{
    OMX_ERRORTYPE eError                                            = OMX_ErrorNone;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate = pBufHeader->pPlatformPrivate;
    OMX_U16 nBuf;
    OMX_BUFFERHEADERTYPE *pBuf                              = NULL;
    int flag = 1, i;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering WMADECGetBufferDirection Function\n",__LINE__);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: pComponentPrivate = %p\n",__LINE__,pComponentPrivate);
    /*Search this buffer in input buffers list */
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d ::  WMADECGetBufferDirection Function\n",__LINE__);
    OMX_PRINT1(pComponentPrivate->dbg, "%d ::  pComponentPrivate = %p\n",__LINE__,pComponentPrivate);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "%d ::  pComponentPrivate->pInputBufferList = %p\n",
                   __LINE__,pComponentPrivate->pInputBufferList);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d ::  pComponentPrivate->pInputBufferList->numBuffers = %d\n",
                   __LINE__,pComponentPrivate->pInputBufferList->numBuffers);

    nBuf = pComponentPrivate->pInputBufferList->numBuffers;
    for(i=0; i<nBuf; i++)
    {
        pBuf = pComponentPrivate->pInputBufferList->pBufHdr[i];
        if(pBufHeader == pBuf)
        {
            *eDir = OMX_DirInput;
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Buffer %p is INPUT BUFFER\n",__LINE__, pBufHeader);
            flag = 0;
            goto EXIT;
        }
    }

    /*Search this buffer in input buffers list */
    nBuf = pComponentPrivate->pOutputBufferList->numBuffers;

    for(i=0; i<nBuf; i++)
    {
        pBuf = pComponentPrivate->pOutputBufferList->pBufHdr[i];
        if(pBufHeader == pBuf)
        {
            *eDir = OMX_DirOutput;
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Buffer %p is OUTPUT BUFFER\n",__LINE__, 
                           pBufHeader);
            flag = 0;
            goto EXIT;
        }
    }

    if (flag == 1)
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Buffer %p is Not Found in the List\n",__LINE__,
                       pBufHeader);
        eError = OMX_ErrorUndefined;
        goto EXIT;
    }
 EXIT:
    OMX_ERROR4(pComponentPrivate->dbg, "%d :: Exiting WMADECGetBufferDirection Function\n",__LINE__);
    return eError;
}

/*-------------------------------------------------------------------*/
/**
 * WMADECHandleDataBuf_FromLCML () This function is used by the component thread to
 * request a buffer from the application.  Since it was called from 2 places,
 * it made sense to turn this into a small function.
 *
 * @param pData pointer to WMA Decoder Context Structure
 * @param pCur pointer to the buffer to be requested to be filled
 *
 * @retval none
 **/
/*-------------------------------------------------------------------*/
OMX_ERRORTYPE WMADECHandleDataBuf_FromLCML(WMADEC_COMPONENT_PRIVATE* pComponentPrivate, LCML_WMADEC_BUFHEADERTYPE* msgBuffer)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int ret = 0;

    OMX_COMPONENTTYPE* pHandle = (OMX_COMPONENTTYPE*)pComponentPrivate->pHandle;
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering WMADECHandleDataBuf_FromLCML Function\n",__LINE__);
    OMX_PRINT2(pComponentPrivate->dbg, "%d :: pComponentPrivate = %p\n",__LINE__, pComponentPrivate);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering WMADECHandleDataBuf_FromLCML Function\n",  __LINE__);
                   
    if (ret == -1)
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error While reading from the LCML Pipe\n",__LINE__);
        eError = OMX_ErrorHardware;
        goto EXIT;
    }

    OMX_PRINT2(pComponentPrivate->dbg, "%d :: Entering WMADECHandleDataBuf_FromLCML Function\n",__LINE__);
                   
    if (!(pComponentPrivate->bIsStopping))
    {
        if (msgBuffer->eDir == OMX_DirInput)
        {
            OMX_PRCOMM2(pComponentPrivate->dbg, "%d: Component Sending Empty Input buffer%p to App\n",__LINE__,msgBuffer->buffer);
            OMX_PRCOMM2(pComponentPrivate->dbg, "Sending Empty INPUT BUFFER to App = %p\n", msgBuffer->buffer->pBuffer);
                           
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                              PREF(msgBuffer->buffer,pBuffer),
                              0,  PERF_ModuleHLMM);
#endif
            msgBuffer->buffer->nFilledLen = 0;
            pComponentPrivate->cbInfo.EmptyBufferDone (pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       msgBuffer->buffer);
                                       
            pComponentPrivate->nEmptyBufferDoneCount++;
        }
        else if (msgBuffer->eDir == OMX_DirOutput)
        {
            OMX_PRCOMM2(pComponentPrivate->dbg, "%d: Component Sending Filled Output buffer%p to App size = %ld\n",
                          __LINE__,msgBuffer->buffer,msgBuffer->buffer->nFilledLen);
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                              msgBuffer->buffer->pBuffer,
                              msgBuffer->buffer->nFilledLen,
                              PERF_ModuleHLMM);
#endif           

            /*Copying tick count information to output buffer*/
            msgBuffer->buffer->nTickCount = (OMX_U32)pComponentPrivate->arrBufIndexTick[pComponentPrivate->OpBufindex];

            /* Copying time stamp information to output buffer */
            msgBuffer->buffer->nTimeStamp = (OMX_TICKS)pComponentPrivate->arrBufIndex[pComponentPrivate->OpBufindex];
            pComponentPrivate->OpBufindex++;
            pComponentPrivate->OpBufindex %= pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferCountActual;              
            
            pComponentPrivate->cbInfo.FillBufferDone (pHandle,
                                                      pHandle->pApplicationPrivate,
                                                      msgBuffer->buffer);
                                                       
            pComponentPrivate->nOutStandingFillDones--;
            OMX_PRSTATE2(pComponentPrivate->dbg, "nOutstandingFillDones = %ld\n\n", pComponentPrivate->nOutStandingFillDones);
        }
        else
        {
            OMX_PRINT2(pComponentPrivate->dbg, "%d :: Wrong Direction dir = %d\n", __LINE__,msgBuffer->eDir);
            OMX_PRDSP2(pComponentPrivate->dbg, "%d: Component Got buffer %p From LCML\n", __LINE__,msgBuffer->buffer);
            eError = OMX_ErrorBadParameter;
            goto EXIT;
        }
    }
    else
    {
        OMX_PRINT2(pComponentPrivate->dbg, "%d :: Entering WMADECHandleDataBuf_FromLCML Function\n",
                       __LINE__);
        if (msgBuffer->eDir == OMX_DirInput)
        {
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d: Component Sending Empty Input buffer%p to App\n",
                          __LINE__,msgBuffer->buffer);
            OMX_PRCOMM2(pComponentPrivate->dbg, "Sending Empty INPUT BUFFER to App = %p\n",
                           msgBuffer->buffer->pBuffer);
                           
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                              PREF(msgBuffer->buffer,pBuffer),
                              0, PERF_ModuleHLMM);
#endif
            msgBuffer->buffer->nFilledLen = 0;
            pComponentPrivate->cbInfo.EmptyBufferDone (pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       msgBuffer->buffer);
            pComponentPrivate->nEmptyBufferDoneCount++;
        }
        else if (msgBuffer->eDir == OMX_DirOutput)
        {
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d: Component Sending Filled Output buffer%p to App\n",
                          __LINE__,msgBuffer->buffer);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "Sending Filled OUTPUT BUFFER to App = %p\n",
                           msgBuffer->buffer->pBuffer);

#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingBuffer(pComponentPrivate->pPERFcomp,
                               msgBuffer->buffer->pBuffer,
                               msgBuffer->buffer->nFilledLen,
                               PERF_ModuleHLMM);
#endif  
            pComponentPrivate->cbInfo.FillBufferDone (pHandle,
                                                      pHandle->pApplicationPrivate,
                                                      msgBuffer->buffer);
            pComponentPrivate->nOutStandingFillDones--;
            OMX_PRCOMM2(pComponentPrivate->dbg, "nOutstandingFillDones = %ld\n\n", pComponentPrivate->nOutStandingFillDones);
        }
    }
 EXIT:
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting WMADECHandleDataBuf_FromLCML Function\n",
                   __LINE__);
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

OMX_ERRORTYPE WMADECLCML_Callback (TUsnCodecEvent event,void * args [10])
{   
    OMX_ERRORTYPE eError                                                   = OMX_ErrorNone;
    OMX_U8 *pBuffer                                                        = args[1];
    LCML_WMADEC_BUFHEADERTYPE *pLcmlHdr;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate_CC = NULL;      
    OMX_COMPONENTTYPE *pHandle                                     = NULL;

#ifdef WMADEC_DEBUG
    LCML_DSP_INTERFACE *phandle;
#endif
    LCML_DSP_INTERFACE *pLcmlHandle;    
    OMX_U16 i=0;
#ifdef UNDER_CE
    int numCalls;
#endif

    pComponentPrivate_CC = 
        (WMADEC_COMPONENT_PRIVATE*)((LCML_DSP_INTERFACE*)args[6])->pComponentPrivate;
    pHandle = pComponentPrivate_CC->pHandle;        

#ifdef WMADEC_DEBUG
    phandle = (LCML_DSP_INTERFACE *)args[6];
#endif
    pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate_CC->pLcmlHandle;
    OMX_PRINT1(pComponentPrivate_CC->dbg, "======================RECEIVED SOME CALLBACK===============\n");
    OMX_PRINT1(pComponentPrivate_CC->dbg, "[CURRENT STATE-------------------------------------->] %d\n",
                  pComponentPrivate_CC->curState);
    switch(event)
    {
    case EMMCodecDspError:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecDspError\n");
        break;

    case EMMCodecInternalError:
        OMX_ERROR4(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecInternalError\n");
        break;

    case EMMCodecInitError:
        OMX_ERROR4(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecInitError\n");
        break;

    case EMMCodecDspMessageRecieved:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecDspMessageRecieved\n");
        break;

    case EMMCodecBufferProcessed:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecBufferProcessed\n");
        break;
            
    case EMMCodecProcessingStarted:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecProcessingStarted\n");
        break;
            
    case EMMCodecProcessingPaused:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecProcessingPaused\n");
        break;

    case EMMCodecProcessingStoped:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecProcessingStoped\n");
        break;
    
    case EMMCodecProcessingEof:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecProcessingEof\n");
        break;

    case EMMCodecBufferNotProcessed:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecBufferNotProcessed\n");
        break;

    case EMMCodecAlgCtrlAck:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecAlgCtrlAck\n");
        break;

    case EMMCodecStrmCtrlAck:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecStrmCtrlAck\n");
        break;

    default:
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  Got event = %d\n",event);
        break;
    }
    OMX_PRINT1(pComponentPrivate_CC->dbg, "%d :: Entering the WMADECLCML_Callback Function\n",__LINE__);
    OMX_PRINT2(pComponentPrivate_CC->dbg, "args = %p ",args[0]);
    OMX_PRINT2(pComponentPrivate_CC->dbg, "event = %d ",event);
    
    if(event == EMMCodecBufferProcessed) {
        if( (OMX_U32)args [0] == EMMCodecInputBuffer) {
            OMX_PRBUFFER2(pComponentPrivate_CC->dbg, "%d :: Input: pBuffer = %p\n",__LINE__, pBuffer);
            eError = WMADECGetCorresponding_LCMLHeader(pComponentPrivate_CC,
                                                       pBuffer, 
                                                       OMX_DirInput, 
                                                       &pLcmlHdr);
#ifdef __PERF_INSTRUMENTATION__
            PERF_ReceivedFrame(pComponentPrivate_CC->pPERFcomp,
                               PREF(pLcmlHdr->buffer,pBuffer),
                               0, PERF_ModuleCommonLayer);
#endif            
            WMADEC_ClearPending(pComponentPrivate_CC,pLcmlHdr->buffer, OMX_DirInput);
            
            if (eError != OMX_ErrorNone)
            {
                OMX_ERROR4(pComponentPrivate_CC->dbg, "%d :: Error: Invalid Buffer Came ...\n",__LINE__);
                goto EXIT;
            }
            
#ifdef UNDER_CE
            ((OMXBufferStatus*)pLcmlHdr->buffer->pAppPrivate)->EmptyBufferDone = 1;
#endif
            WMADECHandleDataBuf_FromLCML(pComponentPrivate_CC, pLcmlHdr);
        } 
        else if ((OMX_U32)args [0] == EMMCodecOuputBuffer)
        {
            OMX_PRBUFFER2(pComponentPrivate_CC->dbg, "=======================OUTPUT BUFFER PROCESSED========================\n");
            OMX_PRBUFFER2(pComponentPrivate_CC->dbg, "%d :: Output: pBuffer = %p\n",__LINE__, pBuffer);
            if (!WMADEC_IsValid(pComponentPrivate_CC,pBuffer,OMX_DirOutput))
            {
                /* If the buffer we get back from the DSP is not valid call FillBufferDone
                   on a valid buffer */
#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingFrame(pComponentPrivate_CC->pPERFcomp,
                                  pComponentPrivate_CC->pOutputBufferList->pBufHdr[pComponentPrivate_CC->nInvalidFrameCount++]->pBuffer,
                                  pComponentPrivate_CC->pOutputBufferList->pBufHdr[pComponentPrivate_CC->nInvalidFrameCount++]->nFilledLen,
                                  PERF_ModuleHLMM);
#endif
                pComponentPrivate_CC->cbInfo.FillBufferDone (pComponentPrivate_CC->pHandle,
                                                             pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                             pComponentPrivate_CC->pOutputBufferList->pBufHdr[pComponentPrivate_CC->nInvalidFrameCount++]
                                                             );
                pComponentPrivate_CC->nOutStandingFillDones--;
            }
            else
            {
                eError = WMADECGetCorresponding_LCMLHeader(pComponentPrivate_CC,
                                                           pBuffer, 
                                                           OMX_DirOutput, 
                                                           &pLcmlHdr);
                WMADEC_ClearPending(pComponentPrivate_CC,pLcmlHdr->buffer, OMX_DirOutput);
                if (eError != OMX_ErrorNone)
                {
                    OMX_ERROR4(pComponentPrivate_CC->dbg, "%d :: Error: Invalid Buffer Came ...\n",
                                  __LINE__);
                    goto EXIT;
                }
                pLcmlHdr->buffer->nFilledLen = (OMX_U32)args[8];
            
                OMX_PRBUFFER2(pComponentPrivate_CC->dbg, "pLcmlHdr->buffer->nFilledLen = %ld\n", pLcmlHdr->buffer->nFilledLen);
                
#ifdef __PERF_INSTRUMENTATION__
                PERF_ReceivedFrame(pComponentPrivate_CC->pPERFcomp,
                                   PREF(pLcmlHdr->buffer,pBuffer),
                                   PREF(pLcmlHdr->buffer,nFilledLen),
                                   PERF_ModuleCommonLayer);

                pComponentPrivate_CC->lcml_nCntOpReceived++;
                OMX_PRDSP2(pComponentPrivate_CC->dbg, "pComponentPrivate_CC->lcml_nCntOpReceived = %ld\n\n", pComponentPrivate_CC->lcml_nCntOpReceived);

                if ((pComponentPrivate_CC->nLcml_nCntIp >= 1) && 
                    (pComponentPrivate_CC->nLcml_nCntOpReceived == 1))
                {
                    PERF_Boundary(pComponentPrivate_CC->pPERFcomp,
                                  PERF_BoundaryStart | PERF_BoundarySteadyState);
                }
#endif

#ifdef UNDER_CE
                ((OMXBufferStatus*)pLcmlHdr->buffer->pAppPrivate)->FillBufferDone = 1;
#endif
                OMX_PRBUFFER2(pComponentPrivate_CC->dbg, " OUTPUT RETURNING pBuffer->nFilledLen =%ld\n",pLcmlHdr->buffer->nFilledLen);                                              
                WMADECHandleDataBuf_FromLCML(pComponentPrivate_CC, pLcmlHdr);
            }
        }
    } 
    else if (event == EMMCodecStrmCtrlAck)
    {           
        LCML_DPRINT("%d :: GOT MESSAGE USN_DSPACK_STRMCTRL ----\n",__LINE__);

        if (args[1] == (void *)USN_STRMCMD_FLUSH)
        {
            pHandle = pComponentPrivate_CC->pHandle;    
            if ( args[2] == (void *)EMMCodecInputBuffer)
            {
                if (args[0] == (void *)USN_ERR_NONE )
                {

                    for (i=0; i < pComponentPrivate_CC->nNumInputBufPending; i++) {
#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate_CC->pPERFcomp,
                                          PREF(pComponentPrivate_CC->pInputBufHdrPending[i],pBuffer),
                                          0,
                                          PERF_ModuleHLMM);
#endif
                        pComponentPrivate_CC->pInputBufHdrPending[i]->nFilledLen = 0;
                        pComponentPrivate_CC->cbInfo.EmptyBufferDone (pComponentPrivate_CC->pHandle,
                                                                      pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                                      pComponentPrivate_CC->pInputBufHdrPending[i]);
                        pComponentPrivate_CC->nEmptyBufferDoneCount++;
                        pComponentPrivate_CC->pInputBufHdrPending[i] = NULL;
                    }
                    pComponentPrivate_CC->nNumInputBufPending=0; 
   
                    pthread_mutex_lock(&pComponentPrivate_CC->codecFlush_mutex);
                    if(pComponentPrivate_CC->codecFlush_waitingsignal == 0){
                        pComponentPrivate_CC->codecFlush_waitingsignal = 1; 
                        pthread_cond_signal(&pComponentPrivate_CC->codecFlush_threshold);
                        OMX_PRCOMM2(pComponentPrivate_CC->dbg, "flush ack. received. for input port\n");
                    }     
                    pthread_mutex_unlock(&pComponentPrivate_CC->codecFlush_mutex);
                    // mutex above signals that the dsp side has finished flushing and processing can continue
                    pComponentPrivate_CC->cbInfo.EventHandler(pHandle, 
                                                              pHandle->pApplicationPrivate,
                                                              OMX_EventCmdComplete, 
                                                              OMX_CommandFlush,
                                                              INPUT_PORT, 
                                                              NULL); 
                }
                else
                {
                    goto EXIT;                            
                }
            }
            else if ( args[2] == (void *)EMMCodecOuputBuffer)
            { 
                if (args[0] == (void *)USN_ERR_NONE )
                {
                    for (i=0; i < pComponentPrivate_CC->nNumOutputBufPending; i++) {
#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingFrame(pComponentPrivate_CC->pPERFcomp,
                                          PREF(pComponentPrivate_CC->pOutputBufHdrPending[i],pBuffer),
                                          PREF(pComponentPrivate_CC->pOutputBufHdrPending[i],nFilledLen),
                                          PERF_ModuleHLMM);
#endif  
                        pComponentPrivate_CC->cbInfo.FillBufferDone (pComponentPrivate_CC->pHandle,
                                                                     pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                                     pComponentPrivate_CC->pOutputBufHdrPending[i]
                                                                     );
                        pComponentPrivate_CC->nOutStandingFillDones--;
                        pComponentPrivate_CC->pOutputBufHdrPending[i] = NULL;
                    }
                    pComponentPrivate_CC->nNumOutputBufPending=0;

                    pthread_mutex_lock(&pComponentPrivate_CC->codecFlush_mutex);
                    if(pComponentPrivate_CC->codecFlush_waitingsignal == 0){
                        pComponentPrivate_CC->codecFlush_waitingsignal = 1; 
                        pthread_cond_signal(&pComponentPrivate_CC->codecFlush_threshold);
                        OMX_PRCOMM2(pComponentPrivate_CC->dbg, "flush ack. received. for output port\n");
                    }     
                    pthread_mutex_unlock(&pComponentPrivate_CC->codecFlush_mutex);
                    pComponentPrivate_CC->cbInfo.EventHandler(pComponentPrivate_CC->pHandle, 
                                                              pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                              OMX_EventCmdComplete, 
                                                              OMX_CommandFlush,
                                                              OUTPUT_PORT,
                                                              NULL);
                }
                else
                {               
                    goto EXIT;                            
                }
            }
            
        }

    } 
    else if(event == EMMCodecProcessingStoped)
    {
        pthread_mutex_lock(&pComponentPrivate_CC->codecStop_mutex);
        if(pComponentPrivate_CC->codecStop_waitingsignal == 0){
            pComponentPrivate_CC->codecStop_waitingsignal = 1;             
            pthread_cond_signal(&pComponentPrivate_CC->codecStop_threshold);
            OMX_PRCOMM2(pComponentPrivate_CC->dbg, "stop ack. received. stop waiting for sending disable command completed\n");
        }
        pthread_mutex_unlock(&pComponentPrivate_CC->codecStop_mutex);

        OMX_U32 aParam[3] = {0};
        aParam[0] = USN_STRMCMD_FLUSH; 
        aParam[1] = 0x0; 
        aParam[2] = 0x0; 
#if 0
        //TOOK THIS STUFF OUT... THE CODEC IS STOPPED, NO NEED TO FLUSH!!!
        OMX_PRCOMM2(pComponentPrivate_CC->dbg, "Flushing input port\n");
        if (pComponentPrivate_CC->codecFlush_waitingsignal == 0){
            pthread_mutex_lock(&pComponentPrivate_CC->codecFlush_mutex);
        }
        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                   EMMCodecControlStrmCtrl, (void*)aParam);
        if (pComponentPrivate_CC->codecFlush_waitingsignal == 0){
            pthread_cond_wait(&pComponentPrivate_CC->codecFlush_threshold, &pComponentPrivate_CC->codecFlush_mutex);
            pComponentPrivate_CC->codecFlush_waitingsignal = 0;
            pthread_mutex_unlock(&pComponentPrivate_CC->codecFlush_mutex);
        }
        if (eError != OMX_ErrorNone)
        {
            goto EXIT;
        }

        aParam[0] = USN_STRMCMD_FLUSH; 
        aParam[1] = 0x1; 
        aParam[2] = 0x0; 

        OMX_PRCOMM2(pComponentPrivate_CC->dbg, "Flushing output port\n");
        if (pComponentPrivate_CC->codecFlush_waitingsignal == 0){
            pthread_mutex_lock(&pComponentPrivate_CC->codecFlush_mutex);
        }
        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                   EMMCodecControlStrmCtrl, (void*)aParam);
        if (pComponentPrivate_CC->codecFlush_waitingsignal == 0){
            pthread_cond_wait(&pComponentPrivate_CC->codecFlush_threshold, &pComponentPrivate_CC->codecFlush_mutex);
            pComponentPrivate_CC->codecFlush_waitingsignal = 0;
            pthread_mutex_unlock(&pComponentPrivate_CC->codecFlush_mutex);
        }
        if (eError != OMX_ErrorNone)
        {
            goto EXIT;
        }
#endif
        if (!pComponentPrivate_CC->bNoIdleOnStop)
        {
            pComponentPrivate_CC->curState = OMX_StateIdle;
#ifdef RESOURCE_MANAGER_ENABLED        
            eError = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, 
                                            OMX_WMA_Decoder_COMPONENT, 
                                            OMX_StateIdle, 1234, NULL);
#endif        
            if (pComponentPrivate_CC->bPreempted == 0) {        
                pComponentPrivate_CC->cbInfo.EventHandler(pComponentPrivate_CC->pHandle,
                                                          pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                          OMX_EventCmdComplete,
                                                          OMX_CommandStateSet,
                                                          pComponentPrivate_CC->curState,
                                                          NULL);
            }
        }
        else
        {
            pComponentPrivate_CC->bDspStoppedWhileExecuting = OMX_TRUE;
            pComponentPrivate_CC->bNoIdleOnStop= OMX_FALSE;
        }
        
    }
    else if (event == EMMCodecProcessingPaused)
    {

        pComponentPrivate_CC->curState = OMX_StatePause;
    
        pComponentPrivate_CC->cbInfo.EventHandler(pHandle, pHandle->pApplicationPrivate,
                                                  OMX_EventCmdComplete, 
                                                  OMX_CommandStateSet, 
                                                  pComponentPrivate_CC->curState,
                                                  NULL);
    }
    else if (event == EMMCodecDspError)
    {

#ifdef _ERROR_PROPAGATION__
        for(i=0;i<9;i++)
        {
            OMX_PRINT2(pComponentPrivate_CC->dbg, "arg [%d] = %p \n",i,args[i]);
        }
        /* Cheking for MMU_fault */
        OMX_PRINT2(pComponentPrivate_CC->dbg, "Printing ERROR CODE args 4 =%d\n",(int)args[4]);
        OMX_PRINT2(pComponentPrivate_CC->dbg, "Printing ERROR CODE args 5 =%d\n",(int)args[5]);         
        
        if((args[4] == (void *)USN_ERR_UNKNOWN_MSG) && (args[5] == NULL))
        {               
            pComponentPrivate_CC->bIsInvalidState=OMX_TRUE;
            pComponentPrivate_CC->curState = OMX_StateInvalid;
            pHandle = pComponentPrivate_CC->pHandle;
            pComponentPrivate_CC->cbInfo.EventHandler(pHandle, 
                                                      pHandle->pApplicationPrivate,
                                                      OMX_EventError,
                                                      OMX_ErrorInvalidState, 
                                                      OMX_TI_ErrorSevere,
                                                      NULL);
        }
#endif
        if (((int)args[4] == USN_ERR_WARNING) && 
            ((int)args[5] == IUALG_WARN_PLAYCOMPLETED))
        {
            OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d :: GOT MESSAGE IUALG_WARN_PLAYCOMPLETED\n",
                          __LINE__);

            if(pComponentPrivate_CC->dasfmode == 0)
            {
                pComponentPrivate_CC->LastOutputBufferHdrQueued->nFlags |= 
                    OMX_BUFFERFLAG_EOS;

                pComponentPrivate_CC->cbInfo.EventHandler(
                                                          pComponentPrivate_CC->pHandle,
                                                          pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                          OMX_EventBufferFlag,
                                                          pComponentPrivate_CC->LastOutputBufferHdrQueued->nOutputPortIndex,
                                                          pComponentPrivate_CC->LastOutputBufferHdrQueued->nFlags, (OMX_PTR)OMX_BUFFERFLAG_EOS);

            } else
            {
#ifndef UNDER_CE
                pComponentPrivate_CC->cbInfo.EventHandler(
                                                          pComponentPrivate_CC->pHandle,
                                                          pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                          OMX_EventBufferFlag,
                                                          (OMX_U32) NULL,
                                                          OMX_BUFFERFLAG_EOS, (OMX_PTR)OMX_BUFFERFLAG_EOS);
#endif              
            }
        }
    }
    
    if(event == EMMCodecDspMessageRecieved) {
        OMX_PRSTATE2(pComponentPrivate_CC->dbg, "%d :: commandedState  = %p\n",__LINE__,args[0]);
        OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: arg1 = %p\n",__LINE__,args[1]);
        OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: arg2 = %p\n",__LINE__,args[2]);
    }

#ifdef _ERROR_PROPAGATION__

    else if (event ==EMMCodecInitError) {
        /* Cheking for MMU_fault */
        if((args[4] == (void *)USN_ERR_UNKNOWN_MSG) && (args[5] == NULL)) {

            pComponentPrivate_CC->bIsInvalidState=OMX_TRUE;
            pComponentPrivate_CC->curState = OMX_StateInvalid;
            pHandle = pComponentPrivate_CC->pHandle;
            pComponentPrivate_CC->cbInfo.EventHandler(pHandle, 
                                                      pHandle->pApplicationPrivate,
                                                      OMX_EventError,
                                                      OMX_ErrorInvalidState, 
                                                      OMX_TI_ErrorSevere,
                                                      NULL);
        }   
    }
    else if (event ==EMMCodecInternalError) {

        /* Cheking for MMU_fault */
        if((args[4] == (void *)USN_ERR_UNKNOWN_MSG) && (args[5] == NULL)) {

            pComponentPrivate_CC->bIsInvalidState=OMX_TRUE;
            pComponentPrivate_CC->curState = OMX_StateInvalid;
            pHandle = pComponentPrivate_CC->pHandle;
            pComponentPrivate_CC->cbInfo.EventHandler(pHandle, 
                                                      pHandle->pApplicationPrivate,
                                                      OMX_EventError,
                                                      OMX_ErrorInvalidState, 
                                                      OMX_TI_ErrorSevere,
                                                      NULL);
        }

    }
#endif

 EXIT:
    OMX_PRINT1(pComponentPrivate_CC->dbg, "%d :: Exiting the WMADECLCML_Callback Function\n",__LINE__);
    return eError;
}
/* -------------------------------------------------------------------*/
/**
 *  WMADEC_GetCorresponding_LCMLHeader() function will be called by WMADEC_LCML_Callback
 *                                 component to write the msg
 * @param *pBuffer,          Event which gives to details about USN status
 * @param LCML_NBAMRENC_BUFHEADERTYPE **ppLcmlHdr          

 * @retval OMX_NoError              Success, ready to roll
 *         OMX_Error_BadParameter   The input parameter pointer is null
 **/
/* -------------------------------------------------------------------*/

OMX_ERRORTYPE WMADECGetCorresponding_LCMLHeader(WMADEC_COMPONENT_PRIVATE *pComponentPrivate,
                                                OMX_U8 *pBuffer,
                                                OMX_DIRTYPE eDir,
                                                LCML_WMADEC_BUFHEADERTYPE **ppLcmlHdr)
{

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    LCML_WMADEC_BUFHEADERTYPE *pLcmlBufHeader;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate_CC;
    
    int nIpBuf;
    int nOpBuf;
    OMX_U16 i;
    
    pComponentPrivate_CC = pComponentPrivate;
    nIpBuf = pComponentPrivate_CC->pInputBufferList->numBuffers;
    nOpBuf = pComponentPrivate_CC->pOutputBufferList->numBuffers;  
    
    while (!pComponentPrivate_CC->bInitParamsInitialized)
    {
#ifndef UNDER_CE
        sched_yield();
#else
        Sleep(0);
#endif
    }
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Inside WMADECGetCorresponding_LCMLHeader..\n",__LINE__);
    OMX_PRINT2(pComponentPrivate->dbg, "%d :: eDir = %d\n",__LINE__,eDir);

    if(eDir == OMX_DirInput) {

        OMX_PRINT1(pComponentPrivate->dbg, "%d :: pComponentPrivate_CC = %p\n",__LINE__,
                      pComponentPrivate_CC);
        OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: pComponentPrivate_CC->pLcmlBufHeader[INPUT_PORT] = %p\n",
                      __LINE__,pComponentPrivate_CC->pLcmlBufHeader[INPUT_PORT]);

        pLcmlBufHeader = pComponentPrivate_CC->pLcmlBufHeader[INPUT_PORT];
        OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: nIpBuf = %d\n",__LINE__,nIpBuf);
        for(i=0; i<nIpBuf; i++)
        {
            OMX_PRBUFFER1(pComponentPrivate->dbg, "pBuffer = %p\n",pBuffer);
            OMX_PRBUFFER1(pComponentPrivate->dbg, "pLcmlBufHeader->buffer->pBuffer = %p\n",
                          pLcmlBufHeader->buffer->pBuffer);
            if(pBuffer == pLcmlBufHeader->buffer->pBuffer)
            {
                *ppLcmlHdr = pLcmlBufHeader;
                OMX_ERROR4(pComponentPrivate->dbg, "%d::Corresponding LCML Header Found\n",__LINE__);
                goto EXIT;
            }
            pLcmlBufHeader++;
        }
    }
    else if (eDir == OMX_DirOutput)
    {

        pLcmlBufHeader = pComponentPrivate_CC->pLcmlBufHeader[OUTPUT_PORT];
        OMX_PRBUFFER1(pComponentPrivate->dbg, "pBuffer = %p\n",pBuffer);
        OMX_PRBUFFER2(pComponentPrivate->dbg, "pLcmlBufHeader->buffer- = %p\n",pLcmlBufHeader->buffer);
        OMX_PRBUFFER2(pComponentPrivate->dbg, "pLcmlBufHeader->buffer->pBuffer = %p\n",
                      pLcmlBufHeader->buffer->pBuffer);
        OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: nOpBuf = %d\n",__LINE__,nOpBuf);

        for(i=0; i<nOpBuf; i++)
        {
            if(pBuffer == pLcmlBufHeader->buffer->pBuffer)
            {
                OMX_PRBUFFER1(pComponentPrivate->dbg, "pBuffer = %p\n",pBuffer);
                OMX_PRBUFFER2(pComponentPrivate->dbg, "pLcmlBufHeader->buffer->pBuffer = %p\n",
                              pLcmlBufHeader->buffer->pBuffer);
                *ppLcmlHdr = pLcmlBufHeader;
                OMX_ERROR4(pComponentPrivate->dbg, "%d::Corresponding LCML Header Found\n",__LINE__);
                goto EXIT;
            }
            pLcmlBufHeader++;
        }
    }
    else 
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d:: Invalid Buffer Type :: exiting...\n",__LINE__);
    }

 EXIT:
    return eError;
}


/* -------------------------------------------------------------------*/
/**
 *  WMADEC_GetLCMLHandle() 
 * 
 * @retval OMX_HANDLETYPE
 *
 -------------------------------------------------------------------*/

#ifndef UNDER_CE
OMX_HANDLETYPE WMADECGetLCMLHandle(WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{

    void *handle;
    OMX_ERRORTYPE (*fpGetHandle)(OMX_HANDLETYPE);
    OMX_HANDLETYPE pHandle = NULL;
    char *error;
    OMX_ERRORTYPE eError;

    handle = dlopen("libLCML.so", RTLD_LAZY);
    if (!handle)
    {
        fputs(dlerror(), stderr);
        goto EXIT;
    }

    fpGetHandle = dlsym (handle, "GetHandle");
    if ((error = dlerror()) != NULL)
    {
        fputs(error, stderr);
        goto EXIT;
    }
    
    eError = (*fpGetHandle)(&pHandle);
    if(eError != OMX_ErrorNone)
    {
        eError = OMX_ErrorUndefined;
        OMX_ERROR4(pComponentPrivate->dbg, "eError != OMX_ErrorNone...\n");
        pHandle = NULL;
        goto EXIT;
    }
    
    ((LCML_DSP_INTERFACE*)pHandle)->pComponentPrivate = pComponentPrivate;

 EXIT:

    return pHandle;
}
#else
/*=================================================================*/
/*WINDOWS Explicit dll load procedure
  ===================================================================*/
OMX_HANDLETYPE WMADECGetLCMLHandle(WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{

    typedef OMX_ERRORTYPE (*LPFNDLLFUNC1)(OMX_HANDLETYPE);
    OMX_HANDLETYPE pHandle = NULL;
    OMX_ERRORTYPE eError;
    HINSTANCE hDLL;               // Handle to DLL
    LPFNDLLFUNC1 fpGetHandle1;
    hDLL = LoadLibraryEx(TEXT("OAF_BML.dll"), NULL,0);
    if (hDLL == NULL)
    {
        OMX_ERROR4(pComponentPrivate->dbg, "BML Load Failed!!!\n");
        return pHandle;
    }

    fpGetHandle1 = (LPFNDLLFUNC1)GetProcAddress(hDLL,TEXT("GetHandle"));
    if (!fpGetHandle1)
    {
        // handle the error
        FreeLibrary(hDLL);
        return pHandle;
    }

    // call the function
    eError = fpGetHandle1(&pHandle);
    if(eError != OMX_ErrorNone) {
        eError = OMX_ErrorUndefined;
        OMX_ERROR4(pComponentPrivate->dbg, "eError != OMX_ErrorNone...\n");
        pHandle = NULL;
        return pHandle;
    }

    ((LCML_DSP_INTERFACE*)pHandle)->pComponentPrivate = pComponentPrivate;    
    
    return pHandle;
}


#endif


#ifndef UNDER_CE
/*=================================================================*/
/**@FreeLCMLHandle                                                                      */
/*==================================================================*/
OMX_ERRORTYPE WMADECFreeLCMLHandle(WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{

    int retValue;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (pComponentPrivate->bLcmlHandleOpened)
    {
        retValue = dlclose(pComponentPrivate->pLcmlHandle);

        if (retValue != 0)
        {
            eError = OMX_ErrorUndefined;
        }
        pComponentPrivate->bLcmlHandleOpened = 0;
    }

    return eError;
}
#else
/*=================================================================*/
/**    @   FreeLCMLHandle                                                                       */
/*==================================================================*/
OMX_ERRORTYPE WMADECFreeLCMLHandle(WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{

    int retValue;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (pComponentPrivate->bLcmlHandleOpened)
    {

        retValue = FreeLibrary(pComponentPrivate->pLcmlHandle);
        if (retValue == 0) /* Zero Indicates failure */
        {         
            eError = OMX_ErrorUndefined;
        }
        pComponentPrivate->bLcmlHandleOpened = 0;
    }
    return eError;
}
#endif
/* ================================================================================= */
/**
 * @fn WMADEC_CommandToIdle() description for WMADEC_CommandToIdle  
 WMADEC_CommandToIdle().  
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
OMX_ERRORTYPE WMADEC_CommandToIdle(WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;
    
#ifdef RESOURCE_MANAGER_ENABLED 
    OMX_ERRORTYPE rm_error = OMX_ErrorNone;
#endif
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMX_U16 arr[50];
    int inputPortFlag = 0;
    int outputPortFlag = 0;

    char *p = "hello";
    LCML_CALLBACKTYPE cb;
    LCML_DSP *pLcmlDsp;


    OMX_PRINT1(pComponentPrivate->dbg, "%d: WMADECHandleCommand: Cmd Idle \n",__LINE__);
    OMX_PRINT2(pComponentPrivate->dbg, "curstate = %d\n",pComponentPrivate->curState);
    if (pComponentPrivate->curState == OMX_StateIdle)
    {
        pComponentPrivate->cbInfo.EventHandler (pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorSameState,
                                                OMX_TI_ErrorMinor, NULL);
        OMX_ERROR2(pComponentPrivate->dbg, "%d :: Error: Same State Given by Application\n",__LINE__);
    }
    else if (pComponentPrivate->curState == OMX_StateLoaded)
    {
#ifdef __PERF_INSTRUMENTATION__
        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundarySetup);
#endif
        if (pComponentPrivate->dasfmode == 1)
        {
            pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled= FALSE;
            pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated= FALSE;
            
            if(pComponentPrivate->streamID == 0)
            {
                OMX_ERROR4(pComponentPrivate->dbg, "**************************************\n");
                OMX_ERROR4(pComponentPrivate->dbg, ":: Error = OMX_ErrorInsufficientResources\n");
                OMX_ERROR4(pComponentPrivate->dbg, "**************************************\n");
                eError = OMX_ErrorInsufficientResources;
                pComponentPrivate->curState = OMX_StateInvalid;
                
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventError, 
                                                       OMX_ErrorInvalidState,
                                                       OMX_TI_ErrorSevere, 
                                                       NULL);
                goto EXIT;
            }
        }         
        while (1)
        {
            OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: In while loop OMX_StateLoaded [INPUT_PORT]->bPopulated  %d \n",
                          __LINE__,pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated);
            OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: In while loop OMX_StateLoaded [INPUT_PORT]->bEnabled    %d \n",
                          __LINE__,pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled);
            OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: In while loop OMX_StateLoaded [OUTPUT_PORT]->bPopulated %d \n",
                          __LINE__,pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated);
            OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: In while loop OMX_StateLoaded [OUTPUT_PORT]->bEnabled   %d \n",
                          __LINE__,pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled);

            if (pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated &&  
                pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled)
            {
                inputPortFlag = 1;
            }

            if (!pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated && 
                !pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled)
            {
                inputPortFlag = 1;
            }

            if (pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated && 
                pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled)
            {
                outputPortFlag = 1;
            }

            if (!pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated && 
                !pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled)
            {
                outputPortFlag = 1;
            }

            if (inputPortFlag && outputPortFlag) 
            {
                break;
            }
            /* From Loaded to Idle. All enable ports are populated. */
            pComponentPrivate->InLoaded_readytoidle = 1;            
#ifndef UNDER_CE
            pthread_mutex_lock(&pComponentPrivate->InLoaded_mutex); 
            pthread_cond_wait(&pComponentPrivate->InLoaded_threshold, 
                              &pComponentPrivate->InLoaded_mutex);
            /*---------------------*/
            pthread_mutex_unlock(&pComponentPrivate->InLoaded_mutex);
#else
            OMX_WaitForEvent(&(pComponentPrivate->InLoaded_event));
#endif
            break; 
        }
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Inside WMADECHandleCommand\n",__LINE__);
        cb.LCML_Callback = (void *) WMADECLCML_Callback;
        pLcmlHandle = (OMX_HANDLETYPE) WMADECGetLCMLHandle(pComponentPrivate);
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Inside WMADECHandleCommand\n",__LINE__);
        if (pLcmlHandle == NULL)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: LCML Handle is NULL........exiting..\n",__LINE__);
            goto EXIT;
        }

        /* Got handle of dsp via phandle filling information about DSP   specific things */
        pLcmlDsp = (((LCML_DSP_INTERFACE*)pLcmlHandle)->dspCodec);
        eError = WMADECFill_LCMLInitParams(pHandle, pLcmlDsp, arr);
        if(eError != OMX_ErrorNone)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error returned from WMADECFill_LCMLInitParams()\n",
                          __LINE__);
            goto EXIT;
        }
        
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        pComponentPrivate->pLcmlHandle = (LCML_DSP_INTERFACE *)pLcmlHandle;
        
        /*filling create phase params */
        cb.LCML_Callback = (void *) WMADECLCML_Callback;
        OMX_PRDSP2(pComponentPrivate->dbg, "%d :: Calling LCML_InitMMCodec...\n",__LINE__);
        
#ifndef UNDER_CE
        /* TeeDN will be default for decoder component */
        OMX_PRSTATE2(pComponentPrivate->dbg, "WMA decoder support TeeDN\n");
        OMX_PRDSP2(pComponentPrivate->dbg, "pComponentPrivate Before calling InitMMCodec = %p\n",
                      pComponentPrivate);
                      
        OMX_PRINT1(pComponentPrivate->dbg, "&pComponentPrivate = %p\n",&pComponentPrivate);
        OMX_PRDSP2(pComponentPrivate->dbg, "pLcmlHandle = %p\n",pLcmlHandle);
        OMX_PRINT2(pComponentPrivate->dbg, "p = %p\n",p);
        OMX_PRDSP1(pComponentPrivate->dbg, "pLcmlHandle->pCodecinterfacehandle = %p\n",
                      ((LCML_DSP_INTERFACE *)pLcmlHandle)->pCodecinterfacehandle);
                        
        eError = LCML_InitMMCodecEx(((LCML_DSP_INTERFACE *)pLcmlHandle)->pCodecinterfacehandle,
                                    p,&pLcmlHandle,(void *)p,&cb,(OMX_STRING)pComponentPrivate->sDeviceString);
        OMX_PRDSP2(pComponentPrivate->dbg, "pComponentPrivate After calling InitMMCodec = %p\n",
                      pComponentPrivate);

        if(eError != OMX_ErrorNone)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error returned from LCML_Init() error = %d\n",__LINE__, eError);
            goto EXIT;
        }
#else
        eError = LCML_InitMMCodec(((LCML_DSP_INTERFACE *)pLcmlHandle)->pCodecinterfacehandle,
                                  p,&pLcmlHandle,(void *)p,&cb);
        if (eError != OMX_ErrorNone)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error : InitMMCodec failed...>>>>>> \n",__LINE__);
            goto EXIT;
        }

#endif

#ifdef HASHINGENABLE
        /* Enable the Hashing Code */
        eError = LCML_SetHashingState(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle, OMX_TRUE);
        if (eError != OMX_ErrorNone) {
            OMX_ERROR4(pComponentPrivate->dbg, "Failed to set Mapping State\n");
            goto EXIT;
        }
#endif

#ifdef RESOURCE_MANAGER_ENABLED 
        OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: WMADEC: About to call RMProxy_SendCommand\n", __LINE__);
        pComponentPrivate->rmproxyCallback.RMPROXY_Callback = 
            (void *) WMAD_ResourceManagerCallback;
                                        
        if (pComponentPrivate->curState != OMX_StateWaitForResources){                                                          
            rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_RequestResource, 
                                              OMX_WMA_Decoder_COMPONENT, 
                                              WMA_CPU, 1234, NULL);
                                                    
            OMX_PRDSP2(pComponentPrivate->dbg, "%d :: WMADEC: RMProxy_SendCommand returned %d\n", 
                          __LINE__,rm_error);
            if(rm_error == OMX_ErrorNone)
            {
                /* resource is available */
                pComponentPrivate->curState = OMX_StateIdle;
                pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete, 
                                                        OMX_CommandStateSet,
                                                        pComponentPrivate->curState, 
                                                        NULL);
#ifdef __PERF_INSTRUMENTATION__
                PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | 
                              PERF_BoundarySetup);
#endif  
                rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, 
                                                  OMX_WMA_Decoder_COMPONENT, 
                                                  OMX_StateIdle,0,NULL);
            }
        }
        else if(rm_error == OMX_ErrorInsufficientResources){
            /* resource is not available, need set state to OMX_StateWaitForResources */
            pComponentPrivate->curState = OMX_StateWaitForResources;
            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                   pHandle->pApplicationPrivate,
                                                   OMX_EventCmdComplete,
                                                   OMX_CommandStateSet,
                                                   pComponentPrivate->curState,
                                                   NULL);
            OMX_ERROR2(pComponentPrivate->dbg, "%d :: WMADEC: Error - insufficient resources\n", __LINE__);
        }
#else
        OMX_PRSTATE2(pComponentPrivate->dbg, "Setting to OMX_StateIdle - Line %d\n",__LINE__);
        pComponentPrivate->curState = OMX_StateIdle;
        pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                pHandle->pApplicationPrivate,
                                                OMX_EventCmdComplete, 
                                                OMX_CommandStateSet,
                                                pComponentPrivate->curState, 
                                                NULL);
#endif

        OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: WMADEC: State has been Set to Idle\n",__LINE__);

#ifdef __PERF_INSTRUMENTATION__
        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | 
                      PERF_BoundarySetup);
#endif  

    } 
    else if (pComponentPrivate->curState == OMX_StateExecuting)
    {
        char *pArgs = "damedesuStr";
#ifdef HASHINGENABLE
        /*Hashing Change*/
        pLcmlHandle = (LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle;
        eError = LCML_FlushHashes(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle);
        if (eError != OMX_ErrorNone) {
            OMX_ERROR2(pComponentPrivate->dbg, "Error occurred in Codec mapping flush!\n");
        }
#endif          
        /*Set the bIsStopping bit */
#ifdef __PERF_INSTRUMENTATION__
        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | 
                      PERF_BoundarySteadyState);
#endif  

        OMX_PRINT2(pComponentPrivate->dbg, "%d :: WMADEC: About to set bIsStopping bit\n", __LINE__);
        if (pComponentPrivate->codecStop_waitingsignal == 0){
            pthread_mutex_lock(&pComponentPrivate->codecStop_mutex); 
        } 
        eError = LCML_ControlCodec(
                                   ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                   MMCodecControlStop,(void *)pArgs);
        if (pComponentPrivate->codecStop_waitingsignal == 0){
            pthread_cond_wait(&pComponentPrivate->codecStop_threshold, &pComponentPrivate->codecStop_mutex);
            pComponentPrivate->codecStop_waitingsignal = 0; // reset the wait condition for next time
            pthread_mutex_unlock(&pComponentPrivate->codecStop_mutex);
        }
        if(eError != OMX_ErrorNone)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d: Error Occurred in Codec Stop..\n", __LINE__);
            goto EXIT;
        }
        else{
            OMX_PRSTATE2(pComponentPrivate->dbg, "%d: Codec has been stopped\n", __LINE__);
        }
    } 
    else if(pComponentPrivate->curState == OMX_StatePause)
    {
        char *pArgs = "damedesuStr";
        OMX_PRDSP2(pComponentPrivate->dbg, "%d :: Comp: Stop Command Received\n",__LINE__);
#ifdef HASHINGENABLE
        /*Hashing Change*/
        pLcmlHandle = (LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle;
        eError = LCML_FlushHashes(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle);
        if (eError != OMX_ErrorNone) {
            OMX_ERROR2(pComponentPrivate->dbg, "Error occurred in Codec mapping flush!\n");
        }
#endif          

#ifdef __PERF_INSTRUMENTATION__
        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | 
                      PERF_BoundarySteadyState);
#endif  

        if (pComponentPrivate->codecStop_waitingsignal == 0){
            pthread_mutex_lock(&pComponentPrivate->codecStop_mutex); 
        } 
        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                   MMCodecControlStop,
                                   (void *)pArgs);
        if (pComponentPrivate->codecStop_waitingsignal == 0){
            pthread_cond_wait(&pComponentPrivate->codecStop_threshold, &pComponentPrivate->codecStop_mutex);
            pComponentPrivate->codecStop_waitingsignal = 0;
            pthread_mutex_unlock(&pComponentPrivate->codecStop_mutex);
        }
        pComponentPrivate->curState = OMX_StateIdle;
#ifdef RESOURCE_MANAGER_ENABLED        

        rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, 
                                          OMX_WMA_Decoder_COMPONENT, 
                                          OMX_StateIdle, 1234, NULL);
        if(rm_error != OMX_ErrorNone)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: ERROR: Send Comman Failed", __LINE__);
            goto EXIT;
        }
#endif        
        
        OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: The component is stopped\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler (pHandle,
                                                pHandle->pApplicationPrivate,
                                                OMX_EventCmdComplete,OMX_CommandStateSet,
                                                pComponentPrivate->curState,
                                                NULL);
    } 
    else
    {
        /* This means, it is invalid state from application */
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler(pHandle,
                                               pHandle->pApplicationPrivate,
                                               OMX_EventError,
                                               OMX_ErrorIncorrectStateTransition,
                                               OMX_TI_ErrorMinor,
                                               NULL);
    }

 EXIT:
    return eError;
}
/* ================================================================================= */
/**
 * @fn WMADEC_CommandToLoaded() description for WMADEC_CommandToLoaded  
 WMADEC_CommandToLoaded().  
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
OMX_ERRORTYPE WMADEC_CommandToLoaded(WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{

    OMX_COMPONENTTYPE *pHandle =
        (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    char *p = "hello";
    OMX_PRINT1(pComponentPrivate->dbg, "%d: WMADECHandleCommand: Cmd Loaded - curState = %d\n",
                  __LINE__,pComponentPrivate->curState);
    if (pComponentPrivate->curState == OMX_StateLoaded)
    {
        pComponentPrivate->cbInfo.EventHandler (pHandle,
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                OMX_ErrorSameState,
                                                OMX_TI_ErrorMinor,
                                                NULL);
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Same State Given by Application\n",__LINE__);
        goto EXIT;
    }
    OMX_PRBUFFER1(pComponentPrivate->dbg, "%d: pComponentPrivate->pInputBufferList->numBuffers = %d\n",
                  __LINE__,pComponentPrivate->pInputBufferList->numBuffers);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "%d: pComponentPrivate->pOutputBufferList->numBuffers = %d\n",
                  __LINE__,pComponentPrivate->pOutputBufferList->numBuffers);

    if (pComponentPrivate->curState == OMX_StateWaitForResources)
    {
        OMX_PRDSP2(pComponentPrivate->dbg, "%d: WMADECHandleCommand: Cmd Loaded\n",__LINE__);
        
#ifdef __PERF_INSTRUMENTATION__
        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundaryCleanup); 
#endif  
        pComponentPrivate->curState = OMX_StateLoaded;
        
#ifdef __PERF_INSTRUMENTATION__
        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundaryCleanup);
#endif  
        pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventCmdComplete, 
                                                OMX_CommandStateSet,
                                                pComponentPrivate->curState,
                                                NULL);
        goto EXIT;
    }
    OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: In side OMX_StateLoaded State: \n",__LINE__);
    if (pComponentPrivate->curState != OMX_StateIdle &&
        pComponentPrivate->curState != OMX_StateWaitForResources)
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler (pHandle,
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                OMX_ErrorIncorrectStateTransition,
                                                OMX_TI_ErrorMinor, 
                                                NULL);
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Invalid State Given by Application\n",__LINE__);
        goto EXIT;
    }
#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundaryCleanup);
#endif  
    OMX_PRBUFFER1(pComponentPrivate->dbg, "pComponentPrivate->pInputBufferList->numBuffers = %d\n",
                  pComponentPrivate->pInputBufferList->numBuffers);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "pComponentPrivate->pOutputBufferList->numBuffers = %d\n",
                  pComponentPrivate->pOutputBufferList->numBuffers);
    while (1)
    {
        OMX_PRDSP2(pComponentPrivate->dbg, "HandleCommand - in while(1) loop\n");
        if (!pComponentPrivate->pInputBufferList->numBuffers &&
            !pComponentPrivate->pOutputBufferList->numBuffers)
        {
            break;
        }
        pComponentPrivate->InIdle_goingtoloaded = 1;        
#ifndef UNDER_CE
        pthread_mutex_lock(&pComponentPrivate->InIdle_mutex); 
        pthread_cond_wait(&pComponentPrivate->InIdle_threshold, 
                          &pComponentPrivate->InIdle_mutex);
        pthread_mutex_unlock(&pComponentPrivate->InIdle_mutex);
#else
        OMX_WaitForEvent(&(pComponentPrivate->InIdle_event));
#endif
        break;
    }

    /* Now Deinitialize the component No error should be returned from
     * this function. It should clean the system as much as possible */
    OMX_PRSTATE1(pComponentPrivate->dbg, "%d :: In side OMX_StateLoaded State: \n",__LINE__);
    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                               EMMCodecControlDestroy, (void *)p);
    OMX_PRSTATE1(pComponentPrivate->dbg, "%d :: In side OMX_StateLoaded State: \n",__LINE__);
    if (eError != OMX_ErrorNone)
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d : Error: in Destroying the codec: no.  %x\n",__LINE__, eError);
        goto EXIT;
    }
    OMX_PRDSP1(pComponentPrivate->dbg, "%d: WMADECHandleCommand: Cmd Loaded\n",__LINE__);
#ifdef __PERF_INSTRUMENTATION__
    PERF_SendingCommand(pComponentPrivate->pPERF, -1, 0, PERF_ModuleComponent);
#endif
    eError = EXIT_COMPONENT_THRD;
    pComponentPrivate->bInitParamsInitialized = 0;
    /* Send StateChangeNotification to application */
    pComponentPrivate->bLoadedCommandPending = OMX_FALSE;

    
 EXIT:
    return eError;
}

/* ================================================================================= */
/**
 * @fn WMADEC_CommandToExecuting() description for WMADEC_CommandToExecuting  
 WMADEC_CommandToExecuting().  
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
OMX_ERRORTYPE WMADEC_CommandToExecuting(WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;
    OMX_U16 i;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    char *p = "hello";

    LCML_WMADEC_BUFHEADERTYPE *pLcmlHdr = NULL;
    OMX_U32 cmdValues[3];
    char *pArgs = "damedesuStr";

#ifdef RESOURCE_MANAGER_ENABLED 
    int rm_error;
#endif    
    OMX_PRINT1(pComponentPrivate->dbg, "%d: WMADECHandleCommand: Cmd Executing \n",__LINE__);
    if (pComponentPrivate->curState == OMX_StateExecuting)
    {
        pComponentPrivate->cbInfo.EventHandler (pHandle,
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                OMX_ErrorSameState,
                                                OMX_TI_ErrorMinor,
                                                NULL);
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Same State Given by Application\n",__LINE__);
        goto EXIT;
    }
    else if (pComponentPrivate->curState == OMX_StateIdle)
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        /* Sending commands to DSP via LCML_ControlCodec third argument
           is not used for time being */

        pComponentPrivate->nNumInputBufPending = 0;
        pComponentPrivate->nNumOutputBufPending = 0;
        if(pComponentPrivate->dasfmode == 1)
        {
            OMX_U32 pValues[4];
            OMX_PRDSP2(pComponentPrivate->dbg, "%d :: ---- Comp: DASF Functionality is ON ---\n",__LINE__);
            if (pComponentPrivate->pHeaderInfo->iChannel == 1)
            {
                pComponentPrivate->pParams->iAudioFormat = WMA_MONO_CHANNEL;
            }
            else
            {
                pComponentPrivate->pParams->iAudioFormat = WMA_STEREO_NON_INTERLEAVED;
            }
            OMX_PRINT1(pComponentPrivate->dbg, "pParams->iAudioFormat = %d\n", pComponentPrivate->pParams->iAudioFormat);
            pComponentPrivate->pParams->iStrmId = pComponentPrivate->streamID;
            pComponentPrivate->pParams->iSamplingRate = 
                pComponentPrivate->pHeaderInfo->iSamplePerSec;

            OMX_PRINT1(pComponentPrivate->dbg, "pParams->iSamplingRate = %ld\n",
                          pComponentPrivate->pParams->iSamplingRate);
            OMX_PRINT1(pComponentPrivate->dbg, "pParams->iStrmId = %ld\n",
                          pComponentPrivate->pParams->iStrmId);
            OMX_PRINT1(pComponentPrivate->dbg, "pParams->iAudioFormat = %d\n",
                          pComponentPrivate->pParams->iAudioFormat);

            pValues[0] = USN_STRMCMD_SETCODECPARAMS;
            pValues[1] = (OMX_U32)pComponentPrivate->pParams;
            pValues[2] = sizeof(WMADEC_AudioCodecParams);
            pValues[3] = OUTPUT_PORT;
            eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                       EMMCodecControlStrmCtrl,(void *)pValues);
            if(eError != OMX_ErrorNone)
            {
                OMX_ERROR4(pComponentPrivate->dbg, "%d: Error Occurred in Codec StreamControl..\n",
                              __LINE__);
                goto EXIT;
            }
        }
        if( pComponentPrivate->dasfmode )
        {
            OMX_PRDSP2(pComponentPrivate->dbg, "Setting WMA_IAUDIO_BLOCK\n");
            pComponentPrivate->pDynParams->iOutputFormat = WMA_IAUDIO_BLOCK; /* EAUDIO_BLOCKED */
        }
        else
        {
            OMX_PRDSP1(pComponentPrivate->dbg, "Setting WMA_IAUDIO_INTERLEAVED\n");
            pComponentPrivate->pDynParams->iOutputFormat = WMA_IAUDIO_INTERLEAVED; /* EAUDIO_INTERLEAVED */
        }
        pComponentPrivate->pDynParams->size = sizeof( WMADEC_UALGParams );
        
        cmdValues[0] = WMA_IUALG_CMD_SETSTATUS;
        cmdValues[1] = (OMX_U32)( pComponentPrivate->pDynParams );
        cmdValues[2] = sizeof( WMADEC_UALGParams );

        p = (void *)&cmdValues;
        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                   EMMCodecControlAlgCtrl, (void *)p);

        if(eError != OMX_ErrorNone)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d: Error Occurred in Codec algctrl..\n", __LINE__);
            goto EXIT;
        }
        pComponentPrivate->bBypassDSP = 0;

        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)(pComponentPrivate->pLcmlHandle))->pCodecinterfacehandle,
                                   EMMCodecControlStart, (void *)pArgs);

        if(eError != OMX_ErrorNone)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d: Error Occurred in Codec Start..\n", __LINE__);
            goto EXIT;
        }
        OMX_PRSTATE2(pComponentPrivate->dbg, ": Codec Has Been Started \n");
                    
        pComponentPrivate->SendAfterEOS = 1; /*Enrique Zertuche, added because it wasnt set compared with the mp3*/
    }
    else if (pComponentPrivate->curState == OMX_StatePause)
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                   EMMCodecControlStart, (void *)pArgs);
        if (eError != OMX_ErrorNone)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "Error While Resuming the codec\n");
            goto EXIT;
        }
        if (pComponentPrivate->nNumInputBufPending < 
            pComponentPrivate->pInputBufferList->numBuffers)
        {
            pComponentPrivate->nNumInputBufPending = pComponentPrivate->pInputBufferList->numBuffers;
        }

        for (i=0; i < pComponentPrivate->nNumInputBufPending; i++)
        {
            if (pComponentPrivate->pInputBufHdrPending[i])
            {
                if (!WMADEC_IsPending(pComponentPrivate, 
                                      pComponentPrivate->pInputBufHdrPending[i], 
                                      OMX_DirInput) )
                {
                    WMADECGetCorresponding_LCMLHeader(
                                                      pComponentPrivate,
                                                      pComponentPrivate->pInputBufHdrPending[i]->pBuffer, 
                                                      OMX_DirInput, &pLcmlHdr);
                                
                    WMADEC_SetPending(pComponentPrivate,
                                      pComponentPrivate->pInputBufHdrPending[i],
                                      OMX_DirInput);
                                      
                    eError = LCML_QueueBuffer(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                              EMMCodecInputBuffer,  
                                              pComponentPrivate->pInputBufHdrPending[i]->pBuffer,
                                              pComponentPrivate->pInputBufHdrPending[i]->nAllocLen,
                                              pComponentPrivate->pInputBufHdrPending[i]->nFilledLen,
                                              (OMX_U8 *) pLcmlHdr->pIpParam,
                                              sizeof(WMADEC_UAlgInBufParamStruct),
                                              NULL);
                }
            }
        }
        pComponentPrivate->nNumInputBufPending = 0;
        if (pComponentPrivate->nNumOutputBufPending < 
            pComponentPrivate->pOutputBufferList->numBuffers) 
        {
            pComponentPrivate->nNumOutputBufPending = 
                pComponentPrivate->pOutputBufferList->numBuffers;
        }
        for (i=0; i < pComponentPrivate->nNumOutputBufPending; i++) 
        {
            if (pComponentPrivate->pOutputBufHdrPending[i]) 
            {
                if (!WMADEC_IsPending(pComponentPrivate, 
                                      pComponentPrivate->pOutputBufHdrPending[i], 
                                      OMX_DirOutput) ) 
                {
                    WMADECGetCorresponding_LCMLHeader(
                                                      pComponentPrivate,
                                                      pComponentPrivate->pOutputBufHdrPending[i]->pBuffer, 
                                                      OMX_DirOutput, &pLcmlHdr);
                            
                    WMADEC_SetPending(pComponentPrivate,
                                      pComponentPrivate->pOutputBufHdrPending[i],
                                      OMX_DirOutput);
                                      
                    pComponentPrivate->LastOutputBufferHdrQueued = 
                        pComponentPrivate->pOutputBufHdrPending[i];

                    eError = LCML_QueueBuffer(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                              EMMCodecOuputBuffer,  
                                              pComponentPrivate->pOutputBufHdrPending[i]->pBuffer,
                                              pComponentPrivate->pOutputBufHdrPending[i]->nAllocLen,
                                              pComponentPrivate->pOutputBufHdrPending[i]->nFilledLen,
                                              NULL,
                                              sizeof(WMADEC_UAlgInBufParamStruct),
                                              NULL);
                }
            }
        }
        pComponentPrivate->nNumOutputBufPending = 0;
    }
    else 
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler (pHandle,
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                OMX_ErrorIncorrectStateTransition,
                                                OMX_TI_ErrorMinor,
                                                NULL);
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Invalid State Given by Application\n",__LINE__);
        goto EXIT;
    }
#ifdef RESOURCE_MANAGER_ENABLED    
    rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, 
                                      OMX_WMA_Decoder_COMPONENT, 
                                      OMX_StateExecuting, 1234, NULL);
#endif
    
    
    pComponentPrivate->curState = OMX_StateExecuting;
#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | 
                  PERF_BoundarySteadyState);
#endif  
    /*Send state change notificaiton to Application */
    OMX_PRINT1(pComponentPrivate->dbg, "About to call EventHandler\n");
    pComponentPrivate->cbInfo.EventHandler( pHandle, pHandle->pApplicationPrivate,
                                            OMX_EventCmdComplete, OMX_CommandStateSet,
                                            pComponentPrivate->curState, NULL);
 EXIT:
    return eError;
}

/* ================================================================================= */
/**
 * @fn WMADEC_CommandToPause() description for WMADEC_CommandToPause  
 WMADEC_CommandToPause().  
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
OMX_ERRORTYPE WMADEC_CommandToPause(WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{

    OMX_COMPONENTTYPE *pHandle =
        (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    char *p = "hello";

    OMX_PRINT1(pComponentPrivate->dbg, "%d: WMADECHandleCommand: Cmd Pause\n",__LINE__);
    if (pComponentPrivate->curState == OMX_StatePause)
    {
        pComponentPrivate->cbInfo.EventHandler (pHandle, pHandle->pApplicationPrivate,
                                                OMX_EventError, OMX_ErrorSameState,
                                                OMX_TI_ErrorMinor, NULL);
                                                
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Same State Given by Application\n",__LINE__);
        goto EXIT;
    }
    if (pComponentPrivate->curState != OMX_StateExecuting &&
        pComponentPrivate->curState != OMX_StateIdle) 
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        
        pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                OMX_ErrorIncorrectStateTransition,
                                                OMX_TI_ErrorMajor,
                                                NULL);
                                                
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Invalid State Given by Application\n",__LINE__);
        goto EXIT;
    }
#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif
    OMX_PRSTATE1(pComponentPrivate->dbg, "About to command to pause\n");
    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                               EMMCodecControlPause, (void *)p);

    if (eError != OMX_ErrorNone) 
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d : Error: in Pausing the codec\n",__LINE__);
        goto EXIT;
    }

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
 EXIT:

    return eError;
}
/* ================================================================================= */
/**
 * @fn WMADEC_CommandToWaitForResources() description for WMADEC_CommandToWaitForResources  
 WMADEC_CommandToWaitForResources().  
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
OMX_ERRORTYPE WMADEC_CommandToWaitForResources(WMADEC_COMPONENT_PRIVATE *pComponentPrivate)
{  

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;

    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (pComponentPrivate->curState == OMX_StateWaitForResources)
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        
        pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                OMX_ErrorSameState,
                                                OMX_TI_ErrorMinor,
                                                NULL);
                                                
        OMX_ERROR2(pComponentPrivate->dbg, "%d :: Error: Same State Given by Application\n",__LINE__);
    }
    else if (pComponentPrivate->curState == OMX_StateLoaded)
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        pComponentPrivate->curState = OMX_StateWaitForResources;
        
        pComponentPrivate->cbInfo.EventHandler(pHandle,
                                               pHandle->pApplicationPrivate,
                                               OMX_EventCmdComplete, 
                                               OMX_CommandStateSet, 
                                               pComponentPrivate->curState,
                                               NULL);
    }
    else
    {
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        
        pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                               pHandle->pApplicationPrivate,
                                               OMX_EventError, 
                                               OMX_ErrorIncorrectStateTransition,
                                               OMX_TI_ErrorMinor, 
                                               "NULL");
    }

    return eError;
}

/* ================================================================================= */
/**
 * @fn WMADEC_SetPending() description for WMADEC_SetPending  
 WMADEC_SetPending().  
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
void WMADEC_SetPending(WMADEC_COMPONENT_PRIVATE *pComponentPrivate, 
                       OMX_BUFFERHEADERTYPE *pBufHdr, 
                       OMX_DIRTYPE eDir) 
{

    OMX_U16 i;

    if (eDir == OMX_DirInput)
    {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++)
        {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i])
            {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 1;
                OMX_PRBUFFER2(pComponentPrivate->dbg, "*******************INPUT BUFFER %d IS PENDING \
                              ******************************\n",i);
            }
        }
    }
    else
    {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++)
        {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i])
            {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 1;
                OMX_PRBUFFER2(pComponentPrivate->dbg, "*******************OUTPUT BUFFER %d IS PENDING \
                              ******************************\n",i);
            }
        }
    }
}
/* ================================================================================= */
/**
 * @fn WMADEC_ClearPending() description for WMADEC_ClearPending  
 WMADEC_ClearPending().  
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

void WMADEC_ClearPending(WMADEC_COMPONENT_PRIVATE *pComponentPrivate, 
                         OMX_BUFFERHEADERTYPE *pBufHdr, 
                         OMX_DIRTYPE eDir) 
{

    OMX_U16 i;

    if (eDir == OMX_DirInput)
    {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++)
        {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i])
            {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 0;
                OMX_PRBUFFER1(pComponentPrivate->dbg, "*******************INPUT BUFFER %d IS RECLAIMED\
                              ******************************\n",i);
            }
        }
    }
    else
    {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++)
        {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i])
            {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 0;
                OMX_PRBUFFER1(pComponentPrivate->dbg, "*******************OUTPUT BUFFER %d IS RECLAIMED******************************\n",i);
            }
        }
    }
}

/* ================================================================================= */
/**
 * @fn WMADEC_IsPending() description for WMADEC_IsPending  
 WMADEC_IsPending().  
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
OMX_U32 WMADEC_IsPending(WMADEC_COMPONENT_PRIVATE *pComponentPrivate, 
                         OMX_BUFFERHEADERTYPE *pBufHdr, 
                         OMX_DIRTYPE eDir) 
{   

    OMX_U16 i;

    if (eDir == OMX_DirInput)
    {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++)
        {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i])
            {
                return pComponentPrivate->pInputBufferList->bBufferPending[i];
            }
        }
    }
    else
    {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++)
        {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i])
            {
                return pComponentPrivate->pOutputBufferList->bBufferPending[i];
            }
        }
    }
    return -1;
}

/* ================================================================================= */
/**
 * @fn WMADEC_IsValid() description for WMADEC_Fill_LCMLInitParamsEx  
 WMADEC_IsValid().  
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
OMX_U32 WMADEC_IsValid(WMADEC_COMPONENT_PRIVATE *pComponentPrivate, 
                       OMX_U8 *pBuffer, 
                       OMX_DIRTYPE eDir) 
{
    OMX_U16 i;
    int found=0;

    if (eDir == OMX_DirInput)
    {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++)
        {
            if (pBuffer == pComponentPrivate->pInputBufferList->pBufHdr[i]->pBuffer)
            {
                found = 1;
            }
        }
    }
    else
    {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++)
        {
            if (pBuffer == pComponentPrivate->pOutputBufferList->pBufHdr[i]->pBuffer)
            {
                found = 1;
            }
        }
    }
    return found;
}


/* ================================================================================= */
/**
 * @fn WMADEC_Fill_LCMLInitParamsEx() description for WMADEC_Fill_LCMLInitParamsEx  
 WMADEC_Fill_LCMLInitParamsEx().  
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

OMX_ERRORTYPE WMADECFill_LCMLInitParamsEx(OMX_HANDLETYPE pComponent,OMX_U32 indexport)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf,nIpBufSize,nOpBuf,nOpBufSize;
    OMX_U16 i;
    OMX_BUFFERHEADERTYPE *pTemp;
    int size_lcml;
    char *char_temp = NULL;
    LCML_WMADEC_BUFHEADERTYPE *pTemp_lcml = NULL;
    LCML_DSP_INTERFACE *pHandle = (LCML_DSP_INTERFACE *)pComponent;
    WMADEC_COMPONENT_PRIVATE *pComponentPrivate = pHandle->pComponentPrivate;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entered WMADECFill_LCMLInitParams\n",__LINE__);
    OMX_PRDSP1(pComponentPrivate->dbg, "%d :: WMADECFill_LCMLInitParams - pComponentPrivate = %p\n",
                  __LINE__,pComponentPrivate);
    OMX_PRDSP1(pComponentPrivate->dbg, "%d :: WMADECFill_LCMLInitParams - pHandle = %p\n",
                  __LINE__,pHandle);

    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nIpBufSize = INPUT_WMADEC_BUFFER_SIZE;

    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    nOpBufSize = pComponentPrivate->pPortDef[OUTPUT_BUFFER]->nBufferSize;


    OMX_PRBUFFER1(pComponentPrivate->dbg, "nIpBuf = %ld\n",nIpBuf);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "nOpBuf = %ld\n",nOpBuf);
    if(indexport == 0 || indexport == -1){
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_WmaDecUtils.c\n",__LINE__);
        size_lcml = nIpBuf * sizeof(LCML_WMADEC_BUFHEADERTYPE);
        WMAD_OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, LCML_WMADEC_BUFHEADERTYPE);
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[ALLOC] %p\n",__LINE__,pTemp_lcml);
        OMX_PRDSP1(pComponentPrivate->dbg, "Line %d::pTemp_lcml = %p\n",__LINE__,pTemp_lcml);
    
        if(pTemp_lcml == NULL)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Memory Allocation Failed\n",__LINE__);
            eError = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    
        pComponentPrivate->pLcmlBufHeader[INPUT_PORT] = pTemp_lcml;

        OMX_PRBUFFER1(pComponentPrivate->dbg, "nIpBuf = %ld\n",nIpBuf);
    
        for (i=0; i<nIpBuf; i++)
        {
            pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
            pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
            pTemp->nAllocLen = nIpBufSize;
            pTemp->nFilledLen = nIpBufSize;
            pTemp->nVersion.s.nVersionMajor = WMADEC_MAJOR_VER;
            pTemp->nVersion.s.nVersionMinor = WMADEC_MINOR_VER;
            pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
            pTemp->nTickCount = NOT_USED;
            pTemp_lcml->buffer = pTemp;
            pTemp_lcml->eDir = OMX_DirInput;

            WMAD_OMX_MALLOC_SIZE(pTemp_lcml->pIpParam, 
                                 sizeof(WMADEC_UAlgInBufParamStruct)+256,
                                 WMADEC_UAlgInBufParamStruct);
                             
            char_temp = (char*)pTemp_lcml->pIpParam;
            char_temp += 128;
            pTemp_lcml->pIpParam = (WMADEC_UAlgInBufParamStruct*)char_temp;
        
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[ALLOC] %p\n",__LINE__,pTemp_lcml->pIpParam);
            if (pTemp_lcml->pIpParam == NULL)
            {
                /* Free previously allocated memory before bailing */
                if (pTemp_lcml != NULL) {
                    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:::[FREE] %p\n",__LINE__,pTemp_lcml);
                    OMX_WMADECMEMFREE_STRUCT(pTemp_lcml);
                }

                OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Malloc Failed...Exiting..\n",__LINE__);
                goto EXIT;
            }
            pTemp_lcml->pIpParam->bLastBuffer = 0;

            /* This means, it is not a last buffer. This flag is to be modified by
             * the application to indicate the last buffer */
            pTemp->nFlags = NORMAL_BUFFER;
            pTemp_lcml++;
        }
    }
    if(indexport == 1 || indexport == -1){
        /* Allocate memory for all output buffer headers..
         * This memory pointer will be sent to LCML */
        size_lcml = nOpBuf * sizeof(LCML_WMADEC_BUFHEADERTYPE);
        WMAD_OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, LCML_WMADEC_BUFHEADERTYPE);
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:[ALLOC] %p\n",__LINE__,pTemp_lcml);
    
        if(pTemp_lcml == NULL)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Memory Allocation Failed\n",__LINE__);
            eError = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
    
        pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT] = pTemp_lcml;

        for (i=0; i<nOpBuf; i++)
        {
            pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[i];
            pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
            pTemp->nAllocLen = nOpBufSize;
            pTemp->nFilledLen = nOpBufSize;
            pTemp->nVersion.s.nVersionMajor = WMADEC_MAJOR_VER;
            pTemp->nVersion.s.nVersionMinor = WMADEC_MINOR_VER;
            pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
            pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
            pTemp->nTickCount = NOT_USED;
            /* This means, it is not a last buffer. This flag is to be modified by
             * the application to indicate the last buffer */

            pTemp_lcml->buffer = pTemp;
            pTemp_lcml->eDir = OMX_DirOutput;
            pTemp->nFlags = NORMAL_BUFFER;
            pTemp++;
            pTemp_lcml++;
        }
    }
    pComponentPrivate->bPortDefsAllocated = 1;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting WMADECFill_LCMLInitParams",__LINE__);

    pComponentPrivate->bInitParamsInitialized = 1;
 EXIT:
    return eError;
}
/*  =========================================================================*/
/*  func    GetBits                                                          */
/*                                                                           */
/*  desc    Gets aBits number of bits from position aPosition of one buffer  */
/*            and returns the value in a TUint value.                        */
/*  =========================================================================*/
OMX_U32 WMADEC_GetBits(OMX_U32* nPosition, OMX_U8 nBits, OMX_U8* pBuffer, OMX_BOOL bIcreasePosition)
{
    OMXDBG_PRINT(stderr, PRINT, 1, 0, "%d :: Entering WMADEC_GetBits",__LINE__);
    OMX_U32 nOutput;
    OMX_U32 nNumBitsRead = 0;
    OMX_U32 nBytePosition = 0;
    OMX_U8  nBitPosition =  0;
    nBytePosition = *nPosition / 8;
    nBitPosition =  *nPosition % 8;

    if (bIcreasePosition)
    *nPosition += nBits;
    nOutput = ((OMX_U32)pBuffer[nBytePosition] << (24+nBitPosition) );
    nNumBitsRead = nNumBitsRead + (8 - nBitPosition);
    if (nNumBitsRead < nBits)
    {
        nOutput = nOutput | ( pBuffer[nBytePosition + 1] << (16+nBitPosition));
        nNumBitsRead = nNumBitsRead + 8;
    }
    if (nNumBitsRead < nBits)
    {
        nOutput = nOutput | ( pBuffer[nBytePosition + 2] << (8+nBitPosition));
        nNumBitsRead = nNumBitsRead + 8;
    }
    if (nNumBitsRead < nBits)
    {
        nOutput = nOutput | ( pBuffer[nBytePosition + 3] << (nBitPosition));
        nNumBitsRead = nNumBitsRead + 8;
    }
    nOutput = nOutput >> (32 - nBits) ;
    return nOutput;
}
/*  ==========================================================================*/
/*  func    WMADEC_Parser                                                     */
/*                                                                            */
/*  desc      Parses the WAVEFORMATEX data structure that is sent in the 1st  */
/*            buffer and the it creates the RCA header and the block align    */
/*  ==========================================================================*/
OMX_ERRORTYPE WMADEC_Parser(OMX_U8* pBuffer, RCA_HEADER *pStreamData)
{
    OMX_ERRORTYPE eError=0;
    OMXDBG_PRINT(stderr, PRINT, 1, 0, "Entering WMADEC_Parser function\n");                

    //These are the important fields that the socket node needs
    pStreamData->iTypeSpecific=0x1c;
    pStreamData->iFormatTag= *((OMX_U16*)(pBuffer));
    pStreamData->iChannel=*((OMX_U16 *)(pBuffer+2));    
    pStreamData->iSamplePerSec=*((OMX_U32 *)(pBuffer+4));
    pStreamData->iAvgBytesPerSec=*((OMX_U32 *)(pBuffer+8)); //check
    pStreamData->iBlockAlign=*((OMX_U16 *)(pBuffer+2+4+4+2));
    pStreamData->iValidBitsPerSample=*((OMX_U16 *)(pBuffer+2+4+4+2+2));   
    pStreamData->iEncodeOptV=*((OMX_U16 *)(pBuffer+2+4+4+2+2+2+2+2+2));
    pStreamData->iMaxPacketSize=pStreamData->iBlockAlign+31;

    /*After this, the buffer has extra info, the Codc Specific Data Size which has 16 bites of length (2 bytes), and the Codec Specific Data of vari*/
    //Parsing information ended.

    switch (pStreamData->iFormatTag)
    {

    case WAVE_FORMAT_MSAUDIO :
        {
            OMXDBG_PRINT(stderr, DSP, 2, 0, "WAVE_FORMAT_MSAUDIO\n");
            break;                       
            /*Make Necessary validations for supported parameter for specific format here */ 
        }
    case WAVE_FORMAT_MSAUDIO_9 :
        {
            OMXDBG_PRINT(stderr, DSP, 2, 0, "WAVE_FORMAT_MSAUDIO_9\n");
            break;                       
            /*Make Necessary validations for supported parameter for specific format here */ 
        }
    case WAVE_FORMAT_MSAUDIO_9_LOOSELESS :
        {
            OMXDBG_PRINT(stderr, DSP, 2, 0, "WAVE_FORMAT_MSAUDIO_9_LOOSELESS\n");
            break;                       
            /*Make Necessary validations for supported parameter for specific format here */ 
        }       
    default:
        {
            break;
        }       
    }           
    
    OMXDBG_PRINT(stderr, PRINT, 1, 0, "Exiting WMADEC_Parser function\n");         
    return eError;
}


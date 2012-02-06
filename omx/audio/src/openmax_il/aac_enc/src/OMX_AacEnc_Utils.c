
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
* @file OMX_AacEnc_Utils.c
*
* This file implements OMX Component for AAC encoder that
* is fully compliant with the OMX Audio specification 1.0.
*
* @path  $(CSLPATH)\
*
* @rev  1.0
*/
/* ----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 13-Dec-2005 mf:  Initial Version. Change required per OMAPSWxxxxxxxxx
*! to provide _________________.
*!
*!
*! 13-Dec-2005 mf:
*! This is newest file
* =========================================================================== */


/* ------compilation control switches -------------------------*/
/****************************************************************
*  INCLUDE FILES
****************************************************************/
/* ----- system and platform files ----------------------------*/
#include <wchar.h>
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
#include <dlfcn.h>
#include <dbapi.h>
#include <string.h>
#include <stdio.h>
#include "OMX_AacEnc_Utils.h"
#include "Aacencsocket_ti.h"
#include <encode_common_ti.h>
#include "OMX_AacEnc_CompThread.h"
#include "usn.h"
#ifdef RESOURCE_MANAGER_ENABLED
#include <ResourceManagerProxyAPI.h>
#endif




/* ========================================================================== */
/**
* @AACENCFill_LCMLInitParams () This function is used by the component thread to
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
/*static AACENC_COMPONENT_PRIVATE *pComponentPrivate_CC;*/

OMX_ERRORTYPE AACENCFill_LCMLInitParams(OMX_HANDLETYPE pComponent, LCML_DSP *plcml_Init, OMX_U16 arr[])
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf =0 ,nIpBufSize=0 ,nOpBuf = 0 ,nOpBufSize=0;
    OMX_U32 i=0, j=0, k=0;
    OMX_BUFFERHEADERTYPE *pTemp             = NULL;
    OMX_S32 size_lcml                       = 0;
    char *ptr;
    LCML_AACENC_BUFHEADERTYPE *pTemp_lcml   = NULL;
    LCML_DSP_INTERFACE *pHandle = (LCML_DSP_INTERFACE *)pComponent;
     AACENC_COMPONENT_PRIVATE *pComponentPrivate = pHandle->pComponentPrivate;
    LCML_STRMATTR *strmAttr                 = NULL;
    OMX_U16 HigherBitsSamplingRate          = 0;
    OMX_U16 FramesPerOutBuf                 = 0;
    OMX_U16 Channels                        = 0;


    OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: AACENCFill_LCMLInitParams\n ",__LINE__);
    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    pComponentPrivate->nRuntimeInputBuffers = nIpBuf;
    nIpBufSize = pComponentPrivate->pPortDef[INPUT_PORT]->nBufferSize;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    nOpBufSize = pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferSize; 
    /*saving a copy a number of output buffers */
    pComponentPrivate->nRuntimeOutputBuffers = nOpBuf;

    /*recovering the value for the number of frames per Ouput Buffer */
    FramesPerOutBuf = (OMX_U16)pComponentPrivate->FramesPer_OutputBuffer;
    OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: UTIL: Frames per output buffer = %d \n\n",__LINE__, FramesPerOutBuf);


    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: ------ Buffer Details -----------\n",__LINE__);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: Input  Buffer Count = %ld \n",__LINE__,nIpBuf);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: Input  Buffer Size = %ld\n",__LINE__,nIpBufSize);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: Output Buffer Count = %ld\n",__LINE__,nOpBuf);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: Output Buffer Size = %ld\n",__LINE__,nOpBufSize);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: ------ Buffer Details ------------\n",__LINE__);

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

    plcml_Init->NodeInfo.AllUUIDs[0].uuid = (struct DSP_UUID*)&MPEG4AACENC_SN_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[0].DllName,AACENC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[0].eDllType = DLL_NODEOBJECT;


    plcml_Init->NodeInfo.AllUUIDs[1].uuid = (struct DSP_UUID*)&MPEG4AACENC_SN_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[1].DllName,AACENC_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[1].eDllType = DLL_DEPENDENT;


    plcml_Init->NodeInfo.AllUUIDs[2].uuid = &USN_TI_UUID;
    strcpy ((char*)plcml_Init->NodeInfo.AllUUIDs[2].DllName,AACENC_USN_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[2].eDllType = DLL_DEPENDENT;

    plcml_Init->DeviceInfo.TypeofDevice = 0;
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_AacEncUtils.c\n",__LINE__);
    if(pComponentPrivate->dasfmode == 1) {
        OMX_PRDSP2(pComponentPrivate->dbg, "%d :: Codec is configuring to DASF mode\n",__LINE__);
        OMX_MALLOC_GENERIC(strmAttr, LCML_STRMATTR);
        OMX_PRINT1(pComponentPrivate->dbg, "strmAttr %p \n",strmAttr);
        
        pComponentPrivate->strmAttr = strmAttr;
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: [ALLOC] %p\n",__LINE__,strmAttr);
        strmAttr->uSegid        = 0;
        strmAttr->uAlignment    = 0;
        strmAttr->uTimeout      = 1000;
        strmAttr->uBufsize      = INPUT_AACENC_BUFFER_SIZE_DASF;
        strmAttr->uNumBufs      = NUM_AACENC_INPUT_BUFFERS_DASF;
        strmAttr->lMode         = STRMMODE_PROCCOPY;

        plcml_Init->DeviceInfo.TypeofDevice = 1;
        plcml_Init->DeviceInfo.TypeofRender = 1;
        plcml_Init->DeviceInfo.AllUUIDs[0].uuid = &DCTN_TI_UUID;
        plcml_Init->DeviceInfo.DspStream = strmAttr;
    }

    /*copy the other information*/
    plcml_Init->SegID       = OMX_AACENC_DEFAULT_SEGMENT;
    plcml_Init->Timeout     = -1;/*OMX_AACENC_SN_TIMEOUT;*/
    plcml_Init->Alignment   = 0;
    plcml_Init->Priority    = OMX_AACENC_SN_PRIORITY;
    plcml_Init->ProfileID   = -1; 

    pComponentPrivate->unNumChannels   = (OMX_U16)pComponentPrivate->aacParams[OUTPUT_PORT]->nChannels;       /*Number of Channels*/
    /* splitting values for Sample rate and bit rate */
    pComponentPrivate->ulSamplingRate  = (OMX_U16)pComponentPrivate->aacParams[OUTPUT_PORT]->nSampleRate;       /*Sampling rate*/
    HigherBitsSamplingRate             =(OMX_U16)((pComponentPrivate->aacParams[OUTPUT_PORT]->nSampleRate >>16)& 0xFFFF); 
    pComponentPrivate->unBitrate       = pComponentPrivate->aacParams[OUTPUT_PORT]->nBitRate;                /*Bit rate 2bytes*/
    pComponentPrivate->nObjectType     = pComponentPrivate->aacParams[OUTPUT_PORT]->eAACProfile;            /*Object Type */

    /*   Remaping Number of channels for SN */
    /*   SN does use 0: Mono and  1: stereo      */
    if(pComponentPrivate->unNumChannels==2)
    {   
        Channels=1;
    }
    else if (pComponentPrivate->unNumChannels==1)
    {
        Channels=0;
    }
    
    if (pComponentPrivate->aacParams[OUTPUT_PORT]->eAACStreamFormat == OMX_AUDIO_AACStreamFormatRAW ||
        pComponentPrivate->aacParams[OUTPUT_PORT]->eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4FF){
        pComponentPrivate->File_Format = 0;
            OMX_PRDSP2(pComponentPrivate->dbg, "OMX_AUDIO_AACStreamFormatRAW \n");
    }
    else if (pComponentPrivate->aacParams[OUTPUT_PORT]->eAACStreamFormat == OMX_AUDIO_AACStreamFormatADIF) {
        pComponentPrivate->File_Format = 1;
            OMX_PRDSP2(pComponentPrivate->dbg, "OMX_AUDIO_AACStreamFormatADIF \n");
    }
    else if((pComponentPrivate->aacParams[OUTPUT_PORT]->eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP4ADTS) ||
            (pComponentPrivate->aacParams[OUTPUT_PORT]->eAACStreamFormat == OMX_AUDIO_AACStreamFormatMP2ADTS) )
    {
        pComponentPrivate->File_Format = 2;
            OMX_PRDSP2(pComponentPrivate->dbg, "OMX_AUDIO_AACStreamFormatMP2ADTS \n");
    }

    if(pComponentPrivate->dasfmode == 1) {
        OMX_PRDSP2(pComponentPrivate->dbg, "%d :: AAC ENCODER RUNNING UNDER DASF MODE\n",__LINE__);
        
        arr[0]  = 2;                                                      /*Number of Streams*/
        arr[1]  = 0;                                                      /*ID of the Input Stream*/
        arr[2]  = 1;                                                      /*Type of Input Stream*/
        arr[3]  = NUM_AACENC_INPUT_BUFFERS_DASF;                          /*Number of buffers for Input Stream*/
        arr[4]  = 1;                                                      /*ID of the Output Stream*/
        arr[5]  = 0;                                                      /*Type of Output Stream*/ 
        arr[6]  = 2;                                                      /*Number of buffers for Output Stream*/
        arr[7]  = 1;                                                      /*PNS Enable*/ 
        arr[8]  = 1;                                                      /*TNS Enable*/
        arr[9]  = Channels;                                               /*Number of Channels*/
        arr[10] = pComponentPrivate->ulSamplingRate;                      /*Sampling rate- Lower bits*/
        arr[11] = HigherBitsSamplingRate;                                 /*Sampling rate -Higher bits */
        arr[12] = (OMX_U16)(pComponentPrivate->unBitrate & 0xFFFF);       /*Bit rate 2bytes*/ 
        arr[13] = (OMX_U16)(pComponentPrivate->unBitrate >> 16);          /*Bit rate 2bytes*/
        arr[14] = (OMX_U16)pComponentPrivate->nObjectType;                /*Object Type */
        arr[15] = (OMX_U16)pComponentPrivate->bitRateMode;                /*bitrateMode*/
        arr[16] = (OMX_U16)pComponentPrivate->File_Format;                /*FileFormat*/
        arr[17] = FramesPerOutBuf;                                        /*FramesPerOutBuf*/
        arr[18] = END_OF_CR_PHASE_ARGS;
        
    }

    if(pComponentPrivate->dasfmode == 0) {
        OMX_PRDSP2(pComponentPrivate->dbg, "%d :: AAC ENCODER RUNNING UNDER FILE MODE\n",__LINE__);
        arr[0]  = 2;                                                      /*Number of Streams*/
        arr[1]  = 0;                                                      /*ID of the Input Stream*/
        arr[2]  = 0;                                                      /*Type of Input Stream*/
        if (pComponentPrivate->pInputBufferList->numBuffers) {
            arr[3] = (OMX_U16) pComponentPrivate->pInputBufferList->numBuffers;     /*Number of buffers for Input Stream*/
            OMX_PRBUFFER1(pComponentPrivate->dbg, "arr[3] InputBuffers %d \n",arr[3]);
        }
        else {
            arr[3] = 1;
        }
        arr[4]  = 1;                                                      /*ID of the Output Stream*/
        arr[5]  = 0;                                                      /*Type of Output Stream*/ 
        if (pComponentPrivate->pOutputBufferList->numBuffers) {
            arr[6] = (OMX_U16) pComponentPrivate->pOutputBufferList->numBuffers;    /*Number of buffers for Output Stream*/
            OMX_PRBUFFER1(pComponentPrivate->dbg, "arr[6] Output Buffers%d \n",arr[6]);
        }
        else {
            arr[6] = 1;
        }
        
        arr[7]  = 1;                                                      /*PNS Enable*/ 
        arr[8]  = 1;                                                      /*TNS Enable*/
        /*  Adjusting the value for SN enum-type compatibility */
        arr[9]  = Channels;                                               /*Number of Channels*/
            OMX_PRCOMM2(pComponentPrivate->dbg, "arr[9] Channels %d \n",arr[9]);
        arr[10] = pComponentPrivate->ulSamplingRate;                      /*Sampling rate- Lower bits*/
        arr[11] = HigherBitsSamplingRate;                                 /*Sampling rate -Higher bits */
        arr[12] = (OMX_U16)(pComponentPrivate->unBitrate & 0xFFFF);       /*Bit rate 2bytes*/ 
        arr[13] = (OMX_U16)(pComponentPrivate->unBitrate >> 16);          /*Bit rate 2bytes*/
        arr[14] = (OMX_U16)pComponentPrivate->nObjectType;                                        /*bitsperSample;*/
            OMX_PRINT2(pComponentPrivate->dbg, "arr[14] Object Type %d \n",arr[14]);
        arr[15] = (OMX_U16)pComponentPrivate->bitRateMode;                /*bitrateMode*/
            OMX_PRINT2(pComponentPrivate->dbg, "arr[15] Bit Rate %d \n",arr[15]);
        arr[16] = pComponentPrivate->File_Format;                         /*FileFormat*/
            OMX_PRINT2(pComponentPrivate->dbg, "arr[16] format type %d \n",arr[16]);
        arr[17] = FramesPerOutBuf;                                        /*FramesPerOutBuf*/
        arr[18] = END_OF_CR_PHASE_ARGS;
        
    }

    plcml_Init->pCrPhArgs = arr;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Comp: OMX_AacEncUtils.c\n",__LINE__);
    size_lcml = nIpBuf * sizeof(LCML_AACENC_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(ptr, size_lcml,char);
    pTemp_lcml = (LCML_AACENC_BUFHEADERTYPE *)ptr;
    pComponentPrivate->pLcmlBufHeader[INPUT_PORT] = pTemp_lcml;
    for (i=0; i<nIpBuf; i++) 
    {
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nIpBufSize;
        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = AACENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = AACENC_MINOR_VER; 
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirInput;

        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pIpParam,
                               sizeof(AACENC_UAlgInBufParamStruct),
                               AACENC_UAlgInBufParamStruct);
        if (pTemp_lcml->pIpParam == NULL) {

            pTemp_lcml = pComponentPrivate->pLcmlBufHeader[INPUT_PORT];

            for(k=0;k<i;k++) {
                OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pIpParam, AACENC_UAlgInBufParamStruct);
                pTemp_lcml++;
            }
            OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);

            if(pComponentPrivate->dasfmode == 1) {
                OMX_MEMFREE_STRUCT(pComponentPrivate->strmAttr);
            }

            OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Exiting Fill_LCMLInitParams\n",__LINE__);
            return OMX_ErrorInsufficientResources;
        }
        OMX_PRDSP2(pComponentPrivate->dbg, "pTemp_lcml->pIpParam %p \n",pTemp_lcml->pIpParam);
        
        pTemp_lcml->pIpParam->bLastBuffer = 0;
        /* This means, it is not a last buffer. This flag is to be modified by
                * the application to indicate the last buffer */
        pTemp->nFlags = NORMAL_BUFFER;
        pTemp_lcml++;
    }

    /* Allocate memory for all output buffer headers,  This memory pointer will be sent to LCML */
    size_lcml = nOpBuf * sizeof(LCML_AACENC_BUFHEADERTYPE);

    OMX_MALLOC_SIZE(ptr, size_lcml,char);
    pTemp_lcml = (LCML_AACENC_BUFHEADERTYPE *)ptr;

    pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT] = pTemp_lcml;
    for (j=0; j<nOpBuf; j++)
    {
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[j];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nOpBufSize;
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = AACENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = AACENC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        /* This means, it is not a last buffer. This flag is to be modified by
                    the application to indicate the last buffer */
        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirOutput;
        /* SN : Each output buffer may be accompanied by an output buffer parameters structure*/ 

        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pOpParam, 
                               sizeof(AACENC_UAlgOutBufParamStruct),
                               AACENC_UAlgOutBufParamStruct);
        if (pTemp_lcml->pOpParam == NULL) {

            pTemp_lcml = pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT];

            for(k=0;k<j;k++) {
                OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pOpParam, AACENC_UAlgOutBufParamStruct);
                pTemp_lcml++;
            }
            OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT]);

            pTemp_lcml = pComponentPrivate->pLcmlBufHeader[INPUT_PORT];

            for(k=0;k<i;k++) {
                OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pIpParam, AACENC_UAlgInBufParamStruct);
                pTemp_lcml++;
            }
            OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);

            if(pComponentPrivate->dasfmode == 1) {
                OMX_MEMFREE_STRUCT(pComponentPrivate->strmAttr);
            }

            OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Exiting Fill_LCMLInitParams\n",__LINE__);
            return OMX_ErrorInsufficientResources;
        }


        OMX_PRDSP1(pComponentPrivate->dbg, "%d :: UTIL: size of pOpParam: %d \n",__LINE__,sizeof(pTemp_lcml->pOpParam->unFrameSizes));
        OMX_PRDSP1(pComponentPrivate->dbg, "%d :: UTIL: numframes of pOpParam: %d \n\n",__LINE__,sizeof(pTemp_lcml->pOpParam->unNumFramesEncoded)) ;
        OMX_PRDSP1(pComponentPrivate->dbg, "UTIL: pTemp_lcml->pOpParam %p \n",pTemp_lcml->pOpParam);
        
        pTemp->nFlags = NORMAL_BUFFER;
        /*pTemp++;*/
        pTemp_lcml++;
    }
    pComponentPrivate->bPortDefsAllocated = 1;
    pComponentPrivate->bBypassDSP = 0;
    pComponentPrivate->bNoIdleOnStop= OMX_FALSE;


    OMX_MALLOC_SIZE_DSPALIGN(pComponentPrivate->ptAlgDynParams,
                                sizeof(MPEG4AACENC_UALGParams),
                                MPEG4AACENC_UALGParams);
    if (pComponentPrivate->ptAlgDynParams == NULL) {
        pTemp_lcml = pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT];

        for(k=0;k<j;k++) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pOpParam, AACENC_UAlgOutBufParamStruct);
            pTemp_lcml++;
        }
        OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT]);

        pTemp_lcml = pComponentPrivate->pLcmlBufHeader[INPUT_PORT];

        for(k=0;k<i;k++) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pIpParam, AACENC_UAlgInBufParamStruct);
            pTemp_lcml++;
        }
        OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);

        if(pComponentPrivate->dasfmode == 1) {
            OMX_MEMFREE_STRUCT(pComponentPrivate->strmAttr);
        }
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Exiting Fill_LCMLInitParams\n",__LINE__);
        return OMX_ErrorInsufficientResources;
    }

    OMX_PRINT2(pComponentPrivate->dbg, "UTIL: pComponentPrivate->ptAlgDynParams %p \n",pComponentPrivate->ptAlgDynParams);

#ifdef __PERF_INSTRUMENTATION__
        pComponentPrivate->nLcml_nCntIp = 0;
        pComponentPrivate->nLcml_nCntOpReceived = 0;
#endif


EXIT:
    if (eError != OMX_ErrorNone) {
        OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->ptAlgDynParams, MPEG4AACENC_UALGParams);
        pTemp_lcml = pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT];

        for(k=0;k<j;k++) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pOpParam, AACENC_UAlgOutBufParamStruct);
            pTemp_lcml++;
        }
        OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT]);

        pTemp_lcml = pComponentPrivate->pLcmlBufHeader[INPUT_PORT];

        for(k=0;k<i;k++) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pIpParam, AACENC_UAlgInBufParamStruct);
            pTemp_lcml++;
        }
        OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);

        if(pComponentPrivate->dasfmode == 1) {
            OMX_MEMFREE_STRUCT(pComponentPrivate->strmAttr);
        }
    }

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Exiting Fill_LCMLInitParams\n",__LINE__);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
* AACENC_StartComponentThread() This function is called by the component to create
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

OMX_ERRORTYPE AACENC_StartComponentThread(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    AACENC_COMPONENT_PRIVATE *pComponentPrivate = (AACENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering  AACENC_StartComponentThread\n", __LINE__);
    /* Initialize all the variables*/
    pComponentPrivate->bIsStopping = 0;
    pComponentPrivate->bIsThreadstop = 0;
    pComponentPrivate->lcml_nOpBuf = 0;
    pComponentPrivate->lcml_nIpBuf = 0;         /* Initializing counter */
    pComponentPrivate->app_nBuf = 0;                    /* Not Used */
    pComponentPrivate->num_Op_Issued = 0;
    pComponentPrivate->num_Sent_Ip_Buff = 0;
    pComponentPrivate->num_Reclaimed_Op_Buff = 0;       /* Not Used */
    pComponentPrivate->bIsEOFSent = 0;
    
    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->cmdDataPipe);
    if (eError) 
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Inside  AACENC_StartComponentThread\n", __LINE__);
        eError = OMX_ErrorInsufficientResources;
        pComponentPrivate->cbInfo.EventHandler(pHandle,
                pHandle->pApplicationPrivate,
                OMX_EventError,
                eError,
                OMX_TI_ErrorSevere,
                NULL);

        return eError;
    }
    
    /* create the pipe used to send buffers to the thread */
    eError = pipe (pComponentPrivate->dataPipe);
    if (eError) 
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Inside  AACENC_StartComponentThread\n", __LINE__);
        close(pComponentPrivate->cmdDataPipe[0]);
        close(pComponentPrivate->cmdDataPipe[1]);
        eError = OMX_ErrorInsufficientResources;
        pComponentPrivate->cbInfo.EventHandler(pHandle,
                pHandle->pApplicationPrivate,
                OMX_EventError,
                eError,
                OMX_TI_ErrorSevere,
                NULL);

        return eError;
    }

    /* create the pipe used to send commands to the thread */
    eError = pipe (pComponentPrivate->cmdPipe);
    if (eError) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Inside  AACENC_StartComponentThread\n", __LINE__);
        close(pComponentPrivate->cmdDataPipe[0]);
        close(pComponentPrivate->cmdDataPipe[1]);
        close(pComponentPrivate->dataPipe[0]);
        close(pComponentPrivate->dataPipe[1]);
        eError = OMX_ErrorInsufficientResources;
        pComponentPrivate->cbInfo.EventHandler(pHandle,
                pHandle->pApplicationPrivate,
                OMX_EventError,
                eError,
                OMX_TI_ErrorSevere,
                NULL);
        return eError;
    }

    /* create the pipe used to send commands to the thread */
    eError = pthread_create (&(pComponentPrivate->ComponentThread), NULL, AACENC_ComponentThread, pComponentPrivate);
    if (eError || !pComponentPrivate->ComponentThread) {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Inside  AACENC_StartComponentThread\n", __LINE__);
        close(pComponentPrivate->cmdDataPipe[0]);
        close(pComponentPrivate->cmdDataPipe[1]);
        close(pComponentPrivate->dataPipe[0]);
        close(pComponentPrivate->dataPipe[1]);
        close(pComponentPrivate->cmdPipe[0]);
        close(pComponentPrivate->cmdPipe[1]);
        eError = OMX_ErrorInsufficientResources;
        pComponentPrivate->cbInfo.EventHandler(pHandle,
                pHandle->pApplicationPrivate,
                OMX_EventError,
                eError,
                OMX_TI_ErrorSevere,
                NULL);
        return eError;
    }

    OMX_PRDSP1(pComponentPrivate->dbg, "%d :: pComponent[%x] AACENC_StartComponentThread\n", __LINE__, (int)pComponent) ; 
    OMX_PRDSP1(pComponentPrivate->dbg, "%d :: pHandle[%x] AACENC_StartComponentThread\n", __LINE__, (int)pHandle) ; 
    pComponentPrivate->bCompThreadStarted = 1;
    
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting from AACENC_StartComponentThread\n", __LINE__);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
* AACENC_FreeCompResources() This function is called by the component during
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

OMX_ERRORTYPE AACENC_FreeCompResources(OMX_HANDLETYPE pComponent)
{
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    AACENC_COMPONENT_PRIVATE *pComponentPrivate = (AACENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE err = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0;
    OMX_U32 nOpBuf = 0;

    OMX_PRINT1(pComponentPrivate->dbg, " %d :: Entering AACENC_FreeCompResources\n",__LINE__);
    if (pComponentPrivate->bPortDefsAllocated) {
        nIpBuf = pComponentPrivate->pPortDef[INPUT_PORT]->nBufferCountActual;
        nOpBuf = pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferCountActual;
    }

    if (pComponentPrivate->bCompThreadStarted) {
        OMX_CLOSE_PIPE(pComponentPrivate->dataPipe[0],err);
        OMX_CLOSE_PIPE(pComponentPrivate->dataPipe[1],err);
        OMX_CLOSE_PIPE(pComponentPrivate->cmdPipe[0],err);
        OMX_CLOSE_PIPE(pComponentPrivate->cmdPipe[1],err);
        OMX_CLOSE_PIPE(pComponentPrivate->cmdDataPipe[0],err);
        OMX_CLOSE_PIPE(pComponentPrivate->cmdDataPipe[1],err);
    }

    if (pComponentPrivate->bPortDefsAllocated) {
        OMX_MEMFREE_STRUCT(pComponentPrivate->pPortDef[INPUT_PORT]);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pPortDef[OUTPUT_PORT]);
        OMX_MEMFREE_STRUCT(pComponentPrivate->aacParams[INPUT_PORT]);
        OMX_MEMFREE_STRUCT(pComponentPrivate->aacParams[OUTPUT_PORT]);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pcmParam[INPUT_PORT]);
        OMX_MEMFREE_STRUCT(pComponentPrivate->pcmParam[OUTPUT_PORT]);
    }
    pComponentPrivate->bPortDefsAllocated = 0;

    OMX_PRDSP1(pComponentPrivate->dbg, "\n\n FreeCompResources: Destroying mutexes.\n\n");
    pthread_mutex_destroy(&pComponentPrivate->InLoaded_mutex);
    pthread_cond_destroy(&pComponentPrivate->InLoaded_threshold);
    
    pthread_mutex_destroy(&pComponentPrivate->InIdle_mutex);
    pthread_cond_destroy(&pComponentPrivate->InIdle_threshold);
    
    pthread_mutex_destroy(&pComponentPrivate->AlloBuf_mutex);
    pthread_cond_destroy(&pComponentPrivate->AlloBuf_threshold);

    pthread_mutex_destroy(&pComponentPrivate->bufferReturned_mutex);
    pthread_cond_destroy(&pComponentPrivate->bufferReturned_condition);


    OMX_MEMFREE_STRUCT(pComponentPrivate->pInputBufferList);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pOutputBufferList);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: AACENC: After AACENC_FreeCompResources\n",__LINE__);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: AACENC: [FREE] %p\n",__LINE__,pComponentPrivate);

    OMX_MEMFREE_STRUCT(pComponentPrivate->sDeviceString);

    if (pComponentPrivate->pLcmlHandle != NULL) {
        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle)->pCodecinterfacehandle,
                                                     EMMCodecControlDestroy, NULL);
        OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: AACENC: After CodecControlDestroy \n",__LINE__);
        if (eError != OMX_ErrorNone)
        {
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: LCML_ControlCodec EMMCodecControlDestroy: no.  %x\n",__LINE__, eError);
            pComponentPrivate->cbInfo.EventHandler (pHandle,
                                    pHandle->pApplicationPrivate,
                                    OMX_EventError,
                                    eError,
                                    OMX_TI_ErrorSevere,
                                    NULL);
            return eError;
        }
        pComponentPrivate->bCodecDestroyed = OMX_TRUE;
        pComponentPrivate->pLcmlHandle = NULL;
    }

    /* CLose LCML .      - Note:  Need to handle better - */
    if ((pComponentPrivate->ptrLibLCML != NULL && pComponentPrivate->bGotLCML) &&
        (pComponentPrivate->bCodecDestroyed))
    {
        OMX_PRDSP2(pComponentPrivate->dbg, "AACENC: About to Close LCML %p \n",pComponentPrivate->ptrLibLCML);
        dlclose( pComponentPrivate->ptrLibLCML);
        pComponentPrivate->ptrLibLCML = NULL;
        OMX_PRDSP2(pComponentPrivate->dbg, "AACENC: Closed LCML \n");

        pComponentPrivate->bCodecDestroyed = OMX_FALSE;     /* restoring flags */
        pComponentPrivate->bGotLCML        = OMX_FALSE;
    }


EXIT:
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting AACENC_FreeCompResources()\n",__LINE__);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Returning = 0x%x\n",__LINE__,eError);

    return eError;
}

/* ========================================================================== */
/**
* @AACENC_CleanupInitParams() This function is called by the component during
* de-init to free structues that are been allocated at intialization stage
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
OMX_ERRORTYPE AACENC_CleanupInitParams(OMX_HANDLETYPE pComponent)
{
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    AACENC_COMPONENT_PRIVATE *pComponentPrivate = (AACENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;
    LCML_AACENC_BUFHEADERTYPE *pTemp_lcml=NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf = 0;
    OMX_U32 nOpBuf = 0;
    OMX_U32 i      = 0;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering AACENC_CleanupInitParams()\n", __LINE__);
    OMX_MEMFREE_STRUCT(pComponentPrivate->strmAttr);

    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[INPUT_PORT];
    nIpBuf = pComponentPrivate->nRuntimeInputBuffers;
    for(i=0; i<nIpBuf; i++) 
    {
        OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pIpParam, AACENC_UAlgInBufParamStruct);
        pTemp_lcml++;
    }

    /*Free ouput buffers params */
    pTemp_lcml = pComponentPrivate->pLcmlBufHeader[OUTPUT_BUFFER];
    nOpBuf = pComponentPrivate->nRuntimeOutputBuffers;
    for (i=0; i<nOpBuf; i++)
    {
        OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pOpParam, AACENC_UAlgOutBufParamStruct);
        pTemp_lcml++;
    }

    OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);
    OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT]);
    OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->ptAlgDynParams, MPEG4AACENC_UALGParams);
    if(pComponentPrivate->dasfmode == 1) 
    {
        OMX_MEMFREE_STRUCT_DSPALIGN(pComponentPrivate->pParams, AACENC_AudioCodecParams);
    }

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting Successfully AACENC_CleanupInitParams()\n",__LINE__);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}

/* ========================================================================== */
/**
* AACENC_StopComponentThread() This function is called by the component during
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

OMX_ERRORTYPE AACENC_StopComponentThread(OMX_HANDLETYPE pComponent)
{
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)pComponent;
    AACENC_COMPONENT_PRIVATE *pComponentPrivate = (AACENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_ERRORTYPE threadError = OMX_ErrorNone;
    int pthreadError = 0;

    OMX_PRINT1(pComponentPrivate->dbg, " %d :: UTIL: Entering AACENC_StopComponentThread\n",__LINE__);
    /*Join the component thread*/
    pComponentPrivate->bIsThreadstop = 1;
    write (pComponentPrivate->cmdPipe[1], &pComponentPrivate->bIsThreadstop, sizeof(OMX_U16));
    OMX_PRSTATE1(pComponentPrivate->dbg, "UTIL: pComponentPrivate->bIsThreadstop = %ld \n",pComponentPrivate->bIsThreadstop);
    pthreadError = pthread_join (pComponentPrivate->ComponentThread,(void*)&threadError);
    if (0 != pthreadError) 
    {
        eError = OMX_ErrorHardware;
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error closing ComponentThread - pthreadError = %d\n",__LINE__,pthreadError);
        return eError;
    }

    /*Check for the errors*/
    if (OMX_ErrorNone != threadError && OMX_ErrorNone != eError) 
    {
        eError = OMX_ErrorInsufficientResources;
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error while closing Component Thread\n",__LINE__);
        return eError;
    }
    OMX_PRINT1(pComponentPrivate->dbg, " %d :: UTIL: Exiting AACENC_StopComponentThread\n",__LINE__);
    return eError;
}


/* ========================================================================== */
/**
* AACENCHandleCommand() This function is called by the component when ever it
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

OMX_U32 AACENCHandleCommand(AACENC_COMPONENT_PRIVATE *pComponentPrivate)
{

    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *) pComponentPrivate->pHandle;
    OMX_COMMANDTYPE command;
    OMX_STATETYPE commandedState;
    OMX_U32 commandData;
    OMX_HANDLETYPE pLcmlHandle = pComponentPrivate->pLcmlHandle;
    OMX_ERRORTYPE rm_error = OMX_ErrorNone;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U16 arr[100]={0};
    OMX_S32 ret = 0;
    OMX_U32 i=0;
    char *p  = "";
    char *pArgs = "";
    OMX_U32 pValues[4]={0};
    OMX_U32 pValues1[4]={0};
    LCML_CALLBACKTYPE cb;
    LCML_DSP *pLcmlDsp = NULL;
    LCML_AACENC_BUFHEADERTYPE *pLcmlHdr;
    int inputPortFlag = 0;
    int outputPortFlag = 0;

    OMX_PRSTATE1(pComponentPrivate->dbg, "%d :: AACENC: Entering AACENCHandleCommand Function - curState = %d\n",__LINE__,pComponentPrivate->curState);
    ret = read (pComponentPrivate->cmdPipe[0], &command, sizeof (command));
    OMX_TRACE1(pComponentPrivate->dbg, "%d :: AACENC: Command pipe has been read = %ld \n",__LINE__,ret);
    if (ret == -1) 
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error in Reading from the Data pipe\n", __LINE__);
        eError = OMX_ErrorHardware;
        pComponentPrivate->cbInfo.EventHandler (pHandle,
                pHandle->pApplicationPrivate,
                OMX_EventError,
                eError,
                OMX_TI_ErrorSevere,
                NULL);
        return eError;
    }
    ret = read (pComponentPrivate->cmdDataPipe[0], &commandData, sizeof (commandData));
    OMX_TRACE1(pComponentPrivate->dbg, "%d :: AACENC: Command data pipe has been read = %ld \n",__LINE__,ret);
    if (ret == -1) 
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error in Reading from the Data pipe\n", __LINE__);
        eError = OMX_ErrorHardware;
        pComponentPrivate->cbInfo.EventHandler (pHandle,
                pHandle->pApplicationPrivate,
                OMX_EventError,
                eError,
                OMX_TI_ErrorSevere,
                NULL);
        return eError;
    }

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedCommand(pComponentPrivate->pPERFcomp,command,commandData,PERF_ModuleLLMM);
#endif

    OMX_PRDSP2(pComponentPrivate->dbg, "%d :: AACENCHandleCommand :: Command is = %d\n",__LINE__,command);
    if (command == OMX_CommandStateSet) 
    {
        commandedState = (OMX_STATETYPE)commandData;
        if ( pComponentPrivate->curState==commandedState)
        {
            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                   pHandle->pApplicationPrivate,
                                                   OMX_EventError, 
                                                   OMX_ErrorSameState,
                                                   OMX_TI_ErrorMinor, 
                                                   NULL);
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Same State Given by Application\n",__LINE__);   
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: AACENC: State Given is: %d \n", __LINE__, commandedState);
        }
        else
        {
            switch(commandedState) 
            {
                case OMX_StateIdle:
                    OMX_PRDSP2(pComponentPrivate->dbg, "%d: AACENCHandleCommand: Cmd Idle \n",__LINE__);
                    OMX_PRDSP2(pComponentPrivate->dbg, "AACENC: curstate = %d\n",pComponentPrivate->curState);
                    if (pComponentPrivate->curState == OMX_StateLoaded) 
                    {

#ifdef __PERF_INSTRUMENTATION__
                        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundarySetup);
#endif
                        if(pComponentPrivate->dasfmode == 1) 
                        {
                            if (pComponentPrivate->dasfmode == 1) 
                            {
                                pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled= FALSE;
                                pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated= FALSE;
                            }
                            if (pComponentPrivate->streamID==0)
                            {
                                
                                OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Insufficient resources\n", __LINE__);
                                eError = OMX_ErrorInsufficientResources;
                                OMX_ERROR4(pComponentPrivate->dbg, "AACENC: State changed to OMX_StateInvalid Line %d\n",__LINE__);
                                pComponentPrivate->curState = OMX_StateInvalid;
                                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                                        pHandle->pApplicationPrivate,
                                                                        OMX_EventError, 
                                                                        eError,
                                                                        OMX_TI_ErrorMajor,
                                                                        "No Stream ID Available");
                                return eError;
                            }
                        }
                    
                        OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: OMX_StateLoaded [INPUT_PORT]->bPopulated  %d \n",__LINE__,pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated);
                        OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: OMX_StateLoaded [INPUT_PORT]->bEnabled    %d \n",__LINE__,pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled);
                        OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: OMX_StateLoaded [OUTPUT_PORT]->bPopulated %d \n",__LINE__,pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated);
                        OMX_PRCOMM2(pComponentPrivate->dbg, "%d :: OMX_StateLoaded [OUTPUT_PORT]->bEnabled   %d \n",__LINE__,pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled);

                        if (pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated &&  pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled)  {
                            inputPortFlag = 1;
                        }

                        if (!pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated && !pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled) {
                            inputPortFlag = 1;
                        }

                        if (pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated && pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled) {
                            outputPortFlag = 1;
                        }

                        if (!pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated && !pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled) {
                            outputPortFlag = 1;
                        }

                        OMX_PRCOMM2(pComponentPrivate->dbg, "inputPortFlag = %d\n",inputPortFlag);
                        OMX_PRCOMM2(pComponentPrivate->dbg, "outputPortFlag = %d\n",outputPortFlag);
                        if (!(inputPortFlag && outputPortFlag)) 
                        {
                            omx_mutex_wait(&pComponentPrivate->InLoaded_mutex,&pComponentPrivate->InLoaded_threshold,
                                           &pComponentPrivate->InLoaded_readytoidle);
                        }

                        cb.LCML_Callback = (void *) AACENCLCML_Callback;
                        pLcmlHandle = (OMX_HANDLETYPE) AACENCGetLCMLHandle(pComponentPrivate);
                    
                        if (pLcmlHandle == NULL) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: AACENC: LCML Handle is NULL........exiting..\n",__LINE__);
                            eError = OMX_ErrorHardware;
                            pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
                            return eError;
                        }

                        /* Got handle of dsp via phandle filling information about DSP
                        specific things */
                        pLcmlDsp = (((LCML_DSP_INTERFACE*)pLcmlHandle)->dspCodec);
                        eError = AACENCFill_LCMLInitParams(pHandle, pLcmlDsp, arr);
                        if(eError != OMX_ErrorNone) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error returned from AACENCFill_LCMLInitParams()\n",__LINE__);
                            pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorMajor,
                                                NULL);
                            return eError;
                        }
                        pComponentPrivate->pLcmlHandle = (LCML_DSP_INTERFACE *)pLcmlHandle;
                        /*filling create phase params */
                        cb.LCML_Callback = (void *) AACENCLCML_Callback;
                        OMX_PRDSP2(pComponentPrivate->dbg, "%d :: AACENC: Calling LCML_InitMMCodec...\n",__LINE__);

                        eError = LCML_InitMMCodecEx(((LCML_DSP_INTERFACE *)pLcmlHandle)->pCodecinterfacehandle,
                                                       p,&pLcmlHandle,(void *)p,&cb, (OMX_STRING)pComponentPrivate->sDeviceString);
                        OMX_PRDSP2(pComponentPrivate->dbg, "%d :: AACENC: After Calling LCML_InitMMCodec...\n",__LINE__);

                        if(eError != OMX_ErrorNone) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error returned from LCML_Init()\n",__LINE__);
                            /* send an event to client */
                            eError = OMX_ErrorInvalidState;
                            /* client should unload the component if the codec is not able to load */
                            pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
                            return eError;
                        }

#ifdef HASHINGENABLE
                        /* Enable the Hashing Code */
                        eError = LCML_SetHashingState(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle, OMX_TRUE);
                        if (eError != OMX_ErrorNone) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "Error: Failed to set Mapping State\n");
                            goto EXIT;
                        }
#endif                  
                        /* need check the resource with RM */
                        OMX_PRINT2(pComponentPrivate->dbg, "%d :: AACENC: About to call RMProxy_SendCommand\n", __LINE__);
#ifdef RESOURCE_MANAGER_ENABLED


                        OMX_PRDSP1(pComponentPrivate->dbg, "%d :: AACENC: Returned from RMProxy_SendCommand\n", __LINE__);
                        OMX_PRDSP1(pComponentPrivate->dbg, "%d :: AACENC: RMProxy_SendCommand returned %d\n", __LINE__,rm_error);
                        if(rm_error == OMX_ErrorNone) 
                        {
                            /* resource is available */
                            pComponentPrivate->curState = OMX_StateIdle;

#ifdef __PERF_INSTRUMENTATION__
                            PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySetup);
#endif
                            rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_AAC_Encoder_COMPONENT, OMX_StateIdle, 3456, NULL);
                        }
                        else if(rm_error == OMX_ErrorInsufficientResources) 
                        {
                            /* resource is not available, need set state to OMX_StateWaitForResources */
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Insufficient resources\n", __LINE__);
                            pComponentPrivate->curState = OMX_StateWaitForResources;
                            pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                                   pHandle->pApplicationPrivate,
                                                                   OMX_EventCmdComplete, 
                                                                   OMX_CommandStateSet, 
                                                                   pComponentPrivate->curState, 
                                                                   NULL);
                            OMX_ERROR2(pComponentPrivate->dbg, "%d :: AACENC: OMX_ErrorInsufficientResources\n", __LINE__);
                        }
                        pComponentPrivate->curState = OMX_StateIdle;
                        pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                               pHandle->pApplicationPrivate,
                                                               OMX_EventCmdComplete, 
                                                               OMX_CommandStateSet,
                                                               pComponentPrivate->curState, 
                                                               NULL);
#else           
                pComponentPrivate->curState = OMX_StateIdle;
                pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete,
                                                        OMX_CommandStateSet,
                                                        pComponentPrivate->curState,
                                                        NULL);


#endif
                    } 
                    else if (pComponentPrivate->curState == OMX_StateExecuting) 
                    {

#ifdef __PERF_INSTRUMENTATION__
                        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif

                        OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: AACENC: Setting Component to OMX_StateIdle\n",__LINE__);
                        OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: AACENC: About to Call MMCodecControlStop\n", __LINE__);

                        pComponentPrivate->bIsStopping = 1;

                        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                      MMCodecControlStop,(void *)pArgs);
                        if (OMX_ErrorNone != eError) {
                            AACENC_FatalErrorRecover(pComponentPrivate);
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error from LCML_ControlCodec EMMCodecControlStop = %x\n",__LINE__,eError);
                            return eError;
                        }
                        omx_mutex_wait(&pComponentPrivate->codecStop_mutex,&pComponentPrivate->codecStop_threshold,
                                       &pComponentPrivate->codecStop_waitingsignal);

                        pComponentPrivate->nNumOutputBufPending=0;

#ifdef HASHINGENABLE
                        /*Hashing Change*/
                        pLcmlHandle = (LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle;
                        eError = LCML_FlushHashes(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle);
                        if (eError != OMX_ErrorNone) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "Error occurred in Codec mapping flush!\n");
                            break;
                        }
#endif



                    }
                    else if(pComponentPrivate->curState == OMX_StatePause) 
                    {
#ifdef HASHINGENABLE
                        /*Hashing Change*/
                        pLcmlHandle = (LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle;
                        eError = LCML_FlushHashes(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle);
                        if (eError != OMX_ErrorNone) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "Error occurred in Codec mapping flush!\n");
                            break;
                        }
#endif

#ifdef __PERF_INSTRUMENTATION__
                        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif
                        pComponentPrivate->curState = OMX_StateIdle;        
                
#ifdef RESOURCE_MANAGER_ENABLED
            
                        rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_AAC_Encoder_COMPONENT, OMX_StateIdle, 3456, NULL); 
#endif
                        pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                               pHandle->pApplicationPrivate,
                                                               OMX_EventCmdComplete,
                                                               OMX_CommandStateSet,
                                                               pComponentPrivate->curState,
                                                               NULL);
                    }
                    else 
                    {
                        /* This means, it is invalid state from application */
                        OMX_ERROR4(pComponentPrivate->dbg, "%d :: AACENC: OMX_ErrorIncorrectStateTransition\n",__LINE__);
                        pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                               pHandle->pApplicationPrivate,
                                                               OMX_EventError,
                                                               OMX_ErrorIncorrectStateTransition,
                                                               OMX_TI_ErrorMinor,
                                                               "Invalid State Error");
                    }
                    break;


                case OMX_StateExecuting:
                    OMX_PRDSP2(pComponentPrivate->dbg, "%d :: AACENCHandleCommand: Cmd Executing \n",__LINE__);
                    if(pComponentPrivate->curState == OMX_StateIdle) 
                    {
                        /* Sending commands to DSP via LCML_ControlCodec third argument is not used for time being */
                        pComponentPrivate->bIsStopping = 0;
                        if(pComponentPrivate->dasfmode == 1) 
                        {
                            OMX_PRDSP2(pComponentPrivate->dbg, "%d :: ---- Comp: DASF Functionality is ON ---\n",__LINE__);

                            OMX_MALLOC_SIZE_DSPALIGN(pComponentPrivate->pParams,
                                sizeof(AACENC_AudioCodecParams),
                                AACENC_AudioCodecParams);
                            if (pComponentPrivate->pParams != NULL) {
                                pComponentPrivate->cbInfo.EventHandler (pHandle,
                                        pHandle->pApplicationPrivate,
                                        OMX_EventError,
                                        OMX_ErrorInsufficientResources,
                                        OMX_TI_ErrorSevere,
                                        NULL);
                                return OMX_ErrorInsufficientResources;
                            }

                            OMX_PRDSP2(pComponentPrivate->dbg, "AACENC: pComponentPrivate->pParams %p \n",pComponentPrivate->pParams);
                            if(pComponentPrivate->unNumChannels == 1)   /*MONO*/
                                pComponentPrivate->pParams->iAudioFormat = 0x0001;
                            else
                                pComponentPrivate->pParams->iAudioFormat = 0x0002;
                            
                            pComponentPrivate->pParams->iStrmId = pComponentPrivate->streamID;
                            pComponentPrivate->pParams->iSamplingRate = pComponentPrivate->ulSamplingRate;
                                                                        
                            pValues[0] = USN_STRMCMD_SETCODECPARAMS;
                            pValues[1] = (OMX_U32)pComponentPrivate->pParams;
                            pValues[2] = sizeof(AACENC_AudioCodecParams);
                            eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle, 
                                                         EMMCodecControlStrmCtrl,
                                                         (void *)pValues);
                            if(eError != OMX_ErrorNone) 
                            {
                                OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Occurred in Codec StreamControl..\n",__LINE__);
                                pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
                                return eError;
                            }
                        }

                        pComponentPrivate->ptAlgDynParams->audenc_dynamicparams.size                = sizeof(MPEG4AACENC_UALGParams);
                        pComponentPrivate->ptAlgDynParams->audenc_dynamicparams.bitRate             = pComponentPrivate->unBitrate;
                        pComponentPrivate->ptAlgDynParams->audenc_dynamicparams.sampleRate          = pComponentPrivate->ulSamplingRate;
                        pComponentPrivate->ptAlgDynParams->audenc_dynamicparams.numChannels         = (pComponentPrivate->unNumChannels==2)?1:0;   /* Reduced */
                        pComponentPrivate->ptAlgDynParams->useTns                                   = 1;
                        pComponentPrivate->ptAlgDynParams->usePns                                   = 1;
                        pComponentPrivate->ptAlgDynParams->outObjectType                            = pComponentPrivate->nObjectType;
                        pComponentPrivate->ptAlgDynParams->outFileFormat                            = pComponentPrivate->File_Format;

                        OMX_PRDSP1(pComponentPrivate->dbg, "AACENC: dynamicparams.bitRate %d \n", (int)pComponentPrivate->unBitrate);
                        OMX_PRDSP1(pComponentPrivate->dbg, "AACENC: dynamicparams.sampleRate %d \n ",pComponentPrivate->ulSamplingRate);
                        OMX_PRDSP1(pComponentPrivate->dbg, "AACENC: dynamicparams.numChannels %d \n", pComponentPrivate->unNumChannels);
                        OMX_PRDSP1(pComponentPrivate->dbg, "AACENC: ptAlgDynParams->outFileFormat %d \n",pComponentPrivate->File_Format);

                        pValues1[0] = IUALG_CMD_SETSTATUS;
                        pValues1[1] = (OMX_U32)pComponentPrivate->ptAlgDynParams;
                        pValues1[2] = sizeof(MPEG4AACENC_UALGParams);

                        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                      EMMCodecControlAlgCtrl,(void *)pValues1);
                        if(eError != OMX_ErrorNone) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Occurred in Codec StreamControl..\n",__LINE__);
                            pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
                            return eError;
                        }
                        OMX_PRDSP1(pComponentPrivate->dbg, "%d :: AACENC: Algcontrol has been sent to DSP\n",__LINE__);

                        eError = LCML_ControlCodec( ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                       EMMCodecControlStart,(void *)pArgs);
                        if(eError != OMX_ErrorNone) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Occurred in Codec Start..\n", __LINE__);
                            pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
                            return eError;
                        }
                }
                else if (pComponentPrivate->curState == OMX_StatePause) 
                {
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                  EMMCodecControlStart, (void *)pArgs);
                    if (eError != OMX_ErrorNone) 
                    {
                        OMX_ERROR4(pComponentPrivate->dbg, "%d:: Error: While Resuming the codec\n",__LINE__);
                        pComponentPrivate->cbInfo.EventHandler (pHandle, 
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
                        return eError;
                    }
                    for (i=0; i < pComponentPrivate->nNumInputBufPending; i++) 
                    {
                        if (pComponentPrivate->pInputBufHdrPending[i]!= NULL) 
                        {
                            OMX_PRINT2(pComponentPrivate->dbg, "i: %d \n",(int)i);
                                AACENCGetCorresponding_LCMLHeader(pComponentPrivate, pComponentPrivate->pInputBufHdrPending[i]->pBuffer, OMX_DirInput, &pLcmlHdr);
                                AACENC_SetPending(pComponentPrivate,pComponentPrivate->pInputBufHdrPending[i],OMX_DirInput,__LINE__);

                                eError = LCML_QueueBuffer( 
                                            ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                            EMMCodecInputBuffer,
                                            pComponentPrivate->pInputBufHdrPending[i]->pBuffer,
                                            pComponentPrivate->pInputBufHdrPending[i]->nAllocLen,
                                            pComponentPrivate->pInputBufHdrPending[i]->nFilledLen,
                                            (OMX_U8 *) pLcmlHdr->pIpParam,
                                            sizeof(AACENC_UAlgInBufParamStruct),
                                            NULL);
                               if(eError != OMX_ErrorNone){
                                   OMX_ERROR4(pComponentPrivate->dbg, "%d:: Error: LCML QUEUE BUFFER\n",__LINE__);
                                   AACENC_FatalErrorRecover(pComponentPrivate);
                                   return eError;
                               }
                        }
                    }
                    pComponentPrivate->nNumInputBufPending =0;

                    for (i=0; i < pComponentPrivate->nNumOutputBufPending; i++) 
                    {


                            if (pComponentPrivate->pOutputBufHdrPending[i]!= NULL)
                            {
                                AACENCGetCorresponding_LCMLHeader(pComponentPrivate, pComponentPrivate->pOutputBufHdrPending[i]->pBuffer, OMX_DirOutput, &pLcmlHdr);
                                AACENC_SetPending(pComponentPrivate,pComponentPrivate->pOutputBufHdrPending[i],OMX_DirOutput,__LINE__);
                            

                                eError = LCML_QueueBuffer( 
                                             ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                             EMMCodecOuputBuffer,
                                             pComponentPrivate->pOutputBufHdrPending[i]->pBuffer,
                                             pComponentPrivate->pOutputBufHdrPending[i]->nAllocLen,
                                             0,
                                             (OMX_U8 *) pLcmlHdr->pOpParam,
                                             sizeof(AACENC_UAlgOutBufParamStruct),
                                             NULL);
                                if(eError != OMX_ErrorNone){
                                   OMX_ERROR4(pComponentPrivate->dbg, "%d:: Error: LCML QUEUE BUFFER\n",__LINE__);
                                   AACENC_FatalErrorRecover(pComponentPrivate);
                                   return eError;
                               }
                            }
                    }
                    pComponentPrivate->nNumOutputBufPending = 0;

                    
                }
                else 
                {
                    pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                           pHandle->pApplicationPrivate,
                                                           OMX_EventError, 
                                                           OMX_ErrorIncorrectStateTransition,
                                                           OMX_TI_ErrorMinor,
                                                           "Invalid State Error");
                    OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Invalid State Given by Application\n",__LINE__);
                    return eError;
                }
                
#ifdef RESOURCE_MANAGER_ENABLED
                rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_AAC_Encoder_COMPONENT, OMX_StateExecuting, 3456, NULL);

#endif

pComponentPrivate->curState = OMX_StateExecuting; /* --- Transition to Executing --- */
                
#ifdef __PERF_INSTRUMENTATION__
                PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundarySteadyState);
#endif



                /*Send state change notificaiton to Application */
                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandStateSet,
                                                       pComponentPrivate->curState, 
                                                       NULL);
                break;


                case OMX_StateLoaded:
                        OMX_PRSTATE1(pComponentPrivate->dbg, "%d :: AACENC: AACENCHandleCommand: Cmd Loaded - curState = %d\n",__LINE__,pComponentPrivate->curState);
                        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: AACENC: pComponentPrivate->pInputBufferList->numBuffers = %d \n",__LINE__,pComponentPrivate->pInputBufferList->numBuffers);
                        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: AACENC: pComponentPrivate->pOutputBufferList->numBuffers = %d \n",__LINE__,pComponentPrivate->pOutputBufferList->numBuffers);
                        if (pComponentPrivate->curState == OMX_StateWaitForResources) 
                        {
                            OMX_PRDSP1(pComponentPrivate->dbg, "%d :: AACENC: AACENCHandleCommand: Cmd Loaded\n",__LINE__);

#ifdef __PERF_INSTRUMENTATION__
                            PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundaryCleanup); 
#endif
                            pComponentPrivate->curState = OMX_StateLoaded;

#ifdef __PERF_INSTRUMENTATION__
                            PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundaryCleanup);
#endif

                            pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                                   pHandle->pApplicationPrivate,
                                                                   OMX_EventCmdComplete, 
                                                                   OMX_CommandStateSet,
                                                                   pComponentPrivate->curState,
                                                                   NULL);
                            pComponentPrivate->bLoadedCommandPending = OMX_FALSE;
                            break;
                        }
                        OMX_PRSTATE1(pComponentPrivate->dbg, "%d :: AACENC: Inside OMX_StateLoaded State: \n",__LINE__);
                        if (pComponentPrivate->curState != OMX_StateIdle && 
                            pComponentPrivate->curState != OMX_StateWaitForResources) 
                        {
                            pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                                   pHandle->pApplicationPrivate,
                                                                   OMX_EventError, 
                                                                   OMX_ErrorIncorrectStateTransition,
                                                                   OMX_TI_ErrorMinor, 
                                                                   "Incorrect State Transition");
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Invalid State Given by Application\n",__LINE__);
                            return eError;
                        }

#ifdef __PERF_INSTRUMENTATION__
                        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryStart | PERF_BoundaryCleanup);
#endif
                        OMX_PRBUFFER2(pComponentPrivate->dbg, "AACENC: pComponentPrivate->pInputBufferList->numBuffers = %d \n",pComponentPrivate->pInputBufferList->numBuffers);
                        OMX_PRBUFFER2(pComponentPrivate->dbg, "AACENC: pComponentPrivate->pOutputBufferList->numBuffers = %d \n",pComponentPrivate->pOutputBufferList->numBuffers);

                        if (pComponentPrivate->pInputBufferList->numBuffers ||
                            pComponentPrivate->pOutputBufferList->numBuffers) 
                        {
                            omx_mutex_wait(&pComponentPrivate->InIdle_mutex,&pComponentPrivate->InIdle_threshold,
                                           &pComponentPrivate->InIdle_goingtoloaded);
                        }

                        /* Now Deinitialize the component No error should be returned from this function. It should clean the system as much as possible */
                        if (pLcmlHandle != NULL) {
                            OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: AACENC: Before CodecControlDestroy \n",__LINE__);
                            eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                     EMMCodecControlDestroy, (void *)pArgs);
#ifdef __PERF_INSTRUMENTATION__
                        PERF_SendingCommand(pComponentPrivate->pPERF, -1, 0, PERF_ModuleComponent);
#endif                                
                        
                            OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: AACENC: After CodecControlDestroy \n",__LINE__);
                            if (eError != OMX_ErrorNone)
                            {
                                OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: LCML_ControlCodec EMMCodecControlDestroy: no.  %x\n",__LINE__, eError);
                                pComponentPrivate->cbInfo.EventHandler (pHandle,
                                    pHandle->pApplicationPrivate,
                                    OMX_EventError,
                                    eError,
                                    OMX_TI_ErrorSevere,
                                    NULL);
                                return eError;
                            }
                            OMX_PRDSP1(pComponentPrivate->dbg, "%d :: AACENCHandleCommand: Cmd Loaded\n",__LINE__);
                            pComponentPrivate->pLcmlHandle = NULL;
                            pLcmlHandle = NULL;
                            /*Closing LCML Lib*/
                            /* This flag is used in Deinit()  function to close LCML. */
                            pComponentPrivate->bCodecDestroyed = OMX_TRUE;
                        }
                        if (pComponentPrivate->ptrLibLCML != NULL)
                        {
                            OMX_PRDSP1(pComponentPrivate->dbg, "AAC: About to Close LCML %p \n",pComponentPrivate->ptrLibLCML);
                            OMX_PRDSP1(pComponentPrivate->dbg, "AAC: Closed LCML \n");
                            dlclose( pComponentPrivate->ptrLibLCML  );  
                            pComponentPrivate->ptrLibLCML = NULL;
                            OMX_PRDSP1(pComponentPrivate->dbg, "AAC: Closed LCML \n");  
                            
                        }
                        OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: AACENC: After CodecControlDestroy \n",__LINE__);
                        OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: AACENCHandleCommand: Cmd Loaded\n",__LINE__);
                        eError = EXIT_COMPONENT_THRD;
                        /* Send StateChangeNotification to application */
                        break;


                case OMX_StatePause:
                        if (pComponentPrivate->curState != OMX_StateExecuting && 
                            pComponentPrivate->curState != OMX_StateIdle) 
                        {
                            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                                   pHandle->pApplicationPrivate,
                                                                   OMX_EventError,
                                                                   OMX_ErrorIncorrectStateTransition,
                                                                   OMX_TI_ErrorMinor,
                                                                   "Incorrect State Transition");
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: Invalid State Given by Application\n",__LINE__);
                            return eError;
                        }
                        OMX_PRDSP2(pComponentPrivate->dbg, "%d :: AACENC: about to call LCML_ControlCodec for PAUSE \n",__LINE__);
                        eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle, EMMCodecControlPause, (void *)pArgs);
                        if (eError != OMX_ErrorNone) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: in Pausing the codec\n",__LINE__);
                            pComponentPrivate->cbInfo.EventHandler (pHandle,
                                    pHandle->pApplicationPrivate,
                                    OMX_EventError,
                                    eError,
                                    OMX_TI_ErrorSevere,
                                    NULL);
                            return eError;
                        }
#ifdef __PERF_INSTRUMENTATION__
                        PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundarySteadyState);
#endif
                        break;


                case OMX_StateWaitForResources:
                        if (pComponentPrivate->curState == OMX_StateLoaded) 
                        {

#ifdef RESOURCE_MANAGER_ENABLED         
            rm_error = RMProxy_NewSendCommand(pHandle, RMProxy_StateSet, OMX_AAC_Encoder_COMPONENT, OMX_StateWaitForResources, 3456, NULL);
#endif 
                        
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
                            pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                                   pHandle->pApplicationPrivate,
                                                                   OMX_EventError,
                                                                   OMX_ErrorIncorrectStateTransition,
                                                                   OMX_TI_ErrorMinor,
                                                                   "Incorrect State Transition");
                        }
                        break;

            
                case OMX_StateInvalid:
                         OMX_PRSTATE2(pComponentPrivate->dbg, "%d: HandleCommand: Cmd OMX_StateInvalid:\n",__LINE__);
                         AACENC_CleanupInitParams(pHandle);
                         if (pComponentPrivate->curState != OMX_StateWaitForResources && 
                             pComponentPrivate->curState != OMX_StateInvalid && 
                             pComponentPrivate->curState != OMX_StateLoaded &&
                             pLcmlHandle != NULL)
                         {
                             eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                         EMMCodecControlDestroy, (void *)pArgs);
                             if (eError != OMX_ErrorNone)
                                 OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: LCML_ControlCodec EMMCodecControlDestroy: no.  %x\n",__LINE__, eError);
                             else{
                                 pComponentPrivate->bCodecDestroyed = OMX_TRUE;
                                 pComponentPrivate->pLcmlHandle = NULL;
                                 pLcmlHandle = NULL;
                             }
                         }
                         
                         pComponentPrivate->curState = OMX_StateInvalid;
                         pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                                 pHandle->pApplicationPrivate,
                                                                 OMX_EventError,
                                                                 OMX_ErrorInvalidState,
                                                                 OMX_TI_ErrorSevere,
                                                                 NULL);

                         break;

                case OMX_StateMax:
                         OMX_PRDSP2(pComponentPrivate->dbg, "%d :: AACENCHandleCommand: Cmd OMX_StateMax::\n",__LINE__);
                         break;
            } /* End of Switch */
        }
    }


    else if (command == OMX_CommandMarkBuffer) 
    {
        OMX_PRDSP2(pComponentPrivate->dbg, "AACENC: command OMX_CommandMarkBuffer received %d\n",__LINE__);
        if(!pComponentPrivate->pMarkBuf) 
        {
            OMX_PRDSP1(pComponentPrivate->dbg, "AACENC: command OMX_CommandMarkBuffer received %d\n",__LINE__);
            pComponentPrivate->pMarkBuf = (OMX_MARKTYPE *)(commandData);
        }
    }
    else if (command == OMX_CommandPortDisable) 
    {
        if (!pComponentPrivate->bDisableCommandPending) 
        {
            if(commandData == 0x0 || commandData == -1)
            {
                /* disable port */
                pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled = OMX_FALSE;
            }
            if(commandData == 0x1 || commandData == -1)
            {
                char *pArgs = "";
                pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled = OMX_FALSE;

                if (pComponentPrivate->curState == OMX_StateExecuting) 
                {
                    pComponentPrivate->bNoIdleOnStop = OMX_TRUE;
                    OMX_PRINT2(pComponentPrivate->dbg, "AACENC: About to stop socket node line %d\n",__LINE__);

                    pComponentPrivate->bIsStopping = 1;

                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                  MMCodecControlStop,(void *)pArgs);

                    if (OMX_ErrorNone != eError) {
                        AACENC_FatalErrorRecover(pComponentPrivate);
                        OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error from LCML_ControlCodec EMMCodecControlStop = %x\n",__LINE__,eError);
                        return eError;
                    }
                    omx_mutex_wait(&pComponentPrivate->codecStop_mutex,&pComponentPrivate->codecStop_threshold,
                                   &pComponentPrivate->codecStop_waitingsignal);
                }
            }
        }

        if(commandData == 0x0) 
        {
            if(!pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated)
            {
                /* return cmdcomplete event if input unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler( pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete,
                                                        OMX_CommandPortDisable,INPUT_PORT, NULL);
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
                pComponentPrivate->cbInfo.EventHandler( pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete, 
                                                        OMX_CommandPortDisable,OUTPUT_PORT, NULL);
                pComponentPrivate->bDisableCommandPending = 0;
            }
            else 
            {
                pComponentPrivate->bDisableCommandPending = 1;
                pComponentPrivate->bDisableCommandParam = commandData;
            }
        }

        if(commandData == -1) 
        {
            if (!pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated && 
                !pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated)
            {

                /* return cmdcomplete event if input & output unpopulated */ 
                pComponentPrivate->cbInfo.EventHandler( pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete, 
                                                        OMX_CommandPortDisable,INPUT_PORT, NULL);

                pComponentPrivate->cbInfo.EventHandler( pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete, 
                                                        OMX_CommandPortDisable,OUTPUT_PORT, NULL);
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
        if (!pComponentPrivate->bEnableCommandPending) 
        {
            if(commandData == 0x0 || commandData == -1) 
            {
                /* enable in port */
                OMX_PRCOMM2(pComponentPrivate->dbg, "AACENC: setting input port to enabled\n");
                pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled = OMX_TRUE;
                OMX_PRCOMM2(pComponentPrivate->dbg, "WAKE UP!! HandleCommand: In utils setting output port to enabled. \n");
                if(pComponentPrivate->AlloBuf_waitingsignal)
                {
                     pComponentPrivate->AlloBuf_waitingsignal = 0;
                }
                OMX_PRCOMM2(pComponentPrivate->dbg, "AACENC: pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled = %d\n",pComponentPrivate->pPortDef[INPUT_PORT]->bEnabled);
                
            }
            if(commandData == 0x1 || commandData == -1) 
            {

                omx_mutex_signal(&pComponentPrivate->AlloBuf_mutex,&pComponentPrivate->AlloBuf_threshold,
                                 &pComponentPrivate->AlloBuf_waitingsignal);

                if (pComponentPrivate->curState == OMX_StateExecuting) 
                {
                    char *pArgs = "";
                    pComponentPrivate->bDspStoppedWhileExecuting = OMX_FALSE;

                    OMX_PRCOMM1(pComponentPrivate->dbg, "AACENC: About to start socket node line %d\n",__LINE__);
                    eError = LCML_ControlCodec( ((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,
                                                   EMMCodecControlStart,(void *)pArgs);
                }
                OMX_PRCOMM1(pComponentPrivate->dbg, "AACENC: setting output port to enabled\n");
                pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled = OMX_TRUE;
                OMX_PRCOMM1(pComponentPrivate->dbg, "AACENC: pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled = %d\n",pComponentPrivate->pPortDef[OUTPUT_PORT]->bEnabled);
            }
        }

        if(commandData == 0x0) 
        {
            if (pComponentPrivate->curState == OMX_StateLoaded || pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated) 
            {
                pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandPortEnable,
                                                       INPUT_PORT,
                                                       NULL);
                pComponentPrivate->bEnableCommandPending = 0;
            }
            else 
            {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->nEnableCommandParam = commandData;
            }
        }
        
        else if(commandData == 0x1) 
        {
            if (pComponentPrivate->curState == OMX_StateLoaded || pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated) 
            {
                pComponentPrivate->cbInfo.EventHandler( pHandle, 
                                                        pHandle->pApplicationPrivate,
                                                        OMX_EventCmdComplete,
                                                        OMX_CommandPortEnable,
                                                        OUTPUT_PORT, 
                                                        NULL);
                pComponentPrivate->bEnableCommandPending = 0;
            }
            else 
            {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->nEnableCommandParam = commandData;
            }
        }

        else if(commandData == -1) 
        {
            if (pComponentPrivate->curState == OMX_StateLoaded || 
                (pComponentPrivate->pPortDef[INPUT_PORT]->bPopulated
                && pComponentPrivate->pPortDef[OUTPUT_PORT]->bPopulated)) 
            {
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortEnable,
                                                       INPUT_PORT, 
                                                       NULL);
                pComponentPrivate->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete, 
                                                       OMX_CommandPortEnable,
                                                       OUTPUT_PORT, 
                                                       NULL);
                pComponentPrivate->bEnableCommandPending = 0;
                AACENCFill_LCMLInitParamsEx(pHandle);
                OMX_PRDSP1(pComponentPrivate->dbg, "\nAACENC: calling fillexparams \n");
            }
            else 
            {
                pComponentPrivate->bEnableCommandPending = 1;
                pComponentPrivate->nEnableCommandParam = commandData;
            }
        }

        omx_mutex_signal(&pComponentPrivate->AlloBuf_mutex,&pComponentPrivate->AlloBuf_threshold,
                         &pComponentPrivate->AlloBuf_waitingsignal);

    }


    else if (command == OMX_CommandFlush) 
    {

        OMX_U32 aParam[3] = {0};
        if (pComponentPrivate->nNumInputBufPending)
        {

            OMX_PRDSP2(pComponentPrivate->dbg, "%d :: AACENC: Inside OMX_CommandFlush Command \n",__LINE__);
            if(commandData == 0x0 || commandData == -1) 
            {
                OMX_ERROR2(pComponentPrivate->dbg, "Flushing input port:: unhandled ETB's = %lud, handled ETB's = %d\n",
                           pComponentPrivate->EmptythisbufferCount, pComponentPrivate->nHandledEmptyThisBuffers);
                if (pComponentPrivate->EmptythisbufferCount == pComponentPrivate->nHandledEmptyThisBuffers) {
                    pComponentPrivate->bFlushInputPortCommandPending = OMX_FALSE;
                    pComponentPrivate->nOutStandingFillDones = 0;
                    aParam[0] = USN_STRMCMD_FLUSH; 
                    aParam[1] = 0x0; 
                    aParam[2] = 0x0; 
                    OMX_PRCOMM1(pComponentPrivate->dbg, "%d :: AACENCHandleCommand::Flushing input port \n",__LINE__);
                    eError=LCML_ControlCodec(( (LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,EMMCodecControlStrmCtrl,(void*)aParam); 
                    if (eError != OMX_ErrorNone) 
                    {
                        pComponentPrivate->cbInfo.EventHandler (pHandle,
                                pHandle->pApplicationPrivate,
                                OMX_EventError,
                                eError,
                                OMX_TI_ErrorSevere,
                                NULL);
                        return eError;
                    }
            }
                else {
                    pComponentPrivate->bFlushInputPortCommandPending = OMX_TRUE;
                    }
            }
        }
        else
        {
             OMX_PRCOMM1(pComponentPrivate->dbg, "%d :: UTIL: Flushing input port \n",__LINE__);
             pComponentPrivate->nOutStandingEmptyDones = 0;                              
             for (i=0; i < MAX_NUM_OF_BUFS; i++) 
             {
                pComponentPrivate->pInputBufHdrPending[i] = NULL;
             }
             pComponentPrivate->nNumInputBufPending=0;                     
             for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) 
             {

                pComponentPrivate->cbInfo.EmptyBufferDone (
                               pComponentPrivate->pHandle,
                               pComponentPrivate->pHandle->pApplicationPrivate,
                               pComponentPrivate->pInputBufferList->pBufHdr[i]
                               );
                AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate,OMX_DirInput);
             }
             pComponentPrivate->cbInfo.EventHandler(pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventCmdComplete,
                                                    OMX_CommandFlush,INPUT_PORT, NULL);

        }



        if (pComponentPrivate->nNumOutputBufPending)
        {
            if(commandData == 0x1 || commandData == -1)
            {
                OMX_ERROR2(pComponentPrivate->dbg, "Flushing output port:: unhandled FTB's = %lud, handled FTB's = %d\n",
                           pComponentPrivate->FillthisbufferCount, pComponentPrivate->nHandledFillThisBuffers);
                if (pComponentPrivate->FillthisbufferCount == pComponentPrivate->nHandledFillThisBuffers) {
                    pComponentPrivate->bFlushOutputPortCommandPending = OMX_FALSE;
                    pComponentPrivate->nOutStandingEmptyDones=0;
                    OMX_PRCOMM1(pComponentPrivate->dbg, "%d :: AACENCHandleCommand::Flushing ouput queue \n",__LINE__);
                    aParam[0] = USN_STRMCMD_FLUSH; 
                    aParam[1] = 0x1; 
                    aParam[2] = 0x0; 
        
                    OMX_PRCOMM1(pComponentPrivate->dbg, "%d :: AACENCHandleCommand::Flushing ouput port  \n",__LINE__);
                    eError = LCML_ControlCodec(((LCML_DSP_INTERFACE*)pLcmlHandle)->pCodecinterfacehandle,EMMCodecControlStrmCtrl, (void*)aParam);
                    if (eError != OMX_ErrorNone) 
                    {
                        pComponentPrivate->cbInfo.EventHandler (pHandle,
                                pHandle->pApplicationPrivate,
                                OMX_EventError,
                                eError,
                                OMX_TI_ErrorSevere,
                                NULL);
                        return eError;
                    }
            }
                else {
                    pComponentPrivate->bFlushOutputPortCommandPending = OMX_TRUE;
                }
            }
        }
        else
        {
            OMX_PRCOMM1(pComponentPrivate->dbg, "%d :: UTIL: Flushing output port \n",__LINE__);
            pComponentPrivate->nOutStandingFillDones = 0;                               
            for (i=0; i < MAX_NUM_OF_BUFS; i++) 
            {
                pComponentPrivate->pOutputBufHdrPending[i] = NULL;
            }
            pComponentPrivate->nNumOutputBufPending=0;
            for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) 
            {
            
                 pComponentPrivate->cbInfo.FillBufferDone (
                                  pComponentPrivate->pHandle,
                                  pComponentPrivate->pHandle->pApplicationPrivate,
                                  pComponentPrivate->pOutputBufferList->pBufHdr[i]
                                  );
                 AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate,OMX_DirOutput);
            }
            pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                    pHandle->pApplicationPrivate,
                                                    OMX_EventCmdComplete,
                                                    OMX_CommandFlush,OUTPUT_PORT, NULL);
        }

    }

EXIT:
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: AACENC: Exiting AACENCHandleCommand Function\n",__LINE__);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: AACENC: Returning %d\n",__LINE__,eError);
    /* report the error to the client via event */
    if(eError != OMX_ErrorNone && eError != EXIT_COMPONENT_THRD){
        OMX_ERROR4(pComponentPrivate->dbg, "%d:: Error: LCML QUEUE BUFFER\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler (pHandle,
                                                pHandle->pApplicationPrivate,
                                                OMX_EventError,
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);

    }
    return eError;
}


/* ========================================================================== */
/**
* AACENCHandleDataBuf_FromApp() This function is called by the component when ever it
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
OMX_ERRORTYPE AACENCHandleDataBuf_FromApp(OMX_BUFFERHEADERTYPE* pBufHeader, AACENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_DIRTYPE eDir = 0;
    LCML_AACENC_BUFHEADERTYPE *pLcmlHdr = NULL;
    LCML_DSP_INTERFACE *pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLcmlHandle;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDefIn = NULL;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Entering AACENCHandleDataBuf_FromApp - curState = %d\n",__LINE__,pComponentPrivate->curState);

    /*Find the direction of the received buffer from buffer list*/
    eError = AACENCGetBufferDirection(pBufHeader, &eDir);
    if (eError != OMX_ErrorNone) 
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: UTIL: The pBufHeader is not found in the list\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler (pComponentPrivate->pHandle,
                                                pComponentPrivate->pHandle->pApplicationPrivate,
                                                OMX_EventError,
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
        return eError;
    }

    if (eDir == OMX_DirInput) 
    {
        pComponentPrivate->nHandledEmptyThisBuffers++;
        OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: UTIL:  Buffer Dir = input\n",__LINE__);
        pPortDefIn = pComponentPrivate->pPortDef[OMX_DirInput];
        if ((pBufHeader->nFilledLen > 0) || (pBufHeader->nFlags & OMX_BUFFERFLAG_EOS)) 
        {
            pComponentPrivate->bBypassDSP = 0;          /* flag for buffers with data */
            eError = AACENCGetCorresponding_LCMLHeader(pComponentPrivate, pBufHeader->pBuffer, OMX_DirInput, &pLcmlHdr);
            if (eError != OMX_ErrorNone) 
            {
                OMX_ERROR4(pComponentPrivate->dbg, "%d :: UTIL: Error: Invalid INPUT Buffer Came ...\n",__LINE__);
                pComponentPrivate->cbInfo.EventHandler (pComponentPrivate->pHandle,
                        pComponentPrivate->pHandle->pApplicationPrivate,
                        OMX_EventError,
                        eError,
                        OMX_TI_ErrorSevere,
                        NULL);
                return eError;
            }
            pLcmlHdr->pIpParam->bLastBuffer = 0;        /* it is not the last buffer yet  */

#ifdef __PERF_INSTRUMENTATION__
            /*For Steady State Instumentation*/
    #if 0 
            if ((pComponentPrivate->nLcml_nCntIp == 1)) {
                    PERF_Boundary(pComponentPrivate->pPERFcomp,
                                  PERF_BoundaryStart | PERF_BoundarySteadyState);
            }
    #endif
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                              PREF(pBufHeader,pBuffer),
                              pPortDefIn->nBufferSize, 
                              PERF_ModuleCommonLayer);
#endif

            if(pBufHeader->nFlags & OMX_BUFFERFLAG_EOS) 
            {
                OMX_PRCOMM1(pComponentPrivate->dbg, "%d :: UTIL: End of Stream has been reached \n",__LINE__);
                pLcmlHdr->pIpParam->bLastBuffer   = 1;  /* EOS flag for SN. - It is the last buffer with data for SN */
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: pLcmlHdr->pIpParam->bLastBuffer = %d \n",__LINE__,(int)pLcmlHdr->pIpParam->bLastBuffer);
            }

            /* Store time stamp information */
            pComponentPrivate->timestampBufIndex[pComponentPrivate->IpBufindex] = pBufHeader->nTimeStamp;
            pComponentPrivate->tickcountBufIndex[pComponentPrivate->IpBufindex] = pBufHeader->nTickCount;
            pComponentPrivate->IpBufindex++;
            pComponentPrivate->IpBufindex %= pComponentPrivate->pPortDef[OMX_DirOutput]->nBufferCountActual;
            
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: Sending input buffer header to Codec = %p to LCML\n",__LINE__,pBufHeader);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: Sending INPUT BUFFER to Codec = %p\n",__LINE__,pBufHeader->pBuffer);
            OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp] pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);
            OMX_PRDSP2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp] pComponentPrivate->bDspStoppedWhileExecuting = %ld\n",__LINE__,pComponentPrivate->bDspStoppedWhileExecuting);

            if (pComponentPrivate->curState == OMX_StateExecuting) 
            {
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp] The current state while sending the buffer = %d \n",__LINE__,pComponentPrivate->curState);
                if(!pComponentPrivate->bDspStoppedWhileExecuting) 
                {
                    if (!AACENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirInput)) 
                    {
                        AACENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirInput,__LINE__);
                        eError = LCML_QueueBuffer(  pLcmlHandle->pCodecinterfacehandle,
                                                    EMMCodecInputBuffer, 
                                                   (OMX_U8 *)pBufHeader->pBuffer, 
                                                    pBufHeader->nAllocLen,
                                                    pBufHeader->nFilledLen, 
                                                    (OMX_U8 *)pLcmlHdr->pIpParam, 
                                                    sizeof(AACENC_UAlgInBufParamStruct), 
                                                    NULL);
                        if (eError != OMX_ErrorNone) 
                        {
                            OMX_ERROR4(pComponentPrivate->dbg, "%d :: UTIL: SetBuff: IP: Error Occurred\n",__LINE__);
                            AACENC_FatalErrorRecover(pComponentPrivate);
                            eError = OMX_ErrorHardware;
                            return eError;
                        }
                        pComponentPrivate->lcml_nIpBuf++;
                        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL:  [HandleData_FromApp] lcml_nIpBuf count :  %d\n",__LINE__, (int)pComponentPrivate->lcml_nIpBuf);
                    }
                }
                else 
                {
#ifdef __PERF_INSTRUMENTATION__
                    PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                      PREF(pBufHeader, pBuffer),
                                      0,
                                      PERF_ModuleHLMM);
#endif  
                    pComponentPrivate->cbInfo.EmptyBufferDone (
                                               pComponentPrivate->pHandle,
                                               pComponentPrivate->pHandle->pApplicationPrivate,
                                               pBufHeader
                                               );
                    AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate,OMX_DirInput);
                    pComponentPrivate->nOutStandingEmptyDones--;
                }
            }
            else 
            {
                /* Save received buffers */
                pComponentPrivate->pInputBufHdrPending[pComponentPrivate->nNumInputBufPending++] = pBufHeader;
            }
        }
        
        else 
        {
            pComponentPrivate->bBypassDSP = 1;          /* flag :   empty buffer */
            OMX_PRDSP2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp] setting pComponentPrivate->bBypassDSP = 1 \n", __LINE__);
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp] Calling FillBufferDone\n", __LINE__);

#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                            PREF(pComponentPrivate->pOutputBufferList->pBufHdr[0],pBuffer),
                            PREF(pComponentPrivate->pOutputBufferList->pBufHdr[0],nFilledLen),
                            PERF_ModuleHLMM);
#endif

            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp] (filled <0 or EOS )Calling FillBufferDone \n",__LINE__);
            pComponentPrivate->cbInfo.FillBufferDone(pComponentPrivate->pHandle,
                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                       pComponentPrivate->pOutputBufferList->pBufHdr[0]
                                       );
            AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate, OMX_DirOutput);
            pComponentPrivate->nOutStandingFillDones--;
            OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->FillbufferdoneCount = %ld \n",__LINE__,pComponentPrivate->FillbufferdoneCount);
            OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->FillthisbufferCount = %ld \n",__LINE__,pComponentPrivate->FillthisbufferCount);
            

#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                            PREF(pComponentPrivate->pInputBufferList->pBufHdr[0], pBuffer),
                            0,
                            PERF_ModuleHLMM);
#endif

            OMX_PRINT1(pComponentPrivate->dbg, "%d :: [HandleData_FromApp] (filled <0 or EOS )Calling EmptyBufferDone \n",__LINE__);
            pComponentPrivate->cbInfo.EmptyBufferDone ( pComponentPrivate->pHandle,
                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                       pComponentPrivate->pInputBufferList->pBufHdr[0]
                                       );
            pComponentPrivate->nOutStandingEmptyDones--;
            AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate,OMX_DirInput);
            OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->EmptybufferdoneCount = %ld \n",__LINE__,pComponentPrivate->EmptybufferdoneCount);
            OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->EmptythisbufferCount = %ld \n",__LINE__,pComponentPrivate->EmptythisbufferCount);
        }

        if(pBufHeader->nFlags & OMX_BUFFERFLAG_EOS) 
        {
            OMX_PRINT2(pComponentPrivate->dbg, "%d :: UTIL: Component Detected EOS\n",__LINE__);
            if(pComponentPrivate->dasfmode == 0) 
            {
                pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags |= OMX_BUFFERFLAG_EOS;      
                OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags = %d \n",__LINE__,(int)pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags);
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                                       OMX_EventBufferFlag,
                                                       pComponentPrivate->pOutputBufferList->pBufHdr[0]->nOutputPortIndex,
                                                       pComponentPrivate->pOutputBufferList->pBufHdr[0]->nFlags, NULL);
                                    
            }
            pBufHeader->nFlags = 0;  
        }

        OMX_PRSTATE1(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);
        if(pBufHeader->pMarkData) 
        {
            OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: Detected pBufHeader->pMarkData\n",__LINE__);
            /* copy mark to output buffer header */
            OMX_PRSTATE2(pComponentPrivate->dbg, "UTIL: pComponentPrivate->curState = %d\n",pComponentPrivate->curState);
            pComponentPrivate->pOutputBufferList->pBufHdr[0]->pMarkData = pBufHeader->pMarkData;
            OMX_PRSTATE2(pComponentPrivate->dbg, "UTIL: pComponentPrivate->curState = %d\n",pComponentPrivate->curState);
            pComponentPrivate->pOutputBufferList->pBufHdr[0]->hMarkTargetComponent = pBufHeader->hMarkTargetComponent;
            OMX_PRSTATE2(pComponentPrivate->dbg, "UTIL: pComponentPrivate->curState = %d\n",pComponentPrivate->curState);
            /* trigger event handler if we are supposed to */
            if(pBufHeader->hMarkTargetComponent == pComponentPrivate->pHandle && pBufHeader->pMarkData) 
            {
                OMX_PRSTATE1(pComponentPrivate->dbg, "UTIL: pComponentPrivate->curState = %d\n",pComponentPrivate->curState);
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                                       OMX_EventMark,
                                                       0,
                                                       0,
                                                       pBufHeader->pMarkData);
            }
        }

        if (pComponentPrivate->bFlushInputPortCommandPending) {
            OMX_SendCommand(pComponentPrivate->pHandle,OMX_CommandFlush,0,NULL);
        }

    } 

    
    else if (eDir == OMX_DirOutput) 
    {
        /* Make sure that output buffer is issued to output stream only when
        * there is an outstanding input buffer already issued on input stream
        */
        OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: UTIL: Buffer Dir = output\n",__LINE__);
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->bIsStopping = %ld\n",__LINE__, pComponentPrivate->bIsStopping);
        OMX_PRDSP1(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->bBypassDSP  = %ld\n",__LINE__, pComponentPrivate->bBypassDSP);
        OMX_PRSTATE1(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);

        pComponentPrivate->nHandledFillThisBuffers++;

#ifdef ANDROID
        if (pComponentPrivate->bFirstOutputBuffer){
        // if this is the first output buffer, fill the config data, then return the buffer (skip DSP)
            AACENCWriteConfigHeader(pComponentPrivate, pBufHeader);
                OMX_PRINT2(pComponentPrivate->dbg, "%d :: UTIL: AACENCWriteConfigHeader = %p\n",__LINE__, pBufHeader->pBuffer);
            pComponentPrivate->cbInfo.FillBufferDone (
                                   pComponentPrivate->pHandle,
                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                   pBufHeader
                                   );
            AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate, OMX_DirOutput);
            pComponentPrivate->bFirstOutputBuffer = 0;
            return eError;
        }
#endif

        if (!(pComponentPrivate->bIsStopping)) 
        {
            if (pComponentPrivate->bBypassDSP == 0) 
            {
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: Sending Output buffer header to Codec= %p to LCML\n",__LINE__,pBufHeader);
                OMX_PRBUFFER1(pComponentPrivate->dbg, "%d :: UTIL: Sending OUTPUT BUFFER to Codec = %p\n",__LINE__,pBufHeader->pBuffer);
#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingFrame(pComponentPrivate->pPERFcomp,
                                 PREF(pBufHeader,pBuffer),
                                  0,
                                  PERF_ModuleCommonLayer);
#endif

                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate = %p\n",__LINE__,pComponentPrivate);
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: pBufHeader = %p\n",__LINE__,pBufHeader);
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: pBufHeader->pBuffer = %p\n",__LINE__,pBufHeader->pBuffer);
                eError = AACENCGetCorresponding_LCMLHeader(pComponentPrivate, pBufHeader->pBuffer, OMX_DirOutput, &pLcmlHdr);
                OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);

                if (pComponentPrivate->bBypassDSP == 0) 
                {
                    OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp]pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);
                    OMX_PRINT2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp]pComponentPrivate->bDspStoppedWhileExecuting = %ld\n",__LINE__,pComponentPrivate->bDspStoppedWhileExecuting);

                    if (pComponentPrivate->curState == OMX_StateExecuting) 
                    {
                        if (!AACENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirOutput)) 
                        {
                            AACENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirOutput,__LINE__);
                            pComponentPrivate->LastOutputBufferHdrQueued = pBufHeader;
                            pLcmlHdr->pOpParam->unNumFramesEncoded=0; /* Resetting the value  for each time*/
                            eError = LCML_QueueBuffer(pLcmlHandle->pCodecinterfacehandle,
                                                      EMMCodecOuputBuffer, 
                                                      (OMX_U8 *)pBufHeader->pBuffer, 
                                                      pBufHeader->nAllocLen,
                                                      pBufHeader->nFilledLen,
                                                      (OMX_U8 *)pLcmlHdr->pOpParam,
                                                      sizeof(AACENC_UAlgOutBufParamStruct),
                                                      NULL);
                            OMX_PRBUFFER2(pComponentPrivate->dbg,
                                          "%d :: UTIL: Queuing Ouput buffer buffer \n",
                                          __LINE__);
                            if (eError != OMX_ErrorNone ) 
                            {
                                OMX_ERROR4(pComponentPrivate->dbg,
                                           "%d :: UTIL: Issuing DSP OP: Error Occurred\n",
                                           __LINE__);
                                eError = OMX_ErrorHardware;
                                AACENC_FatalErrorRecover(pComponentPrivate);
                                return eError;
                            }
                            pComponentPrivate->lcml_nOpBuf++;
                            OMX_PRBUFFER2(pComponentPrivate->dbg,
                                          "%d :: UTIL: tlcml_nOpBuf count : %d\n",
                                          __LINE__,
                                         (int)pComponentPrivate->lcml_nOpBuf);
                        }      
                  }
                  else if (pComponentPrivate->curState == OMX_StatePause) 
                  {
                     OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: pComponentPrivate->nNumOutputBufPending++ = %d \n",__LINE__,(int)pComponentPrivate->nNumOutputBufPending++);
                     OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: pBufHeader = %p \n",__LINE__, pBufHeader);
                     pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = pBufHeader;
                  }
                }
            }
            else 
            {
                OMX_PRDSP2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp] OMX_DirOutput - bBypassDSP = %d \n", __LINE__, (int)pComponentPrivate->bBypassDSP);
                OMX_PRSTATE2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp]pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate->curState);
                OMX_PRDSP2(pComponentPrivate->dbg, "%d :: [HandleData_FromApp]pComponentPrivate->bDspStoppedWhileExecuting = %ld\n",__LINE__,pComponentPrivate->bDspStoppedWhileExecuting);

                if (pComponentPrivate->curState == OMX_StateExecuting) 
                {
                    if (!AACENC_IsPending(pComponentPrivate,pBufHeader,OMX_DirOutput)) 
                    {
                        AACENC_SetPending(pComponentPrivate,pBufHeader,OMX_DirOutput,__LINE__);
                        pComponentPrivate->LastOutputBufferHdrQueued = pBufHeader;
                        if (pLcmlHdr != NULL) {
                            pLcmlHdr->pOpParam->unNumFramesEncoded = 0; /* Resetting the value  for each time*/
                            eError = LCML_QueueBuffer(pLcmlHandle->pCodecinterfacehandle,
                                                      EMMCodecOuputBuffer,
                                                      (OMX_U8 *)pBufHeader->pBuffer,
                                                      pBufHeader->nAllocLen,
                                                      pBufHeader->nFilledLen,
                                                      (OMX_U8 *)pLcmlHdr->pOpParam,
                                                      sizeof(AACENC_UAlgOutBufParamStruct),
                                                      NULL);
                            if (eError != OMX_ErrorNone )
                            {
                                OMX_ERROR4(pComponentPrivate->dbg,
                                           "%d :: UTIL: Issuing DSP OP: Error Occurred\n",
                                           __LINE__);
                                AACENC_FatalErrorRecover(pComponentPrivate);
                                eError = OMX_ErrorHardware;
                                return eError;
                            }
                            pComponentPrivate->lcml_nOpBuf++;
                            OMX_PRDSP2(pComponentPrivate->dbg,
                                       "%d :: UTIL: lcml_nOpBuf count : %d\n",
                                       __LINE__,
                                       (int)pComponentPrivate->lcml_nOpBuf);
                        }
                    }
                }
                else 
                {
                    pComponentPrivate->pOutputBufHdrPending[pComponentPrivate->nNumOutputBufPending++] = pBufHeader;
                }
            }
        }

        if (pComponentPrivate->bFlushOutputPortCommandPending) {
            OMX_SendCommand( pComponentPrivate->pHandle,
                                  OMX_CommandFlush,
                                  1,NULL);
        }
    } 
    else 
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: UTIL: BufferHeader  = %p, Buffer  = %p Unknown\n",__LINE__,pBufHeader, pBufHeader->pBuffer);
        eError = OMX_ErrorBadParameter;
    }
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL:Exiting from  HandleDataBuf_FromApp ..........>>>>>\n",__LINE__);
    /* report the error to the client via event */
    if(eError != OMX_ErrorNone){
        OMX_ERROR4(pComponentPrivate->dbg, "%d:: Error: LCML QUEUE BUFFER\n",__LINE__);
        pComponentPrivate->cbInfo.EventHandler (pComponentPrivate->pHandle,
                                                pComponentPrivate->pHandle->pApplicationPrivate,
                                                OMX_EventError,
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);

    }
    return eError;
}

/*-------------------------------------------------------------------*/
/**
* AACENCGetBufferDirection () This function is used by the component thread to
* request a buffer from the application.  Since it was called from 2 places,
* it made sense to turn this into a small function.
*
* @param pData pointer to AAC Decoder Context Structure
* @param pCur pointer to the buffer to be requested to be filled
*
* @retval none
**/
/*-------------------------------------------------------------------*/

OMX_ERRORTYPE AACENCGetBufferDirection(OMX_BUFFERHEADERTYPE *pBufHeader, OMX_DIRTYPE *eDir)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    AACENC_COMPONENT_PRIVATE *pComponentPrivate = pBufHeader->pPlatformPrivate;
    OMX_U32 nBuf=0;
    OMX_BUFFERHEADERTYPE *pBuf = NULL;
    OMX_U32 i=0;
    OMX_S16 flag = 1;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Entering AACENCGetBufferDirection Function\n",__LINE__);
    /*Search this buffer in input buffers list */
    
    nBuf = pComponentPrivate->pInputBufferList->numBuffers;
    for(i=0; i<nBuf; i++) 
    {
        pBuf = pComponentPrivate->pInputBufferList->pBufHdr[i];
        if(pBufHeader == pBuf) 
        {
            *eDir = OMX_DirInput;
            OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Buffer %p is INPUT BUFFER\n",__LINE__, pBufHeader);
            flag = 0;
            return eError;
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
            OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Buffer %p is OUTPUT BUFFER\n",__LINE__, pBufHeader);
            flag = 0;
            return eError;
        }
    }

    if (flag == 1) 
    {
        OMX_ERROR4(pComponentPrivate->dbg, "%d :: UTIL: Buffer %p is Not Found in the List\n",__LINE__,pBufHeader);
        eError = OMX_ErrorUndefined;
        return eError;
    }
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Exiting AACENCGetBufferDirection Function\n",__LINE__);
    return eError;
}

/* -------------------------------------------------------------------*/
/**
  *  AACENCLCML_Callback() function will be called LCML component to write the msg
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

OMX_ERRORTYPE AACENCLCML_Callback(TUsnCodecEvent event,void * args [10])
{
 /*   OMX_S16 ret = 0; */
    OMX_COMPONENTTYPE *pHandle=NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U8 *pBuffer = args[1];
    LCML_AACENC_BUFHEADERTYPE *pLcmlHdr = NULL;;
    AACENC_COMPONENT_PRIVATE *pComponentPrivate_CC = NULL;
    OMX_S16 i = 0;
    int k=0 ;
    OMX_TICKS bufferDuration =0;

#ifdef RESOURCE_MANAGER_ENABLED 
    OMX_ERRORTYPE rm_error = OMX_ErrorNone;
#endif  

    pComponentPrivate_CC = (AACENC_COMPONENT_PRIVATE*)((LCML_DSP_INTERFACE *)args[6])->pComponentPrivate;
    OMX_PRINT1(pComponentPrivate_CC->dbg, "%d:pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate_CC->curState);

pHandle = pComponentPrivate_CC->pHandle;

    switch(event) {
        
        case EMMCodecDspError:
            OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecDspError\n");
            break;

        case EMMCodecInternalError:
            OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecInternalError\n");
            break;

        case EMMCodecInitError:
            OMX_PRDSP2(pComponentPrivate_CC->dbg, "[LCML CALLBACK EVENT]  EMMCodecInitError\n");
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
    
    OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: UTIL: Entering the AACENCLCML_Callback Function\n",__LINE__);
    OMX_PRINT2(pComponentPrivate_CC->dbg, "UTIL: args = %p \n",args[0]);
    OMX_PRBUFFER2(pComponentPrivate_CC->dbg, " %d :: UTIL: EMMCodecInputBuffer = %p \n",__LINE__,(void *)EMMCodecInputBuffer);
    OMX_PRBUFFER2(pComponentPrivate_CC->dbg, " %d :: UTIL: EMMCodecOuputBuffer = %p \n",__LINE__,(void *)EMMCodecOuputBuffer);
    OMX_PRINT2(pComponentPrivate_CC->dbg, " %d :: UTIL:Entering the LCML_Callback() : event = %d\n",__LINE__,event);
    OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d :: UTIL: pHandle:%p \n",__LINE__,pHandle);
    OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d :: UTIL: pComponentPrivate_CC:%p \n",__LINE__,pComponentPrivate_CC);
    OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d :: UTIL: pLcmlHdr:%p \n",__LINE__,pLcmlHdr);
/*  OMX_PRINT1(pComponentPrivate_CC->dbg, "%d :: UTIL: pLcmlHdr->pIpParam:%p \n",__LINE__,pLcmlHdr->pIpParam); */


    if(event == EMMCodecBufferProcessed) 
    {
        OMX_PRBUFFER2(pComponentPrivate_CC->dbg, "%d :: UTIL: GOT MESSAGE EMMCodecBufferProcessed \n",__LINE__);
        if( (OMX_U32)args [0] == EMMCodecInputBuffer) 
        {
            OMX_PRBUFFER2(pComponentPrivate_CC->dbg, "%d :: UTIL: Input: pBuffer = %p\n",__LINE__, pBuffer);
            eError = AACENCGetCorresponding_LCMLHeader(pComponentPrivate_CC, pBuffer, OMX_DirInput, &pLcmlHdr);

#ifdef __PERF_INSTRUMENTATION__
            PERF_ReceivedFrame(pComponentPrivate_CC->pPERFcomp,
                               PREF(pLcmlHdr->buffer,pBuffer),
                               0,
                               PERF_ModuleCommonLayer);
#endif

            AACENC_ClearPending(pComponentPrivate_CC,pLcmlHdr->buffer,OMX_DirInput,__LINE__);
            if (eError != OMX_ErrorNone) 
            {
                OMX_ERROR4(pComponentPrivate_CC->dbg, "%d :: UTIL: Error: Invalid Buffer Came ...\n",__LINE__);
                pComponentPrivate_CC->cbInfo.EventHandler (pComponentPrivate_CC->pHandle,
                        pComponentPrivate_CC->pHandle->pApplicationPrivate,
                        OMX_EventError,
                        eError,
                        OMX_TI_ErrorSevere,
                        NULL);
                return eError;
            }
            OMX_PRCOMM2(pComponentPrivate_CC->dbg, "Input CallBack %p\n", pLcmlHdr->buffer);

        if (pComponentPrivate_CC->curState != OMX_StatePause) 
        {

#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate_CC->pPERFcomp,
                                  PREF(pLcmlHdr->buffer,pBuffer),
                                  0,
                                  PERF_ModuleHLMM);
#endif
            pComponentPrivate_CC->cbInfo.EmptyBufferDone (pComponentPrivate_CC->pHandle,
                                                       pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                       pLcmlHdr->buffer);
            AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate_CC,OMX_DirInput);
            pComponentPrivate_CC->nOutStandingEmptyDones--;
            pComponentPrivate_CC->lcml_nIpBuf--;

        }
        else 
        {
            OMX_ERROR4(pComponentPrivate_CC->dbg, "UTIL: Couldn't calling EmptyBufferDone() because pComponentPrivate->curState = %d\n",pComponentPrivate_CC->curState);
            pComponentPrivate_CC->pInBufHdrPausedPending[pComponentPrivate_CC->PendingInPausedBufs++] = pLcmlHdr->buffer;
        }


            pComponentPrivate_CC->nOutStandingEmptyDones++;
            OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: pComponentPrivate->nOutStandingEmptyDones = %ld\n",__LINE__,pComponentPrivate_CC->nOutStandingEmptyDones);
        } 
        else if ((OMX_U32)args [0] == EMMCodecOuputBuffer) 
        {
            OMX_PRBUFFER1(pComponentPrivate_CC->dbg, "%d :: UTIL: Output: pBuffer = %p\n",__LINE__, pBuffer);
            pComponentPrivate_CC->nOutStandingFillDones++;
            OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: Incrementing nOutStandingFillDones = %d\n",__LINE__, (int)pComponentPrivate_CC->nOutStandingFillDones);
            OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: pComponentPrivate_CC->nOutStandingFillDones = %ld\n",__LINE__, pComponentPrivate_CC->nOutStandingFillDones);
            eError = AACENCGetCorresponding_LCMLHeader(pComponentPrivate_CC, pBuffer, OMX_DirOutput, &pLcmlHdr);
            AACENC_ClearPending(pComponentPrivate_CC,pLcmlHdr->buffer,OMX_DirOutput,__LINE__);
            if (eError != OMX_ErrorNone) 
            {
                OMX_ERROR4(pComponentPrivate_CC->dbg, "%d :: UTIL: Error: Invalid Buffer Came ...\n",__LINE__);
                pComponentPrivate_CC->cbInfo.EventHandler (pComponentPrivate_CC->pHandle,
                        pComponentPrivate_CC->pHandle->pApplicationPrivate,
                        OMX_EventError,
                        eError,
                        OMX_TI_ErrorSevere,
                        NULL);
                return eError;
            }
            pLcmlHdr->buffer->nFilledLen = (OMX_U32)args[8];
            OMX_PRBUFFER2(pComponentPrivate_CC->dbg, "%d :: UTIL: pLcmlHdr->buffer->nFilledLen = %ld \n",__LINE__,pLcmlHdr->buffer->nFilledLen);

#ifdef __PERF_INSTRUMENTATION__
            PERF_ReceivedFrame(pComponentPrivate_CC->pPERFcomp,
                               PREF(pLcmlHdr->buffer,pBuffer),
                               PREF(pLcmlHdr->buffer,nFilledLen),
                               PERF_ModuleCommonLayer);
            pComponentPrivate_CC->nLcml_nCntOpReceived++;
            if ((pComponentPrivate_CC->nLcml_nCntIp >= 1) && (pComponentPrivate_CC->nLcml_nCntOpReceived == 1)) 
            {
                PERF_Boundary(pComponentPrivate_CC->pPERFcomp,
                PERF_BoundaryStart | PERF_BoundarySteadyState);
            }
#endif
            pComponentPrivate_CC->LastOutbuf = pLcmlHdr->buffer;    /* back up of processed buffer */
            OMX_PRINT2(pComponentPrivate_CC->dbg, "Output CallBack %p\n", pLcmlHdr->buffer);
            OMX_PRINT2(pComponentPrivate_CC->dbg, "size content  %d\n", (int)pLcmlHdr->buffer->nFilledLen);

/* Previously in HandleDatabuffer form LCML */

        /* Copying time stamp information to output buffer */
        pLcmlHdr->buffer->nTimeStamp = pComponentPrivate_CC->temp_TS;
        bufferDuration = ((pLcmlHdr->pOpParam->unNumFramesEncoded) * 1024 * 1000000) /
                                              pComponentPrivate_CC->ulSamplingRate;
        /* Update time stamp information */
        pComponentPrivate_CC->temp_TS += bufferDuration;
        pLcmlHdr->buffer->nTickCount = pComponentPrivate_CC->tickcountBufIndex[pComponentPrivate_CC->OpBufindex];
        pComponentPrivate_CC->OpBufindex++;
        pComponentPrivate_CC->OpBufindex %= pComponentPrivate_CC->pPortDef[OMX_DirOutput]->nBufferCountActual;

        if (pComponentPrivate_CC->curState != OMX_StatePause) 
        {

            OMX_PRBUFFER2(pComponentPrivate_CC->dbg, "%d :: UTIL: Component Sending Filled Output buffer%p to App\n",__LINE__,pLcmlHdr->buffer);
            OMX_PRBUFFER2(pComponentPrivate_CC->dbg, "UTIL:: Calling FillBufferDone from Line %d\n",__LINE__);

#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingFrame(pComponentPrivate_CC->pPERFcomp,
                                pLcmlHdr->buffer->pBuffer,
                                pLcmlHdr->buffer->nFilledLen,
                                PERF_ModuleHLMM);
#endif

#ifdef AACENC_DEBUG     
            unsigned long TmpNumFrames = pLcmlHdr->pOpParam->unNumFramesEncoded; 
            OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: UTIL: Num frames: %lx \n",__LINE__,TmpNumFrames);
#endif
            for(k=0; k<MPEG4AACENC_MAX_OUTPUT_FRAMES; k++)
            {
                OMX_PRINT2(pComponentPrivate_CC->dbg, "%d Frame size[%d]: %lx \n",__LINE__,k,pLcmlHdr->pOpParam->unFrameSizes[k]);  
            }
            pLcmlHdr->buffer->pOutputPortPrivate=pLcmlHdr->pOpParam;
            pComponentPrivate_CC->cbInfo.FillBufferDone (
                               pHandle,
                               pHandle->pApplicationPrivate,
                               pLcmlHdr->buffer
                               );
            AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate_CC, OMX_DirOutput);
            pComponentPrivate_CC->nOutStandingFillDones--;
            pComponentPrivate_CC->lcml_nOpBuf--;
            }
        else
        {
            OMX_ERROR4(pComponentPrivate_CC->dbg, "UTIL: Couldn't calling fillBufferDone() because pComponentPrivate->curState = %d\n",pComponentPrivate_CC->curState);
            pComponentPrivate_CC->pOutBufHdrPausedPending[pComponentPrivate_CC->PendingOutPausedBufs++] = pLcmlHdr->buffer;
        }
        
    
        }
    } 


    else if (event == EMMCodecStrmCtrlAck) 
    {
          OMX_PRDSP1(pComponentPrivate_CC->dbg, "%d :: UTIL: GOT MESSAGE USN_DSPACK_STRMCTRL \n",__LINE__);
          if (args[1] == (void *)USN_STRMCMD_FLUSH) 
          {
                 pHandle = pComponentPrivate_CC->pHandle;                              
                 if ( args[2] == (void *)EMMCodecInputBuffer) 
                 {
                     if (args[0] == (void*)USN_ERR_NONE ) 
                     {
                         OMX_PRCOMM1(pComponentPrivate_CC->dbg, "%d :: UTIL: Flushing input port \n",__LINE__);
                         for (i=0; i < pComponentPrivate_CC->nNumInputBufPending; i++) {
#ifdef __PERF_INSTRUMENTATION__
                            PERF_SendingFrame(pComponentPrivate_CC->pPERFcomp,
                                              PREF(pComponentPrivate_CC->pInputBufferList->pBufHdr[i],pBuffer),
                                              0,PERF_ModuleHLMM);
#endif
                            pComponentPrivate_CC->cbInfo.EmptyBufferDone (
                                           pComponentPrivate_CC->pHandle,
                                           pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                           pComponentPrivate_CC->pInputBufHdrPending[i]);
                            AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate_CC,OMX_DirInput);
                            pComponentPrivate_CC->pInputBufHdrPending[i] = NULL;
                         }
                         pComponentPrivate_CC->nNumInputBufPending=0;
                         pComponentPrivate_CC->cbInfo.EventHandler(pHandle,
                                                                  pHandle->pApplicationPrivate,
                                                                  OMX_EventCmdComplete,
                                                                  OMX_CommandFlush,INPUT_PORT, NULL);
                    }
                    else 
                    {
                        OMX_ERROR4(pComponentPrivate_CC->dbg, "%d :: UTIL: LCML reported error while flushing input port\n",__LINE__);
                        eError = OMX_ErrorHardware;
                        pComponentPrivate_CC->cbInfo.EventHandler (pComponentPrivate_CC->pHandle,
                                pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                OMX_EventError,
                                eError,
                                OMX_TI_ErrorSevere,
                                NULL);
                        return eError;
                    }
                }
                else if ( args[2] == (void *)EMMCodecOuputBuffer) 
                { 
                    if (args[0] == (void*)USN_ERR_NONE ) 
                    {                     
                         OMX_PRDSP1(pComponentPrivate_CC->dbg, "\tCallback FLUSH OUT %ld\n",pComponentPrivate_CC->nNumOutputBufPending);
                         pComponentPrivate_CC->nOutStandingFillDones = 0;                                
                         for (i=0; i < pComponentPrivate_CC->nNumOutputBufPending; i++) 
                         {
#ifdef __PERF_INSTRUMENTATION__
                            PERF_SendingFrame(pComponentPrivate_CC->pPERFcomp,
                            PREF(pComponentPrivate_CC->pOutputBufferList->pBufHdr[i],pBuffer),
                            PREF(pComponentPrivate_CC->pOutputBufferList->pBufHdr[i],nFilledLen),
                            PERF_ModuleHLMM);
#endif
                            pComponentPrivate_CC->cbInfo.FillBufferDone (
                                           pComponentPrivate_CC->pHandle,
                                           pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                           pComponentPrivate_CC->pOutputBufHdrPending[i]
                                           );
                            AACENC_SignalIfAllBuffersAreReturned(pComponentPrivate_CC, OMX_DirOutput);
                            pComponentPrivate_CC->pOutputBufHdrPending[i] = NULL;
                         }
                         pComponentPrivate_CC->nNumOutputBufPending=0;
                         pComponentPrivate_CC->cbInfo.EventHandler(
                                         pHandle, pHandle->pApplicationPrivate,
                                         OMX_EventCmdComplete, OMX_CommandFlush,OUTPUT_PORT, NULL);
                     }
                     else 
                     {
                        OMX_ERROR4(pComponentPrivate_CC->dbg, "%d :: UTIL: LCML reported error while flushing output port\n",__LINE__);
                        eError = OMX_ErrorHardware;
                        pComponentPrivate_CC->cbInfo.EventHandler (pComponentPrivate_CC->pHandle,
                                pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                OMX_EventError,
                                eError,
                                OMX_TI_ErrorSevere,
                                NULL);
                        return eError;
                     }
            }
        }
    }

    
    else if(event == EMMCodecProcessingStoped) 
    {
        OMX_PRINT2(pComponentPrivate_CC->dbg, "AAC encoder received stop ack, waiting for all outstanding buffers to be returned");

        AACENC_waitForAllBuffersToReturn(pComponentPrivate_CC);

        omx_mutex_signal(&pComponentPrivate_CC->codecStop_mutex,&pComponentPrivate_CC->codecStop_threshold,
                         &pComponentPrivate_CC->codecStop_waitingsignal);

        if (!pComponentPrivate_CC->bNoIdleOnStop) 
        {
            pComponentPrivate_CC->curState = OMX_StateIdle;
#ifdef RESOURCE_MANAGER_ENABLED

            rm_error = RMProxy_NewSendCommand(pComponentPrivate_CC->pHandle,
                                           RMProxy_StateSet,
                                           OMX_AAC_Encoder_COMPONENT,
                                           OMX_StateIdle,
                                           3456, NULL);

#endif  
            if (pComponentPrivate_CC->bPreempted == 0) {
                pComponentPrivate_CC->cbInfo.EventHandler(pComponentPrivate_CC->pHandle,
                                                       pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                       OMX_EventCmdComplete,
                                                       OMX_CommandStateSet,
                                                       pComponentPrivate_CC->curState,
                                                       NULL);
            }
            else {
                pComponentPrivate_CC->cbInfo.EventHandler(pComponentPrivate_CC->pHandle,
                                                       pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                       OMX_EventError,
                                                       OMX_ErrorResourcesPreempted,
                                                       OMX_TI_ErrorMajor,
                                                       NULL);
            }

        }
        else 
        {
            OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d :: UTIL: setting bDspStoppedWhileExecuting\n",__LINE__);
            pComponentPrivate_CC->bDspStoppedWhileExecuting = OMX_TRUE;
            OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: bNoIdleOnStop %ld \n", __LINE__,pComponentPrivate_CC->bNoIdleOnStop);
            pComponentPrivate_CC->bNoIdleOnStop= OMX_FALSE;
        }

    }

    
    else if(event == EMMCodecDspMessageRecieved) 
    {
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d :: UTIL: GOT MESSAGE EMMCodecDspMessageRecieved \n",__LINE__);
        OMX_PRSTATE1(pComponentPrivate_CC->dbg, "%d :: UTIL: commandedState  = %p\n",__LINE__,args[0]);
        OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: UTIL: arg1 = %p\n",__LINE__,args[1]);
        OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: UTIL: arg2 = %p\n",__LINE__,args[2]);
        if(0x0500 == (OMX_U32)args[2]) 
        {
            OMX_PRINT2(pComponentPrivate_CC->dbg, "%d :: UTIL: See Message Here\n",__LINE__);
        }
    }

    
    else if(event == EMMCodecAlgCtrlAck) 
    {
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d :: UTIL: GOT MESSAGE USN_DSPACK_ALGCTRL \n",__LINE__);
    }

    
    else if (event == EMMCodecDspError) 
    {
        OMX_PRSTATE2(pComponentPrivate_CC->dbg, "%d :: commandedState  = %d\n",__LINE__,(int)args[0]);
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d :: arg4 = %d\n",__LINE__,(int)args[4]);
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d :: arg5 = %d\n",__LINE__,(int)args[5]);
        OMX_PRDSP2(pComponentPrivate_CC->dbg, "%d ::UTIL: EMMCodecDspError Here\n",__LINE__);
        switch ( (OMX_U32) args [4])
        {
            /* USN_ERR_NONE,: Indicates that no error encountered during execution of the command and the command execution completed succesfully.
             * USN_ERR_WARNING,: Indicates that process function returned a warning. The exact warning is returned in Arg2 of this message.
             * USN_ERR_PROCESS,: Indicates that process function returned a error type. The exact error type is returnd in Arg2 of this message.
             * USN_ERR_PAUSE,: Indicates that execution of pause resulted in error.
             * USN_ERR_STOP,: Indicates that execution of stop resulted in error.
             * USN_ERR_ALGCTRL,: Indicates that execution of alg control resulted in error.
             * USN_ERR_STRMCTRL,: Indiactes the execution of STRM control command, resulted in error.
             * USN_ERR_UNKNOWN_MSG,: Indicates that USN received an unknown command. */

#ifdef _ERROR_PROPAGATION__
            case USN_ERR_PAUSE:
            case USN_ERR_STOP:
            case USN_ERR_ALGCTRL:
            case USN_ERR_STRMCTRL:
            case USN_ERR_UNKNOWN_MSG:
                {
                    AACENC_FatalErrorRecover(pComponentPrivate_CC);
                }
                break;
#endif

            case USN_ERR_WARNING:
            case USN_ERR_PROCESS:
                AACENC_HandleUSNError (pComponentPrivate_CC, (OMX_U32)args[5]);
                break;
            case USN_ERR_NONE:
                if((args[5] == (void*)NULL) && (pComponentPrivate_CC->MMUFault==OMX_FALSE))
                {
                    OMX_ERROR4(pComponentPrivate_CC->dbg, "%d :: UTIL: MMU_Fault \n",__LINE__);
                    pComponentPrivate_CC->MMUFault = OMX_TRUE;
                    AACENC_FatalErrorRecover(pComponentPrivate_CC);
                }
                break;
            default:
                break;
        }

    }


    else if (event == EMMCodecProcessingPaused) 
    { 
        pComponentPrivate_CC->bPauseCommandPending = OMX_TRUE;
        AACENC_TransitionToPause(pComponentPrivate_CC);

    }

#ifdef _ERROR_PROPAGATION__

    else if (event ==EMMCodecInitError)
    {
        /* Cheking for MMU_fault */
        if((args[4] == (void*)USN_ERR_UNKNOWN_MSG) && (args[5] == (void*)NULL)) 
        {
            OMX_ERROR4(pComponentPrivate_CC->dbg, "%d :: UTIL: MMU_Fault \n",__LINE__);
            AACENC_FatalErrorRecover(pComponentPrivate_CC);
        }   
    }

    
    else if (event ==EMMCodecInternalError)
    {
        /* Cheking for MMU_fault */
        if((args[4] == (void*)USN_ERR_UNKNOWN_MSG) && (args[5] == (void*)NULL)) 
        {
            OMX_ERROR4(pComponentPrivate_CC->dbg, "%d :: UTIL: MMU_Fault \n",__LINE__);
            pComponentPrivate_CC->bIsInvalidState=OMX_TRUE;
            OMX_PRSTATE2(pComponentPrivate_CC->dbg, "State changed to OMX_StateInvalid Line %d\n",__LINE__);
            
            pComponentPrivate_CC->curState = OMX_StateInvalid;
            pHandle = pComponentPrivate_CC->pHandle;
            pComponentPrivate_CC->cbInfo.EventHandler(pHandle, 
                                                       pHandle->pApplicationPrivate,
                                                       OMX_EventError,
                                                       OMX_ErrorStreamCorrupt, 
                                                       OMX_TI_ErrorSevere,
                                                       NULL);
        }
    }
#endif

    OMX_PRINT1(pComponentPrivate_CC->dbg, "%d:pComponentPrivate->curState = %d\n",__LINE__,pComponentPrivate_CC->curState);
    OMX_PRINT1(pComponentPrivate_CC->dbg, "%d :: UTIL: Exiting the LCML_Callback Function\n",__LINE__);
    /* report the error to the client via event */
    if(eError != OMX_ErrorNone){
        OMX_ERROR4(pComponentPrivate_CC->dbg, "%d:: Error: \n",__LINE__);
        pComponentPrivate_CC->cbInfo.EventHandler (pComponentPrivate_CC->pHandle, 
                                                pComponentPrivate_CC->pHandle->pApplicationPrivate,
                                                OMX_EventError, 
                                                eError,
                                                OMX_TI_ErrorSevere,
                                                NULL);
    }
    return eError;
}

/* -------------------------------------------------------------------*/
/**
  *  AACENCGetCorresponding_LCMLHeader() function will be called by LCML_Callback
  *                                 component to write the msg
  * @param *pBuffer,          Event which gives to details about USN status
  * @param LCML_AACENC_BUFHEADERTYPE **ppLcmlHdr            //  args [0] //bufType;

  * @retval OMX_NoError              Success, ready to roll
  *         OMX_Error_BadParameter   The input parameter pointer is null
 **/
/* -------------------------------------------------------------------*/
OMX_ERRORTYPE AACENCGetCorresponding_LCMLHeader(AACENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U8 *pBuffer, OMX_DIRTYPE eDir, LCML_AACENC_BUFHEADERTYPE **ppLcmlHdr)
{

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    LCML_AACENC_BUFHEADERTYPE *pLcmlBufHeader = NULL;
    int nIpBuf =0 ;
    int nOpBuf =0;
    OMX_U16 i  =0; 

    AACENC_COMPONENT_PRIVATE *pComponentPrivate_CC = NULL;
    pComponentPrivate_CC = pComponentPrivate;
    
    nIpBuf=pComponentPrivate_CC->pInputBufferList->numBuffers;
    nOpBuf=pComponentPrivate_CC->pOutputBufferList->numBuffers;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Entering AACENCGetCorresponding_LCMLHeader..\n",__LINE__);

    if(eDir == OMX_DirInput) {
        OMX_PRDSP1(pComponentPrivate->dbg, "%d :: UTIL: AACENCGetCorresponding_LCMLHeader -- eDir = %d \n",__LINE__,eDir);
        pLcmlBufHeader = pComponentPrivate_CC->pLcmlBufHeader[INPUT_PORT];
        OMX_PRDSP1(pComponentPrivate->dbg, "%d :: UTIL: Before corresponding LCML Header is Found \n",__LINE__);
        for(i=0; i<nIpBuf; i++) {
            if(pBuffer == pLcmlBufHeader->buffer->pBuffer) {
                *ppLcmlHdr = pLcmlBufHeader;
                OMX_PRDSP2(pComponentPrivate->dbg, "%d::  UTIL: Corresponding LCML Header Found\n",__LINE__);
                OMX_PRBUFFER2(pComponentPrivate->dbg, "::  UTIL: pBuffer = %p\n",pBuffer);
                OMX_PRBUFFER2(pComponentPrivate->dbg, "::  UTIL: pLcmlBufHeader->buffer = %p\n",pLcmlBufHeader->buffer);
                OMX_PRBUFFER2(pComponentPrivate->dbg, "::  UTIL: pLcmlBufHeader->buffer->pBuffer = %p\n",pLcmlBufHeader->buffer->pBuffer);
                return eError;
            }
            pLcmlBufHeader++;
        }
        OMX_PRDSP2(pComponentPrivate->dbg, "%d::  UTIL: Corresponding LCML Header Not Found\n",__LINE__);
        eError = OMX_ErrorUndefined;
    }
    else if (eDir == OMX_DirOutput) 
    {
        OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: AACENCGetCorresponding_LCMLHeader -- eDir = %d \n",__LINE__,eDir);
        pLcmlBufHeader = pComponentPrivate_CC->pLcmlBufHeader[OUTPUT_PORT];
        OMX_PRINT2(pComponentPrivate->dbg, "%d :: UTIL: Before corresponding LCML Header is Found \n",__LINE__);
        OMX_PRBUFFER2(pComponentPrivate->dbg, "nOpBuf = %d\n",nOpBuf);
        for(i=0; i<nOpBuf; i++) 
        {
            if(pBuffer == pLcmlBufHeader->buffer->pBuffer) 
            {
                *ppLcmlHdr = pLcmlBufHeader;
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:: UTIL: Corresponding LCML Header Found\n",__LINE__);
                OMX_PRBUFFER2(pComponentPrivate->dbg, ":: UTIL: pBuffer = %p\n",pBuffer);
                OMX_PRBUFFER2(pComponentPrivate->dbg, "::  UTIL: pLcmlBufHeader->buffer = %p\n",pLcmlBufHeader->buffer);
                OMX_PRBUFFER2(pComponentPrivate->dbg, ":: UTIL: pLcmlBufHeader->buffer->pBuffer = %p\n",pLcmlBufHeader->buffer->pBuffer);
                return eError;
            }
            pLcmlBufHeader++;
        }
        OMX_PRDSP2(pComponentPrivate->dbg, "%d::  UTIL: Corresponding LCML Header Not Found\n",__LINE__);
        eError = OMX_ErrorUndefined;
    } 
    else 
    {
      OMX_PRBUFFER2(pComponentPrivate->dbg, "%d:: UTIL: Invalid Buffer Type :: exiting...\n",__LINE__);
    }

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: UTIL: Exiting AACENCGetCorresponding_LCMLHeader..\n",__LINE__);
    return eError;
}


/* -------------------------------------------------------------------*/
/**
  *  AACENC_GetLCMLHandle() 
  * 
  * @retval OMX_HANDLETYPE
  *
 -------------------------------------------------------------------*/
OMX_HANDLETYPE AACENCGetLCMLHandle(AACENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle = NULL;
    void *handle;
    const char *error;
    OMX_ERRORTYPE (*fpGetHandle)(OMX_HANDLETYPE);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Entering AACENCGetLCMLHandle..\n",__LINE__);
    dlerror();
    handle = dlopen("libLCML.so", RTLD_LAZY);
    if (!handle) {
        fputs(dlerror(), stderr);
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting AACENCGetLCMLHandle..\n",__LINE__);
        return pHandle;
    }

    dlerror();
    fpGetHandle = dlsym (handle, "GetHandle");
    if(NULL == fpGetHandle){
        if ((error = dlerror()) != NULL) {
            fputs(error, stderr);
        }
        dlclose(handle);
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting AACENCGetLCMLHandle..\n",__LINE__);
        return pHandle;
    }
    eError = (*fpGetHandle)(&pHandle);
    if(eError != OMX_ErrorNone) {
        eError = OMX_ErrorUndefined;
        OMX_ERROR4(pComponentPrivate->dbg, "eError != OMX_ErrorNone...\n");
        pHandle = NULL;

        dlclose(handle);                            /* got error - Close LCML lib  */
        OMX_ERROR4(pComponentPrivate->dbg, "AAC: [AACENCGetLCMLHandle] closing LCML \n");
        handle = NULL;
        OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting AACENCGetLCMLHandle..\n",__LINE__);
        return pHandle;
    }
    
    ((LCML_DSP_INTERFACE*)pHandle)->pComponentPrivate= pComponentPrivate;
    pComponentPrivate->ptrLibLCML=handle;           /* saving LCML lib pointer  */
    OMX_PRDSP2(pComponentPrivate->dbg, "AAC: ptrLibLCML = %p\n",pComponentPrivate->ptrLibLCML);
    pComponentPrivate->bGotLCML = OMX_TRUE;
    
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting AACENCGetLCMLHandle..\n",__LINE__);
    return pHandle;
}

/* ================================================================================= */
/**
* @fn AACENC_SetPending() description for AACENC_SetPending  
AACENC_SetPending().  
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
void AACENC_SetPending(AACENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir, OMX_U32 lineNumber) 
{
    OMX_U16 i;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 1;
                OMX_PRBUFFER2(pComponentPrivate->dbg, "*******************INPUT BUFFER %d IS PENDING Line %ld******************************\n",i,lineNumber);
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 1;
                OMX_PRBUFFER2(pComponentPrivate->dbg, "*******************OUTPUT BUFFER %d IS PENDING Line %ld******************************\n",i,lineNumber);
            }
        }
    }
}

/* ================================================================================= */
/**
* @fn AACENC_ClearPending() description for AACENC_ClearPending  
AACENC_ClearPending().  
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
void AACENC_ClearPending(AACENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir, OMX_U32 lineNumber) 
{
    OMX_U16 i;

    OMX_PRINT1(pComponentPrivate->dbg, "pComponentPrivate = %p\n",pComponentPrivate);
    OMX_PRBUFFER1(pComponentPrivate->dbg, "pBufHdr = %p\n",pBufHdr);
    OMX_PRINT2(pComponentPrivate->dbg, "eDir = %d\n",eDir);

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                pComponentPrivate->pInputBufferList->bBufferPending[i] = 0;
                OMX_PRBUFFER2(pComponentPrivate->dbg, "*******************INPUT BUFFER %d IS RECLAIMED Line %ld******************************\n",i,lineNumber);
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                pComponentPrivate->pOutputBufferList->bBufferPending[i] = 0;
                OMX_PRBUFFER2(pComponentPrivate->dbg, "*******************OUTPUT BUFFER %d IS RECLAIMED Line %ld******************************\n",i,lineNumber);
            }
        }
    }
}

/* ================================================================================= */
/**
* @fn AACENC_IsPending() description for AACENC_IsPending  
AACENC_IsPending().  
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
OMX_U32 AACENC_IsPending(AACENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir) 
{
    OMX_U16 i;

    if (eDir == OMX_DirInput) {
        for (i=0; i < pComponentPrivate->pInputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pInputBufferList->pBufHdr[i]) {
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: Buffer pending: pBufHdr = %p \n",__LINE__,pBufHdr);
                return pComponentPrivate->pInputBufferList->bBufferPending[i];
            }
        }
    }
    else {
        for (i=0; i < pComponentPrivate->pOutputBufferList->numBuffers; i++) {
            if (pBufHdr == pComponentPrivate->pOutputBufferList->pBufHdr[i]) {
                OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: UTIL: Buffer pending: pBufHdr = %p \n",__LINE__,pBufHdr);
                return pComponentPrivate->pOutputBufferList->bBufferPending[i];
            }
        }
    }
    return -1;
}

OMX_ERRORTYPE AACENC_TransitionToPause(AACENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMX_PRINT1(pComponentPrivate->dbg, "AACENC_TransitionToPause:::pComponentPrivate->nOutStandingFillDones = %ld\n",pComponentPrivate->nOutStandingFillDones );
    OMX_PRINT1(pComponentPrivate->dbg, "AACENC_TransitionToPause:::pComponentPrivate->nOutStandingEmptyDones = %ld\n",pComponentPrivate->nOutStandingEmptyDones );
    if (pComponentPrivate->nOutStandingFillDones <= 0 && pComponentPrivate->nOutStandingEmptyDones <= 0) 
    {
        pComponentPrivate->curState = OMX_StatePause;
        pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                pComponentPrivate->pHandle->pApplicationPrivate,
                                                OMX_EventCmdComplete,
                                                OMX_CommandStateSet,
                                                pComponentPrivate->curState,
                                                NULL);
    pComponentPrivate->bPauseCommandPending = OMX_FALSE;
    }
    return eError;
}


/* ========================================================================== */
/**
* AACENCFill_LCMLInitParamsEx () This function is used by the component thread to
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
OMX_ERRORTYPE AACENCFill_LCMLInitParamsEx(OMX_HANDLETYPE pComponent)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf,nIpBufSize,nOpBuf,nOpBufSize;
    OMX_U16 i=0, j=0, k=0;
    OMX_BUFFERHEADERTYPE *pTemp = NULL;
    OMX_U32 size_lcml;
    LCML_AACENC_BUFHEADERTYPE *pTemp_lcml = NULL;
    LCML_DSP_INTERFACE *pHandle = (LCML_DSP_INTERFACE *)pComponent;
    AACENC_COMPONENT_PRIVATE *pComponentPrivate = pHandle->pComponentPrivate;

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: AACENCFill_LCMLInitParams\n ",__LINE__);
    nIpBuf = pComponentPrivate->pInputBufferList->numBuffers;
    nIpBufSize = pComponentPrivate->pPortDef[INPUT_PORT]->nBufferSize;
    nOpBuf = pComponentPrivate->pOutputBufferList->numBuffers;
    nOpBufSize = pComponentPrivate->pPortDef[OUTPUT_PORT]->nBufferSize;

    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: ------ Buffer Details -----------\n",__LINE__);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: Input  Buffer Count = %ld \n",__LINE__,nIpBuf);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: Input  Buffer Size = %ld\n",__LINE__,nIpBufSize);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: Output Buffer Count = %ld\n",__LINE__,nOpBuf);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: Output Buffer Size = %ld\n",__LINE__,nOpBufSize);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "%d :: ------ Buffer Details ------------\n",__LINE__);

    size_lcml = nIpBuf * sizeof(LCML_AACENC_BUFHEADERTYPE);
    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, LCML_AACENC_BUFHEADERTYPE);
    OMX_PRDSP2(pComponentPrivate->dbg, "pTemp_lcml %p to %p \n",pTemp_lcml,(pTemp_lcml + sizeof(pTemp_lcml) ));
    
    pComponentPrivate->pLcmlBufHeader[INPUT_PORT] = pTemp_lcml;
    for (i=0; i<nIpBuf; i++) {
    
        pTemp = pComponentPrivate->pInputBufferList->pBufHdr[i];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nIpBufSize;
        pTemp->nFilledLen = nIpBufSize;
        pTemp->nVersion.s.nVersionMajor = AACENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = AACENC_MINOR_VER; 
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirInput;

        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pIpParam, sizeof(AACENC_UAlgInBufParamStruct), AACENC_UAlgInBufParamStruct);
        if (pTemp_lcml->pIpParam == NULL) {
            pTemp_lcml = pComponentPrivate->pLcmlBufHeader[INPUT_PORT];

            for(k=0;k<i;k++) {
                OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pIpParam, AACENC_UAlgInBufParamStruct);
                pTemp_lcml++;
            }
            OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);

            OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting Fill_LCMLInitParams\n",__LINE__);
            return OMX_ErrorInsufficientResources;
        }
        OMX_PRDSP2(pComponentPrivate->dbg, "pTemp_lcml %p to %p \n",pTemp_lcml,(pTemp_lcml + sizeof(pTemp_lcml) ));
        
        pTemp_lcml->pIpParam->bLastBuffer = 0;
        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */
        pTemp->nFlags = NORMAL_BUFFER;
        pTemp_lcml++;
    }

    /* Allocate memory for all output buffer headers,  This memory pointer will be sent to LCML */
    size_lcml = nOpBuf * sizeof(LCML_AACENC_BUFHEADERTYPE);

    OMX_MALLOC_SIZE(pTemp_lcml, size_lcml, LCML_AACENC_BUFHEADERTYPE);
    OMX_PRDSP2(pComponentPrivate->dbg, "size_lcml %d to %lx \n", (int)size_lcml,(size_lcml + sizeof(size_lcml) ));
    
    pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT] = pTemp_lcml;



    OMX_PRBUFFER2(pComponentPrivate->dbg, "[AACENCFill_LCMLInitParamsEx] nOpBuf = %d\n", (int)nOpBuf);
    OMX_PRBUFFER2(pComponentPrivate->dbg, "[AACENCFill_LCMLInitParamsEx] pComponentPrivate->pOutputBufferList->numBuffers = %d\n",pComponentPrivate->pOutputBufferList->numBuffers);
    for (j=0; j<nOpBuf; j++) {
           OMX_PRDSP2(pComponentPrivate->dbg, "[AACENCFill_LCMLInitParamsEx] pTemp_lcml = %p\n",pTemp_lcml);    
        pTemp = pComponentPrivate->pOutputBufferList->pBufHdr[j];
        pTemp->nSize = sizeof(OMX_BUFFERHEADERTYPE);
        pTemp->nAllocLen = nOpBufSize;
        pTemp->nFilledLen = nOpBufSize;
        pTemp->nVersion.s.nVersionMajor = AACENC_MAJOR_VER;
        pTemp->nVersion.s.nVersionMinor = AACENC_MINOR_VER;
        pComponentPrivate->nVersion = pTemp->nVersion.nVersion;
        pTemp->pPlatformPrivate = pHandle->pComponentPrivate;
        pTemp->nTickCount = NOT_USED;
        /* This means, it is not a last buffer. This flag is to be modified by
         * the application to indicate the last buffer */
        pTemp_lcml->buffer = pTemp;
        pTemp_lcml->eDir = OMX_DirOutput;

        OMX_MALLOC_SIZE_DSPALIGN(pTemp_lcml->pOpParam, sizeof(AACENC_UAlgOutBufParamStruct), AACENC_UAlgOutBufParamStruct);
        if (pTemp_lcml->pOpParam == NULL) {
            pTemp_lcml = pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT];

            for(k=0;k<j;k++) {
                OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pOpParam, AACENC_UAlgOutBufParamStruct);
                pTemp_lcml++;
            }
            OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT]);

            pTemp_lcml = pComponentPrivate->pLcmlBufHeader[INPUT_PORT];

            for(k=0;k<i;k++) {
                OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pIpParam, AACENC_UAlgInBufParamStruct);
                pTemp_lcml++;
            }
            OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);
            OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting Fill_LCMLInitParams\n",__LINE__);
            return OMX_ErrorInsufficientResources;
        }

        OMX_PRINT2(pComponentPrivate->dbg, "\n pTemp_lcml->pOpParam %p \n",pTemp_lcml->pOpParam);
        pTemp->nFlags = NORMAL_BUFFER;
        pTemp++;
        pTemp_lcml++;
    }
    pComponentPrivate->bPortDefsAllocated = 1;
    pComponentPrivate->bBypassDSP = 0;

EXIT:
    if (eError != OMX_ErrorNone) {
        pTemp_lcml = pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT];

        for(k=0;k<j;k++) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pOpParam, AACENC_UAlgOutBufParamStruct);
            pTemp_lcml++;
        }
        OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[OUTPUT_PORT]);

        pTemp_lcml = pComponentPrivate->pLcmlBufHeader[INPUT_PORT];

        for(k=0;k<i;k++) {
            OMX_MEMFREE_STRUCT_DSPALIGN(pTemp_lcml->pIpParam, AACENC_UAlgInBufParamStruct);
            pTemp_lcml++;
        }
        OMX_MEMFREE_STRUCT(pComponentPrivate->pLcmlBufHeader[INPUT_PORT]);
    }

    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Exiting Fill_LCMLInitParams\n",__LINE__);
    OMX_PRINT1(pComponentPrivate->dbg, "%d :: Returning = 0x%x\n",__LINE__,eError);
    return eError;
}


OMX_ERRORTYPE AACENCWriteConfigHeader(AACENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr){

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nPosition = 0;
    OMX_U8  nBits = 0;
    OMX_U32 tempData = 0;
    OMX_U8 rateIndex = 0;
    OMX_U16 nBuf = 0;
	OMX_U16 nBuf2 = 0;
    //nBytePosition = nPosition / 8;  //add this back if we need to handle more than 4 bytes (U32).
    //nBitPosition =  nPosition % 8;
    memset(pBufHdr->pBuffer, 0x0, pBufHdr->nAllocLen); // make sure we start with zeroes

    nBits = 5; //audioObjectType
    nPosition += nBits;
    OMX_PRINT2(pComponentPrivate->dbg, "profile is %d\n", pComponentPrivate->aacParams[OUTPUT_PORT]->eAACProfile);
    if (pComponentPrivate->aacParams[OUTPUT_PORT]->eAACProfile == OMX_AUDIO_AACObjectLC)
    {
        tempData = AACENC_OBJ_TYP_LC << (16-nPosition);
        OMX_PRINT2(pComponentPrivate->dbg, "profile is LC, tempData = 2 << (32-5) = %ld\n", tempData);
    }
    else if (pComponentPrivate->aacParams[OUTPUT_PORT]->eAACProfile == OMX_AUDIO_AACObjectHE)
    {
        tempData = AACENC_OBJ_TYP_HEAAC << (16-nPosition);
    }
    else if (pComponentPrivate->aacParams[OUTPUT_PORT]->eAACProfile == OMX_AUDIO_AACObjectHE_PS)
    {
        tempData = AACENC_OBJ_TYP_PS << (16-nPosition);
    }
    nBuf = tempData;

    nBits = 4; //SamplingFrequencyIndex
    nPosition += nBits;
    rateIndex = AACEnc_GetSampleRateIndexL(pComponentPrivate->aacParams[OUTPUT_PORT]->nSampleRate);
    tempData = rateIndex << (16-nPosition);
    nBuf |= tempData;
    OMX_PRBUFFER2(pComponentPrivate->dbg, "CONFIG BUFFER = %d\n\n", nBuf);

    nBits = 4; //channelConfiguration
    nPosition += nBits;
    tempData = pComponentPrivate->aacParams[OUTPUT_PORT]->nChannels << (16-nPosition);
    nBuf |= tempData;


	nBuf2 =	(nBuf>> 8) | (nBuf << 8); /* Changing Endianess */

    OMX_PRBUFFER2(pComponentPrivate->dbg, "CONFIG BUFFER = %d\n\n", nBuf2);

    memcpy(pBufHdr->pBuffer, &nBuf2, sizeof(OMX_U16));
    pBufHdr->nFlags = NORMAL_BUFFER;  // clear any other flags then add the needed ones
    pBufHdr->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
    pBufHdr->nFlags |= OMX_BUFFERFLAG_CODECCONFIG;
    pBufHdr->nFilledLen = sizeof(OMX_U16); //need make this dynamic for non basic LC cases.

    return eError;
}

/*=======================================================================*/
/*! @fn AACDec_GetSampleRateIndexL

 * @brief Gets the sample rate index

 * @param  aRate : Actual Sampling Freq

 * @Return  Index

 */
/*=======================================================================*/
int AACEnc_GetSampleRateIndexL( const int aRate)
{
    int index = 0;

    switch( aRate ){
    case 96000:
        index = 0;
        break;
    case 88200:
        index = 1;
        break;
    case 64000:
        index = 2;
        break;
    case 48000:
        index = 3;
        break;
    case 44100:
        index = 4;
        break;
    case 32000:
        index = 5;
        break;
    case 24000:
        index = 6;
        break;
    case 22050:
        index = 7;
        break;
    case 16000:
        index = 8;
        break;
    case 12000:
        index = 9;
        break;
    case 11025:
        index = 10;
        break;
    case 8000:
        index = 11;
        break;
    default:
        break;
    }

    return index;
}

/*  =========================================================================*/
/*  func    AACENC_HandleUSNError
*
*  desc    Handles error messages returned by the dsp
*
*@return n/a
*/
/*  =========================================================================*/
void AACENC_HandleUSNError (AACENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U32 arg)
{
    switch (arg)
    {

        case IUALG_WARN_CONCEALED:
        case IUALG_WARN_UNDERFLOW:
        case IUALG_WARN_OVERFLOW:
        case IUALG_WARN_ENDOFDATA:
            OMX_ERROR4(pComponentPrivate->dbg,  "Algorithm Error" );
            /* all of these are informative messages, Algo can recover, no need to notify the
             * IL Client at this stage of the implementation */
            break;
        case IUALG_WARN_PLAYCOMPLETED:

            {
                OMX_PRINT2(pComponentPrivate->dbg, "%d :: UTIL: IUALG_WARN_PLAYCOMPLETED/USN_ERR_WARNING event received\n", __LINE__);
                pComponentPrivate->bPlayCompleteFlag = 1;
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                                       OMX_EventBufferFlag,
                                                       (OMX_U32)NULL,
                                                       OMX_BUFFERFLAG_EOS,
                                                       NULL);
                pComponentPrivate->pLcmlBufHeader[0]->pIpParam->bLastBuffer = 0;
            }
            break;

#ifdef _ERROR_PROPAGATION__
        case IUALG_ERR_BAD_HANDLE:
        case IUALG_ERR_DATA_CORRUPT:
        case IUALG_ERR_NOT_SUPPORTED:
        case IUALG_ERR_ARGUMENT:
        case IUALG_ERR_NOT_READY:
        case IUALG_ERR_GENERAL:

            {
                /* all of these are fatal messages, Algo can not recover
                 * hence return an error */
                OMX_ERROR4(pComponentPrivate->dbg,  "Algorithm Error, cannot recover" );
                AACENC_FatalErrorRecover(pComponentPrivate);
            }
            break;
#endif
        default:
            break;
    }
}


#ifdef RESOURCE_MANAGER_ENABLED
void AACENC_ResourceManagerCallback(RMPROXY_COMMANDDATATYPE cbData)
{
    OMX_COMMANDTYPE Cmd = OMX_CommandStateSet;
    OMX_STATETYPE state = OMX_StateIdle;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)cbData.hComponent;
    AACENC_COMPONENT_PRIVATE *pCompPrivate = NULL;

    pCompPrivate = (AACENC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate;

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

}
#endif

void AACENC_FatalErrorRecover(AACENC_COMPONENT_PRIVATE *pComponentPrivate){
    char *pArgs = "";
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    OMX_ERROR4(pComponentPrivate->dbg, "Begin FatalErrorRecover\n");
    AACENC_CleanupInitParams(pComponentPrivate->pHandle);
    if (pComponentPrivate->curState != OMX_StateWaitForResources &&
        pComponentPrivate->curState != OMX_StateLoaded &&
        pComponentPrivate->MMUFault == OMX_FALSE &&     //Comp-Thread still running, can't destroy codec yet
        pComponentPrivate->pLcmlHandle != NULL) {
        eError = LCML_ControlCodec(((
                 LCML_DSP_INTERFACE*)pComponentPrivate->pLcmlHandle)->pCodecinterfacehandle,
                 EMMCodecControlDestroy, (void *)pArgs);
        if (eError != OMX_ErrorNone)
            OMX_ERROR4(pComponentPrivate->dbg, "%d :: Error: LCML_ControlCodec EMMCodecControlDestroy: no.  %x\n",__LINE__, eError);
        else{
            pComponentPrivate->bCodecDestroyed = OMX_TRUE;
            pComponentPrivate->pLcmlHandle = NULL;
        }
    }

#ifdef RESOURCE_MANAGER_ENABLED
    eError = RMProxy_NewSendCommand(pComponentPrivate->pHandle,
             RMProxy_FreeResource,
             OMX_AAC_Encoder_COMPONENT, 0, 3456, NULL);

    eError = RMProxy_Deinitalize();
    if (eError != OMX_ErrorNone) {
        OMX_ERROR4(pComponentPrivate->dbg, "::From RMProxy_Deinitalize\n");
    }
#endif

    pComponentPrivate->curState = OMX_StateInvalid;
    pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                       OMX_EventError,
                                       OMX_ErrorInvalidState,
                                       OMX_TI_ErrorSevere,
                                       NULL);
    OMX_ERROR4(pComponentPrivate->dbg, "Completed FatalErrorRecover \
               \nEntering Invalid State\n");
}
/* ========================================================================== */
/**
 * @SignalIfAllBuffersAreReturned() This function send signals if OMX returned all buffers to app
 *
 * @param AACENC_COMPONENT_PRIVATE *pComponentPrivate
 *
 * @pre None
 *
 * @post None
 *
 * @return None
*/
/* ========================================================================== */
void AACENC_SignalIfAllBuffersAreReturned(AACENC_COMPONENT_PRIVATE *pComponentPrivate,
                                          OMX_U8 counterport)
{
    pthread_mutex_lock(&pComponentPrivate->bufferReturned_mutex);
    if(!counterport)
        pComponentPrivate->EmptybufferdoneCount++;
    else
        pComponentPrivate->FillbufferdoneCount++;
    if ((pComponentPrivate->EmptythisbufferCount == pComponentPrivate->EmptybufferdoneCount) &&
        (pComponentPrivate->FillthisbufferCount ==   pComponentPrivate->FillbufferdoneCount)) {
        pthread_cond_broadcast(&pComponentPrivate->bufferReturned_condition);
        OMX_PRINT2(pComponentPrivate->dbg, "Sending pthread signal that OMX has returned all buffers to app");
    }
    pthread_mutex_unlock(&pComponentPrivate->bufferReturned_mutex);
}

/**
* @AACENC_waitForAllBuffersToReturn This function waits for all buffers to return
*
* @param AACENC_COMPONENT_PRIVATE *pComponentPrivate
*
* @return None
*/
void AACENC_waitForAllBuffersToReturn(
        AACENC_COMPONENT_PRIVATE *pComponentPrivate)
{
    pthread_mutex_lock(&pComponentPrivate->bufferReturned_mutex);
    while (pComponentPrivate->EmptythisbufferCount != pComponentPrivate->EmptybufferdoneCount ||
           pComponentPrivate->FillthisbufferCount  != pComponentPrivate->FillbufferdoneCount) {
        pthread_cond_wait(&pComponentPrivate->bufferReturned_condition, &pComponentPrivate->bufferReturned_mutex);
    }
    pthread_mutex_unlock(&pComponentPrivate->bufferReturned_mutex);
    OMX_PRINT2(pComponentPrivate->dbg, ":: OMX has returned all input and output buffers\n");
}

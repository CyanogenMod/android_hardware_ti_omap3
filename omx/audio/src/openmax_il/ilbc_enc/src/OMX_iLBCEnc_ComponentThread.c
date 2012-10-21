
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
* @file OMX_iLBCEnc_ComponentThread.c
*
* This file implements iLBC Encoder Component Thread and its functionality
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
#include <dbapi.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#endif
/*-------program files ----------------------------------------*/
#include "OMX_iLBCEnc_Utils.h"
/* #include <ResourceManagerProxyAPI.h> */
/* ================================================================================= */
/**
* @fn ILBCENC_CompThread() Component thread
*
*  @see         OMX_iLBCEnc_ComponentThread.h
*/
/* ================================================================================ */

void* ILBCENC_CompThread(void* pThreadData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int status = 0;
    struct timespec tv;
    int fdmax = 0;
    int ret = 0;
    fd_set rfds;
    OMX_U32 nRet = 0;
    OMX_BUFFERHEADERTYPE *pBufHeader = NULL;
    ILBCENC_COMPONENT_PRIVATE* pComponentPrivate = 
        (ILBCENC_COMPONENT_PRIVATE*)pThreadData;
    OMX_COMPONENTTYPE *pHandle = pComponentPrivate->pHandle;

    ILBCENC_DPRINT("%d Entering ILBCENC_CompThread\n", __LINE__);

#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate->pPERFcomp = PERF_Create(PERF_FOURCC('N', 'B', '_', 'E'),
                                               PERF_ModuleComponent |
                                               PERF_ModuleAudioDecode);
#endif

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

#ifndef UNDER_CE
        sigset_t set;
        sigemptyset (&set);
        sigaddset (&set, SIGALRM);
        status = pselect (fdmax+1, &rfds, NULL, NULL, &tv, &set);
#else
        status = select (fdmax+1, &rfds, NULL, NULL, &tv);
#endif
        if (0 == status) {
            ILBCENC_DPRINT("%d bIsThreadstop = %ld\n",__LINE__,pComponentPrivate->bIsThreadstop);
            ILBCENC_DPRINT("%d lcml_nOpBuf = %ld\n",__LINE__,pComponentPrivate->lcml_nOpBuf);
            ILBCENC_DPRINT("%d lcml_nIpBuf = %ld\n",__LINE__,pComponentPrivate->lcml_nIpBuf);
            ILBCENC_DPRINT("%d app_nBuf = %ld\n",__LINE__,pComponentPrivate->app_nBuf);
            if (pComponentPrivate->bIsThreadstop == 1)  {
                pComponentPrivate->bIsThreadstop = 0;
                pComponentPrivate->lcml_nOpBuf = 0;
                pComponentPrivate->lcml_nIpBuf = 0;
                pComponentPrivate->app_nBuf = 0;
                if (pComponentPrivate->curState != OMX_StateIdle) {
                    ILBCENC_DPRINT("%d pComponentPrivate->curState is not OMX_StateIdle\n",__LINE__);
                    goto EXIT;
                }
            }
            ILBCENC_DPRINT("%d Component Time Out !!!!! \n",__LINE__);
        } else if(-1 == status) {
            ILBCENC_EPRINT("%d Error in Select\n", __LINE__);
            pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                     pHandle->pApplicationPrivate,
                                                     OMX_EventError,
                                                     OMX_ErrorInsufficientResources,
                                                     0,
                                                     "Error from CompThread in select");
            eError = OMX_ErrorInsufficientResources;

        } else if ((FD_ISSET (pComponentPrivate->dataPipe[0], &rfds))
                   && (pComponentPrivate->curState != OMX_StatePause)) {
            ILBCENC_DPRINT("%d DATA pipe is set in Component Thread\n",__LINE__);
            ret = read(pComponentPrivate->dataPipe[0], &pBufHeader, sizeof(pBufHeader));
            if (ret == -1) {
                ILBCENC_EPRINT("%d Error while reading from the pipe\n",__LINE__);
                goto EXIT;
            }
            eError = ILBCENC_HandleDataBufFromApp(pBufHeader,pComponentPrivate);
            if (eError != OMX_ErrorNone) {
                ILBCENC_EPRINT("%d ILBCENC_HandleDataBufFromApp returned error\n",__LINE__);
                break;
            }
        } 
        else if(FD_ISSET (pComponentPrivate->cmdPipe[0], &rfds)) {
            /* Do not accept any command when the component is stopping */
            ILBCENC_DPRINT("%d CMD pipe is set in Component Thread\n",__LINE__);
            nRet = ILBCENC_HandleCommand(pComponentPrivate);
            if (nRet == ILBCENC_EXIT_COMPONENT_THRD) {

                if(eError != OMX_ErrorNone) {
                    ILBCENC_DPRINT("%d ILBCENC_CleanupInitParams returned error\n",__LINE__);
                    goto EXIT;
                }
                pComponentPrivate->curState = OMX_StateLoaded;
#ifdef __PERF_INSTRUMENTATION__
                PERF_Boundary(pComponentPrivate->pPERFcomp,PERF_BoundaryComplete | PERF_BoundaryCleanup);
#endif
                if (pComponentPrivate->bPreempted == 0) { 
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventCmdComplete,
                                                            OMX_ErrorNone,
                                                            pComponentPrivate->curState,
                                                            NULL);
                                                        
                }
                else {
                    pComponentPrivate->cbInfo.EventHandler(
                                                           pHandle, pHandle->pApplicationPrivate,
                                                           OMX_EventError,
                                                           OMX_ErrorResourcesLost,pComponentPrivate->curState, NULL);
                    pComponentPrivate->bPreempted = 0;
                }   
                 
            }
        }
    
    }
 EXIT:
    ILBCENC_DPRINT("%d Exiting ILBCENC_CompThread\n", __LINE__);
    ILBCENC_DPRINT("%d Returning = 0x%x\n",__LINE__,eError);
#ifdef __PERF_INSTRUMENTATION__
    PERF_Done(pComponentPrivate->pPERFcomp);
#endif
    return (void*)eError;
}

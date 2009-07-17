
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
* @file OMX_WbAmrEnc_CompThread.c
*
* This file implements WBAMR Encoder Component Thread and its functionality
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

#include <dlfcn.h>

#endif
/*-------program files ----------------------------------------*/
#include "OMX_WbAmrEncoder.h"
#include "OMX_WbAmrEnc_Utils.h"
#include "OMX_WbAmrEnc_CompThread.h"

/* ================================================================================= */
/**
* @fn WBAMRENC_CompThread() Component thread
*
*  @see         OMX_WbAmrEnc_CompThread.h
*/
/* ================================================================================ */

void* WBAMRENC_CompThread(void* pThreadData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int status;
    struct timespec tv;
    int fdmax;
    int ret = 0;
    fd_set rfds;
    OMX_U32 nRet;
    OMX_BUFFERHEADERTYPE *pBufHeader = NULL;
    WBAMRENC_COMPONENT_PRIVATE* pComponentPrivate = (WBAMRENC_COMPONENT_PRIVATE*)pThreadData;
    OMX_COMPONENTTYPE *pHandle = pComponentPrivate->pHandle;
    
    OMX_U32 commandData;
    OMX_COMMANDTYPE command;
    
    WBAMRENC_DPRINT("%d :: Entering WBAMRENC_CompThread\n", __LINE__);

#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate->pPERFcomp = PERF_Create(PERF_FOURCC('W', 'B', 'E', '_'),
                                               PERF_ModuleComponent |
                                               PERF_ModuleAudioDecode);
#endif

    fdmax = pComponentPrivate->cmdPipe[0];

    if (pComponentPrivate->dataPipe[0] > fdmax) {
        fdmax = pComponentPrivate->dataPipe[0];
    }

    while (1) {
        FD_ZERO (&rfds);
        FD_SET (pComponentPrivate->dataPipe[0], &rfds);
        FD_SET (pComponentPrivate->cmdPipe[0], &rfds);

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

        if (pComponentPrivate->bIsThreadstop == 1) {
            WBAMRENC_DPRINT(":: Comp Thrd Exiting here...\n");
            goto EXIT;
        }
        if (0 == status) {
            if (pComponentPrivate->bIsThreadstop == 1)  {
                pComponentPrivate->bIsThreadstop = 0;
                pComponentPrivate->lcml_nOpBuf = 0;
                pComponentPrivate->lcml_nIpBuf = 0;
                pComponentPrivate->app_nBuf = 0;
                if (pComponentPrivate->curState != OMX_StateIdle) {
                    WBAMRENC_DPRINT("%d :: pComponentPrivate->curState is not OMX_StateIdle\n",__LINE__);
                    goto EXIT;
                }
            }
            WBAMRENC_DPRINT("%d :: Component Time Out !!!!! \n",__LINE__);
        } else if(-1 == status) {
            WBAMRENC_EPRINT("%d :: Error in Select\n", __LINE__);
            pComponentPrivate->cbInfo.EventHandler ( pHandle,
                                                     pHandle->pApplicationPrivate,
                                                     OMX_EventError,
                                                     OMX_ErrorInsufficientResources,
                                                     OMX_TI_ErrorSevere,
                                                     "Error from Component Thread in select");
            exit(1);
        } 
        else if ((FD_ISSET (pComponentPrivate->dataPipe[0], &rfds))) {
            WBAMRENC_DPRINT("%d :: DATA pipe is set in Component Thread\n",__LINE__);

            ret = read(pComponentPrivate->dataPipe[0], &pBufHeader, sizeof(pBufHeader));
            if (ret == -1) {
                WBAMRENC_EPRINT("%d :: Error while reading from the pipe\n",__LINE__);
                goto EXIT;
            }
            eError = WBAMRENC_HandleDataBufFromApp(pBufHeader,pComponentPrivate);
            if (eError != OMX_ErrorNone) {
                WBAMRENC_DPRINT("%d :: WBAMRENC_HandleDataBufFromApp returned error\n",__LINE__);
                break;
            }
        }
        else if(FD_ISSET (pComponentPrivate->cmdPipe[0], &rfds)) {
            /* Do not accept any command when the component is stopping */
            WBAMRENC_DPRINT("%d :: CMD pipe is set in Component Thread\n",__LINE__);
            ret = read(pComponentPrivate->cmdPipe[0], &command, sizeof (command));
            if (ret == -1) {
                WBAMRENC_EPRINT("%d :: Error in Reading from the Data pipe\n", __LINE__);
                eError = OMX_ErrorHardware;
                goto EXIT;
            }
            ret = read(pComponentPrivate->cmdDataPipe[0], &commandData, sizeof (commandData));
            if (ret == -1) {
                WBAMRENC_EPRINT("%d :: Error in Reading from the Data pipe\n", __LINE__);
                eError = OMX_ErrorHardware;
                goto EXIT;
            }
            nRet = WBAMRENC_HandleCommand(pComponentPrivate,command,commandData);
            if (nRet == WBAMRENC_EXIT_COMPONENT_THRD) {
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
                    pComponentPrivate->cbInfo.EventHandler( pHandle,
                                                            pHandle->pApplicationPrivate,
                                                            OMX_EventError,
                                                            OMX_ErrorResourcesLost,
                                                            OMX_TI_ErrorMajor,
                                                            NULL);
                    pComponentPrivate->bPreempted = 0;
                }
                    
            }
        }
    }
 EXIT:
#ifdef __PERF_INSTRUMENTATION__
    PERF_Done(pComponentPrivate->pPERFcomp);
#endif

    WBAMRENC_DPRINT("%d :: Exiting WBAMRENC_CompThread\n", __LINE__);
    WBAMRENC_DPRINT("%d :: Returning = 0x%x\n",__LINE__,eError);
    return (void*)OMX_ErrorNone;
}


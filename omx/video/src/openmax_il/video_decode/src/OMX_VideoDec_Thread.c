
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
* @file OMX_Video_Dec_Thread.c
*
* This file implements OMX Component for MPEG-4 decoder that 
* is fully compliant with the Khronos OMX specification 1.0.
*
* @path  $(CSLPATH)\src
*
* @rev  0.1
*/
/* -------------------------------------------------------------------------- */
/* ============================================================================= 
*! 
*! Revision History 
*! =============================================================================
*!
*! 02-Feb-2006 mf: Revisions appear in reverse chronological order; 
*! that is, newest first.  The date format is dd-Mon-yyyy.  
* =========================================================================== */

/* ------compilation control switches ----------------------------------------*/
/*******************************************************************************
*  INCLUDE FILES                                                 
*******************************************************************************/
/* ----- system and platform files -------------------------------------------*/
#define _XOPEN_SOURCE 600
    #include <wchar.h>    
    #include <sys/select.h>
    #include <signal.h>   
    #include <unistd.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <sys/ioctl.h>
    #include <fcntl.h>
    #include <errno.h>
     

#include <dbapi.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "OMX_VideoDecoder.h"
#include "OMX_VideoDec_Utils.h"
#include "OMX_VideoDec_Thread.h"
#include "OMX_VideoDec_DSP.h"
#define LOG_NDEBUG 0
#define LOG_TAG "OMX_VidDec_Thread"
#include <utils/Log.h>


/* Include functions useful for thread naming
 * */
#include <sys/prctl.h>


extern OMX_ERRORTYPE VIDDEC_HandleCommand (OMX_HANDLETYPE pHandle, OMX_U32 nParam1);
extern OMX_ERRORTYPE VIDDEC_DisablePort(VIDDEC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U32 nParam1);
extern OMX_ERRORTYPE VIDDEC_EnablePort(VIDDEC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U32 nParam1);
extern OMX_ERRORTYPE VIDDEC_HandleDataBuf_FromApp( VIDDEC_COMPONENT_PRIVATE *pComponentPrivate);
extern OMX_ERRORTYPE VIDDEC_HandleDataBuf_FromDsp( VIDDEC_COMPONENT_PRIVATE *pComponentPrivate );
extern OMX_ERRORTYPE VIDDEC_HandleFreeDataBuf( VIDDEC_COMPONENT_PRIVATE *pComponentPrivate );
extern OMX_ERRORTYPE VIDDEC_HandleFreeOutputBufferFromApp(VIDDEC_COMPONENT_PRIVATE *pComponentPrivate) ;
extern OMX_ERRORTYPE VIDDEC_Start_ComponentThread(OMX_HANDLETYPE pHandle);
extern OMX_ERRORTYPE VIDDEC_Stop_ComponentThread(OMX_HANDLETYPE pComponent);
extern OMX_ERRORTYPE VIDDEC_HandleCommandMarkBuffer(VIDDEC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U32 nParam1, OMX_PTR pCmdData);
extern OMX_ERRORTYPE VIDDEC_HandleCommandFlush(VIDDEC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U32 nParam1, OMX_BOOL bPass);
extern OMX_ERRORTYPE VIDDEC_Handle_InvalidState (VIDDEC_COMPONENT_PRIVATE* pComponentPrivate);
extern int pselect(int  n, fd_set*  readfds, fd_set*  writefds, fd_set*  errfds,
        const struct timespec*  timeout, const sigset_t*  sigmask);


/*----------------------------------------------------------------------------*/
/**
  * OMX_VidDec_Thread() is the open max thread. This method is in charge of
  * listening to the buffers coming from DSP, application or commands through the pipes
  **/
/*----------------------------------------------------------------------------*/

/** Default timeout used to come out of blocking calls*/
#define VIDD_TIMEOUT (1000) /* milliseconds */

void* OMX_VidDec_Thread (void* pThreadData)
{
    int status;
    sigset_t set;
    struct timespec tv;
    int fdmax;
    fd_set rfds;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMMANDTYPE eCmd;
    OMX_U32 nParam1;
    OMX_PTR pCmdData;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate;
    LCML_DSP_INTERFACE *pLcmlHandle;
    OMX_BOOL bFlag = OMX_FALSE;
    /* Set the thread's name
     * */
    prctl(PR_SET_NAME, (unsigned long) "OMX VIDDEC", 0, 0, 0);
    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pThreadData;

#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate->pPERFcomp = PERF_Create(PERF_FOURS("VD T"),
                                               PERF_ModuleComponent | PERF_ModuleVideoDecode);
#endif

    pLcmlHandle = (LCML_DSP_INTERFACE *)pComponentPrivate->pLCML;

    /**Looking for highest number of file descriptor for pipes in order to put in select loop */
    fdmax = pComponentPrivate->cmdPipe[VIDDEC_PIPE_READ];
    if (pComponentPrivate->bDynamicConfigurationInProgress) {
        if (pComponentPrivate->free_inpBuf_Q[VIDDEC_PIPE_READ] > fdmax) {
            fdmax = pComponentPrivate->free_inpBuf_Q[VIDDEC_PIPE_READ];
        }

        if (pComponentPrivate->free_outBuf_Q[VIDDEC_PIPE_READ] > fdmax) {
            fdmax = pComponentPrivate->free_outBuf_Q[VIDDEC_PIPE_READ];
        }

        if (pComponentPrivate->filled_inpBuf_Q[VIDDEC_PIPE_READ] > fdmax) {
            fdmax = pComponentPrivate->filled_inpBuf_Q[VIDDEC_PIPE_READ];
        }

        if (pComponentPrivate->filled_outBuf_Q[VIDDEC_PIPE_READ] > fdmax) {
            fdmax = pComponentPrivate->filled_outBuf_Q[VIDDEC_PIPE_READ];
        }
    }
    while (1) {
        FD_ZERO (&rfds);
        FD_SET(pComponentPrivate->cmdPipe[VIDDEC_PIPE_READ], &rfds);
        if (!pComponentPrivate->bDynamicConfigurationInProgress) {
            FD_SET(pComponentPrivate->filled_outBuf_Q[VIDDEC_PIPE_READ], &rfds);
            FD_SET(pComponentPrivate->free_inpBuf_Q[VIDDEC_PIPE_READ], &rfds);
            FD_SET(pComponentPrivate->free_outBuf_Q[VIDDEC_PIPE_READ], &rfds);
            FD_SET(pComponentPrivate->filled_inpBuf_Q[VIDDEC_PIPE_READ], &rfds);
        }
        tv.tv_sec = 0;
        tv.tv_nsec = 30000;


        sigemptyset (&set);
        sigaddset (&set, SIGALRM);
        status = pselect (fdmax+1, &rfds, NULL, NULL, NULL, &set);
        sigdelset (&set, SIGALRM);
        
        if (0 == status) {
            ;
        } 
        else if (-1 == status) {
            OMX_TRACE4(pComponentPrivate->dbg, "Error in Select\n");
            /*severity errors are greater to least, that is why of >*/
            /*it is to avoid overwriting a high severity error with one of less value*/
            if (pComponentPrivate->nLastErrorSeverity > OMX_TI_ErrorSevere) {
                pComponentPrivate->nLastErrorSeverity = OMX_TI_ErrorSevere;
            }
            pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                   OMX_EventError,
                                                   OMX_ErrorInsufficientResources, 
                                                   OMX_TI_ErrorSevere,
                                                   "Error from Component Thread in select");
	     eError = OMX_ErrorInsufficientResources;
         break;
        }
        else {
            if (FD_ISSET(pComponentPrivate->cmdPipe[VIDDEC_PIPE_READ], &rfds)) {
                if(!bFlag) {

                    bFlag = OMX_TRUE;
                    read(pComponentPrivate->cmdPipe[VIDDEC_PIPE_READ], &eCmd, sizeof(eCmd));
                    read(pComponentPrivate->cmdDataPipe[VIDDEC_PIPE_READ], &nParam1, sizeof(nParam1));

#ifdef __PERF_INSTRUMENTATION__
                    PERF_ReceivedCommand(pComponentPrivate->pPERFcomp,
                                         eCmd, nParam1, PERF_ModuleLLMM);
#endif
                    if (eCmd == OMX_CommandStateSet) {
                        if ((OMX_S32)nParam1 < -2) {
                            OMX_ERROR2(pComponentPrivate->dbg, "Incorrect variable value used\n");
                        }
                        if ((OMX_S32)nParam1 != -1 && (OMX_S32)nParam1 != -2) {
                            eError = VIDDEC_HandleCommand(pComponentPrivate, nParam1);
                            if (eError != OMX_ErrorNone) {
                             /* Do nothing
                              */
                            }
                        }
                        else if ((OMX_S32)nParam1 == -1) {
                            break;
                        }
                        else if ((OMX_S32)nParam1 == -2) {
                            OMX_VidDec_Return (pComponentPrivate, OMX_ALL, OMX_FALSE);
                            VIDDEC_Handle_InvalidState( pComponentPrivate);
                            break;
                        }
                    } 
                    else if (eCmd == OMX_CommandPortDisable) {
                        eError = VIDDEC_DisablePort(pComponentPrivate, nParam1);
                        if (eError != OMX_ErrorNone) {
                            pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                                   OMX_EventError,
                                                                   eError,
                                                                   OMX_TI_ErrorSevere,
                                                                   "Error in DisablePort function");
                        }
                    }
                    else if (eCmd == OMX_CommandPortEnable) {
                        eError = VIDDEC_EnablePort(pComponentPrivate, nParam1);
                        if (eError != OMX_ErrorNone) {
                            pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                                   OMX_EventError,
                                                                   eError,
                                                                   OMX_TI_ErrorSevere,
                                                                   "Error in EnablePort function");
                        }
                    } else if (eCmd == OMX_CommandFlush) {
                        eError = VIDDEC_HandleCommandFlush (pComponentPrivate, nParam1, OMX_TRUE);
                        if (eError != OMX_ErrorNone) {
                            pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                                   pComponentPrivate->pHandle->pApplicationPrivate,
                                                                   OMX_EventError,
                                                                   eError,
                                                                   OMX_TI_ErrorSevere,
                                                                   "Error in EnablePort function");
                        }
                    }
                    else if (eCmd == OMX_CommandMarkBuffer)    {
                        read(pComponentPrivate->cmdDataPipe[VIDDEC_PIPE_READ], &pCmdData, sizeof(pCmdData));
                        pComponentPrivate->arrCmdMarkBufIndex[pComponentPrivate->nInCmdMarkBufIndex].hMarkTargetComponent = ((OMX_MARKTYPE*)(pCmdData))->hMarkTargetComponent;
                        pComponentPrivate->arrCmdMarkBufIndex[pComponentPrivate->nInCmdMarkBufIndex].pMarkData = ((OMX_MARKTYPE*)(pCmdData))->pMarkData;
                        pComponentPrivate->nInCmdMarkBufIndex++;
                        pComponentPrivate->nInCmdMarkBufIndex %= VIDDEC_MAX_QUEUE_SIZE;

                    }
                    bFlag = OMX_FALSE;
                }
            }
            if(pComponentPrivate->bPipeCleaned){
                pComponentPrivate->bPipeCleaned =0;
            }
            else{
                if (FD_ISSET(pComponentPrivate->filled_outBuf_Q[VIDDEC_PIPE_READ], &rfds)) {
                    eError = VIDDEC_HandleDataBuf_FromDsp(pComponentPrivate);
                    if (eError != OMX_ErrorNone) {
                        OMX_PRBUFFER4(pComponentPrivate->dbg, "Error while handling filled DSP output buffer\n");
                        //recover
                        VIDDEC_FatalErrorRecover(pComponentPrivate);

                    }
                }
                if (FD_ISSET(pComponentPrivate->free_inpBuf_Q[VIDDEC_PIPE_READ], &rfds)) {
                    eError = VIDDEC_HandleFreeDataBuf(pComponentPrivate);
                    if (eError != OMX_ErrorNone) {
                        OMX_PRBUFFER4(pComponentPrivate->dbg, "Error while processing free input buffers\n");
                        //recover
                        VIDDEC_FatalErrorRecover(pComponentPrivate);
                    }
                }
                if (pComponentPrivate->bDynamicConfigurationInProgress) {
                    continue;
                }
                if ((FD_ISSET(pComponentPrivate->filled_inpBuf_Q[VIDDEC_PIPE_READ], &rfds))){
                    OMX_PRSTATE2(pComponentPrivate->dbg, "eExecuteToIdle 0x%x\n",pComponentPrivate->eExecuteToIdle);
                    /* When doing a reconfiguration, don't send input buffers to SN & wait for SN to be ready*/
                    eError = VIDDEC_HandleDataBuf_FromApp (pComponentPrivate);
                    if (eError != OMX_ErrorNone) {
                        OMX_PRBUFFER4(pComponentPrivate->dbg, "Error while handling filled input buffer\n");
                        //recover
                        VIDDEC_FatalErrorRecover(pComponentPrivate);
                    }
                }
                if (pComponentPrivate->bDynamicConfigurationInProgress || !pComponentPrivate->bFirstHeader) {
                    continue;
                }
                if (FD_ISSET(pComponentPrivate->free_outBuf_Q[VIDDEC_PIPE_READ], &rfds)) {
                    eError = VIDDEC_HandleFreeOutputBufferFromApp(pComponentPrivate);
                    if (eError != OMX_ErrorNone) {
                        OMX_PRBUFFER4(pComponentPrivate->dbg, "Error while processing free output buffer\n");
                        //recover
                        VIDDEC_FatalErrorRecover(pComponentPrivate);
                    }
                }
            }
        }
    }

#ifdef __PERF_INSTRUMENTATION__
    PERF_Done(pComponentPrivate->pPERFcomp);
#endif

    return (void *)eError;
}

void* OMX_VidDec_Return (void* pThreadData, OMX_U32 nPortId, OMX_BOOL bReturnOnlyOne)
{
    int status = 0;
    struct timeval tv1;
    sigset_t set;
    struct timespec tv;
    int fdmax = 0;
    OMX_U32 iLock = 0;
    fd_set rfds;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    VIDDEC_COMPONENT_PRIVATE* pComponentPrivate = NULL;

    pComponentPrivate = (VIDDEC_COMPONENT_PRIVATE*)pThreadData;
    OMX_PRINT1(pComponentPrivate->dbg, "+++ENTERING Port #%ld\n", nPortId);
    gettimeofday(&tv1, NULL);
    if ( nPortId == VIDDEC_INPUT_PORT || nPortId == OMX_ALL) {
        /*Looking for highest number of file descriptor for pipes in order to put in select loop */
        fdmax = pComponentPrivate->free_inpBuf_Q[VIDDEC_PIPE_READ];

        if (pComponentPrivate->filled_inpBuf_Q[VIDDEC_PIPE_READ] > fdmax) {
            fdmax = pComponentPrivate->filled_inpBuf_Q[VIDDEC_PIPE_READ];
        }

        /*remove extra parameters*/
        OMX_PRINT1(pComponentPrivate->dbg,
            "Enter nCInBFApp %ld nCInBFDsp %ld\n",
            pComponentPrivate->nCountInputBFromApp,
            pComponentPrivate->nCountInputBFromDsp);
        while (pComponentPrivate->nCountInputBFromApp != 0 ||
            pComponentPrivate->nCountInputBFromDsp != 0) {
            FD_ZERO (&rfds);
            FD_SET(pComponentPrivate->free_inpBuf_Q[VIDDEC_PIPE_READ], &rfds);
            FD_SET(pComponentPrivate->filled_inpBuf_Q[VIDDEC_PIPE_READ], &rfds);

            tv.tv_sec = 0;
            tv.tv_nsec = 10000;

            sigemptyset (&set);
            sigaddset (&set, SIGALRM);
            status = pselect (fdmax+1, &rfds, NULL, NULL, &tv, &set);
            sigdelset (&set, SIGALRM);
            if (0 == status) {
                OMX_PRINT2(pComponentPrivate->dbg, "Pselect status 0\n");
                iLock++;
                if (iLock > 2){
                    pComponentPrivate->bPipeCleaned = 1;
                    break;
                }
            }
            else if (-1 == status) {
                OMX_PRINT2(pComponentPrivate->dbg, "Error in Select\n");
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                                       OMX_EventError,
                                                       OMX_ErrorInsufficientResources,
                                                       OMX_TI_ErrorSevere,
                                                       "Error from Component Thread in select");
                eError = OMX_ErrorInsufficientResources;
                break;
            }
            else {
                if (FD_ISSET(pComponentPrivate->free_inpBuf_Q[VIDDEC_PIPE_READ], &rfds) && !bReturnOnlyOne) {
                    eError = VIDDEC_HandleFreeDataBuf (pComponentPrivate);
                    if (eError != OMX_ErrorNone) {
                        OMX_PRBUFFER4(pComponentPrivate->dbg, "Error while handling free input buffer\n");
                        //recover
                        VIDDEC_FatalErrorRecover(pComponentPrivate);
                    }
                    pComponentPrivate->bPipeCleaned = OMX_TRUE;
                    /*doing continue to return buffers from DSP and then component ones*/
                    /*in order to keep buffer order*/
                    continue;
                }
                if ((FD_ISSET(pComponentPrivate->filled_inpBuf_Q[VIDDEC_PIPE_READ], &rfds))) {
                    eError = VIDDEC_HandleDataBuf_FromApp (pComponentPrivate);
                    if (eError != OMX_ErrorNone) {
                        OMX_PRBUFFER4(pComponentPrivate->dbg, "Error while handling filled input buffer\n");
                        //recover
                        VIDDEC_FatalErrorRecover(pComponentPrivate);
                    }
                    pComponentPrivate->bPipeCleaned = OMX_TRUE;
                    /*doing continue to return buffers from DSP and then component ones*/
                    /*in order to keep buffer order*/
                    if (bReturnOnlyOne) {
                        break;
                    }
                    continue;
                }
            }
        }
        OMX_PRINT1(pComponentPrivate->dbg,
            "Exit nCInBFApp %ld nCInBFDsp %ld\n",
            pComponentPrivate->nCountInputBFromApp,
            pComponentPrivate->nCountInputBFromDsp);
    }

    if ((nPortId == VIDDEC_OUTPUT_PORT || nPortId == OMX_ALL)) {
        /*Looking for highest number of file descriptor for pipes in order to put in select loop */
        fdmax = pComponentPrivate->free_outBuf_Q[VIDDEC_PIPE_READ];

        if (pComponentPrivate->filled_outBuf_Q[VIDDEC_PIPE_READ] > fdmax) {
            fdmax = pComponentPrivate->filled_outBuf_Q[VIDDEC_PIPE_READ];
        }
        
        OMX_PRINT1(pComponentPrivate->dbg,
            "Enter nCOutBFDsp %ld nCOutBFApp %ld\n",
            pComponentPrivate->nCountOutputBFromDsp,
            pComponentPrivate->nCountOutputBFromApp);
        while (pComponentPrivate->nCountOutputBFromApp != 0 ||
            pComponentPrivate->nCountOutputBFromDsp != 0) {
            FD_ZERO (&rfds);
            FD_SET(pComponentPrivate->free_outBuf_Q[VIDDEC_PIPE_READ], &rfds);
            FD_SET(pComponentPrivate->filled_outBuf_Q[VIDDEC_PIPE_READ], &rfds);
            tv.tv_sec = 0;
            tv.tv_nsec = 10000;
            sigemptyset (&set);
            sigaddset (&set, SIGALRM);
            status = pselect (fdmax+1, &rfds, NULL, NULL, &tv, &set);
            sigdelset (&set, SIGALRM);
            if (0 == status) {
                iLock++;
                if (iLock > 2){
                    pComponentPrivate->bPipeCleaned = 1;
                    break;
                }
            }
            else if (-1 == status) {
                OMX_PRINT2(pComponentPrivate->dbg, "Error in Select\n");
                pComponentPrivate->cbInfo.EventHandler(pComponentPrivate->pHandle,
                                                       pComponentPrivate->pHandle->pApplicationPrivate,
                                                       OMX_EventError,
                                                       OMX_ErrorInsufficientResources,
                                                       OMX_TI_ErrorSevere,
                                                       "Error from Component Thread in select");
                eError = OMX_ErrorInsufficientResources;
                break;
            }
            else {
                if (FD_ISSET(pComponentPrivate->filled_outBuf_Q[VIDDEC_PIPE_READ], &rfds) && !bReturnOnlyOne) {
                    eError = VIDDEC_HandleDataBuf_FromDsp (pComponentPrivate);
                    if (eError != OMX_ErrorNone) {
                        OMX_PRBUFFER4(pComponentPrivate->dbg, "Error while handling filled DSP output buffer\n");
                        //recover
                        VIDDEC_FatalErrorRecover(pComponentPrivate);
                    }
                    pComponentPrivate->bPipeCleaned = OMX_TRUE;
                    /*doing continue to return buffers from DSP and then component ones*/
                    /*in order to keep buffer order*/
                    continue;
                }
                if (FD_ISSET(pComponentPrivate->free_outBuf_Q[VIDDEC_PIPE_READ], &rfds)) {
                    OMX_PRSTATE2(pComponentPrivate->dbg, "eExecuteToIdle 0x%x\n",pComponentPrivate->eExecuteToIdle);
                    eError = VIDDEC_HandleFreeOutputBufferFromApp (pComponentPrivate);
                    if (eError != OMX_ErrorNone) {
                        OMX_PRBUFFER4(pComponentPrivate->dbg, "Error while processing free output buffer\n");
                        //recover
                        VIDDEC_FatalErrorRecover(pComponentPrivate);
                    }
                    pComponentPrivate->bPipeCleaned = OMX_TRUE;
                    /*doing continue to return buffers from DSP and then component ones*/
                    /*in order to keep buffer order*/
                    if (bReturnOnlyOne) {
                        break;
                    }
                    continue;
                }
            }
        }
        OMX_PRINT1(pComponentPrivate->dbg,
            "Exit nCOutBFDsp %ld nCOutBFApp %ld\n",
            pComponentPrivate->nCountOutputBFromDsp,
            pComponentPrivate->nCountOutputBFromApp);
        pComponentPrivate->bPipeCleaned = OMX_TRUE;
    }
    OMX_PRINT1(pComponentPrivate->dbg, "---Exiting(0x%x)\n", eError);
    return (void *)eError;
}


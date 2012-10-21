
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
* @file OMX_VideoEnc_Thread.c
*
* This file implements OMX Component for MPEG-4 encoder that
* is fully compliant with the OMX specification 1.5.
*
* @path  $(CSLPATH)\src
*
* @rev  0.1
*/
/* -------------------------------------------------------------------------- */
/* =============================================================================
*!
*! Revision History
*! ================================================================
*!
*! 02-Feb-2006 mf: Revisions appear in reverse chronological order;
*! that is, newest first.  The date format is dd-Mon-yyyy.
* =========================================================================== */


/* ------compilation control switches ----------------------------------------*/
/******************************************************************************
*  INCLUDE FILES
*******************************************************************************/
/* ----- system and platform files -------------------------------------------*/
#ifdef UNDER_CE
    #include <windows.h>
    #include <oaf_osal.h>
    #include <omx_core.h>
#else
    #define _XOPEN_SOURCE 600
    #include <wchar.h>
    #include <unistd.h>
    #include <sys/select.h>
    #include <errno.h>
    #include <fcntl.h>
#endif

#include <dbapi.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
/* Include functions useful for thread naming
 * */
#include <sys/prctl.h>

/*------- Program Header Files -----------------------------------------------*/
#include "OMX_VideoEnc_Utils.h"

/******************************************************************************
*  EXTERNAL REFERENCES NOTE : only use if not found in header file
*******************************************************************************/
/*--------data declarations --------------------------------------------------*/

/*--------function prototypes ------------------------------------------------*/

/******************************************************************************
*  PUBLIC DECLARATIONS Defined here, used elsewhere
*******************************************************************************/
/*--------data declarations --------------------------------------------------*/

/*--------function prototypes ------------------------------------------------*/

/******************************************************************************
*  PRIVATE DECLARATIONS Defined here, used only here
*******************************************************************************/
/*--------data declarations --------------------------------------------------*/

/*--------macro definitions --------------------------------------------------*/

/*--------function prototypes ------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  * OMX_VideoEnc_Thread()
  *
  * Called by VIDENC_Start_ComponentThread function.
  *
  * @param pThreadData
  *
  * @retval OMX_ErrorNone                  success, ready to roll
  *         OMX_ErrorInsufficientResources if the malloc fails
  **/
/*----------------------------------------------------------------------------*/
void* OMX_VIDENC_Thread (void* pThreadData)
{
    int status = -1;
    int fdmax = -1;
    fd_set rfds;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_COMMANDTYPE eCmd = -1;
    OMX_U32 nParam1;
    int nRet = -1;
    OMX_PTR pCmdData = NULL;
    VIDENC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    LCML_DSP_INTERFACE* pLcmlHandle = NULL;
    sigset_t set;
    /* Set the thread's name
     * */
    prctl(PR_SET_NAME, (unsigned long) "OMX VIDENC", 0, 0, 0);
    if (!pThreadData)
    {
        eError = OMX_ErrorBadParameter;
        goto OMX_CONF_CMD_BAIL;
    }

    pComponentPrivate = (VIDENC_COMPONENT_PRIVATE*)pThreadData;
    pLcmlHandle = (LCML_DSP_INTERFACE*)pComponentPrivate->pLCML;

#ifdef __PERF_INSTRUMENTATION__
    pComponentPrivate->pPERFcomp = PERF_Create(PERF_FOURCC('V', 'E', ' ', 'T'),
                                               PERF_ModuleComponent |
                                               PERF_ModuleVideoEncode);
#endif

    /** Looking for highest number of file descriptor
        for pipes inorder to put in select loop */

    fdmax = pComponentPrivate->nCmdPipe[0];

    if (pComponentPrivate->nFree_oPipe[0] > fdmax)
    {
        fdmax = pComponentPrivate->nFree_oPipe[0];
    }

    if (pComponentPrivate->nFilled_iPipe[0] > fdmax)
    {
        fdmax = pComponentPrivate->nFilled_iPipe[0];
    }
    pComponentPrivate->bPipeCleaned = OMX_FALSE;
    while (1)
    {
        if(pComponentPrivate->bInInvalidState == 1) {
            pComponentPrivate->bInInvalidState++;
            OMX_ERROR2(pComponentPrivate->dbg, "  --VE in invalid state detected\n");
            OMX_VIDENC_FatalErrorRecover(pComponentPrivate);
        }

        FD_ZERO (&rfds);
        FD_SET (pComponentPrivate->nCmdPipe[0], &rfds);
        FD_SET (pComponentPrivate->nFree_oPipe[0], &rfds);
        FD_SET (pComponentPrivate->nFilled_iPipe[0], &rfds);

        sigemptyset(&set);
        sigaddset(&set,SIGALRM);
        status = pselect(fdmax+1, &rfds, NULL, NULL, NULL,&set);

        if (0 == status)
        {
            OMX_TRACE2(pComponentPrivate->dbg, "pselect() = 0\n");
#ifndef UNDER_CE
            sched_yield();
#else
            sched_yield();
#endif
        }
        else if (-1 == status)
        {
            eError = OMX_ErrorInvalidState;
            OMX_TRACE3(pComponentPrivate->dbg, "select() error.\n");
            OMX_VIDENC_HandleError(pComponentPrivate, eError);
        }
        else
        {
            if (FD_ISSET(pComponentPrivate->nCmdPipe[0], &rfds))
            {
                nRet = read(pComponentPrivate->nCmdPipe[0],
                            &eCmd,
                            sizeof(eCmd));
                if (nRet == -1)
                {
                    OMX_PRCOMM4(pComponentPrivate->dbg, "Error while reading from cmdPipe\n");
                    OMX_VIDENC_HandleError(pComponentPrivate, OMX_ErrorHardware);
                }

#ifdef __PERF_INSTRUMENTATION__
                        PERF_ReceivedCommand(pComponentPrivate->pPERFcomp,
                                             eCmd, 0, PERF_ModuleLLMM);
#endif
                if (eCmd == (OMX_COMMANDTYPE)-1)
                {
                    OMX_PRCOMM2(pComponentPrivate->dbg, "Received thread close command.\n");
                    OMX_CONF_SET_ERROR_BAIL(eError, OMX_ErrorNone);
                }

                /* dummy command to process fatal error event */
                if (eCmd == (OMX_COMMANDTYPE)VIDENC_FATAL_ERROR_COMMAND)
                {
                    continue;
                }
                if (eCmd == OMX_CommandMarkBuffer)
                {
                    nRet = read(pComponentPrivate->nCmdDataPipe[0],
                                &pCmdData,
                                sizeof(pCmdData));
                    if (nRet == -1)
                    {
                        OMX_PRCOMM4(pComponentPrivate->dbg, "Error while reading from cmdDataPipe\n");
                        OMX_VIDENC_HandleError(pComponentPrivate, OMX_ErrorHardware);
                    }
                }
                else
                {
                    nRet = read(pComponentPrivate->nCmdDataPipe[0],
                                &nParam1,
                                sizeof(nParam1));
                    if (nRet == -1)
                    {
                        OMX_PRCOMM4(pComponentPrivate->dbg, "Error while reading from cmdDataPipe\n");
                        OMX_VIDENC_HandleError(pComponentPrivate, OMX_ErrorHardware);
                    }
                }

#ifdef __PERF_INSTRUMENTATION__
                    PERF_ReceivedCommand(pComponentPrivate->pPERFcomp,
                                         eCmd,
                                         (eCmd == OMX_CommandMarkBuffer) ? ((OMX_U32) pCmdData) : nParam1,
                                         PERF_ModuleLLMM);
#endif

                switch (eCmd)
                {
                    case OMX_CommandStateSet :
                    OMX_PRSTATE2(pComponentPrivate->dbg, "Enters OMX_VIDENC_HandleCommandStateSet\n");
                        eError = OMX_VIDENC_HandleCommandStateSet(pComponentPrivate,
                                                                  nParam1);
                        if(eError != OMX_ErrorNone) {
                            OMX_VIDENC_HandleError(pComponentPrivate, eError);
                        }
                    OMX_PRSTATE2(pComponentPrivate->dbg, "Exits OMX_VIDENC_HandleCommandStateSet\n");
                        break;
                    case OMX_CommandFlush :
                    OMX_PRSTATE2(pComponentPrivate->dbg, "Enters OMX_VIDENC_HandleCommandFlush\n");
                        eError = OMX_VIDENC_HandleCommandFlush(pComponentPrivate,
                                                               nParam1,
                                                               OMX_FALSE);
                        if(eError != OMX_ErrorNone) {
                            OMX_VIDENC_HandleError(pComponentPrivate, eError);
                        }
                    OMX_PRSTATE2(pComponentPrivate->dbg, "Exits OMX_VIDENC_HandleCommandFlush\n");
                        break;
                    case OMX_CommandPortDisable :
                    OMX_PRSTATE2(pComponentPrivate->dbg, "Enters OMX_VIDENC_HandleCommandDisablePort\n");
                        eError = OMX_VIDENC_HandleCommandDisablePort(pComponentPrivate,
                                                                     nParam1);
                        if(eError != OMX_ErrorNone) {
                            OMX_VIDENC_HandleError(pComponentPrivate, eError);
                        }
                    OMX_PRSTATE2(pComponentPrivate->dbg, "Exits OMX_VIDENC_HandleCommandDisablePort\n");
                        break;
                    case OMX_CommandPortEnable :
                    OMX_PRSTATE2(pComponentPrivate->dbg, "Enters OMX_VIDENC_HandleCommandDisablePort\n");
                        eError = OMX_VIDENC_HandleCommandEnablePort(pComponentPrivate,
                                                                    nParam1);
                        if(eError != OMX_ErrorNone) {
                            OMX_VIDENC_HandleError(pComponentPrivate, eError);
                        }
                    OMX_PRSTATE2(pComponentPrivate->dbg, "Exits OMX_VIDENC_HandleCommandDisablePort\n");
                        break;
                    case OMX_CommandMarkBuffer :
                        if (!pComponentPrivate->pMarkBuf)
                        {
                            pComponentPrivate->pMarkBuf = (OMX_MARKTYPE*)(pCmdData);
                        }
                        break;
                    default:
                        OMX_VIDENC_EVENT_HANDLER(pComponentPrivate,
                                                 OMX_EventError,
                                                 OMX_ErrorUndefined,
                                                 0,
                                                 NULL);
                }
            }

            if (!pComponentPrivate->bPipeCleaned) {
                if ((FD_ISSET(pComponentPrivate->nFilled_iPipe[0], &rfds)) &&
                    (pComponentPrivate->eState != OMX_StatePause &&
                     pComponentPrivate->eState != OMX_StateIdle &&
                     pComponentPrivate->eState != OMX_StateLoaded))
                {
                    OMX_PRBUFFER1(pComponentPrivate->dbg, "Enters OMX_VIDENC_Process_FilledInBuf\n");
                    eError = OMX_VIDENC_Process_FilledInBuf(pComponentPrivate);
                    if(eError != OMX_ErrorNone) {
                        OMX_VIDENC_HandleError(pComponentPrivate, eError);
                    }
                    OMX_PRBUFFER1(pComponentPrivate->dbg, "Exits OMX_VIDENC_Process_FilledInBuf\n");
                }

                if (FD_ISSET(pComponentPrivate->nFree_oPipe[0], &rfds) &&
                    (pComponentPrivate->eState != OMX_StatePause &&
                     pComponentPrivate->eState != OMX_StateIdle &&
                     pComponentPrivate->eState != OMX_StateLoaded))
                {
                    OMX_PRBUFFER1(pComponentPrivate->dbg, "Enters OMX_VIDENC_Process_FreeOutBuf\n");
                    eError = OMX_VIDENC_Process_FreeOutBuf(pComponentPrivate);
                    if(eError != OMX_ErrorNone) {
                        OMX_VIDENC_HandleError(pComponentPrivate, eError);
                    }
                    OMX_PRBUFFER1(pComponentPrivate->dbg, "Exits OMX_VIDENC_Process_FreeOutBuf\n");
                }
            }
            else {
                pComponentPrivate->bPipeCleaned = OMX_FALSE;
            }
        }
    }

OMX_CONF_CMD_BAIL:

#ifdef __PERF_INSTRUMENTATION__
    if (pComponentPrivate)
        PERF_Done(pComponentPrivate->pPERFcomp);
#endif
    if (pComponentPrivate)
        OMX_PRINT2(pComponentPrivate->dbg, "Component Thread Exits\n");
    return (void*)eError;
}

void* OMX_VIDENC_Return (void* pThreadData, OMX_U32 nPortId)
{
    int status = -1;
    int fdmax = -1;
    fd_set rfds;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int nRet = -1;
    VIDENC_COMPONENT_PRIVATE* pComponentPrivate = NULL;
    sigset_t set;
    struct timespec tv;
    /* Set the thread's name
     * */
    pComponentPrivate = (VIDENC_COMPONENT_PRIVATE*)pThreadData;

    /** Looking for highest number of file descriptor
        for pipes inorder to put in select loop */
    if ( nPortId == 0 || nPortId == -1) {
        fdmax = pComponentPrivate->nFilled_iPipe[0];

        while (1)
        {
            FD_ZERO (&rfds);
            FD_SET (pComponentPrivate->nFilled_iPipe[0], &rfds);

            tv.tv_sec = 0;
            tv.tv_nsec = 10000;
            sigemptyset(&set);
            sigaddset(&set,SIGALRM);
            status = pselect(fdmax+1, &rfds, NULL, NULL, &tv, &set);

            if (0 == status)
            {
                OMX_TRACE2(pComponentPrivate->dbg, "pselect() = 0\n");
                break;
            }
            else if (-1 == status)
            {
                break;
            }
            else
            {
                if ((FD_ISSET(pComponentPrivate->nFilled_iPipe[0], &rfds)))
                {
                    OMX_PRBUFFER1(pComponentPrivate->dbg, "Enters OMX_VIDENC_Process_FilledInBuf\n");
                    eError = OMX_VIDENC_Process_FilledInBuf(pComponentPrivate);
                    if(eError != OMX_ErrorNone) {
                        OMX_VIDENC_HandleError(pComponentPrivate, eError);
                break;
                    }
                    OMX_PRBUFFER1(pComponentPrivate->dbg, "Exits OMX_VIDENC_Process_FilledInBuf\n");
                    pComponentPrivate->bPipeCleaned = OMX_TRUE;
                }
            }
        }
    }
    if ( nPortId == 1 || nPortId == -1) {
        fdmax = pComponentPrivate->nFree_oPipe[0];

        while (1)
        {
            FD_ZERO (&rfds);
            FD_SET (pComponentPrivate->nFree_oPipe[0], &rfds);

            tv.tv_sec = 0;
            tv.tv_nsec = 10000;
            sigemptyset(&set);
            sigaddset(&set,SIGALRM);
            status = pselect(fdmax+1, &rfds, NULL, NULL, &tv, &set);

            if (0 == status)
            {
                OMX_TRACE2(pComponentPrivate->dbg, "pselect() = 0\n");
                break;
            }
            else if (-1 == status)
            {
                break;
            }
            else
            {
                if (FD_ISSET(pComponentPrivate->nFree_oPipe[0], &rfds))
                {
                    OMX_PRBUFFER1(pComponentPrivate->dbg, "Enters OMX_VIDENC_Process_FreeOutBuf\n");
                    eError = OMX_VIDENC_Process_FreeOutBuf(pComponentPrivate);
                    if(eError != OMX_ErrorNone) {
                        OMX_VIDENC_HandleError(pComponentPrivate, eError);
                break;
                    }
                    OMX_PRBUFFER1(pComponentPrivate->dbg, "Exits OMX_VIDENC_Process_FreeOutBuf\n");
                    pComponentPrivate->bPipeCleaned = OMX_TRUE;
                }
            }
        }
    }
OMX_CONF_CMD_BAIL:
    return (void*)OMX_ErrorNone;
}

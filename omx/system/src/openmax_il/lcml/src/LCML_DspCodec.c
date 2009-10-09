
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
/* ====================================================================
*             Texas Instruments OMAP(TM) Platform Software
* (c) Copyright Texas Instruments, Incorporated. All Rights Reserved.
*
* Use of this software is controlled by the terms and conditions found
* in the license agreement under which this software has been supplied.
* ==================================================================== */
/**
* @file LCML_DspCodec.c
*
* This file implements LCML for Linux 8.x
*
* @path  $(CSLPATH)\
*
* @rev  1.0
*/
/* ----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*!
*!
* ============================================================================= */

#ifdef UNDER_CE
    #include <windows.h>
#else
    #include <errno.h>
#endif

#include <pthread.h>

/* Common WinCE and Linux Headers */
#include "LCML_DspCodec.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "usn.h"
#include <sys/time.h>

#define CEXEC_DONE 1
/*DSP_HNODE hDasfNode;*/
#define ABS_DLL_NAME_LENGTH 128


#define LCML_MALLOC(p,s,t) \
    p = (t*)malloc(s);                  \
    if (NULL == p){             \
        LCML_DPRINT("LCML:::::::: ERROR(#%d F:%s)!!! Ran out of memory while trying to allocate %d bytes!!!\n",__LINE__,__FUNCTION__,s);    \
    }else { \
        LCML_DPRINT("LCML:::::::: (#%d F:%s)Success to allocate %d bytes ... pointer %p\n",__LINE__,__FUNCTION__,s,p); \
    }

#define LCML_FREE(p)    \
        LCML_DPRINT("LCML:::::::: (#%d F:%s)Freeing pointer %p done",__LINE__,__FUNCTION__,p); \
        free(p);    

/*Prototyping*/
static OMX_ERRORTYPE InitMMCodec(OMX_HANDLETYPE hInt,
                                 OMX_STRING  codecName,
                                 void *toCodecInitParams,
                                 void *fromCodecInfoStruct,
                                 LCML_CALLBACKTYPE *pCallbacks);
static OMX_ERRORTYPE InitMMCodecEx(OMX_HANDLETYPE hInt,
                                   OMX_STRING  codecName,
                                   void *toCodecInitParams,
                                   void *fromCodecInfoStruct,
                                   LCML_CALLBACKTYPE *pCallbacks,
                                   OMX_STRING  Args);
static OMX_ERRORTYPE WaitForEvent(OMX_HANDLETYPE hComponent,
                                  TUsnCodecEvent event,
                                  void *args[10]);
static OMX_ERRORTYPE QueueBuffer(OMX_HANDLETYPE hComponent,
                                 TMMCodecBufferType bufType,
                                 OMX_U8 *buffer,
                                 OMX_S32 bufferLen,
                                 OMX_S32 bufferSizeUsed ,
                                 OMX_U8 *auxInfo,
                                 OMX_S32 auxInfoLen,
                                 OMX_U8 *usrArg);
static OMX_ERRORTYPE ControlCodec(OMX_HANDLETYPE hComponent,
                                  TControlCmd iCodecCmd,
                                  void *args[10]);
static OMX_ERRORTYPE DmmMap(DSP_HPROCESSOR ProcHandle,
                     OMX_U32 size,
                     OMX_U32 sizeUsed,
                     void* pArmPtr,
                     DMM_BUFFER_OBJ* pDmmBuf,
                     OMX_U32 MapBufLen);
                     
static OMX_ERRORTYPE DmmUnMap(DSP_HPROCESSOR ProcHandle,
                              void *pMapPtr,
                              void *pResPtr);
static OMX_ERRORTYPE DeleteDspResource(LCML_DSP_INTERFACE *hInterface);
static OMX_ERRORTYPE FreeResources(LCML_DSP_INTERFACE *hInterface);

void* MessagingThread(void *arg);

static int append_dsp_path(char * dll64p_name, char *absDLLname);


/** ========================================================================
*  GetHandle function is called by OMX component to get LCML handle
*  @param hInterface - Handle of the component to be accessed
*  @return OMX_ERRORTYPE
*      If the command successfully executes, the return code will be
*      OMX_NoError.  Otherwise the appropriate OMX error will be returned.
** =========================================================================*/
OMX_ERRORTYPE GetHandle(OMX_HANDLETYPE *hInterface )
{
    OMX_ERRORTYPE err = 0 ;
    LCML_DSP_INTERFACE* pHandle;
    struct LCML_CODEC_INTERFACE *dspcodecinterface ;

    LCML_DPRINT("%d :: GetHandle application\n",__LINE__);
     LCML_MALLOC(*hInterface,sizeof(LCML_DSP_INTERFACE),LCML_DSP_INTERFACE);

    if (hInterface == NULL)
    {
        err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    memset(*hInterface, 0, sizeof(LCML_DSP_INTERFACE));

    pHandle = (LCML_DSP_INTERFACE*)*hInterface;

    LCML_MALLOC(dspcodecinterface,sizeof(LCML_CODEC_INTERFACE),LCML_CODEC_INTERFACE);
    if (dspcodecinterface == NULL)
    {
        err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    memset(dspcodecinterface, 0, sizeof(LCML_CODEC_INTERFACE));

    pHandle->pCodecinterfacehandle = dspcodecinterface;
    dspcodecinterface->InitMMCodec = InitMMCodec;
    dspcodecinterface->InitMMCodecEx = InitMMCodecEx;
    dspcodecinterface->WaitForEvent = WaitForEvent;
    dspcodecinterface->QueueBuffer = QueueBuffer;
    dspcodecinterface->ControlCodec = ControlCodec;

    LCML_MALLOC(pHandle->dspCodec,sizeof(LCML_DSP),LCML_DSP);
    if(pHandle->dspCodec == NULL)
    {
        err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    memset(pHandle->dspCodec, 0, sizeof(LCML_DSP));

    pthread_mutex_init (&pHandle->mutex, NULL);
    LCML_DPRINT("%p :: GetHandle application\n",pHandle);
    dspcodecinterface->pCodec = *hInterface;
    LCML_DPRINT("%p :: GetHandle application\n",pHandle->dspCodec);
EXIT:
    return (err);
}



/** ========================================================================
*  InitMMCodecEx initialise the OMX Component specific handle to LCML.
*  The memory is allocated and the dsp node is created. Add notification object
*  to listener thread.
*
*  @param  hInterface - Handle to LCML which is allocated and filled
*  @param  codecName - not used
*  @param  toCodecInitParams - not used yet
*  @param  fromCodecInfoStruct - not used yet
*  @param  pCallbacks - List of callback that uses to call OMX
*  @param  Args - additional arguments
*  @return OMX_ERRORTYPE
*      If the command successfully executes, the return code will be
*      OMX_NoError.  Otherwise the appropriate OMX error will be returned.
* ==========================================================================*/
static OMX_ERRORTYPE InitMMCodecEx(OMX_HANDLETYPE hInt,
                                   OMX_STRING codecName,
                                   void * toCodecInitParams,
                                   void * fromCodecInfoStruct,
                                   LCML_CALLBACKTYPE *pCallbacks,
                                   OMX_STRING Args)

{

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 dllinfo;

    LCML_DPRINT("%d :: InitMMCodecEx application\n", __LINE__);

    if (hInt == NULL )
    {
        eError = OMX_ErrorInsufficientResources;
        goto ERROR;
    }
    if(Args ==NULL)
    {
        InitMMCodec(hInt, codecName, toCodecInitParams, fromCodecInfoStruct, pCallbacks);
    }
    else
    {
        LCML_DSP_INTERFACE * phandle;
        LCML_CREATEPHASEARGS crData;
        DSP_STATUS status;
        int i = 0, k = 0;
        struct DSP_NODEATTRIN NodeAttrIn;
        struct DSP_CBDATA     *pArgs;
        BYTE  argsBuf[32 + sizeof(ULONG)];
        char abs_dsp_path[ABS_DLL_NAME_LENGTH];
#ifndef CEXEC_DONE
        UINT argc = 1;
        char argv[ABS_DLL_NAME_LENGTH];
        k = append_dsp_path(DSP_DOF_IMAGE, argv);
        if (k < 0)
        {
            LCML_DPRINT("%d :: append_dsp_path returned an error!\n", __LINE__);
            eError = OMX_ErrorBadParameter;
            goto ERROR;
        }
#endif
        int tmperr;

        phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)hInt)->pCodec);

#ifdef __PERF_INSTRUMENTATION__
        phandle->pPERF = PERF_Create(PERF_FOURCC('C','M','L',' '),
                                 PERF_ModuleAudioDecode | PERF_ModuleAudioEncode |
                                 PERF_ModuleVideoDecode | PERF_ModuleVideoEncode |
                                 PERF_ModuleImageDecode | PERF_ModuleImageEncode |
                                 PERF_ModuleCommonLayer);
        PERF_Boundary(phandle->pPERF,
                      PERF_BoundaryStart | PERF_BoundarySetup);
#endif
        /* INIT DSP RESOURCE */
        if(pCallbacks)
            phandle->dspCodec->Callbacks.LCML_Callback = pCallbacks->LCML_Callback;
        else
        {
            eError = OMX_ErrorBadParameter;
            goto ERROR;
        }

        /* INITIALIZATION OF DSP */
        LCML_DPRINT("%d :: Entering Init_DSPSubSystem\n", __LINE__);
        status = DspManager_Open(0, NULL);
        DSP_ERROR_EXIT(status, "DSP Manager Open", ERROR);
        LCML_DPRINT("DspManager_Open Successful\n");

        /* Attach and get handle to processor */
        status = DSPProcessor_Attach(TI_PROCESSOR_DSP, NULL, &(phandle->dspCodec->hProc));
        DSP_ERROR_EXIT(status, "Attach processor", ERROR);
        LCML_DPRINT("DSPProcessor_Attach Successful\n");
        LCML_DPRINT("Base Image is Already Loaded\n");

        for (dllinfo=0; dllinfo < phandle->dspCodec->NodeInfo.nNumOfDLLs; dllinfo++)
        {
            LCML_DPRINT("%d :: Register Component Node\n",phandle->dspCodec->NodeInfo.AllUUIDs[dllinfo].eDllType);

            k = append_dsp_path((char*)phandle->dspCodec->NodeInfo.AllUUIDs[dllinfo].DllName, abs_dsp_path);
            if (k < 0)
            {
                LCML_DPRINT("%d :: append_dsp_path returned an error!\n", __LINE__);
                eError = OMX_ErrorBadParameter;
                goto ERROR;
            }

            status = DSPManager_RegisterObject((struct DSP_UUID *)phandle->dspCodec->NodeInfo.AllUUIDs[dllinfo].uuid,
                                                phandle->dspCodec->NodeInfo.AllUUIDs[dllinfo].eDllType, abs_dsp_path);

            DSP_ERROR_EXIT (status, "Register Component Library", ERROR);
        }

        /* NODE specific data */
        NodeAttrIn.cbStruct = sizeof(struct DSP_NODEATTRIN);
        NodeAttrIn.iPriority = phandle->dspCodec->Priority;
        NodeAttrIn.uTimeout = phandle->dspCodec->Timeout;
NodeAttrIn.uTimeout = 1000; /* WORKAROUND */       
        NodeAttrIn.uProfileID= phandle->dspCodec->ProfileID;
        /* Prepare Create Phase Argument */
        LCML_DPRINT("%d :: Prepare Create Phase Argument \n", __LINE__);
        /* TO DO check is application setting it properly */
        i = 0;
        if(phandle->dspCodec->pCrPhArgs !=NULL)
        {
            while((phandle->dspCodec->pCrPhArgs[i] != END_OF_CR_PHASE_ARGS) && (i < LCML_DATA_SIZE))
            {
                LCML_DPRINT("%d :: copying Create Phase Argument \n", i);
                crData.cData[i] = phandle->dspCodec->pCrPhArgs[i];
                LCML_DPRINT("%d :: CR PH arg[%d] = %d \n",__LINE__, i, crData.cData[i]);
                i++;
                LCML_DPRINT("%d : Inside create phase while loop\n", __LINE__);
            }
        }
        else
        {
            LCML_DPRINT("%d :: ILLEGAL CREATEPHASE SET IT ..\n", __LINE__);
            eError = OMX_ErrorBadParameter;
            goto ERROR;
        }

        if (i >= LCML_DATA_SIZE)
        {
            LCML_DPRINT("%d :: Reached end of Create Phase Args Array. Did not find END_OF_CR_PHASE_ARGS marker. \n", __LINE__);
            eError = OMX_ErrorBadParameter;
            goto ERROR;            
        }
        
        /* LCML_DPRINT("Create Phase args  strlen = %d\n",strlen(crData.cData)); */
        /* crData.cbData = sizeof (ULONG) + strlen(crData.cData); */
        crData.cbData = i*2;
        LCML_DPRINT("Create Phase args  strlen = %ld\n", crData.cbData);

        status = DSPNode_Allocate(phandle->dspCodec->hProc,
                                  (struct DSP_UUID *)phandle->dspCodec->NodeInfo.AllUUIDs[0].uuid,
                                  (struct DSP_CBDATA*)&crData,
                                  &NodeAttrIn,&(phandle->dspCodec->hNode));
        DSP_ERROR_EXIT(status, "Allocate Component", ERROR);
        LCML_DPRINT("%d :: DSPNode_Allocate Successfully\n", __LINE__);

        pArgs = (struct DSP_CBDATA *)argsBuf;
        strcpy((char*)pArgs->cData, Args);
        pArgs->cbData = (ULONG)strlen ((char *)pArgs->cData);

        /* For debugging on connect DSP nodes */
        LCML_DPRINT("[LCML] - struct DSP_CBDATA.cbData (length): %d\n", (int)pArgs->cbData);
        LCML_DPRINT("[LCML] - struct DSP_CBDATA.cData: %s\n", (char *)pArgs->cData);

        if (phandle->dspCodec->DeviceInfo.TypeofDevice == 1)
        {
            LCML_DPRINT("%d :: Audio Device Selected\n", __LINE__);
            LCML_DPRINT("%d :: -------- DASF Functionality ------- \n",__LINE__);
            status = DSPNode_Allocate(phandle->dspCodec->hProc,
                                      (struct DSP_UUID *)phandle->dspCodec->DeviceInfo.AllUUIDs[0].uuid,
                                      NULL,
                                      NULL,
                                      &(phandle->dspCodec->hDasfNode));
            DSP_ERROR_EXIT(status, "DASF Allocate Component", ERROR);


            LCML_DPRINT("%d :: DASF DSPNode_Allocate Successfully\n", __LINE__);
            LCML_DPRINT("%d :: DASF DSPNode_Allocate Successfully\n", __LINE__);
            if(phandle->dspCodec->DeviceInfo.DspStream != NULL)
            {
                if(phandle->dspCodec->DeviceInfo.TypeofRender == 0)
                {
                    /* render for playback */
                    LCML_DPRINT("%d :: Render for playback\n", __LINE__);
                    status = DSPNode_ConnectEx(phandle->dspCodec->hNode,
                                               0,
                                               (phandle->dspCodec->hDasfNode),
                                               0,
                                               (struct DSP_STRMATTR *)phandle->dspCodec->DeviceInfo.DspStream,
                                               pArgs);
                    DSP_ERROR_EXIT(status, "Node Connect", ERROR);
                }
                else if(phandle->dspCodec->DeviceInfo.TypeofRender == 1)
                {
                    /* render for record */
                    LCML_DPRINT("%d :: Render for record\n", __LINE__);
                    status = DSPNode_ConnectEx(phandle->dspCodec->hDasfNode,
                                               0,
                                               phandle->dspCodec->hNode,
                                               0,
                                               (struct DSP_STRMATTR *)phandle->dspCodec->DeviceInfo.DspStream,
                                               pArgs);
                    DSP_ERROR_EXIT(status, "Node Connect", ERROR);
                }
            }
            else
            {
                LCML_DPRINT("%d :: ILLEGAL STREAM PARAMETER SET IT ..\n",__LINE__);
                eError = OMX_ErrorBadParameter;
                goto ERROR;
            }
        }

        status = DSPNode_Create(phandle->dspCodec->hNode);
        DSP_ERROR_EXIT(status, "Create the Node", ERROR);
        LCML_DPRINT("%d :: After DSPNode_Create !!! \n", __LINE__);

        status = DSPNode_Run(phandle->dspCodec->hNode);
        DSP_ERROR_EXIT (status, "Goto RUN mode", ERROR);
        LCML_DPRINT("%d :: DSPNode_Run Successfully\n", __LINE__);

        if ((phandle->dspCodec->In_BufInfo.DataTrMethod == DMM_METHOD) || (phandle->dspCodec->Out_BufInfo.DataTrMethod == DMM_METHOD))
        {
            struct DSP_NOTIFICATION* notification;
            LCML_DPRINT("%d :: Registering the Node for Messaging\n",__LINE__);

	        LCML_MALLOC(notification,sizeof(struct DSP_NOTIFICATION),struct DSP_NOTIFICATION)
            if(notification == NULL)
            {
                LCML_DPRINT("%d :: malloc failed....\n",__LINE__);
                goto ERROR;
            }
            memset(notification, 0, sizeof(struct DSP_NOTIFICATION));

            status = DSPNode_RegisterNotify(phandle->dspCodec->hNode, DSP_NODEMESSAGEREADY, DSP_SIGNALEVENT, notification);
            DSP_ERROR_EXIT(status, "DSP node register notify", ERROR);
            phandle->g_aNotificationObjects[0] = notification;
#ifdef __ERROR_PROPAGATION__
            struct DSP_NOTIFICATION* notification_mmufault;

            LCML_DPRINT("%d :: Registering the Node for Messaging\n",__LINE__);

            LCML_MALLOC(notification_mmufault,sizeof(struct DSP_NOTIFICATION),struct DSP_NOTIFICATION);
            if(notification_mmufault == NULL)
            {
                LCML_DPRINT("%d :: malloc failed....\n",__LINE__);
                goto ERROR;
            }
            memset(notification_mmufault,0,sizeof(struct DSP_NOTIFICATION));
            
            status = DSPProcessor_RegisterNotify(phandle->dspCodec->hProc, DSP_MMUFAULT, DSP_SIGNALEVENT, notification_mmufault);
            DSP_ERROR_EXIT(status, "DSP node register notify DSP_MMUFAULT", ERROR);
            phandle->g_aNotificationObjects[1] =  notification_mmufault;
            
            struct DSP_NOTIFICATION* notification_syserror ;
           
            LCML_DPRINT("%d :: Registering the Node for Messaging\n",__LINE__);

            LCML_MALLOC(notification_syserror,sizeof(struct DSP_NOTIFICATION),struct DSP_NOTIFICATION);
            if(notification_syserror == NULL)
            {
                LCML_DPRINT("%d :: malloc failed....\n",__LINE__);
                goto ERROR;
            }
            memset(notification_syserror,0,sizeof(struct DSP_NOTIFICATION));
            
            status = DSPProcessor_RegisterNotify(phandle->dspCodec->hProc, DSP_SYSERROR, DSP_SIGNALEVENT, notification_syserror);
            DSP_ERROR_EXIT(status, "DSP node register notify DSP_SYSERROR", ERROR);
            phandle->g_aNotificationObjects[2] =  notification_syserror;
#endif     
        }

        /* Listener thread */
        phandle->pshutdownFlag = 0;
        phandle->g_tidMessageThread = 0;
        phandle->bUsnEos = OMX_FALSE;
        tmperr = pthread_create(&phandle->g_tidMessageThread,
                                NULL,
                                MessagingThread,
                                (void*)phandle);
        if (tmperr || !phandle->g_tidMessageThread)
        {
            LCML_DPRINT("Thread creation failed: 0x%x",tmperr);
            eError = OMX_ErrorInsufficientResources;
            goto ERROR;
        }

#ifdef __PERF_INSTRUMENTATION__
        PERF_ThreadCreated(phandle->pPERF,
                           phandle->g_tidMessageThread,
                           PERF_FOURCC('C','M','L','T'));
#endif
        /* init buffers buffer counter */
        phandle->iBufinputcount = 0;
        phandle->iBufoutputcount = 0;
        
        for (i = 0; i < QUEUE_SIZE; i++)
        {
            phandle->Arminputstorage[i] = NULL;
            phandle->Armoutputstorage[i] = NULL;
            phandle->pAlgcntlDmmBuf[i] = NULL;
            phandle->pStrmcntlDmmBuf[i] = NULL;
            phandle->algcntlmapped[i] = 0;
            phandle->strmcntlmapped[i] = 0;
        }
#ifdef __PERF_INSTRUMENTATION__
        PERF_Boundary(phandle->pPERF,
                      PERF_BoundaryComplete | PERF_BoundarySetup);
#endif
    }

ERROR:
#ifndef CEXEC_DONE
    LCML_FREE(argv);
#endif
    LCML_DPRINT("%d :: Exiting Init_DSPSubSystem\n", __LINE__);
    return eError;
}

/** ========================================================================
*  InitMMCodec initialise the OMX Component specific handle to LCML.
*  The memory is allocated and the dsp node is created. Add notification object
*  to listener thread.
*
*  @param  hInterface - Handle to LCML which is allocated and filled
*  @param  codecName - not used
*  @param  toCodecInitParams - not used yet
*  @param  fromCodecInfoStruct - not used yet
*  @param  pCallbacks - List of callback that uses to call OMX
*  @return OMX_ERRORTYPE
*      If the command successfully executes, the return code will be
*      OMX_NoError.  Otherwise the appropriate OMX error will be returned.
* ==========================================================================*/
static OMX_ERRORTYPE InitMMCodec(OMX_HANDLETYPE hInt,
                                 OMX_STRING  codecName,
                                 void * toCodecInitParams,
                                 void * fromCodecInfoStruct,
                                 LCML_CALLBACKTYPE *pCallbacks)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 dllinfo;
    LCML_DSP_INTERFACE * phandle;
#ifndef CEXEC_DONE
    UINT argc = 1;
    char argv[ABS_DLL_NAME_LENGTH];
    k = append_dsp_path(DSP_DOF_IMAGE, argv);
    if (k < 0)
    {
        LCML_DPRINT("%d :: append_dsp_path returned an error!\n", __LINE__);
        eError = OMX_ErrorBadParameter;
        goto ERROR;
    }
#endif
    LCML_CREATEPHASEARGS crData;
    DSP_STATUS status;
    int i = 0, k =0;
    struct DSP_NODEATTRIN NodeAttrIn;
    int tmperr;
    char abs_dsp_path[ABS_DLL_NAME_LENGTH];

    LCML_DPRINT("%d :: InitMMCodec application\n",__LINE__);

    if (hInt == NULL )
    {
        eError = OMX_ErrorInsufficientResources;
        goto ERROR;
    }



    phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)hInt)->pCodec);
#ifdef __PERF_INSTRUMENTATION__
    phandle->pPERF = PERF_Create(PERF_FOURCC('C','M','L',' '),
                             PERF_ModuleAudioDecode | PERF_ModuleAudioEncode |
                             PERF_ModuleVideoDecode | PERF_ModuleVideoEncode |
                             PERF_ModuleImageDecode | PERF_ModuleImageEncode |
                             PERF_ModuleCommonLayer);
    PERF_Boundary(phandle->pPERF,
                  PERF_BoundaryStart | PERF_BoundarySetup);
#endif

    /* INIT DSP RESOURCE */
    if(pCallbacks)
    {
        phandle->dspCodec->Callbacks.LCML_Callback =  pCallbacks->LCML_Callback;
    }
    else
    {
        eError = OMX_ErrorBadParameter;
        goto ERROR;
    }

    /* INITIALIZATION OF DSP */
    LCML_DPRINT("%d :: Entering Init_DSPSubSystem\n", __LINE__);
    status = DspManager_Open(0, NULL);
    DSP_ERROR_EXIT(status, "DSP Manager Open", ERROR);
    LCML_DPRINT("DspManager_Open Successful\n");

    /* Attach and get handle to processor */
    status = DSPProcessor_Attach(TI_PROCESSOR_DSP, NULL, &(phandle->dspCodec->hProc));
    DSP_ERROR_EXIT(status, "Attach processor", ERROR);
    LCML_DPRINT("DSPProcessor_Attach Successful\n");
    LCML_DPRINT("Base Image is Already Loaded\n");

    for(dllinfo=0; dllinfo < phandle->dspCodec->NodeInfo.nNumOfDLLs; dllinfo++)
    {
        LCML_DPRINT("%d :: Register Component Node\n",phandle->dspCodec->NodeInfo.AllUUIDs[dllinfo].eDllType);

        k = append_dsp_path((char*)phandle->dspCodec->NodeInfo.AllUUIDs[dllinfo].DllName, abs_dsp_path);
        if (k < 0)
        {
            LCML_DPRINT("%d :: append_dsp_path returned an error!\n", __LINE__);
            eError = OMX_ErrorBadParameter;
            goto ERROR;
        }

        status = DSPManager_RegisterObject((struct DSP_UUID *)phandle->dspCodec->NodeInfo.AllUUIDs[dllinfo].uuid,
                                               phandle->dspCodec->NodeInfo.AllUUIDs[dllinfo].eDllType,
                                               abs_dsp_path);

        DSP_ERROR_EXIT (status, "Register Component Library", ERROR)
    }

    /* NODE specific data */

    NodeAttrIn.cbStruct = sizeof(struct DSP_NODEATTRIN);
    NodeAttrIn.iPriority = phandle->dspCodec->Priority;
    NodeAttrIn.uTimeout = phandle->dspCodec->Timeout;
    NodeAttrIn.uTimeout  = 1000; /* WORKAROUND */
    NodeAttrIn.uProfileID= phandle->dspCodec->ProfileID;
    /* Prepare Create Phase Argument */

    LCML_DPRINT("%d :: Prepare Create Phase Argument \n",__LINE__);
    /* TO DO check is application setting it properly */
    if(phandle->dspCodec->pCrPhArgs !=NULL)
    {
        while((phandle->dspCodec->pCrPhArgs[i] != END_OF_CR_PHASE_ARGS) && (i < LCML_DATA_SIZE))
        {
            LCML_DPRINT("%d :: copying Create Phase Argument \n",i);
            crData.cData[i] =phandle->dspCodec->pCrPhArgs[i];
            LCML_DPRINT("%d :: CR PH arg[%d] = %d \n",__LINE__,i,crData.cData[i]);
            i++;
            LCML_DPRINT("%d : Inside create phase while loop\n",__LINE__);
        }
    }
    else
    {
        LCML_DPRINT("%d :: ILLEGAL CREATEPHASE SET IT ..\n",__LINE__);
        eError = OMX_ErrorBadParameter;
        goto ERROR;
    }

    if (i >= LCML_DATA_SIZE)
    {
       LCML_DPRINT("%d :: Reached end of Create Phase Args Array. Did not find END_OF_CR_PHASE_ARGS marker. \n", __LINE__);
       eError = OMX_ErrorBadParameter;
       goto ERROR;
    }


    /* LCML_DPRINT("Create Phase args  strlen = %d\n",strlen(crData.cData)); */
    /* crData.cbData = sizeof (ULONG) + strlen(crData.cData); */
    crData.cbData = i * 2;
    LCML_DPRINT("Create Phase args  strlen = %ld\n", crData.cbData);

    status = DSPNode_Allocate(phandle->dspCodec->hProc,
                             (struct DSP_UUID *)phandle->dspCodec->NodeInfo.AllUUIDs[0].uuid,
                             (struct DSP_CBDATA*)&crData, &NodeAttrIn,
                             &(phandle->dspCodec->hNode));
    DSP_ERROR_EXIT(status, "Allocate Component", ERROR);
    LCML_DPRINT("%d :: DSPNode_Allocate Successfully\n", __LINE__);

    if(phandle->dspCodec->DeviceInfo.TypeofDevice == 1)
    {
        LCML_DPRINT("%d :: Audio Device Selected\n", __LINE__);
        LCML_DPRINT("%d :: -------- DASF Functionality ------- \n",__LINE__);
        status = DSPNode_Allocate(phandle->dspCodec->hProc,
                                 (struct DSP_UUID *)phandle->dspCodec->DeviceInfo.AllUUIDs[0].uuid,
                                 NULL,
                                 NULL,
                                 &(phandle->dspCodec->hDasfNode));
        DSP_ERROR_EXIT(status, "DASF Allocate Component", ERROR);

        LCML_DPRINT("%d :: DASF DSPNode_Allocate Successfully\n", __LINE__);
        if(phandle->dspCodec->DeviceInfo.DspStream !=NULL)
        {
            if(phandle->dspCodec->DeviceInfo.TypeofRender == 0)
            {
                /* render for playback */
                LCML_DPRINT("%d :: Render for playback\n", __LINE__);
                status = DSPNode_Connect(phandle->dspCodec->hNode,
                                         0,
                                         phandle->dspCodec->hDasfNode,
                                         0,
                                         (struct DSP_STRMATTR *)phandle->dspCodec->DeviceInfo.DspStream);
                DSP_ERROR_EXIT(status, "Node Connect", ERROR);
            }
            else if(phandle->dspCodec->DeviceInfo.TypeofRender == 1)
            {
                /* render for record */
                LCML_DPRINT("%d :: Render for record\n", __LINE__);
                status = DSPNode_Connect(phandle->dspCodec->hDasfNode,
                                         0,
                                         phandle->dspCodec->hNode,
                                         0,
                                         (struct DSP_STRMATTR *)phandle->dspCodec->DeviceInfo.DspStream);
                DSP_ERROR_EXIT(status, "Node Connect", ERROR);
            }
        }
        else
        {
            LCML_DPRINT("%d :: ILLEGAL STREAM PARAMETER SET IT ..\n",__LINE__);
            eError = OMX_ErrorBadParameter;
            goto ERROR;
        }
    }

    status = DSPNode_Create(phandle->dspCodec->hNode);
    DSP_ERROR_EXIT(status, "Create the Node", ERROR);
    LCML_DPRINT("%d :: After DSPNode_Create !!! \n", __LINE__);

    status = DSPNode_Run (phandle->dspCodec->hNode);
    DSP_ERROR_EXIT (status, "Goto RUN mode", ERROR);
    LCML_DPRINT("%d :: DSPNode_Run Successfully\n", __LINE__);

    if ((phandle->dspCodec->In_BufInfo.DataTrMethod == DMM_METHOD) ||
        (phandle->dspCodec->Out_BufInfo.DataTrMethod == DMM_METHOD))
    {
        struct DSP_NOTIFICATION* notification;
        LCML_DPRINT("%d :: Registering the Node for Messaging\n",__LINE__);

        LCML_MALLOC(notification,sizeof(struct DSP_NOTIFICATION),struct DSP_NOTIFICATION);
        if(notification == NULL)
        {
            LCML_DPRINT("%d :: malloc failed....\n",__LINE__);
            goto ERROR;
        }
        memset(notification,0,sizeof(struct DSP_NOTIFICATION));

        status = DSPNode_RegisterNotify(phandle->dspCodec->hNode, DSP_NODEMESSAGEREADY, DSP_SIGNALEVENT, notification);
        DSP_ERROR_EXIT(status, "DSP node register notify", ERROR);
        phandle->g_aNotificationObjects[0] =  notification;
#ifdef __ERROR_PROPAGATION__      
        struct DSP_NOTIFICATION* notification_mmufault;
       
        LCML_DPRINT("%d :: Registering the Node for Messaging\n",__LINE__);

        LCML_MALLOC(notification_mmufault,sizeof(struct DSP_NOTIFICATION),struct DSP_NOTIFICATION);
        if(notification_mmufault == NULL)
        {
            LCML_DPRINT("%d :: malloc failed....\n",__LINE__);
            goto ERROR;
        }
        memset(notification_mmufault,0,sizeof(struct DSP_NOTIFICATION));
        
        status = DSPProcessor_RegisterNotify(phandle->dspCodec->hProc, DSP_MMUFAULT, DSP_SIGNALEVENT, notification_mmufault);
        DSP_ERROR_EXIT(status, "DSP node register notify DSP_MMUFAULT", ERROR);
        phandle->g_aNotificationObjects[1] =  notification_mmufault;
        
        struct DSP_NOTIFICATION* notification_syserror ;
       
        LCML_DPRINT("%d :: Registering the Node for Messaging\n",__LINE__);

        LCML_MALLOC(notification_syserror,sizeof(struct DSP_NOTIFICATION),struct DSP_NOTIFICATION);
        if(notification_syserror == NULL)
        {
            LCML_DPRINT("%d :: malloc failed....\n",__LINE__);
            goto ERROR;
        }
        memset(notification_syserror,0,sizeof(struct DSP_NOTIFICATION));
        
        status = DSPProcessor_RegisterNotify(phandle->dspCodec->hProc, DSP_SYSERROR, DSP_SIGNALEVENT, notification_syserror);
        DSP_ERROR_EXIT(status, "DSP node register notify DSP_SYSERROR", ERROR);
        phandle->g_aNotificationObjects[2] =  notification_syserror;
#endif       
    }

    /* Listener thread */
    phandle->pshutdownFlag = 0;
    phandle->g_tidMessageThread = 0;

    tmperr = pthread_create(&phandle->g_tidMessageThread,
                            NULL,
                            MessagingThread,
                            (void*)phandle);
    if(tmperr || !phandle->g_tidMessageThread)
    {
        LCML_DPRINT("Thread creation failed: 0x%x",tmperr);
        eError = OMX_ErrorInsufficientResources;
        goto ERROR;
    }

#ifdef __PERF_INSTRUMENTATION__
    PERF_ThreadCreated(phandle->pPERF,
                       phandle->g_tidMessageThread,
                       PERF_FOURCC('C','M','L','T'));
#endif
    /* init buffers buffer counter */
    phandle->iBufinputcount =0;
    phandle->iBufoutputcount =0;
    
    for (i = 0; i < QUEUE_SIZE; i++)
    {
        phandle->Arminputstorage[i] = NULL;
        phandle->Armoutputstorage[i] = NULL;
        phandle->pAlgcntlDmmBuf[i] = NULL;
        phandle->pStrmcntlDmmBuf[i] = NULL;
        phandle->algcntlmapped[i] = 0;
        phandle->strmcntlmapped[i] = 0;
    }

#ifdef __PERF_INSTRUMENTATION__
    PERF_Boundary(phandle->pPERF,
                  PERF_BoundaryComplete | PERF_BoundarySetup);
#endif

ERROR:
#ifndef CEXEC_DONE
    LCML_FREE(argv);
#endif
    LCML_DPRINT("%d :: Exiting Init_DSPSubSystem\n", __LINE__);
    return eError;
}



/** ========================================================================
*  The LCML_WaitForEvent Wait for a event sychronously
*  @param  hInterface -  Handle of the component to be accessed.  This is the
*      component handle returned by the call to the GetHandle function.
*  @param  event - Event occured
*  @param  args - Array of "void *" that contains the associated arguments for
*             occured event
*
*  @return OMX_ERRORTYPE
*      If the command successfully executes, the return code will be
*      OMX_NoError.  Otherwise the appropriate OMX error will be returned.
** ==========================================================================*/
static OMX_ERRORTYPE WaitForEvent(OMX_HANDLETYPE hComponent,
                                  TUsnCodecEvent event,
                                  void * args[10] )
{
    OMX_ERRORTYPE eError = OMX_ErrorUndefined;
    LCML_DPRINT("%d :: WaitForEvent application\n",__LINE__);
    return eError;
}


/** ========================================================================
*  The LCML_QueueBuffer send data to DSP convert it into USN format and send
*  it to DSP via setbuff
*  @param [in] hInterface -  Handle of the component to be accessed.  This is
*      the component handle returned by the call to the GetHandle function.
*  @param  bufType - type of buffer
*  @param  buffer - pointer to buffer
*  @param  bufferLen - length of  buffer
*  @param  bufferSizeUsed - length of used buffer
*  @param  auxInfo - pointer to parameter
*  @param  auxInfoLen - length of  parameter
*  @param  usrArg - not used
*  @return OMX_ERRORTYPE
*      If the command successfully executes, the return code will be
*      OMX_NoError.  Otherwise the appropriate OMX error will be returned.
* ==========================================================================*/
static OMX_ERRORTYPE QueueBuffer (OMX_HANDLETYPE hComponent,
                                  TMMCodecBufferType bufType,
                                  OMX_U8 * buffer, OMX_S32 bufferLen,
                                  OMX_S32 bufferSizeUsed ,OMX_U8 * auxInfo,
                                  OMX_S32 auxInfoLen ,OMX_U8 * usrArg )
{
    LCML_DSP_INTERFACE * phandle;
    int streamId = 0;
    DSP_STATUS status;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    char * tmp2=NULL;
    DMM_BUFFER_OBJ* pDmmBuf=NULL;
    int commandId;
    struct DSP_MSG msg;
    OMX_U32 MapBufLen=0;

    LCML_DPRINT("%d :: QueueBuffer application\n",__LINE__);

    if (hComponent == NULL)
    {
        eError = OMX_ErrorInsufficientResources;
        return eError;
    }

    phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)hComponent)->pCodec);

    LCML_DPRINT("LCML QueueBuffer: phandle->iBufinputcount is %d (%p) \n", phandle->iBufinputcount, phandle);

#ifdef __PERF_INSTRUMENTATION__
    PERF_XferingBuffer(phandle->pPERF,
                       buffer,
                       bufferSizeUsed,
                       PERF_ModuleComponent,
                       PERF_ModuleSocketNode);
#endif
    pthread_mutex_lock(&phandle->mutex);
    LCML_MALLOC(tmp2,sizeof(TArmDspCommunicationStruct) + 256,char);
    if (tmp2 == NULL)
    {
            eError = OMX_ErrorInsufficientResources;
            goto EXIT;
    }
    memset(tmp2,0,sizeof(TArmDspCommunicationStruct)+256);
    phandle->commStruct = (TArmDspCommunicationStruct *)(tmp2 + 128);
    phandle->commStruct->iBufferPtr = (OMX_U32) buffer;
    phandle->commStruct->iBufferSize = bufferLen;
    phandle->commStruct->iParamPtr = (OMX_U32) auxInfo;
    phandle->commStruct->iParamSize = auxInfoLen;
    /*USN updation */
    phandle->commStruct->iBufSizeUsed =  bufferSizeUsed ;
    phandle->commStruct->iArmArg = (OMX_U32) buffer;
    phandle->commStruct->iArmParamArg = (OMX_U32) auxInfo;

    /* if the bUsnEos flag is set interpret the usrArg as a buffer header */
    if (phandle->bUsnEos == OMX_TRUE) {
        phandle->commStruct->iEOSFlag = (((OMX_BUFFERHEADERTYPE*)usrArg)->nFlags & 0x00000001);
    }
    else { 
        phandle->commStruct->iEOSFlag = 0;
    }
    phandle->commStruct->iUsrArg = (OMX_U32) usrArg;
    phandle->iBufoutputcount = phandle->iBufoutputcount % QUEUE_SIZE;
    phandle->iBufinputcount = phandle->iBufinputcount % QUEUE_SIZE;
    phandle->commStruct->Bufoutindex = phandle->iBufoutputcount;
    phandle->commStruct->BufInindex = phandle->iBufinputcount;
    if (bufType == EMMCodecInputBufferMapBufLen)
    {
        bufType = EMMCodecInputBuffer;
        MapBufLen = 1;
    }
    else if (bufType == EMMCodecOutputBufferMapBufLen)
    {
        bufType = EMMCodecOuputBuffer;
        MapBufLen = 1;
    }


    if ((bufType >= EMMCodecStream0) && (bufType <= (EMMCodecStream0 + 20)))
    {
        streamId = bufType - EMMCodecStream0;
    }

	phandle->commStruct->iStreamID = streamId;

    if (bufType == EMMCodecInputBuffer || !(streamId % 2))
    {
        phandle->Arminputstorage[phandle->iBufinputcount] = phandle->commStruct;
        pDmmBuf = phandle->dspCodec->InDmmBuffer;
        pDmmBuf = pDmmBuf + phandle->iBufinputcount;
        phandle->iBufinputcount++;
        phandle->iBufinputcount = phandle->iBufinputcount % QUEUE_SIZE;
        LCML_DPRINT("VPP port %d use InDmmBuffer (%d) %p\n", streamId, phandle->iBufinputcount, pDmmBuf);

    }
    else if (bufType == EMMCodecOuputBuffer || streamId % 2)
    {
        phandle->Armoutputstorage[phandle->iBufoutputcount] = phandle->commStruct;
        pDmmBuf = phandle->dspCodec->OutDmmBuffer;
        pDmmBuf = pDmmBuf + phandle->iBufoutputcount;
        phandle->iBufoutputcount++;
        phandle->iBufoutputcount = phandle->iBufoutputcount % QUEUE_SIZE;
    }
    else
    {
        LCML_DPRINT("Unrecognized buffer type..");
        eError = OMX_ErrorBadParameter;
        if(tmp2)
        {
            free(tmp2);
            phandle->commStruct = NULL;
        }
        goto EXIT;
    }
    commandId = USN_GPPMSG_SET_BUFF|streamId;
    LCML_DPRINT("Sending command ID 0x%x",commandId);
    if( pDmmBuf == NULL)
    {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    LCML_DPRINT("buffer = 0x%p bufferlen = %ld auxInfo = 0x%p auxInfoLen %ld\n",
        buffer, bufferLen, auxInfo, auxInfoLen );

    phandle->commStruct->iArmbufferArg = (OMX_U32)buffer;
    if ((buffer != NULL) && (bufferLen != 0))
    {
        LCML_DPRINT("mapping buffer \n");

        if (bufType == EMMCodecInputBuffer || !(streamId % 2)) {
            if (MapBufLen)
            {
                /*using this option only when not mapping the entire memory region
                 * can cause a DSP MMU FAULT or DSP SYS ERROR */

                eError = DmmMap(phandle->dspCodec->hProc, bufferLen, bufferSizeUsed,buffer, (pDmmBuf), MapBufLen);
            }
            else
            {
                phandle->commStruct->iBufferSize = bufferSizeUsed ? bufferSizeUsed : bufferLen;
                eError = DmmMap(phandle->dspCodec->hProc, bufferSizeUsed ? bufferSizeUsed: bufferLen, bufferSizeUsed,buffer, (pDmmBuf), MapBufLen);
            }
        }
        else if (bufType == EMMCodecOuputBuffer || streamId % 2) {
            eError = DmmMap(phandle->dspCodec->hProc, bufferLen, 0,buffer, (pDmmBuf), MapBufLen);
        }
        if (eError != OMX_ErrorNone)
        {
            goto EXIT;
        }

        phandle->commStruct->iBufferPtr = (OMX_U32) pDmmBuf->pMapped;
        /* storing reserve address for buffer */
        pDmmBuf->bufReserved = pDmmBuf->pReserved;
        LCML_DPRINT("reserved buffer 0x%p  reserved1 buffer 0x%p \n", pDmmBuf->pReserved, pDmmBuf->bufReserved);
    }

    if (auxInfoLen != 0 && auxInfo != NULL )
    {
        LCML_DPRINT("mapping parameter \n");
        eError = DmmMap(phandle->dspCodec->hProc, phandle->commStruct->iParamSize, phandle->commStruct->iParamSize, (void*)phandle->commStruct->iParamPtr, (pDmmBuf), 0);
        if (eError != OMX_ErrorNone)
        {
            goto EXIT;
        }

        phandle->commStruct->iParamPtr = (OMX_U32 )pDmmBuf->pMapped ;
        /* storing reserve address for param */
        pDmmBuf->paramReserved = pDmmBuf->pReserved;
    }

    eError = DmmMap(phandle->dspCodec->hProc, sizeof(TArmDspCommunicationStruct),sizeof(TArmDspCommunicationStruct), (void *)phandle->commStruct, (pDmmBuf), 0);
    if (eError != OMX_ErrorNone)
    {
        goto EXIT;
    }
    
    /* storing mapped address of struct */
    phandle->commStruct->iArmArg = (OMX_U32)pDmmBuf->pMapped;
    
    LCML_DPRINT("sending SETBUFF \n");
    msg.dwCmd = commandId;
    msg.dwArg1 = (int)pDmmBuf->pMapped;
    msg.dwArg2 = 0;

    status = DSPNode_PutMessage (phandle->dspCodec->hNode, &msg, DSP_FOREVER);
    LCML_DPRINT("after SETBUFF \n");
    DSP_ERROR_EXIT (status, "Send message to node", EXIT);

EXIT:
    pthread_mutex_unlock(&phandle->mutex);
    return eError;
}


/** ========================================================================
*  The LCML_ControlCodec send command to DSP convert it into USN format and
*  send it to DSP
*  @param  hInterface -  Handle of the component to be accessed.  This is the
*      component handle returned by the call to the GetHandle function.
*  @param  bufType - type of buffer
*  @param  iCodecCmd -  command refer TControlCmd
*  @param  args - pointer to send some specific command to DSP
*
*  @return OMX_ERRORTYPE
*      If the command successfully executes, the return code will be
*      OMX_NoError.  Otherwise the appropriate OMX error will be returned.
** ==========================================================================*/
static OMX_ERRORTYPE ControlCodec(OMX_HANDLETYPE hComponent,
                                  TControlCmd iCodecCmd,
                                  void * args[10])
{
    LCML_DSP_INTERFACE * phandle;
    DSP_STATUS status;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    LCML_DPRINT("%d :: ControlCodec application\n",__LINE__);
    if (hComponent == NULL )
    {
        eError= OMX_ErrorInsufficientResources;
        return eError;
    }

    phandle = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE *)hComponent)->pCodec);

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedCommand(phandle->pPERF,
                         (OMX_U32) iCodecCmd,
                         (OMX_U32) args,
                         PERF_ModuleLLMM);
#endif
    switch (iCodecCmd)
    {
        case EMMCodecControlPause:
        {
            struct DSP_MSG msg = {USN_GPPMSG_PAUSE, 0, 0};
            LCML_DPRINT("Sending PAUSE command");
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingCommand(phandle->pPERF,
                                msg.dwCmd,
                                msg.dwArg1,
                                PERF_ModuleSocketNode);                                
#endif
            status = DSPNode_PutMessage (phandle->dspCodec->hNode, &msg, DSP_FOREVER);
            DSP_ERROR_EXIT (status, "Send message to node", EXIT);
            break;
        }
        case EMMCodecControlStart:
        {
            struct DSP_MSG msg = {USN_GPPMSG_PLAY, 0, 0};
            LCML_DPRINT("Sending PLAY --1 command");
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingCommand(phandle->pPERF,
                                msg.dwCmd,
                                msg.dwArg1,
                                PERF_ModuleSocketNode);                                
#endif
            status = DSPNode_PutMessage (phandle->dspCodec->hNode, &msg, DSP_FOREVER);
            DSP_ERROR_EXIT (status, "Send message to node", EXIT);
            break;
        }
        case MMCodecControlStop:
        {
            struct DSP_MSG msg = {USN_GPPMSG_STOP, 0, 0};
            LCML_DPRINT("Sending STOP command\n");
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingCommand(phandle->pPERF,
                                msg.dwCmd,
                                msg.dwArg1,
                                PERF_ModuleSocketNode);                                
#endif
            status = DSPNode_PutMessage (phandle->dspCodec->hNode, &msg, DSP_FOREVER);
            DSP_ERROR_EXIT (status, "Send message to node", EXIT);
            break;
        }
        case EMMCodecControlDestroy:
        {
            int pthreadError = 0;
            LCML_DPRINT("Destroy the codec");
#ifdef __PERF_INSTRUMENTATION__
            PERF_Boundary(phandle->pPERF,
                          PERF_BoundaryStart | PERF_BoundaryCleanup);
            PERF_SendingCommand(phandle->pPERF,
                                -1, 0, PERF_ModuleComponent);
#endif
            phandle->pshutdownFlag = 1;
            pthreadError = pthread_join(phandle->g_tidMessageThread, NULL);
            if (0 != pthreadError)
            {
                eError = OMX_ErrorHardware;
                LCML_DPRINT("%d :: Error while closing Component Thread\n", pthreadError);
            }
            LCML_DPRINT("Destroy the codec %d",eError);
            DeleteDspResource (phandle);

#ifdef __PERF_INSTRUMENTATION__
            PERF_OBJHANDLE pPERF = phandle->pPERF;
#endif

            FreeResources(phandle);

#ifdef __PERF_INSTRUMENTATION__
            PERF_Boundary(pPERF, PERF_BoundaryComplete | PERF_BoundaryCleanup);
            PERF_Done(pPERF);
#endif

            break;
        }

        /* this case is for sending extra custom commands to DSP socket node */
        case EMMCodecControlSendDspMessage:
        {
            /* todo: Check to see if the arguments are valid */
            struct DSP_MSG msg = {(int)args[0], (int)args[1], (int)args[2]};
            LCML_DPRINT("message to codec");
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingCommand(phandle->pPERF,
                                msg.dwCmd,
                                msg.dwArg1,
                                PERF_ModuleSocketNode);                                
#endif
            status = DSPNode_PutMessage (phandle->dspCodec->hNode, &msg, DSP_FOREVER);
            DSP_ERROR_EXIT (status, "Send message to node", EXIT);
            break;
        }
        case EMMCodecControlAlgCtrl:
        {
            struct DSP_MSG msg;
            int i;
            pthread_mutex_lock(&phandle->mutex);
            for (i = 0; i < QUEUE_SIZE; i++)
            {
                /* searching for empty slot */
                if (phandle->pAlgcntlDmmBuf[i] == NULL)
                    break;
            }
            if(i >= QUEUE_SIZE)
            {
                pthread_mutex_unlock(&phandle->mutex);
                eError = OMX_ErrorUndefined;
                goto EXIT;
            }
            LCML_MALLOC(phandle->pAlgcntlDmmBuf[i],sizeof(DMM_BUFFER_OBJ),DMM_BUFFER_OBJ);
            if(phandle->pAlgcntlDmmBuf[i] == NULL)
            {
                eError = OMX_ErrorInsufficientResources;
                pthread_mutex_unlock(&phandle->mutex);
                goto EXIT;
            }
            memset(phandle->pAlgcntlDmmBuf[i],0,sizeof(DMM_BUFFER_OBJ));
            eError = DmmMap(phandle->dspCodec->hProc,(int)args[2],(int)args[2], args[1],(phandle->pAlgcntlDmmBuf[i]), 0);
            if (eError != OMX_ErrorNone)
            {
                pthread_mutex_unlock(&phandle->mutex);
                goto EXIT;
            }
            phandle->algcntlmapped[i] = 1;
            msg.dwCmd = USN_GPPMSG_ALGCTRL;
            msg.dwArg1 = (int)args[0];
            msg.dwArg2 = (int)phandle->pAlgcntlDmmBuf[i]->pMapped;
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingCommand(phandle->pPERF,
                                msg.dwCmd,
                                msg.dwArg1,
                                PERF_ModuleSocketNode);                                
#endif
            status = DSPNode_PutMessage (phandle->dspCodec->hNode, &msg, DSP_FOREVER);
            pthread_mutex_unlock(&phandle->mutex);
            DSP_ERROR_EXIT (status, "Send message to node", EXIT);
            break;
        }
        case EMMCodecControlStrmCtrl:
        {
            struct DSP_MSG msg;
            
            if ((int)args[0] == USN_STRMCMD_FLUSH) {
                msg.dwCmd = USN_GPPMSG_STRMCTRL | (int)args[1]; 
                msg.dwArg1  = USN_STRMCMD_FLUSH;
                msg.dwArg2  = 0;
                phandle->flush_pending[(int)args[1]]= 1;
            }
            else 
            {
                int i;
                pthread_mutex_lock(&phandle->mutex);
                for (i = 0; i < QUEUE_SIZE; i++)
                {
                    /* searching for empty slot */
                    if (phandle->pStrmcntlDmmBuf[i] == NULL)
                        break;
                }
                if(i >= QUEUE_SIZE)
                {
                    eError=OMX_ErrorUndefined;
                    pthread_mutex_unlock(&phandle->mutex);
                    goto EXIT;
                }

                LCML_MALLOC(phandle->pStrmcntlDmmBuf[i],sizeof(DMM_BUFFER_OBJ),DMM_BUFFER_OBJ);
                if(phandle->pStrmcntlDmmBuf[i] == NULL)
                {
                    eError = OMX_ErrorInsufficientResources;
                    pthread_mutex_unlock(&phandle->mutex);
                    goto EXIT;
                }
                
                memset(phandle->pStrmcntlDmmBuf[i],0,sizeof(DMM_BUFFER_OBJ)); //ATC
                
                eError = DmmMap(phandle->dspCodec->hProc, (int)args[2],(int)args[2], args[1],(phandle->pStrmcntlDmmBuf[i]), 0);
                if (eError != OMX_ErrorNone)
                {
                    pthread_mutex_unlock(&phandle->mutex);
                    goto EXIT;
                }
                phandle->strmcntlmapped[i] = 1;
                if(phandle->dspCodec->DeviceInfo.TypeofRender == 0)
                {
                    /* playback mode */
                    msg.dwCmd = USN_GPPMSG_STRMCTRL | 0x01;
                    msg.dwArg1 = (int)args[0];
                    msg.dwArg2 = (int)phandle->pStrmcntlDmmBuf[i]->pMapped;
                }
                else if(phandle->dspCodec->DeviceInfo.TypeofRender == 1)
                {
                    /* record mode */
                    msg.dwCmd = USN_GPPMSG_STRMCTRL;
                    msg.dwArg1 = (int)args[0];
                    msg.dwArg2 = (int)phandle->pStrmcntlDmmBuf[i]->pMapped;
                }
            }
#ifdef __PERF_INSTRUMENTATION__
            PERF_SendingCommand(phandle->pPERF,
                                msg.dwCmd,
                                msg.dwArg1,
                                PERF_ModuleSocketNode);                                
#endif
            status = DSPNode_PutMessage (phandle->dspCodec->hNode, &msg, DSP_FOREVER);
            pthread_mutex_unlock(&phandle->mutex);
            DSP_ERROR_EXIT (status, "Send message to node", EXIT);

            LCML_DPRINT("STRMControl: arg[0]: message = %x\n",(int)args[0]);
            LCML_DPRINT("STRMControl: arg[1]: address = %p\n",args[1]);
            LCML_DPRINT("STRMControl: arg[2]: size = %d\n",(int)args[2]);
            break;
        }
        case EMMCodecControlUsnEos:
        {
            phandle->bUsnEos = OMX_TRUE;
            break;
        }
        
    }

EXIT:
    return eError;
}


/** ========================================================================
*  DmmMap () method is used to allocate the memory using DMM.
*
*  @param ProcHandle -  Component identification number
*  @param size  - Buffer header address, that needs to be sent to codec
*  @param pArmPtr - Message used to send the buffer to codec
*  @param pDmmBuf - buffer id
*
*  @retval OMX_ErrorNone  - Success
*          OMX_ErrorHardware  -  Hardware Error
** ==========================================================================*/
OMX_ERRORTYPE DmmMap(DSP_HPROCESSOR ProcHandle,
                     OMX_U32 size,
                     OMX_U32 sizeUsed,
                     void* pArmPtr,
                     DMM_BUFFER_OBJ* pDmmBuf, 
                     OMX_U32 MapBufLen)
{
    OMX_ERRORTYPE eError = OMX_ErrorUndefined;
    DSP_STATUS status;
    int nSizeReserved = 0;

    if(pDmmBuf == NULL)
    {
        LCML_DPRINT("pBuf is NULL\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    if(pArmPtr == NULL)
    {
        LCML_DPRINT("pBuf is NULL\n");
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
        LCML_DPRINT("DSPProcessor_ReserveMemory() failed - error 0x%x", (int)status);
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    pDmmBuf->nSize = size;

	
    LCML_DPRINT(" DMM MAP Reserved: %p (for buf %p), size 0x%x (%d)\n", pDmmBuf->pReserved, pArmPtr, nSizeReserved,nSizeReserved);

    /* Map */
    status = DSPProcessor_Map(ProcHandle,
                              pDmmBuf->pAllocated,/* malloc'd data here*/
                              size , /* size */
                              pDmmBuf->pReserved, /* reserved space */
                              &(pDmmBuf->pMapped), /* returned map pointer */
                              0); /* final param is reserved.  set to zero. */
    if(DSP_FAILED(status))
    {
        LCML_DPRINT("DSPProcessor_Map() failed - error 0x%x", (int)status);
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    LCML_DPRINT("DMM Mapped: %p, size 0x%x (%d)\n",pDmmBuf->pMapped, size,size);

    /* Previously we used to Flush or Invalidate the mapped buffer.  This was
     * removed due to bridge is now handling the flush/invalidate operation */
    eError = OMX_ErrorNone;

EXIT:
   return eError;
}


/** ========================================================================
*  DmmUnMap () method is used to de-allocate the memory using DMM.
*
*  @param ProcHandle -  Component identification number
*  @param pMapPtr  - Map address
*  @param pResPtr - reserve adress
*
*  @retval OMX_ErrorNone  - Success
*          OMX_ErrorHardware  -  Hardware Error
** ==========================================================================*/
OMX_ERRORTYPE DmmUnMap(DSP_HPROCESSOR ProcHandle, void* pMapPtr, void* pResPtr)
{
    DSP_STATUS status = DSP_SOK;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if(pMapPtr == NULL)
    {
        LCML_DPRINT("pMapPtr is NULL\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
    if(pResPtr == NULL)
    {
        LCML_DPRINT("pResPtr is NULL\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }
    status = DSPProcessor_UnMap(ProcHandle,pMapPtr);
    if(DSP_FAILED(status))
    {
        LCML_DPRINT("DSPProcessor_UnMap() failed - error 0x%x",(int)status);
   }

    LCML_DPRINT("unreserving  structure =0x%p\n",pResPtr );
    status = DSPProcessor_UnReserveMemory(ProcHandle,pResPtr);
    if(DSP_FAILED(status))
    {
        LCML_DPRINT("DSPProcessor_UnReserveMemory() failed - error 0x%x", (int)status);
    }

EXIT:
    return eError;
}

#ifdef UNDER_CE
#define LCML_DPRINT
#else
#define LCML_DPRINT(...)    
#endif

/** ========================================================================
* FreeResources () method is used to allocate the memory using DMM.
*
* @param hInterface  - Component identification number
*
* @retval OMX_ErrorNone           Success
*         OMX_ErrorHardware       Hardware Error
** ==========================================================================*/
OMX_ERRORTYPE FreeResources (LCML_DSP_INTERFACE *hInterface)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    LCML_DSP_INTERFACE *codec;

    LCML_DPRINT("%d :: LCML:: FreeResources\n",__LINE__);
    if(hInterface->dspCodec != NULL)
    {
        LCML_FREE(hInterface->dspCodec);
        hInterface->dspCodec = NULL;
    }
    codec = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE*)hInterface->pCodecinterfacehandle)->pCodec);
    if(codec != NULL)
    {
        pthread_mutex_lock(&codec->mutex);
        LCML_DPRINT("%d :: LCML:: FreeResources\n",__LINE__);
        if(codec->g_aNotificationObjects[0]!= NULL)
        {
            LCML_FREE(codec->g_aNotificationObjects[0]);
            codec->g_aNotificationObjects[0] = NULL;
#ifdef __ERROR_PROPAGATION__
            if(codec->g_aNotificationObjects[1]!= NULL)
            {
                LCML_FREE(codec->g_aNotificationObjects[1]);
                codec->g_aNotificationObjects[1] = NULL;
            }
            if(codec->g_aNotificationObjects[2]!= NULL)
            {
                LCML_FREE(codec->g_aNotificationObjects[2]);
                codec->g_aNotificationObjects[2] = NULL;
            }
 #endif
            LCML_FREE(((LCML_CODEC_INTERFACE*)hInterface->pCodecinterfacehandle));
            hInterface->pCodecinterfacehandle = NULL;
        }
        pthread_mutex_unlock(&codec->mutex);
        pthread_mutex_destroy (&codec->mutex);
        LCML_FREE(codec);
        codec = NULL;

    }
    LCML_DPRINT("%d :: LCML:: FreeResources\n",__LINE__);
    return eError;
}

/** ========================================================================
* DeleteDspResource () method is used to allocate the memory using DMM.
*
* @param hInterface                   Component identification number
*
* @retval OMX_ErrorNone           Success
*         OMX_ErrorHardware       Hardware Error
** ==========================================================================*/
OMX_ERRORTYPE DeleteDspResource(LCML_DSP_INTERFACE *hInterface)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    DSP_STATUS status;
    DSP_STATUS nExit;
    struct DSP_NODEATTR nodeAttr;
    OMX_U32 dllinfo;
    LCML_DSP_INTERFACE *codec;

    /* Get current state of node, if it is running, then only terminate it */
    
    status = DSPNode_GetAttr(hInterface->dspCodec->hNode, &nodeAttr, sizeof(nodeAttr));
    DSP_ERROR_EXIT (status, "DeInit: Error in Node GetAtt ", EXIT);
        status = DSPNode_Terminate(hInterface->dspCodec->hNode, &nExit);
    LCML_DPRINT("%d :: LCML:: Node Has Been Terminated --1\n",__LINE__);
    codec = (LCML_DSP_INTERFACE *)(((LCML_CODEC_INTERFACE*)hInterface->pCodecinterfacehandle)->pCodec);
    if(codec->g_aNotificationObjects[0]!= NULL)
    {
    }
#ifdef __ERROR_PROPAGATION__
    if(codec->g_aNotificationObjects[1]!= NULL)
    {
       /* status = DSPNode_RegisterNotify(hInterface->dspCodec->hProc, 0, DSP_SIGNALEVENT, codec->g_aNotificationObjects[1]);
        DSP_ERROR_EXIT(status, "DSP node de-register notify", EXIT);*/
    }
#endif    
    if (hInterface->dspCodec->DeviceInfo.TypeofDevice == 1) {
        /* delete DASF node */
        status = DSPNode_Delete(hInterface->dspCodec->hDasfNode);
        DSP_ERROR_EXIT (status, "DeInit: DASF Node Delete ", EXIT);
        LCML_DPRINT("%d :: Deleted the DASF node Successfully\n",__LINE__);
    }
	/* delete SN */
	status = DSPNode_Delete(hInterface->dspCodec->hNode);
    LCML_DPRINT("%d :: Deleted the node Successfully\n",__LINE__);

    LCML_DPRINT("%d :: Entering UnLoadDLLs \n", __LINE__);
    for(dllinfo=0;dllinfo < hInterface->dspCodec->NodeInfo.nNumOfDLLs ;dllinfo++)
    {
        LCML_DPRINT("%d :: Register Component Node\n",hInterface->dspCodec->NodeInfo.AllUUIDs[dllinfo].eDllType);
        status = DSPManager_UnregisterObject ((struct DSP_UUID *) hInterface->dspCodec->NodeInfo.AllUUIDs[dllinfo].uuid,
                                                                                        hInterface->dspCodec->NodeInfo.AllUUIDs[dllinfo].eDllType);
        /*DSP_ERROR_EXIT (status, "Unregister DSP Object, Socket UUID ", EXIT);*/
    }

    /* detach processor from gpp */
    status = DSPProcessor_Detach(hInterface->dspCodec->hProc);
    DSP_ERROR_EXIT (status, "DeInit: DSP Processor Detach ", EXIT);

    status = DspManager_Close(0, NULL);
    DSP_ERROR_EXIT (status, "DeInit: DSPManager Close ", EXIT);

EXIT:
    return eError;

}


/** ========================================================================
* This is the function run in the message thread.  It waits for an event
* signal from Bridge and then reads all available messages.
*
* @param[in] arg  Unused - Required by pthreads API
*
* @retval  OMX_ErrorNone Success, ready to roll
** ==========================================================================*/
void* MessagingThread(void* arg)
{
    /* OMX_ERRORTYPE eError = OMX_ErrorUndefined; */
    DSP_STATUS status = DSP_SOK;
    struct DSP_MSG msg = {0,0,0};
    unsigned int index=0;
    LCML_MESSAGINGTHREAD_STATE threadState = EMessagingThreadCodecStopped;
    int waitForEventsTimeout = 1000;
    int getMessageTimeout = 10;

    LCML_DPRINT("\n*****************Inside the Messaging thread************\n");
#ifdef __PERF_INSTRUMENTATION__
    ((LCML_DSP_INTERFACE *)arg)->pPERFcomp = 
        PERF_Create(PERF_FOURCC('C','M','L','T'),
                    PERF_ModuleAudioDecode | PERF_ModuleAudioEncode |
                    PERF_ModuleVideoDecode | PERF_ModuleVideoEncode |
                    PERF_ModuleImageDecode | PERF_ModuleImageEncode |
                    PERF_ModuleCommonLayer);
#endif

    /* get message from DSP */
    while (1)
    {
        if (((LCML_DSP_INTERFACE *)arg)->pshutdownFlag == 1)
        {
            LCML_DPRINT("Breaking out of loop inmessaging thread \n");
            break;
        }

        if (threadState == EMessagingThreadCodecRunning) {
            waitForEventsTimeout = 10000;
            getMessageTimeout = 10;
        }
        /* set the timeouts lower when the codec is stopped so that thread deletion response will be faster */        
        else if (threadState == EMessagingThreadCodecStopped) {        
            waitForEventsTimeout = 10;
            getMessageTimeout = 10;
        }

#ifdef __ERROR_PROPAGATION__        
        status = DSPManager_WaitForEvents(((LCML_DSP_INTERFACE *)arg)->g_aNotificationObjects, 3, &index, waitForEventsTimeout);
#else
        status = DSPManager_WaitForEvents(((LCML_DSP_INTERFACE *)arg)->g_aNotificationObjects, 1, &index, waitForEventsTimeout);
#endif       
        if (DSP_SUCCEEDED(status))
        {
            LCML_DPRINT("GOT notofication FROM DSP HANDLE IT \n");
#ifdef __ERROR_PROPAGATION__            
            if (index == 0){
#endif            
            /* Pull all available messages out of the message loop */
            while (DSP_SUCCEEDED(status))
            {
                /* since there is a message waiting, grab it and pass  */
                status = DSPNode_GetMessage(((LCML_DSP_INTERFACE *)arg)->dspCodec->hNode, &msg, getMessageTimeout);
                if (DSP_SUCCEEDED(status))
                {
                    int streamId = (msg.dwCmd & 0x000000ff);
                    int commandId = msg.dwCmd & 0xffffff00;
                    TMMCodecBufferType bufType ;/* = EMMCodecScratchBuffer; */
                    TUsnCodecEvent  event = EMMCodecInternalError;
                    void * args[10];
                    TArmDspCommunicationStruct  *tmpDspStructAddress = NULL;
                    LCML_DSP_INTERFACE *hDSPInterface = ((LCML_DSP_INTERFACE *)arg) ;
                    DMM_BUFFER_OBJ* pDmmBuf = NULL;

                    LCML_DPRINT("GOT MESSAGE FROM DSP HANDLE IT  %d \n", index);
                    LCML_DPRINT("msg = 0x%lx arg1 = 0x%lx arg2 = 0x%lx", msg.dwCmd, msg.dwArg1, msg.dwArg2);
                    LCML_DPRINT("Message EMMCodecOuputBuffer outside loop");
#ifdef __PERF_INSTRUMENTATION__
                    PERF_ReceivedCommand(hDSPInterface->pPERFcomp,
                                         msg.dwCmd, msg.dwArg1,
                                         PERF_ModuleSocketNode);
#endif

                    if (commandId == USN_DSPMSG_BUFF_FREE )
                    {
                        threadState = EMessagingThreadCodecRunning;                    
 #ifdef __PERF_INSTRUMENTATION__
                                        PERF_XferingBuffer(hDSPInterface->pPERFcomp,
                                                          args [1],
                                                          (OMX_U32) args [8],
                                                          PERF_ModuleSocketNode,
                                                          PERF_ModuleLLMM);
#endif                       
                        pthread_mutex_lock(&hDSPInterface->mutex);
                        if (!(streamId % 2))
                        {
                            int i = 0;
                            int j = 0;
                            bufType = streamId + EMMCodecStream0;
                            LCML_DPRINT("Address Arminputstorage %p \n", ((LCML_DSP_INTERFACE *)arg)->Arminputstorage);
                            LCML_DPRINT("Address dspinterface %p \n", ((LCML_DSP_INTERFACE *)arg));

                            hDSPInterface->iBufinputcount = hDSPInterface->iBufinputcount % QUEUE_SIZE;
                            i = hDSPInterface->iBufinputcount;
                            while(j++ < QUEUE_SIZE)
                            {
                                if (hDSPInterface->Arminputstorage[i] != NULL && hDSPInterface ->Arminputstorage[i]->iArmArg == msg.dwArg1)
                                {
                                    LCML_DPRINT("InputBuffer loop");
                                    tmpDspStructAddress = ((LCML_DSP_INTERFACE *)arg)->Arminputstorage[i] ;
                                    hDSPInterface->Arminputstorage[i] =NULL;
                                    pDmmBuf = hDSPInterface ->dspCodec->InDmmBuffer;
                                    pDmmBuf = pDmmBuf + (tmpDspStructAddress->BufInindex);
                                    LCML_DPRINT("Address output  matching index= %ld \n ",tmpDspStructAddress->BufInindex);
                                    break;
                                }
                                i++;
                                i = i % QUEUE_SIZE;
                                LCML_DPRINT("Message EMMCodecInputBuffer loop");
                            }
                        }
                        else if (streamId % 2)
                        {
                            int i = 0;
                            int j = 0;
                            bufType = streamId + EMMCodecStream0;;
                            LCML_DPRINT("Address Armoutputstorage %p \n ",((LCML_DSP_INTERFACE *)arg)->Armoutputstorage);
                            LCML_DPRINT("Address dspinterface %p \n ",((LCML_DSP_INTERFACE *)arg));

                            hDSPInterface->iBufoutputcount = hDSPInterface->iBufoutputcount % QUEUE_SIZE;
                            i = hDSPInterface->iBufoutputcount;
                            while(j++ < QUEUE_SIZE)
                            {
                                if( hDSPInterface ->Armoutputstorage[i] != NULL
                                        && hDSPInterface ->Armoutputstorage[i]->iArmArg == msg.dwArg1)
                                {
                                    LCML_DPRINT("output buffer loop");
                                    tmpDspStructAddress = hDSPInterface->Armoutputstorage[i] ;
                                    hDSPInterface ->Armoutputstorage[i] =NULL;
                                    pDmmBuf = hDSPInterface ->dspCodec->OutDmmBuffer;
                                    pDmmBuf = pDmmBuf + (tmpDspStructAddress->Bufoutindex);
                                    LCML_DPRINT("Address output  matching index= %ld\n ",tmpDspStructAddress->Bufoutindex);
                                    break;
                                }
                                i++;
                                i = i % QUEUE_SIZE;
                                LCML_DPRINT("Message EMMCodecOuputBuffer loop");
                            }
                        }

                        if (tmpDspStructAddress != NULL)
                        {
                            char *tmp2 = NULL;
                            
                            event = EMMCodecBufferProcessed;
                            args[0] = (void *) bufType;
                            args[1] = (void *) tmpDspStructAddress->iArmbufferArg; /* arm address fpr buffer */
                            args[2] = (void *) tmpDspStructAddress->iBufferSize;
                            args[3] = (void *) tmpDspStructAddress->iArmParamArg; /* arm address for param */
                            args[4] = (void *) tmpDspStructAddress->iParamSize;
                            args[5] = (void *) tmpDspStructAddress->iArmArg;
                            args[6] = (void *) arg;  /* handle */
                            args[7] = (void *) tmpDspStructAddress->iUsrArg;  /* user arguments */

                            if (((LCML_DSP_INTERFACE *)arg)->bUsnEos) {
                                ((OMX_BUFFERHEADERTYPE*)args[7])->nFlags |= tmpDspStructAddress->iEOSFlag;
                            }
                            /* USN updates*/
                            args[8] = (void *) tmpDspStructAddress->iBufSizeUsed ;
                            /* managing buffers  and free buffer logic */

                            LCML_DPRINT("GOT MESSAGE EMMCodecBufferProcessed  and now unmapping buffer type %p \n", args[2]);

                            if (tmpDspStructAddress ->iBufferPtr != (OMX_U32)NULL)
                            {
                                LCML_DPRINT("GOT MESSAGE EMMCodecBufferProcessed and now unmapping buufer %lx\n size=%ld",
                                             tmpDspStructAddress ->iBufferPtr, tmpDspStructAddress ->iBufferSize);

                                DmmUnMap(hDSPInterface->dspCodec->hProc,
                                         (void*)tmpDspStructAddress->iBufferPtr,
                                         pDmmBuf->bufReserved);
                            }

                            if (tmpDspStructAddress->iParamPtr != (OMX_U32)NULL)
                            {
                                LCML_DPRINT("GOT MESSAGE EMMCodecBufferProcessed and now unmapping parameter buufer\n");

                                DmmUnMap(hDSPInterface ->dspCodec->hProc,
                                         (void*)tmpDspStructAddress->iParamPtr,
                                         pDmmBuf->paramReserved);
                            }

                            LCML_DPRINT("GOT MESSAGE EMMCodecBufferProcessed  and now unmapping  structure =0x%p\n",tmpDspStructAddress );
                            DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);
                            tmp2 = (char *)tmpDspStructAddress;
                            tmp2 = (tmp2 - 128);
                            if (tmp2)
                            {
                                LCML_FREE(tmp2);
                                tmp2 = NULL;
                            }

                            /* free(tmpDspStructAddress); */
                            tmpDspStructAddress = NULL;
                        }
                        pthread_mutex_unlock(&hDSPInterface->mutex);
                    } /* End of USN_DSPMSG_BUFF_FREE */

                    else if (commandId == USN_DSPACK_STOP)
                    {
                        threadState = EMessagingThreadCodecStopped;                    
                        
                        /* Start of USN_DSPACK_STOP */
                        int i = 0;
                        int j = 0;
                        int k = 0;
                        LCML_DPRINT("GOT MESSAGE EMMCodecProcessingStoped \n");
                        pthread_mutex_lock(&hDSPInterface->mutex);
                        LCML_DPRINT("LCMLSTOP: hDSPInterface->dspCodec->DeviceInfo.TypeofDevice %d\n", hDSPInterface->dspCodec->DeviceInfo.TypeofDevice);
                        if (hDSPInterface->dspCodec->DeviceInfo.TypeofDevice == 0)
                        {
                            j = 0;
                            hDSPInterface->iBufinputcount = hDSPInterface->iBufinputcount % QUEUE_SIZE;
                            i = hDSPInterface->iBufinputcount;
                            
                            hDSPInterface->iBufoutputcount = hDSPInterface->iBufoutputcount % QUEUE_SIZE;
                            k = hDSPInterface->iBufoutputcount;

                            while(j++ < QUEUE_SIZE)
                            {
                                LCML_DPRINT("LCMLSTOP: %d hDSPInterface->Arminputstorage[i] = %p\n", i, hDSPInterface->Arminputstorage[i]);
                                if (hDSPInterface->Arminputstorage[i] != NULL)
                                {
                                    char *tmp2 = NULL;
                                    /* callback the component with the buffers that are being freed */
                                    tmpDspStructAddress = hDSPInterface->Arminputstorage[i] ;

                                    pDmmBuf = hDSPInterface ->dspCodec->InDmmBuffer;
                                    pDmmBuf = pDmmBuf + (tmpDspStructAddress->BufInindex);
                                    LCML_DPRINT("pDmmBuf->pMapped %p\n", pDmmBuf->pMapped);

                                    event = EMMCodecBufferProcessed;
                                    args[0] = (void *) EMMCodecInputBuffer;
                                    args[1] = (void *) tmpDspStructAddress->iArmbufferArg; /* arm address fpr buffer */
                                    args[2] = (void *) tmpDspStructAddress->iBufferSize;
                                    args[3] = (void *) tmpDspStructAddress->iArmParamArg; /* arm address for param */
                                    args[4] = (void *) tmpDspStructAddress->iParamSize;
                                    args[5] = (void *) tmpDspStructAddress->iArmArg;
                                    args[6] = (void *) arg;  /* handle */                            
                                    args[7] = (void *) tmpDspStructAddress->iUsrArg;  /* user arguments */
                                    /* USN updates*/
                                    args[8] = (void *) tmpDspStructAddress->iBufSizeUsed ;

                                    LCML_DPRINT("LCMLSTOP: tmpDspStructAddress->iBufferPtr %p, tmpDspStructAddress->iParamPtr %p, msg.dwArg1 %p\n",
                                            tmpDspStructAddress->iBufferPtr,
                                            tmpDspStructAddress->iParamPtr,
                                            msg.dwArg1);
                                    if (tmpDspStructAddress->iBufferPtr != (OMX_U32)NULL)
                                    {
                                        DmmUnMap(hDSPInterface->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iBufferPtr,
                                                 pDmmBuf->bufReserved);
                                    }

                                    if (tmpDspStructAddress->iParamPtr != (OMX_U32)NULL)
                                    {
                                        DmmUnMap(hDSPInterface ->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iParamPtr,
                                                 pDmmBuf->paramReserved);
                                    }
                                    DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);

                                    tmp2 = (char*)tmpDspStructAddress;
                                    tmp2 = ( tmp2 - 128);
                                    LCML_DPRINT("%d :: LCML:: FreeResources\n",__LINE__);
                                    if (tmp2)
                                    {
                                        LCML_FREE(tmp2);
                                        tmp2 = NULL;
                                    }
                                    hDSPInterface->Arminputstorage[i] = NULL;
                                    tmpDspStructAddress     = NULL;
#ifdef __PERF_INSTRUMENTATION__
                                    PERF_XferingBuffer(hDSPInterface->pPERFcomp,
                                                      args [1],
                                                      (OMX_U32) args [2],
                                                      PERF_ModuleSocketNode,
                                                      PERF_ModuleLLMM);
#endif
                                    hDSPInterface->dspCodec->Callbacks.LCML_Callback(event,args);
                                }
                                
                                LCML_DPRINT("LCMLSTOP: %d hDSPInterface->Armoutputstorage[k] = %p\n", k, hDSPInterface->Armoutputstorage[k]);
                                if (hDSPInterface->Armoutputstorage[k] != NULL)
                                {
                                    char * tmp2 = NULL;
                                    tmpDspStructAddress = hDSPInterface->Armoutputstorage[k] ;

                                    pDmmBuf = hDSPInterface ->dspCodec->OutDmmBuffer;
                                    pDmmBuf = pDmmBuf + (tmpDspStructAddress->Bufoutindex);

                                    event = EMMCodecBufferProcessed;
                                    args[0] = (void *) EMMCodecOuputBuffer;
                                    args[1] = (void *) tmpDspStructAddress->iArmbufferArg; /* arm address fpr buffer */
                                    args[2] = (void *) tmpDspStructAddress->iBufferSize;
                                    args[3] = (void *) tmpDspStructAddress->iArmParamArg; /* arm address for param */
                                    args[4] = (void *) tmpDspStructAddress->iParamSize;
                                    args[5] = (void *) tmpDspStructAddress->iArmArg;
                                    args[6] = (void *) arg;  /* handle */                           
                                    args[7] = (void *) tmpDspStructAddress->iUsrArg;  /* user arguments */
                                    /* USN updates*/

                                    LCML_DPRINT("LCMLSTOP: tmpDspStructAddress->iBufferPtr %p, tmpDspStructAddress->iParamPtr %p, msg.dwArg1 %p\n",
                                            tmpDspStructAddress->iBufferPtr,
                                            tmpDspStructAddress->iParamPtr,
                                            msg.dwArg1);
                                    if (tmpDspStructAddress ->iBufferPtr != (OMX_U32)NULL)
                                    {
                                    LCML_DPRINT("tmpDspStructAddress ->iBufferPtr is not NULL\n");
                                        DmmUnMap(hDSPInterface->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iBufferPtr,
                                                 pDmmBuf->bufReserved);
                                    }

                                    if (tmpDspStructAddress->iParamPtr != (OMX_U32)NULL)
                                    {
                                    LCML_DPRINT("tmpDspStructAddress->iParamPtr is not NULL\n");
                                        DmmUnMap(hDSPInterface ->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iParamPtr,
                                                 pDmmBuf->paramReserved);
                                    }
                                    DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);

                                    tmp2 = (char *) tmpDspStructAddress;
                                    tmp2 = ( tmp2 - 128);
                                    LCML_DPRINT("%d :: LCML:: FreeResources\n",__LINE__);
                                    if(tmp2)
                                    {
                                        LCML_FREE(tmp2);
                                        tmp2 = NULL;
                                    }
                                    tmpDspStructAddress->iBufSizeUsed = 0;
                                    args[8] = (void *) tmpDspStructAddress->iBufSizeUsed ;

                                    hDSPInterface->Armoutputstorage[k] = NULL;
                                    tmpDspStructAddress = NULL;
#ifdef __PERF_INSTRUMENTATION__
                                    PERF_XferingBuffer(hDSPInterface->pPERFcomp,
                                                      args[1],
                                                      (OMX_U32) args[2],
                                                      PERF_ModuleSocketNode,
                                                      PERF_ModuleLLMM);
#endif
                                    hDSPInterface->dspCodec->Callbacks.LCML_Callback(event,args);
                                }
                                i++;
                                i = i % QUEUE_SIZE;
                                k++;
                                k = k % QUEUE_SIZE;
                            }
                        }
                        pthread_mutex_unlock(&hDSPInterface->mutex);
                        args[6] = (void *) arg;  /* handle */
                        event = EMMCodecProcessingStoped;
                        
                    } /* end of USN_DSPACK_STOP */
                    else if (commandId == USN_DSPACK_PAUSE)
                    {
                    
                        event = EMMCodecProcessingPaused;
                        LCML_DPRINT("GOT MESSAGE EMMCodecProcessingPaused \n");
                        args[6] = (void *) arg;  /* handle */
                    }
                    else if (commandId == USN_DSPMSG_EVENT)
                    {
                        threadState = EMessagingThreadCodecStopped;                    
                    
                        event = EMMCodecDspError;
                        LCML_DPRINT("GOT MESSAGE EMMCodecDspError \n");
                        args[0] = (void *) msg.dwCmd;
                        args[4] = (void *) msg.dwArg1;
                        args[5] = (void *) msg.dwArg2;
                        args[6] = (void *) arg;  /* handle */
                    }
                    else if (commandId == USN_DSPACK_ALGCTRL)
                    {
                    
                        int i;
                        event = EMMCodecAlgCtrlAck;
                        pthread_mutex_lock(&hDSPInterface->mutex);
                        for (i = 0; i < QUEUE_SIZE; i++)
                        {
                            pDmmBuf = ((LCML_DSP_INTERFACE *)arg)->pAlgcntlDmmBuf[i];
                            if ((pDmmBuf) &&
                                (((LCML_DSP_INTERFACE *)arg)->algcntlmapped[i]) &&
                                (pDmmBuf->pMapped == (void *)msg.dwArg2))
                            {
                                DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);
                                LCML_FREE(pDmmBuf);
                                pDmmBuf = NULL;
                                ((LCML_DSP_INTERFACE *)arg)->algcntlmapped[i] = 0;
                                ((LCML_DSP_INTERFACE *)arg)->pAlgcntlDmmBuf[i] = NULL;
                                break;
                            }
                        }
                        args[0] = (void *) msg.dwArg1;
                        args[6] = (void *) arg;  /* handle */
                        LCML_DPRINT("GOT MESSAGE USN_DSPACK_ALGCTRL \n");
                        pthread_mutex_unlock(&hDSPInterface->mutex);
                    }
                    else if (commandId == USN_DSPACK_STRMCTRL)
                    {
                    
                        int i = 0;
                        int j = 0;
                        int ackType = 0;
                        pthread_mutex_lock(&hDSPInterface->mutex);
                        if (hDSPInterface->flush_pending[0] && (streamId == 0) && (msg.dwArg1 == USN_ERR_NONE))
                        {
                            hDSPInterface->flush_pending[0] = 0;
                            ackType = USN_STRMCMD_FLUSH;
                            j = 0;
                            hDSPInterface->iBufinputcount = hDSPInterface->iBufinputcount % QUEUE_SIZE;
                            i = hDSPInterface->iBufinputcount;
                            while(j++ < QUEUE_SIZE)
                            {
                                LCML_DPRINT("LCMLFLUSH: %d hDSPInterface->Arminputstorage[i] = %p\n", i, hDSPInterface->Arminputstorage[i]);
                                if (hDSPInterface->Arminputstorage[i] != NULL)
                                {
                                    char *tmp2 = NULL;
                                    tmpDspStructAddress = hDSPInterface->Arminputstorage[i] ;

                                    pDmmBuf = hDSPInterface ->dspCodec->InDmmBuffer;
                                    pDmmBuf = pDmmBuf + (tmpDspStructAddress->BufInindex);
                                    LCML_DPRINT("pDmmBuf->pMapped %p\n", pDmmBuf->pMapped);

                                    event = EMMCodecBufferProcessed;
                                    args[0] = (void *) EMMCodecInputBuffer;
                                    args[1] = (void *) tmpDspStructAddress->iArmbufferArg;
                                    args[2] = (void *) tmpDspStructAddress->iBufferSize;
                                    args[3] = (void *) tmpDspStructAddress->iArmParamArg;
                                    args[4] = (void *) tmpDspStructAddress->iParamSize;
                                    args[5] = (void *) tmpDspStructAddress->iArmArg;
                                    args[6] = (void *) arg;            
                                    args[7] = (void *) tmpDspStructAddress->iUsrArg;

                                    args[8] = (void *) tmpDspStructAddress->iBufSizeUsed ;

                                    LCML_DPRINT("LCMLFLUSH: tmpDspStructAddress->iBufferPtr %p, tmpDspStructAddress->iParamPtr %p, msg.dwArg1 %p\n",
                                            tmpDspStructAddress->iBufferPtr,
                                            tmpDspStructAddress->iParamPtr,
                                            msg.dwArg1);
                                    if (tmpDspStructAddress->iBufferPtr != (OMX_U32)NULL)
                                    {
                                        DmmUnMap(hDSPInterface->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iBufferPtr,
                                                 pDmmBuf->bufReserved);
                                    }

                                    if (tmpDspStructAddress->iParamPtr != (OMX_U32)NULL)
                                    {
                                        DmmUnMap(hDSPInterface ->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iParamPtr,
                                                 pDmmBuf->paramReserved);
                                    }
                                    DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);

                                    tmp2 = (char*)tmpDspStructAddress;
                                    tmp2 = ( tmp2 - 128);
                                    LCML_DPRINT("%d :: LCML:: FreeResources\n",__LINE__);
                                    if (tmp2)
                                    {
                                        LCML_FREE(tmp2);
                                        tmp2 = NULL;
                                    }
                                    hDSPInterface->Arminputstorage[i] = NULL;
                                    tmpDspStructAddress     = NULL;
#ifdef __PERF_INSTRUMENTATION__
                                    PERF_XferingBuffer(hDSPInterface->pPERFcomp,
                                                      args [1],
                                                      (OMX_U32) args [2],
                                                      PERF_ModuleSocketNode,
                                                      PERF_ModuleLLMM);
#endif
                                    hDSPInterface->dspCodec->Callbacks.LCML_Callback(event,args);
                                }
                                i++;
                                i = i % QUEUE_SIZE;
                            }
                            for (i = 0; i < QUEUE_SIZE; i++)
                            {
                                pDmmBuf = ((LCML_DSP_INTERFACE *)arg)->pStrmcntlDmmBuf[i];
                                if ((pDmmBuf) &&
                                    (((LCML_DSP_INTERFACE *)arg)->strmcntlmapped[i]) &&
                                    (pDmmBuf->pMapped == (void *)msg.dwArg2))
                                {
                                    DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);
                                    LCML_FREE(pDmmBuf);
                                    pDmmBuf = NULL;
                                    ((LCML_DSP_INTERFACE *)arg)->strmcntlmapped[i] = 0;
                                    ((LCML_DSP_INTERFACE *)arg)->pStrmcntlDmmBuf[i] = NULL;
                                    break;
                                }
                            }
                        } 
                        else if (hDSPInterface->flush_pending[1] && (streamId == 1) && (msg.dwArg1 == USN_ERR_NONE))
                        {
                            hDSPInterface->flush_pending[1] = 0;
                            ackType = USN_STRMCMD_FLUSH;                            
                            j = 0;
                            hDSPInterface->iBufoutputcount = hDSPInterface->iBufoutputcount % QUEUE_SIZE;
                            i = hDSPInterface->iBufoutputcount;
                            while(j++ < QUEUE_SIZE)
                            {
                                LCML_DPRINT("LCMLFLUSH: %d hDSPInterface->Armoutputstorage[i] = %p\n", i, hDSPInterface->Armoutputstorage[i]);
                                if (hDSPInterface->Armoutputstorage[i] != NULL)
                                {
                                    char * tmp2 = NULL;
                                    tmpDspStructAddress = hDSPInterface->Armoutputstorage[i] ;

                                    pDmmBuf = hDSPInterface ->dspCodec->OutDmmBuffer;
                                    pDmmBuf = pDmmBuf + (tmpDspStructAddress->Bufoutindex);

                                    event = EMMCodecBufferProcessed;
                                    args[0] = (void *) EMMCodecOuputBuffer;
                                    args[1] = (void *) tmpDspStructAddress->iArmbufferArg;
                                    args[2] = (void *) tmpDspStructAddress->iBufferSize;
                                    args[3] = (void *) tmpDspStructAddress->iArmParamArg;
                                    args[4] = (void *) tmpDspStructAddress->iParamSize;
                                    args[5] = (void *) tmpDspStructAddress->iArmArg;
                                    args[6] = (void *) arg;           
                                    args[7] = (void *) tmpDspStructAddress->iUsrArg;


                                    LCML_DPRINT("LCMLFLUSH: tmpDspStructAddress->iBufferPtr %p, tmpDspStructAddress->iParamPtr %p, msg.dwArg1 %p\n",
                                            tmpDspStructAddress->iBufferPtr,
                                            tmpDspStructAddress->iParamPtr,
                                            msg.dwArg1);
                                    if (tmpDspStructAddress ->iBufferPtr != (OMX_U32)NULL)
                                    {
                                    LCML_DPRINT("tmpDspStructAddress ->iBufferPtr is not NULL\n");
                                        DmmUnMap(hDSPInterface->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iBufferPtr,
                                                 pDmmBuf->bufReserved);
                                    }

                                    if (tmpDspStructAddress->iParamPtr != (OMX_U32)NULL)
                                    {
                                    LCML_DPRINT("tmpDspStructAddress->iParamPtr is not NULL\n");
                                        DmmUnMap(hDSPInterface ->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iParamPtr,
                                                 pDmmBuf->paramReserved);
                                    }
                                    DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);

                                    tmp2 = (char *) tmpDspStructAddress;
                                    tmp2 = ( tmp2 - 128);
                                    LCML_DPRINT("%d :: LCML:: FreeResources\n",__LINE__);
                                    if(tmp2)
                                    {
                                        LCML_FREE(tmp2);
                                        tmp2 = NULL;
                                    }
                                    tmpDspStructAddress->iBufSizeUsed = 0;
                                    args[8] = (void *) tmpDspStructAddress->iBufSizeUsed ;

                                    hDSPInterface->Armoutputstorage[i] = NULL;
                                    tmpDspStructAddress = NULL;
#ifdef __PERF_INSTRUMENTATION__
                                    PERF_XferingBuffer(hDSPInterface->pPERFcomp,
                                                      args[1],
                                                      (OMX_U32) args[2],
                                                      PERF_ModuleSocketNode,
                                                      PERF_ModuleLLMM);
#endif
                                    hDSPInterface->dspCodec->Callbacks.LCML_Callback(event,args);
                                }
                                i++;
                                i = i % QUEUE_SIZE;
                            }
                            for (i = 0; i < QUEUE_SIZE; i++)
                            {
                                pDmmBuf = ((LCML_DSP_INTERFACE *)arg)->pStrmcntlDmmBuf[i];
                                if ((pDmmBuf) &&
                                    (((LCML_DSP_INTERFACE *)arg)->strmcntlmapped[i]) &&
                                    (pDmmBuf->pMapped == (void *)msg.dwArg2))
                                {
                                    DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);
                                    LCML_FREE(pDmmBuf);
                                    pDmmBuf = NULL;
                                    ((LCML_DSP_INTERFACE *)arg)->strmcntlmapped[i] = 0;
                                    ((LCML_DSP_INTERFACE *)arg)->pStrmcntlDmmBuf[i] = NULL;
                                    break;
                                }
                            }
                        }
                        if (hDSPInterface->flush_pending[2] && (streamId == 2) && (msg.dwArg1 == USN_ERR_NONE))
                        {
                            hDSPInterface->flush_pending[0] = 0;
                            ackType = USN_STRMCMD_FLUSH;                            
                            j = 0;
                            hDSPInterface->iBufinputcount = hDSPInterface->iBufinputcount % QUEUE_SIZE;
                            i = hDSPInterface->iBufinputcount;
                            while(j++ < QUEUE_SIZE)
                            {
                                LCML_DPRINT("LCMLFLUSH (port 2): %d hDSPInterface->Arminputstorage[i] = %p (stream ID %d)\n", i, hDSPInterface->Arminputstorage[i], streamId);
                                if ((hDSPInterface->Arminputstorage[i] != NULL) && (hDSPInterface->Arminputstorage[i]->iStreamID == streamId))
                                {
                                    char *tmp2 = NULL;
                                    tmpDspStructAddress = hDSPInterface->Arminputstorage[i] ;

                                    pDmmBuf = hDSPInterface ->dspCodec->InDmmBuffer;
                                    pDmmBuf = pDmmBuf + (tmpDspStructAddress->BufInindex);
                                    LCML_DPRINT("pDmmBuf->pMapped %p\n", pDmmBuf->pMapped);

                                    event = EMMCodecBufferProcessed;
                                    args[0] = (void *) EMMCodecInputBuffer;
                                    args[1] = (void *) tmpDspStructAddress->iArmbufferArg;
                                    args[2] = (void *) tmpDspStructAddress->iBufferSize;
                                    args[3] = (void *) tmpDspStructAddress->iArmParamArg;
                                    args[4] = (void *) tmpDspStructAddress->iParamSize;
                                    args[5] = (void *) tmpDspStructAddress->iArmArg;
                                    args[6] = (void *) arg;            
                                    args[7] = (void *) tmpDspStructAddress->iUsrArg;

                                    args[8] = (void *) tmpDspStructAddress->iBufSizeUsed ;

                                    LCML_DPRINT("LCMLFLUSH: tmpDspStructAddress->iBufferPtr %p, tmpDspStructAddress->iParamPtr %p, msg.dwArg1 %p\n",
                                            tmpDspStructAddress->iBufferPtr,
                                            tmpDspStructAddress->iParamPtr,
                                            msg.dwArg1);
                                    if (tmpDspStructAddress->iBufferPtr != (OMX_U32)NULL)
                                    {
                                        DmmUnMap(hDSPInterface->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iBufferPtr,
                                                 pDmmBuf->bufReserved);
                                    }

                                    if (tmpDspStructAddress->iParamPtr != (OMX_U32)NULL)
                                    {
                                        DmmUnMap(hDSPInterface ->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iParamPtr,
                                                 pDmmBuf->paramReserved);
                                    }
                                    DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);

                                    tmp2 = (char*)tmpDspStructAddress;
                                    tmp2 = ( tmp2 - 128);
                                    LCML_DPRINT("%d :: LCML:: FreeResources\n",__LINE__);
                                    if (tmp2)
                                    {
                                        LCML_FREE(tmp2);
                                        tmp2 = NULL;
                                    }
                                    hDSPInterface->Arminputstorage[i] = NULL;
                                    tmpDspStructAddress     = NULL;
#ifdef __PERF_INSTRUMENTATION__
                                    PERF_XferingBuffer(hDSPInterface->pPERFcomp,
                                                      args [1],
                                                      (OMX_U32) args [2],
                                                      PERF_ModuleSocketNode,
                                                      PERF_ModuleLLMM);
#endif
                                    hDSPInterface->dspCodec->Callbacks.LCML_Callback(event,args);
                                }
                                i++;
                                i = i % QUEUE_SIZE;
                            }
                        } 
                        else if (hDSPInterface->flush_pending[3] && (streamId == 3) && (msg.dwArg1 == USN_ERR_NONE))
                        {
                            hDSPInterface->flush_pending[1] = 0;
                            ackType = USN_STRMCMD_FLUSH;                            
                            j = 0;
                            hDSPInterface->iBufoutputcount = hDSPInterface->iBufoutputcount % QUEUE_SIZE;
                            i = hDSPInterface->iBufoutputcount;
                            while(j++ < QUEUE_SIZE)
                            {
                                LCML_DPRINT("LCMLFLUSH: %d hDSPInterface->Armoutputstorage[i] = %p (stream id %d)\n", i, hDSPInterface->Armoutputstorage[i], streamId);
                                if ((hDSPInterface->Armoutputstorage[i] != NULL) && (hDSPInterface->Armoutputstorage[i]->iStreamID == streamId))
                                {
                                    char * tmp2 = NULL;
                                    tmpDspStructAddress = hDSPInterface->Armoutputstorage[i] ;

                                    pDmmBuf = hDSPInterface ->dspCodec->OutDmmBuffer;
                                    pDmmBuf = pDmmBuf + (tmpDspStructAddress->Bufoutindex);

                                    event = EMMCodecBufferProcessed;
                                    args[0] = (void *) EMMCodecOuputBuffer;
                                    args[1] = (void *) tmpDspStructAddress->iArmbufferArg;
                                    args[2] = (void *) tmpDspStructAddress->iBufferSize;
                                    args[3] = (void *) tmpDspStructAddress->iArmParamArg;
                                    args[4] = (void *) tmpDspStructAddress->iParamSize;
                                    args[5] = (void *) tmpDspStructAddress->iArmArg;
                                    args[6] = (void *) arg;           
                                    args[7] = (void *) tmpDspStructAddress->iUsrArg;


                                    LCML_DPRINT("LCMLFLUSH: tmpDspStructAddress->iBufferPtr %p, tmpDspStructAddress->iParamPtr %p, msg.dwArg1 %p\n",
                                            tmpDspStructAddress->iBufferPtr,
                                            tmpDspStructAddress->iParamPtr,
                                            msg.dwArg1);
                                    if (tmpDspStructAddress ->iBufferPtr != (OMX_U32)NULL)
                                    {
                                    LCML_DPRINT("tmpDspStructAddress ->iBufferPtr is not NULL\n");
                                        DmmUnMap(hDSPInterface->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iBufferPtr,
                                                 pDmmBuf->bufReserved);
                                    }

                                    if (tmpDspStructAddress->iParamPtr != (OMX_U32)NULL)
                                    {
                                    LCML_DPRINT("tmpDspStructAddress->iParamPtr is not NULL\n");
                                        DmmUnMap(hDSPInterface ->dspCodec->hProc,
                                                 (void*)tmpDspStructAddress->iParamPtr,
                                                 pDmmBuf->paramReserved);
                                    }
                                    DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);

                                    tmp2 = (char *) tmpDspStructAddress;
                                    tmp2 = ( tmp2 - 128);
                                    LCML_DPRINT("%d :: LCML:: FreeResources\n",__LINE__);
                                    if(tmp2)
                                    {
                                        LCML_FREE(tmp2);
                                        tmp2 = NULL;
                                    }
                                    tmpDspStructAddress->iBufSizeUsed = 0;
                                    args[8] = (void *) tmpDspStructAddress->iBufSizeUsed ;

                                    hDSPInterface->Armoutputstorage[i] = NULL;
                                    tmpDspStructAddress = NULL;
#ifdef __PERF_INSTRUMENTATION__
                                    PERF_XferingBuffer(hDSPInterface->pPERFcomp,
                                                      args[1],
                                                      (OMX_U32) args[2],
                                                      PERF_ModuleSocketNode,
                                                      PERF_ModuleLLMM);
#endif
                                    hDSPInterface->dspCodec->Callbacks.LCML_Callback(event,args);
                                }
                                i++;
                                i = i % QUEUE_SIZE;
                            }
                        }

                        if (ackType != USN_STRMCMD_FLUSH) {
                            for (i = 0; i < QUEUE_SIZE; i++)
                            {
                                pDmmBuf = ((LCML_DSP_INTERFACE *)arg)->pStrmcntlDmmBuf[i];
                                if ((pDmmBuf) &&
                                    (((LCML_DSP_INTERFACE *)arg)->strmcntlmapped[i]) &&
                                    (pDmmBuf->pMapped == (void *)msg.dwArg2))
                                {
                                    DmmUnMap(hDSPInterface->dspCodec->hProc, pDmmBuf->pMapped, pDmmBuf->pReserved);
                                    LCML_FREE(pDmmBuf);
                                    pDmmBuf = NULL;
                                    ((LCML_DSP_INTERFACE *)arg)->strmcntlmapped[i] = 0;
                                    ((LCML_DSP_INTERFACE *)arg)->pStrmcntlDmmBuf[i] = NULL;
                                    break;
                                }
                            }
                        }
                        pthread_mutex_unlock(&hDSPInterface->mutex);
                        
                        event = EMMCodecStrmCtrlAck;
                        bufType = streamId + EMMCodecStream0;  
                        args[0] = (void *) msg.dwArg1; /* SN error status */
                        args[1] = (void *) ackType;    /* acknowledge Id */
                        args[2] = (void *) bufType;    /* port Id */
                        args[6] = (void *) arg;        /* handle */
                        LCML_DPRINT("GOT MESSAGE USN_DSPACK_STRMCTRL \n");
                    }
                    else
                    {
                        event = EMMCodecDspMessageRecieved;
                        args[0] = (void *) msg.dwCmd;
                        args[1] = (void *) msg.dwArg1;
                        args[2] = (void *) msg.dwArg2;
                        args[6] = (void *) arg;  /* handle */
                        LCML_DPRINT("GOT MESSAGE EMMCodecDspMessageRecieved \n");
                    }

                    /* call callback */
                    LCML_DPRINT("calling callback in application %p \n",((LCML_DSP_INTERFACE *)arg)->dspCodec);
#ifdef __PERF_INSTRUMENTATION__
                    PERF_SendingCommand(hDSPInterface->pPERFcomp,
                                        msg.dwCmd,
                                        msg.dwArg1,
                                        PERF_ModuleLLMM);
#endif
                    hDSPInterface->dspCodec->Callbacks.LCML_Callback(event,args);

                }/* end of internal if(DSP_SUCCEEDED(status)) */
                else
                {
                    LCML_DPRINT("%d :: DSPManager_getmessage() failed: 0x%lx",__LINE__, status);
                }

            }/* end of internal while loop*/
#ifdef __ERROR_PROPAGATION__                
            }/*end of if(index == 0)*/            
            if (index == 1){
            
                struct DSP_PROCESSORSTATE  procState;
                DSPProcessor_GetState(((LCML_DSP_INTERFACE *)arg)->dspCodec->hProc, &procState, sizeof(procState));
                
                /*
                fprintf(stdout, " dwErrMask = %0x \n",procState.errInfo.dwErrMask);
                fprintf(stdout, " dwVal1 = %0x \n",procState.errInfo.dwVal1);
                fprintf(stdout, " dwVal2 = %0x \n",procState.errInfo.dwVal2);
                fprintf(stdout, " dwVal3 = %0x \n",procState.errInfo.dwVal3);
                fprintf(stdout, "MMU Fault Error.\n");
                */
            
                TUsnCodecEvent  event = EMMCodecDspError;
                void * args[10];
                LCML_DSP_INTERFACE *hDSPInterface = ((LCML_DSP_INTERFACE *)arg) ;
                args[0] = NULL;
                args[4] = NULL;
                args[5] = NULL;
                args[6] = (void *) arg;  /* handle */
                hDSPInterface->dspCodec->Callbacks.LCML_Callback(event,args);
      
            }
            if (index == 2){
            
                struct DSP_PROCESSORSTATE  procState;
                DSPProcessor_GetState(((LCML_DSP_INTERFACE *)arg)->dspCodec->hProc, &procState, sizeof(procState));
                
                /*
                fprintf(stdout, " dwErrMask = %0x \n",procState.errInfo.dwErrMask);
                fprintf(stdout, " dwVal1 = %0x \n",procState.errInfo.dwVal1);
                fprintf(stdout, " dwVal2 = %0x \n",procState.errInfo.dwVal2);
                fprintf(stdout, " dwVal3 = %0x \n",procState.errInfo.dwVal3);
                fprintf(stdout, "SYS_ERROR Error.\n");
                */
            
                TUsnCodecEvent  event = EMMCodecDspError;
                void * args[10];
                LCML_DSP_INTERFACE *hDSPInterface = ((LCML_DSP_INTERFACE *)arg) ;
                args[0] = NULL;
                args[4] = NULL;
                args[5] = NULL;
                args[6] = (void *) arg;  /* handle */
                hDSPInterface->dspCodec->Callbacks.LCML_Callback(event,args);
      
            }
#endif            
        } /* end of external if(DSP_SUCCEEDED(status)) */
        else
        {
            LCML_DPRINT("%d :: DSPManager_WaitForEvents() failed: 0x%lx",__LINE__, status);
        }

    } /* end of external while(1) loop */

    LCML_DPRINT("Exiting LOOP of LCML \n");
#ifdef __PERF_INSTRUMENTATION__
    PERF_Done(((LCML_DSP_INTERFACE *)arg)->pPERFcomp);
#endif
    return (void*)OMX_ErrorNone;
}


static int append_dsp_path(char * dll64p_name, char *absDLLname)
{
    int len = 0;
    char *dsp_path = NULL;
    if (!(dsp_path = getenv("DSP_PATH")))
    {
        printf("DSP_PATH Environment variable not set using /system/lib/dsp default");
        dsp_path = "/system/lib/dsp";
    }
    len = strlen(dsp_path) + strlen("/") + strlen(dll64p_name) + 1 /* null terminator */;
    if (len >= ABS_DLL_NAME_LENGTH) return -1;

    strcpy(absDLLname,dsp_path);
    strcat(absDLLname,"/");
    strcat(absDLLname,dll64p_name);
    return 0;
}

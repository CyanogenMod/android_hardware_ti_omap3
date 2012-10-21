
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
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */
/**
* @file ResourceManager.c
*
* This file implements resource manager for Linux 25.x that
* is fully compliant with the Khronos OpenMax specification 1.1.
*
* @path  $(CSLPATH)\
*
* @rev  1.0
*/
/* ----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 24-Apr-2005 rg:  Initial Version.
*!
* ============================================================================= */
#include <unistd.h>     // for sleep
#include <stdlib.h>     // for calloc
#include <sys/time.h>   // time is part of the select logic
#include <sys/types.h>  // for opening files
#include <sys/ioctl.h>  // for ioctl support
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/prctl.h>
#include <pthread.h>    // for threading support
#include <string.h>     // for memset
#include <stdio.h>      // for buffered io
#include <fcntl.h>      // for opening files.
#include <errno.h>      // for error handling support
#include <linux/soundcard.h>
#include <signal.h>

#include <Resource_Activity_Monitor.h>
#include <ResourceManagerProxy.h>

int nInstances = 0;
int threadCreated = 0;

int nVideoInstances = 0;
int nVPPInstances = 0;
int nImageInstances = 0;
int nCameraInstances = 0;
int nDisplay1Instances = 0;
int nDisplay2Instances = 0;
int responsePending = 0;
OMX_ERRORTYPE *RM_Error = NULL;
int closeThreadFlag=0;
RMProxy_ComponentList componentList;
sem_t                    *sem = NULL;

int boost_count = 0;
static pthread_mutex_t boost_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef __PERF_INSTRUMENTATION__
#include "perf.h"
PERF_OBJHANDLE pPERF = NULL;
PERF_OBJHANDLE pPERFproxy = NULL;
#endif

#ifdef __VIDEO_EXHAUSTION__
#define MAX_VIDEO_INSTANCES 2
#define MAX_VPP_INSTANCES 2
#define MAX_IMAGE_INSTANCES 2
#endif
#define MAX_CAMERA_INSTANCES 1
#define MAX_DISPLAY1_INSTANCES 1
#define MAX_DISPLAY2_INSTANCES 1
#define PIPE_SIZE 120

/*


*/
#ifdef __PERF_INSTRUMENTATION__
void create_PERF_object()
{
    if (pPERF == NULL)
    {
        pPERF = PERF_Create(PERF_FOURCC('R','M','P',' '),
                            PERF_ModuleLLMM |
                            PERF_ModuleAudioDecode | PERF_ModuleAudioEncode |
                            PERF_ModuleVideoDecode | PERF_ModuleVideoEncode |
                            PERF_ModuleImageDecode | PERF_ModuleImageEncode);
    }
}
#endif


OMX_ERRORTYPE RMProxy_NewInitalize()
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int ret = 0;
    RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init\n");
    RMPROXY_CORE *proxy_core;

    if (nInstances > 8)
    {
        RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - more than 8 instances\n");
        eError = OMX_ErrorInsufficientResources;
        nInstances--;
        return eError;
    }

    /* create the pipe used to send messages to the thread */
    if (flag == 1)
    {
        RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - thread is created already\n");
    }
    else
    {

#ifdef __PERF_INSTRUMENTATION__
        create_PERF_object();
#endif
        ret = pipe(RMProxy_Handle.tothread);
        if (ret)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure to create the pipe\n");
            return (OMX_ErrorInsufficientResources);
        }

        proxy_core = calloc (1, sizeof (RMPROXY_CORE));
        if (proxy_core == NULL)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure to calloc proxy_core\n");
            return OMX_ErrorInsufficientResources;
        }
        /* Create the proxy thread */
        ret = pthread_create(&RMProxy_Handle.threadId, NULL, (void*)RMProxy_Thread, proxy_core);
        if (ret||!RMProxy_Handle.threadId)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure to create RMProxy_Thread\n");
            return (OMX_ErrorInsufficientResources);
        }
        flag = 1;
#ifdef __PERF_INSTRUMENTATION__
        PERF_ThreadCreated(pPERF,
                           RMProxy_Handle.threadId,
                           PERF_FOURCC('R','M','P','X'));
#endif

    }


    nInstances++;

    EXIT:
    return (eError);
}




OMX_ERRORTYPE RMProxy_NewInitalizeEx(OMX_LINUX_COMPONENTTYPE componetType)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int ret = 0;
    RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init\n");
    RMPROXY_CORE *proxy_core;

    

    switch (componetType)
    {
    case OMX_COMPONENTTYPE_VIDEO:
        nVideoInstances++;
#ifdef __VIDEO_EXHAUSTION__
        if (nVideoInstances > MAX_VIDEO_INSTANCES)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - more than %d instances\n", MAX_VIDEO_INSTANCES);
            eError = OMX_ErrorInsufficientResources;
            nVideoInstances--;
            goto EXIT;
        }
#endif
        break;

        case OMX_COMPONENTTYPE_VPP:
        nVPPInstances++;
#ifdef __VIDEO_EXHAUSTION__
        if (nVPPInstances> MAX_VPP_INSTANCES)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - more than 2 instances\n");
            eError = OMX_ErrorInsufficientResources;
            nVPPInstances--;
            goto EXIT;
        }
#endif
        break;

    case OMX_COMPONENTTYPE_IMAGE:

        nImageInstances++;
#ifdef __VIDEO_EXHAUSTION__
        if (nImageInstances > MAX_IMAGE_INSTANCES)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - more than %d instances\n", MAX_IMAGE_INSTANCES);
            eError = OMX_ErrorInsufficientResources;
            nImageInstances--;
            goto EXIT;
        }
#endif
        break;


    case OMX_COMPONENTTYPE_CAMERA:
        nCameraInstances++;
        if (nCameraInstances > MAX_CAMERA_INSTANCES)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - more than %d instances\n", MAX_CAMERA_INSTANCES);
            eError = OMX_ErrorInsufficientResources;
            nCameraInstances--;
            goto EXIT;
        }
        break;


    case OMX_COMPONENTTYPE_DISPLAY1:
        nDisplay1Instances++;
        if (nDisplay1Instances > MAX_DISPLAY1_INSTANCES)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - more than %d instances\n", MAX_DISPLAY1_INSTANCES);
            eError = OMX_ErrorInsufficientResources;
            nDisplay1Instances--;
            goto EXIT;
        }
        break;

	case OMX_COMPONENTTYPE_DISPLAY2:
        nDisplay2Instances++;
        if (nDisplay2Instances > MAX_DISPLAY2_INSTANCES)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - more than %d instances\n", MAX_DISPLAY2_INSTANCES);
            eError = OMX_ErrorInsufficientResources;
            nDisplay2Instances--;
            goto EXIT;
        }
        break;

    case OMX_COMPONENTTYPE_AUDIO:
        break;

    default:
        RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - Invalid Component Type\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
        break;
    }

    /* create the pipe used to send messages to the thread */
    if (flag == 1)
    {
        RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Init - thread is created already\n");
    }
    else
    {
#ifdef __PERF_INSTRUMENTATION__
        create_PERF_object();
#endif

        ret = pipe(RMProxy_Handle.tothread);
        if (ret)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure to create the pipe\n");
            return (OMX_ErrorInsufficientResources);
        }

        proxy_core = calloc (1, sizeof (RMPROXY_CORE));
        if (proxy_core == NULL)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure to calloc proxy_core\n");
            return OMX_ErrorInsufficientResources;
        }
        /* Create the proxy thread */
        ret = pthread_create(&RMProxy_Handle.threadId, NULL, (void*)RMProxy_Thread, proxy_core);
        if (ret||!RMProxy_Handle.threadId)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure to create RMProxy_Thread\n");
            return (OMX_ErrorInsufficientResources);
        }

#ifdef __PERF_INSTRUMENTATION__
        PERF_ThreadCreated(pPERF,
                           RMProxy_Handle.threadId,
                           PERF_FOURCC('R','M','P','X'));
#endif
        flag = 1;
    }

    EXIT:
    
    return (eError);
}




OMX_ERRORTYPE RMProxy_Deinitalize()
{
    RMPROXY_COMMANDDATATYPE RMProxy_CommandData ;

    RMProxy_CommandData.hComponent  = NULL ;
    RMProxy_CommandData.RM_Cmd      = RMProxy_Exit ;
    RMProxy_CommandData.param1      = 0x00 ;
    RMProxy_CommandData.param2      = 0x00 ;
    OMX_ERRORTYPE threadError = OMX_ErrorNone;
    int ret = 0;

    RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Deinit\n");

    nInstances--;

    /* the last component within one the same progressor will close pipe and destroy thread */
    if (nVideoInstances == 0 &&
            nVPPInstances == 0 &&
            nImageInstances == 0 &&
            nCameraInstances == 0 &&
            nDisplay1Instances == 0 && nDisplay2Instances == 0 &&
            nInstances == 0)
    {

        /* send the command RM_Exit using pipe */
        /*RMPROXY_DPRINT("[Resource_Manager_Proxy] - will destroy RMProxy thread\n");*/
#ifdef __PERF_INSTRUMENTATION__
        PERF_SendingCommand(pPERF, -1, 0, PERF_ModuleComponent);
#endif

        write(RMProxy_Handle.tothread[1],&RMProxy_CommandData,sizeof(RMProxy_CommandData));
        /* kill the thread */
        ret = pthread_join(RMProxy_Handle.threadId, (void*)&threadError);
        if (ret)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure to kill thread\n");
        }

#ifdef __PERF_INSTRUMENTATION__
        PERF_Done(pPERF);
#endif

        flag = 0;
    }
    else
    {
        /*RMPROXY_DPRINT("[Resource_Manager_Proxy] - do nothing with nInstances=%d\n", nInstances);*/
    }
    return (OMX_ErrorNone);
}


/*


*/
OMX_ERRORTYPE RMProxy_DeinitalizeEx(OMX_LINUX_COMPONENTTYPE componentType)
{
    RMPROXY_COMMANDDATATYPE RMProxy_CommandData ;
    RMProxy_CommandData.hComponent  = NULL ;
    RMProxy_CommandData.RM_Cmd      = RMProxy_Exit ;
    RMProxy_CommandData.param1      = 0x00 ;
    RMProxy_CommandData.param2      = 0x00 ;

    OMX_ERRORTYPE threadError = OMX_ErrorNone;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int ret = 0;

    RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Deinit\n");

    switch (componentType)
    {
    case OMX_COMPONENTTYPE_AUDIO:
        break;

    case OMX_COMPONENTTYPE_VIDEO:
        nVideoInstances--;
        break;

    case OMX_COMPONENTTYPE_VPP:
        nVPPInstances--;
        break;

    case OMX_COMPONENTTYPE_IMAGE:
        nImageInstances--;
        break;

    case OMX_COMPONENTTYPE_CAMERA:
        nCameraInstances--;
        break;

    case OMX_COMPONENTTYPE_DISPLAY1:
        nDisplay1Instances--;
        break;
	case OMX_COMPONENTTYPE_DISPLAY2:
        nDisplay2Instances--;
        break;

    default:
        RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_DeInit - Invalid Component Type\n");
        eError = OMX_ErrorBadParameter;
        goto EXIT;
        break;
    }

    /* the last component within one the same progressor will close pipe and destroy thread */
    if (nVideoInstances == 0 &&
            nVPPInstances == 0 &&
            nImageInstances == 0 &&
            nCameraInstances == 0 &&
            nDisplay1Instances == 0 && nDisplay2Instances == 0 &&
            nInstances == 0)
    {
        /* send the command RM_Exit using pipe */
#ifdef __PERF_INSTRUMENTATION__
        PERF_SendingCommand(pPERF, -1, 0, PERF_ModuleComponent);
#endif
        write(RMProxy_Handle.tothread[1],&RMProxy_CommandData,sizeof(RMProxy_CommandData));
        /* kill the thread */
        ret = pthread_join(RMProxy_Handle.threadId, (void*)&threadError);
        if (ret)
        {
            RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure to kill thread\n");
        }

#ifdef __PERF_INSTRUMENTATION__
        PERF_Done(pPERF);
#endif
        flag = 0;
    }
    else
    {
        /*RMPROXY_DPRINT("[Resource_Manager_Proxy] - do nothing with nInstances=%d\n", nInstances);*/
    }
    EXIT:
    return (OMX_ErrorNone);
}



OMX_ERRORTYPE RMProxy_NewSendCommand(OMX_HANDLETYPE hComponent, RMPROXY_COMMANDTYPE cmd, OMX_U32 param1, OMX_U32 param2, OMX_U32 param3, OMX_PTR param4)
{
    RMPROXY_COMMANDDATATYPE  RMProxy_CommandData ;
    OMX_ERRORTYPE            ReturnValue = OMX_ErrorNone ;

    /* this ensures that one request completes before another starts */
    while (responsePending) {
        sleep(1);
    }

    if (cmd == RMProxy_RequestResource)
    { 
        responsePending = 1;
        sem = malloc(sizeof(sem_t)) ; 
        sem_init(sem, 0x00, 0x00);   
        RM_Error = malloc(sizeof(OMX_ERRORTYPE)) ; 
        if (NULL != RM_Error)
            RMProxy_CommandData.RM_Error  = RM_Error ;
        RMProxy_CommandData.sem       = sem;
    }


    RMProxy_CommandData.hComponent   = hComponent ;
    RMProxy_CommandData.RM_Cmd       = cmd ;
    RMProxy_CommandData.nPid       = getpid() ;
    RMProxy_CommandData.param1       = param1 ;
    RMProxy_CommandData.param2       = param2 ;
    RMProxy_CommandData.param3       = param3 ;
    RMProxy_CommandData.param4       = (OMX_U32)param4 ;


#ifdef __PERF_INSTRUMENTATION__
        PERF_SendingCommand(pPERF, cmd, param1, PERF_ModuleComponent);
#endif

        write(RMProxy_Handle.tothread[1],&RMProxy_CommandData,sizeof(RMPROXY_COMMANDDATATYPE)) ;


    if (cmd == RMProxy_RequestResource) {
#ifndef __ENABLE_RMPM_STUB__
        if(!RMProxy_CheckForStubMode())
        {
            // wait for response back from resource manager server 
            if (sem) { 
                sem_wait(sem);
            }
            if (NULL != RM_Error)
                ReturnValue = *RM_Error;

            free(sem);
            sem=NULL;
            free(RM_Error);
            RM_Error=NULL;
        }
#else
        if(RMProxy_CheckForStubMode())
        {
            /* if using stubbed implementation always return grant */
            ReturnValue = OMX_ErrorNone;
        }
#endif
        responsePending = 0;
    }
    
    /* send the command and parameter using pipe */
    return(ReturnValue);

}
/* scan for QoS dll, if DNE then fall back to stub mode */
int RMProxy_CheckForStubMode()
{
    char *qosdllname;
    int stubMode = 0;
    FILE* fp = NULL;

    /* first check omap version is supported */
    if (get_omap_version() == OMAP_NOT_SUPPORTED){
        stubMode = 1;
        RMPROXY_DPRINT("OMAP version not supported by RM: falling back to stub mode\n");
        return stubMode;
    }
   
    /* check QoS dependency is also met */
    qosdllname = getenv ("QOSDYN_FILE");
    if (qosdllname == NULL) /*is the var defined? */
    {
        stubMode = 1;
        RMPROXY_DPRINT("Failed to get QOS DLL file name by RM: falling back to stub mode\n");
        return stubMode;
    }
    if( (fp = fopen(qosdllname, "r")) == NULL) {
        stubMode = 1;
        RMPROXY_DPRINT("Failed to open QOS DLL by RM: falling back to stub mode\n");
    }
    else {
        stubMode = 0; //file exists, so no need to use stub implementation
        fclose(fp);
    }

    return stubMode;
}
/*


*/
void *RMProxy_Thread(RMPROXY_CORE *core)
{
    RESOURCEMANAGER_COMMANDDATATYPE rm_data;
    
    RMPROXY_COMMANDDATATYPE *cmd_data= &(core->cmd_data);
    fd_set rfds2 ;
    fd_set rfds ;

    int fdmax;
    int status;
    char namedPipeName[PIPE_SIZE];
    char handleString[100];
    int numClients=0;
    int i;
    int index=-1;
    struct timespec tv;
    int readPipeOpened=0;
    int fatalErrorHandled = 0;
    OMX_PTR threadhandle=NULL;

    componentList.numRegisteredComponents = 0;

#ifdef __PERF_INSTRUMENTATION__
    pPERFproxy = PERF_Create(PERF_FOURCC('R','M','P','T'),
                             PERF_ModuleComponent |
                             PERF_ModuleAudioDecode | PERF_ModuleAudioEncode |
                             PERF_ModuleVideoDecode | PERF_ModuleVideoEncode |
                             PERF_ModuleImageDecode | PERF_ModuleImageEncode);
#endif

    prctl(PR_SET_NAME, (unsigned long) "RM-Proxy", 0, 0, 0);

    RMPROXY_DPRINT("[Resource_Manager_Proxy] - RMProxy_Thread\n");



    fdmax = RMProxy_Handle.tothread[0];
    tv.tv_sec = 0;
    tv.tv_nsec = 100000000;
    while (1)
    {
        //RMPROXY_DPRINT("RMProxy - Beginning of thread while loop\n");
        FD_ZERO(&rfds2);
        FD_SET(RMProxy_Handle.tothread[0], &rfds2);
        sigset_t set;
        sigemptyset (&set);
        sigaddset (&set, SIGALRM);
        status = pselect(fdmax+1, &rfds2, NULL, NULL, &tv, &set);
        if (FD_ISSET(RMProxy_Handle.tothread[0], &rfds2)) {
             // handle command from OMX component
            read(RMProxy_Handle.tothread[0], cmd_data, sizeof(RMPROXY_COMMANDDATATYPE));

#ifdef __PERF_INSTRUMENTATION__
            PERF_ReceivedCommand(pPERFproxy, cmd_data->RM_Cmd, cmd_data->param1,
                                 PERF_ModuleLLMM);
#endif

            switch (cmd_data->RM_Cmd)
            {
            // Forward command to resource manager server without waiting for response back
            case RMProxy_StateSet:
                    RMPROXY_DPRINT("RMProxy_Thread %d\n",__LINE__);
                RM_SetState(cmd_data->hComponent, cmd_data->param1, cmd_data->param2);
                break;

            case RMProxy_FreeResource:
                RM_FreeResource(cmd_data->hComponent, cmd_data->param1, cmd_data->param2,numClients);
                /* Find the index of the component to free */
                for (i=0; i < componentList.numRegisteredComponents; i++) {
                    if (componentList.component[i].componentHandle == cmd_data->hComponent) {
                        index = i;
                        break;
                    }
                }
                if (index != -1) {
                    /* Shift all other components in the list up */
                    for(i=index; i < componentList.numRegisteredComponents-1; i++) {
                        componentList.component[i].componentHandle = componentList.component[i+1].componentHandle;
                        componentList.component[i].callback = componentList.component[i+1].callback;
                    }
                    /* set the end of the list to keep the list clean */
                    componentList.component[i].componentHandle = NULL;
                    componentList.component[i].callback = NULL;
                    /* Decrement the count of registered components */
                    componentList.numRegisteredComponents--;
                    /* reset index */
                    index = -1;
                }
                if (numClients > 0)
                {
                    numClients--;
                }
                break;

            case RMProxy_CancelWaitForResource:
                RMPROXY_DPRINT("RMProxy_Thread %d\n",__LINE__);
                RM_CancelWaitForResource(cmd_data->hComponent, cmd_data->param1, cmd_data->param2);
                break;

            // Forward command to resource manager server and waiting for response back
            case RMProxy_RequestResource:
                RMPROXY_DPRINT("RMProxy_Thread %d\n",__LINE__);
                /* add component and corresponding callback to the list */
                componentList.component[componentList.numRegisteredComponents].componentHandle = cmd_data->hComponent;
                componentList.component[componentList.numRegisteredComponents++].callback = (RMPROXY_CALLBACKTYPE*)cmd_data->param4;
                if (numClients == 0) {
                    if ((RMProxyfdwrite=open(RM_SERVER_IN, O_WRONLY))<0){
                        RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure to open WRITE pipe [RMProxyfdwrite=%d], [errno=%d]\n", RMProxyfdwrite, errno);
                    } else {
                        RMPROXY_DPRINT("[RM_PROXY] - WRITE pipe opened successfully\n");
                    }
                    
                    rm_data.hComponent = cmd_data->hComponent;
                    rm_data.nPid = getpid();
                    rm_data.RM_Cmd = RMProxy_OpenPipe;
                    rm_data.rm_status = RM_ErrorNone;
                    rm_data.param1 = cmd_data->param1;
                    threadhandle = cmd_data->hComponent;
                    rm_data.param2 = cmd_data->param2;
                    rm_data.param3 = cmd_data->param4;
                    // send request to resource manager

#ifdef __PERF_INSTRUMENTATION__
                    PERF_SendingCommand(pPERFproxy, rm_data.RM_Cmd, rm_data.param1, PERF_ModuleHLMM);
#endif

                    /*  RMProxy_itoa((int)cmd_data.hComponent,handleString);*/
                    RMProxy_itoa((int)rm_data.nPid,handleString);

                    strcpy(namedPipeName,RM_SERVER_OUT);
                    strcat(namedPipeName,"_");
                    strcat(namedPipeName,handleString);

                    //send the request to open the pipe

                    if ((write(RMProxyfdwrite, &rm_data, sizeof(rm_data))) < 0) 
                        RMPROXY_DPRINT("[RM_Proxy] - failure write data to resource manager\n");

                    /* wait for the pipe to be created while trying to open it
                       assumes the pipe open will eventually succeed */
                    RMPROXY_DPRINT("[RM_Proxy] - try to open the IN Pipe, RM_SERVER_OUT for PID %d\n", (int)getpid());
                    RMPROXY_DPRINT("namedPipeName = %s\n", (char*)namedPipeName);
                    while((RMProxyfdread=open(namedPipeName, O_RDONLY )) < 0)
                    {
                        RMPROXY_DPRINT("[RM_Proxy] - waiting on pipe creation\
                                        [RMProxyfdread=%d],\
                                        [errno=%d]\n", RMProxyfdread, errno);
                        sleep(.05);
                    }
                    readPipeOpened = 1;
                    RMPROXY_DPRINT("[RM_Proxy] - read pipe opened successfully\n");

                }
                else {
                    rm_data.RM_Cmd = RMProxy_ReusePipe;
                    rm_data.hComponent = cmd_data->hComponent;
                    rm_data.param1 = cmd_data->param1;
                    rm_data.param2 = cmd_data->param2;
                    rm_data.param4 = (OMX_U32)threadhandle;
                    if ((write(RMProxyfdwrite, &rm_data, sizeof(rm_data))) < 0)
                        RMPROXY_DPRINT("[RM_Proxy] - failure write data to resource manager\n");
                    
                    
                }
                RM_RequestResource(cmd_data->hComponent,
                                   cmd_data->param1,
                                   cmd_data->param2,
                                   cmd_data->param3,
                                   cmd_data->nPid,
                                   cmd_data->sem,
                                   cmd_data->RM_Error);
                numClients++;

                break;

            case RMProxy_WaitForResource:
                    RMPROXY_DPRINT("RMProxy_Thread %d\n",__LINE__);
                RM_WaitForResource(cmd_data->hComponent,
                                   cmd_data->param1,
                                   cmd_data->sem,
                                   cmd_data->RM_Error);
                break;

                // clean
            case RMProxy_Exit:
                closeThreadFlag = 1;
                close(RMProxy_Handle.tothread[0]);
                close(RMProxy_Handle.tothread[1]);

                close(RMProxyfdread);
                readPipeOpened = 0;
                close(RMProxyfdwrite);

                if (core != NULL)
                {
                    free(core);
                    core = NULL;
                }
                if(unlink(namedPipeName)<0) {
                    /*RMPROXY_DPRINT("[Resource Manager] - unlink RM_SERVER_OUT = %s error\n", namedPipeName);*/
                }

                
                return (NULL);

            default:
                break;
            }

        }

        if (readPipeOpened) {
        FD_ZERO(&rfds);
        FD_SET(RMProxyfdread, &rfds);
            
         status = pselect(RMProxyfdread+1, &rfds, NULL, NULL, &tv, &set);

        if (FD_ISSET(RMProxyfdread, &rfds)) {
            //RMPROXY_DPRINT("RMProxyfdread set\n");
            read(RMProxyfdread,&rm_data,sizeof(rm_data));
            if (rm_data.rm_status == RM_DENY) {
            
                if (RM_Error) {
                    *RM_Error = OMX_ErrorInsufficientResources;  
                }
                if (sem) {
                    sem_post(sem) ;
                }
                
            }
            else if (rm_data.rm_status == RM_PREEMPT) {
                if (!RM_Error)
                    RM_Error = malloc(sizeof(OMX_ERRORTYPE));
                if (NULL != RM_Error)
                    *RM_Error = OMX_RmProxyCallback_ResourcesPreempted;
                cmd_data->hComponent = rm_data.hComponent;
                if (NULL != core)
                {
                    RMProxy_CallbackClient(rm_data.hComponent,RM_Error, core);
                    free (RM_Error);
                    RM_Error = NULL;
                }
            }
            else if (rm_data.rm_status == RM_RESOURCEFATALERROR && !fatalErrorHandled) {
                /* if RM denies us due to dsp recovery during request resource, we will deadlock the system if Callback is used
                   because the main thread in the OMX component is already blocked on RMProxy_RequestResource */
                if(!responsePending){
                    if (!RM_Error)
                        RM_Error = malloc(sizeof(OMX_ERRORTYPE));
                    if (NULL != RM_Error)
                        *RM_Error = OMX_RmProxyCallback_FatalError;
                    cmd_data->hComponent = rm_data.hComponent;
                    if (NULL != core)
                    {
                        RMProxy_CallbackClient(rm_data.hComponent,RM_Error, core);
                        free (RM_Error);
                        RM_Error = NULL;
                    }
                }else {
                    RMPROXY_DPRINT("RM_RESOURCEFATALERROR while requesting resources");
                    if (RM_Error) {
                        *RM_Error = OMX_RmProxyCallback_FatalError;
                    }
                    if (sem) {
                        sem_post(sem) ;
                    }
                }
                /* unknown behavior can occur if we tell the system to handle a fatal error multiple times.
                fatal error should only need to handled once per component */
                fatalErrorHandled = 1;
            }
            else if (rm_data.rm_status == RM_GRANT) {
#ifndef __ENABLE_RMPM_STUB__
                /* if not in stub mode... */
                if(!RMProxy_CheckForStubMode())
                {
                    /* if we are stubbing out the actual implmentation there will be no response */
                    if (RM_Error) {
                        *RM_Error = OMX_ErrorNone;  
                    }
                
                    if (sem) { 
                        sem_post(sem) ;
                    }
                }
#endif                
            
                // signal the component that resource is available
                
            }
            else if (rm_data.rm_status == RM_RESOURCEACQUIRED) {
                RMPROXY_DPRINT("Got RM_RESOURCEACQUIRED\n");
                if (!RM_Error)
                    RM_Error = malloc(sizeof(OMX_ERRORTYPE));
                if ( NULL != RM_Error)
                    *RM_Error = OMX_RmProxyCallback_ResourcesAcquired;
                if (NULL != core)
                {
                    RMProxy_CallbackClient(rm_data.hComponent,RM_Error, core);
                    free (RM_Error);
                    RM_Error = NULL;
                }
                    
            }
        }

    
    //RMPROXY_DPRINT("RMProxy - End of thread while loop\n");

    }
}
#ifdef __PERF_INSTRUMENTATION__
    PERF_Done(pPERFproxy);
#endif
}

void RM_RequestResource(OMX_HANDLETYPE hComponent, OMX_U32 param, OMX_U32 param2, OMX_U32 param3,OMX_U32 nPid, sem_t *sem, OMX_ERRORTYPE *RM_Error)
{
    RESOURCEMANAGER_COMMANDDATATYPE rm_data;
    rm_data.hComponent = hComponent;
    rm_data.RM_Cmd = RMProxy_RequestResource;
    rm_data.param1 = param;
    rm_data.param2 = param2;
    rm_data.param3 = param3;
    rm_data.nPid = nPid;
    rm_data.rm_status = RM_ErrorNone;

    // send request to resource manager

#ifdef __PERF_INSTRUMENTATION__
    PERF_SendingCommand(pPERFproxy, rm_data.RM_Cmd, rm_data.param1, PERF_ModuleHLMM);
#endif
    if ((write(RMProxyfdwrite, &rm_data, sizeof(rm_data))) < 0)
        RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure write data to resource manager\n");

#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedCommand(pPERFproxy, PERF_CommandStatus, rm_data.rm_status, PERF_ModuleHLMM);
#endif
}

/*


*/
void RM_WaitForResource(OMX_HANDLETYPE hComponent, OMX_U32 param, sem_t *sem, OMX_ERRORTYPE *RM_Error)
{
    RMPROXY_DPRINT("[Resource_Manager_Proxy] - RM_WaitForResource\n");


    RESOURCEMANAGER_COMMANDDATATYPE rm_data;
    rm_data.hComponent = hComponent;
    rm_data.RM_Cmd = RMProxy_WaitForResource;
    rm_data.param1 = param;
    rm_data.nPid = getpid();
    rm_data.rm_status = RM_ErrorNone;

    // send request to resource manager

#ifdef __PERF_INSTRUMENTATION__
    PERF_SendingCommand(pPERFproxy, rm_data.RM_Cmd, rm_data.param1, PERF_ModuleHLMM);
#endif
    if ((write(RMProxyfdwrite, &rm_data, sizeof(rm_data))) < 0)
        RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure write data to resource manager\n");
#ifdef __PERF_INSTRUMENTATION__
    PERF_ReceivedCommand(pPERFproxy, PERF_CommandStatus, rm_data.rm_status, PERF_ModuleHLMM);
#endif
}

/*


*/
void RM_SetState(OMX_HANDLETYPE hComponent, OMX_U32 param1, OMX_U32 param2)
{
    RESOURCEMANAGER_COMMANDDATATYPE rm_data;
    rm_data.hComponent = hComponent;
    rm_data.RM_Cmd = RMProxy_StateSet;
    rm_data.param1 = param1;
    rm_data.param2 = param2;
    rm_data.nPid = getpid();
    rm_data.rm_status = RM_ErrorNone;
    if ((write(RMProxyfdwrite, &rm_data, sizeof(rm_data))) < 0)
        RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure write data to resource manager\n");
}

/*


*/
void RM_FreeResource(OMX_HANDLETYPE hComponent, OMX_U32 param1, OMX_U32 param2, OMX_U32 numClients)
{
    RESOURCEMANAGER_COMMANDDATATYPE rm_data;
    rm_data.hComponent = hComponent;
    if (numClients > 1) {
        rm_data.RM_Cmd = RMProxy_FreeResource;
    }
    else {
        rm_data.RM_Cmd = RMProxy_FreeAndCloseResource;
    }
    rm_data.nPid = getpid();
    rm_data.param1 = param1;
    rm_data.param2 = param2;
    rm_data.rm_status = RM_ErrorNone;

    if ((write(RMProxyfdwrite, &rm_data, sizeof(rm_data))) < 0)
        RMPROXY_DPRINT("[Resource_Manager_Proxy] - failure write data to resource manager\n");

}

/*


*/
void RM_CancelWaitForResource(OMX_HANDLETYPE hComponent, OMX_U32 param1, OMX_U32 param2)
{
    RMPROXY_DPRINT("[Resource_Manager_Proxy] - RM_CancelWaitForResource\n");
}



void RMProxy_itoa(int n, char s[])
{
    int i, sign;

    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do {       /* generate digits in reverse order */
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    RMProxy_reverse(s);
}



void RMProxy_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

int RMProxy_RequestBoost(int level)
{
    if(pthread_mutex_lock(&boost_mutex) != 0)
    {
        RMPROXY_DPRINT("%d:: Error in Mutex lock\n",__LINE__);
        return OMX_ErrorUndefined;
    }
    boost_count++;
    RMPROXY_DPRINT("init count = %d\n", boost_count);
    if (boost_count == 1)
    {
        rm_request_boost(level);
    }
    if(pthread_mutex_unlock(&boost_mutex) != 0)
    {
        RMPROXY_DPRINT("%d :: Core: Error in Mutex unlock\n",__LINE__);
        return OMX_ErrorUndefined;
    }
    return OMX_ErrorNone;
}


int RMProxy_ReleaseBoost()
{
    if(pthread_mutex_lock(&boost_mutex) != 0)
    {
        RMPROXY_DPRINT("%d:: Error in Mutex lock\n",__LINE__);
        return OMX_ErrorUndefined;
    }
    boost_count--;
    RMPROXY_DPRINT("init count = %d\n", boost_count);
    if (boost_count == 0)
    {
        rm_release_boost();
    }
    if(pthread_mutex_unlock(&boost_mutex) != 0)
    {
        RMPROXY_DPRINT("%d :: Core: Error in Mutex unlock\n",__LINE__);
        return OMX_ErrorUndefined;
    }
    return OMX_ErrorNone;
}

void RMProxy_CallbackClient(OMX_HANDLETYPE hComponent, OMX_ERRORTYPE *error , RMPROXY_CORE *core)
{
    int i;
    int index = -1;

    RMPROXY_COMMANDDATATYPE *cmd_data= &(core->cmd_data);


    for (i=0; i < componentList.numRegisteredComponents; i++) {
        if (componentList.component[i].componentHandle == hComponent) {
            index = i;
            break;
        }
    }
    if (index != -1) {
        RMPROXY_DPRINT("Calling back client\n");
        cmd_data->RM_Error =error;
        componentList.component[index].callback->RMPROXY_Callback(*cmd_data);
    }
}

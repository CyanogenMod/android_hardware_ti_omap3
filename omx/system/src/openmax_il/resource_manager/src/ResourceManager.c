
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
* This file implements resource manager for Linux 23.x that 
* is fully compliant with the Khronos OpenMax specification 1.0.
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
#include <string.h>     // for memset
#include <stdio.h>      // for buffered io
#include <fcntl.h>      // for opening files.
#include <errno.h>      // for error handling support
#include <linux/soundcard.h>
#include <qosregistry.h>
#include <qosti.h>
#include <qos_ti_uuid.h>
#include <dbapi.h>
#include <DSPManager.h>
#include <DSPProcessor.h>
#include <DSPProcessor_OEM.h>


#ifdef __PERF_INSTRUMENTATION__
#include "perf.h"
PERF_OBJHANDLE pPERF = NULL;
#endif
#include <ResourceManager.h>
#include <ResourceManagerProxyAPI.h>
#include <PolicyManagerAPI.h>
#include <Resource_Activity_Monitor.h>
#include <pthread.h>
#include <signal.h>

#undef LOG_TAG
#define LOG_TAG "OMXRM"

RM_ComponentList componentList;
RM_ComponentList pendingComponentList;
int fdread, fdwrite;
int pmfdread, pmfdwrite;
int eErrno;

/* dynamic stub mode will be enabled 
   if there is a run-time failure */
bool stub_mode = 0;

/* used to block new requests while mmu-fault
   recovery in progress */
bool mmuRecoveryInProgress = 0;

unsigned int totalCpu=0;
unsigned int imageTotalCpu=0;
unsigned int videoTotalCpu=0;
unsigned int audioTotalCpu=0;
unsigned int lcdTotalCpu=0;
unsigned int cameraTotalCpu=0;

RESOURCEMANAGER_COMMANDDATATYPE cmd_data;
RESOURCEMANAGER_COMMANDDATATYPE globalrequest_cmd_data;
POLICYMANAGER_COMMANDDATATYPE policy_data;
POLICYMANAGER_RESPONSEDATATYPE policyresponse_data;


/*------------------------------------------------------------------------------------*
  * main() 
  *
  *                     This is the thread of resource manager
  *
  * @param 
  *                     None
  *
  * @retval 
  *                     None
  */
/*------------------------------------------------------------------------------------*/
int main()
{
#ifdef __ENABLE_RMPM_STUB__
    RM_DPRINT("Warning - using stub version of Resource Manager!!\n");
#endif
    RM_DPRINT("[Resource Manager] - start resource manager main function\n");
        
#ifdef __PERF_INSTRUMENTATION__
    pPERF = PERF_Create(PERF_FOURCC('R','M',' ',' '),
                        PERF_ModuleAudioDecode | PERF_ModuleAudioEncode |
                        PERF_ModuleVideoDecode | PERF_ModuleVideoEncode |
                        PERF_ModuleImageDecode | PERF_ModuleImageEncode |
                        PERF_ModuleSystem);
#endif
    
    int size = 0;
    int ret;
    char rmsideNamedPipeName[120];
    char rmsideHandleString[100];
    OMX_S16 fdmax;
    int status = 0;
    fd_set watchset;
    OMX_BOOL Exitflag = OMX_FALSE;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int reuse_pipe = -1;
    cpuStruct.snapshotsCaptured = 0;
    cpuStruct.averageCpuLoad = 0;
    pthread_t dsp_monitor = NULL;

    RM_DPRINT("[Resource Manager] - going to create the read & write pipe\n");
    unlink(RM_SERVER_IN);
        
    if((mknod(RM_SERVER_IN,S_IFIFO|PERMS,0)<0)&&(eErrno!=EEXIST)) 
        RM_DPRINT("[Resource Manager] - mknod failure to create the read pipe, error=%d\n", eErrno);


    sleep(1); /* since the policy manager needs to create pipes that this process will 
                        read we will wait to give time for the policy manager time to establish pipes */
    componentList.numRegisteredComponents = 0;

    /* start the MMU fault monitor */
    pthread_create(&dsp_monitor, NULL, RM_FatalErrorWatchThread, NULL);

    /* check that running OMAP is supported, if not fallback to stub mode */
    if (get_omap_version() == OMAP_NOT_SUPPORTED){
        stub_mode = 1;
        RM_EPRINT("OMAP version not supported by RM: falling back to stub mode\n");
    }

#ifndef __ENABLE_RMPM_STUB__
    /* after here, we know for sure if we are in stub_mode or not. */
    if (!stub_mode) 
    {
        // create pipe for read
        if((pmfdwrite=open(PM_SERVER_IN,O_WRONLY))<0)
            RM_DPRINT("[Policy Manager] - failure to open the WRITE pipe\n");

        if((pmfdread=open(PM_SERVER_OUT,O_RDONLY))<0)
            RM_DPRINT("[Policy Manager] - failure to open the READ pipe\n");
    }

#endif    
    // create pipe for read
    if((fdread=open(RM_SERVER_IN,O_RDONLY))<0)
        RM_DPRINT("[Resource Manager] - failure to open the READ pipe\n");

    FD_ZERO(&watchset);
    size=sizeof(cmd_data);

    RM_DPRINT("[Resource Manager] - going enter while loop\n");

    while(!Exitflag) {
        FD_SET(fdread, &watchset);
        fdmax = fdread;
#ifndef __ENABLE_RMPM_STUB__
        if (!stub_mode){
            FD_SET(pmfdread, &watchset);
            if (pmfdread > fdmax) {
                fdmax = pmfdread;
            }
        }
#endif
        sigset_t set;
        sigemptyset (&set);
        sigaddset (&set, SIGALRM);
        status = pselect(fdmax+1, &watchset, NULL, NULL, NULL, &set);
                 
        if(FD_ISSET(fdread, &watchset)) {
            ret = read(fdread, &cmd_data, size);
            if((size>0)&&(ret>0)&&(-1 != ret)) {
            
#ifdef __PERF_INSTRUMENTATION__
                PERF_ReceivedCommand(pPERF, cmd_data.RM_Cmd, cmd_data.param1,
                                     PERF_ModuleLLMM);
#endif
                // actually get data 
                RM_DPRINT("[Resource Manager] - get data\n");

                switch (cmd_data.RM_Cmd) {  
                    case RM_RequestResource:
                        HandleRequestResource(cmd_data);
                        break;

                    case RM_WaitForResource:
                        HandleWaitForResource(cmd_data);
                        break;

                    case RM_FreeResource:
                        HandleFreeResource(cmd_data);
                        RM_RemoveComponentFromList(cmd_data.hComponent,cmd_data.nPid);
                        break;

                    case RM_FreeAndCloseResource:
                        HandleFreeResource(cmd_data);
                        RM_ClosePipe(cmd_data);
                        RM_RemoveComponentFromList(cmd_data.hComponent,cmd_data.nPid);
                        break;

                    case RM_CancelWaitForResource:
                        HandleCancelWaitForResource(cmd_data);
                        break;

                    case RM_StateSet:
                        HandleStateSet(cmd_data);
                        break;

                    case RM_OpenPipe:
                        // create pipe for write
                        RM_CreatePipe(cmd_data,rmsideNamedPipeName,rmsideHandleString);

                        break;
                        
                    case RM_ReusePipe:
                        reuse_pipe = RM_GetPipe((OMX_HANDLETYPE)cmd_data.param4,cmd_data.nPid);
                        if(reuse_pipe != -1){
                            RM_AddPipe(cmd_data,reuse_pipe);
                        }
                        else{
                            RM_DPRINT("[Resource Manager] - Couldn't reuse pipe, opening a new one \n");
                            RM_CreatePipe(cmd_data,rmsideNamedPipeName,rmsideHandleString);
                        }
                        break;


                        case RM_Exit:
                        case RM_Init:
                        break;

                        case RM_ExitTI:
                            Exitflag = OMX_TRUE;
                            break;
                }  
            }
                else {
                close(fdread);
                if((fdread=open(RM_SERVER_IN,O_RDONLY))<0)                                
                    RM_DPRINT("[Resource Manager] - failure to re-open the Read pipe\n");
                RM_DPRINT("[Resource Manager] - re-opened Read pipe\n");
            }
        }
#ifndef __ENABLE_RMPM_STUB__
        else if (FD_ISSET(pmfdread,&watchset)) {
            if (!stub_mode){
            RM_DPRINT("if stub mode fallback, should not see this...\n");
            read(pmfdread,&policyresponse_data,sizeof(policyresponse_data));

            switch(policyresponse_data.PM_Cmd) {
                case PM_PREEMPTED:
                cmd_data.rm_status = RM_PREEMPT;
                cmd_data.hComponent = policyresponse_data.hComponent;
                RM_SetStatus(cmd_data.hComponent, cmd_data.nPid,RM_WaitingForClient);                
                int preemptpipe = RM_GetPipe(policyresponse_data.hComponent,policyresponse_data.nPid);
                if (write(preemptpipe,&cmd_data,sizeof(cmd_data)) < 0)
                    RM_DPRINT("Didn't write pipe\n");
                else
                    RM_DPRINT("Wrote RMProxy pipe\n");
                break;

                case PM_DENYPOLICY:
                    globalrequest_cmd_data.rm_status = RM_DENY;
                    RM_SetStatus(globalrequest_cmd_data.hComponent,globalrequest_cmd_data.nPid,RM_WaitingForClient);        
                    if(write(RM_GetPipe(globalrequest_cmd_data.hComponent,globalrequest_cmd_data.nPid), &globalrequest_cmd_data, sizeof(globalrequest_cmd_data)) < 0)
                        RM_DPRINT ("[Resource Manager] - failure write data back to component\n");
                    else
                        RM_DPRINT ("[Resource Manager] - Denied by policy, ok to write data back to component\n");
                    
                break;

                case PM_GRANTPOLICY:
                    /* if policy request is granted then check to see if we are currently handling an MMU fault,
                       then check to see if resource is available */

                    if (!mmuRecoveryInProgress) {
                        if (RM_GetQos() == QOS_OK)
                        {
                            globalrequest_cmd_data.rm_status = RM_GRANT;
                            RM_SetStatus(globalrequest_cmd_data.hComponent,globalrequest_cmd_data.nPid,RM_ComponentActive);
                            if(write(RM_GetPipe(globalrequest_cmd_data.hComponent,globalrequest_cmd_data.nPid),
                                     &globalrequest_cmd_data, sizeof(globalrequest_cmd_data)) < 0)
                            {
                                RM_DPRINT ("[Resource Manager] - failure write data back to component\n");
                            }
                            else
                            {
                                RM_DPRINT ("[Resource Manager] - Granted by policy, ok to write data back to component\n");
                            }
                        }
                        else {
                            policy_data.PM_Cmd = PM_FreeResources;
                            policy_data.param1 = globalrequest_cmd_data.param1;
                            policy_data.hComponent=globalrequest_cmd_data.hComponent;
                            policy_data.nPid = globalrequest_cmd_data.nPid;
                    
                            if (write(pmfdwrite,&policy_data,sizeof(policy_data)) < 0)
                                RM_DPRINT ("[Resource Manager] - failure write data to the policy manager\n");
                            else
                                RM_DPRINT ("[Resource Manager] - wrote the data to the policy manager\n");                        
                        }
                    }
                    else {
                        globalrequest_cmd_data.rm_status = RM_RESOURCEFATALERROR;
                        RM_SetStatus(globalrequest_cmd_data.hComponent,globalrequest_cmd_data.nPid,RM_WaitingForClient);
                        if(write(RM_GetPipe(globalrequest_cmd_data.hComponent,globalrequest_cmd_data.nPid),
                                 &globalrequest_cmd_data, sizeof(globalrequest_cmd_data)) < 0)
                        {
                            RM_EPRINT ("[Resource Manager] - failure write data back to component\n");
                        }
                        else
                        {
                            RM_EPRINT ("[Resource Manager] -Denied request due to pending DSP recovery\n");
                        }   
                    }

#ifdef __PERF_INSTRUMENTATION__
                PERF_SendingCommand(pPERF, cmd_data.RM_Cmd, cmd_data.param1, PERF_ModuleLLMM);
#endif

                break;
                default: 
                break;
            }
        }
     }
#endif
        else {
            RM_DPRINT("[Resource Manager] fdread not ready\n"); 
        }
    }

    close(fdread);
    close(fdwrite);

    if(unlink(RM_SERVER_IN)<0)
        RM_DPRINT("[Resource Manager] - unlink RM_SERVER_IN error\n");


#ifdef __PERF_INSTRUMENTATION__
    PERF_Done(pPERF);
#endif
    exit(0);
}


/*
   Description : This function will initialize Qos
   
   Parameter   : 
   
   Return      : 
   
*/
OMX_ERRORTYPE InitializeQos()
{
    OMX_ERRORTYPE eError= OMX_ErrorNone;
#ifndef __ENABLE_RMPM_STUB__
    int status = 0;
    unsigned int uProcId = 0;	/* default proc ID is 0. */
    unsigned int index = 0;
    unsigned int numProcs;
    char *qosdllname;

    qosdllname = getenv ("QOSDYN_FILE");
    if (qosdllname == NULL)
    {
        eError= OMX_ErrorHardware;
        fprintf (stderr, "[Resource Manager] - No QosDyn DLL file name\n");
        stub_mode = 1;
        return eError;
    }
    fprintf (stderr, "[Resource Manager] Read QOSDYN file from: %s\n", qosdllname);

    status = DspManager_Open(0, NULL);
    if (DSP_FAILED(status)) {
        fprintf(stderr, "DSPManager_Open failed \n");
        return -1;
    } 
    while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,&dspInfo,
        (unsigned int)sizeof(struct DSP_PROCESSORINFO),&numProcs))) {
        if ((dspInfo.uProcessorType == DSPTYPE_55) || 
            (dspInfo.uProcessorType == DSPTYPE_64)) {
            uProcId = index;
            status = 0;
            break;
        }
        index++;
    }
    status = DSPProcessor_Attach(uProcId, NULL, &hProc);
    if (DSP_SUCCEEDED(status)) {
        status = DSPManager_RegisterObject(
                (struct DSP_UUID *)&QOS_TI_UUID,
                DSP_DCDNODETYPE, qosdllname);
        if (DSP_SUCCEEDED(status)) {
            /*  Register the node DLL. */
            status = DSPManager_RegisterObject(
                    &QOS_TI_UUID, DSP_DCDLIBRARYTYPE,
                    qosdllname);
            if (DSP_FAILED(status)) {
                fprintf (stderr, "[Resource Manager] - InitializeQos() -- DSPManager_RegisterObject() object fail=0x%x\n", (unsigned int)status);
                eError= OMX_ErrorHardware;
                stub_mode = 1;
                return eError;
            }
        }
        else
        {
            fprintf (stderr, "[Resource Manager] - InitializeQos() -- DSPManager_RegisterObject() object fail=0x%x\n", (unsigned int)status);
            eError= OMX_ErrorHardware;
            stub_mode = 1;
            return eError;
        }
    } 
    else
    {
        fprintf (stderr, "[Resource Manager] - InitializeQos() -- DSPProcessor_Attach() object fail=0x%x\n", (unsigned int)status);
        eError= OMX_ErrorHardware;
        return eError;

    }
    registry = DSPRegistry_Create();
    if ( !registry) {
        fprintf(stderr, "DSP RegistryCreate FAILED\n");
        eError= OMX_ErrorHardware;
        return eError;
    }

#endif

    return eError;
}

/*
   Description : This function will handle request resource
   
   Parameter   : 
   
   Return      : 
   
*/
void HandleRequestResource(RESOURCEMANAGER_COMMANDDATATYPE cmd)
{
#ifndef __ENABLE_RMPM_STUB__
    if (!stub_mode)
    {
        RM_DPRINT ("[Resource Manager] - HandleRequestResource() function call\n");

        policy_data.PM_Cmd = PM_RequestPolicy;
        policy_data.param1 = cmd.param1;
        policy_data.hComponent=cmd.hComponent;
        policy_data.nPid = cmd.nPid;
        globalrequest_cmd_data.hComponent = cmd.hComponent;
        globalrequest_cmd_data.nPid = cmd.nPid;
        globalrequest_cmd_data.RM_Cmd = RM_RequestResource;
        globalrequest_cmd_data.param1 = cmd.param1;
        if (write(pmfdwrite,&policy_data,sizeof(policy_data)) < 0)
             RM_DPRINT ("[Resource Manager] - failure write data to the policy manager\n");
        else
            RM_DPRINT ("[Resource Manager] - wrote the data to the policy manager\n");
    }
#else
    if (stub_mode)
    {
        /* if using stubbed implementation we won't check policy manager but will set the component to active */
        RM_SetStatus(cmd.hComponent,cmd.nPid,RM_ComponentActive);
        RM_DPRINT("stub mode, grant by default & set component to active\n");
    }
#endif
}

/*
   Description : This fucntion will handle wait for resource
   
   Parameter   : 
   
   Return      : 
   
*/
void HandleWaitForResource(RESOURCEMANAGER_COMMANDDATATYPE cmd)
{
    int index;
    RM_DPRINT ("[Resource Manager] - HandleWaitForResource() function call\n");
    index = RM_GetListIndex(cmd.hComponent,cmd.nPid);
    if (-1 == index)
    {
        RM_DPRINT ("[Resource Manager] - Failure index became negative\n");
        goto EXIT;
    }

    if (componentList.component[index].reason == RM_ReasonPolicy) {
        RM_SetStatus(cmd.hComponent,cmd.nPid,RM_WaitingForPolicy);
    }
    else if (componentList.component[index].reason == RM_ReasonResource) {
        RM_SetStatus(cmd.hComponent,cmd.nPid,RM_WaitingForResource);
    }
    
#ifdef __PERF_INSTRUMENTATION__
    PERF_SendingCommand(pPERF, cmd_data.RM_Cmd, cmd_data.param1, PERF_ModuleLLMM);
#endif    // 

    if(write(RM_GetPipe(cmd_data.hComponent,cmd_data.nPid), &cmd_data, sizeof(cmd_data)) < 0)
        RM_DPRINT ("[Resource Manager] - failure write data back to component\n");
    else
        RM_DPRINT ("[Resource Manager] -sending wait for resources\n");
EXIT:
    return;
}


/*
   Description : This fucntion will free resource
   
   Parameter   : 
   
   Return      : 
   
*/
void HandleFreeResource(RESOURCEMANAGER_COMMANDDATATYPE cmd)
{
    int i;
    
    RM_DPRINT ("[Resource Manager] - RM_FreeResource() function call\n");
#ifndef __ENABLE_RMPM_STUB__
    if (!stub_mode)
    {
        policy_data.PM_Cmd = PM_FreePolicy;
        policy_data.hComponent = cmd.hComponent;
        policy_data.param1 = cmd.param1;
        policy_data.nPid = cmd.nPid;
        if (write(pmfdwrite,&policy_data,sizeof(policy_data)) < 0)
            RM_DPRINT ("[Resource Manager] - failure write data to the policy manager\n");
        else
            RM_DPRINT ("[Resource Manager] - wrote the data to the policy manager\n");
    }
#endif

    RM_DPRINT("componentList.numRegisteredComponents = %d\n",componentList.numRegisteredComponents);
    /* Now if there are pending components they might be able to run. */
    if (componentList.numRegisteredComponents-1 > 0) {
        for (i=0; i < componentList.numRegisteredComponents-1; i++) {
            RM_DPRINT("HandleFreeResource %d\n",__LINE__);
            if (componentList.component[i].componentState== OMX_StateWaitForResources) {
                /* temporarily now assume policy is available */
                RM_DPRINT("HandleFreeResource %d\n",__LINE__);
                cmd_data.rm_status = RM_RESOURCEACQUIRED;
                cmd_data.hComponent = componentList.component[i].componentHandle;
            
                RM_DPRINT("HandleFreeResource %d\n",__LINE__);
                write(RM_GetPipe(componentList.component[i].componentHandle,componentList.component[i].nPid), &cmd_data, sizeof(cmd_data));
            }
            else if (componentList.component[i].status == RM_WaitingForResource) {
            }
        }
    }
}

/*
   Description : This fucntion will cancel wait for resource
   
   Parameter   : 
   
   Return      : 
   
*/
void HandleCancelWaitForResource(RESOURCEMANAGER_COMMANDDATATYPE cmd)
{
        RM_DPRINT ("[Resource Manager] - RM_CancelWaitForResource() function call\n");
}

/*
   Description : This fucntion will set the resource state
   
   Parameter   : 
   
   Return      : 
   
*/
void HandleStateSet(RESOURCEMANAGER_COMMANDDATATYPE cmd)
{
    int i;
    int index=-1;
    OMX_STATETYPE previousState;
    OMX_STATETYPE newState;
    int componentType = 0;
    RM_DPRINT("HandleStateSet %d\n",__LINE__);

    switch (cmd.param1) {
        case OMX_MP3_Decoder_COMPONENT:
        case OMX_AAC_Decoder_COMPONENT:
        case OMX_AAC_Encoder_COMPONENT:
        case OMX_ARMAAC_Encoder_COMPONENT:
        case OMX_ARMAAC_Decoder_COMPONENT:
        case OMX_PCM_Decoder_COMPONENT:
        case OMX_PCM_Encoder_COMPONENT:
        case OMX_NBAMR_Decoder_COMPONENT:
        case OMX_NBAMR_Encoder_COMPONENT:
        case OMX_WBAMR_Decoder_COMPONENT:
        case OMX_WBAMR_Encoder_COMPONENT:
        case OMX_WMA_Decoder_COMPONENT:
        case OMX_G711_Decoder_COMPONENT:
        case OMX_G711_Encoder_COMPONENT:
        case OMX_G722_Decoder_COMPONENT:
        case OMX_G722_Encoder_COMPONENT:
        case OMX_G723_Decoder_COMPONENT:
        case OMX_G723_Encoder_COMPONENT:
        case OMX_G726_Decoder_COMPONENT:
        case OMX_G726_Encoder_COMPONENT:
        case OMX_G729_Decoder_COMPONENT:
        case OMX_G729_Encoder_COMPONENT:
        case OMX_GSMFR_Decoder_COMPONENT:
        case OMX_GSMHR_Decoder_COMPONENT:
        case OMX_GSMFR_Encoder_COMPONENT:
        case OMX_GSMHR_Encoder_COMPONENT:
        case OMX_ILBC_Decoder_COMPONENT:
        case OMX_ILBC_Encoder_COMPONENT:
        case OMX_RAGECKO_Decoder_COMPONENT:
            componentType = RM_AUDIO;
        break;

    /* video*/
        case OMX_MPEG4_Decode_COMPONENT:
        case OMX_MPEG4_Encode_COMPONENT:
        case OMX_H263_Decode_COMPONENT:
        case OMX_H263_Encode_COMPONENT:
        case OMX_H264_Decode_COMPONENT:
        case OMX_H264_Encode_COMPONENT:
        case OMX_WMV_Decode_COMPONENT:
        case OMX_MPEG2_Decode_COMPONENT:
        case OMX_720P_Decode_COMPONENT:
        case OMX_720P_Encode_COMPONENT:
            componentType = RM_VIDEO;
        break;

    /* image*/
        case OMX_JPEG_Decoder_COMPONENT:
        case OMX_JPEG_Encoder_COMPONENT:
        case OMX_VPP_COMPONENT:
            componentType = RM_IMAGE;
        break;

    /* camera*/
        case OMX_CAMERA_COMPONENT:
            componentType = RM_CAMERA;
        break;

        /* lcd */
        case OMX_DISPLAY_COMPONENT:
            componentType = RM_LCD;
        break;


        default:
            RM_DPRINT("[HandleStateSet] Unknown component type\n");
            componentType = -1;
        break;
        
    }

    
    RM_DPRINT ("[Resource Manager] - HandleStateSet() function call\n");

    for (i=0; i < componentList.numRegisteredComponents; i++) {
        if (componentList.component[i].componentHandle == cmd.hComponent && cmd.nPid == componentList.component[i].nPid) {
            index = i;
            break;
        }        
    }
    
    if (index != -1) {
        previousState = componentList.component[index].componentState;
        newState = cmd.param2;
        componentList.component[index].componentState = newState;
        if ((previousState == OMX_StateIdle || previousState == OMX_StatePause) && newState == OMX_StateExecuting) {
            /* If component is transitioning from Idle to Executing update the 
                 totalCpu usage of all of the components */
            if (componentType == RM_AUDIO) {
                audioTotalCpu += componentList.component[index].componentCPU;
                RM_DPRINT("RM_AUDIO request for resources\n");
            }
            else if (componentType == RM_VIDEO) {
                videoTotalCpu += componentList.component[index].componentCPU;
            }
            else if (componentType == RM_IMAGE) {
                imageTotalCpu += componentList.component[index].componentCPU;
            }
            else if (componentType == RM_CAMERA) {
                cameraTotalCpu += componentList.component[index].componentCPU;
            }
            else if (componentType == RM_LCD) {
                lcdTotalCpu += componentList.component[index].componentCPU;
            }
            else {
                RM_DPRINT("ERROR - unknown component type\n");
            }
            totalCpu = audioTotalCpu + videoTotalCpu + imageTotalCpu +
                       cameraTotalCpu + lcdTotalCpu;
            /* Inform the Resource Activity Monitor of the new CPU usage */
            RM_DPRINT("total CPU to set constraint = %d\n", totalCpu);
            rm_set_vdd1_constraint(totalCpu);        
        }
        else if (previousState == OMX_StateExecuting && (newState == OMX_StateIdle || newState == OMX_StatePause)) {

            /* If component is transitioning from Executing to Idle update the 
                 totalCpu usage of all of the components */
            if (componentType == RM_AUDIO) {
                audioTotalCpu -= componentList.component[index].componentCPU;
            }
            else if (componentType == RM_VIDEO) {
                videoTotalCpu -= componentList.component[index].componentCPU;
            }
            else if (componentType == RM_IMAGE) {
                imageTotalCpu -= componentList.component[index].componentCPU;
            }
            else if (componentType == RM_CAMERA) {
                cameraTotalCpu -= componentList.component[index].componentCPU;
            }
            else if (componentType == RM_LCD) {
                lcdTotalCpu -= componentList.component[index].componentCPU;
            }
            else {
                RM_DPRINT("ERROR - unknown component type\n");
            }
            
            /* Inform the Resource Activity Monitor of the new CPU usage */
            totalCpu = audioTotalCpu + videoTotalCpu + imageTotalCpu +
                       cameraTotalCpu + lcdTotalCpu;
            
            rm_set_vdd1_constraint(totalCpu);
        }
        RM_DPRINT("newState = %d\n",newState);
        if (newState == OMX_StateWaitForResources) {
            HandleWaitForResource(cmd);
        }
    }
    else {
        RM_DPRINT("ERROR: Component is not registered\n");
    }
}


void RM_AddPipe(RESOURCEMANAGER_COMMANDDATATYPE cmd, int aPipe)
{
    int alreadyRegistered = 0;
    int i;

    for (i=0; i < componentList.numRegisteredComponents; i++) {
        if (cmd.hComponent == componentList.component[i].componentHandle && cmd.nPid == componentList.component[i].nPid) {
            alreadyRegistered = 1;
            break;
        }
    }

    if (!alreadyRegistered) {
        componentList.component[componentList.numRegisteredComponents].componentHandle = cmd.hComponent;
        componentList.component[componentList.numRegisteredComponents].nPid = cmd.nPid;
        componentList.component[componentList.numRegisteredComponents].componentPipe = aPipe;
        componentList.component[componentList.numRegisteredComponents].componentState = OMX_StateIdle;
        componentList.component[componentList.numRegisteredComponents].reason = RM_ReasonNone;
        componentList.component[componentList.numRegisteredComponents++].componentCPU = cmd.param2;
        
    }
}


int RM_GetPipe(OMX_HANDLETYPE hComponent,OMX_U32 aPid)
{
    int returnPipe=-1;
    int listIndex;
    listIndex = RM_GetListIndex(hComponent,aPid);
    if (listIndex != -1) {
        returnPipe = componentList.component[listIndex].componentPipe;
    }
    return returnPipe;
}


void RM_ClosePipe(RESOURCEMANAGER_COMMANDDATATYPE cmd_data)
{
 
    int listIndex;
    listIndex = RM_GetListIndex(cmd_data.hComponent,cmd_data.nPid);
    /* If pipe is found close it */
    if (listIndex != -1) {
        fdwrite = componentList.component[listIndex].componentPipe;
        close(fdwrite);
    }
 }

void RM_CreatePipe(RESOURCEMANAGER_COMMANDDATATYPE cmd_data, char rmsideNamedPipeName[],char rmsideHandleString[])
{
    RM_itoa((int)cmd_data.nPid,rmsideHandleString);
    strcpy(rmsideNamedPipeName,RM_SERVER_OUT);
    strcat(rmsideNamedPipeName,"_");
    strcat(rmsideNamedPipeName,rmsideHandleString);

    // try to create the pipe, don't fail it already exists (reuse the pipe instead)
    RM_DPRINT("[Resource Manager] - Create and open the write (out) pipe\n");
    if((mknod(rmsideNamedPipeName,S_IFIFO|PERMS,0)<0) && (errno!=EEXIST))
    RM_DPRINT("[Resource Manager] - mknod failure to create the write pipe, error=%d\n", errno);
    //wait for the pipe to be established before opening it.??

    RM_DPRINT("[Resource Manager] - Try opening the write out pipe for PID %d\n", (int)cmd_data.nPid);
    if((fdwrite=open(rmsideNamedPipeName,O_WRONLY))<0) {
        RM_DPRINT("[Resource Manager] - failure to open the WRITE pipe, errno=%d\n", errno);
    }
    else {
        RM_DPRINT("[Resource Manager] - WRITE pipe opened successfully, Add pipe to the table\n");
        RM_AddPipe(cmd_data,fdwrite);
    }
}

void RM_itoa(int n, char s[])
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
    RM_reverse(s);
} 



void RM_reverse(char s[])
{
    int c, i, j;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

int RM_SetStatus(OMX_HANDLETYPE hComponent, OMX_U32 aPid, RM_COMPONENTSTATUS status)
{
    int listIndex;
    listIndex = RM_GetListIndex(hComponent,aPid);

    if (listIndex != -1) {
        componentList.component[listIndex].status = status;
    }

    return 0;
}

 int RM_SetReason(OMX_HANDLETYPE hComponent, RM_DENYREASON reason)
{
    int listIndex;
    listIndex = RM_GetListIndex(hComponent,0);
    
    if (listIndex != -1) {
        componentList.component[listIndex].reason= reason;
    }

    return 0;
}
 
int RM_GetListIndex(OMX_HANDLETYPE hComponent, OMX_U32 aPid) 
{
    int i;
    int match=-1;

    for (i=0; i < componentList.numRegisteredComponents; i++) {
        if (hComponent == componentList.component[i].componentHandle && aPid == componentList.component[i].nPid) {
            match = i;
            break;
        }
    }
    return match;
}


int RM_RemoveComponentFromList(OMX_HANDLETYPE hComponent,OMX_U32 aPid)
{
    int index = -1;
    int i;
    index = RM_GetListIndex(hComponent,aPid);

    if (index != -1) {
        /* Shift all other components in the list up */    
        for(i=index; i < componentList.numRegisteredComponents-1; i++) {
            componentList.component[i].componentCPU = componentList.component[i+1].componentCPU;
            componentList.component[i].componentHandle = componentList.component[i+1].componentHandle;
            componentList.component[i].componentState = componentList.component[i+1].componentState;
            componentList.component[i].componentPipe = componentList.component[i+1].componentPipe;
            componentList.component[i].reason = componentList.component[i+1].reason;
            componentList.component[i].status = componentList.component[i+1].status;
            componentList.component[i].nPid = componentList.component[i+1].nPid;
        }
        /* Decrement the count of registered components */
        componentList.numRegisteredComponents--;
    }

    return 0;
}

/* @deprecate
   Install_Bridge not being used in Android releases
   since it is easier to build this module into the kernel */
int Install_Bridge()
{
    int fd;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    char *filename = "/tmp/bridgeinstalled";

    system("insmod  /dspbridge/bridgedriver.ko phys_mempool_base=0x87000000  phys_mempool_size=0x600000 shm_size=0x40f000");
    system("mdev -s");

    fd = creat(filename, mode);
    if (-1 == fd)
    {
        RM_DPRINT("Failed to create file descriptor\n");
        return -1;
    }

    RM_DPRINT("Bridge Installed\n");
    return 0;
}

/* @deprecate
   Uninstall_Bridge not being used in Android releases
   since it is easier to build this module into the kernel */
int Uninstall_Bridge()
{
    system("rmmod bridgedriver");
    system("rm -f /dev/DspBridge");

    printf("Bridge Uninstalled \n");    
    return 0;
}


/* @deprecate
   Load_Baseimage is provided as reference only
   since it is usually easier to load using a 
   service in init.rc stage */
int LoadBaseimage()
{
    int fd;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    char *filename = "/tmp/baseimageloaded";

    struct stat sb = {0};

    while (stat("/tmp/bridgeinstalled",&sb)) {
        sched_yield();
    }
/*    unlink("/tmp/bridgeinstalled");*/
    unsigned int uProcId = 0;	/* default proc ID is 0. */
    unsigned int index = 0;
    struct DSP_PROCESSORINFO dspInfo;
    DSP_HPROCESSOR hProc;
    int status = 0;
    unsigned int numProcs;
    char* argv[2];

    argv[0] = "/lib/dsp/baseimage.dof";
    status = DspManager_Open(0, NULL);
    if (DSP_FAILED(status)) {
        printf("DSPManager_Open failed \n");
        return -1;
    } 
    while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,&dspInfo,
        (unsigned int)sizeof(struct DSP_PROCESSORINFO),&numProcs))) {
        if ((dspInfo.uProcessorType == DSPTYPE_55) || 
            (dspInfo.uProcessorType == DSPTYPE_64)) {
            uProcId = index;
            status = 0;
            break;
        }
        index++;
    }
    status = DSPProcessor_Attach(uProcId, NULL, &hProc);
    if (DSP_SUCCEEDED(status)) {
        status = DSPProcessor_Stop(hProc);
        if (DSP_SUCCEEDED(status)) {
            status = DSPProcessor_Load(hProc,1,(const char **)argv,NULL);
            if (DSP_SUCCEEDED(status)) {
                status = DSPProcessor_Start(hProc);
                if (DSP_SUCCEEDED(status)) {
                } 
                else {
                }
            } 
			else {
            }
            DSPProcessor_Detach(hProc);
        }
    }
    else {
    }
    fd = creat(filename, mode);
    if (-1 == fd)
    {
        RM_DPRINT("Failed to create file descriptor\n");
        return -1;
    }
    RM_DPRINT("Baseimage Loaded\n");
    return 0;
}


/*
   Description : This function will free Qos
   
   Parameter   : 
   
   Return      : 
   
*/
void FreeQos()
{
    RM_DPRINT ("[Resource Manager] - FreeQos() function call\n");

    DSPRegistry_Delete(registry);
    RM_DPRINT ("[Resource Manager] - FreeQos() registry deleted\n");

    QosTI_Delete();
    RM_DPRINT ("[Resource Manager] - FreeQos() Qos deleted\n");
}

/*
   Description : This function will register Qos
   
   Parameter   : 
   
   Return      : 
   
*/
void RegisterQos()
{
        RM_DPRINT ("[Resource Manager] - RegisterQos() function call\n");
}

/*
   Description : This function will get the current iva load using Qos
   
   Parameter   : 
   
   Return      : current IVA load in MHz
   
*/
int RM_GetQos()
{
    unsigned long NumFound;
    int i=0;
    int sum=0;
    int status = 0;
    int currentOverallUtilization=0;
    unsigned int dsp_currload=0, dsp_predload=0, dsp_currfreq=0, dsp_predfreq=0;
    int maxMhz=0;
    int op=0;
    int dsp_max_freq = 0;
    int cpu_variant = 0;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int memoryAvailable = QOS_DENY;

    /* if already set stub mode, we don't need to check for QoS */
    if (!stub_mode){
        /* try initialize Qos, if fail fallback to stub mode */
        eError = InitializeQos();
        if (eError != OMX_ErrorNone)
        {
            RM_EPRINT ("InitializeQos failed, fallback to stub mode\n");
            stub_mode = 1;
        }
    }

    if (!stub_mode)
    {
        struct QOSDATA *data;
        struct QOSRESOURCE_MEMORY *request;
        int found = 0;

        for (data = registry->ResourceRegistry; data;
                                      data = data->Next) {
            if (data->Id == QOSDataType_Memory_DynAlloc) {
                request = (struct QOSRESOURCE_MEMORY *)data;
                if (request->heapId == KAllHeaps){
                    found = 1;
                    break;
                }
            }
        }
        if(found == 0){
            RM_EPRINT("RM_GetQos failed, request->heapId not found\n");
            FreeQos();
            return QOS_DENY;
        }

        /* do not need to check DSP if reqested heap is 0 */
        if (cmd_data.param3 > 0) {
            status = QosTI_DspMsg(QOS_TI_GETMEMSTAT,
                request->heapId, USED_HEAPSIZE, ((DWORD *)
                &request->size), ((DWORD *)&request->allocated));

            if (DSP_SUCCEEDED(status)) {
                status = QosTI_DspMsg(QOS_TI_GETMEMSTAT,
                    request->heapId, LARGEST_FREE_BLOCKSIZE,
                    NULL, ((DWORD *)&request->largestfree));

                if (DSP_SUCCEEDED(status)) { /* 4 bytes alignment */
                    if (request->size > (cmd_data.param3 + 4))
                        memoryAvailable = true;
                } else { /*DSP return defined in dspbridge/api/inc/errbase.h */
                    RM_EPRINT ("QOS_TI_GETMEMSTAT return ERR(0x%x)\n", (unsigned int)status);
                    FreeQos();
                    return QOS_DENY;
                }
            } else {
                RM_EPRINT("QOS_TI_GETMEMSTAT return ERR(0x%x)\n", (unsigned int)status);
                FreeQos();
                return QOS_DENY;
            }
       } else
            memoryAvailable = true;

        RM_DPRINT("getting iva load: stub mode is %d\n", stub_mode);
        /* initialize array */
        for (i=0; i < RM_CPUAVGDEPTH; i++) {
            cpuStruct.cpuLoadSnapshots[i] = 0;
        }

        cpu_variant = get_omap_version();
        if (cpu_variant != OMAP_NOT_SUPPORTED) {
           maxMhz = get_curr_cpu_mhz(cpu_variant);
           dsp_max_freq = get_dsp_max_freq();
           op = rm_get_vdd1_constraint();
        }
        else {
            RM_EPRINT("OMAP NOT SUPPORTED, failed to get QOS!!\n");
            FreeQos();
            return QOS_DENY;
        }
        
        results = NULL;
        NumFound = 0;
        status = QosTI_GetProcLoadStat (&dsp_currload, &dsp_predload, &dsp_currfreq, &dsp_predfreq);
        if (DSP_SUCCEEDED(status)) 
        {
            /* get the dsp load from the Qos call without a DSP wake up*/
            currentOverallUtilization = dsp_currload;
            RM_DPRINT("GetProcLoadStat: dsp_currload = %d.\n\n", dsp_currload);
        }
        else 
        {
            RM_DPRINT("DSPRegistrey lookup used.\n\n");
            /* get the dsp load from the Registry call by a DSP wake up*/
            status = DSPRegistry_Find(QOSDataType_Processor_C6X, registry, results, &NumFound);
            if ( !DSP_SUCCEEDED(status) && status != DSP_ESIZE) {
                RM_DPRINT("None.\n\n");
            }

            results = malloc(NumFound * sizeof (struct QOSDATA *));
            if (!results) {
                RM_DPRINT("FAILED (out of memory)\n\n");
                return QOS_DENY;
            }

            /* Get processor usage */
            status = DSPRegistry_Find(QOSDataType_Processor_C6X, registry, results, &NumFound);
            if (DSP_SUCCEEDED(status)) {
            } 
            else {
                NumFound = 0;
            }
            p = (struct  QOSRESOURCE_PROCESSOR *) results[0];
            currentOverallUtilization = p->currentLoad;
        }
        /* we are done with the bridge handle, let's free it */
        FreeQos();

        /* If we have not yet captured RM_CPUAVGDEPTH samples add this to the next slot in the array */
        if (cpuStruct.snapshotsCaptured < RM_CPUAVGDEPTH) {
            cpuStruct.cpuLoadSnapshots[cpuStruct.snapshotsCaptured++] = currentOverallUtilization;
        }
        else {
            /* If the array is now full, shift the existing entries of the array */
            for (i=0; i < RM_CPUAVGDEPTH-1; i++) {
                cpuStruct.cpuLoadSnapshots[i] = cpuStruct.cpuLoadSnapshots[i+1];
            }
            /* ...and then put the most recent value in the last slot in the array */            
            cpuStruct.cpuLoadSnapshots[RM_CPUAVGDEPTH-1] = currentOverallUtilization;
        }

        /* Calculate the average */
        sum = 0;
        RM_EPRINT("QOS snapshots:\n");
        for (i=0; i < cpuStruct.snapshotsCaptured; i++) {
            sum += cpuStruct.cpuLoadSnapshots[i];
            RM_EPRINT("\tindex %d = %d MHz\n", i, cpuStruct.cpuLoadSnapshots[i]);
        }
        cpuStruct.averageCpuLoad = sum / cpuStruct.snapshotsCaptured;

        cpuStruct.cyclesInUse = currentOverallUtilization;
        cpuStruct.cyclesAvailable = dsp_max_freq - cpuStruct.cyclesInUse;
        RM_EPRINT("Calculating QoS: \n\tdsp_max_freq = %d\n\tcyclesInUse = %d\n\n", dsp_max_freq, cpuStruct.cyclesInUse);

        RM_EPRINT("QoS Results: \n\tmemoryAvailable = %d\n\tcyclesAvailable = %d\n\trequestedCycles = %d\n", 
                   memoryAvailable, cpuStruct.cyclesAvailable, cmd_data.param2);

        /* if memory is available and DSP cycles are available grant request */
        if (memoryAvailable && (cpuStruct.cyclesAvailable >= cmd_data.param2)) {                        
            return QOS_OK;                        
        }
        else {
            return QOS_DENY;
        }
    } //end not stub mode

    else
    {    /* in stub mode, we always grant requests */
        return QOS_OK;
    }

}


void *RM_FatalErrorWatchThread()
{

    int status = 0;
    unsigned int index=0;
    struct DSP_NOTIFICATION * notificationObjects[2];
    DSP_HPROCESSOR hProc;
    struct DSP_NOTIFICATION* notification_mmufault;
    struct DSP_NOTIFICATION* notification_syserror ;
    int i, dspAttempts=0;
    unsigned int uProcId = 0;	/* default proc ID is 0. */
    unsigned int numProcs = 0;

    while (1) {
    /* this outer loop is used to re-register and 
       continue monitoring for faults after an error
       occurs */

        do {
            status = DspManager_Open(0, NULL);
            /* DspManager_Open will fail until the bridge driver
               is in a valid state again */
            if (DSP_FAILED(status)){
                RM_EPRINT("DSPManager_Open failed, waiting for bridge driver \n");
                sleep(1);
            }
            else {
                /* should be safe to unblock new requests now */
                mmuRecoveryInProgress = 0;
                dspAttempts = 0;
                break; //RM is now open for business again
            }
        } while(++dspAttempts < MAX_TRIES);

        while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,&dspInfo,
            (unsigned int)sizeof(struct DSP_PROCESSORINFO),&numProcs))) {
            if ((dspInfo.uProcessorType == DSPTYPE_55) ||
                (dspInfo.uProcessorType == DSPTYPE_64)) {
                uProcId = index;
                status = 0;
                break;
            }
            index++;
        }
        status = DSPProcessor_Attach(uProcId, NULL, &hProc);
        DSP_ERROR_EXIT(status, "DSP processor attach failed", EXIT);
        notification_mmufault = (struct DSP_NOTIFICATION*)malloc(sizeof(struct DSP_NOTIFICATION));
        if(notification_mmufault == NULL) {
            RM_EPRINT("%d :: malloc failed....exiting rm-dsp-monitor-thread\n",__LINE__);
            /* detach processor from gpp */
            status = DSPProcessor_Detach(hProc);
            DSP_ERROR_EXIT (status, "DeInit: DSP Processor Detach ", EXIT);
            status = DspManager_Close(0, NULL);
            DSP_ERROR_EXIT (status, "DeInit: DSPManager Close ", EXIT);
            return NULL;
        }
        else
        {
            memset(notification_mmufault,0,sizeof(struct DSP_NOTIFICATION));
        }

        status = DSPProcessor_RegisterNotify(hProc, DSP_MMUFAULT, DSP_SIGNALEVENT, notification_mmufault);
        DSP_ERROR_EXIT(status, "DSP node register notify DSP_MMUFAULT", EXIT);
        notificationObjects[0] =  notification_mmufault;

        notification_syserror = (struct DSP_NOTIFICATION*)malloc(sizeof(struct DSP_NOTIFICATION));
        if(notification_syserror == NULL) {
            RM_EPRINT("%d :: malloc failed....exiting rm-dsp-monitor-thread\n",__LINE__);
            /* free the first notification object */
            free(notification_mmufault);
            /* detach processor from gpp */
            status = DSPProcessor_Detach(hProc);
            DSP_ERROR_EXIT (status, "DeInit: DSP Processor Detach ", EXIT);
            status = DspManager_Close(0, NULL);
            DSP_ERROR_EXIT (status, "DeInit: DSPManager Close ", EXIT);
            return NULL;
        }
        else
        {
            memset(notification_syserror,0,sizeof(struct DSP_NOTIFICATION));
        }
        
        status = DSPProcessor_RegisterNotify(hProc, DSP_SYSERROR, DSP_SIGNALEVENT, notification_syserror);
        DSP_ERROR_EXIT(status, "DSP node register notify DSP_SYSERROR", EXIT);
        notificationObjects[1] =  notification_syserror;

        while (1) {
            status = DSPManager_WaitForEvents(notificationObjects, 2, &index, 1000000);
            if (DSP_SUCCEEDED(status)) {
                if (index == 0 || index == 1){
                    /* exception received - start telling all components to close */
                    RM_EPRINT("DSP ERROR [%d] ... starting to preempt MM components\n",index);
                    mmuRecoveryInProgress = 1;
                    for(i=0; i < componentList.numRegisteredComponents; i++) {
                        cmd_data.rm_status = RM_RESOURCEFATALERROR;
                        cmd_data.hComponent = componentList.component[i].componentHandle;
                        cmd_data.nPid = componentList.component[i].nPid;
                        RM_SetStatus(cmd_data.hComponent, cmd_data.nPid,RM_WaitingForClient);  
                        int preemptpipe = RM_GetPipe(cmd_data.hComponent,cmd_data.nPid);
                        if (write(preemptpipe,&cmd_data,sizeof(cmd_data)) < 0){
                            RM_EPRINT("Didn't write pipe\n");
                        }
                        else {
                            RM_EPRINT("Wrote RMProxy pipe, cleaned [%d] components\n", i+1);
                        }
                    }

                    /* dsp close ensures that the dsp resources are freed by bridge */
                    status = DspManager_Close(0, NULL);
                    DSP_ERROR_EXIT (status, "DeInit: DSPManager Close ", EXIT);
                    RM_EPRINT("dsp manager close %d\n", status);

                    /* this loop should_ be required, but seems that opencore
                       does NOT DeInit the components as expected, so this
                       condition never becomes true, to work around this
                       we'll break after waiting a few moments for the client
                       to respond, this will work only because OMX components
                       implementing RM_Callback for fatal error will call
                       EMM_CodecDestroy and free resources and enter OMX_StateInvalid */
                    int maxTries = 5;
                    struct timespec tv;
                    tv.tv_sec = 0;
                    tv.tv_nsec = 20000;
                    int n = 0;
                    while (componentList.numRegisteredComponents != 0) {
                        RM_DPRINT("waiting for all component connections to be closed...\n");
                        RM_DPRINT("\tcurrently %d pending\n", componentList.numRegisteredComponents);
                        RM_DPRINT("\tpending[0] componentHandle == %p\n", componentList.component[0].componentHandle);
                        n = nanosleep(&tv, NULL);
                        if (!(--maxTries))
                            break;
                    }
                    break; //continue at start of outer while loop to continue monitoring
                }
            }
        } //end inner while(1)
    } // end outer while(1)
EXIT:    
    RM_DPRINT("leaving dsp_monitor thread\n\n");
    return NULL;
}

/* @deprecate */
void ReloadBaseimage()
{
}


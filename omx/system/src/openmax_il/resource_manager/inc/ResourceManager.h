
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
* @file ResourceManager.h
*
* This file contains the definitions used by OMX component and resource manager to 
* access common items. This file is fully compliant with the Khronos 1.0 specification.
*
* @path  $(CSLPATH)\
*
* @rev  1.0
*/
/* ---------------------------------------------------------------------------- 
*! 
*! Revision History 
*! ===================================
*! 14-Apr-2005 rg:  Initial Version. 
*!
* ============================================================================= */
 
#ifndef RESOURCEMANAGER_H__
#define RESOURCEMANAGER_H__

#include <ResourceManagerAPI.h>

#define RM_DEBUG

#include <utils/Log.h>
#undef LOG_TAG
#define LOG_TAG "OMXRM"
#define RM_EPRINT LOGE

#ifdef  RM_DEBUG
        #define RM_DPRINT LOGD
#define DSP_ERROR_EXIT(err, msg, label)                \
    if (DSP_FAILED (err)) {                        \
        ALOGD("\n****************RM ERROR : DSP ************************\n");\
        ALOGD("Error: %s : Err Num = %lx", msg, err);  \
        ALOGD("\n****************RM ERROR : DSP ************************\n");\
        goto label;                               \
    }                                              /**/
//    fprintf(stdout,__VA_ARGS__)
#else
        #define RM_DPRINT(...)
#define DSP_ERROR_EXIT(err, msg, label)                \
    if (DSP_FAILED (err)) {                        \
        printf("\n****************RM ERROR : DSP ************************\n");\
        printf("Error: %s : Err Num = %lx", msg, err);  \
        printf("\n****************RM ERROR : DSP ************************\n");\
        goto label;                               \
    }                                              /**/
#endif

#define MAXSTREAMCOUNT	10
#define RESOURCEMANAGERID 7
#define RM_MAXCOMPONENTS 100
#define MAX_TRIES 10

#define RM_IMAGE 1
#define RM_VIDEO 2
#define RM_AUDIO 3
#define RM_LCD 4
#define RM_CAMERA 5
#define RM_CPUAVGDEPTH 5
#define RM_CPUAVERAGEDELAY 1

typedef enum _RM_COMPONENTSTATUS
{
    RM_ComponentActive = 1,  /* component has been granted resource and is currently running */
    RM_WaitingForClient,         /* component has been denied resource and RM is now waiting for the component to either 
                                                    go to the WaitForResources state or else cancel the request */
    RM_WaitingForResource,   /* component has been denied resource and is in the WaitForResources state */
    RM_WaitingForPolicy         /* component has been denied policy and is in the WaitForResources state */
} RM_COMPONENTSTATUS;

typedef enum _RM_DENYREASON
{
    RM_ReasonNone = 0,          /* component has not been denied resource */
    RM_ReasonResource,          /* component has been denied resource because DSP resource is unavailable */
    RM_ReasonPolicy             /* component has been denied resource because policy manager denied request */
} RM_DENYREASON;



typedef struct RM_RegisteredComponentData
{
    OMX_HANDLETYPE componentHandle;
    OMX_U32 nPid;
    OMX_STATETYPE componentState;
    unsigned int componentCPU;
    int componentPipe;
    RM_COMPONENTSTATUS status;
    RM_DENYREASON reason;
} RM_RegisteredComponentData;


typedef struct RM_ComponentList
{ 
    int numRegisteredComponents;
    RM_RegisteredComponentData component[RM_MAXCOMPONENTS];
}RM_ComponentList;

typedef struct RM_CPULoadStruct
{ 
    int averageCpuLoad;
    int cyclesInUse;
    int cyclesAvailable;
    int snapshotsCaptured;
    int cpuLoadSnapshots[RM_CPUAVGDEPTH];
}RM_CPULoadStruct;

struct QOSREGISTRY *registry;
struct QOSRESOURCE_MEMORY *m;
struct QOSRESOURCE_PROCESSOR *p;
struct QOSDATA **results = NULL;
struct DSP_PROCESSORINFO dspInfo;
DSP_HPROCESSOR hProc;
RM_CPULoadStruct cpuStruct;

/* defining the DSP opp points for vdd1 */
#define RM_OPERATING_POINT_1 0
#define RM_OPERATING_POINT_2 1
#define RM_OPERATING_POINT_3 2
#define RM_OPERATING_POINT_4 3
#define RM_OPERATING_POINT_5 4

/* for 3440 only */
#define RM_OPERATING_POINT_6 5

#define QOS_OK 1
#define QOS_DENY 0

// internal functions
void FreeQos();
void RegisterQos(); 
OMX_ERRORTYPE InitializeQos();
int RM_GetQos();

void HandleRequestResource(RESOURCEMANAGER_COMMANDDATATYPE cmd);
void HandleWaitForResource(RESOURCEMANAGER_COMMANDDATATYPE cmd);
void HandleFreeResource(RESOURCEMANAGER_COMMANDDATATYPE cmd);
void HandleCancelWaitForResource(RESOURCEMANAGER_COMMANDDATATYPE cmd);
void HandleStateSet(RESOURCEMANAGER_COMMANDDATATYPE cmd);
void RM_AddPipe(RESOURCEMANAGER_COMMANDDATATYPE cmd, int aPipe);
void RM_ClosePipe(RESOURCEMANAGER_COMMANDDATATYPE cmd_data);
void RM_CreatePipe(RESOURCEMANAGER_COMMANDDATATYPE cmd_data, char rmsideNamedPipeName[],char rmsideHandleString[]);
int RM_RemoveComponentFromList(OMX_HANDLETYPE hComponent, OMX_U32 aPid);
int RM_GetPipe(OMX_HANDLETYPE hComponent,OMX_U32 aPid);
void RM_itoa(int n, char s[]);
void RM_reverse(char s[]);
int RM_SetStatus(OMX_HANDLETYPE hComponent, OMX_U32 aPid,RM_COMPONENTSTATUS status);
int RM_GetListIndex(OMX_HANDLETYPE hComponent,OMX_U32 aPid);
int RM_SetReason(OMX_HANDLETYPE hComponent, RM_DENYREASON reason);

int Install_Bridge();
int Uninstall_Bridge();
int LoadBaseimage();
void *RM_FatalErrorWatchThread();

#endif


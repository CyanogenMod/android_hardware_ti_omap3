
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

#include <ResourceManagerProxyAPI.h>

#undef RMPROXY_DEBUG 

#ifdef  RMPROXY_DEBUG
        #include <utils/Log.h>
        #define LOG_TAG "OMXRM PROXY"
        #define RMPROXY_DPRINT LOGD
#else
        #define RMPROXY_DPRINT(...)
#endif

#define RM_SERVER_IN "/dev/rm_server_in"
#define RM_SERVER_OUT "/dev/rm_server_out"

#define PERMS 0777

#define MAXSTREAMCOUNT	10
#define RMPROXY_MAXCOMPONENTS 100
int flag = 0;
int RMProxyfdread, RMProxyfdwrite;
int eErrno;

typedef struct _RMPROXY_CORE
{
    RMPROXY_COMMANDDATATYPE cmd_data;

} RMPROXY_CORE;

RMPROXY_HANDLETYPE RMProxy_Handle; 

/** The _RM_COMMANDDATATYPE structure defines error structure
 */
typedef enum _RESOURCEMANAGER_ERRORTYPE
{
  RM_ErrorNone = 0,
  RM_GRANT, 
  RM_DENY, 
  RM_PREEMPT, 
  RM_RESOURCEACQUIRED,
  RM_RESOURCEFATALERROR,
  PM_ErrorMax = 0x7FFFFFFF
} RESOURCEMANAGER_ERRORTYPE;


/** The _RM_COMMANDDATATYPE structure defines command data structure
 */
typedef struct _RESOURCEMANAGER_COMMANDDATATYPE
{
  OMX_HANDLETYPE			hComponent;  
  OMX_U32                                  nPid;
  RMPROXY_COMMANDTYPE		RM_Cmd; 
  OMX_U32					param1;  
  OMX_U32                                 param2;
  OMX_U32                                 param3;  
  OMX_U32                                 param4;  
  RESOURCEMANAGER_ERRORTYPE	rm_status;
} RESOURCEMANAGER_COMMANDDATATYPE; 


typedef struct RMProxy_RegisteredComponentData
{
    OMX_HANDLETYPE componentHandle;
    RMPROXY_CALLBACKTYPE *callback;
} RMProxy_RegisteredComponentData;


typedef struct RMProxy_ComponentList
{
    int numRegisteredComponents;
    RMProxy_RegisteredComponentData component[RMPROXY_MAXCOMPONENTS];
}RMProxy_ComponentList;



void *RMProxy_Thread(); 
void *RMProxy_OldThread(); 
//void HandleRMProxyCommand(RMPROXY_COMMANDDATATYPE cmd_data);
void RM_OldRequestResource(OMX_HANDLETYPE hComponent, OMX_U32 param, OMX_U32 param2, sem_t *sem, OMX_ERRORTYPE *RM_Error);
void RM_RequestResource(OMX_HANDLETYPE hComponent, OMX_U32 param, OMX_U32 param2, OMX_U32 param3,OMX_U32 aPid, sem_t *sem, OMX_ERRORTYPE *RM_Error);
void RM_WaitForResource(OMX_HANDLETYPE hComponent, OMX_U32 param, sem_t *sem, OMX_ERRORTYPE *RM_Error);
void RM_SetState(OMX_HANDLETYPE hComponent, OMX_U32 param1, OMX_U32 param2);
void RM_FreeResource(OMX_HANDLETYPE hComponent, OMX_U32 param1, OMX_U32 param2, OMX_U32 numClients);
void RM_CancelWaitForResource(OMX_HANDLETYPE hComponent, OMX_U32 param1, OMX_U32 param2);
void RMProxy_itoa(int n, char s[]);
void RMProxy_reverse(char s[]);
void RMProxy_CallbackClient(OMX_HANDLETYPE hComponent, OMX_ERRORTYPE *error, RMPROXY_CORE *core);
int RMProxy_CheckForStubMode();

#endif


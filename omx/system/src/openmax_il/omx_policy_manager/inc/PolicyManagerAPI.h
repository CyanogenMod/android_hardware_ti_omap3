
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
* @file PolicyManager.h
*
* This file contains the definitions used by OMX component and resource manager to 
* access common items. This file is fully compliant with the OMX Audio specification 1.5.
*
* @path  $(CSLPATH)\
*
* @rev  1.0
*/
/* ---------------------------------------------------------------------------- 
*! 
*! Revision History 
*! ===================================
*! 14-Apr-2006 rg:  Initial Version. 
*!
* ============================================================================= */
 
#ifndef POLICYMANAGERAPI_H__
#define POLICYMANAGERAPI_H__


#include <OMX_Types.h>
#include <OMX_Core.h>

#define PM_SERVER_IN "/dev/pm_server_in"
#define PM_SERVER_OUT "/dev/pm_server_out"   
#define PERMS 0666

/** The PolicyManager command type enumeration is used to specify the action in the
 *  RM_SendCommand method.
 */
typedef enum _PM_COMMANDTYPE
{
    PM_Init = 1, 
	PM_RequestPolicy,    
	PM_WaitForPolicy,
	PM_FreePolicy,
	PM_CancelWaitForPolicy,
	PM_FreeResources,
	PM_StateSet,
	PM_OpenPipe,
	PM_Exit,
	PM_ExitTI
} PM_COMMANDTYPE;

/** The _RM_COMMANDDATATYPE structure defines error structure
 */
#if 1
typedef enum _POLICYMANAGER_ERRORTYPE
{
  PM_ErrorNone = 0,
  PM_GRANT, 
  PM_DENY, 
  PPM_ErrorMax = 0x7FFFFFFF
} POLICYMANAGER_ERRORTYPE;
#endif

typedef enum _POLICYMANAGER_TORESOURCEMANAGER
{
    PM_GRANTPOLICY = 0,
    PM_DENYPOLICY,
    PM_PREEMPTED
} POLICYMANAGER_TORESOURCEMANAGER;

/** The _RM_COMMANDDATATYPE structure defines command data structure
 */
typedef struct _POLICYMANAGER_COMMANDDATATYPE
{
  OMX_HANDLETYPE			hComponent;  
  OMX_U32                                 nPid;
  PM_COMMANDTYPE			PM_Cmd; 
  OMX_U32					param1;  
  OMX_U32                                 param2;
  OMX_U32                                 param3;
  POLICYMANAGER_ERRORTYPE	rm_status;
} POLICYMANAGER_COMMANDDATATYPE; 

typedef struct _POLICYMANAGER_RESPONSEDATATYPE
{
  OMX_HANDLETYPE                                                         hComponent;  
  OMX_U32                                   nPid;
  POLICYMANAGER_TORESOURCEMANAGER                       PM_Cmd; 
  OMX_U32					                                    param1;  
  OMX_U32                                 param2;
  OMX_U32                                 param3;
  POLICYMANAGER_ERRORTYPE	rm_status;
} POLICYMANAGER_RESPONSEDATATYPE; 

#endif


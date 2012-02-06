
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
 
#ifndef RESOURCEMANAGERAPI_H__
#define RESOURCEMANAGERAPI_H__


#include <OMX_Types.h>
#include <OMX_Core.h>

#define RM_SERVER_IN "/dev/rm_server_in"
#define RM_SERVER_OUT "/dev/rm_server_out"   
#define PM_SERVER_IN "/dev/pm_server_in"
#define PM_SERVER_OUT "/dev/pm_server_out"   

#define PERMS 0777

/** The ResourceManager command type enumeration is used to specify the action in the
 *  RM_SendCommand method.
 */
typedef enum _RM_COMMANDTYPE
{
    RM_Init = 1, 
	RM_RequestResource,    
	RM_WaitForResource,
	RM_FreeResource,
       RM_FreeAndCloseResource,	
	RM_CancelWaitForResource,
	RM_StateSet,
	RM_OpenPipe,
	RM_ReusePipe,
	RM_Exit,
	RM_ExitTI
} RM_COMMANDTYPE;

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
  OMX_U32                                   nPid;
  RM_COMMANDTYPE			RM_Cmd; 
  OMX_U32					param1;  
  OMX_U32                                 param2;
  OMX_U32                                 param3;
  OMX_U32                                 param4;  
  RESOURCEMANAGER_ERRORTYPE	rm_status;
} RESOURCEMANAGER_COMMANDDATATYPE; 

#endif


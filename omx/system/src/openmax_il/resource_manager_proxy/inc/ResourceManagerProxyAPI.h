
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
* @file ResourceManagerProxyAPI.h
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
 
#ifndef RESOURCEMANAGERPROXYAPI_H__
#define RESOURCEMANAGERPROXYAPI_H__


#include <OMX_Types.h>
#include <OMX_Core.h>
#include <semaphore.h>
#include <pthread.h>


/** The ResourceManager command type enumeration is used to specify the action in the
 *  RM_SendCommand method.
 */
typedef enum _RMPROXY_COMMANDTYPE
{
    RMProxy_Init = 1, 
	RMProxy_RequestResource,    
	RMProxy_WaitForResource,
	RMProxy_FreeResource,
	RMProxy_FreeAndCloseResource,
	RMProxy_CancelWaitForResource,
	RMProxy_StateSet,
	RMProxy_OpenPipe, 
	RMProxy_ReusePipe,
	RMProxy_Exit 
} RMPROXY_COMMANDTYPE;


/** The _RM_COMMANDDATATYPE structure defines command data structure
 */
typedef struct _RMPROXY_COMMANDDATATYPE
{
  OMX_HANDLETYPE        hComponent;
  OMX_U32               nPid;
  RMPROXY_COMMANDTYPE   RM_Cmd;
  sem_t                 *sem;
  OMX_U32               param1;
  OMX_U32               param2;
  OMX_U32               param3;
  OMX_U32               param4;
  OMX_ERRORTYPE         *RM_Error;
} RMPROXY_COMMANDDATATYPE;


/** The _RM_HANDLETYPE structure defines handle structure
 */
typedef struct _RMPROXY_HANDLETYPE
{
  pthread_t    threadId;
  int          tothread[2];
} RMPROXY_HANDLETYPE;

typedef struct RMPROXY_CALLBACKTYPE {
    void (*RMPROXY_Callback) (RMPROXY_COMMANDDATATYPE cbData);
}RMPROXY_CALLBACKTYPE;

/** The OMX component type enumeration is used to specify component type
 */
typedef enum _OMX_COMPONENTINDEXTYPE
{
        /* audio component*/
        OMX_MP3_Decoder_COMPONENT = 0,
        OMX_AAC_Decoder_COMPONENT,
        OMX_AAC_Encoder_COMPONENT,
        OMX_ARMAAC_Encoder_COMPONENT,
        OMX_ARMAAC_Decoder_COMPONENT,
        OMX_PCM_Decoder_COMPONENT,
        OMX_PCM_Encoder_COMPONENT,
        OMX_NBAMR_Decoder_COMPONENT,
        OMX_NBAMR_Encoder_COMPONENT,
        OMX_WBAMR_Decoder_COMPONENT,
        OMX_WBAMR_Encoder_COMPONENT,
        OMX_WMA_Decoder_COMPONENT,
        OMX_G711_Decoder_COMPONENT,
        OMX_G711_Encoder_COMPONENT,
        OMX_G722_Decoder_COMPONENT,
        OMX_G722_Encoder_COMPONENT,
        OMX_G723_Decoder_COMPONENT,
        OMX_G723_Encoder_COMPONENT,
        OMX_G726_Decoder_COMPONENT,
        OMX_G726_Encoder_COMPONENT,
        OMX_G729_Decoder_COMPONENT,
        OMX_G729_Encoder_COMPONENT,
        OMX_GSMFR_Decoder_COMPONENT,
        OMX_GSMHR_Decoder_COMPONENT,
        OMX_GSMFR_Encoder_COMPONENT,
        OMX_GSMHR_Encoder_COMPONENT,
        OMX_ILBC_Decoder_COMPONENT,
        OMX_ILBC_Encoder_COMPONENT,
        OMX_IMAADPCM_Decoder_COMPONENT,
        OMX_IMAADPCM_Encoder_COMPONENT,		
        OMX_RAGECKO_Decoder_COMPONENT,

        /* video*/
        OMX_MPEG4_Decode_COMPONENT,
        OMX_MPEG4_Encode_COMPONENT,
        OMX_H263_Decode_COMPONENT,
        OMX_H263_Encode_COMPONENT,
        OMX_H264_Decode_COMPONENT,
        OMX_H264_Encode_COMPONENT,
        OMX_WMV_Decode_COMPONENT,
        OMX_MPEG2_Decode_COMPONENT,
        OMX_720P_Decode_COMPONENT,
        OMX_720P_Encode_COMPONENT,

        /* image*/
        OMX_JPEG_Decoder_COMPONENT,
        OMX_JPEG_Encoder_COMPONENT,
        OMX_VPP_COMPONENT,

        /* camera*/
        OMX_CAMERA_COMPONENT,
        OMX_DISPLAY_COMPONENT
} OMX_COMPONENTINDEXTYPE ;

typedef enum _OMX_LINUX_COMPONENTTYPE
{
    OMX_COMPONENTTYPE_AUDIO = 0,
    OMX_COMPONENTTYPE_VIDEO,
    OMX_COMPONENTTYPE_VPP,
    OMX_COMPONENTTYPE_IMAGE,
    OMX_COMPONENTTYPE_CAMERA,
    OMX_COMPONENTTYPE_DISPLAY1,
    OMX_COMPONENTTYPE_DISPLAY2
} OMX_LINUX_COMPONENTTYPE ;

typedef enum _OMX_LINUX_CALLBACK_EVENTTYPE
{
    OMX_RmProxyCallback_ResourcesAcquired = 0,
    OMX_RmProxyCallback_ResourcesPreempted = OMX_ErrorResourcesPreempted,
    OMX_RmProxyCallback_FatalError = 1
} OMX_LINUX_CALLBACK_EVENTTYPE;

typedef enum _RMPROXY_BOOST_LEVEL
{
    RMPROXY_MAX_BOOST = 0,
    RMPROXY_NOMINAL_BOOST,
} RMPROXY_BOOST_LEVEL;

OMX_ERRORTYPE RMProxy_NewInitalize();
OMX_ERRORTYPE RMProxy_NewInitalizeEx(OMX_LINUX_COMPONENTTYPE componetType);
OMX_ERRORTYPE RMProxy_Deinitalize();
OMX_ERRORTYPE RMProxy_DeinitalizeEx(OMX_LINUX_COMPONENTTYPE componentType);
OMX_ERRORTYPE RMProxy_NewSendCommand(OMX_HANDLETYPE hComponent, RMPROXY_COMMANDTYPE cmd, OMX_U32 para1, OMX_U32 para2, OMX_U32 para3, OMX_PTR para4);
OMX_ERRORTYPE RMProxy_RequestBoost(int level);
OMX_ERRORTYPE RMProxy_ReleaseBoost();

#endif


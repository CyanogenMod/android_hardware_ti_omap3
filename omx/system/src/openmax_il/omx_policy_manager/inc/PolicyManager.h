
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
 
#ifndef POLICYMANAGER_H__
#define POLICYMANAGER_H__

#include <PolicyManagerAPI.h>

/* add for Qos API
#include <dbapi.h>
#include "dasf_ti.h"
*/

#undef PM_DEBUG
#ifndef ANDROID
#define ANDROID 
#endif

#ifdef  PM_DEBUG
  #ifdef ANDROID
    #define PM_DPRINT LOGD
    #undef LOG_TAG
    #define LOG_TAG "OMX Policy Manager"
    #include <utils/Log.h>
  #else
    #define PM_DPRINT(...)    fprintf(stdout,__VA_ARGS__)
  #endif
#else
        #define PM_DPRINT(...)
#endif

#define MAXSTREAMCOUNT	10
#define RESOURCEMANAGERID 7
#define RM_MAXCOMPONENTS 100
#define PM_MAXSTRINGLENGTH 2000
#define RM_IMAGE 1
#define RM_VIDEO 2
#define RM_AUDIO 3
#define RM_LCD 4
#define RM_CAMERA 5
#define PM_NUM_COMPONENTS 47
#define OMX_POLICY_MAX_COMBINATION_LENGTH 50
#define OMX_POLICY_MAX_COMBINATIONS 111

char *PM_ComponentTable[PM_NUM_COMPONENTS]= {
	/* audio component*/
    "OMX_MP3_Decoder_COMPONENT",
    "OMX_AAC_Decoder_COMPONENT",
    "OMX_AAC_Encoder_COMPONENT",
    "OMX_ARMAAC_Encoder_COMPONENT",
    "OMX_ARMAAC_Decoder_COMPONENT",
    "OMX_PCM_Decoder_COMPONENT",
    "OMX_PCM_Encoder_COMPONENT",
    "OMX_NBAMR_Decoder_COMPONENT",
    "OMX_NBAMR_Encoder_COMPONENT",
    "OMX_WBAMR_Decoder_COMPONENT",
    "OMX_WBAMR_Encoder_COMPONENT",
    "OMX_WMA_Decoder_COMPONENT",
    "OMX_G711_Decoder_COMPONENT",
    "OMX_G711_Encoder_COMPONENT",
    "OMX_G722_Decoder_COMPONENT",
    "OMX_G722_Encoder_COMPONENT",
    "OMX_G723_Decoder_COMPONENT",
    "OMX_G723_Encoder_COMPONENT",
    "OMX_G726_Decoder_COMPONENT",
    "OMX_G726_Encoder_COMPONENT",
    "OMX_G729_Decoder_COMPONENT",
    "OMX_G729_Encoder_COMPONENT",
    "OMX_GSMFR_Decoder_COMPONENT",
    "OMX_GSMHR_Decoder_COMPONENT",
    "OMX_GSMFR_Encoder_COMPONENT",
    "OMX_GSMHR_Encoder_COMPONENT",
    "OMX_ILBC_Decoder_COMPONENT",
    "OMX_ILBC_Encoder_COMPONENT",
    "OMX_IMAADPCM_Decoder_COMPONENT",
    "OMX_IMAADPCM_Encoder_COMPONENT",	
    "OMX_RAGECKO_Decoder_COMPONENT",

    /* video*/
    "OMX_MPEG4_Decode_COMPONENT",
    "OMX_MPEG4_Encode_COMPONENT",
    "OMX_H263_Decode_COMPONENT",
    "OMX_H263_Encode_COMPONENT",
    "OMX_H264_Decode_COMPONENT",
    "OMX_H264_Encode_COMPONENT",
    "OMX_WMV_Decode_COMPONENT",
    "OMX_MPEG2_Decode_COMPONENT",
    "OMX_720P_Decode_COMPONENT",
    "OMX_720P_Encode_COMPONENT",

    /* image*/
    "OMX_JPEG_Decoder_COMPONENT",
    "OMX_JPEG_Encoder_COMPONENT",
    "OMX_VPP_COMPONENT",

    /* camera*/
    "OMX_CAMERA_COMPONENT",
    "OMX_DISPLAY_COMPONENT"

};

typedef struct OMX_POLICY_COMPONENT_PRIORITY {
    OMX_COMPONENTINDEXTYPE component;
    OMX_U8 priority;
} OMX_POLICY_COMPONENT_PRIORITY;


typedef struct OMX_POLICY_COMBINATION {
    OMX_U8 numComponentsInCombination;
    OMX_BOOL bCombinationIsActive;
    OMX_POLICY_COMPONENT_PRIORITY component[OMX_POLICY_MAX_COMBINATION_LENGTH];
} OMX_POLICY_COMBINATION;


typedef struct OMX_POLICY_COMBINATION_LIST {
    OMX_U8 numCombinations;
    OMX_U8 combinationList[OMX_POLICY_MAX_COMBINATIONS];
}OMX_POLICY_COMBINATION_LIST;

typedef struct OMX_POLICY_MANAGER_COMPONENTS_TYPE {
    OMX_HANDLETYPE componentHandle;
    OMX_U32 nPid;
} OMX_POLICY_MANAGER_COMPONENTS_TYPE;

// internal functions
void HandleRequestPolicy(POLICYMANAGER_COMMANDDATATYPE cmd);
void HandleWaitForPolicy(POLICYMANAGER_COMMANDDATATYPE cmd);
void HandleFreePolicy(POLICYMANAGER_COMMANDDATATYPE cmd);
void HandleFreeResources(POLICYMANAGER_COMMANDDATATYPE cmd);
void HandleCancelWaitForPolicy(POLICYMANAGER_COMMANDDATATYPE cmd);
void PreemptComponent(OMX_HANDLETYPE hComponent, OMX_U32 aPid);
void DenyPolicy(OMX_HANDLETYPE hComponent, OMX_U32 aPid);
void HandleStateSet(POLICYMANAGER_COMMANDDATATYPE cmd);
void RM_AddPipe(POLICYMANAGER_COMMANDDATATYPE cmd, int aPipe);
void RM_ClosePipe(POLICYMANAGER_COMMANDDATATYPE cmd_data);
int RM_GetPipe(POLICYMANAGER_COMMANDDATATYPE cmd);
void RM_itoa(int n, char s[]);
void RM_reverse(char s[]);
void GrantPolicy(OMX_HANDLETYPE hComponent, OMX_U8 aComponentIndex, OMX_U8 aPriority, OMX_U32 aPid);
void RemoveComponentFromList(OMX_HANDLETYPE hComponent, OMX_U32 aPid, OMX_U32 cComponentIndex);
int PopulatePolicyTable();
OMX_COMPONENTINDEXTYPE PolicyStringToIndex(char* aString) ;
OMX_BOOL CheckActiveCombination(OMX_COMPONENTINDEXTYPE componentRequestingPolicy, int *priority);
OMX_BOOL CheckAllCombinations(OMX_COMPONENTINDEXTYPE componentRequestingPolicy, int *combination, int *priority);
OMX_POLICY_COMBINATION_LIST GetSupportingCombinations(OMX_COMPONENTINDEXTYPE componentRequestingPolicy);
OMX_BOOL CanAllComponentsCoexist(OMX_COMPONENTINDEXTYPE componentRequestingPolicy,int *combination);
int GetPriority(OMX_COMPONENTINDEXTYPE component, int combination);



#endif


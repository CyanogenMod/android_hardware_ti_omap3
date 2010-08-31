/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*******************************************************************************\
*
*   FILE NAME:      ccm_vaci_debug.c
*
*   BRIEF:          This file include debug code for the Connectivity Chip
*                   Manager (CCM) Voice and Audio Control (VAC) component.
*                  
*
*   DESCRIPTION:    This file includes mainly enum-string conversion functions
*
*   AUTHOR:         Ronen Kalish
*
\*******************************************************************************/
#include "ccm_vaci_debug.h"

const char *_CCM_VAC_DebugResourceStr(ECAL_Resource eResource)
{
    const char *recourceName;
    
    switch(eResource)
    {
        case CAL_RESOURCE_I2SH:
            recourceName = "I2S";
            break;
        
        case CAL_RESOURCE_PCMH:
            recourceName = "PCM";
            break;
        
        case CAL_RESOURCE_PCMT_1:
            recourceName = "PCM_FRAME_1";
            break;
        
        case CAL_RESOURCE_PCMT_2:
            recourceName = "PCM_FRAME_2";
            break;
        
        case CAL_RESOURCE_PCMT_3:
            recourceName = "PCM_FRAME_3";
            break;
        
        case CAL_RESOURCE_PCMT_4:
            recourceName = "PCM_FRAME_4";
            break;
        
        case CAL_RESOURCE_PCMT_5:
            recourceName = "PCM_FRAME_5";
            break;
        
        case CAL_RESOURCE_PCMT_6:
            recourceName = "PCM_FRAME_6";
            break;
        
        case CAL_RESOURCE_FM_ANALOG:
            recourceName = "FM_ANALOG";
            break;
        
        case CAL_RESOURCE_PCMIF:
            recourceName = "PCM_IF";
            break;
        
        case CAL_RESOURCE_FMIF:
            recourceName = "FM_IF";
            break;
        
        case CAL_RESOURCE_CORTEX:
            recourceName = "CORTEX";
            break;
        
        case CAL_RESOURCE_FM_CORE:
            recourceName = "FM_CORE";
            break;
        
        default:
            recourceName = "UNKNOWN";
            break;
    }
    
    return (recourceName);
}

const char *_CCM_VAC_DebugOperationStr(ECAL_Operation eOperation)
{
    const char *operationName;
    
    switch(eOperation)
    {
        case CAL_OPERATION_FM_TX:
            operationName = "FM_TX";
            break;
        
        case CAL_OPERATION_FM_RX:
            operationName = "FM_RX";
            break;
        
        case CAL_OPERATION_A3DP:
            operationName = "A3DP";
            break;
        
        case CAL_OPERATION_BT_VOICE:
            operationName = "BT_VOICE";
            break;
        
        case CAL_OPERATION_WBS:
            operationName = "WBS";
            break;
        
        case CAL_OPERATION_AWBS:
            operationName = "AWBS";
            break;
        
        case CAL_OPERATION_FM_RX_OVER_SCO:
            operationName = "FM over SCO";
            break;
        
        case CAL_OPERATION_FM_RX_OVER_A3DP:
            operationName = "FM over A3DP";
            break;
        
        default:
            operationName = "UNKNOWN";
            break;
    }
    
    return (operationName);
}


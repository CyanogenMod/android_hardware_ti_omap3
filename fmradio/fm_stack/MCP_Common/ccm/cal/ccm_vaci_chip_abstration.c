/*
 * TI's FM Stack
 *
 * Copyright 2001-2008 Texas Instruments, Inc. - http://www.ti.com/
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
*   FILE NAME:      ccm_vaci_configuration_engine.c
*
*   BRIEF:          This file defines the implementation of the Connectivity Chip
*                   Manager (CCM) Voice and Audio Control (VAC) configuration
*                   engine component.
*
*
*   DESCRIPTION:    The configuration engine is a CCM-VAC internal module Performing
*                   VAC operations (starting and stopping an operation, changing
*                   routing and configuration)
*
*   AUTHOR:        Malovany Ram
*
\*******************************************************************************/



/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "ccm_vaci_chip_abstration.h"
#include "mcp_endian.h"
#include "mcp_hal_string.h"
#include "ccm_vaci_cal_chip_6350.h"
#include "ccm_vaci_cal_chip_6450_1_0.h"
#include "ccm_vaci_cal_chip_1273.h"
#include "mcp_hal_memory.h"

/*******************************************************************************
 *
 * Macro definitions
 *
 ******************************************************************************/

/*Convert Sample frequency to Frame Sync Frequency*/
#define CAL_SAMPLE_FREQ_TO_CODEC(_sampleFreq)              (((_sampleFreq) == 0) ? 8000 : \
                                                                 (((_sampleFreq) == 1) ? 11025 : \
                                                                  (((_sampleFreq) == 2) ? 12000 : \
                                                                   (((_sampleFreq) == 3) ? 16000 : \
                                                                    (((_sampleFreq) == 4) ? 22050 : \
                                                                     (((_sampleFreq) == 5) ? 24000 : \
                                                                      (((_sampleFreq) == 6) ? 32000 : \
                                                                       (((_sampleFreq) == 7) ? 44100 : 48000))))))))

/********************************************************************************
 *
 * Globals
 *
 *******************************************************************************/

struct _Cal_Config_ID
{
    /*Chip version parameter */
    ECAL_ChipVersion chip_version;

    /*Operation Data Struct*/
    Cal_Resource_Config resourcedata;

	McpConfigParser 			*pConfigParser;		  /* configuration file storage and parser */
};


typedef struct
{
    Cal_Config_ID       calconfigid[MCP_HAL_MAX_NUM_OF_CHIPS];
} _Cal_Resource_Chipid_Config;

MCP_STATIC  _Cal_Resource_Chipid_Config calresource_Chipidconfig;


MCP_STATIC _Cal_Codec_Config codecConfigParams[] =  {
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_NUMBER_OF_PCM_CLK_CH1,	0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_NUMBER_OF_PCM_CLK_CH2,	0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_PCM_CLOCK_RATE,			0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_PCM_DIRECTION_ROLE,		0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_FRAME_SYNC_DUTY_CYCLE,	0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_FRAME_SYNC_EDGE,		0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_FRAME_SYNC_POLARITY, 	0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH1_DATA_OUT_SIZE,		0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH1_DATA_OUT_OFFSET, 	0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH1_OUT_EDGE,			0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH1_DATA_IN_SIZE,		0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH1_DATA_IN_OFFSET, 		0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH1_IN_EDGE, 			0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH2_DATA_OUT_SIZE,		0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH2_DATA_OUT_OFFSET,	0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH2_OUT_EDGE,			0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH2_DATA_IN_SIZE,		0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH2_DATA_IN_OFFSET,		0},
				{CCM_VAC_CONFIG_INI_CODEC_CONFIG_PARAM_CH2_IN_EDGE,				0},
			 };
typedef enum
{
	NUMBER_OF_PCM_CLK_CH1_LOC = 0,
	NUMBER_OF_PCM_CLK_CH2_LOC ,
	PCM_CLOCK_RATE_LOC,
	PCM_DIRECTION_ROLE_LOC,
	FRAME_SYNC_DUTY_CYCLE_LOC,
	FRAME_SYNC_EDGE_LOC,
	FRAME_SYNC_POLARITY_LOC,
	CH1_DATA_OUT_SIZE_LOC,
	CH1_DATA_OUT_OFFSET_LOC,
	CH1_OUT_EDGE_LOC,
	CH1_DATA_IN_SIZE_LOC,
	CH1_DATA_IN_OFFSET_LOC,
	CH1_IN_EDGE_LOC,
	CH2_DATA_OUT_SIZE_LOC,
	CH2_DATA_OUT_OFFSET_LOC,
	CH2_OUT_EDGE_LOC,
	CH2_DATA_IN_SIZE_LOC,
	CH2_DATA_IN_OFFSET_LOC,
	CH2_IN_EDGE_LOC,
	CONFIG_PARAM_MAX_VALUE = CH2_IN_EDGE_LOC,
}_Cal_Param_Loc;



_Cal_Codec_Config fmPcmConfigParams[] = {
				{CCM_VAC_CONFIG_INI_FM_PCMI_RIGHT_LEFT_SWAP,	0},
				{CCM_VAC_CONFIG_INI_FM_PCMI_BIT_OFFSET_VECTOR,	0},
				{CCM_VAC_CONFIG_INI_FM_PCMI_SLOT_OFSET_VECTOR,			0},
				{CCM_VAC_CONFIG_INI_FM_PCMI_PCM_INTERFACE_CHANNEL_DATA_SIZE,		0},
			 };

_Cal_Codec_Config fmI2sConfigParams[] =	{
				{CCM_VAC_CONFIG_INI_FM_I2S_DATA_WIDTH,	0},
				{CCM_VAC_CONFIG_INI_FM_I2S_DATA_FORMAT, 0},
				{CCM_VAC_CONFIG_INI_FM_I2S_MASTER_SLAVE,			0},
				{CCM_VAC_CONFIG_INI_FM_I2S_SDO_TRI_STATE_MODE,		0},
				{CCM_VAC_CONFIG_INI_FM_I2S_SDO_PHASE_WS_PHASE_SELECT,			0},
				{CCM_VAC_CONFIG_INI_FM_I2S_SDO_3ST_ALWZ,		0},
			 };

/********************************************************************************
 *
 * Internal functions prototypes
 *
 *******************************************************************************/
MCP_STATIC const char *ChipToString(ECAL_ChipVersion chip);
MCP_STATIC const char *OperationToString(ECAL_Operation eResource);
MCP_STATIC const char *ResourceToString(ECAL_Resource eResource);
MCP_STATIC const char *IFStatusToString(BtHciIfStatus status);
MCP_STATIC void Cal_Prep_PCMIF_Config(McpHciSeqCmdToken *pToken, void *pUserData);
MCP_STATIC McpU8 Set_params_PCMIF(Cal_Resource_Data  *presourcedata,McpU8 *pBuffer);
MCP_STATIC void CAL_Init_id_data (Cal_Config_ID *pConfigid, BtHciIfObj *hciIfObj);

/********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/
void CCM_CAL_StaticInit(void)
{
}

void CAL_Create(McpHalChipId chipId, BtHciIfObj *hciIfObj, Cal_Config_ID **ppConfigid,McpConfigParser 			*pConfigParser)
{
    MCP_FUNC_START("CAL_Create");


    *ppConfigid = (Cal_Config_ID *)1;

    MCP_FUNC_END();

}

void CAL_Destroy(Cal_Config_ID **ppConfigid)
{
    *ppConfigid = (Cal_Config_ID *)0;
}


void CAL_VAC_Set_Chip_Version(Cal_Config_ID *pConfigid,
                              McpU16 projectType,
                              McpU16 versionMajor,
                              McpU16 versionMinor)
{
    MCP_UNUSED_PARAMETER (pConfigid);
    MCP_UNUSED_PARAMETER (projectType);
    MCP_UNUSED_PARAMETER (versionMajor);
    MCP_UNUSED_PARAMETER (versionMinor);
}

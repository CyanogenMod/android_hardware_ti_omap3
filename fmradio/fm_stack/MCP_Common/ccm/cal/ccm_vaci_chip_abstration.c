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
#include "ccm_vaci_cal_chip_6450_2.h"
#include "ccm_vaci_cal_chip_1273.h"
#include "ccm_vaci_cal_chip_1273_2.h"
#include "ccm_vaci_cal_chip_1283.h"
#include "mcp_hal_memory.h"
#include "mcp_hal_log.h"

MCP_HAL_LOG_SET_MODULE(MCP_HAL_LOG_MODULE_TYPE_CCM_CAL);

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
} ;


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
    MCP_FUNC_START ("CCM_CAL_StaticInit");

    /* no need to do anything - everything is per object */

    MCP_FUNC_END ();
}

void CAL_Create(McpHalChipId chipId, BtHciIfObj *hciIfObj, Cal_Config_ID **ppConfigid,McpConfigParser 			*pConfigParser)
{    
    MCP_FUNC_START("CAL_Create");

    MCP_FUNC_END();

}

void CAL_Destroy(Cal_Config_ID **ppConfigid)
{       
    MCP_FUNC_START ("CAL_Destroy");

    MCP_FUNC_END ();
}

/*---------------------------------------------------------------------------
 *            CAL_Init_id_data()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initialize Resource and FM data acording to the chip
 *
 */

MCP_STATIC void CAL_Init_id_data (Cal_Config_ID *pConfigid, BtHciIfObj *hciIfObj)
{       
}



void CCM_CAL_Configure(Cal_Config_ID *pConfigid,
                              McpU16 projectType,
                              McpU16 versionMajor,
                              McpU16 versionMinor)
{
}


void CAL_GetAudioCapabilities (Cal_Config_ID *pConfigid,
                               TCAL_MappingConfiguration *tOperationToResource,
                               TCAL_OpPairConfig *tOpPairConfig,
                               TCAL_ResourceSupport *tResource,
                               TCAL_OperationSupport *tOperation)

{   
    MCP_FUNC_START("CAL_GetAudioCapabilities");

    MCP_FUNC_END();
}



/*FM-VAC*/

ECAL_RetValue CAL_ConfigResource(Cal_Config_ID *pConfigid,
                                 ECAL_Operation eOperation, 
                                 ECAL_Resource eResource,
                                 TCAL_DigitalConfig *ptConfig,
                                 TCAL_ResourceProperties *ptProperties,
                                 TCAL_CB fCB,
                                 void *pUserData)
{
    MCP_FUNC_START("CAL_ConfigResource");

    MCP_FUNC_END();

    return CAL_STATUS_SUCCESS;

}


/*---------------------------------------------------------------------------
 *            Cal_Prep_PCMIF_Config()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Prepare function for thr PCM interface
 *
 */
MCP_STATIC void Cal_Prep_PCMIF_Config(McpHciSeqCmdToken *pToken, void *pUserData)
{
}


MCP_STATIC McpU8 Set_params_PCMIF(Cal_Resource_Data  *presourcedata,McpU8 *pBuffer)
{
    return 0; /* number of bytes written */
}
ECAL_RetValue CAL_StopResourceConfiguration (Cal_Config_ID *pConfigid,
                                             ECAL_Operation eOperation,
                                             ECAL_Resource eResource,
                                             TCAL_ResourceProperties *ptProperties,
                                             TCAL_CB fCB,
                                             void *pUserData)
{
    MCP_FUNC_START("CalGenericHciCB");

    MCP_FUNC_END();

    return CAL_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
 *            CAL_Config_CB_Complete()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  generic callback function for command complete on HCI commands 
 *            directly followed by other commands
 *
 */

void CAL_Config_CB_Complete(BtHciIfClientEvent *pEvent)
{
    MCP_FUNC_START("CAL_Config_CB_Complete");

    MCP_LOG_INFO (("CAL_Config_CB_Complete: received event->type %d with status %d", 
                  pEvent->type,pEvent->status));

    MCP_FUNC_END();
}

void CAL_Config_Complete_Null_CB(BtHciIfClientEvent *pEvent)
{
    MCP_FUNC_START("CAL_Config_Complete_Null_CB");

    MCP_LOG_INFO (("CAL_Config_Complete_Null_CB: received event type %d with status %d", 
                  pEvent->type, pEvent->status));

    MCP_FUNC_END();
}

ECAL_Resource StringtoResource(const McpU8 *str1)
{
    return CAL_RESOURCE_INVALID;
}


ECAL_Operation StringtoOperation(const McpU8 *str1)
{
    return CAL_OPERATION_INVALID;
}



/*---------------------------------------------------------------------------
 *            ChipToString()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Convert enum eChip_Version to string
 *
 */
MCP_STATIC const char *ChipToString(ECAL_ChipVersion chip)
{
    switch (chip)
    {
    case CAL_CHIP_6350:
        return "6350";
    case CAL_CHIP_6450_1_0:
        return "6450_1_0";
    case CAL_CHIP_6450_2:
        return "6450_2";
    case CAL_CHIP_1273:
        return "1273";
    case CAL_CHIP_1273_2:
    	return "1273_2";
    case CAL_CHIP_1283:
        return "1283";
    case CAL_CHIP_5500:
        return "5500";    
    }
    return "UNKNOWN";
}

/*---------------------------------------------------------------------------
 *            OperationToString()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Convert enum Operation to string
 *
 */
MCP_STATIC const char *OperationToString(ECAL_Operation eOperation)
{
    switch (eOperation)
    {
    case CAL_OPERATION_FM_TX:
        return "CAL_OPERATION_FM_TX";
    case CAL_OPERATION_FM_RX:
        return "CAL_OPERATION_FM_RX";
    case CAL_OPERATION_A3DP:
        return "CAL_OPERATION_A3DP";
    case CAL_OPERATION_BT_VOICE:
        return "CAL_OPERATION_BT_VOICE";
    case CAL_OPERATION_WBS:
        return "CAL_OPERATION_WBS";    
    case CAL_OPERATION_AWBS:
        return "CAL_OPERATION_AWBS";
    case CAL_OPERATION_FM_RX_OVER_SCO:
        return "CAL_OPERATION_FM_RX_OVER_SCO";
    case CAL_OPERATION_FM_RX_OVER_A3DP:
        return "CAL_OPERATION_FM_RX_OVER_A3DP";
    default:
        return "UNKNOWN";
    }

    return "UNKNOWN";
}



/*---------------------------------------------------------------------------
 *            ResourceToString()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Convert enum Resource to string
 *
 */

MCP_STATIC const char *ResourceToString(ECAL_Resource eResource)
{
    switch (eResource)
    {
    case CAL_RESOURCE_I2SH:
        return "CAL_RESOURCE_I2SH";
    case CAL_RESOURCE_PCMH:
        return "CAL_RESOURCE_PCMH";
    case CAL_RESOURCE_PCMT_1:
        return "CAL_RESOURCE_PCMT_1";
    case CAL_RESOURCE_PCMT_2:
        return "CAL_RESOURCE_PCMT_2";
    case CAL_RESOURCE_PCMT_3:
        return "CAL_RESOURCE_PCMT_3";    
    case CAL_RESOURCE_PCMT_4:
        return "CAL_RESOURCE_PCMT_4";    
    case CAL_RESOURCE_PCMT_5:
        return "CAL_RESOURCE_PCMT_5";
    case CAL_RESOURCE_PCMT_6:
        return "CAL_RESOURCE_PCMT_6";    
    case CAL_RESOURCE_PCMIF:
        return "CAL_RESOURCE_PCMIF";    
    case CAL_RESOURCE_FMIF:
        return "CAL_RESOURCE_FMIF";    
    case CAL_RESOURCE_FM_ANALOG:
        return "CAL_RESOURCE_FM_ANALOG";    
    case CAL_RESOURCE_CORTEX:
        return "CAL_RESOURCE_CORTEX";    
    case CAL_RESOURCE_FM_CORE:
        return "CAL_RESOURCE_FM_CORE";   
    default:
        return "UNKNOWN";
    }

    return "UNKNOWN";
}


MCP_STATIC const char *IFStatusToString(BtHciIfStatus status)
{
    switch (status)
    {
    case BT_HCI_IF_STATUS_SUCCESS:
        return "BT_HCI_IF_STATUS_SUCCESS";
    case BT_HCI_IF_STATUS_FAILED:
        return "BT_HCI_IF_STATUS_FAILED";
    case BT_HCI_IF_STATUS_PENDING:
        return "BT_HCI_IF_STATUS_PENDING";
    case BT_HCI_IF_STATUS_INTERNAL_ERROR:
        return "BT_HCI_IF_STATUS_INTERNAL_ERROR";
    case BT_HCI_IF_STATUS_IMPROPER_STATE:
        return "BT_HCI_IF_STATUS_IMPROPER_STATE";
    default:
        return "UNKNOWN";
    }

    return "UNKNOWN";

}


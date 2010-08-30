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


/******************************************************************************\
*
*   FILE NAME:      fm_drv_if.c
*
*   DESCRIPTION:	Implements platform independent part of the Shared Transport
*                   (in Kernel for Linux, in simulation layer for WinXP).
*
*   AUTHOR:         Malovany Ram
*
\******************************************************************************/

/******************************************************************************
 *
 * Include files
 *
 ******************************************************************************/
#include "fm_drv_if.h"
#include "fmc_log.h"
#include "fmc_defs.h"
#include "fmc_fw_defs.h"
#include "mcp_hal_hci.h"


FMC_LOG_SET_MODULE(FMC_LOG_MODULE_FMDRVIF);


/*******************************************************************************
 *
 * Types
 *
 ******************************************************************************/


/* Macros for packet conversion */
#define	HCI_VS_FM_PWR_OPCODE		0xFD37
#define	HCI_VS_READ_OPCODE			0xFD33
#define	HCI_VS_WRITE_OPCODE			0xFD35


#define CHAN8_FM_PWR_OPCODE         0xFE
#define CHAN8_FM_INTERRUPT          0xFF
#define CHAN8_FM_INTERRUPT_OPCODE   0xF0


/* FM specific Positions at the supply buffer (channel 8 header not included) */
#define FM_OPCODE_POS	            0
#define FM_PARAM_LEN_POS            1
#define FM_PARAM_POS                3
#define FM_POWER_PARAM_POS          0

/*Fm power values */
#define FM_POWER_UP                 1
#define FM_POWER_DOWN               0


/* HCi max packet len */
#define MAX_HCI_PACKET_LEN 255

/* Bit Mask Packet Type for channel filtering */
#define FM_DRV_IF_PKT_TYPE_FM		0x00000100

/*************************Channel 8 Data Command\Event\General***************************/
/* Packet types */
#define CHAN8_PKT_TYPE              0x08

/* FM Channel-8 command positions */
#define CHAN8_TYPE_POS              0
#define CHAN8_CMD_LEN_POS           1
#define CHAN8_FM_OPCODE_POS         2
#define CHAN8_RW_POS                3
#define CHAN8_PARAM_LEN_POS         4
#define CHAN8_PARAM_POS             5

/* Channel-8 event positions */
#define CHAN8_EVEN_CMD_TYPE_POS		0
#define CHAN8_EVEN_CMD_LEN_POS		1
#define CHAN8_EVEN_STATUS_POS		2
#define CHAN8_EVEN_NUM_HCI_POS		3
#define CHAN8_EVEN_FM_OPCODE_POS	4
#define CHAN8_EVEN_RW_POS		    5
#define CHAN8_EVEN_PARAM_LEN_POS	6
#define CHAN8_EVEN_PARAM_POS		7

/*Channel 8 Power command size */
#define CHAN8_PWR_COMMAND_LEN		7

/* Channel-8 data sizes */
#define CHAN8_OPCODE_SIZE 	        1
#define CHAN8_RD_WR_SIZE 	        1
#define CHAN8_PARAM_LEN_SIZE 	    1

/*Channel 8 Write\Read values */
#define CHAN8_RD_CMD	            0x01
#define CHAN8_WR_CMD	            0x00

/*Channel 8 Status values */
#define CHAN8_STATUS_SUCCESS	    0x00


/******************************************************************************/


/* fm if drv name */
#define TI_FM_CHD_DRV_NAME	        "/dev/tifm"


/* HAL HCI Handle */
static handle_t         hHalHci;

/* Write CB Params */
static handle_t 		hComCompCbParam;

/* Channel 8 command type struct */
typedef struct 
{
	 McpU8			    *pCmdParms;    /* Pointer to the Channel 8 command packet buf */
	 McpU16				uCmdParmsLen;  /* Channel 8 command len */ 
} Channel8CommandData;


/* Callback functions for FM interface */
FMDrvIfCallBacks TfmDrvIfCallBacks;

#define UNUSED_PARAMETER(_PARM)     ((_PARM) = (_PARM))

/*******************************************************************************
 *
 * Function declarations
 *
 ******************************************************************************/

static void fmDrvIfRecvCallback(handle_t hHandleCb,
                                McpU8 *pkt,
                                McpU32 len,
                                eHalHciStatus status);
static void create_channel_8_packet(const McpU16 uHciOpcode,
									const McpU8 *pCmdParms,
									Channel8CommandData *channel8CommandData);


/*******************************************************************************
 *
 * Function definitions
 *
 ******************************************************************************/
/*------------------------------------------------------------------------------
 * FMDrvIf_TransportOn() 
 *
 *     
 */

/* Open FM character device */

EMcpfRes FMDrvIf_TransportOn(handle_t hMcpf,
                             const FM_DRV_InterputCb InterruptCb,
                             FM_DRV_CmdComplCb CmdCompleteCb)
{
    EMcpfRes status = RES_OK;

    FMC_FUNC_START("FMDrvIf_TransportOn");
	 
    FMC_LOG_INFO(("FMDrvIf_TransportOn: Enter"));
    
    FMC_VERIFY_ERR(((InterruptCb != NULL) && (CmdCompleteCb != NULL)),
                   RES_ERROR,
                   ("FMDrvIf_TransportOn Failed, wrong Parms"));

    /* Register callbacks */
	TfmDrvIfCallBacks.fInteruptCalllback = InterruptCb;
	TfmDrvIfCallBacks.fCmdCompCallback = CmdCompleteCb;

    hHalHci = hal_hci_OpenSocket(hMcpf,
                                 0, 
                                 FM_DRV_IF_PKT_TYPE_FM,
                                 NULL,
                                 fmDrvIfRecvCallback,
                                 NULL);

    FMC_VERIFY_ERR((hHalHci != NULL),
                   RES_ERROR,
                   ("FMDrvIf_TransportOn Failed, hal_hci_OpenSocket  returned NULL"));

	FMC_FUNC_END();

    return status;
}

/*------------------------------------------------------------------------------
 * FMDrvIf_TransportOff() 
 *
 *     
 */

/* Open FM character device */

EMcpfRes FMDrvIf_TransportOff(void)
{
    EMcpfRes status = RES_OK;

    FMC_FUNC_START("FMDrvIf_TransportOff");	

    FMC_LOG_INFO(("FMDrvIf_TransportOff: Enter"));

    hal_hci_CloseSocket(hHalHci);
     
	/* Nullify the HAL HCI handle */
	hHalHci = NULL;

	FMC_FUNC_END();

    return status;	
}

/*------------------------------------------------------------------------------
 * FmDrvIf_Write() 
 *
 *     
 */
EMcpfRes FmDrvIf_Write(const McpU16 uHciOpcode,
				       const McpU8 *pHciCmdParms,
				       const McpU16 uHciCmdParmsLen,		
				       const handle_t hCbParam)
{   
	McpU16 uSentLen;
	EMcpfRes status = RES_OK;
    eHalHciStatus halHciStatus;
	Channel8CommandData channel8CommandData;
	McpU8 *hciFrag[1];
       McpU32 hciFragLen[1];

    FMC_FUNC_START("FmDrvIf_Write");	

    /* Verify user CB parms */
    FMC_VERIFY_ERR((hCbParam != NULL),
                   RES_ERROR,
                   ("FmDrvIf_Write: hCbParam == NULL"));
	
	hComCompCbParam = hCbParam;

	create_channel_8_packet(uHciOpcode, pHciCmdParms, &channel8CommandData);

	/* Packet Type will be added in HAL HCI */
	hciFrag[0] = &channel8CommandData.pCmdParms[1];
	hciFragLen[0] = channel8CommandData.uCmdParmsLen-1;
	
    halHciStatus = hal_hci_SendPacket(hHalHci,
                                      CHAN8_PKT_TYPE,
                                      1,
                                      hciFrag,
                                      hciFragLen);

    FMC_VERIFY_ERR((HAL_HCI_STATUS_PENDING == halHciStatus),
                   RES_ERROR,
                   ("FmDrvIf_Write: hal_hci_SendPacket fail returned %d", halHciStatus));

	FMC_FUNC_END();

    return status;
}

void fmDrvIfRecvCallback(handle_t hHandleCb,
                         McpU8 *pkt,
                         McpU32 len,
                         eHalHciStatus status)
{
	TCmdComplEvent cmdCompEvent;

	FMC_FUNC_START("fmDrvIfRecvCallback");	
	
	UNUSED_PARAMETER(hHandleCb);

    FMC_VERIFY_ERR_NO_RETVAR((HAL_HCI_STATUS_SUCCESS == status),
                             ("fmDrvIfRecvCallback: status %d", status));

    /* FM DRV got one packet */

    /* According to the FM Opcode will be calling to the appropriate Callback */						
    if(pkt[CHAN8_EVEN_FM_OPCODE_POS] == CHAN8_FM_INTERRUPT)
    {
        TfmDrvIfCallBacks.fInteruptCalllback();					
    }
    else
    {
        /* Get the Status Value */
        if(pkt[CHAN8_EVEN_STATUS_POS] == CHAN8_STATUS_SUCCESS)
        {
            cmdCompEvent.eResult = RES_OK;
        }
        else
        {
            cmdCompEvent.eResult = RES_ERROR;
        }
        
        /* Get the Param Data Length */
        cmdCompEvent.uEvtParamLen = pkt[CHAN8_EVEN_PARAM_LEN_POS];
        
        /* Get the Param Data Pointer */
        cmdCompEvent.pEvtParams = &pkt[CHAN8_EVEN_PARAM_POS];
        
        TfmDrvIfCallBacks.fCmdCompCallback(hComCompCbParam,&cmdCompEvent);
    }

	FMC_FUNC_END();	
}

/* Create FM channel 8 packet */
 void create_channel_8_packet(const McpU16 uHciOpcode,
                              const McpU8 *pCmdParms,
                              Channel8CommandData *channel8CommandData)
{
	McpU8 chan8_index, fm_index;
	McpU8 params_len,fmPowerValue;
    int idx;
	
	FMC_FUNC_START("create_channel_8_packet");

	/* Set the Channel 8Command pointer to the start value */
	channel8CommandData->pCmdParms=pCmdParms-CHAN8_COMMAND_HDR_LEN;    

	/* We need to verify if it is a Power command since the 8 channel command
     * creation should be different */
	if (uHciOpcode == HCI_VS_FM_PWR_OPCODE)
	{
        channel8CommandData->uCmdParmsLen = CHAN8_PWR_COMMAND_LEN;
        fmPowerValue = pCmdParms[FM_POWER_PARAM_POS];
        channel8CommandData->pCmdParms[CHAN8_TYPE_POS] = CHAN8_PKT_TYPE;
        channel8CommandData->pCmdParms[CHAN8_CMD_LEN_POS] = 5;
        channel8CommandData->pCmdParms[CHAN8_FM_OPCODE_POS] = CHAN8_FM_PWR_OPCODE;
        channel8CommandData->pCmdParms[CHAN8_RW_POS] = CHAN8_WR_CMD;
        channel8CommandData->pCmdParms[CHAN8_PARAM_LEN_POS] = 2;
        channel8CommandData->pCmdParms[CHAN8_PARAM_POS] = 0;
        
		if(fmPowerValue==FM_POWER_UP)
		{
			channel8CommandData->pCmdParms[CHAN8_PARAM_POS+1] = FM_POWER_UP;
		}
		else
		{
			channel8CommandData->pCmdParms[CHAN8_PARAM_POS+1] = FM_POWER_DOWN;
		}
		return ;
	}

	params_len=pCmdParms[FM_PARAM_LEN_POS];	

	/* Byte 0 - Logical channel 8 */
	channel8CommandData->pCmdParms[CHAN8_TYPE_POS]=CHAN8_PKT_TYPE;
	
	/* Byte 2 - FM opcode */
	channel8CommandData->pCmdParms[CHAN8_FM_OPCODE_POS]=pCmdParms[FM_OPCODE_POS];

	/* Byte 3 - Get and set the Read\Write value */
	channel8CommandData->pCmdParms[CHAN8_RW_POS]=(uHciOpcode == HCI_VS_READ_OPCODE) ? CHAN8_RD_CMD : CHAN8_WR_CMD;

	/* READ command except read_hw_register  since read_hw_register has data in it */
	if ((channel8CommandData->pCmdParms[CHAN8_RW_POS] == CHAN8_RD_CMD) &&
	    (channel8CommandData->pCmdParms[CHAN8_FM_OPCODE_POS] != FMC_FW_OPCODE_CMN_HARDWARE_REG_SET_GET)) {


		/* Byte 4 - Length of opcode data to read or write */
		channel8CommandData->pCmdParms[CHAN8_PARAM_LEN_POS] = params_len;
        
		/* Byte 1 - Number of bytes following */
		channel8CommandData->pCmdParms[CHAN8_CMD_LEN_POS] = CHAN8_OPCODE_SIZE +
		    CHAN8_RD_WR_SIZE + CHAN8_PARAM_LEN_SIZE;		
	} else {

		/* Byte 4 - Length of opcode data to read or write */
		channel8CommandData->pCmdParms[CHAN8_PARAM_LEN_POS] = params_len;

		/* Byte 1 - Number of bytes following */
		channel8CommandData->pCmdParms[CHAN8_CMD_LEN_POS] = params_len + CHAN8_OPCODE_SIZE +
		    CHAN8_RD_WR_SIZE + CHAN8_PARAM_LEN_SIZE;		

		chan8_index = CHAN8_PARAM_POS;
		fm_index = FM_PARAM_POS;

		/* Need to copy parms to the relevant place in Channel 8 header */
		while (params_len) {
			channel8CommandData->pCmdParms[chan8_index] = pCmdParms[fm_index];
			chan8_index++;
			fm_index++;
			params_len--;
		}
	}

    /* Set the all command length according to the Read command values  the 
        1 is for the command type 0x08
        1 is for the Total length of following bytes packet */
		channel8CommandData->uCmdParmsLen = channel8CommandData->pCmdParms[CHAN8_CMD_LEN_POS] + 2;				

	FMC_FUNC_END();	
}



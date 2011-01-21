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


#include "mcpf_defs.h"
#include "fmc_types.h"

#include "fmc_config.h"
#include "fmc_defs.h"
#include "fmc_core.h"
#include "fmc_log.h"
#include "ccm_im.h"
#include "fmc_utils.h"
#include "fmc_os.h"
#include "fm_drv_if.h"

#include "BusDrv.h"
#include "TxnDefs.h"
#include "mcp_hal_defs.h"
#include "mcp_IfSlpMng.h"
#include "mcp_transport.h"
#include "mcpf_mem.h"
#include "mcpf_services.h"
#include "mcp_hciDefs.h"
#include "mcp_txnpool.h"
#include "mcpf_report.h"
#include "mcpf_main.h"
#ifdef BLUEZ_SOLUTION
#include "fm_chrlib.h"
#endif

FMC_LOG_SET_MODULE(FMC_LOG_MODULE_FMCORE);

#ifndef MCP_STK_ENABLE
#define Report1(s) (MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, mcpHalLogModuleId, s))
#endif

/* [ToDo] Make sure there is a single process at a time handled by the transport layer */


#if FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED

typedef void (*_FmcTransportCommandCompleteCb)( FmcStatus   status,
                                                FMC_U8      *data,
                                                FMC_UINT    dataLen);

typedef struct {
    FmcCoreEventCb      clientCb;

    /*
        cmdParms stores a local copy of the command parameters. The parameters are copied
        inside the transport function, to avoid caller's and transport's memory management coupling.
        
        The command parms must be stored in a valid location until the command is acknowledged.
    */


#ifdef MCP_STK_ENABLE
   FMC_U8                  cmdBuff[FMC_CORE_MAX_PARMS_LEN+CHAN8_COMMAND_HDR_LEN];   
   FMC_U8                 *cmdParms;
#else
    FMC_U8                  cmdParms[FMC_CORE_MAX_PARMS_LEN];
#endif

    FMC_UINT                parmsLen;

    /* 
        holds client cb event data that will be sent in response to the current process that is handled
        by the transport layer
    */
    FmcCoreEvent        event;

    FMC_BOOL            fmInterruptRecieved;

    CcmObj                  *ccmObj;
    CCM_IM_StackHandle      ccmImStackHandle;
    handle_t               hMcpf;
 /* Handle to CCM Adapter */
    handle_t                        ccmaObj;
} _FmcCoreData;

static _FmcCoreData _fmcTransportData;

FMC_STATIC void _FMC_CORE_CcmImCallback(CcmImEvent *event);

FMC_STATIC FmcStatus _FMC_CORE_SendAnyWriteCommand( FmcFwOpcode fmOpcode,
                                                                        FMC_U8*     fmCmdParms,
                                                                        FMC_UINT    len);

FMC_STATIC void _FMC_CORE_CmdCompleteCb(    FmcStatus   status,
                                                        FMC_U8      *data,
                                                        FMC_UINT    dataLen);

FMC_STATIC void _FMC_CORE_InterruptCb(const FMC_U8 EventCode, 
                                                    const FMC_U32 ParamsTotalLength, 
                                                    const FMC_U8* Params);

#ifdef MCP_STK_ENABLE
FMC_STATIC void _FM_DRV_IF_InterruptCb(void);
#endif

FMC_STATIC FmcStatus _FMC_CORE_HCI_RegisterUnregisterGeneralEvent(FMC_BOOL registerInt);

FMC_STATIC void _FMC_CORE_interruptCb(  FmcStatus   status,                                                    
                                                    FmcCoreEventType type);

#ifdef MCP_STK_ENABLE
FMC_STATIC void _FMC_CORE_HCI_CmdCompleteCb(handle_t hHandle, TCmdComplEvent *pEvent);
#endif

CCM_IM_StackHandle _FMC_CORE_GetCcmImStackHandle(void);


FmcStatus FMC_CORE_Init(handle_t hMcpf)
{
    FmcStatus       status = FMC_STATUS_SUCCESS;
    CcmImStatus       ccmImStatus;
    CcmImObj        *ccmImObj;
    CcmStatus       ccmStatus;

    FMC_FUNC_START("FMC_CORE_Init");

    /* [ToDo] - Protect against mutliple initializations / handle them correctly if allowed*/
    _fmcTransportData.clientCb = NULL;

#ifdef MCP_STK_ENABLE
    _fmcTransportData.hMcpf = mcpf_create(NULL, NULL);
    /*Initialize the parms pointer  */
    _fmcTransportData.cmdParms=_fmcTransportData.cmdBuff +CHAN8_COMMAND_HDR_LEN;

/* Init the CCM Module */
    ccmStatus = CCM_StaticInit();
    FMC_VERIFY_FATAL_NO_RETVAR((status == FMC_STATUS_SUCCESS), ("CCM_StaticInit failed, ccmStatus %d", ccmStatus));    

    ccmStatus = CCM_Create(_fmcTransportData.hMcpf, MCP_HAL_CHIP_ID_0,(handle_t *)& _fmcTransportData.ccmObj);
    FMC_VERIFY_FATAL_NO_RETVAR((status == FMC_STATUS_SUCCESS), ("CCM_Create failed, ccmStatus %d", ccmStatus));    

    _fmcTransportData.ccmaObj = *CCM_GetCcmaObj( _fmcTransportData.ccmObj);
/******************************************************************************************/
#else
    _fmcTransportData.hMcpf = hMcpf;
    _fmcTransportData.ccmObj = MCPF_GET_CCM_OBJ(_fmcTransportData.hMcpf);
    ccmImObj = CCM_GetIm(_fmcTransportData.ccmObj);
    FMC_VERIFY_FATAL_NO_RETVAR((ccmImObj != NULL),  ("CCM_GetIm Returned a Null CCM IM Obj"));
    /* 
        The transport layer is the layer that interacts with the CCM Init Manager on behalf of the FM stack.
        Therefore, the transport layer registers an internal callback to receive CCM IM 
        notifications and process them.
    */  
    ccmImStatus = CCM_IM_RegisterStack(ccmImObj, CCM_IM_STACK_ID_FM, _FMC_CORE_CcmImCallback, &_fmcTransportData.ccmImStackHandle);
    FMC_VERIFY_FATAL_NO_RETVAR((ccmImStatus == CCM_IM_STATUS_SUCCESS),  ("CCM_IM_RegisterStack Failed (%d)", ccmImStatus));

    _fmcTransportData.ccmaObj = CCM_GetCcmaObj(MCPF_GET_CCM_OBJ(_fmcTransportData.hMcpf));
#endif

    FMC_FUNC_END();
    
    return status;
}

FmcStatus FMC_CORE_Deinit(void)
{
    FmcStatus       status = FMC_STATUS_SUCCESS;
    CcmImStatus       ccmImStatus;

    FMC_FUNC_START("FMC_CORE_Deinit");
#ifdef MCP_STK_ENABLE
     /* Destroy CCM */
    CCM_Destroy((handle_t *)&_fmcTransportData.ccmObj);
    mcpf_destroy(_fmcTransportData.hMcpf);
 #else  
    ccmImStatus = CCM_IM_DeregisterStack(&_fmcTransportData.ccmImStackHandle);
    FMC_VERIFY_FATAL_NO_RETVAR((ccmImStatus == CCM_IM_STATUS_SUCCESS),  ("CCM_IM_DeregisterStack Failed (%d)", ccmImStatus));
#endif

    FMC_FUNC_END();
    
    return status;
}
FmcStatus FMC_CORE_SetCallback(FmcCoreEventCb eventCb)
{
	FmcStatus status;

	FMC_FUNC_START("FMC_CORE_SetCallback");
	
	/* Verify that only a single client at a time requests notifications */
	if(eventCb != NULL)
	{
		status = FMC_CORE_RegisterUnregisterIntCallback(FMC_TRUE);
	   
		FMC_VERIFY_FATAL((_fmcTransportData.clientCb == NULL), FMC_STATUS_INTERNAL_ERROR,
			("FMC_CORE_SetCallback: Callback already set"));

	}
	else
	{
   
		status = FMC_CORE_RegisterUnregisterIntCallback(FMC_FALSE);
	}
	_fmcTransportData.clientCb = eventCb;

	status = FMC_STATUS_SUCCESS;
	
	FMC_FUNC_END();
	
	return status;
} 


FmcStatus FMC_CORE_TransportOff(void)
{
	FmcStatus       status = FMC_STATUS_SUCCESS;
	EMcpfRes       stIfStatus = RES_OK;
	CcmImStatus     ccmImStatus;

	FMC_FUNC_START("FMC_CORE_TransportOff");

	FMC_LOG_INFO(("FMC_CORE_TransportOff: Calling TI_CHIP_MNGR_FMOff"));

#ifdef BLUEZ_SOLUTION
	/* remove unused vars warnings, since we ignore these status-es */
	(void) stIfStatus;
	(void) ccmImStatus;
	status = fm_close_dev(0);
#endif

#if defined(MCP_STK_ENABLE) && !defined(BLUEZ_SOLUTION)


	stIfStatus=CCMA_TransportOff(_fmcTransportData.ccmaObj);
 if (stIfStatus != RES_OK) 
        {
        FMC_FATAL(FMC_STATUS_INTERNAL_ERROR, ("FMC_CORE_TransportOn: CCMA_TransportOff Failed (%d)", stIfStatus));      
        }   

 stIfStatus = FMDrvIf_TransportOff();
     if (stIfStatus != RES_OK) 
        {
        FMC_FATAL(FMC_STATUS_INTERNAL_ERROR, ("FMC_CORE_TransportOn: FMDrvIf_TransportOff Failed (%d)", stIfStatus));      
        }   
    else
        {
            status = FMC_STATUS_SUCCESS;
        }

#elif !defined(BLUEZ_SOLUTION)
  ccmImStatus = CCM_IM_StackOff(_fmcTransportData.ccmImStackHandle);

    if (ccmImStatus == CCM_IM_STATUS_SUCCESS) 
    {
        FMC_LOG_INFO(("FMC_CORE_TransportOff: CCM IM FM Off Completed Successfully (Immediately)"));
        status = FMC_STATUS_SUCCESS;
    }
    else if (ccmImStatus == CCM_IM_STATUS_PENDING)
    {
        FMC_LOG_INFO(("FMC_CORE_TransportOff: Waiting for CCM IM FM Off To Complete"));
        status = FMC_STATUS_PENDING;
    }
    else
    {
        FMC_FATAL(FMC_STATUS_INTERNAL_ERROR, ("FMC_CORE_TransportOff: CCM_IM_FmOff Failed (%d)", ccmImStatus ));
    }

#endif
    FMC_FUNC_END();

    return status;
}

FmcStatus FMC_CORE_RegisterUnregisterIntCallback(FMC_BOOL reg)
{
    FmcStatus       status = FMC_STATUS_SUCCESS;

    FMC_FUNC_START("FMC_CORE_RegisterUnregisterIntCallback");
    

    status = _FMC_CORE_HCI_RegisterUnregisterGeneralEvent(reg);     

    _fmcTransportData.fmInterruptRecieved = FMC_FALSE;
    
    FMC_FUNC_END();

    return status;
}

/*
    This function is called whenever a command that was processed by the transport layer completes.
    It is called regardless of the transport used to send the command (HCI / I2C).

    The function initializes the fields of the transport event and sends it to the registered client callback
*/
void _FMC_CORE_CmdCompleteCb(   FmcStatus   status,
                                        FMC_U8      *data,
                                        FMC_UINT    dataLen)
{
    /* _fmcTransportData.event.type was set when the command was originally received */
    
    _fmcTransportData.event.status = status;

    /* the data is not copied. The client's callback must copy the data if it wishes to access it afterwards */
    _fmcTransportData.event.data = data;
    _fmcTransportData.event.dataLen = dataLen;
    _fmcTransportData.event.fmInter = FMC_FALSE;

    (_fmcTransportData.clientCb)(&_fmcTransportData.event);

}

void _FMC_CORE_interruptCb( FmcStatus   status,
                                    FmcCoreEventType type)
{
    /* _fmcTransportData.event.type was set when the command was originally received */
    FMC_UNUSED_PARAMETER(type); 
    _fmcTransportData.event.status = status;

    _fmcTransportData.event.fmInter = FMC_TRUE;

    (_fmcTransportData.clientCb)(&_fmcTransportData.event);

}
void _FMC_CORE_CcmImCallback(CcmImEvent *event)
{
    FMC_FUNC_START("_FMC_CORE_CcmImCallback");
    
    FMC_LOG_INFO(("Stack: %d, Event: %d", event->stackId, event->type));

    FMC_VERIFY_FATAL_NO_RETVAR((event->stackId == CCM_IM_STACK_ID_FM), 
                                    ("Received an event for another stack (%d)", event->stackId));

    switch (event->type)
    {
        case CCM_IM_EVENT_TYPE_ON_COMPLETE:

            _fmcTransportData.event.type = FMC_CORE_EVENT_TRANSPORT_ON_COMPLETE;
            _fmcTransportData.event.data = NULL;
            _fmcTransportData.event.dataLen = 0;
            
            (_fmcTransportData.clientCb)(&_fmcTransportData.event);
            
            break;

        case CCM_IM_EVENT_TYPE_OFF_COMPLETE:

            _fmcTransportData.event.type = FMC_CORE_EVENT_TRANSPORT_OFF_COMPLETE;
            _fmcTransportData.event.data = NULL;
            _fmcTransportData.event.dataLen = 0;
            
            (_fmcTransportData.clientCb)(&_fmcTransportData.event);
            
            break;

        default:
            FMC_FATAL_NO_RETVAR(("_FMC_CORE_CcmImCallback: Invalid CCM IM Event (%d)", event->type));           
    };

    FMC_FUNC_END();
}
CCM_IM_StackHandle _FMC_CORE_GetCcmImStackHandle(void)
{
	return _fmcTransportData.ccmImStackHandle;
}
CcmObj* _FMC_CORE_GetCcmObjStackHandle(void)
{       
	return _fmcTransportData.ccmObj;
}

/*************************************************************************************************
                HCI-Specific implementation of transport API  - 
                This Implementation should be changed when working over I2C.
*************************************************************************************************/
/*  Several clients can use the FM Transport. 
    Currently the VAC is the only module (through the sequencer) that uses the FM transport.*/
typedef enum{
    _FMC_CORE_TRANSPORT_CLIENT_FM,
    _FMC_CORE_TRANSPORT_CLIENT_VAC,
    _FMC_CORE_TRANSPORT_MAX_NUM_OF_CLIENTS
}_FmcCoreTransportClients;

/* Vendor Specific Opcodes for the various FM-related commands over HCI */
typedef enum{
	_FMC_CMD_I2C_FM_READ = 0x0133,
	_FMC_CMD_I2C_FM_WRITE = 0x0135,
	_FMC_CMD_FM_POWER_MODE = 0x0137,
	_FMC_CMD_FM_SET_AUDIO_PATH = 0x0139,
	_FMC_CMD_FM_CHANGE_I2C_ADDR = 0x013A,
	_FMC_CMD_FM_I2C_FM_READ_HW_REG = 0x0134,
}_FmcCoreTransportFmCOmmands;
/* 
    Format of an HCI WRITE command to FM over I2C is:
    
    HCI Header:
    ----------
    - HCI Packet Type (Added internally by the HCI Transport Layer)
    
    - HCI Opcode :                                          2 bytes (LSB, MSB - LE)
    - HCI Parameters Total Len (total length of all subsequent fields):     1 byte

    HCI Parameters:
    --------------
    - FM Opcode :                                   1 byte
    - FM Parameters Len:                                2 bytes (LSB, MSB - LE)
    - FM Cmd Parameter Value:                           N bytes 
*/

/* Length of 2nd field of HCI Parameters (described above) */
#define FMC_CORE_HCI_FM_PARMS_LEN_FIELD_LEN         (2)

/* Total length of HCI Parameters section */
#define FMC_CORE_HCI_WRITE_FM_TOTAL_PARMS_LEN(fmParmsLen)       \
        (sizeof(FmcFwOpcode) + FMC_CORE_HCI_FM_PARMS_LEN_FIELD_LEN + fmParmsLen)

/* 
    Format of an HCI READ command to FM over I2C is:
    
    HCI Header:
    ----------
    - HCI Packet Type (Added internally by the HCI Transport Layer)
    
    - HCI Opcode :                                          2 bytes (LSB, MSB - LE)
    - HCI Parameters Total Len (total length of all subsequent fields):     1 byte

    HCI Parameters:
    --------------
    - FM Opcode :                                           1 byte
    - Len of FM Parameters to read:                             2 bytes (LSB, MSB - LE)
*/

/* Length of 2nd field of HCI Parameters (described above) */
#define FMC_CORE_HCI_LEN_OF_FM_PARMS_TO_READ_FIELD_LEN          (2)

/* Total length of HCI Parameters section */
#define FMC_CORE_HCI_READ_FM_TOTAL_PARMS_LEN        \
        (sizeof(FmcFwOpcode) + FMC_CORE_HCI_LEN_OF_FM_PARMS_TO_READ_FIELD_LEN)


FMC_STATIC FmcStatus _FMC_CORE_HCI_SendFmCommand(   
                    _FmcCoreTransportClients                                clientHandle,
                    _FmcTransportCommandCompleteCb      cmdCompleteCb,
                    FMC_U16                             hciOpcode, 
                    FMC_U8                              *hciCmdParms,
                    FMC_UINT                            parmsLen);



FmcStatus FMC_CORE_SendPowerModeCommand(FMC_BOOL powerOn)
{
    FmcStatus status;

    FMC_FUNC_START("FMC_CORE_SendPowerModeCommand");

    /* Convert the transport interface power on indication to the firmware power command value */
    if (powerOn == FMC_TRUE)
    {
        _fmcTransportData.cmdParms[0] =  FMC_FW_FM_CORE_POWER_UP;
    }
    else
    {
        _fmcTransportData.cmdParms[0] =  FMC_FW_FM_CORE_POWER_DOWN;
    }
    
    _fmcTransportData.parmsLen = 1;
    
    _fmcTransportData.event.type = FMC_CORE_EVENT_POWER_MODE_COMMAND_COMPLETE;
    
    status = _FMC_CORE_HCI_SendFmCommand(       _FMC_CORE_TRANSPORT_CLIENT_FM,
                                                    _FMC_CORE_CmdCompleteCb,
                                                    _FMC_CMD_FM_POWER_MODE,
                                                    _fmcTransportData.cmdParms,
                                                    (FMC_U8)_fmcTransportData.parmsLen);
    FMC_OS_Sleep(30);

    FMC_VERIFY_FATAL((status == FMC_STATUS_PENDING), status, 
                    ("FMC_CORE_SendWriteCommand: _FMC_CORE_HCI_SendFmCommand Failed (%d)", status));
    

    FMC_FUNC_END();

    return status;
}

FmcStatus FMC_CORE_SendWriteCommand(FmcFwOpcode fmOpcode, FMC_U16 fmCmdParms)
{
    FmcStatus   status;
    FMC_U8      fmCmdParmsBe[2];    /* Buffer to hols the 2 bytes of FM command parameters */
    
    FMC_FUNC_START("FMC_CORE_SendWriteCommand");
    
    /* Store the cmd parms in BE (always 2 bytes for a write command) */
    FMC_UTILS_StoreBE16(&fmCmdParmsBe[0], fmCmdParms);
    _fmcTransportData.event.type = FMC_CORE_EVENT_WRITE_COMPLETE;
    status = _FMC_CORE_SendAnyWriteCommand( 
                    fmOpcode, 
                    fmCmdParmsBe,
                    sizeof(fmCmdParms));
    FMC_VERIFY_ERR((status == FMC_STATUS_PENDING), status, ("FMC_CORE_SendWriteCommand"));
    

    FMC_FUNC_END();

    return status;
}
FmcStatus FMC_CORE_SendWriteRdsDataCommand(FmcFwOpcode fmOpcode, FMC_U8 *rdsData, FMC_UINT len)
{
    FmcStatus status;

    FMC_FUNC_START("FMC_CORE_SendWriteRdsDataCommand");
    _fmcTransportData.event.type = FMC_CORE_EVENT_WRITE_COMPLETE;
        
    status = _FMC_CORE_SendAnyWriteCommand( 
                    fmOpcode, 
                    rdsData,
                    len);
    FMC_VERIFY_ERR((status == FMC_STATUS_PENDING), status, ("FMC_CORE_SendWriteRdsDataCommand"));
        

    FMC_FUNC_END();

    return status;
}
FmcStatus FMC_CORE_SendReadCommand(FmcFwOpcode fmOpcode,FMC_U16 fmParameter)
{
    FmcStatus status;
    FMC_FUNC_START("FMC_CORE_SendReadCommand");

    /* 
    Format of an HCI READ command to FM over I2C is:
    
    HCI Header:
    ----------
    - HCI Packet Type (Added internally by the HCI Transport Layer)
    
    - HCI Opcode :                                          2 bytes (LSB, MSB - LE)
    - HCI Parameters Total Len (total length of all subsequent fields):     1 byte

    HCI Parameters:
    --------------
    - FM Opcode :                                           1 byte
    - Len of FM Parameters to read:                             2 bytes (LSB, MSB - LE)

        HCI Opcode & HCI Packet Len are arguments in the call to _FMC_CORE_HCI_SendFmCommand.
        FM Opcode & FM Size of Data to read are part of the HCI Command parameters.
    */

    /* Preapre the HCI Command Parameters buffer */
    
    /* Store the FM Opcode*/
    _fmcTransportData.cmdParms[0] = fmOpcode;

    /* Store the FM Parameters Len in LE - For a "simple" (non-RDS) read command it is always 2 bytes */
    FMC_UTILS_StoreLE16(&_fmcTransportData.cmdParms[1], fmParameter);

    _fmcTransportData.parmsLen = FMC_CORE_HCI_READ_FM_TOTAL_PARMS_LEN;
    _fmcTransportData.event.type = FMC_CORE_EVENT_READ_COMPLETE;
        
    status = _FMC_CORE_HCI_SendFmCommand(       _FMC_CORE_TRANSPORT_CLIENT_FM,
                                                    _FMC_CORE_CmdCompleteCb,
                                                    _FMC_CMD_I2C_FM_READ,
                                                    _fmcTransportData.cmdParms,
                                                    _fmcTransportData.parmsLen);
    FMC_VERIFY_FATAL((status == FMC_STATUS_PENDING), status, 
                    ("FMC_CORE_SendReadCommand: _FMC_CORE_HCI_SendFmCommand Failed (%d)", status));
    

    FMC_FUNC_END();

    return status;
}


FmcStatus FMC_CORE_SendHciScriptCommand(    FMC_U16     hciOpcode, 
                                                                FMC_U8      *hciCmdParms, 
                                                                FMC_UINT    len)
{
    FmcStatus status;

    FMC_FUNC_START("FMC_CORE_SendHciScriptCommand");
    
    FMC_VERIFY_ERR((len <= FMC_CORE_MAX_PARMS_LEN), FMC_STATUS_INVALID_PARM, 
                    ("FMC_CORE_SendHciScriptCommand: data len (%d) exceeds max len (%d)", len, FMC_CORE_MAX_PARMS_LEN));

    FMC_OS_MemCopy(_fmcTransportData.cmdParms, hciCmdParms, len);
    _fmcTransportData.parmsLen = len;
    _fmcTransportData.event.type = FMC_CORE_EVENT_SCRIPT_CMD_COMPLETE;
        
    status = _FMC_CORE_HCI_SendFmCommand(           _FMC_CORE_TRANSPORT_CLIENT_FM,
                                                        _FMC_CORE_CmdCompleteCb,
                                                        hciOpcode,
                                                        _fmcTransportData.cmdParms,
                                                        _fmcTransportData.parmsLen);

    FMC_VERIFY_ERR((status == FMC_STATUS_PENDING), status, 
                    ("FMC_CORE_SendHciScriptCommand: _FMC_CORE_HCI_SendFmCommand Failed (%d)", status));
    

    FMC_FUNC_END();

    return status;
}


FmcStatus _FMC_CORE_SendAnyWriteCommand(FmcFwOpcode fmOpcode,
                                                                FMC_U8*     fmCmdParms,
                                                                FMC_UINT    len)
{
    FmcStatus status;   

    FMC_FUNC_START("_FMC_CORE_SendAnyWriteCommand");

/*
    HCI Parameters:
    --------------
    - FM Opcode :                                   1 byte
    - FM Parameters Len:                                2 bytes (LSB, MSB - LE)
    - FM Cmd Parameters:                                len bytes (passed in the correct endiannity

    HCI Command Header fields (HCI Opcode & HCI Parameters Total Len) are arguments in the call to 
    _FMC_CORE_HCI_SendFmCommand.
*/

    /* Preapre the HCI Command Parameters buffer */
    
    /* Store the FM Opcode*/
    _fmcTransportData.cmdParms[0] = fmOpcode;

    /* Store the FM Parameters Len in LE */

    FMC_UTILS_StoreLE16(&_fmcTransportData.cmdParms[1], (FMC_U16)len);
    
    /* Copy the FM cmd parms (should be in the correct endiannity already) */
    FMC_OS_MemCopy(&_fmcTransportData.cmdParms[3], fmCmdParms, len);

    _fmcTransportData.parmsLen = FMC_CORE_HCI_WRITE_FM_TOTAL_PARMS_LEN(len);
    
    status = _FMC_CORE_HCI_SendFmCommand(       _FMC_CORE_TRANSPORT_CLIENT_FM,
                                                    _FMC_CORE_CmdCompleteCb,
                                                    _FMC_CMD_I2C_FM_WRITE,
                                                    _fmcTransportData.cmdParms,
                                                    _fmcTransportData.parmsLen);

    FMC_VERIFY_ERR((status == FMC_STATUS_PENDING), status, 
                    ("_FMC_CORE_SendAnyWriteCommand: _FMC_CORE_HCI_SendFmCommand Failed (%d)", status));
    
    FMC_FUNC_END();

    return status;
}

/*************************************************************************************************
                PORTING TO A PLATFORM SPECIFIC HCI TRANSPORT FROM HERE
*************************************************************************************************/
#include "mcp_hci_adapt.h"

/*Fm Transport for VAC operations*/
#include "fm_transport_if.h"
#include "ccm_adapt.h"

#define TRANS_CHAN_EVT (4)

#define HCI_COMMAND_COMPLETE_HEADER_LEN (4)


typedef struct 
{
     McpU16 			uHciOpcode;
	 McpU8			    *pHciCmdParms;
	 McpU16				uHciCmdParmsLen;
     void               *pHciUserData;
    
} _FmcHciCommandData;

typedef struct {
    _FmcHciCommandData                  commandData;
	CcmaClientCb                        sequencerCb;
	void* pUserData;
	_FmcTransportCommandCompleteCb		cmdCompleteCb;
} _FmcTransportHciData;

FMC_STATIC _FmcTransportHciData _fmcTransportHciData[_FMC_CORE_TRANSPORT_MAX_NUM_OF_CLIENTS];

FMC_STATIC EMcpfRes  FMA_SendCommand(_FmcHciCommandData *cmdData);
FMC_STATIC EMcpfRes  FMA_RxInd (handle_t hTrans, handle_t hndl, TTxnStruct *pTxn);

/*
	Callback for calls to FM transport done by the FM client handle.
*/
#ifndef MCP_STK_ENABLE
FMC_STATIC void _FMC_CORE_HCI_CmdCompleteCb(handle_t hHandle, THciaCmdComplEvent *pEvent);
#endif



/*
	Callback for calles to FM transport done by the VAC client handle.
*/
FMC_STATIC void _FM_IF_FmVacCmdCompleteCb( FmcStatus	status,
											FMC_U8		*data,
											FMC_UINT	dataLen);

	
/*
	This Function is used to register Callback for interrupts. Since FM Interrupts are recieved any command related we have 
	To capture the FM Interrupt in the transport layer and send it to this callback.
*/
FmcStatus _FMC_CORE_HCI_RegisterUnregisterGeneralEvent(FMC_BOOL registerInt)
{
    static FMC_BOOL unregistered = FMC_TRUE;

    if(registerInt&&unregistered)
    {
    	/*
		When working over HCI the FM Interrupt is generated throghu a Special HCI event HCE_FM_EVENT.
		This registartion function will verify that each time this event is recieved over the HCI the callback will be called.
	*/

#ifndef MCP_STK_ENABLE
       HCIA_RegisterClient ((const handle_t)(((Tmcpf *)_fmcTransportData.hMcpf)->hHcia), (const McpU8)TRANS_CHAN_EVT, (const McpU16)HCE_FM_EVENT, FMA_RxInd, _fmcTransportData.hMcpf, HCIA_REG_METHOD_NORMAL);
#endif
        unregistered = FMC_FALSE;
    }
    else if(!(registerInt||unregistered))
    {
#ifndef MCP_STK_ENABLE
        HCIA_UnregisterClient(((Tmcpf *)_fmcTransportData.hMcpf)->hHcia, TRANS_CHAN_EVT, HCE_FM_EVENT, HCIA_REG_METHOD_NORMAL);
#endif

        unregistered = FMC_TRUE;
    }

    return FMC_STATUS_SUCCESS;
}


FmcStatus FMC_CORE_TransportOn(void)
{
    FmcStatus      status = FMC_STATUS_SUCCESS;
    EMcpfRes       stStatus = RES_OK;
    CcmStatus      ccmStatus= CCM_STATUS_FAILED;
    CcmImStatus     ccmImStatus;

    FMC_FUNC_START("FMC_CORE_TransportOn");

#ifdef BLUEZ_SOLUTION
	/* remove unused vars warning, since we are ignoring these status-es*/
	(void) stStatus;
	(void) ccmStatus;
	(void) ccmImStatus;
    /* _FM_DRV_IF_InterruptCb func callback with which
       pthread_create needs to be called
     */
    status = fm_open_dev(0, _FM_DRV_IF_InterruptCb, _FMC_CORE_HCI_CmdCompleteCb,
		    &_fmcTransportHciData[_FMC_CORE_TRANSPORT_CLIENT_FM]);
    if (status <= 0) {
	    /* print error here */
    }
    status = FMC_STATUS_SUCCESS;
#endif


#if defined(MCP_STK_ENABLE) && !defined(BLUEZ_SOLUTION)

    stStatus = FMDrvIf_TransportOn(_fmcTransportData.hMcpf,_FM_DRV_IF_InterruptCb,_FMC_CORE_HCI_CmdCompleteCb);
    if (stStatus != RES_OK) 
    {
	    FMC_FATAL(FMC_STATUS_INTERNAL_ERROR, ("FMC_CORE_TransportOn: FMDrvIf_TransportOn Failed (%d)", stStatus));      
    }   

    stStatus =CCMA_TransportOn (_fmcTransportData.ccmaObj);     
    if (stStatus != RES_OK) 
    {
	    FMC_FATAL(FMC_STATUS_INTERNAL_ERROR, ("FMC_CORE_TransportOn: CCMA_TransportOn Failed (%d)", stStatus));      
    }   

    ccmStatus =CCM_Configure(_fmcTransportData.ccmObj);     
    if (ccmStatus != CCM_STATUS_SUCCESS) 
    {
	    FMC_FATAL(FMC_STATUS_INTERNAL_ERROR, ("FMC_CORE_TransportOn: CCM_Configure Failed (%d)", ccmStatus));      
    }   

    status = FMC_STATUS_SUCCESS;
#elif !defined(BLUEZ_SOLUTION)
    /* Notify chip manager that FM need to turn on */
    ccmImStatus = CCM_IM_StackOn(_fmcTransportData.ccmImStackHandle);

    if (ccmImStatus == CCM_IM_STATUS_SUCCESS) 
    {
	    FMC_LOG_INFO(("FMC_CORE_TransportOn: CCM IM FM On Completed Successfully (Immediately)"));
	    status = FMC_STATUS_SUCCESS;
    }
    else if (ccmImStatus == CCM_IM_STATUS_PENDING)
    {
	    FMC_LOG_INFO(("FMC_CORE_TransportOn: Waiting for CCM IM FM On To Complete"));
	    status = FMC_STATUS_PENDING;
    }
    else
    {
	    FMC_FATAL(FMC_STATUS_INTERNAL_ERROR, ("FMC_CORE_TransportOn: CCM_IM_FmOn Failed (%d)", ccmImStatus));
    }

#endif


    FMC_FUNC_END();

    return status;
}


#ifndef MCP_STK_ENABLE

FMC_STATIC EMcpfRes FMA_RxInd (handle_t hTrans, handle_t hndl, TTxnStruct *pTxn)
{
	EMcpfRes res = RES_OK;
    FMC_U8 EventCode;
    FMC_U32 ParamsTotalLength;
    FMC_U8* Params;

	MCPF_UNUSED_PARAMETER(hTrans);
	MCPF_UNUSED_PARAMETER(hndl);
    
    Report1(("FMA_RxInd %0x", pTxn->tHeader.tHciHeader.uPktType));

	/* Extract Payload length (SCO & EVT pckts have a 1 byte data len field, ACL have 2 bytes) */
	switch(pTxn->tHeader.tHciHeader.uPktType)
	{
    case HCI_PKT_TYPE_EVENT:

        EventCode = pTxn->pData[1];
		ParamsTotalLength 	= (McpU8)pTxn->pData[2];
        Params = &(pTxn->pData[3]);
		
       _FMC_CORE_InterruptCb(EventCode, ParamsTotalLength, Params) ;
			break;
    default:
        FMC_LOG_INFO(("FMA_RxInd unexpected pkt rcvd 0x%x", pTxn->tHeader.tHciHeader.uPktType));
        res = RES_ERROR;
        break;
    }
    /* release buffer and transaction*/
	mcpf_txnPool_FreeBuf(_fmcTransportData.hMcpf, (void *)pTxn);
    
    return res;
}

#endif

FMC_STATIC void _FMC_CORE_InterruptCb(const FMC_U8 EventCode, const FMC_U32 ParamsTotalLength, const FMC_U8* Params)
{
    FmcStatus   cmdCompleteStatus = FMC_STATUS_SUCCESS;
    FMC_U8      len = (FMC_U8)ParamsTotalLength; 
    FMC_U8 evtCode = EventCode;
    FMC_UNUSED_PARAMETER(evtCode);  
    FMC_UNUSED_PARAMETER(Params);   
    FMC_UNUSED_PARAMETER(len);
    /* Parse HCI event */
    _FMC_CORE_interruptCb(cmdCompleteStatus,FMC_CORE_EVENT_FM_INTERRUPT);

}


#ifdef MCP_STK_ENABLE
FMC_STATIC void _FM_DRV_IF_InterruptCb(void)
{    
    _FMC_CORE_interruptCb(FMC_STATUS_SUCCESS,FMC_CORE_EVENT_FM_INTERRUPT);
}
#endif
/*
    Send an any FM command over HCI (using HCIA)
*/
FmcStatus _FMC_CORE_HCI_SendFmCommand(  
                _FmcCoreTransportClients                clientHandle,
                _FmcTransportCommandCompleteCb      cmdCompleteCb,
                FMC_U16                             hciOpcode, 
                FMC_U8                              *hciCmdParms,
                FMC_UINT                            parmsLen)
{
    FmcStatus   status;
    EMcpfRes    comStatus;

    FMC_FUNC_START("_FMC_TRANSPORT_HCI_SendFmCommandOverHci");

    if(_fmcTransportHciData[clientHandle].cmdCompleteCb!=NULL)
    {
        FMC_FATAL(FMC_STATUS_INTERNAL_ERROR, 
                ("_FMC_TRANSPORT_HCI_SendFmCommandOverHci: Waiting for Command complete and tried to send command Failed (%d)", 
                FMC_STATUS_FAILED));
        
    }
    _fmcTransportHciData[clientHandle].cmdCompleteCb = cmdCompleteCb;

   _fmcTransportHciData[clientHandle].commandData.uHciOpcode = (FMC_U16)(hciOpcode | FMC_HCC_GROUP_SHIFT(FMC_HCC_GRP_VENDOR_SPECIFIC));
   _fmcTransportHciData[clientHandle].commandData.uHciCmdParmsLen = (FMC_U8)parmsLen;
   _fmcTransportHciData[clientHandle].commandData.pHciCmdParms = hciCmdParms;
   _fmcTransportHciData[clientHandle].commandData.pHciUserData = &_fmcTransportHciData[clientHandle];
                
    comStatus = FMA_SendCommand(&(_fmcTransportHciData[clientHandle].commandData));
            
        switch(comStatus)
             {
        case RES_OK:
            status = FMC_STATUS_PENDING;

        break;
        default:
            FMC_FATAL(FMC_STATUS_INTERNAL_ERROR, 
                        ("_FMC_TRANSPORT_HCI_SendFmCommandOverHci: Unexpected return value from FMA_SendCommand (%d)", comStatus));
    }
    
    FMC_FUNC_END();

    return status;
}

#ifdef MCP_STK_ENABLE
FMC_STATIC void _FMC_CORE_HCI_CmdCompleteCb(handle_t hHandle, TCmdComplEvent *pEvent)
{

    FmcStatus   cmdCompleteStatus;
    FMC_U8      len = 0; 
    FMC_U8      *data = NULL;

    _FmcTransportHciData *_fmcTransportHciDataInEvt = (_FmcTransportHciData*)(hHandle);

    _FmcTransportCommandCompleteCb tempCmdCompleteCb = _fmcTransportHciDataInEvt->cmdCompleteCb;

    _fmcTransportHciDataInEvt->cmdCompleteCb = NULL;
	len=pEvent->uEvtParamLen;
	data=pEvent->pEvtParams;

	if(pEvent->eResult==RES_OK)
		cmdCompleteStatus=FMC_STATUS_SUCCESS;
	else
		cmdCompleteStatus=FMC_STATUS_FAILED;
	
    /* Call trasnport-independent callback that will forward to the FM stack */
    (tempCmdCompleteCb)(cmdCompleteStatus, data, len);
}
    
#else
FMC_STATIC void _FMC_CORE_HCI_CmdCompleteCb(handle_t hHandle, THciaCmdComplEvent *pEvent)
{
    FmcStatus   cmdCompleteStatus;
    FMC_U8      len = 0; 
    FMC_U8      *data = NULL;

    _FmcTransportHciData *_fmcTransportHciDataInEvt = (_FmcTransportHciData*)(hHandle);

    _FmcTransportCommandCompleteCb tempCmdCompleteCb = _fmcTransportHciDataInEvt->cmdCompleteCb;

    _fmcTransportHciDataInEvt->cmdCompleteCb = NULL;

    FMC_VERIFY_FATAL_NORET ((tempCmdCompleteCb != NULL), ("HCIDataInEvt returned a Null cmdCompleteCb"));
		
    if (pEvent->eResult != RES_OK)
    {
        cmdCompleteStatus = FMC_STATUS_FM_COMMAND_FAILED;
    }
    else
    {

       if( pEvent->uEvtOpcode != HCE_COMMAND_COMPLETE)
        {
            cmdCompleteStatus = FMC_STATUS_FM_COMMAND_FAILED;
         /*   FMC_ERR_NO_RETVAR(("_FMC_CORE_HCI_CmdCompleteCb: Unexpected HCI Event Received (%d)", pEvent->uEvtOpcode));*/
        }
        else
        {
            len = (FMC_U8)(pEvent->uEvtParamLen - HCI_COMMAND_COMPLETE_HEADER_LEN); 
            data = (len == 0) ? NULL : &(pEvent->pEvtParams[HCI_COMMAND_COMPLETE_HEADER_LEN]);

            cmdCompleteStatus = FMC_STATUS_SUCCESS;
              
    /* Call trasnport-independent callback that will forward to the FM stack */
    (tempCmdCompleteCb)(cmdCompleteStatus, data, len);
}
    }
}

#endif

/*
*   FM_TRANSPORT_IF_SendFmVacCommand()
*
*   This API function was created for the of the VAC Module to sent FM Commands when configuring the 
*   FM IF. The reason we need this is since the FM can work over I2C (and not HCI). 
*   In this case the implementation of this function will be changed.
*
*/
CcmaStatus FM_TRANSPORT_IF_SendFmVacCommand( 
                                                McpU8               *hciCmdParms, 
                                                McpU8               hciCmdParmsLen,
                                                CcmaClientCb sequencerCb,
                                                void* pUserData)
{
    FmcStatus   status;
    FMC_UINT    len;
    FMC_FUNC_START("FM_TRANSPORT_IF_SendFmVacCommand");

    /* save the sequencer cb to be called when completed. and save the user data.*/
    _fmcTransportHciData[_FMC_CORE_TRANSPORT_CLIENT_VAC].sequencerCb = sequencerCb;
    _fmcTransportHciData[_FMC_CORE_TRANSPORT_CLIENT_VAC].pUserData = pUserData;

    _fmcTransportData.cmdParms[0] = hciCmdParms[0];
    
    /* Store the FM Parameters Len in LE */
    len = (hciCmdParmsLen -(sizeof(FmcFwOpcode) + FMC_CORE_HCI_FM_PARMS_LEN_FIELD_LEN));
    
    FMC_UTILS_StoreLE16(&_fmcTransportData.cmdParms[1], (FMC_U16)len);
    
    /* Copy the FM cmd parms (should be in the correct endiannity already) */
    FMC_OS_MemCopy(&_fmcTransportData.cmdParms[3], &hciCmdParms[3], len);
    
    _fmcTransportData.parmsLen = FMC_CORE_HCI_WRITE_FM_TOTAL_PARMS_LEN(len);
    
    status = _FMC_CORE_HCI_SendFmCommand(       _FMC_CORE_TRANSPORT_CLIENT_VAC,
                                                    _FM_IF_FmVacCmdCompleteCb,
                                                    _FMC_CMD_I2C_FM_WRITE,
                                                    _fmcTransportData.cmdParms,
                                                    _fmcTransportData.parmsLen);
    
    FMC_VERIFY_ERR((status == FMC_STATUS_PENDING), status, ("FM_TRANSPORT_IF_SendFmVacCommand"));
    
    FMC_FUNC_END();

    return CCMA_STATUS_PENDING;
}
/*
*   _FM_IF_FmVacCmdCompleteCb()
*
*   callback for FM commands sent by vac - it will call the sequencer to continue the process.
*/
void _FM_IF_FmVacCmdCompleteCb( FmcStatus   status,
                                        FMC_U8      *data,
                                        FMC_UINT    dataLen)
{
    CcmaClientEvent      clientEvent;
    
    FMC_FUNC_START("_FM_IF_FmVacCmdCompleteCb");

    FMC_VERIFY_FATAL_NO_RETVAR((status == FMC_STATUS_SUCCESS), 
                                    ("_FM_IF_FmVacCmdCompleteCb: Unexpected Event Type or complition status "));
    
    clientEvent.type = CCMA_CLIENT_EVENT_HCI_CMD_COMPLETE;
    /* the status is checked previosly for success */
    clientEvent.status = CCMA_STATUS_SUCCESS;
    /* get the event data.*/
    clientEvent.data.hciEventData.eventType = (CcmaHciEventType )FMC_HCE_COMMAND_COMPLETE;
    clientEvent.data.hciEventData.parmLen = (FMC_U8)dataLen;
    clientEvent.data.hciEventData.parms = data;
    /* get the user data set in the send command*/  
    clientEvent.pUserData = _fmcTransportHciData[_FMC_CORE_TRANSPORT_CLIENT_VAC].pUserData;

    /* call the sequencer cb.*/
    _fmcTransportHciData[_FMC_CORE_TRANSPORT_CLIENT_VAC].sequencerCb(&clientEvent);
    
    FMC_FUNC_END();

    
}

#ifdef BLUEZ_SOLUTION
FMC_STATIC EMcpfRes FMA_SendCommand(_FmcHciCommandData *cmdData)
{
	return fm_send_req(cmdData->uHciOpcode, cmdData->pHciCmdParms,
		cmdData->uHciCmdParmsLen, cmdData->pHciUserData);
}
#endif

#if defined(MCP_STK_ENABLE) && !defined(BLUEZ_SOLUTION)
FMC_STATIC EMcpfRes  FMA_SendCommand(_FmcHciCommandData *cmdData)
{
        return FmDrvIf_Write (cmdData->uHciOpcode, 
						      cmdData->pHciCmdParms, cmdData->uHciCmdParmsLen,							  
							  cmdData->pHciUserData);        
}

#elif !defined(BLUEZ_SOLUTION)
FMC_STATIC EMcpfRes  FMA_SendCommand(_FmcHciCommandData *cmdData)
{
        return HCIA_SendCommand (((Tmcpf *)(_fmcTransportData.hMcpf))->hHcia,
							 cmdData->uHciOpcode, 
							 cmdData->pHciCmdParms, cmdData->uHciCmdParmsLen,
							 _FMC_CORE_HCI_CmdCompleteCb, 
							 cmdData->pHciUserData);        
}

#endif

/*Utility function */
handle_t *FMC_GetCcmaObj(void)
{
    return &_fmcTransportData.ccmaObj;
}

#endif /*FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED*/





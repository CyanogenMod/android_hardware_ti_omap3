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
*	FILE NAME:		fm_rx.c
*
*	BRIEF:			This file defines the API of the FM Rx stack.
*
*	DESCRIPTION:	General
*
*					
*					
*	AUTHOR:   Zvi Schneider
*
\*******************************************************************************/
#include "mcpf_defs.h"
#include "fmc_types.h"
#include "fmc_defs.h"
#include "fmc_os.h"
#include "fmc_log.h"
#include "fmc_debug.h"
#include "fm_rx.h"
#include "fm_rx_sm.h"

FMC_LOG_SET_MODULE(FMC_LOG_MODULE_FMRX);

#if  FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED

/*
	Possible initialization states
*/
typedef enum _tagFmRxState {
	_FM_RX_INIT_STATE_NOT_INITIALIZED,
	_FM_RX_INIT_STATE_INITIALIZED,
	_FM_RX_INIT_STATE_INIT_FAILED
} _FmRxInitState;
typedef struct
{
	FMC_BOOL cond;
	FmRxStatus	errorStatus;
}_FmRxConditionParams;

/* current init state */
static _FmRxInitState _fmRxInitState = _FM_RX_INIT_STATE_NOT_INITIALIZED;


/* Macro that should be used at the start of APIs that require the FM TX to be enabled to start */
#define _FM_RX_FUNC_START_AND_LOCK_ENABLED(funcName)										\
			FMC_FUNC_START_AND_LOCK(funcName);												\
			FMC_VERIFY_ERR(	((FM_RX_SM_IsContextEnabled() == FMC_TRUE) &&					\
								(FM_RX_SM_IsCmdPending(FM_RX_CMD_DISABLE) == FMC_FALSE)),	\
								FM_RX_STATUS_CONTEXT_NOT_ENABLED,								\
			("%s May Be Called only when FM RX is enabled", funcName))

#define _FM_RX_FUNC_END_AND_UNLOCK_ENABLED()	FMC_FUNC_END_AND_UNLOCK()

FmRxStatus _FM_RX_SimpleCmdAndCopyParams(FmRxContext *fmContext, 
								FMC_UINT paramIn,
								FMC_BOOL condition,
								FmRxCmdType cmdT,
								const char * funcName);
FmRxStatus _FM_RX_SimpleCmd(FmRxContext *fmContext, 
								FmRxCmdType cmdT,
								const char * funcName);


 FmRxCallBack fmRxInitAsyncAppCallback = NULL;

extern FMC_BOOL fmRxSendDisableEventToApp;
/********************************************************************************
 *
 * Function declarations
 *
 *******************************************************************************/

 /*-------------------------------------------------------------------------------
 * FM_RX_Init()
 *
 * Brief:  
 *		Initializes FM RX module.
 *
 */
FmRxStatus FM_RX_Init(handle_t hMcpf)
{
	FmRxStatus	status = FM_RX_STATUS_SUCCESS;

	FMC_FUNC_START(("FM_RX_Init"));
	
	FMC_VERIFY_ERR((_fmRxInitState == _FM_RX_INIT_STATE_NOT_INITIALIZED), FM_RX_STATUS_NOT_DE_INITIALIZED,
						("FM_RX_Init: Invalid call while FM RX Is not De-Initialized"));
	
	/* Assume failure. If we fail before reaching the end, we will stay in this state */
	_fmRxInitState = _FM_RX_INIT_STATE_INIT_FAILED;
	
	/* Init RX & TX common module */
	status = FMCI_Init(hMcpf);
	FMC_VERIFY_FATAL((status == FMC_STATUS_SUCCESS), status, 
						("FM_RX_Init: FMCI_Init Failed (%s)", FMC_DEBUG_FmcStatusStr(status)));
	
	/* Init FM TX state machine */
	FM_RX_SM_Init();
		
	_fmRxInitState = _FM_RX_INIT_STATE_INITIALIZED;
	
	FMC_LOG_INFO(("FM_RX_Init: FM RX Initialization completed Successfully"));
	
	FMC_FUNC_END();
	
	return status;
}



 /*-------------------------------------------------------------------------------
 * FM_RX_Init_Async()
 *
 * Brief:  
 *      Initializes FM RX module Synchronous\Asynchronous.
 *
 */
FmRxStatus FM_RX_Init_Async(const FmRxCallBack fmInitCallback, handle_t hMcpf)
{
    FmRxStatus  status = FM_RX_STATUS_SUCCESS;
    FmRxSmContextState contextState=FM_RX_SM_CONTEXT_STATE_DESTROYED;
    FmRxContext *context = NULL;

    FMC_FUNC_START(("FM_RX_Init_Async"));

    FMC_VERIFY_ERR((0 != fmInitCallback), FM_RX_STATUS_INVALID_PARM, ("Null fmInitCallback"));
    
    /*Save the CallBack */
    fmRxInitAsyncAppCallback =fmInitCallback;

    /*Check if the stack is initialized*/
    if (_fmRxInitState==_FM_RX_INIT_STATE_INITIALIZED)
       {
    
        /*Need to verify What is the current SM state */
        contextState =FM_RX_SM_GetContextState();

        /*Get the current context */
        context =FM_RX_SM_GetContext();

        /*According to the State we need to perfrom the crospanding operations */
        switch(contextState)
        {                

            /*
                The Stack is allready disabled    
                Need only to Destroy 
            */
            case FM_RX_SM_CONTEXT_STATE_DISABLED:
                   status= FM_RX_Destroy(&context);                                                       
                   FMC_VERIFY_ERR((status== FM_RX_STATUS_SUCCESS), FM_RX_STATUS_FAILED, 
                   ("FM_RX_Init_Async: FM_RX_Destroy (%s)", FMC_DEBUG_FmcStatusStr(status)));
                 break;
           
            case FM_RX_SM_CONTEXT_STATE_ENABLED:
             /*
                Set up a flag that specified that this 
                specific event is not pass to the App
            */
                    fmRxSendDisableEventToApp=FMC_FALSE;
                    status= FM_RX_Disable(context);                      
                   FMC_VERIFY_ERR((status== FM_RX_STATUS_PENDING), FM_RX_STATUS_FAILED, 
                   ("FM_RX_Init_Async: FM_RX_Disable (%s)", FMC_DEBUG_FmcStatusStr(status)));
                 break;                      
             /*
                Set up a flag that specified that this specific event     
                Is not pass to the App
            */   
            case  FM_RX_SM_CONTEXT_STATE_DISABLING:          
                    fmRxSendDisableEventToApp=FMC_FALSE;
                    status = FM_RX_STATUS_PENDING;
                    break;
             /*
                At enabling state we need to disable
                the stuck, but not to pass the event 
                to the application  
             */
            case  FM_RX_SM_CONTEXT_STATE_ENABLING:                                  
                    fmRxSendDisableEventToApp=FMC_FALSE;
                    status=FM_RX_Disable(context);                 
                    FMC_VERIFY_ERR((status== FM_RX_STATUS_PENDING), FM_RX_STATUS_FAILED, 
                   ("FM_RX_Init_Async: FM_RX_Disable (%s)", FMC_DEBUG_FmcStatusStr(status)));

              /*  
                Action is not needed here.
              */
            case FM_RX_SM_CONTEXT_STATE_DESTROYED:
            default:
                 break;
            }

    }
    else
     {
        /*  
            First Init
        */
           status=FM_RX_Init(hMcpf);
      }         

    FMC_LOG_INFO(("FM_RX_Init_Async: FM RX Initialization completed Successfully"));
    
    FMC_FUNC_END();
    
    return status;
}
/*-------------------------------------------------------------------------------
 * FM_Deinit()
 *
 * Brief:  
 *		Deinitializes FM RX module.
 *
 */
FmRxStatus FM_RX_Deinit(void)
{
	FmRxStatus	status = FM_RX_STATUS_SUCCESS;
	
	FMC_FUNC_START(("FM_RX_Deinit"));
	
	if (_fmRxInitState !=_FM_RX_INIT_STATE_INITIALIZED)
	{
		FMC_LOG_INFO(("FM_RX_Deinit: Module wasn't properly initialized. Exiting without de-initializing"));
		FMC_RET(FM_RX_STATUS_SUCCESS);
	}
	
	FMC_VERIFY_FATAL((FM_RX_SM_GetContextState() ==  FM_RX_SM_CONTEXT_STATE_DESTROYED), 
						FM_RX_STATUS_CONTEXT_NOT_DESTROYED, ("FM_RX_Deinit: FM RX Context must first be destoryed"));
	
	/* De-Init FM TX state machine */
	FM_RX_SM_Deinit();
	
	/* De-Init RX & TX common module */
	FMCI_Deinit();
	
	_fmRxInitState = _FM_RX_INIT_STATE_NOT_INITIALIZED;
	
	FMC_FUNC_END();
	
	return status;
}

/*-------------------------------------------------------------------------------
 * FM_RX_Create()
 *
 * Brief:  
 *		Allocates a unique FM RX context.
 *
 */
FmRxStatus FM_RX_Create(FmcAppHandle *appHandle, const FmRxCallBack fmCallback, FmRxContext **fmContext)
{
	FmRxStatus	status = FM_RX_STATUS_SUCCESS;
	
	FMC_FUNC_START(("FM_RX_Create"));
	
	FMC_VERIFY_ERR((appHandle == NULL), FMC_STATUS_NOT_SUPPORTED, ("FM_RX_Create: appHandle Must be null currently"));
	FMC_VERIFY_ERR((_fmRxInitState == _FM_RX_INIT_STATE_INITIALIZED), FM_RX_STATUS_NOT_INITIALIZED,
						("FM_RX_Create: FM RX Not Initialized"));
	FMC_VERIFY_FATAL((FM_RX_SM_GetContextState() ==  FM_RX_SM_CONTEXT_STATE_DESTROYED), 
						FM_RX_STATUS_CONTEXT_NOT_DESTROYED, ("FM_RX_Deinit: FM TX Context must first be destoryed"));
	
	status = FM_RX_SM_Create(fmCallback, fmContext);
	FMC_VERIFY_FATAL((status == FM_RX_STATUS_SUCCESS), status, 
						("FM_RX_Create: FM_RX_SM_Create Failed (%s)", FMC_DEBUG_FmcStatusStr(status)));
	
	FMC_LOG_INFO(("FM_RX_Create: Successfully Create FM RX Context"));
	
	FMC_FUNC_END();
	
	return status;
}

/*-------------------------------------------------------------------------------
 * FM_RX_Destroy()
 *
 * Brief:  
 *		Releases the FM RX context (previously allocated with FM_RX_Create()).
 *
 */
FmRxStatus FM_RX_Destroy(FmRxContext **fmContext)
{
	FmRxStatus	status = FM_RX_STATUS_SUCCESS;
	
	FMC_FUNC_START(("FM_RX_Destroy"));
	
	FMC_VERIFY_ERR((*fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_Destroy: Invalid Context Ptr"));
	FMC_VERIFY_ERR((FM_RX_SM_GetContextState() ==  FM_RX_SM_CONTEXT_STATE_DISABLED), 
						FM_RX_STATUS_CONTEXT_NOT_DISABLED, ("FM_RX_Destroy: FM RX Context must be disabled before destroyed"));
	
	status = FM_RX_SM_Destroy(fmContext);
	FMC_VERIFY_FATAL((status == FM_RX_STATUS_SUCCESS), status, 
						("FM_RX_Destroy: FM_RX_SM_Destroy Failed (%s)", FMC_DEBUG_FmcStatusStr(status)));
	
	/* Make sure pointer will not stay dangling */
	*fmContext = NULL;
	
	FMC_LOG_INFO(("FM_RX_Destroy: Successfully Destroyed FM RX Context"));
	
	FMC_FUNC_END();
	
	return status;
}

/*-------------------------------------------------------------------------------
 * FM_RX_Enable()
 *
 * Brief:  
 *		Enable FM RX
 *
 */
FmRxStatus FM_RX_Enable(FmRxContext *fmContext)
{
	FmRxStatus		status;
	FmRxGenCmd * enableCmd = NULL;
	FMC_FUNC_START_AND_LOCK(("FM_RX_Enable"));
	
	FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_Enable: Invalid Context Ptr"));
	FMC_VERIFY_ERR((FM_RX_SM_GetContextState() ==  FM_RX_SM_CONTEXT_STATE_DISABLED), 
						FM_RX_STATUS_CONTEXT_NOT_DISABLED, ("FM_RX_Enable: FM RX Context Is Not Disabled"));
	FMC_VERIFY_ERR((FM_RX_SM_IsCmdPending(FM_RX_CMD_ENABLE) == FMC_FALSE), FM_RX_STATUS_IN_PROGRESS,
						("FM_RX_Enable: Enabling Already In Progress"));
	
	/* When we are disabled there must not be any pending commands in the queue */
	FMC_VERIFY_FATAL((FMC_IsListEmpty(FMCI_GetCmdsQueue()) == FMC_TRUE), FMC_STATUS_INTERNAL_ERROR,
						("FM_RX_Enable: Context is Disabled but there are pending command in the queue"));
	
	/* 
		Direct FM task events to the RX's event handler.
		This must be done here (and not in fm_rx_sm.c) since only afterwards
		we will be able to send the event the event handler
	*/
	status = FMCI_SetEventCallback(Fm_Rx_Stack_EventCallback);
	FMC_VERIFY_FATAL((status == FMC_STATUS_SUCCESS), status, 
						("FM_RX_Enable: FMCI_SetEventCallback Failed (%s)", FMC_DEBUG_FmcStatusStr(status)));
	
	/* Allocates the command and insert to commands queue */
	status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, FM_RX_CMD_ENABLE, (FmcBaseCmd**)&enableCmd);
	FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, ("FM_RX_Enable"));
	
	
	status = FM_RX_STATUS_PENDING;
	
	FMC_FUNC_END_AND_UNLOCK();
	/* Trigger RX SM to execute the command in FM Task context */
	FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
	
	
	return status;
	
}
/*-------------------------------------------------------------------------------
 * FM_RX_Disable()
 *
 * Brief:  
 *		Disable FM RX
 *
 */
FmRxStatus FM_RX_Disable(FmRxContext *fmContext)
{
	FmRxStatus		status;
	FmRxGenCmd * disableCmd = NULL;
	
	FMC_FUNC_START_AND_LOCK(("FM_RX_Disable"));
	FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_Disable: Invalid Context Ptr"));
	
	status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, FM_RX_CMD_DISABLE, (FmcBaseCmd**)&disableCmd);
	FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, ("FM_RX_Disable"));

	status = FM_RX_STATUS_PENDING;

	FMC_FUNC_END_AND_UNLOCK();
	
	/* Trigger TX SM to execute the command in FM Task context */
	FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);

	return status;
}




/*-------------------------------------------------------------------------------
 * FM_RX_SetBand()
 *
 * Brief:  
 *		Set the radio band.
 *
 */
FmRxStatus FM_RX_SetBand(FmRxContext *fmContext, FmcBand band)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								band,
								((FMC_BAND_JAPAN==band)||(FMC_BAND_EUROPE_US == band)),
								FM_RX_CMD_SET_BAND,
								"FM_RX_SetBand");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetBand()
 *
 * Brief:  
 *		Returns the current band.
 *
 */
FmRxStatus FM_RX_GetBand(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_BAND,
								"FM_RX_GetBand");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetMonoStereoMode()
 *
 * Brief:  
 *		Set the Mono/Stereo mode.
 *
 */
FmRxStatus FM_RX_SetMonoStereoMode(FmRxContext *fmContext, FmRxMonoStereoMode mode)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								mode,
								((FM_RX_MONO_MODE==mode)||(FM_RX_STEREO_MODE== mode)),
								FM_RX_CMD_SET_MONO_STEREO_MODE,
								"FM_RX_SetMonoStereoMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetMonoStereoMode()
 *
 * Brief: 
 *		Returns the current Mono/Stereo mode.
 *
 */
FmRxStatus FM_RX_GetMonoStereoMode(FmRxContext *fmContext)
{

	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_MONO_STEREO_MODE,
								"FM_RX_GetMonoStereoMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetMuteMode()
 *
 * Brief:  
 *		Set the mute mode.
 *
 */
FmRxStatus FM_RX_SetMuteMode(FmRxContext *fmContext, FmcMuteMode mode)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								mode,
								((FMC_MUTE==mode)||(FMC_ATTENUATE== mode)||(FMC_NOT_MUTE== mode)),
								FM_RX_CMD_SET_MUTE_MODE,
								"FM_RX_SetMuteMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetMuteMode()
 *
 * Brief:  
 *		Returns the current mute mode.
 *
 */
FmRxStatus FM_RX_GetMuteMode(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_MUTE_MODE,
								"FM_RX_GetMuteMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRfDependentMute()
 *
 * Brief:  
 *		Enable/Disable the RF dependent mute feature.
 *
 */
FmRxStatus FM_RX_SetRfDependentMuteMode(FmRxContext *fmContext, FmRxRfDependentMuteMode mode)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								mode,
								((FM_RX_RF_DEPENDENT_MUTE_ON==mode)||(FM_RX_RF_DEPENDENT_MUTE_OFF== mode)),
								FM_RX_CMD_SET_RF_DEPENDENT_MUTE_MODE,
								"FM_RX_SetRfDependentMuteMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetRfDependentMute()
 *
 * Brief:  
 *		Returns the current RF-Dependent mute mode.
 *
 */
FmRxStatus FM_RX_GetRfDependentMute(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_RF_DEPENDENT_MUTE_MODE,
								"FM_RX_GetRfDependentMute");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRssiThreshold()
 *
 * Brief:  
 *		Sets the RSSI tuning threshold
 *
 */
FmRxStatus FM_RX_SetRssiThreshold(FmRxContext *fmContext, FMC_INT threshold)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								threshold,
								((threshold >= 1)&&(threshold <= 127)),
								FM_RX_CMD_SET_RSSI_THRESHOLD,
								"FM_RX_SetRssiThreshold");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetRssiThreshold()
 *
 * Brief:  
 *		Returns the current RSSI threshold.
 *
 */
FmRxStatus FM_RX_GetRssiThreshold(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_RSSI_THRESHOLD,
								"FM_RX_GetRssiThreshold");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetDeEmphasisFilter()
 *
 * Brief: 
 *		Selects the receiver's de-emphasis filter
 *
 */
FmRxStatus FM_RX_SetDeEmphasisFilter(FmRxContext *fmContext, FmcEmphasisFilter filter)
{

	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								filter,
								((FMC_EMPHASIS_FILTER_NONE==filter)||(FMC_EMPHASIS_FILTER_75_USEC==filter)||(filter== FMC_EMPHASIS_FILTER_50_USEC)),
								FM_RX_CMD_SET_DEEMPHASIS_FILTER,
								"FM_RX_SetDeEmphasisFilter");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetDeEmphasisFilter()
 *
 * Brief:  
 *		Returns the current de-emphasis filter
 *
 */
FmRxStatus FM_RX_GetDeEmphasisFilter(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_DEEMPHASIS_FILTER,
								"FM_RX_GetDeEmphasisFilter");
}


/*-------------------------------------------------------------------------------
 * FM_RX_SetVolume()
 *
 * Brief:  
 *		Set the gain level of the audio left & right channels
 *
 */
FmRxStatus FM_RX_SetVolume(FmRxContext *fmContext, FMC_UINT level)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								level,
								(level<=70),
								FM_RX_CMD_SET_VOLUME,
								"FM_RX_SetVolume");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetVolume()
 *
 * Brief:  
 *		Returns the current volume level.
 *
 */
FmRxStatus FM_RX_GetVolume(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_VOLUME,
								"FM_RX_GetVolume");
}

/*-------------------------------------------------------------------------------
 * FM_RX_Tune()
 *
 * Brief:  
 *		Tune the receiver to the specified frequency
 *
 */
FmRxStatus FM_RX_Tune(FmRxContext *fmContext, FmcFreq freq)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								freq,
								((FMC_FIRST_FREQ_JAPAN_KHZ<=freq)&&(FMC_LAST_FREQ_US_EUROPE_KHZ>=freq)),
								FM_RX_CMD_TUNE,
								"FM_RX_Tune");
}

/*-------------------------------------------------------------------------------
 * FM_RX_IsValidChannel()
 *
 */
FmRxStatus FM_RX_IsValidChannel(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_IS_CHANNEL_VALID,
                                "FM_RX_IsValidChannel");
}


/*-------------------------------------------------------------------------------
 * FM_RX_GetTunedFrequency()
 *
 */
FmRxStatus FM_RX_GetTunedFrequency(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_TUNED_FREQUENCY,
								"FM_RX_GetTunedFrequency");
}


/*-------------------------------------------------------------------------------
 * FM_RX_Seek()
 *
 * Brief: 
 *		Seeks the next good station or stop the current seek.
 *
 */
FmRxStatus FM_RX_Seek(FmRxContext *fmContext, FmRxSeekDirection direction)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								direction,
								((direction == FM_RX_SEEK_DIRECTION_DOWN )||(direction == FM_RX_SEEK_DIRECTION_UP)),
								FM_RX_CMD_SEEK,
								"FM_RX_Seek");
}

/*-------------------------------------------------------------------------------
 * FM_RX_StopSeek()
 *
 * Brief: 
 *		Stops a seek in progress
 *
 */
FmRxStatus FM_RX_StopSeek(FmRxContext *fmContext)
{

	FmRxStatus status = FM_RX_STATUS_PENDING;

	/* Lock the stack */
	_FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_StopSeek");
	
    FMC_UNUSED_PARAMETER(fmContext);
	
	if(FM_RX_SM_GetRunningCmd() == FM_RX_CMD_SEEK)
	{
		FM_RX_SM_SetUpperEvent( FM_RX_SM_UPPER_EVENT_STOP_SEEK);
		FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
	}
	else
	{
		status = FM_RX_STATUS_SEEK_IS_NOT_IN_PROGRESS;
	}
	_FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
	return status;
}


/*-------------------------------------------------------------------------------
 * FM_RX_CompleteScan()
 *
 */
FmRxStatus FM_RX_CompleteScan(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext, 
                                FM_RX_CMD_COMPLETE_SCAN,
                                "FM_RX_GetTunedFrequency");
}


/*-------------------------------------------------------------------------------
 * FM_RX_CompleteScanProgress()
 *
 */
FmRxStatus FM_RX_GetCompleteScanProgress(FmRxContext *fmContext)
{
  
    FmRxStatus status = FM_RX_STATUS_PENDING;

    /* Lock the stack */
    _FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_GetCompleteScanProgress");
    
    FMC_UNUSED_PARAMETER(fmContext);
    
    if(FM_RX_SM_GetRunningCmd() == FM_RX_CMD_COMPLETE_SCAN)
    {
        FM_RX_SM_SetUpperEvent( FM_RX_SM_UPPER_EVENT_GET_COMPLETE_SCAN_PROGRESS);
        FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
    }
    else
    {
        status = FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS;
    }
    _FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
    return status;
}

/*-------------------------------------------------------------------------------
 * FM_RX_StopCompleteScan()
 *
 */
FmRxStatus FM_RX_StopCompleteScan(FmRxContext *fmContext)
{
  
    FmRxStatus status = FM_RX_STATUS_PENDING;

    /* Lock the stack */
    _FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_StopCompleteScan");
    
    FMC_UNUSED_PARAMETER(fmContext);
    
    if(FM_RX_SM_GetRunningCmd() == FM_RX_CMD_COMPLETE_SCAN)
    {
        FM_RX_SM_SetUpperEvent( FM_RX_SM_UPPER_EVENT_STOP_COMPLETE_SCAN);
        FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
    }
    else
    {
        status = FM_RX_STATUS_COMPLETE_SCAN_IS_NOT_IN_PROGRESS;
    }
    _FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
    return status;
}


/*-------------------------------------------------------------------------------
 * FM_RX_GetRssi()
 *
 * Brief:  
 *		Returns the current RSSI.
 *
 */
FmRxStatus FM_RX_GetRssi(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_RSSI,
								"FM_RX_GetRssi");
}

/*-------------------------------------------------------------------------------
 * FM_RX_EnableRds()
 *
 * Brief:  
 *		Enables RDS reception.
 *
 */
FmRxStatus FM_RX_EnableRds(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_ENABLE_RDS,
								"FM_RX_EnableRds");
}

/*-------------------------------------------------------------------------------
 * FM_RX_DisableRds()
 *
 * Brief:  
 *		Disables RDS reception.
 *
 */
FmRxStatus FM_RX_DisableRds(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_DISABLE_RDS,
								"FM_RX_DisableRds");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRdsSystem()
 *
 * Brief: 
 *		Choose the RDS mode - RDS/RBDS
 *
 */
FmRxStatus FM_RX_SetRdsSystem(FmRxContext *fmContext, FmcRdsSystem	rdsSystem)
{
	
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								rdsSystem,
								((rdsSystem==FM_RDS_SYSTEM_RBDS)||(rdsSystem==FM_RDS_SYSTEM_RDS)),
								FM_RX_CMD_SET_RDS_SYSTEM,
								"FM_RX_SetRdsSystem");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetRdsSystem()
 *
 * Brief:  
 *		Returns the current RDS System in use.
 *
 */
FmRxStatus FM_RX_GetRdsSystem(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_RDS_SYSTEM,
								"FM_RX_GetRdsSystem");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRdsGroupMask()
 *
 * Brief: 
 *		Selects which RDS gorups to report in a raw RDS event
 *
 */
FmRxStatus FM_RX_SetRdsGroupMask(FmRxContext *fmContext, FmcRdsGroupTypeMask groupMask)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								groupMask,
								FMC_TRUE,
								FM_RX_CMD_SET_RDS_GROUP_MASK,
								"FM_RX_SetRdsGroupMask");
}

/*-------------------------------------------------------------------------------
 * FM_RX_GetRdsGroupMask()
 *
 * Brief: 
 *		Returns the current RDS group mask
 *
 */
FmRxStatus FM_RX_GetRdsGroupMask(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_RDS_GROUP_MASK,
								"FM_RX_GetRdsGroupMask");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetRdsAFSwitchMode()
 *
 * Brief: 
 *		Sets the RDS AF feature On or Off
 *
 */
FmRxStatus FM_RX_SetRdsAfSwitchMode(FmRxContext *fmContext, FmRxRdsAfSwitchMode mode)
{
	return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
								mode,
								((mode==FM_RX_RDS_AF_SWITCH_MODE_ON)||(mode==FM_RX_RDS_AF_SWITCH_MODE_OFF)),
								FM_RX_CMD_SET_RDS_AF_SWITCH_MODE,
								"FM_RX_SetRdsAfSwitchMode");
}
/*-------------------------------------------------------------------------------
 * FM_RX_GetRdsAFSwitchMode()
 *
 * Brief:  
 *		Returns the current RDS AF Mode.
 *
 */
FmRxStatus FM_RX_GetRdsAfSwitchMode(FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_GET_RDS_AF_SWITCH_MODE,
								"FM_RX_GetRdsAfSwitchMode");
}

/*-------------------------------------------------------------------------------
 * FM_RX_SetChannelSpacing()
 *
 * Brief:  
 *      Sets the channel Seek Spacing.
 *
 */
FmRxStatus FM_RX_SetChannelSpacing(FmRxContext *fmContext,FmcChannelSpacing channelSpacing)
{
    return _FM_RX_SimpleCmdAndCopyParams(fmContext, 
                                channelSpacing,
                                ((FMC_CHANNEL_SPACING_50_KHZ==channelSpacing)||
                                                          (FMC_CHANNEL_SPACING_100_KHZ== channelSpacing)||
                                                           (FMC_CHANNEL_SPACING_200_KHZ== channelSpacing)),
                                FM_RX_CMD_SET_CHANNEL_SPACING,
                                "FM_RX_SetChannelSpacing");
}


/*-------------------------------------------------------------------------------
 * FM_RX_SetChannelSpacing()
 *
 * Brief:  
 *      Gets the channel Seek Spacing.
 *
 */
FmRxStatus FM_RX_GetChannelSpacing(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext,                      
                                FM_RX_CMD_GET_CHANNEL_SPACING,
                                "FM_RX_GetChannelSpacing");
}


/*-------------------------------------------------------------------------------
 * FM_RX_GetFwVersion()
 *
 * Brief:  
 *      Sets the channel Seek Spacing.
 *
 */
FmRxStatus FM_RX_GetFwVersion(FmRxContext *fmContext)
{
    return _FM_RX_SimpleCmd(fmContext,                      
                                FM_RX_CMD_GET_FW_VERSION,
                                "FM_RX_GetFwVersion");
}


/*-------------------------------------------------------------------------------
 * FM_RX_ChangeAudioTarget()
 *
 * Brief:  
 *		bbb
 *
 */
FmRxStatus FM_RX_ChangeAudioTarget (FmRxContext *fmContext, FmRxAudioTargetMask rxTagets, ECAL_SampleFrequency eSampleFreq)
{
	FmRxStatus		status;
	FmRxSetAudioTargetCmd	*setAudioTargetCmd = NULL;
	
	_FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_ChangeAudioTarget");
	FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_ChangeAudioTarget: Invalid Context Ptr"));
	
	/* Verify that the band value is valid */
	/*FMC_VERIFY_ERR(condition, 
						FM_RX_STATUS_INVALID_PARM, ("%s: Invalid param",funcName));*/
	
	/* Allocates the command and insert to commands queue */
	status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, 
												FM_RX_CMD_CHANGE_AUDIO_TARGET, 
												(FmcBaseCmd**)&setAudioTargetCmd);
	FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, ("FM_RX_ChangeAudioTarget"));
	
	
	/* Copy cmd parms for the cmd execution phase*/
	setAudioTargetCmd->rxTargetMask= rxTagets;
	setAudioTargetCmd->eSampleFreq = eSampleFreq;
	
	status = FM_RX_STATUS_PENDING;

	/* Trigger TX SM to execute the command in FM Task context */
	FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
	
	_FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
	
	return status;
}
/*-------------------------------------------------------------------------------
 * FM_RX_ChangeDigitalTargetConfiguration()
 *
 * Brief:  
 *		bbb
 *
 */
FmRxStatus FM_RX_ChangeDigitalTargetConfiguration(FmRxContext *fmContext, ECAL_SampleFrequency eSampleFreq)
{
	FmRxStatus		status;
	FmRxSetDigitalAudioConfigurationCmd 	*setDigitalTargetConfigurationCmd = NULL;
	
	_FM_RX_FUNC_START_AND_LOCK_ENABLED("FM_RX_ChangeAudioTarget");
	FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("FM_RX_ChangeAudioTarget: Invalid Context Ptr"));
	
	/* Verify that the band value is valid */
	/*FMC_VERIFY_ERR(condition, 
						FM_RX_STATUS_INVALID_PARM, ("%s: Invalid param",funcName));*/
	
	/* Allocates the command and insert to commands queue */
	status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, 
													FM_RX_CMD_CHANGE_DIGITAL_AUDIO_CONFIGURATION, 
													(FmcBaseCmd**)&setDigitalTargetConfigurationCmd);
	FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, ("FM_RX_ChangeAudioTarget"));
	
	
	/* Copy cmd parms for the cmd execution phase*/
	setDigitalTargetConfigurationCmd->eSampleFreq = eSampleFreq;
	
	status = FM_RX_STATUS_PENDING;
	
	/* Trigger TX SM to execute the command in FM Task context */
	FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);
	
	_FM_RX_FUNC_END_AND_UNLOCK_ENABLED();
	
	return status;
		
}
/*-------------------------------------------------------------------------------
 * FM_RX_EnableAudioRouting()
 *
 * Brief:  
 *		bbb
 *
 */
FmRxStatus FM_RX_EnableAudioRouting (FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_ENABLE_AUDIO,
								"FM_RX_EnableAudioRouting");
}
/*-------------------------------------------------------------------------------
 * FM_RX_DisableAudioRouting()
 *
 * Brief:  
 *		bbb
 */
FmRxStatus FM_RX_DisableAudioRouting (FmRxContext *fmContext)
{
	return _FM_RX_SimpleCmd(fmContext, 
								FM_RX_CMD_DISABLE_AUDIO,
								"FM_RX_DisableAudioRouting");
}

/***************************************************************************************************
 *													
 ***************************************************************************************************/


FmRxStatus _FM_RX_SimpleCmdAndCopyParams(FmRxContext *fmContext, 
								FMC_UINT paramIn,
								FMC_BOOL condition,
								FmRxCmdType cmdT,
								const char * funcName)
{
	FmRxStatus		status;
	FmRxSimpleSetOneParamCmd * baseCmd;

	_FM_RX_FUNC_START_AND_LOCK_ENABLED(funcName);
	FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("%s: Invalid Context Ptr",funcName));

	/* Verify that the band value is valid */
	FMC_VERIFY_ERR(condition, 
						FM_RX_STATUS_INVALID_PARM, ("%s: Invalid param",funcName));

	/* Allocates the command and insert to commands queue */
	status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, cmdT, (FmcBaseCmd**)&baseCmd);
	FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, (funcName));
	
	/* Copy cmd parms for the cmd execution phase*/
	/*we assume here strongly that the stract pointed by base command as as its first field base command and another field with the param*/

	baseCmd->paramIn = paramIn;
	
	status = FM_RX_STATUS_PENDING;
	
	/* Trigger TX SM to execute the command in FM Task context */
	FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);

	_FM_RX_FUNC_END_AND_UNLOCK_ENABLED();

	return status;
}
FmRxStatus _FM_RX_SimpleCmd(FmRxContext *fmContext, 
								FmRxCmdType cmdT,
								const char * funcName)
{
	FmRxStatus		status;
	FmRxSimpleSetOneParamCmd * baseCmd;
	_FM_RX_FUNC_START_AND_LOCK_ENABLED(funcName);
	FMC_VERIFY_ERR((fmContext == FM_RX_SM_GetContext()), FMC_STATUS_INVALID_PARM, ("%s: Invalid Context Ptr",funcName));

	status = FM_RX_SM_AllocateCmdAndAddToQueue(fmContext, cmdT, (FmcBaseCmd**)&baseCmd);
	FMC_VERIFY_ERR((status == FMC_STATUS_SUCCESS), status, (funcName));

	status = FM_RX_STATUS_PENDING;

	/* Trigger TX SM to execute the command in FM Task context */
	FMCI_NotifyFmTask(FMC_OS_EVENT_FMC_STACK_TASK_PROCESS);

	_FM_RX_FUNC_END_AND_UNLOCK_ENABLED();

	return status;
}
#else  /*FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED*/

FmRxStatus FM_RX_Init(void)
{
	
		return FM_RX_STATUS_SUCCESS;
	
}

FmRxStatus FM_RX_Deinit(void)
{

		return FM_RX_STATUS_SUCCESS;
}


#endif /*FMC_CONFIG_FM_STACK == FMC_CONFIG_ENABLED*/





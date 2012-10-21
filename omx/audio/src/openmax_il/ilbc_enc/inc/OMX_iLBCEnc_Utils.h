
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
/* =============================================================================
*             Texas Instruments OMAP(TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */
/**
* @file OMX_iLBCEnc_Utils.h
*
* This is an header file for an iLBC Encoder that is fully
* compliant with the OMX Audio specification 1.5.
* This the file that the application that uses OMX would include in its code.
*
* @path $(CSLPATH)\OMAPSW_MPU\linux\audio\src\openmax_il\ilbc_enc\inc
*
* @rev 1.0
*/
/* --------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 04-Jul-2008 ad: Update for June 08 code review findings.
*! 10-Feb-2008 on: Initial Version
*! This is newest file
* =========================================================================== */
#ifndef OMX_ILBCENC_UTILS__H
#define OMX_ILBCENC_UTILS__H

#include <pthread.h>
#include "LCML_DspCodec.h"
#include <TIDspOmx.h>
#include "usn.h"
#include <OMX_TI_Common.h>
#ifdef RESOURCE_MANAGER_ENABLED
#include <ResourceManagerProxyAPI.h>
#endif
#ifdef __PERF_INSTRUMENTATION__
    #include "perf.h"
#endif

#ifdef DSP_RENDERING_ON
    #include <AudioManagerAPI.h>
#endif

#ifndef UNDER_CE
/*  #include <ResourceManagerProxyAPI.h> */
#endif

#ifdef UNDER_CE
    #define sleep Sleep
#endif


/* ======================================================================= */
/**
 * @def    ILBCENC_DEBUG   Turns debug messaging on and off
 */
/* ======================================================================= */
#undef ILBCENC_DEBUG
/* ======================================================================= */
/**
 * @def    ILBCENC_MEMCHECK   Turns memory messaging on and off
 */
/* ======================================================================= */
#undef ILBCENC_MEMCHECK

/* ======================================================================= */
/**
 * @def    ILBCENC_EPRINT   Error print macro
 */
/* ======================================================================= */
#ifndef UNDER_CE
        #define ILBCENC_EPRINT(...)    fprintf(stderr,__VA_ARGS__)
#else
        #define ILBCENC_EPRINT  printf
#endif
#ifndef UNDER_CE
/* ======================================================================= */
/**
 * @def    ILBCENC_DEBUG   Debug print macro
 */
/* ======================================================================= */
#ifdef  ILBCENC_DEBUG
        #define ILBCENC_DPRINT(...)    fprintf(stderr,__VA_ARGS__)
#else
        #define ILBCENC_DPRINT(...)
#endif
/* ======================================================================= */
/**
 * @def    ILBCENC_MEMCHECK   Memory print macro
 */
/* ======================================================================= */
#ifdef  ILBCENC_MEMCHECK
        #define ILBCENC_MEMPRINT(...)    fprintf(stderr,__VA_ARGS__)
#else
        #define ILBCENC_MEMPRINT(...)
#endif

#else   
#ifdef DEBUG
        #define ILBCENC_DPRINT     printf
        #define ILBCENC_MEMPRINT   printf
#else
        #define ILBCENC_DPRINT
        #define ILBCENC_MEMPRINT
#endif

#endif

#define ILBCENC_EXIT_COMPONENT_THRD  10

/* ======================================================================= */
/**
 *  M A C R O S FOR MALLOC and MEMORY FREE and CLOSING PIPES
 */
/* ======================================================================= */

#define OMX_ILBCCONF_INIT_STRUCT(_s_, _name_)   \
    memset((_s_), 0x0, sizeof(_name_)); \
    (_s_)->nSize = sizeof(_name_);      \
    (_s_)->nVersion.s.nVersionMajor = 0x1;  \
    (_s_)->nVersion.s.nVersionMinor = 0x0;  \
    (_s_)->nVersion.s.nRevision = 0x0;      \
    (_s_)->nVersion.s.nStep = 0x0

#define OMX_ILBCCLOSE_PIPE(_pStruct_,err)\
    ILBCENC_DPRINT("%d :: CLOSING PIPE \n",__LINE__);\
    err = close (_pStruct_);\
    if(0 != err && OMX_ErrorNone == eError){\
        eError = OMX_ErrorHardware;\
        printf("%d :: Error while closing pipe\n",__LINE__);\
        goto EXIT;\
    }

#define ILBCENC_OMX_ERROR_EXIT(_e_, _c_, _s_)\
    _e_ = _c_;\
    printf("\n**************** OMX ERROR ************************\n");\
    printf("%d : Error Name: %s : Error Num = %x",__LINE__, _s_, _e_);\
    printf("\n**************** OMX ERROR ************************\n");\
    goto EXIT;

/* ======================================================================= */
/**
 * @def ILBCENC_NUM_INPUT_BUFFERS   Default number of input buffers
 */
/* ======================================================================= */
#define ILBCENC_NUM_INPUT_BUFFERS 1
/* ======================================================================= */
/**
 * @def ILBCENC_NUM_INPUT_BUFFERS_DASF  Default No.of input buffers DASF
 */
/* ======================================================================= */
#define ILBCENC_NUM_INPUT_BUFFERS_DASF 2
/* ======================================================================= */
/**
 * @def ILBCENC_NUM_OUTPUT_BUFFERS   Default number of output buffers
 */
/* ======================================================================= */
#define ILBCENC_NUM_OUTPUT_BUFFERS 1
/* ======================================================================= */
/**
 * @def ILBCENC_INPUT_BUFFER_SIZE        Default input buffer size
 *      ILBCENC_INPUT_BUFFER_SIZE_DASF  Default input buffer size DASF
 *      ILBCENC_INPUT_FRAME_SIZE        Default input Frame Size
 */
/* ======================================================================= */
#define ILBCENC_PRIMARY_INPUT_BUFFER_SIZE 320
#define ILBCENC_PRIMARY_INPUT_BUFFER_SIZE_DASF 320
#define ILBCENC_PRIMARY_INPUT_FRAME_SIZE 320

#define ILBCENC_SECONDARY_INPUT_BUFFER_SIZE 480
#define ILBCENC_SECONDARY_INPUT_BUFFER_SIZE_DASF 480
#define ILBCENC_SECONDARY_INPUT_FRAME_SIZE 480

/* ======================================================================= */
/**
 * @def ILBCENC_OUTPUT_BUFFER_SIZE   Default output buffer size
 *      ILBCENC_OUTPUT_FRAME_SIZE     Default output frame size
 */
/* ======================================================================= */
#define ILBCENC_PRIMARY_OUTPUT_BUFFER_SIZE 38
#define ILBCENC_PRIMARY_OUTPUT_FRAME_SIZE 38

#define ILBCENC_SECONDARY_OUTPUT_BUFFER_SIZE 50
#define ILBCENC_SECONDARY_OUTPUT_FRAME_SIZE 50

#define ILBCENC_PRIMARY_CODEC_FRAME_TYPE 0
#define ILBCENC_SECONDARY_CODEC_FRAME_TYPE 1


/* ======================================================================= */
/**
 * @def ILBCENC_APP_ID  App ID Value setting
 */
/* ======================================================================= */
#define ILBCENC_APP_ID 100  /*investigate this one*/

/* ======================================================================= */
/**
 * @def    ILBCENC_SAMPLING_FREQUENCY   Sampling frequency
 */
/* ======================================================================= */
#define ILBCENC_SAMPLING_FREQUENCY 8000
/* ======================================================================= */
/**
 * @def    ILBCENC_CPU_LOAD                    CPU Load in MHz
 */
/* ======================================================================= */
#define ILBCENC_CPU_LOAD 12
/* ======================================================================= */
/**
 * @def    ILBCENC_MAX_NUM_OF_BUFS   Maximum number of buffers
 */
/* ======================================================================= */
#define ILBCENC_MAX_NUM_OF_BUFS 10
/* ======================================================================= */
/**
 * @def    ILBCENC_NUM_OF_PORTS   Number of ports
 */
/* ======================================================================= */
#define ILBCENC_NUM_OF_PORTS 2
/* ======================================================================= */
/**
 * @def    ILBCENC_XXX_VER    Component version
 */
/* ======================================================================= */
#define ILBCENC_MAJOR_VER 0xF1
#define ILBCENC_MINOR_VER 0xF2
/* ======================================================================= */
/**
 * @def    ILBCENC_NOT_USED    Defines a value for "don't care" parameters
 */
/* ======================================================================= */
#define ILBCENC_NOT_USED 10
/* ======================================================================= */
/**
 * @def    ILBCENC_NORMAL_BUFFER  Defines flag value with all flags off
 */
/* ======================================================================= */
#define ILBCENC_NORMAL_BUFFER 0
/* ======================================================================= */
/**
 * @def    OMX_ILBCENC_DEFAULT_SEGMENT    Default segment ID for the LCML
 */
/* ======================================================================= */
#define ILBCENC_DEFAULT_SEGMENT (0)
/* ======================================================================= */
/**
 * @def    OMX_ILBCENC_SN_TIMEOUT    Timeout value for the socket node
 */
/* ======================================================================= */
#define ILBCENC_SN_TIMEOUT (-1)
/* ======================================================================= */
/**
 * @def    OMX_ILBCENC_SN_PRIORITY   Priority for the socket node
 */
/* ======================================================================= */
#define ILBCENC_SN_PRIORITY (10)
/* ======================================================================= */
/**
 * @def    OMX_ILBCENC_NUM_DLLS   number of DLL's
 */
/* ======================================================================= */
#define ILBCENC_NUM_DLLS (2)
/* ======================================================================= */
/**
 * @def    ILBCENC_USN_DLL_NAME   USN DLL name
 */
/* ======================================================================= */
#ifdef UNDER_CE
    #define ILBCENC_USN_DLL_NAME "\\windows\\usn.dll64P"
#else
    #define ILBCENC_USN_DLL_NAME "/usn.dll64P"
#endif

/* ======================================================================= */
/**
 * @def    ILBCENC_DLL_NAME   ILBC Encoder socket node dll name
 */
/* ======================================================================= */
#ifdef UNDER_CE
    #define ILBCENC_DLL_NAME "\\windows\\ilbcenc_sn.dll64P"
#else
    #define ILBCENC_DLL_NAME "/ilbcenc_sn.dll64P"
#endif

/* ======================================================================= */
/** ILBCENC_StreamType  Stream types
*
*  @param  ILBCENC_DMM                  DMM
*
*  @param  ILBCENC_INSTRM               Input stream
*
*  @param  ILBCENC_OUTSTRM              Output stream
*/
/* ======================================================================= */
enum ILBCENC_StreamType {
    ILBCENC_DMM = 0,
    ILBCENC_INSTRM,
    ILBCENC_OUTSTRM
};

/* ======================================================================= */
/**
 * @def    ILBCENC_STREAM_COUNT    Number of streams
 */
/* ======================================================================= */
#define ILBCENC_STREAM_COUNT 2

/* ======================================================================= */
/**
 * @def _ERROR_PROPAGATION__     Allow Logic to Detec&Report Arm Errors
 */
/* ======================================================================= */
#define _ERROR_PROPAGATION__

/* ======================================================================= */
/** ILBCENC_COMP_PORT_TYPE  Port types
 *
 *  @param  ILBCENC_INPUT_PORT              Input port
 *
 *  @param  ILBCENC_OUTPUT_PORT         Output port
 */
/*  ====================================================================== */
/*This enum must not be changed. */
typedef enum ILBCENC_COMP_PORT_TYPE {
    ILBCENC_INPUT_PORT = 0,
    ILBCENC_OUTPUT_PORT
}ILBCENC_COMP_PORT_TYPE;


typedef enum ILBCENC_CODEC_TYPE {
    ILBCENC_PRIMARY_CODEC = 0,
    ILBCENC_SECONDARY_CODEC
}ILBCENC_CODEC_TYPE;

/* ======================================================================= */
/** ILBCENC_BUFFER_Dir  Buffer Direction
*
*  @param  ILBCENC_DIRECTION_INPUT      Input direction
*
*  @param  ILBCENC_DIRECTION_OUTPUT Output direction
*
*/
/* ======================================================================= */
typedef enum {
    ILBCENC_DIRECTION_INPUT,
    ILBCENC_DIRECTION_OUTPUT
}ILBCENC_BUFFER_Dir;

/* ======================================================================= */
/** ILBCENC_BUFFS  Buffer details
*
*  @param  BufHeader Buffer header
*
*  @param  Buffer   Buffer
*
*/
/* ======================================================================= */
typedef struct ILBCENC_BUFFS {
    char BufHeader;
    char Buffer;
}ILBCENC_BUFFS;

/* =================================================================================== */
/**
* Socket node Audio Codec Configuration parameters.
*/
/* =================================================================================== */
typedef struct ILBCENC_AudioCodecParams {
    unsigned long  iSamplingRate;
    unsigned long  iStrmId;
    unsigned short iAudioFormat;
}ILBCENC_AudioCodecParams;

/* =================================================================================== */
/**
* ILBCENC_UAlgInBufParamStruct      Input Buffer Param Structure
* usLastFrame                       To Send Last Buufer Flag
*/
/* =================================================================================== */
typedef struct {
        unsigned long int usLastFrame;
}ILBCENC_FrameStruct;

typedef struct{
         unsigned long int usNbFrames;
         ILBCENC_FrameStruct *pParamElem;
}ILBCENC_ParamStruct;

/* =================================================================================== */
/**
* ILBCENC_LCML_BUFHEADERTYPE Buffer Header Type
*/
/* =================================================================================== */
typedef struct ILBCENC_LCML_BUFHEADERTYPE {
      ILBCENC_BUFFER_Dir eDir;
      ILBCENC_FrameStruct *pFrameParam;
      ILBCENC_ParamStruct *pBufferParam;
      DMM_BUFFER_OBJ* pDmmBuf;
      OMX_BUFFERHEADERTYPE* buffer;
}ILBCENC_LCML_BUFHEADERTYPE;


typedef struct _ILBCENC_BUFFERLIST ILBCENC_BUFFERLIST;

/* =================================================================================== */
/**
* _ILBCENC_BUFFERLIST Structure for buffer list
*/
/* ================================================================================== */
struct _ILBCENC_BUFFERLIST{
    OMX_BUFFERHEADERTYPE sBufHdr;
    OMX_BUFFERHEADERTYPE *pBufHdr[ILBCENC_MAX_NUM_OF_BUFS];
    OMX_U32 bufferOwner[ILBCENC_MAX_NUM_OF_BUFS];
    OMX_U32 bBufferPending[ILBCENC_MAX_NUM_OF_BUFS];
    OMX_U16 numBuffers;
    ILBCENC_BUFFERLIST *pNextBuf;
    ILBCENC_BUFFERLIST *pPrevBuf;
};

/* =================================================================================== */
/**
* ILBCENC_PORT_TYPE Structure for PortFormat details
*/
/* =================================================================================== */
typedef struct ILBCENC_PORT_TYPE {
    OMX_HANDLETYPE hTunnelComponent;
    OMX_U32 nTunnelPort;
    OMX_BUFFERSUPPLIERTYPE eSupplierSetting;
    OMX_U8 nBufferCnt;
    OMX_AUDIO_PARAM_PORTFORMATTYPE* pPortFormat;
} ILBCENC_PORT_TYPE;

#ifdef UNDER_CE
    #ifndef _OMX_EVENT_
        #define _OMX_EVENT_
        typedef struct OMX_Event {
            HANDLE event;
        } OMX_Event;
    #endif
    int OMX_CreateEvent(OMX_Event *event);
    int OMX_SignalEvent(OMX_Event *event);
    int OMX_WaitForEvent(OMX_Event *event);
    int OMX_DestroyEvent(OMX_Event *event);
#endif

/* =================================================================================== */
/**
* ILBCENC_BUFDATA
*/
/* =================================================================================== */
typedef struct ILBCENC_BUFDATA {
   OMX_U8 nFrames;     
}ILBCENC_BUFDATA;

/* =================================================================================== */
/**
* ILBCENC_COMPONENT_PRIVATE Component private data Structure
*/
/* =================================================================================== */
typedef struct ILBCENC_COMPONENT_PRIVATE
{
    /** Array of pointers to BUFFERHEADERTYPE structues
       This pBufHeader[INPUT_PORT] will point to all the
       BUFFERHEADERTYPE structures related to input port,
       not just one structure. Same is the case for output
       port also. */
    OMX_BUFFERHEADERTYPE* pBufHeader[ILBCENC_NUM_OF_PORTS];
    
    /** Keep track of the number of buffers in order to 
        free them in clean init params function*/
    OMX_U32 nRuntimeInputBuffers;

    /** Keep track of the number of buffers in order to 
        free them in clean init params function*/
    OMX_U32 nRuntimeOutputBuffers;

    /** Structure of callback pointers */
    OMX_CALLBACKTYPE cbInfo;

    /** Handle for use with async callbacks */
    OMX_PORT_PARAM_TYPE* sPortParam;

    /** Pointer to port priority management structure */
    OMX_PRIORITYMGMTTYPE* sPriorityMgmt;

    /** This will contain info like how many buffers
        are there for input/output ports, their size etc, but not
        BUFFERHEADERTYPE POINTERS. */
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef[ILBCENC_NUM_OF_PORTS];

    /** Pointer to OMX_PORT_PARAM_TYPE structure */
    OMX_PORT_PARAM_TYPE* pPortParamType;

    /** PCM Component Parameters */
    OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams;

    /** GSMHR Component Parameters */
    OMX_AUDIO_PARAM_GSMEFRTYPE* ilbcParams;
    
    /** Array of pointer to port format structure details*/
    ILBCENC_PORT_TYPE *pCompPort[ILBCENC_NUM_OF_PORTS];
    
    /** LCML Buffer Header */
    ILBCENC_LCML_BUFHEADERTYPE *pLcmlBufHeader[ILBCENC_NUM_OF_PORTS];

    /** This is component handle */
    OMX_COMPONENTTYPE* pHandle;

    /** Current state of this component */
    OMX_STATETYPE curState;

    /** The component thread handle */
    pthread_t ComponentThread;

    /** The pipes for sending buffers to the thread */
    int dataPipe[2];

    /** The pipes for sending command to the thread */
    int cmdPipe[2];

    /** The pipes for sending cmd data to the thread */
    int cmdDataPipe[2];

    /** Flag to indicate dasf mode operation */
    OMX_U32 dasfMode;

    /** Flag for ACDN mode */
    OMX_U32 acdnMode;

    /** Flag to indicate multiframe mode support*/
    OMX_U32 nMultiFrameMode;

    /** Writing pipe Used for DSP_RENDERING_ON */
    OMX_S32 fdwrite;

    /** Writing pipe Used for DSP_RENDERING_ON */
    OMX_S32 fdread;

    /** Flag set when buffer should not be queued to the DSP */
    OMX_U32 bBypassDSP;
    
    /** Play Complete Flag */
    OMX_U32 bPlayCompleteFlag;

    /** Flag to prevent buffers from being sent while stop
        command has been issued */
    OMX_U32 bDspStoppedWhileExecuting;


    /** Flag to indicate the thread is stopping*/
    OMX_U32 bIsThreadstop;

    /** Count of number of buffers outstanding with bridge */
    OMX_U32 lcml_nIpBuf;

    /** Count of number of buffers outstanding with bridge */
    OMX_U32 lcml_nOpBuf;

    /** Count of number of buffers outstanding with app */
    OMX_U32 app_nBuf;

    /** Stream ID from AM */
    OMX_U32 streamID;

    /** Flag to indicate the thread started*/
    OMX_U32 bCompThreadStarted;

    /** Version of component*/
    OMX_U32 nVersion;

    /** For multiframe manage */
    OMX_U32 nHoldLength;

    /** FillThisBufferDone counter*/
    OMX_U32 nFillBufferDoneCount;

    /** FillThisBuffer counter*/
    OMX_U32 nFillThisBufferCount;

    /** EmptyThisBuffer counter*/
    OMX_U32 nEmptyThisBufferCount;

    /** EmptyThisBufferDone counter*/
    OMX_U32 nEmptyBufferDoneCount;

    /** Flag to indicate that initial parameter have been initialized*/
    OMX_U32 bInitParamsInitialized;

    /** Counter of IN buffers received while paused*/
    OMX_U32 nNumInputBufPending;

    /** Counter of OUT buffers received while paused*/
    OMX_U32 nNumOutputBufPending;

    /** For Disable command management*/
    OMX_U32 bDisableCommandPending;
    
    /** For Enable command management*/
    OMX_U32 bEnableCommandPending;

    /** For Disable command management*/
    OMX_U32 bDisableCommandParam;
    
    /** For Enable command management*/
    OMX_U32 bEnableCommandParam;

    /** LCML handle */
    OMX_HANDLETYPE pLcmlHandle;

    /** Flag to mark data on buffer */
    OMX_PTR pMarkData;

    /** Flag to mark data on buffer */
    OMX_MARKTYPE *pMarkBuf;

    /** Flag to mark data on buffer */
    OMX_HANDLETYPE hMarkTargetComponent;

    /** Pointer to the list of IN buffers*/
    ILBCENC_BUFFERLIST *pInputBufferList;

    /** Pointer to the list of OUT buffers*/
    ILBCENC_BUFFERLIST *pOutputBufferList;

    /** LCML stream*/
    LCML_STRMATTR *strmAttr;

    /** Date shared with SN */
    ILBCENC_AudioCodecParams *pParams;

    /** Component name*/
    OMX_STRING cComponentName;

    /** Component version*/
    OMX_VERSIONTYPE ComponentVersion;

    /** IN buffers received while paused*/
    OMX_BUFFERHEADERTYPE *pInputBufHdrPending[ILBCENC_MAX_NUM_OF_BUFS];

    /** OUT buffers received while paused*/
    OMX_BUFFERHEADERTYPE *pOutputBufHdrPending[ILBCENC_MAX_NUM_OF_BUFS];

    /** For multiframe managment*/
    OMX_U8 *pHoldBuffer,*pHoldBuffer2;

    /** For multiframe managment*/
    OMX_U8* iHoldBuffer;

    /** Index to codec type*/
    OMX_U16 codecType;

    /** Size of the IN ILBC ENC frame*/
    OMX_U32 inputFrameSize;

    /** Size of the IN ILBC ENC buffer*/
    OMX_U32 inputBufferSize;    

    /** Size of the OUT ILBC ENC frame*/
    OMX_U32 outputFrameSize;

    /** Size of the OUT ILBC ENC buffer*/
    OMX_U32 outputBufferSize;

    /** Flag to set when socket node stop callback should not transition
        component to OMX_StateIdle */
    OMX_U32 bNoIdleOnStop;

#ifndef UNDER_CE
    pthread_mutex_t AlloBuf_mutex;
    pthread_cond_t AlloBuf_threshold;
    OMX_U8 AlloBuf_waitingsignal;

    pthread_mutex_t InLoaded_mutex;
    pthread_cond_t InLoaded_threshold;
    OMX_U8 InLoaded_readytoidle;

    pthread_mutex_t InIdle_mutex;
    pthread_cond_t InIdle_threshold;
    OMX_U8 InIdle_goingtoloaded;

    pthread_mutex_t ToLoaded_mutex;
#else
    OMX_Event AlloBuf_event;
    OMX_U8 AlloBuf_waitingsignal;

    OMX_Event InLoaded_event;
    OMX_U8 InLoaded_readytoidle;

    OMX_Event InIdle_event;
    OMX_U8 InIdle_goingtoloaded;
#endif

    /** Number of frmaes sent*/
    OMX_U8 nNumOfFramesSent;

    /** Flag to track the EOS*/
    OMX_U8 InBuf_Eos_alreadysent;

#ifdef __PERF_INSTRUMENTATION__
    PERF_OBJHANDLE pPERF, pPERFcomp;
    OMX_U32 nLcml_nCntIp;
    OMX_U32 nLcml_nCntOpReceived;
#endif

    /** Flag to track the EOS*/
    OMX_BUFFERHEADERTYPE *LastOutbuf;

    /** Flag to track the and invalid state*/
    OMX_BOOL bIsInvalidState;
    
    /** Pointer to the device string*/
    OMX_STRING* sDeviceString;
    
    /** Pointer to the LCML library*/
    void* ptrLibLCML;

    /** Circular array to keep buffer timestamps */
    OMX_S64 arrBufIndex[ILBCENC_MAX_NUM_OF_BUFS]; 

    /** Index to arrBufIndex[], used for input buffer timestamps */
    OMX_U8 IpBufindex;

    /** Index to arrBufIndex[], used for output buffer timestamps */
    OMX_U8 OpBufindex;  

    /** Flag for transition to loaded management*/
    OMX_BOOL bLoadedCommandPending;
    
    /** Role of the component*/
    OMX_PARAM_COMPONENTROLETYPE componentRole;
    
    /** Pointer to RM callback */
#ifdef RESOURCE_MANAGER_ENABLED
RMPROXY_CALLBACKTYPE rmproxyCallback;
#endif

    /** Flag to mark PM/RM preemption */
    OMX_BOOL bPreempted;    

    OMX_BOOL DSPMMUFault;

} ILBCENC_COMPONENT_PRIVATE;


#ifndef UNDER_CE
    OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComp);
#else
/*  WinCE Implicit Export Syntax */
#define OMX_EXPORT __declspec(dllexport)
/* =================================================================================== */
/**
*  OMX_ComponentInit()  Initializes component
*
*
*  @param hComp         OMX Handle
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*
*/
/* =================================================================================== */
OMX_EXPORT OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComp);
#endif
/* =================================================================================== */
/**
*  ILBCENC_StartComponentThread()  Starts component thread
*
*
*  @param hComp         OMX Handle
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_StartComponentThread(OMX_HANDLETYPE pHandle);
/* =================================================================================== */
/**
*  ILBCENC_StopComponentThread()  Stops component thread
*
*
*  @param hComp         OMX Handle
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_StopComponentThread(OMX_HANDLETYPE pHandle);
/* =================================================================================== */
/**
*  ILBCENC_FreeCompResources()  Frees allocated memory
*
*
*  @param hComp         OMX Handle
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_FreeCompResources(OMX_HANDLETYPE pComponent);
/* =================================================================================== */
/**
*  ILBCENC_GetCorrespondingLCMLHeader()  Returns LCML header
* that corresponds to the given buffer
*
*  @param pComponentPrivate Component private data
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_GetCorrespondingLCMLHeader(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                                                  OMX_U8 *pBuffer, OMX_DIRTYPE eDir,
                                                  ILBCENC_LCML_BUFHEADERTYPE **ppLcmlHdr);
/* =================================================================================== */
/**
*  ILBCENC_LCMLCallback() Callback from LCML
*
*  @param event     Codec Event
*
*  @param args      Arguments from LCML
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_LCMLCallback(TUsnCodecEvent event, void * args [10]);
/* =================================================================================== */
/**
*  ILBCENC_FillLCMLInitParams() Fills the parameters needed
* to initialize the LCML
*
*  @param pHandle OMX Handle
*
*  @param plcml_Init LCML initialization parameters
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_FillLCMLInitParams(OMX_HANDLETYPE pHandle,
                                          LCML_DSP *plcml_Init,
                                          OMX_U16 arr[]);
/* =================================================================================== */
/**
*  ILBCENC_GetBufferDirection() Returns direction of pBufHeader
*
*  @param pBufHeader        Buffer header
*
*  @param eDir              Buffer direction
*
*  @param pComponentPrivate Component private data
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_GetBufferDirection(OMX_BUFFERHEADERTYPE *pBufHeader,
                                          OMX_DIRTYPE *eDir);
/* ===========================================================  */
/**
*  ILBCENC_HandleCommand()  Handles commands sent via SendCommand()
*
*  @param pComponentPrivate Component private data
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*/
/* =================================================================================== */
OMX_U32 ILBCENC_HandleCommand(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate);
/* =================================================================================== */
/**
*  ILBCENC_HandleDataBufFromApp()  Handles data buffers received
* from the IL Client
*
*  @param pComponentPrivate Component private data
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_HandleDataBufFromApp(OMX_BUFFERHEADERTYPE *pBufHeader,
                                            ILBCENC_COMPONENT_PRIVATE *pComponentPrivate);

/* =================================================================================== */
/**
*  ILBCENC_GetLCMLHandle()  Get the handle to the LCML
*
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*/
/* =================================================================================== */
OMX_HANDLETYPE ILBCENC_GetLCMLHandle(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate);
/* =================================================================================== */
/**
*  ILBCENC_FreeLCMLHandle()  Frees the handle to the LCML
*
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_FreeLCMLHandle(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate);
/* =================================================================================== */
/**
*  ILBCENC_CleanupInitParams()  Starts component thread
*
*  @param pComponent        OMX Handle
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_CleanupInitParams(OMX_HANDLETYPE pHandle);
/* =================================================================================== */
/**
*  ILBCENC_SetPending()  Called when the component queues a buffer
* to the LCML
*
*  @param pComponentPrivate     Component private data
*
*  @param pBufHdr               Buffer header
*
*  @param eDir                  Direction of the buffer
*
*  @return None
*/
/* =================================================================================== */
void ILBCENC_SetPending(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                         OMX_BUFFERHEADERTYPE *pBufHdr,
                         OMX_DIRTYPE eDir,
                         OMX_U32 lineNumber);
/* =================================================================================== */
/**
*  ILBCENC_ClearPending()  Called when a buffer is returned
* from the LCML
*
*  @param pComponentPrivate     Component private data
*
*  @param pBufHdr               Buffer header
*
*  @param eDir                  Direction of the buffer
*
*  @return None
*/
/* =================================================================================== */
void ILBCENC_ClearPending(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                           OMX_BUFFERHEADERTYPE *pBufHdr,
                           OMX_DIRTYPE eDir,
                           OMX_U32 lineNumber);
/* =================================================================================== */
/**
*  ILBCENC_IsPending()
*
*
*  @param pComponentPrivate     Component private data
*
*  @return OMX_ErrorNone = Successful
*          Other error code = fail
*/
/* =================================================================================== */
OMX_U32 ILBCENC_IsPending(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                           OMX_BUFFERHEADERTYPE *pBufHdr,
                           OMX_DIRTYPE eDir);
/* =================================================================================== */
/**
*  ILBCENC_FillLCMLInitParamsEx()  Fills the parameters needed
* to initialize the LCML without recreating the socket node
*
*  @param pComponent            OMX Handle
*
*  @return None
*/
/* =================================================================================== */
OMX_ERRORTYPE ILBCENC_FillLCMLInitParamsEx(OMX_HANDLETYPE pComponent);
/* =================================================================================== */
/**
*  ILBCENC_IsValid() Returns whether a buffer is valid
*
*
*  @param pComponentPrivate     Component private data
*
*  @param pBuffer               Data buffer
*
*  @param eDir                  Buffer direction
*
*  @return OMX_True = Valid
*          OMX_False= Invalid
*/
/* =================================================================================== */
OMX_U32 ILBCENC_IsValid(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate,
                         OMX_U8 *pBuffer,
                         OMX_DIRTYPE eDir);

/* =================================================================================== */
/**
*  iLBCENC_ResourceManagerCallback() Callback from Resource Manager
*
*  @param cbData    RM Proxy command data
*
*  @return None
*/
/* =================================================================================== */
#ifdef RESOURCE_MANAGER_ENABLED
 void ILBCENC_ResourceManagerCallback(RMPROXY_COMMANDDATATYPE cbData);
#endif

/*  =========================================================================*/
/*  func    ILBCENC_FatalErrorRecover
*
*   desc    handles the clean up and sets OMX_StateInvalid
*           in reaction to fatal errors
*
*   @return n/a
*
*  =========================================================================*/
void ILBCENC_FatalErrorRecover(ILBCENC_COMPONENT_PRIVATE *pComponentPrivate);

/* ================================================================================= */
/**
* @fn ILBCENC_CompThread() Component thread
*
*  @param pThreadData   pointer to handle
*/
/* ================================================================================ */
void* ILBCENC_CompThread(void* pThreadData);

/* ======================================================================= */
/** OMX_ILBCENC_INDEXAUDIOTYPE  Defines the custom configuration settings
*                              for the component
*
*  @param  OMX_IndexCustomILBCENCModeConfig      Sets the DASF mode
*
*
*/
/*  ==================================================================== */
typedef enum OMX_ILBCENC_INDEXAUDIOTYPE {
    OMX_IndexCustomILBCENCModeConfig = 0xFF000001,
    OMX_IndexCustomILBCENCStreamIDConfig,
    OMX_IndexCustomILBCENCDataPath,
    OMX_IndexParamAudioILBC,
    OMX_IndexCustomiLBCCodingType
}OMX_ILBCENC_INDEXAUDIOTYPE;


OMX_ERRORTYPE OMX_DmmMap(DSP_HPROCESSOR ProcHandle, int size, void* pArmPtr, DMM_BUFFER_OBJ* pDmmBuf);
OMX_ERRORTYPE OMX_DmmUnMap(DSP_HPROCESSOR ProcHandle, void* pMapPtr, void* pResPtr);

typedef enum
{
    ALGCMD_BITRATE = IUALG_CMD_USERSETCMDSTART,
    ALGCMD_DTX

} eSPEECHENCODE_AlgCmd;


#endif  /* OMX_ILBCENC_UTILS__H */

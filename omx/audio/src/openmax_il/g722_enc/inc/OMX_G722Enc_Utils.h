
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
 *             Texas Instruments OMAP (TM) Platform Software
 *  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
 *
 *  Use of this software is controlled by the terms and conditions found
 *  in the license agreement under which this software has been supplied.
 * =========================================================================== */
/**
 * @file OMX_G722Enc_Utils.h
 *
 * This header file contains data and function prototypes for G722 ENCODER OMX 
 *
 * @path  $(OMAPSW_MPU)\linux\audio\src\openmax_il\g722_enc\inc
 *
 * @rev  0.1
 */
/* ----------------------------------------------------------------------------- 
 *! 
 *! Revision History 
 *! ===================================
 *! Date         Author(s)            Version  Description
 *! ---------    -------------------  -------  ---------------------------------
 *! 08-Mar-2007  A.Donjon             0.1      Code update for G722 ENCODER
 *! 
 *!
 * ================================================================================= */

#include "LCML_DspCodec.h"
#include <OMX_TI_Common.h>
#include "OMX_G722Encoder.h"
#include "usn.h"
#define NEWSENDCOMMAND_MEMORY 123
/*#endif*/

#include <TIDspOmx.h>

#ifdef RESOURCE_MANAGER_ENABLED
#include <ResourceManagerProxyAPI.h>
#endif

/* ComponentThread constant */
#define EXIT_COMPONENT_THRD  10
/* ======================================================================= */
/**
 * @def    G722ENC_XXX_VER    Component version
 */
/* ======================================================================= */
#define G722ENC_MAJOR_VER 1
#define G722ENC_MINOR_VER 1

/* ======================================================================= */
/**
 * @def    NOT_USED    Defines a value for "don't care" parameters
 */
/* ======================================================================= */
#define NOT_USED 10

/* ======================================================================= */
/**
 * @def    NORMAL_BUFFER    Defines the flag value with all flags turned off
 */
/* ======================================================================= */
#define NORMAL_BUFFER 0

/* ======================================================================= */
/**
 * @def    OMX_G722ENC_DEFAULT_SEGMENT    Default segment ID for the LCML
 */
/* ======================================================================= */
#define OMX_G722ENC_DEFAULT_SEGMENT (0)


/* ======================================================================= */
/**
 * @def    OMX_G722ENC_SN_TIMEOUT    Timeout value for the socket node
 */
/* ======================================================================= */
#define OMX_G722ENC_SN_TIMEOUT (-1)

/* ======================================================================= */
/**
 * @def    OMX_G722ENC_SN_PRIORITY   Priority for the socket node
 */
/* ======================================================================= */
#define OMX_G722ENC_SN_PRIORITY (10)

/* ======================================================================= */
/**
 * @def    G722ENC_CPU   TBD, 50MHz for the moment
 */
/* ======================================================================= */
#define G722ENC_CPU (50)


/* ======================================================================= */
/**
 * @def    G722ENC_TIMEOUT_MILLISECONDS   Timeout value for the component thread
 */
/* ======================================================================= */
#define G722ENC_TIMEOUT_MILLISECONDS (1000)

/* ======================================================================= */
/**
 * @def    G722ENC_MAX_NUM_OF_BUFS   Maximum number of buffers
 */
/* ======================================================================= */
#define G722ENC_MAX_NUM_OF_BUFS 10


/* ======================================================================= */
/**
 * @def    USN_DLL_NAME   Path to the USN
 */
/* ======================================================================= */
#define USN_DLL_NAME "usn.dll64P"

/* ======================================================================= */
/**
 * @def    G722ENC_DLL_NAME   Path to the G722ENC SN
 */
/* ======================================================================= */
#define G722ENC_DLL_NAME "g722enc_sn.dll64P"

/* ======================================================================= */
/**
 * @def    DONT_CARE   Don't care value for the LCML initialization params
 */
/* ======================================================================= */
#define DONT_CARE 0


/* ======================================================================= */
/**
 * @def    G722ENC_DEBUG   Turns debug messaging on and off
 */
/* ======================================================================= */
#undef G722ENC_DEBUG
/*#define G722ENC_DEBUG*/

/* ======================================================================= */
/**
 * @def    G722ENC_MEMCHECK   Turns memory messaging on and off
 */
/* ======================================================================= */
#undef G722ENC_MEMCHECK     /* try to avoid the time out due to print message */


/* ======================================================================= */
/**
 * @def    G722ENC_DPRINT   Debug print macro
 */
/* ======================================================================= */

#ifdef  G722ENC_DEBUG
#define G722ENC_DPRINT(...)    fprintf(stdout,__VA_ARGS__)
#else
#define G722ENC_DPRINT(...)
#endif

#ifdef  G722ENC_MEMCHECK
#define G722ENC_MEMPRINT(...)    fprintf(stdout,__VA_ARGS__)
#else
#define G722ENC_MEMPRINT(...)
#endif

/* ======================================================================= */
/**
 * @def    G722ENC_NUM_OF_PORTS   Number of ports
 */
/* ======================================================================= */
#define G722ENC_NUM_OF_PORTS 2

/* ======================================================================= */
/**
 * @def    G722ENC_NUM_STREAMS   Number of streams
 */
/* ======================================================================= */
#define G722ENC_NUM_STREAMS 2

/* ======================================================================= */
/**
 * @def    G722ENC_NUM_INPUT_DASF_BUFFERS   Number of input buffers
 */
/* ======================================================================= */
#define G722ENC_NUM_INPUT_DASF_BUFFERS 2


/* ======================================================================= */
/**
 * @def    G722ENC_AM_DEFAULT_RATE   Default audio manager rate
 */
/* ======================================================================= */
#define G722ENC_AM_DEFAULT_RATE 48000

/* ======================================================================= */
/**
 * @def    G722ENC_SAMPLE_RATE      G722ENC SN sampling frequency
 */
/* ======================================================================= */
#define G722ENC_SAMPLE_RATE 16000

/* ======================================================================= */
/**
 *  M A C R O S FOR MALLOC and MEMORY FREE and CLOSING PIPES
 */
/* ======================================================================= */

#define OMX_G722CONF_INIT_STRUCT(_s_, _name_)   \
    memset((_s_), 0x0, sizeof(_name_));         \
    (_s_)->nSize = sizeof(_name_);              \
    (_s_)->nVersion.s.nVersionMajor = 0x1;      \
    (_s_)->nVersion.s.nVersionMinor = 0x1;      \
    (_s_)->nVersion.s.nRevision = 0x0;          \
    (_s_)->nVersion.s.nStep = 0x0

#define OMX_G722CLOSE_PIPE(_pStruct_,err)                       \
    G722ENC_DPRINT("%d :: CLOSING PIPE \n", __LINE__);          \
    err = close (_pStruct_);                                    \
    if(0 != err && OMX_ErrorNone == eError)                     \
    {                                                           \
        eError = OMX_ErrorHardware;                             \
        printf("%d :: Error while closing pipe\n", __LINE__);   \
        goto EXIT;                                              \
    }

/* ======================================================================= */
/** G722ENC_STREAM_TYPE  Values for create phase params
 *
 *  @param  G722ENCSTREAMDMM             Indicates DMM
 *
 *  @param  G722ENCSTREAMINPUT           Sets input stream
 *
 *  @param  G722ENCSTREAMOUTPUT          Sets output stream
 *
 */
/*  ==================================================================== */
typedef enum {
    G722ENCSTREAMDMM,
    G722ENCSTREAMINPUT,
    G722ENCSTREAMOUTPUT
} G722ENC_STREAM_TYPE;




/* ======================================================================= */
/** G722ENC_COMP_PORT_TYPE  Port definition for component
 *
 *  @param  G722ENC_INPUT_PORT           Index for input port
 *
 *  @param  G722ENC_OUTPUT_PORT          Index for output port
 *
 */
/*  ==================================================================== */
typedef enum G722ENC_COMP_PORT_TYPE {
    G722ENC_INPUT_PORT = 0,
    G722ENC_OUTPUT_PORT
}G722ENC_COMP_PORT_TYPE;



/* =================================================================================== */
/**
 * Socket node input buffer parameters.
 */
/* ================================================================================== */
typedef struct G722ENC_UAlgInBufParamStruct {
    unsigned long bLastBuffer;
}G722ENC_UAlgInBufParamStruct;

/* =================================================================================== */
/**
 * LCML data header.
 */
/* ================================================================================== */
typedef struct G722ENC_LCML_BUFHEADERTYPE {
    OMX_DIRTYPE eDir;
    OMX_BUFFERHEADERTYPE *pBufHdr;
    void *pOtherParams[10];
    G722ENC_UAlgInBufParamStruct *pIpParam;
    /*G722ENC_UAlgOutBufParamStruct *pOpParam; */
}G722ENC_LCML_BUFHEADERTYPE;


/* =================================================================================== */
/**
 * Socket node audio codec parameters
 */
/* ================================================================================== */
typedef struct G722ENC_AudioCodecParams
{
    unsigned long iSamplingRate;
    unsigned long iStrmId;
    unsigned short iAudioFormat;

}G722ENC_AudioCodecParams;

/* =================================================================================== */
/**
 * Structure for buffer list
 */
/* ================================================================================== */
typedef struct _BUFFERLIST G722ENC_BUFFERLIST;
struct _BUFFERLIST{
    OMX_BUFFERHEADERTYPE *pBufHdr[G722ENC_MAX_NUM_OF_BUFS];  /* records buffer header send by client */ 
    OMX_U32 bufferOwner[G722ENC_MAX_NUM_OF_BUFS];
    OMX_U32 numBuffers; 
    OMX_U32 bBufferPending[G722ENC_MAX_NUM_OF_BUFS];
};

/* =================================================================================== */
/**
 * Component private data
 */
/* ================================================================================== */
typedef struct G722ENC_COMPONENT_PRIVATE
{
    /** Array of pointers to BUFFERHEADERTYPE structues
        This pBufHeader[G722ENC_INPUT_PORT] will point to all the
        BUFFERHEADERTYPE structures related to input port,
        not just one structure. Same is for output port
        also. */
    OMX_BUFFERHEADERTYPE* pBufHeader[G722ENC_NUM_OF_PORTS];

    /** Structure of callback pointers */
    OMX_CALLBACKTYPE cbInfo;

    /** Handle for use with async callbacks */
    OMX_PORT_PARAM_TYPE sPortParam;

    /** Input port parameters */
    OMX_AUDIO_PARAM_PORTFORMATTYPE* pInPortFormat;

    /** Output port parameters */
    OMX_AUDIO_PARAM_PORTFORMATTYPE* pOutPortFormat;

    /** Keeps track of whether a buffer is owned by the 
        component or by the IL client */
    OMX_U32 bIsBufferOwned[G722ENC_NUM_OF_PORTS];
    
    /* Audio codec parameters structure */
    G722ENC_AudioCodecParams *pParams;

    /** This will contain info like how many buffers
        are there for input/output ports, their size etc, but not
        BUFFERHEADERTYPE POINTERS. */
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef[G722ENC_NUM_OF_PORTS];
    OMX_AUDIO_PARAM_ADPCMTYPE* g722Params;
    OMX_AUDIO_PARAM_ADPCMTYPE* pcmParams;
    OMX_PRIORITYMGMTTYPE* sPriorityMgmt;

    /** This is component handle */
    OMX_COMPONENTTYPE* pHandle;

    /** Current state of this component */
    OMX_STATETYPE curState;

    /** The component thread handle */
    pthread_t ComponentThread;

    /** The pipes for sending buffers to the thread */
    int dataPipe[2];

    /** The pipes for sending command data to the thread */
    int cmdDataPipe[2];

    /** The pipes for sending buffers to the thread */
    int cmdPipe[2];

    /** The pipes for sending buffers to the thread */
    int lcml_Pipe[2];

    /** Set to indicate component is stopping */
    OMX_U32 bIsStopping;

    OMX_U32 bIsEOFSent;

    /** Count of number of buffers outstanding with bridge */
    OMX_U32 lcml_nIpBuf;

    /** Count of number of buffers outstanding with bridge */
    OMX_U32 lcml_nOpBuf;

    /** Count of buffers sent to the LCML */
    OMX_U32 lcml_nCntIp;

    /** Count of buffers received from the LCML */
    OMX_U32 lcml_nCntOpReceived;

    /** Count of buffers pending from the app */
    OMX_U32 app_nBuf;

    /** Flag for DASF mode */
    OMX_U32 dasfmode;

    /** Audio Stream ID */
    OMX_U32 streamID;

    /** LCML Handle */
    OMX_HANDLETYPE pLcmlHandle;

    /** LCML Buffer Header */
    G722ENC_LCML_BUFHEADERTYPE *pLcmlBufHeader[2];

    /** Tee Mode Flag */
    OMX_U32 teemode;   

    /** Flag set when port definitions are allocated */
    OMX_U32 bPortDefsAllocated;

    /** Flag set when component thread is started */
    OMX_U32 bCompThreadStarted;

    /** Mark data */
    OMX_PTR pMarkData; 
    
    /** Mark buffer */
    OMX_MARKTYPE *pMarkBuf;    

    /** Mark target component */
    OMX_HANDLETYPE hMarkTargetComponent; 

    /** Flag set when buffer should not be queued to the DSP */
    OMX_U32 bBypassDSP; 

    /** Create phase arguments */
    OMX_U16 *pCreatePhaseArgs;

    /** Input buffer list */
    G722ENC_BUFFERLIST *pInputBufferList;

    /** Output buffer list */
    G722ENC_BUFFERLIST *pOutputBufferList;

    /** LCML stream attributes */
    LCML_STRMATTR *strmAttr;

    /** Component version */
    OMX_U32 nVersion;

    /** LCML Handle */
    void *lcml_handle;

    /** Number of initialized input buffers */
    int noInitInputBuf;

    /** Number of initialized output buffers */
    int noInitOutputBuf;

    /** Flag set when LCML handle is opened */
    int bLcmlHandleOpened;

    /** Flag set when initialization params are set */
    OMX_U32 bInitParamsInitialized;

    /** Pipe write handle for audio manager */
    int fdwrite;

    /** Pipe read handle for audio manager */
    int fdread;

    /** Stores input buffers while paused */
    OMX_BUFFERHEADERTYPE *pInputBufHdrPending[G722ENC_MAX_NUM_OF_BUFS];

    /** Number of input buffers received while paused */
    OMX_U32 nNumInputBufPending;

    /** Stores output buffers while paused */   
    OMX_BUFFERHEADERTYPE *pOutputBufHdrPending[G722ENC_MAX_NUM_OF_BUFS];

    /** Number of output buffers received while paused */
    OMX_U32 nNumOutputBufPending;

    /** Keeps track of the number of invalid frames that come from the LCML */
    OMX_U32 nInvalidFrameCount;

    /** Flag set when a disable command is pending */
    OMX_U32 bDisableCommandPending;

    /** Parameter for pending disable command */
    OMX_U32 bDisableCommandParam;

    /** Flag to set when socket node stop callback should not transition
        component to OMX_StateIdle */
    OMX_U32 bNoIdleOnStop;

    /** Flag set when idle command is pending */
    OMX_U32 bIdleCommandPending;

    /** Flag set when socket node is stopped */
    OMX_U32 bDspStoppedWhileExecuting;

    /** Number of outstanding FillBufferDone() calls */
    OMX_U32 nOutStandingFillDones;

    /** Flag set when StrmCtrl has been called */
    OMX_U32 bStreamCtrlCalled;

    OMX_PARAM_COMPONENTROLETYPE componentRole;
    OMX_STRING* sDeviceString;
    OMX_BOOL bLoadedCommandPending;
    OMX_BOOL DSPMMUFault;

    /** Holds the value of RT Mixer mode  */ 
    OMX_U32 rtmx; 
    TI_OMX_DSP_DEFINITION tiOmxDspDefinition;

    /* Removing sleep() calls. Definition. */
    pthread_mutex_t AlloBuf_mutex;    
    pthread_cond_t AlloBuf_threshold;
    OMX_U8 AlloBuf_waitingsignal;
    
    pthread_mutex_t codecStop_mutex;
    pthread_cond_t codecStop_threshold;
    OMX_U8 codecStop_waitingsignal;

    pthread_mutex_t InLoaded_mutex;
    pthread_cond_t InLoaded_threshold;
    OMX_U8 InLoaded_readytoidle;
    
    pthread_mutex_t InIdle_mutex;
    pthread_cond_t InIdle_threshold;
    OMX_U8 InIdle_goingtoloaded;

#ifdef __PERF_INSTRUMENTATION__
    PERF_OBJHANDLE pPERF, pPERFcomp;
    OMX_U32 nLcml_nCntIp;         
    OMX_U32 nLcml_nCntOpReceived;
#endif

    /** Keep buffer timestamps **/ 
    OMX_S64 arrTimestamp[G722ENC_MAX_NUM_OF_BUFS]; 
    /** Keep buffer nTickCounts **/ 
    OMX_S64 arrTickCount[G722ENC_MAX_NUM_OF_BUFS]; 
    /** Index to arrTimestamp[], used for input buffer timestamps */ 
    OMX_U8 IpBufindex; 
    /** Index to arrTimestamp[], used for output buffer timestamps */ 
    OMX_U8 OpBufindex; 
    

    OMX_BOOL bPreempted;
    OMX_BOOL bMutexInitDone;

    /** Pointer to RM callback **/
#ifdef RESOURCE_MANAGER_ENABLED
    RMPROXY_CALLBACKTYPE rmproxyCallback;
#endif

} G722ENC_COMPONENT_PRIVATE;

/* ===========================================================  */
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
/*================================================================== */
OMX_ERRORTYPE OMX_ComponentInit (OMX_HANDLETYPE hComp);

/* ===========================================================  */
/**
 *  G722ENC_Fill_LCMLInitParams() Fills the parameters needed
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
/*================================================================== */
OMX_ERRORTYPE G722ENC_Fill_LCMLInitParams(OMX_HANDLETYPE pHandle,
                                          LCML_DSP *plcml_Init);

/* ===========================================================  */
/**
 *  G722ENC_GetBufferDirection() Returns direction of pBufHeader
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
/*================================================================== */
OMX_ERRORTYPE G722ENC_GetBufferDirection(OMX_BUFFERHEADERTYPE *pBufHeader,
                                         OMX_DIRTYPE *eDir,
                                         G722ENC_COMPONENT_PRIVATE *pComponentPrivate);

/* ===========================================================  */
/**
 *  G722ENC_LCML_Callback() Callback from LCML
 *
 *  @param event     Codec Event
 *
 *  @param args      Arguments from LCML
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_LCML_Callback (TUsnCodecEvent event,void * args [10]);

/* ===========================================================  */
/**
 *  G722ENC_HandleCommand() Handles commands sent via SendCommand()
 *
 *  @param pComponentPrivate Component private data
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_U32 G722ENC_HandleCommand (G722ENC_COMPONENT_PRIVATE *pComponentPrivate);


/* ===========================================================  */
/**
 *  G722ENC_HandleDataBuf_FromApp() Handles data buffers received
 * from application
 *
 *  @param pBufHeader        Buffer header
 *
 *  @param pComponentPrivate Component private data
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_HandleDataBuf_FromApp(OMX_BUFFERHEADERTYPE *pBufHeader,
                                            G722ENC_COMPONENT_PRIVATE *pComponentPrivate);



/* ===========================================================  */
/**
 *  G722ENC_HandleDataBuf_FromLCML() Handles data buffers received
 * from LCML
 *
 *  @param pComponentPrivate Component private data
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
/***** NO LCML *******
       OMX_ERRORTYPE G722ENC_HandleDataBuf_FromLCML(G722ENC_COMPONENT_PRIVATE*
       pComponentPrivate);
**********************/
/* ===========================================================  */
/**
 *  GetLCMLHandle() 
 *
 *  @param 
 *
 *  @return *         
 */
/*================================================================== */
OMX_HANDLETYPE GetLCMLHandle();


/* ===========================================================  */
/**
 *  G722ENC_GetCorresponding_LCMLHeader()  Returns LCML header
 * that corresponds to the given buffer
 *
 *  @param pComponentPrivate Component private data
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_GetCorresponding_LCMLHeader(OMX_U8 *pBuffer,
                                                  OMX_DIRTYPE eDir,
                                                  G722ENC_LCML_BUFHEADERTYPE **ppLcmlHdr);


/* ===========================================================  */
/**
 *  G722Enc_FreeCompResources()  Frees component resources
 *
 *  @param pComponent        OMX Handle
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722Enc_FreeCompResources(OMX_HANDLETYPE pComponent);

/* ===========================================================  */
/**
 *  G722Enc_StartCompThread()  Starts component thread
 *
 *  @param pComponent        OMX Handle
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722Enc_StartCompThread(OMX_HANDLETYPE pComponent);

/* ===========================================================  */
/**
 *  G722ENC_GetLCMLHandle()  Returns handle to the LCML
 *
 *
 *  @return Handle to the LCML
 */
/*================================================================== */
OMX_HANDLETYPE G722ENC_GetLCMLHandle();


/* ========================================================================== */
/**
 * @G722ENC_StopComponentThread() This function is called by the component during
 * de-init to close component thread.
 *
 * @param pComponent  handle for this instance of the component
 *
 * @pre
 *
 * @post
 *
 * @return none
 */
/* ========================================================================== */
OMX_ERRORTYPE G722ENC_StopComponentThread(OMX_HANDLETYPE pComponent);

/* ===========================================================  */
/**
 *  G722ENC_FreeLCMLHandle()  Frees the handle to the LCML
 *
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_FreeLCMLHandle();


/* ===========================================================  */
/**
 *  G722ENC_CleanupInitParams()  Starts component thread
 *
 *  @param pComponent        OMX Handle
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_CleanupInitParams(OMX_HANDLETYPE pComponent);


/* ===========================================================  */
/**
 *  G722ENC_CommandToIdle()  Called when the component is commanded
 * to idle
 *
 *  @param pComponentPrivate     Component private data
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_CommandToIdle(G722ENC_COMPONENT_PRIVATE *pComponentPrivate);

/* ===========================================================  */
/**
 *  G722ENC_CommandToIdle()  Called when the component is commanded
 * to idle
 *
 *  @param pComponentPrivate     Component private data
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_CommandToLoaded(G722ENC_COMPONENT_PRIVATE *pComponentPrivate);

/* ===========================================================  */
/**
 *  G722ENC_CommandToExecuting()  Called when the component is commanded
 * to executing
 *
 *  @param pComponentPrivate     Component private data
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_CommandToExecuting(G722ENC_COMPONENT_PRIVATE *pComponentPrivate);

/* ===========================================================  */
/**
 *  G722ENC_CommandToPause()  Called when the component is commanded
 * to paused
 *
 *  @param pComponentPrivate     Component private data
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_CommandToPause(G722ENC_COMPONENT_PRIVATE *pComponentPrivate);

/* ===========================================================  */
/**
 *  G722ENC_CommandToWaitForResources()  Called when the component is commanded
 * to WaitForResources
 *
 *  @param pComponentPrivate     Component private data
 *
 *  @return OMX_ErrorNone = Successful
 *          Other error code = fail
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_CommandToWaitForResources(G722ENC_COMPONENT_PRIVATE *pComponentPrivate);

/* ===========================================================  */
/**
 *  G722ENC_SetPending()  Called when the component queues a buffer
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
/*================================================================== */
void G722ENC_SetPending(G722ENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir);

/* ===========================================================  */
/**
 *  G722ENC_ClearPending()  Called when a buffer is returned
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
/*================================================================== */
void G722ENC_ClearPending(G722ENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir) ;

/* ===========================================================  */
/**
 *  G722ENC_IsPending()  Returns the status of a buffer
 *
 *  @param pComponentPrivate     Component private data
 *
 *  @param pBufHdr               Buffer header
 *
 *  @param eDir                  Direction of the buffer
 *
 *  @return None
 */
/*================================================================== */
OMX_U32 G722ENC_IsPending(G722ENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE *pBufHdr, OMX_DIRTYPE eDir);

/* ===========================================================  */
/**
 * G722ENC_Fill_LCMLInitParamsEx()  Fills the parameters needed
 * to initialize the LCML without recreating the socket node
 *
 *  @param pComponent            OMX Handle
 *
 *  @return None
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_Fill_LCMLInitParamsEx(OMX_HANDLETYPE pComponent);

/* ===========================================================  */
/**
 *  G722ENC_IsValid() Returns whether the buffer is a valid buffer
 *
 *  @param pComponentPrivate     Component private data
 *
 *  @param pBuffer               Buffer 
 *
 *  @param eDir                  Direction of the buffer
 *
 *  @return None
 */
/*================================================================== */
OMX_U32 G722ENC_IsValid(G722ENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U8 *pBuffer, OMX_DIRTYPE eDir) ;

/* ===========================================================  */
/**
 *  G722ENC_TransitionToIdle() Transitions component to idle
 * 
 *
 *  @param pComponentPrivate     Component private data
 *
 *  @return OMX_ErrorNone = No error
 *          OMX Error code = Error
 */
/*================================================================== */
OMX_ERRORTYPE G722ENC_TransitionToIdle(G722ENC_COMPONENT_PRIVATE *pComponentPrivate);

#ifdef RESOURCE_MANAGER_ENABLED
/***********************************
 *  Callback to the RM                                       *
 ***********************************/
void G722ENC_ResourceManagerCallback(RMPROXY_COMMANDDATATYPE cbData);
#endif

/*  =========================================================================*/
/*  func    G722ENC_FatalErrorRecover
*
*   desc    handles the clean up and sets OMX_StateInvalid
*           in reaction to fatal errors
*
*@return n/a
*
*  =========================================================================*/
void G722ENC_FatalErrorRecover(G722ENC_COMPONENT_PRIVATE *pComponentPrivate);

/*void printEmmEvent (TUsnCodecEvent event);*/

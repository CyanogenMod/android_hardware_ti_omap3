
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
* @file OMX_iLBCDec_Utils.h
*
* This is an header file for an audio iLBC decoder that is fully
* compliant with the OMX Audio specification.
* This the file is used internally by the component
* in its code.
*
* @path  $(CSLPATH)\OMAPSW_MPU\linux\audio\src\openmax_il\iLBC_dec\inc\
*
* @rev 1.0
*/
/* --------------------------------------------------------------------------- */
#ifndef OMX_iLBCDEC_UTILS__H
#define OMX_iLBCDEC_UTILS__H

#include "LCML_DspCodec.h"
#include <pthread.h>
#include "TIDspOmx.h"
#include "OMX_TI_Common.h"
#ifdef RESOURCE_MANAGER_ENABLED
    #include <ResourceManagerProxyAPI.h>
#endif
    #define NEWSENDCOMMAND_MEMORY 123

/*#undef DSP_RENDERING_ON*/

/*#define iLBCDEC_DEBUG */       /* See all debug statement of the component */
/*#define iLBCDEC_MEMDETAILS **/     /* See memory details of the component */
/*#define iLBCDEC_BUFDETAILS **/     /* See buffers details of the component */
/*#define _ERROR_PROPAGATION__ **/
/*#define iLBCDEC_STATEDETAILS **/   /* See all state transitions of the component */

#define iLBCD_INPUT_BUFFER_SIZE  4096 /* Default size of input buffer */
#define iLBCDEC_BUFHEADER_VERSION 0x0 /* Version of the buffer header struct */
#define INVALID_SAMPLING_FREQ  51
#define DONT_CARE 0 /* Value unused or ignored */
#define iLBCDEC_CPU 50 /* TBD, 50MHz for the moment */

/* ========================================================================== */
/**
 * @def    FIFO1, FIFO2              Define Fifo Path
 */
/* ========================================================================== */
#define FIFO1 "/dev/fifo.1"
#define FIFO2 "/dev/fifo.2"
/******************************************************************************/

/* ========================================================================== */
/**
 * @def    iLBC_MAJOR_VER              Define value for "major" version
 */
/* ========================================================================== */
#define iLBCDEC_MAJOR_VER 0xF1 

/* ========================================================================== */
/**
 * @def    iLBC_MINOR_VER              Define value for "minor" version
 */
/* ========================================================================== */
#define iLBCDEC_MINOR_VER 0xF2 

/* ========================================================================== */
/**
 * @def    NOT_USED                            Define a not used value
 */
/* ========================================================================== */
#define NOT_USED 10

/* ========================================================================== */
/**
 * @def    NORMAL_BUFFER                       Define a normal buffer value
 */
/* ========================================================================== */
#define NORMAL_BUFFER 0 

/* ========================================================================== */
/**
 * @def    OMX_iLBC_DEFAULT_SEGMENT        Define the default segment
 */
/* ========================================================================== */
#define OMX_iLBCDEC_DEFAULT_SEGMENT (0) /* Default segment ID */

/* ========================================================================== */
/**
 * @def    OMX_iLBC_SN_TIMEOUT            Define a value for SN Timeout
 */
/* ========================================================================== */
#define OMX_iLBCDEC_SN_TIMEOUT (-1) /* timeout, wait until ack is received */

/* ========================================================================== */
/**
 * @def    OMX_iLBC_SN_PRIORITY           Define a value for SN Priority
 */
/* ========================================================================== */
#define OMX_iLBCDEC_SN_PRIORITY (10) /* Priority used by DSP */

/* ========================================================================== */
/**
 * @def    OMX_iLBC_NUM_DLLS              Define a num of DLLS to be used
 */
/* ========================================================================== */
#define OMX_iLBC_NUM_DLLS (2)   /* non iLBC */

/* ========================================================================== */
/**
 * @def    iLBCD_TIMEOUT   Default timeout used to come out of blocking calls
 */
/* ========================================================================== */
#define iLBCD_TIMEOUT (1000) /* millisecs */

/* ========================================================================== */
/**
 * @def    NUM_iLBCDEC_INPUT_BUFFERS              Number of Input Buffers
 */
/* ========================================================================== */
#define iLBCD_NUM_INPUT_BUFFERS 1 

/* ========================================================================== */
/**
 * @def    NUM_iLBCDEC_OUTPUT_BUFFERS              Number of Output Buffers
 */
/* ========================================================================== */
#define iLBCD_NUM_OUTPUT_BUFFERS 1 

/* ========================================================================== */
/**
 * @def    NUM_iLBCDEC_OUTPUT_BUFFERS_DASF         Number of Output Buffers
 *                                                  on DASF mode
 */
/* ========================================================================== */
#define NUM_iLBCDEC_OUTPUT_BUFFERS_DASF 2 /* 0j0 non iLBC */

/* ========================================================================== */
/**
 * @def    OUTPUT_iLBCDEC_BUFFER_SIZE           Standart Output Buffer Size
 */
/* ========================================================================== */
#define iLBCD_OUTPUT_BUFFER_SIZE 320 /*480 * Default size of output buffer */
#define OUTPUT_iLBCDEC_SECBUF_SIZE 480

/* ========================================================================== */
/**
 * @def    STD_iLBCDEC_BUF_SIZE                  Standart Input Buffer Size
 */
/* ========================================================================== */
#define STD_iLBCDEC_BUF_SIZE 38 /* 34 * 0j0 non iLBC */
#define INPUT_iLBCDEC_SECBUF_SIZE 50

/* ========================================================================== */
/**
 * @def    RTP_Framesize                          Size in Bytes of determined
 *                                               frame.
 */
/* ========================================================================== */
#define RTP_Framesize 34 /* 0j0 non iLBC */

/* ========================================================================== */
/**
 * @def    STREAM_COUNT                         Stream Count value for
 *                                              LCML init.
 */
/* ========================================================================== */
#define STREAM_COUNT 2 /* 0j0 non iLBC */

/* ========================================================================== */
/**
 * @def    INPUT_STREAM_ID                      Input Stream ID
 */
/* ========================================================================== */
#define INPUT_STREAM_ID 0 /* 0j0 non iLBC */

/* ========================================================================== */
/**
 * @def    EXIT_COMPONENT_THRD              Define Exit Componet Value
 */
/* ========================================================================== */
#define EXIT_COMPONENT_THRD  10

/* ========================================================================== */
/**
 * @def    iLBCDEC_SAMPLING_FREQUENCY          Sampling Frequency
 */
/* ========================================================================== */
#define iLBCDEC_SAMPLING_FREQUENCY 8000 /* 0j0 non iLBC */

/* ========================================================================== */
/**
 * @def    iLBCDEC_CPU_LOAD                    CPU Load in MHz
 */
/* ========================================================================== */
#define iLBCDEC_CPU_LOAD 10 /* 0j0 non iLBC */

/* ========================================================================== */
/**
 * @def    MAX_NUM_OF_BUFS                      Max Num of Bufs Allowed
 */
/* ========================================================================== */
#define MAX_NUM_OF_BUFS 10 /* Max number of buffers used */

/* ========================================================================== */
/**
 * @def    iLBC_USN_DLL_NAME             Path & Name of USN DLL to be used
 *                                           at initialization
 */
/* ========================================================================== */
#define USN_DLL_NAME "/usn.dll64P" /* Path of USN DLL */

/* ========================================================================== */
/**
 * @def    iLBC_USN_DLL_NAME             Path & Name of DLL to be useda
 *                                           at initialization
 */
/* ========================================================================== */
#define iLBCDEC_DLL_NAME "/ilbcdec_sn.dll64P" /* "/lib/dsp/iLBCdec_sn.dll64P" * Path of iLBC SN DLL */

/* ========================================================================== */
/**
 * @def    NUM_OF_PORTS                       Number of Comunication Port
 */
/* ========================================================================== */
#define NUM_OF_PORTS 2 /* Number of ports of component */

/* ========================================================================== */
/**
 * @def    _ERROR_PROPAGATION__              Allow Logic to Detec Arm Errors
 */
/* ========================================================================== */
/*#define _ERROR_PROPAGATION__ * 0j0 non iLBC */

/* ============================================================================== * */
/** iLBCD_COMP_PORT_TYPE  describes the input and output port of indices of the
* component.
*
* @param  iLBCD_INPUT_PORT  Input port index
*
* @param  iLBCD_OUTPUT_PORT Output port index
*/
/* ============================================================================ * */
typedef enum iLBCD_COMP_PORT_TYPE {
    iLBCD_INPUT_PORT = 0,
    iLBCD_OUTPUT_PORT
}iLBCD_COMP_PORT_TYPE;

/* ========================================================================== */
/** iLBCDEC_StreamType  StreamType
*
*  @param  iLBCDEC_DMM                    Stream Type DMM
*
*  @param  iLBCDEC_INSTRM                Stream Type Input
*
*  @param  iLBCDEC_OUTSTRM             Stream Type Output
*/
/*  ========================================================================= */
enum iLBCDEC_StreamType
{
    iLBCDEC_DMM,
    iLBCDEC_INSTRM,
    iLBCDEC_OUTSTRM
};

/* ========================================================================== */
/** iLBCDEC_DecodeType  Decode Type Mode
*
*  @param  iLBC                    OMX_AUDIO_iLBCDTX
*
*  @param  iLBCDEC_EFR                OMX_AUDIO_iLBCDTX as EFR
*/
/*  ========================================================================= */
enum iLBCDEC_DecodeType
{
    iLBC,
    iLBCDEC_EFR
};

/* ========================================================================== */
/** iLBCDEC_MimeMode  Mime Mode
*
*  @param  iLBCDEC_NONMIMEMODE                Mime Mode Off
*
*  @param  iLBCDEC_MIMEMODE                Mime Mode On
*/
/*  ========================================================================= */
enum iLBCDEC_MimeMode {
    iLBCDEC_NONMIMEMODE,
    iLBCDEC_MIMEMODE
};


/* ========================================================================== */
/** iLBCDEC_BUFFER_Dir  Direction of the Buffer
*
*  @param  iLBCDEC_DIRECTION_INPUT                Direction Input
*
*  @param  iLBCDEC_DIRECTION_INPUT                Direction Output
*/
/*  ========================================================================= */
typedef enum {
    iLBCDEC_DIRECTION_INPUT,
    iLBCDEC_DIRECTION_OUTPUT
} iLBCDEC_BUFFER_Dir;

/* ========================================================================== */
/**
*  Buffer Information.
*/
/* ========================================================================== */
typedef struct BUFFS
{
    OMX_S8 BufHeader;
    OMX_S8 Buffer;
}BUFFS;

/* ========================================================================== */
/**
* iLBC Buffer Header Type Info.
*/
/* ========================================================================== */
typedef struct BUFFERHEADERTYPE_INFO
{
    OMX_BUFFERHEADERTYPE* pBufHeader[MAX_NUM_OF_BUFS];
    BUFFS bBufOwner[MAX_NUM_OF_BUFS];
}BUFFERHEADERTYPE_INFO;

/* ========================================================================== */
/** LCML_MimeMode  modes
*
*  @param  MODE_MIME                    Mode MIME
*
*  @param  MODE_NONMIME                    Mode NONMIME
*/
/*  ========================================================================= */
typedef enum {
    MODE_MIME,
    MODE_NONMIME
}LCML_MimeMode;

/* ======================================================================= */
/** iLBCD_USN_AudioCodecParams: This contains the information which does to Codec
 * on DSP
 * are sent to DSP.
*/
/* ==================================================================== */
typedef struct iLBCD_AudioCodecParams{
    unsigned long ulSamplingFreq;   /* Specifies the sample frequency */
    unsigned long unUUID;           /* Specifies the UUID */
    unsigned short unAudioFormat;   /* Specifies the audioformat */
}iLBCD_AudioCodecParams;


/* ========================================================================== */
/**
* Socket node alg parameters..
*/
/* ========================================================================== */

typedef struct {        /* Adjusted iLBC */
        unsigned long int usLastFrame;
        unsigned long int usFrameLost;
}iLBCDEC_FrameStruct;


/* ======================================================================= */
/** iLBCDEC_ParamStruct: This struct is passed with input buffers that
 * are sent to DSP.
*/
/* ==================================================================== */
typedef struct{
         unsigned long int usNbFrames;
         iLBCDEC_FrameStruct *pParamElem;
}iLBCDEC_ParamStruct;

/* ======================================================================= */
/** iLBCDEC_UAlgOutBufParamStruct: This is passed with output buffer to DSP.
*/
/* ==================================================================== */
typedef struct {
    /* Number of frames in a buffer */
    unsigned long ulFrameCount;
    unsigned short usLastFrame;
}iLBCDEC_UAlgOutBufParamStruct;

/* ======================================================================= */
/** iLBCDEC_UAlgInBufParamStruct: This struct is passed with input buffers that
 * are sent to DSP.
*/
/* ==================================================================== */
typedef struct {
    /* Set to 1 if buffer is last buffer */
    unsigned long bLastBuffer;
}iLBCDEC_UAlgInBufParamStruct;

/* ======================================================================= */
/** iLBCD_AUDIODEC_PORT_TYPE: This contains component port information.
*
* @see OMX_AUDIO_PARAM_PORTFORMATTYPE
*/
/* ==================================================================== */
typedef struct AUDIODEC_PORT_TYPE {
    /* Used in tunneling, this is handle of tunneled component */
    OMX_HANDLETYPE hTunnelComponent;
    /* Port which has to be tunneled */
    OMX_U32 nTunnelPort;
    /* Buffer Supplier Information */
    OMX_BUFFERSUPPLIERTYPE eSupplierSetting;
    /* Number of buffers */
    OMX_U8 nBufferCnt;
    /* Port format information */
    OMX_AUDIO_PARAM_PORTFORMATTYPE* pPortFormat;
} iLBCD_AUDIODEC_PORT_TYPE;


/* ========================================================================== */
/**
* LCML_iLBCDEC_BUFHEADERTYPE
*/
/* ========================================================================== */
typedef struct iLBCD_LCML_BUFHEADERTYPE {   
    OMX_DIRTYPE eDir;                           /* Direction whether input or output buffer */
    OMX_BUFFERHEADERTYPE *buffer; /*pBufHdr;              * Pointer to OMX Buffer Header */
    /* void *pOtherParams[10];                     * Other parameters, may be useful for enhancements */
    iLBCDEC_FrameStruct *pFrameParam;
    iLBCDEC_ParamStruct *pBufferParam;
    DMM_BUFFER_OBJ* pDmmBuf;
    /***********************/
    iLBCDEC_UAlgOutBufParamStruct *pIpParam;     /* Input Parameter Information structure */
    iLBCDEC_UAlgOutBufParamStruct *pOpParam;    /* Output Parameter Information structure */
}iLBCD_LCML_BUFHEADERTYPE;

typedef struct _iLBCD_BUFFERLIST iLBCD_BUFFERLIST;

/* ======================================================================= */
/** _iLBCD_BUFFERLIST: This contains information about a buffer's owner whether
 * it is application or component, number of buffers owned etc.
*
* @see OMX_BUFFERHEADERTYPE
*/
/* ==================================================================== */
struct _iLBCD_BUFFERLIST{    
    OMX_BUFFERHEADERTYPE *pBufHdr[MAX_NUM_OF_BUFS]; /* Array of pointer to OMX buffer headers */    
    OMX_U32 bufferOwner[MAX_NUM_OF_BUFS];           /* Array that tells about owner of each buffer */
    OMX_U32 bBufferPending[MAX_NUM_OF_BUFS];   
    OMX_U32 numBuffers;                             /* Number of buffers  */
};

/* ========================================================================== */
/*
 * GSMFRDEC_BUFDATA
 */
/* ========================================================================== */
typedef struct iLBCDEC_BUFDATA {
   OMX_U8 nFrames;     
}iLBCDEC_BUFDATA;

#ifdef  iLBCDEC_DEBUG

    #define iLBCDEC_DPRINT(...)  fprintf(stdout, "%s %d::  ",__FUNCTION__, __LINE__);\
                                fprintf(stdout, __VA_ARGS__);\
                                fprintf(stdout, "\n");

    #define iLBCDEC_BUFPRINT printf
    #define iLBCDEC_MEMPRINT printf
    #define iLBCDEC_STATEPRINT printf

#else
    #define iLBCDEC_DPRINT(...)

    #ifdef iLBCDEC_STATEDETAILS
        #define iLBCDEC_STATEPRINT printf
    #else
        #define iLBCDEC_STATEPRINT(...)
    #endif

    #ifdef iLBCDEC_BUFDETAILS
        #define iLBCDEC_BUFPRINT printf
    #else
        #define iLBCDEC_BUFPRINT(...)
    #endif

    #ifdef iLBCDEC_MEMDETAILS
        #define iLBCDEC_MEMPRINT(...)  fprintf(stdout, "%s %d::  ",__FUNCTION__, __LINE__);\
                                      fprintf(stdout, __VA_ARGS__);\
                                      fprintf(stdout, "\n");
    #else
        #define iLBCDEC_MEMPRINT(...)
    #endif

#endif

#define iLBCDEC_EPRINT(...)  fprintf(stdout, "%s %s %d::  ", __FILE__,__FUNCTION__, __LINE__);\
                            fprintf(stdout, __VA_ARGS__);\
                            fprintf(stdout, "\n");

/* ======================================================================= */
/**
 * @def    OMX_CONF_INIT_STRUCT   Macro to Initialise the structure variables
 */
/* ======================================================================= */
#define OMX_CONF_INIT_STRUCT(_s_, _name_)   \
    memset((_s_), 0x0, sizeof(_name_)); \
    (_s_)->nSize = sizeof(_name_);      \
    (_s_)->nVersion.s.nVersionMajor = 0x1;  \
    (_s_)->nVersion.s.nVersionMinor = 0x0;  \
    (_s_)->nVersion.s.nRevision = 0x0;      \
    (_s_)->nVersion.s.nStep = 0x0




/* ======================================================================= */
/** iLBCDEC_COMPONENT_PRIVATE: This is the major and main structure of the
 * component which contains all type of information of buffers, ports etc
 * contained in the component.
*
* @see OMX_BUFFERHEADERTYPE
* @see OMX_AUDIO_PARAM_PORTFORMATTYPE
* @see OMX_PARAM_PORTDEFINITIONTYPE
* @see iLBCD_LCML_BUFHEADERTYPE
* @see OMX_PORT_PARAM_TYPE
* @see OMX_PRIORITYMGMTTYPE
* @see iLBCD_AUDIODEC_PORT_TYPE
* @see iLBCD_BUFFERLIST
* @see iLBCD_AUDIODEC_PORT_TYPE
* @see LCML_STRMATTR
* @see
*/
/* ==================================================================== */

typedef struct iLBCDEC_COMPONENT_PRIVATE
{

    OMX_BUFFERHEADERTYPE* pBufHeader[NUM_OF_PORTS]; /* for compatibility with iLBC fr */

    BUFFERHEADERTYPE_INFO BufInfo[NUM_OF_PORTS]; /* for compatibility with iLBC fr */

    /** Handle for use with async callbacks */
    OMX_CALLBACKTYPE cbInfo;
    /* Component port information */
    /** Handle for use with async callbacks */
    OMX_PORT_PARAM_TYPE *sPortParam;
    
    /* Input port information */
    OMX_AUDIO_PARAM_PORTFORMATTYPE sInPortFormat;
    
    /* Output port information */
    OMX_AUDIO_PARAM_PORTFORMATTYPE sOutPortFormat;
    
    /** This will contain info like how many buffers
        are there for input/output ports, their size etc, but not
        BUFFERHEADERTYPE POINTERS. */
    OMX_PARAM_PORTDEFINITIONTYPE *pPortDef[NUM_OF_PORTS];

    /** iLBC Component Parameters */
    OMX_AUDIO_PARAM_ILBCTYPE *iLBCParams; /* for compatibility with GSM fr */
    
    /** PCM Component Parameters */
    /* OMX_AUDIO_PARAM_PCMMODETYPE* pcmParams[NUM_OF_PORTS];*/
    OMX_AUDIO_PARAM_PCMMODETYPE *pcmParams;
    
    /** This is component handle */
    OMX_COMPONENTTYPE* pHandle;
    
    /** Current state of this component */
    OMX_STATETYPE curState;
    
    /** The component thread handle */
    pthread_t ComponentThread;
    
    /** The pipes for sending buffers to the thread */
    int dataPipe[2];
    
    /** The pipes for sending buffers to the thread */
    int cmdPipe[2];

    /** The pipes for sending command data to the thread */
    int cmdDataPipe[2];

    /** The pipes for sending buffers to the thread **/
    int lcml_Pipe[2];

    /** Set to indicate component is stopping **/
    OMX_U32 bIsStopping;

    /** Count of number of buffers outstanding with bridge **/
    OMX_U32 lcml_nIpBuf;

    /** Count of number of buffers outstanding with bridge **/
    OMX_U32 lcml_nOpBuf;
    
    /** Counts of number of buffers received from App  **/
    OMX_U32 app_nBuf;

    /** Counts of number of input buffers sent to LCML **/
    OMX_U32 lcml_nCntIp;
    
    /** Counts of number of output buffers received from LCML **/
    OMX_U32 lcml_nCntOpReceived;

    /** Counts of number of output buffers reclaimed from lcml  **/
    OMX_U32 num_Reclaimed_Op_Buff;
    
    /** This is LCML handle  */
    OMX_HANDLETYPE pLcmlHandle;
    
    /** Contains pointers to LCML Buffer Headers */
    iLBCD_LCML_BUFHEADERTYPE *pLcmlBufHeader[2];

      /** Flag for mime mode */
    OMX_S16 iAmrMimeFlag;

       /** Sampling Frequeny */
    OMX_S16 iAmrSamplingFrequeny;

    /** Number of channels */
    OMX_U32 iAmrChannels;

    /** Flag for Amr mode */
    OMX_S16 iAmrMode;

    /** Flag for DASF mode */
    OMX_S16 dasfmode;

    /** Flag for mime mode */
    OMX_S16 mimemode;   /* for compatibility with gsm fr */
/******/
    /** ACDN mode flag **/
    OMX_S16 acdnmode; /* OK */    

    /** Writing pipe Used for DSP_RENDERING_ON */
    int fdwrite; /* for compatibility with gsm fr */

    /** Reading pipe Used for DSP_RENDERING_ON */
    int fdread; /* for compatibility with gsm fr */

    /** Audio Stream ID */
    OMX_U32 streamID;
    /** Tells whether buffers on ports have been allocated **/
    OMX_U32 bPortDefsAllocated;
    /** Tells whether component thread has started **/
    OMX_U32 bCompThreadStarted;
    /** Marks the buffer data  **/
    OMX_PTR pMarkData;
    /** Marks the buffer **/
    OMX_MARKTYPE *pMarkBuf;
    /** Marks the target component **/
    OMX_HANDLETYPE hMarkTargetComponent;
    /** Flag to track when input buffer's filled length is 0 **/
    OMX_U32 bBypassDSP;
    /** Input buffer list */
    iLBCD_BUFFERLIST *pInputBufferList;
    /** Output buffer list */
    iLBCD_BUFFERLIST *pOutputBufferList;
    /** LCML stream attributes */
    LCML_STRMATTR  *strmAttr;
    /** Contains the version information **/
    OMX_U32 nVersion;

   /** Play Complete Flag */
   OMX_U32 bPlayCompleteFlag; /* for compatibility with gsm fr */

   /** Number of Bytes holding to be sent*/
   OMX_U32 nHoldLength; /* for compatibility with gsm fr */

   /** Pointer to the data holding to be sent*/
   OMX_U8* pHoldBuffer; /* for compatibility with gsm fr */

   /** Flag set when LCML handle is opened */
    OMX_S16 bLcmlHandleOpened;
    
   /** Keeps track of the number of nFillThisBufferCount() calls */    
    OMX_U32 nFillThisBufferCount;
    
    /** Counts number of FillBufferDone calls**/
    OMX_U32 nFillBufferDoneCount;
    /** Counts number of EmptyThisBuffer calls**/
    OMX_U32 nEmptyThisBufferCount;
    /** Counts number of EmptyBufferDone calls**/
    OMX_U32 nEmptyBufferDoneCount;
    
    /* Audio codec parameters structure **/
    iLBCD_AudioCodecParams *pParams;    
    /** Flag for Init Params Initialized */
    OMX_U32 bInitParamsInitialized;
    
    /** Keeps track of the number of nFillThisBufferCount() calls */
    OMX_U32 bIdleCommandPending;
    
   /** Array of Input Buffers that are pending to sent due State = Idle */    
    OMX_BUFFERHEADERTYPE *pInputBufHdrPending[MAX_NUM_OF_BUFS];

   /** Number of Input Buffers that are pending to sent due State = Idle */
    OMX_U32 nNumInputBufPending;

   /** Number of Input Buffers that are pending to sent due State = Idle */
    OMX_BUFFERHEADERTYPE *pOutputBufHdrPending[MAX_NUM_OF_BUFS];
    OMX_U32 nNumOutputBufPending;

    OMX_U32 bDisableCommandPending;
    OMX_U32 bDisableCommandParam;
    OMX_U32 bNoIdleOnStop;
    OMX_U32 nOutStandingFillDones;
    
    /** Stop Codec Command Sent Flag*/
    OMX_U8 bStopSent;
    OMX_U32 bDspStoppedWhileExecuting;
    
    OMX_U32 nRuntimeInputBuffers; /* OK */
    OMX_U32 nRuntimeOutputBuffers; /* OK */
    
    // Flag to set when mutexes are initialized
    OMX_BOOL bMutexInitialized;
    pthread_mutex_t AlloBuf_mutex;    
    pthread_cond_t AlloBuf_threshold;
    OMX_U8 AlloBuf_waitingsignal;
    
    pthread_mutex_t InLoaded_mutex;
    pthread_cond_t InLoaded_threshold;
    OMX_U8 InLoaded_readytoidle;
    
    pthread_mutex_t InIdle_mutex;
    pthread_cond_t InIdle_threshold;
    OMX_U8 InIdle_goingtoloaded;

    OMX_U8 nPendingOutPausedBufs;
    OMX_BUFFERHEADERTYPE *pOutBufHdrWhilePaused[MAX_NUM_OF_BUFS];
    
    OMX_BUFFERHEADERTYPE *LastOutbuf;  /* for compatibility with gsm fr */
    
    OMX_BOOL bIsInvalidState;  /* for compatibility with gsm fr */    
    OMX_STRING* sDeviceString; /* OK */
    
    void* ptrLibLCML; /* for compatibility with gsm fr */

    /** Circular array to keep buffer timestamps */
    OMX_S64 arrBufIndex[MAX_NUM_OF_BUFS]; 
    /** Index to arrTimestamp[], used for input buffer timestamps **/
    OMX_U8 IpBufindex; 
    /** Index to arrTimestamp[], used for output buffer timestamps **/
    OMX_U8 OpBufindex;
    
    OMX_BOOL bLoadedCommandPending;

    OMX_BOOL DSPMMUFault;
    
    OMX_PARAM_COMPONENTROLETYPE componentRole; /* 0j0 not compatilble with G722 *componentRole; */
    /** Pointer to port priority management structure **/
    OMX_PRIORITYMGMTTYPE* pPriorityMgmt;
/******/    
    /** Keep buffer nTickCounts ***/
    OMX_S64 arrTickCount[MAX_NUM_OF_BUFS]; 
    /** Contains the port related info of both the ports **/
    iLBCD_AUDIODEC_PORT_TYPE *pCompPort[NUM_OF_PORTS]; /* OK */
    /* Checks whether or not buffer were allocated by appliction **/
    int bufAlloced; /* OK */
    /** Flag to check about execution of component thread */
    OMX_U16 bExitCompThrd; /* OK */
    OMX_S64 arrTimestamp[MAX_NUM_OF_BUFS]; /* OK */
    OMX_U32 bBufferIsAllocated; /* OK */
    OMX_U32 nInvalidFrameCount; /* OK */
    /** Counts of number of output buffers sent to LCML */
    OMX_U32 lcml_nCntOp; /* OK */
    /** Counts of number of input buffers received from LCML **/
    OMX_U32 lcml_nCntIpRes; /* OK */
    OMX_U32 numPendingBuffers; /* OK */
    OMX_U16 iLBCcodecType;
#ifdef RESOURCE_MANAGER_ENABLED    
    /** Resource Manager update **/
     RMPROXY_CALLBACKTYPE rmproxyCallback;
#endif     
} iLBCDEC_COMPONENT_PRIVATE;
/******/

/* In component's internal header file */
typedef enum OMX_ILBCINDEXAUDIOTYPE {
    OMX_IndexCustomiLBCDecHeaderInfoConfig = 0xFF00000F, 
    OMX_IndexCustomiLBCDecStreamInfoConfig,
    OMX_IndexCustomiLBCDecDataPath,
    OMX_IndexParamAudioILBC,
    OMX_IndexCustomiLBCCodingType, /* original end */
    OMX_IndexCustomConfigILBCCodec,
    OMX_IndexCustomiLBCDecStreamIDConfig,
    OMX_IndexCustomiLBCDecModeEfrConfig,
    OMX_IndexCustomiLBCDecModeDasfConfig,
    OMX_IndexCustomiLBCDecModeMimeConfig
}OMX_ILBCINDEXAUDIOTYPE;

/**/
#define OMX_AUDIO_CodingiLBC OMX_AUDIO_CodingMIDI +1        /**< Any variant of iLBC encoded data */
#define STRING_iLBC_HEADERINFO "OMX.TI.index.config.iLBCheaderinfo"
#define STRING_iLBC_DECODE "OMX.TI.ILBC.decode"

#define STRING_iLBC_0 "OMX.TI.index.config.tispecific"
#define STRING_iLBC_STREAMIDINFO "OMX.TI.index.config.ilbcstreamIDinfo"
#define STRING_iLBC_DATAPATHINFO "OMX.TI.index.config.ilbc.datapath"
#define STRING_iLBC_3 "OMX.TI.index.config.ilbc.paramAudio"
#define STRING_iLBC_4 "OMX.TI.index.config.ilbc.codingType"

/**
 *
 */
typedef enum {
    iLBC_PrimaryCodec = 0,
    iLBC_SecondaryCodec
}iLBCD_CODEC_TYPE;
/**
 *
 *  P R O T O T Y P E S
 *
 **/
OMX_ERRORTYPE  iLBCDEC_Fill_LCMLInitParams         (OMX_HANDLETYPE, LCML_DSP *, OMX_U16 []);
OMX_ERRORTYPE  iLBCDEC_StartComponentThread        (OMX_HANDLETYPE);
OMX_ERRORTYPE  iLBCDEC_FreeCompResources           (OMX_HANDLETYPE);
OMX_ERRORTYPE  iLBCDEC_StopComponentThread         (OMX_HANDLETYPE);
OMX_U32        iLBCDEC_HandleCommand               (iLBCDEC_COMPONENT_PRIVATE *);
OMX_ERRORTYPE  iLBCDEC_HandleDataBuf_FromApp        (OMX_BUFFERHEADERTYPE *, iLBCDEC_COMPONENT_PRIVATE *); /* 0j0 not implemented */
OMX_ERRORTYPE  iLBCDEC_GetBufferDirection          (OMX_BUFFERHEADERTYPE *, OMX_DIRTYPE *);
OMX_ERRORTYPE  iLBCDEC_LCML_Callback               (TUsnCodecEvent ,void *[10]);
OMX_ERRORTYPE  iLBCDEC_GetCorresponding_LCMLHeader (iLBCDEC_COMPONENT_PRIVATE *, OMX_U8 *, OMX_DIRTYPE, iLBCD_LCML_BUFHEADERTYPE **);
OMX_ERRORTYPE  iLBCDEC_Fill_LCMLInitParams         (OMX_HANDLETYPE, LCML_DSP *, OMX_U16 []);
void           AddHeader                           (BYTE **); /* 0j0 not implemented */
void           ResetPtr                            (BYTE **); /* 0j0 not implemented */
OMX_HANDLETYPE iLBCDEC_GetLCMLHandle               (iLBCDEC_COMPONENT_PRIVATE *);
OMX_ERRORTYPE  iLBCDECFreeLCMLHandle               (iLBCDEC_COMPONENT_PRIVATE *); /* 0j0 not implemented */
OMX_ERRORTYPE  iLBCDEC_CleanupInitParams           (OMX_HANDLETYPE);
void           iLBCDEC_SetPending                  (iLBCDEC_COMPONENT_PRIVATE *, OMX_BUFFERHEADERTYPE *, OMX_DIRTYPE, OMX_U32);
void           iLBCDEC_ClearPending                (iLBCDEC_COMPONENT_PRIVATE *, OMX_BUFFERHEADERTYPE *, OMX_DIRTYPE);
OMX_U32        iLBCDEC_IsPending                   (iLBCDEC_COMPONENT_PRIVATE *, OMX_BUFFERHEADERTYPE *, OMX_DIRTYPE);
OMX_ERRORTYPE  iLBCDECFill_LCMLInitParamsEx        (OMX_HANDLETYPE);
OMX_U32        iLBCDEC_IsValid                     (iLBCDEC_COMPONENT_PRIVATE *, OMX_U8 *, OMX_DIRTYPE) ;
OMX_ERRORTYPE  OMX_DmmMap                          (DSP_HPROCESSOR, int , void *, DMM_BUFFER_OBJ *); /* 0j0 not implemented */
OMX_ERRORTYPE  OMX_DmmUnMap                        (DSP_HPROCESSOR, void *, void *); /* 0j0 not implemented */
void          *iLBCDEC_ComponentThread             (void *);
#ifdef RESOURCE_MANAGER_ENABLED
void           iLBCD_ResourceManagerCallback       (RMPROXY_COMMANDDATATYPE);
#endif
void           printEmmEvent                       (TUsnCodecEvent);

/*  =========================================================================*/
/*  func    iLBCDEC_FatalErrorRecover
*
*   desc    handles the clean up and sets OMX_StateInvalid
*           in reaction to fatal errors
*
*   @return n/a
*
*  =========================================================================*/
void iLBCDEC_FatalErrorRecover(iLBCDEC_COMPONENT_PRIVATE *pComponentPrivate);

#endif

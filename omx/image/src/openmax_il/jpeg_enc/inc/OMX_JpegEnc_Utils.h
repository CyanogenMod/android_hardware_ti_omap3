
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
* Done            Texas Instruments OMAP(TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */
/**
* @file OMX_JpegEnc_Utils.h
*
* This is a header file for a JPEG encoder that is fully
* compliant with the OMX Image specification.
* This the file that the application that uses OMX would include
* in its code.
*
* @path  $(CSLPATH)\inc
*
* @rev  0.1
*/
/* -------------------------------------------------------------------------------- */
/* ================================================================================
*!
*! Revision History
*! ===================================
*!
*! 22-May-2006 mf: Revisions appear in reverse chronological order;
*! that is, newest first.  The date format is dd-Mon-yyyy.
* ================================================================================= */

#ifndef OMX_JPEGENC_UTILS__H
#define OMX_JPEGENC_UTILS__H
#include <OMX_Component.h>
#include <OMX_IVCommon.h>


#ifndef UNDER_CE
#include <dlfcn.h>
#endif
#include "LCML_DspCodec.h"
#include "LCML_Types.h"
#include "LCML_CodecInterface.h"
#include <pthread.h>
#include <stdarg.h>
#include <OMX_Core.h>
#include <OMX_Types.h>
#include <OMX_Image.h>
#include <OMX_TI_Common.h>
#include <OMX_TI_Debug.h>
#ifdef RESOURCE_MANAGER_ENABLED
#include <ResourceManagerProxyAPI.h>
#endif
#include "OMX_JpegEnc_CustomCmd.h"


#ifdef __PERF_INSTRUMENTATION__
#include "perf.h"
#endif

#define OMX_JPEGENC_NonMIME 1
#define OMX_NOPORT 0xFFFFFFFE
#define JPEGE_TIMEOUT (100000)
#define NUM_OF_PORTS  2
#define NUM_OF_BUFFERSJPEG 4

#define MAX_INPARAM_SIZE 1024

#define COMP_MAX_NAMESIZE 127

#define OMX_CustomCommandStopThread (OMX_CommandMax - 1)


#ifdef UNDER_CE
    #include <oaf_debug.h>
#endif

#define KHRONOS_1_1

#ifndef FUNC
#define FUNC 1
#endif

#ifdef RESOURCE_MANAGER_ENABLED
#define JPEGENC1MPImage 1000000
#define JPEGENC2MPImage 2000000
#endif

/*
 *     M A C R O S
 */
#define OMX_CONF_INIT_STRUCT(_s_, _name_)   \
    memset((_s_), 0x0, sizeof(_name_)); \
    (_s_)->nSize = sizeof(_name_);      \
    (_s_)->nVersion.s.nVersionMajor = 0x1;  \
    (_s_)->nVersion.s.nVersionMinor = 0x0;  \
    (_s_)->nVersion.s.nRevision = 0x0;      \
    (_s_)->nVersion.s.nStep = 0x0

#define OMX_CHECK_PARAM(_ptr_)  \
{   \
    if(!_ptr_) {    \
    eError = OMX_ErrorBadParameter; \
    goto EXIT; \
    }   \
}

#define OMX_CONF_SET_ERROR_BAIL(_eError, _eCode)\
{                       \
    _eError = _eCode;               \
    goto OMX_CONF_CMD_BAIL;         \
}

#define OMX_MALLOC_STRUCT(_pStruct_, _sName_)   \
    _pStruct_ = (_sName_*)malloc(sizeof(_sName_));  \
    if(_pStruct_ == NULL){  \
        eError = OMX_ErrorInsufficientResources;    \
        goto EXIT;  \
    } \
    memset(_pStruct_, 0, sizeof(_sName_));

#define OMX_MALLOC_STRUCT_EXTRA(_pStruct, _sName, _nExtraSize)   \
    _pStruct = (_sName*)malloc(sizeof(_sName) + _nExtraSize);  \
    if(_pStruct == NULL){  \
        eError = OMX_ErrorInsufficientResources;    \
        goto EXIT;  \
    }   \
    memset(_pStruct, 0, (sizeof(_sName) + _nExtraSize));


#define OMX_MEMCPY_CHECK(_p_)\
{\
    if (_p_ == NULL) { \
    eError = OMX_ErrorInsufficientResources;  \
    goto EXIT;   \
    } \
}   

#define FREE(_ptr)   \
{                     \
    if (_ptr != NULL) { \
        free(_ptr);   \
        _ptr = NULL; \
    }                \
}

#ifdef RESOURCE_MANAGER_ENABLED
#define OMX_GET_RM_VALUE(_Res_, _RM_, _dbg_) \
{   \
    if ((_Res_) <= JPEGENC1MPImage){  \
        (_RM_) = 30;  \
        }   \
    else {  \
        (_RM_) = 60;  \
        }   \
    \
        \
    OMX_PRMGR2((_dbg_), "Value in MHz requested to RM = %d\n", (_RM_)); \
}
#endif

typedef struct IDMJPGE_TIGEM_Comment {
  OMX_U8 comment[256];
  OMX_U16 commentLen;
} IDMJPGE_TIGEM_Comment;


typedef struct IIMGENC_DynamicParams {
    OMX_U32 nSize;             /* nSize of this structure */
    OMX_U32 nNumAU;            /* Number of Access unit to encode,
                                  * set to XDM_DEFAULT in case of entire frame
                                  */
    OMX_U32 nInputChromaFormat;/* Input chroma format, Refer above comments regarding chroma                    */
    OMX_U32 nInputHeight;       /* Input nHeight*/
    OMX_U32 nInputWidth;        /* Input nWidth*/
    OMX_U32 nCaptureWidth;      /* 0: use imagewidth as pitch, otherwise:
                                   * use given display nWidth (if > imagewidth)
                                   * for pitch.
                                   */
    OMX_U32 nGenerateHeader;    /* XDM_ENCODE_AU or XDM_GENERATE_HEADER */
    OMX_U32 qValue;            /* Q value compression factor for encoder */
} IIMGENC_DynamicParams;


  typedef struct IDMJPGE_TIGEM_CustomQuantTables
  {
    /* The array "lum_quant_tab" defines the quantization table for the luma component. */
    OMX_U16 lum_quant_tab[64];
    /* The array "chm_quant_tab" defines the quantization table for the chroma component.  */
    OMX_U16 chm_quant_tab[64];
  } IDMJPGE_TIGEM_CustomQuantTables;
  

typedef struct IDMJPGE_TIGEM_DynamicParams {
    IIMGENC_DynamicParams  params;
    OMX_U32 captureHeight;       /* if set to 0 use image height 
                                     else should set to actual Image height */
    OMX_U32 DRI_Interval ;  
    JPEGENC_CUSTOM_HUFFMAN_TABLE *huffmanTable;
    IDMJPGE_TIGEM_CustomQuantTables *quantTable;
} IDMJPGE_TIGEM_DynamicParams;


#define OMX_JPEGENC_NUM_DLLS (3)
#ifdef UNDER_CE
#define JPEG_ENC_NODE_DLL "/windows/jpegenc_sn.dll64P"
#define JPEG_COMMON_DLL "/windows/usn.dll64P"
#define USN_DLL "/windows/usn.dll64P"
#else
#define JPEG_ENC_NODE_DLL "jpegenc_sn.dll64P"
#define JPEG_COMMON_DLL "usn.dll64P"
#define USN_DLL "usn.dll64P"
#endif

#define JPGENC_SNTEST_STRMCNT          2
#define JPGENC_SNTEST_INSTRMID         0
#define JPGENC_SNTEST_OUTSTRMID        1
#define JPGENC_SNTEST_ARGLENGTH        20
#define JPGENC_SNTEST_INBUFCNT         4
#define JPGENC_SNTEST_OUTBUFCNT        4
#define JPGENC_SNTEST_MAX_HEIGHT       4096
#define JPGENC_SNTEST_MAX_WIDTH        4096
#define JPGENC_SNTEST_PROG_FLAG        1
#define M_COM   0xFE            /* COMment  */

#define JPEGE_DSPSTOP       0x01
#define JPEGE_BUFFERBACK    0x02
#define JPEGE_IDLEREADY     ( JPEGE_DSPSTOP | JPEGE_BUFFERBACK )

typedef enum Content_Type
{
    APP0_BUFFER = 0,
    APP1_BUFFER,
    APP13_BUFFER,
    COMMENT_BUFFER,
    APP0_NUMBUF,
    APP1_NUMBUF,
    APP13_NUMBUF,
    COMMENT_NUMBUF,
    APP0_THUMB_H,
    APP0_THUMB_W,
    APP1_THUMB_H,
    APP1_THUMB_W,
    APP13_THUMB_H,
    APP13_THUMB_W,
    APP0_THUMB_INDEX,
    APP1_THUMB_INDEX,  
    APP13_THUMB_INDEX,
    DYNPARAMS_HUFFMANTABLE,
    DYNPARAMS_QUANTTABLE
} Content_Type;

typedef struct _THUMBNAIL_INFO {
  OMX_U16 APP0_THUMB_INDEX;
  OMX_U16 APP0_THUMB_W;
  OMX_U16 APP0_THUMB_H;
  OMX_U16 APP1_THUMB_INDEX;
  OMX_U16 APP1_THUMB_W;
  OMX_U16 APP1_THUMB_H;
  OMX_U16 APP13_THUMB_INDEX;
  OMX_U16 APP13_THUMB_W;
  OMX_U16 APP13_THUMB_H;
  APP_INFO APP0_BUF;
  APP_INFO APP1_BUF;
  APP_INFO APP13_BUF;
} THUMBNAIL_INFO;

/*This enum must not be changed.  */
typedef enum JPEG_PORT_TYPE_INDEX
    {
    JPEGENC_INP_PORT,
    JPEGENC_OUT_PORT
}JPEG_PORT_TYPE_INDEX;


typedef enum JPEGENC_BUFFER_OWNER {
    JPEGENC_BUFFER_CLIENT = 0x0,
    JPEGENC_BUFFER_COMPONENT_IN,
    JPEGENC_BUFFER_COMPONENT_OUT,
    JPEGENC_BUFFER_DSP,
    JPEGENC_BUFFER_TUNNEL_COMPONENT
} JPEGENC_BUFFER_OWNER;
    
typedef struct _JPEGENC_BUFFERFLAG_TRACK {
    OMX_U32 flag;
    OMX_U32 buffer_id;
    OMX_HANDLETYPE hMarkTargetComponent; 
    OMX_PTR pMarkData;
} JPEGENC_BUFFERFLAG_TRACK;

typedef struct _JPEGENC_BUFFERMARK_TRACK {
    OMX_U32 buffer_id;
    OMX_HANDLETYPE hMarkTargetComponent; 
    OMX_PTR pMarkData;
} JPEGENC_BUFFERMARK_TRACK;
    

typedef struct JPEGENC_BUFFER_PRIVATE {
    OMX_BUFFERHEADERTYPE* pBufferHdr;    
    JPEGENC_BUFFER_OWNER eBufferOwner;
    OMX_BOOL bAllocByComponent;
    OMX_BOOL bReadFromPipe;
} JPEGENC_BUFFER_PRIVATE;    


typedef struct JPEG_PORT_TYPE   {

    OMX_HANDLETYPE hTunnelComponent;
    OMX_U32 nTunnelPort;
    JPEGENC_BUFFER_PRIVATE* pBufferPrivate[NUM_OF_BUFFERSJPEG];
    JPEGENC_BUFFERFLAG_TRACK sBufferFlagTrack[NUM_OF_BUFFERSJPEG];
    JPEGENC_BUFFERMARK_TRACK sBufferMarkTrack[NUM_OF_BUFFERSJPEG];
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef;
    OMX_BUFFERSUPPLIERTYPE pBufSupplier;
    OMX_PARAM_BUFFERSUPPLIERTYPE* pParamBufSupplier;
    OMX_IMAGE_PARAM_PORTFORMATTYPE* pPortFormat;
    OMX_U8 nBuffCount;
}JPEG_PORT_TYPE;

typedef struct JPEGE_INPUT_PARAMS {
    OMX_U32 *pInParams;
    OMX_U32 size;
} JPEGE_INPUT_PARAMS;

typedef struct _JPEGENC_CUSTOM_PARAM_DEFINITION
{
    OMX_U8 cCustomParamName[128];
    OMX_INDEXTYPE nCustomParamIndex;
} JPEGENC_CUSTOM_PARAM_DEFINITION;



typedef struct JPEGENC_COMPONENT_PRIVATE
{
    JPEG_PORT_TYPE* pCompPort[NUM_OF_PORTS];    
    OMX_PORT_PARAM_TYPE* pPortParamType;
    OMX_PORT_PARAM_TYPE* pPortParamTypeAudio;
    OMX_PORT_PARAM_TYPE* pPortParamTypeVideo;
    OMX_PORT_PARAM_TYPE* pPortParamTypeOthers;
    OMX_PRIORITYMGMTTYPE* pPriorityMgmt;
    OMX_CALLBACKTYPE cbInfo;
    OMX_IMAGE_PARAM_QFACTORTYPE* pQualityfactor;
    OMX_CONFIG_RECTTYPE  *pCrop;
    /** This is component handle */
    OMX_COMPONENTTYPE* pHandle;
    /*Comonent Name& Version*/
    OMX_STRING cComponentName;
    OMX_VERSIONTYPE ComponentVersion;
    OMX_VERSIONTYPE SpecVersion;
    
    /** Current state of this component */
    OMX_STATETYPE   nCurState;
    OMX_STATETYPE   nToState;
    OMX_U8          ExeToIdleFlag;  /* StateCheck */

    OMX_U32 nInPortIn;
    OMX_U32 nInPortOut;
    OMX_U32 nOutPortIn;
    OMX_U32 nOutPortOut;
    OMX_BOOL bInportDisableIncomplete;
    OMX_BOOL bOutportDisableIncomplete;
    OMX_BOOL bSetLumaQuantizationTable;
    OMX_BOOL bSetChromaQuantizationTable;    
    OMX_BOOL bSetHuffmanTable;
    OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE *pCustomLumaQuantTable;
    OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE *pCustomChromaQuantTable;    
    JPEGENC_CUSTOM_HUFFMANTTABLETYPE *pHuffmanTable;
    
    
    /** The component thread handle */
    pthread_t ComponentThread;
    /** The pipes to maintain free buffers */
    int free_outBuf_Q[2]; 
    /** The pipes to maintain input buffers sent from app*/
    int filled_inpBuf_Q[2];
    /** The pipes for sending buffers to the thread */
    int nCmdPipe[2];
    int nCmdDataPipe[2];
    OMX_U32 nApp_nBuf;
    short int nNum_dspBuf;
    int nCommentFlag;
    OMX_U8 *pString_Comment;
    THUMBNAIL_INFO ThumbnailInfo;
    JPEGE_INPUT_PARAMS InParams;
#ifdef RESOURCE_MANAGER_ENABLED
    RMPROXY_CALLBACKTYPE rmproxyCallback;
#endif
    OMX_BOOL bPreempted;    
    int nFlags;
    int nMarkPort;
    OMX_PTR pMarkData;          
    OMX_HANDLETYPE hMarkTargetComponent; 
    OMX_BOOL bDSPStopAck;
   OMX_BOOL bFlushComplete; 
    OMX_BOOL bAckFromSetStatus;
    void* pLcmlHandle;   /* Review Utils.c */
    int isLCMLActive;
    LCML_DSP_INTERFACE* pLCML;
    void * pDllHandle;
    OMX_U8 nDRI_Interval;
#ifdef KHRONOS_1_1
    OMX_PARAM_COMPONENTROLETYPE componentRole;
#endif
    IDMJPGE_TIGEM_DynamicParams *pDynParams;

    pthread_mutex_t jpege_mutex;
    pthread_cond_t  stop_cond;
    pthread_cond_t  flush_cond;
    /* pthread_cond_t  control_cond; */
    pthread_mutex_t jpege_mutex_app;
    pthread_cond_t  populate_cond;
    pthread_cond_t  unpopulate_cond;
    
#ifdef __PERF_INSTRUMENTATION__
    PERF_OBJHANDLE pPERF, pPERFcomp;
#endif
    struct OMX_TI_Debug dbg;

} JPEGENC_COMPONENT_PRIVATE;


OMX_ERRORTYPE HandleJpegEncCommand (JPEGENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U32 nParam1);
OMX_ERRORTYPE JpegEncDisablePort(JPEGENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U32 nParam1);
OMX_ERRORTYPE JpegEncEnablePort(JPEGENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U32 nParam1);
OMX_ERRORTYPE HandleJpegEncCommandFlush(JPEGENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_U32 nParam1);
OMX_ERRORTYPE JPEGEnc_Start_ComponentThread(OMX_HANDLETYPE pHandle);
OMX_ERRORTYPE HandleJpegEncDataBuf_FromApp(JPEGENC_COMPONENT_PRIVATE *pComponentPrivate );
OMX_ERRORTYPE HandleJpegEncDataBuf_FromDsp( JPEGENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE* pBuffHead );
OMX_ERRORTYPE HandleJpegEncFreeDataBuf( JPEGENC_COMPONENT_PRIVATE *pComponentPrivate, OMX_BUFFERHEADERTYPE* pBuffHead );
OMX_ERRORTYPE HandleJpegEncFreeOutputBufferFromApp( JPEGENC_COMPONENT_PRIVATE *pComponentPrivate );
OMX_ERRORTYPE AllocJpegEncResources( JPEGENC_COMPONENT_PRIVATE *pComponentPrivate );
OMX_ERRORTYPE JPEGEnc_Free_ComponentResources(JPEGENC_COMPONENT_PRIVATE *pComponentPrivate);
OMX_ERRORTYPE Fill_JpegEncLCMLInitParams(LCML_DSP *lcml_dsp, OMX_U16 arr[], OMX_HANDLETYPE pComponent); 
OMX_ERRORTYPE GetJpegEncLCMLHandle(OMX_HANDLETYPE pComponent);
OMX_ERRORTYPE SetJpegEncInParams(JPEGENC_COMPONENT_PRIVATE *pComponentPrivate);
OMX_ERRORTYPE SendDynamicParam(JPEGENC_COMPONENT_PRIVATE *pComponentPrivate);
OMX_BOOL IsTIOMXComponent(OMX_HANDLETYPE hComp);


typedef OMX_ERRORTYPE (*fpo)(OMX_HANDLETYPE);

static const struct DSP_UUID JPEGESOCKET_TI_UUID = {
    0xCB70C0C1, 0x4C85, 0x11D6, 0xB1, 0x05, {
        0x00, 0xC0, 0x4F, 0x32, 0x90, 0x31
    }
};


static const struct DSP_UUID USN_UUID = {
    0x79A3C8B3, 0x95F2, 0x403F, 0x9A, 0x4B, {
        0xCF, 0x80, 0x57, 0x73, 0x05, 0x41
    }
};

void* OMX_JpegEnc_Thread (void* pThreadData);

typedef enum ThrCmdType
{
    SetState,
    Flush,
    StopPort,
    RestartPort,
    MarkBuf,
    Start,
    Stop,
    FillBuf,
    EmptyBuf
} ThrCmdType;




typedef enum OMX_JPEGE_INDEXTYPE  {

    OMX_IndexCustomCommentFlag = 0xFF000001,
    OMX_IndexCustomCommentString = 0xFF000002,
    OMX_IndexCustomInputFrameWidth,
    OMX_IndexCustomInputFrameHeight,
    OMX_IndexCustomThumbnailAPP0_INDEX,
    OMX_IndexCustomThumbnailAPP0_W,
    OMX_IndexCustomThumbnailAPP0_H,
    OMX_IndexCustomThumbnailAPP0_BUF,
    OMX_IndexCustomThumbnailAPP1_INDEX,
    OMX_IndexCustomThumbnailAPP1_W,
    OMX_IndexCustomThumbnailAPP1_H,
    OMX_IndexCustomThumbnailAPP1_BUF,
    OMX_IndexCustomThumbnailAPP13_INDEX,
    OMX_IndexCustomThumbnailAPP13_W,
    OMX_IndexCustomThumbnailAPP13_H,
    OMX_IndexCustomThumbnailAPP13_BUF,
    OMX_IndexCustomQFactor,
    OMX_IndexCustomDRI,
    OMX_IndexCustomHuffmanTable,
    OMX_IndexCustomDebug,
}OMX_INDEXIMAGETYPE;

typedef struct IUALG_Buf {
    OMX_PTR            pBufAddr;  
    unsigned long          ulBufSize;
    OMX_PTR            pParamAddr;
    unsigned long        ulParamSize;
    unsigned long          ulBufSizeUsed;
    //IUALG_BufState tBufState;        
    OMX_BOOL           bBufActive;
    OMX_U32          unBufID;
    unsigned long          ulReserved;
} IUALG_Buf;

typedef enum {
    IUALG_CMD_STOP             = 0,
    IUALG_CMD_PAUSE            = 1,
    IUALG_CMD_GETSTATUS        = 2,
    IUALG_CMD_SETSTATUS        = 3,
    IUALG_CMD_USERSETCMDSTART  = 100,
    IUALG_CMD_USERGETCMDSTART  = 150,
    IUALG_CMD_FLUSH            = 0x100
}IUALG_Cmd;
#endif /*OMX_JPEGENC_UTILS__H*/


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
* =========================================================================== */
/** OMX_TI_Common.h
  *  The LCML header file contains the definitions used by both the
  *  application and the component to access common items.
 */

#ifndef __OMX_TI_COMMON_H__
#define __OMX_TI_COMMON_H__

#include "OMX_Component.h"
#include "OMX_TI_Debug.h"

/* OMX_TI_SEVERITYTYPE enumeration is used to indicate severity level of errors
          returned by TI OpenMax components.
    Critical      Requires reboot/reset DSP
    Severe       Have to unload components and free memory and try again
    Major        Can be handled without unloading the component
    Minor        Essentially informational
 */
typedef enum OMX_TI_SEVERITYTYPE {
    OMX_TI_ErrorCritical = 1,
    OMX_TI_ErrorSevere,
    OMX_TI_ErrorMajor,
    OMX_TI_ErrorMinor
} OMX_TI_SEVERITYTYPE;

/* ======================================================================= */
/**
 * @def    EXTRA_BYTES   For Cache alignment
           DSP_CACHE_ALIGNMENT  For Cache alignment
 *
 */
/* ======================================================================= */
#define EXTRA_BYTES 256
#define DSP_CACHE_ALIGNMENT 128

/* ======================================================================= */
/**
 * @def    OMX_GET_SIZE_DSPALIGN Macro to get the DSP aligned size
 */
/* ======================================================================= */
#define OMX_GET_SIZE_DSPALIGN(_size_)            \
    ((_size_+DSP_CACHE_ALIGNMENT-1) & ~(DSP_CACHE_ALIGNMENT-1))

/* ======================================================================= */
/**
 * @def    OMX_MALLOC_GENERIC   Macro to allocate Memory
 */
/* ======================================================================= */
#define OMX_MALLOC_GENERIC(_pStruct_, _sName_)                         \
    OMX_MALLOC_SIZE(_pStruct_,sizeof(_sName_),_sName_)

/* ======================================================================= */
/**
 * @def    OMX_MALLOC_SIZE   Macro to allocate Memory
 *
 * This macro is expected to be rewritten to remove the goto.  Some
 * currently unreachable code has been added to the audio components
 * after calls to the generic MALLOC functions defined in this file
 * (i.e. after OMX_MALLOC_SIZE_DSPALIGN, OMX_MALLOC_GENERIC and OMX_MALLOC_SIZE)
 * in anticipation of the change.
 *
 */
/* ======================================================================= */
#define OMX_MALLOC_SIZE(_ptr_, _size_,_name_)            \
    _ptr_ = (_name_*)newmalloc(_size_);                         \
    if(_ptr_ == NULL){                                          \
        OMXDBG_PRINT(stderr, ERROR, 4, 0, "***********************************\n");        \
        OMXDBG_PRINT(stderr, ERROR, 4, 0, "%d :: Malloc Failed\n",__LINE__);               \
        OMXDBG_PRINT(stderr, ERROR, 4, 0, "***********************************\n");        \
        eError = OMX_ErrorInsufficientResources;                \
        goto EXIT;                                              \
    }                                                           \
    memset(_ptr_,0,_size_);                                     \
    OMXDBG_PRINT(stderr, BUFFER, 2, OMX_DBG_BASEMASK, "%d :: Malloced = %p\n",__LINE__,_ptr_);

/* ======================================================================= */
/**
 * @def    OMX_MEMALIGN_MALLOC_SIZE   Macro to allocate aligned Memory
 */
/* ======================================================================= */
#define OMX_MEMALIGN_MALLOC_SIZE(_ptr_,_size_,_alignment_,_name_)\
    _ptr_ = (_name_*)newmemalign(_alignment_,_size_);            \
    if(_ptr_ == NULL){                                           \
        OMXDBG_PRINT(stderr, ERROR, 4, 0, "***********************************\n");        \
        OMXDBG_PRINT(stderr, ERROR, 4, 0, "%d :: Memalign alloc Failed\n",__LINE__);       \
        OMXDBG_PRINT(stderr, ERROR, 4, 0, "***********************************\n");        \
    } else {                                                     \
        memset(_ptr_,0,_size_);                                  \
        OMXDBG_PRINT(stderr, BUFFER, 2, OMX_DBG_BASEMASK, "%d :: Malloced = %p\n",__LINE__,_ptr_);\
    }

/* ======================================================================= */
/**
 * @def    OMX_MALLOC_SIZE_DSPALIGN   Macro to allocate Memory with cache alignment protection
 */
/* ======================================================================= */
#define OMX_MALLOC_SIZE_DSPALIGN(_ptr_, _size_,_name_)            \
    OMX_MEMALIGN_MALLOC_SIZE(_ptr_, OMX_GET_SIZE_DSPALIGN(_size_), DSP_CACHE_ALIGNMENT, _name_);

/* ======================================================================= */
/**
 *  M A C R O FOR MEMORY FREE
 */
/* ======================================================================= */
#define OMX_MEMFREE_STRUCT(_pStruct_)\
    OMXDBG_PRINT(stderr, BUFFER, 2, OMX_DBG_BASEMASK, "%d :: [FREE] %p\n",__LINE__,_pStruct_); \
    if(_pStruct_ != NULL){\
        newfree(_pStruct_);\
        _pStruct_ = NULL;\
    }

/* ======================================================================= */
/**
 *  M A C R O FOR MEMORY FREE
 */
/* ======================================================================= */
#define OMX_MEMFREE_STRUCT_DSPALIGN(_pStruct_,_name_)\
    if(_pStruct_ != NULL){\
        OMX_MEMFREE_STRUCT(_pStruct_);\
    }

/**
 *@omx_mutex_signal inline function to send signals in a thread safe way
 *@param pthread_mutex_t *omx_mutex
 *@param pthread_cond_t *omx_threshold
 *@param OMX_U8 *omx_signal
 */
static inline void omx_mutex_signal(pthread_mutex_t *omx_mutex,
                                     pthread_cond_t *omx_threshold,
                                     OMX_U8 *omx_signal){
    pthread_mutex_lock(omx_mutex);
    if (*omx_signal == 0) {
        *omx_signal = 1;
        pthread_cond_signal(omx_threshold);
    }
    pthread_mutex_unlock(omx_mutex);
}
/**
 *@omx_mutex_wait inline function to wait for signals in a thread safe way
 *@param pthread_mutex_t *omx_mutex
 *@param pthread_cond_t *omx_threshold
 *@param OMX_U8 *omx_signal
 */
static inline void omx_mutex_wait(pthread_mutex_t *omx_mutex,
                                     pthread_cond_t *omx_threshold,
                                     OMX_U8 *omx_signal){
    pthread_mutex_lock(omx_mutex);
    while (*omx_signal == 0) {
        pthread_cond_wait(omx_threshold,
            omx_mutex);
    }
    *omx_signal = 0;
    pthread_mutex_unlock(omx_mutex);
}

#endif /*  end of  #ifndef __OMX_TI_COMMON_H__ */
/* File EOF */

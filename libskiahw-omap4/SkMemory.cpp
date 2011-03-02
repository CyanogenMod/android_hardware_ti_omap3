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
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include "SkMemory.h"
#include "SkTypes.h"
#include "SkThread.h"
#include <timm_osal_error.h>
#include <timm_osal_memory.h>
extern "C" {

#include "memmgr.h"
#include "tiler.h"
#include <timm_osal_interfaces.h>
#include <timm_osal_trace.h>

};


#define PRINTF SkDebugf

void* tisk_malloc_throw(size_t size)
{
    return tisk_malloc_flags(size, SK_MALLOC_THROW);
}

void* tisk_realloc_throw(void* addr, size_t size)
{

    void* p = realloc(addr, size);
    if (size == 0)
    {
        return p;
    }

    if (p == NULL)
        sk_throw();
    return p;
}

void tisk_free(void* p)
{
    if (p)
    {
        MemMgr_Free(p);
    }
}

void* tisk_malloc_flags(size_t size, unsigned flags)
{
    MemAllocBlock *MemReqDescTiler;
    void *p = NULL;

    MemReqDescTiler = (MemAllocBlock*)TIMM_OSAL_Malloc((sizeof(MemAllocBlock) * 2), TIMM_OSAL_TRUE, 0 ,TIMMOSAL_MEM_SEGMENT_EXT);
    if (!MemReqDescTiler)
    {
        return NULL;
        printf("\nD can't allocate memory for Tiler block allocation \n");
    }

    MemReqDescTiler[0].pixelFormat = PIXEL_FMT_PAGE;
    MemReqDescTiler[0].dim.len = size;
    MemReqDescTiler[0].stride = 0;
    p = (void *)MemMgr_Alloc(MemReqDescTiler,1);

    if (p == NULL)
    {
        if (flags & SK_MALLOC_THROW)
            sk_throw();
    }
    return p;
}




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

#ifndef TISkMemorys_DEFINED
#define TISkMemorys_DEFINED


#include <stdio.h>
#include <stdlib.h>


/** Return a block of memory (at least 4-byte aligned) of at least the
    specified size. If the requested memory cannot be returned, either
    return null (if SK_MALLOC_TEMP bit is clear) or call sk_throw()
    (if SK_MALLOC_TEMP bit is set). To free the memory, call tisk_free().
*/
extern void* tisk_malloc_flags(size_t size, unsigned flags);
/** Same as sk_malloc(), but hard coded to pass SK_MALLOC_THROW as the flag
*/
extern void* tisk_malloc_throw(size_t size);
/** Same as standard realloc(), but this one never returns null on failure. It will throw
    an exception if it fails.
*/
extern void* tisk_realloc_throw(void* buffer, size_t size);
/** Free memory returned by sk_malloc(). It is safe to pass null.
*/
extern void  tisk_free(void*);


#endif




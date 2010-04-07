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

#define PRINTF SkDebugf
//#define PRINTF printf

//#define SK_TRACK_ALLOC
//#define SK_TAG_BLOCKS
//#define SK_CHECK_TAGS

// size this (as a multiple of 4) so that the total offset to the internal data
// is at least a multiple of 8 (since some clients of our malloc may require
// that.
static const char kBlockHeaderTag[128] = {""};
static const char kBlockTrailerTag[128] = {""};

#define kByteFill 0xCD
#define kDeleteFill 0xEF
#define ALIGN_128_BYTE 128

static SkMutex& get_block_mutex() {
    static SkMutex* gBlockMutex;
    if (NULL == gBlockMutex) {
        gBlockMutex = new SkMutex;
    }
    return *gBlockMutex;
}

static struct SkBlockHeader* gHeader;

struct SkBlockHeader {
    SkBlockHeader* fNext;
#ifdef SK_CHECK_TAGS
    SkBlockHeader** fTop; // set to verify in debugger that block was alloc'd / freed with same gHeader
    SkBlockHeader* fPrevious; // set to see in debugger previous block when corruption happens
#endif
    size_t fSize;
    char fHeader[sizeof(kBlockHeaderTag)];
    // data goes here. The offset to this point must be a multiple of 8
    char fTrailer[sizeof(kBlockTrailerTag)];

    void* add(size_t realSize)
    {
        SkAutoMutexAcquire  ac(get_block_mutex());
        InMutexValidate();
        fNext = gHeader;
#ifdef SK_CHECK_TAGS
        fTop = &gHeader;
        fPrevious = NULL;
        if (fNext != NULL)
            fNext->fPrevious = this;
#endif
        gHeader = this;
        fSize = realSize;
        memcpy(fHeader, kBlockHeaderTag, sizeof(kBlockHeaderTag));
        void* result = fTrailer;
        void* trailer = (char*)result + realSize;
        memcpy(trailer, kBlockTrailerTag, sizeof(kBlockTrailerTag));
        return result;
    }

    static void Dump()
    {
        SkAutoMutexAcquire  ac(get_block_mutex());
        InMutexValidate();
        SkBlockHeader* header = gHeader;
        int count = 0;
        size_t size = 0;
        while (header != NULL) {
            char scratch[256];
            char* pos = scratch;
            size_t size = header->fSize;
            int* data = (int*)(void*)header->fTrailer;
            pos += sprintf(pos, "%p 0x%08zx (%7zd)  ",
                data, size, size);
            size >>= 2;
            size_t ints = size > 4 ? 4 : size;
            size_t index;
            for (index = 0; index < ints; index++)
                pos += sprintf(pos, "0x%08x ", data[index]);
            pos += sprintf(pos, " (");
            for (index = 0; index < ints; index++)
                pos += sprintf(pos, "%g ", data[index] / 65536.0f);
            if (ints > 0)
                --pos;
            pos += sprintf(pos, ") \"");
            size_t chars = size > 16 ? 16 : size;
            char* chPtr = (char*) data;
            for (index = 0; index < chars; index++) {
                char ch = chPtr[index];
                pos += sprintf(pos, "%c", ch >= ' ' && ch < 0x7f ? ch : '?');
            }
            pos += sprintf(pos, "\"");
            SkDebugf("%s\n", scratch);
            count++;
            size += header->fSize;
            header = header->fNext;
        }
        SkDebugf("--- count %d  size 0x%08x (%zd) ---\n", count, size, size);
    }

    void remove() const
    {
        SkAutoMutexAcquire  ac(get_block_mutex());
        SkBlockHeader** findPtr = &gHeader;
        do {
            SkBlockHeader* find = *findPtr;
            SkASSERT(find != NULL);
            if (find == this) {
                *findPtr = fNext;
                break;
            }
            findPtr = &find->fNext;
        } while (true);
        InMutexValidate();
        SkASSERT(memcmp(fHeader, kBlockHeaderTag, sizeof(kBlockHeaderTag)) == 0);
        const char* trailer = fTrailer + fSize;
        SkASSERT(memcmp(trailer, kBlockTrailerTag, sizeof(kBlockTrailerTag)) == 0);
    }

    static void Validate()
    {
        SkAutoMutexAcquire  ac(get_block_mutex());
        InMutexValidate();
    }

private:
    static void InMutexValidate()
    {
        SkBlockHeader* header = gHeader;
        while (header != NULL) {
            SkASSERT(memcmp(header->fHeader, kBlockHeaderTag, sizeof(kBlockHeaderTag)) == 0);
            char* trailer = header->fTrailer + header->fSize;
            SkASSERT(memcmp(trailer, kBlockTrailerTag, sizeof(kBlockTrailerTag)) == 0);
            header = header->fNext;
        }
    }
};

void Dump()
{
    SkBlockHeader::Dump();
}

void ValidateHeap()
{
    SkBlockHeader::Validate();
}


void* tisk_malloc_throw(size_t size)
{
    return tisk_malloc_flags(size, SK_MALLOC_THROW);
}

void* tisk_realloc_throw(void* addr, size_t size)
{
#ifdef SK_TAG_BLOCKS
    ValidateHeap();
    if (addr != NULL) {
        SkBlockHeader* header = (SkBlockHeader*)
            ((char*)addr - SK_OFFSETOF(SkBlockHeader, fTrailer));
        header->remove();
#ifdef SK_TRACK_ALLOC
        PRINTF("tisk_realloc_throw %p oldSize=%zd\n", addr, header->fSize);
#endif
        addr = header;
    }
    size_t realSize = size;
    if (size)
        size += sizeof(SkBlockHeader);
#endif

    void* p = realloc(addr, size);
    if (size == 0)
    {
        ValidateHeap();
        return p;
    }

    if (p == NULL)
        sk_throw();
#ifdef SK_TAG_BLOCKS
    else
    {
        SkBlockHeader* header = (SkBlockHeader*) p;
        p = header->add(realSize);
#ifdef SK_TRACK_ALLOC
        PRINTF("tisk_realloc_throw %p size=%zd\n", p, realSize);
#endif
    }
#endif
    ValidateHeap();
    return p;
}

void tisk_free(void* p)
{
    if (p)
    {
        ValidateHeap();
#ifdef SK_TAG_BLOCKS
        SkBlockHeader* header = (SkBlockHeader*)
            ((char*)p - SK_OFFSETOF(SkBlockHeader, fTrailer));
        header->remove();
#ifdef SK_TRACK_ALLOC
        PRINTF("tisk_free %p size=%zd\n", p, header->fSize);
#endif
        size_t size = header->fSize + sizeof(SkBlockHeader);
        memset(header, kDeleteFill, size);
        p = header;
#endif
        ValidateHeap();
        free(p);
        ValidateHeap();
    }
}

void* tisk_malloc_flags(size_t size, unsigned flags)
{
    ValidateHeap();
#ifdef SK_TAG_BLOCKS
    size_t realSize = size;
    size += sizeof(SkBlockHeader);
#endif
    size = (size_t)((size + ALIGN_128_BYTE - 1) & ~(ALIGN_128_BYTE - 1));
    void* p = memalign(ALIGN_128_BYTE, size);
    if (p == NULL)
    {
        if (flags & SK_MALLOC_THROW)
            sk_throw();
    }
#ifdef SK_TAG_BLOCKS
    else
    {
        SkBlockHeader* header = (SkBlockHeader*) p;
        p = header->add(realSize);
        memset(p, kByteFill, realSize);
#ifdef SK_TRACK_ALLOC
        PRINTF("tisk_malloc_flags %p size=%zd\n", p, realSize);
#endif
    }
#endif
    ValidateHeap();
    return p;
}




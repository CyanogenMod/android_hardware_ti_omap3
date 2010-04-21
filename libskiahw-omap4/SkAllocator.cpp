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

#include "SkAllocator.h"


/** We explicitly use the same allocator for our pixels that SkMask does,
 so that we can freely assign memory allocated by one class to the other.
 */
bool TIHeapAllocator::allocPixelRef(SkBitmap* dst,
                                            SkColorTable* ctable) {
    Sk64 size = dst->getSize64();
    if (size.isNeg() || !size.is32()) {
        return false;
    }

    size_t bm_size = size.get32();
    size_t nWidth = dst->width();
    size_t nHeight = dst->height();
    size_t nBytesPerPixel = 2;

    /*round up if nWidth is not multiple of 32*/
    ( (nWidth%32 ) !=0 ) ?  nWidth=32 * (  (  nWidth/32 ) + 1 )  : nWidth;

    /*round up if nHeight is not multiple of 16*/
    ( (nHeight%16) !=0 ) ?  nHeight=16 * (  (  nHeight/16 ) + 1 )  : nHeight;

    if (dst->config() == 6) nBytesPerPixel = 4;

    bm_size = nWidth * nHeight * nBytesPerPixel;

    void* addr = tisk_malloc_flags(bm_size, 0);  // returns NULL on failure
    if (NULL == addr) {
        return false;
    }

    dst->setPixelRef(new TISkMallocPixelRef(addr, size.get32(), ctable))->unref();
    // since we're already allocated, we lockPixels right away
    dst->lockPixels();
    return true;
}


///////////////////////////////////////////////////////////////////////////////

TISkMallocPixelRef::TISkMallocPixelRef(void* storage, size_t size,
                                   SkColorTable* ctable) {
    SkASSERT(storage);
    fStorage = storage;
    fSize = size;
    fCTable = ctable;
    ctable->safeRef();
}

TISkMallocPixelRef::~TISkMallocPixelRef() {
    if (fCTable != NULL) {
        fCTable->safeUnref();
        fCTable = NULL;
    }
    if (fStorage != NULL) {
        tisk_free(fStorage);
        fStorage = NULL;
    }
}

void* TISkMallocPixelRef::onLockPixels(SkColorTable** ct) {
    *ct = fCTable;
    return fStorage;
}

void TISkMallocPixelRef::flatten(SkFlattenableWriteBuffer& buffer) const {
    this->INHERITED::flatten(buffer);

    buffer.write32(fSize);
    buffer.writePad(fStorage, fSize);
    if (fCTable) {
        buffer.writeBool(true);
        fCTable->flatten(buffer);
    } else {
        buffer.writeBool(false);
    }
}

TISkMallocPixelRef::TISkMallocPixelRef(SkFlattenableReadBuffer& buffer) : INHERITED(buffer, NULL) {
    fSize = buffer.readU32();
    fStorage = tisk_malloc_throw(fSize);
    if (fStorage) {
        buffer.read(fStorage, fSize);
    } else {
        SkDebugf("\n%s():%d::ERROR!!!tisk_malloc_throw Failed\n",__FUNCTION__,__LINE__);
    }
    if (buffer.readBool()) {
        fCTable = SkNEW_ARGS(SkColorTable, (buffer));
    } else {
        fCTable = NULL;
    }
}




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


#ifndef SkAllocator_DEFINED
#define SkAllocator_DEFINED

#include "SkPixelRef.h"
#include "SkBitmap.h"
#include "SkMemory.h"
#include "SkColorFilter.h"


/** Subclass of Allocator that returns a pixelref that allocates its pixel
    memory from the heap. This is the default Allocator invoked by
    allocPixels().
*/
class TIHeapAllocator : public SkBitmap::Allocator {
public:
    virtual bool allocPixelRef(SkBitmap*, SkColorTable*);
};


/** We explicitly use the same allocator for our pixels that SkMask does,
    so that we can freely assign memory allocated by one class to the other.
*/
class TISkMallocPixelRef : public SkPixelRef {
public:
    /** Allocate the specified buffer for pixels. The memory is freed when the
        last owner of this pixelref is gone.
     */
    TISkMallocPixelRef(void* addr, size_t size, SkColorTable* ctable);
    virtual ~TISkMallocPixelRef();

    //! Return the allocation size for the pixels
    size_t getSize() const { return fSize; }

    // overrides from SkPixelRef
    virtual void flatten(SkFlattenableWriteBuffer&) const;
    virtual Factory getFactory() const {
        return Create;
    }
    static SkPixelRef* Create(SkFlattenableReadBuffer& buffer) {
        return SkNEW_ARGS(TISkMallocPixelRef, (buffer));
    }

protected:
    // overrides from SkPixelRef
    virtual void* onLockPixels(SkColorTable**);
    virtual void onUnlockPixels() {}

    TISkMallocPixelRef(SkFlattenableReadBuffer& buffer);

private:
    void*           fStorage;
    size_t          fSize;
    SkColorTable*   fCTable;

    typedef SkPixelRef INHERITED;
};

#endif //ifndef SkAllocator_DEFINED


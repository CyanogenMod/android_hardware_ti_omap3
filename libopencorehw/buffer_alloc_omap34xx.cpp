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

/*
 * Author: Srini Gosangi <srini.gosangi@windriver.com>
 * Author: Michael Barabanov <michael.barabanov@windriver.com>
 */

#define LOG_NDEBUG 0
#define LOG_TAG "BufferAllocOmap34xx"
#include <utils/Log.h>

#include "buffer_alloc_omap34xx.h"
#include "oscl_mem.h" // needed for oscl_malloc / oscl_free

/* based on test code in pvmi/media_io/pvmiofileoutput/src/pvmi_media_io_fileoutput.cpp */

BufferAllocOmap34xx::BufferAllocOmap34xx(): refCount(0), bufferSize(0), maxBuffers(NUM_OVERLAY_BUFFERS_REQUESTED), numAllocated(0)
{
}

BufferAllocOmap34xx::~BufferAllocOmap34xx()
{

}

OSCL_EXPORT_REF void BufferAllocOmap34xx::addRef()
{
    ++refCount;
}

OSCL_EXPORT_REF void BufferAllocOmap34xx::removeRef()
{
    --refCount;
    if (refCount <= 0)
    {
       LOGD("BufferAllocOmap34xx::removeRef()");
       // this->~BufferAllocOmap34xx();
#if 0
        OSCL_DELETE(this);
#endif
    }
}


OSCL_EXPORT_REF OsclAny* BufferAllocOmap34xx::allocate()
{
    if (numAllocated < maxBuffers)
    {
        OsclAny* ptr = buffer_address[numAllocated];
        if (ptr) ++numAllocated;
        return ptr;
    }
    return NULL;
}

OSCL_EXPORT_REF void BufferAllocOmap34xx::deallocate(OsclAny* ptr)
{
    if (ptr)
    {
        --numAllocated;
    }
}

OSCL_EXPORT_REF uint32 BufferAllocOmap34xx::getBufferSize()
{
    return bufferSize;
}

OSCL_EXPORT_REF uint32 BufferAllocOmap34xx::getNumBuffers()
{
    return maxBuffers;
}


OSCL_EXPORT_REF bool BufferAllocOmap34xx::queryInterface(const PVUuid& uuid, PVInterface*& aInterface)
{
    aInterface = NULL; // initialize aInterface to NULL in case uuid is not supported

    if (PVMFFixedSizeBufferAllocUUID == uuid)
    {
        // Send back ptr to the allocator interface object
        PVMFFixedSizeBufferAlloc* myInterface	= OSCL_STATIC_CAST(PVMFFixedSizeBufferAlloc*, this);
        refCount++; // increment interface refcount before returning ptr
        aInterface = OSCL_STATIC_CAST(PVInterface*, myInterface);
        return true;
    }
    return false;
}


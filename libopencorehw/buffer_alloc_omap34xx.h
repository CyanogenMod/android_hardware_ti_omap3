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


#ifndef BUFFER_ALLOC_OMAP34XXH_INCLUDED
#define BUFFER_ALLOC_OMAP34XXH_INCLUDED

#include "pv_interface.h"
#include "overlay_common.h"

#ifndef PVMF_FIXEDSIZE_BUFFER_ALLOC_H_INCLUDED
#include "pvmf_fixedsize_buffer_alloc.h"
#endif

/* based on test code in pvmi/media_io/pvmiofileoutput/include/pvmi_media_io_fileoutput.h */

class BufferAllocOmap34xx: public PVInterface, public PVMFFixedSizeBufferAlloc
{
    public:

        BufferAllocOmap34xx();

        virtual ~BufferAllocOmap34xx();

        OSCL_IMPORT_REF void addRef();

        OSCL_IMPORT_REF void removeRef();

        OSCL_IMPORT_REF bool queryInterface(const PVUuid& uuid, PVInterface*& aInterface) ;

        OSCL_IMPORT_REF OsclAny* allocate();

        OSCL_IMPORT_REF void deallocate(OsclAny* ptr) ;

        OSCL_IMPORT_REF uint32 getBufferSize() ;

        OSCL_IMPORT_REF uint32 getNumBuffers() ;

    public:
        int32 refCount;
        int32 bufferSize;
        int32 maxBuffers;
        int32 numAllocated;
        void* buffer_address[NUM_OVERLAY_BUFFERS_MAX]; //max buffers supported in overlay
};

#endif

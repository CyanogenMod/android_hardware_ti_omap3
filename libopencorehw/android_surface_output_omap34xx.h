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

#ifndef ANDROID_SURFACE_OUTPUT_OMAP34XXH_INCLUDED
#define ANDROID_SURFACE_OUTPUT_OMAP34XXH_INCLUDED

#include "android_surface_output.h"
#include "buffer_alloc_omap34xx.h"
#include "overlay_common.h"

// support for shared contiguous physical memory
#include <ui/Overlay.h>

using namespace android;


class AndroidSurfaceOutputOmap34xx : public AndroidSurfaceOutput
{
public:
    AndroidSurfaceOutputOmap34xx();

    // frame buffer interface
    virtual bool initCheck();
    virtual void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters, int num_elements, PvmiKvp * & aRet_kvp);
    virtual PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier,
            PvmiKvp*& aParameters, int& num_parameter_elements, PvmiCapabilityContext aContext);
    virtual PVMFStatus writeFrameBuf(uint8* aData, uint32 aDataLen, const PvmiMediaXferHeader& data_header_info, int aIndex);
    virtual PVMFCommandId writeAsync(uint8 aFormatType, int32 aFormatIndex, uint8* aData, uint32 aDataLen,
            const PvmiMediaXferHeader& data_header_info, OsclAny* aContext);
    virtual void closeFrameBuf();
    virtual void postLastFrame();
    OSCL_IMPORT_REF ~AndroidSurfaceOutputOmap34xx();

    BufferAllocOmap34xx   mbufferAlloc;
    
private:
    bool            mUseOverlay;
    sp<Overlay>     mOverlay;
    int             bufEnc;
    int32           iNumberOfBuffers;
    int32           iBufferSize;
    bool            mConvert;
    int             mOptimalQBufCnt;
    PVMFFormatType  iVideoCompressionFormat;
    int             iBytesperPixel;
    int             icropY;
    int             icropX;
    int             mBuffersQueuedToDSS;
};

#endif // ANDROID_SURFACE_OUTPUT_OMAP34XX_H_INCLUDED

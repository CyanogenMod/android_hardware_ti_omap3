/* ------------------------------------------------------------------
 * Copyright (C) 2008 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#ifndef ANDROID_SURFACE_OUTPUT_OMAP34XXH_INCLUDED
#define ANDROID_SURFACE_OUTPUT_OMAP34XXH_INCLUDED

#include "android_surface_output.h"

// support for shared contiguous physical memory
#include <ui/Overlay.h>


class AndroidSurfaceOutputOmap34xx : public AndroidSurfaceOutput
{
public:
    AndroidSurfaceOutputOmap34xx();

    // frame buffer interface
    virtual bool initCheck();
    virtual PVMFStatus writeFrameBuf(uint8* aData, uint32 aDataLen, const PvmiMediaXferHeader& data_header_info);
    virtual void postLastFrame();
    virtual void closeFrameBuf();

    OSCL_IMPORT_REF ~AndroidSurfaceOutputOmap34xx();
private:

    bool mUseOverlay;
    sp<Overlay> mOverlay;
    int mBuffer_count;
    int mRecycle_buffer_count;
    void* mOverlay_buffer_address[4];//max buffers supported in overlay is 4
};

#endif // ANDROID_SURFACE_OUTPUT_OMAP34XX_H_INCLUDED

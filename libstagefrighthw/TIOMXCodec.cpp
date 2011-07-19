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

#define LOG_NDEBUG 0
#define LOG_TAG "TIOMXCodec"

#include <utils/Log.h>
#include <TIOMXCodec.h>
#include <binder/MemoryDealer.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaBufferGroup.h>
#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/OMXCodec.h>
#include <media/stagefright/Utils.h>
#include <utils/Vector.h>
#include <OMX_Component.h>

#define LOG_FUNCTION_NAME_ENTRY    LOGV(" ###### Calling %s() Line %d ++ ######",  __FUNCTION__, __LINE__);
#define LOG_FUNCTION_NAME_EXIT    LOGV(" ###### Calling %s() -- ######",  __FUNCTION__);

namespace android {

sp<MediaSource> TIOMXCodec::CreateTIOMXCodec(
                                                const sp<IOMX> &omx,
                                                const sp<MetaData> &meta,
                                                const sp<MediaSource> &source,
                                                const char *matchComponentName,
                                                uint32_t flags)
{

    const char *mime;
    bool success = meta->findCString(kKeyMIMEType, &mime);
    CHECK(success);

    Vector<String8> matchingCodecs;
    findMatchingCodecs(mime, false, matchComponentName, flags, &matchingCodecs);

    if (matchingCodecs.isEmpty()) {
        return NULL;
    }

    sp<OMXCodecObserver> observer = new OMXCodecObserver;

    IOMX::node_id node = 0;

    const char *componentName;
    for (size_t i = 0; i < matchingCodecs.size(); ++i) {
        componentName = matchingCodecs[i].string();

#if BUILD_WITH_FULL_STAGEFRIGHT
        sp<MediaSource> softwareCodec =
        InstantiateSoftwareCodec(componentName, source);

        if (softwareCodec != NULL) {
            LOGV("Successfully allocated software codec '%s'", componentName);
            return softwareCodec;
        }
#endif

        LOGV("Attempting to allocate OMX node '%s'", componentName);

        status_t err = omx->allocateNode(componentName, observer, &node);
        if (err == OK) {
            LOGV("Successfully allocated OMX node '%s'", componentName);

#if defined(OMAP_ENHANCEMENT)
            /*************************************************
            Note: We could move this getComponentQuirks into derived
            **************************************************/
            sp<OMXCodec> codec = new TIOMXCodec(
            omx, node, 
#if defined(TARGET_OMAP4)
            getComponentQuirks(componentName, false, flags),
#else
            getComponentQuirks(componentName, false),
#endif
            false, mime, componentName,
            source);
#endif
            observer->setCodec(codec);
            err = codec->configureCodec(meta, flags);

            if (err == OK) {
                return codec;
            }
            LOGV("Failed to configure codec '%s'", componentName);
        }
    }

    return NULL;
}


TIOMXCodec::TIOMXCodec(const sp<IOMX> &omx,
                        IOMX::node_id node,
                        uint32_t quirks,
                        bool isEncoder,
                        const char *mime,
                        const char *componentName,
                        const sp<MediaSource> &source)
                        : OMXCodec(omx, node, quirks, false, mime, componentName, source)

{
}

uint32_t TIOMXCodec::getBufferCount()
{
    OMX_PARAM_PORTDEFINITIONTYPE def;
    InitOMXParams(&def);
    def.nPortIndex = kPortIndexOutput;
    status_t err = mOMX->getParameter(mNode, OMX_IndexParamPortDefinition, &def, sizeof(def));
    if(err != OK) {
        LOGD("Error while retrieving the buffer count: %d", def.nBufferCountActual);
    }
    return def.nBufferCountActual;
}

}

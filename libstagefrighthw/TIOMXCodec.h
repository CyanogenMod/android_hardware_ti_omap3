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

#ifndef TI_OMX_CODEC_H_
#define TI_OMX_CODEC_H_

#include <media/IOMX.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <utils/threads.h>
#include <media/stagefright/OMXCodec.h>

namespace android {

struct TIOMXCodec: public OMXCodec {

static sp<MediaSource> CreateTIOMXCodec(const sp<IOMX> &omx,
                                        const sp<MetaData> &meta,
                                        const sp<MediaSource> &source,
                                        const char *matchComponentName = NULL,
                                        uint32_t flags = 0);


protected:
virtual ~TIOMXCodec(){}

public:
TIOMXCodec(const sp<IOMX> &omx, 
                    IOMX::node_id node, 
                    uint32_t quirks,
                    bool isEncoder, 
                    const char *mime, 
                    const char *componentName, 
                    const sp<MediaSource> &source);

uint32_t getBufferCount();

};

}

#endif

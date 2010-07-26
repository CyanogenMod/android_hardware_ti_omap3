
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

#ifndef SKIAHW_DECODER_ENTRY_H
#define SKIAHW_DECODER_ENTRY_H

#include "SkBitmap.h"
#include "SkStream.h"
#include "SkAllocator.h"
#include "SkImageDecoder.h"
#include "SkImageDecoder_libtijpeg.h"
#include "SkImageUtility.h"
#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include <utils/threads.h>
#include <utils/List.h>

extern "C" {
	#include "OMX_Component.h"
	#include "OMX_IVCommon.h"
}
class SkTIJPEGImageDecoderEntry;

typedef WatchdogThread<SkTIJPEGImageDecoderEntry> DecoderWatchdog;
// typedef SkTIJPEGImageCodecList_Item<SkTIJPEGImageDecoder, DecoderWatchdog> SkTIJPEGImageDecoderList_Item;

class SkTIJPEGImageDecoderList_Item
{
public:
    SkTIJPEGImageDecoder* Decoder;
    android::sp<DecoderWatchdog>  WatchdogTimer;
};

class SkTIJPEGImageDecoderListWrapper
{
public:
    SkTIJPEGImageDecoderListWrapper() {}
    ~SkTIJPEGImageDecoderListWrapper(){
        while(list.begin() != list.end())
        {
            android::List<SkTIJPEGImageDecoderList_Item*>::iterator iter = list.begin();
            SkTIJPEGImageDecoderList_Item* item = static_cast<SkTIJPEGImageDecoderList_Item*>(*iter);
            SkAutoTDelete<SkTIJPEGImageDecoder> autodelete(item->Decoder);
            item->WatchdogTimer.clear();
            SkDebugf("SkTIJPEGImageDecoder Cleanup: 0x%x", item);
            list.erase(iter);
        }
    }
    android::List<SkTIJPEGImageDecoderList_Item*> list;
};

class SkTIJPEGImageDecoderEntry :public SkImageDecoder
{
    friend class WatchdogThread<SkTIJPEGImageDecoderEntry>;
    friend class SkTIJPEGImageDecoder;

protected:
	virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode);

public:
    typedef struct JpegDecoderParams
    {
        // Quatization Table
        // Huffman Table
        // SectionDecode;
        // SubRegionDecode
        OMX_U32 nXOrg;         /* X origin*/
        OMX_U32 nYOrg;         /* Y origin*/
        OMX_U32 nXLength;      /* X length*/
        OMX_U32 nYLength;      /* Y length*/

    }JpegDecoderParams;

    SkTIJPEGImageDecoderEntry();
    ~SkTIJPEGImageDecoderEntry();
    virtual Format getFormat() const { return kJPEG_Format; }

    bool SetJpegDecParams(JpegDecoderParams * jdp) {
        memcpy(&jpegDecParams, jdp, sizeof(JpegDecoderParams));
        return true;
    }


private:
    JpegDecoderParams jpegDecParams;
    bool WatchdogCallback(void* param);
};


extern "C" SkImageDecoder* SkImageDecoder_HWJPEG_Factory() {
    return SkNEW(SkTIJPEGImageDecoderEntry);
}

#endif


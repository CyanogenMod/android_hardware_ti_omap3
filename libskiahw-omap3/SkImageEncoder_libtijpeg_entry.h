
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

#ifndef SKIAHW_ENCODER_ENTRY_H
#define SKIAHW_ENCODER_ENTRY_H

#include <string.h>
#include "SkBitmap.h"
#include "SkStream.h"
#include "SkTemplates.h"
#include "SkImageEncoder.h"
#include "SkImageEncoder_libtijpeg.h"
#include "SkImageUtility.h"
#include <stdio.h>
#include <semaphore.h>
#include <utils/threads.h>
#include <utils/List.h>

extern "C" {
	#include "OMX_Component.h"
	#include "OMX_IVCommon.h"
}
class SkTIJPEGImageEncoderEntry;

typedef WatchdogThread<SkTIJPEGImageEncoderEntry> EncoderWatchdog;

class SkTIJPEGImageEncoderList_Item
{
public:
    SkTIJPEGImageEncoder* Encoder;
    android::sp<EncoderWatchdog>  WatchdogTimer;
};

class SkTIJPEGImageEncoderListWrapper
{
public:
    SkTIJPEGImageEncoderListWrapper() {}
    ~SkTIJPEGImageEncoderListWrapper(){
        while(list.begin() != list.end())
        {
            android::List<SkTIJPEGImageEncoderList_Item*>::iterator iter = list.begin();
            SkTIJPEGImageEncoderList_Item* item = static_cast<SkTIJPEGImageEncoderList_Item*>(*iter);
            SkAutoTDelete<SkTIJPEGImageEncoder> autodelete(item->Encoder);
            item->WatchdogTimer.clear();
            SkDebugf("SkTIJPEGImageEncoder Cleanup: 0x%x", item);
            list.erase(iter);
        }
    }
    android::List<SkTIJPEGImageEncoderList_Item*> list;
};

class SkTIJPEGImageEncoderEntry :public SkImageEncoder
{
    friend class WatchdogThread<SkTIJPEGImageEncoderEntry>;

protected:
	virtual bool onEncode(SkWStream* stream, const SkBitmap& bm, int quality);

public:
    SkTIJPEGImageEncoderEntry();
    ~SkTIJPEGImageEncoderEntry();

private:
    bool WatchdogCallback(void* param);
};

extern "C" SkImageEncoder* SkImageEncoder_HWJPEG_Factory() {
    return SkNEW(SkTIJPEGImageEncoderEntry);
}

extern "C" SkTIJPEGImageEncoderEntry* SkImageEncoder_TIJPEG_Factory() {
    return SkNEW(SkTIJPEGImageEncoderEntry);
}


#endif
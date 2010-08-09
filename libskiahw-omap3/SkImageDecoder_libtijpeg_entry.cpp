
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
/**
* @file SkImageDecoder_libtijpeg_entry.cpp
*
* This file implements SkTIJPEGImageDecoderEntry
*
*/

#include "SkImageDecoder_libtijpeg_entry.h"

#define PRINTF SkDebugf

const int DESIRED_LOAD = 1;
const int MAX_DEL_ATTEMPTS = 3;
const unsigned int MAX_DECODERS = 1;

static SkTIJPEGImageDecoderListWrapper SkTIJPEGImageDecoderList;
static android::Mutex SkTIJPEGImageDecoderListLock;

SkTIJPEGImageDecoderEntry::~SkTIJPEGImageDecoderEntry()
{
    SkDebugf("SkTIJPEGImageDecoderEntry::~SkTIJPEGImageDecoderEntry()");
}

SkTIJPEGImageDecoderEntry::SkTIJPEGImageDecoderEntry()
{
    //Initialize JpegDecoderParams structure
    memset((void*)&jpegDecParams, 0, sizeof(JpegDecoderParams));
}

bool SkTIJPEGImageDecoderEntry::WatchdogCallback(void* __item)
{
    bool restart = false;
    android::List<SkTIJPEGImageDecoderList_Item*>::iterator iter;
    SkTIJPEGImageDecoderList_Item* item = reinterpret_cast<SkTIJPEGImageDecoderList_Item*>(__item);

    android::Mutex::Autolock autolock(SkTIJPEGImageDecoderListLock);
    SkDebugf("SkTIJPEGImageDecoderEntry::WatchdogCallback() item=0x%x", item);

    if(item->Decoder->GetLoad() == 0 || item->Decoder->GetDeleteAttempts() >= MAX_DEL_ATTEMPTS)
    {
        if(item->Decoder->GetDeleteAttempts() > MAX_DEL_ATTEMPTS)
            SkDebugf("    Restart attempt limit reached. deleting...");

        // watchdog has expired, delete our reference to Decoder object
        SkAutoTDelete<SkTIJPEGImageDecoder> autodelete(item->Decoder);
        // reset the strong pointer, okay to do here since lifetime will last longer than reset
        item->WatchdogTimer.clear();
        // delete item from Decoder list
        for(iter = SkTIJPEGImageDecoderList.list.begin(); iter != SkTIJPEGImageDecoderList.list.end(); iter++)
        {
            if(((SkTIJPEGImageDecoderList_Item*)*iter) == item)
            {
                SkTIJPEGImageDecoderList.list.erase(iter);
                break;
            }
        }
    }else{
        // Decoder is still doing something, increment deleltion attempt, restart watchdog
        SkDebugf("  Decoder is still doing something, restart watchdog");
        item->Decoder->IncDeleteAttempts();
        restart = true;
    }
    return restart;

    // return DeleteFromListIfDecoderNotWorking<SkTIJPEGImageDecoderList_Item*, SkTIJPEGImageDecoder>(__item, SkTIJPEGImageDecoderList);
}
bool SkTIJPEGImageDecoderEntry::onDecode(SkStream* stream, SkBitmap* bm, Mode mode)
{

    bool result;
    bool itemFound = false;
    SkBitmap::Config prefConfig = this->getPrefConfig(k32Bit_SrcDepth, false);

    android::List<SkTIJPEGImageDecoderList_Item*>::iterator iter;

    { // scope for lock: SkTIJPEGImageDecoderListLock

        // should only need lock when accesing and modifying static list of decoders
        //      - decoder will handle locking its own critical section
        //      - check decoder load in watchdog callback and make sure not to delete Decoder while it is working
        android::Mutex::Autolock autolock(SkTIJPEGImageDecoderListLock);

        //TODO: Need algo to select decoder from list
        //      Maybe we can keep list sorted by Load and just pick off from the beginning of the list
        //      Below will work for now as we are not supporting parallel decodes anyways
        for(iter = SkTIJPEGImageDecoderList.list.begin(); iter != SkTIJPEGImageDecoderList.list.end(); iter++)
        {
            SkTIJPEGImageDecoderList_Item* item = static_cast<SkTIJPEGImageDecoderList_Item*>(*iter);
            if(item->Decoder->GetLoad() < DESIRED_LOAD)
            {
                itemFound = true;
                item->WatchdogTimer->restart();
                break;
            }
        }

        if(!itemFound)
        {
            if(SkTIJPEGImageDecoderList.list.empty() || SkTIJPEGImageDecoderList.list.size() < MAX_DECODERS)
            {
                SkTIJPEGImageDecoderList_Item* type = new SkTIJPEGImageDecoderList_Item;
                type->Decoder =  SkNEW(SkTIJPEGImageDecoder);
                type->WatchdogTimer = new DecoderWatchdog(this, (void*)type, (nsecs_t) 10*1000*1000*1000);
                type->WatchdogTimer->run("Decoder Watchdog", ANDROID_PRIORITY_DISPLAY);
                SkTIJPEGImageDecoderList.list.insert(SkTIJPEGImageDecoderList.list.begin(), type);
                iter = SkTIJPEGImageDecoderList.list.begin();
            } else {
                // oh well, tried our best. just return an decoder from the top of the list
                iter = SkTIJPEGImageDecoderList.list.begin();
                static_cast<SkTIJPEGImageDecoderList_Item*>(*iter)->WatchdogTimer->restart();
            }
        }
    } //SkTIJPEGImageDecoderListLock

    //propagate the decoder parameters to the decoder object.
    static_cast<SkTIJPEGImageDecoderList_Item*>(*iter)->Decoder->SetJpegDecodeParameters((void*)&jpegDecParams);
    return static_cast<SkTIJPEGImageDecoderList_Item*>(*iter)->Decoder->onDecode(this, stream, bm, prefConfig, mode);
}

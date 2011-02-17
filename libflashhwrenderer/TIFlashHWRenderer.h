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

#ifndef TI_FLASH_HWRENDERER_H_
#define TI_FLASH_HWRENDERER_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ui/Overlay.h>
#include <utils/Log.h>
#include <utils/Vector.h>

#include <binder/IServiceManager.h>
#include <media/OverlayRenderer.h>
#include <media/IMediaPlayerService.h>
#include <media/stagefright/MediaDebug.h>

#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>

#include <OMX_Component.h>
#include <binder/IMemory.h>

#include "FlashHWRenderer.h"

namespace android {

class TIFlashHWRenderer : public FlashHWRenderer {

public:

    struct media_buf_t {
        bool yes; //yes means it is with display
        bool reclaim; //  reclaim means it can be reclaimed.
        void *medBuf;
        media_buf_t(){ yes = false; reclaim = false; medBuf = NULL; }
        ~media_buf_t() {}
        void set(bool y, bool r, void * buf) { yes = y; reclaim = r; medBuf=buf;}
    };

    TIFlashHWRenderer(const sp<MediaSource> &source,
                                            int32_t displayWidth,
                                            int32_t displayHeight,
                                            int colorFormat,
                                            int32_t decodedWidth,
                                            int32_t decodedHeight,
                                            int infoType,
                                            void* info);

    ~TIFlashHWRenderer();

    status dequeueBuffer(MediaBuffer ** mediaBuf);

    status queueBuffer(MediaBuffer * mediaBuf);

    status reclaimBuffer(MediaBuffer ** mediaBuf);

    status getStatus();

    status setCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h) ;

    status getCrop(uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h);

    int32_t getBufferCount();

    status setPosition(int32_t x, int32_t y) ;

    status setSize(uint32_t w, uint32_t h) ;

    status setOrientation(int orientation, uint32_t flags);

    status setAlpha(float alpha) ;

    status setParameter(int param, void *value);

    status getParameter(int param, void *value);

    void Release();

    status initCheck() { return mInitCheck; }

private:

    sp<IOverlayRenderer> mOverlayRenderer;
    status mInitCheck;
    sp<MediaSource> mVideoSource;
    uint_t mBufferCount;
    int32_t mDisplayWidth, mDisplayHeight;
    int32_t mDecodedWidth, mDecodedHeight;
    OMX_COLOR_FORMATTYPE mColorFormat;
    unsigned int mInfoType;
    Vector< sp<IMemory> > mOverlayAddresses;
    Vector <media_buf_t *> mDisplayList;
    int mNumBufsDisplay;
    mutable Mutex mLock;
    /* copy of previously set Crop values */
    int mCropX;
    int mCropY;

    status queryBufferInfo();
    int getVideoFormat();

};

}

#endif

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
###############################################################################
            DO NOT MODIFY THIS FILE. IT WILL BREAK COMPATIBILITY WITH FLASH
###############################################################################
 */

#ifndef FLASH_HWRENDERER_H_
#define FLASH_HWRENDERER_H_

#include <stdint.h>

#define RENDER_ERR_BASE 0x88881000

namespace android {

enum {
    RENDER_ERR_INV_BUF=     (RENDER_ERR_BASE + 1),
    RENDER_ERR_QUE=         (RENDER_ERR_BASE + 2),
    RENDER_ERR_DEQUE=       (RENDER_ERR_BASE + 3),
    RENDER_ERR_STREAM_OFF=  (RENDER_ERR_BASE + 4),
    RENDER_ERR_UNKNOWN=     (RENDER_ERR_BASE + 5),
};

class FlashHWRenderer;
class ISurface;
class Overlay;
class IMemory;
class MediaSource;
class MediaBuffer;
template <typename T> class sp;


typedef uint32_t status;

/*
DLL name should be "libflashhwrenderer.so"
Flash would load the dll, and get the FactoryCreateHWRenderer function symbol
Flash would call FactoryCreateHWRenderer to instantiate the HW overlay renderer

source = is the instance of the stagefright
colorformat = color format that would be given to this decoder
displayWidth = width of the target video object
displayHeight = height of the target video object
decoderType = one of the value from decoder_type
decodedWidth = width of the buffer, in the event flash uses it's own software decoder
decodedHeight = width of the buffer, in the event flash uses it's own software decoder
info = additional future parameter

*/
extern "C" FlashHWRenderer* FactoryCreateHWRenderer(
                const sp<MediaSource> &source,
                int colorFormat,
                int32_t displayWidth,
                int32_t displayHeight,
                int decoderType, // = 0,
                int32_t decodedWidth, // = 0, //padded W from Decoder
                int32_t decodedHeight, // =0, //padded H from Decoder
                int infoType,
                void * info /* = NULL */); //any additional flags specific to HW Renderers.


class FlashHWRenderer {

public:

enum colorFormat
{
    NV12Tiled = 0x01
};

enum decoder_type {
    HW_DECODER = 0,
    SW_DECODER =1
};

/* blocks until an overlay buffer is available and return that buffer */
virtual status dequeueBuffer(MediaBuffer ** mediaBuf) = 0;

/* send buffer to display the overlay buffer and post it */
virtual status queueBuffer(MediaBuffer * mediaBuf) = 0;

/* In case a within FlashHWRenderer */
virtual status reclaimBuffer(MediaBuffer ** mediaBuf) = 0;

/* Future proofing for getting status */
virtual status getStatus() =0;

/* set crop -reference Top Left corner */
virtual status setCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h)  = 0;

/* get crop */
virtual status getCrop(uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h)  = 0;

/************************************************
Surface Controls: Subset of Surface Control/Surface Client  APIs
************************************************/

virtual status setPosition(int32_t x, int32_t y)  = 0;

virtual status setSize(uint32_t w, uint32_t h)  = 0;

virtual status setOrientation(int orientation, uint32_t flags) = 0;

virtual status setAlpha(float alpha=1.0f) = 0;

/*********************************************
Generic set/getParameter:
*********************************************/
virtual status setParameter(int param, void *value) = 0;

virtual status getParameter(int param, void *value) = 0;

/* Release the object */
virtual void Release() = 0;

public:

FlashHWRenderer() {};
virtual ~FlashHWRenderer(){};

};

}
#endif

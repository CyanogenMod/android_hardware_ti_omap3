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


//#define LOG_NDEBUG 0
#define LOG_TAG "TIFlashHWRenderer"

#include "TIFlashHWRenderer.h"
#include "TIOMXCodec.h"

#define ARMPAGESIZE 4096

#define BOOL_STR(val)  ( (val) ? "TRUE" : "FALSE")

#define LOG_FUNCTION_NAME_ENTRY    LOGV(" ###### Calling %s() Line %d ++ ######",  __FUNCTION__, __LINE__);
#define LOG_FUNCTION_NAME_EXIT    LOGV(" ###### Calling %s() -- ######",  __FUNCTION__);

//#define __DUMP_TO_FILE__
#ifdef __DUMP_TO_FILE__
#define NUM_BUFFERS_TO_DUMP 46
#define FIRST_FRAME_INDEX 23
FILE *pOutFile;
#endif



using namespace android;

namespace android{


#ifdef __DUMP_TO_FILE__

void dumpBufferToFile(void* pSrc, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t s)
{
    //LOGD("framenumber = %u, pSrc = %p, x = %u, y = %u, w = %u, h = %u, s = %u", framenumber, pSrc, x, y, w, h, s);
    /* Dumping only the Y component */
    pSrc = pSrc + (y*s);
    for (int i = 0; i < h; i++) {
        fwrite(pSrc+x, 1, w, pOutFile);
        fflush(pOutFile);
        pSrc = pSrc + s;
    }
}

#endif


extern "C" sp<MediaSource> createPlatformOMXCodec(
                                                    const sp<IOMX> &omx,
                                                    const sp<MetaData> &meta,
                                                    const sp<MediaSource> &source,
                                                    const char *matchComponentName,
                                                    uint32_t flags)
{
    return TIOMXCodec::CreateTIOMXCodec(omx, meta, source, matchComponentName, flags);
};

FlashHWRenderer * FactoryCreateHWRenderer(const sp<MediaSource> &source,
                                                                                    int colorFormat,
                                                                                    int32_t displayWidth,
                                                                                    int32_t displayHeight,
                                                                                    int decoderType, // = 0,
                                                                                    int32_t decodedWidth ,
                                                                                    int32_t decodedHeight,
                                                                                    int infoType,
                                                                                    void* info)

{

    /* TODO: Prevent multiple renderers from being created */
    /* TODO: add error handling for input params */

    TIFlashHWRenderer *renderer = new TIFlashHWRenderer(source,
                                                displayWidth, displayHeight,
                                                colorFormat, decodedWidth, decodedHeight, infoType, info);

    if(renderer->initCheck() != OK) {
        LOGE("Error Creating TIHWRenderer");
        delete renderer;
        renderer = NULL;
    }

    return renderer;
}

void TIFlashHWRenderer::Release()
{
    LOG_FUNCTION_NAME_ENTRY
    mOverlayRenderer->releaseMe();
    mOverlayRenderer.clear();
    mOverlayRenderer = NULL;
    LOGD("Successfully released mOverlayRenderer");
    //delete this;

    /* Clean up stuff */
    for(unsigned int i = 0; i < mDisplayList.size(); ++i){
        media_buf_t * medBuf = mDisplayList.editItemAt(i);
        if(medBuf != NULL) delete medBuf;
    }

    /* TODO: make sure all other resources are released as well */

#ifdef __DUMP_TO_FILE__
        if (pOutFile) fclose(pOutFile);
#endif
}

TIFlashHWRenderer::TIFlashHWRenderer( const sp<MediaSource> &source,
                                                                        int32_t displayWidth,
                                                                        int32_t displayHeight,
                                                                        int colorFormat,
                                                                        int32_t decodedWidth,
                                                                        int32_t decodedHeight,
                                                                        int infoType,
                                                                        void * info):
                                                                        mVideoSource(source),
                                                                        mDisplayWidth(displayWidth),
                                                                        mDisplayHeight(displayHeight),
                                                                        mColorFormat((OMX_COLOR_FORMATTYPE)colorFormat),
                                                                        mDecodedWidth(decodedWidth),
                                                                        mDecodedHeight(decodedHeight),
                                                                        mInfoType(infoType)
{
    LOG_FUNCTION_NAME_ENTRY

    mInitCheck = NO_INIT;
    mNumBufsDisplay= 0;
    mCropX = -1;
    mCropY = -1;

    if( mVideoSource.get() == NULL ||  0 == (mDisplayHeight * mDisplayHeight)) {
        LOGE("Invalid Input Found");
            return;
    }

    if(mInfoType == FlashHWRenderer::HW_DECODER) {
        if(   OK != queryBufferInfo() ){
            return;
        }
    }
    else {
        if( !(mDecodedHeight & mDecodedWidth) )
            return;
    }

    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = sm->getService(String16("media.player"));
    sp<IMediaPlayerService> service = interface_cast<IMediaPlayerService>(binder);
    CHECK(service.get() != NULL);
    mOverlayRenderer = service->getOverlayRenderer();
    CHECK(mOverlayRenderer.get() != NULL);

    //LOGV("mBufferCount=%d, displayWidth=%d, displayHeight=%d, colorFormat=%d, decodedWidth=%d, decodedHeight=%d, infoType=%d", mBufferCount, displayWidth, displayHeight, colorFormat, decodedWidth, decodedHeight, infoType);
    mInitCheck = mOverlayRenderer->createOverlayRenderer(mBufferCount, mDisplayWidth, mDisplayHeight, getVideoFormat(), mDecodedWidth, mDecodedHeight, mInfoType, info);
    CHECK(mInitCheck == OK);

    media_buf_t*  mbuf = NULL;
    sp<IMemory> mem;

    for (size_t bCount = 0; bCount < mBufferCount; ++bCount) {
        mem = mOverlayRenderer->getBuffer(bCount);
        CHECK(mem.get() != NULL);
        LOGV("mem->pointer[%d] = %p", bCount, mem->pointer());
        mOverlayAddresses.push(mem);
        mbuf = new media_buf_t; // Is this memory leaked later on...
        mDisplayList.push(mbuf);
        mbuf = NULL;
    }
    mVideoSource->setBuffers(mOverlayAddresses);


#ifdef __DUMP_TO_FILE__

    char filename[100];
    sprintf(filename, "/sdcard/framedump_%dx%d.yuv", displayWidth, displayHeight);
    pOutFile = fopen(filename, "wb");
    if(pOutFile == NULL)
        LOGE("\n!!!!!!!!!!!!!!!!Error opening file %s !!!!!!!!!!!!!!!!!!!!", filename);

#endif

EXIT:
    ;
}


TIFlashHWRenderer::~TIFlashHWRenderer()
{
    LOG_FUNCTION_NAME_ENTRY
}


status TIFlashHWRenderer::queryBufferInfo()
{
    /* Note:
    * Need to put a more stricter control before static casting.
    * Should be 100% sure that the call is with TIOMXCodec as
    * source.
    */
    if(mInfoType == FlashHWRenderer::HW_DECODER) {
        TIOMXCodec *source = static_cast<TIOMXCodec *>(mVideoSource.get());
        mBufferCount = source->getBufferCount();
        mVideoSource->getFormat()->findInt32(kKeyWidth, &mDecodedWidth);
        mVideoSource->getFormat()->findInt32(kKeyHeight, &mDecodedHeight);
        //LOGV("Codec Width x Height  %d x %d  and buf count %d\n", mDecodedWidth, mDecodedHeight, mBufferCount);
        return OK;
    }
    return UNKNOWN_ERROR;
}

int TIFlashHWRenderer::getVideoFormat()
{
    switch(mColorFormat){
        case OMX_COLOR_FormatYUV420PackedSemiPlanar:
        case OMX_COLOR_FormatYUV420SemiPlanar:
            return OVERLAY_FORMAT_YCbCr_420_SP;
        case OMX_COLOR_FormatCbYCrY:
            return OVERLAY_FORMAT_CbYCrY_422_I;
        case OMX_COLOR_FormatYCbYCr:
            return OVERLAY_FORMAT_YCbYCr_422_I;
        default:
            return OVERLAY_FORMAT_DEFAULT;
    }
}


status TIFlashHWRenderer::dequeueBuffer(MediaBuffer** mediaBuf)
{
    Mutex::Autolock lock(mLock);
    media_buf_t *mBuf;
    uint32_t index;

    /* first serve all the reclaimable buffers */
    for(uint32_t cnt = 0; cnt < mDisplayList.size(); ++cnt) {
        mBuf = mDisplayList.editItemAt(cnt);
        if(mBuf->yes ) {
            if(mBuf->reclaim) {
                -- mNumBufsDisplay ;
                *mediaBuf = (MediaBuffer *)mBuf->medBuf;
                mBuf->set(false, false, NULL);
                return OK;
            }
        }
    }

    /* If you are here then we did not have any reclaimable buffers */
    if(mOverlayRenderer->dequeueBuffer(&index) < 0 ){
        *mediaBuf = NULL;
        return RENDER_ERR_DEQUE;
    }

    if((int)index < mOverlayAddresses.size()) {
        mBuf = mDisplayList.editItemAt((int)index);
        if(!mBuf->yes) LOGE("Hmm this Index is not marked in Display list, still returning");
        -- mNumBufsDisplay ;
        *mediaBuf = (MediaBuffer *)mBuf->medBuf;
        mBuf->set(false, false, NULL);
    }

    return OK;
}

status TIFlashHWRenderer::queueBuffer(MediaBuffer* mediaBuf)
{
    Mutex::Autolock lock(mLock);
    uint32_t index = 0;

    /* Note: This loop is ONLY for strictly for HW Rendering
    * later we can make sure we take in any SW buffers
    * and put in an available buffer from Overlaybuf pool
    */
    for(index=0; index < mOverlayAddresses.size(); index++) {
        if(mOverlayAddresses[index]->pointer() == mediaBuf->data()) {
            //LOGV("This is VALID Buffer :%d.  movlyAddr[index]->pointer = %p, media->data = %p", index, mOverlayAddresses[index]->pointer(), mediaBuf->data());
            break;
        }
    }

    if(index == mOverlayAddresses.size()) {
        LOGD("This is NOT Valid Buffer");
        return RENDER_ERR_INV_BUF;
    }

    /* Check for crop */
    int offsetinPixels = mediaBuf->range_offset();
    int cropX = -1, cropY = -1;
    if(offsetinPixels < mOverlayAddresses[index]->size()) {
        //LOGV("offsetinPixels = %d, size = %d", offsetinPixels, mOverlayAddresses[index]->size());
        cropY = (offsetinPixels) /ARMPAGESIZE;
        cropX = (offsetinPixels)%ARMPAGESIZE;
        if( (cropY != mCropY) || (cropX != mCropX)) {
            mCropY = cropY;
            mCropX = cropX;
            //LOGD("Setting crop : %d %d  %d x %d",cropX, cropY, mDisplayWidth, mDisplayHeight);
            mOverlayRenderer->setCrop((uint32_t)cropX, (uint32_t)cropY, mDisplayWidth, mDisplayHeight);
        }
    }

#ifdef __DUMP_TO_FILE__
    static int displaycount = 0;
    static int lastframetodump = FIRST_FRAME_INDEX+NUM_BUFFERS_TO_DUMP;

    if ((displaycount >= FIRST_FRAME_INDEX)  && (displaycount < lastframetodump)){
        dumpBufferToFile(mediaBuf->data(), mCropX, mCropY, mDisplayWidth, mDisplayHeight, 4096);
    }
    displaycount++;
#endif


    /* If we are here, then we have a known buffer coming in */
    int displays_buf_count = mOverlayRenderer->queueBuffer(index);
    if(displays_buf_count < 0 ) {
        return RENDER_ERR_QUE;
    }

    ++mNumBufsDisplay;
    media_buf_t *mBuf = mDisplayList.editItemAt(index);
    mBuf->set(true, false,(void *)mediaBuf);
    if(mNumBufsDisplay != displays_buf_count){ /* This condition occurs when STREAM OFF happens */
        LOGE("There is a mismatch between Render's  and Display Device's (%d != %d) count", mNumBufsDisplay, displays_buf_count);
        /* Everything else as reclaimable */
        for(uint32_t cnt =0; cnt < mDisplayList.size(); ++cnt) {
            if(cnt != index){ /* Not touching the recently enqued buffer */
                /* changing all the others */
                mBuf = mDisplayList.editItemAt(cnt);
                LOGD("Marking Reclaim if [%s] on index %d", BOOL_STR(mBuf->yes), cnt);
                if(mBuf->yes) mBuf->reclaim = true;
            }
        }//end of reclaim
        return RENDER_ERR_STREAM_OFF;
    }//End of handling STREAM OFF condition

    return OK;
}

status TIFlashHWRenderer::reclaimBuffer(MediaBuffer ** mediaBuf)
{
    LOG_FUNCTION_NAME_ENTRY

    Mutex::Autolock lock(mLock);
    media_buf_t *mBuf;
    uint32_t index;
    MediaBuffer* mediaBuffer = NULL;

    while(mNumBufsDisplay > 1){
        if(mOverlayRenderer->dequeueBuffer(&index) < 0 ){
            break;
        }
        else {
            if((int)index < mOverlayAddresses.size()) {
                mBuf = mDisplayList.editItemAt((int)index);
                if(mBuf->yes) mBuf->reclaim = true;
                else LOGE("Hmm this Index is not marked in Display list, still returning");
                -- mNumBufsDisplay ;
            }
            else LOGE("Overlay returned bad index!");
        }
    }

    uint32_t k =0;
    for(uint32_t cnt = 0; cnt < mDisplayList.size(); ++cnt) {
        mBuf = mDisplayList.editItemAt(cnt);
        if(mBuf->yes ) {
            if(mBuf->reclaim) {
            /*********************************************************/
            /************* UNFINISHED & UNTESTED CODE ****************/
            /*********************************************************/
                //mediaBuf[k] = (MediaBuffer *)mBuf->medBuf;
                //mBuf->set(false, false, NULL);
                k++;
            }
        }
    }

    /////////////// remove this

    for(uint32_t cnt = 0; cnt < mDisplayList.size(); cnt++) {
        mBuf = mDisplayList.editItemAt(cnt);
        if(mBuf->yes ) {
            mediaBuffer = (MediaBuffer *)mBuf->medBuf;
            mediaBuffer->release();
        }
    }

    return OK;
}

status TIFlashHWRenderer::getStatus()
{
    return OK;
}

status TIFlashHWRenderer::setCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    return mOverlayRenderer->setCrop(x, y, w, h);
}

status TIFlashHWRenderer::getCrop(uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h)
{
    //return mOverlayRenderer->getCrop(x, y, w, h);
    LOGD("Not Implemented");
    return OK;
}

int32_t TIFlashHWRenderer::getBufferCount()
{
    return mOverlayRenderer->getBufferCount();
}

status TIFlashHWRenderer::setPosition(int32_t x, int32_t y)
{
    return mOverlayRenderer->setPosition(x, y);
}

status TIFlashHWRenderer::setSize(uint32_t w, uint32_t h)
{
    return mOverlayRenderer->setSize(w, h);
}

status TIFlashHWRenderer::setOrientation(int orientation, uint32_t flags)
{
    return mOverlayRenderer->setOrientation(orientation, flags);
}

status TIFlashHWRenderer::setAlpha(float alpha)
{
    LOGD("Not Implemented");
    return OK;
}

status TIFlashHWRenderer::setParameter(int param, void *value)
{
    LOGD("Not Implemented");
    return OK;
}

status TIFlashHWRenderer::getParameter(int param, void *value)
{
    LOGD("Not Implemented");
    return OK;
}

}


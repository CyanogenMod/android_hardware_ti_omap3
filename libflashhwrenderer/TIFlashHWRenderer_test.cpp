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
#define LOG_TAG "TIFlashHWRenderer_test"

#include <stdint.h>
#include <dlfcn.h>   /* For dynamic loading */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cutils/log.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <surfaceflinger/ISurface.h>
#include <utils/RefBase.h>
#include <utils/Errors.h>

#include <media/MediaPlayerInterface.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/FileSource.h>
#include <media/stagefright/MediaBuffer.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/MediaDebug.h>
#include <media/stagefright/MediaSource.h>
#include <media/stagefright/MetaData.h>

#include "FlashHWRenderer.h"
#include "../libstagefrighthw/TIOMXCodec.h"

#include <stdio.h>
#include <string.h>

#define LOG_FUNCTION_NAME_ENTRY    LOGD(" ###### Calling %s() Line #%d ++ ######",  __FUNCTION__, __LINE__);
#define LOG_FUNCTION_NAME_EXIT    LOGD(" ###### Calling %s() -- ######",  __FUNCTION__);

using namespace android;

static const char* FLASH_RENDERER_LIBRARY_NAME = "libflashhwrenderer.so";
static const char* FLASH_RENDERER_FACTORY_NAME = "FactoryCreateHWRenderer";

typedef FlashHWRenderer* (*FlashHWRendererFactory)(
                            const sp<MediaSource> &source,
                            int colorFormat,
                            int32_t displayWidth,
                            int32_t displayHeight,
                            int decoderType, // = 0,
                            int32_t decodedWidth, // = 0, //padded W from Decoder
                            int32_t decodedHeight, // =0, //padded H from Decoder
                            int infoType,
                            void * info /* = NULL */); //any additional flags specific to HW Renderers.

static const char* TI_OMXCODEC_FACTORY_NAME = "createPlatformOMXCodec";
typedef sp<MediaSource> (*TIOMXCodecFactory)(
                            const sp<IOMX> &omx,
                            const sp<MetaData> &meta,
                            const sp<MediaSource> &source,
                            const char *matchComponentName,
                            uint32_t flags);

class TestFlash
{
public:
    OMXClient mClient;
    sp<ProcessState> mProc;
    sp<DataSource> mFileSource;
    sp<MediaSource> mVideoSource;
    sp<MediaSource> mVideoTrack;
    int32_t mVideoWidth, mVideoHeight;
    int32_t mDecodedWidth, mDecodedHeight;
    int64_t mDurationUs;
    sp<MediaSource> mAudioTrack;
    mutable Mutex mLock;
    uint32_t mExtractorFlags;
    status_t mInitCheck;
    FlashHWRenderer* mRenderer;
    void *mLibHandle;

    TestFlash();
    ~TestFlash();
    status_t setDataSource();
    status_t setDataSource_l(const sp<MediaExtractor> &extractor);
    status_t setDataSource_l(const sp<DataSource> &dataSource);
    status_t prepare();
    status_t start();
    status_t stop();
    status_t play(uint32_t frames, uint32_t delay_in_ms);
    status_t initVideoDecoder();
    status_t initVideoRenderer();
    //void dump_output_bufs(int frames);
};



int main()
{

    LOGV("Test Flash Player Prototype");
    TestFlash player;
    if( OK == player.prepare()) {
        if(OK == player.start()){
            LOGD("Playing Now.....");
            //player.dump_output_bufs(60);
            player.play(300, 33);
        }
    }
    else {
        LOGE("Failed to prepare\n");
    }
    player.stop();

    LOGE("BYE");
    return 0;
}


TestFlash::TestFlash()
{
    mRenderer = NULL;
    mInitCheck = UNKNOWN_ERROR;
    mClient.connect();

    // set up the thread-pool
    mProc = (ProcessState::self());
    ProcessState::self()->startThreadPool();

    mLibHandle = ::dlopen(FLASH_RENDERER_LIBRARY_NAME, RTLD_NOW);
    if (mLibHandle != NULL) {
        LOGD("Flash hardware module loaded");
    } else {
        LOGD("Flash hardware module not found");
    }

    if(OK == setDataSource()) {
        LOGV("SUCCESS: SetDataSource()");
        mInitCheck = OK;
    }
    else LOGE("Error in SetDataSource()");

}

TestFlash::~TestFlash()
{
    LOGD("TestFlash::~TestFlash()");

    if (mRenderer) mRenderer->Release();

    if (mVideoSource != NULL)
        mVideoSource.clear();

#if 0
    if (mVideoSource != NULL) {
        // The following hack is necessary to ensure that the OMX
        // component is completely released by the time we may try
        // to instantiate it again.
        wp<MediaSource> tmp = mVideoSource;
        mVideoSource.clear();
        while (tmp.promote() != NULL) {
            usleep(1000);
        }
    }
#endif

    mClient.disconnect();
    if (mLibHandle != NULL) {
        ::dlclose(mLibHandle);
    }
    //IPCThreadState::self()->joinThreadPool();

}


status_t TestFlash::initVideoRenderer()
{
    if (mLibHandle != NULL) {
        FlashHWRendererFactory f = (FlashHWRendererFactory) ::dlsym(mLibHandle, FLASH_RENDERER_FACTORY_NAME);
        if (f != NULL) {
            LOGE("Calling linked TIFlashHWRenderer");
            mRenderer = f(mVideoSource, OMX_COLOR_FormatYUV420PackedSemiPlanar, mVideoWidth, mVideoHeight, 0, 0, 0, 0, NULL);
        }
        else LOGE("Error in linking TIFlashHWRenderer");

        if(mRenderer == NULL) {
            LOGE("Error in Initializing TIFlashHWRenderer");
            return UNKNOWN_ERROR;
        }

        mRenderer->setPosition(32, 40);
        mRenderer->setSize(800, 400);
        mRenderer->setOrientation(0, 0);

        return OK;
    }
    return UNKNOWN_ERROR;
}


status_t TestFlash::initVideoDecoder()
{
    if (mLibHandle == NULL) return UNKNOWN_ERROR;

    TIOMXCodecFactory f = (TIOMXCodecFactory) ::dlsym(mLibHandle, TI_OMXCODEC_FACTORY_NAME);
    if (f != NULL) {
        mVideoSource = f(mClient.interface(), mVideoTrack->getFormat(), mVideoTrack, NULL, 0);
    }

    if(mVideoSource == NULL) {
        LOGE("Error in Initializing TIOMXCodec");
        return UNKNOWN_ERROR;
    }

    int64_t durationUs;
    if (mVideoTrack->getFormat()->findInt64(kKeyDuration, &durationUs)) {
        //Mutex::Autolock autoLock(mMiscStateLock);
        if (mDurationUs < 0 || durationUs > mDurationUs) {
            mDurationUs = durationUs;
        }
    }

    CHECK(mVideoTrack->getFormat()->findInt32(kKeyWidth, &mVideoWidth));
    CHECK(mVideoTrack->getFormat()->findInt32(kKeyHeight, &mVideoHeight));
    LOGD("Bitstream Width x Height  %d x %d \n", mVideoWidth, mVideoHeight);
    CHECK(mVideoSource->getFormat()->findInt32(kKeyWidth, &mDecodedWidth));
    CHECK(mVideoSource->getFormat()->findInt32(kKeyHeight, &mDecodedHeight));
    LOGD("Codec Width x Height  %d x %d \n", mVideoWidth, mVideoHeight);

    return mVideoSource != NULL ? OK : UNKNOWN_ERROR;
}

#if 0
void TestFlash::dump_output_bufs(int frames)
{
    MediaBuffer* medBuf = NULL;
    status_t err;
    int count =0;

    while(count < frames) {
        err = mVideoSource->read(&medBuf);

        if(medBuf != NULL) {
            if(err == OK) {
                LOGV("DUMP: OK: Buf Num: [%d]:  data: %x   size: %d   range_length: %d  range_offset: %d", count,
                medBuf->data(), medBuf->size(), medBuf->range_length(), medBuf->range_offset());

                if(fp !=NULL){
                    char * dataptr = (char *)(medBuf->data()+medBuf->range_offset());
                    for(int i = 0; i< mVideoHeight; ++i) {
                        //ProcMgr_invalidateMemory(dataptr,mVideoWidth, 3);
                        fwrite(dataptr, 1, mVideoWidth, fp);
                        fflush(fp);
                        dataptr += 4096;
                    }
                }


            }
            if(err != OK){
                LOGV("DUMP:NOT OK: [%d] Buf Num: [%d]:  data: %x   size: %d   range_length: %d  range_offset: %d", count, err,
                medBuf->data(), medBuf->size(), medBuf->range_length(), medBuf->range_offset());
            }
            medBuf->release();
            medBuf = NULL;
        } 
        else LOGE("DUMP: ERR: %d", err);

        ++count;
    }

    if(fp)
        fclose(fp);

}
#endif

status_t TestFlash::play(uint32_t frames, uint32_t delay_in_ms)
{
    MediaBuffer* medBuf;
    status_t err;
    //dump_output_bufs(frames);
    if(mVideoSource.get() == NULL)
        return UNKNOWN_ERROR;

    for(int fcount = 0; fcount < frames ; ++fcount) {

        /* Render Loop */
        medBuf == NULL;
        err = mVideoSource->read(&medBuf);
        //if(err == INFO_FORMAT_CHANGED) {
        //	LOGV("INFO FORMAT CHANGED Recevied");}
        if(err == OK) {
            LOGV(" Data %x  size %d range_offset %d", medBuf->data(), medBuf->size(), medBuf->range_offset());
            err = mRenderer->queueBuffer(medBuf);
            if(err != OK) {
                LOGE("Error Queing %x", err);
                medBuf->release();
            }
            medBuf = NULL;
        }
        else  {
            LOGE("Error IN READ %d", err);
            if(medBuf) medBuf->release();
        }
        /* Deque as much as you can */
        for(;;) {
            err = mRenderer->dequeueBuffer(&medBuf);
            if(err != OK)	break;
            else {
                LOGD("Releasing Buffer");
                if(medBuf != NULL) medBuf->release();
                medBuf = NULL;
            }
        }
        usleep(delay_in_ms * 1000);
    }
    return OK;
}




status_t TestFlash::start()
{
    status_t err = mVideoSource->start();

    if(err != OK) {
        LOGE("Error Starting OMXVideoCodec");
    }
    return err;
}

status_t TestFlash::stop()
{
    MediaBuffer* mediaBuffer = NULL;
    mRenderer->reclaimBuffer(&mediaBuffer);

    status_t err = mVideoSource->stop();

    if(err != OK) {
        LOGE("Error Starting OMXVideoCodec");
    }
    return err;
}

status_t TestFlash::prepare()
{
    status_t err;
    if(mVideoSource == NULL)
        err = initVideoDecoder();

    if(err == OK) {
        LOGD("Successfully allocated Decoder");
        err = initVideoRenderer();

        if(err == OK)
            LOGD("Successfully allocated Renderer");

    }
    else {
        LOGE("Failed to Initialize Decoder");
    }

    return err;
}


status_t TestFlash::setDataSource() {
    Mutex::Autolock autoLock(mLock);

    sp<DataSource> dataSource = new FileSource("nasa.mp4");
    status_t err = dataSource->initCheck();

    if (err != OK) {
        return err;
    }
    mFileSource = dataSource;
    return setDataSource_l(dataSource);
}

status_t TestFlash::setDataSource_l(const sp<DataSource> &dataSource) {

    sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource,"video/mpeg4");
    if (extractor == NULL) {
        return UNKNOWN_ERROR;
    }
    return setDataSource_l(extractor);
}


status_t TestFlash::setDataSource_l(const sp<MediaExtractor> &extractor) {
    bool haveAudio = false;
    bool haveVideo = false;

    for (size_t i = 0; i < extractor->countTracks(); ++i) {
        sp<MetaData> meta = extractor->getTrackMetaData(i);

        const char *mime;
        CHECK(meta->findCString(kKeyMIMEType, &mime));

        if (!haveVideo && !strncasecmp(mime, "video/", 6)) {
            mVideoTrack = extractor->getTrack(i);
            haveVideo = true;
        }
        else if (!haveAudio && !strncasecmp(mime, "audio/", 6)) {
            mAudioTrack = extractor->getTrack(i);
            haveAudio = true;
        }

        if (haveAudio && haveVideo) {
            break;
        }
    }

    if (!haveAudio && !haveVideo) {
    LOGE("Does not have Audio(%d) or Video (%d)", haveAudio, haveVideo);
    return UNKNOWN_ERROR;
    }

    mExtractorFlags = extractor->flags();

    LOGD("SUCCESS, End of MediaExtractors");

    return OK;
}



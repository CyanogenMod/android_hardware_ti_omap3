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


#include "CameraHal.h"

#define OPTIMAL_BUFFER_COUNT_WITH_DISPLAY 3

namespace android {

/**
 * Display handler class - This class basically handles the buffer posting to display
 */

class OverlayDisplayAdapter : public DisplayAdapter
{
public:

    typedef struct
        {
        void *mBuffer;
        void *mUser;
        int mOffset;
        int mWidth;
        int mHeight;
        int mWidthStride;
        int mHeightStride;
        CameraFrame::FrameType mType;
        } DisplayFrame;

    enum DisplayStates
        {
        DISPLAY_INIT = 0,
        DISPLAY_STARTED,
        DISPLAY_STOPPED,
        DISPLAY_EXITED
        };

public:

    OverlayDisplayAdapter();
    virtual ~OverlayDisplayAdapter();

    ///Initializes the display adapter creates any resources required
    virtual status_t initialize();

    virtual int setOverlay(const sp<Overlay> &overlay);
    virtual int setFrameProvider(FrameNotifier *frameProvider);
    virtual int setErrorHandler(ErrorNotifier *errorNotifier);
    virtual int enableDisplay(struct timeval *refTime = NULL, S3DParameters *s3dParams = NULL);
    virtual int disableDisplay();
    virtual status_t pauseDisplay(bool pause);

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS

    //Used for shot to snapshot measurement
    virtual status_t setSnapshotTimeRef(struct timeval *refTime = NULL);

#endif

    virtual int useBuffers(void* bufArr, int num);
    virtual bool supportsExternalBuffering();

    //Implementation of inherited interfaces
    virtual void* allocateBuffer(int width, int height, const char* format, int &bytes, int numBufs);
    virtual uint32_t * getOffsets() ;
    virtual int getFd() ;
    virtual int freeBuffer(void* buf);

    ///Class specific functions
    static void frameCallbackRelay(CameraFrame* caFrame);
    void frameCallback(CameraFrame* caFrame);

    void displayThread();

    private:
    void destroy();
    bool processHalMsg();
    status_t PostFrame(OverlayDisplayAdapter::DisplayFrame &dispFrame);
    bool handleFrameReturn();

public:

    static const int DISPLAY_TIMEOUT;
    static const int FAILED_DQS_TO_SUSPEND;

    class DisplayThread : public Thread
        {
        OverlayDisplayAdapter* mDisplayAdapter;
        MessageQueue mDisplayThreadQ;

        public:
            DisplayThread(OverlayDisplayAdapter* da)
            : Thread(false), mDisplayAdapter(da) { }

        ///Returns a reference to the display message Q for display adapter to post messages
            MessageQueue& msgQ()
                {
                return mDisplayThreadQ;
                }

            virtual bool threadLoop()
                {
                mDisplayAdapter->displayThread();
                return false;
                }

            enum DisplayThreadCommands
                {
                DISPLAY_START,
                DISPLAY_STOP,
                DISPLAY_FRAME,
                DISPLAY_EXIT
                };
        };

    //friend declarations
friend class DisplayThread;

private:
    int postBuffer(void* displayBuf);

private:
    bool mFirstInit;
    bool mSuspend;
    int mFailedDQs;
    bool mPaused; //Pause state
    sp<Overlay>  mOverlay;
    sp<DisplayThread> mDisplayThread;
    FrameProvider *mFrameProvider; ///Pointer to the frame provider interface
    MessageQueue mDisplayQ;
    unsigned int mDisplayState;
    unsigned int mFramesWithDisplay;
    ///@todo Have a common class for these members
    mutable Mutex mLock;
    bool mDisplayEnabled;
    int mBufferCount;
    //KeyedVector<int, int> mPreviewBufferMap;
    int *mPreviewBufferMap;
    KeyedVector<int, int> mFramesWithDisplayMap;
    sp<ErrorNotifier> mErrorNotifier;

    uint32_t mFrameWidth;
    uint32_t mFrameHeight;

    uint32_t mXOff;
    uint32_t mYOff;

    const char *mPixelFormat;

#if PPM_INSTRUMENTATION || PPM_INSTRUMENTATION_ABS
    //Used for calculating standby to first shot
    struct timeval mStandbyToShot;
    bool mMeasureStandby;
    //Used for shot to snapshot/shot calculation
    struct timeval mStartCapture;
    bool mShotToShot;

#endif

};

};


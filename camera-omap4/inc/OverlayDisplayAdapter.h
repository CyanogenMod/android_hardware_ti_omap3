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
    virtual int enableDisplay();
    virtual int disableDisplay();
    virtual int useBuffers(void* bufArr, int num);
    virtual bool supportsExternalBuffering();

    //Implementation of inherited interfaces
    virtual void* allocateBuffer(int width, int height, const char* format, int bytes, int numBufs);
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
    sp<Overlay>  mOverlay;
    ErrorNotifier *mErrorNotifier;
    sp<DisplayThread> mDisplayThread;
    FrameProvider *mFrameProvider; ///Pointer to the frame provider interface
    MessageQueue mDisplayQ;
    unsigned int mDisplayState;
    unsigned int mFramesWithDisplay;
    ///@todo Have a common class for these members
    mutable Mutex mLock;
    bool mDisplayEnabled;
    KeyedVector<int, int> mPreviewBufferMap;

};

};


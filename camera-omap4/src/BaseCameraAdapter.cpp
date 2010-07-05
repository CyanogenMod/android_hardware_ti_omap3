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


#define LOG_TAG "CameraHal"

#include "BaseCameraAdapter.h"

namespace android {


/*--------------------Camera Adapter Class STARTS here-----------------------------*/

void BaseCameraAdapter::enableMsgType(int32_t msgs, frame_callback callback, event_callback eventCb, void* cookie)
{
    LOG_FUNCTION_NAME

    if ( CameraFrame::PREVIEW_FRAME_SYNC == msgs )
        {
            {
            Mutex::Autolock lock(mSubscriberLock);
            mFrameSubscribers.add((int) cookie, callback);
            }
        }
    else if ( CameraFrame::IMAGE_FRAME == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mImageSubscribers.add((int) cookie, callback);
            }
        }
    else if ( CameraFrame::RAW_FRAME == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mRawSubscribers.add((int) cookie, callback);
            }
        }
    else if ( CameraFrame::VIDEO_FRAME_SYNC == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mVideoSubscribers.add((int) cookie, callback);
            }
        }
    else if ( CameraHalEvent::ALL_EVENTS == msgs)
        {
         //Subscribe only for focus
         //TODO: Process case by case
            {
                Mutex::Autolock lock(mSubscriberLock);
                mFocusSubscribers.add((int) cookie, eventCb);
            }

            {
                Mutex::Autolock lock(mSubscriberLock);
                mShutterSubscribers.add((int) cookie, eventCb);
            }

        }
    else
        {
        CAMHAL_LOGEA("Message type subscription no supported yet!");
        }

    LOG_FUNCTION_NAME_EXIT
}

void BaseCameraAdapter::disableMsgType(int32_t msgs, void* cookie)
{
    LOG_FUNCTION_NAME

    if ( CameraFrame::PREVIEW_FRAME_SYNC == msgs )
        {
            {
            Mutex::Autolock lock(mSubscriberLock);
            mFrameSubscribers.removeItem((int) cookie);
            }
        }
    else if ( CameraFrame::IMAGE_FRAME == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mImageSubscribers.removeItem((int) cookie);
            }
        }
    else if ( CameraFrame::RAW_FRAME == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mRawSubscribers.removeItem((int) cookie);
            }
        }
    else if ( CameraFrame::VIDEO_FRAME_SYNC == msgs)
        {
            {
                Mutex::Autolock lock(mSubscriberLock);
                mVideoSubscribers.removeItem((int) cookie);
            }
        }
    else if ( CameraHalEvent::ALL_EVENTS == msgs)
        {
         //Subscribe only for focus
         //TODO: Process case by case
            {
                Mutex::Autolock lock(mSubscriberLock);
                mFocusSubscribers.removeItem((int) cookie);
            }
            {
                Mutex::Autolock lock(mSubscriberLock);
                mShutterSubscribers.removeItem((int) cookie);
            }

        }
    else
        {
        CAMHAL_LOGEB("Message type 0x%x subscription no supported yet!", msgs);
        }

    LOG_FUNCTION_NAME_EXIT
}

};


/*--------------------Camera Adapter Class ENDS here-----------------------------*/




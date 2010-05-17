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
/**
* @file CameraHalUtilClasses.cpp
*
* This file maps the CameraHardwareInterface to the Camera interfaces on OMAP4 (mainly OMX).
*
*/

#define LOG_TAG "CameraHal"


#include "CameraHal.h"

namespace android {

/*--------------------FrameProvider Class STARTS here-----------------------------*/

int FrameProvider::enableFrameNotification(int32_t frameTypes)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;

    ///Enable the frame notification to CameraAdapter (which implements FrameNotifier interface)
    mFrameNotifier->enableMsgType(frameTypes<<MessageNotifier::FRAME_BIT_FIELD_POSITION
                                    , mFrameCallback
                                    , NULL
                                    , mCookie
                                    );

    LOG_FUNCTION_NAME_EXIT;
    return ret;
}

int FrameProvider::disableFrameNotification(int32_t frameTypes)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;

    mFrameNotifier->disableMsgType(frameTypes<<MessageNotifier::FRAME_BIT_FIELD_POSITION
                                    , mCookie
                                    );

    LOG_FUNCTION_NAME_EXIT;
    return ret;
}

int FrameProvider::returnFrame(void *frameBuf, CameraFrame::FrameType frameType)
{
    status_t ret = NO_ERROR;

    mFrameNotifier->returnFrame(frameBuf, frameType);

    return ret;
}


/*--------------------FrameProvider Class ENDS here-----------------------------*/

/*--------------------EventProvider Class STARTS here-----------------------------*/

int EventProvider::enableEventNotification(int32_t frameTypes)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;

    ///Enable the frame notification to CameraAdapter (which implements FrameNotifier interface)
    mEventNotifier->enableMsgType(frameTypes<<MessageNotifier::EVENT_BIT_FIELD_POSITION
                                    , NULL
                                    , mEventCallback
                                    , mCookie
                                    );

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

int EventProvider::disableEventNotification(int32_t frameTypes)
{
    LOG_FUNCTION_NAME
    status_t ret = NO_ERROR;

    mEventNotifier->disableMsgType(frameTypes<<MessageNotifier::FRAME_BIT_FIELD_POSITION
                                    , mCookie
                                    );

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

/*--------------------EventProvider Class ENDS here-----------------------------*/

};

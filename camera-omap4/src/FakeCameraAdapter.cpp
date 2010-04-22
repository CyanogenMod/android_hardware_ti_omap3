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


#include "FakeCameraAdapter.h"

namespace android {


/*--------------------Camera Adapter Class STARTS here-----------------------------*/

void FakeCameraAdapter::enableMsgType(int32_t msgs, frame_callback callback, event_callback eventCb, void* cookie)
{
    LOG_FUNCTION_NAME
    ///DisplayAdapter and AppCallbackNotifier call this method for registering their event and frame handler
    ///callbacks
    LOG_FUNCTION_NAME_EXIT
}

void FakeCameraAdapter::disableMsgType(int32_t msgs, void* cookie)
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

}

void FakeCameraAdapter::returnFrame(void* frameBuf)
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT
}

int FakeCameraAdapter::setErrorHandler(ErrorNotifier *errorNotifier)
{
    LOG_FUNCTION_NAME
    return 0;
}

status_t FakeCameraAdapter::getCaps()
{
    LOG_FUNCTION_NAME
    return NO_ERROR;
}

status_t FakeCameraAdapter::initialize()
{
    LOG_FUNCTION_NAME
    return NO_ERROR;
}


status_t FakeCameraAdapter::setParameters(const CameraParameters& params)
{
    LOG_FUNCTION_NAME
    return NO_ERROR;
}

CameraParameters FakeCameraAdapter::getParameters() const
{
    LOG_FUNCTION_NAME
    return mParameters;
}

//API to send a command to the camera
status_t FakeCameraAdapter::sendCommand(int operation, int value1, int value2, int value3)
{
    LOG_FUNCTION_NAME
    return NO_ERROR;
}

//API to cancel a currently executing command
status_t FakeCameraAdapter::cancelCommand(int operation)
{
    LOG_FUNCTION_NAME
    return NO_ERROR;
}

//API to get the frame size required to be allocated. This size is used to override the size passed
//by camera service when VSTAB/VNF is turned ON for example
void FakeCameraAdapter::getFrameSize(int &width, int &height)
{
    LOG_FUNCTION_NAME
}

extern "C" CameraAdapter* CameraAdapter_Factory()
{
    LOG_FUNCTION_NAME
    return new FakeCameraAdapter;
}



};


/*--------------------Camera Adapter Class ENDS here-----------------------------*/



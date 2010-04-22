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


#ifndef FAKE_CAMERA_ADAPTER_H
#define FAKE_CAMERA_ADAPTER_H

#include "CameraHal.h"

namespace android {


class FakeCameraAdapter : public CameraAdapter
{
public:
        ///Initialzes the camera adapter creates any resources required
        virtual status_t initialize();

        virtual int setErrorHandler(ErrorNotifier *errorNotifier);

        //Message/Frame notification APIs
        virtual void enableMsgType(int32_t msgs, frame_callback callback=NULL, event_callback eventCb=NULL, void* cookie=NULL);
        virtual void disableMsgType(int32_t msgs, void* cookie);
        virtual void returnFrame(void* frameBuf);

        //APIs to configure Camera adapter and get the current parameter set
        virtual status_t setParameters(const CameraParameters& params);
        virtual CameraParameters getParameters() const;

        //API to get the caps
        virtual status_t getCaps();

        //API to send a command to the camera
        virtual status_t sendCommand(int operation, int value1=0, int value2=0, int value3=0);

        //API to cancel a currently executing command
        virtual status_t cancelCommand(int operation);

        //API to get the frame size required to be allocated. This size is used to override the size passed
        //by camera service when VSTAB/VNF is turned ON for example
        virtual void getFrameSize(int &width, int &height);
private:
        CameraParameters mParameters;

};

};

#endif //FAKE_CAMERA_ADAPTER_H


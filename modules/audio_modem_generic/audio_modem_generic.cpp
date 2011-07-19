/* audio_modem_generic.cpp
 **
 ** Copyright (C) 2009-2010, Texas Instruments
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#define LOG_NDEBUG 0
#define LOG_TAG "AudioModemGeneric"

#include <utils/Log.h>
#include <utils/String8.h>

#include "audio_modem_interface.h"
#include "audio_modem_generic.h"


namespace android
{
extern "C"
{
    //
    // Function for dlsym() to look up for creating a new AudioModemInterface.
    //
    AudioModemInterface* createAudioModemInterface() {
        return new AudioModemInterfaceGeneric();
    }
}         // extern "C"
// ----------------------------------------------------------------------------

AudioModemInterfaceGeneric::AudioModemInterfaceGeneric()
{
    LOGV("%s", __FUNCTION__);
    LOGV("Build date: %s time: %s", __DATE__, __TIME__);
}

AudioModemInterfaceGeneric::~AudioModemInterfaceGeneric()
{
    LOGV("%s", __FUNCTION__);
}

status_t AudioModemInterfaceGeneric::initCheck()
{
    LOGV("%s", __FUNCTION__);

    return NO_ERROR;
}

status_t AudioModemInterfaceGeneric::setModemRouting(uint32_t routes,
                                                              uint32_t sampleRate)
{
    LOGV("%s: routes %d sampleRate %d", __FUNCTION__, routes, sampleRate);

    return NO_ERROR;
}

status_t AudioModemInterfaceGeneric::setModemAudioOutputVolume(float nearEndvolume,
                                                                        float farEndvolume)
{
    LOGV("%s: nearEndvolume %f2.1 farEndvolume %f2.1", __FUNCTION__, nearEndvolume, farEndvolume);

    return NO_ERROR;
}

status_t AudioModemInterfaceGeneric::setModemAudioInputVolume(float nearEndvolume,
                                                           float farEndvolume)
{
    LOGV("%s: nearEndvolume %f2.1 farEndvolume %f2.1", __FUNCTION__, nearEndvolume, farEndvolume);

    return NO_ERROR;
}

status_t AudioModemInterfaceGeneric::setModemVoiceCallMultiMic(int multiMic)
{
    LOGV("%s: multimic %d", __FUNCTION__, multiMic);

    return NO_ERROR;
}

status_t AudioModemInterfaceGeneric::OpenModemVoiceCallStream()
{
    LOGV("%s", __FUNCTION__);

    return NO_ERROR;
}

status_t AudioModemInterfaceGeneric::OpenModemAudioOutputStream(uint32_t sampleRate,
                                                                         int channelUsed,
                                                                         int audioSampleType)
{
    LOGV("%s: sampleRate %d, channelUsed: %d audioSampleType: %d", __FUNCTION__, sampleRate, channelUsed, audioSampleType);

    return NO_ERROR;
}

status_t AudioModemInterfaceGeneric::OpenModemAudioInputStream(uint32_t sampleRate,
                                                                        int channelUsed,
                                                                        int recordSampleType)
{
    LOGV("%s: sampleRate %d, channelUsed: %d recordSampleType: %d", __FUNCTION__, sampleRate, channelUsed, recordSampleType);

    return NO_ERROR;
}


status_t AudioModemInterfaceGeneric::CloseModemVoiceCallStream()
{
    LOGV("%s", __FUNCTION__);

    return NO_ERROR;
}

status_t AudioModemInterfaceGeneric::CloseModemAudioOutputStream()
{
    LOGV("%s", __FUNCTION__);

    return NO_ERROR;
}

status_t AudioModemInterfaceGeneric::CloseModemAudioInputStream()
{
    LOGV("%s", __FUNCTION__);

    return NO_ERROR;
}

uint32_t AudioModemInterfaceGeneric::GetVoiceCallSampleRate()
{
    LOGV("%s: Put by default 8KHz voice call", __FUNCTION__);
    return PCM_8_KHZ;
}
} // namespace android

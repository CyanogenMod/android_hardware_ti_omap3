/* alsa_omap4_modem.cpp
 **
 ** Copyright 2009-2010 Texas Instruments
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

#define LOG_TAG "Omap4ALSAModem"

#include <utils/Log.h>
#include <cutils/properties.h>
#include <dlfcn.h>

#ifdef AUDIO_BLUETOOTH
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sco.h>
#endif

#include "AudioHardwareALSA.h"
#include "audio_modem_interface.h"
#include "alsa_omap4_modem.h"

// TODO
// separate left/right volume for main/sub mic

namespace android
{
// ----------------------------------------------------------------------------
#define CHECK_ERROR(func, error)   if ((error = func) != NO_ERROR) { \
                                return error; \
                            }

// ----------------------------------------------------------------------------

AudioModemAlsa::AudioModemAlsa()
{
    status_t error;

    LOGV("Build date: %s time: %s", __DATE__, __TIME__);

    LOGD("Initializing devices for Modem OMAP4 ALSA module");

    audioModemSetProperties();
    mModem = create();
    if (mModem) {
        error = mModem->initCheck() ;
        if (error != NO_ERROR) {
            LOGE("Audio Modem Interface was not correctly initialized.");
            delete mModem;
            exit(error);
        }
    }
    else {
        LOGE("No Audio Modem Interface found.");
        exit(-1);
    }
    mVoiceCallState = AUDIO_MODEM_VOICE_CALL_OFF;
}

AudioModemAlsa::~AudioModemAlsa()
{
    LOGD("Destroy devices for Modem OMAP4 ALSA module");
    mDevicePropList.clear();
    if (mModem) delete mModem;
}

AudioModemInterface* AudioModemAlsa::create()
{
    void *dlHandle;
    char libPath[PROPERTY_VALUE_MAX];
    AudioModemInterface *(*audioModem)(void);

    property_get(AUDIO_MODEM_LIB_PATH_PROPERTY,
                 libPath,
                 AUDIO_MODEM_LIB_DEFAULT_PATH);

    LOGW_IF(!strcmp(libPath, AUDIO_MODEM_LIB_DEFAULT_PATH),
                    "Use generic Modem interface");

    dlHandle = dlopen(libPath, RTLD_NOW);
    if (dlHandle == NULL) {
        LOGE("Audio Modem %s dlopen failed: %s", libPath, dlerror());
        exit(-1);
    }

    audioModem = (AudioModemInterface *(*)(void))dlsym(dlHandle, "createAudioModemInterface");
    if (audioModem == NULL) {
        LOGE("createAudioModemInterface function not defined or not exported in %s", libPath);
        exit(-1);
    }

    return(audioModem());
}

AudioModemAlsa::AudioModemDeviceProperties::AudioModemDeviceProperties(
                                                    const char *audioModeName)
{
    if (!strcmp(audioModeName,".hand")) audioMode =
                                    AudioModemInterface::AUDIO_MODEM_HANDSET;
    else if (!strcmp(audioModeName,".free")) audioMode =
                                    AudioModemInterface::AUDIO_MODEM_HANDFREE;
    else if (!strcmp(audioModeName,".head")) audioMode =
                                    AudioModemInterface::AUDIO_MODEM_HEADSET;
    else if (!strcmp(audioModeName,".bt")) audioMode =
                                    AudioModemInterface::AUDIO_MODEM_BLUETOOTH;
    else if (!strcmp(audioModeName,".aux")) audioMode =
                                    AudioModemInterface::AUDIO_MODEM_AUX;
}

void AudioModemAlsa::AudioModemDeviceProperties::initProperties(int index,
                                                                char *propValue,
                                                                bool def)
{
    if (def) {
        strcpy((char *)settingsList[index].name,
               AUDIO_MODEM_SETTING_DEFAULT);
    } else {
        strcpy((char *)settingsList[index].name, propValue);
    }
}

status_t AudioModemAlsa::audioModemSetProperties()
{
    int i, j, k, index = 0;
    char propKey[PROPERTY_KEY_MAX], propValue[PROPERTY_VALUE_MAX];
    AudioModemDeviceProperties *deviceProp;

    mDevicePropList.clear();

    for (i = 0; i < audioModeNameLen; i++) {
        deviceProp = new AudioModemDeviceProperties(audioModeName[i]);
        index = 0;
        for (j = 0; j < modemStreamNameLen; j++) {
            for (k = 0; k < settingNameLen; k++) {
                if (settingName[k].streamUsed & (1 << j)) {
                    // modem.<audio_mode>.<stream>.<setting>
                    strcpy (propKey, PROP_BASE_NAME);
                    strcpy (propValue, "");
                    strcat (propKey, audioModeName[i]);
                    strcat (propKey, modemStreamName[j]);
                    strcat (propKey, settingName[k].name);

                    property_get(propKey, propValue, "");

                    if (!strcmp(propValue, "")) {
                        deviceProp->initProperties(index,
                                                    propValue,
                                                    true);
                    } else {
                        deviceProp->initProperties(index,
                                                    propValue,
                                                    false);
                    }

                    LOGV("%s = %s", propKey,
                                    deviceProp->settingsList[index].name);
                    index++;
                }
            }
        }
        mDevicePropList.add(deviceProp->audioMode, deviceProp);
    }
    return NO_ERROR;
}

status_t AudioModemAlsa::voiceCallControls(uint32_t devices, int mode,
                                           ALSAControl *alsaControl)
{
    status_t error = NO_ERROR;

LOGV("%s: devices %04x mode %d", __FUNCTION__, devices, mode);

    mAlsaControl = alsaControl;

    if ((mode == AudioSystem::MODE_IN_CALL) &&
        (mVoiceCallState == AUDIO_MODEM_VOICE_CALL_OFF)) {
        // enter in voice call mode
        error = setCurrentAudioModemModes(devices);
        if (error < 0) return error;
        mDeviceProp = mDevicePropList.valueFor(mCurrentAudioModemModes);
        error = voiceCallCodecSet();
        if (error < 0) return error;
        error = voiceCallModemSet();
        if (error < 0) return error;
#ifdef AUDIO_BLUETOOTH
        if (mCurrentAudioModemModes ==
                AudioModemInterface::AUDIO_MODEM_BLUETOOTH) {
            error = voiceCallBTDeviceEnable();
            if (error < 0) return error;
        }
#endif
        mVoiceCallState = AUDIO_MODEM_VOICE_CALL_ON;
    } else if ((mode == AudioSystem::MODE_IN_CALL) &&
               (mVoiceCallState == AUDIO_MODEM_VOICE_CALL_ON)) {
        // update in voice call mode
        mPreviousAudioModemModes = mCurrentAudioModemModes;
        error = setCurrentAudioModemModes(devices);
        if (error < 0) {
            mCurrentAudioModemModes = mPreviousAudioModemModes;
            return error;
        }
        mDevicePropPrevious = mDeviceProp;
        mDeviceProp = mDevicePropList.valueFor(mCurrentAudioModemModes);
        if (mCurrentAudioModemModes != mPreviousAudioModemModes) {
            error = voiceCallCodecUpdate();
            if (error < 0) return error;
            error = voiceCallModemUpdate();
            if (error < 0) return error;
#ifdef AUDIO_BLUETOOTH
            if (mCurrentAudioModemModes ==
                    AudioModemInterface::AUDIO_MODEM_BLUETOOTH) {
                error = voiceCallBTDeviceEnable();
                if (error < 0) return error;
            } else if (mPreviousAudioModemModes ==
                    AudioModemInterface::AUDIO_MODEM_BLUETOOTH) {
                error = voiceCallBTDeviceDisable();
                if (error < 0) return error;
            }
#endif
        } else {
            LOGI("Audio Modem Mode doesn't changed: no update needed");
        }
    } else if ((mode != AudioSystem::MODE_IN_CALL) &&
               (mVoiceCallState == AUDIO_MODEM_VOICE_CALL_ON)) {
        // we just exit voice call mode
        mPreviousAudioModemModes = mCurrentAudioModemModes;
        mDevicePropPrevious = mDeviceProp;
        error = voiceCallCodecReset();
        if (error < 0) return error;
        error = voiceCallModemReset();
        if (error < 0) return error;
#ifdef AUDIO_BLUETOOTH
        if (mPreviousAudioModemModes ==
                    AudioModemInterface::AUDIO_MODEM_BLUETOOTH) {
                error = voiceCallBTDeviceDisable();
                if (error < 0) return error;
        }
#endif
        mVoiceCallState = AUDIO_MODEM_VOICE_CALL_OFF;
    }

    return error;
}

status_t AudioModemAlsa::setCurrentAudioModemModes(uint32_t devices)
{
    if (devices & AudioModemInterface::AUDIO_MODEM_HANDSET) {
        mCurrentAudioModemModes = AudioModemInterface::AUDIO_MODEM_HANDSET;
    } else if (devices & AudioModemInterface::AUDIO_MODEM_HANDFREE) {
        mCurrentAudioModemModes = AudioModemInterface::AUDIO_MODEM_HANDFREE;
    } else if (devices & AudioModemInterface::AUDIO_MODEM_HEADSET) {
        mCurrentAudioModemModes = AudioModemInterface::AUDIO_MODEM_HEADSET;
    } else if (devices & AudioModemInterface::AUDIO_MODEM_AUX) {
        mCurrentAudioModemModes = AudioModemInterface::AUDIO_MODEM_AUX;
#ifdef AUDIO_BLUETOOTH
    } else if (devices & AudioModemInterface::AUDIO_MODEM_BLUETOOTH) {
        mCurrentAudioModemModes = AudioModemInterface::AUDIO_MODEM_BLUETOOTH;
#endif
    } else {
        LOGE("Devices %04x not supported", devices);
        return NO_INIT;
    }
    LOGV("New Audio Modem Modes: %04x", mCurrentAudioModemModes);
    return NO_ERROR;
}

status_t AudioModemAlsa::voiceCallCodecSet()
{
    status_t error = NO_ERROR;

    LOGV("Start Audio Codec Voice call: %04x", mCurrentAudioModemModes);

    if (mCurrentAudioModemModes & AudioModemInterface::AUDIO_MODEM_HANDSET) {
        error = voiceCallCodecSetHandset();
    } else if (mCurrentAudioModemModes & AudioModemInterface::AUDIO_MODEM_HANDFREE) {
        error = voiceCallCodecSetHandfree();
    } else if (mCurrentAudioModemModes & AudioModemInterface::AUDIO_MODEM_HEADSET) {
        error = voiceCallCodecSetHeadset();
#ifdef AUDIO_BLUETOOTH
    } else if (mCurrentAudioModemModes & AudioModemInterface::AUDIO_MODEM_BLUETOOTH) {
        error = voiceCallCodecSetBluetooth();
#endif
    } else {
        LOGE("Audio Modem mode not supported: %04x", mCurrentAudioModemModes);
        return INVALID_OPERATION;
    }

    error = voiceCallCodecPCMSet();

    return error;
}

status_t AudioModemAlsa::voiceCallCodecPCMSet()
{
    int error;
    unsigned int sampleRate;

    if ((error = snd_pcm_open(&cHandle,
                    AUDIO_MODEM_PCM_HANDLE_NAME,
                    SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        LOGE("Modem PCM capture open error: %d:%s", error, snd_strerror(error));
        return INVALID_OPERATION;
    }
    if ((error = snd_pcm_open(&pHandle,
                    AUDIO_MODEM_PCM_HANDLE_NAME,
                    SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        LOGE("Modem PCM playback open error: %d:%s", error, snd_strerror(error));
        return INVALID_OPERATION;
    }

    if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_SAMPLERATE].name,
                "16Khz")) {
        sampleRate = 16000;
    } else {
        sampleRate = 8000;
    }

    if ((error = snd_pcm_set_params(cHandle,
                    SND_PCM_FORMAT_S16_LE,
                    SND_PCM_ACCESS_RW_INTERLEAVED,
                    1,
                    sampleRate,
                    1,
                    AUDIO_MODEM_PCM_LATENCY)) < 0) {
        LOGE("Modem PCM capture params error: %s", snd_strerror(error));
        return INVALID_OPERATION;
    }
    if ((error = snd_pcm_set_params(pHandle,
                    SND_PCM_FORMAT_S16_LE,
                    SND_PCM_ACCESS_RW_INTERLEAVED,
                    1,
                    sampleRate,
                    1,
                    AUDIO_MODEM_PCM_LATENCY)) < 0) {
        LOGE("Modem PCM playback params error: %s", snd_strerror(error));
        return INVALID_OPERATION;
    }

    if ((error = snd_pcm_start(cHandle)) < 0) {
        LOGE("Modem PCM capture start error: %d:%s", error, snd_strerror(error));
        return INVALID_OPERATION;
    }
    if ((error = snd_pcm_start(pHandle)) < 0) {
        LOGE("Modem PCM playback start error: %d:%s", error, snd_strerror(error));
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t AudioModemAlsa::voiceCallCodecPCMUpdate()
{
    status_t error;

    if ((error = voiceCallCodecPCMReset()) < 0) {
        LOGE("Modem PCM Update reset error: %d:%s", error, snd_strerror(error));
        return INVALID_OPERATION;
    }
    if ((error = voiceCallCodecPCMSet()) < 0) {
        LOGE("Modem PCM Update set error: %d:%s", error, snd_strerror(error));
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}


status_t AudioModemAlsa::voiceCallCodecPCMReset()
{
    int error;

    if ((error = snd_pcm_drop(cHandle)) < 0) {
        LOGE("Modem PCM capture drop error: %s", snd_strerror(error));
        return INVALID_OPERATION;
    }
    if ((error = snd_pcm_drop(pHandle)) < 0) {
        LOGE("Modem PCM playback drop error: %s", snd_strerror(error));
        return INVALID_OPERATION;
    }

    if ((error = snd_pcm_close(cHandle)) < 0) {
        LOGE("Modem PCM capture close error: %s", snd_strerror(error));
        return INVALID_OPERATION;
    }
    if ((error = snd_pcm_close(pHandle)) < 0) {
        LOGE("Modem PCM playback close error: %s", snd_strerror(error));
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

#ifdef AUDIO_BLUETOOTH
status_t AudioModemAlsa::voiceCallCodecBTPCMSet()
{
    status_t error = NO_ERROR;

    return error;
}

status_t AudioModemAlsa::voiceCallCodecBTPCMReset()
{
    status_t error = NO_ERROR;

    return error;
}

status_t AudioModemAlsa::voiceCallBTDeviceEnable()
{
    status_t error = NO_ERROR;

    return error;
}

status_t AudioModemAlsa::voiceCallBTDeviceDisable()
{
    status_t error = NO_ERROR;

    LOGV("Disable PCM port of Bluetooth SCO device");

    return error;
}
#endif // AUDIO_BLUETOOTH

status_t AudioModemAlsa::voiceCallCodecSetHandset()
{
    status_t error = NO_ERROR;

    // Enable Playback voice path
    CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_HANDSET, -1), error);

    // Enable Capture voice path
    CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Main Mic"), error);
    if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_MULTIMIC].name,
                "Yes")) {
        LOGV("dual mic. enabled");

        // Enable Sub mic
        CHECK_ERROR(mAlsaControl->set("Analog Right Capture Route", "Sub Mic"),
                                        error);
    }
    CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                AUDIO_CODEC_CAPTURE_PREAMP_ATT_HANDSET, -1), error);
    CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                AUDIO_CODEC_CAPTURE_VOL_HANDSET, -1), error);

    CHECK_ERROR(mAlsaControl->set("AMIC_UL PDM Switch", 1), error);
    CHECK_ERROR(mAlsaControl->set("MUX_VX0", "AMic0"), error);
    CHECK_ERROR(mAlsaControl->set("MUX_VX1", "AMic1"), error);
    CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                AUDIO_ABE_AUDUL_VOICE_VOL_HANDSET, -1), error);
    CHECK_ERROR(mAlsaControl->set("Voice Capture Mixer Capture", 1), error);

    // Enable Sidetone
    CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Capture", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT UL Volume",
                                AUDIO_ABE_SIDETONE_UL_VOL_HANDSET, -1), error);

    return error;
}

status_t AudioModemAlsa::voiceCallCodecSetHandfree()
{
    status_t error = NO_ERROR;

    // Enable Playback voice path
    CHECK_ERROR(mAlsaControl->set("DL2 Mixer Voice", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_HANDFREE, -1), error);

    // Enable Capture voice path
    CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Main Mic"), error);
    if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_MULTIMIC].name,
                "Yes")) {
        LOGV("dual mic. enabled");

        // Enable Sub mic
        CHECK_ERROR(mAlsaControl->set("Analog Right Capture Route", "Sub Mic"),
                                        error);
    }
    CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                AUDIO_CODEC_CAPTURE_PREAMP_ATT_HANDFREE, -1), error);
    CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                AUDIO_CODEC_CAPTURE_VOL_HANDFREE, -1), error);

    CHECK_ERROR(mAlsaControl->set("AMIC_UL PDM Switch", 1), error);
    CHECK_ERROR(mAlsaControl->set("MUX_VX0", "AMic0"), error);
    CHECK_ERROR(mAlsaControl->set("MUX_VX1", "AMic1"), error);
    CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                AUDIO_ABE_AUDUL_VOICE_VOL_HANDSET, -1), error);
    CHECK_ERROR(mAlsaControl->set("Voice Capture Mixer Capture", 1), error);

    // Enable Sidetone
    CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Capture", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT UL Volume",
                                AUDIO_ABE_SIDETONE_UL_VOL_HANDFREE, -1), error);

    return error;
}

status_t AudioModemAlsa::voiceCallCodecSetHeadset()
{
    status_t error = NO_ERROR;

    // Enable Playback voice path
    CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_HEADSET, -1), error);

    // Enable Capture voice path
    CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Headset Mic"), error);
    CHECK_ERROR(mAlsaControl->set("Analog Right Capture Route", "Headset Mic"), error);
    CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                AUDIO_CODEC_CAPTURE_PREAMP_ATT_HEADSET, -1), error);
    CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                AUDIO_CODEC_CAPTURE_VOL_HEADSET, -1), error);

    CHECK_ERROR(mAlsaControl->set("AMIC_UL PDM Switch", 1), error);
    CHECK_ERROR(mAlsaControl->set("MUX_VX0", "AMic0"), error);
    CHECK_ERROR(mAlsaControl->set("MUX_VX1", "AMic1"), error);
    CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                AUDIO_ABE_AUDUL_VOICE_VOL_HEADSET, -1), error);
    CHECK_ERROR(mAlsaControl->set("Voice Capture Mixer Capture", 1), error);

    // Enable Sidetone
    CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Capture", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT UL Volume",
                                AUDIO_ABE_SIDETONE_UL_VOL_HEADSET, -1), error);

    return error;
}

#ifdef AUDIO_BLUETOOTH
status_t AudioModemAlsa::voiceCallCodecSetBluetooth()
{
    status_t error = NO_ERROR;

    error = voiceCallCodecStop();

    error = voiceCallCodecBTPCMSet();

    return error;
}
#endif

status_t AudioModemAlsa::voiceCallCodecUpdateHandset()
{
    status_t error = NO_ERROR;

    switch (mPreviousAudioModemModes) {
    case AudioModemInterface::AUDIO_MODEM_HANDFREE:
        // Disable the ABE DL2 mixer used for Voice
        CHECK_ERROR(mAlsaControl->set("DL2 Mixer Voice", 0, 0), error);
        // Enable the ABE DL1 mixer used for Voice
        CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 1), error);
        CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_HANDSET, -1), error);
        // Set mic volume
        CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                AUDIO_ABE_AUDUL_VOICE_VOL_HANDSET, -1), error);
        CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                AUDIO_CODEC_CAPTURE_PREAMP_ATT_HANDSET, -1), error);
        CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                AUDIO_CODEC_CAPTURE_VOL_HANDSET, -1), error);
        // Set Sidetone volume
        CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_HANDSET, -1), error);
        CHECK_ERROR(mAlsaControl->set("SDT UL Volume",
                                AUDIO_ABE_SIDETONE_UL_VOL_HANDSET, -1), error);
        break;

    case AudioModemInterface::AUDIO_MODEM_HEADSET:
        // Enable Capture voice path
        CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Main Mic"), error);
        if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_MULTIMIC].name,
                    "Yes")) {
            LOGV("dual mic. enabled");

            // Enable Sub mic
            CHECK_ERROR(mAlsaControl->set("Analog Right Capture Route", "Sub Mic"),
                                            error);
        }
        // Set mic volume
        CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                AUDIO_ABE_AUDUL_VOICE_VOL_HANDSET, -1), error);
        CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                AUDIO_CODEC_CAPTURE_PREAMP_ATT_HANDSET, -1), error);
        CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                AUDIO_CODEC_CAPTURE_VOL_HANDSET, -1), error);
        // Set Sidetone volume
        CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_HANDSET, -1), error);
        CHECK_ERROR(mAlsaControl->set("SDT UL Volume",
                                AUDIO_ABE_SIDETONE_UL_VOL_HANDSET, -1), error);
        break;

    case AudioModemInterface::AUDIO_MODEM_BLUETOOTH:
#ifdef AUDIO_BLUETOOTH
        // Set Main mic volume
        CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                        AUDIO_CODEC_CAPTURE_PREAMP_ATT_HANDSET), error);
        CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                        AUDIO_CODEC_CAPTURE_VOL_HANDSET), error);
        // Enable Main mic
        CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Main Mic"), error);
        CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", 1, 0), error);

        if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_MULTIMIC].name,
                    "Yes")) {
            LOGV("dual mic. enabled");

            // Enable Sub mic
            CHECK_ERROR(mAlsaControl->set("Analog Right Capture Route", "Sub Mic"), error);
        }
        break;
#endif
    case AudioModemInterface::AUDIO_MODEM_HANDSET:
    default:
        LOGE("%s: Wrong mPreviousAudioModemModes: %04x",
                __FUNCTION__,
                mPreviousAudioModemModes);
        error = INVALID_OPERATION;
        break;

    } //switch (mPreviousAudioModemModes)

    return error;
}

status_t AudioModemAlsa::voiceCallCodecUpdateHandfree()
{
    status_t error = NO_ERROR;

    switch (mPreviousAudioModemModes) {
    case AudioModemInterface::AUDIO_MODEM_HANDSET:
    case AudioModemInterface::AUDIO_MODEM_HEADSET:
        // Disable the ABE DL1 mixer used for Voice
        CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 0, 0), error);
        // Enable the ABE DL2 mixer used for Voice
        CHECK_ERROR(mAlsaControl->set("DL2 Mixer Voice", 1), error);
        CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_HANDFREE, -1), error);
        // Enable Capture voice path
        CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Main Mic"), error);
        if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_MULTIMIC].name,
                    "Yes")) {
            LOGV("dual mic. enabled");

            // Enable Sub mic
            CHECK_ERROR(mAlsaControl->set("Analog Right Capture Route", "Sub Mic"),
                                            error);
        }
        CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                    AUDIO_CODEC_CAPTURE_PREAMP_ATT_HANDFREE, -1), error);
        CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                    AUDIO_CODEC_CAPTURE_VOL_HANDFREE, -1), error);

        CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                    AUDIO_ABE_AUDUL_VOICE_VOL_HANDSET, -1), error);
        // Enable Sidetone
        CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Capture", 1), error);
        CHECK_ERROR(mAlsaControl->set("SDT UL Volume",
                                    AUDIO_ABE_SIDETONE_UL_VOL_HANDFREE, -1), error);
        break;

    case AudioModemInterface::AUDIO_MODEM_BLUETOOTH:
#ifdef AUDIO_BLUETOOTH
        // Set Main mic volume
        CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                        AUDIO_CODEC_CAPTURE_PREAMP_ATT_HANDFREE), error);
        CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                        AUDIO_CODEC_CAPTURE_VOL_HANDFREE), error);
        // Enable Main mic
        CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Main Mic"), error);
        CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", 1, 0), error);

        if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_MULTIMIC].name,
                    "Yes")) {
            LOGV("dual mic. enabled");

            // Enable Sub mic
            CHECK_ERROR(mAlsaControl->set("Analog Right Capture Route", "Sub Mic"), error);
        }
        break;
#endif
    case AudioModemInterface::AUDIO_MODEM_HANDFREE:
    default:
        LOGE("%s: Wrong mPreviousAudioModemModes: %04x",
                __FUNCTION__,
                mPreviousAudioModemModes);
        error = INVALID_OPERATION;
        break;

    } //switch (mPreviousAudioModemModes)

    return error;
}

status_t AudioModemAlsa::voiceCallCodecUpdateHeadset()
{
    status_t error = NO_ERROR;

    switch (mPreviousAudioModemModes) {
    case AudioModemInterface::AUDIO_MODEM_HANDFREE:
    case AudioModemInterface::AUDIO_MODEM_HANDSET:
        // Disable the ABE DL2 mixer used for Voice
        CHECK_ERROR(mAlsaControl->set("DL2 Mixer Voice", 0, 0), error);
        // Enable the ABE DL1 mixer used for Voice
        CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 1), error);
        CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                    AUDIO_ABE_SIDETONE_DL_VOL_HEADSET, -1), error);
        // Enable Capture voice path
        CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Headset Mic"), error);
        CHECK_ERROR(mAlsaControl->set("Analog Right Capture Route", "Headset Mic"), error);
        CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                    AUDIO_CODEC_CAPTURE_PREAMP_ATT_HEADSET, -1), error);
        CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                    AUDIO_CODEC_CAPTURE_VOL_HEADSET, -1), error);
        CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                    AUDIO_ABE_AUDUL_VOICE_VOL_HEADSET, -1), error);
        // Enable Sidetone
        CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Capture", 1), error);
        CHECK_ERROR(mAlsaControl->set("SDT UL Volume",
                                    AUDIO_ABE_SIDETONE_UL_VOL_HEADSET, -1), error);
        break;

    case AudioModemInterface::AUDIO_MODEM_BLUETOOTH:
#ifdef AUDIO_BLUETOOTH
        // Set Main mic volume
        CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume",
                                        AUDIO_CODEC_CAPTURE_PREAMP_ATT_HEADSET), error);
        CHECK_ERROR(mAlsaControl->set("Capture Volume",
                                        AUDIO_CODEC_CAPTURE_VOL_HEADSET), error);
        // Enable Main mic
        CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Main Mic"), error);
        CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", 1, 0), error);

        break;
#endif
    case AudioModemInterface::AUDIO_MODEM_HEADSET:
    default:
        LOGE("%s: Wrong mPreviousAudioModemModes: %04x",
                __FUNCTION__,
                mPreviousAudioModemModes);
        error = INVALID_OPERATION;
        break;

    } //switch (mPreviousAudioModemModes)

    return error;
}

#ifdef AUDIO_BLUETOOTH
status_t AudioModemAlsa::voiceCallCodecUpdateBluetooth()
{
    status_t error = NO_ERROR;

    error = voiceCallCodecStop();

    error = voiceCallCodecBTPCMSet();

    return error;
}
#endif // AUDIO_BLUETOOTH

status_t AudioModemAlsa::voiceCallCodecUpdate()
{
    status_t error = NO_ERROR;

    LOGV("Update Audio Codec Voice call: %04x", mCurrentAudioModemModes);

    if (mCurrentAudioModemModes & AudioModemInterface::AUDIO_MODEM_HANDSET) {
        error = voiceCallCodecUpdateHandset();
    } else if (mCurrentAudioModemModes & AudioModemInterface::AUDIO_MODEM_HANDFREE) {
        error = voiceCallCodecUpdateHandfree();
    } else if (mCurrentAudioModemModes & AudioModemInterface::AUDIO_MODEM_HEADSET) {
        error = voiceCallCodecUpdateHeadset();
#ifdef AUDIO_BLUETOOTH
    } else if (mCurrentAudioModemModes & AudioModemInterface::AUDIO_MODEM_BLUETOOTH) {
        error = voiceCallCodecUpdateBluetooth();
#endif
    } else {
        LOGE("Audio Modem mode not supported: %04x", mCurrentAudioModemModes);
        return INVALID_OPERATION;
    }

    error = voiceCallCodecPCMUpdate();

    return error;
}

status_t AudioModemAlsa::voiceCallCodecReset()
{
    status_t error = NO_ERROR;

    LOGV("Stop Audio Codec Voice call");

    error = voiceCallCodecPCMReset();
#ifdef AUDIO_BLUETOOTH
    error = voiceCallCodecBTPCMReset();
#endif

    error = voiceCallCodecStop();

    return error;
}

status_t AudioModemAlsa::voiceCallCodecStop()
{
    status_t error = NO_ERROR;

    // Enable Playback voice path
    CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 0, 0), error);
    CHECK_ERROR(mAlsaControl->set("DL2 Mixer Voice", 0, 0), error);
    CHECK_ERROR(mAlsaControl->set("SDT DL Volume", 0, -1), error);

    // Enable Capture voice path
    CHECK_ERROR(mAlsaControl->set("Analog Left Capture Route", "Off"), error);
    CHECK_ERROR(mAlsaControl->set("Analog Right Capture Route", "Off"), error);
    CHECK_ERROR(mAlsaControl->set("Capture Preamplifier Volume", 0, -1), error);
    CHECK_ERROR(mAlsaControl->set("Capture Volume", 0, -1), error);

    CHECK_ERROR(mAlsaControl->set("AMIC_UL PDM Switch", 0, 0), error);
    CHECK_ERROR(mAlsaControl->set("MUX_VX0", "None"), error);
    CHECK_ERROR(mAlsaControl->set("MUX_VX1", "None"), error);
    CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume", 0, -1), error);
    CHECK_ERROR(mAlsaControl->set("Voice Capture Mixer Capture", 0, 0), error);

    // Enable Sidetone
    CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Capture", 0, 0), error);
    CHECK_ERROR(mAlsaControl->set("SDT UL Volume", 0, -1), error);

    return error;
}

status_t AudioModemAlsa::voiceCallModemSet()
{
    status_t error = NO_ERROR;

    LOGV("Start Audio Modem Voice call");

    if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_SAMPLERATE].name,
                "16Khz")) {
        error = mModem->setModemRouting(mCurrentAudioModemModes,
               AudioModemInterface::PCM_16_KHZ);
    } else {
        error = mModem->setModemRouting(mCurrentAudioModemModes,
               AudioModemInterface::PCM_8_KHZ);
    }
    if (error < 0) {
        LOGE("Unable to set Modem Voice Call routing: %s", strerror(error));
        return error;
    }

    if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_MULTIMIC].name,
                "Yes")) {
        error =  mModem->setModemVoiceCallMultiMic(
                    AudioModemInterface::MODEM_DOUBLE_MIC);

    } else {
        error =  mModem->setModemVoiceCallMultiMic(
                    AudioModemInterface::MODEM_SINGLE_MIC);
    }
    if (error < 0) {
        LOGE("Unable to set Modem Voice Call multimic.: %s", strerror(error));
        return error;
    }

    error =  mModem->OpenModemVoiceCallStream();
    if (error < 0) {
        LOGE("Unable to open Modem Voice Call stream: %s", strerror(error));
        return error;
    }

    return error;
}

status_t AudioModemAlsa::voiceCallModemUpdate()
{
    status_t error = NO_ERROR;

    LOGV("Update Audio Modem Voice call");

    if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_SAMPLERATE].name,
                "16Khz")) {
        error = mModem->setModemRouting(mCurrentAudioModemModes,
               AudioModemInterface::PCM_16_KHZ);
    } else {
        error = mModem->setModemRouting(mCurrentAudioModemModes,
               AudioModemInterface::PCM_8_KHZ);
    }
    if (error < 0) {
        LOGE("Unable to set Modem Voice Call routing: %s", strerror(error));
        return error;
    }

    if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_MULTIMIC].name,
                "Yes")) {
        error =  mModem->setModemVoiceCallMultiMic(
                    AudioModemInterface::MODEM_DOUBLE_MIC);

    } else {
        error =  mModem->setModemVoiceCallMultiMic(
                    AudioModemInterface::MODEM_SINGLE_MIC);
    }
    if (error < 0) {
        LOGE("Unable to set Modem Voice Call multimic.: %s", strerror(error));
        return error;
    }

    return error;
}

status_t AudioModemAlsa::voiceCallModemReset()
{
    status_t error = NO_ERROR;

    LOGV("Stop Audio Modem Voice call");

    error = mModem->CloseModemVoiceCallStream();
    if (error < 0) {
        LOGE("Unable to close Modem Voice Call stream: %s", strerror(error));
        return error;
    }

    return error;
}

} // namespace android

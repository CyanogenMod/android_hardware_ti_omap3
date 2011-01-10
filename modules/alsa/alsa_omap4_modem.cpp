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
#include <pthread.h>

#include "AudioHardwareALSA.h"
#include "audio_modem_interface.h"
#include "alsa_omap4_modem.h"

// TODO
// separate left/right volume for main/sub mic

namespace android
{
// ----------------------------------------------------------------------------
// Voice call volume
#define VOICE_CALL_VOLUME_PROP(dev, name) \
    {\
        dev, name, NULL\
    }

static voiceCallVolumeList
voiceCallVolumeProp[] = {
    VOICE_CALL_VOLUME_PROP(AudioModemInterface::AUDIO_MODEM_HANDSET,
                            "DL1 Voice Playback Volume"),
    VOICE_CALL_VOLUME_PROP(AudioModemInterface::AUDIO_MODEM_HANDFREE,
                            "DL2 Voice Playback Volume"),
    VOICE_CALL_VOLUME_PROP(AudioModemInterface::AUDIO_MODEM_HEADSET,
                            "DL1 Voice Playback Volume"),
    VOICE_CALL_VOLUME_PROP(AudioModemInterface::AUDIO_MODEM_AUX,
                            "DL1 Voice Playback Volume"),
    VOICE_CALL_VOLUME_PROP(AudioModemInterface::AUDIO_MODEM_BLUETOOTH,
                            "DL1 Voice Playback Volume"),
    VOICE_CALL_VOLUME_PROP(0, "")
};

#define WORKAROUND_AVOID_VOICE_VOLUME_MAX   1
#define WORKAROUND_MAX_VOICE_VOLUME         120
#define WORKAROUND_AVOID_VOICE_VOLUME_MIN   1
#define WORKAROUND_MIN_VOICE_VOLUME         90

// Pointer to Thread local info
voiceCallControlLocalInfo    *mInfo;
// ----------------------------------------------------------------------------
#define CHECK_ERROR(func, error)   if ((error = func) != NO_ERROR) { \
                                return error; \
                            }
#define mAlsaControl mInfo->mAlsaControl
// ----------------------------------------------------------------------------
extern "C"
{
    void *VoiceCallControlsThreadStartup(void *_mAudioModemAlsa) {
        LOGV("%s",__FUNCTION__);
        AudioModemAlsa *mAudioModemAlsa = (AudioModemAlsa * )_mAudioModemAlsa;
        mAudioModemAlsa->voiceCallControlsThread();
        delete mAudioModemAlsa;
        return (NULL);
    }

    static pthread_key_t   mVoiceCallControlThreadKey;
    void voiceCallControlInitThreadOnce(void)
    {
        status_t error = NO_ERROR;
        LOGV("%s",__FUNCTION__);
        error = pthread_key_create(&mVoiceCallControlThreadKey,
                                    NULL);
        if (error != NO_ERROR) {
            LOGE("Can't create the Voice Call Control Thread key");
            exit(error);
        }
    }
}
// ----------------------------------------------------------------------------
AudioModemAlsa::AudioModemAlsa()
{
    status_t error;
    pthread_attr_t  mVoiceCallControlAttr;

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

    // Initialize Min and Max volume
    int i = 0;
    while(voiceCallVolumeProp[i].device) {
        voiceCallVolumeInfo *info = voiceCallVolumeProp[i].mInfo = new voiceCallVolumeInfo;

#if WORKAROUND_AVOID_VOICE_VOLUME_MAX
        LOGV("Workaround: Voice call max volume name %s limited to: %d",
                voiceCallVolumeProp[i].volumeName, WORKAROUND_MAX_VOICE_VOLUME);
        info->max = WORKAROUND_MAX_VOICE_VOLUME;
#else
        error = alsaControl->getmax(voiceCallVolumeProp[i].volumeName, info->max);
#endif
#if WORKAROUND_AVOID_VOICE_VOLUME_MIN
        LOGV("Workaround: Voice call min volume name %s limited to: %d",
                voiceCallVolumeProp[i].volumeName, WORKAROUND_MIN_VOICE_VOLUME);
        info->min = WORKAROUND_MIN_VOICE_VOLUME;
#else
        error = alsaControl->getmin(voiceCallVolumeProp[i].volumeName, info->min);
#endif
        LOGV("Voice call volume name: %s min: %d max: %d", voiceCallVolumeProp[i].volumeName,
                 info->min, info->max);

        if (error != NO_ERROR) {
            LOGE("Audio Voice Call volume was not correctly initialized.");
            delete mModem;
            exit(error);
        }
        i++;
    }

    // Initialize voice call control key once
    mVoiceCallControlKeyOnce = PTHREAD_ONCE_INIT;

    // Initialize voice call control mutex
    pthread_mutex_init(&mVoiceCallControlMutex, NULL);

    // Initialize voice call control param condition
    // This is used to wake up Voice call control thread when
    // new param is available
    pthread_cond_init(&mVoiceCallControlNewParams, NULL);

    // Initialize and set thread detached attribute
    pthread_attr_init(&mVoiceCallControlAttr);
    pthread_attr_setdetachstate(&mVoiceCallControlAttr, PTHREAD_CREATE_JOINABLE);

    pthread_mutex_lock(&mVoiceCallControlMutex);
    mVoiceCallControlMainInfo.updateFlag = false;
    pthread_mutex_unlock(&mVoiceCallControlMutex);

    // Create thread for voice control
    error = pthread_create(&mVoiceCallControlThread, &mVoiceCallControlAttr,
                            VoiceCallControlsThreadStartup, (void *)this);
    if (error != NO_ERROR) {
        LOGE("Error creating mVoiceCallControlThread");
        delete mModem;
        exit(error);
    }
    pthread_attr_destroy(&mVoiceCallControlAttr);
}

AudioModemAlsa::~AudioModemAlsa()
{
    LOGD("Destroy devices for Modem OMAP4 ALSA module");
    mDevicePropList.clear();
    if (mModem) delete mModem;

    pthread_mutex_destroy(&mVoiceCallControlMutex);
    pthread_kill(mVoiceCallControlThread, SIGKILL);
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

status_t AudioModemAlsa::voiceCallControls(uint32_t devices, int mode, bool multimediaUpdate)
{
    LOGV("%s: devices %04x mode %d MultimediaUpdate %d", __FUNCTION__, devices, mode, multimediaUpdate);

    if ((mVoiceCallControlMainInfo.devices != devices) ||
        (mVoiceCallControlMainInfo.mode != mode) ||
        (multimediaUpdate)) {
        pthread_mutex_lock(&mVoiceCallControlMutex);
        mVoiceCallControlMainInfo.devices = devices;
        mVoiceCallControlMainInfo.mode = mode;
        mVoiceCallControlMainInfo.updateFlag = true;
        mVoiceCallControlMainInfo.multimediaUpdate = multimediaUpdate;
        pthread_mutex_unlock(&mVoiceCallControlMutex);
        pthread_cond_signal(&mVoiceCallControlNewParams);
    }
    return NO_ERROR;
}

void AudioModemAlsa::voiceCallControlsThread(void)
{
    status_t error = NO_ERROR;
    ALSAControl alsaControl("hw:00");

    error = pthread_once(&mVoiceCallControlKeyOnce,
                           voiceCallControlInitThreadOnce);
    if (error != NO_ERROR) {
        LOGE("Error in voiceCallControlInitThreadOnce");
        goto exit;
    }

    mInfo = (voiceCallControlLocalInfo *)malloc(sizeof(voiceCallControlLocalInfo));
    if (mInfo == NULL) {
        LOGE("Error allocating mVoiceCallControlThreadInfo");
        error = NO_MEMORY;
        goto exit;
    }

    error = pthread_setspecific(mVoiceCallControlThreadKey, mInfo);
    if (error != NO_ERROR) {
        LOGE("Error in Voice Call info set specific");
        goto exit;
    }

    mInfo->devices = 0;
    mInfo->mode = AudioSystem::MODE_INVALID;
    mAlsaControl = &alsaControl;

    for (;;) {
        pthread_mutex_lock(&mVoiceCallControlMutex);
        // Wait for new parameters
        if (!mVoiceCallControlMainInfo.updateFlag) {
            pthread_cond_wait(&mVoiceCallControlNewParams, &mVoiceCallControlMutex);
        }
        mVoiceCallControlMainInfo.updateFlag = false;
        mInfo = (voiceCallControlLocalInfo *)pthread_getspecific(mVoiceCallControlThreadKey);
        if (mInfo == NULL) {
            LOGE("Error in Voice Call info get specific");
            goto exit;
        }
        mInfo->devices = mVoiceCallControlMainInfo.devices;
        mInfo->mode = mVoiceCallControlMainInfo.mode;
        mInfo->multimediaUpdate = mVoiceCallControlMainInfo.multimediaUpdate;
        pthread_mutex_unlock(&mVoiceCallControlMutex);
        LOGV("%s: devices %04x mode %d forceUpdate %d", __FUNCTION__, mInfo->devices, mInfo->mode,
                                                                                                mInfo->multimediaUpdate);

        // Check mics config
        micChosen();

        if ((mInfo->mode == AudioSystem::MODE_IN_CALL) &&
            (mVoiceCallState == AUDIO_MODEM_VOICE_CALL_OFF)) {
            // enter in voice call mode
            error = setCurrentAudioModemModes(mInfo->devices);
            if (error < 0) goto exit;
            mDeviceProp = mDevicePropList.valueFor(mCurrentAudioModemModes);
            error = voiceCallModemSet();
            if (error < 0) goto exit;
            error = voiceCallCodecSet();
            if (error < 0) goto exit;
            mVoiceCallState = AUDIO_MODEM_VOICE_CALL_ON;
        } else if ((mInfo->mode == AudioSystem::MODE_IN_CALL) &&
                (mVoiceCallState == AUDIO_MODEM_VOICE_CALL_ON)) {
            // update in voice call mode
            mPreviousAudioModemModes = mCurrentAudioModemModes;
            error = setCurrentAudioModemModes(mInfo->devices);
            if (error < 0) {
                mCurrentAudioModemModes = mPreviousAudioModemModes;
                goto exit;
            }
            mDevicePropPrevious = mDeviceProp;
            mDeviceProp = mDevicePropList.valueFor(mCurrentAudioModemModes);
            if (mCurrentAudioModemModes != mPreviousAudioModemModes) {
                error = voiceCallModemUpdate();
                if (error < 0) goto exit;
                error = voiceCallCodecUpdate();
                if (error < 0) goto exit;
            } else if (mInfo->multimediaUpdate) {
                error = multimediaCodecUpdate();
                if (error < 0) goto exit;
            } else {
                LOGI("Audio Modem Mode doesn't changed: no update needed");
            }
        } else if ((mInfo->mode != AudioSystem::MODE_IN_CALL) &&
                (mVoiceCallState == AUDIO_MODEM_VOICE_CALL_ON)) {
            // we just exit voice call mode
            mPreviousAudioModemModes = mCurrentAudioModemModes;
            mDevicePropPrevious = mDeviceProp;
            error = voiceCallModemReset();
            if (error < 0) goto exit;
            error = voiceCallCodecReset();
            if (error < 0) goto exit;
            mVoiceCallState = AUDIO_MODEM_VOICE_CALL_OFF;
        }
    } // for(;;)

exit:
    LOGE("%s: exit with error %d (%s)", __FUNCTION__, error, strerror(error));
    pthread_exit((void*) error);
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
    unsigned int channels;

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

    if (!strcmp(mDeviceProp->settingsList[AUDIO_MODEM_VOICE_CALL_MULTIMIC].name,
                "Yes")) {
        LOGV("dual mic. enabled");
            // Enable Sub mic
        channels = 2;
    } else {
        channels = 1;
    }

    if ((error = snd_pcm_set_params(cHandle,
                    SND_PCM_FORMAT_S16_LE,
                    SND_PCM_ACCESS_RW_INTERLEAVED,
                    channels,
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

status_t AudioModemAlsa::voiceCallCodecSetHandset()
{
    status_t error = NO_ERROR;

    // Enable Playback voice path
    CHECK_ERROR(mAlsaControl->set("Earphone Driver Switch", 1), error);
    CHECK_ERROR(mAlsaControl->set("Earphone Playback Volume", AUDIO_CODEC_EARPIECE_GAIN), error);
    CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 1), error);
    CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Playback", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_HANDSET, -1), error);

    // Enable Capture voice path
    if ((mainMic.type == OMAP_MIC_TYPE_ANALOG) ||
        (subMic.type == OMAP_MIC_TYPE_ANALOG)) {
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
    }
    CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                AUDIO_ABE_AUDUL_VOICE_VOL_HANDSET, -1), error);
    CHECK_ERROR(configMicrophones(), error);
    CHECK_ERROR(configEqualizers(), error);

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
    CHECK_ERROR(mAlsaControl->set("HF Left Playback", "HF DAC"), error);
    CHECK_ERROR(mAlsaControl->set("HF Right Playback", "HF DAC"), error);
    CHECK_ERROR(mAlsaControl->set("Handsfree Playback Volume", AUDIO_CODEC_HANDFREE_GAIN), error);
    CHECK_ERROR(mAlsaControl->set("DL2 Mixer Voice", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_HANDFREE, -1), error);

    // Enable Capture voice path
    if ((mainMic.type == OMAP_MIC_TYPE_ANALOG) ||
        (subMic.type == OMAP_MIC_TYPE_ANALOG)) {
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
    }
    CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                AUDIO_ABE_AUDUL_VOICE_VOL_HANDFREE, -1), error);
    CHECK_ERROR(configMicrophones(), error);
    CHECK_ERROR(configEqualizers(), error);

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
    CHECK_ERROR(mAlsaControl->set("HS Left Playback", "HS DAC"), error);
    CHECK_ERROR(mAlsaControl->set("HS Right Playback", "HS DAC"), error);
    CHECK_ERROR(mAlsaControl->set("Headset Playback Volume", AUDIO_CODEC_HEADSET_GAIN), error);
    CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 1), error);
    CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Playback", 1), error);
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
    CHECK_ERROR(configMicrophones(), error);
    CHECK_ERROR(configEqualizers(), error);

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

    // Enable Playback voice path
    CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 1), error);
    CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Playback", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT DL Volume",
                                AUDIO_ABE_SIDETONE_DL_VOL_BLUETOOTH, -1), error);

    // Enable Capture voice path
    CHECK_ERROR(mAlsaControl->set("AUDUL Voice UL Volume",
                                AUDIO_ABE_AUDUL_VOICE_VOL_BLUETOOTH, -1), error);
    CHECK_ERROR(configMicrophones(), error);
    CHECK_ERROR(configEqualizers(), error);

    // Enable Sidetone
    CHECK_ERROR(mAlsaControl->set("Sidetone Mixer Capture", 1), error);
    CHECK_ERROR(mAlsaControl->set("SDT UL Volume",
                                AUDIO_ABE_SIDETONE_UL_VOL_BLUETOOTH, -1), error);

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
        CHECK_ERROR(voiceCallCodecSetHandset(), error);
        break;

    case AudioModemInterface::AUDIO_MODEM_HEADSET:
    case AudioModemInterface::AUDIO_MODEM_BLUETOOTH:
        CHECK_ERROR(voiceCallCodecSetHandset(), error);
        break;

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
    case AudioModemInterface::AUDIO_MODEM_BLUETOOTH:
        // Disable the ABE DL1 mixer used for Voice
        CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 0, 0), error);
        CHECK_ERROR(voiceCallCodecSetHandfree(), error);
        break;

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
        // Disable the ABE DL2 mixer used for Voice
        CHECK_ERROR(mAlsaControl->set("DL2 Mixer Voice", 0, 0), error);
        CHECK_ERROR(voiceCallCodecSetHeadset(), error);
        break;

    case AudioModemInterface::AUDIO_MODEM_HANDSET:
    case AudioModemInterface::AUDIO_MODEM_BLUETOOTH:
        CHECK_ERROR(voiceCallCodecSetHeadset(), error);
        break;

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

    switch (mPreviousAudioModemModes) {
    case AudioModemInterface::AUDIO_MODEM_HANDFREE:
        // Disable the ABE DL2 mixer used for Voice
        CHECK_ERROR(mAlsaControl->set("DL2 Mixer Voice", 0, 0), error);
        CHECK_ERROR(voiceCallCodecSetBluetooth(), error);
    case AudioModemInterface::AUDIO_MODEM_HANDSET:
    case AudioModemInterface::AUDIO_MODEM_HEADSET:
        CHECK_ERROR(voiceCallCodecSetBluetooth(), error);
        break;

    case AudioModemInterface::AUDIO_MODEM_BLUETOOTH:
    default:
        LOGE("%s: Wrong mPreviousAudioModemModes: %04x",
                __FUNCTION__,
                mPreviousAudioModemModes);
        error = INVALID_OPERATION;
        break;

    } //switch (mPreviousAudioModemModes)

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

status_t AudioModemAlsa::multimediaCodecUpdate()
{
    status_t error = NO_ERROR;

    configEqualizers();

    return error;
}

status_t AudioModemAlsa::voiceCallCodecReset()
{
    status_t error = NO_ERROR;

    LOGV("Stop Audio Codec Voice call");

    error = voiceCallCodecPCMReset();

    error = voiceCallCodecStop();

    return error;
}

status_t AudioModemAlsa::voiceCallCodecStop()
{
    status_t error = NO_ERROR;

    // Enable Playback voice path
    CHECK_ERROR(mAlsaControl->set("DL1 Mixer Voice", 0, 0), error);
    CHECK_ERROR(mAlsaControl->set("DL2 Mixer Voice", 0, 0), error);

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

status_t AudioModemAlsa::voiceCallVolume(ALSAControl *alsaControl, float volume)
{
    status_t error = NO_ERROR;
    unsigned int setVolume;
    int i = 0;

    while(voiceCallVolumeProp[i].device) {
        voiceCallVolumeInfo *info = voiceCallVolumeProp[i].mInfo;

        // Make sure volume is between bounds.
        setVolume = info->min + volume * (info->max - info->min);
        if (setVolume > info->max) setVolume = info->max;
        if (setVolume < info->min) setVolume = info->min;

        LOGV("%s: in call volume level to apply: %d", voiceCallVolumeProp[i].volumeName,
                setVolume);

        error = alsaControl->set(voiceCallVolumeProp[i].volumeName, setVolume, 0);
        if (error < 0) {
            LOGE("%s: error applying in call volume: %d", voiceCallVolumeProp[i].volumeName,
                    setVolume);
            return error;
        }
        i++;
    }
    return NO_ERROR;
}

int AudioModemAlsa::setMicType(MicConfig *mic)
{
    char notFound = 1;
    int index = 0;

    while (notFound && strcmp(MicNameList[index], "eof")) {
        notFound = strcmp(MicNameList[index], mic->name);
        if (notFound)
            index++;
    }
    if (index < AMIC_MAX_INDEX) {
        mic->type = OMAP_MIC_TYPE_ANALOG;
    } else if (index < DMIC_MAX_INDEX) {
        mic->type = OMAP_MIC_TYPE_DIGITAL;
    } else {
       mic->type = OMAP_MIC_TYPE_UNKNOWN;
    }

    return index;
}

void AudioModemAlsa::micChosen(void)
{
    property_get("omap.audio.mic.main", mainMic.name, "AMic0");
    property_get("omap.audio.mic.sub", subMic.name, "AMic1");

    int mainMicIndex = setMicType(&mainMic);
    int subMicIndex = setMicType(&subMic);

    if (mainMic.type == OMAP_MIC_TYPE_UNKNOWN) {
        LOGE("Main Mic type is unknown: switch to AMic0");
        mainMic.type = OMAP_MIC_TYPE_ANALOG;
        strcpy(mainMic.name, "AMic0");
    }
    if (subMic.type == OMAP_MIC_TYPE_UNKNOWN) {
        LOGE("Sub Mic type is unknown: switch to AMic1");
        subMic.type = OMAP_MIC_TYPE_ANALOG;
        strcpy(subMic.name, "AMic1");
    }
    if (mainMicIndex == subMicIndex) {
        LOGE("Same Mic is used for main and sub...");
        if (mainMic.type == OMAP_MIC_TYPE_ANALOG) {
            LOGE("...switch respectively to AMic0 and AMic1");
            strcpy(mainMic.name, "AMic0");
            strcpy(subMic.name, "AMic1");
        }
        if (mainMic.type == OMAP_MIC_TYPE_DIGITAL) {
            LOGE("...switch respectively to DMic0L and DMic0R");
            strcpy(mainMic.name, "DMic0L");
            strcpy(subMic.name, "DMic0R");
        }
    }
    if (mainMic.type != subMic.type) {
        LOGE("Different Mic type is used for main and sub...");
        if (mainMic.type == OMAP_MIC_TYPE_ANALOG) {
            LOGE("...switch respectively to AMic0 and AMic1");
            strcpy(mainMic.name, "AMic0");
            strcpy(subMic.name, "AMic1");
            subMic.type = OMAP_MIC_TYPE_ANALOG;
        }
        if (mainMic.type == OMAP_MIC_TYPE_DIGITAL) {
            LOGE("...switch respectively to DMic0L and DMic0R");
            strcpy(mainMic.name, "DMic0L");
            strcpy(subMic.name, "DMic0R");
            subMic.type = OMAP_MIC_TYPE_DIGITAL;
        }
    }
}

status_t AudioModemAlsa::configMicrophones(void)
{
    ALSAControl control("hw:00");

    status_t error = NO_ERROR;

   switch (mCurrentAudioModemModes) {
    case AudioModemInterface::AUDIO_MODEM_HANDFREE:
    case AudioModemInterface::AUDIO_MODEM_HANDSET:
        if ((mainMic.type == OMAP_MIC_TYPE_DIGITAL) &&
            (subMic.type == OMAP_MIC_TYPE_DIGITAL)) {
            CHECK_ERROR(control.set("AMIC_UL PDM Switch", 0, 0), error);
        } else {
            CHECK_ERROR(control.set("AMIC_UL PDM Switch", 1), error);
        }

        CHECK_ERROR(control.set("MUX_VX0", mainMic.name), error);
        CHECK_ERROR(control.set("MUX_VX1", subMic.name), error);
        break;

    case AudioModemInterface::AUDIO_MODEM_HEADSET:
        // In headset the microphone allways comes from analog
        CHECK_ERROR(control.set("AMIC_UL PDM Switch", 1), error);
        CHECK_ERROR(mAlsaControl->set("MUX_VX0", "AMic0"), error);
        CHECK_ERROR(mAlsaControl->set("MUX_VX1", "AMic1"), error);
        break;

    case AudioModemInterface::AUDIO_MODEM_BLUETOOTH:
        CHECK_ERROR(control.set("AMIC_UL PDM Switch", 0, 0), error);
        CHECK_ERROR(mAlsaControl->set("MUX_VX0", "BT Left"), error);
        CHECK_ERROR(mAlsaControl->set("MUX_VX1", "BT Right"), error);
        break;

    default:
        LOGE("%s: Wrong mCurrentAudioModemModes: %04x",
                __FUNCTION__,
                mCurrentAudioModemModes);
        error = INVALID_OPERATION;
        break;
    } //switch (modem_mode)

    //always enable vx mixer
    CHECK_ERROR(mAlsaControl->set("Voice Capture Mixer Capture", 1), error);

    return error;
}

status_t AudioModemAlsa::configEqualizers(void)
{
    ALSAControl control("hw:00");

    status_t error = NO_ERROR;

    // All equalizers need to be set at flat response
    // as equalizers are done by the modem
    CHECK_ERROR(control.set("AMIC Equalizer", "High-pass 0dB"), error);
    CHECK_ERROR(control.set("DMIC Equalizer", "High-pass 0dB"), error);

    CHECK_ERROR(control.set("DL1 Equalizer", "Flat response"), error);
    CHECK_ERROR(control.set("DL2 Left Equalizer", "Flat response"), error);
    CHECK_ERROR(control.set("DL2 Right Equalizer", "Flat response"), error);
    CHECK_ERROR(control.set("Sidetone Equalizer", "Flat response"), error);

    return error;
}
} // namespace android

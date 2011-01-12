/* alsa_omap4.cpp
 **
 ** Copyright 2009 Texas Instruments
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

#define LOG_TAG "Omap4ALSA"
#include <utils/Log.h>
#include <cutils/properties.h>

#include "AudioHardwareALSA.h"
#include <media/AudioRecord.h>

#if 0

#ifdef AUDIO_MODEM_TI
#include "audio_modem_interface.h"
#include "alsa_omap4_modem.h"
#endif
#endif

#define MM_DEFAULT_DEVICE    "plughw:0,0"
#define BLUETOOTH_SCO_DEVICE "plughw:0,0"
#define FM_TRANSMIT_DEVICE   "plughw:0,0"
#define FM_CAPTURE_DEVICE    "plughw:0,1"
#define MM_LP_DEVICE         "hw:0,6"
#define HDMI_DEVICE          "plughw:0,7"

#ifndef ALSA_DEFAULT_SAMPLE_RATE
#define ALSA_DEFAULT_SAMPLE_RATE 48000 // in Hz
#endif

#ifndef MM_LP_SAMPLE_RATE
//not used for now
#define MM_LP_SAMPLE_RATE 44100        // in Hz
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
static bool fm_enable = false;
static bool mActive = false;

namespace android
{

static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);
static status_t s_init(alsa_device_t *, ALSAHandleList &);
static status_t s_open(alsa_handle_t *, uint32_t, int);
static status_t s_close(alsa_handle_t *);
static status_t s_standby(alsa_handle_t *);
static status_t s_route(alsa_handle_t *, uint32_t, int);
static status_t s_voicevolume(float);

#ifdef AUDIO_MODEM_TI
    AudioModemAlsa *audioModem;
#endif

static hw_module_methods_t s_module_methods = {
    open            : s_device_open
};

extern "C" const hw_module_t HAL_MODULE_INFO_SYM = {
    tag             : HARDWARE_MODULE_TAG,
    version_major   : 1,
    version_minor   : 0,
    id              : ALSA_HARDWARE_MODULE_ID,
    name            : "Omap4 ALSA module",
    author          : "Texas Instruments",
    methods         : &s_module_methods,
    dso             : 0,
    reserved        : {0,},
};

static int s_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    alsa_device_t *dev;
    dev = (alsa_device_t *) malloc(sizeof(*dev));
    if (!dev) return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (hw_module_t *) module;
    dev->common.close = s_device_close;
    dev->init = s_init;
    dev->open = s_open;
    dev->close = s_close;
    dev->standby = s_standby;
    dev->route = s_route;
    dev->voicevolume = s_voicevolume;

    *device = &dev->common;

    LOGD("OMAP4 ALSA module opened");

    return 0;
}

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

// ----------------------------------------------------------------------------

static const int DEFAULT_SAMPLE_RATE = ALSA_DEFAULT_SAMPLE_RATE;

static void setDefaultControls(uint32_t devices, int mode);
void configMicChoices(uint32_t);

typedef void (*AlsaControlSet)(uint32_t devices, int mode);

#define OMAP4_OUT_SCO      (\
        AudioSystem::DEVICE_OUT_BLUETOOTH_SCO |\
        AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET |\
        AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)

#define OMAP4_OUT_FM        (\
        AudioSystem::DEVICE_OUT_FM_TRANSMIT)

#define OMAP4_OUT_HDMI        (\
        AudioSystem::DEVICE_OUT_AUX_DIGITAL)

#define OMAP4_OUT_LP          (\
        AudioSystem::DEVICE_OUT_LOW_POWER)

#define OMAP4_OUT_DEFAULT   (\
        AudioSystem::DEVICE_OUT_ALL &\
        ~OMAP4_OUT_SCO &\
        ~OMAP4_OUT_FM  &\
        ~OMAP4_OUT_LP  &\
        ~OMAP4_OUT_HDMI)

#define OMAP4_IN_SCO        (\
        AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET)

#define OMAP4_IN_FM        (\
        AudioSystem::DEVICE_IN_FM_ANALOG)

#define OMAP4_IN_DEFAULT    (\
        AudioSystem::DEVICE_IN_ALL &\
        ~OMAP4_IN_SCO)

static alsa_handle_t _defaults[] = {
    {
        module      : 0,
        devices     : OMAP4_OUT_SCO,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 2,
        sampleRate  : DEFAULT_SAMPLE_RATE,
        latency     : 200000, // Desired Delay in usec
        bufferSize  : DEFAULT_SAMPLE_RATE / 5, // Desired Number of samples
        mmap        : 0,
        modPrivate  : (void *)&setDefaultControls,
    },
    {
        module      : 0,
        devices     : OMAP4_OUT_FM,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 2,
        sampleRate  : DEFAULT_SAMPLE_RATE,
        latency     : 200000, // Desired Delay in usec
        bufferSize  : DEFAULT_SAMPLE_RATE / 5, // Desired Number of samples
        mmap        : 0,
        modPrivate  : (void *)&setDefaultControls,
    },
    {
        module      : 0,
        devices     : OMAP4_OUT_HDMI,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 2,
        sampleRate  : DEFAULT_SAMPLE_RATE,
        latency     : 200000, // Desired Delay in usec
        bufferSize  : DEFAULT_SAMPLE_RATE / 5, // Desired Number of samples
        mmap        : 0,
        modPrivate  : (void *)&setDefaultControls,
    },
    {
        module      : 0,
        devices     : OMAP4_OUT_DEFAULT,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 2,
        sampleRate  : DEFAULT_SAMPLE_RATE,
        latency     : 85333, // Desired Delay in usec
        bufferSize  : 4096, // Desired Number of samples
        mmap        : 1,
        modPrivate  : (void *)&setDefaultControls,
    },
    {
        module      : 0,
        devices     : OMAP4_OUT_LP,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 2,
        sampleRate  : DEFAULT_SAMPLE_RATE,
        latency     : 140000, // Desired Delay in usec
        bufferSize  : 6144, // Desired Number of samples
        mmap        : 1,
        modPrivate  : (void *)&setDefaultControls,
    },
    {
        module      : 0,
        devices     : OMAP4_IN_SCO,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 1,
        sampleRate  : AudioRecord::DEFAULT_SAMPLE_RATE,
        latency     : 250000, // Desired Delay in usec
        bufferSize  : 2048, // Desired Number of samples
        mmap        : 0,
        modPrivate  : (void *)&setDefaultControls,
    },
    {
        module      : 0,
        devices     : OMAP4_IN_DEFAULT,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 1,
        sampleRate  : AudioRecord::DEFAULT_SAMPLE_RATE,
        latency     : 250000, // Desired Delay in usec
        bufferSize  : 2048, // Desired Number of samples
        mmap        : 0,
        modPrivate  : (void *)&setDefaultControls,
    },
    {
        module      : 0,
        devices     : OMAP4_IN_FM,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S32_LE,
        channels    : 2,
        sampleRate  : 48000,
        latency     : -1, // Doesn't matter, since it is buffer-less
        bufferSize  : 2048, // Desired Number of samples
        mmap        : 0,
        modPrivate  : (void *)&setDefaultControls,
    },

};

// ----------------------------------------------------------------------------

const char *deviceName(alsa_handle_t *handle, uint32_t device, int mode)
{
    char pwr[PROPERTY_VALUE_MAX];
    int status = 0;
    status = property_get("omap.audio.power", pwr, "HQ");

    if (device & OMAP4_OUT_SCO || device & OMAP4_IN_SCO)
        return BLUETOOTH_SCO_DEVICE;

    if (device & OMAP4_OUT_FM)
        return FM_TRANSMIT_DEVICE;

    if (device & OMAP4_OUT_HDMI)
        return HDMI_DEVICE;

    if (device & OMAP4_IN_FM)
        return FM_CAPTURE_DEVICE;

    if (device & OMAP4_IN_DEFAULT)
        return MM_DEFAULT_DEVICE;

    // now that low-power is flexible in buffer size and sample rate
    // a system property can be used to toggle
    if ((device & OMAP4_OUT_LP) ||
       (strcmp(pwr, "LP") == 0))
        return MM_LP_DEVICE;

    return MM_DEFAULT_DEVICE;
}

snd_pcm_stream_t direction(alsa_handle_t *handle)
{
    return (handle->devices & AudioSystem::DEVICE_OUT_ALL) ? SND_PCM_STREAM_PLAYBACK
            : SND_PCM_STREAM_CAPTURE;
}

const char *streamName(alsa_handle_t *handle)
{
    return snd_pcm_stream_name(direction(handle));
}

status_t setHardwareParams(alsa_handle_t *handle)
{
    snd_pcm_hw_params_t *hardwareParams;
    status_t err;

    snd_pcm_uframes_t periodSize, bufferSize, reqBuffSize;
    unsigned int periodTime, bufferTime;
    unsigned int requestedRate = handle->sampleRate;
    int numPeriods = 0;
    char pwr[PROPERTY_VALUE_MAX];
    int status = 0;

    // snd_pcm_format_description() and snd_pcm_format_name() do not perform
    // proper bounds checking.
    bool validFormat = (static_cast<int> (handle->format)
            > SND_PCM_FORMAT_UNKNOWN) && (static_cast<int> (handle->format)
            <= SND_PCM_FORMAT_LAST);
    const char *formatDesc = validFormat ? snd_pcm_format_description(
            handle->format) : "Invalid Format";
    const char *formatName = validFormat ? snd_pcm_format_name(handle->format)
            : "UNKNOWN";

    // device name will only return LP device hw06 if the property is set
    // or if the system is explicitly opening and routing to OMAP4_OUT_LP
    const char* device = deviceName(handle,
                                    handle->devices,
                                    AudioSystem::MODE_NORMAL);

    if (snd_pcm_hw_params_malloc(&hardwareParams) < 0) {
        LOG_ALWAYS_FATAL("Failed to allocate ALSA hardware parameters!");
        return NO_INIT;
    }

    err = snd_pcm_hw_params_any(handle->handle, hardwareParams);
    if (err < 0) {
        LOGE("Unable to configure hardware: %s", snd_strerror(err));
        goto done;
    }

    // Set the interleaved read and write format.
    if (handle->mmap) {
        snd_pcm_access_mask_t *mask =
            (snd_pcm_access_mask_t *)alloca(snd_pcm_access_mask_sizeof());
        snd_pcm_access_mask_none(mask);
        snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
        err = snd_pcm_hw_params_set_access_mask(
                        handle->handle,
                        hardwareParams,
                        mask);
    } else  {
        err = snd_pcm_hw_params_set_access(
                        handle->handle,
                        hardwareParams,
                        SND_PCM_ACCESS_RW_INTERLEAVED);
    }
    if (err < 0) {
        LOGE("Unable to configure PCM read/write format: %s",
                snd_strerror(err));
        goto done;
    }

    err = snd_pcm_hw_params_set_format(handle->handle, hardwareParams,
            handle->format);
    if (err < 0) {
        LOGE("Unable to configure PCM format %s (%s): %s",
                formatName, formatDesc, snd_strerror(err));
        goto done;
    }

    LOGV("Set %s PCM format to %s (%s)", streamName(handle), formatName, formatDesc);

    err = snd_pcm_hw_params_set_channels(handle->handle, hardwareParams,
            handle->channels);
    if (err < 0) {
        LOGE("Unable to set channel count to %i: %s",
                handle->channels, snd_strerror(err));
        goto done;
    }

    LOGV("Using %i %s for %s.", handle->channels,
            handle->channels == 1 ? "channel" : "channels", streamName(handle));

    err = snd_pcm_hw_params_set_rate_near(handle->handle, hardwareParams,
            &requestedRate, 0);
    if (err < 0)
        LOGE("Unable to set %s sample rate to %u: %s",
                streamName(handle), handle->sampleRate, snd_strerror(err));
    else if (requestedRate != handle->sampleRate)
        // Some devices have a fixed sample rate, and can not be changed.
        // This may cause resampling problems; i.e. PCM playback will be too
        // slow or fast.
        LOGW("Requested rate (%u HZ) does not match actual rate (%u HZ)",
                handle->sampleRate, requestedRate);
    else
        LOGV("Set %s sample rate to %u HZ", streamName(handle), requestedRate);

    if (strcmp(device, MM_LP_DEVICE) == 0) {
        numPeriods = 2;
        LOGI("Using ping-pong!");
    } else {
        numPeriods = 4;
        LOGI("Using FIFO");
    }
    reqBuffSize = handle->bufferSize;
    bufferSize = reqBuffSize;

    // try the requested buffer size
    err = snd_pcm_hw_params_set_buffer_size_near(handle->handle, hardwareParams,
            &bufferSize);
    if (err < 0) {
        LOGE("Unable to set buffer size to %d:  %s",
                (int)bufferSize, snd_strerror(err));
        goto done;
    }
    // did we get what we asked for? we should.
    if (bufferSize != reqBuffSize) {
         LOGW("Requested buffer size %d not granted, got %d",
                (int)reqBuffSize, (int)bufferSize);
    }
    // set the period size for our buffer
    periodSize = bufferSize / numPeriods;
    err = snd_pcm_hw_params_set_period_size_near(handle->handle,
            hardwareParams, &periodSize, NULL);
    if (err < 0) {
        LOGE("Unable to set period size to %d:  %s", (int)periodSize, snd_strerror(err));
        goto done;
    }
    // check our period time
    err = snd_pcm_hw_params_get_period_time(hardwareParams, &periodTime, NULL);
    if (err < 0) {
        LOGE("Unable to get period time:  %s", snd_strerror(err));
        goto done;
    }
    // get the buffer & period time
    err = snd_pcm_hw_params_get_buffer_time(hardwareParams, &bufferTime, NULL);
    if (err < 0) {
        LOGE("Unable to set buffer time:  %s", snd_strerror(err));
        goto done;
    }
    // get the buffer size again in case setting the period size changed it
    err = snd_pcm_hw_params_get_buffer_size(hardwareParams, &bufferSize);
    if (err < 0) {
        LOGE("Unable to set buffer size:  %s", snd_strerror(err));
        goto done;
    }

    if (strcmp(device, MM_LP_DEVICE) == 0) {
        handle->bufferSize = periodSize;
        handle->latency = periodTime;
    } else {
        handle->bufferSize = bufferSize;
        handle->latency = bufferTime;
    }

    LOGI("Buffer size: %d", (int)(handle->bufferSize));
    LOGI("Latency: %d", (int)(handle->latency));

    // Commit the hardware parameters back to the device.
    err = snd_pcm_hw_params(handle->handle, hardwareParams);
    if (err < 0) LOGE("Unable to set hardware parameters: %s", snd_strerror(err));

    done:
    snd_pcm_hw_params_free(hardwareParams);

    return err;
}

status_t setSoftwareParams(alsa_handle_t *handle)
{
    snd_pcm_sw_params_t * softwareParams;
    int err;

    snd_pcm_uframes_t bufferSize = 0;
    snd_pcm_uframes_t periodSize = 0;
    snd_pcm_uframes_t startThreshold, stopThreshold;

    if (snd_pcm_sw_params_malloc(&softwareParams) < 0) {
        LOG_ALWAYS_FATAL("Failed to allocate ALSA software parameters!");
        return NO_INIT;
    }

    // Get the current software parameters
    err = snd_pcm_sw_params_current(handle->handle, softwareParams);
    if (err < 0) {
        LOGE("Unable to get software parameters: %s", snd_strerror(err));
        goto done;
    }

    // Configure ALSA to start the transfer when the buffer is almost full.
    snd_pcm_get_params(handle->handle, &bufferSize, &periodSize);

    if (handle->devices & AudioSystem::DEVICE_OUT_ALL) {
        // For playback, configure ALSA to start the transfer when the
        // buffer is full.
        startThreshold = bufferSize - 1;
        stopThreshold = bufferSize;
    } else {
        // For recording, configure ALSA to start the transfer on the
        // first frame.
        startThreshold = 1;
        if (handle->devices & OMAP4_IN_FM) {
            LOGV("Stop Threshold for FM Rx is -1");
            stopThreshold = -1; // For FM Rx via ABE
        } else
            stopThreshold = bufferSize;
    }

    err = snd_pcm_sw_params_set_start_threshold(handle->handle, softwareParams,
            startThreshold);
    if (err < 0) {
        LOGE("Unable to set start threshold to %lu frames: %s",
                startThreshold, snd_strerror(err));
        goto done;
    }

    err = snd_pcm_sw_params_set_stop_threshold(handle->handle, softwareParams,
            stopThreshold);
    if (err < 0) {
        LOGE("Unable to set stop threshold to %lu frames: %s",
                stopThreshold, snd_strerror(err));
        goto done;
    }

    // Allow the transfer to start when at least periodSize samples can be
    // processed.
    err = snd_pcm_sw_params_set_avail_min(handle->handle, softwareParams,
            periodSize);
    if (err < 0) {
        LOGE("Unable to configure available minimum to %lu: %s",
                periodSize, snd_strerror(err));
        goto done;
    }

    // Commit the software parameters back to the device.
    err = snd_pcm_sw_params(handle->handle, softwareParams);
    if (err < 0) LOGE("Unable to configure software parameters: %s",
            snd_strerror(err));

    done:
    snd_pcm_sw_params_free(softwareParams);

    return err;
}

void setDefaultControls(uint32_t devices, int mode)
{
LOGV("%s", __FUNCTION__);
    ALSAControl control("hw:00");

    /* check whether the devices is input or not */
    /* for output devices */
    if (devices & 0x0000FFFF){
        if (devices & AudioSystem::DEVICE_OUT_SPEAKER) {
            /* OMAP4 ABE */
            control.set("DL2 Mixer Multimedia", 1);		// MM_DL    -> DL2 Mixer
            control.set("DL2 Media Playback Volume", 118);
            /* TWL6040 */
            control.set("HF Left Playback", "HF DAC");		// HFDAC L -> HF Mux
            control.set("HF Right Playback", "HF DAC");		// HFDAC R -> HF Mux
            control.set("Handsfree Playback Volume", 23);
            if (fm_enable) {
                LOGE("FM ABE speaker");
                control.set("DL2 Capture Playback Volume", 115);
                control.set("DL1 Capture Playback Volume", 0, -1);
            }
        } else {
            /* OMAP4 ABE */
            control.set("DL2 Mixer Multimedia", 0, 0);
            control.set("DL2 Media Playback Volume", 0, -1);
            control.set("DL2 Capture Playback Volume", 0, -1);
            /* TWL6040 */
            control.set("HF Left Playback", "Off");
            control.set("HF Right Playback", "Off");
            control.set("Handsfree Playback Volume", 0, -1);
        }

        if ((devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) ||
            (devices & AudioSystem::DEVICE_OUT_LOW_POWER)) {
            /* TWL6040 */
            control.set("HS Left Playback", "HS DAC");		// HSDAC L -> HS Mux
            control.set("HS Right Playback", "HS DAC");		// HSDAC R -> HS Mux
            control.set("Headset Playback Volume", 15);
        } else {
            /* TWL6040 */
            control.set("HS Left Playback", "Off");
            control.set("HS Right Playback", "Off");
            control.set("Headset Playback Volume", 0, -1);
        }

        if (devices & AudioSystem::DEVICE_OUT_EARPIECE) {
            /* TWL6040 */
            control.set("Earphone Driver Switch", 1);		// HSDACL -> Earpiece
            control.set("Earphone Playback Volume", 15);
        } else {
            /* TWL6040 */
            control.set("Earphone Driver Switch", 0, 0);
            control.set("Earphone Playback Volume", 0, -1);
        }
        if ((devices & AudioSystem::DEVICE_OUT_EARPIECE) ||
            (devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) ||
            (devices & AudioSystem::DEVICE_OUT_FM_TRANSMIT) ||
            (devices & AudioSystem::DEVICE_OUT_LOW_POWER)) {
            /* OMAP4 ABE */
            control.set("DL1 Mixer Multimedia", 1);		// MM_DL    -> DL1 Mixer
            control.set("Sidetone Mixer Playback", 1);		// DL1 Mixer-> Sidetone Mixer
            if (devices & OMAP4_OUT_FM) {
              /* FM Tx: DL1 MM_EXT Switch */
              control.set("DL1 MM_EXT Switch", 1);
              control.set("DL1 PDM Switch", 0, 0);
            } else {
              control.set("DL1 PDM Switch", 1);                 // Sidetone Mixer -> PDM_DL1
              control.set("DL1 MM_EXT Switch", 0, 0);
            }
            control.set("DL1 Media Playback Volume", 118);
            if (fm_enable) {
                LOGI("FM Enabled, DL1 Capture-Playback Vol ON");
                control.set("DL1 Capture Playback Volume", 115);
                control.set("DL2 Capture Playback Volume", 0, -1);
            }
        } else {
            /* OMAP4 ABE */
            control.set("DL1 Mixer Multimedia", 0, 0);
            control.set("Sidetone Mixer Playback", 0, 0);
            control.set("DL1 PDM Switch", 0, 0);
            control.set("DL1 Media Playback Volume", 0, -1);
            control.set("DL1 Capture Playback Volume", 0, -1);
        }
        if ((devices & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO) ||
            (devices & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_HEADSET) ||
            (devices & AudioSystem::DEVICE_OUT_BLUETOOTH_SCO_CARKIT)) {
            /* OMAP4 ABE */
            /* Bluetooth: DL1 Mixer */
            control.set("DL1 Mixer Multimedia", 1);		// MM_DL    -> DL1 Mixer
            control.set("Sidetone Mixer Playback", 1);		// DL1 Mixer-> Sidetone Mixer
            control.set("DL1 BT_VX Switch", 1);			// Sidetone Mixer -> BT-VX-DL
            control.set("DL1 Media Playback Volume", 118);
        } else {
            control.set("DL1 BT_VX Switch", 0, 0);
        }
        if ((devices & AudioSystem::DEVICE_OUT_SPEAKER) ||
            (devices & AudioSystem::DEVICE_OUT_AUX_DIGITAL)) {
            // Setting DL2 EQ's to 800Hz cut-off frequency, as setting
            // to flat response saturates the audio quality in the
            // handsfree speakers
            control.set("DL2 Left Equalizer Profile", 1, 0);
            control.set("DL2 Right Equalizer Profile", 1, 0);
        }
        if ((devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET) ||
            (devices & AudioSystem::DEVICE_OUT_EARPIECE)) {
            control.set("DL1 Equalizer Profile", 0, 0);
        }
        LOGI("SETTING DEVICE OUT LOW POWER MODE");
        control.set("TWL6040 Power Mode", "Low-Power");

    }

    /* for input devices */
    if (devices >> 16) {
        if (devices & AudioSystem::DEVICE_IN_BUILTIN_MIC) {
            configMicChoices(devices);
            /* TWL6040 */
            control.set("Analog Left Capture Route", "Main Mic");	// Main Mic -> Mic Mux
            control.set("Analog Right Capture Route", "Sub Mic");	// Sub Mic  -> Mic Mux
            control.set("Capture Preamplifier Volume", 1);
            control.set("Capture Volume", 4);
        } else if (devices & AudioSystem::DEVICE_IN_WIRED_HEADSET) {
            /* TWL6040 */
            control.set("Analog Left Capture Route", "Headset Mic");	// Headset Mic -> Mic Mux
            control.set("Analog Right Capture Route", "Headset Mic");	// Headset Mic -> Mic Mux
            control.set("Capture Preamplifier Volume", 1);
            control.set("Capture Volume", 4);
        } else if (devices & OMAP4_IN_FM) {
            /* TWL6040 */
            control.set("Analog Left Capture Route", "Aux/FM Left");     // FM -> Mic Mux
            control.set("Analog Right Capture Route", "Aux/FM Right");   // FM -> Mic Mux
            control.set("Capture Preamplifier Volume", 1);
            control.set("Capture Volume", 1);
            control.set("AMIC_UL PDM Switch", 1);
            control.set("MUX_UL10", "AMic1");
            control.set("MUX_UL11", "AMic0");
        } else if(devices & OMAP4_IN_SCO) {
            LOGI("OMAP4 ABE set for BT SCO Headset");
            control.set("AMIC_UL PDM Switch", 0, 0);
            control.set("MUX_UL00", "BT Right");
            control.set("MUX_UL01", "BT Left");
            control.set("MUX_UL10", "BT Right");
            control.set("MUX_UL11", "BT Left");
            control.set("Voice Capture Mixer Capture", 1);
        } else {
            /* TWL6040 */
            control.set("Analog Left Capture Route", "Off");
            control.set("Analog Right Capture Route", "Off");
            control.set("Capture Preamplifier Volume", 0, -1);
            control.set("Capture Volume", 0, -1);
            control.set("Voice Capture Mixer Capture", 0, 0);
            control.set("AMIC_UL PDM Switch", 0, 0);
            /* ABE */
            control.set("MUX_UL00", "None");
            control.set("MUX_UL01", "None");
            control.set("MUX_UL10", "None");
            control.set("MUX_UL11", "None");
        }
    }
}

void setAlsaControls(alsa_handle_t *handle, uint32_t devices, int mode)
{
    AlsaControlSet set = (AlsaControlSet) handle->modPrivate;
    set(devices, mode);
    handle->curDev = devices;
    handle->curMode = mode;
}

// ----------------------------------------------------------------------------

static status_t s_init(alsa_device_t *module, ALSAHandleList &list)
{
    LOGD("Initializing devices for OMAP4 ALSA module");

    list.clear();

    for (size_t i = 0; i < ARRAY_SIZE(_defaults); i++) {

        snd_pcm_uframes_t bufferSize = _defaults[i].bufferSize;

        for (size_t b = 1; (bufferSize & ~b) != 0; b <<= 1)
            bufferSize &= ~b;

        _defaults[i].module = module;
        _defaults[i].bufferSize = bufferSize;

        list.push_back(_defaults[i]);
    }

#ifdef AUDIO_MODEM_TI
    audioModem = new AudioModemAlsa();
#endif

    return NO_ERROR;
}

static status_t s_open(alsa_handle_t *handle, uint32_t devices, int mode)
{
    // Close off previously opened device.
    // It would be nice to determine if the underlying device actually
    // changes, but we might be recovering from an error or manipulating
    // mixer settings (see asound.conf).
    //
    s_close(handle);

    LOGD("open called for devices %08x in mode %d...", devices, mode);

    const char *stream = streamName(handle);
    const char *devName = deviceName(handle, devices, mode);

    // ASoC multicomponent requires a valid path (frontend/backend) for
    // the device to be opened
    setAlsaControls(handle, devices, mode);

#ifdef AUDIO_MODEM_TI
    audioModem->voiceCallControls(devices, mode, true);
#endif

    // The PCM stream is opened in blocking mode, per ALSA defaults.  The
    // AudioFlinger seems to assume blocking mode too, so asynchronous mode
    // should not be used.
    int err = snd_pcm_open(&handle->handle, devName, direction(handle), 0);

    if (err < 0) {
        LOGE("Failed to initialize ALSA %s device '%s': %s", stream, devName, strerror(err));
        return NO_INIT;
    }

    mActive = true;

    err = setHardwareParams(handle);

    if (err == NO_ERROR) err = setSoftwareParams(handle);

    LOGI("Initialized ALSA %s device '%s'", stream, devName);
    // For FM Rx through ABE, McPDM UL needs to be triggered
    if (devices &  OMAP4_IN_FM) {
       LOGI("Triggering McPDM UL");
       fm_enable = true;
       snd_pcm_start(handle->handle);
    } else if (0 == devices) {
       fm_enable = false;
    }
    return err;
}

static status_t s_close(alsa_handle_t *handle)
{
    status_t err = NO_ERROR;
    snd_pcm_t *h = handle->handle;
    handle->handle = 0;
    handle->curDev = 0;
    handle->curMode = 0;
    if (h) {
        snd_pcm_drain(h);
        err = snd_pcm_close(h);
        mActive = false;
    }

    return err;
}

/*
    this is same as s_close, but don't discard
    the device/mode info. This way we can still
    close the device, hit idle and power-save, reopen the pcm
    for the same device/mode after resuming
*/
static status_t s_standby(alsa_handle_t *handle)
{
    status_t err = NO_ERROR;
    snd_pcm_t *h = handle->handle;
    handle->handle = 0;
    LOGV("In omap4 standby\n");
    if (h) {
        snd_pcm_drain(h);
        err = snd_pcm_close(h);
        mActive = false;
        LOGE("called drain&close\n");
    }

    return err;
}

static status_t s_route(alsa_handle_t *handle, uint32_t devices, int mode)
{
    status_t status = NO_ERROR;

    LOGD("route called for devices %08x in mode %d...", devices, mode);

    if (!devices) {
        LOGV("Ignore the audio routing change as there's no device specified");
        return NO_ERROR;
    }

    if (handle->curDev != devices) {
        if (mActive) {
            status = s_open(handle, devices, mode);
        } else {
            setAlsaControls(handle, devices, mode);
        }
    }

#ifdef AUDIO_MODEM_TI
        status = audioModem->voiceCallControls(devices, mode, false);
#endif

    return status;
}

static status_t s_voicevolume(float volume)
{
    status_t status = NO_ERROR;

#ifdef AUDIO_MODEM_TI
        ALSAControl control("hw:00");
        if (audioModem) {
            status = audioModem->voiceCallVolume(&control, volume);
        } else {
            LOGE("Audio Modem not initialized: voice volume can't be applied");
            status = NO_INIT;
        }
#endif

    return status;
}

void configMicChoices (uint32_t devices) {

    ALSAControl control("hw:00");
    char mic1[PROPERTY_VALUE_MAX];
    char mic2[PROPERTY_VALUE_MAX];
    int status = 0;

    status = property_get("omap.audio.mic.main", mic1, "AMic0");
    status = property_get("omap.audio.mic.sub", mic2, "AMic1");

    //for Main Mic
    if (!strcmp(mic1, "DMic0L")) {
        LOGI("OMAP4 ABE set for Digital Mic 0L");
        control.set("AMIC_UL PDM Switch", 0, 0);  // PDM_UL1 -> off
        control.set("MUX_UL00", mic1);           // DMIC0L_UL -> MM_UL00
    }
    else {
        LOGI("OMAP4 ABE set for Analog Main Mic 0");
        control.set("AMIC_UL PDM Switch", 1);     // PDM_UL1 -> AMIC_UL
        control.set("MUX_UL00", "AMic0");         // AMIC_UL -> MM_UL00
    }
    //for Sub Mic
    if (!strcmp(mic2, "DMic0R")) {
        LOGI("OMAP4 ABE set for Digital Sub Mic 0R");
        control.set("AMIC_UL PDM Switch", 0, 0);  // PDM_UL1 -> off
        control.set("MUX_UL01", mic2);           // DMIC0R_UL -> MM_UL01
    }
    else {
        LOGI("OMAP4 ABE set for Analog Sub Mic 1");
        control.set("AMIC_UL PDM Switch", 1);      // PDM_UL1 -> AMIC_UL
        control.set("MUX_UL01", "AMic1");          // AMIC_UL -> MM_UL01
    }

    /** 
     * configure EQ profile for Analog or Digital
     * since we only use flat response for MM, we can just reset
     * both amic and dmic eq profiles.
    **/
    control.set("DMIC Equalizer Profile", 0, 0);
    control.set("AMIC Equalizer Profile", 0, 0);
}
}

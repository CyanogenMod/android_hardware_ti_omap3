#ifndef _AUDIO_STREAM_IN_OMAP_CPP_
#define _AUDIO_STREAM_IN_OMAP_CPP_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define LOG_TAG "AudioStreamInOmap"
#include <utils/threads.h>
#include <utils/Log.h>
#include <utils/String8.h>

#include <alsa/asoundlib.h>

#include <hardware/AudioHardwareInterface.h>
#include "AudioHardwareOmap.h"
#include "AudioStreamInOmap.h"

using namespace android;

/* AudioStreamIn constructor */
AudioStreamIn::~AudioStreamIn()
{
}

/* Constructor */
AudioStreamInOmap::AudioStreamInOmap()
{
       mAudioHardware = 0;
       snd_pcm_hw_params_alloca(&hwParams);
}

/* Destructor */
AudioStreamInOmap::~AudioStreamInOmap()
{
       if (mAudioHardware) {
               mAudioHardware->closeInputStream(this);
       }
}

/* Set properties */
status_t AudioStreamInOmap::set(AudioHardwareOmap *hw, snd_pcm_t *handle,
                                       int format, int channels, uint32_t rate)
{
       int ret = 0;

       LOGW("Setting input parameters!!!!!!!!!!!!!!!!!!!!");
       ret = snd_pcm_hw_params_any(handle, hwParams);
       if (ret) {
               LOGE("Error initializing input hardware parameters");
       }

       ret = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
       if (ret) {
               LOGE("Error setting input access type");
               return BAD_VALUE;
       }

       ret = snd_pcm_hw_params_set_format(handle, hwParams, SND_PCM_FORMAT_S16_LE);
       if (ret) {
               LOGE("Error setting input format");
               return BAD_VALUE;
       }

       ret = snd_pcm_hw_params_set_rate(handle, hwParams, rate, 0);
       if (ret) {
               LOGE("Error setting input sample rate");
               return BAD_VALUE;
       }

       ret = snd_pcm_hw_params_set_channels(handle, hwParams, channels);
       if (ret) {
               LOGE("Error setting number of input channels");
               return BAD_VALUE;
       }

       ret = snd_pcm_hw_params(handle, hwParams);
       if (ret) {
               LOGE("Error applying parameters to PCM device");
               return BAD_VALUE;
       }

       mAudioHardware = hw;
       pcmHandle = handle;

       return NO_ERROR;
}

/* Get number of bytes per frame */
int AudioStreamInOmap::bytesPerFrame() const
{
       snd_pcm_format_t format;
       int bytesPerFrame = 0;
       int ret = 0;

       ret = snd_pcm_hw_params_get_format(hwParams, &format);
       if (ret) {
               LOGE("Error getting input format");
               return 0;
       }

       switch(format) {
       case SND_PCM_FORMAT_S8:         bytesPerFrame = 1;
                                       break;
       case SND_PCM_FORMAT_S16_LE:     bytesPerFrame = 2;
                                       break;
       default:                        bytesPerFrame = 0;
                                       LOGE("Invalid input format");
                                       break;
       }

       bytesPerFrame *= channelCount();

       return bytesPerFrame;
}

/* Get sample rate */
uint32_t AudioStreamInOmap::sampleRate() const
{
       unsigned int val = 0;
       int dir = 0;
       int ret = 0;

       ret = snd_pcm_hw_params_get_rate(hwParams, &val, &dir);
       if (ret) {
               LOGE("Error getting input sample rate");
               return 0;
       }

       return val;
}

/* Get buffer size */
size_t AudioStreamInOmap::bufferSize() const
{
       snd_pcm_uframes_t val;
       size_t bufferSize = 0;
       int ret = 0;

       ret = snd_pcm_hw_params_get_buffer_size(hwParams, &val);
       if (ret) {
               LOGE("Error getting input buffer size");
               return 0;
       }

       bufferSize = val * bytesPerFrame();

       return bufferSize;
}

/* Get number of channels */
int AudioStreamInOmap::channelCount() const
{
       unsigned int val = 0;
       int ret = 0;

       ret = snd_pcm_hw_params_get_channels(hwParams, &val);
       if (ret) {
               LOGE("Error getting number of input channels");
               return 0;
       }

       return val;
}

/* Get pcm format */
int AudioStreamInOmap::format() const
{
       snd_pcm_format_t format;
       int inFormat;
       int ret = 0;

       ret = snd_pcm_hw_params_get_format(hwParams, &format);
       if (ret) {
               LOGE("Error getting input format");
               return 0;
       }

       switch(format) {
       case SND_PCM_FORMAT_S8:         inFormat = AudioSystem::PCM_8_BIT;
                                       break;
       case SND_PCM_FORMAT_S16_LE:     inFormat = AudioSystem::PCM_16_BIT;
                                       break;
       default:                        inFormat = 0;
                                       LOGE("Invalid input format");
                                       break;
       }

       return inFormat;
}

/* Set volume via hardware */
status_t AudioStreamInOmap::setGain(float gain)
{
       return NO_ERROR;
}

/* Read buffer */
ssize_t AudioStreamInOmap::read(void* buffer, ssize_t bytes)
{
       snd_pcm_uframes_t frames;
       snd_pcm_sframes_t rFrames;

       frames = bytes / bytesPerFrame();

       AutoMutex lock(mLock);
       rFrames = snd_pcm_readi(pcmHandle, buffer, frames);
       if (rFrames < 0) {
               snd_pcm_prepare(pcmHandle);
               LOGW("Buffer overrun");
       }

       return (ssize_t) rFrames;
}

/* Dump for debugging */
status_t AudioStreamInOmap::dump(int fd, const Vector<String16>& args)
{
       return NO_ERROR;
}

#endif

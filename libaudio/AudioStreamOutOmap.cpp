/*
 * AudioStreamOutOmap.cpp
 *
 * Audio output stream handler for OMAP.
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _AUDIO_STREAM_OUT_OMAP_CPP_
#define _AUDIO_STREAM_OUT_OMAP_CPP_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define LOG_TAG "AudioStreamOutOmap"
#include <utils/threads.h>
#include <utils/Log.h>
#include <utils/String8.h>

#include <hardware_legacy/AudioHardwareInterface.h>
#include <alsa/asoundlib.h>

#include "AudioStreamOmap.h"
#include "AudioStreamOutOmap.h"

using namespace android;

/* Constructor */
AudioStreamOutOmap::AudioStreamOutOmap()
{
       streaming = false;
}

/* Destructor */
AudioStreamOutOmap::~AudioStreamOutOmap()
{
       /* If stream is not already closed */
       closeStream();
}

status_t AudioStreamOutOmap::openStream()
{
       snd_pcm_t *handle;
       snd_pcm_hw_params_t *hwParams;
       char *pcmName;
	int ret = 0;

       pcmName = strdup("default");

       ret = snd_pcm_hw_params_malloc(&hwParams);
       if (ret) {
               LOGE("Error allocating hardware params");
               return BAD_VALUE;
       }

       ret = snd_pcm_open(&handle, pcmName, SND_PCM_STREAM_PLAYBACK, 0);
       if (ret) {
               LOGE("Error opening PCM interface");
               return BAD_VALUE;
       }

	ret = snd_pcm_hw_params_any(handle, hwParams);
	if (ret) {
		LOGE("Error initializing output hardware parameters");
		return BAD_VALUE;
	}

	ret = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret) {
		LOGE("Error setting output access type");
		return BAD_VALUE;
	}

       ret = snd_pcm_hw_params_set_format(handle, hwParams, properties.format);
	if (ret) {
		LOGE("Error setting output format");
		return BAD_VALUE;
	}

       ret = snd_pcm_hw_params_set_rate(handle, hwParams, properties.rate, 0);
	if (ret) {
               LOGE("Error setting output sample rate %d", properties.rate);
		return BAD_VALUE;
	}

       ret = snd_pcm_hw_params_set_channels(handle, hwParams, properties.channels);
	if (ret) {
		LOGE("Error setting number of output channels");
		return BAD_VALUE;
	}

       ret = snd_pcm_hw_params_set_buffer_size(handle, hwParams, properties.buffer_size);
	if (ret) {
		LOGE("Error setting buffer size");
		return BAD_VALUE;
	}
	
	ret = snd_pcm_hw_params(handle, hwParams);
	if (ret) {
		LOGE("Error applying parameters to PCM device");
		return BAD_VALUE;
	}

	pcmHandle = handle;
       streaming = true;

       return NO_ERROR;
}

status_t AudioStreamOutOmap::closeStream()
{
       if (pcmHandle)
               snd_pcm_close(pcmHandle);

       streaming = false;

       return NO_ERROR;
}

/* Set properties */
status_t AudioStreamOutOmap::set(int format, int channels, uint32_t rate, int buffer_size)
{
       if (format == AudioSystem::PCM_8_BIT)
               properties.format = SND_PCM_FORMAT_S8;
       else if (format == AudioSystem::PCM_16_BIT)
               properties.format = SND_PCM_FORMAT_S16_LE;
       else if (format == 0)   /* If not specified */
               properties.format = SND_PCM_FORMAT_S16_LE;
       else
               properties.format = SND_PCM_FORMAT_UNKNOWN;

       if (channels > 0)
               properties.channels = channels;
       else                    /* If not specified */
               properties.channels = 2;

       if (rate > 0)
               properties.rate = rate;
       else                    /* If not specified */
               properties.rate = 44100;

       /* Convert buffer size from bytes to frames */
       properties.buffer_size = buffer_size / bytesPerFrame();

	return NO_ERROR;
}

/* Get number of bytes per frame */
int AudioStreamOutOmap::bytesPerFrame() const
{
	int bytesPerFrame = 0;

       switch(properties.format) {
	case SND_PCM_FORMAT_S8:		bytesPerFrame = 1;
					break;
	case SND_PCM_FORMAT_S16_LE:	bytesPerFrame = 2;
					break;
	default:			bytesPerFrame = 0;
					LOGE("Invalid output format");
					break;
	}

	bytesPerFrame *= channelCount();

	return bytesPerFrame;
}

/* Get sample rate */
uint32_t AudioStreamOutOmap::sampleRate() const
{
       return properties.rate;
}

/* Get buffer size */
size_t AudioStreamOutOmap::bufferSize() const
{
       return properties.buffer_size * bytesPerFrame();
}

/* Get number of channels */
int AudioStreamOutOmap::channelCount() const
{
       return properties.channels;
}

/* Get pcm format */
int AudioStreamOutOmap::format() const
{
	int outFormat;

       switch(properties.format) {
	case SND_PCM_FORMAT_S8:		outFormat = AudioSystem::PCM_8_BIT;
					break;
	case SND_PCM_FORMAT_S16_LE:	outFormat = AudioSystem::PCM_16_BIT;
					break;
	default:			outFormat = AudioSystem::INVALID_FORMAT;
					LOGE("Invalid output format");
					break;
	}

	return outFormat;
}

/* Get audio hardware latency (ms) */
uint32_t AudioStreamOutOmap::latency() const
{
	return 0;
}

/* Set volume via hardware */
status_t AudioStreamOutOmap::setVolume(float volume)
{
	return NO_ERROR;
}

/* Write buffer */
ssize_t AudioStreamOutOmap::write(const void* buffer, size_t bytes)
{
	snd_pcm_uframes_t frames;
	snd_pcm_sframes_t wFrames;
	int ret = 0;

	frames = bytes / bytesPerFrame();

       if (!streaming)
               if  (openStream() != NO_ERROR)
                       return 0;

       Mutex::Autolock _l(mLock);
	wFrames = snd_pcm_writei(pcmHandle, buffer, frames);
	if (wFrames < 0) {
		LOGW("Buffer underrun");
		ret = snd_pcm_recover(pcmHandle, wFrames, 0);
		if (ret) {
			LOGE("Error recovering from audio error");
		}
	}
	if ((wFrames > 0) && ((unsigned int) wFrames < frames)) {
		LOGW("Short write %d instead of %d frames", (int) wFrames, (int) frames);
		ret = snd_pcm_prepare(pcmHandle);
		if (ret) {
			LOGE("Error recovering from audio error");
		}
	}

	return (ssize_t) wFrames;
}

/* Dump for debugging */
status_t AudioStreamOutOmap::dump(int fd, const Vector<String16>& args)
{
	return NO_ERROR;
}

status_t AudioStreamOutOmap::standby()
{
       AutoMutex lock(mLock);

       if(pcmHandle) {
               snd_pcm_drain(pcmHandle);
               closeStream();
       }

	return NO_ERROR;
}

#endif

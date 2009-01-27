/*
 * AudioStreamInOmap.cpp
 *
 * Audio input stream handler for OMAP.
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

#include <hardware_legacy/AudioHardwareInterface.h>
#include <alsa/asoundlib.h>

#include "AudioStreamOmap.h"
#include "AudioStreamInOmap.h"

using namespace android;

/* Constructor */
AudioStreamInOmap::AudioStreamInOmap()
{
       streaming = false;
}

/* Destructor */
AudioStreamInOmap::~AudioStreamInOmap()
{
       /* If stream is not already closed */
       closeStream();
}

status_t AudioStreamInOmap::openStream()
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

       ret = snd_pcm_open(&handle, pcmName, SND_PCM_STREAM_CAPTURE, 0);
       if (ret) {
               LOGE("Error opening PCM interface");
               return BAD_VALUE;
       }

	ret = snd_pcm_hw_params_any(handle, hwParams);
	if (ret) {
		LOGE("Error initializing input hardware parameters");
		return BAD_VALUE;
	}

	ret = snd_pcm_hw_params_set_access(handle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret) {
		LOGE("Error setting input access type");
		return BAD_VALUE;
	}

       ret = snd_pcm_hw_params_set_format(handle, hwParams, properties.format);
	if (ret) {
		LOGE("Error setting input format");
		return BAD_VALUE;
	}

       ret = snd_pcm_hw_params_set_rate(handle, hwParams, properties.rate, 0);
	if (ret) {
               LOGE("Error setting input sample rate %d", properties.rate);
		return BAD_VALUE;
       }

       ret = snd_pcm_hw_params_set_channels(handle, hwParams, properties.channels);
	if (ret) {
		LOGE("Error setting number of input channels");
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

status_t AudioStreamInOmap::closeStream()
{
       if (pcmHandle)
               snd_pcm_close(pcmHandle);

       streaming = false;

       return NO_ERROR;
}

/* Set properties */
status_t AudioStreamInOmap::set(int format, int channels, uint32_t rate, int buffer_size)
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
               properties.channels = 1;

       if (rate > 0)
               properties.rate = rate;
       else                    /* If not specified */
               properties.rate = 8000;

       /* Convert buffer size from bytes to frames */
       properties.buffer_size = buffer_size / bytesPerFrame();

	return NO_ERROR;
}

/* Get number of bytes per frame */
int AudioStreamInOmap::bytesPerFrame() const
{
	int bytesPerFrame = 0;

       switch(properties.format) {
	case SND_PCM_FORMAT_S8:		bytesPerFrame = 1;
					break;
	case SND_PCM_FORMAT_S16_LE:	bytesPerFrame = 2;
					break;
	default:			bytesPerFrame = 0;
					LOGE("Invalid input format");
					break;
	}

	bytesPerFrame *= channelCount();

	return bytesPerFrame;
}

/* Get sample rate */
uint32_t AudioStreamInOmap::sampleRate() const
{
       return properties.rate;
}

/* Get buffer size */
size_t AudioStreamInOmap::bufferSize() const
{
       return properties.buffer_size * bytesPerFrame();
}

/* Get number of channels */
int AudioStreamInOmap::channelCount() const
{
       return properties.channels;
}

/* Get pcm format */
int AudioStreamInOmap::format() const
{
	int inFormat;

       switch(properties.format) {
	case SND_PCM_FORMAT_S8:		inFormat = AudioSystem::PCM_8_BIT;
					break;
	case SND_PCM_FORMAT_S16_LE:	inFormat = AudioSystem::PCM_16_BIT;
					break;
	default:			inFormat = AudioSystem::INVALID_FORMAT;
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
	int ret = 0;

	frames = bytes / bytesPerFrame();

       if (!streaming)
               if  (openStream() != NO_ERROR)
                       return 0;

       AutoMutex lock(mLock);
	rFrames = snd_pcm_readi(pcmHandle, buffer, frames);
	if (rFrames < 0) {
		LOGW("Buffer overrun");
		ret = snd_pcm_recover(pcmHandle, rFrames, 0);
		if (ret) {
			LOGE("Error recovering from audio error");
		}
	}
	if ((rFrames > 0) && ((unsigned int) rFrames < frames)) {
		LOGW("Short read %d instead of %d frames", (int) rFrames, (int) frames);
		ret = snd_pcm_prepare(pcmHandle);
		if (ret) {
			LOGE("Error recovering from audio error");
		}
	}
	
	return (ssize_t) rFrames;
}

/* Dump for debugging */
status_t AudioStreamInOmap::dump(int fd, const Vector<String16>& args)
{
	return NO_ERROR;
}

status_t AudioStreamInOmap::standby()
{
       AutoMutex lock(mLock);

       if(pcmHandle) {
               snd_pcm_drain(pcmHandle);
               closeStream();
       }

	return NO_ERROR;
}

#endif

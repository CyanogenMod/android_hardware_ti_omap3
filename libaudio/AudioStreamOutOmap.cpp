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

#include <hardware/AudioHardwareInterface.h>
#include <alsa/asoundlib.h>

#include "AudioHardwareOmap.h"
#include "AudioStreamOutOmap.h"

using namespace android;

/* Constructor */
AudioStreamOutOmap::AudioStreamOutOmap()
{
	int ret = 0;

	mAudioHardware = 0;
	ret = snd_pcm_hw_params_malloc(&hwParams);
	if (ret) {
		LOGE("Error allocating hardware params");
	}
}

/* Destructor */
AudioStreamOutOmap::~AudioStreamOutOmap()
{
	if (mAudioHardware) {
		mAudioHardware->closeOutputStream(this);
	}
}

/* Set properties */
status_t AudioStreamOutOmap::set(AudioHardwareOmap *hw, snd_pcm_t *handle,
				int format, int channels, uint32_t rate)
{
	snd_pcm_format_t alsa_format;
	unsigned int alsa_channels = 0;
	unsigned int alsa_rate = 0;
	snd_pcm_uframes_t  alsa_buffer_size;
	int ret = 0;

	if (format == AudioSystem::PCM_8_BIT)
		alsa_format = SND_PCM_FORMAT_S8;
	else if (format == AudioSystem::PCM_16_BIT)
		alsa_format = SND_PCM_FORMAT_S16_LE;
	else if (format == 0)
		alsa_format = SND_PCM_FORMAT_S16_LE;
	/* If not specified */
	else
		alsa_format = SND_PCM_FORMAT_UNKNOWN;

	/* If not specified */
	if (channels == 0)
		alsa_channels = 2;

	/* If not specified */
	if (rate == 0)
		alsa_rate = 44100;

	/* Default render buffer size */
	alsa_buffer_size = 16384;

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

	ret = snd_pcm_hw_params_set_format(handle, hwParams, alsa_format);
	if (ret) {
		LOGE("Error setting output format");
		return BAD_VALUE;
	}

	ret = snd_pcm_hw_params_set_rate(handle, hwParams, alsa_rate, 0);
	if (ret) {
		LOGE("Error setting output sample rate %d", alsa_rate);
		return BAD_VALUE;
	}

	ret = snd_pcm_hw_params_set_channels(handle, hwParams, alsa_channels);
	if (ret) {
		LOGE("Error setting number of output channels");
		return BAD_VALUE;
	}

	ret = snd_pcm_hw_params_set_buffer_size(handle, hwParams, alsa_buffer_size);
	if (ret) {
		LOGE("Error setting buffer size");
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
int AudioStreamOutOmap::bytesPerFrame() const
{
	snd_pcm_format_t format;
	int bytesPerFrame = 0;
	int ret = 0;

	ret = snd_pcm_hw_params_get_format(hwParams, &format);
	if (ret) {
		LOGE("Error getting output format");
		return 0;
	}

	switch(format) {
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
	unsigned int val = 0;
	int dir = 0;
	int ret = 0;

	ret = snd_pcm_hw_params_get_rate(hwParams, &val, &dir);
	if (ret) {
		LOGE("Error getting output sample rate");
		return 0;
	}

	return val;
}

/* Get buffer size */
size_t AudioStreamOutOmap::bufferSize() const
{
	snd_pcm_uframes_t val;
	size_t bufferSize = 0;
	int ret = 0;

	ret = snd_pcm_hw_params_get_buffer_size(hwParams, &val);
	if (ret) {
		LOGE("Error getting output buffer size");
		return 0;
	}

	bufferSize = val * bytesPerFrame();

	return bufferSize;
}

/* Get number of channels */
int AudioStreamOutOmap::channelCount() const
{
	unsigned int val = 0;
	int ret = 0;

	ret = snd_pcm_hw_params_get_channels(hwParams, &val);
	if (ret) {
		LOGE("Error getting number of output channels");
		return 0;
	}

	return val;
}

/* Get pcm format */
int AudioStreamOutOmap::format() const
{
	snd_pcm_format_t format;
	int outFormat;
	int ret = 0;

	ret = snd_pcm_hw_params_get_format(hwParams, &format);
	if (ret) {
		LOGE("Error getting output format");
		return 0;
	}

	switch(format) {
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
	Mutex *outStream = new Mutex();
	int ret = 0;

	mAudioHardware->reconfigureHardware(AudioHardwareOmap::OUTPUT_STREAM);
	frames = bytes / bytesPerFrame();

	//Mutex::Autolock _l(mLock);
	outStream->lock();
	wFrames = snd_pcm_writei(pcmHandle, buffer, frames);
	outStream->unlock();
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

#endif

/*
 * AudioHardwareOmap.cpp
 *
 * Audio hardware interface for OMAP.
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

#ifndef _AUDIO_HARDWARE_OMAP_CPP_
#define _AUDIO_HARDWARE_OMAP_CPP_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define LOG_TAG "AudioHardwareOmap"
#include <utils/threads.h>
#include <utils/Log.h>
#include <utils/String8.h>

#include <hardware_legacy/AudioHardwareInterface.h>
#include <alsa/asoundlib.h>

#include "AudioHardwareOmap.h"
#include "AudioStreamInOmap.h"
#include "AudioStreamOutOmap.h"

using namespace android;

extern "C" AudioHardwareInterface* createAudioHardware(void) {
	return new AudioHardwareOmap();
}

/* Constructor */
AudioHardwareOmap::AudioHardwareOmap()
{
	mInput = 0;
	mOutput = 0;
	mMicMute = false;
	pcmName = strdup("default");
}

/* Destructor */
AudioHardwareOmap::~AudioHardwareOmap()
{
	delete mInput;
	delete mOutput;
}

/* Check if hardware interface has been initialized */
status_t AudioHardwareOmap::initCheck()
{
	return NO_ERROR;
}

status_t AudioHardwareOmap::setRouting(int mode, uint32_t routes)
{
	return NO_ERROR;
}

status_t AudioHardwareOmap::getRouting(int mode, uint32_t* routes)
{
	return NO_ERROR;
}

status_t AudioHardwareOmap::setMode(int mode)
{
	return NO_ERROR;
}

status_t AudioHardwareOmap::getMode(int* mode)
{
	return NO_ERROR;
}

status_t AudioHardwareOmap::setParameter(const char* key, const char* value)
{
	return NO_ERROR;
}

size_t AudioHardwareOmap::getInputBufferSize(uint32_t sampleRate, int format, int channelCount)
{
        /* Default capture buffer size */
	/* FIXME: Change to avoid a hardcoded value */
	return 1024;
}

/* Set audio volume for a voice call */
status_t AudioHardwareOmap::setVoiceVolume(float volume)
{
	return NO_ERROR;
}

/* Set audio volume for all audio activities other than voice calls */
status_t AudioHardwareOmap::setMasterVolume(float volume)
{
	return INVALID_OPERATION;
}

/* Set hardware mute */
status_t AudioHardwareOmap::setMicMute(bool state)
{
	mMicMute = state;
	return NO_ERROR;
}

/* Get current mute value */
status_t AudioHardwareOmap::getMicMute(bool* state)
{
	*state = mMicMute;
	return NO_ERROR;
}

/* Create and open audio hardware output stream */
AudioStreamOut* AudioHardwareOmap::openOutputStream(int format, int channelCount, uint32_t sampleRate, status_t *status)
{
	int ret = 0;
	*status = 0;

	AutoMutex lock(mLock);
	/* only one output stream allowed */
	if (mOutput) {
		return 0;
	}

	ret = snd_pcm_open(&pcmOutHandle, pcmName, SND_PCM_STREAM_PLAYBACK, 0);
	if (ret) {
		LOGE("Error opening PCM interface");
		return mOutput;
	}

	/* Create new output stream */
	AudioStreamOutOmap* out = new AudioStreamOutOmap();
	if (out->set(this, pcmOutHandle, format, channelCount, sampleRate) == NO_ERROR) {
		mOutput = out;
	} else {
		LOGW("Error setting output hardware parameters");
		delete out;
	}

	currentStream = AudioHardwareOmap::OUTPUT_STREAM;

	return mOutput;
}

/* Close output stream */
void AudioHardwareOmap::closeOutputStream(AudioStreamOut *out)
{
	if (out == mOutput) {
		snd_pcm_close(pcmOutHandle);
		mOutput = 0;
	}
}

/* Create and open audio hardware input stream */
AudioStreamIn* AudioHardwareOmap::openInputStream(int format, int channelCount, uint32_t sampleRate, status_t *status)
{
	int ret = 0;
	*status = 0;

	AutoMutex lock(mLock);
	/* only one output stream allowed */
	if (mInput) {
		return 0;
	}

	ret = snd_pcm_open(&pcmInHandle, pcmName, SND_PCM_STREAM_CAPTURE, 0);
	if (ret) {
		LOGE("Error opening input stream");
		return mInput;
	}

	/* Create new output stream */
	AudioStreamInOmap* in = new AudioStreamInOmap();
	if (in->set(this, pcmInHandle, format, channelCount, sampleRate) == NO_ERROR) {
		mInput = in;
	} else {
		LOGW("Error setting input hardware parameters");
		delete in;
	}

	currentStream = AudioHardwareOmap::INPUT_STREAM;

	return mInput;
}

/* Close input stream */
void AudioHardwareOmap::closeInputStream(AudioStreamIn* in)
{
	if (in == mInput) {
		snd_pcm_close(pcmInHandle);
		mInput = 0;
	}
}

/* Configure hardware to stream in/out */
status_t AudioHardwareOmap::reconfigureHardware(int stream)
{
	snd_pcm_state_t streamState;
	int ret = 0;

	if (currentStream == stream) {
		return NO_ERROR;
	}

	if (stream == AudioHardwareOmap::INPUT_STREAM) {
		snd_pcm_close(pcmInHandle);
		ret = snd_pcm_open(&pcmInHandle, pcmName, SND_PCM_STREAM_CAPTURE, 0);
		if (ret) {
			LOGE("Error opening PCM interface");
			return ret;
		}
		ret = mInput->set(this, pcmInHandle, 0, 0, 0);
		if (ret) {
			LOGW("Error setting input hardware parameters");
			return ret;
		}
	} else if (stream == AudioHardwareOmap::OUTPUT_STREAM) {
		snd_pcm_close(pcmOutHandle);
		ret = snd_pcm_open(&pcmOutHandle, pcmName, SND_PCM_STREAM_PLAYBACK, 0);
		if (ret) {
			LOGE("Error opening PCM interface");
			return ret;
		}
		ret = mOutput->set(this, pcmOutHandle, 0, 0, 0);
		if (ret) {
			LOGW("Error setting output hardware parameters");
			return ret;
		}
	}

	currentStream = stream;

	return NO_ERROR;
}

status_t AudioHardwareOmap::dumpState(int fd, const Vector<String16>& args)
{
	return NO_ERROR;
}

status_t AudioHardwareOmap::doRouting()
{
	return NO_ERROR;
}

status_t AudioHardwareOmap::dump(int fd, const Vector<String16>& args)
{
	if (mInput) {
		mInput->dump(fd, args);
	}
	if (mOutput) {
		mOutput->dump(fd, args);
	}

	return NO_ERROR;
}

#endif

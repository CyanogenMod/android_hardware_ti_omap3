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
#include <string.h>

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

struct mixer_volume_control {
	const char *ctl;
	long min;
	long max;
};

static mixer_volume_control twl4030_master_vol_control[2] = {
/* Output volume */
{"DAC2 Analog", 0, 0},
/* Input volume */
{"Analog", 0, 0},
};

extern "C" AudioHardwareInterface* createAudioHardware(void)
{
	return new AudioHardwareOmap();
}

/* Constructor */
AudioHardwareOmap::AudioHardwareOmap()
{
	mInput = 0;
	mOutput = 0;
	mMicMute = false;
	memset(&mRoutes, 0, sizeof(mRoutes));
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
	if (mode == AudioSystem::MODE_CURRENT)
		mode = mMode;

	if ((mode < 0) || (mode >= AudioSystem::NUM_MODES))
		return BAD_VALUE;

	uint32_t old = mRoutes[mode];
	mRoutes[mode] = routes;
	if ((mode != mMode) || (old == routes))
		return NO_ERROR;

	return doRouting();
}

status_t AudioHardwareOmap::getRouting(int mode, uint32_t* routes)
{
	if (mode == AudioSystem::MODE_CURRENT)
		mode = mMode;

	if ((mode < 0) || (mode >= AudioSystem::NUM_MODES))
		return BAD_VALUE;

	*routes = mRoutes[mode];

	return NO_ERROR;
}

status_t AudioHardwareOmap::setMode(int mode)
{
	if ((mode < 0) || (mode >= AudioSystem::NUM_MODES))
		return BAD_VALUE;

	if (mMode == mode)
		return NO_ERROR;

	mMode = mode;

	return doRouting();
}

status_t AudioHardwareOmap::getMode(int* mode)
{
	*mode = mMode;
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
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;
	char *device = strdup("default");
        const char *elem_name;
        long vol, min, max;
	status_t changed = NO_ERROR;
	int ret = 0;

	ret = snd_mixer_open(&handle, 0);
	if (ret < 0) {
		LOGE("Mixer open error: %s", snd_strerror(ret));
		return BAD_VALUE;
	}

	ret = snd_mixer_attach(handle, device);
	if (ret < 0) {
		LOGE("Mixer attach error: %s", snd_strerror(ret));
		snd_mixer_close(handle);
		return BAD_VALUE;
	}

	ret = snd_mixer_selem_register(handle, NULL, NULL);
	if (ret < 0) {
		LOGE("Mixer register error: %s", snd_strerror(ret));
		snd_mixer_close(handle);
		return BAD_VALUE;
	}

	ret = snd_mixer_load(handle);
	if (ret < 0) {
		LOGE("Mixer load error: %s", snd_strerror(ret));
		snd_mixer_close(handle);
		return BAD_VALUE;
	}

	/* Iterate over all mixer controls */
	for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
		if (!snd_mixer_selem_is_active(elem))
			continue;

		snd_mixer_selem_id_alloca(&sid);
		snd_mixer_selem_get_id(elem, sid);
		elem_name = snd_mixer_selem_id_get_name(sid);
	
		if (!strcmp(twl4030_master_vol_control[0].ctl, elem_name)) {
			snd_mixer_selem_get_playback_dB_range(elem, &min, &max);
			LOGI("Setting mixer control: %s to %.2f", elem_name, volume);
			vol = (long)((max - min)* volume) + min;
			ret = snd_mixer_selem_set_playback_dB_all(elem, vol, 0);
		} else if (!strcmp(twl4030_master_vol_control[1].ctl, elem_name)) {
			snd_mixer_selem_get_capture_dB_range(elem, &min, &max);
			LOGI("Setting mixer control: %s to %.2f", elem_name, volume);
			vol = (long)((max - min)* volume) + min;
			ret = snd_mixer_selem_set_capture_dB_all(elem, vol, 0);
		}
		if (ret < 0) {
			LOGE("Master volume setting error: %s", snd_strerror(ret));
			changed = BAD_VALUE;
			break;
		}
	}

	snd_mixer_close(handle);

	return changed;
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
       if (mOutput)
		return 0;

	/* Create new output stream */
	AudioStreamOutOmap* out = new AudioStreamOutOmap(this);
	if (out->set(format, channelCount, sampleRate, 2048 * 4) == NO_ERROR) {
		mOutput = out;
	} else {
		LOGW("Error setting output hardware parameters");
		delete out;
	}

	return mOutput;
}

/* Create and open audio hardware input stream */
AudioStreamIn* AudioHardwareOmap::openInputStream(int format,
                                       int channelCount,
                                       uint32_t sampleRate,
                                       status_t *status,
                                       AudioSystem::audio_in_acoustics acoustics)
{
	int ret = 0;
	*status = 0;

	AutoMutex lock(mLock);
	/* only one output stream allowed */
       if (mInput)
		return 0;

	/* Create new output stream */
	AudioStreamInOmap* in = new AudioStreamInOmap(this);
       if (in->set(format, channelCount, sampleRate, 2048 * 2) == NO_ERROR) {
		mInput = in;
	} else {
		LOGW("Error setting input hardware parameters");
		delete in;
	}

	return mInput;
}

status_t AudioHardwareOmap::dumpState(int fd, const Vector<String16>& args)
{
	return NO_ERROR;
}

status_t AudioHardwareOmap::doRouting()
{
	snd_mixer_t *handle;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_t *sid;
	char *device = strdup("default");
	uint32_t routes = mRoutes[mMode];
	status_t routed;
	int ret;

	ret = snd_mixer_open(&handle, 0);
	if (ret < 0) {
		LOGE("Mixer open error: %s", snd_strerror(ret));
		return BAD_VALUE;
	}

	ret = snd_mixer_attach(handle, device);
	if (ret < 0) {
		LOGE("Mixer attach error: %s", snd_strerror(ret));
		snd_mixer_close(handle);
		return BAD_VALUE;
	}

	ret = snd_mixer_selem_register(handle, NULL, NULL);
	if (ret < 0) {
		LOGE("Mixer register error: %s", snd_strerror(ret));
		snd_mixer_close(handle);
		return BAD_VALUE;
	}

	ret = snd_mixer_load(handle);
	if (ret < 0) {
		LOGE("Mixer load error: %s", snd_strerror(ret));
		snd_mixer_close(handle);
		return BAD_VALUE;
	}

	snd_mixer_selem_id_alloca(&sid);

	/* Iterate over all mixer controls */
	for (elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem)) {
		if (!snd_mixer_selem_is_active(elem))
			continue;

		snd_mixer_selem_get_id(elem, sid);

		/* Earpiece */
		if (!strcmp(snd_mixer_selem_id_get_name(sid), "Earpiece Mixer AudioL2")) {
			if (routes & AudioSystem::ROUTE_EARPIECE)
				ret = setSwitchItem(elem, true);
			else
				ret = setSwitchItem(elem, false);
			if (ret != NO_ERROR) {
				LOGE("Earpiece routing error");
				snd_mixer_close(handle);
				return ret;
			}
		}

		/* Speaker */
		if (!strcmp(snd_mixer_selem_id_get_name(sid), "HandsfreeL Mux")) {
			if (routes & AudioSystem::ROUTE_SPEAKER)
				ret = setEnumeratedItem(elem, "AudioL2");
			else
				ret = setEnumeratedItem(elem, "AudioL1");
			if (ret != NO_ERROR) {
				LOGE("Speaker left routing error");
				snd_mixer_close(handle);
				return ret;
			}
		}
		if (!strcmp(snd_mixer_selem_id_get_name(sid), "HandsfreeL")) {
			if (routes & AudioSystem::ROUTE_SPEAKER)
				ret = setSwitchItem(elem, true);
			else
				ret = setSwitchItem(elem, false);
			if (ret != NO_ERROR) {
				LOGE("Speaker left switch setting error");
				snd_mixer_close(handle);
				return ret;
			}
		}
		if (!strcmp(snd_mixer_selem_id_get_name(sid), "HandsfreeR Mux")) {
			if (routes & AudioSystem::ROUTE_SPEAKER)
				ret = setEnumeratedItem(elem, "AudioR2");
			else
				ret = setEnumeratedItem(elem, "AudioL1");
			if (ret != NO_ERROR) {
				LOGE("Speaker right routing error");
				snd_mixer_close(handle);
				return ret;
			}
		}
		if (!strcmp(snd_mixer_selem_id_get_name(sid), "HandsfreeR")) {
			if (routes & AudioSystem::ROUTE_SPEAKER)
				ret = setSwitchItem(elem, true);
			else
				ret = setSwitchItem(elem, false);
			if (ret != NO_ERROR) {
				LOGE("Speaker right switch setting error");
				snd_mixer_close(handle);
				return ret;
			}
		}

		/* Headset */
		if (!strcmp(snd_mixer_selem_id_get_name(sid), "HeadsetL Mixer AudioL2")) {
			if (routes & AudioSystem::ROUTE_HEADSET)
				ret = setSwitchItem(elem, true);
			else
				ret = setSwitchItem(elem, false);
			if (ret != NO_ERROR) {
				LOGE("Headset left routing error");
				snd_mixer_close(handle);
				return ret;
			}
		}
		if (!strcmp(snd_mixer_selem_id_get_name(sid), "HeadsetR Mixer AudioR2")) {
			if (routes & AudioSystem::ROUTE_HEADSET)
				ret = setSwitchItem(elem, true);
			else
				ret = setSwitchItem(elem, false);
			if (ret != NO_ERROR) {
				LOGE("Headset right routing error");
				snd_mixer_close(handle);
				return ret;
			}
		}

		/* Microphone */
		if (!strcmp(snd_mixer_selem_id_get_name(sid), "Analog Left")) {
			ret = setEnumeratedItem(elem, "Main mic");
			if (ret != NO_ERROR) {
				LOGE("Main microphone routing error");
				snd_mixer_close(handle);
				return ret;
			}
		}
		if (!strcmp(snd_mixer_selem_id_get_name(sid), "Analog Right")) {
			ret = setEnumeratedItem(elem, "Sub mic");
			if (ret != NO_ERROR) {
				LOGE("Sub microphone routing error");
				snd_mixer_close(handle);
				return ret;
			}
		}
	}

	snd_mixer_close(handle);

	return NO_ERROR;
}

status_t AudioHardwareOmap::setEnumeratedItem(snd_mixer_elem_t *elem, const char *selected_name)
{
	snd_mixer_selem_id_t *sid;
	char item_name[20];
	unsigned int i;
	status_t ret = NAME_NOT_FOUND;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_get_id(elem, sid);

	/* Mixer element is not an enumeration */
	if (!snd_mixer_selem_is_enumerated(elem)) {
		LOGE("Mixer element %s is not an enumerated control", snd_mixer_selem_id_get_name(sid));
		return BAD_TYPE;
	}

	/* Search for matching item name */
	for (i = 0; i < (unsigned int) snd_mixer_selem_get_enum_items(elem); i++) {
		snd_mixer_selem_get_enum_item_name(elem, i, 20, item_name);
		if (!strcmp(selected_name, item_name)) {
			ret = snd_mixer_selem_set_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT, i);
			LOGI("%s: %s", snd_mixer_selem_id_get_name(sid), selected_name);
			break;
		}
	}

	return ret;
}

status_t AudioHardwareOmap::setEnumeratedItem(snd_mixer_elem_t *elem, int index)
{
	snd_mixer_selem_id_t *sid;
	char item_name[20];
	unsigned int i;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_get_id(elem, sid);

	/* Mixer element is not an anumeration */
	if (!snd_mixer_selem_is_enumerated(elem)) {
		LOGE("Mixer element %s is not an enumerated control", snd_mixer_selem_id_get_name(sid));
		return BAD_TYPE;
	}

	/* Not a valid index */
	if (snd_mixer_selem_get_enum_items(elem) > index) {
		LOGE("Mixer element %s has no index %d", snd_mixer_selem_id_get_name(sid), index);
		return BAD_VALUE;
	}

	snd_mixer_selem_set_enum_item(elem, SND_MIXER_SCHN_FRONT_LEFT, index);
	snd_mixer_selem_get_enum_item_name(elem, index, 20, item_name);
	LOGI("%s: %s (%d)", snd_mixer_selem_id_get_name(sid), item_name, index);

	return NO_ERROR;
}

status_t AudioHardwareOmap::setSwitchItem(snd_mixer_elem_t *elem, bool enable)
{
	snd_mixer_selem_id_t *sid;
	status_t ret;

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_get_id(elem, sid);

	if (snd_mixer_selem_has_playback_switch(elem)) {
		ret = snd_mixer_selem_set_playback_switch_all(elem, (int) enable);
		LOGI("%s: %d", snd_mixer_selem_id_get_name(sid), (int)enable);
	} else if (snd_mixer_selem_has_capture_switch(elem)) {
		ret = snd_mixer_selem_set_capture_switch_all(elem, (int) enable);
		LOGI("%s: %d", snd_mixer_selem_id_get_name(sid), (int)enable);
	} else {
		LOGE("Mixer element %s has no switch control", snd_mixer_selem_id_get_name(sid));
		return BAD_TYPE;
	}

	return ret;
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

void AudioHardwareOmap::setInputStream(AudioStreamInOmap *input)
{
	mInput = input;
}

void AudioHardwareOmap::setOutputStream(AudioStreamOutOmap *output)
{
	mOutput = output;
}

#endif

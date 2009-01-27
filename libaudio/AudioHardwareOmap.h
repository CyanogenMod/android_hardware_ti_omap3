/*
 * AudioHardwareOmap.h
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

#ifndef _AUDIO_HARDWARE_OMAP_H_
#define _AUDIO_HARDWARE_OMAP_H_

#include <hardware_legacy/AudioHardwareInterface.h>
#include <alsa/asoundlib.h>

#include "AudioStreamInOmap.h"
#include "AudioStreamOutOmap.h"

using namespace android;



class AudioHardwareOmap : public AudioHardwareInterface
{
	public:
		AudioHardwareOmap();
               ~AudioHardwareOmap();
		virtual status_t initCheck();
		virtual status_t setRouting(int mode, uint32_t routes);
		virtual status_t getRouting(int mode, uint32_t* routes);
		virtual status_t setMode(int mode);
		virtual status_t getMode(int* mode);
		virtual status_t setParameter(const char* key, const char* value);
		virtual size_t getInputBufferSize(uint32_t sampleRate, int format, int channelCount);
		virtual status_t setVoiceVolume(float volume);
		virtual status_t setMasterVolume(float volume);
		virtual status_t setMicMute(bool state);
		virtual status_t getMicMute(bool* state);
		virtual AudioStreamOut* openOutputStream(int format=0, int channelCount=0, uint32_t sampleRate=0, status_t *status=0);
		virtual AudioStreamIn* openInputStream(int format, int channelCount, uint32_t sampleRate, status_t *status);
		virtual status_t dumpState(int fd, const Vector<String16>& args);
		static AudioHardwareInterface* create();
	protected:
		virtual status_t doRouting();
		virtual status_t dump(int fd, const Vector<String16>& args);
		int mMode;
		uint32_t mRoutes[AudioSystem::NUM_MODES];
	private:
		Mutex mLock;
		AudioStreamInOmap *mInput;
		AudioStreamOutOmap *mOutput;
		bool mMicMute;
};

#endif

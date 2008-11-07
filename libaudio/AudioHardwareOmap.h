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

#include <hardware/AudioHardwareInterface.h>

#include <alsa/asoundlib.h>

using namespace android;

class AudioHardwareOmap : public AudioHardwareInterface
{
	public:
		AudioHardwareOmap();
		virtual ~AudioHardwareOmap();
		virtual status_t initCheck();
		virtual status_t standby();
		virtual status_t setVoiceVolume(float volume);
		virtual status_t setMasterVolume(float volume);
		virtual status_t setMicMute(bool state);
		virtual status_t getMicMute(bool* state);
		virtual AudioStreamOut* openOutputStream(int format, int channelCount, uint32_t sampleRate);
		void closeOutputStream(AudioStreamOut *out);
		virtual AudioStreamIn* openInputStream(int format, int channelCount, uint32_t sampleRate);
		void closeInputStream(AudioStreamIn* in);
		static AudioHardwareInterface* create();
	protected:
		virtual status_t doRouting();
		virtual status_t dump(int fd, const Vector<String16>& args);
		int mMode;
		uint32_t mRoutes[AudioSystem::NUM_MODES];
	private:
		Mutex mLock;
		AudioStreamOut *mOutput;
		AudioStreamIn *mInput;
		bool mMicMute;
		/* ALSA specific */
		char *pcmName;
		snd_pcm_t *pcmInHandle;
		snd_pcm_t *pcmOutHandle;
};

#endif

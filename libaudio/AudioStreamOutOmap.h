/*
 * AudioStreamOutOmap.h
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

#ifndef _AUDIO_STREAM_OUT_OMAP_H_
#define _AUDIO_STREAM_OUT_OMAP_H_

#include <hardware/AudioHardwareInterface.h>
#include <alsa/asoundlib.h>

using namespace android;

class AudioStreamOutOmap : public AudioStreamOut
{
       public:
               AudioStreamOutOmap();
               virtual ~AudioStreamOutOmap();
               virtual status_t set(AudioHardwareOmap *hw, snd_pcm_t *handle,
                                       int format, int channels, uint32_t rate);
               int bytesPerFrame() const;
               virtual uint32_t sampleRate() const;
               virtual size_t bufferSize() const;
               virtual int channelCount() const;
               virtual int format() const;
               virtual uint32_t latency() const;
               virtual status_t setVolume(float volume);
               virtual ssize_t write(const void* buffer, size_t bytes);
               virtual status_t dump(int fd, const Vector<String16>& args);
       private:
               AudioHardwareOmap *mAudioHardware;
               snd_pcm_hw_params_t *hwParams;
               snd_pcm_t *pcmHandle;
               Mutex mLock;
};

#endif

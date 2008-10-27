#ifndef _AUDIO_STREAM_IN_OMAP_H_
#define _AUDIO_STREAM_IN_OMAP_H_

#include <hardware/AudioHardwareInterface.h>
#include "AudioHardwareOmap.h"
#include <alsa/asoundlib.h>

using namespace android;

class AudioStreamInOmap : public AudioStreamIn
{
       public:
               AudioStreamInOmap();
               virtual  ~AudioStreamInOmap();
               virtual status_t set(AudioHardwareOmap *hw, snd_pcm_t *handle,
                                       int format, int channels, uint32_t rate);
               int bytesPerFrame() const;
               uint32_t sampleRate() const;
               virtual size_t bufferSize() const;
               virtual int channelCount() const;
               virtual int format() const;
               virtual status_t setGain(float gain);
               virtual ssize_t read(void* buffer, ssize_t bytes);
               virtual status_t dump(int fd, const Vector<String16>& args);
       private:
               AudioHardwareOmap *mAudioHardware;
               snd_pcm_hw_params_t *hwParams;
               snd_pcm_t *pcmHandle;
               Mutex mLock;
};

#endif

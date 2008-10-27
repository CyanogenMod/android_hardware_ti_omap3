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

#include <hardware/AudioHardwareInterface.h>
#include <alsa/asoundlib.h>

#include "AudioHardwareOmap.h"
#include "AudioStreamInOmap.h"
#include "AudioStreamOutOmap.h"

using namespace android;

AudioHardwareInterface* createAudioHardware(void)
{
       return 0;
}

AudioHardwareInterface::AudioHardwareInterface()
{
       memset(&mRoutes, 0, sizeof(mRoutes));
       mMode = 0;
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

/* Put the audio hardware into standby mode to conserve power */
status_t AudioHardwareOmap::standby()
{
       return NO_ERROR;
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

/* Set routes for a mode */
status_t AudioHardwareInterface::setRouting(int mode, uint32_t routes)
{
       uint32_t old = 0;

       if ((mode < 0) || (mode >= AudioSystem::NUM_MODES)) {
               return BAD_VALUE;
       }
       if (mode == AudioSystem::MODE_CURRENT) {
               mode = mMode;
       }
       old = mRoutes[mode];
       mRoutes[mode] = routes;
       if ((mode != mMode) || (old == routes)) {
               return NO_ERROR;
       }

       return doRouting();
}

/* Get routes */
status_t AudioHardwareInterface::getRouting(int mode, uint32_t* routes)
{
       if ((mode < 0) || (mode >= AudioSystem::NUM_MODES)) {
               return BAD_VALUE;
               }
       if (mode == AudioSystem::MODE_CURRENT) {
               mode = mMode;
       }
       *routes = mRoutes[mode];

       return NO_ERROR;
}

/* Set mode when audio mode changes */
status_t AudioHardwareInterface::setMode(int mode)
{
       if ((mode < 0) || (mode >= AudioSystem::NUM_MODES)) {
               return BAD_VALUE;
       }
       if (mMode == mode) {
               return NO_ERROR;
       }
       mMode = mode;
       return doRouting();
}

/* Get current audio mode */
status_t AudioHardwareInterface::getMode(int* mode)
{
       *mode = mMode;
       return NO_ERROR;
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

//status_t AudioHardwareOmap::setParameter(const char* key, const char* value)
status_t AudioHardwareInterface::setParameter(const char* key, const char* value)
{
       return NO_ERROR;
}

/* Create and open audio hardware output stream */
AudioStreamOut* AudioHardwareOmap::openOutputStream(int format, int channelCount, uint32_t sampleRate)
{
       int ret = 0;

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
AudioStreamIn* AudioHardwareOmap::openInputStream(int format, int channelCount, uint32_t sampleRate)
{
       int ret = 0;

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

/* Dump the current state of audio hardware */
//status_t AudioHardwareOmap::dumpState(int fd, const Vector<String16>& args)
status_t AudioHardwareInterface::dumpState(int fd, const Vector<String16>& args)
{
       return NO_ERROR;
}

/* Create an instance of AudioHardwareOmap */
AudioHardwareInterface* AudioHardwareInterface::create()
{
       AudioHardwareInterface* hw = new AudioHardwareOmap();
       if (hw->initCheck() != NO_ERROR) {
               LOGE("Error checking hardware initialization");
               delete hw;
       }

       return hw;
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

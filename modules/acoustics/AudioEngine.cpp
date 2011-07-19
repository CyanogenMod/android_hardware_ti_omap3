/* AudioEngine.cpp
 **
 ** Copyright 2009-2011 Texas Instruments
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>

#define LOG_TAG "AudioEngine"
#include <utils/threads.h>
#include <utils/Log.h>
#include <utils/String8.h>

#include <AudioEngine.h>

#define BUFFER_COUNT        20
#define AUDIO_CHUNK_SIZE    2048
#define RING_SIZE           BUFFER_COUNT * AUDIO_CHUNK_SIZE

namespace android
{

AudioEngine::AudioEngine(acoustic_device_t *dev) :
    mDev(dev),
    mInput(0),
    running(false),
    readRng(RING_SIZE),
    writeRng(RING_SIZE)
{
    sourceBuffer = new char[AUDIO_CHUNK_SIZE];
    procBuffer = new char[AUDIO_CHUNK_SIZE];
    refBuffer = new char[RING_SIZE];
    feedback = new char[AUDIO_CHUNK_SIZE];

    memset(sourceBuffer, 0, AUDIO_CHUNK_SIZE);
    memset(procBuffer, 0, AUDIO_CHUNK_SIZE);
    memset(refBuffer, 0, RING_SIZE);
    memset(feedback, 0, AUDIO_CHUNK_SIZE);
}

AudioEngine::~AudioEngine()
{
    delete sourceBuffer;
    delete procBuffer;
    delete refBuffer;
    delete feedback;
}

bool AudioEngine::isRunning()
{
    return running;
}

bool AudioEngine::threadLoop()
{
    running = true;

    uint32_t bytes = AUDIO_CHUNK_SIZE;
    snd_pcm_sframes_t n, frames = snd_pcm_bytes_to_frames(mInput->handle, bytes);

    do {
        char *buffer = sourceBuffer;
        do {
            n = snd_pcm_readi(mInput->handle, buffer, frames);
            if (n < frames && n != -EAGAIN) {
                if (n < 0)
                    recover(n);
                else {
                    frames -= n;
                    buffer += snd_pcm_frames_to_bytes(mInput->handle, n);
                    snd_pcm_prepare(mInput->handle);
                }
            }
        } while (frames);

        bytes = writeRng.copy(refBuffer, RING_SIZE);
        // If we didn't get enough, then fill with silence.
        if (bytes < RING_SIZE)
            memset(refBuffer + bytes, 0, RING_SIZE - bytes);

        process_data();

        readRng.write(procBuffer, AUDIO_CHUNK_SIZE);

    } while (!exitPending());

    running = false;

    return false;
}

status_t AudioEngine::readyToRun()
{
    return NO_ERROR;
}

void AudioEngine::onFirstRef()
{
}

status_t AudioEngine::setHandle(alsa_handle_t *handle)
{
    mInput = handle;
    return NO_ERROR;
}

status_t AudioEngine::cleanup()
{
    return NO_ERROR;
}

ssize_t AudioEngine::read(void* buffer, ssize_t bytes)
{
    return readRng.read(buffer, bytes);
}

ssize_t AudioEngine::write(const void *buffer, size_t bytes)
{
    // Don't bother saving if we have no input.
    if (!mInput)
        return bytes;

    return writeRng.write(buffer, bytes);
}

status_t AudioEngine::recover(int err)
{
    err = mInput ? snd_pcm_recover(mInput->handle, err, 0) : 0;

    readRng.reset();
    writeRng.reset();

    return err;
}

void AudioEngine::process_data()
{
    snd_pcm_sframes_t frames = snd_pcm_bytes_to_frames(mInput->handle, AUDIO_CHUNK_SIZE);

    // At this point:
    //      sourceBuffer contains AUDIO_CHUNK_SIZE bytes of captured audio
    //      procBuffer is expected to be filled with AUDIO_CHUNK_SIZE processed capture data
    //      refBuffer contains RING_SIZE of playback audio that was sent out
    //      feedback is a scratch buffer for the audio processing.
    //      frames is the number of samples in the procBuffer.

    //process_rx(sourceBuffer, procBuffer, refBuffer, feedback, frames);
}

}

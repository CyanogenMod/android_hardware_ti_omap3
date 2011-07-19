/* AudioEngine.h
 **
 ** Copyright 2009-2011, Texas Instruments
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

#ifndef ANDROID_HARDWARE_TI_AUDIO_ENGINE_H
#define ANDROID_HARDWARE_TI_AUDIO_ENGINE_H

#include <AudioHardwareALSA.h>
#include <RingBuffer.h>

namespace android
{

class AudioEngine: public Thread {
public:
    AudioEngine(acoustic_device_t *dev);
    ~AudioEngine();

    // Thread virtual methods
    virtual bool threadLoop();
    virtual status_t readyToRun();
    virtual void onFirstRef();

    bool isRunning();

    status_t setHandle(alsa_handle_t *handle);
    status_t cleanup();

    ssize_t read(void* buffer, ssize_t bytes);
    ssize_t write(const void *buffer, size_t bytes);
    status_t recover(int);

private:
    acoustic_device_t * mDev;
    alsa_handle_t *     mInput;

    bool                running;
    RingBuffer          readRng;
    RingBuffer          writeRng;

    char *              sourceBuffer;
    char *              procBuffer;
    char *              refBuffer;
    char *              feedback;

    void process_data();
};

}
#endif

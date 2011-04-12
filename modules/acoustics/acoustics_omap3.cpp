/* acoustics_omap3.cpp
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

#define LOG_TAG "Omap3Acoustics"
#include <utils/Log.h>

#include "AudioHardwareALSA.h"
#include "AudioEngine.h"

namespace android
{

static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);

static hw_module_methods_t s_module_methods = {
    open            : s_device_open
};

extern "C" const hw_module_t HAL_MODULE_INFO_SYM = {
    tag             : HARDWARE_MODULE_TAG,
    version_major   : 1,
    version_minor   : 0,
    id              : ACOUSTICS_HARDWARE_MODULE_ID,
    name            : "Omap3 acoustics module",
    author          : "TI",
    methods         : &s_module_methods,
    dso             : 0,
    reserved        : { 0, },
};

static status_t s_use_handle(acoustic_device_t *, alsa_handle_t *);
static status_t s_cleanup(acoustic_device_t *);
static status_t s_set_params(acoustic_device_t *,
        AudioSystem::audio_in_acoustics, void *params);
static ssize_t s_read(acoustic_device_t *, void *buffer, size_t bytes);
static ssize_t s_write(acoustic_device_t *, const void *buffer, size_t bytes);
static status_t s_recover(acoustic_device_t *dev, int);

static acoustic_device_t *dev = 0;
static int refs = 0;

static int s_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    if (!dev) {
        dev = (acoustic_device_t *) malloc(sizeof(*dev));
        if (!dev) return -ENOMEM;

        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->common.tag = HARDWARE_DEVICE_TAG;
        dev->common.version = 0;
        dev->common.module = (hw_module_t *) module;
        dev->common.close = s_device_close;

        // Required methods...
        dev->use_handle = s_use_handle;
        dev->cleanup = s_cleanup;
        dev->set_params = s_set_params;

        // read, write, and recover are optional methods...
        dev->read = s_read;
        dev->write = s_write;
        dev->recover = s_recover;

        AudioEngine *engine = new AudioEngine(dev);
        dev->modPrivate = engine;
    }

    *device = (hw_device_t *) dev;
    refs++;
    return 0;
}

static int s_device_close(hw_device_t* device)
{
    if (device != (hw_device_t *) dev) return BAD_VALUE;
    if (--refs == 0) free(device);

    return 0;
}

static status_t s_use_handle(acoustic_device_t *dev, alsa_handle_t *h)
{
    AudioEngine *engine = (AudioEngine *)dev->modPrivate;

    return engine ? engine->setHandle(h) : (status_t)NO_ERROR;
}

static status_t s_cleanup(acoustic_device_t *dev)
{
    AudioEngine *engine = (AudioEngine *)dev->modPrivate;

    return engine ? engine->cleanup() : (status_t)NO_ERROR;
}

static status_t s_set_params(acoustic_device_t *dev,
        AudioSystem::audio_in_acoustics acoustics, void *params)
{
    return NO_ERROR;
}

static ssize_t s_read(acoustic_device_t *dev, void *buffer, size_t bytes)
{
    AudioEngine *engine = (AudioEngine *)dev->modPrivate;

    return engine ? engine->read(buffer, bytes) : 0;
}

static ssize_t s_write(acoustic_device_t *dev, const void *buffer, size_t bytes)
{
    AudioEngine *engine = (AudioEngine *)dev->modPrivate;

    return engine ? engine->write(buffer, bytes) : 0;
}

static status_t s_recover(acoustic_device_t *dev, int err)
{
    AudioEngine *engine = (AudioEngine *)dev->modPrivate;

    return engine ? engine->recover(err) : (status_t)NO_ERROR;
}

}

/*
 * Copyright (C) 2011 Texas Instruments
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "TIHardwareRenderer.h"

#include <media/stagefright/HardwareAPI.h>
#include <dlfcn.h>

using android::sp;
using android::ISurface;
using android::VideoRenderer;

VideoRenderer *createRendererWithRotation(
        const sp<ISurface> &surface,
        const char *componentName,
        OMX_COLOR_FORMATTYPE colorFormat,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight,
        int32_t rotationDegrees) {
    using android::TIHardwareRenderer;

    if (!strncmp(componentName, "drm.play.", 9)) {
        void * mLibDrmRenderHandle;
        bool (*mDrmPlaySetVideoRenderer) (android::TIHardwareRenderer *);
        mLibDrmRenderHandle = dlopen("libdrmplay.so", RTLD_NOW);
        if  (mLibDrmRenderHandle != NULL) {
            TIHardwareRenderer *renderer =
                new TIHardwareRenderer(
                    surface, displayWidth, displayHeight,
                    decodedWidth, decodedHeight,
                    (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420PackedSemiPlanar, //colorFormat is not provided by client
                    rotationDegrees);

            mDrmPlaySetVideoRenderer = (bool (*)(android::TIHardwareRenderer *))dlsym(mLibDrmRenderHandle, "_Z23DrmPlaySetVideoRendererPN7android18TIHardwareRendererE");

            (*mDrmPlaySetVideoRenderer)(renderer);

            dlclose(mLibDrmRenderHandle);
            mLibDrmRenderHandle = NULL;
            mDrmPlaySetVideoRenderer = NULL;
            return renderer;
        }
    }

    TIHardwareRenderer *renderer =
        new TIHardwareRenderer(
                surface, displayWidth, displayHeight,
                decodedWidth, decodedHeight,
                colorFormat,
                rotationDegrees);

    if (renderer->initCheck() != android::OK) {
        delete renderer;
        renderer = NULL;
    }

    return renderer;
}

//remains for backward compatibility
VideoRenderer *createRenderer(
        const sp<ISurface> &surface,
        const char *componentName,
        OMX_COLOR_FORMATTYPE colorFormat,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight) {
    return createRendererWithRotation(
            surface, componentName, colorFormat,
            displayWidth, displayHeight,
            decodedWidth, decodedHeight,
            0);
}

//S3D
VideoRenderer *createRendererWithRotation(
        const sp<ISurface> &surface,
        const char *componentName,
        OMX_COLOR_FORMATTYPE colorFormat,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight,
        int32_t rotationDegrees,
        int isS3D, int numOfOpBuffers) {
    using android::TIHardwareRenderer;

    TIHardwareRenderer *renderer =
        new TIHardwareRenderer(
                surface, displayWidth, displayHeight,
                decodedWidth, decodedHeight,
                colorFormat,
                rotationDegrees,
                isS3D, numOfOpBuffers);
    if (renderer->initCheck() != android::OK) {
        delete renderer;
        renderer = NULL;
    }

    return renderer;
}

//remains for backward compatibility
VideoRenderer *createRenderer(
        const sp<ISurface> &surface,
        const char *componentName,
        OMX_COLOR_FORMATTYPE colorFormat,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight,
        int isS3D, int numOfOpBuffers) {
    return createRendererWithRotation(
            surface, componentName, colorFormat,
            displayWidth, displayHeight,
            decodedWidth, decodedHeight,
            0, isS3D, numOfOpBuffers);
}

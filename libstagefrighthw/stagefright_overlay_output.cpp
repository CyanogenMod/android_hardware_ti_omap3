#include "TIHardwareRenderer.h"

#include <media/stagefright/HardwareAPI.h>

using android::sp;
using android::ISurface;
using android::VideoRenderer;

VideoRenderer *createRenderer(
        const sp<ISurface> &surface,
        const char *componentName,
        OMX_COLOR_FORMATTYPE colorFormat,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight) {
    using android::TIHardwareRenderer;

    TIHardwareRenderer *renderer =
        new TIHardwareRenderer(
                surface, displayWidth, displayHeight,
                decodedWidth, decodedHeight,
                colorFormat);

    if (renderer->initCheck() != android::OK) {
        delete renderer;
        renderer = NULL;
    }

    return renderer;
}

//S3D
VideoRenderer *createRenderer(
        const sp<ISurface> &surface,
        const char *componentName,
        OMX_COLOR_FORMATTYPE colorFormat,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight, int isS3D) {
    using android::TIHardwareRenderer;
    TIHardwareRenderer *renderer =
        new TIHardwareRenderer(
                surface, displayWidth, displayHeight,
                decodedWidth, decodedHeight,
                colorFormat, isS3D);
    if (renderer->initCheck() != android::OK) {
        delete renderer;
        renderer = NULL;
    }

    return renderer;
}


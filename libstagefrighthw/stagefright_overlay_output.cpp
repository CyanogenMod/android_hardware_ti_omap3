#include "TIHardwareRenderer.h"

#include <media/stagefright/HardwareAPI.h>

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

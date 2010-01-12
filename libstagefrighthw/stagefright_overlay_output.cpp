#include <media/stagefright/HardwareAPI.h>

#include "TIHardwareRenderer.h"

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

    if (colorFormat == OMX_COLOR_FormatCbYCrY
            && !strcmp(componentName, "OMX.TI.Video.Decoder")) {
        return new TIHardwareRenderer(
                surface, displayWidth, displayHeight,
                decodedWidth, decodedHeight);
    }

    return NULL;
}


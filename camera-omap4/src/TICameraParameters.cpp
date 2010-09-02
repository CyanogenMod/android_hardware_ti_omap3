/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */



#define LOG_TAG "CameraParams"
#include <utils/Log.h>

#include <string.h>
#include <stdlib.h>
#include <TICameraParameters.h>
#include "CameraHal.h"

namespace android {

//TI extensions to camera mode
const char TICameraParameters::HIGH_PERFORMANCE_MODE[] = "high-performance";
const char TICameraParameters::HIGH_QUALITY_MODE[] = "high-quality";
const char TICameraParameters::VIDEO_MODE[] = "video-mode";

// TI extensions to standard android Parameters
const char TICameraParameters::KEY_SUPPORTED_CAMERAS[] = "camera-indexes";
const char TICameraParameters::KEY_CAMERA[] = "camera-index";
const char TICameraParameters::KEY_CAMERA_NAME[] = "camera-name";
const char TICameraParameters::KEY_BURST[] = "burst-capture";
const char TICameraParameters::KEY_CAP_MODE[] = "mode";
const char TICameraParameters::KEY_VSTAB[] = "vstab";
const char TICameraParameters::KEY_VNF[] = "vnf";
const char TICameraParameters::KEY_SATURATION[] = "saturation";
const char TICameraParameters::KEY_BRIGHTNESS[] = "brightness";
const char TICameraParameters::KEY_EXPOSURE_MODE[] = "exposure";
const char TICameraParameters::KEY_SUPPORTED_EXPOSURE[] = "exposure-mode-values";
const char TICameraParameters::KEY_CONTRAST[] = "contrast";
const char TICameraParameters::KEY_SHARPNESS[] = "sharpness";
const char TICameraParameters::KEY_ISO[] = "iso";
const char TICameraParameters::KEY_SUPPORTED_ISO_VALUES[] = "iso-mode-values";
const char TICameraParameters::KEY_SUPPORTED_IPP[] = "ipp-values";
const char TICameraParameters::KEY_IPP[] = "ipp";
const char TICameraParameters::KEY_MAN_EXPOSURE[] = "manual-exposure";
const char TICameraParameters::KEY_METERING_MODE[] = "meter-mode";

//TI extensions to Image post-processing
const char TICameraParameters::IPP_LDCNSF[] = "ldc-nsf";
const char TICameraParameters::IPP_LDC[] = "ldc";
const char TICameraParameters::IPP_NSF[] = "nsf";
const char TICameraParameters::IPP_NONE[] = "off";

// TI extensions to standard android pixel formats
const char TICameraParameters::PIXEL_FORMAT_RAW[] = "raw";

// TI extensions to standard android scene mode settings
const char TICameraParameters::SCENE_MODE_CLOSEUP[] = "closeup";
const char TICameraParameters::SCENE_MODE_AQUA[] = "aqua";
const char TICameraParameters::SCENE_MODE_SNOWBEACH[] = "snow-beach";
const char TICameraParameters::SCENE_MODE_MOOD[] = "mood";
const char TICameraParameters::SCENE_MODE_NIGHT_INDOOR[] = "night-indoor";
const char TICameraParameters::SCENE_MODE_DOCUMENT[] = "document";
const char TICameraParameters::SCENE_MODE_BARCODE[] = "barcode";
const char TICameraParameters::SCENE_MODE_VIDEO_SUPER_NIGHT[] = "super-night";
const char TICameraParameters::SCENE_MODE_VIDEO_CINE[] = "cine";
const char TICameraParameters::SCENE_MODE_VIDEO_OLD_FILM[] = "old-film";

// TI extensions to standard android white balance values.
const char TICameraParameters::WHITE_BALANCE_TUNGSTEN[] = "tungsten";
const char TICameraParameters::WHITE_BALANCE_HORIZON[] = "horizon";

// TI extensions to  standard android focus modes.
const char TICameraParameters::FOCUS_MODE_PORTRAIT[] = "portrait";
const char TICameraParameters::FOCUS_MODE_EXTENDED[] = "extended";
const char TICameraParameters::FOCUS_MODE_CAF[] = "caf";



//  TI extensions to add  values for effect settings.
const char TICameraParameters::EFFECT_NATURAL[] = "natural";
const char TICameraParameters::EFFECT_VIVID[] = "vivid";
const char TICameraParameters::EFFECT_COLOR_SWAP[] = "color-swap";

// TI extensions to add exposure preset modes
const char TICameraParameters::EXPOSURE_MODE_OFF[] = "off";
const char TICameraParameters::EXPOSURE_MODE_AUTO[] = "auto";
const char TICameraParameters::EXPOSURE_MODE_NIGHT[] = "night";
const char TICameraParameters::EXPOSURE_MODE_BACKLIGHT[] = "backlight";
const char TICameraParameters::EXPOSURE_MODE_SPOTLIGHT[] = "spotlight";
const char TICameraParameters::EXPOSURE_MODE_SPORTS[] = "sports";
const char TICameraParameters::EXPOSURE_MODE_SNOW[] = "snow";
const char TICameraParameters::EXPOSURE_MODE_BEACH[] = "beach";
const char TICameraParameters::EXPOSURE_MODE_APERTURE[] = "aperture";
const char TICameraParameters::EXPOSURE_MODE_SMALL_APERTURE[] = "small-aperture";


// TI extensions to add iso values
const char TICameraParameters::ISO_MODE_AUTO[] = "auto";
const char TICameraParameters::ISO_MODE_100[] = "100";
const char TICameraParameters::ISO_MODE_200[] = "200";
const char TICameraParameters::ISO_MODE_400[] = "400";
const char TICameraParameters::ISO_MODE_800[] = "800";
const char TICameraParameters::ISO_MODE_1000[] = "1000";
const char TICameraParameters::ISO_MODE_1200[] = "1200";
const char TICameraParameters::ISO_MODE_1600[] = "1600";
};


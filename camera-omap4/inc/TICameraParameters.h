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



#ifndef TI_CAMERA_PARAMETERS_H
#define TI_CAMERA_PARAMETERS_H

#include <utils/KeyedVector.h>
#include <utils/String8.h>

namespace android {

///TI Specific Camera Parameters
class TICameraParameters
{
public:

// Supported Camera indexes
// Example value: "0,1,2,3", where 0-primary, 1-secondary1, 2-secondary2, 3-sterocamera
static const  char KEY_SUPPORTED_CAMERAS[];
// Select logical Camera index
static const char KEY_CAMERA[];

static const char KEY_BURST[];
static const  char KEY_CAP_MODE[];
static const  char KEY_VSTAB[];
static const  char KEY_VNF[];
static const  char KEY_SATURATION[];
static const  char KEY_BRIGHTNESS[];
static const  char KEY_EXPOSURE[];
static const  char KEY_SUPPORTED_EXPOSURE[];
static const  char KEY_CONTRAST[];
static const  char KEY_SHARPNESS[];
static const  char KEY_ISO[];
static const  char KEY_SUPPORTED_ISO_VALUES[];
static const  char KEY_MAN_EXPOSURE[];
static const  char KEY_METERING_MODE[];


// TI extensions to standard android pixel formats
static const  char PIXEL_FORMAT_RAW[];

// TI extensions to standard android scene mode settings
static const  char SCENE_MODE_CLOSEUP[];
static const  char SCENE_MODE_AQUA[];
static const  char SCENE_MODE_SNOWBEACH[];
static const  char SCENE_MODE_MOOD[];
static const  char SCENE_MODE_NIGHT_INDOOR[];
static const  char SCENE_MODE_DOCUMENT[];
static const  char SCENE_MODE_BARCODE[];
static const  char SCENE_MODE_VIDEO_SUPER_NIGHT[];
static const  char SCENE_MODE_VIDEO_CINE[];
static const  char SCENE_MODE_VIDEO_OLD_FILM[];

// TI extensions to standard android white balance settings.
static const  char WHITE_BALANCE_TUNGSTEN[];
static const  char WHITE_BALANCE_HORIZON[];

// TI extensions to add exposure preset modes to android api
static const  char EXPOSURE_MODE_OFF[];
static const  char EXPOSURE_MODE_AUTO[];
static const  char EXPOSURE_MODE_NIGHT[];
static const  char EXPOSURE_MODE_BACKLIGHT[];
static const  char EXPOSURE_MODE_SPOTLIGHT[];
static const  char EXPOSURE_MODE_SPORTS[];
static const  char EXPOSURE_MODE_SNOW[];
static const  char EXPOSURE_MODE_BEACH[];
static const  char EXPOSURE_MODE_APERTURE[];
static const  char EXPOSURE_MODE_SMALL_APERTURE[];

// TI extensions to standard android focus presets.
static const  char FOCUS_MODE_PORTRAIT[];
static const  char FOCUS_MODE_EXTENDED[];
static const  char FOCUS_MODE_CAF[];


// TI extensions to add iso values
static const char ISO_MODE_AUTO[];
static const char ISO_MODE_100[];
static const char ISO_MODE_200[];
static const char ISO_MODE_400[];
static const char ISO_MODE_800[];
static const char ISO_MODE_1000[];
static const char ISO_MODE_1200[];
static const char ISO_MODE_1600[];

//  TI extensions to add  values for effect settings.
static const char EFFECT_NATURAL[];
static const char EFFECT_VIVID[];
static const char EFFECT_COLOR_SWAP[];




};

};

#endif


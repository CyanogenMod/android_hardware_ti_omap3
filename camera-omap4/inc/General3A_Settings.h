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
/**
* @file General3A_Settings.h
*
* This file maps the Camera Hardware Interface to OMX.
*
*/

#include "OMX_TI_IVCommon.h"
#include "OMX_TI_Common.h"
#include "OMX_TI_Index.h"
#include "TICameraParameters.h"

#ifndef GENERAL_3A_SETTINGS_H
#define GENERAL_3A_SETTINGS_H

#define FOCUS_FACE_PRIORITY OMX_IMAGE_FocusControlMax -1
#define FOCUS_REGION_PRIORITY OMX_IMAGE_FocusControlMax -2
#define WB_FACE_PRIORITY OMX_WhiteBalControlMax -1
#define EXPOSURE_FACE_PRIORITY OMX_ExposureControlMax - 1

namespace android {

struct userToOMX_LUT{
    const char * userDefinition;
    int         omxDefinition;
};

struct LUTtype{
    int size;
    const userToOMX_LUT *Table;
};

const userToOMX_LUT isoUserToOMX[] = {
    { TICameraParameters::ISO_MODE_AUTO, 0 },
    { TICameraParameters::ISO_MODE_100, 100 },
    { TICameraParameters::ISO_MODE_200, 200 },
    { TICameraParameters::ISO_MODE_400, 400 },
    { TICameraParameters::ISO_MODE_800, 800 },
    { TICameraParameters::ISO_MODE_1000, 1000 },
    { TICameraParameters::ISO_MODE_1200, 1200 },
    { TICameraParameters::ISO_MODE_1600, 1600 },
};

const userToOMX_LUT effects_UserToOMX [] = {
    { CameraParameters::EFFECT_NONE, OMX_ImageFilterNone },
    { CameraParameters::EFFECT_NEGATIVE, OMX_ImageFilterNegative },
    { CameraParameters::EFFECT_SOLARIZE,  OMX_ImageFilterSolarize },
    { CameraParameters::EFFECT_SEPIA, OMX_ImageFilterSepia },
    { CameraParameters::EFFECT_MONO, OMX_ImageFilterGrayScale },
    { TICameraParameters::EFFECT_NATURAL, OMX_ImageFilterNatural },
    { TICameraParameters::EFFECT_VIVID, OMX_ImageFilterVivid },
    { TICameraParameters::EFFECT_COLOR_SWAP, OMX_ImageFilterColourSwap   },
    { CameraParameters::EFFECT_BLACKBOARD, OMX_TI_ImageFilterBlackBoard },
    { CameraParameters::EFFECT_WHITEBOARD, OMX_TI_ImageFilterWhiteBoard },
    { CameraParameters::EFFECT_AQUA, OMX_TI_ImageFilterAqua },
    { CameraParameters::EFFECT_POSTERIZE, OMX_TI_ImageFilterPosterize },
    { TICameraParameters::EFFECT_BLACKWHITE, OMX_TI_ImageFilterBlackWhite }
};

const userToOMX_LUT scene_UserToOMX [] = {
    { CameraParameters::SCENE_MODE_AUTO, OMX_Manual },
    { TICameraParameters::SCENE_MODE_CLOSEUP, OMX_Closeup },
    { CameraParameters::SCENE_MODE_LANDSCAPE, OMX_Landscape },
    { TICameraParameters::SCENE_MODE_AQUA, OMX_Underwater },
    { TICameraParameters::SCENE_MODE_SPORT, OMX_Sport },
    { TICameraParameters::SCENE_MODE_MOOD, OMX_Mood },
    { CameraParameters::SCENE_MODE_NIGHT_PORTRAIT, OMX_NightPortrait },
    { TICameraParameters::SCENE_MODE_NIGHT_INDOOR, OMX_NightIndoor },
    { CameraParameters::SCENE_MODE_FIREWORKS, OMX_Fireworks },
    { TICameraParameters::SCENE_MODE_DOCUMENT, OMX_Document },
    { TICameraParameters::SCENE_MODE_BARCODE, OMX_Barcode },
    { TICameraParameters::SCENE_MODE_VIDEO_SUPER_NIGHT, OMX_SuperNight },
    { TICameraParameters::SCENE_MODE_VIDEO_CINE, OMX_Cine },
    { TICameraParameters::SCENE_MODE_VIDEO_OLD_FILM, OMX_OldFilm },
    { CameraParameters::SCENE_MODE_ACTION, OMX_TI_Action },
    { CameraParameters::SCENE_MODE_BEACH, OMX_TI_Beach },
    { CameraParameters::SCENE_MODE_CANDLELIGHT, OMX_TI_Candlelight },
    { CameraParameters::SCENE_MODE_NIGHT, OMX_TI_Night },
    { CameraParameters::SCENE_MODE_PARTY, OMX_TI_Party },
    { CameraParameters::SCENE_MODE_PORTRAIT, OMX_TI_Portrait },
    { CameraParameters::SCENE_MODE_SNOW, OMX_TI_Snow },
    { CameraParameters::SCENE_MODE_STEADYPHOTO, OMX_TI_Steadyphoto },
    { CameraParameters::SCENE_MODE_SUNSET, OMX_TI_Sunset },
    { CameraParameters::SCENE_MODE_THEATRE, OMX_TI_Theatre }
};

const userToOMX_LUT whiteBal_UserToOMX [] = {
    { CameraParameters::WHITE_BALANCE_AUTO, OMX_WhiteBalControlAuto },
    { CameraParameters::WHITE_BALANCE_DAYLIGHT, OMX_WhiteBalControlSunLight },
    { CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT, OMX_WhiteBalControlCloudy },
    { TICameraParameters::WHITE_BALANCE_TUNGSTEN, OMX_WhiteBalControlTungsten },
    { CameraParameters::WHITE_BALANCE_FLUORESCENT, OMX_WhiteBalControlFluorescent },
    { CameraParameters::WHITE_BALANCE_INCANDESCENT, OMX_WhiteBalControlIncandescent },
    { TICameraParameters::WHITE_BALANCE_HORIZON, OMX_WhiteBalControlHorizon },
    { CameraParameters::WHITE_BALANCE_SHADE, OMX_TI_WhiteBalControlShade },
    { CameraParameters::WHITE_BALANCE_TWILIGHT, OMX_TI_WhiteBalControlTwilight },
    { CameraParameters::WHITE_BALANCE_WARM_FLUORESCENT, OMX_TI_WhiteBalControlWarmFluorescent },
    { TICameraParameters::WHITE_BALANCE_FACE, WB_FACE_PRIORITY },
    { TICameraParameters::WHITE_BALANCE_SUNSET, OMX_TI_WhiteBalControlSunset }
};

const userToOMX_LUT antibanding_UserToOMX [] = {
    { CameraParameters::ANTIBANDING_OFF, OMX_FlickerCancelOff },
    { CameraParameters::ANTIBANDING_AUTO, OMX_FlickerCancelAuto },
    { CameraParameters::ANTIBANDING_50HZ, OMX_FlickerCancel50 },
    { CameraParameters::ANTIBANDING_60HZ, OMX_FlickerCancel60 }
};

const userToOMX_LUT focus_UserToOMX [] = {
    { CameraParameters::FOCUS_MODE_AUTO, OMX_IMAGE_FocusControlAutoLock },
    { CameraParameters::FOCUS_MODE_INFINITY, OMX_IMAGE_FocusControlAutoInfinity },
    { CameraParameters::FOCUS_MODE_MACRO, OMX_IMAGE_FocusControlAutoMacro },
    { TICameraParameters::FOCUS_MODE_PORTRAIT, OMX_IMAGE_FocusControlPortrait },
    { TICameraParameters::FOCUS_MODE_EXTENDED, OMX_IMAGE_FocusControlExtended },
    { TICameraParameters::FOCUS_MODE_CAF, OMX_IMAGE_FocusControlAuto },
    { TICameraParameters::FOCUS_MODE_TOUCH, FOCUS_REGION_PRIORITY },
    { TICameraParameters::FOCUS_MODE_FACE , FOCUS_FACE_PRIORITY },

};

const userToOMX_LUT exposure_UserToOMX [] = {
    { TICameraParameters::EXPOSURE_MODE_OFF, OMX_ExposureControlOff },
    { TICameraParameters::EXPOSURE_MODE_AUTO, OMX_ExposureControlAuto },
    { TICameraParameters::EXPOSURE_MODE_NIGHT, OMX_ExposureControlNight },
    { TICameraParameters::EXPOSURE_MODE_BACKLIGHT, OMX_ExposureControlBackLight },
    { TICameraParameters::EXPOSURE_MODE_SPOTLIGHT, OMX_ExposureControlSpotLight},
    { TICameraParameters::EXPOSURE_MODE_SPORTS, OMX_ExposureControlSports },
    { TICameraParameters::EXPOSURE_MODE_SNOW, OMX_ExposureControlSnow },
    { TICameraParameters::EXPOSURE_MODE_BEACH, OMX_ExposureControlBeach },
    { TICameraParameters::EXPOSURE_MODE_APERTURE, OMX_ExposureControlLargeAperture },
    { TICameraParameters::EXPOSURE_MODE_SMALL_APERTURE, OMX_ExposureControlSmallApperture },
    { TICameraParameters::EXPOSURE_MODE_FACE, EXPOSURE_FACE_PRIORITY },
};

const userToOMX_LUT flash_UserToOMX [] = {
    { CameraParameters::FLASH_MODE_OFF           ,OMX_IMAGE_FlashControlOff             },
    { CameraParameters::FLASH_MODE_ON            ,OMX_IMAGE_FlashControlOn              },
    { CameraParameters::FLASH_MODE_AUTO          ,OMX_IMAGE_FlashControlAuto            },
    { CameraParameters::FLASH_MODE_TORCH         ,OMX_IMAGE_FlashControlTorch           },
    { CameraParameters::FLASH_MODE_RED_EYE        ,OMX_IMAGE_FlashControlRedEyeReduction },
    { TICameraParameters::FLASH_MODE_FILL_IN        ,OMX_IMAGE_FlashControlFillin          }
};

const LUTtype ExpLUT =
    {
    sizeof(exposure_UserToOMX)/sizeof(exposure_UserToOMX[0]),
    exposure_UserToOMX
    };

const LUTtype WBalLUT =
    {
    sizeof(whiteBal_UserToOMX)/sizeof(whiteBal_UserToOMX[0]),
    whiteBal_UserToOMX
    };

const LUTtype FlickerLUT =
    {
    sizeof(antibanding_UserToOMX)/sizeof(antibanding_UserToOMX[0]),
    antibanding_UserToOMX
    };

const LUTtype SceneLUT =
    {
    sizeof(scene_UserToOMX)/sizeof(scene_UserToOMX[0]),
    scene_UserToOMX
    };

const LUTtype FlashLUT =
    {
    sizeof(flash_UserToOMX)/sizeof(flash_UserToOMX[0]),
    flash_UserToOMX
    };

const LUTtype EffLUT =
    {
    sizeof(effects_UserToOMX)/sizeof(effects_UserToOMX[0]),
    effects_UserToOMX
    };

const LUTtype FocusLUT =
    {
    sizeof(focus_UserToOMX)/sizeof(focus_UserToOMX[0]),
    focus_UserToOMX
    };

const LUTtype IsoLUT =
    {
    sizeof(isoUserToOMX)/sizeof(isoUserToOMX[0]),
    isoUserToOMX
    };

/*
*   class Gen3A_settings
*   stores the 3A settings
*   also defines the look up tables
*   for mapping settings from Hal to OMX
*/
class Gen3A_settings{
    public:

    int Exposure;
    OMX_U32 ExposureValueLeft;
    OMX_U32 ExposureValueRight;
    OMX_U32 ManualGainISOLeft;
    OMX_U32 ManualGainISORight;
    int WhiteBallance;
    int Flicker;
    int SceneMode;
    int Effect;
    int Focus;
    int EVCompensation;
    int Contrast;
    int Saturation;
    int Sharpness;
    int ISO;
    int FlashMode;

    unsigned int Brightness;
};

/*
*   Flags raised when a setting is changed
*/
enum E3ASettingsFlags
{
    SetSceneMode            = 1 << 0,
    SetEVCompensation       = 1 << 1,
    SetWhiteBallance        = 1 << 2,
    SetFlicker              = 1 << 3,
    SetExposure             = 1 << 4,
    SetSharpness            = 1 << 5,
    SetBrightness           = 1 << 6,
    SetContrast             = 1 << 7,
    SetISO                  = 1 << 8,
    SetSaturation           = 1 << 9,
    SetEffect               = 1 << 10,
    SetFocus                = 1 << 11,
    SetManualExposure       = 1 << 12,
    SetManualGain           = 1 << 13,
    SetExpMode              = 1 << 14,
    SetFlash                = 1 << 15,

    E3aSettingMax,
    E3AsettingsAll = ( ((E3aSettingMax -1 ) << 1) -1 ) /// all possible flags raised
};

};

#endif //GENERAL_3A_SETTINGS_H

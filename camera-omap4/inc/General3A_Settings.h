#include "OMX_TI_IVCommon.h"
#include "OMX_TI_Common.h"
#include "OMX_TI_Index.h"
#include "TICameraParameters.h"
#include "CameraHal.h"

#ifndef GENERAL_3A_SETTINGS_H
#define GENERAL_3A_SETTINGS_H

namespace android {

/*
*   Effect string constants
*/
const char * effNone 				= CameraParameters::EFFECT_NONE;
const char * effNegative 			= CameraParameters::EFFECT_NEGATIVE;
const char * effSolarize 			= CameraParameters::EFFECT_SOLARIZE;
const char * effSepia 				= CameraParameters::EFFECT_SEPIA;
const char * effMono 				= CameraParameters::EFFECT_MONO;
const char * effNatural 			= TICameraParameters::EFFECT_NATURAL;
const char * effVivid 				= TICameraParameters::EFFECT_VIVID;
const char * effColourSwap 			= TICameraParameters::EFFECT_COLOR_SWAP;

/*
* ISO Modes
*/
const char * isoAuto 				= TICameraParameters::ISO_MODE_AUTO;
const char * iso100 				= TICameraParameters::ISO_MODE_100;
const char * iso200 				= TICameraParameters::ISO_MODE_200;
const char * iso400 				= TICameraParameters::ISO_MODE_400;
const char * iso800 				= TICameraParameters::ISO_MODE_800;
const char * iso1000 				= TICameraParameters::ISO_MODE_1000;
const char * iso1200 				= TICameraParameters::ISO_MODE_1200;
const char * iso1600 				= TICameraParameters::ISO_MODE_1600;

/*
* Scene modes
*/
const char * sceneManual 			= CameraParameters::SCENE_MODE_AUTO;
const char * scenePortrait 			= CameraParameters::SCENE_MODE_PORTRAIT;
const char * sceneLandscape 		= CameraParameters::SCENE_MODE_LANDSCAPE;
const char * sceneSport 			= CameraParameters::SCENE_MODE_ACTION;
const char * sceneSnow 				= CameraParameters::SCENE_MODE_SNOW;
const char * sceneBeach 			= CameraParameters::SCENE_MODE_BEACH;
const char * sceneNoghtPortrait 	= CameraParameters::SCENE_MODE_NIGHT_PORTRAIT;
const char * sceneFireworks 		= CameraParameters::SCENE_MODE_FIREWORKS;

const char * sceneCloseup 			= TICameraParameters::SCENE_MODE_CLOSEUP;
const char * sceneUnderwater 		= TICameraParameters::SCENE_MODE_AQUA;
const char * sceneMood 				= TICameraParameters::SCENE_MODE_MOOD;
const char * sceneNightIndoor 		= TICameraParameters::SCENE_MODE_NIGHT_INDOOR;
const char * imgSceneDocument 		= TICameraParameters::SCENE_MODE_DOCUMENT;
const char * imgSceneBarcode 		= TICameraParameters::SCENE_MODE_BARCODE;
const char * vidSceneSuperNight 	= TICameraParameters::SCENE_MODE_VIDEO_SUPER_NIGHT;
const char * vidSceneCine 			= TICameraParameters::SCENE_MODE_VIDEO_CINE;
const char * vidSceneOldFilm 		= TICameraParameters::SCENE_MODE_VIDEO_OLD_FILM;

/*
* Whitebalance modes
*/
const char * whiteBalAuto 			= CameraParameters::WHITE_BALANCE_AUTO;
const char * whiteBalDaylight 		= CameraParameters::WHITE_BALANCE_DAYLIGHT;
const char * whiteBalCloudy 		= CameraParameters::WHITE_BALANCE_CLOUDY_DAYLIGHT;
const char * whiteBalShade 			= CameraParameters::WHITE_BALANCE_SHADE;
const char * whiteBalFluorescent 	= CameraParameters::WHITE_BALANCE_FLUORESCENT;
const char * whiteBalIncandescent 	= CameraParameters::WHITE_BALANCE_INCANDESCENT;

const char * whiteBalHorizon 		= TICameraParameters::WHITE_BALANCE_HORIZON;
const char * whiteBalTungsten 		= TICameraParameters::WHITE_BALANCE_TUNGSTEN;

/*
* Antibanding modes
*/
const char * antibandingOff 		= CameraParameters::ANTIBANDING_OFF;
const char * antibandingAuto 		= CameraParameters::ANTIBANDING_AUTO;
const char * antibanding50hz 		= CameraParameters::ANTIBANDING_50HZ;
const char * antibanding60hz 		= CameraParameters::ANTIBANDING_60HZ;

/*
* Focus modes
*/
const char * focusAuto 				= CameraParameters::FOCUS_MODE_AUTO;
const char * focusInfinity 			= CameraParameters::FOCUS_MODE_INFINITY;
const char * focusMacro 			= CameraParameters::FOCUS_MODE_MACRO;

const char * focusPortrait 			= TICameraParameters::FOCUS_MODE_PORTRAIT;
const char * focusExtended 			= TICameraParameters::FOCUS_MODE_EXTENDED;
const char * focusCAF 				= TICameraParameters::FOCUS_MODE_CAF;

/*
* Exposure modes
*/
const char * exposureOff 			= TICameraParameters::EXPOSURE_MODE_OFF;
const char * exposureAuto 			= TICameraParameters::EXPOSURE_MODE_AUTO;
const char * exposureNight 			= TICameraParameters::EXPOSURE_MODE_NIGHT;
const char * exposureBackLight 		= TICameraParameters::EXPOSURE_MODE_BACKLIGHT;
const char * exposureSpotLight 		= TICameraParameters::EXPOSURE_MODE_SPOTLIGHT;
const char * exposureSports			= TICameraParameters::EXPOSURE_MODE_SPORTS;
const char * exposureSnow 			= TICameraParameters::EXPOSURE_MODE_SNOW;
const char * exposureBeach 			= TICameraParameters::EXPOSURE_MODE_BEACH;
const char * exposureAperture 		= TICameraParameters::EXPOSURE_MODE_APERTURE;
const char * exposureSmallAperture 	= TICameraParameters::EXPOSURE_MODE_SMALL_APERTURE;

struct userToOMX_LUT{
    const char * userDefinition;
    int         omxDefinition;
};

struct LUTtype{
    int size;
    userToOMX_LUT *Table;
};

userToOMX_LUT isoUserToOMX[] = {
    {isoAuto, 0},
    {iso100, 100},
    {iso200, 200},
    {iso400, 400},
    {iso800, 800},
    {iso1000, 1000},
    {iso1200, 1200},
    {iso1600, 1600},
};

userToOMX_LUT effects_UserToOMX [] = {
    {  effNone,            OMX_ImageFilterNone         },
    {  effNegative,        OMX_ImageFilterNegative     },
    {  effSolarize,        OMX_ImageFilterSolarize     },
    {  effSepia,           OMX_ImageFilterSepia        },
    {  effMono,            OMX_ImageFilterGrayScale    },
    {  effNatural,         OMX_ImageFilterNatural      },
    {  effVivid,           OMX_ImageFilterVivid        },
    {  effColourSwap,      OMX_ImageFilterColourSwap   },
};

userToOMX_LUT scene_UserToOMX [] = {
    {  sceneManual        ,OMX_Manual       },
    {  sceneCloseup       ,OMX_Closeup      },
    {  scenePortrait      ,OMX_Portrait     },
    {  sceneLandscape     ,OMX_Landscape    },
    {  sceneUnderwater    ,OMX_Underwater   },
    {  sceneSport         ,OMX_Sport        },
    {  sceneBeach		  ,OMX_SnowBeach    },
    {  sceneSnow		  ,OMX_SnowBeach    },
    {  sceneMood          ,OMX_Mood         },
    {  sceneNoghtPortrait ,OMX_NightPortrait},
    {  sceneNightIndoor   ,OMX_NightIndoor  },
    {  sceneFireworks     ,OMX_Fireworks    },
    {  imgSceneDocument   ,OMX_Document     },
    {  imgSceneBarcode    ,OMX_Barcode      },
    {  vidSceneSuperNight ,OMX_SuperNight   },
    {  vidSceneCine       ,OMX_Cine         },
    {  vidSceneOldFilm    ,OMX_OldFilm      }
};

userToOMX_LUT whiteBal_UserToOMX [] = {
    { whiteBalAuto           ,OMX_WhiteBalControlAuto          },
    { whiteBalDaylight       ,OMX_WhiteBalControlSunLight      },
    { whiteBalCloudy         ,OMX_WhiteBalControlCloudy        },
    { whiteBalShade          ,OMX_WhiteBalControlShade         },
    { whiteBalTungsten       ,OMX_WhiteBalControlTungsten      },
    { whiteBalFluorescent    ,OMX_WhiteBalControlFluorescent   },
    { whiteBalIncandescent   ,OMX_WhiteBalControlIncandescent  },
    { whiteBalHorizon        ,OMX_WhiteBalControlHorizon       }
};

userToOMX_LUT antibanding_UserToOMX [] = {
    { antibandingOff    ,OMX_FlickerCancelOff   },
    { antibandingAuto   ,OMX_FlickerCancelAuto  },
    { antibanding50hz   ,OMX_FlickerCancel50    },
    { antibanding60hz   ,OMX_FlickerCancel60    }
};


userToOMX_LUT focus_UserToOMX [] = {
    { focusAuto     ,OMX_IMAGE_FocusControlAutoLock         },
    { focusInfinity ,OMX_IMAGE_FocusControlAutoInfinity },
    { focusMacro    ,OMX_IMAGE_FocusControAutoMacro     },
    { focusPortrait ,OMX_IMAGE_FocusControlPortrait     },
    { focusExtended ,OMX_IMAGE_FocusControlExtended     },
    { focusCAF      ,OMX_IMAGE_FocusControlAuto },
};

userToOMX_LUT exposure_UserToOMX [] = {
    { exposureOff           ,OMX_ExposureControlOff             },
    { exposureAuto          ,OMX_ExposureControlAuto            },
    { exposureNight         ,OMX_ExposureControlNight           },
    { exposureBackLight     ,OMX_ExposureControlBackLight       },
    { exposureSpotLight     ,OMX_ExposureControlSpotLight       },
    { exposureSports        ,OMX_ExposureControlSports          },
    { exposureSnow          ,OMX_ExposureControlSnow            },
    { exposureBeach         ,OMX_ExposureControlBeach           },
    { exposureAperture      ,OMX_ExposureControlLargeAperture   },
    { exposureSmallAperture ,OMX_ExposureControlSmallApperture  }
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

const LUTtype FlickerLUT
    {
    sizeof(antibanding_UserToOMX)/sizeof(antibanding_UserToOMX[0]),
    antibanding_UserToOMX
    };

const LUTtype SceneLUT =
    {
    sizeof(scene_UserToOMX)/sizeof(scene_UserToOMX[0]),
    scene_UserToOMX
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

    unsigned int Brightness;
};

/*
*   Flags raised when a setting is changed
*/
enum E3ASettingsFlags
{
    SetExposure             = 1 << 0,
    SetEVCompensation       = 1 << 1,
    SetWhiteBallance        = 1 << 2,
    SetFlicker              = 1 << 3,
    SetSceneMode            = 1 << 4,
    SetSharpness            = 1 << 5,
    SetBrightness           = 1 << 6,
    SetContrast             = 1 << 7,
    SetISO                  = 1 << 8,
    SetSaturation           = 1 << 9,
    SetEffect               = 1 << 10,
    SetFocus                = 1 << 11,

    E3aSettingMax,
    E3AsettingsAll = ( ((E3aSettingMax -1 ) << 1) -1 ) /// all possible flags raised
};

};

#endif //GENERAL_3A_SETTINGS_H
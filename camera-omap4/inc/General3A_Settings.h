#include "OMX_TI_IVCommon.h"
#include "OMX_TI_Common.h"
#include "OMX_TI_Index.h"

/*
*   Effect string constants
*/
const char * effNone = "none";
const char * effNoise = "noise";
const char * effEmboss = "emboss";
const char * effNegative = "negative";
const char * effSketch = "sketch";
const char * effOilPaint = "oilpaint";
const char * effHatch = "hatch";
const char * effGpen = "gpen";
const char * effAntialias = "antialias";
const char * effDeRing = "dering";
const char * effSolarize = "solarize";
const char * effSepia = "sepia";
const char * effGrayScale = "grayscale";
const char * effNatural = "natural";
const char * effVivid = "vivid";
const char * effColourSwap = "colourswap";
const char * effOutOfFocus = "outoffocus";
const char * effWaterColour = "watercolour";
const char * effPastel = "pastel";
const char * effFilm = "film";

/*
*
*/
const char * isoAuto = "auto";
const char * iso100 = "100";
const char * iso200 = "200";
const char * iso400 = "400";
const char * iso800 = "800";

/*
*
*/
const char * sceneManual = "auto";
const char * sceneCloseup = "closeup";
const char * scenePortrait = "portrait";
const char * sceneLandscape = "landscape";
const char * sceneUnderwater = "aqua";
const char * sceneSport = "action";
const char * sceneSnowBeach = "snowbeach";
const char * sceneMood = "mood";
const char * sceneNoghtPortrait = "nightportrait";
const char * sceneNightIndoor = "nightindoor";
const char * sceneFireworks = "fireworks";

const char * imgSceneDocument = "document";
const char * imgSceneBarcode = "barcode";

const char * vidSceneSuperNight = "supernight";
const char * vidSceneCine = "cine";
const char * vidSceneOldFilm = "oldfilm";

/*
*
*/
const char * whihteBalOff = "off";
const char * whihteBalAuto = "auto";
const char * whihteBalSunlight = "sunlight";
const char * whihteBalCloudy = "cloudy";
const char * whihteBalShade = "shade";
const char * whihteBalTungsteen = "tungsten";
const char * whihteBalFluorescent = "fluorescent";
const char * whihteBalIncandescent = "incandescent";
const char * whihteBalFlash = "flash";
const char * whihteBalHorizon = "horizon";

/*
*
*/
const char * antibandingOff = "off";
const char * antibandingAuto = "auto";
const char * antibanding50hz = "50hz";
const char * antibanding60hz = "60hz";

/*
*
*/
const char * focusAuto = "auto";
const char * focusInfinity = "infinity";
const char * focusMacro = "macro";
const char * focusFixed = "fixed";
const char * focusOff = "off";

/*
*
*/
const char * exposureOff = "off";
const char * exposureAuto = "auto";
const char * exposureNight = "night";
const char * exposureBackLight = "backlighting";
const char * exposureSpotLight = "spotlight";
const char * exposureSports = "sports";
const char * exposureSnow = "snow";
const char * exposureBeach = "beach";
const char * exposureAperture = "aperture";
const char * exposureSmallAperture = "smallaperture";

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
    {iso800, 800}
};

userToOMX_LUT effects_UserToOMX [] = {
    {  effNone,            OMX_ImageFilterNone         },
    {  effNoise,           OMX_ImageFilterNoise        },
    {  effEmboss,          OMX_ImageFilterEmboss       },
    {  effNegative,        OMX_ImageFilterNegative     },
    {  effSketch,          OMX_ImageFilterSketch       },
    {  effOilPaint,        OMX_ImageFilterOilPaint     },
    {  effHatch,           OMX_ImageFilterHatch        },
    {  effGpen,            OMX_ImageFilterGpen         },
    {  effAntialias,       OMX_ImageFilterAntialias    },
    {  effDeRing,          OMX_ImageFilterDeRing       },
    {  effSolarize,        OMX_ImageFilterSolarize     },
    {  effSepia,           OMX_ImageFilterSepia        },
    {  effGrayScale,       OMX_ImageFilterGrayScale    },
    {  effNatural,         OMX_ImageFilterNatural      },
    {  effVivid,           OMX_ImageFilterVivid        },
    {  effColourSwap,      OMX_ImageFilterColourSwap   },
    {  effOutOfFocus,      OMX_ImageFilterOutOfFocus   },
    {  effWaterColour,     OMX_ImageFilterWaterColour  },
    {  effPastel,          OMX_ImageFilterPastel       },
    {  effFilm,            OMX_ImageFilterFilm         }
};

userToOMX_LUT scene_UserToOMX [] = {
    {  sceneManual        ,OMX_Manual       },
    {  sceneCloseup       ,OMX_Closeup      },
    {  scenePortrait      ,OMX_Portrait     },
    {  sceneLandscape     ,OMX_Landscape    },
    {  sceneUnderwater    ,OMX_Underwater   },
    {  sceneSport         ,OMX_Sport        },
    {  sceneSnowBeach     ,OMX_SnowBeach    },
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
    { whihteBalOff            ,OMX_WhiteBalControlOff           },
    { whihteBalAuto           ,OMX_WhiteBalControlAuto          },
    { whihteBalSunlight       ,OMX_WhiteBalControlSunLight      },
    { whihteBalCloudy         ,OMX_WhiteBalControlCloudy        },
    { whihteBalShade          ,OMX_WhiteBalControlShade         },
    { whihteBalTungsteen      ,OMX_WhiteBalControlTungsten      },
    { whihteBalFluorescent    ,OMX_WhiteBalControlFluorescent   },
    { whihteBalIncandescent   ,OMX_WhiteBalControlIncandescent  },
    { whihteBalFlash          ,OMX_WhiteBalControlFlash         },
    { whihteBalHorizon        ,OMX_WhiteBalControlHorizon       }
};

userToOMX_LUT antibanding_UserToOMX [] = {
    { antibandingOff    ,OMX_FlickerCancelOff   },
    { antibandingAuto   ,OMX_FlickerCancelAuto  },
    { antibanding50hz   ,OMX_FlickerCancel50    },
    { antibanding60hz   ,OMX_FlickerCancel60    }
};


userToOMX_LUT focus_UserToOMX [] = {
    { focusAuto     ,OMX_IMAGE_FocusControlAuto         },
    { focusInfinity ,OMX_IMAGE_FocusControlAutoInfinity },
    { focusMacro    ,OMX_IMAGE_FocusControAutoMacro     },
    { focusFixed    ,OMX_IMAGE_FocusControlAutoLock     },
    { focusOff      ,OMX_IMAGE_FocusControlOff          }
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
    unsigned int ApertureFNumber;
    unsigned int ShutterSpeed;
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
    SetAperture             = 1 << 8,
    SetShutterSpeed         = 1 << 9,
    SetISO                  = 1 << 10,
    SetSaturaion            = 1 << 11,
    SetEffect               = 1 << 12,
    SetFocus                = 1 << 13,

    E3aSettingMax,
    E3AsettingsAll = ( ((E3aSettingMax -1 ) << 1) -1 ) /// all possible flags raised
};

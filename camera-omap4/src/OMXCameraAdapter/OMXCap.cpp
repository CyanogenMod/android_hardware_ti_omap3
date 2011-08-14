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
* @file OMXCap.cpp
*
* This file implements the OMX Capabilities feature.
*
*/

#include "CameraHal.h"
#include "OMXCameraAdapter.h"
#include "ErrorUtils.h"
#include "TICameraParameters.h"

namespace android {

#undef LOG_TAG

///Maintain a separate tag for OMXCameraAdapter logs to isolate issues OMX specific
#define LOG_TAG "OMXCameraAdapter"

const char PARAM_SEP[] = ",";
const uint32_t VFR_OFFSET = 8;
const char VFR_BACKET_START[] = "(";
const char VFR_BRACKET_END[] = ")";
static const char FRAMERATE_COUNT = 10;

const CapResolution OMXCameraAdapter::mImageCapRes [] = {
    { 4032, 3024, "4032x3024" },
    { 4000, 3000, "4000x3000" },
    { 3648, 2736, "3648x2736" },
    { 3264, 2448, "3264x2448" },
    { 2592, 1944, "2592x1944" },
    { 2048, 1536, "2048x1536" },
    { 1600, 1200, "1600x1200" },
    { 1280, 1024, "1280x1024" },
    { 1152, 864, "1152x864" },
    { 1280, 960, "1280x960" },
    { 640, 480, "640x480" },
    { 320, 240, "320x240" },
};

const CapResolution OMXCameraAdapter::mPreviewRes [] = {
    { 1920, 1080, "1920x1080" },
    { 1280, 720, "1280x720" },
    { 800, 480, "800x480" },
    { 720, 576, "720x576" },
    { 720, 480, "720x480" },
    { 768, 576, "768x576" },
    { 640, 480, "640x480" },
    { 320, 240, "320x240" },
    { 352, 288, "352x288" },
    { 240, 160, "240x160" },
    { 176, 144, "176x144" },
    { 128, 96, "128x96" },
};

const CapResolution OMXCameraAdapter::mThumbRes [] = {
    { 640, 480, "640x480" },
    { 160, 120, "160x120" },
    { 200, 120, "200x120" },
    { 320, 240, "320x240" },
    { 512, 384, "512x384" },
    { 352, 144, "352x144" },
    { 176, 144, "176x144" },
    { 96, 96, "96x96" },
};

const CapPixelformat OMXCameraAdapter::mPixelformats [] = {
    { OMX_COLOR_FormatCbYCrY, CameraParameters::PIXEL_FORMAT_YUV422I },
    { OMX_COLOR_FormatYUV420SemiPlanar, CameraParameters::PIXEL_FORMAT_YUV420SP },
    { OMX_COLOR_Format16bitRGB565, CameraParameters::PIXEL_FORMAT_RGB565 },
    { OMX_COLOR_FormatRawBayer10bit, TICameraParameters::PIXEL_FORMAT_RAW },
};

const CapFramerate OMXCameraAdapter::mFramerates [] = {
    { 60, "60" },
    { 30, "30" },
    { 25, "25" },
    { 24, "24" },
    { 20, "20" },
    { 15, "15" },
    { 10, "10" },
    { 1, "1" },
};

const CapZoom OMXCameraAdapter::mZoomStages [] = {
    { 65536, "100" },
    { 68157, "104" },
    { 70124, "107" },
    { 72745, "111" },
    { 75366, "115" },
    { 77988, "119" },
    { 80609, "123" },
    { 83231, "127" },
    { 86508, "132" },
    { 89784, "137" },
    { 92406, "141" },
    { 95683, "146" },
    { 99615, "152" },
    { 102892, "157" },
    { 106168, "162" },
    { 110100, "168" },
    { 114033, "174" },
    { 117965, "180" },
    { 122552, "187" },
    { 126484, "193" },
    { 131072, "200" },
    { 135660, "207" },
    { 140247, "214" },
    { 145490, "222" },
    { 150733, "230" },
    { 155976, "238" },
    { 161219, "246" },
    { 167117, "255" },
    { 173015, "264" },
    { 178913, "273" },
    { 185467, "283" },
    { 192020, "293" },
    { 198574, "303" },
    { 205783, "314" },
    { 212992, "325" },
    { 220201, "336" },
    { 228065, "348" },
    { 236585, "361" },
    { 244449, "373" },
    { 252969, "386" },
    { 262144, "400" },
    { 271319, "414" },
    { 281149, "429" },
    { 290980, "444" },
    { 300810, "459" },
    { 311951, "476" },
    { 322437, "492" },
    { 334234, "510" },
    { 346030, "528" },
    { 357827, "546" },
    { 370934, "566" },
    { 384041, "586" },
    { 397148, "606" },
    { 411566, "628" },
    { 425984, "650" },
    { 441057, "673" },
    { 456131, "696" },
    { 472515, "721" },
    { 488899, "746" },
    { 506593, "773" },
    { 524288, "800" },
};

const CapISO OMXCameraAdapter::mISOStages [] = {
    { 0, "auto" },
    { 100, "100" },
    { 200, "200"},
    { 400, "400" },
    { 800, "800" },
    { 1000, "1000" },
    { 1200, "1200" },
    { 1600, "1600" },
};

status_t OMXCameraAdapter::encodePixelformatCap(OMX_COLOR_FORMATTYPE format, const CapPixelformat *cap, size_t capCount, char * buffer, size_t bufferSize)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( ( NULL == buffer ) ||
         ( NULL == cap ) )
        {
        CAMHAL_LOGEA("Invalid input arguments");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        for ( unsigned int i = 0 ; i < capCount ; i++ )
            {
            if ( format == cap[i].pixelformat )
                {
                strncat(buffer, cap[i].param, bufferSize - 1);
                strncat(buffer, PARAM_SEP, bufferSize - 1);
                }
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::encodeFramerateCap(OMX_U32 framerateMax, OMX_U32 framerateMin, const CapFramerate *cap, size_t capCount, char * buffer, size_t bufferSize)
{
    status_t ret = NO_ERROR;
    bool minInserted = false;
    bool maxInserted = false;
    char tmpBuffer[FRAMERATE_COUNT];

    LOG_FUNCTION_NAME

    if ( ( NULL == buffer ) ||
         ( NULL == cap ) )
        {
        CAMHAL_LOGEA("Invalid input arguments");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        for ( unsigned int i = 0 ; i < capCount ; i++ )
            {
            if ( ( framerateMax >= cap[i].framerate ) &&
                 ( framerateMin <= cap[i].framerate ) )
                {
                strncat(buffer, cap[i].param, bufferSize - 1);
                strncat(buffer, PARAM_SEP, bufferSize - 1);

                if ( cap[i].framerate ==  framerateMin )
                    {
                    minInserted = true;
                    }
                }
                if ( cap[i].framerate ==  framerateMax )
                    {
                    maxInserted = true;
                    }
                }

        if ( !maxInserted )
            {
            memset(tmpBuffer, 0, FRAMERATE_COUNT);
            snprintf(tmpBuffer, FRAMERATE_COUNT - 1, "%u,", ( unsigned int ) framerateMax);
            strncat(buffer, tmpBuffer, bufferSize - 1);
            strncat(buffer, PARAM_SEP, bufferSize - 1);
            }

        if ( !minInserted )
            {
            memset(tmpBuffer, 0, FRAMERATE_COUNT);
            snprintf(tmpBuffer, FRAMERATE_COUNT - 1, "%u,", ( unsigned int ) framerateMin);
            strncat(buffer, tmpBuffer, bufferSize - 1);
            strncat(buffer, PARAM_SEP, bufferSize - 1);
            }

        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::encodeVFramerateCap(OMX_TI_CAPTYPE &caps, char * buffer, size_t bufferSize)
{
    status_t ret = NO_ERROR;
    uint32_t minVFR, maxVFR;
    char tmpBuffer[MAX_PROP_VALUE_LENGTH];
    bool skipLast = false;

    LOG_FUNCTION_NAME

    if ( NULL == buffer )
        {
        CAMHAL_LOGEA("Invalid input arguments");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        unsigned int count = caps.ulPrvVarFPSModesCount;
        if ( count > 10 )
        {
            count = 10;
        }
        for ( unsigned int i = 0 ; i < count ; i++ )
            {

            if ( 0 < i )
                {
                if ( ( caps.tPrvVarFPSModes[i-1].nVarFPSMin == caps.tPrvVarFPSModes[i].nVarFPSMin ) &&
                     ( caps.tPrvVarFPSModes[i-1].nVarFPSMax == caps.tPrvVarFPSModes[i].nVarFPSMax ) )
                    {
                    continue;
                    }
                else if (!skipLast)
                    {
                    strncat(buffer, PARAM_SEP, bufferSize - 1);
                    }
                }
            if ( caps.tPrvVarFPSModes[i].nVarFPSMin == caps.tPrvVarFPSModes[i].nVarFPSMax )
                {
                skipLast = true;
                continue;
                }
            else
                {
                skipLast = false;
                }

            CAMHAL_LOGEB("Min fps 0x%x, Max fps 0x%x", ( unsigned int ) caps.tPrvVarFPSModes[i].nVarFPSMin,
                                                       ( unsigned int ) caps.tPrvVarFPSModes[i].nVarFPSMax);

            minVFR = caps.tPrvVarFPSModes[i].nVarFPSMin >> VFR_OFFSET;
            minVFR *= CameraHal::VFR_SCALE;
            maxVFR = caps.tPrvVarFPSModes[i].nVarFPSMax >> VFR_OFFSET;
            maxVFR *= CameraHal::VFR_SCALE;
            snprintf(tmpBuffer, ( MAX_PROP_VALUE_LENGTH - 1 ), "(%d,%d)", minVFR, maxVFR);
            strncat(buffer, tmpBuffer, ( bufferSize - 1 ));
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

size_t OMXCameraAdapter::encodeZoomCap(OMX_S32 maxZoom, const CapZoom *cap, size_t capCount, char * buffer, size_t bufferSize)
{
    status_t res = NO_ERROR;
    size_t ret = 0;

    LOG_FUNCTION_NAME

    if ( ( NULL == buffer ) ||
         ( NULL == cap ) )
        {
        CAMHAL_LOGEA("Invalid input arguments");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        for ( unsigned int i = 0 ; i < capCount ; i++ )
            {
            if ( cap[i].zoomStage <= maxZoom )
                {
                strncat(buffer, cap[i].param, bufferSize - 1);
                strncat(buffer, PARAM_SEP, bufferSize - 1);
                ret++;
                }
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::encodeISOCap(OMX_U32 maxISO, const CapISO *cap, size_t capCount, char * buffer, size_t bufferSize)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( ( NULL == buffer ) ||
         ( NULL == cap ) )
        {
        CAMHAL_LOGEA("Invalid input arguments");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        for ( unsigned int i = 0 ; i < capCount ; i++ )
            {
            if ( cap[i].iso <= maxISO)
                {
                strncat(buffer, cap[i].param, bufferSize - 1);
                strncat(buffer, PARAM_SEP, bufferSize - 1);
                }
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::encodeSizeCap(OMX_TI_CAPRESTYPE &res, const CapResolution *cap, size_t capCount, char * buffer, size_t bufferSize)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( ( NULL == buffer ) ||
         ( NULL == cap ) )
        {
        CAMHAL_LOGEA("Invalid input arguments");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        for ( unsigned int i = 0 ; i < capCount ; i++ )
            {
            if ( ( cap[i].width <= res.nWidthMax )  &&
                 ( cap[i].height <= res.nHeightMax ) &&
                 ( cap[i].width >= res.nWidthMin ) &&
                 ( cap[i].height >= res.nHeightMin ) )
                    {
                    strncat(buffer, cap[i].param, bufferSize -1);
                    strncat(buffer, PARAM_SEP, bufferSize - 1);
                    }
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::insertImageSizes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        ret = encodeSizeCap(caps.tImageResRange, mImageCapRes,
                                        ( sizeof(mImageCapRes) /sizeof(mImageCapRes[0]) ), supported,
                                        MAX_PROP_VALUE_LENGTH);

        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error inserting supported picture sizes 0x%x", ret);
            }
        else
            {
            params.set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, supported);
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertPreviewSizes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        ret = encodeSizeCap(caps.tPreviewResRange, mPreviewRes,
                                        ( sizeof(mPreviewRes) /sizeof(mPreviewRes[0]) ), supported,
                                        MAX_PROP_VALUE_LENGTH);

        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error inserting supported preview sizes 0x%x", ret);
            }
        else
            {
            params.set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, supported);
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertThumbSizes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        ret = encodeSizeCap(caps.tThumbResRange, mThumbRes,
                                        ( sizeof(mThumbRes) /sizeof(mThumbRes[0]) ), supported,
                                        MAX_PROP_VALUE_LENGTH);

        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error inserting supported thumbnail sizes 0x%x", ret);
            }
        else
            {
            //CTS Requirement: 0x0 should always be supported
            strncat(supported, "0x0", MAX_PROP_NAME_LENGTH);
            params.set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, supported);
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertZoomStages(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        size_t zoomStageCount = encodeZoomCap(caps.xMaxWidthZoom, mZoomStages,
                                        ( sizeof(mZoomStages) /sizeof(mZoomStages[0]) ), supported,
                                        MAX_PROP_VALUE_LENGTH);

        params.set(CameraParameters::KEY_ZOOM_RATIOS, supported);
        params.set(CameraParameters::KEY_MAX_ZOOM, zoomStageCount - 1); //As per CTS requirement
        if ( 0 == zoomStageCount )
            {
            params.set(CameraParameters::KEY_ZOOM_SUPPORTED, TICameraParameters::ZOOM_UNSUPPORTED);
            params.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, TICameraParameters::ZOOM_UNSUPPORTED);
            }
        else
            {
            params.set(CameraParameters::KEY_ZOOM_SUPPORTED, TICameraParameters::ZOOM_SUPPORTED);
            params.set(CameraParameters::KEY_SMOOTH_ZOOM_SUPPORTED, TICameraParameters::ZOOM_SUPPORTED);
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertImageFormats(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        for ( int i = 0 ; i < caps.ulImageFormatCount ; i++ )
            {
            ret = encodePixelformatCap(caps.eImageFormats[i], mPixelformats,
                                            ( sizeof(mPixelformats) /sizeof(mPixelformats[0]) ), supported,
                                            MAX_PROP_VALUE_LENGTH);

            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("Error inserting supported picture formats 0x%x", ret);
                break;
                }
            }

        if ( NO_ERROR == ret )
            {
            //jpeg is not supported in OMX capabilies yet
            strncat(supported, CameraParameters::PIXEL_FORMAT_JPEG, MAX_PROP_VALUE_LENGTH - 1);
            params.set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, supported);
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertPreviewFormats(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        for ( int i = 0 ; i < caps.ulPreviewFormatCount; i++ )
            {
            ret = encodePixelformatCap(caps.ePreviewFormats[i], mPixelformats,
                                            ( sizeof(mPixelformats) /sizeof(mPixelformats[0]) ), supported,
                                            MAX_PROP_VALUE_LENGTH);

            if ( NO_ERROR != ret )
                {
                CAMHAL_LOGEB("Error inserting supported preview formats 0x%x", ret);
                break;
                }
            }

        if ( NO_ERROR == ret )
            {
            params.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, supported);
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertFramerates(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        ret = encodeFramerateCap(caps.xFramerateMax,// >> VFR_OFFSET,
                                 caps.xFramerateMin,// >> VFR_OFFSET,
                                 mFramerates,
                                 ( sizeof(mFramerates) /sizeof(mFramerates[0]) ),
                                 supported,
                                 MAX_PROP_VALUE_LENGTH);

        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error inserting supported preview framerates 0x%x", ret);
            }
        else
            {
            params.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, supported);
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertVFramerates(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        ret = encodeVFramerateCap(caps, supported,
                                  MAX_PROP_VALUE_LENGTH);

        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error inserting supported preview framerate ranges 0x%x", ret);
            }
        else
            {
            params.set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, supported);
            CAMHAL_LOGEB("framerate ranges %s", supported);
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertEVs(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        snprintf(supported, MAX_PROP_VALUE_LENGTH, "%d", ( int ) ( caps.xEVCompensationMin * 10 ));
        params.set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, supported);

        snprintf(supported, MAX_PROP_VALUE_LENGTH, "%d", ( int ) ( caps.xEVCompensationMax * 10 ));
        params.set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, supported);
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertISOModes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        ret = encodeISOCap(caps.nSensitivityMax, mISOStages,
                                        ( sizeof(mISOStages) /sizeof(mISOStages[0]) ), supported,
                                        MAX_PROP_VALUE_LENGTH);

        if ( NO_ERROR != ret )
            {
            CAMHAL_LOGEB("Error inserting supported ISO modes 0x%x", ret);
            }
        else
            {
            params.set(TICameraParameters::KEY_SUPPORTED_ISO_VALUES, supported);
            }
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertIPPModes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);

        //Off is always supported
        strncat(supported, TICameraParameters::IPP_NONE, MAX_PROP_NAME_LENGTH);
        strncat(supported, PARAM_SEP, 1);

        if ( caps.bLensDistortionCorrectionSupported )
            {
            strncat(supported, TICameraParameters::IPP_LDC, MAX_PROP_NAME_LENGTH);
            strncat(supported, PARAM_SEP, 1);
            }

        if ( caps.bISONoiseFilterSupported )
            {
            strncat(supported, TICameraParameters::IPP_NSF, MAX_PROP_NAME_LENGTH);
            strncat(supported, PARAM_SEP, 1);
            }


        if ( caps.bISONoiseFilterSupported && caps.bLensDistortionCorrectionSupported )
            {
            strncat(supported, TICameraParameters::IPP_LDCNSF, MAX_PROP_NAME_LENGTH);
            strncat(supported, PARAM_SEP, 1);
            }

        params.set(TICameraParameters::KEY_SUPPORTED_IPP, supported);
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertWBModes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);
        const char *p;

        for ( unsigned int i = 0 ; i < caps.ulWhiteBalanceCount ; i++ )
            {
            p = getLUTvalue_OMXtoHAL(caps.eWhiteBalanceModes[i], WBalLUT);
            if ( NULL != p )
                {
                strncat(supported, p, MAX_PROP_NAME_LENGTH);
                strncat(supported, PARAM_SEP, 1);
                }
            }

        //These modes are not supported by the capability feature
        strncat(supported, TICameraParameters::WHITE_BALANCE_FACE, MAX_PROP_NAME_LENGTH);

        params.set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, supported);
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertEffects(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);
        const char *p;

        for ( unsigned int i = 0 ; i < caps.ulColorEffectCount; i++ )
            {
              p = getLUTvalue_OMXtoHAL(caps.eColorEffects[i], EffLUT);
            if ( NULL != p )
                {
                strncat(supported, p, MAX_PROP_NAME_LENGTH);
                strncat(supported, PARAM_SEP, 1);
                }
            }

        params.set(CameraParameters::KEY_SUPPORTED_EFFECTS, supported);
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertExpModes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];
    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);
        const char *p;

        for ( unsigned int i = 0 ; i < caps.ulExposureModeCount; i++ )
            {
            p = getLUTvalue_OMXtoHAL(caps.eExposureModes[i], ExpLUT);
            if ( NULL != p )
                {
                strncat(supported, p, MAX_PROP_NAME_LENGTH);
                strncat(supported, PARAM_SEP, 1);
                }
            }

        //These modes are not supported by the capability feature

        // Added face_priority support
        strncat(supported, TICameraParameters::EXPOSURE_MODE_FACE, MAX_PROP_NAME_LENGTH);

        // Added manual support
        strncat(supported, PARAM_SEP, 1);
        strncat(supported, "manual", MAX_PROP_NAME_LENGTH);

        params.set(TICameraParameters::KEY_SUPPORTED_EXPOSURE, supported);
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertSceneModes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);
        const char *p;

        for ( unsigned int i = 0 ; i < caps.ulSceneCount; i++ )
            {
            p = getLUTvalue_OMXtoHAL(caps.eSceneModes[i], SceneLUT);
            if ( NULL != p )
                {
                strncat(supported, p, MAX_PROP_NAME_LENGTH);
                strncat(supported, PARAM_SEP, 1);
                }
            }

        params.set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, supported);
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertFocusModes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);
        const char *p;

        for ( unsigned int i = 0 ; i < caps.ulFocusModeCount; i++ )
            {
            p = getLUTvalue_OMXtoHAL(caps.eFocusModes[i], FocusLUT);
            if ( NULL != p )
                {
                strncat(supported, p, MAX_PROP_NAME_LENGTH);
                strncat(supported, PARAM_SEP, 1);
                }
            }

        //These modes are not supported by the capability feature
        strncat(supported, TICameraParameters::FOCUS_MODE_FACE, MAX_PROP_NAME_LENGTH);
        strncat(supported, PARAM_SEP, 1);
        strncat(supported, TICameraParameters::FOCUS_MODE_TOUCH, MAX_PROP_NAME_LENGTH);

        params.set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, supported);
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertFlickerModes(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        memset(supported, '\0', MAX_PROP_VALUE_LENGTH);
        const char *p;

        for ( unsigned int i = 0 ; i < caps.ulFlickerCount; i++ )
            {
            p = getLUTvalue_OMXtoHAL(caps.eFlicker[i], FlickerLUT);
            if ( NULL != p )
                {
                strncat(supported, p, MAX_PROP_NAME_LENGTH);
                strncat(supported, PARAM_SEP, 1);
                }
            }

        params.set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, supported);
        }

    LOG_FUNCTION_NAME

    return ret;
}

status_t OMXCameraAdapter::insertCapabilities(CameraParameters &params, OMX_TI_CAPTYPE &caps)
{
    status_t ret = NO_ERROR;
    char supported[MAX_PROP_VALUE_LENGTH];

    LOG_FUNCTION_NAME

    if ( NO_ERROR == ret )
        {
        ret = insertImageSizes(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertPreviewSizes(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertThumbSizes(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertZoomStages(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertImageFormats(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertPreviewFormats(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertFramerates(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertEVs(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertISOModes(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertIPPModes(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertWBModes(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertEffects(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertExpModes(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertSceneModes(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertFocusModes(params, caps);
        }

    if ( NO_ERROR == ret )
        {
        ret = insertFlickerModes(params, caps);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

status_t OMXCameraAdapter::getOMXCaps(OMX_TI_CAPTYPE *caps)
{
    status_t ret = NO_ERROR;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_TI_CONFIG_SHAREDBUFFER sharedBuffer;

    LOG_FUNCTION_NAME

    if ( OMX_StateInvalid == mComponentState )
        {
        CAMHAL_LOGEA("OMX component is in invalid state");
        ret = -EINVAL;
        }

    if ( NULL == caps )
        {
        CAMHAL_LOGEA("Invalid arguments");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        OMX_INIT_STRUCT_PTR (caps, OMX_TI_CAPTYPE);
        caps->nPortIndex = OMX_ALL;

        OMX_INIT_STRUCT_PTR (&sharedBuffer, OMX_TI_CONFIG_SHAREDBUFFER);
        sharedBuffer.nPortIndex = OMX_ALL;
        sharedBuffer.nSharedBuffSize = sizeof(OMX_TI_CAPTYPE);
        sharedBuffer.pSharedBuff = ( OMX_U8 * ) caps;

        eError =  OMX_GetConfig(mCameraAdapterParameters.mHandleComp, ( OMX_INDEXTYPE ) OMX_TI_IndexConfigCamCapabilities, &sharedBuffer);
        if ( OMX_ErrorNone != eError )
            {
            CAMHAL_LOGEB("Error during capabilities query 0x%x", eError);
            }
        else
            {
            CAMHAL_LOGDA("OMX capability query success");
            }
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}


status_t OMXCameraAdapter::getCaps(CameraParameters &params)
{
    status_t ret = NO_ERROR;
    OMX_TI_CAPTYPE caps;

    LOG_FUNCTION_NAME

#ifdef OMX_CAP_TEST

    //For testing purposes

    caps.bISONoiseFilterSupported = OMX_TRUE;
    caps.bLensDistortionCorrectionSupported = OMX_FALSE;

    caps.ulImageFormatCount = 2;
    caps.eImageFormats[0] = OMX_COLOR_FormatCbYCrY;
    caps.eImageFormats[1] = OMX_COLOR_FormatRawBayer10bit;

    caps.ulPreviewFormatCount = 2;
    caps.ePreviewFormats[0] = OMX_COLOR_FormatCbYCrY;
    caps.ePreviewFormats[1] = OMX_COLOR_FormatYUV420SemiPlanar;

    caps.tImageResRange.nWidthMax = 4032;
    caps.tImageResRange.nHeightMax = 3024;
    caps.tImageResRange.nWidthMin = 320;
    caps.tImageResRange.nHeightMin = 240;

    caps.tPreviewResRange.nWidthMax = 1280;
    caps.tPreviewResRange.nHeightMax = 720;
    caps.tPreviewResRange.nWidthMin = 96;
    caps.tPreviewResRange.nHeightMin = 96;

    caps.tThumbResRange.nWidthMax = 640;
    caps.tThumbResRange.nHeightMax = 480;
    caps.tThumbResRange.nWidthMin = 96;
    caps.tThumbResRange.nHeightMin = 96;

    caps.xFramerateMax = 30 << 16;
    caps.xFramerateMin = 5 << 16;

    caps.xEVCompensationMax = 2 << 16;
    caps.xEVCompensationMin = -2 << 16;

    caps.xMaxWidthZoom = 400000;
    caps.xMaxHeightZoom = 400000;

    caps.ulColorEffectCount = 3;
    caps.eColorEffects[0] = ( OMX_IMAGEFILTERTYPE ) OMX_ImageFilterGrayScale;
    caps.eColorEffects[1] = ( OMX_IMAGEFILTERTYPE ) OMX_ImageFilterSepia;
    caps.eColorEffects[2] = ( OMX_IMAGEFILTERTYPE ) OMX_ImageFilterNatural;

    caps.ulExposureModeCount = 2;
    caps.eExposureModes[0] = OMX_ExposureControlAuto;
    caps.eExposureModes[1] = OMX_ExposureControlNight;

    caps.ulFlickerCount = 1;
    caps.eFlicker[0] = OMX_FlickerCancelAuto;

    caps.ulFocusModeCount = 2;
    caps.eFocusModes[0] = OMX_IMAGE_FocusControlAutoLock;
    caps.eFocusModes[1] = ( OMX_IMAGE_FOCUSCONTROLTYPE ) OMX_IMAGE_FocusControlAutoInfinity;

    caps.ulSceneCount = 3;
    caps.eSceneModes[0] = OMX_Manual;
    caps.eSceneModes[1] = OMX_Closeup;
    caps.eSceneModes[2] = OMX_Landscape;

    caps.ulWhiteBalanceCount = 1;
    caps.eWhiteBalanceModes[0] = OMX_WhiteBalControlFluorescent;

    caps.nSensitivityMax = 1000;

#else

    if ( NO_ERROR == ret )
        {
        ret = getOMXCaps(&caps);
        }

#endif

    if ( NO_ERROR == ret )
        {
        ret = insertCapabilities(params, caps);
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

};


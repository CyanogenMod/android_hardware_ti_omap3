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



#ifndef CAMERA_PROPERTIES_H
#define CAMERA_PROPERTIES_H

#include <utils/KeyedVector.h>
#include <utils/String8.h>
#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/xmlreader.h>
#include <libxml/tree.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>


namespace android {

#define MAX_CAMERAS_SUPPORTED 6
#define MAX_PROP_NAME_LENGTH 50
#define MAX_PROP_VALUE_LENGTH 256


///Class that handles the Camera Properties
class CameraProperties : public RefBase
{
public:
    enum CameraPropertyIndex
        {
        PROP_INDEX_INVALID = 0,
        PROP_INDEX_CAMERA_NAME,
        PROP_INDEX_CAMERA_ADAPTER_DLL_NAME,
        PROP_INDEX_CAMERA_SENSOR_INDEX,
        PROP_INDEX_SUPPORTED_PREVIEW_SIZES,
        PROP_INDEX_SUPPORTED_PREVIEW_FORMATS,
        PROP_INDEX_SUPPORTED_PREVIEW_FRAME_RATES,
        PROP_INDEX_SUPPORTED_PICTURE_SIZES,
        PROP_INDEX_SUPPORTED_PICTURE_FORMATS,
        PROP_INDEX_SUPPORTED_THUMBNAIL_SIZES,
        PROP_INDEX_SUPPORTED_WHITE_BALANCE,
        PROP_INDEX_SUPPORTED_EFFECTS,
        PROP_INDEX_SUPPORTED_ANTIBANDING,
        PROP_INDEX_SUPPORTED_EXPOSURE_MODES,
        PROP_INDEX_SUPPORTED_EV_MAX,
        PROP_INDEX_SUPPORTED_EV_MIN,
        PROP_INDEX_SUPPORTED_EV_STEP,
        PROP_INDEX_SUPPORTED_ISO_VALUES,
        PROP_INDEX_SUPPORTED_SCENE_MODES,
        PROP_INDEX_SUPPORTED_FLASH_MODES,
        PROP_INDEX_SUPPORTED_FOCUS_MODES,
        PROP_INDEX_SUPPORTED_IPP_MODES,
         PROP_INDEX_REQUIRED_PREVIEW_BUFS,
        PROP_INDEX_REQUIRED_IMAGE_BUFS,
        PROP_INDEX_SUPPORTED_ZOOM_RATIOS,
        PROP_INDEX_SUPPORTED_ZOOM_STAGES,
        PROP_INDEX_ZOOM_SUPPORTED,
        PROP_INDEX_SMOOTH_ZOOM_SUPPORTED,
        PROP_INDEX_PREVIEW_SIZE,
        PROP_INDEX_PREVIEW_FORMAT,
        PROP_INDEX_PREVIEW_FRAME_RATE,
        PROP_INDEX_ZOOM,
        PROP_INDEX_PICTURE_SIZE,
        PROP_INDEX_PICTURE_FORMAT,
        PROP_INDEX_JPEG_THUMBNAIL_SIZE,
        PROP_INDEX_WHITEBALANCE,
        PROP_INDEX_EFFECT,
        PROP_INDEX_ANTIBANDING,
        PROP_INDEX_EXPOSURE_MODE,
        PROP_INDEX_EV_COMPENSATION,
        PROP_INDEX_ISO_MODE,
        PROP_INDEX_FOCUS_MODE,
        PROP_INDEX_SCENE_MODE,
        PROP_INDEX_FLASH_MODE,
        PROP_INDEX_JPEG_QUALITY,
        PROP_INDEX_CONTRAST,
        PROP_INDEX_SATURATION,
        PROP_INDEX_BRIGHTNESS,
        PROP_INDEX_SHARPNESS,
        PROP_INDEX_IPP,
        PROP_INDEX_S3D_SUPPORTED,
        PROP_INDEX_MAX
        };

    static const char PROP_KEY_INVALID[];
    static const char PROP_KEY_CAMERA_NAME[];
    static const char PROP_KEY_ADAPTER_DLL_NAME[];
    static const char PROP_KEY_CAMERA_SENSOR_INDEX[];
    static const char PROP_KEY_S3D_SUPPORTED[];
    static const char PROP_KEY_SUPPORTED_PREVIEW_SIZES[];
    static const char PROP_KEY_SUPPORTED_PREVIEW_FORMATS[];
    static const char PROP_KEY_SUPPORTED_PREVIEW_FRAME_RATES[];
    static const char PROP_KEY_SUPPORTED_PICTURE_SIZES[];
    static const char PROP_KEY_SUPPORTED_PICTURE_FORMATS[];
    static const char PROP_KEY_SUPPORTED_THUMBNAIL_SIZES[];
    static const char PROP_KEY_SUPPORTED_WHITE_BALANCE[];
    static const char PROP_KEY_SUPPORTED_EFFECTS[];
    static const char PROP_KEY_SUPPORTED_ANTIBANDING[];
    static const char PROP_KEY_SUPPORTED_EXPOSURE_MODES[];
    static const char PROP_KEY_SUPPORTED_EV_MIN[];
    static const char PROP_KEY_SUPPORTED_EV_MAX[];
    static const char PROP_KEY_SUPPORTED_EV_STEP[];
    static const char PROP_KEY_SUPPORTED_ISO_VALUES[];
    static const char PROP_KEY_SUPPORTED_SCENE_MODES[];
    static const char PROP_KEY_SUPPORTED_FLASH_MODES[];
    static const char PROP_KEY_SUPPORTED_FOCUS_MODES[];
    static const char PROP_KEY_REQUIRED_PREVIEW_BUFS[];
    static const char PROP_KEY_REQUIRED_IMAGE_BUFS[];
    static const char PROP_KEY_SUPPORTED_ZOOM_RATIOS[];
    static const char PROP_KEY_SUPPORTED_ZOOM_STAGES[];
    static const char PROP_KEY_SUPPORTED_IPP_MODES[];
    static const char PROP_KEY_SMOOTH_ZOOM_SUPPORTED[];
    static const char PROP_KEY_ZOOM_SUPPORTED[];
    static const char PROP_KEY_PREVIEW_SIZE[];
    static const char PROP_KEY_PREVIEW_FORMAT[];
    static const char PROP_KEY_PREVIEW_FRAME_RATE[];
    static const char PROP_KEY_ZOOM[];
    static const char PROP_KEY_PICTURE_SIZE[];
    static const char PROP_KEY_PICTURE_FORMAT[];
    static const char PROP_KEY_JPEG_THUMBNAIL_SIZE[];
    static const char PROP_KEY_WHITEBALANCE[];
    static const char PROP_KEY_EFFECT[];
    static const char PROP_KEY_ANTIBANDING[];
    static const char PROP_KEY_EXPOSURE_MODE[];
    static const char PROP_KEY_EV_COMPENSATION[];
    static const char PROP_KEY_ISO_MODE[];
    static const char PROP_KEY_FOCUS_MODE[];
    static const char PROP_KEY_SCENE_MODE[];
    static const char PROP_KEY_FLASH_MODE[];
    static const char PROP_KEY_JPEG_QUALITY[];
    static const char PROP_KEY_BRIGHTNESS[];
    static const char PROP_KEY_SATURATION[];
    static const char PROP_KEY_SHARPNESS[];
    static const char PROP_KEY_CONTRAST[];
    static const char PROP_KEY_IPP[];
    static const char PARAMS_DELIMITER [];

    static const char TICAMERA_FILE_PREFIX[];
    static const char TICAMERA_FILE_EXTN[];

    class CameraProperty
        {
        public:
            CameraProperty(const char *propName, const char *propValue)
                {
                strcpy(mPropName, propName);
                strcpy(mPropValue, propValue);
                }
            status_t setValue(const char * value);

        public:
            char mPropName[MAX_PROP_NAME_LENGTH];
            char mPropValue[MAX_PROP_VALUE_LENGTH];

        };

    CameraProperties();
    ~CameraProperties();

    ///Initializes the CameraProperties class
    status_t initialize();
    status_t loadProperties();
    int camerasSupported();
    CameraProperty** getProperties(int cameraIndex);



private:
    status_t parseAndLoadProps(const char* file);
    status_t parseCameraElements(xmlTextReaderPtr &reader);
    status_t freeCameraProps(int cameraIndex);
    status_t createPropertiesArray(CameraProperties::CameraProperty** &cameraProps);
    CameraProperties::CameraPropertyIndex getCameraPropertyIndex(const char* propName);
    const char* getCameraPropertyKey(CameraProperties::CameraPropertyIndex index);
    void refreshProperties();


private:
    int32_t mCamerasSupported;
    CameraProperty *mCameraProps[MAX_CAMERAS_SUPPORTED][CameraProperties::PROP_INDEX_MAX];

};

};

#endif //CAMERA_PROPERTIES_H


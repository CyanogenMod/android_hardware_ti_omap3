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
#include <libxml/xmlwriter.h>
#include <libxml/tree.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>


namespace android {

#define MAX_CAMERAS_SUPPORTED 6
#define MAX_PROP_NAME_LENGTH 50
#define MAX_PROP_VALUE_LENGTH 2048


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
        PROP_INDEX_ORIENTATION_INDEX,
        PROP_INDEX_FACING_INDEX,
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
        PROP_INDEX_S3D2D_PREVIEW,
        PROP_INDEX_S3D2D_PREVIEW_MODES,
        PROP_INDEX_AUTOCONVERGENCE,
        PROP_INDEX_AUTOCONVERGENCE_MODE,
        PROP_INDEX_MANUALCONVERGENCE_VALUES,
        PROP_INDEX_VSTAB,
        PROP_INDEX_VSTAB_VALUES,
        PROP_INDEX_FRAMERATE_RANGE,
        PROP_INDEX_FRAMERATE_RANGE_SUPPORTED,
        PROP_INDEX_SENSOR_ORIENTATION,
        PROP_INDEX_SENSOR_ORIENTATION_VALUES,
        PROP_INDEX_REVISION,
        PROP_INDEX_FOCAL_LENGTH,
        PROP_INDEX_HOR_ANGLE,
        PROP_INDEX_VER_ANGLE,
        PROP_INDEX_EXIF_MAKE,
        PROP_INDEX_EXIF_MODEL,
        PROP_INDEX_JPEG_THUMBNAIL_QUALITY,
        PROP_INDEX_S3D_FRAME_LAYOUT,
        PROP_INDEX_S3D_FRAME_LAYOUT_VALUES,
        PROP_INDEX_MAX
        };

    static const char INVALID[];
    static const char CAMERA_NAME[];
    static const char ADAPTER_DLL_NAME[];
    static const char CAMERA_SENSOR_INDEX[];
    static const char ORIENTATION_INDEX[];
    static const char FACING_INDEX[];
    static const char S3D_SUPPORTED[];
    static const char SUPPORTED_PREVIEW_SIZES[];
    static const char SUPPORTED_PREVIEW_FORMATS[];
    static const char SUPPORTED_PREVIEW_FRAME_RATES[];
    static const char SUPPORTED_PICTURE_SIZES[];
    static const char SUPPORTED_PICTURE_FORMATS[];
    static const char SUPPORTED_THUMBNAIL_SIZES[];
    static const char SUPPORTED_WHITE_BALANCE[];
    static const char SUPPORTED_EFFECTS[];
    static const char SUPPORTED_ANTIBANDING[];
    static const char SUPPORTED_EXPOSURE_MODES[];
    static const char SUPPORTED_EV_MIN[];
    static const char SUPPORTED_EV_MAX[];
    static const char SUPPORTED_EV_STEP[];
    static const char SUPPORTED_ISO_VALUES[];
    static const char SUPPORTED_SCENE_MODES[];
    static const char SUPPORTED_FLASH_MODES[];
    static const char SUPPORTED_FOCUS_MODES[];
    static const char REQUIRED_PREVIEW_BUFS[];
    static const char REQUIRED_IMAGE_BUFS[];
    static const char SUPPORTED_ZOOM_RATIOS[];
    static const char SUPPORTED_ZOOM_STAGES[];
    static const char SUPPORTED_IPP_MODES[];
    static const char SMOOTH_ZOOM_SUPPORTED[];
    static const char ZOOM_SUPPORTED[];
    static const char PREVIEW_SIZE[];
    static const char PREVIEW_FORMAT[];
    static const char PREVIEW_FRAME_RATE[];
    static const char ZOOM[];
    static const char PICTURE_SIZE[];
    static const char PICTURE_FORMAT[];
    static const char JPEG_THUMBNAIL_SIZE[];
    static const char WHITEBALANCE[];
    static const char EFFECT[];
    static const char ANTIBANDING[];
    static const char EXPOSURE_MODE[];
    static const char EV_COMPENSATION[];
    static const char ISO_MODE[];
    static const char FOCUS_MODE[];
    static const char SCENE_MODE[];
    static const char FLASH_MODE[];
    static const char JPEG_QUALITY[];
    static const char BRIGHTNESS[];
    static const char SATURATION[];
    static const char SHARPNESS[];
    static const char CONTRAST[];
    static const char IPP[];
    static const char AUTOCONVERGENCE[];
    static const char AUTOCONVERGENCE_MODE[];
    static const char MANUALCONVERGENCE_VALUES[];
    static const char SENSOR_ORIENTATION[];
    static const char SENSOR_ORIENTATION_VALUES[];
    static const char REVISION[];
    static const char FOCAL_LENGTH[];
    static const char HOR_ANGLE[];
    static const char VER_ANGLE[];
    static const char EXIF_MAKE[];
    static const char EXIF_MODEL[];
    static const char JPEG_THUMBNAIL_QUALITY[];

    static const char PARAMS_DELIMITER [];

    static const char TICAMERA_FILE_PREFIX[];
    static const char TICAMERA_FILE_EXTN[];
    static const char S3D2D_PREVIEW[];
    static const char S3D2D_PREVIEW_MODES[];
    static const char VSTAB[];
    static const char VSTAB_VALUES[];
    static const char FRAMERATE_RANGE[];
    static const char FRAMERATE_RANGE_SUPPORTED[];
    static const char S3D_FRAME_LAYOUT[];
    static const char S3D_FRAME_LAYOUT_VALUES[];

    class CameraProperty
        {
        public:
            CameraProperty(const char *propName, const char *propValue)
                {
                  strncpy(mPropName, propName, sizeof(mPropName)-1);
                  strncpy(mPropValue, propValue, sizeof(mPropValue)-1);
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
    status_t storeProperties();
    int camerasSupported();
    CameraProperty** getProperties(int cameraIndex);

private:
    status_t parseAndLoadProps(const char* file);
    status_t parseCameraElements(xmlTextReaderPtr &reader);
    status_t storeCameraElements(xmlTextWriterPtr &writer);
    status_t freeCameraProps(int cameraIndex);
    status_t createPropertiesArray(CameraProperties::CameraProperty** &cameraProps);
    CameraProperties::CameraPropertyIndex getCameraPropertyIndex(const char* propName);
    const char* getCameraPropertyKey(CameraProperties::CameraPropertyIndex index);
    void refreshProperties();

    ////Choosing 268 because "system/etc/"+ dir_name(0...255) can be max 268 chars
    char mXMLFullPath[268];

    int32_t mCamerasSupported;
    CameraProperty *mCameraProps[MAX_CAMERAS_SUPPORTED][CameraProperties::PROP_INDEX_MAX];

};

};

#endif //CAMERA_PROPERTIES_H


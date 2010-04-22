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
        PROP_INDEX_SUPPORTED_PREVIEW_SIZES,
        PROP_INDEX_SUPPORTED_PREVIEW_FORMATS,
        PROP_INDEX_SUPPORTED_PREVIEW_FRAME_RATES,
        PROP_INDEX_SUPPORTED_PICTURE_SIZES,
        PROP_INDEX_SUPPORTED_PICTURE_FORMATS,
        PROP_INDEX_SUPPORTED_THUMBNAIL_SIZES,
        PROP_INDEX_SUPPORTED_WHITE_BALANCE,
        PROP_INDEX_SUPPORTED_EFFECTS,
        PROP_INDEX_SUPPORTED_ANTIBANDING,
        PROP_INDEX_SUPPORTED_SCENE_MODES,
        PROP_INDEX_SUPPORTED_FLASH_MODES,
        PROP_INDEX_SUPPORTED_FOCUS_MODES,
        PROP_INDEX_REQUIRED_PREVIEW_BUFS,
        PROP_INDEX_REQUIRED_IMAGE_BUFS  ,
        PROP_INDEX_MAX
        };

    static const char PROP_KEY_INVALID[];
    static const char PROP_KEY_CAMERA_NAME[];
    static const char PROP_KEY_ADAPTER_DLL_NAME[];
    static const char PROP_KEY_SUPPORTED_PREVIEW_SIZES[];
    static const char PROP_KEY_SUPPORTED_PREVIEW_FORMATS[];
    static const char PROP_KEY_SUPPORTED_PREVIEW_FRAME_RATES[];
    static const char PROP_KEY_SUPPORTED_PICTURE_SIZES[];
    static const char PROP_KEY_SUPPORTED_PICTURE_FORMATS[];
    static const char PROP_KEY_SUPPORTED_THUMBNAIL_SIZES[];
    static const char PROP_KEY_SUPPORTED_WHITE_BALANCE[];
    static const char PROP_KEY_SUPPORTED_EFFECTS[];
    static const char PROP_KEY_SUPPORTED_ANTIBANDING[];
    static const char PROP_KEY_SUPPORTED_SCENE_MODES[];
    static const char PROP_KEY_SUPPORTED_FLASH_MODES[];
    static const char PROP_KEY_SUPPORTED_FOCUS_MODES[];
    static const char PROP_KEY_REQUIRED_PREVIEW_BUFS[];
    static const char PROP_KEY_REQUIRED_IMAGE_BUFS[];
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


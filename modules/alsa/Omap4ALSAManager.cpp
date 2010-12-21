/* Omap4ALSAManager.cpp
 **
 ** Copyright 2011-2012 Texas Instruments
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

//includes
#include "Omap4ALSAManager.h"

namespace android {

const char *Omap4ALSAManager::MAIN_MIC = "omap.audio.mic.main";
const char *Omap4ALSAManager::SUB_MIC = "omap.audio.mic.sub";
const char *Omap4ALSAManager::POWER_MODE = "omap.audio.power";

/* below are not implemented/used yet */
const char *Omap4ALSAManager::DL2L_EQ_PROFILE = "omap.audio.dl2l.eq";
const char *Omap4ALSAManager::DL2R_EQ_PROFILE = "omap.audio.dl2r.eq";
const char *Omap4ALSAManager::DL1_EQ_PROFILE = "omap.audio.dl1.eq";
const char *Omap4ALSAManager::AMIC_EQ_PROFILE = "omap.audio.amic.eq";
const char *Omap4ALSAManager::DMIC_EQ_PROFILE = "omap.audio.dmic.eq";


const char  *Omap4ALSAManager::MicNameList[]= {
    "AMic0",  // for Analog Main mic
    "AMic1",  //  for Analog Sub mic
    "DMic0L", // for Digital Left mic 0
    "DMic1L", // for Digital Left mic 1
    "DMic2L", // for Digital Left mic 2
    "DMic0R", // for Digital Right mic 0
    "DMic1R", // for Digital Right mic 1
    "DMic2R", // for Digital Right mic 2
    "eof"
};

const char  *Omap4ALSAManager::PowerModeList[] = {
    "FIFO",  // low latency 4 periods per buffer
    "PingPong",  // low power ping pong using MM_LP_DEVICE
    "eof"
};

const char  *Omap4ALSAManager::EqualizerProfileList[] = {
    "Flat response",  // all-pass filter
    "High-pass 0dB",
    "High-pass -12dB",
    "High-pass -20dB",
    "eof"
};

status_t Omap4ALSAManager::set(const String8& key, const String8& value)
{
    String8 temp = value;
    if (validateValueForKey(key, temp) == NO_ERROR) {
        LOGV("set by Value:: %s::%s", key.string(), value.string());
        if (mParams.indexOfKey(key) < 0) {
            mParams.add(key, value);
            return NO_ERROR;
        } else {
            mParams.replaceValueFor(key, value);
            return ALREADY_EXISTS;
        }
    }
    return BAD_VALUE;
}

status_t Omap4ALSAManager::setFromProperty(const String8& key) {
    status_t status = NO_ERROR;
    char value[PROPERTY_VALUE_MAX];

    // assumes that the value is valid
    if (property_get(key.string(), value, ""))
    {
        LOGV("setFromProperty:: %s::%s", key.string(), value);
        String8 temp = String8(value);
        if (validateValueForKey(key, temp) == NO_ERROR) {
            mParams.add(key, (String8)value);
            return NO_ERROR;
        }
    }
    return BAD_VALUE;
}

status_t Omap4ALSAManager::get(const String8& key, String8& value)
{
    if (mParams.indexOfKey(key) >= 0) {
        value = mParams.valueFor(key);
        return NO_ERROR;
    } else {
        return BAD_VALUE;
    }
}

status_t Omap4ALSAManager::validateValueForKey(const String8& key, String8& value)
{
    int noMatch = 1;
    int i = 0;

    if (key == (String8)MAIN_MIC) {
        LOGV("validate main mic");

        if ((mParams.indexOfKey((String8)SUB_MIC) >= 0) &&
           (value == mParams.valueFor((String8)SUB_MIC))) {
            // don't allow same mic for main and sub
            return BAD_VALUE;
        } else {
            while (noMatch && strcmp(MicNameList[i], "eof")) {
                noMatch = strcmp(MicNameList[i], value.string());
                if (noMatch) i++;
                else break;
            }
            if(noMatch)
                return BAD_VALUE;
            else
                return NO_ERROR;
        }
    }
    else if (key == (String8)SUB_MIC) {
        LOGV("validate SUB mic");
        if ((mParams.indexOfKey((String8)MAIN_MIC) >= 0) &&
           (value == mParams.valueFor((String8)MAIN_MIC))) {
            // don't allow same mic for main and sub
            return BAD_VALUE;
        } else {
            while (noMatch && strcmp(MicNameList[i], "eof")) {
                noMatch = strcmp(MicNameList[i], value.string());
                if (noMatch) i++;
                else break;
            }
            if(noMatch)
                return BAD_VALUE;
            else
                return NO_ERROR;
        }
    }
    else if (key == (String8)POWER_MODE) {
        LOGV("validate power mode");
        while (noMatch && strcmp(PowerModeList[i], "eof")) {
                noMatch = strcmp(PowerModeList[i], value.string());
                if (noMatch) i++;
                else break;
            }
            if(noMatch)
                return BAD_VALUE;
            else
                return NO_ERROR;
    }
    else {
        // @TODO: add constraints as required
        return NO_ERROR;
    }
}

status_t Omap4ALSAManager::remove(const String8& key)
{
    if (mParams.indexOfKey(key) >= 0) {
        mParams.removeItem(key);
        return NO_ERROR;
    } else {
        return BAD_VALUE;
    }
}

Omap4ALSAManager::~Omap4ALSAManager()
{
    mParams.clear();
}

}; //namespace Android

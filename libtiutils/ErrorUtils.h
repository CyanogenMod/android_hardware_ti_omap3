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
#ifndef ERROR_UTILS_H
#define ERROR_UTILS_H

///Header file where all the android error codes are defined
#include <Errors.h>

///Header file where all the OMX error codes are defined
#include "OMX_Core.h"


extern "C"
{
///Header file where all the TI OSAL error codes are defined
#include "timm_osal_error.h"
};

namespace android {

///Generic class with static methods to convert any standard error type to Android error type
class ErrorUtils
{
public:
    ///Method to convert from POSIX to Android errors
    static status_t posixToAndroidError(int error);

    ///Method to convert from TI OSAL to Android errors
    static status_t osalToAndroidError(TIMM_OSAL_ERRORTYPE error);

    ///Method to convert from OMX to Android errors
    static status_t omxToAndroidError(OMX_ERRORTYPE error);

};

};

#endif /// ERROR_UTILS_H

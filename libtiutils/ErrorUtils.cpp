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

#include "ErrorUtils.h"

namespace android {

/**
   @brief Method to convert from POSIX to Android errors

   @param error Any of the standard POSIX error codes (defined in bionic/libc/kernel/common/asm-generic/errno.h)
   @return Any of the standard Android error code (defined in frameworks/base/include/utils/Errors.h)
 */
status_t ErrorUtils::posixToAndroidError(int error)
{
    switch(error)
        {
        case 0:
            return NO_ERROR;
        case EINVAL:
        case EFBIG:
        case EMSGSIZE:
        case E2BIG:
        case EFAULT:
        case EILSEQ:
            return BAD_VALUE;
        case ENOSYS:
            return INVALID_OPERATION;
        case EACCES:
        case EPERM:
            return PERMISSION_DENIED;
        case EADDRINUSE:
        case EAGAIN:
        case EALREADY:
        case EBUSY:
        case EEXIST:
        case EINPROGRESS:
            return ALREADY_EXISTS;
        case ENOMEM:
            return NO_MEMORY;
        default:
            return UNKNOWN_ERROR;
        };

    return NO_ERROR;
}


/**
   @brief Method to convert from TI OSAL to Android errors

   @param error Any of the standard TI OSAL error codes (defined in
                                                                                            hardware/ti/omx/ducati/domx/system/mm_osal/inc/timm_osal_error.h)
   @return Any of the standard Android error code  (defined in frameworks/base/include/utils/Errors.h)
 */
status_t ErrorUtils::osalToAndroidError(TIMM_OSAL_ERRORTYPE error)
{
    switch(error)
    {
    case TIMM_OSAL_ERR_NONE:
        return NO_ERROR;
    case TIMM_OSAL_ERR_ALLOC:
        return NO_MEMORY;
    default:
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

/**
   @brief Method to convert from OMX to Android errors

   @param error Any of the standard OMX error codes (defined in hardware/ti/omx/ducati/domx/system/omx_core/inc/OMX_Core.h)
   @return Any of the standard Android error code (defined in frameworks/base/include/utils/Errors.h)
 */
status_t ErrorUtils::omxToAndroidError(OMX_ERRORTYPE error)
{
    switch(error)
        {
        case OMX_ErrorNone:
            return NO_ERROR;
        case OMX_ErrorBadParameter:
            return BAD_VALUE;
        default:
            return UNKNOWN_ERROR;
        }

    return NO_ERROR;
}


};




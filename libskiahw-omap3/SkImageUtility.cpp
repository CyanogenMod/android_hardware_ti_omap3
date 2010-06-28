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
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */

#include "SkImageUtility.h"

OMX_COLOR_FORMATTYPE SkBitmapToOMXColorFormat(SkBitmap::Config config)
{
    OMX_COLOR_FORMATTYPE colorFormat;
    if (config == SkBitmap::kNo_Config)
        colorFormat = OMX_COLOR_FormatCbYCrY;
    else if (config == SkBitmap::kRGB_565_Config)
        colorFormat = OMX_COLOR_Format16bitRGB565;
    else if (config == SkBitmap::kA8_Config)
        colorFormat = OMX_COLOR_FormatL8;
    else if (config == SkBitmap::kARGB_8888_Config)
        colorFormat = OMX_COLOR_Format32bitARGB8888;
    else // default
        colorFormat = OMX_COLOR_FormatCbYCrY;

    return colorFormat;
}

OMX_COLOR_FORMATTYPE JPEGToOMXColorFormat(int format)
{
    OMX_COLOR_FORMATTYPE colorFormat;

    switch (format) {
        case OMX_COLOR_FormatYCbYCr:
        case OMX_COLOR_FormatYUV444Interleaved:
        case OMX_COLOR_FormatUnused:
            colorFormat = OMX_COLOR_FormatCbYCrY;
            break;
        default:
            colorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
    }

    return colorFormat;
}



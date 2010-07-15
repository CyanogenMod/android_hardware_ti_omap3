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


#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

///Defines for debug statements - Macro LOG_TAG needs to be defined in the respective files
#define DBGUTILS_LOGVA(str)         LOGV("%s:%d %s - " str,__FILE__, __LINE__,__FUNCTION__);
#define DBGUTILS_LOGVB(str,...)    LOGV("%s:%d %s - " str,__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);
#define DBGUTILS_LOGDA(str)         LOGD("%s:%d %s - " str,__FILE__, __LINE__,__FUNCTION__);
#define DBGUTILS_LOGDB(str, ...)    LOGD("%s:%d %s - " str,__FILE__, __LINE__, __FUNCTION__, __VA_ARGS__);
#define DBGUTILS_LOGEA(str)         LOGE("%s:%d %s - " str,__FILE__, __LINE__, __FUNCTION__);
#define DBGUTILS_LOGEB(str, ...)    LOGE("%s:%d %s - " str,__FILE__, __LINE__,__FUNCTION__, __VA_ARGS__);
#define LOG_FUNCTION_NAME           LOGE("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT      LOGE("%d: %s() EXIT", __LINE__, __FUNCTION__);




#endif //DEBUG_UTILS_H


/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/** 
 *  \file   pla_defs.h 
 *  \brief  PLA Linux OS dependent definitions and includes file
 * 
 * This file contains Linux OS specific definitions and H files
 * 
 *  \see    pla_os_types.h
 */

#ifndef PLA_DEFS_H
#define PLA_DEFS_H

#ifdef _BT_APP
#include "server_wrap_common.h"

#define PLA_DEFS_GET_BTL_CALLBACK BTBUS_COMMON_BTL_callback
#endif 
#define PATCHFILE_PATH "patch-X.0.ce"
#define CONFIGFILE_PATH "/system/bin/GPSCConfigFile.cfg"

#endif /* PLA_DEFS_H */



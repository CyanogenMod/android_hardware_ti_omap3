/******************************************************************************\
##                                                                            *
## Unpublished Proprietary and Confidential Information of Texas Instruments  *
## Israel Ltd. Do Not Disclose.                                               *
## Copyright 2008 Texas Instruments Israel Ltd.                               *
## All rights reserved. All unpublished rights reserved.                      *
##                                                                            *
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

/*******************************************************************************\
*
*   FILE NAME:      FMTIS_version.h
*
*   BRIEF:          This file contains the FMTIS software version information
*
*   DESCRIPTION:    This file contains the FMTIS software version information
*
*                   The FMTIS version numbering scheme is defined as follows:
*
*                       FMTIS (Letter)X.YZ.B
*
*                           (Letter) - Target OS (Same convention as CS): W - Windows, L - Linux.
*
*                           X = Major ESI stack release
*
*                           Y = Major API release (first release = 0)
*
*                           Z = Additional features / profiles (first release = 0)
*
*                           B = Bug fixes (first release = 0)
*
*   AUTHOR:         Malovany Ram
*
\*******************************************************************************/

#ifndef FMTI_VERSION_H
#define FMTI_VERSION_H

#include "FMTIS_version_common.h"


/* FMTIS Target OS */
#define FMTIS_TARGET_OS                    (FMTIS_OS_LINUX)

/* Anchor Platform */
#define FMTIS_ANCHOR_VERSION_X             (PLATFORM_ANDROID_ZOOM3)

#endif /* FMTI_VERSION_H */


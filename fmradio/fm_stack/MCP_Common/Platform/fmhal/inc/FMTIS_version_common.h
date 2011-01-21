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
*   FILE NAME:      FMTIS_version_common.h
*
*   BRIEF:          This file contains the FMTIS common software version information
*
*   DESCRIPTION:    This file contains the FMTIS common software version information
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

#ifndef FMTI_VERSION_COMMON_H
#define FMTI_VERSION_COMMON_H


/********************************************************************************
 *
 * Constants
 *
 *******************************************************************************/


/*---------------------------------------------------------------------------
 * Target OS Types
 *      
 *  FMTIS_OS_WIN       "W" - Windows
 *  FMTIS_OS_LINUX     "L" - LINUX
 *  
 */

/* 
 * Target OS Types
 * Target OS type - Windows PC 
 */
#define FMTIS_OS_WIN                   "W"

/* 
 * Target OS Types
 * Target OS type - Linux 
 */
#define FMTIS_OS_LINUX                 "L"

/* Platform Types */
#define PLATFORM_WINDOWS                        (1)
#define PLATFORM_ANDROID_ZOOM2                  (2)
#define PLATFORM_ANDROID_ZOOM3                  (3)

/*******************************************************************************\
 *                                                                              *
 * FMTIS Release:   FMTIS 2.00.5 29.07.10 BUILD NUMBER 3                        *
 *                                                                              *
 * Details:         FMTIS 2.00.5                                                *
 *                                                                              *
 *                                                                              *
\*******************************************************************************/


/* FMTIS Major stack version */
#define FMTIS_SOFTWARE_VERSION_X               (2)

/* FMTIS Major API version */
#define FMTIS_SOFTWARE_VERSION_Y               (0)

/* FMTIS Additional features / profiles */
#define FMTIS_SOFTWARE_VERSION_Z               (0)

/* FMTIS Bug Fixes */
#define FMTIS_SOFTWARE_VERSION_B               (5)

/* FMTIS_Build number */
#define FMTIS_SOFTWARE_BUILD_NUMBER             (3)

/* FMTIS version day */
#define FMTIS_SOFTWARE_VERSION_DAY              (29)

/* FMTIS version month */
#define FMTIS_SOFTWARE_VERSION_MONTH            (7)

/* FMTIS version day year */
#define FMTIS_SOFTWARE_VERSION_YEAR             (2010)

#endif /* FMTI_VERSION_COMMON_H */


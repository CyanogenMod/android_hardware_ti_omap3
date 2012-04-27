/*
 *  Copyright 2001-2008 Texas Instruments - http://www.ti.com/
 * 
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


#ifndef _DLCLASSES_HDR_H
#define _DLCLASSES_HDR_H

/*****************************************************************************
 *****************************************************************************
 *
 *                          DLCLASSES_HDR.H
 *
 * Sample classes in support of the dynamic loader
 *
 * These are just concrete derivations of the virtual ones in dynamic_loader.h
 * with a few additional interfaces for init, etc.
 *****************************************************************************
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include "dynamic_loader.h"

#include "DLstream.h"
#include "DLsymtab.h"
#include "DLalloc.h"
#include "DLinit.h"

#ifdef __cplusplus
}
#endif
#endif				/* _DLCLASSES_HDR_H */


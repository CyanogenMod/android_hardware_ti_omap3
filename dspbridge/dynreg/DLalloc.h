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


#ifndef _DLALLOC_H
#define _DLALLOC_H

/*****************************************************************************
 *****************************************************************************
 *
 *                          DLALLOC.H
 *
 * A class used by the dynamic loader to allocate and deallocate target memory.
 *
 *****************************************************************************
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include "dynamic_loader.h"

/* DL_alloc_t mimics DL memory allocation type */
/* Users can customize if desired */
	/*typedef Dynamic_Loader_Allocate DL_alloc_t;*/

/*****************************************************************************
 * NOTE:    All methods take a 'thisptr' parameter that is a pointer to the
 *          environment for the DL Memory Allocator APIs.  See definition of 
 *          DL_alloc_t.
 *
 *****************************************************************************/

/******************************************************************************
* DLalloc_Init
*
* Parameters:
*   None
*
* Effect:
*   Initialize the handlers for the Allocation Class
*******************************************************************************/
	void DLalloc_init(struct Dynamic_Loader_Allocate * thisptr);

#ifdef __cplusplus
}
#endif
#endif /*_DLALLOC_H */

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


#ifndef _DLINIT_H
#define _DLINIT_H

/*****************************************************************************
 *****************************************************************************
 *
 *                              DLINIT.H    
 *
 * A class used by the dynamic loader to load data into a target.  This class
 * provides the interface-specific functions needed to load data.
 *
 *****************************************************************************
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include "dynamic_loader.h"

/* DL_init_t remembers the start address of the dynamic module in the DL     */
/* memory initializer class                                                  */

	struct DL_init_t {
		struct Dynamic_Loader_Initialize init;
		LDR_ADDR start_address;
	} ;

/*****************************************************************************
 * NOTE:    All methods take a 'thisptr' parameter that is a pointer to the
 *          environment for the DL Memory Initializer APIs.  See definition of 
 *          DL_init_t.
 *
 *****************************************************************************/

/******************************************************************************
* DLinit_init
*
* Parameters:
*   none
*
* Effect:
*   Initialize the handlers for the memory initialization classes

******************************************************************************/
	void DLinit_init(struct DL_init_t *thisptr);

#ifdef __cplusplus
}
#endif
#endif				/* _DLINIT_H */

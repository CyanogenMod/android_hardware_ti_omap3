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


#define TRUE 1
#define FALSE 0
#ifndef NULL
#define NULL 0
#endif

#if defined(NO_STD_LIB)
#include "dl_stdlib.h"
#define DL_STRCMP  DL__strcmp
#else
#include <string.h>
#define DL_STRCMP  strcmp
#endif

#define STATIC_EXPR_STK_SIZE 10	/* maximum parenthesis nesting in relocation
								   		stack expressions */

#include "stdint.h"
#include "doff.h"
#include "dynamic_loader.h"
#include "params.h"
#include "dload_internal.h"
#include "reloc_table.h"

#if LEAD3
#define TI_C55X_REV2 "$TI_capability_requires_rev2"
#define TI_C55X_REV3 "$TI_capability_requires_rev3"
#define TI_C55X_MEM_MODEL "$TI_capability$C5500$MemoryModel"
#endif

/*
 * Plausibility limits
 *
 * These limits are imposed upon the input DOFF file as a check for validity.
 * They are hard limits, in that the load will fail if they are exceeded.
 * The numbers selected are arbitrary, in that the loader implementation does
 * not require these limits.
 */

#define MAX_REASONABLE_STRINGTAB (0x100000)
/* maximum number of bytes in string table */
#define MAX_REASONABLE_SECTIONS (200)	
/* maximum number of code,data,etc. sections */
#define MAX_REASONABLE_SYMBOLS (100000)	/* maximum number of linker symbols */

#define ALIGN_COFF_ENDIANNESS 7	
/* shift count to align F_BIG with DLOAD_LITTLE */
#define ENDIANNESS_MASK (DF_BYTE_ORDER >> ALIGN_COFF_ENDIANNESS)


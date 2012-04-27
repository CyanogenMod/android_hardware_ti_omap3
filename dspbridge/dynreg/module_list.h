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


/*============================================================================
 Filename:     module_list.h



 This C header file gives the layout of the data structure created by the
 dynamic loader to describe the set of modules loaded into the DSP.

 Linked List Structure:
 ----------------------
 The data structure defined here is a singly-linked list.  The list
 represents the set of modules which are currently loaded in the DSP memory.
 The first entry in the list is a header record which contains a flag
 representing the state of the list.  The rest of the entries in the list
 are module records.

 Global symbol  _DLModules designates the first record in the list (i.e. the
 header record).  This symbol must be defined in any program that wishes to
 use DLLview plug-in.

 String Representation:
 ----------------------
 The string names of the module and its sections are stored in a block of
 memory which follows the module record itself.  The strings are ordered:
 module name first, followed by section names in order from the first
 section to the last.  String names are tightly packed arrays of 8-bit
 characters (two characters per 16-bit word on the C55x).  Strings are
 zero-byte-terminated.

 Creating and updating the list:
 -------------------------------
 Upon loading a new module into the DSP memory the dynamic loader inserts a 
 new module record as the first module record in the list.  The fields of
 this module record are initialized to reflect the properties of the module.
 The dynamic loader does NOT increment the flag/counter in the list's header
 record.

 Upon unloading a module from the DSP memory the dynamic loader removes the
 module's record from this list.  The dynamic loader also increments the
 flag/counter in the list's header record to indicate that the list has been
 changed. 

============================================================================*/

#ifndef _MODULE_LIST_H_
#define _MODULE_LIST_H_

#include "stdint.h"		/* portable integer type definitions*/

/* Global pointer to the modules_header structure*/
#define MODULES_HEADER "_DLModules"
#define MODULES_HEADER_NO_UNDERSCORE "DLModules"

/* Initial version number */
#define INIT_VERSION 1

/* Verification number -- to be recorded in each module record */
#define VERIFICATION 0x79

/* forward declarations */
struct dll_module;
struct dll_sect;

/* the first entry in the list is the modules_header record; 
 its address is contained in the global _DLModules pointer */
struct modules_header {
	/* Address of the first dll_module record in the list or NULL.
	 Note: for C55x this is a word address (C55x data is word-addressable)*/
	uint32_t first_module;

	/* Combined storage size (in target addressable units) of the 
	 dll_module record which follows this header record, or zero
	 if the list is empty.  This size includes the module's string table.
	 Note: for C55x the unit is a 16-bit word */
	uint16_t first_module_size;

	/* Counter is incremented whenever a module record is removed from the
	 * list*/
	uint16_t update_flag;
};

/* for each 32-bits in above structure, a bitmap, LSB first, whose bits are:
 0 => a 32-bit value, 1 => 2 16-bit values*/
#define MODULES_HEADER_BITMAP 0x2/* swapping bitmap for type modules_header */

/* information recorded about each section in a module */
struct dll_sect {
	/* Load-time address of the section.
	 Note: for C55x this is a byte address for program sections, and a word
	 address for data sections.  C55x program memory is byte-addressable,
	 while data memory is word-addressable. */
	uint32_t sect_load_adr;

	/* Run-time address of the section.
	 Note 1: for C55x this is a byte address for program sections, and a word
	 address for data sections.
	 Note 2: for C55x two most significant bits of this field indicate the
	 section type: '00' for a code section, '11' for a data section 
	 (C55 addresses are really only 24-bits wide). */
	uint32_t sect_run_adr;
} ;

/* the rest of the entries in the list are module records */
struct dll_module {
	/* Address of the next dll_module record in the list, or 0 if this is 
	 the last record in the list.
	 Note: for C55x this is a word address (C55x data is word-addressable) */
	uint32_t next_module;

	/* Combined storage size (in target addressable units) of the 
	 dll_module record which follows this one, or zero if this is the last
	 record in the list.  This size includes the module's string table.
	 Note: for C55x the unit is a 16-bit word. */
	uint16_t next_module_size;

	/* version number of the tooling; set to INIT_VERSION for Phase 1 */
	uint16_t version;

	/* the verification word; set to VERIFICATION */
	uint16_t verification;

	/* Number of sections in the sects array */
	uint16_t num_sects;

	/* Module's "unique" id; copy of the timestamp from the host COFF file*/
	uint32_t timestamp;

	/* Array of num_sects elements of the module's section records */
	struct dll_sect sects[1];
} ;

/* for each 32 bits in above structure, a bitmap, LSB first, whose bits are:
 0 => a 32-bit value, 1 => 2 16-bit values */
#define DLL_MODULE_BITMAP 0x6	/* swapping bitmap for type dll_module */

#endif				/* _MODULE_LIST_H_ */



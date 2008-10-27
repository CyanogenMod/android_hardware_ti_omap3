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

 
#ifndef _DLSYMTAB_H
#define _DLSYMTAB_H

/*****************************************************************************
 *****************************************************************************
 *
 *							DLSYMTAB.H
 *
 * A class used by the dynamic loader for symbol table support.
 *
 * This implementation uses a flat hash table, malloc/free, and printf
 *****************************************************************************
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include "dynamic_loader.h"

#define LSYM_LOGTBLLEN 5

/* Symbol Table Entry */
struct symbol {
	struct symbol *link;	/* ptr to next symbol in the hash table */
	const char *name;	/* symbol name                                      */
	unsigned versn;	/* id of the module in which this       */
	/* symbol resides                                           */
	unsigned length;	/* length of the symbol name                */
} ;

/* Symbol Table 'class' with support fields.*/
struct symtab {
	/* Symbol Table is a hash table of symbols; size is based on hash fcn   */
	struct symbol *(mytable[(1 << (LSYM_LOGTBLLEN)) + 1]);

	int hash_key;	/* remember hash lookup index from last lookup  */
	unsigned last_length;	/* remember last length */
} ;

/* Customized DL Symbol Manager 'class' */
struct DL_sym_t {
	struct Dynamic_Loader_Sym sym;
	struct symtab a_symtab;
} ;

/*****************************************************************************
 * NOTE:	All methods take a 'thisptr' parameter that is a pointer to the
 *			environment for the DL Symbol Manager APIs.  See definition of 
 *			DL_sym_t.
 *
 *****************************************************************************/

/*****************************************************************************
 * DLsym_init
 *     
 * PARAMETERS :
 *	none
 *
 * EFFECT :
 *	Initialize symbol manager handlers
 *****************************************************************************/
void DLsym_init(struct DL_sym_t * thisptr);

#ifdef __cplusplus
}
#endif
#endif				/* _DLSYMTAB_H */


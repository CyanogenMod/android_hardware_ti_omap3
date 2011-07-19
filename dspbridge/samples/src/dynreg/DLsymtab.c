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


/*****************************************************************************
 *****************************************************************************
 *
 *							DLSYMTAB.C
 *
 * A class used by the dynamic loader for symbol table support.
 *
 * This implementation uses a flat hash table, malloc/free, and printf
 *****************************************************************************
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <host_os.h>
#include <dbtype.h>

#ifndef LOAD_REMOTE_PROCESSOR
#include <std.h>
#include <mem.h>
#endif

#include <stdint.h>

#ifndef TRUE

#define FALSE (0)
#define TRUE  (1)

#endif

#include "DLsymtab.h"
#include "DLsymtab_support.h"

struct my_symbol {
	struct symbol sym;	/* must be first !!!                                */
	/* Info kept by DL, including address   */
	struct dynload_symbol lvalue;
	/* symbol was bound to                                  */
};

/*****************************************************************************
 * NOTE:	All methods take a 'thisptr' parameter that is a pointer to the
 *			environment for the DL Symbol Manager APIs.  See definition of 
 *			DL_sym_t.
 *
 *****************************************************************************/

/******************************************************************************
 * DLsym_Find_Matching_Symbol
 *     
 * PARAMETERS :
 *  name	The name of the desired symbol
 *
 * EFFECT :
 *	Locates a symbol matching the name specified.  A pointer to the
 *  symbol is returned if it exists; 0 is returned if no such symbol is
 *  found.
 *
 *****************************************************************************/
static struct dynload_symbol *DLsym_Find_Matching_Symbol
									(struct DL_sym_t * thisptr,const char *name)
{
	struct my_symbol *fsym;
	fsym = (struct my_symbol *)Find_Matching_Symbol(
									(struct symtab *)&(thisptr->a_symtab),name);
	if (!fsym)
		return NULL; 
	return &(fsym->lvalue);
}			/* DLsym_Find_Matching_Symbol */

/******************************************************************************
 * DLsym_Add_To_Symbol_Table
 *     
 * PARAMETERS :
 *  nname		Pointer to the name of the new symbol
 *  moduleid 	Unique id for the module symbol resides in
 *
 * EFFECT :
 *	The new symbol is added to the table.
 *****************************************************************************/
static struct dynload_symbol *DLsym_Add_To_Symbol_Table
				(struct DL_sym_t *thisptr,const char *nname,unsigned moduleid)
{
	struct my_symbol *newsym;
	char *pname;
	unsigned int slen = strlen(nname) + 1;
	/* length of the symbol name */
	slen = strlen(nname) + 1;
	/* allocate all the memory we need at once, to save headers */
	newsym = (struct my_symbol *)malloc(sizeof(struct my_symbol) + slen);
	if (!newsym) {
		printf("*** Heap space exhausted in Add_Symbol %d\n", slen);
		return NULL;
	}
	if (!newsym)
		return NULL;
	/* get a pointer for the name.  It follows the symbol*/
	pname = (char *)(newsym + 1);
	strcpy(pname, nname);
	Add_To_Symbol_Table((struct symtab *)&(thisptr->a_symtab), &(newsym->sym), 
																pname,moduleid);
	return &(newsym->lvalue);
}			/* DLsym_Add_To_Symbol_Table */

/******************************************************************************
 * DLsym_Purge_Symbol_Table
 *     
 * PARAMETERS :
 *	moduleid	An opaque module id assigned by the dynamic loader
 *
 * EFFECT :
 *	Each symbol in the symbol table whose moduleid matches the argument
 *  is removed from the table.
 *****************************************************************************/
static void DLsym_Purge_Symbol_Table(struct DL_sym_t *thisptr,unsigned moduleid)
{
	Iterate_Symbols((struct symbol **)&(thisptr->a_symtab.mytable),Purge_Symbol,
															(void *)moduleid);
}			/* DLsym_Purge_Symbol_Table */

/******************************************************************************
 * DLsym_Allocate
 *     
 * PARAMETERS :
 *	memsiz	size of desired memory in bytes
 *
 * EFFECT :
 *	Returns a pointer to some "host" memory for use by the dynamic
 *  loader, or NULL for failure.
 *  This function is serves as a replaceable form of "malloc" to
 *  allow the user to configure the memory usage of the dynamic loader.
 *****************************************************************************/
static void *DLsym_Allocate(struct DL_sym_t *thisptr, unsigned memsiz) 
{
	return (void *)malloc(memsiz);
}			/* DLsym_Allocate */

/******************************************************************************
 * Deallocate
 *     
 * PARAMETERS :
 *	memptr	pointer to previously allocated memory
 *
 * EFFECT :
 *	Releases the previously allocated "host" memory.
*****************************************************************************/
static void DLsym_Deallocate(struct DL_sym_t *thisptr, void *memptr) 
{		
	free(memptr);
}			/* DLsym_Deallocate */

/******************************************************************************
 * Error_Report
 *     
 * PARAMETERS :
 *	errstr	pointer to an error string
 *	args	additional arguments
 *
 * EFFECT :
 * This function provides an error reporting interface for the dynamic
 * loader.  The error string and arguments are designed as for the
 * library function vprintf.
 *****************************************************************************/
static void DLsym_Error_Report(struct DL_sym_t *thisptr,const char *errstr,
																va_list args)
{
	vfprintf(stdout, errstr, args);
	fputc('\n', stdout);
}

/******************************************************************************
 * DLsym_init
 *     
 * PARAMETERS :
 *	
 *
 * EFFECT :
 *	A new DL symbol is created and initialized
 *****************************************************************************/
void DLsym_init(struct DL_sym_t *thisptr) 
{
	thisptr->sym.Add_To_Symbol_Table = 
						(struct dynload_symbol *(*)(struct Dynamic_Loader_Sym *
						,const char *,unsigned int))DLsym_Add_To_Symbol_Table;
	thisptr->sym.Allocate = (void *(*)(struct Dynamic_Loader_Sym *,
												unsigned int))DLsym_Allocate;
	thisptr->sym.Deallocate = (void (*)(struct Dynamic_Loader_Sym *, void *))
															DLsym_Deallocate;
	thisptr->sym.Error_Report = (void (*)(struct Dynamic_Loader_Sym *,
									const char *,va_list))DLsym_Error_Report;
	thisptr->sym.Find_Matching_Symbol = (struct dynload_symbol * (*)
		(struct Dynamic_Loader_Sym *, const char *))DLsym_Find_Matching_Symbol;
	thisptr->sym.Purge_Symbol_Table = (void (*)(struct Dynamic_Loader_Sym *,
										unsigned int))DLsym_Purge_Symbol_Table; 
	Init_Symbol_Table((struct symtab *) & (thisptr->a_symtab), LSYM_LOGTBLLEN);
}			/* DLsym_init */

#ifdef __cplusplus
}
#endif


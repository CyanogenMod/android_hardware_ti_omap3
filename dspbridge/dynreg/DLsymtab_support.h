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


#ifndef _DLSYMTAB_SUPPORT_H
#define _DLSYMTAB_SUPPORT_H

/*****************************************************************************
 *****************************************************************************
 *
 *							DLSYMTAB_SUPPORT.C
 *
 * Suuport functions used by the dynamic loader for the symbol table.
 *
 * This implementation uses a flat hash table, malloc/free, and printf
 *****************************************************************************
 *****************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "dynamic_loader.h"
#include "DLsymtab.h"

typedef int Symbol_Action(struct symbol *thissym, void *arg);

void Init_Symbol_Table(struct symtab *a_stable, int llen);

struct symbol *Find_Matching_Symbol(struct symtab *a_table, const char *name);

void Add_To_Symbol_Table(struct symtab *a_stable, struct symbol * newsym,
											const char *nname, unsigned versn);
void Iterate_Symbols(struct symbol **stable, Symbol_Action *action,void *arg);
int Purge_Symbol(struct symbol *thissym, void *arg);

#ifdef __cplusplus
}
#endif
#endif				/* _DLSYMTAB_SUPPORT_H */


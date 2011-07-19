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
 *							DLSYMTAB_SUPPORT.C
 *
 * Suuport functions used by the dynamic loader for the symbol table.
 *
 * This implementation uses a flat hash table
 *****************************************************************************
 *****************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>

#include "DLsymtab.h"
#include "DLsymtab_support.h"

/* Enumeration used as status by the Symbol Manager support routines        */
#define SYMI_continue	0	/* continue iteration*/
#define SYMI_delete		1	/* delete symbol */
#define SYMI_terminate	2	/* terminate iteration */

/******************************************************************************
 * We are using multiplicative Fibonacci hashing;
 * these magic constants have been derived from Knuth.
 *****************************************************************************/
#define AVALUE		UINT32_C(1315424652)

#define KEYSHIFT	4

#if defined(__TMS320C55X__) || defined(_TMS320C5XX)
#define BITS_PER_AU 16
#else
#define BITS_PER_AU 8
#endif

/******************************************************************************
 * Init_Symbol_Table
 *     
 * EFFECT     : Initializes a symbol table
 *
 * PARAMETERS :
 *  a_stable	A pointer to the symbol table object
 *  llen		Log base 2 of length of the hash index.
 *
 * INITIAL CONDITIONS :
 *	The symbol table must be an array of length 2**llen +1
 *
 * POST CONDITIONS :
 *	The symbol table is appropriately initialized
 *****************************************************************************/
void Init_Symbol_Table(struct symtab *a_stable, int llen) 
{
	struct symbol **dst;
	int length;

	dst = (a_stable->mytable);
	length = 1 << llen;
	/* init the shift count */
	*dst++ = (struct symbol *)(sizeof(uint32_t) * BITS_PER_AU - llen);
	do
		*dst++ = 0;
	while ((length -= 1) > 0);	/* zap the pointers */
}			/* Init_Symbol_Table */

/******************************************************************************
 * Find_Matching_Symbol
 *     
 * PARAMETERS :
 *  a_stable	A pointer to the symbol table object
 *  name		The name of the desired symbol
 *
 * EFFECT :
 *	Locates a symbol matching the name specified.  A pointer to the
 * symbol is returned if it exists; 0 is returned if no such symbol is
 * found.
 *
 *****************************************************************************/
struct symbol *Find_Matching_Symbol(struct symtab *a_stable, const char *name)
{
	struct symbol *sptr;
	char c;
	const char *nam1, *nam2;
	uint32_t key, length;
	int len;
	
	/* get name length and hash key */
	nam1 = name;
	key = 0;
	while ((c = *nam1++))
		key = (key << KEYSHIFT) + c;
	a_stable->last_length = length = nam1 - name - 1;
	
	/* Now compute the key from the hash key */
	a_stable->hash_key = (uint32_t)(key * AVALUE) >> (int)*a_stable->mytable; 
	
	/*  Search the list related to this hash key */
	sptr = a_stable->mytable[a_stable->hash_key + 1];
	
	while (sptr != 0) { 
		if (sptr->length == length) {	/* quick check on length */
			nam1 = name;
			nam2 = sptr->name;
			len = length;
			do
				if (*nam1++ != *nam2++)
					goto next_symbol;
			while ((len -= sizeof (char)) > 0) ;
			return sptr;	/* names match !! */
		}
next_symbol:
		sptr = (struct symbol *) sptr->link;
	}
	return 0;	/* no match found */ 
}			/* Find_Matching_Symbol */


/******************************************************************************
 * Add_To_Symbol_Table
 *     
 * PARAMETERS :
 *  a_stable	A pointer to the symbol table object
 *	newsym		symbol block to use for new symbol
 *  nname		Pointer to the name of the new symbol
 *  versn 		id for module symbol resides in
 *
 * EFFECT :
 *	The new symbol is added to the table.
 *****************************************************************************/
void Add_To_Symbol_Table(struct symtab *a_stable, struct symbol *newsym,
											const char *nname, unsigned versn) 
{
	register struct symbol **sptr; 
	
	Find_Matching_Symbol(a_stable, nname); 
	/*Insert newsym at the head of stable[thisptr->hash_key+1] list */
	sptr = &a_stable->mytable[a_stable->hash_key + 1];
	newsym->link = (struct symbol *)*sptr;
	*sptr = newsym;
	newsym->name = nname;
	newsym->length = a_stable->last_length;
	newsym->versn = versn;
}			/* Add_To_Symbol_Table */


/******************************************************************************
 * Iterate_Symbols
 *     
 * PARAMETERS :
 *  stable	A pointer to the symbol table
 *	action	Routine to call with each symbol
 *	arg		Uninterpreted argument passed to "action"
 *
 * EFFECT :
 *	The specified routine is called repeatedly, once for each symbol in
 * the symbol table.  The return value of the called routine is interpreted
 * as follows:
 *	SYMI_continue	// continue iteration
 *	SYMI_terminate	// terminate iteration
 *	SYMI_delete		// delete symbol
 *
 *****************************************************************************/
void Iterate_Symbols(struct symbol **stable, Symbol_Action *action, void *arg) 
{
	unsigned cnt;
	struct symbol *prev, *sym, *nxt;
	
	/* length of hash table */
	cnt = (~UINT32_C(0) >> (unsigned)*stable++) + 1; /* length of hash table */
	do {
		prev = (struct symbol *)stable;
		sym = *stable++; 
		while (sym) {	/* process list */
			int rslt; 
			nxt = sym->link;
			rslt = action(sym, arg);
			if (rslt) {
				/* remove from list */
				if (rslt & SYMI_delete)
					prev->link = nxt;
				if (rslt & SYMI_terminate)
					return;
			} else
				prev = sym; 
			sym = nxt;
		}	/* process list */ 
	} while ((cnt -= 1) > 0); 
}			/* Iterate_Symbols */


/******************************************************************************
 * Purge_Symbol
 *     
 * PARAMETERS :
 *  thissym		Symbol to purge	
 *  arg			Criteria used to indicate symbol should be purged
 *
 * EFFECT :
 *	Purge this symbol from the symbol table if it meets the criteria
 *  specified in arg
 *
 *****************************************************************************/
int Purge_Symbol(struct symbol *thissym, void *arg) 
{
	if ((thissym->versn == (uint32_t)arg)) {
		free(thissym);
		return SYMI_delete;
	}
	return SYMI_continue;
}			/* Purge_Symbol */

#ifdef __cplusplus
}
#endif


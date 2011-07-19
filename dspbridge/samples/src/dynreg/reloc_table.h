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


#ifndef __RELOC_TABLE_H__
#define __RELOC_TABLE_H__
/*
 * Table of relocation operator properties
 */
#include "stdint.h"

/* How does this relocation operation access the program image? */
#define ROP_N	0		/* does not access image */
#define ROP_R	1		/* read from image */
#define ROP_W	2		/* write to image */
#define ROP_RW	3		/* read from and write to image */

/* For program image access, what are the overflow rules for the bit field? */
/* Beware! Procedure repack depends on this encoding */
#define ROP_ANY	0		/* no overflow ever, just truncate the value */
#define ROP_SGN	1		/* signed field */
#define ROP_UNS	2		/* unsigned field */
#define ROP_MAX 3		/* allow maximum range of either signed or unsigned */

/* How does the relocation operation use the symbol reference */
#define ROP_IGN	0		/* no symbol is referenced */
#define ROP_LIT 0		/* use rp->r_uval literal field */
#define ROP_SYM	1		/* symbol value is used in relocation */
#define ROP_SYMD 2		/* delta value vs last link is used */

/* How does the reloc op use the stack? */
#define RSTK_N 0		/* Does not use */
#define RSTK_POP 1		/* Does a POP */
#define RSTK_UOP 2		/* Unary op, stack position unaffected */
#define RSTK_PSH 3		/* Does a push */

/*
 * Computational actions performed by the dynamic loader
 */
enum Dload_Actions {
	RACT_VAL,		/* do not alter the current value (from 
					   	stack or memory fetch) */
	RACT_ASGN,		/* set value to reference amount (from symbol reference) */
	RACT_ADD,		/* add reference to value */
	RACT_PCR,		/* add reference minus PC delta to value */
	RACT_ADDISP,		/* add reference plus r_disp */
	RACT_ASGPC,		/* set value to section address plus reference */

	RACT_PLUS,		/* stack + */
	RACT_SUB,		/* stack - */
	RACT_NEG,		/* stack unary - */

	RACT_MPY,		/* stack * */
	RACT_DIV,		/* stack / */
	RACT_MOD,		/* stack % */

	RACT_SR,		/* stack unsigned >> */
	RACT_ASR,		/* stack signed >> */
	RACT_SL,		/* stack << */
	RACT_AND,		/* stack & */
	RACT_OR,		/* stack | */
	RACT_XOR,		/* stack ^ */
	RACT_NOT,		/* stack ~ */
	RACT_C6SECT,		/* for C60 R_SECT op */
	RACT_C6BASE,		/* for C60 R_BASE op */
	RACT_C6DSPL,		/* for C60 scaled 15-bit displacement */
	RACT_PCR23T		/* for ARM Thumb long branch */
};

/*
 * macros used to extract values
 */
#define RFV_POSN(aaa) ((aaa) & 0xF)
#define RFV_WIDTH(aaa) (((aaa) >> 4) & 0x3F)
#define RFV_ACTION(aaa) ((aaa) >> 10)

#define RFV_SIGN(iii) (((iii) >> 2) & 0x3)
#define RFV_SYM(iii) (((iii) >> 4) & 0x3)
#define RFV_STK(iii) (((iii) >> 6) & 0x3)
#define RFV_ACCS(iii) ((iii) & 0x3)

#if (TMS32060) || (TMS470) || (LEAD)
#define RFV_SCALE(iii) ((iii) >> 11)
#define RFV_BIGOFF(iii) (((iii) >> 8) & 0x7)
#else
#define RFV_BIGOFF(iii) ((iii) >> 8)
#endif

#endif				/* __RELOC_TABLE_H__ */


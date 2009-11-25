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


#ifndef __DLOAD_INTERNAL__
#define __DLOAD_INTERNAL__
/*
 * Internal state definitions for the dynamic loader
 */

#define TRUE 1
#define FALSE 0
typedef int boolean;

typedef int_least32_t RVALUE;	
/*type used for relocation intermediate results*/
typedef uint_least32_t URVALUE;	
/* unsigned version of same; must have at least as many bits */

/*
 * Dynamic loader configuration constants
 */
#define REASONABLE_SECTION_LIMIT 100	
/* error issued if input has more sections than this limit */

#define dload_fill_bss 0	
/* (Addressable unit) value used to clear BSS section */

/*
 * Reorder maps explained (?)
 *
 *The doff file format defines a 32-bit pattern used to determine the byte order
 * of an image being read.  That value is BYTE_RESHUFFLE_VALUE == 0x00010203
 * For purposes of the reorder routine, we would rather have the all-is-OK for
 * 32-bits pattern be 0x03020100.  This first macro makes the translation from
 * doff file header value to MAP value: */
#define REORDER_MAP(rawmap) ((rawmap) ^ 0x3030303)
/* This translation is made in dload_headers.  Thereafter, the all-is-OK value
 * for the maps stored in dlthis is REORDER_MAP(BYTE_RESHUFFLE_VALUE).
 * But sadly, not all bits of the doff file are 32-bit integers.  The notable
 * exceptions are strings and image bits.  Strings obey host byte order: */

/* _BIG_ENDIAN Not defined by bridge driver */
/* #if defined(_BIG_ENDIAN)
#define HOST_BYTE_ORDER(cookedmap) ((cookedmap) ^ 0x3030303)
#else */
#define HOST_BYTE_ORDER(cookedmap) (cookedmap)
/* #endif */ /* _BIG_ENDIAN Not defined by bridge driver */

/* Target bits consist of target AUs (could be bytes, or 16-bits, or 32-bits)
 * stored as an array in host order.  A target order map is defined by: */

/* _BIG_ENDIAN Not defined by bridge driver */
/* #if !defined(_BIG_ENDIAN) || TARGET_AU_BITS > 16 */
#define TARGET_ORDER(cookedmap) (cookedmap)
/* #elif TARGET_AU_BITS > 8
#define TARGET_ORDER(cookedmap) ((cookedmap) ^ 0x2020202)
#else
#define TARGET_ORDER(cookedmap) ((cookedmap) ^ 0x3030303)
#endif */ /* _BIG_ENDIAN Not defined by bridge driver */

struct my_handle;	
/* forward declaration for handle returned by dynamic loader */

/*
 * a list of module handles, which mirrors the debug list on the target
 */
struct dbg_mirror_root {
	uint32_t dbthis;	
	/* must be same as dbg_mirror_list; __DLModules address on target */
	struct my_handle *hnext;	/* must be same as dbg_mirror_list */
	uint16_t changes;	/* change counter */
	uint16_t refcount;	/* number of modules referencing this root */
} ;

struct dbg_mirror_list {
	uint32_t dbthis;
	struct my_handle *hnext, *hprev;
	struct dbg_mirror_root *hroot;
	uint16_t dbsiz;
	uintptr_t context;	/* Save context for .dllview memory allocation */
} ;

#define VARIABLE_SIZE 1
/*
 * the structure we actually return as an opaque module handle
 */
struct my_handle {
	struct dbg_mirror_list dm;	/* !!! must be first !!! */
	uint16_t secn_count;	
	/* sections following << 1, LSB is set for big-endian target */
	struct LDR_SECTION_INFO secns[VARIABLE_SIZE];
} ;
#define MY_HANDLE_SIZE (sizeof(struct my_handle) -\
											sizeof(struct LDR_SECTION_INFO))	
	/* real size of my_handle*/

/*
 * reduced symbol structure used for symbols during relocation
 */
struct Local_Symbol {
	int_least32_t value;	/* Relocated symbol value */
	int_least32_t delta;	/* Original value in input file */
	int16_t secnn;		/* section number */
	int16_t sclass;		/* symbol class */
} ;

/*
 * States of the .cinit state machine
 */
enum cinit_mode {
	CI_count = 0,		/* expecting a count */
	CI_address,		/* expecting an address */
#if CINIT_ALIGN < CINIT_ADDRESS	/* handle case of partial address field */
	CI_partaddress,		/* have only part of the address */
#endif
	CI_copy,		/* in the middle of copying data */
	CI_done			/* end of .cinit table */
};

/*
 * The internal state of the dynamic loader, which is passed around as
 * an object
 */
struct dload_state {
	struct Dynamic_Loader_Stream *strm;	/* The module input stream */
	struct Dynamic_Loader_Sym *mysym;	/* Symbols for this session */
	struct Dynamic_Loader_Allocate *myalloc;/* The target memory allocator */
	struct Dynamic_Loader_Initialize *myio;	/* The target memory initializer */
	unsigned myoptions;	/* Options parameter Dynamic_Load_Module */

	char *str_head;		/* Pointer to string table */
#if BITS_PER_AU > BITS_PER_BYTE
	char *str_temp;		/* Pointer to temporary buffer for strings */
	/* big enough to hold longest string */
	unsigned temp_len;	/* length of last temporary string */
	char *xstrings;		/* Pointer to buffer for expanded */
	/* strings for sec names */
#endif
	unsigned debug_string_size;	
	/* Total size of strings for DLLView section names */

	struct doff_scnhdr_t *sect_hdrs;	/* Pointer to section table */
	struct LDR_SECTION_INFO *ldr_sections;	
	/* Pointer to parallel section info for allocated sections only */
#if TMS32060
	LDR_ADDR bss_run_base;	/* The address of the start of the .bss section */
#endif
	struct Local_Symbol *local_symtab;	/* Relocation symbol table */

	struct LDR_SECTION_INFO *image_secn;	
	/* pointer to DL section info for the section being relocated */
	LDR_ADDR delta_runaddr;	
	/* change in run address for current section during relocation */
	LDR_ADDR image_offset;	/* offset of current packet in section */
	enum cinit_mode cinit_state;	/* current state of cload_cinit() */
	int cinit_count;	/* the current count */
	LDR_ADDR cinit_addr;	/* the current address */
	uint16_t cinit_page;	/* the current page */

	struct my_handle *myhandle;/* Handle to be returned by Dynamic_Load_Module*/
	unsigned dload_errcount;	/* Total number of errors reported so far */

	unsigned allocated_secn_count;	
	/* Number of target sections that require allocation and relocation */
#ifndef TARGET_ENDIANNESS
	boolean big_e_target;	/* Target data in big-endian format */
#endif

	uint_least32_t reorder_map;	/* map for reordering bytes, 0 if not needed */
	struct doff_filehdr_t dfile_hdr;	/* DOFF file header structure */
	struct doff_verify_rec_t verify;	/* Verify record */

	int relstkidx;		/* index into relocation value stack */
	RVALUE relstk[STATIC_EXPR_STK_SIZE];
	/* relocation value stack used in relexp.c */

} ;

#ifdef TARGET_ENDIANNESS
#define TARGET_BIG_ENDIAN TARGET_ENDIANNESS
#else
#define TARGET_BIG_ENDIAN 0		/* dlthis->big_e_target */
#endif

/*
 * Exports from cload.c to rest of the world
 */
extern void dload_error(struct dload_state *dlthis, const char *errtxt, ...);
extern void dload_syms_error(struct Dynamic_Loader_Sym *syms,const char *errtxt,
																	     ...);
extern void dload_headers(struct dload_state * dlthis);
extern void dload_strings(struct dload_state * dlthis, boolean sec_names_only);
extern void dload_sections(struct dload_state * dlthis);
extern void dload_reorder(void *data, int dsiz, uint_least32_t map);
extern uint32_t dload_checksum(void *data, unsigned siz);
#if HOST_ENDIANNESS
extern uint32_t dload_reverse_checksum(void *data, unsigned siz);
#if (TARGET_AU_BITS > 8) && (TARGET_AU_BITS < 32)
extern uint32_t dload_reverse_checksum_16(void *data, unsigned siz);
#endif
#endif

#define is_data_scn(zzz) (DLOAD_SECTION_TYPE((zzz)->type) != DLOAD_TEXT)
#define is_data_scn_num(zzz) (DLOAD_SECT_TYPE(&dlthis->sect_hdrs[(zzz)-1])\
															!= DLOAD_TEXT)

/*
 * exported by reloc.c
 */
extern void dload_relocate(struct dload_state *dlthis, TgtAU_t *data,
												   struct reloc_record_t *rp);
extern RVALUE dload_unpack(struct dload_state *dlthis, TgtAU_t *data, 
										int fieldsz, int offset, unsigned sgn);
extern int dload_repack(struct dload_state *dlthis, RVALUE val, TgtAU_t *data,
										int fieldsz, int offset, unsigned sgn);

#endif				/* __DLOAD_INTERNAL__ */



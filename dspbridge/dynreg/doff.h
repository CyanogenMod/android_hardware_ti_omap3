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


/*****************************************************************************/
/*  DOFF.H - Structures & definitions used for dynamically                   */
/*           loaded modules file format.  This format is a reformatted       */
/*           version of COFF.(see coff.h for details)  It optimizes the      */
/*           layout for the dynamic loader.                                  */
/*                                                                           */
/*  .dof files, when viewed as a sequence of 32-bit integers, look the same  */
/*  on big-endian and little-endian machines.                                */
/*****************************************************************************/
#ifndef _DOFF_H
#define _DOFF_H

#ifndef UINT32_C
#define UINT32_C(zzz) ((uint32_t)zzz)
#endif

#define BYTE_RESHUFFLE_VALUE UINT32_C(0x00010203)

/* DOFF file header containing fields categorizing the remainder of the file */
struct doff_filehdr_t {

	/* string table size, including filename, in bytes                       */
	uint32_t df_strtab_size;

	/* entry point if one exists */
	uint32_t df_entrypt;

	/* identifies byte ordering of file; always set to BYTE_RESHUFFLE_VALUE  */
	uint32_t df_byte_reshuffle;

	/* Size of the string table up to and including the last section name    */
	/* Size includes the name of the COFF file also                          */
	uint32_t df_scn_name_size;

/* _BIG_ENDIAN Not defined by bridge driver */
/* #ifndef _BIG_ENDIAN */
	/* number of symbols */
	uint16_t df_no_syms;

	/* length in bytes of the longest string, including terminating NULL     */
	/* excludes the name of the file                                         */
	uint16_t df_max_str_len;

	/* total number of sections including no-load ones                       */
	uint16_t df_no_scns;

	/* number of sections containing target code allocated or downloaded     */
	uint16_t df_target_scns;

	/* unique id for dll file format & version                               */
	uint16_t df_doff_version;

	/* identifies ISA */
	uint16_t df_target_id;

	/* useful file flags */
	uint16_t df_flags;

	/* section reference for entry point, N_UNDEF for none,                  */
	/* N_ABS for absolute address */
	int16_t df_entry_secn;
/* #else */ /* _BIG_ENDIAN Not defined by bridge driver */
	/* length of the longest string, including terminating NULL              */
/*	uint16_t df_max_str_len; */

	/* number of symbols */
/*	uint16_t df_no_syms; */

	/* number of sections containing target code allocated or downloaded     */
/*	uint16_t df_target_scns; */

	/* total number of sections including no-load ones */
/*	uint16_t df_no_scns; */

	/* identifies ISA */
/*	uint16_t df_target_id; */

	/* unique id for dll file format & version */
/*	uint16_t df_doff_version; */

	/* section reference for entry point, N_UNDEF for none,                  */
	/* N_ABS for absolute address */
/*	int16_t df_entry_secn; */

	/* useful file flags */
/*	uint16_t df_flags; */
/* #endif */ /* _BIG_ENDIAN Not defined by bridge driver */
	/* checksum for file header record */
	uint32_t df_checksum;

} ;

/* flags in the df_flags field */
#define  DF_LITTLE   0x100
#define  DF_BIG      0x200
#define  DF_BYTE_ORDER (DF_LITTLE | DF_BIG)

/* Supported processors */
#define TMS470_ID   0x97
#define LEAD_ID     0x98
#define TMS32060_ID 0x99
#define LEAD3_ID    0x9c

/* Primary processor for loading */
#if TMS470
#define TARGET_ID   TMS470_ID
#elif LEAD
#define TARGET_ID   LEAD_ID
#elif TMS32060
#define TARGET_ID   TMS32060_ID
#elif LEAD3
#define TARGET_ID   LEAD3_ID
#endif

/* Verification record containing values used to test integrity of the bits */
struct doff_verify_rec_t {

	/* time and date stamp */
	uint32_t dv_timdat;

	/* checksum for all section records */
	uint32_t dv_scn_rec_checksum;

	/* checksum for string table */
	uint32_t dv_str_tab_checksum;

	/* checksum for symbol table */
	uint32_t dv_sym_tab_checksum;

	/* checksum for verification record */
	uint32_t dv_verify_rec_checksum;

};

/* String table is an array of null-terminated strings.  The first entry is
 * the filename, which is added by DLLcreate.  No new structure definitions
 * are required.
 */

/* Section Records including information on the corresponding image packets */
/*
 *      !!WARNING!!
 *
 * This structure is expected to match in form LDR_SECTION_INFO in
 * dynamic_loader.h
 */

struct doff_scnhdr_t {

	int32_t ds_offset;	/* offset into string table of name    */
	int32_t ds_paddr;	/* RUN address, in target AU           */
	int32_t ds_vaddr;	/* LOAD address, in target AU          */
	int32_t ds_size;	/* section size, in target AU          */
/* _BIG_ENDIAN Not defined by bridge driver */
/* #ifndef _BIG_ENDIAN */
	uint16_t ds_page;	/* memory page id                      */
	uint16_t ds_flags;	/* section flags                       */
/*#else
	uint16_t ds_flags; */	/* section flags                       */
/*	uint16_t ds_page; */	/* memory page id                      */
/* #endif */
	uint32_t ds_first_pkt_offset;
	/* Absolute byte offset into the file  */
	/* where the first image record resides */

	int32_t ds_nipacks;	/* number of image packets             */
} ;

/* Symbol table entry */
struct doff_syment_t {

	int32_t dn_offset;	/* offset into string table of name    */
	int32_t dn_value;	/* value of symbol                     */
/*#ifndef _BIG_ENDIAN */ /* _BIG_ENDIAN Not defined by bridge driver */
	int16_t dn_scnum;	/* section number                      */
	int16_t dn_sclass;	/* storage class                       */
/*#else
	int16_t dn_sclass; */	/* storage class                       */
/*	int16_t dn_scnum; */	/* section number, 1-based             */
/* #endif */
} ;

/* special values for dn_scnum */
#define  DN_UNDEF  0		/* undefined symbol               */
#define  DN_ABS    -1		/* value of symbol is absolute    */
/* special values for dn_sclass */
#define DN_EXT     2
#define DN_STATLAB 20
#define DN_EXTLAB  21

/* Default value of image bits in packet */
/* Configurable by user on the command line */
#define IMAGE_PACKET_SIZE 1024

/* An image packet contains a chunk of data from a section along with */
/* information necessary for its processing.                          */
struct image_packet_t {

	int32_t i_num_relocs;	/* number of relocations for   */
	/* this packet                 */

	int32_t i_packet_size;	/* number of bytes in array    */
	/* "bits" occupied  by         */
	/* valid data.  Could be       */
	/* < IMAGE_PACKET_SIZE to      */
	/* prevent splitting a         */
	/* relocation across packets.  */
	/* Last packet of a section    */
	/* will most likely contain    */
	/* < IMAGE_PACKET_SIZE bytes   */
	/* of valid data               */

	int32_t i_checksum;	/* Checksum for image packet   */
	/* and the corresponding       */
	/* relocation records          */

	uint_least8_t *i_bits;	/* Actual data in section      */

} ;

/* The relocation structure definition matches the COFF version.  Offsets  */
/* however are relative to the image packet base not the section base.     */
struct reloc_record_t {
	int32_t r_vaddr;	/* (virtual) address of reference   */
	/* expressed in target AUs          */

	union {
		struct {
/*#ifndef _BIG_ENDIAN*/ /* _BIG_ENDIAN Not defined by bridge driver */
			uint8_t _offset;	/* bit offset of rel fld      */
			uint8_t _fieldsz;	/* size of rel fld            */
			uint8_t _wordsz;	/* # bytes containing rel fld */
			uint8_t _dum1;
			uint16_t _dum2;
			uint16_t _type;
/*#else
			unsigned _dum1:8;
			unsigned _wordsz:8;*/	/* # bytes containing rel fld */
/*			unsigned _fieldsz:8;*/	/* size of rel fld            */
/*			unsigned _offset:8;*/	/* bit offset of rel fld      */
/*			uint16_t _type;
			uint16_t _dum2;
#endif */
		} _r_field;

		struct {
			uint32_t _spc;	/* image packet relative PC   */
/*#ifndef _BIG_ENDIAN*//* _BIG_ENDIAN Not defined by bridge driver */
			uint16_t _dum;
			uint16_t _type;	/* relocation type            */
/*#else
			uint16_t _type;*//* relocation type            */
/*			uint16_t _dum;
#endif*/ /* _BIG_ENDIAN Not defined by bridge driver */
		} _r_spc;

		struct {
			uint32_t _uval;	/* constant value             */
/*#ifndef _BIG_ENDIAN*/ /* _BIG_ENDIAN Not defined by bridge driver */
			uint16_t _dum;
			uint16_t _type;	/* relocation type            */
/*#else
			uint16_t _type;*//* relocation type            */
/*			uint16_t _dum;
#endif*/
		} _r_uval;

		struct {
			int32_t _symndx;	/* 32-bit sym tbl index       */
/*#ifndef _BIG_ENDIAN *//* _BIG_ENDIAN Not defined by bridge driver */
			uint16_t _disp;	/* extra addr encode data     */
			uint16_t _type;	/* relocation type            */
/*#else
			uint16_t _type;*//* relocation type            */
/*			uint16_t _disp;*//* extra addr encode data     */
/*#endif*/
		} _r_sym;
	} _u_reloc;
} ;

/* abbreviations for convenience */
#ifndef r_type
#define r_type      _u_reloc._r_sym._type
#define r_uval      _u_reloc._r_uval._uval
#define r_symndx    _u_reloc._r_sym._symndx
#define r_offset    _u_reloc._r_field._offset
#define r_fieldsz   _u_reloc._r_field._fieldsz
#define r_wordsz    _u_reloc._r_field._wordsz
#define r_disp      _u_reloc._r_sym._disp
#endif

/*****************************************************************************/
/*                                                                           */
/* Important DOFF macros used for file processing                            */
/*                                                                           */
/*****************************************************************************/

/* DOFF Versions */
#define         DOFF0                       0

/* Return the address/size >= to addr that is at a 32-bit boundary           */
/* This assumes that a byte is 8 bits                                        */
#define         DOFF_ALIGN(addr)            (((addr) + 3) & ~UINT32_C(3))

/*****************************************************************************/
/*                                                                           */
/* The DOFF section header flags field is laid out as follows:               */
/*                                                                           */
/*  Bits 0-3 : Section Type                                                  */
/*  Bit    4 : Set when section requires target memory to be allocated by DL */
/*  Bit    5 : Set when section requires downloading                         */
/*  Bits 8-11: Alignment, same as COFF                                       */
/*                                                                           */
/*****************************************************************************/

/* Enum for DOFF section types (bits 0-3 of flag): See dynamic_loader.h      */

/* Macros to help processing of sections                                     */
#define DLOAD_SECT_TYPE(s_hdr)      ((s_hdr)->ds_flags & 0xF)

/* DS_ALLOCATE indicates whether a section needs space on the target         */
#define DS_ALLOCATE_MASK            0x10
#define DS_NEEDS_ALLOCATION(s_hdr)  ((s_hdr)->ds_flags & DS_ALLOCATE_MASK)

/* DS_DOWNLOAD indicates that the loader needs to copy bits                  */
#define DS_DOWNLOAD_MASK            0x20
#define DS_NEEDS_DOWNLOAD(s_hdr)    ((s_hdr)->ds_flags & DS_DOWNLOAD_MASK)

/* Section alignment requirement in AUs */
#define DS_ALIGNMENT(ds_flags) (1 << (((ds_flags) >> 8) & 0xF))

#endif				/* _DOFF_H */



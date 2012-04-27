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


/******************************************************************************
 *
 *		This file defines host and target properties for all machines
 * supported by the dynamic loader.  To be tedious...
 *
 *		host == the machine on which the dynamic loader runs
 *		target == the machine that the dynamic loader is loading
 *
 * Host and target may or may not be the same, depending upon the particular
 * use.
 *****************************************************************************/

/******************************************************************************
 *
 *							Host Properties
 *
 *****************************************************************************/

#define BITS_PER_BYTE 8		/* bits in the standard PC/SUN byte */
#define LOG_BITS_PER_BYTE 3	/* log base 2 of same */
#define BYTE_MASK ((1U<<BITS_PER_BYTE)-1)

#if defined(__TMS320C55X__) || defined(_TMS320C5XX)
#define BITS_PER_AU 16
#define LOG_BITS_PER_AU 4
#define FMT_UI32 "0x%lx"	
/* use this print string in error messages for uint32_t */
#define FMT8_UI32 "%08lx"	/* same but no 0x, fixed width field */
#else
#define BITS_PER_AU 8		
/* bits in the smallest addressable data storage unit */
#define LOG_BITS_PER_AU 3/* log base 2 of the same; useful for shift counts */
#define FMT_UI32 "0x%x"
#define FMT8_UI32 "%08x"
#endif

/* generic fastest method for swapping bytes and shorts */
#define SWAP32BY16(zz) (((zz) << 16) | ((zz) >> 16))
#define SWAP16BY8(zz) (((zz) << 8) | ((zz) >> 8))

/* !! don't be tempted to insert type definitions here; use <stdint.h> !! */

/******************************************************************************
 *
 *							Target Properties
 *
 *****************************************************************************/

/*--------------------------------------------------------------------------*/
/* TMS340 (GSP) Target Specific Parameters (bit-addressable)                */
/*--------------------------------------------------------------------------*/
#if TMS340
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x0L	/* (full address space)            */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  4	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  4	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	1	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0L	/* GSP has no blocking             */

#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '$'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS370 Target Specific Parameters (byte-addressable)                     */
/*--------------------------------------------------------------------------*/
#if TMS370
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x10000L	/* (64K)                           */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  2	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  2	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	1	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0L	/* 370 has no blocking             */

#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '_'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS370C8                                                                 */
/*--------------------------------------------------------------------------*/
#if TMS370C8
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x10000L	/* (64K)                           */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  2	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  2	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	1	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0L	/* C8  has no blocking             */

#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '_'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS370C8P                                                                */
/*--------------------------------------------------------------------------*/
#if TMS370C8P
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x1000000L	/* (16M)                           */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  3	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  3	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	1	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0L	/* C8  has no blocking             */

#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '_'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS37016 (C16) Target Specific Parameters (byte-addressable)             */
/*--------------------------------------------------------------------------*/
#if TMS37016
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x10000L	/* (64K)                           */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  2	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  2	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	1	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0L	/* 370 has no blocking             */

#define COFF_VERSION_0  0
#define COFF_VERSION_1  0
#define COFF_VERSION_2  1
#define COFF_TARG_MIN   0	/* MINIMUM COFF VERSION SUPPORTED   */

#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '$'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS32030 (C30) Target Specific Parameters (32-bit word-addressable)      */
/*--------------------------------------------------------------------------*/
#if TMS32030
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x0L	/* (full address space)            */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  4	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  4	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	1	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0x10000L	/* Block size (data page) is 64K   */
#define COFF_VERSION_0  0
#define COFF_VERSION_1  0
#define COFF_VERSION_2  1
#define COFF_TARG_MIN   0	/* MINIMUM COFF VERSION SUPPORTED   */

#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '$'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS32025 (DSP) Target Specific Parameters (16-bit word-addressable)      */
/*--------------------------------------------------------------------------*/
#if TMS32025
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x10000L	/* 64K                             */

#define _CSTART "_c_int0"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  2	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  2	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	1	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0x80L	/* Block size (data page) is 128   */
#define COFF_VERSION_0  0
#define COFF_VERSION_1  0
#define COFF_VERSION_2	1
#define COFF_TARG_MIN   0	/* MINIMUM COFF VERSION SUPPORTED   */

#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '$'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS380 (EGL) Target Specific Parameters (byte-addressable)               */
/*--------------------------------------------------------------------------*/
#if TMS380
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x10000L	/* 64K                             */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  2	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  2	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	1	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0L	/* 380 has no blocking             */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS320C8x (MVP MP/PP) Target Specific Parameters (byte-addressable)      */
/*--------------------------------------------------------------------------*/
#if MVP_PP || MVP_MP
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x0L	/* (full address space)            */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  4	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  4	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     4	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		4	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	4	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0L	/* MVP  has no blocking            */

#define COFF_VERSION_0	0
#define COFF_VERSION_1	1
#define COFF_VERSION_2	0
#define COFF_TARG_MIN   0	/* MINIMUM COFF VERSION SUPPORTED   */

#if MVP_PP
#define HLL_PREFIX      '$'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '_'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '$'	/* HLL prefix to function names     */
#else
#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '$'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */
#endif

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS320C54x (LEAD) Target Specific Parameters (16-bit word-addressable)   */
/*--------------------------------------------------------------------------*/
#if LEAD
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x40000L	/* 18-bit address space            */

#define TARGET_AU_BITS 16	/* width of the target addressable unit */
#define LOG_TARGET_AU_BITS 4	/* log2 of same */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	1	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define TARGET_ENDIANNESS 1	/* always big-endian */
#endif

/*--------------------------------------------------------------------------*/
/* TMS470 (ARM) Target Specific Parameters (byte-addressable)               */
/*--------------------------------------------------------------------------*/
#if TMS470
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x0L	/* (full address space)            */

#define CINIT_ALIGN     4	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		4	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	4	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#undef TARGET_ENDIANNESS	/* may be big or little endian */

#define TARGET_WORD_ALIGN(zz) (((zz) + 0x3) & -0x4)	
/* align a target address to a word boundary */

#endif

/*--------------------------------------------------------------------------*/
/* TMS320C6x Target Specific Parameters (byte-addressable)                  */
/*--------------------------------------------------------------------------*/
#if TMS32060
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x0L	/* (full address space)            */

#define CINIT_ALIGN     8	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		4	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	4	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */
#if 0				/* obsolete feature */
#define CINIT_BSSREF_BYTES 4	/* bytes in a BSS-add pointer */
#endif

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#undef TARGET_ENDIANNESS	/* may be big or little endian */

#define TARGET_WORD_ALIGN(zz) (((zz) + 0x3) & -0x4)	
/* align a target address to a word boundary */
#endif

/*--------------------------------------------------------------------------*/
/* TARANTULA Target Specific Parameters (byte-addressable)                  */
/*--------------------------------------------------------------------------*/
#if TARANTULA
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x0L	/* (full address space)            */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  4	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  4	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     4	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		4	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	4	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

#define SBLOCKSIZE      0L	/* TARANTULA  has no blocking     */

#define COFF_VERSION_0	0
#define COFF_VERSION_1	0
#define COFF_VERSION_2	1	/* TARANTULA uses COFF format 2   */
#define COFF_TARG_MIN   0	/* MINIMUM COFF VERSION SUPPORTED   */

#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '$'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS320C2xxx (Ankoor) Target Specific Parameters (16-bit word-addressable)*/
/*--------------------------------------------------------------------------*/
#if RTC
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x0	/* 32-bit address space (C28)      */

#define _CSTART "_c_int00"	/* C entry point symbol            */

#define FILL_MIN_WIDTH  2	/* minimum fill value width, in    */
					/* bytes.                          */
#define FILL_MAX_WIDTH  2	/* maximum fill value width, in    */
					/* bytes.                          */
#define FILL_FLAGS      0	/* extra flags for FILL sections   */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	2	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	0	/* Number of LSBs of address that are page number */

/* Custom version field (these are _not_ to be or'ed together)             */
/* Make sure these stay in synch with their duplicates in asm27/t_params.h */

#define F_VERS27        0x00	/* C27x mode                       */
#define F_VERS28        0x10	/* C28x mode                       */
#define F_VERS28A       0x20	/* C28x mode with AMODE enabled    */
#define F_UNUSED3       0x30	/* Reserved for future use         */
#define F_UNUSED4       0x40	/* Reserved for future use         */
#define F_UNUSED15      0xF0	/* Reserved for future use         */

/* Block size (data page) is 64 for C27x and C28x with AMODE off,          */
/*                          128 for C28x with AMODE on                     */

#define SBLOCKSIZE   (((globflg & F_VERSION) == F_VERS28A) ? 0x80L : 0x40L)

#define COFF_VERSION_0	0
#define COFF_VERSION_1	0
#define COFF_VERSION_2	1	/* Ankkoor uses COFF format 2     */
#define COFF_TARG_MIN   0	/* MINIMUM COFF VERSION SUPPORTED   */

#define HLL_PREFIX      '_'	/* HLL prefix to symbol names       */
#define HLL_ALT_PREFIX  '$'	/* HLL prefix to secondary sym name */
#define HLL_FCN_PREFIX  '_'	/* HLL prefix to function names     */

#define LENIENT_SIGNED_RELEXPS 0	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define trg_is_tramp_call(t)    (false)
#define trg_call_type(t)        (0)
#endif

/*--------------------------------------------------------------------------*/
/* TMS320C55xx (LEAD3) Target Specific Parameters (byte-addressable code)   */
/*                                         (16-bit word-addressable data)   */
/*--------------------------------------------------------------------------*/
#if LEAD3
#define MEMORG          0x0L	/* Size of configured memory       */
#define MEMSIZE         0x1000000L	/* 16M - 24-bit address space      */

#define CINIT_ALIGN     1	/* alignment of cinit record in TDATA AUs */
#define CINIT_COUNT		1	/* width of count field in TDATA AUs */
#define CINIT_ADDRESS	2	/* width of address field in TDATA AUs */
#define CINIT_PAGE_BITS	8	/* Number of LSBs of address that are page number */

#define DATA_RUN2LOAD(zz) ((zz) << 1)
/* translate data run address to load address */
#define TDATA_AU_BITS	16	/* bytes per data AU */
#define LOG_TDATA_AU_BITS	4

#define LENIENT_SIGNED_RELEXPS 1	/* DOES SIGNED ALLOW MAX UNSIGNED   */

#define TDATA_TO_TADDR(zz) ((zz) << 1)
#define TADDR_TO_TDATA(zz) ((zz) >> 1)

#define TARGET_WORD_ALIGN(zz) (((zz) + 0x1) & -0x2)	
/* align a target address to a word boundary */

#define TARGET_ENDIANNESS 1	/* always big-endian */

#endif

/*--------------------------------------------------------------------------
 *
 *			DEFAULT SETTINGS and DERIVED PROPERTIES
 *
 * This section establishes defaults for values not specified above
 *--------------------------------------------------------------------------*/
#ifndef TARGET_AU_BITS
#define TARGET_AU_BITS 8	/* width of the target addressable unit */
#define LOG_TARGET_AU_BITS 3	/* log2 of same */
#endif

#ifndef CINIT_DEFAULT_PAGE
#define CINIT_DEFAULT_PAGE 0	/* default .cinit page number */
#endif

#ifndef DATA_RUN2LOAD
#define DATA_RUN2LOAD(zz) (zz)	/* translate data run address to load address */
#endif

#ifndef DBG_LIST_PAGE
#define DBG_LIST_PAGE 0		/* page number for .dllview section */
#endif

#ifndef TARGET_WORD_ALIGN
#define TARGET_WORD_ALIGN(zz) (zz)
/* align a target address to a word boundary */
#endif

#ifndef TDATA_TO_TADDR
#define TDATA_TO_TADDR(zz) (zz)	/* target data address to target AU address */
#define TADDR_TO_TDATA(zz) (zz)	/* target AU address to target data address */
#define TDATA_AU_BITS	TARGET_AU_BITS	/* bits per data AU */
#define LOG_TDATA_AU_BITS	LOG_TARGET_AU_BITS
#endif

/*
 *
 * Useful properties and conversions derived from the above
 *
 */

/*
 * Conversions between host and target addresses
 */
#if LOG_BITS_PER_AU == LOG_TARGET_AU_BITS
#define TADDR_TO_HOST(x) (x)	
/* translate target addressable unit to host address */
#define HOST_TO_TADDR(x) (x)	
/* translate host address to target addressable unit */
#elif LOG_BITS_PER_AU > LOG_TARGET_AU_BITS
#define TADDR_TO_HOST(x) ((x) >> (LOG_BITS_PER_AU-LOG_TARGET_AU_BITS))
#define HOST_TO_TADDR(x) ((x) << (LOG_BITS_PER_AU-LOG_TARGET_AU_BITS))
#else
#define TADDR_TO_HOST(x) ((x) << (LOG_TARGET_AU_BITS-LOG_BITS_PER_AU))
#define HOST_TO_TADDR(x) ((x) >> (LOG_TARGET_AU_BITS-LOG_BITS_PER_AU))
#endif

#if LOG_BITS_PER_AU == LOG_TDATA_AU_BITS
#define TDATA_TO_HOST(x) (x)	
/* translate target addressable unit to host address */
#define HOST_TO_TDATA(x) (x)	
/* translate host address to target addressable unit */
#define HOST_TO_TDATA_ROUND(x) (x)	
/* translate host address to target addressable unit, round up */
#define BYTE_TO_HOST_TDATA_ROUND(x) BYTE_TO_HOST_ROUND(x)
/* byte offset to host offset, rounded up for TDATA size */
#elif LOG_BITS_PER_AU > LOG_TDATA_AU_BITS
#define TDATA_TO_HOST(x) ((x) >> (LOG_BITS_PER_AU-LOG_TDATA_AU_BITS))
#define HOST_TO_TDATA(x) ((x) << (LOG_BITS_PER_AU-LOG_TDATA_AU_BITS))
#define HOST_TO_TDATA_ROUND(x) ((x) << (LOG_BITS_PER_AU-LOG_TDATA_AU_BITS))
#define BYTE_TO_HOST_TDATA_ROUND(x) BYTE_TO_HOST_ROUND(x)
#else
#define TDATA_TO_HOST(x) ((x) << (LOG_TDATA_AU_BITS-LOG_BITS_PER_AU))
#define HOST_TO_TDATA(x) ((x) >> (LOG_TDATA_AU_BITS-LOG_BITS_PER_AU))
#define HOST_TO_TDATA_ROUND(x) (((x) +\
					(1<<(LOG_TDATA_AU_BITS-LOG_BITS_PER_AU))-1) >>\
									(LOG_TDATA_AU_BITS-LOG_BITS_PER_AU))
#define BYTE_TO_HOST_TDATA_ROUND(x) (BYTE_TO_HOST((x) + \
							(1<<(LOG_TDATA_AU_BITS-LOG_BITS_PER_BYTE))-1) &\
												-(TDATA_AU_BITS/BITS_PER_AU))
#endif

/*
 * Input in DOFF format is always expresed in bytes, regardless of loading host
 * so we wind up converting from bytes to target and host units even when the
 * host is not a byte machine.
 */
#if LOG_BITS_PER_AU == LOG_BITS_PER_BYTE
#define BYTE_TO_HOST(x) (x)
#define BYTE_TO_HOST_ROUND(x) (x)
#define HOST_TO_BYTE(x) (x)
#elif LOG_BITS_PER_AU >= LOG_BITS_PER_BYTE
#define BYTE_TO_HOST(x) ((x) >> (LOG_BITS_PER_AU - LOG_BITS_PER_BYTE))
#define BYTE_TO_HOST_ROUND(x) ((x + (BITS_PER_AU/BITS_PER_BYTE-1)) >>\
									(LOG_BITS_PER_AU - LOG_BITS_PER_BYTE))
#define HOST_TO_BYTE(x) ((x) << (LOG_BITS_PER_AU - LOG_BITS_PER_BYTE))
#else
/* lets not try to deal with sub-8-bit byte machines */
#endif

#if LOG_TARGET_AU_BITS == LOG_BITS_PER_BYTE
#define TADDR_TO_BYTE(x) (x)	
/* translate target addressable unit to byte address */
#define BYTE_TO_TADDR(x) (x)	
/* translate byte address to target addressable unit */
#elif LOG_TARGET_AU_BITS > LOG_BITS_PER_BYTE
#define TADDR_TO_BYTE(x) ((x) << (LOG_TARGET_AU_BITS-LOG_BITS_PER_BYTE))
#define BYTE_TO_TADDR(x) ((x) >> (LOG_TARGET_AU_BITS-LOG_BITS_PER_BYTE))
#else
/* lets not try to deal with sub-8-bit byte machines */
#endif

/* _BIG_ENDIAN Not defined by bridge driver */
/*#ifdef _BIG_ENDIAN
#define HOST_ENDIANNESS 1
#else*/
#define HOST_ENDIANNESS 0
/*#endif*/

#ifdef TARGET_ENDIANNESS
#define TARGET_ENDIANNESS_DIFFERS(rtend) (HOST_ENDIANNESS^TARGET_ENDIANNESS)
#elif HOST_ENDIANNESS
#define TARGET_ENDIANNESS_DIFFERS(rtend) (!(rtend))
#else
#define TARGET_ENDIANNESS_DIFFERS(rtend) (rtend)
#endif

/* the unit in which we process target image data */
#if TARGET_AU_BITS <= 8
typedef uint_least8_t TgtAU_t;
#elif TARGET_AU_BITS <= 16
typedef uint_least16_t TgtAU_t;
#else
typedef uint_least32_t TgtAU_t;
#endif

/* size of that unit */
#if TARGET_AU_BITS < BITS_PER_AU
#define TGTAU_BITS BITS_PER_AU
#define LOG_TGTAU_BITS LOG_BITS_PER_AU
#else
#define TGTAU_BITS TARGET_AU_BITS
#define LOG_TGTAU_BITS LOG_TARGET_AU_BITS
#endif



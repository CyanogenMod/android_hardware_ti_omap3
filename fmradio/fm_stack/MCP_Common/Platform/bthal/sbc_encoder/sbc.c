/****************************************************************************
 *
 * File:
 *     $Workfile:sbc.c$ for iAnywhere AV SDK, version 1.2.3
 *     $Revision:96$
 *
 * Description: This file contains the SBC codec for the A2DP profile.
 *             
 * Created:     August 6, 2004
 *
 * Copyright 2004 - 2005 Extended Systems, Inc.

 * Portions copyright 2005-2006 iAnywhere Solutions, Inc.

 * All rights reserved. All unpublished rights reserved.
 *
 * Unpublished Confidential Information of iAnywhere Solutions, Inc.  
 * Do Not Disclose.
 *
 * No part of this work may be used or reproduced in any form or by any means, 
 * or stored in a database or retrieval system, without prior written 
 * permission of iAnywhere Solutions, Inc.
 * 
 * Use of this work is governed by a license granted by iAnywhere Solutions, Inc.  
 * This work contains confidential and proprietary information of Extended 
 * Systems, Inc. which is protected by copyright, trade secret, trademark and 
 * other intellectual property rights.
 *
 ****************************************************************************/

/*
#pragma arm
#pragma Otime
#pragma O3 
*/

#include "sbc.h"
//#include <sbc_math.h>

#if SBC_ENABLE_DEBUG_ANALISYS8
TimeT       total_analisys8_cpu = 0;
#endif

#if SBC_DECODER == XA_ENABLED
/* Function prototypes */
static void SbcResetDecoderState(SbcDecoder *Decoder);
#endif

extern void * memset ( void *, int, size_t );

/****************************************************************************
 *
 * ROMable data
 *
 ****************************************************************************/

/*---------------------------------------------------------------------------
 *
 * Reversed CRC lookup table for CRC-8, Poly: G(X) = X8 + X4 + X3 + X2 + 1.
 */
const U8 SbcCrcTable[256] = {
    0x00, 0x1D, 0x3A, 0x27, 0x74, 0x69, 0x4E, 0x53, 0xE8, 0xF5, 0xD2, 0xCF,
    0x9C, 0x81, 0xA6, 0xBB, 0xCD, 0xD0, 0xF7, 0xEA, 0xB9, 0xA4, 0x83, 0x9E,
    0x25, 0x38, 0x1F, 0x02, 0x51, 0x4C, 0x6B, 0x76, 0x87, 0x9A, 0xBD, 0xA0,
    0xF3, 0xEE, 0xC9, 0xD4, 0x6F, 0x72, 0x55, 0x48, 0x1B, 0x06, 0x21, 0x3C,
    0x4A, 0x57, 0x70, 0x6D, 0x3E, 0x23, 0x04, 0x19, 0xA2, 0xBF, 0x98, 0x85,
    0xD6, 0xCB, 0xEC, 0xF1, 0x13, 0x0E, 0x29, 0x34, 0x67, 0x7A, 0x5D, 0x40,
    0xFB, 0xE6, 0xC1, 0xDC, 0x8F, 0x92, 0xB5, 0xA8, 0xDE, 0xC3, 0xE4, 0xF9,
    0xAA, 0xB7, 0x90, 0x8D, 0x36, 0x2B, 0x0C, 0x11, 0x42, 0x5F, 0x78, 0x65,
    0x94, 0x89, 0xAE, 0xB3, 0xE0, 0xFD, 0xDA, 0xC7, 0x7C, 0x61, 0x46, 0x5B,
    0x08, 0x15, 0x32, 0x2F, 0x59, 0x44, 0x63, 0x7E, 0x2D, 0x30, 0x17, 0x0A,
    0xB1, 0xAC, 0x8B, 0x96, 0xC5, 0xD8, 0xFF, 0xE2, 0x26, 0x3B, 0x1C, 0x01,
    0x52, 0x4F, 0x68, 0x75, 0xCE, 0xD3, 0xF4, 0xE9, 0xBA, 0xA7, 0x80, 0x9D,
    0xEB, 0xF6, 0xD1, 0xCC, 0x9F, 0x82, 0xA5, 0xB8, 0x03, 0x1E, 0x39, 0x24,
    0x77, 0x6A, 0x4D, 0x50, 0xA1, 0xBC, 0x9B, 0x86, 0xD5, 0xC8, 0xEF, 0xF2,
    0x49, 0x54, 0x73, 0x6E, 0x3D, 0x20, 0x07, 0x1A, 0x6C, 0x71, 0x56, 0x4B,
    0x18, 0x05, 0x22, 0x3F, 0x84, 0x99, 0xBE, 0xA3, 0xF0, 0xED, 0xCA, 0xD7,
    0x35, 0x28, 0x0F, 0x12, 0x41, 0x5C, 0x7B, 0x66, 0xDD, 0xC0, 0xE7, 0xFA,
    0xA9, 0xB4, 0x93, 0x8E, 0xF8, 0xE5, 0xC2, 0xDF, 0x8C, 0x91, 0xB6, 0xAB,
    0x10, 0x0D, 0x2A, 0x37, 0x64, 0x79, 0x5E, 0x43, 0xB2, 0xAF, 0x88, 0x95,
    0xC6, 0xDB, 0xFC, 0xE1, 0x5A, 0x47, 0x60, 0x7D, 0x2E, 0x33, 0x14, 0x09,
    0x7F, 0x62, 0x45, 0x58, 0x0B, 0x16, 0x31, 0x2C, 0x97, 0x8A, 0xAD, 0xB0,
    0xE3, 0xFE, 0xD9, 0xC4
};

/*---------------------------------------------------------------------------
 *
 * Loudness offset table for bit allocation (4 subbands).
 */
const S8 LoudnessOffset4[4][4] = {
    {(S8)-1,  0,  0,  0},
    {(S8)-2,  0,  0,  1},
    {(S8)-2,  0,  0,  1},
    {(S8)-2,  0,  0,  1}
};

/*---------------------------------------------------------------------------
 *
 * Loudness offset table for bit allocation (8 subbands).
 */
const S8 LoudnessOffset8[4][8] = {
    {(S8)-2,  0,  0,  0,  0,  0,  0,  1},
    {(S8)-3,  0,  0,  0,  0,  0,  1,  2},
    {(S8)-4,  0,  0,  0,  0,  0,  1,  2},
    {(S8)-4,  0,  0,  0,  0,  0,  1,  2}
};

#if SBC_USE_FIXED_POINT == XA_ENABLED

#if SBC_DECODER == XA_ENABLED
/*---------------------------------------------------------------------------
 *
 * Discrete Cosine Table for Synthesis filter (4 Subbands).
 */
const REAL Synth4[8][4] = {
    {0x000016A0, 0xFFFFE95F, 0xFFFFE95F, 0x000016A0},
    {0x00000C3E, 0xFFFFE26F, 0x00001D90, 0xFFFFF3C1},
    {0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0xFFFFF3C1, 0x00001D90, 0xFFFFE26F, 0x00000C3E},
    {0xFFFFE95F, 0x000016A0, 0x000016A0, 0xFFFFE95F},
    {0xFFFFE26F, 0xFFFFF3C1, 0x00000C3E, 0x00001D90},
    {0xFFFFE000, 0xFFFFE000, 0xFFFFE000, 0xFFFFE000},
    {0xFFFFE26F, 0xFFFFF3C1, 0x00000C3E, 0x00001D90}
};

/*---------------------------------------------------------------------------
 *
 * Discrete Cosine Table for Synthesis filter (8 Subbands).
 */
const REAL Synth8[16][8] = {
    {0x000016A0, 0xFFFFE95F, 0xFFFFE95F, 0x000016A0, 
     0x000016A0, 0xFFFFE95F, 0xFFFFE95F, 0x000016A0},
    {0x000011C7, 0xFFFFE09D, 0x0000063E, 0x00001A9B, 
     0xFFFFE564, 0xFFFFF9C1, 0x00001F62, 0xFFFFEE38},
    {0x00000C3E, 0xFFFFE26F, 0x00001D90, 0xFFFFF3C1, 
     0xFFFFF3C1, 0x00001D90, 0xFFFFE26F, 0x00000C3E},
    {0x0000063E, 0xFFFFEE38, 0x00001A9B, 0xFFFFE09D, 
     0x00001F62, 0xFFFFE564, 0x000011C7, 0xFFFFF9C1},
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, 
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0xFFFFF9C1, 0x000011C7, 0xFFFFE564, 0x00001F62, 
     0xFFFFE09D, 0x00001A9B, 0xFFFFEE38, 0x0000063E},
    {0xFFFFF3C1, 0x00001D90, 0xFFFFE26F, 0x00000C3E, 
     0x00000C3E, 0xFFFFE26F, 0x00001D90, 0xFFFFF3C1},
    {0xFFFFEE38, 0x00001F62, 0xFFFFF9C1, 0xFFFFE564, 
     0x00001A9B, 0x0000063E, 0xFFFFE09D, 0x000011C7},
    {0xFFFFE95F, 0x000016A0, 0x000016A0, 0xFFFFE95F, 
     0xFFFFE95F, 0x000016A0, 0x000016A0, 0xFFFFE95F},
    {0xFFFFE564, 0x0000063E, 0x00001F62, 0x000011C7, 
     0xFFFFEE38, 0xFFFFE09D, 0xFFFFF9C1, 0x00001A9B},
    {0xFFFFE26F, 0xFFFFF3C1, 0x00000C3E, 0x00001D90, 
     0x00001D90, 0x00000C3E, 0xFFFFF3C1, 0xFFFFE26F},
    {0xFFFFE09D, 0xFFFFE564, 0xFFFFEE38, 0xFFFFF9C1, 
     0x0000063E, 0x000011C7, 0x00001A9B, 0x00001F62},
    {0xFFFFE000, 0xFFFFE000, 0xFFFFE000, 0xFFFFE000, 
     0xFFFFE000, 0xFFFFE000, 0xFFFFE000, 0xFFFFE000},
    {0xFFFFE09D, 0xFFFFE564, 0xFFFFEE38, 0xFFFFF9C1, 
     0x0000063E, 0x000011C7, 0x00001A9B, 0x00001F62},
    {0xFFFFE26F, 0xFFFFF3C1, 0x00000C3E, 0x00001D90, 
     0x00001D90, 0x00000C3E, 0xFFFFF3C1, 0xFFFFE26F},
    {0xFFFFE564, 0x0000063E, 0x00001F62, 0x000011C7, 
     0xFFFFEE38, 0xFFFFE09D, 0xFFFFF9C1, 0x00001A9B}
};

/*---------------------------------------------------------------------------
 *
 * Filter Coefficients Table for Decode (4 subbands).
 */
const REAL SbcDecodeCoeff4[40] = {
    0x00000000, 0xFFFFFFEE, 0xFFFFFFCF, 0xFFFFFFA6, 
    0xFFFFFF82, 0xFFFFFF80, 0xFFFFFFC2, 0x00000064, 
    0xFFFFFE9A, 0xFFFFFD62, 0xFFFFFC4D, 0xFFFFFBE1, 
    0xFFFFFCB0, 0xFFFFFF37, 0x000003B0, 0x000009F0, 
    0xFFFFEEA4, 0xFFFFE70A, 0xFFFFE06E, 0xFFFFDBED, 
    0xFFFFDA53, 0xFFFFDBED, 0xFFFFE06E, 0xFFFFE70A, 
    0x0000115B, 0x000009F0, 0x000003B0, 0xFFFFFF37, 
    0xFFFFFCB0, 0xFFFFFBE1, 0xFFFFFC4D, 0xFFFFFD62, 
    0x00000165, 0x00000064, 0xFFFFFFC2, 0xFFFFFF80, 
    0xFFFFFF82, 0xFFFFFFA6, 0xFFFFFFCF, 0xFFFFFFEE
};

/*---------------------------------------------------------------------------
 *
 * Filter Coefficients Table for Decode (8 subbands).
 */
const REAL SbcDecodeCoeff8[80] = {
    0x00000000, 0xFFFFFFF5, 0xFFFFFFE9, 0xFFFFFFDB, 
    0xFFFFFFCA, 0xFFFFFFB5, 0xFFFFFF9F, 0xFFFFFF8B, 
    0xFFFFFF7C, 0xFFFFFF76, 0xFFFFFF7D, 0xFFFFFF96, 
    0xFFFFFFC4, 0x0000000B, 0x0000006C, 0x000000E5, 
    0xFFFFFE8D, 0xFFFFFDF1, 0xFFFFFD52, 0xFFFFFCBC, 
    0xFFFFFC3F, 0xFFFFFBED, 0xFFFFFBD8, 0xFFFFFC14, 
    0xFFFFFCB0, 0xFFFFFDBB, 0xFFFFFF40, 0x00000142, 
    0x000003BF, 0x000006AF, 0x00000A00, 0x00000D9D, 
    0xFFFFEE97, 0xFFFFEAC1, 0xFFFFE705, 0xFFFFE388, 
    0xFFFFE071, 0xFFFFDDE2, 0xFFFFDBF7, 0xFFFFDAC7, 
    0xFFFFDA61, 0xFFFFDAC7, 0xFFFFDBF7, 0xFFFFDDE2, 
    0xFFFFE071, 0xFFFFE388, 0xFFFFE705, 0xFFFFEAC1, 
    0x00001168, 0x00000D9D, 0x00000A00, 0x000006AF, 
    0x000003BF, 0x00000142, 0xFFFFFF40, 0xFFFFFDBB, 
    0xFFFFFCB0, 0xFFFFFC14, 0xFFFFFBD8, 0xFFFFFBED, 
    0xFFFFFC3F, 0xFFFFFCBC, 0xFFFFFD52, 0xFFFFFDF1, 
    0x00000172, 0x000000E5, 0x0000006C, 0x0000000B, 
    0xFFFFFFC4, 0xFFFFFF96, 0xFFFFFF7D, 0xFFFFFF76, 
    0xFFFFFF7C, 0xFFFFFF8B, 0xFFFFFF9F, 0xFFFFFFB5, 
    0xFFFFFFCA, 0xFFFFFFDB, 0xFFFFFFE9, 0xFFFFFFF5
};
#endif /* SBC_DECODER == XA_ENABLED */

#if SBC_ENCODER == XA_ENABLED
/*---------------------------------------------------------------------------
 *
 * Discrete Cosine Table for Analysis filter (4 Subbands).
 */
const REAL Analyze4[8][4] = {
    {0x2D413CCB, 0xD2BEC335, 0xD2BEC334, 0x2D413CCB},
    {0x3B20D79D, 0x187DE2A5, 0xE7821D5B, 0xC4DF2863},
    {0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF},
    {0x3B20D79D, 0x187DE2A5, 0xE7821D5B, 0xC4DF2863},
    {0x2D413CCB, 0xD2BEC335, 0xD2BEC334, 0x2D413CCB},
    {0x187DE2A5, 0xC4DF2863, 0x3B20D79D, 0xE7821D5B},
    {0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0xE7821D5B, 0x3B20D79D, 0xC4DF2863, 0x187DE2A5}
};

/*---------------------------------------------------------------------------
 *
 * Discrete Cosine Table for Analysis filter (8 Subbands).
 */
const REAL Analyze8[16][8] = {
    {0x2D413CCB, 0xD2BEC335, 0xD2BEC334, 0x2D413CCB, 
     0x2D413CCC, 0xD2BEC335, 0xD2BEC334, 0x2D413CCB},
    {0x3536CC51, 0xF383A3E3, 0xC13AD062, 0xDC71898E, 
     0x238E7672, 0x3EC52F9E, 0x0C7C5C1D, 0xCAC933AF},
    {0x3B20D79D, 0x187DE2A5, 0xE7821D5B, 0xC4DF2863, 
     0xC4DF2863, 0xE7821D5B, 0x187DE2A5, 0x3B20D79D},
    {0x3EC52F9E, 0x3536CC51, 0x238E7672, 0x0C7C5C1D, 
     0xF383A3E3, 0xDC71898E, 0xCAC933AF, 0xC13AD062},
    {0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF, 
     0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF},
    {0x3EC52F9E, 0x3536CC51, 0x238E7672, 0x0C7C5C1D, 
     0xF383A3E3, 0xDC71898E, 0xCAC933AF, 0xC13AD062},
    {0x3B20D79D, 0x187DE2A5, 0xE7821D5B, 0xC4DF2863, 
     0xC4DF2863, 0xE7821D5B, 0x187DE2A5, 0x3B20D79D},
    {0x3536CC51, 0xF383A3E3, 0xC13AD062, 0xDC71898E, 
     0x238E7672, 0x3EC52F9E, 0x0C7C5C1D, 0xCAC933AF},
    {0x2D413CCB, 0xD2BEC335, 0xD2BEC334, 0x2D413CCB, 
     0x2D413CCC, 0xD2BEC335, 0xD2BEC334, 0x2D413CCB},
    {0x238E7672, 0xC13AD062, 0x0C7C5C1D, 0x3536CC51, 
     0xCAC933AF, 0xF383A3E3, 0x3EC52F9E, 0xDC71898E},
    {0x187DE2A5, 0xC4DF2863, 0x3B20D79D, 0xE7821D5B, 
     0xE7821D5B, 0x3B20D79D, 0xC4DF2863, 0x187DE2A5},
    {0x0C7C5C1D, 0xDC71898E, 0x3536CC51, 0xC13AD062, 
     0x3EC52F9E, 0xCAC933AF, 0x238E7672, 0xF383A3E3},
    {0x00000000, 0x00000000, 0x00000000, 0x00000000, 
     0x00000000, 0x00000000, 0x00000000, 0x00000000},
    {0xF383A3E3, 0x238E7672, 0xCAC933AF, 0x3EC52F9E, 
     0xC13AD062, 0x3536CC51, 0xDC71898E, 0x0C7C5C1D},
    {0xE7821D5B, 0x3B20D79D, 0xC4DF2863, 0x187DE2A5, 
     0x187DE2A5, 0xC4DF2863, 0x3B20D79D, 0xE7821D5B},
    {0xDC71898E, 0x3EC52F9E, 0xF383A3E3, 0xCAC933AF, 
     0x3536CC51, 0x0C7C5C1D, 0xC13AD062, 0x238E7672}
};

/*---------------------------------------------------------------------------
 *
 * Filter Coefficients Table for Encode (4 subbands).
 */
const REAL SbcCoefficient4[40] = {
    0x00000000, 0x0008CA72, 0x00187168, 0x002CCA00, 
    0x003EDE63, 0x003FC471, 0x001E91CC, 0xFFCDDCEC, 
    0x00B2CFA1, 0x014EDD50, 0x01D9199F, 0x020F771F, 
    0x01A7F715, 0x0064795D, 0xFE27C8EB, 0xFB07D79D, 
    0x08AD8F68, 0x0C7AAE47, 0x0FC8E522, 0x12097927, 
    0x12D60FF8, 0x12097927, 0x0FC8E522, 0x0C7AAE47, 
    0xF7527098, 0xFB07D79D, 0xFE27C8EB, 0x0064795D, 
    0x01A7F715, 0x020F771F, 0x01D9199F, 0x014EDD50, 
    0xFF4D305F, 0xFFCDDCEC, 0x001E91CC, 0x003FC471, 
    0x003EDE63, 0x002CCA00, 0x00187168, 0x0008CA72
};

/*---------------------------------------------------------------------------
 *
 * Filter Coefficients Table for Encode (8 subbands).
 */
const REAL SbcCoefficient8[80] = {
    0x00000000, 0x000290B8, 0x00059FB7, 0x0009163D, 
    0x000D7FC3, 0x0012AD30, 0x00183079, 0x001D3972, 
    0x0020F634, 0x002277A3, 0x0020ADB9, 0x001A7C5A, 
    0x000EC7E8, 0xFFFD120B, 0xFFE4F888, 0xFFC6B3CB, 
    0x005CB9A3, 0x00838DCA, 0x00AB59E4, 0x00D0D9C4, 
    0x00F01125, 0x0104948E, 0x0109C328, 0x00FAFA11, 
    0x00D3F676, 0x00911F5C, 0x002FE87E, 0xFFAF75BB, 
    0xFF1021B0, 0xFE543740, 0xFD7FCAFF, 0xFC98944A, 
    0x045A183C, 0x054F9F4C, 0x063EACDA, 0x071DD8B6, 
    0x07E390FB, 0x08876777, 0x09021AFC, 0x094E1136, 
    0x0967B639, 0x094E1136, 0x09021AFC, 0x08876777, 
    0x07E390FB, 0x071DD8B6, 0x063EACDA, 0x054F9F4C, 
    0xFBA5E7C4, 0xFC98944A, 0xFD7FCAFF, 0xFE543740, 
    0xFF1021B0, 0xFFAF75BB, 0x002FE87E, 0x00911F5C, 
    0x00D3F676, 0x00FAFA11, 0x0109C328, 0x0104948E, 
    0x00F01125, 0x00D0D9C4, 0x00AB59E4, 0x00838DCA, 
    0xFFA3465D, 0xFFC6B3CB, 0xFFE4F888, 0xFFFD120B, 
    0x000EC7E8, 0x001A7C5A, 0x0020ADB9, 0x002277A3, 
    0x0020F634, 0x001D3972, 0x00183079, 0x0012AD30, 
    0x000D7FC3, 0x0009163D, 0x00059FB7, 0x000290B8
};

#endif /* SBC_ENCODER == XA_ENABLED */

#else

#if SBC_DECODER == XA_ENABLED
/*---------------------------------------------------------------------------
 *
 * Discrete Cosine Table for Synthesis filter (4 Subbands).
 */
const REAL Synth4[8][4] = {
    {(REAL) 7.0710678119e-001, (REAL)-7.0710678118e-001, (REAL)-7.0710678120e-001, (REAL) 7.0710678117e-001},
    {(REAL) 3.8268343237e-001, (REAL)-9.2387953252e-001, (REAL) 9.2387953250e-001, (REAL)-3.8268343234e-001},
    {(REAL) 4.8965888581e-012, (REAL)-1.4689766574e-011, (REAL) 2.4482944291e-011, (REAL)-3.4275233828e-011},
    {(REAL)-3.8268343236e-001, (REAL) 9.2387953250e-001, (REAL)-9.2387953252e-001, (REAL) 3.8268343240e-001},
    {(REAL)-7.0710678118e-001, (REAL) 7.0710678120e-001, (REAL) 7.0710678116e-001, (REAL)-7.0710678122e-001},
    {(REAL)-9.2387953251e-001, (REAL)-3.8268343234e-001, (REAL) 3.8268343240e-001, (REAL) 9.2387953253e-001},
    {(REAL)-1.0000000000e+000, (REAL)-1.0000000000e+000, (REAL)-1.0000000000e+000, (REAL)-1.0000000000e+000},
    {(REAL)-9.2387953252e-001, (REAL)-3.8268343240e-001, (REAL) 3.8268343231e-001, (REAL) 9.2387953248e-001}
};

/*---------------------------------------------------------------------------
 *
 * Discrete Cosine Table for Synthesis filter (8 Subbands).
 */
const REAL Synth8[16][8] = {
    {(REAL) 7.0710678119e-001, (REAL)-7.0710678118e-001, (REAL)-7.0710678120e-001, (REAL) 7.0710678117e-001, 
     (REAL) 7.0710678120e-001, (REAL)-7.0710678117e-001, (REAL)-7.0710678121e-001, (REAL) 7.0710678116e-001},
    {(REAL) 5.5557023302e-001, (REAL)-9.8078528040e-001, (REAL) 1.9509032200e-001, (REAL) 8.3146961231e-001, 
     (REAL)-8.3146961229e-001, (REAL)-1.9509032205e-001, (REAL) 9.8078528041e-001, (REAL)-5.5557023298e-001},
    {(REAL) 3.8268343237e-001, (REAL)-9.2387953252e-001, (REAL) 9.2387953250e-001, (REAL)-3.8268343234e-001, 
     (REAL)-3.8268343240e-001, (REAL) 9.2387953253e-001, (REAL)-9.2387953249e-001, (REAL) 3.8268343231e-001},
    {(REAL) 1.9509032202e-001, (REAL)-5.5557023303e-001, (REAL) 8.3146961231e-001, (REAL)-9.8078528041e-001, 
     (REAL) 9.8078528040e-001, (REAL)-8.3146961228e-001, (REAL) 5.5557023297e-001, (REAL)-1.9509032195e-001},
    {(REAL) 4.8965888581e-012, (REAL)-1.4689766574e-011, (REAL) 2.4482944291e-011, (REAL)-3.4275233828e-011, 
     (REAL) 4.4070187902e-011, (REAL)-5.3861589261e-011, (REAL) 6.3656543334e-011, (REAL)-7.3447944693e-011},
    {(REAL)-1.9509032201e-001, (REAL) 5.5557023301e-001, (REAL)-8.3146961229e-001, (REAL) 9.8078528040e-001, 
     (REAL)-9.8078528041e-001, (REAL) 8.3146961234e-001, (REAL)-5.5557023308e-001, (REAL) 1.9509032210e-001},
    {(REAL)-3.8268343236e-001, (REAL) 9.2387953250e-001, (REAL)-9.2387953252e-001, (REAL) 3.8268343240e-001, 
     (REAL) 3.8268343231e-001, (REAL)-9.2387953249e-001, (REAL) 9.2387953254e-001, (REAL)-3.8268343245e-001},
    {(REAL)-5.5557023301e-001, (REAL) 9.8078528041e-001, (REAL)-1.9509032205e-001, (REAL)-8.3146961228e-001, 
     (REAL) 8.3146961234e-001, (REAL) 1.9509032194e-001, (REAL)-9.8078528039e-001, (REAL) 5.5557023310e-001},
    {(REAL)-7.0710678118e-001, (REAL) 7.0710678120e-001, (REAL) 7.0710678116e-001, (REAL)-7.0710678122e-001, 
     (REAL)-7.0710678114e-001, (REAL) 7.0710678124e-001, (REAL) 7.0710678112e-001, (REAL)-7.0710678126e-001},
    {(REAL)-8.3146961230e-001, (REAL) 1.9509032204e-001, (REAL) 9.8078528041e-001, (REAL) 5.5557023297e-001, 
     (REAL)-5.5557023308e-001, (REAL)-9.8078528039e-001, (REAL)-1.9509032191e-001, (REAL) 8.3146961237e-001},
    {(REAL)-9.2387953251e-001, (REAL)-3.8268343234e-001, (REAL) 3.8268343240e-001, (REAL) 9.2387953253e-001, 
     (REAL) 9.2387953248e-001, (REAL) 3.8268343228e-001, (REAL)-3.8268343247e-001, (REAL)-9.2387953256e-001},
    {(REAL)-9.8078528040e-001, (REAL)-8.3146961229e-001, (REAL)-5.5557023298e-001, (REAL)-1.9509032195e-001, 
     (REAL) 1.9509032210e-001, (REAL) 5.5557023310e-001, (REAL) 8.3146961237e-001, (REAL) 9.8078528043e-001},
    {(REAL)-1.0000000000e+000, (REAL)-1.0000000000e+000, (REAL)-1.0000000000e+000, (REAL)-1.0000000000e+000, 
     (REAL)-1.0000000000e+000, (REAL)-1.0000000000e+000, (REAL)-1.0000000000e+000, (REAL)-1.0000000000e+000},
    {(REAL)-9.8078528041e-001, (REAL)-8.3146961232e-001, (REAL)-5.5557023306e-001, (REAL)-1.9509032209e-001, 
     (REAL) 1.9509032192e-001, (REAL) 5.5557023292e-001, (REAL) 8.3146961223e-001, (REAL) 9.8078528037e-001},
    {(REAL)-9.2387953252e-001, (REAL)-3.8268343240e-001, (REAL) 3.8268343231e-001, (REAL) 9.2387953248e-001, 
     (REAL) 9.2387953255e-001, (REAL) 3.8268343248e-001, (REAL)-3.8268343223e-001, (REAL)-9.2387953245e-001},
    {(REAL)-8.3146961231e-001, (REAL) 1.9509032198e-001, (REAL) 9.8078528039e-001, (REAL) 5.5557023309e-001, 
     (REAL)-5.5557023293e-001, (REAL)-9.8078528043e-001, (REAL)-1.9509032216e-001, (REAL) 8.3146961221e-001}
};

const REAL SbcDecodeCoeff4[40] = {
    (REAL) 0.0000000000e+000, (REAL)-2.1461959040e-003, (REAL)-5.9675342800e-003, (REAL)-1.0934836160e-002, 
    (REAL)-1.5348807720e-002, (REAL)-1.5568205960e-002, (REAL)-7.4632676400e-003, (REAL) 1.2240491440e-002, 
    (REAL)-4.3655048000e-002, (REAL)-8.1754034800e-002, (REAL)-1.1550295680e-001, (REAL)-1.2877571600e-001, 
    (REAL)-1.0350712440e-001, (REAL)-2.4529807440e-002, (REAL) 1.1528690960e-001, (REAL) 3.1058539760e-001, 
    (REAL)-5.4237309600e-001, (REAL)-7.7995136400e-001, (REAL)-9.8654664800e-001, (REAL)-1.1273128120e+000, 
    (REAL)-1.1772613280e+000, (REAL)-1.1273128120e+000, (REAL)-9.8654664800e-001, (REAL)-7.7995136400e-001, 
    (REAL) 5.4237309600e-001, (REAL) 3.1058539760e-001, (REAL) 1.1528690960e-001, (REAL)-2.4529807440e-002, 
    (REAL)-1.0350712440e-001, (REAL)-1.2877571600e-001, (REAL)-1.1550295680e-001, (REAL)-8.1754034800e-002, 
    (REAL) 4.3655048000e-002, (REAL) 1.2240491440e-002, (REAL)-7.4632676400e-003, (REAL)-1.5568205960e-002, 
    (REAL)-1.5348807720e-002, (REAL)-1.0934836160e-002, (REAL)-5.9675342800e-003, (REAL)-2.1461959040e-003
};

const REAL SbcDecodeCoeff8[80] = {
    (REAL) 0.0000000000e+000, (REAL)-1.2526031840e-003, (REAL)-2.7460514000e-003, (REAL)-4.4369616160e-003, 
    (REAL)-6.5913560480e-003, (REAL)-9.1194005600e-003, (REAL)-1.1811213520e-002, (REAL)-1.4269738000e-002, 
    (REAL)-1.6094603360e-002, (REAL)-1.6829759120e-002, (REAL)-1.5956364320e-002, (REAL)-1.2932502640e-002, 
    (REAL)-7.2172360160e-003, (REAL) 1.4304428880e-003, (REAL) 1.3197847840e-002, (REAL) 2.7977396320e-002, 
    (REAL)-4.5275957840e-002, (REAL)-6.4235293040e-002, (REAL)-8.3667554400e-002, (REAL)-1.0197786800e-001, 
    (REAL)-1.1722021040e-001, (REAL)-1.2723648240e-001, (REAL)-1.2976677680e-001, (REAL)-1.2254728480e-001, 
    (REAL)-1.0349744480e-001, (REAL)-7.0860603200e-002, (REAL)-2.3392675360e-002, (REAL) 3.9326241920e-002, 
    (REAL) 1.1712326080e-001, (REAL) 2.0887900160e-001, (REAL) 3.1260110480e-001, (REAL) 4.2549842560e-001, 
    (REAL)-5.4399154480e-001, (REAL)-6.6387806240e-001, (REAL)-7.8060313440e-001, (REAL)-8.8957351200e-001, 
    (REAL)-9.8611638400e-001, (REAL)-1.0661153200e+000, (REAL)-1.1260280400e+000, (REAL)-1.1631187760e+000, 
    (REAL)-1.1756405440e+000, (REAL)-1.1631187760e+000, (REAL)-1.1260280400e+000, (REAL)-1.0661153200e+000, 
    (REAL)-9.8611638400e-001, (REAL)-8.8957351200e-001, (REAL)-7.8060313440e-001, (REAL)-6.6387806240e-001, 
    (REAL) 5.4399154480e-001, (REAL) 4.2549842560e-001, (REAL) 3.1260110480e-001, (REAL) 2.0887900160e-001, 
    (REAL) 1.1712326080e-001, (REAL) 3.9326241920e-002, (REAL)-2.3392675360e-002, (REAL)-7.0860603200e-002, 
    (REAL)-1.0349744480e-001, (REAL)-1.2254728480e-001, (REAL)-1.2976677680e-001, (REAL)-1.2723648240e-001, 
    (REAL)-1.1722021040e-001, (REAL)-1.0197786800e-001, (REAL)-8.3667554400e-002, (REAL)-6.4235293040e-002, 
    (REAL) 4.5275957840e-002, (REAL) 2.7977396320e-002, (REAL) 1.3197847840e-002, (REAL) 1.4304428880e-003, 
    (REAL)-7.2172360160e-003, (REAL)-1.2932502640e-002, (REAL)-1.5956364320e-002, (REAL)-1.6829759120e-002, 
    (REAL)-1.6094603360e-002, (REAL)-1.4269738000e-002, (REAL)-1.1811213520e-002, (REAL)-9.1194005600e-003, 
    (REAL)-6.5913560480e-003, (REAL)-4.4369616160e-003, (REAL)-2.7460514000e-003, (REAL)-1.2526031840e-003
};

#endif /* SBC_DECODER == XA_ENABLED */

#if SBC_ENCODER == XA_ENABLED
/*---------------------------------------------------------------------------
 *
 * Discrete Cosine Table for Analysis filter (4 Subbands).
 */
const REAL Analyze4[8][4] = {
    { (REAL) 7.0710678119e-001, (REAL)-7.0710678118e-001, (REAL)-7.0710678120e-001, (REAL) 7.0710678117e-001},
    { (REAL) 9.2387953251e-001, (REAL) 3.8268343237e-001, (REAL)-3.8268343236e-001, (REAL)-9.2387953251e-001},
    { (REAL) 1.0000000000e+000, (REAL) 1.0000000000e+000, (REAL) 1.0000000000e+000, (REAL) 1.0000000000e+000},
    { (REAL) 9.2387953251e-001, (REAL) 3.8268343237e-001, (REAL)-3.8268343236e-001, (REAL)-9.2387953251e-001},
    { (REAL) 7.0710678119e-001, (REAL)-7.0710678118e-001, (REAL)-7.0710678120e-001, (REAL) 7.0710678117e-001},
    { (REAL) 3.8268343237e-001, (REAL)-9.2387953252e-001, (REAL) 9.2387953250e-001, (REAL)-3.8268343234e-001},
    { (REAL) 4.8965888581e-012, (REAL)-1.4689766574e-011, (REAL) 2.4482944291e-011, (REAL)-3.4275233828e-011},
    { (REAL)-3.8268343236e-001, (REAL) 9.2387953250e-001, (REAL)-9.2387953252e-001, (REAL) 3.8268343240e-001}
};

/*---------------------------------------------------------------------------
 *
 * Discrete Cosine Table for Analysis filter (8 Subbands).
 */
const REAL Analyze8[16][8] = {
    {(REAL) 7.0710678119e-001, (REAL)-7.0710678118e-001, (REAL)-7.0710678120e-001, (REAL) 7.0710678117e-001, 
     (REAL) 7.0710678120e-001, (REAL)-7.0710678117e-001, (REAL)-7.0710678121e-001, (REAL) 7.0710678116e-001},
    {(REAL) 8.3146961230e-001, (REAL)-1.9509032201e-001, (REAL)-9.8078528040e-001, (REAL)-5.5557023303e-001, 
     (REAL) 5.5557023301e-001, (REAL) 9.8078528041e-001, (REAL) 1.9509032204e-001, (REAL)-8.3146961229e-001},
    {(REAL) 9.2387953251e-001, (REAL) 3.8268343237e-001, (REAL)-3.8268343236e-001, (REAL)-9.2387953251e-001, 
     (REAL)-9.2387953252e-001, (REAL)-3.8268343238e-001, (REAL) 3.8268343235e-001, (REAL) 9.2387953250e-001},
    {(REAL) 9.8078528040e-001, (REAL) 8.3146961230e-001, (REAL) 5.5557023302e-001, (REAL) 1.9509032202e-001, 
     (REAL)-1.9509032201e-001, (REAL)-5.5557023301e-001, (REAL)-8.3146961230e-001, (REAL)-9.8078528040e-001},
    {(REAL) 1.0000000000e+000, (REAL) 1.0000000000e+000, (REAL) 1.0000000000e+000, (REAL) 1.0000000000e+000, 
     (REAL) 1.0000000000e+000, (REAL) 1.0000000000e+000, (REAL) 1.0000000000e+000, (REAL) 1.0000000000e+000},
    {(REAL) 9.8078528040e-001, (REAL) 8.3146961230e-001, (REAL) 5.5557023302e-001, (REAL) 1.9509032202e-001, 
     (REAL)-1.9509032201e-001, (REAL)-5.5557023301e-001, (REAL)-8.3146961230e-001, (REAL)-9.8078528040e-001},
    {(REAL) 9.2387953251e-001, (REAL) 3.8268343237e-001, (REAL)-3.8268343236e-001, (REAL)-9.2387953251e-001, 
     (REAL)-9.2387953252e-001, (REAL)-3.8268343238e-001, (REAL) 3.8268343235e-001, (REAL) 9.2387953250e-001},
    {(REAL) 8.3146961230e-001, (REAL)-1.9509032201e-001, (REAL)-9.8078528040e-001, (REAL)-5.5557023303e-001, 
     (REAL) 5.5557023301e-001, (REAL) 9.8078528041e-001, (REAL) 1.9509032204e-001, (REAL)-8.3146961229e-001},
    {(REAL) 7.0710678119e-001, (REAL)-7.0710678118e-001, (REAL)-7.0710678120e-001, (REAL) 7.0710678117e-001, 
     (REAL) 7.0710678120e-001, (REAL)-7.0710678117e-001, (REAL)-7.0710678121e-001, (REAL) 7.0710678116e-001},
    {(REAL) 5.5557023302e-001, (REAL)-9.8078528040e-001, (REAL) 1.9509032200e-001, (REAL) 8.3146961231e-001, 
     (REAL)-8.3146961229e-001, (REAL)-1.9509032205e-001, (REAL) 9.8078528041e-001, (REAL)-5.5557023298e-001},
    {(REAL) 3.8268343237e-001, (REAL)-9.2387953252e-001, (REAL) 9.2387953250e-001, (REAL)-3.8268343234e-001, 
     (REAL)-3.8268343240e-001, (REAL) 9.2387953253e-001, (REAL)-9.2387953249e-001, (REAL) 3.8268343231e-001},
    {(REAL) 1.9509032202e-001, (REAL)-5.5557023303e-001, (REAL) 8.3146961231e-001, (REAL)-9.8078528041e-001, 
     (REAL) 9.8078528040e-001, (REAL)-8.3146961228e-001, (REAL) 5.5557023297e-001, (REAL)-1.9509032195e-001},
    {(REAL) 4.8965888581e-012, (REAL)-1.4689766574e-011, (REAL) 2.4482944291e-011, (REAL)-3.4275233828e-011, 
     (REAL) 4.4070187902e-011, (REAL)-5.3861589261e-011, (REAL) 6.3656543334e-011, (REAL)-7.3447944693e-011},
    {(REAL)-1.9509032201e-001, (REAL) 5.5557023301e-001, (REAL)-8.3146961229e-001, (REAL) 9.8078528040e-001, 
     (REAL)-9.8078528041e-001, (REAL) 8.3146961234e-001, (REAL)-5.5557023308e-001, (REAL) 1.9509032210e-001},
    {(REAL)-3.8268343236e-001, (REAL) 9.2387953250e-001, (REAL)-9.2387953252e-001, (REAL) 3.8268343240e-001, 
     (REAL) 3.8268343231e-001, (REAL)-9.2387953249e-001, (REAL) 9.2387953254e-001, (REAL)-3.8268343245e-001},
    {(REAL)-5.5557023301e-001, (REAL) 9.8078528041e-001, (REAL)-1.9509032205e-001, (REAL)-8.3146961228e-001, 
     (REAL) 8.3146961234e-001, (REAL) 1.9509032194e-001, (REAL)-9.8078528039e-001, (REAL) 5.5557023310e-001}
};

/*---------------------------------------------------------------------------
 *
 * Filter Coefficients Table (4 subbands).
 */
const REAL SbcCoefficient4[40] = {
   (REAL) 0.00000000E+00, (REAL) 5.36548976E-04, (REAL) 1.49188357E-03, (REAL) 2.73370904E-03,
   (REAL) 3.83720193E-03, (REAL) 3.89205149E-03, (REAL) 1.86581691E-03, (REAL)-3.06012286E-03,
   (REAL) 1.09137620E-02, (REAL) 2.04385087E-02, (REAL) 2.88757392E-02, (REAL) 3.21939290E-02,
   (REAL) 2.58767811E-02, (REAL) 6.13245186E-03, (REAL)-2.88217274E-02, (REAL)-7.76463494E-02,
   (REAL) 1.35593274E-01, (REAL) 1.94987841E-01, (REAL) 2.46636662E-01, (REAL) 2.81828203E-01,
   (REAL) 2.94315332E-01, (REAL) 2.81828203E-01, (REAL) 2.46636662E-01, (REAL) 1.94987841E-01,
   (REAL)-1.35593274E-01, (REAL)-7.76463494E-02, (REAL)-2.88217274E-02, (REAL) 6.13245186E-03,
   (REAL) 2.58767811E-02, (REAL) 3.21939290E-02, (REAL) 2.88757392E-02, (REAL) 2.04385087E-02,
   (REAL)-1.09137620E-02, (REAL)-3.06012286E-03, (REAL) 1.86581691E-03, (REAL) 3.89205149E-03,
   (REAL) 3.83720193E-03, (REAL) 2.73370904E-03, (REAL) 1.49188357E-03, (REAL) 5.36548976E-04
};

/*---------------------------------------------------------------------------
 *
 * Filter Coefficients Table (8 subbands).
 */
const REAL SbcCoefficient8[80] = {
   (REAL) 0.00000000E+00, (REAL) 1.56575398E-04, (REAL) 3.43256425E-04, (REAL) 5.54620202E-04,
   (REAL) 8.23919506E-04, (REAL) 1.13992507E-03, (REAL) 1.47640169E-03, (REAL) 1.78371725E-03,
   (REAL) 2.01182542E-03, (REAL) 2.10371989E-03, (REAL) 1.99454554E-03, (REAL) 1.61656283E-03,
   (REAL) 9.02154502E-04, (REAL)-1.78805361E-04, (REAL)-1.64973098E-03, (REAL)-3.49717454E-03,
   (REAL) 5.65949473E-03, (REAL) 8.02941163E-03, (REAL) 1.04584443E-02, (REAL) 1.27472335E-02,
   (REAL) 1.46525263E-02, (REAL) 1.59045603E-02, (REAL) 1.62208471E-02, (REAL) 1.53184106E-02,
   (REAL) 1.29371806E-02, (REAL) 8.85757540E-03, (REAL) 2.92408442E-03, (REAL)-4.91578024E-03,
   (REAL)-1.46404076E-02, (REAL)-2.61098752E-02, (REAL)-3.90751381E-02, (REAL)-5.31873032E-02,
   (REAL) 6.79989431E-02, (REAL) 8.29847578E-02, (REAL) 9.75753918E-02, (REAL) 1.11196689E-01,
   (REAL) 1.23264548E-01, (REAL) 1.33264415E-01, (REAL) 1.40753505E-01, (REAL) 1.45389847E-01,
   (REAL) 1.46955068E-01, (REAL) 1.45389847E-01, (REAL) 1.40753505E-01, (REAL) 1.33264415E-01,
   (REAL) 1.23264548E-01, (REAL) 1.11196689E-01, (REAL) 9.75753918E-02, (REAL) 8.29847578E-02,
   (REAL)-6.79989431E-02, (REAL)-5.31873032E-02, (REAL)-3.90751381E-02, (REAL)-2.61098752E-02,
   (REAL)-1.46404076E-02, (REAL)-4.91578024E-03, (REAL) 2.92408442E-03, (REAL) 8.85757540E-03,
   (REAL) 1.29371806E-02, (REAL) 1.53184106E-02, (REAL) 1.62208471E-02, (REAL) 1.59045603E-02,
   (REAL) 1.46525263E-02, (REAL) 1.27472335E-02, (REAL) 1.04584443E-02, (REAL) 8.02941163E-03,
   (REAL)-5.65949473E-03, (REAL)-3.49717454E-03, (REAL)-1.64973098E-03, (REAL)-1.78805361E-04,
   (REAL) 9.02154502E-04, (REAL) 1.61656283E-03, (REAL) 1.99454554E-03, (REAL) 2.10371989E-03,
   (REAL) 2.01182542E-03, (REAL) 1.78371725E-03, (REAL) 1.47640169E-03, (REAL) 1.13992507E-03,
   (REAL) 8.23919506E-04, (REAL) 5.54620202E-04, (REAL) 3.43256425E-04, (REAL) 1.56575398E-04
};

#endif /* SBC_ENCODER == XA_ENABLED */

#endif /* SBC_USE_FIXED_POINT == XA_ENABLED */

/****************************************************************************
 *
 * Functions
 *
 ****************************************************************************/

/*---------------------------------------------------------------------------
 *            SbcIsValidStreamInfo()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Checks the validity of an SbcStreamInfo structure.
 */
static BOOL SbcIsValidStreamInfo(SbcStreamInfo *StreamInfo)
{
    /* Check the channel mode */
    switch (StreamInfo->channelMode) {
    case SBC_CHNL_MODE_DUAL_CHNL:
    case SBC_CHNL_MODE_STEREO:
    case SBC_CHNL_MODE_JOINT_STEREO:
    case SBC_CHNL_MODE_MONO:
        break;
    default:
        return FALSE;
    }

    /* Check the sampling frequency */
    if (StreamInfo->sampleFreq > 4) {
        return FALSE;
    }

    /* Check the allocation method */
    if (StreamInfo->allocMethod > 1) {
        return FALSE;
    }

    /* Check the number of blocks */
    switch (StreamInfo->numBlocks) {
    case 4:
    case 8:
    case 12:
    case 16:
        break;
    default:
        return FALSE;
    }

    /* Check the number of subbands */
    if ((StreamInfo->numSubBands !=4) && 
        (StreamInfo->numSubBands !=8)) {
        return FALSE;
    }

    return TRUE;
}

/*---------------------------------------------------------------------------
 *            SBC_FrameLen()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Returns the maximum size of an encoded SBC frame, derived from
 *            the fields in an initialized StreamInfo structure.
 */
U16 SBC_FrameLen(SbcStreamInfo *StreamInfo)
{
    U16      frameLen = 0;
    U32      temp = 0;
//    U32    sampleFreq = 0;
    U8       join = 0;

/* this unusable code was disabled due to a warning cleanup */  
/* U32      sampleFreq; */

#if TI_CHANGES  == XA_ENABLED
    if (StreamInfo->lastSbcFrameLen != 0)
    {
        frameLen = StreamInfo->lastSbcFrameLen;
    }
    else
    {
#endif
        /* Set the number of channels */
        if (StreamInfo->channelMode == SBC_CHNL_MODE_MONO) {
            StreamInfo->numChannels = 1;
        } else {
            StreamInfo->numChannels = 2;
        }

        if (!SbcIsValidStreamInfo(StreamInfo)) {
            goto exit;
        }

        /* this unusable code was disabled due to a warning cleanup */
#if 0
        switch (StreamInfo->sampleFreq) {
        case 0:
            sampleFreq = 16000;
            break;
        case 1:
            sampleFreq = 32000;
            break;
        case 2:
            sampleFreq = 44100;
            break;
        case 3:
            sampleFreq = 48000;
            break;
        }
#endif

        switch (StreamInfo->channelMode) {
        case SBC_CHNL_MODE_MONO:
        case SBC_CHNL_MODE_DUAL_CHNL:

            temp = (U32)(StreamInfo->numBlocks * 
                         StreamInfo->numChannels * 
                         StreamInfo->bitPool);

            break;
        case SBC_CHNL_MODE_JOINT_STEREO:
            join = 1;
        case SBC_CHNL_MODE_STEREO:

            temp = (U32)(join * StreamInfo->numSubBands) + 
                   (U32)(StreamInfo->numBlocks * StreamInfo->bitPool);

            break;
        }

        frameLen = (U16) ((U32)4 + ((U32)(4 * StreamInfo->numSubBands * 
                                          StreamInfo->numChannels) >> 3) + 
                          (temp >> 3));

        if (temp % 8) {
            frameLen++;
        }

        exit:

#if TI_CHANGES  == XA_ENABLED
        StreamInfo->lastSbcFrameLen = frameLen;
    }
#endif  
    
    return frameLen;
}


/*---------------------------------------------------------------------------
 *            SbcCrcSum4()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Bitwise CRC summing for less than 8 bits.
 */
static void SbcCrcSum4(SbcStreamInfo *StreamInfo, U8 input)
{
    I8 i;
    U8 shift, bit, FCS = StreamInfo->fcs;

    /* Just sum the most significant 4 bits */
    shift = 7;
    for (i = 0; i < 4; i++) {
        bit = (0x01 & (input >> shift--)) ^ (FCS >> 7);
        if (bit) {
            FCS = ((FCS << 1) | bit) ^ 0x1C;
        } else {
            FCS = (FCS << 1);
        }
    }                                         

    StreamInfo->fcs = FCS;
}

/*---------------------------------------------------------------------------
 *            SbcMonoBitAlloc()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Performs bit allocation for a mono stream.  For dual channel
 *            streams, one channel is processed at a time and this function is
 *            called once for each channel.
 */
static void SbcMonoBitAlloc(SbcStreamInfo *StreamInfo, U8 Ch)
{
    I8  sb;
    S8  loudness;
    S8  maxBitNeed = 0;
    U8  bitCount = 0;
    U8  sliceCount = 0;
    S8  bitSlice;
    U8 *bits;
    U8 *scale_factors;

    if (Ch == 0) {
        bits = &StreamInfo->bits[0][0];
        scale_factors = &StreamInfo->scale_factors[0][0];
    } else {
        bits = &StreamInfo->bits[1][0];
        scale_factors = &StreamInfo->scale_factors[1][0];
    }

    if (StreamInfo->allocMethod == SBC_ALLOC_METHOD_SNR) {
        /* SNR allocation method */
        for (sb = 0; sb < StreamInfo->numSubBands; sb++) {
            StreamInfo->bitNeed0[sb] = scale_factors[sb];

            /* Calculate Max Bitneed */
            if (StreamInfo->bitNeed0[sb] > maxBitNeed) {
                maxBitNeed = StreamInfo->bitNeed0[sb];
            }
        }
    } else {
        /* Loudness allocation method */
        for (sb = 0; sb < StreamInfo->numSubBands; sb++) {
            if (scale_factors[sb] == 0) {
                StreamInfo->bitNeed0[sb] = (S8)-5;
            } else {
                if (StreamInfo->numSubBands == 4) {
                    loudness = scale_factors[sb] - 
                               LoudnessOffset4[StreamInfo->sampleFreq][sb];
                } else {
                    loudness = scale_factors[sb] - 
                               LoudnessOffset8[StreamInfo->sampleFreq][sb];
                }
                if (loudness > 0) {
                    /* Divide by 2 */
                    StreamInfo->bitNeed0[sb] = loudness >> 1;
                } else {
                    StreamInfo->bitNeed0[sb] = loudness;
                }
            }

            /* Calculate Max Bitneed */
            if (StreamInfo->bitNeed0[sb] > maxBitNeed) {
                maxBitNeed = StreamInfo->bitNeed0[sb];
            }
        }
    }

    /* Calculate bitslices */
    bitSlice = maxBitNeed + 1;
    do {
        bitSlice--;
        bitCount += sliceCount;
        sliceCount = 0;
        for (sb = 0; sb < StreamInfo->numSubBands; sb++) {
            if ((StreamInfo->bitNeed0[sb] > (bitSlice + 1)) && 
                (StreamInfo->bitNeed0[sb] < (bitSlice + 16))) {
                sliceCount++;
            } else if (StreamInfo->bitNeed0[sb] == (bitSlice + 1)) {
                sliceCount += 2;
            }
        }
    } while ((bitCount + sliceCount) < StreamInfo->bitPool);

    if ((bitCount + sliceCount) == StreamInfo->bitPool) {
        bitCount += sliceCount;
        bitSlice--;
    }

    /* Distribute the bits */
    for (sb = 0; sb < StreamInfo->numSubBands; sb++) {
        if (StreamInfo->bitNeed0[sb] < (bitSlice + 2)) {
            bits[sb] = 0;
        } else {
            bits[sb] = 
                min(StreamInfo->bitNeed0[sb] - bitSlice, 16);
        }
    }

    /* Allocate remaining bits */
    sb = 0;
    while ((bitCount < StreamInfo->bitPool) && (sb < StreamInfo->numSubBands)) {
        if ((bits[sb] >= 2) && (bits[sb] < 16)) {
            bits[sb]++;
            bitCount++;
        } else if ((StreamInfo->bitNeed0[sb] == (bitSlice + 1)) && 
                   (StreamInfo->bitPool > (bitCount + 1))) {
            bits[sb] = 2;
            bitCount += 2;
        }
        sb++;
    }

    sb = 0;
    while ((bitCount < StreamInfo->bitPool) && (sb < StreamInfo->numSubBands)) {
        if (bits[sb] < 16) {
            bits[sb]++;
            bitCount++;
        }
        sb++;
    }
}

/*---------------------------------------------------------------------------
 *            SbcStereoBitAlloc()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Performs bit allocation for stereo streams.  Both channels are
 *            processed together.  
 */
static void SbcStereoBitAlloc(SbcStreamInfo *StreamInfo)
{
    int  sb, numsb = StreamInfo->numSubBands;
    S8  maxBitNeed = 0, bitNeed[2*SBC_MAX_NUM_SB];
    U8   bitPool = StreamInfo->bitPool;
    U8   sampleFreq  = StreamInfo->sampleFreq;
    S8   loudness, bitSlice;
    U8   bitCount = 0, sliceCount = 0;

    if (StreamInfo->allocMethod == SBC_ALLOC_METHOD_SNR) {
        /* SNR allocation method */

        for (sb = 0; sb < numsb; sb++)
        {
            bitNeed[sb] = StreamInfo->scale_factors[0][sb];

            /* Calculate Max Bitneed */
            if (bitNeed[sb] > maxBitNeed)   maxBitNeed = bitNeed[sb];
        }

        for (sb = 0; sb < numsb; sb++) {
            bitNeed[sb+numsb] = StreamInfo->scale_factors[1][sb];

            /* Calculate Max Bitneed */
            if (bitNeed[sb+numsb] > maxBitNeed) maxBitNeed = bitNeed[sb+numsb];
        }
    } 
    else 
    {    /* Loudness allocation method */
        for (sb = 0; sb < numsb; sb++)
        {
            if (StreamInfo->scale_factors[0][sb] == 0)  bitNeed[sb] = -5;
            else
            {
                if (numsb == 4)
                    loudness = StreamInfo->scale_factors[0][sb] - 
                               LoudnessOffset4[sampleFreq][sb];
                else
                    loudness = StreamInfo->scale_factors[0][sb] - 
                               LoudnessOffset8[sampleFreq][sb];

                if (loudness > 0)   bitNeed[sb] = loudness >> 1;
                else                bitNeed[sb] = loudness;
            }

            /* Calculate Max Bitneed */
            if (bitNeed[sb] > maxBitNeed)   maxBitNeed = bitNeed[sb];
        }

        for (sb = 0; sb < numsb; sb++) 
        {
            if (StreamInfo->scale_factors[1][sb] == 0) bitNeed[sb+numsb] = -5;
            else
            {
                if (numsb == 4) {
                    loudness = StreamInfo->scale_factors[1][sb] - 
                               LoudnessOffset4[sampleFreq][sb];
                } else {
                    loudness = StreamInfo->scale_factors[1][sb] - 
                               LoudnessOffset8[sampleFreq][sb];
                }
                if (loudness > 0)   bitNeed[sb+numsb] = loudness >> 1;
                else                bitNeed[sb+numsb] = loudness;
            }

            /* Calculate Max Bitneed */
            if(bitNeed[sb+numsb]>maxBitNeed)    maxBitNeed=bitNeed[sb+numsb];
        }
    }

    /* Calculate bitslices */
    bitSlice = maxBitNeed + 1;
    do {
        bitCount += sliceCount;
        sliceCount = 0;

        for (sb = 0; sb < 2*numsb; sb++)
            if((bitNeed[sb]>bitSlice) && 
               (bitNeed[sb]<(bitSlice+15)))     sliceCount++;
            else if(bitNeed[sb]==bitSlice)      sliceCount+= 2;

        bitSlice--;
    } while ((bitCount + sliceCount) < bitPool);

    if ((bitCount + sliceCount) == bitPool)
    {
        bitCount += sliceCount;
        bitSlice--;
    }

    /* Distribute the bits */
    for (sb = 0; sb < numsb; sb++)
    {
        if (bitNeed[sb] < (bitSlice + 2))
            StreamInfo->bits[0][sb] = 0;
        else
            StreamInfo->bits[0][sb] = min(bitNeed[sb]-bitSlice, 16);
    }

    for (sb = 0; sb < numsb; sb++)
    {
        if (bitNeed[numsb+sb] < (bitSlice + 2))
            StreamInfo->bits[1][sb] = 0;
        else
            StreamInfo->bits[1][sb] = min(bitNeed[numsb+sb]-bitSlice, 16);
    }

    /* Allocate remaining bits */
    sb = 0;
    while ((bitCount < bitPool) && (sb < numsb)) {

        if ((StreamInfo->bits[0][sb] >= 2) && (StreamInfo->bits[0][sb] < 16))
        {
            StreamInfo->bits[0][sb]++;
            bitCount++;
        } 
        else if ((bitNeed[sb] == (bitSlice + 1)) && (bitPool > (bitCount + 1))) 
        {
            StreamInfo->bits[0][sb] = 2;
            bitCount += 2;
        }

        if (!((bitCount < bitPool) && (sb < numsb)))    break;

        if ((StreamInfo->bits[1][sb] >= 2) && (StreamInfo->bits[1][sb] < 16)) 
        {
            StreamInfo->bits[1][sb]++;
            bitCount++;
        } 
        else if((bitNeed[sb+numsb] == (bitSlice + 1)) && 
                (bitPool > (bitCount + 1)))
        {
            StreamInfo->bits[1][sb] = 2;
            bitCount += 2;
        }

        sb++;
    }

    sb = 0;
    while ((bitCount < bitPool) && (sb < numsb))
    {
        if (StreamInfo->bits[0][sb] < 16)
        {
            StreamInfo->bits[0][sb]++;
            bitCount++;
        }

        if (!((bitCount < bitPool) && (sb < numsb)))    break;

        if (StreamInfo->bits[1][sb] < 16) {
            StreamInfo->bits[1][sb]++;
            bitCount++;
        }

        sb++;
    }
}

#if SBC_DECODER == XA_ENABLED
/*---------------------------------------------------------------------------
 *            SBC_InitDecoder()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initializes a Decoder structure for processing a new stream.  
 *            Must be called before each new stream.
 */
void SBC_InitDecoder(SbcDecoder *Decoder)
{
    //OS_MemSet((U8*)Decoder, 0, sizeof(SbcDecoder));
    memset((U8*)Decoder, 0, sizeof(SbcDecoder));
}

/*---------------------------------------------------------------------------
 *            SbcParseHeader()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Parses an SBC header for frame information.
 */
static U16 SbcParseHeader(SbcDecoder *Decoder, U8 *Buffer)
{
    I8  i;
    U8 *ptr = Buffer;
    U8  numBytes;

    /* Initialize FCS */
    Decoder->streamInfo.fcs = 0x0F;

    /* Checksum first byte */
    Decoder->streamInfo.fcs = SbcCrcTable[Decoder->streamInfo.fcs ^ *ptr];

    /* Sampling Frequency */
    Decoder->streamInfo.sampleFreq = *ptr >> 6;

    /* Number of blocks */
    switch ((*ptr >> 4) & 0x03) {
    case 0:
        Decoder->streamInfo.numBlocks = 4;
        break;
    case 1:
        Decoder->streamInfo.numBlocks = 8;
        break;
    case 2:
        Decoder->streamInfo.numBlocks = 12;
        break;
    case 3:
        Decoder->streamInfo.numBlocks = 16;
        break;
    default:
        return 0;
    }

    /* Channel mode and number of Decoders */
    Decoder->streamInfo.channelMode =  ((*ptr >> 2) & 0x03);
    switch (Decoder->streamInfo.channelMode) {
    case 0:
        Decoder->streamInfo.numChannels = 1;
        break;
    case 1:
    case 2:
    case 3:
        Decoder->streamInfo.numChannels = 2;
        break;
    default:
        return 0;
    }

    /* Allocation Method */
    Decoder->streamInfo.allocMethod = (*ptr >> 1) & 0x01;

    /* Subbands */
    switch (*ptr++ & 0x01) {
    case 0:
        Decoder->streamInfo.numSubBands = 4;
        break;
    case 1:
        Decoder->streamInfo.numSubBands = 8;
        break;
    default:
        return 0;
    }

    /* Checksum second byte */
    Decoder->streamInfo.fcs = SbcCrcTable[Decoder->streamInfo.fcs ^ *ptr];

    /* Bitpool */
    Decoder->streamInfo.bitPool = *ptr++;

    /* CRC */
    Decoder->streamInfo.crc = *ptr++;
    Decoder->streamInfo.bitOffset = 24;
    numBytes = 3;

    /* Join */
    if (Decoder->streamInfo.channelMode == SBC_CHNL_MODE_JOINT_STEREO) {

        for (i = 0; i < Decoder->streamInfo.numSubBands - 1; i++) {
            Decoder->streamInfo.join[i] = (*ptr >> (7 - i)) & 0x01;
            Decoder->streamInfo.bitOffset++;
        }
        Decoder->streamInfo.join[i] = 0;
        Decoder->streamInfo.bitOffset++;
        if (Decoder->streamInfo.bitOffset == 32) {
            numBytes = 4;

            /* Checksum fourth byte */
            Decoder->streamInfo.fcs = SbcCrcTable[Decoder->streamInfo.fcs ^ *ptr];

        } else if (Decoder->streamInfo.bitOffset != 28) {
            numBytes = 0;
        }
    }

    /* Return number of complete bytes processed */
    return numBytes;
}

/*---------------------------------------------------------------------------
 *            SbcParseScaleFactors()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Parses an SBC header for scale factors.
 */
static void SbcParseScaleFactors(SbcDecoder *Decoder, U8 *Buffer)
{
    I8  ch, sb;
    U8 *ptr = Buffer;

    /* Scaling Factors */
    for (ch = 0; ch < Decoder->streamInfo.numChannels; ch++) {
        for (sb = 0; sb < Decoder->streamInfo.numSubBands; sb++) {
            if (Decoder->streamInfo.bitOffset % 8) {

                /* Sum the whole byte */
                Decoder->streamInfo.fcs = 
                SbcCrcTable[Decoder->streamInfo.fcs ^ *ptr];

                Decoder->streamInfo.scale_factors[ch][sb] = *ptr++ & 0x0F;
            } else {

                if ((ch == Decoder->streamInfo.numChannels - 1) &&
                    (sb == Decoder->streamInfo.numSubBands - 1)) {

                    /* Sum the next 4 bits */
                    SbcCrcSum4(&Decoder->streamInfo, *ptr);
                }

                Decoder->streamInfo.scale_factors[ch][sb] = *ptr >> 4;
            }

            Decoder->streamInfo.scaleFactors[ch][sb] =
                ((U32)1 << (Decoder->streamInfo.scale_factors[ch][sb] + 1));

            Decoder->streamInfo.bitOffset += 4;
        }
    }
}

/*---------------------------------------------------------------------------
 *            SbcUnpackSample()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Unpacks an audio sample from the stream.  Samples are extracted
 *            from the stream based on the bits required for the current
 *            subband.
 */
static U16 INLINE SbcUnpackSample(SbcDecoder *Decoder, U8 **Buffer, U8 Ch, U8 Sb)
{
    U16 sample = 0;
    U8  *ptr;
    U8  bit;
    U8  bitsLeft = Decoder->streamInfo.bits[Ch][Sb];;

    do {
        ptr = *Buffer;
        bit = 8 - Decoder->streamInfo.bitOffset % 8;

        if (bitsLeft > bit) {
            /* The bits are split over multiple bytes */
            sample += *ptr & (0xFF >> (8 - bit));
            bitsLeft -= bit;
            Decoder->streamInfo.bitOffset += bit;
            sample = sample << min(8, bitsLeft);
            (*Buffer)++;
        } else {
            /* The bits are all in this byte */
            sample += (*ptr >> (bit - bitsLeft)) & (0xFF >> (8 - bitsLeft));
            Decoder->streamInfo.bitOffset += bitsLeft;
            if (bitsLeft == bit) {
                (*Buffer)++;
            }
            bitsLeft = 0;
        }
    } while (bitsLeft);

    return sample;
}

/*---------------------------------------------------------------------------
 *            SbcBuildSubbandSamples()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Unpacks samples from the stream.
 */
static void SbcBuildSubbandSamples(SbcDecoder *Decoder, U8 *Buffer)
{
    I8 ch, sb, blk;

    /* Calculate levels */
    for (ch = 0; ch < Decoder->streamInfo.numChannels; ch++) {
        for (sb = 0; sb < Decoder->streamInfo.numSubBands; sb++) {
            Decoder->streamInfo.levels[ch][sb] = 
                ((U16)1 << Decoder->streamInfo.bits[ch][sb]) - 1;
        }
    }

    /* Construct subband samples */
    for (blk = 0; blk < Decoder->streamInfo.numBlocks; blk++) {
        for (ch = 0; ch < Decoder->streamInfo.numChannels; ch++) {
            for (sb = 0; sb < Decoder->streamInfo.numSubBands; sb++) {
                if (Decoder->streamInfo.levels[ch][sb] > 0) {
                    Decoder->streamInfo.sbSample[blk][ch][sb] =

#if SBC_USE_FIXED_POINT == XA_ENABLED
                    Decoder->streamInfo.scaleFactors[ch][sb] * 
                    ((((((SbcUnpackSample(Decoder, &Buffer, (U8)ch, (U8)sb) << 1) + 1) << 14) / 
                       Decoder->streamInfo.levels[ch][sb]) << 1) - ONE_F);
#else
                    Decoder->streamInfo.scaleFactors[ch][sb] *
                        ((SbcUnpackSample(Decoder, &Buffer, (U8)ch, (U8)sb) * (REAL)2 + (REAL)1) / 
                         Decoder->streamInfo.levels[ch][sb] - (REAL)1);
#endif

                } else {
                    Decoder->streamInfo.sbSample[blk][ch][sb] = 0;
                }
            }
        }
    }
}

/*---------------------------------------------------------------------------
 *            SbcJointProcessing()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Performs joint processing on streams encoded as joint stereo.
 */
static void SbcJointProcessing(SbcDecoder *Decoder)
{
    I8 blk, sb;

    for (blk = 0; blk < Decoder->streamInfo.numBlocks; blk++) {
        for (sb = 0; sb < Decoder->streamInfo.numSubBands; sb++) {
            if (Decoder->streamInfo.join[sb] == 1) {
                Decoder->streamInfo.sbSample[blk][0][sb] = 
                    Decoder->streamInfo.sbSample[blk][0][sb] + 
                    Decoder->streamInfo.sbSample[blk][1][sb];
                Decoder->streamInfo.sbSample[blk][1][sb] = 
                    Decoder->streamInfo.sbSample[blk][0][sb] - 
                    2 * Decoder->streamInfo.sbSample[blk][1][sb];
            }
        }
    }

}

/*---------------------------------------------------------------------------
 *            SbcSynthesisFilter4()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Performs synthesis of the unpacked stream samples with 4
 *            subbands.  Creates the PCM output.
 */
static void SbcSynthesisFilter4(SbcDecoder *Decoder, SbcPcmData *PcmData)
{
    I8    i, blk;
    U8   *ptr;
    S16   pcm;
    REAL  X;

    /* Initialize the pointer for outputting PCM data */
    ptr = PcmData->data + PcmData->dataLen;
    
    for (blk = 0; blk < Decoder->streamInfo.numBlocks; blk++) {
        
        /* Shifting */
        for (i = 79; i > 7; i--) {
            Decoder->V0[i] = Decoder->V0[i - 8];
        }
        
        /* Matrixing */
        Decoder->V0[0] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               Decoder->streamInfo.sbSample[blk][0][3], Synth4[0][0]) + 
                         dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               Decoder->streamInfo.sbSample[blk][0][2], Synth4[0][1]);

        Decoder->V0[1] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                              -Decoder->streamInfo.sbSample[blk][0][3], Synth4[1][0]) + 
                         dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                              -Decoder->streamInfo.sbSample[blk][0][2], Synth4[1][1]);

        Decoder->V0[2] = 0;

        Decoder->V0[3] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                              -Decoder->streamInfo.sbSample[blk][0][3], Synth4[3][0]) + 
                         dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                              -Decoder->streamInfo.sbSample[blk][0][2], Synth4[3][1]);

        Decoder->V0[4] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               Decoder->streamInfo.sbSample[blk][0][3], Synth4[4][0]) + 
                         dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               Decoder->streamInfo.sbSample[blk][0][2], Synth4[4][1]);

        Decoder->V0[5] = dMulP(Decoder->streamInfo.sbSample[blk][0][0]+
                              -Decoder->streamInfo.sbSample[blk][0][3], Synth4[5][0]) + 
                         dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                              -Decoder->streamInfo.sbSample[blk][0][2], Synth4[5][1]);

        Decoder->V0[6] = -Decoder->streamInfo.sbSample[blk][0][0] +
                         -Decoder->streamInfo.sbSample[blk][0][1] +
                         -Decoder->streamInfo.sbSample[blk][0][2] +
                         -Decoder->streamInfo.sbSample[blk][0][3];

        Decoder->V0[7] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                              -Decoder->streamInfo.sbSample[blk][0][3], Synth4[7][0]) + 
                         dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                              -Decoder->streamInfo.sbSample[blk][0][2], Synth4[7][1]);

        /* Calculate 4 audio samples */

        X = dMulP(Decoder->V0[12] +  Decoder->V0[76], SbcDecodeCoeff4[4]) + 
            dMulP(Decoder->V0[16] + -Decoder->V0[64], SbcDecodeCoeff4[8]) +
            dMulP(Decoder->V0[28] +  Decoder->V0[60], SbcDecodeCoeff4[12]) +
            dMulP(Decoder->V0[32] + -Decoder->V0[48], SbcDecodeCoeff4[16]) +
            dMulP(Decoder->V0[44], SbcDecodeCoeff4[20]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[1],  SbcDecodeCoeff4[1]) + 
            dMulP(Decoder->V0[13], SbcDecodeCoeff4[5]) + 
            dMulP(Decoder->V0[17], SbcDecodeCoeff4[9]) + 
            dMulP(Decoder->V0[29], SbcDecodeCoeff4[13]) +  
            dMulP(Decoder->V0[33], SbcDecodeCoeff4[17]) + 
            dMulP(Decoder->V0[45], SbcDecodeCoeff4[21]) + 
            dMulP(Decoder->V0[49], SbcDecodeCoeff4[25]) + 
            dMulP(Decoder->V0[61], SbcDecodeCoeff4[29]) + 
            dMulP(Decoder->V0[65], SbcDecodeCoeff4[33]) + 
            dMulP(Decoder->V0[77], SbcDecodeCoeff4[37]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[2]  + Decoder->V0[78], SbcDecodeCoeff4[2]) + 
            dMulP(Decoder->V0[14] + Decoder->V0[66], SbcDecodeCoeff4[6]) + 
            dMulP(Decoder->V0[18] + Decoder->V0[62], SbcDecodeCoeff4[10]) + 
            dMulP(Decoder->V0[30] + Decoder->V0[50], SbcDecodeCoeff4[14]) +
            dMulP(Decoder->V0[34] + Decoder->V0[46], SbcDecodeCoeff4[18]); 

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[3],  SbcDecodeCoeff4[3]) + 
            dMulP(Decoder->V0[15],  SbcDecodeCoeff4[7]) + 
            dMulP(Decoder->V0[19], SbcDecodeCoeff4[11]) + 
            dMulP(Decoder->V0[31], SbcDecodeCoeff4[15]) +
            dMulP(Decoder->V0[35], SbcDecodeCoeff4[19]) + 
            dMulP(Decoder->V0[47], SbcDecodeCoeff4[23]) + 
            dMulP(Decoder->V0[51], SbcDecodeCoeff4[27]) + 
            dMulP(Decoder->V0[63], SbcDecodeCoeff4[31]) +
            dMulP(Decoder->V0[67], SbcDecodeCoeff4[35]) + 
            dMulP(Decoder->V0[79], SbcDecodeCoeff4[39]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }
    }

    if (Decoder->streamInfo.numChannels > 1) {

        /* Initialize the pointer for outputting PCM data */
        ptr = PcmData->data + PcmData->dataLen + 2;
    
        for (blk = 0; blk < Decoder->streamInfo.numBlocks; blk++) {
        
            /* Shifting */
            for (i = 79; i > 7; i--) {
                Decoder->V1[i] = Decoder->V1[i - 8];
            }
        
            /* Matrixing */
            Decoder->V1[0] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   Decoder->streamInfo.sbSample[blk][1][3], Synth4[0][0]) + 
                             dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   Decoder->streamInfo.sbSample[blk][1][2], Synth4[0][1]);

            Decoder->V1[1] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                  -Decoder->streamInfo.sbSample[blk][1][3], Synth4[1][0]) + 
                             dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                  -Decoder->streamInfo.sbSample[blk][1][2], Synth4[1][1]);

            Decoder->V1[2] = 0;

            Decoder->V1[3] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                  -Decoder->streamInfo.sbSample[blk][1][3], Synth4[3][0]) + 
                             dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                  -Decoder->streamInfo.sbSample[blk][1][2], Synth4[3][1]);

            Decoder->V1[4] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   Decoder->streamInfo.sbSample[blk][1][3], Synth4[4][0]) + 
                             dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   Decoder->streamInfo.sbSample[blk][1][2], Synth4[4][1]);

            Decoder->V1[5] = dMulP(Decoder->streamInfo.sbSample[blk][1][0]+
                                  -Decoder->streamInfo.sbSample[blk][1][3], Synth4[5][0]) + 
                             dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                  -Decoder->streamInfo.sbSample[blk][1][2], Synth4[5][1]);

            Decoder->V1[6] = -Decoder->streamInfo.sbSample[blk][1][0] +
                             -Decoder->streamInfo.sbSample[blk][1][1] +
                             -Decoder->streamInfo.sbSample[blk][1][2] +
                             -Decoder->streamInfo.sbSample[blk][1][3];

            Decoder->V1[7] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                  -Decoder->streamInfo.sbSample[blk][1][3], Synth4[7][0]) + 
                             dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                  -Decoder->streamInfo.sbSample[blk][1][2], Synth4[7][1]);
 
           /* Calculate 4 audio samples */

            X = dMulP(Decoder->V1[12] +  Decoder->V1[76], SbcDecodeCoeff4[4]) + 
                dMulP(Decoder->V1[16] + -Decoder->V1[64], SbcDecodeCoeff4[8]) +
                dMulP(Decoder->V1[28] +  Decoder->V1[60], SbcDecodeCoeff4[12]) +
                dMulP(Decoder->V1[32] + -Decoder->V1[48], SbcDecodeCoeff4[16]) +
                dMulP(Decoder->V1[44], SbcDecodeCoeff4[20]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[1],  SbcDecodeCoeff4[1]) + 
                dMulP(Decoder->V1[13], SbcDecodeCoeff4[5]) + 
                dMulP(Decoder->V1[17], SbcDecodeCoeff4[9]) + 
                dMulP(Decoder->V1[29], SbcDecodeCoeff4[13]) +  
                dMulP(Decoder->V1[33], SbcDecodeCoeff4[17]) + 
                dMulP(Decoder->V1[45], SbcDecodeCoeff4[21]) + 
                dMulP(Decoder->V1[49], SbcDecodeCoeff4[25]) + 
                dMulP(Decoder->V1[61], SbcDecodeCoeff4[29]) + 
                dMulP(Decoder->V1[65], SbcDecodeCoeff4[33]) + 
                dMulP(Decoder->V1[77], SbcDecodeCoeff4[37]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[2]  + Decoder->V1[78], SbcDecodeCoeff4[2]) + 
                dMulP(Decoder->V1[14] + Decoder->V1[66], SbcDecodeCoeff4[6]) + 
                dMulP(Decoder->V1[18] + Decoder->V1[62], SbcDecodeCoeff4[10]) + 
                dMulP(Decoder->V1[30] + Decoder->V1[50], SbcDecodeCoeff4[14]) +
                dMulP(Decoder->V1[34] + Decoder->V1[46], SbcDecodeCoeff4[18]); 

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[3],  SbcDecodeCoeff4[3]) + 
                dMulP(Decoder->V1[15], SbcDecodeCoeff4[7]) + 
                dMulP(Decoder->V1[19], SbcDecodeCoeff4[11]) + 
                dMulP(Decoder->V1[31], SbcDecodeCoeff4[15]) +
                dMulP(Decoder->V1[35], SbcDecodeCoeff4[19]) + 
                dMulP(Decoder->V1[47], SbcDecodeCoeff4[23]) + 
                dMulP(Decoder->V1[51], SbcDecodeCoeff4[27]) + 
                dMulP(Decoder->V1[63], SbcDecodeCoeff4[31]) +
                dMulP(Decoder->V1[67], SbcDecodeCoeff4[35]) + 
                dMulP(Decoder->V1[79], SbcDecodeCoeff4[39]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }
        }

        /* Update the length of the PCM data */
        PcmData->dataLen = ptr - PcmData->data - 2;
    } else {

        /* Update the length of the PCM data */
        PcmData->dataLen = ptr - PcmData->data;
    }
}

/*---------------------------------------------------------------------------
 *            SbcSynthesisFilter8()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Performs synthesis of the unpacked stream samples with 8
 *            subbands.  Creates the PCM output.
 */
static void SbcSynthesisFilter8(SbcDecoder *Decoder, SbcPcmData *PcmData)
{
    I8   i, blk;
    U8  *ptr;
    S16  pcm;
    REAL X;

    /* Initialize the pointer for outputting PCM data */
    ptr = PcmData->data + PcmData->dataLen;
    
    /* Perform synthesis on the current channel */
    for (blk = 0; blk < Decoder->streamInfo.numBlocks; blk++) {
        
        /* Shifting */
        for (i = 159; i > 15; i--) {
            Decoder->V0[i] = Decoder->V0[i - 16];
        }
        
        /* Matrixing */
        Decoder->V0[0] =  dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                                Decoder->streamInfo.sbSample[blk][0][7], Synth8[0][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                                Decoder->streamInfo.sbSample[blk][0][6], Synth8[0][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                                Decoder->streamInfo.sbSample[blk][0][5], Synth8[0][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                                Decoder->streamInfo.sbSample[blk][0][4], Synth8[0][3]);

        Decoder->V0[1] =  dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               -Decoder->streamInfo.sbSample[blk][0][7], Synth8[1][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               -Decoder->streamInfo.sbSample[blk][0][6], Synth8[1][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                               -Decoder->streamInfo.sbSample[blk][0][5], Synth8[1][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                               -Decoder->streamInfo.sbSample[blk][0][4], Synth8[1][3]);

        Decoder->V0[2] =  dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                                Decoder->streamInfo.sbSample[blk][0][7], Synth8[2][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                                Decoder->streamInfo.sbSample[blk][0][6], Synth8[2][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                                Decoder->streamInfo.sbSample[blk][0][5], Synth8[2][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                                Decoder->streamInfo.sbSample[blk][0][4], Synth8[2][3]);

        Decoder->V0[3] =  dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               -Decoder->streamInfo.sbSample[blk][0][7], Synth8[3][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               -Decoder->streamInfo.sbSample[blk][0][6], Synth8[3][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                               -Decoder->streamInfo.sbSample[blk][0][5], Synth8[3][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                               -Decoder->streamInfo.sbSample[blk][0][4], Synth8[3][3]);

        Decoder->V0[4] =  0;

        Decoder->V0[5] =  dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               -Decoder->streamInfo.sbSample[blk][0][7], Synth8[5][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               -Decoder->streamInfo.sbSample[blk][0][6], Synth8[5][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                               -Decoder->streamInfo.sbSample[blk][0][5], Synth8[5][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                               -Decoder->streamInfo.sbSample[blk][0][4], Synth8[5][3]);

        Decoder->V0[6] =  dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                                Decoder->streamInfo.sbSample[blk][0][7], Synth8[6][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                                Decoder->streamInfo.sbSample[blk][0][6], Synth8[6][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                                Decoder->streamInfo.sbSample[blk][0][5], Synth8[6][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                                Decoder->streamInfo.sbSample[blk][0][4], Synth8[6][3]);

        Decoder->V0[7] =  dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               -Decoder->streamInfo.sbSample[blk][0][7], Synth8[7][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               -Decoder->streamInfo.sbSample[blk][0][6], Synth8[7][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                               -Decoder->streamInfo.sbSample[blk][0][5], Synth8[7][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                               -Decoder->streamInfo.sbSample[blk][0][4], Synth8[7][3]);

        Decoder->V0[8] =  dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                                Decoder->streamInfo.sbSample[blk][0][4], Synth8[8][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                                Decoder->streamInfo.sbSample[blk][0][5], Synth8[8][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                                Decoder->streamInfo.sbSample[blk][0][6], Synth8[8][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                                Decoder->streamInfo.sbSample[blk][0][7], Synth8[8][3]);

        Decoder->V0[9] =  dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               -Decoder->streamInfo.sbSample[blk][0][7], Synth8[9][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               -Decoder->streamInfo.sbSample[blk][0][6], Synth8[9][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                               -Decoder->streamInfo.sbSample[blk][0][5], Synth8[9][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                               -Decoder->streamInfo.sbSample[blk][0][4], Synth8[9][3]);

        Decoder->V0[10] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                                Decoder->streamInfo.sbSample[blk][0][7], Synth8[10][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                                Decoder->streamInfo.sbSample[blk][0][6], Synth8[10][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                                Decoder->streamInfo.sbSample[blk][0][5], Synth8[10][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                                Decoder->streamInfo.sbSample[blk][0][4], Synth8[10][3]);

        Decoder->V0[11] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               -Decoder->streamInfo.sbSample[blk][0][7], Synth8[11][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               -Decoder->streamInfo.sbSample[blk][0][6], Synth8[11][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                               -Decoder->streamInfo.sbSample[blk][0][5], Synth8[11][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3]+
                               -Decoder->streamInfo.sbSample[blk][0][4], Synth8[11][3]);

        Decoder->V0[12] = -Decoder->streamInfo.sbSample[blk][0][0] + 
                          -Decoder->streamInfo.sbSample[blk][0][1] +
                          -Decoder->streamInfo.sbSample[blk][0][2] +
                          -Decoder->streamInfo.sbSample[blk][0][3] +
                          -Decoder->streamInfo.sbSample[blk][0][4] +
                          -Decoder->streamInfo.sbSample[blk][0][5] +
                          -Decoder->streamInfo.sbSample[blk][0][6] +
                          -Decoder->streamInfo.sbSample[blk][0][7];

        Decoder->V0[13] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               -Decoder->streamInfo.sbSample[blk][0][7], Synth8[13][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               -Decoder->streamInfo.sbSample[blk][0][6], Synth8[13][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                               -Decoder->streamInfo.sbSample[blk][0][5], Synth8[13][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                               -Decoder->streamInfo.sbSample[blk][0][4], Synth8[13][3]);

        Decoder->V0[14] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                                Decoder->streamInfo.sbSample[blk][0][7], Synth8[14][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                                Decoder->streamInfo.sbSample[blk][0][6], Synth8[14][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                                Decoder->streamInfo.sbSample[blk][0][5], Synth8[14][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                                Decoder->streamInfo.sbSample[blk][0][4], Synth8[14][3]);

        Decoder->V0[15] = dMulP(Decoder->streamInfo.sbSample[blk][0][0] +
                               -Decoder->streamInfo.sbSample[blk][0][7], Synth8[15][0]) + 
                          dMulP(Decoder->streamInfo.sbSample[blk][0][1] +
                               -Decoder->streamInfo.sbSample[blk][0][6], Synth8[15][1]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][2] +
                               -Decoder->streamInfo.sbSample[blk][0][5], Synth8[15][2]) +
                          dMulP(Decoder->streamInfo.sbSample[blk][0][3] +
                               -Decoder->streamInfo.sbSample[blk][0][4], Synth8[15][3]);

        /* Calculate 8 audio samples */

        X = dMulP(Decoder->V0[24] +  Decoder->V0[152], SbcDecodeCoeff8[8])  + 
            dMulP(Decoder->V0[32] + -Decoder->V0[128], SbcDecodeCoeff8[16]) + 
            dMulP(Decoder->V0[56] +  Decoder->V0[120], SbcDecodeCoeff8[24]) +
            dMulP(Decoder->V0[64] + -Decoder->V0[96],  SbcDecodeCoeff8[32]) + 
            dMulP(Decoder->V0[88], SbcDecodeCoeff8[40]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[1],   SbcDecodeCoeff8[1])  + 
            dMulP(Decoder->V0[25],  SbcDecodeCoeff8[9])  + 
            dMulP(Decoder->V0[33],  SbcDecodeCoeff8[17]) + 
            dMulP(Decoder->V0[57],  SbcDecodeCoeff8[25]) +
            dMulP(Decoder->V0[65],  SbcDecodeCoeff8[33]) + 
            dMulP(Decoder->V0[89],  SbcDecodeCoeff8[41]) + 
            dMulP(Decoder->V0[97],  SbcDecodeCoeff8[49]) + 
            dMulP(Decoder->V0[121], SbcDecodeCoeff8[57]) +
            dMulP(Decoder->V0[129], SbcDecodeCoeff8[65]) + 
            dMulP(Decoder->V0[153], SbcDecodeCoeff8[73]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[2],   SbcDecodeCoeff8[2])  + 
            dMulP(Decoder->V0[26],  SbcDecodeCoeff8[10]) + 
            dMulP(Decoder->V0[34],  SbcDecodeCoeff8[18]) + 
            dMulP(Decoder->V0[58],  SbcDecodeCoeff8[26]) +
            dMulP(Decoder->V0[66],  SbcDecodeCoeff8[34]) + 
            dMulP(Decoder->V0[90],  SbcDecodeCoeff8[42]) + 
            dMulP(Decoder->V0[98],  SbcDecodeCoeff8[50]) + 
            dMulP(Decoder->V0[122], SbcDecodeCoeff8[58]) +
            dMulP(Decoder->V0[130], SbcDecodeCoeff8[66]) + 
            dMulP(Decoder->V0[154], SbcDecodeCoeff8[74]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[3],   SbcDecodeCoeff8[3])  + 
            dMulP(Decoder->V0[27],  SbcDecodeCoeff8[11]) + 
            dMulP(Decoder->V0[35],  SbcDecodeCoeff8[19]) + 
            dMulP(Decoder->V0[59],  SbcDecodeCoeff8[27]) +
            dMulP(Decoder->V0[67],  SbcDecodeCoeff8[35]) + 
            dMulP(Decoder->V0[91],  SbcDecodeCoeff8[43]) + 
            dMulP(Decoder->V0[99],  SbcDecodeCoeff8[51]) + 
            dMulP(Decoder->V0[123], SbcDecodeCoeff8[59]) +
            dMulP(Decoder->V0[131], SbcDecodeCoeff8[67]) + 
            dMulP(Decoder->V0[155], SbcDecodeCoeff8[75]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[4]  + Decoder->V0[156], SbcDecodeCoeff8[4])  + 
            dMulP(Decoder->V0[28] + Decoder->V0[132], SbcDecodeCoeff8[12]) + 
            dMulP(Decoder->V0[36] + Decoder->V0[124], SbcDecodeCoeff8[20]) +
            dMulP(Decoder->V0[60] + Decoder->V0[100], SbcDecodeCoeff8[28]) +
            dMulP(Decoder->V0[68] + Decoder->V0[92], SbcDecodeCoeff8[36]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[5],   SbcDecodeCoeff8[5])  + 
            dMulP(Decoder->V0[29],  SbcDecodeCoeff8[13]) + 
            dMulP(Decoder->V0[37],  SbcDecodeCoeff8[21]) + 
            dMulP(Decoder->V0[61],  SbcDecodeCoeff8[29]) +
            dMulP(Decoder->V0[69],  SbcDecodeCoeff8[37]) + 
            dMulP(Decoder->V0[93],  SbcDecodeCoeff8[45]) + 
            dMulP(Decoder->V0[101], SbcDecodeCoeff8[53]) + 
            dMulP(Decoder->V0[125], SbcDecodeCoeff8[61]) + 
            dMulP(Decoder->V0[133], SbcDecodeCoeff8[69]) + 
            dMulP(Decoder->V0[157], SbcDecodeCoeff8[77]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[6],   SbcDecodeCoeff8[6])  + 
            dMulP(Decoder->V0[30],  SbcDecodeCoeff8[14]) + 
            dMulP(Decoder->V0[38],  SbcDecodeCoeff8[22]) + 
            dMulP(Decoder->V0[62],  SbcDecodeCoeff8[30]) +
            dMulP(Decoder->V0[70],  SbcDecodeCoeff8[38]) + 
            dMulP(Decoder->V0[94],  SbcDecodeCoeff8[46]) + 
            dMulP(Decoder->V0[102], SbcDecodeCoeff8[54]) + 
            dMulP(Decoder->V0[126], SbcDecodeCoeff8[62]) +
            dMulP(Decoder->V0[134], SbcDecodeCoeff8[70]) + 
            dMulP(Decoder->V0[158], SbcDecodeCoeff8[78]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }

        X = dMulP(Decoder->V0[7],  SbcDecodeCoeff8[7])  + 
            dMulP(Decoder->V0[31], SbcDecodeCoeff8[15]) + 
            dMulP(Decoder->V0[39], SbcDecodeCoeff8[23]) + 
            dMulP(Decoder->V0[63], SbcDecodeCoeff8[31]) +
            dMulP(Decoder->V0[71], SbcDecodeCoeff8[39]) + 
            dMulP(Decoder->V0[95], SbcDecodeCoeff8[47]) + 
            dMulP(Decoder->V0[103], SbcDecodeCoeff8[55]) + 
            dMulP(Decoder->V0[127], SbcDecodeCoeff8[63]) +
            dMulP(Decoder->V0[135], SbcDecodeCoeff8[71]) + 
            dMulP(Decoder->V0[159], SbcDecodeCoeff8[79]);

        pcm = RealtoS16(X);
        StorePCM16(ptr, (S16)pcm);

        if (Decoder->streamInfo.numChannels == 1) {
            ptr += 2;
        } else {
            ptr += 4;
        }
    }

    if (Decoder->streamInfo.numChannels > 1) {

        /* Initialize the pointer for outputting PCM data */
        ptr = PcmData->data + PcmData->dataLen + 2;
        
        /* Perform synthesis on the current channel */
        for (blk = 0; blk < Decoder->streamInfo.numBlocks; blk++) {
            
            /* Shifting */
            for (i = 159; i > 15; i--) {
                Decoder->V1[i] = Decoder->V1[i - 16];
            }
            
            Decoder->V1[0] =  dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                    Decoder->streamInfo.sbSample[blk][1][7], Synth8[0][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                    Decoder->streamInfo.sbSample[blk][1][6], Synth8[0][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                    Decoder->streamInfo.sbSample[blk][1][5], Synth8[0][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                    Decoder->streamInfo.sbSample[blk][1][4], Synth8[0][3]);

            Decoder->V1[1] =  dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   -Decoder->streamInfo.sbSample[blk][1][7], Synth8[1][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   -Decoder->streamInfo.sbSample[blk][1][6], Synth8[1][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                   -Decoder->streamInfo.sbSample[blk][1][5], Synth8[1][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                   -Decoder->streamInfo.sbSample[blk][1][4], Synth8[1][3]);
                
            Decoder->V1[2] =  dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                    Decoder->streamInfo.sbSample[blk][1][7], Synth8[2][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                    Decoder->streamInfo.sbSample[blk][1][6], Synth8[2][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                    Decoder->streamInfo.sbSample[blk][1][5], Synth8[2][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                    Decoder->streamInfo.sbSample[blk][1][4], Synth8[2][3]);
                
            Decoder->V1[3] =  dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   -Decoder->streamInfo.sbSample[blk][1][7], Synth8[3][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   -Decoder->streamInfo.sbSample[blk][1][6], Synth8[3][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                   -Decoder->streamInfo.sbSample[blk][1][5], Synth8[3][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                   -Decoder->streamInfo.sbSample[blk][1][4], Synth8[3][3]);
                
            Decoder->V1[4] =  0;
                
            Decoder->V1[5] =  dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   -Decoder->streamInfo.sbSample[blk][1][7], Synth8[5][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   -Decoder->streamInfo.sbSample[blk][1][6], Synth8[5][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                   -Decoder->streamInfo.sbSample[blk][1][5], Synth8[5][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                   -Decoder->streamInfo.sbSample[blk][1][4], Synth8[5][3]);
                
            Decoder->V1[6] =  dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                    Decoder->streamInfo.sbSample[blk][1][7], Synth8[6][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                    Decoder->streamInfo.sbSample[blk][1][6], Synth8[6][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                    Decoder->streamInfo.sbSample[blk][1][5], Synth8[6][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                    Decoder->streamInfo.sbSample[blk][1][4], Synth8[6][3]);
                
            Decoder->V1[7] =  dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   -Decoder->streamInfo.sbSample[blk][1][7], Synth8[7][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   -Decoder->streamInfo.sbSample[blk][1][6], Synth8[7][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                   -Decoder->streamInfo.sbSample[blk][1][5], Synth8[7][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                   -Decoder->streamInfo.sbSample[blk][1][4], Synth8[7][3]);
                
            Decoder->V1[8] =  dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                    Decoder->streamInfo.sbSample[blk][1][4], Synth8[8][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                    Decoder->streamInfo.sbSample[blk][1][5], Synth8[8][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                    Decoder->streamInfo.sbSample[blk][1][6], Synth8[8][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                    Decoder->streamInfo.sbSample[blk][1][7], Synth8[8][3]);

            Decoder->V1[9] =  dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   -Decoder->streamInfo.sbSample[blk][1][7], Synth8[9][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   -Decoder->streamInfo.sbSample[blk][1][6], Synth8[9][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                   -Decoder->streamInfo.sbSample[blk][1][5], Synth8[9][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                   -Decoder->streamInfo.sbSample[blk][1][4], Synth8[9][3]);

            Decoder->V1[10] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                    Decoder->streamInfo.sbSample[blk][1][7], Synth8[10][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                    Decoder->streamInfo.sbSample[blk][1][6], Synth8[10][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                    Decoder->streamInfo.sbSample[blk][1][5], Synth8[10][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                    Decoder->streamInfo.sbSample[blk][1][4], Synth8[10][3]);

            Decoder->V1[11] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   -Decoder->streamInfo.sbSample[blk][1][7], Synth8[11][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   -Decoder->streamInfo.sbSample[blk][1][6], Synth8[11][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                   -Decoder->streamInfo.sbSample[blk][1][5], Synth8[11][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3]+
                                   -Decoder->streamInfo.sbSample[blk][1][4], Synth8[11][3]);

            Decoder->V1[12] = -Decoder->streamInfo.sbSample[blk][1][0] + 
                              -Decoder->streamInfo.sbSample[blk][1][1] +
                              -Decoder->streamInfo.sbSample[blk][1][2] +
                              -Decoder->streamInfo.sbSample[blk][1][3] +
                              -Decoder->streamInfo.sbSample[blk][1][4] +
                              -Decoder->streamInfo.sbSample[blk][1][5] +
                              -Decoder->streamInfo.sbSample[blk][1][6] +
                              -Decoder->streamInfo.sbSample[blk][1][7];

            Decoder->V1[13] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   -Decoder->streamInfo.sbSample[blk][1][7], Synth8[13][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   -Decoder->streamInfo.sbSample[blk][1][6], Synth8[13][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                   -Decoder->streamInfo.sbSample[blk][1][5], Synth8[13][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                   -Decoder->streamInfo.sbSample[blk][1][4], Synth8[13][3]);

            Decoder->V1[14] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                    Decoder->streamInfo.sbSample[blk][1][7], Synth8[14][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                    Decoder->streamInfo.sbSample[blk][1][6], Synth8[14][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                    Decoder->streamInfo.sbSample[blk][1][5], Synth8[14][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                    Decoder->streamInfo.sbSample[blk][1][4], Synth8[14][3]);

            Decoder->V1[15] = dMulP(Decoder->streamInfo.sbSample[blk][1][0] +
                                   -Decoder->streamInfo.sbSample[blk][1][7], Synth8[15][0]) + 
                              dMulP(Decoder->streamInfo.sbSample[blk][1][1] +
                                   -Decoder->streamInfo.sbSample[blk][1][6], Synth8[15][1]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][2] +
                                   -Decoder->streamInfo.sbSample[blk][1][5], Synth8[15][2]) +
                              dMulP(Decoder->streamInfo.sbSample[blk][1][3] +
                                   -Decoder->streamInfo.sbSample[blk][1][4], Synth8[15][3]);

            /* Calculate 8 audio samples */

            X = dMulP(Decoder->V1[24] +  Decoder->V1[152], SbcDecodeCoeff8[8])  + 
                dMulP(Decoder->V1[32] + -Decoder->V1[128], SbcDecodeCoeff8[16]) + 
                dMulP(Decoder->V1[56] +  Decoder->V1[120], SbcDecodeCoeff8[24]) +
                dMulP(Decoder->V1[64] + -Decoder->V1[96],  SbcDecodeCoeff8[32]) + 
                dMulP(Decoder->V1[88], SbcDecodeCoeff8[40]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[1],   SbcDecodeCoeff8[1])  + 
                dMulP(Decoder->V1[25],  SbcDecodeCoeff8[9])  + 
                dMulP(Decoder->V1[33],  SbcDecodeCoeff8[17]) + 
                dMulP(Decoder->V1[57],  SbcDecodeCoeff8[25]) +
                dMulP(Decoder->V1[65],  SbcDecodeCoeff8[33]) + 
                dMulP(Decoder->V1[89],  SbcDecodeCoeff8[41]) + 
                dMulP(Decoder->V1[97],  SbcDecodeCoeff8[49]) + 
                dMulP(Decoder->V1[121], SbcDecodeCoeff8[57]) +
                dMulP(Decoder->V1[129], SbcDecodeCoeff8[65]) + 
                dMulP(Decoder->V1[153], SbcDecodeCoeff8[73]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[2],   SbcDecodeCoeff8[2])  + 
                dMulP(Decoder->V1[26],  SbcDecodeCoeff8[10]) + 
                dMulP(Decoder->V1[34],  SbcDecodeCoeff8[18]) + 
                dMulP(Decoder->V1[58],  SbcDecodeCoeff8[26]) +
                dMulP(Decoder->V1[66],  SbcDecodeCoeff8[34]) + 
                dMulP(Decoder->V1[90],  SbcDecodeCoeff8[42]) + 
                dMulP(Decoder->V1[98],  SbcDecodeCoeff8[50]) + 
                dMulP(Decoder->V1[122], SbcDecodeCoeff8[58]) +
                dMulP(Decoder->V1[130], SbcDecodeCoeff8[66]) + 
                dMulP(Decoder->V1[154], SbcDecodeCoeff8[74]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[3],   SbcDecodeCoeff8[3])  + 
                dMulP(Decoder->V1[27],  SbcDecodeCoeff8[11]) + 
                dMulP(Decoder->V1[35],  SbcDecodeCoeff8[19]) + 
                dMulP(Decoder->V1[59],  SbcDecodeCoeff8[27]) +
                dMulP(Decoder->V1[67],  SbcDecodeCoeff8[35]) + 
                dMulP(Decoder->V1[91],  SbcDecodeCoeff8[43]) + 
                dMulP(Decoder->V1[99],  SbcDecodeCoeff8[51]) + 
                dMulP(Decoder->V1[123], SbcDecodeCoeff8[59]) +
                dMulP(Decoder->V1[131], SbcDecodeCoeff8[67]) + 
                dMulP(Decoder->V1[155], SbcDecodeCoeff8[75]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[4]  + Decoder->V1[156], SbcDecodeCoeff8[4])  + 
                dMulP(Decoder->V1[28] + Decoder->V1[132], SbcDecodeCoeff8[12]) + 
                dMulP(Decoder->V1[36] + Decoder->V1[124], SbcDecodeCoeff8[20]) +
                dMulP(Decoder->V1[60] + Decoder->V1[100], SbcDecodeCoeff8[28]) +
                dMulP(Decoder->V1[68] + Decoder->V1[92], SbcDecodeCoeff8[36]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[5],   SbcDecodeCoeff8[5])  + 
                dMulP(Decoder->V1[29],  SbcDecodeCoeff8[13]) + 
                dMulP(Decoder->V1[37],  SbcDecodeCoeff8[21]) + 
                dMulP(Decoder->V1[61],  SbcDecodeCoeff8[29]) +
                dMulP(Decoder->V1[69],  SbcDecodeCoeff8[37]) + 
                dMulP(Decoder->V1[93],  SbcDecodeCoeff8[45]) + 
                dMulP(Decoder->V1[101], SbcDecodeCoeff8[53]) + 
                dMulP(Decoder->V1[125], SbcDecodeCoeff8[61]) + 
                dMulP(Decoder->V1[133], SbcDecodeCoeff8[69]) + 
                dMulP(Decoder->V1[157], SbcDecodeCoeff8[77]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[6],   SbcDecodeCoeff8[6])  + 
                dMulP(Decoder->V1[30],  SbcDecodeCoeff8[14]) + 
                dMulP(Decoder->V1[38],  SbcDecodeCoeff8[22]) + 
                dMulP(Decoder->V1[62],  SbcDecodeCoeff8[30]) +
                dMulP(Decoder->V1[70],  SbcDecodeCoeff8[38]) + 
                dMulP(Decoder->V1[94],  SbcDecodeCoeff8[46]) + 
                dMulP(Decoder->V1[102], SbcDecodeCoeff8[54]) + 
                dMulP(Decoder->V1[126], SbcDecodeCoeff8[62]) +
                dMulP(Decoder->V1[134], SbcDecodeCoeff8[70]) + 
                dMulP(Decoder->V1[158], SbcDecodeCoeff8[78]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }

            X = dMulP(Decoder->V1[7],  SbcDecodeCoeff8[7])  + 
                dMulP(Decoder->V1[31], SbcDecodeCoeff8[15]) + 
                dMulP(Decoder->V1[39], SbcDecodeCoeff8[23]) + 
                dMulP(Decoder->V1[63], SbcDecodeCoeff8[31]) +
                dMulP(Decoder->V1[71], SbcDecodeCoeff8[39]) + 
                dMulP(Decoder->V1[95], SbcDecodeCoeff8[47]) + 
                dMulP(Decoder->V1[103], SbcDecodeCoeff8[55]) + 
                dMulP(Decoder->V1[127], SbcDecodeCoeff8[63]) +
                dMulP(Decoder->V1[135], SbcDecodeCoeff8[71]) + 
                dMulP(Decoder->V1[159], SbcDecodeCoeff8[79]);

            pcm = RealtoS16(X);
            StorePCM16(ptr, (S16)pcm);

            if (Decoder->streamInfo.numChannels == 1) {
                ptr += 2;
            } else {
                ptr += 4;
            }
        }

        /* Update the length of the PCM data */
        PcmData->dataLen = ptr - PcmData->data - 2;

    } else {

        /* Update the length of the PCM data */
        PcmData->dataLen = ptr - PcmData->data;
    }
}

/*---------------------------------------------------------------------------
 *            SbcResetDecoderState()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Resets the parsing state for the Decoder.
 */
static void SbcResetDecoderState(SbcDecoder *Decoder)
{
    Decoder->streamInfo.bitOffset = 0;
    Decoder->parser.rxState = SBC_PARSE_SYNC;
    Decoder->parser.stageLen = 0;
    Decoder->parser.curStageOff = 0;
}

/*---------------------------------------------------------------------------
 *            SbcMuteDecoder()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Mutes the current frame based on the length of PCM data in the
 *            previous frame.  This is used when an error occurs during the
 *            decode.
 */
static void SbcMuteFrame(SbcDecoder *Decoder, SbcPcmData *PcmData)
{
    //OS_MemSet(PcmData->data, 0, Decoder->maxPcmLen);
    PcmData->dataLen = Decoder->maxPcmLen;
    SbcResetDecoderState(Decoder);
}

/*---------------------------------------------------------------------------
 *            SBC_DecodeFrames()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Decodes SBC frames from the data stream.
 */
XaStatus SBC_DecodeFrames(SbcDecoder *Decoder, U8 *Buff, U16 Len, 
                          U16 *BytesDecoded, SbcPcmData *PcmData, 
                          U16 MaxPcmData)
{
    U16      n;
    U16      i;
    U16      frameBits;
    XaStatus status = XA_STATUS_CONTINUE;

    /* Store the receive buffer and length */
    *BytesDecoded = Len;
    Decoder->parser.rxBuff = Buff;

    /* Process the data in Server->parser.rxBuff of length Len */
    while (Len > 0) {
        //Assert(Decoder->parser.stageLen <= SBC_MAX_PCM_DATA);
        //Assert((Decoder->parser.curStageOff == 0) ||
        //       (Decoder->parser.curStageOff < Decoder->parser.stageLen));

        n = min((U16)(Decoder->parser.stageLen - Decoder->parser.curStageOff),
                Len);
        //Assert(n <= SBC_MAX_PCM_DATA);

        /* Stage the data */
        for (i = Decoder->parser.curStageOff; n > 0; n--, i++) {
            Decoder->parser.stageBuff[i] = *(Decoder->parser.rxBuff);
            (Decoder->parser.rxBuff)++;
            Len--;
            Decoder->parser.curStageOff++;
        }

        /* Only call the state machine if the data has been completely
         * staged.
         */
        if (Decoder->parser.curStageOff == Decoder->parser.stageLen) {

            /* Execute the correct state */
            switch (Decoder->parser.rxState) {
            case SBC_PARSE_SYNC: /* 0 bytes staged */
                /* Sync with the beginning of the frame */
                do {
                    Len--;
                    if (*Decoder->parser.rxBuff == SBC_SYNC_WORD) {
                        /* Found the sync word, advance state */
                        Decoder->parser.rxState = SBC_PARSE_HEADER;
                        Decoder->parser.stageLen = 4;
                        Decoder->parser.rxBuff++;
                        break;
                    }
                    Decoder->parser.rxBuff++;
                } while (Len > 0);
                break;
            case SBC_PARSE_HEADER: /* 4 bytes staged */
                switch (SbcParseHeader(Decoder, Decoder->parser.stageBuff)) {
                case 3:
                    /* Processed 3 of the 4 complete bytes */
                    (Decoder->parser.rxBuff)--;
                    Len++;
                    /* Drop through */
                case 4:
                    /* Advance the state */
                    Decoder->parser.rxState = SBC_PARSE_SCALE_FACTORS;
                    Decoder->parser.stageLen = Decoder->streamInfo.numChannels * 
                                            (Decoder->streamInfo.numSubBands >> 1);
                    if (Decoder->streamInfo.bitOffset % 8) {
                        /* Processed a partial byte, need to stage 1 more byte */
                        Decoder->parser.stageLen++;
                    }

                    Decoder->maxPcmLen = Decoder->streamInfo.numChannels * 
                                         Decoder->streamInfo.numBlocks * 
                                         Decoder->streamInfo.numSubBands * 2;

                    if (PcmData == 0) {
                        /* Just getting header info */
                        Len = *BytesDecoded;
                        status = XA_STATUS_SUCCESS;
                        SbcResetDecoderState(Decoder);
                        goto exit;
                    }

                    PcmData->numChannels = Decoder->streamInfo.numChannels;
                    PcmData->sampleFreq = Decoder->streamInfo.sampleFreq;
                    break;
                default:
                    /* Couldn't process the header, skip this Decoder */
                    status = XA_STATUS_FAILED;
                    //Report(("SBC: Invalid header"));
                    SbcMuteFrame(Decoder, PcmData);
                    goto exit;
                }
                break;
            case SBC_PARSE_SCALE_FACTORS:
                /* Get the scale factors */
                SbcParseScaleFactors(Decoder, Decoder->parser.stageBuff);

                /* Compare the calculated crc against the one in the bitstream */
                if (Decoder->streamInfo.fcs != Decoder->streamInfo.crc) {
                    status = XA_STATUS_FAILED;
                    //Report(("SBC: Crc error!"));
                    SbcMuteFrame(Decoder, PcmData);
                    goto exit;
                }

                /* Calculate Bit Allocation */
                switch (Decoder->streamInfo.channelMode) {
                case SBC_CHNL_MODE_MONO:
                    SbcMonoBitAlloc(&Decoder->streamInfo, 0);
                    break;
                case SBC_CHNL_MODE_DUAL_CHNL:
                    SbcMonoBitAlloc(&Decoder->streamInfo, 0);
                    SbcMonoBitAlloc(&Decoder->streamInfo, 1);
                    break;
                case SBC_CHNL_MODE_STEREO:
                case SBC_CHNL_MODE_JOINT_STEREO:
                    SbcStereoBitAlloc(&Decoder->streamInfo);
                    break;
                default:
                    /* Couldn't process the scale factors, skip this frame */
                    status = XA_STATUS_FAILED;
                    //Report(("SBC: Invalid channel mode\n"));
                    SbcMuteFrame(Decoder, PcmData);
                    goto exit;
                }

                /* Advance the state */
                Decoder->parser.rxState = SBC_PARSE_SAMPLES;

                /* Calculate the number of bits left in the frame */
                if ((Decoder->streamInfo.channelMode == SBC_CHNL_MODE_MONO) ||
                    (Decoder->streamInfo.channelMode == SBC_CHNL_MODE_DUAL_CHNL)) {
                    frameBits = Decoder->streamInfo.numBlocks * 
                                Decoder->streamInfo.numChannels * 
                                Decoder->streamInfo.bitPool;
                } else {
                    frameBits = Decoder->streamInfo.numBlocks * 
                                Decoder->streamInfo.bitPool;
                }

                /* Set the stage length */
                Decoder->parser.stageLen = frameBits >> 3;
                if (frameBits % 8) {
                    /* Need one more byte to get all the bits */
                    Decoder->parser.stageLen++;
                }

                /* Adjust pointer if starting in the middle of a byte */
                if (Decoder->streamInfo.bitOffset % 8) {
                    /* Processed a partial byte, need to back up 1 */
                    (Decoder->parser.rxBuff)--;
                    Len++;
                    Decoder->parser.stageLen++;
                }
                break;
            case SBC_PARSE_SAMPLES:
                /* Unpack subband samples */
                SbcBuildSubbandSamples(Decoder, Decoder->parser.stageBuff);

                /* Perform joint processing if necessary */
                if (Decoder->streamInfo.channelMode == 
                    SBC_CHNL_MODE_JOINT_STEREO) {
                    SbcJointProcessing(Decoder);
                }

                /* Run samples through the synthesis filter */
                switch (Decoder->streamInfo.numSubBands) {
                case 4:
                    SbcSynthesisFilter4(Decoder, PcmData);
                    break;
                case 8:
                    SbcSynthesisFilter8(Decoder, PcmData);
                    break;
                }

                /* Start on next Frame */
                if ((MaxPcmData - PcmData->dataLen) >= Decoder->maxPcmLen) {
                    /* Decode next Frame */
                    SbcResetDecoderState(Decoder);
                } else {
                    /* PCM data block filled, return success */
                    status = XA_STATUS_SUCCESS;
                    SbcResetDecoderState(Decoder);
                    goto exit;
                }
            }

            Decoder->parser.curStageOff = 0;
        }
    }

    exit:

    *BytesDecoded -= Len;
    return status;
}
#endif 

#if SBC_ENCODER == XA_ENABLED
/*---------------------------------------------------------------------------
 *            SBC_InitEncoder()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initializes an Encoder structure for processing a new stream.  
 *            Must be called before each new stream is encoded.
 */
void SBC_InitEncoder(SbcEncoder *Encoder)
{
    //OS_MemSet((U8*)Encoder, 0, sizeof(SbcEncoder));
    memset((U8*)Encoder, 0, sizeof(SbcEncoder));
}

/*---------------------------------------------------------------------------
 *            SbcAnalysisFilter4()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Performs analysis of PCM data and creates 4 subband samples.
 */
static void SbcAnalysisFilter4(SbcEncoder *Encoder, SbcPcmData *PcmData)
{
    I8    i, blk;
    U8 *ptr;
    U16 offset = 0;
    REAL *sbSample;
    S16 *mem;

    for (blk = 0; blk < Encoder->streamInfo.numBlocks; blk++) {

        /* Initialize the pointer to current PCM data */
        ptr = PcmData->data + offset;

        /* If we have reached the bottom, shift historical samples to top */
        if (Encoder->X0pos == 0) {           
            for (i = 0; i < 36; i++) {
                Encoder->X0[(80 * 2) - 36 + i] = Encoder->X0[i+4];
            }
            Encoder->X0pos = (80 * 2) - 40;
        }

        /* Input Samples at current position */
        mem = &Encoder->X0[Encoder->X0pos];
        Encoder->X0pos -= 4;

        /* Input Samples */
        if (Encoder->streamInfo.numChannels == 1) {
            for (i = 4; i > 0; i--, ptr += 2) {
                mem[i-1] = PCMtoHost16(ptr);
            }
        } else {
            for (i = 4; i > 0; i--, ptr += 4) {  
                mem[i-1] = PCMtoHost16(ptr);
            }
        }

        Encoder->Y[0] = MulPI(mem[8]  + -mem[32], SbcCoefficient4[8]) + 
                        MulPI(mem[16] + -mem[24], SbcCoefficient4[16]);

        Encoder->Y[1] = MulPI(mem[1],   SbcCoefficient4[1]) +       
                        MulPI(mem[9],   SbcCoefficient4[9]) + 
                        MulPI(mem[17],  SbcCoefficient4[17]) + 
                        MulPI(mem[25],  SbcCoefficient4[25]) + 
                        MulPI(mem[33],  SbcCoefficient4[33]); 

        Encoder->Y[2] = MulPI(mem[2],   SbcCoefficient4[2]) +       
                        MulPI(mem[10],  SbcCoefficient4[10]) + 
                        MulPI(mem[18],  SbcCoefficient4[18]) + 
                        MulPI(mem[26],  SbcCoefficient4[26]) + 
                        MulPI(mem[34],  SbcCoefficient4[34]); 

        Encoder->Y[3] = MulPI(mem[3],   SbcCoefficient4[3]) +       
                        MulPI(mem[11],  SbcCoefficient4[11]) + 
                        MulPI(mem[19],  SbcCoefficient4[19]) + 
                        MulPI(mem[27],  SbcCoefficient4[27]) + 
                        MulPI(mem[35],  SbcCoefficient4[35]); 

        Encoder->Y[4] = MulPI(mem[4]  + mem[36], SbcCoefficient4[4]) +       
                        MulPI(mem[12] + mem[28],  SbcCoefficient4[12]) + 
                        MulPI(mem[20],  SbcCoefficient4[20]);

        Encoder->Y[5] = MulPI(mem[5],   SbcCoefficient4[5]) +       
                        MulPI(mem[13],  SbcCoefficient4[13]) + 
                        MulPI(mem[21],  SbcCoefficient4[21]) + 
                        MulPI(mem[29],  SbcCoefficient4[29]) + 
                        MulPI(mem[37],  SbcCoefficient4[37]); 

        Encoder->Y[6] = MulPI(mem[6],   SbcCoefficient4[6]) +       
                        MulPI(mem[14] + -mem[30],  SbcCoefficient4[14]) + 
                        MulPI(mem[22],  SbcCoefficient4[22]) + 
                        MulPI(mem[38],  SbcCoefficient4[38]); 

        Encoder->Y[7] = MulPI(mem[7],   SbcCoefficient4[7]) +       
                        MulPI(mem[15],  SbcCoefficient4[15]) + 
                        MulPI(mem[23],  SbcCoefficient4[23]) + 
                        MulPI(mem[31],  SbcCoefficient4[31]) + 
                        MulPI(mem[39],  SbcCoefficient4[39]); 

        for (i = 0; i < 4; i++) {
            sbSample = &Encoder->streamInfo.sbSample[blk][0][i];
            *sbSample  = MulP((Encoder->Y[0] + Encoder->Y[4]), Analyze4[0][i]);
            *sbSample += MulP((Encoder->Y[1] + Encoder->Y[3]), Analyze4[1][i]);
            *sbSample += Encoder->Y[2];
            *sbSample += MulP((Encoder->Y[5] - Encoder->Y[7]), Analyze4[5][i]);
        }

        if (Encoder->streamInfo.numChannels > 1) {

            /* Initialize the pointer to current PCM data */
            ptr = PcmData->data + offset + 2;

            /* If we have reached the bottom, shift historical samples to top */
            if (Encoder->X1pos == 0) {           
                for (i = 0; i < 36; i++) {
                    Encoder->X1[(80 * 2) - 36 + i] = Encoder->X1[i+4];
                }
                Encoder->X1pos = (80 * 2) - 40;
            }

            /* Input Samples at current position */
            mem = &Encoder->X1[Encoder->X1pos];
            Encoder->X1pos -= 4;

            /* Input Samples */
            if (Encoder->streamInfo.numChannels == 1) {
                for (i = 4; i > 0; i--, ptr += 2) {
                    mem[i-1] = PCMtoHost16(ptr);
                }
            } else {
                for (i = 4; i > 0; i--, ptr += 4) {
                    mem[i-1] = PCMtoHost16(ptr);
                }
            }

            Encoder->Y[0] = MulPI(mem[8]  + -mem[32], SbcCoefficient4[8]) + 
                            MulPI(mem[16] + -mem[24], SbcCoefficient4[16]);

            Encoder->Y[1] = MulPI(mem[1],   SbcCoefficient4[1]) +       
                            MulPI(mem[9],   SbcCoefficient4[9]) + 
                            MulPI(mem[17],  SbcCoefficient4[17]) + 
                            MulPI(mem[25],  SbcCoefficient4[25]) + 
                            MulPI(mem[33],  SbcCoefficient4[33]); 

            Encoder->Y[2] = MulPI(mem[2],   SbcCoefficient4[2]) +       
                            MulPI(mem[10],  SbcCoefficient4[10]) + 
                            MulPI(mem[18],  SbcCoefficient4[18]) + 
                            MulPI(mem[26],  SbcCoefficient4[26]) + 
                            MulPI(mem[34],  SbcCoefficient4[34]); 

            Encoder->Y[3] = MulPI(mem[3],   SbcCoefficient4[3]) +       
                            MulPI(mem[11],  SbcCoefficient4[11]) + 
                            MulPI(mem[19],  SbcCoefficient4[19]) + 
                            MulPI(mem[27],  SbcCoefficient4[27]) + 
                            MulPI(mem[35],  SbcCoefficient4[35]); 

            Encoder->Y[4] = MulPI(mem[4]  + mem[36], SbcCoefficient4[4]) +       
                            MulPI(mem[12] + mem[28],  SbcCoefficient4[12]) + 
                            MulPI(mem[20],  SbcCoefficient4[20]);

            Encoder->Y[5] = MulPI(mem[5],   SbcCoefficient4[5]) +       
                            MulPI(mem[13],  SbcCoefficient4[13]) + 
                            MulPI(mem[21],  SbcCoefficient4[21]) + 
                            MulPI(mem[29],  SbcCoefficient4[29]) + 
                            MulPI(mem[37],  SbcCoefficient4[37]); 

            Encoder->Y[6] = MulPI(mem[6],   SbcCoefficient4[6]) +       
                            MulPI(mem[14] + -mem[30],  SbcCoefficient4[14]) + 
                            MulPI(mem[22],  SbcCoefficient4[22]) + 
                            MulPI(mem[38],  SbcCoefficient4[38]); 

            Encoder->Y[7] = MulPI(mem[7],   SbcCoefficient4[7]) +       
                            MulPI(mem[15],  SbcCoefficient4[15]) + 
                            MulPI(mem[23],  SbcCoefficient4[23]) + 
                            MulPI(mem[31],  SbcCoefficient4[31]) + 
                            MulPI(mem[39],  SbcCoefficient4[39]); 

            for (i = 0; i < 4; i++) {
                sbSample = &Encoder->streamInfo.sbSample[blk][1][i];
                *sbSample  = MulP((Encoder->Y[0] + Encoder->Y[4]), Analyze4[0][i]);
                *sbSample += MulP((Encoder->Y[1] + Encoder->Y[3]), Analyze4[1][i]);
                *sbSample += Encoder->Y[2];
                *sbSample += MulP((Encoder->Y[5] - Encoder->Y[7]), Analyze4[5][i]);
            }

            /* Go to the next set of samples */
            offset += 16;
        } else {
            offset += 8;
        }
    }
}

/*---------------------------------------------------------------------------
 *            SbcAnalysisFilter8()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Performs analysis of PCM data and creates 8 subband samples.
 */

static void SbcAnalysisFilter8(SbcEncoder *Encoder, SbcPcmData *PcmData)
{
    short   *mem;
    int     i, blk, ch, t, t0, t1, t2;
    int     *ptr= (int *)PcmData->data;
    REAL    *sbSample=&Encoder->streamInfo.sbSample[0][0][0];
    int     numBlocks   = (int)Encoder->streamInfo.numBlocks;
    int     numChannels = (int)Encoder->streamInfo.numChannels;

    int     *bufPtr = (int *)Encoder->X0, *bufEnd = bufPtr+76;
    int     bufPos  = ((int)Encoder->X0pos>>1);

    if (bufPos<36)  bufPos = 36;

    bufPtr += bufPos;

    for (blk=0; blk<numBlocks; blk++) 
    {
        if (numChannels == 1)
        {
            bufPtr[0] = ptr[0];
            bufPtr[1] = ptr[1];
            bufPtr[2] = ptr[2];
            bufPtr[3] = ptr[3];
            ptr += 4;
            bufPtr += 4;
        } 
        else 
        {
            t0 = ptr[0];
            t1 = ptr[1];
            t0 ^= t1<<16;   t1 ^= ((unsigned int)t0)>>16;   t0 ^= t1<<16;
            bufPtr[ 0] = t0;
            bufPtr[76] = t1;
            t0 = ptr[2];
            t1 = ptr[3];
            t0 ^= t1<<16;   t1 ^= ((unsigned int)t0)>>16;   t0 ^= t1<<16;
            bufPtr[ 1] = t0;
            bufPtr[77] = t1;
            t0 = ptr[4];
            t1 = ptr[5];
            t0 ^= t1<<16;   t1 ^= ((unsigned int)t0)>>16;   t0 ^= t1<<16;
            bufPtr[ 2] = t0;
            bufPtr[78] = t1;
            t0 = ptr[6];
            t1 = ptr[7];
            t0 ^= t1<<16;   t1 ^= ((unsigned int)t0)>>16;   t0 ^= t1<<16;
            bufPtr[ 3] = t0;
            bufPtr[79] = t1;
            ptr += 8;
            bufPtr += 4;
        }

        mem = (short *)bufPtr; mem--;
        for (ch = 0; ch < numChannels; ch++, sbSample += 8)
        {
            if(ch>0)    mem = mem+76*2;

            t =
+ MulPI(mem[- 4],0x000D7FC5)    // C[04]= +8.23919506*10^(-4)*2^30
+ MulPI(mem[-20],0x00F01126)    // C[20]= +1.46525263*10^(-2)*2^30
+ MulPI(mem[-36],0x07E390FD)    // C[36]= +1.23264548*10^(-1)*2^30
- MulPI(mem[-52],0x00EFDE52)    // C[52]= -1.46404076*10^(-2)*2^30
+ MulPI(mem[-68],0x000EC7E9);   // C[68]= +9.02154502*10^(-4)*2^30
            sbSample[0] = t;
            sbSample[1] = t;
            sbSample[2] = t;
            sbSample[3] = t;
            sbSample[4] = t;
            sbSample[5] = t;
            sbSample[6] = t;
            sbSample[7] = t;

            t =  
+ MulPI(mem[- 8]+mem[-72],0x00174EB7) // C[08]=2.01182542*10^(-3)*cos(pi/4)*2^30
+ MulPI(mem[-16]-mem[-64],0x0041910C) // C[16]=5.65949473*10^(-3)*cos(pi/4)*2^30
+ MulPI(mem[-24]+mem[-56],0x0095E15C) // C[24]=1.29371806*10^(-2)*cos(pi/4)*2^30 
+ MulPI(mem[-32]-mem[-48],0x0313C8AE) // C[32]=6.79989431*10^(-2)*cos(pi/4)*2^30
+ MulPI(mem[-40]         ,0x06A68266);// C[40]=1.46955068*10^(-1)*cos(pi/4)*2^30
            sbSample[0] += t;
            sbSample[1] -= t;
            sbSample[2] -= t;
            sbSample[3] += t;
            sbSample[4] += t;
            sbSample[5] -= t;
            sbSample[6] -= t;
            sbSample[7] += t;

            t = 
+ MulPI(mem[- 2],0x00053221)    // C[02]= +3.43256425E-04*cos(pi/8)*2^30
+ MulPI(mem[-18],0x009E4ECD)    // C[18]= +1.04584443E-02*cos(pi/8)*2^30
+ MulPI(mem[-34],0x05C4FBBA)    // C[34]= +9.75753918E-02*cos(pi/8)*2^30
- MulPI(mem[-50],0x024F7965)    // C[50]= -3.90751381E-02*cos(pi/8)*2^30
- MulPI(mem[-66],0x0018F8C2)    // C[66]= -1.64973098E-03*cos(pi/8)*2^30
+ MulPI(mem[- 6],0x0016591A)    // C[06]= +1.47640169E-03*cos(pi/8)*2^30
+ MulPI(mem[-22],0x00F5884C)    // C[22]= +1.62208471E-02*cos(pi/8)*2^30
+ MulPI(mem[-38],0x08529048)    // C[38]= +1.40753505E-01*cos(pi/8)*2^30
+ MulPI(mem[-54],0x002C42EB)    // C[54]= +2.92408442E-03*cos(pi/8)*2^30
+ MulPI(mem[-70],0x001E30ED);   // C[70]= +1.99454554E-03*cos(pi/8)*2^30
            t0 = MulP(t, 0x1A82799A); // [cos(3pi/8)/cos(pi/8)]*2^30
            sbSample[0] += t;
            sbSample[1] += t0;
            sbSample[2] -= t0;
            sbSample[3] -= t;
            sbSample[4] -= t;
            sbSample[5] -= t0;
            sbSample[6] += t0;
            sbSample[7] += t;

            t = 
+ MulPI(mem[-10],0x001E30ED)    // C[10]= +1.99454554E-03*+cos(pi/8)*2^30
+ MulPI(mem[-26],0x002C42EB)    // C[26]= +2.92408442E-03*+cos(pi/8)*2^30
+ MulPI(mem[-42],0x08529048)    // C[42]= +1.40753505E-01*+cos(pi/8)*2^30
+ MulPI(mem[-58],0x00F5884C)    // C[58]= +1.62208471E-02*+cos(pi/8)*2^30
+ MulPI(mem[-74],0x0016591A)    // C[74]= +1.47640169E-03*+cos(pi/8)*2^30
+ MulPI(mem[-14],0x0018F8C2)    // C[14]= -1.64973098E-03*-cos(pi/8)*2^30
+ MulPI(mem[-30],0x024F7965)    // C[30]= -3.90751381E-02*-cos(pi/8)*2^30
- MulPI(mem[-46],0x05C4FBBA)    // C[46]= +9.75753918E-02*-cos(pi/8)*2^30
- MulPI(mem[-62],0x009E4ECD)    // C[62]= +1.04584443E-02*-cos(pi/8)*2^30
- MulPI(mem[-78],0x00053221);   // C[78]= +3.43256425E-04*-cos(pi/8)*2^30
            t0 = MulP(t, 0x1A82799A); // [cos(3pi/8)/cos(pi/8)]*2^30
            sbSample[0] += t0;
            sbSample[1] -= t;
            sbSample[2] += t;
            sbSample[3] -= t0;
            sbSample[4] -= t0;
            sbSample[5] += t;
            sbSample[6] -= t;
            sbSample[7] += t0;

            t = 
+ MulPI(mem[- 3],0x0008E98C)    // C[03]= +5.54620202E-04*cos(pi/16)*2^30
+ MulPI(mem[-19],0x00CCD671)    // C[19]= +1.27472335E-02*cos(pi/16)*2^30
+ MulPI(mem[-35],0x06FAD71D)    // C[35]= +1.11196689E-01*cos(pi/16)*2^30
- MulPI(mem[-51],0x01A3907F)    // C[51]= -2.61098752E-02*cos(pi/16)*2^30
- MulPI(mem[-67],0x0002DF8E)    // C[67]= -1.78805361E-04*cos(pi/16)*2^30
+ MulPI(mem[- 5],0x00125153)    // C[05]= +1.13992507E-03*cos(pi/16)*2^30
+ MulPI(mem[-21],0x00FF92C6)    // C[21]= +1.59045603E-02*cos(pi/16)*2^30
+ MulPI(mem[-37],0x085D7360)    // C[37]= +1.33264415E-01*cos(pi/16)*2^30
- MulPI(mem[-53],0x004EFE1A)    // C[53]= -4.91578024E-03*cos(pi/16)*2^30
+ MulPI(mem[-69],0x0019FA13);   // C[69]= +1.61656283E-03*cos(pi/16)*2^30
            t0 = MulP(t, 0x3641AF3D); // [cos(3pi/16)/cos(pi/16)]*2^30
            t1 = MulP(t, 0x2440CA5D); // [cos(5pi/16)/cos(pi/16)]*2^30
            t2 = MulP(t, 0x0CBAFAF0); // [cos(7pi/16)/cos(pi/16)]*2^30
            sbSample[0] += t;
            sbSample[1] += t0;
            sbSample[2] += t1;
            sbSample[3] += t2;
            sbSample[4] -= t2;
            sbSample[5] -= t1;
            sbSample[6] -= t0;
            sbSample[7] -= t;

            t = 
+ MulPI(mem[-11],0x019FA13) // C[11]= +1.61656283E-03*+cos(pi/16)*2^30
- MulPI(mem[-27],0x04EFE1A) // C[27]= -4.91578024E-03*+cos(pi/16)*2^30
+ MulPI(mem[-43],0x85D7360) // C[43]= +1.33264415E-01*+cos(pi/16)*2^30
+ MulPI(mem[-59],0x0FF92C6) // C[59]= +1.59045603E-02*+cos(pi/16)*2^30
+ MulPI(mem[-75],0x0125153) // C[75]= +1.13992507E-03*+cos(pi/16)*2^30
+ MulPI(mem[-13],0x002DF8E) // C[13]= -1.78805361E-04*-cos(pi/16)*2^30
+ MulPI(mem[-29],0x1A3907F) // C[29]= -2.61098752E-02*-cos(pi/16)*2^30
- MulPI(mem[-45],0x6FAD71D) // C[45]= +1.11196689E-01*-cos(pi/16)*2^30
- MulPI(mem[-61],0x0CCD671) // C[61]= +1.27472335E-02*-cos(pi/16)*2^30
- MulPI(mem[-77],0x008E98C);// C[77]= +5.54620202E-04*-cos(pi/16)*2^30
            t0 = MulP(t,0x3641AF3D); // [cos(3pi/16)/cos(pi/16)]*2^30
            t1 = MulP(t,0x2440CA5D); // [cos(5pi/16)/cos(pi/16)]*2^30
            t2 = MulP(t,0x0CBAFAF0); // [cos(7pi/16)/cos(pi/16)]*2^30
            sbSample[0] += t2;
            sbSample[1] -= t1;
            sbSample[2] += t0;
            sbSample[3] -= t;
            sbSample[4] += t;
            sbSample[5] -= t0;
            sbSample[6] += t1;
            sbSample[7] -= t2;

            t = 
+ MulPI(mem[- 1], 0x0002841B)   // C[01]= +1.56575398E-04*cos(pi/16)*2^30
+ MulPI(mem[-17], 0x008106AF)   // C[17]= +8.02941163E-03*cos(pi/16)*2^30
+ MulPI(mem[-33], 0x05357F5D)   // C[33]= +8.29847578E-02*cos(pi/16)*2^30
- MulPI(mem[-49], 0x0356AD3A)   // C[49]= -5.31873032E-02*cos(pi/16)*2^30
- MulPI(mem[-65], 0x0038325E)   // C[65]= -3.49717454E-03*cos(pi/16)*2^30
+ MulPI(mem[- 7], 0x001CA9B3)   // C[07]= +1.78371725E-03*cos(pi/16)*2^30
+ MulPI(mem[-23], 0x00F62786)   // C[23]= +1.53184106E-02*cos(pi/16)*2^30
+ MulPI(mem[-39], 0x09204BE7)   // C[39]= +1.45389847E-01*cos(pi/16)*2^30
+ MulPI(mem[-55], 0x008E5583)   // C[55]= +8.85757540E-03*cos(pi/16)*2^30
+ MulPI(mem[-71], 0x0021CE19);  // C[71]= +2.10371989E-03*cos(pi/16)*2^30
            t0 = MulP(t, 0x3641AF3D); // [cos(3pi/16)/cos(pi/16)]*2^30
            t1 = MulP(t, 0x2440CA5D); // [cos(5pi/16)/cos(pi/16)]*2^30
            t2 = MulP(t, 0x0CBAFAF0); // [cos(7pi/16)/cos(pi/16)]*2^30
            sbSample[0] += t0;
            sbSample[1] -= t2;
            sbSample[2] -= t;
            sbSample[3] -= t1;
            sbSample[4] += t1;
            sbSample[5] += t;
            sbSample[6] += t2;
            sbSample[7] -= t0;

            t = 
+ MulPI(mem[- 9], 0x021CE19)    // C[09]= +2.10371989E-03*+cos(pi/16)*2^30
+ MulPI(mem[-25], 0x08E5583)    // C[25]= +8.85757540E-03*+cos(pi/16)*2^30
+ MulPI(mem[-41], 0x9204BE7)    // C[41]= +1.45389847E-01*+cos(pi/16)*2^30
+ MulPI(mem[-57], 0x0F62786)    // C[57]= +1.53184106E-02*+cos(pi/16)*2^30
+ MulPI(mem[-73], 0x01CA9B3)    // C[73]= +1.78371725E-03*+cos(pi/16)*2^30
+ MulPI(mem[-15], 0x038325E)    // C[15]= -3.49717454E-03*-cos(pi/16)*2^30
+ MulPI(mem[-31], 0x356AD3A)    // C[31]= -5.31873032E-02*-cos(pi/16)*2^30
- MulPI(mem[-47], 0x5357F5D)    // C[47]= +8.29847578E-02*-cos(pi/16)*2^30
- MulPI(mem[-63], 0x08106AF)    // C[63]= +8.02941163E-03*-cos(pi/16)*2^30
- MulPI(mem[-79], 0x002841B);   // C[79]= +1.56575398E-04*-cos(pi/16)*2^30
            t0 = MulP(t, 0x3641AF3D); // [cos(3pi/16)/cos(pi/16)]*2^30
            t1 = MulP(t, 0x2440CA5D); // [cos(5pi/16)/cos(pi/16)]*2^30
            t2 = MulP(t, 0x0CBAFAF0); // [cos(7pi/16)/cos(pi/16)]*2^30
            sbSample[0] += t1;
            sbSample[1] -= t;
            sbSample[2] += t2;
            sbSample[3] += t0;
            sbSample[4] -= t0;
            sbSample[5] -= t2;
            sbSample[6] += t;
            sbSample[7] -= t1;
        }

        if (numChannels == 1) sbSample += 8;
        if (bufPtr == bufEnd)
        {
            bufPtr -= 40;

            for (i = 0; i < 36; i++)    bufPtr[i-36] = bufPtr[i+4];

            if(numChannels>1)
            for (i = 0; i < 36; i++)    bufPtr[i+40] = bufPtr[i+80];
        }
    }

    Encoder->X0pos = Encoder->X1pos = (76-(bufEnd-bufPtr))<<1;

}

/*---------------------------------------------------------------------------
 *            SbcCalculateScaleFactors()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Calculates the scaling factors for the current subband
 *            samples.
 */
static void SbcCalculateScaleFactors(SbcEncoder *Encoder)
{
    int blk, sb;
    int maxValue0, maxValue1, jMaxValue0, jMaxValue1;
    int tmp,value0, value1, jValue0, jValue1;
    int numBlocks   = (int)Encoder->streamInfo.numBlocks;
    int numSubBands = (int)Encoder->streamInfo.numSubBands;
    int numChannels = (int)Encoder->streamInfo.numChannels;

    if (numChannels == 1)
    {
        for (sb = 0; sb < numSubBands; sb++)
        {
            maxValue0 = 0;


            for (blk = 0; blk < numBlocks; blk++)
            {
                value0 =(int)Encoder->streamInfo.sbSample[blk][0][sb];
                tmp = (value0^(value0>>31)) - (value0>>31); 
                maxValue0 |= tmp>>15;
        }

        value0 = 2; value1 = 0;
        while (maxValue0 > value0) {value0<<=1; value1++;}
        Encoder->streamInfo.scaleFactors[0][sb] = value0;
        Encoder->streamInfo.scale_factors[0][sb] = value1;
    }

    }
    else if(Encoder->streamInfo.channelMode != SBC_CHNL_MODE_JOINT_STEREO)
    {
        for (sb = 0; sb < numSubBands; sb++)
        {
            maxValue0 = maxValue1 = 0;

            // Find the maximum absolute value in the subband
            for (blk = 0; blk < numBlocks; blk++)
            {

                value0 =(int)Encoder->streamInfo.sbSample[blk][0][sb];
                tmp = (value0^(value0>>31)) - (value0>>31); // tmp=abs(value0)
                maxValue0 |= tmp>>15;
 
                value1 =(int)Encoder->streamInfo.sbSample[blk][1][sb];
                tmp = (value1 ^ (value1>>31)) - (value1>>31); // tmp=abs(value1)
                maxValue1 |= tmp>>15;
            }

            // Determine the scale factors for channel 0 
            value0 = 2; value1 = 0;
            while (maxValue0 > value0) {value0<<=1; value1++;}
            Encoder->streamInfo.scaleFactors[0][sb] = value0;
            Encoder->streamInfo.scale_factors[0][sb] = value1;

            // Determine the scale factor for channel 1
            value0 = 2; value1 = 0;
            while (maxValue1 > value0) {value0<<=1; value1++;}
            Encoder->streamInfo.scaleFactors[1][sb] = value0;
            Encoder->streamInfo.scale_factors[1][sb] = value1;

            Encoder->streamInfo.join[sb] = 0;
        }
    }
    else
    {
        for (sb = 0; sb < numSubBands-1; sb++)
        {
            maxValue0 = maxValue1 = jMaxValue0 = jMaxValue1 = 0;

            /* Find the maximum absolute value in the subband */
            for (blk = 0; blk < numBlocks; blk++) 
            {
                value0 =(int)Encoder->streamInfo.sbSample[blk][0][sb];
                tmp = (value0^(value0>>31)) - (value0>>31); // tmp=abs(value0)
                maxValue0 |= tmp>>15;

                value1 =(int)Encoder->streamInfo.sbSample[blk][1][sb];
                tmp = (value1^(value1>>31)) - (value1>>31); // tmp=abs(value1)
                maxValue1 |= tmp>>15;

                /// Joint Processing Difference and Sum averages
                Encoder->sbJoint[blk][0] = (value0>>1)+(value1>>1);
                Encoder->sbJoint[blk][1] = (value0>>1)-(value1>>1);

                jValue0 =(int)Encoder->sbJoint[blk][0];
                tmp = (jValue0^(jValue0>>31))-(jValue0>>31); // tmp=abs(jValue0)
                jMaxValue0 |= tmp>>15;

                jValue1 =(int)Encoder->sbJoint[blk][1];
                tmp = (jValue1^(jValue1>>31))-(jValue1>>31); // tmp=abs(jValue1)
                jMaxValue1 |= tmp>>15;
            }

            // Determine the scale factors for channel 0
            value0 = 2; value1 = 0;
            while (maxValue0 > value0) {value0<<=1; value1++;}
            Encoder->streamInfo.scaleFactors[0][sb] = value0;
            Encoder->streamInfo.scale_factors[0][sb] = value1;

            // Determine the scale factor for channel 1
            value0 = 2; value1 = 0;
            while (maxValue1 > value0) {value0<<=1; value1++;}
            Encoder->streamInfo.scaleFactors[1][sb] = value0;
            Encoder->streamInfo.scale_factors[1][sb] = value1;

            Encoder->streamInfo.join[sb] = 0;

            // Determine the scale factors
            value0 = 0; while (jMaxValue0 > 1) {jMaxValue0 >>= 1;   value0++;}
            value1 = 0; while (jMaxValue1 > 1) {jMaxValue1 >>= 1;   value1++;}

            if ((Encoder->streamInfo.scale_factors[0][sb] + 
                 Encoder->streamInfo.scale_factors[1][sb]) > (value0 + value1))
            {
                Encoder->streamInfo.scale_factors[0][sb] = value0;
                Encoder->streamInfo.scale_factors[1][sb] = value1;

                Encoder->streamInfo.scaleFactors[0][sb] = ((U32)1<<(value0+1));
                Encoder->streamInfo.scaleFactors[1][sb] = ((U32)1<<(value1+1));

                // Use joint coding
                for (blk = 0; blk < Encoder->streamInfo.numBlocks; blk++)
                {
                    Encoder->streamInfo.sbSample[blk][0][sb] = 
                        Encoder->sbJoint[blk][0];
                    Encoder->streamInfo.sbSample[blk][1][sb] = 
                        Encoder->sbJoint[blk][1];
                }
                Encoder->streamInfo.join[sb] = 1;
            }
        }
        maxValue0 = maxValue1 = 0;

        /* Find the maximum absolute value in the LAST subband */
        for (blk = 0; blk < numBlocks; blk++)
        {
            value0 =(int)Encoder->streamInfo.sbSample[blk][0][numSubBands-1];
            tmp = (value0 ^ (value0>>31)) - (value0>>31);
            maxValue0 |= tmp>>15;

            value1 =(int)Encoder->streamInfo.sbSample[blk][1][numSubBands-1];
            tmp = (value1 ^ (value1>>31)) - (value1>>31);
            maxValue1 |= tmp>>15;
        }

        value0 = 2; value1 = 0;
        while (maxValue0 > value0) {value0<<=1; value1++;}
        Encoder->streamInfo.scaleFactors[0][numSubBands-1] = value0;
        Encoder->streamInfo.scale_factors[0][numSubBands-1] = value1;

        value0 = 2; value1 = 0;
        while (maxValue1 > value0) {value0<<=1; value1++;}
        Encoder->streamInfo.scaleFactors[1][numSubBands-1] = value0;
        Encoder->streamInfo.scale_factors[1][numSubBands-1] = value1;

        Encoder->streamInfo.join[numSubBands-1] = 0;
    }
}

/*---------------------------------------------------------------------------
 *            SbcPrepareHeader()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Creates the header for the current SBC frame and writes it to
 *            the stream buffer.
 */
static void SbcPrepareHeader(SbcEncoder *Encoder, U8 *Buff, U16 *Len)
{
    I8    sb, ch;
    U8   *crc;
    BOOL  middle = FALSE;

    /* Initialize FCS */
    Encoder->streamInfo.fcs = 0x0F;

    /* Sync Word */
    Buff[(*Len)++] = SBC_SYNC_WORD;

    /* Sampling frequency */
    Buff[*Len] = Encoder->streamInfo.sampleFreq << 6;

    /* Blocks */
    switch (Encoder->streamInfo.numBlocks) {
    case 8:
        Buff[*Len] |= 1 << 4;
        break;
    case 12:
        Buff[*Len] |= 2 << 4;
        break;
    case 16:
        Buff[*Len] |= 3 << 4;
        break;
    }

    /* Channel mode */
    Buff[*Len] |= Encoder->streamInfo.channelMode << 2;

    /* Allocation Method */
    Buff[*Len] |= Encoder->streamInfo.allocMethod << 1;

    /* Subbands */
    if (Encoder->streamInfo.numSubBands == 8) {
        Buff[*Len] |= 1;
    }

    /* Checksum first byte */
    Encoder->streamInfo.fcs = 
        SbcCrcTable[Encoder->streamInfo.fcs ^ Buff[(*Len)++]];

    /* Bitpool size */
    Buff[*Len] = Encoder->streamInfo.bitPool;

    /* Checksum second byte */
    Encoder->streamInfo.fcs = 
        SbcCrcTable[Encoder->streamInfo.fcs ^ Buff[(*Len)++]];

    /* Skip the CRC */
    crc = &Buff[*Len];
    (*Len)++;
    Encoder->streamInfo.bitOffset = *Len * 8;

    /* Join bits */
    if (Encoder->streamInfo.channelMode == SBC_CHNL_MODE_JOINT_STEREO) {
        Buff[*Len] = 0;
        for (sb = 0; sb < Encoder->streamInfo.numSubBands; sb++) {
            if (Encoder->streamInfo.join[sb]) {
                Buff[*Len] |= 1 << (7 - sb);
            }
        }

        if (Encoder->streamInfo.numSubBands == 8) {
            /* Checksum fourth byte */
            Encoder->streamInfo.fcs = SbcCrcTable[Encoder->streamInfo.fcs ^ 
                                                  Buff[(*Len)++]];
            Encoder->streamInfo.bitOffset += 8;
        } else {
            middle = TRUE;
            Encoder->streamInfo.bitOffset += 4;
        }
    }

    /* Scale Factors */
    for (ch = 0; ch < Encoder->streamInfo.numChannels; ch++) {
        for (sb = 0; sb < Encoder->streamInfo.numSubBands; sb++) {
            if (middle) {
                Buff[*Len] |= Encoder->streamInfo.scale_factors[ch][sb];

                /* Checksum the byte */
                Encoder->streamInfo.fcs = SbcCrcTable[Encoder->streamInfo.fcs ^ 
                                                      Buff[(*Len)++]];

                middle = FALSE;
            } else {
                Buff[*Len] = Encoder->streamInfo.scale_factors[ch][sb] << 4;

                if ((ch == (I8)(Encoder->streamInfo.numChannels - 1)) &&
                    (sb == (I8)(Encoder->streamInfo.numSubBands - 1))) {

                    /* Sum the next 4 bits */
                    SbcCrcSum4(&Encoder->streamInfo, Buff[*Len]);
                    Buff[*Len] &= 0xF0;
                }

                middle = TRUE;
            }

            Encoder->streamInfo.bitOffset += 4;
        }
    }

    /* Save the CRC */
    *crc = Encoder->streamInfo.fcs;
}

/*---------------------------------------------------------------------------
 *            SbcPackSample()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Packs an audio sample into the stream.  Samples are inserted
 *            in the stream based on the bits required for the current
 *            subband.
 */

/*---------------------------------------------------------------------------
 *            SbcQantizeSamples()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Quantizes subband samples and packs them into the stream.
 */

static void SbcQuantizeSamples(SbcEncoder *Encoder, U8 *byteBuf, U16 *oldLen)
{
#if SBC_USE_FIXED_POINT == XA_ENABLED

    int blk, ch, sb, sample, bit, bits, tmp;
    U8  *ptr = byteBuf+(int)*oldLen, *siBits, *sf;

    REAL    *sbSample;
    int     bitOffset   = (int)Encoder->streamInfo.bitOffset;
    int     numBlocks   = (int)Encoder->streamInfo.numBlocks;
    int     numChannels = (int)Encoder->streamInfo.numChannels;
    int     numSubBands = (int)Encoder->streamInfo.numSubBands;

    // Quantize and store sample
    for (blk = 0; blk < numBlocks; blk++)
    {
        sbSample= &Encoder->streamInfo.sbSample[blk][0][0];
        siBits  = &Encoder->streamInfo.bits[0][0];
        sf      = &Encoder->streamInfo.scale_factors[0][0];

        for (ch = 0; ch < numChannels; ch++) {
            for (sb = 0; sb < numSubBands; sb++, sbSample++, siBits++, sf++) 
            {
                bits = (int)*siBits;
                if (bits > 0)
                {
    //Assert(Encoder->streamInfo.bits[ch][sb] <= 16);
    sample   = (int)*sbSample;
    sample >>= 1;
    sample <<= 15 - *sf;
    sample  += 0x40000000;

    sample = (sample>>(30-bits)) - (sample>>30);
    sample = sample>>1;

    bit = 8 - (bitOffset & 7);  // 8 - (bitOffset mod 8)
    if (bit == 8)   *ptr = 0;
    bitOffset += bits;
    tmp = ((bits-bit)>>3) + 1;

    if (bits <= bit)    *ptr |= sample << (bit-bits);
    else
    {
        bits -= bit;
        *ptr |= sample >> bits;

        if (bits <= 8)      *(ptr+1) = sample << (8-bits);
        else
        {
            bits -= 8;
            *(ptr+1) = sample >> bits;
            *(ptr+2) = sample << (8-bits);
        }
    }
    ptr += tmp;
                }
            }
        }
    }
 
    Encoder->streamInfo.bitOffset = (U16)bitOffset;
    *oldLen = ptr - byteBuf;

#else
    int blk, ch, sb, sample, bit, bits, len = (int)*oldLen;
    int bitOffset = (int)Encoder->streamInfo.bitOffset;
    U8  *ptr;

    // Calculate levels
    for(ch = 0; ch < Encoder->streamInfo.numChannels; ch++)
        for(sb = 0; sb < Encoder->streamInfo.numSubBands; sb++)
            Encoder->streamInfo.levels[ch][sb] = 
                ((U16)1 << Encoder->streamInfo.bits[ch][sb]) - 1;

    // Quantize and store sample 
    for (blk = 0; blk < Encoder->streamInfo.numBlocks; blk++) {
        for (ch = 0; ch < Encoder->streamInfo.numChannels; ch++) {
            for (sb = 0; sb < Encoder->streamInfo.numSubBands; sb++) {
                bits = (int)Encoder->streamInfo.bits[ch][sb];
                if (bits > 0) {
                    //Assert(Encoder->streamInfo.bits[ch][sb] <= 16);

    sample = RealtoU16(Mul(Encoder->streamInfo.sbSample[blk][ch][sb] / 
          Encoder->streamInfo.scaleFactors[ch][sb] + 
          ONE_F_P, Encoder->streamInfo.levels[ch][sb]) / 2);

    bit = 8 - (bitOffset & 7);  // 8 - (bitOffset mod 8)
    if (bit == 8)   *ptr = 0;
    bitOffset += bits;
    tmp = ((bits-bit)>>3) + 1;

    if (bits <= bit)    *ptr |= sample << (bit-bits);
    else
    {
        bits -= bit;
        *ptr |= sample >> bits;

        if (bits <= 8)      *(ptr+1) = sample << (8-bits);
        else
        {
            bits -= 8;
            *(ptr+1) = sample >> bits;
            *(ptr+2) = sample << (8-bits);
        }
    }
    ptr += tmp;
                }
            }
        }
    }

    Encoder->streamInfo.bitOffset = (U16)bitOffset;
    *oldLen = ptr - byteBuf;
#endif
}


/*---------------------------------------------------------------------------
 *            SBC_EncodeFrames()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Encodes SBC frames into the PCM data stream.
 */
XaStatus SBC_EncodeFrames(SbcEncoder *Encoder, SbcPcmData *PcmData, 
                          U16 *BytesEncoded,  U8 *Buff, U16 *Len,
                          U16 MaxSbcData)
{
    I8         ch;
    XaStatus   status = XA_STATUS_SUCCESS;
    U16        bytesToEncode;
    U16        bytesLeft = PcmData->dataLen;
    U16        encodedFrameLen;
    SbcPcmData pcmData;

#if XA_DEBUG == XA_ENABLED
    U16        oldLen;
#endif

    CheckUnlockedParm(XA_STATUS_INVALID_PARM, 
                      SbcIsValidStreamInfo(&Encoder->streamInfo));

    pcmData.data = PcmData->data;
    pcmData.dataLen = PcmData->dataLen;

    encodedFrameLen = SBC_FrameLen(&Encoder->streamInfo);           
    if (encodedFrameLen > MaxSbcData) {
        status = XA_STATUS_FAILED;
        goto exit;
    }

    /* Set the number of channels */
    if (Encoder->streamInfo.channelMode == SBC_CHNL_MODE_MONO) {
        Encoder->streamInfo.numChannels = 1;
    } else {
        Encoder->streamInfo.numChannels = 2;
    }

    /* Set the amount of PCM data to encode */
    bytesToEncode = Encoder->streamInfo.numChannels * 
                    Encoder->streamInfo.numSubBands * 
                    Encoder->streamInfo.numBlocks * 2;

    *Len = 0;
    *BytesEncoded = 0;
    if (bytesLeft < bytesToEncode) {
        status = XA_STATUS_CONTINUE;
        goto exit;
    }

    while (bytesLeft >= bytesToEncode) {

        /* Subband Analysis */
        if (Encoder->streamInfo.numSubBands == 4) {
            SbcAnalysisFilter4(Encoder, &pcmData);
        } else {
            SbcAnalysisFilter8(Encoder, &pcmData);
        }

        /* Scale Factors */
        SbcCalculateScaleFactors(Encoder);
        
        /* Bit allocation */
        switch (Encoder->streamInfo.channelMode) {
        case SBC_CHNL_MODE_DUAL_CHNL:
        case SBC_CHNL_MODE_MONO:
            for (ch = 0; ch < Encoder->streamInfo.numChannels; ch++) {
                SbcMonoBitAlloc(&Encoder->streamInfo, (U8)ch);
            }
            break;
        case SBC_CHNL_MODE_JOINT_STEREO:
        case SBC_CHNL_MODE_STEREO:
            SbcStereoBitAlloc(&Encoder->streamInfo);
            break;
        }

#if XA_DEBUG == XA_ENABLED
        oldLen = *Len;
#endif

        /* Prepare header */
        SbcPrepareHeader(Encoder, Buff, Len);

        /* Quantize and pack bits */
        SbcQuantizeSamples(Encoder, Buff, Len);

#if XA_DEBUG == XA_ENABLED
        //Assert(encodedFrameLen == *Len - oldLen);
#endif

        bytesLeft -= bytesToEncode;
        *BytesEncoded += bytesToEncode;

        if ((*Len + encodedFrameLen) > MaxSbcData) {
            /* SBC buffer is filled as much as possible */
            goto exit;
        } else {
            pcmData.data += bytesToEncode;
            pcmData.dataLen -= bytesToEncode;
        }
    }

    exit:

    return status;
}
#endif


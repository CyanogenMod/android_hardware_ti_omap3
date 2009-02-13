/****************************************************************************
 *
 * File:
 *     $Workfile:sbc_math.c$ for iAnywhere AV SDK, version 1.2.3
 *     $Revision:22$
 *
 * Description: This file contains sample ASM routines for fixed point math.
 *             
 * Created:     September 14, 2004
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

/*---------------------------------------------------------------------------
 *  The following Mul, MulP, and Div functions are implemented for the Intel
 *  architecture with 64 bit registers using the following formula:
 *
 *  Mul:  (x * y) >> 15
 *
 *  MulP:  (x * y) >> 30
 *
 *  MulPI: ((x << 15) * y) >> 30 
 *             or 
 *         (x * y) >> 15
 *
 *  dMulP: (x >> 13) * (y)
 */
#pragma arm

/* X86 Assembly routine for fixed multiply*/
static REAL INLINE Mul(REAL x, REAL y)
{
    __asm {
        mov  eax, x
        xor  edx, edx
        imul y
        shrd eax, edx, 15
    }
} 

/* X86 Assembly routine for fixed multiply for high precision */
static REAL INLINE MulP(REAL x, REAL y)
{
    __asm {
        mov  eax, x
        xor  edx, edx
        imul y
        shrd eax, edx, 30
    }
}

/* X86 Assembly routine for fixed multiply for high precision */
static REAL INLINE MulPI(REAL x, REAL y)
{
    __asm {
        mov  eax, x
        xor  edx, edx
        imul y
        shrd eax, edx, 15
    }
}
    
#if SBC_DECODER == XA_ENABLED
/* X86 Assembly routine for fixed multiply for high precision */
static REAL INLINE dMulP(REAL x, REAL y)
{
    x = x >> 13;

    __asm {
        mov  eax, x
        xor  edx, edx
        imul y
    }
}

#endif


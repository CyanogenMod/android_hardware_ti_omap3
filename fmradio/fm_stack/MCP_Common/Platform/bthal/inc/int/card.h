/***************************************************************************
 *
 * File:
 *     $Workfile:card.h$ for XTNDAccess Blue SDK, Version 1.3
 *     $Revision:1$
 *
 * Description: Header file for the card reader application.
 *
 * Created:     May 6, 2002
 *
 * Copyright 1999-2002 Extended Systems, Inc.  ALL RIGHTS RESERVED.
 *
 * Unpublished Confidential Information of Extended Systems, Inc.  
 * Do Not Disclose.
 *
 * No part of this work may be used or reproduced in any form or by any 
 * means, or stored in a database or retrieval system, without prior written 
 * permission of Extended Systems, Inc.
 * 
 * Use of this work is governed by a license granted by Extended Systems, 
 * Inc.  This work contains confidential and proprietary information of 
 * Extended Systems, Inc. which is protected by copyright, trade secret, 
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/

#ifndef __CARD_H
#define __CARD_H

#include <bttypes.h>

/* File Types */
                               
#define MASTER_FILE_ID        0x3F
#define LVL1_DEDICATED_ID     0x7F
#define LVL2_DEDICATED_ID     0x5F
#define MASTER_ELEMENTARY_ID  0x2F
#define LVL1_ELEMENTARY_ID    0x6F
#define LVL2_ELEMENTARY_ID    0x4F

/* APDU instructions */

#define INS_SELECT             0xA4
#define INS_STATUS             0xF2
#define INS_READ_BINARY        0xB0
#define INS_UPDATE_BINARY      0xD6
#define INS_READ_RECORD        0xB2
#define INS_UPDATE_RECORD      0xDC
#define INS_SEEK               0xA2
#define INS_INCREASE           0x32
#define INS_VERIFY_CHV         0x20
#define INS_CHANGE_CHV         0x24
#define INS_DISABLE_CHV        0x26
#define INS_ENABLE_CHV         0x28
#define INS_UNBLOCK_CHV        0x2C
#define INS_INVALIDATE         0x04
#define INS_GSM_ALGORITHM      0x88
#define INS_SLEEP              0xFA
#define INS_RESPONSE           0xC0
#define INS_TERMINAL_PROFILE   0x10
#define INS_ENVELOPE           0xC2
#define INS_FETCH              0x12
#define INS_TERMINAL_RESPONSE  0x14

/* Status Codes */

typedef U16 CardStatus;

#define STATUS_NORMAL              0x9000
#define STATUS_NORMAL_PROACTIVE_X  0x9100
#define STATUS_NORMAL_DNLD_ERR_X   0x9E00
#define STATUS_NORMAL_X            0x9F00
#define STATUS_POSTPONED           0x9300
#define STATUS_MEMORY_RETRY_X      0x9200
#define STATUS_MEMORY_ERR          0x9240
#define STATUS_NO_EF_SELECTED      0x9400
#define STATUS_OUT_OF_RANGE        0x9402
#define STATUS_FILE_ID_NOT_FOUND   0x9404
#define STATUS_FILE_INCONSISTENT   0x9408
#define STATUS_NO_CHV_INITIALIZED  0x9802
#define STATUS_NO_ACCESS           0x9804
#define STATUS_CONTRADICT_CHV      0x9808
#define STATUS_CONTRADICT_INV      0x9810
#define STATUS_FAILED_CHV          0x9840
#define STATUS_CANNOT_INCREASE     0x9850
#define STATUS_INVALID_P3          0x6700
#define STATUS_INVALID_P1_P2       0x6B00
#define STATUS_INVALID_P1_P2_RANGE 0x6B02
#define STATUS_INVALID_INS         0x6D00
#define STATUS_INVALID_INS_CLASS   0x6E00
#define STATUS_UNKNOWN_ERR         0x6F00

/* Function prototypes */

void CRD_Init(void);
void CRD_Deinit(void);
void CRD_GetAtr(U8 **AtrPtr, U16 *Len);
void CRD_ParseAPDU(U8 *ApduPtr, U16 Len, U8 **RspApduPtr, U16 *RspLen);

void CrdSelect(U8 *ApduPtr, U16 Len); 
void CrdStatus(U8 *ApduPtr, U16 Len); 
void CrdReadBinary(U8 *ApduPtr, U16 Len); 
void CrdUpdateBinary(U8 *ApduPtr, U16 Len); 
void CrdReadRecord(U8 *ApduPtr, U16 Len); 
void CrdUpdateRecord(U8 *ApduPtr, U16 Len); 
void CrdSeek(U8 *ApduPtr, U16 Len); 
void CrdIncrease(U8 *ApduPtr, U16 Len); 
void CrdVerifyChv(U8 *ApduPtr, U16 Len);
void CrdChangeChv(U8 *ApduPtr, U16 Len); 
void CrdDisableChv(U8 *ApduPtr, U16 Len); 
void CrdEnableChv(U8 *ApduPtr, U16 Len); 
void CrdUnblockChv(U8 *ApduPtr, U16 Len); 
void CrdInvalidate(U8 *ApduPtr, U16 Len); 
void CrdGsmAlgorithm(U8 *ApduPtr, U16 Len); 
void CrdSleep(U8 *ApduPtr, U16 Len); 
void CrdResponse(U8 *ApduPtr, U16 Len); 
void CrdTerminalProfile(U8 *ApduPtr, U16 Len); 
void CrdEnvelope(U8 *ApduPtr, U16 Len); 
void CrdFetch(U8 *ApduPtr, U16 Len); 
void CrdTerminalResponse(U8 *ApduPtr, U16 Len); 

#endif __CARD_H


/******************************************************************************\
##                                                                            *
## Unpublished Proprietary and Confidential Information of Texas Instruments  *
## Israel Ltd. Do Not Disclose.                                               *
## Copyright 2008 Texas Instruments Israel Ltd.                               *
## All rights reserved. All unpublished rights reserved.                      *
##                                                                            *
## No part of this work may be used or reproduced in any form or by any       *
## means, or stored in a database or retrieval system, without prior written  *
## permission of Texas Instruments Israel Ltd. or its parent company Texas    *
## Instruments Incorporated.                                                  *
## Use of this work is subject to a license from Texas Instruments Israel     *
## Ltd. or its parent company Texas Instruments Incorporated.                 *
##                                                                            *
## This work contains Texas Instruments Israel Ltd. confidential and          *
## proprietary information which is protected by copyright, trade secret,     *
## trademark and other intellectual property rights.                          *
##                                                                            *
## The United States, Israel  and other countries maintain controls on the    *
## export and/or import of cryptographic items and technology. Unless prior   *
## authorization is obtained from the U.S. Department of Commerce and the     *
## Israeli Government, you shall not export, reexport, or release, directly   *
## or indirectly, any technology, software, or software source code received  *
## from Texas Instruments Incorporated (TI) or Texas Instruments Israel,      *
## or export, directly or indirectly, any direct product of such technology,  *
## software, or software source code to any destination or country to which   *
## the export, reexport or release of the technology, software, software      *
## source code, or direct product is prohibited by the EAR. The subject items *
## are classified as encryption items under Part 740.17 of the Commerce       *
## Control List (“CCL”). The assurances provided for herein are furnished in  *
## compliance with the specific encryption controls set forth in Part 740.17  *
## of the EAR -Encryption Commodities and Software (ENC).                     *
##                                                                            *
## NOTE: THE TRANSFER OF THE TECHNICAL INFORMATION IS BEING MADE UNDER AN     *
## EXPORT LICENSE ISSUED BY THE ISRAELI GOVERNMENT AND THE APPLICABLE EXPORT  *
## LICENSE DOES NOT ALLOW THE TECHNICAL INFORMATION TO BE USED FOR THE        *
## MODIFICATION OF THE BT ENCRYPTION OR THE DEVELOPMENT OF ANY NEW ENCRYPTION.*
## UNDER THE ISRAELI GOVERNMENT'S EXPORT LICENSE, THE INFORMATION CAN BE USED *
## FOR THE INTERNAL DESIGN AND MANUFACTURE OF TI PRODUCTS THAT WILL CONTAIN   *
## THE BT IC.                                                                 *
##                                                                            *
\******************************************************************************/
/*******************************************************************************\
*
*   FILE NAME:		Card.c
*
*   BRIEF:			Implementation of the SIM Card communication module.
*
*   DESCRIPTION:		This module translate SAP commands to SIM Application commands.
*					The setup needed for this module is:
*					1. BT Radio for the EBTIPS Win App.
*					2. Neptune modem connected to a com port no MODEM_COM_PORT in
*					    the PC.
*					3. SIM card inside the Neptune. (Orange or Cellcom)
*
*   INTEROPERABILITY  The module was tested with Orange and Cellcom SIM Cards. There are
*					distinctions between the two models, Notice the compilation flag.
*
*
*   AUTHOR:			Elad Shultz
*
\*******************************************************************************/

/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include <stdio.h>
#include "card.h"
#include "com.h"
#include "bthal_types.h"

/* SIM Card HAL definitions */

/* SIM command definition according to 3GPP 27007-730 */
#define SIM_COMMAND_READ_BINARY 176		/* read binary */
#define SIM_COMMAND_READ_RECORD 178		/* read record */
#define SIM_COMMAND_GET_RESPONSE 192	/* get response */
#define SIM_COMMAND_UPDATE_BINARY 214	/* update binary */
#define SIM_COMMAND_UPDATE_RECORD 220	/* update record */
#define SIM_COMMAND_STATUS 242			/* status */
#define SIM_COMMAND_RETRIEVE_DATA 203	/* retrieve data */
#define SIM_COMMAND_SET_DATA 219		/* set data */

#define SIM_RES_SW1_NORMAL_ENDING_OF_COMMAND 0x90
#define SIM_RES_SW1_NORMAL_ENDING_OF_COMMAND_WITH_DATA 0x9F
#define SIM_RES_SW2_NORMAL_ENDING_OF_COMMAND 0x00

#define SIM_RES_SW1_FILE_NOT_FOUND 0x94
#define SIM_RES_SW2_FILE_NOT_FOUND 0x04

#define OFFSET_OF_SIM_RESP_MSG_FILE_ID 4
#define OFFSET_OF_SIM_RESP_MSG_RECORD_LEN 14

#define OFFSET_OF_SIM_RESP_MSG_FILE_TYPE 6
#define OFFSET_OF_SIM_RESP_MSG_EF_STRUCTURE_TYPE 13

#define SIM_RESP_MSG_FILE_TYPE_EF 4
#define SIM_RESP_MSG_EF_STRUCTURE_TYPE_TRANSPARENT 0

#define RES_SIZE_OF_LEN 1
#define RES_OFFSET_OF_LEN 0

#define MODEM_COM_PORT 9
#define MAX_SIM_DATA_LEN 300
#define MAX_AT_COMMAND_LEN BTHAL_COM_MAX_READ_BUFF

#define MAX_SIM_PATH_DEPTH 4
#define MAX_PATH_ID_SIZE MAX_SIM_PATH_DEPTH * sizeof(U16)
#define MAX_SIM_RESPONSES 10

#define SIZE_OF_OK 2
#define SIZE_OF_ERROR 5

#define END_STR_DELIMITER 0xd
#define LAST_END_STR_DELIMITER 0xa

/* features set for interoperability with the various SIM Application */
#define CELLCOM_SIM_CARD
//#define ORANGE_SIM_CARD

#ifdef CELLCOM_SIM_CARD

#define USE_SIM_APPLICATION_REQUESTED_LENGTH

#else

#ifdef ORANGE_SIM_CARD
#define DELETE_RECORD_SIZE_FROM_EF_RESPONSE 

#endif

#endif

/* SIM related AT commands */

typedef U16 AtCommandResponse;

#define AT_RES_OK 0x0a00
#define AT_RES_ERROR 0x0a01
#define AT_RES_UNKNOWN 0x0a02
#define AT_RES_SIM_ERROR 0x0a03
#define AT_RES_PARSING_ERROR 0x0a04
#define AT_RES_CRSM_RESPONSE 0x0a05
#define AT_RES_NO_RESOURCE_ERROR 0x0a06

#define CHAR_TO_HEX(c) (U8)(((c)>='0'&&(c)<='9')?((c)-'0'):\
							(((c)>='A'&&(c)<='F')?((c)-'A'+10):\
							(((c)>='a'&&(c)<='f')?((c)-'a'+10):(0xFF)))) 
							
#define TWO_CHARS_TO_HEX(c1,c2) (U8)((CHAR_TO_HEX((c1)) << 4) | CHAR_TO_HEX((c2)))

#define HEX_TO_CHAR(c) (U8)(((c)>=0 && (c)<=9)?((c)+'0'):\
							(((c)>=0xa && (c)<=0xf)?((c)+'A'-0xa):(0xFF)))

static U16   CurrentDirId;
static U16   CurrentFileId;

/* ATR response */

static U8 Atr[] = {
    0x3F, /* Initial character TS */
    0x00, /* Format character T=0 */
};

typedef struct _SimCardCommand {
	U8  command;
	U16 fileId;
	U8  p1;
	U8  p2;
	U8  len;
	U8	pathId[MAX_PATH_ID_SIZE];
	U8	data[MAX_SIM_DATA_LEN];
} SimCardCommand;

/* SIM Card HAL globals */
static U8  ApduResult[2];
static U8 *ApduBuffer;
static U16 ApduBufferLen = 0;

/* response stack - suppose to be FIFO but only one response is currently active */
static U8 ResponseDataStringArray[MAX_SIM_RESPONSES][MAX_AT_COMMAND_LEN];
static U8 numResponses = 0;

static U8 isResponseCommand = FALSE;

static const char *CrdSimCommandName(U8 command);

U8* CrdInsertResponse()
{	
	if (MAX_SIM_RESPONSES == numResponses) 
	{		
		return NULL;
	}

	return ResponseDataStringArray[numResponses++];
}

U8* CrdGetResponse()
{	
	if (!numResponses)
		return NULL;
	
	return ResponseDataStringArray[--numResponses];
}

U8 CrdNumOfResponses()
{
	return numResponses;
}

/* Initialize the SIM card emulation */
void CRD_Init(void)
{
	Report(("CARD: Init SIM card reader\n"));
	/* Open communication port to the Modem */
	if (BTHAL_FALSE == COM_OpenPort(MODEM_COM_PORT))
	{
		Report(("CARD: Error opening COM port %d\n", MODEM_COM_PORT));
 		return;
	}
}

/* Deinitialize the SIM card emulation */
void CRD_Deinit(void)
{
	Report(("CARD: Deinit SIM card reader\n"));
	if (BTHAL_FALSE == COM_ClosePort())
	{
		Report(("CARD: Error closing COM port %d\n", MODEM_COM_PORT));
	}
}

/* Return the Sim Card ATR response */
void CRD_GetAtr(U8 **AtrPtr, U16 *Len)
{
    *AtrPtr = Atr;
    *Len = sizeof(Atr);
}

/* Parses an incoming PDU */
void CRD_ParseAPDU(U8 *ApduPtr, U16 Len, U8 **RspApduPtr, U16 *RspLen)
{
	Report(("CARD: Operation: %02X\n", ApduPtr[1]));
    switch (ApduPtr[1]) {
    case INS_SELECT:
        CrdSelect(ApduPtr, Len);
        break;
    case INS_STATUS:
        CrdStatus(ApduPtr, Len);
        break;
    case INS_READ_BINARY:
        CrdReadBinary(ApduPtr, Len);
        break;
    case INS_UPDATE_BINARY:
        CrdUpdateBinary(ApduPtr, Len);
        break;
    case INS_READ_RECORD:
        CrdReadRecord(ApduPtr, Len);
        break;
    case INS_UPDATE_RECORD:
        CrdUpdateRecord(ApduPtr, Len);
        break;
    case INS_SEEK:
        CrdSeek(ApduPtr, Len);
        break;
    case INS_INCREASE:
        CrdIncrease(ApduPtr, Len);
        break;
    case INS_VERIFY_CHV:
        CrdVerifyChv(ApduPtr, Len);
        break;
    case INS_CHANGE_CHV:
        CrdChangeChv(ApduPtr, Len);
        break;
    case INS_DISABLE_CHV:
        CrdDisableChv(ApduPtr, Len);
        break;
    case INS_ENABLE_CHV:
        CrdEnableChv(ApduPtr, Len);
        break;
    case INS_UNBLOCK_CHV:
        CrdUnblockChv(ApduPtr, Len);
        break;
    case INS_INVALIDATE:
        CrdInvalidate(ApduPtr, Len);
        break;
    case INS_GSM_ALGORITHM:
        CrdGsmAlgorithm(ApduPtr, Len);
        break;
    case INS_SLEEP:
        CrdSleep(ApduPtr, Len);
        break;
    case INS_RESPONSE:
        CrdResponse(ApduPtr, Len);
        break;
    case INS_TERMINAL_PROFILE:
        CrdTerminalProfile(ApduPtr, Len);
        break;
    case INS_ENVELOPE:
        CrdEnvelope(ApduPtr, Len);
        break;
    case INS_FETCH:
        CrdFetch(ApduPtr, Len);
        break;
    case INS_TERMINAL_RESPONSE:
        CrdTerminalResponse(ApduPtr, Len);
        break;
    default:
        ApduBuffer = ApduResult;
        StoreBE16(ApduBuffer, STATUS_INVALID_INS);
        ApduBufferLen = 2;
        break;
    }

    /* Set the Apdu */
    *RspApduPtr = ApduBuffer;
    *RspLen = ApduBufferLen;
}

/* encode SIM card command to AT Command in Neptune modem terminal format */
BtStatus CrdEncodeSIMCommandToATCommand(SimCardCommand* command, U8* outgoingAtCommand)
{
	U16 iStr = 0;
	U8	dataStr[MAX_SIM_DATA_LEN*2]={0};
	if (!command)
		return BTHAL_STATUS_FAILED;
	
	Report(("CARD: Encoding %s\n", CrdSimCommandName(command->command)));
	
	switch (command->command)
	{		
	case SIM_COMMAND_READ_BINARY:
		/* read binary */		
		break;
	case SIM_COMMAND_READ_RECORD:
		/* read record */
		break;

	case SIM_COMMAND_GET_RESPONSE:		
		/* get response */
		break;

	case SIM_COMMAND_UPDATE_BINARY:	
	case SIM_COMMAND_UPDATE_RECORD:

		for (iStr=0; iStr<command->len; iStr++)
		{
			dataStr[iStr*2] = HEX_TO_CHAR((command->data[iStr] >> 4) & 0x0f);
			dataStr[iStr*2+1] = HEX_TO_CHAR(command->data[iStr] & 0x0f);
		}
		dataStr[iStr*2] = 0;

		break;

	case SIM_COMMAND_STATUS:
		/* status */
		break;

	case SIM_COMMAND_RETRIEVE_DATA:
		  /* retrieve data */
		break;

	case SIM_COMMAND_SET_DATA:
		  /* set data */
		break;

	default:
		Report(("CARD: Encoder unknown SIM Command %02X\n", command->command));
		break;
	}

	sprintf((char*)outgoingAtCommand, "AT+CRSM=%d,%d,%d,%d,%d,%s\0",
		command->command,
		command->fileId,
		command->p1,
		command->p2,
		command->len,
		dataStr);

	Report(("CARD: Encoder, encoded string = %s\n", outgoingAtCommand));
	return BTHAL_STATUS_SUCCESS;	
}

/* parse Neptune modem AT Command */
AtCommandResponse CrdParseATCommand(U8* atString, U16 len)
{
	U8 sw1 = 0;
	U8 sw2 = 0;
	U16 dataLen = 0;
	U16 iStr = 0;
	U16 fileId = 0;
	U8 fieldsParsed = 0;
	U8* responoseStr = NULL;
	U8 dataStr[MAX_AT_COMMAND_LEN];

	Report(("CARD: parseATCommand, strLen = %d, string = %s\n", len, atString));
	
	if (SIZE_OF_OK == len)
		if (!strncmp((char*)atString, "OK", SIZE_OF_OK))
			return AT_RES_OK;

	if (SIZE_OF_ERROR == len)
		if (!strncmp((char*)atString, "ERROR", SIZE_OF_ERROR))
		{
			/* ERROR may be sent by the Neptune modem and the response may come right after
			    therfore the ERROR has no meaning */	
#if 0		

			/* response with an ERROR value */
			Report(("CARD: CrdParseATCommand Modem error, sending sw = 0x6F00\n"));

			responoseStr = CrdInsertResponse();
			if (!responoseStr || CrdNumOfResponses() > 1)
			{
				Report(("CARD: CrdParseATCommand resource error, cannot insert response\n"));
				return AT_RES_NO_RESOURCE_ERROR;
			}

			iStr = 0;
			responoseStr[1+iStr++] = 0x6F;
			responoseStr[1+iStr++] = 0x00;

			/* save the length of the response */
			responoseStr[0] = iStr;
			return AT_RES_CRSM_RESPONSE;
#endif
			return AT_RES_ERROR;
		}

	if (len > SIZE_OF_ERROR)
		if (!strncmp((char*)atString, "+CRSM:", 6))
		{	
			responoseStr = CrdInsertResponse();
			if (!responoseStr || CrdNumOfResponses() > 1)
			{
				Report(("CARD: CrdParseATCommand resource error, cannot insert response\n"));
				return AT_RES_NO_RESOURCE_ERROR;
			}

			fieldsParsed = (U8)sscanf((char*)atString,"+CRSM: %d,%d,%s", &sw1, &sw2, &dataStr);			

			if (3 != fieldsParsed)
			{
				if (2 == fieldsParsed)
				{
				
					if (0 == sw1 && 0 == sw2)
					{
						sw1 = SIM_RES_SW1_FILE_NOT_FOUND;
						sw2 = SIM_RES_SW2_FILE_NOT_FOUND;
					}
					iStr = 0;
					responoseStr[RES_SIZE_OF_LEN+iStr++] = sw1;
					responoseStr[RES_SIZE_OF_LEN+iStr++] = sw2;

					/* save the length of the response */
					responoseStr[RES_OFFSET_OF_LEN] = (U8)iStr;
					return AT_RES_CRSM_RESPONSE;
					
				}	
				else 
				{
					/* restore the response string back to the table */
					CrdGetResponse();
					Report(("CARD: CrdParseATCommand parsing error str = %s\n", atString));
					return AT_RES_PARSING_ERROR;
				}
			}

			if (0 == sw1 && 0 == sw2)
			{
				sw1 = SIM_RES_SW1_FILE_NOT_FOUND;
				sw2 = SIM_RES_SW2_FILE_NOT_FOUND;		
				iStr = 0;
				responoseStr[RES_SIZE_OF_LEN+iStr++] = sw1;
				responoseStr[RES_SIZE_OF_LEN+iStr++] = sw2;

				/* save the length of the response */
				responoseStr[RES_OFFSET_OF_LEN] = (U8)iStr;
				return AT_RES_CRSM_RESPONSE;
			}
			
			/* check if SIM response is OK */
			if (!(SIM_RES_SW1_NORMAL_ENDING_OF_COMMAND == sw1 || 
				SIM_RES_SW1_NORMAL_ENDING_OF_COMMAND_WITH_DATA == sw1))
			{
				Report(("CARD: CrdParseATCommand SIM error, sw = %02X%02X\n", sw1, sw2));
				return AT_RES_SIM_ERROR;
			}		

			dataLen = (U16)strlen((char*)dataStr);
			for(iStr=0; iStr<dataLen; iStr += 2)
			{
				responoseStr[RES_SIZE_OF_LEN+iStr/2] = TWO_CHARS_TO_HEX(dataStr[iStr], dataStr[iStr+1]);
			}	

			iStr = (U16)(iStr/2);
			responoseStr[RES_SIZE_OF_LEN+iStr++] = sw1;
			responoseStr[RES_SIZE_OF_LEN+iStr++] = sw2;

			/* save the length of the response */
			responoseStr[RES_OFFSET_OF_LEN] = (U8)iStr;

			if (isResponseCommand)
			{
				fileId = (U16)(((U16)responoseStr[OFFSET_OF_SIM_RESP_MSG_FILE_ID+RES_SIZE_OF_LEN]) << 8 |
						  	responoseStr[OFFSET_OF_SIM_RESP_MSG_FILE_ID+1+RES_SIZE_OF_LEN]);
					
				/* check if the fileId in the response match the current file Id (check if the file exists) */				
				if (fileId != CurrentFileId)
				{
					/* mark as file not found */
					sw1 = SIM_RES_SW1_FILE_NOT_FOUND;
					sw2 = SIM_RES_SW2_FILE_NOT_FOUND;
					iStr = 0;
					responoseStr[RES_SIZE_OF_LEN+iStr++] = sw1;
					responoseStr[RES_SIZE_OF_LEN+iStr++] = sw2;

					/* save the length of the response */
					responoseStr[RES_OFFSET_OF_LEN] = (U8)iStr;
				}
				else {
#ifdef DELETE_RECORD_SIZE_FROM_EF_RESPONSE  					
					/* eliminate the extra length of record in EF Fixed transparent files */ 
					if (SIM_RESP_MSG_FILE_TYPE_EF == responoseStr[OFFSET_OF_SIM_RESP_MSG_FILE_TYPE+RES_SIZE_OF_LEN] && 
					     SIM_RESP_MSG_EF_STRUCTURE_TYPE_TRANSPARENT == responoseStr[OFFSET_OF_SIM_RESP_MSG_EF_STRUCTURE_TYPE+RES_SIZE_OF_LEN])
					{				
						/* delete the record len field byte */
						responoseStr[15] = responoseStr[16];
						responoseStr[16] = responoseStr[17];
						responoseStr[RES_OFFSET_OF_LEN]--;
					}
#endif					
				}
			}
				
			return AT_RES_CRSM_RESPONSE;
		}
		
	return AT_RES_UNKNOWN;
}

BtStatus CrdAtCommandSIMDecoder(U8* incomingAtCommand, U16 len)
{
	U16 iBuff;
	U16 iStr = 0;
	BOOL startString = TRUE;
	U8 tempBuffer[BTHAL_COM_MAX_READ_BUFF];

	if (!incomingAtCommand)
	{
		return BTHAL_STATUS_FAILED;
	}

	
	for (iBuff=0; iBuff<len; iBuff++)
	{
		/* search for end string delimiter */
		if (incomingAtCommand[iBuff] == END_STR_DELIMITER) 
		{
			if (startString)
			{
				startString = FALSE;
				tempBuffer[iStr] = 0;				
				CrdParseATCommand(tempBuffer, iStr);

				iStr = 0;
			}			
			continue;
		}

		/* search for last end string delimiter */
		if (incomingAtCommand[iBuff] == LAST_END_STR_DELIMITER) 
		{
			startString = TRUE;
			continue;
		}

		if (startString)
		{
			tempBuffer[iStr++] = incomingAtCommand[iBuff];
		}
	}
	return BTHAL_STATUS_SUCCESS;
}

void CrdSendAtCommandToModem(SimCardCommand* command)
{
	U8* responseBuffer = NULL;
	U32 bytesRead = 0;
	U8 inAtString[MAX_AT_COMMAND_LEN];
	U8 outAtString[MAX_AT_COMMAND_LEN];	

	CrdEncodeSIMCommandToATCommand(command, outAtString);
	if (BTHAL_STATUS_SUCCESS != COM_WritePort(outAtString, strlen((char*)outAtString))) {
		Report(("CARD: CrdSendAtCommandToModem, Error writing to port !!!!\n"));
		return;
	}

	Report(("CARD: CrdSendAtCommandToModem, finish writing to port\n"));
	/* synchronic port communication - wait for neptune to response */
	Sleep(100);
	
	if (BTHAL_STATUS_SUCCESS == COM_ReadPort(inAtString, &bytesRead))
	{
		Report(("CARD: CrdSendAtCommandToModem, finished reading, ResLen=%d, Res = %s\n",bytesRead, inAtString));	
		CrdAtCommandSIMDecoder(inAtString, (U16)bytesRead);

		Report(("CARD: CrdSendAtCommandToModem, finished decoding, Found %d commands\n", CrdNumOfResponses()));
		if (CrdNumOfResponses())
		{
			responseBuffer = CrdGetResponse();

			/* get the length of the response */

			/* the first byte of the response is the length */
			ApduBufferLen = responseBuffer[RES_OFFSET_OF_LEN];			
			ApduBuffer = responseBuffer+RES_SIZE_OF_LEN;			
		}
		else {
			Report(("CARD: CrdSendAtCommandToModem, no valid response from port !!!!\n"));
		}
	}
	else {
		Report(("CARD: CrdSendAtCommandToModem, Error reading from port !!!!\n"));
	}
	Report(("CARD: CrdSendAtCommandToModem, finish sending ATCommand to Modem\n"));
}



/* Card Select Instruction */
void CrdSelect(U8 *ApduPtr, U16 Len) 
{
	SimCardCommand command;
	U16 fileId = BEtoHost16(&ApduPtr[5]);
	U8  fileName[5];

	UNUSED_PARAMETER(Len);
	
	isResponseCommand = TRUE;

    	/* Convert the file ID to a name */
    	itoa(fileId, (char*)fileName, 16);

	Report(("CARD: CrdSelect, Request for fileId %04X\n", fileId));

     	CurrentDirId = fileId;
     	CurrentFileId = fileId;

	/* the modem does not support select command, so Get Response command is sent insted */
	command.command = SIM_COMMAND_GET_RESPONSE;
	command.fileId = CurrentFileId;
	command.p1 = 0;
	command.p2 = 0;
	command.len = 0; 
	
	CrdSendAtCommandToModem(&command);

	isResponseCommand = FALSE;

}

/* Card Status Instruction */
void CrdStatus(U8 *ApduPtr, U16 Len) 
{
	U16 readLen = ApduPtr[4];
	SimCardCommand command;

	UNUSED_PARAMETER(Len);

	Report(("CARD: CrdStatus, length from status request = %d\n", readLen));
    	if (readLen == 0) 
	{
		readLen = 256;
    	}

	command.command = SIM_COMMAND_STATUS;
	command.fileId = 0;
	command.p1 = 0;
	command.p2 = 0;

#ifdef USE_SIM_APPLICATION_REQUESTED_LENGTH
	command.len = (U8)readLen;
#else
	command.len = 26;  
#endif

	CrdSendAtCommandToModem(&command);
}

/* Read Binary Instruction */
void CrdReadBinary(U8 *ApduPtr, U16 Len) 
{
	U16	offset = BEtoHost16(&ApduPtr[2]);
	U16 readLen = ApduPtr[4];
	SimCardCommand command;

	UNUSED_PARAMETER(Len);

	Report(("CARD: CrdReadBinary, offset = %d, len = %d\n", offset, readLen));
    	if (readLen == 0) 
	{
		readLen = 256;
    	}

	command.command = SIM_COMMAND_READ_BINARY;
	command.fileId = CurrentFileId;
	command.p1 = (U8)(offset >> 8);
	command.p2 = (U8)(offset & 0x00FF);
	command.len = (U8)readLen;

	CrdSendAtCommandToModem(&command);
}

/* Update Binary Instruction */
void CrdUpdateBinary(U8 *ApduPtr, U16 Len) 
{
	U16	offset = BEtoHost16(&ApduPtr[2]);
    	U8	writeLen = ApduPtr[4];
	SimCardCommand command;

	UNUSED_PARAMETER(Len);

	Report(("CARD: CrdUpdateBinary, offset = %d, len = %d\n", offset, writeLen));

	command.command = SIM_COMMAND_UPDATE_BINARY;
	command.fileId = CurrentFileId;
	command.p1 = (U8)(offset >> 8);
	command.p2 = (U8)(offset & 0x00FF);
	command.len = writeLen;
	memcpy(command.data, ApduPtr+5, command.len);

	CrdSendAtCommandToModem(&command);
}

/* Read Record Instruction */
void CrdReadRecord(U8 *ApduPtr, U16 Len) 
{
	U8 recNum = ApduPtr[2];
	U8 mode = ApduPtr[3];
	U8 recordLen = ApduPtr[4];
	SimCardCommand command;

	UNUSED_PARAMETER(Len);

	Report(("CARD: CrdReadRecord, Rec No. = %d, mode = %d, record len = % d\n", recNum, mode, recordLen));

	command.command = SIM_COMMAND_READ_RECORD;
	command.fileId = CurrentFileId;
	command.p1 = recNum;
	command.p2 = mode;
	command.len = (U8)recordLen;

	CrdSendAtCommandToModem(&command);
}

/* Update Record Instruction */
void CrdUpdateRecord(U8 *ApduPtr, U16 Len) 
{
	U8 recNum = ApduPtr[2];
	U8 mode = ApduPtr[3];
	U8 recordLen = ApduPtr[4];
	SimCardCommand command;

	UNUSED_PARAMETER(Len);

	Report(("CARD: CrdUpdateRecord, Rec No. = %d, mode = %d, record len = % d\n", recNum, mode, recordLen));

	command.command = SIM_COMMAND_UPDATE_RECORD;
	command.fileId = CurrentFileId;
	command.p1 = recNum;
	command.p2 = mode;
	command.len = recordLen;
	memcpy(command.data, ApduPtr+5, command.len);

	CrdSendAtCommandToModem(&command);
}

/* Get Response Instruction */
void CrdResponse(U8 *ApduPtr, U16 Len) 
{
	U16 readLen = ApduPtr[4];
	SimCardCommand command;

	isResponseCommand = TRUE;

	UNUSED_PARAMETER(Len);


#if 0
	/* handles multiple responses in the response queue - not supported now */
	if (CrdNumOfResponses())
	{
	
		Report(("CARD: CrdResponse, Response from buffer, length from GetResponse request = %d\n", readLen));
		responseBuffer = CrdGetResponse();

		/* get the length of the response */
		ApduBufferLen = responseBuffer[0];			
		ApduBuffer = responseBuffer+1;			
	}
	else
#endif		
	{
		Report(("CARD: CrdResponse, length from status request = %d\n", readLen));
		if (readLen == 0) readLen = 256;

		command.command = SIM_COMMAND_GET_RESPONSE;
		command.fileId = CurrentFileId;
		command.p1 = 0;
		command.p2 = 0;
		
		/* this should be the correct form by the spec, a patch is made in order to support Neptune modem */
/*		command.len = (U8)readLen;*/
		command.len = 0; 
		
		CrdSendAtCommandToModem(&command);		
	}

	isResponseCommand = FALSE;

}

void CrdSeek(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);
		
    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdIncrease(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);
	
    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdVerifyChv(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_NORMAL); //always accept a PIN
//    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdChangeChv(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdDisableChv(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdEnableChv(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdUnblockChv(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdInvalidate(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdGsmAlgorithm(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdSleep(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdTerminalProfile(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdEnvelope(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdFetch(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

void CrdTerminalResponse(U8 *ApduPtr, U16 Len) 
{
	UNUSED_PARAMETER(Len);
	UNUSED_PARAMETER(ApduPtr);

    StoreBE16(ApduBuffer, STATUS_INVALID_INS);
    ApduBufferLen = 2;
}

const char *CrdSimCommandName(U8 command)
{
    switch (command)
    {
        case SIM_COMMAND_READ_BINARY:
            return "SIM_COMMAND_READ_BINARY";

        case SIM_COMMAND_READ_RECORD:
            return "SIM_COMMAND_READ_RECORD";

        case SIM_COMMAND_GET_RESPONSE:
            return "SIM_COMMAND_GET_RESPONSE";

        case SIM_COMMAND_UPDATE_BINARY:
            return "SIM_COMMAND_UPDATE_BINARY";

        case SIM_COMMAND_UPDATE_RECORD:
            return "SIM_COMMAND_UPDATE_RECORD";

        case SIM_COMMAND_STATUS:
            return "SIM_COMMAND_STATUS";

        case SIM_COMMAND_RETRIEVE_DATA:
            return "SIM_COMMAND_RETRIEVE_DATA";

        case SIM_COMMAND_SET_DATA:
            return "SIM_COMMAND_SET_DATA";

        default:
            return "Unknown SIM Command name";
    }
}



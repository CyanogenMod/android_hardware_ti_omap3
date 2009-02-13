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
*   FILE NAME:      com.c
*
*   DESCRIPTION:   This file contain interface implementation to com port in WIN
*
*   AUTHOR:         Yuval Hevroni
*
\*******************************************************************************/


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include <time.h>
#include <stdio.h>
#include "osapi.h"
#include "com.h"
#include "bthal_log.h"
#include "bthal_os.h"

#define ASCII_XON       0x11
#define ASCII_XOFF      0x13

#define COM_READ_INTERVAL_TIMEOUT 50

static BOOL bthalIsComOpen = FALSE;
static HANDLE bthalComPortHandle; 

/* Number of bytes written in last write operation */
static BTHAL_U32 bthalLastWriteLen;
		    
/* Open communication port, set the CommState and watch thread */
BTHAL_BOOL COM_OpenPort(BTHAL_U8 commPortNumber)
{
	COMMTIMEOUTS	CommTimeOuts;
	DCB				dcb ;
	char PortStr[10];						  

	if (BTHAL_TRUE == bthalIsComOpen)
	{
		return BTHAL_FALSE;
	}

	/* Set COM Port number */
	sprintf(PortStr,"\\\\.\\COM%d",commPortNumber);

	// open COM Port device
	bthalComPortHandle = CreateFile(PortStr, 
								 GENERIC_READ | GENERIC_WRITE,
								 0,                    /* exclusive access */
								 NULL,                 /* no security attrs */
								 OPEN_EXISTING,
								 FILE_ATTRIBUTE_NORMAL ,
								 NULL);

	if (bthalComPortHandle == INVALID_HANDLE_VALUE)
	{
		return BTHAL_FALSE;
	}

	/* get any early notifications */
	SetCommMask(bthalComPortHandle, EV_RXCHAR | EV_TXEMPTY) ;

	/* setup device buffers */
	SetupComm(bthalComPortHandle , 8192, 8192 ) ;

    /* Purge any information in the buffer */
    PurgeComm(bthalComPortHandle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);

	/* set up for overlapped non-blocking I/O */
	GetCommTimeouts(bthalComPortHandle, &CommTimeOuts);
	
    CommTimeOuts.ReadIntervalTimeout = COM_READ_INTERVAL_TIMEOUT;
    CommTimeOuts.ReadTotalTimeoutMultiplier = 0 ;
    CommTimeOuts.ReadTotalTimeoutConstant = 0 ;
    CommTimeOuts.WriteTotalTimeoutMultiplier = 0 ;
    /* If a write operation takes more than 3 seconds, it's probably
     * an error.
     */
    CommTimeOuts.WriteTotalTimeoutConstant = 5000; 
    SetCommTimeouts(bthalComPortHandle, &CommTimeOuts);

	/* get Initial port settings */
	if (!GetCommState(bthalComPortHandle, &dcb))
	{
		CloseHandle(bthalComPortHandle);
		return BTHAL_FALSE;
	}
	
	//Sets up the DCB for communication settings   	 
	dcb.DCBlength = sizeof(DCB);

    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
	dcb.BaudRate = 460800;

    dcb.fBinary = TRUE;
    dcb.fParity = TRUE;
    dcb.fDsrSensitivity = FALSE;

/*
    dcb.fOutxDsrFlow = 0;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = 1;
    dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
    dcb.XonLim = 100;
    dcb.XoffLim = 100;

    // Set the software flow control
    dcb.fInX = dcb.fOutX = 0;
    dcb.XonChar = ASCII_XON;
    dcb.XoffChar = ASCII_XOFF;

    // Set other stuff 

    dcb.fNull = 0;
    dcb.fAbortOnError = 0;
*/
	if (!SetCommState(bthalComPortHandle, &dcb))
	{
		CloseHandle(bthalComPortHandle);
		return BTHAL_FALSE;
	}

	bthalIsComOpen = TRUE;

    /* assert DTR */
    EscapeCommFunction(bthalComPortHandle, SETDTR); 

	bthalLastWriteLen = 0;

	return BTHAL_TRUE;
}

/* Closes the connection to the port.  Resets the connect flag */
BTHAL_BOOL COM_ClosePort()
{
	if (BTHAL_FALSE == bthalIsComOpen)
	{
		return BTHAL_FALSE;
	}
	bthalIsComOpen = FALSE;

	/* disable event notification and */
	SetCommMask(bthalComPortHandle, 0);

	/* drop DTR */
	EscapeCommFunction(bthalComPortHandle, CLRDTR);

	/* purge any outstanding reads/writes and close device handle */
	PurgeComm(bthalComPortHandle, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
	CloseHandle(bthalComPortHandle);

	bthalLastWriteLen = 0;
	bthalComPortHandle = NULL;

	return BTHAL_TRUE;
} 

BthalStatus COM_ReadPort(BTHAL_U8 readBuffer[], BTHAL_U32 *readBytes)
{
    BTHAL_U32 inReadBytes;
	BTHAL_U32 secondInReadBytes;
	BTHAL_U8  buffer[BTHAL_COM_MAX_READ_BUFF];
	S32 bytesRead = 0;
	BOOL sts;

	do {
	sts = ReadFile(bthalComPortHandle, buffer, BTHAL_COM_MAX_READ_BUFF, &inReadBytes, 0);
	Report(("CARD: BTHAL_COM_ReadPort, ReadFile returned , sts = %d, inReadBytes = %d, bthalLastWriteLen = %d!!!\n", sts, inReadBytes, bthalLastWriteLen));
	} while (!inReadBytes && sts);

	if (TRUE == sts)
	{
		bytesRead = inReadBytes - bthalLastWriteLen;
		
		if (2 > bytesRead)
		{
			if (!ReadFile(bthalComPortHandle, buffer+inReadBytes+2, BTHAL_COM_MAX_READ_BUFF-inReadBytes, &secondInReadBytes, 0))
			{
				Report(("CARD: BTHAL_COM_ReadPort, ReadFile Failed !!!\n"));
				return BTHAL_STATUS_SUCCESS;
			}
			bytesRead += secondInReadBytes;
			Report(("CARD: BTHAL_COM_ReadPort, Second ReadFile returned , secondInReadBytes = %d, bytesRead = %d!!!\n", secondInReadBytes, bytesRead));
			
//			*readBytes = 0; 
		}
		*readBytes = (U32)bytesRead;

		memcpy(readBuffer, buffer+bthalLastWriteLen, *readBytes);
		bthalLastWriteLen = 0;
		return BTHAL_STATUS_SUCCESS;
	}

	return BTHAL_STATUS_FAILED;
} 

BthalStatus COM_WritePort(const BTHAL_U8 *buffer, BTHAL_U32 length)
{
    BTHAL_U32 writtenBytes;
	BOOL sts;

	sts = WriteFile(bthalComPortHandle, buffer, length, &writtenBytes, 0);

	bthalLastWriteLen = writtenBytes;
	/* Write finished */
	if (TRUE == sts)
	{
		//bthalLastWriteLen = writtenBytes;
		/* send \r character */
		sts = WriteFile(bthalComPortHandle, "\r\n", 2, &writtenBytes, 0);
		return (BthalStatus)(TRUE == sts ? BTHAL_STATUS_SUCCESS : BTHAL_STATUS_FAILED);
	}	 

	return BTHAL_STATUS_SUCCESS;
}


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
*   FILE NAME:      bthal_pb.c
*
*   DESCRIPTION:    This file uses a sample phonebook implementation on windonws platform
*					for implementing the bthal_pb.h API, providing basic phonebook funcitonality
*					such as getting phonebook entries data. The sample phonebook can be 
*					configured using the PBAP sapmle application.
*
*   AUTHOR:         Yoni Shavit
*
\*******************************************************************************/

#include "bthal_pb.h"
#include "pb_simulator.h"
#include "bthal_common.h"
#include "osapi.h"
#include "string.h"

/* The following MACRO is used to avoid warngings of unreferenced const paramters */
#define UNUSED_CONST_PARAMETER(param)  if (param==param);

#define PB_SIMULOTOR_ASYNCHRONOUS TRUE
#include "btl_defs.h"

/****************************************************************************
 *
 * Section: Types
 *
 ***************************************************************************/

/* Forward declarations */
typedef struct _PbEventParms    PbEventParms;

/********************************************************************************
 *
 * Data Structures
 *
 *******************************************************************************/
 
/*---------------------------------------------------------------------------
 * PbEventParms structure
 *
 * Contains information phonebook event.
 */
struct _PbEventParms {

    /* BTHAL MM event */
    BthalPbEvent      event;

    /* Callback parameter object. Holds pointer to data that was filled in the BTHAL_PB. The BTHAL_PB got the 
     * pointers before as parameters in one of the API functions 
     */
    union {
		/* Used in the BTHAL_PB_EVENT_PB_OPENED */
		struct 
		{	
			/* The hpnebook type */
			BthalPbPhonebookType phonebookType;

			/* An handle pointer for the phonebook that was opened */
			BthalPbHandle *handle;

		} openPb;
		
        /* Used in the BTHAL_PB_EVENT_ENTRIES_NUM and in the BTHAL_PB_EVENT_NEW_MISSED_CALLS_NUM 
		 * and holds the number of entries pointer in both cases */
		BthalPbEntriesNum *entriesNum;

		/* Used in the BTHAL_PB_EVENT_ENTRY_DATA */
		struct 
		{	

			/* Entry ID */
			BthalPbEntryId entryId;
				
			/* Entry foramt */
			BthalPbEntryFormat entryFormat;

			/* Entry VCF filter */
			BthalPbVcardFilter vcardFilter;
			
			/* Pointer to the enrty buffer */
            BTHAL_U8 *vcfEnrtyBuf;

			/* Pointer to the entry lenght */
			BthalPbVcfLen *vcfEntryLen;
			
        } entryData;

		/* Used in the BTHAL_PB_EVENT_ENTRIES_LIST_READY and holds the enries list pointer */ 
		BthalPbEntriesList *entriesList;

		/* Used in the BTHAL_PB_EVENT_ENTRY_NAME */
		struct 
		{	
			/* Entry ID */
			BthalPbEntryId entryId;

			/* Entry Name */
			BthalPbEntryName *entryNameStruct;
			
        } entryNameData;
		
		
    } data;
};
/********************************************************************************
 *
 * Globals
 *
 *******************************************************************************/

extern PbDatabase phonebook;
extern void PbOpenedUICheckBox();

/* Map between the bthal entries Id's and the phonebook entries Id's */
U16 pbIdMap[BTHAL_PB_MAX_ENTRIES_NUM];

/* The phonebook thread of execution */
HANDLE pbThreadHandle;
/* The phonebook event */
HANDLE pbEvent;
DWORD  pbthreadID;

/* Call back params */
PbEventParms pbEventParms;

/* Callback function in the BTL to call when ready */
BthalPbCallBack pbCallbackFunc;


const char VERSION_2_1[] ={ 'V','E','R','S','I','O','N',':','2','.','1'};


/********************************************************************************
 *
 * Internal function prototypes
 *
 *******************************************************************************/
 
static void PB_ThreadFunc(LPVOID param);

/********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/

BthalStatus BTHAL_PB_WasPbChanged(const BthalPbPhonebookType phonebookType, BTHAL_BOOL *pbChanged)
{
	/* We ingonre phonebook type in the sample pb */
	switch (phonebookType)
	{
		default:
			*pbChanged = !( PbGetValidationStatus() );
			PbSetValidationStatus(TRUE);
			return BTHAL_STATUS_SUCCESS;
	}
}


#if PB_SIMULOTOR_ASYNCHRONOUS == TRUE

/****************************************************************************
 *
 *  The Asynchronous BTHAL_PB Sample Implementation 
 *
 ***************************************************************************/




/*-------------------------------------------------------------------------------
 * BTHAL_PB_Init()
 */
BthalStatus BTHAL_PB_Init()
{
	DWORD	dwCreationFlags = 0 ;   /* the thread is created in a running state */
	
	/* Create the phonebook event */
	pbEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	
	/* Create the phonebook thread */
	pbThreadHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PB_ThreadFunc, NULL, dwCreationFlags, &pbthreadID);

	if (pbThreadHandle == NULL)
		return BTHAL_STATUS_FAILED;

	return BTHAL_STATUS_SUCCESS;
	
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_Deinit()
 */
BthalStatus BTHAL_PB_Deinit()
{	
	BOOL retVal;
	
	retVal = TerminateThread(pbThreadHandle, 0);
	if (retVal == 0)
		return BTHAL_STATUS_FAILED;
	
	retVal = CloseHandle(pbThreadHandle);
	if (retVal == 0)
		return BTHAL_STATUS_FAILED;
	
	return BTHAL_STATUS_SUCCESS;

	
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_Register()
 */
BthalStatus BTHAL_PB_Register(const BthalPbCallBack pbCallback)
{	
	pbCallbackFunc = pbCallback;
	/* Asychronous sample phonebook */
	return BTHAL_STATUS_SUCCESS;
}




/*-------------------------------------------------------------------------------
 * BTHAL_PB_OpenPb()
 */
BthalStatus BTHAL_PB_OpenPb(const BthalPbPhonebookType phonebookType, BthalPbHandle *handle)
{
	BOOL retVal;

	pbEventParms.event = BTHAL_PB_EVENT_PB_OPENED;
	pbEventParms.data.openPb.phonebookType = phonebookType;
	pbEventParms.data.openPb.handle = handle;

	/* Set an event fot the pb thread */
	retVal = SetEvent(pbEvent);
	if (retVal == 0)
		return BTHAL_STATUS_FAILED;

	return BTHAL_STATUS_PENDING;
}


/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetPbEntriesNum()
 */
BthalStatus BTHAL_PB_GetPbEntriesNum(const BthalPbHandle handle, BthalPbEntriesNum *entiesNum)
{

	BOOL retVal;

	/* In order to avoid warnings we write the following useless line - */
	if (handle == handle);

    pbEventParms.event = BTHAL_PB_EVENT_ENTRIES_NUM;
	pbEventParms.data.entriesNum = entiesNum;

	/* Set an event fot the pb thread */
	retVal = SetEvent(pbEvent);
	if (retVal == 0)
		return BTHAL_STATUS_FAILED;

	return BTHAL_STATUS_PENDING;

}



/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetNewMissedCallsNum()
 */
BthalStatus BTHAL_PB_GetNewMissedCallsNum(const BthalPbHandle handle, BthalPbEntriesNum *entiesNum)
{	
	BOOL retVal;
	
	/* In order to avoid warnings we write the following useless line - */
	if (handle == handle);

    pbEventParms.event = BTHAL_PB_EVENT_NEW_MISSED_CALLS_NUM;
	pbEventParms.data.entriesNum = entiesNum;

	/* Set an event fot the pb thread */
	retVal = SetEvent(pbEvent);
	if (retVal == 0)
		return BTHAL_STATUS_FAILED;

    return (BTHAL_STATUS_PENDING);
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetEntryData()
 */
BthalStatus BTHAL_PB_GetEntryData(const BthalPbHandle handle,   BthalPbEntryId entryId, 
																BthalPbEntryFormat entryFormat,
													    		BthalPbVcardFilter vcardFilter,
															    BTHAL_U8 *vcfEnrtyBuf, 
																BthalPbVcfLen *vcfEntryLen)
{

	BOOL retVal;

 

	/* In order to avoid warnings we write the following useless line - */
	if (handle == handle);

	if (entryId >= BTHAL_PB_MAX_ENTRIES_NUM)
	{
		return BTHAL_STATUS_FAILED;
	}
	
    pbEventParms.event = BTHAL_PB_EVENT_ENTRY_DATA;

	pbEventParms.data.entryData.entryId = entryId;
	pbEventParms.data.entryData.entryFormat = entryFormat;
	OS_MemCopy((U8 *)&pbEventParms.data.entryData.vcardFilter, (U8 *)&vcardFilter, sizeof(BthalPbVcardFilter));
	pbEventParms.data.entryData.vcfEnrtyBuf = vcfEnrtyBuf;
	pbEventParms.data.entryData.vcfEntryLen = vcfEntryLen;

	/* Set an event fot the pb thread */
	retVal = SetEvent(pbEvent);
	if (retVal == 0)
		return BTHAL_STATUS_FAILED;

    return (BTHAL_STATUS_PENDING);
    
}




/*-------------------------------------------------------------------------------
 * BTHAL_PB_BuildEntriesList()
 */
BthalStatus BTHAL_PB_BuildEntriesList(const BthalPbHandle handle,   BthalPbListSearch listSearch, 
														   			BthalPbListSortingOrder listSortOrder,
																	BthalPbEntriesList *entriesList)
{
	/* In order to avoid warnings we write the following useless lines - */
	if (handle == handle);

	UNUSED_PARAMETER(listSearch);
	UNUSED_PARAMETER(listSortOrder);
	UNUSED_PARAMETER(entriesList);

	/* Sample application currently doesn't support search and sort  */
	return BTHAL_STATUS_NOT_SUPPORTED;
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetListedEntryName()
 */
BthalStatus BTHAL_PB_GetListedEntryName(const BthalPbHandle handle,   BthalPbEntryName *entryNameStruct, 
																 	  BthalPbEntryId entryId)
{
	BOOL retVal;
	
	/* In order to avoid warnings we write the following useless lines - */
	if (handle == handle);

	if (entryId >= BTHAL_PB_MAX_ENTRIES_NUM)
	{
		return BTHAL_STATUS_FAILED;
	}

    pbEventParms.event = BTHAL_PB_EVENT_ENTRY_NAME;
	
	pbEventParms.data.entryNameData.entryNameStruct = entryNameStruct;
	pbEventParms.data.entryNameData.entryId = entryId;

	/* Set an event for the pb thread */
	retVal = SetEvent(pbEvent);
	if (retVal == 0)
		return BTHAL_STATUS_FAILED;


    return (BTHAL_STATUS_PENDING);
    
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_ClosePb()
 */
 BthalStatus BTHAL_PB_ClosePb(BthalPbHandle *handle)
{
	BOOL retVal;
	
	/* In order to avoid warnings we write the following useless lines - */
	if (handle == handle);

    pbEventParms.event = BTHAL_PB_EVENT_PB_CLOSED;
	
	/* Set an event for the pb thread */
	retVal = SetEvent(pbEvent);
	if (retVal == 0)
		return BTHAL_STATUS_FAILED;


    return (BTHAL_STATUS_PENDING);
	
}


/*-------------------------------------------------------------------------------
 * PB_ThreadFunc()
 *
 *	    The main execution function of the phonebook simulator thread.
 *		The main funcitonality of the funciton is Handling the phonebook event, 
 *		that was set due to the PBAP server request.
 *		
 * Parameters:
 *		void
 *
 * Returns:
 *		void
 */

static void PB_ThreadFunc(LPVOID param)
{

	BthalPbCallbackParms callbackParams;
	SyncPhoneBook   	*synchPbRec;
    PbRecord   			*rec;
	PbStatus			pbStatus;
	DWORD 				result;
	U16					i;
	
	UNUSED_PARAMETER(param);

	while (TRUE)
	{
		/* Wait for the phonebook event. Indicates of a PBAP server request */
		result = WaitForSingleObject(pbEvent,INFINITE);
		if (result != WAIT_OBJECT_0)
		{
			callbackParams.event = BTHAL_PB_EVENT_PB_ERROR;
			pbCallbackFunc(&callbackParams);
			break;
		}

		/* Reset the callback parameters */
		OS_MemSet((U8*)&callbackParams, 0, sizeof(BthalPbCallbackParms));

		Sleep(100);
		
		/* Handle the event */	
		switch (pbEventParms.event)
		{
			case BTHAL_PB_EVENT_PB_OPENED:
				PbOpenedUICheckBox(TRUE);
				/* No use for the handle in the sample phonebook */
				
				/* Set phonebook path */
					
				switch (pbEventParms.data.openPb.phonebookType)
				{
					case 5:  	/* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_PHONEBOOK) */
						strcpy(phonebook.currentPath, PB_MAIN_PB);
						break;
					case 9: 	/* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_INCOMING) */	
						strcpy(phonebook.currentPath, PB_MAIN_INCOMING);
						break;
					case 17:     /* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_OUTGOING) */
						strcpy(phonebook.currentPath, PB_MAIN_OUTGOING);
						break;
					case 33:    /* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_MISSED)   */
						strcpy(phonebook.currentPath, PB_MAIN_MISSED);
						break;
					case 65:     /* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_COMBINED) */
						strcpy(phonebook.currentPath, PB_MAIN_COMBINED);
						break;

						
					case 6: 	/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_PHONEBOOK) */
						strcpy(phonebook.currentPath, PB_SIM_PB);
						break;
					case 10:		/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_INCOMING) */
						strcpy(phonebook.currentPath, PB_SIM_INCOMING);
						break;
					case 18: 	/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_OUTGOING) */
						strcpy(phonebook.currentPath, PB_SIM_OUTGOING);
						break;
					case 34:	/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_MISSED)   */
						strcpy(phonebook.currentPath, PB_SIM_MISSED);
						break;
					case 66:		/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_COMBINED) */
						strcpy(phonebook.currentPath, PB_SIM_COMBINED);
						break;
						
					default:
						callbackParams.event = BTHAL_PB_EVENT_PB_ERROR;
						pbCallbackFunc(&callbackParams);
						break;
						
					
				}
				
				/* Call the callback with the correct params */
				callbackParams.event = BTHAL_PB_EVENT_PB_OPENED;
				pbCallbackFunc(&callbackParams);
				break;
				
			case BTHAL_PB_EVENT_ENTRIES_NUM:
				
				*pbEventParms.data.entriesNum = 0;

				/* In this function we build 'BTHAL Ids to phonebook Ids' map, for future pull operation use */
				/* Init the map */
				for (i = 0; i < BTHAL_PB_MAX_ENTRIES_NUM; i++)
					pbIdMap[i] = 0; 
				
			    /* Get the phonebook sizing information, and build the 'BTHAL Ids to phonebook Ids' map  */
			    rec = (PbRecord *)GetHeadList(&phonebook.records);
			    while (rec != (PbRecord *)&phonebook.records) 
				{
			        if (strcmp(rec->path, phonebook.currentPath) == 0) 
				{
						if ((*pbEventParms.data.entriesNum) < BTHAL_PB_MAX_ENTRIES_NUM)
						{
							pbIdMap[*pbEventParms.data.entriesNum] = (U16) rec->uid;
						}
						
			       	(*pbEventParms.data.entriesNum)++;
						
			        }
			        rec = (PbRecord *)GetNextNode(&rec->node);
			    }
				/* Call the callback with the correct params */
				callbackParams.event = BTHAL_PB_EVENT_ENTRIES_NUM;
				callbackParams.data.entriesNum = pbEventParms.data.entriesNum;
				pbCallbackFunc(&callbackParams);
				break;
				
				
			case BTHAL_PB_EVENT_NEW_MISSED_CALLS_NUM:
				/* The phonebook sample doesn't track missed calls. Just provides 3 as an example */
				*pbEventParms.data.entriesNum = 3;
				
				/* Call the callback with the correct params */
				callbackParams.event = BTHAL_PB_EVENT_NEW_MISSED_CALLS_NUM;
				callbackParams.data.entriesNum = pbEventParms.data.entriesNum;
				pbCallbackFunc(&callbackParams);
				break;
				
			case BTHAL_PB_EVENT_ENTRY_DATA:

				if (pbEventParms.data.entryData.entryId < BTHAL_PB_MAX_ENTRIES_NUM)
				{
					rec = getRecByUid((U32) pbIdMap[pbEventParms.data.entryData.entryId]);

					/* Check whether the entry was found */
					if (rec == 0)
					{
						callbackParams.event = BTHAL_PB_EVENT_PB_ERROR;
						pbCallbackFunc(&callbackParams);
						break;
					}
				}
				else
				{
					callbackParams.event = BTHAL_PB_EVENT_PB_ERROR;
					pbCallbackFunc(&callbackParams);					
					break;
				}
								
				/* Sample application currently doesn't support filter  */

				
			    /* Convert data for sending */
			    phonebook.outBuffer = (U8*) pbEventParms.data.entryData.vcfEnrtyBuf;
			    pbStatus = pbConvert2MimeFormat(rec->data, rec->dataLen);
				
				if (pbStatus != PB_STATUS_SUCCESS)
				{
					callbackParams.event = BTHAL_PB_EVENT_PB_ERROR;
					pbCallbackFunc(&callbackParams);
					break;
				}

			
			/* Add support for vCard 3.0 switch between VERSION:2.1 to VERSION:3.0*/
			if (pbEventParms.data.entryData.entryFormat == BTHAL_PB_ENTRY_FORMAT_VCF_3_0)
			{
				char *retval=strstr((const char *)phonebook.outBuffer,VERSION_2_1);
				Assert(retval!=NULL);
				retval[8]='3';
				retval[10]='0';
				
			
			}	
				*pbEventParms.data.entryData.vcfEntryLen = phonebook.outBuffLen;

				/* Call the callback with the correct params */
				callbackParams.event = BTHAL_PB_EVENT_ENTRY_DATA;
				callbackParams.data.entryData.vcfEnrtyBuf = pbEventParms.data.entryData.vcfEnrtyBuf;
				callbackParams.data.entryData.vcfEntryLen = pbEventParms.data.entryData.vcfEntryLen;
				pbCallbackFunc(&callbackParams);
				break;
				
			case BTHAL_PB_EVENT_ENTRY_NAME:
				
				if (pbEventParms.data.entryData.entryId < BTHAL_PB_MAX_ENTRIES_NUM)
				{
					rec = getRecByUid((U32) pbIdMap[pbEventParms.data.entryData.entryId]);

					if (rec == 0)
					{
						callbackParams.event = BTHAL_PB_EVENT_PB_ERROR;
						pbCallbackFunc(&callbackParams);
						break;
					}
				}
				else
				{
					callbackParams.event = BTHAL_PB_EVENT_PB_ERROR;
					pbCallbackFunc(&callbackParams);
					break;
				}
					
				
				synchPbRec = (SyncPhoneBook *)rec->data;

				/* Copy the name of the entry */
				strncpy((char*)pbEventParms.data.entryNameData.entryNameStruct->firstName, (char*)rec->data + synchPbRec->fname.offset, synchPbRec->fname.len);
				pbEventParms.data.entryNameData.entryNameStruct->firstName[synchPbRec->fname.len] = '\0';

				strncpy((char*)pbEventParms.data.entryNameData.entryNameStruct->middleName, (char*)rec->data + synchPbRec->mname.offset, synchPbRec->mname.len);
				pbEventParms.data.entryNameData.entryNameStruct->middleName[synchPbRec->mname.len] = '\0';

				strncpy((char*)pbEventParms.data.entryNameData.entryNameStruct->lastName, (char*)rec->data + synchPbRec->lname.offset, synchPbRec->lname.len);
				pbEventParms.data.entryNameData.entryNameStruct->lastName[synchPbRec->lname.len] = '\0';

				strncpy((char*)pbEventParms.data.entryNameData.entryNameStruct->suffix, (char*)rec->data + synchPbRec->suffix.offset, synchPbRec->suffix.len);
				pbEventParms.data.entryNameData.entryNameStruct->suffix[synchPbRec->suffix.len] = '\0';

				/* Call the callback with the correct params */
				callbackParams.event = BTHAL_PB_EVENT_ENTRY_NAME;
				callbackParams.data.entryNameStruct = pbEventParms.data.entryNameData.entryNameStruct;
				pbCallbackFunc(&callbackParams);
				break;
				
			case BTHAL_PB_EVENT_PB_CLOSED:
				PbOpenedUICheckBox(FALSE);
				callbackParams.event = BTHAL_PB_EVENT_PB_CLOSED;
				pbCallbackFunc(&callbackParams);
				break;

			default:
				callbackParams.event = BTHAL_PB_EVENT_PB_ERROR;
				pbCallbackFunc(&callbackParams);
				break;
		}

		
	}
}
#else /* PB_SIMULOTOR_ASYNCHRONOUS == TRUE */
/****************************************************************************
 *
 *  The Synchronous BTHAL_PB Sample Implementation 
 *
 ***************************************************************************/




/*-------------------------------------------------------------------------------
 * BTHAL_PB_Init()
 */
BthalStatus BTHAL_PB_Init()
{
	return BTHAL_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_Deinit()
 */
BthalStatus BTHAL_PB_Deinit()
{
	return BTHAL_STATUS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_Register()
 */
BthalStatus BTHAL_PB_Register(const BthalPbCallBack pbCallback)
{
	UNUSED_CONST_PARAMETER(pbCallback);

	/* Sychronous sample phonebook */
	return BTHAL_STATUS_NOT_SUPPORTED;
}


/*-------------------------------------------------------------------------------
 * BTHAL_PB_OpenPb()
 */
BthalStatus BTHAL_PB_OpenPb(const BthalPbPhonebookType phonebookType, BthalPbHandle *handle)
{	
	BthalStatus status = BTHAL_STATUS_SUCCESS;

	UNUSED_PARAMETER(handle);

	PbOpenedUICheckBox(TRUE);
	/* No use for the handle in the sample phonebook */
	
	/* Set phonebook path */
		
	switch (phonebookType)
	{
		case 5:  	/* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_PHONEBOOK) */
			strcpy(phonebook.currentPath, PB_MAIN_PB);
			break;
		case 9: 	/* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_INCOMING) */	
			strcpy(phonebook.currentPath, PB_MAIN_INCOMING);
			break;
		case 17:     /* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_OUTGOING) */
			strcpy(phonebook.currentPath, PB_MAIN_OUTGOING);
			break;
		case 33:    /* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_MISSED)   */
			strcpy(phonebook.currentPath, PB_MAIN_MISSED);
			break;
		case 65:     /* (BTHAL_PB_PATH_MAIN + BTHAL_PB_PATH_COMBINED) */
			strcpy(phonebook.currentPath, PB_MAIN_COMBINED);
			break;

			
		case 6: 	/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_PHONEBOOK) */
			strcpy(phonebook.currentPath, PB_SIM_PB);
			break;
		case 10:		/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_INCOMING) */
			strcpy(phonebook.currentPath, PB_SIM_INCOMING);
			break;
		case 18: 	/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_OUTGOING) */
			strcpy(phonebook.currentPath, PB_SIM_OUTGOING);
			break;
		case 34:	/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_MISSED)   */
			strcpy(phonebook.currentPath, PB_SIM_MISSED);
			break;
		case 66:		/* (BTHAL_PB_PATH_SIM + BTHAL_PB_PATH_COMBINED) */
			strcpy(phonebook.currentPath, PB_SIM_COMBINED);
			break;
			
		default:
			status = BTHAL_STATUS_FAILED;
			break;
			
		
	}

	return (status);
}


/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetPbEntriesNum()
 */
BthalStatus BTHAL_PB_GetPbEntriesNum(const BthalPbHandle handle, BthalPbEntriesNum *entiesNum)
{

	BthalStatus 		status = BTHAL_STATUS_SUCCESS;
	PbRecord   		   *rec;
	U16				    i;
	
	UNUSED_CONST_PARAMETER(handle);

    *entiesNum = 0;

	/* In this function we build 'BTHAL Ids to phonebook Ids' map, for future pull operation use */
	/* Init the map */
	for (i = 0; i < BTHAL_PB_MAX_ENTRIES_NUM; i++)
		pbIdMap[i] = 0; 
	
    /* Get the phonebook sizing information, and build the 'BTHAL Ids to phonebook Ids' map  */
    rec = (PbRecord *)GetHeadList(&phonebook.records);
    while (rec != (PbRecord *)&phonebook.records) 
	{
        if (strcmp(rec->path, phonebook.currentPath) == 0) 
	{
		if (*entiesNum < BTHAL_PB_MAX_ENTRIES_NUM)
		{
			pbIdMap[*entiesNum] = (U16) rec->uid;
		}
		
            (*entiesNum)++;
			
        }
        rec = (PbRecord *)GetNextNode(&rec->node);
    }

    return (status);
}


/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetNewMissedCallsNum()
 */
BthalStatus BTHAL_PB_GetNewMissedCallsNum(const BthalPbHandle handle, BthalPbEntriesNum *entiesNum)
{	
	BthalStatus status = BTHAL_STATUS_SUCCESS;
	
	UNUSED_CONST_PARAMETER(handle);

	/* The phonebook sample doesn't track missed calls. Just provides 3 as an example */
	*entiesNum = 3;
		
	return (status);
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetEntryData()
 */
BthalStatus BTHAL_PB_GetEntryData(const BthalPbHandle handle, BthalPbEntryId entryId, 
														BthalPbEntryFormat entryFormat,
														BthalPbVcardFilter vcardFilter,
													    BTHAL_U8 *vcfEnrtyBuf, 
													    BthalPbVcfLen *vcfEntryLen)
{
	BthalStatus status = BTHAL_STATUS_SUCCESS;
	
	PbStatus	pbStatus;
	PbRecord   *rec;

	UNUSED_PARAMETER(vcardFilter);
	UNUSED_CONST_PARAMETER(handle);

	if (entryId >= BTHAL_PB_MAX_ENTRIES_NUM)
	{
		return BTHAL_STATUS_FAILED;
	}
	
	rec = getRecByUid((U32) pbIdMap[entryId]);

	/* Check whether the entry was found */
	if (rec == 0)
		return (BTHAL_STATUS_FAILED);
	
	
	/* Sample application currently doesn't support filter  */

	
    /* Convert data for sending */
    phonebook.outBuffer = (U8*) vcfEnrtyBuf;
    pbStatus = pbConvert2MimeFormat(rec->data, rec->dataLen);
	
	if (pbStatus != PB_STATUS_SUCCESS)
		return (BTHAL_STATUS_FAILED); 

/* Add support for vCard 3.0 switch betweenVERSION:2.1 to VERSION:3.0*/
	if (entryFormat== BTHAL_PB_ENTRY_FORMAT_VCF_3_0)
	{
		char *retval=strstr((const char *)phonebook.outBuffer,VERSION_2_1);
		Assert(retval!=NULL);
		retval[8]='3';
		retval[10]='0';
	}	

	*vcfEntryLen = phonebook.outBuffLen;

	return (status);
    
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_BuildEntriesList()
 */
BthalStatus BTHAL_PB_BuildEntriesList(const BthalPbHandle handle, BthalPbListSearch listSearch, 
															BthalPbListSortingOrder listSortOrder,
															BthalPbEntriesList *entriesList)
{	
	UNUSED_PARAMETER(listSearch);
	UNUSED_PARAMETER(entriesList);
	UNUSED_PARAMETER(listSortOrder);
	UNUSED_CONST_PARAMETER(handle);

	/* Sample application currently doesn't support search and sort  */
	return BTHAL_STATUS_NOT_SUPPORTED;
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetListedEntryName()
 */
BthalStatus BTHAL_PB_GetListedEntryName(const BthalPbHandle handle, BthalPbEntryName *entryNameStruct, 
																  BthalPbEntryId entryId)
{
	BthalStatus 	 status = BTHAL_STATUS_SUCCESS;
	SyncPhoneBook   *synchPbRec;
    PbRecord   		*rec;
	
	UNUSED_CONST_PARAMETER(handle);

	if (entryId >= BTHAL_PB_MAX_ENTRIES_NUM)
	{
		return BTHAL_STATUS_FAILED;
	}
	
	rec = getRecByUid((U32) pbIdMap[entryId]);

	if (rec == 0)
		return BTHAL_STATUS_FAILED;
	
	synchPbRec = (SyncPhoneBook *)rec->data;

	/* Copy the name of the entry */
	strncpy((char*)entryNameStruct->firstName, (char*)(rec->data + synchPbRec->fname.offset), synchPbRec->fname.len);
	entryNameStruct->firstName[synchPbRec->fname.len] = '\0';

	strncpy((char*)entryNameStruct->middleName, (char*)(rec->data + synchPbRec->mname.offset), synchPbRec->mname.len);
	entryNameStruct->firstName[synchPbRec->fname.len] = '\0';

	strncpy((char*)entryNameStruct->lastName, (char*)(rec->data + synchPbRec->lname.offset), synchPbRec->lname.len);
	entryNameStruct->firstName[synchPbRec->fname.len] = '\0';

	strncpy((char*)entryNameStruct->suffix, (char*)(rec->data + synchPbRec->suffix.offset), synchPbRec->suffix.len);
	entryNameStruct->firstName[synchPbRec->fname.len] = '\0';
	
	return (status);
	
}

/*-------------------------------------------------------------------------------
 * BTHAL_PB_ClosePb()
 */
 BthalStatus BTHAL_PB_ClosePb(BthalPbHandle *handle)
{	
	BthalStatus status = BTHAL_STATUS_SUCCESS;
	
	UNUSED_CONST_PARAMETER(handle);

	PbOpenedUICheckBox(FALSE);
	
	return (status);
}

#endif /* PB_SIMULOTOR_ASYNCHRONOUS == TRUE */


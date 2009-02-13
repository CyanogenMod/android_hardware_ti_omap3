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
*   FILE NAME:      bthal_pb.h
*
*   BRIEF:			The BTHAL_PB is a phonebook abstraction that handles the access  
*					to the devices phonebook. The main functionality of the module   
*					is providing phonebook entries to the BTL layer, which uses its API. 
*
*   DESCRIPTION:    General
*
*					The BTHAL_PB is a phonebook abstraction that handles the access  
*					to the devices phonebook. The main functionality of the module   
*					is providing phonebook entries to the BTL layer, which uses its API. 
*
*
*					Synchronization Mode
*
*					The BTHAL_PB supports both synchronous and asynchronous  phonebooks. 
*					The BTHAL_PB_Register function  will be called after the BTHAL_PB  
*					initialization (BTHAL_PB_Init), to register a callback fucntion. 
*					Each function implementation can be either be synch or asynch, 
* 					depending on implementation needs.
*
*					When a certain function returns asynchronously, the BTHAL_PB is 
*					expected to generate an event to the registered callback at the 
*					completion of operation. The previously requested data should 
*					be associated with the event. See BthalPbEvent.
*				
*					Execution Flow
*
*					All client pull requests are received at the BTL, which calls the 
*					BTHAL_PB for the phonebook data. 
*					At the beginning of each pull operation the BTHAL_PB_OpenPb
*					function will be called. The BTL is guaranteed to access opened  
*					phonebooks only. The pull phonebook and pull Vcard listing operation 
*					will first ask the BTHAL_PB for the number of entries in the opened 
*					phonebook, and for the number of new missed calls. The BTHAL_PB should 
*					make than a sorted and filtered entries list (for the pull Vcard listing 
*					operation only). See BTHAL_PB_BuildEntriesList function. The BTHAL_PB_ClosePb 
*					will be called to close the phonebook, after a number of repeatable 
*					requests for VCF entries data (pull phonebook and pull entry operations) 
*					or entries name (pull Vcard listing operation).  
*
*
*   AUTHOR:         Yoni Shavit
*
\*******************************************************************************/

#ifndef __BTHAL_PB_H
#define __BTHAL_PB_H


#include <bthal_common.h>
#include <bthal_types.h>
#include <bthal_config.h>
#include <btl_unicode.h>

/****************************************************************************
 *
 * Section: Constants
 *
 ***************************************************************************/

/*---------------------------------------------------------------------------
 * BTHAL_PB_FILTER_SIZE constant
 *
 *     This constant defines the size (in bytes) of the vCard filter used 
 *     in the Pull Phonebook and Pull Vcard Entry operations.
 */
#define BTHAL_PB_FILTER_SIZE             (8)

/****************************************************************************
 *
 * Section: Types
 *
 ***************************************************************************/

/* Forward declarations */
typedef struct _BthalPbCallbackParms    BthalPbCallbackParms;
typedef struct _BthalPbListSearch 	    BthalPbListSearch;
typedef struct _BthalPbEntryName 	    BthalPbEntryName;
typedef struct _BthalPbEntriesList	    BthalPbEntriesList;
typedef struct _BtlPbVcardFilter    	BthalPbVcardFilter;

/*-------------------------------------------------------------------------------
 * BthalPbCallBack type
 *
 *     A function of this type is called to indicate BTL FTPS events.
 */
typedef void (*BthalPbCallBack)(const BthalPbCallbackParms *callbackParams);

/*---------------------------------------------------------------------------
 * BthalPbEvent type
 *
 *     The BthalPbEvent type defines the events that may be indicated to
 *     the BTL_PBAP_PB module. The data pointer associated with the event should 
 *	   be the same data pointer provided in the API function that started 
 *	   the operation. For example, *handle is provided in BTHAL_PB_OpenPb, and is
 *	   expected to hold the phonebook handle when BTHAL_PB_EVENT_PB_OPENED is 
 *	   generated (Asynchronous mode only)
 */
typedef BTHAL_U8 BthalPbEvent;

/* 
 * Indicates that a phonebook was successfully opened. Should be generated after  
 * BTHAL_PB_EVENT_PB_OPENED was called. The parameter openPb is associated with the event.
 */
#define BTHAL_PB_EVENT_PB_OPENED                  (0x00)

/* 
 * Indicates that the phonebook entries num parameter is ready. Should be generated after  
 * BTHAL_PB_GetPbEntriesNum was called. The parameter entriesNum is associated with the 
 * event.
 */
#define BTHAL_PB_EVENT_ENTRIES_NUM				  (0x01)

/* 
 * Indicates that the new missed calls number is ready. Should be generated after  
 * BTHAL_PB_GetNewMissedCallsNum was called. The parameter entriesNum is associated 
 * with the event.
 */
#define BTHAL_PB_EVENT_NEW_MISSED_CALLS_NUM		  (0x02)

/* 
 * Indicates that an entry data is ready. Should be generated after  
 * BTHAL_PB_GetEntryData was called. The parameter entryData is associated 
 * with the event.
 */
#define BTHAL_PB_EVENT_ENTRY_DATA				  (0x03)

/* 
 * Indicates that sorted/ filtered entries list is ready. Should be generated after  
 * BTHAL_PB_BuildEntriesList was called. The parameter entriesList is associated 
 * with the event.
 */
#define BTHAL_PB_EVENT_ENTRIES_LIST_READY		  (0x04)

/* 
 * Indicates that an entry name is ready. Should be generated after  
 * BTHAL_PB_BuildEntriesList was called. The parameter entryNameStruct is associated 
 * with the event.
 */
#define BTHAL_PB_EVENT_ENTRY_NAME 				  (0x05)

/* 
 * Indicates that a phonebook was successfully closed. No data is associated with
 * the event.
 */
#define BTHAL_PB_EVENT_PB_CLOSED	              (0x06)

/* Indicates that there an error has occurred. No data is associated with this event. */
#define BTHAL_PB_EVENT_PB_ERROR	                  (0xFF)

/* End of BthalpbEvent */


/*---------------------------------------------------------------------------
 * BthalPbEntriesNum type
 *
 *     A number of phonebook entries.
 */
typedef BTHAL_U16 BthalPbEntriesNum;    

/*---------------------------------------------------------------------------
 * BthalPbVcfLen type
 *
 *     The length of a VCF entry buffer.
 */
typedef BTHAL_U16 BthalPbVcfLen;    

/*---------------------------------------------------------------------------
 * BthalPbHandle type
 *
 *     An handle to a phonebook
 */
typedef void *BthalPbHandle;              

/*---------------------------------------------------------------------------
 * BthalPbPhonebookType type
 *
 *     Defines the type of the phonebook to open in the BTHAL_PB_OpenPb function, according to 
 *	   spec definitions. There are two optional repositories for a phonebook: One that is 
 *	   stored locally in the device's memory (MEM), and another one that is stored on the SIM. 
 *	   For instance, a phonebook path type can be (BTHAL_PB_PATH_SIM | BTHAL_PB_PATH_INCOMING),
 *	   which means incoming call history stored on the SIM card.
 */
typedef BTHAL_U16 BthalPbPhonebookType;

#define BTHAL_PB_PATH_MAIN                    0x0001    /* Main phonebook storing device is used (on FS) */
#define BTHAL_PB_PATH_SIM			          0x0002    /* SIM storing device is used */ 
#define BTHAL_PB_PATH_PHONEBOOK			      0x0004    /* Main phonebook path is used (on FS) */
#define BTHAL_PB_PATH_INCOMING                0x0008    /* Incoming call path is used */
#define BTHAL_PB_PATH_OUTGOING                0x0010    /* Outgoing call path is used */
#define BTHAL_PB_PATH_MISSED                  0x0020    /* Missed call path is used */
#define BTHAL_PB_PATH_COMBINED                0x0040    /* Combined call path is used */

/*-------------------------------------------------------------------------------
 * BthalPbSearchAttribute type
 *
 *	   Defines the search attribute upon which a list of entries names should be built in the 
 *	   BTHAL_PB_BuildEntriesList fucntion. A certain entry should be in the list only if 
 *	   if the 'searchValue' (of the BthalPbListSearch struct) string matches the value of the 
 *	   attribute indicated by 'searchAttribute' parameter.
 *     
 */
typedef BTHAL_U8 BthalPbSearchAttribute;

#define BTHAL_PB_SEARCH_ATTRIBUTE_NAME		(0x00) 
#define BTHAL_PB_SEARCH_ATTRIBUTE_NUMBER	(0x01) 
#define BTHAL_PB_SEARCH_ATTRIBUTE_SOUND		(0x02) 

/*-------------------------------------------------------------------------------
 * BthalPbListSortingOrder type
 *
 *	   Defines the sorting order upon which a list of entries names should be built in the 
 *	   BTHAL_PB_BuildEntriesList function. 
 *     
 */
typedef BTHAL_U8 BthalPbListSortingOrder;

#define BTHAL_PB_SORTING_ORDER_INDEXED			(0x00) 
#define BTHAL_PB_SORTING_ORDER_ALPHABETICAL		(0x01) 
#define BTHAL_PB_SORTING_ORDER_PHONETICAL		(0x02) /* According to the sound attribute of the entry */

/*-------------------------------------------------------------------------------
 * BthalPbEntryId type
 *
 *     Defines the entry ID type.
 */
typedef BTHAL_U16 BthalPbEntryId;

/*-------------------------------------------------------------------------------
 * BthalPbEntryFormat type
 *
 *     Defines the format of the entry returned by the function BTHAL_PB_GetEntryData. 
 */
typedef BTHAL_U8 BthalPbEntryFormat;

#define BTHAL_PB_ENTRY_FORMAT_VCF_2_1  	(0x00)
#define BTHAL_PB_ENTRY_FORMAT_VCF_3_0  	(0x01)


/********************************************************************************
 *
 * Data Structures
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BthalPbListSearch structure
 *
 *     Represents a searching parameters upon which a list of entries names should be built in the 
 *	   BTHAL_PB_BuildEntriesList function. A certain entry should be in the list only if 
 *	   if the 'searchValue' string matches the value of the attribute indicated by 'searchAttribute' parameter.
 *	   the matching routine is implementation specific.
 */
struct _BthalPbListSearch
{
	/* The searching attribute,  */
	BthalPbSearchAttribute searchAttribute;

	/* The searching Value */
	BtlUtf8 searchValue[BTHAL_PB_MAX_ENTRY_NAME];

};

/*-------------------------------------------------------------------------------
 * BthalPbEntryName structure
 *
 *     Represents an entry name.  
 */
struct _BthalPbEntryName
{
	BtlUtf8 lastName[BTHAL_PB_MAX_ENTRY_NAME];
	BtlUtf8 firstName[BTHAL_PB_MAX_ENTRY_NAME];
	BtlUtf8 middleName[BTHAL_PB_MAX_ENTRY_NAME];
	BtlUtf8 prefix[BTHAL_PB_MAX_ENTRY_NAME];
	BtlUtf8 suffix[BTHAL_PB_MAX_ENTRY_NAME];
	
};


/*---------------------------------------------------------------------------
 * BthalPbCallbackParms structure
 *
 * Contains information for the BTHAL_PB callback event.
 */
struct _BthalPbCallbackParms {

    /* BTHAL MM event */
    BthalPbEvent      event;

    /* 
     *Callback parameter object. Holds pointer to data that was filled in the BTHAL_PB. The BTHAL_PB got the 
     * pointers before as parameters in one of the API functions 
     */
    union {
		/* Used in the BTHAL_PB_EVENT_PB_OPENED */
		struct 
		{
			/* An handle pointer for the phonebook that was opened */
			BthalPbHandle *handle;

		} openPb;
		
    
        /* 
         *Used in the BTHAL_PB_EVENT_ENTRIES_NUM and in the BTHAL_PB_EVENT_NEW_MISSED_CALLS_NUM 
		 * and holds the number of entries pointer in both cases
		 */
		BthalPbEntriesNum *entriesNum;

		/* Used in the BTHAL_PB_EVENT_ENTRY_DATA */
		struct 
		{
			/* Pointer to the entry buffer */
            BTHAL_U8 *vcfEnrtyBuf;

			/* Pointer to the entry lenght */
			BthalPbVcfLen *vcfEntryLen;
			
        } entryData;

		/* Used in the BTHAL_PB_EVENT_ENTRIES_LIST_READY and holds the entries list pointer */ 
		BthalPbEntriesList *entriesList;

		/* Used in the BTHAL_PB_EVENT_ENTRY_NAME and holds an entry name */ 
		BthalPbEntryName *entryNameStruct;
		
    } data;
};

/*---------------------------------------------------------------------------
 * BthalPbEntriesList structure
 * 
 *  Holds the sorted and filtered set of entries in ascendant order. Cell 0 holds the phonebook entry ID 
 *	of the first entry in the sorted list, Cell 1 holds the phonebook entry ID 
 *	of the second entry in the sorted list, and so on.
 */
struct _BthalPbEntriesList 
{
	/* Number of matching entries */
	BTHAL_U16         entriesNum;

	/* List array */
    BTHAL_U16         entryId[BTHAL_PB_MAX_ENTRIES_NUM];
};

/*---------------------------------------------------------------------------
 * BtlPbVcardFilter structure
 * 
 *  Describes the 64-bit filter value indicating which fields 
 *  should to be returned for each vCard object.  This filter is used
 *  for both the Pull Phonebook and Pull Entry operations.
 */
struct _BtlPbVcardFilter 
{
    /* Array of 8 bytes for this 64-bit filter value */
    BTHAL_U8                  byte[BTHAL_PB_FILTER_SIZE];
} ;

/********************************************************************************
 *
 * Function declarations
 *
 *******************************************************************************/


/*-------------------------------------------------------------------------------
 * BTHAL_PB_Init()
 *
 * Brief:  
 *      Init the BTHAL_PB module.
 *
 * Description:
 *		Init the BTHAL_PB module.
 *		This function is called before any other BTHAL_PB API functions.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		void.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_PB_Init(void);

/*-------------------------------------------------------------------------------
 * BTHAL_PB_Deinit()
 *
 * Brief:  
 *      Deinitializes the BTHAL_PB layer.
 *
 * Description:
 *	    Deinitializes the BTHAL_PB layer.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		void.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_PB_Deinit(void);

/*-------------------------------------------------------------------------------
 * BTHAL_PB_Register()
 *
 * Brief:  
 *      Register the phonebook. 
 *
 * Description:
 *		Register the phonebook. 
 *		If the phonebook implementation has asynchronous components,
 *		all BTHAL_PB events should be sent to the given callback function.
 *		Otherwise, this function should return BT_STATUS_NOT_SUPPORTED
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		pbCallback [in] - Callback to indicate of the BTHAL_PB events.
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Operation is successful.
 *
 *		BTHAL_STATUS_NOT_SUPPORTED - An asynchronous mode of operation is not supported. 
 *
 *		BTHAL_STATUS_FAILED - The operation failed.
 */
BthalStatus BTHAL_PB_Register(const BthalPbCallBack pbCallback);

/*-------------------------------------------------------------------------------
 * BTHAL_PB_OpenPb()
 *
 * Brief:  
 *       Open a phonebook for reading. 
 *
 * Description:
 *	    Open a phonebook for reading. 
 *		This function will be called at the beginning of a pull operation.
 *		The BTL is guaranteed  to access opened phonebooks only.
 *
 *		The function BTHAL_PB_ClosePb will be called at the end of the operation.
 *
 *		Both synchronous and asynchronous mode of operations are optional. 
 *		The return value indicates what mode is being used. 	
 *      In asynchronous mode, the data pointers associated  with the expected 
 *		event are the output parameters of this function.
 *
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 *		phonebookType [in] - The phonebook type.
 *		
 *		handle [out] - A phonebook handle to be filled by the funciton.
 * 
 * Generated Events:
 *     BTHAL_PB_EVENT_PB_OPENED - The operation was successful. (Asynchronous mode only)
 *	   BTHAL_PB_EVENT_PB_ERROR - The operation has failed. (Asynchronous mode only)
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful (Synchronous mode)
 *
 *		BTHAL_STATUS_PENDING - Indicates that the operation was successful (Asynchronous mode)
 *							   See Generated Events section for expected events and their meanings.
 *					
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_PB_OpenPb(const BthalPbPhonebookType phonebookType, BthalPbHandle *handle);

/*-------------------------------------------------------------------------------
 * BTHAL_PB_ClosePb()
 *
 * Brief:  
 *      Close a phonebook.
 *
 * Description:
 *	    Close a phonebook.
 *
 *		The implementer should be aware that in unusual cases this function can be called 
 *		when 'handle' points to an unopened phonebook, or to a phonebook that is 
 *		currently being opened. In that case, return BTHAL_STATUS_FAILED or generate 
 *		BTHAL_PB_EVENT_PB_ERROR event.
 *
 *		Both synchronous and asynchronous mode of operations are optional. 
 *		The return value indicates what mode is being used.
 *
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 *		handle [in/out] -  A phonebook handle.
 *
 * Generated Events:
 *     BTHAL_PB_EVENT_PB_CLOSED - The operation was successful. (Asynchronous mode only)
 *	   BTHAL_PB_EVENT_PB_ERROR - The operation has failed. (Asynchronous mode only)
 *		
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful (Synchronous mode)
 *
 *		BTHAL_STATUS_PENDING - Indicates that the operation was successful (Asynchronous mode)
 *							   See Generated Events section for expected events and their meanings.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_PB_ClosePb(BthalPbHandle *handle);

/*-------------------------------------------------------------------------------
 * BTHAL_PB_WasPbChanged()
 * 
 * Brief:  
 *      Returns TRUE if the given phonebook was changed since the last time this function 
 *		was called. 
 *
 * Description:
 *	    Returns TRUE if the given phonebook was changed since the last time this function 
 *		was called. 
 *
 *		This function is used by the BTL for the PBAP client synchronization when the
 *		Browsing phonebook feature is used. If the client issues a pull entry operation, and the phonebook is
 *		not valid, the operation will be aborted by the server. (In that case the client has
 *		to issue another pull vCard listing operation for phonebook entries list update)
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *
 *		phonebookType[in] -  The phonebook type.
 *		
 *		pbChanged [out] - Should be set to TRUE if the given phonebook was changed since  
 *						  the last time this function was called.  
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful 
 *
 *		BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_PB_WasPbChanged(const BthalPbPhonebookType phonebookType, BTHAL_BOOL *pbChanged);

/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetPbEntriesNum()
 *
 * Brief:  
 *      Retrieves the number of entries in the given phonebook. 
 *
 * Description:
 *	    Retrieves the number of entries in the given phonebook. 
 *		Note that the first entry of the main phonebook on the FS (BTHAL_PB_PATH_PHONEBOOK) 
 *		must be the owner entry by definition. 
 *
 *		Both synchronous and asynchronous mode of operations are optional. 
 *		The return value indicates what mode is being used.
 *      In asynchronous mode, the data pointers associated with the expected 
 *		event are the output parameters  of this function.
 *
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 *		handle [in] - A phonebook handle.	
 *
 *		entiesNum [out] - Number of entries in the requested phonebook.
 * 
 * Generated Events:
 *     BTHAL_PB_EVENT_ENTRIES_NUM - The operation was successful. (Asynchronous mode only)
 *	   BTHAL_PB_EVENT_PB_ERROR - The operation has failed. (Asynchronous mode only)
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful (Synchronous mode)
 *							   
 *		BTHAL_STATUS_PENDING - Indicates that the operation was successful (Asynchronous mode)
 *							   See Generated Events section for expected events and their meanings.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_PB_GetPbEntriesNum(const BthalPbHandle handle, BthalPbEntriesNum *entiesNum);


/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetNewMissedCallsNum()
 *
 * Brief:  
 *      Retrieves the number of new missed calls that was not checked by the user. 
 *
 * Description:
 *	    Retrieves the number of new missed calls that was not checked by the user. 
 *
 *		Both synchronous and asynchronous mode of operations are optional. 
 *		The return value indicates what mode is being used.
 *      In asynchronous mode, the data pointers associated with the expected 
 *		event are the output parameters of this function.
 *
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 *		handle [in] - A phonebook handle.
 *
 *		BTHAL_PB_EVENT_NEW_MISSED_CALLS_NUM [out] - Number of new missed calls that was not checked by the user.
 *
 * Generated Events:
 *     BTHAL_PB_EVENT_NEW_MISSED_CALLS_NUM - The operation was successful. (Asynchronous mode only)
 *	   BTHAL_PB_EVENT_PB_ERROR - The operation has failed. (Asynchronous mode only)
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful (Synchronous mode)
 *
 *		BTHAL_STATUS_PENDING - Indicates that the operation was successful (Asynchronous mode)
 *							   See Generated Events section for expected events and their meanings.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_PB_GetNewMissedCallsNum(const BthalPbHandle handle, BthalPbEntriesNum *entiesNum);

/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetEntryData()
 *
 * Brief:  
 *      Retrieves the content of a phonebook entry. The function should fill 
 *		the given buffer according to the given parameters.
 *
 * Description:
 *	    Retrieves the content of a phonebook entry. The function should fill 
 *		the given buffer according to the given parameters.
 *
 *		Both synchronous and asynchronous mode of operations are optional. 
 *		The return value indicates what mode is being used.
 *		In asynchronous mode, the data pointers associated with the expected 
 *		event are the output parameters of this function.
 *
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 *		handle [in] - A phonebook handle.
 *
 *		entryId [in] - The index of the requested enrty.
 *	
 *		entryFormat [in] - The format of the entry filled by the function.
 *
 *		vcardFilter [in] - A VCF filter. Indicates what VCF fields are requested.
 *
 * 		vcfEnrtyBuf [out] - A buffer be filled by the function if a VCF format was chosen.
 *
 *		vcfEntryLen [out] - The length of the buffer that was filled by the function if a VCF format was chosen.
 *
 * Generated Events:
 *     BTHAL_PB_EVENT_ENTRY_DATA - The operation was successful. (Asynchronous mode only)
 *	   BTHAL_PB_EVENT_PB_ERROR - The operation has failed. (Asynchronous mode only)
 *
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful (Synchronous mode)
 *
 *		BTHAL_STATUS_PENDING - Indicates that the operation was successful (Asynchronous mode)
 *							   See Generated Events section for expected events and their meanings.
 *
 *		BTHAL_STATUS_NOT_FOUND - Indicates that the given entry ID was not found.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_PB_GetEntryData(const BthalPbHandle handle, BthalPbEntryId entryId, 
															  BthalPbEntryFormat entryFormat,
															  BthalPbVcardFilter vcardFilter,
													    	  BTHAL_U8 *vcfEnrtyBuf, 
													    	  BthalPbVcfLen *vcfEntryLen);

/*-------------------------------------------------------------------------------
 * BTHAL_PB_BuildEntriesList()
 *
 * Brief:  
 *      Builds a list of entries names according to the given sorting and searching parameters. 
 *
 * Description:
 *	    Builds a list of entries names according to the given sorting and searching parameters. 
 *		The built list is used for the building of the XML listing for the PullvCardEntry operation.
 *		When the function returns the caller can use the BTHAL_PB_GetListedEntryName to retrieve
 *		the next name of an entry in the list.
 *		If search or sort is not supported by the platform, this function should return BTHAL_STATUS_NOT_SUPPORTED,
 *		and the returned entries of the function BTHAL_PB_GetListedEntryName will not be sorted and will 
 *		not match a searching routine, and will be pulled by ascending index. 
 *	    Note that the first entry in the list of the main phonebook on the FS (BTHAL_PB_PATH_PHONEBOOK) 
 *		must be the owner entry by definition.
 *
 *		Both synchronous and asynchronous mode of operations are optional. 
 *		The return value indicates what mode is being used.
 * 		In asynchronous mode, the data pointers associated with the expected 
 *		event are the output parameters of this function.
 *		
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 *		handle [in] - A phonebook handle.
 *
 *		listSearch [in] - A search attribute and a search value upon which the list should be built. The matching routine 
 *						  is implementation specific. If this parameters is 0, then no search should be performed 
 * 						  and all the entries should appear in the list.
 *		
 *		listSortOrder [in] - A sorting order upon which the list should be built. 
 *	
 *		entriesList [out] - The sorted and filtered set of entries.
 *
 * Generated Events:
 *     BTHAL_PB_EVENT_ENTRIES_LIST_READY - The operation was successful. (Asynchronous mode only)
 *	   BTHAL_PB_EVENT_PB_ERROR - The operation has failed. (Asynchronous mode only)
 *
 *
 * Returns:
 *
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful (Synchronous mode)
 *
 *		BTHAL_STATUS_PENDING - Indicates that the operation was successful (Asynchronous mode)
 *							   See Generated Events section for expected events and their meanings.
 *							   
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 *
 *		BTHAL_STATUS_NOT_SUPPORTED - Search and sort are not supported by the platform.
 */
BthalStatus BTHAL_PB_BuildEntriesList(const BthalPbHandle handle, BthalPbListSearch listSearch, 
																  BthalPbListSortingOrder listSortOrder,
															      BthalPbEntriesList *entriesList);

/*-------------------------------------------------------------------------------
 * BTHAL_PB_GetListedEntryName()
 *
 * Brief:  
 *      Retrieves the name of an entry in the given phonebook.
 *
 * Description:
 *	    Retrieves the name of an entry in the given phonebook. 
 * 		If a certain field of the name doesn't exist, 
 *		It should not be filled with data. 
 *
 *		Both synchronous and asynchronous mode of operations are optional. 
 *		The return value indicates what mode is being used.
 *		In asynchronous mode, the data pointers associated with the expected 
 *		event are the output parameters of this function.
 *
 * Type:
 *		Synchronous/Asynchronous
 *
 * Parameters:
 *		handle [in] - A phonebook handle.
 *
 *		entryNameStruct [out] - The name of the entry.
 *
 *		BthalPbEntryId [in] - The index of the entry in the phonebook.
 *
 * Generated Events:
 *     BTHAL_PB_EVENT_ENTRY_NAME - The operation was successful. (Asynchronous mode only)
 *	   BTHAL_PB_EVENT_PB_ERROR - The operation has failed. (Asynchronous mode only)
 *
 * Returns:
 *		BTHAL_STATUS_SUCCESS - Indicates that the operation was successful (Synchronous mode)
 *
 *		BTHAL_STATUS_PENDING - Indicates that the operation was successful (Asynchronous mode)
 *							   See Generated Events section for expected events and their meanings.
 *
 *		BTHAL_STATUS_NOT_FOUND - The given entry ID was not found.
 *
 *     	BTHAL_STATUS_FAILED - Indicates that the operation failed.
 */
BthalStatus BTHAL_PB_GetListedEntryName(const BthalPbHandle handle, BthalPbEntryName *entryNameStruct, 
																    BthalPbEntryId entryId);

#endif /* __BTHAL_PB_H */







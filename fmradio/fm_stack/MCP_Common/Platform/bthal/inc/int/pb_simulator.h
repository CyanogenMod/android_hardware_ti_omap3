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
*   FILE NAME:      pb_simulator.h
*
*   DESCRIPTION:    This file is a sample phonebook header on windonws platform.
*					It used to implement the bthal_pb.h API, providing basic phonebook funcitonality
*					such as getting phonebook entries data. The sample phonebook can be 
*					configured using the PBAP sapmle application.
*
*   AUTHOR:         Yoni Shavit
*
\*******************************************************************************/

#ifndef __PB_SAMPLE_H
#define __PB_SAMPLE_H


/****************************************************************************
 *
 * Constants
 *
 ****************************************************************************/


#include <bthal_pb.h>
#include <pb_simulator.h>
#include <bthal_common.h>
#include "xastatus.h"
#include "osapi.h"
#include "utils.h"
#include "vParser.h"
#include "pbap.h"
#include "windows.h"
#include <btl_defs.h>
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "io.h"


/*---------------------------------------------------------------------------
 * NUM_VECTORS constant
 *
 *     
 */
#define NUM_VECTORS         6

/*---------------------------------------------------------------------------
 * PB_MAX_DATA_LEN constant
 *
 *     Maximum bytes of data allowed per record.
 */
#define PB_MAX_DATA_LEN         4096

/*---------------------------------------------------------------------------
 * PB_MAX_NAME_LEN constant
 *
 *     Maximum number of characters allowed for pathnames + 1 (null-
 *     terminating character).
 */
#define PB_MAX_NAME_LEN         128

/*---------------------------------------------------------------------------
 * PB_MAX_ENTRIES constant
 *
 *     Maximum bytes of database entries allowed, in addition to the 
 *     restriction that PbUid is a 16-bit value and cannot exceed 0xFFFF.
 *     This value is used when tracking used entries in the searching functions
 *     for returning the vCard listing.
 */
#define PB_MAX_ENTRIES          512

 /****************************************************************************
 *
 * Types
 *
 ****************************************************************************/

/*---------------------------------------------------------------------------
 * PbStatus type
 *
 *     This type is returned from most phonebook APIs to indicate the success
 *     or failure of the operation. In many cases, BT_STATUS_PENDING
 *     is returned, meaning that a future callback will indicate the
 *     result of the operation.
 */
typedef XaStatus PbStatus;

#define PB_STATUS_SUCCESS           XA_STATUS_SUCCESS
#define PB_STATUS_FAILED            XA_STATUS_FAILED
#define PB_STATUS_NO_RESOURCES      XA_STATUS_NO_RESOURCES
#define PB_STATUS_NOT_FOUND         XA_STATUS_NOT_FOUND
#define PB_STATUS_UNKNOWN_REMOTE    XA_STATUS_DEVICE_NOT_FOUND
#define PB_STATUS_INUSE             XA_STATUS_IN_USE
#define PB_STATUS_NOT_SUPPORTED     XA_STATUS_NOT_SUPPORTED
/* End of PbStatus */

/*---------------------------------------------------------------------------
 * Path types
 *
 *    Defines optinal pathes of the PBAP virtual folder structure.
 */
#define 	PB_MAIN_PB 					"telecom/pb"
#define 	PB_MAIN_INCOMING			"telecom/ich"	
#define 	PB_MAIN_OUTGOING			"telecom/och"	
#define 	PB_MAIN_MISSED				"telecom/mch"	
#define 	PB_MAIN_COMBINED		    "telecom/cch"
#define 	PB_SIM_PB 					"SIM1/telecom/pb"
#define 	PB_SIM_INCOMING				"SIM1/telecom/ich"
#define 	PB_SIM_OUTGOING				"SIM1/telecom/och"	
#define 	PB_SIM_MISSED				"SIM1/telecom/mch"	
#define 	PB_SIM_COMBINED		   		"SIM1/telecom/cch"


#define PB_MAX_RECORD_LEN   (PB_MAX_DATA_LEN + sizeof(SyncPhoneBook))
#define PB_PATH             "telecom/pb"


/*---------------------------------------------------------------------------
 * PbUid type
 */
typedef U32 PbUid;



/****************************************************************************
 *
 * Data Structures
 *
 ****************************************************************************/

/*--------------------------------------------------------------------------
 * PbDatabase structure
 *
 *     Description
 */
typedef struct _PbDatabase {
    ListEntry            records;                           /* List of Phonebook records */
    char                 tmpName[PB_MAX_NAME_LEN];          /* Temporary phonebook file */
    PbUid                nextUid;                           /* Next unique identifier */
    U16                  openRecs;                          /* PbGetRecord/PbNewRecord ref count */
    U16                  openCount;                         /* PbOpen count */
    BOOL                 changed;                           /* Database has been changed */
    char                 currentPath[PB_MAX_NAME_LEN];      /* Current path into the database */
    char                 getListingPath[PB_MAX_NAME_LEN];   /* Path used to get the vCard folder listing */
    U16                  pbOffset;                          /* Offset used during database operations */
    
    U8                  *outBuffer;                         /* Temp buffer used when sending record data (Get). */
    U16                  outBuffLen;
} PbDatabase;


/*--------------------------------------------------------------------------
 * PbRecord structure
 *
 *     Description
 */
typedef struct _PbRecord {
    ListEntry       node;
    PbUid           uid;                    /* Local database unique identifier */
    char            path[PB_MAX_NAME_LEN];  /* Complete path for the phonebook record */
    U16             dataLen;                /* Data length in bytes */
    U8             *data;                   /* Pointer to record data */
} PbRecord;

/********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/

/*---------------------------------------------------------------------------
 * PbOpen()
 *
 *     Opens the phonebook.
 *
 * Returns:
 *     PB_STATUS_SUCCESS - Open is successful.
 *
 *     PB_STATUS_FAILED - Error occurred during database operation; could
 *         be either read/write or open/close.
 *
 *     PB_STATUS_NOT_FOUND - Failed to open file.
 *
 *     PB_STATUS_NO_RESOURCES - No DataRecord structs available.
 *
 *     PB_STATUS_DATABASE_CORRUPT - Encountered unexpected EOF, database corrupt.
 */
PbStatus PbOpen(void);


/*---------------------------------------------------------------------------
 * PbClose()
 *
 *     Closes an open phonebook, writing it to disk.
 *
 * Returns:
 *     PB_STATUS_SUCCESS - Close is successful.
 *
 *     PB_STATUS_FAILED - Error occurred during database-file manipulation or
 *         while writing phonebook to disk from memory.
 */
PbStatus PbClose(void);


/*---------------------------------------------------------------------------
 * PbAddOwnerRecord()
 *
 *     Adds the 0.vcf owner vCard record.
 *
 * Returns:
 *     PB_STATUS_SUCCESS - Addition of the owner vCard is successful.
 *
 *     PB_STATUS_FAILED - Failure to add the owner vCard record.
 *
 *     PB_STATUS_NO_RESOURCES - No DataRecord structs available. 
 */
PbStatus PbAddOwnerRecord(const void *OwnerPtr, U16 OwnerLen);

/*---------------------------------------------------------------------------
 * PbNewRecord()
 *
 *     Adds new record to phonebook and opens it as current "in use" record.
 *
 * Returns:
 *     Pointer to the new phonebook record object.  Returns 0 if a
 *     record cannot be created.
 */
PbRecord *PbNewRecord(PbUid *id);

/*---------------------------------------------------------------------------
 *            PbDeleteRecord()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Deletes a record from the phonebook by unique identifier (UID).
 *
 * Input:     UID of record to remove.
 *
 * Return:    PB_STATUS_SUCCESS - Record removed from database.
 *            PB_STATUS_FAILED - Cannot remove a record which is currently in use.
 *            PB_STATUS_NOT_FOUND - Record with specified UID not found in phonebook.
 */
PbStatus PbDeleteRecord(PbUid id);


/*---------------------------------------------------------------------------
 * PbReleaseRecord()
 *
 *     Releases record currently in use, after which it can no longer
 *     be read or modified (without calling PbGetRecord again).
 *
 * Returns:
 *     PB_STATUS_SUCCESS - Record released.
 *
 *     PB_STATUS_FAILED - No record currently in use.
 */
PbStatus PbReleaseRecord(PbRecord *Record);


/*---------------------------------------------------------------------------
 * PbWriteRecord()
 *
 *     Writes data and new path information to the phonebook
 *     database record.
 *
 * Returns:
 *     PB_STATUS_SUCCESS - Copied all of data into current record.
 *
 *     PB_STATUS_FAILED - Data exceeds size supported by phonebook.
 */
PbStatus PbWriteRecord(PbRecord *Record, const char *Path,
                       const void *DataPtr, U16 DataLen);

/*---------------------------------------------------------------------------
 * PbGetUidList()
 *
 *     Returns an array of the PbUid values for each record in the
 *     phonebook database.
 *
 * Returns:
 *     PB_STATUS_SUCCESS - The LuidArray has been built, or there are no
 *         matching records.
 *
 *     PB_STATUS_UNKNOWN_REMOTE - The remot device specified is now known.
 *
 *     PB_STATUS_NO_RESOURCES - The LuidList could not allocated.
 */
PbStatus PbGetUidList(PbUid *UidArray[]);

/*---------------------------------------------------------------------------
 * PbReturnUidList()
 *
 *     Returns a UidList requested via PbGetUidList. 
 *
 */
void PbReturnUidList(PbUid UidArray[]);


/*---------------------------------------------------------------------------
 *            pbRemoveRecord()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Removes a deleted record from the phonebook by unique 
 *            identifier (UID).
 *
 * Input:     UID of record to remove.
 *
 * Return:    PB_STATUS_SUCCESS - Record removed from database.
 *            PB_STATUS_FAILED - Cannot remove a record which is currently in use.
 *            PB_STATUS_NOT_FOUND - Record with specified UID not found in database.
 */
static void pbRemoveRecord(PbUid id);

/*---------------------------------------------------------------------------
 *            getRecByUid()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Searches in database for record with specified unique identifier 
 *            (UID) and, if found, returns pointer to it.
 *
 * Input:     UID of the entry to retrieve.
 *
 * Return:    Pointer to the ListEntry in database if found.  Otherwise,
 *            returns NULL.
 */
PbRecord *getRecByUid(PbUid id);


/*---------------------------------------------------------------------------
 *            writePB()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Write phonebook to disk while retaining it in memory.  This
 *            function should be called after all phonebook operations.
 *
 * Return:    PB_STATUS_SUCCESS - All data written successfully to disk.
 *            PB_STATUS_FAILED - There was an error writing to disk, or temp
 *                file could not be opened.  The phonebook has NOT
 *                been saved properly.
 */
static PbStatus writePB(void);



/*---------------------------------------------------------------------------
 *            __itoa
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Converts an unsigned 32-bit value to a base-10 number string.
 *
 * Return:    void.
 */
static void __itoa(U8 *out, U32 val);


/*---------------------------------------------------------------------------
 *            __htoa
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Converts an unsigned 32-bit value to a base-16 number string.
 *
 * Return:    void.
 */
static void __htoa(U8 *out, U32 val);


/*---------------------------------------------------------------------------
 *            pbConvert2MimeFormat()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Converts a native PbRecord to the MIME exchange format for
 *            transmission. Our PB stores objects in XCPC SyncObject format.
 */
PbStatus pbConvert2MimeFormat(const U8 *DataPtr, U16 DataLen);

/*---------------------------------------------------------------------------
 * PbGetRecord()
 *
 *     Retrieves a record from the database by unique identifer (UID), holds 
 *     pointer to it for reading and writing.
 *
 * Returns:
 *     PB_STATUS_SUCCESS - Record successfully obtained.
 *
 *     PB_STATUS_FAILED - There is already a record in the buffer.
 *
 *     PB_STATUS_NOT_FOUND - No record found with given UID.
 */
PbRecord *PbGetRecord(PbUid id);


/*---------------------------------------------------------------------------
 * PbGetValidationStatus()
 *
 *		Get the validation status of a phone book. When an entry is deleted or
 *		added, the phone book is no longer valid until another pull VCF listing
 */
BOOL PbGetValidationStatus();

/*---------------------------------------------------------------------------
 * PbSetValidationStatus()
 *
 *		Set the validation status of a phone book. When an entry is deleted or
 *		added, the phone book is no longer valid until another pull VCF listing
 */
void PbSetValidationStatus(BOOL status);

#endif __PB_SAMPLE_H
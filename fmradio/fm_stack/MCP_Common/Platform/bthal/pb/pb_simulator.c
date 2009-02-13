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
*   FILE NAME:      pb_simulator.c
*
*   DESCRIPTION:    This file is a sample phonebook implementation on windonws platform.
*					It used to implement the bthal_pb.h API, providing basic phonebook funcitonality
*					such as getting phonebook entries data. The sample phonebook can be 
*					configured using the PBAP sapmle application.
*
*   AUTHOR:         Yoni Shavit
*
\*******************************************************************************/

#include "pb_simulator.h"

/*---------------------------------------------------------------------------
 * Phonebook Access Sample Database Component
 *
 *    The sample phonebook API is used to interact with the built-in object
 *    store component used within PBAP to provide information regarding the
 *    data exchanged during the various PBAP operations. The phonebook API is
 *    responsible for receiving the data indications and data requests to 
 *    provide and/or store the data in the phonebook.  It is also responsible
 *    to determine how this data should be stored or retrieved based on the 
 *    information provided by the object store component.
 *    
 *    The sample phonebook layer is system-dependent and must be ported before
 *    use on any new systems.
 */


/********************************************************************************
 *
 * Globals
 *
 *******************************************************************************/

/* Global list of phonebooks. */
PbDatabase phonebook = {0};
static BOOL phonebookValid = TRUE;

/********************************************************************************
 *
 * Function Prototypes
 *
 *******************************************************************************/

static void freePbRecs();

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
PbStatus PbOpen(void)
{
    PbRecord       *newRec;
    FILE           *filePtr;
    int             length, uid, nextUid;
    char            pbName[PB_MAX_NAME_LEN+4];
    char            separator[10];
    int             numRead = 0;

    if (phonebook.openCount > 0) {
        /* Phonebook Database is already open */
        Report(("PbOpen() phonebook database already open.\n"));
        phonebook.openCount++;
        return PB_STATUS_SUCCESS;
    }

    /* Strip possible ".pbs" extension from PbName. */
    strcpy(pbName, "phonebook.pbs");

    Report(("PbOpen() Opening phonebook '%s'.\n", pbName));

    /* Phonebook is not currently open. */
    memset((U8 *)&phonebook, 0, sizeof(PbDatabase));

    InitializeListHead(&phonebook.records);
#if TI_CHANGES == XA_ENABLED							
	/* A temporary file, in which the phone book data base is kept.
	The temporary file is copied into a final file determined in "pbName" variable.
	Originaly, "tmpnam" function of windows used to generate the temporary location randomly*/
	strcpy(phonebook.tmpName, "my_phonebook.pbs");
#else
	tmpnam(phonebook.tmpName);
#endif /* TI_CHANGES == XA_ENABLED */
	
    phonebook.nextUid = 0;
    phonebook.pbOffset = 0;

    /* Attempt to open specified database file */
    filePtr = fopen(pbName, "rb");
    if (filePtr == 0) {
        /* Determine if file exists.  If not, we will continue on our way
         * and create it later.  If it exists, the fopen() call failed
         * for another reason so return error.
         *
         * NOTE: The _access() function is not ANSI-compliant, but a given
         *     OS may support it anyway or provide a very similar function.
         */
        if (_access(pbName, 0)) {
            Report(("PbOpen(): Created new phonebook '%s'\n", pbName));

            /* Skip all the opening, closing, and reading operations */
            phonebook.openCount++;
            return PB_STATUS_SUCCESS;
        }

        Report(("PbOpen(): Unable to open phonebook '%s'\n", pbName));
        return PB_STATUS_FAILED;
    }

    /* First read the Phonebook Header: phonebook size and next UID */
    fscanf(filePtr, "%d\n", &nextUid);
    phonebook.nextUid = nextUid;

    /* _cnt -- HACK */
    while (filePtr->_cnt > 0 && !feof(filePtr) && !ferror(filePtr)) {
        /* Get the separator */
        fscanf(filePtr, "%s\n", &separator);
        if (strcmp(separator, "*VCARD*") == 0) {
            /* Allocate empty record struct before reading data portion. */
            newRec = (PbRecord *)malloc(sizeof(PbRecord) + PB_MAX_RECORD_LEN);
            if (newRec == 0) {
                goto LoadFailure;
            }
            memset((U8 *)newRec, 0, sizeof(PbRecord));

            fscanf(filePtr, "%d\n%d\n%s\n", &uid, &length, newRec->path);

            newRec->uid = uid;
            newRec->dataLen = (U16)length;
            newRec->data = (U8 *)(newRec + 1);

            /* Read the record data */
            numRead = fread(newRec->data, sizeof(U8), length, filePtr);
            if (numRead != length) {
                Report(("PbOpen(): Error reading vCard data, read %d bytes.\n", numRead));
                goto LoadFailure;
            }
            fscanf(filePtr, "\n");

            InsertTailList(&phonebook.records, &newRec->node);
        }
    }

    /* Close the database file */
    if (fclose(filePtr) == 0) {
        phonebook.openCount++;
        return PB_STATUS_SUCCESS;
    }

LoadFailure:    /* Cleanup */
    fclose(filePtr);
    freePbRecs();
    return PB_STATUS_FAILED;
}

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
PbStatus PbClose(void)
{
    char    pbName[PB_MAX_NAME_LEN+4];

    /* Write phonebook to disk one last time before proceeding, just to
     * be safe (should have already been done after last meaningful operation).
     */
    if (writePB() != PB_STATUS_SUCCESS) {
        return PB_STATUS_FAILED;
    }

    if (phonebook.openCount > 1) {
        phonebook.openCount--;
        return PB_STATUS_SUCCESS;
    }
    Assert(phonebook.openCount == 1);
    phonebook.openCount--;

    /* Assign original phonebook */
    strcpy(pbName, "phonebook.pbs");

    Report(("PbClose() Closing database '%s' openrecs = %d.\n", pbName, phonebook.openRecs));

    /* If the temp file exists, remove the old file and rename the temp. */
    if (_access(phonebook.tmpName, 0) == 0) {
        remove(pbName);

        if (rename(phonebook.tmpName, pbName) != 0) {
            /* Error renaming temp database as actual database.  */
            Report(("PbClose(): Error renaming temp phonebook %s to actual phonebook %s.\n", 
                    phonebook.tmpName, pbName));
            return PB_STATUS_FAILED;
        }
    }

    freePbRecs();
    return PB_STATUS_SUCCESS;
}

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
PbStatus PbAddOwnerRecord(const void *OwnerPtr, U16 OwnerLen)
{
	PbUid			id;
	PbStatus		status;
    PbRecord       *newRec;

    if (!OwnerPtr || OwnerLen == 0) {
        return PB_STATUS_FAILED;
    }

    if (phonebook.nextUid != 0) {
        /* Only add the owner vCard record the first time the
         * phonebook database is created 
         */
        return PB_STATUS_SUCCESS;
    }

    if (OwnerPtr && OwnerLen > 0) {
    	/* Automatically add the owners vCard (0.vcf) - if the phonebook is being newly created */
		newRec = PbNewRecord(&id);
        if (!newRec) {
            return PB_STATUS_NO_RESOURCES;
        }

		Assert((id == 0) && (phonebook.nextUid == 1));
		status = PbWriteRecord(newRec, PB_PATH, OwnerPtr, OwnerLen);
		Assert(status == PB_STATUS_SUCCESS);
    }

    return PB_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
 * PbNewRecord()
 *
 *     Adds new record to phonebook and opens it as current "in use" record.
 *
 * Returns:
 *     Pointer to the new phonebook record object.  Returns 0 if a
 *     record cannot be created.
 */
PbRecord *PbNewRecord(PbUid *id)
{
    PbRecord        *rec = 0;

    if (phonebook.nextUid >= PB_MAX_ENTRIES)
        return 0;

    rec = (PbRecord *)malloc(sizeof(PbRecord) + PB_MAX_RECORD_LEN);
    if (rec == 0) {
        return 0;
    }
    memset((U8 *)rec, 0, sizeof(PbRecord) + PB_MAX_RECORD_LEN);

    rec->uid = phonebook.nextUid++; /* Assign next UID */
    strcpy(rec->path, PB_PATH);     /* Assign default record path */
    rec->data = (U8 *)(rec + 1);
    
    /* Add record to phonebook */
    InsertTailList(&phonebook.records, &rec->node);
    phonebook.openRecs++;

    *id = rec->uid;

    Report(("PbNewRecord() Assigned UID %d.\n", *id));
    return rec;
}

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
PbStatus PbDeleteRecord(PbUid id)
{
    PbRecord    *rec = 0;

    Report(("PbDeleteRecord() Deleting %d UID Entry.\n", id));

    rec = getRecByUid(id);
    if (rec == 0) {
        return PB_STATUS_NOT_FOUND;
    }

    rec->dataLen = 0;

    /* See if record is ready to be removed. */
    pbRemoveRecord(id);
    phonebook.changed = TRUE;

    /* Save phonebook */
    writePB();
    
    return PB_STATUS_SUCCESS;
}

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
PbStatus PbReleaseRecord(PbRecord *Record)
{
    Assert(IsNodeOnList(&phonebook.records, &Record->node));

    /* Save phonebook, if the record hasn't been deleted */
    writePB();

    phonebook.openRecs--;
    return PB_STATUS_SUCCESS;
}

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
                       const void *DataPtr, U16 DataLen)
{
    U16     i, pathLen, write;

    if ((DataLen > 0) && (((U8 *)DataPtr)[0] < '!')) {
        /* Don't accept records that start with white space. */
        return PB_STATUS_FAILED;
    }

    write = (U16)min(DataLen, PB_MAX_RECORD_LEN);
    memcpy(Record->data, DataPtr, write);
    Record->dataLen = write;
    if (Path != 0) {
		/* Setting the old path to zero */
		memset(Record->path, 0, PB_MAX_NAME_LEN);
        /* Copy new path information, being sure to use '/' path 
         * separators instead of  '\'
         */
        pathLen = (U16)strlen(Path);
        for (i = 0; i < pathLen; i++) {
            if (Path[i] == '\\') {
                /* Convert path separator to '/' */
                Record->path[i] = '/';
            } else {
                Record->path[i] = Path[i];
            }
        }
    }
    phonebook.changed = TRUE;

    /* Save database */
    writePB();

    return PB_STATUS_SUCCESS;
}

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
PbStatus PbGetUidList(PbUid *UidArray[])
{
    PbRecord    *rec;
    PbUid       *list;
    U16          numRecords = 0;    /* Empty, terminating record */
	U16          i,j;
	U32 	     temp;
    Assert(UidArray);
    *UidArray = 0;

    rec = (PbRecord *)GetHeadList(&phonebook.records);
    while (rec != (PbRecord *)&phonebook.records) {
        numRecords++;
        rec = (PbRecord *)GetNextNode(&rec->node);
    }

    if (numRecords == 1) {
        return PB_STATUS_SUCCESS;
    }

    list = (PbUid *)malloc(numRecords * (sizeof(PbUid)));
    if (list == 0) {
        return PB_STATUS_NO_RESOURCES;
    }
    memset((U8 *)list, 0, numRecords * (sizeof(PbUid)));

    numRecords = 0;
    rec = (PbRecord *)GetHeadList(&phonebook.records);
    while (rec != (PbRecord *)&phonebook.records) {
        if (rec->uid != 0) {
            /* Ignore the 0.vcf owner vCard */
            list[numRecords++] = rec->uid;
        }
        rec = (PbRecord *)GetNextNode(&rec->node);
    }
	/* Sort the array */
	for (i = 0; i < numRecords - 2 ; i++)
	{
		for (j = (U16)(i + 1); j < (U16)(numRecords - 1) ; j++)
		{
			if (list[j] > list[j-1])
			{
				temp = list[j];
				list[j] = list[j-1];
				list[j] = temp;
			}
		}
	}

    *UidArray = list;
    return PB_STATUS_SUCCESS;
}


/*---------------------------------------------------------------------------
 * PbReturnUidList()
 *
 *     Returns a UidList requested via PbGetUidList. 
 *
 */
void PbReturnUidList(PbUid UidArray[])
{
    free(UidArray);
}


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
static void pbRemoveRecord(PbUid id)
{
    PbRecord    *rec = 0;

    if ((rec = getRecByUid(id)) == 0) {
        return;
    }

    /* Record is ready to be removed from the phonebook and freed. */
    Report(("pbRemoveRecord() Removing UID %d.\n", id));

    RemoveEntryList(&(rec->node));
    free(rec);
}

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
PbRecord *getRecByUid(PbUid id)
{
    ListEntry   *tmp;

    tmp = GetHeadList(&phonebook.records);
    while (tmp != (&phonebook.records)) {
        if (((PbRecord *)tmp)->uid == id) {
            return (PbRecord *)tmp;
        }
        tmp = GetNextNode(tmp);
    }

    return 0;
}

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
static PbStatus writePB(void)
{
    FILE            *tmpPB;
    PbRecord        *rec;
    U16              numRecords = 0;

    if (phonebook.changed == FALSE) {
        /* Nothing to write to the database */
        return PB_STATUS_SUCCESS;
    }

    /* Open temp file for storing database until PbClose() is called */
    tmpPB = fopen(phonebook.tmpName, "wb");
    if (tmpPB == NULL) {
        goto failed;
    }

    /* Write Phonebook Header: phonebook size and next UID */
    fprintf(tmpPB, "%d\n", phonebook.nextUid);

    /* Write the Records: Size and Data */
    rec = (PbRecord *)GetHeadList(&phonebook.records);
    while (rec != (PbRecord *)(&phonebook.records)) {
        /* Each phonebook record includes a separator */
        fprintf(tmpPB, "%s\n", "*VCARD*");
        fprintf(tmpPB, "%d\n%d\n", rec->uid, rec->dataLen);
        fprintf(tmpPB, "%s\n", rec->path);
        fwrite(rec->data, sizeof(U8), rec->dataLen, tmpPB);
        fprintf(tmpPB, "\n");

        rec = (PbRecord *)GetNextNode(&rec->node);
        numRecords++;
    }

    if (fclose(tmpPB) == 0) {
        Report(("WritePB() Wrote %d records.\n", numRecords));
        phonebook.changed = FALSE;
        return PB_STATUS_SUCCESS;
    }

failed:
    Report(("WritePB(): Error saving phonebook to temporary working file.\n"));
    return PB_STATUS_FAILED;
}


static void freePbRecs()
{
    ListEntry      *rec;

    /* Now free the memory */
    while (!IsListEmpty(&phonebook.records)) {
        rec = RemoveHeadList(&phonebook.records);
        free(rec);
    }
}


/*---------------------------------------------------------------------------
 *            pbConvert2MimeFormat()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Converts a native PbRecord to the MIME exchange format for
 *            transmission. Our PB stores objects in XCPC SyncObject format.
 */
PbStatus pbConvert2MimeFormat(const U8 *DataPtr, U16 DataLen)
{
    U8         *data, *vData;
    U16         dataLen;
    vContext    vx;
    SyncObject  so = {0};

    dataLen = DataLen;
    data = (U8*)DataPtr;

    if (dataLen > 0) {
        /* Convert the PB object (SyncObject) to Mime format (vCard) */
        switch (data[0]) {
        case 'p':
            so.phoneBook = (SyncPhoneBook *)data;
            break;

        default:
            return PB_STATUS_FAILED;
        }

        dataLen = BTHAL_PB_MAX_ENTRY_LEN;
        vData = vBuild(phonebook.outBuffer, &dataLen, &so, &vx);
        if (vData == 0) {
            phonebook.outBuffLen = 0;
            return PB_STATUS_FAILED;
        }
    }
    
    phonebook.outBuffLen = dataLen;      /* Indicates in-use */
    
    return PB_STATUS_SUCCESS;
}

/*---------------------------------------------------------------------------
 *            PbGetRecord()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieves a record from the phonebook database by unique 
 *            identifier (UID), returns a pointer to it for reading and 
 *            writing.
 *
 * Input:     UID of record to obtain.
 *
 * Return:    PB_STATUS_SUCCESS - Record successfully obtained.
 *            PB_STATUS_FAILED - A record is already in use.
 *            PB_STATUS_NOT_FOUND - No record found with given name.
 */
PbRecord *PbGetRecord(PbUid id)
{
    PbRecord    *rec;

    Report(("PbGetRecord() UID %d.\n", id));
	
	rec = getRecByUid(id);
    if (rec)
        phonebook.openRecs++;

    return rec;
}


/*---------------------------------------------------------------------------
 * PbGetValidationStatus()
 *
 *		Get the validation status of a phone book. When an entry is deleted or
 *		added, the phone book is no longer valid until another pull VCF listing
 */
BOOL PbGetValidationStatus()
{
	return phonebookValid;
}

/*---------------------------------------------------------------------------
 * PbSetValidationStatus()
 *
 *		Set the validation status of a phone book. When an entry is deleted or
 *		added, the phone book is no longer valid until another pull VCF listing
 */
void PbSetValidationStatus(BOOL status)
{
	phonebookValid = status;
}
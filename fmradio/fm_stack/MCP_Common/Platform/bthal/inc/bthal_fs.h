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
*   FILE NAME:	bthal_fs.h
*
*   BRIEF:          	This file contains function prototypes, constant and type
*			   	definitions for bthal_fs.c module
*
*   DESCRIPTION:	The bthal_fs.h is the fs API for the BT module. It holds the 
*				file operation that can be performed on files.
*
*   AUTHOR:         AmirY / UziD / YanivR
*
\*******************************************************************************/

#ifndef __BTHAL_FS_H
#define __BTHAL_FS_H


#include "bthal_types.h"
#include "bthal_config.h"
#include "bthal_common.h"
#include "btl_utils.h"
#include "bthal_types.h"


/*-------------------------------------------------------------------------------
 *  BtFsStatus
 *
 *  BTHAL FS Error codes to be added to BtStatus as BT_STATUS_xxx or BTHAL_FS_STATUS_xxx
 */
typedef enum _BtFsStatus
{
	BT_STATUS_HAL_FS_SUCCESS =              				0,
	BT_STATUS_HAL_FS_PENDING =              				1,
	BT_STATUS_HAL_FS_ERROR_GENERAL =   						-1,
	BT_STATUS_HAL_FS_ERROR_NODEVICE =    					-2,
	BT_STATUS_HAL_FS_ERROR_FS_CORRUPTED =					-3,
	BT_STATUS_HAL_FS_ERROR_NOT_FORMATTED =					-4,
	BT_STATUS_HAL_FS_ERROR_OUT_OF_SPACE =					-5,
	BT_STATUS_HAL_FS_ERROR_FSFULL =       					-6,
	BT_STATUS_HAL_FS_ERROR_BADNAME =    					-7,
	BT_STATUS_HAL_FS_ERROR_NOTFOUND =    					-8,
	BT_STATUS_HAL_FS_ERROR_EXISTS =       					-9,
	BT_STATUS_HAL_FS_ERROR_ACCESS_DENIED  =					-10,
	BT_STATUS_HAL_FS_ERROR_NAMETOOLONG =  					-11,
	BT_STATUS_HAL_FS_ERROR_INVALID =      					-12,
	BT_STATUS_HAL_FS_ERROR_DIRNOTEMPTY =  					-13,
	BT_STATUS_HAL_FS_ERROR_FILETOOBIG =   					-14,
	BT_STATUS_HAL_FS_ERROR_NOTAFILE =     					-15,
	BT_STATUS_HAL_FS_ERROR_NUMFD =        					-16,
	BT_STATUS_HAL_FS_ERROR_BADFD =        					-17,
	BT_STATUS_HAL_FS_ERROR_LOCKED =       					-18,
	BT_STATUS_HAL_FS_ERROR_NOT_IMPLEMENTED =				-19,
	BT_STATUS_HAL_FS_ERROR_NOT_INITIALIZED =				-20,	
	BT_STATUS_HAL_FS_ERROR_EOF =							-21,
	BT_STATUS_HAL_FS_ERROR_FILE_NOT_CLOSE =					-22,
	BT_STATUS_HAL_FS_ERROR_READING_FILE =					-23,
	BT_STATUS_HAL_FS_ERROR_OPENING_FILE =					-24,
	BT_STATUS_HAL_FS_ERROR_WRITING_TO_FILE =				-25,
	BT_STATUS_HAL_FS_ERROR_MAX_FILE_HANDLE =				-26,
	BT_STATUS_HAL_FS_ERROR_MAKE_DIR =						-27,
	BT_STATUS_HAL_FS_ERROR_REMOVE_DIR =						-28,
	BT_STATUS_HAL_FS_ERROR_MAX_DIRECTORY_HANDLE =			-29,
	BT_STATUS_HAL_FS_ERROR_INVALID_DIRECTORY_HANDLE_VALUE =	-30,
	BT_STATUS_HAL_FS_ERROR_DIRECTORY_NOT_EMPTY =			-31,
	BT_STATUS_HAL_FS_ERROR_FILE_HANDLE =					-32,
	BT_STATUS_HAL_FS_ERROR_FIND_NEXT_FILE =					-33,
	BT_STATUS_HAL_FS_ERROR_OPEN_FLAG_NOT_SUPPORT=			-34,
	BT_STATUS_HAL_FS_ERROR_DIRECTORY_HANDLE_OUT_OF_RANGE =	-35

} BtFsStatus;


/*-------------------------------------------------------------------------------
 *  BthalFsType
 *
 *  whether a file or a directory
 */
typedef enum _BthalFsType
{
	BTHAL_FS_FILE,
	BTHAL_FS_DIR
} BthalFsType;


/*-------------------------------------------------------------------------------
 *  BthalFsPermission
 *
 *  bit masks for the access permissions on a file or directory.
 */
typedef BTHAL_U32 BthalFsPermission;

#define BTHAL_FS_PERM_READ      0x00000001      /* Read Permission  - applies to file or folder */   
#define BTHAL_FS_PERM_WRITE     0x00000002      /* Write Permission - applies to file or folder */
#define BTHAL_FS_PERM_DELETE    0x00000004      /* Delete Permission  - applies to file only */


/*-------------------------------------------------------------------------------
 *  BthalFsStat structure
 *
 *  struct to hold data returned by stat functions.
 */
typedef struct _BthalFsStat
{	
	BthalFsType		        type;				/* file / dir */
	BTHAL_U32		        size;				/* in bytes */
	BTHAL_BOOL		        isReadOnly;			/* whether read-only file */
	BtlDateAndTimeStruct	mTime;				/* modified time */
	BtlDateAndTimeStruct	aTime;				/* accessed time */
	BtlDateAndTimeStruct	cTime;				/* creation time */
	BTHAL_U16               userPerm; 	        /* applied user permission access */
    BTHAL_U16               groupPerm; 	        /* applied group permission access */
    BTHAL_U16               otherPerm; 	        /* applied other permission access */

} BthalFsStat;


/*-------------------------------------------------------------------------------
 *  BthalFsSeekOrigin 
 *
 *  origin of a seek operation.
 */
typedef enum _BthalFsSeekOrigin
{	
	BTHAL_FS_CUR,	/* from current position */
	BTHAL_FS_END,	/* from end of file */
	BTHAL_FS_START	/* from start of file */
} BthalFsSeekOrigin;


/*-------------------------------------------------------------------------------
 *  BthalFsFileDesc
 * 
 *  file descriptor, instead of FILE 
 */
typedef BTHAL_S32 BthalFsFileDesc;


/*-------------------------------------------------------------------------------
 *  BthalFsFileDesc
 * 
 *  directory descriptor
 */
typedef BTHAL_U32 BthalFsDirDesc;

/*-------------------------------------------------------------------------------
 *  BthalFsStatFlags
 * 
 *   bit masks for BTHAL_FS_Stat flags
 */
typedef BTHAL_U32 BthalFsStatFlags;

#define	BTHAL_FS_S_MTIME	    0x00000001		/* Time of last modification of file */
#define	BTHAL_FS_S_ATIME	    0x00000002		/* Time of last access of file */
#define	BTHAL_FS_S_CTIME	    0x00000004		/* Time of creation of file */
#define	BTHAL_FS_S_SIZE		    0x00000008		/* Size of the file in bytes */
#define	BTHAL_FS_S_USERPERM	    0x00000010		/* convey the user access permission */
#define	BTHAL_FS_S_GROUPPERM	0x00000020		/* convey the group access permission */
#define	BTHAL_FS_S_OTHERPERM	0x00000040		/* convey the other access permission */

#define BTHAL_FS_S_ALL_FLAG	    0x000000FF		/* enable all the flag */ 

/*-------------------------------------------------------------------------------
 *  BthalFsOpenFlags
 * 
 *   bit masks for BTHAL_FS_Open flags
 */
typedef BTHAL_U32 BthalFsOpenFlags;

#define	BTHAL_FS_O_WRONLY	0x00000001		/* write only */
#define	BTHAL_FS_O_CREATE	0x00000002		/* create only */
#define	BTHAL_FS_O_TRUNC	0x00000004		/* if file already exists and it is opened for writing, its length is truncated to zero. */
#define	BTHAL_FS_O_APPEND   0x00000008		/* append to EOF */
#define	BTHAL_FS_O_RDONLY	0x00000010		/* read only */
#define	BTHAL_FS_O_RDWR     0x00000020		/* read & write */
#define BTHAL_FS_O_EXCL		0x00000040		/* generate error if BTHAL_FS_O_CREATE is also specified and the file already exists.*/
#define BTHAL_FS_O_BINARY	0x00000080		/* binary file */
#define BTHAL_FS_O_TEXT		0x00000100		/* text file */


#define BTHAL_FS_INVALID_FILE_DESC			-1
#define	BTHAL_FS_INVALID_DIRECTORY_DESC		-1

/********************************************************************************
 *
 * Function declarations
 *
 *******************************************************************************/

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Init()
 *
 * Brief:  		Init FS module.
 *
 * Description:	Initiate the fs module and prepare it for fs operations.
 *
 * Type:
 *		blocking or non-blocking
 *
 * Parameters:
 *		callback [in] - callback function.
 *
 * Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 */
BtFsStatus BTHAL_FS_Init( BthalCallBack	callback);

/*-------------------------------------------------------------------------------
 * BTHAL_FS_DeInit()
 *
 * Brief:  		Deinit FS module.
 *
 * Description:	Deinit FS module.
 *
 * Type:
 *		blocking
 *
 * Parameters:
 *		None.
 *
 * Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 */
BtFsStatus BTHAL_FS_DeInit( void );

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Open()
 *
 * Brief:  		Open's a file.
 *
 * Description:	the file name should include all file-system and drive specification as
 *			   	used by convention in the target platform. from this name the type of file
 *			   	system should be derived (e.g. FFS, SD, hard disk, ...  )
 *			   	pFd is a pointer to the newly opened file 
 *
 * Type:
 *		blocking or non-blocking
 *
 * 	Parameters:
 *		fullpathfilename [in] - filename.
 *		flags [in] - file open flag e.g read, write ...
 *		fd [out] - file handle.
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Open(const BthalUtf8* fullPathFileName, BthalFsOpenFlags flags, BthalFsFileDesc *fd);

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Close()
 *
 * Brief:  		Close file.
 *
 * Description: Close file.
 *
 * Type:
 *		blocking
 *
 * 	Parameters:
 *		fd [in] - file handle.
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Close( const BthalFsFileDesc fd );

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Read()
 *
 * Brief:  		Read from a file.
 *
 * Description: Read from a file.
 *
 * Type:
 *		blocking
 *
 * Parameters:
 *		fd [in] - file handle.
 *		buf [in] - address of buffer to read data into.
 *		nSize [in] - maximum number of byte to be read.
 *		numRead [out] - points to actual number of bytes that have been read.
 *
 * Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Read ( const BthalFsFileDesc fd, void* buf, BTHAL_U32 nSize, BTHAL_U32 *numRead );

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Write()
 *
 * Brief:  		Write to a file.
 *
 * Description: Write to a file.
 *
 * Type:
 *		blocking
 *
 * Parameters:
 *		fd [in] - file handle.
 *		buf [in] - address of buffer to write data from.
 *		nSize [in] - maximum number of byte to be read.
 *		numWritten [out] - points to actual number of bytes that have been written
 *
 * Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Write( const BthalFsFileDesc fd, void* buf, BTHAL_U32 nSize, BTHAL_U32 *numWritten );

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Flush()
 *
 * Brief:  		flush write buffers from memory to file.
 *
 * Description: flush write buffers from memory to file.
 *
 *
 * Type:
 *		blocking
 *
 * Parameters:
 *		fd [in] - file handle.
 *
 * Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Flush( const BthalFsFileDesc fd );

/*-------------------------------------------------------------------------------
 *  BTHAL_FS_Stat()
 *
 * Brief:  		get information from file / Directory.
 *
 * Description: get information from file / Directory.
 *
 *
 * Type:
 *		blocking
 *
 * Parameters:
 *		fullPathName [in] - full path name directory / file.
 *
 *		BthalFsStat [out] - points to BthalFsStat structure that save the info: size, type ...  
 *							
 *
 * Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
 
BtFsStatus BTHAL_FS_Stat( const BthalUtf8* fullPathName, BthalFsStat* fileStat );

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Tell()
 *
 * Brief:  		gets the current position of a file pointer.
 *
 * Description: gets the current position of a file pointer.
 *
 * Type:
 *		blocking
 *
 * Parameters:
 *		fd [in] - file handle.
 *		curPosition [out] - points to current pointer locate in a file. 
 *							
 *
 * Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Tell( const BthalFsFileDesc fd, BTHAL_U32 *curPosition );

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Seek()
 *
 * Brief:  		moves the file pointer to a specified location.
 *
 * Description: moves the file pointer to a specified location.
 *
 * Type:
 *		blocking
 *
 * Parameters:
 *		fd [in] - file handle.
 *		offset [in] - number of bytes from origin
 *		from [in] - initial position origin.							
 *
 * Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Seek( const BthalFsFileDesc fd, BTHAL_S32 offset, 
						 BthalFsSeekOrigin from );

 /*-------------------------------------------------------------------------------
 * BTHAL_FS_Mkdir()
 *
 * Brief:  		make directory.
 *
 * Description: make directory.
 *			   	both the backslash and the forward slash are valid path delimiters
 *	Type:
 *		blocking
 *
 * 	Parameters:
 *		dirFullPathName [in] - directory name.
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Mkdir( const BthalUtf8 *dirFullPathName ); 

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Rmdir()
 *
 * Brief:  		remove directory.
 *
 * Description: remove directory.
 *			   	both the backslash and the forward slash are valid path delimiters
 *
 *	Type:
 *		blocking
 *
 * 	Parameters:
 *		dirFullPathName [in] - directory name.
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Rmdir( const BthalUtf8 *dirFullPathName );
 
/*-------------------------------------------------------------------------------
 * BTHAL_FS_OpenDir()
 *
 * Brief:  		Opens directory for reading.
 *
 * Description: Opens directory for reading.
 *
 *	Type:
 *		blocking
 *
 * 	Parameters:
 *		dirFullPathName [in] - pointer to directory name.
 *		dirDesc [out] - points to Directory handle  
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		BT_STATUS_HAL_FS_ERROR_INVALID_DIRECTORY_HANDLE_VALUE -  Operation failed.
 */
BtFsStatus BTHAL_FS_OpenDir( const BthalUtf8 *dirFullPathName, BthalFsDirDesc *dirDesc );

/*-------------------------------------------------------------------------------
 * BTHAL_FS_ReadDir()
 *
 * Brief:  		get next file name in directory.
 *
 * Description: get first/next file name in a directory. 
 *				return the full path of the file
 *
 *	Type:
 *		blocking
 *
 * 	Parameters:
 *		dirDesc [in] - points to Directory handle .
 *		fileName [out] - return the full path name.  
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		BT_STATUS_HAL_FS_ERROR_FIND_NEXT_FILE	 -  Operation failed.
 */

BtFsStatus BTHAL_FS_ReadDir( const BthalFsDirDesc dirDesc, BthalUtf8 **fileName ); 

/*-------------------------------------------------------------------------------
 * BTHAL_FS_CloseDir()
 *
 * Brief:  		close a directory for reading.
 *
 * Description: close a directory for reading. 
 *
 *	Type:
 *		blocking
 *
 * 	Parameters:
 *		dirDesc [in] - points to Directory handle .
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_CloseDir( const BthalFsDirDesc dirDesc ); 

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Rename()
 *
 * Brief:  		renames a file or a directory.
 *
 * Description: renames a file or a directory. 
 *
 *	Type:
 *		blocking
 *
 * 	Parameters:
 *		fullPathOldName [in] - points to old name .
 *		fullPathNewName [in] - points to new name .
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Rename(const BthalUtf8 *fullPathOldName, const BthalUtf8 *fullPathNewName );

/*-------------------------------------------------------------------------------
 * BTHAL_FS_Remove()
 *
 * Brief:  		removes a file.
 *
 * Description: removes a file. 
 *	
 *	Type:
 *		blocking
 *
 * 	Parameters:
 *		fullPathFileName [in] - points to file name .
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Remove( const BthalUtf8 *fullPathFileName );

/*-------------------------------------------------------------------------------
 * BTHAL_FS_IsAbsoluteName()
 *
 * Brief:  		checks if the file name is full path or absolute path.
 *
 * Description: This function checks whether a given file name has an absolute path name.
 *
 * Type:
 *                Synchronous
 *
 * Parameters:
 *                fileName [in] - name of the file
 *
 *                isAbsolute [out] - result whether the name contains an absolute full path . 
 *                                        In case of failure to check it is set to FALSE
 *
 * Returns:
 *                BT_STATUS_HAL_FS_SUCCESS - function succeeded to check if full path
 *
 *                BT_STATUS_HAL_FS_ERROR_NOTAFILE -  failed to check. NULL name provided
 */
BtFsStatus BTHAL_FS_IsAbsoluteName( const BthalUtf8 *fileName, BTHAL_BOOL *isAbsolute );

#endif /* __BTHAL_FS_H */




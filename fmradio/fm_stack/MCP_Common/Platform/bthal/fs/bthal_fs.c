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
*   FILE NAME:      bthal_fs.c
*
*   DESCRIPTION:    This file contain implementation of file system in WIN
*
*   AUTHOR:         Yaniv Rabin
*
\*******************************************************************************/


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "bthal_fs.h"
#include "mcp_hal_fs.h"
#include <assert.h>
#include "mcp_hal_memory.h"

static BtFsStatus	FsConvertMcpHalFsStatusToBthalFsStatus(McpHalFsStatus mcpHalStatus);

/********************************************************************************
 *
 * Function definitions
 *
 *******************************************************************************/
/*-------------------------------------------------------------------------------
 * BTHAL_FS_Init()
 *
 *	Synopsis:  int FS set zero to all FileStructureArray and DirectoryStructureArray
 * 
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 */
BtFsStatus BTHAL_FS_Init( BthalCallBack	callback )
{
	UNUSED_PARAMETER(callback);

	return BT_STATUS_HAL_FS_SUCCESS;
}

/*-------------------------------------------------------------------------------
 * BTHAL_FS_DeInit()
 *
 *	Synopsis:  
 * 
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 */
BtFsStatus BTHAL_FS_DeInit( void )
{
	return BT_STATUS_HAL_FS_SUCCESS;
}

/*---------------------------------------------------------------------------
 *            BTHAL_FS_Open
 *---------------------------------------------------------------------------
 *
 * Synopsis:  the file name should include all file-system and drive specification as
 *			   used by convention in the target platform.
 *
 * Return:    BT_STATUS_HAL_FS_SUCCESS if success, BT_STATUS_HAL_FS_ERROR_OPENING_FILE otherwise
 *
 */
BtFsStatus BTHAL_FS_Open(const BthalUtf8* fullPathFileName, BthalFsOpenFlags flags, BthalFsFileDesc *fd)
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Open(fullPathFileName, flags, fd));
}

/*---------------------------------------------------------------------------
 *            BTHAL_FS_Close
 *---------------------------------------------------------------------------
 *
 * Synopsis:  close file
 *
 * Return:    BT_STATUS_HAL_FS_SUCCESS if success,
 *			  other - if failed.
 *
 */
BtFsStatus BTHAL_FS_Close( const BthalFsFileDesc fd )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Close(fd));
}

/*---------------------------------------------------------------------------
 *            BTHAL_FS_Read
 *---------------------------------------------------------------------------
 *
 * Synopsis:  read file
 *
 * Return:    BT_STATUS_HAL_FS_SUCCESS if success,
 *			  other - if failed.
 *
 */
BtFsStatus BTHAL_FS_Read ( const BthalFsFileDesc fd, void* buf, BTHAL_U32 nSize, BTHAL_U32 *numRead )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Read(fd, buf, nSize, numRead));
}

/*---------------------------------------------------------------------------
 *            BTHAL_FS_Write
 *---------------------------------------------------------------------------
 *
 * Synopsis:  write file
 *
 * 	Returns:	BT_STATUS_HAL_FS_SUCCESS - if successful,
 *				other - if failed.
 */
BtFsStatus BTHAL_FS_Write( const BthalFsFileDesc fd, void* buf, BTHAL_U32 nSize, BTHAL_U32 *numWritten )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Write(fd, buf, nSize, numWritten));
}

/*---------------------------------------------------------------------------
 *            BTHAL_FS_Tell
 *---------------------------------------------------------------------------
 *
 * Synopsis:  gets the current position of a file pointer.
 *
 * 	Returns:	BT_STATUS_HAL_FS_SUCCESS - if successful,
 *				other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Tell( const BthalFsFileDesc fd, BTHAL_U32 *curPosition )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Tell(fd, curPosition));
}

/*---------------------------------------------------------------------------
 *            BTHAL_FS_Seek
 *---------------------------------------------------------------------------
 *
 * Synopsis:  moves the file pointer to a specified location.
 *
 * 	Returns:	BT_STATUS_HAL_FS_SUCCESS - if successful,
 *				other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Seek( const BthalFsFileDesc fd, BTHAL_S32 offset, BthalFsSeekOrigin from )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Seek(fd, offset, from));
}


/*---------------------------------------------------------------------------
 *            BTHAL_FS_Flush
 *---------------------------------------------------------------------------
 *
 * Synopsis:  flush write buffers from memory to file
 *
 * 	Returns:	BT_STATUS_HAL_FS_SUCCESS - if successful,
 *				BT_STATUS_HAL_FS_ERROR_GENERAL - if failed.
 */
BtFsStatus BTHAL_FS_Flush( const BthalFsFileDesc fd )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Flush(fd));
}


/*---------------------------------------------------------------------------
 *            BTHAL_FS_Stat
 *---------------------------------------------------------------------------
 *
 * Synopsis:  get information of a file or folder - name, size, type, 
 *            created/modified/accessed time, and Read/Write/Delete         . 
 *            access permissions.
 *
 * Returns:	BT_STATUS_HAL_FS_SUCCESS - if successful,
 *				other -  Operation failed.
 */
BtFsStatus BTHAL_FS_Stat( const BthalUtf8* fullPathName, BthalFsStat* fileStat )
{
	McpHalFsStat	mcpStat;

	BtFsStatus btFsStatus = FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Stat(fullPathName, &mcpStat));

	if (btFsStatus == BT_STATUS_HAL_FS_SUCCESS)
	{
		assert(sizeof(BthalFsStat) == sizeof(McpHalFsStat));
		
		MCP_HAL_MEMORY_MemCopy(fileStat, &mcpStat, sizeof(mcpStat));
	}
	
	return btFsStatus;
}

/*---------------------------------------------------------------------------
 *            BTHAL_FS_Mkdir
 *---------------------------------------------------------------------------
 *
 * Synopsis:  make dir
 *
 * 	Returns:	BT_STATUS_HAL_FS_SUCCESS - if successful,
 *				other - if failed 
 */
BtFsStatus BTHAL_FS_Mkdir( const BthalUtf8 *dirFullPathName ) 
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Mkdir(dirFullPathName));
}

/*---------------------------------------------------------------------------
 *            BTHAL_FS_Rmdir
 *---------------------------------------------------------------------------
 *
 * Synopsis:  remove dir
 *
 * 	Returns:	BT_STATUS_HAL_FS_SUCCESS - if successful,
 *				other - if failed 
 */
BtFsStatus BTHAL_FS_Rmdir( const BthalUtf8 *dirFullPathName )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Rmdir(dirFullPathName));
}
/*---------------------------------------------------------------------------
 *            BTHAL_FS_OpenDir
 *---------------------------------------------------------------------------
 *
 * Synopsis:  open a directory for reading,
 *
 * 	Returns:	BT_STATUS_HAL_FS_SUCCESS - if successful,
 *				BT_STATUS_HAL_FS_ERROR_INVALID_DIRECTORY_HANDLE_VALUE - if failed 
 */
BtFsStatus BTHAL_FS_OpenDir( const BthalUtf8 *dirFullPathName, BthalFsDirDesc *dirDesc )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_OpenDir(dirFullPathName, dirDesc));
}

/*-------------------------------------------------------------------------------
 * BTHAL_FS_ReadDir()
 *
 *	Synopsis:  get first/next file name in a directory. return the full path of the file
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		BT_STATUS_HAL_FS_ERROR_FIND_NEXT_FILE -  Operation failed.
 */

BtFsStatus BTHAL_FS_ReadDir( const BthalFsDirDesc dirDesc, BthalUtf8 **fileName )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_ReadDir(dirDesc, fileName));
}

/*-------------------------------------------------------------------------------
 * BTHAL_FS_CloseDir()
 *
 *	Synopsis:  set the busy flag to 0 -> free the cell. 
 *
 * 	Parameters:
 *		dirDesc [in] - points to Directory handle .
 *
 * 	Returns:
 *		BT_STATUS_HAL_FS_SUCCESS - Operation is successful.
 *
 *		BT_STATUS_HAL_FS_ERROR_GENERAL - -  Operation failed.
 *
 */
BtFsStatus BTHAL_FS_CloseDir( const BthalFsDirDesc dirDesc )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_CloseDir(dirDesc));
}

/*-------------------------------------------------------------------------------
 *
 *  BTHAL_FS_Rename
 *
 *  renames a file or a directory
 */
BtFsStatus BTHAL_FS_Rename(const BthalUtf8 *fullPathOldName, const BthalUtf8 *fullPathNewName )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Rename(fullPathOldName, fullPathNewName));
}

/*-------------------------------------------------------------------------------
 *
 *  BTHAL_FS_Remove
 *
 *  removes a file
 */ 
BtFsStatus BTHAL_FS_Remove( const BthalUtf8 *fullPathFileName )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_Remove(fullPathFileName));
}

/*-------------------------------------------------------------------------------
 *
 * BTHAL_FS_IsAbsoluteName()
 *
 * This function checks whether a given file name has an absolute path name
 */
BtFsStatus BTHAL_FS_IsAbsoluteName( const BthalUtf8 *fileName, BTHAL_BOOL *isAbsolute )
{
	return FsConvertMcpHalFsStatusToBthalFsStatus(MCP_HAL_FS_IsAbsoluteName(fileName, isAbsolute));
}

BtFsStatus	FsConvertMcpHalFsStatusToBthalFsStatus(McpHalFsStatus mcpHalStatus)
{
	switch (mcpHalStatus)
	{
		case MCP_HAL_FS_STATUS_SUCCESS: 		return BT_STATUS_HAL_FS_SUCCESS;
		case MCP_HAL_FS_STATUS_PENDING: 		return BT_STATUS_HAL_FS_PENDING;
		case MCP_HAL_FS_STATUS_ERROR_GENERAL: 		return BT_STATUS_HAL_FS_ERROR_GENERAL;
		case MCP_HAL_FS_STATUS_ERROR_NODEVICE: 		return BT_STATUS_HAL_FS_ERROR_NODEVICE;
		case MCP_HAL_FS_STATUS_ERROR_FS_CORRUPTED: 		return BT_STATUS_HAL_FS_ERROR_FS_CORRUPTED;
		case MCP_HAL_FS_STATUS_ERROR_NOT_FORMATTED: 		return BT_STATUS_HAL_FS_ERROR_NOT_FORMATTED;
		case MCP_HAL_FS_STATUS_ERROR_OUT_OF_SPACE: 		return BT_STATUS_HAL_FS_ERROR_OUT_OF_SPACE;
		case MCP_HAL_FS_STATUS_ERROR_FSFULL: 		return BT_STATUS_HAL_FS_ERROR_FSFULL;
		case MCP_HAL_FS_STATUS_ERROR_BADNAME: 		return BT_STATUS_HAL_FS_ERROR_BADNAME;
		case MCP_HAL_FS_STATUS_ERROR_NOTFOUND: 		return BT_STATUS_HAL_FS_ERROR_NOTFOUND;
		case MCP_HAL_FS_STATUS_ERROR_EXISTS: 		return BT_STATUS_HAL_FS_ERROR_EXISTS;
		case MCP_HAL_FS_STATUS_ERROR_ACCESS_DENIED: 		return BT_STATUS_HAL_FS_ERROR_ACCESS_DENIED; 
		case MCP_HAL_FS_STATUS_ERROR_NAMETOOLONG: 		return BT_STATUS_HAL_FS_ERROR_NAMETOOLONG;
		case MCP_HAL_FS_STATUS_ERROR_INVALID: 		return BT_STATUS_HAL_FS_ERROR_INVALID;
		case MCP_HAL_FS_STATUS_ERROR_DIRNOTEMPTY: 		return BT_STATUS_HAL_FS_ERROR_DIRNOTEMPTY;
		case MCP_HAL_FS_STATUS_ERROR_FILETOOBIG: 		return BT_STATUS_HAL_FS_ERROR_FILETOOBIG;
		case MCP_HAL_FS_STATUS_ERROR_NOTAFILE: 		return BT_STATUS_HAL_FS_ERROR_NOTAFILE;
		case MCP_HAL_FS_STATUS_ERROR_NUMFD: 		return BT_STATUS_HAL_FS_ERROR_NUMFD;
		case MCP_HAL_FS_STATUS_ERROR_BADFD: 		return BT_STATUS_HAL_FS_ERROR_BADFD;
		case MCP_HAL_FS_STATUS_ERROR_LOCKED: 		return BT_STATUS_HAL_FS_ERROR_LOCKED;
		case MCP_HAL_FS_STATUS_ERROR_NOT_IMPLEMENTED: 		return BT_STATUS_HAL_FS_ERROR_NOT_IMPLEMENTED;
		case MCP_HAL_FS_STATUS_ERROR_NOT_INITIALIZED: 		return BT_STATUS_HAL_FS_ERROR_NOT_INITIALIZED;
		case MCP_HAL_FS_STATUS_ERROR_EOF: 		return BT_STATUS_HAL_FS_ERROR_EOF;
		case MCP_HAL_FS_STATUS_ERROR_FILE_NOT_CLOSE: 		return BT_STATUS_HAL_FS_ERROR_FILE_NOT_CLOSE;
		case MCP_HAL_FS_STATUS_ERROR_READING_FILE: 		return BT_STATUS_HAL_FS_ERROR_READING_FILE;
		case MCP_HAL_FS_STATUS_ERROR_OPENING_FILE: 		return BT_STATUS_HAL_FS_ERROR_OPENING_FILE;
		case MCP_HAL_FS_STATUS_ERROR_WRITING_TO_FILE: 		return BT_STATUS_HAL_FS_ERROR_WRITING_TO_FILE;
		case MCP_HAL_FS_STATUS_ERROR_MAX_FILE_HANDLE: 		return BT_STATUS_HAL_FS_ERROR_MAX_FILE_HANDLE;
		case MCP_HAL_FS_STATUS_ERROR_MAKE_DIR: 		return BT_STATUS_HAL_FS_ERROR_MAKE_DIR;
		case MCP_HAL_FS_STATUS_ERROR_REMOVE_DIR: 		return BT_STATUS_HAL_FS_ERROR_REMOVE_DIR;
		case MCP_HAL_FS_STATUS_ERROR_MAX_DIRECTORY_HANDLE: 		return BT_STATUS_HAL_FS_ERROR_MAX_DIRECTORY_HANDLE;
		case MCP_HAL_FS_STATUS_ERROR_INVALID_DIRECTORY_HANDLE_VALUE: 		return BT_STATUS_HAL_FS_ERROR_INVALID_DIRECTORY_HANDLE_VALUE;
		case MCP_HAL_FS_STATUS_ERROR_DIRECTORY_NOT_EMPTY: 		return BT_STATUS_HAL_FS_ERROR_DIRECTORY_NOT_EMPTY;
		case MCP_HAL_FS_STATUS_ERROR_FILE_HANDLE: 		return BT_STATUS_HAL_FS_ERROR_FILE_HANDLE;
		case MCP_HAL_FS_STATUS_ERROR_FIND_NEXT_FILE: 		return BT_STATUS_HAL_FS_ERROR_FIND_NEXT_FILE;
		case MCP_HAL_FS_STATUS_ERROR_OPEN_FLAG_NOT_SUPPORT: 		return BT_STATUS_HAL_FS_ERROR_OPEN_FLAG_NOT_SUPPORT;				
		case MCP_HAL_FS_STATUS_ERROR_DIRECTORY_HANDLE_OUT_OF_RANGE: 		return BT_STATUS_HAL_FS_ERROR_DIRECTORY_HANDLE_OUT_OF_RANGE;
		default: return BT_STATUS_HAL_FS_ERROR_GENERAL;
	}
}


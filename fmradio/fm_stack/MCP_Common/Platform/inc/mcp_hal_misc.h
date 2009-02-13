/*
 * TI's FM Stack
 *
 * Copyright 2001-2008 Texas Instruments, Inc. - http://www.ti.com/
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and  
 * limitations under the License.
 */
/*******************************************************************************\
*
*   FILE NAME:      mcp_hal_misc.h
*
*   BRIEF:          This file defines Miscellaneous HAL utilities that are not part of any specific functionality (e.g, strings or memory).
*
*   DESCRIPTION:    General
*                   
*   AUTHOR:         Udi Ron
*
\*******************************************************************************/

#ifndef __MCP_HAL_MISC_H
#define __MCP_HAL_MISC_H


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
/* Included for sprintf */
#include <stdio.h>

#include "mcp_hal_types.h"
#include "mcp_hal_config.h"
#include "mcp_hal_defs.h"


/*-------------------------------------------------------------------------------
 * MCP_HAL_MISC_Srand()
 *
 * Brief:  
 *    Sets a random starting point
 *
 * Description:
 *	The function sets the starting point for generating a series of pseudorandom integers. To reinitialize 
 *	the generator, use 1 as the seed argument. Any other value for seed sets the generator to a random 
 *	starting point. MCP_HAL_MISC_Rand() retrieves the pseudorandom numbers that are generated. 
 *	Calling MCP_HAL_MISC_Rand() before any call to srand generates the same sequence as calling 
 *	MCP_HAL_MISC_Srand() with seed passed as 1
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		seed [in] - Seed for random-number generation
 *
 * Returns:
 *     void
 */
void MCP_HAL_MISC_Srand(McpUint seed);

/*-------------------------------------------------------------------------------
 * MCP_HAL_MISC_Rand()
 *
 * Brief:  
 *     Generates a pseudorandom number
 *
 * Description:
 *    Returns a pseudorandom integer in the range 0 to 65535. 
 *
 *	Use the MCP_HAL_MISC_Srand() function to seed the pseudorandom-number generator before calling rand.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *		void.
 *
 * Returns:
 *     Returns a pseudorandom number, as described above.
 */
McpU16 MCP_HAL_MISC_Rand(void);

/*-------------------------------------------------------------------------------
 * MCP_HAL_MISC_AtoU32Utf8()
 *
 * Brief: 
 *    Convert strings to double  (same as ANSI C atoi)
 *
 * Description: 
 *    Convert strings to double  (same as ANSI C atoi)
 * 	  This function converts a character string to an integer value.
 *    The function do not recognize decimal points or exponents.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *     string [in] - String to be converted which has the following form:
 *
 *    [whitespace][sign][digits]
 *
 *	  Where whitespace consists of space and/or tab characters, which are ignored; 
 *	  sign is either plus (+) or minus (–); and digits are one or more decimal digits. 
 *
 * Returns:
 *	  A U32 value produced by interpreting the input characters as a number.
 *    The return value is 0 if the input cannot be converted to a value of that type. 
 *	  The return value is undefined in case of overflow.
 */
McpU32 MCP_HAL_MISC_AtoU32Utf8(const McpUtf8 *string);

/*-------------------------------------------------------------------------------
 * MCP_HAL_MISC_Sprintf()
 *
 * Brief: 
 *    Write formatted data to a string  (same as ANSI C sprintf)
 *
 * Description: 
 *   	The MCP_HAL_MISC_Sprintf() function formats and stores a series of characters and values in buffer. 
 *	Each argument (if any) is converted and output according to the corresponding format specification 
 *	in format. The format consists of ordinary characters and has the same form and function as the 
 *	format argument for printf. A null character is appended after the last character written. 
 *	If copying occurs between strings that overlap, the behavior is undefined.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *     buffer [out] - Storage location for output
 *
 *	format [in] - Format-control string
 *
 *	argument [in] - Optional arguments
 *
 * Returns:
 *	  Returns the number of bytes stored in buffer, not counting the terminating null character. 
 */
McpInt MCP_HAL_MISC_Sprintf(char *buffer, const char *format, ...);

/*
	Using a macro to be able to use the ANSI C sprintf function as is, without
	having to handle argument formatting ourselves.
*/
#define	MCP_HAL_MISC_Sprintf		sprintf 

/*-------------------------------------------------------------------------------
 * MCP_HAL_MISC_AtoU32()
 *
 * Brief: 
 *    Convert strings to double  (same as ANSI C atoi)
 *
 * Description: 
 *    Convert strings to double  (same as ANSI C atoi)
 * 	  The MCP_HAL_MISC_AtoU32 function converts a character string to an integer value.
 *    The function do not recognize decimal points or exponents.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *     string [in] - String to be converted which has the following form:
 *
 *    [whitespace][sign][digits]
 *
 *	  Where whitespace consists of space and/or tab characters, which are ignored; 
 *	  sign is either plus (+) or minus (–); and digits are one or more decimal digits. 
 *
 * Returns:
 *	  A U32 value produced by interpreting the input characters as a number.
 *    The return value is 0 if the input cannot be converted to a value of that type. 
 *	  The return value is undefined in case of overflow.
 */
McpU32 MCP_HAL_MISC_AtoU32(const char *string);


/*---------------------------------------------------------------------------
 * MCP_HAL_MISC_Assert()
 *
 * Brief: 
 *     Called by the stack to indicate that an assertion failed. 
 *
 * Description: 
 *     Called by the stack to indicate that an assertion failed. MCP_HAL_MISC_Assert
 *     should display the failed expression and the file and line number
 *     where the expression occurred.
 *
 * Type:
 *		Synchronous
 *
 * Parameters:
 *     expression [in] - A string containing the failed expression.
 *
 *     file [in] - A string containing the file in which the expression occurred.
 *
 *     line [in] - The line number that tested the expression.
 *
 * Returns:
 *		void.
 */
void MCP_HAL_MISC_Assert(const char *expression, const char *file, McpU16 line);

#endif	/* __MCP_HAL_MISC_H */


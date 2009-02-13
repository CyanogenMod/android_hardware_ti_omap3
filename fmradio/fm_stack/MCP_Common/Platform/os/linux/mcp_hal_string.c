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
*   FILE NAME:      mcp_hal_string.c
*
*   DESCRIPTION:    This file implements the MCP HAL string utilities for Windows.
*
*   AUTHOR:         Udi Ron
*
\*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mcp_hal_string.h"
#include "fmc_defs.h"

/****************************************************************************
 *
 * Local Prototypes
 *
 ***************************************************************************/

/****************************************************************************
 *
 * Public Functions
 *
 ***************************************************************************/

McpU8 MCP_HAL_STRING_StrCmp(const char *Str1, const char *Str2)
{
	while (*Str1 == *Str2) {
        if (*Str1 == 0 || *Str2 == 0) {
            break;
        }
        Str1++;
        Str2++;
    }

    /* Return zero on success, just like the ANSI strcmp() */
    if (*Str1 == *Str2)
        return 0;

    return 1;
}

McpU8 MCP_HAL_STRING_StriCmp(const char *Str1, const char *Str2)
{	
	const McpU32 lowerToUpperDiff = 'a' - 'A';
	
	while (*Str1 != 0 || *Str2 != 0)
	{
		if ( 	(*Str1==*Str2) || 
			((*Str1 >= 'A') && (*Str1 <= 'Z') && ((char)(*Str1 + lowerToUpperDiff) == *Str2)) ||
			((*Str2 >= 'A') && (*Str2 <= 'Z') && ((char)(*Str2 + lowerToUpperDiff) == *Str1)))
		{
			Str1++;
			Str2++;
		}
		else
			return 1;
	}
	if (*Str1 == *Str2) /* both pointers reached NULL */
	        return 0;
	return 1;
}

McpU16 MCP_HAL_STRING_StrLen(const char *Str)
{
	const char  *cp = Str;

	while (*cp != 0) cp++;

	return (McpU16)(cp - Str);
}

char* MCP_HAL_STRING_StrCpy(char* StrDest, const char *StrSource)
{
	return strcpy(StrDest, StrSource);
}

char* MCP_HAL_STRING_StrnCpy(char* StrDest, const char *StrSource, McpU32 Count)
{
	return strncpy(StrDest, StrSource, Count);
}

char *MCP_HAL_STRING_StrCat(char *strDest, const char *strSource)
{
	return strcat(strDest, strSource);
}

char *MCP_HAL_STRING_StrrChr(const char *Str, McpS32 c)
{
	return strrchr(Str, c);
}

McpS32 MCP_HAL_STRING_StriCmpUtf8(const McpUtf8 *Str1, const McpUtf8 *Str2)
{	
	//Not yet implemented
	FMC_ASSERT(0);
}

McpUtf16 MCP_HAL_STRING_StrLenUtf8(const McpUtf8 *Str)
{
	//Not yet implemented
	FMC_ASSERT(0);
}

McpUtf8 MCP_HAL_STRING_StrCmpUtf8(const McpUtf8 *Str1, const McpUtf8 *Str2)
{
	while (*Str1 == *Str2) {
        if (*Str1 == 0 || *Str2 == 0) {
            break;
        }
        Str1++;
        Str2++;
    }

    /* Return zero on success, just like the ANSI strcmp() */
    if (*Str1 == *Str2)
        return 0;

    return 1;
}
McpUtf8* MCP_HAL_STRING_StrnCpyUtf8(McpUtf8* StrDest, const McpUtf8 *StrSource, McpU32 Count)
{
	//Not yet implemented
	FMC_ASSERT(0);
}

McpUtf8 *MCP_HAL_STRING_StrCatUtf8(McpUtf8 *strDest, const McpUtf8 *strSource)
{
	//Not yet implemented
	FMC_ASSERT(0);
}

McpUtf8 *MCP_HAL_STRING_StrrChrUtf8(const McpUtf8 *Str, McpU32 c)
{
	//Not yet implemented
	FMC_ASSERT(0);
}

McpUtf8* MCP_HAL_STRING_StrCpyUtf8(McpUtf8* StrDest, const McpUtf8 *StrSource)
{
	//Not yet implemented
	FMC_ASSERT(0);
}


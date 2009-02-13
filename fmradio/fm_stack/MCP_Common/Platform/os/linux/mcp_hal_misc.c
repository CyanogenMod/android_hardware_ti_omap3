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
*   FILE NAME:      mcp_hal_utils.c
*
*   DESCRIPTION:    This file implements the MCP HAL Utils for Windows.
*
*   AUTHOR:         Udi Ron
*
\*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "mcp_hal_misc.h"

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
void MCP_HAL_MISC_Srand(McpUint seed)
{
	srand(seed);
}

McpU16 MCP_HAL_MISC_Rand(void)
{
	return (McpU16)(rand() % MCP_U16_MAX);
}

McpU32 MCP_HAL_MISC_AtoU32(const char *string)
{
	int sign = 1;
	int counter = 0;
	int number = 0;
	int tmp;
	int length;
	const char  *cp = string;

	if (string == 0)
		return 0; 

	if ('-'==string[0]) {sign=-1; counter=1;}

	while (*cp != 0) cp++;

	length = (McpU32)(cp - string);

	for(;counter <= length; counter++)
	{
		if ((string[counter]>='0') && (string[counter]<='9'))
		{
			tmp = (string[counter]-'0');		
			number = number*10 + tmp;   
		}
	}

	number *= sign;

	return (McpU32)(number);
}

void MCP_HAL_MISC_Assert(const char *expression, const char *file, McpU16 line)
{
	printf("BTHAL_UTILS_Assert: %s failed in %s at line %u\n", expression, file, line);	
	exit(1);
}


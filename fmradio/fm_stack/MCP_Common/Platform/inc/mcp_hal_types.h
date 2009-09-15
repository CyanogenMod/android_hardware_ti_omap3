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
*   FILE NAME:      mcp_hal_types.h
*
*   BRIEF:    		Definitions for basic basic HAL Types.
*
*   DESCRIPTION:	This file defines the BASIC hal types. These would be used
*					as base types for upper layers
*
*   AUTHOR:         Udi Ron
*
\*******************************************************************************/

#ifndef __MCP_HAL_TYPES_H
#define __MCP_HAL_TYPES_H

/* -------------------------------------------------------------
 *					Platform-Depndent Part							
 *																
 * 		SET THE VALUES OF THE FOLLOWING PRE-PROCESSOR 			
 *		DEFINITIONS 	TO THE VALUES THAT APPLY TO THE 				
 *		TARGET PLATFORM											
 *																
 */

#include <stdint.h>

/* -------------------------------------------------------------
 *					8 Bits Types
 */

typedef uint8_t 	 	McpU8;
typedef int8_t  		McpS8;

/* -------------------------------------------------------------
 *					16 Bits Types
 */

typedef uint16_t  	McpU16;
typedef int8_t 		McpS16;

/* -------------------------------------------------------------
 *					32 Bits Types
 */

typedef uint32_t 	McpU32;
typedef int32_t 	McpS32;


/* -------------------------------------------------------------
 *			Native Integer Types (# of bits irrelevant)
 */
typedef int			McpInt;
typedef unsigned int	McpUint;


/* -------------------------------------------------------------
 *					UTF8,16 types
 */
typedef McpU8 	McpUtf8;
typedef McpU16 	McpUtf16;
	
/* --------------------------------------------------------------
 *					Boolean Definitions							 
 */
typedef McpInt McpBool;

#define MCP_TRUE  (1 == 1)
#define MCP_FALSE (0==1) 

/* --------------------------------------------------------------
 *					Null Definition							 
 */
#ifndef NULL
#define NULL    0
#endif

/* -------------------------------------------------------------
 *					LIMITS						
 */
 
#define	MCP_U8_MAX			UINT8_MAX
#define	MCP_U16_MAX			UINT16_MAX
#define	MCP_U32_MAX			UINT32_MAX

#define MCP_UINT_MAX			(MCP_U32_MAX)


#endif /* __MCP_HAL_TYPES_H */


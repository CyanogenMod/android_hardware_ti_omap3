/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
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

/** 
 *  \file   pla_os_types.h 
 *  \brief  PLA Win OS dependent types and includes file
 * 
 * This file contains Win OS specific types, definitions and H files
 * 
 *  \see    pla_os.c, mcp_hal_types.h
 */

#ifndef PLA_OS_TYPES_H
#define PLA_OS_TYPES_H

/* -------------------------------------------------------------
 *					Platform-Depndent Part							
 *																
 * 		SET THE VALUES OF THE FOLLOWING PRE-PROCESSOR 			
 *		DEFINITIONS 	TO THE VALUES THAT APPLY TO THE 				
 *		TARGET PLATFORM											
 *																
 */

/* Size of type (char) in the target platform, in bytes	*/
#define MCP_CHAR_SIZE		1

/* Size of type (short) in the target platform, in bytes */
#define MCP_SHORT_SIZE 		2

/* Size of type (long) in the target platform, in bytes	*/
#define MCP_LONG_SIZE 		4

/* Size of type (int) in the target platform, in bytes	*/
#define MCP_INT_SIZE 		4

#define likely
#define unlikely


#define SNPRINTF	snprintf

/* Task's Priority */
#define	PLA_THREAD_PRIORITY_ABOVE_NORMAL		(1)
#define	PLA_THREAD_PRIORITY_BELOW_NORMAL		(-1)
#define	PLA_THREAD_PRIORITY_HIGHEST				(2)
#define	PLA_THREAD_PRIORITY_IDLE				(-15)
#define	PLA_THREAD_PRIORITY_LOWEST				(-2)
#define	PLA_THREAD_PRIORITY_NORMAL				(0)
#define	PLA_THREAD_PRIORITY_TIME_CRITICAL		(15)


#endif /* PLA_OS_TYPES_H */



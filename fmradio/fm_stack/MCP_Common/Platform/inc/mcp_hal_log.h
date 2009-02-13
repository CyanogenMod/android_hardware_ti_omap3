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
*   FILE NAME:      mcp_hal_log.h
*
*   BRIEF:          This file defines the API of the MCP HAL log MACROs.
*
*   DESCRIPTION:  The MCP HAL LOG API implements platform dependent logging
*                       MACROs which should be used for logging messages by 
*                   different layers, such as the transport, stack, BTL and BTHAL.
*                   The following 5 Macros mast be implemented according to the platform:
*                   MCP_HAL_LOG_DEBUG
*                   MCP_HAL_LOG_INFO
*                   MCP_HAL_LOG_ERROR
*                   MCP_HAL_LOG_FATAL
*
*   AUTHOR:         Udi Ron 
*
\*******************************************************************************/
#ifndef __MCP_HAL_LOG_H
#define __MCP_HAL_LOG_H


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "mcp_hal_types.h"

/*---------------------------------------------------------------------------
 * McpHalLogSeverity type
 *
 *     Represents possible types of log messages severity
 */
typedef McpU8 McpHalLogSeverity;

#define MCP_HAL_LOG_SEVERITY_DEBUG          ((McpU8)9)
#define MCP_HAL_LOG_SEVERITY_INFO               ((McpU8)8)
#define MCP_HAL_LOG_SEVERITY_ERROR          ((McpU8)7)
#define MCP_HAL_LOG_SEVERITY_FATAL          ((McpU8)6)


/*-------------------------------------------------------------------------------
 * Platform dependent functions
 */
char *MCP_HAL_LOG_FormatMsg(const char *format, ...);

void MCP_HAL_LOG_LogMsg(const char*     fileName, 
                            McpU32          line, 
                            const char      *moduleName, 
                            McpHalLogSeverity   severity,
                            const char      *msg);

#ifdef EBTIPS_RELEASE
#define MCP_HAL_LOG_REPORT_API_FUNCTION_ENTRY_EXIT                      (MCP_HAL_CONFIG_DISABLED)
#else
#define MCP_HAL_LOG_REPORT_API_FUNCTION_ENTRY_EXIT                      (MCP_HAL_CONFIG_ENABLED)
#endif

#define MCP_HAL_LOG_FORMAT_MSG(msg) MCP_HAL_LOG_FormatMsg msg

/*-------------------------------------------------------------------------------
 * MCP_HAL_LOG_DEBUG
 *
 *      Defines trace message in debug level, which should not be used in release build.
 *      This MACRO is not used when EBTIPS_RELEASE is enabled.
 */
#define MCP_HAL_LOG_DEBUG(file, line, moduleName, msg)  MCP_HAL_LOG_LogMsg( file,                       \
                                                                        line,                       \
                                                                        moduleName,                 \
                                                                        MCP_HAL_LOG_SEVERITY_DEBUG,     \
                                                                        MCP_HAL_LOG_FORMAT_MSG(msg))


/*-------------------------------------------------------------------------------
 * MCP_HAL_LOG_INFO
 *
 *      Defines trace message in info level.
 */
#define MCP_HAL_LOG_INFO(file, line, moduleName, msg)       MCP_HAL_LOG_LogMsg( file,                           \
                                                                                line,                           \
                                                                                moduleName,                     \
                                                                                MCP_HAL_LOG_SEVERITY_INFO,          \
                                                                                MCP_HAL_LOG_FORMAT_MSG(msg))


/*-------------------------------------------------------------------------------
 * MCP_HAL_LOG_ERROR
 *
 *      Defines trace message in info level.
 */
#define MCP_HAL_LOG_ERROR(file, line, moduleName, msg)  MCP_HAL_LOG_LogMsg( file,                       \
                                                                          line,                         \
                                                                        moduleName,                 \
                                                                            MCP_HAL_LOG_SEVERITY_ERROR,     \
                                                                            MCP_HAL_LOG_FORMAT_MSG(msg))


/*-------------------------------------------------------------------------------
 * MCP_HAL_LOG_FATAL
 *
 *      Defines trace message in fatal level.
 */
#define MCP_HAL_LOG_FATAL(file, line, moduleName, msg)  MCP_HAL_LOG_LogMsg( file,                           \
                                                                        line,                       \
                                                                        moduleName,                 \
                                                                        MCP_HAL_LOG_SEVERITY_FATAL,     \
                                                                        MCP_HAL_LOG_FORMAT_MSG(msg))


/*-------------------------------------------------------------------------------
 * MCP_HAL_LOG_FUNCTION
 *
 *      Defines trace message in function level, meaning it is used when entering
 *      and exiting a function.
 *      It should not be used in release build.
 *      This MACRO is not used when EBTIPS_RELEASE is enabled.
 */
#define MCP_HAL_LOG_FUNCTION(file, line, moduleName, msg)   MCP_HAL_LOG_LogMsg( file,                           \
                                                                            line,                       \
                                                                                moduleName,                 \
                                                                                MCP_HAL_LOG_SEVERITY_DEBUG,     \
                                                                                MCP_HAL_LOG_FORMAT_MSG(msg))


#endif /* __MCP_HAL_LOG_H */



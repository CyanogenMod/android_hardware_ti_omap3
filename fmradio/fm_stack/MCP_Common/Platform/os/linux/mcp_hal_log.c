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
*   FILE NAME:      mcp_hal_log.c
*
*   DESCRIPTION:    This file implements the API of the MCP HAL log utilities.
*
*   AUTHOR:         Udi Ron
*
\*******************************************************************************/


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include <stdio.h>
#include <stdarg.h>
#include "mcp_hal_log.h"
#include "fm_trace.h"

/****************************************************************************
 *
 * Constants
 *
 ****************************************************************************/

#define MCP_HAL_MAX_FORMATTED_MSG_LEN 			(200)

static char _mcpLog_FormattedMsg[MCP_HAL_MAX_FORMATTED_MSG_LEN + 1];


/*-------------------------------------------------------------------------------
 * MCP_HAL_LOG_FormatMsg()
 *
 *		sprintf-like string formatting. the formatted string is allocated by the function.
 *
 * Type:
 *		Synchronous, non-reentrant 
 *
 * Parameters:
 *
 *		format [in]- format string
 *
 *		...     [in]- additional format arguments
 *
 * Returns:
 *     Returns pointer to the formatted string
 *
 */
char *MCP_HAL_LOG_FormatMsg(const char *format, ...)
{
	va_list     args;

	_mcpLog_FormattedMsg[MCP_HAL_MAX_FORMATTED_MSG_LEN] = '\0';
	
	va_start(args, format);

	vsnprintf(_mcpLog_FormattedMsg, MCP_HAL_MAX_FORMATTED_MSG_LEN, format, args);

	va_end(args);

	return _mcpLog_FormattedMsg;
}


/*-------------------------------------------------------------------------------
 * LogMsg()
 *
 *		Sends a log message to the local logging tool.
 *		Not all parameters are necessarily supported in all platforms.
 *     
 * Type:
 *		Synchronous
 *
 * Parameters:
 *
 *		fileName [in] - name of file originating the message
 *
 *		line [in] - line number in the file
 *
 *		moduleType [in] - e.g. "BTL_BMG", "BTL_SPP", "BTL_OPP", 
 *
 *		severity [in] - debug, error...
 *
 *		msg [in] - message in already formatted string
 *
 * 	Returns:
 *		void
 * 
 */
void MCP_HAL_LOG_LogMsg(	const char*		fileName, 
								McpU32			line, 
								const char		*moduleName,
								McpHalLogSeverity severity,  
								const char* 		msg)
{
	char output = 0;

	switch(severity) {
	case MCP_HAL_LOG_SEVERITY_ERROR:
	case MCP_HAL_LOG_SEVERITY_FATAL:
		/* send msg both to logfile and screen */
		output = FM_TRACE_TO_STACK_LOG_ERR;
		break;
	case MCP_HAL_LOG_SEVERITY_DEBUG:
	case MCP_HAL_LOG_SEVERITY_INFO:
	default:
		/* send msg only to logfile */
		output = FM_TRACE_TO_STACK_LOGFILE;
		break;
	}

	fm_trace_out(output, moduleName, NULL, msg);
}


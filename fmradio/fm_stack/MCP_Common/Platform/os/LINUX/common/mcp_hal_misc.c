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

/*******************************************************************************\
*
*   FILE NAME:      mcp_hal_utils.c
*
*   DESCRIPTION:    This file implements the MCP HAL Utils for Linux.
*
*   AUTHOR:         Chen Ganir
*
\*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "mcp_hal_log.h"
#include "mcp_hal_misc.h"

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

void MCP_HAL_MISC_Assert(const char *expression, const char *file, McpU16 line)
{
    MCP_HAL_LOG_ERROR(file, line, MCP_HAL_LOG_MODULE_TYPE_ASSERT, (expression));
    sleep(5);
    abort();
}


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


#ifndef __MCP_MAIN_H
#define __MCP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif 

#include <stdio.h>
#include "mcpf_defs.h"


/*---------------------------------------------------------------------------
*            MCP_Init()
*---------------------------------------------------------------------------
*
* Synopsis:  Main Entry-point for our application
*
* Return:	MCPF Handler
*
	*/
handle_t MCP_Init (LPTSTR CmdLine);

/*---------------------------------------------------------------------------
*            MCP_Deinit()
*---------------------------------------------------------------------------
*
* Synopsis:  De-initializes the Headset Audio Gateway
*
* Return:    
*
*/
void  MCP_Deinit(handle_t hMcpf);


#ifdef __cplusplus
}
#endif 

#endif

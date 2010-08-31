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



/** \file   mcp_ui.h 
 *  \brief  User Interface Thread
 * 
 *  \see    mcp_ui.c
 */

#ifndef __MCP_UI_H__
#define __MCP_UI_H__

#include "mcpf_defs.h"

/************************************************************************
 * Functions
 ************************************************************************/

/** 
 * \fn     ui_init 
 * \brief  Init UI
 * 
 * Init UI global variable
 * 
 * \note
 * \param	hMcpf  - MCPF Handler
 * \return 	None
 * \sa     	ui_init
 */ 
void ui_init(handle_t hMcpf);

/** 
 * \fn     ui_destroy 
 * \brief  Destroy UI 
 * 
 * Destroy UI global variable & Thread
 * 
 * \note
 * \return 	None
 * \sa     	ui_destroy
 */ 
void ui_destroy(void);

/** 
 * \fn     ui_sendNavcCmd 
 * \brief  Build & Send command to NAVC
 * 
 * Build & Send command to NAVC
 * 
 * \note
 * \param	uOpCode  - Command's operation code
 * \return 	None
 * \sa     	ui_sendNavcCmd
 */ 
void ui_sendNavcCmd(McpU16 uOpCode);

/** 
 * \fn     ui_handleEvent 
 * \brief  Handle UI Events
 * 
 * Handle UI Events
 * 
 * \note
 * \param	hMcpf  - MCPF Handler
 * \param	pMsg  - pointer to MCPF message containing UI Event
 * \return 	status of operation: OK or Error
 * \sa     	ui_handleEvent
 */ 
EMcpfRes ui_handleEvent (handle_t hMcpf, TmcpfMsg * pMsg);

#endif /*__MCP_UI_H__*/

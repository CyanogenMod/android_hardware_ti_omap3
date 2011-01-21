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


#ifndef __PLA_CMDPARSE_H
#define __PLA_CMDPARSE_H

#include "mcpf_defs.h"
#include "mcp_hal_types.h"

#define	MAX_ADS_PORTS		10

typedef struct
{
	McpInt	x_SensorPcPort; 
	McpInt	x_NmeaPcPort;
	McpInt	x_AdsPort[MAX_ADS_PORTS];
	McpU16	x_AdsPortsCounter;
	McpU16	w_AlmanacDataBlockFlag;
	McpS8  s_PathCtrlFile[50];
} TcmdLineParams;

/** 
 * \fn     pla_cmdParse_GeneralCmdLine
 * \brief  Parses the initial command line
 * 
 * This function parses the initial command line, 
 * and returns the BT & NAVC strings, 
 * along with the common parameter.
 * 
 * \note
 * \param	CmdLine     - Received command Line.
 * \param	**bt_str - returned BT cmd line.
 * \param	**nav_str - returned NAVC cmd line.
 * \return 	Port Number
 * \sa     	pla_cmdParse_GeneralCmdLine
 */
McpU32 pla_cmdParse_GeneralCmdLine(char *cmdLine, char **bt_str, char **nav_str);

/** 
 * \fn     pla_cmdParse_NavcCmdLine
 * \brief  Parses the NAVC command line
 * 
 * This function parses the NAVC command line.
 * 
 * \note
 * \param	*nav_str - NAVC cmd line.
 * \param	*tCmdLineParams - Params to fill.
 * \return 	None
 * \sa     	pla_cmdParse_NavcCmdLine
 */
void pla_cmdParse_NavcCmdLine(char *nav_str, TcmdLineParams *tCmdLineParams);


#endif
	McpS8  s_PathCtrlFile[50];

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
*   FILE NAME:      ccm_im_bt_scripts_db.h
*
*   BRIEF:          This file defines the interface BT Rom Scripts repository
*
*   DESCRIPTION:
*	
*
*   AUTHOR:   Udi Ron
*
\*******************************************************************************/
#ifndef __CCM_IM_BT_SCRITPS_DB_H
#define __CCM_IM_BT_SCRITPS_DB_H

typedef struct tagCcmImBtRomScripts_Data {
	const char	*fileName;
	McpUint		size;
	const McpU8	*address;
} CcmImBtRomScripts_Data;

McpBool CCM_IM_BT_SDB_GetMemInitScriptData(	const char 	*scriptFileName,
															McpUint		*scriptSize,
															const McpU8 **scriptAddress);

#endif /* __CCM_IM_BT_SCRITPS_DB_H */


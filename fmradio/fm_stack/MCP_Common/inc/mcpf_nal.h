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


#ifndef __SUPLC_NAL_H__
#define __SUPLC_NAL_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#include "mcpf_defs.h"
#include "mcpf_nal_common.h"

EMcpfRes NAL_executeCommand(eNetCommands cmd,void *data);


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __SUPLC_NAL_H__ */


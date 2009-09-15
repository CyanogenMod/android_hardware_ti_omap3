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

#ifndef _CCM_CONFIG_H
#define _CCM_CONFIG_H

#include "mcp_hal_types.h"

/*-------------------------------------------------------------------------------
 * IM (Init-Manager) Module
 *
 *     Represents configuration parameters for CCM-IM module.
 */

/*
*   TBD
*/
#define CCM_IM_CONFIG_MUTEX_NAME                ("CCM_Mutex")

/*
*   TBD
*/
#define CCM_CONFIG_IM_MAX_NUM_OF_REGISTERED_CLIENTS     ((McpUint)10)

/*-------------------------------------------------------------------------------
 * VAC (Voice and Audio Control) Module
 *
 *     Represents configuration parameters for CCM-VAC module.
 */

#define CCM_VAC_MAX_NUMBER_OF_CLIENTS_PER_OPERATION     2
#define CCM_CAL_MAX_NUMBER_OF_PROPERTIES_PER_RESOURCE   1

/*
 * ini file definitions
 */
#define CCM_VAC_CONFIG_FILE                             ("c:\\ti\\InitScript\\vac_config.ini")
#define CCM_VAC_MEM_CONFIG                              ("[operations]\n" \
                                                         "number_of_operations = 6\n" \
                                                         "operation = CAL_OPERATION_BT_VOICE\n" \
                                                         "operation = CAL_OPERATION_A3DP\n" \
                                                         "operation = CAL_OPERATION_FM_RX\n" \
                                                         "operation = CAL_OPERATION_FM_TX\n" \
                                                         "operation = CAL_OPERATION_FM_RX_OVER_SCO\n" \
                                                         "operation = CAL_OPERATION_FM_RX_OVER_A3DP\n" \
                                                         "[resources]\n" \
                                                         "number_of_resources = 3\n" \
                                                         "resource = CAL_RESOURCE_PCMH\n" \
                                                         "resource = CAL_RESOURCE_I2SH\n" \
                                                         "resource = CAL_RESOURCE_FM_ANALOG\n" \
                                                         "[mapping]\n" \
                                                         "operation = CAL_OPERATION_BT_VOICE\n" \
                                                         "number_of_possible_resources = 1\n" \
                                                         "resource = CAL_RESOURCE_PCMH\n" \
                                                         "default_resource = CAL_RESOURCE_PCMH\n" \
                                                         "operation = CAL_OPERATION_A3DP\n" \
                                                         "number_of_possible_resources = 2\n" \
                                                         "resource = CAL_RESOURCE_PCMH\n" \
                                                         "resource = CAL_RESOURCE_I2SH\n" \
                                                         "default_resource = CAL_RESOURCE_PCMH\n" \
                                                         "operation = CAL_OPERATION_FM_RX\n" \
                                                         "number_of_possible_resources = 3\n" \
                                                         "resource = CAL_RESOURCE_PCMH\n" \
                                                         "resource = CAL_RESOURCE_I2SH\n" \
                                                         "resource = CAL_RESOURCE_FM_ANALOG\n" \
                                                         "default_resource = CAL_RESOURCE_FM_ANALOG\n" \
                                                         "operation = CAL_OPERATION_FM_TX\n" \
                                                         "number_of_possible_resources = 3\n" \
                                                         "resource = CAL_RESOURCE_PCMH\n" \
                                                         "resource = CAL_RESOURCE_I2SH\n" \
                                                         "resource = CAL_RESOURCE_FM_ANALOG\n" \
                                                         "default_resource = CAL_RESOURCE_FM_ANALOG\n" \
                                                         "operation = CAL_OPERATION_FM_RX_OVER_SCO\n" \
                                                         "number_of_possible_resources = 0\n" \
                                                         "operation = CAL_OPERATION_FM_RX_OVER_A3DP\n" \
                                                         "number_of_possible_resources = 0")

#define CCM_VAC_CONFIG_INI_OPERATIONS_SECTION_NAME      ("operations")
#define CCM_VAC_CONFIG_INI_OPERATIONS_NUMBER_KEY_NAME   ("number_of_operations")
#define CCM_VAC_CONFIG_INI_OPERATION_NAME_KEY_NAME      ("operation")

#define CCM_VAC_CONFIG_INI_RESOURCES_SECTION_NAME       ("resources")
#define CCM_VAC_CONFIG_INI_RESOURCES_NUMBER_KEY_NAME    ("number_of_resources")
#define CCM_VAC_CONFIG_INI_RESOURCE_NAME_KEY_NAME       ("resource")

#define CCM_VAC_CONFIG_INI_MAPPING_SECTION_NAME         ("mapping")
#define CCM_VAC_CONFIG_INI_MAPPING_OP_KEY_NAME          ("operation")
#define CCM_VAC_CONFIG_INI_MAPPING_CONFIG_NUM_KEY_NAME  ("number_of_possible_resources")
#define CCM_VAC_CONFIG_INI_MAPPING_RESOURCE_KEY_NAME    ("resource")
#define CCM_VAC_CONFIG_INI_MAPPING_DEF_RESOURCE_KEY_NAME ("default_resource")

#endif


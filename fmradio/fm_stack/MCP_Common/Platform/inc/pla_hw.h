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


#ifndef __PLA_HW_H
#define __PLA_HW_H


#include "mcpf_defs.h"

/* GPIO */

/** 
 * \fn     hw_gpio_set
 * \brief  Set the GPIO
 * 
 * This function sets the requested GPIO to the specified value.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	index - GPIO index.
 * \param	value - value to set.
 * \return 	Result of operation: OK or ERROR
 * \sa     	hw_gpio_set
 */
EMcpfRes	hw_gpio_set (handle_t hPla, McpU32 index, McpU8 value);

/** 
 * \fn     hw_refClk_set
 * \brief  Enable/Disable the Reference Clock
 * 
 * This function enables or disables the 
 * reference clock (for clock configuration #4).
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	uIsEnable - Enable/Disable the ref clk.
 * \return 	Result of operation: OK or ERROR
 * \sa     	hw_refClk_set
 */
EMcpfRes	hw_refClk_set(handle_t hPla, McpU8 uIsEnable);


#endif

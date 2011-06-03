/*
 * OMAP CPU THERMAL GOVERNOR
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Sebastien Sabatier (s-sabatier1@ti.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the dual BSD / GNU General Public License version 2 as
 * published by the Free Software Foundation. When using or
 * redistributing this file, you may do so under either license.
 */

#include "pcb_thermal_governor.h"

/*
 * Internal functions
 */


/*
 *
 * External functions
 *
 */

void pcb_thermal_governor(u32 pcb_temp)
{
#ifdef DEBUG
	printf("PCB Thermal Governor (%ld)\n", pcb_temp); fflush(stdout);
#endif
}

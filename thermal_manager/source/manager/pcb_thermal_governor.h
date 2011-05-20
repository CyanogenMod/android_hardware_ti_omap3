/*
 * OMAP CPU THERMAL GOVERNOR (header file)
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

#ifndef _PCB_THERMAL_GOVERNOR_H
#define _PCB_THERMAL_GOVERNOR_H

#include <stdio.h>
#include "sysfs.h"

/*
 *
 * External functions
 *
 */
void pcb_thermal_governor(u32 pcb_temp);

#endif

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

#ifndef _LPDDR2_THERMAL_GOVERNOR_H
#define _LPDDR2_THERMAL_GOVERNOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sysfs.h"
#include "read_config.h"
#include <sys/reboot.h>

/* config parameters: shall be part of the configuration file */
#define MAX_LPDDR2_TEMP_ZONE 7

/*
 *
 * External functions
 *
 */
void lpddr2_thermal_governor(u32 emif1_temp_zone, u32 emif2_temp_zone);

#endif

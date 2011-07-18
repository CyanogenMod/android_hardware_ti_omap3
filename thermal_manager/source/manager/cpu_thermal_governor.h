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

#ifndef _CPU_THERMAL_GOVERNOR_H
#define _CPU_THERMAL_GOVERNOR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "sysfs.h"
#include "read_config.h"

#define HYSTERESIS_VALUE 2000 /* in milli-celsius degrees to be compliant with HWMON APIs */
#define NORMAL_TEMP_MONITORING_RATE 1000 /* 1 second */
#define FAST_TEMP_MONITORING_RATE 250 /* 250 milli-seconds */
#define OMAP_CPU_THRESHOLD_FATAL 125000

/*
 *
 * External functions
 *
 */
int cpu_thermal_governor(u32 omap_temp);
void init_cpu_thermal_governor(u32 omap_temp);

#endif

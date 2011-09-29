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

#include <utils/Log.h>
#include "lpddr2_thermal_governor.h"

/*
 * Internal functions
 */


/*
 *
 * External functions
 *
 */

void lpddr2_thermal_governor(u32 emif1_temp_zone, u32 emif2_temp_zone)
{
    if ((emif1_temp_zone == MAX_LPDDR2_TEMP_ZONE) ||
        (emif2_temp_zone == MAX_LPDDR2_TEMP_ZONE)) {
#ifdef DEBUG
        LOGD("!!! LPDDR2 FATAL ZONE (%ld, %ld) !!!\n",
            emif1_temp_zone, emif2_temp_zone);
        fflush(stdout);
#endif
        sync();
        reboot(RB_POWER_OFF);
    }
}

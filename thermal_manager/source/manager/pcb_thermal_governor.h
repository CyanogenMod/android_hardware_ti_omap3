/*
 * OMAP CPU THERMAL GOVERNOR (header file)
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Contacts:
 *  Sebastien Sabatier (s-sabatier1@ti.com)
 *  Eduardo Valentin (eduardo.valentin@ti.com)
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
 * Structure to hold the duty cycle parameters
 * The general idea is that the system will use 'nitro_interval' as a time
 * window to control the usage of the maximum OPP. It should be as follows:
 *  - time at the 'nitro_rate' =
 *        ( nitro_percentage / 100) * nitro_interval
 *  - time at the 'cooling_rate' =
 *        ((100 - nitro_percentage) / 100) * nitro_interval
 */
struct duty_cycle_parameters {
    u32 nitro_rate;        /* the maximum OPP frequency */
    u32 cooling_rate;      /* the OPP used to cool off */
    u32 nitro_interval;    /* time interval to control the duty cycle */
    u32 nitro_percentage;  /* % out of nitro_interval to use max OPP */
};

/*
 * Structure to describe what needs to be done for each PCB sensor level
 * The idea is to apply the duty cycle parameters whenever the system detects
 * that the current PCB sensor temperature is lower than the 'pcb_temp_level'.
 * There are two possible actions:
 *  - disable the duty cycle and allow the system to use the "max_opp".
 *  - enable the duty cycle and apply the corresponding 'duty_cycle_params'.
 */
struct pcb_section {
    u32 pcb_temp_level;
    u32 max_opp;
    struct duty_cycle_parameters duty_cycle_params;
    bool duty_cycle_enabled;
};

/*
 *
 * External functions
 *
 */
int pcb_thermal_governor(u32 pcb_temp);
void init_pcb_thermal_governor(void);

#endif

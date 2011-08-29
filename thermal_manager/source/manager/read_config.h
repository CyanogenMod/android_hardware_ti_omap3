/*
 * Function to read the configuration file
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

#ifndef _READ_CONFIG_H
#define _READ_CONFIG_H

/* Standard Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sysfs.h>
#include <stdbool.h>

/* For configuration file */
#include "libconfig.h"
#include "pcb_thermal_governor.h"
#define CONFIG_FILE "/system/bin/libthermal-manager.cfg"
/* Multiple sensor reading definitions */
#define MAX_SENSORS 4
#define PCB_SENSOR_TEMP_PATH 3
/* Multiple CPU Freq files */
#define MAX_CPUFREQ_PATHS 6
#define AVAILABLE_FREQS_PATH 0
#define AVAILABLE_GOVS_PATH 1
#define SCALING_MAX_FREQ_PATH 2
#define SCALING_GOVERNOR_PATH 3
#define SCALING_SET_SPEED_PATH 4
#define CPUINFO_CUR_FREQ_PATH 5
/* Multiple OMAP on-die temperature sensor files */
#define MAX_OMAPTEMP_PATHS 3
#define OMAP_CPU_UPDATE_RATE_PATH 0
#define OMAP_CPU_THRESHOLD_HIGH_PATH 1
#define OMAP_CPU_THRESHOLD_LOW_PATH 2
/* Multiple PCB temperature sensor files */
#define MAX_PCBTEMP_PATHS 3
#define PCB_UPDATE_RATE_PATH 0
#define PCB_THRESHOLD_HIGH_PATH 1
#define PCB_THRESHOLD_LOW_PATH 2
/* Multiple OMAP4430 duty cycle files */
#define MAX_DUTY_CYCLE_PATHS 5
#define NITRO_RATE_PATH 0
#define COOLING_RATE_PATH 1
#define NITRO_INTERVAL_PATH 2
#define NITRO_PERCENTAGE_PATH 3
#define DUTY_CYCLE_ENABLED_PATH 4
/* OPPs number */
#define OPPS_NUMBER 5 /* OPP50, OPP100, TURBO, NITRO, NITRO+SB */
/* CPU Freq governors */
#define GOVS_NUMBER 5
/* PCB temperature sections */
#define MAX_PCB_SECTIONS 10
#define DEBUG 1
struct configinfo {
    char *temperature_file_sensors[MAX_SENSORS];
    char *omap_cpu_temp_sensor_id;
    char *omap_pcb_temp_sensor_id;
    int omap_cpu_threshold_monitoring;
    int omap_cpu_threshold_alert;
    int omap_cpu_threshold_panic;
    int omap_cpu_temperature_slope;
    int omap_cpu_temperature_offset;
    int pcb_threshold;
    bool pcb_do_polling;
    int pcb_polling_interval;
    char *cpufreq_file_paths[MAX_CPUFREQ_PATHS];
    char *omaptemp_file_paths[MAX_OMAPTEMP_PATHS];
    char *pcbtemp_file_paths[MAX_PCBTEMP_PATHS];
    char *duty_cycle_file_paths[MAX_DUTY_CYCLE_PATHS];
    int n_pcb_sections;
    struct pcb_section pcb_sections[MAX_PCB_SECTIONS];
};
struct configinfo config_file;

/* API definitions */
int read_config(void);

#endif

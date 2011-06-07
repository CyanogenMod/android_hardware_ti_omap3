/*
 * THERMAL GOVERNOR
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

#include <string.h>
#include <stdio.h>
#include "cpu_thermal_governor.h"
#include "lpddr2_thermal_governor.h"
#include "pcb_thermal_governor.h"
#include "sysfs.h"
#include "read_config.h"

int init_done = 0;

/*
 *
 * Thermal Manager init:
 * Read the configuration file to collect the various parameters of the thermal manager.
 * Initialize all temperature thresholds according to the current temperature.
 *
 */
void thermal_manager_init(void)
{
    u32 omap_cpu_sensor_temp;   /* temperature reported by the OMAP CPU on-die temp sensor */

    if (init_done == 0) {
        read_config();
        omap_cpu_sensor_temp =
            atoi(read_from_file(config_file.temperature_file_sensors[OMAP_CPU]));
        printf("THERMAL MANAGER - init_cpu_thermal_governor %ld\n",
            omap_cpu_sensor_temp);
        fflush(stdout);
        init_cpu_thermal_governor(omap_cpu_sensor_temp);
        init_done = 1;
    }
}

/*
 *
 * Thermal Manager:
 *  According to the UEvent notified by Linux kernel drivers and
 *  caught by Android JAVA UEVENT observer,
 *  the Thermal Manager calls specific thermal governor (CPU, LPDDR2, PCB).
 *  Note: the current implementation of Thermal Manager does not handle the
 *  thermal events related to the Battery temperature and the PMIC temperature.
 *
 */
void thermal_manager_algo(const char *string)
{
    u32 omap_cpu_sensor_temp;   /* temperature reported by the OMAP CPU on-die temp sensor */
    u32 emif1_temp_zone;
    u32 emif2_temp_zone;
    u32 pcb_temp;

    if (strcmp(string, "omap_cpu") == 0) {
        /*
         * Call dedicated governor to control OMAP junction temperature
         * related to the CPU activity
         */
        omap_cpu_sensor_temp =
            atoi(read_from_file(config_file.temperature_file_sensors[OMAP_CPU]));
        printf("THERMAL MANAGER - OMAP CPU sensor temperature %ld\n",
            omap_cpu_sensor_temp);
        fflush(stdout);
        cpu_thermal_governor(omap_cpu_sensor_temp);
    } else if (strcmp(string, "lpddr2") == 0) {
        /*
         * Call dedicated governor to control LPDDR2 junction temperature
         */
        emif1_temp_zone = atoi(read_from_file(config_file.temperature_file_sensors[EMIF1]));
        emif2_temp_zone = atoi(read_from_file(config_file.temperature_file_sensors[EMIF2]));
        printf("THERMAL MANAGER - emif temperature (1: %ld, 2: %ld)\n",
            emif1_temp_zone, emif2_temp_zone);
        fflush(stdout);
        lpddr2_thermal_governor(emif1_temp_zone, emif2_temp_zone);
    } else if (strcmp(string, "pcb") == 0) {
        /*
         * Call dedicated governor to control PCB junction temperature
         */
        pcb_temp = atoi(read_from_file(config_file.temperature_file_sensors[PCB]));
        printf("THERMAL MANAGER - pcb temperature %ld\n", pcb_temp); fflush(stdout);
        pcb_thermal_governor(pcb_temp);
    }
}

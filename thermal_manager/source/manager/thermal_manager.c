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

#include <utils/Log.h>

#define TM_DEBUG 1

int init_done = 0;

/*
 *
 * Thermal Manager init:
 * Read the configuration file to collect the various parameters of the thermal manager.
 * Initialize all temperature thresholds according to the current temperature.
 *
 */
int thermal_manager_init(void)
{
    u32 omap_cpu_sensor_temp;   /* temperature reported by the OMAP CPU on-die temp sensor */
    char buffer[1024];
    int cpu_fd_id = -1;

    if (init_done == 0) {
        read_config();
        if (config_file.omap_cpu_temp_sensor_id) {
#ifdef TM_DEBUG
            LOGD("CPU path is %s\n", config_file.omap_cpu_temp_sensor_id);
#endif
            cpu_fd_id = open(config_file.omap_cpu_temp_sensor_id, O_RDONLY);
            if (cpu_fd_id < 0) {
                LOGD("Thermal Manager:Cannot find OMAP CPU Temp\n");
                return -1;
            }
            omap_cpu_sensor_temp =
                atoi(read_from_file(config_file.temperature_file_sensors[OMAP_CPU]));
        } else {
            LOGD("Thermal Manager:CPU Temp ID not found\n");
            return -1;
        }

        read(cpu_fd_id, buffer, 1024);
        if (cpu_fd_id > 0)
            init_cpu_thermal_governor(omap_cpu_sensor_temp);

        close(cpu_fd_id);
        init_done = 1;
        return 0;
    }
    return 0;
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
int thermal_manager_algo(const char *string)
{
    u32 omap_cpu_sensor_temp;   /* temperature reported by the OMAP CPU on-die temp sensor */
    u32 emif1_temp_zone;
    u32 emif2_temp_zone;
    u32 pcb_temp;
    int state = 0;

    if (strcmp(string, "omap_cpu") == 0) {
        /*
         * Call dedicated governor to control OMAP junction temperature
         * related to the CPU activity
         */
        omap_cpu_sensor_temp =
            atoi(read_from_file(config_file.temperature_file_sensors[OMAP_CPU]));
        LOGD("Thermal Manager:OMAP CPU sensor temperature %ld\n",
            omap_cpu_sensor_temp);
        fflush(stdout);
        state = cpu_thermal_governor(omap_cpu_sensor_temp);
        return state;

    } else if (strcmp(string, "lpddr2") == 0) {
        /*
         * Call dedicated governor to control LPDDR2 junction temperature
         */
        emif1_temp_zone = atoi(read_from_file(config_file.temperature_file_sensors[EMIF1]));
        emif2_temp_zone = atoi(read_from_file(config_file.temperature_file_sensors[EMIF2]));
        LOGD("Thermal Manager:emif temperature (1: %ld, 2: %ld)\n",
            emif1_temp_zone, emif2_temp_zone);
        fflush(stdout);
        lpddr2_thermal_governor(emif1_temp_zone, emif2_temp_zone);

        return 2;
    } else if (strcmp(string, "pcb") == 0) {
        /*
         * Call dedicated governor to control PCB junction temperature
         */
        pcb_temp = atoi(read_from_file(config_file.temperature_file_sensors[PCB]));
        LOGD("Thermal Manager:pcb temperature %ld\n", pcb_temp); fflush(stdout);
        pcb_thermal_governor(pcb_temp);

        return 3;
    }

    return 0;
}

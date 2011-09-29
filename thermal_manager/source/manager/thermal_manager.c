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

/* TODO: Need to make this better */
#include "../../include/thermal_manager.h"

#define TM_DEBUG 1

int init_done = 0;

/*
 *
 * Thermal Manager init:
 * Read the configuration file to collect the various parameters of the thermal manager.
 * Initialize all temperature thresholds according to the current temperature.
 *
 */
int thermal_manager_init(int type)
{
    u32 omap_cpu_sensor_temp;   /* temperature reported by the OMAP CPU on-die temp sensor */
    char buffer[1024];
    int cpu_fd_id = -1;
    int pcb_fd_id = -1;
    int init_status = 0;

    if (init_done == 0) {
        read_config();
        init_done = 1;
    }

    if (type & OMAP_CPU) {
        if (config_file.omap_cpu_temp_sensor_id) {
            cpu_fd_id = open(config_file.omap_cpu_temp_sensor_id, O_RDONLY);
            if (cpu_fd_id < 0) {
                LOGD("Thermal Manager:Cannot find OMAP CPU Temp\n");
                goto emif;
            }
            omap_cpu_sensor_temp =
                atoi(read_from_file(config_file.temperature_file_sensors[OMAP_CPU_FILE]));
        } else {
            LOGD("Thermal Manager:CPU Temp ID not found in the config\n");
            init_status &= ~OMAP_CPU;
            goto emif;
        }

        read(cpu_fd_id, buffer, 1024);
        if (cpu_fd_id > 0) {
            init_cpu_thermal_governor(omap_cpu_sensor_temp);
            init_status = OMAP_CPU;
        }

        close(cpu_fd_id);
    }
emif:
    if ((type & EMIF1) || (type & EMIF2)) {
        LOGD("Thermal Manager:EMIF Not implmented\n");
        init_status &= (EMIF1 | EMIF2);
        goto pcb;
    }
pcb:
    if (type & PCB) {
        if (config_file.omap_pcb_temp_sensor_id) {
            pcb_fd_id = open(config_file.omap_pcb_temp_sensor_id, O_RDONLY);
            if (pcb_fd_id < 0) {
                LOGD("Thermal Manager:Cannot find PCB Temp\n");
                init_status &= ~PCB;
                goto out;
            }
        } else {
            LOGD("Thermal Manager:PCB Temp ID not found in config\n");
            init_status &= ~PCB;
            goto out;
        }

        read(pcb_fd_id, buffer, 1024);
        if (pcb_fd_id > 0) {
            init_pcb_thermal_governor();
            init_status |= PCB;
        }

        close(pcb_fd_id);
    }

out:
    return init_status;
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
            atoi(read_from_file(config_file.temperature_file_sensors[OMAP_CPU_FILE]));
        LOGD("Thermal Manager:OMAP CPU sensor temperature %ld\n",
            omap_cpu_sensor_temp);
        fflush(stdout);
        state = cpu_thermal_governor(omap_cpu_sensor_temp);
    } else if (strcmp(string, "lpddr2") == 0) {
        /*
         * Call dedicated governor to control LPDDR2 junction temperature
         */
        emif1_temp_zone =
            atoi(read_from_file(config_file.temperature_file_sensors[EMIF1_FILE]));
        emif2_temp_zone =
            atoi(read_from_file(config_file.temperature_file_sensors[EMIF2_FILE]));
        LOGD("Thermal Manager:emif temperature (1: %ld, 2: %ld)\n",
            emif1_temp_zone, emif2_temp_zone);
        fflush(stdout);
        lpddr2_thermal_governor(emif1_temp_zone, emif2_temp_zone);
    } else if (strcmp(string, "pcb") == 0) {
        /*
         * Call dedicated governor to control PCB junction temperature
         */
        pcb_temp = atoi(read_from_file(config_file.temperature_file_sensors[PCB_FILE]));
        LOGD("Thermal Manager:pcb temperature %ld\n", pcb_temp);
        fflush(stdout);
        state = pcb_thermal_governor(pcb_temp);
    }

    return state;
}

/*
 * READ CONFIG
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

#include "read_config.h"
#include <utils/Log.h>
/* TODO: Need to make this better */
#include "../../include/thermal_manager.h"
#include "pcb_thermal_governor.h"

const char *sensor_read_paths[MAX_SENSORS] = { "omap_cpu_temperature_file",
                        "emif1_temperature_file",
                        "emif2_temperature_file",
                        "pcb_temperature_file"
                        };

const char *cpufreq_read_paths[MAX_CPUFREQ_PATHS] = { "scaling_available_frequencies",
                        "scaling_available_governors",
                        "scaling_max_freq",
                        "scaling_governor",
                        "scaling_setspeed",
                        "cpuinfo_cur_freq"
                        };

const char *omaptemp_read_paths[MAX_OMAPTEMP_PATHS] = {"omap_cpu_update_rate",
                        "omap_cpu_temp1_max",
                        "omap_cpu_temp1_max_hyst"
                        };

const char *pcbtemp_read_paths[MAX_PCBTEMP_PATHS] = {"pcb_update_rate",
                        "pcb_temp1_max",
                        "pcb_temp1_max_hyst"
                        };

const char *duty_cycle_read_paths[MAX_DUTY_CYCLE_PATHS] = {"duty_cycle_nitro_rate",
                        "duty_cycle_cooling_rate",
                        "duty_cycle_nitro_interval",
                        "duty_cycle_nitro_percentage",
                        "duty_cycle_enabled",
                        };

/* Read in the configuration file and place results in a global
 * structure. Need to read in locally and then copy over as
 * config_destroy will destroy global pointers if read directly
 * into by the config_lookup_xxxx functions */
int read_config(void)
{
    int index;
    config_t cfg, *cf;
    const char *temp_read[MAX_SENSORS];
    const char *cpufreq_read[MAX_CPUFREQ_PATHS];
    const char *omaptemp_read[MAX_OMAPTEMP_PATHS];
    const char *pcbtemp_read[MAX_PCBTEMP_PATHS];
    const char *duty_cycle_read[MAX_DUTY_CYCLE_PATHS];
    const char *omap_cpu_id;
    const char *omap_pcb_id;
    long value;
    int val_bool;
    const config_setting_t *list;

    cf = &cfg;
    config_init(cf);

    if (!config_read_file (cf, CONFIG_FILE)) {
            LOGD (stderr, "%s:%d = %s\n", CONFIG_FILE,
                config_error_line(cf), config_error_text(cf));
        config_destroy (cf);
        return -1;
    }

    for (index = 0; index < MAX_SENSORS; index++) {
            if (config_lookup_string(cf, sensor_read_paths[index],
            &temp_read[index])) {
                        if ((config_file.temperature_file_sensors[index] =
                calloc(strlen(temp_read[index]),
                sizeof(char))) == NULL) {
                LOGD("Error in allocating memory\n");
                fflush(stdout);
                return -1;
            }
            strcpy (config_file.temperature_file_sensors[index], temp_read[index]);
        }
    }

    if (config_lookup_string(cf, "omap_cpu_temp_sensor_id_file", &omap_cpu_id)) {
        if ((config_file.omap_cpu_temp_sensor_id =
            calloc(strlen(omap_cpu_id), sizeof(char))) == NULL) {
                LOGD("Error in allocating memory\n");
                fflush(stdout);
                return -1;
        }
        strcpy (config_file.omap_cpu_temp_sensor_id, omap_cpu_id);
    }

    if (config_lookup_string(cf, "omap_pcb_temp_sensor_id_file", &omap_pcb_id)) {
        if ((config_file.omap_pcb_temp_sensor_id =
            calloc(strlen(omap_pcb_id), sizeof(char))) == NULL) {
                LOGD("Error in allocating memory\n");
                fflush(stdout);
                return -1;
        }
        strcpy (config_file.omap_pcb_temp_sensor_id, omap_pcb_id);
    }

    if (config_lookup_int(cf, "omap_cpu_threshold_monitoring", &value)) {
        config_file.omap_cpu_threshold_monitoring = value;
    }

    if (config_lookup_int(cf, "omap_cpu_threshold_alert", &value)) {
        config_file.omap_cpu_threshold_alert = value;
    }

    if (config_lookup_int(cf, "omap_cpu_threshold_panic", &value)) {
        config_file.omap_cpu_threshold_panic = value;
    }

    if (config_lookup_int(cf, "omap_cpu_temperature_slope", &value)) {
        config_file.omap_cpu_temperature_slope = value;
    }

    if (config_lookup_int(cf, "omap_cpu_temperature_offset", &value)) {
        config_file.omap_cpu_temperature_offset = value;
    }

    if (config_lookup_int(cf, "pcb_threshold", &value)) {
        config_file.pcb_threshold = value;
    }

    if (config_lookup_bool(cf, "pcb_do_polling", &val_bool)) {
        config_file.pcb_do_polling = val_bool;
    } else {
        config_file.pcb_do_polling = true;
    }

    if (config_lookup_int(cf, "pcb_polling_interval", &value)) {
        config_file.pcb_polling_interval = value;
    } else {
        config_file.pcb_polling_interval = 300000;
    }

    for (index = 0; index < MAX_CPUFREQ_PATHS; index++) {
            if (config_lookup_string(cf, cpufreq_read_paths[index],
            &cpufreq_read[index])) {
                        if ((config_file.cpufreq_file_paths[index] =
                calloc(strlen(cpufreq_read[index]),
                sizeof(char))) == NULL) {
                LOGD ("Error in allocating memory\n");
                fflush(stdout);
                return -1;
            }
        strcpy (config_file.cpufreq_file_paths[index], cpufreq_read[index]);
        }
    }

    for (index = 0; index < MAX_OMAPTEMP_PATHS; index++) {
            if (config_lookup_string(cf, omaptemp_read_paths[index],
            &omaptemp_read[index])) {
                        if ((config_file.omaptemp_file_paths[index] =
                calloc(strlen(omaptemp_read[index]),
                sizeof(char))) == NULL) {
                LOGD ("Error in allocating memory\n");
                fflush(stdout);
                return -1;
            }
        strcpy (config_file.omaptemp_file_paths[index], omaptemp_read[index]);
        }
    }

    for (index = 0; index < MAX_PCBTEMP_PATHS; index++) {
            if (config_lookup_string(cf, pcbtemp_read_paths[index],
            &pcbtemp_read[index])) {
                        if ((config_file.pcbtemp_file_paths[index] =
                calloc(strlen(pcbtemp_read[index]),
                sizeof(char))) == NULL) {
                LOGD ("Error in allocating memory\n");
                return -1;
            }
        strcpy (config_file.pcbtemp_file_paths[index], pcbtemp_read[index]);
        }
    }

    for (index = 0; index < MAX_DUTY_CYCLE_PATHS; index++) {
            if (config_lookup_string(cf, duty_cycle_read_paths[index],
            &duty_cycle_read[index])) {
                        if ((config_file.duty_cycle_file_paths[index] =
                calloc(strlen(duty_cycle_read[index]),
                sizeof(char))) == NULL) {
                LOGD("Error in allocating memory\n");
                return -1;
            }
        strcpy (config_file.duty_cycle_file_paths[index], duty_cycle_read[index]);
        }
    }

    list = config_lookup(&cfg, "pcb_sections");
    if(list) {
        config_file.n_pcb_sections = config_setting_length(list);

        if (config_file.n_pcb_sections > MAX_PCB_SECTIONS)
            config_file.n_pcb_sections = MAX_PCB_SECTIONS;

        for (index = 0; index < config_file.n_pcb_sections; index++) {
            struct config_setting_t *elem = config_setting_get_elem(list, index);
            if (elem) {
                config_setting_lookup_int(elem, "pcb_temp_level",
                 &config_file.pcb_sections[index].pcb_temp_level);
                config_setting_lookup_int(elem, "max_opp",
                 &config_file.pcb_sections[index].max_opp);
                config_setting_lookup_bool(elem, "duty_cycle_enabled",
                 (int *)&config_file.pcb_sections[index].duty_cycle_enabled);
                config_setting_lookup_int(elem, "nitro_rate",
                 &config_file.pcb_sections[index].duty_cycle_params.nitro_rate);
                config_setting_lookup_int(elem, "cooling_rate",
                 &config_file.pcb_sections[index].duty_cycle_params.cooling_rate);
                config_setting_lookup_int(elem, "nitro_interval",
                 &config_file.pcb_sections[index].duty_cycle_params.nitro_interval);
                config_setting_lookup_int(elem, "nitro_percentage",
                 &config_file.pcb_sections[index].duty_cycle_params.nitro_percentage);
            }
        }
    }

    config_destroy(cf);

#ifdef DEBUG
    for (index = 0; index < MAX_SENSORS; index++) {
        LOGD("config_file.temperature_file_sensors[%d] %s\n",
            index, config_file.temperature_file_sensors[index]);
        fflush(stdout);
    }
    LOGD("config_file.omap_cpu_threshold_monitoring %ld\n",
        config_file.omap_cpu_threshold_monitoring);
    fflush(stdout);
    LOGD("config_file.omap_cpu_threshold_alert %ld\n",
        config_file.omap_cpu_threshold_alert);
    fflush(stdout);
    LOGD("config_file.omap_cpu_threshold_panic %ld\n",
        config_file.omap_cpu_threshold_panic);
    fflush(stdout);
    LOGD("config_file.omap_cpu_temperature_slope %ld\n",
        config_file.omap_cpu_temperature_slope);
    fflush(stdout);
    LOGD("config_file.omap_cpu_temperature_offset %ld\n",
        config_file.omap_cpu_temperature_offset);
    fflush(stdout);
    LOGD("config_file.pcb_threshold %ld\n",
        config_file.pcb_threshold);
    fflush(stdout);
    LOGD("config_file.pcb_do_polling %s\n",
        config_file.pcb_do_polling? "yes": "no");
    LOGD("config_file.pcb_polling_interval %d\n",
        config_file.pcb_polling_interval);
    for (index = 0; index < MAX_CPUFREQ_PATHS; index++) {
        LOGD("config_file.cpufreq_file_paths[%d] %s\n",
            index, (char *)config_file.cpufreq_file_paths[index]);
        fflush(stdout);
    }
    for (index = 0; index < MAX_OMAPTEMP_PATHS; index++) {
        LOGD("config_file.omaptemp_file_paths[%d] %s\n",
            index, (char *)config_file.omaptemp_file_paths[index]);
        fflush(stdout);
    }
    for (index = 0; index < MAX_PCBTEMP_PATHS; index++) {
        LOGD("config_file.pcbtemp_file_paths[%d] %s\n",
            index, (char *)config_file.pcbtemp_file_paths[index]);
    }
    for (index = 0; index < MAX_DUTY_CYCLE_PATHS; index++) {
        LOGD("config_file.duty_cycle_file_paths[%d] %s\n",
            index, (char *)config_file.duty_cycle_file_paths[index]);
    }
    for (index = 0; index < config_file.n_pcb_sections; index++) {
        LOGD("config_file.pcb_sections[%d].pcb_temp_level %u\n",
         index, config_file.pcb_sections[index].pcb_temp_level);
        LOGD("config_file.pcb_sections[%d].max_opp %u\n",
         index, config_file.pcb_sections[index].max_opp);
        LOGD("config_file.pcb_sections[%d].duty_cycle_enabled %s\n",
         index, config_file.pcb_sections[index].duty_cycle_enabled ? "yes" : "no");
        LOGD("config_file.pcb_sections[%d].duty_cycle_params.nitro_rate %u\n",
         index, config_file.pcb_sections[index].duty_cycle_params.nitro_rate);
        LOGD("config_file.pcb_sections[%d].duty_cycle_params.cooling_rate %u\n",
         index, config_file.pcb_sections[index].duty_cycle_params.cooling_rate);
        LOGD("config_file.pcb_sections[%d].duty_cycle_params.nitro_interval %u\n",
         index, config_file.pcb_sections[index].duty_cycle_params.nitro_interval);
        LOGD("config_file.pcb_sections[%d].duty_cycle_params.nitro_percentage %u\n",
         index, config_file.pcb_sections[index].duty_cycle_params.nitro_percentage);
    }
#endif
    return 0;
}

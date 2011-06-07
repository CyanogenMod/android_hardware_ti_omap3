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

/* Read in the configuration file and place results in a global
 * structure. Need to read in locally and then copy over as
 * config_destroy will destroy global pointers if read directly
 * into by the config_lookup_xxxx functions */
int read_config (void)
{
    int index;
    config_t cfg, *cf;
    const char *temp_read[MAX_SENSORS];
    const char *cpufreq_read[MAX_CPUFREQ_PATHS];
    const char *omaptemp_read[MAX_OMAPTEMP_PATHS];
    long value;

    cf = &cfg;
    config_init(cf);

    if (!config_read_file (cf, CONFIG_FILE)) {
            fprintf (stderr, "%s:%d = %s\n", CONFIG_FILE,
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
                printf ("Error in allocating memory\n");
                fflush(stdout);
                return -1;
            }
        strcpy (config_file.temperature_file_sensors[index], temp_read[index]);
        }
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

    for (index = 0; index < MAX_CPUFREQ_PATHS; index++) {
            if (config_lookup_string(cf, cpufreq_read_paths[index],
            &cpufreq_read[index])) {
                        if ((config_file.cpufreq_file_paths[index] =
                calloc(strlen(cpufreq_read[index]),
                sizeof(char))) == NULL) {
                printf ("Error in allocating memory\n");
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
                printf ("Error in allocating memory\n");
                fflush(stdout);
                return -1;
            }
        strcpy (config_file.omaptemp_file_paths[index], omaptemp_read[index]);
        }
    }

    config_destroy(cf);

#ifdef DEBUG
    for (index = 0; index < MAX_SENSORS; index++) {
        printf("config_file.temperature_file_sensors[%d] %s\n",
            index, config_file.temperature_file_sensors[index]);
        fflush(stdout);
    }
    printf("config_file.omap_cpu_threshold_monitoring %ld\n",
        config_file.omap_cpu_threshold_monitoring);
    fflush(stdout);
    printf("config_file.omap_cpu_threshold_alert %ld\n",
        config_file.omap_cpu_threshold_alert);
    fflush(stdout);
    printf("config_file.omap_cpu_threshold_panic %ld\n",
        config_file.omap_cpu_threshold_panic);
    fflush(stdout);
    printf("config_file.omap_cpu_temperature_slope %ld\n",
        config_file.omap_cpu_temperature_slope);
    fflush(stdout);
    printf("config_file.omap_cpu_temperature_offset %ld\n",
        config_file.omap_cpu_temperature_offset);
    fflush(stdout);
    printf("config_file.pcb_threshold %ld\n",
        config_file.pcb_threshold);
    fflush(stdout);
    for (index = 0; index < MAX_CPUFREQ_PATHS; index++) {
        printf("config_file.cpufreq_file_paths[%d] %s\n",
            index, (char *)config_file.cpufreq_file_paths[index]);
        fflush(stdout);
    }
    for (index = 0; index < MAX_OMAPTEMP_PATHS; index++) {
        printf("config_file.omaptemp_file_paths[%d] %s\n",
            index, (char *)config_file.omaptemp_file_paths[index]);
        fflush(stdout);
    }
#endif
    return 0;
}

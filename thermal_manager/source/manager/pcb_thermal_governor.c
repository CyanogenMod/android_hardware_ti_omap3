/*
 * OMAP CPU THERMAL GOVERNOR
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

#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <utils/Log.h>
#include "pcb_thermal_governor.h"
#include "read_config.h"

/*
 * Internal functions
 */

/* This table must be ordered by the pcb_temp_lvl */
static int current_pcb_section = -1;
static bool good_to_go = false;
static u32 current_t_high = 0;
static u32 current_t_low = UINT_MAX;

#ifdef DEBUG
static void print_pcb_section(const char *m, const struct pcb_section *c)
{
    LOGD("[%s]: "
        "\tPCB TMP LEVEL: %u"
        "\tMAX OPP: %u"
        "\tDUTY CYCLE ENABLED: %u"
        "\tNITRO RATE: %u"
        "\tCOOLING RATE: %u"
        "\tNITRO INTERVAL: %u"
        "\tNITRO PERCENTAGE: %u",
        m, (uint) c->pcb_temp_level, (uint) c->max_opp,
        (uint) c->duty_cycle_enabled,
        (uint) c->duty_cycle_params.nitro_rate,
        (uint) c->duty_cycle_params.cooling_rate,
        (uint) c->duty_cycle_params.nitro_interval,
        (uint) c->duty_cycle_params.nitro_percentage);
}
#else
static inline void print_pcb_section(const char *m, const struct pcb_section *c)
{
}
#endif

static void update_file(char file[], u32 value)
{
    char buf[SIZE];
    sprintf(buf, "%ld\n", value);
    write_to_file(file, buf);
}

static void update_duty_cycle_nitro_rate(u32 value)
{
    update_file(config_file.duty_cycle_file_paths[NITRO_RATE_PATH], value);
}

static void update_duty_cycle_cooling_rate(u32 value)
{
    update_file(config_file.duty_cycle_file_paths[COOLING_RATE_PATH], value);
}

static void update_duty_cycle_nitro_interval(u32 value)
{
    update_file(config_file.duty_cycle_file_paths[NITRO_INTERVAL_PATH], value);
}

static void update_duty_cycle_nitro_percentage(u32 value)
{
    update_file(config_file.duty_cycle_file_paths[NITRO_PERCENTAGE_PATH], value);
}

static void update_duty_cycle_enabled(u32 value)
{
    update_file(config_file.duty_cycle_file_paths[DUTY_CYCLE_ENABLED_PATH], value);
}

static void update_cpu_scaling_max_freq(u32 value)
{
    update_file(config_file.cpufreq_file_paths[SCALING_MAX_FREQ_PATH], value);
}

/*
 * The goal of these functions is to update the settings of thermal thresholds
 * only if this is required to avoid useless SYSFS accesses.
 */
static void update_t_high(u32 t_high)
{
#ifdef DEBUG
    LOGD("pcb update_t_high (%ld)\n", t_high);
#endif
    update_file(config_file.pcbtemp_file_paths[PCB_THRESHOLD_HIGH_PATH],
            t_high * 1000);
    current_t_high = t_high;
}

static void update_t_low(u32 t_low)
{
#ifdef DEBUG
    LOGD("pcb update_t_low (%ld)\n", t_low);
#endif
    update_file(config_file.pcbtemp_file_paths[PCB_THRESHOLD_LOW_PATH],
            t_low * 1000);
    current_t_low = t_low;
}

/*
 * This function updates both t_high and t_low. The update is done only if
 * the new values are different from the current levels.
 * The caller must ensure that t_high is greater than t_low.
 */
static void update_thresholds(u32 next_t_high, u32 next_t_low)
{
#ifdef DEBUG
    LOGD("H %u-->%u L %u-->%u\n", (uint) current_t_high, (uint) next_t_high,
                    (uint) current_t_low, (uint) next_t_low);
#endif
    if (current_t_high != next_t_high)
        update_t_high(next_t_high);

    if (current_t_low != next_t_low)
        update_t_low(next_t_low);
}

static int pcb_thermal_apply_constraint(const struct pcb_section *c)
{
    int ret = 0;

    update_duty_cycle_enabled(0);
    if (c->duty_cycle_enabled) {
        update_cpu_scaling_max_freq(c->duty_cycle_params.cooling_rate);
        update_duty_cycle_nitro_percentage(c->duty_cycle_params.nitro_percentage);
        update_duty_cycle_nitro_interval(c->duty_cycle_params.nitro_interval);
        update_duty_cycle_cooling_rate(c->duty_cycle_params.cooling_rate);
        update_duty_cycle_nitro_rate(c->duty_cycle_params.nitro_rate);
        update_duty_cycle_enabled(1);
    }
    update_cpu_scaling_max_freq(c->max_opp);

    print_pcb_section("Applied constraint", c);

    return ret;
}

#ifdef DEBUG_TEST
static inline void dump_config(const char *title, unsigned int t)
{
    unsigned int nr, cr, ni, np, max, th, tl;
    nr = atoi(read_from_file(config_file.duty_cycle_file_paths[NITRO_RATE_PATH]));
    cr = atoi(read_from_file(config_file.duty_cycle_file_paths[COOLING_RATE_PATH]));
    ni = atoi(read_from_file(config_file.duty_cycle_file_paths[NITRO_INTERVAL_PATH]));
    np = atoi(read_from_file(config_file.duty_cycle_file_paths[NITRO_PERCENTAGE_PATH]));
    max = atoi(read_from_file(config_file.cpufreq_file_paths[SCALING_MAX_FREQ_PATH]));
    th = atoi(read_from_file(config_file.pcbtemp_file_paths[PCB_THRESHOLD_HIGH_PATH]));
    tl = atoi(read_from_file(config_file.pcbtemp_file_paths[PCB_THRESHOLD_LOW_PATH]));
    LOGD("[%s] configuration for temp %u: "
        "\tNITRO_RATE: %u"
        "\tCOOLING_RATE: %u"
        "\tNITRO_INTERVAL: %u"
        "\tNITRO_PERCENTAGE: %u"
        "\tSCALING_MAX_FREQ: %u"
        "\tPCB T HIGH: %u"
        "\tPCB T LOW: %u",
        title, t, nr, cr, ni, np, max, th, tl);
}

void test_pcb_thermal_governor(void)
{
    unsigned int i;

    for (i = 0; i < 80; i++) {

        LOGD(   "***********************\n"
            "Testing temperature %02u\n"
            "***********************\n", i);

        pcb_thermal_governor(i);
        dump_config("Resulting", i);
        LOGD(   "***********************\n" );
    }
}
#else
static inline void test_pcb_thermal_governor(void)
{
}
#endif

static void pcb_update_status(void)
{
    int pcb_temp;

    pcb_temp = atoi(read_from_file(
        config_file.temperature_file_sensors[PCB_SENSOR_TEMP_PATH]));
    pcb_thermal_governor((u32)pcb_temp / 1000);
}

static void do_polling_pcb(void)
{
    /* Our process ID and Session ID */
    pid_t pid, sid;

    LOGD("%s: starting the daemonizing process", __func__);

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    /* If we got a good PID, then
       we can return the parent process. */
    if (pid > 0)
        return;


    /* now the serious part */
    while (1) {
        pcb_update_status();
        usleep(config_file.pcb_polling_interval);
    }
}

static void sanity_check(void)
{
    struct stat s;

    stat(config_file.duty_cycle_file_paths[NITRO_RATE_PATH], &s);
    good_to_go = S_ISREG(s.st_mode); memset(&s, 0, sizeof(s));
    stat(config_file.duty_cycle_file_paths[COOLING_RATE_PATH], &s);
    good_to_go = good_to_go && S_ISREG(s.st_mode); memset(&s, 0, sizeof(s));
    stat(config_file.duty_cycle_file_paths[NITRO_INTERVAL_PATH], &s);
    good_to_go = good_to_go && S_ISREG(s.st_mode); memset(&s, 0, sizeof(s));
    stat(config_file.duty_cycle_file_paths[NITRO_PERCENTAGE_PATH], &s);
    good_to_go = good_to_go && S_ISREG(s.st_mode); memset(&s, 0, sizeof(s));
    stat(config_file.duty_cycle_file_paths[DUTY_CYCLE_ENABLED_PATH], &s);
    good_to_go = good_to_go && S_ISREG(s.st_mode); memset(&s, 0, sizeof(s));
    if (!good_to_go) {
        LOGE("%s: Duty cycle module for 4430 not available. "
            "Disabling PCB governor.", __func__);
        return;
    }
    stat(config_file.cpufreq_file_paths[SCALING_MAX_FREQ_PATH], &s);
    good_to_go = good_to_go && S_ISREG(s.st_mode); memset(&s, 0, sizeof(s));
    if (!good_to_go) {
        LOGE("%s: CPUfreq not available. "
            "Disabling PCB governor.", __func__);
        return;
    }

    stat(config_file.temperature_file_sensors[PCB_SENSOR_TEMP_PATH], &s);
    good_to_go = good_to_go && S_ISREG(s.st_mode); memset(&s, 0, sizeof(s));
    stat(config_file.pcbtemp_file_paths[PCB_THRESHOLD_HIGH_PATH], &s);
    good_to_go = good_to_go && S_ISREG(s.st_mode); memset(&s, 0, sizeof(s));
    stat(config_file.pcbtemp_file_paths[PCB_THRESHOLD_LOW_PATH], &s);
    good_to_go = good_to_go && S_ISREG(s.st_mode);
    if (!good_to_go) {
        LOGE("%s: PCB sensor not available. "
            "Disabling PCB governor.", __func__);
        return;
    }

    if (config_file.n_pcb_sections == 0) {
        LOGE("%s: No PCB sections defined. Nothing to do."
            "Disabling the PCB governor.", __func__);
        good_to_go = false;
        return;
    }
}

static int compare_pcb_sections(const void *a, const void *b)
{
    const struct pcb_section *sa = a, *sb = b;

    if (sa && sb)
        return sa->pcb_temp_level - sb->pcb_temp_level;

    /* Not sure what would be the outcoming */
    return 0;
}

/*
 *
 * External functions
 *
 */

int pcb_thermal_governor(u32 pcb_temp)
{
    u32 min;
    int i;
    int ret = 0;
    struct pcb_section *pcb_sections =
        config_file.pcb_sections;

#ifdef DEBUG_TEST
    char tbuf[20];
    struct timeval t;
    gettimeofday(&t, NULL);
    sprintf(tbuf, "[%5lu.%06lu] ",
                (unsigned long) t.tv_sec,
                (unsigned long) t.tv_usec);
    LOGD("%sPCB Thermal Governor (%ld)\n", tbuf, pcb_temp);
#endif

    if (!good_to_go)
        return 0;

    for (i = 0; i < config_file.n_pcb_sections; i++)
        if (pcb_sections[i].pcb_temp_level > pcb_temp)
            break;

    /* in case none are found, apply the most restrictive */
    if (i >= config_file.n_pcb_sections)
        i = config_file.n_pcb_sections - 1;

    if (i != current_pcb_section) {
        current_pcb_section = i;
        ret = pcb_thermal_apply_constraint(pcb_sections + i);

        /*
         * TODO: This is not tested, need to be verified on a board
         * with PCB sensor which can generate interrupts.
         */
        if (i == 0)
            min = 0;
        else
            min = pcb_sections[i - 1].pcb_temp_level;
        update_thresholds(pcb_sections[i].pcb_temp_level, min);
    }

    return ret;
}

void init_pcb_thermal_governor(void)
{
    sanity_check();

    if (!good_to_go)
        return;

    qsort(config_file.pcb_sections, config_file.n_pcb_sections,
        sizeof(config_file.pcb_sections[0]), compare_pcb_sections);

    test_pcb_thermal_governor();

    pcb_update_status();

    if (config_file.pcb_do_polling)
        do_polling_pcb();
}

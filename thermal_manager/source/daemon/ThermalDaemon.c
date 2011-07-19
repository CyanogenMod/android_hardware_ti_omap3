/*
 * Thermal Daemon
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Dan Murphy (dmurphy@ti.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the dual BSD / GNU General Public License version 2 as
 * published by the Free Software Foundation. When using or
 * redistributing this file, you may do so under either license.
 */

/* OS-specific headers */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>

#include <utils/Log.h>

#include <poll.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <linux/netlink.h>

#include "../../include/thermal_manager.h"

extern int thermal_read_state(const char *string);

#if defined (__cplusplus)
extern "C" {
#endif /* defined (__cplusplus) */

#define TD_DEBUG 1

/* OMAP CPU definitions */
#define CPU_NAME "omap_cpu"
#define CPU_UEVENT "change@/devices/platform/omap_temp_sensor.0/hwmon/hwmon1"

/* LPDDR definitions */
#define LPDDR_NAME "lpddr2"
/* TODO: Find the uevent */
#define LPDDR1_UEVENT "change@/devices/platform/omap_emif.0"
#define LPDDR2_UEVENT "change@/devices/platform/omap_emif.1"

/* PCB definitions */
#define PCB_NAME "pcb"
/* TODO: Find the uevent */
#define PCB_UEVENT "change@/devices/platform/i2c_omap.3/i2c-3/3-0048/hwmon/hwmon0"


static pthread_t thermalDaemonThrd;
static int fd;
static int cpu_state;
static int pcb_state;
static int lpddr_state;

struct event_logging_sysfs {
    const char *name;
} event_sysfs[] = {
    {"/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"},
    {"/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"},
    {"/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"},
    {"/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq"},
    {"/sys/class/hwmon/hwmon1/device/temp1_max"},
    {"/sys/class/hwmon/hwmon1/device/temp1_max_hyst"},
    {"/sys/class/hwmon/hwmon1/device/update_rate"},
};

static void ThermalDaemonEventLog(void)
{
    char buffer[1024];
    int bytes_read = 0;
    int sys_fd = -1;
    int i = 0;
    int sysfs_count = sizeof(event_sysfs) / sizeof(event_sysfs[0]);

#ifdef TD_DEBUG
    for (i = 0; i < sysfs_count; i++) {
        sys_fd = open(event_sysfs[i].name, O_RDWR);
        if (sys_fd >= 0) {
            bytes_read = read(sys_fd, buffer, 1024);
            LOGD("ThermalDaemon:sysfs %s = %s\n",
                event_sysfs[i].name, buffer);
            close(sys_fd);
            sys_fd = -1;
        }
    }
#else
    return;
#endif

}

static void ThermalDaemonEventHandler(void)
{
    struct sockaddr_nl addr;
    struct pollfd fds;

    char buffer[1024];
    int length = 1024;
    int sz = 64*1024;
    int s;
    int manager_status = -1;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if(s < 0) {
        LOGD("Thermal Daemon: Cannot create a socket\n");
        return;
    }

    setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

    if(bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        LOGD("Thermal Daemon:Cannot bind the socket\n");
        return;
    }
    /* Call into the thermal library */
    manager_status = thermal_manager_init((OMAP_CPU | PCB));
    ThermalDaemonEventLog();
    fd = s;

    while(1) {
        int nr = 0;
        fds.fd = fd;
        fds.events = POLLIN;
        fds.revents = 0;
        nr = poll(&fds, 1, -1);
        if(nr > 0 && fds.revents == POLLIN) {
            int count = recv(fd, &buffer, length, 0);
            if(strcmp(buffer, CPU_UEVENT) == 0) {
#ifdef TD_DEBUG
            LOGD("ThermalDaemon:Uevent posted from %s\n", buffer);
#endif
                if (manager_status & OMAP_CPU) {
                    LOGD("ThermalDaemon:Calling the algo with %s\n",
                         CPU_NAME);

                    cpu_state = thermal_manager_algo(CPU_NAME);
                } else
                    goto emif_check;

                switch (cpu_state) {
                case SAFE_ZONE:
                    LOGD("ThermalDaemon:CPU is in the safe zone\n");
                    ThermalDaemonEventLog();
                    break;
                case MONITOR_ZONE:
                    LOGD("ThermalDaemon:CPU is in the monitoring zone\n");
                    ThermalDaemonEventLog();
                    break;
                case ALERT_ZONE:
                    LOGD("ThermalDaemon:CPU is in the alert zone\n");
                    ThermalDaemonEventLog();
                    break;
                case PANIC_ZONE:
                    LOGD("ThermalDaemon:CPU is in the panic zone\n");
                    ThermalDaemonEventLog();
                    break;
                case FATAL_ZONE:
                    LOGD("ThermalDaemon:CPU is in the fatal zone\n");
                    ThermalDaemonEventLog();
                    break;
                case NO_ACTION:
                default:
                    LOGD("ThermalDaemon:No action taken on cpu thermal event\n");
                    break;
                };
            }
emif_check:
            if((strcmp(buffer, LPDDR1_UEVENT) == 0) ||
                (strcmp(buffer, LPDDR2_UEVENT) == 0)) {
#ifdef TD_DEBUG
                LOGD("ThermalDaemon:Uevent posted from %s\n", buffer);
#endif
                LOGD("ThermalDaemon:Calling the algo with %s\n", LPDDR_NAME);
                lpddr_state = thermal_manager_algo(LPDDR_NAME);
            }

pcb_check:
            if(strcmp(buffer, PCB_UEVENT) == 0) {
#ifdef TD_DEBUG
                LOGD("ThermalDaemon:Uevent posted from %s\n", buffer);
#endif
                if (manager_status & PCB) {
                    LOGD("ThermalDaemon:Calling the algo with %s\n",
                        PCB_NAME);
                    pcb_state = thermal_manager_algo(PCB_NAME);
                } else
                    continue;

                switch (pcb_state) {
                default:
                    LOGD("ThermalDaemon:No action taken on PCB thermal event\n");
                    break;
                }
            }
        }
    } /* End of while */
    return;
}

static void signal_handler(int sig)
{
    LOGD("Thermal Daemon: Thermal daemon is exiting...\n");
    pthread_exit(&thermalDaemonThrd);
    exit(0);
}


int thermal_read_state(const char *string)
{
    if(strcmp(string, CPU_NAME))
        return cpu_state;
    else if(strcmp(string, PCB_NAME))
        return pcb_state;
    else if(strcmp(string, LPDDR_NAME))
        return lpddr_state;
    else
        return -1;
}

int main(int argc, char * argv [])
{
    int status = 0;

    cpu_state = -1;
    pcb_state = -1;
    lpddr_state = -1;
    fd = -1;

    /* Setup the signal handlers */
    LOGD("ThermalDaemon:main:Spawning the demon\n");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    LOGD("Spawning Thermal Daemon thread...\n");
    status = pthread_create(&thermalDaemonThrd, NULL,
        (void *)&ThermalDaemonEventHandler, NULL);
    if (status) {
        LOGD("Thermal Daemon thread failed to be created %i\n", status);
        exit(1);
    }

    status = pthread_join(thermalDaemonThrd, NULL);
    if (status) {
        LOGD("Thermal Daemon join failed to be created %i\n", status);
        exit(1);
    }

    return 0;
}


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */

/*
 * SYSFS functions to read/write SYSFS attributes
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

#include "sysfs.h"

char *read_from_file(const char *path)
{
    FILE *fp;
    char *buf;
    char buffer[SIZE];
    int i;

    if (!path)
        exit(FILE_OPEN_ERROR);

    fp = fopen(path, "r");
    if (fp == NULL) {
#ifdef DEBUG
        LOGD("!!! Could not open (read mode)'%s' !!!\n", path);
        fflush(stdout);
#endif
        exit(FILE_OPEN_ERROR);
    } else {
#ifdef DEBUG
        LOGD("Opened in read mode %s\n", path);
        fflush(stdout);
#endif
    }

    if (fread(buffer, sizeof(buffer), 1, fp) == 0) {
        fclose(fp);
        for (i = 0; i < (int)sizeof(buffer); i++) {
            if (buffer[i] == '\n') {
                buffer[i] = '\0';
                break;
            }
        }
        /* FIXME: clean-up this copy from buffer to buf */
        buf = strtok(buffer, ".");
        return buf;
    } else {
        fclose(fp);
        exit(FILE_OPEN_ERROR);
    }
}


int write_to_file(const char *path, const char *buf)
{
    FILE *fp;

    if (!path)
        return -1;

    fp = fopen(path, "w");
    if (fp == NULL) {
#ifdef DEBUG
        LOGD("!!! Could not open (write mode)'%s' !!!\n", path);
        fflush(stdout);
#endif
        return -1;
    } else {
#ifdef DEBUG
        LOGD("Opened in write mode %s to write %s\n", path, buf);
        fflush(stdout);
#endif
    }
    fwrite(buf, strlen(buf), 1, fp);
    fclose(fp);
    return 0;
}

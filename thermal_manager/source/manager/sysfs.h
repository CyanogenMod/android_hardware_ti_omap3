/*
 * SYSFS functions to read/write SYSFS attributes (header file)
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

#ifndef _SYSFS_H
#define _SYSFS_H

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

typedef unsigned long u32;

/* Return definitions */
#define RET_OK 0
#define GENERAL_ERROR 1
#define MEMORY_ERROR 2
#define CONFIGURATION_ERROR 3
#define FILE_OPEN_ERROR 4
#define FILE_SIZE_ERROR 5

#define SIZE 256
/*
 *
 * External functions
 *
 */
int write_to_file(const char *path, const char *buf);
char *read_from_file(const char *path);

#endif

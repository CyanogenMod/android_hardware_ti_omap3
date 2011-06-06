/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _THERMAL_MANAGER_H
#define _THERMAL_MANAGER_H

#if __cplusplus
extern "C" {
#endif

#define OMAP_CPU (1 << 0)
#define EMIF1      (1 << 1)
#define EMIF2      (1 << 2)
#define PCB      (1 << 3)

#define OMAP_CPU_FILE 0
#define EMIF1_FILE    1
#define EMIF2_FILE    2
#define PCB_FILE      3

/* CPU Zone information */
#define FATAL_ZONE    5
#define PANIC_ZONE    4
#define ALERT_ZONE    3
#define MONITOR_ZONE  2
#define SAFE_ZONE     1
#define NO_ACTION     0

int thermal_manager_algo(const char *string);
int thermal_manager_init(int type);

#if __cplusplus
}  // extern "C"
#endif

#endif  // _THERMAL_MANAGER_H

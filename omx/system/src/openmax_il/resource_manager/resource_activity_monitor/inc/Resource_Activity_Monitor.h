
/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*  this file defines an interface to the kernel PM framework
 *  MultiMedia components can request opp based on requirements/constraints
 *  that the corresponding driver may be unaware of. This ensures there will be 
 *  enough throughput, etc for all multimedia use cases.
 * 
 *  In general, the very existence if this interface serves only to provide a lower
 *  latency mechanism than the Bridge Load Predictor (which itself is is actually just
 *  a passive monitor that takes ~400ms to respond to the requirered constraints from DSP
 *  nodes.
*/

#undef RAM_DEBUG

#ifdef RAM_DEBUG
  #include <utils/Log.h>
  #undef LOG_TAG
  #define LOG_TAG "OMXRM DVFS MONITOR"
  #define RAM_DPRINT LOGD
#else
  #define RAM_DPRINT(...)
#endif

/* define these for array indexing */
#define OPERATING_POINT_1 0
#define OPERATING_POINT_2 1
#define OPERATING_POINT_3 2
#define OPERATING_POINT_4 3
#define OPERATING_POINT_5 4

/* for 3440 */
#define OPERATING_POINT_6 5

#if 0
typedef enum _OPP_LEVEL
{
    OPERATING_POINT_1 = 1,
    OPERATING_POINT_2 = 2,
    OPERATING_POINT_3 = 3,
    OPERATING_POINT_4 = 4,
    OPERATING_POINT_5 = 5,
    OPERATING_POINT_6 = 6,
    OPERATING_POINT_NOT_SUPPORTED = -1,

} OPP_LEVEL;
#endif

typedef enum _BOOST_LEVEL
{
        MAX_BOOST = 0,
        NOMINAL_BOOST,
} BOOST_LEVEL;

typedef enum _C_STATE
{
  C_STATE_1 = 0,
  C_STATE_2,
  C_STATE_3,
  C_STATE_4,
  C_STATE_5,
  C_STATE_6,
  C_STATE_7,
  C_STATE_8,
  C_STATE_9,
  C_STATE_NOT_SUPPORTED /*check wheater kernel supports more*/
} C_STATE;

typedef enum _OMAP_CPU
{
    OMAP3420_CPU = 0,
    OMAP3430_CPU,
    OMAP3440_CPU,
    OMAP3630_CPU,
    OMAP_NOT_SUPPORTED /* this should always be at the end of the list */
} OMAP_CPU;

/* for 3420 family */
static const int vdd1_dsp_mhz_3420[5] = {90, 180, 360, 360, 360};
static const int vdd1_mpu_mhz_3420[5] = {125, 250, 500, 550, 600};

/* for 3430 family */
static const int vdd1_dsp_mhz_3430[5] = {90, 180, 360, 430, 430};
static const int vdd1_mpu_mhz_3430[5] = {125, 250, 500, 550, 600};

/* for 3440 family */
static const int vdd1_dsp_mhz_3440[6] = {90, 180, 360, 430, 430, 520};
static const int vdd1_mpu_mhz_3440[6] = {125, 250, 500, 550, 600, 720};

 /* for 3630 family */
static const int vdd1_dsp_mhz_3630[5] = {260, 520, 660, 800};
static const int vdd1_mpu_mhz_3630[5] = {300, 600, 800, 1000};

/*old for opp based constraints*/
int rm_set_vdd1_constraint(int MHz);
int rm_get_vdd1_constraint();

int dsp_mhz_to_vdd1_opp(int MHz);

/*new for frequency based constraints */
int rm_set_min_scaling_freq(int MHz);
int rm_get_min_scaling_freq();

int dsp_mhz_to_min_scaling_freq(int MHz);

int get_omap_version();
int get_curr_cpu_mhz(int omapVersion);
int get_dsp_max_freq();

void rm_request_boost(int level);
void rm_release_boost();

char * ram_itoa(int a);


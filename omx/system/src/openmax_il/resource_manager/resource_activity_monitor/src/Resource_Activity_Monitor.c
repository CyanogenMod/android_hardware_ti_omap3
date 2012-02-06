
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "Resource_Activity_Monitor.h"

#undef LOG_TAG
#define LOG_TAG "OMXRM DVFS MONITOR"


/* global to keep the current constraint across requests
   this will be used to reset constraints if no MM is active */
int currentMHzConstraint = 0;
int bBoostOn = 0;

/*
   Description : This function will determine the correct opp 
                 and write it to the vdd1_opp sysfs
   
   Parameter   : MHz is the sum of MM requested MHz
   
   Return      : the integer value of the opp that was set
   
*/
int rm_set_vdd1_constraint(int MHz)
{
    int vdd1_opp = OPERATING_POINT_1;
    
#ifdef DVFS_ENABLED
    
    char command[100];
    /* for any MM case, set c-state to 3, unless no mm is active */
    int c_state = C_STATE_2;
    
    currentMHzConstraint = MHz;
    if(MHz == 0)
    {
        /* clear constraints for idle MM case */
        vdd1_opp = OPERATING_POINT_1;
        
        /* set c-state back if no active MM */
        if (get_omap_version() == OMAP3630_CPU)
        {
            c_state = C_STATE_9;
        }
        else
        {
           /*rest omap version support up to 6*/
            c_state = C_STATE_6;
        }
    }
    else
    {
        vdd1_opp = dsp_mhz_to_vdd1_opp(MHz);
    }

    /* plus one to convert zero based array indeces above */
    vdd1_opp++;

    if(!bBoostOn || (bBoostOn && rm_get_vdd1_constraint() < vdd1_opp))
    {
        /* actually set the sysfs for vdd1 */
        RAM_DPRINT("[setting operating point] MHz = %d vdd1_dsp = %d\n",MHz,vdd1_opp);
        strcpy(command,"echo ");
        strcat(command,ram_itoa(vdd1_opp));
        strcat(command," > /sys/power/dsp_opp");
        system(command);

        /* actually set the sysfs for cpuidle/max_state */
        RAM_DPRINT("[setting c-state] c-state %d\n",c_state);
        strcpy(command,"echo ");
        strcat(command,ram_itoa(c_state));
        strcat(command," > /sys/devices/system/cpu/cpu0/cpuidle/max_state");
        system(command);
    }
    else {
        RAM_DPRINT("BOOST is enabled, ignoring current request\n");
    }

#endif

    return vdd1_opp;
}

/*
   Description : This function will convert a MHz value to an opp value
   
   Parameter   : MHz to base the conversion on
   
   Return      : the appropriate OPP based on the supplied MHz
   
*/
int dsp_mhz_to_vdd1_opp(int MHz)
{
    unsigned int vdd1_opp = OPERATING_POINT_1;
    int cpu_variant = get_omap_version();
    
    switch (cpu_variant) {
    case OMAP3420_CPU:
        if (MHz <= vdd1_dsp_mhz_3420[OPERATING_POINT_2]) {
            /* MM should never use opp1, so skip to opp2 */
            vdd1_opp = OPERATING_POINT_2;
        }
        else if (MHz <= vdd1_dsp_mhz_3420[OPERATING_POINT_3]) {
            vdd1_opp = OPERATING_POINT_3;
        }
        else if (MHz <= vdd1_dsp_mhz_3420[OPERATING_POINT_4]) {
            vdd1_opp = OPERATING_POINT_4;
        }
        else {
            vdd1_opp = OPERATING_POINT_5;
        }
        break;
        
        case OMAP3440_CPU:
        if (MHz <= vdd1_dsp_mhz_3440[OPERATING_POINT_2]) {
            /* MM should never use opp1, so skip to opp2 */
            vdd1_opp = OPERATING_POINT_2;
        }
        else if (MHz <= vdd1_dsp_mhz_3440[OPERATING_POINT_3]) {
            vdd1_opp = OPERATING_POINT_3;
        }
        else if (MHz <= vdd1_dsp_mhz_3440[OPERATING_POINT_4]) {
            vdd1_opp = OPERATING_POINT_4;
        }
        else if (MHz <= vdd1_dsp_mhz_3440[OPERATING_POINT_5]) {
            vdd1_opp = OPERATING_POINT_5;
        }
        else {
            vdd1_opp = OPERATING_POINT_6;
        }
        break;
        
        case OMAP3430_CPU:
        if (MHz <= vdd1_dsp_mhz_3430[OPERATING_POINT_2]) {
            /* MM should never use opp1, so skip to opp2 */
            vdd1_opp = OPERATING_POINT_2;
        }
        else if (MHz <= vdd1_dsp_mhz_3430[OPERATING_POINT_3]) {
            vdd1_opp = OPERATING_POINT_3;
        }
        else if (MHz <= vdd1_dsp_mhz_3430[OPERATING_POINT_4]) {
            vdd1_opp = OPERATING_POINT_4;
        }
        else {
            vdd1_opp = OPERATING_POINT_5;
        }
        break;

        case OMAP3630_CPU:
        if (MHz <= vdd1_dsp_mhz_3630[OPERATING_POINT_1]) {
            /* on 3630, opp1 is OK */
            vdd1_opp = OPERATING_POINT_1;
        }
        else if (MHz <= vdd1_dsp_mhz_3630[OPERATING_POINT_2]) {
            vdd1_opp = OPERATING_POINT_2;
        }
        else if (MHz <= vdd1_dsp_mhz_3630[OPERATING_POINT_3]) {
            vdd1_opp = OPERATING_POINT_3;
        }
        else {
            vdd1_opp = OPERATING_POINT_4;
        }
        break;

        default:
            RAM_DPRINT("this omap is not currently supported\n");	
            return OMAP_NOT_SUPPORTED;
        break;
    } /* end switch */
    return vdd1_opp;
}

/*
   Description : This function will determine the current OMAP that is
                 running
   
   Parameter   : n/a
   
   Return      : integer enum representing the version of OMAP;
                 returns OMAP_NOT_SUPPORTED if not supported by this version of RM,
                 otherwise returns the enum value for the correct omap
*/
int get_omap_version()
{
    int cpu_variant = 0;
    int dsp_max_freq = 0;

    dsp_max_freq = get_dsp_max_freq();

    if (dsp_max_freq == vdd1_dsp_mhz_3420[OPERATING_POINT_5]){
        cpu_variant = OMAP3420_CPU;
    }
    else if (dsp_max_freq == vdd1_dsp_mhz_3430[OPERATING_POINT_5]){
        cpu_variant = OMAP3430_CPU;
    }
    else if (dsp_max_freq == vdd1_dsp_mhz_3440[OPERATING_POINT_6]){
        /* 3440 has 6 OPPs */
        cpu_variant = OMAP3440_CPU;
    }
    else if (dsp_max_freq == vdd1_dsp_mhz_3630[OPERATING_POINT_4]){
        cpu_variant = OMAP3630_CPU;
    }
    else {
        cpu_variant = OMAP_NOT_SUPPORTED;
    }

    return cpu_variant;

}



/*
   Description : This function will get the max frequency of IVA
   
   Parameter   : n/a
   
   Return      : the max frequency (in MHz) of IVA according to /sys/power/max_dsp_frequency
   
*/
int get_dsp_max_freq()
{
    int dsp_max_freq = 0;

    FILE *fp = fopen("/sys/power/max_dsp_frequency","r");
    /* what to do if the kernel does not have this sysfs? */
    if (fp == NULL) {
        RAM_DPRINT("open file failed\n");
    }
    else {
        fscanf(fp, "%d",&dsp_max_freq);
        fclose(fp);
        dsp_max_freq /= 1000000;
    }

    return dsp_max_freq;
}

/*
   Description : This function will get the current ARM frequency
   
   Parameter   : integer enum for OMAP_VERSION
   
   Return      : the nearest opp (in MHz) to the current MHz
                 that ARM is actually operating at
   
*/
int get_curr_cpu_mhz(int omapVersion){

    int maxMhz = 0;
    int cur_freq = 0;
    int cpu_variant = 0;

#ifdef DVFS_ENABLED

    cpu_variant = get_omap_version();
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq","r");
    if (fp == NULL) {
       RAM_DPRINT("open file cpuinfo_cur_freq failed\n");
       return -1;
    }
    fscanf(fp, "%d",&cur_freq);
    fclose(fp);
    cur_freq /= 1000;

    if (cpu_variant == OMAP3420_CPU){
        if (cur_freq == vdd1_mpu_mhz_3420[OPERATING_POINT_1]) {
            maxMhz = vdd1_dsp_mhz_3420[OPERATING_POINT_1];
        }
        else if (cur_freq == vdd1_mpu_mhz_3420[OPERATING_POINT_2]) {
            maxMhz = vdd1_dsp_mhz_3420[OPERATING_POINT_2];
        }
        else if (cur_freq == vdd1_mpu_mhz_3420[OPERATING_POINT_3]) {
            maxMhz = vdd1_dsp_mhz_3420[OPERATING_POINT_3];
        }
        else if (cur_freq == vdd1_mpu_mhz_3420[OPERATING_POINT_4]) {
            maxMhz = vdd1_dsp_mhz_3420[OPERATING_POINT_4];
        }
        else if (cur_freq == vdd1_mpu_mhz_3420[OPERATING_POINT_5]) {
            maxMhz = vdd1_dsp_mhz_3420[OPERATING_POINT_5];
        }
        else {
            RAM_DPRINT("Read incorrect frequency from sysfs 3430\n");
            return NULL;
        }
    }
    else if (cpu_variant == OMAP3430_CPU){
        if (cur_freq == vdd1_mpu_mhz_3430[OPERATING_POINT_1]) {
            maxMhz = vdd1_dsp_mhz_3430[OPERATING_POINT_1];
        }
        else if (cur_freq == vdd1_mpu_mhz_3430[OPERATING_POINT_2]) {
            maxMhz = vdd1_dsp_mhz_3430[OPERATING_POINT_2];
        }
        else if (cur_freq == vdd1_mpu_mhz_3430[OPERATING_POINT_3]) {
            maxMhz = vdd1_dsp_mhz_3430[OPERATING_POINT_3];
        }
        else if (cur_freq == vdd1_mpu_mhz_3430[OPERATING_POINT_4]) {
            maxMhz = vdd1_dsp_mhz_3430[OPERATING_POINT_4];
        }
        else if (cur_freq == vdd1_mpu_mhz_3430[OPERATING_POINT_5]) {
            maxMhz = vdd1_dsp_mhz_3430[OPERATING_POINT_5];
        }
        else {
            RAM_DPRINT("Read incorrect frequency from sysfs 3430\n");
            return NULL;
        }
    }
    else if (cpu_variant == OMAP3440_CPU){
        if (cur_freq == vdd1_mpu_mhz_3440[OPERATING_POINT_1]) {
            maxMhz = vdd1_dsp_mhz_3440[OPERATING_POINT_1];
        }
        else if (cur_freq == vdd1_mpu_mhz_3440[OPERATING_POINT_2]) {
            maxMhz = vdd1_dsp_mhz_3440[OPERATING_POINT_2];
        }
        else if (cur_freq == vdd1_mpu_mhz_3440[OPERATING_POINT_3]) {
            maxMhz = vdd1_dsp_mhz_3440[OPERATING_POINT_3];
        }
        else if (cur_freq == vdd1_mpu_mhz_3440[OPERATING_POINT_4]) {
            maxMhz = vdd1_dsp_mhz_3440[OPERATING_POINT_4];
        }
        else if (cur_freq == vdd1_mpu_mhz_3440[OPERATING_POINT_5]) {
            maxMhz = vdd1_dsp_mhz_3440[OPERATING_POINT_5];
        }
        else if (cur_freq == vdd1_mpu_mhz_3440[OPERATING_POINT_6]) {
            maxMhz = vdd1_dsp_mhz_3440[OPERATING_POINT_6];
        }
        else {
            RAM_DPRINT("Read incorrect frequency from sysfs 3430\n");
            return NULL;
        }
    }
    else if (cpu_variant == OMAP3630_CPU){
        if (cur_freq == vdd1_mpu_mhz_3630[OPERATING_POINT_1]) {
            maxMhz = vdd1_dsp_mhz_3630[OPERATING_POINT_1];
        }
        else if (cur_freq == vdd1_mpu_mhz_3630[OPERATING_POINT_2]) {
            maxMhz = vdd1_dsp_mhz_3630[OPERATING_POINT_2];
        }
        else if (cur_freq == vdd1_mpu_mhz_3630[OPERATING_POINT_3]) {
            maxMhz = vdd1_dsp_mhz_3630[OPERATING_POINT_3];
        }
        else if (cur_freq == vdd1_mpu_mhz_3630[OPERATING_POINT_4]) {
            maxMhz = vdd1_dsp_mhz_3630[OPERATING_POINT_4];
        }
        else {
            RAM_DPRINT("Read incorrect frequency from sysfs 3630\n");
            return NULL;
        }
    }
#else
    // if DVFS is not available, use NOMINAL constraints
    if (cpu_variant == OMAP3420_CPU){
        maxMhz = vdd1_dsp_mhz_3420[OPERATING_POINT_4];
    }
    else if (cpu_variant == OMAP3430_CPU){
        maxMhz = vdd1_dsp_mhz_3430[OPERATING_POINT_4];
    }
    else if (cpu_variant == OMAP3440_CPU){
        maxMhz = vdd1_dsp_mhz_3440[OPERATING_POINT_4];
    }
    else if (cpu_variant == OMAP3630_CPU){
        maxMhz = vdd1_dsp_mhz_3630[OPERATING_POINT_3];
    }
#endif
    return maxMhz;
}

/*
   Description : This function will get the vdd1_opp
   
   Parameter   : n/a
   
   Return      : vdd1 OPP 1 -- 6 
   
*/
int rm_get_vdd1_constraint()
{
    int vdd1_opp = 0;
    FILE *fp = fopen("/sys/power/dsp_opp","r");
    if (fp == NULL) {
        RAM_DPRINT("open file failed\n");
    } else {
        fscanf(fp, "%d",&vdd1_opp);
        fclose(fp);
        RAM_DPRINT("[rm_get_vdd1_constraint] vdd1 OPP = %d \n",vdd1_opp);
    }
    return vdd1_opp;
}

/*
   Description : This function will boost vdd1_opp to either nominal or max opp
                 There MUSt be a 1:1 correspondence with rm_release_boost()
   
   Parameter   : level, either MAX_BOOST or NOMINAL_BOOST
   
   Return      : void
   
*/
void rm_request_boost(int level)
{
    int vdd1_opp = rm_get_vdd1_constraint();
    int boostConstraintMHz = 0;
    int cpu_variant =  get_omap_version();
    
    if (level == MAX_BOOST)
    {
        switch (cpu_variant)
        {
            case OMAP3420_CPU:
                boostConstraintMHz = vdd1_dsp_mhz_3420[ sizeof(vdd1_dsp_mhz_3420)/sizeof(vdd1_dsp_mhz_3420[0]) -1];
                break;
                
            case OMAP3440_CPU:
                boostConstraintMHz = vdd1_dsp_mhz_3440[ sizeof(vdd1_dsp_mhz_3440)/sizeof(vdd1_dsp_mhz_3440[0]) -1];
                break;

            case OMAP3630_CPU:
                boostConstraintMHz = vdd1_dsp_mhz_3630[ sizeof(vdd1_dsp_mhz_3630)/sizeof(vdd1_dsp_mhz_3630[0]) -1];
                break;
                
            case OMAP3430_CPU:
                boostConstraintMHz = vdd1_dsp_mhz_3430[ sizeof(vdd1_dsp_mhz_3430)/sizeof(vdd1_dsp_mhz_3430[0]) -1];
                break;

            default:
                RAM_DPRINT("boost not set, current OMAP not supported\n");
                break;
        }
    }
    else if (level == NOMINAL_BOOST)
    { /* NOMITNAL is defined as 1 less than MAX OPP */
        switch (cpu_variant)
        {
            case OMAP3420_CPU:
                boostConstraintMHz = vdd1_dsp_mhz_3420[sizeof(vdd1_dsp_mhz_3420)/sizeof(vdd1_dsp_mhz_3420[0]) - 2];
                break;
                
            case OMAP3440_CPU:
                boostConstraintMHz = vdd1_dsp_mhz_3440[sizeof(vdd1_dsp_mhz_3440)/sizeof(vdd1_dsp_mhz_3440[0]) -2];
                break;

            case OMAP3630_CPU:
                boostConstraintMHz = vdd1_dsp_mhz_3630[sizeof(vdd1_dsp_mhz_3630)/sizeof(vdd1_dsp_mhz_3630[0]) -2];
                break;
                
            case OMAP3430_CPU:
                boostConstraintMHz = vdd1_dsp_mhz_3430[sizeof(vdd1_dsp_mhz_3430)/sizeof(vdd1_dsp_mhz_3430[0]) -2];
                break;

            default:
                RAM_DPRINT("boost not set, current OMAP not supported\n");
                break; 
        }
    }
   
    rm_set_vdd1_constraint(boostConstraintMHz);
    bBoostOn = 1;
}

/*
   Description : In case any request came while boost is ON, this function will
                 update vdd1_opp to the most recent requested OPP.
                 This MUST be called after each use of rm_request_boost()
   
   Parameter   : n/a
   
   Return      : n/a
   
*/
void rm_release_boost()
{
    int vdd1_opp = rm_get_vdd1_constraint();

    bBoostOn = 0;
    rm_set_vdd1_constraint(currentMHzConstraint);
}

/* below are the new implementations frequency based constraints */

/*
   Description : This function will set the minimum cpu scaling frequency
   
   Parameter   : total requested MHz for the current MM use case
   
   Return      : the correct min scaling freqeuncy in MHz that was set through sysfs
   
*/
int rm_set_min_scaling_freq(int MHz)
{
    int freq = 0;
    
#ifdef DVFS_ENABLED
    
    char command[100];
    /* for any MM case, set c-state to 2, unless no mm is active */
    int c_state = C_STATE_2;
    freq = rm_get_min_scaling_freq();
    if (freq < 0) {
        RAM_DPRINT("min scaling freq sysfs is not supported\n");
        return -1;
    }
    if(MHz == 0)
    {
        /* clear constraints for idle MM case */
        /* set c-state back if no active MM */
        if (get_omap_version() == OMAP3630_CPU)
        {
            c_state = C_STATE_9;
        }
        else
        {
           /*rest omap version support up to 6*/
            c_state = C_STATE_6;
        }
    }
    freq = dsp_mhz_to_min_scaling_freq(MHz);

    /* actually set the sysfs for cpufreq */
    RAM_DPRINT("[setting min scaling freq] requested MHz = %d new min scaling freq = %d\n",MHz,freq);
    strcpy(command,"echo ");
    strcat(command,ram_itoa(freq));
    strcat(command," > /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq");
    system(command);

    /* actually set the sysfs for cpuidle/max_state */
    RAM_DPRINT("[setting c-state] c-state %d\n",c_state);
    strcpy(command,"echo ");
    strcat(command,ram_itoa(c_state));
    strcat(command," > /sys/devices/system/cpu/cpu0/cpuidle/max_state");
    system(command);


#endif

    return freq;
}

/*
   Description : This function will get the min scaling freq
   
   Parameter   : n/a
   
   Return      : the currently set min scaling freq in MHz
   
*/
int rm_get_min_scaling_freq()
{
    int min_scaling_freq = 0;
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq","r");
    if (fp == NULL) {
        RAM_DPRINT("open file failed, unable to get min_scaling_freq\n");
        return -1;
    }
    fscanf(fp, "%d",&min_scaling_freq);
    fclose(fp);
    RAM_DPRINT("[rm_get_min_scaling_freq] = %d \n",min_scaling_freq);
    return min_scaling_freq;
}

/*
   Description : This function will determine the the new scaling frequency to set
                 based on the current total MHZ requested from MM
   
   Parameter   : MHz - current total MHZ requested from MM
   
   Return      : new min scaling frequency in MHz
   
*/
int dsp_mhz_to_min_scaling_freq(int MHz)
{
    unsigned int vdd1_opp = OPERATING_POINT_2;
    int freq = 0;
    int cpu_variant = get_omap_version();
    
    switch (cpu_variant) {
    case OMAP3420_CPU:
        if (MHz <= vdd1_dsp_mhz_3420[OPERATING_POINT_2]) {
            /* MM should never use opp1, so skip to opp2 */
            freq = vdd1_dsp_mhz_3420[OPERATING_POINT_2];
        }
        else if (MHz <= vdd1_dsp_mhz_3420[OPERATING_POINT_3]) {
            freq = vdd1_dsp_mhz_3420[OPERATING_POINT_3];
        }
        else if (MHz <= vdd1_dsp_mhz_3420[OPERATING_POINT_4]) {
            freq = vdd1_dsp_mhz_3420[OPERATING_POINT_4];
        }
        else {
            freq = vdd1_dsp_mhz_3420[OPERATING_POINT_5];
        }
        break;
        
        case OMAP3440_CPU:
        if (MHz <= vdd1_dsp_mhz_3440[OPERATING_POINT_2]) {
            /* MM should never use opp1, so skip to opp2 */
            freq = vdd1_dsp_mhz_3440[OPERATING_POINT_2];
        }
        else if (MHz <= vdd1_dsp_mhz_3440[OPERATING_POINT_3]) {
            freq = vdd1_dsp_mhz_3440[OPERATING_POINT_3];
        }
        else if (MHz <= vdd1_dsp_mhz_3440[OPERATING_POINT_4]) {
            freq = vdd1_dsp_mhz_3440[OPERATING_POINT_4];
        }
        else if (MHz <= vdd1_dsp_mhz_3440[OPERATING_POINT_5]) {
            freq = vdd1_dsp_mhz_3440[OPERATING_POINT_5];
        }
        else {
            freq = vdd1_dsp_mhz_3440[OPERATING_POINT_6];
        }
        break;
        
        case OMAP3430_CPU:
        if (MHz <= vdd1_dsp_mhz_3430[OPERATING_POINT_2]) {
            /* MM should never use opp1, so skip to opp2 */
            freq = vdd1_dsp_mhz_3430[OPERATING_POINT_2];
        }
        else if (MHz <= vdd1_dsp_mhz_3430[OPERATING_POINT_3]) {
            freq = vdd1_dsp_mhz_3430[OPERATING_POINT_3];
        }
        else if (MHz <= vdd1_dsp_mhz_3430[OPERATING_POINT_4]) {
            freq = vdd1_dsp_mhz_3430[OPERATING_POINT_4];
        }
        else {
            freq = vdd1_dsp_mhz_3430[OPERATING_POINT_5];
        }
        break;

        case OMAP3630_CPU:
        if (MHz <= vdd1_dsp_mhz_3630[OPERATING_POINT_1]) {
            /* 3630 should be able to run many MM cases at OPP1 */
            freq = vdd1_dsp_mhz_3630[OPERATING_POINT_1];
        }
        else if (MHz <= vdd1_dsp_mhz_3630[OPERATING_POINT_2]) {
            freq = vdd1_dsp_mhz_3630[OPERATING_POINT_2];
        }
        else if (MHz <= vdd1_dsp_mhz_3630[OPERATING_POINT_3]) {
            freq = vdd1_dsp_mhz_3630[OPERATING_POINT_3];
        }
        else {
            freq = vdd1_dsp_mhz_3630[OPERATING_POINT_4];
        }
        break;
    } /* end switch */
    return freq;
}


/*
   Description : This function will convert integer to char.
   
   Parameter   : integer to convert
   
   Return      : char for the integer parameter
   
*/
char * ram_itoa(int a)
{
    static char str[50];
    int i = sizeof(str) - 1;
    if (i != 0) {
        do {
            str[--i] = '0' + a % 10;
        }
        while ((a = a / 10) && i > 0);
    }
    return &str[i];
}


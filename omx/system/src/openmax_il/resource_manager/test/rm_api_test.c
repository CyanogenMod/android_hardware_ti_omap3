/*
 *  Copyright 2001-2008 Texas Instruments - http://www.ti.com/
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 * limitations under the License.
 */
/* =============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* =========================================================================== */
/* ----------------------------------------------------------------------------
*!
*! Test Case definition:
*! -1. get_omap_version() API check [always runs]
*!  0. basic get/set vdd1 between MAX opp and MIN opp argv[3] times
*!  1. 
*!  2. 
*!  3. 
*!  4. 
*!  5. 
*!  6. 
* =========================================================================== */
/* ------compilation control switches -------------------------*/
/****************************************************************
*  INCLUDE FILES
****************************************************************/
/* ----- system and platform files ----------------------------*/
#include <rm_test.h>


/* safe routine to get the maximum of 2 integers */
/* inline int maxint(int a, int b) */

int maxint(int a, int b) {
    return (a > b) ? a : b;
}


int main(int argc, char* argv[]) {
    int error = 0;
    int j =0, tcID =0;
    int numTestCases = 3;
    int omapVersion = OMAP_NOT_SUPPORTED;
    FILE* fOut = NULL;
        
    if (!strcmp(argv[1], "all")) {
        tcID = 0;
    } 
    else {
        printf("Invalid Test Case ID: exiting..\n");
        goto EXIT;
    }

    for (j = 0; j < numTestCases; j++) {
        //printf("main loop counter = %d\n", j);

        if (j == 0) {
            /* do these things first time only */
            /* 1. open the results file */
            fOut = fopen(APP_OUTPUT_FILE, "a");

            if ( fOut == NULL ) {
                printf("Error:  failed to open the file %s for readonly\access\n", APP_OUTPUT_FILE);
                goto EXIT;
            }

        }
        omapVersion = verify_get_omap_version();

        switch (tcID) {

           /* case -1:
                
                
                if (!strcmp(argv[1], "all")) {
                    tcID ++;
                } 
                break;
*/
            case 0:
            {
                verify_opp1_minmax(atoi(argv[2]), omapVersion);
                if (!strcmp(argv[1], "all")) {
                    tcID ++;
                } 
                break;
             }

            case 1:
                printf ("-------------------------------------\n");
                printf ("Testing new API test \n");
                printf ("-------------------------------------\n");
                if (!strcmp(argv[1], "all")) {
                    tcID ++;
                } 
                break;

            case 3:
                printf ("-------------------------------------\n");
                printf ("Testing new API test\n");
                printf ("-------------------------------------\n");
                if (!strcmp(argv[1], "all")) {
                    tcID ++;
                } 
                break;
        }

    }


EXIT:
   
    return error;
}


int verify_get_omap_version(){
    char* omapVersionStr = "";
    int omapVersion = OMAP_NOT_SUPPORTED;

    printf ("-------------------------------------\n");
    printf ("verify get_omap_version API\n");
    printf ("-------------------------------------\n");
    omapVersion = get_omap_version();
    if (omapVersion == OMAP3420_CPU) {
        omapVersionStr = "omap 3420";
    }
    else if (omapVersion == OMAP3430_CPU) {
        omapVersionStr = "omap 3430";
    }
    else if (omapVersion == OMAP3440_CPU) {
        omapVersionStr = "omap 3440";
    }
    else if (omapVersion == OMAP3630_CPU) {
        omapVersionStr = "omap 3630";
    }
    else {
        omapVersionStr = "not supported or ERROR";
    }
    printf("\tThe detected OMAP version is %s. Please verify to pass this test\n\n", omapVersionStr);

    return omapVersion;
}

/*
@input: takes number of times to perform transition between MIN and MAX vdd1_opp
@output: return number of successful transitions beween min and max vdd1_opp
*/
int verify_opp1_minmax(int loopCount, int omapv){
    int i = 0;
    int currentOpp = 0, maxDSPfreq = 0;

    printf ("-------------------------------------\n");
    printf ("verify get/set vdd1 constraint interfaces \n\n");
    printf ("-------------------------------------\n");
    printf ("\tBasic get/set between min opp and max opp\n");
    maxDSPfreq =  get_dsp_max_freq();
    do {
        currentOpp = rm_get_vdd1_constraint();
        printf ("\tget current OPP = %d\n", currentOpp);
        rm_set_vdd1_constraint(maxDSPfreq);
        if (rm_get_vdd1_constraint() == dsp_mhz_to_vdd1_opp(maxDSPfreq)+1){
            /* +1 is required to adjust the returned value from array index to actual opp value */
            printf ("\tsuccessfully set OPP for = %d for max dsp frequency\n\n", maxDSPfreq);
            sleep(1);
            /* reset to min opp [per omap] */
            if (omapv == OMAP3420_CPU) {
                rm_set_vdd1_constraint(vdd1_dsp_mhz_3420[1]);
            }
            else if (omapv == OMAP3430_CPU) {
                rm_set_vdd1_constraint(vdd1_dsp_mhz_3430[1]);
            }
            else if (omapv == OMAP3440_CPU) {
                rm_set_vdd1_constraint(vdd1_dsp_mhz_3440[1]);
            }
            else if (omapv == OMAP3630_CPU) {
                rm_set_vdd1_constraint(vdd1_dsp_mhz_3630[1]);
            }
            else {
                printf("FAIL: omap not supported ERROR\n");
                break;
            }
            printf("\twait for reset to min OPP to complete\n");
            int wait = 0;
            while (rm_get_vdd1_constraint() != 2 && i < loopCount){
                sleep(1);
                wait++;
                if (wait > 10) {
                    printf("FAIL: took too long to reset min OPP\n");
                    break;
                }
             }
         } else {
             printf ("\tfailed to correctly set OPP = %d for max dsp frequency\n\n", maxDSPfreq);
             break;
         }
    }while(i++ < loopCount);

    return i;
}


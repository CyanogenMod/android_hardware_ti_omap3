
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
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found 
*  in the license agreement under which this software has been supplied.
* ============================================================================ */
/**
* @file PolicyManager.c
*
* This file implements policy manager for Linux 8.x that 
* is fully compliant with the Khronos OpenMax specification 1.0.
*
* @path  $(CSLPATH)\
*
* @rev  1.0
*/
/* ---------------------------------------------------------------------------- 
*! 
*! Revision History 
*! ===================================
*! 24-Apr-2005 rg:  Initial Version. 
*!
* ============================================================================= */
#include <unistd.h>     // for sleep
#include <stdlib.h>     // for calloc
#include <sys/time.h>   // time is part of the select logic
#include <sys/types.h>  // for opening files
#include <sys/ioctl.h>  // for ioctl support
#include <sys/file.h>
#include <sys/stat.h>
#include <string.h>     // for memset
#include <stdio.h>      // for buffered io
#include <fcntl.h>      // for opening files.
#include <errno.h>      // for error handling support
#include <linux/soundcard.h>

#ifdef __PERF_INSTRUMENTATION__
#include "perf.h"
PERF_OBJHANDLE pPERF = NULL;
#endif
#include <ResourceManagerProxyAPI.h>
#include <PolicyManager.h>
int fdread, fdwrite;
int eErrno;
unsigned int totalCpu=0;
unsigned int imageTotalCpu=0;
unsigned int videoTotalCpu=0;
unsigned int audioTotalCpu=0;
unsigned int lcdTotalCpu=0;
unsigned int cameraTotalCpu=0;

int registeredComponents=0;
int pendingComponents=0;

OMX_POLICY_MANAGER_COMPONENTS_TYPE pComponentList[100];
OMX_POLICY_MANAGER_COMPONENTS_TYPE pPendingComponentList[100];
OMX_POLICY_COMPONENT_PRIORITY activeComponentList[100];
OMX_POLICY_COMPONENT_PRIORITY pendingComponentList[100];

OMX_POLICY_COMBINATION policyCombinationTable[OMX_POLICY_MAX_COMBINATIONS];

OMX_U8 activePolicyCombination;
OMX_U8 numCombinations;

POLICYMANAGER_RESPONSEDATATYPE response_data;
/*------------------------------------------------------------------------------------*
  * main() 
  *
  *                     This is the thread of policy manager
  *
  * @param 
  *                     None
  *
  * @retval 
  *                     None
  */
/*------------------------------------------------------------------------------------*/
int main()
{
    POLICYMANAGER_COMMANDDATATYPE cmd_data;

    PM_DPRINT("[Policy Manager] - start policy manager main function\n");
        
    int size = 0;
    int ret;

    fd_set watchset;
    OMX_BOOL Exitflag = OMX_FALSE;


    /* Fill policy table based on text file */
    ret = PopulatePolicyTable();
    if (ret != 0)
    {
        PM_DPRINT ("[Policy Manager] Populate Table failed. Check policy table and launch PM again\n"); 
        exit (-1);
    }
    
    PM_DPRINT("[Policy Manager] - going to create the read & write pipe\n");
    unlink(PM_SERVER_IN);
    unlink(PM_SERVER_OUT);
        
    if((mknod(PM_SERVER_IN,S_IFIFO|PERMS,0)<0)&&(errno!=EEXIST)) 
        PM_DPRINT("[Policy Manager] - mknod failure to create the read pipe, error=%d\n", errno);
        
    if((mknod(PM_SERVER_OUT,S_IFIFO|PERMS,0)<0)&&(errno!=EEXIST)) 
        PM_DPRINT("[Policy Manager] - mknod failure to create the write pipe, error=%d\n", errno);



    // create pipe for read
    if((fdread=open(PM_SERVER_IN,O_RDONLY))<0)
        PM_DPRINT("[Policy Manager] - failure to open the READ pipe\n");
        
    // create pipe for read
    if((fdwrite=open(PM_SERVER_OUT,O_WRONLY))<0)
        PM_DPRINT("[Policy Manager] - failure to open the WRITE pipe\n");
        

                
    FD_ZERO(&watchset);
    size=sizeof(cmd_data);
        
    PM_DPRINT("[Policy Manager] - going enter while loop\n");


    while(!Exitflag) {       
        FD_SET(fdread, &watchset);

        if((select(fdread+1, &watchset, NULL, NULL, NULL))<0)
            PM_DPRINT("[Policy Manager] - failure to create SELECT\n");

        if(FD_ISSET(fdread, &watchset)) {
            ret = read(fdread, &cmd_data, size);
            if((size>0)&&(ret>0)) {

                // actually get data 
                PM_DPRINT("[Policy Manager] - get data\n");
                switch (cmd_data.PM_Cmd) {  
                    case PM_RequestPolicy:
                        HandleRequestPolicy(cmd_data);
                        break;

                    case PM_WaitForPolicy:
                        HandleWaitForPolicy(cmd_data);
                        break;

                    case PM_FreePolicy:
                        HandleFreePolicy(cmd_data);
                        break;

                    case PM_CancelWaitForPolicy:
                        break;
                                                
                    case PM_FreeResources:
                        HandleFreeResources(cmd_data);
                        break;
                                                
                    case PM_StateSet:
                        break;
                                                
                    case PM_OpenPipe:
                        // create pipe for read

                        break;

                    case PM_Exit:
                    case PM_Init:
                        break;

                    case PM_ExitTI:
                        Exitflag = OMX_TRUE;
                        break;
                }  
            }
            else {
                close(fdread);
                if((fdread=open(PM_SERVER_IN,O_RDONLY))<0)                                
                    PM_DPRINT("[Policy Manager] - failure to re-open the Read pipe\n");
                PM_DPRINT("[Policy Manager] - re-opened Read pipe\n");
            }
        }
        else {
            PM_DPRINT("[Policy Manager] fdread not ready\n"); 
        }
    }
    close(fdread);
    close(fdwrite);

    if(unlink(PM_SERVER_IN)<0)
        PM_DPRINT("[Policy Manager] - unlink RM_SERVER_IN error\n");
    exit(0);
}



void HandleRequestPolicy(POLICYMANAGER_COMMANDDATATYPE cmd)
{
    int i;
    int combination;
    int priority;
    int returnValue;
    OMX_POLICY_COMBINATION_LIST combinationList;

    /* if this is the first request make sure that there's a combination that supports it */
    if (registeredComponents == 0) {
        combinationList = GetSupportingCombinations(cmd.param1);

        if (combinationList.numCombinations > 0) {
            /* if it is supported, arbitrarily choose the first combination table entry that supports it */
            activePolicyCombination = combinationList.combinationList[0];
            /* and grant policy */
            priority = GetPriority(cmd.param1, activePolicyCombination);
            
            if (priority != -1) {
            
                GrantPolicy(cmd.hComponent,cmd.param1,priority,cmd.nPid);
                return;
            }
        }
        else {
            /* otherwise deny policy and return */
            DenyPolicy(cmd.hComponent,cmd.nPid);
            return;
        }
    }

    /* Determine whether active policy combination will support new request */
    returnValue = CheckActiveCombination(cmd.param1,&priority);

    /* If active combination supports requested component grant policy and then return */
    if (returnValue == OMX_TRUE) { 
        GrantPolicy(cmd.hComponent,cmd.param1,priority,cmd.nPid);
        return;
    }

    /* If not, determine whether there is a policy combination which will allow the new request to coexist with currently running components */
    returnValue = CanAllComponentsCoexist(cmd.param1, &combination);
    if (returnValue == OMX_TRUE) {
        priority = GetPriority(cmd.param1, combination);
        if (priority != -1) {
            /* If so grant policy to the component requesting */
            GrantPolicy(cmd.hComponent,cmd.param1,priority,cmd.nPid);

            /* and make the new policy combination the active one */
            activePolicyCombination = combination;
            return;
        }
    }

    /* If not, determine whether there is a policy combination
       which will allow the new request to run at all */
    returnValue = CheckAllCombinations(cmd.param1,&combination, &priority);

    if (returnValue == OMX_TRUE) {
        if (combination < activePolicyCombination)
        {
            /* assumes the policy table file is sorted with highest
               prio use case at the top of the file, in 720p case the
               720p component should be at the top to prevent it from
               being preempted by lower prio dsp audio codec */
            /* preempt others and grant request */
            for (i=0; i < registeredComponents  ; i++) {
                PreemptComponent(pComponentList[i].componentHandle,pComponentList[i].nPid);
            }
            /* Check priority of existing combo against activeCombo */
            priority = GetPriority(cmd.param1, combination);
            if (priority != -1) {
                /* If so grant policy to the component requesting */
                GrantPolicy(cmd.hComponent,cmd.param1,priority,cmd.nPid);

                /* and make the new policy combination the active one */
                activePolicyCombination = combination;
                return;
            }
        }
    }
    /* otherwise deny policy */
    DenyPolicy(cmd.hComponent,cmd.nPid);
    
}
      
void HandleWaitForPolicy(POLICYMANAGER_COMMANDDATATYPE cmd)
{
    pPendingComponentList[pendingComponents].componentHandle = cmd.hComponent;
    pPendingComponentList[pendingComponents++].nPid = cmd.nPid;
}


void HandleCancelWaitForPolicy(POLICYMANAGER_COMMANDDATATYPE cmd)
{
    int i;
    int match = -1;

    for(i=0; i < pendingComponents; i++) {
        if (pPendingComponentList[i].componentHandle == cmd.hComponent && pPendingComponentList[i].nPid == cmd.nPid) {
            match = i;
            break;
        }
    }

    if (match != -1) {
        for (i=match; i < pendingComponents-1; i++) {
            pPendingComponentList[i].componentHandle = pPendingComponentList[i+1].componentHandle;
            pPendingComponentList[i].nPid = pPendingComponentList[i+1].nPid;
        }
        pendingComponents--;
    }

}

void HandleFreePolicy(POLICYMANAGER_COMMANDDATATYPE cmd)
{
    RemoveComponentFromList( cmd.hComponent,cmd.nPid, cmd.param1);

    /* If there are pending components one of them can now execute */
    if (pendingComponents) {
        
    }
    
}


void HandleFreeResources(POLICYMANAGER_COMMANDDATATYPE cmd)
{
    int i;
    int lowestPriority = GetPriority(cmd.param1, activePolicyCombination);
    int lowestPriorityIndex = -1;

/* If there are lower priority or equal priority components running within active policy combination preempt the lowest */
    for (i=0; i < registeredComponents; i++) {
        if (pComponentList[i].componentHandle != cmd.hComponent) {
            if (activeComponentList[i].priority <= lowestPriority) {
                lowestPriority = activeComponentList[i].priority;
                lowestPriorityIndex = i;
            }
        }
    }

    if (lowestPriorityIndex != -1) {
        PreemptComponent(pComponentList[lowestPriorityIndex].componentHandle,pComponentList[lowestPriorityIndex].nPid);

        /* Grant policy again to prompt the resource manager to again check resources */
        GrantPolicy(cmd.hComponent,cmd.param1,GetPriority(cmd.param1, activePolicyCombination),cmd.nPid);
        return;
    }
/* Otherwise inform the resource manager that it is impossible to free memory by denying policy */
    DenyPolicy(cmd.hComponent,cmd.nPid);

}


void PreemptComponent(OMX_HANDLETYPE hComponent, OMX_U32 aPid)
{
    response_data.hComponent = hComponent;
    response_data.nPid = aPid;
    response_data.PM_Cmd = PM_PREEMPTED;
    write(fdwrite,&response_data,sizeof(response_data));
}


void DenyPolicy(OMX_HANDLETYPE hComponent,OMX_U32 aPid)
{
    response_data.hComponent = hComponent;
    response_data.nPid= aPid;
    response_data.PM_Cmd = PM_DENYPOLICY;
    write(fdwrite,&response_data,sizeof(response_data));
}


void GrantPolicy(OMX_HANDLETYPE hComponent, OMX_U8 aComponentIndex, OMX_U8 aPriority, OMX_U32 aPid)
{
    int i, match =-1;
    response_data.hComponent = hComponent;
    response_data.nPid = aPid;
    response_data.PM_Cmd = PM_GRANTPOLICY;
    write(fdwrite, &response_data, sizeof(response_data));
    match = -1;

    for (i=0; i < registeredComponents; i++) {
        if (pComponentList[i].componentHandle == hComponent && pComponentList[i].nPid == aPid)
        {
            match = i;
            break;
        }
    }
    if (match == -1) {
        /* do not add the same component twice */

        activeComponentList[registeredComponents].component = aComponentIndex;
        activeComponentList[registeredComponents].priority = aPriority;
        pComponentList[registeredComponents].componentHandle = hComponent;
        pComponentList[registeredComponents++].nPid = aPid;
    }
}

void RemoveComponentFromList(OMX_HANDLETYPE hComponent, OMX_U32 aPid, OMX_U32 cComponentIndex) 
{
    int i;
    int match = -1;

    for (i=0; i < registeredComponents; i++) {
        if (activeComponentList[i].component == cComponentIndex) {
            match = i;
            break;
        }
    }
    if (match != -1)
    {
        for (i=match; i< registeredComponents-1; i++) {

            activeComponentList[i].component = activeComponentList[i+1].component;
            activeComponentList[i].priority = activeComponentList[i+1].priority;
        }

        match = -1;
    }
    for(i=0; i < registeredComponents; i++) {
        if (pComponentList[i].componentHandle == hComponent && pComponentList[i].nPid == aPid) {
            match = i;
            break;
        }
    }

    if (match != -1) {
        for (i=match; i < registeredComponents-1; i++) {
            pComponentList[i].componentHandle = pComponentList[i+1].componentHandle;
            pComponentList[i].nPid = pComponentList[i+1].nPid;
        }
        registeredComponents--;
    }
}

int PopulatePolicyTable()
{
    int i,j;
    char line[PM_MAXSTRINGLENGTH];
    int combinationIndex=0;
    int policyIndex=1;
    int index=0;
    char *tablefile= getenv ("PM_TBLFILE");
    FILE *policytablefile;
    char *result = NULL;   
    int ret=0;

    if (tablefile == NULL)
    {
        PM_DPRINT ("No policy table file set\n");
        ret = -1;
        goto EXIT;
    }
    else
    {

        fprintf (stderr, "[Policy Manager] Read Policy Table at: %s\n", tablefile);
        policytablefile  = fopen(tablefile,"r");

        if (policytablefile == NULL) {
            fprintf(stderr, "[Policy Manager] Could not open file. Run again\n");
            ret = -1;
            goto EXIT;
        }
        else {
            /* Initialize policy combination table */
            for (i=0; i < OMX_POLICY_MAX_COMBINATIONS; i++) {
                for (j=0; j < OMX_POLICY_MAX_COMBINATION_LENGTH; j++) {
                    policyCombinationTable[i].component[j].component = 0;
                    policyCombinationTable[i].component[j].priority = 0;
                }
                policyCombinationTable[i].bCombinationIsActive = 0;
                policyCombinationTable[i].numComponentsInCombination = 0;
            }
            while (fgets(line,PM_MAXSTRINGLENGTH,policytablefile) != NULL) {
                if ( combinationIndex >= OMX_POLICY_MAX_COMBINATIONS )
                {
                  /*policy table contains more info than can actually be stored
                   * either fix the policy table or increment the number of
                   * combinations possible */
                  PM_DPRINT ("[Policy Manager] Policy table is bigger than expected.  Run again\n");
                  ret = -1;
                  goto EXIT;
                }
                result = NULL;
                result = strtok( line, "," );
                if (NULL == result) {
                    goto EXIT;
                }
                policyCombinationTable[combinationIndex].component[0].component = PolicyStringToIndex(result);
                result = strtok( NULL, " ,\n" );
                if (NULL == result) {
                    goto EXIT;
                }
                policyCombinationTable[combinationIndex].component[0].priority = atoi(result);
                policyIndex = 1;
                while (result != NULL) {
                    if (policyIndex >= OMX_POLICY_MAX_COMBINATION_LENGTH) {
                        /*policy table contains records with more info than can actually be stored
                         * either fix the policy table or increment the number of
                         * combinations possible */
                        PM_DPRINT ("[Policy Manager] Policy Table Record is wider than expected. Run again\n");
						ret = -1;
                        goto EXIT;
                    }
                    result = strtok( NULL, " ,\n" );
                    if (result != NULL) {
                        if (!(index % 2)) {
                            policyCombinationTable[combinationIndex].component[policyIndex].component = PolicyStringToIndex(result);
                        }
                        else {
                            policyCombinationTable[combinationIndex].component[policyIndex++].priority = atoi(result);
                        }
                        index++;
                    }
                }
                policyCombinationTable[combinationIndex++].numComponentsInCombination = policyIndex;
            } 
            if (!combinationIndex)
            {
             /*policy table is empty it's safer to quit*/
             fprintf (stderr, "Policy table is empty. Run Policy Manager again\n");
             ret = -1;
             goto EXIT;
            }

        }
    }

    numCombinations = combinationIndex;    
EXIT:    
    return(ret);
}


OMX_COMPONENTINDEXTYPE PolicyStringToIndex(char* aString) 
{
    int i;
    for (i=0; i < PM_NUM_COMPONENTS; i++) {
        if (!strcmp(aString,PM_ComponentTable[i])) {
            return i;
        }
    }
    return -1;
}

OMX_BOOL CheckActiveCombination(OMX_COMPONENTINDEXTYPE componentRequestingPolicy, int *priority)
{
    int i;
    OMX_BOOL returnValue = OMX_FALSE;

    for (i=0; i < policyCombinationTable[activePolicyCombination].numComponentsInCombination; i++) {
        if (policyCombinationTable[activePolicyCombination].component[i].component == componentRequestingPolicy) {
            returnValue = OMX_TRUE;
            *priority = policyCombinationTable[activePolicyCombination].component[i].priority;
            break;
        }
    }
    return returnValue;
}

OMX_BOOL CheckAllCombinations(OMX_COMPONENTINDEXTYPE componentRequestingPolicy, int *combination, int *priority)
{
    int i,j;
    OMX_BOOL returnValue = OMX_FALSE;

    for (i=0; i < numCombinations; i++) {
        for (j=0; j < policyCombinationTable[i].numComponentsInCombination; j++) {
            if (policyCombinationTable[i].component[j].component == componentRequestingPolicy) {
                returnValue = OMX_TRUE;
                *priority = policyCombinationTable[i].component[j].priority;
                *combination = i;
                break;
            }
        }
    }
    return returnValue;
}

OMX_BOOL IsComponentSupported(OMX_COMPONENTINDEXTYPE component, int combination)
{
    int i;
    OMX_BOOL bFound = OMX_FALSE;

    for (i=0; i < policyCombinationTable[combination].numComponentsInCombination; i++) {
        if (policyCombinationTable[combination].component[i].component == component) {
            bFound = OMX_TRUE;
            break;
        }
    }
    return bFound;
}


OMX_POLICY_COMBINATION_LIST GetSupportingCombinations(OMX_COMPONENTINDEXTYPE componentRequestingPolicy)
{
    int i,j;
    OMX_BOOL bThisCombinationSupports = OMX_FALSE;
    OMX_POLICY_COMBINATION_LIST returnValue = {0};

    returnValue.numCombinations = 0;
    for (i=0; i < numCombinations; i++) {
        bThisCombinationSupports = OMX_FALSE;
        for (j=0; j < policyCombinationTable[i].numComponentsInCombination; j++) {
            if (policyCombinationTable[i].component[j].component == componentRequestingPolicy) {
                returnValue.combinationList[returnValue.numCombinations++] = i;
            }
        }
    }
    return returnValue;
}

OMX_BOOL CanAllComponentsCoexist(OMX_COMPONENTINDEXTYPE componentRequestingPolicy,int *combination)
{
    int i,j;
    OMX_POLICY_COMBINATION_LIST combinationList;
    OMX_BOOL returnValue = OMX_FALSE;

    /* First get a list of all the combinations that support the one requesting policy */
    combinationList = GetSupportingCombinations(componentRequestingPolicy);

    /* Then loop through the list and see if the other active components are also supported by any of the policies */
    for (i=0; i < combinationList.numCombinations; i++) {
        for (j=0; j < registeredComponents; j++) {
            returnValue = IsComponentSupported(activeComponentList[j].component,combinationList.combinationList[i]);
            if (returnValue == OMX_FALSE){
                break;
            }
        }

        /* if the value is true after looping through all registered components this combination supports all existing components */
        if (returnValue == OMX_TRUE) {
            *combination = combinationList.combinationList[i];
            break;
        }
    }
    return returnValue;
}

int GetPriority(OMX_COMPONENTINDEXTYPE component, int combination)
{
    int i;
    for (i=0; i < policyCombinationTable[combination].numComponentsInCombination; i++) {
        if (policyCombinationTable[combination].component[i].component == component) {
            return policyCombinationTable[combination].component[i].priority;
        }
    }
    return -1;
}

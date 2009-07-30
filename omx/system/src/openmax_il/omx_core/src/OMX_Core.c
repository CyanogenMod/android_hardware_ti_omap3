/* ====================================================================
*             Texas Instruments OMAP(TM) Platform Software
* (c) Copyright Texas Instruments, Incorporated. All Rights Reserved.
*
* Use of this software is controlled by the terms and conditions found
* in the license agreement under which this software has been supplied.
* ==================================================================== */

#include <dlfcn.h>   /* For dynamic loading */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <utils/Log.h>


/* #include "OMX_RegLib.h" */
#include "OMX_Component.h"
#include "OMX_Core.h"
#include "OMX_ComponentRegistry.h"
//#include "pvlogger.h"

#include "ti_omx_config_parser.h"

/** size for the array of allocated components.  Sets the maximum 
 * number of components that can be allocated at once */
#define MAXCOMP (50)
#define MAXNAMESIZE (130)
#define EMPTY_STRING "\0"

/** Determine the number of elements in an array */
#define COUNTOF(x) (sizeof(x)/sizeof(x[0]))

/** Array to hold the DLL pointers for each allocated component */
static void* pModules[MAXCOMP] = {0};

/** Array to hold the component handles for each allocated component */
static void* pComponents[COUNTOF(pModules)] = {0};

/* count for call OMX_Init() */
int count = 0;
pthread_mutex_t mutex;

int tableCount = 0;
ComponentTable componentTable[MAX_TABLE_SIZE];
char * sRoleArray[60][20];
char compName[60][200];


char *tComponentName[MAXCOMP][2] = {
    /*video and image components */
    {"OMX.TI.JPEG.decode", "image_decoder.jpeg" },
    {"OMX.TI.JPEG.encoder", "image_encoder.jpeg"}, 
    {"OMX.TI.Video.Decoder", "video_decoder.h263"},
    {"OMX.TI.Video.Decoder", "video_decoder.avc"},
    {"OMX.TI.Video.Decoder", "video_decoder.mpeg2"},
    {"OMX.TI.Video.Decoder", "video_decoder.mpeg4"},
    {"OMX.TI.Video.Decoder", "video_decoder.wmv"},
    {"OMX.TI.Video.encoder", "video_encoder.mpeg4"},
    {"OMX.TI.Video.encoder", "video_encoder.h263"},
    {"OMX.TI.Video.encoder", "video_encoder.avc"},
    {"OMX.TI.VPP", "iv_renderer.yuv.overlay"},
    {"OMX.TI.Camera", "camera.yuv"}, 

    /* Speech components */
  /*  {"OMX.TI.G729.encode", NULL},
    {"OMX.TI.G729.decode", NULL},	
    {"OMX.TI.G722.encode", NULL},
    {"OMX.TI.G722.decode", NULL},
    {"OMX.TI.G711.encode", NULL},
    {"OMX.TI.G711.decode", NULL},
    {"OMX.TI.G723.encode", NULL},
    {"OMX.TI.G723.decode", NULL},
    {"OMX.TI.G726.encode", NULL},
    {"OMX.TI.G726.decode", NULL},
    {"OMX.TI.GSMFR.encode", NULL},
    {"OMX.TI.GSMFR.decode", NULL}, */
    {"OMX.TI.AMR.encode", "audio_encoder.amrnb"},
    {"OMX.TI.AMR.decode", "audio_decoder.amrnb"},	
    {"OMX.TI.WBAMR.encode", "audio_encoder.amrwb"},
    {"OMX.TI.WBAMR.decode", "audio_decoder.amrwb"},	

    /* Audio components */
    {"OMX.TI.MP3.decode", "audio_decoder.mp3"},	
    {"OMX.TI.AAC.encode", "audio_encoder.aac"}, 
    {"OMX.TI.AAC.decode", "audio_decoder.aac"},	
/*    {"OMX.TI.PCM.encode", NULL},
    {"OMX.TI.PCM.decode", NULL}, */
    {"OMX.TI.WMA.decode", "audio_decoder.wma"},	
/*    {"OMX.TI.RAG.decode", "audio_decoder.ra"},	
    {"OMX.TI.IMAADPCM.decode", NULL},	
    {"OMX.TI.IMAADPCM.encode", NULL},	*/

    /* terminate the table */
    {NULL, NULL},
};


/******************************Public*Routine******************************\
* OMX_Init()
*
* Description:This method will initialize the OMX Core.  It is the 
* responsibility of the application to call OMX_Init to ensure the proper
* set up of core resources.
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
\**************************************************************************/
OMX_ERRORTYPE TIOMX_Init()
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    //__android_log_print(ANDROID_LOG_VERBOSE, __FILE__,"%s %d::TI:OMX_Init  ",__FUNCTION__, __LINE__);

    count++;

    if (count == 1)
    {
        pthread_mutex_init(&mutex, NULL);  
	//__android_log_print(ANDROID_LOG_VERBOSE, __FILE__, "calling BuildComponentTable\n");  
        eError = TIOMX_BuildComponentTable();
	//__android_log_print(ANDROID_LOG_VERBOSE, __FILE__, "buildCOmpoentTable done\n");
    }
    //__android_log_print(ANDROID_LOG_VERBOSE, __FILE__,"%s %d::  ",__FUNCTION__, __LINE__);
    return eError;
}
/******************************Public*Routine******************************\
* OMX_GetHandle
*
* Description: This method will create the handle of the COMPONENTTYPE
* If the component is currently loaded, this method will reutrn the 
* hadle of existingcomponent or create a new instance of the component.
* It will call the OMX_ComponentInit function and then the setcallback
* method to initialize the callback functions
* Parameters:
* @param[out] pHandle            Handle of the loaded components 
* @param[in] cComponentName     Name of the component to load
* @param[in] pAppData           Used to identify the callbacks of component 
* @param[in] pCallBacks         Application callbacks
*
* @retval OMX_ErrorUndefined         
* @retval OMX_ErrorInvalidComponentName
* @retval OMX_ErrorInvalidComponent
* @retval OMX_ErrorInsufficientResources 
* @retval OMX_NOERROR                      Successful
*
* Note
*
\**************************************************************************/

OMX_ERRORTYPE TIOMX_GetHandle( OMX_HANDLETYPE* pHandle, OMX_STRING cComponentName,
    OMX_PTR pAppData, OMX_CALLBACKTYPE* pCallBacks) 
{
    static const char prefix[] = "lib";
    static const char postfix[] = ".so";
    OMX_ERRORTYPE (*pComponentInit)(OMX_HANDLETYPE*);
    OMX_ERRORTYPE err = OMX_ErrorNone;  /* This was ErrorUndefined */
    OMX_COMPONENTTYPE *componentType;
    int i;
    char buf[sizeof(prefix) + MAXNAMESIZE +sizeof(postfix)];
    const char* pErr = dlerror();
    
  //  PVLOGGER2ANDROID((0,"OMX_Core::GetHandle IN"));
    if(pthread_mutex_lock(&mutex) != 0) 
    {
        printf("%d :: Core: Error in Mutex lock\n",__LINE__);
    }

    if ((NULL == cComponentName) || (NULL == pHandle) || (NULL == pCallBacks)) {
        err = OMX_ErrorBadParameter;
        goto EXIT;
    }

    /* Verify that the name is not too long and could cause a crash.  Notice
     * that the comparison is a greater than or equals.  This is to make
     * sure that there is room for the terminating NULL at the end of the
     * name. */
    if( strlen(cComponentName) >= MAXNAMESIZE) {

        err = OMX_ErrorInvalidComponentName;
        goto EXIT;
    }
    /* Locate the first empty slot for a component.  If no slots
     * are available, error out */
    for(i=0; i< COUNTOF(pModules); i++) {
        if(pModules[i] == NULL) break;
    }
    if(i == COUNTOF(pModules)) {
         err = OMX_ErrorInsufficientResources;
        goto EXIT;
    }

    /* load the component and check for an error.  If filename is not an 
     * absolute path (i.e., it does not  begin with a "/"), then the 
     * file is searched for in the following locations:
     *
     *     The LD_LIBRARY_PATH environment variable locations
     *     The library cache, /etc/ld.so.cache.
     *     /lib
     *     /usr/lib
     * 
     * If there is an error, we can't go on, so set the error code and exit */
    strcpy(buf, prefix);                      /* the lengths are defined herein or have been */
    strcat(buf, cComponentName);  /* checked already, so strcpy and strcat are  */
    strcat(buf, postfix);                      /* are safe to use in this context. */
    pModules[i] = dlopen(buf, RTLD_LAZY | RTLD_GLOBAL);
    if( pModules[i] == NULL ) {
        fprintf (stderr, "Failed because %s\n", dlerror());
        err = OMX_ErrorComponentNotFound;
        goto EXIT;
    }

    /* Get a function pointer to the "OMX_ComponentInit" function.  If 
     * there is an error, we can't go on, so set the error code and exit */
    pComponentInit = dlsym(pModules[i], "OMX_ComponentInit");
    if( (pErr != NULL) || (pComponentInit == NULL) ) {
        err = OMX_ErrorInvalidComponent;
        goto EXIT;
    }


    /* We now can access the dll.  So, we need to call the "OMX_ComponentInit"
     * method to load up the "handle" (which is just a list of functions to 
     * call) and we should be all set.*/
    *pHandle = malloc(sizeof(OMX_COMPONENTTYPE));
    if(*pHandle == NULL) {
        err = OMX_ErrorInsufficientResources;
        printf("%d:: malloc of pHandle* failed\n", __LINE__);
        goto EXIT;
    }
    pComponents[i] = *pHandle;
    componentType = (OMX_COMPONENTTYPE*) *pHandle;
    componentType->nSize = sizeof(OMX_COMPONENTTYPE);   
    err = (*pComponentInit)(*pHandle);
    if (OMX_ErrorNone == err) {
        err = (componentType->SetCallbacks)(*pHandle, pCallBacks, pAppData);
        if (err != OMX_ErrorNone) {
            printf("%d :: Core: Error Returned From Component\
                    SetCallBack\n",__LINE__);
            goto EXIT;
        }    
    } 
    else {
       /* when the component fails to initialize, release the
       component handle structure */
       free(*pHandle);
       /* mark the component handle as NULL to prevent the caller from
       actually trying to access the component with it, should they
       ignore the return code */
       *pHandle = NULL;
       pComponents[i] = NULL;
        dlclose(pModules[i]);
     /*  err = OMX_ErrorComponentNotFound; */
       goto EXIT;
    }   
    err = OMX_ErrorNone;
EXIT:
    if(pthread_mutex_unlock(&mutex) != 0) 
    {
        printf("%d :: Core: Error in Mutex unlock\n",__LINE__);
    } 
  //  PVLOGGER2ANDROID((0,"OMX_Core::GetHandle OUT"));
    return (err);
}
 

/******************************Public*Routine******************************\
* OMX_FreeHandle()
*
* Description:This method will unload the OMX component pointed by 
* OMX_HANDLETYPE. It is the responsibility of the calling method to ensure that
* the Deinit method of the component has been called prior to unloading component
*
* Parameters:
* @param[in] hComponent the component to unload
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
\**************************************************************************/
OMX_ERRORTYPE TIOMX_FreeHandle (OMX_HANDLETYPE hComponent)
{

    OMX_ERRORTYPE retVal = OMX_ErrorUndefined;
    OMX_COMPONENTTYPE *pHandle = (OMX_COMPONENTTYPE *)hComponent;
    int i;

    if(pthread_mutex_lock(&mutex) != 0) 
    {
        printf("%d :: Core: Error in Mutex lock\n",__LINE__);
    }

    /* Locate the component handle in the array of handles */
    for(i=0; i< COUNTOF(pModules); i++) {
        if(pComponents[i] == hComponent) break;
    }
    
    
    if(i == COUNTOF(pModules)) {
        retVal = OMX_ErrorBadParameter;
        goto EXIT;
    }

    retVal = pHandle->ComponentDeInit(hComponent);
    if (retVal != OMX_ErrorNone) {
        printf("%d :: Error From ComponentDeInit..\n",__LINE__);
        goto EXIT; 
    }

    /* release the component and the component handle */
    dlclose(pModules[i]);
    pModules[i] = NULL;
    free(pComponents[i]);

    pComponents[i] = NULL;
    retVal = OMX_ErrorNone;
   
EXIT:
    /* The unload is now complete, so set the error code to pass and exit */
    if(pthread_mutex_unlock(&mutex) != 0) 
    {
        printf("%d :: Core: Error in Mutex unlock\n",__LINE__);
    } 

    return retVal;
}

/******************************Public*Routine******************************\
* OMX_DeInit()
*
* Description:This method will release the resources of the OMX Core.  It is the 
* responsibility of the application to call OMX_DeInit to ensure the clean up of these
* resources.
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
\**************************************************************************/
OMX_ERRORTYPE TIOMX_Deinit()
{
    count--;

    if(pthread_mutex_lock(&mutex) != 0)
        printf("%d :: Core: Error in Mutex lock\n",__LINE__); 
        
    if(count == 0)
    {
        if(pthread_mutex_unlock(&mutex) != 0)
            printf("%d :: Core: Error in Mutex unlock\n",__LINE__); 
        if(pthread_mutex_destroy(&mutex) != 0) {
            /*printf("%d :: Core: Error in Mutex destroy\n",__LINE__);*/
		}
    }
    else{
        if(pthread_mutex_unlock(&mutex) != 0)
            printf("%d :: Core: Error in Mutex unlock\n",__LINE__);
    }            

    return OMX_ErrorNone;
}

/*************************************************************************
* OMX_SetupTunnel()
*
* Description: Setup the specified tunnel the two components
*
* Parameters:
* @param[in] hOutput     Handle of the component to be accessed
* @param[in] nPortOutput Source port used in the tunnel
* @param[in] hInput      Component to setup the tunnel with.
* @param[in] nPortInput  Destination port used in the tunnel
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
**************************************************************************/
/* OMX_SetupTunnel */
OMX_API OMX_ERRORTYPE OMX_APIENTRY TIOMX_SetupTunnel(
    OMX_IN  OMX_HANDLETYPE hOutput,
    OMX_IN  OMX_U32 nPortOutput,
    OMX_IN  OMX_HANDLETYPE hInput,
    OMX_IN  OMX_U32 nPortInput)
{
    OMX_ERRORTYPE eError = OMX_ErrorNotImplemented;
    OMX_COMPONENTTYPE *pCompIn, *pCompOut;
    OMX_TUNNELSETUPTYPE oTunnelSetup;

    if (hOutput == NULL && hInput == NULL)
        return OMX_ErrorBadParameter;

    oTunnelSetup.nTunnelFlags = 0;
    oTunnelSetup.eSupplier = OMX_BufferSupplyUnspecified;

    pCompOut = (OMX_COMPONENTTYPE*)hOutput;

    if (hOutput)
    {
        eError = pCompOut->ComponentTunnelRequest(hOutput, nPortOutput, hInput, nPortInput, &oTunnelSetup);
    }


    if (eError == OMX_ErrorNone && hInput) 
    {  
        pCompIn = (OMX_COMPONENTTYPE*)hInput;
        eError = pCompIn->ComponentTunnelRequest(hInput, nPortInput, hOutput, nPortOutput, &oTunnelSetup);
        if (eError != OMX_ErrorNone && hOutput) 
        {
            /* cancel tunnel request on output port since input port failed */
            pCompOut->ComponentTunnelRequest(hOutput, nPortOutput, NULL, 0, NULL);
        }
    }
  
    return eError;
}

/*************************************************************************
* OMX_ComponentNameEnum()
*
* Description: This method will provide the name of the component at the given nIndex
*
*Parameters:
* @param[out] cComponentName       The name of the component at nIndex
* @param[in] nNameLength                The length of the component name
* @param[in] nIndex                         The index number of the component 
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
**************************************************************************/
OMX_API OMX_ERRORTYPE OMX_APIENTRY TIOMX_ComponentNameEnum(
    OMX_OUT OMX_STRING cComponentName,
    OMX_IN  OMX_U32 nNameLength,
    OMX_IN  OMX_U32 nIndex)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (nIndex >=  tableCount)
    {
        eError = OMX_ErrorNoMore;
     }
    else
    {
        strcpy(cComponentName, componentTable[nIndex].name);
    }
    
    return eError;
}


/*************************************************************************
* OMX_GetRolesOfComponent()
*
* Description: This method will query the component for its supported roles
*
*Parameters:
* @param[in] cComponentName     The name of the component to query
* @param[in] pNumRoles     The number of roles supported by the component
* @param[in] roles		The roles of the component
*
* Returns:    OMX_NOERROR          Successful
*                 OMX_ErrorBadParameter		Faliure due to a bad input parameter
*
* Note
*
**************************************************************************/
OMX_API OMX_ERRORTYPE TIOMX_GetRolesOfComponent (
    OMX_IN      OMX_STRING cComponentName,
    OMX_INOUT   OMX_U32 *pNumRoles,
    OMX_OUT     OMX_U8 **roles)
{

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 i = 0;
    OMX_U32 j = 0;
    OMX_BOOL bFound = OMX_FALSE;

    if (cComponentName == NULL)
    {
        eError = OMX_ErrorBadParameter;
        goto EXIT;       
    }


    while(!bFound && i < tableCount)
    {
        if (strcmp(cComponentName, componentTable[i].name) == 0)
        {
            bFound = OMX_TRUE;
        }
        else
        {
            i++;
        }
    }   
    if (roles == NULL)
    { 
        *pNumRoles = componentTable[i].nRoles;
        goto EXIT;
    }
    else
    {
        if (bFound && (*pNumRoles == componentTable[i].nRoles))
       {
           for (j = 0; j<componentTable[i].nRoles; j++) 
           {
               strcpy((OMX_STRING)roles[j], componentTable[i].pRoleArray[j]);
           }
       }
   }
   EXIT:
   return eError;
}

/*************************************************************************
* OMX_GetComponentsOfRole()
*
* Description: This method will query the component for its supported roles
*
*Parameters:
* @param[in] role     The role name to query for
* @param[in] pNumComps     The number of components supporting the given role
* @param[in] compNames      The names of the components supporting the given role
*
* Returns:    OMX_NOERROR          Successful
*
* Note
*
**************************************************************************/
OMX_API OMX_ERRORTYPE TIOMX_GetComponentsOfRole ( 
    OMX_IN      OMX_STRING role,
    OMX_INOUT   OMX_U32 *pNumComps,
    OMX_INOUT   OMX_U8  **compNames)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 i = 0;
    OMX_U32 j = 0;
    OMX_U32 k = 0;

    if (role == NULL)
    {
       eError = OMX_ErrorBadParameter;
       goto EXIT;
    }

   /* This implies that the componentTable is not filled */
    if (componentTable[i].pRoleArray[j] == NULL)
    {
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }


    for (i = 0; i < tableCount; i++)
    {
        for (j = 0; j<componentTable[i].nRoles; j++) 
        { 
            if (strcmp(componentTable[i].pRoleArray[j], role) == 0)
            {
                /* the first call to this function should only count the number
                    of roles so that for the second call compNames can be allocated
                    with the proper size for that number of roles */
                if (compNames != NULL)
                {
                    compNames[k] = (OMX_U8*)componentTable[i].name;
                }
                k++;
            }
        }
        *pNumComps = k;
    }

    EXIT:
    return eError;
}


OMX_ERRORTYPE TIOMX_BuildComponentTable()
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_CALLBACKTYPE sCallbacks;
#ifndef STATIC_TABLE    
    OMX_HANDLETYPE hComp  = 0;
    OMX_U8 cRole[MAXNAMESIZE];
    OMX_STRING tempName = NULL;
    OMX_STRING temp = NULL;
    static OMX_STRING namePrefix = "OMX";
    static OMX_STRING filePrefix = "libOMX.";
    static OMX_STRING suffix = ".so";
#endif    
    int j = 0;
    int numFiles = 0;
	int i;
  //  PVLOGGER2ANDROID((0,"OMX_Core::BuildComponentTable: IN"));
    /* set up dummy call backs */
    sCallbacks.EventHandler    = ComponentTable_EventHandler;
    sCallbacks.EmptyBufferDone = ComponentTable_EmptyBufferDone;
    sCallbacks.FillBufferDone  = ComponentTable_FillBufferDone;

#ifndef STATIC_TABLE
    /* allocate the name table */
	/*
    compName = (OMX_STRING *) malloc(MAX_TABLE_SIZE * sizeof(OMX_STRING));
    sRoleArray = (OMX_STRING**) malloc(MAX_TABLE_SIZE * sizeof(OMX_STRING));
	*/
    
    /* scan the target/lib directory and create a list of files in the directory */
    numFiles= scandir(libdir, &namelist, 0, 0);
    tableCount = 0;
    while (numFiles--){
        /*  check if the file is an OMX component */
        if (strncmp(namelist[numFiles]->d_name, filePrefix, strlen(filePrefix)) == 0){

            /* if the file is an OMX component, trim the prefix and suffix */
            tempName = (OMX_STRING) malloc(sizeof(namelist[numFiles]->d_name) + 1);   /* adding one ensures */
            memset(tempName, 0x00, sizeof(namelist[numFiles]->d_name) + 1);                  /*  that a null terminator will */
                                                                                                                                  /*  always be present */
            /* copy only the name without the suffix */                                                                                                           
            strncpy(tempName, namelist[numFiles]->d_name,strlen(namelist[numFiles]->d_name) - strlen(suffix));
            /* set a pointer to be after the lib prefix, i.e the beginning of the component name */
            temp = strstr(tempName, namePrefix);

            /* then copy the component name to the table */
			/*
            compName[tableCount]= (OMX_STRING) malloc(MAXNAMESIZE);
			*/
            strncpy(compName[tableCount], temp, strlen(temp) + 1);
            componentTable[tableCount].name = compName[tableCount];

            /* get the handle for the component and query for the roles of each component */
            eError = OMX_GetHandle(&hComp, componentTable[tableCount].name, 0x0, &sCallbacks);
            if (eError == OMX_ErrorNone){  
                j = 0;
                while (eError != OMX_ErrorNoMore){
                    eError = ((OMX_COMPONENTTYPE *) hComp)->ComponentRoleEnum(hComp, cRole, j++);
                    if (eError == OMX_ErrorNotImplemented){
                        j = 1;
                        break;
                    }
                }
                nRoles = j-1;
                componentTable[tableCount].nRoles = nRoles;
                /* sRoleArray[tableCount] = (OMX_STRING *) malloc(nRoles * sizeof(OMX_STRING)); */
                if (nRoles > 0){                
                    /* sRoleArray[tableCount] = (OMX_STRING *) malloc(nRoles * sizeof(OMX_STRING)); */
                    for (j = 0; j<nRoles; j++) {
                        sRoleArray[tableCount][j] = (OMX_STRING) malloc( sizeof(OMX_U8) * MAXNAMESIZE);
                        ((OMX_COMPONENTTYPE *) hComp)->ComponentRoleEnum(hComp, (OMX_U8*)sRoleArray[tableCount][j], j);
                        componentTable[tableCount].pRoleArray[j] =sRoleArray[tableCount][j];
                    }   
                }
                else
                {
                    /* sRoleArray[tableCount] = (OMX_STRING *) malloc(sizeof(OMX_STRING));    */                            
                    sRoleArray[tableCount][j] = (OMX_STRING) malloc(sizeof(OMX_U8) * MAXNAMESIZE);
                    strcpy(sRoleArray[tableCount][j], EMPTY_STRING);
                    componentTable[tableCount].pRoleArray[j] = sRoleArray[tableCount][j];
                }
            }
            if (hComp)
            {
                /* free the component handle */
                eError = OMX_FreeHandle(hComp);
                if (eError != OMX_ErrorNone){
                    goto EXIT;
                }
            }
            /* increment the table counter index only if above was successful */
            tableCount++;
            if (tempName != NULL)
            {
                free(tempName);
            }

        }
    }
 
#endif

	for (i = 0, numFiles = 0; i < MAXCOMP; i ++) {
		if (tComponentName[i][0] == NULL) {
			break;
		}

		for (j = 0; j < numFiles; j ++) {
			if (!strcmp(componentTable[j].name, tComponentName[i][0])) {
				/* insert the role */
				if (tComponentName[i][1] != NULL)
				{
    		   		    componentTable[j].pRoleArray[componentTable[j].nRoles] = tComponentName[i][1];
	    			    componentTable[j].nRoles ++;
	    			}
				break;
			}
		}

		if (j == numFiles) { /* new component */
		    if (tComponentName[i][1] != NULL){
		        componentTable[numFiles].pRoleArray[0] = tComponentName[i][1];
		        componentTable[numFiles].nRoles = 1;
		    }
		    strcpy(compName[numFiles], tComponentName[i][0]);
		    componentTable[numFiles].name = compName[numFiles];
			numFiles ++;
		}
	}
	tableCount = numFiles;


    if (eError != OMX_ErrorNone){
        printf("Error:  Could not build Component Table\n");
    }
  //  PVLOGGER2ANDROID((0,"OMX_Core::BuildComponentTable: OUT"));
    return eError;
}

OMX_ERRORTYPE ComponentTable_EventHandler(
        OMX_IN OMX_HANDLETYPE hComponent,
        OMX_IN OMX_PTR pAppData,
        OMX_IN OMX_EVENTTYPE eEvent,
        OMX_IN OMX_U32 nData1,
        OMX_IN OMX_U32 nData2,
        OMX_IN OMX_PTR pEventData)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE ComponentTable_EmptyBufferDone(
        OMX_OUT OMX_HANDLETYPE hComponent,
        OMX_OUT OMX_PTR pAppData,
        OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE ComponentTable_FillBufferDone(
        OMX_OUT OMX_HANDLETYPE hComponent,
        OMX_OUT OMX_PTR pAppData,
        OMX_OUT OMX_BUFFERHEADERTYPE* pBuffer)
{
    return OMX_ErrorNotImplemented;
}

OMX_BOOL TIOMXConfigParserRedirect(
    OMX_PTR aInputParameters,
    OMX_PTR aOutputParameters)

{
    OMX_BOOL Status = OMX_FALSE;
        
    Status = TIOMXConfigParser(aInputParameters, aOutputParameters);
    
    return Status;
}

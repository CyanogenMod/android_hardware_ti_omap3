
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
/**
* @file iLBCEnc_Test.c
*
* This file implements iLBC Encoder Component Test Application to verify
* which is fully compliant with the Khronos OpenMAX (TM) 1.0 Specification
*
* @path  $(CSLPATH)\OMAPSW_MPU\linux\audio\src\openmax_il\ilbc_enc\tests
*
* @rev  1.0
**/
/* ----------------------------------------------------------------------------
*!
*! Revision History
*! ===================================
*! 04-Jul-2008 ad: Update for June 08 code review findings.
*! 10-Feb-2008 on: Initial Version
*! This is newest file
* =========================================================================== */
/* ------compilation control switches -------------------------*/
/****************************************************************
*  INCLUDE FILES
****************************************************************/
/* ----- system and platform files ----------------------------*/

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <errno.h>
#include <linux/vt.h>
#include <signal.h>
#include <sys/stat.h>
#include <pthread.h>
#include <linux/soundcard.h>

#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#include <OMX_Index.h>
#include <OMX_Types.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Audio.h>
#include <TIDspOmx.h>
#ifdef DSP_RENDERING_ON
#include <AudioManagerAPI.h>
#endif
#include <time.h>
#ifdef OMX_GETTIME
    #include <OMX_Common_Utils.h>
    #include <OMX_GetTime.h>     /*Headers for Performance & measuremet    */
#endif

int preempted = 0;
#undef  WAITFORRESOURCES

/* ======================================================================= */
/**
 * @def ILBCENC_INPUT_BUFFER_SIZE        Default input buffer size
 *      ILBCENC_INPUT_BUFFER_SIZE_DASF  Default input buffer size DASF
 */
/* ======================================================================= */

#define ILBCAPP_PRIMARY_INPUT_BUFFER_SIZE 320
#define ILBCAPP_PRIMARY_INPUT_BUFFER_SIZE_DASF 320

#define ILBCAPP_SECONDARY_INPUT_BUFFER_SIZE 480
#define ILBCAPP_SECONDARY_INPUT_BUFFER_SIZE_DASF 480


/* ======================================================================= */
/**
 * @def ILBCENC_OUTPUT_BUFFER_SIZE   Default output buffer size
 */
/* ======================================================================= */
#define ILBCAPP_PRIMARY_OUTPUT_BUFFER_SIZE 38
#define ILBCAPP_SECONDARY_OUTPUT_BUFFER_SIZE 50

/* ======================================================================= */
/**
 * @def ILBCENC_APP_ID  App ID Value setting
 */
/* ======================================================================= */
#define ILBCENC_APP_ID 100

#define FIFO1 "/dev/fifo.1"
#define FIFO2 "/dev/fifo.2"

#undef APP_DEBUG

#undef APP_MEMCHECK

#undef USE_BUFFER

#define STRESS_TEST_ITERATIONS 20
#ifdef APP_DEBUG
    #define APP_DPRINT(...)    fprintf(stderr,__VA_ARGS__)
#else
    #define APP_DPRINT(...)
#endif

#ifdef APP_MEMCHECK
    #define APP_MEMPRINT(...)    fprintf(stderr,__VA_ARGS__)
#else
    #define APP_MEMPRINT(...)
#endif

#ifdef OMX_GETTIME
  OMX_ERRORTYPE eError = OMX_ErrorNone;
  int GT_FlagE = 0;  /* Fill Buffer 1 = First Buffer,  0 = Not First Buffer  */
  int GT_FlagF = 0;  /*Empty Buffer  1 = First Buffer,  0 = Not First Buffer  */
  static OMX_NODE* pListHead = NULL;
#endif

/* ======================================================================= */
/**
 * @def    APP_DEBUGMEM   Turns memory leaks messaging on and off.
 *         ILBCENC_DEBUGMEM must be defined in OMX Comp in order to get
 *         this functionality On.
 */
/* ======================================================================= */
#undef APP_DEBUGMEM
/*#define APP_DEBUGMEM*/

#ifdef APP_DEBUGMEM
void *arr[500] = {NULL};
int lines[500] = {0};
int bytes[500] = {0};
char file[500][50] = {""};
int ind=0;

#define newmalloc(x) mynewmalloc(__LINE__,__FILE__,x)
#define newfree(z) mynewfree(z,__LINE__,__FILE__)

void * mynewmalloc(int line, char *s, int size)
{
   void *p = NULL;    
   int e=0;
   p = calloc(1,size);
   if(p==NULL){
       printf("Memory not available\n");
       exit(1);
       }
   else{
         while((lines[e]!=0)&& (e<500) ){
              e++;
         }
         arr[e]=p;
         lines[e]=line;
         bytes[e]=size;
         strcpy(file[e],s);
         printf("Allocating %d bytes on address %p, line %d file %s pos %d\n", size, p, line, s, e);
         return p;
   }
}

int mynewfree(void *dp, int line, char *s){
    int q = 0;
    if(dp==NULL){
                 printf("NULL can't be deleted\n");
                 return 0;
    }
    for(q=0;q<500;q++){
        if(arr[q]==dp){
           printf("Deleting %d bytes on address %p, line %d file %s\n", bytes[q],dp, line, s);
           free(dp);
           dp = NULL;
           lines[q]=0;
           strcpy(file[q],"");
           break;
        }            
     }    
     if(500==q)
         printf("\n\nPointer not found. Line:%d    File%s!!\n\n",line, s);
}
#else
#define newmalloc(x) malloc(x)
#define newfree(z) free(z)
#endif


typedef struct ILBCENC_BUFDATA {
   OMX_U8 nFrames;     
}ILBCENC_BUFDATA;

/* ======================================================================= */
/**
 *  M A C R O S FOR MALLOC and MEMORY FREE and CLOSING PIPES
 */
/* ======================================================================= */

#define OMX_ILBCAPP_CONF_INIT_STRUCT(_s_, _name_)   \
    memset((_s_), 0x0, sizeof(_name_)); \
    (_s_)->nSize = sizeof(_name_);      \
    (_s_)->nVersion.s.nVersionMajor = 0xF1;  \
    (_s_)->nVersion.s.nVersionMinor = 0xF2;  \
    (_s_)->nVersion.s.nRevision = 0x0;      \
    (_s_)->nVersion.s.nStep = 0x0

#define OMX_ILBCAPP_INIT_STRUCT(_s_, _name_)    \
    memset((_s_), 0x0, sizeof(_name_)); \

#define OMX_ILBCAPP_MALLOC_STRUCT(_pStruct_, _sName_)   \
    _pStruct_ = (_sName_*)newmalloc(sizeof(_sName_));      \
    if(_pStruct_ == NULL){      \
        printf("***********************************\n"); \
        printf("%d Malloc Failed\n",__LINE__); \
        printf("***********************************\n"); \
        eError = OMX_ErrorInsufficientResources; \
        goto EXIT;      \
    } \
    APP_MEMPRINT("%d ALLOCATING MEMORY = %p\n",__LINE__,_pStruct_);

/* ======================================================================= */
/** iLBCENC_COMP_PORT_TYPE  Port types
 *
 *  @param  iLBCENC_INPUT_PORT            Input port
 *
 *  @param  iLBCENC_OUTPUT_PORT           Output port
 */
/*  ====================================================================== */
/*This enum must not be changed. */
typedef enum iLBCENC_COMP_PORT_TYPE {
    iLBCENC_INPUT_PORT = 0,
    iLBCENC_OUTPUT_PORT
}iLBCENC_COMP_PORT_TYPE;

/* ======================================================================= */
/**
 * @def iLBCENC_MAX_NUM_OF_BUFS       Maximum number of buffers
 * @def iLBCENC_NUM_OF_CHANNELS       Number of Channels
 * @def iLBCENC_SAMPLING_FREQUENCY    Sampling frequency
 */
/* ======================================================================= */
#define iLBCENC_MAX_NUM_OF_BUFS 10
#define iLBCENC_NUM_OF_CHANNELS 1
#define iLBCENC_SAMPLING_FREQUENCY 8000

OMX_U32 inputBufferSize = ILBCAPP_PRIMARY_INPUT_BUFFER_SIZE;
OMX_U32 outputBufferSize = ILBCAPP_PRIMARY_OUTPUT_BUFFER_SIZE;
static OMX_BOOL bInvalidState;
void* ArrayOfPointers[6] = {NULL};
OMX_ERRORTYPE StopComponent(OMX_HANDLETYPE *pHandle);
OMX_ERRORTYPE PauseComponent(OMX_HANDLETYPE *pHandle);
OMX_ERRORTYPE PlayComponent(OMX_HANDLETYPE *pHandle);
OMX_ERRORTYPE send_input_buffer(OMX_HANDLETYPE pHandle, OMX_BUFFERHEADERTYPE* pBuffer, FILE *fIn);
int maxint(int a, int b);

int inputPortDisabled = 0;
int outputPortDisabled = 0;
OMX_U8 NextBuffer[ILBCAPP_SECONDARY_INPUT_BUFFER_SIZE * 3] = {0}; /*check this */
int FirstTime = 1;
int nRead = 0;
int lastbuffersent = 0;
int fdwrite = 0;
int fdread = 0;
#ifdef DSP_RENDERING_ON
AM_COMMANDDATATYPE cmd_data;
#endif

OMX_STRING striLBCEncoder = "OMX.TI.ILBC.encode";

#ifndef USE_BUFFER
int FreeAllResources( OMX_HANDLETYPE *pHandle,
                            OMX_BUFFERHEADERTYPE* pBufferIn,
                            OMX_BUFFERHEADERTYPE* pBufferOut,
                            int NIB, int NOB,
                            FILE* fIn, FILE* fOut);
#else
int  FreeAllResources(OMX_HANDLETYPE *pHandle,
                          OMX_U8* UseInpBuf[],
                          OMX_U8* UseOutBuf[],          
                          int NIB, int NOB,
                          FILE* fIn, FILE* fOut);
#endif  
int IpBuf_Pipe[2] = {0};
int OpBuf_Pipe[2] = {0};
int Event_Pipe[2] = {0};

fd_set rfds;
int DasfMode = 0;
int mframe=0;
static OMX_BOOL bInvalidState;
/* safe routine to get the maximum of 2 integers */
int maxint(int a, int b)
{
   return (a>b) ? a : b;
}

pthread_mutex_t WaitForState_mutex;
pthread_cond_t  WaitForState_threshold;
OMX_U8          WaitForState_flag = 0;
OMX_U8          TargetedState = 0;  
OMX_STATETYPE   state = OMX_StateInvalid;

/* This method will wait for the component to get to the state
 * specified by the DesiredState input. */
static OMX_ERRORTYPE WaitForState(OMX_HANDLETYPE* pHandle,
                                  OMX_STATETYPE DesiredState)
{
    OMX_STATETYPE CurState = OMX_StateInvalid;
    OMX_ERRORTYPE eError = OMX_ErrorNone;

    if (bInvalidState == OMX_TRUE){
        eError = OMX_ErrorInvalidState;
        return eError;
    }

    eError = OMX_GetState(pHandle, &CurState);
    if(CurState != DesiredState){
        WaitForState_flag = 1;
        TargetedState = DesiredState;
        pthread_mutex_lock(&WaitForState_mutex); 
        pthread_cond_wait(&WaitForState_threshold, &WaitForState_mutex);/*Going to sleep till signal arrives*/
        pthread_mutex_unlock(&WaitForState_mutex);                 
    }

    return eError;

}

OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE hComponent,
                           OMX_PTR pAppData,
                           OMX_EVENTTYPE eEvent,
                           OMX_U32 nData1,
                           OMX_U32 nData2,
                           OMX_PTR pEventData)
{
    APP_DPRINT( "%d [iLBC TEST] Entering EventHandler \n", __LINE__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_STATETYPE state = OMX_StateInvalid;
    OMX_U8 writeValue = 0;

    eError = OMX_GetState (hComponent, &state);
    if(eError != OMX_ErrorNone) {
        APP_DPRINT("%d [iLBC TEST] Error returned from GetState\n",__LINE__);
        goto EXIT;
    }
    APP_DPRINT( "%d [iLBC TEST] Component eEvent = %d\n", __LINE__,eEvent);
    switch (eEvent) {
        APP_DPRINT( "%d [iLBC TEST] Component State Changed To %d\n", __LINE__,state);
    case OMX_EventCmdComplete:
        APP_DPRINT( "%d [iLBC TEST] Component State Changed To %d\n", __LINE__,state);
        if (nData1 == OMX_CommandPortDisable) {
            if (nData2 == iLBCENC_INPUT_PORT) {
                inputPortDisabled = 1;
            }
            if (nData2 == iLBCENC_OUTPUT_PORT) {
                outputPortDisabled = 1;
            }
        }
        if ((nData1 == OMX_CommandStateSet) && (TargetedState == nData2) && (WaitForState_flag)){
            WaitForState_flag = 0;
            pthread_mutex_lock(&WaitForState_mutex);
            pthread_cond_signal(&WaitForState_threshold);/*Sending Waking Up Signal*/
            pthread_mutex_unlock(&WaitForState_mutex);
        }
        break;
    case OMX_EventError:
        if (nData1 == OMX_ErrorInvalidState) {
            bInvalidState = OMX_TRUE;
            APP_DPRINT("EventHandler: Invalid State!!!!\n");
            if (WaitForState_flag){
                WaitForState_flag = 0;
                pthread_mutex_lock(&WaitForState_mutex);
                pthread_cond_signal(&WaitForState_threshold);
                pthread_mutex_unlock(&WaitForState_mutex);
            }
        }
                   
        else if(nData1 == OMX_ErrorResourcesPreempted) {
            preempted = 1;
            writeValue = 0;  
            write(Event_Pipe[1], &writeValue, sizeof(OMX_U8));
        }
        else if (nData1 == OMX_ErrorResourcesLost) { 
            WaitForState_flag = 0;
            pthread_mutex_lock(&WaitForState_mutex);
            pthread_cond_signal(&WaitForState_threshold);
            pthread_mutex_unlock(&WaitForState_mutex);            
        }
        break;
    case OMX_EventMax:
        APP_DPRINT( "%d [iLBC TEST] Component OMX_EventMax = %d\n", __LINE__,eEvent);
        break;
    case OMX_EventMark:
        APP_DPRINT( "%d [iLBC TEST] Component OMX_EventMark = %d\n", __LINE__,eEvent);
        break;
    case OMX_EventPortSettingsChanged:
        APP_DPRINT( "%d [iLBC TEST] Component OMX_EventPortSettingsChanged = %d\n", __LINE__,eEvent);
        break;
    case OMX_EventBufferFlag:
        APP_DPRINT( "%d [iLBC TEST] OMX_EventBufferFlag (event %d) received from port %ld\n", __LINE__,eEvent,nData1);
#ifdef WAITFORRESOURCES
        writeValue = 2;  
        write(Event_Pipe[1], &writeValue, sizeof(OMX_U8));
#endif
        break;
    case OMX_EventResourcesAcquired:
        APP_DPRINT( "%d [iLBC TEST] Component OMX_EventResourcesAcquired = %d\n", __LINE__,eEvent);
        writeValue = 1;
        preempted = 0;
        write(Event_Pipe[1], &writeValue, sizeof(OMX_U8));
        break;
    default:
        break;

    }
 EXIT:
    APP_DPRINT( "%d [iLBC TEST] Exiting EventHandler \n", __LINE__);
    return eError;
}

void FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
    write(OpBuf_Pipe[1], &pBuffer, sizeof(pBuffer));
#ifdef OMX_GETTIME
    if (GT_FlagF == 1 ){ /* First Buffer Reply*/  /* 1 = First Buffer,  0 = Not First Buffer  */
        GT_END("Call to FillBufferDone  <First: FillBufferDone>");
        GT_FlagF = 0 ;   /* 1 = First Buffer,  0 = Not First Buffer  */
    }
#endif
}

void EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
    if (!preempted) 
        write(IpBuf_Pipe[1], &pBuffer, sizeof(pBuffer));
#ifdef OMX_GETTIME
    if (GT_FlagE == 1 ){ /* First Buffer Reply*/  /* 1 = First Buffer,  0 = Not First Buffer  */
        GT_END("Call to EmptyBufferDone <First: EmptyBufferDone>");
        GT_FlagE = 0;   /* 1 = First Buffer,  0 = Not First Buffer  */
    }
#endif
}

                
int main(int argc, char* argv[])
{
    OMX_CALLBACKTYPE iLBCCaBa = {(void *)EventHandler,
                                 (void*)EmptyBufferDone,
                                 (void*)FillBufferDone};
    OMX_HANDLETYPE pHandle;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 AppData = ILBCENC_APP_ID;
    OMX_PARAM_PORTDEFINITIONTYPE* pCompPrivateStruct = NULL;
    OMX_AUDIO_PARAM_GSMEFRTYPE *piLBCParam = NULL;
    OMX_BUFFERHEADERTYPE* pInputBufferHeader[iLBCENC_MAX_NUM_OF_BUFS] = {NULL};
    OMX_BUFFERHEADERTYPE* pOutputBufferHeader[iLBCENC_MAX_NUM_OF_BUFS] = {NULL};
    bInvalidState=OMX_FALSE;
#ifdef USE_BUFFER
    OMX_U8* pInputBuffer[iLBCENC_MAX_NUM_OF_BUFS] = {NULL};
    OMX_U8* pOutputBuffer[iLBCENC_MAX_NUM_OF_BUFS] = {NULL};
#endif
    TI_OMX_DSP_DEFINITION* audioinfo = NULL;
    TI_OMX_STREAM_INFO *streaminfo = NULL;

    FILE* fIn = NULL;
    FILE* fOut = NULL;
    struct timeval tv;
    int retval = 0, i = 0, j = 0, k = 0, kk = 0, tcID = 0;
    int frmCount = 0;
    int testcnt = 1;
    int testcnt1 = 1;
    int status = 0;
    int fdmax = 0;
    int nFrameCount = 1;
    int nFrameLen = 0;
    int nOutBuff = 1;
    int NoDataRead=0;
    OMX_INDEXTYPE index = 0;
    OMX_U32 streamId = 0;
    int numInputBuffers=0,numOutputBuffers=0;
    TI_OMX_DATAPATH dataPath;
    OMX_AUDIO_CONFIG_VOLUMETYPE* pCompPrivateStructGain = NULL;
    srand ( time(NULL) );
    bInvalidState=OMX_FALSE;
    
    pthread_mutex_init(&WaitForState_mutex, NULL);
    pthread_cond_init (&WaitForState_threshold, NULL);
    WaitForState_flag = 0;
    
    printf("------------------------------------------------------\n");
    printf("This is Main Thread In ILBC ENCODER Test Application:\n");
    printf("Test Core 1.5 - " __DATE__ ":" __TIME__ "\n");
    printf("------------------------------------------------------\n");

#ifdef OMX_GETTIME
    GTeError = OMX_ListCreate(&pListHead);
    printf("eError = %d\n",GTeError);
    GT_START();
#endif  

    /* check the input parameters */
    if((argc < 9) || (argc > 10)) {
        printf("Usage: [TestApp] [infile] [outfile] [test case] [FM/DM] [DTXON/\
OFF] [# frames] [1 to N] [1 to N] [# of stress test iterations (optional)]\n");
        puts("Example :");
        puts( "\tF2F  Usage: ./iLBCEncTest_common patterns/F00.INP output.cod\
 FUNC_ID_1 FM DTXOFF 0 1 1");
        puts( "\tDASM Usage: ./iLBCEncTest_common patterns/F00.INP output.cod\
 FUNC_ID_1 DM DTXOFF 500 1 1");
        goto EXIT;
    }

    /* check to see that the input file exists */
    struct stat sb = {0};
    status = stat(argv[1], &sb);
    if( status != 0 ) {
        APP_DPRINT("Cannot find file %s. (%u)\n", argv[1], errno);
        goto EXIT;
    }

    /* Open the file of data to be encoded. */
    fIn = fopen(argv[1], "r");
    if( fIn == NULL ) {
        APP_DPRINT("Error:  failed to open the input file %s\n", argv[1]);
        goto EXIT;
    }
    /* Open the file of data to be written. */
    fOut = fopen(argv[2], "w");
    if( fOut == NULL ) {
        APP_DPRINT("Error:  failed to open the output file %s\n", argv[2]);
        goto EXIT;
    }
    
    if(!strcmp(argv[3],"FUNC_ID_1")) {
        printf(" ### Testing TESTCASE 1 PLAY TILL END ###\n");
        tcID = 1;
    } else if(!strcmp(argv[3],"FUNC_ID_2")) {
        printf(" ### Testing TESTCASE 2 STOP IN THE END ###\n");
        tcID = 2;
    } else if(!strcmp(argv[3],"FUNC_ID_3")) {
        printf(" ### Testing TESTCASE 3 PAUSE - RESUME IN BETWEEN ###\n");
        tcID = 3;
    } else if(!strcmp(argv[3],"FUNC_ID_4")) {
        printf(" ### Testing TESTCASE 4 STOP IN BETWEEN ###\n");
        testcnt = 2;
        tcID = 4;
    } else if(!strcmp(argv[3],"FUNC_ID_5")){
        printf(" ### Testing TESTCASE 5 ENCODE without Deleting component Here ###\n");
        if (argc == 10){
            testcnt = atoi(argv[9]);
        }
        else{
            testcnt = STRESS_TEST_ITERATIONS;  /*20 cycles by default*/
        }
        tcID = 5;
    } else if(!strcmp(argv[3],"FUNC_ID_6")) {
        printf(" ### Testing TESTCASE 6 ENCODE with Deleting component Here ###\n");
        if (argc == 10){
            testcnt1 = atoi(argv[9]);
        }
        else {
            testcnt1 = STRESS_TEST_ITERATIONS;  /*20 cycles by default*/
        }
        tcID = 6;
    } else if(!strcmp(argv[3],"FUNC_ID_7")) {
        printf(" ### Testing TESTCASE 7 ENCODE with Volume Control ###\n");
        tcID = 7;
    } else if(!strcmp(argv[3],"FUNC_ID_8")) {
        printf(" ### Testing PLAY TILL END  WITH TWO FRAMES BY BUFFER###\n");
        tcID = 1;
        mframe = 1;
    }

    for(j = 0; j < testcnt1; j++) {
#ifdef DSP_RENDERING_ON  
        if((fdwrite=open(FIFO1,O_WRONLY))<0) {
            printf("[ILBCTEST] - failure to open WRITE pipe\n");
        }
        else {
            printf("[ILBCTEST] - opened WRITE pipe\n");
        }

        if((fdread=open(FIFO2,O_RDONLY))<0) {
            printf("[ILBCTEST] - failure to open READ pipe\n");
            goto EXIT;
        }
        else {
            printf("[ILBCTEST] - opened READ pipe\n");
        }
#endif      
        /* Create a pipe used to queue data from the callback. */
        retval = pipe(IpBuf_Pipe);
        if( retval != 0) {
            APP_DPRINT("Error:Fill Data Pipe failed to open\n");
            goto EXIT;
        }

        retval = pipe(OpBuf_Pipe);
        if( retval != 0) {
            APP_DPRINT("Error:Empty Data Pipe failed to open\n");
            goto EXIT;
        }
        retval = pipe(Event_Pipe);
        if(retval != 0) {
            APP_DPRINT( "Error:Empty Event Pipe failed to open\n");
            goto EXIT;
        }
        /* save off the "max" of the handles for the selct statement */
        fdmax = maxint(IpBuf_Pipe[0], OpBuf_Pipe[0]);
        fdmax = maxint(fdmax,Event_Pipe[0]);

        /* Create an iLBC Encoder instance */
        eError = TIOMX_Init();
        if(eError != OMX_ErrorNone) {
            APP_DPRINT("%d Error returned by TIOMX_Init()\n",__LINE__);
            goto EXIT;
        }

        OMX_ILBCAPP_MALLOC_STRUCT(streaminfo, TI_OMX_STREAM_INFO);
        OMX_ILBCAPP_MALLOC_STRUCT(audioinfo, TI_OMX_DSP_DEFINITION);
        OMX_ILBCAPP_INIT_STRUCT(audioinfo, TI_OMX_DSP_DEFINITION);

        ArrayOfPointers[0]=(TI_OMX_STREAM_INFO*)streaminfo;
        ArrayOfPointers[1]=(TI_OMX_DSP_DEFINITION*)audioinfo;

        if(j > 0) {
            printf ("%d Encoding the file for %d Time in TESTCASE 6\n",__LINE__,j+1);
            fIn = fopen(argv[1], "r");
            if( fIn == NULL ) {
                fprintf(stderr, "Error:  failed to open the file %s for read only access\n",argv[1]);
                goto EXIT;
            }

            fOut = fopen(argv[2], "a");
            if( fOut == NULL ) {
                fprintf(stderr, "Error:  failed to create the output file %s\n",argv[2]);
                goto EXIT;
            }
        }

        /* Load the ILBC Encoder Component */
#ifdef OMX_GETTIME
        GT_START();
        eError = OMX_GetHandle(&pHandle, striLBCEncoder, &AppData, &iLBCCaBa);
        GT_END("Call to GetHandle");
#else 
        eError = TIOMX_GetHandle(&pHandle, striLBCEncoder, &AppData, &iLBCCaBa);
#endif
        if((eError != OMX_ErrorNone) || (pHandle == NULL)) {
            APP_DPRINT("Error in Get Handle function\n");
            goto EXIT;
        }

        /* Setting No.Of Input and Output Buffers for the Component */
        numInputBuffers = atoi(argv[7]);
        APP_DPRINT("\n%d [iLBC TEST] numInputBuffers = %ld \n",__LINE__,numInputBuffers);

        numOutputBuffers = atoi(argv[8]);
        APP_DPRINT("\n%d [iLBC TEST] numOutputBuffers = %ld \n",__LINE__,numOutputBuffers);

        if(!(strcmp(argv[5],"DTXON"))) {
            /* Discontinuous Transmission Mode is enabled  */
            inputBufferSize = ILBCAPP_SECONDARY_INPUT_BUFFER_SIZE;
            outputBufferSize = ILBCAPP_SECONDARY_OUTPUT_BUFFER_SIZE; 
        }

        OMX_ILBCAPP_MALLOC_STRUCT(pCompPrivateStruct, OMX_PARAM_PORTDEFINITIONTYPE);
        OMX_ILBCAPP_CONF_INIT_STRUCT(pCompPrivateStruct, OMX_PARAM_PORTDEFINITIONTYPE);

        OMX_ILBCAPP_MALLOC_STRUCT(piLBCParam, OMX_AUDIO_PARAM_GSMEFRTYPE);
        OMX_ILBCAPP_CONF_INIT_STRUCT(piLBCParam, OMX_AUDIO_PARAM_GSMEFRTYPE);
    
        ArrayOfPointers[2]=(OMX_PARAM_PORTDEFINITIONTYPE*)pCompPrivateStruct;
        ArrayOfPointers[3] = (OMX_AUDIO_PARAM_GSMEFRTYPE *)piLBCParam;

        pCompPrivateStruct->nPortIndex = iLBCENC_INPUT_PORT;
        eError = OMX_GetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
        if (eError != OMX_ErrorNone) {
            eError = OMX_ErrorBadParameter;
            APP_DPRINT("%d OMX_ErrorBadParameter\n",__LINE__);
            goto EXIT;
        }
        APP_DPRINT("%d Setting input port config\n",__LINE__);
        pCompPrivateStruct->nBufferCountActual                 = numInputBuffers;
        pCompPrivateStruct->nBufferCountMin                    = numInputBuffers;
        pCompPrivateStruct->nBufferSize                        = inputBufferSize;

        if(!(strcmp(argv[4],"FM"))) {
            audioinfo->dasfMode = 0;
            DasfMode = 0;
            APP_DPRINT("\n%d [iLBC TEST] audioinfo->dasfMode = %ld \n",__LINE__,audioinfo->dasfMode);
        } else if(!(strcmp(argv[4],"DM"))){
            audioinfo->dasfMode =  1;
            DasfMode = 1;
            APP_DPRINT("\n%d [iLBC TEST] audioinfo->dasfMode = %ld \n",__LINE__,audioinfo->dasfMode);
            APP_DPRINT("%d ILBC ENCODER RUNNING UNDER DASF MODE \n",__LINE__);
            pCompPrivateStruct->nBufferCountActual = 0;
        }  else {
            eError = OMX_ErrorBadParameter;
            printf("\n%d [iLBC TEST] audioinfo->dasfMode Sending Bad Parameter\n",__LINE__);
            printf("%d [iLBC TEST] Should Be One of these Modes FM, DM\n",__LINE__);
            goto EXIT;
        }

        if(audioinfo->dasfMode == 0) {
            if((atoi(argv[6])) != 0) {
                eError = OMX_ErrorBadParameter;
                printf("\n%d [iLBC TEST] No. of Frames Sending Bad Parameter\n",__LINE__);
                printf("%d [iLBC TEST] For FILE mode argv[6] Should Be --> 0\n",__LINE__);
                printf("%d [iLBC TEST] For DASF mode argv[6] Should be greater than ze\
ro depends on number of frames user want to encode\n",__LINE__);
                goto EXIT;
            }
        } else {
            if((atoi(argv[6])) == 0) {
                eError = OMX_ErrorBadParameter;
                printf("\n%d [iLBC TEST] No. of Frames Sending Bad Parameter\n",__LINE__);
                printf("%d [iLBC TEST] For DASF mode argv[6] Should be greater than ze\
ro depends on number of frames user want to encode\n",__LINE__);
                printf("%d [iLBC TEST] For FILE mode argv[6] Should Be --> 0\n",__LINE__);
                goto EXIT;
            }
        }
#ifdef OMX_GETTIME
        GT_START();
        eError = OMX_SetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
        GT_END("Set Parameter Test-SetParameter");
#else
        eError = OMX_SetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
#endif
        if (eError != OMX_ErrorNone) {
            eError = OMX_ErrorBadParameter;
            APP_DPRINT("%d OMX_ErrorBadParameter\n",__LINE__);
            goto EXIT;
        }

        pCompPrivateStruct->nPortIndex = iLBCENC_OUTPUT_PORT;
        eError = OMX_GetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
        if (eError != OMX_ErrorNone) {
            eError = OMX_ErrorBadParameter;
            APP_DPRINT("%d OMX_ErrorBadParameter\n",__LINE__);
            goto EXIT;
        }

        APP_MEMPRINT("%d Setting output port config\n",__LINE__);
        pCompPrivateStruct->nBufferCountActual                 = numOutputBuffers;
        pCompPrivateStruct->nBufferCountMin                    = numOutputBuffers;
        pCompPrivateStruct->nBufferSize                        = outputBufferSize;

#ifdef OMX_GETTIME
        GT_START();
        eError = OMX_SetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
        GT_END("Set Parameter Test-SetParameter");
#else
        eError = OMX_SetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
#endif
        if (eError != OMX_ErrorNone) {
            eError = OMX_ErrorBadParameter;
            APP_DPRINT("%d OMX_ErrorBadParameter\n",__LINE__);
            goto EXIT;
        }

        eError = OMX_GetExtensionIndex(pHandle, "OMX.TI.index.config.ilbc.paramAudio",&index);
        if (eError != OMX_ErrorNone) {
            APP_DPRINT("Error returned from OMX_GetExtensionIndex\n");
            goto EXIT;
        }
        piLBCParam->nPortIndex = iLBCENC_OUTPUT_PORT;
        eError = OMX_GetParameter (pHandle, index, piLBCParam);
        if (eError != OMX_ErrorNone) {
            eError = OMX_ErrorBadParameter;
            APP_DPRINT("%d OMX_ErrorBadParameter\n",__LINE__);
            goto EXIT;
        }
        if(!(strcmp(argv[5],"DTXON"))) {
            /* Discontinuous Transmission Mode is enabled  */
            piLBCParam->bDTX = OMX_TRUE;
        }else if(!(strcmp(argv[5],"DTXOFF"))) {
            /* Discontinuous Transmission Mode is disabled */
            piLBCParam->bDTX = OMX_FALSE;
        } else {
            eError = OMX_ErrorBadParameter;
            printf("\n%d [iLBC TEST] piLBCParam->bDTX Sending Bad Parameter\n",__LINE__);
            printf("%d [iLBC TEST] Should Be One of these Modes DTXON, DTXOFF\n",__LINE__);
            goto EXIT;
        }

#ifdef OMX_GETTIME
        GT_START();
        eError = OMX_SetParameter (pHandle, index, piLBCParam);
        GT_END("Set Parameter Test-SetParameter");
#else
        eError = OMX_SetParameter (pHandle, index, piLBCParam);
#endif
        if (eError != OMX_ErrorNone) {
            eError = OMX_ErrorBadParameter;
            APP_DPRINT("%d OMX_ErrorBadParameter\n",__LINE__);
            goto EXIT;
        }

        pCompPrivateStructGain = newmalloc (sizeof(OMX_AUDIO_CONFIG_VOLUMETYPE));
        if(pCompPrivateStructGain == NULL) {
            APP_DPRINT("%d [iLBC TEST] Malloc Failed\n",__LINE__);
            goto EXIT;
        }
        ArrayOfPointers[4] = (OMX_AUDIO_CONFIG_VOLUMETYPE*) pCompPrivateStructGain;

        /* default setting for gain */
        pCompPrivateStructGain->nSize = sizeof(OMX_AUDIO_CONFIG_VOLUMETYPE);
        pCompPrivateStructGain->nVersion.s.nVersionMajor    = 0xF1;
        pCompPrivateStructGain->nVersion.s.nVersionMinor    = 0xF2;
        pCompPrivateStructGain->nPortIndex                  = OMX_DirOutput;
        pCompPrivateStructGain->bLinear                     = OMX_FALSE;
        pCompPrivateStructGain->sVolume.nValue              = 50;               /* actual volume */
        pCompPrivateStructGain->sVolume.nMin                = 0;                /* min volume */
        pCompPrivateStructGain->sVolume.nMax                = 100;              /* max volume */


        eError = OMX_GetExtensionIndex(pHandle, "OMX.TI.index.config.tispecific",&index);
        if (eError != OMX_ErrorNone) {
            APP_DPRINT("Error returned from OMX_GetExtensionIndex\n");
            goto EXIT;
        }

#ifdef DSP_RENDERING_ON     
        cmd_data.hComponent = pHandle;
        cmd_data.AM_Cmd = AM_CommandIsInputStreamAvailable;
        cmd_data.param1 = 0;
        if((write(fdwrite, &cmd_data, sizeof(cmd_data)))<0) {
            printf("%d [iLBC TEST] - send command to audio manager\n", __LINE__);
        }
        if((read(fdread, &cmd_data, sizeof(cmd_data)))<0) {
            printf("%d [iLBC TEST] - failure to get data from the audio manager\n", __LINE__);
            goto EXIT;
        }
        audioinfo->streamId = cmd_data.streamID;
        streamId = audioinfo->streamId;
#endif
        eError = OMX_SetConfig (pHandle, index, audioinfo);
        if(eError != OMX_ErrorNone) {
            eError = OMX_ErrorBadParameter;
            APP_DPRINT("%d Error from OMX_SetConfig() function\n",__LINE__);
            goto EXIT;
        }

        eError = OMX_GetExtensionIndex(pHandle, "OMX.TI.index.config.ilbc.datapath",&index);
        if (eError != OMX_ErrorNone) {
            printf("Error getting extension index\n");
            goto EXIT;
        }

        if (audioinfo->dasfMode) {
            printf("***************StreamId=%ld******************\n", streamId);
#ifdef RTM_PATH  
            printf("Using Real Time Mixer Path\n");
            dataPath = DATAPATH_APPLICATION_RTMIXER;
#endif

#ifdef ETEEDN_PATH
            printf("Using Eteeden Path\n");
            dataPath = DATAPATH_APPLICATION;
#endif        
        }
        eError = OMX_SetConfig (pHandle, index, &dataPath);
        if(eError != OMX_ErrorNone) {
            eError = OMX_ErrorBadParameter;
            APP_DPRINT("%d iLBCDecTest.c :: Error from OMX_SetConfig() function\n",__LINE__);
            goto EXIT;
        }
#ifdef OMX_GETTIME
        GT_START();
#endif
        eError = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
        if(eError != OMX_ErrorNone) {
            APP_DPRINT("Error from SendCommand-Idle(Init) State function\n");
            goto EXIT;
        }

#ifndef USE_BUFFER
        APP_DPRINT("%d About to call OMX_AllocateBuffer\n",__LINE__);
        if(!DasfMode) {
            for(i = 0; i < numInputBuffers; i++) {
                /* allocate input buffer */
                APP_DPRINT("%d About to call OMX_AllocateBuffer for pInputBufferHeader[%d]\n",__LINE__, i);
                /*eError = OMX_AllocateBuffer(pHandle, &pInputBufferHeader[i], 0, NULL, inputBufferSize*3);*/
                eError = OMX_AllocateBuffer(pHandle, &pInputBufferHeader[i], 0, NULL, inputBufferSize);
                if(eError != OMX_ErrorNone) {
                    APP_DPRINT("%d Error returned by OMX_AllocateBuffer for pInputBufferHeader[%d]\n",__LINE__, i);
                    goto EXIT;
                }
            }
        }
        APP_DPRINT("\n%d [iLBC TEST] pCompPrivateStruct->nBufferSize --> %ld \n",__LINE__,pCompPrivateStruct->nBufferSize);
        for(i = 0; i < numOutputBuffers; i++) {
            /* allocate output buffer */
            APP_DPRINT("%d About to call OMX_AllocateBuffer for pOutputBufferHeader[%d]\n",__LINE__, i);
            eError = OMX_AllocateBuffer(pHandle, &pOutputBufferHeader[i], 1, NULL, outputBufferSize);
            /*eError = OMX_AllocateBuffer(pHandle, &pOutputBufferHeader[i], 1, NULL, outputBufferSize*3);*/
            if(eError != OMX_ErrorNone) {
                APP_DPRINT("%d Error returned by OMX_AllocateBuffer for pOutputBufferHeader[%d]\n",__LINE__, i);
                goto EXIT;
            }
        }
#else
        if(!DasfMode) {
            for(i = 0; i < numInputBuffers; i++) {
                pInputBuffer[i] = (OMX_U8*)newmalloc(inputBufferSize*3 + 256);
                APP_MEMPRINT("%d [TESTAPP ALLOC] pInputBuffer[%d] = %p\n",__LINE__,i,pInputBuffer[i]);
                if(NULL == pInputBuffer[i]) {
                    APP_DPRINT("%d Malloc Failed\n",__LINE__);
                    eError = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
                pInputBuffer[i] = pInputBuffer[i] + 128;
            
                /*  allocate input buffer */
                APP_DPRINT("%d About to call OMX_UseBuffer\n",__LINE__);
                eError = OMX_UseBuffer(pHandle, &pInputBufferHeader[i], 0, NULL, inputBufferSize*13, pInputBuffer[i]);
                if(eError != OMX_ErrorNone) {
                    APP_DPRINT("%d Error returned by OMX_UseBuffer()\n",__LINE__);
                    goto EXIT;
                }
            }
        }

        for(i = 0; i < numOutputBuffers; i++) {
            pOutputBuffer[i] = newmalloc (outputBufferSize*3 + 256);
            APP_MEMPRINT("%d [TESTAPP ALLOC] pOutputBuffer[%d] = %p\n",__LINE__,i,pOutputBuffer[i]);
            if(NULL == pOutputBuffer[i]) {
                APP_DPRINT("%d Malloc Failed\n",__LINE__);
                eError = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pOutputBuffer[i] = pOutputBuffer[i] + 128;

            /* allocate output buffer */
            APP_DPRINT("%d About to call OMX_UseBuffer\n",__LINE__);
            eError = OMX_UseBuffer(pHandle, &pOutputBufferHeader[i], 1, NULL, outputBufferSize*13, pOutputBuffer[i]);
            if(eError != OMX_ErrorNone) {
                APP_DPRINT("%d Error returned by OMX_UseBuffer()\n",__LINE__);
                goto EXIT;
            }
        }
#endif

        /* Wait for startup to complete */
        eError = WaitForState(pHandle, OMX_StateIdle);
#ifdef OMX_GETTIME
        GT_END("Call to SendCommand <OMX_StateIdle> After Disableport and Clear buffers");
#endif
        if(eError != OMX_ErrorNone) {
            APP_DPRINT( "Error:  hiLBCEncoder->WaitForState reports an eError %X\n", eError);
            goto EXIT;
        }

        for(i = 0; i < testcnt; i++) {
            nFrameCount = 1;
            nOutBuff = 1;
            if(i > 0) {
                printf("Encoding the file for %d Time\n",i+1);
                fIn = fopen(argv[1], "r");
                if(fIn == NULL) {
                    fprintf(stderr, "Error:  failed to open the file %s for readonly access\n", argv[1]);
                    goto EXIT;
                }
                fOut = fopen(argv[2], "a");
                if(fOut == NULL) {
                    fprintf(stderr, "Error:  failed to create the output file %s\n", argv[2]);
                    goto EXIT;
                }
            }

            APP_DPRINT("%d [iLBC TEST] Sending OMX_StateExecuting Command\n",__LINE__);
#ifdef OMX_GETTIME
            GT_START();
#endif
            eError = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
            if(eError != OMX_ErrorNone) {
                printf("Error from SendCommand-Executing State function\n");
                goto EXIT;
            }
            eError = WaitForState(pHandle, OMX_StateExecuting);
#ifdef OMX_GETTIME
            GT_END("Call to SendCommand <OMX_StateExecuting>");
#endif
            if(eError != OMX_ErrorNone) {
                APP_DPRINT( "Error:  hiLBCEncoder->WaitForState reports an eError %X\n", eError);
                goto EXIT;
            }
            if(audioinfo->dasfMode == 0) {
                for (k=0; k < numInputBuffers; k++) {
                    OMX_BUFFERHEADERTYPE* pBuffer = pInputBufferHeader[k];          
                    pBuffer->nFlags=0;
#ifdef OMX_GETTIME
                    if (k==0){ 
                        GT_FlagE=1;  /* 1 = First Buffer,  0 = Not First Buffer  */
                        GT_START(); /* Empty Bufffer */
                    }
#endif
                    eError =  send_input_buffer(pHandle, pBuffer, fIn);             
                }
            }

            for (kk = 0; kk < numOutputBuffers; kk++) {
                APP_DPRINT("%d [iLBC TEST] Calling FillThisBuffer \n",__LINE__);
#ifdef OMX_GETTIME
                if (k==0){ 
                    GT_FlagF=1;  /* 1 = First Buffer,  0 = Not First Buffer  */
                    GT_START(); /* Fill Buffer */
                }
#endif
                OMX_FillThisBuffer(pHandle, pOutputBufferHeader[kk]);
            }

            eError = OMX_GetState(pHandle, &state);
            if(eError != OMX_ErrorNone) {
                APP_DPRINT("%d OMX_GetState has returned status %X\n",__LINE__, eError);
                goto EXIT;
            }
    
#ifdef WAITFORRESOURCES
            while(1){
                if( (error == OMX_ErrorNone) && (state != OMX_StateIdle) && 
                    (state != OMX_StateInvalid)){
#else
            while((eError == OMX_ErrorNone) && (state != OMX_StateIdle) && 
                    (state != OMX_StateInvalid) ){
                if(1){
#endif            
                    FD_ZERO(&rfds);
                    FD_SET(IpBuf_Pipe[0], &rfds);
                    FD_SET(OpBuf_Pipe[0], &rfds);
                    FD_SET(Event_Pipe[0], &rfds);

                    tv.tv_sec = 1;
                    tv.tv_usec = 0;

                    retval = select(fdmax+1, &rfds, NULL, NULL, &tv);
                    if(retval == -1) {
                        perror("select()");
                        APP_DPRINT( "[iLBC TEST] select() call error \n");
                        break;
                    }
            
                    if(!retval){
                        NoDataRead++;
                        if(NoDataRead == 3){
                            printf("Stoping component since No data is read from the pipes\n");          
                            StopComponent(pHandle); 
                        }
                    }
                    else{
                        NoDataRead=0;
                    }
            
                    switch (tcID) {
                    case 1:
                    case 2:
                    case 3:
                    case 4:             
                    case 5:
                    case 6:
                    case 7:     
                        if(audioinfo->dasfMode == 0) {
                            if(FD_ISSET(IpBuf_Pipe[0], &rfds)) {                                           
                                OMX_BUFFERHEADERTYPE* pBuffer = NULL;
                                read(IpBuf_Pipe[0], &pBuffer, sizeof(pBuffer));
                                if (frmCount==15 && tcID ==3){ /*Pause the component*/
                                    printf("[iLBC TEST] Pausing Component for 5 Seconds\n");
                                    PauseComponent(pHandle);
                                    sleep(2);
                                    printf("[iLBC TEST] Resume Component\n");
                                    PlayComponent(pHandle);
                                    frmCount++;
                                }
                                if (nFrameCount == 50 && ((tcID == 4) || (tcID == 2))){
                                    lastbuffersent = 1;
                                    StopComponent(pHandle);
                                    break;
                                }
                                eError = send_input_buffer(pHandle, pBuffer, fIn);
                                if (eError != OMX_ErrorNone) { 
                                    APP_DPRINT("goto EXIT %d\n",__LINE__);
                                    /*goto EXIT; */
                                } 
                            }
                        } else {
                            if (frmCount==15 && tcID ==3){  /*Pause the component*/
                                tcID = 1;
                                printf("[iLBC TEST] Pausing Component for 5 Seconds\n");
                                PauseComponent(pHandle);
                                sleep(2);
                                printf("[iLBC TEST] Resume Component\n");
                                PlayComponent(pHandle);         
                                frmCount++;
                            }
                            if (nFrameCount==50 && tcID ==4){ /*Stop the component*/
                                printf("Stoping the Component And Starting Again\n");
                                lastbuffersent = 1;
                                StopComponent(pHandle);
                                nFrameCount = 0;
                                tcID = 1;
                                break;
                            }
                            if(nFrameCount == 10 && tcID == 7) {
                                /* set high gain for record stream */
                                printf("[iLBC encoder] --- will set stream gain to high\n");
                                pCompPrivateStructGain->sVolume.nValue = 0x8000;
                                eError = OMX_SetConfig(pHandle, OMX_IndexConfigAudioVolume, pCompPrivateStructGain);
                                if (eError != OMX_ErrorNone) {
                                    eError = OMX_ErrorBadParameter;
                                    goto EXIT;
                                }
                            }
                            if(nFrameCount == 250 && tcID == 7){
                                /* set low gain for record stream */
                                printf("[iLBC encoder] --- will set stream gain to low\n");
                                pCompPrivateStructGain->sVolume.nValue = 0x2000;
                                eError = OMX_SetConfig(pHandle, OMX_IndexConfigAudioVolume, pCompPrivateStructGain);
                                if (eError != OMX_ErrorNone){
                                        eError = OMX_ErrorBadParameter;
                                        goto EXIT;
                                    }                           
                            }                    
                            APP_DPRINT("%d iLBC ENCODER RUNNING UNDER DASF MODE \n",__LINE__);
                            if(nFrameCount == atoi(argv[6])) {
                                lastbuffersent = 1;
                                StopComponent(pHandle);  
                            }
                            APP_DPRINT("%d iLBC ENCODER READING DATA FROM DASF  \n",__LINE__);
                        }
                        break;      
                    default:
                        APP_DPRINT("%d ### Simple DEFAULT Case Here ###\n",__LINE__);
                    }

                    if( FD_ISSET(OpBuf_Pipe[0], &rfds) ) {
                        OMX_BUFFERHEADERTYPE* pBuf = NULL;
                        read(OpBuf_Pipe[0], &pBuf, sizeof(pBuf));
                        APP_DPRINT("%d [iLBC TEST] pBuf->nFilledLen = %ld\n",__LINE__, pBuf->nFilledLen);
                        nFrameLen = (pBuf->nFilledLen) * 2;
                        frmCount++;
                        nFrameCount++;

                        APP_DPRINT("%d [iLBC TEST] nFrameLen = %d \n",__LINE__, nFrameLen);

                        if (nFrameLen != 0) {
                            APP_DPRINT("%d Writing OutputBuffer No: %d to the file nWrite = %d \n",__LINE__, nOutBuff, nFrameLen);
                            fwrite(pBuf->pBuffer, 1, nFrameLen, fOut);
                            fflush(fOut);
                        }
                        if(pBuf->nFlags == OMX_BUFFERFLAG_EOS) {
                            APP_DPRINT("%d [iLBC TEST] OMX_BUFFERFLAG_EOS is received\n",__LINE__);
                            pBuf->nFlags = 0;
                            StopComponent(pHandle);
                        }
                        else if(!lastbuffersent){
                            lastbuffersent = 0;
                            nOutBuff++;
                            OMX_FillThisBuffer(pHandle, pBuf);
                            APP_DPRINT("%d [iLBC TEST] pBuf->nFlags = %ld\n",__LINE__, pBuf->nFlags);
                        }
                    }

                    if( FD_ISSET(Event_Pipe[0], &rfds) ) {
                        OMX_U8 pipeContents = 0;
                        read(Event_Pipe[0], &pipeContents, sizeof(OMX_U8));

                        if (pipeContents == 0) {
                            printf("Test app received OMX_ErrorResourcesPreempted\n");
                            WaitForState(pHandle,OMX_StateIdle);
                            for (i=0; i < numInputBuffers; i++) {
                                eError = OMX_FreeBuffer(pHandle,iLBCENC_INPUT_PORT,pInputBufferHeader[i]);
                                if( (eError != OMX_ErrorNone)) {
                                    APP_DPRINT ("%d Error in Free Handle function\n",__LINE__);
                                }
                            }

                            for (i=0; i < numOutputBuffers; i++) {
                                eError = OMX_FreeBuffer(pHandle,iLBCENC_OUTPUT_PORT,pOutputBufferHeader[i]);
                                if( (eError != OMX_ErrorNone)) {
                                    APP_DPRINT ("%d Error in Free Handle function\n",__LINE__);
                                }
                            }
#ifdef USE_BUFFER
                            /* newfree the App Allocated Buffers */
                            APP_DPRINT("%d [GSMFRTEST] Freeing the App Allocated Buffers in TestApp\n",__LINE__);
                            for(i=0; i < numInputBuffers; i++) {
                                APP_MEMPRINT("%d [TESTAPPFREE] pInputBuffer[%d] = %p\n",__LINE__,i,pInputBuffer[i]);
                                if(pInputBuffer[i] != NULL){
                                    pInputBuffer[i] = pInputBuffer[i] - 128;
                                    newfree(pInputBuffer[i]);
                                    pInputBuffer[i] = NULL;
                                }
                            }
                            for(i=0; i < numOutputBuffers; i++) {
                                APP_MEMPRINT("%d [TESTAPPFREE] pOutputBuffer[%d] = %p\n",__LINE__,i, pOutputBuffer[i]);
                                if(pOutputBuffer[i] != NULL){
                                    pOutputBuffer[i] = pOutputBuffer[i] - 128;                            
                                    newfree(pOutputBuffer[i]);
                                    pOutputBuffer[i] = NULL;
                                }
                            }
#endif                        
                            OMX_SendCommand(pHandle,OMX_CommandStateSet,OMX_StateLoaded,NULL);
                            WaitForState(pHandle,OMX_StateLoaded);
                            OMX_SendCommand(pHandle,OMX_CommandStateSet,OMX_StateWaitForResources,NULL);
                            WaitForState(pHandle,OMX_StateWaitForResources);
                        }
                        if (pipeContents == 1) {
                            printf("Test app received OMX_ErrorResourcesAcquired\n");

                            OMX_SendCommand(pHandle,OMX_CommandStateSet,OMX_StateIdle,NULL);
                            for (i=0; i < numInputBuffers; i++) {
                                /* allocate input buffer */
                                eError = OMX_AllocateBuffer( pHandle,
                                                            &pOutputBufferHeader[i],
                                                            1,
                                                            NULL,
                                                            outputBufferSize*3);
                                if(eError != OMX_ErrorNone) {
                                    APP_DPRINT("%d Error returned by OMX_AllocateBuffer()\n",__LINE__);
                                }
                            }

                            WaitForState(pHandle,OMX_StateIdle);
                            OMX_SendCommand(pHandle,OMX_CommandStateSet,OMX_StateExecuting,NULL);
                            WaitForState(pHandle,OMX_StateExecuting);
                            rewind(fIn);
                            for (i=0; i < numInputBuffers;i++) {    
                                send_input_buffer(pHandle, pInputBufferHeader[i], fIn);
                            }
                        }
                        if (pipeContents == 2) {

                            /* Send component to Idle */
                            StopComponent(pHandle);
#ifdef WAITFORRESOURCES
                            for (i=0; i < numInputBuffers; i++) {
                                eError = OMX_FreeBuffer(pHandle,iLBCENC_INPUT_PORT,pInputBufferHeader[i]);
                                if( (eError != OMX_ErrorNone)) {
                                    APP_DPRINT ("%d Error in Free Handle function\n",__LINE__);
                                }
                            }

                            for (i=0; i < numOutputBuffers; i++) {
                                eError = OMX_FreeBuffer(pHandle,iLBCENC_OUTPUT_PORT,pOutputBufferHeader[i]);
                                if( (eError != OMX_ErrorNone)) {
                                    APP_DPRINT ("%d:: Error in Free Handle function\n",__LINE__);
                                }
                            }
                            eError = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
                            if(eError != OMX_ErrorNone) {
                                APP_DPRINT ("%d Error from SendCommand-Idle State function\n",__LINE__);
                                printf("goto EXIT %d\n",__LINE__);
                                goto EXIT;
                            }
                            eError = WaitForState(pHandle, OMX_StateLoaded);
#ifdef OMX_GETTIME
                            GT_END("Call to SendCommand <OMX_StateLoaded>");
#endif
                            if(eError != OMX_ErrorNone) {
                                APP_DPRINT( "%d Error:  WaitForState reports an error %X\n",__LINE__, error);
                                printf("goto EXIT %d\n",__LINE__);
                                goto EXIT;
                            }
                            goto SHUTDOWN;
#endif
                        }                        
                    }        
                
                    eError = OMX_GetState(pHandle, &state);
                    if(eError != OMX_ErrorNone) {
                        APP_DPRINT("%d OMX_GetState has returned status %X\n",__LINE__, eError);
                        goto EXIT;
                    }
                }else if (preempted){
                    sched_yield();
                }
                else{
                    goto SHUTDOWN;
                }
            } /* While Loop Ending Here */
            APP_DPRINT("%d [iLBC TEST] The current state of the component = %d \n",__LINE__,state);
            fclose(fOut);
            fclose(fIn);
            FirstTime = 1;
            NoDataRead = 0;
            lastbuffersent = 0;
            printf("[iLBC TEST] iLBC Encoded = %d Frames\n",(nOutBuff));
        } /*Test Case 4 & 5 Inner for loop ends here  */

#ifndef WAITFORRESOURCES
        /* newfree the Allocate and Use Buffers */  
        APP_DPRINT("%d [iLBC TEST] Freeing the Allocate OR Use Buffers in TestApp\n",__LINE__);    
        if(!DasfMode) {
            for(i=0; i < numInputBuffers; i++) {
                APP_DPRINT("%d [iLBC TEST] About to newfree pInputBufferHeader[%d]\n",__LINE__, i);
                eError = OMX_FreeBuffer(pHandle, iLBCENC_INPUT_PORT, pInputBufferHeader[i]);
                if((eError != OMX_ErrorNone)) {
                    APP_DPRINT("%d  Error in FreeBuffer function\n",__LINE__);
                    goto EXIT;
                }
                pInputBufferHeader[i] = NULL;
            }
        }
        for(i=0; i < numOutputBuffers; i++) {
            APP_DPRINT("%d [iLBC TEST] About to newfree pOutputBufferHeader[%d]\n",__LINE__, i);
            eError = OMX_FreeBuffer(pHandle, iLBCENC_OUTPUT_PORT, pOutputBufferHeader[i]);
            if((eError != OMX_ErrorNone)) {
                APP_DPRINT("%d Error in Free Buffer function\n",__LINE__);
                goto EXIT;
            }
            pOutputBufferHeader[i] = NULL;
        }
   
#ifdef USE_BUFFER
        /* newfree the App Allocated Buffers */
        APP_DPRINT("%d [iLBC TEST] Freeing the App Allocated Buffers in TestApp\n",__LINE__);
        if(!DasfMode) {
            for(i=0; i < numInputBuffers; i++) {        
                if(pInputBuffer[i] != NULL){
                    APP_MEMPRINT("%d [iLBC TEST] [TESTAPPFREE] pInputBuffer[%d] = %p\n",__LINE__,i,pInputBuffer[i]);
                    pInputBuffer[i] = pInputBuffer[i] - 128;
                    newfree(pInputBuffer[i]);
                    pInputBuffer[i] = NULL;
                }
            }
        }
        for(i=0; i < numOutputBuffers; i++) {
            if(pOutputBuffer[i] != NULL){
                APP_MEMPRINT("%d [iLBC TEST] [TESTAPPFREE] pOutputBuffer[%d] = %p\n",__LINE__,i, pOutputBuffer[i]);
                pOutputBuffer[i] = pOutputBuffer[i] - 128;
                newfree(pOutputBuffer[i]);
                pOutputBuffer[i] = NULL;
            }
        }
#endif
#endif
        APP_DPRINT ("%d [iLBC TEST] Sending the OMX_StateLoaded Command\n",__LINE__);
#ifdef OMX_GETTIME
        GT_START();
#endif
        eError = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
        if(eError != OMX_ErrorNone) {
            APP_DPRINT("%d  Error from SendCommand-Idle State function\n",__LINE__);
            goto EXIT;
        }
        eError = WaitForState(pHandle, OMX_StateLoaded);
#ifdef OMX_GETTIME
        GT_END("Call to SendCommand <OMX_StateLoaded>");
#endif
        if ( eError != OMX_ErrorNone ){
            printf("Error: WaitForState has timed out %d", eError);
            goto EXIT;
        }
        
#ifdef WAITFORRESOURCES
        eError = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateWaitForResources, NULL);
        if(eError != OMX_ErrorNone) {
            APP_DPRINT ("%d Error from SendCommand-Idle State function\n",__LINE__);
            printf("goto EXIT %d\n",__LINE__);
        
            goto EXIT;
        }
        eError = WaitForState(pHandle, OMX_StateWaitForResources);

        /* temporarily put this here until I figure out what should really happen here */
        sleep(10);
        /* temporarily put this here until I figure out what should really happen here */
#endif 
SHUTDOWN:

        APP_DPRINT ("%d [iLBC TEST] Sending the OMX_CommandPortDisable Command\n",__LINE__);
        eError = OMX_SendCommand(pHandle, OMX_CommandPortDisable, -1, NULL);
        if(eError != OMX_ErrorNone) {
            APP_DPRINT("%d  Error from SendCommand OMX_CommandPortDisable\n",__LINE__);
            goto EXIT;
        }
    
        APP_DPRINT("%d [iLBC TEST] Freeing the Memory Allocated in TestApp\n",__LINE__);

        APP_MEMPRINT("%d [iLBC TEST] [TESTAPPFREE] %p\n",__LINE__,piLBCParam);
        if(piLBCParam != NULL){
            newfree(piLBCParam);
            piLBCParam = NULL;
        }
        APP_MEMPRINT("%d [iLBC TEST] [TESTAPPFREE] %p\n",__LINE__,pCompPrivateStruct);
        if(pCompPrivateStruct != NULL){
            newfree(pCompPrivateStruct);
            pCompPrivateStruct = NULL;
        }
        APP_MEMPRINT("%d [iLBC TEST] [TESTAPPFREE] %p\n",__LINE__,audioinfo);
        if(audioinfo != NULL){
            newfree(audioinfo);
            audioinfo = NULL;
        }
        APP_MEMPRINT("%d [iLBC TEST] [TESTAPPFREE] %p\n",__LINE__,streaminfo);
        if(streaminfo != NULL){
            newfree(streaminfo);
            streaminfo = NULL;
        }
    
        APP_DPRINT("%d [iLBC TEST] Closing the Input and Output Pipes\n",__LINE__);
        eError = close (IpBuf_Pipe[0]);
        if (0 != eError && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            APP_DPRINT("%d Error while closing IpBuf_Pipe[0]\n",__LINE__);
            goto EXIT;
        }
        eError = close (IpBuf_Pipe[1]);
        if (0 != eError && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            APP_DPRINT("%d Error while closing IpBuf_Pipe[1]\n",__LINE__);
            goto EXIT;
        }
        eError = close (OpBuf_Pipe[0]);
        if (0 != eError && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            APP_DPRINT("%d Error while closing OpBuf_Pipe[0]\n",__LINE__);
            goto EXIT;
        }
        eError = close (OpBuf_Pipe[1]);
        if (0 != eError && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            APP_DPRINT("%d Error while closing OpBuf_Pipe[1]\n",__LINE__);
            goto EXIT;
        }
    
        eError = close (Event_Pipe[0]);
        if (0 != eError && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            APP_DPRINT("%d Error while closing Event_Pipe[0]\n",__LINE__);
            goto EXIT;
        }
        eError = close (Event_Pipe[1]);
        if (0 != eError && OMX_ErrorNone == eError) {
            eError = OMX_ErrorHardware;
            APP_DPRINT("%d Error while closing Event_Pipe[1]\n",__LINE__);
            goto EXIT;
        }

#ifdef DSP_RENDERING_ON     
        cmd_data.hComponent = pHandle;
        cmd_data.AM_Cmd = AM_Exit;
        APP_DPRINT("%d DSP_REndering\n",__LINE__);

        if((write(fdwrite, &cmd_data, sizeof(cmd_data)))<0)
            printf("%d- send command to audio manager\n",__LINE__);

        close(fdwrite);
        close(fdread);
#endif
        APP_DPRINT("%d [iLBC TEST] Free the Component handle\n",__LINE__);
        /* Unload the iLBC Encoder Component */
        eError = TIOMX_FreeHandle(pHandle);
        if((eError != OMX_ErrorNone)) {
            APP_DPRINT("%d Error in Free Handle function\n",__LINE__);
            goto EXIT;
        }
        APP_DPRINT("%d [iLBC TEST] Free Handle returned Successfully\n",__LINE__);

        newfree(pCompPrivateStructGain);

    } /*Outer for loop ends here */

    pthread_mutex_destroy(&WaitForState_mutex);
    pthread_cond_destroy(&WaitForState_threshold);

    eError = TIOMX_Deinit();
    if( (eError != OMX_ErrorNone)) {
        APP_DPRINT("APP: Error in Deinit Core function\n");
        goto EXIT;
    }

    printf("%d *********************************************************************\n",__LINE__);
    printf("%d NOTE: An output file %s has been created in file system\n",__LINE__,argv[2]);
    printf("%d *********************************************************************\n",__LINE__);
 EXIT:

    if(bInvalidState==OMX_TRUE){
#ifndef USE_BUFFER
        eError = FreeAllResources(pHandle,
                                  pInputBufferHeader[0],
                                  pOutputBufferHeader[0],
                                  numInputBuffers,
                                  numOutputBuffers,
                                  fIn,
                                  fOut);
#else
        eError = FreeAllResources(pHandle,
                                  pInputBuffer,
                                  pOutputBuffer,
                                  numInputBuffers,
                                  numOutputBuffers,
                                  fIn,
                                  fOut);
#endif
        }
#ifdef APP_DEBUGMEM    
    printf("\n-Printing memory not deleted-\n");
    for(i=0;i<500;i++){
        if (lines[i]!=0){
            printf(" --->%d Bytes allocated on File:%s Line: %d\n",bytes[i],file[i],lines[i]); 
        }
    }
#endif 
#ifdef OMX_GETTIME
    GT_END("ILBC_ENC test <End>");
    OMX_ListDestroy(pListHead); 
#endif    
    return eError;
}

OMX_ERRORTYPE send_input_buffer(OMX_HANDLETYPE pHandle, 
                                OMX_BUFFERHEADERTYPE* pBuffer, 
                                FILE *fIn)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;

    if(FirstTime){
        if(mframe){
            nRead = fread(pBuffer->pBuffer, 1, inputBufferSize*2, fIn);
        }
        else{
            nRead = fread(pBuffer->pBuffer, 1, inputBufferSize, fIn);
        }       
        pBuffer->nFilledLen = nRead;
    }
    else{
        memcpy(pBuffer->pBuffer, NextBuffer,nRead);
        pBuffer->nFilledLen = nRead;
    }
    
    if(mframe){
        nRead = fread(NextBuffer, 1, inputBufferSize*2, fIn);
    }
    else{
        nRead = fread(NextBuffer, 1, inputBufferSize, fIn);
    }   

    if(nRead < inputBufferSize){
        pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
        APP_DPRINT("Sending Last Input Buffer from App\n");
    }else{
        pBuffer->nFlags = 0;
    }
        
    if(pBuffer->nFilledLen != 0){       
        pBuffer->nTimeStamp = rand() % 100;
        if (!preempted) {
            error = OMX_EmptyThisBuffer(pHandle, pBuffer);
            if (error == OMX_ErrorIncorrectStateOperation) 
                error = 0;      
        }
    }
    FirstTime=0;
    return error;   
}
OMX_ERRORTYPE StopComponent(OMX_HANDLETYPE *pHandle)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
#ifdef OMX_GETTIME
    GT_START();
#endif
    error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
    if(error != OMX_ErrorNone) {
        fprintf (stderr,"\nError from SendCommand-Idle(Stop) State function!!!!!!!!\n");
        goto EXIT;
    }
    error = WaitForState(pHandle, OMX_StateIdle);
#ifdef OMX_GETTIME
    GT_END("Call to SendCommand <OMX_StateIdle>");
#endif
    if(error != OMX_ErrorNone) {
        fprintf(stderr, "\nError:  hILBCEncoder->WaitForState reports an error %X!!!!!!!\n", error);
        goto EXIT;
    }
EXIT:
    return error;
}

OMX_ERRORTYPE PauseComponent(OMX_HANDLETYPE *pHandle)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
#ifdef OMX_GETTIME
    GT_START();
#endif
    error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StatePause, NULL);
    if(error != OMX_ErrorNone) {
        fprintf (stderr,"\nError from SendCommand-Idle(Stop) State function!!!!!!!!\n");
        goto EXIT;
    }
    error = WaitForState(pHandle, OMX_StatePause);
#ifdef OMX_GETTIME
    GT_END("Call to SendCommand <OMX_StatePause>");
#endif
    if(error != OMX_ErrorNone) {
        fprintf(stderr, "\nError:  hILBCEncoder->WaitForState reports an error %X!!!!!!!\n", error);
        goto EXIT;
    }
EXIT:
    return error;
}

OMX_ERRORTYPE PlayComponent(OMX_HANDLETYPE *pHandle)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
#ifdef OMX_GETTIME
    GT_START();
#endif
    error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if(error != OMX_ErrorNone) {
        fprintf (stderr,"\nError from SendCommand-Idle(Stop) State function!!!!!!!!\n");
        goto EXIT;
    }
    error = WaitForState(pHandle, OMX_StateExecuting);
#ifdef OMX_GETTIME
    GT_END("Call to SendCommand <OMX_StateExecuting>");
#endif
    if(error != OMX_ErrorNone) {
        fprintf(stderr, "\nError:  hILBCEncoder->WaitForState reports an error %X!!!!!!!\n", error);
        goto EXIT;
    }
EXIT:
    return error;
}

/*=================================================================

                            Freeing All allocated resources
                            
==================================================================*/
#ifndef USE_BUFFER
int FreeAllResources( OMX_HANDLETYPE *pHandle,
                            OMX_BUFFERHEADERTYPE* pBufferIn,
                            OMX_BUFFERHEADERTYPE* pBufferOut,
                            int NIB, int NOB,
                            FILE* fileIn, FILE* fileOut)
{
    printf("%d Freeing all resources by state invalid \n",__LINE__);
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U16 i = 0; 
    if(!DasfMode) {
        for(i=0; i < NIB; i++) {           
               if(pBufferIn+i!=NULL){
                    printf("%d APP: About to newfree pInputBufferHeader[%d]\n",__LINE__, i);               
                    eError = OMX_FreeBuffer(pHandle, OMX_DirInput, pBufferIn+i);
             }

        }
    }

    for(i=0; i < NOB; i++) {
          if(pBufferOut+i!=NULL){
           printf("%d APP: About to newfree pOutputBufferHeader[%d]\n",__LINE__, i);
           eError = OMX_FreeBuffer(pHandle, OMX_DirOutput, pBufferOut+i);
         }
    }

    /*i value is fixed by the number calls to newmalloc in the App */
    for(i=0; i<5;i++)  
    {
        if (ArrayOfPointers[i] != NULL)
            newfree(ArrayOfPointers[i]);
    }

    TIOMX_FreeHandle(pHandle);

    return eError;
}


/*=================================================================

                            Freeing the resources with USE_BUFFER define
                            
==================================================================*/
#else

int FreeAllResources(OMX_HANDLETYPE *pHandle,
                            OMX_U8* UseInpBuf[],
                            OMX_U8* UseOutBuf[],            
                            int NIB,int NOB,
                            FILE* fileIn, FILE* fileOut)
{

        OMX_ERRORTYPE eError = OMX_ErrorNone;
        OMX_U16 i = 0; 
        printf("%d Freeing all resources by state invalid \n",__LINE__);
        /* newfree the UseBuffers */
        for(i=0; i < NIB; i++) {
           UseInpBuf[i] = UseInpBuf[i] - 128;
           printf("%d [TESTAPPFREE] pInputBuffer[%d] = %p\n",__LINE__,i,(UseInpBuf[i]));
           if(UseInpBuf[i] != NULL){
              newfree(UseInpBuf[i]);
              UseInpBuf[i] = NULL;
           }
        }

        for(i=0; i < NOB; i++) {
           UseOutBuf[i] = UseOutBuf[i] - 128;
           printf("%d [TESTAPPFREE] pOutputBuffer[%d] = %p\n",__LINE__,i, UseOutBuf[i]);
           if(UseOutBuf[i] != NULL){
              newfree(UseOutBuf[i]);
              UseOutBuf[i] = NULL;
           }
        }

    /*i value is fixed by the number calls to newmalloc in the App */
        for(i=0; i<4;i++)  
        {
            if (ArrayOfPointers[i] != NULL)
                newfree(ArrayOfPointers[i]);
        }
    
        TIOMX_FreeHandle(pHandle);
    
        return eError;
}

#endif


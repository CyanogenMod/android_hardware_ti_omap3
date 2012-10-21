
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
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/vt.h>
#include <signal.h>
#include <sys/stat.h>

#include <OMX_Index.h>
#include <OMX_Types.h>
#include <TIDspOmx.h>
#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Audio.h>

#include <pthread.h>
#include <stdio.h>
#include <linux/soundcard.h>
#ifdef DSP_RENDERING_ON
#include <AudioManagerAPI.h>
#endif
#include <time.h>
#include "OMX_iLBCDec_Utils.h"

/*#define WAITFORRESOURCES*/

#undef APP_DEBUG
/*#undef APP_DEBUG**/
/*#define APP_DEBUG **/
#undef APP_MEMCHECK
#undef USE_BUFFER

#ifdef APP_DEBUG
#define N_REPETITIONS 5
#else
#define N_REPETITIONS 20
#endif

#ifdef OMX_GETTIME
#include <OMX_Common_Utils.h>
#include <OMX_GetTime.h>     /*Headers for Performance & measuremet    */
#endif

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
 * @def    AFMT_AC3                             Dolby Digital AC3
 *           ./iLBCDecTest  patterns/T04.cod output.out 1 0 0 1 1
 *           GsmFrDecTest is test application itself.
 *           arg1 is input file name    e.g. “patterns/T04.cod”
 *           arg2: codec type:  0 - primary, 1 secondary.
 *           agr3 is Test Case Number ( 1 to 6 depends on Test Case chosen, explained in Test Report)
 *           arg4 is the DASF/File mode flag (for DASF - 1  and NONDASF -  0).
 *           arg5 is the acoustic mode flag (for acoustic supporting - 1  and for no acoustic supporting -  0).
 *           arg6 is to configure number of input buffer (1 to 4).
 *           arg7 is to configure number of output buffer (1 to 4). 
 */
/* ======================================================================= */

/* ======================================================================= */
/**
 * @def  GAIN                      Define a GAIN value for Configure Audio
 */
/* ======================================================================= */
#define GAIN 95

/* ======================================================================= */
/**
 * @def    EXTRA_BUFFBYTES                Num of Extra Bytes to be allocated
 */
/* ======================================================================= */
#define EXTRA_BUFFBYTES (256)

/* ======================================================================= */
/**
 * @def    APP_DEBUGMEM    This Macro turns On the logic to detec memory
 *                         leaks on the App. To debug the component, 
 *                         iLBCDEC_DEBUGMEM must be defined.
 */
/* ======================================================================= */
#undef APP_DEBUGMEM

#ifdef APP_DEBUGMEM
void *arr[500] = {NULL};
int lines[500] = {0};
int bytes[500] = {0};
char file[500][50] = {""};
int ind=0;
#endif
/*******************************/
#define MAX_NUM_OF_BUFS 10
/*******************************/
static OMX_BOOL     bInvalidState;
OMX_STATETYPE       state = OMX_StateInvalid;
pthread_mutex_t     WaitForState_mutex;
pthread_cond_t      WaitForState_threshold;
OMX_U8              WaitForState_flag = 0;
OMX_U8              TargetedState = 0;
OMX_U8              playcompleted = 0;
int                 FirstTime = 1;
#ifdef DSP_RENDERING_ON
AM_COMMANDDATATYPE  cmd_data;
#endif

int IpBuf_Pipe[2] = {0};
int OpBuf_Pipe[2] = {0};
int Event_Pipe[2] = {0};

int preempted = 0;

/**
 * PROTOTYPES
 */

OMX_BOOL             validateArguments         (int nArgc, char *sArgv [], char *iLBC_EncodedFile, OMX_U16 codecType, int tcID, int gDasfMode, int iAcousticMode, int nIpBuffs, int nOpBuffs);
OMX_S16              maxint                    (OMX_S16, OMX_S16);
static OMX_ERRORTYPE WaitForState              (OMX_HANDLETYPE *, OMX_STATETYPE);
OMX_ERRORTYPE        EventHandler              (OMX_HANDLETYPE, OMX_PTR, OMX_EVENTTYPE, OMX_U32, OMX_U32, OMX_PTR);
void                 FillBufferDone            (OMX_HANDLETYPE, OMX_PTR, OMX_BUFFERHEADERTYPE *);
void                 EmptyBufferDone           (OMX_HANDLETYPE, OMX_PTR, OMX_BUFFERHEADERTYPE *);
OMX_ERRORTYPE        StopComponent             (OMX_HANDLETYPE *);
OMX_ERRORTYPE        PauseComponent            (OMX_HANDLETYPE *);
OMX_ERRORTYPE        PlayComponent             (OMX_HANDLETYPE );
OMX_ERRORTYPE        omxSetInputPortParameter  (OMX_HANDLETYPE *, int, int);
OMX_ERRORTYPE        omxSetOutputPortParameter (OMX_HANDLETYPE *, int, int, int);
OMX_ERRORTYPE        iLBCSetInputPort          (OMX_HANDLETYPE *);
OMX_ERRORTYPE        iLBCSetOutputPort         (OMX_HANDLETYPE *, int);
OMX_ERRORTYPE        omxSetConfigMute          (OMX_HANDLETYPE *, OMX_BOOL);
OMX_ERRORTYPE        omxSetConfigVolume        (OMX_HANDLETYPE *, OMX_S32);
void                 iLBCSetCodecType          (OMX_HANDLETYPE *, OMX_U16);
OMX_ERRORTYPE        omxUseBuffers             (OMX_HANDLETYPE *, int, int, int, OMX_BUFFERHEADERTYPE * [], int, int, OMX_BUFFERHEADERTYPE * []);
OMX_ERRORTYPE        testCases                 (OMX_HANDLETYPE *, fd_set *,int, FILE *, FILE *, OMX_S16 *, int *, struct timeval *, int, OMX_BUFFERHEADERTYPE *[], int, OMX_U8 *, int, OMX_BUFFERHEADERTYPE *[], int);
OMX_ERRORTYPE        testCases_3_10_11         (OMX_HANDLETYPE *, int, OMX_S16 *);

OMX_ERRORTYPE        sendInputBuffer           (OMX_HANDLETYPE *pHandle, fd_set *rfds, int tcID, FILE *fIn, int *frmCount,  struct timeval *tv, int nInBufSize, int *count);
OMX_ERRORTYPE        send_input_buffer         (OMX_HANDLETYPE, OMX_BUFFERHEADERTYPE *, FILE *, int, OMX_S16);
OMX_ERRORTYPE        omxFreeBuffers            (OMX_HANDLETYPE *pHandle, int nBuffs, OMX_BUFFERHEADERTYPE *pBufferHeader  [], OMX_U32 nPortIndex);

void                 getString_OMX_State       (char *ptrString, OMX_STATETYPE state);
void                 printTestCaseInfo         (int);
/* ACA defines */
#define APP_OUTPUT_FILE  "output_iLBC.pcm"

/*******************************************************************************
 * MAIN
 ******************************************************************************/
int main(int argc, char* argv[])
{
    fd_set                rfds;
    OMX_CALLBACKTYPE      iLBCCaBa = {(void *)EventHandler,
                                      (void *)EmptyBufferDone,
                                      (void *)FillBufferDone
                                     };
    OMX_HANDLETYPE        pHandle;
    OMX_ERRORTYPE         error = OMX_ErrorNone;
    OMX_U32               AppData = 100;
    OMX_COMPONENTTYPE    *pComponent = NULL; 

    OMX_BUFFERHEADERTYPE *pInputBufferHeader  [MAX_NUM_OF_BUFS] = {NULL};
    OMX_BUFFERHEADERTYPE *pOutputBufferHeader [MAX_NUM_OF_BUFS] = {NULL};
    OMX_U8                count=0;
    bInvalidState = OMX_FALSE;
    TI_OMX_DATAPATH dataPath;

    char   *iLBC_EncFile     = argv [1];  
    OMX_U16 codecType        = atoi (argv [2]);
    int     command          = atoi (argv [3]);
    int     gDasfMode        = atoi (argv [4]);
    int     iAcousticMode    = atoi (argv [5]);
    OMX_S16 numInputBuffers  = atoi (argv [6]); /* # of input buffers:  1 to 4 */
    OMX_S16 numOutputBuffers = atoi (argv [7]); /* # of output buffers: 1 to 4 */       
        
#ifdef DSP_RENDERING_ON
    int ilbcdecfdwrite = 0;
    int ilbcdecfdread = 0;
#endif
#ifdef USE_BUFFER
    OMX_U8* pInputBuffer  [MAX_NUM_OF_BUFS] = {NULL}; 
    OMX_U8* pOutputBuffer [MAX_NUM_OF_BUFS] = {NULL};
#endif
    
    struct timeval tv;
    OMX_S16        retval = 0, 
                   i = 0, 
                   j = 0,
                   k = 0;
    OMX_S16        frmCount = 0;
    OMX_INDEXTYPE  index = 0;  
    OMX_U32        streamId = 0;
        
    char fname [50] = APP_OUTPUT_FILE;
    
    OMX_BOOL bFlag = validateArguments (argc, argv, iLBC_EncFile, codecType, command, gDasfMode, iAcousticMode, numInputBuffers, numOutputBuffers);    
    if (bFlag == OMX_FALSE) {
        printf ("<<<<<<<<< Argument validation fault >>>>>>>>>\n");
        exit (1);
    }
    OMX_S16 testcnt   = (command != 5)? 1: N_REPETITIONS;
    if (command == 2)
        testcnt = 2;
    OMX_S16 testcnt1  = (command != 6)? 1: N_REPETITIONS;

    /* Open the file of data to be rendered. */
    FILE *fIn  = NULL, 
         *fOut = fopen (fname, "w");
    fclose (fOut);

    /*TI_OMX_DSP_DEFINITION *audioinfo  = newmalloc(sizeof(TI_OMX_DSP_DEFINITION));*/
    TI_OMX_DSP_DEFINITION audioinfo;
    /*TI_OMX_STREAM_INFO    *streaminfo = newmalloc(sizeof(TI_OMX_STREAM_INFO));*/
    OMX_STRING             striLBCDecoder = STRING_iLBC_DECODE;
    
    pthread_mutex_init(&WaitForState_mutex, NULL);
    pthread_cond_init (&WaitForState_threshold, NULL);
    WaitForState_flag = 0;

#ifdef OMX_GETTIME
    GTeError = OMX_ListCreate(&pListHead);
    GT_START();
#endif  
      
    APP_DPRINT("%d :: iLBCDecTest.c :: \n",__LINE__);
    error = TIOMX_Init();
    if(error != OMX_ErrorNone) {
        APP_DPRINT("%d :: iLBCDecTest.c :: Error returned by TIOMX_Init()\n",__LINE__);
        exit (1);
    }
    
    APP_DPRINT("%d :: iLBCDecTest.c :: \n",__LINE__);
    for(j = 0; j < testcnt1; j++) {

        retval = pipe(IpBuf_Pipe);
        if( retval != 0) {
            APP_DPRINT( "%d :: iLBCDecTest.c :: Error:Fill Input Pipe failed to open\n",__LINE__);
            goto EXIT;
        }

        retval = pipe(OpBuf_Pipe);
        if( retval != 0) {
            APP_DPRINT( "%d :: iLBCDecTest.c :: Error:Empty Output Pipe failed to open\n",__LINE__);
            goto EXIT;
        }

        retval = pipe(Event_Pipe);
        if( retval != 0) {
            APP_DPRINT( "%d :: iLBCDecTest.c :: Error:Empty Event Pipe failed to open\n",__LINE__);
            goto EXIT;
        }
       
#ifdef DSP_RENDERING_ON
        if((ilbcdecfdwrite = open(FIFO1,O_WRONLY))<0) {
            printf("[iLBCTEST] - failure to open WRITE pipe\n");
        }
        else {
            printf("[iLBCTEST] - opened WRITE pipe\n");
        }

        if((ilbcdecfdread = open(FIFO2,O_RDONLY))<0) {
            printf("[iLBCTEST] - failure to open READ pipe\n");
        }
        else {
            printf("[iLBCTEST] - opened READ pipe\n");
        }
#endif
        
        /* Load the iLBC Decoder Component */
        APP_DPRINT("%d :: iLBCDecTest.c :: \n",__LINE__);
#ifdef OMX_GETTIME
    GT_START();
        error = OMX_GetHandle(&pHandle, striLBCDecoder, &AppData, &iLBCCaBa);
    GT_END("Call to GetHandle");
#else
        error = TIOMX_GetHandle(&pHandle, striLBCDecoder, &AppData, &iLBCCaBa);
#endif
        APP_DPRINT("%d :: iLBCDecTest.c :: \n",__LINE__);
        if((error != OMX_ErrorNone) || (pHandle == NULL)) {
            APP_DPRINT ("%d :: iLBCDecTest.c :: Error in Get Handle function\n",__LINE__);
            goto EXIT;
        }
/************************/
        int nInBufSize = STD_iLBCDEC_BUF_SIZE;
        if (codecType == 1) {
            nInBufSize = INPUT_iLBCDEC_SECBUF_SIZE;
        }
        error = omxSetInputPortParameter (pHandle, numInputBuffers, nInBufSize);
        if (error != OMX_ErrorNone) {
            exit (1);
        }
/********************/
        int nOutBufSize = iLBCD_OUTPUT_BUFFER_SIZE;
        if (codecType == 1) {
            nOutBufSize = OUTPUT_iLBCDEC_SECBUF_SIZE;
        }
        error = omxSetOutputPortParameter (pHandle, numOutputBuffers, nOutBufSize, gDasfMode);
        if (error != OMX_ErrorNone) {
            exit (1);
        }
/*********************/
        iLBCSetCodecType (pHandle, codecType);
/***********************/    
        /* default setting for Mute/Unmute */
        omxSetConfigMute (&pHandle, OMX_FALSE);/**/
        /* default setting for volume */ 
        omxSetConfigVolume (&pHandle, 50);
/*****/
        error = omxUseBuffers (&pHandle, 0, numInputBuffers, nInBufSize, pInputBufferHeader, numOutputBuffers, (command == 7 || command == 9)? nOutBufSize *2: nOutBufSize, pOutputBufferHeader);
        if (error != OMX_ErrorNone) {
            exit (1);
        }
/****** iLBCSetInputPort *******/        
        error = iLBCSetInputPort (&pHandle);
        if (error != OMX_ErrorNone) {
            exit (1);
        }
/***** iLBCSetOutputPort *****/
        error = iLBCSetOutputPort (&pHandle, iLBCDEC_SAMPLING_FREQUENCY);
        if (error != OMX_ErrorNone) {
            error = OMX_ErrorBadParameter;
            printf ("%d:: OMX_ErrorBadParameter\n",__LINE__);
            exit (1);
        }
/****************************/

        /* get TeeDN or ACDN mode */
        audioinfo.acousticMode = iAcousticMode;

        if (audioinfo.acousticMode == OMX_TRUE) {
            printf("Using Acoustic DeviceMode Path\n");
            dataPath = DATAPATH_ACDN;
        }
        else if (gDasfMode) {
#ifdef RTM_PATH 
            printf("Using Real Time Mixer Path\n");   
            dataPath = DATAPATH_APPLICATION_RTMIXER;
#endif

#ifdef ETEEDN_PATH
            printf("Using ETEEDN Path\n");
            dataPath = DATAPATH_APPLICATION;
#endif        
        }
        audioinfo.dasfMode = gDasfMode;
        
        error = OMX_GetExtensionIndex(pHandle, STRING_iLBC_HEADERINFO,&index);
        if (error != OMX_ErrorNone) {
            printf("Error getting extension index\n");
            goto EXIT;
        }

#ifdef DSP_RENDERING_ON
        cmd_data.hComponent = pHandle;
        cmd_data.AM_Cmd = AM_CommandIsOutputStreamAvailable;
        /* for encoder, using AM_CommandIsInputStreamAvailable */
        cmd_data.param1 = 0;
        if((write(ilbcdecfdwrite, &cmd_data, sizeof(cmd_data)))<0) {
            printf("%d iLBCDecTest - send command to audio manager\n", __LINE__);
        }
        if((read(ilbcdecfdread, &cmd_data, sizeof(cmd_data)))<0) {
            printf("%d iLBCDecTest - failure to get data from the audio manager\n", __LINE__);
            goto EXIT;
        }
        audioinfo.streamId = cmd_data.streamID;
        streamId = audioinfo.streamId;  
#endif

        error = OMX_SetConfig (pHandle, index, &audioinfo);
        if(error != OMX_ErrorNone) {
            error = OMX_ErrorBadParameter;
            APP_DPRINT("%d :: iLBCDecTest.c :: Error from OMX_SetConfig() function\n",__LINE__);
            goto EXIT;
        }
    
        error = OMX_GetExtensionIndex(pHandle, STRING_iLBC_DATAPATHINFO,&index);
        if (error != OMX_ErrorNone) {
            printf("Error getting extension index\n");
            goto EXIT;
        } 
    
        error = OMX_SetConfig (pHandle, index, &dataPath);
        if(error != OMX_ErrorNone) {
            error = OMX_ErrorBadParameter;
            APP_DPRINT("%d :: iLBCDecTest.c :: Error from OMX_SetConfig() function\n",__LINE__);
            goto EXIT;
        }
        
        APP_DPRINT ("%d :: iLBCDecTest.c Calling SendCommand-Idle(Init) State function\n",__LINE__);
    #ifdef OMX_GETTIME
        GT_START();
    #endif
        error = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
        if(error != OMX_ErrorNone) {
            APP_DPRINT ("%d :: iLBCDecTest.c :: Error from SendCommand-Idle(Init) State function\n",__LINE__);
            goto EXIT;
        }
        /* Wait for startup to complete */
        error = WaitForState(pHandle, OMX_StateIdle);
    #ifdef OMX_GETTIME
        GT_END("Call to SendCommand <OMX_StateIdle>");
    #endif
        if(error != OMX_ErrorNone) {
            APP_DPRINT( "%d :: iLBCDecTest.c :: Error:  WaitForState reports an error %X\n",__LINE__, error);
            goto EXIT;
        }
        if (gDasfMode) {
            /* get streamID back to application */
            error = OMX_GetExtensionIndex(pHandle, STRING_iLBC_STREAMIDINFO, &index);
            if (error != OMX_ErrorNone) {
                printf("Error getting extension index\n");
                goto EXIT;
            }
            printf("***************StreamId=%ld******************\n", streamId);
        }
        for(i = 0; i < testcnt; i++) {
            fIn = fopen(argv[1], "r");
            if(fIn == NULL) {
                fprintf(stderr, "Error:  failed to open the file %s for readonly access\n", argv[1]);
                goto EXIT;
            }
            if (gDasfMode == 0) {
                if (command == 5 || command == 6 || command == 2) {
                    if (testcnt == 0 && testcnt1 == 0) {
                        fOut = fopen(fname, "w");
                    } else {
                        fOut = fopen(fname, "a");
                    }
                } else {
                    fOut = fopen(fname, "w");
                }
                if(fOut == NULL) {
                    fprintf(stderr, "Error:  failed to create the output file \n");
                    goto EXIT;
                }
            }

    
            APP_DPRINT ("Sending OMX_StateExecuting Command\n");
#ifdef OMX_GETTIME
            GT_START();
#endif
            error = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
            if(error != OMX_ErrorNone) {
                APP_DPRINT ("%d :: iLBCDecTest.c :: Error from SendCommand-Executing State function\n",__LINE__);
                goto EXIT;
            }

            error = WaitForState(pHandle, OMX_StateExecuting);
#ifdef OMX_GETTIME
            GT_END("Call to SendCommand <OMX_StateExecuting>");
#endif
            if(error != OMX_ErrorNone) {
                APP_DPRINT( "%d :: iLBCDecTest.c :: Error:  WaitForState reports an error %X\n",__LINE__, error);
                goto EXIT;
            }
            pComponent = (OMX_COMPONENTTYPE *)pHandle;
/*******/
            for (k=0; k < numInputBuffers; k++) {
#ifdef OMX_GETTIME
                if (k==0) { 
                    GT_FlagE=1;  /* 1 = First Buffer,  0 = Not First Buffer  */
                    GT_START(); /* Empty Bufffer */
                }
#endif
                error = send_input_buffer (pHandle, pInputBufferHeader[k], fIn, nInBufSize, command);
            }

            if (gDasfMode ==0) {
                for (k=0; k < numOutputBuffers; k++) {
                    #ifdef OMX_GETTIME
                    if (k==0) { 
                       GT_FlagF=1;  /* 1 = First Buffer,  0 = Not First Buffer  */
                       GT_START(); /* Fill Buffer */
                    }
                    #endif
                    OMX_FillThisBuffer(pHandle,  pOutputBufferHeader[k]);
                }
            }
    /*******/

            error = OMX_GetState(pHandle, &state);/**/
            retval = 1;
            while (1) {
                if ( (error == OMX_ErrorNone) && (state != OMX_StateIdle) && (state != OMX_StateInvalid)){
                    error = testCases (pHandle, &rfds, command, fIn, fOut, &frmCount, NULL, &tv, gDasfMode, pInputBufferHeader, nInBufSize, &count, numInputBuffers, pOutputBufferHeader, numOutputBuffers);
                    if (error != OMX_ErrorNone) {
                        printf ("<<<<<<<<<<<<<< testCases fault exit >>>>>>>>>>>>>>\n");
                        exit (1);
                    }
                } else if (preempted) {
                    sched_yield ();
                } else {
                    break;
                    /* goto SHUTDOWN */
                }
            } /* While Loop Ending Here */
            APP_DPRINT ("The current state of the component = %d \n",state);
            count = 0;
            FirstTime = 1;
            frmCount = 0;
            /*PlayComponent (pHandle);*/
            if (gDasfMode == 0) {
                fclose(fOut);
            }
            fclose(fIn);
        } /*Inner for loop (testcnt) ends here */

#ifndef USE_BUFFER                
        /* newfree buffers */
        error = omxFreeBuffers (&pHandle, numInputBuffers, pInputBufferHeader, OMX_DirInput);
        if((error != OMX_ErrorNone)) {
            APP_DPRINT ("%d:: Error in Free Input Buffers function\n",__LINE__);
            exit (1);
        }
        
        error = omxFreeBuffers (&pHandle, numOutputBuffers, pOutputBufferHeader, OMX_DirOutput);
        if((error != OMX_ErrorNone)) {
            APP_DPRINT ("%d:: Error in Free Output Buffers function\n",__LINE__);
            exit (1);
        }



#else
        /* newfree the App Allocated Buffers */
        printf("%d :: iLBCDecTest.c :: Freeing the App Allocated Buffers in TestApp\n",__LINE__);
        for(i=0; i < numInputBuffers; i++) {
            APP_MEMPRINT("%d::: [TESTAPPFREE] pInputBuffer[%d] = %p\n",__LINE__,i,pInputBuffer[i]);
            if(pInputBuffer[i] != NULL){
                pInputBuffer[i] = pInputBuffer[i] - 128;
                newfree(pInputBuffer[i]);
                pInputBuffer[i] = NULL;
            }
        }
        for(i=0; i < numOutputBuffers; i++) {
            APP_MEMPRINT("%d::: [TESTAPPFREE] pOutputBuffer[%d] = %p\n",__LINE__,i, pOutputBuffer[i]);
            if(pOutputBuffer[i] != NULL){
                pOutputBuffer[i] = pOutputBuffer[i] - 128;                            
                newfree(pOutputBuffer[i]);
                pOutputBuffer[i] = NULL;
            }
        }
#endif


        APP_DPRINT ("Sending the StateLoaded Command\n");
    #ifdef OMX_GETTIME
        GT_START();
    #endif
        error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
        if(error != OMX_ErrorNone) {
            APP_DPRINT ("%d :: iLBCDecTest.c :: Error from SendCommand-Idle State function\n",__LINE__);
            goto EXIT;
        }
        error = WaitForState(pHandle, OMX_StateLoaded);
    #ifdef OMX_GETTIME
        GT_END("Call to SendCommand <OMX_StateLoaded>");
    #endif
        if(error != OMX_ErrorNone) {
            APP_DPRINT( "%d :: iLBCDecTest.c :: Error:  WaitForState reports an error %X\n",__LINE__, error);
            goto EXIT;
        }

#ifdef WAITFORRESOURCES
        error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateWaitForResources, NULL);
        if(error != OMX_ErrorNone) {
            APP_DPRINT ("%d :: AmrDecTest.c :: Error from SendCommand-Idle State function\n",__LINE__);
            printf("goto EXIT %d\n",__LINE__);

            goto EXIT;
        }
        error = WaitForState(pHandle, OMX_StateWaitForResources);

        /* temporarily put this here until I figure out what should really happen here */
/*        sleep(10);*/
#endif    

        error = OMX_SendCommand(pHandle, OMX_CommandPortDisable, -1, NULL);

#ifdef DSP_RENDERING_ON
        cmd_data.hComponent = pHandle;
        cmd_data.AM_Cmd = AM_Exit;
        if((write(ilbcdecfdwrite, &cmd_data, sizeof(cmd_data)))<0)
            printf("%d [iLBC Test] - send command to audio manager\n",__LINE__);
#endif
        APP_DPRINT ("Free the Component handle\n");
        /* Unload the iLBC Encoder Component */
        error = TIOMX_FreeHandle(pHandle);

        if( (error != OMX_ErrorNone)) {
            APP_DPRINT ("%d :: iLBCDecTest.c :: Error in Free Handle function\n",__LINE__);
            goto EXIT;
        }
    
        APP_DPRINT ("%d :: iLBCDecTest.c :: Free Handle returned Successfully \n\n\n\n",__LINE__);

#ifdef DSP_RENDERING_ON
        close(ilbcdecfdwrite);
        close(ilbcdecfdread);
#endif
        close (Event_Pipe[0]);
        close (Event_Pipe[1]);
        close (IpBuf_Pipe[0]);
        close (IpBuf_Pipe[1]);
        close (OpBuf_Pipe[0]);
        close (OpBuf_Pipe[1]);
    } /*Outer for loop ends here */


EXIT:
    error = TIOMX_Deinit();
    if( (error != OMX_ErrorNone)) {
        APP_DPRINT("APP: Error in Deinit Core function\n");
        goto EXIT;
    }
    pthread_mutex_destroy(&WaitForState_mutex);
    pthread_cond_destroy(&WaitForState_threshold);

#ifdef APP_DEBUGMEM     
    int r;
    printf("\n-Printing memory not delete it-\n");
    for(r=0;r<500;r++){
        if (lines[r]!=0){
            printf(" --->%d Bytes allocated on %p File:%s Line: %d\n",bytes[r],arr[r],file[r],lines[r]);                  
        }
    }
#endif   
#ifdef OMX_GETTIME
    GT_END("iLBCDec test <End>");
    OMX_ListDestroy(pListHead); 
#endif  
    return error;
} /* end main */

/* safe routine to get the maximum of 2 integers */
OMX_S16 maxint(OMX_S16 a, OMX_S16 b)
{
    return (a>b) ? a : b;
}

/************
 *
 */
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

/***********
 *
 */
OMX_ERRORTYPE EventHandler(OMX_HANDLETYPE hComponent,
                           OMX_PTR pAppData,
                           OMX_EVENTTYPE eEvent,
                           OMX_U32 nData1,
                           OMX_U32 nData2,
                           OMX_PTR pEventData)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U8        writeValue = 0;
    
    eError = OMX_GetState (hComponent, &state);
    
    if(eError != OMX_ErrorNone) {
        APP_DPRINT("%d :: iLBCDecTest.c :: Error returned from GetState\n",__LINE__);
    }
/**/
#ifdef APP_DEBUG        
    char strState [30] = {""};
    char strTarget [30] = {""};
    getString_OMX_State       (strState, state);
    getString_OMX_State       (strTarget, nData2);
#endif
 /**/
    switch (eEvent) {
    case OMX_EventResourcesAcquired:
        writeValue = 1;
        write(Event_Pipe[1], &writeValue, sizeof(OMX_U8));
        preempted = 0;
        break;
    case OMX_EventCmdComplete:
        APP_DPRINT ( "%d :: iLBCDecTest.c :: Event: OMX_EventCmdComplete. Component State Changed To %s. Target %s\n", __LINE__, strState, strTarget);
        if ((nData1 == OMX_CommandStateSet) && (TargetedState == nData2) && (WaitForState_flag)){
            WaitForState_flag = 0;
            pthread_mutex_lock(&WaitForState_mutex);
            pthread_cond_signal(&WaitForState_threshold);/*Sending Waking Up Signal*/
            pthread_mutex_unlock(&WaitForState_mutex);
        }
        break;

     case OMX_EventError:
        APP_DPRINT ( "%d :: iLBCDecTest.c  Event: OMX_EventError. Component State Changed To %s. Target %s\n", __LINE__, strState, strTarget);
        if (nData1 == OMX_ErrorInvalidState) {
            bInvalidState =OMX_TRUE;
            if (WaitForState_flag){
            WaitForState_flag = 0;
            pthread_mutex_lock(&WaitForState_mutex);
            pthread_cond_signal(&WaitForState_threshold);/*Sending Waking Up Signal*/
            pthread_mutex_unlock(&WaitForState_mutex);
            }
        } else if(nData1 == OMX_ErrorResourcesPreempted) {
            preempted=1;

            writeValue = 0;  
            write(Event_Pipe[1], &writeValue, sizeof(OMX_U8));
        }
          
        break;
    case OMX_EventMax:

        break;
    case OMX_EventMark:

        break;

    case OMX_EventBufferFlag:
        APP_DPRINT ( "%d :: iLBCDecTest.c  Event: OMX_EventBufferFlag. Component State Changed To %s. Target %s\n", __LINE__, strState, strTarget);
        if((nData2 == (OMX_U32)OMX_BUFFERFLAG_EOS) && (nData1 == (OMX_U32)NULL)){
            printf("Buffer flag received\n");
            playcompleted = 1;
        }
        /* <<<<< Process Manager <<<<< */
        writeValue = 2;  
        write(Event_Pipe[1], &writeValue, sizeof(OMX_U8));
        /* >>>>>>>>>> */
        break;
        
    default:

        break;
    }
    return eError;
}

/***********
 *
 */
void FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
    write(OpBuf_Pipe[1], &pBuffer, sizeof(pBuffer));
#ifdef OMX_GETTIME
    if (GT_FlagF == 1 ) /* First Buffer Reply*/  /* 1 = First Buffer,  0 = Not First Buffer  */
    {
        GT_END("Call to FillBufferDone  <First: FillBufferDone>");
        GT_FlagF = 0 ;   /* 1 = First Buffer,  0 = Not First Buffer  */
    }
#endif
}

/*********
 *
 */
void EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
    if (!preempted)
        write(IpBuf_Pipe[1], &pBuffer, sizeof(pBuffer));
    
#ifdef OMX_GETTIME
    if (GT_FlagE == 1 ) /* First Buffer Reply*/  /* 1 = First Buffer,  0 = Not First Buffer  */
    {
      GT_END("Call to EmptyBufferDone <First: EmptyBufferDone>");
      GT_FlagE = 0;   /* 1 = First Buffer,  0 = Not First Buffer  */
    }
#endif
}

/*******
 *
 */
OMX_ERRORTYPE send_input_buffer(OMX_HANDLETYPE pHandle, OMX_BUFFERHEADERTYPE* pBuffer, FILE *fIn, int nInBufSize, OMX_S16 testCaseNo)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;

    int numRead = 0;
    static OMX_S16 alternate = 0;
    
    if (testCaseNo == 7) { /* Multiframe with each buffer size = 2* framelength */
        numRead = 2* nInBufSize; /*STD_iLBCDEC_BUF_SIZE;*/
    }
    else if (testCaseNo == 8) { /* Multiframe with each buffer size = 2/framelength */
        numRead = nInBufSize/2;
    }
    else if (testCaseNo == 9) { /* Multiframe with alternating buffer size */
        if (alternate == 0) {
            numRead = 2*nInBufSize; /*STD_iLBCDEC_BUF_SIZE;*/
            alternate = 1;
        }
        else {
            numRead =  nInBufSize/2;
            alternate = 0;
        }
    } 
    else {
        numRead = nInBufSize; 
    }
   
    int nRead = fread(pBuffer->pBuffer, 1, numRead , fIn);
    pBuffer->nFilledLen = nRead;
    
    if( nRead<numRead)
        pBuffer->nFlags = OMX_BUFFERFLAG_EOS;
    else
        pBuffer->nFlags = NORMAL_BUFFER;

    if(pBuffer->nFilledLen != 0 ){
        if (pBuffer->nFlags == OMX_BUFFERFLAG_EOS){
            printf("EOS send on last data buffer\n");
        }
        
        pBuffer->nTimeStamp = rand() % 100;
        error = OMX_EmptyThisBuffer(pHandle, pBuffer);
    } else {
    }

    return error;
}

/***********
 *
 */
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
        fprintf(stderr, "\nError:  WaitForState reports an error %X!!!!!!!\n", error);
        goto EXIT;
    }
/*    playcompleted = 0;*/
 EXIT:
    return error;
}

/**********
 *
 */
OMX_ERRORTYPE PauseComponent(OMX_HANDLETYPE *pHandle)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
#ifdef OMX_GETTIME
    GT_START();
#endif
    error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StatePause, NULL);
    if(error != OMX_ErrorNone) {
        fprintf (stderr,"\nError from SendCommand-Pasue State function!!!!!!\n");
        goto EXIT;
    }
    error = WaitForState(pHandle, OMX_StatePause);
#ifdef OMX_GETTIME
    GT_END("Call to SendCommand <OMX_StatePause>");
#endif
    if(error != OMX_ErrorNone) {
        fprintf(stderr, "\nError:  WaitForState reports an error %X!!!!!!!\n", error);
        goto EXIT;
    }
 EXIT:
    return error;
}

/**********
 *
 */
OMX_ERRORTYPE PlayComponent(OMX_HANDLETYPE pHandle)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
#ifdef OMX_GETTIME
    GT_START();
#endif
    error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateExecuting, NULL);
    if(error != OMX_ErrorNone) {
        fprintf (stderr,"\nError from SendCommand-Executing State function!!!!!!!\n");
        return (error);
    }
    error = WaitForState(pHandle, OMX_StateExecuting);
#ifdef OMX_GETTIME
    GT_END("Call to SendCommand <OMX_StateExecuting>");
#endif
    if(error != OMX_ErrorNone) {
        fprintf(stderr, "\nError:  WaitForState reports an error %X!!!!!!!\n", error);
        return (error);
    }

    return (error);
}

#ifdef APP_DEBUGMEM 
/*********
 *
 */
void * mymalloc(int line, char *s, int size)
{
   void *p = NULL;    
   int e=0;
   p = malloc(size);
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
         printf("Allocating %d bytes on address %p, line %d file %s\n", size, p, line, s);
         return p;
   }
}

/*********
 *
 */
int myfree(void *dp, int line, char *s){
    int q = 0;
    if (dp==NULL){
       printf("Null Memory can not be deleted line: %d  file: %s\n", line, s);
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
     return 1;
}
#endif

/*******
 *
 */
OMX_BOOL checkInputParameters (iNumParams)
{
    /* check the input parameters */
    if((iNumParams < 7) || (iNumParams > 8)){
        puts("General Usage: ./iLBCDecTest [infile] [codec type] [test_case] [DASF/F2F] [RTMIXER (0)] [Num in bufs] [Num out bufs] [DASF freq (optional)]");
        puts("Example :");
        puts( "\tF2F  Usage, primary codec:   ./iLBCDecTest [infile] 0 1 0 0 1 1");
        puts( "\tDASF Usage, secondary codec: ./iLBCDecTest [infile] 1 1 1 0 1 2");
        return (OMX_FALSE);
    }
    return (OMX_TRUE);
}
/*********
 *
 */
OMX_BOOL validateArguments (int nArgc, char *sArgv [], char *iLBC_EncodedFile, OMX_U16 codecType, int tcID, int gDasfMode, int iAcousticMode, int nIpBuffs, int nOpBuffs)
{
    /* check the input parameters */
    if (checkInputParameters (nArgc) == OMX_FALSE) {
        return (OMX_FALSE);
    }
    
    APP_DPRINT("------------------------------------------------------\n");
    APP_DPRINT("This is Main Thread In iLBC DECODER Test Application:\n");
    APP_DPRINT("Test Core 1.5 - " __DATE__ ":" __TIME__ "\n");
    APP_DPRINT("------------------------------------------------------\n");

    /* check to see that the input file exists */
    struct stat sb = {0};
    OMX_S16 status = stat(iLBC_EncodedFile, &sb);
    if( status != 0 ) {
        APP_DPRINT( "%d :: iLBCDecTest.c :: Cannot find file %s. (%u)\n",__LINE__, sArgv[1], errno);
        return (OMX_FALSE);
    }
    if (codecType != 0 && codecType != 1) {
        printf ("Wrong codec type, arg 2: %d \n", codecType);
        return (OMX_FALSE);
    }
    if (tcID < 1 && tcID > 6) {    
        printf ("Wrong test case, arg 3: %d \n", tcID);
        return (OMX_FALSE);
    }
    if (gDasfMode < 0 && gDasfMode > 1) {
        printf ("Wrong dasf, arg 4: %d \n", gDasfMode);
        return (OMX_FALSE);        
    }
    if (iAcousticMode < 0 && iAcousticMode > 1) {
        printf ("Wrong acoustic mode, arg 5: %d \n", iAcousticMode);
        return (OMX_FALSE);                
    }
    if (nIpBuffs < 1 && nIpBuffs > 4) {
        printf ("Wrong Input Buffers number, arg 6: %d \n", nIpBuffs);
        return (OMX_FALSE);                     
    }
    if (nOpBuffs < 1 && nOpBuffs > 4) {
        printf ("Wrong Output Buffers number, arg 7: %d \n", nOpBuffs);
        return (OMX_FALSE);                     
    }
    
    printTestCaseInfo (tcID);
    return (OMX_TRUE);
}
/*
 *
 */
OMX_ERRORTYPE omxSetInputPortParameter (OMX_HANDLETYPE *pHandle, int nIpBuffs, int nIpBufSize)
{
    OMX_PARAM_PORTDEFINITIONTYPE pCompPrivateStruct;
    OMX_ERRORTYPE error = OMX_ErrorNone;
/**/    
    pCompPrivateStruct.nSize = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
    pCompPrivateStruct.nPortIndex = OMX_DirInput; 
    error = OMX_GetParameter (pHandle, OMX_IndexParamPortDefinition, &pCompPrivateStruct);
    if (error != OMX_ErrorNone) {
        return (error);
    }

    pCompPrivateStruct.nBufferCountActual = nIpBuffs; 
    pCompPrivateStruct.nBufferSize = nIpBufSize; 

#ifdef OMX_GETTIME
    GT_START();
    error = OMX_SetParameter (pHandle, OMX_IndexParamPortDefinition, &pCompPrivateStruct);
    GT_END("Set Parameter Test-SetParameter");
#else
    error = OMX_SetParameter (pHandle, OMX_IndexParamPortDefinition, &pCompPrivateStruct);
#endif
/**/    
    if(error != OMX_ErrorNone) {
        /*error = OMX_ErrorBadParameter;*/
        printf ("%d :: OMX_ErrorBadParameter\n",__LINE__);
    }
  
    return (error);
}
/*
 *
 */
OMX_ERRORTYPE omxSetOutputPortParameter (OMX_HANDLETYPE *pHandle, int nOpBuffs, int nOpBufSize, int gDasfMode)
{
    OMX_PARAM_PORTDEFINITIONTYPE pCompPrivateStruct;
    OMX_ERRORTYPE error = OMX_ErrorNone;

    pCompPrivateStruct.nSize = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
    pCompPrivateStruct.nPortIndex = OMX_DirOutput;           
            
    error = OMX_GetParameter (pHandle,OMX_IndexParamPortDefinition, &pCompPrivateStruct);
    if (error != OMX_ErrorNone) {
        return (error);
    }
    /* Send output port config */
    pCompPrivateStruct.nBufferSize = nOpBufSize;
    pCompPrivateStruct.nBufferCountActual     = (gDasfMode == 1)? 0 : nOpBuffs;
#ifdef OMX_GETTIME
    GT_START();
    error = OMX_SetParameter (pHandle,OMX_IndexParamPortDefinition, &pCompPrivateStruct);
    GT_END("Set Parameter Test-SetParameter");
#else
    error = OMX_SetParameter (pHandle,OMX_IndexParamPortDefinition, &pCompPrivateStruct);
#endif  
    if (error != OMX_ErrorNone) {
        error = OMX_ErrorBadParameter;
        printf ("%d:: OMX_ErrorBadParameter\n",__LINE__);
    }

    return (error);
}
/*
 *
 */
void iLBCSetCodecType (OMX_HANDLETYPE *hComp, OMX_U16 iLBCcodecType)
{
    OMX_COMPONENTTYPE* pHandle= (OMX_COMPONENTTYPE*)hComp;
/**/    
    ((iLBCDEC_COMPONENT_PRIVATE *)pHandle->pComponentPrivate)->iLBCcodecType = iLBCcodecType;    
  
    return;
}
/*
 *
 */
void printTestCaseInfo (int iTestCase)
{
    
    switch (iTestCase) {
    case 1:
        printf ("-------------------------------------\n");
        printf ("Testing Simple PLAY till EOF \n");
        printf ("-------------------------------------\n");
        break;
    case 2:
        printf ("-------------------------------------\n");
        printf ("Testing Stop and Play \n");
        printf ("-------------------------------------\n");
        break;
    case 3:
        printf ("-------------------------------------\n");
        printf ("Testing PAUSE & RESUME Command\n");
        printf ("-------------------------------------\n");
        break;
    case 4:
        printf ("---------------------------------------------\n");
        printf ("Testing STOP Command by Stopping In-Between\n");
        printf ("---------------------------------------------\n");
        break;
    case 5:
        printf ("-------------------------------------------------\n");
        printf ("Testing Repeated PLAY without Deleting Component\n");
        printf ("-------------------------------------------------\n");
        break;
    case 6:
        printf ("------------------------------------------------\n");
        printf ("Testing Repeated PLAY with Deleting Component\n");
        printf ("------------------------------------------------\n");
        break;
    case 7:
        printf ("----------------------------------------------------------\n");
        printf ("Testing Multiframe with each buffer size = 2 x frameLength\n");
        printf ("----------------------------------------------------------\n");
        break;
    case 8:
        printf ("------------------------------------------------------------\n");
        printf ("Testing Multiframe with each buffer size = 1/2 x frameLength\n");
        printf ("------------------------------------------------------------\n");
        break;
    case 9:
        printf ("------------------------------------------------------------\n");
        printf ("Testing Multiframe with alternating buffer sizes\n");
        printf ("------------------------------------------------------------\n");
        break;
    case 10:
        printf ("------------------------------------------------------------\n");
        printf ("Testing Mute/Unmute for Playback Stream\n");
        printf ("------------------------------------------------------------\n");
        break;
    case 11:
        printf ("------------------------------------------------------------\n");
        printf ("Testing Set Volume for Playback Stream\n");
        printf ("------------------------------------------------------------\n");
        break;

    }
    
    return;
}
/**
 *
 */
OMX_ERRORTYPE omxSetConfigMute (OMX_HANDLETYPE *pHandle, OMX_BOOL bMute)
{
    OMX_AUDIO_CONFIG_MUTETYPE    pCompPrivateStructMute;
    OMX_ERRORTYPE error = OMX_ErrorNone;
    
    /* default setting for Mute/Unmute */
    pCompPrivateStructMute.nSize                    = sizeof (OMX_AUDIO_CONFIG_MUTETYPE);
    pCompPrivateStructMute.nVersion.s.nVersionMajor = 0xF1;
    pCompPrivateStructMute.nVersion.s.nVersionMinor = 0xF2;
    pCompPrivateStructMute.nPortIndex               = OMX_DirInput;
    pCompPrivateStructMute.bMute                    = bMute;

    error = OMX_SetConfig(*pHandle, OMX_IndexConfigAudioMute, &pCompPrivateStructMute);
    if (error != OMX_ErrorNone) {
        error = OMX_ErrorBadParameter;
    }
    return (error);
}
/**
 *
 */
OMX_ERRORTYPE omxSetConfigVolume (OMX_HANDLETYPE *pHandle, OMX_S32 nVolume)
{
    OMX_AUDIO_CONFIG_VOLUMETYPE  pCompPrivateStructVolume;
    OMX_ERRORTYPE error = OMX_ErrorNone;
    
    /* default setting for volume */
    pCompPrivateStructVolume.nSize                    = sizeof(OMX_AUDIO_CONFIG_VOLUMETYPE);
    pCompPrivateStructVolume.nVersion.s.nVersionMajor = 0xF1;
    pCompPrivateStructVolume.nVersion.s.nVersionMinor = 0xF2;
    pCompPrivateStructVolume.nPortIndex               = OMX_DirInput;
    pCompPrivateStructVolume.bLinear                  = OMX_FALSE;
    pCompPrivateStructVolume.sVolume.nValue           = nVolume;  /* actual volume */
    pCompPrivateStructVolume.sVolume.nMin             = 0;   /* min volume */
    pCompPrivateStructVolume.sVolume.nMax             = 100; /* max volume */

    error = OMX_SetConfig(*pHandle, OMX_IndexConfigAudioVolume, &pCompPrivateStructVolume);
    if (error != OMX_ErrorNone) {
        error = OMX_ErrorBadParameter;
    }
    return (error);
}
/**
 *
 */
OMX_ERRORTYPE omxUseBuffers (OMX_HANDLETYPE *pHandle, int gDasfMode, int nIpBuffs, int nIpBufSize, OMX_BUFFERHEADERTYPE *pInputBufferHeader [], int nOpBuffs, int nOpBufSize, OMX_BUFFERHEADERTYPE *pOutputBufferHeader [])
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    int i=0;                                                                                    /* 0j0 <<<<<<<<<<< */
       
#ifndef USE_BUFFER
    for(i=0; i < nIpBuffs; i++) {
        APP_DPRINT("%d :: About to call OMX_AllocateBuffer On Input\n",__LINE__);
        error = OMX_AllocateBuffer(*pHandle, &pInputBufferHeader[i], 0, NULL, nIpBufSize);
        APP_DPRINT("%d :: called OMX_AllocateBuffer\n",__LINE__);
        if(error != OMX_ErrorNone) {
            APP_DPRINT("%d :: Error returned by OMX_AllocateBuffer()\n",__LINE__);
            return (error);
        }
    }

    if (gDasfMode == 0) {
        for(i=0; i < nOpBuffs; i++) {
            error = OMX_AllocateBuffer(*pHandle,&pOutputBufferHeader[i],1,NULL,nOpBufSize);
            if(error != OMX_ErrorNone) {
                APP_DPRINT("%d :: Error returned by OMX_AllocateBuffer()\n",__LINE__);
                return (error);
            }
        }
    }
#else
    OMX_U8* pInputBuffer  [MAX_NUM_OF_BUFS] = {NULL};
    OMX_U8* pOutputBuffer [MAX_NUM_OF_BUFS] = {NULL};

    for(i=0; i<nIpBuffs; i++) {
        pInputBuffer[i] = (OMX_U8*)malloc(nIpBufSize);
        APP_DPRINT("%d :: About to call OMX_UseBuffer On Input\n",__LINE__);
        printf("%d :: About to call OMX_UseBuffer On Input\n",__LINE__);
        error = OMX_UseBuffer(*pHandle,&pInputBufferHeader[i],0,NULL,nIpBufSize,pInputBuffer[i]);
        if(error != OMX_ErrorNone) {
            APP_DPRINT("%d :: Error returned by OMX_UseBuffer()\n",__LINE__);
            return (error);
        }
    }

    if (gDasfMode == 0) {
        for(i=0; i<nOpBuffs; i++) {
            pOutputBuffer[i] = malloc (nOpBufSize + 256);
            pOutputBuffer[i] = pOutputBuffer[i] + 128;
            /* allocate output buffer */
            APP_DPRINT("%d :: About to call OMX_UseBuffer On Output\n",__LINE__);
            printf("%d :: About to call OMX_UseBuffer On Output\n",__LINE__);
            error = OMX_UseBuffer(*pHandle,&pOutputBufferHeader[i],1,NULL,nOpBufSize,pOutputBuffer[i]);
            if(error != OMX_ErrorNone) {
                APP_DPRINT("%d :: Error returned by OMX_UseBuffer()\n",__LINE__);
                return (error);
            }
            pOutputBufferHeader[i]->nFilledLen = 0;
        }
    }
#endif
    return (error);
}
/************
 *
 */
OMX_ERRORTYPE iLBCSetInputPort (OMX_HANDLETYPE *pHandle)
{
        OMX_ERRORTYPE error = OMX_ErrorNone;
        
        OMX_AUDIO_PARAM_ILBCTYPE piLBCParam;
        
        APP_MEMPRINT("%d:::[TESTAPPALLOC] %p\n",__LINE__,&piLBCParam);
        piLBCParam.nSize = sizeof (OMX_AUDIO_PARAM_ILBCTYPE);
        piLBCParam.nVersion.s.nVersionMajor = 0xF1;
        piLBCParam.nVersion.s.nVersionMinor = 0xF2;
        piLBCParam.nPortIndex = OMX_DirInput;
        error = OMX_GetParameter (*pHandle,OMX_IndexParamAudioILBC, &piLBCParam); /* <<<< 0j0 Gsm_FR,pGmfrParam);    */
        if (error != OMX_ErrorNone) {
            printf ("%d:: OMX_ErrorBadParameter\n",__LINE__);
            return (error);
        }
        
#ifdef OMX_GETTIME
        GT_START();
        error = OMX_SetParameter (*pHandle, OMX_IndexParamAudioiLBC, &piLBCParam); /* <<<< 0j0 Gsm_FR,pGmfrParam); */
        GT_END("Set Parameter Test-SetParameter");
#else
        error = OMX_SetParameter (*pHandle, OMX_IndexParamAudioILBC, &piLBCParam); /* <<<<< 0j0 Gsm_FR,pGmfrParam); */
#endif
        if (error != OMX_ErrorNone) {
            printf ("%d:: OMX_ErrorBadParameter\n",__LINE__);
            return (OMX_ErrorBadParameter);
        }
        
    return (error);
}
/************
 *
 */
OMX_ERRORTYPE iLBCSetOutputPort (OMX_HANDLETYPE *pHandle, int iSampRate)
{
    OMX_AUDIO_PARAM_PCMMODETYPE  pPcmParam; 
    OMX_ERRORTYPE error = OMX_ErrorNone;
    
    pPcmParam.nPortIndex    = OMX_DirOutput;
    pPcmParam.nChannels     = 1;
    pPcmParam.nSamplingRate = iSampRate; /* iLBCDEC_SAMPLING_FREQUENCY;*/
#ifdef OMX_GETTIME
    GT_START();
    error = OMX_SetParameter (*pHandle, OMX_IndexParamAudioPcm, &pPcmParam);
    GT_END("Set Parameter Test-SetParameter");
#else
    error = OMX_SetParameter (*pHandle, OMX_IndexParamAudioPcm, &pPcmParam);
#endif

    return (error);
}
/**
 *
 */
OMX_ERRORTYPE omxFreeBuffers (OMX_HANDLETYPE *pHandle, int nBuffs, OMX_BUFFERHEADERTYPE *pBufferHeader  [], OMX_U32 nPortIndex)
{
    int i = 0;
    OMX_ERRORTYPE error = OMX_ErrorNone;
    
    for (i=0; i<nBuffs; i++) {
        APP_DPRINT("%d :: App: Freeing %p BufHeader of Port %d\n", __LINE__, pBufferHeader[i], nPortIndex);
        error = OMX_FreeBuffer(*pHandle, nPortIndex, pBufferHeader[i]);
        if((error != OMX_ErrorNone)) {
            APP_DPRINT ("%d:: Error in Free Handle function\n",__LINE__);
            return (error);
        }
    }
    
    return (error);
}
/***
 *
 */
OMX_ERRORTYPE testCases (OMX_HANDLETYPE *pHandle, fd_set *rfds, int tcID, FILE *fIn, FILE *fOut, OMX_S16 *frmCnt, int *totalFilled, struct timeval *tv, int gDasfMode, OMX_BUFFERHEADERTYPE *pInputBufferHeader [], int nInBufSize, OMX_U8 *count, int numInputBuffers, OMX_BUFFERHEADERTYPE *pOutputBufferHeader [],int numOutputBuffers)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    static int tcID_4 = 1;
    OMX_S16 fdmax = maxint (IpBuf_Pipe[0], OpBuf_Pipe[0]);
    fdmax = maxint (fdmax, Event_Pipe [0]);
    FD_ZERO (rfds);
    FD_SET (IpBuf_Pipe[0], rfds);
    FD_SET (OpBuf_Pipe[0], rfds);
    FD_SET (Event_Pipe[0], rfds);
    tv->tv_sec = 1;
    tv->tv_usec = 0;

    int retval = select (fdmax+1, rfds, NULL, NULL, tv);
    if(retval == -1) {
        perror("select()");
        APP_DPRINT ( "%d :: iLBCDecTest.c :: Error \n",__LINE__);
        return (-1);
    }
                
    if (retval == 0)
        (*count)++;
    if (*count >= 3 && state != OMX_StatePause){
        printf("Shutting down Since there is nothing else to send nor read---------- \n");
        StopComponent(pHandle);
    }

    if(FD_ISSET(IpBuf_Pipe[0], rfds)) {

        OMX_BUFFERHEADERTYPE* pBuffer = NULL;
        read(IpBuf_Pipe[0], &pBuffer, sizeof(OMX_BUFFERHEADERTYPE)); /*pBuffer));*/
        (*frmCnt)++;
        pBuffer->nFlags = NORMAL_BUFFER; /*0; */

        if (100 == *frmCnt) {
            if (4== tcID){ /*Stop Tests*/
                printf("Send Stop Command to component ---------- \n");
                StopComponent(pHandle);
            } else if (2== tcID && tcID_4) {
                printf("Send Stop Command to component ---------- \n");
                StopComponent(pHandle);
                tcID_4 = 0;
            }
        }
        if (state == OMX_StateExecuting){
            error = send_input_buffer (pHandle, pBuffer, fIn, nInBufSize, tcID);            
            if (error != OMX_ErrorNone) {
                return (error); 
            } 
        }

        error = testCases_3_10_11 (pHandle, tcID, frmCnt);
    }
/*****************/                
    if( FD_ISSET(OpBuf_Pipe[0], rfds) ) {            
        OMX_BUFFERHEADERTYPE  *pBuf = NULL;
        read(OpBuf_Pipe[0], &pBuf, sizeof(pBuf));
        if(pBuf->nFlags == OMX_BUFFERFLAG_EOS){
            printf("EOS received by App, Stopping the component\n");
            pBuf->nFlags = 0;
            StopComponent(pHandle);
            fseek(fIn, 0, SEEK_SET);
        }
        fwrite(pBuf->pBuffer, 1, pBuf->nFilledLen, fOut);
        fflush(fOut);
        if (state == OMX_StateExecuting) {
            OMX_FillThisBuffer(pHandle, pBuf);
        }
    }
  /**************/
    /*<<<<<<< Process Manager <<<<<<< */
    if ( FD_ISSET (Event_Pipe[0], rfds) ) {
        OMX_U8 pipeContents = 0;
        read(Event_Pipe[0], &pipeContents, sizeof(OMX_U8));

        if (pipeContents == 0) {
            printf("Test app received OMX_ErrorResourcesPreempted\n");
            WaitForState(pHandle,OMX_StateIdle);
            int i;
            for (i=0; i < numInputBuffers; i++) {
                error = OMX_FreeBuffer(pHandle,OMX_DirInput,pInputBufferHeader[i]);
                if( (error != OMX_ErrorNone)) {
                    APP_DPRINT ("%d :: AmrDecTest.c :: Error in Free Handle function\n",__LINE__);
                }
            }

            for (i=0; i < numOutputBuffers; i++) {
                error = OMX_FreeBuffer(pHandle,OMX_DirOutput,pOutputBufferHeader[i]);
                if( (error != OMX_ErrorNone)) {
                    APP_DPRINT ("%d:: Error in Free Handle function\n",__LINE__);
                }
            }

#ifdef USE_BUFFER
            /* newfree the App Allocated Buffers */
            APP_DPRINT("%d :: AmrDecTest.c :: Freeing the App Allocated Buffers in TestApp\n",__LINE__);
            for(i=0; i < numInputBuffers; i++) {
                APP_MEMPRINT("%d::: [TESTAPPFREE] pInputBuffer[%d] = %p\n",__LINE__,i,pInputBuffer[i]);
                if(pInputBuffer[i] != NULL){
                    pInputBuffer[i] = pInputBuffer[i] - 128;
                    newfree(pInputBuffer[i]);
                    pInputBuffer[i] = NULL;
                }
            }
            for(i=0; i < numOutputBuffers; i++) {
                APP_MEMPRINT("%d::: [TESTAPPFREE] pOutputBuffer[%d] = %p\n",__LINE__,i, pOutputBuffer[i]);
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
        } else if (pipeContents == 1) {

            printf("Test app received OMX_ErrorResourcesAcquired\n");

            OMX_SendCommand(pHandle,OMX_CommandStateSet,OMX_StateIdle,NULL);
            int i;
            for (i=0; i < numInputBuffers; i++) {
                /* allocate input buffer */
                error = OMX_AllocateBuffer(pHandle,&pInputBufferHeader[i],0,NULL, iLBCD_INPUT_BUFFER_SIZE*3); /*To have enought space for    */
                if(error != OMX_ErrorNone) {
                    APP_DPRINT("%d :: AmrDecTest.c :: Error returned by OMX_AllocateBuffer()\n",__LINE__);
                }
            }
            WaitForState(pHandle,OMX_StateIdle);
            OMX_SendCommand(pHandle,OMX_CommandStateSet,OMX_StateExecuting,NULL);
            WaitForState(pHandle,OMX_StateExecuting);
            rewind(fIn);
            for (i=0; i < numInputBuffers;i++) {    
/*                  send_input_buffer(pHandle, pInputBufferHeader[i], fIn); 0j0 */
                send_input_buffer (pHandle, pInputBufferHeader[i], fIn, nInBufSize, tcID);
            }                
        }
        if (pipeContents == 2) {

            StopComponent(pHandle);
            int i;
            for (i=0; i < numInputBuffers; i++) {
                error = OMX_FreeBuffer(pHandle,OMX_DirInput,pInputBufferHeader[i]);
                if( (error != OMX_ErrorNone)) {
                    APP_DPRINT ("%d :: AmrDecTest.c :: Error in Free Handle function\n",__LINE__);
                }
            }
            for (i=0; i < numOutputBuffers; i++) {
                error = OMX_FreeBuffer(pHandle,OMX_DirOutput,pOutputBufferHeader[i]);
                if( (error != OMX_ErrorNone)) {
                    APP_DPRINT ("%d:: Error in Free Handle function\n",__LINE__);
                }
            }

#if 1
            error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateLoaded, NULL);
            if(error != OMX_ErrorNone) {
                APP_DPRINT ("%d :: AmrDecTest.c :: Error from SendCommand-Idle State function\n",__LINE__);
                printf("goto EXIT %d\n",__LINE__);

                /*goto EXIT;*/
                return (-1);
            }
            error = WaitForState(pHandle, OMX_StateLoaded);
#ifdef OMX_GETTIME
            GT_END("Call to SendCommand <OMX_StateLoaded>");
#endif
            if(error != OMX_ErrorNone) {
                APP_DPRINT( "%d :: AmrDecTest.c :: Error:  WaitForState reports an error %X\n",__LINE__, error);
                printf("goto EXIT %d\n",__LINE__);
                /*goto EXIT;*/
                return (-1);
            }
#endif
            /*goto SHUTDOWN;*/
            return (-1);
     /**/                               
        } 
    }

    if (playcompleted) {
        if (tcID == 5) {                      
        } else {
/*                        StopComponent(pHandle);*/
        }
        playcompleted = 0;
        *count = 0;
    }
    return (error);
}
/***
 *
 */
void getString_OMX_State (char *ptrString, OMX_STATETYPE state)
{
    switch (state) {
        case OMX_StateInvalid:
            strcpy (ptrString, "OMX_StateInvalid\0");
            break;
        case OMX_StateLoaded:
            strcpy (ptrString, "OMX_StateLoaded\0");
            break;
        case OMX_StateIdle:
            strcpy (ptrString, "OMX_StateIdle\0");
            break;
        case OMX_StateExecuting:
            strcpy (ptrString, "OMX_StateExecuting\0");
            break;
        case OMX_StatePause:
            strcpy (ptrString, "OMX_StatePause\0");
            break;
        case OMX_StateWaitForResources:
            strcpy (ptrString, "OMX_StateWaitForResources\0");
            break;
        case OMX_StateMax:
            strcpy (ptrString, "OMX_StateMax\0");
            break;
        default:
            strcpy (ptrString, "Invalid state\0");
            break;
            
    }
    return;
}
/***
 *
 ***/
OMX_ERRORTYPE sendInputBuffer (OMX_HANDLETYPE *pHandle, fd_set *rfds, int tcID, FILE *fIn, int *frmCount,  struct timeval *tv, int nInBufSize, int *count)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    
    OMX_S16 fdmax = maxint(IpBuf_Pipe[0], OpBuf_Pipe[0]);
    fdmax = maxint (fdmax, Event_Pipe [0]);
    FD_ZERO(rfds);
    FD_SET(IpBuf_Pipe[0], rfds);
    FD_SET(OpBuf_Pipe[0], rfds);
    FD_SET(Event_Pipe[0], rfds);
    tv->tv_sec = 1;
    tv->tv_usec = 0;

    int retval = select(fdmax+1, rfds, NULL, NULL, tv);
    if(retval == -1) {
        perror("select()");
        APP_DPRINT ( "%d :: iLBCDecTest.c :: Error \n",__LINE__);
        return -1;
    }

    if (retval == 0)
        (*count)++;
    if (*count == 3){
        printf("Shutting down Since there is nothing else to send nor read---------- \n");
        StopComponent(pHandle);
    }

    if(FD_ISSET(IpBuf_Pipe[0], rfds)) {

        OMX_BUFFERHEADERTYPE* pBuffer = NULL;
        read(IpBuf_Pipe[0], &pBuffer, sizeof(OMX_BUFFERHEADERTYPE)); /*pBuffer));*/
        frmCount++;
        pBuffer->nFlags = NORMAL_BUFFER; /*0; */

        if (state == OMX_StateExecuting){
            error = send_input_buffer (pHandle, pBuffer, fIn, nInBufSize, tcID);
        }
    }
    return (error);
}
/***
 *
 **/
OMX_ERRORTYPE testCases_3_10_11 (OMX_HANDLETYPE *pHandle, int tcID, OMX_S16 *frmCount)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    
    switch (tcID) {
        case 3: /*Pause Test*/
            
            if(*frmCount == 100) {   /*100 Frames processed */
                printf (" Sending Pause command to Codec \n");
                PauseComponent(pHandle);
                printf("5 secs sleep...\n");
                sleep(5);
                printf (" Sending Resume command to Codec \n");
                PlayComponent(pHandle);
            }               
        break;
        case 10:  /*Mute and UnMuteTest*/
            if(*frmCount == 35){ 
                printf("************Mute the playback stream*****************\n"); 
                error = omxSetConfigMute (pHandle, OMX_TRUE);
            } else if(*frmCount == 120) { 
                printf("************Unmute the playback stream*****************\n"); 
                error = omxSetConfigMute (pHandle, OMX_FALSE);
            }

        break;
        case 11: /*Set Volume Test*/
            if(*frmCount == 35) { 
                printf("************Set stream volume to high*****************\n"); 
                error = omxSetConfigVolume (pHandle, 0x8000);
            }
            if(*frmCount == 120) { 
                printf("************Set stream volume to low*****************\n"); 
                error = omxSetConfigVolume (pHandle, 0x1000);
            }           
        break;
        
        default:
        break;
    }
    return error;
}

/*
 *  Copyright 2001-2010 Texas Instruments - http://www.ti.com/
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
 *  limitations under the License.
 */


/*

Change history:

1. Changed the dspReserveSize to BUFFER_SIZE from BUFFER_SIZE*2
2. mmbufpos is currently not used, for future use.
3. Changed inside InitializeProcessor() DSP_PROCESSORINFO dspInfo to struct DSP_PROCESSORINFO dspInfo [12/11/07]

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <dbapi.h>
#include <DSPProcessor_OEM.h>


#define DEBUG_1

typedef unsigned long  Addr;
typedef unsigned int   Uint;
typedef unsigned short Ushort;
typedef unsigned long  Ulong;
typedef Uint   BSize;
typedef Ushort Status;


typedef struct LOG_Obj
{
    Arg* bufend;
    Uns  flag;
    Uns  seqnum;
    Addr curptr;
    Uns  lenmask;
    Addr bufbeg;
} LOG_Obj;

typedef struct
{
    DSP_HPROCESSOR hProcessor;  /* Handle to processor. */
    DSP_HNODE      hNode;       /* Handle to node. */
} DMMCOPY_TASK;

// INST2 ARM adapter
typedef struct INST2_Arm_Adapter
{
    LgUns   ulMmMask;
    LgUns   ulBiosMask;
    LgUns  *pMpuTimerMsw;
    LgUns  *pMpuTimerLsw;
    LgUns   bUpdated;
    //Added two more values to allocated buffer for logging
    void   *pNewLogBuf;
    LgUns    ulNewLogSize;
} INST2_Arm_Adapter ;

/*NOTE : xxxxDSP or xxxxxARM implies that the address belongs
to DSP address space or ARM address space */
typedef struct LogBufInfo
{
    Addr  logObjStartAddrDSP;  /* the one read from the file & points to the LOG_Obj object start */
    Addr  logObjStartAddrARM;  /* the address as seen by arm after translation for the LOG_Obj */
    Addr  logBufStartAddrDSP;  /* LOG Buf start address and is read from the LOG_Obj */
    Addr  logBufStartAddrARM;  /* the translated one of the LOG Buf start address */
    char *mappedLogObjAddr;    /* mapped address for Log Obj
				after mapping the Arm physical address in the current process memory mapping */
    char *mappedLogBufAddr;    /* mapped address of Log Buf after mapping*/
    Uint  index;               /* index as read from the file*/
    BSize bufSize;             /* bufSize for roll */
    Addr  logBufCurPtrDSP;     /* this points to the place till which we have copied the buffers on to the file*/
    Uint  sequenceNum;         /* sequence number as in LOG_obj buffer */
    Uint  writeCompleteFlag;   /* used only if defined WRITE_LESS, required to write upto the latest record
                        if the logging is no more happening*/
}LogBufInfo;

#define BUFFER_SIZE 			0x1000	/* to map adapter and timers */
#define DEF_SLEEP_TIME 			500000 	/* micro secs */
#define DEF_MM_BUF_POS 			4
#define DEF_MM_BUF_SIZE 		0x10000
#define MAX_NUM_OF_LOG_BUFS     8		/* Max log bufs that utility can extract and log */
#define PAGE_ALIGN_MASK         0xfffff000  /* Align mask for a 0x1000 page */
#define PAGE_ALIGN_UNMASK       0x00000fff  /* Unmask for a 0x1000 page */
#define LOG_BUF_REC_SIZE        0x20	/* size of each logbuf record */
#define MAX_NO_WRITE_COUNT      20
#define MAX_UPDATION_WAIT	20
#define SUCCESS                 0
#define FAILURE                 -1
#define DSP_CACHE_LINE		128
//******************************************//
//		 		MACROS						//
//******************************************//

/*Macro to translate DSP Addr to ARM Addr*/
#define TRANSLATE_DSP_TO_ARM(dspAddr)  ((dspAddr+armPhyMemOff))

/*Macro to translate DSP Addr to Mapped Address */
#define DSP2MAPPED(mappedAddr,equDSPAddr,toBeTranslatedAddr) \
					(mappedAddr + (toBeTranslatedAddr-equDSPAddr))

//Macro to translate DSP Addr to ARM Addr
#define TRANSLATE_UNCACHED(dspAddr) ((unsigned long)dspAddr+(unsigned long)armPhyMemOffUncached)

//Macro to translate DSP Addr to Mapped Address
#define DSP2MAPPED(mappedAddr,equDSPAddr,toBeTranslatedAddr)  (mappedAddr + (toBeTranslatedAddr-equDSPAddr))

//Macro to add addresses in a circular buffer
#define BUF_ADD(bufBeg,curPos,bufSize,addition)  (bufBeg + (((curPos - bufBeg)+addition)%bufSize))

//Macro to sub addresses in a circular buffer
#define BUF_SUB(bufEnd,curPos,bufSize,subtract)  (bufEnd - (((bufEnd - curPos)+subtract)%bufSize))

#define VALIDATE(Addr) ((Addr & 0xF0000000) != 0xF0000000)

#ifdef WRITE_LESS
    #define NUM_OF_REC_LESS 128
    #define WRITE_LESS_IN_BYTES(numOfRecords)   (numOfRecords * LOG_BUF_REC_SIZE)
#endif

//******************************************//
//		 	GLOBAL VARIABLES				//
//******************************************//

unsigned long armPhyMemOffUncached=0;
Addr          MswStart=0;
Addr          LswStart =0;
int           mem_fd; /* this is file descriptor for the device /dev/mem */
volatile INST2_Arm_Adapter *adapterAddrMpu= NULL;
LgUns         ulMmMask = 0;
LgUns         ulBiosMask = 0;
UINT          g_dwDSPWordSize = 1;   /* default for 2430 */
Uint          armPhyMemOff=0;

int           numOfTblEntries =0;
char          logDataFileName[32] = {'\0'};
FILE         *logDataFilePtr = NULL;
const Uint    delimiters[2] = {-1, 0}; /* delimiter after and collect data from
					 all the bufs in the LogBufArr */
Uint          noWriteCount = 0;        /* # of sleep periods after test is considered done if nothing is written */

Addr addrTableStart   = 0;
Addr adapterAddrDsp   = 0;
Addr gpTimerRegAddLSW = 0;
Addr gpTimerRegAddMSW = 0;
Addr callocBuffer     = 0;
Addr mmBuffer         = 0;
Addr dspResMemLSW     = 0;
Addr dspResMemMSW     = 0;
Addr dspMappedLSW     = 0;
Addr dspMappedMSW     = 0;
Addr dspResMemBufUnaligned = 0;
Addr dspResMemBuf     = 0;
Addr dspMappedBuf     = 0;
unsigned long mmBufPos = DEF_MM_BUF_POS;
Uint mmBufSize = DEF_MM_BUF_SIZE;
Uint sleepTime = DEF_SLEEP_TIME;

//-----------------------------------------//

/********************************************
            Functions
*********************************************/
Addr mapAddr(Addr startAddr,Uint size, int prot, int flag);

static int InitializeProcessor(DMMCOPY_TASK* copyTask);

static Status mapTimerAndBufferToDSPMem(DMMCOPY_TASK* copyTask);

static Status unMapTimerAndBufferToDSPMem (DMMCOPY_TASK* copyTask);

void checkIpArgs(int argc, char ** argv);

Status initializeLogBufInfo (LogBufInfo ** logBufInfoArr);

Status writeBufDataToFile (DMMCOPY_TASK *,LogBufInfo **, Ushort );

void cleanUpLogBufInfo(LogBufInfo **, Uint);

Addr mapAddr (Addr startAddr,
              Uint size,
              int prot,
              int flag);

Status writeToFile(DMMCOPY_TASK *,LogBufInfo * logBufInfo,
					Addr startAddr, Uint numOfBytes, int bufIndex);

Uint getPrevSeqNum(LogBufInfo * logBufInfoPtr,
                   Addr curPtrDSP, int bufIndex);


int main(int argc, char ** argv)
{
    int status = 0;
    DMMCOPY_TASK dmmcopyTask;
    dmmcopyTask.hProcessor = NULL;
    dmmcopyTask.hNode = NULL;
    short i=0;
    LogBufInfo* logBufInfoArr[MAX_NUM_OF_LOG_BUFS];
    int updateCounter=0;

    //very minimal input arg check method
    checkIpArgs(argc, argv);

    //opening fd for /dev/mem/ required for mapping.
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd<0)
    {
        fprintf(stderr, "GETLOG_ERR:main:failed to open mem\n");
        perror("open");
        exit(0);
    }

    //getting handle to DSP
    status = DspManager_Open(argc, NULL);

    //doing the initilization and mapping the GPTimers to DSP virtual address space
    if (DSP_SUCCEEDED(status))
    {
        /* Perform processor level initialization.  */
        status = InitializeProcessor(&dmmcopyTask);
        if (DSP_SUCCEEDED(status))
        {
            status=mapTimerAndBufferToDSPMem(&dmmcopyTask);
            if (status==FAILURE)
            {
                fprintf(stderr, "GETLOG_ERR:main:failed to map GP timer to DSP mem\n");
                close(mem_fd);
                unMapTimerAndBufferToDSPMem(&dmmcopyTask);
                exit(0);
            }
        }
        else
        {
            fprintf(stderr, "GETLOG_ERR:main:failed to Initialize Processor mem\n");
            close(mem_fd);
            exit(0);
        }
    }
    else
    {
        fprintf(stderr, "GETLOG_ERR:main:DspManager_Open failed\n");
        close(mem_fd);
        exit(0);
    }

    //Now, the process of retrieving logs on to ARM side
	//Waiting for the updation to happen
    fprintf(stderr,"main: Going in while 1 loop till DSP Updation is complete\n");

    while (1)
    {
        if (adapterAddrMpu->bUpdated==0)
        {
            fprintf(stderr,"main:DSP Updation Done\n");
            updateCounter++;
            break;
        }
        usleep(250000);
    }

    /*Populating the info about log buffers into the sturcture */
    if (initializeLogBufInfo(logBufInfoArr)!=SUCCESS)
    {
        fprintf(stderr,"GETLOG_ERR:main:Failed to initializeLogBufInfo\n");
        unMapTimerAndBufferToDSPMem(&dmmcopyTask);
        close(mem_fd);
        exit(0);
    }

#ifdef DEBUG_1
    for (i=0;i<numOfTblEntries;++i)
    {
        fprintf(stderr,"\n*****************************\n");
        fprintf(stderr,"Log Buffer Info : %d\n",i);
        fprintf(stderr,"*****************************\n");
        fprintf(stderr,"logObjStartAddrDSP : 0x%lx\n",logBufInfoArr[i]->logObjStartAddrDSP);
        fprintf(stderr,"logObjStartAddrARM : 0x%lx\n",logBufInfoArr[i]->logObjStartAddrARM);
        fprintf(stderr,"logBufStartAddrDSP : 0x%lx\n",logBufInfoArr[i]->logBufStartAddrDSP);
        fprintf(stderr,"logBufStartAddrARM : 0x%lx\n",logBufInfoArr[i]->logBufStartAddrARM);
        fprintf(stderr,"logBufCurPtrDSP: 0x%lx\n",logBufInfoArr[i]->logBufCurPtrDSP);
        fprintf(stderr,"SequenceNumber: 0x%x\n",logBufInfoArr[i]->sequenceNum);
        fprintf(stderr,"\n*****************************\n");
    }
#endif


    /*now checking & initializing the new log file into which we dump the buffer data
    right now using "w" pointer assuming we write a new file every time and not append
    existing files.*/
    logDataFilePtr = fopen(logDataFileName,"w");
    if (logDataFilePtr==NULL)
    {
        perror("fopen");
        unMapTimerAndBufferToDSPMem(&dmmcopyTask);
        cleanUpLogBufInfo(logBufInfoArr,numOfTblEntries);
        close(mem_fd);
        exit(0);
    }

    fprintf(stderr,"main:Going into while 1 loop for writing buf datat to file\n");

    while (1)
    {
        // Write LogBuffer Data into the log file
	if (writeBufDataToFile(&dmmcopyTask,logBufInfoArr,numOfTblEntries)!=SUCCESS)
        {
            unMapTimerAndBufferToDSPMem(&dmmcopyTask);
            cleanUpLogBufInfo(logBufInfoArr,numOfTblEntries);
            close(mem_fd);
            exit(0);
        }

        usleep(sleepTime);
    }

    //unMapTimerAndBufferToDSPMem(&dmmcopyTask);
    //cleanUpLogBufInfo(logBufInfoArr,numOfTblEntries);

    return(0);
}



void checkIpArgs(int argc, char ** argv)
{

    if (argc < 11)
    {
        fprintf(stderr,"Invalid Arguments \n \
			Arg 1: armPhyMemOffUnchached [Hex] \n \
			Arg 2: adapaterAddrDsp [Hex] \n \
			Arg 3: ulMmMask [Hex] \n \
			Arg 4: ulBiosMask[Hex] \n \
			Arg 5: gpTimerRegAddLSW [Hex] \n \
			Arg 6: gpTImerRegAddMSW [Hex] \n \
			Arg 7: armPhyMemOff [Hex] \n \
			Arg 8: addrTableStart [Hex] \n \
			Arg 9: numOfTblEntries \n \
			Arg 10: logDataFileName \n \
			Arg 11: mmBufSize [Hex] [Optional]  [DEFAULT = 0x8000] \n \
			Arg 12: mmBufPos [Optional] [DEFAULT = 4] \n \
			Arg 13: sleepTime (ms) [Optional] [DEFAULT = 500 ms] \n");
	printf("For e.g : InstUtility ........\n");

        exit (-1);
    }
    else
    {
        sscanf(argv[1],"%lx",&armPhyMemOffUncached);
        sscanf(argv[2],"%lx",&adapterAddrDsp);
        sscanf(argv[3],"%lx",&ulMmMask);
        sscanf(argv[4],"%lx",&ulBiosMask);
        sscanf(argv[5],"%lx",&gpTimerRegAddLSW);
        sscanf(argv[6],"%lx",&gpTimerRegAddMSW);
        sscanf(argv[7],"%x", &armPhyMemOff);
        sscanf(argv[8],"%lx",&addrTableStart);
        sscanf(argv[9],"%d", &numOfTblEntries);
        if (strlen(argv[10])>31)
        {
            strncpy(&logDataFileName[0],argv[10],31);
            logDataFileName[31]='\0';
        }
        else
        {
		strcpy(&logDataFileName[0],argv[10]);
        }
    }

    if (argc >= 12)
    {
	sscanf(argv[11], "%x", &mmBufSize);
    }

    if (argc >= 13)
    {
	sscanf(argv[12], "%lu", &mmBufPos);
    }

    if (argc >= 14)
    {
	sscanf(argv[13], "%d", &sleepTime);
    }

    fprintf(stderr,"	\n \
       Arg 1: armPhyMemOffUnchached = 0x%lx\n  \
	Arg 2: adapaterAddrDsp=0x%lx \n  \
	Arg 3: ulMmMask= 0x%lx \n  \
	Arg 4: ulBiosMask= 0x%lx \n \
	Arg 5: gpTimerRegAddLSW = 0x%lx \n \
	Arg 6: gpTImerRegAddMSW = 0x%lx \n \
	Arg 7: armPhyMemOff = 0x%x \n \
	Arg 8: addrTableStart = 0x%lx \n \
	Arg 9: numOfTblEntries= %d \n \
	Arg 10: logDataFileName = %s \n \
	Arg 11: mmBufSize  = 0x%x bytes\n \
	Arg 12: mmBufPos = %lu \n \
	Arg 13: sleepTime = %u ms \n", \
            armPhyMemOffUncached,adapterAddrDsp,ulMmMask,ulBiosMask,gpTimerRegAddLSW,gpTimerRegAddMSW,armPhyMemOff, \
            addrTableStart,numOfTblEntries,logDataFileName,mmBufSize,mmBufPos,sleepTime);

    if (mmBufSize & (mmBufSize - 1))
    {
        fprintf(stderr, "mmBufSize must be a power of 2\n");
        exit (-1);
    }

    return;
}

/////////////////////////////////////////////////////////////////////////////
//Function: mapTimerAndBufferToDSPMem
//Descrption : Reserve and Map GPTimer LSW/MSW and memory buffer
//Args : @copyTask  - Physical address
//Return :  SUCCESS/FAILURE
/////////////////////////////////////////////////////////////////////////////
static Status mapTimerAndBufferToDSPMem(DMMCOPY_TASK * copyTask)
{

    int status = 0;
    ULONG dspReserveSize = BUFFER_SIZE;
    callocBuffer =(Addr) (char *)calloc(mmBufSize + ((PAGE_ALIGN_UNMASK + 1) << 1),1);

    if (callocBuffer == (Addr) NULL)
        return(FAILURE);

    mmBuffer = (callocBuffer + PAGE_ALIGN_UNMASK) & PAGE_ALIGN_MASK;

    status = DSPProcessor_FlushMemory(copyTask->hProcessor,(PVOID)mmBuffer,mmBufSize,0);
    if (!DSP_SUCCEEDED(status))
    {
        fprintf(stdout, "DSPProcessor_FlushMemory (mmBuffer) failed. Status = 0x%x\n", (UINT) status);
        return(FAILURE);
    }
    fprintf(stderr, "Log Multimedia Buffer address (malloced) 0x%lx\n",mmBuffer);


    // Aligning to Page size of next multiple of mmBufSize
    dspReserveSize = mmBufSize << 1;

    status = DSPProcessor_ReserveMemory(copyTask->hProcessor,dspReserveSize,(PVOID *) & dspResMemBufUnaligned);

    // Aligning to Page size of next multiple of mmBufSize
    dspResMemBuf = (dspResMemBufUnaligned + (mmBufSize - 1)) & ~(mmBufSize - 1);

    if (DSP_SUCCEEDED(status))
    {
        fprintf(stderr,"DSPProcessor_ReserveMemory succeeded. dspResMemBuf = 0x%x \n", (UINT) dspResMemBuf);

        status = DSPProcessor_Map(copyTask->hProcessor, (PVOID) mmBuffer, mmBufSize, (PVOID) dspResMemBuf,(PVOID *) & dspMappedBuf, DSP_MAPELEMSIZE32);
        if (DSP_SUCCEEDED(status))
        {
            fprintf(stderr,"DSPProcessor_Map succeeded. dspMappedBuf = 0x%x \n", (UINT) dspMappedBuf);
        }
        else
        {
            fprintf(stdout, "DSPProcessor_Map failed. Status = 0x%x\n", (UINT) status);
            return(FAILURE);
        }
    }
    else
    {
        fprintf(stdout, "DSPProcessor_ReserveMemory failed. Status = 0x%x\n", (UINT) status);
        return(FAILURE);
    }


    // Need to resize the dspReserveSize to 0x1000
	 dspReserveSize = BUFFER_SIZE;

    status = DSPProcessor_ReserveMemory(copyTask->hProcessor,dspReserveSize,(PVOID *) & dspResMemLSW);

    if (DSP_SUCCEEDED(status))
    {
        fprintf(stderr,"DSPProcessor_ReserveMemory succeeded. dspResMemLSW = 0x%x \n", (UINT) dspResMemLSW);

        status = DSPProcessor_Map(copyTask->hProcessor, (PVOID) gpTimerRegAddLSW, 4, (PVOID) dspResMemLSW, (PVOID *) & dspMappedLSW, 0x4000|DSP_MAPPHYSICALADDR|DSP_MAPELEMSIZE32);
        if (DSP_SUCCEEDED(status))
        {
            fprintf(stderr,"DSPProcessor_Map succeeded. dspMappedLSW = 0x%x \n", (UINT) dspMappedLSW);
        }
        else
        {
            fprintf(stdout, "DSPProcessor_Map failed. Status = 0x%x\n", (UINT) status);
            return(FAILURE);
        }
    }
    else
    {
        fprintf(stdout, "DSPProcessor_ReserveMemory failed. Status = 0x%x\n", (UINT) status);
        return(FAILURE);
    }


    status = DSPProcessor_ReserveMemory(copyTask->hProcessor,dspReserveSize,(PVOID *) & dspResMemMSW);

    if (DSP_SUCCEEDED(status))
    {
        fprintf(stderr,"DSPProcessor_ReserveMemory succeeded. dspResMemMSW = 0x%x \n", (UINT) dspResMemMSW);

        status = DSPProcessor_Map(copyTask->hProcessor, (PVOID) gpTimerRegAddMSW, 4, (PVOID) dspResMemMSW, (PVOID *) & dspMappedMSW, 0x4000|DSP_MAPPHYSICALADDR|DSP_MAPELEMSIZE32);
        if (DSP_SUCCEEDED(status))
        {
            fprintf(stderr,"DSPProcessor_Map succeeded. dspMappedMSW = 0x%x \n", (UINT) dspMappedMSW);
        }
        else
        {
            fprintf(stdout, "DSPProcessor_Map failed. Status = 0x%x\n", (UINT) status);
            return(FAILURE);
        }
    }
    else
    {
        fprintf(stdout, "DSPProcessor_ReserveMemory failed. Status = 0x%x\n", (UINT) status);
        return(FAILURE);
    }

    fprintf(stderr,"dspResMemBuf: 0x%lx,dspMappedBuf: 0x%lx\n",dspResMemBuf,dspMappedBuf);
    fprintf(stderr,"dspResMemLSW: 0x%lx,dspMappedLSW: 0x%lx\n",dspResMemLSW,dspMappedLSW);
    fprintf(stderr,"dspResMemMSW: 0x%lx,dspMappedMSW: 0x%lx\n",dspResMemMSW,dspMappedMSW);

    adapterAddrMpu = (INST2_Arm_Adapter *) mapAddr(TRANSLATE_UNCACHED(adapterAddrDsp),BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED);

    if (adapterAddrMpu > 0)
    {
        fprintf(stdout, "Before: pMpuTimerMsw 0x%lx, pMpuTimerLsw 0x%lx, ulMmMask 0x%lx, bUpdated 0x%lx\n", (Addr) adapterAddrMpu->pMpuTimerMsw, (Addr) adapterAddrMpu->pMpuTimerLsw, adapterAddrMpu->ulMmMask, adapterAddrMpu->bUpdated);
        fprintf(stdout, "Setting: pMpuTimerMsw 0x%lx, pMpuTimerLsw 0x%lx, ulMmMask 0x%lx\n", dspMappedMSW, dspMappedLSW, ulMmMask);
        adapterAddrMpu->pMpuTimerLsw = (LgUns *) dspMappedLSW;
        adapterAddrMpu->pMpuTimerMsw = (LgUns *) dspMappedMSW;
        adapterAddrMpu->ulMmMask = ulMmMask;
        adapterAddrMpu->ulBiosMask = ulBiosMask;
        adapterAddrMpu->bUpdated = 1;
        adapterAddrMpu->pNewLogBuf = (void *)dspMappedBuf;
        adapterAddrMpu->ulNewLogSize = mmBufSize;
        fprintf(stdout, "After:  pMpuTimerMsw 0x%lx, pMpuTimerLsw 0x%lx, ulMmMask 0x%lx, bUpdated 0x%lx\n", (Addr) adapterAddrMpu->pMpuTimerMsw, (Addr) adapterAddrMpu->pMpuTimerLsw, adapterAddrMpu->ulMmMask, adapterAddrMpu->bUpdated);
    }
    else
    {
        fprintf(stderr, "Failed to map adapterAddrDsp");
        return(FAILURE);
    }
	return(SUCCESS);
}

/////////////////////////////////////////////////////////////////////////////
//Function: InitializeProcessor
//Descrption : intilizing the DSP and getting the handle for DSP
//Args : @copyTask  - Physical address
//Return :  SUCCESS/FAILURE
/////////////////////////////////////////////////////////////////////////////
static int
InitializeProcessor(DMMCOPY_TASK * copyTask)
{
    int status = -EPERM;
    struct DSP_PROCESSORINFO dspInfo;
    UINT numProcs;
    UINT index = 0;
    INT procId = 0;

    /* Attach to DSP */
    while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,
                                                      &dspInfo,
                                                      (UINT)
                                                      sizeof
                                                      (dspInfo),
                                                      &numProcs)))
    {
        if ((dspInfo.uProcessorType == DSPTYPE_55)
            || (dspInfo.uProcessorType == DSPTYPE_64))
        {
            if (dspInfo.uProcessorType == DSPTYPE_55)
            {
                g_dwDSPWordSize = 2;
            }
            else
            {
                g_dwDSPWordSize = 1;
            }

            printf("DSP device detected !! \n");
            procId = index;
            status = 0;
            break;
        }
        index++;
    }

    /* Attach to an available DSP (in this case, the 1st DSP). */
    if (DSP_SUCCEEDED(status))
    {
        status = DSPProcessor_Attach(procId, NULL,
                                     &copyTask->hProcessor);
        if (DSP_SUCCEEDED(status))
        {
            fprintf(stdout, "DSPProcessor_Attach succeeded.\n");
        }
        else
        {
            fprintf(stdout,
                    "DSPProcessor_Attach failed. Status = 0x%x\n",
                    (UINT) status);
        }
    }
    else
    {
	fprintf(stdout, "Failed to get the desired processor \n");
    }
    return(status);
}




/////////////////////////////////////////////////////////////////////////////
//Function: unMapTimerAndBufferToDSPMem
//Descrption : will do unmapping of the Buffer to DSP Mem
//Args : @copyTask  - Physical address
//Return :  SUCCESS/FAILURE
/////////////////////////////////////////////////////////////////////////////

static Status unMapTimerAndBufferToDSPMem(DMMCOPY_TASK* copyTask)
{
    int status = -EPERM;
    //Ushort updationTimerCounter=0;

    if (adapterAddrMpu)
    {
        //setting bUpdated value to 2 so that we will restore original values on the DSP side.
        adapterAddrMpu->ulMmMask = 0;
        adapterAddrMpu->ulBiosMask = 0;
        adapterAddrMpu->pNewLogBuf = 0;
        adapterAddrMpu->ulNewLogSize = 0;
        adapterAddrMpu->pMpuTimerLsw = 0;
        adapterAddrMpu->pMpuTimerMsw = 0;
        adapterAddrMpu->bUpdated=1;
        printf("INST: Log file written. Waiting for INST DSP side cleanup\n");
        while (adapterAddrMpu->bUpdated)
        {
            sleep(1);
        }
        fprintf(stderr,"unMapTimerAndBufferToDSPMem:DSP Log Multimedia Updation Complete. ");
        //unmapping mpu adapter
        munmap((void *)adapterAddrMpu,BUFFER_SIZE);
    }

    if (dspMappedBuf)
    {
        status = DSPProcessor_UnMap(copyTask->hProcessor,(PVOID) dspMappedBuf);
        if (DSP_SUCCEEDED(status))
        {
            fprintf(stdout, "DSPProcessor_UnMap succeeded.\n");
        }
        else
        {
            fprintf(stdout, "DSPProcessor_UnMap failed.Status = 0x%x\n", (UINT) status);
        }
    }

    if (dspResMemBufUnaligned)
        status = DSPProcessor_UnReserveMemory(copyTask->hProcessor,(PVOID) dspResMemBufUnaligned);

    if (callocBuffer)
        free((void *)callocBuffer);


    /* Unmap DSP Recv buffer from MPU receive buffer */
    if (dspMappedLSW)
    {
        status = DSPProcessor_UnMap(copyTask->hProcessor,(PVOID) dspMappedLSW);
        if (DSP_SUCCEEDED(status))
        {
            fprintf(stdout, "DSPProcessor_UnMap succeeded.\n");
        }
        else
        {
            fprintf(stdout, "DSPProcessor_UnMap failed.Status = 0x%x\n", (UINT) status);
        }
    }

    /* Unreserve DSP virtual memory */
    if (dspResMemLSW)
    {
        status = DSPProcessor_UnReserveMemory(copyTask->hProcessor,(PVOID) dspResMemLSW);
        if (DSP_SUCCEEDED(status))
        {
            fprintf(stdout, "DSPProcessor_UnReserveMemory succeeded.\n");
        }
        else
        {
            fprintf(stdout, "DSPProcessor_UnReserveMemory failed. "
                    "Status = 0x%x\n", (UINT) status);
        }
    }


    if (dspMappedMSW)
    {
        /* Upmap DSP Send buffer from MPU receive buffer */
        status = DSPProcessor_UnMap(copyTask->hProcessor,(PVOID) dspMappedMSW);
        if (DSP_SUCCEEDED(status))
        {
            fprintf(stdout, "DSPProcessor_UnMap succeeded.\n");
        }
        else
        {
            fprintf(stdout, "DSPProcessor_UnMap failed. "
                    "Status = 0x%x\n", (UINT) status);
        }
    }

    /* Unreserve DSP virtual memory */
    if (dspResMemMSW)
    {
        status = DSPProcessor_UnReserveMemory(copyTask->hProcessor,(PVOID) dspResMemMSW);
        if (DSP_SUCCEEDED(status))
        {
            fprintf(stdout, "DSPProcessor_UnReserveMemory succeeded.\n");
        }
        else
        {
            fprintf(stdout, "DSPProcessor_UnReserveMemory failed. "
                    "Status = 0x%x\n", (UINT) status);
        }
    }

    printf("INST: DSP Cleanup done. Exiting.\n");
    return(status);
}


////////////////////////////////////////////////////////////////////////////
//Function: initializeLogBufInfo
//Description: Initializes the BufSize value, Maps the Log Object and Log Buf address to proces
//                                  address space using mmap & stores the mapped address in LogBufInfo Array
//@LogBufInfo : Array pointer in which the addresses and index is filled
//Status : SUCCESS/FAILURE
/////////////////////////////////////////////////////////////////////////////

Status initializeLogBufInfo(LogBufInfo ** logBufInfoArr)
{
    Addr tblAddrStartARM        =0;
    Addr mappedAddr     =0;
    int bufCount                =0;
    LOG_Obj * tempLogObj    =NULL;

    fprintf(stderr,"DEBUG_1:initializeLogBufInfo:addrTableStart =%lx\n", addrTableStart);

    if (!VALIDATE(addrTableStart))
    {
        fprintf(stderr,"GETLOG_ERR:initializeLogBufInfo:Invalid addrTableStart =%lx\n",addrTableStart);
        return(FAILURE);
    }

    tblAddrStartARM = TRANSLATE_DSP_TO_ARM(addrTableStart);

    /* map LOG_OBJ table arm address */
    if ((mappedAddr = mapAddr(tblAddrStartARM,
                              numOfTblEntries*sizeof(LOG_Obj),
				PROT_READ,MAP_SHARED))<0)
    {
        fprintf(stderr,"GETLOG_ERR:initializeLogBufInfo:Failed to MapAddr");
        return(FAILURE);
    }

    for (bufCount=0;bufCount<numOfTblEntries;++bufCount)
    {
        /* allocating & initializing for Log Obj Info */
        logBufInfoArr[bufCount]=(LogBufInfo*)malloc (sizeof(LogBufInfo));
        memset(logBufInfoArr[bufCount],0,sizeof(LogBufInfo));

        /* tempLogObj now points to current log object after being mapped.		  */
        tempLogObj = (LOG_Obj *)(mappedAddr+bufCount*sizeof(LOG_Obj));

        /* Updating the Address info */
        logBufInfoArr[bufCount]->mappedLogObjAddr = (char *)tempLogObj;
        logBufInfoArr[bufCount]->logObjStartAddrDSP =(Addr)(addrTableStart + bufCount*sizeof(LOG_Obj));
        logBufInfoArr[bufCount]->logObjStartAddrARM = (Addr)TRANSLATE_DSP_TO_ARM(logBufInfoArr[bufCount]->logObjStartAddrDSP);
        //fprintf(stderr," DEBUG 1 :0x%x\n",logBufInfoArr[bufCount]->logObjStartAddrDSP);
        //fprintf(stderr," DEBUG 1 :0x%x\n",logBufInfoArr[bufCount]->logObjStartAddrARM);

        /* Updaing Index  and Buf Size*/
        logBufInfoArr[bufCount]->index = bufCount;
        logBufInfoArr[bufCount]->bufSize = tempLogObj->lenmask + 1;

        /* The log info buffer start address is extracted from corresponding log obj */
        logBufInfoArr[bufCount]->logBufStartAddrDSP = tempLogObj->bufbeg;

        if (!VALIDATE(logBufInfoArr[bufCount]->logBufStartAddrDSP))
        {
            fprintf(stderr,"GETLOG_ERR:initializeLogBufInfo:Invalid logBufStartAddrDSP = %ld\n",logBufInfoArr[bufCount]->logBufStartAddrDSP);
            cleanUpLogBufInfo(logBufInfoArr,bufCount+1);
            return(FAILURE);
        }

        logBufInfoArr[bufCount]->logBufStartAddrARM = TRANSLATE_DSP_TO_ARM(logBufInfoArr[bufCount]->logBufStartAddrDSP);


        //This is done because now we are using ARM side allocated buffer, and we dont need to map it arm side again
         logBufInfoArr[bufCount]->mappedLogBufAddr = (char *) mmBuffer;
	if (logBufInfoArr[bufCount]->mappedLogObjAddr <0)
        {
            fprintf(stderr,"GETLOG_ERR:initializeLogBufInfo:Failed to MapAddr = %ld",logBufInfoArr[bufCount]->logBufStartAddrARM);
            cleanUpLogBufInfo(logBufInfoArr,bufCount+1);
            return(FAILURE);
        }

        logBufInfoArr[bufCount]->logBufCurPtrDSP=  tempLogObj->curptr;
        logBufInfoArr[bufCount]->sequenceNum = tempLogObj->seqnum;
        logBufInfoArr[bufCount]->writeCompleteFlag =0;

    }
    return(SUCCESS);
}



/////////////////////////////////////////////////////////////////////////////
//Function: initializeMapping
//Description: Initializes the BufSize value, Maps the Log Object and Log Buf address to proces
//                                  address space using mmap & stores the mapped address in LogBufInfo Array
//@LogBufInfo : Array pointer in which the addresses and index is filled
//@numOfBufs : gives the number of bufInfo structures filled in the LogBufInfo Array and is
//                                      filled after parsing the file.
//Status : SUCCESS/FAILURE
/////////////////////////////////////////////////////////////////////////////

Status writeBufDataToFile(DMMCOPY_TASK * copyTask,LogBufInfo ** logbufInfoArrPtr, Ushort numOfLogBufs)
{

    int     i=0;
    Addr logBufEndDSP=0;
    Addr newCurPtr=0;
    Addr startAddr2Write=0;
    Uint  numOfBytes2Write=0;
    Uint  numOfWrites=0;
    Addr curPtr;
    Uint  seqNum=0;
    Uint  newSeqNum=0;
    //Uint  prevRecSeqNum=0;
    LOG_Obj * tempLogObj=NULL;

#ifdef WRITE_LESS
    int diffSeqNum=0;
#endif

    for (i=0;i<numOfLogBufs;++i)
    {

        curPtr = logbufInfoArrPtr[i]->logBufCurPtrDSP;
        logBufEndDSP = logbufInfoArrPtr[i]->logBufStartAddrDSP + logbufInfoArrPtr[i]->bufSize;
	seqNum= logbufInfoArrPtr[i]->sequenceNum;
        tempLogObj = (LOG_Obj *)logbufInfoArrPtr[i]->mappedLogObjAddr;
        newCurPtr = tempLogObj->curptr;
        newSeqNum=tempLogObj->seqnum;
        //prevRecSeqNum = getPrevSeqNum(logbufInfoArrPtr[i], newCurPtr,i);

#ifdef WRITE_LESS
        diffSeqNum = newSeqNum - seqNum;
        if (diffSeqNum > NUM_OF_REC_LESS)
        {
            newCurPtr = BUF_SUB(logBufEndDSP-1,newCurPtr, logbufInfoArrPtr[i]->bufSize,WRITE_LESS_IN_BYTES(NUM_OF_REC_LESS));
            newSeqNum = newSeqNum-NUM_OF_REC_LESS;
            fprintf(stderr,"DEBUG_1:writeBufDataToFile:Writing Less Num of Records:%d\n",NUM_OF_REC_LESS);
        }
        else
        {
            logbufInfoArrPtr[i]->writeCompleteFlag++;
            if (logbufInfoArrPtr[i]->writeCompleteFlag<=3)
            {
                fprintf(stderr,"DEBUG_1:writeBufDataToFile:Not Writing to file-FlagCount:%d\n",logbufInfoArrPtr[i]->writeCompleteFlag);
                continue;
            }
        }
#endif


        if (newSeqNum<=seqNum)
        {
            numOfBytes2Write=0;
            continue;
        }
        else
        {
            /* it implies that the newSeqNum > current seqNum*/
            if (newCurPtr==curPtr)
            {
                numOfBytes2Write= logbufInfoArrPtr[i]->bufSize;
            }
            else if (newCurPtr < curPtr)
            {
                numOfBytes2Write= (logBufEndDSP - curPtr) + (newCurPtr- logbufInfoArrPtr[i]->logBufStartAddrDSP);
            }
            else
            { /*newCurPtr >curPtr */
                numOfBytes2Write = newCurPtr - curPtr;
            }
        }

        startAddr2Write = DSP2MAPPED((Addr)logbufInfoArrPtr[i]->mappedLogBufAddr,logbufInfoArrPtr[i]->logBufStartAddrDSP,curPtr);



        //
#ifdef DEBUG_1
        fprintf(stderr,"DEBUG_1:writeBufDataToFile:CurPtr:\t0x%lx\n",curPtr);
        fprintf(stderr,"DEBUG_1:writeBufDataToFile:newCurPtr:\t0x%lx\n",newCurPtr);
        fprintf(stderr,"DEBUG_1:writeBufDataToFile:SeqNum Written:\t0x%x\n",seqNum);
        fprintf(stderr,"DEBUG_1:writeBufDataToFile:StartAddr2Write:\t0x%lx\n",startAddr2Write);
        fprintf(stderr,"DEBUG_1:writeBufDataToFile:NumOfBytes:\t0x%x\n",numOfBytes2Write);
        fprintf(stderr,"---------------------------------------------------------\n");
#endif

        if (numOfBytes2Write)
        {

            if (writeToFile(copyTask,logbufInfoArrPtr[i],startAddr2Write,numOfBytes2Write,i)!=SUCCESS)
                return(FAILURE);
            numOfWrites++;
            logbufInfoArrPtr[i]->writeCompleteFlag=0;
        }

        //Updating the  values for the new ones
        logbufInfoArrPtr[i]->sequenceNum = newSeqNum;
        logbufInfoArrPtr[i]->logBufCurPtrDSP = newCurPtr;

    }


    if (numOfWrites)
    {
        noWriteCount=0;
        fflush(logDataFilePtr);
	}
    else
        noWriteCount++;


    if (noWriteCount==MAX_NO_WRITE_COUNT)
    {
        fprintf(stderr,"DEBUG_1:MAX_NO_WRITE_COUNT reached,Closing file\n");
        fwrite(&delimiters, 2 * sizeof(Uint), 1, logDataFilePtr);
        fclose(logDataFilePtr);
        return(FAILURE);
    }

    return(SUCCESS);

}

////////////////////////////////////////////////////////////////////////
//Function:writeToFile
//Descrption : This function will write to file the buffer contents as specified by
//start  addr & number of bytes
//@logBufInfo: Buffer info
//@startAddr : start Addr of the buffer from which the conents are written to file
//@numOfBytes: number of byter to be written
//Return : SUCESS/FAILURE
////////////////////////////////////////////////////////////////////////

Status writeToFile(DMMCOPY_TASK * copyTask,LogBufInfo * logBufInfo, Addr startAddr, Uint numOfBytes,int bufIndex)
{

    int retVal=0;
    Addr endOfBufAddr =0;
    Uint indexNumBytes[2]={0,0};
    Uint  tempSize=0;
    int status = 0;

    if (logDataFilePtr==NULL)
    {
        fprintf(stderr,"GETLOG_ERR:writeToFile:Null File Ptr found:%s", logDataFileName);
        return(FAILURE);
	}

    endOfBufAddr = (Addr)(logBufInfo->mappedLogBufAddr + logBufInfo->bufSize);

    /* putting the index number and numOfBytes to be written into one array to in
    order to fwrite them at one shot */
    indexNumBytes[0] =logBufInfo->index;
    indexNumBytes[1]= numOfBytes;

    /* writing index and number of bytes */
    retVal=fwrite(&indexNumBytes[0],sizeof(Uint),2,logDataFilePtr);
    if (retVal!=2)
    {
        fprintf(stderr,"GETLOG_ERR:writeTofile:Error in writing index&numofBytes\n");
        return(FAILURE);
	}

    /* this condition checks if the circular buffer roll over  has happend or not */
    if (startAddr + numOfBytes > endOfBufAddr)
    {
        tempSize = endOfBufAddr - startAddr;
        retVal = fwrite((void *)startAddr,tempSize,1,logDataFilePtr);

        fprintf(stderr,"DEBUG_1:writeTofile:startAddr-1:\t0x%lx\n",startAddr);
        if (retVal<1)
        {
            fprintf(stderr,"GETLOG_ERR:writeTofile:Overflow_1:Failed to Write Buf Data\n");
            return(FAILURE);
        }

        fflush(logDataFilePtr);

        status = DSPProcessor_FlushMemory(copyTask->hProcessor,(PVOID) startAddr ,tempSize,0);
        if (!DSP_SUCCEEDED(status))
        {
            fprintf(stdout, "DSPProcessor_FlushMemory (log buffer) failed. Status = 0x%x\n", (UINT) status);
            return(FAILURE);
        }

        tempSize = numOfBytes -tempSize;
        /*
        if(bufIndex==3)
        {
            retVal = fwrite((void *)mmBuffer,tempSize,1,logDataFilePtr);
            if(retVal<1)
            {
                fprintf(stderr,"GETLOG_ERR:writeTofile:Overflow_2:Failed to Write Buf Data\n");
                return FAILURE;
            }
            fprintf(stderr,"DEBUG_1:writeTofile:mmBuffer:\t0x%x\n",mmBuffer);
            status = DSPProcessor_FlushMemory(copyTask->hProcessor,(void *)mmBuffer,tempSize,0);
             if (!DSP_SUCCEEDED(status))
             {
                fprintf(stdout, "DSPProcessor_FlushMemory (log buffer) failed. Status = 0x%x\n", (UINT) status);
                return FAILURE;
             }
        }
        */
	retVal = fwrite((void *)logBufInfo->mappedLogBufAddr,tempSize,1,logDataFilePtr);
        if (retVal<1)
        {
            fprintf(stderr,"GETLOG_ERR:writeTofile:Overflow_2:Failed to Write Buf Data\n");
            return(FAILURE);
        }
        fprintf(stderr,"DEBUG_1:writeTofile:logBufInfo->mappedLogBufAddr:\t0x%lx\n",(Addr) logBufInfo->mappedLogBufAddr);
        status = DSPProcessor_FlushMemory(copyTask->hProcessor,logBufInfo->mappedLogBufAddr,tempSize,0);
        if (!DSP_SUCCEEDED(status))
        {
            fprintf(stdout, "DSPProcessor_FlushMemory (log buffer) failed. Status = 0x%x\n", (UINT) status);
            return(FAILURE);
        }


        fflush(logDataFilePtr);

    }
    else
    {
        retVal = fwrite((void *)startAddr, numOfBytes,1,logDataFilePtr);
        if (retVal<1)
        {
            fprintf(stderr,"GETLOG_ERR:writeTofile:Normal:Failed to Write Buf Data\n");
            perror("fwrite");
            return(FAILURE);
        }

        fprintf(stderr,"DEBUG_1:writeTofile:startAddr-only:\t0x%lx\n",startAddr);

        fflush(logDataFilePtr);

        status = DSPProcessor_FlushMemory(copyTask->hProcessor,(void *)startAddr,numOfBytes,0);
        if (!DSP_SUCCEEDED(status))
        {
            fprintf(stdout, "DSPProcessor_FlushMemory (log buffer) failed. Status = 0x%x\n", (UINT) status);
            return(FAILURE);
        }

    }

    return(SUCCESS);
}

/////////////////////////////////////////////////////////////////////////////
//Function:getPrevSeqNum
//Descrption : Gets the Prev Records Sequence Number
//Args : @logBufInfoPtr- LogBufInfo
//@curPtrDSP - current Buffer Pointer (DSP) as read from the log Obj
//Return : Corresponding ARM address
/////////////////////////////////////////////////////////////////////////////

Uint getPrevSeqNum(LogBufInfo * logBufInfoPtr, Addr curPtrDSP, int bufIndex)
{

    Uint seqNum=0;
    Addr prevBufAddr=0;
    Addr bufEnd=0;

    if (logBufInfoPtr->sequenceNum==0)
        return(seqNum);

    bufEnd = logBufInfoPtr->logBufStartAddrDSP+ logBufInfoPtr->bufSize -1; // - 1 required to position buf end to the last valid address

    /* doing a circular BUF_SUB, this subtraction takes into account the buf overflow  */
    prevBufAddr = BUF_SUB(bufEnd, curPtrDSP, logBufInfoPtr->bufSize, LOG_BUF_REC_SIZE);

    /* converting the DSP address to equivalent Mapped address */
    prevBufAddr = DSP2MAPPED((Addr)logBufInfoPtr->mappedLogBufAddr,logBufInfoPtr->logBufStartAddrDSP,prevBufAddr);


    //Quick Fix
	if (bufIndex==3)
    {
        prevBufAddr = mmBuffer+(curPtrDSP - logBufInfoPtr->logBufStartAddrDSP);
    }

    seqNum = *(Uint*)prevBufAddr;

#ifdef DEBUG_1
    fprintf(stderr,"prevRecSeqNum Num:ox%lx : %0x\n",prevBufAddr,seqNum);
#endif

    return(seqNum);
}

/////////////////////////////////////////////////////////////////////////////
//Function:cleanUp
//Descrption : frees the allocated memory in case of errors and intermediate exits
//Args : @logBufInfoPtr- LogBufInfo
//@numOfLogBufs - current Buffer Pointer (DSP) as read from the log Obj
//Return :  void
/////////////////////////////////////////////////////////////////////////////
void cleanUpLogBufInfo(LogBufInfo ** lbInfo, Uint numOfLogBufs)
{
    int i=0;

    for ( i=0;i<numOfLogBufs;++i)
    {
        free((LogBufInfo*)lbInfo[i]);
    }


}

/////////////////////////////////////////////////////////////////////////////
//Function: mapAddr
//Descrption : will do memory mapping for a given physical address. this is
//                                     wrapper func for the mmap() system call
//Args : @startAddr  - Physical address
//@ prot - memory protection
//@ flag - type of the mapping object like SHARED,PRIVATE
//Return :  void
/////////////////////////////////////////////////////////////////////////////

Addr mapAddr(Addr startAddr,Uint size, int prot, int flag)
{
    Addr mappedAddr=0;
//Uint additionalSize=startAddr &  PAGE_ALIGN_MASK;

    if ((mappedAddr=(Addr) mmap (0,
                                 size,//+additionalSize,
                                 prot,
                                 flag,
                                 mem_fd,
                                 startAddr &  PAGE_ALIGN_MASK))<0)
    {
        fprintf(stderr,"map2Addr:Error in mapping\n");
        perror("mmap");
        return(mappedAddr);
    }

    /* Need to add the PAGE offset that was masked before mapping  */
    mappedAddr+= (startAddr & PAGE_ALIGN_UNMASK);
    /* fprintf(stderr,"Inside mapAddr :0x%x\n",mappedAddr);*/

    return(mappedAddr);
}



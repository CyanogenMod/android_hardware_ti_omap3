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
 *  limitations under the License.
 */

/*
 *  ======== strmcopy.c ========
 *  Description:
 *      Demonstrates zero copy messaging.
 *
 *  Usage:
 *      strmcopy.out
 *
 *
 *  Notes:
 *      Data is sent from an input buffer. The DSP send back the same data 
 *      The host receives the data and compares it to original data.
 *
 */

#include <stdio.h>
#include <dbapi.h>
#include <stdlib.h>
#include <string.h>
/* Default stream attributes */
#define DEFAULTALIGNMENT   0
#define DEFAULTSEGMENT     0
#define DEFAULTTIMEOUT  50000000
#define DEFAULTBUFSIZE  1024	/* Default buffer size in bytes. */
#define DEFAULTBUFSIZESTR  "1024"	/* Default buffer size in bytes. */
#define DEFAULTDSPUNIT  0	/* Default unit number for the board. */
#define DEFAULTDSPEXEC  "./strmcopy_tiomap24xx.dof55L"
#define DEFAULTNBUFS    1	/* Default # of buffers. */

#define ARGSIZE         32

/* strmcopy task context data structure. */
struct STRMCOPY_TASK {
	DSP_HPROCESSOR hProcessor;	/* Handle to processor. */
	struct DSP_UUID hNodeID;	/* Handle to node ID GUID. */
	DSP_HNODE hNode;	/* Handle to node. */
	BYTE *pInBufs;		/* Input stream buffers. */
	BYTE *pOutBufs;		/* Output stream buffers. */
	struct DSP_NOTIFICATION hNotification;	/* DSP notification object. */
	INT nStrmMode;		/* stream mode */
} ;

/*
 *  strmcopy node
 */
/* ZCMSG_TI_UUID = 30DBD781_F3FB_11D5_A8DD_00B0D055F6D1 */
static const struct DSP_UUID nodeuuid = {0x30dbd781, 0xf3fb, 0x11d5, 0xa8, 0xdd,
									{ 0x00, 0xb0, 0xd0, 0x55, 0xf6, 0xd1} };

/* Forward declarations: */

/*static DSP_STATUS ProcessArgs(int argc, char ** argv, FILE ** inFile,
                               FILE ** outFile, INT *pStrmMode);*/

/* Initialization and cleanup routines. */
static DSP_STATUS InitializeProcessor(struct STRMCOPY_TASK *copyTask);
static DSP_STATUS InitializeNode(struct STRMCOPY_TASK *copyTask);
static DSP_STATUS CleanupProcessor(struct STRMCOPY_TASK *copyTask);
static DSP_STATUS CleanupNode(struct STRMCOPY_TASK *copyTask);
static DSP_STATUS RunTask(struct STRMCOPY_TASK *copyTask, FILE *inFile,
																FILE *outFile);
struct DSP_MSG zcopymsgtodsp, zcopymsgfromdsp;
const int KDspBufDesc = 0x20000000;
DSP_STATUS MsgToDsp(struct STRMCOPY_TASK *copyTask);
DSP_STATUS MsgFromDsp(struct STRMCOPY_TASK *copyTask);


DSP_STATUS MsgToDsp(struct STRMCOPY_TASK *copyTask)
{
	int i = 0;
	DSP_STATUS status = DSP_SOK;
	/* BYTE * pSmMsgBuf = copyTask->pOutBufs ; */
	unsigned int *pSmMsgBuf = (unsigned int *)copyTask->pOutBufs;

	fprintf(stdout, "MsgToDsp entry.\n");
	if (DSP_FAILED(status)) {
		fprintf(stdout, "DSPNode_AllocMsgBuf failed.\n");
	}
	zcopymsgtodsp.dwArg1 = (DWORD)pSmMsgBuf;
	zcopymsgtodsp.dwCmd = KDspBufDesc;
	zcopymsgtodsp.dwArg2 = 1024 >> 2;
	for (i = 0;i < (DEFAULTBUFSIZE / sizeof(unsigned int));i++) {
		*pSmMsgBuf++ = i;
	}
	status = DSPNode_PutMessage(copyTask->hNode,&zcopymsgtodsp,DEFAULTTIMEOUT);
	fprintf(stdout, "MsgToDsp exit.\n");
	return status;
}

DSP_STATUS MsgFromDsp(struct STRMCOPY_TASK *copyTask)
{
	int i = 0;
	DSP_STATUS status = DSP_SOK;
	/* BYTE * dataRxAddr = NULL ;
	BYTE * dspDataTx  = NULL ;*/
	unsigned int *dataRxAddr = NULL;
	unsigned int *dspDataTx = NULL;
	int msgSize = 0;

	fprintf(stdout, "MsgFromDsp Entry\n");
	status = DSPNode_GetMessage(copyTask->hNode,&zcopymsgfromdsp,
																DEFAULTTIMEOUT);
	fprintf(stdout, "DSPNode_AllocMsgBuf zcopymsgfromdsp.dwArg1:%ld.\n",
												(DWORD)zcopymsgfromdsp.dwArg1);
	msgSize = zcopymsgfromdsp.dwArg2;
	if ((status != DSP_SOK) || (msgSize == 0) || 
			(zcopymsgfromdsp.dwCmd != (int)KDspBufDesc)) {
			/*  not a zero-copy message*/
		return status;
	}
	/* Validate the received data
	dataRxAddr = (BYTE *)zcopymsgfromdsp.dwArg1 ;
	dspDataTx = (BYTE *)zcopymsgtodsp.dwArg1 ;*/
	dataRxAddr = (unsigned int *)zcopymsgfromdsp.dwArg1;
	dspDataTx = (unsigned int *)zcopymsgtodsp.dwArg1;
	copyTask->pInBufs = (BYTE *)zcopymsgfromdsp.dwArg1;
	for (i = 0;i < (DEFAULTBUFSIZE / sizeof(unsigned int));i++) {
		if (dataRxAddr[i] != (2 * dspDataTx[i])) {
			fprintf(stdout, " Error Sent = %x Received = %x ",
						(unsigned int)dspDataTx[i],(unsigned int)dataRxAddr[i]);
			status = DSP_EFAIL;
			break;
		}
	}
	fprintf(stdout, "MsgFromDsp Entry\n");
	return status;
}

/*
 *  ======== main ========
 */
int main(int argc, char **argv)
{
	INT nStrmMode = STRMMODE_ZEROCOPY;
	FILE *inFile = NULL;	/* Input file handle. */
	FILE *outFile = NULL;	/* Output file handle. */
	struct STRMCOPY_TASK strmcopyTask;
	DSP_STATUS status = DSP_SOK;

	DspManager_Open(argc, NULL);
	/* Process command line arguments, open data files: */
	/*status = ProcessArgs(argc, argv, &inFile, &outFile, &nStrmMode); */
	strmcopyTask.hProcessor = NULL;
	strmcopyTask.hNode = NULL;
	strmcopyTask.pInBufs = NULL;
	strmcopyTask.pOutBufs = NULL;
	strmcopyTask.nStrmMode = nStrmMode;
	if (DSP_SUCCEEDED(status)) {
		/* Perform processor level initialization. */
		status = InitializeProcessor(&strmcopyTask);
	}
	if (DSP_SUCCEEDED(status)) {
		/* Perform node level initialization. */
		status = InitializeNode(&strmcopyTask);
		if (DSP_SUCCEEDED(status)) {
			/* Run task. */
			status = RunTask(&strmcopyTask, inFile, outFile);
			if (DSP_SUCCEEDED(status)) {
				fprintf(stdout, "RunTask succeeded.\n");
				fprintf(stdout, "Zecopy Message successful\n");
			} else {
				fprintf(stdout, "RunTask failed.\n");
				fprintf(stdout, "Zecopy Message failed\n");
			}
		}
	}
	/* Close opened files. */
	if (inFile) {
		fclose(inFile);
	}
	if (outFile) {
		fclose(outFile);
	}
	CleanupNode(&strmcopyTask);
	CleanupProcessor(&strmcopyTask);
	DspManager_Close(0, NULL);
	/*printf("Hit enter to exit:  ");
	(void)getchar();*/
	return (DSP_SUCCEEDED(status) ? 0 : -1);
}

/*
 *  ======== InitializeProcessor ========
 *  Perform processor related initialization.
 */
static DSP_STATUS InitializeProcessor(struct STRMCOPY_TASK *copyTask)
{
	DSP_STATUS status = DSP_EFAIL;
	struct DSP_PROCESSORINFO dspInfo;
	UINT numProcs;
	UINT index = 0;
	INT procId = 0;
	while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,&dspInfo,
						(UINT)sizeof(struct DSP_PROCESSORINFO),&numProcs))) {
		if ((dspInfo.uProcessorType == DSPTYPE_55) || 
									(dspInfo.uProcessorType == DSPTYPE_64)) {
			printf("DSP device detected !! \n");
			procId = index;
			status = DSP_SOK;
			break;
		}
		index++;
	}
	/* Attach to an available DSP (in this case, the 1st DSP). */
	if (DSP_SUCCEEDED(status)) {
		status = DSPProcessor_Attach(procId, NULL, &copyTask->hProcessor);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPProcessor_Attach succeeded.\n");
		} else {
			fprintf(stdout, "DSPProcessor_Attach failed. Status = 0x%x\n",
																(UINT)status);
		}
	} else
		fprintf(stdout, "Failed to get the desired processor \n");
#ifdef PROC_LOAD
	if (DSP_SUCCEEDED(status)) {
		fprintf(stdout, " DSP Image: %s.\n", argv);
		status = DSPProcessor_Load(copyTask->hProcessor, argc, &argv, NULL);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPProcessor_Load succeeded.\n");
		} else {
			fprintf(stdout, "DSPProcessor_Load failed.\n");
		}
		status = DSPProcessor_Start(copyTask->hProcessor);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPProcessor_Start succeeded.\n");
		} else {
			fprintf(stdout, "DSPProcessor_Start failed.\n");
		}
	}
#endif
	return (status);
}

/*
 *  ======== InitializeNode ========
 *  Perform node related initialization.
 */
static DSP_STATUS InitializeNode(struct STRMCOPY_TASK *copyTask)
{
	BYTE argsBuf[ARGSIZE + sizeof(ULONG)];
	struct DSP_CBDATA *pArgs;
	struct DSP_NODEATTRIN nodeAttrIn;
	struct DSP_STRMATTR attrs;
	struct DSP_UUID uuid;
	DSP_STATUS status = DSP_SOK;

	uuid = nodeuuid;
	attrs.uBufsize = DEFAULTBUFSIZE;
	attrs.uNumBufs = DEFAULTNBUFS;
	attrs.uAlignment = 0;
	attrs.uTimeout = DSP_FOREVER;
	attrs.lMode = copyTask->nStrmMode;
	if (copyTask->nStrmMode == STRMMODE_ZEROCOPY) {
		attrs.uSegid = DSP_SHMSEG0;	/* Use SM for zero-copy */
	} else {
		attrs.uSegid = DEFAULTSEGMENT;
	}
	nodeAttrIn.uTimeout = 10000;
	nodeAttrIn.iPriority = 5;
	pArgs = (struct DSP_CBDATA *)argsBuf;
	pArgs->cbData = ARGSIZE;
	strncpy((char *)pArgs->cData, DEFAULTBUFSIZESTR, 5);
	/* Allocate the strmcopy node. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Allocate(copyTask->hProcessor, &uuid, pArgs,
												&nodeAttrIn, &copyTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Allocate succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Allocate failed. Status = 0x%x\n",
																(UINT)status);
		}
	}
	/* Create the strmcopy node on the DSP. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Create(copyTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Create succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Create failed. Status = 0x%x\n",
																(UINT)status);
		}
	}
	/* Start the strmcopy node on the DSP. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Run(copyTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Run succeeded.\n");
		} else {
			fprintf(stdout,"DSPNode_Run failed. Status = 0x%x\n",(UINT)status);
		}
	}

	return (status);
}

/*
 *  ======== RunTask ========
 *  Run strmcopy task.
 */
static DSP_STATUS RunTask(struct STRMCOPY_TASK *copyTask, FILE *inFile,
																FILE *outFile)
{
	DSP_STATUS status = DSP_SOK;
	BYTE *pSmMsgBuf = NULL;
	/* Allocate zero-copy message buffer */
	status = DSPNode_AllocMsgBuf(copyTask->hNode, DEFAULTBUFSIZE, NULL,
														(BYTE **)&pSmMsgBuf);
	fprintf(stdout,"DSPNode_AllocMsgBuf pSmMsgBuf:%x\n",
													(unsigned int)pSmMsgBuf);
	if (DSP_FAILED(status)) {
		fprintf(stdout, "DSPNode_AllocMsgBuf failed.\n");
	}
	copyTask->pOutBufs = pSmMsgBuf;
	/* Put a buffer into the output channel. */
	fprintf(stdout, "Sending %d bytes to DSP.\n", DEFAULTBUFSIZE);
	status = MsgToDsp(copyTask);
	if (!DSP_SUCCEEDED(status)) {
		fprintf(stdout, "MsgToDsp failed.\n");
		status = DSP_EFAIL;
	} else {
		fprintf(stdout, "Zero copy Message is successfully sent to DSP\n");
	}
	status = MsgFromDsp(copyTask);
	if (!DSP_SUCCEEDED(status)) {
		fprintf(stdout, "MsgFromDsp failed.\n");
		status = DSP_EFAIL;
	} else {
		fprintf(stdout,"Zero copy Message is successfully received from DSP\n");
	}
	/*  Free zero-copy message buffer */
	status = DSPNode_FreeMsgBuf(copyTask->hNode, pSmMsgBuf, NULL);
	fprintf(stdout,"DSPNode_FreeMsgBuf pSmMsgBuf:%x\n",(unsigned int)pSmMsgBuf);
	if (DSP_FAILED(status)) {
		fprintf(stdout, "DSPNode_FreeMsgBuf failed.\n");
	}
	return (status);
}

/*
 *  ======== CleanupNode ========
 *  Perform node related cleanup.
 */
static DSP_STATUS CleanupNode(struct STRMCOPY_TASK *copyTask)
{
	DSP_STATUS status = DSP_SOK;
	DSP_STATUS exitStatus;

	if (copyTask->hNode) {
		/* Terminate DSP node.*/
		status = DSPNode_Terminate(copyTask->hNode, &exitStatus);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout,"DSPNode_Terminate succeeded.\n");
		} else {
			fprintf(stdout,"DSPNode_Terminate failed: 0x%x\n", (UINT)status);
		}
		/* Delete DSP node. */
		status = DSPNode_Delete(copyTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Delete succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Delete failed: 0x%x\n", (UINT)status);
		}
		copyTask->hNode = NULL;
	}
	return (status);
}

/*
 *  ======== CleanupProcessor ========
 *  Perform processor related cleanup.
 */
static DSP_STATUS CleanupProcessor(struct STRMCOPY_TASK *copyTask)
{
	DSP_STATUS status = DSP_SOK;

	if (copyTask->hProcessor) {
		/* Detach from processor. */
		status = DSPProcessor_Detach(copyTask->hProcessor);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPProcessor_Detach succeeded.\n");
		} else {
			fprintf(stdout, "DSPProcessor_Detach failed.\n");
		}
		copyTask->hProcessor = NULL;
	}
	return (status);
}

/* *  ======== ProcessArgs ========
 *  Process command line arguments for this sample, returning input and
 *  output file handles.
 */
#if 0
static DSP_STATUS ProcessArgs(int argc, char **argv, FILE **inFile, 
												FILE **outFile, INT *pStrmMode)
{
	DSP_STATUS status = DSP_EFAIL;
	INT nModeVal;

	if (argc != 4) {
		fprintf(stdout, "Usage: %s <stream transport id> <input-filename> "
											"<output-filename> \n", argv[0]);
		fprintf(stdout, " where <stream transport id> is :\n");
		fprintf(stdout, "  0 - proc-copy, 1 - dsp-dma, 2 - zero-copy\n");
	} else {
		/* set stream mode */
		nModeVal = atoi(argv[1]);
		if (nModeVal == 1) {
			*pStrmMode = STRMMODE_RDMA;	/* dsp-dma */
			fprintf(stdout, "Data streaming using DSP-DMA transport.\n");
		} else if (nModeVal == 2) {
			*pStrmMode = STRMMODE_ZEROCOPY;	/* zero-copy */
			fprintf(stdout,"Data streaming using Zero-Copy transport "
													"buffer exchange.\n");
		} else {
			*pStrmMode = STRMMODE_PROCCOPY;	/* legacy processor copy */
			fprintf(stdout, "Data streaming using Processor-Copy transport.\n");
		}
		/* Open input & output files: */
		*inFile = fopen(argv[2], "rb");
		if (*inFile) {
			*outFile = fopen(argv[3], "wb");
			if (*outFile) {
				status = DSP_SOK;
			} else {
				fprintf(stdout, "%s: Unable to open file %s for writing\n",
															argv[0], argv[3]);
				fclose(*inFile);
				*inFile = NULL;
			}
		} else {
			fprintf(stdout, "%s: Unable to open file %s for reading\n",
															argv[0], argv[2]);
		}
	}
	return (status);
}
#endif


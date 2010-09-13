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
 *      Copy data from file to file on the host file system. Demonstrates
 *      data streaming using n-way buffering to/from a DSP program, and
 *      end-of-stream handling. This example uses DDSP API.
 *
 *  Usage:
 *      strmcopy.out <streaming transport id> <input-filename> <output-filename>
 *
 *   where streaming transport id is:
 *       0 - proc-copy
 *       1 - dsp-dma
 *       2 - zero-copy
 *
 *  Notes:
 *      Data is read from an input file, sent to the input stream of a DSP
 *      task which sends each buffer into it's output stream back to the
 *      host.  The host receives the data and writes it to the output file.
 *      The host program continues to stream data until end of input file
 *      is reached. If the output file exists, it is truncated on open.
 *
 */

#include <stdio.h>
#include <dbapi.h>
#include <stdlib.h>

/* Default stream attributes */
#define DEFAULTALIGNMENT   0
#define DEFAULTSEGMENT     0
#define DEFAULTTIMEOUT  5000
#define DEFAULTBUFSIZE  1024	/* Default buffer size in bytes. */
#define DEFAULTDSPUNIT  0	/* Default unit number for the board. */
#define DEFAULTDSPEXEC  "./strmcopy_tiomap24xx.dof55L"
#define DEFAULTNBUFS    1	/* Default # of buffers. */

#define ARGSIZE         32

/* strmcopy task context data structure. */
struct STRMCOPY_TASK {
	DSP_HPROCESSOR hProcessor;	/* Handle to processor. */
	struct DSP_UUID hNodeID;	/* Handle to node ID GUID. */
	DSP_HNODE hNode;	/* Handle to node. */
	DSP_HSTREAM hInStream;	/* Handle to node input stream. */
	DSP_HSTREAM hOutStream;	/* Handle to node output stream. */
	BYTE **ppInBufs;	/* Input stream buffers. */
	BYTE **ppOutBufs;	/* Output stream buffers. */
	struct DSP_NOTIFICATION hNotification;	/* DSP notification object. */
	INT nStrmMode;		/* stream mode */
};

/*
 *  strmcopy node
 */
/* {7EEB2C7E-785A-11d4-A650-00C04F0C04F3} */
static const struct DSP_UUID nodeuuid = { 0x7eeb2c7e, 0x785a, 0x11d4, 0xa6, 0x50,
										{ 0x0, 0xc0, 0x4f, 0xc, 0x4, 0xf3} };

/* Forward declarations: */
static int ProcessArgs(int argc,char **argv,FILE **inFile,FILE **outFile,
																INT *pStrmMode);

/* Initialization and cleanup routines. */
static int InitializeProcessor(struct STRMCOPY_TASK *copyTask);
static int InitializeNode(struct STRMCOPY_TASK *copyTask);
static int InitializeStreams(struct STRMCOPY_TASK *copyTask);
static int CleanupProcessor(struct STRMCOPY_TASK *copyTask);
static int CleanupNode(struct STRMCOPY_TASK *copyTask);
static int CleanupStreams(struct STRMCOPY_TASK *copyTask);
static int RunTask(struct STRMCOPY_TASK *copyTask, FILE *inFile,
																FILE *outFile);

/*
 *  ======== main ========
 */
int main(int argc, char **argv)
{
	INT nStrmMode = 0;
	FILE *inFile = NULL;	/* Input file handle. */
	FILE *outFile = NULL;	/* Output file handle. */
	struct STRMCOPY_TASK strmcopyTask;
	int status = 0;

	DspManager_Open(argc, NULL);

	/* Process command line arguments, open data files: */
	status = ProcessArgs(argc, argv, &inFile, &outFile, &nStrmMode);
	strmcopyTask.hProcessor = NULL;
	strmcopyTask.hNode = NULL;
	strmcopyTask.hInStream = NULL;
	strmcopyTask.hOutStream = NULL;
	strmcopyTask.ppInBufs = NULL;
	strmcopyTask.ppOutBufs = NULL;
	strmcopyTask.nStrmMode = nStrmMode;
	if (DSP_SUCCEEDED(status)) {
		/* Perform processor level initialization. */
		status = InitializeProcessor(&strmcopyTask);
	}
	if (DSP_SUCCEEDED(status)) {
		/* Perform node level initialization. */
		status = InitializeNode(&strmcopyTask);
		if (DSP_SUCCEEDED(status)) {
			/* Perform stream level initialization. */
			status = InitializeStreams(&strmcopyTask);
			if (DSP_SUCCEEDED(status)) {
				/* Run task. */
				status = RunTask(&strmcopyTask, inFile, outFile);
				if (DSP_SUCCEEDED(status)) {
					fprintf(stdout, "RunTask succeeded.\n");
				} else {
					fprintf(stdout, "RunTask failed.\n");
				}
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
	CleanupStreams(&strmcopyTask);
	CleanupNode(&strmcopyTask);
	CleanupProcessor(&strmcopyTask);
	DspManager_Close(0, NULL);
	/*printf("Hit enter to exit:  ");
	   (void)getchar(); */
	return (DSP_SUCCEEDED(status) ? 0 : -1);
}

/*
 *  ======== InitializeProcessor ========
 *  Perform processor related initialization.
 */
static int InitializeProcessor(struct STRMCOPY_TASK *copyTask)
{
	int status = -EPERM;
	struct DSP_PROCESSORINFO dspInfo;
	UINT numProcs;
	UINT index = 0;
	INT procId = 0;

	while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,&dspInfo,
						(UINT)sizeof(struct DSP_PROCESSORINFO),&numProcs))) {
		if ((dspInfo.uProcessorType == DSPTYPE_55)
									|| (dspInfo.uProcessorType == DSPTYPE_64)) {
			printf("DSP device detected !! \n");
			procId = index;
			status = 0;
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
static int InitializeNode(struct STRMCOPY_TASK *copyTask)
{
	BYTE argsBuf[ARGSIZE + sizeof(ULONG)];
	struct DSP_CBDATA *pArgs;
	struct DSP_NODEATTRIN nodeAttrIn;
	struct DSP_STRMATTR attrs;
	struct DSP_UUID uuid;
	int status = 0;

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

	/* Allocate the strmcopy node. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Allocate(copyTask->hProcessor,&uuid,pArgs,&nodeAttrIn,
															&copyTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Allocate succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Allocate failed. Status = 0x%x\n",
																(UINT)status);
		}
	}
	/* Connect the strmcopy node to the GPP. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Connect((DSP_HNODE)DSP_HGPPNODE,0,copyTask->hNode,0,
																		&attrs);
		if (DSP_FAILED(status)) {
			fprintf(stdout, "DSPNode_Connect failed. Status = 0x%x\n",
																(UINT)status);
		}
	}
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Connect(copyTask->hNode, 0, (DSP_HNODE)DSP_HGPPNODE, 0,
																		&attrs);
		if (DSP_FAILED(status)) {
			fprintf(stdout, "DSPNode_Connect failed. Status = 0x%x\n",
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
			fprintf(stdout, "DSPNode_Run failed. Status = 0x%x\n",
																(UINT)status);
		}
	}

	return (status);
}

/*
 *  ======== InitializeStreams ========
 *  Perform stream related initialization.
 */
static int InitializeStreams(struct STRMCOPY_TASK *copyTask)
{
	int status = 0;
	struct DSP_STREAMATTRIN attrs;

	attrs.cbStruct = sizeof(struct DSP_STREAMATTRIN);
	attrs.uTimeout = DEFAULTTIMEOUT;
	attrs.uAlignment = DEFAULTALIGNMENT;
	attrs.uNumBufs = DEFAULTNBUFS;
	attrs.lMode = copyTask->nStrmMode;
	if (copyTask->nStrmMode == STRMMODE_PROCCOPY) {
		attrs.uSegment = DEFAULTSEGMENT;
	} else {
		attrs.uSegment = DSP_SHMSEG0;	/* Use shared memory segment 0 */
	}
	/* Open an input data stream from the host to the strmcopy node. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPStream_Open(copyTask->hNode, DSP_TONODE, 0, &attrs,
														&copyTask->hInStream);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Open of input stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_Open of input stream failed. Status"
													"= 0x%x\n",(UINT)status);
		}
	}
	/* Open an output stream from the strmcopy node to the host. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPStream_Open(copyTask->hNode, DSP_FROMNODE, 0, &attrs,
														&copyTask->hOutStream);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Open of output stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_Open of output stream failed. Status "
													"= 0x%x\n",(UINT)status);
		}
	}
	/* Allocate and issue buffer to input data stream. */
	if (DSP_SUCCEEDED(status)) {
		copyTask->ppInBufs = (BYTE **)malloc(sizeof(BYTE *)*DEFAULTNBUFS);
		status = DSPStream_AllocateBuffers(copyTask->hInStream, DEFAULTBUFSIZE,
											copyTask->ppInBufs, DEFAULTNBUFS);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_AllocateBuffers for input "
														"stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_AllocateBuffers for input stream failed."
									" Status = 0x%x\n", (unsigned int)status);
		}
	}
	/* Allocate and issue buffer to output data stream. */
	if (DSP_SUCCEEDED(status)) {
		copyTask->ppOutBufs = (BYTE **)malloc(sizeof(BYTE *)*DEFAULTNBUFS);
		status = DSPStream_AllocateBuffers(copyTask->hOutStream, DEFAULTBUFSIZE,
											copyTask->ppOutBufs, DEFAULTNBUFS);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_AllocateBuffers for output "
														"stream succeeded.\n");
		} else {
			fprintf(stdout,"DSPStream_AllocateBuffers for output stream failed."
											" Status = 0x%x\n", (UINT)status);
		}
	}
	return (status);
}

/*
 *  ======== RunTask ========
 *  Run strmcopy task.
 */
static int RunTask(struct STRMCOPY_TASK *copyTask, FILE *inFile,
																FILE *outFile)
{
	int status = 0;
	BYTE *pInBuf = NULL;
	BYTE *pOutBuf = NULL;
	DWORD dwArg = 0;
	DWORD dwBufSize = DEFAULTBUFSIZE;
	int cBytesRead = 0;

	if (copyTask->ppOutBufs && copyTask->ppInBufs) {
		pInBuf = copyTask->ppInBufs[0];
		pOutBuf = copyTask->ppOutBufs[0];
		/* Read a fixed-size buffer from a data stream, send buffer to DSP,
		and receive the same buffer back from the DSP.*/
		do {
			/* Read a block of data from the input file: */
			cBytesRead = fread(pInBuf, sizeof(BYTE), DEFAULTBUFSIZE, inFile);
			if (cBytesRead > 0) {
				/* Put a buffer into the output channel. */
				fprintf(stdout, "Sending %d bytes to DSP.\n", cBytesRead);
				/* Add I/O request to DSP. */
				status = DSPStream_Issue(copyTask->hInStream,pInBuf,cBytesRead,
															DEFAULTBUFSIZE, 0);
				if (!DSP_SUCCEEDED(status)) {
					fprintf(stdout, "DSPStream_Issue failed.\n");
					break;
				}
				/* Wait for output completion to reclaim a queued buffer:*/
				status = DSPStream_Reclaim(copyTask->hInStream, &pInBuf,
													&dwBufSize, NULL, &dwArg);
				if (!DSP_SUCCEEDED(status)) {
					fprintf(stdout,"DSPStream_Reclaim failed, 0x%x.\n",
																(UINT)status);
					break;
				}
				/*Put a buffer into the input channel to get back buffer sent.*/
				status = DSPStream_Issue(copyTask->hOutStream, pOutBuf,
											DEFAULTBUFSIZE, DEFAULTBUFSIZE, 0);
				if (!DSP_SUCCEEDED(status)) {
					fprintf(stdout, "DSPStream_Issue failed.\n");
					break;
				}
				/*Wait for input notification to reclaim a queued buffer:*/
				status = DSPStream_Reclaim(copyTask->hOutStream, &pOutBuf,
														&dwBufSize,NULL,&dwArg);
				if (status == -ETIME) {
					fprintf(stdout, "DSPStream_Reclaim timed out.\n");
					break;
				} else if (!DSP_SUCCEEDED(status)) {
					fprintf(stdout, "DSPStream_Reclaim failed, 0x%x.\n",
																(UINT)status);
					break;
				}
				fprintf(stdout, "Writing %d bytes to output file.\n",
													(unsigned int)dwBufSize);
				/*Copy the block of data from the DSP into the output file:*/
				fwrite(pOutBuf, sizeof(BYTE), dwBufSize, outFile);
			}
		} while ((cBytesRead > 0) && DSP_SUCCEEDED(status));
	} else {
		status = -EPERM;
	}
	return (status);
}

/*
 *  ======== CleanupStreams ========
 *  Perform stream related cleanup.
 */
static int CleanupStreams(struct STRMCOPY_TASK *copyTask)
{
	struct DSP_STREAMINFO streamInfo;
	int status = 0;
	DWORD dwArg = 0;
	DWORD dwBufsize = DEFAULTBUFSIZE;
	BYTE *pBuf;

	/* Reclaim and free stream buffers. */
	if (copyTask->hInStream) {
		DSPStream_Idle(copyTask->hInStream, true);
		status = DSPStream_GetInfo(copyTask->hInStream, &streamInfo,
															sizeof(streamInfo));
		if (copyTask->ppInBufs) {
			/* Reclaim the buffer if it's still in the stream */
			if (streamInfo.uNumberBufsInStream) {
				status = DSPStream_Reclaim(copyTask->hInStream, &pBuf,
													&dwBufsize, NULL, &dwArg);
			}
			status = DSPStream_FreeBuffers(copyTask->hInStream, 
											copyTask->ppInBufs, DEFAULTNBUFS);
			if (DSP_FAILED(status)) {
				fprintf(stdout, "DSPStream_FreeBuffer of input buffer failed. "
											"Status = 0x%x\n", (UINT)status);
			}
			free(copyTask->ppInBufs);
			copyTask->ppInBufs = NULL;
			/* Close open streams. */
			status = DSPStream_Close(copyTask->hInStream);
			if (DSP_FAILED(status)) {
				fprintf(stdout, "DSPStream_Close of input stream failed. "
											"Status = 0x%x\n", (UINT)status);
			}
			copyTask->hInStream = NULL;
		}
	}
	if (copyTask->hOutStream) {
		DSPStream_Idle(copyTask->hOutStream, true);
		/* Need to determine the number of buffers left in the stream */
		status = DSPStream_GetInfo(copyTask->hOutStream, &streamInfo,
															sizeof(streamInfo));
		if (copyTask->ppOutBufs) {
			if (streamInfo.uNumberBufsInStream) {
				status = DSPStream_Reclaim(copyTask->hOutStream, &pBuf, 
														&dwBufsize,NULL,&dwArg);
			}
			/* Free buffer. */
			status = DSPStream_FreeBuffers(copyTask->hOutStream, 
											copyTask->ppOutBufs, DEFAULTNBUFS);
			if (DSP_FAILED(status)) {
				fprintf(stdout, "DSPStream_FreeBuffer of output buffer failed."
											" Status = 0x%x\n", (UINT)status);
			}
			free(copyTask->ppOutBufs);
			copyTask->ppOutBufs = NULL;
		}
		status = DSPStream_Close(copyTask->hOutStream);
		if (DSP_FAILED(status)) {
			fprintf(stdout, "DSPStream_Close of output stream failed. "
											"Status = 0x%x\n", (UINT)status);
		}
		copyTask->hOutStream = NULL;
	}
	return (status);
}

/*
 *  ======== CleanupNode ========
 *  Perform node related cleanup.
 */
static int CleanupNode(struct STRMCOPY_TASK *copyTask)
{
	int status = 0;
	int exitStatus;

	if (copyTask->hNode) {
		/*Terminate DSP node.*/
		status = DSPNode_Terminate(copyTask->hNode, &exitStatus);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Terminate succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Terminate failed: 0x%x\n", (UINT)status);
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
static int CleanupProcessor(struct STRMCOPY_TASK *copyTask)
{
	int status = 0;

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
static int ProcessArgs(int argc, char **argv, FILE **inFile,
												FILE **outFile, INT *pStrmMode)
{
	int status = -EPERM;
	INT nModeVal;
	if (argc != 4) {
		fprintf(stdout, "Usage: %s <stream transport id> <input-filename>"
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
			fprintf(stdout, "Data streaming using Zero-Copy transport"
													" buffer exchange.\n");
		} else {
			*pStrmMode = STRMMODE_PROCCOPY;	/* legacy processor copy */
			fprintf(stdout, "Data streaming using Processor-Copy transport.\n");
		}
		/* Open input & output files: */
		*inFile = fopen(argv[2], "rb");
		if (*inFile) {
			*outFile = fopen(argv[3], "wb");
			if (*outFile) {
				status = 0;
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


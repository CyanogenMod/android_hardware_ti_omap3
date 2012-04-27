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
 *  ======== wdmmcopy.c ========
 *  Description:
 *      wdmmcopy demonstrates usage of the Dynamic Memory Mapping APIs
 *      by creating two MPU buffers, mapping them to the DSP virtual
 *      address space, and copying data on the DSP from one buffer to
 *      another.  Bridge messaging is used to synchronize with the DSP.
 *
 *  Usage:
 *      wdmmcopy <input-filename> <output-filename>
 *
 *  Example:
 *      wdmmcopy "release\input.txt" "release\output.txt"
 *
 *  Notes:
 *      The GPP reads data from an input file and copies the data into
 *      an MPU buffer that has been mapped to the DSP virtual address
 *      space using the DMM APIs.  The GPP then notifies the DSP that
 *      the file read has completed and includes the number of words
 *      that were read.  Using this information, the DSP copies the data
 *      from the MPU mapped buffer to another buffer, which is also an
 *      MPU buffer that has been mapped to the DSP virtual address space.
 *      The DSP then notifies the GPP that the copy operation has completed,
 *      which prompts the GPP to take the data stored in the buffer and
 *      write it to the output file.
 *
 *! Revision History:
 *! ================
 *! 19-Apr-2004 sb : Aligned DMM definitions with Symbian
 *! 19-Mar-2004 map: Created based on wstrmcopy.c
 */

#include <stdio.h>

#ifdef UNDER_CE
#include <windows.h>
#endif

#include <stdlib.h>
#include <dbapi.h>
#include <DSPProcessor_OEM.h>

/* Application sepcific attributes */
#define DEFAULTDSPUNIT      0	/* Default unit number for the board. */
#define DEFAULTNBUFS        1	/* Default # of buffers. */
#define DMMCOPY_TASK_ID     TEXT("dmmcopy")
#define ARGSIZE             32

#define DEFAULTBUFSIZE      0x19000	/* Default buffer size in bytes. */

/* Messaging DSP commands */
#define DMM_SETUPBUFFERS    0xABCD
#define DMM_WRITEREADY      0xADDD
#define PAGE_ALIGN_MASK		0xfffff000	/* Align mask for a 0x1000 page */
#define PAGE_ALIGN_UNMASK	0x00000fff	/* Unmask for a 0x1000 page */

UINT g_dwDSPWordSize = 1;	// default for 2430

/* dmmcopy task context data structure. */
struct DMMCOPY_TASK {
	DSP_HPROCESSOR hProcessor;	/* Handle to processor. */
	DSP_HNODE hNode;	/* Handle to node. */
} ;

/* DMMCOPY_TI_uuid = 28BA464F_9C3E_484E_990F_48305B183848 */
static const struct DSP_UUID DMMCOPY_TI_uuid = {
	0x28ba464f, 0x9c3e, 0x484e, 0x99, 0x0f, { 0x48, 0x30, 0x5b, 0x18, 0x38,0x48}
};

/* Forward declarations: */
static int ProcessArgs(INT argc,char **argv,FILE **inFile,
																FILE **outFile);

/* Initialization and cleanup routines. */
static int InitializeProcessor(struct DMMCOPY_TASK *copyTask);
static int InitializeNode(struct DMMCOPY_TASK *copyTask);
static int CleanupProcessor(struct DMMCOPY_TASK *copyTask);
static int CleanupNode(struct DMMCOPY_TASK *copyTask);
static int RunTask(struct DMMCOPY_TASK *copyTask,FILE *inFile,
																FILE *outFile);

/*
 *  ======== main ========
 */
INT main(INT argc, char **argv)
{
	FILE *inFile = NULL;	/* Input file handle. */
	FILE *outFile = NULL;	/* Output file handle. */
	struct DMMCOPY_TASK dmmcopyTask;
	int status = 0;

	DspManager_Open(argc, NULL);
	/* Process command line arguments, open data files: */
	status = ProcessArgs(argc, argv, &inFile, &outFile);
	dmmcopyTask.hProcessor = NULL;
	dmmcopyTask.hNode = NULL;
	if (DSP_SUCCEEDED(status)) {
		/* Perform processor level initialization. */
		status = InitializeProcessor(&dmmcopyTask);
	}
	if (DSP_SUCCEEDED(status)) {
		/* Perform node level initialization. */
		status = InitializeNode(&dmmcopyTask);
		if (DSP_SUCCEEDED(status)) {
			/* Run task. */
			status = RunTask(&dmmcopyTask, inFile, outFile);
			if (DSP_SUCCEEDED(status)) {
				fprintf(stdout, "RunTask succeeded.\n");
			} else {
				fprintf(stdout, "RunTask failed.\n");
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
	CleanupNode(&dmmcopyTask);
	CleanupProcessor(&dmmcopyTask);
	/*
	   printf("Hit enter to exit:  ");
	   (void)getchar();
	 */
	return (DSP_SUCCEEDED(status) ? 0 : -1);
}

/*
 *  ======== InitializeProcessor ========
 *  Perform processor related initialization.
 */
static int InitializeProcessor(struct DMMCOPY_TASK *copyTask)
{
	int status = -EPERM;
	struct DSP_PROCESSORINFO dspInfo;
	UINT numProcs;
	UINT index = 0;
	INT procId = 0;
	/* Attach to DSP */
	while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,&dspInfo,
						(UINT)sizeof(struct DSP_PROCESSORINFO),&numProcs))) {
		if ((dspInfo.uProcessorType == DSPTYPE_55) || 
									(dspInfo.uProcessorType == DSPTYPE_64)) {
			if (dspInfo.uProcessorType == DSPTYPE_55) {
				g_dwDSPWordSize = 2;
			} else {
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
	if (DSP_SUCCEEDED(status)) {
		status = DSPProcessor_Attach(procId, NULL,&copyTask->hProcessor);
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
#endif				/* PROC_LOAD */
	return (status);
}

/*
 *  ======== InitializeNode ========
 *  Perform node related initialization.
 */
static int InitializeNode(struct DMMCOPY_TASK *copyTask)
{
	BYTE argsBuf[ARGSIZE + sizeof(ULONG)];
	struct DSP_CBDATA *pArgs;
	struct DSP_NODEATTRIN nodeAttrIn;
	struct DSP_UUID uuid;
	int status = 0;

	uuid = DMMCOPY_TI_uuid;
	nodeAttrIn.uTimeout = DSP_FOREVER;
	nodeAttrIn.iPriority = 5;
	pArgs = (struct DSP_CBDATA *)argsBuf;
	pArgs->cbData = ARGSIZE;
	/* Allocate the dmmcopy node. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Allocate(copyTask->hProcessor, &uuid, pArgs,
												&nodeAttrIn, &copyTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Allocate succeeded.\n");
		} else {
			fprintf(stdout,"DSPNode_Allocate failed. Status = 0x%x\n",
																(UINT)status);
		}
	}
	/* Create the dmmcopy node on the DSP. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Create(copyTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Create succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Create failed. Status = 0x%x\n",
																(UINT)status);
		}
	}
	/* Start the dmmcopy node on the DSP. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Run(copyTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Run succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Run failed. Status = 0x%x\n",(UINT)status);
		}
	}
	return (status);
}

/*
 *  ======== RunTask ========
 *  Run dmmcopy task.
 */
static int RunTask(struct DMMCOPY_TASK *copyTask, FILE *inFile,
																FILE *outFile)
{
	INT cBytesRead = 0;
	int status = 0;

	/* Actual MPU Buffer addresses */
	ULONG aBufferSend = 0;
	ULONG aBufferRecv = 0;
	ULONG aBufferSend1 = 0;
	ULONG aBufferRecv1 = 0;
	/* Reserved buffer DSP virtual addresses (bytes) */
	PVOID aDspSendBuffer = NULL;
	PVOID aDspRecvBuffer = NULL;
	/* Base addresses of reserved DSP virtual address ranges (bytes) */
	PVOID dspAddrSend = NULL;
	PVOID dspAddrRecv = NULL;
	/* MPU buffer size */
	ULONG ulSendBufferSize = DEFAULTBUFSIZE;
	ULONG ulRecvBufferSize = DEFAULTBUFSIZE;
	/* DSP reserve size = buffersize + 4K page size */
	ULONG ulSendResv = ulSendBufferSize + 0x1000;
	ULONG ulRecvResv = ulRecvBufferSize + 0x1000;
	/* Messaging used for GPP/DSP synchronization */
	struct DSP_MSG msgToDsp;
	struct DSP_MSG msgFromDsp;
	INT j;
	INT i = 0;

	/* Allocate MPU Buffers */
	aBufferSend = (unsigned long) (char *)calloc(ulSendBufferSize + ((PAGE_ALIGN_UNMASK + 1) << 1),1);
	aBufferSend1 = (aBufferSend + PAGE_ALIGN_UNMASK) & PAGE_ALIGN_MASK;
	aBufferRecv = (unsigned long) (char *)calloc(ulRecvBufferSize + ((PAGE_ALIGN_UNMASK + 1) << 1),1);
	aBufferRecv1 = (aBufferRecv + PAGE_ALIGN_UNMASK) & PAGE_ALIGN_MASK;

	/* Initialize buffers */
	for (i = 0;i < ulSendBufferSize;i++) {
		((PSTR) aBufferSend1)[i] = 0;
		((PSTR) aBufferRecv1)[i] = 0;
	}
	if (!aBufferSend1 || !aBufferRecv1) {
		fprintf(stdout, "Failed to allocate MPU buffers.\n");
		status = -ENOMEM;
	} else {
		/* Reserve DSP virtual memory for send buffer */
		status = DSPProcessor_ReserveMemory(copyTask->hProcessor, ulSendResv,
														&dspAddrSend);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPProcessor_ReserveMemory succeeded. dspAddrSend"
												"= 0x%x \n",(UINT)dspAddrSend);
		} else {
			fprintf(stdout, "DSPProcessor_ReserveMemory failed. "
											"Status = 0x%x\n", (UINT)status);
		}
	}
	if (DSP_SUCCEEDED(status)) {
		/* Reserve DSP virtual memory for receive buffer */
		status = DSPProcessor_ReserveMemory(copyTask->hProcessor, ulRecvResv,
														&dspAddrRecv);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout,"DSPProcessor_ReserveMemory succeeded. dspAddrdRecv"
												"= 0x%x \n",(UINT)dspAddrRecv);
		} else {
			fprintf(stdout, "DSPProcessor_ReserveMemory failed."
											" Status = 0x%x\n", (UINT)status);
		}
	}
	if (DSP_SUCCEEDED(status)) {
		/* Map MPU "send buffer" to DSP "receive buffer" virtual address */
		status = DSPProcessor_Map(copyTask->hProcessor, (PVOID)aBufferSend1, 
										ulSendBufferSize, dspAddrSend,
												&aDspRecvBuffer, 0);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPProcessor_Map succeeded.\n");
		} else {
			fprintf(stdout, "DSPProcessor_Map failed. "
											"Status = 0x%x\n",(UINT)status);
		}
	}
	if (DSP_SUCCEEDED(status)) {
		/* Map MPU "receive buffer" to DSP "send buffer" virtual address */
		status = DSPProcessor_Map(copyTask->hProcessor, (PVOID)aBufferRecv1, 
										ulRecvBufferSize, dspAddrRecv, 
													&aDspSendBuffer,0);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPProcessor_Map succeeded.\n");
		} else {
			fprintf(stdout, "DSPProcessor_Map failed. "
											"Status = 0x%x\n",(UINT)status);
		}
	}
	if (DSP_SUCCEEDED(status)) {
		/* Notify DSP of word-adjusted buffer addresses */
		msgToDsp.dwCmd = DMM_SETUPBUFFERS;
		msgToDsp.dwArg1 = (DWORD)aDspRecvBuffer / g_dwDSPWordSize;
		msgToDsp.dwArg2 = (DWORD)aDspSendBuffer / g_dwDSPWordSize;
		status = DSPNode_PutMessage(copyTask->hNode, &msgToDsp, DSP_FOREVER);
		fprintf(stdout, "Sending DMM BUFs to DSP cmd=SETUP, DspRecvBuf=0x%x, "
								"DspSendBuf=0x%x \n",(UINT)aDspRecvBuffer,
														(UINT)aDspSendBuffer);
		if (DSP_FAILED(status)) {
			fprintf(stdout, "DSPProcessor_PutMessage failed. Status = 0x%x\n",
																(UINT)status);
		}
	}
	if (DSP_SUCCEEDED(status)) {
		/*
		 * Read a fixed-size buffer from a data stream,
		 * copy data to output buffer, and read back from
		 * input buffer.
		 */
		/* do a flush of the buffers, before using them */
		DSPProcessor_FlushMemory(copyTask->hProcessor,(PVOID)aBufferSend1,
															DEFAULTBUFSIZE,0);
		DSPProcessor_FlushMemory(copyTask->hProcessor,(PVOID)aBufferRecv1,
															DEFAULTBUFSIZE,0);
		do {
			/* Read a block of data from the input file */
			cBytesRead = fread((PSTR)aBufferSend1, sizeof(BYTE),DEFAULTBUFSIZE,
																		inFile);
			if (cBytesRead > 0) {
				fprintf(stdout, "Read %d bytes from input file.\n", cBytesRead);
				/* Go ahead and flush here */
				DSPProcessor_FlushMemory(copyTask->hProcessor,
										(PVOID)aBufferSend1,DEFAULTBUFSIZE,0);
				/* Tell DSP how many words to read */
				msgToDsp.dwCmd = DMM_WRITEREADY;
				msgToDsp.dwArg1 = (DWORD)cBytesRead / g_dwDSPWordSize;
				status = DSPNode_PutMessage(copyTask->hNode, &msgToDsp, 
																DSP_FOREVER);
				if (DSP_FAILED(status)) {
					fprintf(stdout, "DSPProcessor_PutMessage failed. "
											"Status = 0x%x\n", (UINT)status);
				} else {
					/*
					 * Wait to hear back from DSP that it has finished
					 * copying the buffers and that it's okay for us
					 * to read buffer again
					 */
					status = DSPNode_GetMessage(copyTask->hNode, &msgFromDsp,
																DSP_FOREVER);
					if (DSP_SUCCEEDED(status)) {
						/* Go ahead and flush here */
						DSPProcessor_InvalidateMemory(copyTask->hProcessor,
											(PVOID)aBufferRecv1,DEFAULTBUFSIZE);
						fprintf(stdout, "Writing %d bytes to output file.\n",
																	cBytesRead);

						for (j = 0;j < cBytesRead;j++) {
							if (((unsigned char *)aBufferSend1)[j] !=
											((unsigned char *)aBufferRecv1)[j]) {
								fprintf(stdout,"Sent and received buffers"
														" don't match \n");
								fprintf(stdout, "Press any key to continue \n");
								(void)getchar();
								break;
							}
						}
						/*
						 * Copy the block of data from the DSP into
						 * the output file
						 */
						fwrite((PSTR)aBufferRecv1, sizeof(BYTE), cBytesRead,
																	outFile);
					} else {
						fprintf(stdout,"DSPProcessor_GetMessage failed. "
											"Status = 0x%x\n", (UINT)status);
					}
				}
			}
		} while ((cBytesRead > 0) && DSP_SUCCEEDED(status));
	}
	/* Unmap DSP Recv buffer from MPU receive buffer */
	status = DSPProcessor_UnMap(copyTask->hProcessor, (PVOID)aDspRecvBuffer);
	if (DSP_SUCCEEDED(status)) {
		fprintf(stdout, "DSPProcessor_UnMap succeeded.\n");
	} else {
		fprintf(stdout, "DSPProcessor_UnMap failed. Status = 0x%x\n",
																(UINT)status);
	}
	/* Upmap DSP Send buffer from MPU receive buffer */
	status = DSPProcessor_UnMap(copyTask->hProcessor,(PVOID)aDspSendBuffer);
	if (DSP_SUCCEEDED(status)) {
		fprintf(stdout, "DSPProcessor_UnMap succeeded.\n");
	} else {
		fprintf(stdout, "DSPProcessor_UnMap failed. Status = 0x%x\n",
																(UINT)status);
	}
	/* Unreserve DSP virtual memory */
	status = DSPProcessor_UnReserveMemory(copyTask->hProcessor,
															(PVOID)dspAddrSend);
	if (DSP_SUCCEEDED(status)) {
		fprintf(stdout, "DSPProcessor_UnReserveMemory succeeded.\n");
	} else {
		fprintf(stdout, "DSPProcessor_UnReserveMemory failed. "
											"Status = 0x%x\n", (UINT)status);
	}
	/* Unreserve DSP virtual memory */
	status = DSPProcessor_UnReserveMemory(copyTask->hProcessor, 
															(PVOID)dspAddrRecv);
	if (DSP_SUCCEEDED(status)) {
		fprintf(stdout, "DSPProcessor_UnReserveMemory succeeded.\n");
	} else {
		fprintf(stdout, "DSPProcessor_UnReserveMemory failed. "
											"Status = 0x%x\n", (UINT)status);
	}
	/* Free MPU buffers (at originally allocated addresses) */
	if (aBufferSend) {
		free((VOID *)aBufferSend);
	}
	if (aBufferRecv) {
		free((VOID *)aBufferRecv);
	}
	return (status);
}

/*
 *  ======== CleanupNode ========
 *  Perform node related cleanup.
 */
static int CleanupNode(struct DMMCOPY_TASK *copyTask)
{
	int exitStatus;
	int status = 0;

	if (copyTask->hNode) {
		/* Terminate DSP node */
		status = DSPNode_Terminate(copyTask->hNode, &exitStatus);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Terminate succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Terminate failed: 0x%x\n", (UINT)status);
		}
		/* Delete DSP node */
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
static int CleanupProcessor(struct DMMCOPY_TASK *copyTask)
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

/*
 *  ======== ProcessArgs ========
 *  Process command line arguments for this sample, returning input and
 *  output file handles.
 */
static int ProcessArgs(INT argc,char **argv,FILE **inFile,FILE **outFile)
{
	int status = -EPERM;
	if (argc != 3) {
		fprintf(stdout, "Usage: dmmcopy <input-filename> <output-filename>\n");
	} else {
		/* Open input & output files: */
		*inFile = fopen(argv[1], "rb");
		if (*inFile) {
			*outFile = fopen(argv[2], "wb");
			if (*outFile) {
				status = 0;
			} else {
				fprintf(stdout, "%s: Unable to open file %s for writing\n",
															argv[0], argv[2]);
				fclose(*inFile);
				*inFile = NULL;
			}
		} else {
			fprintf(stdout, "%s: Unable to open file %s for reading\n",
															argv[0], argv[1]);
		}
	}
	return (status);
}


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
 *  ======== scale.c ========
 *  Description:
 *      Send buffers of data to the DSP to be scaled and returned back.  This
 *      test program exercise an xDAIS socket containing the SCALE_TI
 *      algorithm.
 *
 *  Usage:
 *      scale.out <iterations>
 */

#include <stdio.h>
#include <dbapi.h>
#include <stdlib.h>

#define MESSAGES            1

/* default parameters: */
#define DEFAULTITERATIONS   8	/* number of loop iterations */
#define DEFAULTBUFSIZE      32	/* buffer size in bytes */
#define DEFAULTDSPUNIT      0	/* unit number for the DSP */

/* node-specific message command codes: */
#define CMD_SETSCALE        DSP_RMSUSERCODESTART + 1
#define CMD_ACKSCALE        DSP_RMSUSERCODESTART + 2

/* scale application context data structure: */
struct SCALE_TASK{
	DSP_HPROCESSOR hProcessor;	/* processor handle */
	DSP_HNODE hNode;	/* node handle */
	DSP_HSTREAM hInStream;	/* input stream handle */
	DSP_HSTREAM hOutStream;	/* output stream handle */
	BYTE *pInBuf;		/* input stream buffer */
	BYTE *pOutBuf;		/* output stream buffer */
	UINT uIterations;	/* number of loop iterations */
} ;

/*
 *  xDAIS socket node for SCALE_TI
 */
/* {d91a01bd_d215_11d4_8626_00105a98ca0b } */
static const struct DSP_UUID nodeuuid = {
	0xd91a01bd, 0xd215, 0x11d4, 0x86,0x26,{ 0x0, 0x10, 0x5a, 0x98, 0xca, 0x0b}};

/* Forward declarations: */
static DSP_STATUS ProcessArgs(int argc, char **argv, 
												struct SCALE_TASK *scaleTask);
static DSP_STATUS InitializeProcessor(struct SCALE_TASK * scaleTask);
static DSP_STATUS InitializeNode(struct SCALE_TASK * scaleTask);
static DSP_STATUS InitializeStreams(struct SCALE_TASK * scaleTask);
static DSP_STATUS CleanupProcessor(struct SCALE_TASK * scaleTask);
static DSP_STATUS CleanupNode(struct SCALE_TASK * scaleTask);
static DSP_STATUS CleanupStreams(struct SCALE_TASK * scaleTask);
static DSP_STATUS RunTask(struct SCALE_TASK * scaleTask);

/*
 *  ======== main ========
 */
int
main(int argc, char **argv)
{
	struct SCALE_TASK scaleTask;
	DSP_STATUS status = DSP_SOK;

	status = (DBAPI)DspManager_Open(0, NULL);

	if (DSP_FAILED(status)) {
		printf("\nFATAL ERROR: Bridge Initialisation FAILED\n");
		return 0;
	}

	status = ProcessArgs(argc, argv, &scaleTask);

	/* initialize context object */
	scaleTask.hProcessor = NULL;
	scaleTask.hNode = NULL;
	scaleTask.hInStream = NULL;
	scaleTask.hOutStream = NULL;
	scaleTask.pInBuf = NULL;
	scaleTask.pOutBuf = NULL;

	if (DSP_SUCCEEDED(status)) {
		/* do processor level initialization */
		status = InitializeProcessor(&scaleTask);
		if (DSP_SUCCEEDED(status)) {
			/* do node level initialization */
			status = InitializeNode(&scaleTask);
			if (DSP_SUCCEEDED(status)) {
				/* do stream level initialization */
				status = InitializeStreams(&scaleTask);
				if (DSP_SUCCEEDED(status)) {
					/* do processing */
					status = RunTask(&scaleTask);
					if (DSP_SUCCEEDED(status)) {
						fprintf(stdout, "\nRunTask succeeded.\n");
					} else {
						fprintf(stdout, "\nRunTask failed.\n");
					}
				}
			}
		}
	}
	CleanupStreams(&scaleTask);
	CleanupNode(&scaleTask);
	CleanupProcessor(&scaleTask);
	status = DspManager_Close(0, NULL);
	if (DSP_FAILED(status)) {
		printf("\nERROR: Bridge Close FAILED\n");
	}
	return (DSP_SUCCEEDED(status) ? 0 : -1);
}

static DSP_STATUS ProcessArgs(int argc,char **argv,struct SCALE_TASK *scaleTask)
{
	DSP_STATUS status = DSP_EFAIL;

	if (argc != 2) {
		fprintf(stdout, "Usage: %s <No. of Iterations> \n", argv[0]);
	} else {
		sscanf(argv[1], "%u", &(scaleTask->uIterations));
		status = DSP_SOK;

	}

	return status;
}

/*
 *  ======== InitializeProcessor ========
 *  Perform processor related initialization.
 */
static DSP_STATUS InitializeProcessor(struct SCALE_TASK *scaleTask)
{
	DSP_STATUS status = DSP_EFAIL;
	struct DSP_PROCESSORATTRIN *pAttrIn;
	struct DSP_PROCESSORINFO dspInfo;
	UINT numProcs;
	UINT index = 0;
	INT procId = 0;

	pAttrIn = malloc(sizeof(struct DSP_PROCESSORATTRIN));
	pAttrIn->cbStruct = sizeof(struct DSP_PROCESSORATTRIN);
	pAttrIn->uTimeout = 50;

	/* Attach to DSP */
	/* status = DSPProcessor_Attach(DEFAULTDSPNIT, NULL,*/
	while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index, &dspInfo,
						(UINT)sizeof(struct DSP_PROCESSORINFO), &numProcs))) {
#if 0
		fprintf(stdout,
			"DSPManager_EnumInfo, got data for proc index  %d of %d \n",
			index, numProcs);
		fprintf(stdout, "uProcessorFamily = %d \n",
			dspInfo.uProcessorFamily);
		fprintf(stdout, "uProcessorType = %d \n",
			dspInfo.uProcessorType);
		fprintf(stdout, "uClockRate = %d \n", dspInfo.uClockRate);
		fprintf(stdout, "ulInternalMemsize = %d \n",
			dspInfo.ulInternalMemSize);
		fprintf(stdout, "ulExternalMemsize = %d \n",
			dspInfo.ulExternalMemSize);
		fprintf(stdout, "uProcessorID = %d \n", dspInfo.uProcessorID);
		fprintf(stdout, "tyRunningRTOS = %d \n", dspInfo.tyRunningRTOS);
		fprintf(stdout, "nNodeMinPrio = %d \n",
			dspInfo.nNodeMinPriority);
		fprintf(stdout, "nNodeMaxPrio = %d \n",
			dspInfo.nNodeMaxPriority);
#endif
		if ((dspInfo.uProcessorType == DSPTYPE_55) || 
									(dspInfo.uProcessorType == DSPTYPE_64)) {
			printf("DSP device detected !! \n");
			procId = index;
			status = DSP_SOK;
			break;
		}
		index++;
	}
	/* attach to the DSP */
	if (DSP_SUCCEEDED(status)) {
		status = DSPProcessor_Attach(procId, NULL, &scaleTask->hProcessor);
	} else {
		fprintf(stdout, "Failed to get the desired Processor !! \n");
	}
	if (DSP_SUCCEEDED(status)) {
		fprintf(stdout, "DSPProcessor_Attach succeeded.\n");
	} else {
		fprintf(stdout, "DSPProcessor_Attach failed. Status = 0x%x\n",
																(UINT)status);
	}
#ifdef PROC_LOAD
	if (DSP_SUCCEEDED(status)) {
		fprintf(stdout, " DSP Image: %s.\n", argv);
		/* Load the DSP image specified */
		status = DSPProcessor_Load(scaleTask->hProcessor, argc, &argv, NULL);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPProcessor_Load succeeded.\n");
		} else {
			fprintf(stdout, "DSPProcessor_Load failed.\n");
		}
		if (DSP_SUCCEEDED(status)) {
			/* Start DSP */
			status = DSPProcessor_Start(scaleTask->hProcessor);
			if (DSP_SUCCEEDED(status)) {
				fprintf(stdout, "DSPProcessor_Start succeeded.\n");
			} else {
				fprintf(stdout, "DSPProcessor_Start failed.\n");
			}
		}
	}
#endif				/* PROC_LOAD */
	return (status);
}

/*
 *  ======== InitializeNode ========
 *  Perform node related initialization.
 */
static DSP_STATUS InitializeNode(struct SCALE_TASK *scaleTask)
{
	struct DSP_NODEATTRIN nodeAttrIn;
	struct DSP_STRMATTR attrs;
	struct DSP_UUID uuid;
	DSP_STATUS status = DSP_SOK;

	uuid = nodeuuid;

	attrs.uSegid = 0;
	attrs.uBufsize = DEFAULTBUFSIZE;
	attrs.uNumBufs = 1;
	attrs.uAlignment = 0;
	attrs.uTimeout = DSP_FOREVER;
	attrs.lMode = STRMMODE_PROCCOPY;

	nodeAttrIn.uTimeout = DSP_FOREVER;
	nodeAttrIn.iPriority = 5;
	/* allocate the scale socket node */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Allocate(scaleTask->hProcessor, &uuid, NULL,
												&nodeAttrIn, &scaleTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Allocate succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Allocate failed. Status = 0x%x\n",
																(UINT) status);
		}
	}
	/* connect the scale socket input node to the GPP */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Connect((DSP_HNODE) DSP_HGPPNODE, 0, scaleTask->hNode,
																	0, &attrs);
		if (DSP_FAILED(status)) {
			fprintf(stdout,"DSPNode_Connect failed. Status = 0x%x\n",
																(UINT)status);
		}
	}

	/* connect the scale socket output node to the GPP */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Connect(scaleTask->hNode, 0, (DSP_HNODE) DSP_HGPPNODE,
																	0, &attrs);
		if (DSP_FAILED(status)) {
			fprintf(stdout, "DSPNode_Connect failed. Status = 0x%x\n",
																(UINT)status);
		}
	}
	/* create the socket node on the DSP */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Create(scaleTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Create succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Create failed. Status = 0x%x\n",
																(UINT)status);
		}
	}

	/* start the socket node on the DSP */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Run(scaleTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Run succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Run failed. Status = 0x%x\n",(UINT)status);
		}
	}
	return (status);
}

/*
 *  ======== InitializeStreams ========
 *  Perform stream related initialization.
 */
static DSP_STATUS InitializeStreams(struct SCALE_TASK *scaleTask)
{
	DSP_STATUS status = DSP_SOK;

	/* open an output data stream (from host to DSP node) */
	if (DSP_SUCCEEDED(status)) {
		status = DSPStream_Open(scaleTask->hNode, DSP_TONODE, 0, NULL,
														&scaleTask->hOutStream);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Open of output stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_Open of output stream failed. "
											"Status = 0x%x\n", (UINT)status);
		}
	}

	/* open an input stream (from the DSP node to the host) */
	if (DSP_SUCCEEDED(status)) {
		status = DSPStream_Open(scaleTask->hNode, DSP_FROMNODE, 0, NULL,
														&scaleTask->hInStream);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Open of input stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_Open of input stream failed. "
											"Status = 0x%x\n", (UINT)status);
		}
	}
	/* allocate buffer for the input stream */
	if (DSP_SUCCEEDED(status)) {
		status = DSPStream_AllocateBuffers(scaleTask->hInStream, DEFAULTBUFSIZE,
														&scaleTask->pInBuf, 1);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout,"DSPStream_AllocateBuffers for input "
													"stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_AllocateBuffers for input stream "
									"failed. Status = 0x%x\n", (UINT)status);
		}
	}

	/* allocate buffer for the output data stream */
	if (DSP_SUCCEEDED(status)) {
		status = DSPStream_AllocateBuffers(scaleTask->hOutStream,
										DEFAULTBUFSIZE, &scaleTask->pOutBuf, 1);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout,"DSPStream_AllocateBuffers for output "
														"stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_AllocateBuffers for output stream "
									"failed. Status = 0x%x\n", (UINT)status);
		}
	}
	return (status);
}

/*
 *  ======== RunTask ========
 *  Run the xDAIS socket; stream data to it and back for scaling.
 */
static DSP_STATUS RunTask(struct SCALE_TASK *scaleTask)
{
	BYTE *pOutBuf = scaleTask->pOutBuf;
	BYTE *pInBuf = scaleTask->pInBuf;
	DSP_STATUS status = DSP_SOK;
	struct DSP_MSG message;
	DWORD dwBufSize;
	DWORD dwArg;
	UINT loopCount;
	UINT loop = 0;
	BYTE *index;
	UINT i;

	/* initialize input and output buffer contents */
	index = scaleTask->pOutBuf;

	for (i = 0;i < DEFAULTBUFSIZE;i++) {
		*index++ = 1;
	}

	index = scaleTask->pInBuf;

	for (i = 0;i < DEFAULTBUFSIZE;i++) {
		*index++ = 0;
	}

	/* display initial buffer values */
	index = scaleTask->pOutBuf;
	fprintf(stdout, "Buffer to be scaled by DSP: ");

	for (i = 0;i < 16;i++) {
		fprintf(stdout, "%x ", *index++);
	}

	fprintf(stdout, "\n");

	printf("scaleTask->uIterations %d", scaleTask->uIterations);

	/* pass buffers to DSP for scaling: */
	for (loopCount = 0;loopCount < scaleTask->uIterations;loopCount++) {
#if MESSAGES
		/* send command to node to set scale factor: */
		fprintf(stdout, "\nSetting alg scale factor to %d...", loop);
		message.dwCmd = CMD_SETSCALE;
		message.dwArg1 = (DWORD) loop;
		status = DSPNode_PutMessage(scaleTask->hNode, &message, 15000);
		if (!DSP_SUCCEEDED(status)) {
			fprintf(stdout,"DSPNode_PutMessage failed.\n "
										"Status = %x.\n",(unsigned int)status);
			break;
		}
		/* wait for acknowledgement from node */
		status = DSPNode_GetMessage(scaleTask->hNode, &message, 15000);
		if (!DSP_SUCCEEDED(status)) {
			fprintf(stdout, "\nDSPNode_GetMessage failed. Status = %x.\n",
																(UINT)status);
			break;
		}
		/* check for proper response */
		if ((message.dwCmd == CMD_ACKSCALE) && (message.dwArg1 == (DWORD)loop)){
			fprintf(stdout, "DSP ACK.\n");
		} else {
			fprintf(stdout,"\nDidn't receive command acknowledgement!");
			break;
		}
#else
		fprintf(stdout, "\n");
#endif

		/* issue filled buffer to the DSP */
		status = DSPStream_Issue(scaleTask->hOutStream, pOutBuf, DEFAULTBUFSIZE,
															DEFAULTBUFSIZE, 0);
		if (!DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Issue to output failed. Status = %x.\n",
																(UINT)status);
			break;
		}
		/* reclaim empty buffer back from the output stream */
		status = DSPStream_Reclaim(scaleTask->hOutStream, &pOutBuf, &dwBufSize,
																NULL, &dwArg);
		if (!DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Reclaim of output failed %x.\n",
																(UINT)status);
			break;
		}
		/* issue empty buffer to the input stream */
		status = DSPStream_Issue(scaleTask->hInStream, pInBuf,DEFAULTBUFSIZE,
															DEFAULTBUFSIZE, 0);
		if (!DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Issue to input failed. Status = %x.\n",
																(UINT)status);
			break;
		}
		/* reclaim filled buffer back from the input stream */
		status = DSPStream_Reclaim(scaleTask->hInStream, &pInBuf, &dwBufSize,
															      NULL, &dwArg);
		if (!DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Reclaim from input failed %x.\n",
																(UINT)status);
			break;
		}
		fprintf(stdout, "Iteration %d completed, received: ", loop++);
		index = scaleTask->pInBuf;
		for (i = 0;i < 16;i++) {
			fprintf(stdout, "%x ", *index++);
		}
	}
	return (status);
}

/*
 *  ======== CleanupStreams ========
 *  Perform stream related cleanup.
 */
static DSP_STATUS CleanupStreams(struct SCALE_TASK *scaleTask)
{
	DSP_STATUS status = DSP_SOK;
	struct DSP_STREAMINFO streamInfo;
	DWORD dwArg = 0;
	DWORD dwBufsize = DEFAULTBUFSIZE;

	/* cleanup input stream */
	if (scaleTask->hInStream) {
		/* idle input stream */
		status = DSPStream_Idle(scaleTask->hInStream, true);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Idle of input stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_Idle of input stream failed %x.\n",
																(UINT)status);
		}
		status = DSPStream_GetInfo(scaleTask->hInStream, &streamInfo,
															sizeof(streamInfo));
		/* Reclaim and free input buffer */
		if (scaleTask->pInBuf) {
			/* Reclaim the buffer if it's still in the stream */
			if (streamInfo.uNumberBufsInStream) {
				status = DSPStream_Reclaim(scaleTask->hInStream, 
									&scaleTask->pInBuf,&dwBufsize,NULL,&dwArg);
			}
			status = DSPStream_FreeBuffers(scaleTask->hInStream,
														&scaleTask->pInBuf, 1);
			if (DSP_SUCCEEDED(status)) {
				fprintf(stdout,"DSPStream_FreeBuffers of input "
														"buffer succeeded.\n");
			} else {
				fprintf(stdout,"DSPStream_FreeBuffers of "
													"input buffer failed.\n");
			}
		}
		/* close the input stream */
		status = DSPStream_Close(scaleTask->hInStream);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPStream_Close of input stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_Close of input stream failed %x.\n",
																(UINT)status);
		}
		scaleTask->hInStream = NULL;
	}
	if (scaleTask->hOutStream) {
		/* idle the output stream */
		DSPStream_Idle(scaleTask->hOutStream, true);
		status = DSPStream_GetInfo(scaleTask->hOutStream, &streamInfo,
															sizeof(streamInfo));
		/* Reclaim and free output buffer */
		if (scaleTask->pOutBuf) {
			if (streamInfo.uNumberBufsInStream) {
				status = DSPStream_Reclaim(scaleTask->hOutStream,
								&scaleTask->pOutBuf, &dwBufsize, NULL, &dwArg);
			}
			status = DSPStream_FreeBuffers(scaleTask->hOutStream,
														&scaleTask->pOutBuf, 1);
			if (DSP_SUCCEEDED(status)) {
				fprintf(stdout,"DSPStream_FreeBuffers of output "
														"buffer succeeded.\n");
			} else {
				fprintf(stdout,"DSPStream_FreeBuffers of output "
														"buffer failed.\n");
			}
		}
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout,"DSPStream_Idle of output stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_Idle of output stream failed %x.\n",
																(UINT)status);
		}
		/* close the output stream */
		status = DSPStream_Close(scaleTask->hOutStream);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout,"DSPStream_Close of output stream succeeded.\n");
		} else {
			fprintf(stdout, "DSPStream_Close of output stream failed %x.\n",
																(UINT)status);
		}
		scaleTask->hOutStream = NULL;
	}
	return (status);
}

/*
 *  ======== CleanupNode ========
 *  Perform node related cleanup.
 */
static DSP_STATUS CleanupNode(struct SCALE_TASK *scaleTask)
{
	DSP_STATUS status = DSP_SOK;
	DSP_STATUS exitStatus;

	if (scaleTask->hNode) {
		/* terminate DSP node */
		status = DSPNode_Terminate(scaleTask->hNode, &exitStatus);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Terminate succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Terminate failed.\n");
		}
		/* delete DSP node */
		status = DSPNode_Delete(scaleTask->hNode);
		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPNode_Delete succeeded.\n");
		} else {
			fprintf(stdout, "DSPNode_Delete failed.\n");
		}
		scaleTask->hNode = NULL;
	}
	return (status);
}

/*
 *  ======== CleanupProcessor ========
 *  Perform processor related cleanup.
 */
static DSP_STATUS CleanupProcessor(struct SCALE_TASK *scaleTask)
{
	DSP_STATUS status = DSP_SOK;

	if (scaleTask->hProcessor) {
		/* detach from processor */
		status = DSPProcessor_Detach(scaleTask->hProcessor);

		if (DSP_SUCCEEDED(status)) {
			fprintf(stdout, "DSPProcessor_Detach succeeded.\n");
		} else {
			fprintf(stdout, "DSPProcessor_Detach failed %x.\n",(UINT)status);
		}

		scaleTask->hProcessor = NULL;
	}

	return (status);
}


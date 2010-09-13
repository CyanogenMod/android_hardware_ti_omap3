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
 *  ======== ping.c ========
 *  DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *  Description:
 *     Console application to manage the DSP/BIOS Bridge Ping Task Node.
 *
 *  Demonstrates:
 *      1. How to create a task node with initial arguments.
 *      2. How to get notified of, and retrieve messages, sent from a DSP node.
 *
 *  Requires:
 *      1. Node PING_TI must be configured into the base image (via Gconf)
 *
 *  Usage:
 *      ping.out <message_count>
 *
 *  Notes:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbapi.h>		/* DSP/BIOS Bridge APIs                  */

#define DEFAULTDELAY  "50"	/* default ping delay                    */
#define DEFAULTMSGS   50	/* default message count                 */
#define DSPUNIT       0		/* Default (first) DSP ID                */
#define ARGSIZE       32	/* Size of arguments to Ping node.       */
#define MAXNAMELEN    64	/* Max length of event name.             */
#define MAXMSGLEN     128	/* Max length of MessageBox msg.         */

#define PING           0x01

/* PING_TI_UUID = 12A3C3C1_D015_11D4_9F69_00C04F3A59AE */
static const struct DSP_UUID PING_TI_uuid = {
	0x12a3c3c1, 0xd015,0x11d4,0x9f,0x69,{0x00, 0xc0, 0x4f, 0x3a, 0x59, 0xae} };

/* Ping task context data structure - passed between functions in this module */
struct PING_TASK {
	DSP_HPROCESSOR hProcessor;	/* Handle to processor.             */
	DSP_HNODE hNode;	/* Handle to node.                  */
	HANDLE hEvent;		/* Event to be signalled by DSP     */
	UINT msgCount;		/* Number of messages DSP sends     */
} ;

/* Argument structure for DSPNode_Allocate():                           */
struct PING_NODEDATA {
	ULONG cbData;
	BYTE cData[ARGSIZE];
} ;

/* Forward Declarations */
static int ProcessArgs(int argc, char **argv, UINT *pMsgCnt,
												struct PING_NODEDATA *argsBuf);
static int AttachProcessor(struct PING_TASK * pingTask);
static int CreateNode(struct PING_TASK * pingTask,
												struct PING_NODEDATA * argsBuf);
static int RunNode(struct PING_TASK * pingTask);
static int DestroyNode(struct PING_TASK * pingTask);
static int DetachProcessor(struct PING_TASK * pingTask);

/*
 *  ======== main ========
 */
int main(int argc, char **argv)
{
	struct PING_TASK pingTask;	/* Ping task context                    */
	UINT msgCount;		/* Number of messages DSP sends         */
	struct PING_NODEDATA argsBuf;	/* Task Node args.                      */
	int status = 0;

	/* Process command line arguments, open data files: */
	status = ProcessArgs(argc, argv, &msgCount, &argsBuf);
	if (DSP_SUCCEEDED(status)) {
		/* Initialize context: */
		pingTask.hProcessor = NULL;
		pingTask.hNode = NULL;
		pingTask.hEvent = NULL;
		pingTask.msgCount = msgCount;
		status = DspManager_Open(0, NULL);
		if (DSP_SUCCEEDED(status)) {
			/* Perform processor level initialization. */
			status = AttachProcessor(&pingTask);
			if (DSP_SUCCEEDED(status)) {
				/* Perform node level initialization. */
				status = CreateNode(&pingTask, &argsBuf);
				if (DSP_SUCCEEDED(status)) {
					/* Run Ping task. */
					status = RunNode(&pingTask);
				}
			}
			/* Cleanup all allocated resources. */
			DestroyNode(&pingTask);
			DetachProcessor(&pingTask);
		}
	}
	status = DspManager_Close(0, NULL);

	return (DSP_SUCCEEDED(status) ? 0 : -1);
}

/*
 *  ======== AttachProcessor ========
 *  Perform processor related initialization.
 */
static int AttachProcessor(struct PING_TASK *pingTask)
{
	int status = -EPERM;
	struct DSP_PROCESSORINFO dspInfo;
	UINT numProcs;
	UINT index = 0;
	INT procId = 0;

	while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,&dspInfo,
						(UINT)sizeof(struct DSP_PROCESSORINFO), &numProcs))) {
		if ((dspInfo.uProcessorType == DSPTYPE_55) || 
									(dspInfo.uProcessorType == DSPTYPE_64)) {
			printf("DSP device detected !! \n");
			procId = index;
			status = 0;
			break;
		}
		index++;
	}

	/* Attach to a DSP (first by default) */
	status = DSPProcessor_Attach(procId, NULL, &pingTask->hProcessor);
	if (DSP_FAILED(status)) {
		fprintf(stdout, "DSPProcessor_Attach failed: 0x%x\n",(UINT)status);
	}

	return (status);
}

/*
 *  ======== CreateNode ========
 *  Perform node related initialization.
 */
static int CreateNode(struct PING_TASK *pingTask,
												struct PING_NODEDATA *argsBuf)
{
	struct DSP_CBDATA *pArgs;
	int status;

	pArgs = (struct DSP_CBDATA *)argsBuf;
	/* Allocate the ping node, passing arguments for node create phase. */
	status = DSPNode_Allocate(pingTask->hProcessor, &PING_TI_uuid, pArgs, NULL,
															&pingTask->hNode);
	if (DSP_FAILED(status)) {
		fprintf(stdout, "DSPNode_Allocate failed: 0x%x\n", (UINT)status);
	}
	/* Create the ping node on the DSP. */
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_Create(pingTask->hNode);
		if (DSP_FAILED(status)) {
			fprintf(stdout, "DSPNode_Create failed: 0x%x\n", (UINT)status);
		}
        else
           fprintf(stdout,"DSPNodeCreate succeeded \n"); 
    }
	/* Register for message notifications:  */
	if (DSP_SUCCEEDED(status)) {
		pingTask->hEvent = malloc(sizeof(struct DSP_NOTIFICATION));
		status = DSPNode_RegisterNotify(pingTask->hNode, DSP_NODEMESSAGEREADY, 
											DSP_SIGNALEVENT, pingTask->hEvent);
		if (DSP_FAILED(status)) {
			fprintf(stdout,"DSPNode_RegisterNotify failed:0x%x\n",(UINT)status);
		}
        else
           fprintf(stdout,"DSPNode_registerNotify succeeded \n"); 
	}
	return (status);
}

/*
 *  ======== RunNode ========
 *  Run ping task.
 */
static int RunNode(struct PING_TASK *pingTask)
{
	int status;
	int exitStatus;
	struct DSP_MSG dspMsg;
	UINT uIndex;
	DWORD cEvents;
	struct DSP_NOTIFICATION* ahEvents[2];
	int n = 0;

	cEvents = 1;
	ahEvents[0] = pingTask->hEvent;

	/* Start the ping node on the DSP. */
	status = DSPNode_Run(pingTask->hNode);
	if (DSP_FAILED(status)) {
		fprintf(stdout, "DSPNode_Run failed: 0x%x\n", (UINT)status);
	} else
		fprintf(stdout,"DSPNode_run succeeded \n"); 

	if (DSP_SUCCEEDED(status)) {
		for (n = 0;n < pingTask->msgCount;n = n + 1) {
			dspMsg.dwCmd = PING;
			status = DSPNode_PutMessage(pingTask->hNode, &dspMsg, 15000);
			if (!DSP_SUCCEEDED(status)) {
				fprintf(stdout, "DSPNode_PutMessage:DSP Ping failed, 0x%x.\n",
																(UINT)status);
			}
			/* Block until pinged by DSP or key event: */
			status = DSPManager_WaitForEvents(ahEvents, cEvents, &uIndex,
																DSP_FOREVER);
			/* Process events: */
			if (DSP_SUCCEEDED(status) && uIndex == 0) {
				/* DSP signalled us: */
				while (DSP_SUCCEEDED(status)) {
					status = DSPNode_GetMessage(pingTask->hNode, &dspMsg, 0);
					if (DSP_SUCCEEDED(status)) {
						fprintf(stdout, "Ping: Id %f Msg %f Mem %f\n",
									(DOUBLE)dspMsg.dwCmd, (DOUBLE)dspMsg.dwArg1,
														(DOUBLE)dspMsg.dwArg2);
					}
				}
				if (DSP_FAILED(status)) {
					if (status == -ETIME) {
						/* Okay to timeout if message queue is empty */
						status = 0;
						continue;
					} else {
						fprintf(stdout,"DSPNode_GetMessage failed: 0x%x\n",
																(UINT)status);
					}
				}
			} else {
				/* Wait error: */
				fprintf(stdout,"DSPManager_WaitForBridgeEvents Failed: 0x%x\n",
																(UINT)status);
			}
		}
	}
	/* Terminate the DSP node. */
	status = DSPNode_Terminate(pingTask->hNode, &exitStatus);
	if (DSP_FAILED(status)) {
		fprintf(stdout, "DSPNode_Terminate failed: 0x%x\n",(UINT)status);
	}
	return (status);
}

/*
 *  ======== DestroyNode ========
 *  Perform node related cleanup.
 */
static int DestroyNode(struct PING_TASK *pingTask)
{
	int status = 0;

	if (pingTask->hNode) {
		/* Delete DSP node. */
		status = DSPNode_Delete(pingTask->hNode);
		if (DSP_FAILED(status)) {
			fprintf(stdout, "DSPNode_Delete failed: 0x%x\n", (UINT)status);
		}
		pingTask->hNode = NULL;
	}
	if (pingTask->hEvent) {
		free(pingTask->hEvent);
	}
	return (status);
}

/*
 *  ======== DetachProcessor ========
 *  Perform processor related cleanup.
 */
static int DetachProcessor(struct PING_TASK *pingTask)
{
	int status = 0;

	if (pingTask->hProcessor) {
		/* Detach from processor. */
		status = DSPProcessor_Detach(pingTask->hProcessor);
		if (DSP_FAILED(status)) {
			fprintf(stdout, "DSPProcessor_Detach failed: 0x%x\n",(UINT)status);
		}
		pingTask->hProcessor = NULL;
	}

	return (status);
}

/*
 *  ======== ProcessArgs ========
 *  Process command line arguments for this sample, returning input and
 *  output file handles.
 */
static int ProcessArgs(int argc, char **argv, UINT *pMsgCnt,
												struct PING_NODEDATA *argsBuf)
{
	int status = 0;
	switch (argc) {
	case 1:
		*pMsgCnt = DEFAULTMSGS;
		strncpy((char *)argsBuf->cData, DEFAULTDELAY, ARGSIZE);
		break;
	case 2:
		sscanf(argv[1], "%u", pMsgCnt);
		strncpy((char *)argsBuf->cData, DEFAULTDELAY, ARGSIZE);
		break;
	default:
		fprintf(stdout, "Usage: %s <message_count> \n", argv[0]);
		strncpy((char *)argsBuf->cData, DEFAULTDELAY, ARGSIZE);
		status = -EPERM;
		break;
	}
	if (DSP_SUCCEEDED(status)) {
		argsBuf->cbData = strlen((char *)argsBuf->cData) + 1;
	}
	return (status);
}


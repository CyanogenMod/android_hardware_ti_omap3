/*
 * dspbridge/src/qos/linux/qos/qosti.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed .as is. WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

/*  ============================================================================
  File    QosTI.c
  Desc    Interface to xDAIS Qos gateway component
  Rev     0.6.0
--------------------------------------------- DSP/BIOS  Bridge
--------------------------------------------- Gateway Component
*/
#include <qosregistry.h>
#include <qosti_dspdecl.h>
#include <qosti.h>
#include <qos_ti_uuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dbapi.h>		/* DSP/BIOS Bridge APIs                  */
#include <_dbdebug.h>
/*#define STUB_OUT_BRIDGE_API     1 */
DSP_HPROCESSOR hProcessor = NULL;
DSP_HNODE hNode = NULL;
INT iMode = EDynamicLoad;
/*  ============================================================================
    name    KNodeDll
    desc    Dynamic qos node DLL.
    ============================================================================
*/
CHAR pszNodeDll[] = "/dspbridge/qosdyn_3430.dll64P";
/* This is a much better way than DEBUGMSG... */
void DbgMsg(DWORD dwZone, char *szFormat, ...)
{
	va_list ap;
	if (dwZone >= DSPAPI_DEBUG_LEVEL) {
		va_start(ap, szFormat);
		vprintf(szFormat, ap);
		va_end(ap);
	}
}
/*  ============================================================================
    func   MsgToDsp
    desc   Send Message to DSP
    ret    DSP_SOK if Message was transferred to DSP successfully.
    ============================================================================
	*/
DSP_STATUS QosTI_DspMsg(DWORD dwCmd, DWORD dwArg1, DWORD dwArg2, DWORD *dwOut1,
							DWORD *dwOut2)
{
	INT status = DSP_SOK;
#ifndef STUB_OUT_BRIDGE_API
	struct DSP_MSG dspMsg;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "QosTI_DspMsg+\n");
	dspMsg.dwCmd = dwCmd;
	dspMsg.dwArg1 = dwArg1;
	dspMsg.dwArg2 = dwArg2;
	status = DSPNode_PutMessage(hNode, &dspMsg, 10000);
	if (DSP_SUCCEEDED(status)) {
		status = DSPNode_GetMessage(hNode, &dspMsg, 10000);
		if (DSP_SUCCEEDED(status)) {
			DbgMsg(DSPAPI_ZONE_TEST,
				"Qosti: Id %d Msg %d Mem %d\n",
				dspMsg.dwCmd, dspMsg.dwArg1, dspMsg.dwArg2);
			/* Debug */
			if (dspMsg.dwCmd != dwCmd) {
				DbgMsg(DSPAPI_ZONE_WARNING,
					"Received unknown command: [%d]\n",
					dspMsg.dwCmd);
				status = DSP_EFAIL;
			} else {
				if (dwOut1)
					*dwOut1 = dspMsg.dwArg1;

				if (dwOut2)
					*dwOut2 = dspMsg.dwArg2;

			}
		} else
			DbgMsg(DSPAPI_ZONE_ERROR, "DSPNode_PutMessage:DSP QoS "
						"failed, 0x%x.\n", status);

	}
	DbgMsg(DSPAPI_ZONE_FUNCTION, "QosTI_DspMsg-\n");
#endif
	return status;
}

/*  ============================================================================
  func   Create
    desc   Create Qos service.
    ============================================================================
*/
DSP_STATUS QosTI_Create()
{
	DSP_STATUS status = DSP_SOK;
#ifndef STUB_OUT_BRIDGE_API
	struct DSP_PROCESSORINFO dspInfo;
	UINT numProcs;
	UINT index = 0;
	INT procId = 0;
	struct DSP_CBDATA Args;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "QosTI_Create+\n");
	/*  Open the DSP device */
	DbgMsg(DSPAPI_ZONE_TEST, "Calling DspManager_Open...\n");
	status = DspManager_Open(0, NULL);
	DbgMsg(DSPAPI_ZONE_TEST, "after DspManager_Open (status=%d)\n", status);
	if (DSP_SUCCEEDED(status)) {
		/* Perform processor level initialization. */
		status = DSP_EFAIL;
		while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,
			&dspInfo, (UINT)sizeof(struct DSP_PROCESSORINFO),
				&numProcs))) {
			if (dspInfo.uProcessorType == DSPTYPE_55) {
				DbgMsg(DSPAPI_ZONE_TEST,
						"DSP device detected !! \n");
				procId = index;
				status = DSP_SOK;
				break;
			}
			index++;
		}
		/* Attach to a DSP (first by default) */
		status = DSPProcessor_Attach(procId, NULL, &hProcessor);
		if (DSP_SUCCEEDED(status)) {
			if (iMode == EDynamicLoad) {
				/*  Register the node object. */
				status = DSPManager_RegisterObject(
						(struct DSP_UUID *)&QOS_TI_UUID,
						DSP_DCDNODETYPE, pszNodeDll);
				if (DSP_SUCCEEDED(status)) {
					/*  Register the node DLL. */
					status = DSPManager_RegisterObject(
					&QOS_TI_UUID, DSP_DCDLIBRARYTYPE,
						pszNodeDll);
					if (DSP_FAILED(status))
						DbgMsg(DSPAPI_ZONE_TEST,
						"DSPManager_RegisterObject "
						"failed: 0x%x\n",
						(PVOID) status);
				} else
					DbgMsg(DSPAPI_ZONE_TEST,
					"DSPManager_RegisterObject "
					"failed: 0x%x\n", (PVOID) status);

			}
			/* Perform node level initialization. */
			/* Allocate the node, passing arguments for node */
			/* create phase. */
			Args.cbData = 0;
			status = DSPNode_Allocate(hProcessor, &QOS_TI_UUID,
					&Args, NULL, &hNode);
			if (DSP_SUCCEEDED(status)) {
				/* Create the node on the DSP. */
				status = DSPNode_Create(hNode);
				if (DSP_SUCCEEDED(status)) {
					/* Run task. */
					status = DSPNode_Run(hNode);
					if (DSP_FAILED(status))
						DbgMsg(DSPAPI_ZONE_TEST,
						"DSPNode_Run failed: 0x%x\n",
							(PVOID) status);
				} else
					DbgMsg(DSPAPI_ZONE_TEST,
					"DSPNode_Create failed: 0x%x\n",
						(PVOID) status);
			} else
				DbgMsg(DSPAPI_ZONE_TEST,
				"DSPNode_Allocate failed: %x\n",
					status);

		} else
			DbgMsg(DSPAPI_ZONE_TEST,
			"DSPProcessor_Attach failed: 0x%x\n",
				(PVOID) status);
	}
	if (DSP_FAILED(status))
		QosTI_Delete();

	DbgMsg(DSPAPI_ZONE_FUNCTION, "QosTI_Create-\n");
#endif
	return status;
}

/*  ============================================================================

    func   Delete



    desc   Delete Qos service.



    ============================================================================
	*/

void QosTI_Delete()
{
#ifndef STUB_OUT_BRIDGE_API
	DSP_STATUS status = DSP_SOK;
	DSP_STATUS exitStatus;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "QosTI_Delete+\n");
	/* Terminate the DSP node. */
	status = DSPNode_Terminate(hNode, &exitStatus);
	if (DSP_FAILED(status)) {
		DbgMsg(DSPAPI_ZONE_TEST, "DSPNode_Terminate failed:0x%x\n",
				(PVOID)status);
	}
	/* Cleanup all allocated resources. */
	if (hNode) {
		/* Delete DSP node. */
		status = DSPNode_Delete(hNode);
		if (DSP_FAILED(status))
			DbgMsg(DSPAPI_ZONE_TEST,
				"DSPNode_Delete failed: 0x%x\n",
				(PVOID) status);

		hNode = NULL;
	}
	if (iMode == EDynamicLoad) {
		/*  Unregister the node dynamic libraries. */
		status = DSPManager_UnregisterObject(&QOS_TI_UUID,
					DSP_DCDLIBRARYTYPE);
		status = DSPManager_UnregisterObject(&QOS_TI_UUID,
					DSP_DCDNODETYPE);
	}
	if (hProcessor) {
		/* Detach from processor. */
		status = DSPProcessor_Detach(hProcessor);
		if (DSP_FAILED(status)) {
			DbgMsg(DSPAPI_ZONE_TEST, "DSPProcessor_Detach failed:"
				" 0x%x\n", (PVOID) status);
		}
		hProcessor = NULL;
	}
	status = DspManager_Close(0, NULL);
	DbgMsg(DSPAPI_ZONE_FUNCTION, "QosTI_Delete-\n");
#endif
	return;
}

/*  ========================================================================
    func   GetDynLoaderMemStat
    desc   Get the current memory utilization for heaps used in dynamic loading.
    arg   IN heapDesc:  value in range 0..4 => Heap Identifier
	Valid values:
		EDynloadDaram    = DYN_DARAM heap (internal)
		EDynloadSaram    = DYN_SARAM heap (internal)
		EDynloadExternal = DYN_EXTERNAL heap (external)
		EDynloadSram     = DYN_SRAM heap (internal)
	arg   OUT memInitSize:             initially configured size of heap
	arg   OUT memUsed:                 size of heap in use (not free)
	arg   OUT memLargestFreeBlockSize: size of largest contiguous free
				memory
	arg   OUT memFreeBlocks:           number of free blocks in heap
	arg   OUT memAllocBlocks:          number of allocated blocks in heap
	ret   DSP_SOK if successful.
    ========================================================================
*/
DSP_STATUS QosTI_GetDynLoaderMemStat(UINT heapDesc, UINT *memInitSize,
			UINT *memUsed, UINT *memLargestFreeBlockSize,
			UINT *memFreeBlocks, UINT *memAllocBlocks)
{
	INT status = DSP_SOK;
	struct DSP_RESOURCEINFO ResourceInfo;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "QosTI_GetDynLoaderMemStat+\n");
	if ((heapDesc == EDynloadDaram) || (heapDesc == EDynloadSaram) ||
	    (heapDesc == EDynloadExternal) || (heapDesc == EDynloadSram)) {
		/* Query DLL Heap */
		ResourceInfo.cbStruct = sizeof(ResourceInfo);
		status = DSPProcessor_GetResourceInfo(hProcessor, heapDesc,
					&ResourceInfo, sizeof(ResourceInfo));
		if (DSP_SUCCEEDED(status)) {
			*memInitSize = ResourceInfo.result.memStat.ulSize;
			*memUsed = *memInitSize -
				ResourceInfo.result.memStat.ulTotalFreeSize;
			*memLargestFreeBlockSize =
				ResourceInfo.result.memStat.ulLenMaxFreeBlock;
			*memFreeBlocks =
				ResourceInfo.result.memStat.ulNumFreeBlocks;
			*memAllocBlocks =
				ResourceInfo.result.memStat.ulNumAllocBlocks;
		} else
			DbgMsg(DSPAPI_ZONE_TEST,
				"GetDynLoaderMemStat:Heap [%d] Query Error"
				"[%d]\n", heapDesc, status);

	} else
		/* Bad argument */
		status = DSP_EINVALIDARG;

	DbgMsg(DSPAPI_ZONE_FUNCTION, "QosTI_GetDynLoaderMemStat-\n");
	return status;
}

/*  ========================================================================
    func   QosTI_GetProcLoadStat
    desc   Get the Processor load statistics
    arg   OUT currentLoad:
    arg   OUT predLoad:
    arg   OUT currDspFreq:
    arg   OUT predictedFreq:
    ret   DSP_SOK if successful.
    ======================================================================== */
DSP_STATUS QosTI_GetProcLoadStat(UINT *currentLoad, UINT *predLoad,
		    UINT *currDspFreq, UINT *predictedFreq)
{
	INT status = DSP_SOK;
	struct DSP_RESOURCEINFO ResourceInfo;

	ResourceInfo.cbStruct = sizeof(ResourceInfo);
	status = DSPProcessor_GetResourceInfo(hProcessor,
		DSP_RESOURCE_PROCLOAD,
		&ResourceInfo, sizeof(ResourceInfo));
	if (DSP_SUCCEEDED(status)) {
		*currentLoad = ResourceInfo.result.procLoadStat.uCurrLoad;
		*predLoad = ResourceInfo.result.procLoadStat.uPredictedLoad;
		*currDspFreq = ResourceInfo.result.procLoadStat.uCurrDspFreq;
		*predictedFreq =
			ResourceInfo.result.procLoadStat.uPredictedFreq;

	} else
		status = DSP_EFAIL;


	return status;
}


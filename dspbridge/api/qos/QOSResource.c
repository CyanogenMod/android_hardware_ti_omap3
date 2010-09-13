/*
 * dspbridge/src/qos/linux/qos/QOSResource.c
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
    File    QOSResource.c
    Path    $(PROJROOT)\qos\linux\qos
    Desc    Defines QOSResourceFunctionHandler
    Rev     1.0.0
    ============================================================================
    Revision History
    SEP 25, 2005                 Jeff Taylor
	Created.
*/
#include <qosregistry.h>
#include <qosti.h>
#include <stdio.h>
#include <stdlib.h>
#include <_dbdebug.h>

ULONG QOS_Memory_IsAvailable(struct QOSDATA *DataObject, ULONG Parameter1)
{
	struct QOSDATA *data;
	struct QOSRESOURCE_MEMORY *request;
	struct QOSRESOURCE_MEMORY *resource;
	data = (struct QOSDATA *)Parameter1;
	request = (struct QOSRESOURCE_MEMORY *)DataObject;
	if (data->Id == DataObject->Id) {
		/* Update the data before evaluating. */
		DSPQos_TypeSpecific(data, QOS_FN_ResourceUpdateInfo, 0);
		resource = (struct QOSRESOURCE_MEMORY *)data;
		if (resource->heapId == request->heapId)
			if (resource->size > (request->align + request->size))
				return true;
	}
	return false;
}

/*  name        QOSResourceFunctionHandler
	Implementation
		Implements QOS_FN_
	Parameters
		DataObject	Far pointer to the structure for the data object
		FunctionCode	Type-specific function code
		Parameter1		Function-specific parameter
	Return
		ULONG			Function-specific return code.
	Requirement Coverage
		This method addresses requirement(s):  SR10085, SR10008
*/

ULONG
QOS_Memory_Scratch_FunctionHandler(struct QOSDATA *DataObject,
				ULONG FunctionCode, ULONG Parameter1)
{
	struct QOSDATA *data = (struct QOSDATA *)Parameter1;
	struct QOSRESOURCE_MEMORY *request =
				(struct QOSRESOURCE_MEMORY *)DataObject;
	struct QOSRESOURCE_MEMORY *resource;
	int status;
	switch (FunctionCode) {
	case QOS_FN_ResourceIsAvailable:
		if (data->Id == DataObject->Id) {
			resource = (struct QOSRESOURCE_MEMORY *)data;
			if (resource->group == request->group)
				return QOS_Memory_IsAvailable(DataObject,
						Parameter1);
		}
		return false;
	case QOS_FN_ResourceUpdateInfo:
		/* validate scratchType arg */
		status = -EINVAL;
		if ((request->type == ESharedScratchAllHeaps) ||
				(request->type == ESharedScratchDaram) ||
				(request->type == ESharedScratchSaram)) {
			status = QosTI_DspMsg(QOS_TI_GETSHAREDSCRATCH,
				request->type, request->group, ((DWORD *)
				&request->size), ((DWORD *)&request->heapId));
		}
		return status;
	default:
		break;
	}
	return -ENOSYS;
}

ULONG QOS_Memory_DynAlloc_FunctionHandler(struct QOSDATA *DataObject,
					ULONG FunctionCode, ULONG Parameter1)
{
	struct QOSRESOURCE_MEMORY *request =
			(struct QOSRESOURCE_MEMORY *)DataObject;
	int status;
	switch (FunctionCode) {
	case QOS_FN_ResourceIsAvailable:
		return QOS_Memory_IsAvailable(DataObject, Parameter1);
	case QOS_FN_ResourceUpdateInfo:
		/* validate heap arg */
		status = -EINVAL;
		if (request->heapId <= KAllHeaps) {
			/* Use the type of heap requested to get size memory in
			 * use (not free) */
			status = QosTI_DspMsg(QOS_TI_GETMEMSTAT,
			request->heapId, USED_HEAPSIZE, ((DWORD *)
			&request->size), ((DWORD *)&request->allocated));
			/* get size of largest free block */
			/* (in requested heap-type) */
			status = QosTI_DspMsg(QOS_TI_GETMEMSTAT,
				request->heapId, LARGEST_FREE_BLOCKSIZE,
				NULL, ((DWORD *)&request->largestfree));
		}
		return status;
	default:
		break;
	}
	return -ENOSYS;
}

ULONG QOS_Memory_DynLoad_FunctionHandler(struct QOSDATA *DataObject,
				ULONG FunctionCode, ULONG Parameter1)
{
	struct QOSRESOURCE_MEMORY *request =
				(struct QOSRESOURCE_MEMORY *)DataObject;
	UINT memFreeBlocks, memAllocBlocks;
	switch (FunctionCode) {
	case QOS_FN_ResourceIsAvailable:
		return QOS_Memory_IsAvailable(DataObject, Parameter1);
	case QOS_FN_ResourceUpdateInfo:
		return QosTI_GetDynLoaderMemStat(request->heapId,
			&request->size, &request->allocated,
			&request->largestfree, &memFreeBlocks,
			&memAllocBlocks);
	default:
		break;
	}
	return -ENOSYS;
}

ULONG QOS_Processor_FunctionHandler(struct QOSDATA *DataObject,
				ULONG FunctionCode, ULONG Parameter1)
{
	struct QOSRESOURCE_PROCESSOR *request =
				(struct QOSRESOURCE_PROCESSOR *)DataObject;
	struct QOSDATA *data = (struct QOSDATA *) Parameter1;
	struct QOSRESOURCE_PROCESSOR *resource;
	switch (FunctionCode) {
	case QOS_FN_ResourceIsAvailable:
		if (data->Id == DataObject->Id) {
			DSPQos_TypeSpecific(data, QOS_FN_ResourceUpdateInfo, 0);
			resource = (struct QOSRESOURCE_PROCESSOR *)data;
			if (resource->Utilization <= request->Utilization)
				return true;
		}
		return false;
	case QOS_FN_ResourceUpdateInfo:
		QosTI_DspMsg(QOS_TI_GETCPULOAD, 0, 0,
					((DWORD *)&request->Utilization), NULL);
		return QosTI_GetProcLoadStat((&request->currentLoad),
			(&request->predLoad),
			(&request->currDspFreq),
			(&request->predictedFreq));
	default:
		break;
	}
	return -ENOSYS;
}

ULONG QOS_Resource_DefaultFunctionHandler(struct QOSDATA *DataObject,
				ULONG FunctionCode, ULONG Parameter1)
{
	struct QOSDATA *data = (struct QOSDATA *)Parameter1;
	switch (FunctionCode) {
	case QOS_FN_ResourceIsAvailable:
		if (DataObject->Id == data->Id) {
			DSPQos_TypeSpecific(data, QOS_FN_ResourceUpdateInfo, 0);
			return true;
		}
		return false;
	case QOS_FN_ResourceUpdateInfo:
		return 0;
	}
	return -ENOSYS;
}


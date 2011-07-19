/*
 * dspbridge/src/qos/linux/qos/QOSRegistry.c
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
    File    QOSRegistry.c
    Path    $(PROJROOT)\samples\qos\symbian\eka2\qos
    Desc    Defines QOSRegistry_xxx API.
    Rev     1.0.0
    ============================================================================
    Revision History
    SEP 26, 2005                 Jeff Taylor
      Created.
*/

#include <qosregistry.h>
#include <qosti.h>
#include <stdio.h>
#include <stdlib.h>
#include <_dbdebug.h>

const struct QOSDATA standard_datatypes[] = {
	{QOSDataType_Memory_DynLoad, NULL, QOS_Memory_DynLoad_FunctionHandler,
		sizeof(struct QOSRESOURCE_MEMORY)} ,
	{QOSDataType_Memory_DynAlloc, NULL, QOS_Memory_DynAlloc_FunctionHandler,
		sizeof(struct QOSRESOURCE_MEMORY)} ,
	{QOSDataType_Memory_Scratch, NULL, QOS_Memory_Scratch_FunctionHandler,
		sizeof(struct QOSRESOURCE_MEMORY)} ,
	{QOSDataType_Processor_C55X, NULL, QOS_Processor_FunctionHandler,
		sizeof(struct QOSRESOURCE_PROCESSOR)} ,
	{QOSDataType_Processor_C6X, NULL, QOS_Processor_FunctionHandler,
		sizeof(struct QOSRESOURCE_PROCESSOR)} ,
	{QOSDataType_Peripheral_DMA, NULL, QOS_Resource_DefaultFunctionHandler,
		sizeof(struct QOSDATA)} ,
	{QOSDataType_Stream, NULL, QOS_Resource_DefaultFunctionHandler,
		sizeof(struct QOSRESOURCE_STREAM)} ,
	{QOSDataType_Component, NULL, QOS_Component_DefaultFunctionHandler,
		sizeof(struct QOSCOMPONENT)} ,
	{QOSDataType_Registry, NULL, QOS_Registry_FunctionHandler,
		sizeof(struct QOSREGISTRY)} ,
	{QOSDataType_DynDependentLibrary, NULL,
		QOS_DynDependentLibrary_FunctionHandler,
		sizeof(struct QOSDYNDEPLIB)}
};

struct QOSRESOURCE_MEMORY default_memory_resources[] = {
	/* Dynamic Loading heaps */
	{{QOSDataType_Memory_DynLoad, NULL, NULL, 0} , 0, EDynloadDaram, 0, 0,
		0, 0, 0} ,
	{{QOSDataType_Memory_DynLoad, NULL, NULL, 0} , 0, EDynloadSaram, 0, 0,
		0, 0, 0} ,
	{{QOSDataType_Memory_DynLoad, NULL, NULL, 0} , 0, EDynloadExternal, 0,
		0, 0, 0, 0} ,
	/* BIOS Dynamic Allocation heaps */
	{{QOSDataType_Memory_DynAlloc, NULL, NULL, 0} , 0, KAllHeaps, 0, 0, 0,
		0, 0} ,
	/* Shared scratch memory, only type 1 is currently supported */
	{{QOSDataType_Memory_Scratch, NULL, NULL, 0} , 0, 0, 0, 1, 0, 0,
		KAllScratchGroups}
};

struct QOSDATA *DSPData_Create(ULONG id)
{
	struct QOSDATA *data;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPData_Create+\n");
	if (id & QOS_USER_DATA_TYPE)
		/* We don't know what this is! */
		data = NULL;
	else {
		data = calloc(standard_datatypes[id].Size, 1);
		if (data)
			*data = standard_datatypes[id];
	}
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPData_Create-\n");
	return data;
}

int DSPData_Delete(struct QOSDATA *data)
{
	struct QOSCOMPONENT *comp;
	int status = 0;
	struct QOSDATA *temp;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPData_Delete+\n");
	/* Note: This routine is recursive. It calls itself to delete lists of
	 * resources and dependencies.
	 Since resources and dependencies are end conditions,
	 the worst case stack depth is 2.
	 */
	while (data && DSP_SUCCEEDED(status)) {
		switch (data->Id) {
		case QOSDataType_Component:
			comp = (struct QOSCOMPONENT *)data;
			/* Free all the required resources */
			status = DSPData_Delete(comp->resourceList);
			/* Free all the dependency libraries */
			status = DSPData_Delete((struct QOSDATA *)
					comp->dynDepLibList);
			break;
		case QOSDataType_Registry:
			DSPRegistry_Delete((struct QOSREGISTRY *)data);
			break;
		case QOSDataType_DynDependentLibrary:
		case QOSDataType_Memory_DynAlloc:
		case QOSDataType_Memory_DynLoad:
		case QOSDataType_Memory_Scratch:
		case QOSDataType_Peripheral_DMA:
		case QOSDataType_Processor_C55X:
		case QOSDataType_Processor_C6X:
		case QOSDataType_Stream:
			break;
		default:
			/* We don't know what this is! */
			status = -EINVAL;
			break;
		}
		if (DSP_SUCCEEDED(status)) {
			/* If this item is still in a list,
			 * delete the rest of the list, too. */
			temp = data->Next;
			free(data);
			data = temp;
		}
	}
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPData_Delete-\n");
	return 0;
}

bool DSPData_IsResource(ULONG Id)
{
	switch (Id) {
	case QOSDataType_Component:
	case QOSDataType_DynDependentLibrary:
	case QOSDataType_Registry:
		return false;
	default:
		return true;
	}
	return true;
}

/*  ============================================================================
  name        DSPRegistry_Create
	Implementation
		Creates empty Registry, then adds all the default
		system resources
	Parameters
		none
	Return
		QOSREGISTRY*	ptr to new system registry
		NULL			Failure (out of memory)
	Requirement Coverage
		This method addresses requirement(s):  SR10085
*/

struct QOSREGISTRY *DSPRegistry_Create()
{
	int status = 0;
	struct QOSREGISTRY *registry;
	struct QOSDATA *data;
	struct QOSRESOURCE_MEMORY *mem_resource;
	struct QOSRESOURCE_STREAM *stream_resource;
	UINT NumHeaps;
	UINT i;
	UINT NumDefaultMemoryResources;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Create+\n");
	registry = (struct QOSREGISTRY *)DSPData_Create(QOSDataType_Registry);
	if (!registry)
		goto func_end;

	/* Create the QoS Node */
	status = QosTI_Create();
	if (DSP_SUCCEEDED(status)) {
		DbgMsg(DSPAPI_ZONE_TEST, "QoS Node Create succeeded\n");
	} else {
		DbgMsg(DSPAPI_ZONE_TEST, "QoS Node Create failed [%d]\n",
				status);
		free(registry);
		registry = NULL;
	}
	/* Default memory resources */
	NumDefaultMemoryResources = (sizeof(default_memory_resources))
					/ sizeof(struct QOSRESOURCE_MEMORY);
	for (i = 0; i < NumDefaultMemoryResources; i++) {
		if (DSP_SUCCEEDED(status)) {
			mem_resource = (struct QOSRESOURCE_MEMORY *)
					DSPData_Create(
					default_memory_resources[i].data.Id);
			if (mem_resource) {
				/* Copy correct header */
				default_memory_resources[i].data =
					mem_resource->data;
				*mem_resource = default_memory_resources[i];
				status = DSPRegistry_Add(
					(struct QOSDATA *)registry,
					(struct QOSDATA *)mem_resource);
			}
		}
	}
	/* BIOS Dynamic Allocation heaps */
	if (DSP_SUCCEEDED(status)) {
		status = DSPQos_TypeSpecific((struct QOSDATA *)registry,
			QOS_FN_GetNumDynAllocMemHeaps, (ULONG)&NumHeaps);
		for (i = 0; DSP_SUCCEEDED(status) && i < NumHeaps; i++) {
			mem_resource = (struct QOSRESOURCE_MEMORY *)
				DSPData_Create(QOSDataType_Memory_DynAlloc);
			if (mem_resource) {
				mem_resource->heapId = i;
				status = DSPRegistry_Add(
					(struct QOSDATA *)registry,
					(struct QOSDATA *)mem_resource);
			}
		}
	}
	/* Processor */
	if (DSP_SUCCEEDED(status)) {
		data = DSPData_Create(QOSDataType_Processor_C6X);
		if (data)
			status = DSPRegistry_Add((struct QOSDATA *)registry,
				data);

	}
	/* Streams */
	if (DSP_SUCCEEDED(status)) {
		stream_resource = (struct QOSRESOURCE_STREAM *)
				DSPData_Create(QOSDataType_Stream);
		if (stream_resource) {
			stream_resource->Direction = DSP_TONODE;
			status = DSPRegistry_Add((struct QOSDATA *)registry,
					(struct QOSDATA *)stream_resource);
		}
	}
	if (DSP_SUCCEEDED(status)) {
		stream_resource = (struct QOSRESOURCE_STREAM *)
				DSPData_Create(QOSDataType_Stream);
		if (stream_resource) {
			stream_resource->Direction = DSP_FROMNODE;
			status = DSPRegistry_Add((struct QOSDATA *)registry,
					(struct QOSDATA *)stream_resource);
		}
	}
	if (DSP_SUCCEEDED(status)) {
		DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Create-\n");
		return registry;
	}
func_end:
	DSPRegistry_Delete(registry);
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Create-\n");
	return NULL;
}

/*  ============================================================================
  name        DSPRegistry_Delete
	Implementation
	Deletes Registry and cleans up QoS Gateway & Registry
	objects that it owns.
	Parameters
		registry		ptr to previously created registry
	Return
		none
	Requirement Coverage
		This method addresses requirement(s):  SR10085
*/

void DSPRegistry_Delete(struct QOSREGISTRY *registry)
{
	UINT status = 0;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Delete+\n");
	if (registry) {
		/* Clean-up the resource registry list */
		status = DSPData_Delete(registry->ResourceRegistry);
		/* Clean-up the component registry list */
		status = DSPData_Delete(registry->ComponentRegistry);
		/* Shut down QoS socket node */
		QosTI_Delete();
		free(registry);
	}
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Delete-\n");
}

/*  ============================================================================
    name        FindTargetList
	Implementation
		Finds the appropriate target list based on the type of
		the listhead (registry or component) and the Id of an
		item in the list.
	Parameters
		listhead	ptr to object with list head (registry or
				component)
		Id		Type ID of the object to be found or inserted.
	Return
		QOSDATA **		ptr to list head field within registry
						or component
	Requirement Coverage
		This method addresses requirement(s):
*/

struct QOSDATA **FindTargetList(struct QOSDATA *listhead, UINT Id)
{
	struct QOSDATA **target = NULL;
	bool TargetIsResource;
	struct QOSREGISTRY *registry;
	struct QOSCOMPONENT *comp;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "FindTargetList+\n");
	if (listhead) {
		/* Find the target list */
		TargetIsResource = DSPData_IsResource(Id);
		switch (listhead->Id) {
		case QOSDataType_Component:
			DbgMsg(DSPAPI_ZONE_TEST,
				"FindTargetList: QOSDataType_Component\n");
			comp = (struct QOSCOMPONENT *)listhead;
			if (TargetIsResource)
				target = &comp->resourceList;
			else if (Id == QOSDataType_DynDependentLibrary)
				target = (struct QOSDATA **)
						&comp->dynDepLibList;

			break;
		case QOSDataType_Registry:
			DbgMsg(DSPAPI_ZONE_TEST,
				"FindTargetList: QOSDataType_Registry\n");
			registry = (struct QOSREGISTRY *)listhead;
			if (TargetIsResource)
				target = &registry->ResourceRegistry;
			else if (Id == QOSDataType_Component)
				target = &registry->ComponentRegistry;

			break;
		default:
			/* Only a component or registry can be a target */
			break;
		}
	}
	DbgMsg(DSPAPI_ZONE_FUNCTION, "FindTargetList-\n");
	return target;
}

/*  ============================================================================
  name        DSPRegistry_Find
	Implementation
	Finds resource(s) or component(s) that match the given Id. For
	resources, each matching resource's TypeSpecific function is called
	with the function ID QOS_FN_ResourceUpdateInfo to ensure that all
	resources have current data in their structures.
	Parameters
		Id			requested Id
		registry		system registry
		ResultList		ptr to array of QOSDATA pointers
		Size			ptr to ULONG number of matches found
	Return
		0			successful
		-EINVAL			block for results is too small
		-ENOENT			item not found
	Requirement Coverage
		This method addresses requirement(s):  SR10008
*/

int DSPRegistry_Find(UINT Id, struct QOSREGISTRY *registry,
			struct QOSDATA **ResultList, ULONG *Size)
{
	int status = 0;
	struct QOSDATA *target;
	struct QOSDATA **list_ptr;
	bool TargetIsResource = false;
	ULONG EntriesFound = 0;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Find+\n");
	TargetIsResource = DSPData_IsResource(Id);
	list_ptr = FindTargetList((struct QOSDATA *)registry, Id);
	if  (!list_ptr) {
		/* Only resources or components can be
			"found" in the registry */
		status = -EINVAL;
		goto func_end;
	}
	for (target = *list_ptr; target && DSP_SUCCEEDED(status);
				target = target->Next) {
		if (target->Id != Id)
			continue;

		DbgMsg(DSPAPI_ZONE_TEST, "Found a match.\n");
		/* Found a match. */
		if (*Size > EntriesFound) {
			if (TargetIsResource)
				/*Update resource info (only when we can return
				 the ptr) TODO: Perhaps we should add an API
				 function that does not have this side effect!*/
				status = DSPQos_TypeSpecific(target,
					QOS_FN_ResourceUpdateInfo, 0);

			DbgMsg(DSPAPI_ZONE_TEST,
				"Adding ptr to results list\n");
			/* Looks like this will fit. Add it to the return list.
			 ResultList[EntriesFound] will be the pointer to
			 the entry.*/
			ResultList[EntriesFound] = target;
		}
		EntriesFound++;
	}
func_end:
	if (EntriesFound > *Size) {
		*Size = EntriesFound;
		status = -EINVAL;
	} else if (EntriesFound == 0) {
		/* None found */
		status = -ENOENT;
	}
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Find-\n");
	return status;
}

/*  ============================================================================
  name        DSPRegistry_Add
	Implementation
		Add given resource or component to the list
	Parameters
		listhead	ptr to a list container (component or registry)
		entry		entry to add in list
	Return
		int		Error code or 0 for success
	Requirement Coverage
		This method addresses requirement(s):  SR10085
*/

int DSPRegistry_Add(struct QOSDATA *listhead, struct QOSDATA *entry)
{
	struct QOSDATA **target;
	int status = -EINVAL;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Add+\n");
	/* First, find the target list */
	target = FindTargetList(listhead, entry->Id);
	if (target) {
		/* Add to the head of the list */
		entry->Next = *target;
		*target = entry;
		status = 0;
	}
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Add-\n");
	return status;
}

/*  ============================================================================
  name        DSPRegistry_Remove
	Implementation
		Removes given resource or component from the list
	Parameters
		listhead	ptr to a list container (component or registry)
		entry		resource or component to remove
	Return
		int		Error code or 0 for success
	Requirement Coverage
		This method addresses requirement(s):  SR10085
*/

int DSPRegistry_Remove(struct QOSDATA *listhead, struct QOSDATA *entry)
{
	struct QOSDATA *target;
	struct QOSDATA **list_ptr;
	int status = -EINVAL;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Remove+\n");
	/* First, find the target list */
	list_ptr = FindTargetList(listhead, entry->Id);
	if (list_ptr) {
		/* Find the item */
		status = -ENOENT;
		for (target = *list_ptr; target &&
				target != entry && target->Next != entry;
				target = target->Next)
			;
		if (target) {
			if (target == entry)
				/* First entry is the match. Remove it. */
				*list_ptr = entry->Next;

			/* Remove the entry. */
			target->Next = entry->Next;
			entry->Next = NULL;
			status = 0;
		}
	}
	DbgMsg(DSPAPI_ZONE_FUNCTION, "DSPRegistry_Remove-\n");
	return status;
}

/*  ============================================================================
  name        DSPQos_TypeSpecific
	Implementation
	Calls the type-specific function defined for this data type. Internally,
	this is implemented as a call to the QOSDATA structure's TypeSpecific()
	function.
	Parameters
		DataObject	Far pointer to the structure for the data object
		FunctionCode	Type-specific function code
		Parameter1		Function-specific parameter
	Return
		ULONG			Function-specific return code.
	Requirement Coverage
		This method addresses requirement(s):  SR10085, SR10008
*/

ULONG DSPQos_TypeSpecific(struct QOSDATA *DataObject, ULONG FunctionCode,
					ULONG Parameter1)
{
	return DataObject->TypeSpecific(DataObject, FunctionCode, Parameter1);
}

ULONG QOS_Registry_FunctionHandler(struct QOSDATA *DataObject,
					ULONG FunctionCode, ULONG Parameter1)
{
	struct QOSDATA *request;
	struct QOSDATA *data;
	struct QOSREGISTRY *registry;
	bool TargetIsResource;
	int status;
	DbgMsg(DSPAPI_ZONE_FUNCTION, "QOS_Registry_FunctionHandler+\n");
	registry = (struct QOSREGISTRY *)DataObject;
	status = -ENOSYS;
	switch (FunctionCode) {
	case QOS_FN_GetNumDynAllocMemHeaps:
		/* QOS_FN_GetNumDynAllocMemHeaps
		   Implementation
		   This is a Registry-specific function code. Queries the
		   number of dynamic allocation BIOS heaps available on
		   the system.
		   Parameter1
		   UINT *		ptr to storage for number of dynamic
					heaps available
		   Return
		   int   		Error code or 0 for success
		   Requirement Coverage
		   This method addresses requirement(s):        SR10085
		 */
		/* get the number of heaps */
		status = QosTI_DspMsg(QOS_TI_GETMEMSTAT, NUMHEAPS, 0,
			(DWORD *)Parameter1, NULL);
		break;
	case QOS_FN_HasAvailableResource:
		/*  QOS_FN_HasAvailableResource
		   Implementation
		   This is a Registry-specific function code. Ask Registry if
		   requested resource is available based on current state of
		   the system
		   Parameter1
		   QOSDATA *    requested resource or component
		   Return
		   true requested resource (or all resources for component) available
		   false        requested resource (or all resources for component) not
		   available
		   Requirement Coverage
		   This method addresses requirement(s):        SR10085
		 */
		request = (struct QOSDATA *)Parameter1;
		status = false;
		if (request) {
			TargetIsResource = DSPData_IsResource(request->Id);
			if (TargetIsResource) {
				/* Looking for an available resource... */
				for (data = registry->ResourceRegistry; data;
						data = data->Next) {
					if (DSPQos_TypeSpecific(request,
						QOS_FN_ResourceIsAvailable,
							(ULONG)data)) {
						status = true;
						break;
					}
				}
			} else if (request->Id == QOSDataType_Component) {
				/* Looking for all the resources
					to satisfy this component... */
				status = true;
				for (request = ((struct QOSCOMPONENT *)
					request)->resourceList; request &&
					status; request = request->Next) {
					/* Looking for an available resource. */
					status = false;
					for (data = registry->ResourceRegistry;
						data; data = data->Next) {
						if (DSPQos_TypeSpecific(request,
						QOS_FN_ResourceIsAvailable,
								(ULONG) data)) {
							status = true;
							break;
						}
					}
				}
			}
		}
		break;
	}
	DbgMsg(DSPAPI_ZONE_FUNCTION, "QOS_Registry_FunctionHandler-\n");
	return status;
}


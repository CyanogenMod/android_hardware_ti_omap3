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
 *  ======== qostest.c ========
 *  Description:
 *     Console application to test the QoS services.
 *
 *  Requires:
 *
 *  Usage:
 *      qostest.out
 *
 *  Notes:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dbapi.h>		/* DSP/BIOS Bridge APIs                  */
#include <qosregistry.h>
#include <qosti.h>

#define DEFAULTDELAY  "50"	/* default ping delay                    */
#define DEFAULTMSGS   50	/* default message count                 */
#define DSPUNIT       0		/* Default (first) DSP ID                */
#define ARGSIZE       32	/* Size of arguments to Ping node.       */
#define MAXNAMELEN    64	/* Max length of event name.             */
#define MAXMSGLEN     128	/* Max length of MessageBox msg.         */

/*
 *  ======== main ========
 */
int main(int argc, char **argv)
{
#if 0
	PING_TASK pingTask;	/* Ping task context                    */
	UINT msgCount;		/* Number of messages DSP sends         */
	PING_NODEDATA argsBuf;	/* Task Node args.                      */
#endif
	DSP_STATUS status = DSP_SOK;
	struct QOSREGISTRY *registry;
	struct QOSDATA **results = NULL;
	ULONG NumFound;
	UINT id;
	struct QOSDATA *data;
	struct QOSRESOURCE_MEMORY *m;
	struct QOSRESOURCE_STREAM *s;
	struct QOSRESOURCE_PROCESSOR *p;
	ULONG i;
	bool MemoryIsPresent = false;
	bool ProcessorIsPresent = false;
	struct QOSCOMPONENT *comp;
	struct QOSDYNDEPLIB *dyndep;
	ULONG NumTestsPassed = 0;
	ULONG NumRegistryTestsPassed = 0;

	printf("\n****************************\n");
	printf("*** QOS Test version 1.0 ***\n");
	printf("****************************\n");

	printf
	    ("\n*** TEST CASE 1: Creating and Deleting data structures ***\n");
	printf("Calling data create...");
	data = DSPData_Create(QOSDataType_Memory_DynAlloc);
	if (data) {
		printf("SUCCESS\n");
		printf("Calling data delete...");
		status = DSPData_Delete(data);
		if (DSP_SUCCEEDED(status)) {
			printf("SUCCESS\n");
		} else {
			printf("FAILED\n");
		}
	} else {
		printf("FAILED\n");
	}

	if (data && DSP_SUCCEEDED(status)) {
		printf("\nPASSED: data structures can be created and deleted\n");
		NumTestsPassed++;
		NumRegistryTestsPassed++;
	} else {
		printf("\nFAILED: failed to create and delete data structures\n");
	}

	printf("\n*** TEST CASE 2: Enumerating Resources ***\n");
	printf("Calling registry create...\n\n");
	registry = DSPRegistry_Create();
	if ( !registry) {
		printf("FAILED\n");
		goto func_end;
	}
	printf("SUCCESS: Created Registry\n");
	for (i = 3; i > 0; i--) {
		printf("Waiting for nodes to get past startup...%d\n",(int) i);
		sleep(1);
	}
	printf("Continuing...\n");
	printf("Reading Resource Registry...\n\n");
	for (id = QOSDataType_Memory_DynLoad;id < QOSDataType_Component; id++) {
		printf("RESOURCE TYPE %d ", id);
		switch (id) {
			case QOSDataType_Memory_DynAlloc:
			case QOSDataType_Memory_DynLoad:
			case QOSDataType_Memory_Scratch:
				printf("(Memory): \n");
				break;
			case QOSDataType_Peripheral_DMA:
				printf("(DMA): \n");
				break;
			case QOSDataType_Stream:
				printf("(Stream): \n");
				break;
			case QOSDataType_Processor_C55X:
			case QOSDataType_Processor_C6X:
				printf("(Processor): \n");
				break;
			default:
				printf("(Unknown): \n");
				break;
		}
		results = NULL;
		NumFound = 0;
		status = DSPRegistry_Find(id, registry, results, &NumFound);
		if ( !DSP_SUCCEEDED(status) && status != DSP_ESIZE) {
			printf("None.\n\n");
			continue;
		}
		results = malloc(NumFound * sizeof (struct QOSDATA *));
		if (!results) {
			printf("FAILED (out of memory)\n\n");
			continue;
		}
		status = DSPRegistry_Find(id, registry, results, &NumFound);
		if (DSP_SUCCEEDED(status)) {
			printf("Found %lu\n", NumFound);
		} else {
			NumFound = 0;
			printf("None.\n");
		}
		for (i = 0; i < NumFound; i++) {
			/* Display info for the resources */
			switch (id) {
				case QOSDataType_Memory_DynAlloc:
				case QOSDataType_Memory_DynLoad:
				case QOSDataType_Memory_Scratch:
					MemoryIsPresent = true;
					m = (struct  QOSRESOURCE_MEMORY *) results[i];
					printf("%d: align=%d,heap=%d,size=%d,type=%d,used =%d,"
						"MaxFree= %d,group=%d\n",(int)i,(int)m->align,
						(int)m->heapId,(int)m->size,(int)m->type,
						(int)m->allocated,(int)m->largestfree,(int)m->group);
				break;
				case QOSDataType_Stream:
					s = (struct  QOSRESOURCE_STREAM *) results[i];
					printf("  %d: Direction=%d\n", (int)i, (int)s->Direction);
				break;
				case QOSDataType_Processor_C55X:
				case QOSDataType_Processor_C6X:
					ProcessorIsPresent = true;
					p = (struct  QOSRESOURCE_PROCESSOR *) results[i];
					printf ("%%Used= %d, %%current Load= %d, %%predicted Load"
						   "= %d, current frequency =%d, predicted frequency=%d\n", 
					          (int)p->Utilization, (int)p->currentLoad,(int)p->predLoad,
					          (int)p->currDspFreq, (int)p->predictedFreq);
					break;
				default:
					break;
			}
		}
		printf("\n");
		free(results);
	}
	if (MemoryIsPresent && ProcessorIsPresent) {
		printf("PASSED: resource enumeration - Memory and Processor are "
														"both present.\n");
		NumTestsPassed++;
		NumRegistryTestsPassed++;
	} else {
		printf("FAILED: resource enumeration - Either memory or "
											"processor was not present.\n");
	}
	printf("\n*** TEST CASE 3: Creating, registering, and deleting "
														"a component ***\n");
	printf("Creating required data structures...");
	MemoryIsPresent = false;
	status = DSP_EFAIL;
	comp = (struct QOSCOMPONENT *)DSPData_Create(QOSDataType_Component);
	dyndep =(struct QOSDYNDEPLIB *)DSPData_Create(
											QOSDataType_DynDependentLibrary);
	m = (struct QOSRESOURCE_MEMORY *)DSPData_Create(
												QOSDataType_Memory_DynAlloc);
	p = (struct QOSRESOURCE_PROCESSOR *)DSPData_Create(
												QOSDataType_Processor_C6X);
	if (comp && dyndep && m && p) {
		printf("SUCCESS\n");
		printf("Adding component to the registry...");
		status = DSPRegistry_Add((struct QOSDATA *)registry,
														(struct QOSDATA *)comp);
		if (!DSP_SUCCEEDED(status)) {
			goto func_cont;
		}
		printf("SUCCESS\n");
		printf ("Adding dynamic dependency to component...");
		status = DSPRegistry_Add((struct QOSDATA *)comp,
												(struct QOSDATA *)dyndep);
		if ( !DSP_SUCCEEDED(status)) {
			goto func_cont;
		}
		/* dyndep is now managed by the component...*/
		printf("SUCCESS\n");
		dyndep = NULL;
		m->align = 4;
		m->heapId = KAllHeaps;
		m->size = 4096;
		p->Utilization = 10;
		printf("Adding memory resource requirement to component..");
		status = DSPRegistry_Add((struct QOSDATA *)comp,(struct QOSDATA *)m);
		if ( !DSP_SUCCEEDED(status)) {
			goto func_cont;
		}
		/* m is now managed by the component... */
		printf("SUCCESS\n");
		m = NULL;
		printf ("Adding processor resource requirement to component...");
		status = DSPRegistry_Add((struct QOSDATA *)comp, (struct QOSDATA *)p);
		if ( !DSP_SUCCEEDED(status)) {
			goto func_cont;
		}
		/* p is now managed by the component... */
		printf("SUCCESS\n");
		p = NULL;
		printf ("Registering component...");
		status = DSPComponent_Register(registry, comp);
		if ( !DSP_SUCCEEDED (status)) {
			goto func_cont;
		}
		printf ("SUCCESS\n");
		printf("Querying registry for availability "
										"of component's requirements...");
		MemoryIsPresent = DSPQos_TypeSpecific((struct QOSDATA *)registry,
							QOS_FN_HasAvailableResource,(ULONG)comp);
		printf((MemoryIsPresent) ? ("SUCCESS\n") : ("FAILED\n"));
		printf("Unregistering component...");
		status = DSPComponent_Unregister(registry,comp);
		if ( !DSP_SUCCEEDED(status)) {
			goto func_cont;
		}
		printf("SUCCESS\n");
		printf("Removing component from the registry...");
		status = DSPRegistry_Remove((struct QOSDATA *)registry,
														(struct QOSDATA *)comp);
		if ( !DSP_SUCCEEDED(status)) {
			goto func_cont;
		}
		printf("SUCCESS\n");
		/* This is successful for registry 
		 * accesses, even if requirements not
		 * available.*/
		NumRegistryTestsPassed++;
	}
func_cont:
	if (DSP_FAILED(status)) {
		printf("FAILED\n");
		MemoryIsPresent = false;/* Force the failure for this test case*/
	}
	printf("\nMemory Cleanup:\n");
	if (m) {
		DSPRegistry_Remove((struct QOSDATA *)comp, (struct QOSDATA *)m);
		printf("Deleting orphaned memory resource...");
		status = DSPData_Delete((struct QOSDATA *) m);
		if (DSP_SUCCEEDED(status)) {
			printf("SUCCESS\n");
		} else {
			printf("FAILED\n");
			MemoryIsPresent = false;/*Force the failure for this test case*/
		}
	}
	if (p) {
		DSPRegistry_Remove((struct QOSDATA *)comp, (struct QOSDATA *)p);
		printf("Deleting orphaned processor resource...");
		status = DSPData_Delete((struct QOSDATA *)p);
		if (DSP_SUCCEEDED(status)) {
			printf("SUCCESS\n");
		} else {
			printf("FAILED\n");
			MemoryIsPresent = false;/* Force the failure for this test case */
		}
	}
	if (dyndep) {
		DSPRegistry_Remove((struct QOSDATA *)comp, (struct QOSDATA *)dyndep);
		printf("Deleting orphaned dynamic dependency library...");
		status = DSPData_Delete((struct QOSDATA *)dyndep);
		if (DSP_SUCCEEDED(status)) {
			printf("SUCCESS\n");
		} else {
			printf("FAILED\n");
			MemoryIsPresent = false;/* Force the failure for this test case */
		}
	}
	if (comp) {
		printf("Deleting component and all its lists...");
		status = DSPData_Delete((struct QOSDATA *)comp);
		if (DSP_SUCCEEDED(status)) {
			printf("SUCCESS\n");
		} else {
			printf("FAILED\n");
			MemoryIsPresent = false;/*Force the failure for this test case*/
		}
	}
	if (MemoryIsPresent && DSP_SUCCEEDED(status)) {
		printf("\nPASSED: All registry calls were successful!\n");
		NumTestsPassed++;
	} else {
		printf("\nFAILED: Registry calls were not successful\n");
	}
	printf("\n*** TEST CASE 4: Getting number of Dynamic Allocation "
														"Memory Heaps ***\n");
	status = DSPQos_TypeSpecific((struct QOSDATA *)registry,
								QOS_FN_GetNumDynAllocMemHeaps,(ULONG)&NumFound);
	if (DSP_SUCCEEDED(status)) {
		printf("\nPASSED: NumHeaps = %ld\n", NumFound);
		NumTestsPassed++;
		NumRegistryTestsPassed++;
	} else {
		printf("\nFAILED\n");
	}
	printf("\n*** TEST CASE 5: Querying the database for required "
														"resources ***\n");
	printf("Calling data create...");
	MemoryIsPresent = false;
	ProcessorIsPresent = false;
	m= (struct QOSRESOURCE_MEMORY *)DSPData_Create(QOSDataType_Memory_DynAlloc);
	if (m) {
		printf("SUCCESS\n");
		m->align = 4;
		m->heapId = KAllHeaps;
		m->size = 4096;
		printf("Requesting 4096 byte memory resource...");
		MemoryIsPresent = DSPQos_TypeSpecific((struct QOSDATA *)registry,
							QOS_FN_HasAvailableResource, (ULONG)m);
		printf((MemoryIsPresent) ? ("SUCCESS\n") : ("FAILED\n"));
		printf("Calling data delete...");
		status = DSPData_Delete((struct QOSDATA *) m);
		printf(DSP_SUCCEEDED(status) ? ("SUCCESS\n") : ("FAILED\n"));
	}
	printf("Calling data create...");
	p = (struct QOSRESOURCE_PROCESSOR *)
									DSPData_Create(QOSDataType_Processor_C6X);
	if (p) {
		printf("SUCCESS\n");
		p->Utilization = 10;
		printf("Requesting 10%% of C6x processor...");
		ProcessorIsPresent = DSPQos_TypeSpecific((struct QOSDATA *)registry,
							QOS_FN_HasAvailableResource, (ULONG)p);
		printf(ProcessorIsPresent ? ("SUCCESS\n") : ("FAILED\n"));
		printf("Calling data delete...");
		status = DSPData_Delete((struct QOSDATA *)p);
		printf(DSP_SUCCEEDED(status) ? ("SUCCESS\n") : ("FAILED\n"));
	}
	if (MemoryIsPresent && ProcessorIsPresent) {
		printf("\nPASSED: Querying the database for required resources\n");
		NumTestsPassed++;
	} else {
		printf("\nFAILED: Querying the database for required resources\n");
	}
	printf("\n*** TEST CASES END: Calling registry delete... ***\n");
	DSPRegistry_Delete(registry);
	printf("DONE.\n");
func_end:
	printf("\n****************************\n");
	printf("***** END OF TEST RUN ******\n");
	printf("****************************\n");
	printf("*** %ld/%d Qos Test Cases PASSED\n", NumTestsPassed, 5);
	printf("*** %ld/%d Registry Test Cases PASSED\n", NumRegistryTestsPassed,4);
	if (NumTestsPassed < 5) {
		printf("Some SR's failed!!!\n");
		if (NumRegistryTestsPassed < 4) {
			printf("Registry SR FAILED!!!\n");
		} else {
			printf("Registry SR PASSED!!!\n");
		}
	} else {
		printf("All SR's verified!!!\n");
	}

	return (DSP_SUCCEEDED(status) ? 0 : -1);
}



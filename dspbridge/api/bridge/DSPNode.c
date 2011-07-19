/*
 * dspbridge/src/api/linux/DSPNode.c
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


/*
 *  ======== DSPNode.c ========
 *  Description:
 *      This is the source for the DSP/BIOS Bridge API node module. The
 *      parameters are validated at the API level, but the bulk of the
 *      work is done at the driver level through the RM NODE module.
 *
 *  Public Functions:
 *      DSPNode_Allocate
 *      DSPNode_AllocMsgBuf
 *      DSPNode_ChangePriority
 *      DSPNode_Connect
 *      DSPNode_ConnectEx
 *      DSPNode_Create
 *      DSPNode_Delete
 *      DSPNode_FreeMsgBuf
 *      DSPNode_GetAttr
 *      DSPNode_GetMessage
 *      DSPNode_Pause
 *      DSPNode_PutMessage
 *      DSPNode_RegisterNotify
 *      DSPNode_Run
 *      DSPNode_Terminate
 *
 *! Revision History
 *! ================
 *! 14-Mar-2002 map Set *pBuffer to null before returning error status in
 *!		            DSPNode_AllocMsgBuf.
 *! 01-Oct-2001 rr  CMM error codes are converted to DSP_STATUS in
 *!                 DSPNode_Allocate.
 *! 11-Sep-2001 ag  Zero-copy message support.
 *! 08-Jun-2001 jeh Fixed priority range check in DSPNode_ChangePriority.
 *! 23-Apr-2001 jeh Added pStatus parameter to DSPNode_Terminate.
 *! 06-Feb-2001 kc: Added check for alignment value in DSPNode_AllocMsgBuf
 *! 08-Dec-2000 ag  Added alignment to DSPNode_AllocMsgBuf().
 *! 05-Dec-2000 ag  Added SM support to DSPNode_[Alloc][Free]MsgBuf().
 *! 09-Nov-2000 rr: Code cleaned up. Use of IsValidEvent/Mask Macros.
 *! 27-Oct-2000 jeh Updated to version 0.9 of API spec.
 *! 07-Sep-2000 jeh Changed type HANDLE in DSPNode_RegisterNotify to
 *!                 DSP_HNOTIFICATION. Added DSP_STRMATTR param to
 *!                 DSPNode_Connect.
 *! 04-Aug-2000 rr: Name changed to DSPNode.c
 *! 27-Jul-2000 rr: Types updated to ver 0.8 API.
 *! 18-Jul-2000 rr: Node calls into the Class driver.
 *!                 Only parameters are validated here.
 *! 17-May-2000 rr: DSPNode_Connect checks for GHPPNODE.
 *! 15-May-2000 gp: Made input args to DSPNode_Allocate() CONST.
 *!                 Return DSP_ENOTIMPL from DSPNode_ChangePriority().
 *! 02-May-2000 rr: Reg functions use SERVICES.
 *! 12-Apr-2000 ww: Created based on DirectDSP API specification, Version 0.6.
 *
 */

/*  ----------------------------------- Host OS */
#include <host_os.h>
#include <stdlib.h>
#include <malloc.h>

/*  ----------------------------------- DSP/BIOS Bridge */
#include <dbdefs.h>
#include <errno.h>

/*  ----------------------------------- Trace & Debug */
#include <dbg.h>
#include <dbg_zones.h>

/*  ----------------------------------- Resource Manager */
#include <memry.h>

/*  ----------------------------------- Others */
#include <dsptrap.h>
/*  ----------------------------------- This */
#include "_dbdebug.h"
#include "_dbpriv.h"

#include <DSPNode.h>

#ifdef DEBUG_BRIDGE_PERF
#include <perfutils.h>
#endif

static struct mmap_element *mmaplist;
static sem_t sem_mmap;
/*  ----------------------------------- Globals */
extern int hMediaFile;		/* class driver handle */

static void start(void) __attribute__((constructor));
int insert_mmapelement(struct mmap_element *elem,
			struct mmap_element **mmaplist);
int delete_mmapelement(BYTE *vb, struct mmap_element **mmaplist);


void start(void)
{
	if (sem_init(&sem_mmap, 0, 1) == -1)
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("NODE: Failed to Initialize"
				"the MMAP semaphore\n")));
}

/* Declared here, not to users */
int GetNodeType(DSP_HNODE hNode, DSP_NODETYPE *pNodeType);

/*
 *  ======== DSPNode_Allocate ========
 *  Purpose:
 *      Allocate data structures for controlling and communicating
 *      with a node on a specific DSP processor..
 */
DBAPI DSPNode_Allocate(DSP_HPROCESSOR hProcessor,
		 IN CONST struct DSP_UUID *pNodeID,
		 IN CONST OPTIONAL struct DSP_CBDATA *pArgs,
		 IN OPTIONAL struct DSP_NODEATTRIN *pAttrIn,
		 OUT DSP_HNODE *phNode)
{
	int status = 0;
	Trapped_Args tempStruct;
	struct CMM_OBJECT *hCmm;		/* shared memory mngr handle */
	struct CMM_INFO pInfo;		/* Used for virtual space allocation */
	PVOID pVirtBase;
	struct DSP_BUFFERATTR bufAttr;
	struct mmap_element *pelement;
    DSP_NODETYPE nodeType;
    struct DSP_NDBPROPS    nodeProps;
    UINT            heapSize = 0;
    PVOID           pGPPVirtAddr = NULL;
    UINT            uProfileID;
	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_Allocate:\r\n")));
	if (!hProcessor) {
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("NODE: DSPNode_Allocate: "
				"hProcessor is Invalid \r\n")));
		goto func_end;
	}
	if (!(pNodeID) || !(phNode)) {
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("NODE: DSPNode_Allocate: "
			"Invalid pointer in the Input\r\n")));
		goto func_end;
	}
	/* First get the NODE properties, allocate, reserve
				memory for Node heap */
	if (pAttrIn) {
		status = DSPNode_GetUUIDProps(hProcessor, pNodeID, &nodeProps);
		pAttrIn->pGPPVirtAddr = NULL;
		if (DSP_SUCCEEDED(status)) {
			uProfileID = pAttrIn->uProfileID;
			DEBUGMSG(DSPAPI_ZONE_FUNCTION,
					("DSPNodeAllocate: User requested"
						  "node heap profile \n"));
			if (uProfileID < nodeProps.uCountProfiles)
				heapSize =
				nodeProps.aProfiles[uProfileID].ulHeapSize;
			if (heapSize) {
				/* allocate heap memory */
				/* Make heap size multiple of page size * */
				heapSize = PG_ALIGN_HIGH(heapSize, PG_SIZE_4K);
				/* align memory on cache line boundary * */
				pGPPVirtAddr = memalign(GEM_CACHE_LINE_SIZE,
							heapSize);
				DEBUGMSG(DSPAPI_ZONE_FUNCTION,
					("DSPNodeAllocate: Node heap memory"
							  "addr, size \n"));
				if ((pGPPVirtAddr == NULL))
					status = -ENOMEM;
				pAttrIn->uHeapSize = heapSize;
				pAttrIn->pGPPVirtAddr = pGPPVirtAddr;
			}
		} else {
			DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT(
				"NODE:DSPNode_Allocate: Failed to get Node "
				"UUID properties \r\n")));
		}
	}
	if (DSP_SUCCEEDED(status)) {
		/* Set up the structure  Call DSP Trap */
		tempStruct.ARGS_NODE_ALLOCATE.hProcessor = hProcessor;
		tempStruct.ARGS_NODE_ALLOCATE.pNodeID =
						(struct DSP_UUID *)pNodeID;
		tempStruct.ARGS_NODE_ALLOCATE.pArgs =
						(struct DSP_CBDATA *)pArgs;
		tempStruct.ARGS_NODE_ALLOCATE.pAttrIn =
					(struct DSP_NODEATTRIN *)pAttrIn;
		tempStruct.ARGS_NODE_ALLOCATE.phNode = phNode;
		status = DSPTRAP_Trap(&tempStruct, CMD_NODE_ALLOCATE_OFFSET);
	}
	 /* If 1st SM segment is configured then allocate and map it to
		this process.*/
	if (DSP_FAILED(status))
		goto func_end;

	tempStruct.ARGS_CMM_GETHANDLE.hProcessor = hProcessor;
	tempStruct.ARGS_CMM_GETHANDLE.phCmmMgr = &hCmm;
	status = DSPTRAP_Trap(&tempStruct, CMD_CMM_GETHANDLE_OFFSET);
	if (DSP_SUCCEEDED(status)) {
		/* Get SM segment info from CMM */
		tempStruct.ARGS_CMM_GETINFO.hCmmMgr = hCmm;
		tempStruct.ARGS_CMM_GETINFO.pCmmInfo = &pInfo;
		status = DSPTRAP_Trap(&tempStruct, CMD_CMM_GETINFO_OFFSET);
		if (DSP_FAILED(status)) {
			status = -EPERM;
			DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT(
			"NODE: DSPNode_Allocate: "
			"Failed to get SM segment\r\n")));
		} else
			status = 0;

	} else {
		status = -EPERM;
		DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT(
			"NODE: DSPNode_Allocate:Failed to CMM handle\r\n")));
	}
	if (DSP_FAILED(status))
		goto func_end;

	GetNodeType(*phNode, &nodeType);
	if ((nodeType != NODE_DEVICE) && (pInfo.ulNumGPPSMSegs > 0)) {
		/* Messaging uses 1st segment */
		if ((pInfo.segInfo[0].dwSegBasePa != 0) &&
		    (pInfo.segInfo[0].ulTotalSegSize) > 0) {
			pVirtBase = mmap(NULL, pInfo.segInfo[0].ulTotalSegSize,
				 PROT_READ | PROT_WRITE, MAP_SHARED |
				 MAP_LOCKED, hMediaFile,
				 pInfo.segInfo[0].dwSegBasePa);

			if (!pVirtBase) {
				DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("NODE: "
				"DSPNode_Allocate:Virt alloc failed\r\n")));
				status = -ENOMEM;
				/* Clean up */
				tempStruct.ARGS_NODE_DELETE.hNode = *phNode;
				DSPTRAP_Trap(&tempStruct,
					CMD_NODE_DELETE_OFFSET);
				goto func_end;
			}
			/* set node translator's virt addr range for seg */
			bufAttr.uAlignment = 0;
			bufAttr.uSegment = 1 | MEMRY_SETVIRTUALSEGID;
			bufAttr.cbStruct = 0;
			status = DSPNode_AllocMsgBuf(*phNode,
					pInfo.segInfo[0].ulTotalSegSize,
					&bufAttr, (BYTE **)&pVirtBase);
			if (DSP_FAILED(status)) {
				/* If failed to set segment, unmap */
				munmap(pVirtBase,
					pInfo.segInfo[0].ulTotalSegSize);
				/* Clean up */
				tempStruct.ARGS_NODE_DELETE.hNode = *phNode;
				DSPTRAP_Trap(&tempStruct,
					CMD_NODE_DELETE_OFFSET);
				goto func_end;
			}

			pelement = malloc(sizeof(struct mmap_element));
			if (!pelement) {
				fprintf(stdout, "DSPNODE:Alloc for mmap element failed\n");
			} else {
				pelement->virt_base = (BYTE *)pVirtBase;
				pelement->seg_size = pInfo.segInfo[0].ulTotalSegSize;
				insert_mmapelement(pelement, &mmaplist);
			}
		}
	}

func_end:
	if (DSP_FAILED(status) && pGPPVirtAddr)
		free(pGPPVirtAddr);

    return status;
}

int insert_mmapelement(struct mmap_element *elem, struct mmap_element **mmaplist)
{
	sem_wait(&sem_mmap);
	elem->next = *mmaplist;
	*mmaplist = elem;
	sem_post(&sem_mmap);

	return 0;
}

int delete_mmapelement(BYTE *vb, struct mmap_element **mmaplist)
{
	int ret = 0;
	struct mmap_element *tmp = *mmaplist, *tmp2 = NULL;

	sem_wait(&sem_mmap);
	for (; tmp && tmp->virt_base != vb; tmp = tmp->next)
		tmp2 = tmp;

	if (!tmp) {
		ret = -1;
		goto func_end;
	} else if (!tmp2)
		*mmaplist = tmp->next;
	else
		tmp2->next = tmp->next;

	free(tmp);

func_end:
	sem_post(&sem_mmap);

	return ret;
}

void munmap_all(void)
{
	struct mmap_element *tmp;

	sem_wait(&sem_mmap);

	while (mmaplist) {
		munmap(mmaplist->virt_base, mmaplist->seg_size);
		tmp = mmaplist;
		mmaplist = mmaplist->next;
		free(tmp);
	}
	sem_post(&sem_mmap);
}
/*
 *  ======== DSPNode_AllocMsgBuf ========
 */
DBAPI DSPNode_AllocMsgBuf(DSP_HNODE hNode, UINT uSize,
		   IN OPTIONAL struct DSP_BUFFERATTR *pAttr, OUT BYTE **pBuffer)
{
	int status = 0;
	Trapped_Args tempStruct;
	DEBUGMSG(DSPAPI_ZONE_FUNCTION,
		(TEXT("NODE: DSPNode_AllocMsgBuf:\r\n")));

	if (uSize == 0) {
		status = -EINVAL;
		if (pBuffer)
			*pBuffer = NULL;

	} else if (hNode) {
		if (pBuffer) {
			/* Set up the structure */
			tempStruct.ARGS_NODE_ALLOCMSGBUF.hNode = hNode;
			tempStruct.ARGS_NODE_ALLOCMSGBUF.uSize = uSize;
			tempStruct.ARGS_NODE_ALLOCMSGBUF.pAttr = pAttr;
			/* Va Base */
			tempStruct.ARGS_NODE_ALLOCMSGBUF.pBuffer = pBuffer;
			/* Call DSP Trap */
			status = DSPTRAP_Trap(&tempStruct,
				CMD_NODE_ALLOCMSGBUF_OFFSET);
			if (DSP_SUCCEEDED(status)) {
				if (*pBuffer == NULL) {
					DEBUGMSG(DSPAPI_ZONE_FUNCTION,
					(TEXT("NODE: DSPNode_AllocMsgBuf: "
					"No SM\r\n")));
					status = -ENOMEM;	/* No SM */
				}
			} else
				*pBuffer = NULL;

		} else {
			/* Invalid pointer */
			status = -EFAULT;
			DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("NODE: "
			"DSPNode_AllocBuf: Invalid pointer in the Input\r\n")));
		}
	} else {
		/* Invalid handle */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("NODE: DSPNode_AllocMsgBuf: "
						"hNode is Invalid \r\n")));
		if (pBuffer)
			*pBuffer = NULL;

	}

	return status;
}

/*
 *  ======== DSPNode_ChangePriority ========
 *  Purpose:
 *      Change a task node's runtime priority within the DSP RTOS.
 */
DBAPI DSPNode_ChangePriority(DSP_HNODE hNode, INT iPriority)
{
	int status = 0;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION,
			(TEXT("NODE: DSPNode_ChangePriority:\r\n")));

	if (hNode) {
		/* Set up the structure */
		if (iPriority >= DSP_NODE_MIN_PRIORITY &&
		    iPriority <= DSP_NODE_MAX_PRIORITY) {
			/* Call DSP Trap */
			tempStruct.ARGS_NODE_CHANGEPRIORITY.hNode = hNode;
			tempStruct.ARGS_NODE_CHANGEPRIORITY.iPriority =
							iPriority;
			status = DSPTRAP_Trap(&tempStruct,
					CMD_NODE_CHANGEPRIORITY_OFFSET);
		} else
			status = -EDOM;

	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("NODE: DSPNode_ChangePriority: "
			"hNode is Invalid \r\n")));
	}

	return status;
}

/*
 *  ======== DSPNode_Connect ========
 *  Purpose:
 *      Make a stream connection, either between two nodes on a DSP,
 *      or between a node on a DSP and the GPP.
 */
DBAPI DSPNode_Connect(DSP_HNODE hNode, UINT uStream, DSP_HNODE hOtherNode,
		UINT uOtherStream, IN OPTIONAL struct DSP_STRMATTR *pAttrs)
{
	return DSPNode_ConnectEx(hNode, uStream, hOtherNode, uOtherStream,
				 pAttrs, NULL);
}

/*
 *  ======== DSPNode_ConnectEx ========
 *  Purpose:
 *      Make a stream connection, either between two nodes on a DSP,
 *      or between a node on a DSP and the GPP.
 */
DBAPI DSPNode_ConnectEx(DSP_HNODE hNode, UINT uStream, DSP_HNODE hOtherNode,
		  UINT uOtherStream, IN OPTIONAL struct DSP_STRMATTR *pAttrs,
		  IN OPTIONAL struct DSP_CBDATA *pConnParam)
{
	int status = 0;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_ConnectEx:\r\n")));

	if ((hNode) && (hOtherNode)) {
		/* Set up the structure */
		/* Call DSP Trap */
		tempStruct.ARGS_NODE_CONNECT.hNode = hNode;
		tempStruct.ARGS_NODE_CONNECT.uStream = uStream;
		tempStruct.ARGS_NODE_CONNECT.hOtherNode = hOtherNode;
		tempStruct.ARGS_NODE_CONNECT.uOtherStream = uOtherStream;
		tempStruct.ARGS_NODE_CONNECT.pAttrs = pAttrs;
		tempStruct.ARGS_NODE_CONNECT.pConnParam = pConnParam;
		status = DSPTRAP_Trap(&tempStruct, CMD_NODE_CONNECT_OFFSET);
	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("NODE: DSPNode_Connect: "
		"hNode or hOtherNode is Invalid Handle\r\n")));
	}

	return status;
}

/*
 *  ======== DSPNode_Create ========
 *  Purpose:
 *      Create a node in a pre-run (i.e., inactive) state on its
 *		DSP processor.
 */
DBAPI DSPNode_Create(DSP_HNODE hNode)
{
	int status = 0;
	Trapped_Args tempStruct;
#ifdef DEBUG_BRIDGE_PERF
	struct timeval tv_beg;
	struct timeval tv_end;
	struct timezone tz;
	int timeRetVal = 0;

	timeRetVal = getTimeStamp(&tv_beg);
#endif


	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_Create:\r\n")));

	if (hNode) {
		/* Set up the structure */
		/* Call DSP Trap */
		tempStruct.ARGS_NODE_CREATE.hNode = hNode;
		status = DSPTRAP_Trap(&tempStruct, CMD_NODE_CREATE_OFFSET);
	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
		(TEXT("NODE: DSPNode_Create: hNode is Invalid Handle\r\n")));
	}

#ifdef DEBUG_BRIDGE_PERF
	timeRetVal = getTimeStamp(&tv_end);
	PrintStatistics(&tv_beg, &tv_end, "DSPNode_Create", 0);

#endif

	return status;
}

/*
 *  ======== DSPNode_Delete ========
 *  Purpose:
 *      Delete all DSP-side and GPP-side resources for the node.
 */
DBAPI DSPNode_Delete(DSP_HNODE hNode)
{
	int status = 0;
	Trapped_Args tempStruct;
	BYTE *pVirtBase = NULL;
	struct DSP_BUFFERATTR bufAttr;
	struct CMM_OBJECT *hCmm;		/* shared memory mngr handle */
	struct CMM_INFO pInfo;		/* Used for virtual space allocation */
	DSP_NODETYPE nodeType;
	struct DSP_NODEATTR    nodeAttr;
#ifdef DEBUG_BRIDGE_PERF
	struct timeval tv_beg;
	struct timeval tv_end;
	struct timezone tz;
	int timeRetVal = 0;

	timeRetVal = getTimeStamp(&tv_beg);
#endif

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_Delete:\r\n")));
	if (!hNode) {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("NODE: DSPNode_Delete: "
					"hNode is Invalid Handle\r\n")));
		return status;
	}
	/* Get segment size.
	 >0 is SM segment. Get default SM Mgr*/
	tempStruct.ARGS_CMM_GETHANDLE.hProcessor = NULL;
	tempStruct.ARGS_CMM_GETHANDLE.phCmmMgr = &hCmm;
	status = DSPTRAP_Trap(&tempStruct, CMD_CMM_GETHANDLE_OFFSET);
	if (DSP_SUCCEEDED(status)) {
		/* Get SM segment info from CMM */
		tempStruct.ARGS_CMM_GETINFO.hCmmMgr = hCmm;
		tempStruct.ARGS_CMM_GETINFO.pCmmInfo = &pInfo;
		status = DSPTRAP_Trap(&tempStruct, CMD_CMM_GETINFO_OFFSET);
		if (DSP_FAILED(status)) {
			status = -EPERM;
			DEBUGMSG(DSPAPI_ZONE_ERROR,
				(TEXT("NODE: DSPNode_Delete:"
					" Failed to get SM segment\r\n")));
		} else
			status = 0;

	} else {
		status = -EPERM;
		DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("NODE: DSPNode_Delete: "
					"Failed to CMM handle\r\n")));
	}
	if (!DSP_SUCCEEDED(status)) {
		status = -EBADR;	/* no SM segments*/
		return status;
	}
    status = DSPNode_GetAttr(hNode, &nodeAttr, sizeof(nodeAttr));
	GetNodeType(hNode, &nodeType);
	if (nodeType != NODE_DEVICE) {
		/*segInfo index starts at 0.These checks may not be required*/
		if ((pInfo.segInfo[0].dwSegBasePa != 0) &&
		    (pInfo.segInfo[0].ulTotalSegSize) > 0) {
			/* get node translator's virtual address range
			   so we can free it */
			bufAttr.uAlignment = 0;
			bufAttr.uSegment = 1 | MEMRY_GETVIRTUALSEGID;
			DSPNode_AllocMsgBuf(hNode, 1, &bufAttr, &pVirtBase);
			/* Free virtual space */
			if (!pVirtBase)
				goto loop_end;

			if (munmap(pVirtBase,
					pInfo.segInfo[0].ulTotalSegSize)) {
				status = -EPERM;
			}

			delete_mmapelement(pVirtBase, &mmaplist);
		}
	}
loop_end:
	if (DSP_SUCCEEDED(status)) {
		/* Set up the structure Call DSP Trap */
		tempStruct.ARGS_NODE_DELETE.hNode = hNode;
		status = DSPTRAP_Trap(&tempStruct, CMD_NODE_DELETE_OFFSET);
		/* Free any node heap memory */
		if (nodeAttr.inNodeAttrIn.pGPPVirtAddr) {
			DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("DSPNodeDelete:"
					"Freeing Node heap addr \n")));
			free(nodeAttr.inNodeAttrIn.pGPPVirtAddr);
		}
	}
#ifdef DEBUG_BRIDGE_PERF
	timeRetVal = getTimeStamp(&tv_end);
	PrintStatistics(&tv_beg, &tv_end, "DSPNode_Delete", 0);
#endif

	return status;
}

/*
 *  ======== DSPNode_FreeMsgBuf ========
 */
DBAPI DSPNode_FreeMsgBuf(DSP_HNODE hNode, IN BYTE *pBuffer,
				IN OPTIONAL struct DSP_BUFFERATTR *pAttr)
{
	int status = 0;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_FreeMsgBuf:\r\n")));

	if (hNode) {
		if (pBuffer) {
			/* Set up the structure */
			/* Call DSP Trap */
			tempStruct.ARGS_NODE_FREEMSGBUF.hNode = hNode;
			tempStruct.ARGS_NODE_FREEMSGBUF.pBuffer = pBuffer;
			tempStruct.ARGS_NODE_FREEMSGBUF.pAttr = pAttr;
			status = DSPTRAP_Trap(&tempStruct,
				CMD_NODE_FREEMSGBUF_OFFSET);
			if (DSP_FAILED(status)) {
				DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: "
						"DSPNode_FreeMsgBuf:"
						"Failed to Free SM buf\r\n")));
			}
		} else {
			/* Invalid parameter */
			status = -EFAULT;
			DEBUGMSG(DSPAPI_ZONE_ERROR,
				(TEXT("NODE: DSPNode_FreeMsgBuf: "
				"Invalid pointer in the Input\r\n")));
		}
	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
				(TEXT("NODE: DSPNode_FreeMsgBuf: "
				"hNode is Invalid \r\n")));
	}

	return status;
}

/*
 *  ======== DSPNode_GetAttr ========
 *  Purpose:
 *      Copy the current attributes of the specified node.
 */
DBAPI DSPNode_GetAttr(DSP_HNODE hNode, OUT struct DSP_NODEATTR *pAttr,
		UINT uAttrSize)
{
	int status = 0;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_GetAttr:\r\n")));

	if (hNode) {
		if (pAttr) {
			if (uAttrSize >= sizeof(struct DSP_NODEATTR)) {
				/* Set up the structure */
				/* Call DSP Trap */
				tempStruct.ARGS_NODE_GETATTR.hNode = hNode;
				tempStruct.ARGS_NODE_GETATTR.pAttr = pAttr;
				tempStruct.ARGS_NODE_GETATTR.uAttrSize =
								uAttrSize;
				status = DSPTRAP_Trap(&tempStruct,
					CMD_NODE_GETATTR_OFFSET);
			} else {
				status = -EINVAL;
				DEBUGMSG(DSPAPI_ZONE_ERROR,
					(TEXT("NODE: DSPNode_GetAttr: "
					"Size is too small \r\n")));
			}
		} else {
			/* Invalid parameter */
			status = -EFAULT;
			DEBUGMSG(DSPAPI_ZONE_ERROR,
				(TEXT("NODE: DSPNode_GetAttr: "
				"Invalid pointer in the Input\r\n")));
		}
	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("NODE: DSPNode_GetAttr: "
			"hNode is Invalid \r\n")));
	}

	return status;
}

/*
 *  ======== DSPNode_GetMessage ========
 *  Purpose:
 *      Retrieve an event message from a task node.
 */
DBAPI DSPNode_GetMessage(DSP_HNODE hNode, OUT struct DSP_MSG *pMessage,
				UINT uTimeout)
{
	int status = 0;
	Trapped_Args tempStruct;
#ifdef DEBUG_BRIDGE_PERF
	struct timeval tv_beg;
	struct timeval tv_end;
	struct timezone tz;
	int timeRetVal = 0;

	timeRetVal = getTimeStamp(&tv_beg);

#endif

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_GetMessage:\r\n")));

	if (hNode) {
		if (pMessage) {
			/* Set up the structure */
			/* Call DSP Trap */
			tempStruct.ARGS_NODE_GETMESSAGE.hNode = hNode;
			tempStruct.ARGS_NODE_GETMESSAGE.pMessage = pMessage;
			tempStruct.ARGS_NODE_GETMESSAGE.uTimeout = uTimeout;
			status = DSPTRAP_Trap(&tempStruct,
				CMD_NODE_GETMESSAGE_OFFSET);
		} else {
			status = -EFAULT;
			DEBUGMSG(DSPAPI_ZONE_ERROR,
				(TEXT("NODE: DSPNode_GetMessage:"
				"pMessage is Invalid \r\n")));
		}
	} else {
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("NODE: DSPNode_GetMessage: "
			"hNode is Invalid \r\n")));
	}
#ifdef DEBUG_BRIDGE_PERF
	timeRetVal = getTimeStamp(&tv_end);
	PrintStatistics(&tv_beg, &tv_end, "DSPNode_GetMessage", 0);
#endif


	return status;
}

/*
 *  ======== GetNodeType ========
 *  Purpose:
 *      Return the node type
 */
int GetNodeType(DSP_HNODE hNode, DSP_NODETYPE *pNodeType)
{
	/*int status;*/
	int status = 0;
	struct DSP_NODEATTR nodeAttr;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("GetNodeType:\r\n")));

	if (hNode) {
		status = DSPNode_GetAttr(hNode, &nodeAttr, sizeof(nodeAttr));
		if (DSP_SUCCEEDED(status)) {
			*pNodeType =
			nodeAttr.iNodeInfo.nbNodeDatabaseProps.uNodeType;
		}
	} else {
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("GetNodeType: "
					"hNode is Invalid \r\n")));
	}

	return status;
}

/*
 *  ======== DSPNode_Pause ========
 *  Purpose:
 *      Temporarily suspend execution of a node that is currently running
 *      on a DSP.
 */
DBAPI DSPNode_Pause(DSP_HNODE hNode)
{
	int status = 0;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_Pause:\r\n")));

	if (hNode) {
		/* Set up the structure */
		/* Call DSP Trap */
		tempStruct.ARGS_NODE_PAUSE.hNode = hNode;
		status = DSPTRAP_Trap(&tempStruct, CMD_NODE_PAUSE_OFFSET);
	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("NODE: DSPNode_Pause: "
				"hNode is Invalid Handle\r\n")));
	}

	return status;
}

/*
 *  ======== DSPNode_PutMessage ========
 *  Purpose:
 *      Send an event message to a task node.
 */
DBAPI DSPNode_PutMessage(DSP_HNODE hNode, IN CONST struct DSP_MSG *pMessage,
						UINT uTimeout)
{
	int status = 0;
	Trapped_Args tempStruct;
#ifdef DEBUG_BRIDGE_PERF
	struct timeval tv_beg;
	struct timeval tv_end;
	struct timeval tz;
	int timeRetVal = 0;

	timeRetVal = getTimeStamp(&tv_beg);
#endif

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_PutMessage:\r\n")));

	if (hNode) {
		if (pMessage) {
			/* Set up the structure */
			/* Call DSP Trap */
			tempStruct.ARGS_NODE_PUTMESSAGE.hNode = hNode;
			tempStruct.ARGS_NODE_PUTMESSAGE.pMessage =
						(struct DSP_MSG *)pMessage;
			tempStruct.ARGS_NODE_PUTMESSAGE.uTimeout = uTimeout;
			status = DSPTRAP_Trap(&tempStruct,
				CMD_NODE_PUTMESSAGE_OFFSET);
		} else {
			status = -EFAULT;
			DEBUGMSG(DSPAPI_ZONE_ERROR,
				(TEXT("NODE: DSPNode_PutMessage: "
						"pMessage is Invalid \r\n")));
		}
	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("NODE: DSPNode_PutMessage: "
					"hNode is Invalid \r\n")));
	}
#ifdef DEBUG_BRIDGE_PERF
	timeRetVal = getTimeStamp(&tv_end);
	PrintStatistics(&tv_beg, &tv_end, "DSPNode_PutMessage", 0);
#endif


	return status;
}

/*
 *  ======== DSPNode_RegisterNotify ========
 *  Purpose:
 *      Register to be notified of specific events for this node.
 */
DBAPI
DSPNode_RegisterNotify(DSP_HNODE hNode, UINT uEventMask,
		       UINT uNotifyType, struct DSP_NOTIFICATION *hNotification)
{
	int status = 0;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION,
			(TEXT("NODE: DSPNode_RegisterNotify:\r\n")));

	if ((hNode) && (hNotification)) {
		if (IsValidNodeEvent(uEventMask)) {
			if (IsValidNotifyMask(uNotifyType)) {
				/* Set up the structure */
				/* Call DSP Trap */
				tempStruct.ARGS_NODE_REGISTERNOTIFY.hNode =
							hNode;
				tempStruct.ARGS_NODE_REGISTERNOTIFY.uEventMask =
							uEventMask;
				tempStruct.ARGS_NODE_REGISTERNOTIFY\
					.uNotifyType = uNotifyType;
				tempStruct.ARGS_NODE_REGISTERNOTIFY\
					.hNotification = hNotification;

				status = DSPTRAP_Trap(&tempStruct,
						CMD_NODE_REGISTERNOTIFY_OFFSET);
			} else {
				status = -ENOSYS;
				DEBUGMSG(DSPAPI_ZONE_ERROR,
					(TEXT("NODE: DSPNode_RegisterNotify: "
					"Invalid Notification Mask \r\n")));
			}
		} else {
			status = -EINVAL;
			DEBUGMSG(DSPAPI_ZONE_ERROR,
				(TEXT("NODE: DSPNode_RegisterNotify:"
						"Invalid Event type\r\n")));
		}
	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("NODE: DSPNode_RegisterNotify: "
				"hNode is Invalid \r\n")));
	}

	return status;
}

/*
 *  ======== DSPNode_Run ========
 *  Purpose:
 *      Start a task node running.
 */
DBAPI DSPNode_Run(DSP_HNODE hNode)
{
	int status = 0;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_Run:\r\n")));

	if (hNode) {
		/* Set up the structure */
		/* Call DSP Trap */
		tempStruct.ARGS_NODE_RUN.hNode = hNode;
		status = DSPTRAP_Trap(&tempStruct, CMD_NODE_RUN_OFFSET);
	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR, (TEXT("NODE: DSPNode_Run: "
					"hNode is Invalid Handle\r\n")));
	}

	return status;
}

/*
 *  ======== DSPNode_Terminate ========
 *  Purpose:
 *      Signal a task node running on a  DSP processor that it should
 *      exit its execute-phase function.
 */
DBAPI DSPNode_Terminate(DSP_HNODE hNode, int *pStatus)
{
	int status = 0;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION, (TEXT("NODE: DSPNode_Terminate:\r\n")));

	if (hNode) {
		/* !DSP_ValidWritePtr means it is a valid write ptr */
		if (!DSP_ValidWritePtr(pStatus, sizeof(int))) {
			/* Set up the structure */
			/* Call DSP Trap */
			tempStruct.ARGS_NODE_TERMINATE.hNode = hNode;
			tempStruct.ARGS_NODE_TERMINATE.pStatus = pStatus;
			status = DSPTRAP_Trap(&tempStruct,
				CMD_NODE_TERMINATE_OFFSET);
		} else
			status = -EFAULT;

	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("NODE: DSPNode_Terminate: "
					"hNode is Invalid Handle\r\n")));
	}

	return status;
}


/*
 *  ======== DSPNode_GetUUIDProps ========
 *  Purpose:
 *      Get Node properties from DCD/DOF file given the UUID
 */
DBAPI DSPNode_GetUUIDProps(DSP_HPROCESSOR hProcessor,
		IN CONST struct DSP_UUID *pNodeID,
		 OUT struct DSP_NDBPROPS *pNodeProps)
{
	int status = 0;
	Trapped_Args tempStruct;

	DEBUGMSG(DSPAPI_ZONE_FUNCTION,
				(TEXT("NODE:DSPNode_GetUUIDProps:\r\n")));

	if (hProcessor) {
		if ((pNodeID) && (pNodeProps)) {
			/* Set up the structure */
			/* Call DSP Trap */
			tempStruct.ARGS_NODE_GETUUIDPROPS.hProcessor =
						hProcessor;
			tempStruct.ARGS_NODE_GETUUIDPROPS.pNodeID =
						(struct DSP_UUID *)pNodeID;
			tempStruct.ARGS_NODE_GETUUIDPROPS.pNodeProps =
					(struct DSP_NDBPROPS *) pNodeProps;
			status = DSPTRAP_Trap(&tempStruct,
				CMD_NODE_GETUUIDPROPS_OFFSET);
		} else {
			/* Invalid parameter */
			status = -EFAULT;
			DEBUGMSG(DSPAPI_ZONE_ERROR,
				(TEXT("NODE: DSPNode_GetUUIDProps: "
				       "Invalid pointer in the Input\r\n")));
		}
	} else {
		/* Invalid pointer */
		status = -EFAULT;
		DEBUGMSG(DSPAPI_ZONE_ERROR,
			(TEXT("NODE: DSPNode_GetUUIDProps: "
					"hProcessor is Invalid \r\n")));
	}

	return status;
}


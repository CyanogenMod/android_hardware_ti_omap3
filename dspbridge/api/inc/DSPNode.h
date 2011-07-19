/*
 * dspbridge/mpu_api/inc/DSPNode.h
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
 *  ======== DSPNode.h ========
 *  Description:
 *      This is the header for the DSP/BIOS Bridge node module.
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
 *  Notes:
 *
 *! Revision History:
 *! ================
 *! 23-Nov-2002 gp: Comment change: uEventMask now referred to as a "type".
 *!                 Comment cleanup, correspondence to db_api.doc.
 *! 12-Dec-2001 ag  DSP_ENOTIMPL added to DSPNode_Connect().
 *! 11-Sep-2001 ag  Added new error codes.
 *! 23-Apr-2001 jeh Added pStatus parameter to DSPNode_Terminate.
 *! 16-Feb-2001 jeh Added new error codes.
 *! 13-Feb-2001 kc: DSP/BIOS Bridge name updates.
 *! 27-Oct-2000 jeh Updated to version 0.9 API spec.
 *! 07-Sep-2000 jeh Changed type HANDLE in DSPNode_RegisterNotify to
 *!                 DSP_HNOTIFICATION. Added DSP_STRMATTR parameter to
 *!                 DSPNode_Connect().
 *! 27-Jul-2000 rr: Updated to ver 0.8 of DSPAPI.
 *! 27-Jun-2000 rr: Created from DBAPI.h
 */

#ifndef DSPNode_
#define DSPNode_

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== DSPNode_Allocate ========
 *  Purpose:
 *      Allocate data structures for controlling and communicating with a node
 *      on a specific DSP processor.
 *  Parameters:
 *      hProcessor:         The processor handle.
 *      pNodeID:            Ptr to DSP_UUID for the node.
 *      pArgs:              Ptr to optional node arguments.
 *      pAttrIn:            Ptr to optional node attributes.
 *      phNode:             Ptr to location to store node handle on return.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            One of the input parameters pointers is invalid.
 *      -EFAULT:            Invalid processor handle.
 *      -ENOMEM:            Memory is not available to allocate a node
 *      -ENOKEY:            The node with the specified UUID is not registered.
 *      -EBADR:             The specified processor is in the wrong state
 *                          (not running)
 *      -EDOM:              The iPriority field specified in pAttrIn is out
 *                          of range.
 *      -EPERM:             General failure.
 */
extern DBAPI
DSPNode_Allocate(DSP_HPROCESSOR hProcessor,
		 IN CONST struct DSP_UUID * pNodeID,
		 IN CONST OPTIONAL struct DSP_CBDATA * pArgs,
		 IN OPTIONAL struct DSP_NODEATTRIN * pAttrIn,
		 OUT DSP_HNODE * phNode);

/*
 *  ======== DSPNode_AllocMsgBuf ========
 *  Purpose:
 *      Allocate and prepare a buffer whose descriptor will be passed to a DSP
 *      Node within a (DSP_MSG) message
 *  Parameters:
 *      hNode:              The node handle.
 *      uSize:              The size of the buffer (GPP bytes) to be allocated.
 *      pAttr:              Pointer to a DSP_BUFFERATTR structure.
 *      pBuffer:            Location to store the address of the allocated
 *                          buffer on output.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -ENOMEM:            Insufficent memory.
 *      -EFAULT:            pBuffer is not a valid address.
 *      -EPERM:             General Failure.
 *      -EPERM:             Alignment value not supported.(Must be 0, 1, 2, 4)
 *      -EBADR:             Invalid Segment Id.
 *      -EINVAL:            Invalid Size. Must be greater than zero.
 */
	extern DBAPI DSPNode_AllocMsgBuf(DSP_HNODE hNode, UINT uSize,
					 IN OPTIONAL struct DSP_BUFFERATTR * pAttr,
					 OUT BYTE ** pBuffer);

/*
 *  ======== DSPNode_ChangePriority ========
 *  Purpose:
 *      Change a task node's runtime priority within the DSP RTOS.
 *  Parameters:
 *      hNode:              The node handle.
 *      iPriority:          New runtime priority level.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -EDOM:              iPriority is out of range.
 *      -EPERM:             Operation is invalid for this node type.
 *      -ETIME:             A timeout occured before DSP responded.
 *      DSP_ERESTART:       A critical error occurred and the DSP is being
 *                          restarted.
 *      -EBADR:             The node is not allocated, paused, or running.
 *      -EPERM:             Unable to change the priority level.
 */
	extern DBAPI DSPNode_ChangePriority(DSP_HNODE hNode, INT iPriority);

/*
 *  ======== DSPNode_Connect ========
 *  Purpose:
 *      Make a stream connection, either between two nodes on a DSP,
 *      or between a node on a DSP and the GPP.
 *  Parameters:
 *      hNode:                  The first node handle.
 *      uStream:                Output stream index on first node (0 based).
 *      hOtherNode:             The second node handle.
 *      uOtherStream:           Input stream index on second node (0 based).
 *      pAttrs:                 Stream attributes. If NULL, defaults used.
 *  Returns:
 *      0:                      Success.
 *      -EFAULT:                Invalid node handle.
 *      -ENOMEM:                GPP memory allocation failure.
 *      DSP_EALREADYCONNCECTED: One of the specified connections has already
 *                              been made.
 *      -EBADR:                 The node is not in the NODE_ALLOCATED state.
 *      -EINVAL:                A Stream index is not valid.
 *      -ECONNREFUSED:          No more connections are allowed
 *      -EPERM:                 Unable to make connection.
 *      -ENOSYS:                Stream mode valid but not supported.
 *      -EPERM                  Illegal Stream mode specified.
 *
 */
	extern DBAPI DSPNode_Connect(DSP_HNODE hNode, UINT uStream,
				     DSP_HNODE hOtherNode, UINT uOtherStream,
				     IN OPTIONAL struct DSP_STRMATTR * pAttr);

/*
 *  ======== DSPNode_ConnectEx ========
 *  Purpose:
 *      Make a stream connection, either between two nodes on a DSP,
 *      or between a node on a DSP and the GPP.
 *  Parameters:
 *      hNode:                  The first node handle.
 *      uStream:                Output stream index on first node (0 based).
 *      hOtherNode:             The second node handle.
 *      uOtherStream:           Input stream index on second node (0 based).
 *      pAttrs:                 Stream attributes. If NULL, defaults used.
 *      pConnParam:             A pointer to a DSP_CBDATA structure that defines 
 *                              connection parameter for device nodes to pass to DSP side. 
 *                              If the value of this parameter is NULL, then this API behaves 
 *                              like DSPNode_Connect. This parameter will have length of the 
 *                              string and the null terminated string in DSP_CBDATA struct. 
 *                              This can be extended in future to pass binary data.
 *
 *  Returns:
 *      0:                      Success.
 *      -EFAULT:                Invalid node handle.
 *      -ENOMEM:                GPP memory allocation failure.
 *      DSP_EALREADYCONNCECTED: One of the specified connections has already
 *                              been made.
 *      -EBADR:                 The node is not in the NODE_ALLOCATED state.
 *      -EINVAL:                A Stream index is not valid.
 *      -ECONNREFUSED:          No more connections are allowed
 *      -EPERM:                 Unable to make connection.
 *      -ENOSYS:                Stream mode valid but not supported.
 *      -EPERM                  Illegal Stream mode specified.
 *
 */
	extern DBAPI DSPNode_ConnectEx(DSP_HNODE hNode, UINT uStream,
				       DSP_HNODE hOtherNode, UINT uOtherStream,
				       IN OPTIONAL struct DSP_STRMATTR * pAttr,
				       IN OPTIONAL struct DSP_CBDATA * pConnParam);

/*
 *  ======== DSPNode_Create ========
 *  Purpose:
 *      Create a node in a pre-run (i.e., inactive) state on its DSP processor.
 *  Parameters:
 *      hNode:              The node handle.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -ESPIPE:            Create function, or iAlg, not found in the COFF file
 *      DSP_WRONGSTATE:     Operation is invalid for the current node state.
 *      DSP_ETASK:          Unable to create the task or process on the DSP.
 *      -ENOMEM:            Memory Allocation failure on the DSP.
 *      DSP_ERESOURCE:      A requested resource is not available.
 *      DSP_EMULINST:       Multiple instances are not allowed.
 *      -ENOENT:            A specified entity was not found.
 *      DSP_EOUTOFIO:       An I/O resource is not available.
 *      DSP_ESTREAM:        Stream creation failure on the DSP.
 *      -ETIME:             A timeout occurred before the DSP responded.
 *      DSP_ERESTART:       A critical error has occurred and the DSP is
 *                          being restarted.
 *      -ENXIO:           Overlay region for this phase in use by another node
 *      DSP_EUSER1-16:      A node-specific failure occurred on the DSP.
 *      -EPERM:             Unable to Create the node.
 *  Details:
 */

	extern DBAPI DSPNode_Create(DSP_HNODE hNode);

/*
 *  ======== DSPNode_Delete ========
 *  Purpose:
 *      Delete all DSP-side and GPP-side resources for the node.
 *  Parameters:
 *      hNode:              The node handle.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      DSP_EDELETE:        A Deletion failure occured.
 *      DSP_EFREE:          A DSP memory free operation failed.
 *      DSP_EIOFREE:        A DSP I/O free operation failed.
 *      -ETIME:             Timeout occured before the DSP responded.
 *      DSP_ERESTART:       A critical error has occurred and the DSP is
 *                          being restarted.
 *      DSP_EUSER1-16:      A node-specific failure occurred on the DSP.
 *      -ENXIO:           Overlay region for this phase in use by another node
 *      -EPERM:             Unable to delete the node.
 *      -ESPIPE:            Delete function not found in the COFF file.
 */
	extern DBAPI DSPNode_Delete(DSP_HNODE hNode);

/*
 *  ======== DSPNode_FreeMsgBuf ========
 *  Purpose:
 *      Free a message buffer previously allocated by DSPNode_AllocMsgBuf..
 *  Parameters:
 *      hNode:              The node handle.
 *      pBuffer:            (Address) Buffer allocated by DSP_AllocMsgBuf.
 *      pAttr:              Same buffer attributes passed to DSP_AllocMsgBuf.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -EFAULT:            pBuffer is not valid.
 *      -EBADR:             Invalid Segment Id.
 *      -EPERM:             Failure to free the buffer.
 */
	extern DBAPI DSPNode_FreeMsgBuf(DSP_HNODE hNode, IN BYTE * pBuffer,
					IN OPTIONAL struct DSP_BUFFERATTR * pAttr);

/*
 *  ======== DSPNode_GetAttr ========
 *  Purpose:
 *      Copy the current attributes of the specified node.
 *  Parameters:
 *      hNode:              The node handle.
 *      pAttr:              Location to store the node attributes.
 *      uAttrSize:          The size of structure.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -EFAULT:            Parameter pAttr is not valid.
 *      -EPERM:             Unable to retrieve node attributes.
 *      -EINVAL:            The size of the specified DSP_NODEATTR structure
 *                          is too small to hold all node information.
 */
	extern DBAPI DSPNode_GetAttr(DSP_HNODE hNode,
				     OUT struct DSP_NODEATTR * pAttr, UINT uAttrSize);

/*
 *  ======== DSPNode_GetMessage ========
 *  Purpose:
 *      Retrieve an event message from a task node.
 *  Parameters:
 *      hNode:              The node handle.
 *      pMessage:           The message structure.
 *      uTimeout:           Timeout to wait for message.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -EFAULT:            Parameter pMessage is not valid.
 *      -EPERM:             Messages cannot be retrieved from this type of
 *                          node (eg a device node).
 *      -ETIME:             A timeout occurred and there is no message ready.
 *      DSP_ERESTART:       A critical error has occurred and the DSP is
 *                          being restarted.
 *      -EPERM:             An error occurred trying to retrieve a message.
 *      -ESRCH              Message contains a shared memory buffer and unable
 *                          to map buffer to process.
 */
	extern DBAPI DSPNode_GetMessage(DSP_HNODE hNode, OUT struct DSP_MSG * pMessage,
					UINT uTimeout);

/*
 *  ======== DSPNode_Pause ========
 *  Purpose:
 *      Temporarily suspend execution of a task node that is
 *      currently running on a DSP.
 *  Parameters:
 *      hNode:              The node handle.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -EPERM:             Invalid operation for this node type.
 *      -ETIME:             A timeout occured before the DSP responded.
 *      -EBADR:             Operation is invalid for the current node state.
 *      DSP_ERESTART:       A critical error has occurred and the DSP is
 *                          being restarted.
 *      -EPERM:             General failure.
 */
	extern DBAPI DSPNode_Pause(DSP_HNODE hNode);

/*
 *  ======== DSPNode_PutMessage ========
 *  Purpose:
 *      Send an event message to a task node.
 *  Parameters:
 *      hNode:              The node handle.
 *      pMessage:           The message structure.
 *      uTimeout:           Timeout (msecs) waiting for message to be queued.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -EFAULT:            Parameter pMessage is not valid.
 *      -EPERM:             Invalid operation for this node type
 *      -EBADR:             Node is in an invalid state to send a message.
 *      -ETIME:             Time out occured.
 *      DSP_ERESTART:       A critical error has occurred and the DSP is
 *                          being restarted.
 *      -ESRCH              The shared memory buffer contained in the message
 *                          could not be mapped into the clients address space.
 *      -EPERM:             General failure.
 */
	extern DBAPI DSPNode_PutMessage(DSP_HNODE hNode,
					IN CONST struct DSP_MSG * pMessage,
					UINT uTimeout);

/*
 *  ======== DSPNode_RegisterNotify ========
 *  Purpose:
 *      Register to be notified of specific events for this node.
 *  Parameters:
 *      hNode:              The node handle.
 *      uEventMask:         Type of event about which to be notified.
 *      uNotifyType:        Type of notification to be sent.
 *      hNotification:      Handle of DSP_NOTIFICATION object.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle or hNotification.
 *      -EINVAL:            Invalid uEventMask.
 *      DSP_ENOTIMP:        The specifed uNotifyType is not supported.
 *      -EPERM:             Unable to register for notification.
 */
	extern DBAPI DSPNode_RegisterNotify(DSP_HNODE hNode, UINT uEventMask,
					    UINT uNotifyType,
					    struct DSP_NOTIFICATION* hNotification);

/*
 *  ======== DSPNode_Run ========
 *  Purpose:
 *      Start a node running, or resume execution of a previously paused node.
 *  Parameters:
 *      hNode:              The node handle.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -EPERM:             Invalid operation for this type of node.
 *      -ESPIPE:            Execute function not found in the COFF file.
 *      -EBADR:             The node is not in the Created or Paused state.
 *      -ETIME:             A timeout occured before the DSP responded.
 *      DSP_ERESTART:       A critical error has occurred and the DSP is
 *                          being restarted.
 *      -ENXIO:           Overlay region for this phase in use by another node
 *      -EPERM:             Unable to start or resume execution.
 */
	extern DBAPI DSPNode_Run(DSP_HNODE hNode);

/*
 *  ======== DSPNode_Terminate ========
 *  Purpose:
 *      Signal a task or xDAIS socket node running on a DSP processor that
 *      it should exit its execute-phase function.
 *  Parameters:
 *      hNode:              Handle of node to terminate.
 *      pStatus:            Location to execute-phase function return value.
 *                          Possible values are between DSP_EUSER1-16.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            Invalid node handle.
 *      -EPERM:             Invalid operation for this type of node.
 *      -EBADR:             The node is not in the Created or Paused state.
 *      -ETIME:             A timeout occured before the DSP responded.
 *      DSP_ERESTART:       A critical error has occurred and the DSP is
 *                          being restarted.
 *      -EPERM:             Unable to Terminate the node.
 */
	extern DBAPI DSPNode_Terminate(DSP_HNODE hNode, int * pStatus);

/*
 *  ======== DSPNode_GetUUIDProps ========
 *  Purpose:
 *      Fetch the Node Properties from the DCD/DOF file, give the UUID
 *  Parameters:
 *      hProcessor:         The processor handle.
 *      pNodeID:            Ptr to DSP_UUID for the node.
 *      pNodeProps:         Ptr to location to store node properties.
 *  Returns:
 *      0:                  Success.
 *      -EFAULT:            One of the input parameters pointers is invalid.
 *      -EFAULT:            Invalid processor handle.
 *      -ENOMEM:            Memory is not available to allocate a node
 *      -ENOKEY:            The node with the specified UUID is not registered.
 *      -EBADR:             The specified processor is in the wrong state
 *                          (not running)
 *      -EDOM:              The iPriority field specified in pAttrIn is out
 *                          of range.
 *      -EPERM:             General failure.
 */
	extern DBAPI DSPNode_GetUUIDProps(DSP_HPROCESSOR hProcessor,
				      IN CONST struct DSP_UUID * pNodeID,
				      OUT struct DSP_NDBPROPS * pNodeProps);
#ifdef __cplusplus
}
#endif
#endif				/* DSPNode_ */

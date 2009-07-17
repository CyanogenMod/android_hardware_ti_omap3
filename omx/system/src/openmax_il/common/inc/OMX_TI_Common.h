/* ====================================================================
*             Texas Instruments OMAP(TM) Platform Software
* (c) Copyright Texas Instruments, Incorporated. All Rights Reserved.
*
* Use of this software is controlled by the terms and conditions found
* in the license agreement under which this software has been supplied.
* ==================================================================== */

/** OMX_TI_Common.h
 *  The LCML header file contains the definitions used by both the
 *  application and the component to access common items.
 */

#ifndef __OMX_TI_COMMON_H__
#define __OMX_TI_COMMON_H__

/* OMX_TI_SEVERITYTYPE enumeration is used to indicate severity level of errors returned by TI OpenMax components. 
   Critcal	Requires reboot/reset DSP
   Severe	Have to unload components and free memory and try again
   Major	Can be handled without unloading the component
   Minor	Essentially informational 
*/
typedef enum OMX_TI_SEVERITYTYPE {
	OMX_TI_ErrorCritical=1,
	OMX_TI_ErrorSevere,
	OMX_TI_ErrorMajor,
	OMX_TI_ErrorMinor
} OMX_TI_SEVERITYTYPE;
#endif
/* File EOF */


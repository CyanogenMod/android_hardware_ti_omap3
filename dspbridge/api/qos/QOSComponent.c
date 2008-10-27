/*
 * dspbridge/src/qos/linux/qos/QOSComponent.c
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
    File    QOSComponent.c
    Path    $(PROJROOT)\qos\linux\qos
    Desc    Defines DSPComponent_xxx.
    Rev     1.0.0
    ============================================================================
    Revision History
    SEP 25, 2005                 Jeff Taylor
	Created.
*/

#include <qosregistry.h>
#include <errbase.h>

/*  ============================================================================
  name        DSPComponent_Register
	Implementation
	Informs Registry that the given component is using system resources.
	Internally, this increments the InUse field of the
	QOSCOMPONENT structure.
	Parameters
		registry		system registry
		comp			component using system resources
	Return
		DSP_STATUS		Error code or DSP_SOK for success
	Requirement Coverage
		This method addresses requirement(s):  SR10085
*/
DSP_STATUS DSPComponent_Register(struct QOSREGISTRY *registry,
					struct QOSCOMPONENT *comp)
{
	if (comp != NULL)
		comp->InUse++;

	return DSP_SOK;
}

/*  ============================================================================
  name        DSPComponent_Unregister
	Implementation
	Informs Registry that component is no longer using system resources.
	Internally, this decrements the InUse field of the
	QOSCOMPONENT structure.
	Parameters
		registry		system registry
		comp			component releasing system resources
	Return
		DSP_STATUS		Error code or DSP_SOK for success
	Requirement Coverage
		This method addresses requirement(s):  SR10085
*/
DSP_STATUS DSPComponent_Unregister(struct QOSREGISTRY *registry,
			struct QOSCOMPONENT *comp)
{
	DSP_STATUS status = DSP_SOK;
	if (comp != NULL) {
		/* Negative status returned if error */
		if (comp->InUse > 0) {
			comp->InUse--;
			status = DSP_SOK;
		} else
			status = DSP_EWRONGSTATE;

	}
	return status;
}

ULONG QOS_Component_DefaultFunctionHandler(struct QOSDATA *DataObject,
				ULONG FunctionCode, ULONG Parameter1)
{
	return DSP_ENOTIMPL;
}

ULONG QOS_DynDependentLibrary_FunctionHandler(struct QOSDATA *DataObject,
				ULONG FunctionCode, ULONG Parameter1)
{
	return DSP_ENOTIMPL;
}


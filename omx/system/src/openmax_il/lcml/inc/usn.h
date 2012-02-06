
/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* ====================================================================
*             Texas Instruments OMAP(TM) Platform Software
* (c) Copyright Texas Instruments, Incorporated. All Rights Reserved.
*
* Use of this software is controlled by the terms and conditions found
* in the license agreement under which this software has been supplied.
* ==================================================================== */

/** Usn.h
 *  The LCML header file contains the definitions used by 
 *   the component to access USN specific iteams.
 *   copied from USN.h and iUlg.h located in dsp 
 */
#ifndef __USN_H
#define __USN_H


#define MDN_MONO_CHANNEL                0x0001
#define MDN_STEREO_INTERLEAVED          0x0002
#define MDN_STEREO_NON_INTERLEAVED      0x0003
#define MDN_MONO_DUPLICATED             0x0004

typedef enum {
    USN_GPPMSG_PLAY          = 0x0100,
    USN_GPPMSG_STOP          = 0x0200,
    USN_GPPMSG_PAUSE         = 0x0300,
    USN_GPPMSG_ALGCTRL       = 0x0400,
    USN_GPPMSG_STRMCTRL      = 0x0500,
    USN_GPPMSG_SET_BUFF      = 0x0600,
    USN_GPPMSG_SET_STRM_NODE = 0x0700,
    USN_GPPMSG_GET_NODE_PTR  = 0x0800
}USN_HostToNodeCmd;

typedef enum {  
    USN_DSPACK_STOP          = 0x0200,
    USN_DSPACK_PAUSE         = 0x0300,
    USN_DSPACK_ALGCTRL       = 0x0400,
    USN_DSPACK_STRMCTRL      = 0x0500,
    USN_DSPMSG_BUFF_FREE     = 0x0600,
    USN_DSPACK_SET_STRM_NODE = 0x0700,
    USN_DSPACK_GET_NODE_PTR  = 0x0800,
    USN_DSPMSG_EVENT         = 0x0E00
}USN_NodeToHostCmd;

typedef enum {
    USN_ERR_NONE,
    USN_ERR_WARNING,
    USN_ERR_PROCESS,
    USN_ERR_PAUSE,
    USN_ERR_STOP,
    USN_ERR_ALGCTRL,
    USN_ERR_STRMCTRL,
    USN_ERR_UNKNOWN_MSG
} USN_ErrTypes;


typedef enum {
    IUALG_OK                  = 0x0000,
    IUALG_WARN_CONCEALED      = 0x0100,
    IUALG_WARN_UNDERFLOW      = 0x0200,
    IUALG_WARN_OVERFLOW       = 0x0300,
    IUALG_WARN_ENDOFDATA      = 0x0400,
    IUALG_WARN_PLAYCOMPLETED  = 0x0500,
    IUALG_WARN_ALG_ERR        = 0x0700,
    IUALG_ERR_BAD_HANDLE      = 0x0F00,
    IUALG_ERR_DATA_CORRUPT    = 0x0F01,
    IUALG_ERR_NOT_SUPPORTED   = 0x0F02,
    IUALG_ERR_ARGUMENT        = 0x0F03,
    IUALG_ERR_NOT_READY       = 0x0F04,
    IUALG_ERR_GENERAL         = 0x0FFF
}IUALG_Event;

typedef enum {
    USN_STRMCMD_PLAY,
    USN_STRMCMD_PAUSE,
    USN_STRMCMD_STOP,
    USN_STRMCMD_SETCODECPARAMS,
    USN_STRMCMD_IDLE,   
    USN_STRMCMD_FLUSH    
}USN_StrmCmd;

/** IUALG_Cmd: This enum type describes the standard set of commands that
 * will be passed to iualg control API.
 *
 * @param IUALG_CMD_STOP: This command indicates that higher layer framework
 * has received a stop command and no more process API will be called for the current
 * data stream. The iualg layer is expected to ensure that all processed output is
 * put in the output IUALG_Buf buffers and the state of all buffers changed to free
 * or DISPATCH after this function call.
 *
 * @param IUALG_CMD_PAUSE: This command indicates that higher layer framework
 * has received a PAUSE command on the current data stream. The iualg layer
 * can change the state of some of its output IUALG_Bufs to DISPATCH to enable
 * high level framework to use the processed data until the command was received.
 *
 * @param IUALG_CMD_GETSTATUS: This command indicates that some algo specific
 * status needs to be returned to the framework. The pointer to the status structure
 * will be in IALG_status * variable passed to the control API.
 * The interpretation of the content of this pointer is left to IUALG layer.
 *
 * @param IUALG_CMD_SETSTATUS: This command indicates that some algo specific
 * status needs to be set. The pointer to the status structure will be in
 * IALG_status * variable passed to the control API. The interpretation of the
 * content of this pointer is left to IUALG layer.
 *
 * @param IUALG_CMD_USERSETCMDSTART: The algorithm specific control set
 * commands can have the enum type set from this number. The pointer to the status
 * structure which needs to be set will be in IALG_status * variable passed to the
 * control API. The interpretation of the content of this pointer is left to IUALG
 * layer.
 *
 * @param IUALG_CMD_USERGETCMDSTART: The algorithm specific control get
 * commands can have the enum type set from this number. The pointer to the status
 * structure which needs to be returned will be in IALG_status * variable passed to
 * the control API. The interpretation of the content of this pointer is left to
 * IUALG layer.
 *
 * @param IUALG_CMD_FLUSH: Flush all the buffers pending for the underlying
 * stream. The stream id is passed in the lower 8-bit of the command.
 * If the lower 8-bit is equal to 0xFF, all the streams have to be flushed.
 */
typedef enum {
    IUALG_CMD_STOP             = 0,
    IUALG_CMD_PAUSE            = 1,
    IUALG_CMD_GETSTATUS        = 2,
    IUALG_CMD_SETSTATUS        = 3,
    IUALG_CMD_USERSETCMDSTART  = 100,
    IUALG_CMD_USERGETCMDSTART  = 150,
    IUALG_CMD_FLUSH            = 0x100
}IUALG_Cmd;

#endif

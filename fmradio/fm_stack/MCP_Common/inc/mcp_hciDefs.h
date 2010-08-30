/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



/** \file   msp_transDefs.h 
 *  \brief  HCI (Host Controller Interface protocol) common definitions
 *
 *  \see    msp_uartBusDrv.c
 */


#ifndef _MCP_TRANS_DEFS_API_H_
#define _MCP_TRANS_DEFS_API_H_

/************************************************************************
 * Defines
 ************************************************************************/

/* HCI packet type - the first byte of HCI frame over UART */
#define HCI_PKT_TYPE_COMMAND		0x01
#define HCI_PKT_TYPE_ACL			0x02
#define HCI_PKT_TYPE_SCO			0x03
#define HCI_PKT_TYPE_EVENT			0x04 
#define HCI_PKT_TYPE_NAVC			0x09 

/* Interface Sleep Management packet types */
#define HCI_PKT_TYPE_SLEEP_IND		0x30
#define HCI_PKT_TYPE_SLEEP_ACK		0x31
#define HCI_PKT_TYPE_WAKEUP_IND		0x32
#define HCI_PKT_TYPE_WAKEUP_ACK		0x33 


#define HCI_PKT_TYPE_LEN			1

#define HCI_HEADER_SIZE_COMMAND     3
#define HCI_HEADER_SIZE_ACL         4
#define HCI_HEADER_SIZE_SCO         3
#define HCI_HEADER_SIZE_EVENT       2
#define HCI_HEADER_SIZE_NAVC        3
#define HCI_HEADER_SIZE_IFSLPMNG    0

#define HCI_PREAMBLE_SIZE(type)		(HCI_HEADER_SIZE_##type + HCI_PKT_TYPE_LEN)

#endif  /* _MCP_TRANS_DEFS_API_H_ */

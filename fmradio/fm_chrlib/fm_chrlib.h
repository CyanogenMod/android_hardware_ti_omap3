/*
 * FM lib, to use TI's FM stack over FM character driver-For shared transport
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

#ifndef _FM_CHR_H
#define _FM_CHR_H

/* Radio device node*/
#define TI_FMRADIO	"/dev/tifm"

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* Macros to enable/disable debug logs*/
#define FM_CHRLIB_ERR(fmt, arg...)	printf("(fmlib):"fmt"\n" , ## arg)
#ifdef DEBUG
#define FM_CHRLIB_DBG(fmt, arg...)	printf("(fmlib):"fmt"\n" , ## arg)
#else
#define FM_CHRLIB_DBG(fmt, arg...)
#endif

/* Macros to check the file open/close state*/
#define INVALID_DESC	-1
#define RADIO_DEV_OPEN	 1
#define RADIO_DEV_CLOSE	 0
#define RADIO_DEV_EXIT	-1

/* Forward declarations of the library functions*/

int fm_open_dev(int, void*, void*, void *);
int fm_close_dev(int);
int fm_send_req(unsigned short hci_op, unsigned char *cmd_params,
                unsigned short params_len, unsigned char *user_data);

typedef struct {
	uint8_t         evt;
	uint8_t         plen;
} __attribute__ ((packed))      hci_event_hdr;

typedef struct {
	uint8_t         ncmd;
	uint16_t        opcode;
} __attribute__ ((packed)) evt_cmd_complete;

#endif /* _FM_CHR_H */

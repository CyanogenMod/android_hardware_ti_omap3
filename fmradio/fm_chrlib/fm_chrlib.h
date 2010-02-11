/*
 *  FM library, to use TI's FM stack over FM character driver -For shared transport
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program;if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _FM_CHR_H
#define _FM_CHR_H

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

/* Alias for the hci-lib functionalities*/
#define hci_open_dev fm_open_dev
#define hci_close_dev fm_close_dev
#define hci_send_req fm_send_req
#define hci_send_cmd fm_send_cmd

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

/* Forward declarations of the library functions*/
int fm_send_req(int , struct hci_request*, int);
int fm_open_dev(int);
int fm_close_dev(int);

#endif /* _FM_CHR_H */

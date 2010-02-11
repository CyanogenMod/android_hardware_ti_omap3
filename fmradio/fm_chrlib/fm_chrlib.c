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

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "fm_chrlib.h"

static int fm_fd = INVALID_DESC;
static int dev_state = RADIO_DEV_CLOSE;

/* HCI functions that require open device
 * dd - Device descriptor returned by hci_open_dev. */
int fm_send_cmd(int dd, uint16_t ogf, uint16_t ocf, uint8_t plen, void *param)
{
	hci_command_hdr hc;
	char *buffer, *ptr;
	int size;

#ifdef DEBUG
	int count;
#endif
	FM_CHRLIB_DBG("Inside %s", __FUNCTION__);

	buffer = NULL;
	ptr = NULL;
	size = 0;

	hc.opcode = htobs(cmd_opcode_pack(ogf, ocf));
	hc.plen= plen;

	/* for sizeof(HCI_COMMAND_PKT) + HCI_COMMAND_HDR_SIZE + plen */
	buffer = (void*)malloc(1 + HCI_COMMAND_HDR_SIZE + plen);
	if (buffer == NULL) {
		FM_CHRLIB_ERR(" fm_send_cmd() Buffer allocation failed (%d) ", errno);

		return -1;
	}

	ptr = buffer;

	*ptr = HCI_COMMAND_PKT;
	ptr += 1;	/* 1 byte 0x01 field */
	size += 1;

	memcpy(ptr, &hc, HCI_COMMAND_HDR_SIZE);
	ptr += HCI_COMMAND_HDR_SIZE;	/* 2 bytes sizeof(hci_command_hdr) */
	size += HCI_COMMAND_HDR_SIZE;

	if (plen) {
		memcpy(ptr, param, plen);
		size += plen;
	}

#ifdef DEBUG
	ptr = buffer;
	count = 0;
	while(count < (plen+4)) {
		printf(" 0x%02x ", *ptr);
		ptr++;
		count++;
	}
	printf("\n");
#endif
	while (write(dd, buffer, size) < 0) {
		if (errno == EAGAIN || errno == EINTR)
			continue;

		FM_CHRLIB_ERR("fm_send_cmd() Write failed (%d)", errno);
		free (buffer);

		return -1;
	}
	free (buffer);

	return 0;
}

/* Function to send the commands from the Stack to the FM character driver*/
int fm_send_req(int dd, struct hci_request *r, int to)
{
	unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
	unsigned int opcode = htobs(cmd_opcode_pack(r->ogf, r->ocf));
	socklen_t len;
	hci_event_hdr *hdr;
	int try;

	FM_CHRLIB_DBG("Inside %s", __FUNCTION__);

	/* Wait fot the device to open before write*/
	while(dev_state != RADIO_DEV_OPEN)
		usleep(1);

	/* Assign the global opened file descriptor*/
	dd = fm_fd;

	if (fm_send_cmd(dd, r->ogf, r->ocf, r->clen, r->cparam) < 0) {
		FM_CHRLIB_ERR("fm_send_cmd() failed (%d)", errno);

		return -1;
	}
	FM_CHRLIB_DBG("Data Sent successfully\n");

	try = 10;
	while (try--) {
		evt_cmd_complete *cc;
		evt_cmd_status *cs;
		evt_remote_name_req_complete *rn;
		remote_name_req_cp *cp;

		if (to) {
			struct pollfd p;
			int n;

			p.fd = dd; p.events = POLLIN;
			while ((n = poll(&p, 1, to)) < 0) {
				if (errno == EAGAIN || errno == EINTR)
					continue;

				FM_CHRLIB_ERR(" fm_send_req(): poll failed (%d) ", errno);

				return -1;
			}

			if (!n) {
				errno = ETIMEDOUT;
				FM_CHRLIB_ERR(" fm_send_req(): Timeout (%d) ", errno);

				return -1;
			}

			to -= 10;
			if (to < 0) to = 0;

		}

		FM_CHRLIB_DBG(" Reading. . . ");
		while ((len = read(dd, buf, sizeof(buf))) < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;

			FM_CHRLIB_ERR(" fm_send_req(): Read failed (%d) ", errno);

			return -1;
		}

		hdr = (void *) (buf + 1);
		ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
		len -= (1 + HCI_EVENT_HDR_SIZE);

		/* Check for the type of event received*/
		if(hdr->evt == EVT_CMD_COMPLETE) {
			FM_CHRLIB_DBG(" Inside EVT_CMD_COMPLETE ");
			cc = (void *) ptr;

			/* Validate the opceode of the response*/
			if (cc->opcode != opcode)
				continue;


			ptr += EVT_CMD_COMPLETE_SIZE;
			len -= EVT_CMD_COMPLETE_SIZE;

			/* Copy the response parameters*/
			r->rlen = MIN(len, r->rlen);
			memcpy(r->rparam, ptr, r->rlen);

		} else {
			FM_CHRLIB_DBG(" Inside default ");
			if (hdr->evt != r->event)
				break;

			r->rlen = MIN(len, r->rlen);
			memcpy(r->rparam, ptr, r->rlen);
		}
		FM_CHRLIB_DBG(" fm_send_req() passed ");

		return 0;
	}
	errno = ETIMEDOUT;

	FM_CHRLIB_ERR(" fm_send_req(): Timeout (%d) ",errno);
	return -1;

}

/* Open FM character device. Returns device descriptor (dd). */
int fm_open_dev(int dev_id)
{
	int oflags, retval;

	FM_CHRLIB_DBG(" Inside %s ", __FUNCTION__);

	retval = 0;

	/* If not closed, open the device only once. Avoid multiplr open*/
	if (fm_fd == INVALID_DESC) {
		FM_CHRLIB_DBG("	Opening %s device ", TI_FMRADIO);

		/* Open /dev/tifm*/
		fm_fd = open(TI_FMRADIO, O_RDWR);
		if (fm_fd < 0) {
			FM_CHRLIB_ERR("	Opening %s device failed ", TI_FMRADIO);
			return -1;
		}
		FM_CHRLIB_DBG(" Opened %s ", TI_FMRADIO);

		/* Update the Open state*/
		dev_state = RADIO_DEV_OPEN;

		oflags = fcntl(fm_fd, F_GETFL);
		fcntl(fm_fd, F_SETOWN, getpid());
		retval = fcntl(fm_fd, F_SETFL, oflags | FASYNC);

		if(retval < 0) {
			FM_CHRLIB_ERR(" F_SETFL Fails ");
			return -1;
		}
	}

	return fm_fd;
}

/* Close FM character device*/
int fm_close_dev(int dd)
{
	int ret;

	FM_CHRLIB_DBG(" Inside %s ", __FUNCTION__);

	ret = 0;

	/* Avoid multiple close, i.e close the device only if it is opened*/
	if(fm_fd != INVALID_DESC) {
		FM_CHRLIB_DBG("	Closing %s device ", TI_FMRADIO);

		/* Close /dev/tifm*/
		ret = close(fm_fd);
		if(ret < 0) {
			FM_CHRLIB_ERR("	Closing %s device failed ", TI_FMRADIO);
			return -1;
		}
		FM_CHRLIB_DBG("	Closed %s device ", TI_FMRADIO);

		/* Update the state of the descripto*/
		fm_fd = INVALID_DESC;
		dev_state = RADIO_DEV_CLOSE;
	}

	return ret;
}


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

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <sys/param.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <pthread.h>

#include <android/log.h>
#include <utils/Log.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "fm_chrlib.h"

/* basically hci_request's response only members..*/
struct resp {
	unsigned char rlen;
	void *rparam;
} __attribute__ ((packed));

typedef struct
{
	int                     eResult;
	unsigned char           uEvtParamLen;
	unsigned char           *pEvtParams;
} TCmdComplEvent;

void                     *_fmcTransportHciData;
typedef void             * handle_t;
typedef void             (*FM_DRV_CmdComplCb)(void*, TCmdComplEvent*);
FM_DRV_CmdComplCb        fCmdCompCallback;
typedef void             (*_FM_DRV_IF_InterruptCb) (void);
_FM_DRV_IF_InterruptCb   fInterruptCallback;

static int fm_fd = INVALID_DESC;
static int dev_state = RADIO_DEV_CLOSE;
pthread_t recv_thread;

int fm_send_cmd(int dd, uint16_t ogf, uint16_t ocf, uint8_t plen, void *param)
{
	hci_command_hdr hc;
	int skb_len = HCI_COMMAND_PKT + HCI_COMMAND_HDR_SIZE + plen;
	char skb[skb_len];

	FM_CHRLIB_DBG("Inside %s", __FUNCTION__);

	hc.opcode = htobs(cmd_opcode_pack(ogf, ocf));
	hc.plen= plen;

	skb[0] = HCI_COMMAND_PKT;
	memcpy(&skb[1], &hc, HCI_COMMAND_HDR_SIZE);

	if (plen)
		memcpy(&skb[4], param, plen);

	while (write(dd, skb, skb_len) < 0) {
		if (errno == EAGAIN || errno == EINTR)
			continue;

		FM_CHRLIB_ERR("fm_send_cmd() Write failed (%d)", errno);
		return -1;
	}
	return 0;
}

int fm_send_req(unsigned short hci_op, unsigned char *cmd_params,
		unsigned short params_len, unsigned char *user_data)
{
	FM_CHRLIB_DBG("Inside %s for 0x%02x ", __FUNCTION__, hci_op);

	/* Wait for the device to open before write*/
	while(dev_state != RADIO_DEV_OPEN)
		usleep(1);

	if (fm_send_cmd(fm_fd, 0x3F, hci_op, params_len, cmd_params) < 0) {
		FM_CHRLIB_ERR("fm_send_cmd() failed (%d)", errno);
		return -1;
	}
	FM_CHRLIB_DBG("Data Sent successfully\n");
	return 0;
}

void fm_receiver_thread(void)
{
	int try = 0, i;
	unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr;
	socklen_t len;
	hci_event_hdr *hdr;
	TCmdComplEvent *CmdComplEvent;
	int pollcnt = 10; /* keep the pollcnt minimum till FM ON happens */

	FM_CHRLIB_DBG(" Inside %s ", __func__);

	evt_cmd_complete *cc;
	while(dev_state != RADIO_DEV_EXIT) {
		do {
			struct pollfd p;
			int n;

			memset(&p, 0, sizeof(p));
			p.fd = fm_fd; p.events = POLLIN;
			if ((n = poll(&p, 1, pollcnt)) < 0) {
				if (errno == EAGAIN || errno == EINTR)
					continue;
				FM_CHRLIB_ERR(" poll failed (%d) ", errno);
				continue;
			}
			if (!n) {
				errno = ETIMEDOUT;
				/* no not really a timeout now.. since we run continously..*/
				continue; // ?
			} else if (p.revents & POLLIN) {
				/* proper poll repeatedly somehow? */
				break;
			}
		} while(dev_state != RADIO_DEV_EXIT);
		if ((len = read(fm_fd, buf, sizeof(buf))) < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			FM_CHRLIB_ERR(" Read failed (%d) ", errno);
			continue; // to poll?
		}

		hdr = (void *) (buf + 1);
		ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
		len -= (1 + HCI_EVENT_HDR_SIZE);

		/* Check for the type of event received*/
		if(hdr->evt == EVT_CMD_COMPLETE) {
			FM_CHRLIB_DBG(" Inside EVT_CMD_COMPLETE ");
			cc = (void *) ptr;

			ptr += EVT_CMD_COMPLETE_SIZE;
			len -= EVT_CMD_COMPLETE_SIZE;

			ptr++; //an extra Zero perhaps?
			len--;

			/* prepare response buffer */
			struct resp *r = malloc(sizeof(*r));
			memset(r, 0, sizeof(*r));
			/* r->rparam to be malloc-ed later..*/

			/* Copy the response parameters*/
			unsigned short rlen = MIN(len, 1024);
			r->rparam = malloc(rlen);
			memcpy(r->rparam, ptr, rlen);
			/* hopefully mem is being freed inside stack _cb*/

			CmdComplEvent = malloc(sizeof(*CmdComplEvent));
			CmdComplEvent->eResult = 0; /* Respose is RES_OK*/
			CmdComplEvent->uEvtParamLen = rlen;
			CmdComplEvent->pEvtParams = r->rparam;

			fCmdCompCallback(_fmcTransportHciData, CmdComplEvent);
		} else {
			FM_CHRLIB_DBG(" Not an EVT_CMD_COMPLETE ");
			pollcnt = 25; /* Increase the poll count after FM ON */
			fInterruptCallback();
		}
	}	//end of while (1)
	FM_CHRLIB_DBG("%s exited...\n", __func__);
}

void fm_sig_handler(int sig)
{
	if (sig == SIGINT) {
		FM_CHRLIB_DBG("SIGINT received\n");
		dev_state = RADIO_DEV_EXIT;
	}
}

/* Open FM character device. Returns device descriptor (dd). */
int fm_open_dev(int dev_id, void* fm_interrupt_cb, void* cmd_cmplt_cb, void *hHandle)
{
	int oflags, retval;
	_fmcTransportHciData = hHandle;

	fCmdCompCallback = cmd_cmplt_cb;
	fInterruptCallback = fm_interrupt_cb;
	retval = 0;

	/* should always throw up on logcat */
	LOGE("fm_chrlib: FM Stack version MCPv2.5 Beta \n");
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

	signal(SIGINT, fm_sig_handler);
	/* create a receiver thread */
	pthread_create(&recv_thread, NULL,
			(void*(*)(void*))fm_receiver_thread, NULL);

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
		pthread_kill(recv_thread, SIGINT);
		/* Close /dev/tifm*/
		ret = close(fm_fd);
		if(ret < 0) {
			FM_CHRLIB_ERR("	Closing %s device failed ", TI_FMRADIO);
			return -1;
		}
		pthread_join(recv_thread, NULL);
		FM_CHRLIB_DBG("	Closed %s device ", TI_FMRADIO);

		/* Update the state of the descripto*/
		fm_fd = INVALID_DESC;
		dev_state = RADIO_DEV_CLOSE;
	}

	return ret;
}


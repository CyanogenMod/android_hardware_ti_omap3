/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2008-2009  Texas Instruments, Inc.
 *  Copyright (C) 2002-2003  Maxim Krasnyansky <maxk@qualcomm.com>
 *  Copyright (C) 2002-2008  Marcel Holtmann <marcel@holtmann.org>
 *
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <syslog.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/sco.h>

/* Static Local variables */

/* BD Address of the BT head set */
static bdaddr_t bdaddr;

/* Buffer to receive data feom headset */
static unsigned char *buffer;

/* Default data size */
static long data_size = 672;

/* Handling termination of process through signals */
static volatile int terminate = 0;

/* Static functions declerations */
static void send_hciCmd(int dev_id, int command_length, char **command);


/** Function to handle signal terminations */
static void sig_term(int sig) {
	terminate = 1;
}

/** do_connect Function
 *  This function Creates the SCO connection to the BT headset
 *
 *  Parameters :
 *  @ svr        : BD address of headset
 *  Returns     SCO socket id on success
 *              suitable error code
 */
static int do_connect(char *svr)
{
	struct sockaddr_sco addr;
	struct sco_conninfo conn;
	socklen_t optlen;
	int sk;

	/* Create socket */
	sk = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO);
	if (sk < 0) {
		syslog(LOG_ERR, "Can't create socket: %s (%d)",
							strerror(errno), errno);
		return -1;
	}

	/* Bind to local address */
	memset(&addr, 0, sizeof(addr));
	addr.sco_family = AF_BLUETOOTH;
	bacpy(&addr.sco_bdaddr, &bdaddr);

	if (bind(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		syslog(LOG_ERR, "Can't bind socket: %s (%d)",
							strerror(errno), errno);
		goto error;
	}

	/* Connect to remote device */
	memset(&addr, 0, sizeof(addr));
	addr.sco_family = AF_BLUETOOTH;
	str2ba(svr, &addr.sco_bdaddr);

	if (connect(sk, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		syslog(LOG_ERR, "Can't connect: %s (%d)",
							strerror(errno), errno);
		goto error;
	}

	/* Get connection information */
	memset(&conn, 0, sizeof(conn));
	optlen = sizeof(conn);

	if (getsockopt(sk, SOL_SCO, SCO_CONNINFO, &conn, &optlen) < 0) {
		syslog(LOG_ERR, "Can't get SCO connection information: %s (%d)",
							strerror(errno), errno);
		goto error;
	}

	syslog(LOG_INFO, "Connected [handle %d, class 0x%02x%02x%02x]",
		conn.hci_handle,
		conn.dev_class[2], conn.dev_class[1], conn.dev_class[0]);

	return sk;

error:
	close(sk);
	return -1;
}

/** dump_mode Function
 *  This function waits till disconnection is intiated from headset or from
 *  the application.
 *
 *  Parameters :
 *  @ sk        : SCO socket id
 *  Returns VOID
 */
static void dump_mode(int sk)
{
	int len;

	/*  Wait till disconnect is issued from the headset OR
	 *  IF the application is killed using signals
	 */
	while ( ((len = read(sk, buffer, data_size)) > 0) && (!terminate) )
		syslog(LOG_INFO, "Recevied %d bytes", len);
}

/** send_hciCmd Function
 *  This function takes the hci commands for the BT chip configurations, creates 
 *  a hci channel to send the commadns through UART to configure BT chip
 *
 *  Parameters :
 *  @ dev_id            : HCI device ID
 *  @ command_length    : Number of arguments of the command
 *  @ command           : Pointer to command list
 *  Returns 0 upon success
 *        , different error messages depending upon the error.
 */
static void send_hciCmd(int dev_id, int command_length, char **command)
{
        unsigned char buf[HCI_MAX_EVENT_SIZE], *ptr = buf;
        struct hci_filter flt;
        hci_event_hdr *hdr;
        int i, opt, len, dd;
        uint16_t ocf;
        uint8_t ogf;
        
        if (dev_id < 0)
                dev_id = hci_get_route(NULL);

        errno = 0;
        ogf = strtol(command[0], NULL, 16);
        ocf = strtol(command[1], NULL, 16);

        for (i = 2, len = 0; i < command_length && len < sizeof(buf); i++, len++)
                *ptr++ = (uint8_t) strtol(command[i], NULL, 16);

        dd = hci_open_dev(dev_id);
        if (dd < 0) {
                perror("Device open failed");
                return;
        }

        /* Setup filter */
        hci_filter_clear(&flt);
        hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
        hci_filter_all_events(&flt);
        if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
                perror("HCI filter setup failed");
                return;
        }

	    /* Send the BT chip configuration commands */
        if (hci_send_cmd(dd, ogf, ocf, len, buf) < 0) {
                perror("Send failed");
                return;
        }

	    /* Wait for the command completion event */
        len = read(dd, buf, sizeof(buf));
        if (len < 0) {
                perror("Read failed");
                return;
        }

        hdr = (void *)(buf + 1);
        ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
        len -= (1 + HCI_EVENT_HDR_SIZE);

        hci_close_dev(dd);
}

/** USAGE Function
 *  This function displays the usage of the bt_sco_app application.
 *
 *  Parameters :
 *  @ VOID
 *  Returns VOID
 */
static void usage(void)
{
	printf("bt_scoapp\n"
		"Usage:\n");
	printf("\tbt_scoapp [bd_addr]\n");
}

/** Main Function
 *  The main function takes the command line BD adress of headset as inputs ,
 *  Calls the hci send configuration function and then creates a SCO connection.
 *
 *  Parameters :
 *  @ argc     : Number of arguments on the command line
 *  @ argv     : Pointer to argument list - BD addr is the only valid argument.
 *  Returns 0 upon success
 *        , different error messages depending upon the error.
 */
int main(int argc ,char *argv[])
{
	struct sigaction sa;
	int opt, i = 0, sk;

	/* BT PCM configurations commands */
	char *command[] = {        "0x3f", "0x106",                 /* OCF and OGF */
                                   "0x80", "0x00",                  /* Bit clock - 128KHz*/
			           "0x00",                          /* BT chip as Master*/
			           "0x40", "0x1f", "0x00", "0x00",  /* Sampling rate - 8KHz*/
			           "0x01", "0x00",                  /* 50% Duty cycle*/
			           "0x00",                          /* Frame sync at falling edge*/
			           "0x00",                          /* FS Active high polarity*/
			           "0x00",                          /* FS direction - [Reserved]*/
			           "0x10", "0x00",                  /* CH1 16 -bit OUT size*/
			           "0x01", "0x00",                  /* One Clock delay */
			           "0x00",                          /* Data driven at rising edge*/
			           "0x10", "0x00",                  /* CH1 16 -bit IN size */
			           "0x01", "0x00",                  /* CH1 DAta IN One Clock delay*/
			           "0x00",                          /* Data driven at sampling edge*/
			           "0x00",                          /* Reserved bit*/
			           "0x10", "0x00",                  /* CH2 16 -bit OUT size*/
			           "0x11", "0x00",                  /* CH2 data OUT off set*/
			           "0x00",                          /* Data driven at rising edge*/
			           "0x10", "0x00",                  /* CH2 16 -bit IN size*/
			           "0x11", "0x00",                  /* CH2 data IN off set*/
			           "0x00",                          /* Data Sampled at rising edge*/
			           "0x00"                           /* Reserved bit*/
			    };         
	int command_length = 36; /* Length of the BT configuration commands */

	/* Check if the number of arguemts mentioned is 2 */
	if (argc != 2)
	{
		printf("\n Wrong input - No BD headset address specified");
		usage();
		exit(1);
	}

	memset(&sa, 0, sizeof(sa));
	sa.sa_flags   = SA_NOCLDSTOP;
        sa.sa_handler = sig_term;
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT,  &sa, NULL);
        sigaction(SIGCHLD, &sa, NULL);
        sigaction(SIGPIPE, &sa, NULL);
	
	openlog("bt_sco_app", LOG_PERROR | LOG_PID, LOG_LOCAL0);

	/* Allocate memory for the buffer */
	if (!(buffer = malloc(data_size))) {
		perror("Can't allocate data buffer");
		exit(1);
	}
    /* send the BT configuration commands */
	send_hciCmd(-1,command_length,command);

	sleep(2); /* wait for some time while BT chip is configured */

	sk = do_connect(argv[1]);
	if (sk < 0)
		exit(1);
	dump_mode(sk);

	syslog(LOG_INFO, "Exit");

	closelog();

	return 0;
}

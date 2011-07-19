/*
 * GSM 07.10 Implementation with User Space Serial Ports
 *
 * Code heavily based on gsmMuxd written by
 * Copyright (C) 2003 Tuukka Karvonen <tkarvone@iki.fi>
 * Modified November 2004 by David Jander <david@protonic.nl>
 * Modified January 2006 by Tuukka Karvonen <tkarvone@iki.fi>
 * Modified January 2006 by Antti Haapakoski <antti.haapakoski@iki.fi>
 * Modified March 2006 by Tuukka Karvonen <tkarvone@iki.fi>
 * Modified October 2006 by Vasiliy Novikov <vn@hotbox.ru>
 *
 * Copyright (C) 2008 M. Dietrich <mdt@emdete.de>
 * Modified January 2009 by Ulrik Bech Hald <ubh@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* If compiled with the MUX_ANDROID flag this mux will be enabled to run under Android */

/**************************/
/* INCLUDES                          */
/**************************/
#include <errno.h>
#include <fcntl.h>
//#include <features.h>
#include <paths.h>
#ifdef MUX_ANDROID
#include <pathconf.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>


/**************************/
/* DEFINES                            */
/**************************/
/*Logging*/\
#ifndef MUX_ANDROID
#include <syslog.h>
//  #define LOG(lvl, f, ...) do{if(lvl<=syslog_level)syslog(lvl,"%s:%d:%s(): " f "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);}while(0)
#define LOGMUX(lvl,f,...) do{if(lvl<=syslog_level){\
								if (logtofile){\
								  fprintf(muxlogfile,"%d:%s(): " f "\n", __LINE__, __FUNCTION__, ##__VA_ARGS__);\
								  fflush(muxlogfile);}\
								else\
								  fprintf(stderr,"%d:%s(): " f "\n", __LINE__, __FUNCTION__, ##__VA_ARGS__);\
								}\
							}while(0)
#else //will enable logging using android logging framework (not to file)
#define LOG_TAG "MUXD"
#include <utils/Log.h> //all Android LOG macros are defined here.
#define LOGMUX(lvl,f,...) do{if(lvl<=syslog_level){\
								LOG_PRI(android_log_lvl_convert[lvl],LOG_TAG,"%d:%s(): " f, __LINE__, __FUNCTION__, ##__VA_ARGS__);}\
						  }while(0)

//just dummy defines since were not including syslog.h.
#define LOG_EMERG	0
#define LOG_ALERT	1
#define LOG_CRIT	2
#define LOG_ERR		3
#define LOG_WARNING	4
#define LOG_NOTICE	5
#define LOG_INFO	6
#define LOG_DEBUG	7

/* Android's log level are in opposite order of syslog.h */
int android_log_lvl_convert[8]={ANDROID_LOG_SILENT, /*8*/
								ANDROID_LOG_SILENT, /*7*/
								ANDROID_LOG_FATAL, /*6*/
								ANDROID_LOG_ERROR,/*5*/
								ANDROID_LOG_WARN,/*4*/
								ANDROID_LOG_INFO,/*3*/
								ANDROID_LOG_DEBUG,/*2*/
								ANDROID_LOG_VERBOSE};/*1*/

#endif /*MUX_ANDROID*/

#define SYSCHECK(c) do{if((c)<0){LOGMUX(LOG_ERR,"system-error: '%s' (code: %d)", strerror(errno), errno);\
						return -1;}\
					}while(0)

/*MUX defines */
#define GSM0710_FRAME_FLAG 0xF9// basic mode flag for frame start and end
#define GSM0710_FRAME_ADV_FLAG 0x7E// advanced mode flag for frame start and end
#define GSM0710_FRAME_ADV_ESC 0x7D// advanced mode escape symbol
#define GSM0710_FRAME_ADV_ESC_COPML 0x20// advanced mode escape complement mask
#define GSM0710_FRAME_ADV_ESCAPED_SYMS { GSM0710_FRAME_ADV_FLAG, GSM0710_FRAME_ADV_ESC, 0x11, 0x91, 0x13, 0x93 }// advanced mode escaped symbols: Flag, Escape, XON and XOFF
// bits: Poll/final, Command/Response, Extension
#define GSM0710_PF 0x10//16
#define GSM0710_CR 0x02//2
#define GSM0710_EA 0x01//1
// type of frames
#define GSM0710_TYPE_SABM 0x2F//47 Set Asynchronous Balanced Mode
#define GSM0710_TYPE_UA 0x63//99 Unnumbered Acknowledgement
#define GSM0710_TYPE_DM 0x0F//15 Disconnected Mode
#define GSM0710_TYPE_DISC 0x43//67 Disconnect
#define GSM0710_TYPE_UIH 0xEF//239 Unnumbered information with header check
#define GSM0710_TYPE_UI 0x03//3 Unnumbered Acknowledgement
// control channel commands
#define GSM0710_CONTROL_PN (0x80|GSM0710_EA)//?? DLC parameter negotiation
#define GSM0710_CONTROL_CLD (0xC0|GSM0710_EA)//193 Multiplexer close down
#define GSM0710_CONTROL_PSC (0x40|GSM0710_EA)//??? Power Saving Control
#define GSM0710_CONTROL_TEST (0x20|GSM0710_EA)//33 Test Command
#define GSM0710_CONTROL_MSC (0xE0|GSM0710_EA)//225 Modem Status Command
#define GSM0710_CONTROL_NSC (0x10|GSM0710_EA)//17 Non Supported Command Response
#define GSM0710_CONTROL_RPN (0x90|GSM0710_EA)//?? Remote Port Negotiation Command
#define GSM0710_CONTROL_RLS (0x50|GSM0710_EA)//?? Remote Line Status Command
#define GSM0710_CONTROL_SNC (0xD0|GSM0710_EA)//?? Service Negotiation Command
// V.24 signals: flow control, ready to communicate, ring indicator,
// data valid three last ones are not supported by Siemens TC_3x
#define GSM0710_SIGNAL_FC 0x02
#define GSM0710_SIGNAL_RTC 0x04
#define GSM0710_SIGNAL_RTR 0x08
#define GSM0710_SIGNAL_IC 0x40//64
#define GSM0710_SIGNAL_DV 0x80//128
#define GSM0710_SIGNAL_DTR 0x04
#define GSM0710_SIGNAL_DSR 0x04
#define GSM0710_SIGNAL_RTS 0x08
#define GSM0710_SIGNAL_CTS 0x08
#define GSM0710_SIGNAL_DCD 0x80//128
//
#define GSM0710_COMMAND_IS(type, command) ((type & ~GSM0710_CR) == command)
#define GSM0710_FRAME_IS(type, frame) ((frame->control & ~GSM0710_PF) == type)
#ifndef min
#define min(a,b) ((a < b) ? a :b)
#endif
#define GSM0710_WRITE_RETRIES 5
#define GSM0710_MAX_CHANNELS 32
// Defines how often the modem is polled when automatic restarting is
// enabled The value is in seconds
#define GSM0710_POLLING_INTERVAL 5
#define GSM0710_BUFFER_SIZE 2048


/**************************/
/* TYPES                                */
/**************************/
typedef struct GSM0710_Frame
{
	unsigned char channel;
	unsigned char control;
	int length;
	unsigned char *data;
} GSM0710_Frame;

typedef struct GSM0710_Buffer
{
	unsigned char data[GSM0710_BUFFER_SIZE];
	unsigned char *readp;
	unsigned char *writep;
	unsigned char *endp;
	unsigned int datacount;
	int newdataready; /*newdataready = 1: new data written to internal buffer. newdataready=0: acknowledged by assembly thread*/
	int input_sleeping; /*input_sleeping = 1 if ser_read_thread (input to buffer) is waiting because buffer is full */
	int flag_found;// set if last character read was flag
	unsigned long received_count;
	unsigned long dropped_count;
	unsigned char adv_data[GSM0710_BUFFER_SIZE];
	int adv_length;
	int adv_found_esc;
} GSM0710_Buffer;

typedef struct Channel // Channel data
{
	int id; // gsm 07 10 channel id
	char* devicename;
	int fd;
	int opened;
	unsigned char v24_signals;
	char* ptsname;
	char* origin;
	int remaining;
	unsigned char *tmp;
	int reopen;
	int disc_ua_pending;
} Channel;

typedef enum MuxerStates
{
	MUX_STATE_OPENING,
	MUX_STATE_INITILIZING,
	MUX_STATE_MUXING,
	MUX_STATE_CLOSING,
	MUX_STATE_OFF,
	MUX_STATES_COUNT // keep this the last
} MuxerStates;

typedef struct Serial
{
	char *devicename;
	int fd;
	MuxerStates state;
	GSM0710_Buffer *in_buf;// input buffer
	unsigned char *adv_frame_buf;
	time_t frame_receive_time;
	int ping_number;
} Serial;

/* Struct is used for passing fd, read function and read funtion arg to a device polling thread */
typedef struct Poll_Thread_Arg
{
int fd;
int (*read_function_ptr)(void *);
void * read_function_arg;
}Poll_Thread_Arg;


/**************************/
/* FUNCTION PROTOTYPES      */
/**************************/
/* increases buffer pointer by one and wraps around if necessary */
//void gsm0710_buffer_inc(GSM0710_Buffer *buf, void&* p);
#define gsm0710_buffer_inc(readp,datacount) do { readp++; datacount--; \
                                       if (readp == buf->endp) readp = buf->data; \
                                     } while (0)

/* Tells how many chars are saved into the buffer. */
//int gsm0710_buffer_length(GSM0710_Buffer *buf);
//#define gsm0710_buffer_length(buf) ((buf->readp > buf->writep) ? (GSM0710_BUFFER_SIZE - (buf->readp - buf->writep)) : (buf->writep-buf->readp))
#define gsm0710_buffer_length(buf) (buf->datacount)


/* tells how much free space there is in the buffer */
//int gsm0710_buffer_free(GSM0710_Buffer *buf);
//#define gsm0710_buffer_free(buf) ((buf->readp > buf->writep) ? ((buf->readp - buf->writep)-1) : (GSM0710_BUFFER_SIZE - (buf->writep-buf->readp))-1)
#define gsm0710_buffer_free(buf) (GSM0710_BUFFER_SIZE - buf->datacount)


static int watchdog(Serial * serial);
static int close_devices();
static int thread_serial_device_read(void * vargp);
static int pseudo_device_read(void * vargp);
static void* poll_thread(void* vargp);
static void* assemble_frame_thread(void* vargp);
static int create_thread(pthread_t * thread_id, void * thread_function, void * thread_function_arg );
static void set_main_exit_signal(int signal);
static int restart_pty_interface(Channel* channel);



/**************************/
/* CONSTANTS & GLOBALS     */
/**************************/
static unsigned char close_channel_cmd[] = { GSM0710_CONTROL_CLD | GSM0710_CR, GSM0710_EA | (0 << 1) };
static unsigned char test_channel_cmd[] = { GSM0710_CONTROL_TEST | GSM0710_CR, GSM0710_EA | (6 << 1), 'P', 'I', 'N', 'G', '\r', '\n', };
//static unsigned char psc_channel_cmd[] = { GSM0710_CONTROL_PSC | GSM0710_CR, GSM0710_EA | (0 << 1), };
//static unsigned char wakeup_sequence[] = { GSM0710_FRAME_FLAG, GSM0710_FRAME_FLAG, };
/* crc table from gsm0710 spec */
static const unsigned char r_crctable[] = {//reversed, 8-bit, poly=0x07
	0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, 0x0E, 0x9F, 0xED,
	0x7C, 0x09, 0x98, 0xEA, 0x7B, 0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A,
	0xF8, 0x69, 0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67, 0x38,
	0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D, 0x36, 0xA7, 0xD5, 0x44,
	0x31, 0xA0, 0xD2, 0x43, 0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0,
	0x51, 0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F, 0x70, 0xE1,
	0x93, 0x02, 0x77, 0xE6, 0x94, 0x05, 0x7E, 0xEF, 0x9D, 0x0C, 0x79,
	0xE8, 0x9A, 0x0B, 0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19,
	0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17, 0x48, 0xD9, 0xAB,
	0x3A, 0x4F, 0xDE, 0xAC, 0x3D, 0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0,
	0xA2, 0x33, 0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21, 0x5A,
	0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F, 0xE0, 0x71, 0x03, 0x92,
	0xE7, 0x76, 0x04, 0x95, 0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A,
	0x9B, 0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89, 0xF2, 0x63,
	0x11, 0x80, 0xF5, 0x64, 0x16, 0x87, 0xD8, 0x49, 0x3B, 0xAA, 0xDF,
	0x4E, 0x3C, 0xAD, 0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
	0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1, 0xCA, 0x5B, 0x29,
	0xB8, 0xCD, 0x5C, 0x2E, 0xBF, 0x90, 0x01, 0x73, 0xE2, 0x97, 0x06,
	0x74, 0xE5, 0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB, 0x8C,
	0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9, 0x82, 0x13, 0x61, 0xF0,
	0x85, 0x14, 0x66, 0xF7, 0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C,
	0xDD, 0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3, 0xB4, 0x25,
	0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1, 0xBA, 0x2B, 0x59, 0xC8, 0xBD,
	0x2C, 0x5E, 0xCF, };
// config stuff
static char* revision = "$Rev: 1 $";
static int no_daemon = 0;
static int pin_code = -1;
static int use_ping = 0;
static int use_timeout = 0;
static int logtofile = 0;
static int syslog_level = LOG_INFO;
static FILE * muxlogfile;
static int vir_ports = 2; /* number of virtual ports to create */
/*misc global vars */
static int main_exit_signal=0;  /* 1:main() received exit signal */
static int uih_pf_bit_received = 0;
static unsigned int pts_reopen=0; /*If != 0,  signals watchdog that one cahnnel needs to be reopened */

/*pthread */
pthread_t ser_read_thread;
pthread_t frame_assembly_thread;
pthread_t pseudo_terminal[GSM0710_MAX_CHANNELS-1]; /* -1 because control channel cannot be mapped to pseudo-terminal /dev/pts/* */
pthread_cond_t newdataready_signal = PTHREAD_COND_INITIALIZER;
pthread_cond_t bufferready_signal = PTHREAD_COND_INITIALIZER;
pthread_attr_t thread_attr;
pthread_mutex_t syslogdump_lock;
pthread_mutex_t write_frame_lock;
pthread_mutex_t main_exit_signal_lock;
pthread_mutex_t pts_reopen_lock;
pthread_mutex_t datacount_lock;
pthread_mutex_t newdataready_lock;
pthread_mutex_t bufferready_lock;

// serial io
static Serial serial;
// muxed io channels
static Channel channellist[GSM0710_MAX_CHANNELS]; // remember: [0] is not used acticly because it's the control channel
// some state
// +CMUX=<mode>[,<subset>[,<port_speed>[,<N1>[,<T1>[,<N2>[,<T2>[,<T3>[,<k>]]]]]]]]
static int cmux_mode = 1;
static int cmux_subset = 0;
static int cmux_port_speed = 5; //115200 baud rate
// Maximum Frame Size (N1): 64/31
static int cmux_N1 = 1509;
#if 0
// Acknowledgement Timer (T1) sec/100: 10
static int cmux_T1 = 10;
// Maximum number of retransmissions (N2): 3
static int cmux_N2 = 3;
// Response Timer for multiplexer control channel (T2) sec/100: 30
static int cmux_T2 = 30;
// Response Timer for wake-up procedure(T3) sec: 10
static int cmux_T3 = 10;
// Window Size (k): 2
static int cmux_k = 2;
#endif

/*
 * The following arrays must have equal length and the values must
 * correspond. also it has to correspond to the gsm0710 spec regarding
 * baud id of CMUX the command.
 */
static int baud_rates[] = {
	0, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};
static speed_t baud_bits[] = {
	0, B9600, B19200, B38400, B57600, B115200, B230400, B460800, B921600
};


/**************************/
/* MAIN CODE                        */
/**************************/
/*
* Purpose:  Determine baud-rate index for CMUX command
* Input:	    baud_rate - baud rate (eg. 460800)
* Return:    -1 if fail, i - baud rate index if success
*/
static int baud_rate_index(
	int baud_rate)
{
	unsigned int i;
	for (i = 0; i < sizeof(baud_rates) / sizeof(*baud_rates); ++i)
		if (baud_rates[i] == baud_rate)
			return i;
	return -1;
}

/*
* Purpose:  Calculates frame check sequence from given characters.
* Input:	    input - character array
*                length - number of characters in array (that are included)
* Return:    frame check sequence
*/
static unsigned char frame_calc_crc(
	const unsigned char *input,
	int length)
{
	unsigned char fcs = 0xFF;
	int i;
	for (i = 0; i < length; i++)
		fcs = r_crctable[fcs ^ input[i]];
	return 0xFF - fcs;
}

/*
* Purpose:  Escapes GSM0710_FRAME_ADV_ESCAPED_SYMS characters.
* Input:	    adv_buf - pointer to the new buffer with the escaped content
*                data - pointer to the char buffer to be parsed
*                length - the length of the data char buffer
* Return:    adv_i - number of added escape chars
*/
static int fill_adv_frame_buf(
	unsigned char *adv_buf,
	const unsigned char *data,
	int length)
{
	static const unsigned char esc[] = GSM0710_FRAME_ADV_ESCAPED_SYMS;
	int i, esc_i, adv_i = 0;
	for (i = 0; i < length; ++i, ++adv_i)
	{
		adv_buf[adv_i] = data[i];
		for (esc_i = 0; esc_i < sizeof(esc) / sizeof(esc[0]); ++esc_i)
			if (data[i] == esc[esc_i])
			{
				adv_buf[adv_i] = GSM0710_FRAME_ADV_ESC;
				adv_i++;
				adv_buf[adv_i] = data[i] ^ GSM0710_FRAME_ADV_ESC_COPML;
				break;
			}
	}
	return adv_i;
}

/*
* Purpose:  ascii/hexdump a byte buffer 
* Input:	    prefix - string to printed before hex data on every line
*                ptr - the string to be dumped
*                length - the length of the string to be dumped
* Return:    0
*/
static int syslogdump(
	const char *prefix,
	const unsigned char *ptr,
	unsigned int length)
{
if (LOG_DEBUG>syslog_level) /*No need for all frame logging if it's not to be seen */
	return 0;

	char buffer[100];
	unsigned int offset = 0l;
	int i;
	
	pthread_mutex_lock(&syslogdump_lock); 	//new lock
	while (offset < length)
	{
		int off;
		strcpy(buffer, prefix);
		off = strlen(buffer);
		SYSCHECK(snprintf(buffer + off, sizeof(buffer) - off, "%08x: ", offset));
		off = strlen(buffer);
		for (i = 0; i < 16; i++)
		{
			if (offset + i < length){
				SYSCHECK(snprintf(buffer + off, sizeof(buffer) - off, "%02x%c", ptr[offset + i], i == 7 ? '-' : ' '));
				}
			else{
				SYSCHECK(snprintf(buffer + off, sizeof(buffer) - off, " .%c", i == 7 ? '-' : ' '));
				}
			off = strlen(buffer);
		}
		SYSCHECK(snprintf(buffer + off, sizeof(buffer) - off, " "));
		off = strlen(buffer);
		for (i = 0; i < 16; i++)
			if (offset + i < length)
			{
				SYSCHECK(snprintf(buffer + off, sizeof(buffer) - off, "%c", (ptr[offset + i] < ' ') ? '.' : ptr[offset + i]));
				off = strlen(buffer);
			}
		offset += 16;
		LOGMUX(LOG_DEBUG,"%s", buffer);
	}
	pthread_mutex_unlock(&syslogdump_lock);/*new lock*/

	return 0;
}

/*
* Purpose:  Writes a frame to a logical channel. C/R bit is set to 1. 
*                Doesn't support FCS counting for GSM0710_TYPE_UI frames.
* Input:	    channel - channel number (0 = control)
*                input - the data to be written
*                length - the length of the data
*                type - the type of the frame (with possible P/F-bit)
* Return:    number of characters written
*/
static int write_frame(
	int channel,
	const unsigned char *input,
	int length,
	unsigned char type)
{
	/* new lock */
	pthread_mutex_lock(&write_frame_lock);
	LOGMUX(LOG_DEBUG, "Enter");
/* flag, GSM0710_EA=1 C channel, frame type, length 1-2 */
	unsigned char prefix[5] = { GSM0710_FRAME_FLAG, GSM0710_EA | GSM0710_CR, 0, 0, 0 };
	unsigned char postfix[2] = { 0xFF, GSM0710_FRAME_FLAG };
	int prefix_length = 4;
	int c;
//	char w = 0;
//	int count = 0;
//	do
//	{
//commented out wakeup sequence:
// 		syslogdump(">s ", (unsigned char *)wakeup_sequence, sizeof(wakeup_sequence));
//		write(serial.fd, wakeup_sequence, sizeof(wakeup_sequence));
//		SYSCHECK(tcdrain(serial.fd));
//		fd_set rfds;
//		FD_ZERO(&rfds);
//		FD_SET(serial.fd, &rfds);
//		struct timeval timeout;
//		timeout.tv_sec = 0;
//		timeout.tv_usec = 1000 / 100 * cmux_T2;
//		int sel = select(serial.fd + 1, &rfds, NULL, NULL, &timeout);
//		if (sel > 0 && FD_ISSET(serial.fd, &rfds))
//			read(serial.fd, &w, 1);
//		else
//			count++;
//	} while (w != wakeup_sequence[0] && count < cmux_N2);
//	if (w != wakeup_sequence[0])
//		LOGMUX(LOG_WARNING, "Didn't get frame-flag after wakeup");
	LOGMUX(LOG_DEBUG, "Sending frame to channel %d", channel);
/* GSM0710_EA=1, Command, let's add address */
	prefix[1] = prefix[1] | ((63 & (unsigned char) channel) << 2);
/* let's set control field */
	prefix[2] = type;
	if ((type ==  GSM0710_TYPE_UIH || type ==  GSM0710_TYPE_UI) &&
	    uih_pf_bit_received == 1 &&
	    GSM0710_COMMAND_IS(input[0],GSM0710_CONTROL_MSC) ){
	  prefix[2] = prefix[2] | GSM0710_PF; //Set the P/F bit in Response if Command from modem had it set
	  uih_pf_bit_received = 0; //Reset the variable, so it is ready for next command
	}
/* let's not use too big frames */
	length = min(cmux_N1, length);
	if (!cmux_mode)
	{
/* Modified acording PATCH CRC checksum */
/* postfix[0] = frame_calc_crc (prefix + 1, prefix_length - 1); */
/* length */
		if (length > 127)
		{
			prefix_length = 5;
			prefix[3] = (0x007F & length) << 1;
			prefix[4] = (0x7F80 & length) >> 7;
		}
		else
			prefix[3] = 1 | (length << 1);
		postfix[0] = frame_calc_crc(prefix + 1, prefix_length - 1);
		syslogdump(">s ", prefix,prefix_length); /* syslogdump for basic mode */
		c = write(serial.fd, prefix, prefix_length);
		if (c != prefix_length)
		{
			LOGMUX(LOG_WARNING, "Couldn't write the whole prefix to the serial port for the virtual port %d. Wrote only %d bytes",
				channel, c);
			return 0;
		}
		if (length > 0)
		{
		syslogdump(">s ", input,length); /* syslogdump for basic mode */
			c = write(serial.fd, input, length);
			if (length != c)
			{
				LOGMUX(LOG_WARNING, "Couldn't write all data to the serial port from the virtual port %d. Wrote only %d bytes",
					channel, c);
				return 0;
			}
		}
		syslogdump(">s ", postfix,2); /* syslogdump for basic mode */
		c = write(serial.fd, postfix, 2);
		if (c != 2)
		{
			LOGMUX(LOG_WARNING, "Couldn't write the whole postfix to the serial port for the virtual port %d. Wrote only %d bytes",
				channel, c);
			return 0;
		}
	}
	else/* cmux_mode */
	{
		int offs = 1;
		serial.adv_frame_buf[0] = GSM0710_FRAME_ADV_FLAG;
		offs += fill_adv_frame_buf(serial.adv_frame_buf + offs, prefix + 1, 2);/* address, control */
		offs += fill_adv_frame_buf(serial.adv_frame_buf + offs, input, length);/* data */
		/* CRC checksum */
		postfix[0] = frame_calc_crc(prefix + 1, 2);
		offs += fill_adv_frame_buf(serial.adv_frame_buf + offs, postfix, 1);/* fcs */
		serial.adv_frame_buf[offs] = GSM0710_FRAME_ADV_FLAG;
		offs++;
		syslogdump(">s ", (unsigned char *)serial.adv_frame_buf, offs);
		c = write(serial.fd, serial.adv_frame_buf, offs);
		if (c != offs)
		{
			LOGMUX(LOG_WARNING, "Couldn't write the whole advanced packet to the serial port for the virtual port %d. Wrote only %d bytes",
				channel, c);
			return 0;
		}
	}
	LOGMUX(LOG_DEBUG, "Leave");
	//new lock
	pthread_mutex_unlock(&write_frame_lock);
	return length;
}

/*
* Purpose:  Handles received data from pseudo terminal device (application)
* Input:	    buf - buffer, which contains received data
*                len - the length of the buffer channel
*                channel - logical channel id where data was received
* Return:    The number of remaining bytes in partial packet
*/
static int handle_channel_data(
	unsigned char *buf,
	int len,
	int channel)
{
	int written = 0;
	int i = 0;
	int last = 0;
/* try to write 5 times */
	while (written != len && i < GSM0710_WRITE_RETRIES)
	{
		last = write_frame(channel, buf + written, len - written, GSM0710_TYPE_UIH);
		written += last;
		if (last == 0)
			i++;
	}
	if (i == GSM0710_WRITE_RETRIES)
		LOGMUX(LOG_WARNING, "Couldn't write data to channel %d. Wrote only %d bytes, when should have written %d",
				channel, written, len);
	return 0;
}

/* 
* Purpose:  Close mux logical channel
* Input:      channel - logical channel struct
* Return:    0
*/
static int logical_channel_close(Channel* channel)
{
	if (channel->fd >= 0)
		close(channel->fd);
	channel->fd = -1;
	if (channel->ptsname != NULL)
		free(channel->ptsname);
	channel->ptsname = NULL;
	if (channel->tmp != NULL)
		free(channel->tmp);
	channel->tmp = NULL;
	if (channel->origin != NULL)
		free(channel->origin);
	channel->origin = NULL;
	channel->opened = 0;
	channel->v24_signals = 0;
	channel->remaining = 0;
	return 0;
}

/* 
* Purpose:  Initialize mux logical channel
* Input:      channel - logical channel struct
*                id - logical channel id number
* Return:    0
*/
static int logical_channel_init(Channel* channel, int id)
{
	channel->id = id; // connected channel-id
	channel->devicename = id?"/dev/ptmx":NULL; // TODO do we need this to be dynamic anymore?
	channel->fd = -1;
	channel->ptsname = NULL;
	channel->tmp = NULL;
	channel->origin = NULL;
	channel->reopen = 0;
	channel->disc_ua_pending = 0;
	return logical_channel_close(channel);
}

/* 
* Purpose:  Restart pseudo terminal device and reading thread, connect it to existing mux channel
* Input:      pointer to Channel struct
* Return:    1 if fail, 0 if success
*/
int restart_pty_interface(Channel* channel)
{
	if (channel->fd < 0) // is this channel free?
				{
					SYSCHECK(channel->fd = open(channel->devicename, O_RDWR | O_NONBLOCK)); //open pseudo terminal devices from /dev/ptmx master
					char* pts = ptsname(channel->fd);
					if (pts == NULL) SYSCHECK(-1);
					channel->ptsname = strdup(pts);
					struct termios options;
					tcgetattr(channel->fd, &options); //get the parameters
					options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); //set raw input
					options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
					options.c_oflag &= ~(OPOST| OLCUC| ONLRET| ONOCR| OCRNL); //set raw output
					tcsetattr(channel->fd, TCSANOW, &options);
					if (!strcmp(channel->devicename, "/dev/ptmx"))
					{
						//Otherwise programs cannot access the pseudo terminals
						SYSCHECK(grantpt(channel->fd));
						SYSCHECK(unlockpt(channel->fd));
					}
					channel->v24_signals = GSM0710_SIGNAL_DV | GSM0710_SIGNAL_RTR | GSM0710_SIGNAL_RTC | GSM0710_EA;
					//create thread
					LOGMUX(LOG_INFO, "Reopened %s, channel number: %d fd: %d ",channel->ptsname, channel->id, channel->fd);
					Poll_Thread_Arg* poll_thread_arg = (Poll_Thread_Arg*) malloc(sizeof(Poll_Thread_Arg)); //iniitialize pointer to thread args
					poll_thread_arg->fd = channel->fd;
					poll_thread_arg->read_function_ptr = &pseudo_device_read;
					poll_thread_arg->read_function_arg = (void *) (channel);
					if(create_thread(&pseudo_terminal[channel->id], poll_thread,(void*) poll_thread_arg)!=0){ //create thread for reading input from virtual port
					  LOGMUX(LOG_ERR,"Could not restart thread for listening on %s", channel->ptsname);
					  return 1;
					}
					LOGMUX(LOG_INFO, "Restarted thread listening on %s", channel->ptsname);				
		}
	return 0;
}

/* 
* Purpose:  Read from line discipline buffer. (input from application connected to pseudo terminal)
* Input:      void pointer to Channel struct
* Return:    1 if fail, 0 if success
*/
int pseudo_device_read(void * vargp)
{
	LOGMUX(LOG_DEBUG, "Enter");
	Channel* channel = (Channel*)vargp;
		unsigned char buf[4096];
		/* information from virtual port */
		int len = read(channel->fd, buf + channel->remaining, sizeof(buf) - channel->remaining);
		if (!channel->opened)
		{
			LOGMUX(LOG_WARNING, "Write to a channel which wasn't acked to be open.");
			write_frame(channel->id, NULL, 0, GSM0710_TYPE_SABM | GSM0710_PF);
			LOGMUX(LOG_DEBUG, "Leave");
			return 1;
		}
		if (len >= 0)
		{
			LOGMUX(LOG_DEBUG, "Data from channel %d, %d bytes", channel->id, len);
			if (channel->remaining > 0)
			{
				memcpy(buf, channel->tmp, channel->remaining);
				free(channel->tmp);
				channel->tmp = NULL;
			}
			if (len + channel->remaining > 0)
				channel->remaining = handle_channel_data(buf, len + channel->remaining, channel->id);
			/* copy remaining bytes from last packet into tmp */
			if (channel->remaining > 0)
			{
				channel->tmp = malloc(channel->remaining);
				memcpy(channel->tmp, buf + sizeof(buf) - channel->remaining, channel->remaining);
			}
			LOGMUX(LOG_DEBUG, "Leave");
			return 0;
		}
		else{
			switch (errno){
			  case EINTR:
				LOGMUX(LOG_ERR,"Interrupt signal EINTR caught");
				break;
			  case EAGAIN:
				LOGMUX(LOG_ERR,"Interrupt signal EAGAIN caught");
				break;
			  case EBADF:
				LOGMUX(LOG_ERR,"Interrupt signal EBADF caught");
				break;
			  case EINVAL:
				LOGMUX(LOG_ERR,"Interrupt signal EINVAL caught");
				break;
			  case EFAULT:
				LOGMUX(LOG_ERR,"Interrupt signal EFAULT caught");
				break;
			  case EIO:
				LOGMUX(LOG_ERR,"Interrupt signal EIO caught");
				break;
			  case EISDIR:
				LOGMUX(LOG_ERR,"Interrupt signal EISDIR caught");
				break;
			  default:
				LOGMUX(LOG_ERR,"Unknown interrupt signal caught\n");
			}
			// dropped connection. close channel but re-open afterwards

	LOGMUX(LOG_INFO, "Appl. dropped connection, device %s shutting down. Set to be reopened", channel->ptsname);
		/*disconnect channel from pty*/
		close(channel->fd);
		channel->fd = -1;
		free(channel->ptsname);
		channel->ptsname = NULL;
		pthread_mutex_lock(&pts_reopen_lock);
		pts_reopen = 1; /*global flag to signal at least one channel needs to be reopened */
		pthread_mutex_unlock(&pts_reopen_lock);
		channel->reopen = 1; /* set channel to be reopened. this will not be cleared when doing a channel close */
		}
	LOGMUX(LOG_DEBUG, "Leave");
	return 1;
}

/* 
* Purpose:  Allocate a channel and corresponding virtual port and a start a reading thread on that port
* Input:      origin - string to define origin of allocation
* Return:    1 if fail, 0 if success
*/
static int c_alloc_channel(const char* origin, pthread_t * thread_id)
{
	LOGMUX(LOG_DEBUG, "Enter");
	int i;
	if (serial.state == MUX_STATE_MUXING)
		for (i=1;i<GSM0710_MAX_CHANNELS;i++)
			if (channellist[i].fd < 0) // is this channel free?
			{
				LOGMUX(LOG_DEBUG, "Found free channel %d fd %d on %s", i, channellist[i].fd, channellist[i].devicename);
				channellist[i].origin = strdup(origin);
				SYSCHECK(channellist[i].fd = open(channellist[i].devicename, O_RDWR | O_NONBLOCK)); //open pseudo terminal devices from /dev/ptmx master
				char* pts = ptsname(channellist[i].fd);
				if (pts == NULL) SYSCHECK(-1);
				channellist[i].ptsname = strdup(pts);
				struct termios options;
				tcgetattr(channellist[i].fd, &options); //get the parameters
				options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); //set raw input
				options.c_iflag &= ~(INLCR | ICRNL | IGNCR);
				options.c_oflag &= ~(OPOST| OLCUC| ONLRET| ONOCR| OCRNL); //set raw output
				tcsetattr(channellist[i].fd, TCSANOW, &options);
				if (!strcmp(channellist[i].devicename, "/dev/ptmx"))
				{
					//Otherwise programs cannot access the pseudo terminals
					SYSCHECK(grantpt(channellist[i].fd));
					SYSCHECK(unlockpt(channellist[i].fd));
				}
				channellist[i].v24_signals = GSM0710_SIGNAL_DV | GSM0710_SIGNAL_RTR | GSM0710_SIGNAL_RTC | GSM0710_EA;
				//create thread
                LOGMUX(LOG_DEBUG, "New channel properties: number: %d fd: %d device: %s", i, channellist[i].fd, channellist[i].devicename);
                Poll_Thread_Arg* poll_thread_arg = (Poll_Thread_Arg*) malloc(sizeof(Poll_Thread_Arg)); //iniitialize pointer to thread args
                poll_thread_arg->fd = channellist[i].fd;
                poll_thread_arg->read_function_ptr = &pseudo_device_read;
                poll_thread_arg->read_function_arg = (void *) (channellist+i);
              	if(create_thread(thread_id, poll_thread,(void*) poll_thread_arg)!=0){ //create thread for reading input from virtual port
              	  LOGMUX(LOG_ERR,"Could not create thread for listening on %s", channellist[i].ptsname);
              	  return 1;
              	}
              	LOGMUX(LOG_DEBUG, "Thread is running and listening on %s", channellist[i].ptsname);

				write_frame(i, NULL, 0, GSM0710_TYPE_SABM | GSM0710_PF); //should be moved?? messy
				LOGMUX(LOG_INFO, "Connecting %s to virtual channel %d for %s on %s",
					channellist[i].ptsname, channellist[i].id, channellist[i].origin, serial.devicename);
				return 0;
			}
	LOGMUX(LOG_ERR, "Not muxing or no free channel found");
	return 1;
}

/* 
* Purpose:  Allocates memory for a new buffer and initializes it.
* Input:      -
* Return:    pointer to a new buffer
*/
static GSM0710_Buffer *gsm0710_buffer_init()
{
	GSM0710_Buffer* buf = (GSM0710_Buffer*)malloc(sizeof(GSM0710_Buffer));
	if (buf)
	{
		memset(buf, 0, sizeof(GSM0710_Buffer));
		buf->readp = buf->data;
		buf->writep = buf->data;
		buf->endp = buf->data + GSM0710_BUFFER_SIZE;
	}
	return buf;
}

/* 
* Purpose:  Destroys the buffer (i.e. frees up the memory
* Input:      buf - buffer to be destroyed
* Return:    -
*/
static void gsm0710_buffer_destroy(
	GSM0710_Buffer* buf)
{
	free(buf);
}

/* 
* Purpose:  Writes data to the buffer
* Input:      buf - pointer to the buffer
*                input - input data (in user memory)
*                length - how many characters should be written
* Return:    number of characters written
*/
static int gsm0710_buffer_write(
	GSM0710_Buffer* buf,
	const unsigned char *input,
	int length)
{
	LOGMUX(LOG_DEBUG, "Enter");
	LOGMUX(LOG_DEBUG,"GSM0710 buffer (up-to-date): free %d, stored %d", gsm0710_buffer_free(buf),gsm0710_buffer_length(buf));
	int c = buf->endp - buf->writep;
	length = min(length, gsm0710_buffer_free(buf));
	if (length > c)
	{
		memcpy(buf->writep, input, c);
		memcpy(buf->data, input + c, length - c);
		buf->writep = buf->data + (length - c);
	}
	else
	{
		memcpy(buf->writep, input, length);
		buf->writep += length;
		if (buf->writep == buf->endp)
			buf->writep = buf->data;
	}

	pthread_mutex_lock(&datacount_lock); 
	buf->datacount += length; /*updating the data-not-yet-read counter*/
	LOGMUX(LOG_DEBUG,"GSM0710 buffer (up-to-date): written %d, free %d, stored %d", length,gsm0710_buffer_free(buf),gsm0710_buffer_length(buf));
	pthread_mutex_unlock(&datacount_lock);
	
	pthread_mutex_lock(&newdataready_lock);
	buf->newdataready = 1; /*signal assembly thread that new buffer data is ready*/
	pthread_mutex_unlock(&newdataready_lock);
	pthread_cond_signal(&newdataready_signal);
	LOGMUX(LOG_DEBUG,"Leave");
	return length;
}

/* 
* Purpose:  destroys a frame
* Input:      frame - pointer to the frame
* Return:    -
*/ 
static void destroy_frame(
	GSM0710_Frame * frame)
{
	if (frame->length > 0)
		free(frame->data);
	free(frame);
}

/* 
* Purpose:  Gets a complete basic mode frame from buffer. You have to remember to free this frame
*                when it's not needed anymore
* Input:      buf - the buffer, where the frame is extracted
* Return:    frame or null, if there isn't ready frame with given index
*/
static GSM0710_Frame* gsm0710_base_buffer_get_frame(
	GSM0710_Buffer * buf)
{
	int end;
	int length_needed = 5;// channel, type, length, fcs, flag
	unsigned char fcs = 0xFF;
	GSM0710_Frame *frame = NULL;
	unsigned char *local_readp;
	unsigned int local_datacount, local_datacount_backup;
	LOGMUX(LOG_DEBUG, "Enter");

/*Find start flag*/
	while (!buf->flag_found && gsm0710_buffer_length(buf) > 0)
	{
		if (*buf->readp == GSM0710_FRAME_FLAG)
			buf->flag_found = 1;
		pthread_mutex_lock(&datacount_lock); /* need to lock to operate on buf->datacount*/
		gsm0710_buffer_inc(buf->readp,buf->datacount); 
		pthread_mutex_unlock(&datacount_lock); 
	}
	if (!buf->flag_found)// no frame started
		{
		LOGMUX(LOG_DEBUG, "Leave. No start frame 0xf9 found in bytes stored in GSM0710 buffer");
		return NULL;
		}
/*skip empty frames (this causes troubles if we're using DLC 62) - skipping frame start flags*/
	while (gsm0710_buffer_length(buf) > 0 && (*buf->readp == GSM0710_FRAME_FLAG))
	{
		pthread_mutex_lock(&datacount_lock); /* need to lock to operate on buf->datacount*/
		gsm0710_buffer_inc(buf->readp,buf->datacount);
		pthread_mutex_unlock(&datacount_lock); 

	}
/* Okay, we're ready to analyze a proper frame header */

	/*Make local copy of buffer pointer and data counter. They are shared between 2 threads, so we want to update them only after a frame extraction */
	/*From now on, only operate on these local copies */
	local_readp = buf->readp;
	local_datacount = local_datacount_backup = buf->datacount; /* current no. of stored bytes in buffer */

	if (local_datacount >= length_needed) /* enough data stored for 0710 frame header+footer? */
	{
		if ((frame = (GSM0710_Frame*)malloc(sizeof(GSM0710_Frame))) != NULL)
		{
			frame->channel = ((*local_readp & 252) >> 2); /*frame header address-byte read*/
			if (frame->channel > vir_ports ) /* Field Sanity check if channel ID actually exists */
			{
				LOGMUX(LOG_WARNING, "Dropping frame: Corrupt! Channel Addr. field indicated %d, which does not exist",frame->channel);
				destroy_frame(frame);
				buf->flag_found = 0;
				buf->dropped_count++;
				goto update_buffer_dropping_frame; /* throw whole frame away, up until and incl. local_readp */
			}		
			fcs = r_crctable[fcs ^ *local_readp];
			gsm0710_buffer_inc(local_readp,local_datacount);
			length_needed--;
			frame->control = *local_readp; /*frame header type-byte read*/
			fcs = r_crctable[fcs ^ *local_readp];
			gsm0710_buffer_inc(local_readp,local_datacount);
			length_needed--;
			frame->length = (*local_readp & 254) >> 1; /*Frame header 1st length-byte read*/
			fcs = r_crctable[fcs ^ *local_readp];
		}
		else
			LOGMUX(LOG_ERR, "Out of memory, when allocating space for frame");
		if ((*local_readp & 1) == 0)/*if frame payload length byte extension bit not set, a 2nd length byte is in header*/
		{
			//Current spec (version 7.1.0) states these kind of
			//frames to be invalid Long lost of sync might be
			//caused if we would expect a long frame because of an
			//error in length field.
			gsm0710_buffer_inc(local_readp,local_datacount);
			frame->length += (*local_readp*128); /*Frame header 2nd length-byte read*/
			fcs = r_crctable[fcs^*local_readp];
		}
		length_needed += frame->length; /*length_needed : 1 length byte + payload + 1 fcs byte + 1 end frame flag */
		LOGMUX(LOG_DEBUG, "length_needed: %d, available in local_datacount: %d",length_needed,local_datacount);

		if (frame->length > cmux_N1) /* Field Sanity check if payload is bigger than the max size negotiated in +CMUX */
		{
			LOGMUX(LOG_WARNING, "Dropping frame: Corrupt! Length field indicated %d. Max %d allowed",frame->length, cmux_N1);
			destroy_frame(frame);
			buf->flag_found = 0;
			buf->dropped_count++;
			goto update_buffer_dropping_frame; /* throw whole frame away, up until and incl. local_readp */
		}
		if (!(local_datacount >= length_needed))
		{
			free(frame);
			LOGMUX(LOG_DEBUG, "Leave, frame extraction cancelled. Frame not completely stored in re-assembly buffer yet");
			return NULL;
		}		
		gsm0710_buffer_inc(local_readp,local_datacount);

/*Okay, done with the frame header. Start extracting the payload data */
		if (frame->length > 0)
		{
			if ((frame->data = malloc(sizeof(char) * frame->length)) != NULL)
			{
				end = buf->endp - local_readp;
				if (frame->length > end) /*wrap-around necessary*/
				{
					memcpy(frame->data, local_readp, end);
					memcpy(frame->data + end, buf->data, frame->length - end);
					local_readp = buf->data + (frame->length - end);
					local_datacount -= frame->length;
				}
				else
				{
					memcpy(frame->data, local_readp, frame->length);
					local_readp += frame->length;
					local_datacount -= frame->length;
					if (local_readp == buf->endp)
						local_readp = buf->data;
				}
				if (GSM0710_FRAME_IS(GSM0710_TYPE_UI, frame))
				{
					for (end = 0; end < frame->length; end++)
						fcs = r_crctable[fcs ^ (frame->data[end])];
				}
			}
			else
			{
				LOGMUX(LOG_ERR, "Out of memory, when allocating space for frame data");
				frame->length = 0;
			}
		}
/*Okay, check FCS*/
		if (r_crctable[fcs ^ (*local_readp)] != 0xCF)
		{
			gsm0710_buffer_inc(local_readp,local_datacount);
			if (*local_readp != GSM0710_FRAME_FLAG) /* the FCS didn't match, but the next byte may not even be an end-frame-flag*/
			{
				LOGMUX(LOG_WARNING, "Dropping frame: Corrupt! End flag not present and FCS mismatch.");
				destroy_frame(frame);
				buf->flag_found = 0;
				buf->dropped_count++;
				goto update_buffer_dropping_frame; /* throw whole frame away, up until and incl. local_readp */
			}
			else 
			{
			LOGMUX(LOG_WARNING, "Dropping frame: FCS doesn't match");
			destroy_frame(frame);
			buf->flag_found = 0;
			buf->dropped_count++;
			goto update_buffer_dropping_frame; /* throw whole frame away, up until and incl. local_readp */
			}
		}
		else
		{
/*Okay, check end flag */
			gsm0710_buffer_inc(local_readp,local_datacount);
			if (*local_readp != GSM0710_FRAME_FLAG)
			{
				LOGMUX(LOG_WARNING, "Dropping frame: End flag not present. Instead: %d", *local_readp);
				destroy_frame(frame);
				buf->flag_found = 0;
				buf->dropped_count++;
				goto update_buffer_dropping_frame;
			}
			else
				buf->received_count++;
			gsm0710_buffer_inc(local_readp,local_datacount); /* prepare readp for next frame extraction */
		}
	}
	else 
	{
		LOGMUX(LOG_DEBUG, "Leave, not enough bytes stored in buffer for header information yet");
		return NULL;
	}

/* Everything went fine, update GSM0710 buffer pointer and counter */
	pthread_mutex_lock(&datacount_lock); 
	buf->readp = local_readp;
	buf->datacount -= (local_datacount_backup - local_datacount); /* subtract whatever we analyzed */
	pthread_mutex_unlock(&datacount_lock); 
	buf->flag_found = 0; /* prepare for any future frame processing*/
	LOGMUX(LOG_DEBUG, "Leave, frame found");
	return frame;


	update_buffer_dropping_frame:
		/*Update GSM0710 buffer pointer and counter */
		pthread_mutex_lock(&datacount_lock); 
		buf->readp = local_readp;
		buf->datacount -= (local_datacount_backup - local_datacount); /* subtract whatever we analyzed */
		pthread_mutex_unlock(&datacount_lock); 
		return gsm0710_base_buffer_get_frame(buf);	/*continue extracting more frames if any*/
}

/* 
* Purpose:  Gets a advanced option frame from buffer. You have to remember to free this frame
*                when it's not needed anymore
* Input:      buf - the buffer, where the frame is extracted
* Return:    frame or null, if there isn't ready frame with given index
*/
static GSM0710_Frame *gsm0710_advanced_buffer_get_frame(
	GSM0710_Buffer * buf)
{
	LOGMUX(LOG_DEBUG, "Enter");
l_begin:
/* Okay, find start flag in buffer*/
	while (!buf->flag_found && gsm0710_buffer_length(buf) > 0)
	{
		if (*buf->readp == GSM0710_FRAME_ADV_FLAG)
		{
			buf->flag_found = 1;
			buf->adv_length = 0;
			buf->adv_found_esc = 0;
		}
		pthread_mutex_lock(&datacount_lock); /* need lock to operate on buf->datacount*/
		gsm0710_buffer_inc(buf->readp,buf->datacount); 
		pthread_mutex_unlock(&datacount_lock); 
	}
	if (!buf->flag_found)// no frame started
		return NULL;
	if (0 == buf->adv_length)
/* skip empty frames (this causes troubles if we're using DLC 62) */
		while (gsm0710_buffer_length(buf) > 0 && (*buf->readp == GSM0710_FRAME_ADV_FLAG))
		{
			pthread_mutex_lock(&datacount_lock); /* need to lock to operate on buf->datacount*/
			gsm0710_buffer_inc(buf->readp,buf->datacount);
			pthread_mutex_unlock(&datacount_lock); 
		}

/* Okay, we're ready to start analyzing the frame and filter out any escape char */
	while (gsm0710_buffer_length(buf) > 0)
	{
		if (!buf->adv_found_esc && GSM0710_FRAME_ADV_FLAG == *(buf->readp)) /* Whole frame parsed for escape chars, closing flag found */
		{
			GSM0710_Frame *frame = NULL;
			unsigned char *data = buf->adv_data;
			unsigned char fcs = 0xFF;
			pthread_mutex_lock(&datacount_lock); /* need to lock to operate on buf->datacount*/
			gsm0710_buffer_inc(buf->readp,buf->datacount);
			pthread_mutex_unlock(&datacount_lock);
			if (buf->adv_length < 3)
			{
				LOGMUX(LOG_WARNING, "Too short adv frame, length:%d", buf->adv_length);
				buf->flag_found = 0;
				goto l_begin; /* throw away current frame and start looking for new frame start flag */
			}
			/* Okay, extract the header information */
			if ((frame = (GSM0710_Frame*)malloc(sizeof(GSM0710_Frame))) != NULL) /* frame is sane, allocate memory for it */
			{
				frame->channel = ((data[0] & 252) >> 2); /* the channel address field */
				fcs = r_crctable[fcs ^ data[0]];
				frame->control = data[1]; /* the frame type field */
				fcs = r_crctable[fcs ^ data[1]];
				frame->length = buf->adv_length - 3; /* the frame length field (total - address field - type field - fcs field) */
			}
			else
				LOGMUX(LOG_ERR,"Out of memory, when allocating space for frame");
			/* Okay, extract the payload data */
			if (frame->length > 0)
			{
				if ((frame->data = (unsigned char *) malloc(sizeof(char) * frame->length)))
				{
					memcpy(frame->data, data + 2, frame->length); /*copy data from first payload field*/
					if (GSM0710_FRAME_IS(GSM0710_TYPE_UI, frame))
					{
						int i;
						for (i = 0; i < frame->length; ++i)
							fcs = r_crctable[fcs ^ (frame->data[i])];
					}
				}
				else
				{
					LOGMUX(LOG_ERR,"Out of memory, when allocating space for frame data");
					buf->flag_found = 0;
					goto l_begin;
				}
			}
			/* Okay, check FCS field */
			if (r_crctable[fcs ^ data[buf->adv_length - 1]] != 0xCF)
			{
				LOGMUX(LOG_WARNING, "Dropping frame: FCS doesn't match");
				destroy_frame(frame);
				buf->flag_found = 0;
				buf->dropped_count++;
				goto l_begin;
			}
			else
			{
				buf->received_count++;
				buf->flag_found = 0;
				LOGMUX(LOG_DEBUG, "Leave success");
				return frame;
			}
		}
		if (buf->adv_length >= sizeof(buf->adv_data)) /* frame data too much for buffer.. increase buffer size? */
		{
			LOGMUX(LOG_WARNING, "Too long adv frame, length:%d", buf->adv_length);
			buf->flag_found = 0;
			buf->dropped_count++;
			goto l_begin;
		}
		if (buf->adv_found_esc) /* Treat found escape char (throw it away and complement 6th bit in next field */
		{
			buf->adv_data[buf->adv_length] = *(buf->readp) ^ GSM0710_FRAME_ADV_ESC_COPML;
			buf->adv_length++;
			buf->adv_found_esc = 0;
		}
		else if (GSM0710_FRAME_ADV_ESC == *(buf->readp)) /* no untreated escape char. if current field is escape char, note it down */
			buf->adv_found_esc = 1;
		else /*field is regular payload char, store it*/
		{
			buf->adv_data[buf->adv_length] = *(buf->readp); 
			buf->adv_length++;
		}
		pthread_mutex_lock(&datacount_lock); /* need to lock to operate on buf->datacount*/
		gsm0710_buffer_inc(buf->readp,buf->datacount);
		pthread_mutex_unlock(&datacount_lock);
	}
	return NULL;
}

/* 
* Purpose:  Compares two strings.
*                strstr might not work because WebBox sends garbage before the first OK
*                when it's not needed anymore
* Input:      haystack - string to check
*                length - length of string to check
*                needle - reference string to compare to. must be null-terminated.
* Return:    1 if comparison was success, else 0
*/
static int memstr(
	const char *haystack,
	int length,
	const char *needle)
{
	int i;
	int j = 0;
	if (needle[0] == '\0')
		return 1;
	for (i = 0; i < length; i++)
		if (needle[j] == haystack[i])
		{
			j++;
			if (needle[j] == '\0') // Entire needle was found
				return 1;
		}
		else
			j = 0;
	return 0;
}

/* 
* Purpose:  Sends an AT-command to a given serial port and waits for reply.
* Input:      fd - file descriptor
*                cmd - command
*                to - how many seconds to wait for response
* Return:   0 on success (OK-response), -1 otherwise
*/
static int chat(
	int serial_device_fd,
	char *cmd,
	int to)
{
	LOGMUX(LOG_DEBUG, "Enter");
	unsigned char buf[1024];
	int sel;
	int len;
	int wrote = 0;
	syslogdump(">s ", (unsigned char *) cmd, strlen(cmd));
	SYSCHECK(wrote = write(serial_device_fd, cmd, strlen(cmd)));
	LOGMUX(LOG_DEBUG, "Wrote %d bytes", wrote);

#ifdef MUX_ANDROID
/* tcdrain not available on ANDROID */
ioctl(serial_device_fd, TCSBRK, 1); //equivalent to tcdrain(). perhaps permanent replacement?
#else
SYSCHECK(tcdrain(serial_device_fd));
#endif

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(serial_device_fd, &rfds);
	struct timeval timeout;
	timeout.tv_sec = to;
	timeout.tv_usec = 0;
	do
	{
		SYSCHECK(sel = select(serial_device_fd + 1, &rfds, NULL, NULL, &timeout));
		LOGMUX(LOG_DEBUG, "Selected %d", sel);
		if (FD_ISSET(serial_device_fd, &rfds))
		{
			memset(buf, 0, sizeof(buf));
			len = read(serial_device_fd, buf, sizeof(buf));
			SYSCHECK(len);
			LOGMUX(LOG_DEBUG, "Read %d bytes from serial device", len);
			syslogdump("<s ", buf, len);
			errno = 0;
			if (memstr((char *) buf, len, "OK"))
			{
				LOGMUX(LOG_DEBUG, "Received OK");
				return 0;
			}
			if (memstr((char *) buf, len, "ERROR"))
			{
				LOGMUX(LOG_DEBUG, "Received ERROR");
				return -1;
			}
		}
	} while (sel);
	return -1;
}

/* 
* Purpose:  Handles commands received from the control channel.
* Input:      frame - the mux frame struct
* Return:   0
*/
static int handle_command(
	GSM0710_Frame * frame)
{
	LOGMUX(LOG_DEBUG, "Enter");
	unsigned char type, signals;
	int length = 0, i, type_length, channel, supported = 1;
	unsigned char *response;
//struct ussp_operation op;
	if (frame->length > 0)
	{
		type = frame->data[0];// only a byte long types are handled now skip extra bytes
		for (i = 0; (frame->length > i && (frame->data[i] & GSM0710_EA) == 0); i++);
		i++;
		type_length = i;
		if ((type & GSM0710_CR) == GSM0710_CR)
		{
//command not ack extract frame length
			while (frame->length > i)
			{
				length = (length * 128) + ((frame->data[i] & 254) >> 1);
				if ((frame->data[i] & 1) == 1)
					break;
				i++;
			}
			i++;
			switch ((type & ~GSM0710_CR))
			{
			case GSM0710_CONTROL_CLD:
				LOGMUX(LOG_INFO, "The mobile station requested mux-mode termination");
				serial.state = MUX_STATE_CLOSING;
				break;
			case GSM0710_CONTROL_PSC:
				LOGMUX(LOG_DEBUG, "Power Service Control command: ***");
				LOGMUX(LOG_DEBUG, "Frame->data = %s / frame->length = %d", frame->data + i, frame->length - i);
			break;
			case GSM0710_CONTROL_TEST:
				LOGMUX(LOG_DEBUG, "Test command: ");
				LOGMUX(LOG_DEBUG, "Frame->data = %s / frame->length = %d", frame->data + i, frame->length - i);
				//serial->ping_number = 0;
				break;
			case GSM0710_CONTROL_MSC:
				if (i + 1 < frame->length)
				{
					channel = ((frame->data[i] & 252) >> 2);
					i++;
					signals = (frame->data[i]);
//op.op = USSP_MSC;
//op.arg = USSP_RTS;
//op.len = 0;
					LOGMUX(LOG_DEBUG, "Modem status command on channel %d", channel);
					if ((signals & GSM0710_SIGNAL_FC) == GSM0710_SIGNAL_FC)
						LOGMUX(LOG_DEBUG, "No frames allowed");
					else
					{
//op.arg |= USSP_CTS;
						LOGMUX(LOG_DEBUG, "Frames allowed");
					}
					if ((signals & GSM0710_SIGNAL_RTC) == GSM0710_SIGNAL_RTC)
					{
//op.arg |= USSP_DSR;
						LOGMUX(LOG_DEBUG, "Signal RTC");
					}
					if ((signals & GSM0710_SIGNAL_IC) == GSM0710_SIGNAL_IC)
					{
//op.arg |= USSP_RI;
						LOGMUX(LOG_DEBUG, "Signal Ring");
					}
					if ((signals & GSM0710_SIGNAL_DV) == GSM0710_SIGNAL_DV)
					{
//op.arg |= USSP_DCD;
						LOGMUX(LOG_DEBUG, "Signal DV");
					}
				}
				else
					LOGMUX(LOG_ERR, "Modem status command, but no info. i: %d, len: %d, data-len: %d",
						i, length, frame->length);
				break;
			default:
				LOGMUX(LOG_ERR,"Unknown command (%d) from the control channel", type);
				if ((response = malloc(sizeof(char) * (2 + type_length))) != NULL)
				{
					i = 0;
					response[i++] = GSM0710_CONTROL_NSC;
					type_length &= 127; //supposes that type length is less than 128
					response[i++] = GSM0710_EA | (type_length << 1);
					while (type_length--)
					{
						response[i] = frame->data[i - 2];
						i++;
					}
					write_frame(0, response, i, GSM0710_TYPE_UIH);
					free(response);
					supported = 0;
				}
				else
					LOGMUX(LOG_ERR,"Out of memory, when allocating space for response");
				break;
			}
			if (supported)
			{
//acknowledge the command
				frame->data[0] = frame->data[0] & ~GSM0710_CR;
				write_frame(0, frame->data, frame->length, GSM0710_TYPE_UIH);
				switch ((type & ~GSM0710_CR)){
				  case GSM0710_CONTROL_MSC:
				    if (frame->control & GSM0710_PF){ //Check if the P/F var needs to be set again (cleared in write_frame)
				      uih_pf_bit_received = 1;
				    }
				    LOGMUX(LOG_DEBUG, "Sending 1st MSC command App->Modem");
				    frame->data[0] = frame->data[0] | GSM0710_CR; //setting the C/R bit to "command"
				    write_frame(0, frame->data, frame->length, GSM0710_TYPE_UIH);
				    break;
				  default:
				    break;
				}
			}
		}
		else
		{
//received ack for a command
			if (GSM0710_COMMAND_IS(type, GSM0710_CONTROL_NSC))
				LOGMUX(LOG_ERR, "The mobile station didn't support the command sent");
			else
				LOGMUX(LOG_DEBUG, "Command acknowledged by the mobile station");
		}
	}
	return 0;
}

/* 
* Purpose:  Extracts and assembles frames from the mux GSM0710 buffer
* Input:      buf - the receiver buffer
* Return:   number of frames extracted
*/
int extract_frames(GSM0710_Buffer* buf)
{
	LOGMUX(LOG_DEBUG, "Enter");
	int frames_extracted = 0;
	GSM0710_Frame *frame;
	while ((frame = cmux_mode 
		? gsm0710_advanced_buffer_get_frame(buf)
		: gsm0710_base_buffer_get_frame(buf)))
	{
		frames_extracted++;
		/*Okay, go ahead and signal ser_read_thread to wake up if it is sleeping because reassembly buffer was full before */
		pthread_mutex_lock(&bufferready_lock);
		if (buf->input_sleeping == 1)
		{
			pthread_cond_signal(&newdataready_signal);
		}
		pthread_mutex_unlock(&bufferready_lock);

		if ((GSM0710_FRAME_IS(GSM0710_TYPE_UI, frame) || GSM0710_FRAME_IS(GSM0710_TYPE_UIH, frame)))
		{
			LOGMUX(LOG_DEBUG, "Frame is UI or UIH");
			if (frame->control & GSM0710_PF){
			  uih_pf_bit_received = 1;
			}

			if (frame->channel > 0)
			{
				LOGMUX(LOG_DEBUG, "Writing %d byte frame received on channel %d to %s",frame->length,frame->channel, channellist[frame->channel].ptsname);
//data from logical channel
				//syslogdump("Frame:", frame->data, frame->length);
				int write_result;
				if ((write_result = write(channellist[frame->channel].fd, frame->data, frame->length)) >= 0)
				{
				LOGMUX(LOG_DEBUG, "write() returned. Written %d/%d bytes of frame to %s",write_result,frame->length,channellist[frame->channel].ptsname);
				fsync(channellist[frame->channel].fd); /*push to /dev/pts device */
				}
				else{
					switch (errno){
					  case EINTR:
						LOGMUX(LOG_ERR,"Interrupt signal EINTR caught");
						break;
					  case EAGAIN:
						LOGMUX(LOG_ERR,"Interrupt signal EAGAIN caught");
						break;
					  case EBADF:
						LOGMUX(LOG_ERR,"Interrupt signal EBADF caught");
						break;
					  case EINVAL:
						LOGMUX(LOG_ERR,"Interrupt signal EINVAL caught");
						break;
					  case EFAULT:
						LOGMUX(LOG_ERR,"Interrupt signal EFAULT caught");
						break;
					  case EIO:
						LOGMUX(LOG_ERR,"Interrupt signal EIO caught");
						break;
					  case EFBIG:
						LOGMUX(LOG_ERR,"Interrupt signal EFBIG caught");
						break;
					  case ENOSPC:
						LOGMUX(LOG_ERR,"Interrupt signal ENOSPC caught");
						break;
					  case EPIPE:
						LOGMUX(LOG_ERR,"Interrupt signal EPIPE caught");
						break;
					  default:
						LOGMUX(LOG_ERR,"Unknown interrupt signal caught from write()\n");
					}		
				}
			}
			else
			{
//control channel command
				LOGMUX(LOG_DEBUG, "Frame channel == 0, control channel command");
				handle_command(frame);
			}
		}
		else
		{
//not an information frame
			LOGMUX(LOG_DEBUG, "Not an information frame");
			switch ((frame->control & ~GSM0710_PF))
			{
			case GSM0710_TYPE_UA:
				LOGMUX(LOG_DEBUG, "Frame is UA");
				if (channellist[frame->channel].opened)
				{
					SYSCHECK(logical_channel_close(channellist+frame->channel));
					LOGMUX(LOG_INFO, "Logical channel %d for %s closed",
						frame->channel, channellist[frame->channel].origin);
				}
				else
				{
					if(channellist[frame->channel].disc_ua_pending == 0){
						channellist[frame->channel].opened = 1;
						if (frame->channel == 0)
						{
							LOGMUX(LOG_DEBUG, "Control channel opened");
							//send version Siemens version test
							//static unsigned char version_test[] = "\x23\x21\x04TEMUXVERSION2\0";
							//write_frame(0, version_test, sizeof(version_test), GSM0710_TYPE_UIH);
						}
						else
							LOGMUX(LOG_INFO, "Logical channel %d opened", frame->channel);
					}
					else {
						LOGMUX(LOG_INFO, "UA to acknowledgde DISC on channel %d received", frame->channel);
						channellist[frame->channel].disc_ua_pending = 0; 
					}
				}
				break;
			case GSM0710_TYPE_DM:
				if (channellist[frame->channel].opened)
				{
					SYSCHECK(logical_channel_close(channellist+frame->channel));
					LOGMUX(LOG_INFO, "DM received, so the channel %d for %s was already closed",
						frame->channel, channellist[frame->channel].origin);
				}
				else
				{
					if (frame->channel == 0)
					{
						LOGMUX(LOG_INFO, "Couldn't open control channel.\n->Terminating");
						serial.state = MUX_STATE_CLOSING;
//close channels
					}
					else
						LOGMUX(LOG_INFO, "Logical channel %d for %s couldn't be opened", frame->channel, channellist[frame->channel].origin);
				}
				break;
			case GSM0710_TYPE_DISC:
				if (channellist[frame->channel].opened)
				{
					channellist[frame->channel].opened = 0;
					write_frame(frame->channel, NULL, 0, GSM0710_TYPE_UA | GSM0710_PF);
					if (frame->channel == 0)
					{
						serial.state = MUX_STATE_CLOSING;
						LOGMUX(LOG_INFO, "Control channel closed");
					}
					else
						LOGMUX(LOG_INFO, "Logical channel %d for %s closed", frame->channel, channellist[frame->channel].origin);
				}
				else
				{
//channel already closed
					LOGMUX(LOG_WARNING, "Received DISC even though channel %d for %s was already closed",
							frame->channel, channellist[frame->channel].origin);
					write_frame(frame->channel, NULL, 0, GSM0710_TYPE_DM | GSM0710_PF);
				}
				break;
			case GSM0710_TYPE_SABM:
//channel open request
				if (channellist[frame->channel].opened)
				{
					if (frame->channel == 0)
						LOGMUX(LOG_INFO, "Control channel opened");
					else
						LOGMUX(LOG_INFO, "Logical channel %d for %s opened",
							frame->channel, channellist[frame->channel].origin);
				}
				else
//channel already opened
					LOGMUX(LOG_WARNING, "Received SABM even though channel %d for %s was already closed",
						frame->channel, channellist[frame->channel].origin);
				channellist[frame->channel].opened = 1;
				write_frame(frame->channel, NULL, 0, GSM0710_TYPE_UA | GSM0710_PF);
				break;
			}
		}
		destroy_frame(frame);
	}
	LOGMUX(LOG_DEBUG, "Leave");
	return frames_extracted;
}

/* 
* Purpose:  Thread function. Will constantly check GSM0710_Buffer for mux frames, and, if any, 
*                assemble them in Frame struct and send them to the appropriate pseudo terminal
* Input:      vargp - void pointer to the receiver buffer
* Return:    NULL - when buffer is destroyed
*/
void* assemble_frame_thread(void * vargp)
{
	GSM0710_Buffer* buf = (GSM0710_Buffer*) vargp;

	while(buf!=NULL)
	{
		pthread_mutex_lock(&newdataready_lock);
		while (!(buf->datacount > 0) || !buf->newdataready) /* true if no data available in buffer or no new data written since last extract_frames() call */
		{
			LOGMUX(LOG_DEBUG,"assemble_frame_thread put to sleep. GSM0710 buffer stored %d, newdataready: %d", buf->datacount, buf->newdataready);
			pthread_cond_wait(&newdataready_signal,&newdataready_lock); /* sleep until signalled by thread_serial_device_read() */
			LOGMUX(LOG_DEBUG,"assemble_frame_thread awoken. GSM0710 buffer stored %d, newdataready: %d", buf->datacount, buf->newdataready);
		}
		buf->newdataready = 0; /*reset newdataready since buffer will be processed in extract_frames()*/
		pthread_mutex_unlock(&newdataready_lock);

		extract_frames(buf);
	}

	LOGMUX(LOG_ERR, "assemble_frame_thread terminated");
	return NULL;
}

/* 
* Purpose:  Function responsible by all signal handlers treatment any new signal must be added here
* Input:      param - signal ID
* Return:    -
*/
void signal_treatment(
	int param)
{
	switch (param)
	{
	case SIGPIPE:
		exit(0);
	break;
	case SIGHUP:
//reread the configuration files
	break;
	case SIGINT:
	case SIGTERM:
	case SIGUSR1:
		//exit(0);
//sig_term(param);

             set_main_exit_signal(1);
	break;
	case SIGKILL:
	default:
		exit(0);
	break;
	}
}

/* 
* Purpose:  Poll a device without select(). read() will do the blocking if VMIN=1.
*                call a reading function for the particular device
* Input:      vargp - a pointer to a Poll_Thread_Arg struct.
* Return:    NULL if error
*/
void* poll_thread_serial(void *vargp) {
  LOGMUX(LOG_DEBUG,"Enter");
  Poll_Thread_Arg* poll_thread_arg = (Poll_Thread_Arg*)vargp;
  if(poll_thread_arg->fd== -1 ){
    LOGMUX(LOG_ERR, "Serial port not initialized");
    goto terminate;
  }
  while (1)
  {
    if((*(poll_thread_arg->read_function_ptr))(poll_thread_arg->read_function_arg)!=0){ /*Call reading function*/
      LOGMUX(LOG_WARNING, "Device read function returned error");
      goto terminate;
    }
  }
  goto terminate;

  terminate:
    LOGMUX(LOG_ERR, "Device polling thread terminated");
    free(poll_thread_arg);  //free the memory allocated for the thread args before exiting thread
    return NULL;
}

/* 
* Purpose:  Thread function. Reads whatever data is in the line discipline (coming from modem)
* Input:      vargp - void pointer the serial struct
* Return:    0 if data successfully read, else read error
*/
int thread_serial_device_read(void * vargp)
{
	Serial * serial = (Serial*) vargp;
	LOGMUX(LOG_DEBUG, "Enter");
	{
		switch (serial->state)
		{
			case MUX_STATE_MUXING:
			{
				unsigned char buf[4096];
				int len;
				//input from serial port
				LOGMUX(LOG_DEBUG, "Serial Data");
				int length;
				if ((length = gsm0710_buffer_free(serial->in_buf)) > 0) /*available space in buffer (not locked since we want to utilize all available space)*/
				{
					if((len = read(serial->fd, buf, min(length, sizeof(buf)))) > 0)
					{
						syslogdump("<s ", buf, len);
						gsm0710_buffer_write(serial->in_buf, buf, len);
					}
					else if ((length > 0) && (len == 0))
					{
						LOGMUX(LOG_DEBUG,"Waiting for data from serial device");
					}

					else
					{
						switch (errno){
						  case EINTR:
							LOGMUX(LOG_ERR,"Interrupt signal EINTR caught");
							break;
						  case EAGAIN:
							LOGMUX(LOG_ERR,"Interrupt signal EAGAIN caught");
							break;
						  case EBADF:
							LOGMUX(LOG_ERR,"Interrupt signal EBADF caught");
							break;
						  case EINVAL:
							LOGMUX(LOG_ERR,"Interrupt signal EINVAL caught");
							break;
						  case EFAULT:
							LOGMUX(LOG_ERR,"Interrupt signal EFAULT caught");
							break;
						  case EIO:
							LOGMUX(LOG_ERR,"Interrupt signal EIO caught");
							break;
						  case EISDIR:
							LOGMUX(LOG_ERR,"Interrupt signal EISDIR caught");
							break;
						  default:
							LOGMUX(LOG_ERR,"Unknown interrupt signal caught\n");
							}
					}			
				}
				else
				{
					/* Okay, internal buffer is full. we need to wait for the assembly thread to deliver a frame to the app(s). and free-up space */
					pthread_mutex_lock(&bufferready_lock);
					LOGMUX(LOG_WARNING,"Internal re-assembly buffer is full, waiting for flush to appl!");
					serial->in_buf->input_sleeping = 1; /* set sleeping flag - must be inside lock*/
					while (!(gsm0710_buffer_free(serial->in_buf)) > 0)
					{
						LOGMUX(LOG_DEBUG,"ser_read_thread put to sleep. GSM0710 buffer has %d bytes free", gsm0710_buffer_free(serial->in_buf));
						pthread_cond_wait(&bufferready_signal,&bufferready_lock); /* sleep until signalled by assembly thread() */
						LOGMUX(LOG_DEBUG,"ser_read_thread awoken");
					}
					serial->in_buf->input_sleeping = 0; /* unset sleeping flag - must be inside lock*/
					LOGMUX(LOG_WARNING,"Internal re-assembly buffer partly flushed, free space: %d",gsm0710_buffer_free(serial->in_buf));		
					pthread_mutex_unlock(&bufferready_lock);
					
				}
				LOGMUX(LOG_DEBUG, "Leave, keep watching");
				return 0;
			break;
			}
			default:
				LOGMUX(LOG_WARNING, "Don't know how to handle reading in state %d", serial->state);
			break;
			}
	}
	LOGMUX(LOG_DEBUG, "Leave, stop watching");
	return 1;
}

/* 
* Purpose:  Open and initialize the serial device used.
* Input:      serial - the serial struct
* Return:    0 if port successfully opened, else 1.
*/
int open_serial_device(
	Serial* serial
	)
{
	LOGMUX(LOG_DEBUG, "Enter");
	unsigned int i;
	for (i=0;i<GSM0710_MAX_CHANNELS;i++)
		SYSCHECK(logical_channel_init(channellist+i, i));
/* open the serial port */
	SYSCHECK(serial->fd = open(serial->devicename, O_RDWR | O_NOCTTY | O_NONBLOCK));

	LOGMUX(LOG_INFO, "Opened serial port");
	int fdflags;
	SYSCHECK(fdflags = fcntl(serial->fd, F_GETFL));
	SYSCHECK(fcntl(serial->fd, F_SETFL, fdflags & ~O_NONBLOCK));
	struct termios t;
	tcgetattr(serial->fd, &t);
	t.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD);	
	t.c_cflag |= CREAD | CLOCAL | CS8 ;
	t.c_cflag &= ~(CRTSCTS);
	t.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
	t.c_iflag &= ~(INPCK | IGNPAR | PARMRK | ISTRIP | IXANY | ICRNL);
	t.c_iflag &= ~(IXON | IXOFF);
	t.c_oflag &= ~(OPOST | OCRNL);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;

	#ifdef MUX_ANDROID
	//Android does not directly define _POSIX_VDISABLE. It can be fetched using pathconf()
	long posix_vdisable;
	char cur_path[FILENAME_MAX];
       if (!getcwd(cur_path, sizeof(cur_path))){
         LOGMUX(LOG_ERR,"_getcwd returned errno %d",errno);
         return 1;
       }
    posix_vdisable = pathconf(cur_path, _PC_VDISABLE);
	t.c_cc[VINTR] = posix_vdisable;
	t.c_cc[VQUIT] = posix_vdisable;
	t.c_cc[VSTART] = posix_vdisable;
	t.c_cc[VSTOP] = posix_vdisable;
	t.c_cc[VSUSP] = posix_vdisable;
	#else
	t.c_cc[VINTR] = _POSIX_VDISABLE;
	t.c_cc[VQUIT] = _POSIX_VDISABLE;
	t.c_cc[VSTART] = _POSIX_VDISABLE;
	t.c_cc[VSTOP] = _POSIX_VDISABLE;
	t.c_cc[VSUSP] = _POSIX_VDISABLE;
	#endif

	speed_t speed = baud_bits[cmux_port_speed];
	cfsetispeed(&t, speed);
	cfsetospeed(&t, speed);
	SYSCHECK(tcsetattr(serial->fd, TCSANOW, &t));
	int status = TIOCM_DTR | TIOCM_RTS;
	ioctl(serial->fd, TIOCMBIS, &status);
	LOGMUX(LOG_INFO, "Configured serial device");
	serial->ping_number = 0;
	time(&serial->frame_receive_time); //get the current time
	serial->state = MUX_STATE_INITILIZING;
	LOGMUX(LOG_DEBUG, "Switched Mux state to %d ",serial->state);
	return 0;
}

/* 
* Purpose:  Initialize mux connection with modem.
* Input:      serial - the serial struct
* Return:    0
*/
int start_muxer(
	Serial* serial
	)
{
	LOGMUX(LOG_INFO, "Configuring modem");
	char gsm_command[100];
	//check if communication with modem is online
	if (chat(serial->fd, "AT\r\n", 1) < 0)
	{
		LOGMUX(LOG_WARNING, "Modem does not respond to AT commands, trying close mux mode");
		//if (cmux_mode) we do not know now so write both
			write_frame(0, NULL, 0, GSM0710_CONTROL_CLD | GSM0710_CR);
		//else
			write_frame(0, close_channel_cmd, 2, GSM0710_TYPE_UIH);
		SYSCHECK(chat(serial->fd, "AT\r\n", 1));
	}
	SYSCHECK(chat(serial->fd, "ATZ\r\n", 3));
	SYSCHECK(chat(serial->fd, "ATE0\r\n", 1));
	if (0)// additional siemens c35 init
	{
		SYSCHECK(snprintf(gsm_command, sizeof(gsm_command), "AT+IPR=%d\r\n", baud_rates[cmux_port_speed]));
		SYSCHECK(chat(serial->fd, gsm_command, 1));
		SYSCHECK(chat(serial->fd, "AT\r\n", 1));
		SYSCHECK(chat(serial->fd, "AT&S0\r\n", 1));
		SYSCHECK(chat(serial->fd, "AT\\Q3\r\n", 1));
	}
	if (pin_code >= 0)
	{
		LOGMUX(LOG_DEBUG, "send pin %04d", pin_code);
//Some modems, such as webbox, will sometimes hang if SIM code is given in virtual channel
		SYSCHECK(snprintf(gsm_command, sizeof(gsm_command), "AT+CPIN=%04d\r\n", pin_code));
		SYSCHECK(chat(serial->fd, gsm_command, 10));
	}

if (cmux_mode){
	SYSCHECK(snprintf(gsm_command, sizeof(gsm_command), "AT+CMUX=1\r\n"));
}
else {
		SYSCHECK(snprintf(gsm_command, sizeof(gsm_command), "AT+CMUX=%d,%d,%d,%d\r\n"
			, cmux_mode
			, cmux_subset
			, cmux_port_speed
			, cmux_N1
			));
}

	LOGMUX(LOG_INFO, "Starting mux mode");
	SYSCHECK(chat(serial->fd, gsm_command, 3));
	serial->state = MUX_STATE_MUXING;
	LOGMUX(LOG_DEBUG, "Switched Mux state to %d ",serial->state);
	LOGMUX(LOG_INFO, "Waiting for mux-mode");
	sleep(1);
	LOGMUX(LOG_INFO, "Init control channel");
	return 0;
}

/* 
* Purpose:  Close all devices, send mux termination frames
* Input:      -
* Return:    0
*/
static int close_devices()
{
	LOGMUX(LOG_DEBUG, "Enter");
	int i;
	for (i=1;i<GSM0710_MAX_CHANNELS;i++)
	{
//terminate command given. Close channels one by one and finaly close
//the mux mode
		if (channellist[i].fd >= 0)
		{
			if (channellist[i].opened)
			{
				LOGMUX(LOG_INFO, "Closing down the logical channel %d", i);
				if (cmux_mode)
					write_frame(i, NULL, 0, GSM0710_CONTROL_CLD | GSM0710_CR);
				else
					write_frame(i, close_channel_cmd, 2, GSM0710_TYPE_UIH);
				SYSCHECK(logical_channel_close(channellist+i));
			}
			LOGMUX(LOG_INFO, "Logical channel %d closed", channellist[i].id);
		}
	}
	if (serial.fd >= 0)
	{
		if (cmux_mode)
			write_frame(0, NULL, 0, GSM0710_CONTROL_CLD | GSM0710_CR);
		else
			write_frame(0, close_channel_cmd, 2, GSM0710_TYPE_UIH);
		static const char* poff = "AT@POFF\r\n";
		syslogdump(">s ", (unsigned char *)poff, strlen(poff));
		write(serial.fd, poff, strlen(poff));
		SYSCHECK(close(serial.fd));
		serial.fd = -1;
	}
	serial.state = MUX_STATE_OFF;
	return 0;
}

/* 
* Purpose:  The watchdog state machine restarted every x seconds
* Input:      serial - the serial struct
* Return:    1 if error, 0 if success
*/
static int watchdog(Serial * serial)
{
	LOGMUX(LOG_DEBUG, "Enter");
	int i;

	LOGMUX(LOG_DEBUG, "Serial state is %d", serial->state);
	switch (serial->state)
	{
	case MUX_STATE_OPENING:
		if (open_serial_device(serial) != 0){
			LOGMUX(LOG_WARNING, "Could not open serial device and start muxer");
                     return 1;
              }
		LOGMUX(LOG_INFO, "Watchdog started");
	case MUX_STATE_INITILIZING:
              if (start_muxer(serial) < 0)
              LOGMUX(LOG_WARNING, "Could not open all devices and start muxer errno=%d", errno);

			  /* Create thread for assemble of frames from data in GSM0710 mux buffer */
			  if(create_thread(&frame_assembly_thread, assemble_frame_thread,(void*) serial->in_buf)!=0){ 
				LOGMUX(LOG_ERR,"Could not create thread for frame-assmbly");
				return 1;
			  }

			  /* Create thread for polling on serial device (mux input) and writing to GSM0710 mux buffer */
              Poll_Thread_Arg* poll_thread_arg = (Poll_Thread_Arg*) malloc(sizeof(Poll_Thread_Arg)); //iniitialize pointer to thread args ;
              poll_thread_arg->fd = serial->fd;
              poll_thread_arg->read_function_ptr = &thread_serial_device_read;
              poll_thread_arg->read_function_arg = (void *) serial;
		if(create_thread(&ser_read_thread, poll_thread_serial,(void*) poll_thread_arg)!=0){ //create thread for reading input from serial device
		  LOGMUX(LOG_ERR,"Could not create thread for listening on %s", serial->devicename);
		  return 1;
		}
	       LOGMUX(LOG_DEBUG, "Thread is running and listening on %s", serial->devicename); //listening on serial port
	       write_frame(0, NULL, 0, GSM0710_TYPE_SABM | GSM0710_PF); //need to move? messy

		//tempary solution. call to allocate virtual port(s)
        if (vir_ports<GSM0710_MAX_CHANNELS){         
       		for (i=1;i<=vir_ports;i++){
       		  LOGMUX(LOG_INFO, "Allocating logical channel %d/%d ",i,vir_ports);
       		  if((c_alloc_channel("watchdog_init", &pseudo_terminal[i])) != 0)
                set_main_exit_signal(1); //exit main function if channel couldn't be allocated
                sleep(1);
       		}
       	}
       	else{
                LOGMUX(LOG_ERR,"Cannot allocate %d virtual ports", vir_ports);
                return 1;
       	}
	    LOGMUX(LOG_INFO, "Multiplexing started..");

	break;
	case MUX_STATE_MUXING:
		/* Re-establish previously closed logical channel and pseudo terminal */
		if (pts_reopen==1){
			for(i=1;i<=vir_ports;i++){
				if (channellist[i].reopen == 1){
				  if(restart_pty_interface(&channellist[i]) != 0)
				  	{
  	                set_main_exit_signal(1); //exit main function if channel couldn't be allocated
				  	}
  	              else {
				    channellist[i].reopen = 0;
					pthread_mutex_lock(&pts_reopen_lock);
     			    pts_reopen=0;
					pthread_mutex_unlock(&pts_reopen_lock);

  	              	}
				}
				}
			}	
		if (use_ping)
		{
			if (serial->ping_number > use_ping)
			{
				LOGMUX(LOG_DEBUG, "no ping reply for %d times, resetting modem", serial->ping_number);
				serial->state = MUX_STATE_CLOSING;
				LOGMUX(LOG_DEBUG, "Switched Mux state to %d ",serial->state);
			}
			else
			{
				LOGMUX(LOG_DEBUG, "Sending PING to the modem");
				//write_frame(0, psc_channel_cmd, sizeof(psc_channel_cmd), GSM0710_TYPE_UI);
				write_frame(0, test_channel_cmd, sizeof(test_channel_cmd), GSM0710_TYPE_UI);
				serial->ping_number++;
			}
		}
		if (use_timeout)
		{
			time_t current_time;
			time(&current_time); //get the current time
			if (current_time - serial->frame_receive_time > use_timeout)
			{
				LOGMUX(LOG_DEBUG, "timeout, resetting modem");
				serial->state = MUX_STATE_CLOSING;
				LOGMUX(LOG_DEBUG, "Switched Mux state to %d ",serial->state);
			}

		}

	break;
	case MUX_STATE_CLOSING:
		close_devices();
		serial->state = MUX_STATE_OPENING;
		LOGMUX(LOG_DEBUG, "Switched Mux state to %d ",serial->state);
	break;
	default:
		LOGMUX(LOG_WARNING, "Don't know how to handle state %d", serial->state);
	break;
	}
	return 0;
}

/* 
* Purpose:  shows how to use this program
* Input:      name - string containing name of program
* Return:    -1
*/
static int usage(
	char *_name)
{
	fprintf(stderr, "\tUsage: %s [options]\n", _name);
	fprintf(stderr, "Options:\n");
	// process control
	fprintf(stderr, "\t-d: Fork, get a daemon [%s]\n", no_daemon?"no":"yes");
	fprintf(stderr, "\t-v: Set verbose logging level. 0 (Silent) - 7 (Debug) [%d]\n",syslog_level);
	// modem control
	fprintf(stderr, "\t-s <serial port name>: Serial port device to connect to [%s]\n", serial.devicename);
	fprintf(stderr, "\t-t <timeout>: reset modem after this number of seconds of silence [%d]\n", use_timeout);
	fprintf(stderr, "\t-P <pin-code>: PIN code to unlock SIM [%d]\n", pin_code);
	fprintf(stderr, "\t-p <number>: use ping and reset modem after this number of unanswered pings [%d]\n", use_ping);
	// legacy - will be removed
	fprintf(stderr, "\t-b <baudrate>: mode baudrate [%d]\n", baud_rates[cmux_port_speed]);
	fprintf(stderr, "\t-m <modem>: Mode (basic, advanced) [%s]\n", cmux_mode?"advanced":"basic");
	fprintf(stderr, "\t-f <framsize>: Frame size [%d]\n", cmux_N1);
	fprintf(stderr, "\t-n <number of ports>: Number of virtual ports to create, must be in range 1-31 [%d]\n", vir_ports);
	fprintf(stderr, "\t-o <output log to file>: Output log to /tmp/gsm0710muxd.log [%s]\n", logtofile?"yes":"no");
	//
	fprintf(stderr, "\t-h: Show this help message and show current settings.\n");
	return -1;
}

/* 
* Purpose:  function to set global flag main_exit_signal
* Input:      signal - ID number of signal
* Return:    -
*/
void set_main_exit_signal(int signal){
  //new lock
  pthread_mutex_lock(&main_exit_signal_lock);
  main_exit_signal = signal;
  pthread_mutex_unlock(&main_exit_signal_lock);
}

/* 
* Purpose:  Creates a detached thread. also checks for errors on exit.
* Input:      thread_id - pointer to pthread_t id
*                thread_function - void pointer to thread function
*                thread_function_arg - void pointer to thread function args
* Return:    0 if success, 1 if fail
*/
int create_thread(pthread_t * thread_id, void * thread_function, void * thread_function_arg ){
LOGMUX(LOG_DEBUG,"Enter");
              pthread_attr_init(&thread_attr);
              pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);

		if(pthread_create(thread_id, &thread_attr, thread_function, thread_function_arg)!=0){
                switch (errno){
                  case EAGAIN:
                    LOGMUX(LOG_ERR,"Interrupt signal EAGAIN caught");
                    break;
                  case EINVAL:
                    LOGMUX(LOG_ERR,"Interrupt signal EINVAL caught");
                    break;
                  default:
                    LOGMUX(LOG_ERR,"Unknown interrupt signal caught");
                }
                LOGMUX(LOG_ERR,"Could not create thread");
                set_main_exit_signal(1); //exit main function if thread couldn't be created
                return 1;
              }
              pthread_attr_destroy(&thread_attr); /* Not strictly necessary */
return 0; //thread created successfully
}

/* 
* Purpose:  Poll a device (file descriptor) using select()
*                if select returns data to be read. call a reading function for the particular device
* Input:      vargp - a pointer to a Poll_Thread_Arg struct.
* Return:    NULL if error
*/
void* poll_thread(void *vargp) {
  LOGMUX(LOG_DEBUG,"Enter");
  Poll_Thread_Arg* poll_thread_arg = (Poll_Thread_Arg*)vargp;
  if(poll_thread_arg->fd== -1 ){
    LOGMUX(LOG_ERR, "Serial port not initialized");
    goto terminate;
  }
  while (1)
  {
    fd_set fdr,fdw;
    FD_ZERO(&fdr);
    FD_ZERO(&fdw);
    FD_SET(poll_thread_arg->fd,&fdr);
    if (select((1+poll_thread_arg->fd),&fdr,&fdw,NULL,NULL)>0) {
      if (FD_ISSET(poll_thread_arg->fd,&fdr)) {
        if((*(poll_thread_arg->read_function_ptr))(poll_thread_arg->read_function_arg)!=0){ /*Call reading function*/
          LOGMUX(LOG_WARNING, "Device read function returned error");
          goto terminate;
        }
      }
    }
    else{ //No need to evaluate retval=0 case, since no use of timeout in select()
      switch (errno){
        case EINTR:
          LOGMUX(LOG_ERR,"Interrupt signal EINTR caught");
          break;
        case EAGAIN:
          LOGMUX(LOG_ERR,"Interrupt signal EAGAIN caught");
          break;
        case EBADF:
          LOGMUX(LOG_ERR,"Interrupt signal EBADF caught");
          break;
        case EINVAL:
          LOGMUX(LOG_ERR,"Interrupt signal EINVAL caught");
          break;
        default:
          LOGMUX(LOG_ERR,"Unknown interrupt signal caught\n");
      }
      goto terminate;
    }
  }
  goto terminate;

  terminate:
    LOGMUX(LOG_ERR, "Device polling thread terminated");
    free(poll_thread_arg);  //free the memory allocated for the thread args before exiting thread
    return NULL;
}

/* 
* Purpose:  The main program loop
* Input:      argc - number of input arguments
*                argv - array of strings (input arguments)
* Return:    0
*/
int main(int argc,char *argv[])
{
	LOGMUX(LOG_DEBUG, "Enter");
	int opt;
//for fault tolerance
	serial.devicename = "/dev/ttyS0";
	while ((opt = getopt(argc, argv, "dov:s:t:p:f:n:h?m:b:P:")) > 0)
	{
		switch (opt)
		{
		case 'v':
			syslog_level = atoi(optarg);
			if ((syslog_level>LOG_DEBUG) || (syslog_level < 0)){
              usage(argv[0]);
			  exit(0);
			#ifdef MUX_ANDROID
			//syslog_level=android_log_lvl_convert[syslog_level];
			#endif
			}
			break;
		case 'o':
			logtofile = 1;
			if ((muxlogfile=fopen("/tmp/gsm0710muxd.log", "w+")) == NULL){
				 fprintf(stderr, "Error: %s.\n", strerror(errno));
				 usage(argv[0]);
				 exit(0);
				 }
			else
				fprintf(stderr, "gsm0710muxd log is output to /tmp/gsm0710muxd.log\n");
			break;			
		case 'd':
			no_daemon = !no_daemon;
			break;
		case 's':
			serial.devicename = optarg;
			break;
		case 't':
			use_timeout = atoi(optarg);
			break;
		case 'p':
			use_ping = atoi(optarg);
			break;
		case 'P':
			pin_code = atoi(optarg);
			break;
		// will be removed if +CMUX? works
		case 'f':
			cmux_N1 = atoi(optarg);
			break;
		case 'n':
			vir_ports = atoi(optarg);
			if ((vir_ports>GSM0710_MAX_CHANNELS-1) || (vir_ports < 1)){
			  usage(argv[0]);
			  exit(0);
			}
			break;
		case 'm':
			if (!strcmp(optarg, "basic"))
				cmux_mode = 0;
			else if (!strcmp(optarg, "advanced"))
				cmux_mode = 1;
			else
				cmux_mode = 0;
			break;
		case 'b':
			cmux_port_speed = baud_rate_index(atoi(optarg));
			break;
		default:
		case '?':
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		}
	}

	umask(0);
//signals treatment
	signal(SIGHUP, signal_treatment);
	signal(SIGPIPE, signal_treatment);
	signal(SIGKILL, signal_treatment);
	signal(SIGINT, signal_treatment);
	signal(SIGUSR1, signal_treatment);
	signal(SIGTERM, signal_treatment);

#ifndef MUX_ANDROID
	if (no_daemon)
		openlog(argv[0], LOG_NDELAY | LOG_PID | LOG_PERROR, LOG_LOCAL0);
	else
		openlog(argv[0], LOG_NDELAY | LOG_PID, LOG_LOCAL0);
#endif
//allocate memory for data structures
	if ((serial.in_buf = gsm0710_buffer_init()) == NULL
	 || (serial.adv_frame_buf = (unsigned char*)malloc((cmux_N1 + 3) * 2 + 2)) == NULL)
	{
		LOGMUX(LOG_ERR,"Out of memory");
		exit(-1);
	}
	LOGMUX(LOG_DEBUG, "%s %s starting", *argv, revision);
//Initialize modem and virtual ports
	serial.state = MUX_STATE_OPENING;

	LOGMUX(LOG_INFO,"Called with following options:");
	LOGMUX(LOG_INFO,"\t-d: Fork, get a daemon [%s]", no_daemon?"no":"yes");
	LOGMUX(LOG_INFO,"\t-v: Set verbose logging level. 0 (Silent) - 7 (Debug) [%d]",syslog_level);
	LOGMUX(LOG_INFO,"\t-s <serial port name>: Serial port device to connect to [%s]", serial.devicename);
	LOGMUX(LOG_INFO,"\t-t <timeout>: reset modem after this number of seconds of silence [%d]", use_timeout);
	LOGMUX(LOG_INFO,"\t-P <pin-code>: PIN code to unlock SIM [%d]", pin_code);
	LOGMUX(LOG_INFO,"\t-p <number>: use ping and reset modem after this number of unanswered pings [%d]", use_ping);
	LOGMUX(LOG_INFO,"\t-b <baudrate>: mode baudrate [%d]", baud_rates[cmux_port_speed]);
	LOGMUX(LOG_INFO,"\t-m <modem>: Mode (basic, advanced) [%s]", cmux_mode?"advanced":"basic");
	LOGMUX(LOG_INFO,"\t-f <framsize>: Frame size [%d]", cmux_N1);
	LOGMUX(LOG_INFO,"\t-n <number of ports>: Number of virtual ports to create, must be in range 1-31 [%d]", vir_ports);
	LOGMUX(LOG_INFO,"\t-o <output log to file>: Output log to /tmp/gsm0710muxd.log [%s]", logtofile?"yes":"no");

while((main_exit_signal==0) && (watchdog(&serial)==0)){
	LOGMUX(LOG_INFO, "GSM0710 buffer. Stored %d", gsm0710_buffer_length(serial.in_buf));
	LOGMUX(LOG_INFO, "Frames received/dropped: %d/%d",serial.in_buf->received_count,serial.in_buf->dropped_count);
	sleep(5);
}


//finalize everything
	SYSCHECK(close_devices());
	free(serial.adv_frame_buf);
	gsm0710_buffer_destroy(serial.in_buf);
	LOGMUX(LOG_INFO, "Received %ld frames and dropped %ld received frames during the mux-mode",
		serial.in_buf->received_count, serial.in_buf->dropped_count);
	LOGMUX(LOG_DEBUG, "%s finished", argv[0]);

#ifndef MUX_ANDROID	
	closelog();// close syslog
#endif

if (logtofile)
  fclose(muxlogfile);	

return 0;
}


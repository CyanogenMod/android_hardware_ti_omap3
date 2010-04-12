/*
 *  TI FM kernel driver's test application.
 *
 *  Copyright (C) 2009 Texas Instruments
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _KFMAPP_H
#define _KFMAPP_H

#define DEFAULT_RADIO_DEVICE    "/dev/radio0"
#define DEFAULT_FM_ALSA_CARD    "hw:CARD=1"

#define CTL_INDEX_0                0
#define CTL_INDEX_1                1

#define FMAPP_BATCH		   0
#define FMAPP_INTERACTIVE          1

#define FM_MUTE_ON                 0
#define FM_MUTE_OFF                1
#define FM_MUTE_ATTENUATE          2

#define FM_SEARCH_DIRECTION_DOWN   0
#define FM_SEARCH_DIRECTION_UP     1

#define FM_MODE_SWITCH_CTL_NAME   "Mode Switch"
#define FM_MODE_OFF		   0
#define FM_MODE_TX		   1
#define FM_MODE_RX	           2

#define FM_BAND_SWITCH_CTL_NAME    "Region Switch"
#define FM_BAND_EUROPE_US          0
#define FM_BAND_JAPAN              1

#define FM_RF_DEPENDENT_MUTE_CTL_NAME     "RF Dependent Mute"
#define FM_RX_RF_DEPENDENT_MUTE_ON        1
#define FM_RX_RF_DEPENDENT_MUTE_OFF       0

#define FM_RX_GET_RSSI_LVL_CTL_NAME 	  "RSSI Level"
#define FM_RX_RSSI_THRESHOLD_LVL_CTL_NAME "RSSI Threshold"

#define FM_STEREO_MONO_CTL_NAME	          "Stereo/Mono"
#define FM_STEREO_MODE                    0
#define FM_MONO_MODE                      1

#define FM_RX_DEEMPHASIS_CTL_NAME    	  "De-emphasis Filter"
#define FM_RX_EMPHASIS_FILTER_50_USEC     0
#define FM_RX_EMPHASIS_FILTER_75_USEC     1

#define FM_RDS_SWITCH_CTL_NAME    	  "RDS Switch"
#define FM_RDS_DISABLE                    0
#define FM_RDS_ENABLE                     1

#define FM_RX_RDS_OPMODE_CTL_NAME	  "RDS Operation Mode"
#define FM_RDS_SYSTEM_RDS                  0
#define FM_RDS_SYSTEM_RBDS                 1

#define FM_RX_AF_SWITCH_CTL_NAME	  "AF Switch"
#define FM_RX_RDS_AF_SWITCH_MODE_ON	   1
#define FM_RX_RDS_AF_SWITCH_MODE_OFF	   0

#ifndef ANDROID
#define VIDIOC_S_HW_FREQ_SEEK    _IOW('V', 82, struct v4l2_hw_freq_seek)

struct v4l2_hw_freq_seek {
		unsigned int                 tuner;
		enum v4l2_tuner_type  type;
		unsigned int                seek_upward;
		unsigned int                wrap_around;
		unsigned int                 reserved[8];

 };
#endif
/* Auto scan info */
#define  FMAPP_ASCAN_SIGNAL_THRESHOLD_PER  50 /* 50 % */
#define  FMAPP_ASCAN_NO_OF_SIGNAL_SAMPLE   3  /* 3 Samples */

struct tx_rds {
        unsigned char   text_type;
        unsigned char   text[25];
        unsigned int    af_freq;
};
#endif


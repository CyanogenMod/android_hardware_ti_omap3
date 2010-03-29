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

#include <stdio.h>
#include <fcntl.h>
#include <alsa/asoundlib.h>
#include <linux/videodev.h>
#include <math.h>
#include <pthread.h>
#include <errno.h>

#include "kfmapp.h"

static char *g_mutemodes[]={"Mute On","Mute Off","Attenuate Voice"};
static char *g_bands[]={"Europe/US","Japan"};
static char *g_sm_modes[]={"Stereo","Mono"};
static char *g_rx_deemphasis_modes[]={"50 usec","75 usec"};
static char *g_rds_modes[]={"Off","On"};
static char *g_rds_opmodes[]={"RDS","RDBS"};
static char *g_af_switch_mode[]={"Off","On"};
static int g_vol_to_set;
static pthread_t g_rds_thread_ptr;
volatile char g_rds_thread_terminate,g_rds_thread_running;

/* Program Type */
static char *pty_str[]= {"None", "News", "Current Affairs",
                         "Information","Sport", "Education",
                         "Drama", "Culture","Science",
                         "Varied Speech", "Pop Music",
                         "Rock Music","Easy Listening",
                         "Light Classic Music", "Serious Classics",
                         "other Music","Weather", "Finance",
                         "Childrens Progs","Social Affairs",
                         "Religion", "Phone In", "Travel",
                         "Leisure & Hobby","Jazz", "Country",
                         "National Music","Oldies","Folk",
                         "Documentary", "Alarm Test", "Alarm"};

void fmapp_display_rx_menu(void)
{
   printf("Available FM RX Commands:\n");
   printf("p power on/off\n");
   printf("f <freq> tune to freq\n");
   printf("gf get frequency\n");
   printf("gr get rssi level\n");
   printf("t  turns RDS on/off\n");
   printf("gt get RDS on/off\n");
   printf("+ increases the volume\n");
   printf("- decreases the volume\n");
   printf("v <0-70> sets the volume\n");
   printf("gv get volume\n");
   printf("b switches Japan / Eur-Us\n");
   printf("gb get band\n");
   printf("s switches stereo / mono\n");
   printf("gs get stereo/mono mode\n");
   printf("m changes mute mode\n");
   printf("gm get mute mode\n");
   printf("e set deemphasis filter\n");
   printf("ge get deemphasis filter\n");
   printf("d set rf dependent mute\n");
   printf("gd get rf dependent mute\n");
   printf("z set rds system\n");
   printf("gz get rds system\n");
   printf("c set rds af switch\n");
   printf("gc get rds af switch\n");
   printf("< seek down\n");
   printf("> seek up\n");
   printf("? <(-128)-(127)> set RSSI threshold\n");
   printf("g? get rssi threshold\n");
   printf("ga get tuner attributes\n");
   printf("gn auto scan\n");
   printf("q quit rx menu\n");
}
int fmapp_change_rx_power_mode(snd_ctl_t *fm_snd_ctrl)
{
   static char on_off_status = FM_MODE_RX;
   int ret;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_MODE_SWITCH_CTL_NAME);

   if(on_off_status == FM_MODE_OFF)
      on_off_status  = FM_MODE_RX;
   else
      on_off_status = FM_MODE_OFF;
   snd_ctl_elem_value_set_enumerated(value,CTL_INDEX_0, on_off_status);

   if(on_off_status == FM_MODE_RX)
      printf("Powering on FM RX... (this might take awhile)\n");

   ret = snd_ctl_elem_write(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to perform power on/off\n");
     return ret;
   }

   if(on_off_status == FM_MODE_RX)
      printf("FM RX powered ON\n");
   else
      printf("FM RX powered OFF\n");

   return 0;
}
int fmapp_get_rx_frequency(int radio_fd)
{
   struct v4l2_frequency vf;
   int res;

   res = ioctl(radio_fd, VIDIOC_G_FREQUENCY,&vf);
   if(res < 0)
   {
     printf("Failed to read current frequency\n");
     return res;
   }

   printf("Tuned to frequency %2.1f MHz \n",(float)vf.frequency/1000);
   return 0;
}
int fmapp_set_rx_frequency(int radio_fd,char *cmd)
{
   float user_freq;
   struct v4l2_frequency vf;
   int res;

   sscanf(cmd, "%f", &user_freq);

   vf.tuner = 0;
   vf.frequency = rint(user_freq * 1000);

   res = ioctl(radio_fd, VIDIOC_S_FREQUENCY, &vf);
   if(res < 0)
   {
       printf("Failed to set frequency %f\n",user_freq);
       return res;
   }
   printf("Tuned to frequency %2.1f MHz\n",user_freq);
   return 0;
}

inline void display_volume_bar()
{
  int index;
  printf("\nVolume: ");
  for(index=0; index<g_vol_to_set; index++)
     printf("#");

  printf("\nVolume is : %d\n",g_vol_to_set);

}
int fmapp_set_rx_volume(int radio_fd,char *cmd,int interactive,int vol_to_set)
{
   struct v4l2_control vctrl;
   int res;

   if(interactive == FMAPP_INTERACTIVE)
     sscanf(cmd, "%d", &g_vol_to_set);
   else
     g_vol_to_set = vol_to_set;

   vctrl.id = V4L2_CID_AUDIO_VOLUME;
   vctrl.value = g_vol_to_set;
   res = ioctl(radio_fd,VIDIOC_S_CTRL,&vctrl);
   if(res < 0)
   {
     g_vol_to_set = 0;
     printf("Failed to set volume\n");
     return res;
   }
   printf("Setting volume to %d \n",g_vol_to_set);
   return 0;
}

int fmapp_get_rx_volume(int radio_fd)
{
   struct v4l2_control vctrl;
   int res;

   vctrl.id = V4L2_CID_AUDIO_VOLUME;
   res = ioctl(radio_fd,VIDIOC_G_CTRL,&vctrl);
   if(res < 0)
   {
     printf("Failed to get volume\n");
     return res;
   }
   g_vol_to_set = vctrl.value;
   display_volume_bar();
   return 0;
}

int fmapp_rx_increase_volume(int radio_fd)
{
   int ret;

   g_vol_to_set +=1;
   if(g_vol_to_set > 70)
      g_vol_to_set = 70;

   ret = fmapp_set_rx_volume(radio_fd,NULL,FMAPP_BATCH,g_vol_to_set);
   if(ret < 0)
     return ret;

   display_volume_bar();
   return 0;
}
int fmapp_rx_decrease_volume(int radio_fd)
{
   int ret;
   g_vol_to_set -=1;
   if(g_vol_to_set < 0)
      g_vol_to_set = 0;

   ret = fmapp_set_rx_volume(radio_fd,NULL,FMAPP_BATCH,g_vol_to_set);
   if(ret < 0)
    return ret;

   display_volume_bar();
   return 0;
}
int fmapp_set_rx_mute_mode(int radio_fd)
{
   struct v4l2_control vctrl;
   static int mute_mode = FM_MUTE_OFF;
   int res;

   switch (mute_mode)
   {
     case FM_MUTE_OFF:
          mute_mode = FM_MUTE_ON;
          break;

     case FM_MUTE_ON:
          mute_mode = FM_MUTE_ATTENUATE;
          break;

     case FM_MUTE_ATTENUATE:
          mute_mode = FM_MUTE_OFF;
          break;
   }

   vctrl.id = V4L2_CID_AUDIO_MUTE;
   vctrl.value = mute_mode;
   res = ioctl(radio_fd,VIDIOC_S_CTRL,&vctrl);
   if(res < 0)
   {
     printf("Failed to set mute mode\n");
     return res;
   }

  printf("Setting to \"%s\" \n",g_mutemodes[mute_mode]);
  return 0;
}
int fmapp_get_rx_mute_mode(int radio_fd)
{
   struct v4l2_control vctrl;
   int res;

   vctrl.id = V4L2_CID_AUDIO_MUTE;
   res = ioctl(radio_fd,VIDIOC_G_CTRL,&vctrl);
   if(res < 0)
   {
     printf("Failed to get mute mode\n");
     return res;
   }

   printf("%s\n",g_mutemodes[vctrl.value]);
   return 0;
}
int fmapp_rx_seek(int radio_fd, int seek_direction)
{
   struct v4l2_hw_freq_seek frq_seek;
   int res;

   printf("Seeking %s..\n",seek_direction?"up":"down");
   frq_seek.seek_upward = seek_direction;
   errno = 0;
   res = ioctl(radio_fd,VIDIOC_S_HW_FREQ_SEEK,&frq_seek);
   if(errno == EAGAIN)
   {
     printf("Band limit reached\n");
   }
   else if(res <0)
   {
     printf("Seek operation failed\n");
     return res;
   }
   /* Display seeked freq */
   fmapp_get_rx_frequency(radio_fd);
   return 0;
}
int fmapp_set_band(snd_ctl_t *fm_snd_ctrl)
{
   static unsigned char curr_band = FM_BAND_EUROPE_US;
   int ret;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_BAND_SWITCH_CTL_NAME);

   if(curr_band == FM_BAND_EUROPE_US)
      curr_band  = FM_BAND_JAPAN;
   else if(curr_band == FM_BAND_JAPAN)
      curr_band = FM_BAND_EUROPE_US;

   snd_ctl_elem_value_set_enumerated(value,CTL_INDEX_0, curr_band);

   printf("Switching to %s\n",g_bands[curr_band]);
   ret = snd_ctl_elem_write(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to configure band\n");
     return ret;
   }
   return 0;
}
int fmapp_get_band(snd_ctl_t *fm_snd_ctrl)
{
   int ret;
   unsigned char curr_band;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_BAND_SWITCH_CTL_NAME);
   snd_ctl_elem_value_set_index(value,CTL_INDEX_0);

   ret = snd_ctl_elem_read(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to get band\n");
     return ret;
   }
   curr_band = snd_ctl_elem_value_get_integer(value, CTL_INDEX_0);

   printf("Band is %s\n",g_bands[curr_band]);

   return 0;
}

int fmapp_set_rfmute(snd_ctl_t *fm_snd_ctrl)
{
   static char rf_mute = FM_RX_RF_DEPENDENT_MUTE_OFF;
   int ret;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_RF_DEPENDENT_MUTE_CTL_NAME);

   if(rf_mute == FM_RX_RF_DEPENDENT_MUTE_OFF)
      rf_mute = FM_RX_RF_DEPENDENT_MUTE_ON;
   else
      rf_mute = FM_RX_RF_DEPENDENT_MUTE_OFF;

   snd_ctl_elem_value_set_enumerated(value,CTL_INDEX_0, rf_mute);
   ret = snd_ctl_elem_write(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to perform rfdependent mute\n");
     return ret;
   }

   printf("RF Dependent Mute %s\n",rf_mute ? "on":"off");
   return 0;
}

int fmapp_get_rfmute(snd_ctl_t *fm_snd_ctrl)
{
   int ret;
   unsigned char rf_mute;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_RF_DEPENDENT_MUTE_CTL_NAME);
   snd_ctl_elem_value_set_index(value,CTL_INDEX_0);

   ret = snd_ctl_elem_read(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to get state of rfdependent mute\n");
     return ret;
   }
   rf_mute = snd_ctl_elem_value_get_integer(value, CTL_INDEX_0);

   printf("RF Dependent Mute %s\n",rf_mute ? "on":"off");
   return 0;
}
int fmapp_get_rx_rssi_lvl(snd_ctl_t *fm_snd_ctrl)
{
   int ret;
   short curr_rssi_lvl;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_RX_GET_RSSI_LVL_CTL_NAME);
   snd_ctl_elem_value_set_index(value,CTL_INDEX_0);

   ret = snd_ctl_elem_read(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to get current rssi level\n");
     return ret;
   }
   curr_rssi_lvl = snd_ctl_elem_value_get_integer(value, CTL_INDEX_0);

   printf("RSSI level is %d\n",curr_rssi_lvl);
   return 0;
}
int fmapp_get_rds_operation_mode(snd_ctl_t *fm_snd_ctrl)
{
   int ret;
   unsigned char mode;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_RX_RDS_OPMODE_CTL_NAME);
   snd_ctl_elem_value_set_index(value,CTL_INDEX_0);

   ret = snd_ctl_elem_read(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to get rds operation mode\n");
     return ret;
   }
   mode = snd_ctl_elem_value_get_integer(value, CTL_INDEX_0);

   printf("%s mode\n",g_rds_opmodes[mode]);
   return 0;
}
int fmapp_get_rx_af_switch(snd_ctl_t *fm_snd_ctrl)
{
   int ret;
   unsigned char af_mode;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value,FM_RX_AF_SWITCH_CTL_NAME);
   snd_ctl_elem_value_set_index(value,CTL_INDEX_0);

   ret = snd_ctl_elem_read(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to get af switch mode\n");
     return ret;
   }
   af_mode = snd_ctl_elem_value_get_integer(value, CTL_INDEX_0);
   printf("AF switch is %s\n",g_af_switch_mode[af_mode]);

   return 0;
}
int fmapp_get_rx_rssi_threshold(snd_ctl_t *fm_snd_ctrl)
{
   short rssi_threshold;
   snd_ctl_elem_value_t *value;
   int ret;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_RX_RSSI_THRESHOLD_LVL_CTL_NAME);
   snd_ctl_elem_value_set_index(value,CTL_INDEX_0);

   ret = snd_ctl_elem_read(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to set rssi threshold\n");
     return ret;
   }
   rssi_threshold = snd_ctl_elem_value_get_integer(value, CTL_INDEX_0);

   printf("RSSI threshold set to %d\n",rssi_threshold);
   return 0;
}
int fmapp_set_rds_operation_mode(snd_ctl_t *fm_snd_ctrl)
{
   static unsigned char rds_mode = FM_RDS_SYSTEM_RDS;
   int ret;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_RX_RDS_OPMODE_CTL_NAME);

   if(rds_mode == FM_RDS_SYSTEM_RDS)
      rds_mode = FM_RDS_SYSTEM_RBDS;
   else
      rds_mode = FM_RDS_SYSTEM_RDS;

   snd_ctl_elem_value_set_enumerated(value,CTL_INDEX_0,rds_mode);
   ret = snd_ctl_elem_write(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to set rds operation mode\n");
     return ret;
   }
   printf("Set to %s\n",g_rds_opmodes[rds_mode]);
   return 0;
}
int fmapp_set_rx_af_switch(snd_ctl_t *fm_snd_ctrl)
{
   static unsigned char af_mode = FM_RX_RDS_AF_SWITCH_MODE_OFF;
   int ret;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_RX_AF_SWITCH_CTL_NAME);

   if(af_mode == FM_RX_RDS_AF_SWITCH_MODE_ON)
      af_mode = FM_RX_RDS_AF_SWITCH_MODE_OFF;
   else
      af_mode = FM_RX_RDS_AF_SWITCH_MODE_ON;

   snd_ctl_elem_value_set_enumerated(value,CTL_INDEX_0,af_mode);
   ret = snd_ctl_elem_write(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to set af switch mode\n");
     return ret;
   }
   printf("AF Switch %s\n",g_af_switch_mode[af_mode]);
   return 0;
}
int fmapp_set_rx_rssi_threshold(snd_ctl_t *fm_snd_ctrl,char *cmd)
{
   int rssi_threshold;
   int ret;
   snd_ctl_elem_value_t *value;

   sscanf(cmd, "%d", &rssi_threshold);

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value,FM_RX_RSSI_THRESHOLD_LVL_CTL_NAME);

   snd_ctl_elem_value_set_integer(value,CTL_INDEX_0, rssi_threshold);
   ret = snd_ctl_elem_write(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to set rssi level\n");
     return ret;
   }

   printf("Setting rssi to %d\n",rssi_threshold);
   return 0;
}
int fmapp_set_streo_mono_mode(snd_ctl_t *fm_snd_ctrl)
{
   snd_ctl_elem_value_t *value;
   int ret;
   static int stereo_mono_mode = FM_STEREO_MODE;

   if(stereo_mono_mode == FM_STEREO_MODE)
       stereo_mono_mode = FM_MONO_MODE;
   else
       stereo_mono_mode = FM_STEREO_MODE;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_STEREO_MONO_CTL_NAME);

   snd_ctl_elem_value_set_integer(value,CTL_INDEX_0, stereo_mono_mode);
   ret = snd_ctl_elem_write(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to set stereo/mono mode\n");
     return ret;
   }
   printf("Set to %s Mode\n",g_sm_modes[stereo_mono_mode]);
   return 0;
}
int fmapp_get_streo_mono_mode(snd_ctl_t *fm_snd_ctrl)
{
   int ret;
   unsigned char mode;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value, FM_STEREO_MONO_CTL_NAME);
   snd_ctl_elem_value_set_index(value,CTL_INDEX_0);

   ret = snd_ctl_elem_read(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to get stereo/mono mode\n");
     return ret;
   }
   mode = snd_ctl_elem_value_get_integer(value, CTL_INDEX_0);

   printf("%s mode\n",g_sm_modes[mode]);
   return 0;
}
int fmapp_get_rx_deemphasis_filter_mode(snd_ctl_t *fm_snd_ctrl)
{
   int ret;
   unsigned char mode;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value,FM_RX_DEEMPHASIS_CTL_NAME);
   snd_ctl_elem_value_set_index(value,CTL_INDEX_0);

   ret = snd_ctl_elem_read(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to get rx de-emphasis filter mode\n");
     return ret;
   }
   mode = snd_ctl_elem_value_get_integer(value, CTL_INDEX_0);

   printf("De-emphasis filter %s\n",g_rx_deemphasis_modes[mode]);
   return 0;
}
int fmapp_get_rx_tunner_attributes(int radio_fd)
{
   struct v4l2_tuner vtun;
   float freq_multiplicator;
   float sigstrength_percentage;
   int res;

   vtun.index = 0;
   res = ioctl(radio_fd,VIDIOC_G_TUNER,&vtun);
   if(res < 0)
   {
     printf("Failed to get tunner attributes\n");
     return res;
   }
   printf("-----------------------\n");
   printf("Tuner Name: %s\n",vtun.name);
   freq_multiplicator = (62.5 * ((vtun.capability & V4L2_TUNER_CAP_LOW)
                        ? 1 : 1000));
   printf("  Low Freq: %d %s\n",
           (unsigned int )((float)vtun.rangelow * freq_multiplicator),
          (vtun.capability & V4L2_TUNER_CAP_LOW) ? "Hz":"KHz");
   printf(" High Freq: %d %s\n",
           (unsigned int) ((float)vtun.rangehigh * freq_multiplicator),
          (vtun.capability & V4L2_TUNER_CAP_LOW) ? "Hz":"KHz");
   printf("Audio Mode: %s\n",(vtun.audmode == V4L2_TUNER_MODE_STEREO) ? "STEREO":"MONO");
   sigstrength_percentage = ((float)vtun.signal /0xFFFF) * 100;
   printf("Signal Strength: %d%%\n",(unsigned int)sigstrength_percentage);
   printf("-----------------------\n");
   return 0;
}
int fmapp_get_scan_valid_frequencies(int radio_fd)
{
    int ret;
    struct v4l2_tuner vtun;
    struct v4l2_frequency vf;
    struct v4l2_control vctrl;
    float freq_multiplicator,start_frq,end_frq,
          freq,perc,threshold,divide_by;
    long totsig;
    unsigned char index;

    vtun.index = 0;
    ret = ioctl(radio_fd, VIDIOC_G_TUNER, &vtun); /* get frequency range */
    if (ret < 0) {
   	printf("Failed to get frequency range");
	return ret;
    }
    freq_multiplicator = (62.5 * ((vtun.capability & V4L2_TUNER_CAP_LOW)
                              ? 1 : 1000));

    divide_by = (vtun.capability & V4L2_TUNER_CAP_LOW) ? 1000000 : 1000;
    start_frq = ((float)vtun.rangelow * freq_multiplicator)/divide_by;
    end_frq = ((float)vtun.rangehigh * freq_multiplicator)/divide_by;

    threshold = FMAPP_ASCAN_SIGNAL_THRESHOLD_PER;

    /* Enable Mute */
    vctrl.id = V4L2_CID_AUDIO_MUTE;
    vctrl.value = FM_MUTE_ON;
    ret = ioctl(radio_fd,VIDIOC_S_CTRL,&vctrl);
    if(ret < 0)
    {
      printf("Failed to set mute mode\n");
      return ret;
   }
   printf("Auto Scanning..\n");
   for(freq=start_frq;freq<=end_frq;freq+=0.1)
   {
	vf.tuner = 0;
	vf.frequency = rint(freq*1000);
  	ret = ioctl(radio_fd, VIDIOC_S_FREQUENCY, &vf);	/* tune */
        if (ret < 0) {
		printf("failed to set freq");
		return ret;
	}
	totsig = 0;
	for(index=0;index<FMAPP_ASCAN_NO_OF_SIGNAL_SAMPLE;index++)
	{
		vtun.index = 0;
		ret = ioctl(radio_fd, VIDIOC_G_TUNER, &vtun);	/* get info */
		if (ret < 0) {
			printf("Failed to get frequency range");
			return ret;
		}
		totsig += vtun.signal;
		perc = (totsig / (65535.0 * index));
		usleep(1);
        }
	perc = (totsig / (65535.0 * FMAPP_ASCAN_NO_OF_SIGNAL_SAMPLE));
	if ((perc*100.0) > threshold)
	   printf("%2.1f MHz(%d%%)\n",freq,((unsigned short)(perc * 100.0)));
   }
   /* Disable Mute */
   vctrl.id = V4L2_CID_AUDIO_MUTE;
   vctrl.value = FM_MUTE_OFF;
   ret = ioctl(radio_fd,VIDIOC_S_CTRL,&vctrl);
   if(ret < 0)
   {
      printf("Failed to set mute mode\n");
      return ret;
   }
   printf("Scan Completed\n");
   return 0;
}

int fmapp_set_rx_deemphasis_filter_mode(snd_ctl_t *fm_snd_ctrl)
{
   snd_ctl_elem_value_t *value;
   int ret;
   static int deemphasis_mode = FM_RX_EMPHASIS_FILTER_50_USEC;

   if(deemphasis_mode == FM_RX_EMPHASIS_FILTER_50_USEC)
      deemphasis_mode = FM_RX_EMPHASIS_FILTER_75_USEC;
   else
      deemphasis_mode = FM_RX_EMPHASIS_FILTER_50_USEC;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value,FM_RX_DEEMPHASIS_CTL_NAME);

   snd_ctl_elem_value_set_integer(value,CTL_INDEX_0, deemphasis_mode);
   ret = snd_ctl_elem_write(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to set rx de-emphasis filter mode\n");
     return ret;
   }
   printf("Set to De-emphasis %s mode\n",g_rx_deemphasis_modes[deemphasis_mode]);
   return 0;
}
int fmapp_get_rds_onoff(snd_ctl_t *fm_snd_ctrl)
{
   int ret;
   unsigned char mode;
   snd_ctl_elem_value_t *value;

   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value,FM_RDS_SWITCH_CTL_NAME);
   snd_ctl_elem_value_set_index(value,CTL_INDEX_0);

   ret = snd_ctl_elem_read(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to get rx rds on/off status\n");
     return ret;
   }
   mode = snd_ctl_elem_value_get_integer(value, CTL_INDEX_0);

   printf("RDS %s\n",g_rds_modes[mode]);
   return 0;
}

void fmapp_rds_decode(int blkno, int byte1, int byte2)
{
    static char rds_psn[9];
    static char rds_txt[65];
    static int  rds_pty,ms_code;
    static int group,spare,blkc_byte1,blkc_byte2;

    switch (blkno) {
    case 0: /* Block A */
        printf("----------------------------------------\n");
        printf("block A - id=%d\n",(byte1 << 8) | byte2);
	break;
    case 1: /* Block B */
	printf("block B - group=%d%c tp=%d pty=%d spare=%d\n",
		    (byte1 >> 4) & 0x0f,
		    ((byte1 >> 3) & 0x01) + 'A',
		    (byte1 >> 2) & 0x01,
		    ((byte1 << 3) & 0x18) | ((byte2 >> 5) & 0x07),
		    byte2 & 0x1f);
	group = (byte1 >> 3) & 0x1f;
	spare = byte2 & 0x1f;
	rds_pty = ((byte1 << 3) & 0x18) | ((byte2 >> 5) & 0x07);
        ms_code = (byte2 >> 3)& 0x1;
	break;
    case 2: /* Block C */
        printf("block C - 0x%02x 0x%02x\n",byte1,byte2);
	blkc_byte1 = byte1;
	blkc_byte2 = byte2;
	break;
    case 3 : /* Block D */
	printf("block D - 0x%02x 0x%02x\n",byte1,byte2);
	switch (group) {
	case 0: /* Group 0A */
	    rds_psn[2*(spare & 0x03)+0] = byte1;
	    rds_psn[2*(spare & 0x03)+1] = byte2;
	    if ((spare & 0x03) == 0x03)
		    printf("PSN: %s, PTY: %s, MS: %s\n",rds_psn,
                            pty_str[rds_pty],ms_code?"Music":"Speech");
	    break;
	case 4: /* Group 2A */
	    rds_txt[4*(spare & 0x0f)+0] = blkc_byte1;
	    rds_txt[4*(spare & 0x0f)+1] = blkc_byte2;
	    rds_txt[4*(spare & 0x0f)+2] = byte1;
	    rds_txt[4*(spare & 0x0f)+3] = byte2;
            /* Display radio text once we get 16 characters */
//	    if ((spare & 0x0f) == 0x0f)
	    if (spare > 16)
            {
	        printf("Radio Text: %s\n",rds_txt);
//              memset(&rds_txt,0,sizeof(rds_txt));
            }
	    break;
         }
         printf("----------------------------------------\n");
         break;
     default:
         printf("unknown block [%d]\n",blkno);
    }
}
void *rds_thread(void *data)
{
  unsigned char buf[600];
  int radio_fd;
  int ret,index;
  radio_fd = (int)data;

  while(!g_rds_thread_terminate)
  {
    ret = read(radio_fd,buf,500);
    if(ret < 0)
       break;
    else if( ret > 0)
    {
       for(index=0;index<ret;index+=3)
         fmapp_rds_decode(buf[index+2] & 0x7,buf[index+1],buf[index]);
    }
  }
/* TODO: Need to conform thread termination.
 * below msg is not coming ,have a doubt on thread termination.
 * Fix this later.  */
  printf("RDS thread exiting..\n");
  return NULL;
}
int fmapp_set_rds_onoff(int radio_fd,snd_ctl_t *fm_snd_ctrl,unsigned char fmapp_mode)
{
   snd_ctl_elem_value_t *value;
   int ret;
   static unsigned char rds_mode = FM_RDS_DISABLE;

   if(rds_mode == FM_RDS_DISABLE)
   {
      rds_mode = FM_RDS_ENABLE;
   }
   else
   {
     rds_mode = FM_RDS_DISABLE;
   }
   snd_ctl_elem_value_alloca(&value);
   snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
   snd_ctl_elem_value_set_name(value,FM_RDS_SWITCH_CTL_NAME);

   snd_ctl_elem_value_set_integer(value,CTL_INDEX_0, rds_mode);
   ret = snd_ctl_elem_write(fm_snd_ctrl, value);
   if(ret < 0)
   {
     printf("Failed to set rds on/off status\n");
     return ret;
   }
   /* Create rds receive thread once */
   if(fmapp_mode == FM_MODE_RX && rds_mode == FM_RDS_ENABLE &&
      g_rds_thread_running == 0)
   {
      g_rds_thread_running = 1;
      pthread_create(&g_rds_thread_ptr,NULL,rds_thread,(void *)radio_fd);
   }

   printf("RDS %s\n",g_rds_modes[rds_mode]);
   return 0;
}
void fmapp_execute_rx_get_command(int radio_fd,snd_ctl_t *fm_snd_ctrl,char *cmd)
{
   switch(cmd[0])
   {
     case 'f':
          fmapp_get_rx_frequency(radio_fd);
          break;
     case 'r':
	  fmapp_get_rx_rssi_lvl(fm_snd_ctrl);
          break;
     case 't':
          fmapp_get_rds_onoff(fm_snd_ctrl);
          break;
     case 'v':
	  fmapp_get_rx_volume(radio_fd);
          break;
     case 'm':
          fmapp_get_rx_mute_mode(radio_fd);
          break;
     case 'b':
          fmapp_get_band(fm_snd_ctrl);
          break;
     case 'd':
	  fmapp_get_rfmute(fm_snd_ctrl);
          break;
     case 'z':
          fmapp_get_rds_operation_mode(fm_snd_ctrl);
	  break;
     case 'c':
          fmapp_get_rx_af_switch(fm_snd_ctrl);
	  break;
     case '?':
	  fmapp_get_rx_rssi_threshold(fm_snd_ctrl);
	  break;
     case 's':
          fmapp_get_streo_mono_mode(fm_snd_ctrl);
          break;
     case 'e':
          fmapp_get_rx_deemphasis_filter_mode(fm_snd_ctrl);
          break;
     case 'a':
	  fmapp_get_rx_tunner_attributes(radio_fd);
          break;
     case 'n':
	  fmapp_get_scan_valid_frequencies(radio_fd);
	  break;
     default:
          printf("unknown command; type 'h' for help\n");
   }
}
void fmapp_execute_rx_other_command(int radio_fd,snd_ctl_t *fm_snd_ctrl,char *cmd)
{
   switch(cmd[0])
   {
     case 'p':
          fmapp_change_rx_power_mode(fm_snd_ctrl);
          break;
     case 'f':
          fmapp_set_rx_frequency(radio_fd,cmd+1);
          break;
     case 't':
          fmapp_set_rds_onoff(radio_fd,fm_snd_ctrl,FM_MODE_RX);
          break;
     case '+':
	  fmapp_rx_increase_volume(radio_fd);
          break;
     case '-':
          fmapp_rx_decrease_volume(radio_fd);
          break;
     case 'v':
	  fmapp_set_rx_volume(radio_fd,cmd+1,FMAPP_INTERACTIVE,0);
	  break;
     case 'm':
          fmapp_set_rx_mute_mode(radio_fd);
          break;
     case '<':
          fmapp_rx_seek(radio_fd,FM_SEARCH_DIRECTION_DOWN);
	  break;
     case '>':
          fmapp_rx_seek(radio_fd,FM_SEARCH_DIRECTION_UP);
	  break;
     case 'b':
          fmapp_set_band(fm_snd_ctrl);
          break;
     case 'h':
          fmapp_display_rx_menu();
          break;
     case 'd':
	  fmapp_set_rfmute(fm_snd_ctrl);
          break;
     case 'z':
	  fmapp_set_rds_operation_mode(fm_snd_ctrl);
	  break;
     case 'c':
          fmapp_set_rx_af_switch(fm_snd_ctrl);
	  break;
     case '?':
	  fmapp_set_rx_rssi_threshold(fm_snd_ctrl,cmd+1);
	  break;
     case 's':
          fmapp_set_streo_mono_mode(fm_snd_ctrl);
          break;
     case 'e':
          fmapp_set_rx_deemphasis_filter_mode(fm_snd_ctrl);
          break;
  }
}
void fmapp_execute_rx_command(int radio_fd,snd_ctl_t *fm_snd_ctrl)
{
   char cmd[100];

   printf("Switched to RX menu\n");
   printf("type 'h' for help\n");

   while(1)
   {
     fgets(cmd, sizeof(cmd), stdin);
     switch(cmd[0])
     {
      case 'g':
          fmapp_execute_rx_get_command(radio_fd,fm_snd_ctrl,cmd+1);
	  break;
      case 'q':
          printf("quiting RX menu\n");
 	  return;
      default:
          fmapp_execute_rx_other_command(radio_fd,fm_snd_ctrl,cmd);
	  break;
     }
   }
}
void fmapp_display_tx_menu(void)
{
	printf("Available FM TX Commands:\n");
	printf("p power on/off\n");
	printf("f <freq> tune to freq\n");
	printf("r enable rds\n");
	printf("m changes mute mode\n");
	printf("s switches stereo / mono\n");
	printf("b switches Japan / Eur-Us\n");
	printf("0 <Type,AF,Radio Text> pls conform to format\n");
#if 0
	printf("gf get frequency\n");
	printf("gr get rssi level\n");
	printf("gt get RDS on/off\n");
	printf("+ increases the volume\n");
	printf("- decreases the volume\n");
	printf("gm get mute mode\n");
	printf("v <0-70> sets the volume\n");
	printf("gv get volume\n");
	printf("< seek down\n");
	printf("> seek up\n");
	printf("b switches Japan / Eur-Us\n");
	printf("gb get band\n");
	printf("gs get stereo/mono mode\n");
	printf("e set deemphasis filter\n");
	printf("ge get deemphasis filter\n");
	printf("d set rf dependent mute\n");
	printf("gd get rf dependent mute\n");
	printf("? <(-16)-(15)> set RSSI threshold\n");
	printf("g? get rssi threshold\n");
#endif
	printf("q quit rx menu\n");
}
int fmapp_set_rx_rds_fifo(int radio_fd, char *str)
{
        float user_freq = 0;
        struct tx_rds   rds;

        sscanf(str, "%d,%f,%[^\n]s\n", &rds.text_type, &user_freq, rds.text);
        rds.af_freq = rint(user_freq * 1000);
        printf("RDS: >%d<, >%s<, >%d<\n", rds.text_type, rds.text, rds.af_freq);

        write(radio_fd, &rds, sizeof(rds));
        return 0;
}

int fmapp_set_tx_mute_mode(int radio_fd)
{
	struct v4l2_control vctrl;
	static int tx_mute_mode = FM_MUTE_OFF;
	int res;

	switch (tx_mute_mode)
	{
		case FM_MUTE_OFF:
			tx_mute_mode = FM_MUTE_ON;
			break;

		case FM_MUTE_ON:
			tx_mute_mode = FM_MUTE_OFF;
			break;
	}

	vctrl.id = V4L2_CID_AUDIO_MUTE;
	vctrl.value = tx_mute_mode;
	res = ioctl(radio_fd,VIDIOC_S_CTRL,&vctrl);
	if(res < 0)
	{
		printf("Failed to set mute mode\n");
		return res;
	}

	printf("Setting to \"%s\" \n",g_mutemodes[tx_mute_mode]);
	return 0;
}
int fmapp_change_tx_power_mode(snd_ctl_t *fm_snd_ctrl)
{
	static char on_off_status = FM_MODE_TX;
	int ret;
	snd_ctl_elem_value_t *value;

	snd_ctl_elem_value_alloca(&value);
	snd_ctl_elem_value_set_interface(value, SND_CTL_ELEM_IFACE_MIXER);
	snd_ctl_elem_value_set_name(value, FM_MODE_SWITCH_CTL_NAME);

	if(on_off_status == FM_MODE_OFF)
		on_off_status  = FM_MODE_TX;
	else
		on_off_status = FM_MODE_OFF;
	snd_ctl_elem_value_set_enumerated(value,CTL_INDEX_0, on_off_status);

	if(on_off_status == FM_MODE_TX)
		printf("Powering on FM TX... (this might take awhile)\n");

	ret = snd_ctl_elem_write(fm_snd_ctrl, value);
	if(ret < 0)
	{
		printf("Failed to perform power on/off\n");
		return ret;
	}

	if(on_off_status == FM_MODE_TX)
		printf("FM TX powered ON\n");
	else
		printf("FM TX powered OFF\n");

	return 0;
}
int fmapp_set_tx_frequency(int radio_fd,char *cmd)
{
	float user_freq;
	struct v4l2_frequency vf;
	int res;

	sscanf(cmd, "%f", &user_freq);

	vf.tuner = 0;
	vf.frequency = rint(user_freq * 1000);

	res = ioctl(radio_fd, VIDIOC_S_FREQUENCY, &vf);
	if(res < 0)
	{
		printf("Failed to set frequency %f\n",user_freq);
		return res;
	}

	printf("Tuned to frequency %2.1f\n",user_freq);
	return 0;
}
void fmapp_execute_tx_other_command(int radio_fd,snd_ctl_t *fm_snd_ctrl,char *cmd)
{
	switch(cmd[0])
	{
		case 'p':
			fmapp_change_tx_power_mode(fm_snd_ctrl);
			break;
		case 'f':
			fmapp_set_tx_frequency(radio_fd,cmd+1);
			break;
		case 'r':
			fmapp_set_rds_onoff(radio_fd,fm_snd_ctrl,FM_MODE_TX);
			break;
		case 'l':
//			fmapp_set_tx_pwr_lvl(fm_snd_ctrle;
			break;
		case 'm':
			fmapp_set_tx_mute_mode(radio_fd);
			break;
		case 's':
			fmapp_set_streo_mono_mode(fm_snd_ctrl);
			break;
		case 'b':
			fmapp_set_band(fm_snd_ctrl);
			break;
		case '0':
			fmapp_set_rx_rds_fifo(radio_fd, cmd+1);
			break;
		case 'h':
			fmapp_display_tx_menu();
			break;
	}
}
void fmapp_execute_tx_command(int radio_fd,snd_ctl_t *fm_snd_ctrl)
{
	char cmd[100];

	printf("Switched to TX menu\n");
	fmapp_display_tx_menu();

	while(1)
	{
		fgets(cmd, sizeof(cmd), stdin);
		switch(cmd[0])
		{
			case 'q':
				printf("quiting TX menu\n");
				return;
			default:
				fmapp_execute_tx_other_command(radio_fd,fm_snd_ctrl,cmd);
				break;
		}
	}
}

int fmapp_read_anddisplay_capabilities(int radio_fd)
{
  struct v4l2_capability cap;
  int res;

  res = ioctl(radio_fd,VIDIOC_QUERYCAP,&cap);
  if(res < 0)
  {
    printf("Failed to read %s capabilities\n",DEFAULT_RADIO_DEVICE);
    return res;
  }
  if((cap.capabilities & V4L2_CAP_RADIO) == 0)
  {
    printf("%s is not radio devcie",DEFAULT_RADIO_DEVICE);
    return -1;
  }
  printf("\n***%s Info ****\n",DEFAULT_RADIO_DEVICE);
  printf("Driver       : %s\n",cap.driver);
  printf("Card         : %s\n",cap.card);
  printf("Bus          : %s\n",cap.bus_info);
  printf("Capabilities : 0x%x\n",cap.capabilities);

  return 0;
}
int main()
{
   char choice[100];
   char exit_flag;
   static int radio_fd;
   snd_ctl_t *fm_snd_ctrl;
   int ret;

   printf("** TI Kernel Space FM Driver Test Application **\n");

   printf("Opening device %s\n",DEFAULT_RADIO_DEVICE);
   radio_fd = open(DEFAULT_RADIO_DEVICE, O_RDWR);
   if(radio_fd < 0)
   {
       printf("Unable to open %s \nTerminating..\n",DEFAULT_RADIO_DEVICE);
       return 0;
   }
   ret = snd_ctl_open(&fm_snd_ctrl,DEFAULT_FM_ALSA_CARD,0);
   if(ret < 0)
   {
       printf("%s open error [error code %d]",DEFAULT_FM_ALSA_CARD, ret);
       close(radio_fd);
       return 0;
   }
   ret = fmapp_read_anddisplay_capabilities(radio_fd);
   if(ret< 0)
   {
     close(radio_fd);
     snd_ctl_close(fm_snd_ctrl);
     return ret;
   }
   exit_flag = 1;
   while(exit_flag)
   {
       printf("1 FM RX\n");
       printf("2 FM TX\n");
       printf("3 Exit\n");
       fgets(choice, sizeof(choice), stdin);

       switch(atoi(choice))
       {
          case 1: /* FM RX */
              fmapp_execute_rx_command(radio_fd,fm_snd_ctrl);
              break;
	  case 2: /* FM TX */
              fmapp_execute_tx_command(radio_fd,fm_snd_ctrl);
              break;
          case 3:
              printf("Terminating..\n\n");
              exit_flag = 0;
              break;
          default:
              printf("Invalid choice , try again\n");
              continue;
      }
   }
   if(g_rds_thread_running)
      g_rds_thread_terminate = 1; // Terminate RDS thread

   close(radio_fd);
   snd_ctl_close(fm_snd_ctrl);
   return 0;
}

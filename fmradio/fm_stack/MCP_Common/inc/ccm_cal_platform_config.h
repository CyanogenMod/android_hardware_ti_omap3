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

/*******************************************************************************\
*
*   FILE NAME:      ccm_cal_platform_config.h
*
*   BRIEF:          This file defines the platform dependednd configuration 
*                   parameters 
*                  
*   DESCRIPTION:    
*
*   AUTHOR:         Malovany Ram 
*
\*******************************************************************************/
#ifndef __CCM_CAL_PLATFORM_CONFIG_H__
#define __CCM_CAL_PLATFORM_CONFIG_H__
/*******************************************************************************
 *
 * Defines per host configuration
 *
 ******************************************************************************/
#define NUMBER_OF_PCM_CLK_CH1			(1)
#define NUMBER_OF_PCM_CLK_CH2 			(17)
/*******************************************************************************
 *
 * Write codec Configuration command parameters
 *
 ******************************************************************************/
#define PCM_CLOCK_RATE					(3072)
#define PCM_DIRECTION_ROLE				(0)	/* Master =0 Slave =1*/
#define FRAME_SYNC_DUTY_CYCLE			(1)
#define FRAME_SYNC_EDGE					(1)	/* Rising Edge =0 , Falling Edge =1 */
#define FRAME_SYNC_POLARITY				(0) /*Active-high=0 , Active-low =1 */
#define CH1_DATA_OUT_SIZE				(16)
#define CH1_DATA_OUT_OFFSET				(1)
#define CH1_OUT_EDGE					(1)/* Rising Edge =0 , Falling Edge =1 */
#define CH1_DATA_IN_SIZE				(16)
#define CH1_DATA_IN_OFFSET				(NUMBER_OF_PCM_CLK_CH1) /*Number of PCM clock cycles between rising of frame sync of data start */
#define CH1_IN_EDGE						(0)/* Rising Edge =0 , Falling Edge =1 */
#define CH2_DATA_OUT_SIZE				(16)
#define CH2_DATA_OUT_OFFSET				(NUMBER_OF_PCM_CLK_CH2)
#define CH2_OUT_EDGE					(1)/* Rising Edge =0 , Falling Edge =1 */
#define CH2_DATA_IN_SIZE				(16)
#define CH2_DATA_IN_OFFSET				(17)
#define CH2_IN_EDGE						(0)/* Rising Edge =0 , Falling Edge =1 */

/*******************************************************************************
 *
 * Digital (I2S\PCM) FM Configurations
 *
 ******************************************************************************/

/*******************************************************************************
 * PCM FM Configurations
 ******************************************************************************/
/*
Bit 0 - PCMI_i2sz_sel
'0' - I2S/PCM interface is operating on I2S protocol (default)
'1' - I2S/PCM interface is operating on PCM protocol
*/
#define FM_PCMI_I2S_SELECT_OFFSET				(0)
/*
Bit1 - PCMI_left_right_swap
'0' - Left data is sent out as first channel (default)
'1' - Right data is sent out as first channel 
*/

#define FM_PCMI_RIGHT_LEFT_SWAP				(0x0000)/*Left data is sent out as first channel */
#define FM_PCMI_RIGHT_LEFT_SWAP_OFFSET				(1)
/*
Bit 4:2 - bit offset vector
Fine offset from 0 to 7. This will be added to Slot Offset Vector, to determine when the transmission or reception of a new channel data should start, within a framesync period, after the active framesync edge. (000-default)
*/

#define FM_PCMI_BIT_OFFSET_VECTOR			(0x0000)
#define FM_PCMI_BIT_OFFSET_VECTOR_OFFSET			(2)
/*
Bit 8:5 - Slot offset vector
Offset in multiples of 8 from the active edge of Framesync, at which tristate should be deactived. (0000 - default)
*/

#define FM_PCMI_SLOT_OFSET_VECTOR			(0x0001)
#define FM_PCMI_SLOT_OFSET_VECTOR_OFFSET			(5)
/*
Bit 10:9 - PCM interface channel data size
Number of bits/channel (left/right) in an audio sample

00 - 16 bits (default); 01 - 15 bits ; 10 - 14 bits;  11 - 13 bits

*/
#define FM_PCMI_PCM_INTERFACE_CHANNEL_DATA_SIZE	(0x0000)
#define FM_PCMI_PCM_INTERFACE_CHANNEL_DATA_SIZE_OFFSET	(9)

/*******************************************************************************
 * I2S FM Configurations
 ******************************************************************************/
/*
Bits 3:0 - Data width
Sets the expected clock frequency with respect to Frame?Sync, as well as the bit length of the word.
0000 - bit-clock = 32Fs 	   
0001 - bit-clock = 40Fs  (Removed)
0010 - bit-clock = 45Fs and right channel uses 23 bits
0011 - bit-clock = 45Fs and right channel uses 22 bits
0100 - bit-clock = 48Fs 	   
0101 - bit-clock = 50Fs (default)
0110 - bit-clock = 60Fs  
0111 -	   bit-clock = 64Fs
1000 -	   bit-clock = 80Fs
1001 - bit-clock = 96Fs
1010 - bit clock = 128Fs
*/
#define FM_I2S_DATA_WIDTH					(0x0000)
#define FM_I2S_DATA_WIDTH_OFFSET			(0)
/*
Bits 5:4 - Data Format
Controls the I2S format
00 - I2S Standard format (default)
01 - I2S Left Justified format
10 - I2S Right Justified format
11 - User defined Offset Format* supplied by host. UD mode is not supported for 45 FS data width
*/

#define FM_I2S_DATA_FORMAT					(0x0000)
#define FM_I2S_DATA_FORMAT_OFFSET			(4)
/*
Bit 6 - 0 = master, 1 = slave
Sets I2S interface in Slave or Master Mode
*/

#define FM_I2S_MASTER_SLAVE					(0x0000)
#define FM_I2S_MASTER_SLAVE_OFFSET			(6)
/*
Bit 7 - SDO Tri-state mode
0 - SDO is tri-stated after sending (default)		 
1 - buffer always active
*/

#define FM_I2S_SDO_TRI_STATE_MODE					(0x0001)
#define FM_I2S_SDO_TRI_STATE_MODE_OFFSET			(7)
/*
Bit 8 - SDO phase select
Bit 9 -- WS phase (slave only) 
Clock edge for SDO/WS change
"00" - WS and SDI sampled on rising edge/WS and SDO outputted on rising edge
"01" - WS sampled/outputted on rising edge; SDI sampled/SDO outputted on falling edge  - (default)
"10" - WS sampled/outputted on falling edge; SDI sampled/SDO outputted on rising edge
"11" - WS and SDI sampled on falling edge/WS and SDO outputted on falling edge
*/

#define FM_I2S_SDO_PHASE_WS_PHASE_SELECT					(0x0000)
#define FM_I2S_SDO_PHASE_WS_PHASE_SELECT_OFFSET			(8)

/*
Bit 10 - SDO_3st_alwz
serial data out tri-state control
"0" Tristate based on other options - default
"1" Tristate always
*/

#define FM_I2S_SDO_3ST_ALWZ					(0x0000)
#define FM_I2S_SDO_3ST_ALWZ_OFFSET 		(10)
/*

Bit 15-12: Frame_sync_rate 
0 - 48 KHz (default)
1 - 44.1 KHz
2 - 32 KHz
4 - 24 KHz
5 - 22.05 KHz
6 - 16 KHz
8 - 12 KHz
9 - 11.025KHz
10 - 8 KHz
*/

#define FM_I2S_FRAME_SYNC_RATE					(0x0000)
#define FM_I2S_FRAME_SYNC_RATE_OFFSET 		(12)





#endif /* __CCM_CAL_PLATFORM_CONFIG_H__ */


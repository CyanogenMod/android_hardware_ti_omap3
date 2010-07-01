/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */

#include <dirent.h>
#include "SkBitmap.h"
#include "SkStream.h"
#include "SkImageDecoder.h"
#include "SkImageEncoder.h"

namespace android {
    Mutex       countMutex;
}; //namespace android

//------------------------------------------------------------------------- JPEG
// code byte (which is not an FF).  Here are the marker codes of interest
// in this program.  (See jdmarker.c for a more complete list.)
//--------------------------------------------------------------------------

#define M_SOF0  0xC0          // Start Of Frame N
#define M_SOF1  0xC1          // N indicates which compression process
#define M_SOF2  0xC2          // Only SOF0-SOF2 are now in common use
#define M_SOF3  0xC3
#define M_SOF5  0xC5          // NB: codes C4 and CC are NOT SOF markers
#define M_SOF6  0xC6
#define M_SOF7  0xC7
#define M_SOF9  0xC9
#define M_SOF10 0xCA
#define M_SOF11 0xCB
#define M_SOF13 0xCD
#define M_SOF14 0xCE
#define M_SOF15 0xCF
#define M_SOI   0xD8          // Start Of Image (beginning of datastream)
#define M_EOI   0xD9          // End Of Image (end of datastream)
#define M_SOS   0xDA          // Start Of Scan (begins compressed data)
#define M_JFIF  0xE0          // Jfif marker
#define M_EXIF  0xE1          // Exif marker.  Also used for XMP data!
#define M_XMP   0x10E1        // Not a real tag (same value in file as Exif!)
#define M_COM   0xFE          // COMment
#define M_DQT   0xDB
#define M_DHT   0xC4
#define M_DRI   0xDD
#define M_IPTC  0xED          // IPTC marker

typedef unsigned char U8;
typedef unsigned long U32;
typedef signed long   S32;
typedef struct dirent DIRENTRY;

typedef struct _JPEG_IMAGE_INFO {
    int nWidth;
    int nHeight;
}JPEG_IMAGE_INFO;

const struct JPGD_TEST_OUTPUT_FILE_LIST {
   char fileName[128];
   char md5CheckSumArm[33];
   char md5CheckSumTi[33];
   char md5CheckSumSimcop[33];

}st_jpgd_file_list[] = {

{ //L_SKIA_JPGD_0001
"4X4_rgb16.raw",
"2b9acee0619b37dcb71c162af547a511",
"6777f86ae4b429920f8d07bb0c287528",
"0"
},
{//L_SKIA_JPGD_0002
"img_70x70_rgb16.raw",
"3fb218dc4ab1088a4eb3e97d66e4c12a",
"4b055c105bbb139f56626f4c92046e72",
"0"
},
{//L_SKIA_JPGD_0003
"img01_94x80_rgb16.raw",
"b98c4f3160ac37c9ee49c72923e78ae5",
"22d0747166587e44f50240a13600a02e",
"0"
},
{//L_SKIA_JPGD_0004
"I_000283_SQCIF128x96_prog_rgb16.raw",
"b0a223a268a3b4abc01e2ca19b8be786",
"b6addcf589c41da5d99ddb6529b1e114",
"0"
},
{ //L_SKIA_JPGD_0005
"I_000284_SQCIF128x96_seq_rgb16.raw",
"1057d4ab991300e794abae780b9bc7d7",
"fac7a4514225319acd70652ec94a0311",
"0"
},
{//L_SKIA_JPGD_0006
"I_000280_QQVGA160x120_prog_rgb16.raw",
"4027d1cc522f68a583e10f6f05d5d686",
"13a09115b8b86ac8a7a1abe308cef60f",
"0"
},
{//L_SKIA_JPGD_0007
"I_000281_QQVGA160X120_seq_rgb16.raw",
"af85d380e381740670867a9e62948a62",
"333919ec855413d597aad945a53ead56",
"0"
},
{//L_SKIA_JPGD_0008
"I_000278_QCIF176X144_prog_rgb16.raw",
"87f8b887a6c6613dd70100cb7e555557",
"fb527cf55d387dd14d554af9707ed676",
"0"
},
{//L_SKIA_JPGD_0009
"I_000279_QCIF176X144_seq_rgb16.raw",
"72759b03e0bc80d940a7a63031fa59c4",
"f16bd94c374209476bdf01227da0c3b6",
"0"
},
{ //L_SKIA_JPGD_0010
"img_200x200_rgb16.raw",
"a77a7c5e396266ea30b3940b93c7deb8",
"5fcf5d522c24433bbc69d373061dc2ce",
"0"
},
{//L_SKIA_JPGD_0011
"I_000275_CGA320x200_prog_rgb16.raw",
"42e8d2d67a041916d09d22c9cbff7ca3",
"cef9dac3fbfa4a9da52d653663f42d7d",
"0"
},
{//L_SKIA_JPGD_0012
"I_000276_CGA320x200_seq_rgb16.raw",
"58a49c90a37e4d38a851218b807b3717",
"aa93d1d18890dd12f0cdbb4a71f9d043",
"0"
},
{//L_SKIA_JPGD_0013
"I_000282_QVGA320X240_prog_rgb16.raw",
"7bd50ae9193c643593d500305c0eb759",
"088cdd4b2caaeea763c5c4f1205c91ad",
"0"
},
{//L_SKIA_JPGD_0014
"laugh_QVGA320x240_seq_rgb16.raw",
"7431b5f79bf4cd47d8836e236a8d5578",
"429af3044e0d902f875a6e7df5bb9a00",
"0"
},
{ //L_SKIA_JPGD_0015
"I_000137_VGA_0.3MP_480x640_rgb16.raw",
"8bf35cca44ef8ba2b7303741c5de729c",
"5732bc27a98ce6db32c363638b2311e7",
"0"
},
{//L_SKIA_JPGD_0016
"I_000066_SVGA_0.5MP_600x800_rgb16.raw",
"f33cf01c88c7fbe0317bca246daced1b",
"9ff991c346123198ed7faea71e87376f",
"0"
},
{//L_SKIA_JPGD_0017
"shrek_D1_720x480_rgb16.raw",
"dd957a6adb17187f8df2bafa4e19d79e",
"4bbd5d406cc08c1c80ea8d444c0b3525",
"0"
},
{//L_SKIA_JPGD_0018
"I_000245_XGA_0.8MP_768x1024_rgb16.raw",
"c368f85091e6519d14695300bc2ea885",
"1c7aea5781e525b6a403145f00d52e19",
"0"
},
{//L_SKIA_JPGD_0019
"patzcuaro_800x600_rgb16.raw",
"edb0d6cc412f3d33375b1bb309547387",
"3c1bbbf46c8e557492ecb19e422fd1f0",
"0"
},
{ //L_SKIA_JPGD_0020
"I_000263_2MP_1600x1200_prog_rgb16.raw",
"a0b7023480da14df9fa51890c74bbbd7",
"02509a23fe0d09fd21f69182212daf83",
"0"
},
{//L_SKIA_JPGD_0021
"I_000272_2MP_1600x1200_seq_rgb16.raw",
"6c7a9ece7efb324195a9b050fd978f2a",
"1923760e05c6e1392bf865f01d1403e8",
"0"
},
{//L_SKIA_JPGD_0022
"I_000268_5MP_2560x1920_prog_rgb16.raw",
"23f56c22eee9058c05a11f1116903576",
"18fa305c19b9628450789ae6858b00d7",
"0"
},
{//L_SKIA_JPGD_0023
"I_000259_5MP_internet_seq_2560x1920_rgb16.raw",
"2c7a8bc37900d86b3fb9d5af6de728c8",
"cd7a903ddd93e9be41b79334249a7fc4",
"0"
},
{//L_SKIA_JPGD_0024
"6MP_3072x2048_internet_seq_rgb16.raw",
"77d0e5e52c214a0d8f00340b9c6c7d02",
"1facb45e2b83b07d6e652b467b98326a",
"0"
},
{ //L_SKIA_JPGD_0025
"I_000264_8MP_3264x2448_43_ratio_prog_rgb16.raw",
"5cb456948b9057a9c015e7f5de124a73",
"93a985fec9de9ea97e4c4133e679f704",
"0"
},
{//L_SKIA_JPGD_0026
"I_000261_8MP_internet_seq_4000x2048_rgb16.raw",
"e84c18d2b01c47c250b6d4454ecaf00e",
"c63121316e64a7dddb4f63e567455f9f",
"0"
},
{//L_SKIA_JPGD_0027
"I_000266_12MP_4000x3000_prog_rgb16.raw",
"7bddd72c9841988b8466f19489094ace",
"024ab26e63cb594f738f0b0a02b45101",
"0"
},
{//L_SKIA_JPGD_0028
"I_000265_12MP_4000x3000_seq_rgb16.raw",
"63e741c4fc71ad4d00b54908afffd599",
"13218edcbff4fc86624fcf70b9d27ed0",
"0"
},
{ //L_SKIA_JPGD_0029
"4X4_rgb32.raw",
"b40a90eed578461ea192549aaf220a54",
"a509a311c1b0186530c3ead9a2368f07",
"0"
},
{ //L_SKIA_JPGD_0030
"I_000283_SQCIF128x96_prog_rgb32.raw",
"5ae3951218bd356a1e3ec74901383aba",
"2a9057e6865616b3d1eafb942b897cf8",
"0"
},
{//L_SKIA_JPGD_0031
"I_000276_CGA320x200_seq_rgb32.raw",
"d462bbb655f86b4a61d001e6b5970eaa",
"24dd2926f0532d8975793e42563586eb",
"0"
},
{//L_SKIA_JPGD_0032
"I_000137_VGA_0.3MP_480x640_rgb32.raw",
"ed6d2f853ab88fe3ab650f0ff0421e7a",
"7283b2371644b220d9e7e2c8e7de3e77",
"0"
},
{//L_SKIA_JPGD_0033
"patzcuaro_800x600_rgb32.raw",
"46a7e0cbcece24317b2d5b46e62ab9c4",
"41ef3153e3029b777583163d2df1828d",
"0"
},
{//L_SKIA_JPGD_0034
"I_000245_XGA_0.8MP_768x1024_rgb32.raw",
"66361a6b7892ea0b73a907a1e5d1cc11",
"399fcbd4621999ab5f9f0178712f7995",
"0"
},
{ //L_SKIA_JPGD_0035
"I_000263_2MP_1600x1200_prog_rgb32.raw",
"fffa2122e9e4d1f9b7025cab54fb3b91",
"b9af9d8a998ff27a82a8257ab41c7b7c",
"0"
},
{//L_SKIA_JPGD_0036
"I_000259_5MP_internet_seq_2560x1920_rgb32.raw",
"425700943da62a28ee9aee06a24b99a3",
"de70232d3ab011dae6cfd8a9b88105cf",
"0"
},
{//L_SKIA_JPGD_0037
"I_000264_8MP_3264x2448_43_ratio_prog_rgb32.raw",
"8cdb419d06024f647ed78bf703cabf82",
"56e7ebc2bf4e87343aba9ebb4206201e",
"0"
},
{//L_SKIA_JPGD_0038
"I_000265_12MP_4000x3000_seq_rgb32.raw",
"3839ff350f861e44c97543d1860cde51",
"e3f43253741d2fcfb85e34f318d1f703",
"0"
},
{//L_SKIA_JPGD_0039
"patzcuaro_800x600_rgb16_resized_50.raw",
"07292fb019e18fc6ff4b63712d4885d8",
"77454844fc4e2d449906ebf48c919bde",
"0"
},
{ //L_SKIA_JPGD_0040
"I_000263_2MP_1600x1200_prog_rgb16_resized_50.raw",
"f64b04d5d09a05961f340aef6949b75b",
"36a4d22709578fc341eee9fcae92cf7a",
"0"
},
{//L_SKIA_JPGD_0041
"I_000272_2MP_1600x1200_seq_rgb16_resized_25.raw",
"8b4e626b6d2a38e2f58ea588b44c2aa1",
"920ed0fb130eafab849ec9754cad066f",
"0"
},
{//L_SKIA_JPGD_0042
"I_000268_5MP_2560x1920_prog_rgb16_resized_25.raw",
"dd726cb8c337c52c70945d1a08d041b5",
"5372d22fda640313fffffcfdce42b3d8",
"0"
},
{//L_SKIA_JPGD_0043
"I_000261_8MP_internet_seq_4000x2048_rgb16_resized_12.raw",
"1ac8e419fe3a88aec09f6a6361ef42f7",
"9c86917230195c7694a4b24c9b0c7174",
"0"
},
{//L_SKIA_JPGD_0044
"I_000265_12MP_4000x3000_seq_rgb16_resized_12.raw",
"28f93e79922d88c3653c32a34ad9c05b",
"2757a17da91891646484f85d60613cc2",
"0"
},
{ //L_SKIA_JPGD_0045
"patzcuaro_800x600_rgb32_resized_50.raw",
"05dbe1ce920ff53aba42191855899503",
"24bda07f57822232e88ffae9e2901b1a",
"0"
},
{//L_SKIA_JPGD_0046
"I_000263_2MP_1600x1200_prog_rgb32_resized_50.raw",
"49cf1260e30300c6362cff8b49441152",
"fad7c7ba0c78fa361b7d5ef66f5bef2b",
"0"
},
{//L_SKIA_JPGD_0047
"I_000272_2MP_1600x1200_seq_rgb32_resized_25.raw",
"aef0ab42e171377acc31eacb48d6803a",
"06bacc6b6ed21e676937cfded3356ce5",
"0"
},
{//L_SKIA_JPGD_0048
"I_000268_5MP_2560x1920_prog_rgb32_resized_25.raw",
"3eca611fb03281c2a5e01be3a5ce4cad",
"21453096d209cfb5eb2d56be8f5499d0",
"0"
},
{//L_SKIA_JPGD_0049
"I_000261_8MP_internet_seq_4000x2048_rgb32_resized_12.raw",
"812711998628bbd130c16bdb1446931a",
"cd54955716e2d58afc29b7b1656b3aaf",
"0"
},
{ //L_SKIA_JPGD_0050
"I_000265_12MP_4000x3000_seq_rgb32_resized_12.raw",
"d7f73657f3f8ec9823d8eb9be68fb6ef",
"e8c62691fe01498927f9e0fe2db2b866",
"0"
},
{ //L_SKIA_JPGD_0051-001
"4X4_rgb16_stress-1.raw",
"2b9acee0619b37dcb71c162af547a511",
"6777f86ae4b429920f8d07bb0c287528",
"0",
},
{ //L_SKIA_JPGD_0051-002
"img01_94x80_rgb16-stress-2.raw",
"b98c4f3160ac37c9ee49c72923e78ae5",
"22d0747166587e44f50240a13600a02e",
"0",
},
{ //L_SKIA_JPGD_0051-003
"I_000278_QCIF176X144_prog_rgb16-stress-3.raw",
"87f8b887a6c6613dd70100cb7e555557",
"fb527cf55d387dd14d554af9707ed676",
"0",
},
{//L_SKIA_JPGD_0051-004
"img_200x200_rgb16-stress-4.raw",
"a77a7c5e396266ea30b3940b93c7deb8",
"5fcf5d522c24433bbc69d373061dc2ce",
"0",
},
{//L_SKIA_JPGD_0051-005
"I_000137_VGA_0.3MP_480x640_rgb16-stress-5.raw",
"8bf35cca44ef8ba2b7303741c5de729c",
"5732bc27a98ce6db32c363638b2311e7",
"0",
},
{//L_SKIA_JPGD_0051-006
"shrek_D1_720x480_rgb16-stress-6.raw",
"dd957a6adb17187f8df2bafa4e19d79e",
"4bbd5d406cc08c1c80ea8d444c0b3525",
"0",
},
{//L_SKIA_JPGD_0051-007
"patzcuaro_800x600_rgb16-stress-7.raw",
"edb0d6cc412f3d33375b1bb309547387",
"3c1bbbf46c8e557492ecb19e422fd1f0",
"0",
},
{//L_SKIA_JPGD_0051-008
"I_000263_2MP_1600x1200_prog_rgb16-stress-8.raw",
"a0b7023480da14df9fa51890c74bbbd7",
"02509a23fe0d09fd21f69182212daf83",
"0",
},
{//L_SKIA_JPGD_0051-009
"I_000259_5MP_internet_seq_2560x1920_rgb16-stress-9.raw",
"2c7a8bc37900d86b3fb9d5af6de728c8",
"cd7a903ddd93e9be41b79334249a7fc4",
"0",
},
{ //L_SKIA_JPGD_0051-010
"I_000265_12MP_4000x3000_seq_rgb16-stress-10.raw",
"63e741c4fc71ad4d00b54908afffd599",
"13218edcbff4fc86624fcf70b9d27ed0",
"0",
},
{ //L_SKIA_JPGD_0057-[1]
"I_000272_2MP_1600x1200_seq_rgb16-parallel-1.raw",
"6c7a9ece7efb324195a9b050fd978f2a",
"1923760e05c6e1392bf865f01d1403e8",
"0",
},
{ //L_SKIA_JPGD_0057-[2]
"I_000263_2MP_1600x1200_prog_rgb16-parallel-2.raw",
"a0b7023480da14df9fa51890c74bbbd7",
"02509a23fe0d09fd21f69182212daf83",
"0",
},
{ //L_SKIA_JPGD_0058-[1]
"I_000268_5MP_2560x1920_prog_rgb16-parallel-1.raw",
"23f56c22eee9058c05a11f1116903576",
"18fa305c19b9628450789ae6858b00d7",
"0",
},
{ //L_SKIA_JPGD_0058-[2]
"shrek_D1_720x480_rgb16-parallel-2.raw",
"dd957a6adb17187f8df2bafa4e19d79e",
"4bbd5d406cc08c1c80ea8d444c0b3525",
"0",
},
{ //L_SKIA_JPGD_0059-[1]
"I_000272_2MP_1600x1200_seq_rgb16-p1.raw",
"6c7a9ece7efb324195a9b050fd978f2a",
"1923760e05c6e1392bf865f01d1403e8",
"0",
},
{ //L_SKIA_JPGD_0059-[2]
"patzcuaro_800x600_rgb16-p2.raw",
"edb0d6cc412f3d33375b1bb309547387",
"3c1bbbf46c8e557492ecb19e422fd1f0",
"0",
},
{ //L_SKIA_JPGD_0059-[3]
"I_000245_XGA_0.8MP_768x1024_rgb16-p3.raw",
"c368f85091e6519d14695300bc2ea885",
"1c7aea5781e525b6a403145f00d52e19",
"0",
},

{   /*add the new test output filename and its md5sum before this comment line*/
"",
"",
"",
""
} /* this NULL string is mandatory */

}, /* end of st_jpgd_file_list[] initialization */
st_jpge_file_list[] = {

{ //L_SKIA_JPGE_0001
"4X4.jpg",
"4299d5c08c42607e7c1fd6438576a5c1",
"a7f5755e8f100b58fa033f1534eeac2c",
"0"
},
{//L_SKIA_JPGE_0002
"img_70x70.jpg",
"1bf0a6396523829b10449721ef09d24c",
"cf8fbb9b82893eb33756f3aaff5627dc",
"0"
},
{//L_SKIA_JPGE_0003
"img01_94x80.jpg",
"1e4b82bd5b804a85283efc0e33bded48",
"602a8d255f3a118241e7b26d81090d8e",
"0"
},
{//L_SKIA_JPGE_0004
"I_000284_SQCIF128x96_seq.jpg",
"0ab8cdd2b75552166dc2068555471698",
"18a1603132469e834f59be988992db11",
"0"
},
{ //L_SKIA_JPGE_0005
"I_000281_QQVGA160X120_seq.jpg",
"344562e64ccfe598d0349b18b38eab0c",
"adfa333b3187b8a4aa8bc346f6d1a49e",
"0"
},
{//L_SKIA_JPGE_0006
"I_000279_QCIF176X144_seq.jpg",
"17232de96f4fcd0c9058f65e65c7db8a",
"eb95aeed520e96fcaeaa3b27625df2a3",
"0"
},
{//L_SKIA_JPGE_0007
"img_200x200.jpg",
"2a199bdc7d0ee4df06debad860db0c6b",
"4bc543d09728816a0704e5db0bcd2117",
"0"
},
{//L_SKIA_JPGE_0008
"I_000276_CGA320x200_seq.jpg",
"048f8849a1943e98215d9c96dc149664",
"838a6ce22ee21c37f2733be3ecb2f948",
"0"
},
{//L_SKIA_JPGE_0009
"laugh_QVGA320x240_seq.jpg",
"ab7be68b39f4261250c44b73be227617",
"dc5e6d3d6ae1e8246f6d248fba2659de",
"0"
},
{ //L_SKIA_JPGE_0010
"I_000137_VGA_0.3MP_480x640.jpg",
"5425dd28fab7423d18c4e486b3a0e41f",
"19e9b604e1b009be68cba176a2b0c325",
"0"
},
{//L_SKIA_JPGE_0011
"I_000066_SVGA_0.5MP_600x800.jpg",
"31fd54d1327e4c26ad5497499940166b",
"1884d4d6c7dbdf43221de52e87ef91e6",
"0"
},
{//L_SKIA_JPGE_0012
"shrek_D1_720x480.jpg",
"98087d8ba35373752c94a494741aadf7",
"1e56fd872d2a68aa729fe9d6bcada8bb",
"0"
},
{//L_SKIA_JPGE_0013
"I_000245_XGA_0.8MP_768x1024.jpg",
"7ce98aaf39d6ad1b7e998d2a491cae43",
"23367ee165220c3f9bff632abddbccb6",
"0"
},
{//L_SKIA_JPGE_0014
"patzcuaro_800x600.jpg",
"a9eb435982d171ece1d86510b861396c",
"be9087c9b94ca96422cb6662ac5ab1b1",
"0"
},
{ //L_SKIA_JPGE_0015
"I_000272_2MP_1600x1200_seq.jpg",
"0c8a29c7db4f0c162fc8a05bfffeb7c0",
"695ab8a89dabb69dcf1bcef0191a72ec",
"0"
},
{//L_SKIA_JPGE_0016
"I_000259_5MP_internet_seq_2560x1920.jpg",
"1f1a0245adf618085ad6060586b20bcd",
"1853e38904704700aa6b8a53ca9373f8",
"0"
},
{//L_SKIA_JPGE_0017
"6MP_3072x2048_internet_seq.jpg",
"eebd6cd63598210f87cdbf72cef73101",
"9a6455f987eddb487fedf78d9702f3c6",
"0"
},
{//L_SKIA_JPGE_0018
"I_000261_8MP_internet_seq_4000x2048.jpg",
"2bc266d3381d10ab990b77199734e7b0",
"3d84983d70b445f802a33243d9b27efd",
"0"
},
{//L_SKIA_JPGE_0019
"I_000265_12MP_4000x3000_seq.jpg",
"556cfa8880f7079586c1027379d026f3",
"ea42862e003b4c531db37e5102d5e154",
"0"
},
{ //L_SKIA_JPGE_0020
"4X4-2.jpg",
"a04a1d9083202c90a54b93e75a9ede1d",
"31f2f2d376478482539cd61eea430d6b",
"0"
},
{//L_SKIA_JPGE_0021
"I_000283_SQCIF128x96_prog-2.jpg",
"93a88994419efec109c0dc9554c37deb",
"5b55816ff724a63d990c1a84e7b87685",
"0"
},
{//L_SKIA_JPGE_0022
"I_000276_CGA320x200_seq-2.jpg",
"7b7161a1541ac7eed0934d4b95c3308e",
"58702737601343ffe282a6d6df60c208",
"0"
},
{//L_SKIA_JPGE_0023
"I_000137_VGA_0.3MP_480x640-2.jpg",
"500462c7aa1feb32fa1d7885bde8cd50",
"e85eb323426dba72c61b7bd89b47ea9a",
"0"
},
{//L_SKIA_JPGE_0024
"patzcuaro_800x600-2.jpg",
"9c4a593b6f38e803627aced4948cd5d0",
"a4ffea527828d4bab3781d259b0f0fe4",
"0"
},
{ //L_SKIA_JPGE_0025
"I_000245_XGA_0.8MP_768x1024-2.jpg",
"aa7feee54fe999f78e62dbe54da451fb",
"09dba3c7512a33657aed5eb9b8e846bf",
"0"
},
{//L_SKIA_JPGE_0026
"I_000263_2MP_1600x1200_prog-2.jpg",
"8a8c2f2b4ad6deab6c3ca08322e7acf5",
"aded3782d1abe1a384bf631bdef11884",
"0"
},
{//L_SKIA_JPGE_0027
"I_000259_5MP_internet_seq_2560x1920-2.jpg",
"f1c8f20b0750cc21777691a22f01d720",
"391493535a38ea90be96e947253eaede",
"0"
},
{//L_SKIA_JPGE_0028
"I_000264_8MP_3264x2448_43_ratio_prog-2.jpg",
"50f7013fcc363c0f8ea881c4affe4fc3",
"36be4bec60e2a6d5d42204ba52f8b902",
"0"
},
{//L_SKIA_JPGE_0029
"I_000265_12MP_4000x3000_seq-2.jpg",
"83c00357b4e2169177585061dced6745",
"11acc114bcb0593ddb8333d2b6f3b3cc",
"0"
},
{ //L_SKIA_JPGE_0030
"patzcuaro_800x600-qf80.jpg",
"946a604ed5c8f49098883ab147cdf65a",
"cd95b449430d7006e3e34dbf925bc82c",
"0"
},
{//L_SKIA_JPGE_0031
"I_000272_2MP_1600x1200_seq-qf70.jpg",
"16218578e3ed51fcdcd6f9b7f2280f0a",
"68ca71153cdd0f5538b23180db23568f",
"0"
},
{//L_SKIA_JPGE_0032
"I_000272_2MP_1600x1200_seq-qf60.jpg",
"8aecb09293d9e06f1f046f55276e6f91",
"e426ea5b8b6e58c555607abe40f19190",
"0"
},
{//L_SKIA_JPGE_0033
"I_000272_2MP_1600x1200_seq-qf50.jpg",
"58c1ca6235a32131cae4edff0645cd37",
"de7258f426a283846c367192fcac7fbf",
"0"
},
{//L_SKIA_JPGE_0034
"I_000259_5MP_internet_seq_2560x1920-qf50.jpg",
"39ee3be4a1ea587d302b75f09f67492c",
"b4dffe04ac35dbf162a30952a30cf928",
"0"
},
{ //L_SKIA_JPGE_0035
"I_000259_5MP_internet_seq_2560x1920-qf40.jpg",
"c9956b26a90e0011125e1b7e8041788b",
"e7c434f216f94743b854b463a928fc1a",
"0"
},
{//L_SKIA_JPGE_0036
"I_000259_5MP_internet_seq_2560x1920-qf30.jpg",
"45e69f911299b4260acc8e174d83cac9",
"32d21b32c16fefe540bb19febbde8db4",
"0"
},
{//L_SKIA_JPGE_0037
"I_000259_5MP_internet_seq_2560x1920-qf20.jpg",
"ddf0895ab91ca23f3b0795ff3461cf44",
"37c1090c32724434fba0c728c8b7f41d",
"0"
},
{//L_SKIA_JPGE_0038
"I_000259_5MP_internet_seq_2560x1920-qf10.jpg",
"06d91aa5b8d6d74365d38fb7fa78e7f6",
"7a2d0b35b5ffbf1ddc6ddce1619712cd",
"0"
},
{//L_SKIA_JPGE_0039
"I_000066_SVGA_0.5MP_600x800_Corrupted_data.jpg",
"6eaddad911310f43fceb344161e9a16d",
"f7d8e3c43dc6bbb056406d2365fabacb",
"0"
},
{ //L_SKIA_JPGE_0041-001
"img01_94x80-stress-1.jpg",
"1e4b82bd5b804a85283efc0e33bded48",
"602a8d255f3a118241e7b26d81090d8e",
"0",
},
{ //L_SKIA_JPGE_0041-002
"img_200x200-stress-2.jpg",
"2a199bdc7d0ee4df06debad860db0c6b",
"4bc543d09728816a0704e5db0bcd2117",
"0",
},
{ //L_SKIA_JPGE_0041-003
"I_000137_VGA_0.3MP_480x640-stress-3.jpg",
"5425dd28fab7423d18c4e486b3a0e41f",
"19e9b604e1b009be68cba176a2b0c325",
"0",
},
{ //L_SKIA_JPGE_0041-004
"patzcuaro_800x600-stress-4.jpg",
"a9eb435982d171ece1d86510b861396c",
"be9087c9b94ca96422cb6662ac5ab1b1",
"0",
},
{ //L_SKIA_JPGE_0041-005
"I_000272_2MP_1600x1200_seq-stress-5.jpg",
"0c8a29c7db4f0c162fc8a05bfffeb7c0",
"695ab8a89dabb69dcf1bcef0191a72ec",
"0",
},
{ //L_SKIA_JPGE_0041-006
"I_000259_5MP_internet_seq_2560x1920-stress-6.jpg",
"1f1a0245adf618085ad6060586b20bcd",
"1853e38904704700aa6b8a53ca9373f8",
"0",
},
{ //L_SKIA_JPGE_0041-007
"I_000261_8MP_internet_seq_4000x2048-stress-7.jpg",
"2bc266d3381d10ab990b77199734e7b0",
"3d84983d70b445f802a33243d9b27efd",
"0",
},
{ //L_SKIA_JPGE_0041-008
"I_000265_12MP_4000x3000_seq-stress-8.jpg",
"556cfa8880f7079586c1027379d026f3",
"ea42862e003b4c531db37e5102d5e154",
"0",
},
{ //L_SKIA_JPGE_0041-009
"4X4-stress-9.jpg",
"4299d5c08c42607e7c1fd6438576a5c1",
"a7f5755e8f100b58fa033f1534eeac2c",
"0",
},
{ //L_SKIA_JPGE_0041-010
"I_000066_SVGA_0.5MP_600x800-stress-10.jpg",
"31fd54d1327e4c26ad5497499940166b",
"1884d4d6c7dbdf43221de52e87ef91e6",
"0",
},
{ //L_SKIA_JPGE_0042-[1]
"I_000259_5MP_internet_seq_2560x1920-parallel-1.jpg",
"1f1a0245adf618085ad6060586b20bcd",
"1853e38904704700aa6b8a53ca9373f8",
"0",
},
{ //L_SKIA_JPGE_0042-[2]
"I_000259_5MP_internet_seq_2560x1920-parallel-2.jpg",
"1f1a0245adf618085ad6060586b20bcd",
"1853e38904704700aa6b8a53ca9373f8",
"0",
},
{ //L_SKIA_JPGE_0043-[1]
"I_000261_8MP_internet_seq_4000x2048-parallel-1.jpg",
"2bc266d3381d10ab990b77199734e7b0",
"3d84983d70b445f802a33243d9b27efd",
"0",
},
{ //L_SKIA_JPGE_0043-[2]
"patzcuaro_800x600-parallel-2.jpg",
"a9eb435982d171ece1d86510b861396c",
"be9087c9b94ca96422cb6662ac5ab1b1",
"0",
},
{ //L_SKIA_JPGE_0044-[1]
"patzcuaro_800x600-p1.jpg",
"a9eb435982d171ece1d86510b861396c",
"be9087c9b94ca96422cb6662ac5ab1b1",
"0",
},
{ //L_SKIA_JPGE_0044-[2]
"I_000245_XGA_0.8MP_768x1024-p2.jpg",
"7ce98aaf39d6ad1b7e998d2a491cae43",
"23367ee165220c3f9bff632abddbccb6",
"0",
},
{ // L_SKIA_JPGE_0044-[3]
"shrek_D1_720x480-p3.jpg",
"98087d8ba35373752c94a494741aadf7",
"1e56fd872d2a68aa729fe9d6bcada8bb",
"0",
},

{   /*add the new test output filename and its md5sum before this comment line*/
"",
"",
"",
""
} /* this NULL string is mandatory */

}; /* End of st_jpge_file_list[] initialization */





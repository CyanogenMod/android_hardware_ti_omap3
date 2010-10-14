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

#ifndef TARGET_OMAP4
#include "SkImageDecoder_libtijpeg_entry.h"
#endif

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

typedef struct JpegDecoderParams
{
    // Quatization Table
    // Huffman Table
    // SectionDecode;

    // SubRegionDecode
    U32 nXOrg;         /* X origin*/
    U32 nYOrg;         /* Y origin*/
    U32 nXLength;      /* X length*/
    U32 nYLength;      /* Y length*/

}JpegDecoderParams;

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
"6777f86ae4b429920f8d07bb0c287528",
"6777f86ae4b429920f8d07bb0c287528",
"0"
},
{//L_SKIA_JPGD_0002
"img_70x70_rgb16.raw",
"b41e849ecd2ed8ff261e079b14edaadd",
"4b055c105bbb139f56626f4c92046e72",
"0"
},
{//L_SKIA_JPGD_0003
"img01_94x80_rgb16.raw",
"0a211cdf7b281c447a895b4381877454",
"22d0747166587e44f50240a13600a02e",
"0"
},
{//L_SKIA_JPGD_0004
"I_000283_SQCIF128x96_prog_rgb16.raw",
"6fd98d2f30214f58d3f348a0c1d8cd19",
"b6addcf589c41da5d99ddb6529b1e114",
"0"
},
{ //L_SKIA_JPGD_0005
"I_000284_SQCIF128x96_seq_rgb16.raw",
"e33310d24c0346d2f02ee578b2b55192",
"fac7a4514225319acd70652ec94a0311",
"0"
},
{//L_SKIA_JPGD_0006
"I_000280_QQVGA160x120_prog_rgb16.raw",
"fd5ff4478e07db288638d037a06971d8",
"13a09115b8b86ac8a7a1abe308cef60f",
"0"
},
{//L_SKIA_JPGD_0007
"I_000281_QQVGA160X120_seq_rgb16.raw",
"0f3f7294516432c6b970ae2a5c0df803",
"333919ec855413d597aad945a53ead56",
"0"
},
{//L_SKIA_JPGD_0008
"I_000278_QCIF176X144_prog_rgb16.raw",
"5036175853051c3346efa1e3da057604",
"fb527cf55d387dd14d554af9707ed676",
"0"
},
{//L_SKIA_JPGD_0009
"I_000279_QCIF176X144_seq_rgb16.raw",
"2ceb1dc6268668efbb0fe02d6ce4c947",
"f16bd94c374209476bdf01227da0c3b6",
"0"
},
{ //L_SKIA_JPGD_0010
"img_200x200_rgb16.raw",
"65428e14a0fe5d8c294a1a4be5add094",
"5fcf5d522c24433bbc69d373061dc2ce",
"0"
},
{//L_SKIA_JPGD_0011
"I_000275_CGA320x200_prog_rgb16.raw",
"04450826d62b51183679ec4c0c54c06f",
"cef9dac3fbfa4a9da52d653663f42d7d",
"0"
},
{//L_SKIA_JPGD_0012
"I_000276_CGA320x200_seq_rgb16.raw",
"4efff8e7e00d2f93310961451a6391bb",
"aa93d1d18890dd12f0cdbb4a71f9d043",
"0"
},
{//L_SKIA_JPGD_0013
"I_000282_QVGA320X240_prog_rgb16.raw",
"4f1dd89d631e803664412797b62907b4",
"088cdd4b2caaeea763c5c4f1205c91ad",
"0"
},
{//L_SKIA_JPGD_0014
"laugh_QVGA320x240_seq_rgb16.raw",
"62c970262d4336acb9a57421b7b543af",
"429af3044e0d902f875a6e7df5bb9a00",
"0"
},
{ //L_SKIA_JPGD_0015
"I_000137_VGA_0.3MP_480x640_rgb16.raw",
"0fc1de7a31928eec5bafc61d73f59e6d",
"5732bc27a98ce6db32c363638b2311e7",
"0"
},
{//L_SKIA_JPGD_0016
"I_000066_SVGA_0.5MP_600x800_rgb16.raw",
"3b69b279c7d39ce910b2fa66edb7233a",
"9ff991c346123198ed7faea71e87376f",
"0"
},
{//L_SKIA_JPGD_0017
"shrek_D1_720x480_rgb16.raw",
"d020595f0fd23768aa51c887f5191e05",
"4bbd5d406cc08c1c80ea8d444c0b3525",
"0"
},
{//L_SKIA_JPGD_0018
"I_000245_XGA_0.8MP_768x1024_rgb16.raw",
"6e4f85ac628cda55a7d33ec3e7f1e204",
"1c7aea5781e525b6a403145f00d52e19",
"0"
},
{//L_SKIA_JPGD_0019
"patzcuaro_800x600_rgb16.raw",
"bae77685fcdd89c4432e273a192a3274",
"3c1bbbf46c8e557492ecb19e422fd1f0",
"0"
},
{ //L_SKIA_JPGD_0020
"I_000263_2MP_1600x1200_prog_rgb16.raw",
"0f5512ae7021d1a053a3295e1d40c8f4",
"02509a23fe0d09fd21f69182212daf83",
"0"
},
{//L_SKIA_JPGD_0021
"I_000272_2MP_1600x1200_seq_rgb16.raw",
"659ff432d95820fb63d0cac6ef4908d8",
"1923760e05c6e1392bf865f01d1403e8",
"0"
},
{//L_SKIA_JPGD_0022
"I_000268_5MP_2560x1920_prog_rgb16.raw",
"6947c3cf180fb49f7a1d4b117f436698",
"18fa305c19b9628450789ae6858b00d7",
"0"
},
{//L_SKIA_JPGD_0023
"I_000259_5MP_internet_seq_2560x1920_rgb16.raw",
"3166a37e2f5df351cbb9946deca9cdc8",
"cd7a903ddd93e9be41b79334249a7fc4",
"0"
},
{//L_SKIA_JPGD_0024
"6MP_3072x2048_internet_seq_rgb16.raw",
"4a8bc28de1123e140358698685aab59b",
"1facb45e2b83b07d6e652b467b98326a",
"0"
},
{ //L_SKIA_JPGD_0025
"I_000264_8MP_3264x2448_43_ratio_prog_rgb16.raw",
"f00bf8f65a0b9a5c8d3814b6dfa3f9a8",
"93a985fec9de9ea97e4c4133e679f704",
"0"
},
{//L_SKIA_JPGD_0026
"I_000261_8MP_internet_seq_4000x2048_rgb16.raw",
"13cf12b26fd7bd6f38389d1dd8796646",
"c63121316e64a7dddb4f63e567455f9f",
"0"
},
{//L_SKIA_JPGD_0027
"I_000266_12MP_4000x3000_prog_rgb16.raw",
"130e680f82f0e160f0a371218f07fb72",
"024ab26e63cb594f738f0b0a02b45101",
"0"
},
{//L_SKIA_JPGD_0028
"I_000265_12MP_4000x3000_seq_rgb16.raw",
"dde80504cf70ac28ae48362ac304832c",
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
"6661f9611152ee04d8f04af62f061079",
"77454844fc4e2d449906ebf48c919bde",
"0"
},
{ //L_SKIA_JPGD_0040
"I_000263_2MP_1600x1200_prog_rgb16_resized_50.raw",
"8e433a140440ffda09aa5500d92f8abf",
"36a4d22709578fc341eee9fcae92cf7a",
"0"
},
{//L_SKIA_JPGD_0041
"I_000272_2MP_1600x1200_seq_rgb16_resized_25.raw",
"3d583cc58126604331b7bc94b27867a7",
"920ed0fb130eafab849ec9754cad066f",
"0"
},
{//L_SKIA_JPGD_0042
"I_000268_5MP_2560x1920_prog_rgb16_resized_25.raw",
"8a52d78d946db2848e9c5f3c3cf9c3af",
"5372d22fda640313fffffcfdce42b3d8",
"0"
},
{//L_SKIA_JPGD_0043
"I_000261_8MP_internet_seq_4000x2048_rgb16_resized_12.raw",
"d7d77dcc0b5060633331a33b02dc0ac3",
"9c86917230195c7694a4b24c9b0c7174",
"0"
},
{//L_SKIA_JPGD_0044
"I_000265_12MP_4000x3000_seq_rgb16_resized_12.raw",
"9315b24755004953dc0377ab43fa160d",
"2757a17da91891646484f85d60613cc2",
"0"
},
{ //L_SKIA_JPGD_0045
"patzcuaro_800x600_rgb32_resized_50.raw",
"1c592c12a386af495589653a4b2498c4",
"24bda07f57822232e88ffae9e2901b1a",
"0"
},
{//L_SKIA_JPGD_0046
"I_000263_2MP_1600x1200_prog_rgb32_resized_50.raw",
"63ea2f9f336c5d29bd8e6c76d807c0c8",
"fad7c7ba0c78fa361b7d5ef66f5bef2b",
"0"
},
{//L_SKIA_JPGD_0047
"I_000272_2MP_1600x1200_seq_rgb32_resized_25.raw",
"1e584c398e5db31d8c25a4e86aec4ffb",
"06bacc6b6ed21e676937cfded3356ce5",
"0"
},
{//L_SKIA_JPGD_0048
"I_000268_5MP_2560x1920_prog_rgb32_resized_25.raw",
"21165710b87baa80f73b19cbcc693370",
"21453096d209cfb5eb2d56be8f5499d0",
"0"
},
{//L_SKIA_JPGD_0049
"I_000261_8MP_internet_seq_4000x2048_rgb32_resized_12.raw",
"d76a5b77002aa1cf4058c7008cf2841d",
"cd54955716e2d58afc29b7b1656b3aaf",
"0"
},
{ //L_SKIA_JPGD_0050
"I_000265_12MP_4000x3000_seq_rgb32_resized_12.raw",
"9ef2c382ad2d7e6f9a450dc1d64efde6",
"e8c62691fe01498927f9e0fe2db2b866",
"0"
},
{ //L_SKIA_JPGD_0051-001
"4X4_rgb16_stress-1.raw",
"6777f86ae4b429920f8d07bb0c287528",
"6777f86ae4b429920f8d07bb0c287528",
"0",
},
{ //L_SKIA_JPGD_0051-002
"img01_94x80_rgb16-stress-2.raw",
"0a211cdf7b281c447a895b4381877454",
"22d0747166587e44f50240a13600a02e",
"0",
},
{ //L_SKIA_JPGD_0051-003
"I_000278_QCIF176X144_prog_rgb16-stress-3.raw",
"5036175853051c3346efa1e3da057604",
"fb527cf55d387dd14d554af9707ed676",
"0",
},
{//L_SKIA_JPGD_0051-004
"img_200x200_rgb16-stress-4.raw",
"65428e14a0fe5d8c294a1a4be5add094",
"5fcf5d522c24433bbc69d373061dc2ce",
"0",
},
{//L_SKIA_JPGD_0051-005
"I_000137_VGA_0.3MP_480x640_rgb16-stress-5.raw",
"0fc1de7a31928eec5bafc61d73f59e6d",
"5732bc27a98ce6db32c363638b2311e7",
"0",
},
{//L_SKIA_JPGD_0051-006
"shrek_D1_720x480_rgb16-stress-6.raw",
"d020595f0fd23768aa51c887f5191e05",
"4bbd5d406cc08c1c80ea8d444c0b3525",
"0",
},
{//L_SKIA_JPGD_0051-007
"patzcuaro_800x600_rgb16-stress-7.raw",
"bae77685fcdd89c4432e273a192a3274",
"3c1bbbf46c8e557492ecb19e422fd1f0",
"0",
},
{//L_SKIA_JPGD_0051-008
"I_000263_2MP_1600x1200_prog_rgb16-stress-8.raw",
"0f5512ae7021d1a053a3295e1d40c8f4",
"02509a23fe0d09fd21f69182212daf83",
"0",
},
{//L_SKIA_JPGD_0051-009
"I_000259_5MP_internet_seq_2560x1920_rgb16-stress-9.raw",
"3166a37e2f5df351cbb9946deca9cdc8",
"cd7a903ddd93e9be41b79334249a7fc4",
"0",
},
{ //L_SKIA_JPGD_0051-010
"I_000265_12MP_4000x3000_seq_rgb16-stress-10.raw",
"dde80504cf70ac28ae48362ac304832c",
"13218edcbff4fc86624fcf70b9d27ed0",
"0",
},
{ //L_SKIA_JPGD_0057-[1]
"I_000272_2MP_1600x1200_seq_rgb16-parallel-1.raw",
"659ff432d95820fb63d0cac6ef4908d8",
"1923760e05c6e1392bf865f01d1403e8",
"0",
},
{ //L_SKIA_JPGD_0057-[2]
"I_000263_2MP_1600x1200_prog_rgb16-parallel-2.raw",
"0f5512ae7021d1a053a3295e1d40c8f4",
"02509a23fe0d09fd21f69182212daf83",
"0",
},
{ //L_SKIA_JPGD_0058-[1]
"I_000268_5MP_2560x1920_prog_rgb16-parallel-1.raw",
"6947c3cf180fb49f7a1d4b117f436698",
"18fa305c19b9628450789ae6858b00d7",
"0",
},
{ //L_SKIA_JPGD_0058-[2]
"shrek_D1_720x480_rgb16-parallel-2.raw",
"d020595f0fd23768aa51c887f5191e05",
"4bbd5d406cc08c1c80ea8d444c0b3525",
"0",
},
{ //L_SKIA_JPGD_0059-[1]
"I_000272_2MP_1600x1200_seq_rgb16-p1.raw",
"659ff432d95820fb63d0cac6ef4908d8",
"1923760e05c6e1392bf865f01d1403e8",
"0",
},
{ //L_SKIA_JPGD_0059-[2]
"patzcuaro_800x600_rgb16-p2.raw",
"bae77685fcdd89c4432e273a192a3274",
"3c1bbbf46c8e557492ecb19e422fd1f0",
"0",
},
{ //L_SKIA_JPGD_0059-[3]
"I_000245_XGA_0.8MP_768x1024_rgb16-p3.raw",
"6e4f85ac628cda55a7d33ec3e7f1e204",
"1c7aea5781e525b6a403145f00d52e19",
"0",
},
//Odd resolution image decoding
{ //L_SKIA_JPGD_0063
"img_odd_res_199x200_rgb16.raw",
"794ed08ea8676cb6d0fc348c8ef532cd",
"12ca99ee457adb1e0361f096782f2c71",
"0",
},
{ //L_SKIA_JPGD_0064
"img_odd_res_200x199_rgb16.raw",
"3c171bc497f79c0d15a20400710d0baf",
"ec11aa02fa94f44186568cf040db48d3",
"0",
},
{ //L_SKIA_JPGD_0065
"img_odd_res_199x199_rgb16.raw",
"2ff1c1b4a041a4d38d80205876b6f025",
"53a9e97c99727de6733a80ad2b556fac",
"0",
},
{ //L_SKIA_JPGD_0066
"img_odd_res_767x1024_rgb16.raw",
"7ec0ee596b2e6fbc008117e1bfd1bb4f",
"490c8cd6f3813a3aac56e4c0a9e0fef8",
"0",
},
{ //L_SKIA_JPGD_0067
"img_odd_res_768x1023_rgb16.raw",
"16d547fb9b5e167f92be085deb8abce8",
"6811ba1bf39b953858ec215ae7bc1334",
"0",
},
{ //L_SKIA_JPGD_0068
"img_odd_res_767x1023_rgb16.raw",
"b81489462a83a4d361cbd8df79423336",
"a976e1aa21fc0d0ae46ba11dc806f047",
"0",
},
{ //L_SKIA_JPGD_0069
"img_odd_res_199x200_rgb32.raw",
"9434f7e1ba93d6bb02fda46aaab9c161",
"93aa6faebd44c10b13e8b2b0ad21ec89",
"0",
},
{ //L_SKIA_JPGD_0070
"img_odd_res_200x199_rgb32.raw",
"d90b0f8d7abc6a4959adc710550ea429",
"3c251999e4ed40a097f857b885ecb3d5",
"0",
},
{ //L_SKIA_JPGD_0071
"img_odd_res_199x199_rgb32.raw",
"6a84ed0f62eb98d657399d1aa18f4f73",
"a70f1ee67d0a6bb01242aa67fa561d06",
"0",
},
{ //L_SKIA_JPGD_0072
"img_odd_res_767x1024_rgb32.raw",
"819de71c50b425b1389e3c553a6f6b50",
"80002ce1ef40df0972d9b4455192807c",
"0",
},
{ //L_SKIA_JPGD_0073
"img_odd_res_768x1023_rgb32.raw",
"68ffc17ae5cac1038bb9b5046523fc2e",
"56a47a71b648924900ece4fa727acc36",
"0",
},
{ //L_SKIA_JPGD_0074
"img_odd_res_767x1023_rgb32.raw",
"abc084372dec2a1e6694a23f84b64e97",
"6b617a091f14314e2f475c1789bddbf9",
"0",
},
//Sub-region decoding tests
{ //L_SKIA_JPGD_0081
"I_000066_SVGA_SR_LT_256x256_rgb16.raw",
"0",
"3bc350a02438afd1affb0e4d403118a7",
"0",
},
{ //L_SKIA_JPGD_0082
"I_000066_SVGA_SR_RT_256x256_rgb16.raw",
"0",
"9aec56961b08f857f7c133fbae58ae6a",
"0",
},
{ //L_SKIA_JPGD_0083
"I_000066_SVGA_SR_LB_256x256_rgb16.raw",
"0",
"1492db6dab4ed6cff2276ab9f41d5deb",
"0",
},
{ //L_SKIA_JPGD_0084
"I_000066_SVGA_SR_RB_256x256_rgb16.raw",
"0",
"330e34b246153b4fc8cfdcf3b8951f28",
"0",
},
{ //L_SKIA_JPGD_0085
"I_000066_SVGA_SR_M_256x256_rgb16.raw",
"0",
"a216e47d2428f8ee457787a569f3b5ad",
"0",
},
{ //L_SKIA_JPGD_0086
"I_000272_2MP_SR_LT_256x256_seq_rgb16.raw",
"0",
"342da694feb99e82a3e79802db36696f",
"0",
},
{ //L_SKIA_JPGD_0087
"I_000272_2MP_SR_RT_256x256_seq_rgb16.raw",
"0",
"54ef3d1b7b1a5a12bcfd0f37f9680b18",
"0",
},
{ //L_SKIA_JPGD_0088
"I_000272_2MP_SR_LB_256x256_seq_rgb16.raw",
"0",
"8666b904945c90a287c246dc826e87e3",
"0",
},
{ //L_SKIA_JPGD_0089
"I_000272_2MP_SR_RB_256x256_seq_rgb16.raw",
"0",
"d6aef82f8917fc0d867dcec025686ac6",
"0",
},
{ //L_SKIA_JPGD_0090
"I_000272_2MP_SR_M_256x256_seq_rgb16.raw",
"0",
"4b3a59288d7d666bda9d89864cf8ae28",
"0",
},
{ //L_SKIA_JPGD_0091
"I_000259_5MP_SR_LT_512x512_rgb16.raw",
"0",
"d976cbf524391a306ee9a9b0a3cd7478",
"0",
},
{ //L_SKIA_JPGD_0092
"I_000259_5MP_SR_RT_512x512_rgb16.raw",
"0",
"4b927aaec8a6a2057bf05fff998ffe56",
"0",
},
{ //L_SKIA_JPGD_0093
"I_000259_5MP_SR_LB_512x512_rgb16.raw",
"0",
"41d4e79f2e635cefaaf546f48ce11a41",
"0",
},
{ //L_SKIA_JPGD_0094
"I_000259_5MP_SR_RB_512x512_rgb16.raw",
"0",
"d61c0bdf7fdf94861ed6ab34ceaae7f7",
"0",
},
{ //L_SKIA_JPGD_0095
"I_000259_5MP_SR_M_512x512_rgb16.raw",
"0",
"cea7c673adeab41fae6398e2aadfb863",
"0",
},
{ //L_SKIA_JPGD_0096
"I_000261_8MP_SR_LT_512x512_rgb16.raw",
"0",
"042cee6a54ff9980dc689fa253c72cea",
"0",
},
{ //L_SKIA_JPGD_0097
"I_000261_8MP_SR_RT_512x512_rgb16.raw",
"0",
"b7420037b1d8b336c3013cdd8ece77f3",
"0",
},
{ //L_SKIA_JPGD_0098
"I_000261_8MP_SR_LB_512x512_rgb16.raw",
"0",
"a019c232f57c0eca231d7116c36a7584",
"0",
},
{ //L_SKIA_JPGD_0099
"I_000261_8MP_SR_RB_512x512_rgb16.raw",
"0",
"5d5107a7defeb55a34d5d41f05243887",
"0",
},
{ //L_SKIA_JPGD_0100
"I_000261_8MP_SR_M_512x512_rgb16.raw",
"0",
"437a4c5026a6bc3542718832ade49606",
"0",
},
{ //L_SKIA_JPGD_0101
"I_000265_12MP_SR_LT_1024x1024_rgb16.raw",
"0",
"be67c6c78e90fbc787e2cac77ebbe25b",
"0",
},
{ //L_SKIA_JPGD_0102
"I_000265_12MP_SR_RT_1024x1024_rgb16.raw",
"0",
"b00498ed37e53bde123097864d2ef59a",
"0",
},
{ //L_SKIA_JPGD_0103
"I_000265_12MP_SR_LB_1024x1024_rgb16.raw",
"0",
"078e996e3f2c7fb6928f4bba35135a65",
"0",
},
{ //L_SKIA_JPGD_0104
"I_000265_12MP_SR_RB_1024x1024_rgb16.raw",
"0",
"e04edc98f3107cf620c9375bbdf92431",
"0",
},
{ //L_SKIA_JPGD_0105
"I_000265_12MP_SR_M_1024x1024_rgb16.raw",
"0",
"9fa62caca8b65d3d4c288bdf5c027bc9",
"0",
},
{ //L_SKIA_JPGD_0106
"I_000282_QVGA_SR_LT_128x128_prog_rgb16.raw",
"0",
"b913e4d632adc96fb82468fa7f3a334a",
"0",
},
{ //L_SKIA_JPGD_0107
"I_000282_QVGA_SR_RT_128x128_prog_rgb16.raw",
"0",
"62b5578207fd1f9ed43ca59bd13dcac8",
"0",
},
{ //L_SKIA_JPGD_0108
"I_000282_QVGA_SR_LB_128x128_prog_rgb16.raw",
"0",
"9be1c17e667d76784be3474bda4dc9e4",
"0",
},
{ //L_SKIA_JPGD_0109
"I_000282_QVGA_SR_RB_128x128_prog_rgb16.raw",
"0",
"b4a325f2d06ec36d5b3905d03b181284",
"0",
},
{ //L_SKIA_JPGD_0110
"I_000282_QVGA_SR_M_128x128_prog_rgb16.raw",
"0",
"5c1add95145128102bdfb0dd9f0a04aa",
"0",
},
{ //L_SKIA_JPGD_0111
"I_000263_2MP_SR_LT_256x256_prog_rgb16.raw",
"0",
"b94e5bcf5614390ab1ccc664506b8712",
"0",
},
{ //L_SKIA_JPGD_0112
"I_000263_2MP_SR_RT_256x256_prog_rgb16.raw",
"0",
"c66c1c0f791ac3711fc321d095443886",
"0",
},
{ //L_SKIA_JPGD_0113
"I_000263_2MP_SR_LB_256x256_prog_rgb16.raw",
"0",
"43b9e71941f8f0be7dd98ec1a78661c8",
"0",
},
{ //L_SKIA_JPGD_0114
"I_000263_2MP_SR_RB_256x256_prog_rgb16.raw",
"0",
"d7c7fc8d8ae875a24742da21c5940578",
"0",
},
{ //L_SKIA_JPGD_0115
"I_000263_2MP_SR_M_256x256_prog_rgb16.raw",
"0",
"4719c34eda29dc042440241e73b70256",
"0",
},
{ //L_SKIA_JPGD_0116
"I_000268_5MP_SR_LT_512x512_prog_rgb16.raw",
"0",
"1fb516f45e5e08374887474eeaf4ce87",
"0",
},
{ //L_SKIA_JPGD_0117
"I_000268_5MP_SR_RT_512x512_prog_rgb16.raw",
"0",
"470cad2baab36f8d34f14071808cab2f",
"0",
},
{ //L_SKIA_JPGD_0118
"I_000268_5MP_SR_LB_512x512_prog_rgb16.raw",
"0",
"6f35571ac2c5bb858eb96ba3fdc70ada",
"0",
},
{ //L_SKIA_JPGD_0119
"I_000268_5MP_SR_RB_512x512_prog_rgb16.raw",
"0",
"5b9bfbff7b4d6168dd8a64c20d19fadd",
"0",
},
{ //L_SKIA_JPGD_0120
"I_000268_5MP_SR_M_512x512_prog_rgb16.raw",
"0",
"2be82e24f472910cc4a80418b51ec527",
"0",
},
{ //L_SKIA_JPGD_0121
"I_000264_8MP_SR_LT_512x512_prog_rgb16.raw",
"0",
"a8b67948fb76fe74deae01f2ba86a93a",
"0",
},
{ //L_SKIA_JPGD_0122
"I_000264_8MP_SR_RT_512x512_prog_rgb16.raw",
"0",
"171d123135fd0dfb9f471b41f35a5cec",
"0",
},
{ //L_SKIA_JPGD_0123
"I_000264_8MP_SR_LB_512x512_prog_rgb16.raw",
"0",
"4bc180f031e6b620668cf52a68b53fb2",
"0",
},
{ //L_SKIA_JPGD_0124
"I_000264_8MP_SR_RB_512x512_prog_rgb16.raw",
"0",
"85c5383ffda934b968b5334c09282c4e",
"0",
},
{ //L_SKIA_JPGD_0125
"I_000264_8MP_SR_M_512x512_prog_rgb16.raw",
"0",
"4de5e61c9219375ed6b662e92edef584",
"0",
},
{ //L_SKIA_JPGD_0126
"I_000266_12MP_SR_LT_1024x1024_prog_rgb16.raw",
"0",
"8ee34f20888aa0e417c0bf043f94084c",
"0",
},
{ //L_SKIA_JPGD_0127
"I_000266_12MP_SR_RT_1024x1024_prog_rgb16.raw",
"0",
"76118ff0ac2419fafe714ff377e21aab",
"0",
},
{ //L_SKIA_JPGD_0128
"I_000266_12MP_SR_LB_1024x1024_prog_rgb16.raw",
"0",
"c712390428ddc7f9d4b254a4b20c82d6",
"0",
},
{ //L_SKIA_JPGD_0129
"I_000266_12MP_SR_RB_1024x1024_prog_rgb16.raw",
"0",
"2c78248df2eb1a4514d24726b80de40a",
"0",
},
{ //L_SKIA_JPGD_0130
"I_000266_12MP_SR_M_1024x1024_prog_rgb16.raw",
"0",
"83ec53f65fcbc01eec22bba488e7f3a5",
"0",
},
{
"img_40x40_rgb16.raw",
"6e9fff7a8e60e946b92c7b6c47433f86",
"63139ba7599fe4341dffca68b4d9f1b8",
"0",
},
{
"img_180x160_rgb16.raw",
"f408ea2fcc1b9d7bbcabb6d148145882",
"17d421d84a5ef6273ae9977a2be0a6d1",
"0",
},
{
"img_1MP_1280x800_rgb16.raw",
"d0a292a9787777c88dfcf6b88d85da83",
"fdb8ffb00cb4f365168018377cb5836c",
"0",
},
{
"img_1.3MP_1440x900_rgb16.raw",
"b421db256b6a7ffadd463d51a47ff007",
"94856a4405a1297fcebbdec3db10a3c4",
"0",
},
{
"img_2.3MP_1920x1200_rgb16.raw",
"c4147fdbfff5abf9434ceba01012d106",
"2123de76480e46e5d32b148079d1aa32",
"0",
},
{
"img_3.1MP_2048x1536_rgb16.raw",
"3368ba2cc93cdc1e6ded2da83256005e",
"cfac123a662b5ab620bf70a164485aa5",
"0",
},
{
"img_3.5MP_2048x1800_rgb16.raw",
"40567346e639c35e84ffa7cdc0c7b60e",
"ae5dcfe04d5e84d8ebc04dcef8e19bce",
"0",
},
{
"img_4MP_2560x1600_rgb16.raw",
"39a45d18b4704132cdf6fcc046803635",
"a32cf73ac74dc58562c8682935e5f6f7",
"0",
},
{
"img_4.6MP_2560x1800_rgb16.raw",
"23e4558eb43329712e2f9d5049999466",
"64e7a6ed7853acd63bbfa0355bd45d44",
"0",
},
{
"img_5.5MP_2952x1882_rgb16.raw",
"bc4f4d15bc900fa03fed5c2adff026af",
"92ee346d094b8aeae743ccad18ad9a39",
"0",
},
{
"img_6.6MP_2842x2357_rgb16.raw",
"a41f975466a825fcbcf113717a9f8866",
"0617c159a2d646725eb305c4a918776d",
"0",
},
{
"img_7MP_3072x2304_rgb16.raw",
"b1481275e6d08d0e089c4e94897aa1d6",
"43492efa4c692b1b22391a2cd1193d1d",
"0",
},
{
"img_7.5MP_3474x2154_rgb16.raw",
"fb5a60722e818aa9ee945bea33f2e688",
"d19d81deb16ce2d9c05d0966f67558b9",
"0",
},
{
"img_8.4MP_3300x2550_rgb16.raw",
"ea001f46b80cc88b4a2683c1b90231a5",
"908cd33b6a8d7bde6f4a170838631d3e",
"0",
},
{
"img_9MP_3000x3000_rgb16.raw",
"fb207122f381ec74a37e7f51b93dfdf4",
"f92b8cf6f34bc55a87110a77b8080626",
"0",
},
{
"img_9.6MP_3600x2400_rgb16.raw",
"9caf282b3fea637477eb5006b645d283",
"56d862f49be7e32a4aa1bb0ec850ca00",
"0",
},
{
"img_10MP_3664x2748_rgb16.raw",
"e6a5990cf7b6196aafefb3f676fd9e07",
"5fe98fd2187bc3c66a2e4b1931e25760",
"0",
},
{
"img_10.5MP_3780x2782_rgb16.raw",
"023f347e337352500c8130f9ca10195f",
"15eb944841474f5e643fff5f268bb8bc",
"0",
},
{
"img_11MP_4028x2732_rgb16.raw",
"6def0018f1c8080e1a4bbd67aec75c3f",
"2ecda0f3d20919187f7ef7dd23c29b93",
"0",
},
{
"img_11.5MP_4195x2763_rgb16.raw",
"d8f040b9bebc98198a939afafcf75b85",
"03512217ed67fa02c9ecf426d505982c",
"0",
},
{
"img_40x40_prog_rgb16.raw",
"3e3c3aa0223c14fc9a77c1b4bf3bc7f7",
"32b37b460eeb8178b2f9106e70ecfa12",
"0",
},
{
"img_180x160_prog_rgb16.raw",
"a4c84eb25aeb60c187347acd8773d775",
"f9998e2122402d62a4ad871da59ba078",
"0",
},
{
"img_1MP_1280x800_prog_rgb16.raw",
"45ba70e53ad45e8582ea67024d36d372",
"7cb9d80e3864f7d61ff9255d28aa1b16",
"0",
},
{
"img_1.3MP_1440x900_prog_rgb16.raw",
"7747b9bf929b005d07d03950844bd87c",
"2e1a44665b26561df4689ba80a8611cf",
"0",
},
{
"img_2.3MP_1920x1200_prog_rgb16.raw",
"8df6a3093c63c1683f09f11cb547a0ef",
"1cbcead543fadcde8cdcc188fb340fed",
"0",
},
{
"img_3.1MP_2048x1536_prog_rgb16.raw",
"fc7ba0ef2819464bda8717dd8a5ba9ad",
"40b3b4bb3821d606f10e792fe23af1b7",
"0",
},
{
"img_3.5MP_2048x1800_prog_rgb16.raw",
"1c34fe298f43318e8edf1d1b14b6eb3e",
"cf678552d5bb13d5cacd049c9a2d7b06",
"0",
},
{
"img_4MP_2560x1600_prog_rgb16.raw",
"8d8bb8cc94ce7cd905f4596c4c0b3ce2",
"2d70aa0164f6460f143defb133b7b586",
"0",
},
{
"img_4.6MP_2560x1800_prog_rgb16.raw",
"3730c2bcb3865227f4c4947e2d559ad2",
"d0b35f02f5c3a93273abb666d6efe3f2",
"0",
},
{
"img_5.5MP_2952x1882_prog_rgb16.raw",
"521eddebfe4d9950c8b6c60f4222363b",
"f7ca8638ed903097eda11ee522560ce0",
"0",
},
{
"img_6.6MP_2842x2357_prog_rgb16.raw",
"7a7425491d3c79733b62d2312a9ea14d",
"f11771283329d7645d5d059d3165a238",
"0",
},
{
"img_7MP_3072x2304_prog_rgb16.raw",
"ce35dc347f28562813e24b013837d395",
"d84e92536fe793a45d18cd0aa453e4a2",
"0",
},
{
"img_7.5MP_3474x2154_prog_rgb16.raw",
"433fefa7b3645230e2a835c32ac43a7f",
"61137f2d0ab3eb793e47accd7d9f5bbc",
"0",
},
{
"img_8.4MP_3300x2550_prog_rgb16.raw",
"4684e680fe0ccfd577c7dc26f763b263",
"74ef4244ed653c169ff018d5625d4f53",
"0",
},
{
"img_9MP_3000x3000_prog_rgb16.raw",
"3e127657ac6a86bc3aafbcab5ad8fa82",
"34ab565c1fc50295e2042891e04ba18a",
"0",
},
{
"img_9.6MP_3600x2400_prog_rgb16.raw",
"30300028bb83269d1782d391227f00e0",
"666ae67916133e0b99c4250d2625275e",
"0",
},
{
"img_10MP_3664x2748_prog_rgb16.raw",
"8ff30e8a4a42e2174e44b84b672826b5",
"1ed236a4ff1b9f4ff17a87fcad0afe2d",
"0",
},
{
"img_10.5MP_3780x2782_prog_rgb16.raw",
"cd2842901dbb3fa90efcfb0573a8656e",
"e778a356cbf67b7e89c6bd2e9a5cbff4",
"0",
},
{
"img_11MP_4028x2732_prog_rgb16.raw",
"add56b059e186a71769f6de1d7ef10b9",
"e04ce2755be9ec5d4bb24c822c750cf3",
"0",
},
{
"img_11.5MP_4195x2763_prog_rgb16.raw",
"f336c0484438265ae41c4887f88943a9",
"aa684887a67b414fed278782f0016de6",
"0",
},
{//L_SKIA_JPGD_0172
"img_6.6MP_2842x2357_prog_rgb32_SF_12.raw",
"f19a5e1365bf8284ef97f60d9f46448d",
"e10f8e686d43255baffc5fcccdc120af",
"0",
},
{ // L_SKIA_JPGD_0173:
"img_1MP_1280x800_SF2_rgb32.raw",
"7713330ebf34804a10b371373d9f3d0f",
"5aca25621deaf471cbb861731d83da78",
"0"
},
{ // L_SKIA_JPGD_0174:
"img_1.3MP_1440x900_SF2_rgb32.raw",
"8206e14b0644b26c7ad0076723ef5e18",
"cf8ff7ef85517d576fbd5fbda692caef",
"0"
},
{ // L_SKIA_JPGD_0175:
"img_2.3MP_1920x1200_SF2_rgb32.raw",
"2c0af1df3ea7768519d7c110449981c9",
"79f40193523425e158ae6498f90f57de",
"0"
},
{ // L_SKIA_JPGD_0176:
"img_3.1MP_2048x1536_SF2_rgb32.raw",
"7b825bc9041c1c932ff1a6a1b6e63138",
"01c23cc5ec67caa8bf00eb370202b8d2",
"0"
},
{ // L_SKIA_JPGD_0177:
"img_3.5MP_2048x1800_SF2_rgb32.raw",
"8319abbf0f96060a130e16fa1ed32946",
"a30fb77e3c15b3ca98db90de2d874e91",
"0"
},
{ // L_SKIA_JPGD_0178:
"img_4MP_2560x1600_SF2_rgb32.raw",
"40434d2dcab783a1d10a9c9d75fa096e",
"4014bc62cc595410d1d857fd89f077ae",
"0"
},
{ // L_SKIA_JPGD_0179:
"img_4.6MP_2560x1800_SF2_rgb32.raw",
"7d03aebeac0b7ddf1cee721a9e481c86",
"51399d3c549ac7da358af8fab4679027",
"0"
},
{ // L_SKIA_JPGD_0180:
"img_5.5MP_2952x1882_SF2_rgb32.raw",
"6f0b29f0e4c6f5df7d5502c8c29c2e6c",
"0ccc7a136461e5665032b3ba49b569e4",
"0"
},
{ // L_SKIA_JPGD_0181:
"img_6.6MP_2842x2357_SF2_rgb32.raw",
"ddd8ae81d2407f43e248da0ba908f233",
"df47af347ce2da509c3ec9ff43dccc50",
"0"
},
{ // L_SKIA_JPGD_0182:
"img_7MP_3072x2304_SF2_rgb32.raw",
"8c2f77a98eed33fcc5ad0239f7290343",
"5c2d1fdec6eab435743f6186e33f66e8",
"0"
},
{ // L_SKIA_JPGD_0183:
"img_7.5MP_3474x2154_SF2_rgb32.raw",
"03be867848e682a93cfe97bde2169b1c",
"f2e76e1cf09028fd518bb6d28f2e6596",
"0"
},
{ // L_SKIA_JPGD_0184:
"I_000261_8MP_4000x2048_SF2_rgb32.raw",
"cc3e931b5c59de6fa15b07b5df14d874",
"09f6913f276fa22511c0f9d461b57716",
"0"
},
{ // L_SKIA_JPGD_0185:
"img_8.4MP_3300x2550_SF2_rgb32.raw",
"628fa442b73b6d250bd48b5ff01793b1",
"f794ccbbabb572cac7c821215de03565",
"0"
},
{ // L_SKIA_JPGD_0186:
"img_9MP_3000x3000_SF2_rgb32.raw",
"e03e5606a64b3e750cbad841839670eb",
"a6c5fd3b7b824e90d459352a5ef108b7",
"0"
},
{ // L_SKIA_JPGD_0187:
"img_9.6MP_3600x2400_SF2_rgb32.raw",
"ef2d8fec99d64ba4538fc83dcbfcf3c2",
"f852aab988eee10b67d0ddd55110664c",
"0"
},
{ // L_SKIA_JPGD_0188:
"img_10MP_3664x2748_SF2_rgb32.raw",
"129a9ac3ef2ba964a3132bd22b0645e4",
"5f400b17c4c06641ea92b100e0be7a7a",
"0"
},
{ // L_SKIA_JPGD_0189:
"img_10.5MP_3780x2782_SF2_rgb32.raw",
"229875a7f705e67cae020a421e31abd3",
"92d6eb70eac1c591444ed039413e0002",
"0"
},
{ // L_SKIA_JPGD_0190:
"img_11MP_4028x2732_SF2_rgb32.raw",
"1b9f9bf6405a2ec9916d1e1c80d27307",
"d1f7a3404293bccae65dcdaf5887ed25",
"0"
},
{ // L_SKIA_JPGD_0191:
"img_11.5MP_4195x2763_SF2_rgb32.raw",
"8950673a40513474826bffa3d15763c2",
"306afa493b83175b1740fdc9af11f63c",
"0"
},
{ // L_SKIA_JPGD_0192:
"I_000265_12MP_4000x3000_SF2_rgb32.raw",
"6ed22f04831a459205a3dfb348e34cc4",
"c9388e39723057e6e3bb0232f29bdae4",
"0"
},
{ // L_SKIA_JPGD_0193:
"img_1MP_1280x800_prog_SF2_rgb32.raw",
"91fdc76caf5cd0dde0e9db53eda38ddc",
"4f9688f0bd96fb937bc76bdbbdc5dbd1",
"0"
},
{ // L_SKIA_JPGD_0194:
"img_1.3MP_1440x900_prog_SF2_rgb32.raw",
"051271e08a527cf3b2dee55fe9f5e07b",
"b2ad642905a7e690361d9cffda3c285f",
"0"
},
{ // L_SKIA_JPGD_0195:
"img_2.3MP_1920x1200_prog_SF2_rgb32.raw",
"dc927366db42f045ce2fd275fb95daf8",
"cbb2546dd17639f98b9b8d3ad59331bf",
"0"
},
{ // L_SKIA_JPGD_0196:
"img_3.1MP_2048x1536_prog_SF2_rgb32.raw",
"ff6454b2101ae3f69b42cf751eb80872",
"0125d487645fdc6534b2e688758a4d98",
"0"
},
{ // L_SKIA_JPGD_0197:
"img_3.5MP_2048x1800_prog_SF2_rgb32.raw",
"7213328a3a25993e8fea0e328a820732",
"dd65e4c22f7e5d8a41f675161260ab26",
"0"
},
{ // L_SKIA_JPGD_0198:
"img_4MP_2560x1600_prog_SF2_rgb32.raw",
"879020d9ecb282b20d4f7884f307f07f",
"763565ae63901ba5d2fb9b34a1676df9",
"0"
},
{ // L_SKIA_JPGD_0199:
"img_4.6MP_2560x1800_prog_SF2_rgb32.raw",
"5a748339fc069764f8839de10c06d1dc",
"350899a74ecb3e191fb1ecfebf0a72d7",
"0"
},
{ // L_SKIA_JPGD_0200:
"img_5.5MP_2952x1882_prog_SF2_rgb32.raw",
"7687ce6c41d2de7e47248a86f61eec48",
"26a59ef0b34d5f9a2f6de96a3f2d21d1",
"0"
},
{ // L_SKIA_JPGD_0201:
"img_6.6MP_2842x2357_prog_SF2_rgb32.raw",
"2ac5c8ac5685953ff1900e7c5f68fc51",
"622bfe377bc43a465a72f682e82248cc",
"0"
},
{ // L_SKIA_JPGD_0202:
"img_7MP_3072x2304_prog_SF2_rgb32.raw",
"867691a86876d32005aeac5686521855",
"aa3405b3edc58be69ef01c5d07c74ef7",
"0"
},
{ // L_SKIA_JPGD_0203:
"img_7.5MP_3474x2154_prog_SF2_rgb32.raw",
"30c6b338b91db4480a2fe6b17e296357",
"5fc4969f30037e32d852c9d4824c02f8",
"0"
},
{ // L_SKIA_JPGD_0204:
"I_000264_8MP_3264x2448_prog_SF2_rgb32.raw",
"7707a90b4e222cbb255bb9a82946e0fb",
"a3c2773cf15b241bf39cacda3aa241a8",
"0"
},
{ // L_SKIA_JPGD_0205:
"img_8.4MP_3300x2550_prog_SF2_rgb32.raw",
"43f5e66b3f56eb6fb5776fb972accf2f",
"b8e0d0a1aaab291663ecda68b8f1bbcf",
"0"
},
{ // L_SKIA_JPGD_0206:
"img_9MP_3000x3000_prog_SF2_rgb32.raw",
"c19c3192821884defc6d3709638be0fc",
"06f465452f9e0ea3c871dc88532bdf7a",
"0"
},
{ // L_SKIA_JPGD_0207:
"img_9.6MP_3600x2400_prog_SF2_rgb32.raw",
"3662b6882f0e3864111f166c2d30b817",
"6ddac2a18e902500353d8e2e67d7102c",
"0"
},
{ // L_SKIA_JPGD_0208:
"img_10MP_3664x2748_prog_SF2_rgb32.raw",
"f9a827829fc736b260ad0ac64965c5ef",
"25a1d6a6b98e8ccaf0d5c92318967dcc",
"0"
},
{ // L_SKIA_JPGD_0209:
"img_10.5MP_3780x2782_prog_SF2_rgb32.raw",
"01ae268e1796a5ced2094fe100536c23",
"189f2027479f971fbc37ac7a8d97f94d",
"0"
},
{ // L_SKIA_JPGD_0210:
"img_11MP_4028x2732_prog_SF2_rgb32.raw",
"2fd019908486ef5bd973bc6a16c8a196",
"169568584f1e6580e474822f53710f2d",
"0"
},
{ // L_SKIA_JPGD_0211:
"img_11.5MP_4195x2763_prog_SF2_rgb32.raw",
"db67aaff0f27b1048c1bbd74b36f9e6e",
"6aec05cd8119cc4346e549d9c2b57fb9",
"0"
},
{ // L_SKIA_JPGD_0212:
"I_000266_12MP_4000x3000_prog_SF2_rgb32.raw",
"40a086976bfc2f722c9e312b0becb891",
"7a590855d01907ffc46c1fdec12fed66",
"0"
},
{ // L_SKIA_JPGD_0213:
"img_1MP_1280x800_SF4_rgb32.raw",
"c4fdd6a291ceecd8542f4b813d7d9cc3",
"addc9525b7ed043bff481b5634db33db",
"0"
},
{ // L_SKIA_JPGD_0214:
"img_1.3MP_1440x900_SF4_rgb32.raw",
"ad70ddfdf0ed81dd163fe8c0f000f696",
"3940da8aa4d511c18976f475d9fef4f2",
"0"
},
{ // L_SKIA_JPGD_0215:
"img_2.3MP_1920x1200_SF4_rgb32.raw",
"5b7836820e7960ff6113fc3be6b46eca",
"4c00e8aa6f5440e80dfee213caab66e5",
"0"
},
{ // L_SKIA_JPGD_0216:
"img_3.1MP_2048x1536_SF4_rgb32.raw",
"d0eecf1e2cde003a016da809a369f628",
"d9c4e613fc6e9167dc8ad3bbcd811ba0",
"0"
},
{ // L_SKIA_JPGD_0217:
"img_3.5MP_2048x1800_SF4_rgb32.raw",
"94ad070f99af4256af036eddb48b7cc5",
"81b85c11a654830022e5728db2522cd9",
"0"
},
{ // L_SKIA_JPGD_0218:
"img_4MP_2560x1600_SF4_rgb32.raw",
"a4f8ad12c48e98140c2975c67c121785",
"ec9bc0847379ae2f00b12254cd62221e",
"0"
},
{ // L_SKIA_JPGD_0219:
"img_4.6MP_2560x1800_SF4_rgb32.raw",
"2a9acd2bbeeabe99e0090ad874056e67",
"6fc436a59f767325f6cb4ebf34e1acf9",
"0"
},
{ // L_SKIA_JPGD_0220:
"img_5.5MP_2952x1882_SF4_rgb32.raw",
"5627a21a0de54faabe5515a47632a93a",
"2e40dfc269684f64e19559272ed74aa6",
"0"
},
{ // L_SKIA_JPGD_0221:
"img_6.6MP_2842x2357_SF4_rgb32.raw",
"26a621af674b4a7ad1aa781e72b4c21c",
"09fd71a0814dd20c7e98cd6be9fc988c",
"0"
},
{ // L_SKIA_JPGD_0222:
"img_7MP_3072x2304_SF4_rgb32.raw",
"59cb3e201b94498cb9030c59e3ba5158",
"d53583a7de3da9ad6563a4c73a511b56",
"0"
},
{ // L_SKIA_JPGD_0223:
"img_7.5MP_3474x2154_SF4_rgb32.raw",
"1521d850695f20cb0d4505f4014b6403",
"a072bea579129c7bd00533d89eb53503",
"0"
},
{ // L_SKIA_JPGD_0224:
"I_000261_8MP_4000x2048_SF4_rgb32.raw",
"d357e2f00c0afe5fec553a312cf9fde7",
"62a8d8e787ccb5fdebac7eac0faf1a02",
"0"
},
{ // L_SKIA_JPGD_0225:
"img_8.4MP_3300x2550_SF4_rgb32.raw",
"d86f02c9941ad34451093794b0bef7d5",
"ebe8ee11e8042a7ab55f6879f5642768",
"0"
},
{ // L_SKIA_JPGD_0226:
"img_9MP_3000x3000_SF4_rgb32.raw",
"7879d98e61ab8c855ccab2bf65151576",
"5c9294be06d23aae52f947f35cb7b452",
"0"
},
{ // L_SKIA_JPGD_0227:
"img_9.6MP_3600x2400_SF4_rgb32.raw",
"10b1ad465a6b9c7ed50c8eed67bad761",
"98acd9418f4290facd816658e87fae85",
"0"
},
{ // L_SKIA_JPGD_0228:
"img_10MP_3664x2748_SF4_rgb32.raw",
"1c9ae5b839a562052fa20ee3d2e8dbf8",
"c6f346a995302e1c96018049362f37c7",
"0"
},
{ // L_SKIA_JPGD_0229:
"img_10.5MP_3780x2782_SF4_rgb32.raw",
"99bd7230b39d99934236a964e7b9deb0",
"02064f8059873e8bbee6de4a02a2bf37",
"0"
},
{ // L_SKIA_JPGD_0230:
"img_11MP_4028x2732_SF4_rgb32.raw",
"f4c4973a502e253ec01cbabeff67eb59",
"98b9bb982061e239cf29870b2aee5db3",
"0"
},
{ // L_SKIA_JPGD_0231:
"img_11.5MP_4195x2763_SF4_rgb32.raw",
"12fa798a17d029bf448605a24c286ff1",
"30c500ed5a956e0784cab38e403830b9",
"0"
},
{ // L_SKIA_JPGD_0232:
"I_000265_12MP_4000x3000_SF4_rgb32.raw",
"74ab67c13317f5a4251c9303e59091d1",
"f286f1767f3b55990f55b87fd5a1be86",
"0"
},
{ // L_SKIA_JPGD_0233:
"img_1MP_1280x800_prog_SF4_rgb32.raw",
"dc376ad17a1951eeaa7fdecdcfe3194e",
"31ecd9f3082ceccd6125b54d4fed9764",
"0"
},
{ // L_SKIA_JPGD_0234:
"img_1.3MP_1440x900_prog_SF4_rgb32.raw",
"03c66f7748885d9b6bae8f8b6f2afdf9",
"5c36a92c5cd875cb08e5021df131cf77",
"0"
},
{ // L_SKIA_JPGD_0235:
"img_2.3MP_1920x1200_prog_SF4_rgb32.raw",
"e8d083d07de164689635dc2387b93c73",
"565104406eb86815780fe3c68c5a4931",
"0"
},
{ // L_SKIA_JPGD_0236:
"img_3.1MP_2048x1536_prog_SF4_rgb32.raw",
"5b864c715234d17dc2e9d7b71f150884",
"7bea36e86d784cc228515776d62833f5",
"0"
},
{ // L_SKIA_JPGD_0237:
"img_3.5MP_2048x1800_prog_SF4_rgb32.raw",
"9e8ac9907fc395d7b1919e035ba6262f",
"ed430a13b2f9d38abd4ce61a07aac4c6",
"0"
},
{ // L_SKIA_JPGD_0238:
"img_4MP_2560x1600_prog_SF4_rgb32.raw",
"058cde0bdb5e8ac61cdae104a388fd6f",
"47cb38d503e02876abd235124961b8e6",
"0"
},
{ // L_SKIA_JPGD_0239:
"img_4.6MP_2560x1800_prog_SF4_rgb32.raw",
"291af0862059ed9f6980ba09a2f341a8",
"b524d7dc8b43088023f98f9c1a83573e",
"0"
},
{ // L_SKIA_JPGD_0240:
"img_5.5MP_2952x1882_prog_SF4_rgb32.raw",
"3bfacda31ac3841cdb6a9c42702ab82a",
"06b5f1a7116b57695bf17af9fd4a1b8a",
"0"
},
{ // L_SKIA_JPGD_0241:
"img_6.6MP_2842x2357_prog_SF4_rgb32.raw",
"38544e13a52dc7cd0282613c5ab1f65a",
"60d749cb20b947ee2f7756602fd550a1",
"0"
},
{ // L_SKIA_JPGD_0242:
"img_7MP_3072x2304_prog_SF4_rgb32.raw",
"69144f0509436243e791bb4aa473c2e2",
"688a7d6c099d177c6b1eae1c248e5c50",
"0"
},
{ // L_SKIA_JPGD_0243:
"img_7.5MP_3474x2154_prog_SF4_rgb32.raw",
"8f049d7801023e9e14b16dfc54bb9dbe",
"fec74f1096c6bee5ff40ca438b4eea4e",
"0"
},
{ // L_SKIA_JPGD_0244:
"I_000264_8MP_3264x2448_prog_SF4_rgb32.raw",
"ffa2f73f1cc106b83bcdafd2927187e0",
"936941cfa76f301c2d6208c16829841f",
"0"
},
{ // L_SKIA_JPGD_0245:
"img_8.4MP_3300x2550_prog_SF4_rgb32.raw",
"49ac040e906b238ac85dcd65bd9b612a",
"a41db1f464ea56726a66575d14be698d",
"0"
},
{ // L_SKIA_JPGD_0246:
"img_9MP_3000x3000_prog_SF4_rgb32.raw",
"0fabeb20fef08ee32987d5664d568448",
"f88cbec222a455b562eaed8b787c5fc5",
"0"
},
{ // L_SKIA_JPGD_0247:
"img_9.6MP_3600x2400_prog_SF4_rgb32.raw",
"80ce46c84e34739ca1909aea5bc11d02",
"63b2940ad1fb7d6fe39d84f6cacb2057",
"0"
},
{ // L_SKIA_JPGD_0248:
"img_10MP_3664x2748_prog_SF4_rgb32.raw",
"5eee3454f2f518afc8032d4eb0e43bc8",
"5da3adb0832ed25332afbef41ff731f5",
"0"
},
{ // L_SKIA_JPGD_0249:
"img_10.5MP_3780x2782_prog_SF4_rgb32.raw",
"683889a714d9fc0fc1f0eaf2bc79c35e",
"c22c7111965e88514ff1e79e487efb3e",
"0"
},
{ // L_SKIA_JPGD_0250:
"img_11MP_4028x2732_prog_SF4_rgb32.raw",
"1b042b891f5cc583f8d126e609aec69b",
"9c4ed0cb3b768704e670bc2c91e93d52",
"0"
},
{ // L_SKIA_JPGD_0251:
"img_11.5MP_4195x2763_prog_SF4_rgb32.raw",
"9429bb521e608eb58869aced9aa815f1",
"2cf72cc66eb10dc2bc4af9cd96be50a6",
"0"
},
{ // L_SKIA_JPGD_0252:
"I_000266_12MP_4000x3000_prog_SF4_rgb32.raw",
"9019ae5fd772853fd30b2a60176a0195",
"bf467bb162b69a3aea841dcd43418880",
"0"
},
{ // L_SKIA_JPGD_0253:
"img_1MP_1280x800_SF8_rgb32.raw",
"1a0d8a719bce3878ac0cfd2b2f7288f6",
"f7af8b96f6f912da7633dafd575a457b",
"0"
},
{ // L_SKIA_JPGD_0254:
"img_1.3MP_1440x900_SF8_rgb32.raw",
"a7ed4000ab046c57db220c678da25994",
"b9abab8cc025bd44480d7ce7d9708b00",
"0"
},
{ // L_SKIA_JPGD_0255:
"img_2.3MP_1920x1200_SF8_rgb32.raw",
"6ea14942fca529954b245de07da1493e",
"2a65952d146e2689327e97698193742e",
"0"
},
{ // L_SKIA_JPGD_0256:
"img_3.1MP_2048x1536_SF8_rgb32.raw",
"7808cf9255a0ddaac5355dd3d886c290",
"b0a1cb29c0da12a164b08dd3cc8d5876",
"0"
},
{ // L_SKIA_JPGD_0257:
"img_3.5MP_2048x1800_SF8_rgb32.raw",
"3cc38165b8a8c1dd74ca91a82841a3b4",
"bb45cd6c7a59b812b229d9911a991cbe",
"0"
},
{ // L_SKIA_JPGD_0258:
"img_4MP_2560x1600_SF8_rgb32.raw",
"6b489766b17732442c11992f3c8672d0",
"c2ec199586da3e6e30cd78bbf6647095",
"0"
},
{ // L_SKIA_JPGD_0259:
"img_4.6MP_2560x1800_SF8_rgb32.raw",
"2eea12ad6ec8f9f566a59e93ba0c718b",
"48fa621426d49cecb2efbdd8254c56e5",
"0"
},
{ // L_SKIA_JPGD_0260:
"img_5.5MP_2952x1882_SF8_rgb32.raw",
"df042cfc54c6d3ec0c51379802e216bc",
"f83f8a2a19cf241f5a41a3dd0cd88030",
"0"
},
{ // L_SKIA_JPGD_0261:
"img_6.6MP_2842x2357_SF8_rgb32.raw",
"cc3a563b4b1b384b612455038aeb24d2",
"7e4959cde1e622eb185a8702eed7d793",
"0"
},
{ // L_SKIA_JPGD_0262:
"img_7MP_3072x2304_SF8_rgb32.raw",
"67fa158cd7b18d171220e804af202a58",
"c3f2cbb8066650ae1950893d8d57f49b",
"0"
},
{ // L_SKIA_JPGD_0263:
"img_7.5MP_3474x2154_SF8_rgb32.raw",
"24475783fa9ee1ba9268d98cd33193b0",
"911fb7dd4daaf5d75a29e394de3e1eee",
"0"
},
{ // L_SKIA_JPGD_0264:
"I_000259_5MP_2560x1920_SF8_rgb32.raw",
"e003ee795cde8cbebcdae4bbacb56cec",
"8c97ed132e2890cdc5d30c04e066bc6d",
"0"
},
{ // L_SKIA_JPGD_0265:
"img_8.4MP_3300x2550_SF8_rgb32.raw",
"a64dcc859a0183c292f04fc741497441",
"80e75969a202970fb828586a8c16f603",
"0"
},
{ // L_SKIA_JPGD_0266:
"img_9MP_3000x3000_SF8_rgb32.raw",
"5d908ae9f60b0765dc8282e0873f7ae9",
"b29a218bbe5e5642203873cc228d991a",
"0"
},
{ // L_SKIA_JPGD_0267:
"img_9.6MP_3600x2400_SF8_rgb32.raw",
"6ffa20aaa632e1a06aadfbaa71980372",
"620a003a9b58d5e8a864ae18a080abe0",
"0"
},
{ // L_SKIA_JPGD_0268:
"img_10MP_3664x2748_SF8_rgb32.raw",
"b49a2c75c8452906a73cc9b7a0bf9749",
"2332f935d3ca296bf5da1af52004f8c5",
"0"
},
{ // L_SKIA_JPGD_0269:
"img_10.5MP_3780x2782_SF8_rgb32.raw",
"463ca2c33ba2da8d6c50e6fe45cee781",
"40eb80fd35c5fdc1d5094322ca6ccc51",
"0"
},
{ // L_SKIA_JPGD_0270:
"img_11MP_4028x2732_SF8_rgb32.raw",
"b4e1ba10b8d98300bfb64a168434ae46",
"db4155308c235be81ee402db7dca939d",
"0"
},
{ // L_SKIA_JPGD_0271:
"img_11.5MP_4195x2763_SF8_rgb32.raw",
"1005ec85fd3817837a1058d7841c846f",
"3ca4dbc6b88c8c32a9ca081cc3e75145",
"0"
},
{ // L_SKIA_JPGD_0272:
"6MP_3072x2048_SF8_rgb32.raw",
"904280669b7d301af23ed507cf25477b",
"23841b99dc68d1c6f546de8bb9e0e3b9",
"0"
},
{ // L_SKIA_JPGD_0273:
"img_1MP_1280x800_prog_SF8_rgb32.raw",
"b7ab8efe2997bed30f43c81649070a12",
"83c55ec046bca3a8e17405566ea90df5",
"0"
},
{ // L_SKIA_JPGD_0274:
"img_1.3MP_1440x900_prog_SF8_rgb32.raw",
"44a2b6bbcac06922020a4055add4e540",
"78c32547205a12a25d6c4d2566ce1c35",
"0"
},
{ // L_SKIA_JPGD_0275:
"img_2.3MP_1920x1200_prog_SF8_rgb32.raw",
"fc0f37746e0807237f0c30ef9c9c0256",
"a2a4247f2b0b74b386b440b28bef1395",
"0"
},
{ // L_SKIA_JPGD_0276:
"img_3.1MP_2048x1536_prog_SF8_rgb32.raw",
"6c1545c2997fff5dd9bfe470bb34b9c1",
"146553718cb36dfaf0941ccf284b5600",
"0"
},
{ // L_SKIA_JPGD_0277:
"img_3.5MP_2048x1800_prog_SF8_rgb32.raw",
"676795a30158a3d2a8057886cb58116a",
"1f72ab436ddcff3e51cafb657782b2fb",
"0"
},
{ // L_SKIA_JPGD_0278:
"img_4MP_2560x1600_prog_SF8_rgb32.raw",
"dcf1be2e7af615a92e5b64dcf113d437",
"202610ee72a97ce62c93fcd34e48b754",
"0"
},
{ // L_SKIA_JPGD_0279:
"img_4.6MP_2560x1800_prog_SF8_rgb32.raw",
"8f9a697b631ce90fb4a6a38c6636910b",
"38d7d9bb9ffa94535916c4232e7000ab",
"0"
},
{ // L_SKIA_JPGD_0280:
"img_5.5MP_2952x1882_prog_SF8_rgb32.raw",
"29fe2d1159c47aadebcd34e9a45c3afb",
"216ab790dfc70218c1a5cb6ab3eaf6ab",
"0"
},
{ // L_SKIA_JPGD_0281:
"img_6.6MP_2842x2357_prog_SF8_rgb32.raw",
"f19a5e1365bf8284ef97f60d9f46448d",
"e10f8e686d43255baffc5fcccdc120af",
"0"
},
{ // L_SKIA_JPGD_0282:
"img_7MP_3072x2304_prog_SF8_rgb32.raw",
"eb1be1f186dc4232c4f628aec1ba8f0b",
"ec6654204ca84e1b8438bc01c015ea53",
"0"
},
{ // L_SKIA_JPGD_0283:
"img_7.5MP_3474x2154_prog_SF8_rgb32.raw",
"e5706b5b7c676c7c75c259935f801411",
"dc715147fc4c4b3c1509dc164c6d9681",
"0"
},
{ // L_SKIA_JPGD_0284:
"I_000264_8MP_3264x2448_prog_SF8_rgb32.raw",
"d5ec45777bc75169b5b769555f04c3d5",
"01c9168265725172aaba2de3c2d53b1b",
"0"
},
{ // L_SKIA_JPGD_0285:
"I_000263_2MP_1600x1200_prog_SF8_rgb32.raw",
"e18971b44631a29dfb9c732443134f3f",
"1fc2ef9e96aee2f4f41483faa6ce3917",
"0"
},
{ // L_SKIA_JPGD_0286:
"img_9MP_3000x3000_prog_SF8_rgb32.raw",
"cc5f0049d4a544afe0b3df415570803d",
"0baf86777df0811b582d98b0fe01f104",
"0"
},
{ // L_SKIA_JPGD_0287:
"img_9.6MP_3600x2400_prog_SF8_rgb32.raw",
"4bd5b7067cd48be9e131693c93c98b65",
"fee131822cd895aae2d2e548f1a3440e",
"0"
},
{ // L_SKIA_JPGD_0288:
"img_10MP_3664x2748_prog_SF8_rgb32.raw",
"24eed0ef96820335ed8749ded3a57a08",
"e13ae55d320ca92e22021d96268cb76e",
"0"
},
{ // L_SKIA_JPGD_0289:
"img_10.5MP_3780x2782_prog_SF8_rgb32.raw",
"0c997f0fec40b545a0ecfd5f264c5988",
"b0e0b5f6cc2c143ae77b020e248cc223",
"0"
},
{ // L_SKIA_JPGD_0290:
"img_11MP_4028x2732_prog_SF8_rgb32.raw",
"a4991c9aa8e8754d1ddef50259fe2de8",
"d99eb71dd8c7b3ed1f7e686a22271967",
"0"
},
{ // L_SKIA_JPGD_0291:
"img_11.5MP_4195x2763_prog_SF8_rgb32.raw",
"edeea158fd40da8dec3b5efc2f68cc6b",
"b3d3eeae46a3ff175198b6322ded0d90",
"0"
},
{ // L_SKIA_JPGD_0292:
"I_000268_5MP_2560x1920_prog_SF8_rgb32.raw",
"4f10bafac67a821fa7d3447f0fc8f542",
"a31f2b4d7c15c45fac1c20aa31bf231e",
"0"
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
"4299d5c08c42607e7c1fd6438576a5c1", //ARM
//"a120a60c09380c556ec39da1bc6f09b5",   //Hack: DSP: another valid md5sum for stress test
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
//odd resolution image encoding tests
{ //L_SKIA_JPGE_0047
"img_odd_res_199x200.jpg",
"5093ece361c468d67808bba395a76364",
"0",
"0",
},
{ //L_SKIA_JPGE_0048
"img_odd_res_200x199.jpg",
"146af37e9169a50302501fec47a0577e",
"0",
"0",
},
{ //L_SKIA_JPGE_0049
"img_odd_res_199x199.jpg",
"1a69c8557aa0fa326e60057d9a03f02f",
"0",
"0",
},
{ //L_SKIA_JPGE_0050
"img_odd_res_199x200-2.jpg",
"5786d0eb5b588f7031684aa02a8e8c21",
"0",
"0",
},
{ //L_SKIA_JPGE_0051
"img_odd_res_200x199-2.jpg",
"f4f44027d7ab8c99d6d430b463277f83",
"0",
"0",
},
{ //L_SKIA_JPGE_0052
"img_odd_res_199x199-2.jpg",
"91b3d572df64f73183ad4ab7584fce24",
"0",
"0",
},
{ //L_SKIA_JPGE_0053
"img_odd_res_767x1024.jpg",
"0754244ddefc8666e8b73716de74b932",
"0",
"0",
},
{ //L_SKIA_JPGE_0054
"img_odd_res_768x1023.jpg",
"af502b74562e6800618d847cc59d5ec5",
"0",
"0",
},
{ //L_SKIA_JPGE_0055
"img_odd_res_767x1023.jpg",
"dd08b4e5c12573f2c8b956a884a12783",
"0",
"0",
},
{ //L_SKIA_JPGE_0056
"img_odd_res_767x1024-2.jpg",
"07d05f233842098ba5bf890203b024a2",
"0",
"0",
},
{ //L_SKIA_JPGE_0057
"img_odd_res_768x1023-2.jpg",
"10c56f122d2ae788d0c0745f27c5a3ce",
"0",
"0",
},
{ //L_SKIA_JPGE_0058
"img_odd_res_767x1023-2.jpg",
"b7b7bd3509bc4c8b908c033dfa55b87f",
"0",
"0",
},
{
"img_40x40.jpg",
"de42966b308851437e6d2a57b752052c",
"0",
"0",
},
{
"img_180x160.jpg",
"96b9cc698fdb1f4b86aa318ab2b4b8b7",
"0",
"0",
},
{
"img_1MP_1280x800.jpg",
"0",
"d314741f1d5954c46465fbbbf1ab61d5",
"0",
},
{
"img_1.3MP_1440x900.jpg",
"0",
"7c2eba27047229f0113d35c9f162778e",
"0",
},
{
"img_2.3MP_1920x1200.jpg",
"0",
"30d5ffd2f343f0fb4b712fa544091eae",
"0",
},
{
"img_3.1MP_2048x1536.jpg",
"0",
"90eaae721846e682b597dcc22245a3df",
"0",
},
{
"img_3.5MP_2048x1800.jpg",
"0",
"e48bef13d29919de20e71372ac7b2310",
"0",
},
{
"img_4MP_2560x1600.jpg",
"0",
"aea3b25de29ea0268b0ef92ba2b60861",
"0",
},
{
"img_4.6MP_2560x1800.jpg",
"0",
"b9e793a7ee914465c0db6abac86e8dcb",
"0",
},
{
"img_5.5MP_2952x1882.jpg",
"0",
"7c37445ca471480ff42735b3c3c7e39f",
"0",
},
{
"img_6.6MP_2842x2357.jpg",
"eb87115c2a0580a58ef1c0abdc0228bc",
"0",
"0",
},
{
"img_7MP_3072x2304.jpg",
"0",
"7a092e59ed2db8e4cc84df66a745e1d4",
"0",
},
{
"img_7.5MP_3474x2154.jpg",
"0",
"96ee2cfeb19992f544a78f7cc868a8ad",
"0",
},
{
"img_8.4MP_3300x2550.jpg",
"0",
"70c077cf61d71d5ef4d77ccade482cbd",
"0",
},
{
"img_9MP_3000x3000.jpg",
"0",
"8cd36ce9dd49556a53069dfb15fc76f8",
"0",
},
{
"img_9.6MP_3600x2400.jpg",
"0",
"4bd6c8f7c45d510fae3bd678e3c07269",
"0",
},
{
"img_10MP_3664x2748.jpg",
"0",
"330d0e0e1a0b2e6043efefe62540dcf1",
"0",
},
{
"img_10.5MP_3780x2782.jpg",
"0",
"67755f45edf70c6dfe6408e1d57d6161",
"0",
},
{
"img_11MP_4028x2732.jpg",
"0",
"4b3ad9cca93b192110e8a0c09f83f0a1",
"0",
},
{
"img_11.5MP_4195x2763.jpg",
"f897c84c422729f7ab58531255a357d1",
"0",
"0",
},

{   /*add the new test output filename and its md5sum before this comment line*/
"",
"",
"",
""
} /* this NULL string is mandatory */

}; /* End of st_jpge_file_list[] initialization */





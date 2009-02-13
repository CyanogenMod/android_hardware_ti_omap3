/******************************************************************************\
##                                                                            *
## Unpublished Proprietary and Confidential Information of Texas Instruments  *
## Israel Ltd. Do Not Disclose.                                               *
## Copyright 2008 Texas Instruments Israel Ltd.                               *
## All rights reserved. All unpublished rights reserved.                      *
##                                                                            *
## No part of this work may be used or reproduced in any form or by any       *
## means, or stored in a database or retrieval system, without prior written  *
## permission of Texas Instruments Israel Ltd. or its parent company Texas    *
## Instruments Incorporated.                                                  *
## Use of this work is subject to a license from Texas Instruments Israel     *
## Ltd. or its parent company Texas Instruments Incorporated.                 *
##                                                                            *
## This work contains Texas Instruments Israel Ltd. confidential and          *
## proprietary information which is protected by copyright, trade secret,     *
## trademark and other intellectual property rights.                          *
##                                                                            *
## The United States, Israel  and other countries maintain controls on the    *
## export and/or import of cryptographic items and technology. Unless prior   *
## authorization is obtained from the U.S. Department of Commerce and the     *
## Israeli Government, you shall not export, reexport, or release, directly   *
## or indirectly, any technology, software, or software source code received  *
## from Texas Instruments Incorporated (TI) or Texas Instruments Israel,      *
## or export, directly or indirectly, any direct product of such technology,  *
## software, or software source code to any destination or country to which   *
## the export, reexport or release of the technology, software, software      *
## source code, or direct product is prohibited by the EAR. The subject items *
## are classified as encryption items under Part 740.17 of the Commerce       *
## Control List (“CCL”). The assurances provided for herein are furnished in  *
## compliance with the specific encryption controls set forth in Part 740.17  *
## of the EAR -Encryption Commodities and Software (ENC).                     *
##                                                                            *
## NOTE: THE TRANSFER OF THE TECHNICAL INFORMATION IS BEING MADE UNDER AN     *
## EXPORT LICENSE ISSUED BY THE ISRAELI GOVERNMENT AND THE APPLICABLE EXPORT  *
## LICENSE DOES NOT ALLOW THE TECHNICAL INFORMATION TO BE USED FOR THE        *
## MODIFICATION OF THE BT ENCRYPTION OR THE DEVELOPMENT OF ANY NEW ENCRYPTION.*
## UNDER THE ISRAELI GOVERNMENT'S EXPORT LICENSE, THE INFORMATION CAN BE USED *
## FOR THE INTERNAL DESIGN AND MANUFACTURE OF TI PRODUCTS THAT WILL CONTAIN   *
## THE BT IC.                                                                 *
##                                                                            *
\******************************************************************************/
/*******************************************************************************\
*
*   FILE NAME:      sample_rate_converter.c
*
*   DESCRIPTION:    This file contains the Sample Rate Converter implementation.
*
*   AUTHOR:         
*
\*******************************************************************************/


#include "btl_config.h"


#if (BTL_CONFIG_A2DP_SAMPLE_RATE_CONVERTER == BTL_CONFIG_ENABLED)


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#include "sample_rate_converter.h"


/********************************************************************************
 *
 * Definitions
 *
 *******************************************************************************/

#define FILTER_LENGTH		12*12
#define RATIOMAX			12
#define SUBFILTER_LENGTH	FILTER_LENGTH/RATIOMAX

const short coef[144]={
	127,3320,11093,5020,-5753,2351,191,-935,274,0,0,0,
	93,3924,11391,3741,-5650,2896,-345,-673,214,0,0,0,
	210,4576,11558,2436,-5362,3291,-818,-421,155,0,0,0,
	333,5266,11580,1135,-4905,3524,-1211,-192,103,0,0,0,
	477,5987,11449,-130,-4297,3594,-1513,5,59,0,0,0,
	653,6726,11158,-1329,-3564,3507,-1718,163,27,0,0,0,
	871,7469,10705,-2430,-2734,3276,-1825,280,5,0,0,0,
	1138,8201,10092,-3408,-1837,2919,-1838,355,-7,0,0,0,
	1459,8907,9325,-4237,-910,2462,-1768,392,-10,0,0,0,
	1837,9567,8417,-4899,8,1931,-1626,396,-9,0,0,0,
	2274,10163,7383,-5377,879,1356,-1429,372,-5,0,0,0,
	2769,10678,6243,-5663,1670,766,-1192,329,-2,0,0,0};


/********************************************************************************
 *
 * Variables
 *
 *******************************************************************************/
 
static	SrcSamplingFreq 	SRC_inSampleFreq;
static	BTHAL_U8 			SRC_inNumOfChannels;
static	BTHAL_U16			SRC_outBlkMaxLen;
static	long				SRC_ratio;
static	long				SRC_leap;
static	long 				oldSamples[2*(SUBFILTER_LENGTH-1)];


/********************************************************************************
 *
 * Functions
 *
 *******************************************************************************/

long *filter12_sterio(void *out, void *inp, long leap, long count)
{
	long H, L, h, l, i;
	const short *c; 
	long *d=(long *)inp, *o=(long *)out; 

	i = leap>>16;	leap &= 0xffff;
	while(count--)
	{
		for(; i<FILTER_LENGTH; i+=leap)
		{
			c=coef+i;

			h=d[11];l=h<<16;h>>=16;l>>=16;	H = h*c[ 0];	L = l*c[ 0];
			h=d[10];l=h<<16;h>>=16;l>>=16;	H+= h*c[ 1];	L+= l*c[ 1];
			h=d[ 9];l=h<<16;h>>=16;l>>=16;	H+= h*c[ 2];	L+= l*c[ 2];
			h=d[ 8];l=h<<16;h>>=16;l>>=16;	H+= h*c[ 3];	L+= l*c[ 3];
			h=d[ 7];l=h<<16;h>>=16;l>>=16;	H+= h*c[ 4];	L+= l*c[ 4];
			h=d[ 6];l=h<<16;h>>=16;l>>=16;	H+= h*c[ 5];	L+= l*c[ 5];
			h=d[ 5];l=h<<16;h>>=16;l>>=16;	H+= h*c[ 6];	L+= l*c[ 6];
			h=d[ 4];l=h<<16;h>>=16;l>>=16;	H+= h*c[ 7];	L+= l*c[ 7];
			h=d[ 3];l=h<<16;h>>=16;l>>=16;	H+= h*c[ 8];	L+= l*c[ 8];
/*			h=d[ 2];l=h<<16;h>>=16;l>>=16;	H+= h*c[ 9];	L+= l*c[ 9];
			h=d[ 1];l=h<<16;h>>=16;l>>=16;	H+= h*c[10];	L+= l*c[10];
			h=d[ 0];l=h<<16;h>>=16;l>>=16;	H+= h*c[11];	L+= l*c[11];*/
											H>>=15; 		L>>=15;
			*o++=(L&0xffff)+(H<<16);
		}
		i-=FILTER_LENGTH;
		d++;
	}
	return o;
}

short *filter12_mono(void *out, void *inp, long leap, long count)
{
	long L, i;
	const short *c; 
	short *d=(short *)inp, *o=(short *)out; 

	i = leap>>16;	leap &= 0xffff; 
	while(count--)
	{
		for(; i<FILTER_LENGTH; i+=leap)
		{
			c=coef+i;

			L = c[ 0]*d[11];	L+= c[ 1]*d[10];	L+= c[ 2]*d[ 9];
			L+= c[ 3]*d[ 8];	L+= c[ 4]*d[ 7];	L+= c[ 5]*d[ 6];
			L+= c[ 6]*d[ 5];	L+= c[ 7]*d[ 4];	L+= c[ 8]*d[ 3];
/*			L+= c[ 9]*d[ 2];	L+= c[10]*d[ 1];	L+= c[11]*d[ 0];	*/

			*o++ = (short)(L>>=15);
		}
		i-=FILTER_LENGTH;
		d++;
	}
	return o;
}

BthalStatus SRC_Convert(BTHAL_U8 	*inBlk, 
						BTHAL_U16 	inBlkLen,  
						BTHAL_U8	*outBlk, 
						BTHAL_U16	*outBlkLen)
{
	short	*ptr16a, *ptr16b;
	long	*ptr32a, *ptr32b, i, numOfSamples;
	long	outLen, inpLen = (long)inBlkLen;

	if( inBlk	== 0)	return	BTHAL_STATUS_FAILED;
	if(outBlk	== 0)	return	BTHAL_STATUS_FAILED;
	if(inpLen	== 0)	return	BTHAL_STATUS_FAILED;
	if(outBlkLen== 0)	return	BTHAL_STATUS_FAILED;

	outLen = inpLen*SRC_ratio;
	if(SRC_inSampleFreq==SRC_SAMPLING_FREQ_32000)	outLen =(outLen+1)/2;
	if(outLen>(long)SRC_outBlkMaxLen)				return BTHAL_STATUS_FAILED;
	*outBlkLen = (BTHAL_U16)outLen;

	if(SRC_ratio == 1)
	{
		inpLen>>=1;
		ptr16a = (short *)inBlk;
		ptr16b = (short *)outBlk;
		while(inpLen--)	*ptr16b++ = *ptr16a++;

		return BTHAL_STATUS_SUCCESS;
	}

	if(SRC_inNumOfChannels == 1)
	{
		numOfSamples = inpLen>>1;
		if(numOfSamples < SUBFILTER_LENGTH)
		{
		ptr16a =(short *)inBlk;
			ptr16b =(short *)oldSamples + SUBFILTER_LENGTH-1;
			i=numOfSamples;			while(i--)	*ptr16b++=*ptr16a++;

			filter12_mono(outBlk,oldSamples,SRC_leap,numOfSamples);

		ptr16b =(short *)oldSamples;
			ptr16a = ptr16b+numOfSamples;
			i=SUBFILTER_LENGTH-1;	while(i--)	*ptr16b++=*ptr16a++;

			return BTHAL_STATUS_SUCCESS;
		}
		ptr16a =(short *)inBlk;
		ptr16b =(short *)oldSamples + SUBFILTER_LENGTH-1;
		i=SUBFILTER_LENGTH-1;	while(i--)	*ptr16b++=*ptr16a++;	

		ptr16a=filter12_mono(outBlk, oldSamples, SRC_leap, SUBFILTER_LENGTH-1);

		i=SRC_leap;		
		if(SRC_inSampleFreq==SRC_SAMPLING_FREQ_32000) 
			if((SUBFILTER_LENGTH-1)&1)
				i|=i<<15;
		numOfSamples-= SUBFILTER_LENGTH-1;

		filter12_mono(ptr16a, inBlk, i, numOfSamples);

		ptr16a =(short *)inBlk + numOfSamples;
		ptr16b =(short *)oldSamples;
		i=SUBFILTER_LENGTH-1;	while(i--)	*ptr16b++=*ptr16a++;
	}
	else
	{
		numOfSamples = inpLen>>2;
		if(numOfSamples < SUBFILTER_LENGTH)
		{
		ptr32a=(long *)inBlk;
			ptr32b = oldSamples + SUBFILTER_LENGTH-1;

			i=numOfSamples;			while(i--)	*ptr32b++=*ptr32a++;

			filter12_sterio(outBlk,oldSamples,SRC_leap,numOfSamples);

		ptr32b=oldSamples;
			ptr32a = ptr32b + numOfSamples;
			i=SUBFILTER_LENGTH-1;	while(i--)	*ptr32b++=*ptr32a++;

			return BTHAL_STATUS_SUCCESS;
		}
		ptr32a =(long *)inBlk;
		ptr32b =oldSamples + SUBFILTER_LENGTH-1;
		i=SUBFILTER_LENGTH-1;	while(i--)	*ptr32b++=*ptr32a++;	

		ptr32a=filter12_sterio(outBlk, oldSamples, SRC_leap, SUBFILTER_LENGTH-1);

		i=SRC_leap;		
		if(SRC_inSampleFreq==SRC_SAMPLING_FREQ_32000) 
			if((SUBFILTER_LENGTH-1)&1)
				i|=i<<15;
		numOfSamples-= SUBFILTER_LENGTH-1;

		filter12_sterio(ptr32a, inBlk, i, numOfSamples);

		ptr32a =(long *)inBlk + numOfSamples;
		ptr32b =oldSamples;
		i=SUBFILTER_LENGTH-1;	while(i--)	*ptr32b++=*ptr32a++;
	}

	return BTHAL_STATUS_SUCCESS;
}


BthalStatus SRC_ConfigureNewSong(SrcSamplingFreq inSampleFreq, BTHAL_U8 inNumOfChannels, BTHAL_U16 outBlkMaxLen, SrcSamplingFreq *outSampleFreq)
{
	if(inNumOfChannels<1 || inNumOfChannels>2) return BTHAL_STATUS_FAILED;

	SRC_inSampleFreq 	= inSampleFreq;
	SRC_inNumOfChannels = inNumOfChannels;
	SRC_outBlkMaxLen	= outBlkMaxLen;

	switch(inSampleFreq)
    {
        case SRC_SAMPLING_FREQ_8000 :  
        	*outSampleFreq = SRC_SAMPLING_FREQ_48000; /* ratio=6 */
			SRC_ratio=6; SRC_leap=FILTER_LENGTH/6;
        	break;
        case SRC_SAMPLING_FREQ_11025:  
        	*outSampleFreq = SRC_SAMPLING_FREQ_44100; /* ratio=4 */
			SRC_ratio=4; SRC_leap=FILTER_LENGTH/4;
        	break;
        case SRC_SAMPLING_FREQ_12000:  
        	*outSampleFreq = SRC_SAMPLING_FREQ_48000; /* ratio=4 */
			SRC_ratio=4; SRC_leap=FILTER_LENGTH/4;
        	break;
        case SRC_SAMPLING_FREQ_16000:   
        	*outSampleFreq = SRC_SAMPLING_FREQ_48000; /* ratio=3 */
			SRC_ratio=3; SRC_leap=FILTER_LENGTH/3;
        	break;
        case SRC_SAMPLING_FREQ_22050:  
        	*outSampleFreq = SRC_SAMPLING_FREQ_44100; /* ratio=2 */
			SRC_ratio=2; SRC_leap=FILTER_LENGTH/2;
        	break;
        case SRC_SAMPLING_FREQ_24000:  
        	*outSampleFreq = SRC_SAMPLING_FREQ_48000; /* ratio=2 */
			SRC_ratio=2; SRC_leap=FILTER_LENGTH/2;
        	break;
        case SRC_SAMPLING_FREQ_32000:  
        	*outSampleFreq = SRC_SAMPLING_FREQ_48000; /* ratio=3/2 */
			SRC_ratio=3; SRC_leap=FILTER_LENGTH*2/3;
        	break;
        case SRC_SAMPLING_FREQ_44100:  
        	*outSampleFreq = SRC_SAMPLING_FREQ_44100; /* ratio=1 */
			SRC_ratio=1; SRC_leap=FILTER_LENGTH;
        	break;
        case SRC_SAMPLING_FREQ_48000:  
        	*outSampleFreq = SRC_SAMPLING_FREQ_48000; /* ratio=1 */
			SRC_ratio=1; SRC_leap=FILTER_LENGTH;
        	break;
        default: return BTHAL_STATUS_FAILED;
    }

	return BTHAL_STATUS_SUCCESS;
}

BthalStatus SRC_ResetSongHistory(void)
{
	long	i;

	for(i=0; i<SUBFILTER_LENGTH-1; i++)	oldSamples[i]=0;

	return BTHAL_STATUS_SUCCESS;
}

#endif	/* BTL_CONFIG_A2DP_SAMPLE_RATE_CONVERTER == BTL_CONFIG_ENABLED */



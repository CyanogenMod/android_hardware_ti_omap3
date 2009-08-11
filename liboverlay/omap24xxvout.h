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

#ifndef OMAP24XXVOUT_H
#define OMAP24XXVOUT_H

/* This is for user apps */
#define OMAP24XX_OUTPUT_LCD        4
#define OMAP24XX_OUTPUT_TV         5
#define OMAP24XX_GFX_DESTINATION   100
#define OMAP24XX_VIDEO_SOURCE      101

struct omap24xxvout_colorkey {
	unsigned int output_dev;
	unsigned int key_type;
	unsigned int key_val;
};

struct omap24xxvout_bgcolor {
	unsigned int color;
	unsigned int output_dev;
};

struct omap24xxvout_colconv{
	short int RY,RCr,RCb;
	short int GY,GCr,GCb;
	short int BY,BCr,BCb; 
};


/* non-standard V4L2 ioctls that are specific to OMAP2 */
#define VIDIOC_S_OMAP2_MIRROR		_IOW ('V', 1,  int)
#define VIDIOC_G_OMAP2_MIRROR		_IOR ('V', 2,  int)
#define VIDIOC_S_OMAP2_ROTATION		_IOW ('V', 3,  int)
#define VIDIOC_G_OMAP2_ROTATION		_IOR ('V', 4,  int)
#define VIDIOC_S_OMAP2_LINK		_IOW ('V', 5,  int)
#define VIDIOC_G_OMAP2_LINK		_IOR ('V', 6,  int)
#define VIDIOC_S_OMAP2_COLORKEY		_IOW ('V', 7,  struct omap24xxvout_colorkey)
#define VIDIOC_G_OMAP2_COLORKEY		_IOW ('V', 8,  struct omap24xxvout_colorkey)
#define VIDIOC_S_OMAP2_BGCOLOR		_IOW ('V', 9,  struct omap24xxvout_bgcolor)
#define VIDIOC_G_OMAP2_BGCOLOR		_IOW ('V', 10, struct omap24xxvout_bgcolor)
#define VIDIOC_OMAP2_COLORKEY_ENABLE		_IOW ('V', 11, int)
#define VIDIOC_OMAP2_COLORKEY_DISABLE		_IOW ('V', 12, int)
#define VIDIOC_S_OMAP2_DEFCOLORCONV		_IOW ('V', 13, int)
#define VIDIOC_S_OMAP2_COLORCONV		_IOW ('V', 14, struct omap24xxvout_colconv)
#define VIDIOC_G_OMAP2_COLORCONV		_IOR ('V', 15, struct omap24xxvout_colconv)


#endif	/* ifndef OMAP24XXVOUT_H */

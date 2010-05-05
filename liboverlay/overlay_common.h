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

#ifndef OVERLAY_COMMON_H_
#define OVERLAY_COMMON_H_


#ifdef TARGET_OMAP4
/*This is required to acheive the best performance for ducati codecs.
* this will break arm based video codecs, and is acceptable as ducati codecs are
* enabled in the system bydefault.
*/
#define NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE    4
#define NUM_OVERLAY_BUFFERS_REQUESTED  (12)
#else
#define NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE    3
#define NUM_OVERLAY_BUFFERS_REQUESTED  (6)
#endif
/* Used in setAttributes */
#define CACHEABLE_BUFFERS 0x1
#define MAINTAIN_COHERENCY 0x2

/* The following defines are used to set the maximum values supported
 * by the overlay.
 * 720p is the max resolution currently supported (1280x720)
 * */

#define MAX_OVERLAY_WIDTH_VAL (1280)
#define MAX_OVERLAY_HEIGHT_VAL (1280)
#define MAX_OVERLAY_RESOLUTION (1280 * 720)


#endif  // OVERLAY_COMMON_H_


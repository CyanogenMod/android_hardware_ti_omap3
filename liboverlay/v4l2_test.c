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

#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <hardware/overlay.h>
#include <linux/videodev.h>
#include <stdlib.h>
#include "v4l2_utils.h"


#define LCD_WIDTH 800
#define LCD_HEIGHT 480
//#define READ_FROM_FILE

static void play(uint32_t video_pipeline, uint32_t src_w, uint32_t src_h, uint32_t format,
                        uint32_t global_alpha, char* img1, char *img2, uint32_t sleep_time, uint32_t num_frames)
{

    struct v4l2_capability caps;
    int num_bufs = 4, i, k, ret, size, fd;

    struct {
        void *start;
        size_t length;
    } buf_info[num_bufs];

    FILE* f1 = NULL;
    FILE* f2 = NULL;

    switch(format)
    {
        case OVERLAY_FORMAT_RGBA_8888:
        case OVERLAY_FORMAT_ARGB_8888:
            size = 4;
            break;
        case OVERLAY_FORMAT_RGB_565:
        case OVERLAY_FORMAT_YCbYCr_422_I:
        case OVERLAY_FORMAT_ARGB_4444:
            size = 2;
            break;
        default:
            printf("\nUnsupported format!\n");
            return;
    }
    
#ifdef READ_FROM_FILE    

    f1 = fopen(img1, "r");
    if ( f1 == NULL ) {
        printf("\n\n\n\nError: failed to open the file %s for reading\n\n\n\n", img1);
        return;
    }

    f2 = fopen(img2, "r");
    if ( f2 == NULL ) {
        printf("\n\n\n\nError: failed to open the file %s for reading\n\n\n\n", img2);
        return;
    }
    
#endif

    if ((fd = v4l2_overlay_open(video_pipeline)) < 0) {
        printf("Can not open video device\n");
        return;
    }
    
#if 0
    if (v4l2_overlay_get_caps(fd, &caps))
        return;

    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
        printf("No streaming capability in display driver!");
        goto EXIT;
    }
    if (!(caps.capabilities & V4L2_CAP_VIDEO_OVERLAY)) {
        printf("No v4l2_overlay_ capability in display driver!");
        goto EXIT;
    }
    if (!(caps.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        printf("No output capability in display driver!");
        goto EXIT;
    }
#endif

    if ( v4l2_overlay_init(fd, src_w, src_h, format) != 0 )
    {
        printf("Failed initializing overlays\n");
        goto EXIT;
    }
    if ( v4l2_overlay_set_rotation(fd, 0, 0) != 0 )
    {
        printf("Failed defaulting rotation\n");
        goto EXIT;
    }
    if ( v4l2_overlay_set_crop(fd, 0, 0, src_w, src_h) != 0 )
    {
        printf("Failed defaulting crop window\n");
        goto EXIT;
    }
    if ( v4l2_overlay_set_position(fd, 0, 0, LCD_WIDTH, LCD_HEIGHT) != 0 )
    {
        printf("Set Position Failed!\n");
        goto EXIT;
    }
    if ( v4l2_overlay_req_buf(fd, &num_bufs, 0, 1) != 0 )
    {
        printf("Failed requesting buffers\n");
        goto EXIT;
    }
    for (i = 0; i < num_bufs; i++) {
        if (v4l2_overlay_map_buf(fd, i, &buf_info[i].start, &buf_info[i].length))
        {
            printf("Failed mapping buffers\n");
            goto EXIT;
        }
    }

    size = size * src_w * src_h;

#ifdef READ_FROM_FILE    

    // copy the file into the buffers:
    ret = fread (buf_info[0].start, 1, size, f1);
    if (ret != size) 
    {
        printf ("\nReading error file1.\n"); 
        goto EXIT;
    }
    ret = fread (buf_info[2].start, 1, size, f1);
    if (ret != size) 
    {
        printf ("\nReading error file1. size = %d, read = %d\n", size, ret); 
        goto EXIT;
    }
    ret = fread (buf_info[1].start, 1, size, f2);
    if (ret != size) 
    {
        printf ("\nReading error file2. size = %d, read = %d\n", size, ret); 
        goto EXIT;
    }
    ret = fread (buf_info[3].start, 1, size, f2);
    if (ret != size) 
    {
        printf ("\nReading error file2.\n"); 
        goto EXIT;
    }
#else

    memset (buf_info[0].start, 0x60, size/2);
    memset ((buf_info[0].start + size/2), 0xAF, size/2);

    memset (buf_info[1].start, 0xAF, size/2);
    memset ((buf_info[1].start + size/2), 0x60, size/2);

    memcpy(buf_info[2].start, buf_info[0].start, size);
    memcpy(buf_info[3].start, buf_info[1].start, size);    
    
#endif    
    printf("\nStarting fake video");

    for (i = 0; i < num_bufs; i++) {
      v4l2_overlay_q_buf(fd, i);
    }

    if (v4l2_overlay_stream_on(fd) != 0)
    {
        printf("Stream Enable Failed!");
        goto EXIT;
    }

    for (k = 0; k < num_frames; k++) {
        v4l2_overlay_dq_buf(fd, &i);
        printf("\ndequeue index = %d", i);
        v4l2_overlay_q_buf(fd, i);
        sleep(sleep_time);
    }
    
    v4l2_overlay_stream_off(fd);

    printf("\nExit Successfully!\n\n");
EXIT:
    close(fd);
#ifdef READ_FROM_FILE        
    fclose(f1);    
    fclose(f2);    
#endif    
    return;
}


int main (int argc, char *argv[])
{

    if (argc != 10) {
        printf("\n\nUsage: v4l2_test <1 - video1, 2 - video2> <src_width> <src_height> <format> <global_alpha> <img1> <img2> <sleep(secs)> <No. of Frames>\n\n");
        printf("\n\nExample: ./system/bin/v4l2_test 1 176 144 20 0 /pepper.yuv /pepper.yuv 1 20");
        printf("\nFormats");
        printf("\n1 -\tOVERLAY_FORMAT_RGBA_8888");
        printf("\n4 -\tOVERLAY_FORMAT_RGB_565");
        printf("\n100 -\tOVERLAY_FORMAT_ARGB_8888");
        printf("\n200 -\tOVERLAY_FORMAT_ARGB_4444");
        printf("\n20 -\tOVERLAY_FORMAT_YCbCr_422_I");
        return 0;    
    }

    play(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), argv[6], argv[7], atoi(argv[8]), atoi(argv[9]));
    return 0;
}

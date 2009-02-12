/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fcntl.h>
#include <errno.h>
#include <cutils/log.h>
#include <hardware/overlay.h>
#include <linux/videodev.h>
#include "v4l2_utils.h"
#include <stdlib.h>


static void play(int32_t id, int32_t src_w, int32_t src_h,
                 int32_t dst_w, int32_t dst_h, int32_t dst_x, int32_t dst_y)
{
    struct v4l2_capability caps;
    int num_bufs = 4, i, j, k, l, m, ret;
    int32_t w, h, x, y;
    struct {
        void *start;
        size_t length;
    } buf_info[num_bufs];

    int ctl_fd = v4l2_overlay_open(id);
    if (ctl_fd < 0) {
        LOGE("Can not open video device %d\n", id);
        return;
    }

    if (v4l2_overlay_get_caps(ctl_fd, &caps))
        return;

    if (!(caps.capabilities & V4L2_CAP_STREAMING)) {
        LOGE("No streaming capability in display driver!");
        return;
    }
    if (!(caps.capabilities & V4L2_CAP_VIDEO_OVERLAY)) {
        LOGE("No v4l2_overlay_ capability in display driver!");
        //return;
    }
    if (!(caps.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
        LOGE("No output capability in display driver!");
        //return;
    }

    /* set color key? */
    /* set rotation? */
    /* tell the driver what the format of the src buffers is */
#if 0
    v4l2_overlay_set_pixfmt(&format, OVERLAY_FORMAT_RGB_565, src_w, src_h);
    if (v4l2_overlay_src_set_format(ctl_fd, &format))
        return;
    if (v4l2_overlay_set_crop(ctl_fd, 0, 0, src_w, src_h))
        return;

    /* set the output window */
    v4l2_overlay_set_window(&format, dst_x, dst_y, dst_w, dst_h);
    v4l2_overlay_set_pixfmt(&format, OVERLAY_FORMAT_RGB_565, dst_w, dst_h)
    if (v4l2_overlay_set_format(ctl_fd, &format))
        return;

    /* check the result and adjust the cropping rectangle if necessary */
    if (v4l2_overlay_get_format(ctl_fd, &format))
        return;
    v4l2_overlay_get_window(&format, &x, &y, &w, &h);
    if (w > dst_w)
      w = w * dst_w / src_w;
    if (h > dst_h)
      h = h * dst_h / src_h;
    if (v4l2_overlay_set_crop(ctl_fd, 0, 0, w, h))
        return;
#endif

    v4l2_overlay_dump_state(ctl_fd);
    v4l2_overlay_init(ctl_fd, src_w, src_h, OVERLAY_FORMAT_RGB_565);

    v4l2_overlay_set_position(ctl_fd, dst_x, dst_y, dst_w, dst_h);

    /* request some source buffers */
    if (v4l2_overlay_req_buf(ctl_fd, &num_bufs))
        return;
    for (i = 0; i < num_bufs; i++) {
        if (v4l2_overlay_map_buf(ctl_fd, i, &buf_info[i].start,
                                 &buf_info[i].length))
            return;
    }
    LOGI("starting fake video");

    v4l2_overlay_stream_on(ctl_fd);
    for (i = 0; i < num_bufs; i++) {
      v4l2_overlay_q_buf(ctl_fd, i);
    }

    for (k = 0; k < 1000; k++) {
        /* render frame */
        i = -1;
        v4l2_overlay_dq_buf(ctl_fd, &i);
        v4l2_overlay_dq_buf(ctl_fd, &m);
        LOGE("dq returned %d %d", i, m);
        /*
        for (j = 0; j < src_w * src_h; j++)
          ((uint16_t *)(buf_info[i].start))[j] = (uint16_t)0xf100;
        for (j = k % src_w; j < (k % src_w) + 10; j++)
          for (l = 0; l < 10; l++)
            ((uint16_t *)(buf_info[i].start))[j + (src_w * (src_h/2 + l))] =
                (uint16_t)0x0000;
                */
        v4l2_overlay_q_buf(ctl_fd, i);
        v4l2_overlay_q_buf(ctl_fd, i);
    }
    v4l2_overlay_stream_off(ctl_fd);
}


int main (int argc, char *argv[])
{

  if (argc >= 7) {
    play(V4L2_OVERLAY_PLANE_VIDEO1, atoi(argv[1]), atoi(argv[2]),
         atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
    return 0;
  }

  printf("not enough args -- using defaults!\n");
  play(V4L2_OVERLAY_PLANE_VIDEO1, 100, 200, 100, 200, 0, 0);
  return 0;
}

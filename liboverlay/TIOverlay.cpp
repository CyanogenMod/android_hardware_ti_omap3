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

#define LOG_TAG "TIOverlay"

#include <hardware/hardware.h>
#include <hardware/overlay.h>

extern "C" {
#include "v4l2_utils.h"
}

#include <pthread.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/videodev.h>

#include <cutils/log.h>
#include <cutils/ashmem.h>
#include <cutils/atomic.h>
#include "overlay_common.h"
#include "TIOverlay.h"

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y) ((X) > (Y) ? (X) : (Y))

int sysfile_write(const char* pathname, const void* buf, size_t size);
int sysfile_read(const char* pathname, const void* buf, size_t size);

#ifdef TARGET_OMAP4
//currently picoDLP is excluded, till it is thoroughly validated with .35 kernel
#define MAX_DISPLAY_CNT 3
#define MAX_MANAGER_CNT 3
#define PANEL_NAME_FOR_TV "hdmi"

/* These definitions must match those in plat/display.h */
#define OMAP_DSS_COLOR_RGB24U (1 << 7)  /* RGB24, 32-bit container */
#define OMAP_DSS_COLOR_ARGB32 (1 << 11) /* ARGB32 */

static void
changeFramebufferColorMode(int color_mode) {
        int ret;
        char color_mode_string[32];
        sprintf(color_mode_string, "%d", color_mode);
        if (sysfile_write("/sys/devices/platform/omapdss/overlay0/color_mode",
                          color_mode_string, strlen(color_mode_string)) < 0)
                LOGE("Unable to change color mode of overlay0 to %d",
                     color_mode);
}

#define CHANGE_FRAMEBUFFER_COLOR_MODE(color_mode) \
        changeFramebufferColorMode(color_mode)

#else
#define MAX_DISPLAY_CNT 3
#define MAX_MANAGER_CNT 2
#define PANEL_NAME_FOR_TV "tv"

#define CHANGE_FRAMEBUFFER_COLOR_MODE(color_mode)

#endif

const int KCloneDevice = OVERLAY_ON_PRIMARY;

static displayPanelMetaData screenMetaData[MAX_DISPLAY_CNT];
static displayManagerMetaData managerMetaData[MAX_MANAGER_CNT];

/*****************************************************************************/

#define LOG_FUNCTION_NAME_ENTRY    LOGV(" ###### Calling %s() ++ ######",  __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    LOGV(" ###### Calling %s() -- ######",  __FUNCTION__);

/** This variable is to make sure that the overlay control device is opened
* only once from the surface flinger process;
* this will not protect the situation where the device is loaded from multiple processes
* and also, this is not valid in the data context, which is created in a process different
* from the surface flinger process
*/
static overlay_control_context_t* TheOverlayControlDevice = NULL;

static struct hw_module_methods_t overlay_module_methods = {
    open: overlay_device_open
};

struct overlay_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: OVERLAY_HARDWARE_MODULE_ID,
        name: "Sample Overlay module",
        author: "The Android Open Source Project",
        methods: &overlay_module_methods,
        dso: NULL, /* remove compilation warnings */
        reserved: {0}, /* remove compilation warnings */
    }
};

/*
* Algorithm for calculating scaling factors while rendering HDMI.
*
*   Input
*       hdmi_w: HDMI output width
*       hdmi_h: HDMI output height
*       video_w: Video width (after cropping)
*       video_h: Video height (after cropping)
*       par: Pixel aspect ratio
*
*       Note: hdmi_w, hdmi_h and par are determined according to HDMI video format (CEA/VESA format)
*
*   Output
*       out_pos_x: Output window X position
*       out_pos_y: Output window Y position
*       out_w: Output width
*       out_h: Output height
*
*   Algorithm
*       h_div = hdmi_w / video_w
*       v_div = hdmi_h / video_h
*
*       if (h_div > v_div)
*           out_w = (hdmi_h / video_h) * video_w / par = v_div * video_w / par
*           out_h = (hdmi_h / video_h) * video_h = hdmi_h
*       else
*           out_w = (hdmi_w / video_w) * video_w = hdmi_w
*           out_h = (hdmi_w / video_w) * par * video_h = h_div * video_h / par
*
*       out_x_pos = (hdmi_w - out_w) / 2
*       out_y_pos = (hdmi_h - out_h) / 2
*/

struct hdmi_data {
    int cea_code;
    int width;
    int height;
    int par_h;
    int par_v;
    int interlaced;
};

/* Note: for modes with pixel repetition, par_h should be multiplied by PR (Pixel Repetition) factor */
struct hdmi_data hdmi[] = {
    {1,     640,    480,    1,  1,  0}, /* 640x480p60 4:3 */
    {19,    1280,   720,    1,  1,  0}, /* 1280x720p50 16:9 */
    {4,     1280,   720,    1,  1,  0}, /* 1280x720p60 16:9 */
    {2,     720,    480,    8,  9,  0}, /* 720x480p60 4:3 */
    {5,     1920,   540,    1,  1,  1}, /* 1920x1080i60 16:9 */
    {6,     1440,   240,    1,  1,  1}, /* 1440x240i, pixel aspect ratio is not considered*/
    {16,    1920,   1080,   1,  1,  0}, /* 1920x1080p60 16:9 */
    {17,    720,    576,    16, 15, 0}, /* 720x576p50 4:3 */
    {20,    1920,   540,    1,  1,  1}, /*1920x1080i50  16:9 */
    {21,    1440,   288,    1,  1,  1}, /* 1440x288i pixel aspect ratio not considered */
    {29,    1440,   576,    1,  1,  0}, /*1440x576p, pixel aspect ratio not considered */
    {31,    1920,   1080,   1,  1,  0}, /* 1920x1080p50 16:9 */
    {32,    1920,   1080,   1,  1,  0}, /* 1920x1080p24 16:9 */
    {35,    2880,   480,    1,  1,  0}, /*2880x480p, pixel aspect ratio not considered */
    {37,    2880,   576,    1,  1,  0}, /*2880x576p, pixel aspect ratio not considered */
    {-1,    -1,     -1,     1,  1,  0} /*VESA or custome code, hence rely on kernel timings*/
};

int sysfile_write(const char* pathname, const void* buf, size_t size) {
    int fd = open(pathname, O_WRONLY);
    if (fd == -1) {
        LOGE("Can't open [%s]", pathname);
        return -1;
    }
    size_t written_size = write(fd, buf, size);
    if (written_size <= 0) {
        LOGE("Can't write [%s]", pathname);
        close(fd);
        return -1;
    }
    if (close(fd) == -1) {
        LOGE("cant close [%s]", pathname);
        return -1;
    }
    return 0;
}

int sysfile_read(const char* pathname, void* buf, size_t size) {
    int fd = open(pathname, O_RDONLY);
    if (fd == -1) {
        LOGE("Can't open the file[%s]", pathname);
        return -1;
    }
    size_t bytesread = read(fd, buf, size);
    if ((int)bytesread < 0) {
        LOGE("cant read from file[%s]", pathname);
        close(fd);
        return -1;
    }
    if (close(fd) == -1) {
        LOGE("cant close file[%s]", pathname);
        return -1;
    }
    return bytesread;
}

int InitDisplayManagerMetaData() {
    /**
    *Initialize the display names and the associated paths to enable
    */
    for (int i = 0; i < MAX_DISPLAY_CNT; i++) {
        sprintf(screenMetaData[i].displayenabled, "/sys/devices/platform/omapdss/display%d/enabled", i);
        char displaypathname[PATH_MAX];
        sprintf(displaypathname, "/sys/devices/platform/omapdss/display%d/name", i);
        if (sysfile_read(displaypathname, (void*)(&screenMetaData[i].displayname), PATH_MAX) < 0) {
            LOGE("lcd name get failed");
            return -1;
        }
        strtok(screenMetaData[i].displayname, "\n");
        LOGD("LCD[%d] NAME[%s] \n", i, screenMetaData[i].displayname);
        LOGD("LCD[%d] PATH[%s] \n", i, screenMetaData[i].displayenabled);

        char displaytimingspath[PATH_MAX];
        sprintf(displaytimingspath, "/sys/devices/platform/omapdss/display%d/timings", i);
        if (sysfile_read(displaytimingspath, (void*)(&screenMetaData[i].displaytimings), PATH_MAX) < 0) {
            LOGE("LCD[%d] get timings failed", i);
            return -1;
        }
        LOGD("LCD[%d] timings[%s] \n", i, screenMetaData[i].displaytimings);
    }

    /**
    *Initialize the manager names and the paths to control manager transparency settings
    */
    for (int i = 0; i < MAX_MANAGER_CNT; i++) {
        char managerpathname[PATH_MAX];
        sprintf(managerpathname, "/sys/devices/platform/omapdss/manager%d/name", i);
        if (sysfile_read(managerpathname, (void*)(&managerMetaData[i].managername), PATH_MAX) < 0) {
            LOGE("manager name get failed");
            return -1;
        }
        strtok(managerMetaData[i].managername, "\n");
        LOGD("MANAGER[%d] NAME[%s] \n", i, managerMetaData[i].managername);
        sprintf(managerMetaData[i].managerdisplay, "/sys/devices/platform/omapdss/manager%d/display", i);
        sprintf(managerMetaData[i].managertrans_key_enabled, "/sys/devices/platform/omapdss/manager%d/trans_key_enabled", i);
        sprintf(managerMetaData[i].managertrans_key_type, "/sys/devices/platform/omapdss/manager%d/trans_key_type", i);
        sprintf(managerMetaData[i].managertrans_key_value, "/sys/devices/platform/omapdss/manager%d/trans_key_value", i);
    }
    return 0;
}

// ****************************************************************************
// Control context shared methods: to be used between control and data contexts
// ****************************************************************************
int overlay_control_context_t::create_shared_overlayobj(overlay_object** overlayobj)
{
    LOG_FUNCTION_NAME_ENTRY
    int fd;
    int ret = 0;
    // NOTE: assuming sizeof(overlay_object) < four pages
    int size = getpagesize()*4;
    overlay_object *p;

    if ((fd = ashmem_create_region("overlay_data", size)) < 0) {
        LOGE("Failed to Create Overlay Data!\n");
        return fd;
    }

    p = (overlay_object*)mmap(NULL, size, PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) {
        LOGE("Failed to Map Overlay Data!\n");
        close(fd);
        return -1;
    }

    memset(p, 0, size);
    p->marker = OVERLAY_DATA_MARKER;
    p->mControlHandle.overlayobj_size = size;
    p->mControlHandle.overlayobj_sharedfd = fd;
    p->refCnt = 1;

    if ((ret = pthread_mutexattr_init(&p->attr)) != 0) {
        LOGE("Failed to initialize overlay mutex attr");
    }

    if (ret == 0 && (ret = pthread_mutexattr_setpshared(&p->attr, PTHREAD_PROCESS_SHARED)) != 0) {
        LOGE("Failed to set the overlay mutex attr to be shared across-processes");
    }

    if (ret == 0 && (ret = pthread_mutex_init(&p->lock, &p->attr)) != 0) {
        LOGE("Failed to initialize overlay mutex\n");
    }

    if (ret != 0) {
        munmap(p, size);
        close(fd);
        return -1;
    }

    LOG_FUNCTION_NAME_EXIT
    *overlayobj = p;
    return fd;
}

void overlay_control_context_t::destroy_shared_overlayobj(overlay_object *overlayobj, bool isCtrlpath)
{
    LOG_FUNCTION_NAME_ENTRY
    if (overlayobj == NULL)
        return;

    // Last side deallocated releases the mutex, otherwise the remaining
    // side will deadlock trying to use an already released mutex
    if (android_atomic_dec(&(overlayobj->refCnt)) == 1) {
        if (pthread_mutex_destroy(&(overlayobj->lock))) {
            LOGE("Failed to uninitialize overlay mutex!\n");
        }

        if (pthread_mutexattr_destroy(&(overlayobj->attr))) {
            LOGE("Failed to uninitialize the overlay mutex attr!\n");
        }

        overlayobj->marker = 0;
    }

    int overlayfd = -1;
    if (isCtrlpath) {
        overlayfd = overlayobj->mControlHandle.overlayobj_sharedfd;
    }
    else {
        overlayfd = overlayobj->mDataHandle.overlayobj_sharedfd;
    }
    if (munmap(overlayobj, overlayobj->mControlHandle.overlayobj_size)) {
        LOGE("Failed to Unmap Overlay Shared Data!\n");
    }
    if (close(overlayfd)) {
        LOGE("Failed to Close Overlay Shared Data!\n");
    }

    LOG_FUNCTION_NAME_EXIT
}

overlay_object* overlay_control_context_t::open_shared_overlayobj(int ovlyfd, int ovlysize)
{
    LOG_FUNCTION_NAME_ENTRY
    int rc   = -1;
    int mode = PROT_READ | PROT_WRITE;
    overlay_object* overlay;

    overlay = (overlay_object*)mmap(0, ovlysize, mode, MAP_SHARED, ovlyfd, 0);

    if (overlay == MAP_FAILED) {
        LOGE("Failed to Map Overlay Shared Data!\n");
    } else if ( overlay->marker != OVERLAY_DATA_MARKER ) {
        LOGE("Invalid Overlay Shared Marker!\n");
        munmap( overlay, ovlysize);
    } else if ( (int)overlay->mControlHandle.overlayobj_size != ovlysize ) {
        LOGE("Invalid Overlay Shared Size!\n");
        munmap(overlay, ovlysize);
    } else {
        android_atomic_inc(&overlay->refCnt);
        rc = 0;
    }

    LOG_FUNCTION_NAME_EXIT
    if (!rc)
    {
        overlay->mDataHandle.overlayobj_sharedfd = ovlyfd;
        return overlay;
    }
    else
    {
        return NULL;
    }
}


#include <stdlib.h>
/**
* Precondition:
* This function has to be called after setting the crop window parameters
*/
void overlay_control_context_t::calculateWindow(overlay_object *overlayobj, overlay_ctrl_t *finalWindow,
                                                int panelId, bool isCtrlpath)
{
    LOG_FUNCTION_NAME_ENTRY
    /*
    * If default UI is on TV, android UI will receive 1920x1080 for screen size, and w & h will be 1920x1080 by default.
    * It will remain 1920x1080 even if overlay is requested on LCD. A better choice would be to query the display size:
    * and use the full resolution only for TV, for LCD lets respect whatever surface flinger asks for: this is required to
    * maintain the aspect ratio decided by media player
    */
    uint32_t dummy, w2, h2;
    overlay_ctrl_t   *data   = overlayobj->data();
    int fd = -1;
    int linkfd = -1;
    uint32_t tempW, tempH;
    char displayCode[20];
    char displayMode[20];
    char displayCodepath[PATH_MAX];
    int index = 0;
    int interlace_factor = 1;
    char found = 0;
    float h_div, v_div;

    uint32_t finalCropW = overlayobj->mData.cropW;
    uint32_t finalCropH = overlayobj->mData.cropH;
    uint32_t finalCropX = overlayobj->mData.cropX;
    uint32_t finalCropY = overlayobj->mData.cropY;

    if (sscanf(screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displaytimings, "%u,%u/%u/%u/%u,%u/%u/%u/%u\n",
        &dummy, &w2, &dummy, &dummy, &dummy, &h2, &dummy, &dummy, &dummy) != 9) {
        w2 = finalWindow->posW;
        h2 = finalWindow->posH; /* use default value, if could not read timings */
    }
    LOGV("calculateWindow(): w2=%d, h2=%d", w2, h2);

    switch (panelId) {
        case OVERLAY_ON_PRIMARY:
        case OVERLAY_ON_SECONDARY:
        case OVERLAY_ON_PICODLP:
            finalWindow->posX = data->posX;
            finalWindow->posY = data->posY;
            // Adjust the coordinate system to match the V4L change
            switch ( data->rotation ) {
            case 90:
            case 270:
                finalWindow->posW = data->posH;
                finalWindow->posH = data->posW;
                /*
                * Some times during rotation usecases, it is observed that,
                * Surface Flinger transformation is incorrectly sending
                * position co-ordinates, and these are outside the bounds.
                * This happens only during the transition, and finally Surface-flinger
                * sends the correct co-ordinates
                * The intermediate settings are ignored if they are out of bounds
                */
                if (finalWindow->posX > LCD_HEIGHT) {
                    finalWindow->posX = 0;
                }
                if (finalWindow->posY > LCD_WIDTH) {
                    finalWindow->posY = 0;
                }
                break;
            case 180:
            default: // 0
                finalWindow->posW = data->posW;
                finalWindow->posH = data->posH;
                break;
            }
            /**
            * check the final window scaling factors against H/W caps
            * and adjust
            * (1) Crop window keeping the Aspect ratio same--> For downscaling limit
            * (2) Final Output Window --> For Upsclaing limit
            * Before start checks, roundup the Window width & height to even integer
            */
            finalWindow->posH = finalWindow->posH - (finalWindow->posH % 2);
            finalWindow->posW = finalWindow->posW - (finalWindow->posW % 2);
            /* Handling the Downscaling scenario for Height */
            if (overlayobj->mData.cropH > (finalWindow->posH * MAX_DSS_DOWNSCALING_FACTOR)) {
                tempH = MAX(overlayobj->mData.cropH, 1); //this is to avoid cropH being zero scenario
                finalCropH = MAX_DSS_DOWNSCALING_FACTOR * finalWindow->posH;
                //adjust the cropW for the new cropH, to maintain aspect ratio
                finalCropW = (overlayobj->mData.cropW * finalCropH) / tempH;
            } else if (finalWindow->posH > (overlayobj->mData.cropH * MAX_DSS_UPSCALING_FACTOR)) {
                /* Handling the upscaling scenario for Height*/
                tempH = MAX(finalWindow->posH, 1); //this is to avoid posH being zero scenario
                finalWindow->posH = MAX_DSS_UPSCALING_FACTOR * overlayobj->mData.cropH;
                //adjust the window width to maintain aspect ratio
                finalWindow->posW = (finalWindow->posW * finalWindow->posH ) / tempH;
            }

            /* Handling the Downscaling scenario for Width*/
            if (overlayobj->mData.cropW > (finalWindow->posW * MAX_DSS_DOWNSCALING_FACTOR)) {
                tempW = MAX(overlayobj->mData.cropW, 1); //this is to avoid cropW being zero scenario
                finalCropW = MAX_DSS_DOWNSCALING_FACTOR * finalWindow->posW;
                //adjust the cropH for the new cropW, to maintain aspect ratio
                finalCropH = (overlayobj->mData.cropH * finalCropW) / tempW;
            } else if (finalWindow->posW > (overlayobj->mData.cropW * MAX_DSS_UPSCALING_FACTOR)) {
                /* Handling the upscaling scenario for width */
                tempW = MAX(finalWindow->posW, 1); //this is to avoid posW being zero scenario
                finalWindow->posW = MAX_DSS_UPSCALING_FACTOR * overlayobj->mData.cropW;
                //adjust the window height to maintain aspect ratio
                finalWindow->posH = (finalWindow->posH * finalWindow->posW ) / tempW;
            }

            //now calculate cropX and cropY so that we crop from center
            finalCropX = overlayobj->mData.cropX + ((overlayobj->mData.cropW - finalCropW) >> 1);
            finalCropY = overlayobj->mData.cropY + ((overlayobj->mData.cropH - finalCropH) >> 1);

            if (isCtrlpath) {
                fd = overlayobj->getctrl_videofd();
                linkfd = overlayobj->getctrl_linkvideofd();
            } else {
                fd = overlayobj->getdata_videofd();
                linkfd = overlayobj->getdata_linkvideofd();
            }

            if (linkfd > 0)
                fd = linkfd;

            if (v4l2_overlay_set_crop(fd, finalCropX, finalCropY,\
                                      finalCropW, finalCropH)) {
                LOGE("Failed defaulting crop window\n");
                return;
            }
            break;
        case OVERLAY_ON_TV:
            {
#ifdef TARGET_OMAP4
                for (index = 0;index < MAX_DISPLAY_CNT; index++) {
                    if (strcmp(screenMetaData[index].displayname, "hdmi") == 0) {
                        LOGD("found Panel Id @ [%d]", index);
                        break;
                    }
                }
                sprintf(displayCodepath, "/sys/devices/platform/omapdss/display%d/code", index);
                memset(displayMode,0,sizeof(displayMode));
                memset(displayCode,0,sizeof(displayCode));
                if (sysfile_read(displayCodepath, displayMode, PATH_MAX) < 0) {
                    LOGE("HDMI Code get failed");
                    return;
                }
                if(!strncmp(displayMode, "CEA:", 4)) {
                    strcpy(displayCode, displayMode+4);
                }

                /* Find index in HDMI data table */
                for (index = 0; index < (sizeof(hdmi) / sizeof(struct hdmi_data)); index++) {
                    if (hdmi[index].cea_code == atoi(displayCode)) {
                        LOGV("Found CEA code %d: %dx%d%s %d:%d\n",
                        atoi(displayCode),
                        hdmi[index].width,
                        hdmi[index].height,
                        hdmi[index].interlaced ? "i" : "p",
                        hdmi[index].par_h,
                        hdmi[index].par_v);
                        found = 1;
                        break;
                    }
                }

                if (found == 0) {
                    LOGE("Unknown CEA code: %d\n", atoi(displayCode));
                    //May be VESA code or custom code, then rely on kernel timings
                    index --;
                    hdmi[index].width  = w2;
                    hdmi[index].height = h2;
                }

                if (hdmi[index].interlaced)
                    interlace_factor = 2;
                /**
                * For interlaced mode of the TV, modify the timings read, to convert them into
                * progressive and finally update final window height for Interlaced mode
                */
                if ((overlayobj->mData.cropH > 1080) || (overlayobj->mData.cropW > 1920)) {
                    //Here the input is beyond 1080p or it is a portrait 1080p clip.
                    //Since downscaling is not supported at 1080p video timings, we have to crop input to meet requirements
                    //make sure aspect ratio is maintained.
                    w2 = w2;
                    h2 = h2 * interlace_factor;
                    finalWindow->posX = 0;
                    finalWindow->posY = 0;
                    if (overlayobj->mData.cropW * h2 > w2 * overlayobj->mData.cropH) {
                        finalCropW= w2;
                        finalCropH= (overlayobj->mData.cropH * finalCropW) / MAX(overlayobj->mData.cropW, 1);
                    } else {
                        finalCropH = h2;
                        finalCropW = (overlayobj->mData.cropW * finalCropH) / MAX(overlayobj->mData.cropH, 1);
                    }
                    //now calculate cropX and cropY so that we crop from center
                    finalCropX = overlayobj->mData.cropX + ((overlayobj->mData.cropW - finalCropW) >> 1);
                    finalCropY = overlayobj->mData.cropY + ((overlayobj->mData.cropH - finalCropH) >> 1);
                }

                h_div = (hdmi[index].width) / MAX((float)finalCropW, 1.0000);
                v_div = ((hdmi[index].height * interlace_factor)) / MAX((float)finalCropH, 1.0000);

                LOGV("H div=%0.4f [%d]\n", h_div, h_div);
                LOGV("V div=%0.4f [%d]\n", v_div, v_div);

                if (h_div > v_div) {
                    finalWindow->posW = ((v_div * hdmi[index].par_v * finalCropW) / MAX(hdmi[index].par_h, 1));
                    finalWindow->posH = hdmi[index].height;
                }
                else {
                    finalWindow->posW = hdmi[index].width;
                    finalWindow->posH = ((h_div * hdmi[index].par_h * finalCropH) / MAX((hdmi[index].par_v * interlace_factor), 1));
                }

                /* Position */
                finalWindow->posX = (hdmi[index].width - finalWindow->posW) / 2;
                finalWindow->posY = (hdmi[index].height - finalWindow->posH) / 2;
#else
                finalWindow->posX = 0;
                finalWindow->posY = 0;
                overlay_ctrl_t   *data   = overlayobj->data();
                if (data->rotation == 0) {
                    unsigned int aspect_ratio = 0;
                    unsigned int w_ar = (TV_WIDTH * 100) / overlayobj->w;
                    unsigned int h_ar = (TV_HEIGHT * 100) / overlayobj->h;
                    if (w_ar < h_ar) aspect_ratio = w_ar;
                    else aspect_ratio = h_ar;

                    finalWindow->posW = (overlayobj->w * aspect_ratio) / 100;
                    finalWindow->posH = (overlayobj->h * aspect_ratio) / 100;
                    if (finalWindow->posW < TV_WIDTH) finalWindow->posX = (TV_WIDTH - finalWindow->posW) / 2;
                    if (finalWindow->posH < TV_HEIGHT) finalWindow->posY = (TV_HEIGHT - finalWindow->posH) / 2;
                    LOGD("Maximize the window. Maintain aspect ratio.");
                }
                else LOGD("Window settings not changed bcos Rotation != 0. Wouldnt rotation be zero when watching on TV???");
#endif
                if (isCtrlpath) {
                    fd = overlayobj->getctrl_videofd();
                } else {
                    fd = overlayobj->getdata_videofd();
                }
            }
            break;
        default:
            LOGE("Leave the default  values");
    };

    LOGV("Adjusted CROP Settings: cropX(%d)/cropY(%d)/cropW(%d)/cropH(%d)",
            finalCropX, finalCropY, finalCropW, finalCropH);

    LOGV("calculateWindow(): posX=%d, posY=%d, posW=%d, posH=%d",
            finalWindow->posX, finalWindow->posY, finalWindow->posW, finalWindow->posH);

    if (v4l2_overlay_set_crop(fd, finalCropX, finalCropY,\
                finalCropW, finalCropH)) {
        LOGE("Failed defaulting crop window\n");
        return;
    }

    LOG_FUNCTION_NAME_EXIT
}

void overlay_control_context_t::calculateDisplayMetaData(overlay_object *overlayobj, int panelId)
{
    LOG_FUNCTION_NAME_ENTRY
    const char* paneltobeDisabled = NULL;
    static const char* panelname = "lcd";
    static const char* managername = "lcd";
    overlay_ctrl_t   *stage  = overlayobj->staging();

    switch(panelId){
        case OVERLAY_ON_PRIMARY: {
            LOGV("REQUEST FOR LCD1");
            panelname = "lcd";
            managername = "lcd";
        }
        break;
        case OVERLAY_ON_SECONDARY: {
            LOGV("REQUEST FOR LCD2");
            panelname = "lcd2";
            managername ="2lcd";
            paneltobeDisabled = "pico_DLP";
        }
        break;
        case OVERLAY_ON_TV: {
            LOGV("REQUEST FOR TV");
            panelname = PANEL_NAME_FOR_TV;
            managername = "tv";
        }
        break;
        case OVERLAY_ON_PICODLP: {
            LOGV("REQUEST FOR PICO DLP");
            panelname = "pico_DLP";
            managername = "2lcd";
            paneltobeDisabled = "lcd2";
        }
        break;
        case OVERLAY_ON_VIRTUAL_SINK:
            LOGD("REQUEST FOR VIRTUAL SINK: Setting the Default display for now");
        default: {
            panelname = "lcd";
            managername = "lcd";
        }
        break;
    };
    overlayobj->mDisplayMetaData.mPanelIndex = -1;
    for (int i = 0; i < MAX_DISPLAY_CNT; i++) {
        LOGV("Display id [%s], dst id [%s]", screenMetaData[i].displayname, panelname);
        if (strcmp(screenMetaData[i].displayname, panelname) == 0) {
            LOGD("found Panel Id @ [%d], displayname [%s]", i, panelname);
            overlayobj->mDisplayMetaData.mPanelIndex = i;
            break;
        }
    }
    if (overlayobj->mDisplayMetaData.mPanelIndex == -1) {
        LOGE("The screenMetaData was not populated correctly. Could not find panel, '%s'.", panelname);
    }

    overlayobj->mDisplayMetaData.mManagerIndex = -1;
    for (int i = 0; i < MAX_MANAGER_CNT; i++) {
        LOGV("managername name [%s], dst name [%s]", managerMetaData[i].managername, managername);
        if (strcmp(managerMetaData[i].managername, managername) == 0) {
            LOGD("found Display Manager @ [%d], managername [%s]", i, managername);
            overlayobj->mDisplayMetaData.mManagerIndex = i;
            break;
        }
    }
    if (overlayobj->mDisplayMetaData.mManagerIndex == -1) {
        LOGE("The managerMetaData was not populated correctly. Could not find manager, '%s'", managername);
    }

    overlayobj->mDisplayMetaData.mTobeDisabledPanelIndex = -1;
    if (paneltobeDisabled != NULL) {
        //since we are switching to the pico_DLP/2LCD, switch off the 2LCD/pico_DLP first
        for (int i = 0; i < MAX_DISPLAY_CNT; i++) {
            LOGD("Display id [%s]", screenMetaData[i].displayname);
            LOGD("Display to be disabled id [%s]", paneltobeDisabled);
            if (strcmp(screenMetaData[i].displayname, paneltobeDisabled) == 0) {
                LOGD("found to be disabled Panel Id @ [%d]", i);
                overlayobj->mDisplayMetaData.mTobeDisabledPanelIndex = i;
                break;
            }
        }
    }
    LOG_FUNCTION_NAME_EXIT
}

void overlay_control_context_t::close_shared_overlayobj(overlay_object *overlayobj)
{
    destroy_shared_overlayobj(overlayobj, false);
    overlayobj = NULL;
}

int overlay_data_context_t::enable_streaming_locked(overlay_object* overlayobj, bool isDatapath)
{
    LOG_FUNCTION_NAME_ENTRY
    int rc = 0;
    if (overlayobj == NULL) {
        LOGE("Null Arguments / Overlay not initd");
        return -1;
    }

    if (!overlayobj->dataReady) {
        LOGI("Cannot enable stream without queuing at least one buffer");
        return -1;
    }
    overlayobj->streamEn = 1;
    int fd, linkfd;
    if (isDatapath) {
        fd = overlayobj->getdata_videofd();
        linkfd = overlayobj->getdata_linkvideofd();
    }
    else {
        fd = overlayobj->getctrl_videofd();
        linkfd = overlayobj->getctrl_linkvideofd();
    }
    rc = v4l2_overlay_stream_on(fd);
    if (rc) {
        LOGE("Stream Enable Failed!/%d\n", rc);
        overlayobj->streamEn = 0;
    }
    if (linkfd > 0) {
        rc = v4l2_overlay_stream_on(linkfd);
        if (rc) {
            LOGE("link device Stream Enable Failed!/%d\n", rc);
            overlayobj->streamEn = 0;
        }
    }
    LOG_FUNCTION_NAME_EXIT
    return rc;
}

// function not appropriate since it has mutex protection within.
int overlay_data_context_t::enable_streaming(overlay_object* overlayobj, bool isDatapath)
{
    int ret;
    if (overlayobj == NULL) {
    LOGE("Null Arguments / Overlay not initd");
    return -1;
    }
    pthread_mutex_lock(&overlayobj->lock);

    ret = enable_streaming_locked(overlayobj, isDatapath);
    pthread_mutex_unlock(&overlayobj->lock);
    return ret;
}

int overlay_data_context_t::disable_streaming_locked(overlay_object* overlayobj, bool isDatapath)
{
    LOG_FUNCTION_NAME_ENTRY
    if (overlayobj == NULL) {
        LOGE("Null Arguments / Overlay not initd");
        return -1;
    }

    int ret = 0;

    if (overlayobj->streamEn) {
        int fd;
        int linkfd;

        if (isDatapath) {
            fd = overlayobj->getdata_videofd();
            linkfd = overlayobj->getdata_linkvideofd();
        }
        else {
            fd = overlayobj->getctrl_videofd();
            linkfd = overlayobj->getctrl_linkvideofd();
        }
        ret = v4l2_overlay_stream_off( fd );
        if (linkfd > 0 ) {
            v4l2_overlay_stream_off( linkfd );
        }

        if (ret) {
            LOGE("Stream Off Failed!/%d\n", ret);
        } else {
            overlayobj->streamEn = 0;
            overlayobj->qd_buf_count = 0;
        }
    }
    LOG_FUNCTION_NAME_EXIT
    return ret;
}

// function not appropriate since it has mutex protection within.
int overlay_data_context_t::disable_streaming(overlay_object* overlayobj, bool isDatapath)
{
    int ret;
    if (overlayobj == NULL) {
    LOGE("Null Arguments / Overlay not initd");
    return -1;
    }
    pthread_mutex_lock(&overlayobj->lock);

    ret = disable_streaming_locked(overlayobj, isDatapath);
    pthread_mutex_unlock(&overlayobj->lock);
    return ret;
}

// ****************************************************************************
// Control module context: used only in the control context
// ****************************************************************************

int overlay_control_context_t::overlay_get(struct overlay_control_device_t *dev, int name)
{
    int result = -1;

    switch (name) {
        case OVERLAY_MINIFICATION_LIMIT:   result = 0;  break; // 0 = no limit
        case OVERLAY_MAGNIFICATION_LIMIT:  result = 0;  break; // 0 = no limit
        case OVERLAY_SCALING_FRAC_BITS:    result = 0;  break; // 0 = infinite
        case OVERLAY_ROTATION_STEP_DEG:    result = 90; break; // 90 rotation steps (for instance)
        case OVERLAY_HORIZONTAL_ALIGNMENT: result = 1;  break; // 1-pixel alignment
        case OVERLAY_VERTICAL_ALIGNMENT:   result = 1;  break; // 1-pixel alignment
        case OVERLAY_WIDTH_ALIGNMENT:      result = 1;  break; // 1-pixel alignment
        case OVERLAY_HEIGHT_ALIGNMENT:     break;
    }

    return result;
}

overlay_t* overlay_control_context_t::overlay_createOverlay(struct overlay_control_device_t *dev,
                                        uint32_t w, uint32_t h, int32_t  format, int isS3D)
{
    LOG_FUNCTION_NAME_ENTRY

    overlay_object            *overlayobj;
    overlay_control_context_t *self = (overlay_control_context_t *)dev;

    int ret;
    uint32_t num = NUM_OVERLAY_BUFFERS_REQUESTED;
    int fd;
    int overlay_fd;
    int pipelineId = -1;
    int index = 0;
    /* Validate the width and height are within a valid range for the
    * video driver.
    * */
    if (w > MAX_OVERLAY_WIDTH_VAL || h > MAX_OVERLAY_HEIGHT_VAL
    || w*h >MAX_OVERLAY_RESOLUTION){
       LOGE("%d: Error - Currently unsupported settings width %d, height %d",
       __LINE__, w, h);
       return NULL;
    }

    if (w <= 0 || h <= 0){
       LOGE("%d: Error - Invalid settings width %d, height %d",
       __LINE__, w, h);
       return NULL;
    }

    if (format == OVERLAY_FORMAT_DEFAULT) {
        format = OVERLAY_FORMAT_CbYCrY_422_I;  //UYVY is the default format
    }

    int overlayid = -1;

    //NOTE : do we need mutex protection here??
    //may not be needed here, because Overlay creation happens only from Surface flinger context
    for (int i = 0; i < MAX_NUM_OVERLAYS; i++) {
        if (self->mOmapOverlays[i] == NULL) {
            overlayid = i;
            break;
        }
    }

    if (overlayid < 0) {
        LOGE("No free overlay available");
        return NULL;
    }

#ifdef TARGET_OMAP4
    if (isS3D)
    {
        LOGD("Enabling the OVERLAY[0] for S3D \n");
        fd = v4l2_overlay_open(-1);
    }
    else
#endif
    {
        LOGD("Enabling the OVERLAY[%d]", overlayid);
        fd = v4l2_overlay_open(overlayid);
    }

    if (fd < 0) {
        LOGE("Failed to open overlay device[%d]\n", overlayid);
        return NULL;
    }

    overlay_fd = self->create_shared_overlayobj(&overlayobj);
    if ((overlay_fd < 0) || (overlayobj == NULL)) {
        LOGE("Failed to create shared data");
        close(fd);
        return NULL;
    }

    v4l2_overlay_getId(fd, &pipelineId);

    if ((pipelineId >= 0) && (pipelineId <= MAX_NUM_OVERLAYS)) {
        sprintf(overlayobj->overlaymanagerpath, "/sys/devices/platform/omapdss/overlay%d/manager", pipelineId);
        sprintf(overlayobj->overlayenabled, "/sys/devices/platform/omapdss/overlay%d/enabled", pipelineId);
    }
#ifdef TARGET_OMAP4
    //lets reset the manager to the lcd to start with
    if (!isS3D) {
      if (sysfile_write(overlayobj->overlaymanagerpath, "lcd", sizeof("lcd")) < 0) {
            LOGE("Overlay Manager set failed, but proceed anyway");
        }
    }
#endif

    LOGD("Creating overlay%s from W%d/H%d/FMT%d ...",
            isS3D ? " with S3D enable" : "", w, h, format);

    if (v4l2_overlay_init(fd, w, h, format)) {
        LOGE("Failed initializing overlays\n");
        goto error1;
    }

    if (v4l2_overlay_set_rotation(fd, 0, 0, 0)) {
        LOGE("Failed defaulting rotation\n");
        goto error1;
    }

    if (v4l2_overlay_set_crop(fd, 0, 0, w, h)) {
        LOGE("Failed defaulting crop window\n");
        goto error1;
    }

#ifdef TARGET_OMAP4
    if (v4l2_overlay_set_colorkey(fd, 1, 0x00, EVIDEO_SOURCE)){
        LOGE("Failed enabling color key\n");
        goto error1;
    }
#endif

    /* Enable the video zorder and video transparency
    * for the controls to be visible on top of video, give the graphics highest zOrder
    **/
    if (!isS3D) {
        /* Omap has 3 video pipelines whose zOrders are 0,1,and 2.
         * The graphics overlay is special with reserved zOrder of 3.
         * Graphics calls are not coming to this module.
         * Overlay with higher zOrder is displayed on top of the one
         * with lower zOrder. Two overlays assigned to the same
         * zOrder will result in distortions on the video output.
         * An available zOrder is assigned to this overlay.
         **/
        int maxZorder = -1;
        for (int i=0; i < MAX_NUM_OVERLAYS; i++) {
            if (self->mOmapOverlays[i] != NULL) {
                LOGD("mZorderUsage[%d] = %d is occupied", i, self->mZorderUsage[i]);
                if (self->mZorderUsage[i] > maxZorder) {
                    maxZorder = self->mZorderUsage[i];
                }
            }
        }

        if (maxZorder == MAX_NUM_OVERLAYS -1) {
            LOGD("No zOrder is available\n");
            goto error1;
        }

        self->mZorderUsage[overlayid] = maxZorder+1;
        if (v4l2_overlay_set_zorder(fd, maxZorder+1)) {
            LOGE("Failed setting zorder\n");
            goto error1;
        }
        LOGD("mZorderUsage[%d] is assigned to %d", overlayid, self->mZorderUsage[overlayid]);
    }

    if (v4l2_overlay_req_buf(fd, &num, 0, 0, EMEMORY_MMAP)) {
        LOGE("Failed requesting buffers\n");
        goto error1;
    }

    overlayobj->init(fd, w, h, format, num, overlayid);
    overlayobj->controlReady = 0;
    overlayobj->streamEn = 0;
    overlayobj->dispW = LCD_WIDTH; // Need to determine this properly: change it in setParameter
    overlayobj->dispH = LCD_HEIGHT; // Need to determine this properly

    overlayobj->mData.cropX = 0;
    overlayobj->mData.cropY = 0;
    overlayobj->mData.cropW = w;
    overlayobj->mData.cropH = h;

    overlayobj->mData.s3d_active = isS3D;
    overlayobj->mData.s3d_mode = OVERLAY_S3D_MODE_ON;
    overlayobj->mData.s3d_fmt = OVERLAY_S3D_FORMAT_NONE;
    overlayobj->mData.s3d_order = OVERLAY_S3D_ORDER_LF;
    overlayobj->mData.s3d_subsampling = OVERLAY_S3D_SS_NONE;

    self->mOmapOverlays[overlayid] = overlayobj;

    /* Configure Default GFX Overlay to be RGB8888 to Properly Handle
       Alpha Transparency if this is the 1st Overlay Created */
    if (self->mNumOverlays == 0) {
            CHANGE_FRAMEBUFFER_COLOR_MODE(OMAP_DSS_COLOR_ARGB32);
    }
    self->mNumOverlays++;

    LOG_FUNCTION_NAME_EXIT

    return overlayobj;
error1:
    close(fd);
    self->destroy_shared_overlayobj(overlayobj);
    return NULL;
}

overlay_t* overlay_control_context_t::overlay_createOverlay(struct overlay_control_device_t *dev,
                                        uint32_t w, uint32_t h, int32_t  format)
{
    // For Non-3D case
    return overlay_createOverlay(dev, w, h, format, 0);
}

int overlay_control_context_t::overlay_requestOverlayClone(struct overlay_control_device_t* dev,
                                                           overlay_t* overlay,int enable)
{
    overlay_control_context_t *self = (overlay_control_context_t *)dev;
    overlay_object *overlayobj = static_cast<overlay_object *>(overlay);
    LOGD("TIOVerlay: req Clone[%d]", enable);

    int overlayid = -1;
    int linkfd = -1;
    if (!enable) {
        linkfd = overlayobj->getctrl_linkvideofd();
        if (linkfd > 0) {
            overlayobj->setctrl_linkvideofd(-1);
            close(linkfd);
            return -1;
        }
    }

    //NOTE : do we need mutex protection here??
    //may not be needed here, because Overlay creation happens only from Surface flinger context
    for (int i = 0; i < MAX_NUM_OVERLAYS; i++) {
        if (self->mOmapOverlays[i] == NULL) {
            overlayid = i;
            break;
        }
    }

    if (overlayid < 0) {
        LOGE("No free overlay available");
        return -1;
    }

    linkfd = v4l2_overlay_open(overlayid);
    if (linkfd < 0) {
        LOGE("Failed to open overlay linked device\n");
        return -1;
    }

    if (v4l2_overlay_init(linkfd, overlayobj->w, overlayobj->h, overlayobj->format)) {
        LOGE("Failed initializing overlays\n");
        goto error1;
    }

    if (v4l2_overlay_set_rotation(linkfd, 0, 0, 0)) {
        LOGE("Failed defaulting rotation\n");
        goto error1;
    }

    if (v4l2_overlay_set_crop(linkfd, 0, 0, overlayobj->w, overlayobj->h)) {
        LOGE("Failed defaulting crop window\n");
        goto error1;
    }

    if (v4l2_overlay_req_buf(linkfd, (uint32_t*)&overlayobj->num_buffers, 0, 0, EMEMORY_USRPTR)) {
        LOGE("Error creating linked buffers1");
        goto error1;
    }

    overlayobj->setctrl_linkvideofd(linkfd);
    LOGD("link_fd = %d", linkfd);
    return (linkfd);

error1:
    close(linkfd);
    return -1;
}

void overlay_control_context_t::overlay_destroyOverlay(struct overlay_control_device_t *dev,
                                   overlay_t* overlay)
{
    LOG_FUNCTION_NAME_ENTRY
    if ((dev == NULL) || (overlay == NULL)) {
        LOGE("Null Arguments / Overlay not initd");
        return;
    }

    overlay_control_context_t *self = (overlay_control_context_t *)dev;
    overlay_object *overlayobj = static_cast<overlay_object *>(overlay);

    int rc, ret;
    int fd = overlayobj->getctrl_videofd();
    int linkfd = overlayobj->getctrl_linkvideofd();
    int index = overlayobj->getIndex();
    int num_ovls_with_alpha = 0;
    overlay_ctrl_t   *data = overlayobj->data();


    overlay_data_context_t::disable_streaming(overlayobj, false);

    LOGI("Destroying overlay/fd=%d/obj=%08lx", fd, (unsigned long)overlay);

#ifdef TARGET_OMAP3
    if (data->colorkey < 0) {
        /*  Lets Iterate among all created overlays to identify how many of them
            have local alpha blending enabled.
            IF we have any overlays using/requiring local alpha, apart from the
            current to-be destroyed overlay, DO NOT switch it off.
        */
        for (int i=0; i < MAX_NUM_OVERLAYS; i++) {
            if (self->mOmapOverlays[i] != NULL) {
                overlay_object *ovlobj = static_cast<overlay_object *>(self->mOmapOverlays[i]);
                overlay_ctrl_t *ovldata = ovlobj->data();

                if(ovldata->colorkey < 0)
                    num_ovls_with_alpha++;
            }
        }

        if (num_ovls_with_alpha == 1) {
            LOGE("%s : Lets Switch off Alpha Blending.\n", __func__ );
            if ((ret = v4l2_overlay_set_local_alpha(fd,0)))
                LOGE("Failed disabling local alpha \n");
        }
    }
#endif

    if (close(fd)) {
        LOGI( "Error closing overly fd/%d\n", errno);
    }

    if (linkfd > 0 ) {
        if (close(linkfd)) {
            LOGI( "Error closing link fd/%d\n", errno);
        }
    }

    self->destroy_shared_overlayobj(overlayobj);

    //NOTE : needs no protection, as the calls are already serialized at Surfaceflinger level
    self->mOmapOverlays[index] = NULL;

    /* When this overlay is destructed, its zOrder should be
     * freed and all other overlays whose zOrder is higher
     * than this overlay's zOrder should be lowered by 1 in order
     * to make zOrder consectively occupied.
     **/

    int targetedZorder = self->mZorderUsage[index];
    LOGD("mZorderUsage[%d] = %d should be removed", index, targetedZorder);

    self->mZorderUsage[index] = -1;
    for (int i=0; i < MAX_NUM_OVERLAYS; i++) {
        if (self->mOmapOverlays[i] != NULL) {
            if (self->mZorderUsage[i] > targetedZorder) {
                LOGD("mZorderUsage[%d]=%d should be lowered by 1",i, self->mZorderUsage[i]);
                self->mZorderUsage[i] -= 1;
                overlay_object *overlayobj = static_cast<overlay_object *>(self->mOmapOverlays[i]);
                int fd = overlayobj->getctrl_videofd();

                if (v4l2_overlay_set_zorder(fd, self->mZorderUsage[i])) {
                    LOGE("Failed setting zorder\n");
                    //There is nothing to do if failed
                }
            }
        }
    }

    self->mNumOverlays--;
    /* Restore GFX Overlay to Default Color Mode of RGB24U When There
       Are No More Overlays */
    if (self->mNumOverlays == 0) {
            CHANGE_FRAMEBUFFER_COLOR_MODE(OMAP_DSS_COLOR_RGB24U);
    }

    LOG_FUNCTION_NAME_EXIT
}

int overlay_control_context_t::overlay_setPosition(struct overlay_control_device_t *dev,
                               overlay_t* overlay, int x, int y, uint32_t w, uint32_t h)
{
    LOG_FUNCTION_NAME_ENTRY
    if ((dev == NULL) || (overlay == NULL)) {
        LOGE("Null Arguments / Overlay not initd");
        return -1;
    }

    overlay_object *overlayobj = static_cast<overlay_object *>(overlay);
    overlay_ctrl_t   *data   = overlayobj->data();
    overlay_ctrl_t   *stage  = overlayobj->staging();
    int fd = overlayobj->getctrl_videofd();
    overlay_ctrl_t finalWindow;
    int rc = 0;

    overlay_control_context_t *self = (overlay_control_context_t *)dev;

    int targetedOverlay = overlayobj->getIndex();
    int targetedZorder = self->mZorderUsage[targetedOverlay];
    LOGV("mZorderUsage[%d] = %d needs to be assigned the highest zOrder", targetedOverlay, targetedZorder);
    int maxZorder = -1;



    // FIXME:  This is a hack to deal with seemingly unintentional negative
    // offset that pop up now and again.  I believe the negative offsets are
    // due to a surface flinger bug that has not yet been found or fixed.
    //
    // This logic here is to return an error if the rectangle is not fully
    // within the display, unless we have not received a valid position yet,
    // in which case we will do our best to adjust the rectangle to be within
    // the display.

    // Require a minimum size
    if (w < 16 || h < 16) {
        // Return an error
        rc = -1;
        goto END;
    } else if (!overlayobj->controlReady) {
        if ( x < 0 ) x = 0;
        if ( y < 0 ) y = 0;
        if ( w > overlayobj->dispW ) w = overlayobj->dispW;
        if ( h > overlayobj->dispH ) h = overlayobj->dispH;
        if ( (x + w) > overlayobj->dispW ) w = overlayobj->dispW - x;
        if ( (y + h) > overlayobj->dispH ) h = overlayobj->dispH - y;
    } else if (x < 0 || y < 0 || (x + w) > overlayobj->dispW ||
               (y + h) > overlayobj->dispH) {
        // Return an error
        rc = -1;
        goto END;
    }

    if (data->posX == (unsigned int)x &&
        data->posY == (unsigned int)y &&
        data->posW == w &&
        data->posH == h) {
        LOGV("Nothing to set position!\n");
        goto END;
    }

    stage->posX = x;
    stage->posY = y;
    stage->posW = w;
    stage->posH = h;

    LOGI("Setting position X%d/Y%d/W%d/H%d", x, y, w, h);

    /* Ideally, the zOrder of overlay should be matched to the order of corresponding Views at UI.
     * The color of these views is currently set to 0 (color key) and overlays underneath these views punch
     * holes and are allowed to see through. However, the order of views is not communicated to
     * overlay module. The linkage is missing in the framework (or not I am aware of).
     * In order to compensate this loss, it is assumed that the overlay whose position and/or size
     * are changed is placed on top of all other overlays.
     **/
    for(int i=0; i < MAX_NUM_OVERLAYS; i++) {
        if(self->mOmapOverlays[i] != NULL) {
            if( self->mZorderUsage[i] > maxZorder) {
                maxZorder = self->mZorderUsage[i];
            }

            if(self->mZorderUsage[i] > targetedZorder) {
                LOGI("mZorderUsage[%d]=%d should be lowered by 1",i, self->mZorderUsage[i]);
                self->mZorderUsage[i] -= 1;
                overlay_object *overlayobj_selected = static_cast<overlay_object *>(self->mOmapOverlays[i]);
                int fdd = overlayobj_selected->getctrl_videofd();

                if (v4l2_overlay_set_zorder(fdd, self->mZorderUsage[i])) {
                    LOGE("Failed setting zorder\n");
                    //there is nothing to do if failed
                }
           }
        }
    }

    if( maxZorder != targetedZorder ) {
        self->mZorderUsage[targetedOverlay] = maxZorder;
        if (v4l2_overlay_set_zorder(fd, maxZorder)) {
            LOGE("Failed setting zorder\n");
            //there is nothing to do if failed
        }
    }

END:
    LOG_FUNCTION_NAME_EXIT
    return rc;
}

int overlay_control_context_t::overlay_getPosition(struct overlay_control_device_t *dev,
                               overlay_t* overlay, int* x, int* y, uint32_t* w, uint32_t* h)
{
    LOG_FUNCTION_NAME_ENTRY
    if ((dev == NULL) || (overlay == NULL)) {
        LOGE("Null Arguments / Overlay not initd");
        return -1;
    }

    int fd = static_cast<overlay_object *>(overlay)->getctrl_videofd();

    if (v4l2_overlay_get_position(fd, x, y, (int32_t*)w, (int32_t*)h)) {
        return -EINVAL;
    }
    return 0;
}

int overlay_control_context_t::overlay_setParameter(struct overlay_control_device_t *dev,
                                overlay_t* overlay, int param, int value)
{
    LOG_FUNCTION_NAME_ENTRY

    if ((dev == NULL) || (overlay == NULL)) {
        LOGE("Null Arguments / Overlay not initd");
        return -1;
    }

    overlay_object *overlayobj = static_cast<overlay_object *>(overlay);
    overlay_ctrl_t *stage = static_cast<overlay_object *>(overlay)->staging();
    int rc = 0;

    switch (param) {
    case OVERLAY_DITHER:
        break;

    case OVERLAY_TRANSFORM:
        switch ( value ) {
        case 0:
            stage->rotation = 0;
            stage->mirror = false;
            break;
        case OVERLAY_TRANSFORM_ROT_90:
            stage->rotation = 90;
            stage->mirror = false;
            break;
        case OVERLAY_TRANSFORM_ROT_180:
            stage->rotation = 180;
            stage->mirror = false;
            break;
        case OVERLAY_TRANSFORM_ROT_270:
            stage->rotation = 270;
            stage->mirror = false;
            break;
        case OVERLAY_TRANSFORM_FLIP_H:
            stage->rotation = 180;
            stage->mirror = true;
            break;
        case OVERLAY_TRANSFORM_FLIP_V:
            stage->rotation = 0;
            stage->mirror = true;
            break;
        case (HAL_TRANSFORM_FLIP_V | OVERLAY_TRANSFORM_ROT_90):
            stage->rotation = 90;
            stage->mirror = true;
            break;
        case (HAL_TRANSFORM_FLIP_H | OVERLAY_TRANSFORM_ROT_90):
            stage->rotation = 270;
            stage->mirror = true;
            break;
        default:
            rc = -EINVAL;
            break;
        }
        break;
    case OVERLAY_COLOR_KEY:
        stage->colorkey = value;
        break;
    case OVERLAY_PLANE_ALPHA:
        //adjust the alpha to the HW limit
        stage->alpha = MIN(value, 0xFF);
        break;
    case OVERLAY_PLANE_Z_ORDER:
        //limit the max value of z-order to hw limit i.e.3 and gfx and vid1 are 3 & 2 respectively
        //make sure that video is behind graphics so it to either 0 or 1
        stage->zorder = MIN(value, 1);
        break;
    case OVERLAY_SET_DISPLAY_WIDTH:
        overlayobj->dispW = value;
        break;
    case OVERLAY_SET_DISPLAY_HEIGHT:
        overlayobj->dispH = value;
        break;
    case OVERLAY_SET_SCREEN_ID:
        if (value > OVERLAY_ON_VIRTUAL_SINK) {
            value = OVERLAY_ON_PRIMARY;
        }
        stage->panel = value;
        LOGV("overlay_setParameter(): OVERLAY_SET_SCREEN_ID : stage->panel=%d",stage->panel);
        break;
    default:
        rc = -1;

    }

    LOG_FUNCTION_NAME_EXIT
    return rc;
}

int overlay_control_context_t::overlay_stage(struct overlay_control_device_t *dev,
                          overlay_t* overlay) {
    return 0;
}

int overlay_control_context_t::overlay_commit(struct overlay_control_device_t *dev,
                          overlay_t* overlay) {
    LOG_FUNCTION_NAME_ENTRY
    if ((dev == NULL) || (overlay == NULL)) {
        LOGE("Null Arguments / Overlay not initd");
        return -1;
    }

    overlay_object *overlayobj = static_cast<overlay_object *>(overlay);
    overlay_control_context_t *self = (overlay_control_context_t *)dev;

    overlay_ctrl_t   *data   = overlayobj->data();
    overlay_ctrl_t   *stage  = overlayobj->staging();

    int ret = 0;
    overlay_ctrl_t finalWindow;
    char overlaymanagername[PATH_MAX];
    int strmatch;
    int fd = overlayobj->getctrl_videofd();
    overlay_data_t eCropData;
    int rotation_degree = stage->rotation;
    int transparency_type = EVIDEO_SOURCE;
    int videopipezorder = stage->zorder;
    int transkey = stage->colorkey;

#ifndef TARGET_OMAP4
    /** NOTE: In order to support HDMI without app explicitly requesting for
    * the screen ID, this is the alternative path check for the overlay manager.
    * (here assumption is : overlay manager is set from command line
    * prior to playing the media clip.)
    */
    if (sysfile_read(overlayobj->overlaymanagerpath, &overlaymanagername, PATH_MAX) < 0) {
        LOGE("Overlay manager Name Get failed [%d]", __LINE__);
        return -1;
    }
    strtok(overlaymanagername, "\n");
#endif

    pthread_mutex_lock(&overlayobj->lock);

    if ((stage->posW == 0) || (stage->posH == 0)) {
        LOGE("InValid Window size: posW[%d], posH[%d]", stage->posW, stage->posH);
        goto end;
    }
    overlayobj->controlReady = 1;

    if (overlayobj->mData.s3d_active) {
        uint32_t current_panel;

        if (!v4l2_overlay_get_display_id(fd, &current_panel)) {
            data->panel = current_panel;
        }
    }

    if (data->posX == stage->posX && data->posY == stage->posY &&
        data->posW == stage->posW && data->posH == stage->posH &&
        data->rotation == stage->rotation &&
#ifdef TARGET_OMAP4
        data->alpha == stage->alpha &&
        data->mirror == stage->mirror &&
#endif
        data->colorkey == stage->colorkey &&
        data->panel == stage->panel) {
        LOGV("Nothing to commit!\n");
        goto end;
    }

    data->posX       = stage->posX;
    data->posY       = stage->posY;
    data->posW       = stage->posW;
    data->posH       = stage->posH;
    data->rotation   = stage->rotation;
    data->alpha      = stage->alpha;
    data->colorkey   = stage->colorkey;
    data->zorder     = stage->zorder;
    data->mirror     = stage->mirror;

#ifndef TARGET_OMAP4
    /** NOTE: In order to support HDMI without app explicitly requesting for
    * the screen ID, this is the alternative path check for the overlay manager.
    * (here assumption is : overlay manager is set from command line
    * prior to playing the media clip.)
    */
    strmatch = strcmp(overlaymanagername, "tv");
    if (!strmatch) {
        stage->panel = OVERLAY_ON_TV;
    }
#endif
    // Disable streaming to ensure that stream_on is called again which indirectly sets overlayenabled to 1
    if ((ret = overlay_data_context_t::disable_streaming_locked(overlayobj, false))) {
        LOGE("Stream Off Failed!/%d\n", ret);
        goto end;
    }

    if (stage->panel == OVERLAY_ON_TV) {
        /** 90 and 270 degree rotation is not applicable for TV
        * hence reset the rotation to 0 deg if the panel is TV
        * 180 degree rotation is required to handle mirroring scenario
        */
        if (data->rotation != 180) {
            rotation_degree = 0;
        }
        transparency_type = EGRAPHICS_DESTINATION;
        videopipezorder = 1;
        transkey = 0;
    }

    if ((ret = v4l2_overlay_set_rotation(fd, rotation_degree, 0, data->mirror))) {
        LOGE("Set Rotation Failed!/%d\n", ret);
        goto end;
    }

    calculateDisplayMetaData(overlayobj, stage->panel);
    if (data->panel != stage->panel) {
        LOGD("data->panel/0x%x / stage->panel/0x%x\n", data->panel, stage->panel );
        data->panel = stage->panel;

        LOGD("Panel path [%s]", screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displayenabled);
        LOGD("Manager display [%s]", managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managerdisplay);
        LOGD("Manager path [%s]", overlayobj->overlaymanagerpath);
        LOGD("Manager name [%s]", managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managername);
        LOGD("Display name [%s]", screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displayname);
        if (overlayobj->mDisplayMetaData.mTobeDisabledPanelIndex != -1) {
            char disablepanelpath[PATH_MAX];
            sprintf(disablepanelpath, "/sys/devices/platform/omapdss/display%d/enabled", \
                overlayobj->mDisplayMetaData.mTobeDisabledPanelIndex);
            if (sysfile_write(disablepanelpath, "0", sizeof("0")) < 0) {
                LOGE("panel disable failed");
                ret = -1;
                goto end;
            }
        }

        if (!overlayobj->mData.s3d_active) {
            if (sysfile_write(overlayobj->overlayenabled, "0", sizeof("0")) < 0) {
                LOGE("Failed: overlay Disable");
                ret = -1;
                goto end;
            }

            /* Set the manager to the overlay */
            if (sysfile_read(overlayobj->overlaymanagerpath, &overlaymanagername, PATH_MAX) < 0) {
                LOGE("Overlay manager Name Get failed [%d]", __LINE__);
                ret = -1;
                goto end;
            }
            strtok(overlaymanagername, "\n");
            LOGI("Before assigning Overlay-Manager to Overlay");
            LOGI("overlaymanagerpath= %s, overlaymanagername= %s",overlayobj->overlaymanagerpath,overlaymanagername);

            /**
             * Before setting the manager, reset the overlay window to the default
             * this is to support the panels of various timings. The Actual window
             * is set after manager is set.
             * Assumption# The lowest resolution panel is QQVGA
             */
            if ((ret = v4l2_overlay_set_position(fd, 0, 0, QQVGA_WIDTH, QQVGA_HEIGHT))) {
                LOGE("Set Position Failed!/%d\n", ret);
                goto end;
            }

            if (sysfile_write(overlayobj->overlaymanagerpath, \
                managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managername, PATH_MAX) < 0) {
                LOGE("Unable to set the overlay->manager");
                ret = -1;
                goto end;
            }

            if (sysfile_read(overlayobj->overlaymanagerpath, &overlaymanagername, PATH_MAX) < 0) {
                LOGE("Overlay manager Name Get failed [%d]", __LINE__);
                ret = -1;
                goto end;
            }
            strtok(overlaymanagername, "\n");
            LOGI("After assigning  Overlay-Manager to Overlay");
            LOGI("overlaymanagerpath= %s, overlaymanagername= %s",overlayobj->overlaymanagerpath,overlaymanagername);

#ifdef TARGET_OMAP4

            /** Set the manager display to the panel*/
            if (sysfile_write(managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managerdisplay, \
                screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displayname, PATH_MAX) < 0) {
                LOGE("Unable to set the manager->display");
                ret = -1;
                goto end;
            }
#else
            char displaytimings[PATH_MAX];
            sprintf(displaytimings, "/sys/devices/platform/omapdss/display%d/timings", overlayobj->mDisplayMetaData.mPanelIndex);
            if (!strcmp(managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managername, "tv")) {
                if (sysfile_write(displaytimings, "ntsc", sizeof("ntsc")) < 0) {
                    LOGE("Setting display timings failed");
                    ret = -1;
                    goto end;
                }
            }
#endif

            // Enable the requested panel here
            if (sysfile_write(screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displayenabled, "1", sizeof("1")) < 0) {
                LOGE("Panel enable failed");
                ret = -1;
                goto end;
            }
        } else {
            //Currently need to disable streaming to change display id
            ret = v4l2_overlay_set_display_id(fd, data->panel);
            if(ret)
                LOGE("failed to set display ID\n");

            ret = v4l2_overlay_set_s3d_mode(fd, overlayobj->mData.s3d_mode);
            if(ret)
                LOGE("failed to reset S3D mode\n");
        }

#ifdef TARGET_OMAP4
        //Read timings after display has been enabled as they could have changed
        char displaytimingspath[PATH_MAX];
        sprintf(displaytimingspath, "/sys/devices/platform/omapdss/display%d/timings",
                overlayobj->mDisplayMetaData.mPanelIndex);
        if (sysfile_read(displaytimingspath,
            (void*)(&screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displaytimings), PATH_MAX) < 0) {
            LOGE("display get timings failed");
            ret = -1;
            goto end;
        }
        LOGD("Display timings [%s]\n", screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displaytimings);
#endif
    }
    //Calculate window size. As of now this is applicable only for non-LCD panels
    calculateWindow(overlayobj, &finalWindow, stage->panel);

    LOGI("Position/X%d/Y%d/W%d/H%d/R%d/A%d/Z%d\n", data->posX, data->posY, data->posW, data->posH,
            data->rotation, data->alpha, data->zorder);
    LOGI("Adjusted Position/X%d/Y%d/W%d/H%d\n", finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH);

    if ((ret = v4l2_overlay_set_position(fd, finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH))) {
        LOGE("Set Position Failed!/%d\n", ret);
        goto end;
    }

#if defined (TARGET_OMAP4)
    if (transkey < 0) {
        if ((ret = v4l2_overlay_set_colorkey(fd, 0, 0x00, EVIDEO_SOURCE))) {
            LOGE("Failed enabling color key\n");
            goto end;
        }
    }
    else {
        if ((ret = v4l2_overlay_set_colorkey(fd, 1, transkey, transparency_type))) {
            LOGE("Failed enabling color key\n");
            goto end;
        }
    }
    //Currently not supported with V4L2_S3D driver
    if (!overlayobj->mData.s3d_active) {
        /*zOrder is assigned at the creation of overlay and removed at the destruction.
        *no need to assign again for LCD manager
        * But for HDMI manager, inorder to support UI+Video together, the z-order values are fixed,
        * Hence configure zOrder again if the manager is TV
        */
        if (!strcmp(overlaymanagername, "tv")) {
            if ((ret = v4l2_overlay_set_zorder(fd, videopipezorder))) {
                LOGE("Failed setting zorder\n");
                goto end;
            }
        }
    }
#elif defined (TARGET_OMAP3)
    if(data->colorkey < 0) {
        // Request to Enable Local Alpha Blending
        if ((ret = v4l2_overlay_set_colorkey(fd, 0, 0x00, EVIDEO_SOURCE))) {
            LOGE("Failed enabling color key\n");
            goto end;
        }

        if ((ret = v4l2_overlay_set_local_alpha(fd,1))) {
            LOGE("Failed enabling local alpha \n");
            goto end;
        }
    } else {
        //  Request to Enable Color Key
        if ((ret = v4l2_overlay_set_local_alpha(fd,0))) {
            LOGE("Failed disabling local alpha \n");
            goto end;
        }

        if ((ret = v4l2_overlay_set_colorkey(fd, 1, 0x00, EVIDEO_SOURCE))) {
            LOGE("Failed enabling color key\n");
            goto end;
        }
    }
#endif

    if (overlayobj->getctrl_linkvideofd() > 0) {
        CommitLinkDevice(dev, overlayobj);
    }

end:
    pthread_mutex_unlock(&overlayobj->lock);
    LOG_FUNCTION_NAME_EXIT
    return ret;

}

int overlay_control_context_t::CommitLinkDevice(struct overlay_control_device_t *dev,
                          overlay_object* overlayobj) {
    LOG_FUNCTION_NAME_ENTRY
    if ((dev == NULL) || (overlayobj == NULL)) {
        LOGE("Null Arguments / Overlay not initd");
        return -1;
    }

    overlay_control_context_t *self = (overlay_control_context_t *)dev;
    int pipelineId = -1;
    overlay_ctrl_t   *data   = overlayobj->data();
    int ret = 0;
    overlay_ctrl_t finalWindow;
    int linkfd = overlayobj->getctrl_linkvideofd();

    calculateDisplayMetaData(overlayobj, KCloneDevice);

    // Disable streaming to ensure that stream_on is called again which indirectly sets overlayenabled to 1
    if ((ret = overlay_data_context_t::disable_streaming_locked(overlayobj, false))) {
        LOGE("Stream Off Failed!/%d\n", ret);
        goto end;
    }

    if ((ret = v4l2_overlay_set_rotation(linkfd, data->rotation, 0, data->mirror))) {
        LOGE("Set Rotation Failed!/%d\n", ret);
        goto end;
    }

    //Calculate window size. As of now this is applicable only for non-LCD panels
    calculateWindow(overlayobj, &finalWindow, KCloneDevice);

    LOGI("Link Position/X%d/Y%d/W%d/H%d\n", data->posX, data->posY, data->posW, data->posH );
    LOGI("Link Adjusted Position/X%d/Y%d/W%d/H%d\n", finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH);
    LOGI("Rotation/%d\n", data->rotation );
    LOGI("alpha/%d\n", data->alpha );
    LOGI("zorder/%d\n", data->zorder );

    v4l2_overlay_getId(linkfd, &pipelineId);

    if ((pipelineId >= 0) && (pipelineId <= MAX_NUM_OVERLAYS)) {
        char overlaymanagerpath[PATH_MAX];
        sprintf(overlaymanagerpath, "/sys/devices/platform/omapdss/overlay%d/manager", pipelineId);
        sysfile_write(overlaymanagerpath, managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managername,\
                                          sizeof("2lcd"));
    }

    if ((ret = v4l2_overlay_set_position(linkfd, finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH))) {
        LOGE("Set Position Failed!/%d\n", ret);
        goto end;
    }

    if (data->colorkey < 0) {
        if ((ret = v4l2_overlay_set_colorkey(linkfd, 0, 0x00, EVIDEO_SOURCE))) {
            LOGE("Failed enabling color key\n");
            goto end;
        }
    }
    else {
        if ((ret = v4l2_overlay_set_colorkey(linkfd, 1, data->colorkey, EVIDEO_SOURCE))) {
            LOGE("Failed enabling color key\n");
            goto end;
        }
    }

//zOrder is assigned at the creation of overlay and removed at the destruction.
//no need to assign again v4l2_overlay_set_zorder(linkfd, data->zorder)

end:
    LOG_FUNCTION_NAME_EXIT
    return ret;
}

int overlay_control_context_t::overlay_control_close(struct hw_device_t *dev)
{
    LOG_FUNCTION_NAME_ENTRY
    if (dev == NULL) {
        LOGE("Null Arguments");
        return -1;
    }

    struct overlay_control_context_t* self = (struct overlay_control_context_t*)dev;

    if (self) {
        for (int i = 0; i < MAX_NUM_OVERLAYS; i++) {
            if (self->mOmapOverlays[i]) {
                overlay_destroyOverlay((struct overlay_control_device_t *)self, (overlay_t*)(self->mOmapOverlays[i]));
            }
        }

        free(self);
        TheOverlayControlDevice = NULL;
    }
    return 0;
}

// ****************************************************************************
// Data module
// ****************************************************************************

int overlay_data_context_t::overlay_initialize(struct overlay_data_device_t *dev,
                       overlay_handle_t handle)
{
    LOG_FUNCTION_NAME_ENTRY
    if (dev == NULL) {
        LOGE("Null Arguments ");
        return -1;
    }

    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    struct stat stat;

    int i;
    int rc = -1;

    int ovlyfd = static_cast<const struct handle_t *>(handle)->overlayobj_sharedfd;
    int size = static_cast<const struct handle_t *>(handle)->overlayobj_size;
    int video_fd = static_cast<const struct handle_t *>(handle)->video_fd;
    overlay_object* overlayobj = overlay_control_context_t::open_shared_overlayobj(ovlyfd, size);
    if (overlayobj == NULL) {
        LOGE("Overlay Initialization failed");
        return -1;
    }

    ctx->omap_overlay = overlayobj;
    overlayobj->mDataHandle.video_fd = video_fd;
    overlayobj->mDataHandle.overlayobj_sharedfd = ovlyfd;
    overlayobj->mDataHandle.overlayobj_size = size;

    ctx->omap_overlay->cacheable_buffers = 0;
    ctx->omap_overlay->maintain_coherency = 1;
    ctx->omap_overlay->optimalQBufCnt = NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE;
    ctx->omap_overlay->attributes_changed = 0;

    ctx->omap_overlay->mData.cropX = 0;
    ctx->omap_overlay->mData.cropY = 0;
    ctx->omap_overlay->mData.cropW = overlayobj->w;
    ctx->omap_overlay->mData.cropH = overlayobj->h;

    //S3D Default value set from createOverlay
    ctx->omap_overlay->mData.s3d_mode = overlayobj->mData.s3d_mode;
    ctx->omap_overlay->mData.s3d_fmt = overlayobj->mData.s3d_fmt;
    ctx->omap_overlay->mData.s3d_order = overlayobj->mData.s3d_order;
    ctx->omap_overlay->mData.s3d_subsampling = overlayobj->mData.s3d_subsampling;

    if (fstat(video_fd, &stat)) {
           LOGE("Error = %s from %s\n", strerror(errno), "overlay initialize");
           return -1;
       }

    LOGD("Num of Buffers = %d", ctx->omap_overlay->num_buffers);

    ctx->omap_overlay->dataReady = 0;
    ctx->omap_overlay->qd_buf_count = 0;

    ctx->omap_overlay->mapping_data = new mapping_data_t;
    ctx->omap_overlay->buffers     = new void* [NUM_OVERLAY_BUFFERS_MAX];
    ctx->omap_overlay->buffers_len = new size_t[NUM_OVERLAY_BUFFERS_MAX];
    if (!ctx->omap_overlay->buffers || !ctx->omap_overlay->buffers_len || !ctx->omap_overlay->mapping_data) {
            LOGE("Failed alloc'ing buffer arrays\n");
            overlay_control_context_t::close_shared_overlayobj(ctx->omap_overlay);
    } else {
        for (i = 0; i < ctx->omap_overlay->num_buffers; i++) {
            rc = v4l2_overlay_map_buf(video_fd, i, &ctx->omap_overlay->buffers[i],
                                       &ctx->omap_overlay->buffers_len[i]);
            if (rc) {
                LOGE("Failed mapping buffers\n");
                overlay_control_context_t::close_shared_overlayobj( ctx->omap_overlay );
                break;
            }
        }
    }
    ctx->omap_overlay->mappedbufcount = ctx->omap_overlay->num_buffers;
    LOG_FUNCTION_NAME_EXIT
    LOGV("Initialize ret = %d", rc);
    InitDisplayManagerMetaData();
    return ( rc );
}

int overlay_data_context_t::overlay_resizeInput(struct overlay_data_device_t *dev, uint32_t w, uint32_t h)
{
    LOGD("overlay_resizeInput %dx%d %d", (int)w, (int)h, (int) ((struct overlay_data_context_t*)dev)->omap_overlay->num_buffers);
    LOG_FUNCTION_NAME_ENTRY
    if (dev == NULL) {
        LOGE("Null Arguments ");
        return -1;
    }

    int ret = 0;
    int degree = 0;
    int link_fd = -1;
    uint32_t mirror = 0;

    // Position and output width and heigh
    int32_t _x = 0;
    int32_t _y = 0;
    int32_t _w = 0;
    int32_t _h = 0;

    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    if (!ctx->omap_overlay) {
        LOGI("overlay Not Init'd!\n");
        return -1;
    }
    /* Validate the width and height are within a valid range for the
     * video driver.
     */
    if (w > MAX_OVERLAY_WIDTH_VAL ||
         h > MAX_OVERLAY_HEIGHT_VAL ||
         w*h >MAX_OVERLAY_RESOLUTION) {
       LOGE("%d: Error - Currently unsupported settings width %d, height %d", __LINE__, w, h);
       return -1;
    }
    if (w <= 0 || h <= 0){
       LOGE("%d: Error - Invalid settings width %d, height %d",
       __LINE__, w, h);
       return -1;
    }

    int fd = ctx->omap_overlay->getdata_videofd();
    overlay_ctrl_t *stage = ctx->omap_overlay->staging();
    link_fd = ctx->omap_overlay->getdata_linkvideofd();

    pthread_mutex_lock(&ctx->omap_overlay->lock);
    ret = ctx->disable_streaming_locked(ctx->omap_overlay);

    if ((ctx->omap_overlay->w == (unsigned int)w) && (ctx->omap_overlay->h == (unsigned int)h) && (ctx->omap_overlay->attributes_changed == 0)){
        LOGE("Same as current width and height. Attributes did not change either. So do nothing.");
        //Lets reset the statemachine and disable the stream
        ctx->omap_overlay->dataReady = 0;
        goto end;
    }

    if (ret)
        goto end;

    if ((ret = v4l2_overlay_get_position(fd, &_x,  &_y, &_w, &_h))) {
        LOGD(" Could not get the position when resizing overlay \n");
        goto end;
    }

    if ((ret = v4l2_overlay_get_rotation(fd, &degree, NULL, &mirror))) {
        LOGD("Get rotation value failed! \n");
        goto end;
    }

    for (int i = 0; i < ctx->omap_overlay->mappedbufcount; i++) {
        v4l2_overlay_unmap_buf(ctx->omap_overlay->buffers[i], ctx->omap_overlay->buffers_len[i]);
    }

    if ((ret = v4l2_overlay_init(fd, w, h, ctx->omap_overlay->format))) {
        LOGE("Error initializing overlay");
        goto end;
    }

    //Update the overlay object with the new width and height
    ctx->omap_overlay->w = w;
    ctx->omap_overlay->h = h;

    ctx->omap_overlay->mData.cropX = 0;
    ctx->omap_overlay->mData.cropY = 0;
    ctx->omap_overlay->mData.cropW = w;
    ctx->omap_overlay->mData.cropH = h;

    if ((ret = v4l2_overlay_set_rotation(fd, degree, 0, mirror))) {
        LOGE("Failed rotation\n");
        goto end;
    }

    if ((ret = v4l2_overlay_set_crop(fd, ctx->omap_overlay->mData.cropX, ctx->omap_overlay->mData.cropY, \
                                    ctx->omap_overlay->mData.cropW, ctx->omap_overlay->mData.cropH))) {
        LOGE("Failed crop window\n");
        goto end;
    }

    if ((ret = v4l2_overlay_set_position(fd, _x, _y, _w, _h))) {
        LOGD(" Could not set the position when resizing overlay \n");
        goto end;
    }

    if ((ret = v4l2_overlay_req_buf(fd, (uint32_t *)(&ctx->omap_overlay->num_buffers),
            ctx->omap_overlay->cacheable_buffers, ctx->omap_overlay->maintain_coherency, EMEMORY_MMAP))) {
        LOGE("Error creating buffers");
        goto end;
    }

    if (link_fd > 0) {
        if ((ret = v4l2_overlay_init(link_fd, w, h, ctx->omap_overlay->format))) {
            LOGE("Error resizing link overlay");
            goto end;
        }

        if ((ret = v4l2_overlay_set_rotation(link_fd, degree, 0, mirror))) {
            LOGE("Failed set rotation for link\n");
            goto end;
        }

        if ((ret = v4l2_overlay_set_crop(link_fd, ctx->omap_overlay->mData.cropX, ctx->omap_overlay->mData.cropY, \
                                 ctx->omap_overlay->mData.cropW, ctx->omap_overlay->mData.cropH))) {
            LOGE("Failed set crop window for link\n");
            goto end;
        }

        if ((ret = v4l2_overlay_set_position(link_fd, 0,  0, LCD_WIDTH, LCD_HEIGHT))) {
            LOGE(" Could not set the position for link \n");
            goto end;
        }

        if ((ret = v4l2_overlay_req_buf(link_fd, (uint32_t *)(&ctx->omap_overlay->num_buffers),
            ctx->omap_overlay->cacheable_buffers, ctx->omap_overlay->maintain_coherency, EMEMORY_USRPTR))) {
            LOGE("Error creating linked buffers2");
            goto end;
        }
    }

    for (int i = 0; i < ctx->omap_overlay->num_buffers; i++) {
        v4l2_overlay_map_buf(fd, i, &ctx->omap_overlay->buffers[i], &ctx->omap_overlay->buffers_len[i]);
    }

    ctx->omap_overlay->mappedbufcount = ctx->omap_overlay->num_buffers;
    ctx->omap_overlay->dataReady = 0;

    /* The control pameters just got set */
    ctx->omap_overlay->controlReady = 1;
    ctx->omap_overlay->attributes_changed = 0; // Reset it

    LOG_FUNCTION_NAME_EXIT
end:

    pthread_mutex_unlock(&ctx->omap_overlay->lock);
    return ret;
}

int overlay_data_context_t::overlay_data_setParameter(struct overlay_data_device_t *dev,
                                     int param, int value)
{
    LOG_FUNCTION_NAME_ENTRY
    if (dev == NULL) {
        LOGE("Null Arguments ");
        return -1;
    }

    int ret = 0;
    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;

    if ( ctx->omap_overlay == NULL ) {
        LOGI("Overlay Not Init'd!\n");
        return -1;
    }

    switch(param) {
    case CACHEABLE_BUFFERS:
        ctx->omap_overlay->cacheable_buffers = value;
        ctx->omap_overlay->attributes_changed = 1;
        break;
    case MAINTAIN_COHERENCY:
        ctx->omap_overlay->maintain_coherency = value;
        ctx->omap_overlay->attributes_changed = 1;
        break;
    case OPTIMAL_QBUF_CNT:
        if (value < 0) {
            LOGE("InValid number of optimal quebufffers requested[%d]", value);
            return -1;
        }
        ctx->omap_overlay->optimalQBufCnt = MIN(value, NUM_OVERLAY_BUFFERS_MAX);

        break;
    case OVERLAY_NUM_BUFFERS:
        if (value <= 0) {
            LOGE("InValid number of Overlay bufffers requested[%d]", value);
            return -1;
        }
        ctx->omap_overlay->num_buffers = MIN(value, NUM_OVERLAY_BUFFERS_MAX);
        ctx->omap_overlay->attributes_changed = 1;
        break;

    case SET_CLONE_FD:
        if (value <= 0) {
            int linkfd = ctx->omap_overlay->getdata_linkvideofd();
            if (linkfd >= 0) {
                ctx->omap_overlay->setdata_linkvideofd(-1);
                close(linkfd);
            }
            return 0;
        }
        ctx->omap_overlay->setdata_linkvideofd(value);
    }

    LOG_FUNCTION_NAME_EXIT
    return ( ret );
}

int overlay_data_context_t::overlay_set_s3d_params(struct overlay_data_device_t *dev, uint32_t s3d_mode,
                           uint32_t s3d_fmt, uint32_t s3d_order, uint32_t s3d_subsampling) {

    //LOG_FUNCTION_NAME_ENTRY
    if (dev == NULL) {
        LOGE("Null Arguments[%d]", __LINE__);
        return -1;
    }

    if ((OVERLAY_S3D_MODE_ANAGLYPH  < (int)s3d_mode || ((int)s3d_mode < OVERLAY_S3D_MODE_OFF)) ||
        (OVERLAY_S3D_FORMAT_FRM_SEQ < (int)s3d_fmt || ((int)s3d_fmt < OVERLAY_S3D_FORMAT_NONE)) ||
        (OVERLAY_S3D_ORDER_RF < (int)s3d_order || ((int)s3d_order < OVERLAY_S3D_ORDER_LF)) ||
        (OVERLAY_S3D_SS_VERT < (int)s3d_subsampling || ((int)s3d_subsampling < OVERLAY_S3D_SS_NONE))){
        LOGE("Invalid Arguments [%d]", __LINE__);
        return -1;
    }

    int ret = 0;
    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;

    if (ctx->omap_overlay == NULL) {
        LOGE("Shared Data Not Init'd!\n");
        return -1;
    }

    int fd = ctx->omap_overlay->getdata_videofd();
    pthread_mutex_lock(&ctx->omap_overlay->lock);

    if (ctx->omap_overlay->mData.s3d_mode != s3d_mode)
    {
        if ((ret = v4l2_overlay_set_s3d_mode(fd, s3d_mode))) {
            LOGE("Set S3D mode Failed!/%d\n", ret);
            pthread_mutex_unlock(&ctx->omap_overlay->lock);
            return ret;
        }
        ctx->omap_overlay->mData.s3d_mode = s3d_mode;
        LOGI("[s3d_params functions] s3d mode/%d\n",  s3d_mode);
    }
 /*
    if ((ret = ctx->disable_streaming_locked(ctx->omap_overlay))) {
         LOGE("Disable stream Failed!/%d\n", ret);
        pthread_mutex_unlock(&ctx->omap_overlay->lock);
        return ret;
    }
    */

    if (ctx->omap_overlay->mData.s3d_fmt != s3d_fmt || ctx->omap_overlay->mData.s3d_order != s3d_order
        || ctx->omap_overlay->mData.s3d_subsampling != s3d_subsampling)
    {
        if ((ret = v4l2_overlay_set_s3d_format(fd, s3d_fmt, s3d_order,s3d_subsampling))) {
        LOGE("Set S3D format Failed!/%d\n", ret);
        pthread_mutex_unlock(&ctx->omap_overlay->lock);
        return ret;
        }
        ctx->omap_overlay->mData.s3d_fmt = s3d_fmt;
        ctx->omap_overlay->mData.s3d_order = s3d_order;
        ctx->omap_overlay->mData.s3d_subsampling = s3d_subsampling;
    }

    LOG_FUNCTION_NAME_EXIT

    pthread_mutex_unlock(&ctx->omap_overlay->lock);
    return ret;
}

int overlay_data_context_t::overlay_setCrop(struct overlay_data_device_t *dev, uint32_t x,
                           uint32_t y, uint32_t w, uint32_t h) {
    //LOG_FUNCTION_NAME_ENTRY
    //This print is commented as part of on-the-fly crop support
    if (dev == NULL) {
        LOGE("Null Arguments[%d]", __LINE__);
        return -1;
    }

    if (((int)x < 0)||((int)y < 0)||((int)w <= 0)||((int)h <= 0)||(w > MAX_OVERLAY_WIDTH_VAL)||(h > MAX_OVERLAY_HEIGHT_VAL)) {
        LOGE("CropW = %d cropH = %d, cropX = %d, cropY=%d", w, h, x, y);
        LOGE("Invalid Arguments [%d]", __LINE__);
        return -1;
    }

    int rc = 0;
    overlay_ctrl_t finalWindow;
    int _x, _y, _w, _h;
    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    if (ctx->omap_overlay == NULL) {
        LOGI("Shared Data Not Init'd!\n");
        return -1;
    }
    if (ctx->omap_overlay->mData.cropX == x && ctx->omap_overlay->mData.cropY == y &&
        ctx->omap_overlay->mData.cropW == w && ctx->omap_overlay->mData.cropH == h) {
        LOGV("Nothing to crop!\n");
        if (ctx->omap_overlay->getdata_linkvideofd() <= 0) {
            return 0;
        }
    }
    int fd = ctx->omap_overlay->getdata_videofd();
    int linkfd = ctx->omap_overlay->getdata_linkvideofd();

    pthread_mutex_lock(&ctx->omap_overlay->lock);

    ctx->omap_overlay->dataReady = 1;

    if ((rc = v4l2_overlay_get_position(fd, &_x,  &_y, &_w, &_h))) {
        LOGD(" Could not get the position when setting the crop \n");
        goto end;
    }
    finalWindow.posX = _x;
    finalWindow.posY = _y;
    finalWindow.posW = _w;
    finalWindow.posH = _h;

#ifndef TARGET_OMAP4
    if ((rc = ctx->disable_streaming_locked(ctx->omap_overlay))){
        goto end;
    }
    ctx->omap_overlay->mData.cropX = x;
    ctx->omap_overlay->mData.cropY = y;
    ctx->omap_overlay->mData.cropW = w;
    ctx->omap_overlay->mData.cropH = h;

    LOGD("Crop Win/X%d/Y%d/W%d/H%d\n", x, y, w, h );

    rc = v4l2_overlay_set_crop(fd, x, y, w, h);
    if (rc) {
        LOGE("Set Crop Window Failed!/%d\n", rc);
    }

    overlay_control_context_t::calculateDisplayMetaData(ctx->omap_overlay, ctx->omap_overlay->data()->panel);
    overlay_control_context_t::calculateWindow(ctx->omap_overlay, &finalWindow, ctx->omap_overlay->data()->panel, false);

    if ((rc = v4l2_overlay_set_position(fd, finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH))) {
        LOGD(" Could not set the position when setting the crop \n");
        goto end;
    }
#else
    if((ctx->omap_overlay->mData.cropW != w) ||(ctx->omap_overlay->mData.cropH != h)) {
        /** Since setCrop is called with crop window change, hence disable the stream first
        * and set the final window as well
        */
        if ((rc = ctx->disable_streaming_locked(ctx->omap_overlay))){
            goto end;
        }
        ctx->omap_overlay->mData.cropX = x;
        ctx->omap_overlay->mData.cropY = y;
        ctx->omap_overlay->mData.cropW = w;
        ctx->omap_overlay->mData.cropH = h;

        LOGD("Crop Win/X%d/Y%d/W%d/H%d\n", x, y, w, h );
        rc = v4l2_overlay_set_crop(fd, x, y, w, h);
        if (rc) {
            LOGE("Set Crop Window Failed!/%d\n", rc);
            goto end;
        }

        overlay_control_context_t::calculateDisplayMetaData(ctx->omap_overlay, ctx->omap_overlay->data()->panel);
        overlay_control_context_t::calculateWindow(ctx->omap_overlay, &finalWindow, ctx->omap_overlay->data()->panel, false);

        if ((rc = v4l2_overlay_set_position(fd, finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH))) {
            LOGD(" Could not set the position when setting the crop \n");
            goto end;
        }

        if (linkfd > 0) {
            rc = v4l2_overlay_set_crop(linkfd, x, y, w, h);
            if (rc) {
                LOGE("LINK: Set Crop Window Failed!/%d\n", rc);
                goto end;
            }

            overlay_control_context_t::calculateDisplayMetaData(ctx->omap_overlay, KCloneDevice);
            overlay_control_context_t::calculateWindow(ctx->omap_overlay, &finalWindow, KCloneDevice, false);

            if ((rc = v4l2_overlay_set_position(linkfd, finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH))) {
                LOGD(" LINK: Could not set the position when setting the crop \n");
                goto end;
            }
        }
    }
    else {
        /** setCrop is called only with the crop co-ordinates change
        * this is allowed on-the-fly. Hence no need of disabling the stream and
        * the final window remains the same
        */
        ctx->omap_overlay->mData.cropX = x;
        ctx->omap_overlay->mData.cropY = y;
        ctx->omap_overlay->mData.cropW = w;
        ctx->omap_overlay->mData.cropH = h;

        rc = v4l2_overlay_set_crop(fd, x, y, w, h);
        if (rc) {
            LOGE("Set Crop Window Failed!/%d\n", rc);
        }
        if (linkfd > 0) {
            rc = v4l2_overlay_set_crop(linkfd, x, y, w, h);
            if (rc) {
                LOGE("Set Crop Window Failed!/%d\n", rc);
            }
        }
    }
#endif

end:
    pthread_mutex_unlock(&ctx->omap_overlay->lock);
    return rc;

}

int overlay_data_context_t::overlay_getCrop(struct overlay_data_device_t *dev , uint32_t* x,
                           uint32_t* y, uint32_t* w, uint32_t* h) {
    if (dev == NULL) {
        LOGE("Null Arguments ");
        return -1;
    }
    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    int fd = ctx->omap_overlay->getdata_videofd();
    return v4l2_overlay_get_crop(fd, x, y, w, h);
}

int overlay_data_context_t::overlay_dequeueBuffer(struct overlay_data_device_t *dev,
                          overlay_buffer_t *buffer) {
    /* blocks until a buffer is available and return an opaque structure
     * representing this buffer.
     */
    if ((dev == NULL)||(buffer == NULL)) {
        LOGE("Null Arguments ");
        return -1;
    }

    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    int fd = ctx->omap_overlay->getdata_videofd();
    int linkfd = ctx->omap_overlay->getdata_linkvideofd();
    int rc;
    int rc1;
    int i = -1;
    int ii = -1;

    pthread_mutex_lock(&ctx->omap_overlay->lock);
    if (ctx->omap_overlay->streamEn == 0) {
        LOGE("Cannot dequeue when streaming is disabled. ctx->omap_overlay->qd_buf_count = %d", ctx->omap_overlay->qd_buf_count);
        rc = -EPERM;
    }

    else if ( ctx->omap_overlay->qd_buf_count < ctx->omap_overlay->optimalQBufCnt ) {
        LOGV("Queue more buffers before attempting to dequeue!");
        rc = -EPERM;
    }

    else if ( (rc = v4l2_overlay_dq_buf(fd, &i, EMEMORY_MMAP, NULL, 0 )) != 0 ) {
        LOGE("Failed to DQ/%d\n", rc);
       //in order to recover from DQ failure scenario, let's disable the stream.
       //the stream gets re-enabled in the subsequent Q buffer call
       //if streamoff also fails!!! just return the errorcode to the client
       rc = disable_streaming_locked(ctx->omap_overlay, true);
       if (rc == 0) { rc = -1; } //this is required for TIHardwareRenderer
    }
    else if ( i < 0 || i > ctx->omap_overlay->num_buffers ) {
        LOGE("dqbuffer i=%d",i);
        rc = -EPERM;
    }
    else {
        *((int *)buffer) = i;
        ctx->omap_overlay->qd_buf_count --;
        LOGV("INDEX DEQUEUE = %d", i);
        LOGV("qd_buf_count --");
    }

    if ((linkfd > 0) && (rc == 0)) {
        if ( (rc1 = v4l2_overlay_dq_buf(linkfd, &ii, EMEMORY_USRPTR, ctx->omap_overlay->buffers[i],
            ctx->omap_overlay->mapping_data->length)) != 0 ) {
            LOGE("Failed to DQ link/%d\n", rc1);
        }
    }

    LOGV("qd_buf_count = %d", ctx->omap_overlay->qd_buf_count);

    pthread_mutex_unlock(&ctx->omap_overlay->lock);

    return ( rc );
}

#ifdef __FILE_DUMP__
static int noofbuffer = 0;
#endif
int overlay_data_context_t::overlay_queueBuffer(struct overlay_data_device_t *dev,
                        overlay_buffer_t buffer) {

    if (dev == NULL) {
        LOGE("Null Arguments ");
        return -1;
    }

    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    if (((int)buffer < 0)||((int)buffer >= ctx->omap_overlay->num_buffers)) {
        LOGE("Invalid buffer Index [%d]@[%d]", (int)buffer, __LINE__);
        return -1;
    }
    int fd = ctx->omap_overlay->getdata_videofd();
    int linkfd = ctx->omap_overlay->getdata_linkvideofd();

    if ( !ctx->omap_overlay->controlReady ) {
        LOGI("Control not ready but queue buffer requested!!!\n");
    }

#ifdef __FILE_DUMP__
   if(noofbuffer > 10) {
    for(int i=0 ; i < 10 ; i++) {
    Util_Memcpy_2Dto1D(ctx->buffers[i], ctx->height, ctx->width, 4096);
    }
    noofbuffer = 0;
   }
   noofbuffer++;
#endif

    pthread_mutex_lock(&ctx->omap_overlay->lock);

    int rc = v4l2_overlay_q_buf(fd, (int)buffer, EMEMORY_MMAP, NULL, 0);
    if (rc < 0) {
        LOGD("queueBuffer failed. rc = %d", rc);
        rc = -EPERM;
        goto EXIT;
    }

    if (linkfd > 0) {
        rc = v4l2_overlay_q_buf(linkfd, (int)buffer, EMEMORY_USRPTR, ctx->omap_overlay->buffers[(int)buffer],
                  ctx->omap_overlay->mapping_data->length);
        if (rc < 0) {
            LOGE("link queueBuffer failed. rc = %d", rc);
            rc = 0;
        }
    }

    if (ctx->omap_overlay->qd_buf_count < ctx->omap_overlay->num_buffers && rc == 0) {
        ctx->omap_overlay->qd_buf_count ++;
    }

    if (ctx->omap_overlay->streamEn == 0) {
        /*DSS2: 2 buffers need to be queue before enable streaming*/
        ctx->omap_overlay->dataReady = 1;
        rc = ctx->enable_streaming_locked(ctx->omap_overlay);
    }
    if (rc == 0) {
        rc = ctx->omap_overlay->qd_buf_count;
    }

EXIT:
    pthread_mutex_unlock(&ctx->omap_overlay->lock);
    return ( rc );
}

void* overlay_data_context_t::overlay_getBufferAddress(struct overlay_data_device_t *dev,
                               overlay_buffer_t buffer)
{
    if (dev == NULL) {
        LOGE("Null Arguments ");
        return NULL;
    }

    int ret;
    struct v4l2_buffer buf;
    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    if (((int)buffer < 0)||((int)buffer >= ctx->omap_overlay->num_buffers)) {
        LOGE("Invalid buffer Index [%d]@[%d]", (int)buffer, __LINE__);
        return NULL;
    }
    int fd = ctx->omap_overlay->getdata_videofd();
    ret = v4l2_overlay_query_buffer(fd, (int)buffer, &buf);
    if (ret)
        return NULL;

    // Initialize ctx->mapping_data
    memset(ctx->omap_overlay->mapping_data, 0, sizeof(mapping_data_t));

    ctx->omap_overlay->mapping_data->fd = fd;
    ctx->omap_overlay->mapping_data->length = buf.length;
    ctx->omap_overlay->mapping_data->offset = buf.m.offset;
    ctx->omap_overlay->mapping_data->ptr = NULL;

    if ((int)buffer >= 0 && (int)buffer < ctx->omap_overlay->num_buffers) {
        ctx->omap_overlay->mapping_data->ptr = ctx->omap_overlay->buffers[(int)buffer];
        LOGV("Buffer/%d/addr=%08lx/len=%d \n", (int)buffer, (unsigned long)ctx->omap_overlay->mapping_data->ptr,
             ctx->omap_overlay->buffers_len[(int)buffer]);
    }

    return (void *)ctx->omap_overlay->mapping_data;

}

int overlay_data_context_t::overlay_getBufferCount(struct overlay_data_device_t *dev)
{
    if (dev == NULL) {
        LOGE("Null Arguments ");
        return -1;
    }
    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    return (ctx->omap_overlay->num_buffers);
}

int overlay_data_context_t::overlay_data_close(struct hw_device_t *dev) {

    LOG_FUNCTION_NAME_ENTRY
    if (dev == NULL) {
        LOGE("Null Arguments ");
        return -1;
    }

    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    int rc;

    if (ctx) {
        int buf;
        int i;

        pthread_mutex_lock(&ctx->omap_overlay->lock);

        if ((rc = (ctx->disable_streaming_locked(ctx->omap_overlay)))) {
            LOGE("Stream Off Failed!/%d\n", rc);
        }

        for (i = 0; i < ctx->omap_overlay->mappedbufcount; i++) {
            LOGV("Unmap Buffer/%d/%08lx/%d", i, (unsigned long)ctx->omap_overlay->buffers[i], ctx->omap_overlay->buffers_len[i] );
            rc = v4l2_overlay_unmap_buf(ctx->omap_overlay->buffers[i], ctx->omap_overlay->buffers_len[i]);
            if (rc != 0) {
                LOGE("Error unmapping the buffer/%d/%d", i, rc);
            }
        }
        delete (ctx->omap_overlay->mapping_data);
        delete [](ctx->omap_overlay->buffers);
        delete [](ctx->omap_overlay->buffers_len);
        ctx->omap_overlay->mapping_data = NULL;
        ctx->omap_overlay->buffers = NULL;
        ctx->omap_overlay->buffers_len = NULL;

        int linkfd = ctx->omap_overlay->getdata_linkvideofd();
        if (linkfd >= 0) {
            close(linkfd);
        }

        pthread_mutex_unlock(&ctx->omap_overlay->lock);

        ctx->omap_overlay->dataReady = 0;
        overlay_control_context_t::close_shared_overlayobj(ctx->omap_overlay);

        free(ctx);
    }

    return 0;
}

/*****************************************************************************/

static int overlay_device_open(const struct hw_module_t* module,
                               const char* name, struct hw_device_t** device)
{
    LOG_FUNCTION_NAME_ENTRY
    int status = -EINVAL;

    if (!strcmp(name, OVERLAY_HARDWARE_CONTROL)) {
        if (TheOverlayControlDevice != NULL) {
            LOGE("Control device is already Open");
            *device = &(TheOverlayControlDevice->common);
            return 0;
        }
        overlay_control_context_t *dev;
        dev = (overlay_control_context_t*)malloc(sizeof(*dev));
        if (dev){
        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->common.tag = HARDWARE_DEVICE_TAG;
        dev->common.version = 0;
        dev->common.module = const_cast<hw_module_t*>(module);
        dev->common.close = overlay_control_context_t::overlay_control_close;

        dev->get = overlay_control_context_t::overlay_get;
        dev->createOverlay = overlay_control_context_t::overlay_createOverlay;
        //S3D
        dev->createOverlay_S3D = overlay_control_context_t::overlay_createOverlay;
        dev->destroyOverlay = overlay_control_context_t::overlay_destroyOverlay;
        dev->setPosition = overlay_control_context_t::overlay_setPosition;
        dev->getPosition = overlay_control_context_t::overlay_getPosition;
        dev->setParameter = overlay_control_context_t::overlay_setParameter;
        dev->stage = overlay_control_context_t::overlay_stage;
        dev->commit = overlay_control_context_t::overlay_commit;
        //clone
        dev->requestOverlayClone = overlay_control_context_t::overlay_requestOverlayClone;

        *device = &dev->common;
        for (int i = 0; i < MAX_NUM_OVERLAYS; i++)
        {
            dev->mOmapOverlays[i] = NULL;
        }
        dev->mNumOverlays = 0;

        TheOverlayControlDevice = dev;

        status =  InitDisplayManagerMetaData();
    } else {
            /* Error: no memory available*/
            status = -ENOMEM;
    }

    } else if (!strcmp(name, OVERLAY_HARDWARE_DATA)) {
        overlay_data_context_t *dev;
        dev = (overlay_data_context_t*)malloc(sizeof(*dev));
        if(dev) {
        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->common.tag = HARDWARE_DEVICE_TAG;
        dev->common.version = 0;
        dev->common.module = const_cast<hw_module_t*>(module);
        dev->common.close = overlay_data_context_t::overlay_data_close;

        dev->initialize = overlay_data_context_t::overlay_initialize;
        dev->resizeInput = overlay_data_context_t::overlay_resizeInput;
        dev->setCrop = overlay_data_context_t::overlay_setCrop;
        dev->getCrop = overlay_data_context_t::overlay_getCrop;
        dev->setParameter = overlay_data_context_t::overlay_data_setParameter;
        dev->dequeueBuffer = overlay_data_context_t::overlay_dequeueBuffer;
        dev->queueBuffer = overlay_data_context_t::overlay_queueBuffer;
        dev->getBufferAddress = overlay_data_context_t::overlay_getBufferAddress;
        dev->getBufferCount = overlay_data_context_t::overlay_getBufferCount;
        //S3D
        dev->set_s3d_params = overlay_data_context_t::overlay_set_s3d_params;

        *device = &dev->common;
         status = 0;
    }else {
        /** Error: no memory available */
        status = - ENOMEM;
    }
  }

    LOG_FUNCTION_NAME_EXIT
    return status;
}


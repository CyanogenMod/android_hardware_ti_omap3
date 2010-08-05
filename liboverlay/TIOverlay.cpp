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

#ifdef TARGET_OMAP4
#define MAX_DISPLAY_CNT 4
#define MAX_MANAGER_CNT 3
#define PANEL_NAME_FOR_TV "hdmi"
#else
#define MAX_DISPLAY_CNT 2
#define MAX_MANAGER_CNT 2
#define PANEL_NAME_FOR_TV "tv"
#endif

static displayPanelMetaData screenMetaData[MAX_DISPLAY_CNT];
static displayManagerMetaData managerMetaData[MAX_MANAGER_CNT];

/*****************************************************************************/

#define LOG_FUNCTION_NAME_ENTRY    LOGD(" ###### Calling %s() ++ ######",  __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    LOGD(" ###### Calling %s() -- ######",  __FUNCTION__);

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

int sysfile_write(const char* pathname, const void* buf, size_t size) {
    int fd = open(pathname, O_RDWR);
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
    int fd = open(pathname, O_RDWR);
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
            LOGE("display get timings failed");
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


/**
* Precondition:
* This function has to be called after setting the crop window parameters
*/
void overlay_control_context_t::calculateWindow(overlay_object *overlayobj, overlay_ctrl_t *finalWindow)
{
    LOG_FUNCTION_NAME_ENTRY
    overlay_ctrl_t   *stage  = overlayobj->staging();
    /*
    * If default UI is on TV, android UI will receive 1920x1080 for screen size, and w & h will be 1920x1080 by default.
    * It will remain 1920x1080 even if overlay is requested on LCD. A better choice would be to query the display size:
    * and use the full resolution only for TV, for LCD lets respect whatever surface flinger asks for: this is required to
    * maintain the aspect ratio decided by media player
    */
    uint32_t dummy, w2, h2;
    if (sscanf(screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displaytimings, "%u,%u/%u/%u/%u,%u/%u/%u/%u\n",
        &dummy, &w2, &dummy, &dummy, &dummy, &h2, &dummy, &dummy, &dummy) != 9) {
        w2 = finalWindow->posW;
        h2 = finalWindow->posH; /* use default value, if could not read timings */
    }
    switch (stage->panel) {
        case OVERLAY_ON_PRIMARY:
        case OVERLAY_ON_SECONDARY:
        case OVERLAY_ON_PICODLP:
            LOGE("Nothing to Adjust");
            //This is  added as a place holder for future enhancements if any required
            break;
        case OVERLAY_ON_TV:
            {
                finalWindow->posX = 0;
                finalWindow->posY= 0;
#ifdef TARGET_OMAP4
                if (overlayobj->mData.cropW * h2 > w2 * overlayobj->mData.cropH) {
                    finalWindow->posW= w2;
                    finalWindow->posH= overlayobj->mData.cropH * w2 / overlayobj->mData.cropW;
                    finalWindow->posY = (h2 - finalWindow->posH) / 2;
                } else {
                    finalWindow->posH = h2;
                    finalWindow->posW = overlayobj->mData.cropW * h2 / overlayobj->mData.cropH;
                    finalWindow->posX = (w2 - finalWindow->posW) / 2;
                }
#else
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
            }
            break;
        default:
            LOGE("Leave the default  values");
        };
        LOG_FUNCTION_NAME_EXIT
}

void overlay_control_context_t::calculateDisplayMetaData(overlay_object *overlayobj)
{
    LOG_FUNCTION_NAME_ENTRY
    const char* paneltobeDisabled = NULL;
    static const char* panelname = "lcd";
    static const char* managername = "lcd";
    overlay_ctrl_t   *stage  = overlayobj->staging();

    switch(stage->panel){
        case OVERLAY_ON_PRIMARY: {
            LOGD("REQUEST FOR LCD1");
            panelname = "lcd";
            managername = "lcd";
        }
        break;
        case OVERLAY_ON_SECONDARY: {
            LOGD("REQUEST FOR LCD2");
            panelname = "2lcd";
            managername ="2lcd";
            paneltobeDisabled = "pico_DLP";
        }
        break;
        case OVERLAY_ON_TV: {
            LOGD("REQUEST FOR TV");
            panelname = PANEL_NAME_FOR_TV;
            managername = "tv";
        }
        break;
        case OVERLAY_ON_PICODLP: {
            LOGD("REQUEST FOR PICO DLP");
            panelname = "pico_DLP";
            managername = "2lcd";
            paneltobeDisabled = "2lcd";
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
        LOGD("Display id [%s]", screenMetaData[i].displayname);
        LOGD("dst id [%s]", panelname);
        if (strcmp(screenMetaData[i].displayname, panelname) == 0) {
            LOGD("found Panel Id @ [%d]", i);
            overlayobj->mDisplayMetaData.mPanelIndex = i;
            break;
        }
    }
    if (overlayobj->mDisplayMetaData.mPanelIndex == -1) {
        LOGE("The screenMetaData was not populated correctly. Could not find panel id. ");
    }

    overlayobj->mDisplayMetaData.mManagerIndex = -1;
    for (int i = 0; i < MAX_MANAGER_CNT; i++) {
        LOGD("managername name [%s]", managerMetaData[i].managername);
        LOGD("dst name [%s]", managername);

        if (strcmp(managerMetaData[i].managername, managername) == 0) {
        LOGD("found Display Manager @ [%d]", i);
        overlayobj->mDisplayMetaData.mManagerIndex = i;
        break;
        }
    }
    if (overlayobj->mDisplayMetaData.mManagerIndex == -1) {
        LOGE("The managerMetaData was not populated correctly. Could not find manager  id. ");
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
    int fd;
    if (isDatapath) {
        fd = overlayobj->getdata_videofd();
    }
    else {
        fd = overlayobj->getctrl_videofd();
    }
    rc = v4l2_overlay_stream_on(fd);
    if (rc) {
        LOGE("Stream Enable Failed!/%d\n", rc);
        overlayobj->streamEn = 0;
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
    LOGI("disable_streaming_locked");

    if (overlayobj->streamEn) {
        int fd;
        if (isDatapath) {
            fd = overlayobj->getdata_videofd();
        }
        else {
            fd = overlayobj->getctrl_videofd();
        }
        ret = v4l2_overlay_stream_off( fd );
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
                                        uint32_t w, uint32_t h, int32_t  format)
{
    LOGD("overlay_createOverlay:IN w=%d h=%d format=%d\n", w, h, format);
    LOG_FUNCTION_NAME_ENTRY;

    overlay_object            *overlayobj;
    overlay_control_context_t *self = (overlay_control_context_t *)dev;

    int ret;
    uint32_t num = NUM_OVERLAY_BUFFERS_REQUESTED;
    int fd;
    int overlay_fd;
    int pipelineId = 0;
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

    LOGD("Enabling the OVERLAY[%d]", overlayid);
    fd = v4l2_overlay_open(overlayid);
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

    if (v4l2_overlay_init(fd, w, h, format)) {
        LOGE("Failed initializing overlays\n");
        goto error1;
    }

    if (v4l2_overlay_set_crop(fd, 0, 0, w, h)) {
        LOGE("Failed defaulting crop window\n");
        goto error1;
    }

    if (v4l2_overlay_set_rotation(fd, 0, 0)) {
        LOGE("Failed defaulting rotation\n");
        goto error1;
    }

    if (v4l2_overlay_getId(fd, &pipelineId)) {
        LOGE("Failed: getting overlay Id");
        goto error1;
    }
    if ((pipelineId < 0) || (pipelineId > MAX_NUM_OVERLAYS)) {
        LOGE("Failed: Invalid overlay Id");
        goto error1;
    }

    sprintf(overlayobj->overlaymanagerpath, "/sys/devices/platform/omapdss/overlay%d/manager", pipelineId);
    sprintf(overlayobj->overlayzorderpath, "/sys/devices/platform/omapdss/overlay%d/zorder", pipelineId);
    sprintf(overlayobj->overlayenabled, "/sys/devices/platform/omapdss/overlay%d/enabled", pipelineId);

#ifndef TARGET_OMAP4
    if (v4l2_overlay_set_colorkey(fd, 1, 0)){
        LOGE("Failed enabling color key\n");
        goto error1;
    }
#else
    if (v4l2_overlay_set_global_alpha(fd, 1, 1)){
        LOGE("Failed enabling alpha\n");
        goto error1;
    }


    /* Enable the video zorder and video transparency
    * for the controls to be visible on top of video, give the graphics highest zOrder
    * and set the video source as the tranparency key type
    * set black as the transparency key value
    * and enable the trasparency feature
    **/
    /** in order to findout which manager to set, check for the name
    * and set the properties for that lcd manager
    */
    for (int i = 0; i < MAX_MANAGER_CNT; i++) {
        if (strcmp(managerMetaData[i].managername, "lcd") == 0) {
            LOGD("found LCD manager @ [%d]", i);
            index = i;
            break;
        }
    }

    if (sysfile_write("sys/devices/platform/omapdss/overlay0/zorder", "3",  strlen("0")) < 0) {
        goto error1;
    }

    if (sysfile_write("sys/devices/platform/omapdss/overlay1/zorder", "0",  strlen("0")) < 0) {
        goto error1;
    }

    if (sysfile_write(overlayobj->overlayzorderpath, "1",  strlen("0")) < 0) {
        goto error1;
    }

    if (sysfile_write(managerMetaData[index].managertrans_key_value, "0", strlen("0")) < 0) {
        goto error1;
    }

    if (sysfile_write(managerMetaData[index].managertrans_key_type, "video-source", strlen("video-source")) < 0) {
        goto error1;
    }

    if (sysfile_write(managerMetaData[index].managertrans_key_enabled, "1", strlen("1")) < 0) {
        goto error1;
    }
#endif

    if (v4l2_overlay_req_buf(fd, &num, 0, 0)) {
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

    self->mOmapOverlays[overlayid] = overlayobj;

    LOGD("overlay_createOverlay: OUT");
    return overlayobj;

    LOG_FUNCTION_NAME_EXIT;
error1:
    close(fd);
    self->destroy_shared_overlayobj(overlayobj);
    return NULL;
}

void overlay_control_context_t::overlay_destroyOverlay(struct overlay_control_device_t *dev,
                                   overlay_t* overlay)
{
    LOGE("overlay_destroyOverlay:IN dev (%p) and overlay (%p)", dev, overlay);
    LOG_FUNCTION_NAME_ENTRY;
    if ((dev == NULL) || (overlay == NULL)) {
        LOGE("Null Arguments / Overlay not initd");
        return;
    }

    overlay_control_context_t *self = (overlay_control_context_t *)dev;
    overlay_object *overlayobj = static_cast<overlay_object *>(overlay);

    int rc;
    int fd = overlayobj->getctrl_videofd();
    int index = overlayobj->getIndex();

    overlay_data_context_t::disable_streaming(overlayobj, false);

    //lets reset the manager to the lcd
    if (sysfile_write(overlayobj->overlaymanagerpath, "lcd", sizeof("lcd")) < 0) {
        LOGE("Overlay Manager reset failed, but proceed anyway");
    }
    LOGI("Destroying overlay/fd=%d/obj=%08lx", fd, (unsigned long)overlay);

    if (close(fd)) {
        LOGI( "Error closing overly fd/%d\n", errno);
    }

    self->destroy_shared_overlayobj(overlayobj);

    //NOTE : needs no protection, as the calls are already serialized at Surfaceflinger level
    self->mOmapOverlays[index] = NULL;

    LOGD("overlay_destroyOverlay:OUT");
    LOG_FUNCTION_NAME_EXIT;
}

int overlay_control_context_t::overlay_setPosition(struct overlay_control_device_t *dev,
                               overlay_t* overlay, int x, int y, uint32_t w, uint32_t h)
{
    LOG_FUNCTION_NAME_ENTRY;
    if ((dev == NULL) || (overlay == NULL)) {
        LOGE("Null Arguments / Overlay not initd");
        return -1;
    }

    overlay_object *overlayobj = static_cast<overlay_object *>(overlay);

    overlay_ctrl_t   *stage  = overlayobj->staging();

    int rc = 0;

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
    }

    if (rc == 0) {
        stage->posX = x;
        stage->posY = y;
        stage->posW = w;
        stage->posH = h;
    }

    LOG_FUNCTION_NAME_EXIT;
    return rc;
}

int overlay_control_context_t::overlay_getPosition(struct overlay_control_device_t *dev,
                               overlay_t* overlay, int* x, int* y, uint32_t* w, uint32_t* h)
{
    LOG_FUNCTION_NAME_ENTRY;
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
    LOG_FUNCTION_NAME_ENTRY;
    if (value < 0)
        return -EINVAL;

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
            break;
        case OVERLAY_TRANSFORM_ROT_90:
            stage->rotation = 90;
            break;
        case OVERLAY_TRANSFORM_ROT_180:
            stage->rotation = 180;
            break;
        case OVERLAY_TRANSFORM_ROT_270:
            stage->rotation = 270;
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
        //limit the max value of z-order to hw limit i.e.3
        //make sure that video is behind graphics so it to only 2
        stage->zorder = MIN(value, 2);
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
        break;
    default:
        rc = -1;

    }
    LOG_FUNCTION_NAME_EXIT;
    return rc;
}

int overlay_control_context_t::overlay_stage(struct overlay_control_device_t *dev,
                          overlay_t* overlay) {
    return 0;
}

int overlay_control_context_t::overlay_commit(struct overlay_control_device_t *dev,
                          overlay_t* overlay) {
    LOG_FUNCTION_NAME_ENTRY;
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

    /** NOTE: In order to support HDMI without app explicitly requesting for
    * the screen ID, this is the alternative path
    * check for the overlay manager (here assumption is that: overlay manager
    * is set from command line prior to playing the media clip
    */
    if (sysfile_read(overlayobj->overlaymanagerpath, &overlaymanagername, PATH_MAX) < 0) {
        LOGE("Overlay manager Name Get failed [%d]", __LINE__);
        return -1;
    }
    strtok(overlaymanagername, "\n");

    pthread_mutex_lock(&overlayobj->lock);

    overlayobj->controlReady = 1;

    if (data->posX == stage->posX && data->posY == stage->posY &&
        data->posW == stage->posW && data->posH == stage->posH &&
        data->rotation == stage->rotation && data->alpha == stage->alpha
        && data->panel == stage->panel) {
        LOGI("Nothing to do!\n");
        goto end;
    }

    data->posX       = stage->posX;
    data->posY       = stage->posY;
    data->posW       = stage->posW;
    data->posH       = stage->posH;
    data->rotation   = stage->rotation;
    data->colorkey   = stage->colorkey;
    data->alpha      = stage->alpha;


    // Adjust the coordinate system to match the V4L change
    switch ( data->rotation ) {
    case 90:
        finalWindow.posX = data->posY;
        finalWindow.posY = data->posX;
        finalWindow.posW = data->posH;
        finalWindow.posH = data->posW;
        break;
    case 180:
        finalWindow.posX = ((overlayobj->dispW - data->posX) - data->posW);
        finalWindow.posY = ((overlayobj->dispH - data->posY) - data->posH);
        finalWindow.posW = data->posW;
        finalWindow.posH = data->posH;
        break;
    case 270:
        finalWindow.posX = data->posY;
        finalWindow.posY = data->posX;
        finalWindow.posW = data->posH;
        finalWindow.posH = data->posW;
        break;
    default: // 0
        finalWindow.posX = data->posX;
        finalWindow.posY = data->posY;
        finalWindow.posW = data->posW;
        finalWindow.posH = data->posH;
        break;
    }

    strmatch = strcmp(overlaymanagername, "tv");
    if (!strmatch) {
        stage->panel = OVERLAY_ON_TV;
    }

    //Calculate window size. As of now this is applicable only for non-LCD panels
    calculateWindow(overlayobj, &finalWindow);

    LOGI("Position/X%d/Y%d/W%d/H%d\n", data->posX, data->posY, data->posW, data->posH );
    LOGI("Adjusted Position/X%d/Y%d/W%d/H%d\n", finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH);
    LOGI("Rotation/%d\n", data->rotation );
    LOGI("alpha/%d\n", data->alpha );
    LOGI("zorder/%d\n", data->zorder );
    LOGI("data->panel/0x%x/stage->panel/0x%x\n", data->panel, stage->panel );    

    if ((ret = v4l2_overlay_get_crop(fd, &eCropData.cropX, &eCropData.cropY, &eCropData.cropW, &eCropData.cropH))) {
        LOGE("commit:Get crop value Failed!/%d\n", ret);
        goto end;
    }

    if ((ret = overlay_data_context_t::disable_streaming_locked(overlayobj, false))) {
        LOGE("Stream Off Failed!/%d\n", ret);
        goto end;
    }

    if ((ret = v4l2_overlay_set_rotation(fd, data->rotation, 0))) {
        LOGE("Set Rotation Failed!/%d\n", ret);
        goto end;
    }

    if ((ret = v4l2_overlay_set_crop(fd,
                    eCropData.cropX,
                    eCropData.cropY,
                    eCropData.cropW,
                    eCropData.cropH))) {
        LOGE("Set Cropping Failed!/%d\n",ret);
        goto end;
    }

    if ((ret = v4l2_overlay_set_position(fd, finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH))) {
        LOGE("Set Position Failed!/%d\n", ret);
        goto end;
    }

    //unlock the mutex here as the subsequent operations are file operations
    //otherwise it may hang
    pthread_mutex_unlock(&overlayobj->lock);

#ifndef TARGET_OMAP4
    if ((ret = v4l2_overlay_set_colorkey(fd, 1, 0x00))) {
        LOGE("Failed enabling color key\n");
        goto end;
    }
#else
    if ((ret = v4l2_overlay_set_global_alpha(fd, 1, 255))) {
        LOGE("Failed enabling alpha\n");
        goto end;
    }
    if (data->zorder != stage->zorder) {
        data->zorder = stage->zorder;
        //Set up the z-order for the overlay:
        //TBD:Surface flinger or the driver has to re-work the zorder of all the
        //other active overlays for a given manager to service the current request.
        char z_order[16];
        sprintf(z_order, "%d", data->zorder);
        if (sysfile_write(overlayobj->overlayzorderpath, &z_order,  strlen("0")) < 0) {
            LOGE("zorder setting failed");
            return -1;
        }
    }
#endif

    if (data->panel != stage->panel) {
        data->panel = stage->panel;
        calculateDisplayMetaData(overlayobj);
        LOGD("Panel path [%s]", screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displayenabled);
        LOGD("Manager display [%s]", managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managerdisplay);
        LOGD("manager path [%s]", overlayobj->overlaymanagerpath);
        LOGD("manager name [%s]", managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managername);

        if (overlayobj->mDisplayMetaData.mTobeDisabledPanelIndex != -1) {
            char disablepanelpath[PATH_MAX];
            sprintf(disablepanelpath, "/sys/devices/platform/omapdss/display%d/enabled", \
                overlayobj->mDisplayMetaData.mTobeDisabledPanelIndex);
            if (sysfile_write(disablepanelpath, "0", sizeof("0")) < 0) {
                LOGE("panel disable failed");
                return -1;
            }
        }
        if (sysfile_write(overlayobj->overlayenabled, "0", sizeof("0")) < 0) {
            LOGE("Failed: overlay Disable");
            return -1;
        }

        /* Set the manager to the overlay */
        if (sysfile_write(overlayobj->overlaymanagerpath, \
            &managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managername, PATH_MAX) < 0) {
            LOGE("Unable to set the overlay->manager");
            return -1;
        }

#ifdef TARGET_OMAP4

        /** set the manager display to the panel*/
        if (sysfile_write(managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managerdisplay, \
            screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displayname, PATH_MAX) < 0) {
            LOGE("Unable to set the manager->display");
            return -1;
        }
#else
        char displaytimings[PATH_MAX];
        sprintf(displaytimings, "/sys/devices/platform/omapdss/display%d/timings", overlayobj->mDisplayMetaData.mPanelIndex);
        if (!strcmp(managerMetaData[overlayobj->mDisplayMetaData.mManagerIndex].managername, "tv")) {
            if (sysfile_write(displaytimings, "ntsc", sizeof("ntsc")) < 0) {
                LOGE("Setting display timings failed");
                return -1;
            }
        }

#endif
        //enable the requested panel here
        if (sysfile_write(screenMetaData[overlayobj->mDisplayMetaData.mPanelIndex].displayenabled, "1", sizeof("1")) < 0) {
            LOGE("Panel enable failed");
            return -1;
        }

        // Disable streaming to ensure that stream_on is called again which indirectly sets overlayenabled to 1
        if ((ret = overlay_data_context_t::disable_streaming(overlayobj, false))) {
            LOGE("Stream Off Failed!/%d\n", ret);
            goto end;
        }
    }
    LOG_FUNCTION_NAME_EXIT;
    return 0;

end:
    pthread_mutex_unlock(&overlayobj->lock);
    LOG_FUNCTION_NAME_EXIT;
    return ret;

}

int overlay_control_context_t::overlay_control_close(struct hw_device_t *dev)
{
    LOG_FUNCTION_NAME_ENTRY;
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
    LOG_FUNCTION_NAME_ENTRY;
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
    LOG_FUNCTION_NAME_EXIT;
    LOGD("Initialize ret = %d", rc);
    return ( rc );
}

int overlay_data_context_t::overlay_resizeInput(struct overlay_data_device_t *dev, uint32_t w, uint32_t h)
{
    LOGD("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
    LOG_FUNCTION_NAME_ENTRY;
    if (dev == NULL) {
        LOGE("Null Arguments ");
        return -1;
    }

    int ret = 0;
    int rc;
    uint32_t numb = NUM_OVERLAY_BUFFERS_REQUESTED;
    overlay_data_t eCropData;
    int degree = 0;

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

    if ((ctx->omap_overlay->w == (unsigned int)w) && (ctx->omap_overlay->h == (unsigned int)h) && (ctx->omap_overlay->attributes_changed == 0)){
        LOGE("Same as current width and height. Attributes did not change either. So do nothing.");
        //Lets reset the statemachine and disable the stream
        ctx->omap_overlay->dataReady = 0;
        if ((rc = ctx->disable_streaming_locked(ctx->omap_overlay))) {
            return -1;
        }
        return 0;
    }

    pthread_mutex_lock(&ctx->omap_overlay->lock);

    if ((rc = ctx->disable_streaming_locked(ctx->omap_overlay))) {
        goto end;
    }

    if ((ret = v4l2_overlay_get_crop(fd, &eCropData.cropX, &eCropData.cropY, &eCropData.cropW, &eCropData.cropH))) {
        LOGE("resizeip: Get crop value Failed!/%d\n", rc);
        goto end;
    }

    if ((ret = v4l2_overlay_get_position(fd, &_x,  &_y, &_w, &_h))) {
        LOGD(" Could not set the position when creating overlay \n");
        goto end;
    }

    if ((ret = v4l2_overlay_get_rotation(fd, &degree, NULL))) {
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
    if ((ret = v4l2_overlay_set_rotation(fd, degree, 0))) {
        LOGE("Failed rotation\n");
        goto end;
    }

     if ((ret = v4l2_overlay_set_crop(fd, eCropData.cropX, eCropData.cropY, eCropData.cropW, eCropData.cropH))) {
        LOGE("Failed crop window\n");
        goto end;
    }
#ifndef TARGET_OMAP4
    if ((ret = v4l2_overlay_set_colorkey(fd,1, 0x00))) {
        LOGE("Failed enabling color key\n");
        goto end;
    }
#endif

    if ((ret = v4l2_overlay_set_position(fd, _x,  _y, _w, _h))) {
        LOGD(" Could not set the position when creating overlay \n");
        goto end;
    }

    if ((ret = v4l2_overlay_req_buf(fd, (uint32_t *)(&ctx->omap_overlay->num_buffers), ctx->omap_overlay->cacheable_buffers, ctx->omap_overlay->maintain_coherency))) {
        LOGE("Error creating buffers");
        goto end;
    }

    for (int i = 0; i < ctx->omap_overlay->num_buffers; i++) {
        v4l2_overlay_map_buf(fd, i, &ctx->omap_overlay->buffers[i], &ctx->omap_overlay->buffers_len[i]);
    }

    ctx->omap_overlay->mappedbufcount = ctx->omap_overlay->num_buffers;
    ctx->omap_overlay->dataReady = 0;

    /* The control pameters just got set */
    ctx->omap_overlay->controlReady = 1;
    ctx->omap_overlay->attributes_changed = 0; // Reset it

    LOG_FUNCTION_NAME_EXIT;
end:

    pthread_mutex_unlock(&ctx->omap_overlay->lock);
    return ret;
}

int overlay_data_context_t::overlay_data_setParameter(struct overlay_data_device_t *dev,
                                     int param, int value)
{
    LOG_FUNCTION_NAME_ENTRY;
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

    if ( ctx->omap_overlay->dataReady ) {
        LOGI("Too late. Cant set it now!\n");
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
    }

    LOG_FUNCTION_NAME_EXIT;
    return ( ret );
}


int overlay_data_context_t::overlay_setCrop(struct overlay_data_device_t *dev, uint32_t x,
                           uint32_t y, uint32_t w, uint32_t h) {
    //LOG_FUNCTION_NAME_ENTRY;
    //This print is commented as part of on-the-fly crop support
    if (dev == NULL) {
        LOGE("Null Arguments[%d]", __LINE__);
        return -1;
    }

    if (((int)x < 0)||((int)y < 0)||((int)w <= 0)||((int)h <= 0)||(w > MAX_OVERLAY_WIDTH_VAL)||(h > MAX_OVERLAY_HEIGHT_VAL)) {
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
    if (ctx->omap_overlay->mData.cropX == x && ctx->omap_overlay->mData.cropY == y && ctx->omap_overlay->mData.cropW == w
        && ctx->omap_overlay->mData.cropH == h) {
        //LOGI("Nothing to do!\n");
        return 0;
    }
    int fd = ctx->omap_overlay->getdata_videofd();

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

    LOGE("Crop Win/X%d/Y%d/W%d/H%d\n", x, y, w, h );

    rc = v4l2_overlay_set_crop(fd, x, y, w, h);
    if (rc) {
        LOGE("Set Crop Window Failed!/%d\n", rc);
    }

    overlay_control_context_t::calculateWindow(ctx->omap_overlay, &finalWindow);
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

        LOGE("Crop Win/X%d/Y%d/W%d/H%d\n", x, y, w, h );
        rc = v4l2_overlay_set_crop(fd, x, y, w, h);
        if (rc) {
            LOGE("Set Crop Window Failed!/%d\n", rc);
            goto end;
        }
        overlay_control_context_t::calculateWindow(ctx->omap_overlay, &finalWindow);
        if ((rc = v4l2_overlay_set_position(fd, finalWindow.posX, finalWindow.posY, finalWindow.posW, finalWindow.posH))) {
            LOGD(" Could not set the position when setting the crop \n");
            goto end;
        }
    }
    else {
        /** setCrop is called only with the crop co-ordinates change and
        * is allowed on-the-fly. Hence no need of disabling the stream and
        * the final window remains the same
        */
        ctx->omap_overlay->mData.cropX = x;
        ctx->omap_overlay->mData.cropY = y;
        ctx->omap_overlay->mData.cropW = w;
        ctx->omap_overlay->mData.cropH = h;

        LOGE("Crop Win/X%d/Y%d/W%d/H%d\n", x, y, w, h );

        rc = v4l2_overlay_set_crop(fd, x, y, w, h);
        if (rc) {
            LOGE("Set Crop Window Failed!/%d\n", rc);
        }
    }
#endif

    LOG_FUNCTION_NAME_EXIT;
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

    int rc;
    int i = -1;

    pthread_mutex_lock(&ctx->omap_overlay->lock);
    if (ctx->omap_overlay->streamEn == 0) {
        LOGE("Cannot dequeue when streaming is disabled. ctx->omap_overlay->qd_buf_count = %d", ctx->omap_overlay->qd_buf_count);
        rc = -2; // This error code is specifically checked for in CameraHal. So, do not change it to something generic.
    }

   else if ( ctx->omap_overlay->qd_buf_count <= 1 ) { //dss2 require at least 2 buf queue to perform a dequeue to avoid hang
        printf("Not enough buffers to dequeue");
        rc = -EPERM;
    }

    else if ( (rc = v4l2_overlay_dq_buf(fd, &i )) != 0 ) {
        LOGE("Failed to DQ/%d\n", rc);
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
   int rc = v4l2_overlay_q_buf(fd, (int)buffer);
   if (rc < 0) {
       LOGD("queueBuffer failed. rc = %d", rc);
       rc = -EPERM;
       goto EXIT;
   }

    if (ctx->omap_overlay->qd_buf_count < ctx->omap_overlay->num_buffers && rc == 0) {
        ctx->omap_overlay->qd_buf_count ++;
    }

    // Catch the case where the data side had no need to set the crop window
    LOGV("qd_buf_count = %x", ctx->omap_overlay->qd_buf_count);
    if ( ctx->omap_overlay->qd_buf_count >= ctx->omap_overlay->optimalQBufCnt && (ctx->omap_overlay->streamEn == 0)) {
        /*DSS2: 2 buffers need to be queue before enable streaming*/
        ctx->omap_overlay->dataReady = 1;
        ctx->enable_streaming_locked(ctx->omap_overlay);
    }
    rc = ctx->omap_overlay->qd_buf_count;

EXIT:
    pthread_mutex_unlock(&ctx->omap_overlay->lock);
    return ( rc );
}

void* overlay_data_context_t::overlay_getBufferAddress(struct overlay_data_device_t *dev,
                               overlay_buffer_t buffer)
{
    LOG_FUNCTION_NAME_ENTRY;
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
        LOGE("Buffer/%d/addr=%08lx/len=%d \n", (int)buffer, (unsigned long)ctx->omap_overlay->mapping_data->ptr,
             ctx->omap_overlay->buffers_len[(int)buffer]);
    }

    return (void *)ctx->omap_overlay->mapping_data;
}

int overlay_data_context_t::overlay_getBufferCount(struct overlay_data_device_t *dev)
{
    LOG_FUNCTION_NAME_ENTRY;
    if (dev == NULL) {
        LOGE("Null Arguments ");
        return -1;
    }

    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;

    return (ctx->omap_overlay->num_buffers);
}

int overlay_data_context_t::overlay_data_close(struct hw_device_t *dev) {

    LOG_FUNCTION_NAME_ENTRY;
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
        dev->destroyOverlay = overlay_control_context_t::overlay_destroyOverlay;
        dev->setPosition = overlay_control_context_t::overlay_setPosition;
        dev->getPosition = overlay_control_context_t::overlay_getPosition;
        dev->setParameter = overlay_control_context_t::overlay_setParameter;
        dev->stage = overlay_control_context_t::overlay_stage;
        dev->commit = overlay_control_context_t::overlay_commit;

        *device = &dev->common;
        for (int i = 0; i < MAX_NUM_OVERLAYS; i++)
        {
            dev->mOmapOverlays[i] = NULL;
        }
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

        *device = &dev->common;
         status = 0;
    }else {
        /** Error: no memory available */
        status = - ENOMEM;
    }
  }
    return status;
}


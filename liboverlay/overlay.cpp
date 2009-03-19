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

#define LOG_TAG "TIOverlay"

#include <hardware/hardware.h>
#include <hardware/overlay.h>

extern "C" {
#include "v4l2_utils.h"
}

#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

/*****************************************************************************/

#define LOG_FUNCTION_NAME    LOGD(" %s ###### Calling %s() ######",  __FILE__, __FUNCTION__);

pthread_mutex_t global_mutex; 

struct overlay_control_context_t {
    struct overlay_control_device_t device;
    /* our private state goes below here */
    struct overlay_t* overlay_video1;
    struct overlay_t* overlay_video2;
};

struct overlay_data_context_t {
    struct overlay_data_device_t device;
    /* our private state goes below here */
    int ctl_fd;
    int width;
    int height;
    int num_buffers;
    size_t *buffers_len;
    void **buffers;
    bool stream_on;
};

static int overlay_device_open(const struct hw_module_t* module,
                               const char* name, struct hw_device_t** device);

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
    }
};

/*****************************************************************************/

/*
 * This is the overlay_t object, it is returned to the user and represents
 * an overlay. here we use a subclass, where we can store our own state. 
 * This handles will be passed across processes and possibly given to other
 * HAL modules (for instance video decode modules).
 */
struct handle_t : public native_handle {
    /* add the data fields we need here, for instance: */
    int ctl_fd;
    int width;
    int height;
    int num_buffers;
};

static int handle_fd(const overlay_handle_t overlay) {
    return static_cast<const struct handle_t *>(overlay)->ctl_fd;
}

static int handle_num_buffers(const overlay_handle_t overlay) {
    return static_cast<const struct handle_t *>(overlay)->num_buffers;
}

static int handle_width(const overlay_handle_t overlay) {
    return static_cast<const struct handle_t *>(overlay)->width;
}

static int handle_height(const overlay_handle_t overlay) {
    return static_cast<const struct handle_t *>(overlay)->height;
}

class overlay_object : public overlay_t {

    handle_t mHandle;
    int rotation;
	
    static overlay_handle_t getHandleRef(struct overlay_t* overlay) {
        /* returns a reference to the handle, caller doesn't take ownership */
        return &(static_cast<overlay_object *>(overlay)->mHandle);
    }

public:
    overlay_object(int ctl_fd, int w, int h, int format, int num_buffers) {
        //LOG_FUNCTION_NAME        
        this->overlay_t::getHandleRef = getHandleRef;
        mHandle.version = sizeof(native_handle);
        mHandle.numFds = 1;
        mHandle.numInts = 3; // extra ints we have in our handle
        mHandle.ctl_fd = ctl_fd;
        mHandle.width = w;
        mHandle.height = h;
        mHandle.num_buffers = num_buffers;
        this->w = w;
        this->h = h;
        this->format = format;
        rotation = 0;
    }

    int ctl_fd() { return mHandle.ctl_fd; }
    int getRotation() { return rotation; }	
    void setRotation(int rot) { rotation = rot; }		
    int num_buffers() { return mHandle.num_buffers; }
};

// ****************************************************************************
// Control module
// ****************************************************************************

static int overlay_get(struct overlay_control_device_t *dev, int name) {
    //LOG_FUNCTION_NAME
    int result = -1;
    switch (name) {
        case OVERLAY_MINIFICATION_LIMIT:
            result = 0; // 0 = no limit
            break;
        case OVERLAY_MAGNIFICATION_LIMIT:
            result = 0; // 0 = no limit
            break;
        case OVERLAY_SCALING_FRAC_BITS:
            result = 0; // 0 = infinite
            break;
        case OVERLAY_ROTATION_STEP_DEG:
            result = 90; // 90 rotation steps (for instance)
            break;
        case OVERLAY_HORIZONTAL_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
        case OVERLAY_VERTICAL_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
        case OVERLAY_WIDTH_ALIGNMENT:
            result = 1; // 1-pixel alignment
            break;
        case OVERLAY_HEIGHT_ALIGNMENT:
            break;
    }
    return result;
}

static overlay_t* overlay_createOverlay(struct overlay_control_device_t *dev,
         uint32_t w, uint32_t h, int32_t format)
{
    //LOG_FUNCTION_NAME   
    uint32_t num_buffers = 4;
    /* open the device node */
    int fd, ret;
    overlay_object *overlay;
    overlay_control_context_t *ctx = (overlay_control_context_t *)dev;

    LOGI("Create overlay, w=%d h=%d format=%d", w, h, format);

    if (ctx->overlay_video1) {
        LOGE("Overlays already in use");
        return NULL;
    }

    pthread_mutex_lock(&global_mutex);
    fd = v4l2_overlay_open(V4L2_OVERLAY_PLANE_VIDEO1);
    pthread_mutex_unlock(&global_mutex); 
    
    LOGI("Opened video1 fd=%d", fd);

    if (fd < 0) {
        LOGE("Can't open overlay device");
        return NULL;
    }
    /* check the input params, reject if not supported or invalid, initialize
     * the hw */
    ret = v4l2_overlay_init(fd, w, h, format);
    if (ret) {
        LOGE("Error initializing overlays");
        return NULL;
    }

    ret = v4l2_overlay_set_crop(fd, 0, 0, w, h);

    ret = v4l2_overlay_set_rotation(fd, 0, 0);

    ret = v4l2_overlay_req_buf(fd, &num_buffers);

    ret = v4l2_overlay_set_colorkey(fd, 1 /*enable*/, 0);

    /* Create overlay object. Talk to the h/w here and adjust to what it can
     * do. the overlay_t returned can  be a C++ object, subclassing overlay_t
     * if needed.
     *
     * we probably want to keep a list of the overlay_t created so they can
     * all be cleaned up in overlay_close().
     */
     overlay = new overlay_object(fd, w, h, format, (int)num_buffers);
     ctx->overlay_video1 = overlay;
     if (v4l2_overlay_stream_on(fd))
         return NULL;
     return overlay;
}

static void overlay_destroyOverlay(struct overlay_control_device_t *dev,
                                   overlay_t* overlay)
{
    //LOG_FUNCTION_NAME   
    /* free resources associated with this overlay_t */
    overlay_control_context_t *ctx = (overlay_control_context_t *)dev;
    int fd = static_cast<overlay_object *>(overlay)->ctl_fd();


    pthread_mutex_lock(&global_mutex); 
   
    v4l2_overlay_stream_off(fd);

    if (v4l2_overlay_set_colorkey(fd, 0 /*disable*/, 0) < 0)
    {
        LOGE("Error disabling color key");
    }

    LOGI("@@@@@@@@@@@@@@@@:Closed video1 fd =%d", fd);
    close(fd);
 

    LOGI("Destroy overlay");
    if (!overlay){
        pthread_mutex_unlock(&global_mutex); 
        return;
    }

    /* XXX: figure out how to free buffers !!! */

    /* XXX: is this right? */
    if (ctx->overlay_video1 == overlay)
        ctx->overlay_video1 = NULL;
    if (ctx->overlay_video2 == overlay)
        ctx->overlay_video2 = NULL;
    /* should I also free the overlay_t? */

    delete overlay;
    pthread_mutex_unlock(&global_mutex);
    
}

static int overlay_setPosition(struct overlay_control_device_t *dev,
                               overlay_t* overlay,
                               int x, int y, uint32_t w, uint32_t h) {

    //LOG_FUNCTION_NAME   
    int ret = 0;
    int fd = static_cast<overlay_object *>(overlay)->ctl_fd();
    int temp_x;
    uint32_t temp_w;
    int32_t gx, gy, gw, gh;

    v4l2_overlay_get_position(fd, &gx, &gy, &gw, &gh);

    // if same as before, then return
    if (((static_cast<overlay_object *>(overlay)->getRotation())%180) == 0)    
    {
        if ((x == gx) && (y == gy) && (w == gw) && (h == gh)) return ret;
    }
    else
    {
        if ((x == gy) && (y == gx) && (w == gh) && (h == gw)) 	return ret;
    }
		
    if (((static_cast<overlay_object *>(overlay)->getRotation())%180) != 0)    
    {
        temp_x = x; x = y; y = temp_x;
        temp_w = w; w = h; h = temp_w;
        LOGI("Swapped (x and y) and (w and h) because of rotation");
    }

    pthread_mutex_lock(&global_mutex); 
    if (v4l2_overlay_stream_off(fd))
    {
        ret = -EINVAL;
        goto EXIT;
    }
    if (v4l2_overlay_set_position(fd, x, y, w, h))
    {
        ret = -EINVAL;
        goto EXIT;
    }
    if (v4l2_overlay_stream_on(fd))
    {
        ret = -EINVAL;
        goto EXIT;
    }

    LOGI("position set by sf to x=%d y=%d w=%u h=%u", x, y, w, h);

EXIT:
    pthread_mutex_unlock(&global_mutex);     
    return ret;
}

static int overlay_getPosition(struct overlay_control_device_t *dev,
                               overlay_t* overlay,
                               int* x, int* y, uint32_t* w, uint32_t* h) {
    //LOG_FUNCTION_NAME   
    int fd = static_cast<overlay_object *>(overlay)->ctl_fd();
    int ret;
    int32_t win_w, win_h;

    ret = v4l2_overlay_get_position(fd, x, y, (int32_t *)w, (int32_t *)h);
    if (ret)
        return -EINVAL;
    return 0;
}

static int overlay_setParameter(struct overlay_control_device_t *dev,
         overlay_t* overlay, int param, int value) {

    LOG_FUNCTION_NAME
    int fd = static_cast<overlay_object *>(overlay)->ctl_fd();        
    int result = 0;
    int rotation = 0;
    int32_t x, y, w, h;
    uint32_t pix_w, pix_h;	
	
    switch (param) {

        case OVERLAY_ROTATION_DEG:

            LOGI("Rotate by %d degrees", value);
            value = value % 360;
            if ((value%90) != 0)
            {
                result = -EINVAL;
                break;
            }
            result = v4l2_overlay_set_rotation(fd, value, 0);
            break;
            
        case OVERLAY_DITHER:
            LOGI("Set Dither");
            break;
            
        case OVERLAY_TRANSFORM:
            switch(value){
                //case OVERLAY_TRANSFORM_ROT_0:
                case 0:
			rotation = 0;
			break;
                case OVERLAY_TRANSFORM_ROT_90:
			rotation = 90;
			break;
                case OVERLAY_TRANSFORM_ROT_180:
			rotation = 180;
			break;
                case OVERLAY_TRANSFORM_ROT_270:
			rotation = 270;
			break;
                case OVERLAY_TRANSFORM_FLIP_H:
                case OVERLAY_TRANSFORM_FLIP_V:					
			return result;
                default:
                    result = -EINVAL;
                    break;
            }
			
            if ((static_cast<overlay_object *>(overlay)->getRotation()) != rotation)
            {
                v4l2_overlay_stream_off(fd);
                result = v4l2_overlay_set_rotation(fd, rotation, 0);
                if (result) break;
                LOGI("Rotated by %d degrees", rotation);
                static_cast<overlay_object *>(overlay)->setRotation(rotation);				
                if ((rotation % 180) != 0){
                    v4l2_overlay_get_position(fd, &x, &y, &w, &h);
                    v4l2_overlay_set_position(fd, y, x, h, w);
                }			
                v4l2_overlay_stream_on(fd);				
            }
            break;
            
        default:
            result = -EINVAL;
            break;
    }
    return result;
}

static int overlay_control_close(struct hw_device_t *dev)
{
    //LOG_FUNCTION_NAME   
    struct overlay_control_context_t* ctx =
            (struct overlay_control_context_t*)dev;
    overlay_object *overlay_v1;
    //overlay_object *overlay_v2;

    if (ctx) {
        overlay_v1 = static_cast<overlay_object *>(ctx->overlay_video1);
        //overlay_v2 = static_cast<overlay_object *>(ctx->overlay_video2);
        /* free all resources associated with this device here
         * in particular the overlay_handle_t, outstanding overlay_t, etc...
         */
        int fd;
        //v4l2_overlay_stream_off(overlay_v2->ctl_fd());
        overlay_destroyOverlay((struct overlay_control_device_t *)ctx,
                               overlay_v1);
        //overlay_destroyOverlay((struct overlay_control_device_t *)ctx, overlay_v2);
        /* should i free the overlay_t's here? */

        free(ctx);
        pthread_mutex_destroy(&global_mutex);
    }
    return 0;
}

// ****************************************************************************
// Data module
// ****************************************************************************

int overlay_initialize(struct overlay_data_device_t *dev,
                       overlay_handle_t handle)
{
    //LOG_FUNCTION_NAME   
    /*
     * overlay_handle_t should contain all the information to "inflate" this
     * overlay. Typically it'll have a file descriptor, information about
     * how many buffers are there, etc...
     * It is also the place to mmap all buffers associated with this overlay
     * (see getBufferAddress).
     *
     * NOTE: this function doesn't take ownership of overlay_handle_t
     *
     */
    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    int i, ret;
    struct stat stat;

    ctx->num_buffers = handle_num_buffers(handle);
    ctx->ctl_fd = handle_fd(handle);
    if (fstat(ctx->ctl_fd, &stat))
        LOGE("Error = %s from %s", strerror(errno), "overlay initialize");
    ctx->width = handle_width(handle);
    ctx->height = handle_height(handle);
    ctx->buffers = new void *[ctx->num_buffers];
    ctx->buffers_len = new size_t[ctx->num_buffers];
    for (i = 0; i < ctx->num_buffers; i++)
        v4l2_overlay_map_buf(ctx->ctl_fd, i, &ctx->buffers[i],
                             &ctx->buffers_len[i]);
    return 0;
}


static int overlay_setCrop(struct overlay_data_device_t *dev,
                                    uint32_t x, uint32_t y, uint32_t w, uint32_t h) 
{

    //LOG_FUNCTION_NAME   
    int ret = 0;
    struct overlay_data_context_t* ctx =
            (struct overlay_data_context_t*)dev;

    pthread_mutex_lock(&global_mutex); 
    if (v4l2_overlay_stream_off(ctx->ctl_fd))
    {
        ret = -EINVAL;
        goto EXIT;
    }
    if (v4l2_overlay_set_crop(ctx->ctl_fd, x, y, w, h))
    {
        ret = -EINVAL;
        goto EXIT;
    }		
    if (v4l2_overlay_stream_on(ctx->ctl_fd))
    {
        ret = -EINVAL;
        goto EXIT;
    }

    LOGI("Set Crop to x=%d y=%d w=%u h=%u", x, y, w, h);

EXIT:
    pthread_mutex_unlock(&global_mutex);     
    return ret;
}


static int overlay_getCrop(struct overlay_data_device_t *dev,
                           uint32_t* x, uint32_t* y, uint32_t* w, uint32_t* h) 
{
    //LOG_FUNCTION_NAME
    struct overlay_data_context_t* ctx =
            (struct overlay_data_context_t*)dev;
    
    return v4l2_overlay_get_crop(ctx->ctl_fd, x, y, w, h);
}


int overlay_dequeueBuffer(struct overlay_data_device_t *dev,
                          overlay_buffer_t *buffer)
{
    ////LOG_FUNCTION_NAME   
    /* blocks until a buffer is available and return an opaque structure
     * representing this buffer.
     */
    int ret, i = -1;
    struct overlay_data_context_t* ctx =
            (struct overlay_data_context_t*)dev;

    ret = v4l2_overlay_dq_buf(ctx->ctl_fd, &i);
    if (i < 0 || i > ctx->num_buffers)
        return -EINVAL;

    *((int *)buffer) = i;
    return ret;
}

int overlay_queueBuffer(struct overlay_data_device_t *dev,
        overlay_buffer_t buffer)
{
    ////LOG_FUNCTION_NAME   
    int ret;
    struct overlay_data_context_t* ctx =
            (struct overlay_data_context_t*)dev;

    ret = v4l2_overlay_q_buf(ctx->ctl_fd, (int)buffer);
    return ret;
}

void *overlay_getBufferAddress(struct overlay_data_device_t *dev,
        overlay_buffer_t buffer)
{
    //LOG_FUNCTION_NAME   
    /* this may fail (NULL) if this feature is not supported. In that case,
     * presumably, there is some other HAL module that can fill the buffer,
     * using a DSP for instance */
    struct overlay_data_context_t* ctx =
            (struct overlay_data_context_t*)dev;

    LOGI("index is %d", buffer);
    if ((int)buffer >= ctx->num_buffers || (int)buffer < 0)
        return NULL;
    LOGI("buffer len is %d", ctx->buffers_len[(int)buffer]);
    return ctx->buffers[(int)buffer];
}

int overlay_getBufferCount(struct overlay_data_device_t *dev)
{
    //LOG_FUNCTION_NAME   
    struct overlay_data_context_t* ctx =
            (struct overlay_data_context_t*)dev;
    return ctx->num_buffers;
}

static int overlay_data_close(struct hw_device_t *dev)
{
    //LOG_FUNCTION_NAME   
    struct overlay_data_context_t* ctx = (struct overlay_data_context_t*)dev;
    if (ctx) {
        overlay_data_device_t *overlay_dev = &ctx->device;
        int buf;
        /* free all resources associated with this device here
         * in particular all pending overlay_buffer_t if needed.
         * 
         * NOTE: overlay_handle_t passed in initialize() is NOT freed and
         * its file descriptors are not closed (this is the responsibility
         * of the caller).
         */
        for (int i = 0; i < ctx->num_buffers; i++) {
            /* in case they are queued, dq them */
            //v4l2_overlay_dq_buf(ctx->ctl_fd, &buf);
            /* unmap them */
            v4l2_overlay_unmap_buf(ctx->buffers[buf],
                                   ctx->buffers_len[buf]);
        }
        delete(ctx->buffers);
        delete(ctx->buffers_len);
        free(ctx);
    }
    return 0;
}

/*****************************************************************************/

static int overlay_device_open(const struct hw_module_t* module,
                               const char* name, struct hw_device_t** device)
{
    //LOG_FUNCTION_NAME   
    int status = -EINVAL;
    if (!strcmp(name, OVERLAY_HARDWARE_CONTROL)) {

        if (pthread_mutex_init(&global_mutex, NULL))
            LOGD("pthread_mutex_init failed");    

        struct overlay_control_context_t *dev;
        dev = (overlay_control_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = overlay_control_close;
        
        dev->device.get = overlay_get;
        dev->device.createOverlay = overlay_createOverlay;
        dev->device.destroyOverlay = overlay_destroyOverlay;
        dev->device.setPosition = overlay_setPosition;
        dev->device.getPosition = overlay_getPosition;
        dev->device.setParameter = overlay_setParameter;

        *device = &dev->device.common;
        status = 0;
    } else if (!strcmp(name, OVERLAY_HARDWARE_DATA)) {
        struct overlay_data_context_t *dev;
        dev = (overlay_data_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = overlay_data_close;
        
        dev->device.initialize = overlay_initialize;
        dev->device.dequeueBuffer = overlay_dequeueBuffer;
        dev->device.queueBuffer = overlay_queueBuffer;
        dev->device.getBufferAddress = overlay_getBufferAddress;
        dev->device.getBufferCount = overlay_getBufferCount;
        
        *device = &dev->device.common;
        status = 0;
    }
    return status;
}

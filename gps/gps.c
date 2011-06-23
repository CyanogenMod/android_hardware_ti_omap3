/****************************************************************************
**+-----------------------------------------------------------------------+**
**|                                                                       |**
**| Copyright(c) 1998 - 2011 Texas Instruments. All rights reserved.      |**
**| All rights reserved.                                                  |**
**|                                                                       |**
**| Redistribution and use in source and binary forms, with or without    |**
**| modification, are permitted provided that the following conditions    |**
**| are met:                                                              |**
**|                                                                       |**
**|  * Redistributions of source code must retain the above copyright     |**
**|    notice, this list of conditions and the following disclaimer.      |**
**|  * Redistributions in binary form must reproduce the above copyright  |**
**|    notice, this list of conditions and the following disclaimer in    |**
**|    the documentation and/or other materials provided with the         |**
**|    distribution.                                                      |**
**|  * Neither the name Texas Instruments nor the names of its            |**
**|    contributors may be used to endorse or promote products derived    |**
**|    from this software without specific prior written permission.      |**
**|                                                                       |**
**| THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS   |**
**| "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT     |**
**| LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR |**
**| A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT  |**
**| OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, |**
**| SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT      |**
**| LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, |**
**| DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY |**
**| THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT   |**
**| (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE |**
**| OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  |**
**|                                                                       |**
**+-----------------------------------------------------------------------+**
****************************************************************************/

/*
 * file gps.c
 */

#include <hardware/gps.h>
#include <cutils/properties.h>
#include <utils/Log.h>

#include "ti_gps.h"

#define LOG_TAG "ti_gps_hal"

/* Interfaces exposed by framework for supporting location based services. */
GpsInterface Interface = {
    sizeof(GpsInterface),	/* Size of the GPS interface */
    hgps_init,                  /* init */
    NULL,                 /* start loc fix */
    NULL,                  /* stop location fix */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

const GpsInterface* ti_get_gps_interface(struct gps_device_t* dev)
{
    LOGD("ti_get_gps_interface: Returning Interface\n");
    return &Interface;
}

static int open_gps(const struct hw_module_t* module, char const* name,
        struct hw_device_t** device)
{
    struct gps_device_t *dev = (struct gps_device_t *) malloc(sizeof(struct gps_device_t));
    memset(dev, 0, sizeof(*dev));

    LOGD("In open_gps\n");
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t*)module;
    dev->get_gps_interface = ti_get_gps_interface;

    *device = (struct hw_device_t*)dev;
    LOGD("Exiting open_gps\n");
    return 0;
}

int hgps_init(GpsCallbacks* callbacks)
{
    LOGD(" hgps_init: Entering \n");
    return -1;
}

void set_privacy_lock (int enable_lock )
{

}

static struct hw_module_methods_t ti_gps_module_methods = {
    .open = open_gps
};

const struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = GPS_HARDWARE_MODULE_ID,
    .name = "TI Dummy GPS Module",
    .author = "TI",
    .methods = &ti_gps_module_methods,
};


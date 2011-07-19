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


#define LOG_TAG "CameraHal"


#include "CameraHal.h"
#include "TICameraParameters.h"

extern "C" {

#include "memmgr.h"
#include "tiler.h"
//#include <timm_osal_interfaces.h>
//#include <timm_osal_trace.h>


};

namespace android {

///@todo Move these constants to a common header file, preferably in tiler.h
#define STRIDE_8BIT (4 * 1024)
#define STRIDE_16BIT (4 * 1024)


///Utility Macro Declarations
#define ZERO_OUT_ARR(a,b) { for(unsigned int i=0;i<b;i++) a[i]=NULL;}

#define ZERO_OUT_STRUCT(a, b) memset(a, 0, sizeof(b));

/*--------------------MemoryManager Class STARTS here-----------------------------*/
///@todo Change the name of the MemoryManager class to TilerMemoryManager to indicate that it allocates TILER buffers only
void* MemoryManager::allocateBuffer(int width, int height, const char* format, int &bytes, int numBufs)
{
    LOG_FUNCTION_NAME
    ///We allocate numBufs+1 because the last entry will be marked NULL to indicate end of array, which is used when freeing
    ///the buffers
    const uint numArrayEntriesC = (uint)(numBufs+1);

    MemAllocBlock *tMemBlock;


    ///Allocate a buffer array
    uint32_t *bufsArr = new uint32_t[numArrayEntriesC];
    if(!bufsArr)
        {
        CAMHAL_LOGEB("Allocation failed when creating buffers array of %d uint32_t elements", numArrayEntriesC);
        LOG_FUNCTION_NAME_EXIT
        return NULL;
        }

    ///Initialize the array with zeros - this will help us while freeing the array in case of error
    ///If a value of an array element is NULL, it means we didnt allocate it
    ZERO_OUT_ARR(bufsArr, numArrayEntriesC);

    ///If the bytes field is not zero, it means it is a 1-D tiler buffer request (possibly for image capture bit stream buffer)
    if(bytes!=0)
        {
        ///MemAllocBlock is the structure that describes the buffer alloc request to MemMgr
        tMemBlock = (MemAllocBlock*)malloc(sizeof(MemAllocBlock));

        if(!tMemBlock)
            {
            delete [] bufsArr;
            return NULL;
            }

        ZERO_OUT_STRUCT(tMemBlock, MemAllocBlock );

        ///1D buffers
        for (int i = 0; i < numBufs; i++)
            {
            tMemBlock->dim.len = bytes;
            tMemBlock->pixelFormat = PIXEL_FMT_PAGE;
            tMemBlock->stride = 0;
            CAMHAL_LOGDB("requested bytes = %d", bytes);
            CAMHAL_LOGDB("tMemBlock.dim.len = %d", tMemBlock->dim.len);
            bufsArr[i] = (uint32_t)MemMgr_Alloc(tMemBlock, 1);
            if(!bufsArr[i])
                {
                LOGE("Buffer allocation failed for iteration %d", i);
                goto error;
                }
            else
                {
                LOGD("Allocated Tiler PAGED mode buffer address[%x]", bufsArr[i]);
                }
            }

        }
    else ///If bytes is not zero, then it is a 2-D tiler buffer request
        {
        ///2D buffers
        ///MemAllocBlock is the structure that describes the buffer alloc request to MemMgr
        tMemBlock = (MemAllocBlock*)malloc(sizeof(MemAllocBlock)*2);

        if(!tMemBlock)
            {
            delete [] bufsArr;
            return NULL;
            }

        for(int i=0;i<2;i++)
            {
            ZERO_OUT_STRUCT(&tMemBlock[i], MemAllocBlock );
            }

        for (int i = 0; i < numBufs; i++)
            {
            int numAllocs = 1;
            pixel_fmt_t pixelFormat[2];
            int stride[2];

            if(!strcmp(format,(const char *) CameraParameters::PIXEL_FORMAT_YUV422I))
                {
                ///YUV422I format
                pixelFormat[0] = PIXEL_FMT_16BIT;
                stride[0] = STRIDE_16BIT;
                numAllocs = 1;
                }
            else if(!strcmp(format,(const char *) CameraParameters::PIXEL_FORMAT_YUV420SP))
                {
                ///YUV420 NV12 format
                pixelFormat[0] = PIXEL_FMT_8BIT;
                pixelFormat[1] = PIXEL_FMT_16BIT;
                stride[0] = STRIDE_8BIT;
                stride[1] = STRIDE_16BIT;
                numAllocs = 2;
                }
            else if(!strcmp(format,(const char *) CameraParameters::PIXEL_FORMAT_RGB565))
                {
                ///RGB 565 format
                pixelFormat[0] = PIXEL_FMT_16BIT;
                stride[0] = STRIDE_16BIT;
                numAllocs = 1;
                }
            else if(!strcmp(format,(const char *) TICameraParameters::PIXEL_FORMAT_RAW))
                {
                ///RAW format
                pixelFormat[0] = PIXEL_FMT_16BIT;
                stride[0] = STRIDE_16BIT;
                numAllocs = 1;
                }
            else
                {
                ///By default assume YUV420 NV12 format
                ///YUV420 NV12 format
                pixelFormat[0] = PIXEL_FMT_8BIT;
                pixelFormat[1] = PIXEL_FMT_16BIT;
                stride[0] = STRIDE_8BIT;
                stride[1] = STRIDE_16BIT;
                numAllocs = 2;
                }

            for(int index=0;index<numAllocs;index++)
                {
                tMemBlock[index].pixelFormat = pixelFormat[index];
                tMemBlock[index].stride = stride[index];
                tMemBlock[index].dim.area.width=  width;/*width*/
                tMemBlock[index].dim.area.height=  height;/*height*/
                }

            bufsArr[i] = (uint32_t)MemMgr_Alloc(tMemBlock, numAllocs);
            if(!bufsArr[i])
                {
                CAMHAL_LOGEB("Buffer allocation failed for iteration %d", i);
                goto error;
                }
            else
                {
                LOGD("Allocated Tiler PAGED mode buffer address[%x]", bufsArr[i]);
                }
            }

        }

        LOG_FUNCTION_NAME_EXIT


        ///Free the request structure before returning from the function
        free(tMemBlock);

        return (void*)bufsArr;

    error:
        LOGE("Freeing buffers already allocated after error occurred");
        freeBuffer(bufsArr);
        free(tMemBlock);

        if ( NULL != mErrorNotifier.get() )
            {
            mErrorNotifier->errorNotify(-ENOMEM);
            }

        LOG_FUNCTION_NAME_EXIT
        return NULL;
}

//TODO: Get needed data to map tiler buffers
//Return dummy data for now
uint32_t * MemoryManager::getOffsets()
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return NULL;
}

int MemoryManager::getFd()
{
    LOG_FUNCTION_NAME

    LOG_FUNCTION_NAME_EXIT

    return -1;
}

int MemoryManager::freeBuffer(void* buf)
{
    status_t ret = NO_ERROR;
    LOG_FUNCTION_NAME

    uint32_t *bufEntry = (uint32_t*)buf;

    if(!bufEntry)
        {
        CAMHAL_LOGEA("NULL pointer passed to freebuffer");
        LOG_FUNCTION_NAME_EXIT
        return BAD_VALUE;
        }

    while(*bufEntry)
        {
        ret |= MemMgr_Free((void*)*bufEntry++);
        }

    ///@todo Check if this way of deleting array is correct, else use malloc/free
    uint32_t * bufArr = (uint32_t*)buf;
    delete [] bufArr;

    LOG_FUNCTION_NAME_EXIT
    return ret;
}

status_t MemoryManager::setErrorHandler(ErrorNotifier *errorNotifier)
{
    status_t ret = NO_ERROR;

    LOG_FUNCTION_NAME

    if ( NULL == errorNotifier )
        {
        CAMHAL_LOGEA("Invalid Error Notifier reference");
        ret = -EINVAL;
        }

    if ( NO_ERROR == ret )
        {
        mErrorNotifier = errorNotifier;
        }

    LOG_FUNCTION_NAME_EXIT

    return ret;
}

};


/*--------------------MemoryManager Class ENDS here-----------------------------*/

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
/**
* @file CameraHal.cpp
*
* This file maps the Camera Hardware Interface to V4L2.
*
*/

#include "CameraHal.h"

#define VIDEO_DEVICE        "/dev/video5"
#define MIN_WIDTH           208 // 960 //820
#define MIN_HEIGHT          154 // 800 //616
#define PICTURE_WIDTH   2560 /* 5mp. 8mp - 3280 */
#define PICTURE_HEIGHT  2048 /* 5mp. 8mp - 2464 */
#define PIXEL_FORMAT           V4L2_PIX_FMT_YUYV
#define LOG_FUNCTION_NAME    LOGD("%d: %s() Executing...", __LINE__, __FUNCTION__);


namespace android {

int CameraHal::camera_device = 0;
wp<CameraHardwareInterface> CameraHal::singleton;


CameraHal::CameraHal()
                  : mParameters(),
                    mHeap(0),
                    mPictureHeap(0),
                    fcount(6),
                    nQueued(0),
                    nDequeued(0),
                    previewStopped(true),
                    doubledPreviewWidth(false),
                    doubledPreviewHeight(false),
                    mPreviewFrameSize(0),
                    mRawPictureCallback(0),
                    mJpegPictureCallback(0),
                    mPictureCallbackCookie(0),
                    mPreviewCallback(0),
                    mPreviewCallbackCookie(0),
                    mAutoFocusCallback(0),
                    mAutoFocusCallbackCookie(0),
                    mCurrentPreviewFrame(0)
{
    initDefaultParameters();
}

void CameraHal::initDefaultParameters()
{
    CameraParameters p;

    p.setPreviewSize(MIN_WIDTH, MIN_HEIGHT);
    p.setPreviewFrameRate(15);
    p.setPreviewFormat("yuv422sp");

    p.setPictureSize(PICTURE_WIDTH, PICTURE_HEIGHT);
    p.setPictureFormat("jpeg");

    if (setParameters(p) != NO_ERROR) {
        LOGE("Failed to set default parameters?!");
    }

}

bool CameraHal::initHeapLocked()
{
    LOG_FUNCTION_NAME

    int width, height;
    int nSizeBytes;
    struct v4l2_requestbuffers creqbuf;

    mParameters.getPreviewSize(&width, &height);

    LOGD("initHeapLocked: preview size=%dx%d", width, height);

    // Note that we enforce yuv422 in setParameters().
    int how_big = width * height * 2;
    nSizeBytes =  how_big;

    if (nSizeBytes & 0xfff)
    {
        how_big = (nSizeBytes & 0xfffff000) + 0x1000;
    }

    // If we are being reinitialized to the same size as before, no
    // work needs to be done.
    //if (how_big == mPreviewFrameSize)
        //return true;

    mPreviewFrameSize = how_big;
    LOGD("mPreviewFrameSize = 0x%x = %d", mPreviewFrameSize, mPreviewFrameSize);

    // Make a new mmap'ed heap that can be shared across processes.
    // use code below to test with pmem
    int nSurfaceFlingerHeapSize = mPreviewFrameSize;
    if (doubledPreviewWidth) nSurfaceFlingerHeapSize = nSurfaceFlingerHeapSize >> 1;
    if (doubledPreviewHeight) nSurfaceFlingerHeapSize = nSurfaceFlingerHeapSize >> 1;
    mSurfaceFlingerHeap = new MemoryHeapBase(nSurfaceFlingerHeapSize);
    mSurfaceFlingerBuffer = new MemoryBase(mSurfaceFlingerHeap, 0, nSurfaceFlingerHeapSize);

    //Need to add 0x20 to align to 32 byte boundary later
    //Need to add 256 to align to 128 byte boundary later
    mHeap = new MemoryHeapBase((mPreviewFrameSize + 0x20 + 256)* kBufferCount);
    LOGD("mHeap Base = %p \tSize = 0x%x = %d", mHeap->getBase(), mHeap->getSize(), mHeap->getSize());

    // Make an IMemory for each frame so that we can reuse them in callbacks.

    unsigned long base, offset;
    base = (unsigned long)mHeap->getBase();
    for (int i = 0; i < kBufferCount; i++) {

        /*Align buffer to 32 byte boundary */
        while ((base & 0x1f) != 0)
        {
            base++;
        }
        /* Buffer pointer shifted to avoid DSP cache issues */
        base += 128;
        offset = base - (unsigned long)mHeap->getBase();
        mBuffers[i] = new MemoryBase(mHeap, offset, mPreviewFrameSize);

        LOGD("Buffer %d: Base = %p Offset = 0x%x", i, base, offset);
        base = base + mPreviewFrameSize + 128;
    }


    /* Check if the camera driver can accept 'kBufferCount' number of buffers*/

    creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    creqbuf.memory = V4L2_MEMORY_USERPTR;
    creqbuf.count  = kBufferCount;
    if (ioctl(camera_device, VIDIOC_REQBUFS, &creqbuf) < 0){
        LOGE ("VIDIOC_REQBUFS Failed. errno = %d", errno);
        return false;
    }


    /* allocate user memory, and queue each buffer */
    for (unsigned int i = 0; i < creqbuf.count; ++i) {

        struct v4l2_buffer buffer;
        buffer.type = creqbuf.type;
        buffer.memory = creqbuf.memory;
        buffer.index = i;

        if (ioctl(camera_device, VIDIOC_QUERYBUF, &buffer) < 0) {
            LOGE("VIDIOC_QUERYBUF Failed");
            return false;
        }

        LOGD("buffer.length = %d, mPreviewFrameSize = %d", buffer.length, mPreviewFrameSize);
        ssize_t offset;
        size_t size;
        mBuffers[i]->getMemory(&offset, &size);
        buffer.length = mPreviewFrameSize;
        buffer.m.userptr = (unsigned long) (mHeap->getBase()) + offset;

        LOGD("User Buffer [%d].start = %p  length = %d\n", i, (void*)buffer.m.userptr, buffer.length);


        if (ioctl(camera_device, VIDIOC_QBUF, &buffer) < 0) {
            LOGE("CAMERA VIDIOC_QBUF Failed");
            return false;
        }
        nQueued++;
    }

    return true;
}

CameraHal::~CameraHal()
{
    singleton.clear();
}

sp<IMemoryHeap> CameraHal::getPreviewHeap() const
{
    LOG_FUNCTION_NAME
    //return mHeap;
    return mSurfaceFlingerHeap;
}

// ---------------------------------------------------------------------------

int CameraHal::previewThread()
{
    int w, h;
    unsigned long offset;
    void *croppedImage;
    struct v4l2_buffer cfilledbuffer;
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;

    if (!previewStopped){

        /* De-queue the next avaliable buffer */
        while (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
            LOGE("VIDIOC_DQBUF Failed");
        }
        nDequeued++;

        //LOGV("previewThread: generated frame to buffer %d", mCurrentPreviewFrame++);

        mParameters.getPreviewSize(&w, &h);
        if (doubledPreviewWidth || doubledPreviewHeight){
            croppedImage = cropImage(cfilledbuffer.m.userptr);
            if (doubledPreviewWidth) w = w >> 1;
            if (doubledPreviewHeight) h = h >> 1;
        }
        else {
            croppedImage = (void*)cfilledbuffer.m.userptr;
        }
            
        convertYUYVtoYUV422SP((uint8_t*)croppedImage, (uint8_t *) (mSurfaceFlingerHeap->getBase()), w, h);
        if (doubledPreviewWidth || doubledPreviewHeight){
            free(croppedImage);
        }
        
        // Notify the client of a new frame.
        mPreviewCallback(mSurfaceFlingerBuffer, mPreviewCallbackCookie);

        if (!previewStopped){
            while (ioctl(camera_device, VIDIOC_QBUF, &cfilledbuffer) < 0) {
                LOGE("VIDIOC_QBUF Failed.");
            }
            nQueued++;
        }
    }

    return NO_ERROR;
}

status_t CameraHal::startPreview(preview_callback cb, void* user)
{
    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);
    if (mPreviewThread != 0) {
        // already running
        return INVALID_OPERATION;
    }

    if ((camera_device = open(VIDEO_DEVICE, O_RDWR)) <= 0) {
        LOGE ("Could not open the camera device: %d, errno: %d", camera_device, errno);
        return -1;
    }

    int width, height;
    mParameters.getPreviewSize(&width, &height);
    struct v4l2_format format;
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = width;
    format.fmt.pix.height = height;
    format.fmt.pix.pixelformat = PIXEL_FORMAT;

    /* set size & format of the video image */
    if (ioctl(camera_device, VIDIOC_S_FMT, &format) < 0){
        LOGE ("Failed to set VIDIOC_S_FMT.");
        return -1;
    }

    if (!initHeapLocked()){
        LOGE("initHeapLocked failed");
        return -1;
    }

    /* turn on streaming */
    struct v4l2_requestbuffers creqbuf;
    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMON, &creqbuf.type) < 0) {
        LOGE("VIDIOC_STREAMON Failed");
        return -1;
    }

    previewStopped = false;

    mPreviewCallback = cb;
    mPreviewCallbackCookie = user;
    mPreviewThread = new PreviewThread(this);
    return NO_ERROR;
}

void CameraHal::stopPreview()
{
    LOG_FUNCTION_NAME

    sp<PreviewThread> previewThread;
    struct v4l2_requestbuffers creqbuf;

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        previewStopped = true;
    }

    if (mPreviewThread != 0) {

        struct v4l2_buffer cfilledbuffer;
        cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cfilledbuffer.memory = V4L2_MEMORY_USERPTR;
        int DQcount = nQueued - nDequeued;
        if (DQcount < 0)
            LOGE("Something seriously wrong. Dequeued > Queued");

        LOGD("No. of buffers remaining to be dequeued = %d", DQcount);
#if 0
        for (int i = 0; i < DQcount; i++){
            /* De-queue the next avaliable buffer */
            if (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
                LOGE("VIDIOC_DQBUF Failed");
            }
            LOGE("VIDIOC_DQBUF %d", i);
        }
#endif
        /* Turn off streaming */
        creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) == -1) {
            LOGE("VIDIOC_STREAMOFF Failed");
            //return;
        }
        LOGD("Turned off Streaming");

        close(camera_device);
    }

    { // scope for the lock
        Mutex::Autolock lock(mLock);
        previewThread = mPreviewThread;
    }

    // don't hold the lock while waiting for the thread to quit
    if (previewThread != 0) {
        previewThread->requestExitAndWait();
    }

    Mutex::Autolock lock(mLock);
    mPreviewThread.clear();
}

bool CameraHal::previewEnabled()
{
    return (!previewStopped);
}

// ---------------------------------------------------------------------------

int CameraHal::beginAutoFocusThread(void *cookie)
{
    LOG_FUNCTION_NAME

    CameraHal *c = (CameraHal *)cookie;
    return c->autoFocusThread();
}

int CameraHal::autoFocusThread()
{
    LOG_FUNCTION_NAME

    if (mAutoFocusCallback != NULL) {
        mAutoFocusCallback(true, mAutoFocusCallbackCookie);
        mAutoFocusCallback = NULL;

    LOG_FUNCTION_NAME

        return NO_ERROR;
    }
    return UNKNOWN_ERROR;
}

status_t CameraHal::autoFocus(autofocus_callback af_cb,
                                       void *user)
{
    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    if (mAutoFocusCallback != NULL) {
        return mAutoFocusCallback == af_cb ? NO_ERROR : INVALID_OPERATION;
    }

    mAutoFocusCallback = af_cb;
    mAutoFocusCallbackCookie = user;
    if (createThread(beginAutoFocusThread, this) == false)
        return UNKNOWN_ERROR;
    return NO_ERROR;
}

/*static*/ int CameraHal::beginPictureThread(void *cookie)
{
    LOG_FUNCTION_NAME

    CameraHal *c = (CameraHal *)cookie;
    return c->pictureThread();
}

int CameraHal::pictureThread()
{

    int w, h;
    int pictureSize;
    unsigned long base, offset;
    struct v4l2_buffer buffer;
    struct v4l2_format format;
    struct v4l2_buffer cfilledbuffer;
    struct v4l2_requestbuffers creqbuf;
    sp<MemoryBase> mPictureBuffer;
    sp<MemoryBase> memBase;

    LOG_FUNCTION_NAME

    if (mShutterCallback)
        mShutterCallback(mPictureCallbackCookie);

    mParameters.getPictureSize(&w, &h);
    LOGD("Picture Size: Width = %d \tHeight = %d", w, h);

    if ((camera_device = open(VIDEO_DEVICE, O_RDWR)) <= 0) {
        LOGE ("Could not open the camera device: %d, errno: %d", camera_device, errno);
        return -1;
    }

    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = w;
    format.fmt.pix.height = h;
    format.fmt.pix.pixelformat = PIXEL_FORMAT;

    /* set size & format of the video image */
    if (ioctl(camera_device, VIDIOC_S_FMT, &format) < 0){
        LOGE ("Failed to set VIDIOC_S_FMT.");
        return -1;
    }

    /* Check if the camera driver can accept 1 buffer */
    creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    creqbuf.memory = V4L2_MEMORY_USERPTR;
    creqbuf.count  = 1;
    if (ioctl(camera_device, VIDIOC_REQBUFS, &creqbuf) < 0){
        LOGE ("VIDIOC_REQBUFS Failed. errno = %d", errno);
        return -1;
    }

    pictureSize = w * h * 2;
    if (pictureSize & 0xfff)
    {
        pictureSize = (pictureSize & 0xfffff000) + 0x1000;
    }
    LOGD("pictureFrameSize = 0x%x = %d", pictureSize, pictureSize);

    // Make a new mmap'ed heap that can be shared across processes.
    if (mPictureHeap == 0) {
	    mPictureHeap = new MemoryHeapBase(pictureSize + 0x20 + 256);
    }
    base = (unsigned long)mPictureHeap->getBase();

    /*Align buffer to 32 byte boundary */
    while ((base & 0x1f) != 0)
    {
        base++;
    }

    /* Buffer pointer shifted to avoid DSP cache issues */
    base += 128;
    offset = base - (unsigned long)mPictureHeap->getBase();
    mPictureBuffer = new MemoryBase(mPictureHeap, offset, pictureSize);

    LOGD("Picture Buffer: Base = %p Offset = 0x%x", base, offset);

    buffer.type = creqbuf.type;
    buffer.memory = creqbuf.memory;
    buffer.index = 0;

    if (ioctl(camera_device, VIDIOC_QUERYBUF, &buffer) < 0) {
        LOGE("VIDIOC_QUERYBUF Failed");
        return -1;
    }

    buffer.length = pictureSize;
    buffer.m.userptr = (unsigned long) (mPictureHeap->getBase()) + offset;

    if (ioctl(camera_device, VIDIOC_QBUF, &buffer) < 0) {
        LOGE("CAMERA VIDIOC_QBUF Failed");
        return -1;
    }

    /* turn on streaming */
    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMON, &creqbuf.type) < 0) {
        LOGE("VIDIOC_STREAMON Failed");
        return -1;
    }

    LOGD("De-queue the next avaliable buffer");

    /* De-queue the next avaliable buffer */
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;
    while (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
        LOGE("VIDIOC_DQBUF Failed");
    }

    LOGD("pictureThread: generated a picture");

    /* turn off streaming */
    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) < 0) {
        LOGE("VIDIOC_STREAMON Failed");
        return -1;
    }

    close(camera_device);

    if (mRawPictureCallback) {
        sp<MemoryHeapBase> mYUV422SPPictureHeap = new MemoryHeapBase(cfilledbuffer.length + 256);
        memBase = new MemoryBase(mYUV422SPPictureHeap, 128, cfilledbuffer.length);
        void *yuv422buffer = (void *)((unsigned long)mYUV422SPPictureHeap->getBase() + 128);
        convertYUYVtoYUV422SP((uint8_t*)(cfilledbuffer.m.userptr), (uint8_t *) yuv422buffer, w, h);
        mRawPictureCallback(memBase, mPictureCallbackCookie);
    }


    if (mJpegPictureCallback) {

#if HARDWARE_OMX

        void *yuv422buffer = malloc(cfilledbuffer.length + 256);
        yuv422buffer = (void *)((unsigned long)yuv422buffer + 128);
        convertYUYVtoUYVY((uint8_t*)(cfilledbuffer.m.userptr), (uint8_t *) yuv422buffer, w, h);

        sp<MemoryBase> jpegMemBase = encodeImage(yuv422buffer, cfilledbuffer.length);

        yuv422buffer = (void *)((unsigned long)yuv422buffer - 128);
        free(yuv422buffer);

        mJpegPictureCallback(jpegMemBase, mPictureCallbackCookie);

/*
        sp<MemoryBase> jpegMemBase = encodeImage((void*)(cfilledbuffer.m.userptr), cfilledbuffer.length);
        mJpegPictureCallback(jpegMemBase, mPictureCallbackCookie);
*/
#else
        mJpegPictureCallback(mPictureBuffer, mPictureCallbackCookie);
#endif

    }

    return NO_ERROR;
}

status_t CameraHal::takePicture(shutter_callback shutter_cb,
                                         raw_callback raw_cb,
                                         jpeg_callback jpeg_cb,
                                         void* user)
{
    LOG_FUNCTION_NAME

    stopPreview();
    mShutterCallback = shutter_cb;
    mRawPictureCallback = raw_cb;
    mJpegPictureCallback = jpeg_cb;
    mPictureCallbackCookie = user;
    LOGD("Creating Picture Thread");
    //##############################TODO use  thread
    //##############################TODO use  thread
    //##############################TODO use  thread
    //##############################TODO use  thread
/*if (createThread(beginPictureThread, this) == false)
        return -1;
*/
    pictureThread();

    return NO_ERROR;
}

status_t CameraHal::cancelPicture(bool cancel_shutter,
                                           bool cancel_raw,
                                           bool cancel_jpeg)
{
    LOG_FUNCTION_NAME

    if (cancel_shutter) mShutterCallback = NULL;
    if (cancel_raw) mRawPictureCallback = NULL;
    if (cancel_jpeg) mJpegPictureCallback = NULL;
    return NO_ERROR;
}

status_t CameraHal::dump(int fd, const Vector<String16>& args) const
{
/*
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;
    AutoMutex lock(&mLock);
    if (mFakeCamera != 0) {
        mFakeCamera->dump(fd, args);
        mParameters.dump(fd, args);
        snprintf(buffer, 255, " preview frame(%d), size (%d), running(%s)\n", mCurrentPreviewFrame, mPreviewFrameSize, mPreviewRunning?"true": "false");
        result.append(buffer);
    } else {
        result.append("No camera client yet.\n");
    }
    write(fd, result.string(), result.size());
*/
    return NO_ERROR;
}

int CameraHal::validateSize(int w, int h)
{
    LOG_FUNCTION_NAME

    return true;
}


status_t CameraHal::setParameters(const CameraParameters& params)
{
    LOG_FUNCTION_NAME

    int w, h;
    int framerate;

    Mutex::Autolock lock(mLock);

    if (strcmp(params.getPreviewFormat(), "yuv422sp") != 0) {
        LOGE("Only yuv422sp preview is supported");
        return -1;
    }

    if (strcmp(params.getPictureFormat(), "jpeg") != 0) {
        LOGE("Only jpeg still pictures are supported");
        return -1;
    }

    params.getPreviewSize(&w, &h);
    if (!validateSize(w, h)) {
        LOGE("Preview size not supported");
        return -1;
    }

    params.getPictureSize(&w, &h);
    if (!validateSize(w, h)) {
        LOGE("Picture size not supported");
        return -1;
    }

    framerate = params.getPreviewFrameRate();
    // validate framerate

    mParameters = params;

    mParameters.getPreviewSize(&w, &h);
    doubledPreviewWidth = false;
    doubledPreviewHeight = false;
    if (w < MIN_WIDTH){
        w = w << 1;
        doubledPreviewWidth = true;
        LOGE("Double Preview Width");
    }
    if (h < MIN_HEIGHT){
        h= h << 1;
        doubledPreviewHeight = true;
        LOGE("Double Preview Height");
    }
    mParameters.setPreviewSize(w, h);

    return NO_ERROR;
}

CameraParameters CameraHal::getParameters() const
{
    LOG_FUNCTION_NAME
    CameraParameters params;

    {
        Mutex::Autolock lock(mLock);
        params = mParameters;
    }

    int w, h;
    params.getPreviewSize(&w, &h);
    if (doubledPreviewWidth){
        w = w >> 1;
    }
    if (doubledPreviewHeight){
        h= h >> 1;
    }
    params.setPreviewSize(w, h);
    return params;
}

void CameraHal::release()
{
    LOG_FUNCTION_NAME

    close(camera_device);
}


sp<CameraHardwareInterface> CameraHal::createInstance()
{
    LOG_FUNCTION_NAME

    if (singleton != 0) {
        sp<CameraHardwareInterface> hardware = singleton.promote();
        if (hardware != 0) {
            return hardware;
        }
    }

    sp<CameraHardwareInterface> hardware(new CameraHal());

    singleton = hardware;
    return hardware;
}


void* CameraHal::cropImage(unsigned long buffer)
{
    void *croppedImage;
    int w, h, w2, h2, src, dest, src_incr, dest_incr, count;

    mParameters.getPreviewSize(&w2, &h2);
    w = w2;
    h = h2;

    if (doubledPreviewWidth) w = w >> 1;
    if (doubledPreviewHeight) h= h >> 1;
    croppedImage = malloc( w * h * 2);

    src_incr = w2 << 1;
    dest_incr = w << 1;
    for(src = 0, dest = 0, count = 0; count < h ; src+=src_incr, dest+=dest_incr, count++)
    {
        memcpy((void *)((unsigned long)croppedImage + dest), (void *)(buffer + src), dest_incr);
    }
    return croppedImage;
}

void CameraHal::convertYUYVtoYUV422SP(uint8_t *inputBuffer, uint8_t *outputBuffer, int width, int height)
{
    int buffSize;
    int i, y, u, v;

    buffSize = width * height * 2;

    y = 0;
    u = buffSize >> 1;
    v = u + (u >> 1);

    for(i = 0; i < buffSize; i+=4)
    {
        outputBuffer[y] = inputBuffer[i+0]; y++;
        outputBuffer[u] = inputBuffer[i+1]; u++;
        outputBuffer[y] = inputBuffer[i+2]; y++;
        outputBuffer[v] = inputBuffer[i+3]; v++;
    }
}

#if HARDWARE_OMX

void CameraHal::convertYUYVtoUYVY(uint8_t *inputBuffer, uint8_t *outputBuffer, int width, int height)
{
    int buffSize;
    buffSize = width * height * 2;

    for(int i = 0; i < buffSize; i+=2)
    {
        outputBuffer[i] = inputBuffer[i+1];
        outputBuffer[i+1] = inputBuffer[i];
    }
}


sp<MemoryBase> CameraHal::encodeImage(void *buffer, uint32_t bufflen)
{
    int w, h, size;
    SkTIJPEGImageEncoder encoder;

    LOG_FUNCTION_NAME
    mParameters.getPictureSize(&w, &h);
    size =  (w * h) + 12288;

    // Make a new mmap'ed heap that can be shared across processes.
    sp<MemoryHeapBase> mJpegImageHeap = new MemoryHeapBase(size + 256);
    // Make an IMemory for each frame
    sp<MemoryBase>mJpegBuffer = new MemoryBase(mJpegImageHeap, 128, size);
    void *outBuffer = (OMX_U8 *)((unsigned long)(mJpegImageHeap->getBase()) + 128);

    encoder.encodeImage(outBuffer, size, buffer, bufflen, w, h, 100);

    return mJpegBuffer;
}

#endif

extern "C" sp<CameraHardwareInterface> openCameraHardware()
{
    LOG_FUNCTION_NAME

    return CameraHal::createInstance();
}

}; // namespace android


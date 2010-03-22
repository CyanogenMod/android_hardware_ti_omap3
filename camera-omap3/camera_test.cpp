#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include <pthread.h>

#include <ui/Surface.h>
#include <ui/ISurface.h>
#include <ui/ISurfaceFlingerClient.h>
#include <ui/Overlay.h>
#include <ui/SurfaceComposerClient.h>

#include <CameraHal.h>
#include <CameraHardwareInterface.h>

#define PRINTOVER(arg...)     LOGD(#arg)
#define LOG_FUNCTION_NAME         LOGD("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT    LOGD("%d: %s() EXIT", __LINE__, __FUNCTION__);
#define KEY_SATURATION      "saturation"
#define KEY_BRIGHTNESS      "brightness"
#define KEY_EXPOSURE        "exposure"
#define KEY_ZOOM            "zoom"
#define KEY_CONTRAST        "contrast"
#define KEY_SHARPNESS       "sharpness"
#define KEY_ISO             "iso"
#define KEY_CAF             "caf"
#define KEY_MODE            "mode"
#define KEY_COMPENSATION    "compensation"
#define KEY_ROTATION        "picture-rotation"
#define KEY_IPP             "ippMode"

#define COMPENSATION_OFFSET 20
#define DELIMITER           "|"

//todo
//#define STRESS_TEST 1

using namespace android;

namespace android {
    class Test {
    public:
        static const sp<ISurface>& getISurface(const sp<SurfaceControl>& s) {
            return s->getISurface();
        }
    };
};

int print_menu;
sp<CameraHardwareInterface> hardware;
sp<SurfaceComposerClient> client;
sp<SurfaceControl> previewControl;
sp<Surface> previewSurface;
sp<SurfaceControl> overlayControl;
sp<ISurface> overlaySurface;
Overlay *overlay;
CameraParameters params;
float compensation = 0.0;
double latitude = 0.0;
double longitude = 0.0;
double degree_by_step = 17.56097560975609756097;
double altitude = 0.0;
int awb_mode = 0;
int effects_mode = 0;
int scene_mode = 0;
int caf_mode = 0;
int rotation = 0;
bool reSizePreview = true;
bool hardwareActive = false;
int saturation = 0;
int zoomIdx = 0;
int contrast = 0;
int brightness = 0;
int sharpness = 0;
int iso_mode = 0;
int capture_mode = 0;
int exposure_mode = 0;
int ippIdx = 0;
int jpegQuality = 85;
timeval autofocus_start, picture_start;

#if STRESS_TEST

static pthread_attr_t st_attr;
static pthread_t st_hthread;
static void *st_priv;
static sem_t st_sem;
static int st_started;
static int st_thread_exit = 0;

#endif

const char *ipp_mode[] = { "off", "Chroma Suppression", "Edge Enhancement" };
const char *iso [] = { "auto", "100", "200", "400", "800"};
const char *effects [] = {
    "none",
    "mono",
    "negative",
    "solarize",
    "sepia",
    "whiteboard",
    "blackboard",
    "cool",
    "emboss",
};
const char *caf [] = { "Off", "On" };
const char *scene [] = {
    "auto",
    "portrait",
    "landscape",
    "night",
    "night-portrait",
    "fireworks",

};
const char *strawb_mode[] = {
    "auto",
    "incandescent",
    "fluorescent",
    "daylight",
};
const char *antibanding[] = {
    "off",
    "auto",
    "50hz",
    "60hz",
};
int antibanding_mode = 0;
const char *focus[] = {
    "auto",
    "infinity",
    "macro",
};
int focus_mode = 0;
const char *exposure[] = {"Auto", "Macro", "Portrait", "Landscape", "Sports", "Night", "Night Portrait", "Backlighting", "Manual"};
const char *capture[] = { "High Performance", "High Quality" };
const struct { int idx; const char *zoom_description; } zoom [] = {
    { 0, "1x"},
    { 1, "2x"},
    { 2, "3x"},
    { 3, "4x"},
};
const struct { int width, height; const char *desc; } previewSize[] = {
    { 176, 144, "176x144" },
    { 320, 240, "QVGA" },
    { 640, 480, "VGA" },
    { 800, 480, "WVGA" },
};

const struct { int width, height; const char *name; } captureSize[] = {
    {  320, 240,  "QVGA" },
    {  640, 480,  "VGA" },
    {  800, 600,  "SVGA" },
    { 1280, 960,  "1MP" },
    { 1600,1200,  "2MP" },
    { 2048,1536,  "3MP" },
    { 2560,2048,  "5MP" },
    { 3280,2464,  "8MP" },
};

const struct { int fps; } frameRate[] = {
    {0},
    {5},
    {10},
    {15},
    {20},
    {30}
};

int previewSizeIDX = ARRAY_SIZE(previewSize)-1;
int captureSizeIDX = ARRAY_SIZE(captureSize)-1;
int frameRateIDX = ARRAY_SIZE(frameRate)-1;

#if STRESS_TEST

static unsigned int capture_counter = 1;

#endif

int dump_preview = 0;

/** Calculate delay from a reference time */
unsigned long long timeval_delay(const timeval *ref)
{
    unsigned long long st, end, delay;
    timeval current_time;

    gettimeofday(&current_time, 0);

    st = ref->tv_sec * 1000000 + ref->tv_usec;
    end = current_time.tv_sec * 1000000 + current_time.tv_usec;
    delay = end - st;

    return delay;
}

/** Callback for takePicture() */
void my_shutter_callback(void* user)
{
    printf("shutter cb: user=0x%08X\n", (int)user);
}

/** Callback for takePicture() */
void my_raw_callback(const sp<IMemory>& mem, void* user)
{

    static int      counter = 1;
    unsigned char   *buff = NULL;
    int             size;
    int             fd = -1;
    char            fn[256];

    LOG_FUNCTION_NAME

    if (mem == NULL)
        goto out;

    fn[0] = 0;
    sprintf(fn, "/img%03d.yuv", counter);
    fd = open(fn, O_CREAT | O_WRONLY | O_SYNC | O_TRUNC, 0777);
    if(fd < 0)
        goto out;

    size = mem->size();
    if (size <= 0)
        goto out;

    buff = (unsigned char *)mem->pointer();
    if (!buff)
        goto out;

    if (size != write(fd, buff, size))
        printf("Bad Write int a %s error (%d)%s\n", fn, errno, strerror(errno));

    counter++;
    printf("%s: user=0x%08X, buffer=%08X, size=%d\n",
            __FUNCTION__, (int)user, (int)buff, size);

    if (fd >= 0)
        close(fd);

out:
    LOG_FUNCTION_NAME_EXIT
}

void saveFile(const sp<IMemory>& mem, void* user)
{
    static int      counter = 1;
    unsigned char   *buff = NULL;
    int             size;
    int             fd = -1;
    char            fn[256];

    LOG_FUNCTION_NAME

    if (mem == NULL)
        goto out;

    fn[0] = 0;
    sprintf(fn, "/preview%03d.yuv", counter);
    fd = open(fn, O_CREAT | O_WRONLY | O_SYNC | O_TRUNC, 0777);
    if(fd < 0)
        goto out;

    size = mem->size();
    if (size <= 0)
        goto out;

    buff = (unsigned char *)mem->pointer();
    if (!buff)
        goto out;

    if (size != write(fd, buff, size))
        printf("Bad Write int a %s error (%d)%s\n", fn, errno, strerror(errno));

    counter++;
    printf("%s: user=0x%08X, buffer=%08X, size=%d\n",
            __FUNCTION__, (int)user, (int)buff, size);

    if (fd >= 0)
        close(fd);

out:
    LOG_FUNCTION_NAME_EXIT
}

/** Callback for startPreview() */
void my_preview_callback(const sp<IMemory>& mem, void* user)
{
    if(dump_preview){
        saveFile(mem, user);
        dump_preview = 0;
    }
    printf(".");fflush(stdout);
}

/** Callback for takePicture() */
void my_jpeg_callback(const sp<IMemory>& mem, void* user)
{
    static int	counter = 1;
    unsigned char	*buff = NULL;
    int		size;
    int		fd = -1;
    char		fn[256];

    LOG_FUNCTION_NAME

    if (mem == NULL)
        goto out;

    fn[0] = 0;
    sprintf(fn, "/sdcard/img%03d.jpg", counter);
    fd = open(fn, O_CREAT | O_WRONLY | O_SYNC | O_TRUNC, 0777);
    if(fd < 0)
        goto out;

    size = mem->size();
    if (size <= 0)
        goto out;

    buff = (unsigned char *)mem->pointer();
    if (!buff)
        goto out;

    if (size != write(fd, buff, size))
        printf("Bad Write int a %s error (%d)%s\n", fn, errno, strerror(errno));

    counter++;
    printf("%s: user=0x%08X, buffer=%08X, size=%d stored at %s\n",
        __FUNCTION__, (int)user, (int)buff, size, fn);
out:
    if (fd >= 0)
        close(fd);

#if STRESS_TEST

    if (st_started)
        sem_post(&st_sem);

#endif

    LOG_FUNCTION_NAME_EXIT
}

/** Callback for autoFocus() */
void my_autofocus_callback(bool focused, void* user)
{

    printf("AutoFocus cb: focused=%s user=0x%08X time=%lld\n",
            (focused)?"OK":"FAIL", (int)user,
             timeval_delay(&autofocus_start));
}

void my_notify_callback(int32_t msgType,
               int32_t ext1,
               int32_t ext2,
               void* user)
{

    printf("Notify cb: %d %d %d %p\n",
           msgType, ext1, ext2, user);

    if(msgType&CAMERA_MSG_FOCUS) {
        printf("AutoFocus %s in %llu us\n",
               (ext1)?"OK":"FAIL", timeval_delay(&autofocus_start));
    }
    if(msgType&CAMERA_MSG_SHUTTER) {
        printf("Shutter done in %llu us\n",
               timeval_delay(&picture_start));
    }
}

void my_data_callback(int32_t msgType,
               const sp<IMemory>& dataPtr,
               void* user)
{
    printf("Data   cb: %d %p\n",
           msgType, user);

    if(msgType&CAMERA_MSG_PREVIEW_FRAME) {
        my_preview_callback(dataPtr, user);
    }
    if(msgType&CAMERA_MSG_RAW_IMAGE) {
        printf("RAW done in %llu us\n",
               timeval_delay(&picture_start));
        my_raw_callback(dataPtr, user);
    }
    if(msgType&CAMERA_MSG_COMPRESSED_IMAGE) {
        printf("JPEG done in %llu us\n",
               timeval_delay(&picture_start));
        my_jpeg_callback(dataPtr, user);
    }
}

void my_data_callback_timestamp(nsecs_t timestamp,
               int32_t msgType,
               const sp<IMemory>& dataPtr,
               void* user)
{
    printf("DataTime cb: %d %lld %p %p\n",
           msgType, timestamp, user, dataPtr.get());
}

#if STRESS_TEST

void rand_delay()
{
    int dly = 1;
    int min = 100000;       // 100ms
    int max = 1000000 - 1;  // ~1s
    struct timeval tv;

    gettimeofday(&tv, NULL);

    srand(tv.tv_usec);

    do {
        dly = rand();
    } while ((dly < min) || (dly > max));

    usleep(dly);
}

void *st_thread_func(void *arg)
{
    static int first_time_started = 1;

    st_started = 1;

    while (1) {

        if (first_time_started)
            first_time_started = 0;
        else
            sem_wait(&st_sem);

        if (st_thread_exit)
            break;

        if (reSizePreview) {
            if (overlay != NULL) {
                hardware->stopPreview();
                overlay->destroy();
            }
            overlay = new OverlayMMS(640, 480, 0);
            ((CameraHal *)hardware.get())->setOverlay(overlay);
        }
        hardware->setParameters(params);
        hardware->startPreview();

        rand_delay();

        hardware->setParameters(params);
        gettimeofday(&picture_start, 0);
        hardware->takePicture();
        hardware->stopPreview();
    }

    printf("Stress test thread exit\n");

    return NULL;
}

int create_stress_test()
{
    size_t stacksize = 512 * 1024;
    int ret = 0;

    sem_init(&st_sem, 0, 0);

    pthread_attr_init(&st_attr);

    pthread_attr_setstacksize(&st_attr, stacksize);

    ret = pthread_create(&st_hthread, &st_attr, st_thread_func, st_priv);

    if (0 > ret) {
        pthread_attr_destroy(&st_attr);
        printf("Create stress test thread failed\n");
        return -1;
    }

    printf("Stress test created\n");

    return 0;
}

int destroy_stress_test()
{
    st_thread_exit = 1;

    sem_post(&st_sem);

    if (0 > pthread_join(st_hthread, NULL)) {
        printf("Error while join stress test thread\n");
    }

    pthread_attr_destroy(&st_attr);

    sem_destroy(&st_sem);

    printf("Stress test destroyed\n");

    return 0;
}

#endif

int menu_gps()
{
    char ch;
    char coord_str[100];

    if (print_menu) {
        printf("\n\n== GPS MENU ============================\n\n");
        printf("   0. Get Parameters\n");
        printf("   e. Latitude:       %.20lf\n", latitude);
        printf("   d. Longitude:      %.20lf\n", longitude);
        printf("   c. Altitude:       %.20lf\n", altitude);
        printf("\n");
        printf("   q. Return to main menu\n");
        printf("\n");
        printf("   Choice: ");
    }

    ch = getchar();
    printf("%c", ch);

    print_menu = 1;
    switch (ch) {
        case '0':
            hardware->getParameters();
            break;

        case 'e':
            latitude += degree_by_step;
            if (latitude > 90.0) {
                latitude -= 180.0;
            }
            snprintf(coord_str, 100, "%.20lf", latitude);
            params.set(params.KEY_GPS_LATITUDE, coord_str);
            hardware->setParameters(params);
            break;

        case 'd':
            longitude += degree_by_step;
            if (longitude > 180.0) {
                longitude -= 360.0;
            }
            snprintf(coord_str, 100, "%.20lf", longitude);
            params.set(params.KEY_GPS_LONGITUDE, coord_str);
            hardware->setParameters(params);
            break;

        case 'c':
            altitude += 12345.67890123456789;
            if (altitude > 100000.0) {
                altitude -= 200000.0;
            }
            snprintf(coord_str, 100, "%.20lf", altitude);
            params.set(params.KEY_GPS_ALTITUDE, coord_str);
            hardware->setParameters(params);
            break;

        case 'Q':
        case 'q':
            return -1;

        default:
            print_menu = 0;
            break;
    }

    return 0;
}

int createOverlay(unsigned int width, unsigned int height)
{

    client = new SurfaceComposerClient();

    if ( NULL == client.get() ) {
        printf("Unable to establish connection to Surface Composer \n");
        
        return -1;
    }

    overlayControl = client->createSurface(getpid(), 0, width, height, 
                    PIXEL_FORMAT_UNKNOWN, ISurfaceComposer::ePushBuffers);

    if ( NULL == overlayControl.get() ) {
        printf("Unable to create Overlay control surface\n");

        return -1;
    }

    overlaySurface = Test::getISurface(overlayControl);

    if ( NULL == overlaySurface.get() ) {
        printf("Unable to get overlay ISurface interface\n"); 

        return -1;
    }

    sp<OverlayRef> overlayRef = overlaySurface->createOverlay(width, height, OVERLAY_FORMAT_DEFAULT);

    if( NULL == overlayRef.get() ) {
        printf("Unable to initialize Overlay device\n");
    }

    overlay = new Overlay(overlayRef);

    if ( NULL == overlay ) {
        printf("Unable to instantiate Overlay object\n");

        return -1;
    }

    previewControl = client->createSurface(getpid(), 0, width, height, PIXEL_FORMAT_RGB_565);

    if ( NULL == previewControl.get() ) {
        printf("Unable to create preview control surface\n");

        return -1;
    }

    previewSurface = previewControl->getSurface();

    if ( NULL == previewSurface.get() ) {
        printf("Unable to create preview surface\n");

        return -1;
    }

    client->openTransaction();
    previewControl->setLayer(100000);
    client->closeTransaction();

    Surface::SurfaceInfo info;

    previewSurface->lock(&info);
    ssize_t bpr = info.s * bytesPerPixel(info.format);
    memset((void *) info.bits, 0, bpr*info.h);
    previewSurface->unlockAndPost();

    client->openTransaction();
    previewControl->setPosition(0, 0);
    previewControl->setSize(width, height);
    client->closeTransaction();

    return 0;
}

int destroyOverlay()
{

    if ( NULL != overlaySurface.get() ) {
        overlaySurface.clear();
    }

    if ( NULL != overlayControl.get() ) {
        overlayControl->clear();
        overlayControl.clear();
    }

    if ( NULL != previewSurface.get() ) {
        previewSurface.clear();
    }

    if ( NULL != previewControl.get() ) {
        previewControl->clear();
        previewControl.clear();
    }

    if ( NULL != client.get() ) {
        client.clear();
    }

    return 0;
}

int openCamera()
{
    hardware = openCameraHardware();

    if ( NULL == hardware.get() ) {
        printf("Unable to CameraHAL \n");

        return -1;
    }

    hardware->setCallbacks(my_notify_callback, my_data_callback,
                   my_data_callback_timestamp, NULL);
    hardware->enableMsgType(CAMERA_MSG_ALL_MSGS);

    return 0;
}

int closeCamera()
{
    if( NULL == hardware.get() ) {
        printf("CameraHAL not initialized\n");

        return -1;
    }

    hardware->release();
    hardware.clear();

    return 0;
}

int startPreview()
{
    if ( reSizePreview && !hardwareActive ) {

        if( openCamera() < 0 ) {
            printf("Camera initialization failed\n");

            return -1;
        }

        hardware->setParameters(params);
        hardware->startPreview();

        params = hardware->getParameters();
        params.setPreviewSize(previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height);
        params.setPictureSize(captureSize[captureSizeIDX].width, captureSize[captureSizeIDX].height);

        if ( createOverlay(previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height) < 0 ) {
            printf("Error encountered during Overlay initialization\n");

            closeCamera();

            return -1;
        }

        hardware->setOverlay(overlay);

        reSizePreview = false;
        hardwareActive = true;
    } else if ( reSizePreview && hardwareActive ) {

        params.setPreviewSize(previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height);
        params.setPictureSize(captureSize[captureSizeIDX].width, captureSize[captureSizeIDX].height);

        if ( createOverlay(previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height) < 0 ) {
            printf("Error encountered during Overlay initialization\n");

            closeCamera();

            return -1;
        }

        hardware->setParameters(params);
        hardware->startPreview();
        hardware->setOverlay(overlay);

        reSizePreview = false;
        hardwareActive = true;
    }

    return 0;
}

void stopPreview()
{
    if ( hardwareActive ) {
        hardware->stopPreview();
        closeCamera();
        destroyOverlay();

        reSizePreview = true;
        hardwareActive = false;
    }
}

void initDefaults()
{

    antibanding_mode = 0;
    focus_mode = 0;
    previewSizeIDX = ARRAY_SIZE(previewSize)-1;
    captureSizeIDX = ARRAY_SIZE(captureSize)-1;
    frameRateIDX = ARRAY_SIZE(frameRate)-1;
    compensation = 0.0;
    awb_mode = 0;
    effects_mode = 0;
    scene_mode = 0;
    caf_mode = 0;
    rotation = 0;
    saturation = 0;
    zoomIdx = 0;
    contrast = 0;
    brightness = 0;
    sharpness = 0;
    iso_mode = 0;
    capture_mode = 0;
    exposure_mode = 0;
    ippIdx = 0;
    jpegQuality = 85;

    params.setPreviewSize(previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height);
    params.setPictureSize(captureSize[captureSizeIDX].width, captureSize[captureSizeIDX].height);
    params.set(KEY_ROTATION, rotation);
    params.set(KEY_COMPENSATION, compensation);
    params.set(params.KEY_WHITE_BALANCE, strawb_mode[awb_mode]);
    params.set(KEY_MODE, (capture_mode + 1));
    params.set(params.KEY_SCENE_MODE, scene[scene_mode]);
    params.set(KEY_CAF, caf_mode);
    params.set(KEY_ISO, iso_mode);
    params.set(KEY_SHARPNESS, sharpness);
    params.set(KEY_CONTRAST, contrast);
    params.set(KEY_ZOOM, zoom[zoomIdx].idx);
    params.set(KEY_EXPOSURE, exposure_mode);
    params.set(KEY_BRIGHTNESS, brightness);
    params.set(KEY_SATURATION, saturation);
    params.set(params.KEY_EFFECT, effects[effects_mode]);
    params.setPreviewFrameRate(frameRate[frameRateIDX].fps);
    params.set(params.KEY_ANTIBANDING, antibanding[antibanding_mode]);
    params.set(params.KEY_FOCUS_MODE, focus[focus_mode]);
    params.set(KEY_IPP, ippIdx);
    params.set(CameraParameters::KEY_JPEG_QUALITY, jpegQuality);

    if ( hardwareActive )
        hardware->setParameters(params);
}

int menu()
{
    char ch;

    if (print_menu) {
        printf("\n\n== MAIN MENU ===========================\n\n");
        printf("   0. Reset to defaults\n");
        printf("   1. Start Preview\n");
        printf("   2. Stop Preview\n");
        printf("   3. Picture Rotation:       %3d degree\n", rotation );
        printf("   4. Preview size:   %4d x %4d\n",
            previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height);
        printf("   5. Picture size:   %4d x %4d - %s\n",
            captureSize[captureSizeIDX].width, captureSize[captureSizeIDX].height,
            captureSize[captureSizeIDX].name);
        printf("   7. EV offset:      %4.1f\n", compensation);
        printf("   8. AWB mode:       %s\n", strawb_mode[awb_mode]);
        printf("   z. Zoom            %s\n", zoom[zoomIdx].zoom_description);
        printf("   j. Exposure        %s\n", exposure[exposure_mode]);
        printf("   e. Effect:         %s\n", effects[effects_mode]);
        printf("   w. Scene:          %s\n", scene[scene_mode]);
        printf("   s. Saturation:     %d\n", saturation);
        printf("   c. Contrast:       %d\n", contrast);
        printf("   h. Sharpness:      %d\n", sharpness);
        printf("   b. Brightness:     %d\n", brightness);
        printf("   y. Continuous AF:  %s\n", caf[caf_mode]);
        printf("   r. Framerate:     %3d\n", frameRate[frameRateIDX].fps);
        printf("   x. Antibanding:    %s\n", antibanding[antibanding_mode]);
        printf("   g. Focus mode:     %s\n", focus[focus_mode]);
        printf("   i. ISO mode:       %s\n", iso[iso_mode]);
        printf("   u. Capture Mode:   %s\n", capture[capture_mode]);
        printf("   k. IPP Mode:       %s\n", ipp_mode[ippIdx]);
        printf("   o. Jpeg Quality:   %d\n", jpegQuality);
        printf("   f. Auto Focus\n");
        printf("   p. Take picture\n");

#if STRESS_TEST

        printf("   t. Start stress test\n");

#endif

        printf("   a. GEO tagging settings menu\n");
        printf("\n");
        printf("   q. Quit\n");
        printf("\n");
        printf("   Choice: ");
    }

    ch = getchar();
    printf("%c", ch);

    print_menu = 1;
    switch (ch) {
    case '0':
        initDefaults();
        break;

    case '1':
        if ( startPreview() < 0 ) {
            printf("Error while starting preview\n");

            return -1;
        }

        break;

    case '2':
        stopPreview();
        break;

    case '3':
        rotation += 90;
        rotation %= 360;
        params.set(KEY_ROTATION, rotation);
        if ( hardwareActive )
            hardware->setParameters(params);

        break;

    case '4':
        previewSizeIDX += 1;
        previewSizeIDX %= ARRAY_SIZE(previewSize);
        params.setPreviewSize(previewSize[previewSizeIDX].width, previewSize[previewSizeIDX].height);
        reSizePreview = true;
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case '5':
        captureSizeIDX += 1;
        captureSizeIDX %= ARRAY_SIZE(captureSize);
        params.setPictureSize(captureSize[captureSizeIDX].width, captureSize[captureSizeIDX].height);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case '7':
        if( compensation > 2.0){
            compensation = -2.0;
        } else {
            compensation += 0.1;
        }
        params.set(KEY_COMPENSATION, (int) (compensation * 10) + COMPENSATION_OFFSET);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case '8':
        awb_mode++;
        awb_mode %= ARRAY_SIZE(strawb_mode);
        params.set(params.KEY_WHITE_BALANCE, strawb_mode[awb_mode]);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;


    case 'o':
    case 'O':
        if ( jpegQuality >= 100) {
            jpegQuality = 0;
        } else {
            jpegQuality += 5;
        }
        params.set(CameraParameters::KEY_JPEG_QUALITY, jpegQuality);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'k':
    case 'K':
        ippIdx += 1;
        ippIdx %= ARRAY_SIZE(ipp_mode);
        params.set(KEY_IPP, ippIdx);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;


    case 'u':
    case 'U':
        capture_mode++;
        capture_mode %= ARRAY_SIZE(capture);
        params.set(KEY_MODE, (capture_mode + 1));
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'w':
    case 'W':
        scene_mode++;
        scene_mode %= ARRAY_SIZE(scene);
        params.set(params.KEY_SCENE_MODE, scene[scene_mode]);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'y':
    case 'Y':
        caf_mode++;
        caf_mode %= ARRAY_SIZE(caf);
        params.set(KEY_CAF, caf_mode);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'i':
    case 'I':
        iso_mode++;
        iso_mode %= ARRAY_SIZE(iso);
        params.set(KEY_ISO, iso_mode);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'h':
    case 'H':
        if( sharpness >= 100){
            sharpness = 0;
        } else {
            sharpness += 10;
        }
        params.set(KEY_SHARPNESS, sharpness);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'c':
    case 'C':
        if( contrast >= 100){
            contrast = -100;
        } else {
            contrast += 10;
        }
        params.set(KEY_CONTRAST, contrast);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'z':
    case 'Z':
        zoomIdx++;
        zoomIdx %= ARRAY_SIZE(zoom);
        params.set(KEY_ZOOM, zoom[zoomIdx].idx);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'j':
    case 'J':
        exposure_mode++;
        exposure_mode %= ARRAY_SIZE(exposure);
        params.set(KEY_EXPOSURE, exposure_mode);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'b':
    case 'B':
        if ( brightness >= 100) {
            brightness = -100;
        } else {
            brightness += 10;
        }
        params.set(KEY_BRIGHTNESS, brightness);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;
        
    case 's':
    case 'S':
        if ( saturation >= 100) {
            saturation = -100;
        } else {
            saturation += 10;
        }
        params.set(KEY_SATURATION, saturation);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'e':
    case 'E':
        effects_mode++;
        effects_mode %= ARRAY_SIZE(effects);
        params.set(params.KEY_EFFECT, effects[effects_mode]);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'r':
    case 'R':
	    frameRateIDX += 1;
        frameRateIDX %= ARRAY_SIZE(frameRate);
        params.setPreviewFrameRate(frameRate[frameRateIDX].fps);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'X':
    case 'x':
        antibanding_mode++;
        antibanding_mode %= ARRAY_SIZE(antibanding);
        params.set(params.KEY_ANTIBANDING, antibanding[antibanding_mode]);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'g':
    case 'G':
        focus_mode++;
        focus_mode %= ARRAY_SIZE(focus);
        params.set(params.KEY_FOCUS_MODE, focus[focus_mode]);
        if ( hardwareActive )
            hardware->setParameters(params);
        break;

    case 'F':
    case 'f':
        gettimeofday(&autofocus_start, 0);
        if ( hardwareActive )
            hardware->autoFocus();
        break;

    case 'P':
    case 'p':
        hardware->setParameters(params);
        gettimeofday(&picture_start, 0);
        if ( hardwareActive )
            hardware->takePicture();
        break;



#if STRESS_TEST

    case 'T':
    case 't':
        ret = create_stress_test();
        if (ret < 0)
            return -1;
        break;

#endif

    case 'N':
    case 'n':
        dump_preview = 1;
        break;

    // GPS menu
    case 'a':
        while (1) {
            if ( menu_gps() < 0)
                break;
        };
        break;

    case 'Q':
    case 'q':

#if STRESS_TEST

        if (st_started) {
            ret = destroy_stress_test();
            if (ret < 0) {
                printf("Destroy stress test thread failed\n");
                return -1;
            }
        }

#endif

        stopPreview();
        return -1;

    default:
        print_menu = 0;
        break;
    }

    return 0;
}

char *load_script(char *config)
{
    FILE *infile;
    size_t fileSize;
    char *script;
    size_t nRead = 0;

    infile = fopen(config, "r");

    if( (NULL == infile)){

        printf("Error while opening script file %s!\n", config);
        return NULL;
    }

    fseek(infile, 0, SEEK_END);
    fileSize = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    script = (char *) malloc(fileSize);
    if ( NULL == script ) {
        printf("Unable to allocate buffer for the script\n");

        return NULL;
    }

    if((nRead = fread(script, 1, fileSize, infile)) != fileSize){
        printf("Error while reading script file!\n");

        free(script);
        return NULL;
    }

    return script;
}

int execute_script(char *script)
{
    char *cmd, *ctx;
    char id;
    unsigned int i;
    int dly;

    LOG_FUNCTION_NAME

    cmd = strtok_r((char *) script, DELIMITER, &ctx);
    while( NULL != cmd ){
        id = cmd[0];
        printf("%s \n", cmd);

        switch(id){
            case '1':
                if ( startPreview() < 0 ) {
                    printf("Error while starting preview\n");

                    return -1;
                }

                break;

            case '2':
                hardware->stopPreview();
                hardware->release();
                break;

            case '3':
                rotation = atoi(cmd + 1);
                params.set(KEY_ROTATION, rotation);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case '4':
                for(i = 0; i < ARRAY_SIZE(previewSize); i++){
                    if( strcmp((cmd + 1), previewSize[i].desc) == 0)
                        break;
                }

                previewSizeIDX = i;
                params.setPreviewSize(previewSize[i].width, previewSize[i].height);
                reSizePreview = true;
                break;

            case '5':
                for(i = 0; i < ARRAY_SIZE(captureSize); i++){
                    if( strcmp((cmd + 1), captureSize[i].name) == 0)
                        break;
                }

                params.setPictureSize(captureSize[i].width, captureSize[i].height);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case '7':
                compensation = atof(cmd + 1);
                params.set(KEY_COMPENSATION, (int) (compensation * 10) + COMPENSATION_OFFSET);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case '8':
                params.set(params.KEY_WHITE_BALANCE, (cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'o':
            case 'O':
                jpegQuality = atoi(cmd + 1);
                params.set(CameraParameters::KEY_JPEG_QUALITY, jpegQuality);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'k':
            case 'K':
                params.set(KEY_IPP, (cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'u':
            case 'U':
                params.set(KEY_MODE, (cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'w':
            case 'W':
                params.set(params.KEY_SCENE_MODE, (cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'y':
            case 'Y':
                caf_mode = atoi(cmd + 1);
                params.set(KEY_CAF, caf_mode);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'i':
            case 'I':
                iso_mode = atoi(cmd + 1);
                params.set(KEY_ISO, iso_mode);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'h':
            case 'H':
                sharpness = atoi(cmd + 1);
                params.set(KEY_SHARPNESS, sharpness);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'c':
            case 'C':
                contrast = atoi(cmd + 1);
                params.set(KEY_CONTRAST, contrast);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'z':
            case 'Z':
                params.set(KEY_ZOOM, atoi(cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'j':
            case 'J':
                params.set(KEY_EXPOSURE, (cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'b':
            case 'B':
                brightness = atoi(cmd + 1);
                params.set(KEY_BRIGHTNESS, brightness);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 's':
            case 'S':
                saturation = atoi(cmd + 1);
                params.set(KEY_SATURATION, saturation);
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'e':
            case 'E':
                params.set(params.KEY_EFFECT, (cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'r':
            case 'R':
                params.setPreviewFrameRate(atoi(cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'X':
            case 'x':
                params.set(params.KEY_ANTIBANDING, (cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'g':
            case 'G':
                params.set(params.KEY_FOCUS_MODE, (cmd + 1));
                if ( hardwareActive )
                    hardware->setParameters(params);
                break;

            case 'F':
            case 'f':
                gettimeofday(&autofocus_start, 0);
                if ( hardwareActive )
                    hardware->autoFocus();
                break;

            case 'P':
            case 'p':
                hardware->setParameters(params);
                gettimeofday(&picture_start, 0);
                if ( hardwareActive )
                    hardware->takePicture();
                break;

            case 'D':
            case 'd':
                dly = atoi(cmd + 1);

                dly *= 1000000;
                usleep(dly);
                break;

            case 'Q':
            case 'q':

#if STRESS_TEST

                if (st_started) {
                    ret = destroy_stress_test();
                    if (ret < 0) {
                        printf("Destroy stress test thread failed\n");
                        return -1;
                    }
                }

#endif

                stopPreview();
                goto exit;

            default:
                break;
        }

        cmd = strtok_r(NULL, DELIMITER, &ctx);
    }

exit:

    return 0;
}

int main(int argc, char *argv[])
{
    char *cmd;

    if( openCamera() < 0 ) {
        printf("Camera initialization failed\n");

        return -1;
    }

    hardwareActive = true;
    params = hardware->getParameters();
    initDefaults();

    cmd = NULL;
    if ( argc < 2 ) {

        print_menu = 1;
        while (1) {
            if ( menu() < 0)
                break;
        };

    } else {

        cmd = load_script(argv[1]);

        if( cmd != NULL) {

            execute_script(cmd);

            free(cmd);
        }
    }

    return 0;
}


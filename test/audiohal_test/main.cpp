#ifndef     AUDIO_TEST_CPP
#define AUDIO_TEST_CPP
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <getopt.h>
#include <utils/Log.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <media/AudioRecord.h>

#include <hardware_legacy/AudioHardwareInterface.h>

#include "AudioHardwareALSA.h"


#define BUFFER_LENGTH    4096
#define FRAME_COUNT 80

#define LLONG_MAX    9223372036854775807LL

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM AudioSystem::PCM_16_BIT


using namespace android;

typedef struct g_AudioHalStreamIn_t {
    uint32_t device;
    uint32_t channels;
    uint32_t rate;
    int format;
    AudioStreamIn* streamIn;
    }g_AudioHalStreamIn;

typedef struct g_AudioHalStreamOut_t {
    uint32_t device;
    uint32_t channels;
    uint32_t rate;
    int format;
    AudioStreamOut* streamOut;
    }g_AudioHalStreamOut;

struct g_AudioHalParams_t {
      AudioHardwareALSA* hardware;
      g_AudioHalStreamIn streamInParams;
      g_AudioHalStreamOut streamOutParams;
    };

g_AudioHalParams_t g_AudioHalParams;

// use emulated popcount optimization
// http://www.df.lth.se/~john_e/gems/gem002d.html
static inline uint32_t popCount(uint32_t u)
{
    u = ((u&0x55555555) + ((u>>1)&0x55555555));
    u = ((u&0x33333333) + ((u>>2)&0x33333333));
    u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
    u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
    u = ( u&0x0000ffff) + (u>>16);
    return u;
}

enum test_type_t {
    TEST_PLAY = 0,
    TEST_REC,
    TEST_LOOPBACK,
    TEST_DUPLEX,
    TEST_UNDEFINED
};


struct wav_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t riff_fmt;
    uint32_t fmt_id;
    uint32_t fmt_sz;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;       /* sample_rate * num_channels * bps / 8 */
    uint16_t block_align;     /* num_channels * bps / 8 */
    uint16_t bits_per_sample;
    uint32_t data_id;
    uint32_t data_sz;
};

char buffer[BUFFER_LENGTH];

static char *next;
static unsigned avail;
static float m_volume = 1; //default maximum
unsigned int buffer_size = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char playback_file[100] = {0};
char rec_file[100] = {0};
int timelimit = 0;

void audioHal_Initialize()
{
    g_AudioHalParams.hardware = new AudioHardwareALSA();
    if(g_AudioHalParams.hardware == NULL) {
        printf("Failed to create Hardware \n");
        exit(-1);
    }
    /*initializing in params */
    g_AudioHalParams.streamInParams.device = AudioSystem::DEVICE_IN_BUILTIN_MIC;
    g_AudioHalParams.streamInParams.channels = AudioSystem::CHANNEL_IN_LEFT;
    g_AudioHalParams.streamInParams.format= AudioSystem::PCM_16_BIT;
    g_AudioHalParams.streamInParams.rate = 8000;
    g_AudioHalParams.streamInParams.streamIn = NULL;

    /*initializing out params */
    g_AudioHalParams.streamOutParams.device = AudioSystem::DEVICE_OUT_WIRED_HEADSET;
    g_AudioHalParams.streamOutParams.channels = AudioSystem::CHANNEL_IN_LEFT | AudioSystem::CHANNEL_IN_RIGHT;
    g_AudioHalParams.streamOutParams.format= AudioSystem::PCM_16_BIT;
    g_AudioHalParams.streamOutParams.rate = 48000;
    g_AudioHalParams.streamOutParams.streamOut = NULL;

}

int audioStreamInOpen()
{

    AudioStreamIn* in = NULL;
    uint32_t mFlags = 0 ;
    status_t status;
    fprintf(stderr,"opening the input stream device 0x%x channels %d rate %d\n",
                    g_AudioHalParams.streamInParams.device,
                    g_AudioHalParams.streamInParams.channels,
                    g_AudioHalParams.streamInParams.rate);

    in = g_AudioHalParams.hardware->openInputStream(
            g_AudioHalParams.streamInParams.device,
            &g_AudioHalParams.streamInParams.format,
            &g_AudioHalParams.streamInParams.channels,
            &g_AudioHalParams.streamInParams.rate,
            &status,
            (AudioSystem::audio_in_acoustics) mFlags);
    if (in == NULL) {
        fprintf(stderr,"openInputStream failed for device 0x%x\n",g_AudioHalParams.streamInParams.device);
        return -1;
    } else if (status != NO_ERROR){
        fprintf(stderr,"openInputStream return status %d\n",status);
        g_AudioHalParams.hardware->closeInputStream(in);
        return -1;
    }
    g_AudioHalParams.streamInParams.streamIn = in;

    return 0;
}


int wav_rec()
{
    struct wav_header hdr;
    int fd;
    unsigned total = 0, size;
    unsigned char tmp;
    static off64_t count = 0;
    ssize_t bytes_read;

    pthread_mutex_lock (&mutex);
    if (audioStreamInOpen() < 0 ) {
        pthread_mutex_unlock (&mutex);
        return -1;
    }
    g_AudioHalParams.hardware->setMasterVolume(m_volume);
    pthread_mutex_unlock (&mutex);
    size = (unsigned int)g_AudioHalParams.streamInParams.streamIn->bufferSize();
    //adjusting buffer size to match audioHal
    size = size/popCount(g_AudioHalParams.streamInParams.channels);
    printf("Rec: set buffer size = %d\n", size);
    printf("Rec: opening file to record %s\n", rec_file);

    fd = open(rec_file, O_CREAT | O_RDWR, 777);
    if (fd < 0) {
        fprintf(stderr,"Rec: cannot open output file\n");
        g_AudioHalParams.hardware->closeInputStream(g_AudioHalParams.streamInParams.streamIn);
        return -1;
    }
    hdr.riff_id = ID_RIFF;
    hdr.riff_sz = 0;
    hdr.riff_fmt = ID_WAVE;
    hdr.fmt_id = ID_FMT;
    hdr.fmt_sz = 16;
    hdr.audio_format = g_AudioHalParams.streamInParams.format;
    hdr.num_channels = popCount(g_AudioHalParams.streamInParams.channels);
    hdr.sample_rate = g_AudioHalParams.streamInParams.rate;
    hdr.byte_rate = hdr.sample_rate * hdr.num_channels * 2;
    hdr.block_align = hdr.num_channels * 2;
    hdr.bits_per_sample = 16;
    hdr.data_id = ID_DATA;
    hdr.data_sz = 0;

    write(fd, &hdr, sizeof(hdr));

    fcntl(0, F_SETFL, O_NONBLOCK);

    if (timelimit == 0) {
        count = LLONG_MAX;
    } else {
        if (hdr.audio_format == AudioSystem::PCM_16_BIT)
            count = hdr.sample_rate * hdr.num_channels * 2;
        else if (hdr.audio_format == AudioSystem::PCM_8_BIT)
            count = hdr.sample_rate * hdr.num_channels ;
        count *= (off64_t)timelimit;
    }
    if (count > LLONG_MAX)
        count = LLONG_MAX;

    printf("\nRec: *** RECORDING * HIT ENTER TO STOP ***\n");
    for (;;) {

        while (read(0, &tmp, 1) == 1) {
            printf("read %c\n",tmp);
            if (tmp == 10)
            {
                printf("Rec: Recording is over.\n");
                goto done;
            }
        }
        printf("reading .......\n");
        bytes_read = g_AudioHalParams.streamInParams.streamIn->read(buffer, size);
        count = count - bytes_read;
        if (write(fd, buffer, bytes_read) != (signed) bytes_read) {
            fprintf(stderr,"Rec: cannot write buffer\n");
            goto fail;
        }
        total += bytes_read;
        if(count <= 0)
        {
            printf("******* Recording has completed *******\n");
            break;
        }
    }

done:
    count = 0;
    printf("Rec: Record done, total=%d\n", total);
    /* update lengths in header */
    hdr.data_sz = total;
    hdr.riff_sz = total + 8 + 16 + 8;
    lseek(fd, 0, SEEK_SET);
    write(fd, &hdr, sizeof(hdr));
    close(fd);
    pthread_mutex_lock (&mutex);
    g_AudioHalParams.hardware->closeInputStream(g_AudioHalParams.streamInParams.streamIn);
    pthread_mutex_unlock (&mutex);
    return 0;

fail:
    printf("Rec: Record failed\n");
    close(fd);
    unlink(rec_file);
    pthread_mutex_lock (&mutex);
    g_AudioHalParams.hardware->closeInputStream(g_AudioHalParams.streamInParams.streamIn);
    pthread_mutex_unlock (&mutex);
    return -1;
}

int audioStreamOutOpen()
{

    AudioStreamOut* out = NULL;
    uint32_t mFlags = 0 ;
    status_t status;
    fprintf(stderr,"opening the output stream device 0x%x channels %d rate %d\n",
                    g_AudioHalParams.streamOutParams.device,
                    g_AudioHalParams.streamOutParams.channels,
                    g_AudioHalParams.streamOutParams.rate);

    out = g_AudioHalParams.hardware->openOutputStream(
            g_AudioHalParams.streamOutParams.device,
            &g_AudioHalParams.streamOutParams.format,
            &g_AudioHalParams.streamOutParams.channels,
            &g_AudioHalParams.streamOutParams.rate,
            &status);

    if (out == NULL) {
        fprintf(stderr,"openOutputStream failed for device 0x%x\n",g_AudioHalParams.streamOutParams.device);
        return -1;
    } else if (status != NO_ERROR){
        fprintf(stderr,"openOutputStream return status %d\n",status);
        g_AudioHalParams.hardware->closeOutputStream(out);
        return -1;
    }
    g_AudioHalParams.streamOutParams.streamOut = out;

    return 0;
}

void re_route(uint32_t device)
{
    pthread_mutex_lock (&mutex);
    g_AudioHalParams.hardware->closeOutputStream(g_AudioHalParams.streamOutParams.streamOut);
    g_AudioHalParams.streamOutParams.device = device;
    printf("re-opening.....");
    audioStreamOutOpen();
    pthread_mutex_unlock (&mutex);
}
int read_input(char* string, uint32_t time)
{
    fd_set read_fds;
    struct timeval timeout;
    int rc;
    /* Set time limit. */
    timeout.tv_sec = (time_t)time;
    timeout.tv_usec = 0;
    /* Create a descriptor set containing our two sockets.  */
    FD_ZERO(&read_fds);
    FD_SET(0, &read_fds);
    rc = select(1, &read_fds, NULL, NULL, &timeout);
    if (rc <= 0) {
        //perror("select failed");
        return -1;
    } else if (rc > 0) {
        if (FD_ISSET(0, &read_fds)) {
            while (read(0, string, sizeof(string)) > 0 && string[0] != 0x0) {
                return strlen(string);
            }
        }
    }
    return -1;
}

void process_commands(char* cmd)
{
    char cmd_buf[1024];
    int ch;
    printf("Executing command %c \n", *cmd);
    /* ignore non characters */
    if (*cmd < '!' || *cmd > '~')
        return;

    switch(cmd[0]) {
        /*device change */
        case 'd' :
            printf("Enter the device no to route - speakers/headset 0/1\n");
            /*flushing prev data */
            if(read_input(cmd_buf, 2) > 0) {
                switch(cmd_buf[0]) {
                    case '0':
                        if(g_AudioHalParams.streamOutParams.device ==(AudioSystem::DEVICE_OUT_WIRED_HEADSET)) {
                            printf("Routing playback to Speakers\n");
                            re_route(AudioSystem::DEVICE_OUT_SPEAKER);
                        }
                        break;

                    case '1':
                        if(g_AudioHalParams.streamOutParams.device == (AudioSystem::DEVICE_OUT_SPEAKER)) {
                            printf("Routing playback to Headset\n");
                            re_route(AudioSystem::DEVICE_OUT_WIRED_HEADSET);
                        }
                        break;
                    default:
                        printf("UNKNOWN OPTION\n");
                }
            } else
                printf("Re-enter the option\n");

            memset(&cmd_buf,0,sizeof(cmd_buf));
            break;
        default:
            printf("Not supported command %c\n",cmd[0]);
            break;
    }
}
int pcm_play(int (*fill)(void *buf, unsigned size))
{
    FILE *infile;
    int set = 0;
    ssize_t rFrames;
    status_t status = 0;
    int format = 0;
    char *play_buffer = NULL;

    pthread_mutex_lock (&mutex);
    if (audioStreamOutOpen() < 0 ) {
        pthread_mutex_unlock (&mutex);
        return -1;
    }

    g_AudioHalParams.hardware->setMasterVolume(m_volume);
    pthread_mutex_unlock (&mutex);
    buffer_size = (unsigned int)g_AudioHalParams.streamOutParams.streamOut->bufferSize();
    printf("Play: buffer size=%d\n", buffer_size);
    char cmd_buf[1024];
    for (;;) {
        if(read_input(cmd_buf, 0) > 0)
        {
            process_commands(cmd_buf);
            memset(&cmd_buf,0,sizeof(cmd_buf));
        }
        play_buffer = (char *) (malloc(buffer_size));
        if (fill(play_buffer, buffer_size))
            break;
        rFrames = g_AudioHalParams.streamOutParams.streamOut->write(play_buffer , buffer_size);
        free(play_buffer);
    }

    EXIT:
    if(play_buffer)
     free(play_buffer);
    pthread_mutex_lock (&mutex);
    g_AudioHalParams.hardware->closeOutputStream(g_AudioHalParams.streamOutParams.streamOut);
    pthread_mutex_unlock (&mutex);

    return 0;
}

int fill_buffer(void *buf, unsigned size)
{
    if (size > avail)
    {
        fprintf(stderr,"Play: no more data\n");
        return -1;
    }
    memcpy(buf, next, size);
    next += size;
    avail -= size;
    return 0;
}

void play_file(int fd, unsigned count)
{
    unsigned num, total;

    num = count/BUFFER_LENGTH;
    num++;
    total = num*BUFFER_LENGTH;
    /*next is the pointer of file content buffer*/
    next = (char*) malloc(total);
    if (!next) {
        fprintf(stderr,"could not allocate %d bytes\n", total);
        return;
    }
    memset(next, 0, total);
    if (read(fd, next, count) != (signed) count) {
        fprintf(stderr,"could not read %d bytes\n", count);
        return;
    }
    avail = total;
    printf("Play: file size=%d\n", avail);
    pcm_play(fill_buffer);
}

// Basic Playback
int wav_play()
{
    struct wav_header hdr;
    unsigned rate;
    int fd;

    printf("Play: wav_play %s\n",playback_file);
    fd = open(playback_file, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr,"Play: cannot open '%s'\n", playback_file);
    return -1;
    }

    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
     fprintf(stderr,"Play: cannot read header\n");
        close(fd);
    return -1;
    }

    printf("Play: %d ch, %d hz, %d bit, %d format %s format_size %d\n",
            hdr.num_channels, hdr.sample_rate, hdr.bits_per_sample, hdr.audio_format,
            (hdr.audio_format == FORMAT_PCM) ? "PCM" : "unknown", hdr.fmt_sz);

    if ((hdr.riff_id != ID_RIFF) ||
        (hdr.riff_fmt != ID_WAVE) ||
        (hdr.fmt_id != ID_FMT)) {
        fprintf(stderr,"Play: '%s' is not a riff/wave file\n", playback_file);
        close(fd);
        return -1;
    }
    if ((hdr.audio_format != FORMAT_PCM) ||
        (hdr.fmt_sz != 16)) {
    fprintf(stderr, "Play: '%s' is not pcm format\n", playback_file);
        close(fd);
        return -1;
    }
    if (hdr.bits_per_sample != 16) {
        fprintf(stderr, "Play: '%s' is not 16bit per sample\n", playback_file);
        close(fd);
        return -1;
    }
    uint32_t channels = 0;
    //copy from ALSAStreamOps.cpp
    switch(hdr.num_channels) {
        case 4:
            channels |= AudioSystem::CHANNEL_OUT_BACK_LEFT;
            channels |= AudioSystem::CHANNEL_OUT_BACK_RIGHT;
            // Fall through...
        default:
        case 2:
            channels |= AudioSystem::CHANNEL_OUT_FRONT_RIGHT;
            // Fall through...
        case 1:
            channels |= AudioSystem::CHANNEL_OUT_FRONT_LEFT;
            break;
    }
   g_AudioHalParams.streamOutParams.channels = channels;
   g_AudioHalParams.streamOutParams.rate = hdr.sample_rate;

   play_file(fd, hdr.data_sz);

    close(fd);
    return 0;
}

void* playback_thread(void* arg)
{
   wav_play();
   pthread_exit(NULL);
   return NULL;
}

// duplex Test
int duplex()
{
    printf("Running duplex Test!\n");

    AudioHardwareALSA *hardware = new AudioHardwareALSA();
    hardware->setMasterVolume(m_volume);
    hardware->setMode(1);

    pthread_t thread_id;
    int ret;
    pthread_attr_t attr;

    /* Initialize and set thread detached attribute */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    ret =  pthread_create(&thread_id, &attr, playback_thread, NULL);
    if (ret){
        printf("ERROR: return code from pthread_create() is %d\n", ret);
        return 0;
    }
    sleep(1);
    wav_rec();
    /* Free attribute and wait for the other threads */
    pthread_attr_destroy(&attr);
    void* status ;
    ret = pthread_join(thread_id, &status);
    if (ret) {
        printf("ERROR; return code from pthread_join() is %d\n", ret);
        delete hardware;
        hardware = NULL;
        return 0;
    }

    delete hardware;
    hardware = NULL;
    return 0;
}



// Loopback Test
int loopback(int duration)
{
    printf("Running Loopback Test!\n");
    int16_t outbuffer [FRAME_COUNT];
    size_t count = 0;
    size_t fileSize;
    status_t status = 0;
    int format = 1;
    uint32_t device = 1, device_in = 262144, m_rate = 8000, m_out_channels = 1, m_in_channels = 1,mFlags = 0;

    AudioHardwareALSA *hardware = new AudioHardwareALSA();
    hardware->setMasterVolume(m_volume);
    hardware->setMode(1);

    AudioStreamOut *out = hardware->openOutputStream(device, &format, &m_out_channels, &m_rate, &status);
    AudioStreamIn *in = hardware->openInputStream(device_in, &format, &m_in_channels, &m_rate, &status, (AudioSystem::audio_in_acoustics) mFlags);

    while((int)count < duration){
        in->read(outbuffer, FRAME_COUNT*sizeof(int16_t));
    out->write(outbuffer, FRAME_COUNT*sizeof(int16_t));
    count++;
    }

    usleep(50000);
    delete hardware;
    hardware = NULL;
    return 0;
}

static void usage()
{
    printf(
("Usage: audiotest -t <testname> [OPTION]...<file>\n"
"\n"
"Play Wav   :    audiotest -t play [OPTION] <filename>\n"
"Record Wav :    audiotest -t rec [OPTION] <filename>\n"
"Loopback:    audiotest -t loopback [OPTION]\n"
"duplex  :    audiotest -t duplex [OPTION] <playback file> <recorded file to save> \n"
"[OPTION]  \n"
"-h, --help              help\n"
"-t, --test              play/rec/loopback/duplex\n"
"-D, --deviceE           select device (speaker - 0/headset - 1) -only playback case\n"
"-c, --channels=#        channels (1/2)- rec case\n"
"-f, --format=FORMAT     sample (8/16)-rec case\n"
"-r, --rate=#            sample rate(8000/16000) - rec case\n"
"-d, --duration=#        seconds - loopback case\n"
"-v, --volume=#          0-10 mute=0 - all cases\n")
    );
}




int main(int argc, char *argv[    ])
{

    char device_name[10];
    int play = 0, i;
    int tmp, test_type = TEST_UNDEFINED;
    int m_device = 0;
    int m_channels = 0;
    int m_rate = 0;
    int ch;
    int found_test = 0;
    printf("\n\n**********************************************\n");
    printf("********** AUDIO HAL TEST FRAMEWORK **********\n");
    printf("**********************************************\n\n");
    int option_index;
    static const char short_options[] = "t:D:c:f:r:d:v:h";
    static const struct option long_options[] = {
        {"help", 0, 0, 'h'},
        {"test", 1, 0, 't'},
        {"device", 1, 0, 'D'},
        {"channels", 1, 0, 'c'},
        {"format", 1, 0, 'f'},
        {"rate", 1, 0, 'r'},
        {"duration", 1, 0 ,'d'},
        {"volume", 1, 0 ,'v'},
        {0, 0, 0, 0}
    };

    if (argc < 2) {
        usage();
        return 0;

    }

    while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (ch) {
            case 't':
                found_test = 1;
                if (strcasecmp(optarg, "play") == 0)
                    test_type = TEST_PLAY;
                else if (strcasecmp(optarg, "rec") == 0)
                    test_type = TEST_REC;
                else if (strcasecmp(optarg, "loopback") == 0)
                    test_type = TEST_LOOPBACK;
                else if (strcasecmp(optarg, "duplex") == 0)
                    test_type = TEST_DUPLEX;
                else
                    test_type = TEST_UNDEFINED;
                break;
            default:
                printf("didn't specify testcase\n");
                usage();
                return 0;
        }
        if(found_test)
            break;
    }


    while ((ch = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
            switch (ch) {
            case 'h':
                usage();
                return 0;

            case 'D':
        tmp = strtol(optarg, NULL, 0);
                if(tmp == 0)
                    m_device = (AudioSystem::DEVICE_OUT_SPEAKER);
                else if (tmp == 1)
                    m_device = (AudioSystem::DEVICE_OUT_WIRED_HEADSET);
                break;

           case 'c':
                if (test_type == TEST_REC) {
                    tmp = strtol(optarg, NULL, 0);
                    if ((tmp != 1) && (tmp != 2)) {
                        printf("doesn't support channels-using default channels %d\n",m_channels);
                    } else {
                        if (tmp == 1)
                            m_channels = AudioSystem::CHANNEL_IN_LEFT;
                        else if (tmp == 2)
                            m_channels = AudioSystem::CHANNEL_IN_LEFT | AudioSystem::CHANNEL_IN_RIGHT;
                        //m_channels = tmp;
                    }
                } else
                    printf("currently not supported \n");
                break;

           case 'r':
                if (test_type == TEST_REC) {
                    tmp = strtol(optarg, NULL, 0);
                    if ((tmp != 8000) && (tmp != 16000)) {
                        printf("doesn't support rate-using default rate %d\n",m_rate);
                    } else
                        m_rate = tmp;
                } else
                    printf("currently not supported \n");
                break;
            case 'd':
        timelimit = strtol(optarg, NULL, 0);
        break;
            case 'v':
                tmp = strtol(optarg, NULL, 0);
                m_volume = tmp * 0.1;
                break;
            case 'f':
                printf("currently not supported \n");
                break;
            case ':':
            case '?':
                printf("missing param/unknown option\n");
                usage();
                return 0;
        }
    }


    audioHal_Initialize();

    switch(test_type) {
        case TEST_PLAY:
            strcpy(playback_file, argv[argc-1]);

            if(m_device)
                g_AudioHalParams.streamOutParams.device = m_device;

            printf("Play: start playback\n");
            wav_play();
            break;

        case TEST_REC:
            strcpy(rec_file, argv[argc-1]);

            if(m_device)
                g_AudioHalParams.streamInParams.device = m_device;
            if(m_channels)
                g_AudioHalParams.streamInParams.channels = m_channels;
            if(m_rate)
                g_AudioHalParams.streamInParams.rate = m_rate;

            printf("Rec: start recording to rec.wav\n");
            wav_rec();
            break;

        case TEST_LOOPBACK:
            loopback(timelimit);
            break;

        case TEST_DUPLEX:
            strcpy(playback_file, argv[argc -2]);
            strcpy(rec_file, argv[argc-1]);
            printf("duplex started\n");
            duplex();
            break;
        default:
            printf("Play: start playback out.wav\n");
            wav_play();
            break;
    }

   /*deleting hardware */
   delete g_AudioHalParams.hardware;
   g_AudioHalParams.hardware = NULL;

   return 0;
}

#endif

#ifndef 	AUDIO_TEST_CPP
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

#define ADDR_MIN 	0x1
#define ADDR_MAX 	0x49
#define DATA_MIN	0x0
#define DATA_MAX	0xff

#define BUFFER_LENGTH	4096
#define FRAME_COUNT 80

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1
//DEFAULTS
#define DEFAULT_REC_FORMAT AudioSystem::PCM_16_BIT
#define DEFAULT_REC_CHANNELS 1

#define DEFAULT_PLAY_RATE 48000
#define DEFAULT_REC_RATE 8000

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

using namespace android;

char buffer[BUFFER_LENGTH];

static char *next;
static unsigned avail;
static uint32_t m_channels = DEFAULT_REC_CHANNELS;
static uint32_t m_rate = DEFAULT_REC_RATE;
static float m_volume = 1; //default maximum
unsigned int buffer_size = 0;
unsigned int device = (AudioSystem::DEVICE_OUT_WIRED_HEADSET);

AudioHardwareALSA * hardware = NULL;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char playback_file[100] = {0};
char rec_file[100] = {0};

int wav_rec()
{
    struct wav_header hdr;
    int fd, format = AudioSystem::PCM_16_BIT;
    unsigned total = 0, size;
    unsigned char tmp;
    uint32_t mFlags = 0 ;
    static int count = 0;
    int set = 0;
    pthread_mutex_lock (&mutex);
    if (hardware == NULL) {
        set = 1;
        hardware = new AudioHardwareALSA();
    }
    pthread_mutex_unlock (&mutex);
    ssize_t rFrames;
    status_t status;
   //using default device for recording 
    unsigned int device = (AudioSystem::DEVICE_IN_BUILTIN_MIC);

    hdr.riff_id = ID_RIFF;
    hdr.riff_sz = 0;
    hdr.riff_fmt = ID_WAVE;
    hdr.fmt_id = ID_FMT;
    hdr.fmt_sz = 16;
    hdr.audio_format = FORMAT_PCM;
    hdr.num_channels = m_channels; 	//1;	//2;
    hdr.sample_rate = m_rate;	//8000;	//44100;
    hdr.byte_rate = hdr.sample_rate * hdr.num_channels * 2;
    hdr.block_align = hdr.num_channels * 2;
    hdr.bits_per_sample = 16;
    hdr.data_id = ID_DATA;
    hdr.data_sz = 0;

    pthread_mutex_lock (&mutex);
    fprintf(stderr,"opening the input stream device 0x%x channels %d rate %d\n",device, m_channels, m_rate);
    AudioStreamIn *in = hardware->openInputStream(device,
                                                  &format, 
                                                  &m_channels, 
                                                  &m_rate, 
                                                  &status, 
                                                  (AudioSystem::audio_in_acoustics) mFlags);
    if (in == NULL) {
        fprintf(stderr,"openInputStream failed for device 0x%x\n",device);
        if(set)
            delete hardware;
        pthread_mutex_unlock (&mutex);
        return -1;
    } else if (status != NO_ERROR){
        fprintf(stderr,"openInputStream return status %d\n",status);
        hardware->closeInputStream(in);
        if(set)
            delete hardware;
        pthread_mutex_unlock (&mutex);
        return -1;
    }
    hardware->setMasterVolume(m_volume);
    pthread_mutex_unlock (&mutex);
    size = (unsigned int)in->bufferSize();
    printf("Rec: set buffer size = %d\n", size);
    printf("Rec: opening file to record %s\n", rec_file);

    fd = open(rec_file, O_CREAT | O_RDWR, 777);
    if (fd < 0) {
 	fprintf(stderr,"Rec: cannot open output file\n");
        hardware->closeInputStream(in);
        delete hardware;
        return -1;
    }

    write(fd, &hdr, sizeof(hdr));

    fcntl(0, F_SETFL, O_NONBLOCK);
    printf("\nRec: *** RECORDING * HIT ENTER TO STOP ***\n");

    for (;;) {

        while (read(0, &tmp, 1) == 1) {
          if (tmp == 10)
          {
		printf("Rec: Recording is over.\n");	
		goto done;
	  }	
        }
        if(in != NULL)
        	rFrames = in->read(buffer, size);
        else
                printf("Something went wrong during the recording!\n");
        count++;

	if (write(fd, buffer, size) != (signed) size) {
            fprintf(stderr,"Rec: cannot write buffer\n");
            goto fail;
        }
        total += size;
        if(count == 100)
        {
		printf("******* Recording has completed *******\n");	
		goto done;
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
    hardware->closeInputStream(in);
    if (set)
        delete hardware;
    pthread_mutex_unlock (&mutex);
    return 0;

fail:
    printf("Rec: Record failed\n");
    close(fd);
    unlink(rec_file);
    pthread_mutex_lock (&mutex);
    hardware->closeInputStream(in);
    if(set)
        delete hardware;
    pthread_mutex_unlock (&mutex);
    return -1;
}

int pcm_play(unsigned rate, unsigned channels,
             int (*fill)(void *buf, unsigned size))
{
    FILE *infile;
    int set = 0;

    pthread_mutex_lock (&mutex);
    if (hardware == NULL){
        hardware = new AudioHardwareALSA();
        set = 1;
    }
    pthread_mutex_unlock (&mutex);
    ssize_t rFrames;
    status_t status = 0;
    int format = 0;
    char *play_buffer = NULL;

    printf("Using device %d\n", device);
    pthread_mutex_lock (&mutex);
    AudioStreamOut *out  = hardware->openOutputStream(device, &format, &channels, &rate, &status);
    pthread_mutex_unlock (&mutex);
    printf("Play: format %d m_channels %d m_rate %d status=%d\n", format, channels, rate, status);
    if(status != NO_ERROR) {
        printf("openOutputStream return failure \n");
        goto EXIT;
    }
    pthread_mutex_lock (&mutex);
    hardware->setMasterVolume(m_volume);
    pthread_mutex_unlock (&mutex);
    if(out != NULL) {
      buffer_size = (unsigned int)out->bufferSize();
    }
    else
    {
      printf("Output stream did not properly initialize!\n");
      goto EXIT;
    }
    printf("Play: buffer size=%d\n", buffer_size);
   // buffer_size = BUFFER_LENGTH;
   for (;;) {
        play_buffer = (char *) (malloc(buffer_size));
	if (fill(play_buffer, buffer_size))
	    break;

        if(out != NULL)
        {
	  rFrames = out->write(play_buffer , buffer_size);
        }
        else
        {
          printf("Something went wrong\n");
          goto EXIT;
        }
        free(play_buffer);
    }
    
    EXIT:
    if(play_buffer)
     free(play_buffer);
    pthread_mutex_lock (&mutex);
    hardware->closeOutputStream(out);
    if (set) {
        delete hardware;
    }
    pthread_mutex_unlock (&mutex);

//    free(next);
    return 0;
}

int fill_buffer(void *buf, unsigned size)
{
//  printf("fill_buffer: size=%d, avail=%d\n", size, avail);
    if (size > avail)
    {	
	fprintf(stderr,"Play: no more data\n");
        return -1;
    }
    memcpy(buf, next, size);
    next += size;
    avail -= size;
//  printf("Play: avail=%d\n", avail);
    return 0;
}

void play_file(unsigned rate, unsigned channels,
               int fd, unsigned count)
{
    unsigned num, total;	

    /*next is the pointer of file content buffer*/	
    num = count/BUFFER_LENGTH;
    num++;
    total = num*BUFFER_LENGTH;
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
    pcm_play(rate, channels, fill_buffer);
}

// Basic Playback
int wav_play()
{
    struct wav_header hdr;
    unsigned rate, channels;
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

    play_file(hdr.sample_rate, hdr.num_channels,
              fd, hdr.data_sz);
    
    close(fd);
    return 0;
}	

void* playback_thread(void* arg)
{
   hardware = (AudioHardwareALSA *)arg;
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

    ret =  pthread_create(&thread_id, &attr, playback_thread, (void*)hardware);
    if (ret){
        printf("ERROR: return code from pthread_create() is %d\n", ret);
        delete hardware;
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
        return 0;
    }

    delete hardware;
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




int main(int argc, char *argv[	])
{
    char device_name[10];
    int play = 0, i;
    int timelimit = 0;
    int tmp, test_type = TEST_UNDEFINED;
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
                    device = (AudioSystem::DEVICE_OUT_SPEAKER);
                else if (tmp == 1)
                    device = (AudioSystem::DEVICE_OUT_WIRED_HEADSET);
                break;

           case 'c':
                if (test_type == TEST_REC) {
                    tmp = strtol(optarg, NULL, 0);
                    if ((tmp != 1) && (tmp != 2)) {
                        printf("doesn't support channels-using default channels %d\n",m_channels);
                    } else
                        m_channels = tmp;
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

    if(test_type == TEST_DUPLEX) {
        strcpy(playback_file, argv[argc -2]);
        strcpy(rec_file, argv[argc-1]);
    } else if(test_type == TEST_PLAY) {
        strcpy(playback_file, argv[argc-1]);
    } else if(test_type == TEST_REC) {
        strcpy(rec_file, argv[argc-1]);
    }

    if(test_type == TEST_DUPLEX) {
        printf("duplex started\n");
        return duplex();
    } else if(test_type == TEST_LOOPBACK) {
        printf("loopback started\n");
        return loopback(timelimit);
    } else if(test_type == TEST_PLAY) {
        printf("Play: start playback\n");
        return wav_play();
    } else if(test_type == TEST_REC) {
        printf("Rec: start recording to rec.wav\n");
        return wav_rec();
    } else {
        printf("Play: start playback out.wav\n");
        return wav_play();
    }	
    return 0;
}
#endif

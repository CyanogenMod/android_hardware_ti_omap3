#ifndef 	AUDIO_TEST_CPP
#define AUDIO_TEST_CPP
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <string.h>
#include <utils/Log.h>
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
static uint32_t m_channels;
static uint32_t m_rate;
unsigned int buffer_size = 0;
unsigned int device_number = 0;

int wav_rec(const char *fn)
{
    struct wav_header hdr;
    int fd, format = 2;
    unsigned total = 0, size;
    unsigned char tmp;
    uint32_t mFlags = 0, aj = 262144;
    static int count = 0;
    FILE *infile;
    AudioHardwareALSA * hardware = new AudioHardwareALSA();
    ssize_t rFrames;
    status_t status;
    

    hdr.riff_id = ID_RIFF;
    hdr.riff_sz = 0;
    hdr.riff_fmt = ID_WAVE;
    hdr.fmt_id = ID_FMT;
    hdr.fmt_sz = 16;
    hdr.audio_format = FORMAT_PCM;
    hdr.num_channels = 1; 	//1;	//2;
    hdr.sample_rate = 8000;	//8000;	//44100;
    hdr.byte_rate = hdr.sample_rate * hdr.num_channels * 2;
    hdr.block_align = hdr.num_channels * 2;
    hdr.bits_per_sample = 16;
    hdr.data_id = ID_DATA;
    hdr.data_sz = 0;

    m_channels = hdr.num_channels;
    m_rate = hdr.sample_rate;

    AudioStreamIn *in = hardware->openInputStream(aj, 
                                                  &format, 
                                                  &m_channels, 
                                                  &m_rate, 
                                                  &status, 
                                                  (AudioSystem::audio_in_acoustics) mFlags);
    if (in == NULL) {
        fprintf(stderr,"openInputStream failed\n");
        delete hardware;
        return -1;
    }
    hardware->setMasterVolume(1.0f);
    size = (unsigned int)in->bufferSize();
    printf("Rec: set buffer size = %d\n", size);

    fd = open(fn, O_CREAT | O_RDWR, 777);
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
          if (tmp == 13) 
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

	if (write(fd, buffer, size) != size) {
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
    hardware->closeInputStream(in);
    delete hardware;
    return 0;

fail:
    printf("Rec: Record failed\n");
    close(fd);
    unlink(fn);
    hardware->closeInputStream(in);
    delete hardware;
    return -1;
}

int pcm_play(unsigned rate, unsigned channels,
             int (*fill)(void *buf, unsigned size))
{
    FILE *infile;
    AudioHardwareALSA * hardware = new AudioHardwareALSA();
    ssize_t rFrames;
    status_t status = 0;
    int format = 2;
    uint32_t device = 2;
    char *play_buffer = NULL;
//  AudioStreamOut *out  = hardware->openOutputStream(AudioSystem::PCM_16_BIT, 0, 0, &status);
//  AudioStreamOut *out  = hardware->openOutputStream(AudioSystem::PCM_16_BIT, 1, 8000, &status);

    switch(device_number)
        {
          case 0:
            device = 2;
            break;
          case 1:
            device = 8194;
            break;
          case 2:
            device = 16386;
            break;
          case 3:
            break;
          case 4:
            break;
          default: 
            printf("Playback is using the  MM device by default!");
            break;
        }
    printf("Using device %d\n", device);
    AudioStreamOut *out  = hardware->openOutputStream(device, &format, &m_channels, &m_rate, &status);
    printf("Play: status=%d\n", status);
    hardware->setMasterVolume(1.0f);
    if(out != NULL)
      buffer_size = (unsigned int)out->bufferSize();
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
    hardware->closeOutputStream(out);
    delete hardware;

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
    if (read(fd, next, count) != count) {
        fprintf(stderr,"could not read %d bytes\n", count);
        return;
    }
    avail = total;
    printf("Play: file size=%d\n", avail);
    pcm_play(rate, channels, fill_buffer);
}

// Basic Playback
int wav_play(char *fn)
{
    struct wav_header hdr;
    unsigned rate, channels;
    int fd;

    printf("Play: wav_play\n");
    fd = open(fn, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr,"Play: cannot open '%s'\n", fn);
	return -1;
    }
    printf("Play: open file '%s'\n", fn);

    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
 	fprintf(stderr,"Play: cannot read header\n");
        close(fd);
	return -1;
    }

    m_channels = hdr.num_channels;
    m_rate= hdr.sample_rate;

    printf("Play: %d ch, %d hz, %d bit, %s\n",
            hdr.num_channels, hdr.sample_rate, hdr.bits_per_sample,
            hdr.audio_format == FORMAT_PCM ? "PCM" : "unknown");
    
    if ((hdr.riff_id != ID_RIFF) ||
        (hdr.riff_fmt != ID_WAVE) ||
        (hdr.fmt_id != ID_FMT)) {
    	fprintf(stderr,"Play: '%s' is not a riff/wave file\n", fn);
        close(fd);
        return -1;
    }
    if ((hdr.audio_format != FORMAT_PCM) ||
        (hdr.fmt_sz != 16)) {
	fprintf(stderr, "Play: '%s' is not pcm format\n", fn);
        close(fd);
        return -1;
    }
    if (hdr.bits_per_sample != 16) {
	fprintf(stderr, "Play: '%s' is not 16bit per sample\n", fn);
        close(fd);
        return -1;
    }

    play_file(hdr.sample_rate, hdr.num_channels,
              fd, hdr.data_sz);
    
    close(fd);
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
    hardware->setMasterVolume(1.0f);
    hardware->setMode(1);
    
    AudioStreamOut *out = hardware->openOutputStream(device, &format, &m_out_channels, &m_rate, &status);
    AudioStreamIn *in = hardware->openInputStream(device_in, &format, &m_in_channels, &m_rate, &status, (AudioSystem::audio_in_acoustics) mFlags);

    while(count < duration){
        in->read(outbuffer, FRAME_COUNT*sizeof(int16_t));
	out->write(outbuffer, FRAME_COUNT*sizeof(int16_t));
	count++;        
    }

    usleep(50000);
    delete hardware;
    return 0;
}

int main(int argc, char *argv[	])
{
    char fn[100], device_name[10];
    int play = 0, i;
    int loopbackDuration = 0;

    printf("\n\n**********************************************\n");
    printf("********** AUDIO HAL TEST FRAMEWORK **********\n");
    printf("**********************************************\n\n");

    memset(fn, 0, 100);
    memset(device_name, 0, 10);
    for (i = 1; i < argc; i++)
    {
      if (!strcmp(argv[i],"-rec")) {
            play = 1;
        } else if (!strcmp(argv[i],"-play")) {
            play = 2;
        } else if (!strcmp(argv[i], "-loopback")) {
	    play = 3;
            loopbackDuration = atoi(argv[i+1]); 
	} else if (!strcmp(argv[i], "-usage")) {
	    printf("Play Wav File    :    audiotest -play <file_name>\n");
	    printf("Record Wav File  :    audiotest -rec <file_name>\n");
            printf("Loopback Test    :    audiotest -loopback <duration in seconds>\n");
            printf("Usage            :    audiotest -usage\n");
	    exit(0);
        }
     
      if(i == 2)
      {
        strcpy(fn, argv[i]);
      }
      if(i == 3)
      {
        strcpy(device_name, argv[i]);
        if (!strcmp(argv[i],"MM")) {
           device_number = 0;
        } else if (!strcmp(argv[i],"TONES")) {
            device_number = 1;
        } else if (!strcmp(argv[i],"VOICE")) {
            device_number = 2;
        }
      }  
    }
   
    if (play == 3) {
        return loopback(loopbackDuration); 
    } else if (play == 2) {
      printf("Play: start playback\n");
	return wav_play(fn);
    } else if (play == 1) {
        printf("Rec: start recording to rec.wav\n");
	return wav_rec(fn);
    } else {
	printf("Play: start playback out.wav\n");
	return wav_play(fn);
    }	
    return 0;
}
#endif

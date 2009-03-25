#include "SkImageEncoder_libtijpeg.h"



int main(int argc, char **argv)
{
    char inFilename[50]; 
    char *inputfilename = "/face_176x144_32bit.raw";
    char outFilename[50];
    char *outputfilename= "/face_32bit.jpg";
    int inputformat = 6;
    int w = 176;
    int h= 144;
    int quality = 100;
    int inbufferlen;
    int outbufferlen;
    SkTIJPEGImageEncoder JpegEnc;

    if (argc != 7){
        printf("\n\nUsage: SkImageEncoderTest <input filename> <output filename> <input format 0 - yuv, 4-16bit, 6-32bit> <width> <height> <quality>\n\n");
        strcpy(inFilename, inputfilename);
        strcpy(outFilename, outputfilename);
        printf("\nUsing Default Parameters");
        printf("\nInput Filename = %s", inFilename);
        printf("\nOutput Filename = %s", outFilename);
        printf("\nInput Format = %d", inputformat);		
        printf("\nw = %d", w);
        printf("\nh = %d", h);
        printf("\nquality = %d", quality);
    }
    else{
        strcpy(inFilename, argv[1]);
        strcpy(outFilename, argv[2]);
        inputformat = atoi(argv[3]);
        w = atoi(argv[4]);
        h = atoi(argv[5]);
        quality = atoi(argv[6]);
    }		
    
    FILE* fIn = NULL;    
    fIn = fopen(inFilename, "r");
    if ( fIn == NULL ) {
        printf("\nError: failed to open the file %s for reading", inFilename);
        return 0;
    }

    printf("\nOpened File %s\n", inFilename);

    if (inputformat == 0)
        inbufferlen = w * h * 2;
    else if (inputformat == 4)
        inbufferlen = w * h * 2;
    else if (inputformat == 6)
        inbufferlen = w * h * 4;
	
    void *inBuffer = malloc(inbufferlen + 256);
    inBuffer = (void *)((int)inBuffer + 128);
    fread((void *)inBuffer,  1, inbufferlen, fIn);
    if ( fIn != NULL ) fclose(fIn);

    outbufferlen =  (w * h) + 12288;
    void *outBuffer = malloc(outbufferlen + 256);
    outBuffer = (void *)((int)outBuffer + 128);


    printf("\n\n before calling encodeImage \n\n");
    
    if (JpegEnc.encodeImage(outBuffer, outbufferlen, inBuffer, inbufferlen, w, h, quality, (SkBitmap::Config)inputformat )){

    printf("\n\n after calling encodeImage - success \n\n");

        FILE* f = NULL;    
        f = fopen(outFilename, "w");
        if ( f == NULL ) {
            printf("\nError: failed to open the file %s for writing", outFilename);
            return 0;
        }

        fwrite(outBuffer,  1, outbufferlen, f);
        if ( f != NULL ) fclose(f);

        printf("Test Successful\n");
    }
    else printf("Test UnSuccessful\n");
    
    free(inBuffer);
    free(outBuffer);

    return 0;
}


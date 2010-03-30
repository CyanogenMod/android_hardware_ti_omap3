#include "SkImageEncoder_libtijpeg.h"

#define MULTIPLE 2 //image width must be a multiple of this number
#define ALIGN_128_BYTE 128

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
    int outbufferlen;
    SkTIJPEGImageEncoder JpegEnc;
    int nWidthNew, nHeightNew, nMultFactor, nBytesPerPixel, inBuffSize, nBytesToPad;

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
        printf("\nquality = %d\n", quality);
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

    nBytesPerPixel = 2;
    if (inputformat == 0)
        nBytesPerPixel = 2;
    else if (inputformat == 4)
        nBytesPerPixel = 2;
    else if (inputformat == 6)
        nBytesPerPixel = 4;

    void *tempBuffer = malloc(w*h*nBytesPerPixel);
    fread(tempBuffer,  1, (w*h*nBytesPerPixel), fIn);
    if ( fIn != NULL ) fclose(fIn);
    
    nMultFactor = (w + 16 - 1)/16;
    nWidthNew = (int)(nMultFactor) * 16;

    nMultFactor = (h + 16 - 1)/16;
    nHeightNew = (int)(nMultFactor) * 16;

    inBuffSize = nWidthNew * nHeightNew * nBytesPerPixel;
    if (inBuffSize < 1600)
        inBuffSize = 1600;
	
    inBuffSize = (OMX_U32)((inBuffSize + ALIGN_128_BYTE - 1) & ~(ALIGN_128_BYTE - 1));
    void *inputBuffer = memalign(ALIGN_128_BYTE, inBuffSize);
    if (inputBuffer == NULL) {
        printf("\n %s():%d::ERROR:: inputBuffer memory allocation failed. \n",__FUNCTION__,__LINE__);
        return 0;
    }

    int pad_width = w%MULTIPLE;
    int pad_height = h%2;
    int pixels_to_pad = 0;
    if (pad_width)
        pixels_to_pad = MULTIPLE - pad_width;
    int bytes_to_pad = pixels_to_pad * nBytesPerPixel;
    printf("\npixels_to_pad  = %d\n", pixels_to_pad );
    int src = (int)inputBuffer;
    int dst = (int)tempBuffer;
    int i, j;
    int row = w * nBytesPerPixel;
    
    if (pad_height && pad_width) 
    {
        printf("\ndealing with pad_width");
        for(i = 0; i < h; i++)
        {
            memcpy((void *)src, (void *)dst, row);
            dst = dst + row;
            src = src + row;
            for (j = 0; j < bytes_to_pad; j++){
                *((char *)src) = 0;
                src++;
            }
        }
        
        printf("\ndealing with odd height");
        memset((void *)src, 0, row+bytes_to_pad); 

        w+=pixels_to_pad;
        h++;
    }
    else if (pad_height) 
    {
        memcpy((void *)src, (void*)dst, (w*h*nBytesPerPixel));
        printf("\nadding one line and making it zero");
        memset((void *)(src + (w*h*nBytesPerPixel)), 0, row); 
        h++;
    }
    else if (pad_width)
    {
        printf("\ndealing with odd width");
    
        for(i = 0; i < h; i++)
        {
            memcpy((void *)src, (void *)dst, row);
            dst = dst + row;
            src = src + row;
            for (j = 0; j < bytes_to_pad; j++){
                *((char *)src) = 0;
                src++;
            }
        }
        w+=pixels_to_pad;
    }
    else 
    {
        printf("\nno padding");
        memcpy((void *)src, (void*)dst, (w*h*1.5));
    }

    outbufferlen =  (w * h) + 12288;
    outbufferlen = (OMX_U32)((outbufferlen + ALIGN_128_BYTE - 1) & ~(ALIGN_128_BYTE - 1));
    void *outBuffer = memalign(ALIGN_128_BYTE, outbufferlen);
    if (inputBuffer == NULL) {
        printf("\n %s():%d::ERROR:: outputBuffer memory allocation failed. \n",__FUNCTION__,__LINE__);
        return 0;
    }

    printf("\n\n before calling encodeImage \n\n");
    
    if (JpegEnc.encodeImage(outBuffer, outbufferlen, inputBuffer, inBuffSize, w, h, quality, (SkBitmap::Config)inputformat )){

    printf("\n\n after calling encodeImage - success \n\n");

        FILE* f = NULL;    
        f = fopen(outFilename, "w");
        if ( f == NULL ) {
            printf("\nError: failed to open the file %s for writing", outFilename);
            return 0;
        }

        fwrite(outBuffer,  1, JpegEnc.jpegSize, f);
        if ( f != NULL ) fclose(f);

        printf("Test Successful\n");
    }
    else printf("Test UnSuccessful\n");
    
    free(inputBuffer);
    free(outBuffer);
    free(tempBuffer);

    return 0;
}



#include "SkBitmap.h"
#include "SkStream.h"
#include "SkImageDecoder_libtijpeg.h"

void DecodeMultipleImages(void);

int main(int argc, char **argv)
{
    SkImageDecoder *jpegDec;
    SkBitmap    bm;
    SkBitmap::Config prefConfig = SkBitmap::kARGB_8888_Config;
    int size = 1;
    
    if (argc < 2)
    {
        printf("\n\n\tUsage:\n\n\t ./system/bin/SkImageDecoderTest <mode: 1- multiple images> <input filename> <output filename> <output format 4-16bit, 6-32bit> <Scale Factor: 1, 2, 3, 4>\n\n");
        return 0;
    }

    if (atoi(argv[1] ) == 1)
    {
        DecodeMultipleImages();
        return 0;
    }
        
    SkFILEStream   inStream(argv[2]);
    SkFILEWStream   outStream(argv[3]);
    if (argc >= 5)
        prefConfig = (SkBitmap::Config)atoi(argv[4]);
      
    if (argc == 6)
        size = atoi(argv[5]);

    jpegDec = SkImageDecoder::Factory(&inStream);
    jpegDec->setSampleSize(size);
    printf("\nSet Scale Factor in Application to %d\n", jpegDec->getSampleSize());    
    printf("\n\nRunning Jpeg Decoder Test\n\n");
    if (jpegDec->DecodeStream(&inStream, &bm, prefConfig, SkImageDecoder::kDecodePixels_Mode) == false)
    {
        printf("\n\nTest Failed\n\n");
        return 0;
    }
    
    outStream.write(bm.getPixels(), bm.getSize());
    printf("\nWrote %d bytes to output file.\n", bm.getSize());    
    printf("\n\nTest Successful !\n\n");
    
    return 0;
}


void DecodeMultipleImages(void)
{
    #define MAX_IMAGES 3

    int i;
    SkImageDecoder *jpegDec;
    SkBitmap    bm[MAX_IMAGES];

    char JDI_filenames[MAX_IMAGES][50] = {
        "/temp/J100_176x144_1.jpg",
        "/temp/J100_176x144_2.jpg",
        "/temp/J100_176x144_3.jpg",
    };
    
    char JDO_filenames[MAX_IMAGES][50] = {
        "/temp/J100_176x144_1.raw",
        "/temp/J100_176x144_2.raw",
        "/temp/J100_176x144_3.raw",
    };
    
        
    SkFILEStream   inStream(JDI_filenames[0]);
    SkFILEWStream *pOutStream;

    jpegDec = SkImageDecoder::Factory(&inStream);

    printf("\n\nLOOPING !!!\n");
    
    for(i = 0; i < MAX_IMAGES; i++){
        
        if (jpegDec->DecodeStream(&inStream, &(bm[i]), SkBitmap::kARGB_8888_Config, SkImageDecoder::kDecodePixels_Mode) == false)
        {
            printf("\n\nImage %d Failed\n\n", i);
        }

        pOutStream = new SkFILEWStream(JDO_filenames[i]);
        pOutStream->write(bm[i].getPixels(), bm[i].getSize());
        free(pOutStream);
        
        printf("\n\nImage %d Successful !\n\n", i);

        inStream.setPath(JDI_filenames[i+1]);
    }

}









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
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */
/**
* @file SkLibTiJpeg_Test.cpp
*
* This test program will be used to test the JPEG decoder and encoder
* from libskia level.
*
*/

#include <unistd.h>
#include "SkLibTiJpeg_Test.h"
#include "SkTime.h"

#ifdef ANDROID
#include <cutils/properties.h>
#endif

extern "C" {
#include "md5.h"
};

#define LIBSKIAHWTEST "[SkLibTiJpeg_Test_Script]"
#define PASS 0
#define FAIL 1
#define ERROR -1

#define PRINT printf

//enable this for additional debug prints.
//#define DEBUG 1

#define TIME_MEASUREMENT 1
#ifdef DEBUG
#define DBGPRINT printf
#else
#define DBGPRINT
#endif

#define BUFSIZE              (1024 * 8)
#define MD5_SUM_LENGTH       16
#define JPGD_MD5SUM_LIST     0
#define JPGE_MD5SUM_LIST     1

#define PASSCOUNT_ARM        0
#define PASSCOUNT_TIDSP      1
#define PASSCOUNT_SIMCOP     2
#define FAILCOUNT            3
#define COUNT_MANUALVERIFY   4
#define COUNT_MAX_INDEX      5

#define CODECTYPE_SW         0
#define CODECTYPE_DSP        1
#define CODECTYPE_SIMCOP     2

//test case count
unsigned int nTestCount[5]; //[0]-ARM; [1]-TI;
                            //[2]-SIMCOP; [3]-Fail count;
                            //[4]-manual verification needed
int flagDumpMd5Sum;
int flagCodecType;          //0- SW codec ( ARM)
                            //1- TI DSP codec
                            //2- SIMCOP codec

FILE* pFileDump;            //for dumping the md5sum strings
char testID[40];

//------------------------------------------------------------------------------------
#ifdef TIME_MEASUREMENT
class AutoTimeMillis {
public:
    AutoTimeMillis(const char label[]) : fLabel(label) {
        if (!fLabel) {
            fLabel = "";
        }
        fNow = SkTime::GetMSecs();
    }

    ~AutoTimeMillis() {
        PRINT("---- Input file Resolution :%dx%d",width,height);
        PRINT("---- JPEG Time (ms): %s %d\n", fLabel, SkTime::GetMSecs() - fNow);
    }

    void setResolution(int width, int height){
        this->width=width;
        this->height=height;
    }

private:
    const char* fLabel;
    SkMSec      fNow;
    int width;
    int height;
};
#endif

//-----------------------------------------------------------------------------
void printDecoderTestUsage() {

    PRINT("\nDecoder Test parameters:\n");
    PRINT("SkLibTiJpeg_Test <D> <Input_File_Name> <Output_File_Name> <nOutformat> <SF %s> <nSections> <nXOrigin> <nYOrigin> <nXLength> <nYLength> <SF value/UserW> <UserH>\n\n","%");
    PRINT("<D>               = D-Decoder test\n");
    PRINT("<Input_File_Name> = .jpg file name\n");
    PRINT("<Output_File_Name>= .raw file name\n");
    PRINT("<nOutformat>      = Output color format: 4-16bit_RBG565, 6-32bit_ARGB8888\n");
    PRINT("<SF %s>            = Scalefactor: 100, 50, 25, 12 percent\n","%");
    PRINT("<nSections>       = SliceOutput Mode: Number of rows per slice\n");
    PRINT("<nXOrigin>        = Sub Region Decode: X-origin\n");
    PRINT("<nYOrigin>        = Sub Region Decode: Y-origin\n");
    PRINT("<nXLength>        = Sub Region Decode: X-Length\n");
    PRINT("<nYLength>        = Sub Region Decode: Y-Length\n");
    PRINT("<SFvalue | nUserW>= Scale Factor number(OMAP4->1-256; OMAP3->1,2,4,8) OR User's desired output Width\n");
    PRINT("<nUserH >         = User's desired output Height.\n");

    PRINT("\nNOTE: nUserW * nUserH : if nUserW alone given it will be taken as scaleFactor value.\n");
    PRINT("     if both values are given then the scale factor is calculated based on valid values of them\n");
    PRINT("     if both value are valid then the scale factor will be calculated using nUserW\n");
    PRINT("     if nUserW = 0 and nUserH > 0 then the scale factor will be calculated using nUserH\n");

} //End of printDecoderTestUsage()

//-----------------------------------------------------------------------------
void printEncoderTestUsage() {

    PRINT("\nEncoder Test parameters:\n");
    PRINT("SkLibTiJpeg_Test <E> <Input_File_Name> <Output_File_Name> <nInformat> <nQFactor 1-100>\n\n");
    PRINT("<E>               = E-Encoder test\n");
    PRINT("<Input_File_Name> = .raw file name\n");
    PRINT("<Output_File_Name>= .jpg file name\n");
    PRINT("<nInformat>       = Input color format: 4-16bit_RBG565, 6-32bit_ARGB8888\n");
    PRINT("<nQFactor>        = Quality factor: 1-100\n");
} //End of printEncoderTestUsage()

//-----------------------------------------------------------------------------
void printInputScriptFormat() {

    PRINT("\nUsing Script/Folder:\n");
    PRINT("SkLibTiJpeg_Test <<S|F>[M][C]> <script file name | Folder Path> [Repeat Count]\n");
    PRINT("\n[S|F] - S-> scriptfile; F-> Folder path; **NOTE: 'F'is only for Decoder Test\n");
    PRINT("[M]   - Do Memory leak test\n");
    PRINT("[C]   - Dump the md5sum values in to a file <md5sum_dump.txt>.\n");
    PRINT("[Repeat Count] - Repeat the script/folder contents this many times.\n");
    PRINT("\nThe script file should contain <%s> as the first line.\n",LIBSKIAHWTEST);
    PRINT("Example format of the script file is:\n");

    PRINT("\n|------------------------------------------------------------------------------|\n");
    PRINT("[SkLibTiJpeg_Test_Script]\n");
    PRINT("#Commented line or commented testcase starts with '#' character.\n");
    PRINT("#TestcaseID SkLibTiJpeg_Test <E | D> <Input File> <Output File> <other params......> \n");
    PRINT("DECTestID-1: SkLibTiJpeg_Test D I_000265_12MP_4000x3000.jpg output/12MP-1.raw 4 100 \n");
    PRINT("#TestID-2: SkLibTiJpeg_Test D akiy4big07.jpg output/akiy4big07-2.raw 4 100 \n");
    PRINT("ENCTestID-1: SkLibTiJpeg_Test E akiy4big07-2.raw enctest-1.jpg 1200 1600 4 100 \n");
    PRINT("\n|------------------------------------------------------------------------------|\n");
}

//-----------------------------------------------------------------------------
void printUsage() {

   PRINT("\n\nUSAGE: \n\n");
   PRINT("Encoder Test: ./SkLibTiJpeg_Test <E> < parameters..... > \n");
   PRINT("Decoder Test: ./SkLibTiJpeg_Test <D> < parameters..... > \n");
   PRINT("Using Script: ./SkLibTiJpeg_Test <S[M][C]> <script file name> [Repeat Count]\n");
   PRINT("Folder Decode: ./SkLibTiJpeg_Test <F[M][C]> <Folder Path> [Repeat Count]\n");

   PRINT("\n|------------------------------------------------------------------------------|\n");
   printEncoderTestUsage();
   PRINT("\n|------------------------------------------------------------------------------|\n");
   printDecoderTestUsage();
   PRINT("\n|------------------------------------------------------------------------------|\n");
   printInputScriptFormat();
   PRINT("\n|------------------------------------------------------------------------------|\n");

} //End of printUsage()

//-----------------------------------------------------------------------------
//Currently this function will parse the header and give only image width and Height.
//can be added to parse for other information also.
U32 jpegHeaderParser(SkStream* stream, JPEG_IMAGE_INFO* JpegImageInfo) {

    U8 cByte;
    U8 flagDone = 0;
    U32 marker;
    size_t segLength;
    S32 lSize = 0;
    lSize = stream->getLength();
    stream->rewind();
    JpegImageInfo->nWidth = 0;
    JpegImageInfo->nHeight = 0;

    //check for the JPEG marker
    cByte = stream->readU8();
    if ( cByte != 0xff || stream->readU8() != M_SOI )  {
        return 0;
    }

    while(1) {
        for(;;) {
            cByte = stream->readU8();
            //PRINT("%02x-",cByte);
            if( cByte == 0xff ) break;
        } //end of for (;;)

        marker = stream->readU8();

        switch ( marker) {

            case M_SOS:
            case M_EOI:
                return 0;
            break;

            case M_SOF2:
                //PRINTF("nProgressive IMAGE!\n");
                //JpegImageInfo->nProgressive=1;
            case M_SOF0:
            case M_SOF1:
            case M_SOF3:
            case M_SOF5:
            case M_SOF6:
            case M_SOF7:
            case M_SOF9:
            case M_SOF10:
            case M_SOF11:
            case M_SOF13:
            case M_SOF14:
            case M_SOF15:
                //skip 2bytes length and one byte bits/sample.
                stream->skip(3);
                //read Height and width
                JpegImageInfo->nHeight = (int)( (stream->readU8() << 8) | (stream->readU8()) );
                JpegImageInfo->nWidth = (int)( (stream->readU8() << 8) | (stream->readU8()) );
                flagDone = 1;
            break;

            default:
                //read the length of the marker segment and skip that segment data
                segLength = (int)( (stream->readU8() << 8) || (stream->readU8()) );
                segLength = stream->skip(segLength-2);  //segLength include these 2 bytes for length value.
                if (segLength == 0){
                    return 0;
                }
            break;
        } //end of Switch

        if(flagDone)
            return lSize;

    } //end of while(1)
} //End of jpegHeaderParser()

//-----------------------------------------------------------------------------
U32 getMeminfo(FILE* pFile, const char* pStr) {

    U32 value = 0;
    int i;
    char strLine[80];

    //search the line which contains the required mem info.
    while(!feof(pFile) ) {
        if( NULL == fgets(strLine, sizeof(strLine), pFile)) {
            break;
        }
        if ( NULL == strstr(strLine, pStr) ) {
            continue;
        }
        else {
            sscanf(strLine+strlen(pStr), "%ld", &value);
            break;
        }
    }
    rewind(pFile);
    return value;
}
//-----------------------------------------------------------------------------
void analyzeMemoryInfo(const char* firstFile, const char* secondFile) {
    U32 freeMemBeforeTest = 0;
    U32 freeMemAfterTest = 0;
    U32 cachedMemBeforeTest = 0;
    U32 cachedMemAfterTest = 0;
    FILE* pFile1 = NULL;

    PRINT("\n\n>>>>>>>>>>>>>>>>>>>>>>>>>>> ANALYZING MEMORY LEAK <<<<<<<<<<<<<<<<<<<<<<<<<<< \n\n");
    pFile1 = fopen(firstFile, "r");
    if (pFile1 == NULL) {
        PRINT("%s():%d::!!!! Could not open file %s\n",__FUNCTION__,__LINE__,firstFile);
        goto EXIT;
    }

    //get the memory info before test.
    freeMemBeforeTest =  getMeminfo(pFile1, "MemFree:");
    cachedMemBeforeTest = getMeminfo(pFile1, "Cached:");
    fclose(pFile1);

    pFile1 = fopen(secondFile, "r");
    if (pFile1 == NULL) {
        PRINT("%s():%d::!!!! Could not open file %s\n",__FUNCTION__,__LINE__,secondFile);
        goto EXIT;
    }

    //get the memory info after test
    freeMemAfterTest =  getMeminfo(pFile1, "MemFree:");
    cachedMemAfterTest = getMeminfo(pFile1, "Cached:");
    fclose(pFile1);

    PRINT("MemFree: Before::After =>  %ld KB :: %ld KB\n",freeMemBeforeTest, freeMemAfterTest);
    PRINT("Cached:  Before::After =>  %ld KB :: %ld KB\n\n",cachedMemBeforeTest, cachedMemAfterTest);

    if( ((freeMemBeforeTest+cachedMemBeforeTest) - (freeMemAfterTest-cachedMemAfterTest)) > (5*1024*1024)) {
        PRINT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        PRINT("!!!!!! Looks like there is a MEMORY LEAK during the Test Execution!!!!!!\n");
        PRINT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
    else {
        PRINT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        PRINT("!!!!!!!!!!!!!!!!!!!!!!!! MEMORY TEST OK !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        PRINT("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    }
    return;

EXIT:
    PRINT("!!!!!!!!!!!!!!!!!!!!! MEMORY TEST not Successful!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    return;
} //End of analyzeMemoryInfo()

//-----------------------------------------------------------------------------
void extractFileName(char* InFileName, char* imgFileName) {

   char ch = '/';
   char* pTmpName = NULL;

   pTmpName = strrchr( InFileName, (int)ch );
   if (pTmpName == NULL) {
       strcpy ( imgFileName, (const char*)InFileName );
   }
   else {
       strcpy ( imgFileName, (const char*)pTmpName+1 );
   }
} //End of extractFileName()

//-----------------------------------------------------------------------------
void md5SumFile(unsigned char *md, char* pFileName) {

    MD5_CTX md5_c;
    int fd;
    int i;
    static unsigned char buf[BUFSIZE];
    FILE* pFile = NULL;

    pFile = fopen(pFileName, "r");
    if ( pFile == NULL ) {
        PRINT("\nERROR!!!! Not able to open the file: %s\n", pFileName);
        return;
    }

    MD5_Init(&md5_c);
    for (;;) {
        i = fread( (void*)buf, 1, BUFSIZE, pFile );
        if (i <= 0) break;
        MD5_Update( &md5_c, buf, (unsigned long)i );
    }
    MD5_Final( md, &md5_c );

    fclose(pFile);

    DBGPRINT("%s:%d::MD5SUM = ",__FUNCTION__,__LINE__);
    for (i = 0; i < MD5_SUM_LENGTH; i++)
        DBGPRINT("%02x",md[i]);
    DBGPRINT("\n");

} //End of md5SumFile()

//-----------------------------------------------------------------------------
void md5SumBuf(unsigned char* md, void* pBuf, unsigned long nBufSize) {

    MD5_CTX md5_c;
    int i;

    MD5_Init(&md5_c);
    MD5_Update(&md5_c, pBuf, nBufSize);
    MD5_Final( md, &md5_c );

    DBGPRINT("%s:%d::MD5SUM = ",__FUNCTION__,__LINE__);
    for (i = 0; i < MD5_SUM_LENGTH; i++)
        DBGPRINT("%02x",md[i]);
    DBGPRINT("\n");

} //End of md5SumBuf()

//-----------------------------------------------------------------------------
void updateTestCount(int id) {

    android::countMutex.lock();
    if ( id < COUNT_MAX_INDEX ) {
        nTestCount[id] += 1;
    }
    android::countMutex.unlock();

}

//-----------------------------------------------------------------------------
void dumpMd5Sum(char* imgFileName, char* mdString, void* pList) {
    const struct JPGD_TEST_OUTPUT_FILE_LIST *pDBList = (const struct JPGD_TEST_OUTPUT_FILE_LIST *)pList;

    //dump the file name and its md5sum string in to a file
    fprintf(pFileDump, "{ // %s\n",testID);
    fprintf(pFileDump, "\"%s\",\n",imgFileName);

    switch(flagCodecType) {

        case CODECTYPE_SW:
            if(pDBList){
                fprintf(pFileDump, "\"%s\",\n",mdString);
                fprintf(pFileDump, "\"%s\",\n",pDBList->md5CheckSumTi);
                fprintf(pFileDump, "\"%s\"\n",pDBList->md5CheckSumSimcop);
            }
            else {
                fprintf(pFileDump, "\"%s\",\n",mdString);
                fprintf(pFileDump, "\"0\",\n");
                fprintf(pFileDump, "\"0\"\n");
            }
        break;

        case CODECTYPE_DSP:
            if(pDBList){
                fprintf(pFileDump, "\"%s\",\n",pDBList->md5CheckSumArm);
                fprintf(pFileDump, "\"%s\",\n",mdString);
                fprintf(pFileDump, "\"%s\"\n",pDBList->md5CheckSumSimcop);
            }
            else {
                fprintf(pFileDump, "\"0\",\n");
                fprintf(pFileDump, "\"%s\",\n",mdString);
                fprintf(pFileDump, "\"0\"\n");
            }
        break;

        case CODECTYPE_SIMCOP:
            if(pDBList){
                fprintf(pFileDump, "\"%s\",\n",pDBList->md5CheckSumArm);
                fprintf(pFileDump, "\"%s\",\n",pDBList->md5CheckSumTi);
                fprintf(pFileDump, "\"%s\"\n",mdString);
            }
            else {
                fprintf(pFileDump, "\"0\",\n");
                fprintf(pFileDump, "\"0\",\n");
                fprintf(pFileDump, "\"%s\"\n",mdString);
            }
        break;

        default:
        break;
    }
    fprintf(pFileDump, "},\n");

}  //End of dumpMd5Sum()

//-----------------------------------------------------------------------------
int verifyMd5Sum( char* InFileName, void *pBuf, unsigned long nBufSize, int flag ) {

    unsigned char md[MD5_SUM_LENGTH];   //16bytes (128bits)
    char mdString[(MD5_SUM_LENGTH * 2) + 1];    //32bytes + null char
    char imgFileName[128];
    const struct JPGD_TEST_OUTPUT_FILE_LIST *pMd5SumDBList = NULL;
    int i;
    int result = ERROR;

    /*Generate the md5sum for the current output from file */
    if ( pBuf == NULL) {
        md5SumFile( md, InFileName );
    }
    else {
    /*Generate the md5sum for the current output from buffer */
        md5SumBuf( md, pBuf, nBufSize );
    }

    /* Convert the md5 sum to a string */
    for (i = 0; i < MD5_SUM_LENGTH; i++){
        sprintf( (char*)&mdString[i*2],"%02x",md[i]);
    }

    /* Extract the file name alone from the full path */
    extractFileName(InFileName, imgFileName);
    PRINT("md5sum String = %s\n",mdString);

    /*comapre the md5sum with the data base */
    if ( flag == JPGD_MD5SUM_LIST ) {
        pMd5SumDBList = &st_jpgd_file_list[0];
    }
    else {
        pMd5SumDBList = &st_jpge_file_list[0];
    }

    for(i = 0; strlen(pMd5SumDBList[i].fileName) != 0; i++) {

        if ( strcmp ( pMd5SumDBList[i].fileName, imgFileName ) == 0 ) {
            /* File name found.  Now check the md5sum */
            if ( strcmp( pMd5SumDBList[i].md5CheckSumArm , (const char*)mdString ) == 0 ) {
                PRINT("Md5Sum matches with ARM Codec output.\n");
                updateTestCount(PASSCOUNT_ARM);
                result = PASS;
                break;
            }
            else if ( strcmp( pMd5SumDBList[i].md5CheckSumTi , (const char*)mdString ) == 0 ) {
                PRINT("Md5Sum matches with TI Codec output.\n");
                updateTestCount(PASSCOUNT_TIDSP);
                result = PASS;
                break;
            }
            else if ( strcmp( pMd5SumDBList[i].md5CheckSumSimcop , (const char*)mdString ) == 0 ) {
                PRINT("Md5Sum matches with SIMCOP Codec output.\n");
                updateTestCount(PASSCOUNT_SIMCOP);
                result = PASS;
                break;
            }
            else {
                PRINT("%s():%d:: ERROR!!! Md5Sum Mismatch !!!.\n",__FUNCTION__,__LINE__);
                result = FAIL;
                break;
            }
        }
    }

    if ( result == ERROR ) {
        PRINT("\n%s():%d:: !!!!!New test output file. Manual verification required.!!!!!.\n",__FUNCTION__,__LINE__);
        updateTestCount(COUNT_MANUALVERIFY);
        if(flagDumpMd5Sum) {
            dumpMd5Sum(imgFileName, mdString, NULL);
        }
        return PASS;
    }
    else {
        if(flagDumpMd5Sum) {
            dumpMd5Sum(imgFileName, mdString,(void*) &pMd5SumDBList[i]);
        }
        return result;
    }

} //End of verifyMd5Sum()

//-----------------------------------------------------------------------------
int calcScaleFactor(int userW, int userH, SkStream* iStream) {
    int nOrgW = 0;
    int nOrgH = 0;
    int scaleFactor = 1;
    S32 inputFileSize = 0;
    JPEG_IMAGE_INFO JpegImageInfo;
    char outFileName[256];

    /* Parse the input file and get the image Width and Height */
    inputFileSize = jpegHeaderParser(iStream, &JpegImageInfo);
    iStream->rewind();
    PRINT("Input Image W x H = %d x %d :: fileSize = %ld\n",JpegImageInfo.nWidth, JpegImageInfo.nHeight,inputFileSize);
    if ( inputFileSize == 0) {
        scaleFactor = 1;
    }
    else {
        nOrgW = JpegImageInfo.nWidth;
        nOrgH = JpegImageInfo.nHeight;

        if ( userW >= nOrgW || userH >= nOrgH ){
            scaleFactor = 1;
        }
        else if ( userW > 0 ) {
            scaleFactor = nOrgW / userW;
        }
        else if ( userH > 0 ) {
            scaleFactor = nOrgH / userH;
        }
    }
    PRINT("%s():%d:: Calculated ScaleFactor = %d\n",__FUNCTION__,__LINE__,scaleFactor);
    return scaleFactor;

} //End of calcScaleFactor()

//-----------------------------------------------------------------------------
bool isDSPJPGD(SkStream *stream) {
    char prop[2] = {'0','\0'};
    char sizeTh[] = "0000000000";
    unsigned int imageSizeTh;
    unsigned int fileSize = stream->getLength();

    property_get("jpeg.libskiahw.decoder.enable", prop, "0");
    if(prop[0] == '1'){	//uses DSP JPGD
        //check for the file size threshold
        property_get("jpeg.libskiahw.decoder.thresh", sizeTh, "0");
        imageSizeTh = atoi(sizeTh);
        if(fileSize > imageSizeTh) {
            return true;
        }
    }
    return false;   //uses SW JPGD

} //End of isDSPJPGD()
//-----------------------------------------------------------------------------
int runJPEGDecoderTest(int argc, char** argv) {

    SkImageDecoder* skJpegDec = NULL;
    SkBitmap    skBM;
    SkBitmap::Config prefConfig = SkBitmap::kRGB_565_Config;
    int reSize = 1;
    int nReqWidth = 0;
    int nReqHeight = 0;
    int result = PASS;
    char* cmd = NULL;
    JpegDecoderParams jpegDecParams;
    bool bSubRegDecFlag = false;

    memset((void*)&jpegDecParams, 0, sizeof(JpegDecoderParams) );
    flagCodecType = CODECTYPE_SW;

    /*check the parameter counts*/
    if (argc < 4) {
        PRINT("%s:%d::!!!!!Wrong number of arguments....\n",__FUNCTION__,__LINE__);
        printDecoderTestUsage();
        return ERROR;
    }

    /* check if the output file already exists and delete it*/
    {
        char rmCmd[300];
        FILE* fp = fopen(argv[3],"r");
        if (fp) {
            DBGPRINT("%s():%d:: Output file already exists. Deleting it.\n",__FUNCTION__,__LINE__);
            fclose(fp);
            sprintf(rmCmd,"rm %s",argv[3]);
            system(rmCmd);
        }
    }

    SkFILEStream   inStream(argv[2]);
    PRINT("InputFile  = %s\n",argv[2]);
    PRINT("OutputFile = %s\n",argv[3]);

    if (inStream.isValid() == false) {
        PRINT("%s:%d::!!!!!!!!!!Input File is not found....\n",__FUNCTION__,__LINE__);
        return ERROR;
    }

    /*output color format*/
    if (argc >= 5) {
        prefConfig = (SkBitmap::Config)atoi(argv[4]);
        if(prefConfig == SkBitmap::kARGB_8888_Config) {
            DBGPRINT("%s():%d:: Output Color Format : ARGB_8888 32bit.\n",__FUNCTION__,__LINE__);
        }
        else if (prefConfig == SkBitmap::kRGB_565_Config) {
            DBGPRINT("%s():%d:: Output Color Format : RGB_565 16bit.\n",__FUNCTION__,__LINE__);
        }
        else{
            /*Eventhough the android supports some more RGB color format, TI DSP SN supports the
              above two color format among the android color formats.
              TI DSP SN supports more formats in YUV too, but android does not support YUV o/p formats.*/
            PRINT("%s():%d::!!!!Invalid output color format option..\n",__FUNCTION__,__LINE__);

            /* Allow the wrong color format to test the error handling */
            //prefConfig = SkBitmap::kRGB_565_Config;
        }
    }

    /*resize mode*/
    if (argc >= 6) {
        reSize = atoi(argv[5]);

        switch ( reSize){
            case (12):
                reSize = 8;
                break;

            case (25):
                reSize = 4;
                break;

            case (50):
                reSize = 2;
                break;

            case (100):
                reSize = 1;
                break;

            default:
                reSize = 1;
                DBGPRINT("%s():%d:: Scalefactor set to default 1 (noResize) .\n",__FUNCTION__,__LINE__);
                break;
        }

        PRINT("%s():%d:: Scale factor=%d. (1-100,2-50,4-25,8-12 percent resize)\n",__FUNCTION__,__LINE__,reSize);
    }

    /*Section Decode - Slice output mode*/
    if (argc >= 7) {
    }
    else{
    }

   /*Sub Region Decode*/
    if (argc == 11) {
        jpegDecParams.nXOrg = atoi(argv[7]);
        jpegDecParams.nYOrg = atoi(argv[8]);
        jpegDecParams.nXLength = atoi(argv[9]);
        jpegDecParams.nYLength = atoi(argv[10]);

        if ( jpegDecParams.nXOrg || jpegDecParams.nYOrg ||
             jpegDecParams.nXLength || jpegDecParams.nYLength ) {
            bSubRegDecFlag = true;
        }
        /* Check for the co-ordinates multiples */
        /* TI DSP SN has contraint on the multiples of the co-ori\dinates */
        // Allow the users input. Libskiahw should take care in case of errors.
        DBGPRINT("%s():%d:: NOTE: Refer the Codec guide for the constraints on X & Y origin values.\n",__FUNCTION__,__LINE__);

    }
    else{
    }

    /* Check for the scale factor number : omap3= 1,2,4,8 ; OMAP4= 1 to 1256 */
    if (argc == 12) {
        reSize = atoi(argv[11]);
    }
    /* Check for the desired output resolution and modify the scale factor accordingly. */
    else if (argc == 13) {
        nReqWidth = atoi(argv[11]);
        nReqHeight = atoi(argv[12]);
        reSize = calcScaleFactor(nReqWidth, nReqHeight, &inStream );
    }

    /* Get the Decoder handle */
    skJpegDec = SkImageDecoder::Factory(&inStream);
    if( skJpegDec == NULL ) {
        PRINT("%s():%d:: !!!! NULL Decoder Handle received..\n",__FUNCTION__,__LINE__);
        return FAIL;
    }

    /*set the scale factor */
    skJpegDec->setSampleSize(reSize);

#ifndef TARGET_OMAP4
    /*check which decoder handles is chosen by the Factory()*/
    if( isDSPJPGD(&inStream) ){
        flagCodecType = CODECTYPE_DSP;

        /*set the subregion decode parameters*/
        if ( ((SkTIJPEGImageDecoderEntry*)skJpegDec)->SetJpegDecParams((SkTIJPEGImageDecoderEntry::JpegDecoderParams*) &jpegDecParams ) == false ) {
            PRINT("%s():%d:: !!!! skJpegDec->SetJpegDecodeParameters returned false..\n",__FUNCTION__,__LINE__);
        }
    }
    else { //SW decoder
        if( bSubRegDecFlag ){
            PRINT("%s():%d:: !!!! SW Decoder does not support the Sub Region Decoding...\n",__FUNCTION__,__LINE__);
            PRINT("%s():%d:: !!!! Select DSP JPG decoder and run subregion decode tests.\n",__FUNCTION__,__LINE__);
            return FAIL;
        }
    }
#endif

#ifdef TIME_MEASUREMENT
   {
    AutoTimeMicros atm("Decode Measurement");
#endif 
    /*call decode*/
    if (skJpegDec->decode(&inStream, &skBM, prefConfig, SkImageDecoder::kDecodePixels_Mode) == false) {
        PRINT("%s():%d:: !!!! skJpegDec->decode returned false..\n",__FUNCTION__,__LINE__);
        PRINT("%s():%d:: !!!! Test Failed..\n",__FUNCTION__,__LINE__);
        delete skJpegDec;
        return FAIL;
    }
#ifdef TIME_MEASUREMENT
   atm.setResolution(skBM.width() , skBM.height());
   atm.setScaleFactor(reSize);
   }
#endif

    {   //scope to close the output file/stream  handle
        SkFILEWStream   outStream(argv[3]);
        outStream.write(skBM.getPixels(), skBM.getSize());
        DBGPRINT("%s():%d:: Wrote %d bytes to output file<%s>.\n",__FUNCTION__,__LINE__,skBM.getSize(),argv[3]);
        delete skJpegDec;

         result = verifyMd5Sum(argv[3], skBM.getPixels(), skBM.getSize(), JPGD_MD5SUM_LIST );
    }

    //Now change the output filename with W & H if the scalefactor modified the output resolution
    if (reSize != 1) {
        char temp[256];
        PRINT("%s:%d:: Modified output WxH = %d x %d\n",__FUNCTION__,__LINE__,skBM.width(),skBM.height());
        strcpy(temp, argv[3]);
        cmd = strrchr(temp, '.');
        cmd[0] = '\0';
        cmd =(char*) malloc(600);
        if ( cmd !=NULL) {
            sprintf(cmd, "mv %s %s___%dx%d.raw",argv[3],temp,skBM.width(),skBM.height() );
            system(cmd);
            free(cmd);
        }
    }
    return result;

} //End of runJPEGDecoderTest()

//-----------------------------------------------------------------------------
int runJPEGEncoderTest(int argc, char** argv) {

    SkImageEncoder* skJpegEnc = NULL;
    SkBitmap skBM;
    SkBitmap::Config inConfig = SkBitmap::kRGB_565_Config;
    void *tempInBuff = NULL;
    int nWidth = 0;
    int nHeight = 0;
    int nQFactor = 100;

    /*check the parameter counts*/
    if (argc < 6) {
        PRINT("%s:%d::!!!!!Wrong number of arguments....\n",__FUNCTION__,__LINE__);
        printEncoderTestUsage();
        return ERROR;
    }

    /* check if the output file already exists and delete it*/
    {
        char rmCmd[300];
        FILE* fp = fopen(argv[3],"r");
        if (fp) {
            DBGPRINT("%s():%d:: Output file already exists. Deleting it.\n",__FUNCTION__,__LINE__);
            fclose(fp);
            sprintf(rmCmd,"rm %s",argv[3]);
            system(rmCmd);
        }
    }

    SkFILEStream   inStream(argv[2]);
    SkFILEWStream   outStream(argv[3]);

    PRINT("InputFile  = %s\n",argv[2]);
    PRINT("OutputFile = %s\n",argv[3]);

    if (inStream.isValid() == false) {
        PRINT("%s:%d::!!!!!!!!!!!Input File is not found....\n",__FUNCTION__,__LINE__);
        return ERROR;
    }

    /* width and height */
    nWidth = atoi(argv[4]);
    nHeight = atoi(argv[5]);

    /*Input color format*/
    if (argc >= 7) {
        inConfig = (SkBitmap::Config)atoi(argv[6]);
        if(inConfig == SkBitmap::kARGB_8888_Config) {
            DBGPRINT("%s():%d:: Input Color Format : ARGB_8888 32bit.\n",__FUNCTION__,__LINE__);
        }
        else if (inConfig == SkBitmap::kRGB_565_Config) {
            DBGPRINT("%s():%d:: Input Color Format : RGB_565 16bit.\n",__FUNCTION__,__LINE__);
        }
        else{
            /*Eventhough the android supports some more RGB color format, TI DSP SN supports the
              above two color format among the android color formats.
              TI DSP SN supports more formats in YUV too, but android does not support YUV i/p formats.*/
            PRINT("%s():%d::!!!!Invalid input color format option..\n",__FUNCTION__,__LINE__);

            /* still allows to execute to validate the error handling libskiiahw code */
        }
    }

    /* QFactor */
    if (argc >= 8) {
        nQFactor = atoi(argv[7]);
        DBGPRINT("%s():%d:: Quality Factor : %d.\n",__FUNCTION__,__LINE__,nQFactor);
    }

    /* create the input temp buffer for the input file */
    tempInBuff = (void*)malloc(inStream.getLength());
    if ( tempInBuff == NULL) {
        PRINT("%s():%d::!!!!malloc failed...\n",__FUNCTION__,__LINE__);
        return ERROR;
    }
    inStream.read(tempInBuff, inStream.getLength());

    /*configure the SkBitmap */
    skBM.setConfig(inConfig, nWidth, nHeight);
    skBM.setPixels(tempInBuff);

    /* get the JPEG Encoder Handle */
    skJpegEnc = SkImageEncoder::Create(SkImageEncoder::kJPEG_Type);

    /* call onEncode */
    if (skJpegEnc->encodeStream( &outStream, skBM, nQFactor) == false) {
        PRINT("%s():%d::!!!onEncode failed....\n",__FUNCTION__,__LINE__);
        free(tempInBuff);
        delete skJpegEnc;
        return FAIL;
    }

    DBGPRINT("%s():%d:: Encoded image written in to output file<%s>.\n",__FUNCTION__,__LINE__,argv[3]);
    free(tempInBuff);
    delete skJpegEnc;

    return PASS;
}//End of runJPEGEncoderTest()


//-----------------------------------------------------------------------------
int runFromCmdArgs(int argc, char** argv) {

    if(strcmp(argv[1],"D") == 0) {
        /*call decoder test function*/
        return runJPEGDecoderTest(argc, argv);
    }
    else if (strcmp(argv[1],"E") == 0) {
        /*call Encoder test function*/
        if( runJPEGEncoderTest(argc, argv) == PASS) {
            return ( verifyMd5Sum (argv[3], NULL, NULL, JPGE_MD5SUM_LIST) );
        }
        else {
            return FAIL;
        }
    }
    else {
        PRINT("%s():%d:: !!! Invalid test option. only D/E is valid (Decoder/Encoder).\n",__FUNCTION__,__LINE__);
        printUsage();
    }
    return ERROR;

}//end of runFromCmdArgs()

//-----------------------------------------------------------------------------
void RunFromScript(char* scriptFileName) {

    FILE* fp = NULL;
    char *pArgs[12];
    char strLine[500];
    char *pstrLine = strLine;
    int argsCount=0, i=0;
    int result = 0;

    DBGPRINT("%s():%d::Automated Test Started....\n",__FUNCTION__,__LINE__);
    fp = fopen( scriptFileName, "r");
    if ( NULL == fp ) {
        PRINT("\n !!Error while opening the script file-%s.\n",scriptFileName);
        return;
    }

    for(i = 0; i < 12; i++) {
        pArgs[i] = (char *)malloc(200);
    }

    /* Check the script file is valid one or not */
    memset( (void*)pstrLine, 0, 500);
    fgets (pstrLine, sizeof(strLine), fp);
    if (strncmp(LIBSKIAHWTEST, pstrLine, strlen(LIBSKIAHWTEST) ) != 0) {
        PRINT("\n%s():%d:: !!!!Not a  valid  script file-(%s).\n",__FUNCTION__,__LINE__,scriptFileName);
        printInputScriptFormat();
        fclose(fp);
        return;
    }

    while( !feof(fp) ) {

        /*Read one line from the script and parse*/
        memset( (void*)pstrLine, 0, 500);
        if( NULL == fgets (pstrLine, sizeof(strLine), fp) ) {
            /*file end*/
            break;
        }

        if ( strlen(pstrLine) < strlen(LIBSKIAHWTEST) ) {
            continue;
        }

        /*Check for comment line in the script and skip*/
        if (pstrLine[0] == '#' || pstrLine[0] == '\n') {
            pstrLine[0] = '\0';
            continue;
        }
        DBGPRINT("===============================================================================\n");
        DBGPRINT("TestCase read from script file: \n%s ",pstrLine);

        argsCount = sscanf(pstrLine,"%s %s %s %s %s %s %s %s %s %s %s %s %s", &testID[0],
                     pArgs[0],pArgs[1],pArgs[2],pArgs[3],pArgs[4],pArgs[5],
                     pArgs[6],pArgs[7],pArgs[8],pArgs[9],pArgs[10],pArgs[11]);

        if ( argsCount == 0 ) {
            /*Empty Line with whitespaces. Skip this and continue */
            continue;
        }

        DBGPRINT("TestCase: ./");
        for (i = 0; i < argsCount-1; i++) {
            DBGPRINT("%s ",pArgs[i]);
        }
        DBGPRINT("\n");

        PRINT("===============================================================================\n");
        PRINT("Executing %s.....\n",testID);
        result = runFromCmdArgs(argsCount-1,(char**)&pArgs);

        /* Print the result */
        if (result == FAIL){
            updateTestCount(FAILCOUNT);
            PRINT("\n%15s: <%s>.....FAIL !!!\n",testID,pArgs[2]);
        }
        else if(result == ERROR) {
            PRINT("\n%15s.....ERROR occured during the test !!!.\n",testID);
        }
        else {
            PRINT("\n%15s.....PASS\n",testID);
        }

    } //End of while loop


    for(i = 0; i < 12; i++) {
        free(pArgs[i]);
    }

    fclose(fp);
    PRINT("\n <--------------------------End of script: %s------------------------>\n",scriptFileName);

} //End of RunFromScript()

//-----------------------------------------------------------------------------
void parseFolderOptions(int argc, char** argv) {

    char* pStr = argv[1];
    char* pArgs[12];
    int argCnt = 0;
    int count = 1;
    int flagMemoryTest = 0;
    int result = PASS;
    int i;
    DIR* pDir = NULL;
    DIRENTRY* pDirEnt = NULL;

    flagDumpMd5Sum = 0;     //reset flag

    for(i = 0; i < 12; i++) {
        pArgs[i] = (char *)malloc(200);
    }

    //options:
    // M-memory test.
    // C-dump MD5-checksum strings into a file.
    for (i=0; pStr[i] != '\0'; i++) {
        switch (pStr[i]) {
            case 'M':
                flagMemoryTest = 1;
            break;

            case 'C':
                pFileDump = fopen("md5sum_dump.txt", "w");
                if (pFileDump == NULL) {
                    PRINT("%s():%d::!!!!! Counld not create <md5sum_dump.txt> file!!! \n",__FUNCTION__,__LINE__);
                    flagDumpMd5Sum = 0;
                }
                else {
                    flagDumpMd5Sum = 1;
                }
            break;

            default:
            break;
        }
    }

    if (flagMemoryTest) {
        //get the memory info
        system("cat /proc/meminfo > MemInfoBeforeTest.txt");
        system("cat MemInfoBeforeTest.txt");
    }

    if (argc == 3) {
        //run the script content one time
        count = 1;
    }
    else if (argc == 4 ){
        count = atoi(argv[3]);
        if (count == 0) {
            count =1;
        }
    }

    //Navigate through the directory for .jpg files.
    pDir = opendir(argv[2]);
    if (pDir == NULL) {
        PRINT("%s():%d:: !!!!! unable to open the Directory (%s)!!!!\n",__FUNCTION__,__LINE__,argv[2]);
        return;
    }

    //Create the parameters for decoder test
    strcpy(pArgs[0], argv[0]);
    argCnt++;

    //Decoder flag
    strcpy(pArgs[1], "D");
    argCnt++;

    //output color format
    if (argc > 4 ) {
        strcpy(pArgs[4], argv[4]);
    }
    else {
        strcpy(pArgs[4], "4");  //default RGB16
    }
    argCnt++;

    //scale factor percentage
    if (argc > 5 ) {
        strcpy(pArgs[5], argv[5]);
    }
    else {
        strcpy(pArgs[5], "100");  //default scale factor 100%
    }
    argCnt++;

    //Output Slice mode
    if (argc > 6 ) {
        strcpy(pArgs[6], argv[6]);
        argCnt++;
    }

    //Subregion Decode xOrg, yOrg, xLength, yLength
    if (argc > 10 ) {
        strcpy(pArgs[7], argv[7]);
        strcpy(pArgs[8], argv[8]);
        strcpy(pArgs[9], argv[9]);
        strcpy(pArgs[10], argv[10]);
        argCnt += 4;
    }

    //Scale Factor value or the desired output Width
    if (argc > 11 ) {
        strcpy(pArgs[11], argv[11]);
        argCnt++;
    }

    //desired output Height
    if (argc > 11 ) {
        strcpy(pArgs[11], argv[11]);
        argCnt++;
    }

    argCnt +=2;     //for input file and output file parameters
    for (i = 0; i < count; i++) {   //to repeat the folder
        do { //for the files inside the specified folder
            pDirEnt = readdir(pDir);
            if (pDirEnt != NULL) {
                if (strstr(pDirEnt->d_name, ".jpg") == NULL &&
                    strstr(pDirEnt->d_name, ".JPG") == NULL ) {
                    continue;
                }

                PRINT("\n-------------------------------------------------------------------------------\n");
                //input file
                strcpy(pArgs[2], argv[2]); //path
                pStr = argv[2];
                if ( pStr[strlen(pStr)-1] != '/') {
                    strcat(pArgs[2], "/");
                }
                strcat(pArgs[2], pDirEnt->d_name);  //append the file name

                //output File
                strcpy(pArgs[3], pArgs[2]);
                ((char*)pArgs[3])[strlen(pArgs[2])-3] = '\0';
                strcat(pArgs[3], "raw");

                result = runFromCmdArgs(argCnt, (char**)&pArgs);

                /* Print the result */
                if (result){
                    PRINT("\n<%s>.....FAIL !!!\n",pArgs[2]);
                }
                else {
                    PRINT("\n.....PASS\n\n");
                }
            } //end of if(pDir != NULL)
        } while (pDirEnt != NULL);
        PRINT("\n\nRepeating the folder... (%d) more times..\n\n",((count-1)-i) );
        rewinddir(pDir);
        PRINT("\n===================================================================================\n");
    } //end of for loop

    closedir(pDir);

    if (flagDumpMd5Sum == 1 && pFileDump != NULL) {
        fclose(pFileDump);
    }

    if (flagMemoryTest) {
        //wait for 2 seconds
        sleep(2);

        //get the memory info
        system("cat /proc/meminfo > MemInfoAfterTest.txt");
        system("cat MemInfoAfterTest.txt");

        //Analyze the memory info
        analyzeMemoryInfo("MemInfoBeforeTest.txt", "MemInfoAfterTest.txt");

        //delete the meminfo files
        system("rm MemInfoBeforeTest.txt");
        system("rm MemInfoAfterTest.txt");
    }

} //End of parseFolderOptions()

//-----------------------------------------------------------------------------
void parseScriptOptions(int argc, char** argv) {

    char* pStr = argv[1];
    int flagMemoryTest = 0;
    int i, count = 1;

    flagDumpMd5Sum = 0;     //reset flag

    //options:
    // M-memory test.
    // C-dump MD5-checksum strings into a file.
    for (i=0; pStr[i] != '\0'; i++) {
        switch (pStr[i]) {
            case 'M':
                flagMemoryTest = 1;
            break;

            case 'C':
                pFileDump = fopen("md5sum_dump.txt", "w");
                if (pFileDump == NULL) {
                    PRINT("%s():%d::!!!!! Counld not create <md5sum_dump.txt> file!!! \n",__FUNCTION__,__LINE__);
                    flagDumpMd5Sum = 0;
                }
                else {
                    flagDumpMd5Sum = 1;
                }
            break;

            default:
            break;
        }
    }

    if (flagMemoryTest) {
        //get the memory info
        system("cat /proc/meminfo");
        system("cat /proc/meminfo > MemInfoBeforeTest.txt");
    }

    if (argc == 3) {
        //run the script content one time
        count = 1;
    }
    else if (argc == 4) {
        count = atoi(argv[3]);
        if (count == 0) {
            count =1;
        }
    }

    for(i = 0; i < count; i++) {
        /* Test using script file*/
        RunFromScript(argv[2]);
        PRINT("\n\nRepeating the Script... (%d) more times..\n\n",((count-1)-i) );
    }

    if (flagDumpMd5Sum == 1 && pFileDump != NULL) {
        fclose(pFileDump);
    }

    if (flagMemoryTest) {
        //get the memory info
        sleep(2);
        system("cat /proc/meminfo");
        system("cat /proc/meminfo > MemInfoAfterTest.txt");

        //Analyze the memory info
        analyzeMemoryInfo("MemInfoBeforeTest.txt", "MemInfoAfterTest.txt");

        //delete the meminfo files
        system("rm MemInfoBeforeTest.txt");
        system("rm MemInfoAfterTest.txt");
    }


} //End of parseScriptOptions()

//-----------------------------------------------------------------------------
int main(int argc, char** argv) {

    int result = PASS;
    int count = 0;
    int i = 0;
    unsigned int nTotalTests = 0;
    char* pStr = NULL;

    nTestCount[0] = 0;
    nTestCount[1] = 0;
    nTestCount[2] = 0;
    nTestCount[3] = 0;
    nTestCount[4] = 0;

    PRINT("\n|------------------------------------------------------------------------------|\n");
    PRINT  ("|-Skia JPEG Decoder/Encoder Test App built on:"__DATE__":"__TIME__"\n");
    PRINT  ("|------------------------------------------------------------------------------|\n");

    if (argc < 2) {
        printUsage();
        return 0;
    }
    pStr = (char*)argv[1];
    switch (pStr[0]) {

        case 'D':
        case 'E':
            /* test using command line parameters*/
            result = runFromCmdArgs(argc, argv);

            if (result == PASS){
                PRINT("\nTest.....PASS\n");
            }
            else {
                PRINT("\nTest.....FAIL !!!\n");
            }

        break;

        case 'F':
            //Folder for Decoder test only. Decodes all the jpg files in the folder specified.
            parseFolderOptions(argc, argv);
        break;

        case 'S':
            parseScriptOptions(argc, argv);
        break;

        default:
            PRINT("%s():%d::!!!! Invalid options..\n",__FUNCTION__,__LINE__);
            printUsage();
        break;
    }

    //print the PASS FAIL report
    nTotalTests = ( nTestCount[0] + nTestCount[1] + nTestCount[2] + nTestCount[3] + nTestCount[4]);
    if ( nTotalTests ) {
        PRINT("\n|------------------------------------------------------------------------------|\n");
        PRINT  ("| Total Test cases run           = %d\n|\n",nTotalTests );
        PRINT  ("| Test cases run on ARM          = %d\t (%d %c)\n",nTestCount[0], ((100*nTestCount[0])/nTotalTests),'%' );
        PRINT  ("| Test cases run on TI-DSP       = %d\t (%d %c)\n",nTestCount[1], ((100*nTestCount[1])/nTotalTests),'%' );
        PRINT  ("| Test cases run on SIMCOP       = %d\t (%d %c)\n",nTestCount[2], ((100*nTestCount[2])/nTotalTests),'%' );
        PRINT  ("| Test cases FAILED              = %d\t (%d %c)\n",nTestCount[3], ((100*nTestCount[3])/nTotalTests),'%' );
        PRINT  ("| Test cases needs manual check  = %d\t (%d %c)\n",nTestCount[4], ((100*nTestCount[4])/nTotalTests),'%' );
        PRINT("|\n|------------------------------------------------------------------------------|\n");
    }

    return 0;
} //end of main





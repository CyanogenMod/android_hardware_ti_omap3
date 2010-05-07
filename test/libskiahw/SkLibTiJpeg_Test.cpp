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

#include <stdio.h>
#include <stdlib.h>
#include "SkLibTiJpeg_Test.h"

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

//test case count
unsigned int nTestCount[5];	//[0]-ARM; [1]-TI;
                            //[2]-SIMCOP; [3]-Fail count;
                            //[4]-manual verification needed

//-----------------------------------------------------------------------------
void printDecoderTestUsage() {

    PRINT("\nDecoder Test parameters:\n");
    PRINT("SkLibTiJpeg_Test <D> <Input_File_Name> <Output_File_Name> <nOutformat> <nResizeMode> <nSections> <nXOrigin> <nYOrigin> <nXLenght> <nYLenght>\n\n ");
    PRINT("<D>               = D-Decoder test\n");
    PRINT("<Input_File_Name> = .jpg file name\n");
    PRINT("<Output_File_Name>= .raw file name\n");
    PRINT("<nOutformat>      = Output color format: 4-16bit_RBG565, 6-32bit_ARGB8888\n");
    PRINT("<nResizeMode>     = Scalefactor: 100, 50, 25, 12 percent\n");
    PRINT("<nSections>       = SliceOutput Mode: Number of rows per slice\n");
    PRINT("<nXOrigin>        = Sub Region Decode: X-origin\n");
    PRINT("<nYOrigin>        = Sub Region Decode: Y-origin\n");
    PRINT("<nXLength>        = Sub Region Decode: X-Length\n");
    PRINT("<nYLength>        = Sub Region Decode: Y-Length\n");
} //End of printDecoderTestUsage()

//-----------------------------------------------------------------------------
void printEncoderTestUsage() {

    PRINT("\nEncoder Test parameters:\n");
    PRINT("SkLibTiJpeg_Test <E> <Input_File_Name> <Output_File_Name> <nInformat> <nQFactor 1-100>\n\n ");
    PRINT("<E>               = E-Encoder test\n");
    PRINT("<Input_File_Name> = .raw file name\n");
    PRINT("<Output_File_Name>= .jpg file name\n");
    PRINT("<nInformat>       = Input color format: 4-16bit_RBG565, 6-32bit_ARGB8888\n");
    PRINT("<nQFactor>        = Quality factor: 1-100\n");
} //End of printEncoderTestUsage()

//-----------------------------------------------------------------------------
void printInputScriptFormat() {

    PRINT("\n The script file should contain <%s> as the first line.\n",LIBSKIAHWTEST);
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

   PRINT("\n\nUSAGE: \n\n./SkLibTiJpeg_Test <script file name> <Repeat Count> OR \n\n");
   PRINT("./SkLibTiJpeg_Test <E | D> < parameters..... > \n");

   printInputScriptFormat();
   PRINT("\n|------------------------------------------------------------------------------|\n");
   printDecoderTestUsage();
   PRINT("\n|------------------------------------------------------------------------------|\n");
   printEncoderTestUsage();
   PRINT("\n|------------------------------------------------------------------------------|\n");
   PRINT("\n|------------------------------------------------------------------------------|\n");

} //End of printUsage()

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

void updateTestCount(int id) {

    android::countMutex.lock();
    if ( id < 4 ) {
        nTestCount[id] += 1;
    }
    android::countMutex.unlock();

}

//-----------------------------------------------------------------------------
int verifyMd5Sum( char* InFileName, void *pBuf, unsigned long nBufSize, int flag ) {

    unsigned char md[MD5_SUM_LENGTH];	//16bytes (128bits)
    char mdString[(MD5_SUM_LENGTH * 2) + 1];	//32bytes + null char
    char imgFileName[128];
    const struct JPGD_TEST_OUTPUT_FILE_LIST *pMd5SumDBList = NULL;
    int i;

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
                PRINT("Md5Sum matches with ARM decoder output.\n");
                updateTestCount(PASSCOUNT_ARM);
                return PASS;
            }
            else if ( strcmp( pMd5SumDBList[i].md5CheckSumTi , (const char*)mdString ) == 0 ) {
                PRINT("Md5Sum matches with TI decoder output.\n");
                updateTestCount(PASSCOUNT_TIDSP);
                return PASS;
            }
            else if ( strcmp( pMd5SumDBList[i].md5CheckSumSimcop , (const char*)mdString ) == 0 ) {
                PRINT("Md5Sum matches with SIMCOP decoder output.\n");
                updateTestCount(PASSCOUNT_SIMCOP);
                return PASS;
            }
            else {
                PRINT("%s():%d:: ERROR!!! Md5Sum Mismatch !!!.\n",__FUNCTION__,__LINE__);
                updateTestCount(FAILCOUNT);
                return FAIL;
            }
        }
    }
    PRINT("\n%s():%d:: !!!!!New test output file. Manual verification required.!!!!!.\n",__FUNCTION__,__LINE__);
    updateTestCount(COUNT_MANUALVERIFY);
    return PASS;

} //End of verifyMd5Sum()

//-----------------------------------------------------------------------------
int runJPEGDecoderTest(int argc, char** argv) {

    SkImageDecoder* skJpegDec = NULL;
    SkBitmap    skBM;
    SkBitmap::Config prefConfig = SkBitmap::kRGB_565_Config;
    int reSize = 1;

    /*check the parameter counts*/
    if (argc < 4) {
        PRINT("%s:%d::!!!!!Wrong number of arguments....\n",__FUNCTION__,__LINE__);
        printDecoderTestUsage();
        return ERROR;
    }

    SkFILEStream   inStream(argv[2]);
    SkFILEWStream   outStream(argv[3]);

    PRINT("InputFile  = %s\n",argv[2]);
    PRINT("OutputFile = %s\n",argv[3]);

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

        DBGPRINT("%s():%d:: Scale factor=%d. (1-100,2-50,4-25,8-12 percent resize)\n",__FUNCTION__,__LINE__,reSize);
    }

    /*Section Decode - Slice output mode*/
    if (argc >= 7) {
    }
    else{
    }

   /*Sub Region Decode*/
    if (argc == 11) {
    }
    else{
    }

    /* Get the Decoder handle */
    skJpegDec = SkImageDecoder::Factory(&inStream);

    /*set the scale factor */
    skJpegDec->setSampleSize(reSize);
    
    /*call decode*/
    if (skJpegDec->decode(&inStream, &skBM, prefConfig, SkImageDecoder::kDecodePixels_Mode) == false) {
        PRINT("%s():%d:: !!!! skJpegDec->decode returned false..\n",__FUNCTION__,__LINE__);
        PRINT("%s():%d:: !!!! Test Failed..\n",__FUNCTION__,__LINE__);
        delete skJpegDec;
        return FAIL;
    }

    outStream.write(skBM.getPixels(), skBM.getSize());
    DBGPRINT("%s():%d:: Wrote %d bytes to output file<%s>.\n",__FUNCTION__,__LINE__,skBM.getSize(),argv[3]);
    delete skJpegDec;

    return ( verifyMd5Sum(argv[3], skBM.getPixels(), skBM.getSize(), JPGD_MD5SUM_LIST ) );

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

    SkFILEStream   inStream(argv[2]);
    SkFILEWStream   outStream(argv[3]);

    PRINT("InputFile  = %s\n",argv[2]);
    PRINT("OutputFile = %s\n",argv[3]);

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
    char testID[40];
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
        if (result){
            PRINT("\n%15s: <%s>.....FAIL !!!\n",testID,pArgs[2]);
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

} //End of RunFromScript

//-----------------------------------------------------------------------------
int main(int argc, char** argv) {

    int result = PASS;
    int count = 0;
    int i = 0;
    unsigned int nTotalTests = 0;

    nTestCount[0] = 0;
    nTestCount[1] = 0;
    nTestCount[2] = 0;
    nTestCount[3] = 0;
    nTestCount[4] = 0;

    PRINT("\n|------------------------------------------------------------------------------|\n");
    PRINT  ("|-Skia JPEG Decoder/Encoder Test App built on:"__DATE__":"__TIME__"\n");
    PRINT  ("|------------------------------------------------------------------------------|\n");
    if (argc > 3) {
        /* test using command line parameters*/
        result = runFromCmdArgs(argc, argv);
 
        if (result){
            PRINT("\nTest.....FAIL !!!\n");
        }
        else {
            PRINT("\nTest.....PASS\n");
        }
    }
    else if (argc == 2) {
        /* Test using script file*/
        RunFromScript(argv[1]);
    }
    else if (argc == 3) {
        count = atoi(argv[2]);
        if (count == 0) {
            count =1;
        }
        for(i = 0; i < count; i++) {
            /* Test using script file*/
            RunFromScript(argv[1]);
            PRINT("\n\nRepeating the Script... (%d) more times..\n\n",((count-1)-i) );
        }
    }
    else {
        /* Parameters required*/
        printUsage();
    }

    //print the PASS FAIL report
    nTotalTests = ( nTestCount[0] + nTestCount[1] + nTestCount[2] + nTestCount[3] + nTestCount[4]);

    PRINT("\n|------------------------------------------------------------------------------|\n");
    PRINT  ("| Total Test cases run           = %d\n\n",nTotalTests );
    PRINT  ("| Test cases run on ARM          = %d\t (%d %c)\n",nTestCount[0], ((nTestCount[0]/nTotalTests)*100),'%' );
    PRINT  ("| Test cases run on TI-DSP       = %d\t (%d %c)\n",nTestCount[1], ((nTestCount[1]/nTotalTests)*100),'%' );
    PRINT  ("| Test cases run on SIMCOP       = %d\t (%d %c)\n",nTestCount[2], ((nTestCount[2]/nTotalTests)*100),'%' );
    PRINT  ("| Test cases FAILED              = %d\t (%d %c)\n",nTestCount[3], ((nTestCount[3]/nTotalTests)*100),'%' );
    PRINT  ("| Test cases needs manual check  = %d\t (%d %c)\n",nTestCount[4], ((nTestCount[4]/nTotalTests)*100),'%' );
    PRINT("\n|------------------------------------------------------------------------------|\n");

    return 0;
} //end of main





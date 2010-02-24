#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <dlfcn.h>
#include <semaphore.h>

#include <LCML_DspCodec.h>
#include "scale.h"
#include <utils/Log.h>

#define USN_DLL_NAME "usn.dll64P"
#define VPP_NODE_DLL "vpp_sn.dll64P"
#define NUM_OF_VPP_BUFFERS (1)

#define ALIGNMENT 4096
#define ALIGN(p)  ((void*)((((unsigned long) p + ALIGNMENT - 1) / ALIGNMENT) * ALIGNMENT))

static const struct DSP_UUID COMMON_TI_UUID = {
        0x79A3C8B3, 0x95F2, 0x403F, 0x9A, 0x4B, {
        0xCF, 0x80, 0x57, 0x73, 0x05, 0x41
    }
};

typedef struct GPPToVPPInputFrameStatus {

    /* INPUT FRAME */
      
    /* input size*/
    OMX_U32      ulInWidth;          /*  picture buffer width          */ 
    OMX_U32      ulInHeight;         /*  picture buffer height         */ 
    OMX_U32      ulCInOffset;        /* offset of the C frame in the   *
                                    * buffer (equal to zero if there *
                                    * is no C frame)                 */ 
    
    /* PROCESSING PARAMETERS */
    
    /*    crop           */ 
    OMX_U32      ulInXstart;          /*  Hin active start             */ 
    OMX_U32      ulInXsize;           /*  Hin active width             */ 
    OMX_U32      ulInYstart;          /*  Vin active start             */ 
    OMX_U32      ulInYsize;           /* Vin active height             */ 

    /*   zoom            */ 
    OMX_U32      ulZoomFactor;        /*zooming ratio (/1024)          */ 
    OMX_U32      ulZoomLimit;         /* zooming ratio limit (/1024)   */ 
    OMX_U32      ulZoomSpeed;         /* speed of ratio change         */ 

    /*  stabilisation             */ 
    OMX_U32      ulXoffsetFromCenter16;    /*  add 1/16/th accuracy offset */ 
    OMX_U32      ulYoffsetFromCenter16;    /* add 1/16/th accuracy offset  */ 

    /*  gain and contrast             */ 
    OMX_U32      ulContrastType;      /*    Contrast method            */ 
    OMX_U32      ulVideoGain;         /* gain on video (Y and C)       */ 

    /*  effect             */ 
    OMX_U32      ulFrostedGlassOvly;  /*  Frosted glass effect overlay          */ 
    OMX_U32      ulLightChroma;       /*  Light chrominance process             */ 
    OMX_U32      ulLockedRatio;       /*  keep H/V ratio                        */ 
    OMX_U32      ulMirror;            /*  to mirror the picture                 */ 
    OMX_U32      ulRGBRotation;          /*  0, 90, 180, 270 deg.                  */ 
    OMX_U32      ulYUVRotation;          /*  0, 90, 180, 270 deg.                  */ 
  
#ifndef _55_
    OMX_U32	     eIORange;              /*  Video Color Range Conversion */	
    OMX_U32      ulDithering;           /*  dithering                             */
    OMX_U32      ulOutPitch;		 // OutPitch (bytes)
	OMX_U32      ulAlphaRGB;		 // Global A value of an ARGB output
    
#endif
  
}GPPToVPPInputFrameStatus;


/* OUTPPUT BUFFER */

typedef struct GPPToVPPOutputFrameStatus {

    OMX_U32      ulOutWidth;          /*  RGB/YUV picture buffer width           */ 
    OMX_U32      ulOutHeight;         /*  RGB/YUV picture buffer height          */ 
    OMX_U32      ulCOutOffset;        /*  Offset of the C frame in the buffer (equal to 0 if there is no C frame)             */ 
  
}GPPToVPPOutputFrameStatus;



OMX_HANDLETYPE      pDllHandle;
LCML_DSP_INTERFACE* pLCML;

//sem_t   semResized;

//We suspect a problem with semaphores (and possibly all futex-es) so we are going to wait on a pipe
int     pipeResized[2];

#define READ_END    0
#define WRITE_END   1

/* -------------------------------------------------------------------*/
/**
  *  GetLCMLHandle() function will be called to load LCML component 
  *
  * 
  *
  * @retval OMX_NoError              Success, ready to roll
  *         OMX_ErrorUndefined    The input parameter pointer is null
  **/
/*-------------------------------------------------------------------*/
LCML_DSP_INTERFACE* GetLCMLHandle()
{
    OMX_ERRORTYPE (*fpGetHandle)(OMX_HANDLETYPE);
    OMX_HANDLETYPE pHandle = NULL;
    char *error = NULL;
    OMX_ERRORTYPE eError;

#if 1 
    pDllHandle = dlopen("libLCML.so", RTLD_LAZY);
    if (!pDllHandle) {
        fprintf(stderr,"dlopen: %s",dlerror());
        goto EXIT;
    }

    fpGetHandle = dlsym (pDllHandle, "GetHandle");
    error = (char *) dlerror();
    if ( error != NULL) {
        if(fpGetHandle){
                dlclose(pDllHandle);
                pDllHandle = NULL;
            }
        fprintf(stderr,"dlsym: %s", error);
        goto EXIT;
    }
#else

    OMX_ERRORTYPE GetHandle(OMX_HANDLETYPE *hInterface );
    fpGetHandle = GetHandle;

#endif

    if (fpGetHandle) {
        eError = (*fpGetHandle)(&pHandle);
        if(eError != OMX_ErrorNone) {
            eError = OMX_ErrorUndefined;
            LOGV("eError != OMX_ErrorNone...\n");
            pHandle = NULL;
            goto EXIT;
        }
    } else {
       fprintf(stderr,"ERROR: fpGetHandle is NULL.\n");
    }

EXIT:
    return pHandle;
}

OMX_ERRORTYPE Fill_LCMLInitParams(OMX_U16 arr[], LCML_DSP *plcml_Init, int inWidth, int inHeight, int outWidth, int outHeight, int inFmt, int outFmt)
{

    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 nIpBuf,nIpBufSize,nOpBuf,nOpBufSize;
    char  valueStr[52]; /*Changed length*/                      
    OMX_U32 Input_FrameWidth;    
    OMX_U32 Output_FrameWidth;    
    OMX_U16 OutputRGB_Format;    
    OMX_U16 Input_FrameFormat;    
    OMX_U16 Output_FrameFormat;    
    OMX_U16 Overlay;            
    OMX_U16 Alpha = 0; /*Not implemented at OMX level*/
    OMX_U16 ParamSize = 0;
    char * pcSNArgs = NULL;
    OMX_U8 *pTemp = NULL;
    int index;

    nIpBuf = 1;
    nIpBufSize = inWidth*inHeight*2;

    nOpBuf = 1;
    nOpBufSize = outWidth*outHeight*2;

    plcml_Init->In_BufInfo.nBuffers      = nIpBuf;
    plcml_Init->In_BufInfo.nSize         = nIpBufSize;
    plcml_Init->In_BufInfo.DataTrMethod  = DMM_METHOD;
    plcml_Init->Out_BufInfo.nBuffers     = nOpBuf;
    plcml_Init->Out_BufInfo.nSize        = nOpBufSize;
    plcml_Init->Out_BufInfo.DataTrMethod = DMM_METHOD;

    plcml_Init->DeviceInfo.TypeofDevice       = 0;
    plcml_Init->DeviceInfo.DspStream          = NULL;
    plcml_Init->NodeInfo.nNumOfDLLs           = 3;
    plcml_Init->NodeInfo.AllUUIDs[0].uuid     = &VPPNODE_TI_UUID;
    strcpy ((char *)plcml_Init->NodeInfo.AllUUIDs[0].DllName, VPP_NODE_DLL);
    plcml_Init->NodeInfo.AllUUIDs[0].eDllType = DLL_NODEOBJECT;

    plcml_Init->NodeInfo.AllUUIDs[1].uuid     = &VPPNODE_TI_UUID;
    strcpy ((char *)plcml_Init->NodeInfo.AllUUIDs[1].DllName, VPP_NODE_DLL);
    plcml_Init->NodeInfo.AllUUIDs[1].eDllType = DLL_DEPENDENT;

    plcml_Init->NodeInfo.AllUUIDs[2].uuid     = (struct DSP_UUID *) &COMMON_TI_UUID;
    strcpy ((char *)plcml_Init->NodeInfo.AllUUIDs[2].DllName, USN_DLL_NAME);
    plcml_Init->NodeInfo.AllUUIDs[2].eDllType = DLL_DEPENDENT;
    
    plcml_Init->SegID     = 0;
    plcml_Init->Timeout   = -1;
    plcml_Init->Alignment = 0;
    plcml_Init->Priority = 5;

    plcml_Init->ProfileID = 0;
    /*Main input port */
    arr[0] = 5; /*# of Streams*/
    arr[1] = 0; /*Stream ID*/
    arr[2] = 0; /*Stream based input stream*/
    arr[3] = NUM_OF_VPP_BUFFERS; /*Number of buffers on input stream*/
    /*Overlay input port*/
    arr[4] = 1; /*Stream ID*/
    arr[5] = 0; /*Stream based input stream*/
    arr[6] = NUM_OF_VPP_BUFFERS; /*Number of buffers on input stream*/
    /*RGB output port*/
    arr[7] = 2; /*Stream ID*/
    arr[8] = 0; /*Stream basedoutput stream for RGB data*/
    arr[9] = NUM_OF_VPP_BUFFERS; /*Number of buffers on output stream*/
    /*YUV output port*/
    arr[10] = 3; /*Stream ID*/
    arr[11] = 0; /*Stream based output stream for YUV data*/
    arr[12] = NUM_OF_VPP_BUFFERS; /*Number of buffers on output stream*/
    /*Alpha input port, Not implemented at OMX level*/
    arr[13] = 4; /*Stream ID*/
    arr[14] = 0; /*Stream based input stream*/
    arr[15] = NUM_OF_VPP_BUFFERS; /*Number of buffers on output stream*/


    pcSNArgs = (char *) (arr + 16);


    Input_FrameWidth    = inWidth;
    Output_FrameWidth   = outWidth;

    if ( inFmt )
        Input_FrameFormat = VGPOP_E420_IN;
    else
        Input_FrameFormat = VGPOP_E422_IN_UY;   //422 interlieved

    OutputRGB_Format    = VGPOP_ERGB_NONE;

    if ( outFmt )
        Output_FrameFormat  = VGPOP_E420_OUT;
    else
        Output_FrameFormat  = VGPOP_E422_OUT_UY;

    /*for overlay*/
    //Overlay = 0;
    Overlay = 0;

    memset(valueStr, 0, sizeof(valueStr));
//     LOGV(":%lu:%lu:%u:%u:%u:%d\n",
//     Input_FrameWidth,
//     Output_FrameWidth,
//     OutputRGB_Format,
//     Input_FrameFormat,
//     Output_FrameFormat,
//     Overlay);  
    sprintf(valueStr, ":%lu:%lu:%u:%u:%u:%d:%d\n",
    Input_FrameWidth,
    Output_FrameWidth,
    OutputRGB_Format,
    Input_FrameFormat,
    Output_FrameFormat,
    Overlay,
    Alpha);


    while(valueStr[ParamSize] != '\0'){
        ParamSize++;
    }

    /*Copy VPP parameters */
    pTemp = memcpy(pcSNArgs,valueStr,ParamSize);
    if(pTemp == NULL){
        eError = OMX_ErrorUndefined;
        goto EXIT;
    }

    if ( (ParamSize % 2) != 0) {
        index =(ParamSize+1) >> 1;
    }
    else {
        index = ParamSize >> 1;  /*Divide by 2*/
    }
    index = index + 16;  /*Add 16 to the index in order to point to the correct location*/

    arr[index] = END_OF_CR_PHASE_ARGS;
    plcml_Init->pCrPhArgs = arr;

EXIT:
    return eError;
}

int correct_range(int val, int range)
{
    if( val < 0 )
        return 0;
    else if( val > range)
        return range;

    return val;
}

/**
* aspect_ratio_calc() This routine calculates crop rect and position to
* keep correct aspect ratio from original frame
*
* @param sens_width      unsigned int - Sensor image width (before HW resizer)
*
* @param sens_height     unsigned int - Sensor image height (before HW resizer)
*
* @param pixel_width     unsigned int - Pixel width (according to sensor mode)
*
* @param pixel_height    unsigned int - Pixle height (according to sensor mode)
*
* @param src_width       unsigned int - Source image width (after HW resizer)
*
* @param src_height      unsigned int - Source image height (after HW resizer)
*
* @param dst_width       unsigned int - Destination image width (after HW resizer)
*
* @param dst_height      unsigned int - Destination image height (after HW resizer)
*
* @param align_crop_width  unsigned int-Align result requirements by horizontal for Crop
*
* @param align_crop_height unsigned int-Align result requirements by vertical for Crop
*
* @param align_pos_width   unsigned int-Align result requirements by horizontal for Position
*
* @param align_pos_height  unsigned int-Align result requirements by vertical for Position
*
* @param crop_src_left   *int - Return crop rectangel: left
*
* @param crop_src_top    *int - Return crop rectangel: top
*
* @param crop_src_width  *int - Return crop rectangel: width
*
* @param crop_src_height *int - Return crop rectangel: height
*
* @param pos_dst_left    *int - Return crop rectangel: left (Can be NULL)
*
* @param pos_dst_top     *int - Return crop rectangel: top (Can be NULL)
*
* @param pos_dst_width   *int - Return crop rectangel: width (Can be NULL)
*
* @param pos_dst_height  *int - Return crop rectangel: height (Can be NULL)
*
* @param flags           unsigned int - Flags
*
* @param zero if no error
*
*/
/* ========================================================================== */
int aspect_ratio_calc(
        unsigned int sens_width,  unsigned int sens_height,
        unsigned int pix_width,   unsigned int pix_height,
        unsigned int src_width,   unsigned int src_height,
        unsigned int dst_width,   unsigned int dst_height,
        unsigned int align_crop_width, unsigned int align_crop_height,
        unsigned int align_pos_width,  unsigned int align_pos_height,
        int *crop_src_left,  int *crop_src_top,
        int *crop_src_width, int *crop_src_height,
        int *pos_dst_left,   int *pos_dst_top,
        int *pos_dst_width,  int *pos_dst_height,
        unsigned int flags)
{
    unsigned int phys_rate, src_rate, dst_rate;
    unsigned int cswidth, csheight; // cs - Crop Source
    unsigned int pdwidth, pdheight; // pd - Position Destination
    unsigned int phys_width, phys_height;
    unsigned int max_width, max_height;
    int error = 0;

#ifdef DEBUG_LOG
    LOGE("aspect_ratio_calc Enter");
#endif

    if ((crop_src_width==NULL) || (crop_src_height==NULL) ||
        (crop_src_left==NULL) || (crop_src_top==NULL)) {

        error = -1;

        LOGE("aspect_ratio_calc Invalid arguments");
        goto fail;
    }

#ifdef DEBUG_LOG
    LOGE("Input Parameters:");
    LOGE("\tSensor      : Width = %5d,  Height = %5d",
        (int)sens_width, (int)sens_height);
    LOGE("\tPixel Aspect: Width = %5d,  Height = %5d",
        (int)pix_width, (int)pix_height);
    LOGE("\tSource      : Width = %5d,  Height = %5d",
        (int)src_width, (int)src_height);
    LOGE("\tDestination : Width = %5d,  Height = %5d",
        (int)dst_width, (int)dst_height);
    LOGE("\tAlign Crop  : Width = %5d,  Height = %5d",
        (int)align_crop_width, (int)align_crop_height);
    LOGE("\tAlign Pos   : Width = %5d,  Height = %5d",
        (int)align_pos_width, (int)align_pos_height);
    LOGE("\tFlags       : %s,%s,%s,%s,%s",
        (flags & ASPECT_RATIO_FLAG_KEEP_ASPECT_RATIO)?"Keep Aspect Ratio":"-",
        (flags & ASPECT_RATIO_FLAG_SHRINK_WHOLE_SRC )?"Shrink Whole Src":"-",
        (flags & ASPECT_RATIO_FLAG_CROP_BY_PHYSICAL)?"Crop rect by Physical area":"-",
        (flags & ASPECT_RATIO_FLAG_CROP_BY_SOURCE)?"Crop rect by Source area":"-",
        (flags & ASPECT_RATIO_FLAG_CROP_BY_DESTINATION)?"Crop rect by Destination area":"-");
#endif

    // Correct source angle according to pixel aspect ration
    phys_width  = sens_width  * pix_width;
    phys_height = sens_height * pix_height;

#ifdef DEBUG_LOG
    LOGE("\tPhisical    : Width = %5d,  Height = %5d",
        (int)phys_width, (int)phys_height);
#endif

    // Calculate Aspect ration by physical, multiplied by (1<<16)
    phys_rate = (phys_width<<16) / phys_height;

    // Calculate Aspect ration by source multiplied by (1<<16)
    src_rate = (src_width<<16) / src_height;

    // Calculate Aspect ration by destination multiplied by (1<<16)
    dst_rate = (dst_width <<16) / dst_height;

#ifdef DEBUG_LOG
    LOGE( "\tScale       : Phys  = %5d,  Src    = %5d,  Dst   = %5d",
        (int)phys_rate>>10, (int)src_rate>>10, (int)dst_rate>>10);
#endif

    cswidth  = -1;
    csheight = -1;
    pdwidth  = dst_width;
    pdheight = dst_height;

    // Compare Source and destination aspect ratio
    if ((phys_rate>>10) == (dst_rate>>10)) {
        cswidth  = phys_width;
        csheight = phys_height;
    } else {
        if (phys_rate < dst_rate) {
            if (flags & ASPECT_RATIO_FLAG_SHRINK_WHOLE_SRC) {
                csheight = phys_height;
                pdwidth  = -1;
            } else {
                cswidth  = phys_width;
            }
        } else {
            if (flags & ASPECT_RATIO_FLAG_SHRINK_WHOLE_SRC) {
                cswidth  = phys_width;
                pdheight = -1;
            } else {
                csheight = phys_height;
            }
        }
    }

    if ((signed)cswidth == -1) {
        cswidth  = (phys_height * dst_rate) >> 16;
    }

    if ((signed)csheight == -1) {
        csheight = (phys_width << 16) / dst_rate;
    }

    switch (flags & ASPECT_RATIO_FLAG_CROP_BY_ALL) {
    case ASPECT_RATIO_FLAG_CROP_BY_PHYSICAL:
        max_width  = phys_width;
        max_height = phys_height;
        break;

    case ASPECT_RATIO_FLAG_CROP_BY_SOURCE:
        // Convert to destination dimension
        cswidth    *= src_width;
        cswidth    /= phys_width;
        csheight   *= src_height;
        csheight   /= phys_height;

        max_width  = src_width;
        max_height = src_height;
        break;

    case ASPECT_RATIO_FLAG_CROP_BY_DESTINATION:
        // Convert to destination dimension
        cswidth    *= dst_width;
        cswidth    /= phys_width;
        csheight   *= dst_height;
        csheight   /= phys_height;

        max_width  = dst_width;
        max_height = dst_height;
        break;

    default:
        LOGE( "Invalid setting for Cropping by...");
        error = -1;
        goto fail;
    }

#ifdef DEBUG_LOG
    LOGE( "Output Parameters:");
    LOGE( "\tCrop        : "
        "Top   = %5d,  Left   = %5d,  Width = %5d,  Height = %5d",
        (int)((max_height-((csheight>max_height)?max_height:csheight))>>1),
        (int)((max_width -((cswidth >max_width )?max_width :cswidth ))>>1),
        (int)((cswidth >max_width )?max_width :cswidth ),
        (int)((csheight>max_height)?max_height:csheight));
#endif

    // Width align
    cswidth   = (cswidth + align_crop_width);
    cswidth  /= (align_crop_width*2);
    cswidth  *= (align_crop_width*2);

    // Height align
    csheight  = (csheight + align_crop_height);
    csheight /= (align_crop_height*2);
    csheight *= (align_crop_height*2);

    *crop_src_width  = (cswidth  > max_width )?max_width:cswidth;
    *crop_src_height = (csheight > max_height)?max_height:csheight;
    *crop_src_left   = (max_width  - *crop_src_width ) >> 1;
    *crop_src_top    = (max_height - *crop_src_height) >> 1;

#ifdef DEBUG_LOG
    LOGE( "\tCrop Align  : "
        "Top   = %5d,  Left   = %5d,  Width = %5d,  Height = %5d",
        (int)*crop_src_top, (int)*crop_src_left,
        (int)*crop_src_width, (int)*crop_src_height);
#endif

    if ((signed)pdwidth == -1) {
        pdwidth = (pdheight * phys_rate) >> 16;
    }

    if ((signed)pdheight == -1) {
        pdheight = (pdwidth << 16) / phys_rate;
    }

#ifdef DEBUG_LOG
    LOGE( "\tPos         : "
        "Top   = %5d,  Left   = %5d,  Width = %5d,  Height = %5d",
        (int)((dst_height-((pdheight>dst_height)?dst_height:pdheight))>>1),
        (int)((dst_width -((pdwidth >dst_width )?dst_width :pdwidth ))>>1),
        (int)((pdwidth  > dst_width )?dst_width :pdwidth ),
        (int)((pdheight > dst_height)?dst_height:pdheight));
#endif

    // Width align
    pdwidth   = (pdwidth + align_pos_width);
    pdwidth  /= (align_pos_width*2);
    pdwidth  *= (align_pos_width*2);

    // Height align
    pdheight  = (pdheight + align_pos_height);
    pdheight /= (align_pos_height*2);
    pdheight *= (align_pos_height*2);

    if (pos_dst_width)
        *pos_dst_width = pdwidth;

    if (pos_dst_height)
        *pos_dst_height = pdheight;

    if (pos_dst_left)
        *pos_dst_left = (dst_width - pdwidth ) >> 1;

    if (pos_dst_top)
        *pos_dst_top = (dst_height - pdheight) >> 1;

#ifdef DEBUG_LOG
    LOGE( "\tPos Align   : "
        "Top   = %5d,  Left   = %5d,  Width = %5d,  Height = %5d",
        (int)((pos_dst_top)?*pos_dst_top:-1),
        (int)((pos_dst_left)?*pos_dst_left:-1),
        (int)((pos_dst_width)?*pos_dst_width:-1),
        (int)((pos_dst_height)?*pos_dst_height:-1));


    LOGE( "Exit ( %d x %d x %d x %d)",
        (int)*crop_src_top, (int)*crop_src_left, (int)*crop_src_width, (int)*crop_src_height);
#endif

fail:
    return error;
}

int scale_process(void* inBuffer, int inWidth, int inHeight, void* outBuffer, int outWidth, int outHeight, int rotation, int fmt, float zoom, int crop_top, int crop_left, int crop_width, int crop_height)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_U32 w,h,zfactor;
    double aspect_ratio;

    GPPToVPPInputFrameStatus    PrevIpFrameStatus;
    GPPToVPPOutputFrameStatus   PrevOpYUVFrameStatus;

    GPPToVPPInputFrameStatus*   pPrevIpFrameStatus = &PrevIpFrameStatus;
    GPPToVPPOutputFrameStatus*  pPrevOpYUVFrameStatus = &PrevOpYUVFrameStatus;

    pPrevIpFrameStatus->ulInWidth             = inWidth;
    pPrevIpFrameStatus->ulInHeight            = inHeight;
    pPrevIpFrameStatus->ulCInOffset           = 0; /* offset of the C frame in the   *
                                                    * buffer (equal to zero if there *
                                                    * is no C frame)                 */
    crop_top = correct_range(crop_top, inHeight);
    crop_left = correct_range(crop_left, inWidth);
    crop_width = correct_range(crop_width, inWidth);
    crop_height = correct_range(crop_height, inHeight);

    /* crop */
    pPrevIpFrameStatus->ulInXstart            = crop_left;
    pPrevIpFrameStatus->ulInXsize             = crop_width;
    pPrevIpFrameStatus->ulInYstart            = crop_top;
    pPrevIpFrameStatus->ulInYsize             = crop_height;

    /* zoom*/
    pPrevIpFrameStatus->ulZoomFactor          = zoom*1024; //(outWidth * 1024) / inWidth;
    pPrevIpFrameStatus->ulZoomLimit           = zoom*1024; //(outWidth * 1024) / inWidth;
    pPrevIpFrameStatus->ulZoomSpeed           = 0;
    
    pPrevIpFrameStatus->ulFrostedGlassOvly    = OMX_FALSE;        
    pPrevIpFrameStatus->ulLightChroma         = OMX_TRUE;          
    pPrevIpFrameStatus->ulLockedRatio         = OMX_FALSE;          
    pPrevIpFrameStatus->ulMirror              = OMX_FALSE;      
    pPrevIpFrameStatus->ulRGBRotation         = 0;    
    pPrevIpFrameStatus->ulYUVRotation         = rotation;
    
    pPrevIpFrameStatus->ulContrastType        = 0;  
    pPrevIpFrameStatus->ulVideoGain           = 64;   /*Video Gain (contrast) in VGPOP ranges from 0 to 127, being 64 = Gain 1 (no contrast)*/
    
    pPrevIpFrameStatus->ulXoffsetFromCenter16 = 0;        
    pPrevIpFrameStatus->ulYoffsetFromCenter16 = 0;        
    pPrevIpFrameStatus->ulOutPitch            = 0;  /*Not supported at OMX level*/
    pPrevIpFrameStatus->ulAlphaRGB            = 0; /*Not supported at OMX level*/
   
    /*Init pComponentPrivate->pPrevOpYUVFrameStatus */
    pPrevOpYUVFrameStatus->ulOutWidth         = outWidth;
    pPrevOpYUVFrameStatus->ulOutHeight        = outHeight;

    if( fmt )
        pPrevOpYUVFrameStatus->ulCOutOffset   = outWidth*outHeight; /*  Offset of the C frame in the buffer */
    else
        pPrevOpYUVFrameStatus->ulCOutOffset   = 0;

    eError = LCML_QueueBuffer(pLCML->pCodecinterfacehandle,
                              EMMCodecInputBuffer,
                              inBuffer,
                              inWidth * inHeight * 2,
                              inWidth * inHeight * 2,
                              (void*)pPrevIpFrameStatus,
                              sizeof(GPPToVPPInputFrameStatus),
                              NULL);
    if (eError != OMX_ErrorNone) {
        LOGV("Camera Component: Error 0x%X While sending the input buffer to Codec\n",eError);
        goto OMX_CAMERA_BAIL_CMD;
    }

	LOGV("222222222222222222222222222222222222222222222222\n");
    eError = LCML_QueueBuffer(pLCML->pCodecinterfacehandle,
                              EMMCodecStream3,
                              outBuffer,
                              outWidth * outHeight * 2,
                              0,
                              (void*)pPrevOpYUVFrameStatus,
                              sizeof(GPPToVPPOutputFrameStatus),  
                              NULL);
    if (eError != OMX_ErrorNone) {
        LOGV("Camera Component: Error 0x%X While sending the output buffer to Codec\n",eError);
        goto OMX_CAMERA_BAIL_CMD;
    }

	LOGV("3333333333333333333333333333333333333333333333333\n");
//    sem_wait( &semResized );

    char ch;
    read(pipeResized[READ_END], &ch, 1);

	LOGV("444444444444444444444444444444444444444444444444444444\n");
    return 0;

    
OMX_CAMERA_BAIL_CMD:
    return -1;
}


OMX_ERRORTYPE ZoomCallback (TUsnCodecEvent event,void * args [10])
{
//    LOGV("Callback event=%d\n",event);

    if( event == EMMCodecBufferProcessed )
    {
        if( (int)args[0] == EMMCodecInputBuffer )
        {
            LOGV("\n\nImage processed.\n\n\n");


//            sem_post( &semResized );
            
            write(pipeResized[WRITE_END], "Q", 1);
            LOGV("\n\nImage processed semaphore posted.\n\n\n");            
        }
    }
    return OMX_ErrorNone;
}

int scale_init(int inWidth, int inHeight, int outWidth, int outHeight, int inFmt, int outFmt)
{
    LCML_CALLBACKTYPE   cb;
    OMX_ERRORTYPE       err;
    LCML_DSP*           pLcmlDsp;
    OMX_U16             array[100];

    char p[32] = "damedesuStr";

    pLCML = GetLCMLHandle();

    if( pLCML == NULL )
    {
        LOGV("Cannot get LCML handle.\n");
        return -1;
    }

    cb.LCML_Callback = ZoomCallback;

    pLcmlDsp = pLCML->dspCodec;

    err = Fill_LCMLInitParams( array, pLcmlDsp, inWidth, inHeight, outWidth, outHeight, inFmt, outFmt);

    err = LCML_InitMMCodec( pLCML->pCodecinterfacehandle, p, &pLCML, (void*)p, &cb );
    if( err != OMX_ErrorNone )
    {
        LOGV("LCML_InitMMCodec error = 0x%08x", err);
        return -1;
    }

    err = LCML_ControlCodec( pLCML->pCodecinterfacehandle, EMMCodecControlStart, "damedesuStr" );
    if( err != OMX_ErrorNone )
    {
        LOGV("LCML_ControlCodec(EMMCodecControlStart) error = 0x%08x\n", err);
        return -1;
    }

//    sem_init( &semResized, 0 , 0 );
    pipe(pipeResized);

    return 0;
}

int scale_deinit()
{
    OMX_ERRORTYPE err;

    err = LCML_ControlCodec( pLCML->pCodecinterfacehandle, MMCodecControlStop, NULL );
    if( err != OMX_ErrorNone )
    {
        LOGV("LCML_ControlCodec(MMCodecControlStop) error=0x%08x\n", err);
        return -1;
    }

    err = LCML_ControlCodec( pLCML->pCodecinterfacehandle, EMMCodecControlDestroy, NULL );
    if( err != OMX_ErrorNone )
    {
        LOGV("LCML_ControlCodec(MMCodecControlStop) error=0x%08x\n", err);
        return -1;
    }

    close(pipeResized[READ_END]);
    close(pipeResized[WRITE_END]);    
    return 0;
}



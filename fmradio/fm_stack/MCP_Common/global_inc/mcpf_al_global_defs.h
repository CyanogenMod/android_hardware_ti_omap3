/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */



#ifndef __MCPF_AL_GLOBAL_DEFS_H__
#define __MCPF_AL_GLOBAL_DEFS_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "mcpf_global_defs.h"

#define GPS_MAX_SVS             32
#define NMEA_STRING_LENGTH      800
#define MAX_LENGTH_MSG 		4096

typedef enum alCmds
{
    E_AL_CMD_NAVC_CREATE = 1,
    E_AL_CMD_NAVC_START,
    E_AL_CMD_GET_FIX,
    E_AL_CMD_STOP_FIX,
    E_AL_CMD_NAVC_STOP,
    E_AL_CMD_NAVC_DESTROY,
    E_AL_CMD_SET_POS_MODE,
    E_AL_CMD_SUPLC_CREATE,

    E_AL_CMD_FROM_NETWORK,

    E_AL_CMD_MAX          = 30,

    E_AL_RES_NAVC_CREATE_COMP,
    E_AL_RES_SUPLC_CREATE_COMP,
    E_AL_RES_NAVC_START_COMP,
    E_AL_RES_NAVC_STOP_COMP,
    E_AL_RES_NAVC_STOP_FIX_COMP,
    E_AL_RES_REPORT_FIX,



}eMCPF_AL_Opcodes;


/* XXX: External applications are expected to know their respective location fix session id. */
typedef struct stopLocFix{
    uInt32  uSessionId;

}TMCPF_AL_StopLocFix;



typedef enum fixMode{
    E_AL_AUTONOMOUS,
    E_AL_MS_BASED,
    E_AL_MS_ASSISTED,

}eMCPF_AL_FixMode;


typedef struct locationFix{
    eMCPF_AL_FixMode    e_locationFixMode;
    sInt32              s32_fixFrequency;

}TMCPF_AL_LocFixReq;

typedef struct setPositionMode{
    eMCPF_AL_FixMode    e_locationFixMode;
    sInt32              s32_fixFrequency;

}TMCPF_AF_setPositionMode;

typedef struct {
    uInt8 a_message[MAX_LENGTH_MSG];
    //uInt8 *p_message;
    uInt32 u32_msgSize;

}TMCPF_AF_msgFromNetwork;

/* For commands from Adaption layer */
typedef union
{
    TMCPF_AL_LocFixReq s_locationFixReq;
    TMCPF_AL_StopLocFix s_stopLocationFix;
    TMCPF_AF_setPositionMode s_setPositionMode;
    TMCPF_AF_msgFromNetwork  s_msgFromNetwork;

}TMCPF_AL_CmdParams;



// Cmd Message Structure
typedef struct
{
    eMCPF_AL_Opcodes    e_opcode;
    TMCPF_AL_CmdParams  u_payload;

}TMCPF_AL_Cmd;






 /* ------ For Reporting to Adaptation layer -----*/

typedef enum
{
    E_AL_REPORT_FIX_NMEA = 1,
    E_AL_REPORT_FIX_PNR_SNR,
    E_AL_REPORT_FIX_RAW_MES

}eMCPF_AL_LocReport_Opcode;






// Structure to report location fix
 typedef struct
 {
    double latitude;
    double longitude;
    double altitude;
    double v_accuracy;
    double h_accuracy;
    float  accuracy;
    float speed_mps;
    float speed_accuracy;

}TMCPF_AL_Raw_Report;

/* Struct to report NMEA Structure */
typedef struct
{
    uInt8 a_nmeaString[NMEA_STRING_LENGTH];

}TMCPF_AL_Nmea_Report;



/* Struct to report SNR and PNR Structure*/

/* Space vehicle's informaton. */
typedef struct{
    int     prn;
    float   snr;
    float   elevation;
    float   azimuth;
}TMCPF_AL_GpsSvInfo;

/* Space vehicle's  status. */
typedef struct{
    TMCPF_AL_GpsSvInfo      sv_list[GPS_MAX_SVS];   /* SVs' information. */

    sInt32                  num_svs;                /* Number of SVs currently visible. */
    uInt32                  ephemeris_mask;         /* Bit mask indicating which SV has
                                                       ephemeris data. */
    uInt32    almanac_mask;                         /* Bit mask indicating which SV has
                                                       almanac data. */
    uInt32    used_in_fix_mask;                     /* Bit mask indicating which SV was used for
                                                       computing the most recent position fix. */
}TMCPF_AL_SvInfo_Report;




typedef union
{

    TMCPF_AL_Raw_Report         s_rawReport;
    TMCPF_AL_Nmea_Report        s_nmeaReport;
    TMCPF_AL_SvInfo_Report      s_svInfoReport;
}TMCPF_AL_LocReport_Params;

typedef struct
{
    eMCPF_AL_LocReport_Opcode   e_locationFixOpcode;
    TMCPF_AL_LocReport_Params   u_locReport_payload;

}TMCPF_AL_FixReport;

/* For commands from Adaption layer */
typedef union
{
    TMCPF_AL_FixReport  s_locationFixReport;
    sInt8               s8_result;

}TMCPF_AL_ResponseParams;

/* Report Message Structure */
typedef struct
{
    eMCPF_AL_Opcodes            e_opcode;
    TMCPF_AL_ResponseParams     u_payload;

}TMCPF_AL_Response;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __MCPF_AL_GLOBAL_DEFS_H__ */

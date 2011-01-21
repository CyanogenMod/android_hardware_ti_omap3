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


/** Include Files **/
#include <stdarg.h>
#include <stdio.h>
#include "string.h"
#include "pla_cmdParse.h"


/************************************************************************/
/*                           Definitions                                */
/************************************************************************/

//for now, define REPORT as printf
#ifndef Report
#define Report(x) printf x
#endif

#define PATHCONFIGFILE "-c/system/bin/pathconfigfile.txt"


/************************************************************************/
/*                          Global Variables                            */
/************************************************************************/


/************************************************************************/
/*                  Internal Function Declaration                       */
/************************************************************************/
static void getParamString(char *paramName, char *buf);


/************************************************************************/
/*                  External Function Declaration                       */
/************************************************************************/
extern void MCP_HAL_LOG_EnableUdpLogging(const char* ip, unsigned long port);
extern void MCP_HAL_LOG_EnableFileLogging(const char* fileName);
extern void MCP_HAL_LOG_EnableStdoutLogging(void);
extern void MCP_HAL_LOG_EnableLogToAndroid(const char *app_name);



/************************************************************************/
/*                              APIs                                    */
/************************************************************************/
/**
 * \fn     pla_cmdParse_GeneralCmdLine
 * \brief  Parses the initial command line
 *
 * This function parses the initial command line,
 * and returns the BT & NAVC strings,
 * along with the common parameter.
 *
 * \note
 * \param   CmdLine     - Received command Line.
 * \param   **bt_str - returned BT cmd line.
 * \param   **nav_str - returned NAVC cmd line.
 * \return  Port Number
 * \sa      pla_cmdParse_GeneralCmdLine
 */
McpU32 pla_cmdParse_GeneralCmdLine(LPSTR CmdLine, char **bt_str, char **nav_str)
{
    /* Currently, in Linux the serial port name is hardcoded in bthal_config.h.
     * So, put some number in order to prevent assert in a caller function */
    McpU32      uPortNum = -1;
    
    char        *pParam;
    char        *paramParse;
    char        pathfile[256] = "";
    char        logName[30] = "";
    char        logFile[256] = "";
    char        logIp[30] = "";
    char        portStr[30] = "";
    unsigned long port = 0;
    char        *pPort;
    int         i=0;

    MCPF_UNUSED_PARAMETER(bt_str);
    
	pPort = strstr(CmdLine, "-p");
	if(pPort != NULL)
	{
		uPortNum = atoi(pPort+2);
		sprintf((*nav_str), "%s%d", "-p", uPortNum); 
	}

    
    /*Issue Fix -nav*/
    if (0 != (pParam = strstr((const char *)CmdLine, "-nav")))
    {
	
	paramParse = pParam;   
	paramParse += 5;	 
	
	strcat (*nav_str, " ");

	while(*paramParse != '"')
	{
		
              pathfile[i] = *paramParse;
		paramParse++;
		i++;
       }
       strcat (*nav_str, pathfile);

	   		MCP_HAL_LOG_INFO(__FILE__,
                                      __LINE__,
                                      MCP_HAL_LOG_MODULE_TYPE_MCP_MAIN,
                                      ("pla_cmdParse_GeneralCmdLine111: NAV Cmd line \"%s\"", *nav_str));
    }
    else
    {
	strcat (*nav_str, " ");
	strcat (*nav_str, PATHCONFIGFILE);
    }

#ifdef ANDROID
    if (0 != (pParam = strstr((const char *)CmdLine, "--android_log")))
    {
        getParamString(pParam, logName);
        MCP_HAL_LOG_EnableLogToAndroid((const char *)logName);
    }
#endif
    if (0 != (pParam = strstr((const char *)CmdLine, "-logfile")))
    {
        getParamString(pParam, logFile);
        MCP_HAL_LOG_EnableFileLogging((const char *)logFile);
    }
    else if (0 != strstr((const char *)CmdLine, "--log_to_stdout"))
    {
        MCP_HAL_LOG_EnableStdoutLogging();
    }
    else if (0 != (pParam = strstr((const char *)CmdLine, "-log_ip")))
    {
        getParamString(pParam, logIp);
        
        if (0 != (pParam = strstr((const char *)CmdLine, "-log_port")))
        {
            getParamString(pParam, portStr);
            port = atoi((const char *)portStr);
        }
        if (strlen(logIp)>0 && port>0)
        {
            MCP_HAL_LOG_EnableUdpLogging(logIp, port);
            MCP_HAL_LOG_INFO(__FILE__,
                             __LINE__,
                             MCP_HAL_LOG_MODULE_TYPE_MCP_MAIN,
                             ("pla_cmdParse_GeneralCmdLine: UDP logging, IP %s, port %d",
                              logIp, port));
        }
    }
    else if (0 != strstr((const char *)CmdLine, "--help"))
    {
        printf ("btipsd usage :\nbtipsd [options]\n-h, --help : Show this screen\n-r, --run_as_daemon : Run the mcpd as daemon in the background\n-l, --logfile [LOGFILENAME] : Log to specified file\n-d, --log_to_stdout : Log to STDOUT\n-log_ip [IP_ADDRESS] : Target UDP Listener IP for logging\n-log_port [PORT] : Target UDP Listener PORT for logging\n");
#ifdef ANDROID
        printf ("-no_android_log : Disable Android logging\n");
#endif
        return 0;
    }
    
    MCP_HAL_LOG_INFO(__FILE__,
                                      __LINE__,
                                      MCP_HAL_LOG_MODULE_TYPE_MCP_MAIN,
                                      ("pla_cmdParse_GeneralCmdLine: cmd line \"%s\"", CmdLine));
    
    return uPortNum;
  
}


/**
 * \fn     pla_cmdParse_NavcCmdLine
 * \brief  Parses the NAVC command line
 *
 * This function parses the NAVC command line.
 *
 * \note
 * \param   *nav_str - NAVC cmd line.
 * \param   *tCmdLineParams - Params to fill.
 * \return  None
 * \sa      pla_cmdParse_NavcCmdLine
 */
void pla_cmdParse_NavcCmdLine(char *nav_str, TcmdLineParams *tCmdLineParams)
{
    char    *temp;
    char    *pHyphen;
    char    *pSpace;
    char *pComma;
    char *pPort;
    McpU16  i = 0;


    /* Parse params */
    temp = nav_str;
    while(temp)
    {
        pHyphen = strstr(temp, "-");
        pSpace = strstr(temp, " ");
        if(pSpace != NULL)
        {
            temp = pSpace + 1;
        }
        else
        {
            temp = pSpace;
        }

        if( (strncmp( pHyphen, "-p", 2 ) == 0) &&   (sscanf( pHyphen+2, "%d", &tCmdLineParams->x_SensorPcPort) == 1) )
        {
            continue;
        }

        if( (strncmp( pHyphen, "-l", 2 ) == 0) &&  (sscanf( pHyphen+2, "%d", &tCmdLineParams->x_NmeaPcPort) == 1) )
        {
             continue;
        }

        if(strncmp( pHyphen, "-ads", 4 ) == 0)
        {
            pPort = pHyphen+4;
            pComma = strstr(pPort, ",");
            while (pComma)
            {
                sscanf( pPort, "%d", &tCmdLineParams->x_AdsPort[i]);
                i++;
                pPort = pComma + 1;
                pComma = strstr(pPort, ",");
            }
            sscanf( pPort, "%d", &tCmdLineParams->x_AdsPort[i]);
            i++;
            tCmdLineParams->x_AdsPortsCounter = i;
            continue;
        }

        if( (strncmp( pHyphen, "-b", 2 ) == 0) )
        {
            /* To Restrict the Almanac data injection during 3GPP test*/
            sscanf(pHyphen+2, "%x", (McpU32 *) &tCmdLineParams->w_AlmanacDataBlockFlag);
            continue;
        }

	   if( (strncmp( pHyphen, "-c", 2 ) == 0) )
			{
				/* Path Control File */
				sscanf(pHyphen+2, "%s", &tCmdLineParams->s_PathCtrlFile);

				continue;
			}
	}
}

static void getParamString(char *paramName, char *buf)
{
    char *param = strchr((const char *)paramName, ' ');
    
    /* Skip spaces */
    while (*param == ' ')
    {
        param++;
    }

    /* Copy parameter string to output buffer */
    while (*param != ' ' && *param != '\0')
    {
        *buf++ = *param++;
    }
    *buf = '\0';
}


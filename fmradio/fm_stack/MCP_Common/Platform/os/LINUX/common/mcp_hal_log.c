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

/*******************************************************************************\
*
*   FILE NAME:      mcp_hal_log.c
*
*   DESCRIPTION:    This file implements the API of the MCP HAL log utilities.
*
*   AUTHOR:         Chen Ganir
*
\*******************************************************************************/


/********************************************************************************
 *
 * Include files
 *
 *******************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>


#include "mcp_hal_types.h"
#include "mcp_hal_log.h"
#include "mcp_hal_log_udp.h"
#include "mcp_hal_config.h"
#include "mcp_hal_string.h"
#include "mcp_hal_memory.h"
#include "mcp_defs.h"
#include "mcpf_defs.h"

#define FILE_LOG_START          "\n********** BEGIN **********\n"
#define FILE_LOG_END            "\n********* FILE WAS REOPENED **********\n"
#define FILE_LOG_REOPEN	        "\n********* REOPENING FILE **********\n"

#ifdef ANDROID
#include "cutils/log.h"
#undef LOG_TAG
#define LOG_TAG gMcpLogAppName
#endif

void MCP_HAL_InitUdpSockets(void);
static McpU8 readConfFile(void);

/****************************************************************************
 *
 * Constants
 *
 ****************************************************************************/

#define MCP_HAL_MAX_FORMATTED_MSG_LEN           (200)
#define MCP_HAL_MAX_USER_MSG_LEN                (100)
#define BUFFSIZE 255
#define APP_NAME_MAX_LEN			(50)
#define MCP_HAL_MAX_TAGS			(5)
#define MCP_HAL_MAX_TAG_LENGTH	(4)

#define GPS_CONFIG_FILE_PATH "/system/bin/GpsConfigFile.txt"
#define RDONLY  "r"
#define MAX_BUF_LENGTH 128
#define LOGGER_IP  "logger_ip"
#define LOGGER_PORT "logger_port"

static char _mcpLog_FormattedMsg[MCP_HAL_MAX_FORMATTED_MSG_LEN + 1];

McpU8 gMcpLogEnabled = 0;
McpU8 gMcpLogToStdout = 0;
McpU8 gMcpLogToFile = 0;
McpU8 gMcpLogToUdpSocket = 0;

mcp_log_socket_t  gMcpSocket[2];

#ifdef ANDROID
McpU8 gMcpLogToAndroid = 0;
char gMcpLogAppName[APP_NAME_MAX_LEN] = {0};
#endif

char gMcpTagString[MCP_HAL_MAX_TAGS][MCP_HAL_MAX_TAG_LENGTH] = {MCP_HAL_LOG_INTERNAL_TAG, MCP_HAL_LOG_TOSENSOR_TAG, MCP_HAL_LOG_FROMSENSOR_TAG,
																    MCP_HAL_LOG_NULL_TAG, MCP_HAL_LOG_INTERNAL_TAG} ;


char gMcpLogUdpTargetAddress[30] = "";
unsigned long gMcpLogUdpTargetPort = MCP_LOG_PORT_DEFAULT;
char gMcpLogFileName[512] = "";
int g_udp_sock = -1;

time_t g_start_time_seconds = 0;

struct sockaddr_in g_udp_logserver;

static pthread_key_t thread_id;

static McpU8 gInitialized = 0;

FILE* log_output_handle = NULL;

/* this key will hold the thread-specific thread handle that will be used
 * for self identification of threads. the goal is that every thread will know its name
 * to ease having a consistent self identifying logs */
static pthread_key_t thread_name;

#ifdef ANDROID
void MCP_HAL_LOG_EnableLogToAndroid(const char *app_name)
{
    gMcpLogToAndroid = 1;
    gMcpLogEnabled = 1;
    strncpy(gMcpLogAppName,app_name,APP_NAME_MAX_LEN-1);
}
#endif

void MCP_HAL_LOG_EnableUdpLogging(const char* ip, unsigned long port)
{
    gMcpLogEnabled = 1;
    strcpy(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetAddress,ip);
    gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetPort = port;
    gMcpLogToUdpSocket = 1;
    MCP_HAL_InitUdpSockets();
}

void MCP_HAL_LOG_Set_Mode(McpU8 udpMode)
{

    if (0 != udpMode)
    {
   	 gMcpLogEnabled = 1;
    }
    else
    {
   	 gMcpLogEnabled = 0;
    }

    if (udpMode <= MCP_LOG_MODE_TAG)
       gMcpLogToUdpSocket = udpMode;
    else
	gMcpLogToUdpSocket = 1;

    LOGD("MCP_HAL_LOG_Set_Mode: %d", gMcpLogToUdpSocket);
}


void MCP_HAL_LOG_EnableFileLogging(const char* fileName)
{
#if LOG_FILE_MAX_SIZE > 0
    char prevFileName[256];
#endif /* LOG_FILE_MAX_SIZE > 0 */
	
    gMcpLogEnabled = 1;
    strcpy(gMcpLogFileName,fileName); 
    gMcpLogToStdout = 0; 
    gMcpLogToFile = 1; 

#if LOG_FILE_MAX_SIZE > 0
    /* rename the current file to .prev */
    strcpy(prevFileName, fileName);
    strcpy(prevFileName+strlen(fileName), ".prev");
    remove(prevFileName);
    rename(fileName, prevFileName);

    /* open the new file for writing */
    log_output_handle = fopen(gMcpLogFileName,"wt+");
#else
    /* open the new file, appending at end of file */
    log_output_handle = fopen(gMcpLogFileName,"at");
#endif /* LOG_FILE_MAX_SIZE > 0 */
    if (log_output_handle == NULL)
    {
        fprintf(stderr, "MCP_HAL_LOG_LogMsg failed to open '%s' for writing",gMcpLogFileName);
        gMcpLogEnabled = 0;
        return;
    }    

#if LOG_FILE_MAX_SIZE > 0
    fwrite(FILE_LOG_START, 1, MCP_HAL_STRING_StrLen(FILE_LOG_START), log_output_handle);
#endif /* LOG_FILE_MAX_SIZE > 0 */
}

void MCP_HAL_LOG_EnableStdoutLogging(void)
{
    gMcpLogEnabled = 1;
    gMcpLogToStdout = 1; 
    gMcpLogToFile = 0; 
    strcpy(gMcpLogFileName,(char*)"");
    log_output_handle = stdout;
}

static unsigned int MCP_HAL_LOG_GetThreadId(void)
{
    #ifndef SYS_gettid      //should be normally defined on standard Linux
    #define SYS_gettid 224 //224 on android
    #endif

    return syscall(SYS_gettid);
}

static void MCP_HAL_LOG_SetThreadIdName(unsigned int id, const char *name)
{
    int rc;
    rc = pthread_setspecific(thread_id, (void *)id);
    if (0 != rc)
        fprintf(stderr, "MCP_HAL_LOG_SetThreadIdName | pthread_setspecific() (id) failed: %s", strerror(rc));

    rc = pthread_setspecific(thread_name, name);
    if (0 != rc)
        fprintf(stderr, "MCP_HAL_LOG_SetThreadIdName | pthread_setspecific() (handle) failed: %s", strerror(rc));
}

static char *MCP_HAL_LOG_GetThreadName(void)
{
    return pthread_getspecific(thread_name);
}

void MCP_HAL_LOG_SetThreadName(const char* name)
{
    MCP_HAL_LOG_SetThreadIdName(MCP_HAL_LOG_GetThreadId(), name);
}

/*-------------------------------------------------------------------------------
 * MCP_HAL_LOG_FormatMsg()
 *
 *      sprintf-like string formatting. the formatted string is allocated by the function.
 *
 * Type:
 *      Synchronous, non-reentrant 
 *
 * Parameters:
 *
 *      format [in]- format string
 *
 *      ...     [in]- additional format arguments
 *
 * Returns:
 *     Returns pointer to the formatted string
 *
 */
char *MCP_HAL_LOG_FormatMsg(const char *format, ...)
{
    va_list     args;

    _mcpLog_FormattedMsg[MCP_HAL_MAX_FORMATTED_MSG_LEN] = '\0';

    va_start(args, format);

    vsnprintf(_mcpLog_FormattedMsg, MCP_HAL_MAX_FORMATTED_MSG_LEN, format, args);

    va_end(args);

    return _mcpLog_FormattedMsg;
}

void MCP_HAL_InitUdpSockets(void)
{
   char address[30];
    /* Initialize the socket */
    if (gMcpLogToUdpSocket >= 1 &&  gMcpSocket[MCP_LOG_NORMAL_INDEX].udp_sock < 0 )
    {
        /* Create the UDP socket */
        if ((gMcpSocket[MCP_LOG_NORMAL_INDEX].udp_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) >= 0)
        {
            /* Construct the server sockaddr_in structure */
            memset(&gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr, 0, sizeof(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr)); /* Clear struct */
            gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr.sin_family = AF_INET; /* Internet/IP */
            gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr.sin_port = htons(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetPort); /* server port */

            if (inet_aton(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetAddress, &gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr.sin_addr)==0)
            {
				LOGD(" Looger: UDP Socket Create Fails for Index: %d", MCP_LOG_NORMAL_INDEX);
			}
            LOGD(" Looger: UDP Socket Create Success for Ind: %d", MCP_LOG_NORMAL_INDEX);
        }

	LOGD("InitUdp: Normal IP: %s, port: %d\n",inet_ntoa(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr.sin_addr),gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetPort );

    } else
	{
            LOGD(" MCP_HAL_InitUdpSockets: NORMAL Ind fail sock: %d \n", gMcpSocket[MCP_LOG_NORMAL_INDEX].udp_sock );
	}

    /*Initialize UDP for re-routing log to the local server*/
    if (gMcpLogToUdpSocket >= 1 &&  gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].udp_sock < 0 )
    {
        /* Create the UDP socket */
        if ((gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].udp_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) >= 0)
        {
            /* Construct the server sockaddr_in structure */
            memset(&gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr, 0, sizeof(gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr)); /* Clear struct */
            gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr.sin_family = AF_INET; /* Internet/IP */
            gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr.sin_port = htons(gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpLogUdpTargetPort); /* server port */

            if (inet_aton(gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpLogUdpTargetAddress, &gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr.sin_addr)==0)
            {
				LOGD(" Looger: UDP Socket Create Fails for Index: %d", MCP_LOG_RE_ROUTE_INDEX);
			}
            LOGD(" Looger: UDP Socket Create Success for Ind: %d", MCP_LOG_RE_ROUTE_INDEX);
        }
	LOGD("InitUdp: Re_Route IP: %s, port: %d\n", inet_ntoa(gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr.sin_addr),gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpLogUdpTargetPort );
    }else
	{
            LOGD(" MCP_HAL_InitUdpSockets: RE_ROUTE Ind fail sock: %d \n", gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].udp_sock );
	}

}


void MCP_HAL_DeInitUdpSockets(void)
{
    McpU8 u_i;
    /* Deinitialize the socket */

   for(u_i=0;u_i<MCP_LOG_MAX_INDEX;u_i++)
   {
   	 if (gMcpLogToUdpSocket >= 1 && gMcpSocket[u_i].udp_sock >=0 )
    	{
       	 close(gMcpSocket[u_i].udp_sock);
        	gMcpSocket[u_i].udp_sock = -1;
    	}
   }

}

void MCP_HAL_LOG_Init(void)
{
    McpU8 u_i;

   /*Initialize udp data*/

   for(u_i=0;u_i<MCP_LOG_MAX_INDEX;u_i++)
   {
   	gMcpSocket[u_i].udp_sock = -1;
	strcpy(gMcpSocket[u_i].mcpLogUdpTargetAddress, " "); /*Initialize to localhost*/
   	gMcpSocket[u_i].mcpLogUdpTargetPort = MCP_LOG_PORT_DEFAULT;
   }

   strcpy(gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpLogUdpTargetAddress, "127.0.0.1"); /*Initialize RE_ROUTE_INDEX to localhost*/

#ifdef CLIENT_CUSTOM
   strcpy(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetAddress, "192.168.16.1"); /*Initialize NORMAL_INDEX to adb bridge*/
#endif

#if 0 /*Need to be removed once tested control file path*/
   if (readConfFile() != RES_OK)
   {
	 LOGD("mcp_hal_log.c: ERROR opening config file GpsConfigFile.txt");
   }
#endif

   MCP_HAL_LOG_EnableLogToAndroid(gMcpLogAppName);
   
   /*Moved UDP LOG Enable to gpsc_app_api.c after parsing the path control file to get right IP/Port*/
   //MCP_HAL_LOG_EnableUdpLogging(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetAddress , gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetPort);
   
   if (gInitialized == 0)
    {
		gInitialized = 1;
	}

}

void MCP_HAL_LOG_Deinit(void)
{
    int rc;

    MCP_HAL_DeInitUdpSockets();

    rc = pthread_key_delete(thread_id);
    if (0 != rc)
        fprintf(stderr, "MCP_HAL_LogDeinit | pthread_key_delete() failed: %s", strerror(rc));

    rc = pthread_key_delete(thread_name);
    if (0 != rc)
        fprintf(stderr, "MCP_HAL_LogDeinit | pthread_key_delete() failed: %s", strerror(rc));
}

const char *MCP_HAL_LOG_SeverityCodeToName(McpHalLogSeverity Severity)
{
   switch(Severity)
    {       
        case (MCP_HAL_LOG_SEVERITY_FUNCTION):
            return ("FUNCT");
        case (MCP_HAL_LOG_SEVERITY_DEBUG):
            return ("DEBUG");
        case (MCP_HAL_LOG_SEVERITY_INFO):
            return ("INFO");
        case (MCP_HAL_LOG_SEVERITY_ERROR):
            return ("ERROR");
        case (MCP_HAL_LOG_SEVERITY_FATAL):
            return ("FATAL");
        default:
            return (" ");
    }
    return (" ");
}


static void MCP_HAL_LOG_LogToFile(const char*       fileName, 
                           McpU32           line, 
                           McpHalLogModuleId_e moduleId,
                           McpHalLogSeverity severity,  
                           const char*      msg)
{
    struct timeval detail_time;
    char copy_of_msg[MCP_HAL_MAX_USER_MSG_LEN+1] = "";
    char log_formatted_str[MCP_HAL_MAX_FORMATTED_MSG_LEN+1] = "";
    size_t copy_of_msg_len = 0;
    char* moduleName = NULL;
    static int file_size = 0;
#if LOG_FILE_MAX_SIZE > 0
    FILE* fw;
    char prevFileName[256];
#endif /* LOG_FILE_MAX_SIZE > 0 */

    /* Remove any CR at the end of the user message */
    strncpy(copy_of_msg,msg,MCP_HAL_MAX_USER_MSG_LEN);
    copy_of_msg_len = strlen(copy_of_msg);
    if (copy_of_msg_len>0)
    	{
	    if (copy_of_msg[copy_of_msg_len-1] == '\n')
	        copy_of_msg[copy_of_msg_len-1] = ' ';
    	}

    /* Get the thread name */
    char *threadName = MCP_HAL_LOG_GetThreadName();
    
    /* Query for current time */
    gettimeofday(&detail_time,NULL);

    /* Get the module name */
    moduleName = MCP_HAL_LOG_Modules[moduleId].name;

    /* Format the final log message to be printed */
    snprintf(log_formatted_str,MCP_HAL_MAX_FORMATTED_MSG_LEN,
              "%06ld.%06ld|%-5s|%-15s|%s|%s {%s@%ld}\n",
              detail_time.tv_sec - g_start_time_seconds,
              detail_time.tv_usec,
              MCP_HAL_LOG_SeverityCodeToName(severity),
              moduleName,
              (threadName != NULL ? threadName: "UNKNOWN"),
              copy_of_msg,
              (fileName==NULL?"UNKOWN":fileName),
              line);

#if LOG_FILE_MAX_SIZE > 0
    if (file_size > LOG_FILE_MAX_SIZE)
    {
        fwrite(FILE_LOG_END, 1, MCP_HAL_STRING_StrLen(FILE_LOG_END), log_output_handle);

        /* rename the current file to .prev */
        strcpy(prevFileName, gMcpLogFileName);
        strcpy(prevFileName+strlen(gMcpLogFileName), ".prev");
        remove(prevFileName);
        rename(fileName, prevFileName);

        fw = log_output_handle;
        log_output_handle = freopen(gMcpLogFileName, "r+", fw);
        file_size = fwrite(FILE_LOG_REOPEN, 1, MCP_HAL_STRING_StrLen(FILE_LOG_REOPEN), log_output_handle);
    }
#endif /* LOG_FILE_MAX_SIZE */

    /* Write the formatted final message to the file */
    file_size += fwrite(log_formatted_str, 1, MCP_HAL_STRING_StrLen(log_formatted_str), log_output_handle);
    fflush(log_output_handle);
}

static void MCP_HAL_LOG_LogToUdp(const char*        fileName,
                          McpU32            line,
                          McpHalLogModuleId_e moduleId,
                          McpHalLogSeverity severity,
                          const char*       msg,
                          McpU8 logTagId)
{
    udp_log_msg_t udp_msg;
    char *threadName = MCP_HAL_LOG_GetThreadName();
    size_t logStrLen = 0;
   McpU8 tLen =0;

    /* Reset log message */
    memset(&udp_msg,0,sizeof(udp_log_msg_t));

    /* Copy user message */
    if (logTagId == MCP_HAL_LOG_NULL_TAG_ID)
    {
        strncpy(udp_msg.message,msg,MCPHAL_LOG_MAX_MESSAGE_LENGTH-1);
    }
    else if ( logTagId <  MCP_HAL_LOG_NULL_TAG_ID)
    {
       tLen = strlen(gMcpTagString[logTagId]);

	if (tLen <= MCP_HAL_MAX_TAG_LENGTH)
   	    strncpy(udp_msg.message, gMcpTagString[logTagId], tLen);
	else
		tLen = 0;

	strncat(udp_msg.message+tLen, msg, MCPHAL_LOG_MAX_MESSAGE_LENGTH-(tLen+1));
    }
    else
    {
   	  strncpy(udp_msg.message, MCP_HAL_LOG_ERROR_STRING, strlen(MCP_HAL_LOG_ERROR_STRING));
    }

	logStrLen = strlen(udp_msg.message);
    if (udp_msg.message[logStrLen-1] == '\n')
        udp_msg.message[logStrLen-1] = ' ';

    /* Copy file name */
    strncpy(udp_msg.fileName,(fileName==NULL ? "MAIN":fileName),MCPHAL_LOG_MAX_FILENAME_LENGTH-1);

    /* Copy thread name */
    strncpy(udp_msg.threadName,(threadName!=NULL ? threadName :"UNKNOWN"),MCPHAL_LOG_MAX_THREADNAME_LENGTH-1);

    /* Copy other relevant log information */
    udp_msg.line = line;
    udp_msg.moduleId = moduleId;
    udp_msg.severity = severity;

    /* Write log message to socket */
   if(MCP_HAL_LOG_NULL_TAG_ID == logTagId)
   {
    	(void)sendto(gMcpSocket[MCP_LOG_NORMAL_INDEX].udp_sock, &udp_msg, sizeof(udp_msg), MSG_DONTWAIT,(struct sockaddr *) &gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr,sizeof(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr));
   }
   else
   {
    	(void)sendto(gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].udp_sock, &udp_msg, sizeof(udp_msg), MSG_DONTWAIT,(struct sockaddr *) &gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr,sizeof(gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr));
   }
}

    

#ifdef ANDROID    

static void MCP_HAL_LOG_LogToAndroid(const char*        fileName, 
                          McpU32            line, 
                          McpHalLogModuleId_e moduleId,
                          McpHalLogSeverity severity,  
                          const char*       msg)
{
    char copy_of_msg[MCP_HAL_MAX_USER_MSG_LEN+1] = "";
    size_t copy_of_msg_len = 0;
    char *threadName = MCP_HAL_LOG_GetThreadName();

    strncpy(copy_of_msg,msg,MCP_HAL_MAX_USER_MSG_LEN);
    copy_of_msg_len = strlen(copy_of_msg);
    if (copy_of_msg_len >0)
    	{
	    if (copy_of_msg[copy_of_msg_len-1] == '\n')
	        copy_of_msg[copy_of_msg_len-1] = ' ';
    	}
    
    switch (severity)
    {
        case MCP_HAL_LOG_SEVERITY_FUNCTION:
            LOGV("%s(%s):%s (%ld@%s)",MCP_HAL_LOG_Modules[moduleId].name,
                threadName,
                copy_of_msg,
                line,
                fileName);
            break;
        case MCP_HAL_LOG_SEVERITY_DEBUG:   
            LOGD("%s(%s):%s (%ld@%s)",MCP_HAL_LOG_Modules[moduleId].name,
                threadName,
                copy_of_msg,
                line,
                fileName);
            break;
        case MCP_HAL_LOG_SEVERITY_INFO:   
            LOGI("%s(%s):%s (%ld@%s)",MCP_HAL_LOG_Modules[moduleId].name,
                threadName,
                copy_of_msg,
                line,
                fileName);
            break;
        case MCP_HAL_LOG_SEVERITY_ERROR:  
            LOGE("%s(%s):%s (%ld@%s)",MCP_HAL_LOG_Modules[moduleId].name,
                MCP_HAL_LOG_GetThreadName(),
                copy_of_msg,
                line,
                fileName);
            break;
        case MCP_HAL_LOG_SEVERITY_FATAL:
            LOGE("%s(%s):%s (%ld@%s)",MCP_HAL_LOG_Modules[moduleId].name,
                threadName,
                copy_of_msg,
                line,
                fileName);
            break;
        default:
            break;
    }
}
#endif

/*-------------------------------------------------------------------------------
 * MCP_HAL_LOG_LogMsg()
 *
 *      Sends a log message to the local logging tool.
 *      Not all parameters are necessarily supported in all platforms.
 *     
 * Type:
 *      Synchronous
 *
 * Parameters:
 *
 *      fileName [in] - name of file originating the message
 *
 *      line [in] - line number in the file
 *
 *      moduleType [in] - e.g. "BTL_BMG", "BTL_SPP", "BTL_OPP", 
 *
 *      severity [in] - debug, error...
 *
 *      msg [in] - message in already formatted string
 *
 *  Returns:
 *      void
 * 
 */
void MCP_HAL_LOG_LogMsg(    const char*     fileName, 
                                McpU32          line, 
                                McpHalLogModuleId_e moduleId,
                                McpHalLogSeverity severity,  
                                const char*         msg)
{
	if (gMcpLogEnabled && gInitialized == 1)
    {

        /* Log to UDP (textual) */
        if ( MCP_LOG_MODE_NORMAL == gMcpLogToUdpSocket )
        {
            MCP_HAL_LOG_LogToUdp(fileName,line,moduleId,severity,msg, MCP_HAL_LOG_NULL_TAG_ID);
        }
		else if ( MCP_LOG_MODE_TAG == gMcpLogToUdpSocket )
		{
			 MCP_HAL_LOG_LogToUdp(fileName,line,moduleId,severity,msg, MCP_HAL_LOG_INTERNAL_TAG_ID);
		}
		/*else          Should be fixed by GPS team.
		{
		   LOGD("Wrong Log Mode: %d\n", gMcpLogToUdpSocket);
		}*/

           /* Log to Logcat */
        if ( 1 == gMcpLogToAndroid )
            MCP_HAL_LOG_LogToAndroid(fileName,line,moduleId,severity,msg);

	} 
	else
	{
		LOGD("Log Not Enabled: gMcpLogEnabled: %d, gInitialized: %d\n", gMcpLogEnabled,gInitialized);
	}

}

void MCP_HAL_LOG_LogTagMsg(    const char*     fileName,
                                McpU32          line,
                                McpHalLogModuleId_e moduleId,
                                McpHalLogSeverity severity,
                                const char*         msg,
                                McpU8			tagId)
{
	if ( tagId <= MCP_HAL_LOG_NULL_TAG_ID )
	   MCP_HAL_LOG_LogToUdp(fileName,line,moduleId,severity,msg, tagId);

} /*MCP_HAL_LOG_LogTagMsg*/


/*-------------------------------------------------------------------------------
 * DumpBuf()
 *
 *		Sends a log message to the local logging tool.
 *		Not all parameters are necessarily supported in all platforms.
 *     
 * Type:
 *		Synchronous
 *
 * Parameters:
 *
 *		fileName [in] - name of file originating the message
 *
 *		line [in] - line number in the file
 *
 *		severity [in] - debug, error...
 *
 *		pTitle [in] - title, already formatted string, limited to 40 bytes length
 *
 *		pBuf [in] - pointer to buffer to dump contents
 *
 *		len [in]  - buffer size to dump
 * 	Returns:
 *		void
 *
 */
void MCP_HAL_LOG_DumpBuf (const char 		*fileName,
						 McpU32				line,
						 McpHalLogSeverity 	severity,
						 const char			*pTitle,
						 const McpU8		*pBuf,
						 const McpU32		len)
{
#ifdef TI_DBG
#define D_LINE_SIZE	64

	McpU32 i,j;
	char  * pOut, outBuf[(D_LINE_SIZE * 3)+50];

	pOut = outBuf;

	for (i=0; i<len && pOut<(outBuf + sizeof(outBuf)); i += D_LINE_SIZE)
	{
		pOut += sprintf(pOut, "%s %u:", pTitle, i);
		for (j=0; j<D_LINE_SIZE && len>i+j && (pOut<(outBuf + sizeof(outBuf))); j++  )
		{
			pOut += sprintf(pOut, " %02x", *(pBuf+i+j));
		}
		LOGD("DUMPBUF: %s", outBuf);
		pOut = outBuf;
	}
#else
    MCPF_UNUSED_PARAMETER(fileName);
    MCPF_UNUSED_PARAMETER(line);
    MCPF_UNUSED_PARAMETER(severity);
#endif
}



void MCP_HAL_LOG_LogBinBuf( const McpU8	logMode,
									McpU8	tagId,
						 			const McpU8	*pBuf,
						 			const McpU32	len)
{
    udp_log_msg_t udp_msg;
	
    char pTitle[50] = "ToSensor/FromSensor";
	
#define D_LINE_SIZE	32

	McpU32 i,j;
	char  * pOut, outBuf[(D_LINE_SIZE * 3)+50];

	pOut = outBuf;

	strcpy(udp_msg.fileName, pTitle);
	//strcpy(udp_msg.moduleName, pTitle);
	udp_msg.line = 111;
	udp_msg.severity = 2;

	if(tagId > MCP_HAL_LOG_NULL_TAG_ID)
		tagId = MCP_HAL_LOG_NULL_TAG_ID;

	memset(&udp_msg.message, 0, sizeof(udp_msg.message));
	memset(&outBuf, 0, sizeof(outBuf));

	for (i=0; i<len && pOut<(outBuf + sizeof(outBuf)); i += D_LINE_SIZE)
	{
		pOut += sprintf(pOut, "%s %u:", gMcpTagString[tagId], i);
		for (j=0; j<D_LINE_SIZE && len>i+j && (pOut<(outBuf + sizeof(outBuf))); j++  )
		{
			pOut += sprintf(pOut, " %02x", *(pBuf+i+j));
		}


		memcpy(&udp_msg.message, &outBuf, strlen(outBuf));
		if (MCP_LOG_MODE_TAG == logMode)
		{
	    		(void)sendto(gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].udp_sock, &udp_msg, sizeof(udp_msg), 0,(struct sockaddr *) &gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr,sizeof(gMcpSocket[MCP_LOG_RE_ROUTE_INDEX].mcpSockAddr));
		}
		else
		{
		    	(void)sendto(gMcpSocket[MCP_LOG_NORMAL_INDEX].udp_sock, &udp_msg, sizeof(udp_msg), 0,(struct sockaddr *) &gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr,sizeof(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpSockAddr));
		}
		pOut = outBuf;
	}


}

/**
 * Function:        readConfFile
 * Brief:
 * Description:
 * Note:            Internal function.
 * Params:
 * Return:          Success: RES_OK.
                    Failure: RES_XXX_ERROR.
 */
static McpU8 readConfFile()
{
    McpU8 retVal = RES_OK;
    FILE *fp = NULL;

    char a_inputBuffer[MAX_BUF_LENGTH] = {'\0'};
    char *p_token = NULL;

    LOGD(" readConfFile: Entering \n");

    fp = fopen(GPS_CONFIG_FILE_PATH, RDONLY);
    if (NULL == fp)
    {
        LOGD(" readConfFile: fopen FAILED !!! \n");
        return RES_FILE_ERROR;
    }

    while( (fgets(a_inputBuffer, sizeof(a_inputBuffer), fp) ) &&
           (strcmp(a_inputBuffer, "\n") != RES_OK) )
    {
       LOGD(" readConfFile: a_inputBuffer = %s \n", a_inputBuffer);
       p_token = (char *)strtok(a_inputBuffer, ":");
       if ( (p_token) &&
            (strcmp(p_token, LOGGER_IP) == RES_OK) )
       {
           LOGD(" readConfFile: p_token = %s \n", p_token);
           p_token = (char *) strtok(NULL, ":");
           LOGD(" readConfFile: p_token = %s \n", p_token);
		   if (strlen(p_token) < sizeof(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetAddress))
		      strcpy(gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetAddress, p_token);

		   else
		   {
			   LOGD("readConfFile: Incorrect IP Address...");
			   return RES_ERROR;
		   }

       }
	   if ( (p_token) &&
            (strcmp(p_token, LOGGER_PORT) == RES_OK) )
       {
           LOGD(" readConfFile: p_token = %s \n", p_token);
           p_token = (char *) strtok(NULL, ":");
           LOGD(" readConfFile: p_token = %s \n", p_token);

		gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetPort  = atoi(p_token);

			LOGD("PORT VALUE READ: %d\n",  gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetPort);



       }
    }


    fclose(fp);
    LOGD(" readConfFile: Exiting Successfully. IP: %s, Port: %d\n", gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetAddress, gMcpSocket[MCP_LOG_NORMAL_INDEX].mcpLogUdpTargetPort);

    return retVal;
}

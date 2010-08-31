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


/***************************************************************************/
/*                                                                          */
/*    MODULE:   report.h                                                    */
/*    PURPOSE:  Report module internal header API                           */
/*                                                                          */
/***************************************************************************/
#ifndef __MCPF_REPORT_H__
#define __MCPF_REPORT_H__

/** \file  report.h 
 * \brief Report module API \n
 * APIs which are used for reporting messages to the User while running. \n\n
 * 
 * The report mechanism: Messages are reported per Module and severity Level    \n
 * Therefore, each module has a report flag which indicate if reporting for that module is enabled, \n
 * and each severity has a severity flag which indicate if reporting for that severity is enabled.  \n
 * Only if both flags are enabled, the message is printed. \n
 * The report flags of all Module are indicated in a bit map Table which is contained in the report module handle   \n
 * The report flags of all severities  are indicated in a bit map Table which is contained in the report module handle  \n
 */
#include "mcp_hal_types.h"
#include "mcp_hal_log.h"
#include "mcpf_defs.h"

/************************************/      
/*      Report Severity values      */
/************************************/

/** \enum EReportSeverity
 * \brief Report Severity Types
 * 
 * \par Description
 * All available severity Levels of the events which are reported to user.\n
 * 
 * \sa
 */
typedef enum
{
    REPORT_SEVERITY_INIT           =  1,    /**< Init Level (event happened during initialization)          */
    REPORT_SEVERITY_INFORMATION    =  2,    /**< Information Level                                          */
    REPORT_SEVERITY_WARNING        =  3,    /**< Warning Level                                              */
    REPORT_SEVERITY_ERROR          =  4,    /**< Error Level (error accored)                                */
    REPORT_SEVERITY_FATAL_ERROR    =  5,    /**< Fatal-Error Level (fatal-error accored)                    */
    REPORT_SEVERITY_SM             =  6,    /**< State-Machine Level (event happened in State-Machine)      */
    REPORT_SEVERITY_CONSOLE        =  7,    /**< Consol Level (event happened in Consol)                    */
    REPORT_SEVERITY_DEBUG_RX       =  8,    /**< Debug RX Level (event happened during RX debug)            */
    REPORT_SEVERITY_DEBUG_TX       =  9,    /**< Debug TX Level (event happened during TX debug)            */
    REPORT_SEVERITY_DEBUG_CONTROL  = 10,    /**< Debug Control Level (event happened during Control debug)  */
    REPORT_SEVERITY_MAX            = REPORT_SEVERITY_DEBUG_CONTROL + 1  /**< Maximum number of report severity levels   */

} EReportSeverity;

/** \struct TReport
 * \brief Report Module Object
 * 
 * \par Description
 * All the Databases and other parameters which are needed for reporting messages to user
 * 
 * \sa
 */
typedef struct 
{
    handle_t		hMcpf;												/**< Handle to Operating System Object																									*/
    McpU8           aSeverityTable[REPORT_SEVERITY_MAX];                /**< Severities Table: Table which holds for each severity level a flag which indicates whether the severity is enabled for reporting   */
    McpU8			aModuleTable[MCP_MAX_LOG_MODULES];					/**< Modules Table: Table which holds for each module a flag which indicates whether the module is enabled for reporting				*/
} TReport;

/** \struct TReportParamInfo
 * \brief Report Parameter Information
 * 
 * \par Description
 * Struct which defines all the Databases and other parameters which are needed
 * for reporting messages to user. 
 * The actual Content of the Report Parameter Could be one of the followed (held in union): 
 * Severity Table | Module Table | Enable/Disable indication of debug module usage
 * 
 * \sa  EExternalParam, ETwdParam
 */
typedef struct
{
    McpU32       paramType;                             /**< The reported parameter type - one of External Parameters (which includes Set,Get, Module, internal number etc.)
                                                            * of Report Module. Used by user for Setting or Getting Report Module Paramters, for exaple Set/Get severety table
                                                            * to/from Report Module
                                                            */
    McpU32       paramLength;                           /**< Length of reported parameter   */

    union
    {
        McpU8    aSeverityTable[REPORT_SEVERITY_MAX];   /**< Table which holds severity flag for every available LOG severity level. 
                                                            * This flag indicates for each severity - whether it is enabled for Logging or not. 
                                                            * User can Set/Get this Table
                                                            */
        McpU8    aModuleTable[MCP_MAX_LOG_MODULES];        /**< Table which holds module flag for every available LOG module.
                                                            * This flag indicates for each module - whether it is enabled for Logging or not.               
                                                            * User can Set/Get this Table
                                                            */                                                          
        McpU32   uReportPPMode;                         /**< Used by user for Indicating if Debug Module should be enabled/disabled                                                                 */

    } content;

} TReportParamInfo;

/** \struct TReportInitParams
 * \brief Report Init Parameters
 * 
 * \par Description
 * Struct which defines all the Databases needed for init and set the defualts of the Report Module.
 * 
 */
typedef struct
{
    /* Note: The arrays sizes are aligned to 4 byte to avoid padding added by the compiler! */
    McpU8       aSeverityTable[(REPORT_SEVERITY_MAX + 3) & ~3]; /**< Table in the size of all available LOG severity levels which indicates for each severity - whether it is enabled for Logging or not.   */
    McpU8       aModuleTable[(MCP_MAX_LOG_MODULES + 3) & ~3];  /**< Table in the size of all available LOG modules which indicates for each module - whether it is enabled for Logging or not              */              

} TReportInitParams;

/* Mapping between MCPF Report Modules IDs (EReportModule) and MCP_HAL_LOG modules IDs */
 extern McpHalLogModuleId_e aModuleIdMapping[]; 

/****************************/
/* report module Macros     */
/****************************/

/* The report mechanism is like that:
    Each module has a report flag. Each severity has a severity flag.
    Only if bits are enabled, the message is printed */
/* The modules which have their report flag enable are indicated by a bit map in the reportModule 
    variable contained in the report handle*/
/* The severities which have are enabled are indicated by a bit map in the reportSeverity
    variable contained in the report handle*/


/** \def MCPF_REPORT_INIT
 * \brief Macro which reports the user a message in INIT severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the INIT Severity Level and the Module itself are enabled for reporting
 */


#ifndef TI_LOG_DISABLE

#define MCPF_REPORT_INIT(hMcpf, module, msg)                        \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport && (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_INIT]) && (((TReport *)hReport)->aModuleTable[module]))  \
       { MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], ("INIT:")); \
	     MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		} 

/** \def MCPF_REPORT_INFORMATION
 * \brief Macro which reports the user a message in INFORMATION severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the INFORMATION Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_INFORMATION(hMcpf, module, msg)                 \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport && (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_INFORMATION]) && (((TReport *)hReport)->aModuleTable[module])) \
		{ MCP_HAL_LOG_INFO(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		}  

/** \def MCPF_REPORT_WARNING
 * \brief Macro which reports the user a message in WARNING severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the WARNING Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_WARNING(hMcpf, module, msg)                     \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport &&  ((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_WARNING])                                                      \
		{ MCP_HAL_LOG_ERROR(__FILE__, __LINE__, aModuleIdMapping[module], ("WARNING:")); \
		  MCP_HAL_LOG_ERROR(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		}  

/** \def MCPF_REPORT_ERROR
 * \brief Macro which reports the user a message in ERROR severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the ERROR Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_ERROR(hMcpf, module, msg)                       \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport &&  ((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_ERROR])    \
		{ MCP_HAL_LOG_ERROR(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		} 

/** \def MCPF_REPORT_FATAL_ERROR
 * \brief Macro which reports the user a message in FATAL ERROR severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the FATAL ERROR Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_FATAL_ERROR(hMcpf, module, msg)                 \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport && (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_FATAL_ERROR]) && (((TReport *)hReport)->aModuleTable[module])) \
		{ MCP_HAL_LOG_FATAL(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		} 

/** \def MCPF_REPORT_SM
 * \brief Macro which reports the user a message in State-Machine severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the State-Machine Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_SM(hMcpf, module, msg)                          \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport &&  (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_SM])    && (((TReport *)hReport)->aModuleTable[module]))  \                                                           \
		{ MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], ("SM:")); \
		  MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		} 

/** \def MCPF_REPORT_CONSOLE
 * \brief Macro which reports the user a message in CONSOLE severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the CONSOLE Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_CONSOLE(hMcpf, module, msg)                     \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport && (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_CONSOLE])    && (((TReport *)hReport)->aModuleTable[module]))  \
       { MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], ("CONSOLE:")); \
	     MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		} 

/** \def MCPF_REPORT_DEBUG_RX
 * \brief Macro which reports the user a message in DEBUG RX severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the DEBUG RX Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_DEBUG_RX(hMcpf, module, msg)                            \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport &&  (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_DEBUG_RX])  && (((TReport *)hReport)->aModuleTable[module]))\
       { MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], ("DEBUG RX:")); \
	     MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		} 

/** \def MCPF_REPORT_DEBUG_TX
 * \brief Macro which reports the user a message in DEBUG TX severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the DEBUG TX Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_DEBUG_TX(hMcpf, module, msg)                            \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport &&  (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_DEBUG_TX]) && (((TReport *)hReport)->aModuleTable[module]))\
       { MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], ("DEBUG TX:")); \
	     MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		} 

/** \def MCPF_REPORT_DEBUG_CONTROL
 * \brief Macro which reports the user a message in DEBUG CONTROL severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the DEBUG CONTROL Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_DEBUG_CONTROL(hMcpf, module, msg)                       \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport &&  (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_DEBUG_CONTROL]) && (((TReport *)hReport)->aModuleTable[module]))\
       { MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], ("DEBUG CONTROL:")); \
	     MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, aModuleIdMapping[module], msg); } \
			break; \
		}
 
/** \def MCPF_REPORT_DUMP_BUFFER
 * \brief Macro which dumps a buffer with the user's message in INFORMATION severity level.
 * For each module which tries to use this message to report the user: 
 *  - the message will be written only is the INFORMATION Severity Level and
 *  - the Module itself are enabled for reporting
 */
#define MCPF_REPORT_DUMP_BUFFER(hMcpf, module, msg, pBuf, len)                      \
    for(;;)                                                                         \
    {                                                                               \
        handle_t hReport = NULL;                                                    \
		if (hMcpf)                                                                  \
        {                                                                           \
            hReport = ((Tmcpf *)hMcpf)->hReport;                                    \
        }                                                                           \
		if (hReport &&                                                              \
            (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_INFORMATION]) &&  \
             (((TReport *)hReport)->aModuleTable[module]))                          \
        {                                                                           \
            MCP_HAL_LOG_DUMPBUF(__FILE__, __LINE__, msg, pBuf, len);                \
        }                                                                           \
		break;                                                                      \
    }  

/** \def MCPF_REPORT_HEX_INFORMATION
 * \brief Macro which reports the user a message in HEX INFORMATION severity level.
 * For each module which tries to use this message to report the user: 
 * the message will be written only is the HEX INFORMATION Severity Level and the Module itself are enabled for reporting
 */
#define MCPF_REPORT_HEX_INFORMATION(hMcpf, module, data, datalen)   \
    for(;;) { handle_t hReport = NULL; \
		if (hMcpf) hReport = ((Tmcpf *)hMcpf)->hReport; \
		if (hReport && (((TReport *)hReport)->aSeverityTable[REPORT_SEVERITY_INFORMATION]) && (((TReport *)hReport)->aModuleTable[module])) \
       { report_PrintDump (data, datalen); } break; } 


/** \def MCPF_OS_REPORT
 * \brief Macro which writes a message to user via specific Operating System printf.
 * E.g. print is done using the relevant used OS printf method.
 */
#define MCPF_OS_REPORT(msg)                                           \
    for(;;) { MCP_HAL_LOG_DEBUG(__FILE__, __LINE__, MCP_HAL_LOG_MODULE_TYPE_UNKNOWN, msg); \
				break; \
			} 

#ifdef INIT_MESSAGES
/** \def MCPF_INIT_REPORT
 * \brief Macro which writes a message to user via specific Operating System printf.
 * E.g. print is done using the relevant used OS printf method.
 */
#define MCPF_INIT_REPORT(msg)                                         \
    for(;;) { os_print msg; break; } 
#else
/** \def MCPF_INIT_REPORT
 * \brief Macro which writes a message to user via specific Operating System printf.
 * E.g. print is done using the relevant used OS printf method.
 */
#define MCPF_INIT_REPORT(msg) 
#endif
                     

#else  /* TI_LOG_DISABLE */

#define MCPF_REPORT_INIT(hMcpf, module, msg)
#define MCPF_REPORT_INFORMATION(hMcpf, module, msg)
#define MCPF_REPORT_WARNING(hMcpf, module, msg)
#define MCPF_REPORT_ERROR(hMcpf, module, msg)
#define MCPF_REPORT_FATAL_ERROR(hMcpf, module, msg)
#define MCPF_REPORT_SM(hMcpf, module, msg)
#define MCPF_REPORT_CONSOLE(hMcpf, module, msg)
#define MCPF_REPORT_DEBUG_RX(hMcpf, msg)
#define MCPF_REPORT_DEBUG_TX(hMcpf, msg)
#define MCPF_REPORT_DEBUG_CONTROL(hMcpf, msg)
#define MCPF_OS_REPORT(msg)

#endif  /* TI_LOG_DISABLE */


/****************************/
/* report module prototypes */
/****************************/

/** \brief  Create Report Module Object
 * \param  hMcpf   			- Mcpf module object handle
 * \return Handle to the report module on success, NULL otherwise
 * 
 * \par Description
 * Report module creation function, called by the config mgr in creation phase  \n
 * performs the following:  \n
 *  -   Allocate the report handle
 */ 
handle_t report_Create                 (handle_t hMcpf);

/** \brief  Set Report Module Defaults
 * \param  hReport      - Report module object handle
 * \param  pInitParams  - Pointer to Input Init Parameters
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Report module configuration function, called by the config mgr in configuration phase    \n
 * performs the following:  \n
 *  -   Reset & init local variables
 *  -   Resets all report modules bits
 *  -   Resets all severities bits
 *  -   Init the description strings * 
 */ 
EMcpfRes report_SetDefaults            (handle_t hReport, TReportInitParams *pInitParams);

/** \brief  Unload Report Module
 * \param  hReport      - Report module object handle
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Report Module unload function, called by the config mgr in the unload phase  \n
 * performs the following:  \n
 *  -   Free all memory allocated by the module     
 */ 
EMcpfRes report_Unload                 (handle_t hReport);

/** \brief  Set Report Module
 * \param  hReport      - Report module object handle
 * \param  module_index - Index of Module to be enabled for reporting 
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Enables the relevant module for reporting -  according to the received module index 
 */ 
EMcpfRes report_SetReportModule        (handle_t hReport, McpU8 module_index);

/** \brief  Clear Report Module
 * \param  hReport      - Report module object handle
 * \param  module_index - Index of Module to be disable for reporting 
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Disables the relevant module for reporting -  according to the received module index 
 */ 
EMcpfRes report_ClearReportModule      (handle_t hReport, McpU8 module_index);

/** \brief  Set Report Severity
 * \param  hReport      - Report module object handle
 * \param  uSeverity - Index of Severity to be enabled for reporting 
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Enables the relevant severity for reporting -  according to the received severity index 
 */ 
EMcpfRes report_SetReportSeverity(handle_t hReport, McpU8 uSeverity);

/** \brief  Clear Report Severity
 * \param  hReport      - Report module object handle
 * \param  module_index - Index of Severity to be disable for reporting 
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Disables the relevant Severity for reporting -  according to the received Severity index 
 */ 
EMcpfRes report_ClearReportSeverity(handle_t hReport, McpU8 uSeverity);

/** \brief  Get Report Module Table
 * \param  hReport      - Report module object handle
 * \param  pModules     - Pointer to output Module Table
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Returns the Modules Table (the table which holds Modules reporting indication) held in Report Module Object
 */ 
EMcpfRes report_GetReportModuleTable   (handle_t hReport, McpU8 *pModules); 

/** \brief  Set Report Module Table
 * \param  hReport      - Report module object handle
 * \param  pModules     - Pointer to input Module Table
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Updates the Modules Table: copies the input Modules Table (the table which holds Modules reporting indication) 
 * to the Modules Table which is held in Report Module Object
 */ 
EMcpfRes report_SetReportModuleTable   (handle_t hReport, McpU8 *pModules); 

/** \brief  Get Report Severity Table
 * \param  hReport      - Report module object handle
 * \param  pSeverities  - Pointer to input Severity Table
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Returns the Severities Table (the table which holds Severities reporting indication) held in Report Module Object
 */ 
EMcpfRes report_GetReportSeverityTable (handle_t hReport, McpU8 *pSeverities);

/** \brief  Set Report Severity Table
 * \param  hReport      - Report module object handle
 * \param  pSeverities  - Pointer to input Severity Table
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Updates the Severities Table: copies the input Severities Table (the table which holds Severities reporting indication) 
 * to the Severities Table which is held in Report Module Object
 */ 
EMcpfRes report_SetReportSeverityTable (handle_t hReport, McpU8 *pSeverities);

/** \brief  Set Report Parameters
 * \param  hReport      - Report module object handle
 * \param  pParam       - Pointer to input Report Parameters Information
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Report set param function sets parameter/s to Report Module Object. 
 * Called by the following:
 *  -   configuration Manager in order to set a parameter/s from the OS abstraction layer
 *  -   Form inside the driver 
 */ 
EMcpfRes report_SetParam               (handle_t hReport, TReportParamInfo *pParam);                   

/** \brief  Get Report Parameters
 * \param  hReport      - Report module object handle
 * \param  pParam       - Pointer to output Report Parameters Information
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Report get param function gets parameter/s from Report Module Object. 
 * Called by the following:
 *  -   configuration Manager in order to get a parameter/s from the OS abstraction layer
 *  -   from inside the driver 
 */ 
EMcpfRes report_GetParam               (handle_t hReport, TReportParamInfo *pParam); 

/** \brief Report Dump
 * \param  pBuffer  - Pointer to input HEX buffer
 * \param  pString  - Pointer to output string
 * \param  size     - size of output string
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Converts hex buffer to string buffer
 * NOTE:    1. The caller has to make sure that the string size is at lease: ((Size * 2) + 1)
 *          2. A string terminator is inserted into lase char of the string 
 */ 
EMcpfRes report_Dump                  (McpU8 *pBuffer, char *pString, McpU32 size);

/** \brief Report Print Dump
 * \param  pData    - Pointer to input data buffer
 * \param  datalen  - size of input data buffer
 * \return RES_COMPLETE on success or RES_ERROR on failure
 * 
 * \par Description
 * Performs HEX dump of input Data buffer. Used for debug only! 
 */ 
EMcpfRes report_PrintDump                 (McpU8 *pData, McpU32 datalen);

#endif /* __MCPF_REPORT_H__ */



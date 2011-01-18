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


/** \file   mcp_hal_uart.c
 *  \brief  Linux OS Adaptation Layer for UART Interface implementation
 *
 *  \see    mcp_hal_uart.h
 */

#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "mcpf_mem.h"
#include "mcpf_report.h"
#include "mcp_hal_os.h"
#include "mcp_hal_types.h"

#include "BusDrv.h"
#include "TxnDefs.h"
#include "mcp_hal_uart.h"

/************************************************************************
 * Defines
 ************************************************************************/

/* Software flow control characters */
#define XON_CHAR       0x11
#define XOFF_CHAR      0x13

#define NUMBER_ZERO         0
#define MAX_FILENAME_SIZE   20

#define MIN(a,b) (((a) < (b)) ? (a):(b))

/************************************************************************
 * Types
 ************************************************************************/
typedef int             PORT_HANDLE;
typedef pthread_t       THREAD_HANDLE;
typedef sem_t*          SEM_HANDLE;

/* HAL UART Events */
typedef enum
{
    Event_WriteCompl,
    Event_Mng,
    Event_MaxNum

} EUartEvent;

#ifdef TRAN_DBG
/* Debug counters */
typedef struct
{

    McpU32       Tx;
    McpU32       TxCompl;
    McpU32       Rx;
    McpU32       RxReadyInd;

} THalUartDebugCount;
#endif

/* Transport Layer Object */
typedef struct
{
    /* module handles */
    handle_t              hMcpf;
    TI_TxnEventHandlerCb  fEventHandlerCb;
    handle_t              hHandleCb;
    TUartCfg              uartCfg;
	char				cDrvName[BUSDRV_NAME_MAX_SIZE];

    PORT_HANDLE           hOsPort;
    THREAD_HANDLE         hOsThread;
    sem_t                 tSem;
	int 				  pipeFd[2];

    McpBool               terminationFlag;
    McpS32                iRxReadNum;		/* number of bytes requested to read */
	McpS32                iRxAvailNum;		/* number of available bytes to read from UART device */
    McpS32                iRxRepNum;	    /* number of bytes to report for previous read call */

    McpU8                 *pInData;       	/* Current data pointer for receiver */

#ifdef TRAN_DBG
    THalUartDebugCount dbgCount;
#endif

} THalUartObj;

#ifdef TRAN_DBG
THalUartObj * g_pHalUartObj;
#endif


/************************************************************************
 * Internal functions prototypes
 ************************************************************************/

static EMcpfRes uartPortInit (THalUartObj  *pHalUart);
static EMcpfRes uartPortConfig (THalUartObj  *pHalUart);
static void * uartThreadFun (void * pParam);
static EMcpfRes uartPortDestroy (THalUartObj    *pHalUart);
static EMcpfRes uartPortRxReset (THalUartObj *pHalUart, McpU8 *pBuf, McpU16 len);
static McpU32 uartSpeedToOSbaudrate (handle_t hMcpf, ThalUartSpeed  speed);

static void signalSem  (handle_t hMcpf, SEM_HANDLE hSem);
static void waitForSem (handle_t hMcpf, SEM_HANDLE hSem);
static void halUartDestroy (THalUartObj  * pHalUart);


void rxTest (THalUartObj  *pHalUart);

/************************************************************************
 *
 *   Module functions implementation
 *
 ************************************************************************/

/**
 * \fn     HAL_UART_Create
 * \brief  Create the HAL UART object
 *
 */

handle_t HAL_UART_Create (const handle_t hMcpf, const char *pDrvName)
{
    THalUartObj  * pHalUart;

    pHalUart = mcpf_mem_alloc (hMcpf, sizeof(THalUartObj));
    if (pHalUart == NULL)
    {
        return NULL;
    }

#ifdef TRAN_DBG
    g_pHalUartObj = pHalUart;
#endif

    mcpf_mem_zero (hMcpf, pHalUart, sizeof(THalUartObj));

    pHalUart->hMcpf = hMcpf;
	if (strlen(pDrvName) < BUSDRV_NAME_MAX_SIZE)
	{
		strcpy (pHalUart->cDrvName, pDrvName);
	}
	else
	{
		/* driver name string is too long, truncate it */
		mcpf_mem_copy (hMcpf, pHalUart->cDrvName, (McpU8 *) pDrvName, BUSDRV_NAME_MAX_SIZE - 1);
		pHalUart->cDrvName[BUSDRV_NAME_MAX_SIZE-1] = 0; /* line termination */
	}

    /* Create Rx thread semaphore semaphore */
    if (sem_init(&pHalUart->tSem,
                 0,                            /* Not shared between processes */
                 0) < NUMBER_ZERO )            /* Not available */
    {
        MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("Rx sem_init failed!\n"));
        halUartDestroy (pHalUart);
        return NULL;
    }

    MCPF_REPORT_INIT (pHalUart->hMcpf, HAL_UART_MODULE_LOG,  ("HAL_UART_Create OK"));
   
    return ((handle_t) pHalUart);
}


/**
 * \fn     HAL_UART_Destroy
 * \brief  Destroy the HAL UART object
 *
 */
EMcpfRes HAL_UART_Destroy (handle_t hHalUart)
{
    THalUartObj  *pHalUart = (THalUartObj *) hHalUart;

    /* Release all the thread waiting for this semaphore before deleting it. */
    signalSem (pHalUart->hMcpf, &pHalUart->tSem);

    /* Destroy the semaphore */
    if ( sem_destroy(&pHalUart->tSem) < NUMBER_ZERO)
    {
        MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,  ("sem_destroy failed!\n"));
        return RES_ERROR;
    }

    mcpf_mem_free (pHalUart->hMcpf, pHalUart);
    return RES_OK;
}


/**
 * \fn     HAL_UART_Init
 * \brief  HAL UART object initialization
 *
 * Initiate state of HAL UART object
 *
 */
EMcpfRes HAL_UART_Init (  handle_t                    hHalUart,
                          const TI_TxnEventHandlerCb fEventHandlerCb,
                          const handle_t            hHandleCb,
                          const TBusDrvCfg          *pConf,
                          McpU8                     *pBuf,
                          const McpU16              len)
{
    THalUartObj  *pHalUart = (THalUartObj *) hHalUart;
    EMcpfRes      status;

    /** Used when uart input buffer hs to be flushed
      * McpU8 *pBuffer;
      * McpU16 lengthToBeRead = 1; */

    pHalUart->fEventHandlerCb = fEventHandlerCb;
    pHalUart->hHandleCb = hHandleCb;
    /* Data will be read into this buffer */
    pHalUart->pInData = pBuf;
    pHalUart->iRxReadNum  = len;

    mcpf_mem_copy(pHalUart->hMcpf, &pHalUart->uartCfg,
                  (void *) &pConf->tUartCfg, sizeof(pConf->tUartCfg));

    status = uartPortInit (pHalUart);

    if (status != RES_OK)
    {
        MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("uartPortInit failed!\n") );
        if (pHalUart->hOsPort)
        {
            close(pHalUart->hOsPort);
        }
        halUartDestroy (pHalUart);
        return RES_ERROR;
    }

    /* Flush the com port buffer.
     * Ideally not required as this is done in uartPortInit()*/
    /* uartPortRxReset (pHalUart, pBuffer, lengthToBeRead); */

    MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("HAL_UART_Init: create thread, hHalUart=%p \n", hHalUart));

    /* Used for exiting from UART thread */
    pHalUart->terminationFlag = MCP_FALSE;

    if(pthread_create( &pHalUart->hOsThread,           /* Thread handle */
                       NULL,                           /* Default attributes */
                       uartThreadFun,                  /* Function */
                       hHalUart) != NUMBER_ZERO)    /* Thread function parameter */
    {
          MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("pthread_create failed!\n"));
          if (pHalUart->hOsPort)
          {
            close(pHalUart->hOsPort);
          }

          halUartDestroy (pHalUart);
          return RES_ERROR;
    }

    MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("HAL_UART_Init: waiting for UART thread initialization completion ...\n"));
    /* wait for completion of thread initialization */
    waitForSem (pHalUart->hMcpf, &pHalUart->tSem);
    MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("HAL_UART_Init: UART thread initialization is completed\n"));

    return status;
}


/**
 * \fn     HAL_UART_Deinit
 * \brief  Deinitializes the HAL UART object
 *
 */
EMcpfRes HAL_UART_Deinit (handle_t  hHalUart)
{
    THalUartObj  *pHalUart = (THalUartObj *) hHalUart;
    McpU8   uMngEvt =  (McpU8) Event_Mng; 


    /* Send destroy event to uart thread */
    pHalUart->terminationFlag = MCP_TRUE;

    /* Signal UART thread to terminate*/
    if(write(pHalUart->pipeFd[1], (const void*) &uMngEvt, 1) == -1)
    {
		MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,
							  ("%s: Pipe write failed, err=%u\n", __FUNCTION__, errno));
		return RES_ERROR;
    }

    MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: waiting for completion of UART thread deletion ...\n", __FUNCTION__));
    /* wait for completion of uart thread deletion */
    waitForSem (pHalUart->hMcpf, &pHalUart->tSem);
    MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: thread deletion is completed\n", __FUNCTION__));

    return RES_OK;
}


/**
 * \fn     HAL_UART_Read
 * \brief  Read data from OS UART device
 *
 */
ETxnStatus HAL_UART_Read (const handle_t    hHalUart,
                          McpU8         	*pBuf,
                          const McpU16  	uLen,
                          McpU16        	*uReadLen)
{
    THalUartObj	*pHalUart  = (THalUartObj *) hHalUart;
	McpS32		iReadNum;
    McpS32		iBytesRead = 0;
	ETxnStatus  eRes = TXN_STATUS_ERROR;

    MCPF_REPORT_DEBUG_RX(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: begin pBuf=%p len=%u avail=%d\n", 
										   __FUNCTION__, pBuf, uLen, pHalUart->iRxAvailNum));

	if (!pHalUart->terminationFlag)
    {
		iReadNum = MIN(uLen, pHalUart->iRxAvailNum);
		if (iReadNum > 0)
		{
			/* Read received bytes from UART device */
			iBytesRead = read (pHalUart->hOsPort, pBuf, iReadNum);

			if (iBytesRead >= 0)
			{
				pHalUart->iRxAvailNum -= iBytesRead;
				eRes = TXN_STATUS_COMPLETE;
			}
			else
			{
				MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,
								   ("%s: ReadFile failed, err=%u\n", __FUNCTION__, errno));

				*uReadLen = 0;
				return TXN_STATUS_ERROR;
			}
		}
		else
		{
			/* Nothing is available to read for now, prepare for the incoming data */
			eRes = TXN_STATUS_PENDING;
			pHalUart->iRxReadNum = uLen;	/* byte number requested to read  	   */
			pHalUart->pInData 	 = pBuf;    /* buffer to use for the incoming data */

			MCPF_REPORT_DEBUG_RX(pHalUart->hMcpf, HAL_UART_MODULE_LOG, 
								 ("%s: prepare pBuf=%p len=%d readNum=%d retLen=%d\n", 
								  __FUNCTION__, pBuf, uLen, pHalUart->iRxReadNum, iBytesRead));
		}
	}
	else
	{
		eRes = TXN_STATUS_ERROR;
	}


	MCPF_REPORT_DEBUG_RX(pHalUart->hMcpf, HAL_UART_MODULE_LOG, 
						 ("HAL_UART_Read: end pBuf=%p len=%u readNum=%d retLen=%d\n", 
						  pBuf, uLen, pHalUart->iRxReadNum, iBytesRead));

	*uReadLen = (McpU16) iBytesRead;
	return eRes;
}

/**
 * \fn     HAL_UART_ReadResult
 * \brief  Return the read status and the number of received bytes
 *
 */
EMcpfRes HAL_UART_ReadResult (const handle_t  hHalUart, McpU16 *pLen)
{
    THalUartObj  *pHalUart = (THalUartObj *) hHalUart;

    *pLen = (McpU16) pHalUart->iRxRepNum;

	MCPF_REPORT_DEBUG_RX(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: availNum=%d readNum=%d retLen=%d\n",
                         __FUNCTION__, pHalUart->iRxAvailNum, pHalUart->iRxReadNum, pHalUart->iRxRepNum));

    return RES_OK;
}


/**
 * \fn     HAL_UART_Write
 * \brief  Wrtite data to OS UART device
 *
 */
ETxnStatus HAL_UART_Write (const handle_t   hHalUart,
                           const McpU8  *pBuf,
                           const McpU16 len,
                           McpU16       *sentLen)
{
    THalUartObj  *pHalUart = (THalUartObj *) hHalUart;
    McpS32          iBytesSent ;
    McpS32          iBytesToSend = (McpS32) len;
    McpU8           uTxComplEvt =  (McpU8) Event_WriteCompl; 
  
    MCPF_REPORT_DEBUG_TX(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("HAL_UART_Write: pBuf=%p len=%u\n", pBuf, len));
    for(;;)
    {
        iBytesSent = write (pHalUart->hOsPort,
                            pBuf,
                            iBytesToSend);

        if (iBytesSent == iBytesToSend)
        {
            /* write is done */
            break;
        }
        else if (iBytesSent < 0)
        {
            MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,
                                  ("%s: WriteFile failed, err=%u\n", __FUNCTION__, errno));

            *sentLen = 0;   /* consider in the case of error nothing is sent */
            return TXN_STATUS_ERROR;
        }
        else 
        {
            /* continue to send */
            pBuf += iBytesSent;
            iBytesToSend -= iBytesSent;
        }
    }
    *sentLen = (McpU16) len;

    /* Signal Tx completion to UART thread */
    if(write(pHalUart->pipeFd[1], (const void*) &uTxComplEvt, 1) == -1)
    {
		MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,
							  ("%s: Pipe write failed, err=%u\n", __FUNCTION__, errno));
		return TXN_STATUS_ERROR;
    }

    return TXN_STATUS_PENDING;
}

/**
 * \fn     HAL_UART_Port_Reset
 * \brief  reset and optionally start HAL UART  hardware to
 * 
 * 
 */ 
EMcpfRes HAL_UART_Port_Reset (handle_t  *hHalUart, McpU8 *pBuf, McpU16 len, const McpU16 baudrate)
{
    THalUartObj     *pHalUart = (THalUartObj *) hHalUart;

    MCP_UNUSED_PARAMETER(pBuf);
    MCP_UNUSED_PARAMETER(len);

    pHalUart->uartCfg.uBaudRate = (McpU32)baudrate;
    return uartPortConfig(pHalUart);
}

/**
 * \fn     HAL_UART_RestartRead
 * \brief  Signals to start read operation
 *
 */
EMcpfRes HAL_UART_RestartRead (const handle_t  hHalUart)
{
    handle_t  lHalUart = hHalUart; /* reduce compiler warnings*/
    
    MCP_UNUSED_PARAMETER(lHalUart);
    /* Nothing to do, the read is not stopped */
    return RES_OK;
}

/**
 * \fn     HAL_UART_ResetRead
 * \brief  Cleans Rx buffer and starts a new read operation
 *
 */
EMcpfRes HAL_UART_ResetRead (const handle_t  hHalUart, McpU8 *pBuf, const McpU16 len)
{
    THalUartObj  *pHalUart = (THalUartObj *) hHalUart;
    EMcpfRes eRes;

    eRes = uartPortRxReset (pHalUart, pBuf, len);

    return eRes;
}


/**
 * \fn     HAL_UART_Set
 * \brief  Set HAL UART object new configuration
 *
 */

EMcpfRes HAL_UART_Set (handle_t             hHalUart,
                         const TBusDrvCfg   *pConf)
{
    THalUartObj  *pHalUart = (THalUartObj *) hHalUart;

    pHalUart->uartCfg.uBaudRate = pConf->tUartCfg.uBaudRate;
    uartPortConfig(pHalUart);

    return RES_OK;
}


/**
 * \fn     halUartDestroy
 * \brief  Destroy HAL UART Object
 *
 * Frees memory occupied by HAL UART Object
 *
 * \note
 * \param   pHalUart - pointer to HAL UART object
 * \return  void
 * \sa
 */
static void halUartDestroy (THalUartObj  * pHalUart)
{
    mcpf_mem_free (pHalUart->hMcpf, pHalUart);
}



/**
 * \fn     uartPortDestroy
 * \brief  UART port de-initialization
 *
 * Stop and close OS UART port, free allocated memory for HAL UART object
 *
 * \note
 * \param   pHalUart  - pointer to HAL UART object
 * \return  Returns the status of operation: OK or Error
 * \sa      uartThreadFun
 */
static EMcpfRes uartPortDestroy (THalUartObj  *pHalUart)
{
    /*
     * Discard all characters from the output & input buffer.
     * We neednot terminate pending read or write operations as no async calls are used */
    tcflush (pHalUart->hOsPort, TCIOFLUSH);
    close (pHalUart->hOsPort);

	close (pHalUart->pipeFd[ 0 ]);
	close (pHalUart->pipeFd[ 1 ]);

    return RES_OK;
}

#define MCPHAL_OS_SERIAL_PORT_NAME                                   "/dev/ttyO1"

/**
 * \fn     uartPortInit
 * \brief  UART port initialization
 *
 * Open and initiate Win OS UART device
 *
 * \note
 * \param   pHalUart  - pointer to HAL UART object
 * \return  Returns the status of operation: OK or Error
 * \sa      HAL_UART_Init
 */
static EMcpfRes uartPortInit (THalUartObj  *pHalUart)
{
    /* data structure containing terminal information */
    struct termios  devCtrl;
    char comFileName[MAX_FILENAME_SIZE];

    MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("uartPortInit: PortNum = %d \n", (pHalUart->uartCfg.portNum - 1) ));

    sprintf (comFileName, MCPHAL_OS_SERIAL_PORT_NAME );

    pHalUart->hOsPort = open( comFileName, O_RDWR );
    if (pHalUart->hOsPort < NUMBER_ZERO)
    {
        /* Error opening the file */
        MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("Com Port open failed, dev=%s! \n", comFileName));
        return RES_ERROR;
    }

	if (pipe (pHalUart->pipeFd) != 0)
	{
        MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("Pipe create failed!\n"));
        return RES_ERROR;
	}

    bzero(&devCtrl, sizeof(devCtrl));

    /* Get and set the comm port state */
    if (tcgetattr(pHalUart->hOsPort, &devCtrl) < 0)
    {
        MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("tcgetattr failed, dev=%s! \n", comFileName));
        return RES_ERROR;
    }
	cfmakeraw(&devCtrl);

    devCtrl.c_cflag = B115200 | CS8 | CLOCAL | CREAD;

    if (tcsetattr(pHalUart->hOsPort, TCSANOW, &devCtrl) < 0)
    {
        MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,  ("tcsetattr failed, dev=%s! \n", comFileName));
        return RES_ERROR;
    }

    pHalUart->uartCfg.uBaudRate = HAL_UART_SPEED_115200;    /* Set default baud rate */

    /* Set initial baudrate */
    if ( uartPortConfig (pHalUart) != RES_OK)
    {
        MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,  ("uartPortConfig failed, fileName=%s! \n", comFileName));
        return RES_ERROR;
    }

    tcflush(pHalUart->hOsPort, TCIOFLUSH);

    return RES_OK;
}


/**
 * \fn     uartPortConfig
 * \brief  UART port configuration
 *
 * Change UART port configuration parameters as baudrate and Xon/Xoff limits
 *
 * \note    UART port is to be initialized before the function call
 * \param   pHalUart  - pointer to HAL UART object
 * \return  Returns the status of operation: OK or Error
 * \sa      uartPortInit
 */
static EMcpfRes uartPortConfig (THalUartObj  *pHalUart)
{
    struct termios  devCtrl;               /* data structure containing terminal information */
    McpU32          uFlagRate;

    uFlagRate = uartSpeedToOSbaudrate (pHalUart->hMcpf, pHalUart->uartCfg.uBaudRate);

    /* Get and set the comm port state */
    tcgetattr( pHalUart->hOsPort, &devCtrl );

    cfsetospeed(&devCtrl, uFlagRate);
    cfsetispeed(&devCtrl, uFlagRate);

    if (tcsetattr(pHalUart->hOsPort, TCSANOW, &devCtrl))
    {
        MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,  ("uartPortConfig set baudrate failed! \n"));
        return RES_ERROR;     
    }

    tcflush(pHalUart->hOsPort, TCIFLUSH);

    return RES_OK;
}

/**
 * \fn     uartThreadFun
 * \brief  UART thread main function
 *
 * Thread function waits on and processes OS events (write complete, read ready and management)
 *
 * \note
 * \param   pHalUart  - pointer to HAL UART object
 * \return  N/A
 * \sa      HAL_UART_Init
 */
static void *uartThreadFun (void * pParam)
{
    THalUartObj  *pHalUart = (THalUartObj *) pParam;
    TTxnEvent     tEvent;
    fd_set  	  readFds;
    int     	  iRetCode;

    /* Identify ! */
    MCP_HAL_LOG_SetThreadName("UART");
    
    MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: started, pHalUart=%p\n", __FUNCTION__, pHalUart));

    /* This thread would continue running till termination flag is set to TRUE */
    pHalUart->terminationFlag = MCP_FALSE;

    /* signal semaphore the thread is created */
    signalSem (pHalUart->hMcpf,  &pHalUart->tSem);

    while (!pHalUart->terminationFlag)
    {
        // MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("uartThreadFun: Reading data synchronously...\n"));

        /* initialize file descr. bit sets */
        FD_ZERO (&readFds);
        FD_SET (pHalUart->hOsPort,   &readFds);
        FD_SET (pHalUart->pipeFd[0], &readFds);

#ifdef GPSC_DRV_DEBUG
		MCPF_REPORT_INFORMATION(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: waiting on select...\n", __FUNCTION__));
#endif


		MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: waiting on select...\n", __FUNCTION__));

		iRetCode = select (FD_SETSIZE, &readFds, NULL, NULL, NULL);

		MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: awaiking from select\n", __FUNCTION__));

		if (iRetCode > 0)
        {
				/* Determine which file descriptor is ready for read */
				if (FD_ISSET(pHalUart->hOsPort, &readFds))
				{
					McpS32	iReadNum, iBytesRead;

					iRetCode = ioctl (pHalUart->hOsPort, FIONREAD, &pHalUart->iRxAvailNum);

					/*cmcm check return of ioctl*/
					if (iRetCode >= 0 )
					{
					//MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: ReadReady event,  res=%d avail=%d\n", __FUNCTION__, iRetCode, pHalUart->iRxAvailNum));

					/* read requested number of bytes */
					iReadNum = MIN(pHalUart->iRxReadNum, pHalUart->iRxAvailNum);

					if (iReadNum > 0)
					{
						/* Read received bytes from UART device */
						iBytesRead = read (pHalUart->hOsPort, pHalUart->pInData, iReadNum);

						if (iBytesRead >= 0)
						{
							pHalUart->iRxAvailNum -= iBytesRead;
							pHalUart->iRxRepNum = iBytesRead;
						}
						else
						{
							MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,
											   ("%s: Read failed, err=%u\n", __FUNCTION__, errno));
						}
					}
					else
					{
						MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,
										   ("%s: uexpected num zero\n", __FUNCTION__));
					}

					tEvent.eEventType = Txn_ReadReadylInd;
					pHalUart->fEventHandlerCb (pHalUart->hHandleCb, &tEvent);
				}
			else
			 {
				MCPF_REPORT_ERROR(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("uartThreadFun: ioctl error %d", strerror(errno)));
			 }
			}


			if (FD_ISSET(pHalUart->pipeFd[0], &readFds))
			{
				McpU8	uEvt;

                /* Read management event */
				iRetCode = read (pHalUart->pipeFd[0], (void*)&uEvt, 1);
				if (iRetCode > 0)
				{
                    switch (uEvt)
                    {
                    case Event_WriteCompl:
                        MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: WriteComplete event\n", __FUNCTION__));
                        tEvent.eEventType = Txn_WriteComplInd;
                        pHalUart->fEventHandlerCb (pHalUart->hHandleCb, &tEvent);
                        break;

                    case Event_Mng:
                        MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s:  Exiting the loop\n", __FUNCTION__));
                        break;

                    default:
                        break;
                    }
				}
				else
				{
					MCPF_REPORT_ERROR(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: pipe read error\n", __FUNCTION__));
				}
			}
        }
		else if (iRetCode == 0)
		{
			MCPF_REPORT_ERROR(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: select timeout\n", __FUNCTION__));
		}
		else
		{
			MCPF_REPORT_ERROR(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: select failed\n", __FUNCTION__));
			return NULL;

		}
    }/* while */

    /* Termination flag is set. Exit from this context */
    MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: exit thread\n",__FUNCTION__));
	MCPF_REPORT_INFORMATION(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("LPOINT: uartThreadFun EXITED..."));

    uartPortDestroy (pHalUart);

    signalSem (pHalUart->hMcpf, &pHalUart->tSem);  /* signal semaphore the thread is about to exit */

    return NULL;
}



/**
 * \fn     signalSem
 * \brief  Signal semaphore
 *
 * Release counting semaphore and increments count by one unit
 *
 * \note
 * \param   hMcpf - OS framework handle
 * \param   hSem  - OS semphore handle
 * \return  void
 * \sa
 */
static void signalSem (handle_t hMcpf, SEM_HANDLE hSem)
{
    int result;

    /* Returns 0 on success */
    result = sem_post(hSem);

    if (result)
    {
        MCPF_REPORT_ERROR (hMcpf, HAL_UART_MODULE_LOG, ("%s: sem_post failed, handle=%p!\n", __FUNCTION__, hSem));
    }
}


/**
 * \fn     waitForSem
 * \brief  Waits for semaphore
 *
 * Wait for counting semaphore forever
 *
 * \note
 * \param   hMcpf - OS framework handle
 * \param   hSem  - OS semphore handle
 * \return  void
 * \sa
 */
static void waitForSem (handle_t hMcpf, SEM_HANDLE hSem)
{
    int result;

    /* wait infinitely for the semaphore.
     * sem_wait() is used to acquire the semaphore access.
     * This function suspends the calling thread until the semaphore has a non-zero count.
     * It then atomically decreases the semaphore count */
    result = sem_wait (hSem);

    /* If the call was unsuccessful, the state of the semaphore is unchanged,
     * and the function returns a value of -1 and sets errno to indicate the error. */
    if (result == -1)
    {
        MCPF_REPORT_ERROR (hMcpf, HAL_UART_MODULE_LOG,
            ("%s: sem_wait failed, result = %d \n", __FUNCTION__, result));
    }
}

#if 0 /* tbd remove */
/**
 * \fn     uartPortRxStart
 * \brief  UART port receive start
 *
 * Resets and start UART receive operation
 *
 * \note
 * \param   pHalUart  - pointer to HAL UART object
 * \return  Returns the status of operation: OK or Error
 * \sa      HAL_UART_Init
 */
static EMcpfRes uartPortRxStart (THalUartObj *pHalUart, McpU8 *pBuf, McpU16 len)
{
    /* Activate DTR line */
    MCP_UNUSED_PARAMETER(pHalUart);
    MCP_UNUSED_PARAMETER(pBuf);
    MCP_UNUSED_PARAMETER(len);
    return RES_OK;
}
#endif

/**
 * \fn     uartPortRxReset
 * \brief  UART port receive reset
 *
 * Purges UART device input buffer and triggers the new receive operation
 *
 * \note
 * \param   pHalUart  - pointer to HAL UART object
 * \return  Returns the status of operation: OK or Error
 * \sa      uartPortRxStart
 */
static EMcpfRes uartPortRxReset (THalUartObj *pHalUart, McpU8 *pBuf, McpU16 len)
{
    McpS32	iReadNum, iRetCode;
    
    MCP_UNUSED_PARAMETER(len);

	/* 
	 * Read and discard all available bytes from UART port input buffer.
	 * ReadFile returns 0 when fails (no more data to read or error)
	 */

    iRetCode = ioctl (pHalUart->hOsPort, FIONREAD, &iReadNum);

    MCPF_REPORT_DEBUG_CONTROL(pHalUart->hMcpf, HAL_UART_MODULE_LOG, ("%s: Rx queue  res=%d avail=%d\n", __FUNCTION__, iRetCode, iReadNum));

    /* read available number of bytes from UART input queue */
    iRetCode = read (pHalUart->hOsPort, pBuf, iReadNum);
    if (iRetCode < 0)
    {
        MCPF_REPORT_ERROR (pHalUart->hMcpf, HAL_UART_MODULE_LOG,
                           ("%s: Read failed, err=%u\n", __FUNCTION__, errno));
		return RES_ERROR;
    }
	return RES_OK;
}


/**
 * \fn     uartSpeedToOSbaudrate
 * \brief  Convert UART speed to baudrate
 *
 * Convert UART configuration speed to Win OS baudrate
 *
 * \note
 * \param   speed  - HAL UART speed value
 * \return  baudrate
 * \sa      uartSpeedToOSbaudrate
 */
static McpU32 uartSpeedToOSbaudrate(handle_t hMcpf, ThalUartSpeed  speed)
{
    McpU32 baudrate;

    switch (speed) {
    case HAL_UART_SPEED_9600:
        baudrate = B9600;
        break;
    case HAL_UART_SPEED_38400:
        baudrate = B38400;
        break;
    case HAL_UART_SPEED_57600:
        baudrate = B57600;
        break;
    case HAL_UART_SPEED_115200:
        baudrate = B115200;
        break;
#if 0
    case HAL_UART_SPEED_128000:
        baudrate = B128000;
        break;
#endif
    case HAL_UART_SPEED_230400:
        baudrate = B230400;
        break;
#if 0
    case HAL_UART_SPEED_256000:
        baudrate = B256000;
        break;
#endif
    case HAL_UART_SPEED_460800:
        baudrate = B460800;
        break;
    case HAL_UART_SPEED_921600:
        baudrate = B921600;
        break;
    case HAL_UART_SPEED_1000000:
        baudrate = B1000000;
        break;
    case HAL_UART_SPEED_2000000:
        baudrate = B2000000;
        break;
    case HAL_UART_SPEED_3000000:
        baudrate = B3000000;
        break;
    default:
        MCPF_REPORT_ERROR (hMcpf, HAL_UART_MODULE_LOG, ("baud rate = %u is not supported!\n", speed));
        baudrate = speed * 1000;
    }
    return baudrate;
}
void resetDataPtr(handle_t hHalUart, McpU8 *pBuf)
{
    THalUartObj  *pHalUart = (THalUartObj *) hHalUart;
    pHalUart->pInData = pBuf;
}


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
#include "pla_hw.h"



typedef struct _McpGPIOhandle
{
    McpS32 *nShutdownHnd;
}McpGPIOhandle;


typedef enum _GpsGpioState
{
	GPIO_POWER_OFF,
	GPIO_POWER_ON,
	GPIO_POWER_RESET,
	GPIO_POWER_SLEEP
}GpsGpioState;

/************************************************************************/
/*								APIs                                    */
/************************************************************************/
#ifdef CLIENT_CUSTOM 
#define GPIO_RESET_DEVICE       "/dev/gps_reset"


void set_nShutdown(McpU8 value)
{
    McpBool result;
    static McpS32 gpsreset_fd = -1;
    const char buf[] = { value };   // low level is assert

    if (gpsreset_fd == -1)
    {
      gpsreset_fd = open(GPIO_RESET_DEVICE, O_RDWR);
    }

    if (gpsreset_fd != -1)
    {
        result = (write(gpsreset_fd, buf, 1) ==1);
        if(!result)        
        printf("write to /dev/gps_reset unsuccessful");        
        //close(gpsreset_fd);
    }
    else
    {
        printf("\nunable to open /dev/gps_reset\n");
        perror("error:\n");
    }
}
#endif
/************************************************************************/
/** 
 * \fn     hw_gpio_set
 * \brief  Set the GPIO
 * 
 * This function sets the requested GPIO to the specified value.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	index - GPIO index.
 * \param	value - value to set.
 * \return 	Result of operation: OK or ERROR
 * \sa     	hw_gpio_set
 */
EMcpfRes	hw_gpio_set(handle_t hPla, McpU32 index, McpU8 value)
{
	switch(index)
	{
            case BIT_VDD_CORE:
            break;

#ifndef MCP_STK_ENABLE /* Code is not needed for ST in Kernel mode */
            case BIT_GPS_EN_RESET:

#ifdef CLIENT_CUSTOM
             printf("Entering BIT_GPS_EN_RESET value=%d \n", value);
             if(value == GPIO_POWER_OFF)
                  set_nShutdown(0);
             else if(value == GPIO_POWER_ON)
                  set_nShutdown(1);
             else
		          printf("hw_gpio_set: invalid option\n", value);	  	

#else// for ZOOM2
                printf("Entering BIT_GPS_EN_RESET value=%d \n", value);
            if(value == GPIO_POWER_OFF)
                system("echo 0 > /sys/gpsgpio/nshutdown");
            else if(value == GPIO_POWER_ON)
               system("echo 1 > /sys/gpsgpio/nshutdown");
#endif
            break;
#endif

            case BIT_PA_EN:
            break;

            case BIT_SLEEPX:
            break;

	     case BIT_TIMESTAMP:
            break;

	     case BIT_I2C_UART:
            break;

	     case BIT_RESET_INT:
            break;

            default:
            break;
	}
	MCPF_UNUSED_PARAMETER(hPla);
	return RES_COMPLETE;
}

/** 
 * \fn     hw_refClk_set
 * \brief  Enable/Disable the Reference Clock
 * 
 * This function enables or disables the 
 * reference clock (for clock configuration #4).
 * Note: WinXP on PC application can't control the Ref Clk.
 * 
 * \note
 * \param	hPla - PLA handler. 
 * \param	uIsEnable - Enable/Disable the ref clk.
 * \return 	Result of operation: OK or ERROR
 * \sa     	hw_refClk_set
 */
EMcpfRes	hw_refClk_set(handle_t hPla, McpU8 uIsEnable)
{
	MCPF_UNUSED_PARAMETER(hPla);
	MCPF_UNUSED_PARAMETER(uIsEnable);
	
	return RES_COMPLETE;
}

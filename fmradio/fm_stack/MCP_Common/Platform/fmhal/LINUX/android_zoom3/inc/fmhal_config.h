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


#ifndef __FMHAL_CONFIG_H__
#define __FMHAL_CONFIG_H__

#define FMHAL_OS_MAX_NUM_OF_SEMAPHORES                          (1)

/*
*   The maximum number of FM-Specific Events
*   
*   1 fm process event + 1 timer event + 1 fm interrupt event + 1 fm disable event
*/
#define FMHAL_OS_MAX_NUM_OF_EVENTS_FM                           (4)

/*
*   The maximum number of FM-Specific timers
*   
*   
*/
#define FMHAL_OS_MAX_NUM_OF_TIMERS                              (1) 

/*
*   FMC Server socket path
*
*
*/
#define FMHAL_OS_SERVER_PATH                                    "/mnt/tmp/ti_fm.socket"

#endif /* __FMHAL_CONFIG_H__ */


/*
 *  Copyright 2001-2008 Texas Instruments - http://www.ti.com/
 * 
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


#ifndef __DL_STDLIB__
#define __DL_STDLIB__
/*
 * Standard library functions for DL usage
 *
 * This module is used for systems where the standard library functions are not
 * accessible from the dynamic loader and system-wide alternatives have been 
 * provided
 */

extern unsigned DL__strlen(const char *string);
#define strlen DL__strlen

extern int DL__strcmp(register const char *string1,
												register const char *string2);
#define strcmp DL__strcmp

#endif				/* __DL_STDLIB__ */


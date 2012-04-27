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


#ifndef _DLSTREAM_H
#define _DLSTREAM_H

/*****************************************************************************
 *****************************************************************************
 *
 *								DLSTREAM.H									
 *
 * A class used by the dynamic loader for input of the module image
 *
 *****************************************************************************
 *****************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#include "dynamic_loader.h"

/*****************************************************************************
 * NOTE:	All methods take a 'thisptr' parameter that is a pointer to the
 *			environment for the DL Stream APIs.  See definition of DL_stream_t.
 *
 *****************************************************************************/

/* DL_stream_t extends the DL stream class with buffer management info       */
struct DL_stream_t {
	struct Dynamic_Loader_Stream dstrm;
	unsigned char *mbase;
	uint32_t mcur;
} ;

/******************************************************************************
 * DLstream_open
 *     
 * PARAMETERS :
 *  image		Image to be loaded
 *  imagesize	Number of units to be loaded
 *
 * EFFECT :
 *	Set up the stream with the image to be loaded.  Initialize the pointer 
 *  and size also.
 * 
 *****************************************************************************/
	int DLstream_open(struct DL_stream_t * thisptr, void *image);

/******************************************************************************
 * DLSTREAM_open
 *     
 * PARAMETERS :
 *   None
 *
 * EFFECT :
 *	Do any necessary cleanup for the stream that was loaded.  
 * 
 ******************************************************************************/
	void DLstream_close(struct DL_stream_t * thisptr);

/******************************************************************************
 * DLstream_init
 *     
 * PARAMETERS :
 *   None
 *
 * EFFECT :
 *	Initialize the handlers for DL APIs  
 * 
 ******************************************************************************/
	void DLstream_init(struct DL_stream_t * thisptr);

#ifdef __cplusplus
}
#endif
#endif				/* _DLSTREAM_H */


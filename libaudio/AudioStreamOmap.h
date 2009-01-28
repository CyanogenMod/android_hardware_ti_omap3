/*
 * AudioStreamOutOmap.h
 *
 * Audio output stream handler for OMAP.
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _AUDIO_STREAM_OMAP_H_
#define _AUDIO_STREAM_OMAP_H_

#include <alsa/asoundlib.h>

using namespace android;

struct StreamProperties {
	unsigned int channels;
	snd_pcm_format_t format;
	unsigned int rate;
	snd_pcm_uframes_t buffer_size;
};

#endif

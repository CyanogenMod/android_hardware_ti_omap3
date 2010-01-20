#ifndef __JPEGENCEXIF_H__
#define __JPEGENCEXIF_H__

#include <libexif/exif-entry.h>
#include <libexif/exif-data.h>
#include <libexif/exif-ifd.h>
#include <libexif/exif-loader.h>
#include <string.h>
#include <stdlib.h>

typedef struct _exif_buffer
{
  unsigned char *data;
  unsigned int size;
} exif_buffer;

typedef struct {
    int longDeg, longMin, longSec;
    int latDeg, latMin, latSec;
    int altitude;
    unsigned long timestamp;
} gps_data;

void exif_buf_free (exif_buffer * buf);

exif_buffer *exif_new_buf(unsigned char *data, unsigned int size);

void exif_entry_set_string (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    const char *data);
void exif_entry_set_undefined (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    exif_buffer * buf);

void exif_entry_set_byte (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    ExifByte n);
void exif_entry_set_short (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    ExifShort n);
void exif_entry_set_long (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    ExifLong n);
void exif_entry_set_rational (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    ExifRational r);

void exif_entry_set_sbyte (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    ExifSByte n);
void exif_entry_set_sshort (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    ExifSShort n);
void exif_entry_set_slong (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    ExifSLong n);
void exif_entry_set_srational (ExifData * pEdata, ExifIfd eEifd, ExifTag eEtag,
    ExifSRational r);

exif_buffer * get_exif_buffer(void *gpsLocation);

#endif

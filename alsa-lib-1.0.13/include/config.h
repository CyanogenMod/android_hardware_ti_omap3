/* include/config.h.  Generated from config.h.in by configure.  */
/* include/config.h.in.  Generated from configure.in by autoheader.  */

/* Directory with aload* device files */
#define ALOAD_DEVICE_DIRECTORY "/dev/"

/* Directory with ALSA device files */
#define ALSA_DEVICE_DIRECTORY "/dev/"

/* Build instrument component */
#define BUILD_INSTR "1"

/* Build mixer component */
#define BUILD_MIXER "1"

/* Build PCM component */
#define BUILD_PCM "1"

/* Build PCM adpcm plugin */
#define BUILD_PCM_PLUGIN_ADPCM "1"

/* Build PCM alaw plugin */
#define BUILD_PCM_PLUGIN_ALAW "1"

/* Build PCM lfloat plugin */
#define BUILD_PCM_PLUGIN_LFLOAT "1"

/* Build PCM mulaw plugin */
#define BUILD_PCM_PLUGIN_MULAW "1"

/* Build PCM rate plugin */
#define BUILD_PCM_PLUGIN_RATE "1"

/* Build PCM route plugin */
#define BUILD_PCM_PLUGIN_ROUTE "1"

/* Build raw MIDI component */
#define BUILD_RAWMIDI "1"

/* Build sequencer component */
#define BUILD_SEQ "1"

/* directory containing ALSA configuration database */
#define DATADIR "/etc"

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `resmgr' library (-lresmgr). */
/* #undef HAVE_LIBRESMGR */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Avoid calculation in float */
/* #undef HAVE_SOFT_FLOAT */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <wordexp.h> header file. */
//#define HAVE_WORDEXP_H 1

/* No assert debug */
/* #undef NDEBUG */

/* Name of package */
#define PACKAGE "alsa-lib"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* directory containing ALSA add-on modules */
#define PKGLIBDIR "/usr/lib/alsa-lib"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Support /dev/aload* access for auto-loading */
#define SUPPORT_ALOAD "1"

/* Support resmgr with alsa-lib */
/* #undef SUPPORT_RESMGR */

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* directory to put tmp socket files */
#define TMPDIR "/tmp"

/* sound library version string */
#define VERSION "1.0.13"

/* compiled with versioned symbols */
//#define VERSIONED_SYMBOLS 

/* Toolchain Symbol Prefix */
#define __SYMBOL_PREFIX ""

/* Disable __u8 etc re-definition */
#define HAVE_COMPAT_H 1

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

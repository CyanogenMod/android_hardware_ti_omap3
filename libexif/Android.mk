LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

libexifgnu_DEFINES = -DHAVE_CONFIG -DGETTEXT_PACKAGE=\"libexifgnu-12\" -DLOCALEDIR=\"/share/locale\"

LOCAL_SRC_FILES := \
    ./libexif/exif-mnote-data.c \
    ./libexif/exif-loader.c \
    ./libexif/pentax/mnote-pentax-tag.c \
    ./libexif/pentax/mnote-pentax-entry.c \
    ./libexif/pentax/exif-mnote-data-pentax.c \
    ./libexif/exif-ifd.c \
    ./libexif/exif-byte-order.c \
    ./libexif/exif-content.c \
    ./libexif/fuji/exif-mnote-data-fuji.c \
    ./libexif/fuji/mnote-fuji-tag.c \
    ./libexif/fuji/mnote-fuji-entry.c \
    ./libexif/exif-tag.c \
    ./libexif/canon/exif-mnote-data-canon.c \
    ./libexif/canon/mnote-canon-tag.c \
    ./libexif/canon/mnote-canon-entry.c \
    ./libexif/exif-log.c \
    ./libexif/exif-data.c \
    ./libexif/exif-entry.c \
    ./libexif/olympus/mnote-olympus-entry.c \
    ./libexif/olympus/exif-mnote-data-olympus.c \
    ./libexif/olympus/mnote-olympus-tag.c \
    ./libexif/exif-mem.c \
    ./libexif/exif-utils.c \
    ./libexif/exif-format.c

LOCAL_C_INCLUDES += $(LOCAL_PATH)/libexif

LOCAL_CFLAGS += -Wall $(libexifgnu_DEFINES) -g -O2
LOCAL_CPPFLAGS += -Wall $(libexifgnu_DEFINES) -g -O2

LOCAL_MODULE:= libexifgnu
LOCAL_MODULE_TAGS:= optional

include $(BUILD_STATIC_LIBRARY)

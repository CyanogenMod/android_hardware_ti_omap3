/***************************************************************************
 *
 * File:
 *     iAnywhere Data Sync SDK
 *     $Id:vp_types.h,v 1.15, 2006-01-13 04:40:47Z, Kevin Hendrix$
 *
 * Description:
 *     Definition of vParse Parser functions and status values.
 *
 * Copyright 2003-2006 Extended Systems, Inc.
 * Portions copyright 2006 iAnywhere Solutions, Inc.
 * All rights reserved. All unpublished rights reserved.
 *
 * Unpublished Confidential Information of iAnywhere Solutions, Inc.  
 * Do Not Disclose.
 *
 * No part of this work may be used or reproduced in any form or by any 
 * means, or stored in a database or retrieval system, without prior written 
 * permission of iAnywhere Solutions, Inc.
 * 
 * Use of this work is governed by a license granted by iAnywhere Solutions, 
 * Inc.  This work contains confidential and proprietary information of 
 * iAnywhere Solutions, Inc. which is protected by copyright, trade secret, 
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/

#ifndef __VP_TYPES_H
#define __VP_TYPES_H

#include "xatypes.h"

/*---------------------------------------------------------------------------
 * vObjectType type
 */
typedef U16 vObjectType;

#define VCARD       0x0001
#define VNOTE       0x0002
#define VCALENDAR   0x0004
#define VEVENT      0x0008
#define VTODO       0x0010
#define VALARM      0x0020
#define VFREEBUSY   0x0040
#define VJOURNAL    0x0080
#define VTIMEZONE   0x0100
#define STANDARD    0x0200
#define DAYLIGHT    0x0400
/* End of vObjectType */
                      
/* The object types must be in the same order as in vObjectType */
#define VOBJECT_TYPES {\
    "VCARD",        /* VCARD       0x0001 */ \
    "VNOTE",        /* VNOTE       0x0002 */ \
    "VCALENDAR",    /* VCALENDAR   0x0004 */ \
    "VEVENT",       /* VEVENT      0x0008 */ \
    "VTODO",        /* VTODO       0x0010 */ \
    "VALARM",       /* VALARM      0x0020 */ \
    "VFREEBUSY",    /* VFREEBUSY   0x0040 */ \
    "VJOURNAL",     /* VJOURNAL    0x0080 */ \
    "VTIMEZONE",    /* VTIMEZONE   0x0100 */ \
    "STANDARD",     /* STANDARD    0x0200 */ \
    "DAYLIGHT",     /* DAYLIGHT    0x0400 */ \
    0 \
}

/*---------------------------------------------------------------------------
 * vCardType type
 *      vCard property types.
 */
typedef U8 vCardType;

#define VCARD_FN           0x00
#define VCARD_N            0x01
#define VCARD_NICKNAME     0x02
#define VCARD_PHOTO        0x03
#define VCARD_BDAY         0x04
#define VCARD_ADR          0x05
#define VCARD_LABEL        0x06
#define VCARD_TEL          0x07
#define VCARD_EMAIL        0x08
#define VCARD_MAILER       0x09
#define VCARD_TZ           0x0a
#define VCARD_GEO          0x0b
#define VCARD_TITLE        0x0c
#define VCARD_ROLE         0x0d
#define VCARD_LOGO         0x0e
#define VCARD_AGENT        0x0f
#define VCARD_ORG          0x10
#define VCARD_CATEGORIES   0x11
#define VCARD_NOTE         0x12
#define VCARD_PRODID       0x13
#define VCARD_REV          0x14
#define VCARD_SORTSTRING   0x15
#define VCARD_SOUND        0x16
#define VCARD_UID          0x17
#define VCARD_URL          0x18
#define VCARD_VERSION      0x19
#define VCARD_CLASS        0x1a
#define VCARD_KEY          0x1b
#define VCARD_LOCATION     0x1c
#define X_ESI_CUST1         0x1d
#define X_ESI_CUST2         0x1e
#define X_ESI_CUST3         0x1f
#define X_ESI_CUST4         0x20
#define X_ESI_ASSIST        0x21
#define X_ESI_SPOUSE        0x22
#define X_ESI_CHILDREN      0x23
#define X_ESI_ANNI          0x24
#define X_ESI_PROFESSION    0x25
#define X_ESI_NICK          0x26
#define X_ESI_MANAGER       0x27
#define X_ESI_REFERREDBY    0x28
#define X_ESI_HOBBY         0x29
#define X_ESI_ACCOUNT       0x2a
#define X_ESI_FTP           0x2b
#define X_ESI_MILEAGE       0x2c
#define X_ESI_BILLING       0x2d
#define X_ESI_ORGID         0x2e
#define X_ESI_GOVID         0x2f
#define X_ESI_CUSTID        0x30
#define X_ESI_COMPNAME      0x31
#define X_ESI_NONE          0xff
/* End of vCardType */

/* The vCard property types must be in the same order as in vCardType */
#if VP_ESI_PROPERTIES == XA_ENABLED
#define VCARD_TYPES {\
    "FN",           /* VCARD_FN           0x00 */ \
    "N",            /* VCARD_N            0x01 */ \
    "NICKNAME",     /* VCARD_NICKNAME     0x02 */ \
    "PHOTO",        /* VCARD_PHOTO        0x03 */ \
    "BDAY",         /* VCARD_BDAY         0x04 */ \
    "ADR",          /* VCARD_ADR          0x05 */ \
    "LABEL",        /* VCARD_LABEL        0x06 */ \
    "TEL",          /* VCARD_TEL          0x07 */ \
    "EMAIL",        /* VCARD_EMAIL        0x08 */ \
    "MAILER",       /* VCARD_MAILER       0x09 */ \
    "TZ",           /* VCARD_TZ           0x0a */ \
    "GEO",          /* VCARD_GEO          0x0b */ \
    "TITLE",        /* VCARD_TITLE        0x0c */ \
    "ROLE",         /* VCARD_ROLE         0x0d */ \
    "LOGO",         /* VCARD_LOGO         0x0e */ \
    "AGENT",        /* VCARD_AGENT        0x0f */ \
    "ORG",          /* VCARD_ORG          0x10 */ \
    "CATEGORIES",   /* VCARD_CATEGORIES   0x11 */ \
    "NOTE",         /* VCARD_NOTE         0x12 */ \
    "PRODID",       /* VCARD_PRODID       0x13 */ \
    "REV",          /* VCARD_REV          0x14 */ \
    "SORT-STRING",  /* VCARD_SORTSTRING   0x15 */ \
    "SOUND",        /* VCARD_SOUND        0x16 */ \
    "UID",          /* VCARD_UID          0x17 */ \
    "URL",          /* VCARD_URL          0x18 */ \
    "VERSION",      /* VCARD_VERSION      0x19 */ \
    "CLASS",        /* VCARD_CLASS        0x1a */ \
    "KEY",          /* VCARD_KEY          0x1b */ \
    "LOCATION",     /* VCARD_LOCATION     0x1c */ \
    "X-ESI-CUST1",      /* X_ESI_CUST1      0x1d */ \
    "X-ESI-CUST2",      /* X_ESI_CUST2      0x1e */ \
    "X-ESI-CUST3",      /* X_ESI_CUST3      0x1f */ \
    "X-ESI-CUST4",      /* X_ESI_CUST4      0x20 */ \
    "X-ESI-ASSIST",     /* X_ESI_ASSIST     0x21 */ \
    "X-ESI-SPOUSE",     /* X_ESI_SPOUSE     0x22 */ \
    "X-ESI-CHILDREN",   /* X_ESI_CHILDREN   0x23 */ \
    "X-ESI-ANNI",       /* X_ESI_ANNI       0x24 */ \
    "X-ESI-PROFESSION", /* X_ESI_PROFESSION 0x25 */ \
    "X-ESI-NICK",       /* X_ESI_NICK       0x26 */ \
    "X-ESI-MANAGER",    /* X_ESI_MANAGER    0x27 */ \
    "X-ESI-REFERREDBY", /* X_ESI_REFERREDBY 0x28 */ \
    "X-ESI-HOBBY",      /* X_ESI_HOBBY      0x29 */ \
    "X-ESI-ACCOUNT",    /* X_ESI_ACCOUNT    0x2a */ \
    "X-ESI-FTP",        /* X_ESI_FTP        0x2b */ \
    "X-ESI-MILEAGE",    /* X_ESI_MILEAGE    0x2c */ \
    "X-ESI-BILLING",    /* X_ESI_BILLING    0x2d */ \
    "X-ESI-ORGID",      /* X_ESI_ORGID      0x2e */ \
    "X-ESI-GOVID",      /* X_ESI_GOVID      0x2f */ \
    "X-ESI-CUSTID",     /* X_ESI_CUSTID     0x30 */ \
    "X-ESI-COMPNAME",   /* X_ESI_COMPNAME   0x31 */ \
    "X-ESI-NONE",       /* X_ESI_NONE       0xff */ \
    0 \
}
#else /* VP_ESI_PROPERTIES == XA_ENABLED */
#define VCARD_TYPES {\
    "FN",           /* VCARD_FN           0x00 */ \
    "N",            /* VCARD_N            0x01 */ \
    "NICKNAME",     /* VCARD_NICKNAME     0x02 */ \
    "PHOTO",        /* VCARD_PHOTO        0x03 */ \
    "BDAY",         /* VCARD_BDAY         0x04 */ \
    "ADR",          /* VCARD_ADR          0x05 */ \
    "LABEL",        /* VCARD_LABEL        0x06 */ \
    "TEL",          /* VCARD_TEL          0x07 */ \
    "EMAIL",        /* VCARD_EMAIL        0x08 */ \
    "MAILER",       /* VCARD_MAILER       0x09 */ \
    "TZ",           /* VCARD_TZ           0x0a */ \
    "GEO",          /* VCARD_GEO          0x0b */ \
    "TITLE",        /* VCARD_TITLE        0x0c */ \
    "ROLE",         /* VCARD_ROLE         0x0d */ \
    "LOGO",         /* VCARD_LOGO         0x0e */ \
    "AGENT",        /* VCARD_AGENT        0x0f */ \
    "ORG",          /* VCARD_ORG          0x10 */ \
    "CATEGORIES",   /* VCARD_CATEGORIES   0x11 */ \
    "NOTE",         /* VCARD_NOTE         0x12 */ \
    "PRODID",       /* VCARD_PRODID       0x13 */ \
    "REV",          /* VCARD_REV          0x14 */ \
    "SORT-STRING",  /* VCARD_SORTSTRING   0x15 */ \
    "SOUND",        /* VCARD_SOUND        0x16 */ \
    "UID",          /* VCARD_UID          0x17 */ \
    "URL",          /* VCARD_URL          0x18 */ \
    "VERSION",      /* VCARD_VERSION      0x19 */ \
    "CLASS",        /* VCARD_CLASS        0x1a */ \
    "KEY",          /* VCARD_KEY          0x1b */ \
    "LOCATION",     /* VCARD_LOCATION     0x1c */ \
    0 \
}
#endif /* VP_ESI_PROPERTIES == XA_ENABLED */

/*---------------------------------------------------------------------------
 * vQuotedPrintableType type
 */
typedef U16  vQuotedPrintableType;

#define VCARD_PM_QUOTEDPRINTABLE    0x0001
/* End of vQuotedPrintableType */

#define VGLOBAL_TYPES {\
    "QUOTED-PRINTABLE", \
    0 \
}

/*---------------------------------------------------------------------------
 * vNParts type
 *      N part types.
 */
typedef U16  vNParts;

#define VCARD_N_PT_LAST       0x0000
#define VCARD_N_PT_FIRST      0x0001
#define VCARD_N_PT_MIDDLE     0x0002
#define VCARD_N_PT_TITLE      0x0003
#define VCARD_N_PT_SUFFIX     0x0004
/* End of vNParts */

/*---------------------------------------------------------------------------
 * vTelType type
 *      TEL parameter types.
 */
typedef U16  vTelType;

#define VCARD_TEL_QUOTEDPRINTABLE 0x0001
#define VCARD_TEL_PM_HOME         0x0002
#define VCARD_TEL_PM_MSG          0x0004
#define VCARD_TEL_PM_WORK         0x0008
#define VCARD_TEL_PM_PREF         0x0010
#define VCARD_TEL_PM_VOICE        0x0020
#define VCARD_TEL_PM_FAX          0x0040
#define VCARD_TEL_PM_CELL         0x0080
#define VCARD_TEL_PM_TLX          0x0100
#define VCARD_TEL_PM_PAGER        0x0200
#define VCARD_TEL_PM_CAR          0x0400
#define VCARD_TEL_PM_ISDN         0x0800
/* #if VP_ESI_PROPERTIES == XA_ENABLED */
#define VCARD_TEL_PM_X_ESI_ASSIST 0x1000
#define VCARD_TEL_PM_X_ESI_RADIO  0x2000
#define VCARD_TEL_PM_X_ESI_TTYTDD 0x4000
#define VCARD_TEL_PM_X_ESI_OTHER  0x8000
/* #else VP_ESI_PROPERTIES == XA_ENABLED */
#define VCARD_TEL_PM_VIDEO        0x1000
#define VCARD_TEL_PM_BBS          0x2000
#define VCARD_TEL_PM_X_MODEM      0x4000
#define VCARD_TEL_PM_X_PCS        0x8000
/* #endif VP_ESI_PROPERTIES == XA_ENABLED */
/* End of vTelType */

/* The telephone parameter types must be in the same order as in vTelType
 *    "VIDEO", "BBS", "MODEM" and "PCS" are not represented when ESI
 *    properties are enabled.
 */
#if VP_ESI_PROPERTIES == XA_ENABLED
#define VCARD_TELTYPES {\
    "QUOTED-PRINTABLE", /* 0x0001 is reserved for QUOTED-PRINTABLE */ \
    "HOME",         /* VCARD_TEL_PM_HOME     0x0002 */ \
    "MSG",          /* VCARD_TEL_PM_MSG      0x0004 */ \
    "WORK",         /* VCARD_TEL_PM_WORK     0x0008 */ \
    "PREF",         /* VCARD_TEL_PM_PREF     0x0010 */ \
    "VOICE",        /* VCARD_TEL_PM_VOICE    0x0020 */ \
    "FAX",          /* VCARD_TEL_PM_FAX      0x0040 */ \
    "CELL",         /* VCARD_TEL_PM_CELL     0x0080 */ \
    "TLX",          /* VCARD_TEL_PM_TLX      0x0100 */ \
    "PAGER",        /* VCARD_TEL_PM_PAGER    0x0200 */ \
    "CAR",          /* VCARD_TEL_PM_CAR      0x0400 */ \
    "ISDN",         /* VCARD_TEL_PM_ISDN     0x0800 */ \
    "X-ESI-ASSIST", /* VCARD_TEL_PM_X_ESI_ASSIST 0x1000 */ \
    "X-ESI-RADIO",  /* VCARD_TEL_PM_X_ESI_RADIO  0x2000 */ \
    "X-ESI-TTYTDD", /* VCARD_TEL_PM_X_ESI_TTYTDD 0x4000 */ \
    "X-ESI-OTHER",  /* VCARD_TEL_PM_X_ESI_OTHER  0x8000 */ \
    0 \
}
#else /* VP_ESI_PROPERTIES == XA_ENABLED */
#define VCARD_TELTYPES {\
    "QUOTED-PRINTABLE", /* 0x0001 is reserved for QUOTED-PRINTABLE */ \
    "HOME",         /* VCARD_TEL_PM_HOME     0x0002 */ \
    "MSG",          /* VCARD_TEL_PM_MSG      0x0004 */ \
    "WORK",         /* VCARD_TEL_PM_WORK     0x0008 */ \
    "PREF",         /* VCARD_TEL_PM_PREF     0x0010 */ \
    "VOICE",        /* VCARD_TEL_PM_VOICE    0x0020 */ \
    "FAX",          /* VCARD_TEL_PM_FAX      0x0040 */ \
    "CELL",         /* VCARD_TEL_PM_CELL     0x0080 */ \
    "TLX",          /* VCARD_TEL_PM_TLX      0x0100 */ \
    "PAGER",        /* VCARD_TEL_PM_PAGER    0x0200 */ \
    "CAR",          /* VCARD_TEL_PM_CAR      0x0400 */ \
    "ISDN",         /* VCARD_TEL_PM_ISDN     0x0800 */ \
    "VIDEO",        /* VCARD_TEL_PM_VIDEO    0x1000 */ \
    "BBS",          /* VCARD_TEL_PM_BBS      0x2000 */ \
    "MODEM",        /* VCARD_TEL_PM_X_MODEM  0x4000 */ \
    "PCS",          /* VCARD_TEL_PM_X_PCS    0x8000 */ \
    0 \
}
#endif /* VP_ESI_PROPERTIES == XA_ENABLED */

/*---------------------------------------------------------------------------
 * vAdrParts type
 *      ADR part types.
 */
typedef U16  vAdrParts;

#define VCARD_ADR_PT_POST     0x0000
#define VCARD_ADR_PT_EXTENDED 0x0001
#define VCARD_ADR_PT_STREET   0x0002
#define VCARD_ADR_PT_CITY     0x0003
#define VCARD_ADR_PT_STATE    0x0004
#define VCARD_ADR_PT_ZIP      0x0005
#define VCARD_ADR_PT_COUNTRY  0x0006
/* End of vAdrParts */

/*---------------------------------------------------------------------------
 * vAdrType type
 *      ADR parameter types.
 */
typedef U16  vAdrType;

#define VCARD_ADR_QUOTEDPRINTABLE 0x0001
#define VCARD_ADR_PM_DOM          0x0002
#define VCARD_ADR_PM_INTL         0x0004
#define VCARD_ADR_PM_POSTAL       0x0008
#define VCARD_ADR_PM_PARCEL       0x0010
#define VCARD_ADR_PM_HOME         0x0020
#define VCARD_ADR_PM_WORK         0x0040
#define VCARD_ADR_PM_PREF         0x0080
#define VCARD_ADR_PM_X_ESI_OTHER  0x0100
/* End of vAdrType */

/* The address parameter types must be in the same order as in vAdrType */
#define VCARD_ADRTYPES {\
    "QUOTED-PRINTABLE", \
    "DOM",          /* VCARD_ADR_PM_DOM      0x0002 */ \
    "INTL",         /* VCARD_ADR_PM_INTL     0x0004 */ \
    "POSTAL",       /* VCARD_ADR_PM_POSTAL   0x0008 */ \
    "PARCEL",       /* VCARD_ADR_PM_PARCEL   0x0010 */ \
    "HOME",         /* VCARD_ADR_PM_HOME     0x0020 */ \
    "WORK",         /* VCARD_ADR_PM_WORK     0x0040 */ \
    "PREF",         /* VCARD_ADR_PM_PREF     0x0080 */ \
    "X-ESI-OTHER",  /* VCARD_ADR_PM_X_ESI_OTHER 0x0100 */ \
    0 \
}

/*---------------------------------------------------------------------------
 * vEmailType type
 *      Email parameter types.
 */
typedef U16  vEmailType;

#define VCARD_EML_QUOTEDPRINTABLE 0x0001
#define VCARD_EML_PM_HOME         0x0002
#define VCARD_EML_PM_WORK         0x0004
#define VCARD_EML_PM_PREF         0x0008
#define VCARD_EML_PM_X400         0x0010
#define VCARD_EML_PM_INTERNET     0x0020
#define VCARD_EML_PM_X_ESI_OTHER  0x0040
/* End of vEmailType */

/* The Email parameter types must be in the same order as in vEmailType */
#define VCARD_EMLTYPES {\
    "QUOTED-PRINTABLE", \
    "HOME",         /* VCARD_EML_PM_HOME        0x0002 */ \
    "WORK",         /* VCARD_EML_PM_WORK        0x0004 */ \
    "PREF",         /* VCARD_EML_PM_PREF        0x0008 */ \
    "X400",         /* VCARD_EML_PM_PREF        0x0010 */ \
    "INTERNET",     /* VCARD_EML_PM_PREF        0x0020 */ \
    "X-ESI-OTHER",  /* VCARD_EML_PM_X_ESI_OTHER 0x0040 */ \
    0 \
}

/*---------------------------------------------------------------------------
 * vOrgParts type
 *      ORG part types.
 */
typedef U16  vOrgParts;

#define VCARD_ORG_PT_COMPANY        0x0000
#define VCARD_ORG_PT_DEPARTMENT     0x0001
/* End of vOrgParts */

/*---------------------------------------------------------------------------
 * vUrlType type
 *      URL parameter types.
 */
typedef U16  vUrlType;

#define VCARD_URL_QUOTEDPRINTABLE   0x0001
#define VCARD_URL_PM_HOME           0x0002
#define VCARD_URL_PM_WORK           0x0004
#define VCARD_URL_PM_PREF           0x0008
#define VCARD_URL_PM_X400           0x0010
#define VCARD_URL_PM_INTERNET       0x0020
/* End of vUrlType */

/* The Email parameter types must be in the same order as in vEmailType */
#define VCARD_URLTYPES {\
    "QUOTED-PRINTABLE", \
    "HOME",         /* VCARD_URL_PM_HOME     0x0002 */ \
    "WORK",         /* VCARD_URL_PM_WORK     0x0004 */ \
    "PREF",         /* VCARD_URL_PM_PREF     0x0008 */ \
    "X400",         /* VCARD_URL_PM_PREF     0x0010 */ \
    "INTERNET",     /* VCARD_URL_PM_PREF     0x0020 */ \
    0 \
}

/*---------------------------------------------------------------------------
 * vNoteType type
 *      vNote property types.
 */
typedef U8  vNoteType;

#define VNOTE_DCREATED        0x00
#define VNOTE_SUBJECT         0x01
#define VNOTE_BODY            0x02
#define VNOTE_CATEGORIES      0x03
#define VNOTE_LAST_MODIFIED   0x04
#define VNOTE_LAST_CLASS      0x05
#define VNOTE_VERSION         0x06
/* End of vNoteType */

/* The Note property types must be in the same order as in vNoteType */
#define VNOTE_TYPES {\
    "DCREATED",         /* VNOTE_DCREATED        0x00 */ \
    "SUBJECT",          /* VNOTE_SUBJECT         0x01 */ \
    "BODY",             /* VNOTE_BODY            0x02 */ \
    "CATEGORIES",       /* VNOTE_CATEGORIES      0x03 */ \
    "LAST-MODIFIED",    /* VNOTE_LAST_MODIFIED   0x04 */ \
    "CLASS",            /* VNOTE_LAST_CLASS      0x05 */ \
    "VERSION",          /* VNOTE_VERSION         0x06 */ \
    0 \
}

/*---------------------------------------------------------------------------
 * vToDoType type
 *      vToDo property types.
 */
typedef U8  vToDoType;

#define VTODO_SUMMARY         0x00
#define VTODO_DUE             0x01
#define VTODO_COMPLETED       0x02
#define VTODO_PRIORITY        0x03
#define VTODO_CLASS           0x04
#define VTODO_CATEGORIES      0x05
#define VTODO_DESCRIPTION     0x06
#define VTODO_DTSTART         0x07
#define VTODO_DALARM          0x08
#define VTODO_VERSION         0x09
#define VTODO_EXDATE          0x0a
#define VTODO_RRULE           0x0b
/* End of vToDoType */

/* The ToDo property types must be in the same order as in vToDoType */
#define VCAL_TODOTYPES {\
    "SUMMARY",          /* VTODO_SUMMARY         0x00 */ \
    "DUE",              /* VTODO_DUE             0x01 */ \
    "COMPLETED",        /* VTODO_COMPLETED       0x02 */ \
    "PRIORITY",         /* VTODO_PRIORITY        0x03 */ \
    "CLASS",            /* VTODO_CLASS           0x04 */ \
    "CATEGORIES",       /* VTODO_CATEGORIES      0x05 */ \
    "DESCRIPTION",      /* VTODO_DESCRIPTION     0x06 */ \
    "DTSTART",          /* VTODO_DTSTART         0x07 */ \
    "DALARM",           /* VTODO_DALARM          0x08 */ \
    "VERSION",          /* VTODO_VERSION         0x09 */ \
    "EXDATE",           /* VTODO_EXDATE          0x0a */ \
    "RRULE",            /* VTODO_RRULE           0x0b */ \
    0 \
}

/*---------------------------------------------------------------------------
 * vEventType type
 *      vEvent property types.
 */
typedef U8  vEventType;

#define VEVENT_DTSTART        0x00
#define VEVENT_DTEND          0x01
#define VEVENT_SUMMARY        0x02
#define VEVENT_DALARM         0x03
#define VEVENT_CATEGORIES     0x04
#define VEVENT_LOCATION       0x05
#define VEVENT_DESCRIPTION    0x06
#define VEVENT_TRANSP         0x07
#define VEVENT_CLASS          0x08
#define VEVENT_VERSION        0x09
#define VEVENT_EXDATE         0x0a
#define VEVENT_RRULE          0x0b
/* End of vEventType */

/* The Event property types must be in the same order as in vEventType */
#define VCAL_EVENTTYPES {\
    "DTSTART",          /* VEVENT_DTSTART        0x00 */ \
    "DTEND",            /* VEVENT_DTEND          0x01 */ \
    "SUMMARY",          /* VEVENT_SUMMARY        0x02 */ \
    "DALARM",           /* VEVENT_DALARM         0x03 */ \
    "CATEGORIES",       /* VEVENT_CATEGORIES     0x04 */ \
    "LOCATION",         /* VEVENT_LOCATION       0x05 */ \
    "DESCRIPTION",      /* VEVENT_DESCRIPTION    0x06 */ \
    "TRANSP",           /* VEVENT_TRANSP         0x07 */ \
    "CLASS",            /* VEVENT_CLASS          0x08 */ \
    "VERSION",          /* VEVENT_VERSION        0x09 */ \
    "EXDATE",           /* VEVENT_EXDATE         0x0a */ \
    "RRULE",            /* VEVENT_RRULE          0x0b */ \
    0 \
}

#endif /* __VP_TYPES_H */

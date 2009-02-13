/***************************************************************************
 *
 * File:
 *     iAnywhere Data Sync SDK
 *     $Id:vParser.h,v 1.31, 2006-02-03 01:10:11Z, Kevin Hendrix$
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

#ifndef __VPARSER_H
#define __VPARSER_H

#include "vp_types.h"
#include "vobject.h"
#include "xastatus.h"
#if SML_DATASYNC == XA_ENABLED
#include "smlconfig.h"
#endif /* SML_DATASYNC == XA_ENABLED */

/*---------------------------------------------------------------------------
 * vParser Component
 *
 *     vParser extends the vObject parsing and building interfaces provided
 *     by the vObject Parser and Builder library in the XTNDAccess SyncML
 *     SDK API. This release focuses on support for XCPC implementations,
 *     which are restricted to vCard 2.1 and vCalendar 1.0. Other
 *     vObject-based objects will be accepted, though they will be reduced
 *     to vCard 2.1 and vCalendar 1.0 XCPC compatible formats.
 */

/****************************************************************************
 *
 * Constants
 *
 ****************************************************************************/

/*----------------------------------------------------------------------------
 * VP_ESI_PROPERTIES constant
 *
 *     This constant indicates whether ESI XCPC properties will be used.
 */
#ifndef VP_ESI_PROPERTIES
#define VP_ESI_PROPERTIES   XA_ENABLED
#endif

/*----------------------------------------------------------------------------
 * VP_CHARSET_LEN constant
 *
 *     This constant specifies the maximum length of the CHARSET field of
 *     an object.
 */
#ifndef VP_CHARSET_LEN
#define VP_CHARSET_LEN      25
#endif

#if VP_CHARSET_LEN < 20
#error "VP_CHARSET_LEN must be 20 or greater"
#endif


/****************************************************************************
 *
 * Types
 *
 ****************************************************************************/

/*----------------------------------------------------------------------------
 * VpFlags type
 *
 *     These flags dictate which set of fields can be built in the case of
 *     a SyncML field-level replace exchange.  If allowed, all fields that
 *     have changed should be synced, but it restrictions exist on the remote
 *     device, a subset of these fields can be specified.  If field-level 
 *     replace is not used, all of the fields in the record will be sent, 
 *     as per the usual behavior.
 */
typedef U32 VpFlags;

/* Field-Level Replace disabled.  All fields in the vObject will be
 * sent according to the normal processing.
 */
#define VPF_FLR_OFF                 0x00000000

/* Use Field-Level Replace on version field. */
#define VPF_FLR_VERSION_FIELD       0x00000001

/* Use Field-Level Replace on name field. */
#define VPF_FLR_NAME_FIELD          0x00000002

/* Use Field-Level Replace on title field. */
#define VPF_FLR_TITLE_FIELD         0x00000004

/* Use Field-Level Replace on address field. */
#define VPF_FLR_ADDRESS_FIELD       0x00000008

/* Use Field-Level Replace on organization field. */
#define VPF_FLR_ORGANIZATION_FIELD  0x00000010

/* Use Field-Level Replace on telephone field. */
#define VPF_FLR_TELEPHONE_FIELD     0x00000020

/* Use Field-Level Replace on telephone field. */
#define VPF_FLR_EMAIL_FIELD         0x00000040

/* Use Field-Level Replace on note field. */
#define VPF_FLR_NOTE_FIELD          0x00000080

/* Use Field-Level Replace on categories field. */
#define VPF_FLR_CATEGORIES_FIELD    0x00000100

/* Use Field-Level Replace on class field. */
#define VPF_FLR_CLASS_FIELD         0x00000100

/* Use Field-Level Replace on location field. */
#define VPF_FLR_LOCATION_FIELD      0x00000200

/* Use Field-Level Replace on birthday field. */
#define VPF_FLR_BIRTHDAY_FIELD      0x00000400

/* Use Field-Level Replace on url field. */
#define VPF_FLR_URL_FIELD           0x00000800

/* Use Field-Level Replace on subject field. */
#define VPF_FLR_SUBJECT_FIELD       0x00001000

/* Use Field-Level Replace on body field. */
#define VPF_FLR_BODY_FIELD          0x00002000

/* Use Field-Level Replace on created field. */
#define VPF_FLR_CREATED_FIELD       0x00004000

/* Use Field-Level Replace on created field. */
#define VPF_FLR_LAST_MODIFIED_FIELD 0x00008000

/* Use Field-Level Replace on summary field. */
#define VPF_FLR_SUMMARY_FIELD       0x00010000

/* Use Field-Level Replace on due field. */
#define VPF_FLR_DUE_FIELD           0x00020000

/* Use Field-Level Replace on completed field. */
#define VPF_FLR_COMPLETED_FIELD     0x00040000

/* Use Field-Level Replace on description field. */
#define VPF_FLR_DESCRIPTION_FIELD   0x00080000

/* Use Field-Level Replace on priority field. */
#define VPF_FLR_PRIORITY_FIELD      0x00100000

/* Use Field-Level Replace on alarm field. */
#define VPF_FLR_ALARM_FIELD         0x00200000

/* Use Field-Level Replace on start date field. */
#define VPF_FLR_START_DATE_FIELD    0x00400000

/* Use Field-Level Replace on end date field. */
#define VPF_FLR_END_DATE_FIELD      0x01000000

/* Use Field-Level Replace on exdate field. */
#define VPF_FLR_EXDATE_FIELD        0x01000000

/* Use Field-Level Replace on rrule field. */
#define VPF_FLR_RRULE_FIELD         0x02000000

/* Use Field-Level Replace on transp field. */
#define VPF_FLR_TRANSP_FIELD        0x04000000

/* Use Field-Level Replace for all fields */
#define VPF_FLR_ALL_FIELDS          0xFFFFFFFF

/* End of VpFlags */

/****************************************************************************
 *
 * Data Structures
 *
 ****************************************************************************/

/*----------------------------------------------------------------------------
 * Prop structure
 *
 *     Indicates parameters, length and offset of each entry in SyncObject
 *     structures.
 */
typedef struct _Prop {
    U16     parm;       /* Parameters for entry */
    U16     len;        /* Length of entry value (minus null termination.) */
    U16     offset;     /* Offset of entry value from beginning of SyncObject. */
#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
    BOOL    modified;   /* Tracks per-property modifications */
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
} Prop;

/*----------------------------------------------------------------------------
 * SyncPhoneBook structure
 *
 *     Defines the format of a PhoneBook SyncObject record.
 */
typedef struct _SyncPhoneBook {
    U8    header;                   /* Single character header for DB   */
    U8    dbVersion;                /* Indicates version of DataBase    */
    BOOL  objectComplete;           /* Indicates object parsed to end   */
    U8    charset[VP_CHARSET_LEN];  /* CHARSET of PhoneBook             */
    void *next;                     /* Pointer to next SyncPhoneBook    */
    Prop  fn;                           /* FN                                    */
    Prop  version;                  /* vCard version                    */
    Prop  lname;                    /* N:LastName;;;;                   */        
    Prop  fname;                    /* N:;FirstName;;;                  */
    Prop  job_position;             /* TITLE                            */
    Prop  company;                  /* ORG:Company;Department           */
    Prop  business_street;          /* ADR;WORK:;;Street;;;;            */
    Prop  business_city;            /* ADR;WORK:;;;City;;;              */
    Prop  business_state;           /* ADR;WORK:;;;;State;;             */
    Prop  business_zip;             /* ADR;WORK:;;;;;Zip;               */
    Prop  business_country;         /* ADR;WORK:;;;;;;Country           */
    Prop  business_phone;           /* TEL;VOICE;WORK                   */
    Prop  home_phone;               /* TEL;VOICE;HOME                   */
    Prop  business_fax;             /* TEL;FAX;WORK                     */
    Prop  other_phone;              /*                                  */
    Prop  business_email;           /* EMAIL;WORK                       */
    Prop  phone_primary;            /*                                  */
    Prop  phone_pager;              /* TEL;PAGER                        */
    Prop  phone_mobile;             /* TEL;CELL                         */
    Prop  note;                     /* NOTE                             */
    Prop  confidential;             /* CLASS                            */
    Prop  category;                 /* CATEGORIES                       */
    Prop  custom1;                  /* X-ESI-CUST1                      */
    Prop  custom2;                  /* X-ESI-CUST2                      */
    Prop  custom3;                  /* X-ESI-CUST3                      */
    Prop  custom4;                  /* X-ESI-CUST4                      */
    Prop  title;                    /* N:;;;Title;                      */
    Prop  department;               /* ORG:Company;Department           */
    Prop  business_phone2;          /* TEL;VOICE;WORK (2nd occurrence)  */
    Prop  business_web;             /* URL;WORK                         */
    Prop  assistant;                /* X-ESI-ASSIST                     */
    Prop  home_fax;                 /* TEL;FAX;HOME                     */
    Prop  home_phone2;              /* TEL;VOICE;HOME (2nd occurrence)  */
    Prop  home_street;              /* ADR;HOME:;;Street;;;;            */
    Prop  home_city;                /* ADR;HOME:;;;City;;;              */
    Prop  home_state;               /* ADR;HOME:;;;;State;;             */
    Prop  home_zip;                 /* ADR;HOME:;;;;;Zip;               */
    Prop  home_country;             /* ADR;HOME:;;;;;;Country           */
    Prop  home_email;               /* EMAIL;HOME                       */
    Prop  home_spouse;              /* X-ESI-SPOUSE                     */
    Prop  home_children;            /* X-ESI-CHILDREN                   */
    Prop  mname;                    /* N:;;MiddleName;;                 */
    Prop  suffix;                   /* N:;;;;Suffix                     */
    Prop  location;                 /* LOCATION                         */
    Prop  birthday;                 /* BDAY                             */
    Prop  anniversary;              /* X-ESI-ANNI                       */
    Prop  phone_car;                /* TEL;CAR                          */
    Prop  other_email;              /* EMAIL;X-ESI-OTHER                */
    Prop  phone_assistant;          /* TEL;VOICE;X-ESI-ASSIST           */
    Prop  other_street;             /* ADR;X-ESI-OTHER:;;Street;;;;     */
    Prop  other_city;               /* ADR;X-ESI-OTHER:;;;City;;;       */
    Prop  other_state;              /* ADR;X-ESI-OTHER:;;;;State;;      */
    Prop  other_zip;                /* ADR;X-ESI-OTHER:;;;;;Zip;        */
    Prop  other_country;            /* ADR;X-ESI-OTHER:;;;;;;Country    */
    Prop  phone_radio;              /* TEL;X-ESI-RADIO                  */
    Prop  phone_tele;               /* TEL;TLX                          */
    Prop  phone_isdn;               /* TEL;ISDN                         */
    Prop  phone_tty_tdd;            /* TEL;X-ESI-TTYTDD                 */
    Prop  other_fax;                /* TEL;FAX;X-ESI-OTHER              */
    Prop  phone_company_main;       /* TEL;VOICE;WORK;PREF              */
    Prop  phone_callback;           /* TEL;VOICE;MSG                    */
    Prop  profession;               /* X-ESI-PROFESSION                 */
    Prop  nick_name;                /* X-ESI-NICK                       */
    Prop  manager;                  /* X-ESI-MANAGER                    */
    Prop  referred_by;              /* X-ESI-REFERREDBY                 */
    Prop  hobby;                    /* X-ESI-HOBBY                      */
    Prop  account;                  /* X-ESI-ACCOUNT                    */
    Prop  ftp_site;                 /* X-ESI-FTP                        */
    Prop  home_web;                 /* URL;HOME                         */
    Prop  mileage;                  /* X-ESI-MILEAGE                    */
    Prop  billing_information;      /* X-ESI-BILLING                    */
    Prop  organization_id;          /* X-ESI-ORGID                      */
    Prop  government_id;            /* X-ESI-GOVID                      */
    Prop  customer_id;              /* X-ESI-CUSTID                     */
    Prop  computer_network_name;    /* X-ESI-COMPNAME                   */
    U8   *data;
} SyncPhoneBook;

/*----------------------------------------------------------------------------
 * SyncNote structure
 *
 *     Defines the format of a Note SyncObject record.
 */
typedef struct _SyncNote {
    U8    header;                   /* Single character header for DB   */
    U8    dbVersion;                /* Indicates version of DataBase    */
    BOOL  objectComplete;           /* Indicates object parsed to end   */
    U8    charset[VP_CHARSET_LEN];  /* CHARSET of SyncNote              */
    void *next;                     /* Pointer to next SyncNote         */
    Prop  version;                  /* vNote version */
    Prop  creation_datetime;        /* DCREATED      */
    Prop  page_title;               /* SUBJECT       */
    Prop  page_note;                /* BODY          */
    Prop  note_categories;          /* CATEGORIES    */
    Prop  completion_date;          /* LAST-MODIFIED */
    Prop  completion_time;          /* LAST-MODIFIED */
    Prop  confidential;             /* CLASS         */
    U8   *data;
} SyncNote;

/*----------------------------------------------------------------------------
 * SyncToDo structure
 *
 *     Defines the format of a ToDo SyncObject record.
 */
typedef struct _SyncToDo {
    U8    header;                   /* Single character header for DB   */
    U8    dbVersion;                /* Indicates version of Database    */
    BOOL  objectComplete;           /* Indicates object parsed to end   */
    U8    charset[VP_CHARSET_LEN];  /* CHARSET of SyncToDo              */
    void *next;                     /* Pointer to next SyncToDo         */
    Prop  version;                  /* vCalendar version                */
    Prop  description;              /* SUMMARY       */
    Prop  end_date;                 /* DUE           */
    Prop  completed;                /* COMPLETED     */
    Prop  priority;                 /* PRIORITY      */
    Prop  confidential;             /* CLASS         */
    Prop  category;                 /* CATEGORIES    */
    Prop  note;                     /* DESCRIPTION   */
    Prop  start_date;               /* DTSTART       */
    Prop  completion_date;          /* COMPLETED     */
    Prop  reminder;                 /* DALARM        */
    Prop  alarm_date;               /* DALARM        */
    Prop  alarm_time;               /* DALARM        */
    Prop  percent_complete;
    Prop  mileage;
    Prop  billing_information;
    Prop  exdate;
    Prop  rrule;
    U8   *data;
} SyncToDo;

/*----------------------------------------------------------------------------
 * SyncEvent structure
 *
 *     Defines the format of an Event SyncObject record.
 */
typedef struct _SyncEvent {
    U8    header;                   /* Single character header for DB   */
    U8    dbVersion;                /* Indicates version of Database    */
    BOOL  objectComplete;           /* Indicates object parsed to end   */
    U8    charset[VP_CHARSET_LEN];  /* CHARSET of SyncEvent             */
    void *next;                     /* Pointer to next SyncEvent        */
    Prop  version;                  /* vCalendar version                */
    Prop  start_date;               /* DTSTART       */
    Prop  start_time;               /* DTSTART       */
    Prop  end_date;                 /* DTEND         */
    Prop  end_time;                 /* DTEND         */
    Prop  description;              /* SUMMARY       */
    Prop  reminder;                 /* DALARM        */
    Prop  alarm_date;               /* DALARM        */
    Prop  alarm_time;               /* DALARM        */
    Prop  category;                 /* CATEGORIES    */
    Prop  location;                 /* LOCATION      */
    Prop  note;                     /* DESCRIPTION   */
    Prop  busy_status;              /* TRANSP        */
    Prop  confidential;             /* CLASS         */
    Prop  exdate;
    Prop  rrule;
    U8   *data;
} SyncEvent;


/*----------------------------------------------------------------------------
 * SyncObject structure
 *
 *     This structure contains SyncObjects passed in to vBuild or out of
 *     vParse. See vBuild and vParse for structure content details in the
 *     context or each respective API.
 */
typedef struct _SyncObject {
    /* A linked list of SyncPhoneBook structures */
    SyncPhoneBook  *phoneBook;

    /* A linked list of SyncNote structures */
    SyncNote       *note;

    /* A linked list of SyncToDo structures */
    SyncToDo       *toDo;

    /* A linked list of SyncEvent structures */
    SyncEvent      *event;

    /* For vParse, the number of bytes remaining to be parsed. */
    U16             count;
} SyncObject;


/*----------------------------------------------------------------------------
 * vContext structure
 *
 *     This structure is provided by the application through the vBuild or
 *     vParse functions. It is used to maintain state during build or parse
 *     operations.
 */
typedef struct _vContext {
    /* === Internal use only === */
    /* Objects under construction */
    U8             *vo;         /* vObject */
    SyncObject      so;         /* SyncObject */

    /* Values used to track vObject parsing state. */
    vObjectType     pType;      /* parent type */
    vObjectType     type;       /* current type */
    U8              property;   /* current Property */
    U16             parameters; /* parameters of current Property */
    U8              charset[VP_CHARSET_LEN];
    SyncObject      cso;        /* current Sync Object. */
    U16             len;        /* length of original object */
    U8             *dp;         /* data pointer used when parsing */
    VoContext       vox;        /* VoContext passed to vObject APIs */
    XaStatus        status;     /* status of parse/build operation */
} vContext;


/****************************************************************************
 *
 * Function Reference
 *
 ****************************************************************************/
/*---------------------------------------------------------------------------
 * vParse()
 *
 *     Parse a UTF-8 or ASCII MIME formatted vObject into a SyncObject.
 *     Multiple nested objects may be returned in the SyncObject container
 *     structure. The pointers "phoneBook," "note," "toDo" and "event" will
 *     be non-null if they point to valid objects. In addition, each of these
 *     may point to multiple like objects through their respective "next"
 *     field.
 *
 *     Only one root level vObject will be parsed with each call. The size of
 *     any remaining root level objects to be parsed is indicated with a
 *     non-zero "count" field. In this case, vParse should be called again
 *     with the remaining vObjects.
 *
 *     If the object is parsed to completion, the "objectComplete" field
 *     in the SyncObject will be set to TRUE. Otherwise, "objectComplete"
 *     will be set to false and more object data is expected.
 *
 *     Each SyncObject is allowed one CHARSET parameter value. When the
 *     CHARSET parameter is encountered, the SyncObject "charset" field is
 *     set to the value. If multiple CHARSET parameters exist in the object,
 *     the SyncObject will be set to the last CHARSET parameter parsed.
 *
 * Parameters:
 *     vobject - Pointer to vObject to parse.
 *
 *     len - On input, length of vObject. On output, length of returned
 *          SyncObject.
 *
 *     vx - Pointer to vContext structure used to track state of vParse
 *          process.
 *
 * Returns:
 *     SyncObject* - A pointer to a SyncObject with the count field set
 *                   to the length of remaining objects to be parsed and
 *                   the passed in "len" parameter set to the SyncObject
 *                   size. 
 */
SyncObject *vParse(U8 *vobject, U16 *len, vContext *vx);

/*---------------------------------------------------------------------------
 * vBuild()
 *
 *     Build a UTF-8 or ASCII MIME formatted vObject from a SyncObject. Only
 *     one of the "phoneBook," "note," "toDo" and "event" pointers will be
 *     processed. If more than one is set, an error will be indicated with a
 *     return of zero. Multiple like objects will be processed through their
 *     respective "next" field.
 *
 *     A CHARSET parameter can be specified for the object before it is
 *     passed to vBuild. For example, the CHARSET could be set for a
 *     phonebook by setting the syncObject->phonebook->charset field to
 *     "CHARSET=UTF-8". This CHARSET parameter will then be added to any
 *     property that has a QUOTED-PRINTABLE parameter. The maximum length of
 *     the CHARSET string is specified with the VP_CHARSET_LEN constant.
 *
 * Parameters:
 *     vObject - Pointer to buffer that will receive the vObject. If null,
 *         vBuild will return the estimated object size in the "len"
 *         parameter. This size should be used to provide sufficient
 *         buffer space to vBuild.
 *
 *     len - On input, specifies length of "vObject" buffer. On output, the
 *         vObject size or estimated size if "vObject" is null.
 *
 *     syncObject - Pointer to a SyncObject from which vObjects will be built.
 *
 *     vx - Pointer to vContext structure used to track state of vBuild
 *         process.
 *     flags - Controls the fields that will be built for this SyncObject.
 *             This is specifically used when the Field-Level Replace feature
 *             of DataSync is utlized to send only the fields that have changed
 *             as opposed to the entire vObject.
 *             (Requires SML_FIELD_LEVEL_REPLACE)
 *
 * Returns:
 *     U8* - A pointer to a vObject with the passed in "len" parameter set
 *           to the vObject size or estimated size if "vObject" is null.
 */
#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
U8 *vBuild(U8 *vObject, U16 *len, const SyncObject *syncObject, 
           vContext *vx, VpFlags flags);
#else /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
U8 *vBuild(U8 *vObject, U16 *len, const SyncObject *syncObject, vContext *vx);
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */

/*---------------------------------------------------------------------------
 * vReleaseSyncObject()
 *
 *     Release memory allocated for a SyncObject. This must be called at
 *     some time after a call to vParse, before the application closes.
 *
 * Parameters:
 *     so - Pointer to SyncObject structure that contains object to release.
 *
 * Returns:
 *     None.
 */
void vReleaseSyncObject(SyncObject *so);

#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
/*---------------------------------------------------------------------------
 * vCompareFields()
 *
 *     Compare the vObject fields based on the SyncObject type 
 *     (phonebook/event/todo/note) and mark any changed fields for upcoming
 *     field-level replace synchronizations.
 *
 * Parameters:
 *     savedObj - Pointer to the saved SyncObject
 *
 *     rcvdObj - Pointer to the received SyncObject
 *
 * Returns:
 *     None.
 */
void vCompareFields(const void *savedObj, void *rcvdObj);

/*---------------------------------------------------------------------------
 * vResetFields()
 *
 *     Reset all of the changed vObject fields based on the SyncObject type 
 *     (phonebook/event/todo/note) as the field-level synchronization is now 
 *     complete.
 *
 * Parameters:
 *     obj - Pointer to the SyncObject
 *
 * Returns:
 *     None.
 */
void vResetFields(void *obj);

/*---------------------------------------------------------------------------
 * vFieldLevelReplace()
 *
 *     Process the vObject fields received for the SyncObject type 
 *     (phonebook/event/todo/note) performed in the field-level synchronization.
 *     This process will combine the saved object and received object fields
 *     into one combined object that will be returned.
 *
 * Parameters:
 *     savedObj - Pointer to the saved SyncObject
 *
 *     rcvdObj - Pointer to the received SyncObject
 *
 *     len - Length of the combined SyncObject.
 *
 * Returns:
 *     Pointer to the combined SyncObject.
 */
void *vFieldLevelReplace(const void *savedObj, const void *rcvdObj, U16 *len);

/*---------------------------------------------------------------------------
 * vReleaseFieldLevelRelace()
 *
 *     Release the combined SyncObject allocated in the vFieldLevelReplace
 *     function.
 *
 * Parameters:
 *     obj - Pointer to the SyncObject
 *
 * Returns:
 *     None.
 */
void vReleaseFieldLevelRelace(const void *obj);
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */

#endif /* __VPARSER_H */

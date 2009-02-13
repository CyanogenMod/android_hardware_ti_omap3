/***************************************************************************
 *
 * File:
 *     iAnywhere Data Sync SDK
 *     $Id:vp_map.h,v 1.18, 2006-01-17 20:19:52Z, Kevin Hendrix$
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

#ifndef __VP_MAP_H
#define __VP_MAP_H

typedef struct _VpMap {
    U16 offset;
    U16 prop;
    U16 part;
    U16 parm;
    U32 flrField;
} VpMap;

#define PHONE_BOOK_DEFAULT_MAP {\
VP_OFFSET(SyncPhoneBook, fn), VCARD_FN , 0, 0, VPF_FLR_NAME_FIELD,\
VP_OFFSET(SyncPhoneBook, version), VCARD_VERSION, 0, 0, VPF_FLR_VERSION_FIELD,\
VP_OFFSET(SyncPhoneBook, lname), VCARD_N, VCARD_N_PT_LAST, 0, VPF_FLR_NAME_FIELD,\
VP_OFFSET(SyncPhoneBook, fname), VCARD_N, VCARD_N_PT_FIRST, 0, VPF_FLR_NAME_FIELD,\
VP_OFFSET(SyncPhoneBook, mname), VCARD_N, VCARD_N_PT_MIDDLE, 0, VPF_FLR_NAME_FIELD,\
VP_OFFSET(SyncPhoneBook, title), VCARD_TITLE, VCARD_N_PT_TITLE, 0,VPF_FLR_TITLE_FIELD,\
VP_OFFSET(SyncPhoneBook, suffix),VCARD_N, VCARD_N_PT_SUFFIX,0,VPF_FLR_NAME_FIELD,\
VP_OFFSET(SyncPhoneBook, business_street), VCARD_ADR, VCARD_ADR_PT_STREET, VCARD_ADR_PM_WORK, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, business_city), VCARD_ADR, VCARD_ADR_PT_CITY, VCARD_ADR_PM_WORK, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, business_state), VCARD_ADR, VCARD_ADR_PT_STATE, VCARD_ADR_PM_WORK, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, business_zip), VCARD_ADR, VCARD_ADR_PT_ZIP, VCARD_ADR_PM_WORK, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, business_country), VCARD_ADR, VCARD_ADR_PT_COUNTRY, VCARD_ADR_PM_WORK, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, business_phone), VCARD_TEL, 0,VCARD_TEL_PM_VOICE | VCARD_TEL_PM_WORK, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, business_phone2), VCARD_TEL, 0,VCARD_TEL_PM_VOICE | VCARD_TEL_PM_WORK, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, business_fax), VCARD_TEL, 0, VCARD_TEL_PM_FAX | VCARD_TEL_PM_WORK, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, business_email), VCARD_EMAIL, 0, VCARD_EML_PM_WORK, VPF_FLR_EMAIL_FIELD,\
VP_OFFSET(SyncPhoneBook, company), VCARD_ORG, 0, 0, VPF_FLR_ORGANIZATION_FIELD,\
VP_OFFSET(SyncPhoneBook, department),VCARD_ORG, VCARD_ORG_PT_DEPARTMENT, 0, VPF_FLR_ORGANIZATION_FIELD,\
VP_OFFSET(SyncPhoneBook, business_web),VCARD_URL, 0, VCARD_URL_PM_WORK, VPF_FLR_URL_FIELD,\
VP_OFFSET(SyncPhoneBook, home_fax), VCARD_TEL, 0, VCARD_TEL_PM_FAX | VCARD_TEL_PM_HOME, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, home_street), VCARD_ADR, VCARD_ADR_PT_STREET,VCARD_ADR_PM_HOME, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, home_city), VCARD_ADR, VCARD_ADR_PT_CITY,VCARD_ADR_PM_HOME, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, home_state), VCARD_ADR, VCARD_ADR_PT_STATE, VCARD_ADR_PM_HOME, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, home_zip), VCARD_ADR, VCARD_ADR_PT_ZIP, VCARD_ADR_PM_HOME, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, home_country), VCARD_ADR, VCARD_ADR_PT_COUNTRY, VCARD_ADR_PM_HOME, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, home_phone),VCARD_TEL, 0, VCARD_TEL_PM_VOICE | VCARD_TEL_PM_HOME, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, home_phone2),VCARD_TEL, 0, VCARD_TEL_PM_VOICE | VCARD_TEL_PM_HOME, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, home_email),VCARD_EMAIL, 0, VCARD_EML_PM_HOME, VPF_FLR_EMAIL_FIELD,\
VP_OFFSET(SyncPhoneBook, home_web),VCARD_URL, 0, VCARD_URL_PM_HOME, VPF_FLR_URL_FIELD,\
VP_OFFSET(SyncPhoneBook, home_spouse),X_ESI_SPOUSE, 0,0,0,\
VP_OFFSET(SyncPhoneBook, home_children),X_ESI_CHILDREN, 0,0,0,\
VP_OFFSET(SyncPhoneBook, phone_pager), VCARD_TEL, 0, VCARD_TEL_PM_PAGER, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_mobile), VCARD_TEL, 0, VCARD_TEL_PM_CELL, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_car), VCARD_TEL, 0, VCARD_TEL_PM_CAR, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_radio), VCARD_TEL, 0, VCARD_TEL_PM_X_ESI_RADIO, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_tele), VCARD_TEL, 0, VCARD_TEL_PM_TLX, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_isdn), VCARD_TEL, 0, VCARD_TEL_PM_ISDN, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_tty_tdd), VCARD_TEL, 0, VCARD_TEL_PM_X_ESI_TTYTDD, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_company_main), VCARD_TEL, 0, VCARD_TEL_PM_VOICE | VCARD_TEL_PM_WORK | VCARD_TEL_PM_PREF, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_callback), VCARD_TEL, 0, VCARD_TEL_PM_VOICE | VCARD_TEL_PM_MSG, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_primary), X_ESI_NONE, 0, 0, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, phone_assistant), VCARD_TEL, 0, VCARD_TEL_PM_X_ESI_ASSIST, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, location), VCARD_LOCATION, 0, 0, VPF_FLR_LOCATION_FIELD,\
VP_OFFSET(SyncPhoneBook, birthday), VCARD_BDAY, 0, 0, VPF_FLR_BIRTHDAY_FIELD,\
VP_OFFSET(SyncPhoneBook, job_position), VCARD_ROLE, 0,0,0,\
VP_OFFSET(SyncPhoneBook, anniversary), X_ESI_ANNI, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, profession), X_ESI_PROFESSION, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, nick_name), X_ESI_NICK, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, manager), X_ESI_MANAGER,0, 0, 0,\
VP_OFFSET(SyncPhoneBook, referred_by), X_ESI_REFERREDBY, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, hobby), X_ESI_HOBBY, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, account), X_ESI_ACCOUNT, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, ftp_site), X_ESI_FTP, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, mileage), X_ESI_MILEAGE, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, billing_information), X_ESI_BILLING, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, organization_id), X_ESI_ORGID, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, government_id), X_ESI_GOVID, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, customer_id), X_ESI_CUSTID, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, computer_network_name), X_ESI_COMPNAME, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, assistant), X_ESI_ASSIST, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, other_street), VCARD_ADR, VCARD_ADR_PT_STREET, VCARD_ADR_PM_X_ESI_OTHER, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, other_city), VCARD_ADR, VCARD_ADR_PT_CITY, VCARD_ADR_PM_X_ESI_OTHER, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, other_state), VCARD_ADR, VCARD_ADR_PT_STATE, VCARD_ADR_PM_X_ESI_OTHER, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, other_zip), VCARD_ADR, VCARD_ADR_PT_ZIP, VCARD_ADR_PM_X_ESI_OTHER, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, other_country), VCARD_ADR, VCARD_ADR_PT_COUNTRY, VCARD_ADR_PM_X_ESI_OTHER, VPF_FLR_ADDRESS_FIELD,\
VP_OFFSET(SyncPhoneBook, other_phone), X_ESI_NONE, 0, 0, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, other_fax), VCARD_TEL, 0, VCARD_TEL_PM_FAX | VCARD_TEL_PM_X_ESI_OTHER, VPF_FLR_TELEPHONE_FIELD,\
VP_OFFSET(SyncPhoneBook, other_email), VCARD_EMAIL, 0, VCARD_EML_PM_X_ESI_OTHER, VPF_FLR_EMAIL_FIELD,\
VP_OFFSET(SyncPhoneBook, note), VCARD_NOTE, 0, 0, VPF_FLR_NOTE_FIELD,\
VP_OFFSET(SyncPhoneBook, confidential), VCARD_CLASS, 0, 0, VPF_FLR_CLASS_FIELD,\
VP_OFFSET(SyncPhoneBook, category), VCARD_CATEGORIES, 0, 0, VPF_FLR_CATEGORIES_FIELD,\
VP_OFFSET(SyncPhoneBook, custom1), X_ESI_CUST1, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, custom2), X_ESI_CUST2, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, custom3), X_ESI_CUST3, 0, 0, 0,\
VP_OFFSET(SyncPhoneBook, custom4), X_ESI_CUST4, 0, 0, 0\
}

typedef struct _PhoneBookMap {
    VpMap fn;                       /* FN: First Name*/
    VpMap version;              /* VERSION*/
    VpMap lname;                /* N:LastName;;;; */
    VpMap fname;                /* N:;FirstName;;;*/
    VpMap mname;                /* N:;;MiddleName;; */
    VpMap title;                /* N:;;;Title;*/
    VpMap suffix;               /* N:;;;;Suffix */
    VpMap job_position;         /* TITLE*/
    VpMap business_street;      /* ADR;WORK:;;Street;;;;*/
    VpMap business_city;        /* ADR;WORK:;;;City;;;*/
    VpMap business_state;       /* ADR;WORK:;;;;State;; */
    VpMap business_zip;         /* ADR;WORK:;;;;;Zip; */
    VpMap business_country;     /* ADR;WORK:;;;;;;Country */
    VpMap business_phone;       /* TEL;VOICE;WORK */
    VpMap business_phone2;      /* TEL;VOICE;WORK (2nd occurrence)*/
    VpMap business_fax;         /* TEL;FAX;WORK */
    VpMap business_email;       /* EMAIL;WORK */
    VpMap company;              /* ORG:Company; */
    VpMap department;           /* ORG:;Department*/
    VpMap business_web;         /* URL;WORK */
    VpMap home_fax;             /* TEL;FAX;HOME */
    VpMap home_street;          /* ADR;HOME:;;Street;;;;*/
    VpMap home_city;            /* ADR;HOME:;;;City;;;*/
    VpMap home_state;           /* ADR;HOME:;;;;State;; */
    VpMap home_zip;             /* ADR;HOME:;;;;;Zip; */
    VpMap home_country;         /* ADR;HOME:;;;;;;Country */
    VpMap home_phone;           /* TEL;VOICE;HOME */
    VpMap home_phone2;          /* TEL;VOICE;HOME (2nd occurrence)*/
    VpMap home_email;           /* EMAIL;HOME */
    VpMap home_web;             /* URL;HOME */
    VpMap home_spouse;          /* X-ESI-SPOUSE */
    VpMap home_children;        /* X-ESI-CHILDREN */
    VpMap phone_pager;          /* TEL;PAGER*/
    VpMap phone_mobile;         /* TEL;CELL */
    VpMap phone_car;            /* TEL;CAR*/
    VpMap phone_radio;          /* TEL;X-ESI-RADIO*/
    VpMap phone_tele;           /* TEL;TLX*/
    VpMap phone_isdn;           /* TEL;ISDN */
    VpMap phone_tty_tdd;        /* TEL;X-ESI-TTYTDD */
    VpMap phone_company_main;   /* TEL;VOICE;WORK;PREF*/
    VpMap phone_callback;       /* TEL;VOICE;MSG*/
    VpMap phone_primary;        /* */
    VpMap phone_assistant;      /* TEL;VOICE;X-ESI-ASSIST */
    VpMap location;             /* LOCATION */
    VpMap birthday;             /* BDAY */
    VpMap anniversary;          /* X-ESI-ANNI */
    VpMap profession;           /* X-ESI-PROFESSION */
    VpMap nick_name;            /* X-ESI-NICK */
    VpMap manager;              /* X-ESI-MANAGER*/
    VpMap referred_by;          /* X-ESI-REFERREDBY */
    VpMap hobby;                /* X-ESI-HOBBY*/
    VpMap account;              /* X-ESI-ACCOUNT*/
    VpMap ftp_site;             /* X-ESI-FTP*/
    VpMap mileage;              /* X-ESI-MILEAGE*/
    VpMap billing_information;  /* X-ESI-BILLING*/
    VpMap organization_id;      /* X-ESI-ORGID*/
    VpMap government_id;        /* X-ESI-GOVID*/
    VpMap customer_id;          /* X-ESI-CUSTID */
    VpMap computer_network_name;/* X-ESI-COMPNAME */
    VpMap assistant;            /* X-ESI-ASSIST */
    VpMap other_street;         /* ADR;X-ESI-OTHER:;;Street;;;; */
    VpMap other_city;           /* ADR;X-ESI-OTHER:;;;City;;; */
    VpMap other_state;          /* ADR;X-ESI-OTHER:;;;;State;;*/
    VpMap other_zip;            /* ADR;X-ESI-OTHER:;;;;;Zip;*/
    VpMap other_country;        /* ADR;X-ESI-OTHER:;;;;;;Country*/
    VpMap other_phone;          /* */
    VpMap other_fax;            /* TEL;FAX;X-ESI-OTHER*/
    VpMap other_email;          /* EMAIL;X-ESI-OTHER*/
    VpMap note;                 /* NOTE */
    VpMap confidential;         /* CLASS*/
    VpMap category;             /* CATEGORIES */
    VpMap custom1;              /* X-ESI-CUST1*/
    VpMap custom2;              /* X-ESI-CUST2*/
    VpMap custom3;              /* X-ESI-CUST3*/
    VpMap custom4;              /* X-ESI-CUST4*/
} PhoneBookMap;

#define NOTE_DEFAULT_MAP {\
VP_OFFSET(SyncNote, version),VNOTE_VERSION, 0,0, VPF_FLR_VERSION_FIELD,\
VP_OFFSET(SyncNote, creation_datetime),VNOTE_DCREATED, 0, 0, VPF_FLR_CREATED_FIELD,\
VP_OFFSET(SyncNote, page_title),VNOTE_SUBJECT, 0, 0, VPF_FLR_SUBJECT_FIELD,\
VP_OFFSET(SyncNote, page_note),VNOTE_BODY, 0, 0, VPF_FLR_BODY_FIELD,\
VP_OFFSET(SyncNote, note_categories),VNOTE_CATEGORIES, 0, 0, VPF_FLR_CATEGORIES_FIELD,\
VP_OFFSET(SyncNote, completion_date),VNOTE_LAST_MODIFIED, 0, 0, VPF_FLR_LAST_MODIFIED_FIELD,\
VP_OFFSET(SyncNote, completion_time),VNOTE_LAST_MODIFIED, 1, 0, VPF_FLR_LAST_MODIFIED_FIELD,\
VP_OFFSET(SyncNote, confidential),VNOTE_LAST_CLASS, 0, 0, VPF_FLR_CLASS_FIELD\
}

typedef struct _NoteMap {
    VpMap version;              /* VERSION */
    VpMap creation_datetime;    /* DCREATED*/
    VpMap page_title;           /* SUBJECT */
    VpMap page_note;            /* BODY*/
    VpMap note_categories;      /* CATEGORIES*/
    VpMap completion_date;      /* LAST-MODIFIED */
    VpMap completion_time;      /* LAST-MODIFIED */
    VpMap confidential;         /* CLASS */
} NoteMap;

#define TODO_DEFAULT_MAP {\
VP_OFFSET(SyncToDo, version),VTODO_VERSION, 0,0, VPF_FLR_VERSION_FIELD,\
VP_OFFSET(SyncToDo, description),VTODO_SUMMARY, 0, 0, VPF_FLR_SUMMARY_FIELD,\
VP_OFFSET(SyncToDo, end_date),VTODO_DUE, 0, 0, VPF_FLR_DUE_FIELD,\
VP_OFFSET(SyncToDo, completed),VTODO_COMPLETED, 0xff, 0, VPF_FLR_COMPLETED_FIELD,\
VP_OFFSET(SyncToDo, priority),VTODO_PRIORITY, 0, 0, VPF_FLR_PRIORITY_FIELD,\
VP_OFFSET(SyncToDo, confidential),VTODO_CLASS, 0, 0, VPF_FLR_CLASS_FIELD,\
VP_OFFSET(SyncToDo, category),VTODO_CATEGORIES, 0, 0, VPF_FLR_CATEGORIES_FIELD,\
VP_OFFSET(SyncToDo, note),VTODO_DESCRIPTION, 0, 0, VPF_FLR_DESCRIPTION_FIELD,\
VP_OFFSET(SyncToDo, start_date),VTODO_DTSTART, 0, 0, VPF_FLR_START_DATE_FIELD,\
VP_OFFSET(SyncToDo, completion_date),VTODO_COMPLETED, 0, 0, VPF_FLR_COMPLETED_FIELD,\
VP_OFFSET(SyncToDo, reminder),VTODO_DALARM, 0xff, 0, VPF_FLR_ALARM_FIELD,\
VP_OFFSET(SyncToDo, alarm_date),VTODO_DALARM, 0, 0, VPF_FLR_ALARM_FIELD,\
VP_OFFSET(SyncToDo, alarm_time),VTODO_DALARM, 1, 0, VPF_FLR_ALARM_FIELD,\
VP_OFFSET(SyncToDo, percent_complete),0xff, 0, 0, 0,\
VP_OFFSET(SyncToDo, mileage),0xff, 0, 0, 0,\
VP_OFFSET(SyncToDo, billing_information),0xff, 0, 0, 0,\
VP_OFFSET(SyncToDo, exdate),VTODO_EXDATE, 0, 0, VPF_FLR_EXDATE_FIELD,\
VP_OFFSET(SyncToDo, rrule),VTODO_RRULE, 0, 0, VPF_FLR_RRULE_FIELD\
}

typedef struct _ToDoMap {
VpMap version;                  /* VERSION */
    VpMap description;          /* SUMMARY */
    VpMap end_date;             /* DUE */
    VpMap completed;            /* COMPLETED */
    VpMap priority;             /* PRIORITY*/
    VpMap confidential;         /* CLASS */
    VpMap category;             /* CATEGORIES*/
    VpMap note;                 /* DESCRIPTION */
    VpMap start_date;           /* DTSTART */
    VpMap completion_date;      /* COMPLETED */
    VpMap reminder;             /* DALARM*/
    VpMap alarm_date;           /* DALARM*/
    VpMap alarm_time;           /* DALARM*/
    VpMap percent_complete;
    VpMap mileage;
    VpMap billing_information;
    VpMap exdate;               /* EXDATE */
    VpMap rrule;                /* RRULE*/
} ToDoMap;

#define EVENT_DEFAULT_MAP {\
VP_OFFSET(SyncEvent, version),VEVENT_VERSION, 0,0, VPF_FLR_VERSION_FIELD,\
VP_OFFSET(SyncEvent, start_date),VEVENT_DTSTART, 0, 0, VPF_FLR_START_DATE_FIELD,\
VP_OFFSET(SyncEvent, start_time),VEVENT_DTSTART, 1, 0, VPF_FLR_START_DATE_FIELD,\
VP_OFFSET(SyncEvent, end_date),VEVENT_DTEND, 0, 0, VPF_FLR_END_DATE_FIELD,\
VP_OFFSET(SyncEvent, end_time),VEVENT_DTEND, 1, 0, VPF_FLR_END_DATE_FIELD,\
VP_OFFSET(SyncEvent, description),VEVENT_SUMMARY, 0, 0, VPF_FLR_SUMMARY_FIELD,\
VP_OFFSET(SyncEvent, reminder),VEVENT_DALARM, 0xff, 0, VPF_FLR_ALARM_FIELD,\
VP_OFFSET(SyncEvent, alarm_date),VEVENT_DALARM, 0, 0, VPF_FLR_ALARM_FIELD,\
VP_OFFSET(SyncEvent, alarm_time),VEVENT_DALARM, 1, 0, VPF_FLR_ALARM_FIELD,\
VP_OFFSET(SyncEvent, category),VEVENT_CATEGORIES, 0, 0, VPF_FLR_CATEGORIES_FIELD,\
VP_OFFSET(SyncEvent, location),VEVENT_LOCATION, 0, 0, VPF_FLR_LOCATION_FIELD,\
VP_OFFSET(SyncEvent, note),VEVENT_DESCRIPTION, 0, 0, VPF_FLR_DESCRIPTION_FIELD,\
VP_OFFSET(SyncEvent, busy_status),VEVENT_TRANSP, 0, 0, VPF_FLR_TRANSP_FIELD,\
VP_OFFSET(SyncEvent, confidential),VEVENT_CLASS, 0, 0, VPF_FLR_CLASS_FIELD,\
VP_OFFSET(SyncEvent, exdate),VEVENT_EXDATE, 0, 0, VPF_FLR_EXDATE_FIELD,\
VP_OFFSET(SyncEvent, rrule),VEVENT_RRULE, 0, 0, VPF_FLR_RRULE_FIELD\
}

typedef struct _EventMap {
    VpMap version;              /* VERSION*/
    VpMap start_date;           /* DTSTART*/
    VpMap start_time;           /* DTSTART*/
    VpMap end_date;             /* DTEND*/
    VpMap end_time;             /* DTEND*/
    VpMap description;          /* SUMMARY*/
    VpMap reminder;             /* DALARM */
    VpMap alarm_date;           /* DALARM */
    VpMap alarm_time;           /* DALARM */
    VpMap category;             /* CATEGORIES */
    VpMap location;             /* LOCATION */
    VpMap note;                 /* DESCRIPTION*/
    VpMap busy_status;          /* TRANSP */
    VpMap confidential;         /* CLASS*/
    VpMap exdate;               /* EXDATE */
    VpMap rrule;                /* RRULE*/
} EventMap;

#endif /* __VP_MAP_H */

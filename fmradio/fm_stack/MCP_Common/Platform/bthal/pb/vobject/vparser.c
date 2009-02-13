/****************************************************************************
 *
 * File:
 *     iAnywhere Data Sync SDK
 *     $Id:vParser.c,v 1.42, 2006-02-03 01:09:44Z, Kevin Hendrix$
 *
 * Description:
 *     The vParse parser. The parser builds all types
 *     and versions of vObjects.
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
#include "osapi.h"
#include "oslib.h"
#include "utils.h"
#include "vobject.h"
#include "vParser.h"
#include "vp_map.h"

static void voCallback(VoCallbackParms *Parms);
static void vpStoreCardProperty(VoContext *context);
static void vpStoreNoteProperty(VoContext *context);
static void vpStoreEventProperty(VoContext *context);
static void vpStoreToDoProperty(VoContext *context);
static void vpFreePhoneBook(SyncPhoneBook *pb);
static void vpFreeNote(SyncNote *n);
static void vpFreeToDo(SyncToDo *t);
static void vpFreeEvent(SyncEvent *e);
static void vpBuildPhoneBook(SyncPhoneBook *pb, VoContext *vox, VpFlags flags);
static void vpBuildNote(SyncNote *n, VoContext *vox, VpFlags flags);
static void vpBuildToDo(SyncToDo *t, VoContext *vox, VpFlags flags);
static void vpBuildEvent(SyncEvent *n, VoContext *vox, VpFlags flags);
#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
static void vCopySoProperty(Prop *destProp, void *destObj, const Prop *srcProp,  
                            const void *srcObj, U16 *offset);
static void vComparePhonebookFields(const SyncPhoneBook *SavedPb, SyncPhoneBook *RcvdPb);
static void vCompareEventFields(const SyncEvent *SavedEvent, SyncEvent *RcvdEvent);
static void vCompareToDoFields(const SyncToDo *SavedToDo, SyncToDo *RcvdToDo);
static void vCompareNoteFields(const SyncNote *SavedNote, SyncNote *RcvdNote);
static void vResetPhonebookFields(SyncPhoneBook *Pb);
static void vResetEventFields(SyncEvent *Event);
static void vResetToDoFields(SyncToDo *ToDo);
static void vResetNoteFields(SyncNote *Note);
static SyncPhoneBook *vFlrPhonebook(const SyncPhoneBook *SavedPb, 
                                    const SyncPhoneBook *RcvdPb, U16 *Len);
static SyncEvent *vFlrEvent(const SyncEvent *SavedEvent, 
                            const SyncEvent *RcvdEvent,U16 *Len);
static SyncToDo *vFlrToDo(const SyncToDo *SavedToDo, const SyncToDo *RcvdToDo, U16 *Len);
static SyncNote *vFlrNote(const SyncNote *SavedNote, const SyncNote *RcvdNote, U16 *Len);
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
static U16  vpEstimateObjectSize(const SyncObject *so);


#define VP_NUM_PHONEBOOK_ENTRIES    (sizeof(phoneBookMap) / sizeof(VpMap))
#define VP_NUM_NOTE_ENTRIES         (sizeof(noteMap) / sizeof(VpMap))
#define VP_NUM_EVENT_ENTRIES        (sizeof(eventMap) / sizeof(VpMap))
#define VP_NUM_TODO_ENTRIES         (sizeof(toDoMap) / sizeof(VpMap))

#define VP_PHONEBOOK_INDEX          ((U8 *)vx->cso.phoneBook)
#define VP_NOTE_INDEX               ((U8 *)vx->cso.note)
#define VP_EVENT_INDEX              ((U8 *)vx->cso.event)
#define VP_TODO_INDEX               ((U8 *)vx->cso.toDo)

#define VP_OFFSET(s, m) ((I16)(&((s *)0)->m))

const char *vObject_Types[]   = VOBJECT_TYPES;
const char *vCard_Types[]     = VCARD_TYPES;
const char *vGlobal_Types[]   = VGLOBAL_TYPES;
const char *vCard_TelTypes[]  = VCARD_TELTYPES;
const char *vCard_AdrTypes[]  = VCARD_ADRTYPES;
const char *vCard_EmlTypes[]  = VCARD_EMLTYPES;
const char *vCard_UrlTypes[]  = VCARD_URLTYPES;
const char *vNote_Types[]     = VNOTE_TYPES;
const char *vCal_ToDoTypes[]  = VCAL_TODOTYPES;
const char *vCal_EventTypes[] = VCAL_EVENTTYPES;

const PhoneBookMap  phoneBookMap = PHONE_BOOK_DEFAULT_MAP;
const NoteMap       noteMap      = NOTE_DEFAULT_MAP;
const ToDoMap       toDoMap      = TODO_DEFAULT_MAP;
const EventMap      eventMap     = EVENT_DEFAULT_MAP;

/*---------------------------------------------------------------------------
 *            vParse()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Parse the first vObject at the vobject pointer. If vobject
 *            has more than one root level object, the count of bytes not
 *            parsed will be returned in the SyncObject structure.
 * 
 */
SyncObject *vParse(U8 *vobject, U16 *len, vContext *vx)
{
    OS_MemSet((U8 *)vx, 0, sizeof(vContext));

    VO_Init(&vx->vox, voCallback);

    vx->len = *len;
    vx->so.count = VO_Parse(&vx->vox, vobject, *len);
    *len = vx->len;

    return &vx->so;
}

/*---------------------------------------------------------------------------
 *            vBuild()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Build a vObject from the provided SyncObject.
 * 
 */
#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
U8 *vBuild(U8 *vObject, U16 *len, const SyncObject *so, vContext *vx, VpFlags flags)
#else /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
U8 *vBuild(U8 *vObject, U16 *len, const SyncObject *so, vContext *vx)
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
{
    VoStatus        status;
    U16             tLen;
    SyncPhoneBook  *pb = so->phoneBook;
    SyncNote       *n = so->note;
    SyncToDo       *t = so->toDo;
    SyncEvent      *e = so->event;
#if SML_FIELD_LEVEL_REPLACE != XA_ENABLED
    VpFlags         flags = VPF_FLR_OFF;
#endif /* SML_FIELD_LEVEL_REPLACE != XA_ENABLED  */

    /* Estimate the object size */
    if ((tLen = vpEstimateObjectSize(so)) == 0) {
        /* vpEstimateObjectSize checks for errors. */
        return 0;
    }

    /* If the vObject pointer is null, return the estimated size.
     * vpEstimateObjectSize checks the SyncObject, so tLen will
     * be 0, if errors.
     */
    if (!vObject) {
        *len = tLen;
        return 0;
    }

    OS_MemSet((U8 *)vx, 0, sizeof(vContext));
    OS_MemSet(vObject, 0, *len);

    status = VO_Init(&vx->vox, voCallback);
    status = VO_QueueEncodeBuffer(&vx->vox, vObject, *len);
    *len = 0;

    /* Build the vObject */
    if (pb) {
        while (pb) {
            /* Note: flags field is ignored if field-level replace is disabled */
            vpBuildPhoneBook(pb, &vx->vox, flags);
            pb = pb->next;
        }
    }
    else if (n) {
        while (n) {
            /* Note: flags field is ignored if field-level replace is disabled */
            vpBuildNote(n, &vx->vox, flags);
            n = n->next;
        }
    }
    else if (t) {
        /* Put the vCalendar wrapper around the ToDo. */
        VO_BeginObject(&vx->vox, "VCALENDAR");
        VO_AddProperty(&vx->vox, "VERSION");
        VO_AddPropertyValue(&vx->vox, "1.0", 3);

        while (t) {
            /* Note: flags field is ignored if field-level replace is disabled */
            vpBuildToDo(t, &vx->vox, flags);
            t = t->next;
        }

        /* Write the END statement */
        VO_EndObject(&vx->vox, "VCALENDAR");
    }
    else if (e) {
        /* Put the vCalendar wrapper around the Event. */
        VO_BeginObject(&vx->vox, "VCALENDAR");
        VO_AddProperty(&vx->vox, "VERSION");
        VO_AddPropertyValue(&vx->vox, "1.0", 3);

        while (e) {
            /* Note: flags field is ignored if field-level replace is disabled */
            vpBuildEvent(e, &vx->vox, flags);
            e = e->next;
        }

        /* Write the END statement */
        VO_EndObject(&vx->vox, "VCALENDAR");
    }

    status = VO_GetEncodedBuffer(&vx->vox, &vx->vo, len);
    return vx->vo;
}

/*---------------------------------------------------------------------------
 *            vReleaseSyncObject()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Free the memory associated with the SyncObject.
 * 
 */
void vReleaseSyncObject(SyncObject *so)
{
    /* Nothing to free */
    if (!so) return;

    if (so->phoneBook) {
        vpFreePhoneBook(so->phoneBook);
    }

    if (so->note) {
        vpFreeNote(so->note);
    }
    
    if (so->toDo) {
        vpFreeToDo(so->toDo);
    }
    
    if (so->event) {
        vpFreeEvent(so->event);
    }
}

/*---------------------------------------------------------------------------
 *            voCallback()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Processes events from the vObject parser. This callback builds
 *            the card data record which represents the contents of a vcard.
 * 
 */
static void voCallback(VoCallbackParms *Parms)
{
    U8          i, j, jstart;
    U16         len;
    const U8   *parm, **types;
    char        parmBuff[100];   /* Enough for lots of parameters */
    VoStatus    status;
    vContext   *vx = ContainingRecord(Parms->context, vContext, vox); 

    switch (Parms->event) {
    case VOE_PROPERTY_BEGIN:
        switch(vx->type) {
        case VCARD:
            types = (U8**)vCard_Types;
            break;

        case VNOTE:
            types = (U8**)vNote_Types;
            break;

        case VEVENT:
            types = (U8**)vCal_EventTypes;
            break;

        case VTODO:
            types = (U8**)vCal_ToDoTypes;
            break;

        case VALARM:
            if (vx->pType == VEVENT) {
                types = (U8**)vCal_EventTypes;
            }
            else if (vx->pType == VTODO) {
                types = (U8**)vCal_ToDoTypes;
            }
            else {
                return;
            }
            break;

        default:
            return;
        }

        /* The property name is available, see if it's one we store. */
        for (i = j = 0; types[i]; i++, j = 0) {
            if (OS_StrCmp(Parms->u.property, (char*)types[i]) == 0) {
                /* Cache index of current property. */
                vx->property = i;
                break;
            }
        }
        break;

    case VOE_PROPERTY_VALUE:
        /* Now store the property value in the correct place. */
        switch(vx->type) {
        case VCARD:
            vpStoreCardProperty(Parms->context);
            break;

        case VNOTE:
            vpStoreNoteProperty(Parms->context);
            break;

        case VEVENT:
            vpStoreEventProperty(Parms->context);
            break;

        case VTODO:
            vpStoreToDoProperty(Parms->context);
            break;

        case VALARM:
            if (vx->pType == VEVENT) {
                vpStoreEventProperty(Parms->context);
            }
            else if (vx->pType == VTODO) {
                vpStoreToDoProperty(Parms->context);
            }
            break;

        default:
            break;
        }

        break;

    case VOE_PROPERTY_END:
        vx->property = sizeof(vCard_Types);
        vx->parameters = 0;
        break;

    case VOE_PARAMETER_BEGIN:
        /* In vCard 2.1 the parameter "value" can be specified without
         * it's type if it is not ambiguous. In this case we're looking
         * for the "TYPE" field which can be specified as:
         *    in vCard 3.0: TEL;TYPE=HOME,PREF:+1 808 555-1212
         *    in vCard 2.1: TEL;HOME,PREF:+1 808 555-1212
         * For this reason we have to pass both the parameter name and
         * value through the "value" lookup function.
         */
        parm = (U8*)Parms->u.parameter;
        len = OS_StrLen(Parms->u.parameter);
        goto lookupParameter;

    case VOE_PARAMETER_VALUE:
        len = sizeof(parmBuff);

        /* Values must be "read" into a local buffer. */
        status = VO_ReadParameterValue(Parms->context, (U8 *)parmBuff, &len);
        /* ToDo: handle large parm values */
        Assert(status == VO_OK);
        if (status != VO_OK) {
            vx->status = XA_STATUS_INVALID_TYPE;
            VO_Abort(Parms->context);
            break;
        }

        parmBuff[len] = 0;
        parm = (U8*)parmBuff;

        if (vx->charset[8] == '&') {
            /* This is the value for CHARSET */
            OS_MemCopy(vx->charset+8, parm, min(len, VP_CHARSET_LEN));
            if (len + 8 < VP_CHARSET_LEN)
                vx->charset[len+8] = 0;
            else
                vx->charset[VP_CHARSET_LEN - 1] = 0;
        }

lookupParameter:
        /* Check for Quoted-Printable */
        if (VO_StrniCmp((char*)parm, "QUOTED-PRINTABLE", len) == 0) {
            vx->parameters |= VCARD_PM_QUOTEDPRINTABLE;
        }

        /* Check for Quoted-Printable */
        if (VO_StrniCmp((char*)parm, "CHARSET", 7) == 0) {
            OS_MemCopy(vx->charset, "CHARSET=&", 9);
        }

        /* Some properties have other associated parameters. If so, point to 
         * the correct types list.
         */
        switch (vx->type) {
        case VCARD:
            switch (vx->property) {
            case VCARD_TEL:
                types = (U8**)vCard_TelTypes;
                break;

            case VCARD_ADR:
                types = (U8**)vCard_AdrTypes;
                break;

            case VCARD_EMAIL:
                types = (U8**)vCard_EmlTypes;
                break;

            case VCARD_URL:
                types = (U8**)vCard_UrlTypes;
                break;

            default:
                return;
            }
            break;

        default:
            return;
        }


        /* This property may have parameters. Process the entire
         * comma delimited list.
         */
        for (jstart = 0; jstart < len; jstart = (U8)(j + 1)) {

            for (j = jstart; parm[j]; j++) {
                if (parm[j] == ',' && parm[j-1] != '\\') {
                    break;
                }
            }

            for (i = 0; types[i]; i++) {
                if (VO_StrniCmp((char*)parm+jstart, (char*)types[i], j - jstart) == 0) {
                    /* Match! Cache the index of each property. */
                    vx->parameters |= (1 << i);
                    break;
                }
            }
        }
        break;

    case VOE_PARAMETER_END:
        break;

    case VOE_OBJECT_BEGIN:
        /* The object type is available, see if it's one we store. */
        for (i = j = 0; vObject_Types[i]; i++, j = 0) {
            if (OS_StrCmp(Parms->u.object, vObject_Types[i]) == 0) {
                
                /* save the parent type for nested objects */
                if ((1 << i) == VALARM &&
                    (vx->type == VEVENT || vx->type == VTODO)) {
                    vx->pType = vx->type;
                }
                
                /* Cache index of current property. */
                vx->type = (U16)(1 << i);

                /* Get some memory for the object */
                switch (1 << i) {
                case VCARD:
                    /* Initialize vCard & property tracking values. */
                    vx->property = sizeof(vCard_Types);
                    vx->parameters = 0;
                    len = (U16)(sizeof(SyncPhoneBook) + vx->len);
                    if ((vx->cso.phoneBook = OS_Malloc(len)) == 0) {
                        vx->status = XA_STATUS_NO_RESOURCES;
                        VO_Abort(Parms->context);
                        return;
                    }
                    OS_MemSet((U8 *)vx->cso.phoneBook, 0, len);
                    vx->cso.phoneBook->data = (U8 *)(vx->cso.phoneBook + 1);
                    vx->dp = vx->cso.phoneBook->data;

                    /* The first byte must be non-zero for successful DB writes */
                    vx->cso.phoneBook->header = 'p';

                    if (!vx->so.phoneBook) {
                        vx->so.phoneBook = vx->cso.phoneBook;
                    }
                    else {
                        vx->so.phoneBook->next = vx->cso.phoneBook;
                    }
                    break;

                case VNOTE:
                    /* Initialize vNote & property tracking values. */
                    vx->property = sizeof(vNote_Types);
                    vx->parameters = 0;
                    len = (U16)(sizeof(SyncNote) + vx->len);
                    if ((vx->cso.note = OS_Malloc(len)) == 0) {
                        vx->status = XA_STATUS_NO_RESOURCES;
                        VO_Abort(Parms->context);
                        return;
                    }
                    OS_MemSet((U8 *)vx->cso.note, 0, len);
                    vx->cso.note->data = (U8 *)(vx->cso.note + 1);
                    vx->dp = vx->cso.note->data;

                    /* The first byte must be non-zero for successful DB writes */
                    vx->cso.note->header = 'n';

                    if (!vx->so.note) {
                        vx->so.note = vx->cso.note;
                    }
                    else {
                        vx->so.note->next = vx->cso.note;
                    }
                    break;

                case VCALENDAR:
                    /* Contains vEvents or vToDos */
                    break;

                case VEVENT:
                    /* Initialize vCalendar Event & property tracking values. */
                    vx->property = sizeof(vCal_EventTypes);
                    vx->parameters = 0;
                    len = (U16)(sizeof(SyncEvent) + vx->len);
                    if ((vx->cso.event = OS_Malloc(len)) == 0) {
                        vx->status = XA_STATUS_NO_RESOURCES;
                        VO_Abort(Parms->context);
                        return;
                    }
                    OS_MemSet((U8 *)vx->cso.event, 0, len);
                    vx->cso.event->data = (U8 *)(vx->cso.event + 1);
                    vx->dp = vx->cso.event->data;

                    /* The first byte must be non-zero for successful DB writes */
                    vx->cso.event->header = 'e';

                    if (!vx->so.event) {
                        vx->so.event = vx->cso.event;
                    }
                    else {
                        vx->so.event->next = vx->cso.event;
                    }
                    break;

                case VTODO:
                    /* Initialize vCalendar ToDo & property tracking values. */
                    vx->property = sizeof(vCal_ToDoTypes);
                    vx->parameters = 0;
                    len = (U16)(sizeof(SyncToDo) + vx->len);
                    if ((vx->cso.toDo = OS_Malloc(len)) == 0) {
                        vx->status = XA_STATUS_NO_RESOURCES;
                        VO_Abort(Parms->context);
                        return;
                    }
                    OS_MemSet((U8 *)vx->cso.toDo, 0, len);
                    vx->cso.toDo->data = (U8 *)(vx->cso.toDo + 1);
                    vx->dp = vx->cso.toDo->data;

                    /* The first byte must be non-zero for successful DB writes */
                    vx->cso.toDo->header = 't';

                    if (!vx->so.toDo) {
                        vx->so.toDo = vx->cso.toDo;
                    }
                    else {
                        vx->so.toDo->next = vx->cso.toDo;
                    }
                    break;

                case VALARM:
                    /* Nested inside vEvent or vToDo */
                    break;

                case VFREEBUSY:
                    /* not translated */
                    break;

                case VJOURNAL:
                    /* not translated */
                    break;

                case VTIMEZONE:
                    /* not translated */
                    break;

                case STANDARD:
                    /* not translated */
                    break;

                case DAYLIGHT:
                    /* not translated */
                    break;
                }
                break;
            }
        }
        break;

    case VOE_OBJECT_END:
        switch(vx->type) {
        case VCARD:
            Assert(vx->cso.phoneBook);
            OS_MemCopy(vx->cso.phoneBook->charset, vx->charset, VP_CHARSET_LEN);
            vx->cso.phoneBook->objectComplete = TRUE;
            vx->len = (U16)(vx->dp - (U8 *)vx->cso.phoneBook);
            break;

        case VNOTE:
            Assert(vx->cso.note);
            OS_MemCopy(vx->cso.note->charset, vx->charset, VP_CHARSET_LEN);
            vx->cso.note->objectComplete = TRUE;
            vx->len = (U16)(vx->dp - (U8 *)vx->cso.note);
            break;

        case VEVENT:
            Assert(vx->cso.event);
            OS_MemCopy(vx->cso.event->charset, vx->charset, VP_CHARSET_LEN);
            vx->cso.event->objectComplete = TRUE;
            vx->len = (U16)(vx->dp - (U8 *)vx->cso.event);
            break;

        case VTODO:
            Assert(vx->cso.toDo);
            OS_MemCopy(vx->cso.toDo->charset, vx->charset, VP_CHARSET_LEN);
            vx->cso.toDo->objectComplete = TRUE;
            vx->len = (U16)(vx->dp - (U8 *)vx->cso.toDo);
            break;

        default:
            break;
        }

        break;

    case VOE_ERROR:
        switch(vx->type) {
        case VCARD:
            vx->cso.phoneBook->objectComplete = FALSE;
            vx->len = (U16)(vx->dp - (U8 *)vx->cso.phoneBook);
            break;

        case VNOTE:
            vx->cso.note->objectComplete = FALSE;
            vx->len = (U16)(vx->dp - (U8 *)vx->cso.note);
            break;

        case VEVENT:
            vx->cso.event->objectComplete = FALSE;
            vx->len = (U16)(vx->dp - (U8 *)vx->cso.event);
            break;

        case VTODO:
            vx->cso.toDo->objectComplete = FALSE;
            vx->len = (U16)(vx->dp - (U8 *)vx->cso.toDo);
            break;

        default:
            break;
        }

        break;
    }
}

/*---------------------------------------------------------------------------
 *            vpFreePhoneBook()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of freeing a PhoneBook recursively.
 * 
 */
void vpFreePhoneBook(SyncPhoneBook *pb)
{
    if (pb->next) {
        vpFreePhoneBook(pb->next);
    }
    OS_Free(pb);
}


/*---------------------------------------------------------------------------
 *            vpFreeNote()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of freeing a Note recursively.
 * 
 */
void vpFreeNote(SyncNote *n)
{
    if (n->next) {
        vpFreeNote(n->next);
    }
    OS_Free(n);
}

/*---------------------------------------------------------------------------
 *            vpFreeToDo()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of freeing a ToDo recursively.
 * 
 */
void vpFreeToDo(SyncToDo *t)
{
    if (t->next) {
        vpFreeToDo(t->next);
    }
    OS_Free(t);
}

/*---------------------------------------------------------------------------
 *            vpFreeEvent()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of freeing an Event recursively.
 * 
 */
void vpFreeEvent(SyncEvent *e)
{
    if (e->next) {
        vpFreeEvent(e->next);
    }
    OS_Free(e);
}

/*---------------------------------------------------------------------------
 *            vpStoreCardProperty()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of storing a PhoneBook property.
 * 
 */
void vpStoreCardProperty(VoContext *context)
{
    VoStatus    status;
    U16         len;
    U8          i, j;
    Prop       *entry, *tEnt;
    const VpMap *pbTbl = (VpMap *)&phoneBookMap;
    BOOL        firstBusinessPhone = FALSE, firstHomePhone = FALSE;
    vContext   *vx = ContainingRecord(context, vContext, vox);


    switch (vx->property) {
    case VCARD_N:     /* =Name */
    case VCARD_ADR:
    case VCARD_ORG:
        /* Read these by parts. */
        status = VO_PART_OK;
        for (j=0; status == VO_PART_OK; j++) {
            len = (U16)(vx->len - (vx->dp - vx->cso.phoneBook->data));
            status = VO_ReadPropertyValue(context, vx->dp, &len, TRUE);
            for (i=0; i<VP_NUM_PHONEBOOK_ENTRIES; i++) {
                if (vx->property == pbTbl[i].prop &&
                    j == pbTbl[i].part &&
                    (vx->parameters & pbTbl[i].parm) == pbTbl[i].parm) {
                    entry = (Prop *)(VP_PHONEBOOK_INDEX + pbTbl[i].offset);

                    if (entry->len) {
                        /* Don't overwrite existing value. */
                        continue;
                    }

                    entry->len = len;
                    entry->offset = (U16)(vx->dp - (U8 *)vx->cso.phoneBook);
                    entry->parm = (U16)(vx->parameters | pbTbl[i].parm);
                }
            }
            vx->dp += len;
            *vx->dp = 0;
            vx->dp++;
        }
        break;

    case VCARD_TEL:
        /* TEL needs special handling */
        len = (U16)(vx->len - (vx->dp - vx->cso.phoneBook->data));
        status = VO_ReadPropertyValue(context, vx->dp, &len, FALSE);
        Assert(status == VO_OK);
        for (i=0; i<VP_NUM_PHONEBOOK_ENTRIES; i++) {
            /* Default HOME or WORK parameter to VOICE, if no other
             * parameters are specified.
             */
            if (vx->property == pbTbl[i].prop &&
                !(vx->parameters & ~(VCARD_TEL_PM_HOME | VCARD_TEL_PM_WORK))) {
                vx->parameters |= VCARD_TEL_PM_VOICE;
            }

            if (vx->property == pbTbl[i].prop &&
                (vx->parameters & pbTbl[i].parm) == pbTbl[i].parm) {
                entry = (Prop *)(VP_PHONEBOOK_INDEX + pbTbl[i].offset);

                /* Make sure we don't overwrite first business phone occurrence */
                if (VP_OFFSET(SyncPhoneBook, business_phone) == pbTbl[i].offset) {
                    if (entry->len) {
                        continue;
                    }
                    firstBusinessPhone = TRUE;
                }

                /* Make sure we don't overwrite first home phone occurrence */
                if (VP_OFFSET(SyncPhoneBook, home_phone) == pbTbl[i].offset) {
                    if (entry->len) {
                        continue;
                    }
                    firstHomePhone = TRUE;
                }

                /* Check for second business phone occurrence */
                if (VP_OFFSET(SyncPhoneBook, business_phone2) == pbTbl[i].offset) {
                    tEnt = (Prop *)(VP_PHONEBOOK_INDEX + VP_OFFSET(SyncPhoneBook, business_phone));
                    if (!tEnt->len || firstBusinessPhone) {
                        continue;
                    }
                }

                /* Check for second home phone occurrence */
                if (VP_OFFSET(SyncPhoneBook, home_phone2) == pbTbl[i].offset) {
                    tEnt = (Prop *)(VP_PHONEBOOK_INDEX + VP_OFFSET(SyncPhoneBook, home_phone));
                    if (!tEnt->len || firstHomePhone) {
                        continue;
                    }
                }

                entry->len = len;
                entry->offset = (U16)(vx->dp - (U8 *)vx->cso.phoneBook);
                entry->parm = (U16)(vx->parameters | pbTbl[i].parm);
            }
        }
        vx->dp += len;
        *vx->dp = 0;
        vx->dp++;
        break;
    
    case VCARD_NOTE:
    case VCARD_FN:
    case VCARD_TITLE:
    case VCARD_NICKNAME:
    case VCARD_PHOTO:
    case VCARD_BDAY:
    case VCARD_LABEL:
    case VCARD_EMAIL:
    case VCARD_MAILER:
    case VCARD_TZ:
    case VCARD_GEO:
    case VCARD_ROLE:
    case VCARD_LOGO:
    case VCARD_AGENT:
    case VCARD_CATEGORIES:
    case VCARD_PRODID:
    case VCARD_REV:
    case VCARD_SORTSTRING:
    case VCARD_SOUND:
    case VCARD_UID:
    case VCARD_URL:
    case VCARD_VERSION:
    case VCARD_CLASS:
    case VCARD_KEY:
    case VCARD_LOCATION:
    case X_ESI_CUST1:
    case X_ESI_CUST2:
    case X_ESI_CUST3:
    case X_ESI_CUST4:
    case X_ESI_ASSIST:
    case X_ESI_SPOUSE:
    case X_ESI_CHILDREN:
    case X_ESI_ANNI:
    case X_ESI_PROFESSION:
    case X_ESI_NICK:
    case X_ESI_MANAGER:
    case X_ESI_REFERREDBY:
    case X_ESI_HOBBY:
    case X_ESI_ACCOUNT:
    case X_ESI_FTP:
    case X_ESI_MILEAGE:
    case X_ESI_BILLING:
    case X_ESI_ORGID:
    case X_ESI_GOVID:
    case X_ESI_CUSTID:
    case X_ESI_COMPNAME:
        /* Do not read these by parts. */
        len = (U16)(vx->len - (vx->dp - vx->cso.phoneBook->data));
        status = VO_ReadPropertyValue(context, vx->dp, &len, FALSE);
        Assert(status == VO_OK);
        for (i=0; i<VP_NUM_PHONEBOOK_ENTRIES; i++) {
            if (vx->property == pbTbl[i].prop &&
                (vx->parameters & pbTbl[i].parm) == pbTbl[i].parm) {
                entry = (Prop *)(VP_PHONEBOOK_INDEX + pbTbl[i].offset);

                if (entry->len) {
                    /* Don't overwrite existing value. */
                    continue;
                }

                entry->len = len;
                entry->offset = (U16)(vx->dp - (U8 *)vx->cso.phoneBook);
                entry->parm = (U16)(vx->parameters | pbTbl[i].parm);
            }
        }
        vx->dp += len;
        *vx->dp = 0;
        vx->dp++;
        break;

    case X_ESI_NONE:
    default:
        break;
    }
}

/*---------------------------------------------------------------------------
 *            vpStoreNoteProperty()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of storing a Note property.
 * 
 */
void vpStoreNoteProperty(VoContext *context)
{
    VoStatus    status;
    U16         len;
    U8          i;
    Prop       *entry;
    const VpMap *nTbl = (VpMap *)&noteMap;
    vContext   *vx = ContainingRecord(context, vContext, vox); 

    switch (vx->property) {
    case VNOTE_DCREATED:
    case VNOTE_SUBJECT:
    case VNOTE_BODY:
    case VNOTE_CATEGORIES:
    case VNOTE_LAST_MODIFIED:
    case VNOTE_LAST_CLASS:
        /* Do not read these by parts. */
        len = (U16)(vx->len - (vx->dp - vx->cso.note->data));
        status = VO_ReadPropertyValue(context, vx->dp, &len, FALSE);
        Assert(status == VO_OK);
        for (i=0; i<VP_NUM_NOTE_ENTRIES; i++) {
            if (vx->property == nTbl[i].prop &&
                (vx->parameters & nTbl[i].parm) == nTbl[i].parm) {
                entry = (Prop *)(VP_NOTE_INDEX + nTbl[i].offset);

                if (entry->len) {
                    /* Don't overwrite existing value. */
                    continue;
                }

                entry->len = len;
                entry->offset = (U16)(vx->dp - (U8 *)vx->cso.note);
                entry->parm = (U16)(vx->parameters | nTbl[i].parm);
            }
        }
        vx->dp += len;
        *vx->dp = 0;
        vx->dp++;
        break;

    case X_ESI_NONE:
    default:
        break;
    }
}

/*---------------------------------------------------------------------------
 *            vpStoreEventProperty()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of storing an Event property.
 * 
 */
void vpStoreEventProperty(VoContext *context)
{
    VoStatus    status;
    U16         len;
    U8          i;
    Prop       *entry;
    const VpMap *eTbl = (VpMap *)&eventMap;
    vContext    *vx = ContainingRecord(context, vContext, vox); 

    switch (vx->property) {
    case VEVENT_DTSTART:
    case VEVENT_DTEND:
    case VEVENT_SUMMARY:
    case VEVENT_DALARM:
    case VEVENT_CATEGORIES:
    case VEVENT_LOCATION:
    case VEVENT_DESCRIPTION:
    case VEVENT_TRANSP:
    case VEVENT_CLASS:
    case VEVENT_EXDATE:
    case VEVENT_RRULE:
        /* Do not read these by parts. */
        len = (U16)(vx->len - (vx->dp - vx->cso.event->data));
        status = VO_ReadPropertyValue(context, vx->dp, &len, FALSE);
        Assert(status == VO_OK);
        for (i=0; i<VP_NUM_EVENT_ENTRIES; i++) {
            if (vx->property == eTbl[i].prop &&
                (vx->parameters & eTbl[i].parm) == eTbl[i].parm) {
                entry = (Prop *)(VP_EVENT_INDEX + eTbl[i].offset);

                if (entry->len) {
                    /* Don't overwrite existing value. */
                    continue;
                }

                entry->len = len;
                entry->offset = (U16)(vx->dp - (U8 *)vx->cso.event);
                entry->parm = (U16)(vx->parameters | eTbl[i].parm);
            }
        }
        vx->dp += len;
        *vx->dp = 0;
        vx->dp++;
        break;

    case X_ESI_NONE:
    default:
        break;
    }
}

/*---------------------------------------------------------------------------
 *            vpStoreToDoProperty()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of storing a ToDo property.
 * 
 */
void vpStoreToDoProperty(VoContext *context)
{
    VoStatus    status;
    U16         len;
    U8          i;
    Prop       *entry;
    const VpMap *tTbl = (VpMap *)&toDoMap;
    vContext    *vx = ContainingRecord(context, vContext, vox); 

    switch (vx->property) {
    case VTODO_SUMMARY:
    case VTODO_DUE:
    case VTODO_COMPLETED:
    case VTODO_PRIORITY:
    case VTODO_CLASS:
    case VTODO_CATEGORIES:
    case VTODO_DESCRIPTION:
    case VTODO_DTSTART:
    case VTODO_DALARM:
    case VTODO_EXDATE:
    case VTODO_RRULE:
        /* Do not read these by parts. */
        len = (U16)(vx->len - (vx->dp - vx->cso.toDo->data));
        status = VO_ReadPropertyValue(context, vx->dp, &len, FALSE);
        Assert(status == VO_OK);
        for (i=0; i<VP_NUM_TODO_ENTRIES; i++) {
            if (vx->property == tTbl[i].prop &&
                (vx->parameters & tTbl[i].parm) == tTbl[i].parm) {
                entry = (Prop *)(VP_TODO_INDEX + tTbl[i].offset);

                if (entry->len) {
                    /* Don't overwrite existing value. */
                    continue;
                }

                entry->len = len;
                entry->offset = (U16)(vx->dp - (U8 *)vx->cso.toDo);
                entry->parm = (U16)(vx->parameters | tTbl[i].parm);
            }
        }
        vx->dp += len;
        *vx->dp = 0;
        vx->dp++;
        break;

    case X_ESI_NONE:
    default:
        break;
    }
}

/*---------------------------------------------------------------------------
 *            vpBuildParms()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Builds the property parameter list.
 * 
 */
void vpBuildParms(U16 parm, const U8 **types, void *val, VoContext *vox)
{
    U8  i;

    for (i=0; types[i]; i++) {
        /* Find the matching parms */
        if ((1 << i) & parm) {

            /* write the parm */
            VO_AddParameter(vox, (char*)types[i], 0);

            if ((((1 << i) & parm) == VCARD_PM_QUOTEDPRINTABLE) &&
                (OS_MemCmp(val, 8, "CHARSET=", 8))) {
                /* add the CHARSET parameter */
                VO_AddParameter(vox, val, 0);
            }
        }
    }
}

/*---------------------------------------------------------------------------
 *            vpGatherParts()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Gathers the parts for a property value.
 * 
 */
void vpGatherParts(U8 ix, const VpMap *tbl, U8 tblSize, void *val, VoContext *vox)
{
    U8      i;
    Prop   *part[10] = {0};
    U16     lastPart = 0;
    U8      partBuf[200], *p;

    /* Find the parts */
    for (i=0; i<tblSize; i++) {
        if (tbl[i].prop == tbl[ix].prop) {
            /* If there are parameters, do they match? */
            if (tbl[ix].parm && !(tbl[i].parm & tbl[ix].parm)) {
                /* No, skip it. */
                continue;
            }
            /* Save a pointer to the value in the part array */
            part[tbl[i].part] = (Prop *)((U8 *)val + tbl[i].offset);
            /* Remember the highest part index */
            if (tbl[i].part > lastPart) {
                lastPart = tbl[i].part;
            }
        }
    }

    /* Write the part values in order */
    p = partBuf;
    for (i=0; i<=lastPart; i++) {
        if (part[i]) {
            /* Don't overrun the part buffer! */
            if ((part[i]->len + (p - partBuf)) < sizeof(partBuf)) {
                OS_MemCopy(p, (U8 *)val + part[i]->offset, part[i]->len);
                p += part[i]->len;
            }
        }

        /* separate the parts with a semi-colon */
        *p = ';';
        p++;
    }
    /* Clear the last semi-colon */
    p--;
    *p = 0;

    /* Write the value */
    VO_AddPropertyValue(vox, (char*)partBuf, (U16)(p - partBuf));
}

/*---------------------------------------------------------------------------
 *            vpBuildPhoneBook()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of building a vCard from a PhoneBook.
 * 
 */
static void vpBuildPhoneBook(SyncPhoneBook *pb, VoContext *vox, VpFlags flags)
{
    VoStatus    status;
    U8          i;
    U16         avail;
    Prop       *entry;
#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
    VpFlags     mandatoryFields;
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
    const VpMap *pbTbl = (VpMap *)&phoneBookMap;

    /* Write the BEGIN statement */
    VO_BeginObject(vox, "VCARD");

    for (i=0; i<VP_NUM_PHONEBOOK_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)pb + pbTbl[i].offset);
        if (!entry->len) {
            /* Nothing there, skip it */
            continue;
        }

#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
        /* If we are performing field-level replace and this field is being sent
         * back, then check if this field is changed before sending it back. Note:
         * the Name and Version fields are mandatory for every vCard, so these
         * will be sent regardless if they have changed.
         */
        mandatoryFields = (VPF_FLR_VERSION_FIELD | VPF_FLR_NAME_FIELD);
        if ((flags != VPF_FLR_OFF) && ((pbTbl[i].flrField & mandatoryFields) == 0) &&
            (((flags & pbTbl[i].flrField) == 0) || 
            (((flags & pbTbl[i].flrField) != 0) && (entry->modified == FALSE)))) {
            /* Nothing has changed or this field is not being replaced, skip it */
            continue;
        }
#else /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
        UNUSED_PARAMETER(flags);
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */

        /* First build the property and parm tags */
        switch (pbTbl[i].prop) {
        case VCARD_N:
            /* Write the property only if on part VCARD_N_PT_LAST */
            if (pbTbl[i].part == VCARD_N_PT_LAST) {
                VO_AddProperty(vox, vCard_Types[pbTbl[i].prop]);

                /* add the parameters */
                vpBuildParms(entry->parm, (U8**)vGlobal_Types, pb->charset, vox);

                /* Gather the parts */
                vpGatherParts(i, pbTbl,
                    VP_NUM_PHONEBOOK_ENTRIES, pb, vox);
            }
            continue;

        case VCARD_ADR:
            /* Write the property only if on part VCARD_ADR_PT_CITY */
            if (pbTbl[i].part == VCARD_ADR_PT_CITY) {
                VO_AddProperty(vox, vCard_Types[pbTbl[i].prop]);

                /* add the parameters */
                vpBuildParms(entry->parm, (U8**)vCard_AdrTypes, pb->charset, vox);

                /* Gather the parts */
                vpGatherParts(i, pbTbl,
                    VP_NUM_PHONEBOOK_ENTRIES, pb, vox);
            }
            continue;

        case VCARD_ORG:
            /* Write the property only if on part VCARD_ORG_PT_COMPANY */
            if (pbTbl[i].part == VCARD_ORG_PT_COMPANY) {
                VO_AddProperty(vox, vCard_Types[pbTbl[i].prop]);

                /* add the parameters */
                vpBuildParms(entry->parm, (U8**)vGlobal_Types, pb->charset, vox);

                /* Gather the parts */
                vpGatherParts(i, pbTbl,
                    VP_NUM_PHONEBOOK_ENTRIES, pb, vox);
            }
            continue;

        case VCARD_TEL:
            /* Write the property */
            VO_AddProperty(vox, vCard_Types[pbTbl[i].prop]);

            /* add the parameters */
            vpBuildParms(entry->parm, (U8**)vCard_TelTypes, pb->charset, vox);
            break;

        case VCARD_EMAIL:
            /* Write the property */
            VO_AddProperty(vox, vCard_Types[pbTbl[i].prop]);

            /* add the parameters */
            vpBuildParms(entry->parm, (U8**)vCard_EmlTypes, pb->charset, vox);
            break;

        case VCARD_URL:
            /* Write the property */
            VO_AddProperty(vox, vCard_Types[pbTbl[i].prop]);

            /* add the parameters */
            vpBuildParms(entry->parm, (U8**)vCard_UrlTypes, pb->charset, vox);
            break;

        case VCARD_NOTE:
        case VCARD_FN:
        case VCARD_TITLE:
        case VCARD_NICKNAME:
        case VCARD_PHOTO:
        case VCARD_BDAY:
        case VCARD_LABEL:
        case VCARD_MAILER:
        case VCARD_TZ:
        case VCARD_GEO:
        case VCARD_ROLE:
        case VCARD_LOGO:
        case VCARD_AGENT:
        case VCARD_CATEGORIES:
        case VCARD_PRODID:
        case VCARD_REV:
        case VCARD_SORTSTRING:
        case VCARD_SOUND:
        case VCARD_UID:
        case VCARD_VERSION:
        case VCARD_CLASS:
        case VCARD_KEY:
        case VCARD_LOCATION:
        case X_ESI_CUST1:
        case X_ESI_CUST2:
        case X_ESI_CUST3:
        case X_ESI_CUST4:
        case X_ESI_ASSIST:
        case X_ESI_SPOUSE:
        case X_ESI_CHILDREN:
        case X_ESI_ANNI:
        case X_ESI_PROFESSION:
        case X_ESI_NICK:
        case X_ESI_MANAGER:
        case X_ESI_REFERREDBY:
        case X_ESI_HOBBY:
        case X_ESI_ACCOUNT:
        case X_ESI_FTP:
        case X_ESI_MILEAGE:
        case X_ESI_BILLING:
        case X_ESI_ORGID:
        case X_ESI_GOVID:
        case X_ESI_CUSTID:
        case X_ESI_COMPNAME:
            /* Write the property */
            VO_AddProperty(vox, vCard_Types[pbTbl[i].prop]);

            /* add the parameters */
            vpBuildParms(entry->parm, (U8**)vGlobal_Types, pb->charset, vox);
            break;

        case X_ESI_NONE:
            break;

        default:
            return;
        }

        /* Write the value */
        if (entry->len) {
            status = VO_AddPropertyValue(vox, (char*)((U8 *)pb + entry->offset), entry->len);
            if (status == VO_EOF) {
                /* Write what we can and end the card. */
                avail = VO_GetPropertyValueAvailLen(vox);
                if (avail > sizeof("END:VCARD\r\n")) {
                    avail -= sizeof("END:VCARD\r\n");
                } else break;
                status = VO_AddPropertyValue(vox, (char*)((U8 *)pb + entry->offset), avail);
                Assert(status == VO_OK);
                break;
            }
        }
    }

    /* Write the END statement */
    VO_EndObject(vox, "VCARD");
}

/*---------------------------------------------------------------------------
 *            vpBuildNote()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of building a vNote.
 * 
 */
static void vpBuildNote(SyncNote *n, VoContext *vox, VpFlags flags)
{
    U8          i;
    Prop       *entry;
#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
    VpFlags     mandatoryFields;
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */

    const VpMap *nTbl = (VpMap *)&noteMap;

    /* Write the BEGIN statement */
    VO_BeginObject(vox, "VNOTE");

    for (i=0; i<VP_NUM_NOTE_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)n + nTbl[i].offset);
        if (!entry->len || nTbl[i].part) {
            /* Nothing to copy or redundant, skip it. */
            continue;
        }


#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
        /* If we are performing field-level replace and this field is being sent
         * back, then check if this field is changed before sending it back. Note:
         * the Version field is mandatory for every vNote, so it
         * will be sent regardless if it has changed.
         */
        mandatoryFields = VPF_FLR_VERSION_FIELD;
        if ((flags != VPF_FLR_OFF) && ((nTbl[i].flrField & mandatoryFields) == 0) && 
            (((flags & nTbl[i].flrField) == 0) || 
            (((flags & nTbl[i].flrField) != 0) && (entry->modified == FALSE)))) {
            /* Nothing has changed or this field is not being replaced, skip it */
            continue;
        }
#else /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
        UNUSED_PARAMETER(flags);
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */

        /* First build the property tag */
        switch (nTbl[i].prop) {
        case VNOTE_DCREATED:
        case VNOTE_SUBJECT:
        case VNOTE_BODY:
        case VNOTE_CATEGORIES:
        case VNOTE_LAST_MODIFIED:
        case VNOTE_LAST_CLASS:
            /* Write the property */
            VO_AddProperty(vox, vNote_Types[nTbl[i].prop]);

            /* add the parameters */
            vpBuildParms(entry->parm, (U8**)vGlobal_Types, n->charset, vox);
            break;

        case X_ESI_NONE:
            break;

        default:
            return;
        }

        /* Write the value */
        if (entry->len) {
            VO_AddPropertyValue(vox, (char*)((U8 *)n + entry->offset), entry->len);
        }
    }

    /* Write the END statement */
    VO_EndObject(vox, "VNOTE");
}

/*---------------------------------------------------------------------------
 *            vpBuildToDo()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of building a vToDo.
 * 
 */
static void vpBuildToDo(SyncToDo *t, VoContext *vox, VpFlags flags)
{
    U8          i;
    U16         avail;
    Prop       *entry;
    VoStatus    status;
#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
    VpFlags     mandatoryFields;
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
    const VpMap *tTbl = (VpMap *)&toDoMap;

    /* Write the BEGIN statement */
    VO_BeginObject(vox, "VTODO");

    for (i=0; i<VP_NUM_TODO_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)t + tTbl[i].offset);
        if (!entry->len || tTbl[i].part) {
            /* Nothing to copy or redundant, skip it. */
            continue;
        }

#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
        /* If we are performing field-level replace and this field is being sent
         * back, then check if this field is changed before sending it back. Note:
         * the Version field is mandatory for every vTodo, so it
         * will be sent regardless if it has changed.
         */
        mandatoryFields = VPF_FLR_VERSION_FIELD;
        if ((flags != VPF_FLR_OFF) && ((tTbl[i].flrField & mandatoryFields) == 0) && 
            (((flags & tTbl[i].flrField) == 0) || 
            (((flags & tTbl[i].flrField) != 0) && (entry->modified == FALSE)))) {
            /* Nothing has changed or this field is not being replaced, skip it */
            continue;
        }
#else /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
        UNUSED_PARAMETER(flags);
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */

        /* First build the property tag */
        switch (tTbl[i].prop) {
        case VTODO_SUMMARY:
        case VTODO_DUE:
        case VTODO_COMPLETED:
        case VTODO_PRIORITY:
        case VTODO_CLASS:
        case VTODO_CATEGORIES:
        case VTODO_DESCRIPTION:
        case VTODO_DTSTART:
        case VTODO_DALARM:
        case VTODO_EXDATE:
        case VTODO_RRULE:
            /* Write the property */
            VO_AddProperty(vox, vCal_ToDoTypes[tTbl[i].prop]);

            /* add the parameters */
            vpBuildParms(entry->parm, (U8**)vGlobal_Types, t->charset, vox);
            break;

        case X_ESI_NONE:
            break;

        default:
            return;
        }

        /* Write the value */
        if (entry->len) {
            status = VO_AddPropertyValue(vox, (char*)((U8 *)t + entry->offset), entry->len);
            if (status == VO_EOF) {
                avail = VO_GetPropertyValueAvailLen(vox);
                if (avail > sizeof("END:VTODO\r\nEND:VCALENDAR\r\n")) {
                    avail -= sizeof("END:VTODO\r\nEND:VCALENDAR\r\n");
                } else break;
                status = VO_AddPropertyValue(vox, (char*)((U8 *)t + entry->offset), avail);
                Assert(status == VO_OK);
            }
        }
    }

    /* Write the END statement */
    VO_EndObject(vox, "VTODO");
}

/*---------------------------------------------------------------------------
 *            vpBuildEvent()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does the work of building a vEvent.
 * 
 */
static void vpBuildEvent(SyncEvent *e, VoContext *vox, VpFlags flags)
{
    U8          i;
    U16         avail;
    VoStatus    status;
    Prop       *entry;
#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
    VpFlags     mandatoryFields;
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
    const VpMap *eTbl = (VpMap *)&eventMap;

    /* Write the BEGIN statement */
    VO_BeginObject(vox, "VEVENT");

    for (i=0; i<VP_NUM_EVENT_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)e + eTbl[i].offset);
        if (!entry->len || eTbl[i].part) {
            /* Nothing to copy or redundant, skip it. */
            continue;
        }

#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
        /* If we are performing field-level replace and this field is being sent
         * back, then check if this field is changed before sending it back. Note:
         * the Version field is mandatory for every vEvent, so it
         * will be sent regardless if it has changed.
         */
        mandatoryFields = VPF_FLR_VERSION_FIELD;
        if ((flags != VPF_FLR_OFF) && ((eTbl[i].flrField & mandatoryFields) == 0) && 
            (((flags & eTbl[i].flrField) == 0) || 
            (((flags & eTbl[i].flrField) != 0) && (entry->modified == FALSE)))) {
            /* Nothing has changed or this field is not being replaced, skip it */
            continue;
        }
#else /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */
        UNUSED_PARAMETER(flags);
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */

        /* First build the property tag */
        switch (eTbl[i].prop) {
        case VEVENT_DTSTART:
        case VEVENT_DTEND:
        case VEVENT_SUMMARY:
        case VEVENT_DALARM:
        case VEVENT_CATEGORIES:
        case VEVENT_LOCATION:
        case VEVENT_DESCRIPTION:
        case VEVENT_TRANSP:
        case VEVENT_CLASS:
        case VEVENT_EXDATE:
        case VEVENT_RRULE:
            /* Write the property */
            VO_AddProperty(vox, vCal_EventTypes[eTbl[i].prop]);

            /* add the parameters */
            vpBuildParms(entry->parm, (U8**)vGlobal_Types, e->charset, vox);
            break;

        case X_ESI_NONE:
            break;

        default:
            return;
        }

        /* Write the value */
        if (entry->len) {
            status = VO_AddPropertyValue(vox, (char*)((U8 *)e + entry->offset), entry->len);
            if (status == VO_EOF) {
                avail = VO_GetPropertyValueAvailLen(vox);
                if (avail > sizeof("END:VEVENT\r\nEND:VCALENDAR\r\n")) {
                    avail -= sizeof("END:VEVENT\r\nEND:VCALENDAR\r\n");
                } else break;
                status = VO_AddPropertyValue(vox, (char*)((U8 *)e + entry->offset), avail);
                Assert(status == VO_OK);
            }
        }
    }

    /* Write the END statement */
    VO_EndObject(vox, "VEVENT");
}

/*---------------------------------------------------------------------------
 *            vpEstimateObjectSize()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Does a rough estimate of the size of a vObject required for
 *            the data contained in the SyncObject. The size is purposely
 *            overestimated for efficiency.
 * 
 */
U16 vpEstimateObjectSize(const SyncObject *so)
{
    SyncPhoneBook  *pb = so->phoneBook;
    SyncNote       *n = so->note;
    SyncToDo       *t = so->toDo;
    SyncEvent      *e = so->event;
    U8              i;
    Prop           *entry;
    U16             tLen = 0;
    const VpMap    *pbTbl = (VpMap *)&phoneBookMap;
    const VpMap    *nTbl = (VpMap *)&noteMap;
    const VpMap    *tTbl = (VpMap *)&toDoMap;
    const VpMap    *eTbl = (VpMap *)&eventMap;

    /* Make sure we have only one object to process */
    i = 0;
    if (pb) i++; if (n) i++; if (t) i++; if (e) i++;
    if (i != 1) return 0;

    while (pb) {
        tLen += 40; /* Add for VCARD header */
        /* Calculate the object size */
        for (i=0; i<VP_NUM_PHONEBOOK_ENTRIES; i++) {
            /* Point to the value */
            entry = (Prop *)((U8 *)pb + pbTbl[i].offset);
            if (entry->len) {
                tLen = (U16)(tLen + entry->len + 10); /* Add 10 bytes for tags */
                
                /* additional account for folding*/
                if ((entry->len / 70) > 0) {
                    tLen = (U16)(tLen + (entry->len / 70) * 3 + 3);
                }
            }
            if (entry->parm) {
                tLen = (U16)(tLen + 20); /* Add 20 bytes for parms */
            }
        }

        pb = pb->next;
    }

    while (n) {
        tLen += 40; /* Add for NOTE header */
        /* Calculate the object size */
        for (i=0; i<VP_NUM_NOTE_ENTRIES; i++) {
            /* Point to the value */
            entry = (Prop *)((U8 *)n + nTbl[i].offset);
            if (entry->len) {
                tLen = (U16)(tLen + entry->len + 12); /* Add 12 bytes for tags */
                
                /* additional account for folding*/
                if ((entry->len / 70) > 0) {
                    tLen = (U16)(tLen + (entry->len / 70) * 3 + 3);
                }
            }
            if (entry->parm) {
                tLen = (U16)(tLen + 20); /* Add 20 bytes for parms */
            }
        }

        n = n->next;
    }

    while (t) {
        if (tLen == 0) tLen = 45; /* Add for VCALENDAR header */
        tLen += 25; /* Add for VTODO header */
        /* Calculate the object size */
        for (i=0; i<VP_NUM_TODO_ENTRIES; i++) {
            /* Point to the value */
            entry = (Prop *)((U8 *)t + tTbl[i].offset);
            if (entry->len) {
                tLen = (U16)(tLen + entry->len + 12); /* Add 12 bytes for tags */
                
                /* additional account for folding*/
                if ((entry->len / 70) > 0) {
                    tLen = (U16)(tLen + (entry->len / 70) * 3 + 3);
                }
            }
            if (entry->parm) {
                tLen = (U16)(tLen + 20); /* Add 20 bytes for parms */
            }
        }

        t = t->next;
    }

    while (e) {
        if (tLen == 0) tLen = 45; /* Add for VCALENDAR header */
        tLen += 25; /* Add for VEVENT header */
        /* Calculate the object size */
        for (i=0; i<VP_NUM_EVENT_ENTRIES; i++) {
            /* Point to the value */
            entry = (Prop *)((U8 *)e + eTbl[i].offset);
            if (entry->len) {
                tLen = (U16)(tLen + entry->len + 12); /* Add 12 bytes for tags */
                
                /* additional account for folding*/
                if ((entry->len / 70) > 0) {
                    tLen = (U16)(tLen + (entry->len / 70) * 3 + 3);
                }
            }
            if (entry->parm) {
                tLen += 20; /* Add 20 bytes for parms */
            }
        }

        e = e->next;
    }

    return tLen;
}

#if SML_FIELD_LEVEL_REPLACE == XA_ENABLED
/*---------------------------------------------------------------------------
 *            vCompareFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Compare the vObject fields based on the SyncObject type 
 *            (phonebook/event/todo/note) and mark any changed fields for 
 *            upcoming field-level replace synchronizations.
 */
void vCompareFields(const void *savedObj, void *rcvdObj)
{
    /* Check for a vObject header byte */
    switch (((U8 *)rcvdObj)[0]) {
    case 'p': /* Phonebook */
        vComparePhonebookFields((SyncPhoneBook *)savedObj, (SyncPhoneBook *)rcvdObj);
        break;
    case 'n': /* Note */
        vCompareNoteFields((SyncNote *)savedObj, (SyncNote *)rcvdObj);
        break;
    case 't': /* ToDo */
        vCompareToDoFields((SyncToDo *)savedObj, (SyncToDo *)rcvdObj);
        break;
    case 'e': /* Event */
        vCompareEventFields((SyncEvent *)savedObj, (SyncEvent *)rcvdObj);
        break;
    }
}

/*---------------------------------------------------------------------------
 *            vResetFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Reset all of the changed vObject fields based on the SyncObject 
 *            type (phonebook/event/todo/note) as the field-level 
 *            synchronization is now complete.
 */
void vResetFields(void *obj)
{
    /* Check for a vObject header byte */
    switch (((U8 *)obj)[0]) {
    case 'p': /* Phonebook */
        vResetPhonebookFields((SyncPhoneBook *)obj);
        break;
    case 'n': /* Note */
        vResetNoteFields((SyncNote *)obj);
        break;
    case 't': /* ToDo */
        vResetToDoFields((SyncToDo *)obj);
        break;
    case 'e': /* Event */
        vResetEventFields((SyncEvent *)obj);
        break;
    }
}

/*---------------------------------------------------------------------------
 *            vFieldLevelReplace()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Process the vObject fields received for the SyncObject type 
 *            (phonebook/event/todo/note) performed in the field-level 
 *            synchronization.  This process will combine the saved object 
 *            and received object fields into one combined object that will 
 *            be returned.
 */
void *vFieldLevelReplace(const void *savedObj, const void *rcvdObj, U16 *len)
{
    void    *data = 0;

    /* Check for a vObject header byte */
    switch (((U8 *)rcvdObj)[0]) {
    case 'p': /* Phonebook */
        data = vFlrPhonebook((SyncPhoneBook *)savedObj, (SyncPhoneBook *)rcvdObj, len);
        break;
    case 'n': /* Note */
        data = vFlrNote((SyncNote *)savedObj, (SyncNote *)rcvdObj, len);
        break;
    case 't': /* ToDo */
        data = vFlrToDo((SyncToDo *)savedObj, (SyncToDo *)rcvdObj, len);
        break;
    case 'e': /* Event */
        data = vFlrEvent((SyncEvent *)savedObj, (SyncEvent *)rcvdObj, len);
        break;
    }

    return data;
}

/*---------------------------------------------------------------------------
 *            vReleaseFieldLevelRelace()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Release the combined SyncObject allocated in the 
 *            vFieldLevelReplace function.
 */
void vReleaseFieldLevelRelace(const void *obj)
{
    /* Nothing to free */
    if (!obj) return;

    switch (((U8 *)obj)[0]) {
    case 'p': /* Phonebook */
        vpFreePhoneBook((SyncPhoneBook *)obj);
        break;
    case 'n': /* Note */
        vpFreeNote((SyncNote *)obj);
        break;
    case 't': /* ToDo */
        vpFreeToDo((SyncToDo *)obj);
        break;
    case 'e': /* Event */
        vpFreeEvent((SyncEvent *)obj);
        break;
    }
}

/*---------------------------------------------------------------------------
 *            vCopySoProperty()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Copy a Prop for use in creating a combined SyncObject
 *            for field-level replace.
 */
static void vCopySoProperty(Prop *destProp, void *destObj,
                            const Prop *srcProp, const void *srcObj, U16 *offset)
{
    U8      *destPtr, *srcPtr, nullTerm;

    nullTerm = 0;

    /* Copy the New Properties data portion */
    destPtr = (U8 *)destObj + *offset;
    srcPtr = (U8 *)srcObj + srcProp->offset;
    OS_MemCopy(destPtr, srcPtr, srcProp->len);

    /* Check for null-termination to copy */
    if (srcPtr[srcProp->len] == 0x00) {
        destPtr[srcProp->len] = 0x00;
        nullTerm = 1;
    }

    /* Setup the new property fields */
    destProp->offset = *offset;
    destProp->modified = FALSE;
    destProp->parm = srcProp->parm;
    destProp->len = srcProp->len;

    /* Assign new offset value into the data portion */
    *offset += (srcProp->len + nullTerm);
}

/*---------------------------------------------------------------------------
 *            vFlrPhonebook()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Process the vObject fields received for the phonebook SyncObject 
 *            type performed in the field-level synchronization. This process 
 *            will combine the saved object and received object fields into one 
 *            combined phonebook SyncObject that will be returned.            
 */
static SyncPhoneBook *vFlrPhonebook(const SyncPhoneBook *SavedPb, 
                                    const SyncPhoneBook *RcvdPb, U16 *Len)
{
    U8                  i;
    U16                 offset, len;
    SyncPhoneBook      *newPb;
    Prop               *entry, *newEntry, *savedEntry;
    const VpMap        *phoneBookTbl = (VpMap *)&phoneBookMap;

    /* Offset originally equals the number of bytes between the start of the
     * SyncObject and the data portion.
     */
    len = sizeof(SyncPhoneBook) + 4096;     //TBD: figure out better constant
    if ((newPb = OS_Malloc(len)) == 0) {
        return 0;
    }
    OS_MemSet((U8 *)newPb, 0, len);
    newPb->data = (U8 *)(newPb + 1);
    offset = (newPb->data - (U8 *)newPb);

    /* Copy the PhoneBook information */
    newPb->header = RcvdPb->header;
    newPb->dbVersion = RcvdPb->dbVersion;
    newPb->objectComplete = RcvdPb->objectComplete;
    for (i = 0; i < VP_CHARSET_LEN; i++) {
        newPb->charset[i] = RcvdPb->charset[i];
    }
    newPb->next = RcvdPb->next;

    for (i = 0; i < VP_NUM_PHONEBOOK_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)RcvdPb + phoneBookTbl[i].offset);
        savedEntry = (Prop *)((U8 *)SavedPb + phoneBookTbl[i].offset);
        newEntry = (Prop *)((U8 *)newPb + phoneBookTbl[i].offset);

        if (entry->len > 0) {
            /* This field has been replaced - use received value */
            vCopySoProperty(newEntry, newPb, entry, RcvdPb, &offset);
        } else if (savedEntry->len > 0) {
            /* Unchanged field - use previously saved value */
            vCopySoProperty(newEntry, newPb, savedEntry, SavedPb, &offset);
        }
    }

    /* Pass back the overall length of this object */
    *Len = offset;
    /* Return the new phoneBook object */
    return newPb;
}

/*---------------------------------------------------------------------------
 *            vFlrEvent()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Process the vObject fields received for the event SyncObject 
 *            type performed in the field-level synchronization. This process 
 *            will combine the saved object and received object fields into one 
 *            combined event SyncObject that will be returned.            
 */
static SyncEvent *vFlrEvent(const SyncEvent *SavedEvent, 
                            const SyncEvent *RcvdEvent, U16 *Len)
{
    U8                  i;
    U16                 offset, len;
    SyncEvent          *newEvent;
    Prop               *entry, *savedEntry, *newEntry;
    const VpMap        *eventTbl = (VpMap *)&eventMap;

    /* Offset originally equals the number of bytes between the start of the
     * SyncObject and the data portion.
     */
    len = sizeof(SyncEvent) + 4096;     //TBD: figure out better constant
    if ((newEvent = OS_Malloc(len)) == 0) {
        return 0;
    }
    OS_MemSet((U8 *)newEvent, 0, len);
    newEvent->data = (U8 *)(newEvent + 1);
    offset = (newEvent->data - (U8 *)newEvent);

    /* Copy the Event information */
    newEvent->header = RcvdEvent->header;
    newEvent->dbVersion = RcvdEvent->dbVersion;
    newEvent->objectComplete = RcvdEvent->objectComplete;
    for (i = 0; i < VP_CHARSET_LEN; i++) {
        newEvent->charset[i] = RcvdEvent->charset[i];
    }
    newEvent->next = RcvdEvent->next;

    for (i = 0; i < VP_NUM_EVENT_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)RcvdEvent + eventTbl[i].offset);
        savedEntry = (Prop *)((U8 *)SavedEvent + eventTbl[i].offset);
        newEntry = (Prop *)((U8 *)newEvent + eventTbl[i].offset);

        if (entry->len > 0) {
            /* This field has been replaced - use received value */
            vCopySoProperty(newEntry, newEvent, entry, RcvdEvent, &offset);
        } else if (savedEntry->len > 0) {
            /* Unchanged field - use previously saved value */
            vCopySoProperty(newEntry, newEvent, savedEntry, SavedEvent, &offset);
        }
    }

    /* Pass back the overall length of this object */
    *Len = offset;
    /* Return the new event object */
    return newEvent;
}

/*---------------------------------------------------------------------------
 *            vFlrToDo()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Process the vObject fields received for the todo SyncObject 
 *            type performed in the field-level synchronization. This process 
 *            will combine the saved object and received object fields into one 
 *            combined todo SyncObject that will be returned.            
 */
static SyncToDo *vFlrToDo(const SyncToDo *SavedToDo, 
                          const SyncToDo *RcvdToDo, U16 *Len)
{
    U8                  i;
    U16                 offset, len;
    SyncToDo           *newToDo;
    Prop               *entry, *savedEntry, *newEntry;
    const VpMap        *toDoTbl = (VpMap *)&toDoMap;

    /* Offset originally equals the number of bytes between the start of the
     * SyncObject and the data portion.
     */
    len = sizeof(SyncToDo) + 4096;     //TBD: figure out better constant
    if ((newToDo = OS_Malloc(len)) == 0) {
        return 0;
    }
    OS_MemSet((U8 *)newToDo, 0, len);
    newToDo->data = (U8 *)(newToDo + 1);
    offset = (newToDo->data - (U8 *)newToDo);

    /* Copy the ToDo information */
    newToDo->header = RcvdToDo->header;
    newToDo->dbVersion = RcvdToDo->dbVersion;
    newToDo->objectComplete = RcvdToDo->objectComplete;
    for (i = 0; i < VP_CHARSET_LEN; i++) {
        newToDo->charset[i] = RcvdToDo->charset[i];
    }
    newToDo->next = RcvdToDo->next;

    for (i = 0; i < VP_NUM_TODO_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)RcvdToDo + toDoTbl[i].offset);
        savedEntry = (Prop *)((U8 *)SavedToDo + toDoTbl[i].offset);
        newEntry = (Prop *)((U8 *)newToDo + toDoTbl[i].offset);

        if (entry->len > 0) {
            /* This field has been replaced - use received value */
            vCopySoProperty(newEntry, newToDo, entry, RcvdToDo, &offset);
        } else if (savedEntry->len > 0) {
            /* Unchanged field - use previously saved value */
            vCopySoProperty(newEntry, newToDo, savedEntry, SavedToDo, &offset);
        }
    }

    /* Pass back the overall length of this object */
    *Len = offset;
    /* Return the new toDo object */
    return newToDo;
}

/*---------------------------------------------------------------------------
 *            vFlrNote()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Process the vObject fields received for the note SyncObject 
 *            type performed in the field-level synchronization. This process 
 *            will combine the saved object and received object fields into one 
 *            combined note SyncObject that will be returned.            
 */
static SyncNote *vFlrNote(const SyncNote *SavedNote, const SyncNote *RcvdNote, 
                          U16 *Len)
{
    U8                  i;
    U16                 offset, len;
    SyncNote           *newNote;
    Prop               *entry, *savedEntry, *newEntry;
    const VpMap        *noteTbl = (VpMap *)&noteMap;

    /* Offset originally equals the number of bytes between the start of the
     * SyncObject and the data portion.
     */
    len = sizeof(SyncNote) + 4096;     //TBD: figure out better constant
    if ((newNote = OS_Malloc(len)) == 0) {
        return 0;
    }
    OS_MemSet((U8 *)newNote, 0, len);
    newNote->data = (U8 *)(newNote + 1);
    offset = (newNote->data - (U8 *)newNote);

    /* Copy the Note information */
    newNote->header = RcvdNote->header;
    newNote->dbVersion = RcvdNote->dbVersion;
    newNote->objectComplete = RcvdNote->objectComplete;
    for (i = 0; i < VP_CHARSET_LEN; i++) {
        newNote->charset[i] = RcvdNote->charset[i];
    }
    newNote->next = RcvdNote->next;

    for (i = 0; i < VP_NUM_NOTE_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)RcvdNote + noteTbl[i].offset);
        savedEntry = (Prop *)((U8 *)SavedNote + noteTbl[i].offset);
        newEntry = (Prop *)((U8 *)newNote + noteTbl[i].offset);

        if (entry->len > 0) {
            /* This field has been replaced - use received value */
            vCopySoProperty(newEntry, newNote, entry, RcvdNote, &offset);
        } else if (savedEntry->len > 0) {
            /* Unchanged field - use previously saved value */
            vCopySoProperty(newEntry, newNote, savedEntry, SavedNote, &offset);
        }
    }

    /* Pass back the overall length of this object */
    *Len = offset;
    /* Return the new note object */
    return newNote;
}

/*---------------------------------------------------------------------------
 *            vComparePhonebookFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Compare the vObject fields in the phonebook SyncObject type 
 *            and mark any changed fields for upcoming field-level replace 
 *            synchronizations.             
 */
static void vComparePhonebookFields(const SyncPhoneBook *SavedPb, SyncPhoneBook *RcvdPb)
{
    U8                  i;
    Prop               *entry, *savedEntry;
    const VpMap        *phoneBookTbl = (VpMap *)&phoneBookMap;

    Assert(RcvdPb);

    for (i = 0; i < VP_NUM_PHONEBOOK_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)RcvdPb + phoneBookTbl[i].offset);
        savedEntry = (Prop *)((U8 *)SavedPb + phoneBookTbl[i].offset);

        if ((!SavedPb) || (entry->len != savedEntry->len) || 
            (!OS_MemCmp(((U8 *)RcvdPb + entry->offset), 
            entry->len, ((U8 *)SavedPb + savedEntry->offset), 
            savedEntry->len))) {
            /* Element was modified (or is new), mark this and move on */
            entry->modified = TRUE;
        } else if (savedEntry->modified) {
            /* If the saved record has modified fields that have not been
             * synced, be sure to incorporate these fields as well.
             */
            entry->modified = TRUE;
        }
    }
}

/*---------------------------------------------------------------------------
 *            vCompareEventFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Compare the vObject fields in the event SyncObject type 
 *            and mark any changed fields for upcoming field-level replace 
 *            synchronizations.             
 */
static void vCompareEventFields(const SyncEvent *SavedEvent, SyncEvent *RcvdEvent)
{
    U8                  i;
    Prop               *entry, *savedEntry;
    const VpMap        *eventTbl = (VpMap *)&eventMap;

    Assert(RcvdEvent);

    for (i = 0; i < VP_NUM_EVENT_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)RcvdEvent + eventTbl[i].offset);
        savedEntry = (Prop *)((U8 *)SavedEvent + eventTbl[i].offset);

        if ((!SavedEvent) || (entry->len != savedEntry->len) || 
            (!OS_MemCmp(((U8 *)RcvdEvent + entry->offset), 
            entry->len, ((U8 *)SavedEvent + savedEntry->offset), 
            savedEntry->len))) {
            /* Element was modified (or is new), mark this and move on */
            entry->modified = TRUE;
        } else if (savedEntry->modified) {
            /* If the saved record has modified fields that have not been
             * synced, be sure to incorporate these fields as well.
             */
            entry->modified = TRUE;
        }
    }
}

/*---------------------------------------------------------------------------
 *            vCompareToDoFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Compare the vObject fields in the todo SyncObject type 
 *            and mark any changed fields for upcoming field-level replace 
 *            synchronizations.             
 */
static void vCompareToDoFields(const SyncToDo *SavedToDo, SyncToDo *RcvdToDo)
{
    U8                  i;
    Prop               *entry, *savedEntry;
    const VpMap        *toDoTbl = (VpMap *)&toDoMap;

    Assert(RcvdToDo);

    for (i = 0; i < VP_NUM_TODO_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)RcvdToDo + toDoTbl[i].offset);
        savedEntry = (Prop *)((U8 *)SavedToDo + toDoTbl[i].offset);

        if ((!SavedToDo) || (entry->len != savedEntry->len) || 
            (!OS_MemCmp(((U8 *)RcvdToDo + entry->offset), 
            entry->len, ((U8 *)SavedToDo + savedEntry->offset), 
            savedEntry->len))) {
            /* Element was modified (or is new), mark this and move on */
            entry->modified = TRUE;
        } else if (savedEntry->modified) {
            /* If the saved record has modified fields that have not been
             * synced, be sure to incorporate these fields as well.
             */
            entry->modified = TRUE;
        }
    }
}

/*---------------------------------------------------------------------------
 *            vCompareNoteFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Compare the vObject fields in the note SyncObject type 
 *            and mark any changed fields for upcoming field-level replace 
 *            synchronizations.             
 */
static void vCompareNoteFields(const SyncNote *SavedNote, SyncNote *RcvdNote)
{
    U8                  i;
    Prop               *entry, *savedEntry;
    const VpMap        *noteTbl = (VpMap *)&noteMap;

    Assert(RcvdNote);

    for (i = 0; i < VP_NUM_NOTE_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)RcvdNote + noteTbl[i].offset);
        savedEntry = (Prop *)((U8 *)SavedNote + noteTbl[i].offset);

        if ((!SavedNote) || (entry->len != savedEntry->len) || 
            (!OS_MemCmp(((U8 *)RcvdNote + entry->offset), 
            entry->len, ((U8 *)SavedNote + savedEntry->offset), 
            savedEntry->len))) {
            /* Element was modified (or is new), mark this and move on */
            entry->modified = TRUE;
        } else if (savedEntry->modified) {
            /* If the saved record has modified fields that have not been
             * synced, be sure to incorporate these fields as well.
             */
            entry->modified = TRUE;
        }
    }
}

/*---------------------------------------------------------------------------
 *            vResetPhonebookFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Reset all of the changed fields in the phonebook SyncObject 
 *            type as the field-level synchronization is now complete.
 */
static void vResetPhonebookFields(SyncPhoneBook *Pb)
{
    U8                  i;
    Prop               *entry;
    const VpMap        *phoneBookTbl = (VpMap *)&phoneBookMap;

    Assert(Pb);

    for (i = 0; i < VP_NUM_PHONEBOOK_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)Pb + phoneBookTbl[i].offset);

        /* Reset all fields to indicate no modifications, as the 
         * phonebook has just been synchronized.
         */
        entry->modified = FALSE;
    }
}


/*---------------------------------------------------------------------------
 *            vResetEventFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Reset all of the changed fields in the event SyncObject 
 *            type as the field-level synchronization is now complete.
 */
static void vResetEventFields(SyncEvent *Event)
{
    U8                  i;
    Prop               *entry;
    const VpMap        *eventTbl = (VpMap *)&eventMap;

    Assert(Event);

    for (i = 0; i < VP_NUM_EVENT_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)Event + eventTbl[i].offset);

        /* Reset all fields to indicate no modifications, as the 
         * event has just been synchronized.
         */
        entry->modified = FALSE;
    }
}

/*---------------------------------------------------------------------------
 *            vResetToDoFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Reset all of the changed fields in the todo SyncObject 
 *            type as the field-level synchronization is now complete.
 */
static void vResetToDoFields(SyncToDo *ToDo)
{
    U8                  i;
    Prop               *entry;
    const VpMap        *toDoTbl = (VpMap *)&toDoMap;

    Assert(ToDo);

    for (i = 0; i < VP_NUM_TODO_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)ToDo + toDoTbl[i].offset);

        /* Reset all fields to indicate no modifications, as the 
         * toDo has just been synchronized.
         */
        entry->modified = FALSE;
    }
}

/*---------------------------------------------------------------------------
 *            vResetNoteFields()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Reset all of the changed fields in the note SyncObject 
 *            type as the field-level synchronization is now complete.
 */
static void vResetNoteFields(SyncNote *Note)
{
    U8                  i;
    Prop               *entry;
    const VpMap        *noteTbl = (VpMap *)&noteMap;

    Assert(Note);

    for (i = 0; i < VP_NUM_NOTE_ENTRIES; i++) {
        /* Point to the value */
        entry = (Prop *)((U8 *)Note + noteTbl[i].offset);

        /* Reset all fields to indicate no modifications, as the 
         * toDo has just been synchronized.
         */
        entry->modified = FALSE;
    }
}
#endif /* SML_FIELD_LEVEL_REPLACE == XA_ENABLED */

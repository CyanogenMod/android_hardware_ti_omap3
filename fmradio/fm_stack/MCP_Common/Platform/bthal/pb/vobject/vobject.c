/****************************************************************************
 *
 * File:
 *     iAnywhere Data Sync SDK
 *     $Id:vobject.c,v 1.25, 2006-01-13 04:41:02Z, Kevin Hendrix$
 *
 * Description:
 *     The vObject parser. The parser is designed to work with all types
 *     and versions of vObjects.
 * 
 * Copyright 2002-2006 Extended Systems, Inc.
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

/* vObject positional states. */
#define VPOS_STAGE_PROP         0   /* Read the Property Name into the stage */
#define VPOS_STAGE_PARM         1   /* Read the Property Parms into the stage */
#define VPOS_STAGE_OBJ_TYPE     2   /* Read the object type into the stage */
#define VPOS_PROP_VALUE         4   /* Indicate the Property Value */
#define VPOS_PARM_VALUE         5   /* Indicate the Parameter Value */

/* vObject current token states. */
#define VTOK_CHAR               0   /* Current character is a normal character */
#define VTOK_CR_SKIPPED         1   /* CR character was consumed */
#define VTOK_CRLF_SKIPPED       2   /* CR-LF was consumed */
#define VTOK_EOL                4   /* End-of-line was consumed */
#define VTOK_EQUALS_SKIPPED     5   /* Start of soft line break detected and skipped */
#define VTOK_CR_SKIPPED_QP      6   /* CR character consumed in Quoted-Printable soft line break */
#define VTOK_CRLF_SKIPPED_QP    7   /* CR-LF consumed in Quoted-Printable soft line break */
#define VTOK_CONSUME            8   /* Current character should be consumed */

/* vObject token/position state flags. */
#define VOF_BEGIN           0x01    /* Start of a new object */
#define VOF_END             0x02    /* End of current object */
#define VOF_ESCAPE          0x04    /* Escape ('\') character found */
#define VOF_QUOTED          0x08    /* Within a quoted ('"') value */
#define VOF_END_PARM        0x10    /* Need to indicate VOE_PARAMETER_END event */


/* Internal functions */
static BOOL     GetToken(VoContext *Vo);
static void     CheckParameters(VoContext *Vo);
static void     vWrite(VoContext *Vo, const char *Value, U16 Length, VoEncoding Encoding);
static VoStatus SkipValue(VoContext *Vo, BOOL ByParts);

#define VoCallback(_EVENT)     do {cbParms.event = (_EVENT);Vo->callback(&cbParms);} while(0);

/*----------------------------------------------------------------------------
 *            VO_Init()
 *----------------------------------------------------------------------------
 * Synopsis: Initialize the VoContext context.
 * 
 */
VoStatus VO_Init(VoContext *Vo, VoCallback Callback)
{
    CheckUnlockedParm(VO_INVALID_PARM, (Vo && Callback));

    OS_MemSet((U8 *)Vo, 0, sizeof(VoContext));

    Vo->callback = Callback;

    return VO_OK;
}

/*----------------------------------------------------------------------------
 *            VO_Parse()
 *----------------------------------------------------------------------------
 * Synopsis: Parse vObject data. This parser does not perform any special
 *           handling for groups.
 */
U16 VO_Parse(VoContext *Vo, const U8 *Buffer, U16 BufferLen)
{
    VoCallbackParms cbParms;

    CheckUnlockedParm(BufferLen, Vo && Vo->callback);

    Vo->buffer = Buffer;
    Vo->bufferLen = BufferLen;

    cbParms.context = Vo;
    cbParms.u.object = (const char *)Vo->stage;

    while (GetToken(Vo)) {

        if (Vo->vTok == VTOK_CHAR) {
            switch (Vo->vPos) {

            /* VPOS_STAGE_PROP Semi's start parameters, colons start prop value, comma's ignore, equals ignore, un-fold. */
            /* VPOS_STAGE_PARM Semi's starts next parm, colons start prop value, comma's ignore, equals starts parm value, un-fold */
            case VPOS_STAGE_PARM:
            case VPOS_STAGE_PROP:
                if ((Vo->flags & VOF_QUOTED) == 0) {
                    switch (Vo->chr) {
                    case ';':
                        if ((Vo->flags & VOF_ESCAPE) == 0) {
                            Vo->stage[Vo->stageLen] = 0;
                            Vo->stageLen = 0;
                        
                            if (Vo->vPos == VPOS_STAGE_PARM) {
                                /* Start of Parameter */
                                if (Vo->flags & VOF_END_PARM)
                                    VoCallback(VOE_PARAMETER_END);
                            
                                CheckParameters(Vo);
                                VoCallback(VOE_PARAMETER_BEGIN);
                                Vo->flags |= VOF_END_PARM;
                                continue;
                            }
                            if (Vo->vPos == VPOS_STAGE_PROP) {
                                VoCallback(VOE_PROPERTY_BEGIN);
                                Vo->vPos = VPOS_STAGE_PARM;
                                continue;
                            }
                        }
                        Vo->flags &= ~VOF_ESCAPE;
                        break;
                    
                    case ':':
                        if ((Vo->flags & VOF_ESCAPE) == 0) {
                            /* Start of property value */
                            Vo->stage[Vo->stageLen] = 0;
                            Vo->stageLen = 0;

                            if (Vo->vPos == VPOS_STAGE_PARM) {
                                if (Vo->flags & VOF_END_PARM)
                                    VoCallback(VOE_PARAMETER_END);
                                
                                CheckParameters(Vo);
                                VoCallback(VOE_PARAMETER_BEGIN);
                                Vo->flags |= VOF_END_PARM;
                            }
                            
                            if (Vo->vPos == VPOS_STAGE_PARM || Vo->vPos == VPOS_PARM_VALUE)
                                VoCallback(VOE_PARAMETER_END);
                            
                            Vo->flags &= ~VOF_END_PARM;
                            
                            if (Vo->vPos == VPOS_STAGE_PROP) {
                                if (OS_StrCmp((char*)Vo->stage, "BEGIN") == 0) {
                                    Vo->flags |= VOF_BEGIN;
                                    Vo->vPos = VPOS_STAGE_OBJ_TYPE;
                                    continue;
                                }
                                if (OS_StrCmp((char*)Vo->stage, "END") == 0) {
                                    Vo->flags |= VOF_END;
                                    Vo->vPos = VPOS_STAGE_OBJ_TYPE;
                                    continue;
                                }
                                VoCallback(VOE_PROPERTY_BEGIN);
                            }
                            Vo->vPos = VPOS_PROP_VALUE;
                            continue;
                        }
                        Vo->flags &= ~VOF_ESCAPE;
                        break;

                    case '=':
                        if (Vo->vPos == VPOS_STAGE_PARM) {
                            Vo->stage[Vo->stageLen] = 0;
                            Vo->stageLen = 0;
                        
                            if (Vo->flags & VOF_END_PARM)
                                VoCallback(VOE_PARAMETER_END);

                            CheckParameters(Vo);
                            VoCallback(VOE_PARAMETER_BEGIN);
                            Vo->flags |= VOF_END_PARM;
                            Vo->vPos = VPOS_PARM_VALUE;
                            continue;
                        }
                        break;
                    }
                }

                if (Vo->flags & VOF_ESCAPE) {
                    /* We've encountered an escape that didn't apply to an
                     * escapable character, re-insert it into the stage buffer.
                     * I don't think that this is *required* but it seems nice.
                     */
                    if (Vo->stageLen < MAX_KEYWORD_SIZE)
                        Vo->stage[Vo->stageLen++] = '\\';

                    Vo->flags &= ~VOF_ESCAPE;
                }

                /* Drop in to copy character into stage buffer */

            case VPOS_STAGE_OBJ_TYPE:
                /* Don't copy if *Vo->chr == White space */
                if (Vo->stageLen < MAX_KEYWORD_SIZE && Vo->chr != 0x20 && Vo->chr != 0x09)
                    Vo->stage[Vo->stageLen++] = (char)ToUpper(Vo->chr);
                break;

            case VPOS_PROP_VALUE:
                /* Indicate property value data */
                VoCallback(VOE_PROPERTY_VALUE);
                if (Vo->bufferLen && Vo->vTok != VTOK_EOL) {
                    /* Application failed to parse the entire value. */
                    SkipValue(Vo, FALSE);
                }
                break;

            case VPOS_PARM_VALUE:
                /* Indicate parameter value data */
                CheckParameters(Vo);
                VoCallback(VOE_PARAMETER_VALUE);
                if (Vo->bufferLen && Vo->chr != ':' && Vo->chr != ';') {
                    /* Application failed to parse the entire value. */
                    SkipValue(Vo, FALSE);
                }
                if (Vo->chr == ';') {
                    Vo->vPos = VPOS_STAGE_PARM;
                }
                else if (Vo->chr == ':') {
                    VoCallback(VOE_PARAMETER_END);
                    Vo->vPos = VPOS_PROP_VALUE;
                    Vo->flags &= ~VOF_END_PARM;
                }
                break;
            }
        }

        if (Vo->vTok == VTOK_EOL) {
            Vo->stage[Vo->stageLen] = 0;
            Vo->stageLen = 0;

            /* Avoid problems by clearing flags whose scope is limited to one line. */
            Vo->flags &= ~(VOF_ESCAPE|VOF_QUOTED);

            /* Check if were done processing the vObject. */
            if (Vo->flags & VOF_END) {
                Vo->flags &= ~VOF_END;
                VoCallback(VOE_OBJECT_END);
                        
                if (--Vo->depth == 0) {
                    return Vo->bufferLen;
                }
            }
            /* Indicate new object start */
            else if (Vo->flags & VOF_BEGIN) {
                Vo->depth++;
                Vo->flags &= ~VOF_BEGIN;
                VoCallback(VOE_OBJECT_BEGIN);
            }
            /* Indicate end of property */
            else {
                if (Vo->vPos != VPOS_PROP_VALUE) 
                    VoCallback(VOE_ERROR);

                VoCallback(VOE_PROPERTY_END);
                Vo->encoding = Vo->baseEncoding;
            }
            Vo->vPos = VPOS_STAGE_PROP;
        }  
    }

    /* Were out of data to process. */
    Assert(Vo->bufferLen == 0);

    /* Check if were done processing the this object */
    if (Vo->flags & VOF_END) {
        Vo->flags &= ~VOF_END;
               
        Vo->stage[Vo->stageLen] = 0;
        Vo->stageLen = 0;
        
        VoCallback(VOE_OBJECT_END);
        Vo->depth--;
    }

    return 0;
}


/*----------------------------------------------------------------------------
 *            VO_ReadValue()
 *----------------------------------------------------------------------------
 * Synopsis: Read value data during a VOE_PROPERTY_VALUE or VOE_PARAMETER_VALUE
 *           event.
 */
VoStatus VO_ReadValue(VoContext *Vo, U8 *Buffer, U16 *BufferLen, BOOL ByParts)
{
    U16      offset = 0;
    VoStatus status = VO_OK;

    CheckUnlockedParm(VO_INVALID_PARM, (Vo && Buffer && BufferLen));
    CheckUnlockedParm(VO_FAILED, (Vo->vPos == VPOS_PARM_VALUE || Vo->vPos == VPOS_PROP_VALUE));

    /* First byte of value has already been read into 'ch'. */
    do {
        if (Vo->vTok == VTOK_CHAR) {
            /* VPOS_PROP_VALUE semi's separate parts, comma's ignore, colons ignore, un-fold */
            /* VPOS_PARM_VALUE semi's start next parm, comma's ignore, colons start property value, un-fold */

            if ((Vo->flags & VOF_QUOTED) == 0) {
                switch (Vo->chr) {
                case ':':
                    if (Vo->vPos == VPOS_PARM_VALUE && ((Vo->flags & VOF_ESCAPE) == 0)) {
                        /* Start of Property Value */
                        goto done;
                    }
                    Vo->flags &= ~VOF_ESCAPE;
                    break;
                
                case ';':
                    if (Vo->vPos == VPOS_PARM_VALUE && ((Vo->flags & VOF_ESCAPE) == 0)) {
                        /* Start of next parameter */
                        goto done;
                    }
                    if (ByParts && Vo->vPos == VPOS_PROP_VALUE && ((Vo->flags & VOF_ESCAPE) == 0)) {
                        /* Start of next property part, exiting now leaves the
                         * semi-colon in the 'ch' so set the vTok to TOK_
                         * CONSUME so we eat the semi-colon when we come back.
                         */
                        Vo->vTok = VTOK_CONSUME;
                        status = VO_PART_OK;
                        goto done;
                    }
                    Vo->flags &= ~VOF_ESCAPE;
                    break;                
                }
            }

            if (Vo->flags & VOF_ESCAPE) {
                if (offset < *BufferLen) {
                    *Buffer++ = '\\';
                    offset++;
                    Vo->flags &= ~VOF_ESCAPE;
                }
            }

            if (offset < *BufferLen) {
                *Buffer++ = Vo->chr;
                offset++;
            }
            else if (offset == *BufferLen) {
                /* Have character in 'ch' to write but no more space. */
                status = VO_MORE;
                goto done;
            }
        }

        if (Vo->vTok == VTOK_EOL) {
            goto done;
        }

    } while (GetToken(Vo));

    *BufferLen = offset;
    return VO_EOF;

done:
    /* Null terminate result but don't add terminator to byte count. */
    if (offset < *BufferLen)
        *Buffer = 0;

    *BufferLen = offset;
    return status;
}


/*----------------------------------------------------------------------------
 *            VO_Abort()
 *----------------------------------------------------------------------------
 * Synopsis: Cancel processing of current buffer.
 *
 */
void VO_Abort(VoContext *Vo)
{
    Vo->bufferLen = 0;
}


/*----------------------------------------------------------------------------
 *            VO_BeginObject()
 *----------------------------------------------------------------------------
 * Synopsis: Begin encoding a new vObject type.
 *
 */
VoStatus VO_BeginObject(VoContext *Vo, const char *ObjectType)
{
    U16     len;

    CheckUnlockedParm(VO_FAILED, Vo && (Vo->e.state == VPOS_STAGE_PROP));

    len = OS_StrLen(ObjectType);

    if ((Vo->e.outBuffLen - Vo->e.offset) >= len + 8) {
        OS_MemCopy(Vo->e.outBuff + Vo->e.offset, "BEGIN:", 6);
        Vo->e.offset += 6;
        OS_MemCopy(Vo->e.outBuff + Vo->e.offset, ObjectType, len);
        Vo->e.offset = (U16)(Vo->e.offset + len);

        Vo->e.outBuff[Vo->e.offset++] = '\r';
        Vo->e.outBuff[Vo->e.offset++] = '\n';

        Assert(Vo->e.outBuffLen >= Vo->e.offset);
        return VO_OK;
    }
    return VO_EOF;
}


/*----------------------------------------------------------------------------
 *            VO_AddProperty()
 *----------------------------------------------------------------------------
 * Synopsis: Add a property type to the current vObject.
 *
 */
VoStatus VO_AddProperty(VoContext *Vo, const char *Property)
{
    U16     len;

    CheckUnlockedParm(VO_FAILED, Vo && (Vo->e.state == VPOS_STAGE_PROP));

    len = OS_StrLen(Property);

    if ((Vo->e.outBuffLen - Vo->e.offset) >= len) {
        Vo->e.lineStart = Vo->e.offset;

        OS_MemCopy(Vo->e.outBuff + Vo->e.offset, Property, len);
        Vo->e.offset = (U16)(Vo->e.offset + len);

        Vo->e.state = VPOS_STAGE_PARM;

        return VO_OK;
    }
    return VO_EOF;
}


/*----------------------------------------------------------------------------
 *            VO_AddParameter()
 *----------------------------------------------------------------------------
 * Synopsis: Add a parameter to the current vObject proerty.
 *
 */
VoStatus VO_AddParameter(VoContext *Vo, const char *Parm, const char *Value)
{
    U16     len1, len2, foldLen;

    CheckUnlockedParm(VO_FAILED, Vo && (Vo->e.state == VPOS_STAGE_PARM));

    len1 = OS_StrLen(Parm);
    len2 = OS_StrLen((Value ? Value : ""));

    /* Calc number of bytes added by folding. */
    foldLen = (U16) ( (((Vo->e.offset - Vo->e.lineStart) + 2 + len1 + len2) / 70) * 3 );

    if ((Vo->e.outBuffLen - Vo->e.offset) >= (len1 + len2 + foldLen + 2)) {
        Vo->e.outBuff[Vo->e.offset++] = ';';

        if ((VO_StrniCmp(Parm, "QUOTED-PRINTABLE", 16) == 0) ||
            (Value && VO_StrniCmp(Value, "QUOTED-PRINTABLE", 16) == 0)) {
            /* Set QP encoding format in case we need to fold property data */
            Vo->e.encoding = VOENC_QP;
        }

        vWrite(Vo, Parm, len1, VOENC_7BIT);

        if (len2) {
            Vo->e.outBuff[Vo->e.offset++] = '=';
            vWrite(Vo, Value, len2, VOENC_7BIT);
        }
        Assert(Vo->e.outBuffLen >= Vo->e.offset);
        return VO_OK;
    }

    return VO_EOF;
}


/*----------------------------------------------------------------------------
 *            VO_AddPropertyValue()
 *----------------------------------------------------------------------------
 * Synopsis:  Add the property value to the current vObject property.
 *
 */
VoStatus VO_AddPropertyValue(VoContext *Vo, const char *Value, U16 Length)
{
    U16     foldLen;

    CheckUnlockedParm(VO_FAILED, Vo && (Vo->e.state == VPOS_STAGE_PARM));

    /* Calc number of bytes added by folding. */
    foldLen = (U16) ( (((Vo->e.offset - Vo->e.lineStart) + 3 + Length) / 70) * 3 );

    if ((Vo->e.outBuffLen - Vo->e.offset) >= Length + foldLen + 3) {
        Vo->e.outBuff[Vo->e.offset++] = ':';

        vWrite(Vo, Value, Length, Vo->e.encoding);

        Vo->e.outBuff[Vo->e.offset++] = '\r';
        Vo->e.outBuff[Vo->e.offset++] = '\n';

        Vo->e.state = VPOS_STAGE_PROP;
        Vo->e.encoding = VOENC_7BIT;

        return VO_OK;
    }
    return VO_EOF;
}


/*----------------------------------------------------------------------------
 *            VO_GetPropertyValueAvailLen()
 *----------------------------------------------------------------------------
 * Synopsis:  Determines the amount of available space in the encode buffer.
 *
 */
U16 VO_GetPropertyValueAvailLen(VoContext *Vo)
{
    U16     foldLen, length;

    CheckUnlockedParm(VO_FAILED, Vo && (Vo->e.state == VPOS_STAGE_PARM));

    /* Calc number of bytes added by folding. */
    length = (U16) (Vo->e.outBuffLen - Vo->e.offset);
    foldLen = (U16) ( (((Vo->e.offset - Vo->e.lineStart) + 3 + length) / 70) * 3 );

    return (U16) (length - foldLen - 3);    /* 3 = ':\r\n' */
}


/*----------------------------------------------------------------------------
 *            VO_EndObject()
 *----------------------------------------------------------------------------
 * Synopsis: Close the current vObject.
 *
 */
VoStatus VO_EndObject(VoContext *Vo, const char *ObjectType)
{
    U16     len;

    CheckUnlockedParm(VO_FAILED, Vo && (Vo->e.state == VPOS_STAGE_PROP));

    len = OS_StrLen(ObjectType);

    if ((Vo->e.outBuffLen - Vo->e.offset) >= len + 6) {
        OS_MemCopy(Vo->e.outBuff + Vo->e.offset, "END:", 4);
        Vo->e.offset += 4;
        OS_MemCopy(Vo->e.outBuff + Vo->e.offset, ObjectType, len);
        Vo->e.offset = (U16)(Vo->e.offset + len);

        Vo->e.outBuff[Vo->e.offset++] = '\r';
        Vo->e.outBuff[Vo->e.offset++] = '\n';

        Assert(Vo->e.outBuffLen >= Vo->e.offset);
        return VO_OK;
    }
    return VO_EOF;
}


/*----------------------------------------------------------------------------
 *            VO_QueueEncodeBuffer()
 *----------------------------------------------------------------------------
 * Synopsis: Submit a buffer to encode the vObject in.
 *
 */
VoStatus VO_QueueEncodeBuffer(VoContext *Vo, U8 *Buffer, U16 BufferLen)
{
    CheckUnlockedParm(VO_INVALID_PARM, (Vo && Buffer));
    CheckUnlockedParm(VO_FAILED, (Vo->e.outBuff == 0));

    Vo->e.outBuff = Buffer;
    Vo->e.outBuffLen = BufferLen;
    Vo->e.offset = 0;

    return VO_OK;
}


/*----------------------------------------------------------------------------
 *            VO_GetEncodedBuffer()
 *----------------------------------------------------------------------------
 * Synopsis: Retrieve the encoded vObject buffer.
 *
 */
VoStatus VO_GetEncodedBuffer(VoContext *Vo, U8 **Buffer, U16 *BufferLen)
{
    CheckUnlockedParm(VO_INVALID_PARM, (Vo && Buffer && BufferLen));
    CheckUnlockedParm(VO_FAILED, (Vo->e.outBuff));

    *Buffer = Vo->e.outBuff;
    *BufferLen = Vo->e.offset;

    Vo->e.outBuff = 0;
    Vo->e.outBuffLen = 0;

    return VO_OK;
}


/*----------------------------------------------------------------------------
 *            VO_StrniCmp()
 *----------------------------------------------------------------------------
 * Synopsis: Compares two strings, up to "Count" bytes in a case indifferent
 *           manner.
 *
 */
U8 VO_StrniCmp(const char *Str1, const char *Str2, U32 Count)
{
    while (Count && (ToUpper(*Str1) == ToUpper(*Str2))) {
        if (*Str1 == 0 || *Str2 == 0) {
            break;
        }
        Str1++;
        Str2++;
        Count--;
    }

    /* Return zero on success, just like the ANSI strcmp() */
    if (Count == 0 || ToUpper(*Str1) == ToUpper(*Str2))
        return 0;

    return 1;
}

/****************************************************************************
 *                         VoContext Internal Functions
 ****************************************************************************/
/*---------------------------------------------------------------------------
 * Returns TRUE if character token is valid. FALSE if end-of-input was 
 * reached.
 */
static BOOL GetToken(VoContext *Vo)
{
    if (Vo->vTok == VTOK_EOL) {
        /* Already have current 'ch' */
        Vo->vTok = VTOK_CHAR;
        return TRUE;
    }

    while (Vo->bufferLen) {
        Vo->chr = *Vo->buffer;

        /* Consume character */
        Vo->bufferLen--;
        Vo->buffer++;

        switch (Vo->vTok) {
        case VTOK_CR_SKIPPED:
            /* This logic consumes \r's at the end of a the parameter value.
             * For example: "N:Doe;John\r\r\r\n" will have the \r\r disarded,
             * is this appropriate?
             */
            if (Vo->chr != '\r')
                Vo->vTok = VTOK_CRLF_SKIPPED;
            
            if (Vo->chr == '\r' || Vo->chr == '\n')
                break;

            /* Drop down to process non-CR/LF character */

        case VTOK_CRLF_SKIPPED:
            /* Make sure it's not a folded line before resetting state. */
            if (Vo->chr != 0x20 && Vo->chr != 0x09) {
                /* Check for blank lines (ie, /r/n/r/n). I don't think this is
                 * legal but I've seen vCards with it. If we find a '/r' then we
                 * go back to VTOK_CR_SKIPPED state.
                 */
                if (Vo->chr == '\r') {
                    Vo->vTok = VTOK_CR_SKIPPED;
                    break;
                }
                /* Beginning of a new property */
                Vo->vTok = VTOK_EOL;
                return TRUE;
            }

            /* Consume the space to complete the un-folding */
            Vo->vTok = VTOK_CHAR;
            break;

        case VTOK_EQUALS_SKIPPED:
            if (Vo->chr == '\r') {
                Vo->vTok = VTOK_CR_SKIPPED_QP;
                break;
            }

            /* Not a Quoted-Printable soft line break. Roll back. */
            Vo->chr = '=';
            Vo->buffer--;
            Vo->bufferLen++;
            Vo->vTok = VTOK_CHAR;
            return TRUE;

        case VTOK_CR_SKIPPED_QP:
            if (Vo->chr != '\r')
                Vo->vTok = VTOK_CRLF_SKIPPED_QP;

            if (Vo->chr == '\r' || Vo->chr == '\n')
                break;

            /* Drop down to process non-CR/LF character */

        case VTOK_CRLF_SKIPPED_QP:
            /* Quoted-Printable soft-line break has been consumed. */
            if (Vo->chr == '\r') {
                Vo->vTok = VTOK_CR_SKIPPED_QP;
                break;
            }
            Vo->vTok = VTOK_CHAR;
            return TRUE;

        case VTOK_CONSUME:
            Vo->vTok = VTOK_CHAR;
            /* Drop into normal VTOK_CHAR processing */

        case VTOK_CHAR:
            if (Vo->chr == '\r') {
                Vo->vTok = VTOK_CR_SKIPPED;
                break;
            }
            
            if (Vo->chr == '\\') {
                if ((Vo->flags & VOF_ESCAPE) == 0) {
                    Vo->flags |= VOF_ESCAPE;
                    break;
                }
                Vo->flags &= ~VOF_ESCAPE;
            }

            if (Vo->chr == '"') {
                /* We don't consume quotes, just notice them. It appears
                 * that they may only apply to parameter values.
                 */
                if (Vo->flags & VOF_QUOTED)
                    Vo->flags &= ~VOF_QUOTED;
                else Vo->flags |= VOF_QUOTED;
            }

            if (Vo->chr == '=' && Vo->encoding == VOENC_QP) {
                Vo->vTok = VTOK_EQUALS_SKIPPED;
                break;
            }
            return TRUE;
        }
    }
    return FALSE;
}

/*---------------------------------------------------------------------------
 * Writes to vObject buffer. Includes line folding if necessary.
 */
static void vWrite(VoContext *Vo, const char *Value, U16 Length, VoEncoding Encoding)
{
    U16 len;

    while (Length) {
        len = (U16) min(70 - (Vo->e.offset - Vo->e.lineStart), Length);

        OS_MemCopy(Vo->e.outBuff + Vo->e.offset, Value, len);
        Value += len;
        Length = (U16) (Length - len);
        
        Vo->e.offset = (U16) (Vo->e.offset + len);

        if (Length) {
            /* Insert fold */
            if (Encoding == VOENC_QP)
                Vo->e.outBuff[Vo->e.offset++] = '=';

            Vo->e.outBuff[Vo->e.offset++] = '\r';
            Vo->e.outBuff[Vo->e.offset++] = '\n';

            Vo->e.lineStart = Vo->e.offset;

            if (Encoding != VOENC_QP)
                Vo->e.outBuff[Vo->e.offset++] = ' ';
        }
    }
}

/*----------------------------------------------------------------------------
 * Skip value data during a VOE_PROPERTY_VALUE or VOE_PARAMETER_VALUE event.
 */
static VoStatus SkipValue(VoContext *Vo, BOOL ByParts)
{
    U16         len = 25;
    U8          temp[25];
    VoStatus    status;

    while ((status = VO_ReadValue(Vo, temp, &len, ByParts)) == VO_MORE) 
        len = 25;

    return status;
}

/*---------------------------------------------------------------------------
 * Checks for encoding in property parameters.
 */
static void CheckParameters(VoContext *Vo)
{
    if (Vo->vPos == VPOS_STAGE_PARM) {
        if (OS_StrCmp((char*)Vo->stage, "QUOTED-PRINTABLE") == 0)
            Vo->encoding = VOENC_QP;

        return;
    }

    Assert(Vo->vPos == VPOS_PARM_VALUE);

    /* In this case the first byte has already been pulled. */
    if (Vo->chr == 'Q' && VO_StrniCmp((char*)Vo->buffer, "UOTED-PRINTABLE", 15) == 0)
        Vo->encoding = VOENC_QP;
}



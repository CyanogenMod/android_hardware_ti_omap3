/***************************************************************************
 *
 * File:
 *     iAnywhere Data Sync SDK
 *     $Id:vobject.h,v 1.23, 2006-01-13 04:41:17Z, Kevin Hendrix$
 *
 * Description:
 *     Definition of vObject Parser/Builder functions and status values.
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

#ifndef __VOBJECT_H
#define __VOBJECT_H

#include "xatypes.h"

/*----------------------------------------------------------------------------
 * vObject Parser and Builder Layer
 *
 *     The vObject library provides a set of functions for parsing and 
 *     building a variety of vObject types. Specific support for vCard 2.1,
 *     vCard 3.0, vCalendar 1.0 and iCalendar 2.0 has been tested. However
 *     other vObject based objects should be compatible. For copies and
 *     information on the IETF and Versit specifications refer to
 *     "http://www.imc.org/pdi" and "http://www.imc.org/pdi/pdiproddev.html".
 */

/****************************************************************************
 *
 * Constants
 *
 ****************************************************************************/

/*----------------------------------------------------------------------------
 * MAX_KEYWORD_SIZE constant
 *
 *     This constant determines the maximum size keyword that the vObject
 *     parser can process. It should be as large as the largest property or
 *     parameter string that can be received. The default (minimum) value is
 *     enough space to hold the encoding type "QUOTED-PRINTABLE". If a 
 *     received keyword exceeds this limit it will be truncated.
 */
#ifndef MAX_KEYWORD_SIZE
#define MAX_KEYWORD_SIZE 17
#endif

#if MAX_KEYWORD_SIZE < 17
#error "Maximum keyword size must be at least 17 bytes."
#endif


/****************************************************************************
 *
 * Types
 *
 ****************************************************************************/

/*----------------------------------------------------------------------------
 * VoStatus type
 *
 *     Status of the vObject function call.
 */
typedef I8 VoStatus;

/* The command is successful and complete. */
#define VO_OK               0

/* The command operation failed. */
#define VO_FAILED           1

/* More data is available from the input buffer. Additional read calls
 * can be made to retrieve the remaining data.
 */
#define VO_MORE             2

/* More data "parts" are available from the input buffer. This status is
 * returned when a read-by-parts is requested and this part has been 
 * completely but there are more parts in the property.
 */
#define VO_PART_OK          3

/* The input or output end was reached unexpectedly. */
#define VO_EOF              4

/* A function parameter is invalid. */
#define VO_INVALID_PARM     5

/* End of VoStatus */

/*----------------------------------------------------------------------------
 * VoEvent type
 *
 *     The vObject parser communicates events of this type to the vObject
 *     callback. Most events are only indicated while parsing a vObject.
 */
typedef U8 VoEvent;

/* A new property has been found. Its type is provided in the callback
 * parameters "u.property" field.
 */
#define VOE_PROPERTY_BEGIN      1

/* The end of the current property has been reached. */
#define VOE_PROPERTY_END        2

/* The current properties' value is available. If the application is interested
 * in the value it should call VO_ReadPropertyValue to retrieve the value.
 */
#define VOE_PROPERTY_VALUE      3

/* A new parameter has been found. Its type is provided in the callback
 * parameters "u.parameter" field.
 */
#define VOE_PARAMETER_BEGIN     4

/* The end of the current parameter has been reached. */
#define VOE_PARAMETER_END       5

/* The current parameters' value is available. If the application is interested
 * in the value it should call VO_ReadParameterValue to retrieve the value.
 * Notice that some vObject parameters do not contain values.
 */
#define VOE_PARAMETER_VALUE     6

/* A new object type has been found. Its type is provided in the callback
 * parameters "u.object" field.
 */
#define VOE_OBJECT_BEGIN        7

/* The end of the current object has been found. Its type is provided in
 * the callback parameters "u.object" field.
 */
#define VOE_OBJECT_END          8

/* A processing error has occurred. This is typically due to a malformed
 * vObject. The application has the option to allow processing to continue, 
 * or to abort by calling VO_Abort. If processing is allowed to continue
 * the application should be aware that the results indicated by the object
 * parser may not reflect the senders intended values.
 */
#define VOE_ERROR               9

/* End of VoEvent */


/*----------------------------------------------------------------------------
 * VoEncoding type
 *
 *     This type is used to express the encoding format applied to a vObject,
 *     or to a property within the vObject.
 */
typedef U8 VoEncoding;

/* ASCII 7bit encoding. This is the default encoding for vCards and vCalendar
 * v1.0 content types.
 */
#define VOENC_7BIT       0

/* Base64 content encoding. All binary data must be base64 encoded. Refer to 
 * RFC-2045 for information on this encoding format.
 */
#define VOENC_BINARY     1

/* UTF-8 transformation format of ISO-10646 encoding. This is the default
 * encoding for iCalendar v2.0. Refer to RFC-2249 for information on this
 * encoding format.
 */
#define VOENC_UTF8       2

/* Quoted-Printable encoding format. This format can only be used with
 * vCard 2.1 and vCalendar 1.0 content formats. Refer to RFC-2045 for
 * information on this encoding format.
 */
#define VOENC_QP         3

/* End of VoEncoding */

/* Forward typedefs */
typedef struct _VoContext VoContext;
typedef struct _VoCallbackParms VoCallbackParms;

/*----------------------------------------------------------------------------
 * VoCallback type
 *
 *     A function of this type is provided to the vObject library to receive
 *     event notifications. All event notifications include the event type
 *     and a pointer to the VoContext context provided during initialization.
 *     The application may wish to attach additional private information to
 *     the VoContext context by encapsulating the VoContext structure within a
 *     larger private context structure.
 */
typedef void (*VoCallback)(VoCallbackParms *Parms);

/* End of VoCallback */

/****************************************************************************
 *
 * Data Structures
 *
 ****************************************************************************/

/*----------------------------------------------------------------------------
 * VoCallbackParms structure
 *
 *     This structure is provided during callback events to provide information
 *     about the event.
 */
struct _VoCallbackParms {
    VoEvent          event;
    VoContext       *context;

    union {

        /* The current property type. Only valid during VOE_PROPERTY_BEGIN event
         * indications. This string is always in upper case and null terminated.
         */
        const char  *property;

        /* The current parameter type. Only valid during VOE_PARAMETER_BEGIN event
         * indications. This string is always in upper case and null terminated.
         */
        const char  *parameter;

        /* The current object type. Only valid during VOE_OBJECT_BEGIN and
         * VOE_OBJECT_END event indications. This string is always in upper
         * case and null terminated.
         */
        const char  *object;

    } u;

};


/*----------------------------------------------------------------------------
 * VoContext structure
 *
 *     This structure is provided by the application to the vObject library
 *     during initialization and to all subsequent vObject API calls. It
 *     contains all the private state information that the vObject library 
 *     needs during object encoding and decoding.
 */
struct _VoContext {

    /* === Internal use only === */
    /* Decoding Variables */
    U8               stage[MAX_KEYWORD_SIZE]; /* Prop/Parm staging buffer */
    U8               stageLen;      /* Bytes in stage buffer */
    U8               vPos;          /* Parser positional state */
    U8               vTok;          /* Parser token state */
    U8               depth;         /* vObject depth */
    U8               chr;           /* Current character */
    U8               baseEncoding;  /* vObject baseline encoding */
    U8               encoding;      /* Current encoding */
    U8               flags;         /* Token/position state flags */
    VoCallback       callback;      /* Application callback function */
    const U8        *buffer;        /* Buffer to parse */
    U16              bufferLen;     /* Remaining buffer length */

    /* Encoding Variables */
    struct {
        U8           state;         /* Encoder positional state */
        U8           encoding;      /* Current encoding */
        U16          offset;        /* Write offset in encoding buffer */
        U16          lineStart;     /* Current line start position, for folding */
        U8          *outBuff;       /* Encoding buffer */
        U16          outBuffLen;    /* Encoding buffer length */
    } e;
};


/****************************************************************************
 *
 * Function Reference
 *
 ****************************************************************************/
/*---------------------------------------------------------------------------
 * VO_Init()
 *
 *     Initialize the VoContext context. Once initialized, the context can be
 *     used with any vObject API function. The context does not need to be
 *     deinitialized.
 *
 * Parameters:
 *     Vo - Parser context to initialize.
 *
 *     Callback - Application callback function that receives notifications
 *         from the vObject parser.
 *
 * Returns:
 *     VO_OK - The context was initialized.
 *     
 *     VO_INVALID_PARM - An invalid function parameter was specified
 *         (XA_ERROR_CHECK only).
 */
VoStatus VO_Init(VoContext *Vo, VoCallback Callback);


/****************************************************************************
 *
 * Section: vObject Parsing Functions
 *
 ****************************************************************************
 *
 *     The following group of functions is used when parsing a vObject.
 *
 ****************************************************************************/

/*---------------------------------------------------------------------------
 * VO_Parse()
 *
 *     This function is called to process vObject data. It will indicate
 *     events to the VoCallback as the structure of the object is decoded.
 *     The parser will continue to parse the input buffer until the end is
 *     reached, or it parses a complete root level object, or abort is called.
 *     
 *     Here is an example of how this function indicates the properties and
 *     parameters found in the following vCard version 3.0 line:
 *
 *     EMAIL;TYPE=INTERNET,PREF:Frank_Dawson@Lotus.com
 *
 *     Property        = EMAIL
 *     Parameter       = TYPE
 *     Parameter Value = INTERNET,PREF
 *     Property Value  = Frank_Dawson@Lotus.com
 *
 * Parameters:
 *     Vo - An initialized VoContext context.
 *
 *     Buffer - The data buffer containing the vObject.
 *     
 *     BufferLen - The length of the data buffer, in bytes.
 *
 * Returns:
 *     The number of bytes remaining (unparsed) in the Buffer.
 */
U16 VO_Parse(VoContext *Vo, const U8 *Buffer, U16 BufferLen);


/*---------------------------------------------------------------------------
 * VO_ReadPropertyValue()
 *
 *     Read a property value into the specified buffer. This function is only
 *     valid during VOE_PROPERTY_VALUE events.
 *
 * Parameters:
 *     Vo - The VoContext context provided in the event callback.
 *     
 *     Buffer - A buffer to receive the property value data.
 *
 *     BufferLen - A pointer to the length, in bytes of "Buffer" on input.
 *         This value is updated upon successful completion to reflect the
 *         number of bytes copied into "Buffer".
 *     
 *     ByParts - Some vObjects contain structured properties, using this
 *         option; each part of a structured property can be copied 
 *         individually. When TRUE, the function will return after copying
 *         the next value part. When FALSE, the entire value data is copied.
 *         A good example is the vCard NAME property. This property contains
 *         structured values. In the example "N:Public;John;Quinlan;Mr.;Esq."
 *         there are 5 "parts".
 *
 * Returns:
 *     VO_OK - The property value has been completely copied.
 *     
 *     VO_PART_OK - The current part of the property value has been copied.
 *         More property parts exist. This is only returned when "ByParts"
 *         is TRUE. When the last property part has been read, VO_OK is 
 *         returned.
 *     
 *     VO_MORE - The property value (or value part) was copied into the 
 *         available buffer space. More value data exists to be read.
 *     
 *     VO_FAILED - The parser is not in the correct state (XA_ERROR_CHECK
 *         only).
 *
 *     VO_INVALID_PARM - In invalid function parameter was provided
 *         (XA_ERROR_CHECK only).
 */
VoStatus VO_ReadPropertyValue(VoContext *Vo, U8 *Buffer, U16 *BufferLen, 
                                   BOOL ByParts);


/*---------------------------------------------------------------------------
 * VO_ReadParameterValue()
 *
 *     Read a parameter value into the specified buffer. This function is only
 *     valid during VOE_PARAMETER_VALUE events.
 *
 * Parameters:
 *     Vo - The VoContext context provided in the event callback.
 *     
 *     Buffer - A buffer to receive the parameter value data.
 *
 *     BufferLen - A pointer to the length, in bytes of "Buffer" on input.
 *         This value is updated upon successful completion to reflect the
 *         number of bytes copied into "Buffer".
 *     
 * Returns:
 *     VO_OK - The parameter value has been completely copied.
 *     
 *     VO_MORE - The parameter value was copied into the available buffer
 *         space. More value data exists to be read.
 *     
 *     VO_FAILED - The parser is not in the correct state (XA_ERROR_CHECK
 *         only).
 *
 *     VO_INVALID_PARM - In invalid function parameter was provided
 *         (XA_ERROR_CHECK only).
 */
VoStatus VO_ReadParameterValue(VoContext *Vo, U8 *Buffer, U16 *BufferLen);


/*---------------------------------------------------------------------------
 * VO_Abort()
 *
 *     This function is called to cancel the vObject processing. It only has
 *     an effect when called during a VoCallback indication. In which case it
 *     will force the return from VO_Parse as soon as possible. Note that
 *     it is possible to receive further events after calling abort.
 *
 * Parameters:
 *     Vo - The VoContext context provided in the event callback.
 */
void VO_Abort(VoContext *Vo);


/****************************************************************************
 *
 * Section: vObject Building Functions
 *
 ****************************************************************************
 *
 *     The following group of functions is used when building a vObject.
 *
 ****************************************************************************/

/*---------------------------------------------------------------------------
 * VO_BeginObject()
 *
 *     This function is called to start encoding a new object into the vObject
 *     encode buffer.
 *
 * Parameters:
 *     Vo - An initialized VoContext context.
 *     
 *     ObjectType - A pointer to the null-terminated string name of the object.
 *
 * Returns:
 *     VO_OK - The data has been written to the encode buffer.
 *
 *     VO_FAILED - The encoder is not in the correct state.
 *
 *     VO_EOF - There was not sufficient space in the encode buffer to
 *         write the requested data. No data has been written.
 */
VoStatus VO_BeginObject(VoContext *Vo, const char *ObjectType);


/*---------------------------------------------------------------------------
 * VO_AddProperty()
 *
 *     This function is called to start encoding a new property into the
 *     vObject encode buffer.
 *
 * Parameters:
 *     Vo - An initialized VoContext context.
 *     
 *     Property - A pointer to the null-terminated string name of the property.
 *
 * Returns:
 *     VO_OK - The data has been written to the encode buffer.
 *
 *     VO_FAILED - The encoder is not in the correct state.
 *
 *     VO_EOF - There was not sufficient space in the encode buffer to
 *         write the requested data. No data has been written.
 */
VoStatus VO_AddProperty(VoContext *Vo, const char *Property);


/*---------------------------------------------------------------------------
 * VO_AddParameter()
 *
 *     This function is called to start encoding a new parameter into the
 *     vObject encode buffer. The entire parameter type and value must be
 *     encoded in one function call.
 *
 * Parameters:
 *     Vo - An initialized VoContext context.
 *     
 *     Parm - A pointer to the null-terminated string name of the parameter.
 *     
 *     Value - A pointer to the null-terminated string parameter value.
 *
 * Returns:
 *     VO_OK - The data has been written to the encode buffer.
 *
 *     VO_FAILED - The encoder is not in the correct state.
 *
 *     VO_EOF - There was not sufficient space in the encode buffer to
 *         write the requested data. No data has been written.
 */
VoStatus VO_AddParameter(VoContext *Vo, const char *Parm, const char *Value);


/*---------------------------------------------------------------------------
 * VO_AddPropertyValue()
 *
 *     This function is called to encode a properties' value into the vObject
 *     encode buffer. The entire property value must be encoded in one function
 *     call.
 *
 * Parameters:
 *     Vo - An initialized VoContext context.
 *     
 *     Value - A pointer to the property value data.
 *
 *     Length - The length of the property value data, in bytes.
 *
 * Returns:
 *     VO_OK - The data has been written to the encode buffer.
 *
 *     VO_FAILED - The encoder is not in the correct state.
 *
 *     VO_EOF - There was not sufficient space in the encode buffer to
 *         write the requested data. No data has been written.
 */
VoStatus VO_AddPropertyValue(VoContext *Vo, const char *Value, U16 Length);


/*---------------------------------------------------------------------------
 * VO_EndObject()
 *
 *     This function is called to end the encoding of the current vObject.
 *
 * Parameters:
 *     Vo - An initialized VoContext context.
 *     
 *     ObjectType - A pointer to the null-terminated string name of the 
 *         object. This value must match the value provided in the last
 *         VO_BeginObject call.
 *
 * Returns:
 *     VO_OK - The data has been written to the encode buffer.
 *
 *     VO_FAILED - The encoder is not in the correct state.
 *
 *     VO_EOF - There was not sufficient space in the encode buffer to
 *         write the requested data. No data has been written.
 */
VoStatus VO_EndObject(VoContext *Vo, const char *ObjectType);


/*---------------------------------------------------------------------------
 * VO_QueueEncodeBuffer()
 *
 *     Queue an empty buffer in the vObject encoder. As vObjects are encoded
 *     they are written to this buffer. Only one buffer can be queued at a
 *     time.
 *
 * Parameters:
 *     Vo - An initialized VoContext context.
 *     
 *     Buffer - An empty buffer that receives encoded vObjects.
 *
 *     BufferLen - The length of the encode buffer, in bytes.
 *
 * Returns:
 *     VO_OK - The buffer was accepted for encoding.
 *
 *     VO_INVALID_PARM - An invalid function parameter was provided.
 *
 *     VO_FAILED - The encoder could not accept the buffer.
 */
VoStatus VO_QueueEncodeBuffer(VoContext *Vo, U8 *Buffer, U16 BufferLen);


/*---------------------------------------------------------------------------
 * VO_GetEncodedBuffer()
 *
 *     Retrieve the queued buffer from the encoder.
 *
 * Parameters:
 *     Vo - An initialized VoContext context.
 *     
 *     Buffer - A pointer to an encoding buffer pointer that receives the 
 *         returned encode buffer.
 *     
 *     BufferLen - A pointer to an integer that receives the number of bytes
 *         encoded into the returned buffer.
 *
 * Returns:
 *     VO_OK - The encode buffer was returned.
 *
 *     VO_INVALID_PARM - An invalid function parameter was provided.
 *
 *     VO_FAILED - There is no encoder buffer.
 */
VoStatus VO_GetEncodedBuffer(VoContext *Vo, U8 **Buffer, U16 *BufferLen);

/*---------------------------------------------------------------------------
 * VO_GetPropertyValueAvailLen()
 *
 *     Determines the amount of available space in the encode buffer for 
 *     property value data. This value accounts for line folding characters.
 *
 * Parameters:
 *     Vo - An initialized VoContext context.
 *     
 *
 * Returns:
 *     U16 - The available length. Note that this should be reduced by
 *         the length needed to encode the END:OBJECT if appropriate.
 */
U16 VO_GetPropertyValueAvailLen(VoContext *Vo);

/*---------------------------------------------------------------------------
 * VO_StrniCmp()
 *
 *     This function compares, at most, the first Count characters of Str1 and
 *     Str2. The comparison is performed without regard to case and ends if a
 *     null terminator is reached in either string before Count characters are
 *     compared. This function is most useful for comparing parameter values.
 *
 * Parameters:
 *     Str1 - Null-terminated string to compare.
 *     
 *     Str2 - Null-terminated string to compare.
 *     
 *     Count - Number of characters to compare.
 *
 * Returns:
 *     0 - The substrings match.
 *
 *     !0 - The substrings do not match.
 */
U8 VO_StrniCmp(const char *Str1, const char *Str2, U32 Count);


/****************************************************************************
 *
 * Types and macros used internally by the API
 *
 ****************************************************************************/

#define VO_ReadPropertyValue(_VO, _B, _BLEN, _BYPARTS) \
            VO_ReadValue((_VO), (_B), (_BLEN), (_BYPARTS))

#define VO_ReadParameterValue(_VO, _B, _BLEN) \
            VO_ReadValue((_VO), (_B), (_BLEN), FALSE)

VoStatus VO_ReadValue(VoContext *Vo, U8 *Buffer, U16 *BufferLen, BOOL ByParts);

#endif /* __VOBJECT_H */

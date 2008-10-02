/* =========================================================================
 * This file is part of NITRO
 * =========================================================================
 * 
 * (C) Copyright 2004 - 2008, General Dynamics - Advanced Information Systems
 *
 * NITRO is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public 
 * License along with this program; if not, If not, 
 * see <http://www.gnu.org/licenses/>.
 *
 */

#include "nitf/TREUtils.h"
#include "nitf/TREPrivateData.h"


NITFAPI(int) nitf_TREUtils_parse(nitf_TRE * tre,
        char *bufptr, nitf_Error * error)
{
    int status = 1;
    int iterStatus = NITF_SUCCESS;
    int offset = 0;
    int length;
    nitf_TRECursor cursor;
    nitf_Field *field = NULL;
    nitf_TREPrivateData *privData = NULL;

    /* get out if TRE is null */
    if (!tre)
    {
        nitf_Error_init(error, "parse -> invalid tre object",
                NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }
    
    privData = (nitf_TREPrivateData*)tre->priv;

    /* flush the hash first, to protect from duplicate entries */
    if (privData)
        nitf_TREPrivateData_flush(privData, error);
    
    cursor = nitf_TRECursor_begin(tre);
    while (offset < privData->length && status)
    {
        if ((iterStatus = nitf_TRECursor_iterate(&cursor, error)) == NITF_SUCCESS)
        {
            length = cursor.length;
            if (length == NITF_TRE_GOBBLE)
            {
                length = privData->length - offset;
            }

            /* no need to call setValue, because we already know
             * it is OK for this one to be in the hash
             */
            /* construct the field */
            field = nitf_Field_construct(length,
                    cursor.desc_ptr->data_type, error);
            if (!field)
                goto CATCH_ERROR;

            /* first, check to see if we need to swap bytes */
            if (field->type == NITF_BINARY
                    && (length == NITF_INT16_SZ || length == NITF_INT32_SZ))
            {
                if (length == NITF_INT16_SZ)
                {
                    nitf_Int16 int16 =
                        (nitf_Int16)NITF_NTOHS(*((nitf_Int16 *) (bufptr + offset)));
                    status = nitf_Field_setRawData(field,
                            (NITF_DATA *) & int16, length, error);
                }
                else if (length == NITF_INT32_SZ)
                {
                    nitf_Int32 int32 =
                        (nitf_Int32)NITF_NTOHL(*((nitf_Int32 *) (bufptr + offset)));
                    status = nitf_Field_setRawData(field,
                            (NITF_DATA *) & int32, length, error);
                }
            }
            else
            {
                /* check for the other binary lengths ... */
                if (field->type == NITF_BINARY)
                {
                    /* TODO what to do??? 8 bit is ok, but what about 64? */
                    /* for now, just let it go through... */
                }

                /* now, set the data */
                status = nitf_Field_setRawData(field, (NITF_DATA *) (bufptr + offset),
                        length, error);
            }

#ifdef NITF_DEBUG
            {
                fprintf(stdout, "Adding Field [%s] to TRE [%s]\n",
                        cursor.tag_str, tre->tag);
            }
#endif

            /* add to the hash */
            nitf_HashTable_insert(((nitf_TREPrivateData*)tre->priv)->hash,
                    cursor.tag_str, field, error);

            offset += length;
        }
        /* otherwise, the iterate function thinks we are done */
        else
        {
            break;
        }
    }
    nitf_TRECursor_cleanup(&cursor);

    /* check if we still have more to parse, and throw an error if so */
    if (offset < privData->length)
    {
        nitf_Error_init(error, "TRE data is longer than it should be",
                NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        status = NITF_FAILURE;
    }
    return status;

    /* deal with errors here */
    CATCH_ERROR:
    return NITF_FAILURE;
}

NITFAPI(char *) nitf_TREUtils_getRawData(nitf_TRE * tre, nitf_Uint32* treLength, nitf_Error * error)
{
    int status = 1;
    int offset = 0;
    nitf_Uint32 length;
    int tempLength;
    char *data = NULL; /* data buffer - Caller must free this */
    char *tempBuf = NULL; /* temp data buffer */
    nitf_Pair *pair; /* temp nitf_Pair */
    nitf_Field *field; /* temp nitf_Field */
    nitf_TRECursor cursor; /* the cursor */

    /* get acutal length of TRE */
    length = nitf_TREUtils_computeLength(tre);
    *treLength = length;

    if (length <= 0)
    {
        nitf_Error_init(error, "TRE has invalid length",
                NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        return NULL;
    }

    /* allocate the memory - this does not get freed in this function */
    data = (char *) NITF_MALLOC(length + 1);
    if (!data)
    {
        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO),
                NITF_CTXT, NITF_ERR_MEMORY);
        goto CATCH_ERROR;
    }
    memset(data, 0, length + 1);

    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor) && status && offset < length)
    {
        if (nitf_TRECursor_iterate(&cursor, error) == NITF_SUCCESS)
        {
            pair = nitf_HashTable_find(((nitf_TREPrivateData*)tre->priv)->hash,
                    cursor.tag_str);
            if (pair && pair->data)
            {
                tempLength = cursor.length;
                if (tempLength == NITF_TRE_GOBBLE)
                {
                    tempLength = length - offset;
                }
                field = (nitf_Field *) pair->data;

                /* get the raw data */
                tempBuf = NITF_MALLOC(tempLength);
                if (!tempBuf)
                {
                    nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO),
                            NITF_CTXT, NITF_ERR_MEMORY);
                    goto CATCH_ERROR;
                }
                /* get the data as raw buf */
                nitf_Field_get(field, (NITF_DATA *) tempBuf,
                        NITF_CONV_RAW, tempLength, error);

                /* first, check to see if we need to swap bytes */
                if (field->type == NITF_BINARY)
                {
                    if (tempLength == NITF_INT16_SZ)
                    {
                        nitf_Int16 int16 =
                            (nitf_Int16)NITF_HTONS(*((nitf_Int16 *) tempBuf));
                        memcpy(tempBuf, (char*)&int16, tempLength);
                    }
                    else if (tempLength == NITF_INT32_SZ)
                    {
                        nitf_Int32 int32 =
                            (nitf_Int32)NITF_HTONL(*((nitf_Int32 *) tempBuf));
                        memcpy(tempBuf, (char*)&int32, tempLength);
                    }
                    else
                    {
                        /* TODO what to do??? 8 bit is ok, but what about 64? */
                        /* for now, just let it go through... */
                    }
                }

                /* now, memcpy the data */
                memcpy(data + offset, tempBuf, tempLength);
                offset += tempLength;

                /* free the buf */
                NITF_FREE(tempBuf);
            }
            else
            {
                nitf_Error_init(error,
                "Failed due to missing TRE field(s)",
                NITF_CTXT, NITF_ERR_INVALID_OBJECT);
                goto CATCH_ERROR;
            }
        }
    }
    nitf_TRECursor_cleanup(&cursor);
    return data;

  /* deal with errors here */
  CATCH_ERROR:
    if (data)
        NITF_FREE(data);
    return NULL;
}

NITFAPI(NITF_BOOL) nitf_TREUtils_readField(nitf_IOHandle handle,
        char *fld,
        int length, nitf_Error * error)
{
    NITF_BOOL status;

    /* Make sure the field is nulled out  */
    memset(fld, 0, length);

    /* Read from the IO handle */
    status = nitf_IOHandle_read(handle, fld, length, error);
    if (!status)
    {
        nitf_Error_init(error,
                "Unable to read from IO object",
                NITF_CTXT, NITF_ERR_READING_FROM_FILE);
    }
    return status;
}

NITFAPI(NITF_BOOL) nitf_TREUtils_setValue(nitf_TRE * tre,
        const char *tag,
        NITF_DATA * data,
        size_t dataLength, nitf_Error * error)
{
    nitf_Pair *pair;
    nitf_Field *field = NULL;
    nitf_TRECursor cursor;
    NITF_BOOL done = 0;
    NITF_BOOL status = 1;
    nitf_FieldType type = NITF_BCS_A;
    int length; /* used temporarily for storing the length */
    /* get out if TRE is null */
    if (!tre)
    {
        nitf_Error_init(error, "setValue -> invalid tre object",
                NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }

    /* If the field already exists, get it and modify it */
    if (nitf_HashTable_exists(((nitf_TREPrivateData*)tre->priv)->hash, tag))
    {
        pair = nitf_HashTable_find(
                ((nitf_TREPrivateData*)tre->priv)->hash, tag);
        field = (nitf_Field *) pair->data;

        if (!field)
        {
            nitf_Error_init(error, "setValue -> invalid field object",
                    NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
            return NITF_FAILURE;
        }

        /* check to see if the data passed in is too large or too small */
        if ((dataLength > field->length && !field->resizable) || dataLength < 1)
        {
            nitf_Error_init(error, "setValue -> invalid dataLength",
                    NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
            return NITF_FAILURE;
        }

        if (!nitf_Field_setRawData
            (field, (NITF_DATA *) data, dataLength, error))
        {
            return NITF_FAILURE;
        }
#ifdef NITF_DEBUG
        fprintf(stdout, "Setting (and filling) Field [%s] to TRE [%s]\n",
                tag, tre->tag);
#endif
        
        /* Now we need to fill our data */
        if (!nitf_TREUtils_fillData(tre, 
                                    ((nitf_TREPrivateData*)tre->priv)->description, 
                                    error))
            return NITF_FAILURE;

    }
    /* it doesn't exist in the hash yet, so we need to find it */
    else
    {

        cursor = nitf_TRECursor_begin(tre);
        while (!nitf_TRECursor_isDone(&cursor) && !done && status)
        {
            if (nitf_TRECursor_iterate(&cursor, error) == NITF_SUCCESS)
            {
                /* we found it */
                if (strcmp(tag, cursor.tag_str) == 0)
                {
                    if (cursor.desc_ptr->data_type == NITF_BCS_A)
                    {
                        type = NITF_BCS_A;
                    }
                    else if (cursor.desc_ptr->data_type == NITF_BCS_N)
                    {
                        type = NITF_BCS_N;
                    }
                    else if (cursor.desc_ptr->data_type == NITF_BINARY)
                    {
                        type = NITF_BINARY;
                    }
                    else
                    {
                        /* bad type */
                        nitf_Error_init(error,
                                "setValue -> invalid data type",
                                NITF_CTXT,
                                NITF_ERR_INVALID_PARAMETER);
                        return NITF_FAILURE;
                    }

                    length = cursor.length;
                    /* check to see if we should gobble the rest */
                    if (length == NITF_TRE_GOBBLE)
                    {
                        length = dataLength;
                    }

                    /* construct the field */
                    field = nitf_Field_construct(length, type, error);

                    /* now, set the data */
                    nitf_Field_setRawData(field, (NITF_DATA *) data,
                            dataLength, error);

#ifdef NITF_DEBUG
                    fprintf(stdout, "Adding (and filling) Field [%s] to TRE [%s]\n",
                            cursor.tag_str, tre->tag);
#endif

                    /* add to the hash */
                    nitf_HashTable_insert(
                            ((nitf_TREPrivateData*)tre->priv)->hash,
                            cursor.tag_str, field, error);


                    /* Now we need to fill our data */
                    if (!nitf_TREUtils_fillData(tre, ((nitf_TREPrivateData*)tre->priv)->description, error))
                        return NITF_FAILURE;

                    done = 1; /* set, so we break out of loop */
                }
            }
        }
        /* did we find it? */
        if (!done)
        {
            nitf_Error_initf(error, NITF_CTXT, NITF_ERR_UNK,
                    "Unable to find tag, '%s', in TRE hash for TRE '%s'",
                    tag, tre->tag);
            status = 0;
        }
        nitf_TRECursor_cleanup(&cursor);
    }
    return status;

}

NITFAPI(NITF_BOOL) nitf_TREUtils_setDescription(nitf_TRE* tre,
        nitf_Uint32 length, nitf_Error* error)
{

    nitf_TREDescriptionSet *descriptions = NULL;
    nitf_TREDescriptionInfo *infoPtr = NULL;
    int numDescriptions = 0;
    if (!tre)
    {
        nitf_Error_init(error, "setDescription -> invalid tre object",
                NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }
    descriptions = (nitf_TREDescriptionSet*)tre->handler->data;

    if (!descriptions)
    {
        nitf_Error_init(error, "TRE Description Set is NULL",
                NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        return NITF_FAILURE;
    }
    
    tre->priv = NULL;
    
    infoPtr = descriptions->descriptions;
    while (infoPtr && (infoPtr->description != NULL))
    {
        if (numDescriptions == descriptions->defaultIndex)
        {
            tre->priv = nitf_TREPrivateData_construct(error);
            if (!tre->priv)
            {
                return NITF_FAILURE;
            }
            
            ((nitf_TREPrivateData*)tre->priv)->length = length;
            ((nitf_TREPrivateData*)tre->priv)->description =
                    infoPtr->description;
            break;
        }
        numDescriptions++;
        infoPtr++;
    }

    if (!tre->priv)
    {
        nitf_Error_init(error, "TRE Description is NULL",
                NITF_CTXT, NITF_ERR_INVALID_OBJECT);
        return NITF_FAILURE;;
    }
    
    return NITF_SUCCESS;
}

NITFAPI(NITF_BOOL) nitf_TREUtils_fillData(nitf_TRE * tre,
        const nitf_TREDescription* descrip,
        nitf_Error * error)
{
    nitf_TRECursor cursor;

    /* set the description so the cursor can use it */
    ((nitf_TREPrivateData*)tre->priv)->description = 
        (nitf_TREDescription*)descrip;

    /* loop over the description, and add blank fields for the
     * "normal" fields... any special case fields (loops, conditions)
     * won't be added here
     */
    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor))
    {
        if (nitf_TRECursor_iterate(&cursor, error))
        {
            nitf_Pair* pair = nitf_HashTable_find(
                    ((nitf_TREPrivateData*)tre->priv)->hash, cursor.tag_str);
            if (!pair || !pair->data)
            {
                nitf_Field* field = NULL;
                int fieldLength = cursor.length;
                
                /* If it is a GOBBLE length, there isn't really a standard
                 * on how long it can be... therefore we'll just throw in
                 * a field of size 1, just to have something...
                 */
                if (fieldLength == NITF_TRE_GOBBLE)
                {
                    fieldLength = 1;
                }
                
                field = nitf_Field_construct(fieldLength,
                        cursor.desc_ptr->data_type,
                        error);
                
                /* set the field to be resizable later on */
                if (cursor.length == NITF_TRE_GOBBLE)
                    field->resizable = 1;

                /* special case if BINARY... must set Raw Data */
                if (cursor.desc_ptr->data_type == NITF_BINARY)
                {
                    char* tempBuf = (char *) NITF_MALLOC(fieldLength);
                    if (!tempBuf)
                    {
                        nitf_Field_destruct(&field);
                        nitf_Error_init(error, NITF_STRERROR(NITF_ERRNO),
                                NITF_CTXT, NITF_ERR_MEMORY);
                        goto CATCH_ERROR;
                    }

                    memset(tempBuf, 0, fieldLength);
                    nitf_Field_setRawData(field, (NITF_DATA *) tempBuf,
                            fieldLength, error);
                }
                else if (cursor.desc_ptr->data_type == NITF_BCS_N)
                {
                    /* this will get zero/blank filled by the function */
                    nitf_Field_setString(field, "0", error);
                }
                else
                {
                    /* this will get zero/blank filled by the function */
                    nitf_Field_setString(field, " ", error);
                }
                
                /* add to hash if there wasn't an entry yet */
                if (!pair)
                {
                    nitf_HashTable_insert(
                            ((nitf_TREPrivateData*)tre->priv)->hash,
                            cursor.tag_str, field, error);
                }
                /* otherwise, just set the data pointer */
                else
                {
                    pair->data = (NITF_DATA *) field;
                }
            }
        }
    }
    nitf_TRECursor_cleanup(&cursor);

    /* no problems */
    /*    return tre->descrip; */
    return NITF_SUCCESS;
  
  CATCH_ERROR:
    return NITF_FAILURE;
}

NITFAPI(int) nitf_TREUtils_print(nitf_TRE * tre, nitf_Error * error)
{
    nitf_Pair *pair; /* temp pair */
    int status = NITF_SUCCESS;
    nitf_TRECursor cursor;

    /* get out if TRE is null */
    if (!tre)
    {
        nitf_Error_init(error, "print -> invalid tre object",
                NITF_CTXT, NITF_ERR_INVALID_PARAMETER);
        return NITF_FAILURE;
    }

    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor) && (status == NITF_SUCCESS))
    {
        if ((status = nitf_TRECursor_iterate(&cursor, error)) == NITF_SUCCESS)
        {
            pair = nitf_HashTable_find(
                    ((nitf_TREPrivateData*)tre->priv)->hash, cursor.tag_str);
            if (!pair || !pair->data)
            {
                nitf_Error_initf(error, NITF_CTXT, NITF_ERR_UNK,
                "Unable to find tag, '%s', in TRE hash for TRE '%s'", cursor.tag_str, tre->tag);
                status = NITF_FAILURE;
            }
            else
            {
                printf("%s (%s) = [",
                cursor.desc_ptr->label == NULL ?
                "null" : cursor.desc_ptr->label, cursor.tag_str);
                nitf_Field_print((nitf_Field *) pair->data);
                printf("]\n");
            }
        }
    }
    nitf_TRECursor_cleanup(&cursor);
    return status;
}
NITFAPI(int) nitf_TREUtils_computeLength(nitf_TRE * tre)
{
    int length = 0;
    int tempLength;
    nitf_Error error;
    nitf_Pair *pair; /* temp nitf_Pair */
    nitf_Field *field; /* temp nitf_Field */
    nitf_TRECursor cursor;

    /* get out if TRE is null */
    if (!tre)
        return -1;

    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor))
    {
        if (nitf_TRECursor_iterate(&cursor, &error) == NITF_SUCCESS)
        {
            tempLength = cursor.length;
            if (tempLength == NITF_TRE_GOBBLE)
            {
                /* we don't have any other way to know the length of this
                 * field, other than to see if the field is in the hash
                 * and use the length defined when it was created.
                 * Otherwise, we don't add any length.
                 */
                tempLength = 0;
                pair = nitf_HashTable_find(
                        ((nitf_TREPrivateData*)tre->priv)->hash, cursor.tag_str);
                if (pair)
                {
                    field = (nitf_Field *) pair->data;
                    if (field)
                        tempLength = field->length;
                }
            }
            length += tempLength;
        }
    }
    nitf_TRECursor_cleanup(&cursor);
    return length;
}

NITFAPI(NITF_BOOL) nitf_TREUtils_isSane(nitf_TRE * tre)
{
    int status = 1;
    nitf_Error error;
    nitf_TRECursor cursor;

    /* get out if TRE is null */
    if (!tre)
        return NITF_FAILURE;

    cursor = nitf_TRECursor_begin(tre);
    while (!nitf_TRECursor_isDone(&cursor) && status)
    {
        if (nitf_TRECursor_iterate(&cursor, &error) == NITF_SUCCESS)
            if (!nitf_TRE_exists(tre, cursor.tag_str))
                status = !status;
    }
    nitf_TRECursor_cleanup(&cursor);
    return status;
}

NITFPRIV(NITF_BOOL) basicRead(nitf_IOHandle ioHandle, nitf_Uint32 length,
        nitf_TRE* tre, struct _nitf_Record* record, nitf_Error* error)
{
    int ok;
    char *data = NULL;
    NITF_BOOL success;

    if (!tre)
        goto CATCH_ERROR;
    
    /* set the description/length */
    nitf_TREUtils_setDescription(tre, length, error);
    
    /*if (!tre->descrip) goto CATCH_ERROR;*/ 
    data = (char*)NITF_MALLOC( length );
    if (!data)
    {
        nitf_Error_init(error, NITF_STRERROR( NITF_ERRNO ),NITF_CTXT, NITF_ERR_MEMORY );
        goto CATCH_ERROR;
    }
    memset(data, 0, length);
    success = nitf_TREUtils_readField(ioHandle, data, length, error);
    if (!success) goto CATCH_ERROR;
    
    ok = nitf_TREUtils_parse(tre, data, error);
    NITF_FREE( data );
    data = NULL;
    
    if (!ok)
        goto CATCH_ERROR;
    return NITF_SUCCESS;
  
  CATCH_ERROR:
    if (data) NITF_FREE(data);
    return NITF_FAILURE;
}

NITFPRIV(NITF_BOOL) basicInit(nitf_TRE * tre, const char* id, nitf_Error * error)
{
    nitf_TREDescriptionSet* set;
    const nitf_TREDescriptionInfo* descInfo;
    
    assert(tre);

    set = (nitf_TREDescriptionSet*)tre->handler->data;
    
    /* create a new private data struct */
    tre->priv = nitf_TREPrivateData_construct(error);
    if (!tre->priv)
        return NITF_FAILURE;

    /* search for the description with the given ID - if one was provided */
    if (id)
    {
        descInfo = set->descriptions;
        while(descInfo && descInfo->name)
        {
            if (strcmp(descInfo->name, id) == 0)
            {
                /* we have a match! */
                if (nitf_TREUtils_fillData(tre, descInfo->description, error))
                    return NITF_SUCCESS;
                else
                {
                    nitf_TRE_destruct(&tre);
                    return NITF_FAILURE;
                }
            }
            descInfo++;
        }
    }
    else
    {
        /* TODO - do we try a default one? */
    }
    
    nitf_Error_initf(error, NITF_CTXT, NITF_ERR_INVALID_OBJECT,
            "No matching id '%s' found!", id);
    return NITF_FAILURE;
}

NITFPRIV(NITF_BOOL) basicWrite(nitf_IOHandle ioHandle, nitf_TRE* tre,
        struct _nitf_Record* record, nitf_Error* error)
{
    nitf_Uint32 length;
    char* data = nitf_TREUtils_getRawData(tre, &length, error);
    if (!data) return NITF_FAILURE;
    return nitf_IOHandle_write(ioHandle, data, length, error);
}

NITFPRIV(int) basicGetCurrentSize(nitf_TRE* tre, nitf_Error* error)
{
    return nitf_TREUtils_computeLength(tre);
}


NITFPRIV(NITF_BOOL) basicClone(nitf_TRE *source,
                               nitf_TRE *tre,
                               nitf_Error *error)
{
    nitf_TREPrivateData *sourcePriv = NULL;
    nitf_TREPrivateData *trePriv = NULL;
    
    if (!tre || !source || !source->priv)
        return NITF_FAILURE;
    
    sourcePriv = (nitf_TREPrivateData*)source->priv;
    
    /* this clones the hash */
    if (!(trePriv = nitf_TREPrivateData_clone(sourcePriv, error)))
        return NITF_FAILURE;
    
    /* just copy over the optional length and static description */
    trePriv->length = sourcePriv->length;
    trePriv->description = sourcePriv->description;
    
    tre->priv = trePriv;
    
    return NITF_SUCCESS;
}


NITFPRIV(void) basicDestruct(nitf_TRE* tre)
{
    if (tre && tre->priv)
        nitf_TREPrivateData_destruct((nitf_TREPrivateData**)&tre->priv);
}

/*
 *  TODO: Make all these private functions accessible so that our other
 *  handlers, like DefaultTRE can make use of them
 *
 */
NITFPRIV(nitf_List*) basicFind(nitf_TRE* tre,
                               const char* pattern,
                               nitf_Error* error)
{
    nitf_List* list;
    
    nitf_HashTableIterator it = nitf_HashTable_begin(
            ((nitf_TREPrivateData*)tre->priv)->hash);
    nitf_HashTableIterator end = nitf_HashTable_end(
            ((nitf_TREPrivateData*)tre->priv)->hash);

    list = nitf_List_construct(error);
    if (!list) return NULL;

    while (nitf_HashTableIterator_notEqualTo(&it, &end))
    {
        nitf_Pair* pair = nitf_HashTableIterator_get(&it);

        if (strstr(pair->key, pattern))
        {
            /* Should check this, I suppose */
            nitf_List_pushBack(list, pair, error);

        }
        nitf_HashTableIterator_increment(&it);

    }

    return list;
}

NITFPRIV(NITF_BOOL) basicSetField(nitf_TRE* tre, const char* tag, NITF_DATA* data, size_t dataLength, nitf_Error* error)
{
    return nitf_TREUtils_setValue(tre, tag, data, dataLength, error);
}

NITFPRIV(nitf_Field*) basicGetField(nitf_TRE* tre, const char* tag)
{
    nitf_Pair* pair = nitf_HashTable_find(
            ((nitf_TREPrivateData*)tre->priv)->hash, tag);
    if (!pair) return NULL;
    return (nitf_Field*)pair->data;
}

NITFPRIV(NITF_BOOL) basicIncrement(nitf_TREEnumerator** it, nitf_Error* error)
{
    /* Free this baby */
    nitf_TRECursor* cursor = (nitf_TRECursor*)(*it)->data;

    if (nitf_TRECursor_isDone(cursor))
    {
        nitf_TRECursor_cleanup(cursor);
        NITF_FREE(cursor);
        NITF_FREE(*it);
        *it = NULL;
        return NITF_SUCCESS;
    }

    return nitf_TRECursor_iterate(cursor, error);
}

NITFPRIV(nitf_Pair*) basicGet(nitf_TREEnumerator* it, nitf_Error* error)
{
    nitf_TRECursor* cursor = (nitf_TRECursor*)it->data;
    nitf_Pair* data;

    if (!nitf_TRE_exists(cursor->tre, cursor->tag_str))
        goto CATCH_ERROR;

    data = nitf_HashTable_find(((nitf_TREPrivateData*)cursor->tre->priv)->hash,
            cursor->tag_str);
    if (!data)
        goto CATCH_ERROR;

    return data;

  CATCH_ERROR:
    nitf_Error_initf(error, NITF_CTXT, NITF_ERR_INVALID_OBJECT,
            "Couldnt retrieve tag [%s]", cursor->tag_str);
    return NULL;
}

NITFPRIV(nitf_TREEnumerator*) basicBegin(nitf_TRE* tre, nitf_Error* error)
{
    nitf_TREEnumerator* it =
        (nitf_TREEnumerator*)NITF_MALLOC(sizeof(nitf_TREEnumerator));
    nitf_TRECursor* cursor =
        (nitf_TRECursor*)NITF_MALLOC(sizeof(nitf_TRECursor));
    *cursor = nitf_TRECursor_begin(tre);
    assert(nitf_TRECursor_iterate(cursor, error));
    
    it->data = cursor;
    it->next = basicIncrement;
    it->get = basicGet;
    return it;

}

NITFAPI(nitf_TREHandler*) nitf_TREUtils_createBasicHandler(nitf_TREDescriptionSet* set, nitf_TREHandler *handler,nitf_Error* error)
{
    handler->init = basicInit;
    handler->read = basicRead;
    handler->setField = basicSetField;
    handler->getField = basicGetField;
    handler->find = basicFind;
    handler->write = basicWrite;
    handler->begin = basicBegin;
    handler->getCurrentSize = basicGetCurrentSize;
    handler->clone = basicClone;
    handler->destruct = basicDestruct;

    handler->data = set;
    return handler;
}
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>           // for isnan and isinf

#include "../merc.h"
#include "array.h"

// Returns whether the ARRAY is empty.
bool is_array_empty(ARRAY *arr)
{
    return !arr || !arr->ptr || arr->length == 0;
}

// Creates an array with the given length and element size
//
// @param length Number of elements in array (length can be zero)
// @param size Size of each element (in bytes) (size may not be zero)
// @param deleter (optional) Function used to delete the elements when freed.
// @param copier Function required when copying the array or setting the element.
//
// @return Returns the allocated ARRAY, or NULL if an error occurred.
ARRAY *new_arrayx(size_t length, size_t size, ARRAY_DELETER_FUNC *deleter, ARRAY_COPIER_FUNC *copier)
{
    if (size < 1) return NULL;

    ARRAY *arr = malloc(sizeof(ARRAY));

    if (arr)
    {
        if (length > 0)
        {
            arr->ptr = malloc(length * size);
            if (!arr->ptr)
            {
                free(arr);
                return NULL;
            }
            memset(arr->ptr, 0, length * size);
        }
        else
            arr->ptr = NULL;

        arr->size = size;
        arr->length = length;
        arr->deleter = deleter;
        arr->copier = copier;
    }
    return arr;
}

ARRAY *new_array(size_t length, size_t size) { return new_arrayx(length, size, NULL, NULL); }

ARRAY *copy_array(ARRAY *arr)
{
    if (arr)
    {
        if (arr->size < 1) return NULL;

        ARRAY *arr2 = malloc(sizeof(ARRAY));

        if (arr2)
        {
            if (arr->ptr && arr->length > 0)
            {
                arr2->ptr = malloc(arr->length * arr->size);
                if (!arr2->ptr)
                {
                    free(arr2);
                    return NULL;
                }

                if (arr->copier)
                {
                    void *ptr = arr->ptr;
                    void *ptr2 = arr2->ptr;
                    for(int i = 0; i < arr->length; i++, ptr+=arr->size, ptr2+=arr->size)
                    {
                        (*arr->copier)(ptr2, ptr);
                    }
                }
                else
                    memcpy(arr2->ptr, arr->ptr, arr->length * arr->size);
            }
            else
                arr2->ptr = NULL;   // "Empty" array
            arr2->size = arr->size;
            arr2->length = arr->length;
            arr2->deleter = arr->deleter;
            arr2->copier = arr->copier;
        }

        return arr2;
    }

    return NULL;
}

void free_array(ARRAY *arr)
{
    if (arr)
    {
        if (arr->ptr)
        {
            if (arr->deleter)
            {
                void *ptr = arr->ptr;
                for(size_t i = 0; i < arr->length; i++, ptr+=arr->size)
                {
                    (*arr->deleter)(ptr);
                }
            }

            free(arr->ptr);
        }

        free(arr);
    }
}

// This will return the address of the element.
// This will allow for changing the value.
void *array_get(ARRAY *arr, size_t index)
{
    if (arr && arr->ptr)
    {
        if (index >= 0 && index < arr->length)
        {
            return (void *)(arr->ptr + (arr->size * index));
        }
    }

    return NULL;
}

bool array_set(ARRAY *arr, size_t index, void *data)
{
    if (arr && arr->ptr && arr->copier)
    {
        if (index >= 0 && index < arr->length)
        {
            void *ptr = (void *)(arr->ptr + (arr->size * index));

            (*arr->copier)(ptr, data);
            return true;
        }
    }

    return false;
}

bool array_set_length(ARRAY *arr, size_t new_length)
{
    if (arr)
    {
        if (arr->length > new_length)       // Ensmallen the array
        {
            // Need to remove excess data
            if (arr->deleter)
            {
                void *ptr = array_get(arr, new_length);
                for(int i = new_length; i < arr->length; i++, ptr += arr->size)
                {
                    (*arr->deleter)(ptr);
                }
            }

            if (new_length > 0)
            {
                void *new_ptr = realloc(arr->ptr, new_length * arr->size);
                if (!new_ptr)
                {
                    // This should not happen in this case since memory is being reduced.
                    return false;
                }
                arr->ptr = new_ptr;
            }
            else if (arr->ptr)
            {
                // Data should already be freed if the deleter was set
                free(arr->ptr);
                arr->ptr = NULL;
            }
            arr->length = new_length;
        }
        else if (arr->length < new_length)  // Embiggen the array
        {
            void *new_ptr = realloc(arr->ptr, new_length * arr->size);
            if (!new_ptr)
            {
                return false;
            }

            arr->ptr = new_ptr;

            // Initialize new elements
            memset(arr->ptr + arr->size * arr->length, 0, arr->size * (new_length - arr->length));
            arr->length = new_length;
        }
        // Otherwise, the array doesn't need to be changed
        
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------
 * array_append
 *
 * Grows the array by one and copies data into the new last slot.
 * Requires a copier.  Returns false on allocation failure or missing copier.
 * ---------------------------------------------------------------------- */
bool array_append(ARRAY *arr, void *data)
{
    if (!arr || !arr->copier) return false;

    if (!array_set_length(arr, arr->length + 1)) return false;

    return array_set(arr, arr->length - 1, data);
}

/* -------------------------------------------------------------------------
 * array_insert
 *
 * Inserts data at index, shifting all elements from index onward up by one.
 * Passing index == arr->length is equivalent to array_append.
 * Requires a copier.  Returns false on out-of-range index, allocation
 * failure, or missing copier.
 * ---------------------------------------------------------------------- */
bool array_insert(ARRAY *arr, size_t index, void *data)
{
    if (!arr || !arr->copier)       return false;
    if (index > arr->length)        return false;

    /* Append first to get the allocation and length bump done */
    if (!array_set_length(arr, arr->length + 1)) return false;

    /* Shift elements [index .. length-2] up by one slot, back-to-front
     * to avoid overwriting data that hasn't been moved yet.
     * For types with a deleter (e.g. strings) the new last slot was
     * zeroed by array_set_length, so no stale pointer is left behind. */
    for (size_t i = arr->length - 1; i > index; i--)
    {
        void *dst = array_get(arr, i);
        void *src = array_get(arr, i - 1);
        /* Raw byte move — bypasses copier/deleter intentionally since we
         * are just relocating ownership, not duplicating. */
        memmove(dst, src, arr->size);
    }

    /* Zero the vacated slot so that array_set's copier starts clean
     * (important for string arrays: avoids freeing a stale pointer). */
    // This is only needed because if the array set somehow fails, and the array is freed, there won't be any erroneous value freed when dealing with strings
    memset(array_get(arr, index), 0, arr->size);

    return array_set(arr, index, data);
}

/* -------------------------------------------------------------------------
 * array_remove
 *
 * Removes the element at index, shifting all later elements down by one,
 * then shrinks the array by one.
 * The deleter (if any) is called on the element being removed.
 * Returns false on out-of-range index.
 * ---------------------------------------------------------------------- */
bool array_remove(ARRAY *arr, size_t index)
{
    if (!arr || !arr->ptr)      return false;
    if (index >= arr->length)   return false;

    /* Delete the element being removed */
    if (arr->deleter)
        (*arr->deleter)(array_get(arr, index));

    /* Shift elements [index+1 .. length-1] down by one slot.
     * Raw byte move — ownership is being transferred, not duplicated,
     * so we deliberately skip the copier/deleter here. */
    for (size_t i = index; i < arr->length - 1; i++)
        memmove(array_get(arr, i), array_get(arr, i + 1), arr->size);

    /* Zero the now-vacated last slot before shrinking so the deleter
     * inside array_set_length doesn't chase a stale pointer. */
    // This was irrelevant because the pointer was already deleted.
    // It will soon be 
    //memset(array_get(arr, arr->length - 1), 0, arr->size);

    /* Shrink by one — pass a sentinel NULL deleter path since we already
     * deleted the element above and zeroed the tail. */
    void *new_ptr = realloc(arr->ptr, (arr->length - 1) * arr->size);
    if (!new_ptr && arr->length > 1) return false;
    if (new_ptr) arr->ptr = new_ptr;
    arr->length--;

    return true;
}



static void __int_array_copier(void *ptr, void *src)
{
    if (ptr && src)
    {
        *((int *)ptr) = *((int *)src);
    }
}

ARRAY *new_int_array(size_t length)
{
    return new_arrayx(length, sizeof(int), NULL, __int_array_copier);
}

static void __float_array_copier(void *ptr, void *src)
{
    if (ptr && src)
    {
        *((double *)ptr) = *((double *)src);
    }
}

ARRAY *new_float_array(size_t length)
{
    return new_arrayx(length, sizeof(double), NULL, __float_array_copier);
}

static void __string_array_deleter(void *data)
{
    // Data is the *address* to the element in the array
    char *str = *((char **)data);
    if (str)
        free_string(str);
}

static void __string_array_copier(void *ptr, void *src)
{
    if (ptr && src)
    {
        char *str = *((char **)src);
        *((char **)ptr) = str ? str_dup(str) : NULL;
    }
}

ARRAY *new_string_array(size_t length)
{
    return new_arrayx(length, sizeof(char *), __string_array_deleter, __string_array_copier);
}



/* =========================================================================
 * Internal parsing helpers
 * ======================================================================= */

/* Advance past optional whitespace */
// Might merge this in with the existing "skip_whitespace" function
static const char *_skip_ws(const char *p)
{
    while (p && isspace((unsigned char)*p)) p++;
    return p;
}

/* -------------------------------------------------------------------------
 * _hex4
 * Parse exactly 4 hex digits from p, store codepoint in *out.
 * Returns pointer past the 4 digits, or NULL on invalid input.
 * This is used for decoding \uXXXX codes for Unicode.
 * ---------------------------------------------------------------------- */
static const char *_hex4(const char *p, uint32_t *out)
{
    *out = 0;
    for (int i = 0; i < 4; i++, p++)
    {
        *out <<= 4;
        if      (*p >= '0' && *p <= '9') *out |= (uint32_t)(*p - '0');
        else if (*p >= 'a' && *p <= 'f') *out |= (uint32_t)(*p - 'a' + 10);
        else if (*p >= 'A' && *p <= 'F') *out |= (uint32_t)(*p - 'A' + 10);
        else return NULL;
    }
    return p;
}

/* -------------------------------------------------------------------------
 * _encode_utf8
 * Encode a Unicode codepoint into buf (must have at least 4 bytes free).
 * Returns number of bytes written, or 0 on invalid codepoint.
 * ---------------------------------------------------------------------- */
// TODO: integrate with the utf8_support branch when ready as there are existing functions for that
static size_t _encode_utf8(uint32_t cp, char *buf)
{
    if (cp <= 0x7F)
    {
        buf[0] = (char)cp;
        return 1;
    }
    else if (cp <= 0x7FF)
    {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    else if (cp <= 0xFFFF)
    {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    else if (cp <= 0x10FFFF)
    {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6)  & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * _parse_escape
 *
 * Called when a '\' has just been consumed.  Parses the escape sequence
 * at *pp, optionally writes decoded bytes to *dst, advances both pointers.
 *
 * Pass dst=NULL for a dry-run byte count (first pass).
 * Returns number of bytes written (or that would be written).
 * ---------------------------------------------------------------------- */
static size_t _parse_escape(const char **pp, char **dst)
{
    const char *p = *pp;
    char utf8buf[4];
    size_t written = 0;

    switch (*p)
    {
        /* Standard single-character JSON escapes */
        case '"':  case '\\': case '/':
            if (dst) *(*dst)++ = *p;
            written = 1; p++;
            break;
        case 'b':
            if (dst) *(*dst)++ = '\b';
            written = 1; p++;
            break;
        case 'f':
            if (dst) *(*dst)++ = '\f';
            written = 1; p++;
            break;
        case 'n':
            if (dst) *(*dst)++ = '\n';
            written = 1; p++;
            break;
        case 'r':
            if (dst) *(*dst)++ = '\r';
            written = 1; p++;
            break;
        case 't':
            if (dst) *(*dst)++ = '\t';
            written = 1; p++;
            break;

        case 'u':
        {
            p++;    /* skip 'u' */
            uint32_t cp = 0;
            const char *after = _hex4(p, &cp);
            if (!after) { written = 0; break; }
            p = after;

            /* Surrogate pair: high \uD800-\uDBFF + low \uDC00-\uDFFF */
            if (cp >= 0xD800 && cp <= 0xDBFF)
            {
                if (p[0] == '\\' && p[1] == 'u')
                {
                    p += 2;
                    uint32_t low = 0;
                    after = _hex4(p, &low);
                    if (after && low >= 0xDC00 && low <= 0xDFFF)
                    {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                        p = after;
                    }
                    /* else: leave cp as lone high surrogate (best-effort) */
                }
            }

            written = _encode_utf8(cp, utf8buf);
            if (dst && written)
            {
                memcpy(*dst, utf8buf, written);
                *dst += written;
            }
            break;
        }

        default:
            /* Unknown escape: preserve the character as-is */
            if (dst) *(*dst)++ = *p;
            written = 1; p++;
            break;
    }

    *pp = p;
    return written;
}

/* -------------------------------------------------------------------------
 * _utf8_to_unicode
 *
 * Decode one UTF-8 sequence from *pp, advance *pp, return the codepoint.
 * Returns U+FFFD (replacement character) on invalid input.
 * ---------------------------------------------------------------------- */
static uint32_t _utf8_to_unicode(const char **pp)
{
    const unsigned char *p = (const unsigned char *)*pp;
    uint32_t cp;
    size_t extra;

    if      (*p < 0x80) { cp = *p;        extra = 0; }  /* ASCII */
    else if (*p < 0xC0) { cp = 0xFFFD;    extra = 0; }  /* stray continuation  */
    else if (*p < 0xE0) { cp = *p & 0x1F; extra = 1; }  /* 2-byte start */
    else if (*p < 0xF0) { cp = *p & 0x0F; extra = 2; }  /* 3 byte start */
    else if (*p < 0xF8) { cp = *p & 0x07; extra = 3; }  /* 4 byte start */
    else                { cp = 0xFFFD;    extra = 0; }  /* invalid lead byte   */

    p++;
    for (size_t i = 0; i < extra; i++, p++)
    {
        if ((*p & 0xC0) != 0x80) { cp = 0xFFFD; break; }
        cp = (cp << 6) | (*p & 0x3F);
    }

    // Invalid code points
    //  above 0x10FFFF (the current upper limit on Unicode?)
    //  below 0x800 for 3 byte codes (overlong encoding)
    //  below 0x10000 for 4 byte codes (overlong encodings)
    //  from 0xD800 to 0xDFFF (surrogate pair range)
    if (cp > 0x10FFFF || (extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000) || (cp >= 0xD800 && cp <= 0xDFFF))
        cp = 0xFFFD;

    *pp = (const char *)p;
    return cp;
}

/* -------------------------------------------------------------------------
 * _escape_string
 *
 * Converts a raw UTF-8 string to a JSON-quoted string suitable for use
 * in array_join output.  Uses two passes (measure, then build).
 *
 *   - Wraps output in double-quotes
 *   - Escapes control chars, '"', and '\'
 *   - Passes valid UTF-8 multi-byte sequences through as-is
 *   - Encodes codepoints > U+FFFF as surrogate pairs (\uHHHH\uHHHH)
 *   - Encodes invalid UTF-8 bytes as \uFFFD
 *
 * Returns a newly allocated string — caller must free() it.
 * ---------------------------------------------------------------------- */
static char *_escape_string(const char *s)
{
    if (!s) return NULL;

    /* ---- Measure output length ---- */
    const char *p = s;
    size_t out_len = 2;     /* opening and closing '"' */

    while (*p)
    {
        unsigned char c = (unsigned char)*p;

        if (c < 0x20)
        {
            switch (c) {
                case '\b': case '\f': case '\n':
                case '\r': case '\t': out_len += 2; break;  /* \x        */
                default:              out_len += 6; break;  /* \uXXXX    */
            }
            p++;
        }
        else if (c == '"' || c == '\\') { out_len += 2; p++; }
        else if (c < 0x80)              { out_len += 1; p++; }
        else
        {
            const char *before = p;
            uint32_t cp = _utf8_to_unicode(&p);
            size_t raw_bytes = (size_t)(p - before);

            if      (cp == 0xFFFD && raw_bytes == 1) out_len += 6;   /* \uFFFD          */
            else if (cp > 0xFFFF)                    out_len += 12;  /* surrogate pair  */
            else                                     out_len += raw_bytes; /* pass-through */
        }
    }

    /* ---- Build output ---- */
    char *out = malloc(out_len + 1);
    if (!out) return NULL;

    char *dst = out;
    *dst++ = '"';

    p = s;
    while (*p)
    {
        unsigned char c = (unsigned char)*p;

        if (c < 0x20)
        {
            switch (c) {
                case '\b': *dst++ = '\\'; *dst++ = 'b';  p++; break;
                case '\f': *dst++ = '\\'; *dst++ = 'f';  p++; break;
                case '\n': *dst++ = '\\'; *dst++ = 'n';  p++; break;
                case '\r': *dst++ = '\\'; *dst++ = 'r';  p++; break;
                case '\t': *dst++ = '\\'; *dst++ = 't';  p++; break;
                default:
                    snprintf(dst, 7, "\\u%04X", (unsigned)c);
                    dst += 6; p++;
                    break;
            }
        }
        else if (c == '"')  { *dst++ = '\\'; *dst++ = '"';  p++; }
        else if (c == '\\') { *dst++ = '\\'; *dst++ = '\\'; p++; }
        else if (c < 0x80)  { *dst++ = *p++; }
        else
        {
            const char *before = p;
            uint32_t cp = _utf8_to_unicode(&p);
            size_t raw_bytes = (size_t)(p - before);

            if (cp == 0xFFFD && raw_bytes == 1)
            {
                memcpy(dst, "\\uFFFD", 6);
                dst += 6;
            }
            else if (cp > 0xFFFF)
            {
                uint32_t hi = 0xD800 + ((cp - 0x10000) >> 10);
                uint32_t lo = 0xDC00 + ((cp - 0x10000) & 0x3FF);
                snprintf(dst, 13, "\\u%04X\\u%04X", (hi&0xFFFF), (lo&0xFFFF));
                dst += 12;
            }
            else
            {
                memcpy(dst, before, raw_bytes);
                dst += raw_bytes;
            }
        }
    }

    *dst++ = '"';
    *dst   = '\0';
    return out;
}


/* =========================================================================
 * Split functions
 * ======================================================================= */

/* -------------------------------------------------------------------------
 * split_string_array
 *
 * Parses:  ["hello","caf\u00e9","\uD83D\uDE00","日本語",NULL]
 *
 * NULL handling:
 *   - A bare (unquoted) NULL token sets that array element to a NULL
 *     char* (i.e. the slot holds a null pointer, not the string "NULL").
 *   - Matching is case-insensitive: NULL, null, Null, etc. all accepted.
 *
 * UTF-8 safety:
 *   - All delimiter scanning operates on raw bytes.
 *   - UTF-8 continuation bytes (0x80-0xBF) and leading bytes (0xC0-0xFF)
 *     never equal any ASCII delimiter tested here.
 *   - \uXXXX escapes are decoded to UTF-8 via _parse_escape.
 *   - Surrogate pairs are recombined before encoding.
 *   - Buffer is sized by a dry-run byte count for exact allocation.
 * ---------------------------------------------------------------------- */
ARRAY *split_string_array(const char *input)
{
    if (!input) return NULL;

    const char *p = _skip_ws(input);
    if (*p != '[') return NULL;
    p++;

    /* ---- First pass: count tokens ---- */
    size_t count = 0;
    const char *scan = p;
    while (*scan)
    {
        scan = _skip_ws(scan);
        if (*scan == ']') break;
        if (*scan == '"')
        {
            scan++;
            while (*scan && *scan != '"')
            {
                if (*scan == '\\') { scan++; _parse_escape(&scan, NULL); }
                else scan++;
            }
            if (*scan == '"') scan++;
            count++;
        }
        else if (strncasecmp(scan, "NULL", 4) == 0)
        {
            /* Bare NULL token (case-insensitive): confirm it ends on a
             * delimiter so we don't mis-fire on an unquoted word that
             * merely starts with "null". */
            const char *after = _skip_ws(scan + 4);
            if (*after == ',' || *after == ']' || *after == '\0')
            {
                scan += 4;
                count++;
            }
        }
        scan = _skip_ws(scan);
        if (*scan == ',') scan++;
    }

    if (count == 0) return NULL;

    ARRAY *arr = new_string_array(count);
    if (!arr) return NULL;

    /* ---- Second pass: extract and store ---- */
    size_t idx = 0;
    p = _skip_ws(input);
    p++;    /* skip '[' */

    while (*p && idx < count)
    {
        p = _skip_ws(p);
        if (*p == ']') break;

        if (*p == '"')
        {
            p++;    /* skip opening quote */

            /* Dry run: measure exact byte length after escape decoding */
            const char *start = p;
            size_t byte_len = 0;
            const char *tmp = p;
            while (*tmp && *tmp != '"')
            {
                if (*tmp == '\\') { tmp++; byte_len += _parse_escape(&tmp, NULL); }
                else              { tmp++; byte_len++; }
            }

            char *buf = malloc(byte_len + 1);
            if (!buf) { free_array(arr); return NULL; }

            /* Live run: copy decoded bytes into buf */
            char *dst = buf;
            p = start;
            while (*p && *p != '"')
            {
                if (*p == '\\') { p++; _parse_escape(&p, &dst); }
                else            *dst++ = *p++;
            }
            *dst = '\0';

            array_set(arr, idx++, &buf);
            free(buf);  /* string copier in array_set duplicates it */

            if (*p == '"') p++;     /* skip closing quote */
        }
        else if (strncasecmp(p, "NULL", 4) == 0)
        {
            const char *after = _skip_ws(p + 4);
            if (*after == ',' || *after == ']' || *after == '\0')
            {
                /* Store a null pointer in this slot.
                 * array_set uses the copier which would call str_dup(NULL),
                 * so write directly — the slot was zeroed by new_arrayx. */
                char *null_ptr = NULL;
                void *slot = array_get(arr, idx++);
                if (slot) *((char **)slot) = null_ptr;
                p += 4;
            }
        }

        p = _skip_ws(p);
        if (*p == ',') p++;
    }

    return arr;
}

/* -------------------------------------------------------------------------
 * split_int_array
 *
 * Parses:  [1,3,7,1]
 * ---------------------------------------------------------------------- */
ARRAY *split_int_array(const char *input)
{
    if (!input) return NULL;

    const char *p = _skip_ws(input);
    if (*p != '[') return NULL;
    p++;

    /* ---- First pass: count tokens ---- */
    size_t count = 0;
    const char *scan = p;
    while (*scan)
    {
        scan = _skip_ws(scan);
        if (*scan == ']') break;
        if (*scan == '-' || isdigit((unsigned char)*scan))
        {
            if (*scan == '-') scan++;
            while (isdigit((unsigned char)*scan)) scan++;
            count++;
        }
        scan = _skip_ws(scan);
        if (*scan == ',') scan++;
    }

    if (count == 0) return NULL;

    ARRAY *arr = new_int_array(count);
    if (!arr) return NULL;

    /* ---- Second pass: parse and store ---- */
    size_t idx = 0;
    p = _skip_ws(input);
    p++;    /* skip '[' */

    while (*p && idx < count)
    {
        p = _skip_ws(p);
        if (*p == ']') break;

        if (*p == '-' || isdigit((unsigned char)*p))
        {
            char *end = NULL;
            int val = (int)strtol(p, &end, 10);
            array_set(arr, idx++, &val);
            p = end;
        }

        p = _skip_ws(p);
        if (*p == ',') p++;
    }

    return arr;
}

/* -------------------------------------------------------------------------
 * _is_special_float
 *
 * Returns the number of characters consumed if the text at p is a
 * case-insensitive NaN or Inf/Infinity token (optionally preceded by a
 * sign already consumed by the caller), otherwise 0.
 * ---------------------------------------------------------------------- */
static size_t _is_special_float(const char *p)
{
    if (strncasecmp(p, "infinity", 8) == 0) return 8;
    if (strncasecmp(p, "inf",      3) == 0) return 3;
    if (strncasecmp(p, "nan",      3) == 0) return 3;
    return 0;
}


/* -------------------------------------------------------------------------
 * split_float_array
 *
 * Parses:  [1.3,7.4,89.0]
 *
 * Also handles:
 *   - Scientific notation:  1.5e10, 2E-3
 *   - NaN:                  NaN, nan, NAN  (any case)
 *   - Infinity:             Inf, inf, Infinity, infinity, +Inf, -Inf, etc.
 *
 * join_float_array emits NaN, Inf, or -Inf for these special values.
 * ---------------------------------------------------------------------- */
ARRAY *split_float_array(const char *input)
{
    if (!input) return NULL;

    const char *p = _skip_ws(input);
    if (*p != '[') return NULL;
    p++;

    /* ---- First pass: count tokens ---- */
    size_t count = 0;
    const char *scan = p;
    while (*scan)
    {
        scan = _skip_ws(scan);
        if (*scan == ']') break;

        /* optional leading sign */
        const char *tok = scan;
        if (*tok == '+' || *tok == '-') tok++;

        size_t special = _is_special_float(tok);
        if (special)
        {
            scan = tok + special;
            count++;
        }
        else if (*tok == '.' || isdigit((unsigned char)*tok))
        {
            scan = tok;
            while (isdigit((unsigned char)*scan) || *scan == '.') scan++;
            if (*scan == 'e' || *scan == 'E')   /* optional exponent */
            {
                scan++;
                if (*scan == '+' || *scan == '-') scan++;
                while (isdigit((unsigned char)*scan)) scan++;
            }
            count++;
        }
        scan = _skip_ws(scan);
        if (*scan == ',') scan++;
    }

    if (count == 0) return NULL;

    ARRAY *arr = new_float_array(count);
    if (!arr) return NULL;

    /* ---- Second pass: parse and store ---- */
    /* strtod handles nan/inf/infinity (with optional sign) on all
     * POSIX-compliant platforms, so we just widen the entry condition
     * to also accept '+', and let strtod do the actual work. */
    size_t idx = 0;
    p = _skip_ws(input);
    p++;    /* skip '[' */

    while (*p && idx < count)
    {
        p = _skip_ws(p);
        if (*p == ']') break;

        /* peek past optional sign to check for digit, dot, or special */
        const char *tok = p;
        if (*tok == '+' || *tok == '-') tok++;

        if (*tok == '.' || isdigit((unsigned char)*tok) || _is_special_float(tok))
        {
            char *end = NULL;
            double val = strtod(p, &end);
            array_set(arr, idx++, &val);
            p = end;
        }

        p = _skip_ws(p);
        if (*p == ',') p++;
    }

    return arr;
}

/* =========================================================================
 * Join functions
 * ======================================================================= */

/* -------------------------------------------------------------------------
 * array_join  (generic)
 *
 * Calls stringer(element) for each element, assembles [t0,t1,...,tN].
 * Each token returned by stringer must be heap-allocated; array_join
 * free()s them after use.
 * ---------------------------------------------------------------------- */
char *array_join(ARRAY *arr, ARRAY_STRINGER_FUNC *stringer)
{
    if (!arr || !arr->ptr || arr->length == 0 || !stringer) return NULL;

    /* ---- Stringify each element, accumulate total byte length ---- */
    char **tokens = malloc(arr->length * sizeof(char *));
    if (!tokens) return NULL;

    size_t total = 2;               /* '[' and ']'  */
    if (arr->length > 1)
        total += arr->length - 1;   /* commas       */

    for (size_t i = 0; i < arr->length; i++)
    {
        void *elem = array_get(arr, i);
        tokens[i] = stringer(elem);
        if (!tokens[i])
        {
            for (size_t j = 0; j < i; j++) free(tokens[j]);
            free(tokens);
            return NULL;
        }
        total += strlen(tokens[i]);
    }

    /* ---- Assemble output string ---- */
    char *out = malloc(total + 1);
    if (!out)
    {
        for (size_t i = 0; i < arr->length; i++) free(tokens[i]);
        free(tokens);
        return NULL;
    }

    char *dst = out;
    *dst++ = '[';
    for (size_t i = 0; i < arr->length; i++)
    {
        if (i > 0) *dst++ = ',';
        size_t len = strlen(tokens[i]);
        memcpy(dst, tokens[i], len);
        dst += len;
        free(tokens[i]);
    }
    *dst++ = ']';
    *dst   = '\0';

    free(tokens);
    return out;
}

/* ---- Concrete stringers (static — not part of public API) ------------ */

static char *__int_stringer(const void *data)
{
    if (!data) return NULL;
    int val = *((const int *)data);
    char *buf = malloc(13);         /* INT_MIN = 11 digits + sign + NUL */
    if (buf) snprintf(buf, 13, "%d", val);
    return buf;
}

static char *__float_stringer(const void *data)
{
    if (!data) return NULL;
    double val = *((const double *)data);

    /* Emit canonical tokens for special values so output is portable
     * and round-trips cleanly back through split_float_array. */
    if (isnan(val))
    {
        char *buf = malloc(4);
        if (buf) memcpy(buf, "NaN", 4);
        return buf;
    }
    if (isinf(val))
    {
        char *buf = malloc(5);
        if (buf) memcpy(buf, val > 0 ? "Inf" : "-Inf", val > 0 ? 4 : 5);
        return buf;
    }

    char *buf = malloc(32);         /* "%.17g" worst case ~24 bytes */
    if (buf) snprintf(buf, 32, "%.17g", val);
    return buf;
}

static char *__string_stringer(const void *data)
{
    if (!data) return NULL;
    /* data is the address of the char* element in the array */
    const char *s = *((const char **)data);
    if (!s)
    {
        /* Null pointer element — emit the bare token "NULL" */
        char *buf = malloc(5);
        if (buf) memcpy(buf, "NULL", 5);    /* includes NUL terminator */
        return buf;
    }
    return _escape_string(s);
}
/* ---- Concrete join functions ----------------------------------------- */

char *join_int_array(ARRAY *arr)    { return array_join(arr, __int_stringer);    }
char *join_float_array(ARRAY *arr)  { return array_join(arr, __float_stringer);  }
char *join_string_array(ARRAY *arr) { return array_join(arr, __string_stringer); }


/* =========================================================================
 * Stack operations  (LIFO — top is the last element)
 * ======================================================================= */

/* -------------------------------------------------------------------------
 * array_stack_push
 *
 * Copies data onto the top of the stack (appends to the end).
 * Requires a copier.  Returns false on allocation failure or no copier.
 * ---------------------------------------------------------------------- */
bool array_stack_push(ARRAY *arr, void *data)
{
    return array_append(arr, data);
}

/* -------------------------------------------------------------------------
 * array_stack_pop
 *
 * Copies the top element (last slot) into *out, then removes it.
 * Pass out=NULL to discard the value (deleter still called if set).
 * Returns false on empty array.
 * ---------------------------------------------------------------------- */
bool array_stack_pop(ARRAY *arr, void *out)
{
    if (is_array_empty(arr)) return false;

    size_t top = arr->length - 1;
    void  *slot = array_get(arr, top);

    /* Copy value to caller's buffer before deletion */
    if (out)
    {
        /* Transfer ownership: copy raw bytes, skip deleter, shrink. */
        memcpy(out, slot, arr->size);
        memset(slot, 0, arr->size);     /* zero so array_remove won't delete */
    }

    return array_remove(arr, top);
}

/* -------------------------------------------------------------------------
 * array_stack_peek
 *
 * Returns a pointer to the top element without removing it.
 * The pointer is invalidated by any operation that reallocates the array.
 * Returns NULL on empty array.
 * ---------------------------------------------------------------------- */
void *array_stack_peek(ARRAY *arr)
{
    if (is_array_empty(arr)) return NULL;
    return array_get(arr, arr->length - 1);
}


/* =========================================================================
 * Queue operations  (FIFO — enqueue at tail, dequeue from head)
 * ======================================================================= */

/* -------------------------------------------------------------------------
 * array_queue_enqueue
 *
 * Copies data onto the tail of the queue (appends to the end).
 * Requires a copier.  Returns false on allocation failure or no copier.
 * ---------------------------------------------------------------------- */
bool array_queue_enqueue(ARRAY *arr, void *data)
{
    return array_append(arr, data);
}

/* -------------------------------------------------------------------------
 * array_queue_dequeue
 *
 * Copies the head element (index 0) into *out, then removes it,
 * shifting all remaining elements down by one.
 * Pass out=NULL to discard the value (deleter still called if set).
 * Returns false on empty array.
 * ---------------------------------------------------------------------- */
bool array_queue_dequeue(ARRAY *arr, void *out)
{
    if (is_array_empty(arr)) return false;

    void *slot = array_get(arr, 0);

    /* Copy value to caller's buffer before deletion */
    if (out)
    {
        /* Transfer ownership: copy raw bytes, skip deleter, shift & shrink. */
        memcpy(out, slot, arr->size);
        memset(slot, 0, arr->size);     /* zero so array_remove won't delete */
    }

    return array_remove(arr, 0);
}

/* -------------------------------------------------------------------------
 * array_queue_peek
 *
 * Returns a pointer to the head element (index 0) without removing it.
 * The pointer is invalidated by any operation that reallocates the array.
 * Returns NULL on empty array.
 * ---------------------------------------------------------------------- */
void *array_queue_peek(ARRAY *arr)
{
    if (is_array_empty(arr)) return NULL;
    return array_get(arr, 0);
}


/* =========================================================================
 * Functional operations
 * ======================================================================= */

/* -------------------------------------------------------------------------
 * array_map
 *
 * Produces a new ARRAY of the same length by calling map_fn for every
 * element.  The output array is created with out_size, out_deleter, and
 * out_copier so it can differ in type from the input (e.g. int → string).
 *
 * The mapper writes its result directly into the pre-zeroed destination
 * slot; the output array's copier is intentionally NOT invoked on top,
 * giving the mapper full control (important for types like strings where
 * the mapper may call str_dup itself).
 * ---------------------------------------------------------------------- */
ARRAY *array_map(ARRAY *arr,
                 size_t out_size,
                 ARRAY_DELETER_FUNC *out_deleter,
                 ARRAY_COPIER_FUNC  *out_copier,
                 ARRAY_MAP_FUNC     *map_fn)
{
    if (!arr || !arr->ptr || !map_fn) return NULL;
    if (out_size < 1)                 return NULL;

    ARRAY *out = new_arrayx(arr->length, out_size, out_deleter, out_copier);
    if (!out) return NULL;

    for (size_t i = 0; i < arr->length; i++)
        map_fn(array_get(out, i), array_get(arr, i));

    return out;
}

/* -------------------------------------------------------------------------
 * array_filter
 *
 * Produces a new ARRAY containing only elements for which filter_fn
 * returns true.  Elements are deep-copied via the input array's copier,
 * so a copier is required.
 *
 * A two-pass approach is used: first count matching elements so the
 * output array can be allocated at exactly the right size, then copy.
 * Returns a valid (possibly empty) ARRAY, or NULL on error.
 * ---------------------------------------------------------------------- */
ARRAY *array_filter(ARRAY *arr, ARRAY_FILTER_FUNC *filter_fn)
{
    if (!arr || !arr->ptr || !filter_fn) return NULL;
    if (!arr->copier)                    return NULL;

    /* ---- First pass: count matches ---- */
    size_t count = 0;
    for (size_t i = 0; i < arr->length; i++)
        if (filter_fn(array_get(arr, i))) count++;

    /* Allocate output — even zero-length is a valid empty array */
    if (count == 0)
        return new_arrayx(1, arr->size, arr->deleter, arr->copier);
        /* length=1 minimum for new_arrayx; caller can check via arr->length */

    ARRAY *out = new_arrayx(count, arr->size, arr->deleter, arr->copier);
    if (!out) return NULL;

    /* ---- Second pass: copy matching elements ---- */
    size_t idx = 0;
    for (size_t i = 0; i < arr->length; i++)
    {
        void *elem = array_get(arr, i);
        if (filter_fn(elem))
            (*arr->copier)(array_get(out, idx++), elem);
    }

    return out;
}

/* -------------------------------------------------------------------------
 * array_reduce
 *
 * Folds every element into the caller-managed accumulator *acc by calling:
 *     reduce_fn(acc, element)
 * The accumulator is neither allocated nor freed here; the caller
 * initialises it to the desired seed value before calling.
 * ---------------------------------------------------------------------- */
bool array_reduce(ARRAY *arr, void *acc, ARRAY_REDUCE_FUNC *reduce_fn)
{
    if (!arr || !arr->ptr || !acc || !reduce_fn) return false;

    for (size_t i = 0; i < arr->length; i++)
        reduce_fn(acc, array_get(arr, i));

    return true;
}

/* -------------------------------------------------------------------------
 * array_foreach
 *
 * Calls foreach_fn(element_ptr, index, userdata) for every element in
 * order.  element_ptr is a direct reference into the array's storage;
 * in-place modifications are visible immediately.
 * userdata is passed through unchanged and may be NULL.
 * ---------------------------------------------------------------------- */
bool array_foreach(ARRAY *arr, ARRAY_FOREACH_FUNC *foreach_fn, void *userdata)
{
    if (!arr || !arr->ptr || !foreach_fn) return false;

    for (size_t i = 0; i < arr->length; i++)
        foreach_fn(array_get(arr, i), i, userdata);

    return true;
}

/* =========================================================================
 * Search, slice, concat, sort
 * ======================================================================= */


/*
 * Merges two arrays creating a new array.  The input arrays are not freed.  Empty arrays may not be concatenated.  Both input arrays must have the same element size.  Both input arrays must have the same copiers and deleters.
 *
 * @param a First ARRAY to merge
 * @param b Second ARRAY to merge
 * 
 * @return New array containing the deep copied elements from both arrays, inherited the copier and deleter from the first array.  NULL is returned on error.
 * 
 */
ARRAY *array_concat(ARRAY *a, ARRAY *b)
{
    if (!a || !b)               return NULL;
    if (a->size != b->size)     return NULL;    // First possible sanity check to make sure both arrays are compatible
    if (!a->copier)             return NULL;
    if (a->copier != b->copier) return NULL;    // Second possible sanity check to make sure both arrays are compatible
    if (a->deleter != b->deleter)   return NULL;// Third possible sanity check to make sure both arrays are compatible

    size_t total = a->length + b->length;
    ARRAY *out = new_arrayx(total, a->size, a->deleter, a->copier);
    if (!out) return NULL;

    if (a->ptr)
    {
        for (size_t i = 0; i < a->length; i++)
            (*a->copier)(array_get(out, i), array_get(a, i));
    }

    if (b->ptr)
    {
        for (size_t i = 0; i < b->length; i++)
            (*a->copier)(array_get(out, a->length + i), array_get(b, i));
    }

    return out;
}

/*
 * Cuts a slice of the array.  Array must have copier assigned.
 *
 * @param arr ARRAY to slice.
 * @param start Starting index.
 * @param end Index after slice, clamped to ARRAY length.
 * 
 * @return New array containing slice, or NULL no copier or if nothing to slice.
 */
ARRAY *array_slice(ARRAY *arr, size_t start, size_t end)
{
    if (!arr || !arr->ptr)  return NULL;
    if (!arr->copier)       return NULL;

    /* Clamp end */
    if (end > arr->length) end = arr->length;
    if (start >= end)      return NULL;

    size_t count = end - start;
    ARRAY *out = new_arrayx(count, arr->size, arr->deleter, arr->copier);
    if (!out) return NULL;

    for (size_t i = 0; i < count; i++)
        (*arr->copier)(array_get(out, i), array_get(arr, start + i));

    return out;
}

/*
 * Linearly searches the array for the first instance of the given criteria.
 *
 * @param arr ARRAY to search.
 * @param find_fn Callback used to check if the current element matches the criteria
 * @param userdata Userdata used in the find_fn callback.
 * 
 * @return The index of the first match, or (size_t)-1.
 */
size_t array_find(ARRAY *arr, ARRAY_FIND_FUNC *find_fn, void *userdata)
{
    if (!arr || !arr->ptr || !find_fn) return (size_t)-1;

    for (size_t i = 0; i < arr->length; i++)
        if (find_fn(array_get(arr, i), userdata))
            return i;

    return (size_t)-1;
}

/*
 * Finds all indices that passes the find function, using a linear search.
 * 
 * @param arr ARRAY to search.
 * @param find_fn Callback used to check if the current element matches the criteria
 * @param userdata Userdata used in the find_fn callback.
 * 
 * @return A plain size_t(int) ARRAY without any copier/deleter containing the matching indices.
 * An empty array (length zero) indicates nothing was found.
 */
ARRAY *array_find_all(ARRAY *arr, ARRAY_FIND_FUNC *find_fn, void *userdata)
{
    if (!arr || !arr->ptr || !find_fn) return NULL;

    /* First pass: count matches */
    size_t count = 0;
    for (size_t i = 0; i < arr->length; i++)
        if (find_fn(array_get(arr, i), userdata)) count++;

    ARRAY *out = new_arrayx(count, sizeof(size_t), NULL, NULL);
    if (!out) return NULL;

    if (count == 0)
    {
        return out;
    }

    /* Second pass: record indices */
    size_t idx = 0;
    for (size_t i = 0; i < arr->length; i++)
    {
        if (find_fn(array_get(arr, i), userdata))
        {
            size_t *slot = (size_t *)array_get(out, idx++);
            *slot = i;
        }
    }

    return out;
}

/*
 * In-place sort via qsort.  The comparator receives direct pointers into the array's storage (same contract as the standard qsort comparator).
 *
 * @param arr ARRAY to sort
 * @param cmp Comparator function (same contract as used in qsort)
 *
 * @returns TRUE on success. FALSE on failure.
 */
bool array_sort(ARRAY *arr, ARRAY_COMPARE_FUNC *cmp)
{
    if (!arr || !arr->ptr || !cmp) return false;
    if (arr->length < 2)           return true;  /* already sorted */

    qsort(arr->ptr, arr->length, arr->size, cmp);
    return true;
}

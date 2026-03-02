#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "utf8.h"

// UTF-8 encoding specs
#define UTF8_4BYTES         (0xF0)      // Bit pattern indicating 4 byte code
#define UTF8_4BYTES_CHECK   (0xF8)      // Mask to check for 4 byte encoding
#define UTF8_3BYTES         (0xE0)      // Bit pattern indicating 3 byte code
#define UTF8_3BYTES_CHECK   (0xF0)      // Mask to check for 3 byte encoding
#define UTF8_2BYTES         (0xC0)      // Bit pattern indicating 2 byte code
#define UTF8_2BYTES_CHECK   (0xE0)      // Mask to check for 2 byte encoding
#define UTF8_CONTINUE       (0x80)      // Bit pattern indicating continuation bytes for multibyte code
#define UTF8_CONTINUE_CHECK (0xC0)      // Mask to check for continue byte encoding
#define UTF8_ASCII_MASK     (0x7F)      // Mask for data bits in single byte code (ASCII)

#define UTF8_BYTE4_SHIFT    (18)        // How much to shift code to get for masking byte 4 of code 
#define UTF8_BYTE4_MASK     (0x07)      // Mask for data bits in leading 4 byte code
#define UTF8_BYTE3_SHIFT    (12)        // How much to shift code to get for masking byte 3 of code 
#define UTF8_BYTE3_MASK     (0x0F)      // Mask for data bits in leading 3 byte code
#define UTF8_BYTE2_SHIFT    (6)         // How much to shift code to get for masking byte 2 of code 
#define UTF8_BYTE2_MASK     (0x1F)      // Mask for data bits in leading 2 byte code
#define UTF8_CONTINUE_MASK  (0x3F)      // Mask for data bits in leading continuation byte code

#define UTF8_4BYTE_END      (0x10FFFF)  // Last Unicode value for 4 byte codes (current upper limit)
#define UTF8_4BYTE_START    (0x10000)   // Starting Unicode needing 4 byte codes
#define UTF8_3BYTE_START    (0x800)     // Starting Unicode needing 3 byte codes
#define UTF8_2BYTE_START    (0x80)      // Starting Unicode needing 2 byte codes


#define IS_4BYTE(s)        (((((s)[0])&UTF8_4BYTES_CHECK) == UTF8_4BYTES) && \
                            ((((s)[1])&UTF8_CONTINUE_CHECK) == UTF8_CONTINUE) && \
                            ((((s)[2])&UTF8_CONTINUE_CHECK) == UTF8_CONTINUE) && \
                            ((((s)[3])&UTF8_CONTINUE_CHECK) == UTF8_CONTINUE))

#define IS_3BYTE(s)        (((((s)[0])&UTF8_3BYTES_CHECK) == UTF8_3BYTES) && \
                            ((((s)[1])&UTF8_CONTINUE_CHECK) == UTF8_CONTINUE) && \
                            ((((s)[2])&UTF8_CONTINUE_CHECK) == UTF8_CONTINUE))

#define IS_2BYTE(s)        (((((s)[0])&UTF8_2BYTES_CHECK) == UTF8_2BYTES) && \
                            ((((s)[1])&UTF8_CONTINUE_CHECK) == UTF8_CONTINUE))

#define UTF8_REPLACEMENT        (0xFFFD)    // Standard replacement character when dealing with invalid encodings

// Determines the number of UTF8 bytes needed for the unicode character
size_t utf8_bytes(unichar_t ch)
{
	if (ch >= UTF8_4BYTE_START) return 4;
	if (ch >= UTF8_3BYTE_START) return 3;
	if (ch >= UTF8_2BYTE_START) return 2;
	return 1;
}

// Converts the unicode into its UTF-8 encoded string
char *utf8_getbytes(unichar_t ch)
{
	static char bytes[4][5];
	static int i = 0;

	if (++i > 3) i = 0;
	register char *b = &bytes[i][0];

    if (ch > UTF8_4BYTE_END)
    {
        ch = UTF8_REPLACEMENT;
    }

	if (ch >= UTF8_4BYTE_START)
	{
		b[0] = (char)((ch >> UTF8_BYTE4_SHIFT) & UTF8_BYTE4_MASK) | UTF8_4BYTES;
		b[1] = (char)((ch >> UTF8_BYTE3_SHIFT) & UTF8_CONTINUE_MASK) | UTF8_CONTINUE;
		b[2] = (char)((ch >> UTF8_BYTE2_SHIFT) & UTF8_CONTINUE_MASK) | UTF8_CONTINUE;
		b[3] = (char)(ch & UTF8_CONTINUE_MASK) | UTF8_CONTINUE;
		b[4] = 0;
	}
	else if (ch >= UTF8_3BYTE_START)
	{
		b[0] = (char)((ch >> UTF8_BYTE3_SHIFT) & UTF8_BYTE3_MASK) | UTF8_3BYTES;
		b[1] = (char)((ch >> UTF8_BYTE2_SHIFT) & UTF8_CONTINUE_MASK) | UTF8_CONTINUE;
		b[2] = (char)(ch & UTF8_CONTINUE_MASK) | UTF8_CONTINUE;
		b[3] = 0;
	}
	else if (ch >= UTF8_2BYTE_START)
	{
		b[0] = (char)((ch >> UTF8_BYTE2_SHIFT) & UTF8_BYTE2_MASK) | UTF8_2BYTES;
		b[1] = (char)(ch & UTF8_CONTINUE_MASK) | UTF8_CONTINUE;
		b[2] = 0;
	}
	else    // 1 Byte ASCII
	{
		b[0] = (char)(ch & UTF8_ASCII_MASK);
		b[1] = 0;
	}

	return b;
}

char *utf8_nextchar(register const char *str)
{
	// 2-byte UTF-8
    if (IS_2BYTE(str)) return (char *)str+2;

	// 3-byte UTF-8
    if (IS_3BYTE(str)) return (char *)str+3;

	// 4-byte UTF-8
    if (IS_4BYTE(str)) return (char *)str+4;

	// ASCII
	return (char *)str+1;
}

char *utf8_skip(register const char *str, register size_t len)
{
	for(;*str && len > 0; len--, str = utf8_nextchar(str));
	return (char *)str;
}

unichar_t utf8_getchar(const char *str)
{
    unichar_t code = UTF8_REPLACEMENT;
	// 2-byte UTF-8
    if (IS_2BYTE(str))
    {
        code = ((((unichar_t)*str) & UTF8_BYTE2_MASK) << UTF8_BYTE2_SHIFT) |
               ((unichar_t)*(str+1) & UTF8_CONTINUE_MASK);
        if (code < UTF8_2BYTE_START)    // Overlong encoding
            code = UTF8_REPLACEMENT;
    }
    
	// 3-byte UTF-8
    else if (IS_3BYTE(str))
    {
        code = ((((unichar_t)*str) & UTF8_BYTE3_MASK) << UTF8_BYTE3_SHIFT) |
               ((((unichar_t)*(str+1)) & UTF8_CONTINUE_MASK) << UTF8_BYTE2_SHIFT) |
               ((unichar_t)*(str+2) & UTF8_CONTINUE_MASK);
        if (code < UTF8_3BYTE_START)    // Overlong encoding
            code = UTF8_REPLACEMENT;
    }

	// 4-byte UTF-8
    else if (IS_4BYTE(str))
    {
        code = ((((unichar_t)*str) & UTF8_BYTE4_MASK) << UTF8_BYTE4_SHIFT) |
               ((((unichar_t)*(str+1)) & UTF8_CONTINUE_MASK) << UTF8_BYTE3_SHIFT) |
               ((((unichar_t)*(str+2)) & UTF8_CONTINUE_MASK) << UTF8_BYTE2_SHIFT) |
               ((unichar_t)*(str+3) & UTF8_CONTINUE_MASK);
        if (code < UTF8_4BYTE_START)    // Overlong encoding
            code = UTF8_REPLACEMENT;
        else if (code > UTF8_4BYTE_END) // Invalid code
            code = UTF8_REPLACEMENT;
    }
	// ASCII
    else
        code = (((unichar_t)*str) & UTF8_ASCII_MASK);

    return code;
}

// Determines if the string contains any UTF-8 character sequences.
bool is_utf8_string(register const char *str)
{
    bool utf8 = false;
    while(*str)
    {
        if (IS_4BYTE(str))
        {
            str+=4;
            utf8 = true;
        }
        else if (IS_3BYTE(str))
        {
            str+=3;
            utf8 = true;
        }
        else if (IS_2BYTE(str))
        {
            str+=2;
            utf8 = true;
        }

        // ASCII and bit patterns not in UTF-8 are ignored
    }

    return utf8;
}

// UTF-8 aware strlen
size_t utf8_strlen(register const char *str)
{
	if (!str) return 0;

	register size_t len = 0;
	while(*str)
	{
		len++;
		str = utf8_nextchar(str);
	}

	return len;
}

/** Determines if string A equals, case-insensitively, to string B, being UTF-8 aware.
 * 
 * @param astr String A
 * @param bstr String B
 * 
 * @return 0 - if string A equals (case-insensitive) to string B
 */
int utf8_str_cmp(register const char *astr, register const char *bstr)
{
	unichar_t ch;
	if (astr == NULL) return -1;

	if (bstr == NULL) return 1;

	for (; *astr || *bstr; astr = utf8_nextchar(astr), bstr = utf8_nextchar(bstr)) {
		unichar_t ach = utf8_getchar(astr);
		unichar_t bch = utf8_getchar(bstr);
		if ((ch = (LOWER(ach) - LOWER(bch))))
			return (int)ch;
	}

	return 0;
}

/** Determines if string B begins with string A, being UTF-8 aware.
 * 
 * @param astr Target string
 * @param bstr String to scan
 * 
 * @return FALSE - if string B begins with string A
 */
bool utf8_str_prefix(const char *astr, const char *bstr)
{
    if (astr == NULL)
    {
		return true;
    }

    if (bstr == NULL)
    {
		return true;
    }

	// Empty strings should *never* prefix another string
	if (!*astr) return true;

    for (; *astr; astr = utf8_nextchar(astr), bstr = utf8_nextchar(bstr))
    {
		unichar_t ach = utf8_getchar(astr);
		unichar_t bch = utf8_getchar(bstr);
		if (LOWER(ach) != LOWER(bch))
			return true;
    }

    return false;
}

/** Determines if string B contains string A, being UTF-8 aware.
 * 
 * @param astr Target string
 * @param bstr String to scan
 * 
 * @return FALSE - if string B contains string A
 */
bool utf8_str_infix(const char *astr, const char *bstr)
{
    size_t sstr1;
    size_t sstr2;
    int ichar;
    unichar_t c0;

	c0 = utf8_getchar(astr);
	c0 = LOWER(c0);
    if (!c0)
		return false;

    sstr1 = utf8_strlen(astr);
    sstr2 = utf8_strlen(bstr);

    for (ichar = 0; ichar <= (sstr2 - sstr1); ichar++, bstr = utf8_nextchar(bstr))
    {
		unichar_t bch = utf8_getchar(bstr);
		if ((c0 == LOWER(bch)) &&
			!utf8_str_prefix(astr,bstr))
		    return false;
    }

    return true;
}

/** Determines if string B ends with string A, being UTF-8 aware.
 * 
 * @param astr Target string
 * @param bstr String to scan
 * 
 * @return FALSE - if string B ends with string A
 */
bool utf8_str_suffix(const char *astr, const char *bstr)
{
    size_t sstr1;
    size_t sstr2;

    sstr1 = utf8_strlen(astr);
    sstr2 = utf8_strlen(bstr);

	if (sstr1 <= sstr2)
	{
		bstr = utf8_skip(bstr, sstr2 - sstr1);

		if (!utf8_str_cmp(astr, bstr))
			return false;
	}

	return true;
}

/** Determines number of byte needed to skip the given number of UTF-8 encoded characters.
 * 
 * @param str String to scan.
 * @param len Number of UTF-8 characters to skip.  Returns number of characters skipped.
 * @param bytes Stores the number of bytes skipped.
 * 
 * @return false - if there was an invalid UTF-8 encoding.
 */
bool utf8_skip_len(register const char *str, size_t *len, size_t *bytes)
{
    *bytes = 0;

    const char *start = str;
    size_t n;
    for(n = 0; *str && n < *len; str = utf8_nextchar(str))
    {
        unichar_t ch = utf8_getchar(str);
        if (ch > 0)
            n++;
        else
        {
            *bytes = -1;
            *len = n;
            return false;
        }
    }

    *len = n;                           // Number of characters skipped
    *bytes = (size_t)(str - start);     // Byte len for skipped characters
    return true;
}

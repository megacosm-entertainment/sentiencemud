#ifndef __UTF8_H__
#define __UTF8_H__

typedef int unichar_t;     // Used to store the Unicode value for the character from a UTF-8 encoding

char *utf8_put(char *str, unichar_t ch);
size_t utf8_bytes(unichar_t ch);
char *utf8_getbytes(unichar_t ch);
char *utf8_prevchar(const char *str);
char *utf8_nextchar(const char *str);
char *utf8_skip(const char *str, size_t len);
unichar_t utf8_getchar(const char *str);
bool is_utf8_string(const char *str);
size_t utf8_strlen(const char *str);
int utf8_str_cmp(const char *astr, const char *bstr);
bool utf8_str_prefix(const char *astr, const char *bstr);
bool utf8_str_infix(const char *astr, const char *bstr);
bool utf8_str_suffix(const char *astr, const char *bstr);
bool utf8_skip_len(const char *str, size_t *len, size_t *bytes);

unichar_t utf8_toupper(unichar_t cp);
unichar_t utf8_tolower(unichar_t cp);

#endif
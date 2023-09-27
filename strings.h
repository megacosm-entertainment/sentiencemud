///////////////////////////////////////////////////////////////////////////////
// strings.h
// 
// Helper functions for string handling.
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef ROM__STRINGS_H
#define ROM__STRINGS_H

// All strings in ROM are 7-bit POSIX Locale, with the sign bit reserved for 
// special telnet characters. These macros keep 'ctype.h' functions copacetic
// with char* strings under -Wall.

#ifdef __CYGWIN__
    #define ISALPHA(c) isalpha((unsigned char)c)
    #define ISDIGIT(c) isdigit((unsigned char)c)
    #define ISPRINT(c) isprint((unsigned char)c)
    #define ISSPACE(c) isspace((unsigned char)c)
    #define ISUPPER(c) isupper((unsigned char)c)
    #define ISASCII(c) isascii((unsigned char)c)
    #define ISALNUM(c) isalnum((unsigned char)c)

#else
    #define ISALPHA(c) isalpha(c)
    #define ISDIGIT(c) isdigit(c)
    #define ISPRINT(c) isprint(c)
    #define ISSPACE(c) isspace(c)
    #define ISUPPER(c) isupper(c)
    #define ISASCII(c) isascii(c)
    #define ISALNUM(c) isalnum(c)
#endif

#endif // !ROM__STRINGS_H
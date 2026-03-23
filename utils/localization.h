#ifndef __LOCALIZATION_H__
#define __LOCALIZATION_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include "array.h"
#include "utf8.h"
#include "strdict.h"

typedef struct language_translation_type TRANSLATION;
typedef struct language_localization_type LOCALIZATION_DATA;

typedef enum localization_error_enum {
    LOC_OK                      =  0,
    LOC_ERR_NULL_INPUT          = -1,
    LOC_ERR_INVALID_UTF8        = -2,
    LOC_ERR_FORBIDDEN_CODEPOINT = -3,
    LOC_ERR_UNSUPPORTED_LANG    = -4,
    LOC_ERR_BUFFER_TOO_SMALL    = -5,
    LOC_ERR_NO_LANG_SET         = -6,
    LOC_ERR_ALLOC               = -7,
    LOC_ERR_EMPTY_RESULT        = -8
} LOCALIZATION_ERROR;

struct language_translation_type {
    char *phrase;               // e.g: "editor.label.name"
    char *translation;          // e.g: "Name"
};

struct language_localization_type {
    LOCALIZATION_DATA *next;

    char *filename;         // eg "data/system/localizations/english.json"
    char *name;             // eg "English"
    char *iso_name;         // eg "en"

    CODEPOINT_RANGE *codepoint_ranges;
    size_t codepoint_ranges_count;
    char **filter_words;                // NULL string terminated array
    STRING_DICT *translations;          // Translation dictionary
};

LOCALIZATION_DATA *new_localization_data(void);
void free_localization_data(LOCALIZATION_DATA *loc);
LOCALIZATION_DATA *localization_lookup(const char *iso_name);
void localization_init_default(void);
void localization_init_allowed_languages(void);

LOCALIZATION_ERROR localization_validate_string(const char *str, size_t *bad_position, size_t *bad_offset);
LOCALIZATION_ERROR localization_extract_keywords(const char *str, char *output, size_t max_output, size_t *out_count, size_t *output_needed);
LOCALIZATION_ERROR localization_short_to_keywords(const char *str, char **out_keywords, char **out_invalid);


const char *localization_error_string(LOCALIZATION_ERROR err);

const char *localization_translate(LOCALIZATION_DATA *loc, const char *input);
const char *localization_translatef(LOCALIZATION_DATA *loc, const char *input, ...);

// Macros to help with typing out the translations
#define LT(d,i)         localization_translate(((d) ? (d)->lang : default_localization), (i))
#define LTF(d,i,...)    localization_translatef(((d) ? (d)->lang : default_localization), (i), __VA_ARGS__)

#define LTNL(d,i)       formatf("%s\n\r", localization_translate(((d) ? (d)->lang : default_localization), (i)))
#define LTFNL(d,i,...)  formatf("%s\n\r", localization_translatef(((d) ? (d)->lang : default_localization), (i), __VA_ARGS__))

#define LTD(i)          localization_translate(default_localization, (i))
#define LTDF(i,...)     localization_translatef(default_localization, (i), __VA_ARGS__)

#define LTDNL(i)        formatf("%s\n\r", localization_translate(default_localization, (i)))
#define LTDFNL(i,...)   formatf("%s\n\r", localization_translatef(default_localization, (i), __VA_ARGS__))

#define LTMENU(d,i,k)   formatf("{G" k "{x) %s\n\r", localization_translate(((d) ? (d)->lang : default_localization), (i)))

#ifdef MUD_DEBUG
void localization_dump_translations(LOCALIZATION_DATA *loc);
#endif

extern LOCALIZATION_DATA *default_localization;

#endif /* __LOCALIZATION_H__ */
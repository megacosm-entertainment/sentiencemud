#include "localization.h"
#include "buffer.h"
#include "utf8.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>   /* tolower, ispunct, isspace */

extern char str_empty[1];

uint32_t localizations_count = 0;
LOCALIZATION_DATA *localizations_list = NULL;
LOCALIZATION_DATA *default_localization = NULL;
LOCALIZATION_DATA **localizations_allowed = NULL;

LOCALIZATION_DATA *new_localization_data(void)
{
    LOCALIZATION_DATA *loc = alloc_perm(sizeof(LOCALIZATION_DATA));

    if (loc)
    {
        loc->filename = str_empty;
        loc->name = str_empty;
        loc->iso_name = str_empty;
    }

    return loc;
}

void free_localization_data(LOCALIZATION_DATA *loc)
{
    if (loc)
    {
        free_string(loc->filename);
        free_string(loc->name);
        free_string(loc->iso_name);

        if (loc->codepoint_ranges) free(loc->codepoint_ranges);
        if (loc->filter_words)
        {
            for(uint32_t i = 0;loc->filter_words[i];i++)
                free_string(loc->filter_words[i]);
            free(loc->filter_words);
        }

        if (loc->translations)
        {
            for(uint32_t i = 0;loc->translations[i].phrase;i++)
            {
                free_string(loc->translations[i].phrase);
                free_string(loc->translations[i].translation);
            }
            free(loc->translations);
        }

        free(loc);
    }
}

LOCALIZATION_DATA *localization_lookup(const char *iso_name)
{
    if (IS_NULLSTR(iso_name)) return NULL;

    LOCALIZATION_DATA *cur = localizations_list;
    while(cur)
    {
        if (!str_cmp(iso_name, cur->iso_name))
            return cur;

        cur = cur->next;
    }

    return NULL;
}

void localization_init_default(void)
{
    LOCALIZATION_DATA *loc = localization_lookup(game_settings.default_language);
    if (!loc && strcmp(game_settings.default_language, "en")) { // Only fallback if the default is already not "en"
        pbugf(LOG_WARN, "Selected default localization '%s' not found, falling back to 'en'.", game_settings.default_language);
        loc = localization_lookup("en");    // Attempt some kind of fallback?
    }
    if (!loc) {
        if (localizations_list)
        {
            if (!strcmp(game_settings.default_language, "en"))
                pbugf(LOG_WARN, "Selected default localization '%s' not found, using the first one '%s'.", game_settings.default_language, localizations_list->iso_name);
            else
                pbugf(LOG_WARN, "Selected default localization '%s' not found without 'en' as fallback, using the first one '%s'.", game_settings.default_language, localizations_list->iso_name);
        } else {
            // Should never really *get* here since the game will not run without any localizations loaded.
            perrf(LOG_ERROR, "Selected default localization '%s' not found, with no localizations to fallback.", game_settings.default_language);
        }
        loc = localizations_list;           // Just use the first one.
    }
    default_localization = loc;
}

void localization_init_allowed_languages(void)
{
    if (localizations_allowed)
    {
        free(localizations_allowed);
        localizations_allowed = NULL;
    }

    if (!is_array_empty(game_settings.allowed_languages))
    {
        localizations_allowed = calloc(game_settings.allowed_languages->length + 1, sizeof(LOCALIZATION_DATA *));
        if (localizations_allowed)
        {
            size_t index = 0;
            for(size_t i = 0; i < game_settings.allowed_languages->length;i++)
            {
                void *slot = array_get(game_settings.allowed_languages, i);
                const char *iso_name = *((char **)slot);
                if (iso_name)
                {
                    LOCALIZATION_DATA *loc = localization_lookup(iso_name);
                    if (loc)
                    {
                        localizations_allowed[index++] = loc;
                    }
                }
            }
        }
    }
}


void free_localizations(void)
{
    localizations_list = NULL;
    localizations_count = 0;
    default_localization = NULL;
}

/************************************************************************
 * Validation Functionality *
 ************************************************************************/

static bool is_universal_codepoint(unichar_t cp)
{
    return (cp == '\t' || cp == '\n' || cp == '\r' || cp == ' ');
}

bool localization_codepoint_allowed(LOCALIZATION_DATA *loc, register unichar_t cp)
{
    if (!loc || !loc->codepoint_ranges)
        return false;

    if (is_universal_codepoint(cp))
        return true;

    register const CODEPOINT_RANGE *ranges = loc->codepoint_ranges;
    register size_t lo = 0, hi = loc->codepoint_ranges_count;

    while(lo < hi) {
        size_t mid = lo + (hi - lo) / 2;    // Instead of doing (hi + lo) / 2 to prevent overflow errors
        if (cp < ranges[mid].lo)
            hi = mid;
        else if (cp > ranges[mid].hi)
            lo = mid + 1;
        else {
            return true;        // cp is within range[mid].
        }
    }

    return false;
}

/**
 * Validates whether the characters in the string belong to the allowed languages
 * 
 * Indicates the first instance of a forbidden character, either by being invalid UTF8
 * or not in the specified allowed language codepoints.
 * 
 * @param str Input string
 * @param bad_position Optional reference to hold the character position in the input string where the error occured.
 * @param bad_offset Optional reference to hold the offset in the input string where the error occured.
 * 
 * @returns LOC_OK on success, anything else on error.  See LOCALIZATION_ERROR
 */
LOCALIZATION_ERROR localization_validate_string(const char *str, size_t *bad_position, size_t *bad_offset)
{
    if (!str) return LOC_ERR_NULL_INPUT;

    if (!localizations_allowed || !localizations_allowed[0]) return LOC_ERR_NO_LANG_SET;

    const char *p = str;
    size_t pos = 0;
    while(*p) {
        const char *before = p;
        unichar_t cp = utf8_getchar(p);
        const char *next = utf8_nextchar(p);

        if (!next || cp < 0)
        {
            if (bad_position) *bad_position = pos;
            if (bad_offset) *bad_offset = (size_t)(before - str);
            return LOC_ERR_INVALID_UTF8;
        }

        bool allowed = false;

        for(size_t i = 0; !allowed && localizations_allowed[i]; i++)
            allowed = localization_codepoint_allowed(localizations_allowed[i], cp);
        
        if (!allowed)
        {
            if (bad_position) *bad_position = pos;
            if (bad_offset) *bad_offset = (size_t)(before - str);
            return LOC_ERR_FORBIDDEN_CODEPOINT;
        }

        p = next;
        pos++;
    }

    return LOC_OK;
}

/* Strip leading and trailing ASCII punctuation from a token in place.
 * Returns pointer to the (possibly advanced) start; modifies length. */
static char *strip_punct(register char *tok, register size_t *len)
{
    /* Strip trailing */
    while (*len > 0 && ispunct((unsigned char)tok[*len - 1]))
        (*len)--;
    tok[*len] = '\0';
 
    /* Strip leading */
    while (*len > 0 && ispunct((unsigned char)*tok)) {
        tok++;
        (*len)--;
    }
    return tok;
}

/* ASCII lowercase fold in place */
static inline void ascii_lower(register char *s)
{
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}

static bool localization_is_filter_word(const char *word)
{
    if (!word || !localizations_allowed || !localizations_allowed[0]) return false;

    // Instead of intersection as Claude suggested, I'm doing UNION,
    //  so any of the allowed languages' filter words are removed
    for(size_t i = 0; localizations_allowed[i]; i++) {
        LOCALIZATION_DATA *loc = localizations_allowed[i];
        for(size_t j = 0; loc->filter_words[j]; j++) {
            if (!strcmp(loc->filter_words[j], word))
                return true;
        }
    }

    return false;
}

/**
 * Extracts valid keywords from the input string.
 * 
 * Removes any filter words from the allowed localizations, leaving the keywords behind.
 * If no output is specified, the function can merely determine how much buffer space is
 * needed to hold all of the keywords.  Keywords are separated by the NUL ('\0') character.
 * 
 * @param str Input string
 * @param output Optional output buffer to store keywords.
 * @param max_output Maximum buffer size allowed.
 * @param out_count Optional reference to hold the number of keywords in the buffer.
 * @param output_needed Optional reference to hold the output buffer size needed.
 * 
 * @returns LOC_OK on success, anything else on error.  See LOCALIZATION_ERROR
 */
LOCALIZATION_ERROR localization_extract_keywords(const char *str,
                                             char *output,
                                             size_t max_output,
                                             size_t *out_count,
                                             size_t *output_needed)
{
    if (!str) return LOC_ERR_NULL_INPUT;
    if (!localizations_allowed || !localizations_allowed[0]) return LOC_ERR_NO_LANG_SET;

    size_t count = 0;
    size_t needed = 0;
    size_t written = 0;
    LOCALIZATION_ERROR status = LOC_OK;

    // We tokenise into a small working buffer; 256 bytes covers all
    // reasonable single-word tokens including multi-byte sequences.
    char token_buf[512];
 
    const char *p = str;
    while(*p) {
        // Skip whitespace
        while(*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        // Collect non-whitespace into token buffer
        size_t tlen = 0;
        while(*p && !isspace((unsigned char)*p)) {
            if (tlen < (sizeof(token_buf) - 1))
                token_buf[tlen++] = *p;
            p++;
        }
        token_buf[tlen] = '\0';

        // Strip punctuation
        char *tok = strip_punct(token_buf, &tlen);
        if (tlen == 0) continue;

        ascii_lower(tok);

        // If is a word that needs to be filtered, remove it
        if (localization_is_filter_word(tok))
            continue;
        
        size_t emit_len = tlen + 1; // Token length + NULL
        needed += emit_len;
        count++;

        if (output) {
            if (written + emit_len > max_output) {
                status = LOC_ERR_BUFFER_TOO_SMALL;
                // Keep calculating what is actually needed
            } else {
                memcpy(output + written, tok, emit_len);
                written += emit_len;
            }
        }
    }

    if (out_count) *out_count = count;
    if (output_needed) *output_needed = needed; 

    return status;
}

/**
 * Converts a short description into keywords
 * 
 * Extracts keywords from the given short description, then proceeds to validate
 * each word to make sure they can be in keyword fields, provided the UTF8 restriction
 * game setting is TRUE.  Invalid words are appended into out_invalid to give proper
 * feedback.
 * 
 * @param str Input short descriptiong
 * @param out_keywords Reference to hold heap-allocated keyword string; it could be NULL (caller takes ownership)
 * @param out_invalid Optional reference to hold heap-allocated string containing any invalid keywords; it could be NULL. (caller takes ownership)
 * 
 * @returns LOC_OK on success, anything else on error.  See LOCALIZATION_ERROR
 */
LOCALIZATION_ERROR localization_short_to_keywords(const char *str, char **out_keywords, char **out_invalid)
{
    if (!str || !out_keywords) return LOC_ERR_NULL_INPUT;

    char *temp_str = nocolour(str);
    if (!temp_str) return LOC_ERR_ALLOC;

    size_t needed;

    LOCALIZATION_ERROR status = localization_extract_keywords(temp_str, NULL, 0, NULL, &needed);
    if (status != LOC_OK)
    {
        free_string(temp_str);
        return status;
    }

    char *keywords = malloc(needed);
    if (!keywords)
    {
        free_string(temp_str);
        return LOC_ERR_ALLOC;
    }

    size_t count;
    status = localization_extract_keywords(temp_str, keywords, needed, &count, NULL);
    if (status != LOC_OK)
    {
        free_string(temp_str);
        free(keywords);
        return status;
    }
    if (count == 0) {
        if (out_keywords) *out_keywords = NULL;
        if (out_invalid) *out_invalid = NULL;
        free_string(temp_str);
        free(keywords);
        return LOC_ERR_EMPTY_RESULT;
    }

    char *valid = calloc(1, needed);
    if (!valid)
    {
        free_string(temp_str);
        free(keywords);
        return LOC_ERR_ALLOC;
    }

    char *invalid = calloc(1, needed);
    if (!invalid)
    {
        free_string(temp_str);
        free(keywords);
        free(valid);
        return LOC_ERR_ALLOC;
    }

    // Iterate over the extracted keywords to see if they are validate strings
    char *p = keywords;
    status = LOC_OK;
    for(size_t i = 0; i < count; count++, p += strlen(p) + 1)
    {
        LOCALIZATION_ERROR err = localization_validate_string(p, NULL, NULL);

        if (err == LOC_OK) {
            if (*valid) strcat(valid, " ");
            strcat(valid, p);
        } else if (out_invalid) {
            if (*invalid) strcat(invalid, " ");
            strcat(invalid, p);
        }
    }
    // The buffers are pre-zero'd, so no need to NUL terminate

    *out_keywords = valid;
    if (out_invalid)
        *out_invalid = invalid;
    else
        free(invalid);      // Not cared about
    free_string(temp_str);
    free(keywords);
    return LOC_OK;
}


/************************************************************************
 * Translations Functionality                                           *
 ************************************************************************/

static const char *_translate_phrase(LOCALIZATION_DATA *loc, register const char *input)
{
    if(loc && loc->translations) {
        
        for(register TRANSLATION *trans = loc->translations; trans->phrase; trans++)
        {
            if (!str_cmp(input, trans->phrase))
                return trans->translation;
        }
    }

    return NULL;
}

/**
 * localization_translate - Attempts to translate the given input string into its localized form
 *
 * If the given localization does not contain the given input key, it will fallback
 * to the default localization (if different).
 *
 * @param loc         Localization to use
 * @param input       Input key string to translate
 * @return            The translated output, or the raw input if failed.
 */
const char *localization_translate(LOCALIZATION_DATA *loc, const char *input)
{
    const char *output;
    // Attempt to find the string in the given localization
    output = _translate_phrase(loc, input);
    if (output) return output;

    // If failed, search the default localization (if not the given localization)
    if (loc != default_localization)
    {
        output = _translate_phrase(default_localization, input);
        if (output) return output;
    }

    // If failed, return the raw input
    return input;
}


const char *localization_error_string(LOCALIZATION_ERROR err)
{
    switch (err) {
        case LOC_OK:                    return "OK";
        case LOC_ERR_NULL_INPUT:        return "Null input parameter";
        case LOC_ERR_INVALID_UTF8:      return "Invalid UTF-8 sequence";
        case LOC_ERR_FORBIDDEN_CODEPOINT: return "Code point not allowed in active language";
        case LOC_ERR_UNSUPPORTED_LANG:  return "Unsupported language ID";
        case LOC_ERR_BUFFER_TOO_SMALL:  return "Output buffer too small";
        case LOC_ERR_NO_LANG_SET:       return "No language has been set in context";
        case LOC_ERR_ALLOC:             return "Memory allocation failure";
        case LOC_ERR_EMPTY_RESULT:      return "Empty result";
        default:                        return "Unknown error";
    }
}
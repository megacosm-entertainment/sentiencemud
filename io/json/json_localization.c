#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../recycle.h"
#include "json_common.h"
#include "json_localization.h"
#include "../../utils/localization.h"

#define LOCALIZATION_DIR    SYSTEM_DIR "localizations/"

extern uint32_t localizations_count;
extern LOCALIZATION_DATA *localizations_list;
extern LOCALIZATION_DATA *default_localization;

static inline bool is_valid_codepoint(unichar_t cp)
{
    if (cp > 0x10FFFF) return false;
    if (cp >= 0xD800 && cp <= 0xDFFF) return false;
    return true;
}

/***************************************************************************
 * JSON Localization Loading                                               *
 ***************************************************************************/

static int localization_codepoint_compare(const void *a, const void *b)
{
    CODEPOINT_RANGE *_a = (CODEPOINT_RANGE *)a;
    CODEPOINT_RANGE *_b = (CODEPOINT_RANGE *)b;
    if (_a->lo < _b->lo) return -1;
    if (_a->lo > _b->lo) return 1;
    return 0;
} 

static LOCALIZATION_DATA *localization_load_json(const char *filename)
{
    json_t *root, *arr, *val;
    LOCALIZATION_DATA *loc;
    const char *str;
    size_t index, len;

    root = json_file_load(filename, NULL, NULL, "localization_load_json");
    if (!root)
        return NULL;

    /* Validate format */
    str = json_string_value(json_object_get(root, "_format"));
    if (!str || str_cmp(str, "localization_data")) {
        pbugf(LOG_INIT, "Invalid format in localization file: %s", filename);
        json_decref(root);
        return NULL;
    }

    loc = new_localization_data();
    if (loc) {
        loc->filename = str_dup(filename);

        str = json_string_value(json_object_get(root, "name"));
        loc->name = str_dup(str ? str : "unknown");

        str = json_string_value(json_object_get(root, "iso_name"));
        loc->iso_name = str_dup(str ? str : "unknown");

        arr = json_object_get(root, "codepoint_ranges");
        if (arr && json_is_array(arr)) {
            len = json_array_size(arr);

            loc->codepoint_ranges = calloc(len, sizeof(CODEPOINT_RANGE));
            index = 0;
            for(size_t i = 0; i < len && (val = json_array_get(arr, i)); i++) {
                if (json_is_array(val)) {
                    json_t *lo = json_array_get(val, 0);
                    json_t *hi = json_array_get(val, 1);

                    if (json_is_integer(lo) && json_is_integer(hi)) {
                        unichar_t low = (unichar_t)json_integer_value(lo);
                        unichar_t high = (unichar_t)json_integer_value(hi);

                        if (is_valid_codepoint(low) && is_valid_codepoint(high)) {
                            if (low <= high) {
                                loc->codepoint_ranges[index].lo = low;
                                loc->codepoint_ranges[index].hi = high;
                            } else {
                                loc->codepoint_ranges[index].lo = high;
                                loc->codepoint_ranges[index].hi = low;
                            }

                            index++;
                        }
                    }
                }
            }
            loc->codepoint_ranges_count = index;

            // Need to sort the codepoints by LO to put in ascending order
            qsort(loc->codepoint_ranges, loc->codepoint_ranges_count, sizeof(CODEPOINT_RANGE), localization_codepoint_compare);
        }

        arr = json_object_get(root, "filter_words");
        if (arr && json_is_array(arr)) {
            len = json_array_size(arr);

            loc->filter_words = calloc(len + 1, sizeof(char *));
            index = 0;
            for(size_t i = 0; i < len && (val = json_array_get(arr, i)); i++) {
                if (json_is_string(val))
                {
                    loc->filter_words[index] = str_dup(json_string_value(val));
                    index++;
                }
            }
        }
    
        arr = json_object_get(root, "translations");
        if (arr && json_is_array(arr)) {
            len = json_array_size(arr);

            for(size_t i = 0; i < len && (val = json_array_get(arr, i)); i++) {
                if (json_is_array(val)) {
                    json_t *ph = json_array_get(val, 0);
                    json_t *tr = json_array_get(val, 1);

                    if (json_is_string(ph) && json_is_string(tr)) {
                        const char *_ph = json_string_value(ph);

                        if (!IS_NULLSTR(_ph))
                            strdict_set(loc->translations, _ph, json_string_value(tr));
                    }
                }
            }
        }
    }

    json_decref(root);
    return loc;
}

/***************************************************************************
 * Hot-Reload                                                              *
 ***************************************************************************/

static void localization_copy_fields(LOCALIZATION_DATA *dst, LOCALIZATION_DATA *src)
{
    free_string(dst->filename);
    free_string(dst->name);
    free_string(dst->iso_name);

    if (dst->codepoint_ranges) free(dst->codepoint_ranges);
    if (dst->filter_words)
    {
        for(uint32_t i = 0;dst->filter_words[i];i++)
            free_string(dst->filter_words[i]);
        free(dst->filter_words);
    }

    if (dst->translations)
    {
        strdict_free(dst->translations);
    }

    dst->filename = src->filename;
    dst->name = src->name;
    dst->iso_name = src->iso_name;
    dst->codepoint_ranges = src->codepoint_ranges;
    dst->filter_words = src->filter_words;
    dst->translations = src->translations;

    /* Null out src to prevent double-free */
    src->filename = NULL;
    src->name = NULL;
    src->iso_name = NULL;
    src->codepoint_ranges = NULL;
    src->filter_words = NULL;
    src->translations = NULL;
}

/**
 * Reload a localization definition from its JSON file
 *
 * If the localization already exists, updates it in-place (preserving all
 * pointers). If it doesn't exist, loads it as a new localization and registers it.
 *
 * @param iso_name    Localization ISO Name (matches the JSON filename, e.g. "en" for English)
 * @return            The (re)loaded LOCALIZATION_DATA, or NULL on failure
 */
LOCALIZATION_DATA *localization_reload(const char *iso_name)
{
    char path[512];
    LOCALIZATION_DATA *existing, *temp;

    if (IS_NULLSTR(iso_name))
        return NULL;

    /* Build path: data/systam/localizations/<iso_name>.json */
    snprintf(path, sizeof(path), "%s%s.json", LOCALIZATION_DIR, iso_name);

    /* Parse the JSON file */
    temp = localization_load_json(path);
    if (!temp) {
        log_stringf("localization_reload: Failed to parse %s", path);
        return NULL;
    }

    existing = localization_lookup(iso_name);
    if (existing)
    {
        localization_copy_fields(existing, temp);

        free_localization_data(temp);

        log_stringf("localization_reload: Reloaded localization '%s(%s)' in-place",
                     existing->name, existing->iso_name);
        return existing;
    }
    else
    {
        temp->next = localizations_list;
        localizations_list = temp;
        localizations_count++;

        log_stringf("localization_reload: Loaded new localization '%s(%s)'",
                     temp->name, temp->iso_name);
        return temp;
    }
}


bool load_localizations(void)
{
    DIR *dir;
    struct dirent *entry;
    char path[512];  // Increased from 256 to handle longer paths safely
    LOCALIZATION_DATA *loc, *last;

    log_string("Loading localizations from JSON files...");

    last = NULL;
    localizations_list = NULL;
    localizations_count = 0;
    default_localization = NULL;

    /* Open localizations directory */
    dir = opendir(LOCALIZATION_DIR);
    if (!dir) {
        pbugf(LOG_INIT, "Could not access RACES_DIR at %s", LOCALIZATION_DIR);
        return false;
    }

    /* Load all .json files */
    while ((entry = readdir(dir)) != NULL) {
        /* Skip if not a .json file */
        if (strlen(entry->d_name) < 6)
            continue;
        if (str_suffix(".json", entry->d_name))
            continue;

        snprintf(path, sizeof(path), "%s%s", LOCALIZATION_DIR, entry->d_name);
        loc = localization_load_json(path);

        if (loc) {
            if (!localizations_list)
                localizations_list = loc;
            else
                last->next = loc;
            last = loc;
            loc->next = NULL;

            localizations_count++;
#ifdef MUD_DEBUG
            localization_dump_translations(loc);
#endif
        }
    }
    closedir(dir);

    if (localizations_count == 0) {
        // I consider this an error since the game requires at least ONE localization file.
        perrf(LOG_INIT, "No localizations loaded! Check if there are valid JSON files in %s", LOCALIZATION_DIR);
        return false;
    }

    localization_init_default();
    localization_init_allowed_languages();

    log_stringf("Loaded %d localizations (default localization: \"%s\")", localizations_count, (default_localization?default_localization->name:"<unset>"));
    return true;
}

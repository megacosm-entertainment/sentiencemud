#include <string.h>
#include <regex.h>
#include <jansson.h>
#if defined(CHANNEL_FILTER_USE_PCRE2)
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif
#include "../merc.h"
#include "channel_filter.h"
#include "channel_registry.h"

static bool channel_filter_match_spec(const char *text,
                                      const char *spec,
                                      bool regex_mode);

bool channel_filter_text_matches(const char *text,
                                 const char *pattern,
                                 bool regex_mode)
{
    if (IS_NULLSTR(text) || IS_NULLSTR(pattern))
        return false;

    if (!regex_mode)
        return strstr(text, pattern) != NULL;

#if defined(CHANNEL_FILTER_USE_PCRE2)
    pcre2_code *re;
    pcre2_match_data *match_data;
    int rc;
    int options = PCRE2_CASELESS | PCRE2_UTF;
    int errorcode = 0;
    PCRE2_SIZE erroroffset = 0;

    re = pcre2_compile((PCRE2_SPTR)pattern,
                       PCRE2_ZERO_TERMINATED,
                       options,
                       &errorcode,
                       &erroroffset,
                       NULL);
    if (!re)
        return false;

    match_data = pcre2_match_data_create_from_pattern(re, NULL);
    if (!match_data) {
        pcre2_code_free(re);
        return false;
    }

    rc = pcre2_match(re,
                     (PCRE2_SPTR)text,
                     strlen(text),
                     0,
                     0,
                     match_data,
                     NULL);

    pcre2_match_data_free(match_data);
    pcre2_code_free(re);

    return rc >= 0;
#else
    regex_t regex;
    int rc;

    rc = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE | REG_NOSUB);
    if (rc != 0)
        return false;

    rc = regexec(&regex, text, 0, NULL, 0);
    regfree(&regex);

    return rc == 0;
#endif
}

bool channel_filter_evaluate(CHAR_DATA *sender,
                             const char *channel_id,
                             const char *raw_text,
                             CHANNEL_FILTER_RESULT *out_result)
{
    (void)sender;
    (void)channel_id;

    if (!out_result || IS_NULLSTR(raw_text))
        return false;

    memset(out_result, 0, sizeof(*out_result));
    out_result->decision = CHANNEL_FILTER_ALLOW;
    out_result->queue_for_review = false;
    strlcpy(out_result->filtered_text, raw_text, sizeof(out_result->filtered_text));

    {
        const CHANNEL_DEF_DATA *def = channel_registry_find(channel_id);

        if (def && def->filter_enabled) {
            bool matched = false;

            if (!IS_NULLSTR(def->filter_simple) && channel_filter_match_spec(raw_text, def->filter_simple, false))
                matched = true;

            if (!matched && !IS_NULLSTR(def->filter_regex)
                && channel_filter_match_spec(raw_text, def->filter_regex, true))
                matched = true;

            if (matched) {
                out_result->decision = def->filter_mode;
                out_result->queue_for_review = (def->filter_mode != CHANNEL_FILTER_ALLOW);
                strlcpy(out_result->reason,
                        !IS_NULLSTR(def->filter_regex) ? "channel:regex" : "channel:simple",
                        sizeof(out_result->reason));

                if (def->filter_mode == CHANNEL_FILTER_REDACT)
                    strlcpy(out_result->filtered_text,
                            "[redacted by channel filter]",
                            sizeof(out_result->filtered_text));

                return true;
            }
        }
    }

    if (channel_filter_text_matches(raw_text, "\\[block\\]", true)) {
        out_result->decision = CHANNEL_FILTER_BLOCK;
        out_result->queue_for_review = true;
        strlcpy(out_result->reason, "marker:block", sizeof(out_result->reason));
        return true;
    }

    if (channel_filter_text_matches(raw_text, "\\[review\\]", true)) {
        out_result->decision = CHANNEL_FILTER_REVIEW;
        out_result->queue_for_review = true;
        strlcpy(out_result->reason, "marker:review", sizeof(out_result->reason));
        return true;
    }

    if (channel_filter_text_matches(raw_text, "\\[redact\\]", true)) {
        out_result->decision = CHANNEL_FILTER_REDACT;
        out_result->queue_for_review = true;
        strlcpy(out_result->reason, "marker:redact", sizeof(out_result->reason));
        strlcpy(out_result->filtered_text, "[redacted by channel filter]", sizeof(out_result->filtered_text));
        return true;
    }

    return true;
}

static bool channel_filter_match_spec(const char *text,
                                      const char *spec,
                                      bool regex_mode)
{
    json_t *root;
    json_error_t err;

    if (IS_NULLSTR(text) || IS_NULLSTR(spec))
        return false;

    root = json_loads(spec, 0, &err);
    if (!root) {
        char rules[512];
        char *line;
        char *saveptr = NULL;

        strlcpy(rules, spec, sizeof(rules));
        line = strtok_r(rules, "\n", &saveptr);
        while (line) {
            char rule[256];
            char *sep;

            strlcpy(rule, line, sizeof(rule));
            while (*rule == ' ' || *rule == '\t')
                memmove(rule, rule + 1, strlen(rule));

            sep = strstr(rule, "=>");
            if (sep)
                *sep = '\0';

            if (strchr(rule, ',')) {
                char *tok;
                char *csv_save = NULL;
                tok = strtok_r(rule, ",", &csv_save);
                while (tok) {
                    while (*tok == ' ' || *tok == '\t')
                        tok++;
                    if (!IS_NULLSTR(tok) && channel_filter_text_matches(text, tok, regex_mode))
                        return true;
                    tok = strtok_r(NULL, ",", &csv_save);
                }
            } else {
                if (!IS_NULLSTR(rule) && channel_filter_text_matches(text, rule, regex_mode))
                    return true;
            }

            line = strtok_r(NULL, "\n", &saveptr);
        }

        return false;
    }

    if (json_is_array(root)) {
        size_t i;
        json_t *item;
        bool matched = false;

        json_array_foreach(root, i, item) {
            if (json_is_string(item)) {
                const char *pattern = json_string_value(item);
                if (!IS_NULLSTR(pattern) && channel_filter_text_matches(text, pattern, regex_mode)) {
                    matched = true;
                    break;
                }
            } else if (json_is_object(item)) {
                const char *pattern = json_string_value(json_object_get(item, "match"));
                if (!IS_NULLSTR(pattern) && channel_filter_text_matches(text, pattern, regex_mode)) {
                    matched = true;
                    break;
                }
            }
        }

        json_decref(root);
        return matched;
    }

    if (json_is_object(root)) {
        const char *pattern = json_string_value(json_object_get(root, "match"));
        bool matched = (!IS_NULLSTR(pattern) && channel_filter_text_matches(text, pattern, regex_mode));
        json_decref(root);
        return matched;
    }

    if (json_is_string(root)) {
        const char *pattern = json_string_value(root);
        bool matched = (!IS_NULLSTR(pattern) && channel_filter_text_matches(text, pattern, regex_mode));
        json_decref(root);
        return matched;
    }

    json_decref(root);
    return false;
}

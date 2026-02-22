#include <string.h>
#include "merc.h"
#include "channel_filter.h"

static bool channel_text_has_marker(const char *text, const char *marker)
{
    if (IS_NULLSTR(text) || IS_NULLSTR(marker))
        return false;

    return strstr(text, marker) != NULL;
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

    if (channel_text_has_marker(raw_text, "[block]")) {
        out_result->decision = CHANNEL_FILTER_BLOCK;
        out_result->queue_for_review = true;
        strlcpy(out_result->reason, "marker:block", sizeof(out_result->reason));
        return true;
    }

    if (channel_text_has_marker(raw_text, "[review]")) {
        out_result->decision = CHANNEL_FILTER_REVIEW;
        out_result->queue_for_review = true;
        strlcpy(out_result->reason, "marker:review", sizeof(out_result->reason));
        return true;
    }

    if (channel_text_has_marker(raw_text, "[redact]")) {
        out_result->decision = CHANNEL_FILTER_REDACT;
        out_result->queue_for_review = true;
        strlcpy(out_result->reason, "marker:redact", sizeof(out_result->reason));
        strlcpy(out_result->filtered_text, "[redacted by channel filter]", sizeof(out_result->filtered_text));
        return true;
    }

    return true;
}

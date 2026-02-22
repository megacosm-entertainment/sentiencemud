#ifndef CHANNEL_FILTER_H
#define CHANNEL_FILTER_H

#include <stdbool.h>

typedef struct char_data CHAR_DATA;

typedef enum channel_filter_decision {
    CHANNEL_FILTER_ALLOW = 0,
    CHANNEL_FILTER_REDACT,
    CHANNEL_FILTER_BLOCK,
    CHANNEL_FILTER_REVIEW
} CHANNEL_FILTER_DECISION;

typedef struct channel_filter_result {
    CHANNEL_FILTER_DECISION decision;
    bool queue_for_review;
    char reason[64];
    char filtered_text[MSL];
} CHANNEL_FILTER_RESULT;

bool channel_filter_evaluate(CHAR_DATA *sender,
                             const char *channel_id,
                             const char *raw_text,
                             CHANNEL_FILTER_RESULT *out_result);

#endif

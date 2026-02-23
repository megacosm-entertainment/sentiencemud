#ifndef CHANNEL_REVIEW_H
#define CHANNEL_REVIEW_H

#include <stdbool.h>
#include <stddef.h>
#include "channel_transport.h"
#include "channel_filter.h"

#define CHANNEL_REVIEW_STREAM "audit:filtered_messages"
#define CHANNEL_REVIEW_SCHEMA_VERSION 1

bool channel_review_queue_append(const CHANNEL_MESSAGE *original_msg,
                                 const char *review_stream,
                                 CHANNEL_FILTER_DECISION decision,
                                 const char *reason,
                                 const char *original_text,
                                 const char *delivered_text,
                                 char *out_stream_id,
                                 size_t out_stream_id_sz);

#endif

#include <stdio.h>
#include <string.h>
#include "../merc.h"
#include "channel_review.h"

static const char *channel_filter_decision_name(CHANNEL_FILTER_DECISION decision)
{
    switch (decision) {
    case CHANNEL_FILTER_REDACT:
        return "redact";
    case CHANNEL_FILTER_BLOCK:
        return "block";
    case CHANNEL_FILTER_REVIEW:
        return "review";
    case CHANNEL_FILTER_ALLOW:
    default:
        return "allow";
    }
}

bool channel_review_queue_append(const CHANNEL_MESSAGE *original_msg,
                                 const char *review_stream,
                                 CHANNEL_FILTER_DECISION decision,
                                 const char *reason,
                                 const char *original_text,
                                 const char *delivered_text,
                                 char *out_stream_id,
                                 size_t out_stream_id_sz)
{
    CHANNEL_MESSAGE review_msg;
    char payload[MSL];
    const char *target_stream;

    if (!original_msg)
        return false;

    target_stream = IS_NULLSTR(review_stream) ? CHANNEL_REVIEW_STREAM : review_stream;

    memset(&review_msg, 0, sizeof(review_msg));
    memset(payload, 0, sizeof(payload));

    snprintf(payload, sizeof(payload),
             "schema=%d decision=%s reason=%s channel=%s original=\"%s\" delivered=\"%s\"",
             CHANNEL_REVIEW_SCHEMA_VERSION,
             channel_filter_decision_name(decision),
             IS_NULLSTR(reason) ? "none" : reason,
             IS_NULLSTR(original_msg->channel_id) ? "unknown" : original_msg->channel_id,
             IS_NULLSTR(original_text) ? "" : original_text,
             IS_NULLSTR(delivered_text) ? "" : delivered_text);

    review_msg.channel_id = "staff_review";
    review_msg.topic = target_stream;
    review_msg.sender_name = original_msg->sender_name;
    review_msg.sender_uid = original_msg->sender_uid;
    review_msg.sender_id0 = original_msg->sender_id0;
    review_msg.sender_id1 = original_msg->sender_id1;
    review_msg.message_text = payload;
    review_msg.timestamp = original_msg->timestamp;

    return channel_transport_append_history(target_stream,
                                            &review_msg,
                                            out_stream_id,
                                            out_stream_id_sz);
}

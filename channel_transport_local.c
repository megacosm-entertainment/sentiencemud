#include <pthread.h>
#include <string.h>
#include "merc.h"
#include "channel_transport.h"

#define CHANNEL_LOCAL_MAX_EVENTS 1024

typedef struct local_event {
    char topic[128];
    char channel_id[32];
    char sender_name[64];
    char sender_uid[64];
    unsigned long sender_id0;
    unsigned long sender_id1;
    char message_text[MSL];
    time_t timestamp;
} LOCAL_EVENT;

static LOCAL_EVENT local_queue[CHANNEL_LOCAL_MAX_EVENTS];
static int local_head = 0;
static int local_tail = 0;
static long local_dropped = 0;
static pthread_mutex_t local_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool local_init(void)
{
    pthread_mutex_lock(&local_mutex);
    local_head = 0;
    local_tail = 0;
    local_dropped = 0;
    pthread_mutex_unlock(&local_mutex);
    log_string("ChannelTransport(Local): initialized");
    return true;
}

static void local_shutdown(void)
{
    pthread_mutex_lock(&local_mutex);
    local_head = 0;
    local_tail = 0;
    pthread_mutex_unlock(&local_mutex);
    log_string("ChannelTransport(Local): shutdown");
}

static bool local_publish(const char *topic, const CHANNEL_MESSAGE *msg)
{
    int next_tail;
    LOCAL_EVENT *evt;

    if (!topic || !msg || !msg->channel_id || !msg->message_text)
        return false;

    pthread_mutex_lock(&local_mutex);

    next_tail = (local_tail + 1) % CHANNEL_LOCAL_MAX_EVENTS;
    if (next_tail == local_head) {
        local_dropped++;
        pthread_mutex_unlock(&local_mutex);
        return false;
    }

    evt = &local_queue[local_tail];
    strlcpy(evt->topic, topic, sizeof(evt->topic));
    strlcpy(evt->channel_id, msg->channel_id, sizeof(evt->channel_id));
    strlcpy(evt->sender_name, msg->sender_name ? msg->sender_name : "", sizeof(evt->sender_name));
    strlcpy(evt->sender_uid, msg->sender_uid ? msg->sender_uid : "", sizeof(evt->sender_uid));
    evt->sender_id0 = msg->sender_id0;
    evt->sender_id1 = msg->sender_id1;
    strlcpy(evt->message_text, msg->message_text, sizeof(evt->message_text));
    evt->timestamp = msg->timestamp;

    local_tail = next_tail;
    pthread_mutex_unlock(&local_mutex);

    return true;
}

static bool local_subscribe(const char *topic)
{
    (void)topic;
    return true;
}

static bool local_unsubscribe(const char *topic)
{
    (void)topic;
    return true;
}

static bool local_append_history(const char *stream, const CHANNEL_MESSAGE *msg,
                                 char *out_id, size_t out_id_sz)
{
    (void)stream;
    (void)msg;

    if (out_id && out_id_sz > 0)
        strlcpy(out_id, "local-0", out_id_sz);

    return true;
}

static int local_fetch_history(const char *stream, const char *cursor, int limit, void *out)
{
    (void)stream;
    (void)cursor;
    (void)limit;
    (void)out;
    return 0;
}

static bool local_health_check(void)
{
    return true;
}

static int local_drain_inbound(int max_events)
{
    int processed = 0;
    CHANNEL_MESSAGE msg;
    LOCAL_EVENT evt;

    if (max_events <= 0)
        return 0;

    while (processed < max_events) {
        pthread_mutex_lock(&local_mutex);
        if (local_head == local_tail) {
            pthread_mutex_unlock(&local_mutex);
            break;
        }

        evt = local_queue[local_head];
        local_head = (local_head + 1) % CHANNEL_LOCAL_MAX_EVENTS;
        pthread_mutex_unlock(&local_mutex);

        msg.topic = evt.topic;
        msg.channel_id = evt.channel_id;
        msg.sender_name = evt.sender_name;
        msg.sender_uid = evt.sender_uid;
        msg.sender_id0 = evt.sender_id0;
        msg.sender_id1 = evt.sender_id1;
        msg.message_text = evt.message_text;
        msg.timestamp = evt.timestamp;

        channel_transport_dispatch_inbound(&msg);
        processed++;
    }

    return processed;
}

static const channel_transport local_transport = {
    .name = "local",
    .init = local_init,
    .shutdown = local_shutdown,
    .publish = local_publish,
    .subscribe = local_subscribe,
    .unsubscribe = local_unsubscribe,
    .append_history = local_append_history,
    .fetch_history = local_fetch_history,
    .health_check = local_health_check,
    .drain_inbound = local_drain_inbound
};

const channel_transport *channel_transport_local_backend(void)
{
    return &local_transport;
}

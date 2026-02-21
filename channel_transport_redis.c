#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <hiredis/hiredis.h>
#include <jansson.h>
#include "merc.h"
#include "channel_transport.h"

#define CHANNEL_REDIS_QUEUE_CAPACITY 1024

typedef struct redis_outbound_event {
    char topic[128];
    char channel_id[32];
    char sender_name[64];
    unsigned long sender_id0;
    unsigned long sender_id1;
    char message_text[MSL];
    time_t timestamp;
} REDIS_OUTBOUND_EVENT;

static REDIS_OUTBOUND_EVENT outbound_queue[CHANNEL_REDIS_QUEUE_CAPACITY];
static int outbound_head = 0;
static int outbound_tail = 0;
static long outbound_dropped = 0;

static REDIS_OUTBOUND_EVENT inbound_queue[CHANNEL_REDIS_QUEUE_CAPACITY];
static int inbound_head = 0;
static int inbound_tail = 0;
static long inbound_dropped = 0;

static pthread_mutex_t outbound_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t outbound_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t inbound_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_t outbound_thread;
static pthread_t inbound_thread;
static bool outbound_running = false;
static bool inbound_running = false;
static bool worker_stop = false;

static redisContext *command_ctx = NULL;
static redisContext *subscribe_ctx = NULL;
static pthread_mutex_t connection_state_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool command_connected = false;
static bool subscribe_connected = false;

static void set_connection_state(bool command_ok, bool subscribe_ok)
{
    pthread_mutex_lock(&connection_state_mutex);
    command_connected = command_ok;
    subscribe_connected = subscribe_ok;
    pthread_mutex_unlock(&connection_state_mutex);
}

static void set_command_connected(bool command_ok)
{
    pthread_mutex_lock(&connection_state_mutex);
    command_connected = command_ok;
    pthread_mutex_unlock(&connection_state_mutex);
}

static void set_subscribe_connected(bool subscribe_ok)
{
    pthread_mutex_lock(&connection_state_mutex);
    subscribe_connected = subscribe_ok;
    pthread_mutex_unlock(&connection_state_mutex);
}

static void redis_disconnect_command(void)
{
    if (command_ctx) {
        redisFree(command_ctx);
        command_ctx = NULL;
    }
    set_command_connected(false);
}

static void redis_disconnect_subscribe(void)
{
    if (subscribe_ctx) {
        redisFree(subscribe_ctx);
        subscribe_ctx = NULL;
    }
    set_subscribe_connected(false);
}

static redisContext *redis_connect_context(void)
{
    struct timeval timeout;
    const char *host = game_settings.redis_host ? game_settings.redis_host : "127.0.0.1";
    int port = game_settings.redis_port > 0 ? game_settings.redis_port : 6379;
    redisContext *ctx;

    timeout.tv_sec = game_settings.redis_timeout_sec > 0 ? game_settings.redis_timeout_sec : 1;
    timeout.tv_usec = game_settings.redis_timeout_usec >= 0 ? game_settings.redis_timeout_usec : 500000;

    ctx = redisConnectWithTimeout(host, port, timeout);
    if (!ctx || ctx->err) {
        if (ctx) {
            log_stringf("ChannelTransport(redis): connect failed: %s", ctx->errstr);
            redisFree(ctx);
        }
        return NULL;
    }

    redisSetTimeout(ctx, timeout);
    return ctx;
}

static bool redis_auth_ping(redisContext *ctx)
{
    redisReply *reply;

    if (!ctx)
        return false;

    if (!IS_NULLSTR(game_settings.redis_password)) {
        reply = redisCommand(ctx, "AUTH %s", game_settings.redis_password);
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            log_string("ChannelTransport(redis): AUTH failed");
            if (reply)
                freeReplyObject(reply);
            return false;
        }
        freeReplyObject(reply);
    }

    reply = redisCommand(ctx, "PING");
    if (!reply || reply->type != REDIS_REPLY_STATUS || str_cmp(reply->str, "PONG")) {
        log_string("ChannelTransport(redis): PING failed");
        if (reply)
            freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);
    return true;
}

static bool pop_outbound_event(REDIS_OUTBOUND_EVENT *evt)
{
    if (!evt)
        return false;

    pthread_mutex_lock(&outbound_mutex);
    if (outbound_head == outbound_tail) {
        pthread_mutex_unlock(&outbound_mutex);
        return false;
    }

    *evt = outbound_queue[outbound_head];
    outbound_head = (outbound_head + 1) % CHANNEL_REDIS_QUEUE_CAPACITY;
    pthread_mutex_unlock(&outbound_mutex);
    return true;
}

static bool push_inbound_event(const REDIS_OUTBOUND_EVENT *evt)
{
    int next_tail;

    if (!evt)
        return false;

    pthread_mutex_lock(&inbound_mutex);
    next_tail = (inbound_tail + 1) % CHANNEL_REDIS_QUEUE_CAPACITY;
    if (next_tail == inbound_head) {
        inbound_dropped++;
        pthread_mutex_unlock(&inbound_mutex);
        return false;
    }

    inbound_queue[inbound_tail] = *evt;
    inbound_tail = next_tail;
    pthread_mutex_unlock(&inbound_mutex);
    return true;
}

static bool publish_event_locked(const REDIS_OUTBOUND_EVENT *evt)
{
    redisReply *reply;
    char history_stream[196];
    json_t *payload;
    char *payload_json;
    bool success = true;

    if (!evt)
        return false;

    snprintf(history_stream, sizeof(history_stream), "history:%s", evt->topic);

    reply = redisCommand(command_ctx,
                         "XADD %s * channel %s sender %s sender_id0 %lu sender_id1 %lu message %s ts %ld",
                         history_stream,
                         evt->channel_id,
                         evt->sender_name,
                         evt->sender_id0,
                         evt->sender_id1,
                         evt->message_text,
                         (long)evt->timestamp);
    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        if (reply)
            freeReplyObject(reply);
        return false;
    }
    freeReplyObject(reply);

    payload = json_object();
    json_object_set_new(payload, "channel_id", json_string(evt->channel_id));
    json_object_set_new(payload, "topic", json_string(evt->topic));
    json_object_set_new(payload, "sender_name", json_string(evt->sender_name));
    json_object_set_new(payload, "sender_id0", json_integer((json_int_t)evt->sender_id0));
    json_object_set_new(payload, "sender_id1", json_integer((json_int_t)evt->sender_id1));
    json_object_set_new(payload, "message_text", json_string(evt->message_text));
    json_object_set_new(payload, "timestamp", json_integer((json_int_t)evt->timestamp));

    payload_json = json_dumps(payload, JSON_COMPACT);
    json_decref(payload);
    if (!payload_json) {
        return false;
    }

    reply = redisCommand(command_ctx,
                         "PUBLISH %s %s",
                         evt->topic,
                         payload_json);
    free(payload_json);

    if (!reply || reply->type == REDIS_REPLY_ERROR) {
        if (reply)
            freeReplyObject(reply);
        success = false;
    } else {
        freeReplyObject(reply);
    }

    return success;
}

static bool decode_payload(const char *channel, const char *payload, REDIS_OUTBOUND_EVENT *evt)
{
    json_error_t error;
    json_t *root;
    json_t *v;

    if (!payload || !evt)
        return false;

    root = json_loads(payload, 0, &error);
    if (!root || !json_is_object(root)) {
        if (root)
            json_decref(root);
        return false;
    }

    memset(evt, 0, sizeof(*evt));
    strlcpy(evt->topic, channel ? channel : "", sizeof(evt->topic));

    v = json_object_get(root, "channel_id");
    if (!json_is_string(v)) {
        json_decref(root);
        return false;
    }
    strlcpy(evt->channel_id, json_string_value(v), sizeof(evt->channel_id));

    v = json_object_get(root, "sender_name");
    if (json_is_string(v))
        strlcpy(evt->sender_name, json_string_value(v), sizeof(evt->sender_name));

    v = json_object_get(root, "sender_id0");
    if (json_is_integer(v))
        evt->sender_id0 = (unsigned long)json_integer_value(v);

    v = json_object_get(root, "sender_id1");
    if (json_is_integer(v))
        evt->sender_id1 = (unsigned long)json_integer_value(v);

    v = json_object_get(root, "message_text");
    if (!json_is_string(v)) {
        json_decref(root);
        return false;
    }
    strlcpy(evt->message_text, json_string_value(v), sizeof(evt->message_text));

    v = json_object_get(root, "timestamp");
    if (json_is_integer(v))
        evt->timestamp = (time_t)json_integer_value(v);
    else
        evt->timestamp = current_time;

    json_decref(root);
    return true;
}

static void *redis_outbound_worker_main(void *arg)
{
    REDIS_OUTBOUND_EVENT evt;
    (void)arg;

    while (!worker_stop) {
        bool got_event = pop_outbound_event(&evt);

        if (!got_event) {
            struct timespec wake;
            clock_gettime(CLOCK_REALTIME, &wake);
            wake.tv_nsec += 100000000;
            if (wake.tv_nsec >= 1000000000) {
                wake.tv_sec += 1;
                wake.tv_nsec -= 1000000000;
            }

            pthread_mutex_lock(&outbound_mutex);
            if (!worker_stop && outbound_head == outbound_tail) {
                pthread_cond_timedwait(&outbound_cond, &outbound_mutex, &wake);
            }
            pthread_mutex_unlock(&outbound_mutex);
            continue;
        }

        if (!command_ctx) {
            command_ctx = redis_connect_context();
            if (!command_ctx || !redis_auth_ping(command_ctx)) {
                redis_disconnect_command();
                sleep(1);
                continue;
            }
            set_command_connected(true);
        }

        if (!publish_event_locked(&evt)) {
            redis_disconnect_command();
        }
    }

    redis_disconnect_command();
    return NULL;
}

static bool ensure_subscription(void)
{
    redisReply *reply;

    if (!subscribe_ctx) {
        subscribe_ctx = redis_connect_context();
        if (!subscribe_ctx || !redis_auth_ping(subscribe_ctx)) {
            redis_disconnect_subscribe();
            return false;
        }
        set_subscribe_connected(true);
    }

    reply = redisCommand(subscribe_ctx, "PSUBSCRIBE rt:*");
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        if (reply)
            freeReplyObject(reply);
        redis_disconnect_subscribe();
        return false;
    }

    freeReplyObject(reply);
    return true;
}

static void *redis_inbound_worker_main(void *arg)
{
    (void)arg;

    while (!worker_stop) {
        void *reply_obj = NULL;
        redisReply *reply;
        REDIS_OUTBOUND_EVENT evt;
        const char *kind;
        const char *channel;
        const char *payload;
        int rc;

        if (!ensure_subscription()) {
            sleep(1);
            continue;
        }

        rc = redisGetReply(subscribe_ctx, &reply_obj);
        if (rc != REDIS_OK || !reply_obj) {
            redis_disconnect_subscribe();
            continue;
        }

        reply = (redisReply *)reply_obj;
        if (reply->type != REDIS_REPLY_ARRAY || reply->elements < 3 ||
            !reply->element[0] || reply->element[0]->type != REDIS_REPLY_STRING) {
            freeReplyObject(reply);
            continue;
        }

        kind = reply->element[0]->str;
        if (!str_cmp(kind, "message") && reply->elements >= 3) {
            channel = reply->element[1]->str;
            payload = reply->element[2]->str;
        } else if (!str_cmp(kind, "pmessage") && reply->elements >= 4) {
            channel = reply->element[2]->str;
            payload = reply->element[3]->str;
        } else {
            freeReplyObject(reply);
            continue;
        }

        if (decode_payload(channel, payload, &evt)) {
            push_inbound_event(&evt);
        }

        freeReplyObject(reply);
    }

    redis_disconnect_subscribe();
    return NULL;
}

static bool redis_transport_init(void)
{
    redisContext *probe;

    pthread_mutex_lock(&outbound_mutex);
    outbound_head = 0;
    outbound_tail = 0;
    outbound_dropped = 0;
    worker_stop = false;
    pthread_mutex_unlock(&outbound_mutex);

    pthread_mutex_lock(&inbound_mutex);
    inbound_head = 0;
    inbound_tail = 0;
    inbound_dropped = 0;
    pthread_mutex_unlock(&inbound_mutex);

    set_connection_state(false, false);

    probe = redis_connect_context();
    if (!probe || !redis_auth_ping(probe)) {
        if (probe)
            redisFree(probe);
        log_string("ChannelTransport(redis): startup probe failed");
        return false;
    }
    redisFree(probe);

    outbound_running = (pthread_create(&outbound_thread, NULL, redis_outbound_worker_main, NULL) == 0);
    if (!outbound_running) {
        log_string("ChannelTransport(redis): failed to start outbound worker thread");
        return false;
    }

    inbound_running = (pthread_create(&inbound_thread, NULL, redis_inbound_worker_main, NULL) == 0);
    if (!inbound_running) {
        log_string("ChannelTransport(redis): failed to start inbound worker thread");
        worker_stop = true;
        pthread_mutex_lock(&outbound_mutex);
        pthread_cond_signal(&outbound_cond);
        pthread_mutex_unlock(&outbound_mutex);
        pthread_join(outbound_thread, NULL);
        outbound_running = false;
        return false;
    }

    log_string("ChannelTransport(redis): initialized with inbound/outbound workers");
    return true;
}

static void redis_transport_shutdown(void)
{
    if (!outbound_running && !inbound_running)
        return;

    pthread_mutex_lock(&outbound_mutex);
    worker_stop = true;
    pthread_cond_signal(&outbound_cond);
    pthread_mutex_unlock(&outbound_mutex);

    if (outbound_running) {
        pthread_join(outbound_thread, NULL);
        outbound_running = false;
    }

    if (inbound_running) {
        pthread_join(inbound_thread, NULL);
        inbound_running = false;
    }

    redis_disconnect_command();

    pthread_mutex_lock(&outbound_mutex);
    outbound_head = 0;
    outbound_tail = 0;
    pthread_mutex_unlock(&outbound_mutex);

    pthread_mutex_lock(&inbound_mutex);
    inbound_head = 0;
    inbound_tail = 0;
    pthread_mutex_unlock(&inbound_mutex);

    log_stringf("ChannelTransport(redis): shutdown (dropped outbound=%ld inbound=%ld)",
                outbound_dropped, inbound_dropped);
}

static bool redis_transport_publish(const char *topic, const CHANNEL_MESSAGE *msg)
{
    int next_tail;
    REDIS_OUTBOUND_EVENT *evt;

    if (!topic || !msg || !msg->channel_id || !msg->message_text)
        return false;

    pthread_mutex_lock(&outbound_mutex);

    next_tail = (outbound_tail + 1) % CHANNEL_REDIS_QUEUE_CAPACITY;
    if (next_tail == outbound_head) {
        outbound_dropped++;
        pthread_mutex_unlock(&outbound_mutex);
        return false;
    }

    evt = &outbound_queue[outbound_tail];
    strlcpy(evt->topic, topic, sizeof(evt->topic));
    strlcpy(evt->channel_id, msg->channel_id, sizeof(evt->channel_id));
    strlcpy(evt->sender_name, msg->sender_name ? msg->sender_name : "", sizeof(evt->sender_name));
    evt->sender_id0 = msg->sender_id0;
    evt->sender_id1 = msg->sender_id1;
    strlcpy(evt->message_text, msg->message_text, sizeof(evt->message_text));
    evt->timestamp = msg->timestamp;

    outbound_tail = next_tail;
    pthread_cond_signal(&outbound_cond);
    pthread_mutex_unlock(&outbound_mutex);

    return true;
}

static bool redis_transport_subscribe(const char *topic)
{
    (void)topic;
    return true;
}

static bool redis_transport_unsubscribe(const char *topic)
{
    (void)topic;
    return true;
}

static bool redis_transport_append_history(const char *stream, const CHANNEL_MESSAGE *msg,
                                          char *out_id, size_t out_id_sz)
{
    (void)stream;
    (void)msg;
    if (out_id && out_id_sz > 0)
        strlcpy(out_id, "queued", out_id_sz);
    return true;
}

static int redis_transport_fetch_history(const char *stream, const char *cursor, int limit, void *out)
{
    (void)stream;
    (void)cursor;
    (void)limit;
    (void)out;
    return 0;
}

static bool redis_transport_health_check(void)
{
    bool command_ok;
    bool subscribe_ok;

    pthread_mutex_lock(&connection_state_mutex);
    command_ok = command_connected;
    subscribe_ok = subscribe_connected;
    pthread_mutex_unlock(&connection_state_mutex);

    return outbound_running && inbound_running && command_ok && subscribe_ok;
}

static int redis_transport_drain_inbound(int max_events)
{
    int processed = 0;
    CHANNEL_MESSAGE msg;
    REDIS_OUTBOUND_EVENT evt;

    if (max_events <= 0)
        return 0;

    while (processed < max_events) {
        pthread_mutex_lock(&inbound_mutex);
        if (inbound_head == inbound_tail) {
            pthread_mutex_unlock(&inbound_mutex);
            break;
        }

        evt = inbound_queue[inbound_head];
        inbound_head = (inbound_head + 1) % CHANNEL_REDIS_QUEUE_CAPACITY;
        pthread_mutex_unlock(&inbound_mutex);

        msg.topic = evt.topic;
        msg.channel_id = evt.channel_id;
        msg.sender_name = evt.sender_name;
        msg.sender_id0 = evt.sender_id0;
        msg.sender_id1 = evt.sender_id1;
        msg.message_text = evt.message_text;
        msg.timestamp = evt.timestamp;

        channel_transport_dispatch_inbound(&msg);
        processed++;
    }

    return processed;
}

static const channel_transport redis_transport = {
    .name = "redis",
    .init = redis_transport_init,
    .shutdown = redis_transport_shutdown,
    .publish = redis_transport_publish,
    .subscribe = redis_transport_subscribe,
    .unsubscribe = redis_transport_unsubscribe,
    .append_history = redis_transport_append_history,
    .fetch_history = redis_transport_fetch_history,
    .health_check = redis_transport_health_check,
    .drain_inbound = redis_transport_drain_inbound
};

const channel_transport *channel_transport_redis_backend(void)
{
    return &redis_transport;
}

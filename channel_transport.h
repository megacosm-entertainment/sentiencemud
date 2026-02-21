#ifndef CHANNEL_TRANSPORT_H
#define CHANNEL_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/* Forward declaration from merc.h */
typedef struct char_data CHAR_DATA;

typedef enum channel_backend_mode {
    CHANNEL_BACKEND_LEGACY_ITERATIVE = 0,
    CHANNEL_BACKEND_LOCAL,
    CHANNEL_BACKEND_AUTO,
    CHANNEL_BACKEND_REDIS
} CHANNEL_BACKEND_MODE;

typedef struct channel_message {
    const char *channel_id;
    const char *topic;
    const char *sender_name;
    unsigned long sender_id0;
    unsigned long sender_id1;
    const char *message_text;
    time_t timestamp;
} CHANNEL_MESSAGE;

typedef struct channel_transport channel_transport;
typedef void (*channel_inbound_handler_fn)(const CHANNEL_MESSAGE *msg);

struct channel_transport {
    const char *name;

    bool (*init)(void);
    void (*shutdown)(void);

    bool (*publish)(const char *topic, const CHANNEL_MESSAGE *msg);
    bool (*subscribe)(const char *topic);
    bool (*unsubscribe)(const char *topic);

    bool (*append_history)(const char *stream, const CHANNEL_MESSAGE *msg,
                           char *out_id, size_t out_id_sz);
    int (*fetch_history)(const char *stream, const char *cursor,
                         int limit, void *out);

    bool (*health_check)(void);

    /* Must be non-blocking on game loop thread. */
    int (*drain_inbound)(int max_events);
};

bool channel_transport_init(void);
void channel_transport_shutdown(void);
void channel_transport_pulse(void);

const char *channel_transport_backend_name(void);
CHANNEL_BACKEND_MODE channel_transport_backend_mode(void);

bool channel_transport_publish(const char *topic, const CHANNEL_MESSAGE *msg);
bool channel_transport_subscribe(const char *topic);
bool channel_transport_unsubscribe(const char *topic);

void channel_transport_set_inbound_handler(channel_inbound_handler_fn handler);
void channel_transport_dispatch_inbound(const CHANNEL_MESSAGE *msg);

#endif
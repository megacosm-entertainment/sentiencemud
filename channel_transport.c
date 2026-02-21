#include <string.h>
#include "merc.h"
#include "channel_transport.h"

const channel_transport *channel_transport_local_backend(void);
const channel_transport *channel_transport_redis_backend(void);

static bool legacy_init(void)
{
    log_string("ChannelTransport(LegacyIterative): initialized");
    return true;
}

static void legacy_shutdown(void)
{
    log_string("ChannelTransport(LegacyIterative): shutdown");
}

static bool legacy_publish(const char *topic, const CHANNEL_MESSAGE *msg)
{
    (void)topic;
    (void)msg;
    return true;
}

static bool legacy_subscribe(const char *topic)
{
    (void)topic;
    return true;
}

static bool legacy_unsubscribe(const char *topic)
{
    (void)topic;
    return true;
}

static bool legacy_append_history(const char *stream, const CHANNEL_MESSAGE *msg,
                                  char *out_id, size_t out_id_sz)
{
    (void)stream;
    (void)msg;
    if (out_id && out_id_sz > 0)
        strlcpy(out_id, "legacy-0", out_id_sz);
    return true;
}

static int legacy_fetch_history(const char *stream, const char *cursor, int limit, void *out)
{
    (void)stream;
    (void)cursor;
    (void)limit;
    (void)out;
    return 0;
}

static bool legacy_health_check(void)
{
    return true;
}

static int legacy_drain_inbound(int max_events)
{
    (void)max_events;
    return 0;
}

static const channel_transport legacy_transport = {
    .name = "legacy_iterative",
    .init = legacy_init,
    .shutdown = legacy_shutdown,
    .publish = legacy_publish,
    .subscribe = legacy_subscribe,
    .unsubscribe = legacy_unsubscribe,
    .append_history = legacy_append_history,
    .fetch_history = legacy_fetch_history,
    .health_check = legacy_health_check,
    .drain_inbound = legacy_drain_inbound
};

static const channel_transport *active_transport = &legacy_transport;
static CHANNEL_BACKEND_MODE active_mode = CHANNEL_BACKEND_LEGACY_ITERATIVE;
static channel_inbound_handler_fn inbound_handler = NULL;

static CHANNEL_BACKEND_MODE parse_backend_mode(const char *mode)
{
    if (IS_NULLSTR(mode))
        return CHANNEL_BACKEND_LEGACY_ITERATIVE;

    if (!str_cmp(mode, "legacy") || !str_cmp(mode, "legacy_iterative"))
        return CHANNEL_BACKEND_LEGACY_ITERATIVE;
    if (!str_cmp(mode, "local"))
        return CHANNEL_BACKEND_LOCAL;
    if (!str_cmp(mode, "auto"))
        return CHANNEL_BACKEND_AUTO;
    if (!str_cmp(mode, "redis"))
        return CHANNEL_BACKEND_REDIS;

    return CHANNEL_BACKEND_LEGACY_ITERATIVE;
}

bool channel_transport_init(void)
{
    const char *configured = game_settings.channel_backend;
    CHANNEL_BACKEND_MODE requested = parse_backend_mode(configured);
    const channel_transport *selected = &legacy_transport;
    CHANNEL_BACKEND_MODE selected_mode = requested;
    bool init_ok = false;

    switch (requested) {
        case CHANNEL_BACKEND_LOCAL:
            selected = channel_transport_local_backend();
            selected_mode = CHANNEL_BACKEND_LOCAL;
            break;

        case CHANNEL_BACKEND_AUTO:
            selected_mode = CHANNEL_BACKEND_AUTO;
            selected = game_settings.enable_redis ? channel_transport_redis_backend() : channel_transport_local_backend();
            break;

        case CHANNEL_BACKEND_REDIS:
            selected = channel_transport_redis_backend();
            selected_mode = CHANNEL_BACKEND_REDIS;
            break;

        case CHANNEL_BACKEND_LEGACY_ITERATIVE:
        default:
            selected = &legacy_transport;
            selected_mode = CHANNEL_BACKEND_LEGACY_ITERATIVE;
            break;
    }

    active_transport = selected;
    active_mode = selected_mode;

    if (active_transport && active_transport->init)
        init_ok = active_transport->init();

    if (init_ok)
        return true;

    if (requested == CHANNEL_BACKEND_AUTO || requested == CHANNEL_BACKEND_REDIS) {
        log_string("ChannelTransport: selected backend init failed, falling back to local");
        active_transport = channel_transport_local_backend();
        if (active_transport && active_transport->init && active_transport->init()) {
            return true;
        }
    }

    log_string("ChannelTransport: local backend init failed, using legacy iterative fallback");
    active_transport = &legacy_transport;
    active_mode = CHANNEL_BACKEND_LEGACY_ITERATIVE;

    if (active_transport && active_transport->init)
        return active_transport->init();

    return false;
}

void channel_transport_shutdown(void)
{
    if (active_transport && active_transport->shutdown)
        active_transport->shutdown();

    active_transport = &legacy_transport;
    active_mode = CHANNEL_BACKEND_LEGACY_ITERATIVE;
}

void channel_transport_pulse(void)
{
    if (active_mode == CHANNEL_BACKEND_AUTO && active_transport &&
        !str_cmp(channel_transport_backend_name(), "redis") &&
        active_transport->health_check && !active_transport->health_check()) {
        log_string("ChannelTransport(auto): redis unhealthy, failing over to local");

        if (active_transport->shutdown)
            active_transport->shutdown();

        active_transport = channel_transport_local_backend();
        if (!active_transport || !active_transport->init || !active_transport->init()) {
            log_string("ChannelTransport(auto): local failover failed, using legacy iterative");
            active_transport = &legacy_transport;
            active_mode = CHANNEL_BACKEND_LEGACY_ITERATIVE;
            if (active_transport->init)
                active_transport->init();
        }
    }

    if (active_transport && active_transport->drain_inbound)
        active_transport->drain_inbound(64);
}

const char *channel_transport_backend_name(void)
{
    return active_transport && active_transport->name
        ? active_transport->name
        : "legacy_iterative";
}

CHANNEL_BACKEND_MODE channel_transport_backend_mode(void)
{
    return active_mode;
}

bool channel_transport_publish(const char *topic, const CHANNEL_MESSAGE *msg)
{
    if (!active_transport || !active_transport->publish)
        return false;

    return active_transport->publish(topic, msg);
}

bool channel_transport_subscribe(const char *topic)
{
    if (!active_transport || !active_transport->subscribe)
        return false;

    return active_transport->subscribe(topic);
}

bool channel_transport_unsubscribe(const char *topic)
{
    if (!active_transport || !active_transport->unsubscribe)
        return false;

    return active_transport->unsubscribe(topic);
}

void channel_transport_set_inbound_handler(channel_inbound_handler_fn handler)
{
    inbound_handler = handler;
}

void channel_transport_dispatch_inbound(const CHANNEL_MESSAGE *msg)
{
    if (inbound_handler)
        inbound_handler(msg);
}

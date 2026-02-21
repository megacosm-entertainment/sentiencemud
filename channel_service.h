#ifndef CHANNEL_SERVICE_H
#define CHANNEL_SERVICE_H

#include <stdbool.h>
#include <time.h>
#include "channel_transport.h"

/* Forward declaration from merc.h */
typedef struct char_data CHAR_DATA;

typedef enum channel_scope {
    CHANNEL_SCOPE_GLOBAL = 0,
    CHANNEL_SCOPE_AREA,
    CHANNEL_SCOPE_REGION,
    CHANNEL_SCOPE_ROOM_WV,
    CHANNEL_SCOPE_DIRECT_ENTITY,
    CHANNEL_SCOPE_GROUP_ID,
    CHANNEL_SCOPE_CHURCH_ID
} CHANNEL_SCOPE;

#define CHANNEL_MAX_ROUTE_TARGETS 16

typedef struct channel_route_targets {
    int region_count;
    long region_uids[CHANNEL_MAX_ROUTE_TARGETS];

    int area_count;
    long area_uids[CHANNEL_MAX_ROUTE_TARGETS];

    int topic_group_count;
    const char *topic_groups[CHANNEL_MAX_ROUTE_TARGETS];
} CHANNEL_ROUTE_TARGETS;

typedef struct channel_definition {
    const char *id;
    const char *name;
    const char *command;
    CHANNEL_SCOPE scope;
    bool allow_player_flags;
    CHANNEL_ROUTE_TARGETS route_targets;
} CHANNEL_DEFINITION;

bool channel_service_init(void);
void channel_service_shutdown(void);
void channel_service_pulse(void);

bool channel_service_send(CHAR_DATA *sender, const char *channel_id, const char *raw_text);
const char *channel_service_backend_name(void);

#endif
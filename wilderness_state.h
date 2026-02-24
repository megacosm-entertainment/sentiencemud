#ifndef WILDERNESS_STATE_H
#define WILDERNESS_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef struct wilds_data WILDS_DATA;

typedef enum wilderness_feature_type
{
    WILDERNESS_FEATURE_BANDIT_CAMP = 1,
    WILDERNESS_FEATURE_SPAWN_NODE = 2,
    WILDERNESS_FEATURE_WEATHER_HAZARD = 3,
    WILDERNESS_FEATURE_ROUTE_ENCOUNTER = 4
} WILDERNESS_FEATURE_TYPE;

typedef enum wilderness_actor_type
{
    WILDERNESS_ACTOR_WEATHER = 1,
    WILDERNESS_ACTOR_NPC_SHIP = 2,
    WILDERNESS_ACTOR_NPC_SPAWN_CONTROLLER = 3
} WILDERNESS_ACTOR_TYPE;

typedef struct wilderness_feature_record
{
    long uid;
    int type;
    int x;
    int y;
    int radius;
    bool permanent;
    time_t created_at;
    time_t expires_at;
} WILDERNESS_FEATURE_RECORD;

typedef struct wilderness_actor_record
{
    long uid;
    int type;
    int x;
    int y;
    int z;
    int heading;
    int speed;
    long region_uid;
    bool active;
} WILDERNESS_ACTOR_RECORD;

bool wilderness_state_init(void);
void wilderness_state_shutdown(void);
void wilderness_state_pulse(void);

bool wilderness_state_load(WILDS_DATA *pWilds);
bool wilderness_state_save(WILDS_DATA *pWilds);
void wilderness_state_mark_dirty(WILDS_DATA *pWilds, const char *reason);

#endif

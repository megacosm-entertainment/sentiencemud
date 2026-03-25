/*
 * gmcp_sentience.h — Sentience-native GMCP packages
 *
 * Defines Sentience.Char.* and Sentience.Room.* packages that replace
 * the legacy Char.* / Room.Info packages for clients that negotiate
 * "Sentience 1" support or connect via WebSocket.
 *
 * See docs/PLAN_GMCP_REWORK.md for the full package specification.
 */

#ifndef GMCP_SENTIENCE_H
#define GMCP_SENTIENCE_H

#include <jansson.h>
#include <stdbool.h>

/* Forward declarations — full types live in merc.h / protocol.h */
typedef struct descriptor_data descriptor_t;

/*
 * Dirty-flag bitmask — one bit per Sentience.* package.
 * Set when underlying data changes; cleared after send.
 */
typedef enum {
    SENTIENCE_DIRTY_IDENTITY  = (1 << 0),
    SENTIENCE_DIRTY_VITALS    = (1 << 1),
    SENTIENCE_DIRTY_STATS     = (1 << 2),
    SENTIENCE_DIRTY_COMBAT    = (1 << 3),
    SENTIENCE_DIRTY_WORTH     = (1 << 4),
    SENTIENCE_DIRTY_ROOM      = (1 << 5),
    SENTIENCE_DIRTY_AFFECTS   = (1 << 6),
    SENTIENCE_DIRTY_ENEMIES   = (1 << 7),
} sentience_dirty_t;

#define SENTIENCE_PACKAGE_VERSION 1

/* Maximum classes a character can have simultaneously */
#define SENTIENCE_MAX_CLASSES 8

/*
 * Cache of previously-sent values. Stored in protocol_t.
 * Compared each update cycle to detect changes (dirty tracking).
 */
typedef struct {
    /* Vitals */
    long hp, max_hp;
    long mana, max_mana;
    long move, max_move;

    /* Stats */
    int str, int_, wis, dex, con;
    int str_perm, int_perm, wis_perm, dex_perm, con_perm;
    int hitroll, damroll, wimpy;

    /* Combat (AC) */
    int ac_pierce, ac_bash, ac_slash, ac_exotic;

    /* Worth */
    int alignment;
    long xp, xp_tnl;
    int practices;
    long gold;

    /* Identity */
    int level, tot_level;
    char name[64];
    char race_wnum[128];
    char body_type[32];
    char title[256];
    int num_classes;
    struct {
        char id[128];
        char name[64];
        int level;
        bool is_primary;
    } classes[SENTIENCE_MAX_CLASSES];

    /* Room */
    long room_id0, room_id1;

    /* Tracks which packages have been sent at least once */
    bool initialized;
} sentience_gmcp_cache_t;

/*
 * JSON builder functions — pure functions that take explicit parameters.
 * Testable without game state. Caller must json_decref() the result.
 */
json_t *sentience_build_vitals_json(long hp, long max_hp,
                                     long mana, long max_mana,
                                     long move, long max_move);

json_t *sentience_build_stats_json(int str, int int_, int wis, int dex, int con,
                                    int str_perm, int int_perm, int wis_perm,
                                    int dex_perm, int con_perm,
                                    int hitroll, int damroll, int wimpy);

json_t *sentience_build_combat_json(int ac_pierce, int ac_bash,
                                     int ac_slash, int ac_exotic);

json_t *sentience_build_worth_json(int alignment, long xp, long xp_tnl,
                                    int practices, long gold);

/*
 * Identity builder input — avoids a massive parameter list.
 */
typedef struct {
    const char *name;
    const char *race_wnum;
    const char *race_name;
    const char *body_type;
    int level;
    int tot_level;
    const char *title;
    int num_classes;
    struct {
        const char *id;
        const char *name;
        int level;
        bool is_primary;
    } classes[SENTIENCE_MAX_CLASSES];
} sentience_identity_input_t;

json_t *sentience_build_identity_json(const sentience_identity_input_t *data);

/*
 * Room builder input.
 */
typedef struct {
    const char *wnum;
    const char *name;
    const char *area_name;
    const char *area_wnum;
    const char *sector;
    bool is_wilds;
    int wilds_uid;
    int wilds_x;
    int wilds_y;
    int num_exits;
    struct {
        const char *dir;
        const char *wnum;
        const char *name;
        bool is_door;
        bool is_closed;
        bool is_locked;
    } exits[10]; /* N,E,S,W,U,D + diagonals */
} sentience_room_input_t;

json_t *sentience_build_room_json(const sentience_room_input_t *data);

/*
 * Game-loop entry point. Called from gmcp_update() for each descriptor.
 * Compares current character state with cache, sends dirty packages.
 */
void sentience_gmcp_update(descriptor_t *d);

/*
 * Reset cache to force full resend (e.g., on login or reconnect).
 */
void sentience_gmcp_cache_reset(sentience_gmcp_cache_t *cache);

#endif /* GMCP_SENTIENCE_H */

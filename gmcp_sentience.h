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

    /* Phase 3: Room contents fingerprint (separate from Phase 1 room_id0/room_id1) */
    long contents_room_id[2];
    int contents_count;

    /* Phase 3: Affects transition detection */
    bool had_affects;

    /* Phase 3: Combat transition detection */
    bool was_fighting;

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

/*
 * Phase 3 input structs — Affects, Enemies, Room Contents
 */

/* Char.Affects input — one per active affect */
typedef struct {
    const char *name;           /* af->skill->name or af->custom_name */
    const char *wnum;           /* af->skill wnum string, NULL if none */
    int duration;               /* remaining ticks, -1 for permanent */
    int estimated_seconds;      /* duration * PULSE_TICK / PULSE_PER_SECOND, -1 if permanent */
    const char *modifier;       /* human-readable, e.g. "+2 strength"; NULL if APPLY_NONE/0 */
    int level;
} sentience_affect_input_t;

/* Char.Enemies input — one per enemy in combat */
typedef struct {
    const char *name;           /* short_descr for NPCs, name for players */
    unsigned long instance_id[2];
    int hp_pct;                 /* 0-100 */
    bool is_primary;            /* true if this is ch->fighting */
    const char *target;         /* "you" or target name */
} sentience_enemy_input_t;

/* Room.Contents entity (item, NPC, or player) */
typedef struct {
    const char *name;
    unsigned long instance_id[2];
    const char *short_desc;     /* NULL for players */
} sentience_room_entity_input_t;

/* Room.Contents door */
typedef struct {
    const char *direction;
    const char *state;          /* "open", "closed", "locked" */
    bool is_locked;
} sentience_room_door_input_t;

/* Room.Contents full snapshot */
typedef struct {
    const sentience_room_entity_input_t *items;
    int num_items;
    const sentience_room_entity_input_t *npcs;
    int num_npcs;
    const sentience_room_entity_input_t *players;
    int num_players;
    const sentience_room_door_input_t *doors;
    int num_doors;
} sentience_room_contents_input_t;

/* Room.Map input — pre-rendered map for GMCP delivery */
typedef struct {
    const char *type;       /* "area" or "wilds" */
    const char *map_text;   /* Pre-rendered map string with MUD color codes */
    int width;              /* Character columns */
    int height;             /* Character rows */
} sentience_room_map_input_t;

json_t *sentience_build_room_json(const sentience_room_input_t *data);

/*
 * Phase 3 JSON builder functions.
 */
json_t *sentience_build_client_ready_capabilities_json(void);
json_t *sentience_build_client_ready_state_json(int tick_rate, int pulse_per_second);
json_t *sentience_build_affects_json(const sentience_affect_input_t *affects, int num_affects);
json_t *sentience_build_enemies_json(const sentience_enemy_input_t *enemies, int num_enemies,
                                      long self_hp, long self_max_hp);
json_t *sentience_build_room_contents_json(const sentience_room_contents_input_t *data);
json_t *sentience_build_room_map(const sentience_room_map_input_t *input);

/* ── Channel.Message ──────────────────────────────────────────────── */

typedef struct {
    const char *channel;      /* Channel ID (e.g. "gossip", "say") */
    const char *sender;       /* Sender name ("" for system messages) */
    const char *text;         /* Message text (color-stripped) */
    long        timestamp;    /* Unix timestamp */
    const char *tell_target;  /* Recipient name (NULL if not directed) */
} sentience_channel_message_input_t;

json_t *sentience_build_channel_message(const sentience_channel_message_input_t *input);

/*
 * Send a pre-built JSON object as a named GMCP package to a descriptor.
 * Takes ownership of the json_t (decrefs after sending).
 */
void sentience_send_package(descriptor_t *d, const char *package, json_t *json);

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

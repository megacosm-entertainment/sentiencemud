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
    SENTIENCE_DIRTY_INVENTORY   = (1 << 8),
    SENTIENCE_DIRTY_EQUIPMENT   = (1 << 9),
    SENTIENCE_DIRTY_ABILITIES   = (1 << 10),
    SENTIENCE_DIRTY_REPUTATIONS = (1 << 11),
    SENTIENCE_DIRTY_CHURCH      = (1 << 12),
    SENTIENCE_DIRTY_RACE        = (1 << 13),
} sentience_dirty_t;

#define SENTIENCE_DIRTY_ALL  0x3FFF

#define SENTIENCE_PACKAGE_VERSION 1

/* Maximum classes a character can have simultaneously */
#define SENTIENCE_MAX_CLASSES     32
#define SENTIENCE_MAX_TRAITS      64
#define SENTIENCE_MAX_TITLES      16
#define SENTIENCE_MAX_RACE_SKILLS 16

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

    /* Phase 4: Inventory/Equipment fingerprint */
    int inventory_count;
    int equipment_count;

    /* Phase 4: Abilities fingerprint (set to -1 to force rebuild) */
    int abilities_count;

    /* Phase 4: Reputations fingerprint (set to -1 to force rebuild) */
    int reputation_count;

    /* Phase 4: Church change detection */
    bool has_church;
    long church_uid;

    /* Phase 4: Race change detection */
    int16_t race_uid;

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

typedef struct {
    const char *keyword;
    const char *display;
    bool is_default;
} sentience_class_title_t;

typedef struct {
    const char *id;
    const char *name;
    const char *description;
    const char *category;
    const char *type;           /* "bool", "int", "string" */
    const char *source;         /* "personal", "class", "race" */
    bool value_bool;
    int value_int;
    const char *value_string;
} sentience_trait_t;

typedef struct {
    const char *id;
    const char *name;
    const char *description;
    bool playable;
    bool starting;
    const char *size;
    int stats[5];               /* STR/INT/WIS/DEX/CON */
    int max_stats[5];
    int max_vitals[3];          /* HP/Mana/Move */
    int num_skills;
    const char *skills[SENTIENCE_MAX_RACE_SKILLS];
    const char *resistances;    /* space-separated names */
    const char *vulnerabilities;
    const char *immunities;
    const char *affects;
    const char *remort_into;    /* NULL if no remort */
    int num_traits;
    sentience_trait_t traits[SENTIENCE_MAX_TRAITS];
} sentience_race_info_t;

typedef struct {
    const char *id;
    const char *name;
    int level;
    bool is_primary;
    /* New fields */
    int max_level;
    const char *type;           /* class type name */
    const char *flags;          /* space-separated flag names */
    const char *primary_stat;   /* stat name */
    int hp_min, hp_max;
    bool gains_mana;
    const char *description;
    long xp;
    const char *active_title;   /* chosen title keyword or NULL */
    int num_titles;
    sentience_class_title_t titles[SENTIENCE_MAX_TITLES];
    const char *action_label;   /* NULL for primary class */
    const char *action_cmd;     /* NULL for primary class */
} sentience_identity_class_t;

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
    sentience_identity_class_t classes[SENTIENCE_MAX_CLASSES];
    int num_traits;
    sentience_trait_t traits[SENTIENCE_MAX_TRAITS];
    sentience_race_info_t race_info;
} sentience_identity_input_t;

json_t *sentience_build_identity_json(const sentience_identity_input_t *data);

#define SENTIENCE_MAX_PREFERENCES 64

typedef struct {
    const char *key;
    const char *category;   /* "toggle", "channel", "prompt", "display", "gmcp" */
    const char *type;       /* "bool", "int", "string", "bitfield" */
    const char *source;     /* "default", "account", "character" */
    const char *label;      /* human-readable display label */
    bool value_bool;
    int value_int;
    const char *value_string;
} sentience_pref_entry_t;

typedef struct {
    int num_prefs;
    sentience_pref_entry_t prefs[SENTIENCE_MAX_PREFERENCES];
} sentience_preferences_input_t;

json_t *sentience_build_preferences_json(const sentience_preferences_input_t *input);

#define SENTIENCE_MAX_INVENTORY    128
#define SENTIENCE_MAX_ITEM_ACTIONS 8
#define SENTIENCE_MAX_ITEM_FLAGS   8

typedef struct {
    const char *label;
    const char *cmd;
} sentience_item_action_t;

typedef struct {
    const char *name;           /* short_descr */
    const char *keywords;       /* full obj->name string for display */
    const char *keyword;        /* first keyword with N. prefix if needed */
    unsigned long id[2];        /* instance ID — emitted as JSON array [id0, id1] */
    const char *item_type;      /* item_type_info name */
    int condition;              /* integer 0-100 */
    const char *condition_label;/* damage table name string */
    int level;
    int weight;
    int item_count;             /* container: number of visible items inside, else 0 */
    int num_flags;
    const char *flags[SENTIENCE_MAX_ITEM_FLAGS];
    int num_actions;
    sentience_item_action_t actions[SENTIENCE_MAX_ITEM_ACTIONS];
} sentience_inventory_item_t;

typedef struct {
    int num_items;
    sentience_inventory_item_t items[SENTIENCE_MAX_INVENTORY];
    int capacity_max_weight;
    int capacity_current_weight;
    int capacity_max_items;
    int capacity_current_items;
    int capacity_coin_weight;
} sentience_inventory_input_t;

json_t *sentience_build_inventory_json(const sentience_inventory_input_t *input);

#define SENTIENCE_MAX_EQUIPMENT_SLOTS 51

typedef struct {
    int slot_id;
    const char *slot_name;      /* where_name[slot] */
    bool occupied;
    /* Fields below only valid if occupied == true */
    const char *item_name;      /* short_descr */
    const char *keywords;       /* full obj->name */
    const char *keyword;        /* first keyword for action cmds */
    unsigned long id[2];        /* instance ID */
    const char *item_type;
    int condition;              /* integer 0-100 */
    const char *condition_label;
    int level;
    int num_flags;
    const char *flags[SENTIENCE_MAX_ITEM_FLAGS];
    int num_actions;
    sentience_item_action_t actions[SENTIENCE_MAX_ITEM_ACTIONS];
} sentience_equipment_slot_t;

typedef struct {
    int num_slots;
    sentience_equipment_slot_t slots[SENTIENCE_MAX_EQUIPMENT_SLOTS];
} sentience_equipment_input_t;

json_t *sentience_build_equipment_json(const sentience_equipment_input_t *input);

/*
 * Abilities builder input — unified skills, spells, and songs.
 */
#define SENTIENCE_MAX_ABILITIES 256

typedef struct {
    const char *name;
    const char *type;           /* "skill", "spell", "song" */
    bool available;
    int rating;
    int modifier;
    int mana;
    int level;
    const char *target;         /* "offensive", "defensive", "self", "object", "passive", "ignore" */
    bool can_practice;
    int learn_rate;
    int num_actions;
    sentience_item_action_t actions[2];  /* Cast/Play/Use at most */
} sentience_ability_t;

typedef struct {
    int num_abilities;
    sentience_ability_t abilities[SENTIENCE_MAX_ABILITIES];
} sentience_abilities_input_t;

json_t *sentience_build_abilities_json(const sentience_abilities_input_t *input);

/*
 * Reputations builder input — faction standings.
 */
#define SENTIENCE_MAX_REPUTATIONS 32

typedef struct {
    const char *name;
    const char *rank;
    const char *rank_color;
    int points;
    int paragon_level;
    const char *max_rank;
} sentience_reputation_t;

typedef struct {
    int num_reputations;
    sentience_reputation_t reputations[SENTIENCE_MAX_REPUTATIONS];
} sentience_reputations_input_t;

json_t *sentience_build_reputations_json(const sentience_reputations_input_t *input);

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
 * Send current GMCP preference state to the client.
 * Called on login and after preference updates from Sentience.Client.Preferences.
 */
void sentience_send_client_preferences(descriptor_t *d);

/*
 * Game-loop entry point. Called from gmcp_update() for each descriptor.
 * Compares current character state with cache, sends dirty packages.
 */
void sentience_gmcp_update(descriptor_t *d);

/*
 * Reset cache to force full resend (e.g., on login or reconnect).
 */
void sentience_gmcp_cache_reset(sentience_gmcp_cache_t *cache);

/* ----- Client.Layout storage ----- */
#define LAYOUT_NAME_MAX   32
#define LAYOUT_MAX_COUNT  5
#define LAYOUT_MAX_SIZE   (16 * 1024)  /* 16 KB per layout blob */

typedef struct web_client_layout {
    char name[LAYOUT_NAME_MAX + 1];
    json_t *layout;                      /* Opaque FlexLayout model JSON */
    struct web_client_layout *next;
} web_client_layout_t;

/* Layout storage helpers (gmcp_sentience.c) */
web_client_layout_t *layout_find(web_client_layout_t *list, const char *name);
int                  layout_count(web_client_layout_t *list);
void                 layout_free_all(web_client_layout_t **list);

/* Client.Layout handler and sender */
void sentience_handle_client_layout(descriptor_t *d, const char *json_str);
void sentience_send_layout_restore(descriptor_t *d, const char *name, json_t *layout);

/* Auth.QRCode builder and sender */
json_t *sentience_build_auth_qrcode_json(const char *purpose, const char *image,
                                          const char *uri, long expires_at);
void sentience_send_auth_qrcode(descriptor_t *d, const char *image_data_url,
                                 const char *uri, long expires_at);

#ifdef BUILD_TESTS
bool test_layout_name_is_valid(const char *name);
#endif

#endif /* GMCP_SENTIENCE_H */

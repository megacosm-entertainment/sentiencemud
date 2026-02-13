/**
 * preferences.c - Account-level preference framework implementation
 *
 * Generic key-value preference system for account-level settings that
 * characters inherit unless explicitly overridden. Supports booleans,
 * integers, strings, and bitfields.
 *
 * See preferences.h for full documentation.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "../merc.h"
#include "../interp.h"
#include "../tables.h"
#include "../io/json/json_game_settings.h"
#include "preferences.h"

/***************************************************************************
 * Default Channel Table                                                   *
 ***************************************************************************/

const ACCT_CHANNEL_DEF acct_channel_defaults[] = {
    { "gossip",    "channel_gossip"   },
    { "ooc",       "channel_ooc"      },
    { "music",     "channel_music"    },
    { "auction",   "channel_auction"  },
    { "yell",      "channel_yell"     },
    { "quote",     "channel_quote"    },
    { "helper",    "channel_helper"   },
    { "ct",        "channel_ct"       },
    { "gq",        "channel_gq"       },
    { "autowar",   "channel_autowar"  },
    { "announce",  "channel_announce" },
    { "hints",     "channel_hints"    },
    { "flaming",   "channel_flaming"  },
    { NULL, NULL }
};

/***************************************************************************
 * Name Lookup Tables                                                      *
 ***************************************************************************/

const char *pref_category_names[] = {
    "toggle",       /* PREF_CAT_TOGGLE   */
    "channel",      /* PREF_CAT_CHANNEL  */
    "prompt",       /* PREF_CAT_PROMPT   */
    "display",      /* PREF_CAT_DISPLAY  */
    NULL
};

const char *pref_type_names[] = {
    "bool",         /* PREF_TYPE_BOOL     */
    "int",          /* PREF_TYPE_INT      */
    "string",       /* PREF_TYPE_STRING   */
    "bitfield",     /* PREF_TYPE_BITFIELD */
    NULL
};

/**
 * pref_category_name - Get the name for a category constant
 */
const char *pref_category_name(int category)
{
    if (category < 0 || category >= PREF_CAT_MAX)
        return "unknown";
    return pref_category_names[category];
}

/**
 * pref_category_lookup - Look up a category by name
 *
 * @return  Category constant, or -1 if not found
 */
int pref_category_lookup(const char *name)
{
    for (int i = 0; i < PREF_CAT_MAX; i++) {
        if (!str_cmp(name, pref_category_names[i]))
            return i;
    }
    return -1;
}

/**
 * pref_type_name - Get the name for a type constant
 */
const char *pref_type_name(int type)
{
    if (type < 0 || type > PREF_TYPE_BITFIELD)
        return "unknown";
    return pref_type_names[type];
}

/**
 * pref_type_lookup - Look up a type by name
 *
 * @return  Type constant, or -1 if not found
 */
int pref_type_lookup(const char *name)
{
    if (!str_cmp(name, "bool"))     return PREF_TYPE_BOOL;
    if (!str_cmp(name, "int"))      return PREF_TYPE_INT;
    if (!str_cmp(name, "string"))   return PREF_TYPE_STRING;
    if (!str_cmp(name, "bitfield")) return PREF_TYPE_BITFIELD;
    return -1;
}

/***************************************************************************
 * Memory Management                                                       *
 ***************************************************************************/

/**
 * new_pref_entry - Allocate a new preference entry
 *
 * Uses alloc_perm for persistent allocation consistent with the rest
 * of the codebase.
 */
PREF_ENTRY *new_pref_entry(void)
{
    PREF_ENTRY *entry = (PREF_ENTRY *)alloc_perm(sizeof(PREF_ENTRY));
    entry->next     = NULL;
    entry->key      = str_empty;
    entry->category = PREF_CAT_TOGGLE;
    entry->type     = PREF_TYPE_BOOL;
    entry->min_rank = STAFF_PLAYER;
    entry->val.b    = false;
    return entry;
}

/**
 * free_pref_entry - Free a single preference entry
 *
 * Frees the key and string value (if applicable).
 */
void free_pref_entry(PREF_ENTRY *entry)
{
    if (!entry) return;
    if (entry->key && entry->key != str_empty)
        free_string(entry->key);
    if (entry->type == PREF_TYPE_STRING && entry->val.str
        && entry->val.str != str_empty)
        free_string(entry->val.str);
    /* alloc_perm memory is not individually freed */
}

/**
 * free_pref_list - Free an entire preference list
 */
void free_pref_list(PREF_ENTRY *list)
{
    PREF_ENTRY *next;
    while (list) {
        next = list->next;
        free_pref_entry(list);
        list = next;
    }
}

/***************************************************************************
 * Core Operations                                                         *
 ***************************************************************************/

/**
 * pref_find - Find a preference by key in a list
 *
 * Case-insensitive key comparison.
 *
 * @param list  Head of the preference list
 * @param key   Key to search for
 * @return      Pointer to the entry, or NULL if not found
 */
PREF_ENTRY *pref_find(PREF_ENTRY *list, const char *key)
{
    for (PREF_ENTRY *p = list; p; p = p->next) {
        if (!str_cmp(p->key, key))
            return p;
    }
    return NULL;
}

/**
 * find_or_create - Find an entry by key, or create a new one at list head
 *
 * Internal helper used by all set functions.
 */
static PREF_ENTRY *find_or_create(PREF_ENTRY **list, int category,
                                   const char *key, int type)
{
    PREF_ENTRY *p = pref_find(*list, key);
    if (p) {
        /* If changing type, free old string value */
        if (p->type == PREF_TYPE_STRING && type != PREF_TYPE_STRING
            && p->val.str && p->val.str != str_empty)
            free_string(p->val.str);
        p->type     = type;
        p->category = category;
        return p;
    }

    p = new_pref_entry();
    p->key      = str_dup(key);
    p->category = category;
    p->type     = type;
    p->next     = *list;
    *list       = p;
    return p;
}

/**
 * pref_set_bool - Set a boolean preference
 */
void pref_set_bool(PREF_ENTRY **list, int category,
                   const char *key, bool value)
{
    PREF_ENTRY *p = find_or_create(list, category, key, PREF_TYPE_BOOL);
    p->val.b = value;
}

/**
 * pref_set_int - Set an integer preference
 */
void pref_set_int(PREF_ENTRY **list, int category,
                  const char *key, int value)
{
    PREF_ENTRY *p = find_or_create(list, category, key, PREF_TYPE_INT);
    p->val.i = value;
}

/**
 * pref_set_string - Set a string preference
 */
void pref_set_string(PREF_ENTRY **list, int category,
                     const char *key, const char *value)
{
    PREF_ENTRY *p = find_or_create(list, category, key, PREF_TYPE_STRING);
    p->val.str = str_dup(value ? value : "");
}

/**
 * pref_set_bitfield - Set a bitfield preference
 */
void pref_set_bitfield(PREF_ENTRY **list, int category,
                       const char *key, long bits)
{
    PREF_ENTRY *p = find_or_create(list, category, key, PREF_TYPE_BITFIELD);
    p->val.bits = bits;
}

/**
 * pref_remove - Remove a preference by key
 *
 * @return  true if found and removed
 */
bool pref_remove(PREF_ENTRY **list, const char *key)
{
    PREF_ENTRY *prev = NULL;
    for (PREF_ENTRY *p = *list; p; prev = p, p = p->next) {
        if (!str_cmp(p->key, key)) {
            if (prev)
                prev->next = p->next;
            else
                *list = p->next;
            free_pref_entry(p);
            return true;
        }
    }
    return false;
}

/**
 * pref_count - Count entries in a preference list
 */
int pref_count(PREF_ENTRY *list)
{
    int n = 0;
    for (PREF_ENTRY *p = list; p; p = p->next)
        n++;
    return n;
}

/**
 * pref_count_category - Count entries in a specific category
 */
int pref_count_category(PREF_ENTRY *list, int category)
{
    int n = 0;
    for (PREF_ENTRY *p = list; p; p = p->next) {
        if (p->category == category)
            n++;
    }
    return n;
}

/***************************************************************************
 * Resolved Lookups (Account + Character Inheritance)                      *
 ***************************************************************************/

/**
 * get_char_prefs - Get the preference override list for a character
 *
 * Internal helper. Returns NULL for NPCs or characters without pc_data.
 */
static PREF_ENTRY *get_char_prefs(CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata)
        return NULL;
    return ch->pcdata->preferences;
}

/**
 * pref_get_bool - Get a boolean preference with inheritance
 *
 * Checks character overrides first, then account, then default.
 */
bool pref_get_bool(ACCOUNT_DATA *account, CHAR_DATA *ch,
                   const char *key, bool default_val)
{
    /* Character override */
    PREF_ENTRY *p = pref_find(get_char_prefs(ch), key);
    if (p && p->type == PREF_TYPE_BOOL)
        return p->val.b;

    /* Account preference */
    if (account) {
        p = pref_find(account->preferences, key);
        if (p && p->type == PREF_TYPE_BOOL)
            return p->val.b;
    }

    return default_val;
}

/**
 * pref_get_int - Get an integer preference with inheritance
 */
int pref_get_int(ACCOUNT_DATA *account, CHAR_DATA *ch,
                 const char *key, int default_val)
{
    PREF_ENTRY *p = pref_find(get_char_prefs(ch), key);
    if (p && p->type == PREF_TYPE_INT)
        return p->val.i;

    if (account) {
        p = pref_find(account->preferences, key);
        if (p && p->type == PREF_TYPE_INT)
            return p->val.i;
    }

    return default_val;
}

/**
 * pref_get_string - Get a string preference with inheritance
 *
 * @return  The string value, or default_val. Caller must not free.
 */
const char *pref_get_string(ACCOUNT_DATA *account, CHAR_DATA *ch,
                            const char *key, const char *default_val)
{
    PREF_ENTRY *p = pref_find(get_char_prefs(ch), key);
    if (p && p->type == PREF_TYPE_STRING)
        return p->val.str;

    if (account) {
        p = pref_find(account->preferences, key);
        if (p && p->type == PREF_TYPE_STRING)
            return p->val.str;
    }

    return default_val;
}

/**
 * pref_get_bitfield - Get a bitfield preference with inheritance
 */
long pref_get_bitfield(ACCOUNT_DATA *account, CHAR_DATA *ch,
                       const char *key, long default_val)
{
    PREF_ENTRY *p = pref_find(get_char_prefs(ch), key);
    if (p && p->type == PREF_TYPE_BITFIELD)
        return p->val.bits;

    if (account) {
        p = pref_find(account->preferences, key);
        if (p && p->type == PREF_TYPE_BITFIELD)
            return p->val.bits;
    }

    return default_val;
}

/**
 * pref_is_overridden - Check if a character has overridden a preference
 */
bool pref_is_overridden(CHAR_DATA *ch, const char *key)
{
    return pref_find(get_char_prefs(ch), key) != NULL;
}

/***************************************************************************
 * Snapshot Functions                                                       *
 *                                                                         *
 * Capture current character settings into a preference list.              *
 ***************************************************************************/

/**
 * pref_snapshot_toggles - Capture toggle settings to a pref list
 *
 * Reads all pc_set_table[] entries and writes them as boolean prefs.
 * For inverted flags (e.g. COMM_NOBATTLESPAM for "battlespam"), the
 * stored value is the logical state (true = enabled), not the raw bit.
 */
void pref_snapshot_toggles(CHAR_DATA *ch, PREF_ENTRY **list)
{
    if (!ch || IS_NPC(ch))
        return;

    for (int i = 0; pc_set_table[i].name; i++) {
        bool is_set = false;

        if (pc_set_table[i].vector)
            is_set = IS_SET(ch->act[0], pc_set_table[i].vector);
        else if (pc_set_table[i].vector2)
            is_set = IS_SET(ch->act[1], pc_set_table[i].vector2);
        else if (pc_set_table[i].vector_comm)
            is_set = IS_SET(ch->comm, pc_set_table[i].vector_comm);

        /* Inverted flags: bit set = feature disabled */
        if (pc_set_table[i].inverted)
            is_set = !is_set;

        pref_set_bool(list, PREF_CAT_TOGGLE, pc_set_table[i].name, is_set);

        /* Copy min_rank from the table definition */
        PREF_ENTRY *p = pref_find(*list, pc_set_table[i].name);
        if (p) p->min_rank = pc_set_table[i].min_rank;
    }
}

/**
 * pref_snapshot_channels - Capture channel flags to a pref list
 *
 * Captures channel muting (COMM_NO* flags) and per-channel display
 * flags (channel_flags bitfield) as individual boolean prefs.
 */
void pref_snapshot_channels(CHAR_DATA *ch, PREF_ENTRY **list)
{
    if (!ch || IS_NPC(ch))
        return;

    /* Channel muting — stored as "channel_gossip" = true/false */
    struct {
        const char *name;
        long        flag;
    } channel_mute[] = {
        { "channel_gossip",    COMM_NOGOSSIP   },
        { "channel_music",     COMM_NOMUSIC    },
        { "channel_auction",   COMM_NOAUCTION  },
        { "channel_ooc",       COMM_NO_OOC     },
        { "channel_yell",      COMM_NOYELL     },
        { "channel_quote",     COMM_NOQUOTE    },
        { "channel_helper",    COMM_NOHELPER   },
        { "channel_ct",        COMM_NOCT       },
        { "channel_gq",        COMM_NOGQ       },
        { "channel_autowar",   COMM_NOAUTOWAR  },
        { "channel_announce",  COMM_NOANNOUNCE },
        { "channel_hints",     COMM_NOHINTS    },
        { "channel_flaming",   COMM_NO_FLAMING },
        { NULL, 0 }
    };

    for (int i = 0; channel_mute[i].name; i++) {
        /* Inverted: COMM_NO* set = channel muted = false */
        bool enabled = !IS_SET(ch->comm, channel_mute[i].flag);
        pref_set_bool(list, PREF_CAT_CHANNEL, channel_mute[i].name, enabled);
    }

    /* Per-channel display flags */
    if (ch->pcdata) {
        pref_set_bitfield(list, PREF_CAT_CHANNEL,
                          "channel_display_flags", ch->pcdata->channel_flags);
    }
}

/**
 * pref_snapshot_prompt - Capture prompt string to a pref list
 */
void pref_snapshot_prompt(CHAR_DATA *ch, PREF_ENTRY **list)
{
    if (!ch || IS_NPC(ch))
        return;

    if (!IS_NULLSTR(ch->prompt)) {
        pref_set_string(list, PREF_CAT_PROMPT, "prompt", ch->prompt);
    }
}

/**
 * pref_snapshot_wimpy - Capture wimpy setting to a pref list
 *
 * Wimpy is an integer value (HP threshold for auto-flee).
 */
void pref_snapshot_wimpy(CHAR_DATA *ch, PREF_ENTRY **list)
{
    if (!ch || IS_NPC(ch))
        return;

    pref_set_int(list, PREF_CAT_TOGGLE, "wimpy", ch->wimpy);
}

/**
 * pref_migrate_character - One-time migration of bitfield state to preferences
 *
 * For characters that have no saved preferences yet (legacy characters),
 * snapshots their current toggle, channel, prompt, wimpy, and scroll
 * settings into explicit character-level preferences. This preserves
 * their existing configuration while enabling the preference inheritance
 * system to work correctly.
 *
 * After migration, account preferences will only override settings the
 * character hasn't explicitly set, and the old bitfields become secondary
 * to the preferences system.
 *
 * This function is idempotent — it does nothing if the character already
 * has any preferences saved.
 *
 * @param ch  The character to migrate
 * @return    true if migration was performed, false if skipped
 */
bool pref_migrate_character(CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata)
        return false;

    /* Skip if character already has preferences */
    if (ch->pcdata->preferences != NULL)
        return false;

    /* Snapshot all current bitfield/field state into character preferences */
    pref_snapshot_toggles(ch, &ch->pcdata->preferences);
    pref_snapshot_channels(ch, &ch->pcdata->preferences);
    pref_snapshot_prompt(ch, &ch->pcdata->preferences);
    pref_snapshot_wimpy(ch, &ch->pcdata->preferences);

    /* Scroll (lines per page) */
    if (ch->lines > 0)
        pref_set_int(&ch->pcdata->preferences, PREF_CAT_DISPLAY,
                     "scroll", ch->lines + 2);
    else
        pref_set_int(&ch->pcdata->preferences, PREF_CAT_DISPLAY,
                     "scroll", 0);

    int count = pref_count(ch->pcdata->preferences);
    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
                  "pref_migrate: Migrated %d preferences for %s",
                  count, ch->name);

    return true;
}

/* Channel mute table used by pref_apply_game_defaults, pref_check_channel,
 * and display/toggle code. Defined here so it's available to all. */
static const struct {
    const char *name;
    long        flag;
} channel_mute_table[] = {
    { "gossip",    COMM_NOGOSSIP   },
    { "ooc",       COMM_NO_OOC     },
    { "music",     COMM_NOMUSIC    },
    { "auction",   COMM_NOAUCTION  },
    { "yell",      COMM_NOYELL     },
    { "quote",     COMM_NOQUOTE    },
    { "helper",    COMM_NOHELPER   },
    { "ct",        COMM_NOCT       },
    { "gq",        COMM_NOGQ       },
    { "autowar",   COMM_NOAUTOWAR  },
    { "announce",  COMM_NOANNOUNCE },
    { "hints",     COMM_NOHINTS    },
    { "flaming",   COMM_NO_FLAMING },
    { NULL, 0 }
};

/***************************************************************************
 * Apply Game Defaults and Account Preferences                             *
 ***************************************************************************/

/**
 * pref_apply_game_defaults - Set all toggle defaults from pc_set_table
 *
 * Iterates pc_set_table[] and sets every entry whose default_state is
 * SETTING_ON to the appropriate bitfield on the character. Entries with
 * default_state SETTING_OFF or whose min_rank exceeds the character's
 * staff_rank are skipped.
 *
 * This replaces the manual SET_BIT loop previously in nanny.c character
 * creation and should be called once when a new character is created,
 * before pref_apply_to_character().
 *
 * @param ch  The newly created character
 */
void pref_apply_game_defaults(CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch))
        return;

    for (int i = 0; pc_set_table[i].name; i++) {
        if (pc_set_table[i].min_rank > STAFF_PLAYER
            && (!ch->pcdata || ch->pcdata->staff_rank < pc_set_table[i].min_rank))
            continue;

        long *field;
        long vector;

        if (pc_set_table[i].vector != 0) {
            vector = pc_set_table[i].vector;
            field = &ch->act[0];
        } else if (pc_set_table[i].vector2 != 0) {
            vector = pc_set_table[i].vector2;
            field = &ch->act[1];
        } else if (pc_set_table[i].vector_comm != 0) {
            vector = pc_set_table[i].vector_comm;
            field = &ch->comm;
        } else {
            continue;
        }

        if (pc_set_table[i].default_state == SETTING_ON) {
            if (pc_set_table[i].inverted)
                REMOVE_BIT(*field, vector);
            else
                SET_BIT(*field, vector);
        } else {
            if (pc_set_table[i].inverted)
                SET_BIT(*field, vector);
            else
                REMOVE_BIT(*field, vector);
        }
    }

    /* Default channel states for new characters.
     * Channels listed here are muted by default; account preferences
     * applied afterwards can override these. */
    static const struct { long flag; } default_muted_channels[] = {
        { COMM_NO_OOC     },
        { COMM_NO_FLAMING },
        { 0 }
    };

    for (int i = 0; default_muted_channels[i].flag; i++)
        SET_BIT(ch->comm, default_muted_channels[i].flag);

    /* Apply game_settings preference overrides on top of hardcoded defaults.
     * This lets staff change default preferences at runtime without code
     * changes. The hierarchy is:
     *     pc_set_table (hardcoded) → game_settings.pref_defaults → account → character */
    for (PREF_ENTRY *gp = game_settings.pref_defaults; gp; gp = gp->next) {
        switch (gp->category) {
        case PREF_CAT_TOGGLE:
            if (gp->type == PREF_TYPE_INT && !str_cmp(gp->key, "wimpy")) {
                ch->wimpy = URANGE(0, gp->val.i, ch->max_hit);
                break;
            }
            if (gp->type != PREF_TYPE_BOOL)
                break;
            for (int i = 0; pc_set_table[i].name; i++) {
                if (str_cmp(pc_set_table[i].name, gp->key))
                    continue;
                if (pc_set_table[i].min_rank > STAFF_PLAYER
                    && (!ch->pcdata || ch->pcdata->staff_rank < pc_set_table[i].min_rank))
                    break;

                long *field;
                long vector;
                if (pc_set_table[i].vector != 0) {
                    vector = pc_set_table[i].vector;
                    field = &ch->act[0];
                } else if (pc_set_table[i].vector2 != 0) {
                    vector = pc_set_table[i].vector2;
                    field = &ch->act[1];
                } else if (pc_set_table[i].vector_comm != 0) {
                    vector = pc_set_table[i].vector_comm;
                    field = &ch->comm;
                } else {
                    break;
                }

                bool want_on = gp->val.b;
                if (pc_set_table[i].inverted)
                    want_on = !want_on;

                if (want_on)
                    SET_BIT(*field, vector);
                else
                    REMOVE_BIT(*field, vector);
                break;
            }
            break;

        case PREF_CAT_CHANNEL:
            if (gp->type == PREF_TYPE_BOOL) {
                /* Channel keys are stored as "channel_gossip" etc. but
                 * channel_mute_table uses bare names like "gossip".
                 * Strip the "channel_" prefix for matching. */
                const char *chan_name = gp->key;
                if (!str_prefix("channel_", chan_name))
                    chan_name += 8;

                for (int i = 0; channel_mute_table[i].name; i++) {
                    if (str_cmp(channel_mute_table[i].name, chan_name))
                        continue;
                    if (gp->val.b)
                        REMOVE_BIT(ch->comm, channel_mute_table[i].flag);
                    else
                        SET_BIT(ch->comm, channel_mute_table[i].flag);
                    break;
                }
            }
            break;

        case PREF_CAT_PROMPT:
            if (gp->type == PREF_TYPE_STRING && !IS_NULLSTR(gp->val.str)
                && !str_cmp(gp->key, "prompt")) {
                if (ch->prompt)
                    free_string(ch->prompt);
                ch->prompt = str_dup(gp->val.str);
            }
            break;

        case PREF_CAT_DISPLAY:
            if (gp->type == PREF_TYPE_INT && !str_cmp(gp->key, "scroll")) {
                int val = gp->val.i;
                if (val == 0)
                    ch->lines = 0;
                else
                    ch->lines = URANGE(10, val, 100) - 2;
            }
            break;

        default:
            break;
        }
    }
}

/**
 * pref_apply_to_character - Apply account prefs to a character on login
 *
 * For each account preference, if the character does not have an explicit
 * override, apply the account value to the character's live bitfields and
 * data. This is called during login after the character is loaded.
 *
 * @param account     The account to inherit from
 * @param ch          The character to apply settings to
 * @param char_prefs  The character's override list (may be NULL)
 */
void pref_apply_to_character(ACCOUNT_DATA *account, CHAR_DATA *ch,
                             PREF_ENTRY *char_prefs)
{
    if (!account || !ch || IS_NPC(ch))
        return;

    for (PREF_ENTRY *ap = account->preferences; ap; ap = ap->next) {
        /* Skip if character has an override */
        if (pref_find(char_prefs, ap->key))
            continue;

        switch (ap->category) {
        case PREF_CAT_TOGGLE:
            /* Special case: wimpy is an integer, not a boolean */
            if (ap->type == PREF_TYPE_INT && !str_cmp(ap->key, "wimpy")) {
                ch->wimpy = URANGE(0, ap->val.i, ch->max_hit);
                break;
            }

            if (ap->type != PREF_TYPE_BOOL)
                break;
            /* Find the toggle in pc_set_table to map back to bitfields */
            for (int i = 0; pc_set_table[i].name; i++) {
                if (str_cmp(pc_set_table[i].name, ap->key))
                    continue;

                /* Check staff-only toggles */
                if (pc_set_table[i].min_rank > STAFF_PLAYER
                    && (!ch->pcdata || ch->pcdata->staff_rank < pc_set_table[i].min_rank))
                    break;

                bool want = ap->val.b;
                /* Invert for NO_* flags */
                if (pc_set_table[i].inverted)
                    want = !want;

                if (pc_set_table[i].vector) {
                    if (want)
                        SET_BIT(ch->act[0], pc_set_table[i].vector);
                    else
                        REMOVE_BIT(ch->act[0], pc_set_table[i].vector);
                } else if (pc_set_table[i].vector2) {
                    if (want)
                        SET_BIT(ch->act[1], pc_set_table[i].vector2);
                    else
                        REMOVE_BIT(ch->act[1], pc_set_table[i].vector2);
                } else if (pc_set_table[i].vector_comm) {
                    if (want)
                        SET_BIT(ch->comm, pc_set_table[i].vector_comm);
                    else
                        REMOVE_BIT(ch->comm, pc_set_table[i].vector_comm);
                }
                break;
            }
            break;

        case PREF_CAT_CHANNEL:
            if (ap->type == PREF_TYPE_BOOL) {
                /* Channel muting prefs */
                struct {
                    const char *name;
                    long        flag;
                } channel_mute[] = {
                    { "channel_gossip",    COMM_NOGOSSIP   },
                    { "channel_music",     COMM_NOMUSIC    },
                    { "channel_auction",   COMM_NOAUCTION  },
                    { "channel_ooc",       COMM_NO_OOC     },
                    { "channel_yell",      COMM_NOYELL     },
                    { "channel_quote",     COMM_NOQUOTE    },
                    { "channel_helper",    COMM_NOHELPER   },
                    { "channel_ct",        COMM_NOCT       },
                    { "channel_gq",        COMM_NOGQ       },
                    { "channel_autowar",   COMM_NOAUTOWAR  },
                    { "channel_announce",  COMM_NOANNOUNCE },
                    { "channel_hints",     COMM_NOHINTS    },
                    { "channel_flaming",   COMM_NO_FLAMING },
                    { NULL, 0 }
                };
                for (int i = 0; channel_mute[i].name; i++) {
                    if (!str_cmp(ap->key, channel_mute[i].name)) {
                        /* Inverted: enabled = remove NO* bit */
                        if (ap->val.b)
                            REMOVE_BIT(ch->comm, channel_mute[i].flag);
                        else
                            SET_BIT(ch->comm, channel_mute[i].flag);
                        break;
                    }
                }
            } else if (ap->type == PREF_TYPE_BITFIELD
                       && !str_cmp(ap->key, "channel_display_flags")) {
                if (ch->pcdata)
                    ch->pcdata->channel_flags = ap->val.bits;
            }
            break;

        case PREF_CAT_PROMPT:
            if (ap->type == PREF_TYPE_STRING
                && !str_cmp(ap->key, "prompt")
                && !IS_NULLSTR(ap->val.str)) {
                if (ch->prompt)
                    free_string(ch->prompt);
                ch->prompt = str_dup(ap->val.str);
            }
            break;

        case PREF_CAT_DISPLAY:
            if (ap->type == PREF_TYPE_INT && !str_cmp(ap->key, "scroll")) {
                int val = ap->val.i;
                if (val == 0)
                    ch->lines = 0;
                else
                    ch->lines = URANGE(10, val, 100) - 2;
            }
            break;
        }
    }
}

/***************************************************************************
 * Source Tracking                                                         *
 ***************************************************************************/

const char *pref_source_names[] = {
    "default",      /* PREF_SOURCE_DEFAULT   */
    "account",      /* PREF_SOURCE_ACCOUNT   */
    "character",    /* PREF_SOURCE_CHARACTER */
    NULL
};

/**
 * pref_source_name - Get human-readable source name
 */
const char *pref_source_name(int source)
{
    if (source < 0 || source > PREF_SOURCE_CHARACTER)
        return "unknown";
    return pref_source_names[source];
}

/**
 * pref_get_source - Determine where a setting's effective value comes from
 *
 * Checks character overrides first, then account preferences. If neither
 * has the key, returns PREF_SOURCE_DEFAULT.
 *
 * @param account  The account (may be NULL)
 * @param ch       The character (may be NULL)
 * @param key      The preference key
 * @return         PREF_SOURCE_* constant
 */
int pref_get_source(ACCOUNT_DATA *account, CHAR_DATA *ch, const char *key)
{
    /* Character override takes priority */
    if (ch && !IS_NPC(ch) && ch->pcdata
        && pref_find(ch->pcdata->preferences, key))
        return PREF_SOURCE_CHARACTER;

    /* Account preference */
    if (account && pref_find(account->preferences, key))
        return PREF_SOURCE_ACCOUNT;

    return PREF_SOURCE_DEFAULT;
}

/**
 * pref_get_source_name - Get a display name for a source constant
 */
const char *pref_get_source_name(int source)
{
    return pref_source_name(source);
}

/***************************************************************************
 * Reset Preview and Apply                                                 *
 ***************************************************************************/

/**
 * format_pref_value - Format a preference entry's value as a string
 *
 * Internal helper for preview display.
 */
static void format_pref_value(PREF_ENTRY *p, char *buf, size_t len)
{
    if (!p) {
        snprintf(buf, len, "{D(not set){x");
        return;
    }

    switch (p->type) {
    case PREF_TYPE_BOOL:
        snprintf(buf, len, "%s", p->val.b ? "{GOn{x" : "{ROff{x");
        break;
    case PREF_TYPE_INT:
        snprintf(buf, len, "%d", p->val.i);
        break;
    case PREF_TYPE_STRING:
        snprintf(buf, len, "%.30s%s",
                 p->val.str ? p->val.str : "",
                 (p->val.str && strlen(p->val.str) > 30) ? "..." : "");
        break;
    case PREF_TYPE_BITFIELD:
        snprintf(buf, len, "0x%lx", p->val.bits);
        break;
    default:
        snprintf(buf, len, "?");
        break;
    }
}

/**
 * get_toggle_default_value - Get the game default for a toggle setting
 *
 * Looks up a toggle key in pc_set_table[] and returns its default_state.
 *
 * @param key   The setting name
 * @param found Set to true if found in table
 * @return      The default value (true/false)
 */
static bool get_toggle_default_value(const char *key, bool *found)
{
    for (int i = 0; pc_set_table[i].name; i++) {
        if (!str_cmp(pc_set_table[i].name, key)) {
            *found = true;
            return (pc_set_table[i].default_state == SETTING_ON);
        }
    }
    *found = false;
    return false;
}

/**
 * pref_build_reset_preview - Show what would change when resetting
 *
 * For each character override, shows the current value and what it would
 * become (account value or game default). Writes formatted output to
 * the descriptor.
 *
 * @param d  The descriptor to write to
 * @return   Number of settings that would change
 */
int pref_build_reset_preview(DESCRIPTOR_DATA *d)
{
    CHAR_DATA *ch = d->character;
    ACCOUNT_DATA *acct = d->account;
    char buf[MAX_STRING_LENGTH];
    int changes = 0;

    if (!ch || IS_NPC(ch) || !ch->pcdata)
        return 0;

    PREF_ENTRY *char_prefs = ch->pcdata->preferences;

    if (!char_prefs) {
        write_to_buffer(d,
            "\n\r{CThis character has no custom setting overrides.{x\n\r"
            "All settings are already using account or game defaults.\n\r", 0);
        return 0;
    }

    write_to_buffer(d, "\n\r{CSettings that will be reset:{x\n\r", 0);
    write_to_buffer(d,
        "{D+-----+--------------+----------+-------------------+-------------------+{x\n\r", 0);
    write_to_buffer(d,
        "{D|{x {C#{x   {D|{x {CSetting{x      {D|{x {CCategory{x {D|{x {CCurrent Value{x     {D|{x {CNew Value{x         {D|{x\n\r", 0);
    write_to_buffer(d,
        "{D|{x     {D|{x              {D|{x          {D|{x {Y(character){x      {D|{x {W(acct/default){x   {D|{x\n\r", 0);
    write_to_buffer(d,
        "{D+-----+--------------+----------+-------------------+-------------------+{x\n\r", 0);

    for (PREF_ENTRY *cp = char_prefs; cp; cp = cp->next) {
        changes++;

        char current_val[64];
        char new_val[64];

        format_pref_value(cp, current_val, sizeof(current_val));

        /* Determine new value: account pref, or game default */
        PREF_ENTRY *acct_pref = acct ? pref_find(acct->preferences, cp->key) : NULL;
        if (acct_pref) {
            format_pref_value(acct_pref, new_val, sizeof(new_val));
        } else {
            /* Game default */
            if (cp->category == PREF_CAT_TOGGLE && cp->type == PREF_TYPE_BOOL) {
                bool found = false;
                bool def = get_toggle_default_value(cp->key, &found);
                if (found)
                    snprintf(new_val, sizeof(new_val), "%s", def ? "{GOn{x" : "{ROff{x");
                else
                    snprintf(new_val, sizeof(new_val), "{D(game default){x");
            } else if (cp->category == PREF_CAT_TOGGLE && cp->type == PREF_TYPE_INT
                       && !str_cmp(cp->key, "wimpy")) {
                snprintf(new_val, sizeof(new_val), "0");
            } else if (cp->category == PREF_CAT_CHANNEL && cp->type == PREF_TYPE_BOOL) {
                /* Channel defaults: enabled */
                snprintf(new_val, sizeof(new_val), "{GOn{x");
            } else if (cp->category == PREF_CAT_PROMPT && cp->type == PREF_TYPE_STRING) {
                snprintf(new_val, sizeof(new_val), "{D(game default){x");
            } else {
                snprintf(new_val, sizeof(new_val), "{D(game default){x");
            }
        }

        snprintf(buf, sizeof(buf),
            "{D|{x %-3d {D|{x %-12s {D|{x %-8s {D|{x %-17s {D|{x %-17s {D|{x\n\r",
            changes,
            cp->key,
            pref_category_name(cp->category),
            current_val,
            new_val);
        write_to_buffer(d, buf, 0);
    }

    write_to_buffer(d,
        "{D+-----+--------------+----------+-------------------+-------------------+{x\n\r", 0);

    snprintf(buf, sizeof(buf),
        "\n\r{W%d setting%s will be reset.{x\n\r",
        changes, changes == 1 ? "" : "s");
    write_to_buffer(d, buf, 0);

    return changes;
}

/**
 * pref_reset_to_defaults - Clear character overrides and apply account prefs
 *
 * 1. Removes all character preference overrides
 * 2. Resets all toggle settings to their game defaults (via pc_set_table)
 * 3. Resets all channel flags to defaults (all enabled)
 * 4. Applies account preferences on top of defaults
 *
 * @param account  The account
 * @param ch       The character
 */
void pref_reset_to_defaults(ACCOUNT_DATA *account, CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata)
        return;

    /* Step 1: Free character overrides */
    if (ch->pcdata->preferences) {
        free_pref_list(ch->pcdata->preferences);
        ch->pcdata->preferences = NULL;
    }

    /* Step 2: Reset toggles to game defaults */
    for (int i = 0; pc_set_table[i].name; i++) {
        bool want = (pc_set_table[i].default_state == SETTING_ON);

        /* Invert for NO_* flags */
        if (pc_set_table[i].inverted)
            want = !want;

        if (pc_set_table[i].vector) {
            if (want)
                SET_BIT(ch->act[0], pc_set_table[i].vector);
            else
                REMOVE_BIT(ch->act[0], pc_set_table[i].vector);
        } else if (pc_set_table[i].vector2) {
            if (want)
                SET_BIT(ch->act[1], pc_set_table[i].vector2);
            else
                REMOVE_BIT(ch->act[1], pc_set_table[i].vector2);
        } else if (pc_set_table[i].vector_comm) {
            if (want)
                SET_BIT(ch->comm, pc_set_table[i].vector_comm);
            else
                REMOVE_BIT(ch->comm, pc_set_table[i].vector_comm);
        }
    }

    /* Step 2b: Reset wimpy to default (0) */
    ch->wimpy = 0;

    /* Step 3: Reset channel flags — enable all channels by default */
    REMOVE_BIT(ch->comm, COMM_NOGOSSIP);
    REMOVE_BIT(ch->comm, COMM_NOMUSIC);
    REMOVE_BIT(ch->comm, COMM_NOAUCTION);
    REMOVE_BIT(ch->comm, COMM_NO_OOC);
    REMOVE_BIT(ch->comm, COMM_NOYELL);
    REMOVE_BIT(ch->comm, COMM_NOQUOTE);
    REMOVE_BIT(ch->comm, COMM_NOHELPER);
    REMOVE_BIT(ch->comm, COMM_NOCT);
    REMOVE_BIT(ch->comm, COMM_NOGQ);
    REMOVE_BIT(ch->comm, COMM_NOAUTOWAR);
    REMOVE_BIT(ch->comm, COMM_NOANNOUNCE);
    REMOVE_BIT(ch->comm, COMM_NOHINTS);
    REMOVE_BIT(ch->comm, COMM_NO_FLAMING);

    /* Step 4: Apply account preferences on top of defaults */
    if (account) {
        pref_apply_to_character(account, ch, NULL);
    }
}

/***************************************************************************
 * JSON Serialization                                                      *
 ***************************************************************************/

/**
 * prefs_to_json - Serialize a preference list to a JSON array
 *
 * Each entry becomes a JSON object with key, category, type, and value.
 */
json_t *prefs_to_json(PREF_ENTRY *list)
{
    json_t *array = json_array();

    for (PREF_ENTRY *p = list; p; p = p->next) {
        json_t *obj = json_object();

        json_object_set_new(obj, "key",      json_string(p->key));
        json_object_set_new(obj, "category", json_string(pref_category_name(p->category)));
        json_object_set_new(obj, "type",     json_string(pref_type_name(p->type)));

        if (p->min_rank > STAFF_PLAYER)
            json_object_set_new(obj, "min_rank", json_integer(p->min_rank));

        switch (p->type) {
        case PREF_TYPE_BOOL:
            json_object_set_new(obj, "value", json_boolean(p->val.b));
            break;
        case PREF_TYPE_INT:
            json_object_set_new(obj, "value", json_integer(p->val.i));
            break;
        case PREF_TYPE_STRING:
            json_object_set_new(obj, "value",
                json_string(p->val.str ? p->val.str : ""));
            break;
        case PREF_TYPE_BITFIELD:
            json_object_set_new(obj, "value", json_integer(p->val.bits));
            break;
        }

        json_array_append_new(array, obj);
    }

    return array;
}

/**
 * json_to_prefs - Deserialize a JSON array into a preference list
 *
 * @param array  JSON array of preference objects
 * @param list   Pointer to list head (will be populated via prepend)
 * @return       true on success
 */
bool json_to_prefs(json_t *array, PREF_ENTRY **list)
{
    if (!array || !json_is_array(array))
        return false;

    size_t index;
    json_t *elem;

    json_array_foreach(array, index, elem) {
        const char *key = json_string_value(json_object_get(elem, "key"));
        const char *cat = json_string_value(json_object_get(elem, "category"));
        const char *typ = json_string_value(json_object_get(elem, "type"));

        if (!key || !cat || !typ)
            continue;

        int category = pref_category_lookup(cat);
        int type     = pref_type_lookup(typ);
        if (category < 0 || type < 0)
            continue;

        /* Optional min_rank (defaults to STAFF_PLAYER) */
        int min_rank = STAFF_PLAYER;
        json_t *rank_val = json_object_get(elem, "min_rank");
        if (rank_val && json_is_integer(rank_val))
            min_rank = (int)json_integer_value(rank_val);

        json_t *val = json_object_get(elem, "value");
        if (!val)
            continue;

        switch (type) {
        case PREF_TYPE_BOOL:
            if (json_is_boolean(val))
                pref_set_bool(list, category, key, json_is_true(val));
            break;
        case PREF_TYPE_INT:
            if (json_is_integer(val))
                pref_set_int(list, category, key, (int)json_integer_value(val));
            break;
        case PREF_TYPE_STRING:
            if (json_is_string(val))
                pref_set_string(list, category, key, json_string_value(val));
            break;
        case PREF_TYPE_BITFIELD:
            if (json_is_integer(val))
                pref_set_bitfield(list, category, key,
                                  (long)json_integer_value(val));
            break;
        }

        /* Apply min_rank to the just-created/updated entry */
        if (min_rank > STAFF_PLAYER) {
            PREF_ENTRY *p = pref_find(*list, key);
            if (p) p->min_rank = min_rank;
        }
    }

    return true;
}

/***************************************************************************
 * Staff Command: do_prefadmin                                             *
 *                                                                         *
 * Syntax:                                                                 *
 *   prefadmin <account> list [category]                                   *
 *   prefadmin <account> set <key> <type> <value>                          *
 *   prefadmin <account> remove <key>                                      *
 *   prefadmin <account> snapshot <character>                               *
 *   prefadmin <account> apply <character>                                 *
 *   prefadmin defaults list [category]                                    *
 *   prefadmin defaults set <key> <type> <value>                           *
 *   prefadmin defaults reset <key>                                        *
 *   prefadmin defaults init                                               *
 ***************************************************************************/

/**
 * do_prefadmin - Staff command to manage preferences
 *
 * Allows staff to view, set, remove, snapshot, and apply preferences
 * for an account. Also manages game-wide preference defaults via the
 * "defaults" subcommand.
 *
 * The defaults subsystem is the game's source of truth for all default
 * preference values. "defaults list" shows every known setting with its
 * current effective value. "defaults init" populates all settings from
 * the hardcoded factory values, making them explicitly editable.
 * "defaults set" changes a default, and "defaults reset" reverts a
 * setting back to its factory value from pc_set_table.
 *
 * @param ch        Staff character executing the command
 * @param argument  Command arguments
 */
void do_prefadmin(CHAR_DATA *ch, char *argument)
{
    char arg_account[MAX_INPUT_LENGTH];
    char arg_sub[MAX_INPUT_LENGTH];
    char arg_key[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg_account);
    argument = one_argument(argument, arg_sub);

    if (IS_NULLSTR(arg_account) || IS_NULLSTR(arg_sub)) {
        send_to_char("Syntax: prefadmin <account> list [category]\n\r", ch);
        send_to_char("        prefadmin <account> set <key> <type> <value>\n\r", ch);
        send_to_char("        prefadmin <account> remove <key>\n\r", ch);
        send_to_char("        prefadmin <account> snapshot <character>\n\r", ch);
        send_to_char("        prefadmin <account> apply <character>\n\r", ch);
        send_to_char("        prefadmin defaults list [category]\n\r", ch);
        send_to_char("        prefadmin defaults set <key> <type> <value>\n\r", ch);
        send_to_char("        prefadmin defaults reset <key>\n\r", ch);
        send_to_char("        prefadmin defaults init\n\r", ch);
        send_to_char("\n\rCategories: toggle, channel, prompt, display\n\r", ch);
        send_to_char("Types: bool, int, string, bitfield\n\r", ch);
        return;
    }

    /*
     * SUBCOMMAND GROUP: defaults
     *
     * When the first argument is "defaults", operate on game_settings.pref_defaults.
     * This is the game's source of truth for all default preference values.
     * Settings stored here are what new characters inherit. The pc_set_table
     * hardcoded values serve only as factory fallbacks for settings not yet
     * explicitly stored in pref_defaults.
     */
    if (!str_cmp(arg_account, "defaults")) {
        /*
         * defaults list [category]
         */
        if (!str_prefix(arg_sub, "list")) {
            argument = one_argument(argument, arg_key);
            int filter_cat = -1;
            if (!IS_NULLSTR(arg_key)) {
                filter_cat = pref_category_lookup(arg_key);
                if (filter_cat < 0) {
                    send_to_char("Invalid category. Use: toggle, channel, prompt, display\n\r", ch);
                    return;
                }
            }

            int count = 0;

            send_to_char("{CGame-wide preference defaults (source of truth){x:\n\r", ch);
            send_to_char("{D+----+----------+----------+----------------+--------------+----------+{x\n\r", ch);
            send_to_char("{D|{x {C#{x  {D|{x {CCategory{x {D|{x {CType{x     {D|{x {CKey{x            {D|{x {CValue{x        {D|{x {CSource{x   {D|{x\n\r", ch);
            send_to_char("{D+----+----------+----------+----------------+--------------+----------+{x\n\r", ch);

            /* Show all toggle settings from pc_set_table */
            if (filter_cat < 0 || filter_cat == PREF_CAT_TOGGLE) {
                for (int i = 0; pc_set_table[i].name; i++) {
                    PREF_ENTRY *entry = pref_find(game_settings.pref_defaults, pc_set_table[i].name);
                    bool effective_val;
                    const char *source;
                    bool factory_val = (pc_set_table[i].default_state == SETTING_ON);

                    if (entry && entry->type == PREF_TYPE_BOOL) {
                        effective_val = entry->val.b;
                    } else {
                        effective_val = factory_val;
                    }

                    source = (effective_val != factory_val) ? "{Ymodified{x" : "{Ddefault{x ";

                    count++;
                    sprintf(buf, "{D|{x %-2d {D|{x %-8s {D|{x %-8s {D|{x %-14s {D|{x %-12s {D|{x %-8s {D|{x\n\r",
                            count,
                            "toggle",
                            "bool",
                            pc_set_table[i].name,
                            effective_val ? "{GOn{x " : "{ROff{x",
                            source);
                    send_to_char(buf, ch);
                }
            }

            /* Show all channel settings */
            if (filter_cat < 0 || filter_cat == PREF_CAT_CHANNEL) {
                for (int i = 0; acct_channel_defaults[i].name; i++) {
                    PREF_ENTRY *entry = pref_find(game_settings.pref_defaults,
                                                  acct_channel_defaults[i].pref_key);
                    bool effective_val;
                    const char *source;
                    /* Factory default for all channels is ON */
                    bool factory_val = true;

                    if (entry && entry->type == PREF_TYPE_BOOL) {
                        effective_val = entry->val.b;
                    } else {
                        effective_val = factory_val;
                    }

                    source = (effective_val != factory_val) ? "{Ymodified{x" : "{Ddefault{x ";

                    count++;
                    sprintf(buf, "{D|{x %-2d {D|{x %-8s {D|{x %-8s {D|{x %-14s {D|{x %-12s {D|{x %-8s {D|{x\n\r",
                            count,
                            "channel",
                            "bool",
                            acct_channel_defaults[i].pref_key,
                            effective_val ? "{GOn{x " : "{ROff{x",
                            source);
                    send_to_char(buf, ch);
                }
            }

            /* Show any additional pref_defaults entries not already covered
             * by pc_set_table or acct_channel_defaults */
            PREF_ENTRY *p;
            for (p = game_settings.pref_defaults; p; p = p->next) {
                /* Skip entries already shown above */
                bool already_shown = false;
                for (int i = 0; pc_set_table[i].name; i++) {
                    if (!str_cmp(p->key, pc_set_table[i].name)) {
                        already_shown = true;
                        break;
                    }
                }
                if (!already_shown) {
                    for (int i = 0; acct_channel_defaults[i].name; i++) {
                        if (!str_cmp(p->key, acct_channel_defaults[i].pref_key)) {
                            already_shown = true;
                            break;
                        }
                    }
                }
                if (already_shown)
                    continue;

                if (filter_cat >= 0 && p->category != filter_cat)
                    continue;

                count++;

                char val_str[128];
                switch (p->type) {
                case PREF_TYPE_BOOL:
                    sprintf(val_str, "%s", p->val.b ? "{GOn{x " : "{ROff{x");
                    break;
                case PREF_TYPE_INT:
                    sprintf(val_str, "%d", p->val.i);
                    break;
                case PREF_TYPE_STRING:
                    snprintf(val_str, sizeof(val_str), "%.12s%s",
                             p->val.str ? p->val.str : "",
                             (p->val.str && strlen(p->val.str) > 12) ? "..." : "");
                    break;
                case PREF_TYPE_BITFIELD:
                    sprintf(val_str, "0x%lx", p->val.bits);
                    break;
                default:
                    sprintf(val_str, "?");
                    break;
                }

                sprintf(buf, "{D|{x %-2d {D|{x %-8s {D|{x %-8s {D|{x %-14s {D|{x %-12s {D|{x {Ycustom{x   {D|{x\n\r",
                        count,
                        pref_category_name(p->category),
                        pref_type_name(p->type),
                        p->key,
                        val_str);
                send_to_char(buf, ch);
            }

            send_to_char("{D+----+----------+----------+----------------+--------------+----------+{x\n\r", ch);
            sprintf(buf, "   %d setting%s listed.\n\r", count, count == 1 ? "" : "s");
            send_to_char(buf, ch);
            send_to_char("   {DSource: {Ddefault{D = factory value, {Ymodified{D = changed from factory, {Ycustom{D = extra.{x\n\r", ch);
            return;
        }

        /*
         * defaults set <key> <type> <value>
         */
        if (!str_prefix(arg_sub, "set")) {
            char arg_type[MAX_INPUT_LENGTH];

            argument = one_argument(argument, arg_key);
            argument = one_argument(argument, arg_type);

            if (IS_NULLSTR(arg_key) || IS_NULLSTR(arg_type) || IS_NULLSTR(argument)) {
                send_to_char("Syntax: prefadmin defaults set <key> <type> <value>\n\r", ch);
                return;
            }

            int type = pref_type_lookup(arg_type);
            if (type < 0) {
                send_to_char("Invalid type. Use: bool, int, string, bitfield\n\r", ch);
                return;
            }

            int category = PREF_CAT_TOGGLE;
            if (!str_prefix("channel_", arg_key))
                category = PREF_CAT_CHANNEL;
            else if (!str_cmp("prompt", arg_key))
                category = PREF_CAT_PROMPT;

            switch (type) {
            case PREF_TYPE_BOOL:
                if (!str_cmp(argument, "on") || !str_cmp(argument, "true")
                    || !str_cmp(argument, "1") || !str_cmp(argument, "yes"))
                    pref_set_bool(&game_settings.pref_defaults, category, arg_key, true);
                else
                    pref_set_bool(&game_settings.pref_defaults, category, arg_key, false);
                break;
            case PREF_TYPE_INT:
                pref_set_int(&game_settings.pref_defaults, category, arg_key, atoi(argument));
                break;
            case PREF_TYPE_STRING:
                pref_set_string(&game_settings.pref_defaults, category, arg_key, argument);
                break;
            case PREF_TYPE_BITFIELD:
                pref_set_bitfield(&game_settings.pref_defaults, category, arg_key,
                                  strtol(argument, NULL, 0));
                break;
            }

            json_game_settings_write();
            sprintf(buf, "Game default preference '%s' set. Game settings saved.\n\r", arg_key);
            send_to_char(buf, ch);
            return;
        }

        /*
         * defaults reset <key> (also accepts "remove")
         *
         * Removes an explicit setting from pref_defaults, reverting it to
         * the factory value from pc_set_table. For settings not in
         * pc_set_table (channels, etc.), this removes them entirely.
         */
        if (!str_prefix(arg_sub, "reset") || !str_prefix(arg_sub, "remove")) {
            argument = one_argument(argument, arg_key);
            if (IS_NULLSTR(arg_key)) {
                send_to_char("Syntax: prefadmin defaults reset <key>\n\r", ch);
                return;
            }

            if (pref_remove(&game_settings.pref_defaults, arg_key)) {
                /* Check if this key has a factory default in pc_set_table */
                bool has_factory = false;
                for (int i = 0; pc_set_table[i].name; i++) {
                    if (!str_cmp(arg_key, pc_set_table[i].name)) {
                        has_factory = true;
                        sprintf(buf, "Default '%s' reset to factory value (%s). Game settings saved.\n\r",
                                arg_key,
                                pc_set_table[i].default_state == SETTING_ON ? "On" : "Off");
                        break;
                    }
                }
                if (!has_factory)
                    sprintf(buf, "Default '%s' removed. Game settings saved.\n\r", arg_key);

                json_game_settings_write();
                send_to_char(buf, ch);
            } else {
                send_to_char("Preference not found in game defaults.\n\r", ch);
            }
            return;
        }

        /*
         * defaults init
         *
         * Populate pref_defaults with all toggle settings from pc_set_table
         * and all channel defaults that don't already have an explicit entry.
         * This makes every setting explicitly stored and editable,
         * establishing pref_defaults as the complete source of truth.
         */
        if (!str_prefix(arg_sub, "init")) {
            int seeded = 0;
            int skipped = 0;

            /* Seed toggle settings from pc_set_table */
            for (int i = 0; pc_set_table[i].name; i++) {
                PREF_ENTRY *existing = pref_find(game_settings.pref_defaults, pc_set_table[i].name);
                if (existing) {
                    skipped++;
                    continue;
                }

                bool default_val = (pc_set_table[i].default_state == SETTING_ON);
                pref_set_bool(&game_settings.pref_defaults, PREF_CAT_TOGGLE,
                              pc_set_table[i].name, default_val);
                seeded++;
            }

            /* Seed channel defaults (all ON by default) */
            for (int i = 0; acct_channel_defaults[i].name; i++) {
                PREF_ENTRY *existing = pref_find(game_settings.pref_defaults,
                                                 acct_channel_defaults[i].pref_key);
                if (existing) {
                    skipped++;
                    continue;
                }

                pref_set_bool(&game_settings.pref_defaults, PREF_CAT_CHANNEL,
                              acct_channel_defaults[i].pref_key, true);
                seeded++;
            }

            json_game_settings_write();
            sprintf(buf, "Initialized %d setting%s from factory defaults (%d already set). Game settings saved.\n\r",
                    seeded, seeded == 1 ? "" : "s", skipped);
            send_to_char(buf, ch);
            return;
        }

        send_to_char("Invalid defaults subcommand. Use: list, set, reset, init\n\r", ch);
        return;
    }

    /* Look up account */
    bool was_loaded = false;
    ACCOUNT_DATA *account = get_account_by_identifier(arg_account, &was_loaded);
    if (!account) {
        send_to_char("Account not found.\n\r", ch);
        return;
    }

    /*
     * SUBCOMMAND: list
     */
    if (!str_prefix(arg_sub, "list")) {
        argument = one_argument(argument, arg_key); /* optional category */
        int filter_cat = -1;
        if (!IS_NULLSTR(arg_key)) {
            filter_cat = pref_category_lookup(arg_key);
            if (filter_cat < 0) {
                send_to_char("Invalid category. Use: toggle, channel, prompt, display\n\r", ch);
                return;
            }
        }

        PREF_ENTRY *p;
        int i = 0;

        sprintf(buf, "{CPreferences for account {W%s{x", account->username);
        if (filter_cat >= 0) {
            char tmp[64];
            sprintf(tmp, " {C(category: %s){x", pref_category_name(filter_cat));
            strcat(buf, tmp);
        }
        strcat(buf, "{x:\n\r");
        send_to_char(buf, ch);

        send_to_char("{D+----+----------+----------+----------+--------------------------+{x\n\r", ch);
        send_to_char("{D|{x {C#{x  {D|{x {CCategory{x {D|{x {CType{x     {D|{x {CKey{x      {D|{x {CValue{x                    {D|{x\n\r", ch);
        send_to_char("{D+----+----------+----------+----------+--------------------------+{x\n\r", ch);

        for (p = account->preferences; p; p = p->next) {
            if (filter_cat >= 0 && p->category != filter_cat)
                continue;
            i++;

            char val_str[128];
            switch (p->type) {
            case PREF_TYPE_BOOL:
                sprintf(val_str, "%s", p->val.b ? "{GOn{x " : "{ROff{x");
                break;
            case PREF_TYPE_INT:
                sprintf(val_str, "%d", p->val.i);
                break;
            case PREF_TYPE_STRING:
                snprintf(val_str, sizeof(val_str), "%.24s%s",
                         p->val.str ? p->val.str : "",
                         (p->val.str && strlen(p->val.str) > 24) ? "..." : "");
                break;
            case PREF_TYPE_BITFIELD:
                sprintf(val_str, "0x%lx", p->val.bits);
                break;
            default:
                sprintf(val_str, "?");
                break;
            }

            sprintf(buf, "{D|{x %-2d {D|{x %-8s {D|{x %-8s {D|{x %-8s {D|{x %-24s {D|{x\n\r",
                    i,
                    pref_category_name(p->category),
                    pref_type_name(p->type),
                    p->key,
                    val_str);
            send_to_char(buf, ch);
        }

        send_to_char("{D+----+----------+----------+----------+--------------------------+{x\n\r", ch);

        if (i == 0)
            send_to_char("   No preferences set.\n\r", ch);
        else {
            sprintf(buf, "   %d preference%s listed.\n\r", i, i == 1 ? "" : "s");
            send_to_char(buf, ch);
        }
        return;
    }

    /*
     * SUBCOMMAND: set
     */
    if (!str_prefix(arg_sub, "set")) {
        char arg_type[MAX_INPUT_LENGTH];

        argument = one_argument(argument, arg_key);
        argument = one_argument(argument, arg_type);

        if (IS_NULLSTR(arg_key) || IS_NULLSTR(arg_type) || IS_NULLSTR(argument)) {
            send_to_char("Syntax: preferences <account> set <key> <type> <value>\n\r", ch);
            return;
        }

        int type = pref_type_lookup(arg_type);
        if (type < 0) {
            send_to_char("Invalid type. Use: bool, int, string, bitfield\n\r", ch);
            return;
        }

        /* Determine category from key prefix or default to toggle */
        int category = PREF_CAT_TOGGLE;
        if (!str_prefix("channel_", arg_key))
            category = PREF_CAT_CHANNEL;
        else if (!str_cmp("prompt", arg_key))
            category = PREF_CAT_PROMPT;

        switch (type) {
        case PREF_TYPE_BOOL:
            if (!str_cmp(argument, "on") || !str_cmp(argument, "true")
                || !str_cmp(argument, "1") || !str_cmp(argument, "yes"))
                pref_set_bool(&account->preferences, category, arg_key, true);
            else
                pref_set_bool(&account->preferences, category, arg_key, false);
            break;
        case PREF_TYPE_INT:
            pref_set_int(&account->preferences, category, arg_key, atoi(argument));
            break;
        case PREF_TYPE_STRING:
            pref_set_string(&account->preferences, category, arg_key, argument);
            break;
        case PREF_TYPE_BITFIELD:
            pref_set_bitfield(&account->preferences, category, arg_key,
                              strtol(argument, NULL, 0));
            break;
        }

        save_account(account);
        sprintf(buf, "Preference '%s' set on account %s.\n\r", arg_key, account->username);
        send_to_char(buf, ch);
        return;
    }

    /*
     * SUBCOMMAND: remove
     */
    if (!str_prefix(arg_sub, "remove")) {
        argument = one_argument(argument, arg_key);
        if (IS_NULLSTR(arg_key)) {
            send_to_char("Syntax: preferences <account> remove <key>\n\r", ch);
            return;
        }

        if (pref_remove(&account->preferences, arg_key)) {
            save_account(account);
            sprintf(buf, "Preference '%s' removed from account %s.\n\r",
                    arg_key, account->username);
            send_to_char(buf, ch);
        } else {
            send_to_char("Preference not found.\n\r", ch);
        }
        return;
    }

    /*
     * SUBCOMMAND: snapshot
     *
     * Captures a character's current settings into the account's
     * preference list. Useful for "make my current settings the default
     * for all my characters" workflow.
     */
    if (!str_prefix(arg_sub, "snapshot")) {
        argument = one_argument(argument, arg_key); /* character name */
        if (IS_NULLSTR(arg_key)) {
            send_to_char("Syntax: preferences <account> snapshot <character>\n\r", ch);
            return;
        }

        /* Find the character online */
        CHAR_DATA *victim = get_char_world(ch, arg_key);
        if (!victim || IS_NPC(victim)) {
            send_to_char("Character not found or is an NPC.\n\r", ch);
            return;
        }

        /* Snapshot all categories */
        pref_snapshot_toggles(victim, &account->preferences);
        pref_snapshot_channels(victim, &account->preferences);
        pref_snapshot_prompt(victim, &account->preferences);
        pref_snapshot_wimpy(victim, &account->preferences);

        save_account(account);

        sprintf(buf, "Captured %d preferences from %s to account %s.\n\r",
                pref_count(account->preferences), victim->name, account->username);
        send_to_char(buf, ch);
        return;
    }

    /*
     * SUBCOMMAND: apply
     *
     * Applies account preferences to a character, respecting their
     * override list.
     */
    if (!str_prefix(arg_sub, "apply")) {
        argument = one_argument(argument, arg_key); /* character name */
        if (IS_NULLSTR(arg_key)) {
            send_to_char("Syntax: preferences <account> apply <character>\n\r", ch);
            return;
        }

        CHAR_DATA *victim = get_char_world(ch, arg_key);
        if (!victim || IS_NPC(victim)) {
            send_to_char("Character not found or is an NPC.\n\r", ch);
            return;
        }

        PREF_ENTRY *char_prefs = victim->pcdata ? victim->pcdata->preferences : NULL;
        pref_apply_to_character(account, victim, char_prefs);

        sprintf(buf, "Applied account preferences to %s.\n\r", victim->name);
        send_to_char(buf, ch);
        return;
    }

    send_to_char("Invalid subcommand. Use: list, set, remove, snapshot, apply\n\r", ch);
}

/***************************************************************************
 * Runtime State Wrappers                                                  *
 *                                                                         *
 * These read the current bitfield state by name. Migration path away      *
 * from direct IS_SET() checks on act/comm flags.                          *
 ***************************************************************************/

/**
 * pref_check - Check the current state of a toggle setting by name
 *
 * Looks up the setting in pc_set_table[], reads the appropriate bitfield
 * (act[0], act[1], or comm), and handles inversion. Returns the logical
 * state: true means the feature is enabled.
 *
 * @param ch    The character to check (must be PC)
 * @param key   Setting name (e.g. "autoloot", "holyaura")
 * @return      true if the setting is logically ON, false otherwise
 */
bool pref_check(CHAR_DATA *ch, const char *key)
{
    if (!ch || IS_NPC(ch))
        return false;

    for (int i = 0; pc_set_table[i].name; i++) {
        if (str_cmp(pc_set_table[i].name, key))
            continue;

        long *field;
        long vector;

        if (pc_set_table[i].vector != 0) {
            vector = pc_set_table[i].vector;
            field = &ch->act[0];
        } else if (pc_set_table[i].vector2 != 0) {
            vector = pc_set_table[i].vector2;
            field = &ch->act[1];
        } else if (pc_set_table[i].vector_comm != 0) {
            vector = pc_set_table[i].vector_comm;
            field = &ch->comm;
        } else {
            return false;
        }

        bool is_set = IS_SET(*field, vector);
        if (pc_set_table[i].inverted)
            is_set = !is_set;
        return is_set;
    }

    return false;
}

/**
 * pref_check_channel - Check if a channel is enabled for a character
 *
 * Checks the COMM_NO* flag for the named channel. Returns true if the
 * channel is ON (not muted).
 *
 * @param ch       The character to check
 * @param channel  Channel name (e.g. "gossip", "ooc")
 * @return         true if the channel is enabled (not muted)
 */
bool pref_check_channel(CHAR_DATA *ch, const char *channel)
{
    if (!ch)
        return false;

    for (int i = 0; channel_mute_table[i].name; i++) {
        if (!str_cmp(channel_mute_table[i].name, channel))
            return !IS_SET(ch->comm, channel_mute_table[i].flag);
    }

    return false;
}

/**
 * pref_check_index - Get the current boolean state of a pc_set_table entry
 *
 * Internal helper that operates on a table index (faster than name lookup).
 *
 * @param ch     The character
 * @param index  Index into pc_set_table
 * @return       true if the setting is logically ON
 */
static bool pref_check_index(CHAR_DATA *ch, int index)
{
    long *field;
    long vector;

    if (pc_set_table[index].vector != 0) {
        vector = pc_set_table[index].vector;
        field = &ch->act[0];
    } else if (pc_set_table[index].vector2 != 0) {
        vector = pc_set_table[index].vector2;
        field = &ch->act[1];
    } else if (pc_set_table[index].vector_comm != 0) {
        vector = pc_set_table[index].vector_comm;
        field = &ch->comm;
    } else {
        return false;
    }

    bool is_set = IS_SET(*field, vector);
    if (pc_set_table[index].inverted)
        is_set = !is_set;
    return is_set;
}

/***************************************************************************
 * Player Command: do_prefs                                                *
 *                                                                         *
 * Replaces the old toggle/config/autolist commands with a unified          *
 * preferences command that shows source tracking.                         *
 *                                                                         *
 * Syntax:                                                                 *
 *   prefs                    - Show all settings                          *
 *   prefs <setting>          - Toggle a setting                           *
 *   prefs channels           - Show channel settings                      *
 *   prefs reset              - Reset to account/game defaults             *
 *   prefs snapshot           - Save settings as account defaults          *
 ***************************************************************************/

/**
 * get_source_tag - Get a coloured source indicator for display
 *
 * @param source  PREF_SOURCE_* constant
 * @return        Short coloured string
 */
static const char *get_source_tag(int source)
{
    switch (source) {
    case PREF_SOURCE_CHARACTER: return "{Y(char){x";
    case PREF_SOURCE_ACCOUNT:   return "{C(acct){x";
    default:                    return "{D(def){x ";
    }
}

/**
 * show_toggle_settings - Display toggle settings section
 */
static void show_toggle_settings(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    ACCOUNT_DATA *acct = ch->desc ? ch->desc->account : NULL;

    send_to_char("\n\r{Y--- Toggle Settings ---{x\n\r", ch);
    send_to_char("{D  Setting          Status   Source{x\n\r", ch);
    line(ch, 42, NULL, NULL);

    for (int i = 0; pc_set_table[i].name; i++) {
        if (ch->pcdata->staff_rank < pc_set_table[i].min_rank)
            continue;

        bool is_on = pref_check_index(ch, i);
        int source = pref_get_source(acct, ch, pc_set_table[i].name);

        sprintf(buf, "  %-16s %s    %s\n\r",
                pc_set_table[i].name,
                is_on ? "{WON{x " : "{DOFF{x",
                get_source_tag(source));
        send_to_char(buf, ch);
    }

    /* Wimpy (integer setting) */
    {
        int source = pref_get_source(acct, ch, "wimpy");
        sprintf(buf, "  %-16s {W%-5d{x  %s\n\r",
                "wimpy", ch->wimpy, get_source_tag(source));
        send_to_char(buf, ch);
    }
}

/**
 * show_channel_settings - Display channel on/off settings
 */
static void show_channel_settings(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    ACCOUNT_DATA *acct = ch->desc ? ch->desc->account : NULL;

    send_to_char("\n\r{Y--- Channel Settings ---{x\n\r", ch);
    send_to_char("{D  Channel          Status   Source{x\n\r", ch);
    line(ch, 42, NULL, NULL);

    for (int i = 0; channel_mute_table[i].name; i++) {
        char pref_key[64];
        snprintf(pref_key, sizeof(pref_key), "channel_%s", channel_mute_table[i].name);

        bool enabled = !IS_SET(ch->comm, channel_mute_table[i].flag);
        int source = pref_get_source(acct, ch, pref_key);

        sprintf(buf, "  %-16s %s    %s\n\r",
                channel_mute_table[i].name,
                enabled ? "{WON{x " : "{DOFF{x",
                get_source_tag(source));
        send_to_char(buf, ch);
    }

    /* Channel display flags */
    if (ch->pcdata->channel_flags) {
        struct {
            const char *name;
            long        flag;
        } flags[] = {
            { "gossip",  FLAG_GOSSIP  },
            { "ooc",     FLAG_OOC     },
            { "yell",    FLAG_YELL    },
            { "flaming", FLAG_FLAMING },
            { "quote",   FLAG_QUOTE   },
            { "helper",  FLAG_HELPER  },
            { "tells",   FLAG_TELLS   },
            { "music",   FLAG_MUSIC   },
            { "ct",      FLAG_CT      },
            { NULL, 0 }
        };

        send_to_char("\n\r  {DChannel flags shown:{x ", ch);
        bool first = true;
        for (int i = 0; flags[i].name; i++) {
            if (IS_SET(ch->pcdata->channel_flags, flags[i].flag)) {
                if (!first)
                    send_to_char("{D,{x ", ch);
                send_to_char(flags[i].name, ch);
                first = false;
            }
        }
        if (first)
            send_to_char("{Dnone{x", ch);
        send_to_char("\n\r", ch);
    }
}

/**
 * show_prompt_settings - Display prompt setting
 */
static void show_prompt_settings(CHAR_DATA *ch)
{
    char buf[MAX_STRING_LENGTH];
    ACCOUNT_DATA *acct = ch->desc ? ch->desc->account : NULL;
    int source = pref_get_source(acct, ch, "prompt");

    send_to_char("\n\r{Y--- Prompt ---{x\n\r", ch);
    sprintf(buf, "  Prompt: {W%s{x  %s\n\r",
            ch->prompt ? ch->prompt : "(default)",
            get_source_tag(source));
    send_to_char(buf, ch);
}

/**
 * do_prefs - Player command to view and toggle preferences
 *
 * Unified replacement for toggle/config/autolist that shows settings
 * grouped by category with source indicators (game default, account,
 * or character override).
 *
 * @param ch        The character
 * @param argument  Setting name to toggle, or subcommand
 */
void do_prefs(CHAR_DATA *ch, char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    if (IS_NPC(ch))
        return;

    argument = one_argument(argument, arg);

    /* No argument: show all settings */
    if (IS_NULLSTR(arg)) {
        ACCOUNT_DATA *acct = ch->desc ? ch->desc->account : NULL;

        send_to_char("{C=== Preferences ==={x\n\r", ch);
        send_to_char("{DSources: {D(def){x = game default, "
                     "{C(acct){x = account, "
                     "{Y(char){x = character override\n\r", ch);

        show_toggle_settings(ch);
        show_channel_settings(ch);
        show_prompt_settings(ch);

        /* Show override summary */
        int char_overrides = ch->pcdata->preferences
            ? pref_count(ch->pcdata->preferences) : 0;
        int acct_prefs = (acct && acct->preferences)
            ? pref_count(acct->preferences) : 0;

        send_to_char("\n\r", ch);
        if (char_overrides > 0 || acct_prefs > 0) {
            sprintf(buf, "{D%d character override%s, %d account preference%s set.{x\n\r",
                    char_overrides, char_overrides == 1 ? "" : "s",
                    acct_prefs, acct_prefs == 1 ? "" : "s");
            send_to_char(buf, ch);
        }

        send_to_char("{DType 'prefs <setting>' to toggle. "
                     "'prefs reset' to clear overrides. "
                     "'prefs snapshot' to save to account.{x\n\r", ch);
        return;
    }

    /* Subcommand: channels */
    if (!str_prefix(arg, "channels")) {
        show_channel_settings(ch);
        return;
    }

    /* Subcommand: reset */
    if (!str_prefix(arg, "reset")) {
        int overrides = ch->pcdata->preferences
            ? pref_count(ch->pcdata->preferences) : 0;

        if (overrides == 0) {
            send_to_char("You have no character-specific overrides to reset.\n\r", ch);
            return;
        }

        ACCOUNT_DATA *acct = ch->desc ? ch->desc->account : NULL;
        pref_reset_to_defaults(acct, ch);
        save_char_obj(ch);

        sprintf(buf, "Reset %d character override%s. "
                "Settings restored to %s defaults.\n\r",
                overrides, overrides == 1 ? "" : "s",
                acct && acct->preferences ? "account" : "game");
        send_to_char(buf, ch);
        return;
    }

    /* Subcommand: snapshot */
    if (!str_prefix(arg, "snapshot")) {
        ACCOUNT_DATA *acct = ch->desc ? ch->desc->account : NULL;
        if (!acct) {
            send_to_char("No account found.\n\r", ch);
            return;
        }

        pref_snapshot_toggles(ch, &acct->preferences);
        pref_snapshot_channels(ch, &acct->preferences);
        pref_snapshot_prompt(ch, &acct->preferences);
        pref_snapshot_wimpy(ch, &acct->preferences);

        save_account(acct);

        sprintf(buf, "Saved %d preferences to your account. "
                "These will apply to all characters without overrides.\n\r",
                pref_count(acct->preferences));
        send_to_char(buf, ch);
        return;
    }

    /* Toggle a setting by name */

    /* Check pc_set_table first */
    for (int i = 0; pc_set_table[i].name; i++) {
        if (!str_prefix(arg, pc_set_table[i].name)
            && ch->pcdata->staff_rank >= pc_set_table[i].min_rank) {

            long *field;
            long vector;

            if (pc_set_table[i].vector != 0) {
                vector = pc_set_table[i].vector;
                field = &ch->act[0];
            } else if (pc_set_table[i].vector2 != 0) {
                vector = pc_set_table[i].vector2;
                field = &ch->act[1];
            } else if (pc_set_table[i].vector_comm != 0) {
                vector = pc_set_table[i].vector_comm;
                field = &ch->comm;
            } else {
                send_to_char("Error: invalid setting configuration.\n\r", ch);
                return;
            }

            /* Toggle the bit */
            if (IS_SET(*field, vector))
                REMOVE_BIT(*field, vector);
            else
                SET_BIT(*field, vector);

            /* Determine logical state for display */
            bool is_on = IS_SET(*field, vector);
            if (pc_set_table[i].inverted)
                is_on = !is_on;

            /* Record as character override */
            pref_set_bool(&ch->pcdata->preferences, PREF_CAT_TOGGLE,
                          pc_set_table[i].name, is_on);

            sprintf(buf, "%s is now %s. {Y(character override){x\n\r",
                    pc_set_table[i].name,
                    is_on ? "{WON{x" : "{DOFF{x");
            send_to_char(buf, ch);
            return;
        }
    }

    /* Check channel names */
    for (int i = 0; channel_mute_table[i].name; i++) {
        if (!str_prefix(arg, channel_mute_table[i].name)) {
            char pref_key[64];
            snprintf(pref_key, sizeof(pref_key), "channel_%s",
                     channel_mute_table[i].name);

            /* Toggle: COMM_NO* set = muted */
            if (IS_SET(ch->comm, channel_mute_table[i].flag)) {
                REMOVE_BIT(ch->comm, channel_mute_table[i].flag);
                sprintf(buf, "%s channel is now {WON{x. {Y(character override){x\n\r",
                        channel_mute_table[i].name);
            } else {
                SET_BIT(ch->comm, channel_mute_table[i].flag);
                sprintf(buf, "%s channel is now {DOFF{x. {Y(character override){x\n\r",
                        channel_mute_table[i].name);
            }
            send_to_char(buf, ch);

            /* Record as character override */
            bool enabled = !IS_SET(ch->comm, channel_mute_table[i].flag);
            pref_set_bool(&ch->pcdata->preferences, PREF_CAT_CHANNEL,
                          pref_key, enabled);
            return;
        }
    }

    /* Check for wimpy */
    if (!str_prefix(arg, "wimpy")) {
        if (!IS_NULLSTR(argument)) {
            int val = atoi(argument);
            ch->wimpy = URANGE(0, val, ch->max_hit);
            pref_set_int(&ch->pcdata->preferences, PREF_CAT_TOGGLE,
                         "wimpy", ch->wimpy);
            sprintf(buf, "Wimpy set to %d. {Y(character override){x\n\r",
                    ch->wimpy);
        } else {
            sprintf(buf, "Wimpy is currently set to %d. Use 'prefs wimpy <value>' to change.\n\r",
                    ch->wimpy);
        }
        send_to_char(buf, ch);
        return;
    }

    send_to_char("That is not a valid setting. Type 'prefs' to see all settings.\n\r", ch);
}

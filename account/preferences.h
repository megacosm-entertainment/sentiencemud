/**
 * preferences.h - Account-level preference framework
 *
 * Provides a generic key-value preference system that lives on accounts
 * and can be inherited by characters. Settings set at the account level
 * apply to all characters unless a character has explicitly overridden
 * a given preference.
 *
 * Currently supports:
 *   - Toggle settings (comm flags, act flags via the toggle/config system)
 *   - Channel flags (per-channel display options)
 *   - Prompt string
 *
 * Designed to be extended for future systems like colour themes,
 * display preferences, accessibility settings, etc.
 */

#ifndef ACCOUNT_PREFERENCES_H
#define ACCOUNT_PREFERENCES_H

#include <jansson.h>
#include <stdbool.h>
#include <time.h>

/* Forward declarations */
typedef struct account_data    ACCOUNT_DATA;
typedef struct char_data       CHAR_DATA;
typedef struct descriptor_data DESCRIPTOR_DATA;

/***************************************************************************
 * Preference Categories                                                   *
 *                                                                         *
 * Each category groups related preferences. New categories can be added    *
 * here as new systems are introduced (colour themes, etc.)                *
 ***************************************************************************/

#define PREF_CAT_TOGGLE       0  /* Toggle/config settings (act/comm flags)  */
#define PREF_CAT_CHANNEL      1  /* Channel on/off and display flags         */
#define PREF_CAT_PROMPT       2  /* Prompt string                            */
#define PREF_CAT_DISPLAY      3  /* Display settings (future: colours, etc.) */
#define PREF_CAT_MAX          4  /* Sentinel — keep last                     */

/***************************************************************************
 * Preference Value Types                                                  *
 ***************************************************************************/

#define PREF_TYPE_BOOL        0  /* Boolean on/off toggle                    */
#define PREF_TYPE_INT         1  /* Integer value                            */
#define PREF_TYPE_STRING      2  /* String value                             */
#define PREF_TYPE_BITFIELD    3  /* Bitmask (for channel flags, etc.)        */

/***************************************************************************
 * Override Scope                                                          *
 *                                                                         *
 * INHERIT means "use whatever the account has set".                       *
 * OVERRIDE means "this character has explicitly set this value".          *
 ***************************************************************************/

#define PREF_INHERIT          0  /* Use account-level value                  */
#define PREF_OVERRIDE         1  /* Character has explicitly set this        */

/***************************************************************************
 * Setting Source Tracking                                                 *
 *                                                                         *
 * Identifies where a setting's effective value comes from.                *
 ***************************************************************************/

#define PREF_SOURCE_DEFAULT   0  /* Game default (from pc_set_table)         */
#define PREF_SOURCE_ACCOUNT   1  /* Set at account level                     */
#define PREF_SOURCE_CHARACTER 2  /* Explicitly set on this character         */

extern const char *pref_source_names[];

const char *pref_source_name(int source);

/***************************************************************************
 * Default Channel Table                                                   *
 *                                                                         *
 * Defines channels available at account level and their pref keys.        *
 * All channels default to ON unless overridden.                           *
 * Shared between nanny.c (account menu) and preferences.c (prefadmin).   *
 ***************************************************************************/

typedef struct {
    const char *name;       /* Display name (also used for toggle input) */
    const char *pref_key;   /* Key in the preferences list */
} ACCT_CHANNEL_DEF;

extern const ACCT_CHANNEL_DEF acct_channel_defaults[];

/***************************************************************************
 * Preference Entry                                                        *
 *                                                                         *
 * A single named preference with a typed value. Stored as a linked list   *
 * on the account. Characters store a parallel list of overrides.          *
 ***************************************************************************/

typedef struct pref_entry PREF_ENTRY;

struct pref_entry {
    PREF_ENTRY *next;
    char       *key;            /* Unique name, e.g. "autoloot", "prompt"   */
    int         category;       /* PREF_CAT_* constant                      */
    int         type;           /* PREF_TYPE_* constant                     */
    int         min_rank;       /* STAFF_* rank required to see/toggle      */

    /* Value — only one is meaningful per type */
    union {
        bool    b;              /* PREF_TYPE_BOOL                           */
        int     i;              /* PREF_TYPE_INT                            */
        long    bits;           /* PREF_TYPE_BITFIELD                       */
        char   *str;            /* PREF_TYPE_STRING (allocated)             */
    } val;
};

/***************************************************************************
 * Character Preference Override                                           *
 *                                                                         *
 * Stored on pc_data. Same structure — if a key exists in the character's  *
 * list, it overrides the account value. If absent, the account value is   *
 * inherited.                                                              *
 ***************************************************************************/

/* The character override list uses the same PREF_ENTRY struct */

/***************************************************************************
 * Category / Type Name Lookups                                            *
 ***************************************************************************/

extern const char *pref_category_names[];
extern const char *pref_type_names[];

const char *pref_category_name(int category);
int         pref_category_lookup(const char *name);
const char *pref_type_name(int type);
int         pref_type_lookup(const char *name);

/***************************************************************************
 * Memory Management                                                       *
 ***************************************************************************/

PREF_ENTRY *new_pref_entry(void);
void        free_pref_entry(PREF_ENTRY *entry);
void        free_pref_list(PREF_ENTRY *list);

/***************************************************************************
 * Core Operations                                                         *
 *                                                                         *
 * These operate on a preference list (either account or character).       *
 ***************************************************************************/

/**
 * pref_set_bool - Set a boolean preference in a list
 */
void pref_set_bool(PREF_ENTRY **list, int category,
                   const char *key, bool value);

/**
 * pref_set_int - Set an integer preference in a list
 */
void pref_set_int(PREF_ENTRY **list, int category,
                  const char *key, int value);

/**
 * pref_set_string - Set a string preference in a list
 */
void pref_set_string(PREF_ENTRY **list, int category,
                     const char *key, const char *value);

/**
 * pref_set_bitfield - Set a bitfield preference in a list
 */
void pref_set_bitfield(PREF_ENTRY **list, int category,
                       const char *key, long bits);

/**
 * pref_find - Find a preference entry by key in a list
 *
 * @return  Pointer to the entry, or NULL if not found
 */
PREF_ENTRY *pref_find(PREF_ENTRY *list, const char *key);

/**
 * pref_remove - Remove a preference by key from a list
 *
 * @return  true if found and removed
 */
bool pref_remove(PREF_ENTRY **list, const char *key);

/**
 * pref_count - Count entries in a preference list
 */
int pref_count(PREF_ENTRY *list);

/**
 * pref_count_category - Count entries in a specific category
 */
int pref_count_category(PREF_ENTRY *list, int category);

/***************************************************************************
 * Resolved Lookups (Account + Character Inheritance)                      *
 *                                                                         *
 * These check the character's override list first; if the key isn't       *
 * found there, they fall back to the account preference list.             *
 * If neither has it, they return the provided default.                    *
 ***************************************************************************/

/**
 * pref_get_bool - Get a boolean preference with inheritance
 */
bool pref_get_bool(ACCOUNT_DATA *account, CHAR_DATA *ch,
                   const char *key, bool default_val);

/**
 * pref_get_int - Get an integer preference with inheritance
 */
int pref_get_int(ACCOUNT_DATA *account, CHAR_DATA *ch,
                 const char *key, int default_val);

/**
 * pref_get_string - Get a string preference with inheritance
 *
 * @return  The string value, or default_val if not set. Caller must not free.
 */
const char *pref_get_string(ACCOUNT_DATA *account, CHAR_DATA *ch,
                            const char *key, const char *default_val);

/**
 * pref_get_bitfield - Get a bitfield preference with inheritance
 */
long pref_get_bitfield(ACCOUNT_DATA *account, CHAR_DATA *ch,
                       const char *key, long default_val);

/**
 * pref_is_overridden - Check if a character has overridden a preference
 *
 * @return  true if the character's override list contains the key
 */
bool pref_is_overridden(CHAR_DATA *ch, const char *key);

/***************************************************************************
 * Snapshot / Apply                                                        *
 *                                                                         *
 * Utilities for capturing current character settings into the preference  *
 * system, and for applying account preferences to a character on login.   *
 ***************************************************************************/

/**
 * pref_snapshot_toggles - Capture current toggle settings to a pref list
 *
 * Reads the character's act[0], act[1], and comm bitfields and writes
 * all pc_set_table[] entries as boolean preferences.
 */
void pref_snapshot_toggles(CHAR_DATA *ch, PREF_ENTRY **list);

/**
 * pref_snapshot_channels - Capture channel flags to a pref list
 *
 * Reads channel on/off flags from ch->comm and channel_flags.
 */
void pref_snapshot_channels(CHAR_DATA *ch, PREF_ENTRY **list);

/**
 * pref_snapshot_prompt - Capture prompt string to a pref list
 */
void pref_snapshot_prompt(CHAR_DATA *ch, PREF_ENTRY **list);

/**
 * pref_snapshot_wimpy - Capture wimpy setting to a pref list
 */
void pref_snapshot_wimpy(CHAR_DATA *ch, PREF_ENTRY **list);

/**
 * pref_migrate_character - One-time migration of bitfield state to preferences
 *
 * For legacy characters with no saved preferences, snapshots their current
 * toggle, channel, prompt, wimpy, and scroll settings into explicit
 * character-level preferences. Idempotent — skips if preferences exist.
 *
 * @param ch  The character to migrate
 * @return    true if migration was performed, false if skipped
 */
bool pref_migrate_character(CHAR_DATA *ch);

/**
 * pref_apply_game_defaults - Set all toggle defaults from pc_set_table
 *
 * Sets every pc_set_table entry to its default_state on the character's
 * bitfields. Call once during character creation before apply_to_character.
 */
void pref_apply_game_defaults(CHAR_DATA *ch);

/**
 * pref_apply_to_character - Apply account prefs to a character on login
 *
 * For each account preference, if the character does not have an explicit
 * override, apply the account value to the character's live fields (act,
 * comm, prompt, etc.).
 *
 * @param account     The account to inherit from
 * @param ch          The character to apply settings to
 * @param char_prefs  The character's override list (may be NULL)
 */
void pref_apply_to_character(ACCOUNT_DATA *account, CHAR_DATA *ch,
                             PREF_ENTRY *char_prefs);

/***************************************************************************
 * Source Tracking                                                         *
 *                                                                         *
 * Determine where a setting's effective value originates from.            *
 ***************************************************************************/

/**
 * pref_get_source - Determine the source of a setting's effective value
 *
 * Returns PREF_SOURCE_CHARACTER if the character has an override,
 * PREF_SOURCE_ACCOUNT if the account has a preference, or
 * PREF_SOURCE_DEFAULT if neither has set it.
 *
 * @param account  The account (may be NULL)
 * @param ch       The character (may be NULL)
 * @param key      The preference key
 * @return         PREF_SOURCE_* constant
 */
int pref_get_source(ACCOUNT_DATA *account, CHAR_DATA *ch, const char *key);

/**
 * pref_get_source_name - Get a human-readable source label for display
 *
 * @param source   PREF_SOURCE_* constant
 * @return         "default", "account", or "character"
 */
const char *pref_get_source_name(int source);

/***************************************************************************
 * Reset Preview                                                           *
 *                                                                         *
 * Build a preview of what changes would occur when resetting a            *
 * character's settings to defaults and applying account preferences.      *
 ***************************************************************************/

/**
 * pref_build_reset_preview - Show what would change on reset
 *
 * Writes formatted output to the descriptor showing which character
 * overrides exist, what their current values are, and what they would
 * become after reset (either account value or game default).
 *
 * @param d  The descriptor to write to
 * @return   Number of settings that would change
 */
int pref_build_reset_preview(DESCRIPTOR_DATA *d);

/**
 * pref_reset_to_defaults - Clear character overrides and apply account prefs
 *
 * Removes all character-level preference overrides, then applies account
 * preferences to the character's live fields (act, comm, prompt).
 * Settings not covered by account preferences revert to game defaults.
 *
 * @param account     The account
 * @param ch          The character
 */
void pref_reset_to_defaults(ACCOUNT_DATA *account, CHAR_DATA *ch);

/***************************************************************************
 * JSON Serialization                                                      *
 ***************************************************************************/

/**
 * prefs_to_json - Serialize a preference list to a JSON array
 */
json_t *prefs_to_json(PREF_ENTRY *list);

/**
 * json_to_prefs - Deserialize a JSON array into a preference list
 *
 * @param array  JSON array to read
 * @param list   Pointer to list head (will be populated)
 * @return       true on success
 */
bool json_to_prefs(json_t *array, PREF_ENTRY **list);

/***************************************************************************
 * Player / Staff Commands                                                 *
 ***************************************************************************/

/***************************************************************************
 * Runtime State Wrappers                                                  *
 *                                                                         *
 * These read the current bitfield state for a setting by name. They are   *
 * the migration path away from direct IS_SET() checks on act/comm flags.  *
 * Code should gradually move from IS_SET(ch->act, PLR_FOO) to             *
 * pref_check(ch, "foo").                                                  *
 ***************************************************************************/

/**
 * pref_check - Check the current state of a toggle setting by name
 *
 * Looks up the setting in pc_set_table[], reads the appropriate bitfield
 * (act[0], act[1], or comm), and handles inversion. Returns the logical
 * state: true means the feature is enabled.
 *
 * @param ch    The character to check
 * @param key   Setting name (e.g. "autoloot", "holyaura")
 * @return      true if the setting is logically ON
 */
bool pref_check(CHAR_DATA *ch, const char *key);

/**
 * pref_check_channel - Check if a channel is enabled for a character
 *
 * Checks the COMM_NO* flag for the named channel. Returns true if the
 * channel is ON (not muted).
 *
 * @param ch       The character to check
 * @param channel  Channel name (e.g. "gossip", "ooc")
 * @return         true if the channel is enabled
 */
bool pref_check_channel(CHAR_DATA *ch, const char *channel);

/***************************************************************************
 * Player / Staff Commands                                                 *
 ***************************************************************************/

/**
 * do_prefs - Player command to view/toggle preferences
 *
 * Without argument: Displays all settings grouped by category with
 * source indicators (default/account/character).
 * With argument: Toggles the named setting (creates character override).
 *
 * Syntax: prefs              - Show all settings
 *         prefs <setting>    - Toggle a setting
 *         prefs reset        - Reset all to account/game defaults
 *         prefs snapshot     - Save current settings as account defaults
 */
void do_prefs(CHAR_DATA *ch, char *argument);

/**
 * do_prefadmin - Staff command to view/manage account preferences
 *
 * Syntax: prefadmin <account> [list|set|remove|snapshot|apply]
 */
void do_prefadmin(CHAR_DATA *ch, char *argument);

#endif /* ACCOUNT_PREFERENCES_H */

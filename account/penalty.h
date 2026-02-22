/***************************************************************************
 *  Sentience MUD - Unified Penalty & Bonus System                         *
 *  Copyright (C) 2000-2026 Nibelung Enterprises                          *
 *  All Rights Reserved                                                    *
 *                                                                         *
 *  Manages account-level and character-scoped penalties and bonuses.      *
 *  Replaces the fragmented PLR_DENY/PLR_FREEZE/PLR_LOG/COMM_NOCHANNELS/ *
 *  COMM_NOTELL flag system with a unified, auditable, expirable system.  *
 ***************************************************************************/

#ifndef PENALTY_H
#define PENALTY_H

#include <stdbool.h>
#include <time.h>
#include <jansson.h>

/***************************************************************************
 * Forward Declarations                                                    *
 ***************************************************************************/
typedef struct penalty_data PENALTY_DATA;
typedef struct bonus_data   BONUS_DATA;

/* Forward declare from merc.h */
typedef struct account_data ACCOUNT_DATA;
typedef struct char_data    CHAR_DATA;

/***************************************************************************
 * Penalty Types                                                           *
 ***************************************************************************/

#define PENALTY_DENY          0  /* Cannot log in                          */
#define PENALTY_FREEZE        1  /* Cannot use commands                    */
#define PENALTY_LOG           2  /* All commands logged                    */
#define PENALTY_NOCHANNELS    3  /* Cannot use public channels             */
#define PENALTY_NOTELL        4  /* Cannot use tells                       */
#define PENALTY_NOCHAT        5  /* Banned from specific chat room(s)      */
#define PENALTY_NOEMOTE       6  /* Cannot use emotes                      */
#define PENALTY_BAN_IP        7  /* IP-based ban on account                */
#define PENALTY_BAN_EMAIL     8  /* Email-based ban on account             */
#define PENALTY_BAN_HOST      9  /* Hostname pattern ban on account        */
#define PENALTY_RESTRICT     10  /* Generic restriction (future use)       */
#define PENALTY_CHAN_MUTE    11  /* Per-channel mute; extra=channel_id     */
                                 /* (empty extra = all channels)           */
#define PENALTY_CHAN_WARN    12  /* Per-channel warning (no enforcement);  */
                                 /* extra=channel_id or empty for all      */
#define PENALTY_MAX          13  /* Sentinel - keep last                   */

/***************************************************************************
 * Penalty Scope                                                           *
 ***************************************************************************/

#define PENALTY_SCOPE_ACCOUNT    0  /* Applies to entire account           */
#define PENALTY_SCOPE_CHARACTER  1  /* Applies to specific character only  */

/***************************************************************************
 * Penalty Data Structure                                                  *
 ***************************************************************************/

struct penalty_data {
    PENALTY_DATA *next;
    int           type;         /* PENALTY_* type constant                 */
    int           scope;        /* PENALTY_SCOPE_ACCOUNT or _CHARACTER     */
    char         *reason;       /* Why the penalty was applied             */
    char         *applied_by;   /* Staff member who applied it             */
    time_t        applied_at;   /* When it was applied                     */
    time_t        expires_at;   /* When it expires (0 = permanent)         */
    char         *target_name;  /* Character name (if character-scoped)    */
    char         *extra;        /* Type-specific data (IP, chat room, etc.)*/
};

/***************************************************************************
 * Bonus Types                                                             *
 ***************************************************************************/

#define BONUS_XP              0  /* XP multiplier (value = percentage)     */
#define BONUS_GOLD            1  /* Gold multiplier (value = percentage)   */
#define BONUS_CHAR_SLOTS      2  /* Extra character slots (value = count)  */
#define BONUS_STAFF_SLOTS     3  /* Extra staff slots (value = count)      */
#define BONUS_QP              4  /* Quest point multiplier (value = pct)   */
#define BONUS_TRAIN           5  /* Training multiplier (value = pct)      */
#define BONUS_CUSTOM          6  /* Custom/descriptive bonus               */
#define BONUS_FREE_LEVELS     7  /* Free class levels (value = count)      */
#define BONUS_MAX             8  /* Sentinel - keep last                   */

/***************************************************************************
 * Bonus Scope                                                             *
 ***************************************************************************/

#define BONUS_SCOPE_ACCOUNT    0  /* Applies to entire account             */
#define BONUS_SCOPE_CHARACTER  1  /* Applies to specific character only    */
#define BONUS_SCOPE_UNASSIGNED 2  /* Account-earned, player picks target   */

/***************************************************************************
 * Bonus Flags                                                             *
 ***************************************************************************/

#define BONUS_FLAG_PLAYER_SELECT  (1 << 0)  /* Player can assign to char  */
#define BONUS_FLAG_STACKABLE      (1 << 1)  /* Stacks with same type      */

/***************************************************************************
 * Bonus Data Structure                                                    *
 ***************************************************************************/

struct bonus_data {
    BONUS_DATA *next;
    int         type;           /* BONUS_* type constant                   */
    int         scope;          /* BONUS_SCOPE_ACCOUNT/_CHARACTER/_UNASSIGNED */
    char       *reason;         /* Why the bonus was granted               */
    char       *granted_by;     /* Staff member or system that granted it  */
    time_t      granted_at;     /* When it was granted                     */
    time_t      expires_at;     /* When it expires (0 = permanent)         */
    int         value;          /* Bonus value (percentage, count, etc.)   */
    char       *target_name;    /* Character name (if character-scoped)    */
    long        flags;          /* BONUS_FLAG_* flags                      */
    char       *extra;          /* Type-specific data                      */
};

/***************************************************************************
 * Type Name Lookup Tables                                                 *
 ***************************************************************************/

extern const char *penalty_type_names[];
extern const char *bonus_type_names[];
extern const char *penalty_scope_names[];
extern const char *bonus_scope_names[];

/***************************************************************************
 * Penalty Functions                                                       *
 ***************************************************************************/

/* Memory management */
PENALTY_DATA *new_penalty(void);
void          free_penalty(PENALTY_DATA *penalty);
void          free_penalty_list(PENALTY_DATA *list);

/* Core operations */
PENALTY_DATA *add_penalty(ACCOUNT_DATA *account, int type, int scope,
                          const char *reason, const char *applied_by,
                          time_t expires_at, const char *target_name,
                          const char *extra);
bool          remove_penalty(ACCOUNT_DATA *account, int index);
int           count_penalties(ACCOUNT_DATA *account);
PENALTY_DATA *get_penalty_by_index(ACCOUNT_DATA *account, int index);

/* Query functions */
bool          has_penalty(ACCOUNT_DATA *account, int type,
                          const char *char_name);
PENALTY_DATA *find_penalty(ACCOUNT_DATA *account, int type,
                           const char *char_name);
bool          is_penalty_expired(PENALTY_DATA *penalty);

/* Channel-specific penalty check.
 * Returns true if sender is muted on channel_id (or all channels).
 * Checks PENALTY_NOCHANNELS and PENALTY_CHAN_MUTE.
 * channel_id == NULL checks for any active all-channel block. */
bool          has_channel_penalty(ACCOUNT_DATA *account,
                                  const char *channel_id,
                                  const char *char_name);

/* Expiration management */
int           expire_penalties(ACCOUNT_DATA *account);

/* Type name lookups */
int           penalty_type_lookup(const char *name);
const char   *penalty_type_name(int type);
int           penalty_scope_lookup(const char *name);
const char   *penalty_scope_name(int scope);

/* Duration parsing */
time_t        parse_duration(const char *str);
const char   *penalty_format_duration(time_t seconds, char *buf, size_t buflen);
const char   *format_timestamp(time_t t, char *buf, size_t buflen);

/* Legacy flag migration */
void          migrate_legacy_penalties(ACCOUNT_DATA *account,
                                       CHAR_DATA *ch);

/***************************************************************************
 * Bonus Functions                                                         *
 ***************************************************************************/

/* Memory management */
BONUS_DATA *new_bonus(void);
void        free_bonus(BONUS_DATA *bonus);
void        free_bonus_list(BONUS_DATA *list);

/* Core operations */
BONUS_DATA *add_bonus(ACCOUNT_DATA *account, int type, int scope,
                      const char *reason, const char *granted_by,
                      time_t expires_at, int value,
                      const char *target_name, long flags,
                      const char *extra);
bool        remove_bonus(ACCOUNT_DATA *account, int index);
int         count_bonuses(ACCOUNT_DATA *account);
BONUS_DATA *get_bonus_by_index(ACCOUNT_DATA *account, int index);

/* Query functions */
bool        has_bonus(ACCOUNT_DATA *account, int type,
                      const char *char_name);
BONUS_DATA *find_bonus(ACCOUNT_DATA *account, int type,
                       const char *char_name);
bool        is_bonus_expired(BONUS_DATA *bonus);

/* Aggregation - get total bonus value for a type */
int         get_bonus_value(ACCOUNT_DATA *account, int type,
                            const char *char_name);

/* Expiration management */
int         expire_bonuses(ACCOUNT_DATA *account);

/* Player operations */
bool        assign_bonus_to_character(ACCOUNT_DATA *account,
                                      int index, const char *char_name);

/* Type name lookups */
int         bonus_type_lookup(const char *name);
const char *bonus_type_name(int type);
int         bonus_scope_lookup(const char *name);
const char *bonus_scope_name(int scope);

/***************************************************************************
 * JSON Serialization                                                      *
 ***************************************************************************/

json_t *penalties_to_json(PENALTY_DATA *list);
bool    json_to_penalties(json_t *array, PENALTY_DATA **list);
json_t *bonuses_to_json(BONUS_DATA *list);
bool    json_to_bonuses(json_t *array, BONUS_DATA **list);

/***************************************************************************
 * Staff Commands                                                          *
 ***************************************************************************/

void do_penalty(CHAR_DATA *ch, char *argument);
void do_bonus(CHAR_DATA *ch, char *argument);

#endif /* PENALTY_H */

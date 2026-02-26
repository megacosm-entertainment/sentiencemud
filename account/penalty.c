/***************************************************************************
 *  Sentience MUD - Unified Penalty & Bonus System                         *
 *  Copyright (C) 2000-2026 Nibelung Enterprises                          *
 *  All Rights Reserved                                                    *
 *                                                                         *
 *  Core penalty and bonus management functions. These replace the         *
 *  fragmented PLR_DENY/PLR_FREEZE/PLR_LOG/COMM_NOCHANNELS/COMM_NOTELL   *
 *  flag system with a unified, auditable, time-limited system.            *
 *                                                                         *
 *  Penalties and bonuses are stored at the account level with optional    *
 *  per-character scoping. Legacy character-level flags are automatically  *
 *  migrated into penalty records on first login.                          *
 ***************************************************************************/

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "../merc.h"
#include "../recycle.h"
#include "../log.h"
#include "penalty.h"

/***************************************************************************
 * Type Name Tables                                                        *
 ***************************************************************************/

const char *penalty_type_names[] = {
    "deny",         /* PENALTY_DENY        */
    "freeze",       /* PENALTY_FREEZE      */
    "log",          /* PENALTY_LOG         */
    "nochannels",   /* PENALTY_NOCHANNELS  */
    "notell",       /* PENALTY_NOTELL      */
    "nochat",       /* PENALTY_NOCHAT      */
    "noemote",      /* PENALTY_NOEMOTE     */
    "ban-ip",       /* PENALTY_BAN_IP      */
    "ban-email",    /* PENALTY_BAN_EMAIL   */
    "ban-host",     /* PENALTY_BAN_HOST    */
    "restrict",     /* PENALTY_RESTRICT    */
    "chanmute",     /* PENALTY_CHAN_MUTE   */
    "chanwarn",     /* PENALTY_CHAN_WARN   */
    NULL
};

const char *bonus_type_names[] = {
    "xp",           /* BONUS_XP            */
    "gold",         /* BONUS_GOLD          */
    "charslots",    /* BONUS_CHAR_SLOTS    */
    "staffslots",   /* BONUS_STAFF_SLOTS   */
    "qp",           /* BONUS_QP            */
    "train",        /* BONUS_TRAIN         */
    "custom",       /* BONUS_CUSTOM        */
    "freelevels",   /* BONUS_FREE_LEVELS   */
    NULL
};

const char *penalty_scope_names[] = {
    "account",      /* PENALTY_SCOPE_ACCOUNT   */
    "character",    /* PENALTY_SCOPE_CHARACTER  */
    NULL
};

const char *bonus_scope_names[] = {
    "account",      /* BONUS_SCOPE_ACCOUNT     */
    "character",    /* BONUS_SCOPE_CHARACTER    */
    "unassigned",   /* BONUS_SCOPE_UNASSIGNED   */
    NULL
};

/***************************************************************************
 * Memory Management - Penalties                                           *
 ***************************************************************************/

/**
 * new_penalty - Allocate and zero-initialize a penalty record
 *
 * @return  Newly allocated PENALTY_DATA with all fields zeroed
 */
PENALTY_DATA *new_penalty(void)
{
    PENALTY_DATA *p;

    p = (PENALTY_DATA *)alloc_perm(sizeof(*p));
    memset(p, 0, sizeof(*p));

    p->reason      = &str_empty[0];
    p->applied_by  = &str_empty[0];
    p->target_name = &str_empty[0];
    p->extra       = &str_empty[0];

    return p;
}

/**
 * free_penalty - Free a single penalty record and its strings
 *
 * @param penalty  Penalty to free (safe to call with NULL)
 */
void free_penalty(PENALTY_DATA *penalty)
{
    if (!penalty) return;

    free_string(penalty->reason);
    free_string(penalty->applied_by);
    free_string(penalty->target_name);
    free_string(penalty->extra);

    /* Note: we don't free the struct itself since alloc_perm is used */
}

/**
 * free_penalty_list - Free an entire linked list of penalties
 *
 * @param list  Head of the penalty list
 */
void free_penalty_list(PENALTY_DATA *list)
{
    PENALTY_DATA *next;

    while (list) {
        next = list->next;
        free_penalty(list);
        list = next;
    }
}

/***************************************************************************
 * Memory Management - Bonuses                                             *
 ***************************************************************************/

/**
 * new_bonus - Allocate and zero-initialize a bonus record
 *
 * @return  Newly allocated BONUS_DATA with all fields zeroed
 */
BONUS_DATA *new_bonus(void)
{
    BONUS_DATA *b;

    b = (BONUS_DATA *)alloc_perm(sizeof(*b));
    memset(b, 0, sizeof(*b));

    b->reason      = &str_empty[0];
    b->granted_by  = &str_empty[0];
    b->target_name = &str_empty[0];
    b->extra       = &str_empty[0];

    return b;
}

/**
 * free_bonus - Free a single bonus record and its strings
 *
 * @param bonus  Bonus to free (safe to call with NULL)
 */
void free_bonus(BONUS_DATA *bonus)
{
    if (!bonus) return;

    free_string(bonus->reason);
    free_string(bonus->granted_by);
    free_string(bonus->target_name);
    free_string(bonus->extra);
}

/**
 * free_bonus_list - Free an entire linked list of bonuses
 *
 * @param list  Head of the bonus list
 */
void free_bonus_list(BONUS_DATA *list)
{
    BONUS_DATA *next;

    while (list) {
        next = list->next;
        free_bonus(list);
        list = next;
    }
}

/***************************************************************************
 * Type Name Lookups                                                       *
 ***************************************************************************/

/**
 * penalty_type_lookup - Convert a type name string to its numeric constant
 *
 * @param name  Type name (e.g., "deny", "freeze")
 * @return      Type constant, or -1 if not found
 */
int penalty_type_lookup(const char *name)
{
    if (!name || !name[0]) return -1;

    for (int i = 0; i < PENALTY_MAX; i++) {
        if (!str_cmp(name, penalty_type_names[i]))
            return i;
    }
    return -1;
}

/**
 * penalty_type_name - Convert a type constant to its display name
 *
 * @param type  Type constant
 * @return      Name string, or "unknown" if out of range
 */
const char *penalty_type_name(int type)
{
    if (type < 0 || type >= PENALTY_MAX)
        return "unknown";
    return penalty_type_names[type];
}

/**
 * penalty_scope_lookup - Convert a scope name to its numeric constant
 *
 * @param name  Scope name (e.g., "account", "character")
 * @return      Scope constant, or -1 if not found
 */
int penalty_scope_lookup(const char *name)
{
    if (!name || !name[0]) return -1;

    for (int i = 0; penalty_scope_names[i]; i++) {
        if (!str_cmp(name, penalty_scope_names[i]))
            return i;
    }
    return -1;
}

/**
 * penalty_scope_name - Convert a scope constant to its display name
 *
 * @param scope  Scope constant
 * @return       Name string, or "unknown" if out of range
 */
const char *penalty_scope_name(int scope)
{
    if (scope < 0 || scope > PENALTY_SCOPE_CHARACTER)
        return "unknown";
    return penalty_scope_names[scope];
}

/**
 * bonus_type_lookup - Convert a bonus type name to its numeric constant
 *
 * @param name  Type name (e.g., "xp", "gold")
 * @return      Type constant, or -1 if not found
 */
int bonus_type_lookup(const char *name)
{
    if (!name || !name[0]) return -1;

    for (int i = 0; i < BONUS_MAX; i++) {
        if (!str_cmp(name, bonus_type_names[i]))
            return i;
    }
    return -1;
}

/**
 * bonus_type_name - Convert a bonus type constant to its display name
 *
 * @param type  Type constant
 * @return      Name string, or "unknown" if out of range
 */
const char *bonus_type_name(int type)
{
    if (type < 0 || type >= BONUS_MAX)
        return "unknown";
    return bonus_type_names[type];
}

/**
 * bonus_scope_lookup - Convert a bonus scope name to its numeric constant
 *
 * @param name  Scope name
 * @return      Scope constant, or -1 if not found
 */
int bonus_scope_lookup(const char *name)
{
    if (!name || !name[0]) return -1;

    for (int i = 0; bonus_scope_names[i]; i++) {
        if (!str_cmp(name, bonus_scope_names[i]))
            return i;
    }
    return -1;
}

/**
 * bonus_scope_name - Convert a bonus scope constant to its display name
 *
 * @param scope  Scope constant
 * @return       Name string, or "unknown" if out of range
 */
const char *bonus_scope_name(int scope)
{
    if (scope < 0 || scope > BONUS_SCOPE_UNASSIGNED)
        return "unknown";
    return bonus_scope_names[scope];
}

/***************************************************************************
 * Duration Parsing & Formatting                                           *
 ***************************************************************************/

/**
 * parse_duration - Parse a human-readable duration string into seconds
 *
 * Supports formats like "30m", "2h", "7d", "1w", "permanent", "perm".
 * Multiple units can be combined: "1d12h" = 1 day + 12 hours.
 * Plain numbers are treated as minutes.
 *
 * @param str  Duration string
 * @return     Duration in seconds, or 0 for permanent
 */
time_t parse_duration(const char *str)
{
    time_t total = 0;
    long current = 0;

    if (!str || !str[0]) return 0;

    if (!str_cmp(str, "permanent") || !str_cmp(str, "perm")) return 0;

    for (const char *p = str; *p; p++) {
        if (isdigit((unsigned char)*p)) {
            current = current * 10 + (*p - '0');
        } else {
            switch (LOWER(*p)) {
                case 's': total += current;            current = 0; break;
                case 'm': total += current * 60;       current = 0; break;
                case 'h': total += current * 3600;     current = 0; break;
                case 'd': total += current * 86400;    current = 0; break;
                case 'w': total += current * 604800;   current = 0; break;
                default:
                    /* Unknown suffix, treat as minutes */
                    if (current > 0) {
                        total += current * 60;
                        current = 0;
                    }
                    break;
            }
        }
    }

    /* Trailing number with no suffix = minutes */
    if (current > 0) {
        total += current * 60;
    }

    return total;
}

/**
 * penalty_format_duration - Format seconds into a human-readable duration string
 *
 * @param seconds  Duration in seconds (0 = permanent)
 * @param buf      Output buffer
 * @param buflen   Size of output buffer
 * @return         Pointer to buf
 */
const char *penalty_format_duration(time_t seconds, char *buf, size_t buflen)
{
    if (seconds <= 0) {
        snprintf(buf, buflen, "permanent");
        return buf;
    }

    int days    = (int)(seconds / 86400);
    int hours   = (int)((seconds % 86400) / 3600);
    int minutes = (int)((seconds % 3600) / 60);
    int secs    = (int)(seconds % 60);

    char *p = buf;
    size_t remaining = buflen;
    int written;

    buf[0] = '\0';

    if (days > 0) {
        written = snprintf(p, remaining, "%dd", days);
        p += written; remaining -= written;
    }
    if (hours > 0 && remaining > 0) {
        written = snprintf(p, remaining, "%dh", hours);
        p += written; remaining -= written;
    }
    if (minutes > 0 && remaining > 0) {
        written = snprintf(p, remaining, "%dm", minutes);
        p += written; remaining -= written;
    }
    if (secs > 0 && remaining > 0 && days == 0) {
        snprintf(p, remaining, "%ds", secs);
    }

    /* Edge case: everything was zero */
    if (buf[0] == '\0') {
        snprintf(buf, buflen, "0s");
    }

    return buf;
}

/**
 * format_timestamp - Format a time_t into a human-readable date string
 *
 * @param t       Time value
 * @param buf     Output buffer
 * @param buflen  Size of output buffer
 * @return        Pointer to buf
 */
const char *format_timestamp(time_t t, char *buf, size_t buflen)
{
    if (t == 0) {
        snprintf(buf, buflen, "never");
        return buf;
    }

    struct tm *tm = localtime(&t);
    strftime(buf, buflen, "%Y-%m-%d %H:%M", tm);
    return buf;
}

/***************************************************************************
 * Penalty Core Operations                                                 *
 ***************************************************************************/

/**
 * is_penalty_expired - Check if a penalty has passed its expiration time
 *
 * @param penalty  Penalty to check
 * @return         true if expired, false if permanent or still active
 */
bool is_penalty_expired(PENALTY_DATA *penalty)
{
    if (!penalty) return true;
    if (penalty->expires_at == 0) return false; /* permanent */
    return current_time >= penalty->expires_at;
}

/**
 * add_penalty - Create and prepend a new penalty to the account's list
 *
 * @param account      Target account
 * @param type         PENALTY_* type
 * @param scope        PENALTY_SCOPE_ACCOUNT or _CHARACTER
 * @param reason       Reason for the penalty
 * @param applied_by   Staff member name
 * @param expires_at   Expiration time (0 = permanent)
 * @param target_name  Character name (if character-scoped, else NULL)
 * @param extra        Type-specific data (IP, chat room, etc., else NULL)
 * @return             Newly created penalty, or NULL on error
 */
PENALTY_DATA *add_penalty(ACCOUNT_DATA *account, int type, int scope,
                          const char *reason, const char *applied_by,
                          time_t expires_at, const char *target_name,
                          const char *extra)
{
    PENALTY_DATA *p;

    if (!account) return NULL;
    if (type < 0 || type >= PENALTY_MAX) return NULL;

    p = new_penalty();
    p->type        = type;
    p->scope       = scope;
    p->applied_at  = current_time;
    p->expires_at  = expires_at;

    free_string(p->reason);
    p->reason = str_dup(reason ? reason : "No reason given");

    free_string(p->applied_by);
    p->applied_by = str_dup(applied_by ? applied_by : "System");

    if (scope == PENALTY_SCOPE_CHARACTER && target_name && target_name[0]) {
        free_string(p->target_name);
        p->target_name = str_dup(target_name);
    }

    if (extra && extra[0]) {
        free_string(p->extra);
        p->extra = str_dup(extra);
    }

    /* Prepend to account's penalty list */
    p->next = account->penalties;
    account->penalties = p;

    return p;
}

/**
 * remove_penalty - Remove a penalty by 1-based index
 *
 * @param account  Target account
 * @param index    1-based index into the penalty list
 * @return         true if removed, false if index out of range
 */
bool remove_penalty(ACCOUNT_DATA *account, int index)
{
    PENALTY_DATA *prev = NULL;
    PENALTY_DATA *curr;
    int i = 1;

    if (!account || index < 1) return false;

    for (curr = account->penalties; curr; prev = curr, curr = curr->next) {
        if (i == index) {
            if (prev)
                prev->next = curr->next;
            else
                account->penalties = curr->next;

            free_penalty(curr);
            return true;
        }
        i++;
    }
    return false;
}

/**
 * count_penalties - Count the number of penalties on an account
 *
 * @param account  Target account
 * @return         Number of penalties
 */
int count_penalties(ACCOUNT_DATA *account)
{
    int count = 0;

    if (!account) return 0;

    for (PENALTY_DATA *p = account->penalties; p; p = p->next)
        count++;

    return count;
}

/**
 * get_penalty_by_index - Get a penalty by 1-based index
 *
 * @param account  Target account
 * @param index    1-based index
 * @return         PENALTY_DATA at that index, or NULL
 */
PENALTY_DATA *get_penalty_by_index(ACCOUNT_DATA *account, int index)
{
    int i = 1;

    if (!account || index < 1) return NULL;

    for (PENALTY_DATA *p = account->penalties; p; p = p->next) {
        if (i == index)
            return p;
        i++;
    }
    return NULL;
}

/**
 * has_penalty - Check if an account has an active, unexpired penalty
 *
 * If char_name is provided, checks both account-scoped penalties and
 * character-scoped penalties matching that name.
 *
 * @param account    Target account
 * @param type       PENALTY_* type to check for
 * @param char_name  Character name to check (NULL for account-only)
 * @return           true if an active penalty of this type exists
 */
bool has_penalty(ACCOUNT_DATA *account, int type, const char *char_name)
{
    return find_penalty(account, type, char_name) != NULL;
}

/**
 * find_penalty - Find first matching active, unexpired penalty
 *
 * Checks account-scoped penalties always. If char_name is provided,
 * also checks character-scoped penalties for that character.
 *
 * @param account    Target account
 * @param type       PENALTY_* type to find
 * @param char_name  Character name (NULL for account-scope only)
 * @return           First matching active penalty, or NULL
 */
PENALTY_DATA *find_penalty(ACCOUNT_DATA *account, int type,
                           const char *char_name)
{
    if (!account) return NULL;

    for (PENALTY_DATA *p = account->penalties; p; p = p->next) {
        if (p->type != type)
            continue;
        if (is_penalty_expired(p))
            continue;

        /* Account-scoped penalties always match */
        if (p->scope == PENALTY_SCOPE_ACCOUNT)
            return p;

        /* Character-scoped penalties match if char_name matches */
        if (p->scope == PENALTY_SCOPE_CHARACTER && char_name
            && !str_cmp(p->target_name, char_name))
            return p;
    }
    return NULL;
}

/**
 * has_channel_penalty - Check if a character is blocked from a specific channel
 *
 * Returns true if the account has any active penalty that prevents the
 * named character from sending on channel_id.  Checks in priority order:
 *
 *   1. PENALTY_NOCHANNELS  — blocks all channels (extra ignored)
 *   2. PENALTY_CHAN_MUTE   — blocks a specific channel (extra = channel_id)
 *                            or all channels if extra is empty
 *
 * PENALTY_CHAN_WARN entries are informational only and never block.
 *
 * @param account     Account to check
 * @param channel_id  Channel being used (e.g. "gossip"); NULL = any block
 * @param char_name   Character name for character-scoped penalty matching
 * @return            true if the character is blocked from this channel
 */
bool has_channel_penalty(ACCOUNT_DATA *account,
                         const char *channel_id,
                         const char *char_name)
{
    PENALTY_DATA *p;

    if (!account)
        return false;

    /* Global channel block supersedes per-channel entries */
    if (find_penalty(account, PENALTY_NOCHANNELS, char_name))
        return true;

    /* Per-channel mute: extra empty = all channels; extra set = specific channel */
    for (p = account->penalties; p; p = p->next) {
        if (p->type != PENALTY_CHAN_MUTE)
            continue;
        if (is_penalty_expired(p))
            continue;

        /* Scope check: account-scoped always applies; character-scoped requires name match */
        if (p->scope == PENALTY_SCOPE_CHARACTER
            && (!char_name || str_cmp(p->target_name, char_name)))
            continue;

        /* extra empty = all channels; extra matches specific channel_id */
        if (!p->extra || !p->extra[0])
            return true;   /* all-channel mute */

        if (channel_id && !str_cmp(p->extra, channel_id))
            return true;   /* channel-specific mute */
    }

    return false;
}

/**
 * has_channel_ban_penalty - Check if a character is channel-banned
 *
 * Channel ban semantics are represented by permanent PENALTY_CHAN_MUTE
 * records (expires_at == 0), typically applied by chanban. This helper is
 * intended for receive-side suppression where channels opt in via allow_ban.
 *
 * @param account     Account to check
 * @param channel_id  Channel being viewed (e.g. "gossip")
 * @param char_name   Character name for character-scoped matching
 * @return            true if the character is channel-banned for this channel
 */
bool has_channel_ban_penalty(ACCOUNT_DATA *account,
                             const char *channel_id,
                             const char *char_name)
{
    PENALTY_DATA *p;

    if (!account)
        return false;

    for (p = account->penalties; p; p = p->next) {
        if (p->type != PENALTY_CHAN_MUTE)
            continue;
        if (is_penalty_expired(p))
            continue;

        /* Only permanent mutes count as bans. */
        if (p->expires_at != 0)
            continue;

        if (p->scope == PENALTY_SCOPE_CHARACTER
            && (!char_name || str_cmp(p->target_name, char_name)))
            continue;

        if (!p->extra || !p->extra[0])
            return true;

        if (channel_id && !str_cmp(p->extra, channel_id))
            return true;
    }

    return false;
}

/**
 * expire_penalties - Remove all expired penalties from an account
 *
 * @param account  Target account
 * @return         Number of penalties removed
 */
int expire_penalties(ACCOUNT_DATA *account)
{
    PENALTY_DATA *prev = NULL;
    PENALTY_DATA *curr, *next;
    int removed = 0;

    if (!account) return 0;

    for (curr = account->penalties; curr; curr = next) {
        next = curr->next;

        if (is_penalty_expired(curr)) {
            if (prev)
                prev->next = next;
            else
                account->penalties = next;

            log_message_f(LOG_LEVEL_INFO, LOG_INFO,
                "Penalty expired: %s on account %s (applied by %s)",
                penalty_type_name(curr->type),
                account->username,
                curr->applied_by);

            free_penalty(curr);
            removed++;
        } else {
            prev = curr;
        }
    }

    return removed;
}

/***************************************************************************
 * Bonus Core Operations                                                   *
 ***************************************************************************/

/**
 * is_bonus_expired - Check if a bonus has passed its expiration time
 *
 * @param bonus  Bonus to check
 * @return       true if expired, false if permanent or still active
 */
bool is_bonus_expired(BONUS_DATA *bonus)
{
    if (!bonus) return true;
    if (bonus->expires_at == 0) return false;
    return current_time >= bonus->expires_at;
}

/**
 * add_bonus - Create and prepend a new bonus to the account's list
 *
 * @param account      Target account
 * @param type         BONUS_* type
 * @param scope        BONUS_SCOPE_ACCOUNT, _CHARACTER, or _UNASSIGNED
 * @param reason       Reason for the bonus
 * @param granted_by   Staff member or system name
 * @param expires_at   Expiration time (0 = permanent)
 * @param value        Bonus value (percentage or count, type-dependent)
 * @param target_name  Character name (if character-scoped, else NULL)
 * @param flags        BONUS_FLAG_* flags
 * @param extra        Type-specific data (NULL if none)
 * @return             Newly created bonus, or NULL on error
 */
BONUS_DATA *add_bonus(ACCOUNT_DATA *account, int type, int scope,
                      const char *reason, const char *granted_by,
                      time_t expires_at, int value,
                      const char *target_name, long flags,
                      const char *extra)
{
    BONUS_DATA *b;

    if (!account) return NULL;
    if (type < 0 || type >= BONUS_MAX) return NULL;

    b = new_bonus();
    b->type        = type;
    b->scope       = scope;
    b->granted_at  = current_time;
    b->expires_at  = expires_at;
    b->value       = value;
    b->flags       = flags;

    free_string(b->reason);
    b->reason = str_dup(reason ? reason : "No reason given");

    free_string(b->granted_by);
    b->granted_by = str_dup(granted_by ? granted_by : "System");

    if (target_name && target_name[0]) {
        free_string(b->target_name);
        b->target_name = str_dup(target_name);
    }

    if (extra && extra[0]) {
        free_string(b->extra);
        b->extra = str_dup(extra);
    }

    /* Prepend to account's bonus list */
    b->next = account->bonuses;
    account->bonuses = b;

    return b;
}

/**
 * remove_bonus - Remove a bonus by 1-based index
 *
 * @param account  Target account
 * @param index    1-based index into the bonus list
 * @return         true if removed, false if index out of range
 */
bool remove_bonus(ACCOUNT_DATA *account, int index)
{
    BONUS_DATA *prev = NULL;
    BONUS_DATA *curr;
    int i = 1;

    if (!account || index < 1) return false;

    for (curr = account->bonuses; curr; prev = curr, curr = curr->next) {
        if (i == index) {
            if (prev)
                prev->next = curr->next;
            else
                account->bonuses = curr->next;

            free_bonus(curr);
            return true;
        }
        i++;
    }
    return false;
}

/**
 * count_bonuses - Count the number of bonuses on an account
 *
 * @param account  Target account
 * @return         Number of bonuses
 */
int count_bonuses(ACCOUNT_DATA *account)
{
    int count = 0;

    if (!account) return 0;

    for (BONUS_DATA *b = account->bonuses; b; b = b->next)
        count++;

    return count;
}

/**
 * get_bonus_by_index - Get a bonus by 1-based index
 *
 * @param account  Target account
 * @param index    1-based index
 * @return         BONUS_DATA at that index, or NULL
 */
BONUS_DATA *get_bonus_by_index(ACCOUNT_DATA *account, int index)
{
    int i = 1;

    if (!account || index < 1) return NULL;

    for (BONUS_DATA *b = account->bonuses; b; b = b->next) {
        if (i == index)
            return b;
        i++;
    }
    return NULL;
}

/**
 * has_bonus - Check if an account has an active, unexpired bonus
 *
 * @param account    Target account
 * @param type       BONUS_* type to check for
 * @param char_name  Character name (NULL for account-only)
 * @return           true if an active bonus of this type exists
 */
bool has_bonus(ACCOUNT_DATA *account, int type, const char *char_name)
{
    return find_bonus(account, type, char_name) != NULL;
}

/**
 * find_bonus - Find first matching active, unexpired bonus
 *
 * @param account    Target account
 * @param type       BONUS_* type to find
 * @param char_name  Character name (NULL for account-scope only)
 * @return           First matching active bonus, or NULL
 */
BONUS_DATA *find_bonus(ACCOUNT_DATA *account, int type,
                       const char *char_name)
{
    if (!account) return NULL;

    for (BONUS_DATA *b = account->bonuses; b; b = b->next) {
        if (b->type != type)
            continue;
        if (is_bonus_expired(b))
            continue;

        /* Account-scoped bonuses always match */
        if (b->scope == BONUS_SCOPE_ACCOUNT)
            return b;

        /* Unassigned bonuses match at the account level */
        if (b->scope == BONUS_SCOPE_UNASSIGNED)
            return b;

        /* Character-scoped bonuses match if char_name matches */
        if (b->scope == BONUS_SCOPE_CHARACTER && char_name
            && !str_cmp(b->target_name, char_name))
            return b;
    }
    return NULL;
}

/**
 * get_bonus_value - Get the total bonus value for a type
 *
 * Sums all active, unexpired bonuses matching the type and scope.
 * If BONUS_FLAG_STACKABLE is not set, only the highest value is used.
 *
 * @param account    Target account
 * @param type       BONUS_* type
 * @param char_name  Character name (NULL for account-only)
 * @return           Total bonus value
 */
int get_bonus_value(ACCOUNT_DATA *account, int type,
                    const char *char_name)
{
    int total = 0;
    int highest_nonstackable = 0;
    bool has_nonstackable = false;

    if (!account) return 0;

    for (BONUS_DATA *b = account->bonuses; b; b = b->next) {
        if (b->type != type)
            continue;
        if (is_bonus_expired(b))
            continue;

        bool matches = false;
        if (b->scope == BONUS_SCOPE_ACCOUNT)
            matches = true;
        else if (b->scope == BONUS_SCOPE_UNASSIGNED)
            matches = true;
        else if (b->scope == BONUS_SCOPE_CHARACTER && char_name
                 && !str_cmp(b->target_name, char_name))
            matches = true;

        if (!matches)
            continue;

        if (IS_SET(b->flags, BONUS_FLAG_STACKABLE)) {
            total += b->value;
        } else {
            has_nonstackable = true;
            if (b->value > highest_nonstackable)
                highest_nonstackable = b->value;
        }
    }

    if (has_nonstackable)
        total += highest_nonstackable;

    return total;
}

/**
 * expire_bonuses - Remove all expired bonuses from an account
 *
 * @param account  Target account
 * @return         Number of bonuses removed
 */
int expire_bonuses(ACCOUNT_DATA *account)
{
    BONUS_DATA *prev = NULL;
    BONUS_DATA *curr, *next;
    int removed = 0;

    if (!account) return 0;

    for (curr = account->bonuses; curr; curr = next) {
        next = curr->next;

        if (is_bonus_expired(curr)) {
            if (prev)
                prev->next = next;
            else
                account->bonuses = next;

            log_message_f(LOG_LEVEL_INFO, LOG_INFO,
                "Bonus expired: %s (%d) on account %s (granted by %s)",
                bonus_type_name(curr->type),
                curr->value,
                account->username,
                curr->granted_by);

            free_bonus(curr);
            removed++;
        } else {
            prev = curr;
        }
    }

    return removed;
}

/**
 * assign_bonus_to_character - Assign an unassigned bonus to a character
 *
 * Allows a player to allocate an account-level unassigned bonus to one
 * of their characters. Only works for bonuses with BONUS_SCOPE_UNASSIGNED
 * and BONUS_FLAG_PLAYER_SELECT.
 *
 * @param account    Target account
 * @param index      1-based index of the bonus
 * @param char_name  Character name to assign to
 * @return           true on success, false if invalid
 */
bool assign_bonus_to_character(ACCOUNT_DATA *account, int index,
                               const char *char_name)
{
    BONUS_DATA *b;

    if (!account || !char_name || !char_name[0]) return false;

    b = get_bonus_by_index(account, index);
    if (!b) return false;

    if (b->scope != BONUS_SCOPE_UNASSIGNED) return false;
    if (!IS_SET(b->flags, BONUS_FLAG_PLAYER_SELECT)) return false;

    b->scope = BONUS_SCOPE_CHARACTER;
    free_string(b->target_name);
    b->target_name = str_dup(char_name);

    return true;
}

/***************************************************************************
 * JSON Serialization                                                      *
 ***************************************************************************/

/**
 * penalties_to_json - Serialize a penalty list to a JSON array
 *
 * @param list  Head of penalty list
 * @return      JSON array of penalty objects
 */
json_t *penalties_to_json(PENALTY_DATA *list)
{
    json_t *array = json_array();

    for (PENALTY_DATA *p = list; p; p = p->next) {
        json_t *obj = json_object();

        json_object_set_new(obj, "type",
            json_string(penalty_type_name(p->type)));
        json_object_set_new(obj, "scope",
            json_string(penalty_scope_name(p->scope)));
        json_object_set_new(obj, "reason",
            json_string(p->reason ? p->reason : ""));
        json_object_set_new(obj, "applied_by",
            json_string(p->applied_by ? p->applied_by : ""));
        json_object_set_new(obj, "applied_at",
            json_integer(p->applied_at));
        json_object_set_new(obj, "expires_at",
            json_integer(p->expires_at));

        if (p->scope == PENALTY_SCOPE_CHARACTER
            && p->target_name && p->target_name[0]) {
            json_object_set_new(obj, "target_name",
                json_string(p->target_name));
        }

        if (p->extra && p->extra[0]) {
            json_object_set_new(obj, "extra",
                json_string(p->extra));
        }

        json_array_append_new(array, obj);
    }

    return array;
}

/**
 * json_to_penalties - Deserialize a JSON array into a penalty list
 *
 * @param array  JSON array of penalty objects
 * @param list   Pointer to the list head to populate
 * @return       true on success
 */
bool json_to_penalties(json_t *array, PENALTY_DATA **list)
{
    size_t index;
    json_t *elem;
    PENALTY_DATA *prev = NULL;
    const char *str;

    if (!array || !json_is_array(array) || !list)
        return false;

    json_array_foreach(array, index, elem) {
        PENALTY_DATA *p = new_penalty();

        str = json_string_value(json_object_get(elem, "type"));
        if (str) p->type = penalty_type_lookup(str);

        str = json_string_value(json_object_get(elem, "scope"));
        if (str) p->scope = penalty_scope_lookup(str);

        str = json_string_value(json_object_get(elem, "reason"));
        if (str) { free_string(p->reason); p->reason = str_dup(str); }

        str = json_string_value(json_object_get(elem, "applied_by"));
        if (str) { free_string(p->applied_by); p->applied_by = str_dup(str); }

        p->applied_at = json_integer_value(json_object_get(elem, "applied_at"));
        p->expires_at = json_integer_value(json_object_get(elem, "expires_at"));

        str = json_string_value(json_object_get(elem, "target_name"));
        if (str) { free_string(p->target_name); p->target_name = str_dup(str); }

        str = json_string_value(json_object_get(elem, "extra"));
        if (str) { free_string(p->extra); p->extra = str_dup(str); }

        /* Append in order */
        p->next = NULL;
        if (!*list)
            *list = p;
        else
            prev->next = p;
        prev = p;
    }

    return true;
}

/**
 * bonuses_to_json - Serialize a bonus list to a JSON array
 *
 * @param list  Head of bonus list
 * @return      JSON array of bonus objects
 */
json_t *bonuses_to_json(BONUS_DATA *list)
{
    json_t *array = json_array();

    for (BONUS_DATA *b = list; b; b = b->next) {
        json_t *obj = json_object();

        json_object_set_new(obj, "type",
            json_string(bonus_type_name(b->type)));
        json_object_set_new(obj, "scope",
            json_string(bonus_scope_name(b->scope)));
        json_object_set_new(obj, "reason",
            json_string(b->reason ? b->reason : ""));
        json_object_set_new(obj, "granted_by",
            json_string(b->granted_by ? b->granted_by : ""));
        json_object_set_new(obj, "granted_at",
            json_integer(b->granted_at));
        json_object_set_new(obj, "expires_at",
            json_integer(b->expires_at));
        json_object_set_new(obj, "value",
            json_integer(b->value));
        json_object_set_new(obj, "flags",
            json_integer(b->flags));

        if (b->target_name && b->target_name[0]) {
            json_object_set_new(obj, "target_name",
                json_string(b->target_name));
        }

        if (b->extra && b->extra[0]) {
            json_object_set_new(obj, "extra",
                json_string(b->extra));
        }

        json_array_append_new(array, obj);
    }

    return array;
}

/**
 * json_to_bonuses - Deserialize a JSON array into a bonus list
 *
 * @param array  JSON array of bonus objects
 * @param list   Pointer to the list head to populate
 * @return       true on success
 */
bool json_to_bonuses(json_t *array, BONUS_DATA **list)
{
    size_t index;
    json_t *elem;
    BONUS_DATA *prev = NULL;
    const char *str;

    if (!array || !json_is_array(array) || !list)
        return false;

    json_array_foreach(array, index, elem) {
        BONUS_DATA *b = new_bonus();

        str = json_string_value(json_object_get(elem, "type"));
        if (str) b->type = bonus_type_lookup(str);

        str = json_string_value(json_object_get(elem, "scope"));
        if (str) b->scope = bonus_scope_lookup(str);

        str = json_string_value(json_object_get(elem, "reason"));
        if (str) { free_string(b->reason); b->reason = str_dup(str); }

        str = json_string_value(json_object_get(elem, "granted_by"));
        if (str) { free_string(b->granted_by); b->granted_by = str_dup(str); }

        b->granted_at = json_integer_value(json_object_get(elem, "granted_at"));
        b->expires_at = json_integer_value(json_object_get(elem, "expires_at"));
        b->value      = (int)json_integer_value(json_object_get(elem, "value"));
        b->flags      = (long)json_integer_value(json_object_get(elem, "flags"));

        str = json_string_value(json_object_get(elem, "target_name"));
        if (str) { free_string(b->target_name); b->target_name = str_dup(str); }

        str = json_string_value(json_object_get(elem, "extra"));
        if (str) { free_string(b->extra); b->extra = str_dup(str); }

        /* Append in order */
        b->next = NULL;
        if (!*list)
            *list = b;
        else
            prev->next = b;
        prev = b;
    }

    return true;
}

/***************************************************************************
 * Legacy Flag Migration                                                   *
 ***************************************************************************/

/**
 * migrate_legacy_penalties - Convert old PLR_/COMM_ flags to penalty records
 *
 * Called when a character logs in. Checks for legacy penalty flags
 * (PLR_DENY, PLR_FREEZE, PLR_LOG, COMM_NOCHANNELS, COMM_NOTELL) and
 * converts them to PENALTY_DATA records on the account. The legacy flags
 * are then cleared from the character.
 *
 * Only migrates flags that don't already have a corresponding penalty
 * record, to prevent duplicate migration.
 *
 * Note: COMM_NOCHANNELS and COMM_NOTELL are also used on NPCs/pets,
 * so we only migrate for PCs (IS_NPC check required by caller).
 *
 * @param account  The character's account
 * @param ch       The character being logged in
 */
void migrate_legacy_penalties(ACCOUNT_DATA *account, CHAR_DATA *ch)
{
    bool migrated = false;

    if (!account || !ch || IS_NPC(ch)) return;

    /* PLR_DENY -> PENALTY_DENY */
    if (IS_SET(ch->act[0], PLR_DENY)
        && !has_penalty(account, PENALTY_DENY, ch->name)) {
        add_penalty(account, PENALTY_DENY, PENALTY_SCOPE_ACCOUNT,
            "Migrated from legacy PLR_DENY flag", "System",
            0, NULL, NULL);
        REMOVE_BIT(ch->act[0], PLR_DENY);
        migrated = true;
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Migrated PLR_DENY to penalty record for %s (account %s)",
            ch->name, account->username);
    }

    /* PLR_FREEZE -> PENALTY_FREEZE */
    if (IS_SET(ch->act[0], PLR_FREEZE)
        && !has_penalty(account, PENALTY_FREEZE, ch->name)) {
        add_penalty(account, PENALTY_FREEZE, PENALTY_SCOPE_CHARACTER,
            "Migrated from legacy PLR_FREEZE flag", "System",
            0, ch->name, NULL);
        REMOVE_BIT(ch->act[0], PLR_FREEZE);
        migrated = true;
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Migrated PLR_FREEZE to penalty record for %s (account %s)",
            ch->name, account->username);
    }

    /* PLR_LOG -> PENALTY_LOG */
    if (IS_SET(ch->act[0], PLR_LOG)
        && !has_penalty(account, PENALTY_LOG, ch->name)) {
        add_penalty(account, PENALTY_LOG, PENALTY_SCOPE_CHARACTER,
            "Migrated from legacy PLR_LOG flag", "System",
            0, ch->name, NULL);
        REMOVE_BIT(ch->act[0], PLR_LOG);
        migrated = true;
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Migrated PLR_LOG to penalty record for %s (account %s)",
            ch->name, account->username);
    }

    /* COMM_NOCHANNELS -> PENALTY_NOCHANNELS (PC only) */
    if (IS_SET(ch->comm, COMM_NOCHANNELS)
        && !has_penalty(account, PENALTY_NOCHANNELS, ch->name)) {
        add_penalty(account, PENALTY_NOCHANNELS, PENALTY_SCOPE_CHARACTER,
            "Migrated from legacy COMM_NOCHANNELS flag", "System",
            0, ch->name, NULL);
        REMOVE_BIT(ch->comm, COMM_NOCHANNELS);
        migrated = true;
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Migrated COMM_NOCHANNELS to penalty record for %s (account %s)",
            ch->name, account->username);
    }

    /* COMM_NOTELL -> PENALTY_NOTELL (PC only) */
    if (IS_SET(ch->comm, COMM_NOTELL)
        && !has_penalty(account, PENALTY_NOTELL, ch->name)) {
        add_penalty(account, PENALTY_NOTELL, PENALTY_SCOPE_CHARACTER,
            "Migrated from legacy COMM_NOTELL flag", "System",
            0, ch->name, NULL);
        REMOVE_BIT(ch->comm, COMM_NOTELL);
        migrated = true;
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Migrated COMM_NOTELL to penalty record for %s (account %s)",
            ch->name, account->username);
    }

    if (migrated) {
        save_account(account);
    }
}

/***************************************************************************
 * Staff Commands                                                          *
 ***************************************************************************/

/**
 * do_penalty - Staff command to manage account penalties
 *
 * Syntax:
 *   penalty list <account|player:name>
 *   penalty add <account|player:name> <type> [scope] [duration] <reason>
 *   penalty remove <account|player:name> <#>
 *   penalty info <account|player:name> <#>
 *
 * Types: deny, freeze, log, nochannels, notell, nochat, noemote,
 *        ban-ip, ban-email, ban-host, restrict
 *
 * Scope: account (default), character
 * Duration: perm (default), 30m, 2h, 7d, 1w, etc.
 *
 * @param ch        Staff member issuing the command
 * @param argument  Full argument string
 */
void do_penalty(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];  /* subcommand */
    char arg2[MAX_INPUT_LENGTH];  /* target */
    char arg3[MAX_INPUT_LENGTH];  /* type or index */
    char arg4[MAX_INPUT_LENGTH];  /* scope/duration/reason start */
    ACCOUNT_DATA *account;
    bool loaded = false;

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        send_to_char("Syntax: penalty list <account|player:name>\n\r", ch);
        send_to_char("        penalty add <account|player:name> <type>"
                     " [scope] [duration] <reason>\n\r", ch);
        send_to_char("        penalty remove <account|player:name> <#>\n\r", ch);
        send_to_char("        penalty info <account|player:name> <#>\n\r", ch);
        send_to_char("\n\rTypes: deny, freeze, log, nochannels, notell,"
                     " nochat, noemote,\n\r"
                     "       ban-ip, ban-email, ban-host, restrict\n\r", ch);
        send_to_char("Scope: account (default), character\n\r", ch);
        send_to_char("Duration: perm (default), 30m, 2h, 7d, 1w, etc.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg2);

    if (arg2[0] == '\0') {
        send_to_char("Specify an account name or player:name.\n\r", ch);
        return;
    }

    /* Look up the account */
    account = get_account_by_identifier(arg2, &loaded);
    if (!account) {
        send_to_char("Account not found.\n\r", ch);
        return;
    }

    /*
     * SUBCOMMAND: list
     */
    if (!str_prefix(arg1, "list")) {
        PENALTY_DATA *p;
        char buf[MAX_STRING_LENGTH];
        char dur_buf[64];
        int i = 0;

        /* Expire first */
        expire_penalties(account);

        if (!account->penalties) {
            send_to_char("No penalties on this account.\n\r", ch);
            return;
        }

        snprintf(buf, sizeof(buf),
            "{YPenalties for account {W%s{Y:{x\n\r"
            "{Y #  Type          Scope      Expires     Applied By{x\n\r"
            "{Y--- ------------- ---------- ----------- ----------------{x\n\r",
            account->username);
        send_to_char(buf, ch);

        for (p = account->penalties; p; p = p->next) {
            i++;

            if (p->expires_at == 0) {
                snprintf(dur_buf, sizeof(dur_buf), "permanent");
            } else {
                time_t remaining = p->expires_at - current_time;
                if (remaining <= 0)
                    snprintf(dur_buf, sizeof(dur_buf), "{Dexpired{x");
                else
                    penalty_format_duration(remaining, dur_buf, sizeof(dur_buf));
            }

            snprintf(buf, sizeof(buf),
                "{W%2d{x  %-13s %-10s %-11s %s",
                i,
                penalty_type_name(p->type),
                p->scope == PENALTY_SCOPE_CHARACTER
                    ? p->target_name : "account",
                dur_buf,
                p->applied_by);
            send_to_char(buf, ch);
            send_to_char("\n\r", ch);
        }
        return;
    }

    /*
     * SUBCOMMAND: info
     */
    if (!str_prefix(arg1, "info")) {
        PENALTY_DATA *p;
        char buf[MAX_STRING_LENGTH];
        char time_buf[64];
        char dur_buf[64];
        int index;

        argument = one_argument(argument, arg3);
        if (!is_number(arg3)) {
            send_to_char("Specify a penalty number. Use 'penalty list' first.\n\r", ch);
            return;
        }
        index = atoi(arg3);
        p = get_penalty_by_index(account, index);
        if (!p) {
            send_to_char("Invalid penalty number.\n\r", ch);
            return;
        }

        format_timestamp(p->applied_at, time_buf, sizeof(time_buf));
        if (p->expires_at == 0)
            snprintf(dur_buf, sizeof(dur_buf), "permanent");
        else {
            time_t remaining = p->expires_at - current_time;
            if (remaining <= 0)
                snprintf(dur_buf, sizeof(dur_buf), "{Dexpired{x");
            else
                penalty_format_duration(remaining, dur_buf, sizeof(dur_buf));
        }

        snprintf(buf, sizeof(buf),
            "{YPenalty #%d on {W%s{Y:{x\n\r"
            "  Type:       %s\n\r"
            "  Scope:      %s%s%s\n\r"
            "  Applied by: %s\n\r"
            "  Applied at: %s\n\r"
            "  Expires:    %s\n\r"
            "  Reason:     %s\n\r",
            index, account->username,
            penalty_type_name(p->type),
            penalty_scope_name(p->scope),
            p->scope == PENALTY_SCOPE_CHARACTER ? " (" : "",
            p->scope == PENALTY_SCOPE_CHARACTER ? p->target_name : "",
            p->applied_by,
            time_buf,
            dur_buf,
            p->reason);
        send_to_char(buf, ch);

        if (p->extra && p->extra[0]) {
            snprintf(buf, sizeof(buf), "  Extra:      %s\n\r", p->extra);
            send_to_char(buf, ch);
        }
        return;
    }

    /*
     * SUBCOMMAND: remove
     */
    if (!str_prefix(arg1, "remove")) {
        char buf[MAX_STRING_LENGTH];
        int index;
        PENALTY_DATA *p;

        argument = one_argument(argument, arg3);
        if (!is_number(arg3)) {
            send_to_char("Specify a penalty number. Use 'penalty list' first.\n\r", ch);
            return;
        }
        index = atoi(arg3);

        p = get_penalty_by_index(account, index);
        if (!p) {
            send_to_char("Invalid penalty number.\n\r", ch);
            return;
        }

        snprintf(buf, sizeof(buf),
            "$N removed %s penalty from account %s",
            penalty_type_name(p->type), account->username);
        wiznet(buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);

        snprintf(buf, sizeof(buf),
            "Removed %s penalty (#%d) from account {W%s{x.\n\r",
            penalty_type_name(p->type), index, account->username);

        remove_penalty(account, index);
        save_account(account);

        send_to_char(buf, ch);
        return;
    }

    /*
     * SUBCOMMAND: add
     */
    if (!str_prefix(arg1, "add")) {
        int type, scope;
        time_t expires_at = 0;
        char *target_name = NULL;
        char *extra_data = NULL;
        char buf[MAX_STRING_LENGTH * 2];
        PENALTY_DATA *p;

        argument = one_argument(argument, arg3); /* type */
        if (arg3[0] == '\0') {
            send_to_char("Specify a penalty type.\n\r", ch);
            return;
        }

        type = penalty_type_lookup(arg3);
        if (type < 0) {
            send_to_char("Unknown penalty type. Valid types:\n\r"
                "  deny, freeze, log, nochannels, notell, nochat,\n\r"
                "  noemote, ban-ip, ban-email, ban-host, restrict\n\r", ch);
            return;
        }

        /* Default scope */
        scope = PENALTY_SCOPE_ACCOUNT;

        /* Parse optional scope, duration, and extra */
        argument = one_argument(argument, arg4);
        if (arg4[0] == '\0') {
            send_to_char("Provide a reason for the penalty.\n\r", ch);
            return;
        }

        /* Check if arg4 is a scope */
        int test_scope = penalty_scope_lookup(arg4);
        if (test_scope >= 0) {
            scope = test_scope;
            argument = one_argument(argument, arg4);
            if (arg4[0] == '\0') {
                send_to_char("Provide a reason for the penalty.\n\r", ch);
                return;
            }
        }

        /* If character-scoped, we need a target name */
        if (scope == PENALTY_SCOPE_CHARACTER) {
            /* arg4 should be the character name */
            target_name = arg4;
            argument = one_argument(argument, arg4);
            if (arg4[0] == '\0') {
                send_to_char("Provide a reason for the penalty.\n\r", ch);
                return;
            }
        }

        /* Check if arg4 is a duration */
        if (isdigit((unsigned char)arg4[0])
            || !str_prefix(arg4, "perm")
            || !str_prefix(arg4, "permanent")) {
            time_t dur = parse_duration(arg4);
            if (dur > 0) {
                expires_at = current_time + dur;
            }
            /* Remaining argument is the reason (may be empty) */
            if (argument[0] == '\0') {
                send_to_char("Provide a reason for the penalty.\n\r", ch);
                return;
            }
        } else {
            /* arg4 was not a duration, put it back as part of reason */
            char combined[MAX_STRING_LENGTH];
            if (argument[0] != '\0')
                snprintf(combined, sizeof(combined), "%s %s", arg4, argument);
            else
                snprintf(combined, sizeof(combined), "%s", arg4);
            argument = combined;
        }

        /* For ban types, check if we need extra data */
        if (type == PENALTY_BAN_IP || type == PENALTY_BAN_EMAIL
            || type == PENALTY_BAN_HOST || type == PENALTY_NOCHAT) {
            char extra_arg[MAX_INPUT_LENGTH];
            char *temp = one_argument(argument, extra_arg);
            if (extra_arg[0] != '\0') {
                extra_data = extra_arg;
                argument = temp;
            }
        }

        if (argument[0] == '\0') {
            send_to_char("Provide a reason for the penalty.\n\r", ch);
            return;
        }

        p = add_penalty(account, type, scope, argument, ch->name,
                        expires_at, target_name, extra_data);

        if (!p) {
            send_to_char("Failed to add penalty.\n\r", ch);
            return;
        }

        save_account(account);

        {
            char dur_buf[64];
            if (expires_at == 0)
                snprintf(dur_buf, sizeof(dur_buf), "permanent");
            else
                penalty_format_duration(expires_at - current_time,
                    dur_buf, sizeof(dur_buf));

            snprintf(buf, sizeof(buf),
                "Added {W%s{x penalty (%s) to account {W%s{x%s%s%s.\n\r"
                "  Reason: %s\n\r",
                penalty_type_name(type), dur_buf, account->username,
                scope == PENALTY_SCOPE_CHARACTER ? " (character: " : "",
                scope == PENALTY_SCOPE_CHARACTER && target_name
                    ? target_name : "",
                scope == PENALTY_SCOPE_CHARACTER ? ")" : "",
                argument);
            send_to_char(buf, ch);

            snprintf(buf, sizeof(buf),
                "$N applied %s penalty (%s) to account %s: %s",
                penalty_type_name(type), dur_buf,
                account->username, argument);
            wiznet(buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
        }
        return;
    }

    send_to_char("Unknown subcommand. Use: list, add, remove, info.\n\r", ch);
}

/**
 * do_bonus - Staff command to manage account bonuses
 *
 * Syntax:
 *   bonus list <account|player:name>
 *   bonus add <account|player:name> <type> <value> [scope] [duration] <reason>
 *   bonus remove <account|player:name> <#>
 *   bonus info <account|player:name> <#>
 *   bonus assign <#> <character>  (player command - assign unassigned bonus)
 *
 * Types: xp, gold, charslots, staffslots, qp, train, custom
 *
 * @param ch        Staff member or player issuing the command
 * @param argument  Full argument string
 */
void do_bonus(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char arg3[MAX_INPUT_LENGTH];
    char arg4[MAX_INPUT_LENGTH];
    ACCOUNT_DATA *account;
    bool loaded = false;

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0') {
        if (IS_IMMORTAL(ch)) {
            send_to_char("Syntax: bonus list <account|player:name>\n\r", ch);
            send_to_char("        bonus add <account|player:name> <type>"
                         " <value> [scope] [duration] <reason>\n\r", ch);
            send_to_char("        bonus remove <account|player:name> <#>\n\r", ch);
            send_to_char("        bonus info <account|player:name> <#>\n\r", ch);
        }
        send_to_char("        bonus assign <#> <character>"
                     "  (assign an unassigned bonus)\n\r", ch);
        send_to_char("\n\rTypes: xp, gold, charslots, staffslots,"
                     " qp, train, custom\n\r", ch);
        return;
    }

    /*
     * PLAYER SUBCOMMAND: assign
     * No staff requirement — players can assign their own unassigned bonuses
     */
    if (!str_prefix(arg1, "assign")) {
        argument = one_argument(argument, arg2); /* index */
        argument = one_argument(argument, arg3); /* character name */

        if (!is_number(arg2) || arg3[0] == '\0') {
            send_to_char("Syntax: bonus assign <#> <character>\n\r", ch);
            return;
        }

        if (IS_NPC(ch) || !ch->pcdata || !ch->pcdata->account_name[0]) {
            send_to_char("You don't have an account.\n\r", ch);
            return;
        }

        account = get_account_by_name(ch->pcdata->account_name);
        if (!account) {
            send_to_char("Account error.\n\r", ch);
            return;
        }

        int index = atoi(arg2);
        if (assign_bonus_to_character(account, index, arg3)) {
            char buf[MAX_STRING_LENGTH];
            BONUS_DATA *b = get_bonus_by_index(account, index);

            snprintf(buf, sizeof(buf),
                "Assigned %s bonus (%d) to character {W%s{x.\n\r",
                bonus_type_name(b->type), b->value, arg3);
            send_to_char(buf, ch);
            save_account(account);
        } else {
            send_to_char("Cannot assign that bonus. It may not be "
                "unassigned or player-selectable.\n\r", ch);
        }
        return;
    }

    /* Remaining subcommands require staff */
    if (!IS_IMMORTAL(ch)) {
        send_to_char("Only staff can use that subcommand.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg2);

    if (arg2[0] == '\0') {
        send_to_char("Specify an account name or player:name.\n\r", ch);
        return;
    }

    account = get_account_by_identifier(arg2, &loaded);
    if (!account) {
        send_to_char("Account not found.\n\r", ch);
        return;
    }

    /*
     * SUBCOMMAND: list
     */
    if (!str_prefix(arg1, "list")) {
        BONUS_DATA *b;
        char buf[MAX_STRING_LENGTH];
        char dur_buf[64];
        int i = 0;

        expire_bonuses(account);

        if (!account->bonuses) {
            send_to_char("No bonuses on this account.\n\r", ch);
            return;
        }

        snprintf(buf, sizeof(buf),
            "{YBonuses for account {W%s{Y:{x\n\r"
            "{Y #  Type        Value  Scope      Expires     Granted By{x\n\r"
            "{Y--- ----------- ------ ---------- ----------- ----------------{x\n\r",
            account->username);
        send_to_char(buf, ch);

        for (b = account->bonuses; b; b = b->next) {
            i++;

            if (b->expires_at == 0) {
                snprintf(dur_buf, sizeof(dur_buf), "permanent");
            } else {
                time_t remaining = b->expires_at - current_time;
                if (remaining <= 0)
                    snprintf(dur_buf, sizeof(dur_buf), "{Dexpired{x");
                else
                    penalty_format_duration(remaining, dur_buf, sizeof(dur_buf));
            }

            const char *scope_str;
            if (b->scope == BONUS_SCOPE_UNASSIGNED)
                scope_str = "{Yunassigned{x";
            else if (b->scope == BONUS_SCOPE_CHARACTER)
                scope_str = b->target_name;
            else
                scope_str = "account";

            snprintf(buf, sizeof(buf),
                "{W%2d{x  %-11s %5d  %-10s %-11s %s\n\r",
                i,
                bonus_type_name(b->type),
                b->value,
                scope_str,
                dur_buf,
                b->granted_by);
            send_to_char(buf, ch);
        }
        return;
    }

    /*
     * SUBCOMMAND: info
     */
    if (!str_prefix(arg1, "info")) {
        BONUS_DATA *b;
        char buf[MAX_STRING_LENGTH];
        char time_buf[64];
        char dur_buf[64];
        int index;

        argument = one_argument(argument, arg3);
        if (!is_number(arg3)) {
            send_to_char("Specify a bonus number. Use 'bonus list' first.\n\r", ch);
            return;
        }
        index = atoi(arg3);
        b = get_bonus_by_index(account, index);
        if (!b) {
            send_to_char("Invalid bonus number.\n\r", ch);
            return;
        }

        format_timestamp(b->granted_at, time_buf, sizeof(time_buf));
        if (b->expires_at == 0)
            snprintf(dur_buf, sizeof(dur_buf), "permanent");
        else {
            time_t remaining = b->expires_at - current_time;
            if (remaining <= 0)
                snprintf(dur_buf, sizeof(dur_buf), "{Dexpired{x");
            else
                penalty_format_duration(remaining, dur_buf, sizeof(dur_buf));
        }

        snprintf(buf, sizeof(buf),
            "{YBonus #%d on {W%s{Y:{x\n\r"
            "  Type:       %s\n\r"
            "  Value:      %d\n\r"
            "  Scope:      %s%s%s\n\r"
            "  Granted by: %s\n\r"
            "  Granted at: %s\n\r"
            "  Expires:    %s\n\r"
            "  Flags:      %s%s\n\r"
            "  Reason:     %s\n\r",
            index, account->username,
            bonus_type_name(b->type),
            b->value,
            bonus_scope_name(b->scope),
            b->scope == BONUS_SCOPE_CHARACTER ? " (" : "",
            b->scope == BONUS_SCOPE_CHARACTER
                ? b->target_name : "",
            b->granted_by,
            time_buf,
            dur_buf,
            IS_SET(b->flags, BONUS_FLAG_PLAYER_SELECT)
                ? "player-select " : "",
            IS_SET(b->flags, BONUS_FLAG_STACKABLE)
                ? "stackable" : "",
            b->reason);
        send_to_char(buf, ch);

        if (b->extra && b->extra[0]) {
            snprintf(buf, sizeof(buf), "  Extra:      %s\n\r", b->extra);
            send_to_char(buf, ch);
        }
        return;
    }

    /*
     * SUBCOMMAND: remove
     */
    if (!str_prefix(arg1, "remove")) {
        char buf[MAX_STRING_LENGTH];
        int index;
        BONUS_DATA *b;

        argument = one_argument(argument, arg3);
        if (!is_number(arg3)) {
            send_to_char("Specify a bonus number. Use 'bonus list' first.\n\r", ch);
            return;
        }
        index = atoi(arg3);

        b = get_bonus_by_index(account, index);
        if (!b) {
            send_to_char("Invalid bonus number.\n\r", ch);
            return;
        }

        snprintf(buf, sizeof(buf),
            "$N removed %s bonus (%d) from account %s",
            bonus_type_name(b->type), b->value, account->username);
        wiznet(buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);

        snprintf(buf, sizeof(buf),
            "Removed %s bonus (#%d, value %d) from account {W%s{x.\n\r",
            bonus_type_name(b->type), index, b->value, account->username);

        remove_bonus(account, index);
        save_account(account);

        send_to_char(buf, ch);
        return;
    }

    /*
     * SUBCOMMAND: add
     */
    if (!str_prefix(arg1, "add")) {
        int type, scope;
        int value;
        long flags = 0;
        time_t expires_at = 0;
        char *target_name = NULL;
        char buf[MAX_STRING_LENGTH * 2];
        BONUS_DATA *b;

        /* Parse type */
        argument = one_argument(argument, arg3);
        if (arg3[0] == '\0') {
            send_to_char("Specify a bonus type.\n\r", ch);
            return;
        }
        type = bonus_type_lookup(arg3);
        if (type < 0) {
            send_to_char("Unknown bonus type. Valid types:\n\r"
                "  xp, gold, charslots, staffslots, qp, train, custom\n\r", ch);
            return;
        }

        /* Parse value */
        argument = one_argument(argument, arg4);
        if (!is_number(arg4)) {
            send_to_char("Specify a numeric value for the bonus.\n\r", ch);
            return;
        }
        value = atoi(arg4);

        /* Default scope */
        scope = BONUS_SCOPE_ACCOUNT;

        /* Parse remaining: [scope] [player-select] [stackable] [duration] reason */
        char next_arg[MAX_INPUT_LENGTH];
        argument = one_argument(argument, next_arg);

        while (next_arg[0] != '\0') {
            int test_scope = bonus_scope_lookup(next_arg);
            if (test_scope >= 0) {
                scope = test_scope;
                if (scope == BONUS_SCOPE_CHARACTER) {
                    /* Next arg is the target name */
                    argument = one_argument(argument, next_arg);
                    if (next_arg[0] == '\0') {
                        send_to_char("Specify a character name for "
                            "character-scoped bonus.\n\r", ch);
                        return;
                    }
                    target_name = next_arg;
                }
                argument = one_argument(argument, next_arg);
                continue;
            }

            if (!str_cmp(next_arg, "player-select")
                || !str_cmp(next_arg, "playerselect")) {
                SET_BIT(flags, BONUS_FLAG_PLAYER_SELECT);
                scope = BONUS_SCOPE_UNASSIGNED;
                argument = one_argument(argument, next_arg);
                continue;
            }

            if (!str_cmp(next_arg, "stackable")) {
                SET_BIT(flags, BONUS_FLAG_STACKABLE);
                argument = one_argument(argument, next_arg);
                continue;
            }

            /* Check if it's a duration */
            if (isdigit((unsigned char)next_arg[0])
                || !str_prefix(next_arg, "perm")) {
                time_t dur = parse_duration(next_arg);
                if (dur > 0)
                    expires_at = current_time + dur;
                argument = one_argument(argument, next_arg);
                continue;
            }

            /* Must be start of reason */
            break;
        }

        /* Reconstruct reason from next_arg + remaining argument */
        char reason_buf[MAX_STRING_LENGTH];
        if (argument[0] != '\0')
            snprintf(reason_buf, sizeof(reason_buf), "%s %s",
                next_arg, argument);
        else
            snprintf(reason_buf, sizeof(reason_buf), "%s", next_arg);

        if (reason_buf[0] == '\0') {
            send_to_char("Provide a reason for the bonus.\n\r", ch);
            return;
        }

        b = add_bonus(account, type, scope, reason_buf, ch->name,
                      expires_at, value, target_name, flags, NULL);

        if (!b) {
            send_to_char("Failed to add bonus.\n\r", ch);
            return;
        }

        save_account(account);

        {
            char dur_buf[64];
            if (expires_at == 0)
                snprintf(dur_buf, sizeof(dur_buf), "permanent");
            else
                penalty_format_duration(expires_at - current_time,
                    dur_buf, sizeof(dur_buf));

            snprintf(buf, sizeof(buf),
                "Added {W%s{x bonus (value: %d, %s) to account {W%s{x.\n\r"
                "  Reason: %s\n\r",
                bonus_type_name(type), value, dur_buf,
                account->username, reason_buf);
            send_to_char(buf, ch);

            snprintf(buf, sizeof(buf),
                "$N granted %s bonus (%d, %s) to account %s: %s",
                bonus_type_name(type), value, dur_buf,
                account->username, reason_buf);
            wiznet(buf, ch, NULL, WIZ_PENALTIES, WIZ_SECURE, 0);
        }
        return;
    }

    send_to_char("Unknown subcommand. Use: list, add, remove, info, assign.\n\r", ch);
}

/***************************************************************************
 *  Account Unlock System - Implementation                                 *
 *                                                                         *
 *  Manages unlockable content (races, etc.) tied to player accounts.      *
 ***************************************************************************/

#include <string.h>
#include <stdio.h>
#include <jansson.h>

#include "../merc.h"
#include "../interp.h"
#include "../tables.h"
#include "unlock.h"

/* Forward declarations */
extern void save_account(ACCOUNT_DATA *account);
extern ACCOUNT_DATA *get_account_by_identifier(const char *identifier, bool *was_loaded);

/***************************************************************************
 * Race Unlock Functions                                                   *
 ***************************************************************************/

/**
 * account_has_race_unlock - Check if account has a race unlocked
 *
 * Iterates the account's avail_races list looking for a matching race ID.
 *
 * @param account  The account to check
 * @param race_id  Race string identifier (e.g., "vampire", "avatar")
 * @return         true if the race is unlocked on this account
 */
bool account_has_race_unlock(ACCOUNT_DATA *account, const char *race_id)
{
    ITERATOR it;
    char *id;

    if (!account || !race_id || !account->avail_races)
        return false;

    iterator_start(&it, account->avail_races);
    while ((id = (char *)iterator_nextdata(&it))) {
        if (!str_cmp(id, race_id)) {
            iterator_stop(&it);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}

/**
 * account_add_race_unlock - Grant a race unlock to an account
 *
 * Adds the race to the account's unlocked list if not already present.
 * Does NOT save the account — caller is responsible for saving.
 *
 * @param account  The account to grant the unlock to
 * @param race_id  Race string identifier to unlock
 * @return         true if newly unlocked, false if already unlocked
 */
bool account_add_race_unlock(ACCOUNT_DATA *account, const char *race_id)
{
    if (!account || !race_id)
        return false;

    if (account_has_race_unlock(account, race_id))
        return false;

    if (!account->avail_races)
        account->avail_races = list_create(false);

    list_appendlink(account->avail_races, str_dup(race_id));
    return true;
}

/**
 * account_remove_race_unlock - Revoke a race unlock from an account
 *
 * @param account  The account to revoke from
 * @param race_id  Race string identifier to revoke
 * @return         true if removed, false if wasn't unlocked
 */
bool account_remove_race_unlock(ACCOUNT_DATA *account, const char *race_id)
{
    ITERATOR it;
    char *id;

    if (!account || !race_id || !account->avail_races)
        return false;

    iterator_start(&it, account->avail_races);
    while ((id = (char *)iterator_nextdata(&it))) {
        if (!str_cmp(id, race_id)) {
            iterator_stop(&it);
            list_remlink(account->avail_races, id, false);
            free_string(id);
            return true;
        }
    }
    iterator_stop(&it);

    return false;
}

/**
 * account_race_unlock_count - Count unlocked races on an account
 *
 * @param account  The account to check
 * @return         Number of unlocked races
 */
int account_race_unlock_count(ACCOUNT_DATA *account)
{
    if (!account || !account->avail_races)
        return 0;

    return list_size(account->avail_races);
}

/***************************************************************************
 * Race Availability Functions                                             *
 ***************************************************************************/

/**
 * race_available_for_creation - Check if a race can be chosen at creation
 *
 * A race is available if:
 *   1. It is playable AND starting (normal starting race), OR
 *   2. It is playable AND unlocked on the player's account
 *
 * Remort-only races are available only if explicitly unlocked.
 *
 * @param race     The race to check
 * @param account  The player's account (may be NULL for no unlock check)
 * @return         true if the race can be chosen
 */
bool race_available_for_creation(RACE_DATA *race, ACCOUNT_DATA *account)
{
    if (!race || !race->playable)
        return false;

    /* Shaper is a special internal race, never selectable */
    if (!str_cmp(race->id, "shaper"))
        return false;

    /* Starting races are always available */
    if (race->starting)
        return true;

    /* Non-starting races require an account unlock */
    if (account && account_has_race_unlock(account, race->id))
        return true;

    return false;
}

/***************************************************************************
 * JSON Serialization                                                      *
 ***************************************************************************/

/**
 * unlocked_races_to_json - Serialize unlocked races to a JSON array
 *
 * @param account  The account whose unlocks to serialize
 * @return         JSON array of race ID strings, or NULL if none
 */
json_t *unlocked_races_to_json(ACCOUNT_DATA *account)
{
    json_t *arr;
    ITERATOR it;
    char *id;

    if (!account || !account->avail_races || list_size(account->avail_races) == 0)
        return NULL;

    arr = json_array();

    iterator_start(&it, account->avail_races);
    while ((id = (char *)iterator_nextdata(&it))) {
        json_array_append_new(arr, json_string(id));
    }
    iterator_stop(&it);

    return arr;
}

/**
 * json_to_unlocked_races - Load unlocked races from a JSON array
 *
 * @param arr      JSON array of race ID strings
 * @param account  The account to populate
 */
void json_to_unlocked_races(json_t *arr, ACCOUNT_DATA *account)
{
    size_t index;
    json_t *val;

    if (!arr || !json_is_array(arr) || !account)
        return;

    if (!account->avail_races)
        account->avail_races = list_create(false);

    json_array_foreach(arr, index, val) {
        const char *str = json_string_value(val);
        if (str && str[0]) {
            /* Avoid duplicates during load */
            if (!account_has_race_unlock(account, str))
                list_appendlink(account->avail_races, str_dup(str));
        }
    }
}

/***************************************************************************
 * Staff Command: do_raceunlock                                            *
 *                                                                         *
 * Syntax:                                                                 *
 *   raceunlock <account> list          - Show unlocked races              *
 *   raceunlock <account> add <race>    - Grant a race unlock              *
 *   raceunlock <account> remove <race> - Revoke a race unlock             *
 ***************************************************************************/

/**
 * do_raceunlock - Staff command to manage account race unlocks
 *
 * @param ch        Staff character executing the command
 * @param argument  Command arguments
 */
void do_raceunlock(CHAR_DATA *ch, char *argument)
{
    char arg_account[MAX_INPUT_LENGTH];
    char arg_sub[MAX_INPUT_LENGTH];
    char arg_race[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];

    argument = one_argument(argument, arg_account);
    argument = one_argument(argument, arg_sub);

    if (IS_NULLSTR(arg_account) || IS_NULLSTR(arg_sub)) {
        send_to_char("Syntax: raceunlock <account> list\n\r", ch);
        send_to_char("        raceunlock <account> add <race>\n\r", ch);
        send_to_char("        raceunlock <account> remove <race>\n\r", ch);
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
        int count = account_race_unlock_count(account);

        sprintf(buf, "{CUnlocked races for account {W%s{x:\n\r", account->username);
        send_to_char(buf, ch);

        if (count == 0) {
            send_to_char("   No races unlocked.\n\r", ch);
            return;
        }

        ITERATOR it;
        char *id;
        int i = 0;

        send_to_char("{D+----+----------------+----------+{x\n\r", ch);
        send_to_char("{D|{x {C#{x  {D|{x {CRace ID{x        {D|{x {CValid{x    {D|{x\n\r", ch);
        send_to_char("{D+----+----------------+----------+{x\n\r", ch);

        iterator_start(&it, account->avail_races);
        while ((id = (char *)iterator_nextdata(&it))) {
            RACE_DATA *race = race_lookup(id);
            i++;
            sprintf(buf, "{D|{x %-2d {D|{x %-14s {D|{x %-8s {D|{x\n\r",
                    i, id,
                    race ? "{GYes{x" : "{RNo{x");
            send_to_char(buf, ch);
        }
        iterator_stop(&it);

        send_to_char("{D+----+----------------+----------+{x\n\r", ch);
        sprintf(buf, "   %d race%s unlocked.\n\r", count, count == 1 ? "" : "s");
        send_to_char(buf, ch);
        return;
    }

    /*
     * SUBCOMMAND: add
     */
    if (!str_prefix(arg_sub, "add")) {
        argument = one_argument(argument, arg_race);
        if (IS_NULLSTR(arg_race)) {
            send_to_char("Syntax: raceunlock <account> add <race>\n\r", ch);
            return;
        }

        /* Validate the race exists */
        RACE_DATA *race = race_lookup(arg_race);
        if (!race) {
            send_to_char("That race does not exist.\n\r", ch);
            return;
        }

        if (!race->playable) {
            send_to_char("That race is not playable.\n\r", ch);
            return;
        }

        if (race->starting) {
            send_to_char("That race is already a starting race and doesn't need to be unlocked.\n\r", ch);
            return;
        }

        if (account_add_race_unlock(account, race->id)) {
            save_account(account);
            sprintf(buf, "Race '%s' unlocked for account %s.\n\r",
                    race->name, account->username);
            send_to_char(buf, ch);

            /* Log it */
            snprintf(buf, sizeof(buf),
                     "$N unlocked race '%s' for account %s",
                     race->name, account->username);
            {
                log_context_t ctx = {
                    .actor_type = IS_NPC(ch) ? "npc" : "player",
                    .actor_name = IS_NPC(ch) ? ch->short_descr : ch->name,
                    .actor_uid = { ch->id[0], ch->id[1] },
                    .actor_wnum = (IS_NPC(ch) && ch->pIndexData)
                                  ? widevnum_string_mobile(ch->pIndexData, NULL) : NULL,
                    .action = "raceunlock_add",
                };
                log_event_t ev = {
                    .severity = EVENT_SEV_INFO,
                    .category = LOG_SECURITY,
                    .plain_message = buf,
                    .staff_message = buf,
                    .wiznet_flag = WIZ_SECURE,
                    .wiznet_min_rank = get_mob_level(ch),
                    .context = &ctx,
                    .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
                };
                log_emit_event(&ev, ch);
            }
        } else {
            send_to_char("That race is already unlocked on that account.\n\r", ch);
        }
        return;
    }

    /*
     * SUBCOMMAND: remove
     */
    if (!str_prefix(arg_sub, "remove")) {
        argument = one_argument(argument, arg_race);
        if (IS_NULLSTR(arg_race)) {
            send_to_char("Syntax: raceunlock <account> remove <race>\n\r", ch);
            return;
        }

        if (account_remove_race_unlock(account, arg_race)) {
            save_account(account);
            sprintf(buf, "Race '%s' unlock removed from account %s.\n\r",
                    arg_race, account->username);
            send_to_char(buf, ch);

            snprintf(buf, sizeof(buf),
                     "$N revoked race '%s' unlock from account %s",
                     arg_race, account->username);
            {
                log_context_t ctx = {
                    .actor_type = IS_NPC(ch) ? "npc" : "player",
                    .actor_name = IS_NPC(ch) ? ch->short_descr : ch->name,
                    .actor_uid = { ch->id[0], ch->id[1] },
                    .actor_wnum = (IS_NPC(ch) && ch->pIndexData)
                                  ? widevnum_string_mobile(ch->pIndexData, NULL) : NULL,
                    .action = "raceunlock_remove",
                };
                log_event_t ev = {
                    .severity = EVENT_SEV_INFO,
                    .category = LOG_SECURITY,
                    .plain_message = buf,
                    .staff_message = buf,
                    .wiznet_flag = WIZ_SECURE,
                    .wiznet_min_rank = get_mob_level(ch),
                    .context = &ctx,
                    .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
                };
                log_emit_event(&ev, ch);
            }
        } else {
            send_to_char("That race is not unlocked on that account.\n\r", ch);
        }
        return;
    }

    send_to_char("Invalid subcommand. Use: list, add, remove\n\r", ch);
}

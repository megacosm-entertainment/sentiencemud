/***************************************************************************
 *  Account Unlock System                                                  *
 *                                                                         *
 *  Manages unlockable content (races, etc.) tied to player accounts.      *
 *  Unlocks persist across characters and are earned through gameplay:     *
 *    - Remorting into a race unlocks it for new characters                *
 *    - Earning a transformation (vampire, lich, slayer) unlocks it       *
 *    - Staff can grant/revoke unlocks manually                           *
 *                                                                         *
 *  During character creation, available races = starting races +          *
 *  account-unlocked races.                                                *
 ***************************************************************************/

#ifndef UNLOCK_H
#define UNLOCK_H

#include "../merc.h"

/***************************************************************************
 * Race Unlock Functions                                                   *
 ***************************************************************************/

/**
 * account_has_race_unlock - Check if account has a race unlocked
 *
 * @param account  The account to check
 * @param race_id  Race string identifier (e.g., "vampire", "avatar")
 * @return         true if the race is unlocked on this account
 */
bool account_has_race_unlock(ACCOUNT_DATA *account, const char *race_id);

/**
 * account_add_race_unlock - Grant a race unlock to an account
 *
 * Adds the race to the account's unlocked list if not already present.
 * Saves the account automatically.
 *
 * @param account  The account to grant the unlock to
 * @param race_id  Race string identifier to unlock
 * @return         true if newly unlocked, false if already unlocked
 */
bool account_add_race_unlock(ACCOUNT_DATA *account, const char *race_id);

/**
 * account_remove_race_unlock - Revoke a race unlock from an account
 *
 * @param account  The account to revoke from
 * @param race_id  Race string identifier to revoke
 * @return         true if removed, false if wasn't unlocked
 */
bool account_remove_race_unlock(ACCOUNT_DATA *account, const char *race_id);

/**
 * account_race_unlock_count - Count unlocked races on an account
 *
 * @param account  The account to check
 * @return         Number of unlocked races
 */
int account_race_unlock_count(ACCOUNT_DATA *account);

/***************************************************************************
 * Race Availability Functions                                             *
 ***************************************************************************/

/**
 * race_available_for_creation - Check if a race can be chosen at creation
 *
 * A race is available if it is a starting race OR if the player's account
 * has it unlocked. Remort-only and non-playable races are never available
 * unless explicitly unlocked.
 *
 * @param race     The race to check
 * @param account  The player's account (may be NULL for no unlock check)
 * @return         true if the race can be chosen
 */
bool race_available_for_creation(RACE_DATA *race, ACCOUNT_DATA *account);

/***************************************************************************
 * JSON Serialization                                                      *
 ***************************************************************************/

/**
 * unlocked_races_to_json - Serialize unlocked races to a JSON array
 *
 * @param account  The account whose unlocks to serialize
 * @return         JSON array of race ID strings, or NULL if none
 */
json_t *unlocked_races_to_json(ACCOUNT_DATA *account);

/**
 * json_to_unlocked_races - Load unlocked races from a JSON array
 *
 * @param arr      JSON array of race ID strings
 * @param account  The account to populate
 */
void json_to_unlocked_races(json_t *arr, ACCOUNT_DATA *account);

/***************************************************************************
 * Staff Command                                                           *
 ***************************************************************************/

/**
 * do_raceunlock - Staff command to manage account race unlocks
 *
 * Syntax:
 *   raceunlock <account> list          - Show unlocked races
 *   raceunlock <account> add <race>    - Grant a race unlock
 *   raceunlock <account> remove <race> - Revoke a race unlock
 *
 * @param ch        Staff character executing the command
 * @param argument  Command arguments
 */
void do_raceunlock(CHAR_DATA *ch, char *argument);

#endif /* UNLOCK_H */

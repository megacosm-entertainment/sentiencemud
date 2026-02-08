/**
 * bootstrap_account.c - Account and character creation for bootstrap
 *
 * Creates the first staff account with implementor privileges.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../merc.h"
#include "../recycle.h"
#include "../account/auth.h"
#include "../account/auth_sodium.h"
#include "bootstrap_internal.h"

/**
 * bootstrap_create_account_and_character - Create staff account and character
 *
 * Creates a new account with the given credentials, marks it as a staff
 * account, creates an implementor character, and saves both to disk.
 *
 * @param username  Account username
 * @param email     Email address (may be empty)
 * @param password  Plain-text password (will be hashed)
 * @return          true on success
 */
bool bootstrap_create_account_and_character(const char *username, const char *email, const char *password)
{
    ACCOUNT_DATA *account;
    CHAR_DATA *ch;
    char *hashed_pwd;

    printf("\nCreating staff account '%s'...\n", username);

    /* Create account structure */
    account = new_account();
    if (!account) {
        fprintf(stderr, "Failed to allocate account structure\n");
        return false;
    }

    /* Set basic account fields */
    account->username = str_dup(username);
    account->email = str_dup(email);

    /* Hash password using Argon2id */
    hashed_pwd = hash_password(password);
    if (!hashed_pwd) {
        fprintf(stderr, "Failed to hash password\n");
        free_account(account);
        return false;
    }
    account->passwd = str_dup(hashed_pwd);
    free(hashed_pwd);

    account->passwd_version = PWD_VER_ARGON2ID;
    account->creation_date = current_time;
    account->last_login = 0;
    account->staff_account = true;
    account->character_limit = 10;
    account->staff_limit = 10;

    /* Save account */
    save_account(account);
    printf("Account created and saved.\n");

    /* Create implementor character */
    printf("Creating implementor character '%s'...\n", username);

    ch = new_char();
    if (!ch) {
        fprintf(stderr, "Failed to allocate character structure\n");
        return false;
    }

    ch->name = str_dup(username);
    ch->pcdata = new_pcdata();
    if (!ch->pcdata) {
        fprintf(stderr, "Failed to allocate pcdata structure\n");
        free_char(ch);
        return false;
    }

    /* Set staff rank to IMPLEMENTOR (max rank) */
    ch->pcdata->staff_rank = STAFF_IMPLEMENTOR;
    ch->pcdata->creation_date = current_time;

    /* Link to account */
    ch->pcdata->account_name = str_dup(account->username);
    ch->pcdata->account_id[0] = account->id[0];
    ch->pcdata->account_id[1] = account->id[1];

    /* Set basic character properties */
    ch->level = MAX_LEVEL;
    ch->tot_level = MAX_LEVEL;

    /* Add to account's character list */
    account_add_character(account, ch);

    /* Save account (updated with character) */
    save_account(account);

    /* Save character */
    save_char_obj(ch);

    printf("Character created with STAFF_IMPLEMENTOR rank.\n");

    /* Clean up character structure */
    free_char(ch);

    return true;
}

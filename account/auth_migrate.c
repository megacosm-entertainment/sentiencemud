#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include "../merc.h"
#include "auth.h"

/*
 * Check if a character needs auth data migration
 * Returns true if the character has auth data in PC_DATA that should be migrated
 */
bool needs_auth_migration(CHAR_DATA *ch)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata)
        return false;

    /* Unlinked characters keep their auth in PC_DATA */
    if (IS_NULLSTR(ch->pcdata->account_name))
        return false;

    /* Linked characters should have empty auth fields in PC_DATA */
    bool has_password = !IS_NULLSTR(ch->pcdata->pwd);
    bool has_mfa = ch->pcdata->mfa_enabled || !IS_NULLSTR(ch->pcdata->mfa_key);
    bool has_email = !IS_NULLSTR(ch->pcdata->email);
    bool has_reset = !IS_NULLSTR(ch->pcdata->reset_code);

    return (has_password || has_mfa || has_email || has_reset);
}

/*
 * Report the auth status of a character (for admin commands)
 * Shows where authentication data is stored
 */
void report_auth_status(CHAR_DATA *ch, CHAR_DATA *viewer)
{
    if (!ch || IS_NPC(ch) || !ch->pcdata) {
        send_to_char("Invalid character.\n\r", viewer);
        return;
    }

    char buf[MAX_STRING_LENGTH];
    sprintf(buf, "\n\r{WAuthentication Status for {C%s{W:{x\n\r", ch->name);
    send_to_char(buf, viewer);

    /* Check account linkage */
    if (IS_NULLSTR(ch->pcdata->account_name)) {
        send_to_char("  {YUnlinked Character{x - Auth stored in character file\n\r", viewer);
        sprintf(buf, "  Password: %s\n\r", !IS_NULLSTR(ch->pcdata->pwd) ? "{GPresent{x" : "{RNone{x");
        send_to_char(buf, viewer);
        sprintf(buf, "  MFA: %s\n\r", ch->pcdata->mfa_enabled ? "{GEnabled{x" : "{RDisabled{x");
        send_to_char(buf, viewer);
        sprintf(buf, "  Email: %s\n\r", !IS_NULLSTR(ch->pcdata->email) ? ch->pcdata->email : "{RNone{x");
        send_to_char(buf, viewer);
    } else {
        /* Linked character */
        bool was_loaded = false;
        ACCOUNT_DATA *acct = get_account_online_or_offline(ch->pcdata->account_name, &was_loaded);
        if (!acct) {
            sprintf(buf, "  {RLinked to account '{C%s{R' but account not found!{x\n\r", ch->pcdata->account_name);
            send_to_char(buf, viewer);
            return;
        }

        sprintf(buf, "  {GLinked to Account{x: {C%s{x\n\r", acct->username);
        send_to_char(buf, viewer);

        /* Find character in account */
        ACCOUNT_CHARACTER *acct_char = NULL;
        bool found = get_character_auth_data(ch, acct, &acct_char);

        if (!found || !acct_char) {
            send_to_char("  {RCharacter not found in account character list!{x\n\r", viewer);
            return;
        }

        /* Check where auth data is stored */
        bool has_char_pwd = !IS_NULLSTR(acct_char->pwd);
        bool has_char_mfa = !IS_NULLSTR(acct_char->mfa_key);
        bool has_pc_pwd = !IS_NULLSTR(ch->pcdata->pwd);
        bool has_pc_mfa = ch->pcdata->mfa_enabled;

        if (has_char_pwd || has_char_mfa) {
            send_to_char("  {YCharacter Override Auth{x - Character has its own password/MFA\n\r", viewer);
            sprintf(buf, "    Account Password: %s\n\r", !IS_NULLSTR(acct->passwd) ? "{GPresent{x" : "{RNone{x");
            send_to_char(buf, viewer);
            sprintf(buf, "    Character Password: %s\n\r", has_char_pwd ? "{GPresent{x" : "{RNone{x");
            send_to_char(buf, viewer);
            sprintf(buf, "    Account MFA: %s\n\r", acct->mfa_enabled ? "{GEnabled{x" : "{RDisabled{x");
            send_to_char(buf, viewer);
            sprintf(buf, "    Character MFA: %s\n\r", acct_char->mfa_enabled ? "{GEnabled{x" : "{RDisabled{x");
            send_to_char(buf, viewer);
        } else {
            send_to_char("  {GAccount Auth{x - Uses account credentials\n\r", viewer);
            sprintf(buf, "    Account Password: %s\n\r", !IS_NULLSTR(acct->passwd) ? "{GPresent{x" : "{RNone{x");
            send_to_char(buf, viewer);
            sprintf(buf, "    Account MFA: %s\n\r", acct->mfa_enabled ? "{GEnabled{x" : "{RDisabled{x");
            send_to_char(buf, viewer);
            sprintf(buf, "    Account Email: %s\n\r", !IS_NULLSTR(acct->email) ? acct->email : "{RNone{x");
            send_to_char(buf, viewer);
        }

        /* Check for migration needs */
        if (has_pc_pwd || has_pc_mfa) {
            send_to_char("\n\r  {Y*** MIGRATION NEEDED ***{x\n\r", viewer);
            send_to_char("  Auth data still present in character file (PC_DATA)\n\r", viewer);
            send_to_char("  Will be migrated on next character save.\n\r", viewer);
        } else {
            send_to_char("\n\r  {G*** Migration Complete ***{x\n\r", viewer);
            send_to_char("  Auth data properly stored in account file.\n\r", viewer);
        }
    }

    send_to_char("\n\r", viewer);
}

/*
 * Get a human-readable description of auth source
 */
const char *get_auth_source_name(auth_source_t source)
{
    switch (source) {
        case AUTH_SOURCE_NONE:      return "None";
        case AUTH_SOURCE_ACCOUNT:   return "Account";
        case AUTH_SOURCE_CHARACTER: return "Character";
        case AUTH_SOURCE_BOTH:      return "Account + Character";
        default:                    return "Unknown";
    }
}

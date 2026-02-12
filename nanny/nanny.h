#ifndef NANNY_H
#define NANNY_H

#include "../merc.h"

/*
 * Nanny System - Shared Header
 *
 * This header declares all functions in the nanny (login/character creation) system.
 * The nanny system is currently in transition from a single 6,447-line file to
 * multiple focused modules.
 *
 * Module Organization (target):
 * - nanny.c - Main nanny() dispatcher and connection handling
 * - nanny_account.c - Account operations and menu
 * - nanny_character.c - Character operations and menu
 * - nanny_creation.c - Character creation flow
 * - nanny_auth.c - Authentication handlers (✅ complete)
 * - nanny_utils.c - Utility functions (✅ complete)
 * - nanny_menus.c - Menu handlers (✅ complete)
 */

/*
 * Account Functions (currently in nanny.c, target: nanny_account.c)
 */
void login_get_account(DESCRIPTOR_DATA *d, char *argument);
void login_get_account_password(DESCRIPTOR_DATA *d, char *argument);
void login_confirm_account_name(DESCRIPTOR_DATA *d, char *argument);
void login_new_account_password(DESCRIPTOR_DATA *d, char *argument);
void login_confirm_account_password(DESCRIPTOR_DATA *d, char *argument);
void login_get_account_email(DESCRIPTOR_DATA *d, char *argument);
void login_change_account_email(DESCRIPTOR_DATA *d, char *argument);
void login_verify_account_email_change(DESCRIPTOR_DATA *d, char *argument);
void login_verify_account_password(DESCRIPTOR_DATA *d, char *argument);
void login_account_mfa_verify_for_settings(DESCRIPTOR_DATA *d, char *argument);
void login_account_mfa_disable_confirm(DESCRIPTOR_DATA *d, char *argument);
void login_account_mfa_menu(DESCRIPTOR_DATA *d, char *argument);
void login_get_account_mfa(DESCRIPTOR_DATA *d, char *argument);
void login_account_mfa_confirm(DESCRIPTOR_DATA *d, char *argument);
void login_confirm_account_email_for_reset(DESCRIPTOR_DATA *d, char *argument);
void login_change_account_password(DESCRIPTOR_DATA *d, char *argument);
void login_confirm_account_password_change(DESCRIPTOR_DATA *d, char *argument);
void login_account_menu(DESCRIPTOR_DATA *d, char *argument);
void display_account_menu(DESCRIPTOR_DATA *d);
void display_account_mfa_menu(DESCRIPTOR_DATA *d, char *argument);
void resend_account_verification_code(DESCRIPTOR_DATA *d);
void setup_account_mfa(DESCRIPTOR_DATA *d);
bool account_has_immortal(ACCOUNT_DATA *acct);

/*
 * Character Functions (currently in nanny.c, target: nanny_character.c)
 */
void login_character_menu(DESCRIPTOR_DATA *d, char *argument);
void login_character_password(DESCRIPTOR_DATA *d, char *argument);
void login_confirm_character_password(DESCRIPTOR_DATA *d, char *argument);
void login_character_mfa_disable_confirm(DESCRIPTOR_DATA *d, char *argument);
void login_character_mfa_menu(DESCRIPTOR_DATA *d, char *argument);
void login_character_mfa_confirm(DESCRIPTOR_DATA *d, char *argument);
void login_character_mfa_verify(DESCRIPTOR_DATA *d, char *argument);
void login_character_mfa_verify_for_settings(DESCRIPTOR_DATA *d, char *argument);
void login_confirm_delete_character(DESCRIPTOR_DATA *d, char *argument);
void login_verify_character_delete(DESCRIPTOR_DATA *d, char *argument);
void login_verify_delete_password(DESCRIPTOR_DATA *d, char *argument);
void login_verify_delete_mfa(DESCRIPTOR_DATA *d, char *argument);
void login_get_char_password(DESCRIPTOR_DATA *d, char *argument);
void login_get_char_mfa(DESCRIPTOR_DATA *d, char *argument);
void login_get_account_mfa_for_char(DESCRIPTOR_DATA *d, char *argument);
void login_verify_unlink_password(DESCRIPTOR_DATA *d, char *argument);
void login_verify_unlink_mfa(DESCRIPTOR_DATA *d, char *argument);
void login_set_unlink_password(DESCRIPTOR_DATA *d, char *argument);
void login_verify_character_email_change(DESCRIPTOR_DATA *d, char *argument);
void login_change_character_email(DESCRIPTOR_DATA *d, char *argument);
void display_character_menu(DESCRIPTOR_DATA *d);
void display_character_mfa_menu(DESCRIPTOR_DATA *d, char *argument);
void display_shared_storage_info(DESCRIPTOR_DATA *d);
void setup_character_mfa(DESCRIPTOR_DATA *d);
void resend_character_verification_code(DESCRIPTOR_DATA *d);
void select_character(DESCRIPTOR_DATA *d, ACCOUNT_CHARACTER *ch_entry);
bool get_character_auth_data(CHAR_DATA *ch, ACCOUNT_DATA *acct, ACCOUNT_CHARACTER **acct_char);

/*
 * Character Creation Functions (currently in nanny.c, target: nanny_creation.c)
 */
void login_creating_new_char(DESCRIPTOR_DATA *d, char *argument);
void login_creating_new_staff_char(DESCRIPTOR_DATA *d, char *argument);
void login_confirm_new_name(DESCRIPTOR_DATA *d, char *argument);
void login_get_ascii(DESCRIPTOR_DATA *d, char *argument);
void login_get_alignment(DESCRIPTOR_DATA *d, char *argument);
void login_get_new_race(DESCRIPTOR_DATA *d, char *argument);
void login_get_new_sex(DESCRIPTOR_DATA *d, char *argument);
void login_get_new_class(DESCRIPTOR_DATA *d, char *argument);
void login_get_sub_class(DESCRIPTOR_DATA *d, char *argument);
void login_get_char_body_type(DESCRIPTOR_DATA *d, char *argument);
void login_char_confirm_default_pronouns(DESCRIPTOR_DATA *d, char *argument);
void login_char_set_custom_pronoun_subj(DESCRIPTOR_DATA *d, char *argument);
void login_char_set_custom_pronoun_obj(DESCRIPTOR_DATA *d, char *argument);
void login_char_set_custom_pronoun_poss_adj(DESCRIPTOR_DATA *d, char *argument);
void login_char_set_custom_pronoun_poss_pron(DESCRIPTOR_DATA *d, char *argument);
void login_char_set_custom_pronoun_refl(DESCRIPTOR_DATA *d, char *argument);
void login_char_set_custom_verb_pref(DESCRIPTOR_DATA *d, char *argument);
void login_char_set_custom_pronouns_confirm(DESCRIPTOR_DATA *d, char *argument);
void login_get_staff_email(DESCRIPTOR_DATA *d, char *argument);
void login_staff_mfa_prompt(DESCRIPTOR_DATA *d, char *argument);
void login_staff_password(DESCRIPTOR_DATA *d, char *argument);
void login_confirm_staff_password(DESCRIPTOR_DATA *d, char *argument);
void finalize_staff_character_creation(DESCRIPTOR_DATA *d);

/*
 * Character Linking Functions (currently in nanny.c, could go in character or account module)
 */
void login_link_character_name(DESCRIPTOR_DATA *d, char *argument);
void login_link_character_password(DESCRIPTOR_DATA *d, char *argument);
void login_link_character_mfa(DESCRIPTOR_DATA *d, char *argument);
void complete_character_link(DESCRIPTOR_DATA *d);
bool can_link_characters(ACCOUNT_DATA *acct);
bool can_unlink_characters(ACCOUNT_DATA *acct);

/*
 * Legacy/Old System Functions (currently in nanny.c)
 */
void login_get_name(DESCRIPTOR_DATA *d, char *argument);
void login_get_mfa(DESCRIPTOR_DATA *d, char *argument);
void login_change_passwd_initial(DESCRIPTOR_DATA *d, char *argument);
void login_change_passwd_confirm(DESCRIPTOR_DATA *d, char *argument);

/*
 * Connection and Flow Functions (currently in nanny.c, stay in main nanny.c)
 */
void login_break_connect(DESCRIPTOR_DATA *d, char *argument);
void login_read_imotd(DESCRIPTOR_DATA *d, char *argument);
void login_read_motd(DESCRIPTOR_DATA *d, char *argument);
void proceed_to_game(DESCRIPTOR_DATA *d);
bool is_reconnecting(CHAR_DATA *ch);
ACCOUNT_CHARACTER *find_most_recent_character(ACCOUNT_DATA *acct);
bool set_default_character(ACCOUNT_DATA *acct, const char *char_name);

/*
 * Main Nanny Dispatcher (stays in nanny.c)
 */
void nanny(DESCRIPTOR_DATA *d, char *argument);

#endif /* NANNY_H */

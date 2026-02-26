#ifndef NANNY_MENUS_H
#define NANNY_MENUS_H

#include "../merc.h"

/*
 * Nanny Menu System
 *
 * This module breaks up the mega-functions login_account_menu() and login_character_menu()
 * into focused, maintainable handler functions.
 *
 * Each handler corresponds to a single menu option and contains all the logic
 * for that option (validation, state transitions, prompts, etc.).
 *
 * The dispatchers in nanny.c route input to the appropriate handler.
 */

/*
 * Account Menu Handlers
 * Each handles a specific menu option from the account menu
 */

/* Create new regular character (option 'C') */
void handle_account_create_character(DESCRIPTOR_DATA *d);

/* Create new staff character (option 'I') */
void handle_account_create_staff(DESCRIPTOR_DATA *d);

/* Link existing character to account (option 'L') */
void handle_account_link_character(DESCRIPTOR_DATA *d);

/* Change account email (option 'E') */
void handle_account_change_email(DESCRIPTOR_DATA *d);

/* Change account password (option 'P') */
void handle_account_change_password(DESCRIPTOR_DATA *d);

/* Access account MFA menu (option 'M') */
void handle_account_mfa_menu(DESCRIPTOR_DATA *d);

/* Access account preferences menu (option 'A') */
void handle_account_preferences(DESCRIPTOR_DATA *d);

/* Verify email address (option 'V') */
void handle_account_verify_email(DESCRIPTOR_DATA *d);

/* Resend verification email (option 'R') */
void handle_account_resend_verification(DESCRIPTOR_DATA *d);

/* Display shared storage info (option 'S') */
void handle_account_shared_storage(DESCRIPTOR_DATA *d);

/* Logout from account (option 'Q') */
void handle_account_logout(DESCRIPTOR_DATA *d);

/* Quick-login with default character (option 'Y') */
void handle_account_default_character(DESCRIPTOR_DATA *d);

/* Quick-login with most recent character (option 'Z') */
void handle_account_recent_character(DESCRIPTOR_DATA *d);

/* Select character by number from sorted list */
void handle_account_select_by_number(DESCRIPTOR_DATA *d, int choice);

/* Select character by name prefix */
void handle_account_select_by_name(DESCRIPTOR_DATA *d, const char *name);

/*
 * Character Menu Handlers
 * Each handles a specific option from the character-specific menu
 */

/* Log in with this character (option 'L') */
void handle_character_login(DESCRIPTOR_DATA *d);

/* Change character password (option 'P') */
void handle_character_change_password(DESCRIPTOR_DATA *d);

/* Clear character password (option 'X') */
void handle_character_clear_password(DESCRIPTOR_DATA *d);

/* Access character MFA menu (option 'M') */
void handle_character_mfa_menu(DESCRIPTOR_DATA *d);

/* Change character email (option 'E') */
void handle_character_change_email(DESCRIPTOR_DATA *d);

/* Verify character email (option 'V') */
void handle_character_verify_email(DESCRIPTOR_DATA *d);

/* Resend character verification email (option 'R') */
void handle_character_resend_verification(DESCRIPTOR_DATA *d);

/* Unlink character from account (option 'U') */
void handle_character_unlink(DESCRIPTOR_DATA *d);

/* Delete character (option 'D') */
void handle_character_delete(DESCRIPTOR_DATA *d);

/* Cancel character deletion (option 'C') */
void handle_character_cancel_delete(DESCRIPTOR_DATA *d);

/* Set character as default login (option 'Y') */
void handle_character_set_default(DESCRIPTOR_DATA *d);

/* Reset settings to account defaults (option 'T') */
void handle_character_reset_prefs(DESCRIPTOR_DATA *d);

/* Return to account menu (option 'B') */
void handle_character_back_to_account(DESCRIPTOR_DATA *d);

/*
 * Menu Display Functions
 * These are in nanny.c but declared here for reference
 */
extern void display_account_menu(DESCRIPTOR_DATA *d);
extern void display_character_menu(DESCRIPTOR_DATA *d);
extern void display_account_prefs_menu(DESCRIPTOR_DATA *d);

#endif /* NANNY_MENUS_H */
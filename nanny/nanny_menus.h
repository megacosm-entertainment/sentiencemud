#ifndef NANNY_MENUS_H
#define NANNY_MENUS_H

#include "../merc.h"

/*
 * Nanny Menu System
 *
 * This module breaks up the mega-functions login_account_menu() and login_character_menu()
 * into focused, maintainable handler functions.
 *
 * Original:
 * - login_account_menu(): 443 lines
 * - login_character_menu(): 313 lines
 *
 * Refactored:
 * - Dispatcher: ~50 lines
 * - Individual handlers: 20-50 lines each
 */

/*
 * Account Menu Handlers
 * Each handles a specific menu option from the account menu
 */

/* Create new regular character */
void handle_account_create_character(DESCRIPTOR_DATA *d);

/* Create new staff character */
void handle_account_create_staff(DESCRIPTOR_DATA *d);

/* Link existing character to account */
void handle_account_link_character(DESCRIPTOR_DATA *d);

/* Change account email */
void handle_account_change_email(DESCRIPTOR_DATA *d);

/* Change account password */
void handle_account_change_password(DESCRIPTOR_DATA *d);

/* Access account MFA menu */
void handle_account_mfa_menu(DESCRIPTOR_DATA *d);

/* Verify email address */
void handle_account_verify_email(DESCRIPTOR_DATA *d);

/* Logout from account */
void handle_account_logout(DESCRIPTOR_DATA *d);

/* Select character by number */
void handle_account_select_character(DESCRIPTOR_DATA *d, int char_num);

/*
 * Character Menu Handlers
 * Each handles a specific option from the character-specific menu
 */

/* Enter the game with selected character */
void handle_character_enter_game(DESCRIPTOR_DATA *d);

/* Change character password (override) */
void handle_character_change_password(DESCRIPTOR_DATA *d);

/* Access character MFA menu */
void handle_character_mfa_menu(DESCRIPTOR_DATA *d);

/* Change character email */
void handle_character_change_email(DESCRIPTOR_DATA *d);

/* Delete character */
void handle_character_delete(DESCRIPTOR_DATA *d);

/* Unlink character from account */
void handle_character_unlink(DESCRIPTOR_DATA *d);

/* Return to account menu */
void handle_character_back_to_account(DESCRIPTOR_DATA *d);

/* Set character as default */
void handle_character_set_default(DESCRIPTOR_DATA *d);

/*
 * Menu Display Functions
 * These are already in nanny.c but declared here for reference
 */

/* Display the account menu - already exists in nanny.c */
extern void display_account_menu(DESCRIPTOR_DATA *d);

/* Display the character-specific menu - already exists in nanny.c */
extern void display_character_menu(DESCRIPTOR_DATA *d);

#endif /* NANNY_MENUS_H */

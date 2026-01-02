#ifndef NANNY_UTILS_H
#define NANNY_UTILS_H

#include "../merc.h"

/*
 * Nanny Utility Functions
 *
 * Common utilities to reduce duplication in the nanny (login) system.
 */

/* Echo Management */
void nanny_echo_off(DESCRIPTOR_DATA *d);
void nanny_echo_on(DESCRIPTOR_DATA *d);

/* State Transitions */
void nanny_transition(DESCRIPTOR_DATA *d, int new_state);
void nanny_transition_with_prompt(DESCRIPTOR_DATA *d, int new_state, const char *prompt);
void nanny_transition_with_echo(DESCRIPTOR_DATA *d, int new_state, bool echo_on);

/* Common Transition Patterns */
void nanny_return_to_account_menu(DESCRIPTOR_DATA *d);
void nanny_return_to_character_menu(DESCRIPTOR_DATA *d);

/* Input Validation */
bool validate_email_format(const char *email);
bool validate_password_strength_basic(const char *password, char *error_msg, int error_len);
bool validate_account_name(const char *name);

/* Login Attempt Tracking */
bool check_login_attempts(DESCRIPTOR_DATA *d, int max_attempts);
void increment_login_attempts(DESCRIPTOR_DATA *d);
void reset_login_attempts(DESCRIPTOR_DATA *d);

/* Error Handlers */
void nanny_error_invalid_input(DESCRIPTOR_DATA *d, const char *prompt);
void nanny_error_return_to_menu(DESCRIPTOR_DATA *d, const char *error, bool account_menu);

/* Character Cleanup */
void nanny_cleanup_character(DESCRIPTOR_DATA *d);

/* Rate Limiting */
bool check_rate_limit(DESCRIPTOR_DATA *d, const char *operation, time_t min_interval);

/* Dev Mode Checks */
bool should_skip_password(void);
bool should_skip_mfa(void);

/* Message Utilities */
void nanny_send_prompt(DESCRIPTOR_DATA *d, const char *prompt);
void nanny_send_message(DESCRIPTOR_DATA *d, const char *message);
void nanny_send_error(DESCRIPTOR_DATA *d, const char *error);
void nanny_send_success(DESCRIPTOR_DATA *d, const char *message);

#endif /* NANNY_UTILS_H */

/**
 * bootstrap_internal.h - Internal header for bootstrap module
 *
 * Shared declarations used across bootstrap implementation files.
 * Not exposed to the rest of the codebase.
 */

#ifndef BOOTSTRAP_INTERNAL_H
#define BOOTSTRAP_INTERNAL_H

#include <stdbool.h>

/* Utility functions (shared across bootstrap files) */
bool file_exists(const char *path);

/* File creation functions (bootstrap_files.c) */
bool ensure_directory_structure(void);
bool create_gconfig_rc(void);
bool create_area_lst(void);
bool create_limbo_area(void);
bool create_human_race(void);
bool create_game_settings(void);
bool create_ci_test_fixture_areas(void);

/* Reserved entities (bootstrap_reserved.c) */
bool create_reserved_entities(void);

/* Commands generation (bootstrap_commands.c) */
bool create_commands_json(void);

/* Account/character creation (bootstrap_account.c) */
bool bootstrap_create_account_and_character(const char *username, const char *email, const char *password);

/* Interactive prompts (bootstrap_prompts.c) */
char *prompt_username(void);
char *prompt_email(void);
char *prompt_password(const char *prompt);
bool validate_username(const char *username);
bool validate_email(const char *email);

#endif /* BOOTSTRAP_INTERNAL_H */

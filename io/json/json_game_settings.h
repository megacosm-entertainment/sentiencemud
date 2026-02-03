/***************************************************************************
 *  JSON Game Settings - Config file with environment variable overrides  *
 *                                                                         *
 *  Loads game settings from JSON format with support for:                *
 *  - Hierarchical JSON structure organized by category                   *
 *  - Environment variable overrides (e.g., via Doppler)                  *
 *  - Backward compatibility with .dat format                             *
 *  - Human-readable configuration                                        *
 ***************************************************************************/

#ifndef JSON_GAME_SETTINGS_H
#define JSON_GAME_SETTINGS_H

#include "../../merc.h"
#include <jansson.h>

/***************************************************************************
 * Core Functions                                                          *
 ***************************************************************************/

// Load game settings from JSON file with env var overrides
// Returns: 0 on success, 1 on failure
int json_game_settings_read(void);

// Save game settings to JSON file
// Returns: 0 on success, 1 on failure
int json_game_settings_write(void);

// Migrate old .dat format to new .json format
// Returns: 0 on success, 1 on failure
int json_game_settings_migrate(void);

/***************************************************************************
 * Environment Variable Support                                           *
 ***************************************************************************/

// Apply environment variable overrides to loaded settings
// Env vars are prefixed with "SENTIENCE_" and use underscores
// Example: SENTIENCE_EMAIL_HOST, SENTIENCE_TELNET_PORT
void json_apply_env_overrides(void);

// Get environment variable for a specific setting
// Returns value if set, NULL otherwise
const char *json_get_env_for_setting(const char *setting_name);

// Check if a setting is currently overridden by an environment variable
// Returns true if env var exists and is non-empty
bool json_setting_is_env_override(const char *setting_name);

/***************************************************************************
 * Serialization Helpers                                                  *
 ***************************************************************************/

// Convert game_settings to JSON object
json_t *game_settings_to_json(void);

// Load game_settings from JSON object
// Returns: true on success, false on failure
bool json_to_game_settings(json_t *root);

/***************************************************************************
 * Utility Functions                                                      *
 ***************************************************************************/

// Check if game_settings file is in JSON format
bool json_is_game_settings_json(void);

// Get full path to game settings file
void json_get_game_settings_path(char *path_buf, size_t buf_size);

#endif // JSON_GAME_SETTINGS_H

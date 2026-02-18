/***************************************************************************
 *  JSON Game Settings - Implementation                                    *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../tables.h"
#include "json_common.h"
#include "json_game_settings.h"
#include "../../secret.h"
#include "../../account/preferences.h"



// Environment variable prefix


/***************************************************************************
 * Utility Functions                                                       *
 ***************************************************************************/

void json_get_game_settings_path(char *path_buf, size_t buf_size)
{
    snprintf(path_buf, buf_size, "%s", GAME_SETTINGS_JSON_FILE);
}

bool json_is_game_settings_json(void)
{
    return json_file_is_json(GAME_SETTINGS_JSON_FILE);
}

/***************************************************************************
 * Environment Variable Support                                           *
 ***************************************************************************/

// Convert setting name to environment variable name
// Example: "email_host" -> "SENTIENCE_EMAIL_HOST"
static void setting_to_env_name(const char *setting_name, char *env_name, size_t buf_size)
{
    size_t i, j;

    // Add prefix
    snprintf(env_name, buf_size, "%s", game_settings.env_var_prefix);
    j = strlen(game_settings.env_var_prefix);

    // Convert to uppercase
    for (i = 0; setting_name[i] && j < buf_size - 1; i++, j++) {
        env_name[j] = toupper(setting_name[i]);
    }
    env_name[j] = '\0';
}

// Convert environment variable name to setting name
// Example: "SENTIENCE_EMAIL_HOST" -> "email_host"
// Returns false if the key doesn't start with the expected prefix
static bool env_name_to_setting(const char *env_name, char *setting_name, size_t buf_size)
{
    size_t prefix_len = strlen(game_settings.env_var_prefix);
    size_t i;

    // Check if key starts with our prefix (case-insensitive)
    if (strncasecmp(env_name, game_settings.env_var_prefix, prefix_len) != 0) {
        return false;
    }

    // Convert to lowercase setting name
    for (i = 0; env_name[prefix_len + i] && i < buf_size - 1; i++) {
        setting_name[i] = tolower(env_name[prefix_len + i]);
    }
    setting_name[i] = '\0';

    return true;
}

// Find a setting by name in game_settings_table
static const struct game_setting_type *find_setting_by_name(const char *name)
{
    for (int i = 0; game_settings_table[i].name != NULL; i++) {
        if (strcasecmp(game_settings_table[i].name, name) == 0) {
            return &game_settings_table[i];
        }
    }
    return NULL;
}

// Apply an override value to a setting
// Returns true if applied, false if skipped
static bool apply_override(const char *env_name, const char *value, const char *source)
{
    char setting_name[256];
    const struct game_setting_type *setting;

    // Convert env name to setting name
    if (!env_name_to_setting(env_name, setting_name, sizeof(setting_name))) {
        return false;  // Doesn't match our prefix
    }

    // Find the setting
    setting = find_setting_by_name(setting_name);
    if (!setting) {
        return false;  // Not a recognized setting
    }

    // Skip core settings - they are not overridable
    if (setting->category == SETTING_CAT_CORE) {
        return false;
    }

    // Apply the override based on type
    switch (setting->type) {
        case SETTING_TYPE_BOOL: {
            bool *ptr = (bool *)setting->ptr;
            if (strcasecmp(value, "true") == 0 ||
                strcasecmp(value, "yes") == 0 ||
                strcmp(value, "1") == 0) {
                *ptr = true;
                log_stringf("  Override (%s): %s = true", source, setting->name);
            } else if (strcasecmp(value, "false") == 0 ||
                       strcasecmp(value, "no") == 0 ||
                       strcmp(value, "0") == 0) {
                *ptr = false;
                log_stringf("  Override (%s): %s = false", source, setting->name);
            }
            break;
        }

        case SETTING_TYPE_INT: {
            int *ptr = (int *)setting->ptr;
            *ptr = atoi(value);
            log_stringf("  Override (%s): %s = %d", source, setting->name, *ptr);
            break;
        }

        case SETTING_TYPE_STRING:
        case SETTING_TYPE_EXTSTR: {
            char **ptr = (char **)setting->ptr;

            // Don't log sensitive settings
            if (setting->sensitive) {
                log_stringf("  Override (%s): %s = <redacted>", source, setting->name);
            } else {
                log_stringf("  Override (%s): %s = %s", source, setting->name, value);
            }

            // Free old value if it exists
            if (*ptr && *ptr != str_empty) {
                free_string(*ptr);
            }
            *ptr = str_dup(value);
            break;
        }

        case SETTING_TYPE_FLOAT: {
            float *ptr = (float *)setting->ptr;
            *ptr = atof(value);
            log_stringf("  Override (%s): %s = %.2f", source, setting->name, *ptr);
            break;
        }
    }

    return true;
}

// Callback for secret_iterate - applies overrides from secrets mount
static bool secret_override_callback(const char *key, const char *value, void *user_data)
{
    int *count = (int *)user_data;
    if (apply_override(key, value, "mount")) {
        (*count)++;
        return true;
    }
    return false;
}

const char *json_get_env_for_setting(const char *setting_name)
{
    char env_name[256];
    setting_to_env_name(setting_name, env_name, sizeof(env_name));
    return secret_get(env_name);
}

bool json_setting_is_env_override(const char *setting_name)
{
    char env_name[256];
    setting_to_env_name(setting_name, env_name, sizeof(env_name));
    const char *secret_value = secret_get(env_name);
    return (secret_value != NULL && secret_value[0] != '\0');
}

void json_apply_env_overrides(void)
{
    extern char **environ;
    int mount_count = 0;
    int env_count = 0;

    log_string("Applying secret overrides...");

    // First, apply overrides from secrets mount (if available)
    // This iterates only through secrets that actually exist
    int result = secret_iterate(secret_override_callback, &mount_count);
    if (result >= 0) {
        log_stringf("  Applied %d override(s) from secrets mount", mount_count);
    }

    // Then, scan environment variables for any with our prefix
    // This catches env vars not in the mount (or when no mount is configured)
    if (environ) {
        size_t prefix_len = strlen(game_settings.env_var_prefix);

        for (char **env = environ; *env != NULL; env++) {
            // Check if this env var starts with our prefix
            if (strncasecmp(*env, game_settings.env_var_prefix, prefix_len) != 0) {
                continue;
            }

            // Parse KEY=VALUE
            char *eq = strchr(*env, '=');
            if (!eq) continue;

            // Extract key
            size_t key_len = eq - *env;
            char key[256];
            if (key_len >= sizeof(key)) continue;
            strncpy(key, *env, key_len);
            key[key_len] = '\0';

            // Get value (skip the '=')
            const char *value = eq + 1;

            // Skip if this was already applied from mount
            // (secret_get returns mount value if available)
            if (result >= 0) {
                const char *mount_value = secret_get(key);
                // If mount has this key, it was already applied
                if (mount_value && secret_using_mount()) {
                    continue;
                }
            }

            // Apply the override
            if (apply_override(key, value, "env")) {
                env_count++;
            }
        }
    }

    if (env_count > 0) {
        log_stringf("  Applied %d override(s) from environment variables", env_count);
    }

    if (mount_count == 0 && env_count == 0) {
        log_string("  No overrides found");
    }
}
/***************************************************************************
 * JSON Serialization                                                      *
 ***************************************************************************/

json_t *game_settings_to_json(void)
{
    json_t *root = json_object();
    json_t *existing_root = NULL;
    json_t *categories[SETTING_CAT_MAX];  // One for each category
    // NOTE: SETTING_CAT_* values start at 1, not 0, so index 0 is unused
    const char *category_names[] = {
        NULL,        // index 0 - unused (SETTING_CAT values start at 1)
        "core",      // SETTING_CAT_CORE = 1
        "email",     // SETTING_CAT_EMAIL = 2
        "missions",  // SETTING_CAT_MISSION = 3
        "lockers",   // SETTING_CAT_LOCKER = 4
        "vaults",    // SETTING_CAT_VAULT = 5
        "coffers",   // SETTING_CAT_COFFER = 6
        "global",    // SETTING_CAT_GLOBAL = 7
        "security",  // SETTING_CAT_SECURITY = 8
        "mssp",      // SETTING_CAT_MSSP = 9
        "redis",     // SETTING_CAT_REDIS = 10
        "debug"      // SETTING_CAT_DEBUG = 11
    };
    int i;

    /*
     * Load existing settings file so env/mount-overridden keys can retain
     * their file-backed values when we save.
     */
    existing_root = json_file_load(GAME_SETTINGS_JSON_FILE, NULL, NULL, "game_settings_to_json");

    // Create category objects (start at 1 since SETTING_CAT values start at 1)
    for (i = 1; i < SETTING_CAT_MAX; i++) {
        categories[i] = json_object();
    }
    categories[0] = NULL;  // Unused

    // Add metadata
    json_object_set_new(root, "_version", json_string("1.0"));
    json_object_set_new(root, "_format", json_string("game_settings"));

    // Iterate through all settings and add to appropriate category
    for (i = 0; game_settings_table[i].name != NULL; i++) {
        const struct game_setting_type *setting = &game_settings_table[i];
        json_t *category = categories[setting->category];
        json_t *value = NULL;

        /*
         * For env/mount-overridden settings, preserve the existing file value
         * instead of omitting the key or persisting override-injected runtime
         * values. This keeps configuration complete and file-backed defaults
         * intact while runtime overrides still take precedence in memory.
         */
        if (json_setting_is_env_override(setting->name)) {
            if (existing_root && json_is_object(existing_root)) {
                json_t *existing_category = json_object_get(existing_root, category_names[setting->category]);
                if (existing_category && json_is_object(existing_category)) {
                    json_t *existing_value = json_object_get(existing_category, setting->name);
                    if (existing_value) {
                        json_object_set_new(category, setting->name, json_deep_copy(existing_value));
                        continue;
                    }
                }
            }
            /* Fallback: if no prior file value exists, serialize current value. */
        }

        switch (setting->type) {
            case SETTING_TYPE_BOOL: {
                bool *ptr = (bool *)setting->ptr;
                value = json_boolean(*ptr);
                break;
            }

            case SETTING_TYPE_INT: {
                int *ptr = (int *)setting->ptr;
                value = json_integer(*ptr);
                break;
            }

            case SETTING_TYPE_STRING:
            case SETTING_TYPE_EXTSTR: {
                char **ptr = (char **)setting->ptr;
                // Handle empty strings properly
                if (*ptr == NULL || *ptr == str_empty) {
                    value = json_string("");
                } else {
                    value = json_string(*ptr);
                }
                break;
            }

            case SETTING_TYPE_FLOAT: {
                float *ptr = (float *)setting->ptr;
                value = json_real(*ptr);
                break;
            }
        }

        if (value) {
            json_object_set_new(category, setting->name, value);
        }
    }

    // Add all categories to root (start at 1 since SETTING_CAT values start at 1)
    for (i = 1; i < SETTING_CAT_MAX; i++) {
        json_object_set_new(root, category_names[i], categories[i]);
    }

    // Add preference defaults (stored separately from the table-driven settings)
    if (game_settings.pref_defaults) {
        json_object_set_new(root, "preferences", prefs_to_json(game_settings.pref_defaults));
    }

    if (existing_root) {
        json_decref(existing_root);
    }

    return root;
}

bool json_to_game_settings(json_t *root)
{
    // NOTE: SETTING_CAT_* values start at 1, not 0, so index 0 is unused
    const char *category_names[] = {
        NULL,        // index 0 - unused (SETTING_CAT values start at 1)
        "core",      // SETTING_CAT_CORE = 1
        "email",     // SETTING_CAT_EMAIL = 2
        "missions",  // SETTING_CAT_MISSION = 3
        "lockers",   // SETTING_CAT_LOCKER = 4
        "vaults",    // SETTING_CAT_VAULT = 5
        "coffers",   // SETTING_CAT_COFFER = 6
        "global",    // SETTING_CAT_GLOBAL = 7
        "security",  // SETTING_CAT_SECURITY = 8
        "mssp",      // SETTING_CAT_MSSP = 9
        "redis",     // SETTING_CAT_REDIS = 10
        "debug"      // SETTING_CAT_DEBUG = 11
    };
    int i;

    // Iterate through all settings
    for (i = 0; game_settings_table[i].name != NULL; i++) {
        const struct game_setting_type *setting = &game_settings_table[i];
        const char *category_name = category_names[setting->category];
        json_t *category = json_object_get(root, category_name);

        if (!category) {
            continue;  // Category not found, skip
        }

        json_t *value = json_object_get(category, setting->name);
        if (!value) {
            continue;  // Setting not found in JSON, keep default
        }

        // Apply the value based on type
        switch (setting->type) {
            case SETTING_TYPE_BOOL: {
                bool *ptr = (bool *)setting->ptr;
                if (json_is_boolean(value)) {
                    *ptr = json_boolean_value(value);
                } else if (json_is_integer(value)) {
                    *ptr = json_integer_value(value) != 0;
                }
                break;
            }

            case SETTING_TYPE_INT: {
                int *ptr = (int *)setting->ptr;
                if (json_is_integer(value)) {
                    *ptr = json_integer_value(value);
                }
                break;
            }

            case SETTING_TYPE_STRING:
            case SETTING_TYPE_EXTSTR: {
                char **ptr = (char **)setting->ptr;
                if (json_is_string(value)) {
                    const char *str = json_string_value(value);
                    if (str && str[0] != '\0') {
                        *ptr = str_dup(str);
                    } else {
                        *ptr = str_empty;
                    }
                }
                break;
            }

            case SETTING_TYPE_FLOAT: {
                float *ptr = (float *)setting->ptr;
                if (json_is_real(value)) {
                    *ptr = json_real_value(value);
                } else if (json_is_integer(value)) {
                    *ptr = (float)json_integer_value(value);
                }
                break;
            }
        }
    }

    // Load preference defaults (stored separately from the table-driven settings)
    json_t *prefs = json_object_get(root, "preferences");
    if (prefs && json_is_array(prefs)) {
        free_pref_list(game_settings.pref_defaults);
        game_settings.pref_defaults = NULL;
        json_to_prefs(prefs, &game_settings.pref_defaults);
    }

    return true;
}

/***************************************************************************
 * Core Load/Save Functions                                               *
 ***************************************************************************/

// Initialize defaults (extracted from game_settings_read)
static void init_game_settings_defaults(void)
{
    /* Core Settings (not overridable by environment variables) */
    game_settings.env_var_prefix = "SENTIENCE_";
    game_settings.secrets_mount = str_empty;
    game_settings.game_name = str_empty;
    game_settings.login_string = str_empty;
    game_settings.server_description = str_empty;
    game_settings.system_area = str_empty;
    game_settings.testport = false;
    game_settings.dev_server = false;
    game_settings.wizlock = false;
    game_settings.new_acct_lock = false;
    game_settings.new_char_lock = false;
    game_settings.wizlock_msg = str_empty;
    game_settings.new_acct_lock_msg = str_empty;
    game_settings.new_char_lock_msg = str_empty;
    game_settings.logall = false;
    game_settings.note_boot_errors = false;

    /* Auth */
    game_settings.require_uniq_pass_staff = false;
    game_settings.max_login_attempts = 0;
    game_settings.enable_passwd = true;
    game_settings.enable_mfa = true;
    game_settings.require_email_verif = false;

    /* 2FA */
    game_settings.require_2fa_all = false;
    game_settings.require_2fa_staff = false;

    /* Multiplaying & Linking */
    game_settings.allow_mp_acct_all = false;
    game_settings.allow_mp_acct_staff = false;
    game_settings.allow_mp_host_all = false;
    game_settings.allow_mp_host_staff = false;
    game_settings.allow_link_all = false;
    game_settings.allow_unlink_all = false;

    /* Game Systems */
    game_settings.alignment_system = false;
    game_settings.restrict_races_align = false;
    game_settings.restrict_classes_align = false;

    /* Timers */
    game_settings.idle_time = 0;
    game_settings.idle_disconnect_time = 0;
    game_settings.save_cooldown_seconds =  0;

    /* Misc Maximums */
    game_settings.max_alias = 0;
    game_settings.max_characters = 0;
    game_settings.max_orgs = 0;
    game_settings.max_logfile_size = 0;
    game_settings.org_disable_pk_pneuma_cost = 0;

    /* Email */
    game_settings.enable_email = false;
    game_settings.email_port = 0;
    game_settings.email_username = str_empty;
    game_settings.email_host = str_empty;
    game_settings.email_password = str_empty;
    game_settings.email_from_addr = str_empty;
    game_settings.email_from_name = str_empty;

    /* Missions */
    game_settings.max_mission_allowance = 0;
    game_settings.inc_missions = 0;
    game_settings.max_missions = 0;

    /* Locker Settings */
    game_settings.lockers_enabled = false;
    game_settings.locker_rent_enabled = false;
    game_settings.max_locker_weight = 0;
    game_settings.max_locker_items = 0;
    game_settings.locker_rent_cost = 0;
    game_settings.locker_rent_time = 0;
    game_settings.locker_rent_time_max = 0;
    game_settings.locker_additional_cost_per_tier = 0;
    game_settings.locker_additional_slots_per_tier = 0;
    game_settings.locker_additional_weight_per_tier = 0;
    game_settings.locker_tier_max = 0;

    /* Vault Settings */
    game_settings.max_vault_weight = 0;
    game_settings.max_vault_items = 0;
    game_settings.vault_enabled = false;
    game_settings.vault_rent = false;
    game_settings.vault_rent_per_char = false;
    game_settings.vault_rent_cost = 0;
    game_settings.vault_rent_time = 0;
    game_settings.vault_rent_time_max = 0;
    game_settings.vault_additional_cost_per_char = 0;
    game_settings.vault_additional_slots_per_char = 0;
    game_settings.vault_additional_weight_per_char = 0;
    game_settings.vault_require_room = false;

    /* Coffer Settings */
    game_settings.max_coffer_weight = 0;
    game_settings.max_coffer_items = 0;
    game_settings.coffer_enabled = false;
    game_settings.coffer_rent = false;
    game_settings.coffer_rent_cost = 0;
    game_settings.coffer_rent_currency = str_empty;
    game_settings.coffer_rent_time = 0;
    game_settings.coffer_rent_time_max = 0;

    /* Protocols and Ports*/
    game_settings.enable_telnet = false;
    game_settings.telnet_port = 0;
    game_settings.enable_tls = false;
    game_settings.tls_port = 0;
    game_settings.enable_websocket_tls = false;
    game_settings.websocket_tls_port = 0;
    game_settings.enable_web = false;
    game_settings.ssl_cert_path = str_empty;
    game_settings.ssl_key_path = str_empty;
    game_settings.enable_insecure_warning = false;
    game_settings.insecure_warning_msg = str_empty;

    /* MSSP */
    game_settings.mssp_players = 0;
    game_settings.mssp_uptime = 0;
    game_settings.mssp_crawl_delay = 0;
    game_settings.mssp_hostname = str_empty;
    game_settings.mssp_port = 0;
    game_settings.mssp_tls_port = 0;
    game_settings.mssp_codebase = str_empty;
    game_settings.mssp_contact = str_empty;
    game_settings.mssp_created = 0;
    game_settings.mssp_ip = str_empty;
    game_settings.mssp_language = str_empty;
    game_settings.mssp_location = str_empty;
    game_settings.mssp_minimum_age = 0;
    game_settings.mssp_website = str_empty;
    game_settings.mssp_family = str_empty;
    game_settings.mssp_genre = str_empty;
    game_settings.mssp_status = str_empty;
    game_settings.mssp_gamesystem = str_empty;
    game_settings.mssp_intermud = str_empty;
    game_settings.mssp_subgenre = str_empty;
    game_settings.mssp_discord_server = str_empty;
    game_settings.mssp_areas = 0;
    game_settings.mssp_helpfiles = 0;
    game_settings.mssp_mobiles = 0;
    game_settings.mssp_objects = 0;
    game_settings.mssp_rooms = 0;
    game_settings.mssp_classes = 0;
    game_settings.mssp_levels = 0;
    game_settings.mssp_races = 0;
    game_settings.mssp_skills = 0;
    game_settings.mssp_dbsize = 0;
    game_settings.mssp_ansi = false;
    game_settings.mssp_gmcp = false;
    game_settings.mssp_mccp = false;
    game_settings.mssp_mcp = false;
    game_settings.mssp_msdp = false;
    game_settings.mssp_msp = false;
    game_settings.mssp_mxp = false;
    game_settings.mssp_pueb = false;
    game_settings.mssp_utf8 = false;
    game_settings.mssp_vt100 = false;
    game_settings.mssp_xterm256 = false;
    game_settings.mssp_xtermtrue = false;
    game_settings.mssp_atcp = false;
    game_settings.mssp_ssl = false;
    game_settings.mssp_pay2play = false;
    game_settings.mssp_pay4perks = false;
    game_settings.mssp_hiring_builders = false;
    game_settings.mssp_hiring_coders = false;
    game_settings.mssp_adult_material = false;
    game_settings.mssp_multiclass = false;
    game_settings.mssp_newbie_friendly = false;
    game_settings.mssp_player_cities = false;
    game_settings.mssp_player_clans = false;
    game_settings.mssp_player_crafting = false;
    game_settings.mssp_player_guilds = false;
    game_settings.mssp_equipment_system = str_empty;
    game_settings.mssp_multiplaying = str_empty;
    game_settings.mssp_playerkilling = false;
    game_settings.mssp_quest_system = false;
    game_settings.mssp_roleplaying = false;
    game_settings.mssp_training_system = false;
    game_settings.mssp_world_originality = false;

    /* Redis Settings */
    game_settings.enable_redis = true;  /* Default to enabled */
    game_settings.redis_host = str_dup("127.0.0.1");
    game_settings.redis_port = 6379;
    game_settings.redis_password = str_empty;
    game_settings.redis_timeout_sec = 1;
    game_settings.redis_timeout_usec = 500000;

    /* Debug/Logging Settings */
    game_settings.crash_dump_dir = str_empty;

    /* Cryptography Settings */
    game_settings.crypto_key_passphrase = str_empty;
    game_settings.crypto_key_passphrase_previous = str_empty;
    game_settings.crypto_salt_file = str_empty;
    game_settings.crypto_use_passphrase = false;  // Default to file-based key (backward compatible)
    game_settings.crypto_key_version = 1;         // Default version

    /* Preference Defaults */
    game_settings.pref_defaults = NULL;
}

int json_game_settings_read(void)
{
    json_t *root;

    log_string("Loading game settings from JSON...");

    // Initialize defaults first
    init_game_settings_defaults();

    // Try to load JSON file
    root = json_file_load(GAME_SETTINGS_JSON_FILE, NULL, NULL, "json_game_settings_read");
    if (!root) {
        log_stringf("Warning: Could not load %s", GAME_SETTINGS_JSON_FILE);
        log_string("  Will try to migrate from .dat format...");

        // Try migration
        if (json_game_settings_migrate() == 0) {
            log_string("  Migration successful, retrying load...");
            root = json_file_load(GAME_SETTINGS_JSON_FILE, NULL, NULL, "json_game_settings_read");
            if (!root) {
                log_string("Error: Could not load migrated settings");
                return 1;
            }
        } else {
            log_string("  Migration failed, using defaults");
            return 1;
        }
    }

    // Parse JSON into game_settings
    if (!json_to_game_settings(root)) {
        json_decref(root);
        log_string("Error: Failed to parse game settings JSON");
        return 1;
    }

    json_decref(root);

    // Apply environment variable overrides
    json_apply_env_overrides();

    // Apply post-load validation/defaults
    if (game_settings.idle_disconnect_time <= 0)
        game_settings.idle_disconnect_time = 30;

    if (game_settings.idle_time <= 0)
        game_settings.idle_time = 12;

    if (game_settings.idle_disconnect_time <= game_settings.idle_time)
        game_settings.idle_disconnect_time = game_settings.idle_time + 5;

    log_string("Game settings loaded successfully.");
    return 0;
}

int json_game_settings_write(void)
{
    json_t *root;

    log_string("Saving game settings to JSON...");

    root = game_settings_to_json();
    if (!root) {
        log_string("Error: Failed to serialize game settings");
        return 1;
    }

    if (!json_file_save(root, GAME_SETTINGS_JSON_FILE, "json_game_settings_write",
                        JSON_INDENT(2) | JSON_PRESERVE_ORDER))
        return 1;

    log_string("Game settings saved successfully.");
    return 0;
}

/***************************************************************************
 * Migration from .dat format                                             *
 ***************************************************************************/

int json_game_settings_migrate(void)
{
    extern int game_settings_read_dat(void);  // Old .dat reader from act_wiz.c
    int result;
    FILE *test_fp;
    char game_settings_file_buf[MAX_INPUT_LENGTH];
    char backup_file_buf[MAX_INPUT_LENGTH];
    const char *game_settings_file;
    const char *backup_file;

    log_string("Attempting to migrate game_settings.dat to JSON format...");

    game_settings_file = resolve_game_path(GAME_SETTINGS_FILE, game_settings_file_buf, sizeof(game_settings_file_buf));
    backup_file = resolve_game_path(GAME_SETTINGS_DAT_BACKUP, backup_file_buf, sizeof(backup_file_buf));

    // Check if the .dat file exists first
    test_fp = fopen(game_settings_file, "r");
    if (!test_fp) {
        log_string("Error: No game_settings.dat file found to migrate");
        return 1;
    }
    fclose(test_fp);

    // Load using old format
    result = game_settings_read_dat();
    if (result != 0) {
        log_string("Error: Failed to load game_settings.dat");
        return 1;
    }

    // Backup the old file
    char backup_cmd[512];
    snprintf(backup_cmd, sizeof(backup_cmd), "cp %s %s",
             game_settings_file, backup_file);
    system(backup_cmd);
    log_stringf("  Backed up old file to %s", backup_file);

    // Write as JSON
    result = json_game_settings_write();
    if (result != 0) {
        log_string("Error: Failed to write JSON format");
        return 1;
    }

    log_string("Migration completed successfully!");
    return 0;
}

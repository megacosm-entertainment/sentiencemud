/***************************************************************************
 *  JSON Game Settings - Implementation                                    *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <jansson.h>
#include "merc.h"
#include "tables.h"
#include "json_game_settings.h"

#define GAME_SETTINGS_JSON_FILE DATA_DIR "system/game_settings.json"
#define GAME_SETTINGS_DAT_BACKUP DATA_DIR "system/game_settings.dat.backup"

// Environment variable prefix
#define ENV_PREFIX "SENTIENCE_"

/***************************************************************************
 * Utility Functions                                                       *
 ***************************************************************************/

void json_get_game_settings_path(char *path_buf, size_t buf_size)
{
    snprintf(path_buf, buf_size, "%s", GAME_SETTINGS_JSON_FILE);
}

bool json_is_game_settings_json(void)
{
    FILE *fp = fopen(GAME_SETTINGS_JSON_FILE, "r");
    if (!fp) {
        return false;
    }

    char first_char = fgetc(fp);
    fclose(fp);

    return (first_char == '{');
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
    snprintf(env_name, buf_size, "%s", ENV_PREFIX);
    j = strlen(ENV_PREFIX);

    // Convert to uppercase
    for (i = 0; setting_name[i] && j < buf_size - 1; i++, j++) {
        env_name[j] = toupper(setting_name[i]);
    }
    env_name[j] = '\0';
}

const char *json_get_env_for_setting(const char *setting_name)
{
    char env_name[256];
    setting_to_env_name(setting_name, env_name, sizeof(env_name));
    return getenv(env_name);
}

bool json_setting_is_env_override(const char *setting_name)
{
    const char *env_value = json_get_env_for_setting(setting_name);
    return (env_value != NULL && env_value[0] != '\0');
}

void json_apply_env_overrides(void)
{
    const struct game_setting_type *setting;
    const char *env_value;
    char env_name[256];
    int i;

    log_string("Checking for environment variable overrides...");

    for (i = 0; game_settings_table[i].name != NULL; i++) {
        setting = &game_settings_table[i];
        env_value = json_get_env_for_setting(setting->name);

        if (!env_value) {
            continue;  // No override for this setting
        }

        setting_to_env_name(setting->name, env_name, sizeof(env_name));

        // Apply the override based on type
        switch (setting->type) {
            case SETTING_TYPE_BOOL: {
                bool *ptr = (bool *)setting->ptr;
                if (strcasecmp(env_value, "true") == 0 ||
                    strcasecmp(env_value, "yes") == 0 ||
                    strcmp(env_value, "1") == 0) {
                    *ptr = true;
                    log_stringf("  Override: %s = true", env_name);
                } else if (strcasecmp(env_value, "false") == 0 ||
                           strcasecmp(env_value, "no") == 0 ||
                           strcmp(env_value, "0") == 0) {
                    *ptr = false;
                    log_stringf("  Override: %s = false", env_name);
                }
                break;
            }

            case SETTING_TYPE_INT: {
                int *ptr = (int *)setting->ptr;
                *ptr = atoi(env_value);
                log_stringf("  Override: %s = %d", env_name, *ptr);
                break;
            }

            case SETTING_TYPE_STRING:
            case SETTING_TYPE_EXTSTR: {
                char **ptr = (char **)setting->ptr;

                // Don't log sensitive settings
                if (setting->sensitive) {
                    log_stringf("  Override: %s = <redacted>", env_name);
                } else {
                    log_stringf("  Override: %s = %s", env_name, env_value);
                }

                // Free old value if it exists
                if (*ptr && *ptr != str_empty) {
                    free_string(*ptr);
                }
                *ptr = str_dup(env_value);
                break;
            }

            case SETTING_TYPE_FLOAT: {
                float *ptr = (float *)setting->ptr;
                *ptr = atof(env_value);
                log_stringf("  Override: %s = %.2f", env_name, *ptr);
                break;
            }
        }
    }
}

/***************************************************************************
 * JSON Serialization                                                      *
 ***************************************************************************/

json_t *game_settings_to_json(void)
{
    json_t *root = json_object();
    json_t *categories[SETTING_CAT_MAX];  // One for each category
    const char *category_names[] = {
        "email",
        "missions",
        "lockers",
        "vaults",
        "coffers",
        "global",
        "security",
        "mssp",
        "redis",
        "debug"
    };
    int i;

    // Create category objects
    for (i = 0; i < SETTING_CAT_MAX; i++) {
        categories[i] = json_object();
    }

    // Add metadata
    json_object_set_new(root, "_version", json_string("1.0"));
    json_object_set_new(root, "_format", json_string("game_settings"));

    // Iterate through all settings and add to appropriate category
    for (i = 0; game_settings_table[i].name != NULL; i++) {
        const struct game_setting_type *setting = &game_settings_table[i];
        json_t *category = categories[setting->category];
        json_t *value = NULL;

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

    // Add all categories to root
    for (i = 0; i < SETTING_CAT_MAX; i++) {
        json_object_set_new(root, category_names[i], categories[i]);
    }

    return root;
}

bool json_to_game_settings(json_t *root)
{
    const char *category_names[] = {
        "email",
        "missions",
        "lockers",
        "vaults",
        "coffers",
        "global",
        "security",
        "mssp",
        "redis",
        "debug"
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

    return true;
}

/***************************************************************************
 * Core Load/Save Functions                                               *
 ***************************************************************************/

// Initialize defaults (extracted from game_settings_read)
static void init_game_settings_defaults(void)
{
    /* Basic settings */
    game_settings.game_name = "";
    game_settings.login_string = "";
    game_settings.server_description = "";
    game_settings.testport = false;
    game_settings.dev_server = false;
    game_settings.wizlock = false;
    game_settings.new_acct_lock = false;
    game_settings.new_char_lock = false;
    game_settings.wizlock_msg = "";
    game_settings.new_acct_lock_msg = "";
    game_settings.new_char_lock_msg = "";
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

    /* Misc Maximums */
    game_settings.max_alias = 0;
    game_settings.max_characters = 0;
    game_settings.max_orgs = 0;
    game_settings.max_logfile_size = 0;
    game_settings.org_disable_pk_pneuma_cost = 0;

    /* Email */
    game_settings.enable_email = false;
    game_settings.email_port = 0;
    game_settings.email_username = "";
    game_settings.email_host = "";
    game_settings.email_password = "";
    game_settings.email_from_addr = "";
    game_settings.email_from_name = "";

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
    game_settings.coffer_rent_currency = "";
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
    game_settings.ssl_cert_path = "";
    game_settings.ssl_key_path = "";
    game_settings.enable_insecure_warning = false;
    game_settings.insecure_warning_msg = "";

    /* MSSP */
    game_settings.mssp_players = 0;
    game_settings.mssp_uptime = 0;
    game_settings.mssp_crawl_delay = 0;
    game_settings.mssp_hostname = "";
    game_settings.mssp_port = 0;
    game_settings.mssp_tls_port = 0;
    game_settings.mssp_codebase = "";
    game_settings.mssp_contact = "";
    game_settings.mssp_created = 0;
    game_settings.mssp_ip = "";
    game_settings.mssp_language = "";
    game_settings.mssp_location = "";
    game_settings.mssp_minimum_age = 0;
    game_settings.mssp_website = "";
    game_settings.mssp_family = "";
    game_settings.mssp_genre = "";
    game_settings.mssp_status = "";
    game_settings.mssp_gamesystem = "";
    game_settings.mssp_intermud = "";
    game_settings.mssp_subgenre = "";
    game_settings.mssp_discord_server = "";
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
    game_settings.mssp_equipment_system = "";
    game_settings.mssp_multiplaying = "";
    game_settings.mssp_playerkilling = false;
    game_settings.mssp_quest_system = false;
    game_settings.mssp_roleplaying = false;
    game_settings.mssp_training_system = false;
    game_settings.mssp_world_originality = false;

    /* Redis Settings */
    game_settings.enable_redis = true;  /* Default to enabled */
    game_settings.redis_host = "127.0.0.1";
    game_settings.redis_port = 6379;
    game_settings.redis_password = "";
    game_settings.redis_timeout_sec = 1;
    game_settings.redis_timeout_usec = 500000;

    /* Debug/Logging Settings */
    game_settings.crash_dump_dir = "";
}

int json_game_settings_read(void)
{
    json_t *root;
    json_error_t error;

    log_string("Loading game settings from JSON...");

    // Initialize defaults first
    init_game_settings_defaults();

    // Try to load JSON file
    root = json_load_file(GAME_SETTINGS_JSON_FILE, 0, &error);
    if (!root) {
        log_stringf("Warning: Could not load %s: %s",
                       GAME_SETTINGS_JSON_FILE, error.text);
        log_string("  Will try to migrate from .dat format...");

        // Try migration
        if (json_game_settings_migrate() == 0) {
            log_string("  Migration successful, retrying load...");
            root = json_load_file(GAME_SETTINGS_JSON_FILE, 0, &error);
            if (!root) {
                log_stringf("Error: Could not load migrated settings: %s", error.text);
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
    int result;

    log_string("Saving game settings to JSON...");

    root = game_settings_to_json();
    if (!root) {
        log_string("Error: Failed to serialize game settings");
        return 1;
    }

    // Write with pretty printing for human readability
    result = json_dump_file(root, GAME_SETTINGS_JSON_FILE, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(root);

    if (result != 0) {
        log_stringf("Error: Failed to write %s", GAME_SETTINGS_JSON_FILE);
        return 1;
    }

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

    log_string("Attempting to migrate game_settings.dat to JSON format...");

    // Check if the .dat file exists first
    test_fp = fopen(GAME_SETTINGS_FILE, "r");
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
             GAME_SETTINGS_FILE, GAME_SETTINGS_DAT_BACKUP);
    system(backup_cmd);
    log_stringf("  Backed up old file to %s", GAME_SETTINGS_DAT_BACKUP);

    // Write as JSON
    result = json_game_settings_write();
    if (result != 0) {
        log_string("Error: Failed to write JSON format");
        return 1;
    }

    log_string("Migration completed successfully!");
    return 0;
}

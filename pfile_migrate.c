/***************************************************************************
 *  Pfile Migration Utility - Convert old pfiles to JSON format            *
 *                                                                          *
 *  This utility migrates player and account files from old pfile format   *
 *  to JSON. Password upgrades happen automatically on login via the       *
 *  existing verify_password() system. This only handles immediate         *
 *  upgrade of plaintext passwords (security fix).                         *
 *                                                                          *
 *  Usage:                                                                  *
 *    migrate_all_players()  - Migrate all player files to JSON            *
 *    migrate_all_accounts() - Migrate all account files to JSON           *
 *    migrate_player(name)   - Migrate specific player to JSON             *
 *    migrate_account(name)  - Migrate specific account to JSON            *
 ***************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "merc.h"
#include "recycle.h"
#include "io/json/json_char.h"
#include "io/json/json_account.h"
#include "account/auth_sodium.h"
#include "log.h"

/***************************************************************************
 * Migration Statistics                                                     *
 ***************************************************************************/

typedef struct migration_stats {
    int total_files;
    int already_json;
    int migrated_success;
    int migrated_failed;
    int skipped;
    
    /* Password stats (accounts only) */
    int pwd_plaintext_upgraded;  /* Plaintext → Argon2id (immediate) */
    int pwd_will_auto_upgrade;   /* Legacy hash → will upgrade on login */
} MIGRATION_STATS;

static void init_migration_stats(MIGRATION_STATS *stats)
{
    memset(stats, 0, sizeof(MIGRATION_STATS));
}

static void print_migration_stats(MIGRATION_STATS *stats, const char *type)
{
    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "=== %s Migration Summary ===", type);
    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "Total files found:     %d", stats->total_files);
    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "Already JSON:          %d", stats->already_json);
    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "Successfully migrated: %d", stats->migrated_success);
    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "Failed to migrate:     %d", stats->migrated_failed);
    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "Skipped:               %d", stats->skipped);
    
    if (strcmp(type, "Account") == 0) {
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "\n=== Password Security Notes ===");
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Plaintext upgraded:    %d (immediate security fix)",
            stats->pwd_plaintext_upgraded);
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Legacy hashes:         %d (will auto-upgrade on next login)",
            stats->pwd_will_auto_upgrade);
        if (stats->pwd_will_auto_upgrade > 0) {
            log_message_f(LOG_LEVEL_INFO, LOG_INFO,
                "\nNote: Use 'pwmigrate' command to audit password hash versions.");
        }
    }
}

/***************************************************************************
 * Password Helper - Only for Plaintext Passwords                          *
 ***************************************************************************/

/**
 * check_and_upgrade_plaintext_password - Upgrade plaintext passwords
 *
 * Only upgrades passwords if they are stored as plaintext (PWD_VER_PLAINTEXT).
 * This is an immediate security fix. All other legacy hashes (crypt, SHA256)
 * will be automatically upgraded on next successful login via verify_password().
 *
 * @param account   Account to check
 * @param stats     Statistics to update
 * @return          true if handled (upgraded or not plaintext)
 */
static bool check_and_upgrade_plaintext_password(ACCOUNT_DATA *account, 
                                                  MIGRATION_STATS *stats)
{
    int pwd_version;
    char *new_hash;
    
    if (!account || IS_NULLSTR(account->passwd)) {
        return true; /* No password to upgrade */
    }
    
    pwd_version = detect_password_version(account->passwd);
    
    /* Only handle plaintext - everything else auto-upgrades on login */
    if (pwd_version != PWD_VER_PLAINTEXT) {
        if (pwd_version != PWD_VER_ARGON2ID) {
            stats->pwd_will_auto_upgrade++;
        }
        return true;
    }
    
    /* Plaintext password found - upgrade immediately */
    log_message_f(LOG_LEVEL_WARN, LOG_WARN,
        "SECURITY: Account %s has plaintext password, upgrading to Argon2id",
        account->username);
    
    new_hash = hash_password_v3(account->passwd, PWD_SECURITY_INTERACTIVE);
    if (!new_hash) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "Failed to hash plaintext password for %s", account->username);
        return false;
    }
    
    /* Update account with hashed password */
    free_string(account->passwd);
    account->passwd = str_dup(new_hash);
    free_string(new_hash);
    account->passwd_version = PWD_VER_ARGON2ID;
    
    stats->pwd_plaintext_upgraded++;
    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "Successfully upgraded plaintext password for %s", account->username);
    
    return true;
}

/***************************************************************************
 * Backup Functions                                                         *
 ***************************************************************************/

static bool backup_old_pfile(const char *name, bool is_account)
{
    char old_path[2048];
    char backup_path[4096];
    char backup_dir[2048];
    FILE *src, *dst;
    char buffer[8192];
    size_t bytes;
    bool success = true;

    /* Build paths */
    if (is_account) {
        snprintf(old_path, sizeof(old_path), "%s%c/%s",
                 ACCOUNT_DIR, tolower(name[0]), name);
        snprintf(backup_dir, sizeof(backup_dir), "%s%c.old",
                 ACCOUNT_DIR, tolower(name[0]));
        snprintf(backup_path, sizeof(backup_path), "%s/%s",
                 backup_dir, name);
    } else {
        snprintf(old_path, sizeof(old_path), "%s%c/%s",
                 PLAYER_DIR, tolower(name[0]), name);
        snprintf(backup_dir, sizeof(backup_dir), "%s%c.old",
                 PLAYER_DIR, tolower(name[0]));
        snprintf(backup_path, sizeof(backup_path), "%s/%s",
                 backup_dir, name);
    }

    /* Create backup directory */
    mkdir(backup_dir, 0755);

    /* Copy file */
    src = fopen(old_path, "rb");
    if (!src) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "backup_old_pfile: Failed to open source %s", old_path);
        return false;
    }

    dst = fopen(backup_path, "wb");
    if (!dst) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "backup_old_pfile: Failed to create backup %s", backup_path);
        fclose(src);
        return false;
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
                "backup_old_pfile: Write error for %s", backup_path);
            success = false;
            break;
        }
    }

    fclose(src);
    fclose(dst);

    if (success) {
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Backed up %s to %s", name, backup_path);
    }

    return success;
}

/**
 * pfile_is_complete - Check if an old-format pfile is structurally complete
 *
 * Validates that the file is not truncated by checking that the last
 * section (either #PLAYER or #O) has a proper "End" marker. Truncated
 * files would cause fread_word/fread_number to hit EOF and produce
 * garbage data or crash.
 *
 * @param file_path  Full path to the pfile
 * @return           true if file appears complete, false if truncated
 */
static bool pfile_is_complete(const char *file_path)
{
    FILE *fp = fopen(file_path, "r");
    if (!fp)
        return false;

    /* Read the last meaningful line of the file */
    char last_line[256] = {0};
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        /* Strip trailing whitespace */
        int len = strlen(line);
        while (len > 0 && isspace((unsigned char)line[len - 1]))
            line[--len] = '\0';

        /* Track last non-empty line */
        if (len > 0)
            strncpy(last_line, line, sizeof(last_line) - 1);
    }
    fclose(fp);

    /* A complete old-format pfile ends with "End" (last object/player section)
     * or "#END" (rare, some formats use this as file terminator) */
    if (!str_cmp(last_line, "End") || !str_cmp(last_line, "#END"))
        return true;

    log_message_f(LOG_LEVEL_WARN, LOG_WARN,
        "pfile_is_complete: %s last line is '%s' (expected 'End')",
        file_path, last_line);

    return false;
}

/***************************************************************************
 * Player Migration Functions                                               *
 ***************************************************************************/

/**
 * migrate_player - Migrate a single player file to JSON format
 *
 * @param name      Player name to migrate
 * @param backup    Whether to backup the old pfile before migration
 * @return          true if migration successful, false otherwise
 */
bool migrate_player(char *name, bool backup)
{
    DESCRIPTOR_DATA d;
    char file_path[2048];
    bool loaded;
    bool success = false;

    if (!name || name[0] == '\0') {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "migrate_player: Invalid name");
        return false;
    }

    snprintf(file_path, sizeof(file_path), "%s%c/%s",
             PLAYER_DIR, tolower(name[0]), name);

    if (access(file_path, F_OK) != 0) {
        log_message_f(LOG_LEVEL_WARN, LOG_WARN,
            "migrate_player: File not found: %s", name);
        return false;
    }

    if (json_is_json_file(file_path)) {
        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG,
            "migrate_player: %s already JSON format", name);
        return true;
    }

    /* Validate that old-format pfile is not truncated */
    if (!pfile_is_complete(file_path)) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "migrate_player: %s appears truncated/corrupt - skipping", name);
        return false;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "Migrating player: %s", name);

    memset(&d, 0, sizeof(DESCRIPTOR_DATA));
    d.connected = CON_PLAYING;
    d.account = NULL;
    d.character = NULL;

    loaded = load_char_obj(&d, name);
    if (!loaded || !d.character) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "migrate_player: Failed to load %s", name);
        if (d.character) {
            free_char(d.character);
        }
        return false;
    }

    if (backup) {
        backup_old_pfile(name, false);
    }

    save_char_obj(d.character);

    if (json_is_json_file(file_path)) {
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Successfully migrated %s to JSON", name);
        success = true;
    } else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "JSON verification failed for %s", name);
        success = false;
    }

    free_char(d.character);
    return success;
}

/**
 * migrate_all_players - Migrate all player files to JSON format
 *
 * @param backup    Whether to backup old pfiles before migration
 * @return          Number of files successfully migrated
 */
int migrate_all_players(bool backup)
{
    MIGRATION_STATS stats;
    char dir_path[2048];
    char file_path[4096];
    DIR *dir;
    struct dirent *entry;
    int letter;

    init_migration_stats(&stats);

    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "=== Starting Player Migration (backup=%s) ===",
        backup ? "yes" : "no");

    for (letter = 'a'; letter <= 'z'; letter++) {
        snprintf(dir_path, sizeof(dir_path), "%s%c", PLAYER_DIR, letter);

        dir = opendir(dir_path);
        if (!dir) {
            continue;
        }

        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Scanning directory: %s", dir_path);

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') {
                continue;
            }

            snprintf(file_path, sizeof(file_path), "%s/%s",
                     dir_path, entry->d_name);

            struct stat st;
            if (stat(file_path, &st) != 0 || S_ISDIR(st.st_mode)) {
                continue;
            }

            if (strstr(entry->d_name, ".gz") != NULL) {
                stats.skipped++;
                continue;
            }

            stats.total_files++;

            if (json_is_json_file(file_path)) {
                stats.already_json++;
                continue;
            }

            if (migrate_player(entry->d_name, backup)) {
                stats.migrated_success++;
            } else {
                stats.migrated_failed++;
            }
        }

        closedir(dir);
    }

    print_migration_stats(&stats, "Player");
    return stats.migrated_success;
}

/***************************************************************************
 * Account Migration Functions                                              *
 ***************************************************************************/

/**
 * migrate_account - Migrate account file to JSON + fix plaintext passwords
 *
 * Migrates account to JSON format. Only upgrades plaintext passwords
 * immediately (security fix). Legacy hashes (crypt/SHA256) will auto-upgrade
 * on next successful login via the existing verify_password() system.
 *
 * @param name      Account name to migrate
 * @param backup    Whether to backup the old file before migration
 * @param stats     Statistics to update
 * @return          true if migration successful, false otherwise
 */
bool migrate_account(char *name, bool backup, void *vstats)
{
    DESCRIPTOR_DATA d;
    char file_path[2048];
    bool loaded;
    bool success = false;
    MIGRATION_STATS local_stats;
    MIGRATION_STATS *stats = (MIGRATION_STATS *)vstats;

    if (!stats) {
        init_migration_stats(&local_stats);
        stats = &local_stats;
    }

    if (!name || name[0] == '\0') {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "migrate_account: Invalid name");
        return false;
    }

    snprintf(file_path, sizeof(file_path), "%s%c/%s",
             ACCOUNT_DIR, tolower(name[0]), name);

    if (access(file_path, F_OK) != 0) {
        log_message_f(LOG_LEVEL_WARN, LOG_WARN,
            "migrate_account: File not found: %s", name);
        return false;
    }

    if (json_is_account_json(file_path)) {
        log_message_f(LOG_LEVEL_DEBUG, LOG_DEBUG,
            "migrate_account: %s already JSON format", name);
        
        /* Still check for plaintext passwords even in JSON files */
        memset(&d, 0, sizeof(DESCRIPTOR_DATA));
        if (load_account(&d, name) && d.account) {
            if (check_and_upgrade_plaintext_password(d.account, stats)) {
                if (d.account->passwd_version == PWD_VER_ARGON2ID) {
                    save_account(d.account);
                }
            }
            /* Only free if we loaded a fresh copy from disk.
             * load_account() returns cached accounts from loaded_accounts
             * with an incremented refcount — freeing those would invalidate
             * pointers held by active descriptors (use-after-free crash). */
            if (d.account->refcount > 1) {
                d.account->refcount--;
            } else {
                free_account(d.account);
            }
        }
        
        return true;
    }

    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "Migrating account: %s", name);

    /* Validate that old-format account file is not truncated */
    if (!pfile_is_complete(file_path)) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "migrate_account: %s appears truncated/corrupt - skipping", name);
        return false;
    }

    memset(&d, 0, sizeof(DESCRIPTOR_DATA));
    d.connected = CON_PLAYING;
    d.account = NULL;
    d.character = NULL;

    loaded = load_account(&d, name);
    if (!loaded || !d.account) {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "migrate_account: Failed to load %s", name);
        if (d.account) {
            free_account(d.account);
        }
        return false;
    }

    if (backup) {
        backup_old_pfile(name, true);
    }

    /* Check and upgrade plaintext passwords */
    check_and_upgrade_plaintext_password(d.account, stats);

    /* Save in JSON format */
    save_account(d.account);

    if (json_is_account_json(file_path)) {
        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Successfully migrated %s to JSON", name);
        success = true;
    } else {
        log_message_f(LOG_LEVEL_ERROR, LOG_ERROR,
            "JSON verification failed for %s", name);
        success = false;
    }

    /* Only free if we loaded a fresh copy from disk.
     * load_account() returns cached accounts with incremented refcount;
     * freeing those would invalidate pointers held by active descriptors. */
    if (d.account->refcount > 1) {
        d.account->refcount--;
    } else {
        free_account(d.account);
    }
    return success;
}

/**
 * migrate_all_accounts - Migrate all account files to JSON format
 *
 * @param backup    Whether to backup old files before migration
 * @return          Number of files successfully migrated
 */
int migrate_all_accounts(bool backup)
{
    MIGRATION_STATS stats;
    char dir_path[2048];
    char file_path[4096];
    DIR *dir;
    struct dirent *entry;
    int letter;

    init_migration_stats(&stats);

    log_message_f(LOG_LEVEL_INFO, LOG_INFO,
        "=== Starting Account Migration (backup=%s) ===",
        backup ? "yes" : "no");

    for (letter = 'a'; letter <= 'z'; letter++) {
        snprintf(dir_path, sizeof(dir_path), "%s%c", ACCOUNT_DIR, letter);

        dir = opendir(dir_path);
        if (!dir) {
            continue;
        }

        log_message_f(LOG_LEVEL_INFO, LOG_INFO,
            "Scanning directory: %s", dir_path);

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') {
                continue;
            }

            snprintf(file_path, sizeof(file_path), "%s/%s",
                     dir_path, entry->d_name);

            struct stat st;
            if (stat(file_path, &st) != 0 || S_ISDIR(st.st_mode)) {
                continue;
            }

            if (strstr(entry->d_name, ".gz") != NULL) {
                stats.skipped++;
                continue;
            }

            stats.total_files++;

            if (json_is_account_json(file_path)) {
                stats.already_json++;
                
                /* Still check for plaintext passwords */
                migrate_account(entry->d_name, false, &stats);
                continue;
            }

            if (migrate_account(entry->d_name, backup, &stats)) {
                stats.migrated_success++;
            } else {
                stats.migrated_failed++;
            }
        }

        closedir(dir);
    }

    print_migration_stats(&stats, "Account");
    return stats.migrated_success;
}

/***************************************************************************
 * Command Interface                                                        *
 ***************************************************************************/

/**
 * do_migrate - Immortal command to trigger pfile/account migration
 *
 * Syntax:
 *   migrate players [backup]   - Migrate all player files
 *   migrate accounts [backup]  - Migrate all accounts
 *   migrate player <name>      - Migrate specific player
 *   migrate account <name>     - Migrate specific account
 *   migrate all [backup]       - Migrate both players and accounts
 */
void do_migrate(CHAR_DATA *ch, char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    bool backup = false;
    int player_count, account_count;

    if (IS_NPC(ch)) {
        return;
    }

    if (!IS_IMPLEMENTOR(ch)) {
        send_to_char("Only implementors can use this command.\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (arg1[0] == '\0') {
        send_to_char("Syntax:\n\r", ch);
        send_to_char("  migrate players [backup]   - Migrate all player files to JSON\n\r", ch);
        send_to_char("  migrate accounts [backup]  - Migrate all accounts to JSON\n\r", ch);
        send_to_char("  migrate player <name>      - Migrate specific player\n\r", ch);
        send_to_char("  migrate account <name>     - Migrate specific account\n\r", ch);
        send_to_char("  migrate all [backup]       - Migrate everything\n\r", ch);
        send_to_char("\n\r", ch);
        send_to_char("The 'backup' flag creates .old copies before migration.\n\r", ch);
        send_to_char("\n\r", ch);
        send_to_char("{YPassword Migration:{x\n\r", ch);
        send_to_char("- Plaintext passwords are upgraded immediately to Argon2id\n\r", ch);
        send_to_char("- Legacy hashes (crypt/SHA256) auto-upgrade on next login\n\r", ch);
        send_to_char("- Use 'pwmigrate' command to audit password hash versions\n\r", ch);
        return;
    }

    if (!str_cmp(arg2, "backup")) {
        backup = true;
        send_to_char("Backup mode enabled.\n\r", ch);
    }

    if (!str_cmp(arg1, "players")) {
        char buf[MAX_STRING_LENGTH];
        send_to_char("Starting player migration...\n\r", ch);
        player_count = migrate_all_players(backup);
        sprintf(buf, "Migration complete: %d players migrated.\n\r", player_count);
        send_to_char(buf, ch);
    }
    else if (!str_cmp(arg1, "accounts")) {
        char buf[MAX_STRING_LENGTH];
        send_to_char("Starting account migration...\n\r", ch);
        account_count = migrate_all_accounts(backup);
        sprintf(buf, "Migration complete: %d accounts migrated.\n\r", account_count);
        send_to_char(buf, ch);
        send_to_char("Check logs for password security notes.\n\r", ch);
    }
    else if (!str_cmp(arg1, "player")) {
        char buf[MAX_STRING_LENGTH];
        if (arg2[0] == '\0' || !str_cmp(arg2, "backup")) {
            send_to_char("Specify a player name to migrate.\n\r", ch);
            return;
        }
        sprintf(buf, "Migrating player: %s\n\r", arg2);
        send_to_char(buf, ch);
        if (migrate_player(arg2, true)) {
            send_to_char("Migration successful.\n\r", ch);
        } else {
            send_to_char("Migration failed. Check logs for details.\n\r", ch);
        }
    }
    else if (!str_cmp(arg1, "account")) {
        char buf[MAX_STRING_LENGTH];
        if (arg2[0] == '\0' || !str_cmp(arg2, "backup")) {
            send_to_char("Specify an account name to migrate.\n\r", ch);
            return;
        }
        sprintf(buf, "Migrating account: %s\n\r", arg2);
        send_to_char(buf, ch);
        if (migrate_account(arg2, true, NULL)) {
            send_to_char("Migration successful.\n\r", ch);
        } else {
            send_to_char("Migration failed. Check logs for details.\n\r", ch);
        }
    }
    else if (!str_cmp(arg1, "all")) {
        char buf[MAX_STRING_LENGTH];
        send_to_char("Starting full migration (players + accounts)...\n\r", ch);
        player_count = migrate_all_players(backup);
        account_count = migrate_all_accounts(backup);
        sprintf(buf, "Migration complete: %d players, %d accounts.\n\r",
                player_count, account_count);
        send_to_char(buf, ch);
        send_to_char("Check logs for details. Use 'pwmigrate' to audit password hashes.\n\r", ch);
    }
    else {
        send_to_char("Invalid option. See 'migrate' for syntax.\n\r", ch);
    }
}

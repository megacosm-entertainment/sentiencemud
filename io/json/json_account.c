/***************************************************************************
 *  JSON Account Format - Complete Account Serialization                   *
 *                                                                          *
 *  This file contains comprehensive account serialization including:      *
 *  - Account authentication (password, MFA, email, recovery codes)        *
 *  - Character references with metadata                                   *
 *  - Vault items (shared storage across characters)                       *
 *  - Staff notes                                                          *
 *  - Human-readable flag names for account flags                          *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <jansson.h>
#include "../../merc.h"
#include "../../tables.h"
#include "../../account/penalty.h"
#include "../../account/preferences.h"
#include "../../account/unlock.h"
#include "json_common.h"
#include "json_account.h"
#include "json_char.h"  // For obj_to_json() reuse

extern LOCALIZATION_DATA *default_localization;

/***************************************************************************
 * External Flag Tables                                                    *
 ***************************************************************************/

extern const struct flag_type acct_flags[];

/* Flag helpers now provided by json_common.h */

/***************************************************************************
 * Utility Functions                                                       *
 ***************************************************************************/

void json_get_account_path(const char *username, char *path_buf, size_t buf_size)
{
    char account_dir_buf[256];
    const char *account_dir = resolve_game_path(ACCOUNT_DIR, account_dir_buf, sizeof(account_dir_buf));

    // No .json extension - same path as old pfile
    // Format is auto-detected on load
    snprintf(path_buf, buf_size, "%s%c/%s",
             account_dir, tolower(username[0]), username);
}

void json_get_account_backup_path(const char *username, char *path_buf, size_t buf_size)
{
    char account_dir_buf[256];
    const char *account_dir = resolve_game_path(ACCOUNT_DIR, account_dir_buf, sizeof(account_dir_buf));

    // Backup old pfile before migration
    snprintf(path_buf, buf_size, "%s%c.old/%s",
             account_dir, tolower(username[0]), username);
}

bool json_ensure_account_dir(const char *username)
{
    return json_ensure_dir(ACCOUNT_DIR, username);
}

bool json_is_account_json(const char *filename)
{
    return json_file_is_json(filename);
}

/***************************************************************************
 * Account Metadata Serialization                                          *
 ***************************************************************************/

static json_t *account_metadata_to_json(ACCOUNT_DATA *account)
{
    json_t *meta;

    meta = json_object();

    // Format version
    json_object_set_new(meta, "format_version", json_integer(2));

    // Account data version (for migration tracking)
    json_object_set_new(meta, "version", json_integer(VERSION_ACCOUNT));

    // Account ID (unique identifier)
    json_t *account_id = json_array();
    json_array_append_new(account_id, json_integer(account->id[0]));
    json_array_append_new(account_id, json_integer(account->id[1]));
    json_object_set_new(meta, "account_id", account_id);

    // Timestamps
    json_object_set_new(meta, "created", json_integer(account->creation_date));
    json_object_set_new(meta, "last_saved", json_integer(current_time));
    json_object_set_new(meta, "last_login", json_integer(account->last_login));

    return meta;
}

/***************************************************************************
 * Account Basic Data Serialization                                        *
 ***************************************************************************/

static json_t *account_basic_to_json(ACCOUNT_DATA *account)
{
    json_t *basic, *recovery_array;
    int i;

    basic = json_object();

    // Username
    json_object_set_new(basic, "username", json_string(account->username));

    // Authentication - Password
    if (account->passwd) {
        json_object_set_new(basic, "password", json_string(account->passwd));
        json_object_set_new(basic, "password_version", json_integer(account->passwd_version));
    }

    // Authentication - MFA
    json_object_set_new(basic, "mfa_enabled", json_boolean(account->mfa_enabled));
    if (account->mfa_key) {
        json_object_set_new(basic, "mfa_key", json_string(account->mfa_key));
    }

    // MFA Recovery Codes
    recovery_array = json_array();
    for (i = 0; i < MFA_RECOVERY_CODES; i++) {
        if (account->recovery_codes[i]) {
            json_t *recovery = json_object();
            json_object_set_new(recovery, "code", json_string(account->recovery_codes[i]));
            json_object_set_new(recovery, "used", json_boolean(account->recovery_used[i]));
            json_array_append_new(recovery_array, recovery);
        }
    }
    if (json_array_size(recovery_array) > 0) {
        json_object_set_new(basic, "recovery_codes", recovery_array);
    } else {
        json_decref(recovery_array);
    }

    // Email
    if (account->email) {
        json_object_set_new(basic, "email", json_string(account->email));
        json_object_set_new(basic, "email_verified", json_boolean(account->email_verified));
    }

    // Email verification code (if pending)
    if (account->email_verification_code) {
        json_object_set_new(basic, "email_verification_code", json_string(account->email_verification_code));
        json_object_set_new(basic, "email_verification_time", json_integer(account->email_verification_time));
    }
    if (account->pending_email) {
        json_object_set_new(basic, "pending_email", json_string(account->pending_email));
    }

    // Password reset code (if active)
    if (account->reset_code) {
        json_object_set_new(basic, "reset_code", json_string(account->reset_code));
        json_object_set_new(basic, "reset_time", json_integer(account->reset_time));
        json_object_set_new(basic, "reset_state", json_boolean(account->reset_state));
    }

    // Account flags (human-readable + numeric for backward compatibility)
    json_object_set_new(basic, "account_flags", json_flags_serialize(account->acct_flags, acct_flags));
    json_object_set_new(basic, "acct_flags_numeric", json_integer(account->acct_flags));

    // Account limits and status
    json_object_set_new(basic, "character_count", json_integer(account->character_count));
    json_object_set_new(basic, "character_limit", json_integer(account->character_limit));
    json_object_set_new(basic, "staff_account", json_boolean(account->staff_account));

    // Vault rent
    if (account->vault_rent > 0) {
        json_object_set_new(basic, "vault_rent", json_integer(account->vault_rent));
    }

    // Host info
    if (account->creation_host && account->creation_host[0] != '\0') {
        json_object_set_new(basic, "creation_host", json_string(account->creation_host));
    }
    if (account->last_login_host && account->last_login_host[0] != '\0') {
        json_object_set_new(basic, "last_login_host", json_string(account->last_login_host));
    }

    // Default character
    if (account->default_character && account->default_character[0] != '\0') {
        json_object_set_new(basic, "default_character", json_string(account->default_character));
    }

    // MFA pending enrollment
    if (account->mfa_pending_key && account->mfa_pending_key[0] != '\0') {
        json_object_set_new(basic, "mfa_pending_key", json_string(account->mfa_pending_key));
        json_object_set_new(basic, "mfa_pending", json_boolean(account->mfa_pending));
    }

    // Email verification rate-limiting timestamp
    if (account->email_verification_last_sent > 0) {
        json_object_set_new(basic, "email_verification_last_sent", json_integer(account->email_verification_last_sent));
    }

    // Language preference
    if (account->lang != NULL) {
        json_object_set_new(basic, "language", json_string(account->lang->iso_name));
    }

    return basic;
}

/***************************************************************************
 * Character References Serialization                                      *
 ***************************************************************************/

static json_t *characters_to_json(ACCOUNT_DATA *account)
{
    json_t *characters;
    ITERATOR it;
    ACCOUNT_CHARACTER *ac;

    characters = json_array();

    if (!account->characters) {
        return characters;
    }

    // Iterate through linked list of characters via iterator API
    iterator_start(&it, account->characters);
    while ((ac = (ACCOUNT_CHARACTER *)iterator_nextdata(&it))) {
        if (!ac) {
            continue;
        }

        json_t *char_obj = json_object();
        if (!char_obj) {
            continue;
        }

        // Skip entries with no valid name — these are phantom entries from
        // earlier bugs and should not be persisted
        if (IS_NULLSTR(ac->name) || !str_cmp(ac->name, "(unknown)")) {
            log_stringf("characters_to_json: skipping character with invalid name '%s'",
                       ac->name ? ac->name : "(null)");
            json_decref(char_obj);
            continue;
        }
        json_object_set_new(char_obj, "name", json_string(ac->name));
        json_object_set_new(char_obj, "current_level", json_integer(ac->current_level));
        json_object_set_new(char_obj, "tot_level", json_integer(ac->tot_level));

        if (!IS_NULLSTR(ac->race_name)) {
            json_object_set_new(char_obj, "race_name", json_string(ac->race_name));
        }
        if (!IS_NULLSTR(ac->class_name)) {
            json_object_set_new(char_obj, "class_name", json_string(ac->class_name));
        }
        if (!IS_NULLSTR(ac->last_area)) {
            json_object_set_new(char_obj, "last_area", json_string(ac->last_area));
        }

        // Character status
        json_object_set_new(char_obj, "staff", json_boolean(ac->staff));
        json_object_set_new(char_obj, "staff_rank", json_integer(ac->staff_rank));
        json_object_set_new(char_obj, "deleted", json_boolean(ac->deleted));
        json_object_set_new(char_obj, "delete_time", json_integer(ac->delete_time));
        json_object_set_new(char_obj, "creation_date", json_integer(ac->creation_date));
        json_object_set_new(char_obj, "last_login", json_integer(ac->last_login));
        json_object_set_new(char_obj, "last_logoff", json_integer(ac->last_logoff));

        // Character ID
        json_t *char_id = json_array();
        json_array_append_new(char_id, json_integer(ac->id[0]));
        json_array_append_new(char_id, json_integer(ac->id[1]));
        json_object_set_new(char_obj, "id", char_id);

        // Character-specific auth override (if any)
        if (ac->pwd) {
            json_object_set_new(char_obj, "password", json_string(ac->pwd));
            json_object_set_new(char_obj, "password_version", json_integer(ac->pwd_vers));
        }
        if (ac->mfa_key) {
            json_object_set_new(char_obj, "mfa_key", json_string(ac->mfa_key));
            json_object_set_new(char_obj, "mfa_enabled", json_boolean(ac->mfa_enabled));
        }

        // Last host
        if (ac->last_host && ac->last_host[0] != '\0') {
            json_object_set_new(char_obj, "last_host", json_string(ac->last_host));
        }

        // Old password (for migration)
        if (ac->old_pwd && ac->old_pwd[0] != '\0') {
            json_object_set_new(char_obj, "old_password", json_string(ac->old_pwd));
        }

        // Password reset
        if (ac->reset_code && ac->reset_code[0] != '\0') {
            json_object_set_new(char_obj, "reset_code", json_string(ac->reset_code));
            json_object_set_new(char_obj, "reset_time", json_integer(ac->reset_time));
            json_object_set_new(char_obj, "reset_state", json_integer(ac->reset_state));
        }

        // MFA pending enrollment (character-level)
        if (ac->mfa_pending_key && ac->mfa_pending_key[0] != '\0') {
            json_object_set_new(char_obj, "mfa_pending_key", json_string(ac->mfa_pending_key));
        }

        // MFA recovery codes (character-level)
        {
            int i;
            json_t *char_recovery = json_array();
            for (i = 0; i < MFA_RECOVERY_CODES; i++) {
                if (ac->recovery_codes[i]) {
                    json_t *rc = json_object();
                    json_object_set_new(rc, "code", json_string(ac->recovery_codes[i]));
                    json_object_set_new(rc, "used", json_boolean(ac->recovery_used[i]));
                    json_array_append_new(char_recovery, rc);
                }
            }
            if (json_array_size(char_recovery) > 0) {
                json_object_set_new(char_obj, "recovery_codes", char_recovery);
            } else {
                json_decref(char_recovery);
            }
        }

        // Email (character-level)
        if (ac->email && ac->email[0] != '\0') {
            json_object_set_new(char_obj, "email", json_string(ac->email));
            json_object_set_new(char_obj, "email_verified", json_boolean(ac->email_verified));
        }
        if (ac->pending_email && ac->pending_email[0] != '\0') {
            json_object_set_new(char_obj, "pending_email", json_string(ac->pending_email));
        }
        if (ac->email_verification_code && ac->email_verification_code[0] != '\0') {
            json_object_set_new(char_obj, "email_verification_code", json_string(ac->email_verification_code));
            json_object_set_new(char_obj, "email_verification_time", json_integer(ac->email_verification_time));
        }
        if (ac->email_verification_last_sent > 0) {
            json_object_set_new(char_obj, "email_verification_last_sent", json_integer(ac->email_verification_last_sent));
        }

        // Character-level staff notes
        if (ac->staff_notes) {
            json_t *char_notes = json_array();
            ACCOUNT_NOTE_DATA *cnote;
            for (cnote = ac->staff_notes; cnote; cnote = cnote->next) {
                json_t *cn_obj = json_object();
                json_object_set_new(cn_obj, "author", json_string(cnote->author));
                json_object_set_new(cn_obj, "timestamp", json_integer(cnote->timestamp));
                json_object_set_new(cn_obj, "subject", json_string(cnote->subject));
                json_object_set_new(cn_obj, "text", json_string(cnote->text));
                json_object_set_new(cn_obj, "category", json_integer(cnote->category));
                if (!IS_NULLSTR(cnote->penalty_ref))
                    json_object_set_new(cn_obj, "penalty_ref", json_string(cnote->penalty_ref));
                json_array_append_new(char_notes, cn_obj);
            }
            json_object_set_new(char_obj, "staff_notes", char_notes);
        }

        json_array_append_new(characters, char_obj);
    }
    iterator_stop(&it);

    return characters;
}

/***************************************************************************
 * Vault Items Serialization                                               *
 ***************************************************************************/

static json_t *vault_to_json(ACCOUNT_DATA *account)
{
    json_t *vault;
    OBJ_DATA *obj;

    vault = json_array();

    // Serialize vault items (reuse obj_to_json from json_char.c)
    for (obj = account->vault_items; obj; obj = obj->next_content) {
        json_t *json_obj = obj_to_json(obj, 0);
        if (json_obj) {
            json_array_append_new(vault, json_obj);
        }
    }

    return vault;
}

/***************************************************************************
 * Staff Notes Serialization                                               *
 ***************************************************************************/

static json_t *staff_notes_to_json(ACCOUNT_DATA *account)
{
    json_t *notes;
    ACCOUNT_NOTE_DATA *note;

    notes = json_array();

    if (!account->staff_notes) {
        return notes;
    }

    for (note = account->staff_notes; note; note = note->next) {
        json_t *note_obj = json_object();

        json_object_set_new(note_obj, "author", json_string(note->author));
        json_object_set_new(note_obj, "timestamp", json_integer(note->timestamp));
        json_object_set_new(note_obj, "subject", json_string(note->subject));
        json_object_set_new(note_obj, "text", json_string(note->text));
        json_object_set_new(note_obj, "category", json_integer(note->category));
        if (!IS_NULLSTR(note->penalty_ref))
            json_object_set_new(note_obj, "penalty_ref", json_string(note->penalty_ref));

        json_array_append_new(notes, note_obj);
    }

    return notes;
}

/***************************************************************************
 * Complete Account Serialization                                          *
 ***************************************************************************/

json_t *account_to_json(ACCOUNT_DATA *account)
{
    json_t *root, *characters, *vault, *staff_notes;

    if (!account) {
        return NULL;
    }

    root = json_object();

    // Metadata section
    json_object_set_new(root, "metadata", account_metadata_to_json(account));

    // Account basic info section
    json_object_set_new(root, "account", account_basic_to_json(account));

    // Characters section
    characters = characters_to_json(account);
    if (json_array_size(characters) > 0) {
        json_object_set_new(root, "characters", characters);
    } else {
        json_decref(characters);
    }

    // Vault section
    vault = vault_to_json(account);
    if (json_array_size(vault) > 0) {
        json_object_set_new(root, "vault", vault);
    } else {
        json_decref(vault);
    }

    // Staff notes section
    staff_notes = staff_notes_to_json(account);
    if (json_array_size(staff_notes) > 0) {
        json_object_set_new(root, "staff_notes", staff_notes);
    } else {
        json_decref(staff_notes);
    }

    // Penalties section
    if (account->penalties) {
        json_object_set_new(root, "penalties",
            penalties_to_json(account->penalties));
    }

    // Bonuses section
    if (account->bonuses) {
        json_object_set_new(root, "bonuses",
            bonuses_to_json(account->bonuses));
    }

    // Preferences section
    if (account->preferences) {
        json_object_set_new(root, "preferences",
            prefs_to_json(account->preferences));
    }

    // Unlocked races section
    json_t *unlocked = unlocked_races_to_json(account);
    if (unlocked) {
        json_object_set_new(root, "unlocked_races", unlocked);
    }

    return root;
}

/***************************************************************************
 * File I/O - Write                                                        *
 ***************************************************************************/

static bool backup_old_pfile(const char *filename, const char *username)
{
    char backup_path[512];
    char backup_dir[256];
    char account_dir_buf[256];
    const char *account_dir = resolve_game_path(ACCOUNT_DIR, account_dir_buf, sizeof(account_dir_buf));
    FILE *src, *dst;
    char buffer[4096];
    size_t bytes;

    // Check if file exists and is old format
    if (!json_is_account_json(filename)) {
        // It's an old pfile, back it up
        json_get_account_backup_path(username, backup_path, sizeof(backup_path));

        // Create backup directory
        snprintf(backup_dir, sizeof(backup_dir), "%s%c.old",
            account_dir, tolower(username[0]));
        mkdir(backup_dir, 0755);

        // Copy file to backup
        src = fopen(filename, "r");
        if (!src) {
            return false; // File doesn't exist, no backup needed
        }

        dst = fopen(backup_path, "w");
        if (!dst) {
            fclose(src);
            log_stringf("json_write_account: Failed to create backup %s", backup_path);
            return false;
        }

        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }

        fclose(src);
        fclose(dst);

        log_stringf("JSON: Backed up old account pfile %s → %s", filename, backup_path);
    }

    return true;
}

bool json_write_account(ACCOUNT_DATA *account, const char *filename)
{
    json_t *root;
    char tmp_filename[512];
    int result;

    if (!account) {
        return false;
    }

    // Ensure directory exists
    json_ensure_account_dir(account->username);

    // Backup old pfile before migrating to JSON
    backup_old_pfile(filename, account->username);

    // Serialize to JSON
    root = account_to_json(account);
    if (!root) {
        log_stringf("json_write_account: Failed to serialize %s", account->username);
        return false;
    }

    // Write to temporary file first (atomic write)
    snprintf(tmp_filename, sizeof(tmp_filename), "%s.tmp", filename);
    result = json_dump_file(root, tmp_filename, JSON_INDENT(2) | JSON_PRESERVE_ORDER);
    json_decref(root);

    if (result != 0) {
        log_stringf("json_write_account: Failed to write %s", tmp_filename);
        return false;
    }

    // Atomic rename
    if (rename(tmp_filename, filename) != 0) {
        log_stringf("json_write_account: Failed to rename %s to %s", tmp_filename, filename);
        unlink(tmp_filename);
        return false;
    }

    log_stringf("JSON: Saved account %s to %s", account->username, filename);
    return true;
}

/***************************************************************************
 * File I/O - Read                                                         *
 ***************************************************************************/

bool json_read_account(ACCOUNT_DATA *account, const char *filename)
{
    json_t *root, *metadata, *account_obj, *characters, *vault, *staff_notes;
    json_error_t error;
    json_t *value, *array_elem;
    const char *str;
    size_t index;
    int i;

    // Load JSON file
    root = json_load_file(filename, 0, &error);
    if (!root) {
        // JSON file doesn't exist or is corrupt - check if old pfile exists
        // Return false to allow fallback to old pfile loader
        return false;
    }

    // Read metadata section
    metadata = json_object_get(root, "metadata");
    if (metadata) {
        // Account ID
        json_t *account_id = json_object_get(metadata, "account_id");
        if (account_id && json_is_array(account_id)) {
            account->id[0] = json_integer_value(json_array_get(account_id, 0));
            account->id[1] = json_integer_value(json_array_get(account_id, 1));
        }

        // Account data version
        value = json_object_get(metadata, "version");
        if (value) {
            account->version = json_integer_value(value);
        } else {
            // Pre-versioning JSON file
            account->version = VERSION_ACCOUNT_000;
        }

        // Timestamps
        value = json_object_get(metadata, "created");
        if (value) account->creation_date = json_integer_value(value);
        value = json_object_get(metadata, "last_login");
        if (value) account->last_login = json_integer_value(value);
    }

    // Read account section
    account_obj = json_object_get(root, "account");
    if (!account_obj) {
        log_stringf("json_read_account: No account section in %s", filename);
        json_decref(root);
        return false;
    }

    // Username
    str = json_get_string(account_obj, "username", "");
    if (str) {
        free_string(account->username);
        account->username = str_dup(str);
    }

    // Password
    str = json_get_string(account_obj, "password", "");
    if (str) {
        free_string(account->passwd);
        account->passwd = str_dup(str);
    }
    value = json_object_get(account_obj, "password_version");
    if (value) account->passwd_version = json_integer_value(value);

    // MFA
    account->mfa_enabled = json_is_true(json_object_get(account_obj, "mfa_enabled"));
    str = json_get_string(account_obj, "mfa_key", "");
    if (str) {
        free_string(account->mfa_key);
        account->mfa_key = str_dup(str);
    }

    // Recovery codes
    json_t *recovery_array = json_object_get(account_obj, "recovery_codes");
    if (recovery_array && json_is_array(recovery_array)) {
        i = 0;
        json_array_foreach(recovery_array, index, array_elem) {
            if (i >= MFA_RECOVERY_CODES) break;

            str = json_get_string(array_elem, "code", "");
            if (str) {
                free_string(account->recovery_codes[i]);
                account->recovery_codes[i] = str_dup(str);
                account->recovery_used[i] = json_is_true(json_object_get(array_elem, "used"));
            }
            i++;
        }
    }

    // Email
    str = json_get_string(account_obj, "email", "");
    if (str) {
        free_string(account->email);
        account->email = str_dup(str);
    }
    account->email_verified = json_is_true(json_object_get(account_obj, "email_verified"));

    // Email verification code
    str = json_get_string(account_obj, "email_verification_code", "");
    if (str) {
        free_string(account->email_verification_code);
        account->email_verification_code = str_dup(str);
    }
    value = json_object_get(account_obj, "email_verification_time");
    if (value) account->email_verification_time = json_integer_value(value);

    str = json_get_string(account_obj, "pending_email", "");
    if (str) {
        free_string(account->pending_email);
        account->pending_email = str_dup(str);
    }

    // Password reset code
    str = json_get_string(account_obj, "reset_code", "");
    if (str) {
        free_string(account->reset_code);
        account->reset_code = str_dup(str);
    }
    value = json_object_get(account_obj, "reset_time");
    if (value) account->reset_time = json_integer_value(value);
    account->reset_state = json_is_true(json_object_get(account_obj, "reset_state"));

    // Account flags (prefer human-readable array, fall back to numeric)
    value = json_object_get(account_obj, "account_flags");
    if (value && json_is_array(value)) {
        account->acct_flags = json_flags_deserialize(value, acct_flags);
    } else {
        value = json_object_get(account_obj, "acct_flags_numeric");
        if (value) account->acct_flags = json_integer_value(value);
    }

    // Account limits and status
    value = json_object_get(account_obj, "character_count");
    if (value) account->character_count = json_integer_value(value);
    value = json_object_get(account_obj, "character_limit");
    if (value) account->character_limit = json_integer_value(value);
    account->staff_account = json_is_true(json_object_get(account_obj, "staff_account"));

    // Vault rent
    value = json_object_get(account_obj, "vault_rent");
    if (value) account->vault_rent = json_integer_value(value);

    // Host info
    str = json_get_string(account_obj, "creation_host", "");
    if (str) {
        free_string(account->creation_host);
        account->creation_host = str_dup(str);
    }
    str = json_get_string(account_obj, "last_login_host", "");
    if (str) {
        free_string(account->last_login_host);
        account->last_login_host = str_dup(str);
    }

    // Default character
    str = json_get_string(account_obj, "default_character", "");
    if (str) {
        free_string(account->default_character);
        account->default_character = str_dup(str);
    }

    // MFA pending enrollment
    str = json_get_string(account_obj, "mfa_pending_key", "");
    if (str) {
        free_string(account->mfa_pending_key);
        account->mfa_pending_key = str_dup(str);
    }
    account->mfa_pending = json_is_true(json_object_get(account_obj, "mfa_pending"));

    // Email verification rate-limiting timestamp
    value = json_object_get(account_obj, "email_verification_last_sent");
    if (value) account->email_verification_last_sent = json_integer_value(value);

    value = json_object_get(account_obj, "language");
    if (value) {
        account->lang = localization_lookup(json_string_value(value));
        if (!account->lang) account->lang = default_localization;
    }

    // Read characters section
    characters = json_object_get(root, "characters");
    if (characters && json_is_array(characters)) {
        // Initialize character list if not already done
        if (!account->characters) {
            account->characters = list_create(false);
        }

        json_array_foreach(characters, index, array_elem) {
            ACCOUNT_CHARACTER *ac = new_account_character();
            if (!ac) continue;

            // Basic character info
            str = json_get_string(array_elem, "name", "");
            if (str) { free_string(ac->name); ac->name = str_dup(str); }

            // Skip entries with no valid name — phantom entries from earlier bugs
            if (IS_NULLSTR(ac->name) || !str_cmp(ac->name, "(unknown)")) {
                log_stringf("json_read_account: discarding character with invalid name '%s'",
                           ac->name ? ac->name : "(null)");
                free_account_character(ac);
                continue;
            }

            ac->current_level = json_integer_value(json_object_get(array_elem, "current_level"));
            ac->tot_level = json_integer_value(json_object_get(array_elem, "tot_level"));

            str = json_get_string(array_elem, "race_name", "");
            if (str) { free_string(ac->race_name); ac->race_name = str_dup(str); }

            str = json_get_string(array_elem, "class_name", "");
            if (str) { free_string(ac->class_name); ac->class_name = str_dup(str); }

            str = json_get_string(array_elem, "last_area", "");
            if (str) { free_string(ac->last_area); ac->last_area = str_dup(str); }

            // Character status
            ac->staff = json_is_true(json_object_get(array_elem, "staff"));
            ac->staff_rank = json_integer_value(json_object_get(array_elem, "staff_rank"));
            ac->deleted = json_is_true(json_object_get(array_elem, "deleted"));
            ac->delete_time = json_integer_value(json_object_get(array_elem, "delete_time"));
            ac->creation_date = json_integer_value(json_object_get(array_elem, "creation_date"));
            ac->last_login = json_integer_value(json_object_get(array_elem, "last_login"));
            ac->last_logoff = json_integer_value(json_object_get(array_elem, "last_logoff"));

            // Character ID
            json_t *char_id = json_object_get(array_elem, "id");
            if (char_id && json_is_array(char_id)) {
                ac->id[0] = json_integer_value(json_array_get(char_id, 0));
                ac->id[1] = json_integer_value(json_array_get(char_id, 1));
            }

            // Character-specific auth override
            str = json_get_string(array_elem, "password", "");
            if (str) {
                free_string(ac->pwd);
                ac->pwd = str_dup(str);
                ac->pwd_vers = json_integer_value(json_object_get(array_elem, "password_version"));
            }

            str = json_get_string(array_elem, "mfa_key", "");
            if (str) {
                free_string(ac->mfa_key);
                ac->mfa_key = str_dup(str);
                ac->mfa_enabled = json_is_true(json_object_get(array_elem, "mfa_enabled"));
            }

            // Last host
            str = json_get_string(array_elem, "last_host", "");
            if (str) {
                free_string(ac->last_host);
                ac->last_host = str_dup(str);
            }

            // Old password (migration)
            str = json_get_string(array_elem, "old_password", "");
            if (str) {
                free_string(ac->old_pwd);
                ac->old_pwd = str_dup(str);
            }

            // Password reset
            str = json_get_string(array_elem, "reset_code", "");
            if (str) {
                free_string(ac->reset_code);
                ac->reset_code = str_dup(str);
            }
            value = json_object_get(array_elem, "reset_time");
            if (value) ac->reset_time = json_integer_value(value);
            value = json_object_get(array_elem, "reset_state");
            if (value) ac->reset_state = json_integer_value(value);

            // MFA pending enrollment (character-level)
            str = json_get_string(array_elem, "mfa_pending_key", "");
            if (str) {
                free_string(ac->mfa_pending_key);
                ac->mfa_pending_key = str_dup(str);
            }

            // MFA recovery codes (character-level)
            {
                json_t *char_recovery = json_object_get(array_elem, "recovery_codes");
                if (char_recovery && json_is_array(char_recovery)) {
                    int ri = 0;
                    size_t ridx;
                    json_t *relem;
                    json_array_foreach(char_recovery, ridx, relem) {
                        if (ri >= MFA_RECOVERY_CODES) break;
                        str = json_get_string(relem, "code", "");
                        if (str) {
                            free_string(ac->recovery_codes[ri]);
                            ac->recovery_codes[ri] = str_dup(str);
                            ac->recovery_used[ri] = json_is_true(json_object_get(relem, "used"));
                        }
                        ri++;
                    }
                }
            }

            // Email (character-level)
            str = json_get_string(array_elem, "email", "");
            if (str) {
                free_string(ac->email);
                ac->email = str_dup(str);
            }
            ac->email_verified = json_is_true(json_object_get(array_elem, "email_verified"));

            str = json_get_string(array_elem, "pending_email", "");
            if (str) {
                free_string(ac->pending_email);
                ac->pending_email = str_dup(str);
            }
            str = json_get_string(array_elem, "email_verification_code", "");
            if (str) {
                free_string(ac->email_verification_code);
                ac->email_verification_code = str_dup(str);
            }
            value = json_object_get(array_elem, "email_verification_time");
            if (value) ac->email_verification_time = json_integer_value(value);
            value = json_object_get(array_elem, "email_verification_last_sent");
            if (value) ac->email_verification_last_sent = json_integer_value(value);

            // Character-level staff notes
            {
                json_t *char_notes = json_object_get(array_elem, "staff_notes");
                if (char_notes && json_is_array(char_notes)) {
                    ACCOUNT_NOTE_DATA *prev_cnote = NULL;
                    size_t cnidx;
                    json_t *cnelem;
                    json_array_foreach(char_notes, cnidx, cnelem) {
                        ACCOUNT_NOTE_DATA *cnote = (ACCOUNT_NOTE_DATA *)alloc_mem(sizeof(ACCOUNT_NOTE_DATA));
                        cnote->next = NULL;
                        cnote->penalty_ref = NULL;

                        str = json_get_string(cnelem, "author", "");
                        cnote->author = str ? str_dup(str) : str_dup("unknown");
                        cnote->timestamp = json_integer_value(json_object_get(cnelem, "timestamp"));
                        str = json_get_string(cnelem, "subject", "");
                        cnote->subject = str ? str_dup(str) : str_dup("");
                        str = json_get_string(cnelem, "text", "");
                        cnote->text = str ? str_dup(str) : str_dup("");
                        cnote->category = json_integer_value(json_object_get(cnelem, "category"));
                        str = json_get_string(cnelem, "penalty_ref", "");
                        if (str) cnote->penalty_ref = str_dup(str);

                        if (!ac->staff_notes)
                            ac->staff_notes = cnote;
                        else
                            prev_cnote->next = cnote;
                        prev_cnote = cnote;
                    }
                }
            }

            // Add to account's character list
            list_addlink(account->characters, ac);
        }
    }

    // Read vault section
    vault = json_object_get(root, "vault");
    if (vault && json_is_array(vault)) {
        json_array_foreach(vault, index, array_elem) {
            OBJ_DATA *obj = json_to_obj(array_elem, NULL);
            if (obj) {
                // Add to vault items linked list
                obj->next_content = account->vault_items;
                account->vault_items = obj;
            }
        }
    }

    // Read staff notes section
    staff_notes = json_object_get(root, "staff_notes");
    if (staff_notes && json_is_array(staff_notes)) {
        ACCOUNT_NOTE_DATA *prev_note = NULL;

        json_array_foreach(staff_notes, index, array_elem) {
            ACCOUNT_NOTE_DATA *note = (ACCOUNT_NOTE_DATA *)alloc_mem(sizeof(ACCOUNT_NOTE_DATA));

            str = json_get_string(array_elem, "author", "");
            if (str) note->author = str_dup(str);

            note->timestamp = json_integer_value(json_object_get(array_elem, "timestamp"));

            str = json_get_string(array_elem, "subject", "");
            if (str) note->subject = str_dup(str);

            str = json_get_string(array_elem, "text", "");
            if (str) note->text = str_dup(str);

            note->category = json_integer_value(json_object_get(array_elem, "category"));
            str = json_get_string(array_elem, "penalty_ref", "");
            if (str) note->penalty_ref = str_dup(str);
            else note->penalty_ref = NULL;

            // Link to list
            if (!account->staff_notes) {
                account->staff_notes = note;
            } else {
                prev_note->next = note;
            }
            prev_note = note;
        }
    }

    // Read penalties
    json_t *penalties_arr = json_object_get(root, "penalties");
    if (penalties_arr && json_is_array(penalties_arr)) {
        json_to_penalties(penalties_arr, &account->penalties);
    }

    // Read bonuses
    json_t *bonuses_arr = json_object_get(root, "bonuses");
    if (bonuses_arr && json_is_array(bonuses_arr)) {
        json_to_bonuses(bonuses_arr, &account->bonuses);
    }

    // Read preferences
    json_t *prefs_arr = json_object_get(root, "preferences");
    if (prefs_arr && json_is_array(prefs_arr)) {
        json_to_prefs(prefs_arr, &account->preferences);
    }

    // Read unlocked races
    json_t *unlocked_arr = json_object_get(root, "unlocked_races");
    if (unlocked_arr && json_is_array(unlocked_arr)) {
        json_to_unlocked_races(unlocked_arr, account);
    }

    json_decref(root);

    log_stringf("JSON: Loaded account %s from %s", account->username, filename);
    return true;
}

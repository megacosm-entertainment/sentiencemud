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
#include "json_account.h"
#include "json_char.h"  // For obj_to_json() reuse

/***************************************************************************
 * External Flag Tables                                                    *
 ***************************************************************************/

extern const struct flag_type acct_flags[];

/***************************************************************************
 * Flag Serialization Helpers                                              *
 ***************************************************************************/

static json_t *flags_to_json_array(const struct flag_type *flag_table, long bits)
{
    json_t *flags_array;
    int i;

    if (!flag_table) {
        return json_array();
    }

    flags_array = json_array();

    // Convert each set bit to its flag name
    for (i = 0; flag_table[i].name != NULL; i++) {
        if (!is_stat(flag_table) && IS_SET(bits, flag_table[i].bit)) {
            json_array_append_new(flags_array, json_string(flag_table[i].name));
        } else if (flag_table[i].bit == bits) {
            json_array_append_new(flags_array, json_string(flag_table[i].name));
            break;
        }
    }

    return flags_array;
}

static long flags_from_json_array(const struct flag_type *flag_table, json_t *flags_array)
{
    long bits = 0;
    size_t index;
    json_t *value;
    const char *flag_name;

    if (!flag_table || !flags_array || !json_is_array(flags_array)) {
        return 0;
    }

    // Convert each flag name to its bit value
    json_array_foreach(flags_array, index, value) {
        flag_name = json_string_value(value);
        if (!flag_name) continue;

        // Look up flag by name
        for (int i = 0; flag_table[i].name != NULL; i++) {
            if (!str_cmp(flag_table[i].name, flag_name)) {
                SET_BIT(bits, flag_table[i].bit);
                break;
            }
        }
    }

    return bits;
}

/***************************************************************************
 * Utility Functions                                                       *
 ***************************************************************************/

void json_get_account_path(const char *username, char *path_buf, size_t buf_size)
{
    // No .json extension - same path as old pfile
    // Format is auto-detected on load
    snprintf(path_buf, buf_size, "%s%c/%s",
             ACCOUNT_DIR, tolower(username[0]), username);
}

void json_get_account_backup_path(const char *username, char *path_buf, size_t buf_size)
{
    // Backup old pfile before migration
    snprintf(path_buf, buf_size, "%s%c.old/%s",
             ACCOUNT_DIR, tolower(username[0]), username);
}

bool json_ensure_account_dir(const char *username)
{
    char dir_path[256];

    snprintf(dir_path, sizeof(dir_path), "%s%c",
             ACCOUNT_DIR, tolower(username[0]));

    // Try to create directory (will fail if exists, which is fine)
    mkdir(dir_path, 0755);
    return true;
}

bool json_is_account_json(const char *filename)
{
    FILE *fp;
    char first_char;
    bool is_json;

    fp = fopen(filename, "r");
    if (!fp) {
        return false;  // File doesn't exist
    }

    // JSON files start with '{'
    first_char = fgetc(fp);
    is_json = (first_char == '{');
    fclose(fp);

    return is_json;
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
    json_object_set_new(basic, "account_flags", flags_to_json_array(acct_flags, account->acct_flags));
    json_object_set_new(basic, "acct_flags_numeric", json_integer(account->acct_flags));

    // Account limits and status
    json_object_set_new(basic, "character_count", json_integer(account->character_count));
    json_object_set_new(basic, "character_limit", json_integer(account->character_limit));
    json_object_set_new(basic, "staff_account", json_boolean(account->staff_account));

    // Vault rent
    if (account->vault_rent > 0) {
        json_object_set_new(basic, "vault_rent", json_integer(account->vault_rent));
    }

    return basic;
}

/***************************************************************************
 * Character References Serialization                                      *
 ***************************************************************************/

static json_t *characters_to_json(ACCOUNT_DATA *account)
{
    json_t *characters;
    LLIST_LINK *node;
    ACCOUNT_CHARACTER *ac;

    characters = json_array();

    if (!account->characters) {
        return characters;
    }

    // Iterate through linked list of characters
    for (node = account->characters->head; node; node = node->next) {
        ac = (ACCOUNT_CHARACTER *)node->data;
        if (!ac) continue;

        json_t *char_obj = json_object();

        // Basic character info - use IS_NULLSTR to validate strings before json_string
        // This prevents crashes from dangling pointers that pass NULL check but are invalid
        if (!IS_NULLSTR(ac->name)) {
            json_object_set_new(char_obj, "name", json_string(ac->name));
        } else {
            log_stringf("characters_to_json: WARNING - character with NULL/empty name in account");
            json_object_set_new(char_obj, "name", json_string("(unknown)"));
        }
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

        json_array_append_new(characters, char_obj);
    }

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

    return root;
}

/***************************************************************************
 * File I/O - Write                                                        *
 ***************************************************************************/

static bool backup_old_pfile(const char *filename, const char *username)
{
    char backup_path[512];
    char backup_dir[256];
    FILE *src, *dst;
    char buffer[4096];
    size_t bytes;

    // Check if file exists and is old format
    if (!json_is_account_json(filename)) {
        // It's an old pfile, back it up
        json_get_account_backup_path(username, backup_path, sizeof(backup_path));

        // Create backup directory
        snprintf(backup_dir, sizeof(backup_dir), "%s%c.old",
                ACCOUNT_DIR, tolower(username[0]));
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
    str = json_string_value(json_object_get(account_obj, "username"));
    if (str) {
        free_string(account->username);
        account->username = str_dup(str);
    }

    // Password
    str = json_string_value(json_object_get(account_obj, "password"));
    if (str) {
        free_string(account->passwd);
        account->passwd = str_dup(str);
    }
    value = json_object_get(account_obj, "password_version");
    if (value) account->passwd_version = json_integer_value(value);

    // MFA
    account->mfa_enabled = json_is_true(json_object_get(account_obj, "mfa_enabled"));
    str = json_string_value(json_object_get(account_obj, "mfa_key"));
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

            str = json_string_value(json_object_get(array_elem, "code"));
            if (str) {
                free_string(account->recovery_codes[i]);
                account->recovery_codes[i] = str_dup(str);
                account->recovery_used[i] = json_is_true(json_object_get(array_elem, "used"));
            }
            i++;
        }
    }

    // Email
    str = json_string_value(json_object_get(account_obj, "email"));
    if (str) {
        free_string(account->email);
        account->email = str_dup(str);
    }
    account->email_verified = json_is_true(json_object_get(account_obj, "email_verified"));

    // Email verification code
    str = json_string_value(json_object_get(account_obj, "email_verification_code"));
    if (str) {
        free_string(account->email_verification_code);
        account->email_verification_code = str_dup(str);
    }
    value = json_object_get(account_obj, "email_verification_time");
    if (value) account->email_verification_time = json_integer_value(value);

    str = json_string_value(json_object_get(account_obj, "pending_email"));
    if (str) {
        free_string(account->pending_email);
        account->pending_email = str_dup(str);
    }

    // Password reset code
    str = json_string_value(json_object_get(account_obj, "reset_code"));
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
        account->acct_flags = flags_from_json_array(acct_flags, value);
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

    // Read characters section
    characters = json_object_get(root, "characters");
    if (characters && json_is_array(characters)) {
        // Initialize character list if not already done
        if (!account->characters) {
            account->characters = list_create(false);
        }

        json_array_foreach(characters, index, array_elem) {
            ACCOUNT_CHARACTER *ac = (ACCOUNT_CHARACTER *)alloc_mem(sizeof(ACCOUNT_CHARACTER));

            // Basic character info
            str = json_string_value(json_object_get(array_elem, "name"));
            if (str) ac->name = str_dup(str);

            ac->current_level = json_integer_value(json_object_get(array_elem, "current_level"));
            ac->tot_level = json_integer_value(json_object_get(array_elem, "tot_level"));

            str = json_string_value(json_object_get(array_elem, "race_name"));
            if (str) ac->race_name = str_dup(str);

            str = json_string_value(json_object_get(array_elem, "class_name"));
            if (str) ac->class_name = str_dup(str);

            str = json_string_value(json_object_get(array_elem, "last_area"));
            if (str) ac->last_area = str_dup(str);

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
            str = json_string_value(json_object_get(array_elem, "password"));
            if (str) {
                ac->pwd = str_dup(str);
                ac->pwd_vers = json_integer_value(json_object_get(array_elem, "password_version"));
            }

            str = json_string_value(json_object_get(array_elem, "mfa_key"));
            if (str) {
                ac->mfa_key = str_dup(str);
                ac->mfa_enabled = json_is_true(json_object_get(array_elem, "mfa_enabled"));
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

            str = json_string_value(json_object_get(array_elem, "author"));
            if (str) note->author = str_dup(str);

            note->timestamp = json_integer_value(json_object_get(array_elem, "timestamp"));

            str = json_string_value(json_object_get(array_elem, "subject"));
            if (str) note->subject = str_dup(str);

            str = json_string_value(json_object_get(array_elem, "text"));
            if (str) note->text = str_dup(str);

            // Link to list
            if (!account->staff_notes) {
                account->staff_notes = note;
            } else {
                prev_note->next = note;
            }
            prev_note = note;
        }
    }

    json_decref(root);

    log_stringf("JSON: Loaded account %s from %s", account->username, filename);
    return true;
}

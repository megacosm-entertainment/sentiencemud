#include <sys/types.h>
#include <crypt.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "../merc.h"
#include "../recycle.h"
#include "auth.h"
#include "auth_sodium.h"

/* External functions we depend on */
extern char *sha256_crypt(const char *pwd);
extern bool validate_totp_code(const char *encrypted_key, const char *code);
extern bool get_character_auth_data(CHAR_DATA *ch, ACCOUNT_DATA *acct, ACCOUNT_CHARACTER **acct_char);

/*
 * Helper function to populate AUTH_DATA from ACCOUNT_DATA
 */
static void populate_from_account(AUTH_DATA *auth, ACCOUNT_DATA *acct)
{
    if (!auth || !acct)
        return;

    /* Password data */
    auth->password = acct->passwd ? str_dup(acct->passwd) : str_dup("");
    auth->old_password = acct->old_passwd ? str_dup(acct->old_passwd) : str_dup("");
    auth->password_version = acct->passwd_version;

    /* Reset data */
    auth->reset_code = acct->reset_code ? str_dup(acct->reset_code) : str_dup("");
    auth->reset_time = acct->reset_time;
    auth->reset_state = acct->reset_state;

    /* MFA data */
    auth->mfa_key = acct->mfa_key ? str_dup(acct->mfa_key) : str_dup("");
    auth->mfa_enabled = acct->mfa_enabled;
    auth->mfa_pending_key = acct->mfa_pending_key ? str_dup(acct->mfa_pending_key) : str_dup("");
    auth->mfa_pending = acct->mfa_pending;

    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
        auth->recovery_codes[i] = acct->recovery_codes[i] ? str_dup(acct->recovery_codes[i]) : str_dup("");
        auth->recovery_used[i] = acct->recovery_used[i];
    }

    /* Email data */
    auth->email = acct->email ? str_dup(acct->email) : str_dup("");
    auth->email_verified = acct->email_verified;
    auth->pending_email = acct->pending_email ? str_dup(acct->pending_email) : str_dup("");
    auth->email_verification_code = acct->email_verification_code ? str_dup(acct->email_verification_code) : str_dup("");
    auth->email_verification_time = acct->email_verification_time;
    auth->email_verification_last_sent = acct->email_verification_last_sent;
}

/*
 * Helper function to populate AUTH_DATA from ACCOUNT_CHARACTER
 */
static void populate_from_account_character(AUTH_DATA *auth, ACCOUNT_CHARACTER *acct_char)
{
    if (!auth || !acct_char)
        return;

    /* Password data */
    auth->password = acct_char->pwd ? str_dup(acct_char->pwd) : str_dup("");
    auth->old_password = acct_char->old_pwd ? str_dup(acct_char->old_pwd) : str_dup("");
    auth->password_version = acct_char->pwd_vers;

    /* Reset data */
    auth->reset_code = acct_char->reset_code ? str_dup(acct_char->reset_code) : str_dup("");
    auth->reset_time = acct_char->reset_time;
    auth->reset_state = acct_char->reset_state;

    /* MFA data */
    auth->mfa_key = acct_char->mfa_key ? str_dup(acct_char->mfa_key) : str_dup("");
    auth->mfa_enabled = acct_char->mfa_enabled;
    auth->mfa_pending_key = acct_char->mfa_pending_key ? str_dup(acct_char->mfa_pending_key) : str_dup("");
    auth->mfa_pending = false; /* ACCOUNT_CHARACTER doesn't have mfa_pending field */

    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
        auth->recovery_codes[i] = acct_char->recovery_codes[i] ? str_dup(acct_char->recovery_codes[i]) : str_dup("");
        auth->recovery_used[i] = acct_char->recovery_used[i];
    }

    /* Email data */
    auth->email = acct_char->email ? str_dup(acct_char->email) : str_dup("");
    auth->email_verified = acct_char->email_verified;
    auth->pending_email = acct_char->pending_email ? str_dup(acct_char->pending_email) : str_dup("");
    auth->email_verification_code = acct_char->email_verification_code ? str_dup(acct_char->email_verification_code) : str_dup("");
    auth->email_verification_time = acct_char->email_verification_time;
    auth->email_verification_last_sent = acct_char->email_verification_last_sent;
}

/*
 * Helper function to populate AUTH_DATA from PC_DATA (for unlinked characters)
 */
static void populate_from_pcdata(AUTH_DATA *auth, PC_DATA *pcdata)
{
    if (!auth || !pcdata)
        return;

    /* Password data */
    auth->password = pcdata->pwd ? str_dup(pcdata->pwd) : str_dup("");
    auth->old_password = pcdata->old_pwd ? str_dup(pcdata->old_pwd) : str_dup("");
    auth->password_version = pcdata->pwd_vers;

    /* Reset data */
    auth->reset_code = pcdata->reset_code ? str_dup(pcdata->reset_code) : str_dup("");
    auth->reset_time = pcdata->reset_time;
    auth->reset_state = pcdata->reset_state;

    /* MFA data */
    auth->mfa_key = pcdata->mfa_key ? str_dup(pcdata->mfa_key) : str_dup("");
    auth->mfa_enabled = pcdata->mfa_enabled;
    auth->mfa_pending_key = pcdata->mfa_pending_key ? str_dup(pcdata->mfa_pending_key) : str_dup("");
    auth->mfa_pending = pcdata->mfa_pending;

    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
        auth->recovery_codes[i] = pcdata->recovery_codes[i] ? str_dup(pcdata->recovery_codes[i]) : str_dup("");
        auth->recovery_used[i] = pcdata->recovery_used[i];
    }

    /* Email data */
    auth->email = pcdata->email ? str_dup(pcdata->email) : str_dup("");
    auth->email_verified = pcdata->email_verified;
    auth->pending_email = pcdata->pending_email ? str_dup(pcdata->pending_email) : str_dup("");
    auth->email_verification_code = pcdata->email_verification_code ? str_dup(pcdata->email_verification_code) : str_dup("");
    auth->email_verification_time = pcdata->email_verification_time;
    auth->email_verification_last_sent = pcdata->email_verification_last_sent;
}

/*
 * Helper to overlay character-level auth on top of account auth
 * Character auth takes precedence where it exists
 */
static void overlay_from_character(AUTH_DATA *auth, ACCOUNT_CHARACTER *acct_char)
{
    if (!auth || !acct_char)
        return;

    /* Only overlay non-empty character auth fields */
    if (!IS_NULLSTR(acct_char->pwd)) {
        free_string(auth->password);
        auth->password = str_dup(acct_char->pwd);
        auth->password_version = acct_char->pwd_vers;
    }

    if (!IS_NULLSTR(acct_char->mfa_key)) {
        free_string(auth->mfa_key);
        auth->mfa_key = str_dup(acct_char->mfa_key);
        auth->mfa_enabled = acct_char->mfa_enabled;

        /* Also copy recovery codes if character has MFA */
        for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
            if (!IS_NULLSTR(acct_char->recovery_codes[i])) {
                free_string(auth->recovery_codes[i]);
                auth->recovery_codes[i] = str_dup(acct_char->recovery_codes[i]);
                auth->recovery_used[i] = acct_char->recovery_used[i];
            }
        }
    }

    if (!IS_NULLSTR(acct_char->email)) {
        free_string(auth->email);
        auth->email = str_dup(acct_char->email);
        auth->email_verified = acct_char->email_verified;
    }
}

/*
 * Get unified auth data for a character/account combination
 * Returns allocated AUTH_DATA that must be freed with free_auth_data()
 */
AUTH_DATA *get_auth_data(CHAR_DATA *ch, ACCOUNT_DATA *acct)
{
    AUTH_DATA *auth = alloc_mem(sizeof(AUTH_DATA));
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool found = false;

    /* Initialize to safe defaults */
    memset(auth, 0, sizeof(AUTH_DATA));
    auth->source = AUTH_SOURCE_NONE;
    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
        auth->recovery_codes[i] = str_dup("");
    }

    /* No character - account-only auth */
    if (!ch || IS_NPC(ch)) {
        if (acct) {
            auth->source = AUTH_SOURCE_ACCOUNT;
            populate_from_account(auth, acct);
        }
        return auth;
    }

    /* Check if character is linked to an account */
    if (IS_NULLSTR(ch->pcdata->account_name) || !acct) {
        /* Unlinked character - auth stored in PC_DATA */
        auth->source = AUTH_SOURCE_CHARACTER;
        populate_from_pcdata(auth, ch->pcdata);
        return auth;
    }

    /* Linked character - find character in account */
    found = get_character_auth_data(ch, acct, &acct_char);

    if (!found || !acct_char) {
        /* Character should be linked but not found in account - use account auth */
        auth->source = AUTH_SOURCE_ACCOUNT;
        populate_from_account(auth, acct);
        return auth;
    }

    /* Check for character-level auth override */
    bool has_char_pwd = !IS_NULLSTR(acct_char->pwd);
    bool has_char_mfa = !IS_NULLSTR(acct_char->mfa_key);

    if (has_char_pwd || has_char_mfa) {
        /* Character has its own auth (extra security or legacy) */
        auth->source = AUTH_SOURCE_BOTH;
        populate_from_account(auth, acct);
        overlay_from_character(auth, acct_char);
    } else {
        /* Standard linked character - account auth only */
        auth->source = AUTH_SOURCE_ACCOUNT;
        populate_from_account(auth, acct);
    }

    return auth;
}

/*
 * Get auth data for account only (no character context)
 */
AUTH_DATA *get_account_auth(ACCOUNT_DATA *acct)
{
    return get_auth_data(NULL, acct);
}

/*
 * Get auth data for a specific account character
 */
AUTH_DATA *get_character_auth(ACCOUNT_CHARACTER *acct_char)
{
    AUTH_DATA *auth = alloc_mem(sizeof(AUTH_DATA));

    memset(auth, 0, sizeof(AUTH_DATA));
    auth->source = AUTH_SOURCE_CHARACTER;

    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
        auth->recovery_codes[i] = str_dup("");
    }

    populate_from_account_character(auth, acct_char);
    return auth;
}

/*
 * Free auth data structure
 */
void free_auth_data(AUTH_DATA *auth)
{
    if (!auth)
        return;

    /* Free all allocated strings */
    if (auth->password) free_string(auth->password);
    if (auth->old_password) free_string(auth->old_password);
    if (auth->reset_code) free_string(auth->reset_code);
    if (auth->mfa_key) free_string(auth->mfa_key);
    if (auth->mfa_pending_key) free_string(auth->mfa_pending_key);

    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
        if (auth->recovery_codes[i])
            free_string(auth->recovery_codes[i]);
    }

    if (auth->email) free_string(auth->email);
    if (auth->pending_email) free_string(auth->pending_email);
    if (auth->email_verification_code) free_string(auth->email_verification_code);

    /* Free the structure itself */
    free_mem(auth, sizeof(AUTH_DATA));
}

/*
 * Verify password against auth data
 * Consolidates the tiered password checking logic that was duplicated 16+ times
 * Returns the verification result type
 */
pwd_result_t verify_password(const char *input, AUTH_DATA *auth)
{
    if (!input || !auth || IS_NULLSTR(auth->password))
        return PWD_INVALID;

    /* 1. Try Argon2id (current preferred method) */
    if (is_argon2id_hash(auth->password)) {
        if (verify_password_v3(auth->password, input)) {
            return PWD_VALID_ARGON2ID;
        }
        return PWD_INVALID;
    }

    /* 2. Try system crypt() (legacy, needs upgrade) */
    if (strcmp(crypt(input, auth->password), auth->password) == 0) {
        return PWD_VALID_CRYPT;
    }

    /* 3. Fallback to custom sha256_crypt() (legacy, needs upgrade) */
    if (strcmp(sha256_crypt(input), auth->password) == 0) {
        return PWD_VALID_SHA256;
    }

    /* 4. Fallback to plaintext comparison (ancient legacy, needs forced upgrade) */
    if (strcmp(input, auth->password) == 0) {
        return PWD_VALID_PLAINTEXT;
    }

    return PWD_INVALID;
}

/*
 * Hash a password using the current preferred method (Argon2id)
 */
char *hash_password(const char *plaintext)
{
    if (!plaintext)
        return str_dup("");

    /* Use Argon2id (interactive level for normal logins) */
    return hash_password_v3(plaintext, PWD_SECURITY_INTERACTIVE);
}

/*
 * Hash a password using legacy crypt() method (for compatibility testing)
 */
char *hash_password_legacy(const char *plaintext)
{
    if (!plaintext)
        return str_dup("");

    /* Use system crypt() with plaintext as salt (insecure, kept for compatibility) */
    char *hashed = crypt(plaintext, plaintext);
    return str_dup(hashed);
}

/*
 * Check if password meets strength requirements
 */
bool validate_password_strength(const char *password, char *error_msg, int error_len)
{
    if (!password) {
        if (error_msg)
            snprintf(error_msg, error_len, "Password cannot be NULL");
        return false;
    }

    /* Minimum length check */
    if (strlen(password) < 5) {
        if (error_msg)
            snprintf(error_msg, error_len, "Password must be at least 5 characters");
        return false;
    }

    /* Maximum length check (for buffer safety) */
    if (strlen(password) > 50) {
        if (error_msg)
            snprintf(error_msg, error_len, "Password must be 50 characters or less");
        return false;
    }

    /* Check for at least one letter and one number (basic strength) */
    bool has_letter = false;
    bool has_digit = false;

    for (const char *p = password; *p; p++) {
        if (isalpha(*p))
            has_letter = true;
        if (isdigit(*p))
            has_digit = true;
    }

    if (!has_letter || !has_digit) {
        if (error_msg)
            snprintf(error_msg, error_len, "Password must contain both letters and numbers");
        return false;
    }

    return true;
}

/*
 * Verify MFA code (TOTP) against auth data
 * Consolidates MFA verification logic that was duplicated 17+ times
 */
bool verify_mfa_code(const char *code, AUTH_DATA *auth)
{
    if (!code || !auth)
        return false;

    /* Determine which key to use - pending key if in setup, otherwise active key */
    const char *key_to_use = NULL;

    if (!IS_NULLSTR(auth->mfa_pending_key)) {
        key_to_use = auth->mfa_pending_key;
    } else if (!IS_NULLSTR(auth->mfa_key)) {
        key_to_use = auth->mfa_key;
    } else {
        return false; /* No MFA key available */
    }

    /* Use existing validate_totp_code function which handles decryption */
    return validate_totp_code(key_to_use, code);
}

/*
 * Verify recovery code against auth data
 */
bool verify_recovery_code(const char *code, AUTH_DATA *auth)
{
    if (!code || !auth)
        return false;

    /* Check each recovery code */
    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
        if (!IS_NULLSTR(auth->recovery_codes[i]) &&
            !auth->recovery_used[i] &&
            strcmp(code, auth->recovery_codes[i]) == 0) {
            return true;
        }
    }

    return false;
}

/*
 * Mark a recovery code as used in the actual storage structures
 * Returns true if code was found and marked
 */
bool mark_recovery_code_used(const char *code, AUTH_DATA *auth, ACCOUNT_DATA *acct, ACCOUNT_CHARACTER *acct_char)
{
    if (!code || !auth)
        return false;

    /* Find the matching recovery code */
    for (int i = 0; i < MFA_RECOVERY_CODES; i++) {
        if (!IS_NULLSTR(auth->recovery_codes[i]) &&
            !auth->recovery_used[i] &&
            strcmp(code, auth->recovery_codes[i]) == 0) {

            /* Mark as used in the auth data */
            auth->recovery_used[i] = true;

            /* Mark as used in the actual storage structure */
            if (auth->source == AUTH_SOURCE_ACCOUNT && acct) {
                acct->recovery_used[i] = true;
                return true;
            } else if (auth->source == AUTH_SOURCE_CHARACTER && acct_char) {
                acct_char->recovery_used[i] = true;
                return true;
            } else if (auth->source == AUTH_SOURCE_BOTH) {
                /* For both, mark in character (since that's where MFA override is) */
                if (acct_char) {
                    acct_char->recovery_used[i] = true;
                    return true;
                }
            }

            return true;
        }
    }

    return false;
}

/*
 * Query functions
 */

bool has_character_password(ACCOUNT_CHARACTER *acct_char)
{
    return acct_char && !IS_NULLSTR(acct_char->pwd);
}

bool has_character_mfa(ACCOUNT_CHARACTER *acct_char)
{
    return acct_char && !IS_NULLSTR(acct_char->mfa_key) && acct_char->mfa_enabled;
}

bool requires_character_auth(CHAR_DATA *ch, ACCOUNT_DATA *acct)
{
    if (!ch || IS_NPC(ch) || !acct)
        return false;

    /* Unlinked characters always use their own auth */
    if (IS_NULLSTR(ch->pcdata->account_name))
        return true;

    /* Check if character has override auth */
    ACCOUNT_CHARACTER *acct_char = NULL;
    if (get_character_auth_data(ch, acct, &acct_char) && acct_char) {
        return has_character_password(acct_char) || has_character_mfa(acct_char);
    }

    return false;
}

/*
 * Update functions - these modify the actual storage structures
 * TODO: These will be implemented in Phase 2-3 when we integrate with save logic
 */

void update_password(CHAR_DATA *ch, ACCOUNT_DATA *acct, const char *new_password, bool char_level)
{
    /* Placeholder for Phase 2 implementation */
    /* This will update either account->passwd or acct_char->pwd based on char_level flag */
}

void update_mfa_key(CHAR_DATA *ch, ACCOUNT_DATA *acct, const char *mfa_key, bool char_level)
{
    /* Placeholder for Phase 2 implementation */
    /* This will update either account->mfa_key or acct_char->mfa_key based on char_level flag */
}

void update_email(CHAR_DATA *ch, ACCOUNT_DATA *acct, const char *email, bool char_level)
{
    /* Placeholder for Phase 2 implementation */
    /* This will update either account->email or acct_char->email based on char_level flag */
}

/*
 * Password Migration Functions
 */

/**
 * needs_password_upgrade - Check if password hash needs upgrading
 *
 * @param hash  Password hash to check
 * @return      true if hash should be upgraded to Argon2id
 */
bool needs_password_upgrade(const char *hash)
{
    if (IS_NULLSTR(hash))
        return false;

    /* Already using Argon2id - no upgrade needed */
    if (is_argon2id_hash(hash))
        return false;

    /* Any other format needs upgrade */
    return true;
}

/**
 * migrate_password_on_login - Migrate password to Argon2id on successful login
 *
 * This function is called after successful password verification with a legacy
 * hash format. It rehashes the password using Argon2id and updates the account
 * or character storage immediately.
 *
 * @param ch                  Character logging in (NULL for account-only login)
 * @param acct                Account being authenticated
 * @param plaintext           Plain text password (for rehashing)
 * @param verification_result Result from verify_password()
 * @return                    true if migration succeeded, false otherwise
 */
bool migrate_password_on_login(CHAR_DATA *ch, ACCOUNT_DATA *acct, const char *plaintext, pwd_result_t verification_result)
{
    char *new_hash;
    ACCOUNT_CHARACTER *acct_char = NULL;
    bool has_char_password = false;

    if (!acct || IS_NULLSTR(plaintext))
        return false;

    /* Don't migrate if already using Argon2id */
    if (verification_result == PWD_VALID_ARGON2ID)
        return true;

    /* Don't migrate invalid passwords */
    if (verification_result == PWD_INVALID)
        return false;

    /* Generate new Argon2id hash */
    new_hash = hash_password_v3(plaintext, PWD_SECURITY_INTERACTIVE);
    if (!new_hash) {
        log_message(LOG_LEVEL_ERROR, LOG_ERROR, "migrate_password_on_login: Failed to hash password");
        return false;
    }

    /* Determine where to store the upgraded hash */
    if (ch && !IS_NPC(ch)) {
        /* Check if character has its own password */
        if (get_character_auth_data(ch, acct, &acct_char) && acct_char) {
            has_char_password = !IS_NULLSTR(acct_char->pwd);
        }

        if (has_char_password && acct_char) {
            /* Upgrade character-level password */
            free_string(acct_char->pwd);
            acct_char->pwd = str_dup(new_hash);
            acct_char->pwd_vers = PWD_VER_ARGON2ID;

            log_message_f(LOG_LEVEL_INFO, LOG_SECURITY,
                "Migrated character password to Argon2id: %s (account: %s)",
                ch->name, acct->username);
        } else {
            /* Upgrade account-level password */
            free_string(acct->passwd);
            acct->passwd = str_dup(new_hash);
            acct->passwd_version = PWD_VER_ARGON2ID;

            log_message_f(LOG_LEVEL_INFO, LOG_SECURITY,
                "Migrated account password to Argon2id: %s",
                acct->username);
        }
    } else {
        /* Account-only login (no character context) */
        free_string(acct->passwd);
        acct->passwd = str_dup(new_hash);
        acct->passwd_version = PWD_VER_ARGON2ID;

        log_message_f(LOG_LEVEL_INFO, LOG_SECURITY,
            "Migrated account password to Argon2id: %s",
            acct->username);
    }

    /* Save the account with upgraded password */
    save_account(acct);

    /* Clean up */
    free_string(new_hash);

    return true;
}

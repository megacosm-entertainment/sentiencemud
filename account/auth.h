#ifndef AUTH_H
#define AUTH_H

#include "../merc.h"

/*
 * Authentication API for Sentience MUD
 *
 * This module centralizes all authentication logic to eliminate duplication
 * across ACCOUNT_DATA, ACCOUNT_CHARACTER, and PC_DATA structures.
 *
 * Auth Data Location Strategy:
 * - Linked characters (normal): Auth in ACCOUNT_DATA only
 * - Character override (extra security): Auth in BOTH ACCOUNT_DATA and ACCOUNT_CHARACTER
 * - Unlinked characters (transfers): Auth in PC_DATA only
 */

/* Authentication data source */
typedef enum {
    AUTH_SOURCE_NONE,        /* No auth data found */
    AUTH_SOURCE_ACCOUNT,     /* Primary auth at account level */
    AUTH_SOURCE_CHARACTER,   /* Character-specific auth (unlinked or override) */
    AUTH_SOURCE_BOTH        /* Both required (layered security) */
} auth_source_t;

/* Password verification result */
typedef enum {
    PWD_INVALID,            /* Password is incorrect */
    PWD_VALID_CRYPT,        /* Valid using crypt() - preferred method */
    PWD_VALID_SHA256,       /* Valid using sha256_crypt() - needs upgrade */
    PWD_VALID_PLAINTEXT     /* Valid using plaintext - needs forced upgrade */
} pwd_result_t;

/*
 * Unified auth data structure
 * This is a VIEW of auth data, not the storage itself.
 * Must be freed with free_auth_data() after use.
 */
typedef struct auth_data {
    /* Password data */
    char *password;           /* Current password hash */
    char *old_password;       /* Previous password hash */
    int password_version;     /* Password hash version */

    /* Password reset */
    char *reset_code;         /* Password reset code */
    time_t reset_time;        /* Reset expiration time */
    int reset_state;          /* Reset state machine position */

    /* MFA data */
    char *mfa_key;            /* MFA secret key (encrypted) */
    bool mfa_enabled;         /* MFA enabled flag */
    char *mfa_pending_key;    /* Pending MFA setup key */
    bool mfa_pending;         /* MFA setup in progress */
    char *recovery_codes[MFA_RECOVERY_CODES];
    bool recovery_used[MFA_RECOVERY_CODES];

    /* Email data */
    char *email;              /* Email address */
    bool email_verified;      /* Email verification status */
    char *pending_email;      /* Email pending verification */
    char *email_verification_code;
    time_t email_verification_time;
    time_t email_verification_last_sent;

    /* Metadata */
    auth_source_t source;     /* Where this data came from */
} AUTH_DATA;

/*
 * Core API Functions
 */

/* Get unified auth data for a character/account combination */
AUTH_DATA *get_auth_data(CHAR_DATA *ch, ACCOUNT_DATA *acct);

/* Get auth data for account only */
AUTH_DATA *get_account_auth(ACCOUNT_DATA *acct);

/* Get auth data for a specific account character */
AUTH_DATA *get_character_auth(ACCOUNT_CHARACTER *acct_char);

/* Free auth data structure */
void free_auth_data(AUTH_DATA *auth);

/*
 * Password Functions
 */

/* Verify password against auth data - returns verification result */
pwd_result_t verify_password(const char *input, AUTH_DATA *auth);

/* Hash a password using the current preferred method (crypt) */
char *hash_password(const char *plaintext);

/* Check if password meets strength requirements */
bool validate_password_strength(const char *password, char *error_msg, int error_len);

/*
 * MFA Functions
 */

/* Verify MFA code (TOTP) against auth data */
bool verify_mfa_code(const char *code, AUTH_DATA *auth);

/* Verify recovery code against auth data */
bool verify_recovery_code(const char *code, AUTH_DATA *auth);

/* Mark a recovery code as used */
bool mark_recovery_code_used(const char *code, AUTH_DATA *auth, ACCOUNT_DATA *acct, ACCOUNT_CHARACTER *acct_char);

/*
 * Query Functions
 */

/* Check if an account character has its own password */
bool has_character_password(ACCOUNT_CHARACTER *acct_char);

/* Check if an account character has its own MFA */
bool has_character_mfa(ACCOUNT_CHARACTER *acct_char);

/* Check if a character requires character-level auth in addition to account auth */
bool requires_character_auth(CHAR_DATA *ch, ACCOUNT_DATA *acct);

/*
 * Update Functions (these modify the actual storage structures)
 */

/* Update password - updates either account or character based on char_level flag */
void update_password(CHAR_DATA *ch, ACCOUNT_DATA *acct, const char *new_password, bool char_level);

/* Update MFA key - updates either account or character based on char_level flag */
void update_mfa_key(CHAR_DATA *ch, ACCOUNT_DATA *acct, const char *mfa_key, bool char_level);

/* Update email - updates either account or character based on char_level flag */
void update_email(CHAR_DATA *ch, ACCOUNT_DATA *acct, const char *email, bool char_level);

#endif /* AUTH_H */

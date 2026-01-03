/***************************************************************************
 *  JSON Account Format - Account Data Serialization                       *
 *                                                                          *
 *  Provides JSON serialization for account data including:                *
 *  - Authentication (passwords, MFA, email)                               *
 *  - Character references                                                 *
 *  - Vault items                                                          *
 *  - Staff notes                                                          *
 *  - Human-readable flag names                                            *
 ***************************************************************************/

#ifndef JSON_ACCOUNT_H
#define JSON_ACCOUNT_H

#include "merc.h"
#include <jansson.h>

/***************************************************************************
 * File Structure                                                          *
 ***************************************************************************/

// Single JSON file: accounts/{letter}/{Username}
// Format auto-detected (JSON vs old pfile)
// Contains sections:
//   - metadata (version, timestamps)
//   - account (auth, email, flags, settings)
//   - characters[] (character references)
//   - vault[] (vault items)
//   - staff_notes[] (staff annotations)

/***************************************************************************
 * Core Serialization Functions                                           *
 ***************************************************************************/

// Serialize account to JSON
json_t *account_to_json(ACCOUNT_DATA *account);

// Write account to JSON file (atomic write with backup)
bool json_write_account(ACCOUNT_DATA *account, const char *filename);

// Read account from JSON file (with pfile fallback)
bool json_read_account(ACCOUNT_DATA *account, const char *filename);

/***************************************************************************
 * Utility Functions                                                       *
 ***************************************************************************/

// Get account file path (unified for JSON and pfile)
// Example: accounts/e/Elzamine
void json_get_account_path(const char *username, char *path_buf, size_t buf_size);

// Get backup path for old pfile before migration
// Example: accounts/e.old/Elzamine
void json_get_account_backup_path(const char *username, char *path_buf, size_t buf_size);

// Create directory structure if needed
bool json_ensure_account_dir(const char *username);

// Check if file is JSON format
bool json_is_account_json(const char *filename);

#endif /* JSON_ACCOUNT_H */

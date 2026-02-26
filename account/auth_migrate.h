#ifndef AUTH_MIGRATE_H
#define AUTH_MIGRATE_H

#include "../merc.h"
#include "auth.h"

/*
 * Authentication Migration Utilities
 *
 * These utilities help diagnose and track the migration of authentication data
 * from PC_DATA (character files) to ACCOUNT_DATA/ACCOUNT_CHARACTER (account files).
 */

/* Check if a character needs auth data migration */
bool needs_auth_migration(CHAR_DATA *ch);

/* Report auth status for a character (for admin commands) */
void report_auth_status(CHAR_DATA *ch, CHAR_DATA *viewer);

/* Get human-readable auth source name */
const char *get_auth_source_name(auth_source_t source);

#endif /* AUTH_MIGRATE_H */

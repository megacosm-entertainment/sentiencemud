# Game Settings Changeset System

## Overview

The changeset system provides a complete audit trail and rollback capability for game settings changes. Changesets are **historical records only** - they are NOT automatically applied on server boot.

## How It Works

### Changesets are History, Not Pending Changes

**Important**: Changesets are stored for audit and rollback purposes. They do not represent "pending changes to be applied."

When the server boots:
1. ✅ Game settings are loaded from `game_settings.json`
2. ✅ Environment variable overrides are applied
3. ✅ Changeset history is loaded for reference (NOT applied)

The actual settings values come from the JSON file and environment variables - never from changesets.

### What Gets Logged

Every time settings are modified via `gameedit confirm`, a new changeset is created with:
- **Unique ID**: Sequential changeset number
- **Author**: Character who made the changes
- **Timestamp**: When the changes were confirmed
- **Comment**: Optional description of why changes were made
- **Change List**: For each setting that changed:
  - Setting name
  - Old value
  - New value

### Storage

**File**: `data/system/changesets.dat`

**Format**:
```
#CHANGESETS <count> <next_id>
#CHANGESET
Id 1
Author Tieryo~
Timestamp 1748015085
Comment Enabling locker system~
Change lockers_enabled false~ true~
Change max_locker_items 0~ 30~
#END
#CHANGESET
Id 2
...
#END
#END
```

### Retention

- Stores the last **20 changesets** (`MAX_CHANGESETS`)
- Older changesets are automatically removed when the limit is reached
- This provides a rolling audit trail

## Changeset Lifecycle

### 1. Making Changes

```
gameedit set email_host "smtp.newhost.com"
gameedit set email_port 587
```

These changes are **pending** - stored in memory but not applied or saved.

### 2. Confirming Changes

```
gameedit confirm
```

This:
1. ✅ Applies the changes to the running server
2. ✅ Saves changes to `game_settings.json`
3. ✅ Creates a new changeset in history
4. ✅ Saves the changeset to `changesets.dat`

### 3. Viewing History

```
gameedit history
```

Shows the last 20 changesets with their IDs, authors, timestamps, and number of changes.

### 4. Viewing Details

```
gameedit view 5
```

Shows complete details of changeset #5, including all settings that were changed and their old/new values.

### 5. Rolling Back

```
gameedit rollback 5 confirm
```

This:
1. ✅ Reverts ALL settings to their values from before changeset #5
2. ✅ Creates a NEW changeset recording the rollback
3. ✅ Saves the reverted settings to `game_settings.json`

## Boot Sequence Logs

You may see these messages on server boot:

```
Loading changeset history (for rollback/audit purposes)...
Found 13 changesets in history, next ID: 14
Changeset history loaded: 13 records available for rollback/audit.
 - Changeset #1 by Tieryo with 3 changes
 - Changeset #2 by Tieryo with 2 changes
 ...
```

**This is normal!** The server is loading the history for reference. It is **NOT applying** these changes - they've already been applied when they were originally confirmed.

## Use Cases

### Audit Trail

**Scenario**: You need to know who changed the email configuration and when.

**Solution**:
```
gameedit history

Shows:
#13 | 2025-01-06 10:30 | Tieryo    | Updated email settings      | 4 changes

gameedit view 13

Shows:
+------------------------------------------------------------------------------+
| Changeset #13 by Tieryo on 2025-01-06 10:30:15                              |
+------------------------------------------------------------------------------+
| Comment: Updated email settings for new mail server                         |
+------------------------------------------------------------------------------+
| Changes:                                                                     |
|   email_host         : smtp.oldhost.com → smtp.newhost.com                  |
|   email_port         : 465 → 587                                            |
|   email_username     : ***** → *****                                        |
|   email_password     : ***** → *****                                        |
+------------------------------------------------------------------------------+
```

### Rollback After Bad Change

**Scenario**: You changed several settings, server behavior became unstable.

**Solution**:
```
# View recent changes
gameedit history

# Identify the problematic changeset (e.g., #15)
gameedit view 15

# Rollback to before that changeset
gameedit rollback 15 confirm

# This reverts ALL settings to their state before changeset #15
# A new changeset (#16) is created documenting the rollback
```

### Compliance & Documentation

**Scenario**: Security audit requires documentation of all configuration changes.

**Solution**: Export changeset history for the audit period:
```
gameedit history 20  # Show all stored changesets

# Each changeset shows:
# - Who made the change (author)
# - When it was made (timestamp)
# - Why it was made (comment)
# - Exactly what changed (old → new values)
```

## Best Practices

### 1. Always Add Comments

When making significant changes, add a comment:
```
gameedit set telnet_port 9200
gameedit set tls_port 9201
gameedit confirm

# You'll be prompted for a comment
Enter comment: Changing ports to avoid conflict with new service
```

Or use the comment command:
```
gameedit comment 15
# Opens editor to add/edit the comment for changeset #15
```

### 2. Review Before Confirming

```
gameedit pending
```

Shows exactly what will be changed before you confirm.

### 3. Use Rollback Carefully

Rollback reverts **all settings** to their state before a specific changeset. If you only need to revert one setting:

```
# Don't rollback, just change it back manually
gameedit show email_host   # See current value
gameedit view 13           # See what it was before
gameedit set email_host "smtp.oldhost.com"
gameedit confirm
```

### 4. Document Why, Not Just What

Good comments:
- ✅ "Increasing max_characters to support larger player base"
- ✅ "Disabling wizlock after maintenance completed"
- ✅ "Changing ports to avoid conflict with monitoring service"

Bad comments:
- ❌ "Changed settings"
- ❌ "Update"
- ❌ "No comment provided"

## Troubleshooting

### "Why are changesets loading on boot?"

**Answer**: This is normal and expected. The server loads the changeset **history** for audit/rollback purposes. It does NOT apply these changes - they were already applied when originally confirmed.

Think of it like loading a git history - you're loading the log, not re-applying commits.

### "I rolled back but some settings didn't change"

**Cause**: The settings might be overridden by environment variables.

**Solution**: Check for environment variable overrides:
```
gameedit show <setting>

If you see:
Value Source       | Environment Variable

The setting is overridden by an environment variable and won't be affected by rollback.
```

### "My changeset history is missing"

**Cause**: The `changesets.dat` file might be missing or corrupted.

**Impact**: You lose the audit trail, but settings are not affected (they're in `game_settings.json`).

**Prevention**: Regular backups of `data/system/changesets.dat` if audit trail is important.

## Relationship to Environment Variables

Changesets record changes to the **configuration file** (`game_settings.json`).

Environment variable overrides:
- ✅ Take precedence over the config file
- ✅ Are NOT recorded in changesets
- ❌ Cannot be rolled back (they're external to the server)
- ✅ Are clearly marked with `E` flag in `gameedit show`

If you modify a setting that's overridden by an environment variable:
1. The change is saved to the config file
2. A changeset is created
3. The change won't take effect (env var overrides it)
4. You'll see a warning when making the change

## Technical Details

### Changeset Structure

```c
typedef struct game_settings_changeset {
    int id;                    // Sequential ID
    char *author;              // Who made the change
    time_t timestamp;          // When it was made
    char *comment;             // Why it was made
    LLIST *changes;            // List of GAME_SETTING_CHANGE_HISTORY
} GAME_SETTINGS_CHANGESET;

typedef struct game_setting_change_history {
    const struct game_setting_type *setting;  // Which setting
    char *old_value;                          // Previous value
    char *new_value;                          // New value
} GAME_SETTING_CHANGE_HISTORY;
```

### Storage Limit

The system stores a maximum of 20 changesets (`MAX_CHANGESETS`). When the 21st changeset is created, the oldest is removed.

This provides a balance between:
- Having recent history for troubleshooting
- Not consuming excessive disk space
- Keeping the audit trail manageable

If you need longer-term audit history, back up `changesets.dat` periodically.

### Implementation Files

- [gameedit.c](src/editors/game_settings/gameedit.c) - Changeset creation, storage, retrieval, and rollback
- `data/system/changesets.dat` - Persistent storage of changeset history

## Summary

**Changesets are for history and rollback, not for applying changes on boot.**

- ✅ Created when you confirm changes
- ✅ Loaded on boot for reference
- ✅ Used for audit trail
- ✅ Used for rollback
- ❌ NOT applied on boot (settings come from JSON + env vars)
- ❌ NOT "pending changes" waiting to be applied

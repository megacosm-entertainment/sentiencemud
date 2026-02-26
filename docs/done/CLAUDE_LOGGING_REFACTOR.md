# Claude Logging Refactor Guide

This document outlines the process of refactoring the MUD's logging system from the legacy `log_string`, `log_stringf`, and `bug` functions to the new `zlog`-based functions.

## Current Status

**Completed files:**
- `db.c` - Converted and contains the old wrapper functions for backward compatibility
- `comm.c` - Fully converted
- `handler.c` - Fully converted
- `nanny.c` - Fully converted
- `account/otp.c` - Fixed (was partially done by Gemini with wrong signature)
- `account/account_notes.c` - Fixed
- `act_comm.c` - Fixed
- `olc.c` - Fixed
- `script_commands.c` - Fixed

**Build configuration fixed:**
- Added `log.c` to CMakeLists.txt
- Added `json_race.c` to CMakeLists.txt
- Added `zlog` to target_link_libraries

## New Function Signatures

```c
// For simple messages
void log_message(log_level level, const char *category, const char *message);

// For formatted messages (printf-style)
void log_message_f(log_level level, const char *category, const char *format, ...);
```

### Log Levels (from log.h)
```c
LOG_LEVEL_INFO      // General informational messages
LOG_LEVEL_WARN      // Warnings
LOG_LEVEL_ERROR     // Errors
LOG_LEVEL_DEBUG     // Debug messages
LOG_LEVEL_CRITICAL  // Critical errors
LOG_LEVEL_BUG       // Bug reports (maps to fatal internally)
```

### Log Categories (from log.h)
```c
LOG_INIT      "init"      // Initialization messages
LOG_INFO      "info"      // General informational messages
LOG_WARN      "warn"      // Warnings
LOG_ERROR     "error"     // Errors
LOG_DEBUG     "debug"     // Debug messages
LOG_CRITICAL  "critical"  // Critical errors
LOG_SQL       "sql"       // SQL queries
LOG_HTTP      "http"      // HTTP requests
LOG_SCRIPT    "script"    // Scripting messages
LOG_SECURITY  "security"  // Security-related messages
```

## Replacement Patterns

### Pattern 1: `log_string(message)`

**Before:**
```c
log_string("Something happened.");
```

**After:**
```c
log_message(LOG_LEVEL_INFO, LOG_INFO, "Something happened.");
```

### Pattern 2: `sprintf + log_string`

**Before:**
```c
sprintf(buf, "Player %s connected from %s", name, host);
log_string(buf);
```

**After:**
```c
log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Player %s connected from %s", name, host);
```

### Pattern 3: `log_stringf(format, args)`

**Before:**
```c
log_stringf("Loaded %d areas.", count);
```

**After:**
```c
log_message_f(LOG_LEVEL_INFO, LOG_INFO, "Loaded %d areas.", count);
```

### Pattern 4: `bug(message, 0)` or `bug(format, args)`

**Before:**
```c
bug("Something went wrong!", 0);
bug("Invalid value: %d", value);
```

**After:**
```c
log_message(LOG_LEVEL_BUG, LOG_ERROR, "Something went wrong!");
log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Invalid value: %d", value);
```

### Pattern 5: `sprintf + bug`

**Before:**
```c
sprintf(buf, "Failed to load %s", filename);
bug(buf, 0);
```

**After:**
```c
log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "Failed to load %s", filename);
```

## Category Selection Guide

Choose categories based on context:

| Context | Level | Category |
|---------|-------|----------|
| Server startup/shutdown | LOG_LEVEL_INFO | LOG_INIT |
| Player login/logout | LOG_LEVEL_INFO | LOG_INFO or LOG_SECURITY |
| Bad password attempts | LOG_LEVEL_WARN | LOG_SECURITY |
| Connection errors | LOG_LEVEL_ERROR | LOG_ERROR |
| NULL pointer bugs | LOG_LEVEL_BUG | LOG_ERROR |
| Script errors | LOG_LEVEL_ERROR | LOG_SCRIPT |
| Database issues | LOG_LEVEL_ERROR | LOG_SQL |
| Debug tracing | LOG_LEVEL_DEBUG | LOG_DEBUG |
| Critical failures | LOG_LEVEL_CRITICAL | LOG_CRITICAL |

## Manual Refactoring Strategy

### Step 1: Find files with old logging calls

```bash
cd /sentience/src
grep -l 'log_string\|log_stringf\|bug(' *.c | wc -l
```

### Step 2: Get counts per file

```bash
grep -c 'log_string\|log_stringf\|bug(' *.c | grep -v ':0$' | sort -t: -k2 -nr
```

### Step 3: Work through files one at a time

For each file:

1. **Open the file** in your editor

2. **Search and replace** using regex patterns:

   For `log_string("...")`  simple strings:
   - Find: `log_string\("([^"]+)"\);`
   - Think about the appropriate level/category
   - Replace with: `log_message(LOG_LEVEL_XXX, LOG_XXX, "$1");`

   For `sprintf + log_string` patterns:
   - Find the sprintf, grab the format and args
   - Delete the sprintf line
   - Replace log_string with log_message_f using the format/args

   For `bug("...", 0)` patterns:
   - Find: `bug\s*\("([^"]+)",\s*0\);`
   - Replace with: `log_message(LOG_LEVEL_BUG, LOG_ERROR, "$1");`

   For `bug(format, args)` patterns:
   - Find: `bug\s*\("([^"]+)",\s*(.+)\);`
   - Replace with: `log_message_f(LOG_LEVEL_BUG, LOG_ERROR, "$1", $2);`

3. **Build and test**:
   ```bash
   cmake --build build -j12
   ```

4. **Commit** after each file:
   ```bash
   git add <file>
   git commit -m "Refactor logging in <file> to use zlog"
   ```

### Step 4: Remaining files to convert

**Script files (highest count - mostly bug() calls):**
- `script_mpcmds.c` - 316 calls
- `script_tpcmds.c` - 300 calls
- `script_opcmds.c` - 282 calls
- `script_rpcmds.c` - 266 calls
- `scripts.c` - 214 calls
- `script_commands.c` - 80 calls
- `script_comp.c` - 22 calls
- `script_expand.c` - 10 calls

**Save/Load files:**
- `save.c` - 118 calls
- `olc_save.c` - 56 calls

**Database/JSON files:**
- `db.c` - 36 calls (contains wrapper functions - convert last)
- `db2.c` - 28 calls
- `json_game_settings.c` - 25 calls
- `json_char.c` - 19 calls
- `json_race.c` - 10 calls
- `json_account.c` - 8 calls

**Connection/Network files:**
- `redis_cache.c` - 45 calls
- `connection_websocket.c` - 32 calls
- `async_cache.c` - 18 calls
- `tls.c` - 14 calls
- `connection_tls.c` - 12 calls
- `protocol_websocket.c` - 6 calls

**Gameplay files:**
- `church.c` - 52 calls
- `update.c` - 36 calls
- `act_wiz.c` - 30 calls
- `fight.c` - 20 calls
- `act_obj.c` - 18 calls
- `blueprint.c` - 14 calls
- `act_comm.c` - 13 calls
- `dungeon.c` - 12 calls
- `chat_rooms.c` - 12 calls
- `quest.c` - 10 calls
- `act_move.c` - 10 calls
- `interp.c` - 9 calls
- `skills.c` - 8 calls
- `project.c` - 8 calls
- `mail.c` - 8 calls
- `boat.c` - 8 calls
- `staff.c` - 6 calls
- `weather.c` - 6 calls

**Lower priority (< 6 calls):**
- Various other files

## Common Pitfalls

1. **Don't use `__FILE__` and `__func__`** - The current implementation uses category strings, not file/function names.

2. **Watch for escaped quotes** - Messages with `\"` need careful handling.

3. **Remove unused `buf` variables** - After converting `sprintf+log_string` to `log_message_f`, the buffer variable may become unused.

4. **Check for commented code** - Don't convert logging calls that are already commented out.

5. **Build after each file** - Catch errors early.

## Final Cleanup

After all files are converted:

1. Remove the wrapper functions from `db.c`:
   - `log_string()`
   - `log_stringf()`
   - `bug()`

2. Remove declarations from `merc.h`:
   - `void bug(const char *str, ...);`
   - `void log_string(const char *str);`
   - `void log_stringf(const char *fmt, ...);`

3. Update the `__D__` macro if still needed (currently in merc.h)

## Testing

After converting a batch of files:

1. Build: `cmake --build build -j12`
2. Run the server and check logs appear correctly
3. Test functionality that uses the converted logging

---

Last updated: 2026-01-22

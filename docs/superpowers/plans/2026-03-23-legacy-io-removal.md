# Legacy I/O Format Removal Implementation Plan

> **STATUS: COMPLETE (2026-03-23)** — All 6 tasks executed. ~7,142 lines removed across 23 files.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove all legacy .dat/.are/.pfile format read/write code, leaving JSON as the sole persistence format — clearing the path for an optional PostgreSQL storage backend.

**Architecture:** Phased removal in dependency order: dead code first, then reader functions and their callers, then I/O utilities once all callers are gone. Each phase is independently committable and testable. All data migration is complete in production.

**Tech Stack:** C, Jansson (JSON), CMake + Make (dual build system), Redis (cache layer)

**Scope:** Player files, account files, area/zone files, persist data, and ancillary data files (bans, help, notes, mail). Excludes subsystem-internal formats (boat.c, church.c, blueprint.c, dungeon.c, wilds.c) which use fread_* utilities for their own data — those are separate future migrations.

**Estimated removal:** ~5,500+ lines across ~15 files, plus deletion of pfile_migrate.c (~800 lines).

---

## Current State (Verified 2026-03-23)

**Migration status:** Production has been fully migrated to JSON. The legacy files
on this dev environment are leftover test artifacts from the migration process and
are NOT needed. All legacy player/account files can be deleted. All .are zones have
JSON counterparts (including the 5 that appeared .are-only — they exist in prod).

### Data on Disk (Dev Environment — Stale)
| Data Type | Legacy Files | JSON Files | Action |
|-----------|-------------|------------|--------|
| Accounts | ~14 old format | All exist as JSON | Delete legacy files |
| Characters | Many old format | All exist as JSON | Delete legacy files |
| Areas | 90 .are files | All have .json counterparts | Delete .are files |
| Persist | None (.dat gone) | YES (subdirs) | Already clean |
| Bans | Unknown | JSON path exists | Verify |
| Help | .help files | JSON path exists | Verify |
| Notes | Unknown | JSON preferred | Verify |
| Mail | Unknown | JSON preferred | Verify |

### Code Paths (All save paths are JSON-only; legacy code is read-only fallback)
| System | Save Format | Load Primary | Load Fallback |
|--------|------------|-------------|---------------|
| Characters | JSON only | JSON (Redis → disk) | fread_char() via ENABLE_LEGACY_PFILE_READ |
| Accounts | JSON only | JSON | fread_account() |
| Areas | JSON only | json_area_load() | read_area_new() via ENABLE_LEGACY_AREA_READ |
| Persist | JSON only | json_persist_load_all() | persist_load() .dat path |
| Notes | JSON only | json_load_notes() | fread_* fallback |
| Mail | JSON only | load_mail_json() | fread_* fallback |
| Bans | JSON only | json_load_bans() | fread_* fallback |
| Help | JSON only | JSON deserialize | fread_* fallback |

---

## File Map

### Files to Modify
| File | Changes |
|------|---------|
| `save.c` | Remove fwrite_account (6673-6764), fwrite_account_character (6769-6865), fread_char (1581-3138), fread_account (6374-6540), fread_account_character (6542-6668), fallback paths in load_char_obj_internal and load_account, forward declarations, do_migratefiles (~7914), dead `migrate_character_objects` forward decl (~line 150) |
| `db.c` | Remove ENABLE_LEGACY_AREA_READ define (94-97), area boot fallback (1413-1452), all persist_load_* functions (9292-11031), persist .dat fallback path (11092-11159), fread_* utilities (6057-6620) — last phase only |
| `olc_save.c` | Remove read_area_new (1399-1732), dead .are writer (457-578) |
| `olc_save.h` | Remove `read_area_new` declaration (~line 30) |
| `act_wiz.c` | Remove legacy area reload caller (~line 12446) |
| `tables.c` | Remove `do_migrate` entry (~line 3605), `do_migratefiles` entry (~line 3500) |
| `merc.h` | Remove `do_migrate` declaration (~line 11390), `do_migratefiles` declaration |
| `note.c` | Remove legacy .dat reader fallback path |
| `mail.c` | Remove legacy .dat reader fallback path |
| `ban.c` | Remove legacy .dat reader fallback path |
| `help.c` | Remove legacy .dat reader fallback path |
| `CMakeLists.txt` | Remove pfile_migrate.c entry |
| `Makefile` | Remove pfile_migrate.c entry |

### Files to Delete
| File | Reason |
|------|--------|
| `pfile_migrate.c` | One-time migration utility, no longer needed after conversion |
| `pfile_migrate.h` | Header for above |

### Files NOT Changed (Out of Scope)
- boat.c, church.c, blueprint.c, dungeon.c, wilds.c, db2.c, reputation.c — use fread_* for their own data formats, not legacy player/account/area persistence. Separate future migrations.
- `legacy_obj_index_value_get/set` (olc_save.c:65-72) — used in 36 places across db.c, handler.c, save.c, olc_save.c. NOT legacy I/O; these are material slot compat helpers. Keep.
- `fread_obj_new` (save.c:3481) — still called by church.c, mail.c, and `read_permanent_objs`. Separate migration.
- `OLD_LEVEL_*` constants (save.c:132-137) — still used for staff rank migration at save.c:5269-5276. Keep.

---

## Task 1: Remove Dead Code (Write Functions Never Called)

**Files:**
- Modify: `src/save.c` (lines 6673-6865 — fwrite_account, fwrite_account_character)
- Modify: `src/olc_save.c` (lines 457-578 — unreachable .are format writer)

These functions have ZERO callers anywhere in the codebase. The .are writer is unreachable due to an early return at line 452 (`use_json` is hardcoded `true`).

- [x] **Step 1: Verify zero callers**

```bash
cd /sentience/src
grep -rn '\bfwrite_account\b' *.c editors/**/*.c | grep -v '^save.c'
grep -rn '\bfwrite_account_character\b' *.c editors/**/*.c | grep -v '^save.c'
```

Expected: No output (zero external callers).

- [x] **Step 2: Remove fwrite_account() from save.c**

Delete the function at lines 6673-6764 and its forward declaration (search for `fwrite_account` near the top of the file, around line 160-170).

- [x] **Step 3: Remove fwrite_account_character() from save.c**

Delete the function at lines 6769-6865 and its forward declaration.

- [x] **Step 4: Remove dead .are format writer from olc_save.c**

In `save_area_new()`, remove the unreachable legacy .are writer code block after the JSON save path returns (lines ~457-578). Keep the JSON save path intact.

- [x] **Step 5: Build and test**

```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test
```

Expected: Clean build, all tests pass.

- [x] **Step 6: Commit**

```bash
cd /sentience/src
git add save.c olc_save.c
git commit -m "refactor: remove dead legacy write functions

Remove fwrite_account(), fwrite_account_character() (zero callers),
and unreachable .are format writer in save_area_new().

~400 lines of dead code removed.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 2: Remove Legacy Player & Account Readers

**Files:**
- Modify: `src/save.c` — Remove fread_char (~1581-3138), fread_account (~6374-6540), fread_account_character (~6542-6668), do_migratefiles (~7914), fallback paths, forward declarations, ENABLE_LEGACY_PFILE_READ, dead `migrate_character_objects` forward decl (~line 150)
- Delete: `src/pfile_migrate.c`
- Delete: `src/pfile_migrate.h`
- Modify: `src/tables.c` — Remove `do_migrate` (~line 3605) and `do_migratefiles` (~line 3500) command table entries
- Modify: `src/merc.h` — Remove `do_migrate` (~line 11390) and `do_migratefiles` declarations
- Modify: `src/CMakeLists.txt` — Remove pfile_migrate.c
- Modify: `src/Makefile` — Remove pfile_migrate.c

- [x] **Step 1: Identify all legacy pfile conditional blocks**

```bash
cd /sentience/src
grep -n 'ENABLE_LEGACY_PFILE_READ\|fread_char\|fread_account\b\|fread_account_character' save.c
```

Map all the code blocks that need removal.

- [x] **Step 2: Remove ENABLE_LEGACY_PFILE_READ and its conditional blocks in save.c**

Find the `#ifndef ENABLE_LEGACY_PFILE_READ` / `#define` block and remove it. Then find all `#if ENABLE_LEGACY_PFILE_READ` blocks and remove the legacy-path code within them. Keep the JSON-path code.

In `load_char_obj_internal()`: remove the entire `else if` fallback block (~lines 1261-1291) that calls `fread_char()`. Keep only the JSON/Redis load path.

In `load_account()`: remove the `else` fallback block (~lines 6230-6260) that calls `fread_account()` and `fread_account_character()`. Keep only the JSON path.

In `find_account_by_id()`: remove the legacy scan path (~line 7605) that calls `fread_account()`.

- [x] **Step 3: Remove fread_char() function body**

Delete the `fread_char` function (~lines 1581-3138) and its forward declaration (~line 139).

- [x] **Step 4: Remove fread_account() and fread_account_character() function bodies**

Delete `fread_account` (~6374-6540) and `fread_account_character` (~6542-6668) and their forward declarations (~lines 169-170).

**Note:** Line numbers will have shifted after Step 3. Use function name search, not line numbers.

- [x] **Step 5: Remove do_migratefiles() from save.c**

Delete the `do_migratefiles` function (~line 7914) and its forward declaration. This command calls `load_account()` which can no longer read old formats, making it dead code.

- [x] **Step 6: Remove dead forward declaration migrate_character_objects**

Delete the `migrate_character_objects` forward declaration (~line 150 in save.c). No implementation exists — it's dead.

- [x] **Step 7: Delete pfile_migrate.c, pfile_migrate.h and remove from build files**

```bash
cd /sentience/src
rm pfile_migrate.c pfile_migrate.h
```

Remove `pfile_migrate.c` from both `CMakeLists.txt` (~line 268) and `Makefile` (~line 251).

Search for and remove any remaining header declarations or extern references:

```bash
grep -rn 'pfile_migrate\|migrate_all_players\|migrate_all_accounts\|do_migrate\b' *.h *.c
```

- [x] **Step 8: Remove command table entries from tables.c and merc.h**

In `tables.c`: remove `{ "do_migrate", do_migrate }` (~line 3605) and `{ "do_migratefiles", do_migratefiles }` (~line 3500).

In `merc.h`: remove `void do_migrate(CHAR_DATA *ch, char *argument);` (~line 11390) and the `do_migratefiles` declaration.

**Without this step, the build will fail with linker errors.**

- [x] **Step 9: Build and test**

```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test
```

Expected: Clean build, all tests pass.

- [x] **Step 10: Commit**

```bash
cd /sentience/src
git add -A
git commit -m "refactor: remove legacy pfile/account readers and migration utility

Remove fread_char(), fread_account(), fread_account_character(),
ENABLE_LEGACY_PFILE_READ conditional blocks, do_migratefiles(),
do_migrate command, pfile_migrate.c/h, and associated table entries.
JSON is now the sole persistence format for players and accounts.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 3: Remove Legacy .are Area Reader

**Files:**
- Modify: `src/db.c` — Remove ENABLE_LEGACY_AREA_READ (~94-97), area boot fallback (~1413-1452)
- Modify: `src/olc_save.c` — Remove read_area_new (~1399-1732)
- Modify: `src/olc_save.h` — Remove `read_area_new` declaration (~line 30)
- Modify: `src/act_wiz.c` — Remove legacy area reload path (~line 12446)

**Note:** Do NOT remove `legacy_obj_index_value_get/set` (olc_save.c:65-72) — used in 36 places across 4 files. Not legacy I/O.

- [x] **Step 1: Remove ENABLE_LEGACY_AREA_READ from db.c**

Delete the `#ifndef ENABLE_LEGACY_AREA_READ` / `#define` block (~lines 94-97).

Remove the entire `#if ENABLE_LEGACY_AREA_READ` / `#else` / `#endif` block in the boot loop (~lines 1413-1452). Keep the error handling for zones that fail JSON load — change it from "fall back to .are" to "log error and skip/abort".

- [x] **Step 2: Remove read_area_new() from olc_save.c**

Delete the `read_area_new()` function (~lines 1399-1732, ~333 lines).

- [x] **Step 3: Remove read_area_new declaration from olc_save.h**

Delete `AREA_DATA *read_area_new( FILE *fp );` at ~line 30 of olc_save.h.

**Keep adjacent declarations** (`read_room_new`, `read_script_new`, etc.) — they are called by wilds.c, blueprint.c, dungeon.c.

- [x] **Step 4: Remove legacy area reload from act_wiz.c**

Find the caller at ~line 12446 in act_wiz.c that uses `read_area_new()`. Replace with JSON-only reload or remove the legacy path. Verify the JSON-only area reload path works for the builder `areload` command.

```bash
grep -n 'read_area_new' /sentience/src/act_wiz.c
```

- [x] **Step 5: Clean up area.lst if needed**

Check if area.lst references any .are extensions explicitly. If entries are stem-only (e.g., `limbo` not `limbo.are`), no change needed.

- [x] **Step 6: Build and test**

```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test
```

Expected: Clean build, all tests pass.

- [x] **Step 7: Commit**

```bash
cd /sentience/src
git add -A
git commit -m "refactor: remove legacy .are area format reader

Remove read_area_new(), ENABLE_LEGACY_AREA_READ, area boot fallback,
and act_wiz legacy reload path.
JSON is now the sole area persistence format.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 4: Remove Persist.dat Legacy Reader

**Files:**
- Modify: `src/db.c` — Remove persist_load_token (~9292-9375), persist_load_object (~9378-9958), persist_load_mobile (~9959-10401), persist_load_exit (~10402-10580), persist_load_room (~10581-11031), persist .dat fallback path in persist_load (~11092-11159)

- [x] **Step 1: Remove persist .dat fallback path in persist_load()**

In the `persist_load()` function (~lines 11033-11178), remove the fallback code that reads `persist.dat` (~lines 11092-11159). Keep the JSON path (`json_persist_load_all()`) and error handling.

The function should now:
1. Try `json_persist_load_all()`
2. If it fails, log error and return false
3. No .dat fallback

- [x] **Step 2: Remove all persist_load_* helper functions**

Delete these functions (in reverse order to avoid line number shifts):
- `persist_load_room()` (~10581-11031) — 451 lines
- `persist_load_exit()` (~10402-10580) — 178 lines
- `persist_load_mobile()` (~9959-10401) — 443 lines
- `persist_load_object()` (~9378-9958) — 580 lines
- `persist_load_token()` (~9292-9375) — 83 lines

Also remove any forward declarations for these functions.

Total: ~1,735 lines.

- [x] **Step 3: Simplify json_persist_needs_migration()**

Check what `json_persist_needs_migration()` does (db.c:11056 calls it). If it only checks for `persist.dat` existence, it can be simplified to always return false, or removed:

```bash
grep -rn 'json_persist_needs_migration' /sentience/src/*.c /sentience/src/io/**/*.c
```

- [x] **Step 4: Build and test**

```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test
```

Expected: Clean build, all tests pass.

- [x] **Step 5: Commit**

```bash
cd /sentience/src
git add -A
git commit -m "refactor: remove legacy persist.dat reader

Remove persist_load_token/object/mobile/exit/room and .dat fallback
path from persist_load(). JSON is now the sole persist format.

~1,735 lines removed.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 5: Remove Ancillary Data File Legacy Readers

**Files:**
- Modify: `src/note.c` — Remove legacy .dat reader fallback
- Modify: `src/mail.c` — Remove legacy .dat reader fallback
- Modify: `src/ban.c` — Remove legacy .dat reader fallback
- Modify: `src/help.c` — Remove legacy .dat/.help reader fallback

All four files follow the same pattern: JSON is the preferred/only save path; load tries JSON first, falls back to legacy fread_*. Remove the fallback.

- [x] **Step 1: Remove legacy reader from note.c**

In `load_notes()` (~line 725+), remove the fallback path that uses fread_word/fread_string/fread_number (~lines 774-836). Keep only the `json_load_notes()` call.

- [x] **Step 2: Remove legacy reader from mail.c**

In the mail load function, remove the fread_* fallback path. Keep only `load_mail_json()`.

- [x] **Step 3: Remove legacy reader from ban.c**

Remove the fread_word/fread_number/fread_to_eol fallback (~lines 96-99). Keep only `json_load_bans()`.

- [x] **Step 4: Remove legacy reader from help.c**

Remove the extensive fread_* help/category parser (~lines 879-1103, ~224 lines). Keep only the JSON deserialization path.

- [x] **Step 5: Build and test**

```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test
```

Expected: Clean build, all tests pass.

- [x] **Step 6: Commit**

```bash
cd /sentience/src
git add note.c mail.c ban.c help.c
git commit -m "refactor: remove legacy data file readers (notes, mail, bans, help)

Remove fread_*-based fallback readers from note.c, mail.c, ban.c,
and help.c. JSON is now the sole data file format.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 6: Remove fread_* I/O Utility Functions (Final Cleanup)

**Files:**
- Modify: `src/db.c` — Remove fread_letter (6057-6069), fread_number (6105-6151), fread_flag (6154-6199), fread_string (6232-6358), fread_string_eol (6359-6456), fread_to_eol (6547-6565), fread_word (6571-6620), fwrite_flag (6072-6099)
- Possibly keep: fread_string_new, fread_string_eol_new, fread_string_len (used by modern code)

**Prerequisite:** ALL legacy readers removed (Tasks 1-5 complete).

**⚠️ CRITICAL CHECK:** These functions are also used by subsystems NOT in this plan's scope:
- boat.c (~91 calls), church.c (~82), blueprint.c (~70), wilds.c (~58), dungeon.c (~52), db2.c (~57), reputation.c (~20)
- `fread_obj_new` in save.c is a SEPARATE function (not in db.c) — still called by church.c and `read_permanent_objs`. It survives this plan.

- [x] **Step 1: Audit remaining callers**

```bash
cd /sentience/src
# After all previous tasks, check who still calls fread_*
for fn in fread_letter fread_number fread_flag fread_string fread_string_eol \
          fread_to_eol fread_word fwrite_flag; do
    echo "=== $fn ==="
    grep -rn "\b${fn}\b" *.c editors/**/*.c | grep -v '^db.c:' | grep -v '^\s*//'
done
```

- [x] **Step 2: Decision point**

**If zero remaining callers:** Remove all fread_* and fwrite_flag functions from db.c (~500 lines). Remove any declarations from merc.h or db.h.

**If callers remain in other subsystems (expected):** Do NOT remove yet. Document remaining callers and create follow-up TODO. The functions stay until boat.c, church.c, etc. are migrated to JSON in future plans.

- [x] **Step 3: If removing — build and test**

```bash
cd /sentience/src && ./build tests && cd /sentience && ./sent -test
```

- [x] **Step 4: If removing — commit**

```bash
cd /sentience/src
git add db.c merc.h
git commit -m "refactor: remove legacy fread_*/fwrite_flag I/O utilities

All callers removed. These DikuMUD-era file I/O primitives are no
longer needed.

~500 lines removed.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

- [x] **Step 4b: If NOT removing — document and commit TODO**

Create or update `src/docs/TODO_LEGACY_IO_REMAINING.md` listing the remaining subsystems that still use fread_* and need their own JSON migration.

---

## Out of Scope / Future Work

These subsystems use fread_* for their own data formats and need separate migration plans:

| File | fread_* Calls | Data Type |
|------|--------------|-----------|
| boat.c | 91 | Ship/boat definitions and state |
| church.c | 82 | Church/religious organization data |
| blueprint.c | 70 | Dungeon blueprint definitions |
| wilds.c | 55 | Wilderness system data |
| dungeon.c | 52 | Dungeon instance data |
| db2.c | 53 | Secondary database loading (socials, etc.) |
| reputation.c | 17 | Reputation system data |

Each will need its own JSON migration (similar pattern: add JSON reader, make it primary, deprecate fread_* path, remove).

---

## Rollback Strategy

Each task produces an independent commit. If issues are discovered:
1. `git revert <commit>` for the specific phase
2. Restore data backups if needed (created in migration steps)
3. Legacy readers are self-contained — re-adding is straightforward

## Verification Checklist (Run After All Tasks)

```bash
# 1. Build clean (no warnings from removed code)
cd /sentience/src && ./build tests 2>&1 | grep -i 'error\|warning'

# 2. All tests pass
cd /sentience && ./sent -test

# 3. No references to removed functions remain
cd /sentience/src
grep -rn 'fread_char\b\|fwrite_account\b\|read_area_new\b\|persist_load_token\b' *.c *.h

# 4. No legacy data files remain
find /sentience/accounts/ -type f -not -name "*.json" | wc -l  # Should be 0
find /sentience/characters/ -type f -not -name "*.json" | wc -l  # Should be 0
find /sentience/area/ -name "*.are" | wc -l  # Should be 0

# 5. Game boots and runs normally
cd /sentience && ./sent  # Boot, verify no load errors, test basic gameplay
```

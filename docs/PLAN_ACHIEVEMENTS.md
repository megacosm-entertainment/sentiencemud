# Achievements System - Implementation Plan

## Context

The game needs an achievements system that supports both characters and accounts, is area-owned (identified by widevnum), integrates with the scripting engine, and provides full display features (points, announcements, titles). The design follows existing patterns from the quest v2 system, event system, and OLC framework.

**Design decisions (from user input):**
- Predefined categories (enum, not freeform tags)
- Both binary and progressive achievement types (flag on definition)
- Separate scopes: each achievement is either character-scope or account-scope
- Full display: points, announcements, title rewards, score integration

---

## Phase 1: Data Structures, Registration, and Memory

**Goal**: Foundation - structures, alloc/free, area-owned hash registration.

### New files
- `achievements.h` - All structures, defines, declarations
- `achievements.c` - Global list, registration, lookups, alloc/free

### Structures

**ACHIEVEMENT_INDEX_DATA** (area-owned definition):
- `long vnum`, `char *name`, `char *description`
- `int category` (ACHIEVE_CAT_COMBAT/EXPLORATION/CRAFTING/SOCIAL/STORY/DUNGEON/EVENT/GENERAL)
- `int scope` (ACHIEVE_SCOPE_CHARACTER / ACHIEVE_SCOPE_ACCOUNT)
- `bool progressive` + `int target_count`
- `int points` (achievement points value)
- `long flags` (ACHIEVE_FLAG_HIDDEN, REPEATABLE, SECRET)
- `char *title_reward`, `char *announce_message`
- `ACHIEVEMENT_REWARD_DATA *rewards` (linked list)
- `AREA_DATA *area`, `bool enabled`
- Hash/list pointers: `*next_hash`, `*next`

**ACHIEVEMENT_ENTRY_DATA** (tracking, on PC_DATA and ACCOUNT_DATA):
- `WNUM_LOAD achievement_wnum` (area_uid + vnum)
- `int progress`, `bool completed`
- `time_t earned_at`, `time_t updated_at`
- `*next`

**ACHIEVEMENT_REWARD_DATA** (same pattern as quest v2 rewards):
- `int reward_type` (POINTS/CURRENCY/REPUTATION/TOKEN/ITEM/SCRIPT)
- `long amount`, `WNUM_LOAD target_load`, `WNUM target_wnum`
- `char *currency`, `char *script`, `char *display_string`
- `*next`

### Modifications
- **merc.h**: Forward typedefs, add `achievement_index_hash[MAX_KEY_HASH]` to `AREA_DATA`, add `ACHIEVEMENT_ENTRY_DATA *achievements` to `PC_DATA` and `ACCOUNT_DATA`, extern for global list
- **tables.c**: Category/scope/flag tables for OLC display
- **Makefile + CMakeLists.txt**: Add new files

### Key references
- `quest.c:5351` - `quest_index_v2_register()` pattern
- `event_types.c:152` - `event_index_register()` pattern
- `merc.h:6601` - Area hash tables location

---

## Phase 2: JSON Serialization

**Goal**: Persist definitions in area files, tracking in character/account files.

### Modifications
- **json_area.c**: Add `"achievements"` array serialization/deserialization (pattern: `json_area_serialize_quest_v2` at line 1372, integration at lines 2894/3301)
- **json_char.c**: Save/load `ch->pcdata->achievements` as JSON array (pattern: `quest_history` at lines 1365/3283)
- **json_account.c**: Save/load `account->achievements` as JSON array (alongside bonuses/penalties near line 469)

### Entry JSON format
```json
{"auid": 5, "vnum": 100, "progress": 47, "completed": false, "earned_at": 0, "updated_at": 1698765432}
```

---

## Phase 3: Core Logic and Player Commands

**Goal**: Runtime award/progress/revoke, player-facing `achievements` command, score integration.

### Core functions in achievements.c
- `achievement_award(ch, ach)` - Award binary achievement (returns true if newly awarded)
- `achievement_progress(ch, ach, amount)` - Increment progressive (returns true if completed)
- `achievement_revoke(ch, ach)` - Remove achievement
- `achievement_check(ch, ach)` - Check if earned
- `achievement_get_progress(ch, ach)` - Get current progress (-1 if not found)
- `achievement_get_total_points(ch, include_account)` - Sum all earned points
- `achievement_get_count(ch, include_account)` - Count completed

All functions check `ach->scope` to target `ch->pcdata->achievements` (character) or `ch->account->achievements` (account).

On completion, `achievement_on_complete()` internally:
1. Grants rewards (dispatch by type, same pattern as quest v2 reward granting)
2. Grants title if `title_reward` is set
3. Broadcasts announcement (descriptor_list iteration, follows `event_broadcast` pattern in evtedit.c:2020)
4. Saves character/account

### Player command
- **interp.h/interp.c**: Register `do_achievements`
- **achievements.c** (or act_info.c): Implement `do_achievements`
  - `achievements` - List earned + in-progress
  - `achievements all` - List all available (respecting hidden flag)
  - `achievements <category>` - Filter by category
  - `achievements search <text>` - Search by name
  - `achievements score` - Show total points
  - Progressive display: `[####------] 47/100`
  - Hidden achievements show as `???` until earned

### Score integration
- **act_info.c**: Add achievement points line to `do_score` output

---

## Phase 4: Scripting Integration

**Goal**: Script commands and ifchecks for builder-driven achievement logic.

### Script command: ACHIEVE
Add `scriptcmd_achieve` to `script_commands.c` (pattern: `scriptcmd_quest` at line 3852):

```
ACHIEVE AWARD $MOBILE <widevnum>
ACHIEVE PROGRESS $MOBILE <widevnum> <amount>
ACHIEVE REVOKE $MOBILE <widevnum>
```

Sets `lastreturn` to 1 on success, 0 on failure.

### Register in all command tables
- **script_mpcmds.c**: `{ "achieve", scriptcmd_achieve, false, true }`
- **script_opcmds.c**: Same
- **script_rpcmds.c**: Same
- **script_tpcmds.c**: Same

### Ifchecks
Add to `script_ifc.c` (pattern: `ifc_questactive` at line 2358):
- `ifc_hasachievement(mob, widevnum)` - Returns true/false
- `ifc_achievementprogress(mob, widevnum)` - Returns int (current progress)
- `ifc_achievementcount(mob)` - Returns int (total completed)

Register in `script_const.c` ifcheck table:
```c
{ "hasachievement",      IFC_ANY, "EN", false, ifc_hasachievement,      "ifcheck hasachievement" },
{ "achievementprogress", IFC_ANY, "EN", true,  ifc_achievementprogress, "ifcheck achievementprogress" },
{ "achievementcount",    IFC_ANY, "E",  true,  ifc_achievementcount,    "ifcheck achievementcount" },
```

---

## Phase 5: OLC Editor (achedit)

**Goal**: In-game editor for creating/editing achievement definitions.

### New file
- `editors/achievements/achedit.c` (pattern: `editors/quests/qedit.c`)

### Editor commands
- `show` - Display current achievement
- `name <text>`, `description` (string editor)
- `category <combat|exploration|crafting|social|story|dungeon|event|general>`
- `scope <character|account>`
- `progressive <yes|no>`, `target <count>`
- `points <number>`
- `flags <hidden|repeatable|secret>`
- `title <string|clear>` - Title reward
- `announce <string|clear>` - Custom announcement
- `reward add|remove|list|edit` - Reward management (same pattern as qedit rewards)
- `enabled <yes|no>`

### Tabs
1. **General** - Name, description, category, scope, progressive, target, points, flags, enabled
2. **Rewards** - Reward list and configuration
3. **Notes** - Builder notes

### Modifications
- **olc.h**: `#define ED_ACHIEVEMENT`, `ACHEDIT()` macro
- **olc.c**: Register "achievement" in editor dispatch
- **Makefile + CMakeLists.txt**: Add achedit.c

---

## Phase 6: Polish, Announcements, and Tests

### Announcement broadcast
Default: `"{player} has earned the achievement: {Y{achievement}{x!"`
Custom: Uses `ach->announce_message` with variable expansion.

### Edge cases
- NPC guard: Return false immediately if `IS_NPC(ch)`
- No account guard: Return false for account-scope if `ch->account == NULL`
- Progressive overflow: Cap at `target_count`
- Repeatable: Reset progress on completion if `ACHIEVE_FLAG_REPEATABLE`
- Hidden: Skip in `achievements all` unless earned
- Orphaned entries: If definition no longer exists, display as "(Unknown Achievement)"

### Tests
- **tests/integration/achievement_tests.c** + **tests/data/integration/achievement_tests.json**
- Scenarios: binary award, progressive completion, revoke, duplicate award, scope routing, category filter, JSON roundtrip, script commands, ifchecks

---

## Dependency Graph

```
Phase 1 (structures) → Phase 2 (JSON) → Phase 3 (core + commands)
                                              ↓
                              Phase 4 (scripting) + Phase 5 (OLC)  [parallel]
                                              ↓
                                    Phase 6 (polish + tests)
```

## File Summary

**New files (5-6):**
| File | Purpose |
|------|---------|
| `achievements.h` | Structures, defines, declarations |
| `achievements.c` | Core logic, registration, award/progress/revoke, player command |
| `editors/achievements/achedit.c` | OLC editor |
| `tests/integration/achievement_tests.c` | Test handlers |
| `tests/data/integration/achievement_tests.json` | Test scenarios |

**Modified files (~18):**
| File | Changes |
|------|---------|
| `merc.h` | Typedefs, area hash, PC_DATA/ACCOUNT_DATA fields, extern |
| `olc.h` | ED_ACHIEVEMENT, ACHEDIT macro |
| `olc.c` | Editor dispatch registration |
| `interp.h` | do_achievements declaration |
| `interp.c` | Command table registration |
| `tables.c` | Category/scope/flag tables |
| `io/json/json_area.c` | Achievement definition serialization |
| `io/json/json_char.c` | Character tracking serialization |
| `io/json/json_account.c` | Account tracking serialization |
| `script_commands.c` | scriptcmd_achieve |
| `script_ifc.c` | 3 ifcheck implementations |
| `script_const.c` | Ifcheck table entries |
| `script_mpcmds.c` | Command table entry |
| `script_opcmds.c` | Command table entry |
| `script_rpcmds.c` | Command table entry |
| `script_tpcmds.c` | Command table entry |
| `act_info.c` | Score display integration |
| `Makefile` + `CMakeLists.txt` | Build file updates |

## Verification

After each phase:
1. `cd /sentience/src && ./build` - Confirm compilation
2. After Phase 2+: `./build tests && cd /sentience && ./sent -test:achievement`
3. After Phase 3+: Run game, test `achievements` command manually
4. After Phase 5: Test `achedit create "Test"` in-game, save/reload area

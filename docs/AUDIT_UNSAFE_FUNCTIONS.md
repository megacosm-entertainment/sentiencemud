# Audit: Unsafe C Function Usage

**Date:** 2026-02-26
**Scope:** All `.c` files under `/sentience/src`, excluding `.deps/` and `.build/`
**Companion doc:** [PLAN_C_HARDENING.md](PLAN_C_HARDENING.md) — remediation roadmap

---

## Executive Summary

| Category | Banned/Unsafe | Safe Alternative | Adoption % |
|----------|-------------:|----------------:|----------:|
| String formatting | 4,347 (`sprintf`) | 985 (`snprintf`) + 17 (`vsnprintf`) | 18.7% |
| String copy | 336 (`strcpy`) + 219 (`strncpy`) | 250 (`strlcpy`) | 31.1% |
| String concat | 1,147 (`strcat`) + 48 (`strncat`) | 48 (`strlcat`) | 3.9% |
| Integer parsing | 1,009 (`atoi`) + 169 (`atol`) + 20 (`sscanf`) | 8 (`strtol`) + 4 (`strtoul`) | 1.0% |
| Memory (raw vs managed) | 458 (malloc/calloc/realloc/free) | 3,660 (str_dup/free_string/alloc_perm) | 88.9% |
| Buffer system | — | 2,094 (bprintf/add_buf/new_buf) | N/A |

**Total banned function calls: ~7,300**

---

## Per-File Risk Summary (Top 60)

Files sorted by total count of banned function calls (`sprintf` + `strcpy` + `strcat` + `atoi` + `atol` + `strncpy` + `strtok` + `sscanf`).

| # | File | Total | sprintf | strcpy | strcat | atoi | atol | strncpy | strtok | sscanf |
|--:|------|------:|--------:|-------:|-------:|-----:|-----:|--------:|-------:|-------:|
| 1 | act_info.c | 728 | 502 | 29 | 184 | 9 | 3 | 0 | 1 | 0 |
| 2 | act_wiz.c | 574 | 457 | 12 | 27 | 52 | 16 | 8 | 2 | 0 |
| 3 | bit.c | 477 | 1 | 0 | 476 | 0 | 0 | 0 | 0 | 0 |
| 4 | editors/dungeons/dngedit.c | 405 | 304 | 0 | 0 | 99 | 1 | 1 | 0 | 0 |
| 5 | church.c | 311 | 243 | 9 | 30 | 23 | 5 | 1 | 0 | 0 |
| 6 | boat.c | 219 | 97 | 48 | 0 | 37 | 0 | 37 | 0 | 0 |
| 7 | act_obj.c | 207 | 175 | 1 | 20 | 3 | 5 | 3 | 0 | 0 |
| 8 | nanny.c | 174 | 152 | 4 | 13 | 3 | 0 | 2 | 0 | 0 |
| 9 | olc_act.c | 163 | 76 | 0 | 15 | 71 | 1 | 0 | 0 | 0 |
| 10 | editors/objects/oedit_types.c | 162 | 73 | 0 | 0 | 80 | 9 | 0 | 0 | 0 |
| 11 | script_comp.c | 157 | 152 | 3 | 0 | 2 | 0 | 0 | 0 | 0 |
| 12 | comm.c | 137 | 68 | 49 | 13 | 2 | 0 | 4 | 1 | 0 |
| 13 | editors/mobiles/medit.c | 129 | 22 | 30 | 2 | 68 | 4 | 3 | 0 | 0 |
| 14 | act_comm.c | 117 | 79 | 1 | 31 | 4 | 2 | 0 | 0 | 0 |
| 15 | editors/game_settings/gameedit.c | 112 | 57 | 27 | 14 | 6 | 0 | 8 | 0 | 0 |
| 16 | olc.c | 107 | 74 | 3 | 23 | 0 | 7 | 0 | 0 | 0 |
| 17 | script_commands.c | 106 | 46 | 1 | 1 | 38 | 1 | 19 | 0 | 0 |
| 18 | handler.c | 102 | 67 | 10 | 10 | 0 | 9 | 6 | 0 | 0 |
| 19 | fight.c | 102 | 73 | 0 | 29 | 0 | 0 | 0 | 0 | 0 |
| 20 | editors/events/evtedit.c | 91 | 9 | 0 | 0 | 41 | 25 | 11 | 0 | 5 |
| 21 | update.c | 90 | 72 | 0 | 18 | 0 | 0 | 0 | 0 | 0 |
| 22 | quest.c | 88 | 68 | 0 | 0 | 4 | 4 | 12 | 0 | 0 |
| 23 | editors/wilderness/wedit.c | 85 | 27 | 0 | 0 | 53 | 3 | 1 | 0 | 1 |
| 24 | protocol.c | 79 | 50 | 4 | 19 | 6 | 0 | 0 | 0 | 0 |
| 25 | editors/reserved_vnums/reserved.c | 77 | 46 | 14 | 7 | 0 | 3 | 7 | 0 | 0 |
| 26 | wilds.c | 76 | 68 | 3 | 3 | 0 | 1 | 0 | 0 | 1 |
| 27 | skills.c | 71 | 53 | 2 | 16 | 0 | 0 | 0 | 0 | 0 |
| 28 | special.c | 68 | 68 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 29 | magic_law.c | 60 | 55 | 0 | 5 | 0 | 0 | 0 | 0 | 0 |
| 30 | editors/objects/oedit.c | 60 | 12 | 2 | 1 | 43 | 1 | 1 | 0 | 0 |
| 31 | editors/common.c | 59 | 26 | 8 | 20 | 1 | 0 | 4 | 0 | 0 |
| 32 | interp.c | 54 | 45 | 7 | 0 | 2 | 0 | 0 | 0 | 0 |
| 33 | editors/blueprints/bsedit.c | 53 | 41 | 0 | 0 | 12 | 0 | 0 | 0 | 0 |
| 34 | script_vars.c | 51 | 44 | 5 | 0 | 2 | 0 | 0 | 0 | 0 |
| 35 | storage.c | 50 | 48 | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| 36 | editors/blueprints/bpedit.c | 50 | 27 | 0 | 0 | 20 | 0 | 3 | 0 | 0 |
| 37 | channels/channel_service.c | 50 | 18 | 0 | 30 | 0 | 0 | 0 | 2 | 0 |
| 38 | scripts.c | 47 | 22 | 5 | 0 | 18 | 0 | 2 | 0 | 0 |
| 39 | note.c | 47 | 20 | 1 | 16 | 3 | 0 | 7 | 0 | 0 |
| 40 | editors/quests/qedit.c | 47 | 0 | 0 | 0 | 3 | 35 | 9 | 0 | 0 |
| 41 | script_expand.c | 46 | 38 | 3 | 0 | 5 | 0 | 0 | 0 | 0 |
| 42 | editors/rooms/redit.c | 45 | 20 | 5 | 0 | 19 | 0 | 1 | 0 | 0 |
| 43 | editors/areas/aedit.c | 45 | 16 | 0 | 4 | 23 | 2 | 0 | 0 | 0 |
| 44 | olc_save.c | 44 | 31 | 1 | 3 | 6 | 0 | 3 | 0 | 0 |
| 45 | chat_rooms.c | 44 | 43 | 0 | 0 | 1 | 0 | 0 | 0 | 0 |
| 46 | account/preferences.c | 44 | 37 | 0 | 2 | 5 | 0 | 0 | 0 | 0 |
| 47 | weather.c | 43 | 11 | 0 | 20 | 12 | 0 | 0 | 0 | 0 |
| 48 | act_move.c | 43 | 39 | 2 | 0 | 2 | 0 | 0 | 0 | 0 |
| 49 | script_mpcmds.c | 40 | 6 | 1 | 0 | 26 | 0 | 7 | 0 | 0 |
| 50 | act_obj2.c | 38 | 32 | 0 | 3 | 3 | 0 | 0 | 0 | 0 |

---

## Breakdown by Function

### sprintf — 4,347 calls

The single largest category. Most are formatting player-visible output into fixed-size `char buf[MSL]` or `char buf[MIL]` buffers without length checking.

**Top 15 files:**

| File | Count | Notes |
|------|------:|-------|
| act_info.c | 502 | Player info commands (score, look, who, etc.) |
| act_wiz.c | 457 | Immortal/wizard commands |
| editors/dungeons/dngedit.c | 304 | Dungeon editor display |
| church.c | 243 | Church/organization system |
| act_obj.c | 175 | Object handling (get, drop, wear, etc.) |
| nanny.c | 152 | Login/character creation |
| script_comp.c | 152 | Script compiler error messages |
| boat.c | 97 | Ship/sailing system |
| act_comm.c | 79 | Communication commands |
| olc_act.c | 76 | OLC actions |
| olc.c | 74 | OLC framework |
| fight.c | 73 | Combat output |
| update.c | 72 | Tick/pulse updates |
| wilds.c | 68 | Wilderness system |
| special.c | 68 | Special procedures |

**Common pattern:**
```c
char buf[MAX_STRING_LENGTH];  // typically 4096
sprintf(buf, "...", ...);     // no length check
send_to_char(buf, ch);
```

**Migration:** Replace with `snprintf(buf, sizeof(buf), ...)` or, where the BUFFER system is already in use, switch to `bprintf(buffer, ...)`.

---

### strcat — 1,147 calls

**Top 10 files:**

| File | Count | Notes |
|------|------:|-------|
| bit.c | 476 | Flag-to-string formatting (41% of all strcat!) |
| act_info.c | 184 | Building display strings |
| act_comm.c | 31 | Communication formatting |
| church.c | 30 | Church display |
| channels/channel_service.c | 30 | Channel formatting |
| fight.c | 29 | Combat messages |
| act_wiz.c | 27 | Wizard command output |
| olc.c | 23 | OLC display |
| weather.c | 20 | Weather descriptions |
| editors/common.c | 20 | Editor display helpers |

**Critical hotspot — bit.c:**
`bit.c` contains flag-to-string conversion functions that build strings by repeated `strcat` into a fixed buffer. This is the single riskiest pattern in the codebase — 476 calls, all appending to buffers with no length tracking.

**Migration:** `bit.c` should be rewritten to use BUFFER or `sent_strlcat` with length tracking. The flag functions are natural candidates for a table-driven approach.

---

### strcpy — 336 calls

**Top 10 files:**

| File | Count | Notes |
|------|------:|-------|
| comm.c | 49 | Network communication (HIGH RISK — external input) |
| boat.c | 48 | Ship system |
| editors/mobiles/medit.c | 30 | Mobile editor |
| act_info.c | 29 | Player info display |
| editors/game_settings/gameedit.c | 27 | Game settings editor |
| editors/reserved_vnums/reserved.c | 14 | Vnum reservation |
| act_wiz.c | 12 | Wizard commands |
| string.c | 10 | String editing utilities |
| handler.c | 10 | Core handler functions |
| church.c | 9 | Church system |

**Highest risk:** `comm.c` — this handles network input/output and any strcpy involving player-supplied data is a potential buffer overflow.

**Migration:** Replace with `strlcpy` or `sent_strlcpy`. Priority on `comm.c` first.

---

### strncpy — 219 calls

While `strncpy` looks safe, it does NOT guarantee NUL termination when the source is longer than the destination. It also zero-fills the remainder of the buffer, which is wasteful.

**Top 10 files:**

| File | Count | Notes |
|------|------:|-------|
| boat.c | 37 | Ship system |
| script_commands.c | 19 | Script command handling |
| quest.c | 12 | Quest system |
| editors/events/evtedit.c | 11 | Event editor |
| act_wiz.c | 8 | Wizard commands |
| editors/game_settings/gameedit.c | 8 | Game settings |
| script_mpcmds.c | 7 | Mobile script commands |
| note.c | 7 | Note system |
| blueprint.c | 7 | Blueprint system |
| handler.c | 6 | Core handlers |

**Migration:** Replace with `strlcpy` which guarantees NUL termination and returns the source length for truncation detection.

---

### atoi — 1,009 calls

The second largest category. `atoi` has no error detection — `atoi("abc")` returns 0, `atoi("12abc")` returns 12, `atoi("")` returns 0. All silently.

**Top 15 files:**

| File | Count | Notes |
|------|------:|-------|
| editors/dungeons/dngedit.c | 99 | Dungeon editor |
| editors/objects/oedit_types.c | 80 | Object type editor |
| olc_act.c | 71 | OLC actions |
| editors/mobiles/medit.c | 68 | Mobile editor |
| editors/wilderness/wedit.c | 53 | Wilderness editor |
| act_wiz.c | 52 | Wizard commands |
| editors/objects/oedit.c | 43 | Object editor |
| editors/events/evtedit.c | 41 | Event editor |
| script_commands.c | 38 | Script commands |
| boat.c | 37 | Ship system |
| script_mpcmds.c | 26 | Mobile script commands |
| editors/areas/aedit.c | 23 | Area editor |
| church.c | 23 | Church system |
| editors/blueprints/bpedit.c | 20 | Blueprint editor |
| editors/rooms/redit.c | 19 | Room editor |

**OLC editors total: 641 atoi calls** — this is where builders type freeform numbers. Every one of these should validate input and provide feedback on parse failure.

**Migration:** Replace with `sent_parse_int()` that rejects non-numeric input and provides error feedback.

---

### atol — 169 calls

Same problems as `atoi` but for long values. Concentrated in:

| File | Count | Notes |
|------|------:|-------|
| editors/quests/qedit.c | 35 | Quest editor |
| editors/events/evtedit.c | 25 | Event editor |
| act_wiz.c | 16 | Wizard commands |
| handler.c | 9 | Core handlers |
| editors/objects/oedit_types.c | 9 | Object type editor |
| olc.c | 7 | OLC framework |
| church.c | 5 | Church system |
| act_obj.c | 5 | Object handling |

**Migration:** Replace with `sent_parse_long()`.

---

### strtok — 17 calls

Modifies its input string and uses global state, making it thread-unsafe and dangerous in any reentrant context.

| File | Count |
|------|------:|
| io/cache/redis_cache.c | 4 |
| tests/integration/wnum_tests.c | 4 |
| channels/channel_service.c | 2 |
| act_wiz.c | 2 |
| editors/channels/cedit.c | 2 |
| misc | 3 |

**Migration:** Replace with `strtok_r` (reentrant) or manual parsing.

---

### sscanf — 20 calls

| File | Count |
|------|------:|
| editors/events/evtedit.c | 5 |
| io/common.c | 4 |
| io/json/json_area.c | 2 |
| io/json/json_persist.c | 2 |
| stats.c | 2 |
| misc | 5 |

**Migration:** Replace with `sent_parse_int` / `sent_parse_long` or purpose-built parsers.

---

## Safe Alternative Adoption

Current usage of safe functions shows adoption is happening but concentrated in newer code.

### snprintf — 985 calls (good)

| File | Count | Notes |
|------|------:|-------|
| wilds_wildgen.c | 75 | Newer wilderness generation code |
| channels/channel_service.c | 42 | New channel system |
| account/penalty.c | 38 | Account system |
| requirements.c | 34 | Requirements system |
| scripts.c | 30 | Script engine |

### strlcpy — 250 calls (good)

Heavily concentrated in the channels subsystem (written recently):

| File | Count |
|------|------:|
| channels/channel_service.c | 65 |
| channels/channel_transport_redis.c | 27 |
| channels/channel_registry.c | 19 |
| channels/channel_moderation.c | 13 |
| channels/channel_transport_local.c | 11 |

### strlcat — 48 calls (low)

| File | Count |
|------|------:|
| editors/channels/cedit.c | 16 |
| channels/channel_service.c | 12 |
| act_comm.c | 6 |
| account/preferences.c | 6 |

### bprintf — 146 calls (growing)

The safe BUFFER formatting function. Still underused relative to sprintf:

| File | Count |
|------|------:|
| dungeon.c | 48 |
| blueprint.c | 47 |
| act_wiz.c | 24 |
| editors/common/olc_editor.c | 20 |

---

## Cyclomatic Complexity Report

Measured with `pmccabe`. cURL's threshold is CC ≤ 100.

### Overview

| Metric | Value |
|--------|------:|
| Total functions | 5,752 |
| Average CC | 11.4 |
| Functions CC > 200 | 13 |
| Functions CC > 100 | 56 |
| Functions CC > 50 | 192 |

### All Functions with CC > 100

| CC | Function | File | Lines |
|---:|----------|------|------:|
| 693 | script_varseton | scripts.c | 2,250 |
| 407 | do_quest | quest.c | 1,976 |
| 291 | compile_script | script_comp.c | 1,502 |
| 236 | fread_char | save.c | 1,569 |
| 235 | DECL_OPC_FUN | scripts.c | 2,179 |
| 219 | damage_new | fight.c | 629 |
| 210 | one_hit | fight.c | 596 |
| 201 | test_number_trigger | scripts.c | 435 |
| 196 | SCRIPT_CMD | script_tpcmds.c | 516 |
| 191 | char_update | update.c | 649 |
| 189 | SCRIPT_CMD | script_mpcmds.c | 490 |
| 189 | SCRIPT_CMD | script_opcmds.c | 490 |
| 189 | SCRIPT_CMD | script_rpcmds.c | 492 |
| 184 | interpret | interp.c | 794 |
| 179 | do_look | act_info.c | 669 |
| 178 | test_number_sight_trigger | scripts.c | 446 |
| 168 | do_group | act_comm.c | 655 |
| 167 | music_end | music.c | 364 |
| 163 | aggr_update | update.c | 619 |
| 161 | do_ship_waypoints | boat.c | 866 |
| 160 | set_obj_values | olc_act.c | 1,056 |
| 159 | move_char | act_move.c | 447 |
| 156 | fwrite_char | save.c | 574 |
| 152 | print_live_obj_values | act_wiz.c | 699 |
| 140 | test_string_trigger | scripts.c | 384 |
| 139 | expand_escape_variable | script_expand.c | 532 |
| 139 | obj_set_legacy_value_slot | item_types.c | 249 |
| 139 | obj_get_legacy_value_slot | item_types.c | 244 |
| 138 | expand_entity_mobile | script_expand.c | 572 |
| 136 | do_gq | gq.c | 715 |
| 135 | get_wilderness_map | act_info.c | 436 |
| 134 | expand_string_simple_code | script_expand.c | 261 |
| 132 | SCRIPT_CMD (13948) | script_commands.c | 629 |
| 132 | storage_account_cmd | storage.c | 436 |
| 131 | SCRIPT_CMD (14641) | script_commands.c | 537 |
| 130 | do_buy | act_obj.c | 1,078 |
| 125 | SCRIPT_CMD (10555) | script_commands.c | 360 |
| 125 | do_mset | act_wiz.c | 661 |
| 124 | do_get | act_obj.c | 440 |
| 124 | do_enter | act_enter.c | 507 |
| 123 | test_vnumname_trigger | scripts.c | 385 |
| 122 | show_room | act_info.c | 348 |
| 118 | parse_note | note.c | 594 |
| 115 | read_church | church.c | 525 |
| 115 | req_decompile_node | requirements.c | 345 |
| 112 | fix_character | save.c | 452 |
| 111 | compile_entity | script_comp.c | 508 |
| 110 | SCRIPT_CMD (4901) | script_commands.c | 222 |
| 109 | fread_obj_new | save.c | 853 |
| 109 | wear_obj | act_obj.c | 434 |
| 109 | load_char_obj_internal | save.c | 428 |
| 108 | do_score | act_info.c | 677 |
| 106 | obj_update | update.c | 349 |
| 106 | show_char_to_char_0 | act_info.c | 376 |
| 104 | variable_fix | script_vars.c | 261 |
| 101 | do_prefadmin | account/preferences.c | 541 |

### Complexity by Subsystem

| Subsystem | Functions CC>100 | Worst CC | Notes |
|-----------|----------------:|---------:|-------|
| Scripting engine | 16 | 693 | Most complex subsystem by far |
| Save/load (save.c) | 5 | 236 | Character persistence |
| Combat (fight.c) | 2 | 219 | Core combat loop |
| Player info (act_info.c) | 4 | 179 | Display commands |
| OLC (olc_act.c, editors) | 2 | 160 | Object value setting |
| Commands (interp.c) | 1 | 184 | Command dispatcher |
| Updates (update.c) | 3 | 191 | Tick handlers |
| Objects (act_obj.c) | 3 | 130 | Object manipulation |

---

## Memory Management

### Raw allocator usage (low — good)

| Function | Total | Top Files |
|----------|------:|-----------|
| malloc | 72 | comm.c (8), protocol.c (7), handler.c (8), hunt.c (6), mem.c (6) |
| calloc | 52 | tests/framework (6), redis_cache (6), wilds_wildgen (5) |
| realloc | 4 | hunt.c (1), log.c (1), secret.c (2) |
| free | 330 | test_framework (25), json_persist (21), wilds_wildgen (19) |

### Managed allocator usage (high — good)

| Function | Total | Notes |
|----------|------:|-------|
| str_dup | 1,866 | String ownership pattern, widely adopted |
| free_string | 1,635 | Paired with str_dup |
| alloc_perm | 159 | Persistent allocation for game data |

**Assessment:** Memory management is the strongest area. 88.9% of string-related memory goes through the project's own managed allocators. Raw malloc/free is concentrated in infrastructure code (networking, testing, caching) where it's expected.

---

## BUFFER System Status

### Current API

| Function | Count | Notes |
|----------|------:|-------|
| new_buf / new_buf_size | 308 | Allocation |
| add_buf | 1,640 | String append (most common operation) |
| bprintf | 146 | Safe formatted append |
| free_buf | ~308 | Deallocation (matches new_buf) |
| clear_buf | varies | Reset buffer |
| buf_string | varies | Get raw string pointer |

### Internal Issues

1. **No `len` field** — every `add_buf` calls `strlen` to find the end (O(n) per append)
2. **Uses `strcpy`/`strcat` internally** — the safe layer is built on unsafe primitives
3. **`add_buf_char` creates temp string** — should be a direct byte write
4. **Growth uses malloc+strcpy+free** — should use realloc or memcpy with known length
5. **No hard cap on growth** — could exhaust memory

See [PLAN_C_HARDENING.md](PLAN_C_HARDENING.md) Phase 1 for the remediation plan.

---

## Positive Findings

1. **No `gets` calls** — the most dangerous C function is completely absent
2. **`str_dup`/`free_string`** — excellent string ownership pattern, universally adopted
3. **`alloc_perm`** — custom persistent allocator avoids malloc/free churn for game data
4. **`bprintf` exists** — safe BUFFER formatting is available, just underused
5. **Newer code uses safe patterns** — channels subsystem, account system, requirements all use `strlcpy`/`snprintf`
6. **Build system supports ASan** — overflow detection tooling is already in place
7. **Average complexity is reasonable** — 11.4 average CC is good; the problem is concentrated in 56 outlier functions

# Remaining Legacy fread_* I/O Usage

Audit performed as Task 6 of the legacy persistence removal series (Tasks 1-5
removed area/player/object/mobile/token/ship fallback readers).

## Summary

All 8 core fread_* utility functions in `db.c` still have active callers and
**cannot be removed** until the subsystems below are migrated to JSON I/O.

Three unused variants were removed in this task:
- `fread_string_new()` — zero callers
- `fread_string_eol_new()` — zero callers
- `fread_string_len()` — zero callers

## Functions and Remaining Callers

### fread_word (125+ calls)
| File | Calls | Data Format |
|------|-------|-------------|
| olc_save.c | 27 | Area file section-based key/value loading |
| save.c | 25 | Player character persistence |
| script_vars.c | 13 | Script variable persistence |
| wilds.c | 11 | Wilderness vlinks/terrain |
| church.c | 7 | Church/organization data |
| editors/game_settings/gameedit.c | 5 | Game settings changesets |
| blueprint.c | 4 | Dungeon blueprints |
| dungeon.c | 4 | Dungeon instances |
| project.c | 4 | Project system |
| db2.c | 3 | Socials, NPC ships |
| boat.c | 3 | Ship/boat data |
| reputation.c | 3 | Reputation system |
| editors/reserved_vnums/reserved.c | 2 | Reserved vnum ranges |
| staff.c | 2 | Staff data |
| act_wiz.c | 2 | Wizard commands (ban lists) |

### fread_number (717+ calls)
| File | Calls | Data Format |
|------|-------|-------------|
| olc_save.c | 172 | Area file numeric fields |
| act_wiz.c | 140 | Ban/wiznet/disabled commands |
| save.c | 126 | Player character numeric fields |
| script_vars.c | 56 | Script variable values |
| church.c | 56 | Church numeric properties |
| blueprint.c | 30 | Blueprint definitions |
| wilds.c | 29 | Wilderness coordinates/UIDs |
| boat.c | 27 | Ship properties |
| db2.c | 27 | Socials/NPC ship properties |
| dungeon.c | 24 | Dungeon instance data |
| project.c | 9 | Project system |
| reputation.c | 5 | Reputation values |
| chat_rooms.c | 5 | Chat room configuration |
| editors/reserved_vnums/reserved.c | 4 | Reserved vnum ranges |
| editors/game_settings/gameedit.c | 4 | Game settings |
| staff.c | 3 | Staff data |

### fread_string (258+ calls)
| File | Calls | Data Format |
|------|-------|-------------|
| olc_save.c | 84 | Area file string fields (tilde-terminated) |
| act_wiz.c | 36 | Ban/disabled command strings |
| save.c | 24 | Player character strings |
| church.c | 17 | Church names/descriptions |
| blueprint.c | 16 | Blueprint string fields |
| db2.c | 14 | Social messages, NPC ship names |
| dungeon.c | 14 | Dungeon string fields |
| wilds.c | 12 | Wilderness descriptions |
| project.c | 10 | Project strings |
| script_vars.c | 9 | Script variable string values |
| reputation.c | 7 | Reputation names/descriptions |
| staff.c | 5 | Staff strings |
| editors/game_settings/gameedit.c | 5 | Settings changeset strings |
| chat_rooms.c | 5 | Chat room names/topics |
| editors/reserved_vnums/reserved.c | 3 | Reserved vnum labels |
| boat.c | 3 | Ship names |

### fread_to_eol (32 calls)
| File | Calls | Data Format |
|------|-------|-------------|
| script_vars.c | 9 | Comment/skip lines |
| save.c | 6 | Comment/skip lines |
| act_wiz.c | 4 | Comment/skip lines |
| editors/game_settings/gameedit.c | 3 | Comment/skip lines |
| editors/reserved_vnums/reserved.c | 2 | Comment/skip lines |
| reputation.c | 2 | Comment/skip lines |
| church.c | 2 | Comment/skip lines |
| wilds.c | 1 | Comment/skip lines |
| olc_save.c | 1 | Comment/skip lines |
| dungeon.c | 1 | Comment/skip lines |
| db2.c | 1 | Comment/skip lines |

### fread_string_eol (16 calls)
| File | Calls | Data Format |
|------|-------|-------------|
| db2.c | 8 | Disabled command messages |
| olc_save.c | 3 | Catalyst/section data |
| save.c | 3 | Player catalyst/skill data |
| wilds.c | 2 | Vlink/terrain strings |

### fread_letter (6 calls)
| File | Calls | Data Format |
|------|-------|-------------|
| db2.c | 4 | NPC ship/mobile format markers |
| reputation.c | 1 | Rank color codes |
| olc_save.c | 1 | Reset commands |

### fread_flag (6 calls)
| File | Calls | Data Format |
|------|-------|-------------|
| wilds.c | 3 | Vlink flags (base36-encoded) |
| reputation.c | 2 | Reputation/rank flags |
| church.c | 1 | Church settings flags |

### fwrite_flag (2 calls)
| File | Calls | Data Format |
|------|-------|-------------|
| church.c | 1 | Church settings save |
| wilds.c | 1 | Vlink flags save |

## Subsystems Requiring JSON Migration

Each of these subsystems uses fread_*-based flat-file I/O and needs its own
migration to JSON before the fread_* functions can be fully removed from db.c.

| Subsystem | Files | Priority | Notes |
|-----------|-------|----------|-------|
| **Area/OLC loading** | olc_save.c | High | Heaviest user (280+ calls). Already partially JSON but legacy reader remains. |
| **Player save/load** | save.c | High | 150+ calls. Core player persistence. |
| **Wizard commands** | act_wiz.c | Medium | Ban lists, disabled commands, wiznet. |
| **Church system** | church.c | Medium | Full church org data (ranks, logs, settings). |
| **Script variables** | script_vars.c | Medium | Persistent script variable storage. |
| **Wilderness** | wilds.c | Medium | Vlinks, terrain definitions. |
| **Blueprints** | blueprint.c | Medium | Dungeon blueprint definitions. |
| **Dungeons** | dungeon.c | Medium | Dungeon instance state. |
| **Boats/Ships** | boat.c | Low | Ship properties. |
| **Socials (db2.c)** | db2.c | Low | Social messages, NPC ships. |
| **Reputation** | reputation.c | Low | Reputation factions and ranks. |
| **Chat rooms** | chat_rooms.c | Low | Persistent chat room config. |
| **Projects** | project.c | Low | Project system data. |
| **Staff** | staff.c | Low | Staff tracking. |
| **Game settings** | editors/game_settings/gameedit.c | Low | Settings changeset history. |
| **Reserved vnums** | editors/reserved_vnums/reserved.c | Low | Vnum reservation records. |

## Dead Code Identified

### `ship_special_key_load()` in boat.c (line ~2348)

This function loads a `SPECIAL_KEY_DATA` structure from a FILE pointer. Its only
caller was `ship_load()`, which was removed in Task 4 (legacy ship persistence
removal). The function is now dead code with zero callers outside its own
definition in boat.c. It should be removed in a future cleanup pass.

## Removed in This Task

- `fread_string_new()` from db.c — unused variant of fread_string
- `fread_string_eol_new()` from db.c — unused variant of fread_string_eol
- `fread_string_len()` from db.c — unused length-prefixed string reader

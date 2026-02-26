# Code-Backed Project Reality Audit (2026-02-26)

Method: each active plan was cross-checked against `src/` by (1) planned code-file references that exist vs missing, and (2) keyword-level code footprint (matching file names/content).

## Summary

| Plan | Declared Status | Code-Inferred State | Planned File Refs Present | Code Keyword | Name Hits | Content Hits |
|---|---|---|---:|---|---:|---:|
| `docs/PLAN_ACHIEVEMENTS.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Not started or doc/code mismatch | 0/2 | `achievements` | 0 | 0 |
| `docs/PLAN_ARENA_SYSTEM.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Not started or doc/code mismatch | 0/4 | `arena` | 0 | 14 |
| `docs/PLAN_AURA_SYSTEM_BACKPORT.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Advanced/implemented slices present | 7/7 | `aura` | 0 | 15 |
| `docs/PLAN_BOOTSTRAP.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `bootstrap` | 8 | 20 |
| `docs/PLAN_CASTING_SYSTEM_REWORK.md` | Early Design / Directional Plan | Design-stage | 6/6 | `casting` | 0 | 13 |
| `docs/PLAN_CLASS_JOB_SYSTEM_BACKPORT.md` | Comprehensive Draft for Review | Design-stage | 1/3 | `class` | 4 | 69 |
| `docs/PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md` | Early Design / Directional Plan | Design-stage | 0/0 | `combat` | 0 | 40 |
| `docs/PLAN_CORE_SYSTEMS_INTEGRATION.md` | Program-Level Integration Draft | Design-stage | 0/0 | `io` | 14 | 43 |
| `docs/PLAN_CORPSE_RUNTIME_TYPES.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Advanced/implemented slices present | 2/2 | `corpse` | 3 | 28 |
| `docs/PLAN_CRAFTING_GATHERING_REWORK.md` | New Systems Design / Directional Plan | Design-stage | 0/0 | `crafting` | 0 | 9 |
| `docs/PLAN_DATA_DRIVEN_TABLES.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Advanced/implemented slices present | 2/2 | `data` | 9 | 197 |
| `docs/PLAN_DYNAMIC_CHANNELS.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `channels` | 2 | 18 |
| `docs/PLAN_EFFECTS_SYSTEM_REDESIGN.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `effects` | 1 | 18 |
| `docs/PLAN_EVENT_COMPLETION_CRITERIA.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `event` | 3 | 73 |
| `docs/PLAN_EVENT_EPROGS.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Not started or doc/code mismatch | 0/1 | `event` | 3 | 73 |
| `docs/PLAN_EVENT_WIDEVNUM_MIGRATION.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Partial implementation | 15/17 | `event` | 3 | 73 |
| `docs/PLAN_IO_REFACTOR.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Partial implementation | 7/19 | `io` | 14 | 43 |
| `docs/PLAN_LOGGING_AGGREGATORS.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `logging` | 0 | 28 |
| `docs/PLAN_LOGGING_UNIFIED_ERROR_HANDLING.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `logging` | 0 | 28 |
| `docs/PLAN_LONGER_DAYS.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `longer` | 0 | 39 |
| `docs/PLAN_MAZE_DUNGEON_BACKPORT.md` | Draft for Review (Updated for Area-Scoped JSON) | Design-stage | 2/2 | `maze` | 0 | 19 |
| `docs/PLAN_MODULE_SYSTEM.md` | Early Design / Sketch | Design-stage | 0/0 | `module` | 1 | 24 |
| `docs/PLAN_MULTI_SERVER.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Partial implementation | 5/9 | `server` | 0 | 21 |
| `docs/PLAN_OLC_REFACTOR.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Partial implementation | 34/35 | `olc` | 15 | 83 |
| `docs/PLAN_PARTY_SYSTEM.md` | Early Design / Sketch | Design-stage | 0/0 | `party` | 0 | 0 |
| `docs/PLAN_PERSISTENCE_REFACTOR.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `persistence` | 0 | 26 |
| `docs/PLAN_PREFERENCES_SOURCE_OF_TRUTH.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `preferences` | 2 | 17 |
| `docs/PLAN_QUEST_SYSTEM_REWORK.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `quest` | 1 | 51 |
| `docs/PLAN_SCRIPT_AUDIT_CONSOLIDATED.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Partial implementation | 6/7 | `script` | 14 | 77 |
| `docs/PLAN_SCRIPT_COMMANDS.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Advanced/implemented slices present | 2/2 | `script` | 14 | 77 |
| `docs/PLAN_SCRIPT_ENGINE_AUDIT.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `script` | 14 | 77 |
| `docs/PLAN_SHIP_SYSTEM_ARCHITECTURE.md` | New Systems Design / Directional Plan | Design-stage | 0/0 | `ship` | 0 | 49 |
| `docs/PLAN_WEATHER_AND_NPC_SHIP_SYSTEMS.md` | New Systems Integration Plan | Active code footprint (needs manual phase check) | 0/0 | `ship` | 0 | 49 |
| `docs/PLAN_WILDERNESS_STORAGE_AND_SIMULATION.md` | Corrective Re-sequencing Active | Partial implementation | 6/13 | `io` | 14 | 43 |
| `docs/PLAN_backport_object_multityping.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Active code footprint (needs manual phase check) | 0/0 | `object` | 0 | 121 |
| `docs/PLAN_backport_reputation_system.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Advanced/implemented slices present | 19/19 | `io` | 14 | 43 |
| `docs/PLAN_backport_skills_classes.md` | Active — Phases 0-8 Complete, Phase 9 pending | Advanced/implemented slices present | 2/2 | `class` | 4 | 69 |
| `docs/PLAN_cedit_conventions.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Advanced/implemented slices present | 2/2 | `io` | 14 | 43 |
| `docs/PLAN_command_enhancements.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Partial implementation | 3/4 | `command` | 6 | 111 |
| `docs/PLAN_group_analysis.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Partial implementation | 3/4 | `group` | 3 | 91 |
| `docs/PLAN_pubsub_communication.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Advanced/implemented slices present | 1/1 | `io` | 14 | 43 |
| `docs/PLAN_pubsub_stage1_scopes.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Insufficient direct code linkage in plan text | 0/0 | `pubsub` | 1 | 3 |
| `docs/PLAN_rview_staff_review.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Insufficient direct code linkage in plan text | 0/0 | `rview` | 1 | 1 |
| `docs/PLAN_xmacro_function_registry.md` | In Progress / Planned (verified 2026-02-26 docs audit) | Advanced/implemented slices present | 6/6 | `io` | 14 | 43 |

## Project Evidence

### `docs/PLAN_ACHIEVEMENTS.md`
- **Title:** Achievements System - Implementation Plan
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Not started or doc/code mismatch
- **Primary code keyword:** `achievements`
- **Planned refs missing:** `achievements.h`, `achievements.c`
- **Keyword content-hit count:** 0

### `docs/PLAN_ARENA_SYSTEM.md`
- **Title:** Arena System Enhancement Plan
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Not started or doc/code mismatch
- **Primary code keyword:** `arena`
- **Planned refs missing:** `arena.h`, `arena.c`, `io/json/json_arena.c`, `io/json/json_arena.h`
- **Keyword content-hit count:** 14

### `docs/PLAN_AURA_SYSTEM_BACKPORT.md`
- **Title:** Backport Plan: Character Aura System
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Advanced/implemented slices present
- **Primary code keyword:** `aura`
- **Planned refs found in codebase:** `merc.h`, `recycle.h`, `mem.c`, `handler.c`, `save.c`, `act_info.c`, `script_commands.c`
- **Keyword content-hit count:** 15

### `docs/PLAN_BOOTSTRAP.md`
- **Title:** Bootstrap Mode Implementation Plan
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `bootstrap`
- **Keyword file-name hits (sample):** `bootstrap/bootstrap.c`, `bootstrap/bootstrap.h`, `bootstrap/bootstrap_account.c`, `bootstrap/bootstrap_commands.c`, `bootstrap/bootstrap_files.c`, `bootstrap/bootstrap_internal.h`, `bootstrap/bootstrap_prompts.c`, `bootstrap/bootstrap_reserved.c`
- **Keyword content-hit count:** 20

### `docs/PLAN_CASTING_SYSTEM_REWORK.md`
- **Title:** Plan: Casting System Rework
- **Declared status:** Early Design / Directional Plan
- **Code-inferred state:** Design-stage
- **Primary code keyword:** `casting`
- **Planned refs found in codebase:** `magic.c`, `act_obj.c`, `skill_data.c`, `handler.c`, `fight.c`, `sectors_runtime.c`
- **Keyword content-hit count:** 13

### `docs/PLAN_CLASS_JOB_SYSTEM_BACKPORT.md`
- **Title:** PLAN: Class and Job System Backport (Comprehensive)
- **Declared status:** Comprehensive Draft for Review
- **Code-inferred state:** Design-stage
- **Primary code keyword:** `class`
- **Planned refs found in codebase:** `const.c`
- **Planned refs missing:** `src_20_dev/skills.c`, `src_20_dev/merc.h`
- **Keyword file-name hits (sample):** `act_class.c`, `class_data.c`, `class_data.h`, `tests/integration/class_data_tests.c`
- **Keyword content-hit count:** 69

### `docs/PLAN_COMBAT_LOOP_AND_DAMAGE_REWORK.md`
- **Title:** Plan: Combat Loop and Damage Rework
- **Declared status:** Early Design / Directional Plan
- **Code-inferred state:** Design-stage
- **Primary code keyword:** `combat`
- **Keyword content-hit count:** 40

### `docs/PLAN_CORE_SYSTEMS_INTEGRATION.md`
- **Title:** Plan: Core Systems Integration (Combat, Casting, Party, Classes)
- **Declared status:** Program-Level Integration Draft
- **Code-inferred state:** Design-stage
- **Primary code keyword:** `io`
- **Keyword file-name hits (sample):** `.deps/libbacktrace/mmapio.c`, `.deps/libcotp/utils/validation.c`, `auction.c`, `channels/channel_moderation.c`, `channels/channel_moderation.h`, `connection.c`, `connection.h`, `connection_tcp.c`
- **Keyword content-hit count:** 43

### `docs/PLAN_CORPSE_RUNTIME_TYPES.md`
- **Title:** Plan: Runtime Corpse Type Registry
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Advanced/implemented slices present
- **Primary code keyword:** `corpse`
- **Planned refs found in codebase:** `merc.h`, `tables.c`
- **Keyword file-name hits (sample):** `editors/corpses/corpsedit.c`, `io/json/json_corpse.c`, `io/json/json_corpse.h`
- **Keyword content-hit count:** 28

### `docs/PLAN_CRAFTING_GATHERING_REWORK.md`
- **Title:** Plan: Crafting, Gathering, and Exploration Systems (Net-New)
- **Declared status:** New Systems Design / Directional Plan
- **Code-inferred state:** Design-stage
- **Primary code keyword:** `crafting`
- **Keyword content-hit count:** 9

### `docs/PLAN_DATA_DRIVEN_TABLES.md`
- **Title:** Design: Data-Driven Loot Tables, Attack Patterns, and Requirements Integration
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Advanced/implemented slices present
- **Primary code keyword:** `data`
- **Planned refs found in codebase:** `requirements.c`, `requirements.h`
- **Keyword file-name hits (sample):** `class_data.c`, `class_data.h`, `skill_data.c`, `skill_data.h`, `song_data.c`, `song_data.h`, `tests/integration/class_data_tests.c`, `tests/integration/skill_data_tests.c`
- **Keyword content-hit count:** 197

### `docs/PLAN_DYNAMIC_CHANNELS.md`
- **Title:** PLAN: Dynamic Channels (Dungeon / Instance / Blueprint)
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `channels`
- **Keyword file-name hits (sample):** `channels/channels_common.c`, `channels/channels_common.h`
- **Keyword content-hit count:** 18

### `docs/PLAN_EFFECTS_SYSTEM_REDESIGN.md`
- **Title:** Effects System Redesign Plan
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `effects`
- **Keyword file-name hits (sample):** `effects.c`
- **Keyword content-hit count:** 18

### `docs/PLAN_EVENT_COMPLETION_CRITERIA.md`
- **Title:** PLAN_event_completion_criteria
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `event`
- **Keyword file-name hits (sample):** `event_types.c`, `event_types.h`, `events.c`
- **Keyword content-hit count:** 73

### `docs/PLAN_EVENT_EPROGS.md`
- **Title:** Plan: Event Progs (`eprogs`) and System Progs (`sprogs`)
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Not started or doc/code mismatch
- **Primary code keyword:** `event`
- **Planned refs missing:** `json_area.c`
- **Keyword file-name hits (sample):** `event_types.c`, `event_types.h`, `events.c`
- **Keyword content-hit count:** 73

### `docs/PLAN_EVENT_WIDEVNUM_MIGRATION.md`
- **Title:** Plan: Migrate Event System from Global UIDs to Area-Scoped Widevnums
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Partial implementation
- **Primary code keyword:** `event`
- **Planned refs found in codebase:** `editors/quests/qedit.c`, `quest.c`, `io/json/json_area.c`, `merc.h`, `editors/events/evtedit.c`, `io/json/json_game_settings.c`, `event_types.h`, `event_types.c`
- **Planned refs missing:** `json_area.c`, `evtedit.c`
- **Keyword file-name hits (sample):** `event_types.c`, `event_types.h`, `events.c`
- **Keyword content-hit count:** 73

### `docs/PLAN_IO_REFACTOR.md`
- **Title:** Plan: I/O Subsystem Refactoring
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Partial implementation
- **Primary code keyword:** `io`
- **Planned refs found in codebase:** `save.c`, `db.c`, `olc_save.c`, `db2.c`, `dungeon.c`, `blueprint.c`, `boat.c`
- **Planned refs missing:** `json_*.c`, `json_char.c`, `json_area.c`, `redis_cache.c`, `async_cache.c`, `common.c`, `common.h`, `json_*.h`
- **Keyword file-name hits (sample):** `.deps/libbacktrace/mmapio.c`, `.deps/libcotp/utils/validation.c`, `auction.c`, `channels/channel_moderation.c`, `channels/channel_moderation.h`, `connection.c`, `connection.h`, `connection_tcp.c`
- **Keyword content-hit count:** 43

### `docs/PLAN_LOGGING_AGGREGATORS.md`
- **Title:** Plan: Enhanced Logging for External Aggregators (v2)
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `logging`
- **Keyword content-hit count:** 28

### `docs/PLAN_LOGGING_UNIFIED_ERROR_HANDLING.md`
- **Title:** Plan: Unified Error Handling & Multi-Sink Logging
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `logging`
- **Keyword content-hit count:** 28

### `docs/PLAN_LONGER_DAYS.md`
- **Title:** Plan: Lengthen Game Day (15-Minute Ticks + Timestamp-Based Affects)
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `longer`
- **Keyword content-hit count:** 39

### `docs/PLAN_MAZE_DUNGEON_BACKPORT.md`
- **Title:** Plan: Maze Layout Type & Shared Dungeon Backport
- **Declared status:** Draft for Review (Updated for Area-Scoped JSON)
- **Code-inferred state:** Design-stage
- **Primary code keyword:** `maze`
- **Planned refs found in codebase:** `merc.h`, `script_commands.c`
- **Keyword content-hit count:** 19

### `docs/PLAN_MODULE_SYSTEM.md`
- **Title:** Plan: Module & Namespace System
- **Declared status:** Early Design / Sketch
- **Code-inferred state:** Design-stage
- **Primary code keyword:** `module`
- **Keyword file-name hits (sample):** `tests/framework/test_modules.h`
- **Keyword content-hit count:** 24

### `docs/PLAN_MULTI_SERVER.md`
- **Title:** Multi‑Server Architecture & Migration Plan
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Partial implementation
- **Primary code keyword:** `server`
- **Planned refs found in codebase:** `nanny.c`, `account/auth.c`, `storage.c`, `mail.c`, `trade.c`
- **Planned refs missing:** `json_account.c`, `json_char.c`, `redis_cache.c`, `json_game_settings.c`
- **Keyword content-hit count:** 21

### `docs/PLAN_OLC_REFACTOR.md`
- **Title:** OLC Editor Framework Refactoring Plan
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Partial implementation
- **Primary code keyword:** `olc`
- **Planned refs found in codebase:** `olc.c`, `olc_act.c`, `mxp_links.h`, `editors/areas/aedit.c`, `editors/rooms/redit.c`, `editors/objects/oedit.c`, `editors/objects/oedit_types.c`, `editors/mobiles/medit.c`
- **Planned refs missing:** `editors/missions/msnedit.c`
- **Keyword file-name hits (sample):** `editors/common/olc_commands.c`, `editors/common/olc_commands.h`, `editors/common/olc_display.c`, `editors/common/olc_display.h`, `editors/common/olc_editor.c`, `editors/common/olc_editor.h`, `editors/scripting/olc_mpcode.c`, `io/json/json_olc.c`
- **Keyword content-hit count:** 83

### `docs/PLAN_PARTY_SYSTEM.md`
- **Title:** Plan: Tiered Party System
- **Declared status:** Early Design / Sketch
- **Code-inferred state:** Design-stage
- **Primary code keyword:** `party`
- **Keyword content-hit count:** 0

### `docs/PLAN_PERSISTENCE_REFACTOR.md`
- **Title:** Plan: Persistence Layer Refactor
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `persistence`
- **Keyword content-hit count:** 26

### `docs/PLAN_PREFERENCES_SOURCE_OF_TRUTH.md`
- **Title:** PLAN: Preferences as Source of Truth
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `preferences`
- **Keyword file-name hits (sample):** `account/preferences.c`, `account/preferences.h`
- **Keyword content-hit count:** 17

### `docs/PLAN_QUEST_SYSTEM_REWORK.md`
- **Title:** Quest System Rework Plan
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `quest`
- **Keyword file-name hits (sample):** `quest.c`
- **Keyword content-hit count:** 51

### `docs/PLAN_SCRIPT_AUDIT_CONSOLIDATED.md`
- **Title:** Consolidated Script Engine Audit & Execution Plan
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Partial implementation
- **Primary code keyword:** `script`
- **Planned refs found in codebase:** `scripts.h`, `tests/integration/script_engine_tests.c`, `script_mpcmds.c`, `script_opcmds.c`, `script_rpcmds.c`, `script_tpcmds.c`
- **Planned refs missing:** `script_lua.c`
- **Keyword file-name hits (sample):** `script_cmds.c`, `script_commands.c`, `script_comp.c`, `script_const.c`, `script_expand.c`, `script_ifc.c`, `script_mpcmds.c`, `script_opcmds.c`
- **Keyword content-hit count:** 77

### `docs/PLAN_SCRIPT_COMMANDS.md`
- **Title:** Plan: Script Commands & Ifchecks for New Systems
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Advanced/implemented slices present
- **Primary code keyword:** `script`
- **Planned refs found in codebase:** `script_ifc.c`, `script_const.c`
- **Keyword file-name hits (sample):** `script_cmds.c`, `script_commands.c`, `script_comp.c`, `script_const.c`, `script_expand.c`, `script_ifc.c`, `script_mpcmds.c`, `script_opcmds.c`
- **Keyword content-hit count:** 77

### `docs/PLAN_SCRIPT_ENGINE_AUDIT.md`
- **Title:** Script Engine Audit Plan
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `script`
- **Keyword file-name hits (sample):** `script_cmds.c`, `script_commands.c`, `script_comp.c`, `script_const.c`, `script_expand.c`, `script_ifc.c`, `script_mpcmds.c`, `script_opcmds.c`
- **Keyword content-hit count:** 77

### `docs/PLAN_SHIP_SYSTEM_ARCHITECTURE.md`
- **Title:** Plan: Ship System Architecture (Net-New Completion Track)
- **Declared status:** New Systems Design / Directional Plan
- **Code-inferred state:** Design-stage
- **Primary code keyword:** `ship`
- **Keyword content-hit count:** 49

### `docs/PLAN_WEATHER_AND_NPC_SHIP_SYSTEMS.md`
- **Title:** Plan: Weather and NPC Ship Systems (Wilderness-Dependent)
- **Declared status:** New Systems Integration Plan
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `ship`
- **Keyword content-hit count:** 49

### `docs/PLAN_WILDERNESS_STORAGE_AND_SIMULATION.md`
- **Title:** Plan: Wilderness Storage and Simulation Contract (2026)
- **Declared status:** Corrective Re-sequencing Active
- **Code-inferred state:** Partial implementation
- **Primary code keyword:** `io`
- **Planned refs found in codebase:** `wilds_wildgen.c`, `wilds.c`, `wilds.h`, `weather.c`, `boat.c`, `update.c`
- **Planned refs missing:** `json_area.c`, `wilderness_storage.c/.h`, `wilderness_state.c/.h`, `wilderness_mods.c/.h`, `wilderness_wmap.c/.h`, `wilderness_wterr.c/.h`, `wilderness_vlinks.c/.h`
- **Keyword file-name hits (sample):** `.deps/libbacktrace/mmapio.c`, `.deps/libcotp/utils/validation.c`, `auction.c`, `channels/channel_moderation.c`, `channels/channel_moderation.h`, `connection.c`, `connection.h`, `connection_tcp.c`
- **Keyword content-hit count:** 43

### `docs/PLAN_backport_object_multityping.md`
- **Title:** Backport: Object Multityping & Type-Specific Data
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Active code footprint (needs manual phase check)
- **Primary code keyword:** `object`
- **Keyword content-hit count:** 121

### `docs/PLAN_backport_reputation_system.md`
- **Title:** Backport Analysis: Reputation System
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Advanced/implemented slices present
- **Primary code keyword:** `io`
- **Planned refs found in codebase:** `merc.h`, `reputation.c`, `olc_save.c`, `io/json/json_area.c`, `io/json/json_char.c`, `db.c`, `editors/reputation/repedit.c`, `olc.h`
- **Keyword file-name hits (sample):** `.deps/libbacktrace/mmapio.c`, `.deps/libcotp/utils/validation.c`, `auction.c`, `channels/channel_moderation.c`, `channels/channel_moderation.h`, `connection.c`, `connection.h`, `connection_tcp.c`
- **Keyword content-hit count:** 43

### `docs/PLAN_backport_skills_classes.md`
- **Title:** Backport Plan: Skill, Class, and Supporting System Overhaul
- **Declared status:** Active — Phases 0-8 Complete, Phase 9 pending
- **Code-inferred state:** Advanced/implemented slices present
- **Primary code keyword:** `class`
- **Planned refs found in codebase:** `const.c`, `merc.h`
- **Keyword file-name hits (sample):** `act_class.c`, `class_data.c`, `class_data.h`, `tests/integration/class_data_tests.c`
- **Keyword content-hit count:** 69

### `docs/PLAN_cedit_conventions.md`
- **Title:** PLAN: CEdit Conventions (Match Existing OLC Editors)
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Advanced/implemented slices present
- **Primary code keyword:** `io`
- **Planned refs found in codebase:** `editors/mobiles/medit.c`, `editors/commands/cmdedit.c`
- **Keyword file-name hits (sample):** `.deps/libbacktrace/mmapio.c`, `.deps/libcotp/utils/validation.c`, `auction.c`, `channels/channel_moderation.c`, `channels/channel_moderation.h`, `connection.c`, `connection.h`, `connection_tcp.c`
- **Keyword content-hit count:** 43

### `docs/PLAN_command_enhancements.md`
- **Title:** PLAN: Command System Enhancements - Alias Commands and Argument Restrictions
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Partial implementation
- **Primary code keyword:** `command`
- **Planned refs found in codebase:** `interp.h`, `interp.c`, `editors/commands/cmdedit.c`
- **Planned refs missing:** `cmdedit.c`
- **Keyword file-name hits (sample):** `bootstrap/bootstrap_commands.c`, `editors/common/olc_commands.c`, `editors/common/olc_commands.h`, `io/json/json_commands.c`, `io/json/json_commands.h`, `script_commands.c`
- **Keyword content-hit count:** 111

### `docs/PLAN_group_analysis.md`
- **Title:** PLAN: Group and Party System Rework
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Partial implementation
- **Primary code keyword:** `group`
- **Planned refs found in codebase:** `act_comm.c`, `comm.c`, `update.c`
- **Planned refs missing:** `groups.c/.h`
- **Keyword file-name hits (sample):** `skill_group.c`, `skill_group.h`, `tests/integration/skill_group_tests.c`
- **Keyword content-hit count:** 91

### `docs/PLAN_pubsub_communication.md`
- **Title:** PLAN: Communication System Refactor to Publish/Subscribe Model
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Advanced/implemented slices present
- **Primary code keyword:** `io`
- **Planned refs found in codebase:** `act_comm.c`
- **Keyword file-name hits (sample):** `.deps/libbacktrace/mmapio.c`, `.deps/libcotp/utils/validation.c`, `auction.c`, `channels/channel_moderation.c`, `channels/channel_moderation.h`, `connection.c`, `connection.h`, `connection_tcp.c`
- **Keyword content-hit count:** 43

### `docs/PLAN_pubsub_stage1_scopes.md`
- **Title:** PLAN: PubSub Stage 1 — Scope Routing & Subscription Core
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Insufficient direct code linkage in plan text
- **Primary code keyword:** `pubsub`
- **Keyword file-name hits (sample):** `tests/integration/channel_pubsub_tests.c`
- **Keyword content-hit count:** 3

### `docs/PLAN_rview_staff_review.md`
- **Title:** PLAN: `rview` Staff Review Queue Tool (Phase 1 Spec)
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Insufficient direct code linkage in plan text
- **Primary code keyword:** `rview`
- **Keyword file-name hits (sample):** `rview.c`
- **Keyword content-hit count:** 1

### `docs/PLAN_xmacro_function_registry.md`
- **Title:** Plan: X-Macro Registry for Spell & Command Function Registration
- **Declared status:** In Progress / Planned (verified 2026-02-26 docs audit)
- **Code-inferred state:** Advanced/implemented slices present
- **Primary code keyword:** `io`
- **Planned refs found in codebase:** `magic.h`, `skill_data.c`, `const.c`, `interp.h`, `tables.c`, `interp.c`
- **Keyword file-name hits (sample):** `.deps/libbacktrace/mmapio.c`, `.deps/libcotp/utils/validation.c`, `auction.c`, `channels/channel_moderation.c`, `channels/channel_moderation.h`, `connection.c`, `connection.h`, `connection_tcp.c`
- **Keyword content-hit count:** 43


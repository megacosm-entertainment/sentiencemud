# Sentience MUD Roadmap

**Last Updated:** February 8, 2026

This document tracks the project's development trajectory: what's been completed, what's in progress, and what's planned. Each section links to detailed design documents where they exist.

---

## Completed

These projects are finished and documented in `docs/done/`.

### JSON Persistence
Full migration of player accounts and characters from legacy formats to JSON. Includes Redis caching with async disk write-behind. All player data now saves and loads as structured JSON.

### Redis Integration
Operational caching layer for player data. Async disk writer handles persistence. Character and account lookups hit Redis first, falling back to disk.

### Flagset Migration
Converted legacy integer bitfield flags to a proper flagset system across the codebase. Multi-phase project covering all entity types.

### Object Duplication Fixes
Identified and fixed multiple object duplication bugs in inventory, locker, and container systems. Comprehensive analysis documented.

### Protocol Layer
Multi-protocol connection support: Telnet, TLS (via OpenSSL), and WebSocket. Protocol negotiation and MCCP compression.

### Crypto Modernization (Phase 1)
Password hashing upgraded from SHA-256 to Argon2id via libsodium. Transparent migration on login -- old hashes are re-hashed when players authenticate.

### Account Auth Consolidation
Unified auth API (`account/auth.c`) replacing 16+ duplicated password verification paths and 17 MFA verification copies. Extracted utility modules for the login state machine.

### Logging Refactor
Migrated to zlog-based structured logging with categories, levels, and configurable output targets.

### Test Framework
JSON-driven test framework with conditional compilation (`#ifdef BUILD_TESTS`). Supports unit and integration tests, dependency management, profiles, and coverage analysis.

### Area Find Commands
All eight find commands (`mfind`, `ofind`, `tfind`, `rfind`, `bpfind`, `bsfind`, `dngfind`, `shfind`) updated with widevnum display and area-scoping filters.

---

## In Progress

### Widevnum Migration

**Status:** Phases 1-6 complete. Phase 7 in progress.
**Docs:** [widevnums/](widevnums/)

The core project: transitioning from globally-unique vnums to area-scoped widevnums (`area_uid:local_vnum`). This enables collision-free multi-area development and is a prerequisite for many downstream features.

**What's done:**
- `WNUM` / `WNUM_LOAD` types and conversion infrastructure
- `parse_widevnum()` for runtime input, `parse_widevnum_load()` for deserialization
- `widevnum_string_*()` display functions for all entity types
- `wnum_match_*()` comparison functions
- Per-area hash tables for entity lookup
- JSON serialization with `WNUM_LOAD`
- Stat commands (`rstat`, `ostat`, `mstat`), find commands, and navigation commands (`goto`, `transfer`, `at`)

**What remains (Phase 7-8, ~180 changes across ~30 files):**
- **Critical:** Cross-area comparison bugs in lock/key, quest, global quest, and script systems (~20 comparisons that check bare vnums without area context)
- Display polish for remaining commands (`mload`, `oload`, `mwhere` confirmation messages)
- Struct field migration: bare `long vnum` fields in mail, quest, global quest, and other structures need `WNUM`/`WNUM_LOAD`
- Hardcoded vnum constant migration to the reserved entity system
- Display consistency pass across ~50 files

**Detailed remaining work:** [widevnums/WIDEVNUM_REMAINING_WORK.md](widevnums/WIDEVNUM_REMAINING_WORK.md)

### Crypto Modernization (Phase 2)

**Status:** Planned, not started.
**Docs:** [PLAN_crypto_phase2.md](PLAN_crypto_phase2.md)

Second phase of crypto work, building on the Argon2id migration completed in Phase 1.

### Bootstrap System

**Status:** Partially implemented.
**Docs:** [PLAN_BOOTSTRAP.md](PLAN_BOOTSTRAP.md)

Automated setup for fresh deployments. Directory creation is implemented in `bootstrap/bootstrap_files.c`. Remaining: minimal data file generation, package download support.

---

## Planned: Near-Term

These have detailed design documents and are ready to implement once current work stabilizes.

### Script/Prog Grouping

**Docs:** [PLAN_PROG_GROUPING.md](PLAN_PROG_GROUPING.md) | [TODO_PROG_GROUPING.md](TODO_PROG_GROUPING.md)

Improve OLC display and editing of script triggers on entities. Currently, triggers are shown as a flat list with repeated script names; this groups them under their parent script. Also adds duplicate detection, group-aware deletion, a new `deltrigger` command, and an updated JSON serialization format (backward compatible).

Four phases: grouped display, updated OLC commands, JSON format change, reverse lookup (`uses` command). Each phase is independently deployable. No runtime or persistence changes -- the grouping is derived on demand from existing `PROG_LIST` data.

### Group System Refactor

**Docs:** [PLAN_group_analysis.md](PLAN_group_analysis.md)

Replace the implicit leader-pointer grouping with an explicit `GROUP_DATA` entity. Fixes a bug in `stop_grouped()`, eliminates O(N) group iteration over all loaded characters, and provides a stable group identity for downstream systems (pub/sub, party system).

### Pub/Sub Communication

**Docs:** [PLAN_pubsub_communication.md](PLAN_pubsub_communication.md)

Refactor all player communication channels to a unified publish/subscribe model backed by Redis. Enables universal channel history, timestamping, message reporting, content filtering, and per-channel moderation with graduated enforcement.

Depends on the group refactor for group channel identity. Covers global channels (`gossip`, `ooc`), contextual channels (`say`, `yell`), direct channels (`tell`), group channels (`gtell`), and organizational channels (`churchtalk`).

---

## Planned: Mid-Term

These have design sketches and are contingent on near-term work completing first.

### Party System

**Docs:** [PLAN_PARTY_SYSTEM.md](PLAN_PARTY_SYSTEM.md)

Tiered NPC companion system replacing the flat follower/pet model. Five tiers from temporary escorts (Tier 0) to alt-as-hero (Tier 4), each with different AI behavior, persistence, customization, and death/recovery mechanics.

Depends on group refactor (for `GROUP_DATA` foundation), skill refactor (for companion skill subsets), and class/job system (for companion classes).

### Skill System Refactor

**Docs:** [PLAN_SKILL_REFACTOR.md](PLAN_SKILL_REFACTOR.md)

Move from the hardcoded `skill_table[]` in `const.c` to a data-driven skill system. Enables runtime skill registration, per-class skill lists, and the skill subsets needed by the party/companion system.

### Class/Job System Backport

**Docs:** [PLAN_CLASS_JOB_SYSTEM_BACKPORT.md](PLAN_CLASS_JOB_SYSTEM_BACKPORT.md)

Backport the class and job progression system from the `src_20_dev` reference codebase. Supports the party system's Tier 2-3 companions and general class design improvements.

### Traits System

**Docs:** [PLAN_TRAITS.md](PLAN_TRAITS.md)

Character trait system for races and classes, using JSON definitions in `data/traits/`.

---

## Planned: Long-Term / Aspirational

These have early sketches or are documented in the tech debt tracker. They represent architectural improvements that would significantly evolve the codebase.

### Module & Namespace System

**Docs:** [PLAN_MODULE_SYSTEM.md](PLAN_MODULE_SYSTEM.md)

A framework for organizing subsystems (magic, auth, I/O, networking) as registerable modules with vtable-based contracts and namespace-scoped identity. Extends the widevnum namespace philosophy from data to code. Would enable downstream projects to extend or replace subsystems without modifying core source.

Early sketch -- depends on several subsystems stabilizing first.

### Reset System Modernization

**Docs:** [TECH_DEBT.md](TECH_DEBT.md) (Item #4)

Replace the legacy DikuMUD reset system with conditional resets (game-state-aware spawning), weighted spawn groups, script-integrated triggers (`TRIG_PRERELOAD`, `TRIG_RESETPREPARE`), and blueprint integration. Estimated 6-9 weeks.

### I/O Layer Refactor

**Docs:** [PLAN_IO_REFACTOR.md](PLAN_IO_REFACTOR.md) | [PLAN_JSON_IO_CONSOLIDATION.md](PLAN_JSON_IO_CONSOLIDATION.md)

Reorganize I/O code into `io/json/` and `io/legacy/` with a format-detection registry. Consolidate JSON serialization patterns.

### Maze/Dungeon Blueprint Backport

**Docs:** [PLAN_MAZE_DUNGEON_BACKPORT.md](PLAN_MAZE_DUNGEON_BACKPORT.md) | [TECH_DEBT.md](TECH_DEBT.md) (Item #6)

Replace the external static maze generator with the procedural blueprint system from `src_20_dev`. Dynamic generation at runtime, OLC editors for blueprint configuration, removal of hardcoded maze references.

### Multi-Server Architecture

**Docs:** [PLAN_MULTI_SERVER.md](PLAN_MULTI_SERVER.md)

Distribute game processing across multiple server instances. Aspirational -- significant architectural change.

### Command Enhancements

**Docs:** [PLAN_command_enhancements.md](PLAN_command_enhancements.md)

Various improvements to the player command system.

---

## Technical Debt

Tracked in [TECH_DEBT.md](TECH_DEBT.md). Key items:

| Priority | Item | Effort | Status |
|----------|------|--------|--------|
| Critical | String safety (`sprintf` -> `snprintf`, safe string library) | 4-6 weeks | Post-widevnum |
| Medium | Dead code removal (deprecated IMC module, ~2000 lines) | 1-2 weeks | Identified |
| Medium | War system naming review (insensitive terminology) | 1 day | Identified |
| Low | Room index/runtime split | 6-8 weeks | Deferred to Phase 9+ |
| Security | Vnum range tracking DoS prevention | N/A | Documented, do not implement |

---

## How to Read This Document

- **Completed:** Done and shipped. Reference docs in `docs/done/`.
- **In Progress:** Actively being worked on. Check the linked docs for current status.
- **Planned (Near-Term):** Detailed designs exist, implementation ready when bandwidth allows.
- **Planned (Mid-Term):** Designs exist but depend on near-term work completing.
- **Planned (Long-Term):** Early sketches or aspirational. May change significantly.
- **Technical Debt:** Known issues to address opportunistically or in dedicated cleanup cycles.

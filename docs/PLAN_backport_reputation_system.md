# Backport Analysis: Reputation System

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


This document provides an analysis of the Reputation system from the `src_20_dev` branch and outlines a plan for backporting it into the main `src` codebase.

---

## 1. Reputation System

### 1.1. System Overview (`src_20_dev`)

The reputation system in `src_20_dev` is a complete faction system allowing players to gain and lose standing with various groups in the world.

- **Core Components:**
    -   **`REPUTATION_INDEX_DATA`**: The template for a faction, created by builders in OLC (`repedit`). It defines the faction's name, flags, and a series of ranks.
    -   **`REPUTATION_INDEX_RANK_DATA`**: Defines a specific rank within a faction (e.g., "Friendly", "Hated"), including its point capacity and behavioral flags (`REPUTATION_RANK_HOSTILE`).
    -   **`REPUTATION_DATA`**: A player-specific data structure that tracks their current points and rank within a faction. This is stored on the `CHAR_DATA` struct.
    -   **`MOB_REPUTATION_DATA`**: Attached to mob prototypes to define reputation point gains/losses upon killing them.

- **Functionality:**
    -   Players gain or lose reputation through mob kills, quest rewards, and script actions.
    -   As reputation points cross rank capacity thresholds, the player's rank changes.
    -   Reputation rank is used as a gate for various actions, including purchasing items from shops, learning skills, and accepting quests.
    -   It is deeply integrated with the scripting engine, allowing for dynamic NPC behavior based on player reputation.

### 1.2. Current State (`src`)

The `src` codebase **lacks this entire system**. The concept of reputation is limited to a few hardcoded legacy values for continent-based factions, with no underlying framework for expansion, player tracking, or OLC management.

### 1.3. Backport Plan and Status

This is a staged backport. The first stages establish the data model and persistence,
then OLC tooling, then deeper gameplay integrations.

#### Stage A: Data Structures and Runtime Foundation
- [x] Port `REPUTATION_DATA`, `REPUTATION_INDEX_DATA`, `REPUTATION_INDEX_RANK_DATA`, and `MOB_REPUTATION_DATA` into `merc.h`.
- [x] Add `LLIST *reputations` to `CHAR_DATA` and `MOB_REPUTATION_DATA *mob_reputations` to `MOB_INDEX_DATA`.
- [x] Add `src/reputation.c` with runtime lookup/update helpers and player-facing `reputations` command.

#### Stage B: Persistence and Boot Integration
- [x] Area reputation index load/save in legacy area pipeline (`olc_save.c`).
- [x] Area reputation index load/save in JSON area pipeline (`io/json/json_area.c`).
- [x] Character reputation load/save in JSON character pipeline (`io/json/json_char.c`).
- [x] Global reputation hash normalization in boot globals (`db.c`).

#### Stage C: OLC Editor Integration (Current Focus)
- [x] Add framework editor `editors/reputation/repedit.c` with baseline commands (`list`, `show`, `create`, `name`, `description`, `comments`, `flags`, `initial`, `token`, `rank`).
- [x] Wire editor constants/command registration (`olc.h`, `olc.c`, `interp.c`).
- [x] Ensure build inclusion in both `CMakeLists.txt` and `Makefile`.

#### Stage D: Donor-Parity System Integrations (Required After Editor Baseline)

The donor branch (`src_20_dev`) uses reputation broadly; editor + persistence are not sufficient for gameplay parity.

- [ ] **Mobile OLC hooks**
    - Port `medit addreputation` / `medit delreputation` to maintain `MOB_REPUTATION_DATA` from OLC.
    - Files: `editors/mobiles/medit.c`, `olc_act.c`.
    - Status: `medit` command hooks implemented in framework editor (`editors/mobiles/medit.c`) with recycle support (`mem.c`/`recycle.h`), plus mobile reputation persistence in legacy/JSON area save-load and runtime resolve/copy (`olc_save.c`, `io/json/json_area.c`, `db.c`).

- [ ] **Combat and encounter effects**
    - Apply kill/event-based gains and hostile/peaceful rank behavior checks.
    - Files: `fight.c`, `skills.c`, `reputation.c` (event helpers).

- [ ] **Quest rewards/gating**
    - Reputation modifications and requirement checks in quest acceptance/completion paths.
    - Files: `quest.c`.

- [ ] **Shops and stock gating**
    - Enforce rank/standing checks on shop listings and purchases.
    - Files: `act_obj.c`, shop/stock handlers.
    - Status: baseline gating backported for shopkeeper access and stock visibility/purchase checks (`act_obj.c`) with persisted shop/stock reputation requirements in both legacy and JSON area pipelines (`olc_save.c`, `io/json/json_area.c`) and post-load resolution (`db.c`).

- [ ] **Scripting integration**
    - Add script-side conditions/actions that read/write reputation and rank.
    - Files: `script_*` modules, script command tables.
    - Status: initial ifcheck hook `hasreputation` added (lookup + existence check by vnum/widevnum string), and script currency mutation now supports `award/ deduct reputation` plus `paragon` variants in `script_commands.c`.

- [ ] **DB resolve/fix-up pass**
    - Ensure post-load pointer resolution for reputation references mirrors donor behavior.
    - Files: `db.c` and related load finalization paths.
    - Status: current pass resolves mob/shop/stock/trainer-entry reputation references and copies prototype mob reputation rewards into live NPC instances for safe script/runtime mutation.

#### Stage E: Validation
- [ ] Build debug + tests-enabled configurations.
- [ ] Run unit/integration test suites and targeted manual OLC validation (`repedit`, area save/reload, player save/reload, mob reputation grants).

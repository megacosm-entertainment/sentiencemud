# Plan: I/O Subsystem Refactoring

**Status:** In Progress / Planned (verified 2026-02-26 docs audit)


This document outlines the plan to refactor the Sentience MUD I/O subsystems. The primary goal is to centralize all data persistence and caching logic into a new `src/io` directory, improve code organization, reduce redundancy, and pave the way for migrating remaining legacy systems to JSON.

## 1. Audit Findings

An audit of the codebase revealed three main categories of I/O operations:

### 1.1. Modern JSON I/O
- A significant number of `json_*.c` files (`json_char.c`, `json_area.c`, etc.) handle serialization for most major game data structures.
- The Redis caching system (`redis_cache.c`, `async_cache.c`) also uses the Jansson library for JSON manipulation.
- This code is generally well-structured but is scattered across the `src/` directory.

### 1.2. Legacy I/O (`.dat`, `.are`, `.pfile`)
- **`save.c` & `db.c`**: These are central hubs for character, account, and area loading/saving. They contain complex logic with fallbacks to older formats.
- **`olc_save.c`**: Contains code for writing the legacy `.are` format for areas.
- **`.dat` Files**: Multiple systems still rely on custom `.dat` file formats. Many of these are now purely legacy, with their functionality having been merged into the area-saving mechanisms. Key legacy files include:
  - `ships.dat`
  - `dungeons.dat`
  - `blueprints.dat`
  - `projects.dat`
  - `staff.dat`
  - `socials.dat`
  - `persist.dat`

### 1.3. Caching I/O
- **`redis_cache.c`**: Handles direct interaction with the Redis server for caching character and account data.
- **`async_cache.c`**: Manages a background thread for non-blocking cache operations (dumping to/loading from disk).

### 1.4. Existing `/src/io` Directory
- A `/src/io` directory already exists, containing `common.c` and `common.h`. These files appear to be from a previous refactoring attempt and will be evaluated for integration or deprecation.

## 2. Refactoring Plan

The refactoring will be executed in phases to ensure stability.

### Phase 1: Directory Restructuring & Relocation

The primary goal of this phase is to reorganize files without changing functionality.

#### 2.1. New Directory Structure
The existing `src/io/` directory will be utilized and expanded with the following subdirectories:
- `src/io/json/`: For pure JSON serialization/deserialization logic.
- `src/io/legacy/`: For functions that read/write old `.dat`, `.are`, and player file formats.
- `src/io/cache/`: For Redis and asynchronous caching logic.

#### 2.2. File Relocation Strategy
- **`src/io/json/`**: All existing `json_*.c` and `json_*.h` files will be moved here.
- **`src/io/cache/`**: `redis_cache.c`, `async_cache.c`, and their headers will be moved here.
- **`src/io/legacy/`**: New files will be created here (e.g., `legacy_area_io.c`, `legacy_player_io.c`) to house the legacy file format functions currently residing in `olc_save.c`, `save.c`, `db2.c`, `dungeon.c`, `blueprint.c`, and `boat.c`.
- **`src/io/`**: Core entry-point files like `save.c` will be refactored and moved here (e.g., as `io_char.c`, `io_account.c`), serving as the primary interface for their respective data types. The existing `common.c` and `common.h` will be analyzed and their functionality merged into other `io` files or deprecated.

#### 2.3. Build System Updates
Throughout the relocation process, `src/Makefile` and `src/CMakeLists.txt` will be incrementally updated to reflect the new file locations, ensuring the project remains buildable. All relevant `#include` paths in the source code will also be updated.

### Phase 2: Legacy System Migration

Following the successful reorganization, the next phase will be to migrate the remaining `.dat`-based systems to the modern JSON framework. The priority candidates identified in the audit are:
1.  Projects (`projects.dat`)
2.  Staff (`staff.dat`)
3.  Socials (`socials.dat`)

Each system will be migrated one at a time, following the established patterns in the `src/io/json/` directory.

The document has been created. What is our next step?

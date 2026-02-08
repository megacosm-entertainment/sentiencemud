# Plan: Persistence Layer Refactor

This document outlines the plan to refactor the MUD's system for handling **persistent live world state** (for rooms, mobiles, and objects). The goal is to replace the monolithic `data/world/persist.dat` file with a modern, scalable, and robust system using individual JSON files for storage and Redis for caching.

**Note on Scope:** This plan specifically covers the saving and loading of *runtime state changes* to individual entities. It does **not** cover the saving of area files (`.are`), which are OLC-modified templates. Area files should be saved as a single, cohesive unit, though this operation can and should also be offloaded to a background worker to prevent blocking the main game loop.

This approach is modeled on the asynchronous "write-back" pattern used for character persistence.

---

## Proposed Architecture

**Architectural Prerequisite:** This plan leverages and depends on the ongoing effort to separate static template data from live, runtime data for all entity types. For rooms specifically, this means separating the `ROOM_INDEX_DATA` (the template loaded from an area file) from the live room state. This separation is what makes it clean and efficient to identify and save only the persistent *changes* (the "delta") for a given room.

1.  **Persistent Storage (JSON Files):** The single `persist.dat` will be replaced by a structured directory tree. Each entity with persistent changes will get its own JSON file. This makes the data human-readable, easy to manage with `git`, and eliminates merge conflicts.
    *   `data/persist/rooms/<unique_instance_id>.json`
    *   `data/persist/mobiles/<uid>.json`
    *   `data/persist/objects/<uid>.json`

2.  **Caching & Write-Back Queue (Redis):** Redis serves two critical functions:
    *   **Read Cache:** Acts as a fast read-through cache to minimize disk I/O.
    *   **Write-Back Queue:** It holds the "dirty" data that needs to be written to disk, completely decoupling the main game loop from file I/O latency.

---

## Data Flow

### Read Path (e.g., loading a room's state)

1.  The game engine requests the persistent state for a room.
2.  It first queries Redis for the key `persist:room:<id>`.
3.  **Cache Hit:** If the key exists, Redis returns the JSON data instantly. The game parses it and applies the state.
4.  **Cache Miss:** If the key doesn't exist, the game falls back to reading `data/persist/rooms/<id>.json` from the disk. After loading, it pushes the data into Redis with an expiration time (TTL), so the next request will be a fast cache hit.

### Write Path (Asynchronous)

1.  **Update Redis:** When an in-memory object's persistent state changes, the game logic generates a JSON delta of the changes. This JSON data is written directly to the appropriate key in Redis (e.g., `persist:room:<id>`).
2.  **Queue Save Job:** The Redis key is then pushed into a dedicated "dirty queue" list within Redis (e.g., via `LPUSH dirty_keys persist:room:<id>`). The main game thread's work is now done, making the save operation feel instantaneous.
3.  **Process Queue:** A separate background worker thread, running continuously since boot, uses a blocking command (`BRPOP`) to pull a key from the `dirty_keys` queue.
4.  **Write to Disk:** Upon receiving a key, the worker thread reads the full JSON data from that key in Redis and writes it to the corresponding file on disk (e.g., `data/persist/rooms/<id>.json`).

---

## Phased Implementation Plan

This refactor will be conducted in phases to minimize risk and ensure stability.

### Phase 1: JSON Backend & Migration

1.  **Reverse-Engineer `persist.dat`:** The file format is understood, making migration feasible.
2.  **Create Directory Structure:** Create the new `data/persist/`, `data/persist/rooms/`, etc. directories.
3.  **Develop Migration Script:** A one-time utility script will read `persist.dat` and create the new individual JSON files. This script must correctly identify and use the unique instance ID for each entity.
4.  **Implement JSON Loaders:** New C functions will be written to handle reading the individual JSON files from disk and applying their state to in-memory game objects. **At this stage, saving to disk will not be implemented in the main thread.**

### Phase 2: Asynchronous Write & Caching Implementation

This phase implements the core of the new system.

1.  **Implement Redis "Dirty" Writers:** Create the C functions that will be called when a persistent entity needs saving. These functions will perform two steps:
    a. Write the entity's JSON delta to its primary Redis key (e.g., `persist:room:<id>`).
    b. Push the key name into the `dirty_keys` list in Redis.
2.  **Create Background Worker:** Implement a new thread that starts on boot. This thread will connect to Redis and enter a loop that waits on a `BRPOP` command on the `dirty_keys` list.
3.  **Implement Worker's Save-to-Disk Logic:** When the worker pops a key from the queue, it will read the associated value (the JSON data) from Redis and write it to the appropriate file on disk.
4.  **Implement Read-Through Cache:** The JSON loading functions from Phase 1 will be wrapped with Redis logic. Before reading from disk, they will check Redis. On a cache miss, they will read the disk and then populate the Redis cache.
5.  **Integrate:** The old "save" triggers in the code will be changed to call the new "dirty writer" functions from Step 1.

### Phase 3: Deprecation

1.  **Update Boot Sequence:** The server boot process will be changed to no longer call the old `persist_load` function.
2.  **Remove Old Code:** Once the new system is stable and verified, the legacy code for reading and writing `persist.dat` will be removed entirely.
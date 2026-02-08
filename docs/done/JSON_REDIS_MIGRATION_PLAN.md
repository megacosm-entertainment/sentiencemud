# JSON + Redis Character Storage Migration Plan

## Executive Summary

**Current Problem**: Loading 3,271 objects takes 340ms from disk, causing lag for all users. Character list display requires loading full character data even though only basic info is needed.

**Solution**: Migrate to JSON file format with Redis caching and lazy loading.

**Expected Performance**:
- Character list display: **1000ms → 5ms** (200x faster!)
- Full character login: **340ms → 50-100ms** (3-4x faster)
- Character save: **95ms → instant** (async write)

---

## Architecture

### Storage Layers

```
┌─────────────┐
│ Game Memory │ ← Active character data (CHAR_DATA, OBJ_DATA)
└──────┬──────┘
       │
┌──────▼──────┐
│ Redis Cache │ ← Hot data, TTL-based, key-value store
└──────┬──────┘   Keys: char:{name}:{section}
       │
┌──────▼──────┐
│ JSON Files  │ ← Cold storage, human-readable, version-controlled
└─────────────┘   Path: characters/{initial}/{name}/{section}.json
```

### Data Flow

**Load (Optimized)**:
1. Check Redis: `GET char:elzamine:info`
2. If hit → return (5ms)
3. If miss → Load from JSON → Cache in Redis → return (100ms first time)

**Save (Async)**:
1. Update game memory (instant)
2. Update Redis (5ms, synchronous)
3. Queue JSON write (background thread, non-blocking)
4. JSON write happens asynchronously (no player lag)

**Reconnect**:
1. Check Redis for active character (instant)
2. Transfer descriptor (no disk I/O!)
3. **Result**: Zero lag reconnects!

---

## JSON File Structure

### Modular Files (Lazy Loading)

```
characters/e/Elzamine/
├── metadata.json       # Version, timestamps, account link
├── character.json      # Basic info (ALWAYS loaded for menu)
├── inventory.json      # Lazy-loaded on game entry
├── locker.json         # Lazy-loaded on locker access
├── equipment.json      # Loaded on game entry
├── skills.json         # Loaded on game entry
└── affects.json        # Loaded on game entry
```

### File Sizes (Example: Elzamine with 3,271 objects)

| File | Size | When Loaded | Current Load Time |
|------|------|-------------|-------------------|
| `metadata.json` | ~500B | Account menu | ~1ms |
| `character.json` | ~2KB | Account menu | ~5ms |
| `inventory.json` | ~800KB | Game entry | ~50ms |
| `locker.json` | ~700KB | Locker access | ~50ms |
| `equipment.json` | ~20KB | Game entry | ~5ms |
| `skills.json` | ~10KB | Game entry | ~5ms |
| `affects.json` | ~5KB | Game entry | ~2ms |

**Total for character list**: ~2.5KB, ~5ms (vs 340ms current!)
**Total for full login**: ~1.5MB, ~100ms (vs 340ms current)

---

## JSON Format Examples

### metadata.json
```json
{
  "format_version": 2,
  "created": "2024-10-15T12:34:56Z",
  "last_saved": "2026-01-02T09:55:45Z",
  "account": {
    "name": "PlayerAccount",
    "id": [123456, 789012]
  },
  "migration_status": {
    "from_old_format": true,
    "migrated_at": "2026-01-02T10:00:00Z"
  }
}
```

### character.json (SMALL - for account menu)
```json
{
  "name": "Elzamine",
  "level": 60,
  "tot_level": 180,
  "race": "human",
  "class": ["warrior", "mage", "cleric"],
  "remorts": 2,
  "sex": 1,
  "body_type": 1,
  "pronouns": {
    "subject": "he",
    "object": "him",
    "possessive": "his",
    "possessive_standalone": "his",
    "reflexive": "himself"
  },
  "stats": {
    "perm": {"str": 18, "int": 13, "wis": 14, "dex": 16, "con": 17},
    "mod": {"str": 5, "int": 2, "wis": 0, "dex": 3, "con": 1}
  },
  "vitals": {
    "health": {"current": 1234, "max": 1500},
    "mana": {"current": 500, "max": 800},
    "move": {"current": 600, "max": 700}
  },
  "position": {
    "room_vnum": 3001,
    "wilds": null
  },
  "alignment": 500,
  "gold": 15000,
  "silver": 250,
  "experience": 1234567,
  "title": "the Legendary Warrior",
  "description": "A tall, muscular warrior...",
  "last_played": 1735819200,
  "creation_date": 1634567890,
  "played_hours": 1234,
  "flags": {
    "plr": [2097152],  // PLR_NOSUMMON
    "comm": [1],       // COMM_PROMPT
    "imm": [0],
    "res": [0],
    "vuln": [0]
  }
}
```

### inventory.json (LARGE - lazy-loaded)
```json
{
  "items": [
    {
      "vnum": 100034,
      "id": [2416474, 0],
      "short_descr": "Box of Secrets",
      "description": "A mysterious box sits here.",
      "extra": [0, 0, 0, 0],
      "wear_loc": "WEAR_NONE",
      "level": 50,
      "weight": 100,
      "condition": 100,
      "cost": 5000,
      "timer": 0,
      "values": [0, 0, 0, 0, 0, 0, 0, 0],
      "affects": [],
      "extra_descr": [],
      "contains": [
        {
          "vnum": 1354,
          "id": [2419676, 0],
          "short_descr": "an ivy whip",
          "nest": 1,
          "level": 45,
          "weight": 50
        }
      ]
    }
  ]
}
```

### locker.json (LARGE - lazy-loaded)
```json
{
  "items": [
    {
      "vnum": 8069,
      "id": [2419677, 0],
      "short_descr": "a panther statue",
      "contains": []
    }
  ]
}
```

### equipment.json (MEDIUM - loaded on entry)
```json
{
  "worn": [
    {
      "slot": "WEAR_LIGHT",
      "vnum": 1200,
      "id": [2419800, 0],
      "short_descr": "a glowing orb"
    },
    {
      "slot": "WEAR_HEAD",
      "vnum": 5100,
      "id": [2419801, 0],
      "short_descr": "a steel helmet"
    }
  ]
}
```

### skills.json (MEDIUM - loaded on entry)
```json
{
  "skills": {
    "sword": 95,
    "shield block": 80,
    "second attack": 100,
    "enhanced damage": 90
  },
  "songs": {
    "lullaby": 75
  }
}
```

---

## Redis Key Structure

### Key Naming Convention
```
char:{name}:{section}:{subsection}
```

### Keys and TTL

| Key | Data Type | TTL | Size | Purpose |
|-----|-----------|-----|------|---------|
| `char:elzamine:info` | Hash | 24h | ~2KB | Character list display |
| `char:elzamine:inventory` | JSON String | 1h | ~800KB | Full inventory data |
| `char:elzamine:locker` | JSON String | 1h | ~700KB | Locker data |
| `char:elzamine:equipment` | JSON String | 1h | ~20KB | Equipped items |
| `char:elzamine:skills` | Hash | 1h | ~10KB | Skill percentages |
| `char:elzamine:active` | Flag | 30min | 1B | Is character logged in? |

### Cache Invalidation

**On Save**:
- Update all relevant keys
- Reset TTL to maximum
- Queue async JSON write

**On Logout**:
- Keep cache for 1 hour (fast re-login)
- Mark as inactive: `DEL char:elzamine:active`

**On Login**:
- Mark as active: `SET char:elzamine:active 1 EX 1800`
- Load missing sections from JSON if needed

---

## Implementation Phases

### Phase 0: Prerequisites (1-2 hours)
- [ ] Install Redis server
- [ ] Add Redis client library (hiredis or similar)
- [ ] Create Redis connection pool in `merc.h`
- [ ] Test basic Redis operations

### Phase 1: Info Caching (Quick Win - 4-6 hours)

**Goal**: Speed up account menu character list display

**Tasks**:
1. Create `redis_cache.c/h` module
2. Implement functions:
   - `redis_get_char_info(char *name)` → Returns basic character data
   - `redis_set_char_info(CHAR_DATA *ch)` → Caches basic info
3. Modify `display_account_menu()` to use cached info instead of loading full character
4. Fall back to disk load if cache miss

**Expected Improvement**: Character list display 1000ms → 5ms (200x faster!)

**Files Modified**:
- New: `redis_cache.c`, `redis_cache.h`
- Modified: `nanny.c` (account menu display)
- Modified: `save.c` (cache on save)

### Phase 2: JSON Format Support (Medium - 2-3 days)

**Goal**: Add JSON serialization while maintaining backward compatibility

**Tasks**:
1. Create `json_char.c/h` module
2. Implement functions:
   - `char_to_json(CHAR_DATA *ch)` → Serialize to JSON
   - `json_to_char(char *json, CHAR_DATA *ch)` → Deserialize from JSON
3. Support dual-format:
   - Read: Try JSON first, fall back to old format
   - Write: Write both formats during transition
4. Add version field to detect format

**Expected Improvement**: Faster parsing, easier debugging

**Files Modified**:
- New: `json_char.c`, `json_char.h`
- Modified: `save.c` (dual-format support)

### Phase 3: Modular JSON Files (Medium - 2-3 days)

**Goal**: Split character data into separate files for lazy loading

**Tasks**:
1. Modify `char_to_json()` to write separate files:
   - `character.json`, `inventory.json`, `locker.json`, etc.
2. Implement lazy-loading functions:
   - `load_char_info_only(name)` → Just basic info
   - `load_char_inventory(ch)` → Load inventory section
   - `load_char_locker(ch)` → Load locker section
3. Modify account menu to use `load_char_info_only()`

**Expected Improvement**: Character list display 5ms (cached) or 10ms (uncached from JSON)

**Files Modified**:
- Modified: `json_char.c` (modular files)
- Modified: `nanny.c` (lazy loading)
- Modified: `storage.c` (lazy-load locker on access)

### Phase 4: Full Redis Integration (Large - 3-5 days)

**Goal**: Cache all character data in Redis with lazy loading

**Tasks**:
1. Implement Redis cache for all sections:
   - `redis_get_char_inventory(name)`
   - `redis_get_char_locker(name)`
   - `redis_set_char_inventory(name, data)`
2. Modify load flow:
   - Check Redis → If miss, load JSON → Cache in Redis
3. Modify save flow:
   - Update game memory → Update Redis → Queue JSON write

**Expected Improvement**: Full login 340ms → 50-100ms (3-4x faster)

**Files Modified**:
- Modified: `redis_cache.c` (full caching)
- Modified: `save.c` (Redis-first save)
- Modified: `nanny.c` (Redis-first load)

### Phase 5: Async Writes (Polish - 1-2 days)

**Goal**: Eliminate save lag by writing to disk asynchronously

**Tasks**:
1. Create background thread pool for JSON writes
2. Queue save operations instead of blocking
3. Add write confirmation tracking
4. Handle write failures gracefully

**Expected Improvement**: Save lag 95ms → instant

**Files Modified**:
- New: `async_io.c`, `async_io.h`
- Modified: `save.c` (async writes)

### Phase 6: Migration Tool (1-2 days)

**Goal**: Migrate existing character files to JSON format

**Tasks**:
1. Create standalone migration tool
2. Read old format → Write JSON format
3. Validate data integrity
4. Batch migrate all characters
5. Keep old files as backup

**Expected Improvement**: All characters using new format

**Files Modified**:
- New: `tools/migrate_to_json.c`

---

## Testing Strategy

### Unit Tests
- [ ] JSON serialization/deserialization
- [ ] Redis cache hit/miss behavior
- [ ] Lazy loading logic
- [ ] Async write queue

### Integration Tests
- [ ] Load character from old format
- [ ] Load character from JSON format
- [ ] Save character to both formats
- [ ] Reconnect with cached data
- [ ] Cache expiration and refresh

### Performance Tests
- [ ] Benchmark character list display (target: <10ms)
- [ ] Benchmark full character load (target: <100ms)
- [ ] Benchmark character save (target: <10ms perceived)
- [ ] Stress test: 100 concurrent logins

### Data Integrity Tests
- [ ] Verify object counts match
- [ ] Verify UIDs preserved
- [ ] Verify nested container structure
- [ ] Compare old vs new format output

---

## Rollback Strategy

**Phase 1-2** (Dual Format):
- Both formats written simultaneously
- Can revert to old code instantly
- Data never lost

**Phase 3+** (JSON Only):
- Keep old format files as `.old` backup
- Migration tool can reverse process
- Redis is cache only (not source of truth)

---

## Performance Targets

| Operation | Current | Phase 1 | Phase 3 | Phase 4 | Target |
|-----------|---------|---------|---------|---------|--------|
| Character list display | 1000ms | 5ms | 5ms | 5ms | <10ms |
| First login (cold) | 340ms | 340ms | 100ms | 50ms | <100ms |
| Reconnect (warm) | 340ms | 5ms | 5ms | 1ms | <10ms |
| Character save | 95ms | 95ms | 95ms | 10ms | <20ms |
| Locker access | 0ms* | 0ms* | 50ms | 5ms | <20ms |

*Currently loads with character, not lazy

---

## Resource Requirements

### Redis Memory Estimate

**Per Character** (3,000 objects average):
- Info: 2KB
- Inventory: 800KB
- Locker: 700KB
- Equipment: 20KB
- Skills: 10KB
- **Total**: ~1.5MB

**For 1000 Characters** (all cached):
- 1.5GB RAM

**Recommendations**:
- Start with 4GB Redis instance
- Use LRU eviction policy
- Monitor with `redis-cli INFO memory`

### Disk Space

**JSON vs Old Format**:
- Old format: ~800KB per character (text)
- JSON format: ~1.5MB per character (formatted, readable)
- **Increase**: ~2x disk usage

**Mitigation**:
- Compress old files after migration (`.gz`)
- Delete old files after 30-day validation period

---

## Success Metrics

### Performance
- ✅ Character list loads in <10ms (200x improvement)
- ✅ Character login in <100ms (3x improvement)
- ✅ Saves feel instant (<20ms perceived lag)
- ✅ Reconnects near-instant (cache hit)

### Reliability
- ✅ Zero data loss during migration
- ✅ 99.9% cache hit rate for active players
- ✅ Graceful degradation if Redis unavailable

### Developer Experience
- ✅ JSON files human-readable for debugging
- ✅ Easy to write admin tools (parse JSON)
- ✅ Version control friendly (diff-able)

---

## Next Steps

1. **Review this plan** - Get feedback on architecture
2. **Install Redis** - Set up development environment
3. **Phase 0** - Create Redis connection infrastructure
4. **Phase 1** - Quick win with info caching
5. **Iterate** - Measure, optimize, repeat

---

**Status**: 📋 **DESIGN COMPLETE - READY FOR IMPLEMENTATION**
**Created**: 2026-01-02
**Author**: Claude Code
**Estimated Timeline**: 2-3 weeks for full implementation
**Quick Win Timeline**: Phase 1 in 1 day

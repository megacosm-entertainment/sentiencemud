# TODO: Logging Call-Site Migration

**Status:** Audit complete — migration not yet started (2026-03-12)

This tracks the two-phase migration:
1. **Collapse the wrapper stack** — delete all intermediate layers (see `PLAN_LOGGING_UNIFIED_ERROR_HANDLING.md`)
2. **Enrich call sites** — migrate to `log_emit_event()` with structured context

The target is that every event reaches `log_emit_event()` in one hop, with no
intermediate `_log_message`, `log_message_f`, `log_string`, `bug`, or `p*f` wrapper.

See `PLAN_LOGGING_UNIFIED_ERROR_HANDLING.md` for design and API layers.
See `PLAN_LOGGING_AGGREGATORS.md` for stream architecture.

---

## Wrapper Deletion Checklist

All of these are being **removed**. Call sites get updated to `log_emit_event()`
or the new single-hop `p*f` macros (which call `log_emit_event()` directly).

### db.c — lose caller file/line, all levels
- [ ] `log_string()` / `log_stringf()` — 367 + 587 call sites → `plog()` / `plogf()` (mechanical)
- [ ] `bug()` — 35 call sites → `pbugf()` (mechanical, drop `fpArea` cruft)

### log.c — dead internal functions once macros are rewritten
- [ ] `_log_message()` / `_log_message_f()`
- [ ] `_log_stacktrace()` / `_log_stacktrace_f()`

### log.h — intermediate macros, no callers once log.c internals are gone
- [ ] `log_message()` / `log_message_f()`

### merc.h — old p* macros (rewritten to call log_emit_event directly, old callers updated)
- [ ] `plog()` / `plogf()`
- [ ] `pwarn()` / `pwarnf()`
- [ ] `perr()` / `perrf()`
- [ ] `pbug()` / `pbugf()`
- [ ] `pdebug()` / `pdebugf()`

---

## Call-Site Totals (as of 2026-03-12)

| Macro / Function | Count | Migration action |
|------------------|------:|-----------------|
| `pbugf()`        | 1364  | Rewrite macro → `log_emit_event`; enrich high-value sites |
| `log_stringf()`  |  587  | Replace with `plogf()` (mechanical); enrich high-value sites |
| `log_string()`   |  367  | Replace with `plog()` (mechanical); enrich high-value sites |
| `wiznet()`       |  161  | Replace with `log_emit_event()` + `staff_message` + `wiznet_flag` |
| `plogf()`        |  132  | Rewrite macro → `log_emit_event`; enrich high-value sites |
| `perrf()`        |   73  | Rewrite macro → `log_emit_event`; enrich high-value sites |
| `bug()`          |   35  | Replace with `pbugf()` then delete `bug()` |
| `pwarnf()`       |   22  | Rewrite macro → `log_emit_event`; enrich high-value sites |
| `plog()`         |   13  | Rewrite macro → `log_emit_event`; enrich high-value sites |
| `pdebugf()`      |    9  | Rewrite macro → `log_emit_event`; enrich high-value sites |
| `perr()`         |    5  | Rewrite macro → `log_emit_event` |
| `pbug()`         |    5  | Rewrite macro → `log_emit_event` |
| `pwarn()`        |    0  | Rewrite macro only (unused) |
| `pdebug()`       |    0  | Rewrite macro only (unused) |

**Already using `log_emit_event()` directly:** `fight.c` (combat events), `log.c` (internal)

**Total legacy call sites requiring attention: ~2773**

---

## Migration Tiers

### Tier 1 — `wiznet()` calls (161 sites) — HIGH PRIORITY

These are **not** currently wired to `log_emit_event()`. They must be replaced
with `log_emit_event()` calls that set `staff_message` so the event fans out to
both wiznet and the Redis Stream.

Priority order matches `PLAN_LOGGING_AGGREGATORS.md` Phase 4:

- [x] **`act_wiz.c`** (34 calls) — immortal commands; high audit value
- [x] **`scripts.c`** (21 calls) — script execution events (active calls migrated)
- [x] **`script_expand.c`** (21 calls) — script expansion errors (references are currently in commented debug blocks)
- [x] **`act_move.c`** (19 calls) — movement events (currently commented debug references)
- [x] **`nanny.c`** (5 calls) — login/creation security events
- [x] **`account/penalty.c`** (4 calls) — moderation events
- [x] **`act_obj.c`** (5 calls) — item events
- [x] **`act_comm.c`** (4 calls) — communication events
- [x] **`db.c`** (8 calls) — system/persistence events (active calls migrated; remaining references are commented debug traces)
- [x] **`comm.c`** (7 calls) — connection lifecycle
- [x] **`wilds.c`** (5 calls) — wilderness events (references are currently in commented debug blocks)
- [x] **`fight.c`** (4 calls) — combat events (active calls migrated; includes structured combat event path)
- [x] **`update.c`** (3 calls) — tick events
- [x] Remaining files (misc, low count) — active direct `wiznet()` callsites migrated; remaining matches are comments or dispatcher internals

### Tier 2 — `log_string()` / `log_stringf()` high-value sites (954 total)

These already reach `log_emit_event` but carry no structured context. Prioritise
files where the data has dashboard / audit value:

- [ ] **`nanny.c`** — auth, creation, deletion paths → `LOG_SECURITY` / `LOG_INFO` with actor context
- [ ] **`account/penalty.c`** — moderation → `LOG_SECURITY` with actor + target + value
- [ ] **`save.c`** (35 + 46) — char save errors → `LOG_ERROR` with char name context (partial: JSON/cache save+load failure paths now emit structured context)
- [ ] **`church.c`** (44) — org operations
- [ ] **`fight.c`** (8) — damage/death events (partially done)
- [ ] **`io/cache/redis_cache.c`** (16 + 36) — connection errors → `LOG_ERROR` system context (partial: connect/auth/ping, full-char SET/JSON.SET serialize+write failures, cache-info failure, and partial-update failure paths now emit structured context)
- [ ] **`io/json/json_persist.c`** (17 + 44) — persistence errors (partial: init mkdir, object/mobile/room serialize failures, cached save serialize/dump failures, and persist-worker temp-file write/sync/rename failures now emit structured context)
- [ ] **`io/json/json_char.c`** (32) — character serialisation errors (partial: backup/create/serialize/write/rename failure paths now emit structured context)
- [ ] **`db.c`** (23 + 19) — core db errors
- [ ] **`connection_websocket.c`** (16 + 16) — connection errors (partial: ws/wss handshake, TLS setup, and read/write header/payload failure paths now emit structured context)
- [ ] **`scripts.c`** (177) — script execution (already partially enriched via `pbugf`)

### Tier 3 — `pbugf()` / `plogf()` context enrichment

These already flow through `log_emit_event` but emit only a plain message with
no structured `log_context_t`. High-value enrichment targets:

- [ ] **Script commands** (`script_commands.c` 228, `script_mpcmds.c` 182,
  `script_tpcmds.c` 169, `script_rpcmds.c` 152+1, `script_opcmds.c` 151) —
  add script name, trigger type, mob/room/obj vnum to `extra_json`
- [ ] **`save.c`** (53 pbugf) — add char name + vnum context
- [ ] **`olc_save.c`** (44 pbugf) — add area/zone context
- [ ] **`wilds.c`** (25 pbugf, 19 plogf, 13 perrf, 7 pwarnf) — add coords + region context
- [ ] **`blueprint.c`** (15 pbugf) — add blueprint id context
- [ ] **`act_wiz.c`** (16 pbugf, 10 plogf, 3 plog) — add immortal actor context
- [ ] **`update.c`** (15 pbugf) — add tick/pulse context

### Tier 4 — `bug()` (35 sites)

Old `bug()` call shim; replace with `pbugf()` with category at minimum.
Concentrated in `script_commands.c` (29).

---

## Migration Pattern

### Replacing `wiznet()` with `log_emit_event()`

```c
/* BEFORE */
wiznet("$N has logged in.", ch, NULL, WIZ_LOGINS, 0, 0);

/* AFTER */
log_event_t ev = {
    .severity      = EVENT_SEV_INFO,
    .category      = LOG_SECURITY,
    .plain_message = "character login",
    .staff_message = "$N has logged in.",
    .wiznet_flag   = WIZ_LOGINS,
    .context       = &(log_context_t){
        .actor_type = "char",
      .actor_name = ch->name,
      .actor_uid  = { ch->id[0], ch->id[1] },
        .action     = "login",
    },
    .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
};
log_emit_event(&ev, NULL);
```

### Adding context to `plogf()` / `pbugf()`

```c
/* BEFORE */
pbugf(LOG_SCRIPT, "Script error in mob %d: %s", mob->vnum, errmsg);

/* AFTER — inline context for high-value sites */
log_context_t ctx = {
    .actor_type = "npc",
  .actor_name = mob->short_descr,
  .actor_uid  = { mob->id[0], mob->id[1] },
  .actor_wnum = widevnum_string_mobile(mob->pIndexData, NULL),
    .action     = "script_error",
    .extra_json = jansson_build_extra(...),  /* or hand-built JSON string */
};
log_event_t ev = {
    .severity      = EVENT_SEV_BUG,
    .category      = LOG_SCRIPT,
    .plain_message = errmsg,
    .context       = &ctx,
    .source_file = __FILE__, .source_line = __LINE__, .source_func = __func__,
};
log_emit_event(&ev, NULL);
```

---

## Notes

- `wiznet()` replacement requires `log_emit_staff_event()` to be fully wired
  (see `PLAN_LOGGING_UNIFIED_ERROR_HANDLING.md` Layer 1). Confirm that path
  before starting Tier 1.
- Script command files (`script_*cmds.c`) represent ~800 of the 1364 `pbugf`
  calls. A helper macro for script context will save enormous repetition — do
  this before migrating those files.
- `log_string` / `log_stringf` shims remain in place through migration; no need
  to touch every call site at once.

# Logging Schema & Context Design

## Overview

This document defines the JSON schema for log entries sent to external aggregators, along with contextual data structures and example entries for each logging scenario.

## Core Log Entry Schema

Every log entry, when serialized to JSON, will contain:

```json
{
  "metadata": {
    "timestamp": "2026-01-24T14:32:15.234Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "nanny.c",
    "line": 105,
    "function": "login_get_account"
  },
  "message": {
    "level": "INFO",
    "category": "security",
    "text": "Account admin@192.168.1.1 has connected."
  },
  "context": {
    "actor_type": "account",
    "actor_name": "admin",
    "actor_uid": null,
    "actor_wnum": null,
    "action": "login",
    "target_type": null,
    "target_name": null,
    "target_uid": null,
    "target_wnum": null,
    "value": null,
    "duration_ms": null,
    "extra": null
  }
}
```

## Field Definitions

### metadata
- `timestamp` (ISO 8601): UTC timestamp when log was created
- `server_id` (string): identifier of the server instance (e.g., "na-1", "eu-1", "staging")
- `version` (int): schema version for breaking changes (current: 1)

### source
- `file` (string): source file name (from `__FILE__`)
- `line` (int): line number (from `__LINE__`)
- `function` (string): function name (from `__func__`)

### message
- `level` (enum): INFO, WARN, ERROR, DEBUG, CRITICAL, BUG, SQL, HTTP, SCRIPT, SECURITY
- `category` (string): zlog category (e.g., "security", "combat", "init", "debug", "sql", "http", "script")
- `text` (string): formatted message text

### context (optional, can be null)
- `actor_type` (string): who performed the action ("account", "player", "npc", "ip", "system")
- `actor_name` (string): display name of the actor (player `name`, NPC `short_descr`, IP address, component name)
- `actor_uid` (array[int64, int64], nullable): `[id[0], id[1]]` from CHAR_DATA — persistent unique instance IDs; null for non-character actors
- `actor_wnum` (string, nullable): widevnum of the NPC's mob index (e.g. `"3#1234"`); null for players and non-mob actors
- `action` (string): what the actor did ("login", "logout", "combat", "create", "delete", "move", "cast", etc.)
- `target_type` (string, nullable): type of thing acted upon ("player", "npc", "account", "room", "obj", "vault")
- `target_name` (string, nullable): display name of the target
- `target_uid` (array[int64, int64], nullable): `[id[0], id[1]]` of the target CHAR_DATA; null if target is not a character
- `target_wnum` (string, nullable): widevnum of NPC target's mob index; null for players and non-mob targets
- `value` (int64, nullable): numeric value (damage dealt, level, gold amount, etc.)
- `duration_ms` (int64, nullable): operation duration in milliseconds for perf analysis
- `extra` (object, nullable): arbitrary additional data (JSON object, variable per scenario)

## C Data Structure

```c
typedef struct {
    const char *actor_type;         /* "player", "npc", "account", "system", "ip" */
    const char *actor_name;         /* display name: short_descr for NPCs, name for players */
    unsigned long actor_uid[2];     /* ch->id[0], ch->id[1] — unique instance IDs */
    const char *actor_wnum;         /* widevnum string for NPCs (e.g. "3#1234"), NULL for players */
    const char *action;             /* "login", "combat", "create", "delete", "move", etc. */
    const char *target_type;        /* "char", "account", "room", "obj", "mob", "vault", NULL */
    const char *target_name;        /* display name: short_descr for NPCs, name for players */
    unsigned long target_uid[2];    /* victim->id[0], victim->id[1] */
    const char *target_wnum;        /* widevnum string for NPC targets, NULL for players */
    int64_t value;                  /* numeric value (damage, level, etc.), 0 for N/A */
    int64_t duration_ms;            /* duration in milliseconds, 0 for N/A */
    char *extra_json;               /* arbitrary JSON string for additional context, NULL for N/A */
} log_context_t;
```

## Per-Domain Extra Field Reference

Use this as the canonical guide for `context.extra` keys. Keep these keys stable to preserve query and dashboard compatibility.

### Security (`LOG_SECURITY`)
- Login/connect: `host`, `account_name`, `account_uid`, `login_method`, `auth_method`, `mfa_enabled`, `mfa_verified`
- Access control: `reason`, `ban_type`, `penalty_type`, `duration_secs`, `issuer_name`, `issuer_uid`
- Session state: `descriptor_id`, `reconnect`, `attempt_number`, `max_attempts`

### Admin (`LOG_ADMIN`)
- Command execution: `command`, `argument`, `channel`, `staff_rank`, `staff_trust`
- Mutations: `field`, `old_value`, `new_value`, `reason`
- Enforcement: `target_name_before`, `target_name_after`, `target_uid_before`, `target_uid_after`

### Combat (`LOG_COMBAT`)
- Hit events (implemented in `fight.c`): `zone`, `room_wvnum`, `room_name`, `attack_type`, `dam_type`, `dam_message`, `attacker_is_npc`, `victim_is_npc`
- Weapon enrichment: `weapon_wvnum`, `weapon_class`, `weapon_name`
- Optional high-value fields: `critical`, `immune`, `victim_hp_before`, `victim_hp_after`

### Script (`LOG_SCRIPT` or `LOG_SCRIPTS`)
- Execution identity: `script_type`, `script_vnum`, `script_name`, `trigger_type`, `trigger_phrase`
- Runtime failure: `error_type`, `error_message`, `error_line`, `line_text`
- Bindings: `mob_wvnum`, `obj_wvnum`, `room_wvnum`, `token_wvnum`

### OLC (`LOG_OLC`)
- Edit identity: `editor`, `area_wvnum`, `entity_type`, `entity_wvnum`
- Change details: `field`, `old_value`, `new_value`, `autosave`
- Workflow: `session_id`, `commit_note`, `review_required`

### Quest (`LOG_QUEST`)
- Progress: `quest_id`, `quest_name`, `stage`, `step`, `state`
- Rewards: `reward_xp`, `reward_gold`, `reward_items`
- Attribution: `giver_name`, `giver_wvnum`, `receiver_uid`

### Economy (`LOG_INFO` until dedicated category exists)
- Transaction: `transaction_type`, `transaction_id`, `amount`, `currency`
- Item detail: `item_wvnum`, `item_name`, `quantity`
- Market context: `shop_wvnum`, `auction_id`, `buyer_uid`, `seller_uid`

### System/Performance (`LOG_DEBUG`, `LOG_INIT`, `LOG_HTTP`, `LOG_SQL`)
- Operation timing: `component`, `operation`, `duration_ms`, `threshold_ms`
- Service health: `host`, `port`, `retry_count`, `backoff_ms`, `error`
- Batch telemetry: `queued`, `processed`, `dropped`, `fallback_written`


## Logging Categories & Example Entries

### 1. Security - Account Login

**Successful login:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T14:32:15.234Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "nanny.c",
    "line": 220,
    "function": "login_get_account_password"
  },
  "message": {
    "level": "INFO",
    "category": "security",
    "text": "Account admin@192.168.1.100 has connected."
  },
  "context": {
    "actor_type": "account",
    "actor_name": "admin",
    "action": "login",
    "target_type": null,
    "target_name": null,
    "value": null,
    "duration_ms": null,
    "extra": {
      "host": "192.168.1.100",
      "auth_method": "password",
      "mfa_enabled": true,
      "mfa_verified": true,
      "last_login": "2026-01-23T10:15:00Z",
      "character_count": 3
    }
  }
}
```

**Failed login attempt:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T14:33:45.567Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "nanny.c",
    "line": 240,
    "function": "login_get_account_password"
  },
  "message": {
    "level": "WARN",
    "category": "security",
    "text": "Failed login attempt for account badguy@10.0.0.5 (bad password)"
  },
  "context": {
    "actor_type": "ip",
    "actor_name": "10.0.0.5",
    "action": "login_failed",
    "target_type": "account",
    "target_name": "badguy",
    "value": 2,
    "duration_ms": null,
    "extra": {
      "failure_reason": "bad_password",
      "attempt_number": 2,
      "max_attempts": 3,
      "auth_method": "password"
    }
  }
}
```

### 2. Combat

**Player vs player combat:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T15:10:22.891Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "fight.c",
    "line": 1823,
    "function": "one_hit"
  },
  "message": {
    "level": "INFO",
    "category": "combat",
    "text": "Nibelung hits Alice with a sword for 47 damage."
  },
  "context": {
    "actor_type": "player",
    "actor_name": "Nibelung",
    "actor_uid": [7823401982, 0],
    "actor_wnum": null,
    "action": "attack",
    "target_type": "player",
    "target_name": "Alice",
    "target_uid": [6234109823, 0],
    "target_wnum": null,
    "value": 47,
    "duration_ms": null,
    "extra": {
      "attacker_level": 120,
      "victim_level": 105,
      "weapon": "sword",
      "damage_type": "slash",
      "victim_health_before": 150,
      "victim_health_after": 103,
      "critical": false,
      "location": "VNUM:5001"
    }
  }
}
```

### 3. Character Actions

**Character creation:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T16:05:33.123Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "nanny.c",
    "line": 1200,
    "function": "finalize_character_creation"
  },
  "message": {
    "level": "INFO",
    "category": "info",
    "text": "New character created: Bobert (human, warrior)"
  },
  "context": {
    "actor_type": "account",
    "actor_name": "coolplayer",
    "action": "create_character",
    "target_type": "char",
    "target_name": "Bobert",
    "value": null,
    "duration_ms": 2340,
    "extra": {
      "race": "human",
      "class": "warrior",
      "level": 1,
      "alignment": "neutral",
      "host": "192.168.1.50",
      "character_count_after": 4
    }
  }
}
```

**Character deletion:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T16:15:00.456Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "nanny.c",
    "line": 1500,
    "function": "delete_character"
  },
  "message": {
    "level": "WARN",
    "category": "info",
    "text": "Character OldGuy marked for deletion."
  },
  "context": {
    "actor_type": "char",
    "actor_name": "OldGuy",
    "action": "delete",
    "target_type": "char",
    "target_name": "OldGuy",
    "value": 45,
    "duration_ms": null,
    "extra": {
      "level": 45,
      "playtime_hours": 234,
      "gold_on_char": 50000,
      "items_in_vault": 15,
      "delete_delay_days": 30
    }
  }
}
```

### 4. Administrative Actions

**Immortal using admin command:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T17:20:14.789Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "act_wiz.c",
    "line": 450,
    "function": "do_setstat"
  },
  "message": {
    "level": "WARN",
    "category": "security",
    "text": "Immortal Nibelung used setstat on Bobert: strength 18->20"
  },
  "context": {
    "actor_type": "char",
    "actor_name": "Nibelung",
    "action": "setstat",
    "target_type": "char",
    "target_name": "Bobert",
    "value": 20,
    "duration_ms": null,
    "extra": {
      "admin_level": "IMPLEMENTOR",
      "stat": "strength",
      "old_value": 18,
      "new_value": 20,
      "reason": "testing"
    }
  }
}
```

### 5. System/Performance Issues

**Slow operation warning:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T18:45:30.234Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "db.c",
    "line": 890,
    "function": "save_character"
  },
  "message": {
    "level": "WARN",
    "category": "debug",
    "text": "Slow database save operation."
  },
  "context": {
    "actor_type": "system",
    "actor_name": "character_save",
    "action": "save",
    "target_type": "char",
    "target_name": "Nibelung",
    "value": null,
    "duration_ms": 1250,
    "extra": {
      "threshold_ms": 500,
      "component": "json_persist",
      "operation": "json_dump_file",
      "file_size_kb": 145
    }
  }
}
```

**Redis connection issue:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T19:15:45.678Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "redis_cache.c",
    "line": 234,
    "function": "redis_get"
  },
  "message": {
    "level": "ERROR",
    "category": "http",
    "text": "Redis connection timeout."
  },
  "context": {
    "actor_type": "system",
    "actor_name": "redis",
    "action": "connect",
    "target_type": null,
    "target_name": null,
    "value": null,
    "duration_ms": 5000,
    "extra": {
      "host": "127.0.0.1",
      "port": 6379,
      "timeout_ms": 5000,
      "error": "Connection timed out",
      "retry_count": 3
    }
  }
}
```

### 6. Script Execution

**Script error:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T20:05:12.345Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "script_comp.c",
    "line": 567,
    "function": "execute_script"
  },
  "message": {
    "level": "ERROR",
    "category": "script",
    "text": "Script error in mob script: undefined variable 'foo'"
  },
  "context": {
    "actor_type": "system",
    "actor_name": "script_engine",
    "action": "execute",
    "target_type": "mob",
    "target_name": "6001",
    "value": null,
    "duration_ms": 45,
    "extra": {
      "script_type": "mob",
      "script_vnum": 6001,
      "script_event": "greet",
      "error_type": "undefined_variable",
      "error_line": 23,
      "line_text": "if (foo > 10)"
    }
  }
}
```

### 7. Item Transfer & Trading

**Item transfer between characters:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T21:30:22.567Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "trade.c",
    "line": 340,
    "function": "do_give"
  },
  "message": {
    "level": "INFO",
    "category": "info",
    "text": "Nibelung gave sword to Alice."
  },
  "context": {
    "actor_type": "char",
    "actor_name": "Nibelung",
    "action": "give",
    "target_type": "char",
    "target_name": "Alice",
    "value": null,
    "duration_ms": null,
    "extra": {
      "item_vnum": 5001,
      "item_name": "sword",
      "item_count": 1,
      "location": "VNUM:3001",
      "transaction_id": "txn_1234567890"
    }
  }
}
```

### 8. Vault/Storage Operations

**Item deposited to vault:**

```json
{
  "metadata": {
    "timestamp": "2026-01-24T22:10:45.890Z",
    "server_id": "na-1",
    "version": 1
  },
  "source": {
    "file": "storage.c",
    "line": 456,
    "function": "deposit_vault"
  },
  "message": {
    "level": "INFO",
    "category": "info",
    "text": "Item deposited to vault: gold coin x100"
  },
  "context": {
    "actor_type": "account",
    "actor_name": "coolplayer",
    "action": "vault_deposit",
    "target_type": "vault",
    "target_name": "na-1",
    "value": 100,
    "duration_ms": null,
    "extra": {
      "item_vnum": 5,
      "item_name": "gold coin",
      "quantity": 100,
      "vault_server": "na-1",
      "vault_weight_before": 240,
      "vault_weight_after": 340,
      "vault_items_before": 45,
      "vault_items_after": 46
    }
  }
}
```

## Query Examples for External Aggregators

### Find all failed logins from a specific IP (Elasticsearch/Kibana syntax)
```
level: "WARN" AND action: "login_failed" AND actor_name: "10.0.0.5"
```

### Find all PK deaths
```
category: "combat" AND action: "death" AND extra.victim_type: "player"
```

### Find slow database operations
```
category: "debug" AND duration_ms > 500 AND actor_name: "character_save"
```

### Audit trail for a specific player
```
actor_name: "Nibelung" OR target_name: "Nibelung"
```

### Audit trail by unique character ID (survives renames)
```
actor_uid.0: 7823401982 OR target_uid.0: 7823401982
```

### All NPC-sourced events (with mob index)
```
actor_wnum: * OR target_wnum: *
```

### System health metrics (errors in last hour)
```
category: "http" AND level: "ERROR" AND timestamp > "now-1h"
```

### All immortal commands
```
level: "WARN" AND category: "security" AND context.extra.admin_level: *
```

## Design Notes

1. **Backward Compatibility**: All context fields are optional (nullable). Logs without context remain valid.
2. **Extensibility**: The `extra` field allows arbitrary JSON for scenario-specific data without schema changes.
3. **Performance**: NULL context has minimal overhead; only computed when relevant.
4. **Aggregator Compatibility**: Schema designed for ELK Stack, Splunk, DataDog, Sumologic, etc.
5. **Searchability**: `actor_name`, `actor_uid`, `actor_wnum`, `action`, `target_name`, `target_uid`, `target_wnum`, `level`, and `category` are consistent for filtering. `actor_uid`/`target_uid` are stable across renames; `actor_wnum`/`target_wnum` identify NPC mob indexes.
6. **Compliance**: Timestamp in UTC ISO 8601 for audit requirements; server_id enables multi-tenant queries.

## Implementation Roadmap

See `PLAN_LOGGING_AGGREGATORS.md` for phased implementation starting with:
1. Define schema (this document) ✅
2. Extend logging signatures with optional context
3. Add JSON serialization in logging functions
4. Implement Redis queuing
5. Add logger thread and HTTP dispatch
6. Implement fallback disk buffering
7. Add dynamic configuration and admin commands

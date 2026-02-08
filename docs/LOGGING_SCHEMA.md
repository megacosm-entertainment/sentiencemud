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
    "actor_id": "admin",
    "action": "login",
    "target_type": null,
    "target_id": null,
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
- `actor_type` (string): who performed the action ("account", "char", "ip", "system", "mob", "npc")
- `actor_id` (string): identifier of the actor (username, char name, IP address, component name)
- `action` (string): what the actor did ("login", "logout", "combat", "create", "delete", "move", "cast", etc.)
- `target_type` (string, nullable): type of thing acted upon ("char", "account", "room", "obj", "mob", "vault")
- `target_id` (string, nullable): identifier of target
- `value` (int64, nullable): numeric value (damage dealt, level, gold amount, etc.)
- `duration_ms` (int64, nullable): operation duration in milliseconds for perf analysis
- `extra` (object, nullable): arbitrary additional data (JSON object, variable per scenario)

## C Data Structure

```c
typedef struct {
    const char *actor_type;      // "account", "char", "ip", "system", "mob", "npc"
    const char *actor_id;        // username, char name, IP, component name
    const char *action;          // "login", "combat", "create", "delete", "move", etc.
    const char *target_type;     // "char", "account", "room", "obj", "mob", "vault", NULL
    const char *target_id;       // target identifier, NULL for N/A
    int64_t value;               // numeric value (damage, level, etc.), 0 for N/A
    int64_t duration_ms;         // duration in milliseconds, 0 for N/A
    char *extra_json;            // arbitrary JSON string for additional context, NULL for N/A
} log_context_t;
```

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
    "actor_id": "admin",
    "action": "login",
    "target_type": null,
    "target_id": null,
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
    "actor_id": "10.0.0.5",
    "action": "login_failed",
    "target_type": "account",
    "target_id": "badguy",
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
    "actor_type": "char",
    "actor_id": "Nibelung",
    "action": "attack",
    "target_type": "char",
    "target_id": "Alice",
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
    "actor_id": "coolplayer",
    "action": "create_character",
    "target_type": "char",
    "target_id": "Bobert",
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
    "actor_id": "OldGuy",
    "action": "delete",
    "target_type": "char",
    "target_id": "OldGuy",
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
    "actor_id": "Nibelung",
    "action": "setstat",
    "target_type": "char",
    "target_id": "Bobert",
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
    "actor_id": "character_save",
    "action": "save",
    "target_type": "char",
    "target_id": "Nibelung",
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
    "actor_id": "redis",
    "action": "connect",
    "target_type": null,
    "target_id": null,
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
    "actor_id": "script_engine",
    "action": "execute",
    "target_type": "mob",
    "target_id": "6001",
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
    "actor_id": "Nibelung",
    "action": "give",
    "target_type": "char",
    "target_id": "Alice",
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
    "actor_id": "coolplayer",
    "action": "vault_deposit",
    "target_type": "vault",
    "target_id": "na-1",
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
level: "WARN" AND action: "login_failed" AND actor_id: "10.0.0.5"
```

### Find all PK deaths
```
category: "combat" AND action: "death" AND extra.victim_type: "player"
```

### Find slow database operations
```
category: "debug" AND duration_ms > 500 AND actor_id: "character_save"
```

### Audit trail for a specific player
```
actor_id: "Nibelung" OR target_id: "Nibelung"
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
5. **Searchability**: `actor_id`, `action`, `target_id`, `level`, and `category` are consistent for filtering.
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

# GMCP Phase 3: Extended Packages — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add four new Sentience GMCP packages — Client.Ready (capabilities + state), Char.Affects, Char.Enemies, and Room.Contents — with full test coverage.

**Architecture:** Extend `gmcp_sentience.h/c` with 5 new pure JSON builder functions (same pattern as Phase 1), add input structs and cache fields, wire update logic into `sentience_gmcp_update()`, and hook Client.Ready.Capabilities into the GMCP negotiation paths in `protocol.c` and `protocol_websocket.c`.

**Tech Stack:** C, Jansson (JSON), existing test framework (JSON test data + C handler)

**Spec:** `docs/superpowers/specs/2026-03-25-gmcp-phase3-design.md`

---

## File Map

| File | Action | Purpose |
|------|--------|---------|
| `gmcp_sentience.h` | Modify | Add input structs, builder declarations, cache fields |
| `gmcp_sentience.c` | Modify | Add 5 builder functions + update logic |
| `protocol.c` | Modify | Hook Capabilities send into GMCP negotiation |
| `protocol_websocket.c` | Modify | Hook Capabilities send into WebSocket auto-enable |
| `tests/unit/gmcp_sentience_tests.c` | Modify | Add 5 new scenario runners + dispatcher entries |
| `tests/data/unit/gmcp_sentience_unit_tests.json` | Modify | Add ~14 new test cases |

No new files. No build system changes (no new .c files).

---

### Task 1: Input Structs + Builder Declarations + Cache Fields

Add all Phase 3 type definitions to `gmcp_sentience.h`. This is pure header work — no implementation yet.

**Files:**
- Modify: `gmcp_sentience.h`

- [ ] **Step 1: Add input structs after the existing `sentience_room_input_t` (after line 148)**

Add these structs before the builder function declarations section:

```c
/*
 * Phase 3 input structs — Affects, Enemies, Room Contents
 */

/* Char.Affects input — one per active affect */
typedef struct {
    const char *name;           /* af->skill->name or af->custom_name */
    const char *wnum;           /* af->skill wnum string, NULL if none */
    int duration;               /* remaining ticks, -1 for permanent */
    int estimated_seconds;      /* duration * PULSE_TICK / PULSE_PER_SECOND, -1 if permanent */
    const char *modifier;       /* human-readable, e.g. "+2 strength"; NULL if APPLY_NONE/0 */
    int level;
} sentience_affect_input_t;

/* Char.Enemies input — one per enemy in combat */
typedef struct {
    const char *name;           /* short_descr for NPCs, name for players */
    unsigned long instance_id[2];
    int hp_pct;                 /* 0-100 */
    bool is_primary;            /* true if this is ch->fighting */
    const char *target;         /* "you" or target name */
} sentience_enemy_input_t;

/* Room.Contents entity (item, NPC, or player) */
typedef struct {
    const char *name;
    unsigned long instance_id[2];
    const char *short_desc;     /* NULL for players */
} sentience_room_entity_input_t;

/* Room.Contents door */
typedef struct {
    const char *direction;
    const char *state;          /* "open", "closed", "locked" */
    bool is_locked;
} sentience_room_door_input_t;

/* Room.Contents full snapshot */
typedef struct {
    const sentience_room_entity_input_t *items;
    int num_items;
    const sentience_room_entity_input_t *npcs;
    int num_npcs;
    const sentience_room_entity_input_t *players;
    int num_players;
    const sentience_room_door_input_t *doors;
    int num_doors;
} sentience_room_contents_input_t;
```

- [ ] **Step 2: Add builder function declarations after the existing `sentience_build_room_json` declaration (after line 150)**

```c
/*
 * Phase 3 JSON builder functions.
 */
json_t *sentience_build_client_ready_capabilities_json(void);
json_t *sentience_build_client_ready_state_json(int tick_rate, int pulse_per_second);
json_t *sentience_build_affects_json(const sentience_affect_input_t *affects, int num_affects);
json_t *sentience_build_enemies_json(const sentience_enemy_input_t *enemies, int num_enemies,
                                      long self_hp, long self_max_hp);
json_t *sentience_build_room_contents_json(const sentience_room_contents_input_t *data);
```

- [ ] **Step 3: Add cache extension fields to `sentience_gmcp_cache_t` (before the `initialized` field, around line 80)**

```c
    /* Phase 3: Room contents fingerprint (separate from Phase 1 room_id0/room_id1) */
    long contents_room_id[2];
    int contents_count;

    /* Phase 3: Affects transition detection */
    bool had_affects;

    /* Phase 3: Combat transition detection */
    bool was_fighting;
```

- [ ] **Step 4: Verify build compiles**

Run: `cd /sentience/src && ./build tests 2>&1 | tail -5`
Expected: Build succeeds (new declarations are unused, but that's fine — no `-Werror` on unused declarations)

- [ ] **Step 5: Commit**

```bash
git add gmcp_sentience.h
git commit -m "feat(gmcp): add Phase 3 input structs, builder declarations, and cache fields

Add sentience_affect_input_t, sentience_enemy_input_t,
sentience_room_entity_input_t, sentience_room_door_input_t,
sentience_room_contents_input_t structs for builder inputs.

Declare 5 new builders: client_ready_capabilities, client_ready_state,
affects, enemies, room_contents.

Extend sentience_gmcp_cache_t with contents fingerprint, had_affects,
and was_fighting fields for Phase 3 update logic.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 2: Test Infrastructure — JSON Test Data

Write all test cases in JSON before any builder implementation (TDD).

**Files:**
- Modify: `tests/data/unit/gmcp_sentience_unit_tests.json`

- [ ] **Step 1: Add Client.Ready test cases to the `tests` array**

Append these test objects to the existing `tests` array in the JSON file:

```json
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_client_ready_capabilities",
    "description": "Builds Sentience.Client.Ready.Capabilities with package list and features",
    "input": {
        "function": "build_client_ready_capabilities",
        "test_cases": [
            {
                "scenario": "default_capabilities",
                "params": {},
                "expected": {
                    "packages": [
                        "Sentience.Char.Identity",
                        "Sentience.Char.Vitals",
                        "Sentience.Char.Stats",
                        "Sentience.Char.Combat",
                        "Sentience.Char.Worth",
                        "Sentience.Char.Affects",
                        "Sentience.Char.Enemies",
                        "Sentience.Room.Info",
                        "Sentience.Room.Contents",
                        "Sentience.Link"
                    ],
                    "features": ["links", "osc8"]
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_client_ready_state",
    "description": "Builds Sentience.Client.Ready.State with tick rate info",
    "input": {
        "function": "build_client_ready_state",
        "test_cases": [
            {
                "scenario": "default_tick_rate",
                "params": {
                    "tick_rate": 240,
                    "pulse_per_second": 4
                },
                "expected": {
                    "tick_rate": 240,
                    "pulse_per_second": 4
                }
            }
        ]
    },
    "expected_result": "pass"
}
```

- [ ] **Step 2: Add Char.Affects test cases**

```json
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_affects_empty",
    "description": "Builds empty affects list (combat ended / no affects)",
    "input": {
        "function": "build_affects",
        "test_cases": [
            {
                "scenario": "empty_list",
                "params": {
                    "affects": []
                },
                "expected": {
                    "affects": []
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_affects_single",
    "description": "Builds affects list with a single normal affect",
    "input": {
        "function": "build_affects",
        "test_cases": [
            {
                "scenario": "single_affect",
                "params": {
                    "affects": [
                        {
                            "name": "armor",
                            "wnum": "1:53",
                            "duration": 12,
                            "estimated_seconds": 48,
                            "modifier": "-20 armor class",
                            "level": 25
                        }
                    ]
                },
                "expected": {
                    "affects": [
                        {
                            "name": "armor",
                            "wnum": "1:53",
                            "duration": 12,
                            "estimated_seconds": 48,
                            "modifier": "-20 armor class",
                            "level": 25
                        }
                    ]
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_affects_permanent",
    "description": "Builds affects list with a permanent affect (duration -1)",
    "input": {
        "function": "build_affects",
        "test_cases": [
            {
                "scenario": "permanent_affect",
                "params": {
                    "affects": [
                        {
                            "name": "racial infravision",
                            "wnum": null,
                            "duration": -1,
                            "estimated_seconds": -1,
                            "modifier": null,
                            "level": 1
                        }
                    ]
                },
                "expected": {
                    "affects": [
                        {
                            "name": "racial infravision",
                            "wnum": null,
                            "duration": -1,
                            "estimated_seconds": -1,
                            "modifier": null,
                            "level": 1
                        }
                    ]
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_affects_multiple",
    "description": "Builds affects list with multiple mixed affects",
    "input": {
        "function": "build_affects",
        "test_cases": [
            {
                "scenario": "multiple_affects",
                "params": {
                    "affects": [
                        {
                            "name": "armor",
                            "wnum": "1:53",
                            "duration": 12,
                            "estimated_seconds": 48,
                            "modifier": "-20 armor class",
                            "level": 25
                        },
                        {
                            "name": "bless",
                            "wnum": "1:67",
                            "duration": -1,
                            "estimated_seconds": -1,
                            "modifier": "+1 hitroll",
                            "level": 30
                        },
                        {
                            "name": "poison",
                            "wnum": "1:90",
                            "duration": 5,
                            "estimated_seconds": 20,
                            "modifier": "-2 strength",
                            "level": 15
                        }
                    ]
                },
                "expected": {
                    "affects": [
                        {
                            "name": "armor",
                            "wnum": "1:53",
                            "duration": 12,
                            "estimated_seconds": 48,
                            "modifier": "-20 armor class",
                            "level": 25
                        },
                        {
                            "name": "bless",
                            "wnum": "1:67",
                            "duration": -1,
                            "estimated_seconds": -1,
                            "modifier": "+1 hitroll",
                            "level": 30
                        },
                        {
                            "name": "poison",
                            "wnum": "1:90",
                            "duration": 5,
                            "estimated_seconds": 20,
                            "modifier": "-2 strength",
                            "level": 15
                        }
                    ]
                }
            }
        ]
    },
    "expected_result": "pass"
}
```

- [ ] **Step 3: Add Char.Enemies test cases**

```json
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_enemies_empty",
    "description": "Builds empty enemies list (combat ended)",
    "input": {
        "function": "build_enemies",
        "test_cases": [
            {
                "scenario": "no_combat",
                "params": {
                    "enemies": [],
                    "self_hp": 0,
                    "self_max_hp": 0
                },
                "expected": {
                    "enemies": [],
                    "self": null
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_enemies_single",
    "description": "Builds enemies list with single primary target",
    "input": {
        "function": "build_enemies",
        "test_cases": [
            {
                "scenario": "single_enemy",
                "params": {
                    "enemies": [
                        {
                            "name": "a huge troll",
                            "instance_id": [3, 42],
                            "hp_pct": 73,
                            "is_primary": true,
                            "target": "you"
                        }
                    ],
                    "self_hp": 450,
                    "self_max_hp": 600
                },
                "expected": {
                    "enemies": [
                        {
                            "name": "a huge troll",
                            "instance_id": [3, 42],
                            "hp_pct": 73,
                            "is_primary": true,
                            "target": "you"
                        }
                    ],
                    "self": {
                        "hp": 450,
                        "max_hp": 600,
                        "hp_pct": 75
                    }
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_enemies_multiple",
    "description": "Builds enemies list with primary + secondary targets",
    "input": {
        "function": "build_enemies",
        "test_cases": [
            {
                "scenario": "multiple_enemies",
                "params": {
                    "enemies": [
                        {
                            "name": "a huge troll",
                            "instance_id": [3, 42],
                            "hp_pct": 73,
                            "is_primary": true,
                            "target": "you"
                        },
                        {
                            "name": "a goblin warrior",
                            "instance_id": [3, 108],
                            "hp_pct": 45,
                            "is_primary": false,
                            "target": "Gandalf"
                        }
                    ],
                    "self_hp": 450,
                    "self_max_hp": 600
                },
                "expected": {
                    "enemies": [
                        {
                            "name": "a huge troll",
                            "instance_id": [3, 42],
                            "hp_pct": 73,
                            "is_primary": true,
                            "target": "you"
                        },
                        {
                            "name": "a goblin warrior",
                            "instance_id": [3, 108],
                            "hp_pct": 45,
                            "is_primary": false,
                            "target": "Gandalf"
                        }
                    ],
                    "self": {
                        "hp": 450,
                        "max_hp": 600,
                        "hp_pct": 75
                    }
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_enemies_self_hp",
    "description": "Verifies self block hp_pct calculation edge cases",
    "input": {
        "function": "build_enemies",
        "test_cases": [
            {
                "scenario": "self_full_hp",
                "params": {
                    "enemies": [
                        {
                            "name": "a rat",
                            "instance_id": [1, 1],
                            "hp_pct": 100,
                            "is_primary": true,
                            "target": "you"
                        }
                    ],
                    "self_hp": 500,
                    "self_max_hp": 500
                },
                "expected": {
                    "self": {
                        "hp": 500,
                        "max_hp": 500,
                        "hp_pct": 100
                    }
                }
            },
            {
                "scenario": "self_one_hp",
                "params": {
                    "enemies": [
                        {
                            "name": "a dragon",
                            "instance_id": [5, 99],
                            "hp_pct": 95,
                            "is_primary": true,
                            "target": "you"
                        }
                    ],
                    "self_hp": 1,
                    "self_max_hp": 1000
                },
                "expected": {
                    "self": {
                        "hp": 1,
                        "max_hp": 1000,
                        "hp_pct": 0
                    }
                }
            }
        ]
    },
    "expected_result": "pass"
}
```

- [ ] **Step 4: Add Room.Contents test cases**

```json
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_room_contents_empty",
    "description": "Builds empty room contents (no items, NPCs, players, or doors)",
    "input": {
        "function": "build_room_contents",
        "test_cases": [
            {
                "scenario": "empty_room",
                "params": {
                    "items": [],
                    "npcs": [],
                    "players": [],
                    "doors": []
                },
                "expected": {
                    "items": [],
                    "npcs": [],
                    "players": [],
                    "doors": []
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_room_contents_items",
    "description": "Builds room contents with items only",
    "input": {
        "function": "build_room_contents",
        "test_cases": [
            {
                "scenario": "items_only",
                "params": {
                    "items": [
                        {
                            "name": "a gleaming sword",
                            "instance_id": [2, 15],
                            "short_desc": "A gleaming sword lies here."
                        },
                        {
                            "name": "a leather pouch",
                            "instance_id": [2, 30],
                            "short_desc": "A small leather pouch has been dropped here."
                        }
                    ],
                    "npcs": [],
                    "players": [],
                    "doors": []
                },
                "expected": {
                    "items": [
                        {
                            "name": "a gleaming sword",
                            "instance_id": [2, 15],
                            "short_desc": "A gleaming sword lies here."
                        },
                        {
                            "name": "a leather pouch",
                            "instance_id": [2, 30],
                            "short_desc": "A small leather pouch has been dropped here."
                        }
                    ],
                    "npcs": [],
                    "players": [],
                    "doors": []
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_room_contents_mixed",
    "description": "Builds room contents with all entity types and doors",
    "input": {
        "function": "build_room_contents",
        "test_cases": [
            {
                "scenario": "mixed_contents",
                "params": {
                    "items": [
                        {
                            "name": "a gleaming sword",
                            "instance_id": [2, 15],
                            "short_desc": "A gleaming sword lies here."
                        }
                    ],
                    "npcs": [
                        {
                            "name": "a huge troll",
                            "instance_id": [3, 42],
                            "short_desc": "A huge troll stands here, looking mean."
                        }
                    ],
                    "players": [
                        {
                            "name": "Gandalf",
                            "instance_id": [0, 0],
                            "short_desc": null
                        }
                    ],
                    "doors": [
                        {
                            "direction": "north",
                            "state": "closed",
                            "is_locked": false
                        },
                        {
                            "direction": "east",
                            "state": "open",
                            "is_locked": false
                        }
                    ]
                },
                "expected": {
                    "items": [
                        {
                            "name": "a gleaming sword",
                            "instance_id": [2, 15],
                            "short_desc": "A gleaming sword lies here."
                        }
                    ],
                    "npcs": [
                        {
                            "name": "a huge troll",
                            "instance_id": [3, 42],
                            "short_desc": "A huge troll stands here, looking mean."
                        }
                    ],
                    "players": [
                        {
                            "name": "Gandalf"
                        }
                    ],
                    "doors": [
                        {
                            "direction": "north",
                            "state": "closed",
                            "is_locked": false
                        },
                        {
                            "direction": "east",
                            "state": "open",
                            "is_locked": false
                        }
                    ]
                }
            }
        ]
    },
    "expected_result": "pass"
},
{
    "test_type": "gmcp_sentience_",
    "name": "gmcp_build_room_contents_doors",
    "description": "Builds room contents with various door states",
    "input": {
        "function": "build_room_contents",
        "test_cases": [
            {
                "scenario": "door_states",
                "params": {
                    "items": [],
                    "npcs": [],
                    "players": [],
                    "doors": [
                        {
                            "direction": "north",
                            "state": "open",
                            "is_locked": false
                        },
                        {
                            "direction": "south",
                            "state": "closed",
                            "is_locked": false
                        },
                        {
                            "direction": "west",
                            "state": "locked",
                            "is_locked": true
                        }
                    ]
                },
                "expected": {
                    "doors": [
                        {
                            "direction": "north",
                            "state": "open",
                            "is_locked": false
                        },
                        {
                            "direction": "south",
                            "state": "closed",
                            "is_locked": false
                        },
                        {
                            "direction": "west",
                            "state": "locked",
                            "is_locked": true
                        }
                    ]
                }
            }
        ]
    },
    "expected_result": "pass"
}
```

- [ ] **Step 5: Verify JSON is valid**

Run: `python3 -c "import json; json.load(open('tests/data/unit/gmcp_sentience_unit_tests.json'))"`
Expected: No errors (valid JSON)

- [ ] **Step 6: Commit**

```bash
git add tests/data/unit/gmcp_sentience_unit_tests.json
git commit -m "test(gmcp): add Phase 3 builder test data (14 test cases)

Add JSON test definitions for:
- Client.Ready.Capabilities (1 test)
- Client.Ready.State (1 test)
- Char.Affects (4 tests: empty, single, permanent, multiple)
- Char.Enemies (4 tests: empty, single, multiple, self HP edge cases)
- Room.Contents (4 tests: empty, items-only, mixed, door states)

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 3: Test Handlers — C Scenario Runners (Stubs)

Add the C test scenario runners that read the JSON test data and call the builders. These will fail initially because the builders aren't implemented yet (TDD).

**Files:**
- Modify: `tests/unit/gmcp_sentience_tests.c`

- [ ] **Step 1: Add forward declarations for new scenario runners (after line 16)**

```c
static test_result_t run_gmcp_client_ready_capabilities_scenario(json_t *tc);
static test_result_t run_gmcp_client_ready_state_scenario(json_t *tc);
static test_result_t run_gmcp_affects_scenario(json_t *tc);
static test_result_t run_gmcp_enemies_scenario(json_t *tc);
static test_result_t run_gmcp_room_contents_scenario(json_t *tc);
```

- [ ] **Step 2: Add Client.Ready.Capabilities scenario runner**

```c
/* --- Client.Ready.Capabilities scenario --- */

static test_result_t run_gmcp_client_ready_capabilities_scenario(json_t *tc)
{
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;
    json_t *exp_pkgs, *res_pkgs, *exp_feats, *res_feats;
    size_t i;

    if (!expected) return TEST_ERROR;

    result = sentience_build_client_ready_capabilities_json();
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    /* Verify packages array */
    exp_pkgs = json_object_get(expected, "packages");
    res_pkgs = json_object_get(result, "packages");
    TEST_ASSERT_NOT_NULL(res_pkgs);
    TEST_ASSERT_INT_EQ(json_array_size(exp_pkgs), json_array_size(res_pkgs));

    for (i = 0; i < json_array_size(exp_pkgs); i++) {
        const char *exp_str = json_string_value(json_array_get(exp_pkgs, i));
        const char *res_str = json_string_value(json_array_get(res_pkgs, i));
        TEST_ASSERT_STR_EQ(exp_str, res_str);
    }

    /* Verify features array */
    exp_feats = json_object_get(expected, "features");
    res_feats = json_object_get(result, "features");
    TEST_ASSERT_NOT_NULL(res_feats);
    TEST_ASSERT_INT_EQ(json_array_size(exp_feats), json_array_size(res_feats));

    for (i = 0; i < json_array_size(exp_feats); i++) {
        const char *exp_str = json_string_value(json_array_get(exp_feats, i));
        const char *res_str = json_string_value(json_array_get(res_feats, i));
        TEST_ASSERT_STR_EQ(exp_str, res_str);
    }

    json_decref(result);
    return TEST_SUCCESS;
}
```

- [ ] **Step 3: Add Client.Ready.State scenario runner**

```c
/* --- Client.Ready.State scenario --- */

static test_result_t run_gmcp_client_ready_state_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;

    if (!params || !expected) return TEST_ERROR;

    int tick_rate = (int)json_integer_value(json_object_get(params, "tick_rate"));
    int pps = (int)json_integer_value(json_object_get(params, "pulse_per_second"));

    result = sentience_build_client_ready_state_json(tick_rate, pps);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "tick_rate")),
                       json_integer_value(json_object_get(result, "tick_rate")));
    TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(expected, "pulse_per_second")),
                       json_integer_value(json_object_get(result, "pulse_per_second")));

    json_decref(result);
    return TEST_SUCCESS;
}
```

- [ ] **Step 4: Add Char.Affects scenario runner**

```c
/* --- Char.Affects scenario --- */

static test_result_t run_gmcp_affects_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result, *aff_arr, *exp_arr;
    sentience_affect_input_t inputs[32];
    int num_affects = 0;
    size_t i;

    if (!params || !expected) return TEST_ERROR;

    /* Build input array from JSON params */
    aff_arr = json_object_get(params, "affects");
    if (aff_arr && json_is_array(aff_arr)) {
        json_t *af;
        json_array_foreach(aff_arr, i, af) {
            if (num_affects >= 32) break;
            inputs[num_affects].name = test_json_get_string(af, "name");
            inputs[num_affects].wnum = test_json_get_string(af, "wnum");
            inputs[num_affects].duration = (int)json_integer_value(json_object_get(af, "duration"));
            inputs[num_affects].estimated_seconds = (int)json_integer_value(json_object_get(af, "estimated_seconds"));
            inputs[num_affects].modifier = test_json_get_string(af, "modifier");
            inputs[num_affects].level = (int)json_integer_value(json_object_get(af, "level"));
            num_affects++;
        }
    }

    result = sentience_build_affects_json(inputs, num_affects);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    /* Verify affects array length */
    exp_arr = json_object_get(expected, "affects");
    json_t *res_arr = json_object_get(result, "affects");
    TEST_ASSERT_NOT_NULL(res_arr);
    TEST_ASSERT_INT_EQ(json_array_size(exp_arr), json_array_size(res_arr));

    /* Verify each affect entry */
    for (i = 0; i < json_array_size(exp_arr); i++) {
        json_t *exp_af = json_array_get(exp_arr, i);
        json_t *res_af = json_array_get(res_arr, i);
        const char *exp_name = json_string_value(json_object_get(exp_af, "name"));
        const char *res_name = json_string_value(json_object_get(res_af, "name"));

        TEST_ASSERT_STR_EQ(exp_name, res_name);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_af, "duration")),
                           json_integer_value(json_object_get(res_af, "duration")));
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_af, "estimated_seconds")),
                           json_integer_value(json_object_get(res_af, "estimated_seconds")));
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_af, "level")),
                           json_integer_value(json_object_get(res_af, "level")));

        /* wnum: check null vs string */
        if (json_is_null(json_object_get(exp_af, "wnum"))) {
            TEST_ASSERT_TRUE(json_is_null(json_object_get(res_af, "wnum")));
        } else {
            TEST_ASSERT_STR_EQ(json_string_value(json_object_get(exp_af, "wnum")),
                               json_string_value(json_object_get(res_af, "wnum")));
        }

        /* modifier: check null vs string */
        if (json_is_null(json_object_get(exp_af, "modifier"))) {
            TEST_ASSERT_TRUE(json_is_null(json_object_get(res_af, "modifier")));
        } else {
            TEST_ASSERT_STR_EQ(json_string_value(json_object_get(exp_af, "modifier")),
                               json_string_value(json_object_get(res_af, "modifier")));
        }
    }

    json_decref(result);
    return TEST_SUCCESS;
}
```

- [ ] **Step 5: Add Char.Enemies scenario runner**

```c
/* --- Char.Enemies scenario --- */

static test_result_t run_gmcp_enemies_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result, *en_arr, *exp_arr;
    sentience_enemy_input_t inputs[32];
    int num_enemies = 0;
    long self_hp, self_max_hp;
    size_t i;

    if (!params || !expected) return TEST_ERROR;

    self_hp = json_integer_value(json_object_get(params, "self_hp"));
    self_max_hp = json_integer_value(json_object_get(params, "self_max_hp"));

    en_arr = json_object_get(params, "enemies");
    if (en_arr && json_is_array(en_arr)) {
        json_t *en;
        json_array_foreach(en_arr, i, en) {
            json_t *iid;
            if (num_enemies >= 32) break;
            inputs[num_enemies].name = test_json_get_string(en, "name");
            iid = json_object_get(en, "instance_id");
            inputs[num_enemies].instance_id[0] = json_integer_value(json_array_get(iid, 0));
            inputs[num_enemies].instance_id[1] = json_integer_value(json_array_get(iid, 1));
            inputs[num_enemies].hp_pct = (int)json_integer_value(json_object_get(en, "hp_pct"));
            inputs[num_enemies].is_primary = json_is_true(json_object_get(en, "is_primary"));
            inputs[num_enemies].target = test_json_get_string(en, "target");
            num_enemies++;
        }
    }

    result = sentience_build_enemies_json(inputs, num_enemies, self_hp, self_max_hp);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    /* Verify enemies array */
    exp_arr = json_object_get(expected, "enemies");
    json_t *res_arr = json_object_get(result, "enemies");
    if (exp_arr) {
        TEST_ASSERT_NOT_NULL(res_arr);
        TEST_ASSERT_INT_EQ(json_array_size(exp_arr), json_array_size(res_arr));

        for (i = 0; i < json_array_size(exp_arr); i++) {
            json_t *exp_en = json_array_get(exp_arr, i);
            json_t *res_en = json_array_get(res_arr, i);

            TEST_ASSERT_STR_EQ(json_string_value(json_object_get(exp_en, "name")),
                               json_string_value(json_object_get(res_en, "name")));
            TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_en, "hp_pct")),
                               json_integer_value(json_object_get(res_en, "hp_pct")));
            TEST_ASSERT_TRUE(json_is_true(json_object_get(exp_en, "is_primary"))
                             == json_is_true(json_object_get(res_en, "is_primary")));
        }
    }

    /* Verify self block */
    json_t *exp_self = json_object_get(expected, "self");
    json_t *res_self = json_object_get(result, "self");
    if (json_is_null(exp_self)) {
        TEST_ASSERT_TRUE(json_is_null(res_self));
    } else if (exp_self) {
        TEST_ASSERT_NOT_NULL(res_self);
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_self, "hp")),
                           json_integer_value(json_object_get(res_self, "hp")));
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_self, "max_hp")),
                           json_integer_value(json_object_get(res_self, "max_hp")));
        TEST_ASSERT_INT_EQ(json_integer_value(json_object_get(exp_self, "hp_pct")),
                           json_integer_value(json_object_get(res_self, "hp_pct")));
    }

    json_decref(result);
    return TEST_SUCCESS;
}
```

- [ ] **Step 6: Add Room.Contents scenario runner**

```c
/* --- Room.Contents scenario --- */

static test_result_t run_gmcp_room_contents_scenario(json_t *tc)
{
    json_t *params = json_object_get(tc, "params");
    json_t *expected = json_object_get(tc, "expected");
    json_t *result;
    sentience_room_entity_input_t items[32], npcs[32], players[32];
    sentience_room_door_input_t doors[10];
    sentience_room_contents_input_t data = {0};
    size_t i;

    if (!params || !expected) return TEST_ERROR;

    /* Build items from JSON */
    json_t *j_items = json_object_get(params, "items");
    if (j_items && json_is_array(j_items)) {
        json_t *it;
        json_array_foreach(j_items, i, it) {
            json_t *iid;
            if (data.num_items >= 32) break;
            items[data.num_items].name = test_json_get_string(it, "name");
            iid = json_object_get(it, "instance_id");
            items[data.num_items].instance_id[0] = json_integer_value(json_array_get(iid, 0));
            items[data.num_items].instance_id[1] = json_integer_value(json_array_get(iid, 1));
            items[data.num_items].short_desc = test_json_get_string(it, "short_desc");
            data.num_items++;
        }
    }
    data.items = items;

    /* Build NPCs from JSON */
    json_t *j_npcs = json_object_get(params, "npcs");
    if (j_npcs && json_is_array(j_npcs)) {
        json_t *npc;
        json_array_foreach(j_npcs, i, npc) {
            json_t *iid;
            if (data.num_npcs >= 32) break;
            npcs[data.num_npcs].name = test_json_get_string(npc, "name");
            iid = json_object_get(npc, "instance_id");
            npcs[data.num_npcs].instance_id[0] = json_integer_value(json_array_get(iid, 0));
            npcs[data.num_npcs].instance_id[1] = json_integer_value(json_array_get(iid, 1));
            npcs[data.num_npcs].short_desc = test_json_get_string(npc, "short_desc");
            data.num_npcs++;
        }
    }
    data.npcs = npcs;

    /* Build players from JSON */
    json_t *j_players = json_object_get(params, "players");
    if (j_players && json_is_array(j_players)) {
        json_t *pl;
        json_array_foreach(j_players, i, pl) {
            json_t *iid;
            if (data.num_players >= 32) break;
            players[data.num_players].name = test_json_get_string(pl, "name");
            iid = json_object_get(pl, "instance_id");
            if (iid) {
                players[data.num_players].instance_id[0] = json_integer_value(json_array_get(iid, 0));
                players[data.num_players].instance_id[1] = json_integer_value(json_array_get(iid, 1));
            }
            players[data.num_players].short_desc = NULL;
            data.num_players++;
        }
    }
    data.players = players;

    /* Build doors from JSON */
    json_t *j_doors = json_object_get(params, "doors");
    if (j_doors && json_is_array(j_doors)) {
        json_t *dr;
        json_array_foreach(j_doors, i, dr) {
            if (data.num_doors >= 10) break;
            doors[data.num_doors].direction = test_json_get_string(dr, "direction");
            doors[data.num_doors].state = test_json_get_string(dr, "state");
            doors[data.num_doors].is_locked = json_is_true(json_object_get(dr, "is_locked"));
            data.num_doors++;
        }
    }
    data.doors = doors;

    result = sentience_build_room_contents_json(&data);
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_INT_EQ(SENTIENCE_PACKAGE_VERSION,
                       json_integer_value(json_object_get(result, "_v")));

    /* Verify each array category */
    const char *categories[] = {"items", "npcs", "players", "doors"};
    int c;
    for (c = 0; c < 4; c++) {
        json_t *exp_cat = json_object_get(expected, categories[c]);
        json_t *res_cat = json_object_get(result, categories[c]);
        if (!exp_cat) continue;

        TEST_ASSERT_NOT_NULL(res_cat);
        TEST_ASSERT_INT_EQ(json_array_size(exp_cat), json_array_size(res_cat));

        for (i = 0; i < json_array_size(exp_cat); i++) {
            json_t *exp_ent = json_array_get(exp_cat, i);
            json_t *res_ent = json_array_get(res_cat, i);

            /* name present in all categories */
            TEST_ASSERT_STR_EQ(json_string_value(json_object_get(exp_ent, "name")),
                               json_string_value(json_object_get(res_ent, "name")));

            /* doors have direction/state/is_locked */
            if (strcmp(categories[c], "doors") == 0) {
                TEST_ASSERT_STR_EQ(
                    json_string_value(json_object_get(exp_ent, "direction")),
                    json_string_value(json_object_get(res_ent, "direction")));
                TEST_ASSERT_STR_EQ(
                    json_string_value(json_object_get(exp_ent, "state")),
                    json_string_value(json_object_get(res_ent, "state")));
                TEST_ASSERT_TRUE(json_is_true(json_object_get(exp_ent, "is_locked"))
                                 == json_is_true(json_object_get(res_ent, "is_locked")));
            }

            /* items/npcs have instance_id and short_desc */
            if (strcmp(categories[c], "items") == 0
                || strcmp(categories[c], "npcs") == 0) {
                json_t *eid = json_object_get(exp_ent, "instance_id");
                json_t *rid = json_object_get(res_ent, "instance_id");
                TEST_ASSERT_NOT_NULL(rid);
                TEST_ASSERT_INT_EQ(json_integer_value(json_array_get(eid, 0)),
                                   json_integer_value(json_array_get(rid, 0)));
                TEST_ASSERT_INT_EQ(json_integer_value(json_array_get(eid, 1)),
                                   json_integer_value(json_array_get(rid, 1)));
                TEST_ASSERT_STR_EQ(
                    json_string_value(json_object_get(exp_ent, "short_desc")),
                    json_string_value(json_object_get(res_ent, "short_desc")));
            }

            /* players: name only (no instance_id or short_desc) */
        }
    }

    json_decref(result);
    return TEST_SUCCESS;
}
```

- [ ] **Step 7: Add dispatcher entries to `run_gmcp_sentience_test_case`**

In the `if/else if` chain (around line 305), add before the `else` fallback:

```c
        } else if (strcmp(func_name, "build_client_ready_capabilities") == 0) {
            result = run_gmcp_client_ready_capabilities_scenario(tc);
        } else if (strcmp(func_name, "build_client_ready_state") == 0) {
            result = run_gmcp_client_ready_state_scenario(tc);
        } else if (strcmp(func_name, "build_affects") == 0) {
            result = run_gmcp_affects_scenario(tc);
        } else if (strcmp(func_name, "build_enemies") == 0) {
            result = run_gmcp_enemies_scenario(tc);
        } else if (strcmp(func_name, "build_room_contents") == 0) {
            result = run_gmcp_room_contents_scenario(tc);
```

- [ ] **Step 8: Build and verify tests fail (TDD — red phase)**

Run: `cd /sentience/src && ./build tests 2>&1 | tail -10`
Expected: Build fails — linker errors for undefined `sentience_build_client_ready_capabilities_json`, etc.
This confirms the tests are wired up correctly and will pass once we implement the builders.

- [ ] **Step 9: Commit**

```bash
git add tests/unit/gmcp_sentience_tests.c
git commit -m "test(gmcp): add Phase 3 builder test handlers (red phase)

Add 5 scenario runners for: client_ready_capabilities,
client_ready_state, affects, enemies, room_contents.
Add dispatcher entries to route by function name.

Build will fail (linker errors) until builders are implemented.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 4: Implement Client.Ready Builders

Implement the two simplest builders first. These are static/one-shot and have no game state dependencies.

**Files:**
- Modify: `gmcp_sentience.c`

- [ ] **Step 1: Add `sentience_build_client_ready_capabilities_json` (after the Phase 1 builders, before the helpers section)**

Insert before the `/* ── Helpers for game-loop integration ──*/` comment (line 178):

```c
/* ── Phase 3: Pure JSON builders ────────────────────────────────── */

json_t *sentience_build_client_ready_capabilities_json(void)
{
    json_t *obj = json_object();
    json_t *packages = json_array();
    json_t *features = json_array();

    if (!obj) return NULL;

    json_array_append_new(packages, json_string("Sentience.Char.Identity"));
    json_array_append_new(packages, json_string("Sentience.Char.Vitals"));
    json_array_append_new(packages, json_string("Sentience.Char.Stats"));
    json_array_append_new(packages, json_string("Sentience.Char.Combat"));
    json_array_append_new(packages, json_string("Sentience.Char.Worth"));
    json_array_append_new(packages, json_string("Sentience.Char.Affects"));
    json_array_append_new(packages, json_string("Sentience.Char.Enemies"));
    json_array_append_new(packages, json_string("Sentience.Room.Info"));
    json_array_append_new(packages, json_string("Sentience.Room.Contents"));
    json_array_append_new(packages, json_string("Sentience.Link"));

    json_array_append_new(features, json_string("links"));
    json_array_append_new(features, json_string("osc8"));

    json_object_set_new(obj, "packages", packages);
    json_object_set_new(obj, "features", features);
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}

json_t *sentience_build_client_ready_state_json(int tick_rate, int pulse_per_second)
{
    json_t *obj = json_object();
    if (!obj) return NULL;

    json_object_set_new(obj, "tick_rate",         json_integer(tick_rate));
    json_object_set_new(obj, "pulse_per_second",  json_integer(pulse_per_second));
    json_object_set_new(obj, "_v",                json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}
```

- [ ] **Step 2: Build and run Client.Ready tests**

Run: `cd /sentience/src && ./build tests 2>&1 | tail -5`

If linker errors persist (other builders missing), that's expected. Check if a partial build works by checking compilation of gmcp_sentience.c only. If the full build fails because of linker errors from test code calling the unimplemented builders, add stub implementations for the remaining 3 builders that just return NULL — they'll be replaced in Tasks 5-6.

Temporary stubs (add after the Client.Ready builders):

```c
/* Stubs — replaced in Tasks 5 and 6 */
json_t *sentience_build_affects_json(const sentience_affect_input_t *affects, int num_affects)
{ (void)affects; (void)num_affects; return NULL; }

json_t *sentience_build_enemies_json(const sentience_enemy_input_t *enemies, int num_enemies,
                                      long self_hp, long self_max_hp)
{ (void)enemies; (void)num_enemies; (void)self_hp; (void)self_max_hp; return NULL; }

json_t *sentience_build_room_contents_json(const sentience_room_contents_input_t *data)
{ (void)data; return NULL; }
```

- [ ] **Step 3: Build and run tests**

Run: `cd /sentience/src && ./build tests && ./install && cd /sentience && ./sent -test:gmcp`
Expected: Client.Ready tests PASS (2 new), stub tests FAIL/ERROR (expected — TDD red), Phase 1 tests still PASS (11).

- [ ] **Step 4: Commit**

```bash
git add gmcp_sentience.c
git commit -m "feat(gmcp): implement Client.Ready builders + stub remaining Phase 3 builders

Add sentience_build_client_ready_capabilities_json() — hard-coded
package list and features array.
Add sentience_build_client_ready_state_json() — tick_rate and
pulse_per_second.

Stub out affects, enemies, room_contents builders (return NULL)
to allow build while test handlers are wired up.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 5: Implement Char.Affects Builder

Replace the stub with the real builder.

**Files:**
- Modify: `gmcp_sentience.c`

- [ ] **Step 1: Replace the `sentience_build_affects_json` stub**

```c
json_t *sentience_build_affects_json(const sentience_affect_input_t *affects, int num_affects)
{
    json_t *obj = json_object();
    json_t *arr = json_array();
    int i;

    if (!obj) return NULL;

    for (i = 0; i < num_affects; i++) {
        json_t *af = json_object();

        json_object_set_new(af, "name",
            json_string(affects[i].name ? affects[i].name : "unknown"));

        if (affects[i].wnum)
            json_object_set_new(af, "wnum", json_string(affects[i].wnum));
        else
            json_object_set_new(af, "wnum", json_null());

        json_object_set_new(af, "duration",          json_integer(affects[i].duration));
        json_object_set_new(af, "estimated_seconds",  json_integer(affects[i].estimated_seconds));

        if (affects[i].modifier)
            json_object_set_new(af, "modifier", json_string(affects[i].modifier));
        else
            json_object_set_new(af, "modifier", json_null());

        json_object_set_new(af, "level", json_integer(affects[i].level));

        json_array_append_new(arr, af);
    }

    json_object_set_new(obj, "affects", arr);
    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}
```

- [ ] **Step 2: Build and run affects tests**

Run: `cd /sentience/src && ./build tests && ./install && cd /sentience && ./sent -test:gmcp`
Expected: All 4 affects tests PASS. Previous Client.Ready tests still PASS.

- [ ] **Step 3: Commit**

```bash
git add gmcp_sentience.c
git commit -m "feat(gmcp): implement Char.Affects builder

Replace stub with full implementation. Handles null wnum/modifier
(emits JSON null), permanent durations (-1), and empty arrays.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 6: Implement Char.Enemies Builder

Replace the stub with the real builder.

**Files:**
- Modify: `gmcp_sentience.c`

- [ ] **Step 1: Replace the `sentience_build_enemies_json` stub**

```c
json_t *sentience_build_enemies_json(const sentience_enemy_input_t *enemies, int num_enemies,
                                      long self_hp, long self_max_hp)
{
    json_t *obj = json_object();
    json_t *arr = json_array();
    int i;

    if (!obj) return NULL;

    for (i = 0; i < num_enemies; i++) {
        json_t *en = json_object();
        json_t *iid = json_array();

        json_object_set_new(en, "name",
            json_string(enemies[i].name ? enemies[i].name : "someone"));

        json_array_append_new(iid, json_integer(enemies[i].instance_id[0]));
        json_array_append_new(iid, json_integer(enemies[i].instance_id[1]));
        json_object_set_new(en, "instance_id", iid);

        json_object_set_new(en, "hp_pct",     json_integer(enemies[i].hp_pct));
        json_object_set_new(en, "is_primary",  enemies[i].is_primary ? json_true() : json_false());
        json_object_set_new(en, "target",
            json_string(enemies[i].target ? enemies[i].target : "someone"));

        json_array_append_new(arr, en);
    }

    json_object_set_new(obj, "enemies", arr);

    /* Self block: present when in combat, null when combat ended */
    if (num_enemies > 0) {
        json_t *self = json_object();
        long hp_pct = (self_hp * 100) / UMAX(1, self_max_hp);

        json_object_set_new(self, "hp",     json_integer(self_hp));
        json_object_set_new(self, "max_hp", json_integer(self_max_hp));
        json_object_set_new(self, "hp_pct", json_integer(hp_pct));
        json_object_set_new(obj, "self", self);
    } else {
        json_object_set_new(obj, "self", json_null());
    }

    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}
```

**Note:** This file needs `#include "merc.h"` which is already included — provides the `UMAX` macro.

- [ ] **Step 2: Build and run enemies tests**

Run: `cd /sentience/src && ./build tests && ./install && cd /sentience && ./sent -test:gmcp`
Expected: All 4 enemies tests PASS. Previous tests still PASS.

- [ ] **Step 3: Commit**

```bash
git add gmcp_sentience.c
git commit -m "feat(gmcp): implement Char.Enemies builder

Replace stub with full implementation. Self block with exact HP
present when in combat, null when combat ended. Enemy entries
include instance_id array, hp_pct, is_primary flag, and target.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 7: Implement Room.Contents Builder

Replace the stub with the real builder.

**Files:**
- Modify: `gmcp_sentience.c`

- [ ] **Step 1: Replace the `sentience_build_room_contents_json` stub**

```c
json_t *sentience_build_room_contents_json(const sentience_room_contents_input_t *data)
{
    json_t *obj;
    json_t *items, *npcs_arr, *players_arr, *doors_arr;
    int i;

    if (!data) return NULL;

    obj = json_object();
    if (!obj) return NULL;

    /* Items */
    items = json_array();
    for (i = 0; i < data->num_items; i++) {
        json_t *it = json_object();
        json_t *iid = json_array();

        json_object_set_new(it, "name",
            json_string(data->items[i].name ? data->items[i].name : "something"));
        json_array_append_new(iid, json_integer(data->items[i].instance_id[0]));
        json_array_append_new(iid, json_integer(data->items[i].instance_id[1]));
        json_object_set_new(it, "instance_id", iid);
        json_object_set_new(it, "short_desc",
            json_string(data->items[i].short_desc ? data->items[i].short_desc : ""));
        json_array_append_new(items, it);
    }
    json_object_set_new(obj, "items", items);

    /* NPCs */
    npcs_arr = json_array();
    for (i = 0; i < data->num_npcs; i++) {
        json_t *npc = json_object();
        json_t *iid = json_array();

        json_object_set_new(npc, "name",
            json_string(data->npcs[i].name ? data->npcs[i].name : "someone"));
        json_array_append_new(iid, json_integer(data->npcs[i].instance_id[0]));
        json_array_append_new(iid, json_integer(data->npcs[i].instance_id[1]));
        json_object_set_new(npc, "instance_id", iid);
        json_object_set_new(npc, "short_desc",
            json_string(data->npcs[i].short_desc ? data->npcs[i].short_desc : ""));
        json_array_append_new(npcs_arr, npc);
    }
    json_object_set_new(obj, "npcs", npcs_arr);

    /* Players — name only, no instance_id or short_desc */
    players_arr = json_array();
    for (i = 0; i < data->num_players; i++) {
        json_t *pl = json_object();
        json_object_set_new(pl, "name",
            json_string(data->players[i].name ? data->players[i].name : "someone"));
        json_array_append_new(players_arr, pl);
    }
    json_object_set_new(obj, "players", players_arr);

    /* Doors */
    doors_arr = json_array();
    for (i = 0; i < data->num_doors; i++) {
        json_t *dr = json_object();
        json_object_set_new(dr, "direction",
            json_string(data->doors[i].direction ? data->doors[i].direction : "unknown"));
        json_object_set_new(dr, "state",
            json_string(data->doors[i].state ? data->doors[i].state : "open"));
        json_object_set_new(dr, "is_locked",
            data->doors[i].is_locked ? json_true() : json_false());
        json_array_append_new(doors_arr, dr);
    }
    json_object_set_new(obj, "doors", doors_arr);

    json_object_set_new(obj, "_v", json_integer(SENTIENCE_PACKAGE_VERSION));

    return obj;
}
```

- [ ] **Step 2: Build and run all GMCP tests**

Run: `cd /sentience/src && ./build tests && ./install && cd /sentience && ./sent -test:gmcp`
Expected: ALL tests PASS — Phase 1 (11) + Phase 3 (14) = 25 total.

- [ ] **Step 3: Commit**

```bash
git add gmcp_sentience.c
git commit -m "feat(gmcp): implement Room.Contents builder

Replace stub with full implementation. Builds items, NPCs,
players (name only), and doors arrays. Handles null name/desc
with sensible defaults.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 8: Client.Ready.Capabilities — Protocol Integration

Hook the Capabilities send into the GMCP negotiation paths.

**Files:**
- Modify: `protocol.c`
- Modify: `protocol_websocket.c`

- [ ] **Step 1: Add include for `gmcp_sentience.h` to `protocol.c` (near other includes)**

Check if `gmcp_sentience.h` is already included. If not, add:

```c
#include "gmcp_sentience.h"
```

- [ ] **Step 2: Create a static helper in `protocol.c` for sending Capabilities**

Add near the top of protocol.c (after includes):

```c
/*
 * Send Sentience.Client.Ready.Capabilities if Sentience support is enabled.
 */
static void sentience_send_capabilities(descriptor_t *d)
{
    json_t *caps;
    char *dump;

    if (!d || !d->pProtocol || !d->pProtocol->bGMCPSupport[GMCP_SUPPORT_SENTIENCE])
        return;

    caps = sentience_build_client_ready_capabilities_json();
    if (!caps) return;

    dump = json_dumps(caps, JSON_COMPACT);
    if (dump) {
        SendGMCPRaw(d, "Sentience.Client.Ready.Capabilities", dump);
        free(dump);
    }
    json_decref(caps);
}
```

- [ ] **Step 3: Call `sentience_send_capabilities` after `GMCP_CORE_SUPPORTS_SET` processing**

In `protocol.c`, after the for loop in the `GMCP_CORE_SUPPORTS_SET` case (after line 3856, before the `break`):

```c
          sentience_send_capabilities(apDescriptor);
```

- [ ] **Step 4: Call `sentience_send_capabilities` after `GMCP_CORE_SUPPORTS_ADD` processing**

Same pattern — after the for loop in `GMCP_CORE_SUPPORTS_ADD` (after line 3880, before the `break`):

```c
          sentience_send_capabilities(apDescriptor);
```

- [ ] **Step 5: Hook into `protocol_websocket.c` WebSocket auto-enable**

In `protocol_websocket.c`, after line 288 (`bGMCPSupport[GMCP_SUPPORT_SENTIENCE] = true`):

Add `#include "gmcp_sentience.h"` near the top of the file if not already present, then after the support flag is set:

```c
    /* Send Client.Ready.Capabilities to WebSocket clients */
    {
        json_t *caps = sentience_build_client_ready_capabilities_json();
        if (caps) {
            char *dump = json_dumps(caps, JSON_COMPACT);
            if (dump) {
                send_gmcp_message(proto, "Sentience.Client.Ready.Capabilities", dump);
                free(dump);
            }
            json_decref(caps);
        }
    }
```

Note: WebSocket uses `send_gmcp_message(proto, ...)` not `SendGMCPRaw(d, ...)` — check the existing code pattern in `protocol_websocket.c` for the correct function signature.

- [ ] **Step 6: Build and run all tests**

Run: `cd /sentience/src && ./build tests && ./install && cd /sentience && ./sent -test:gmcp`
Expected: All GMCP tests pass. No new test needed here (Capabilities builder already tested; integration is a protocol path, not unit-testable without a mock connection).

- [ ] **Step 7: Commit**

```bash
git add protocol.c protocol_websocket.c
git commit -m "feat(gmcp): send Client.Ready.Capabilities on GMCP negotiation

Hook sentience_build_client_ready_capabilities_json() into:
- protocol.c: GMCP_CORE_SUPPORTS_SET and GMCP_CORE_SUPPORTS_ADD handlers
- protocol_websocket.c: WebSocket auto-enable path

Clients receive package list and feature flags immediately after
Sentience support is negotiated.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 9: Update Logic — Wire Phase 3 Into `sentience_gmcp_update()`

Add the Phase 3 update logic: Client.Ready.State one-shot, Char.Affects, Char.Enemies, and Room.Contents.

**Files:**
- Modify: `gmcp_sentience.c`

- [ ] **Step 1: Add Client.Ready.State send before the Phase 1 dirty-flag detection block**

In `sentience_gmcp_update()`, after `cache = &proto->sentience_cache;` (line 233) and before the `/* ── Detect changes */` comment (line 235):

```c
    /* ── Phase 3: Client.Ready.State (one-shot on first update) ─── */
    if (!cache->initialized) {
        sentience_send_package(d, "Sentience.Client.Ready.State",
            sentience_build_client_ready_state_json(PULSE_TICK, PULSE_PER_SECOND));
    }
```

- [ ] **Step 2: Add Char.Affects update logic after the Phase 1 send block**

After the `SENTIENCE_DIRTY_ROOM` send block (after line 451) and before `cache->initialized = true` (line 453):

```c
    /* ── Phase 3: Char.Affects ─────────────────────────────────── */
    if (ch->affected || cache->had_affects) {
        AFFECT_DATA *af;
        sentience_affect_input_t aff_inputs[64];
        int num_aff = 0;
        char mod_buf[64][64]; /* modifier string buffers */

        for (af = ch->affected; af && num_aff < 64; af = af->next) {
            /* Name: skill name, custom name, or fallback */
            if (af->skill && af->skill->name)
                aff_inputs[num_aff].name = af->skill->name;
            else if (af->custom_name)
                aff_inputs[num_aff].name = af->custom_name;
            else
                aff_inputs[num_aff].name = "unknown";

            /* Wnum: from skill pointer if set */
            if (af->skill)
                aff_inputs[num_aff].wnum = widevnum_string(af->skill, af->skill->vnum, NULL);
            else
                aff_inputs[num_aff].wnum = NULL;

            aff_inputs[num_aff].duration = af->duration;
            aff_inputs[num_aff].estimated_seconds = (af->duration >= 0)
                ? (af->duration * PULSE_TICK / PULSE_PER_SECOND)
                : -1;

            /* Modifier: human-readable string */
            if (af->location != APPLY_NONE && af->modifier != 0) {
                snprintf(mod_buf[num_aff], sizeof(mod_buf[num_aff]),
                         "%+d %s", af->modifier, affect_loc_name(af->location));
                aff_inputs[num_aff].modifier = mod_buf[num_aff];
            } else {
                aff_inputs[num_aff].modifier = NULL;
            }

            aff_inputs[num_aff].level = af->level;
            num_aff++;
        }

        sentience_send_package(d, "Sentience.Char.Affects",
            sentience_build_affects_json(aff_inputs, num_aff));
        cache->had_affects = (ch->affected != NULL);
    }
```

- [ ] **Step 3: Add Char.Enemies update logic**

```c
    /* ── Phase 3: Char.Enemies ─────────────────────────────────── */
    if (ch->fighting || cache->was_fighting) {
        sentience_enemy_input_t en_inputs[32];
        int num_en = 0;
        CHAR_DATA *vch;

        /* Primary target */
        if (ch->fighting && ch->fighting->in_room == ch->in_room) {
            vch = ch->fighting;
            en_inputs[num_en].name = IS_NPC(vch) ? vch->short_descr : vch->name;
            en_inputs[num_en].instance_id[0] = vch->id[0];
            en_inputs[num_en].instance_id[1] = vch->id[1];
            en_inputs[num_en].hp_pct = (int)((vch->hit * 100) / UMAX(1, vch->max_hit));
            en_inputs[num_en].is_primary = true;
            en_inputs[num_en].target = (vch->fighting == ch) ? "you"
                : (vch->fighting ? (IS_NPC(vch->fighting) ? vch->fighting->short_descr
                                                           : vch->fighting->name)
                                 : "no one");
            num_en++;
        }

        /* Secondary: anyone in room fighting ch, not already listed */
        for (vch = ch->in_room->people; vch && num_en < 32; vch = vch->next_in_room) {
            if (vch == ch || vch == ch->fighting || vch->fighting != ch)
                continue;
            en_inputs[num_en].name = IS_NPC(vch) ? vch->short_descr : vch->name;
            en_inputs[num_en].instance_id[0] = vch->id[0];
            en_inputs[num_en].instance_id[1] = vch->id[1];
            en_inputs[num_en].hp_pct = (int)((vch->hit * 100) / UMAX(1, vch->max_hit));
            en_inputs[num_en].is_primary = false;
            en_inputs[num_en].target = "you";
            num_en++;
        }

        sentience_send_package(d, "Sentience.Char.Enemies",
            sentience_build_enemies_json(en_inputs, num_en, ch->hit, ch->max_hit));
        cache->was_fighting = (ch->fighting != NULL);
    }
```

- [ ] **Step 4: Add Room.Contents update logic**

```c
    /* ── Phase 3: Room.Contents (fingerprint comparison) ──────── */
    {
        long cur_rid[2];
        int cur_count = 0;

        cur_rid[0] = ch->in_room->area ? ch->in_room->area->uid : 0;
        cur_rid[1] = ch->in_room->vnum;

        /* Quick count for fingerprint */
        {
            OBJ_DATA *obj;
            CHAR_DATA *rch;
            int dir;

            for (obj = ch->in_room->contents; obj; obj = obj->next_content) {
                if (can_see_obj(ch, obj))
                    cur_count++;
            }
            for (rch = ch->in_room->people; rch; rch = rch->next_in_room) {
                if (rch != ch && can_see(ch, rch))
                    cur_count++;
            }
            for (dir = 0; dir < MAX_DIR; dir++) {
                EXIT_DATA *ex = ch->in_room->exit[dir];
                if (ex && ex->u1.to_room && IS_SET(ex->exit_info, EX_ISDOOR))
                    cur_count++;
            }
        }

        if (cur_rid[0] != cache->contents_room_id[0]
            || cur_rid[1] != cache->contents_room_id[1]
            || cur_count != cache->contents_count) {

            sentience_room_entity_input_t item_inputs[128];
            sentience_room_entity_input_t npc_inputs[64];
            sentience_room_entity_input_t player_inputs[64];
            sentience_room_door_input_t door_inputs[10];
            sentience_room_contents_input_t data = {0};
            OBJ_DATA *obj;
            CHAR_DATA *rch;
            int dir;

            /* Items */
            for (obj = ch->in_room->contents; obj; obj = obj->next_content) {
                if (!can_see_obj(ch, obj) || data.num_items >= 128)
                    continue;
                item_inputs[data.num_items].name = obj->short_descr ? obj->short_descr : "something";
                item_inputs[data.num_items].instance_id[0] = obj->id[0];
                item_inputs[data.num_items].instance_id[1] = obj->id[1];
                item_inputs[data.num_items].short_desc = obj->description ? obj->description : "";
                data.num_items++;
            }
            data.items = item_inputs;

            /* NPCs and players */
            for (rch = ch->in_room->people; rch; rch = rch->next_in_room) {
                if (rch == ch || !can_see(ch, rch))
                    continue;

                if (IS_NPC(rch)) {
                    if (data.num_npcs >= 64) continue;
                    npc_inputs[data.num_npcs].name = rch->short_descr ? rch->short_descr : "someone";
                    npc_inputs[data.num_npcs].instance_id[0] = rch->id[0];
                    npc_inputs[data.num_npcs].instance_id[1] = rch->id[1];
                    npc_inputs[data.num_npcs].short_desc = rch->long_descr ? rch->long_descr : "";
                    data.num_npcs++;
                } else {
                    if (data.num_players >= 64) continue;
                    player_inputs[data.num_players].name = rch->name ? rch->name : "someone";
                    player_inputs[data.num_players].instance_id[0] = 0;
                    player_inputs[data.num_players].instance_id[1] = 0;
                    player_inputs[data.num_players].short_desc = NULL;
                    data.num_players++;
                }
            }
            data.npcs = npc_inputs;
            data.players = player_inputs;

            /* Doors */
            for (dir = 0; dir < MAX_DIR && data.num_doors < 10; dir++) {
                EXIT_DATA *ex = ch->in_room->exit[dir];
                if (!ex || !ex->u1.to_room || !IS_SET(ex->exit_info, EX_ISDOOR))
                    continue;
                door_inputs[data.num_doors].direction = dir_name[dir];
                if (IS_SET(ex->exit_info, EX_LOCKED))
                    door_inputs[data.num_doors].state = "locked";
                else if (IS_SET(ex->exit_info, EX_CLOSED))
                    door_inputs[data.num_doors].state = "closed";
                else
                    door_inputs[data.num_doors].state = "open";
                door_inputs[data.num_doors].is_locked = IS_SET(ex->exit_info, EX_LOCKED) ? true : false;
                data.num_doors++;
            }
            data.doors = door_inputs;

            sentience_send_package(d, "Sentience.Room.Contents",
                sentience_build_room_contents_json(&data));

            cache->contents_room_id[0] = cur_rid[0];
            cache->contents_room_id[1] = cur_rid[1];
            cache->contents_count = cur_count;
        }
    }
```

- [ ] **Step 5: Verify `cache->initialized = true` is still the last line**

The existing `cache->initialized = true;` (was line 453) must remain at the end of the function, after all the Phase 3 blocks. Do NOT move it.

- [ ] **Step 6: Build and run all tests**

Run: `cd /sentience/src && ./build tests && ./install && cd /sentience && ./sent -test:gmcp && ./sent -test:slink`
Expected: All GMCP tests PASS (25). All slink tests PASS (14). Clean build.

- [ ] **Step 7: Commit**

```bash
git add gmcp_sentience.c
git commit -m "feat(gmcp): wire Phase 3 update logic into sentience_gmcp_update()

Add Client.Ready.State one-shot send on first update cycle.
Add Char.Affects always-send with transition tracking (had_affects).
Add Char.Enemies always-send with transition tracking (was_fighting).
Add Room.Contents fingerprint comparison (room ID + entity count).

Execution order: Client.Ready.State → Phase 1 dirty checks →
Affects → Enemies → Room.Contents → initialized=true.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

### Task 10: Full Test Suite Validation + Push

Final validation and push to remote.

**Files:** None (validation only)

- [ ] **Step 1: Run the full test suite**

Run: `cd /sentience && ./sent -test 2>&1 | tail -20`
Expected: Baseline passes (432+ tests). No new failures. Phase 3 adds 14 new tests.

- [ ] **Step 2: Clean build with release config**

Run: `cd /sentience/src && ./build clean release 2>&1 | tail -5`
Expected: Clean release build succeeds with no warnings.

- [ ] **Step 3: Push to remote**

Run: `cd /sentience/src && git push origin tieryo/skill_cleanup`
Expected: Push succeeds.

- [ ] **Step 4: Run code review**

Dispatch `superpowers:code-reviewer` agent to review all Phase 3 commits against the spec.

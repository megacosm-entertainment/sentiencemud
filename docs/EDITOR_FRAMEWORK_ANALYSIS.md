# Editor Framework Analysis: Unified OLC System

**Author**: AI Analysis  
**Date**: January 28, 2026  
**Status**: Initial Analysis

---

## Executive Summary

This document analyzes the editor systems across legacy (`/sentience/src`) and 20_dev (`/sentience/src_20_dev`) codebases to:
1. Define the common editor framework pattern in legacy
2. Identify outlier editors that don't follow the pattern
3. Recommend editors from 20_dev to port over
4. Design a unified approach for all editors
5. Create a foundation for player-accessible editing tools

### Key Findings

**Legacy (src/)**: Has migrated to a **modular, directory-based** structure with a common framework (`editors/common.c/h`). Most editors are in separate directories, but **outliers remain** (gameedit, socialedit in their directories, but cmdedit still needs work).

**20_dev (src_20_dev/)**: Has a **monolithic structure** with most editors crammed into `olc_act.c` (~25,701 lines). Contains valuable editors not in legacy: **raceedit**, **clsedit** (class editor), **skedit** (skill/spell editor), **songedit** (song editor), **sectoredit** (sector editor), **sgedit** (skill group editor).

---

## Part 1: Legacy Editor Framework (Current State)

### 1.1 Directory Structure (Modular Approach)

```
src/editors/
├── common.c              # Framework implementation
├── common.h              # Framework header (OLC_LAYOUT_CTX, tabs, themes)
├── areas/                # Area editor
├── blueprints/           # Blueprint editor
├── commands/             # Command editor (cmdedit.c)
├── dungeons/             # Dungeon editor
├── game_settings/        # Game settings editor (gameedit.c)
├── help/                 # Help file editor
├── mobiles/              # Mobile editor
├── objects/              # Object editor
├── projects/             # Project editor
├── random_strings/       # Random string generator editor
├── reserved_vnums/       # Reserved VNUM editor
├── rooms/                # Room editor
├── scripting/            # Script editors (mprog, oprog, rprog, etc.)
├── ships/                # Ship editor
├── socials/              # Social editor (socialedit.c)
├── tokens/               # Token editor
└── wilderness/           # Wilderness editor (wedit, vledit)
```

### 1.2 Common Framework Components

#### Core Framework (`editors/common.h` & `editors/common.c`)

**1. Layout Context System**
```c
typedef struct olc_layout_ctx {
    CHAR_DATA   *ch;            // Character viewing
    BUFFER      *buffer;        // Output buffer
    int         screen_width;   // Detected screen width (80-160)
    int         current_tab;    // Current tab index
    int         label_width;    // Calculated label column width
    int         value_width;    // Calculated value column width
} OLC_LAYOUT_CTX;

// Functions:
OLC_LAYOUT_CTX *olc_layout_new(CHAR_DATA *ch);
void olc_layout_free(OLC_LAYOUT_CTX *ctx);
void olc_init_editor(CHAR_DATA *ch, int editor_type, void *pEdit);
```

**2. Tab System**
```c
typedef struct olc_tab_def {
    const char *name;        // Full tab name (e.g., "General", "Combat")
    const char *short_name;  // Short name for narrow displays (e.g., "Gen", "Cbt")
} OLC_TAB_DEF;

typedef struct olc_editor_tabs {
    int          tab_count;              // Number of tabs
    OLC_TAB_DEF  tabs[OLC_MAX_TABS];     // Tab definitions
} OLC_EDITOR_TABS;
```

**3. Generic Command Processor**
```c
void process_olc_command(
    CHAR_DATA *ch,
    char *olc_argument,
    const struct olc_cmd_type olc_table[],
    OLC_FUN *show_func,
    void (*mark_changed_func)(void *pEdit, bool changed)
);
```

**How It Works**:
- Extract command word with `one_argument()`
- Handle `done` command automatically
- If no command, call `show_func`
- Search `olc_table[]` for command prefix match
- Execute command function, mark changes if returns `true`
- Fall back to `interpret()` if command not found

**4. Table Rendering System**
```c
typedef struct olc_table_theme {
    const char *border;
    const char *title_text;
    const char *label_text;
    const char *value_text;
    const char *click_text;
    const char *unset_text;
} OLC_TABLE_THEME;

// Themes: THEME_DEFAULT, THEME_DARK, THEME_COMPACT
```

**5. Screen Width Detection**
```c
int get_olc_screen_width(CHAR_DATA *ch);  // Returns 80-160 based on NAWS
int get_olc_screen_height(CHAR_DATA *ch);
```

### 1.3 Editor Pattern (Standard Implementation)

**Example: Area Editor** (`editors/areas/aedit.c`)

```c
// 1. Command table
const struct olc_cmd_type aedit_table[] = {
    { "?",         show_help      },
    { "commands",  show_commands  },
    { "create",    aedit_create   },
    { "name",      aedit_name     },
    { "show",      aedit_show     },
    // ... more commands
    { NULL,        NULL           }
};

// 2. Entry point command
void do_aedit(CHAR_DATA *ch, char *argument) {
    AREA_DATA *pArea;
    
    if (IS_NPC(ch) || ch->pcdata->security < 9)
        return;
    
    // Parse argument, find/create area
    pArea = /* lookup or create */;
    
    ch->desc->pEdit = (void *)pArea;
    ch->desc->editor = ED_AREA;
    aedit_show(ch, "");
}

// 3. Main editor loop
void aedit(CHAR_DATA *ch, char *argument) {
    process_olc_command(ch, argument, aedit_table, aedit_show, NULL);
}

// 4. Show function
AEDIT(aedit_show) {
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    
    BUFFER *buffer = new_buf();
    // Build display with tab system, tables, etc.
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return false;
}

// 5. Edit functions
AEDIT(aedit_name) {
    AREA_DATA *pArea;
    EDIT_AREA(ch, pArea);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: name <string>\n\r", ch);
        return false;
    }
    
    free_string(pArea->name);
    pArea->name = str_dup(argument);
    send_to_char("Area name set.\n\r", ch);
    return true;  // Returns true to mark changed
}
```

**Key Conventions**:
- Macro: `AEDIT(func)` expands to `bool func(CHAR_DATA *ch, char *argument)`
- Macro: `EDIT_AREA(ch, pArea)` extracts `pEdit` with validation
- Return `true` from edit functions to mark data as changed
- Return `false` from show/query functions

### 1.4 Outlier Editors (Non-Conforming)

#### 1.4.1 Game Settings Editor (`editors/game_settings/gameedit.c`)

**Current State**: Uses custom command dispatcher, not `process_olc_command()`

```c
void do_gameedit(CHAR_DATA *ch, char *argument) {
    char command[MAX_INPUT_LENGTH];
    
    argument = one_argument(argument, command);
    
    if (command[0] == '\0') {
        gameedit_show(ch, argument);
        return;
    }
    
    // Manual command dispatch
    if (!str_cmp(command, "show"))
        gameedit_show(ch, argument);
    else if (!str_cmp(command, "set"))
        gameedit_set(ch, argument);
    else if (!str_cmp(command, "confirm"))
        gameedit_confirm(ch, argument);
    // ... more manual if/else
}
```

**Why It's Different**:
- Doesn't use `olc_cmd_type` table
- Has unique "pending changes" system (confirm/revert workflow)
- Doesn't set `ch->desc->editor` or enter editor mode
- Operates as stateless command set rather than modal editor

**Recommendation**: **Keep as-is**. The pending changes system is appropriate for game-wide settings. Could add `GAMEEDIT()` macro for consistency, but modal editing not needed.

#### 1.4.2 Social Editor (`editors/socials/socialedit.c`)

**Current State**: Partially uses framework, but inconsistent

```c
const struct olc_cmd_type socialedit_table[] = {
    { "charnoarg",    socialedit_char_no_arg   },
    { "othersnoarg",  socialedit_others_no_arg },
    // ... more
    { NULL, NULL }
};

void do_socialedit(CHAR_DATA *ch, char *argument) {
    // Manual dispatch for create/list/save
    if (!str_cmp(command, "list")) {
        socialedit_list(ch, argument);
        return;
    }
    // Then finds social and sets editor mode
}

void socialedit(CHAR_DATA *ch, char *argument) {
    // Custom command processor (doesn't use process_olc_command)
    for (cmd = 0; socialedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, socialedit_table[cmd].name)) {
            (*socialedit_table[cmd].olc_fun)(ch, argument);
            return;
        }
    }
}
```

**Recommendation**: **Migrate to framework**. Should use `process_olc_command()` and follow standard pattern. Easy win.

#### 1.4.3 Command Editor (`editors/commands/cmdedit.c`)

**Status**: Unknown (file exists but not analyzed in detail)

**Recommendation**: Audit and migrate to framework if not already compliant.

---

## Part 2: 20_dev Editors (Porting Candidates)

### 2.1 Architecture Overview

**File**: `src_20_dev/olc_act.c` (~25,701 lines)

All editors in one monolithic file with shared infrastructure:

```c
// Editor dispatcher in olc.c
bool run_olc_editor(DESCRIPTOR_DATA *d) {
    switch (d->editor) {
        case ED_AREA:    aedit(d->character, d->incomm);    break;
        case ED_ROOM:    redit(d->character, d->incomm);    break;
        case ED_OBJECT:  oedit(d->character, d->incomm);    break;
        case ED_MOBILE:  medit(d->character, d->incomm);    break;
        case ED_SKEDIT:  skedit(d->character, d->incomm);   break;
        case ED_CLSEDIT: clsedit(d->character, d->incomm);  break;
        case ED_RACEEDIT: raceedit(d->character, d->incomm); break;
        case ED_SONGEDIT: songedit(d->character, d->incomm); break;
        // ... 35+ editors
    }
}
```

**Editor Pattern** (consistent across 20_dev):
```c
void skedit(CHAR_DATA *ch, char *argument) {
    char command[MIL];
    int  cmd;
    
    smash_tilde(argument);
    argument = one_argument(argument, command);
    
    if (!IS_IMPLEMENTOR(ch)) {
        send_to_char("SkEdit:  Insufficient security - action logged.\n\r", ch);
        edit_done(ch);
        return;
    }
    
    if (!str_cmp(command, "done")) {
        edit_done(ch);
        return;
    }
    
    ch->pcdata->immortal->last_olc_command = current_time;
    
    if (command[0] == '\0') {
        skedit_show(ch, argument);
        return;
    }
    
    // Tab system (numeric command switches tabs)
    if (is_number(command)) {
        int tab = atoi(command);
        if (tab < 1 || tab > ch->desc->nMaxEditTabs) {
            send_to_char("Huh?\n\r", ch);
            return;
        }
        ch->desc->nEditTab = tab - 1;
        skedit_show(ch, "");
        return;
    }
    
    // Command dispatch
    for (cmd = 0; skedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, skedit_table[cmd].name)) {
            if ((*skedit_table[cmd].olc_fun)(ch, argument)) {
                save_skills();  // Auto-save on change
            }
            return;
        }
    }
    
    interpret(ch, arg);
}
```

**Key Differences from Legacy Framework**:
- Manual command dispatch loop (doesn't use `process_olc_command`)
- Inline tab switching with numeric commands
- Auto-save on successful edit (no separate save command)
- Security checks inline in editor functions

### 2.2 Priority Editors to Port

#### 2.2.1 Race Editor (`raceedit` - ED_RACEEDIT)

**Location**: `src_20_dev/race.c` (raceedit functions) + `src_20_dev/olc.c` (table/dispatcher)

**Command Table**:
```c
const struct olc_cmd_type raceedit_table[] = {
    { "?",           show_help            },
    { "act",         raceedit_act         },
    { "aff",         raceedit_aff         },
    { "align",       raceedit_align       },
    { "commands",    show_commands        },
    { "comments",    raceedit_comments    },
    { "create",      raceedit_create      },
    { "description", raceedit_description },
    { "flags",       raceedit_flags       },
    { "form",        raceedit_form        },
    { "gr",          raceedit_gr          },
    { "imm",         raceedit_imm         },
    { "maxstats",    raceedit_maxstats    },
    { "maxvitals",   raceedit_maxvitals   },
    { "name",        raceedit_name        },
    { "off",         raceedit_off         },
    { "parts",       raceedit_parts       },
    { "remort",      raceedit_remort      },
    { "res",         raceedit_res         },
    { "show",        raceedit_show        },
    { "size",        raceedit_size        },
    { "skills",      raceedit_skills      },
    { "starting",    raceedit_starting    },
    { "stats",       raceedit_stats       },
    { "vuln",        raceedit_vuln        },
    { "who",         raceedit_who         },
    { NULL,          NULL                 }
};
```

**Show Function** (excerpt):
```c
RACEEDIT(raceedit_show) {
    RACE_DATA *race;
    EDIT_RACE(ch, race);
    BUFFER *buffer = new_buf();
    
    add_buf(buffer, formatf("%s Race: %s (%d)\n\r", 
        (race->playable ? "Player" : "NPC"), race->name, race->uid));
    
    if (race->pgr)
        add_buf(buffer, formatf("GR: %s\n", gr_to_name(race->pgr)));
    
    add_buf(buffer, "Description:\n\r");
    add_buf(buffer, string_indent(race->description, 3));
    
    add_buf(buffer, formatf("Act:           %s\n", 
        bitmatrix_string(act_flagbank, race->act)));
    add_buf(buffer, formatf("Affected:      %s\n", 
        bitmatrix_string(affect_flagbank, race->aff)));
    
    if (race->playable) {
        add_buf(buffer, "Stats:\n\r");
        for(int i = 0; i < MAX_STATS; i++) {
            add_buf(buffer, formatf(" - %-20s  %2d / %-2d\n\r", 
                flag_string(stat_types, i), race->stats[i], race->max_stats[i]));
        }
        
        add_buf(buffer, "Vitals:\n\r");
        for(int i = 0; i < 3; i++) {
            add_buf(buffer, formatf(" - %-10s  %d\n\r", 
                flag_string(vital_types, i), race->max_vitals[i]));
        }
    }
    
    page_to_char(buffer->string, ch);
    free_buf(buffer);
    return false;
}
```

**Data Structures** (from `merc.h` or similar):
```c
struct race_data {
    int         uid;              // Unique ID
    char        *name;            // Race name
    char        *description;     // Long description
    char        *comments;        // Builder notes
    bool        playable;         // PC or NPC race
    bool        starting;         // Can start as this race
    bool        remort;           // Remort-only race
    RACE_DATA   *premort;         // Required remort race
    char        *who;             // Who list display (6 chars)
    int         stats[MAX_STATS];       // Starting stats
    int         max_stats[MAX_STATS];   // Stat caps
    int         max_vitals[3];          // HP/Mana/Move caps
    int         min_size;               // Size range
    int         max_size;
    int         default_alignment;
    LLIST       *skills;          // Racial skills
    long        act;              // Act flags
    long        aff;              // Affect flags
    long        off;              // Offense flags
    long        imm;              // Immunities
    long        res;              // Resistances
    long        vuln;             // Vulnerabilities
    long        form;             // Body form
    long        parts;            // Body parts
    void        *pgr;             // Generic resource pointer
};
```

**Why Port This**:
- **Critical for character creation**: Players need races defined
- **Well-structured**: Clear separation of PC vs NPC races
- **Comprehensive**: Covers stats, vitals, flags, skills, remorts
- **No equivalent in legacy**: Current race system is hardcoded or JSON-only

**Porting Effort**: **Medium** (3-5 days)
- Need to verify RACE_DATA structure matches legacy
- Migrate functions to `src/editors/races/raceedit.c`
- Adapt to legacy framework (`process_olc_command`, layout system)
- Add tab system if needed (General, Stats, Combat, Skills)
- Integrate with JSON race loading/saving

---

#### 2.2.2 Class Editor (`clsedit` - ED_CLSEDIT)

**Location**: `src_20_dev/olc.c` (clsedit functions embedded)

**Command Table**:
```c
const struct olc_cmd_type clsedit_table[] = {
    { "?",           show_help           },
    { "commands",    show_commands       },
    { "create",      clsedit_create      },
    { "description", clsedit_description },
    { "display",     clsedit_display     },
    { "flags",       clsedit_flags       },
    { "gcl",         clsedit_gcl         },
    { "maxlevel",    clsedit_maxlevel    },
    { "name",        clsedit_name        },
    { "primary",     clsedit_primary     },
    { "show",        clsedit_show        },
    { "skills",      clsedit_skills      },
    { "type",        clsedit_type        },
    { "who",         clsedit_who         },
    { NULL,          NULL                }
};
```

**Data Structures** (inferred):
```c
struct class_data {
    int     uid;              // Unique ID
    char    *name;            // Class name
    char    *description;     // Long description
    char    *display;         // Display name
    char    *who;             // Who list (3 chars)
    int     type;             // Class type (base, prestige, etc.)
    int     max_level;        // Level cap
    bool    primary;          // Is primary class
    long    flags;            // Class flags
    LLIST   *skills;          // Class skills with levels
    void    *gcl;             // Generic class resource
};
```

**Why Port This**:
- **Essential for character progression**: Classes define abilities
- **Skill associations**: Links skills to classes with level requirements
- **Multiclass support**: Primary/secondary class flags
- **No equivalent in legacy**: Classes may be hardcoded

**Porting Effort**: **Medium** (2-4 days)
- Verify CLASS_DATA structure
- Extract functions from `olc.c` to `src/editors/classes/clsedit.c`
- Adapt to legacy framework
- Add skill association UI

---

#### 2.2.3 Skill/Spell Editor (`skedit` - ED_SKEDIT)

**Location**: `src_20_dev/olc.c` (skedit functions)

**Command Table** (partial):
```c
const struct olc_cmd_type skedit_table[] = {
    { "?",           show_help          },
    { "beatlag",     skedit_beatlag     },
    { "brandish",    skedit_brandishfunc },
    { "brew",        skedit_brewfunc     },
    { "cast",        skedit_castfunc     },
    { "commands",    show_commands      },
    { "components",  skedit_components  },
    { "damdice",     skedit_damdice     },
    { "damtype",     skedit_damtype     },
    { "difficulty",  skedit_difficulty  },
    { "equip",       skedit_equipfunc   },
    { "flags",       skedit_flags       },
    { "imbue",       skedit_imbuefunc   },
    { "ink",         skedit_inkfunc     },
    { "interrupt",   skedit_interruptfunc },
    { "level",       skedit_level       },
    { "mana",        skedit_mana        },
    { "name",        skedit_name        },
    { "pulse",       skedit_pulsefunc   },
    { "range",       skedit_range       },
    { "school",      skedit_school      },
    { "show",        skedit_show        },
    { "slot",        skedit_slot        },
    { "sphere",      skedit_sphere      },
    { "target",      skedit_target      },
    { "type",        skedit_type        },
    // ... 40+ commands
    { NULL,          NULL               }
};
```

**Why Port This**:
- **Massive flexibility**: Edit skills and spells in-game
- **Complex attributes**: Cast functions, brew functions, imbue functions
- **Balance tuning**: Mana costs, beat lag, difficulty, damage dice
- **Component system**: Material components for spells
- **Spell schools/spheres**: Magic system organization

**Porting Effort**: **High** (1-2 weeks)
- Very large command set (40+ commands)
- Complex function pointer system (cast, brew, pulse, etc.)
- Need to verify SKILL_DATA structure matches legacy
- May need multiple tabs (General, Combat, Casting, Brewing, etc.)

---

#### 2.2.4 Song Editor (`songedit` - ED_SONGEDIT)

**Location**: `src_20_dev/music.c`

**Pattern**: Uses macros for repetitive song function editing:
```c
#define SONGEDIT_FUNC(f, p, t, n) \
SONGEDIT(songedit_##f##func) \
{ \
    /* Complex macro implementation for editing song functions */ \
}

SONGEDIT_FUNC(presong, TOKEN_PRESONG, SONG_FUN, NULL)
SONGEDIT_FUNC(song, TOKEN_SONG, SONG_FUN, NULL)
```

**Why Port This**:
- **Bard system**: If legacy has bards, songs are critical
- **Function-based editing**: Similar to skill editor complexity
- **Token system integration**: Uses TOKEN_* enums

**Porting Effort**: **Medium-High** (3-5 days)
- Depends on whether legacy has song/music system
- Complex macro-based implementation
- Need to understand token system

---

#### 2.2.5 Sector Editor (`sectoredit` - ED_SECTOREDIT)

**Location**: `src_20_dev/olc.c`

**Command Table** (partial):
```c
const struct olc_cmd_type sectoredit_table[] = {
    { "?",           show_help               },
    { "affinity",    sectoredit_affinity     },
    { "class",       sectoredit_class        },
    { "commands",    show_commands           },
    { "comments",    sectoredit_comments     },
    { "create",      sectoredit_create       },
    { "description", sectoredit_description  },
    { "flags",       sectoredit_flags        },
    { "gsct",        sectoredit_gsct         },
    { "health",      sectoredit_health       },
    { "hidemsgs",    sectoredit_hidemsgs     },
    { "mana",        sectoredit_mana         },
    { "move",        sectoredit_move         },
    // ...
};
```

**Why Port This**:
- **Environmental system**: Sector types define terrain behavior
- **Movement costs**: Health/mana/move regeneration per sector
- **Affinity system**: Elemental affinities for sectors
- **Hide messages**: Stealth system integration

**Porting Effort**: **Low-Medium** (2-3 days)
- Relatively simple structure
- May need to verify against legacy sector system

---

#### 2.2.6 Skill Group Editor (`sgedit` - ED_SGEDIT)

**Location**: `src_20_dev/olc.c`

**Command Table**:
```c
const struct olc_cmd_type sgedit_table[] = {
    { "?",        show_help      },
    { "add",      sgedit_add     },
    { "clear",    sgedit_clear   },
    { "commands", show_commands  },
    { "create",   sgedit_create  },
    { "remove",   sgedit_remove  },
    { "show",     sgedit_show    },
    { NULL,       NULL           }
};
```

**Purpose**: Groups skills together (e.g., "Weapon Proficiency" group contains "sword", "axe", "dagger")

**Why Port This**:
- **Skill organization**: Logical grouping for players and builders
- **Class skill assignments**: Assign groups instead of individual skills
- **Simple implementation**: Only 7 commands

**Porting Effort**: **Low** (1-2 days)

---

### 2.3 Editor Porting Priority Matrix

| Editor        | Priority | Effort   | Complexity | Reason                                    |
|---------------|----------|----------|------------|-------------------------------------------|
| **raceedit**  | Critical | Medium   | Medium     | Essential for PC creation, no legacy equiv |
| **clsedit**   | Critical | Medium   | Medium     | Essential for character progression       |
| **sgedit**    | High     | Low      | Low        | Skill organization, easy port             |
| **sectoredit**| High     | Low-Med  | Low        | Environmental system tuning               |
| **skedit**    | High     | High     | High       | Massive command set, complex functions    |
| **songedit**  | Medium   | Med-High | Medium     | Only if legacy has bard system            |

**Recommended Porting Order**:
1. **sgedit** (1-2 days) - Quick win, useful for skill editor
2. **raceedit** (3-5 days) - Critical for character system
3. **clsedit** (2-4 days) - Critical for character system
4. **sectoredit** (2-3 days) - Environmental tuning
5. **skedit** (1-2 weeks) - Complex but powerful
6. **songedit** (3-5 days) - If needed

**Total Estimated Effort**: 3-5 weeks for priority editors

---

## Part 3: Unified Framework Strategy

### 3.1 Standardization Goals

1. **All editors use `process_olc_command()`**
   - Eliminates duplicate command dispatch code
   - Ensures consistent behavior (done, show, interpret fallback)

2. **All editors follow directory structure**
   ```
   src/editors/<category>/<editor>.c
   ```

3. **All editors use macro conventions**
   ```c
   #define RACEEDIT(fun) bool fun(CHAR_DATA *ch, char *argument)
   #define EDIT_RACE(ch, race) (race = (RACE_DATA *)ch->desc->pEdit)
   ```

4. **All editors use tab system**
   - For complex editors with 10+ fields
   - Tab navigation: just type tab number (e.g., `2` switches to tab 2)

5. **All editors use layout context**
   - Responsive width (80-160 columns)
   - Consistent table rendering

### 3.2 Migration Steps for Outliers

#### 3.2.1 Social Editor Migration

**Current**: Custom command processor in `socialedit()`

**Target**: Use `process_olc_command()`

**Changes**:
```c
// Before (custom loop):
void socialedit(CHAR_DATA *ch, char *argument) {
    for (cmd = 0; socialedit_table[cmd].name != NULL; cmd++) {
        if (!str_prefix(command, socialedit_table[cmd].name)) {
            (*socialedit_table[cmd].olc_fun)(ch, argument);
            return;
        }
    }
}

// After (framework):
void socialedit(CHAR_DATA *ch, char *argument) {
    process_olc_command(ch, argument, socialedit_table, socialedit_show, 
        (void (*)(void *, bool))mark_socials_changed);
}
```

**Benefit**: Removes ~20 lines of duplicate code, adds automatic `done` handling

---

#### 3.2.2 Command Editor Audit

**Task**: Examine `editors/commands/cmdedit.c`

**Questions**:
- Does it use `process_olc_command()` or custom dispatcher?
- Does it follow macro conventions (`CMDEDIT()`, `EDIT_CMD()`)?
- Does it use layout context?

**Action**: Migrate if non-compliant.

---

### 3.3 Adding New Editors (Standard Template)

**File**: `src/editors/<category>/<editor>.c`

```c
/***************************************************************************
 *  <Editor Name> - OLC Editor for <Data Type>
 **************************************************************************/

#include "../../merc.h"
#include "../../olc.h"
#include "../../interp.h"
#include "../../tables.h"
#include "../common.h"

// Forward declarations
DECLARE_OLC_FUN(editorname_show);
DECLARE_OLC_FUN(editorname_create);
DECLARE_OLC_FUN(editorname_name);
// ... more

// Command table
const struct olc_cmd_type editorname_table[] = {
    { "?",         show_help           },
    { "commands",  show_commands       },
    { "create",    editorname_create   },
    { "name",      editorname_name     },
    { "show",      editorname_show     },
    { NULL,        NULL                }
};

// Entry point command
void do_editorname(CHAR_DATA *ch, char *argument) {
    DATA_TYPE *pData;
    char arg[MAX_INPUT_LENGTH];
    
    if (IS_NPC(ch) || ch->pcdata->security < MIN_SECURITY_LEVEL)
        return;
    
    argument = one_argument(argument, arg);
    
    if (arg[0] == '\0') {
        send_to_char("Syntax: editorname <name>\n\r", ch);
        send_to_char("        editorname create <name>\n\r", ch);
        return;
    }
    
    if (!str_cmp(arg, "create")) {
        editorname_create(ch, argument);
        return;
    }
    
    // Find existing data
    pData = lookup_function(arg);
    if (!pData) {
        send_to_char("No such <data> found.\n\r", ch);
        return;
    }
    
    // Enter edit mode
    ch->desc->pEdit = (void *)pData;
    ch->desc->editor = ED_<EDITORNAME>;
    editorname_show(ch, "");
}

// Main editor loop
void editorname(CHAR_DATA *ch, char *argument) {
    process_olc_command(ch, argument, editorname_table, editorname_show, NULL);
}

// Show function
EDITORNAME(editorname_show) {
    DATA_TYPE *pData;
    EDIT_<DATA>(ch, pData);
    
    BUFFER *buffer = new_buf();
    
    // Build display using layout context if needed
    // OLC_LAYOUT_CTX *ctx = olc_layout_new(ch);
    
    add_buf(buffer, formatf("Name: %s\n\r", pData->name));
    // ... more fields
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    // olc_layout_free(ctx);
    return false;
}

// Create function
EDITORNAME(editorname_create) {
    DATA_TYPE *pData;
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: create <name>\n\r", ch);
        return false;
    }
    
    // Allocate and initialize
    pData = alloc_perm(sizeof(DATA_TYPE));
    pData->name = str_dup(argument);
    // ... initialize fields
    
    // Add to global list
    add_to_list(pData);
    
    // Enter edit mode
    ch->desc->pEdit = (void *)pData;
    ch->desc->editor = ED_<EDITORNAME>;
    
    send_to_char("<Data> created.\n\r", ch);
    editorname_show(ch, "");
    return true;
}

// Field edit function
EDITORNAME(editorname_name) {
    DATA_TYPE *pData;
    EDIT_<DATA>(ch, pData);
    
    if (argument[0] == '\0') {
        send_to_char("Syntax: name <string>\n\r", ch);
        return false;
    }
    
    free_string(pData->name);
    pData->name = str_dup(argument);
    
    send_to_char("<Data> name set.\n\r", ch);
    return true;  // Returns true to mark changed
}
```

**Add to `olc.h`**:
```c
// Editor type constant
#define ED_<EDITORNAME>  <next_number>

// Macro definitions
#define EDITORNAME(fun) bool fun(CHAR_DATA *ch, char *argument)
#define EDIT_<DATA>(ch, pData) (pData = (DATA_TYPE *)ch->desc->pEdit)

// Function declarations
void do_editorname(CHAR_DATA *ch, char *argument);
void editorname(CHAR_DATA *ch, char *argument);
DECLARE_OLC_FUN(editorname_show);
DECLARE_OLC_FUN(editorname_create);
// ... more

// Command table
extern const struct olc_cmd_type editorname_table[];
```

**Add to `olc.c` dispatcher**:
```c
bool run_olc_editor(DESCRIPTOR_DATA *d) {
    switch (d->editor) {
        // ... existing cases
        case ED_<EDITORNAME>:
            editorname(d->character, d->incomm);
            break;
    }
}

char *olc_ed_name(CHAR_DATA *ch) {
    if (ch->desc->editor > 0 && ch->desc->editor < elementsof(editor_name_table))
        return editor_name_table[ch->desc->editor];
    return editor_name_table[0];
}

// Add to editor_name_table[]
const char * const editor_name_table[] = {
    "None",
    "AEdit",
    // ... existing editors
    "<EditorName>",
    NULL
};
```

**Add to `show_commands()`**:
```c
bool show_commands(CHAR_DATA *ch, char *argument) {
    switch (ch->desc->editor) {
        // ... existing cases
        case ED_<EDITORNAME>:
            show_olc_cmds(ch, editorname_table);
            break;
    }
}
```

---

## Part 4: Player-Accessible Editing Tools

### 4.1 Requirements & Scope

**Use Cases**:
1. **Home editing**: Players edit their own house/apartment rooms
2. **Clan hall editing**: Clan leaders edit clan territory
3. **Shop stocking**: Shop owners configure merchandise
4. **Personal item creation**: Crafting system integration

**Security Constraints**:
- Players can only edit areas they own (house flag, clan ownership)
- No script editing (security risk)
- No VNUM manipulation (prevent conflicts)
- Limited flag access (no GOD flags, etc.)
- Audit logging for all player edits

### 4.2 Architecture: Permission-Based Editor Wrapper

**Concept**: Reuse existing editor code, but wrap in permission layer

**Implementation**:
```c
// New file: src/editors/player/playeredit.c

typedef struct player_edit_permission {
    int         editor_type;      // ED_ROOM, ED_OBJECT, etc.
    bool        can_create;       // Can create new instances
    bool        can_delete;       // Can delete instances
    long        allowed_flags;    // Bitfield of allowed flags
    long        forbidden_flags;  // Bitfield of forbidden flags
    bool        (*validate_func)(CHAR_DATA *ch, void *pData); // Permission check
} PLAYER_EDIT_PERMISSION;

// Example: Room editing permission
bool validate_room_edit(CHAR_DATA *ch, void *pData) {
    ROOM_INDEX_DATA *room = (ROOM_INDEX_DATA *)pData;
    
    // Check house ownership
    if (IS_SET(room->room_flags, ROOM_HOUSE)) {
        if (room->owner && !str_cmp(room->owner, ch->name))
            return true;
    }
    
    // Check clan ownership
    if (ch->clan && room->clan == ch->clan->id)
        return true;
    
    return false;
}

PLAYER_EDIT_PERMISSION room_permission = {
    .editor_type = ED_ROOM,
    .can_create = false,
    .can_delete = false,
    .allowed_flags = ROOM_DARK | ROOM_NO_MOB | ROOM_INDOORS,
    .forbidden_flags = ROOM_GODS_ONLY | ROOM_SAFE | ROOM_IMP_ONLY,
    .validate_func = validate_room_edit
};

// Wrapper function
void do_homeedit(CHAR_DATA *ch, char *argument) {
    ROOM_INDEX_DATA *room;
    char arg[MAX_INPUT_LENGTH];
    
    if (IS_NPC(ch)) {
        send_to_char("NPCs cannot edit homes.\n\r", ch);
        return;
    }
    
    argument = one_argument(argument, arg);
    
    // Default to current room if no argument
    if (arg[0] == '\0') {
        room = ch->in_room;
    } else {
        int vnum = atoi(arg);
        room = get_room_index(vnum);
    }
    
    if (!room) {
        send_to_char("No such room.\n\r", ch);
        return;
    }
    
    // Validate permission
    if (!validate_room_edit(ch, room)) {
        send_to_char("You don't have permission to edit this room.\n\r", ch);
        log_string(formatf("SECURITY: %s attempted to edit room %d without permission", 
            ch->name, room->vnum));
        return;
    }
    
    // Store permission in descriptor for later checks
    ch->desc->player_edit_perm = &room_permission;
    
    // Enter standard room editor
    ch->desc->pEdit = (void *)room;
    ch->desc->editor = ED_ROOM;
    redit_show(ch, "");
}

// Modified room editor to check permissions
REDIT(redit_flags) {
    ROOM_INDEX_DATA *room;
    EDIT_ROOM(ch, room);
    
    // Check if player editing mode
    if (ch->desc->player_edit_perm) {
        PLAYER_EDIT_PERMISSION *perm = ch->desc->player_edit_perm;
        
        int flag;
        if ((flag = flag_value(room_flags, argument)) != NO_FLAG) {
            // Check if flag is allowed
            if (IS_SET(perm->forbidden_flags, flag)) {
                send_to_char("You cannot set that flag.\n\r", ch);
                return false;
            }
            
            if (!IS_SET(perm->allowed_flags, flag)) {
                send_to_char("You cannot set that flag.\n\r", ch);
                return false;
            }
        }
    }
    
    // Continue with normal flag editing
    // ... existing code
}
```

### 4.3 Player Editor Command Set

**Home Editing**:
```
homeedit             - Edit current room (if owned)
homeedit <vnum>      - Edit specific room (if owned)
homeedit list        - List all rooms you can edit
```

**Available commands in homeedit**:
- `name <string>` - Room name
- `description` - Edit description (string editor)
- `flags <flag>` - Toggle allowed flags (dark, indoors, no_mob)
- `ed <keyword>` - Extra descriptions
- `show` - Show room details
- `done` - Exit editor

**Forbidden commands**:
- No VNUM changes
- No sector changes (could break navigation)
- No script editing
- No reset editing
- No dangerous flags (safe, gods_only, etc.)

**Clan Hall Editing**:
```
clanedit             - Edit clan hall rooms (leader only)
clanedit <vnum>      - Edit specific clan room
```

**Shop Editing**:
```
shopedit             - Edit your shop (if you own one)
shopedit list        - List your shops
```

**Available commands**:
- `profit <buy> <sell>` - Profit margins
- `hours <open> <close>` - Operating hours
- `types <item_type>` - What item types shop buys
- `show` - Show shop details

### 4.4 Audit Logging

**Log all player edits**:
```c
void log_player_edit(CHAR_DATA *ch, int editor_type, void *pData, 
                     const char *command, const char *argument) {
    char buf[MAX_STRING_LENGTH];
    
    switch (editor_type) {
        case ED_ROOM:
            sprintf(buf, "PLAYEREDIT: %s edited room %d - %s %s",
                ch->name, ((ROOM_INDEX_DATA *)pData)->vnum, command, argument);
            break;
        case ED_OBJECT:
            sprintf(buf, "PLAYEREDIT: %s edited object %d - %s %s",
                ch->name, ((OBJ_INDEX_DATA *)pData)->vnum, command, argument);
            break;
        // ... other types
    }
    
    log_string(buf);
    append_file(ch, PLAYER_EDIT_LOG, buf);
}
```

**Review command for staff**:
```
playeredits <player>        - Show all edits by player
playeredits <player> <days> - Show edits in last N days
playeredits room <vnum>     - Show all edits to a room
```

### 4.5 Safety Features

**1. Undo/Revert System**:
```c
typedef struct edit_snapshot {
    void        *data;          // Copy of original data
    time_t      timestamp;      // When snapshot was taken
    int         editor_type;    // What was being edited
} EDIT_SNAPSHOT;

// Take snapshot before first edit
void player_edit_snapshot(CHAR_DATA *ch, void *pData, int editor_type) {
    EDIT_SNAPSHOT *snapshot = alloc_mem(sizeof(EDIT_SNAPSHOT));
    
    // Deep copy data
    switch (editor_type) {
        case ED_ROOM:
            snapshot->data = room_deep_copy((ROOM_INDEX_DATA *)pData);
            break;
        // ... other types
    }
    
    snapshot->timestamp = current_time;
    snapshot->editor_type = editor_type;
    ch->desc->edit_snapshot = snapshot;
}

// Revert command
REDIT(player_redit_revert) {
    if (!ch->desc->edit_snapshot) {
        send_to_char("No changes to revert.\n\r", ch);
        return false;
    }
    
    EDIT_SNAPSHOT *snapshot = ch->desc->edit_snapshot;
    
    // Restore from snapshot
    room_restore_from_copy(room, (ROOM_INDEX_DATA *)snapshot->data);
    
    send_to_char("All changes reverted.\n\r", ch);
    return true;
}
```

**2. Change Preview**:
```c
REDIT(player_redit_preview) {
    EDIT_SNAPSHOT *snapshot = ch->desc->edit_snapshot;
    
    if (!snapshot) {
        send_to_char("No changes to preview.\n\r", ch);
        return false;
    }
    
    BUFFER *buffer = new_buf();
    add_buf(buffer, "Changes made:\n\r\n\r");
    
    // Compare current data with snapshot
    room_show_diff(buffer, (ROOM_INDEX_DATA *)snapshot->data, room);
    
    page_to_char(buf_string(buffer), ch);
    free_buf(buffer);
    return false;
}
```

**3. Auto-save with Timer**:
```c
void player_edit_auto_save(CHAR_DATA *ch) {
    // Save every 5 minutes of idle time in editor
    if (ch->desc->editor != ED_NONE && 
        ch->pcdata && ch->pcdata->immortal &&
        current_time - ch->pcdata->immortal->last_olc_command > 300) {
        
        // Auto-save based on editor type
        switch (ch->desc->editor) {
            case ED_ROOM:
                save_room((ROOM_INDEX_DATA *)ch->desc->pEdit);
                send_to_char("{YAuto-saved.{x\n\r", ch);
                break;
            // ... other types
        }
    }
}
```

---

## Part 5: Implementation Roadmap

### Phase 1: Framework Standardization (1-2 weeks)

**Tasks**:
1. ✅ Document current framework in `editors/common.h`
2. Migrate social editor to use `process_olc_command()`
3. Audit command editor for compliance
4. Create standard editor template documentation
5. Add missing macros to `olc.h` for all editors

**Deliverables**:
- All editors use `process_olc_command()`
- Consistent macro conventions across all editors
- Documentation for adding new editors

---

### Phase 2: Port Priority Editors from 20_dev (3-5 weeks)

**Week 1: Quick Wins**
- Day 1-2: Port **sgedit** (skill groups)
  - Create `src/editors/skill_groups/sgedit.c`
  - Adapt to framework
  - Test with existing skills

**Week 2-3: Critical Editors**
- Day 3-7: Port **raceedit**
  - Create `src/editors/races/raceedit.c`
  - Verify RACE_DATA structure
  - Add tab system (General, Stats, Combat, Skills)
  - Integrate with JSON race loading
  - Test PC creation workflow

- Day 8-11: Port **clsedit**
  - Create `src/editors/classes/clsedit.c`
  - Verify CLASS_DATA structure
  - Add skill association UI
  - Test multiclass system

**Week 3-4: Environmental System**
- Day 12-14: Port **sectoredit**
  - Create `src/editors/sectors/sectoredit.c`
  - Add affinity/regen/movement cost editing
  - Test with wilderness system

**Week 4-5: Complex Skill Editor**
- Day 15-25: Port **skedit**
  - Create `src/editors/skills/skedit.c`
  - Break into multiple tabs (General, Combat, Casting, Brewing, etc.)
  - Test function pointer assignments
  - Verify component system

**Optional (if needed)**:
- Day 26-30: Port **songedit** (only if bard system exists)

**Deliverables**:
- 5-6 new editors operational
- Documentation for each ported editor
- Test cases for critical functionality

---

### Phase 3: Player Editing System (2-3 weeks)

**Week 1: Core Infrastructure**
- Day 1-3: Implement `PLAYER_EDIT_PERMISSION` system
  - Create `src/editors/player/playeredit.c`
  - Add permission validation functions
  - Add audit logging

- Day 4-5: Create `do_homeedit()` command
  - Room editing for house owners
  - Limited command set
  - Test with player-owned houses

**Week 2: Clan & Shop Systems**
- Day 6-8: Create `do_clanedit()` command
  - Clan leader permissions
  - Clan room editing
  - Test with clan halls

- Day 9-10: Create `do_shopedit()` command
  - Shop owner permissions
  - Stock/profit editing
  - Test with player shops

**Week 3: Safety & Polish**
- Day 11-12: Implement undo/snapshot system
- Day 13-14: Add change preview commands
- Day 15: Add auto-save with timer
- Day 16-17: Create `playeredits` review command for staff
- Day 18-20: Documentation and player guides

**Deliverables**:
- `homeedit`, `clanedit`, `shopedit` commands
- Audit logging system
- Undo/preview/auto-save features
- Staff review tools
- Player documentation

---

### Phase 4: Polish & Documentation (1 week)

**Tasks**:
1. Create comprehensive editor documentation
2. Add `help` files for all editors
3. Create builder tutorials
4. Add tab navigation help
5. Create player editing guide

**Deliverables**:
- `docs/EDITOR_GUIDE.md` - Complete editor reference
- In-game help files
- Builder tutorials
- Player editing guide

---

## Part 6: Summary & Recommendations

### 6.1 Current State Assessment

**Legacy (src/)**:
- ✅ **Strong foundation**: Modular structure, common framework
- ✅ **Process standardization**: `process_olc_command()` used by most editors
- ⚠️ **Minor outliers**: Social editor, possibly command editor
- ✅ **Good separation**: Each editor in own directory

**20_dev (src_20_dev/)**:
- ⚠️ **Monolithic**: 25,701-line `olc_act.c` file
- ✅ **Feature-rich**: Has race, class, skill, song editors missing in legacy
- ⚠️ **Duplicate code**: Each editor has manual command dispatcher
- ✅ **Consistent pattern**: All editors follow similar structure (easy to port)

### 6.2 Strategic Recommendations

#### Immediate Actions (Next 2 weeks)
1. **Standardize social editor** - Migrate to `process_olc_command()` (1 day)
2. **Audit command editor** - Verify compliance with framework (1 day)
3. **Port skill group editor** - Quick win, useful foundation (2 days)
4. **Document standard template** - Clear guide for future editors (2 days)

#### Short-term Goals (Next 2 months)
1. **Port critical editors from 20_dev**:
   - **raceedit** (Week 1-2)
   - **clsedit** (Week 3)
   - **sectoredit** (Week 4)
   - **skedit** (Week 5-6)

2. **Begin player editing system**:
   - **Permission framework** (Week 7)
   - **homeedit command** (Week 8)

#### Long-term Goals (3-6 months)
1. **Complete player editing suite**:
   - Clan editing
   - Shop editing
   - Crafting integration

2. **Advanced features**:
   - Version control for player edits
   - Collaborative editing (multiple builders)
   - Template/blueprint system for common patterns
   - AI-assisted description generation

### 6.3 Key Principles Going Forward

1. **Always use `process_olc_command()`** - No custom dispatchers
2. **Follow directory structure** - `src/editors/<category>/<editor>.c`
3. **Use macro conventions** - `EDITNAME()`, `EDIT_DATA()`
4. **Add tab systems for complex editors** - 10+ fields need tabs
5. **Document everything** - Every editor needs help files
6. **Security first** - Player editors need permission layers
7. **Audit everything** - Log all player edits for review
8. **Test thoroughly** - Editors modify core game data

### 6.4 Success Metrics

**Framework Standardization**:
- ✅ 100% of editors use `process_olc_command()`
- ✅ 100% of editors follow directory structure
- ✅ 100% of editors have help files

**20_dev Editor Ports**:
- ✅ Race editor operational and tested
- ✅ Class editor operational and tested
- ✅ Skill editor operational and tested
- ✅ Skill group editor operational
- ✅ Sector editor operational

**Player Editing System**:
- ✅ `homeedit` command tested with player houses
- ✅ `clanedit` command tested with clan halls
- ✅ `shopedit` command tested with player shops
- ✅ Audit logging captures all player edits
- ✅ Staff review command shows edit history
- ✅ Undo/preview system prevents mistakes

**Documentation**:
- ✅ Complete editor reference guide
- ✅ Builder tutorials for all editors
- ✅ Player editing guide
- ✅ Standard template for new editors

---

## Part 7: Questions for Decision-Making

1. **Race/Class System**: Does legacy use the same RACE_DATA/CLASS_DATA structures as 20_dev, or is there a schema difference?

2. **Song System**: Does legacy have a bard/music system that would benefit from songedit, or is this 20_dev-specific?

3. **Skill System**: Are skills in legacy stored the same way as 20_dev (SKILL_DATA with function pointers)?

4. **Player Editing Scope**: What specific player editing features are highest priority?
   - Home editing?
   - Clan hall editing?
   - Shop editing?
   - Crafting integration?

5. **Security Levels**: What trust level should players have for editing? Should there be "builder players" vs "normal players"?

6. **Auto-Save**: Should player edits auto-save immediately, or use a confirm/revert system like gameedit?

7. **JSON Migration**: Should ported editors use JSON persistence or legacy .are format?

---

## Appendices

### Appendix A: Editor Type Constants

**Current in legacy** (`src/olc.h`):
```c
#define ED_NONE             0
#define ED_AREA             1
#define ED_ROOM             2
#define ED_OBJECT           3
#define ED_MOBILE           4
#define ED_MPCODE           5
#define ED_OPCODE           6
#define ED_RPCODE           7
#define ED_SHIP             8
#define ED_HELP             9
#define ED_TPCODE           10
#define ED_TOKEN            11
#define ED_PROJECT          12
#define ED_RSG              13
#define ED_WILDS            14
#define ED_VLINK            15
#define ED_BPSECT           16
#define ED_BLUEPRINT        17
#define ED_DUNGEON          18
#define ED_APCODE           19
#define ED_IPCODE           20
#define ED_DPCODE           21
#define ED_CMDEDIT          22
#define ED_CHANGESET        23
#define ED_ACCNOTE          24
#define ED_CHLOG            25
#define ED_SOCIAL           26
#define ED_GAMESETTING      27
```

**Need to add**:
```c
#define ED_RACE             28
#define ED_CLASS            29
#define ED_SKILL            30
#define ED_SKILLGROUP       31
#define ED_SECTOR           32
#define ED_SONG             33  // If needed
```

### Appendix B: Macro Naming Conventions

| Editor        | Macro         | Edit Macro       | Data Type           |
|---------------|---------------|------------------|---------------------|
| Area          | `AEDIT(fun)`  | `EDIT_AREA()`    | `AREA_DATA`         |
| Room          | `REDIT(fun)`  | `EDIT_ROOM()`    | `ROOM_INDEX_DATA`   |
| Object        | `OEDIT(fun)`  | `EDIT_OBJ()`     | `OBJ_INDEX_DATA`    |
| Mobile        | `MEDIT(fun)`  | `EDIT_MOB()`     | `MOB_INDEX_DATA`    |
| Help          | `HEDIT(fun)`  | `EDIT_HELP()`    | `HELP_DATA`         |
| Ship          | `SHEDIT(fun)` | `EDIT_SHIP()`    | `SHIP_DATA`         |
| Token         | `TEDIT(fun)`  | `EDIT_TOKEN()`   | `TOKEN_INDEX_DATA`  |
| Project       | `PEDIT(fun)`  | `EDIT_PROJECT()` | `PROJECT_DATA`      |
| Social        | `SOCEDIT(fun)`| `EDIT_SOCIAL()`  | `social_type`       |
| Command       | `CMDEDIT(fun)`| `EDIT_CMD()`     | `cmd_type`          |
| **Race**      | `RACEEDIT()`  | `EDIT_RACE()`    | `RACE_DATA`         |
| **Class**     | `CLSEDIT()`   | `EDIT_CLASS()`   | `CLASS_DATA`        |
| **Skill**     | `SKEDIT()`    | `EDIT_SKILL()`   | `SKILL_DATA`        |
| **SkillGroup**| `SGEDIT()`    | `EDIT_SGROUP()`  | `SKILL_GROUP`       |
| **Sector**    | `SECTEDIT()`  | `EDIT_SECTOR()`  | `SECTOR_DATA`       |
| **Song**      | `SONGEDIT()`  | `EDIT_SONG()`    | `SONG_DATA`         |

### Appendix C: File Locations After Migration

```
src/
├── editors/
│   ├── common.c / common.h               # Framework
│   ├── areas/
│   │   └── aedit.c
│   ├── blueprints/
│   │   ├── bsedit.c                      # Blueprint section
│   │   └── bpedit.c                      # Blueprint
│   ├── classes/                          # NEW
│   │   └── clsedit.c                     # ← Port from 20_dev
│   ├── commands/
│   │   └── cmdedit.c
│   ├── dungeons/
│   │   └── dngedit.c
│   ├── game_settings/
│   │   └── gameedit.c
│   ├── help/
│   │   └── hedit.c
│   ├── mobiles/
│   │   └── medit.c
│   ├── objects/
│   │   └── oedit.c
│   ├── player/                           # NEW
│   │   ├── playeredit.c                  # Permission framework
│   │   ├── homeedit.c                    # Home editing
│   │   ├── clanedit.c                    # Clan editing
│   │   └── shopedit.c                    # Shop editing
│   ├── projects/
│   │   └── pedit.c
│   ├── races/                            # NEW
│   │   └── raceedit.c                    # ← Port from 20_dev
│   ├── random_strings/
│   │   └── rsgedit.c
│   ├── reserved_vnums/
│   ├── rooms/
│   │   └── redit.c
│   ├── scripting/
│   │   ├── mpedit.c                      # Mob programs
│   │   ├── opedit.c                      # Object programs
│   │   ├── rpedit.c                      # Room programs
│   │   ├── tpedit.c                      # Token programs
│   │   ├── apedit.c                      # Area programs
│   │   ├── ipedit.c                      # Instance programs
│   │   └── dpedit.c                      # Dungeon programs
│   ├── sectors/                          # NEW
│   │   └── sectoredit.c                  # ← Port from 20_dev
│   ├── ships/
│   │   └── shedit.c
│   ├── skills/                           # NEW
│   │   ├── skedit.c                      # ← Port from 20_dev
│   │   └── sgedit.c                      # ← Port from 20_dev (skill groups)
│   ├── socials/
│   │   └── socialedit.c
│   ├── songs/                            # NEW (if needed)
│   │   └── songedit.c                    # ← Port from 20_dev
│   ├── tokens/
│   │   └── tedit.c
│   └── wilderness/
│       ├── wedit.c                       # Wilderness
│       └── vledit.c                      # Virtual links
```

---

## Conclusion

Legacy has established a solid, modular editor framework with excellent separation of concerns. The path forward is clear:

1. **Standardize remaining outliers** (social, command editors)
2. **Port critical editors from 20_dev** (race, class, skill systems)
3. **Build player editing system** on existing framework
4. **Document everything** for future maintainability

The framework is already 90% of the way there. This analysis provides a concrete roadmap to complete the unification and extend editing capabilities to players in a safe, controlled manner.

**Total Estimated Effort**: 8-10 weeks for complete implementation  
**Highest Priority**: Race and class editors (critical for character system)  
**Stretch Goal**: Full player editing with home/clan/shop support

---

**Next Steps**:
1. Review this analysis with team
2. Prioritize which editors to port first
3. Answer decision-making questions (Part 7)
4. Begin Phase 1 standardization work

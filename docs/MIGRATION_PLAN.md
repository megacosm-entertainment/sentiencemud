# Sentience 1.5 Migration Plan: Data-Driven Race/Class/Token Systems

**Date**: 2026-01-06
**Branch**: auth_refactor → race_class_modernization
**Goal**: Create truly data-driven race/class/title systems with pronoun support

---

## Executive Summary

Sentience 2.0 introduced a **token system** for data-driven skills/spells but kept races/classes hardcoded. Sentience 1.0 added a **pronoun system** after the 2.0 split. Sentience 1.5 will:

1. Port the token system from 2.0
2. **Go beyond 2.0** by making races and classes fully data-driven (loaded from JSON files)
3. Integrate 1.0's pronoun system with the new title/class systems
4. Create in-game OLC editors for races and classes
5. Preserve world data where possible

---

## Current State Analysis

### 1.0 Systems (Current)
- **Races**: 27 hardcoded in `const.c` (lines 518+)
- **Classes**: 4 base classes, 24 subclasses hardcoded in `const.c`
- **Titles**: Gender-based (SEX_MALE/FEMALE/NEUTRAL) for classes and church ranks
- **Pronouns**: Full custom pronoun system with body types (BODY_TYPE_NEUTRAL/MALE/FEMALE/OTHER)
- **Skills/Spells**: Entirely hardcoded in const.c

### 2.0 Systems (Source)
- **Races**: Still hardcoded (same as 1.0)
- **Classes**: Still hardcoded with subclass split
- **Tokens**: NEW - Data-driven skills/spells/affects via TOKEN_INDEX_DATA/TOKEN_DATA
- **Pronouns**: NOT PRESENT (predates 1.0 pronoun system)
- **Skills/Spells**: Hybrid (hardcoded + token-based)

### 1.5 Goals (Target)
- **Races**: JSON-based, OLC-editable, preserves pronoun integration
- **Classes**: JSON-based, OLC-editable, pronoun-aware titles
- **Tokens**: Full 2.0 token system
- **Titles**: Pronoun-aware (not just male/female/neutral)
- **Church Ranks**: Pronoun-aware titles
- **Skills/Spells**: Token-based (phase out hardcoded tables)

---

## Migration Phases

### Phase 1: Token System Foundation (2-3 weeks)

**Goal**: Port 2.0's token infrastructure to support data-driven skills/spells

#### 1.1 Core Token Structures
- [ ] Add `TOKEN_INDEX_DATA` to merc.h (template/prototype for tokens)
- [ ] Add `TOKEN_DATA` to merc.h (instanced tokens on chars/objs/rooms)
- [ ] Add token types: `TOKEN_SKILL`, `TOKEN_SPELL`, `TOKEN_SONG`, etc.
- [ ] Add token value constants: `TOKVAL_SPELL_MANA`, `TOKVAL_SPELL_RATING`, etc.
- [ ] Add token list to `CHAR_DATA->tokens` and `CHAR_DATA->ltokens`

**Files to modify**:
- `merc.h`: Add token structures (based on 2.0 lines 4159-4243)
- `db.c`: Add token loading from area files
- `handler.c`: Add `token_to_char()`, `token_from_char()` functions

#### 1.2 SKILL_ENTRY Hybrid System
- [ ] Modify existing `SKILL_ENTRY` to support token-based skills
- [ ] Add `TOKEN_DATA *token` field to `SKILL_ENTRY`
- [ ] Modify `skill_entry_rating()` to check token vs hardcoded
- [ ] Update `skill_entry_addskill()`, `skill_entry_addspell()` for tokens

**Files to modify**:
- `merc.h`: Update `SKILL_ENTRY` structure (based on 2.0 lines 531-541)
- `skills.c`: Update skill entry functions (based on 2.0 lines 2186-2287)

#### 1.3 Token Integration with Magic System
- [ ] Modify spell casting to check for token-based spells
- [ ] Add token value lookup for mana cost, position, etc.
- [ ] Add token script triggers (`TRIGSLOT_SPELL`, etc.)
- [ ] Update `do_cast()` to handle token spells

**Files to modify**:
- `magic.c`: Update spell casting logic
- `act_wiz.c`: Update skill/spell granting commands

#### 1.4 Token Editor (OLC)
- [ ] Port `tedit.c` from 2.0
- [ ] Commands: `tedit create`, `tedit show`, `tedit <field>`
- [ ] Token saving to area files
- [ ] Token validation

**Files to create**:
- `editors/tokens/tedit.c` (based on 2.0 version)

**Testing**:
- Create test token spell via OLC
- Grant token spell to character
- Cast token spell successfully
- Verify token persists through save/load

---

### Phase 2: Data-Driven Race System (2-3 weeks)

**Goal**: Convert hardcoded races to JSON files with OLC editor

#### 2.1 Race JSON Schema Design
```json
{
  "race_id": "human",
  "name": "Human",
  "display_names": {
    "neutral": "{WHuman ",
    "custom": "{WHuman "
  },
  "playable": true,
  "stats": {
    "str": 0, "int": 0, "wis": 0, "dex": 0, "con": 0
  },
  "max_stats": {
    "str": 18, "int": 18, "wis": 18, "dex": 18, "con": 18
  },
  "max_vitals": {
    "hp": 100, "mana": 100, "move": 100
  },
  "size": 2,
  "alignment_bias": "neutral",
  "bonus_skills": ["common"],
  "starting_eq": [3700, 3701, 3702, 3703, 3704],
  "act_flags": ["npc"],
  "affect_flags": [],
  "immunity_flags": [],
  "resistance_flags": [],
  "vulnerability_flags": [],
  "form_flags": ["edible", "sentient", "biped", "mammal"],
  "body_parts": ["head", "arms", "legs", "heart", "brains", "guts", "hands", "feet", "fingers", "ear", "eye"],
  "remort": false,
  "remort_race": null
}
```

#### 2.2 Race Loading System
- [ ] Create `race_data.c` with JSON parsing functions
- [ ] Add `load_races()` function (called during boot)
- [ ] Add `free_races()` cleanup function
- [ ] Convert `race_table[]` and `pc_race_table[]` to dynamic arrays
- [ ] Add `race_lookup()` for ID-based lookup
- [ ] Migration script to export existing races to JSON

**Files to create**:
- `race_data.c`, `race_data.h`: Race JSON loading/saving
- `data/races/*.json`: Individual race files

**Files to modify**:
- `db.c`: Add `load_races()` call during boot
- `const.c`: Remove hardcoded `race_table[]` and `pc_race_table[]`
- `merc.h`: Update race structures

#### 2.3 Race Editor (OLC)
- [ ] Create `raceedit.c` in `editors/races/`
- [ ] Commands: `raceedit create <id>`, `raceedit <id>`, `raceedit save`
- [ ] Edit all race fields (stats, skills, flags, etc.)
- [ ] Validation (ensure race_id is unique, stats are valid, etc.)

**Files to create**:
- `editors/races/raceedit.c`

#### 2.4 Pronoun Integration
- [ ] Update race display names to be pronoun-agnostic
- [ ] Remove hardcoded male/female race variants
- [ ] Use single display name for all pronouns
- [ ] Update who list to use pronoun-neutral race names

**Files to modify**:
- `act_info.c`: Update `do_who()` to use pronoun-neutral names
- `nanny.c`: Update race selection display

**Testing**:
- Export all 27 races to JSON
- Boot server and verify all races load
- Create new test race via OLC
- Create character with new race
- Verify race persists through save/load

---

### Phase 3: Data-Driven Class System (2-3 weeks)

**Goal**: Convert hardcoded classes to JSON with pronoun-aware titles

#### 3.1 Class JSON Schema Design
```json
{
  "class_id": "mage",
  "name": "Mage",
  "prime_attribute": "int",
  "starting_weapon": 3717,
  "hp_range": [6, 8],
  "gains_mana": true,
  "base_skill_group": "mage basics",
  "subclasses": [
    {
      "subclass_id": "sorcerer",
      "title": "Sorcerer",
      "who_name": "  Sorcerer  ",
      "default_skill_group": "sorcerer skills",
      "alignment": "neutral",
      "is_remort": false,
      "prerequisites": [],
      "starting_eq": [3700, 3701]
    },
    {
      "subclass_id": "witch",
      "title": "Witch",
      "who_name": "   Witch    ",
      "default_skill_group": "witch skills",
      "alignment": "neutral",
      "is_remort": false,
      "prerequisites": [],
      "starting_eq": [3700, 3701]
    }
  ]
}
```

**Note**: Single title per subclass, not gender-specific arrays.

#### 3.2 Class Loading System
- [ ] Create `class_data.c` with JSON parsing
- [ ] Add `load_classes()` function
- [ ] Convert `class_table[]` and `sub_class_table[]` to dynamic
- [ ] Add `class_lookup()`, `subclass_lookup()` functions
- [ ] Migration script to export classes to JSON

**Files to create**:
- `class_data.c`, `class_data.h`
- `data/classes/*.json`: One per base class

**Files to modify**:
- `db.c`: Add `load_classes()` call
- `const.c`: Remove hardcoded class tables
- `merc.h`: Update class structures

#### 3.3 Class Editor (OLC)
- [ ] Create `classedit.c`
- [ ] Commands: `classedit <class_id>`, `classedit save`
- [ ] Subclass editing: `subedit <subclass_id>`
- [ ] Skill group assignment

**Files to create**:
- `editors/classes/classedit.c`

#### 3.4 Pronoun-Aware Class Titles
- [ ] Remove `name[3]` and `who_name[3]` arrays
- [ ] Replace with single `title` and `who_name` strings
- [ ] Update character creation to not use sex-based lookup
- [ ] Update who list to use single title

**Files to modify**:
- `merc.h`: Change `sub_class_type` structure
- `handler.c`: Remove `sub_class_lookup()` sex parameter
- `act_info.c`: Update `do_who()` for single titles
- `nanny.c`: Update class selection

**Testing**:
- Export all classes/subclasses to JSON
- Boot and verify loading
- Create new subclass via OLC
- Create character with new subclass
- Verify title is pronoun-neutral

---

### Phase 4: Pronoun-Aware Church/Org Titles (1 week)

**Goal**: Modernize church rank titles to support custom pronouns

#### 4.1 Church Rank Title System
**Current**: `title_male`, `title_female`, `title_neutral`
**New**: Single `title` field, with optional pronoun template support

**Option A - Simple (Recommended)**:
- Remove gender-specific titles
- Use single title for all members
- Example: "Bishop", "Priest", "Cardinal"

**Option B - Template System**:
- Support pronoun templates: "High {PRONOUN_HIS_HERS}tess" → "High Priest" / "High Priestess"
- Add title parsing function

#### 4.2 Implementation
- [ ] Modify `church_rank_data` structure (remove `title_male`, `title_female`, `title_neutral`)
- [ ] Add single `title` field
- [ ] Update `get_member_title()` to return single title
- [ ] Update church rank loading/saving

**Files to modify**:
- `merc.h`: Update `church_rank_data` structure (line 1412-1423)
- `church.c`: Update title functions (lines 1594-1599, 3887-3890)

**Testing**:
- Create church with new rank structure
- Assign ranks to characters with different pronouns
- Verify titles display correctly

---

### Phase 5: Migration Tools (1 week)

**Goal**: Preserve world data during 1.0→1.5 transition

#### 5.1 Race/Class Export Script
- [ ] Script to read `const.c` and generate JSON files
- [ ] Preserve all existing race/class data
- [ ] Validate JSON output

**Files to create**:
- `tools/export_races.py`
- `tools/export_classes.py`

#### 5.2 Save File Migration (Optional)
- [ ] If save format changes, create migration tool
- [ ] Convert old sex-based class references to new system
- [ ] Preserve character data

**Files to create**:
- `tools/migrate_pfiles.py` (if needed)

---

## Timeline Estimate

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Phase 1: Token System | 2-3 weeks | None |
| Phase 2: Data-Driven Races | 2-3 weeks | Phase 1 complete |
| Phase 3: Data-Driven Classes | 2-3 weeks | Phase 1 complete |
| Phase 4: Church Titles | 1 week | None (can run parallel) |
| Phase 5: Migration Tools | 1 week | Phases 2-3 complete |

**Total**: 8-11 weeks for full implementation

---

## Risk Mitigation

### Backwards Compatibility
- Keep hardcoded tables during transition
- Add feature flag: `USE_JSON_RACES`, `USE_JSON_CLASSES`
- Fallback to hardcoded if JSON load fails

### Testing Strategy
- Unit tests for JSON parsing
- Boot tests to verify all races/classes load
- Character creation tests
- Save/load tests

### Rollback Plan
- Git branch for each phase
- Keep const.c backups
- Document breaking changes

---

## Open Questions for User

1. **Church Title Approach**: Simple single title (Option A) or pronoun template system (Option B)?
2. **Migration Scope**: Do you want to migrate world area files now, or just prepare the system?
3. **Token Priority**: Should we port ALL 2.0 token features, or just core skill/spell tokens?
4. **Editor Permissions**: Who should have access to race/class editors (imms only, builders, etc.)?

---

## Next Steps

1. ✅ Exploration complete
2. ⏭️ Get user approval on approach
3. Begin Phase 1: Token System Foundation
   - Start with TOKEN_INDEX_DATA/TOKEN_DATA structures
   - Port SKILL_ENTRY modifications
   - Create basic token editor

---

## Files Requiring Major Changes

### New Files
- `race_data.c`, `race_data.h`: Race JSON loading
- `class_data.c`, `class_data.h`: Class JSON loading
- `editors/tokens/tedit.c`: Token editor
- `editors/races/raceedit.c`: Race editor
- `editors/classes/classedit.c`: Class editor
- `data/races/*.json`: Race definitions
- `data/classes/*.json`: Class definitions
- `tools/export_races.py`: Migration script
- `tools/export_classes.py`: Migration script

### Modified Files
- `merc.h`: Add token structures, modify race/class structures
- `const.c`: Remove hardcoded tables (or keep with feature flag)
- `db.c`: Add JSON loading calls during boot
- `skills.c`: Token integration for skill system
- `magic.c`: Token integration for spell system
- `handler.c`: Token helper functions, pronoun-aware lookups
- `act_info.c`: Pronoun-neutral who list
- `nanny.c`: Updated character creation
- `church.c`: Single-title rank system
- `Makefile`: Add new source files

---

## Success Criteria

✅ All 27 races loadable from JSON
✅ All 4 base classes + 24 subclasses loadable from JSON
✅ Token-based skills/spells functional
✅ Character creation works with new system
✅ Pronouns properly integrated (no hardcoded male/female assumptions)
✅ World data preserved (areas, mob/obj vnums intact)
✅ OLC editors functional for races, classes, and tokens
✅ Save/load compatibility maintained

---

**End of Migration Plan**

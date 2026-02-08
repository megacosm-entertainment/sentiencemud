# Missing Fields in JSON Character Serialization

Based on analysis of save.c, the following critical fields are NOT being saved to JSON:

## Critical Missing Fields

### Character Combat/Stats
- `hitroll` - Hit bonus
- `damroll` - Damage bonus
- `saving_throw` - Save modifier
- `practice` - Practice sessions
- `train` - Training sessions
- `wimpy` - Wimpy flee threshold
- `armor[4]` - AC values for pierce/bash/slash/magic
- `perm_hit/perm_mana/perm_move` - Permanent vitals (PC_DATA)

### Character Counters/Tracking
- `deaths` - Total deaths
- `arena_deaths` - Arena deaths
- `player_deaths` - PK deaths
- `cpk_deaths` - CPK deaths
- `wars_won` - Wars won
- `arena_kills` - Arena kills
- `player_kills` - PK kills
- `cpk_kills` - CPK kills
- `monster_kills` - Mob kills
- `questpoints` - Quest points
- `quests_completed` - Total quests
- `nextquest` - Time until next quest
- `deitypoints` - Deity points
- `pneuma` - Pneuma points
- `home` - Home room vnum

### Character State
- `position` - Current position (standing/sitting/etc) - we save room but not position!
- `true_sex` - Original sex before polymorph
- `last_level` - Last level gained
- `condition[5]` - Hunger/thirst/drunk/food/etc
- `wimpy` - Wimpy flee level
- `lines` - Pager lines (scrollback)
- `invis_level` - Invisibility level (immortals)

### Bank/Economy
- `bankbalance` (PC_DATA) - Already have gold/silver but missing bank
- `manastore` - Stored mana

### Location/Recall
- `recall` - Recall/death point (ROOM_LOC structure with wuid/id array)
- `checkpoint` - Checkpoint room
- `was_in_room` - Previous room (for things like being forcibly moved)

### Quest System
- `quest` structure (if questing):
  - `questgiver_type`
  - `questgiver`
  - `questreceiver_type`
  - `questreceiver`
  - `countdown` - Quest timer

### Flags & Affects
- `affected_by[2]` - Affect flags (not same as AFFECT_DATA list!)
- `affected_by_perm[2]` - Permanent affect flags
- `imm_flags` / `imm_flags_perm` - Immunity flags
- `res_flags` / `res_flags_perm` - Resistance flags
- `vuln_flags` / `vuln_flags_perm` - Vulnerability flags
- `lostparts` - Lost body parts flags
- `wiznet` - Wiznet flags (immortals)

### Locker/Storage
- `locker_tier` - Locker upgrade level

### Skills
- Already have `learned[]` ✓
- Missing `mod_learned[]` - Skill modifiers
- Missing `group_known[]` - Learned groups
- Missing `songs_learned[]` - Known songs

### Toxins
- `toxin[]` array - Poison/disease levels

### Ships (if applicable)
- Various ship-related fields for boat owners

### Misc PC_DATA
- `security` - Builder security level
- `challenge_delay` - Challenge timer
- `rank[CHURCH_MAX]` - Church ranks
- `reputation[CHURCH_MAX]` - Church reputations
- `ship_quest_points[CHURCH_MAX]` - Ship quest points
- `danger_range` - Danger calculation range
- `need_change_pw` - Password change required
- `room_before_arena` - Location before arena
- `last_project_inquiry` - Last inquiry timestamp

### Objects We Already Handle
- ✓ Inventory
- ✓ Equipment
- ✓ Locker
- ✓ Nested containers

## Priority Fixes

### Must Have (breaks character on load):
1. **armor[4]** - AC values
2. **affected_by[2]** - Affect flags
3. **imm/res/vuln flags** - Defense stats
4. **hitroll/damroll** - Combat stats
5. **practice/train** - Character advancement
6. **bankbalance** - Player wealth
7. **position** - Character position (standing/sleeping/etc)

### Should Have (data loss but not critical):
1. Kill/death counters
2. Quest points and counters
3. Recall location
4. Skill modifiers
5. Group knowledge
6. Condition (hunger/thirst)
7. Permanent vitals

### Nice to Have (convenience/tracking):
1. Last level
2. True sex
3. Lines (pager)
4. Various timestamps
5. Ship data
6. Church ranks

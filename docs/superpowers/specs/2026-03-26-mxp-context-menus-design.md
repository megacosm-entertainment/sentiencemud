# MXP Context Menus for Skills, Inventory & Equipment

**Date:** 2026-03-26  
**Status:** Draft  
**Branch:** `tieryo/skill_cleanup`

## Problem

The in-game display of skills/spells/songs, inventory, and equipment currently outputs plain text. Players on MXP-capable or GMCP (WebSocket) clients cannot interact with these items directly. The existing MXP link system (`mxp_links.c`) supports multi-action context menus with 4-tier transport degradation (GMCP → MXP → OSC8 → plain text), but it's only used in dungeon/blueprint admin displays.

## Goal

Add clickable context menus to:
1. **Skills/spells/songs** in the `skills`/`spells`/`songs` commands
2. **Inventory items** in the `inventory` command
3. **Equipment items** in the `equipment` command

Each item gets a default click action and a right-click context menu with relevant actions. The existing transport system handles protocol degradation automatically.

## Design

### 1. Skill/Spell/Song Links — `mxp_skill_link()`

**New function** in `mxp_links.c`:

```c
void mxp_skill_link(descriptor_t *d, BUFFER *buf,
                    SKILL_ENTRY *entry, CHAR_DATA *ch,
                    const char *text);
```

**Parameters:**
- `d` — Player descriptor (for transport detection)
- `buf` — Output buffer to append to
- `entry` — The skill entry being displayed
- `ch` — The character (for rating/practice checks)
- `text` — The visible display text (skill name, possibly colored)

**Context menu items by ability type:**

#### Spells (`entry->isspell && !entry->song`)

| # | Action | Command | Hint | Condition |
|---|--------|---------|------|-----------|
| 1 | **Cast** (default) | `cast '<name>' ` (trailing space for targeted spells, no space for self/none target) | "Cast this spell" | Always |
| 2 | Skillinfo | `skillinfo '<name>'` | "View skill details" | Always |
| 3 | Help | `help '<name>'` | "View help file" | Always |
| 4 | Practice | `practice '<name>'` | "Practice this spell" | Only if `rating < 100` |
| 5 | Staff: skedit | `skedit '<name>'` | "Edit skill definition" | `staff_only = true` |

#### Songs (`entry->song != NULL`)

| # | Action | Command | Hint | Condition |
|---|--------|---------|------|-----------|
| 1 | **Play** (default) | `play '<name>' ` (trailing space) | "Play this song" | Always |
| 2 | Skillinfo | `skillinfo '<name>'` | "View skill details" | Always |
| 3 | Help | `help '<name>'` | "View help file" | Always |
| 4 | Practice | `practice '<name>'` | "Practice this song" | Only if `rating < 100` |
| 5 | Staff: skedit | `skedit '<name>'` | "Edit skill definition" | `staff_only = true` |

#### Skills (not spell, not song)

| # | Action | Command | Hint | Condition |
|---|--------|---------|------|-----------|
| 1 | **Skillinfo** (default) | `skillinfo '<name>'` | "View skill details" | Always |
| 2 | Help | `help '<name>'` | "View help file" | Always |
| 3 | Practice | `practice '<name>'` | "Practice this skill" | Only if `rating < 100` |
| 4 | Staff: skedit | `skedit '<name>'` | "Edit skill definition" | `staff_only = true` |

**Target detection for trailing space:**
- `TAR_CHAR_OFFENSIVE`, `TAR_CHAR_DEFENSIVE`, `TAR_OBJ_INV`, `TAR_OBJ_CHAR_DEF`, `TAR_OBJ_CHAR_OFF`, `TAR_IGNORE_CHAR_DEF`, `TAR_OBJ_GROUND`, `TAR_CHAR_FORMATION` → trailing space (player types target)
- `TAR_IGNORE`, `TAR_CHAR_SELF` → no trailing space (auto-execute)

**GMCP category:** `"skill"` (new category — `sentience_link.h` must be updated to document this)

**Integration point:** `list_skill_entries()` in `skills.c` (line ~1199). Currently uses `sprintf(buf, "%-26s", eff_name)`. Must be restructured to:
1. Write prefix (index, mana) to buffer via `bprintf()`
2. Write MXP-wrapped name via `mxp_skill_link()`
3. Manually pad to 26 chars based on visual name length
4. Write suffix (rating, modifiers) to buffer

### 2. Inventory Object Links — `mxp_inv_obj_link()`

**New function** in `mxp_links.c`:

```c
void mxp_inv_obj_link(descriptor_t *d, BUFFER *buf,
                      OBJ_DATA *obj, const char *text);
```

**Context menu items:**

| # | Action | Command | Hint | Condition |
|---|--------|---------|------|-----------|
| 1 | **Look** (default) | `look <keyword>` | "Look at item" | Always |
| 2 | Wear/Wield | `wear <keyword>` or `wield <keyword>` | "Wear this" / "Wield this" | `CAN_WEAR` flags check / `ITEM_WEAPON` |
| 3 | Drop | `drop <keyword>` | "Drop item" | Always |
| 4 | Examine | `examine <keyword>` | "Examine item" | Always |
| 5 | Staff: stat | `stat obj <id0> <id1>` | "Stat object" | `staff_only` |
| 6 | Staff: oshow | `oshow <wvnum>` | "Show index" | `staff_only` |
| 7 | Staff: oedit | `oedit <wvnum>` | "Edit index" | `staff_only` |

**Wear vs Wield logic:**
- If `obj->item_type == ITEM_WEAPON` → use `wield <keyword>`
- Otherwise → use `wear <keyword>` (the `wear` command handles shields, armor, held items, etc.)
- Skip wear/wield entirely if item has no wearable flags beyond `ITEM_TAKE` (check via `CAN_WEAR` flags)

**Keyword extraction:** Use `first_keyword(obj->name, ...)` — same pattern as existing `mxp_obj_link()`.

**GMCP category:** `"obj"` (matches existing object link category)

**Integration point:** `show_list_to_char()` in `act_info.c` (line ~781). Currently appends `prgpstrShow[iShow]` (pre-formatted text from `format_obj_to_char()`). Must wrap with MXP link. Challenge: coalesced items — when showing "(3) sword", link the `prgpObj[iShow]` (first instance of that object).

To support this, the `show_list_to_char()` function needs a new `OBJ_DATA *prgpObj[]` array alongside the existing `prgpstrShow[]` and `prgnShow[]` arrays, tracking the first object instance per display group.

### 3. Equipment Object Links — `mxp_eq_obj_link()`

**New function** in `mxp_links.c`:

```c
void mxp_eq_obj_link(descriptor_t *d, BUFFER *buf,
                     OBJ_DATA *obj, const char *text);
```

**Context menu items:**

| # | Action | Command | Hint | Condition |
|---|--------|---------|------|-----------|
| 1 | **Look** (default) | `look <keyword>` | "Look at item" | Always |
| 2 | Remove | `remove <keyword>` | "Remove item" | Always |
| 3 | Examine | `examine <keyword>` | "Examine item" | Always |
| 4 | Staff: stat | `stat obj <id0> <id1>` | "Stat object" | `staff_only` |
| 5 | Staff: oshow | `oshow <wvnum>` | "Show index" | `staff_only` |
| 6 | Staff: oedit | `oedit <wvnum>` | "Edit index" | `staff_only` |

**GMCP category:** `"obj"` (matches existing)

**Integration point:** `show_equipment()` in `act_info.c` (line ~5760). Currently does:
```c
sprintf(buf2, "%s\n\r", format_obj_to_char(eq[iWear], ch, true));
```
Replace with writing the formatted text through `mxp_eq_obj_link()` into the buffer.

### 4. Common Design Decisions

**Keyword quoting:** Skill/spell/song names that contain spaces must be quoted in commands: `cast 'magic missile'`, `skillinfo 'magic missile'`. Use single quotes consistently.

**NULL descriptor handling:** All three functions fall back to plain text (just `add_buf(buf, text)`) when `d` is NULL. This handles mob/script contexts gracefully.

**Buffer convention:** All functions append to a caller-provided `BUFFER*`, consistent with the existing MXP link API.

**Staff action pattern:** Reuse the same stat/show/edit pattern from `mxp_obj_link()` for object links. For skill links, use `skedit <wnum>` (the OLC editor for skills).

## Files Modified

| File | Changes |
|------|---------|
| `mxp_links.c` | Add `mxp_skill_link()`, `mxp_inv_obj_link()`, `mxp_eq_obj_link()` |
| `mxp_links.h` | Declare 3 new functions |
| `sentience_link.h` | Document new `"skill"` category |
| `skills.c` | Modify `list_skill_entries()` to use `mxp_skill_link()` |
| `act_info.c` | Modify `show_list_to_char()` and `show_equipment()` to use MXP links |

## Testing

- Verify MXP output for each transport mode (GMCP, MXP, OSC8, LINK_NONE)
- Verify column alignment is preserved in plain-text and MXP modes
- Verify staff-only actions are hidden from non-immortals
- Verify coalesced inventory items link correctly
- Verify targeted vs self-only spell trailing space behavior
- Verify practice menu item only appears when rating < 100
- Manual testing with WebSocket client for GMCP transport

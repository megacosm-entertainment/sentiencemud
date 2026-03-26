# MXP Context Menus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add clickable context menus to skills/spells/songs, inventory items, and equipment items using the existing MXP link system.

**Architecture:** Three new helper functions in `mxp_links.c` (`mxp_skill_link`, `mxp_inv_obj_link`, `mxp_eq_obj_link`) following the same pattern as `mxp_obj_link()`. Integration into display functions in `skills.c` and `act_info.c`. The existing 4-tier transport system (GMCP → MXP → OSC8 → plain text) handles protocol degradation automatically.

**Tech Stack:** C23, MXP link system (`mxp_links.c`/`sentience_link.c`), Jansson JSON

**Spec:** `docs/superpowers/specs/2026-03-26-mxp-context-menus-design.md`

---

## File Structure

| File | Responsibility |
|------|---------------|
| `mxp_links.h` | Declare 3 new functions |
| `mxp_links.c` | Implement `mxp_skill_link()`, `mxp_inv_obj_link()`, `mxp_eq_obj_link()` |
| `sentience_link.h` | Document `"skill"` as valid category |
| `skills.c` | Integrate `mxp_skill_link()` into `list_skill_entries()` |
| `act_info.c` | Integrate `mxp_inv_obj_link()` into `show_list_to_char()`, `mxp_eq_obj_link()` into `show_equipment()` |

No new files created. No build system changes needed (no new `.c` files).

---

## Task 1: Add `mxp_skill_link()` helper

**Files:**
- Modify: `mxp_links.h` (add declaration)
- Modify: `mxp_links.c` (add implementation, after `mxp_help_link`)
- Modify: `sentience_link.h:45` (add `"skill"` to category comment)

- [ ] **Step 1: Add declaration to `mxp_links.h`**

Add before the `#endif`:

```c
/**
 * mxp_skill_link - Clickable skill/spell/song link with context menu
 *
 * Spells: default=cast, menu: cast/skillinfo/help/practice
 * Songs:  default=play, menu: play/skillinfo/help/practice
 * Skills: default=skillinfo, menu: skillinfo/help/practice
 *
 * Practice only shown if rating < 100. Targeted spells/songs get a
 * trailing space so the player can type a target.
 *
 * @param d      Descriptor
 * @param buf    Output BUFFER
 * @param entry  Skill entry (SKILL_ENTRY*)
 * @param ch     Character (for rating checks)
 * @param text   Display text (skill name, may include colour codes)
 */
void mxp_skill_link(descriptor_t *d, BUFFER *buf, SKILL_ENTRY *entry,
                    CHAR_DATA *ch, const char *text);
```

- [ ] **Step 2: Update category comment in `sentience_link.h`**

Change line 45 from:
```c
    char *category; /* "obj", "mob", "room", "player", "help", "cmd" */
```
to:
```c
    char *category; /* "obj", "mob", "room", "player", "help", "cmd", "skill" */
```

- [ ] **Step 3: Implement `mxp_skill_link()` in `mxp_links.c`**

Add after `mxp_help_link()` (after line 429):

```c
void mxp_skill_link(descriptor_t *d, BUFFER *buf, SKILL_ENTRY *entry,
                    CHAR_DATA *ch, const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[8];
    char c_use[256], c_info[256], c_help[256], c_prac[256], c_edit[128];
    const char *name;
    int rating;
    bool needs_target;

    if (!entry || !text) { add_buf(buf, (text ? text : "")); return; }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, text); return; }

    name = skill_entry_name(entry);
    rating = skill_entry_rating(ch, entry);

    /* Determine if the ability needs a target (trailing space for input) */
    needs_target = false;
    if (entry->skill_data) {
        int tgt = entry->skill_data->target;
        needs_target = (tgt != TAR_IGNORE && tgt != TAR_CHAR_SELF);
    }

    if (entry->isspell && !entry->song) {
        /* Spell: cast is default */
        if (needs_target)
            snprintf(c_use, sizeof(c_use), "cast '%s' ", name);
        else
            snprintf(c_use, sizeof(c_use), "cast '%s'", name);
        items[n++] = (mxp_cmd_hint_t){ c_use, "Cast", false };
    } else if (entry->song) {
        /* Song: play is default */
        snprintf(c_use, sizeof(c_use), "play '%s' ", name);
        items[n++] = (mxp_cmd_hint_t){ c_use, "Play", false };
    }

    /* Skillinfo (default for non-spell/non-song skills) */
    snprintf(c_info, sizeof(c_info), "skillinfo '%s'", name);
    items[n++] = (mxp_cmd_hint_t){ c_info, "Skill info", false };

    /* Help */
    if (entry->skill_data && entry->skill_data->help_keyword
        && entry->skill_data->help_keyword[0]) {
        snprintf(c_help, sizeof(c_help), "help %s",
                 entry->skill_data->help_keyword);
    } else {
        snprintf(c_help, sizeof(c_help), "help '%s'", name);
    }
    items[n++] = (mxp_cmd_hint_t){ c_help, "Help", false };

    /* Practice (only if not mastered) */
    if (rating >= 0 && rating < 100) {
        snprintf(c_prac, sizeof(c_prac), "practice '%s'", name);
        items[n++] = (mxp_cmd_hint_t){ c_prac, "Practice", false };
    }

    /* Staff: skedit */
    if (entry->skill_data && entry->skill_data->name) {
        snprintf(c_edit, sizeof(c_edit), "skedit '%s'",
                 entry->skill_data->name);
        items[n++] = (mxp_cmd_hint_t){ c_edit, "Edit skill", true };
    }

    link_route(d, buf, text, NULL, "skill", items, n, mode);
}
```

- [ ] **Step 4: Build and verify compilation**

Run: `cd /sentience/src && ./build`
Expected: Clean compile, no errors.

- [ ] **Step 5: Commit**

```bash
git add mxp_links.c mxp_links.h sentience_link.h
git commit -m "feat(mxp): add mxp_skill_link() helper for skill context menus

Provides cast/play/skillinfo/help/practice context menu actions
for spells, songs, and skills. Targeted abilities get trailing
space for target input. Practice only shown when rating < 100.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 2: Integrate `mxp_skill_link()` into `list_skill_entries()`

**Files:**
- Modify: `skills.c` (add include, restructure display loop)

The core challenge: `list_skill_entries()` uses `sprintf(buf, "%-26s", eff_name)` which embeds the skill name in a fixed-width format. We need to split this so the name goes through `mxp_skill_link()` while preserving column alignment.

The `%-26s` pads `eff_name` which is `"{Y<name>"` (color code + name). The color code `{Y` is 2 chars that aren't visible. So the format pads to 26 total chars including the invisible `{Y`. The visual width is 24 chars.

Strategy: replace `sprintf(buf, "...%-26s...", eff_name)` with:
1. `bprintf(buffer, " %3d     %-8s ", i, min_mana)` — prefix
2. `mxp_skill_link(d, buffer, entry, ch, eff_name)` — linked name
3. Manual padding: `nocolour_strlen(eff_name)` tells us visible width; pad to 26 chars
4. `bprintf(buffer, "    ...")` — suffix (rating, etc.)

- [ ] **Step 1: Add `#include "mxp_links.h"` to `skills.c`**

Add after the existing `#include "merc.h"` line (line 6):

```c
#include "mxp_links.h"
```

- [ ] **Step 2: Create a helper for padded MXP skill output**

Add a static helper above `list_skill_entries()` (before line 1103):

```c
/*
 * Write an MXP-linked skill name padded to a fixed width.
 * The pad_width accounts for invisible colour codes in eff_name.
 */
static void skill_name_padded(descriptor_t *d, BUFFER *buf,
                               SKILL_ENTRY *entry, CHAR_DATA *ch,
                               const char *eff_name, int pad_width)
{
    int vis_len, pad;
    char *plain;

    mxp_skill_link(d, buf, entry, ch, eff_name);

    /* Calculate visible length (without colour codes) and pad */
    plain = nocolour(eff_name);
    vis_len = (int)strlen(plain);
    free_string(plain);

    for (pad = vis_len; pad < pad_width; pad++)
        add_buf(buf, " ");
}
```

- [ ] **Step 3: Restructure the main display loop (spell view, lines 1221-1241)**

Replace the spell-view `sprintf` calls at lines 1215-1241 with buffer-based output. The key change is splitting each `sprintf(buf, " %3d     %-8s %-26s    ...", i, min_mana, eff_name, ...)` into:

```c
                if( show_spells ) {
                    if( mana > 0 )
                        sprintf(min_mana, "%3d", mana);
                    else
                        strcpy(min_mana, "---");

                    bprintf(buffer, " %3d     %-8s ", i, min_mana);
                    skill_name_padded(ch->desc, buffer, entry, ch, eff_name, 26);

                    if( level < 0 )
                        bprintf(buffer, "    {xUnlocks at {%c%d", col_ulk, -level);
                    else {
                        rating = skill + mod;
                        rating = URANGE(0,rating,100);

                        if( rating >= 100 ) {
                            if( mod )
                                bprintf(buffer, "    {%cMaster {%c(%+d%%)", col_mst, col_mod, mod);
                            else
                                bprintf(buffer, "    {%cMaster", col_mst);
                        } else if( mod )
                            if ( show_learn_amount )
                                bprintf(buffer, "    {%c%d%% {%c(%+d%%) {%c- Gain %d%% per prac{X", col_pct, rating, col_mod, mod, col_lrn, learn);
                            else
                                bprintf(buffer, "    {%c%d%% {%c(%+d%%)", col_pct, rating, col_mod, mod);
                        else if ( show_learn_amount )
                            bprintf(buffer, "    {%c%d%% {%c- Gain %d%% per prac{X", col_pct, rating, col_lrn, learn);
                        else
                            bprintf(buffer, "    {%c%d%%", col_pct, rating);
                    }
```

- [ ] **Step 4: Restructure the skill-only view (lines 1242-1263)**

Same pattern but without the mana column:

```c
                } else {
                    bprintf(buffer, " %3d     ", i);
                    skill_name_padded(ch->desc, buffer, entry, ch, eff_name, 26);

                    if( level < 0 )
                        bprintf(buffer, "    {xUnlocks at {%c%d", col_ulk, -level);
                    else {
                        rating = skill + mod;
                        rating = URANGE(0,rating,100);

                        if( rating >= 100 ) {
                            if( mod )
                                bprintf(buffer, "    {%cMaster {%c(%+d%%)", col_mst, col_mod, mod);
                            else
                                bprintf(buffer, "    {%cMaster", col_mst);
                        } else if( mod )
                            if (show_learn_amount )
                                bprintf(buffer, "    {%c%d%% {%c(%+d%%) {%c- Gain %d%% per prac{X", col_pct, rating, col_mod, mod, col_lrn, learn);
                            else
                                bprintf(buffer, "    {%c%d%% {%c(%+d%%)", col_pct, rating, col_mod, mod);
                        else if ( show_learn_amount )
                            bprintf(buffer, "    {%c%d%% {%c- Gain %d%% per prac{X", col_pct, rating, col_lrn, learn);
                        else
                            bprintf(buffer, "    {%c%d%%", col_pct, rating);
                    }
                }
```

- [ ] **Step 5: Update the favourite tag and newline (lines 1267-1273)**

Replace the `strcat(buf, ...); add_buf(buffer, buf);` pattern:

```c
                if(!favonly && IS_SET(entry->flags, SKILL_FAVOURITE))
                    add_buf(buffer, " {W[FAVOURITE]");

                add_buf(buffer, "{x\n\r");
```

Remove the old `add_buf(buffer, buf)` at line 1273 since we now write directly to buffer.

- [ ] **Step 6: Update the negated-skills display (lines 1171-1176)**

Also update the `/negated` branch to use MXP links:

```c
                if( show_spells ) {
                    bprintf(buffer, " %3d     ---      ", i);
                    skill_name_padded(ch->desc, buffer, entry, ch, eff_name_neg, 26);
                    bprintf(buffer, "    {D%d%%{x\n\r", -rating);
                } else {
                    bprintf(buffer, " %3d     ", i);
                    skill_name_padded(ch->desc, buffer, entry, ch, eff_name_neg, 26);
                    bprintf(buffer, "    {D%d%%{x\n\r", -rating);
                }
```

Where `eff_name_neg` is built similar to the main loop: `sprintf(eff_name_neg, "{%c%s", color, skill_entry_name(entry))`. This already exists at line 1169-1172 — the color is assigned at 1169 and the format is in the sprintf at 1172. Restructure to use the buffer-direct approach.

- [ ] **Step 7: Build and verify**

Run: `cd /sentience/src && ./build`
Expected: Clean compile.

- [ ] **Step 8: Manual smoke test**

Run: `cd /sentience/src && ./install debug`
Restart server, log in, type `skills`, `spells`, `songs`. Verify:
- Column alignment matches the old format for plain text clients
- WebSocket/GMCP client shows clickable skill names with context menus
- Cast/play default clicks include trailing space for targeted abilities
- Practice option absent for mastered skills
- Staff see "Edit skill" option, players don't

- [ ] **Step 9: Commit**

```bash
git add skills.c
git commit -m "feat(mxp): add context menus to skills/spells/songs display

Clickable skill names in list_skill_entries() with cast/play/skillinfo/
help/practice actions. Targeted spells get trailing space for target.
Column alignment preserved for plain-text clients.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 3: Add `mxp_inv_obj_link()` helper

**Files:**
- Modify: `mxp_links.h` (add declaration)
- Modify: `mxp_links.c` (add implementation)

- [ ] **Step 1: Add declaration to `mxp_links.h`**

Add before the `#endif`:

```c
/**
 * mxp_inv_obj_link - Clickable inventory item with wear/drop/examine actions
 *
 * Default: look. Menu: look/wear-or-wield/drop/examine + staff actions.
 * Wear/wield only if item has wearable flags beyond ITEM_TAKE.
 * Weapons use "wield", everything else uses "wear".
 *
 * @param d      Descriptor
 * @param buf    Output BUFFER
 * @param obj    Object instance
 * @param text   Display text (e.g., from format_obj_to_char)
 */
void mxp_inv_obj_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj,
                      const char *text);
```

- [ ] **Step 2: Implement `mxp_inv_obj_link()` in `mxp_links.c`**

Add after `mxp_skill_link()`:

```c
void mxp_inv_obj_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj,
                      const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[8];
    char kw[64];
    char p1[128], p2[128], p3[128], p4[128];
    char c1[128], c2[128], c3[128];

    if (!obj || !text) { add_buf(buf, (text ? text : "")); return; }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, text); return; }

    first_keyword(obj->name, kw, sizeof(kw));
    if (!kw[0]) { add_buf(buf, text); return; }

    /* Look (default) */
    snprintf(p1, sizeof(p1), "look %s", kw);
    items[n++] = (mxp_cmd_hint_t){ p1, "Look", false };

    /* Wear or Wield (only if item has wear flags beyond ITEM_TAKE) */
    if (obj->wear_flags & ~ITEM_TAKE) {
        if (obj->item_type == ITEM_WEAPON) {
            snprintf(p2, sizeof(p2), "wield %s", kw);
            items[n++] = (mxp_cmd_hint_t){ p2, "Wield", false };
        } else {
            snprintf(p2, sizeof(p2), "wear %s", kw);
            items[n++] = (mxp_cmd_hint_t){ p2, "Wear", false };
        }
    }

    /* Drop */
    snprintf(p3, sizeof(p3), "drop %s", kw);
    items[n++] = (mxp_cmd_hint_t){ p3, "Drop", false };

    /* Examine */
    snprintf(p4, sizeof(p4), "examine %s", kw);
    items[n++] = (mxp_cmd_hint_t){ p4, "Examine", false };

    /* Staff actions */
    {
        const char *wvnum = widevnum_string_object(obj->pIndexData, NULL);

        snprintf(c1, sizeof(c1), "stat obj %ld %ld", obj->id[0], obj->id[1]);
        items[n++] = (mxp_cmd_hint_t){ c1, "Stat object", true };

        snprintf(c2, sizeof(c2), "oshow %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c2, "Show index", true };

        snprintf(c3, sizeof(c3), "oedit %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c3, "Edit index", true };
    }

    link_route(d, buf, text, NULL, "obj", items, n, mode);
}
```

- [ ] **Step 3: Build and verify**

Run: `cd /sentience/src && ./build`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add mxp_links.c mxp_links.h
git commit -m "feat(mxp): add mxp_inv_obj_link() for inventory context menus

Look/wear-or-wield/drop/examine for players, stat/oshow/oedit for staff.
Wear/wield only shown for items with wearable flags beyond ITEM_TAKE.
Weapons use 'wield', other wearables use 'wear'.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 4: Add `mxp_eq_obj_link()` helper

**Files:**
- Modify: `mxp_links.h` (add declaration)
- Modify: `mxp_links.c` (add implementation)

- [ ] **Step 1: Add declaration to `mxp_links.h`**

Add before the `#endif`:

```c
/**
 * mxp_eq_obj_link - Clickable equipment item with remove/examine actions
 *
 * Default: look. Menu: look/remove/examine + staff actions.
 *
 * @param d      Descriptor
 * @param buf    Output BUFFER
 * @param obj    Object instance (must be equipped)
 * @param text   Display text
 */
void mxp_eq_obj_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj,
                     const char *text);
```

- [ ] **Step 2: Implement `mxp_eq_obj_link()` in `mxp_links.c`**

Add after `mxp_inv_obj_link()`:

```c
void mxp_eq_obj_link(descriptor_t *d, BUFFER *buf, OBJ_DATA *obj,
                     const char *text)
{
    link_mode_t mode;
    int n = 0;
    mxp_cmd_hint_t items[8];
    char kw[64];
    char p1[128], p2[128], p3[128];
    char c1[128], c2[128], c3[128];

    if (!obj || !text) { add_buf(buf, (text ? text : "")); return; }

    mode = link_mode(d);
    if (mode == LINK_NONE) { add_buf(buf, text); return; }

    first_keyword(obj->name, kw, sizeof(kw));
    if (!kw[0]) { add_buf(buf, text); return; }

    /* Look (default) */
    snprintf(p1, sizeof(p1), "look %s", kw);
    items[n++] = (mxp_cmd_hint_t){ p1, "Look", false };

    /* Remove */
    snprintf(p2, sizeof(p2), "remove %s", kw);
    items[n++] = (mxp_cmd_hint_t){ p2, "Remove", false };

    /* Examine */
    snprintf(p3, sizeof(p3), "examine %s", kw);
    items[n++] = (mxp_cmd_hint_t){ p3, "Examine", false };

    /* Staff actions */
    {
        const char *wvnum = widevnum_string_object(obj->pIndexData, NULL);

        snprintf(c1, sizeof(c1), "stat obj %ld %ld", obj->id[0], obj->id[1]);
        items[n++] = (mxp_cmd_hint_t){ c1, "Stat object", true };

        snprintf(c2, sizeof(c2), "oshow %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c2, "Show index", true };

        snprintf(c3, sizeof(c3), "oedit %s", wvnum);
        items[n++] = (mxp_cmd_hint_t){ c3, "Edit index", true };
    }

    link_route(d, buf, text, NULL, "obj", items, n, mode);
}
```

- [ ] **Step 3: Build and verify**

Run: `cd /sentience/src && ./build`
Expected: Clean compile.

- [ ] **Step 4: Commit**

```bash
git add mxp_links.c mxp_links.h
git commit -m "feat(mxp): add mxp_eq_obj_link() for equipment context menus

Look/remove/examine for players, stat/oshow/oedit for staff.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 5: Integrate `mxp_inv_obj_link()` into inventory display

**Files:**
- Modify: `act_info.c` (add include, modify `show_list_to_char`)

The function `show_list_to_char()` coalesces duplicate items. We need to track the first `OBJ_DATA*` for each display group so we can pass it to `mxp_inv_obj_link()`.

- [ ] **Step 1: Add `#include "mxp_links.h"` to `act_info.c`**

Add after the existing `#include "protocol.h"` line:

```c
#include "mxp_links.h"
```

- [ ] **Step 2: Add `OBJ_DATA **prgpObj` tracking array in `show_list_to_char()`**

After the allocation of `prgnShow` (line 695), add:

```c
    OBJ_DATA **prgpObj = alloc_mem(count * sizeof(OBJ_DATA *));
    if (count > 0)
        memset(prgpObj, 0, count * sizeof(OBJ_DATA *));
```

- [ ] **Step 3: Track first object per display group**

In the formatting loop, when a new display group is created (line 748-753), store the object:

```c
        if (!fCombine)
        {
        prgpstrShow[nShow] = str_dup(pstrShow);
        prgnShow[nShow] = 1;
        prgpObj[nShow] = obj;
        nShow++;
        }
```

- [ ] **Step 4: Use `mxp_inv_obj_link()` in the output loop**

Replace the item text output at lines 805-807. The current code:
```c
        if (!add_buf(output, "{x")
        ||  !add_buf(output, prgpstrShow[iShow])
        ||  !add_buf(output, "\n\r{x"))
```

Replace with:

```c
        add_buf(output, "{x");
        if (prgpObj[iShow] && ch->desc)
            mxp_inv_obj_link(ch->desc, output, prgpObj[iShow], prgpstrShow[iShow]);
        else
            add_buf(output, prgpstrShow[iShow]);
        if (!add_buf(output, "\n\r{x"))
```

Note: `show_list_to_char` is also called for room contents (objects on ground). In that context, wear/wield/drop aren't appropriate. Check if `list->carried_by` is set: if `list->carried_by == ch`, it's inventory; otherwise it's room contents and we should use the existing `mxp_obj_link()` instead.

Update the output section:

```c
        add_buf(output, "{x");
        if (prgpObj[iShow] && ch->desc) {
            if (prgpObj[iShow]->carried_by == ch)
                mxp_inv_obj_link(ch->desc, output, prgpObj[iShow], prgpstrShow[iShow]);
            else
                mxp_obj_link(ch->desc, output, prgpObj[iShow], prgpstrShow[iShow]);
        } else {
            add_buf(output, prgpstrShow[iShow]);
        }
        if (!add_buf(output, "\n\r{x"))
```

- [ ] **Step 5: Free the new array in cleanup**

After the existing `free_mem(prgnShow, ...)` call (line 843), add:

```c
    free_mem(prgpObj, count * sizeof(OBJ_DATA *));
```

- [ ] **Step 6: Build and verify**

Run: `cd /sentience/src && ./build`
Expected: Clean compile.

- [ ] **Step 7: Manual smoke test**

Log in, type `inventory`. Verify:
- Items are clickable with look/wear/drop/examine context menu
- Coalesced items (e.g., "(3) sword") still work and link correctly
- Room contents (type `look`) use look/examine (not wear/drop)
- Column alignment preserved for plain text clients

- [ ] **Step 8: Commit**

```bash
git add act_info.c
git commit -m "feat(mxp): add context menus to inventory display

Inventory items get look/wear-or-wield/drop/examine context menus.
Room contents use look/examine. Coalesced items link to the first
object instance. Falls back to plain text for non-MXP clients.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 6: Integrate `mxp_eq_obj_link()` into equipment display

**Files:**
- Modify: `act_info.c` (`show_equipment()` function)

- [ ] **Step 1: Replace `sprintf(buf2, ...)` with MXP-linked output**

In `show_equipment()` at lines 5789-5801, replace:

```c
        sprintf(buf, "%s ", where_name[iWear]);
        if (eq[iWear]) {
            if (can_see_obj(ch, eq[iWear]))
                sprintf(buf2, "%s\n\r", format_obj_to_char(eq[iWear], ch, true));
            else
                sprintf(buf2, "something.\n\r");
        } else if (wear_params[iWear][1] && IS_SET(victim->act[0], PLR_AUTOEQ) && ch == victim)
            sprintf(buf2, "nothing.\n\r");
        else
            continue;

        ++count;

        strcat(buf, buf2);

        add_buf(buffer, buf);
```

With:

```c
        add_buf(buffer, where_name[iWear]);
        add_buf(buffer, " ");
        if (eq[iWear]) {
            if (can_see_obj(ch, eq[iWear])) {
                const char *obj_text = format_obj_to_char(eq[iWear], ch, true);
                if (ch->desc)
                    mxp_eq_obj_link(ch->desc, buffer, eq[iWear], obj_text);
                else
                    add_buf(buffer, obj_text);
                add_buf(buffer, "\n\r");
            } else {
                add_buf(buffer, "something.\n\r");
            }
        } else if (wear_params[iWear][1] && IS_SET(victim->act[0], PLR_AUTOEQ) && ch == victim) {
            add_buf(buffer, "nothing.\n\r");
        } else {
            continue;
        }

        ++count;
```

- [ ] **Step 2: Build and verify**

Run: `cd /sentience/src && ./build`
Expected: Clean compile.

- [ ] **Step 3: Manual smoke test**

Log in, type `equipment`. Verify:
- Worn items are clickable with look/remove/examine context menu
- Empty slots (with autoeq) show "nothing." without links
- "something." (items you can't see) has no links
- Staff see stat/oshow/oedit options
- Column alignment preserved for all clients

- [ ] **Step 4: Commit**

```bash
git add act_info.c
git commit -m "feat(mxp): add context menus to equipment display

Equipment items get look/remove/examine context menus. Empty slots
and invisible items render as plain text. Staff actions included.

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>"
```

---

## Task 7: Final build, test, push

- [ ] **Step 1: Full build with tests**

Run: `cd /sentience/src && ./build tests`
Expected: Clean compile with test support.

- [ ] **Step 2: Run test suite**

Run: `cd /sentience && ./sent -test`
Expected: All existing tests pass (baseline: 399+ pass, 1 known failure). No regressions.

- [ ] **Step 3: Install and manual integration test**

Run: `cd /sentience/src && ./install debug`
Restart server. Connect via WebSocket client and telnet. Verify all 3 features:
- Skills/spells/songs with context menus
- Inventory with context menus
- Equipment with context menus

- [ ] **Step 4: Push**

```bash
git push origin tieryo/skill_cleanup
```
